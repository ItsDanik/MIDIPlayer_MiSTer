// Plays a song: a thread of its own that queues the audio (silence: the
// music is the core's) and sends each MIDI event with its audio frame
// (MH_MidiEvent()), as a game's audio thread does. Seeking, pausing, muting
// and soloing channels; what each channel plays and how loud.
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

#include "midifile.h"

#define PL_CHANNELS 16

typedef struct {
    int song;            // a song is there
    int paused;
    int ended;           // it played to its end (until pl_play() or pl_seek())
    int waiting;         // MIDI is wanted but no notes sound yet (the SoundFont loads): time stands
    int silent;          // no MIDI: no SoundFont and no MT32-pi: time stands
    uint32_t pos, length;  // audio frames
    uint16_t used;       // channels the song has notes on
    uint8_t program[PL_CHANNELS];
    uint8_t bank[PL_CHANNELS];   // as the synthesizer takes bank select (GS)
    uint8_t drum[PL_CHANNELS];
    float level[PL_CHANNELS];    // 0..1
} pl_state;

// After MH_AudioEnable(1) and MH_MidiEnable(1)
int pl_start(void);
void pl_quit(void);
// Takes the song (NULL: none) and plays it from the start, or holds it there
void pl_play(mf_song* song, int paused);
void pl_pause(int paused);
void pl_seek(uint32_t frame);
// Channel bits: a muted channel gets all notes off at once and no notes
// after; with any solo, every channel not solo is muted
void pl_mute(uint16_t muted, uint16_t solo);
void pl_state_get(pl_state* st);

#endif
