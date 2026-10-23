// MIDI Player (docs/midiplayer.md in hybrid/): plays MIDI files and the
// installed ports' music on the core's synthesizer or an MT32-pi, and picks
// the SoundFonts of every core's OSD slots
#include <stdio.h>

#include "mister_hybrid.h"
#include "player.h"
#include "ui.h"

#define EXIT_TO_MENU 0
#define EXIT_RELOADED 43 // danik_hybrid_launch.sh starts it again once the core has settled

int main(void) {
    // the core's one "game": always there
    static const char* const none[] = { NULL };
    static const mh_game player = { "MIDIPlayer", "MIDI Player", none, NULL, NULL };
    int listed = 0;

    if (!MH_Open()) {
        fprintf(stderr, "midiplayer: no MIDIPlayer core\n");
        return 1;
    }
    // the page on the core's purple screen (host version 17): after the
    // logo, which MH_PickGame() shows with "Loading MIDI Player...", it goes
    // out and the page comes in its place
    MH_Overlay(1);
    if (MH_PickGame(&player, 1, NULL, NULL, &listed) < 0) {
        MH_Close();
        return EXIT_TO_MENU;
    }
    ui_init();
    MH_AudioEnable(1);
    MH_MidiEnable(1);
    MH_MidiVolume(32);
    if (!pl_start()) {
        fprintf(stderr, "midiplayer: no audio thread\n");
        MH_Close();
        return 1;
    }
    ui_run();
    pl_quit();
    MH_MidiEnable(0);
    MH_AudioEnable(0);
    if (MH_CoreReloaded()) {
        MH_Close();
        return EXIT_RELOADED;
    }
    MH_Close();
    return EXIT_TO_MENU;
}
