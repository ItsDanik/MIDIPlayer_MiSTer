// General MIDI's instrument names: what a channel plays on the MT32-pi, and
// on the synthesizer when the font is not known
#include "names.h"

#include <stddef.h>
#include <strings.h>

static const char* const Instruments[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
    "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
    "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass", "Slap Bass 1",
    "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
    "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2", "Choir Aahs", "Voice Oohs",
    "Synth Voice", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)", "Lead 5 (charang)",
    "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
    "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)", "Pad 5 (bowed)", "Pad 6 (metallic)",
    "Pad 7 (halo)", "Pad 8 (sweep)",
    "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)", "FX 5 (brightness)",
    "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
    "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter", "Applause",
    "Gunshot",
};

const char* gm_instrument(int program, int drum) {
    return drum ? "Drums" : Instruments[program & 127];
}

// The titles of id's music, as the Doom Wiki lists them (doomwiki.org: "Doom
// music", "Doom II music", "Final Doom TNT music"), by music lump without
// its D_. A lump not listed is untitled.
typedef struct {
    const char* lump;
    const char* title;
} title;

static const title Doom[] = {
    { "E1M1", "At Doom's Gate" }, { "E1M2", "The Imp's Song" }, { "E1M3", "Dark Halls" },
    { "E1M4", "Kitchen Ace (And Taking Names)" }, { "E1M5", "Suspense" }, { "E1M6", "On the Hunt" },
    { "E3M6", "On the Hunt" }, { "E1M7", "Demons on the Prey" }, { "E2M5", "Demons on the Prey" },
    { "E3M5", "Demons on the Prey" }, { "E1M8", "Sign of Evil" }, { "E3M4", "Sign of Evil" },
    { "E1M9", "Hiding the Secrets" }, { "E3M9", "Hiding the Secrets" }, { "E2M1", "I Sawed the Demons" },
    { "E2M2", "The Demons from Adrian's Pen" }, { "E2M3", "Intermission from DOOM" },
    { "INTER", "Intermission from DOOM" }, { "E2M4", "They're Going to Get You" }, { "E2M6", "Sinister" },
    { "E2M7", "Waltz of the Demons" }, { "E3M7", "Waltz of the Demons" }, { "E2M8", "Nobody Told Me About id" },
    { "E3M2", "Donna to the Rescue" }, { "E3M3", "Deep Into the Code" }, { "E3M8", "Facing the Spider" },
    { "VICTOR", "The End of DOOM" }, { "BUNNY", "Sweet Little Dead Bunny" },
    { NULL, NULL },
};

static const title Doom2[] = {
    { "RUNNIN", "Running from Evil" }, { "RUNNI2", "Running from Evil" }, { "STALKS", "The Healer Stalks" },
    { "STLKS2", "The Healer Stalks" }, { "STLKS3", "The Healer Stalks" }, { "COUNTD", "Countdown to Death" },
    { "COUNT2", "Countdown to Death" }, { "BETWEE", "Between Levels" }, { "DOOM", "DOOM" }, { "DOOM2", "DOOM" },
    { "THE_DA", "In the Dark" }, { "THEDA2", "In the Dark" }, { "THEDA3", "In the Dark" },
    { "SHAWN", "Shawn's Got the Shotgun" }, { "SHAWN2", "Shawn's Got the Shotgun" },
    { "SHAWN3", "Shawn's Got the Shotgun" }, { "DDTBLU", "The Dave D. Taylor Blues" },
    { "DDTBL2", "The Dave D. Taylor Blues" }, { "DDTBL3", "The Dave D. Taylor Blues" },
    { "IN_CIT", "Into Sandy's City" }, { "DEAD", "The Demon's Dead" }, { "DEAD2", "The Demon's Dead" },
    { "ROMERO", "Waiting for Romero to Play" }, { "ROMER2", "Waiting for Romero to Play" },
    { "MESSAG", "Message for the Archvile" }, { "MESSG2", "Message for the Archvile" },
    { "AMPIE", "Bye Bye American Pie" }, { "ADRIAN", "Adrian's Asleep" }, { "TENSE", "Getting Too Tense" },
    { "OPENIN", "Opening to Hell" }, { "EVIL", "Evil Incarnate" }, { "ULTIMA", "The Ultimate Challenge/Conquest" },
    { "READ_M", "Read Me While Listening to This" },
    { NULL, NULL },
};

// Final Doom: TNT Evilution's own music and the Doom II songs it plays
static const title Tnt[] = {
    { "RUNNIN", "Sadistic" }, { "IN_CIT", "Sadistic" }, { "STALKS", "Smells Like Burning Corpse" },
    { "RUNNI2", "Smells Like Burning Corpse" }, { "COUNTD", "Message for the Archvile" },
    { "BETWEE", "Death's Bells" }, { "DOOM2", "Death's Bells" }, { "SHAWN3", "Death's Bells" }, { "DOOM", "More" },
    { "STLKS3", "More" }, { "THE_DA", "Agony Rhapsody" }, { "SHAWN", "Soldier of Chaos" },
    { "DDTBLU", "Into the Beast's Belly" }, { "ROMER2", "Into the Beast's Belly" },
    { "OPENIN", "Into the Beast's Belly" }, { "DEAD", "Infinite" }, { "ROMERO", "Infinite" },
    { "STLKS2", "Let's Kill at Will" }, { "THEDA2", "The Dave D. Taylor Blues" }, { "DDTBL2", "Cold Subtleness" },
    { "DEAD2", "Blood Jungle" }, { "MESSG2", "Blood Jungle" }, { "SHAWN2", "Countdown to Death" },
    { "MESSAG", "Horizon" }, { "COUNT2", "Into Sandy's City" }, { "ULTIMA", "Into Sandy's City" },
    { "DDTBL3", "AimShootKill" }, { "TENSE", "AimShootKill" }, { "AMPIE", "Bye Bye American Pie" },
    { "THEDA3", "Between Levels" }, { "ADRIAN", "DOOM" }, { "EVIL", "Legion of the Lost" },
    { "DM2INT", "Legion of the Lost" }, { "DM2TTL", "TNT Title" },
    { NULL, NULL },
};

// Final Doom: The Plutonia Experiment has no music of its own: each of its
// lumps is a Doom ("1") or Doom II ("2") lump, byte for byte
static const title Plutonia[] = {
    { "RUNNIN", "1E1M2" }, { "STALKS", "1E1M3" }, { "COUNTD", "1E1M6" }, { "BETWEE", "1E1M4" },
    { "DOOM", "1E1M9" }, { "THE_DA", "1E1M8" }, { "SHAWN", "1E2M1" }, { "DDTBLU", "1E2M2" },
    { "IN_CIT", "1E3M3" }, { "DEAD", "1E1M7" }, { "STLKS2", "1BUNNY" }, { "THEDA2", "1E3M8" },
    { "DOOM2", "1E3M2" }, { "DDTBL2", "1E2M8" }, { "RUNNI2", "1E2M7" }, { "DEAD2", "1E2M9" },
    { "STLKS3", "1E1M1" }, { "ROMERO", "1E1M7" }, { "SHAWN2", "1E1M5" }, { "MESSAG", "2MESSAG" },
    { "COUNT2", "2READ_M" }, { "DDTBL3", "2DDTBL2" }, { "AMPIE", "2AMPIE" }, { "THEDA3", "2THE_DA" },
    { "ADRIAN", "2ADRIAN" }, { "MESSG2", "2MESSAG" }, { "ROMER2", "1E2M1" }, { "TENSE", "1E2M2" },
    { "SHAWN3", "1E1M1" }, { "OPENIN", "1VICTOR" }, { "EVIL", "1E1M8" }, { "ULTIMA", "1E2M8" },
    { "READ_M", "2READ_M" }, { "DM2TTL", "2DM2TTL" }, { "DM2INT", "2DM2INT" },
    { NULL, NULL },
};

static const char* find_title(const title* t, const char* lump) {
    for (; t->lump != NULL; t++) {
        if (strcasecmp(t->lump, lump) == 0) {
            return t->title;
        }
    }
    return NULL;
}

const char* music_title(int set, const char* lump) {
    const char* source;

    switch (set) {
    case TITLES_DOOM: return find_title(Doom, lump);
    case TITLES_DOOM2: return find_title(Doom2, lump);
    case TITLES_TNT: return find_title(Tnt, lump);
    case TITLES_PLUTONIA:
        source = find_title(Plutonia, lump);
        return source == NULL ? NULL : find_title(source[0] == '1' ? Doom : Doom2, source + 1);
    default: return NULL;
    }
}

const char* music_place(const char* lump) {
    static const title places[] = {
        { "INTRO", "Title" }, { "INTROA", "Title (unused)" }, { "INTER", "Intermission" }, { "VICTOR", "Victory" },
        { "BUNNY", "Endgame" }, { "DM2TTL", "Title" }, { "DM2INT", "Intermission" }, { "READ_M", "Text screen" },
        { NULL, NULL },
    };

    return find_title(places, lump);
}
