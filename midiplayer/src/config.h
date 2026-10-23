// What the player remembers (docs/midiplayer.md): midiplayer.cfg in the
// folder it runs in (DATADIR), written when something in it changes
#ifndef CONFIG_H
#define CONFIG_H

enum { REPEAT_OFF, REPEAT_ONE, REPEAT_ALL };
enum { DEST_SYNTH, DEST_PI };

typedef struct {
    char folder[512];    // "" the top, "<dir>" a folder of My songs, "game:<n>:<file>" a game's
    char song[100];      // the name selected there
    char font[256];      // the font chosen on the SoundFonts page ("" the OSD slot's)
    int shuffle;
    int repeat;          // REPEAT_*
    int dest;            // DEST_*
} config;

void config_load(config* c);
// Writes it if it differs from what was last read or written
void config_save(const config* c);

#endif
