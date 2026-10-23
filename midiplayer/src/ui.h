// The player's two pages, Player and SoundFonts (docs/midiplayer.md), with
// the pad, the keyboard and the mouse
#ifndef UI_H
#define UI_H

// Reads the settings and names the font and synthesizer chosen last: before
// MH_MidiEnable(1), so that the synthesizer loads that font first
void ui_init(void);
// Runs until the core is gone
void ui_run(void);

#endif
