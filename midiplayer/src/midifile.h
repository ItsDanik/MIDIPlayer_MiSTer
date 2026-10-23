// A song as one list of MIDI events in time: from a Standard MIDI File
// (format 0, 1 and 2, also in a RIFF RMID file) or a MUS lump (Doom's,
// Heretic's, Hexen's and Strife's music), read from memory.
#ifndef MIDIFILE_H
#define MIDIFILE_H

#include <stddef.h>
#include <stdint.h>

#define MF_TEXT_LINES 3
#define MF_TEXT_COLS 80

typedef struct {
    uint32_t frame;      // 44.1kHz audio frame from the start of the song
    uint8_t status;      // 0x80..0xEF, 0xF0 for a SysEx
    uint8_t data1, data2;
    uint16_t length;     // SysEx: bytes in data (without F0 and F7)
    const uint8_t* data;
} mf_event;

typedef struct {
    mf_event* events;
    int count;
    uint32_t length;     // in audio frames, to the end of the last track
    int notes;           // note-ons: 0 for a file with nothing to hear
    // text, copyright and track name events of the first track before the
    // first note, up to MF_TEXT_LINES
    char text[MF_TEXT_LINES][MF_TEXT_COLS];
    int text_lines;
    uint8_t* file;       // the bytes the events point into (owned)
} mf_song;

// Takes the bytes (malloc'ed) in any case: they are the song's or freed.
// 0 when they are no MIDI or MUS file, with the reason in err.
int mf_parse(uint8_t* bytes, size_t size, mf_song* song, char* err, int errlen);
// Reads a file and parses it
int mf_load(const char* path, mf_song* song, char* err, int errlen);
void mf_free(mf_song* song);
// 1 if the bytes start like a file mf_parse() takes
int mf_is_music(const uint8_t* bytes, size_t size);

#endif
