// The song list: My songs and the installed ports' music
#include "library.h"
#include "names.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "archive.h"

// How a game's music is found in its file
enum { MUSIC_DOOM, MUSIC_DOOM2, MUSIC_HERETIC, MUSIC_HEXEN, MUSIC_STRIFE, MUSIC_GRP };

// The games of the installed ports with MIDI music (docs/midiplayer.md): the
// port's folder, the files to look for in it (the full game first), how
static const struct {
    const char* name;
    const char* port;
    const char* files[3];
    int music;
    int titles;  // the music's titles (names.h)
} Games[] = {
    { "Doom", "CrispyDoom", { "DOOM.WAD", "DOOM1.WAD" }, MUSIC_DOOM, TITLES_DOOM },
    { "Doom II", "CrispyDoom", { "DOOM2.WAD" }, MUSIC_DOOM2, TITLES_DOOM2 },
    { "Final Doom - TNT Evilution", "CrispyDoom", { "TNT.WAD" }, MUSIC_DOOM2, TITLES_TNT },
    { "Final Doom - Plutonia", "CrispyDoom", { "PLUTONIA.WAD" }, MUSIC_DOOM2, TITLES_PLUTONIA },
    { "Heretic", "CrispyDoom", { "HERETIC.WAD", "HERETIC1.WAD" }, MUSIC_HERETIC, TITLES_NONE },
    { "Hexen", "CrispyDoom", { "HEXEN.WAD" }, MUSIC_HEXEN, TITLES_NONE },
    { "Strife", "CrispyDoom", { "STRIFE1.WAD", "STRIFE0.WAD" }, MUSIC_STRIFE, TITLES_NONE },
    { "Shadow Warrior", "jfSW", { "*.GRP" }, MUSIC_GRP, TITLES_NONE },
};
#define GAMES ((int)(sizeof(Games) / sizeof(Games[0])))

// Doom II's music by map (sounds.c of the Doom source: mus_runnin is MAP01)
static const char* const Doom2Maps[32] = {
    "RUNNIN", "STALKS", "COUNTD", "BETWEE", "DOOM", "THE_DA", "SHAWN", "DDTBLU",
    "IN_CIT", "DEAD", "STLKS2", "THEDA2", "DOOM2", "DDTBL2", "RUNNI2", "DEAD2",
    "STLKS3", "ROMERO", "SHAWN2", "MESSAG", "COUNT2", "DDTBL3", "AMPIE", "THEDA3",
    "ADRIAN", "MESSG2", "ROMER2", "TENSE", "SHAWN3", "OPENIN", "EVIL", "ULTIMA",
};

// Hexen's music outside its maps (the title, the hub, the finales)
static const char* const HexenOther[] = { "HEXEN", "HUB", "HALL", "ORB", "CHESS" };

#define DEPTH 4  // how deep My songs' folders are looked into

const char* lib_game_name(int game) {
    return game >= 0 && game < GAMES ? Games[game].name : "My songs";
}

const char* lib_port_dir(const char* port, char* out, int size) {
    const char* env = getenv("MIDIPLAYER_DNK");
    struct stat st;
    int i;

    if (env != NULL && env[0] != 0) {
        snprintf(out, (size_t)size, "%s/%s", env, port);
        return out;
    }
    for (i = 0; i < 8; i++) {
        snprintf(out, (size_t)size, "/media/usb%d/games/dnk/%s", i, port);
        if (stat(out, &st) == 0 && S_ISDIR(st.st_mode)) {
            return out;
        }
    }
    snprintf(out, (size_t)size, "/media/fat/games/dnk/%s", port);
    return out;
}

static int is_song_file(const char* name) {
    static const char* const ext[] = { ".mid", ".midi", ".kar", ".rmi", ".mus" };
    const char* dot = strrchr(name, '.');
    int i;

    for (i = 0; dot != NULL && i < (int)(sizeof ext / sizeof ext[0]); i++) {
        if (strcasecmp(dot, ext[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static li_item* add(li_folder* f, int kind, const char* name) {
    li_item* items = realloc(f->items, (size_t)(f->count + 1) * sizeof *items);
    li_item* it;

    if (items == NULL) {
        return NULL;
    }
    f->items = items;
    it = &items[f->count++];
    memset(it, 0, sizeof *it);
    it->kind = kind;
    it->game = f->game;
    snprintf(it->name, sizeof it->name, "%s", name);
    return it;
}

// The songs in a folder and the ones below it, up to DEPTH
static int count_songs(const char* dir, int depth) {
    DIR* d = opendir(dir);
    struct dirent* e;
    int n = 0;

    if (d == NULL) {
        return 0;
    }
    while ((e = readdir(d)) != NULL && n < 100000) {
        char path[1024];
        struct stat st;

        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            n += depth > 0 ? count_songs(path, depth - 1) : 0;
        } else if (is_song_file(e->d_name)) {
            n++;
        }
    }
    closedir(d);
    return n;
}

static int by_name(const void* a, const void* b) {
    const li_item* x = a;
    const li_item* y = b;

    if (x->kind != y->kind) {
        return x->kind == LI_DIR ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

// A folder's folders (those with songs) and songs, by name, folders first
static void add_dir(li_folder* f, const char* dir) {
    DIR* d = opendir(dir);
    struct dirent* e;
    int first = f->count;

    if (d == NULL) {
        return;
    }
    while ((e = readdir(d)) != NULL) {
        char path[1024];
        struct stat st;
        li_item* it;
        int songs = 0;

        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            songs = count_songs(path, DEPTH - 1);
            if (songs == 0) {
                continue;
            }
            it = add(f, LI_DIR, e->d_name);
        } else if (is_song_file(e->d_name) && st.st_size > 0) {
            it = add(f, LI_SONG, e->d_name);
        } else {
            continue;
        }
        if (it == NULL) {
            break;
        }
        snprintf(it->file, sizeof it->file, "%s", strcmp(dir, ".") == 0 ? e->d_name : path);
        it->songs = songs;
    }
    closedir(d);
    qsort(f->items + first, (size_t)(f->count - first), sizeof *f->items, by_name);
}

// A file of the folder by name, case ignored: its path
static int find_file(const char* dir, const char* name, char* path, int size) {
    DIR* d = opendir(dir);
    struct dirent* e;
    int found = 0;

    if (d == NULL) {
        return 0;
    }
    while (!found && (e = readdir(d)) != NULL) {
        if (strcasecmp(e->d_name, name) == 0) {
            snprintf(path, (size_t)size, "%s/%s", dir, e->d_name);
            found = 1;
        }
    }
    closedir(d);
    return found;
}

static int has_music(const ar_dir* a) {
    int i;

    for (i = 0; i < a->count; i++) {
        const char* dot = strrchr(a->entries[i].name, '.');
        if (dot != NULL && strcasecmp(dot, ".MID") == 0) {
            return 1;
        }
    }
    return 0;
}

static void add_game(li_folder* f, int game, const char* path) {
    li_item* it = add(f, LI_GAME, Games[game].name);
    const char* slash = strrchr(path, '/');

    if (it != NULL) {
        it->game = game;
        snprintf(it->file, sizeof it->file, "%s", path);
        // the file it is read from, on the right
        snprintf(it->entry, sizeof it->entry, "%.12s", slash ? slash + 1 : path);
    }
}

// The games whose data is there
static void add_games(li_folder* f) {
    char dir[512], path[1024];
    int g, k;

    for (g = 0; g < GAMES; g++) {
        lib_port_dir(Games[g].port, dir, sizeof dir);
        if (Games[g].music == MUSIC_GRP) {
            // every GRP with music: Shadow Warrior's and its addons'
            DIR* d = opendir(dir);
            struct dirent* e;
            while (d != NULL && (e = readdir(d)) != NULL) {
                size_t len = strlen(e->d_name);
                ar_dir a;
                if (len < 5 || strcasecmp(e->d_name + len - 4, ".GRP") != 0) {
                    continue;
                }
                snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
                if (ar_open(path, &a)) {
                    if (has_music(&a)) {
                        add_game(f, g, path);
                    }
                    ar_close(&a);
                }
            }
            if (d != NULL) {
                closedir(d);
            }
            continue;
        }
        for (k = 0; k < 3 && Games[g].files[k] != NULL; k++) {
            if (find_file(dir, Games[g].files[k], path, sizeof path)) {
                add_game(f, g, path);
                break;
            }
        }
    }
}

void lib_open_top(li_folder* f) {
    int start;

    memset(f, 0, sizeof *f);
    f->game = -1;
    snprintf(f->dir, sizeof f->dir, ".");
    snprintf(f->title, sizeof f->title, "MIDI Player");
    add(f, LI_HEAD, "My songs");
    start = f->count;
    add_dir(f, ".");
    if (f->count == start) {
        add(f, LI_NOTE, "Copy .mid files to");
        add(f, LI_NOTE, "games/dnk/MIDIPlayer/");
    }
    start = f->count;
    add(f, LI_HEAD, "From the installed ports");
    add_games(f);
    if (f->count == start + 1) {
        f->count--;
    }
}

void lib_open_dir(li_folder* f, const char* dir) {
    const char* p;
    int n;

    memset(f, 0, sizeof *f);
    f->game = -1;
    snprintf(f->dir, sizeof f->dir, "%s", dir);
    // "My songs / Classical / Bach"
    n = snprintf(f->title, sizeof f->title, "My songs / ");
    for (p = dir; *p != 0 && n < (int)sizeof f->title - 4; p++) {
        if (*p == '/') {
            n += snprintf(f->title + n, sizeof f->title - (size_t)n, " / ");
        } else {
            f->title[n++] = *p;
            f->title[n] = 0;
        }
    }
    add(f, LI_UP, "..");
    add_dir(f, dir);
}

// A song of a game file, once: of two entries with its name the last, the
// one the game plays
static li_item* add_entry(li_folder* f, const char* file, const ar_entry* e, const char* name) {
    li_item* it;
    int i;

    for (i = 0; i < f->count; i++) {
        if (f->items[i].kind == LI_SONG && strcasecmp(f->items[i].entry, e->name) == 0) {
            f->items[i].offset = e->offset;
            f->items[i].size = e->size;
            return &f->items[i];
        }
    }
    it = add(f, LI_SONG, name);

    if (it != NULL) {
        snprintf(it->file, sizeof it->file, "%s", file);
        snprintf(it->entry, sizeof it->entry, "%s", e->name);
        it->offset = e->offset;
        it->size = e->size;
    }
    return it;
}

// A song of id's games by its map ("MAP01", "E1M1") or what it plays for
// ("Title"), else its lump, and its title where it has one
static void song_name(char* out, int size, const char* label, int titles, const char* lump) {
    const char* title = music_title(titles, lump);
    const char* place = titles != TITLES_NONE ? music_place(lump) : NULL;

    if (label == NULL) {
        label = place != NULL ? place : lump;
    }
    if (title != NULL) {
        snprintf(out, (size_t)size, "%s  %s", label, title);
    } else {
        snprintf(out, (size_t)size, "%s", label);
    }
}

// "GUARDIAN OF ICE" as "Guardian of Ice"
static void title_case(char* s) {
    static const char* const small[] = { "of ", "the ", "and ", "a " };
    char* first = s;
    int start = 1, i;

    for (; *s != 0; s++) {
        *s = (char)(start ? toupper((unsigned char)*s) : tolower((unsigned char)*s));
        start = *s == ' ' || *s == '-';
    }
    for (s = first + 1; *s != 0; s++) {
        for (i = 0; s[-1] == ' ' && i < (int)(sizeof small / sizeof small[0]); i++) {
            if (strncasecmp(s, small[i], strlen(small[i])) == 0) {
                *s = (char)tolower((unsigned char)*s);
            }
        }
    }
}

// A text lump, 0-terminated (malloc'ed)
static char* read_text(const char* file, const ar_dir* a, const char* name) {
    int k = ar_find(a, name);
    uint8_t* bytes = k >= 0 ? ar_read(file, a->entries[k].offset, a->entries[k].size + 1) : NULL;

    if (bytes != NULL) {
        bytes[a->entries[k].size] = 0;
    }
    return (char*)bytes;
}

// Hexen's songs: each map's music ("$MAP <n> <lump>" lines of SNDINFO), by
// map, once, named by the map (MAPINFO); then the music outside the maps
static void add_hexen(li_folder* f, const char* file, const ar_dir* a) {
    char* names = read_text(file, a, "MAPINFO");
    char* sounds = read_text(file, a, "SNDINFO");
    char mapname[100][40];
    char music[100][9];
    char name[100];
    char* line;
    char* save = NULL;
    int map = 0, i, k;

    memset(mapname, 0, sizeof mapname);
    memset(music, 0, sizeof music);
    for (line = names ? strtok_r(names, "\r\n", &save) : NULL; line != NULL; line = strtok_r(NULL, "\r\n", &save)) {
        char* q1 = strchr(line, '"');
        char* q2 = q1 ? strchr(q1 + 1, '"') : NULL;

        if (sscanf(line, " map %d", &map) == 1 && map > 0 && map < 100 && q2 != NULL) {
            snprintf(mapname[map], sizeof mapname[map], "%.*s", (int)(q2 - q1 - 1), q1 + 1);
            title_case(mapname[map]);
        }
    }
    for (line = sounds ? strtok_r(sounds, "\r\n", &save) : NULL; line != NULL; line = strtok_r(NULL, "\r\n", &save)) {
        char lump[9];

        if (sscanf(line, " $MAP %d %8s", &map, lump) == 2 && map > 0 && map < 100) {
            memcpy(music[map], lump, sizeof lump);
        }
    }
    free(names);
    free(sounds);
    for (map = 1; map < 100; map++) {
        if (music[map][0] != 0 && (k = ar_find(a, music[map])) >= 0) {
            snprintf(name, sizeof name, "MAP%02d  %s", map, mapname[map]);
            add_entry(f, file, &a->entries[k], name);
        }
    }
    for (i = 0; i < (int)(sizeof HexenOther / sizeof HexenOther[0]); i++) {
        if ((k = ar_find(a, HexenOther[i])) >= 0) {
            add_entry(f, file, &a->entries[k], HexenOther[i]);
        }
    }
}

void lib_open_game(li_folder* f, int game, const char* file) {
    const char* slash = strrchr(file, '/');
    char name[100];
    ar_dir a;
    int i, m;

    memset(f, 0, sizeof *f);
    f->game = game;
    snprintf(f->title, sizeof f->title, "%s  (%s)", Games[game].name, slash ? slash + 1 : file);
    add(f, LI_UP, "..");
    if (!ar_open(file, &a)) {
        return;
    }
    switch (Games[game].music) {
    case MUSIC_DOOM2:
        // the maps' music in their order, then the rest
        for (m = 0; m < 32; m++) {
            snprintf(name, sizeof name, "D_%s", Doom2Maps[m]);
            if ((i = ar_find(&a, name)) >= 0) {
                char map[8];
                snprintf(map, sizeof map, "MAP%02d", m + 1);
                song_name(name, sizeof name, map, Games[game].titles, Doom2Maps[m]);
                add_entry(f, file, &a.entries[i], name);
            }
        }
        for (i = 0; i < a.count; i++) {
            const char* n = a.entries[i].name;
            for (m = 0; m < 32 && strcasecmp(n + 2, Doom2Maps[m]) != 0; m++) {
            }
            if (strncasecmp(n, "D_", 2) == 0 && m == 32) {
                song_name(name, sizeof name, NULL, Games[game].titles, n + 2);
                add_entry(f, file, &a.entries[i], name);
            }
        }
        break;
    case MUSIC_DOOM:
    case MUSIC_STRIFE:
    case MUSIC_HERETIC: {
        const char* prefix = Games[game].music == MUSIC_HERETIC ? "MUS_" : "D_";
        size_t len = strlen(prefix);
        // the maps' music first ("E1M1", by its map), then the rest (by what
        // it plays for)
        for (m = 1; m >= 0; m--) {
            for (i = 0; i < a.count; i++) {
                const char* lump = a.entries[i].name + len;
                if (strncasecmp(a.entries[i].name, prefix, len) != 0) {
                    continue;
                }
                int map = strlen(lump) == 4 && lump[0] == 'E' && lump[2] == 'M';
                if (map == m) {
                    song_name(name, sizeof name, map ? lump : NULL, Games[game].titles, lump);
                    add_entry(f, file, &a.entries[i], name);
                }
            }
        }
        break;
    }
    case MUSIC_HEXEN:
        add_hexen(f, file, &a);
        break;
    case MUSIC_GRP:
        for (i = 0; i < a.count; i++) {
            const char* dot = strrchr(a.entries[i].name, '.');
            if (dot != NULL && strcasecmp(dot, ".MID") == 0) {
                snprintf(name, sizeof name, "%.*s", (int)(dot - a.entries[i].name), a.entries[i].name);
                add_entry(f, file, &a.entries[i], name);
            }
        }
        break;
    }
    ar_close(&a);
}

void lib_close(li_folder* f) {
    free(f->items);
    memset(f, 0, sizeof *f);
}

int lib_load(const li_item* it, mf_song* song, char* err, int errlen) {
    uint8_t* bytes;

    if (it->size == 0) {
        return mf_load(it->file, song, err, errlen);
    }
    bytes = ar_read(it->file, it->offset, it->size);
    if (bytes == NULL) {
        memset(song, 0, sizeof *song);
        snprintf(err, (size_t)errlen, "cannot be read");
        return 0;
    }
    return mf_parse(bytes, it->size, song, err, errlen);
}

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int lib_lengths(li_folder* f, int ms) {
    double until = now_ms() + ms;

    while (f->next_length < f->count) {
        li_item* it = &f->items[f->next_length++];
        mf_song song;
        char err[80];

        if (it->kind != LI_SONG) {
            continue;
        }
        if (lib_load(it, &song, err, sizeof err) && song.notes > 0) {
            it->length = song.length;
            it->length_state = LEN_KNOWN;
        } else {
            it->length_state = LEN_BAD;
        }
        mf_free(&song);
        if (now_ms() >= until) {
            break;
        }
    }
    return f->next_length >= f->count;
}

void lib_where(const li_folder* f, const li_item* it, char* out, int size) {
    const char* slash = strrchr(it->file, '/');

    if (it->size > 0) {
        snprintf(out, (size_t)size, "%s  -  %s in %s", lib_game_name(it->game), it->entry, slash ? slash + 1 : it->file);
    } else {
        snprintf(out, (size_t)size, "%s", it->file[0] == '.' && it->file[1] == '/' ? it->file + 2 : it->file);
    }
    (void)f;
}
