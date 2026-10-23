// The song's audio thread
#include "player.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "mister_hybrid.h"

#define BLOCK 512     // audio frames a step: 11.6ms
#define LEAD 2048     // queued ahead: what a pause or a seek takes to be heard
#define SYSEX_KEPT 64 // the SysEx messages sent again with the channels' settings

// The channels as the song has set them so far: what goes out again after a
// seek, a pause or a change of synthesizer
typedef struct {
    uint8_t cc[128];
    uint8_t cc_set[128];
    uint8_t program, program_set;
    uint16_t bend;
    uint8_t bend_set;
    uint8_t pressure, pressure_set;
    uint16_t rpn;           // the RPN selected (101 << 7 | 100)
    uint8_t rpn_value[3][2];  // pitch bend range, fine and coarse tuning: data entry MSB, LSB
    uint8_t rpn_set[3];
    uint8_t note[128];      // velocity of each note sounding, 0 for none
    uint8_t drum;
} channel;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t thread;
static int running, quit;

static mf_song* song;
static int next;            // the next event
static uint32_t pos;
static int paused, ended;
static uint16_t used;
static uint16_t muted, solo, silenced;  // silenced: muted as sent
static channel ch[PL_CHANNELS];
static const mf_event* sysex[SYSEX_KEPT];
static int sysexes;
static float level[PL_CHANNELS];
static int ready, wanted;
static int resend;          // the channels' settings go out again before the next events
static int hush;            // every note stops (a pause, the end): MIDI goes out from this thread only
static int wait;            // audio frames the song holds still for the settings to reach an MT32-pi

static void reset_channels(void) {
    int c;

    memset(ch, 0, sizeof ch);
    for (c = 0; c < PL_CHANNELS; c++) {
        ch[c].drum = c == 9;
        ch[c].rpn = 0x3FFF;
    }
    sysexes = 0;
}

// GM on, the GS reset and the XG reset: the synthesizer's channels go back to
// how they start
static int is_reset(const uint8_t* d, int n) {
    return (n >= 4 && d[0] == 0x7E && d[2] == 0x09 && (d[3] == 0x01 || d[3] == 0x03))
        || (n >= 9 && d[0] == 0x41 && d[2] == 0x42 && d[3] == 0x12 && d[4] == 0x40 && d[5] == 0x00 && d[6] == 0x7F)
        || (n >= 7 && d[0] == 0x43 && (d[1] & 0xF0) == 0x10 && d[2] == 0x4C && d[3] == 0 && d[4] == 0 && d[5] == 0x7E);
}

// A GS drum part setting: "use for rhythm part" of a part (40 1x 15)
static void gs_drum_part(const uint8_t* d, int n) {
    static const int part_channel[16] = { 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15 };

    if (n >= 8 && d[0] == 0x41 && d[2] == 0x42 && d[3] == 0x12 && d[4] == 0x40 && (d[5] & 0xF0) == 0x10 && d[6] == 0x15) {
        ch[part_channel[d[5] & 15]].drum = d[7] != 0;
    }
}

// The song's event in the channels' state
static void track(const mf_event* e) {
    channel* c = &ch[e->status & 15];
    int k;

    switch (e->status & 0xF0) {
    case 0x80:
        c->note[e->data1] = 0;
        break;
    case 0x90:
        c->note[e->data1] = e->data2;
        break;
    case 0xB0:
        c->cc[e->data1] = e->data2;
        c->cc_set[e->data1] = 1;
        if (e->data1 == 101) {
            c->rpn = (uint16_t)(e->data2 << 7 | (c->rpn & 0x7F));
        } else if (e->data1 == 100) {
            c->rpn = (uint16_t)((c->rpn & 0x3F80) | e->data2);
        } else if ((e->data1 == 6 || e->data1 == 38) && c->rpn < 3) {
            c->rpn_value[c->rpn][e->data1 == 38] = e->data2;
            c->rpn_set[c->rpn] = 1;
        } else if (e->data1 == 98 || e->data1 == 99) {
            // an NRPN: data entry is no longer an RPN's
            c->rpn = 0x3FFF;
        } else if (e->data1 == 120 || e->data1 == 123) {
            memset(c->note, 0, sizeof c->note);
        } else if (e->data1 == 121) {
            for (k = 1; k < 120; k++) {
                c->cc_set[k] = 0;
            }
            c->bend_set = c->pressure_set = 0;
        }
        break;
    case 0xC0:
        c->program = e->data1;
        c->program_set = 1;
        break;
    case 0xD0:
        c->pressure = e->data1;
        c->pressure_set = 1;
        break;
    case 0xE0:
        c->bend = (uint16_t)(e->data2 << 7 | e->data1);
        c->bend_set = 1;
        break;
    case 0xF0:
        if (is_reset(e->data, e->length)) {
            reset_channels();
        }
        gs_drum_part(e->data, e->length);
        if (sysexes < SYSEX_KEPT) {
            sysex[sysexes++] = e;
        }
        break;
    default:
        break;
    }
}

static int is_muted(int c) {
    return ((muted >> c) & 1) || (solo != 0 && !((solo >> c) & 1));
}

static void send(int at, const mf_event* e) {
    int type = e->status & 0xF0, c = e->status & 15;

    if (e->status == 0xF0) {
        track(e);
        MH_MidiSysEx(at, e->data, e->length);
        return;
    }
    if (type == 0x90 && e->data2 > 0 && is_muted(c)) {
        return;
    }
    track(e);
    MH_MidiEvent(at, e->status, e->data1, e->data2);
}

// Every note stopped, the channels as at the start, then as the song has
// set them: after a seek, a pause, a change of synthesizer
static void send_settings(void) {
    static const uint8_t rpn_lsb[3] = { 0, 1, 2 };
    int c, k;

    MH_MidiReset(0);
    for (k = 0; k < sysexes; k++) {
        MH_MidiSysEx(0, sysex[k]->data, sysex[k]->length);
    }
    for (c = 0; c < PL_CHANNELS; c++) {
        channel* s = &ch[c];

        memset(s->note, 0, sizeof s->note);
        if (s->cc_set[0]) {
            MH_MidiEvent(0, 0xB0 | c, 0, s->cc[0]);
        }
        if (s->cc_set[32]) {
            MH_MidiEvent(0, 0xB0 | c, 32, s->cc[32]);
        }
        if (s->program_set) {
            MH_MidiEvent(0, 0xC0 | c, s->program, 0);
        }
        for (k = 0; k < 3; k++) {
            if (s->rpn_set[k]) {
                MH_MidiEvent(0, 0xB0 | c, 101, 0);
                MH_MidiEvent(0, 0xB0 | c, 100, rpn_lsb[k]);
                MH_MidiEvent(0, 0xB0 | c, 6, s->rpn_value[k][0]);
                MH_MidiEvent(0, 0xB0 | c, 38, s->rpn_value[k][1]);
            }
        }
        if (s->rpn_set[0] || s->rpn_set[1] || s->rpn_set[2]) {
            MH_MidiEvent(0, 0xB0 | c, 101, 127);
            MH_MidiEvent(0, 0xB0 | c, 100, 127);
        }
        for (k = 1; k < 120; k++) {
            if (s->cc_set[k] && k != 6 && k != 32 && k != 38 && (k < 96 || k > 101)) {
                MH_MidiEvent(0, 0xB0 | c, k, s->cc[k]);
            }
        }
        if (s->bend_set) {
            MH_MidiEvent(0, 0xE0 | c, s->bend & 0x7F, s->bend >> 7);
        }
        if (s->pressure_set) {
            MH_MidiEvent(0, 0xD0 | c, s->pressure, 0);
        }
    }
    silenced = 0;
    resend = 0;
}

static void notes_off(uint16_t channels) {
    int c;

    for (c = 0; c < PL_CHANNELS; c++) {
        if ((channels >> c) & 1) {
            MH_MidiEvent(0, 0xB0 | c, 64, 0);
            MH_MidiEvent(0, 0xB0 | c, 123, 0);
            memset(ch[c].note, 0, sizeof ch[c].note);
        }
    }
}

static void levels(void) {
    const float fall = BLOCK / (0.25f * MH_AUDIO_RATE);
    int c, k;

    for (c = 0; c < PL_CHANNELS; c++) {
        const channel* s = &ch[c];
        float target = 0;
        int loudest = 0;

        if (!paused && song != NULL) {
            for (k = 0; k < 128; k++) {
                if (s->note[k] > loudest) {
                    loudest = s->note[k];
                }
            }
            target = loudest / 127.0f * (s->cc_set[7] ? s->cc[7] : 100) / 127.0f
                * (s->cc_set[11] ? s->cc[11] : 127) / 127.0f;
        }
        level[c] = target > level[c] - fall ? target : level[c] - fall;
        if (level[c] < 0) {
            level[c] = 0;
        }
    }
}

// One block of the song
static void step(void) {
    uint32_t end = pos + BLOCK;
    uint16_t now = 0;
    int c, was = ready;

    wanted = MH_MidiWanted();
    ready = wanted && MH_MidiReady();
    if (MH_MidiSynthChanged() || (ready && !was)) {
        resend = 1;
    }
    for (c = 0; c < PL_CHANNELS; c++) {
        now |= (uint16_t)(is_muted(c) << c);
    }
    if (now & ~silenced) {
        notes_off(now & ~silenced);
    }
    silenced = now;
    if (hush) {
        notes_off(0xFFFF);
        MH_MidiReset(0);
        hush = 0;
    }
    if (song == NULL || paused || ended || !ready) {
        return;
    }
    if (resend) {
        send_settings();
        // and the song's own up to its next note (a file's start: a GS
        // reset, the programs and volumes of all its channels)
        while (next < song->count && !((song->events[next].status & 0xF0) == 0x90 && song->events[next].data2 > 0)) {
            send(0, &song->events[next++]);
        }
        // on the MT32-pi's cable all that takes a while (31250 baud): the
        // song's first notes went out behind it and their ends nearly on
        // time, cut short. The song waits for it.
        wait = MH_MidiBacklog();
    }
    if (wait >= BLOCK) {
        wait -= BLOCK;
        return;
    }
    // the rest of the wait at the start of this block, the song after it
    end -= (uint32_t)wait;
    while (next < song->count && song->events[next].frame < end) {
        const mf_event* e = &song->events[next++];
        send(wait + (e->frame > pos ? (int)(e->frame - pos) : 0), e);
    }
    wait = 0;
    pos = end;
    if (next >= song->count && pos >= song->length) {
        pos = song->length;
        ended = 1;
        notes_off(0xFFFF);
    }
}

static void* audio_thread(void* arg) {
    static uint32_t silence[BLOCK];

    (void)arg;
    MH_PinThreadToCPU1();
    for (;;) {
        pthread_mutex_lock(&lock);
        if (quit) {
            if (song != NULL) {
                notes_off(0xFFFF);
                MH_MidiReset(0);
            }
            pthread_mutex_unlock(&lock);
            // that much more for the notes to stop
            MH_AudioWrite(silence, BLOCK, LEAD);
            break;
        }
        step();
        levels();
        pthread_mutex_unlock(&lock);
        MH_AudioWrite(silence, BLOCK, LEAD);
    }
    return NULL;
}

int pl_start(void) {
    reset_channels();
    quit = 0;
    running = pthread_create(&thread, NULL, audio_thread, NULL) == 0;
    return running;
}

void pl_quit(void) {
    if (!running) {
        return;
    }
    pthread_mutex_lock(&lock);
    quit = 1;
    pthread_mutex_unlock(&lock);
    pthread_join(thread, NULL);
    running = 0;
    if (song != NULL) {
        mf_free(song);
        free(song);
        song = NULL;
    }
}

void pl_play(mf_song* s, int hold) {
    mf_song* old;
    int c, k;

    pthread_mutex_lock(&lock);
    old = song;
    song = s;
    next = 0;
    pos = 0;
    paused = hold;
    ended = 0;
    used = 0;
    for (k = 0; s != NULL && k < s->count; k++) {
        if ((s->events[k].status & 0xF0) == 0x90) {
            used |= (uint16_t)(1u << (s->events[k].status & 15));
        }
    }
    reset_channels();
    for (c = 0; c < PL_CHANNELS; c++) {
        level[c] = 0;
    }
    // the last song's notes stop and its settings go
    resend = 1;
    pthread_mutex_unlock(&lock);
    if (old != NULL) {
        mf_free(old);
        free(old);
    }
}

void pl_pause(int p) {
    pthread_mutex_lock(&lock);
    if (p && !paused && song != NULL) {
        hush = 1;
    }
    if (!p && paused) {
        resend = 1;
    }
    paused = p;
    pthread_mutex_unlock(&lock);
}

void pl_seek(uint32_t frame) {
    pthread_mutex_lock(&lock);
    if (song != NULL) {
        // through the song without its notes: the channels as they are there
        if (frame > song->length) {
            frame = song->length;
        }
        reset_channels();
        for (next = 0; next < song->count && song->events[next].frame < frame; next++) {
            const mf_event* e = &song->events[next];
            if ((e->status & 0xF0) != 0x90 && (e->status & 0xF0) != 0x80) {
                track(e);
            }
        }
        pos = frame;
        ended = 0;
        resend = 1;
    }
    pthread_mutex_unlock(&lock);
}

void pl_mute(uint16_t m, uint16_t s) {
    pthread_mutex_lock(&lock);
    muted = m;
    solo = s;
    pthread_mutex_unlock(&lock);
}

void pl_state_get(pl_state* st) {
    int c;

    pthread_mutex_lock(&lock);
    memset(st, 0, sizeof *st);
    st->song = song != NULL;
    st->paused = paused;
    st->ended = ended;
    st->silent = !wanted;
    st->waiting = wanted && !ready;
    st->pos = pos;
    st->length = song != NULL ? song->length : 0;
    st->used = used;
    for (c = 0; c < PL_CHANNELS; c++) {
        st->program[c] = ch[c].program;
        st->bank[c] = ch[c].cc[0];
        st->drum[c] = ch[c].drum;
        st->level[c] = is_muted(c) ? 0 : level[c];
    }
    pthread_mutex_unlock(&lock);
}
