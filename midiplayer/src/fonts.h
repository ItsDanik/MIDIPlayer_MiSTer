// The SoundFonts of games/dnk/SoundFonts/ (MH_SoundFontDir()): what each is
// (its size, its presets, whether the core can load it) and the 8 slots of
// every core's OSD line SoundFont, in slots.txt (docs/midiplayer.md).
#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

#define FONT_SLOTS 8
#define FONT_NAME 256
// the core's sample memory (MH_MIDI_MAX_SAMPLES 16-bit samples)
#define FONT_MAX_SAMPLE_BYTES (64u << 20)

enum { FONT_OK, FONT_TOO_BIG, FONT_DAMAGED };

typedef struct {
    char file[FONT_NAME];   // in the folder
    uint32_t sample_bytes;
    int presets;
    int state;              // FONT_*
} font_info;

typedef struct {
    uint16_t bank, program;
    char name[21];
} font_preset;

// The .sf2 files of the folder, by name (case ignored); the count
int fonts_scan(const char* dir, font_info** list);
// What a file is (FONT_*), *info filled in
int fonts_check(const char* path, font_info* info);
// A font's presets (malloc'ed); the count, 0 when it has none or is damaged
int fonts_presets(const char* path, font_preset** list);
// The name of the preset a channel plays, NULL when the font has none: as the
// synthesizer picks it (hps/mister_midi.c, program()): the bank's program,
// else bank 0's (128's on a drum channel), else program 0 of that bank
const char* fonts_preset_name(const font_preset* list, int count, int bank, int program, int drum);

#endif
