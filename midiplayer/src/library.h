// The song list of the Player page (docs/midiplayer.md, "Songs"): at the top
// My songs (the player's folder: .mid, .midi, .kar, .rmi and .mus files and
// the folders with some in them) and From the installed ports (a folder for
// each game whose data is there, its music read in place from its files).
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdint.h>

#include "midifile.h"

enum {
    LI_HEAD,    // a heading: My songs, From the installed ports
    LI_NOTE,    // a line to read: where songs go
    LI_UP,      // ..
    LI_DIR,     // a folder of My songs
    LI_GAME,    // a game of an installed port
    LI_SONG,
};

enum { LEN_UNKNOWN, LEN_KNOWN, LEN_BAD };

typedef struct {
    int kind;
    char name[100];      // as listed
    char file[1024];     // a song's file (its own, or the game file it is in), a folder, a game's file
    uint32_t offset;     // a song in a game file: where it is in it
    uint32_t size;       // and its size; 0 for a file of its own
    char entry[13];      // a song: its name in the game file; a game: its file's name
    int game;            // a game, and the songs of one: which (lib_game_name())
    int songs;           // a folder: the songs in it
    int length_state;    // a song: LEN_*
    uint32_t length;     // in audio frames
} li_item;

typedef struct {
    int game;            // -1 for My songs
    char dir[512];       // My songs: the folder ("." its top)
    char title[128];     // the path shown above the list
    li_item* items;
    int count;
    int next_length;     // the next song whose length lib_lengths() works out
} li_folder;

// The top: My songs and the games
void lib_open_top(li_folder* f);
// A folder of My songs
void lib_open_dir(li_folder* f, const char* dir);
// A game's songs
void lib_open_game(li_folder* f, int game, const char* file);
void lib_close(li_folder* f);
// Works out the lengths of the songs, a few at a time (at most `ms`
// milliseconds); 1 once all are known
int lib_lengths(li_folder* f, int ms);
// Reads a song
int lib_load(const li_item* item, mf_song* song, char* err, int errlen);
// Where a song comes from, for the Player page: "Classical/Canyon.mid",
// "Doom  -  D_E1M1 in DOOM.WAD"
void lib_where(const li_folder* f, const li_item* item, char* out, int size);
const char* lib_game_name(int game);
// The folder a port keeps its game data in (the DATADIR rule): its own on the
// first USB drive that has one, else the SD card's; MIDIPLAYER_DNK in the
// environment for another games/dnk (tests on the PC)
const char* lib_port_dir(const char* port, char* out, int size);

#endif
