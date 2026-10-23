// Standard MIDI Files and MUS lumps as one list of events in audio frames
#include "midifile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100

// An event as read, before the tracks are merged: in ticks, tempo changes
// among them (status 0xFF)
typedef struct {
    uint32_t tick;
    uint32_t seq;        // the order read in: tracks one after the other
    uint32_t tempo;
    uint8_t status, data1, data2;
    uint16_t length;
    const uint8_t* data;
} raw_event;

typedef struct {
    raw_event* ev;
    int count, size;
    uint32_t end_tick;
    uint32_t first_note;
    int notes;
    // the first track's texts before its notes: (tick, text)
    uint32_t text_tick[8];
    char text[8][MF_TEXT_COLS];
    int texts;
} builder;

static int add(builder* b, uint32_t tick, uint8_t status, uint8_t d1, uint8_t d2) {
    raw_event* e;

    if (b->count == b->size) {
        int size = b->size ? b->size * 2 : 1024;
        raw_event* ev = realloc(b->ev, (size_t)size * sizeof *ev);
        if (ev == NULL) {
            return 0;
        }
        b->ev = ev;
        b->size = size;
    }
    e = &b->ev[b->count];
    memset(e, 0, sizeof *e);
    e->tick = tick;
    e->seq = (uint32_t)b->count++;
    e->status = status;
    e->data1 = d1;
    e->data2 = d2;
    if ((status & 0xF0) == 0x90 && d2 > 0) {
        if (b->notes == 0 || tick < b->first_note) {
            b->first_note = tick;
        }
        b->notes++;
    }
    return 1;
}

static int by_time(const void* a, const void* b) {
    const raw_event* x = a;
    const raw_event* y = b;

    if (x->tick != y->tick) {
        return x->tick < y->tick ? -1 : 1;
    }
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

// A text event for the song's lines: what can be shown, trimmed
static void add_text(builder* b, uint32_t tick, const uint8_t* p, uint32_t len) {
    char* t;
    uint32_t i;
    int n = 0;

    if (b->texts == 8) {
        return;
    }
    t = b->text[b->texts];
    for (i = 0; i < len && n < MF_TEXT_COLS - 1; i++) {
        uint8_t c = p[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
        if (c >= 32 && c < 127 && !(c == ' ' && (n == 0 || t[n - 1] == ' '))) {
            t[n++] = (char)c;
        }
    }
    while (n > 0 && t[n - 1] == ' ') {
        n--;
    }
    t[n] = 0;
    if (n > 0) {
        b->text_tick[b->texts++] = tick;
    }
}

// The merged list in audio frames: tempo changes worked in and left out
static int finish(builder* b, double division_us, int division, mf_song* song, char* err, int errlen) {
    double us = 0, us_per_tick;
    uint32_t last = 0;
    int i, n = 0;

    qsort(b->ev, (size_t)b->count, sizeof *b->ev, by_time);
    song->events = malloc((size_t)(b->count ? b->count : 1) * sizeof *song->events);
    if (song->events == NULL) {
        snprintf(err, (size_t)errlen, "is too big");
        return 0;
    }
    // ticks of a SMPTE division are a fixed time; else 120 bpm until a tempo
    us_per_tick = division_us > 0 ? division_us : 500000.0 / division;
    for (i = 0; i < b->count; i++) {
        const raw_event* r = &b->ev[i];

        us += (double)(r->tick - last) * us_per_tick;
        last = r->tick;
        if (r->status == 0xFF) {
            if (division_us <= 0 && r->tempo > 0) {
                us_per_tick = (double)r->tempo / division;
            }
            continue;
        }
        song->events[n].frame = (uint32_t)(us * RATE / 1e6 + 0.5);
        song->events[n].status = r->status;
        song->events[n].data1 = r->data1;
        song->events[n].data2 = r->data2;
        song->events[n].length = r->length;
        song->events[n].data = r->data;
        n++;
    }
    if (b->end_tick > last) {
        us += (double)(b->end_tick - last) * us_per_tick;
    }
    song->count = n;
    song->length = (uint32_t)(us * RATE / 1e6 + 0.5);
    song->notes = b->notes;
    for (i = 0; i < b->texts && song->text_lines < MF_TEXT_LINES; i++) {
        if (b->notes == 0 || b->text_tick[i] <= b->first_note) {
            memcpy(song->text[song->text_lines++], b->text[i], MF_TEXT_COLS);
        }
    }
    return 1;
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t le16(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

// A variable-length number; 0 at the end of the data
static int varlen(const uint8_t** p, const uint8_t* end, uint32_t* v) {
    int i;

    *v = 0;
    for (i = 0; i < 4 && *p < end; i++) {
        uint8_t c = *(*p)++;
        *v = *v << 7 | (c & 0x7F);
        if (!(c & 0x80)) {
            return 1;
        }
    }
    return 0;
}

// One MTrk chunk. A track cut short ends where it is cut.
static int read_track(builder* b, const uint8_t* p, const uint8_t* end, int first) {
    uint32_t tick = 0, delta, len;
    uint8_t running = 0;

    while (p < end && varlen(&p, end, &delta)) {
        uint8_t status;

        tick += delta;
        if (p >= end) {
            break;
        }
        status = *p;
        if (status & 0x80) {
            p++;
        } else if (running) {
            status = running;
        } else {
            break;
        }
        if (status < 0xF0) {
            uint8_t d1, d2 = 0;
            int two = (status & 0xE0) != 0xC0;

            running = status;
            if (p + (two ? 2 : 1) > end) {
                break;
            }
            d1 = *p++ & 0x7F;
            if (two) {
                d2 = *p++ & 0x7F;
            }
            if (!add(b, tick, status, d1, d2)) {
                return 0;
            }
        } else if (status == 0xF0 || status == 0xF7) {
            if (!varlen(&p, end, &len) || len > (uint32_t)(end - p)) {
                break;
            }
            // a SysEx: without its F7; an F7 escape is left out
            if (status == 0xF0 && len > 0) {
                uint32_t n = p[len - 1] == 0xF7 ? len - 1 : len;
                if (n > 0 && n < 65536) {
                    if (!add(b, tick, 0xF0, 0, 0)) {
                        return 0;
                    }
                    b->ev[b->count - 1].data = p;
                    b->ev[b->count - 1].length = (uint16_t)n;
                }
            }
            p += len;
        } else if (status == 0xFF) {
            uint8_t type;

            if (p >= end) {
                break;
            }
            type = *p++;
            if (!varlen(&p, end, &len) || len > (uint32_t)(end - p)) {
                break;
            }
            if (type == 0x2F) {
                break;
            }
            if (type == 0x51 && len == 3) {
                if (!add(b, tick, 0xFF, 0, 0)) {
                    return 0;
                }
                b->ev[b->count - 1].tempo = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
            } else if (first && type >= 1 && type <= 3) {
                add_text(b, tick, p, len);
            }
            p += len;
        } else {
            // F1..FE have no place in a file: give up on the track
            break;
        }
    }
    if (tick > b->end_tick) {
        b->end_tick = tick;
    }
    return 1;
}

static int parse_smf(const uint8_t* p, size_t size, mf_song* song, char* err, int errlen) {
    builder b;
    const uint8_t* end = p + size;
    uint32_t hlen;
    int division, tracks = 0, ok;
    double division_us = 0;

    memset(&b, 0, sizeof b);
    hlen = be32(p + 4);
    if (hlen < 6 || hlen > size - 8) {
        snprintf(err, (size_t)errlen, "is damaged (header)");
        return 0;
    }
    division = (int)(p[12] << 8 | p[13]);
    if (division & 0x8000) {
        int fps = 256 - (division >> 8), sub = division & 0xFF;
        if (fps == 29) {
            fps = 30;
        }
        division_us = fps > 0 && sub > 0 ? 1e6 / (fps * sub) : 0;
    }
    if (division == 0 || ((division & 0x8000) && division_us == 0)) {
        snprintf(err, (size_t)errlen, "is damaged (header)");
        return 0;
    }
    p += 8 + hlen;
    while (end - p >= 8) {
        uint32_t len = be32(p + 4);
        const uint8_t* body = p + 8;
        const uint8_t* stop = len > (uint32_t)(end - body) ? end : body + len;

        if (memcmp(p, "MTrk", 4) == 0) {
            if (!read_track(&b, body, stop, tracks == 0)) {
                free(b.ev);
                snprintf(err, (size_t)errlen, "is too big");
                return 0;
            }
            tracks++;
        }
        p = stop;
    }
    if (tracks == 0) {
        free(b.ev);
        snprintf(err, (size_t)errlen, "is damaged (no tracks)");
        return 0;
    }
    ok = finish(&b, division_us, division & 0x7FFF, song, err, errlen);
    free(b.ev);
    return ok;
}

// MUS (the DMX sound library's): 140 ticks a second, channel 15 the drums.
// As Chocolate Doom's mus2mid turns it into MIDI.
static int parse_mus(const uint8_t* p, size_t size, mf_song* song, char* err, int errlen) {
    static const uint8_t controllers[] = { 0, 0, 1, 7, 10, 11, 91, 93, 64, 67 };
    static const uint8_t system[] = { 120, 123, 126, 127, 121 };
    builder b;
    uint32_t start = le16(p + 6), tick = 0, length = le16(p + 4);
    const uint8_t* q;
    const uint8_t* end;
    uint8_t velocity[16];
    int channel_of[16], used = 0, i, ok;

    memset(&b, 0, sizeof b);
    if (start >= size) {
        snprintf(err, (size_t)errlen, "is damaged (header)");
        return 0;
    }
    q = p + start;
    end = length > 0 && start + length <= size ? q + length : p + size;
    for (i = 0; i < 16; i++) {
        velocity[i] = 127;
        channel_of[i] = -1;
    }
    while (q < end) {
        uint8_t ev = *q++, type = (ev >> 4) & 7, mus = ev & 15;
        int ch;

        if (type == 6) {
            break;
        }
        if (channel_of[mus] < 0) {
            // as the channels are first used, the drums on 10; an all notes
            // off on each first (mus2mid's fix of "the D_DDTBLU disease")
            if (mus == 15) {
                channel_of[mus] = 9;
            } else {
                used += used == 9;
                channel_of[mus] = used++;
            }
            if (channel_of[mus] > 15 || !add(&b, tick, (uint8_t)(0xB0 | channel_of[mus]), 123, 0)) {
                break;
            }
        }
        ch = channel_of[mus];
        if (type == 0) {
            if (q >= end || !add(&b, tick, (uint8_t)(0x80 | ch), *q++ & 0x7F, 64)) {
                break;
            }
        } else if (type == 1) {
            uint8_t note;
            if (q >= end) {
                break;
            }
            note = *q++;
            if (note & 0x80) {
                if (q >= end) {
                    break;
                }
                velocity[mus] = *q++ & 0x7F;
            }
            if (!add(&b, tick, (uint8_t)(0x90 | ch), note & 0x7F, velocity[mus])) {
                break;
            }
        } else if (type == 2) {
            uint32_t bend;
            if (q >= end) {
                break;
            }
            bend = (uint32_t)*q++ * 64;
            if (!add(&b, tick, (uint8_t)(0xE0 | ch), bend & 0x7F, (bend >> 7) & 0x7F)) {
                break;
            }
        } else if (type == 3) {
            uint8_t c;
            if (q >= end) {
                break;
            }
            c = *q++;
            if (c >= 10 && c <= 14 && !add(&b, tick, (uint8_t)(0xB0 | ch), system[c - 10], 0)) {
                break;
            }
        } else if (type == 4) {
            uint8_t c, v;
            if (q + 2 > end) {
                break;
            }
            c = *q++;
            v = *q++;
            if (v > 127) {
                v = 127;
            }
            if (c == 0) {
                if (!add(&b, tick, (uint8_t)(0xC0 | ch), v, 0)) {
                    break;
                }
            } else if (c <= 9 && !add(&b, tick, (uint8_t)(0xB0 | ch), controllers[c], v)) {
                break;
            }
        } else if (type != 5) {
            break;
        }
        if (ev & 0x80) {
            uint32_t delay;
            if (!varlen(&q, end, &delay)) {
                break;
            }
            tick += delay;
        }
    }
    b.end_tick = tick;
    // 140 ticks a second: 70 to a quarter note of 120 bpm
    ok = finish(&b, 0, 70, song, err, errlen);
    free(b.ev);
    return ok;
}

int mf_is_music(const uint8_t* p, size_t size) {
    return (size >= 14 && memcmp(p, "MThd", 4) == 0)
        || (size >= 26 && memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "RMID", 4) == 0)
        || (size >= 16 && memcmp(p, "MUS\x1A", 4) == 0);
}

int mf_parse(uint8_t* bytes, size_t size, mf_song* song, char* err, int errlen) {
    const uint8_t* p = bytes;
    int ok = 0;

    memset(song, 0, sizeof *song);
    if (size >= 26 && memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "RMID", 4) == 0) {
        // RIFF MIDI: its "data" chunk is the file
        const uint8_t* q = p + 12;
        while (p + size - q >= 8 && memcmp(q, "data", 4) != 0) {
            q += 8 + ((q[4] | q[5] << 8 | q[6] << 16 | (uint32_t)q[7] << 24) + 1) / 2 * 2;
        }
        if (p + size - q >= 8 + 14) {
            size = (size_t)(p + size - q - 8);
            p = q + 8;
        }
    }
    if (size >= 14 && memcmp(p, "MThd", 4) == 0) {
        ok = parse_smf(p, size, song, err, errlen);
    } else if (size >= 16 && memcmp(p, "MUS\x1A", 4) == 0) {
        ok = parse_mus(p, size, song, err, errlen);
    } else {
        snprintf(err, (size_t)errlen, "is not a MIDI file");
    }
    if (!ok) {
        free(song->events);
        free(bytes);
        memset(song, 0, sizeof *song);
        return 0;
    }
    song->file = bytes;
    return 1;
}

int mf_load(const char* path, mf_song* song, char* err, int errlen) {
    FILE* f = fopen(path, "rb");
    uint8_t* bytes;
    long size;

    memset(song, 0, sizeof *song);
    if (f == NULL) {
        snprintf(err, (size_t)errlen, "cannot be read");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    // nothing a MIDI file needs is this big
    if (size <= 0 || size > (32 << 20)) {
        fclose(f);
        snprintf(err, (size_t)errlen, size <= 0 ? "is empty" : "is too big");
        return 0;
    }
    bytes = malloc((size_t)size);
    if (bytes == NULL || fread(bytes, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(bytes);
        snprintf(err, (size_t)errlen, "cannot be read");
        return 0;
    }
    fclose(f);
    return mf_parse(bytes, (size_t)size, song, err, errlen);
}

void mf_free(mf_song* song) {
    free(song->events);
    free(song->file);
    memset(song, 0, sizeof *song);
}
