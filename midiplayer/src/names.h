#ifndef NAMES_H
#define NAMES_H

// A program's General MIDI name ("Drums" on a drum channel)
const char* gm_instrument(int program, int drum);

// The music titles of id's games (Doom, Doom II, Final Doom), by the lump's
// name without its D_; NULL for an untitled one
enum { TITLES_NONE, TITLES_DOOM, TITLES_DOOM2, TITLES_TNT, TITLES_PLUTONIA };
const char* music_title(int set, const char* lump);
// What a lump that belongs to no map plays for ("Title", "Intermission"),
// NULL for a map's
const char* music_place(const char* lump);

#endif
