// The Player and SoundFonts pages
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "config.h"
#include "fonts.h"
#include "gfx.h"
#include "library.h"
#include "mister_hybrid.h"
#include "names.h"
#include "player.h"

// The layout (the mockups of docs/midiplayer.md)
#define MARGIN 34
#define LIST_X (MARGIN + 4)
#define LIST_Y 84
#define LIST_W 250
#define LIST_ROWS 18
#define PANE_X 316
#define RIGHT (GFX_W - MARGIN)
#define BUTTONS_Y 400
#define FONT_ROWS 7

// A held direction: fields to its first repeat, and between repeats (even,
// so that they fall on the frames of an interlaced mode without field rate;
// the page has field rate since host version 18, a frame every field)
#define REPEAT_DELAY 18
#define REPEAT_EVERY 4

// The core's J1 list: A, B, X, Y, L, R, Select, Start, Menu OK, Menu Back
enum { B_A, B_B, B_X, B_Y, B_L, B_R, B_SELECT, B_START, B_MENU_OK, B_MENU_BACK };

// What the pad, the keys and the mouse ask for
enum {
    ACT_NONE,
    ACT_UP, ACT_DOWN, ACT_PAGE_UP, ACT_PAGE_DOWN, ACT_HOME, ACT_END, ACT_LEFT, ACT_RIGHT,
    ACT_OK, ACT_BACK, ACT_PAUSE, ACT_PREV, ACT_NEXT, ACT_REPEAT, ACT_SHUFFLE, ACT_GOTO,
    ACT_PAGE, ACT_CLEAR, ACT_SEEK_BACK, ACT_SEEK_FORWARD,
    ACT_TAB_PLAYER, ACT_TAB_FONTS, ACT_DEST,
};

enum { PAGE_PLAYER, PAGE_FONTS };
enum { FOCUS_FONTS, FOCUS_SLOTS };

// USB HID usages (MH_KeyToHID())
enum {
    K_G = 10, K_P = 19, K_R = 21, K_S = 22, K_X = 27, K_1 = 30, K_0 = 39,
    K_ENTER = 40, K_ESC = 41, K_BACKSPACE = 42, K_TAB = 43, K_SPACE = 44, K_COMMA = 54, K_PERIOD = 55,
    K_HOME = 74, K_PAGE_UP = 75, K_END = 77, K_PAGE_DOWN = 78, K_RIGHT = 79, K_LEFT = 80, K_DOWN = 81, K_UP = 82,
    K_LSHIFT = 225, K_RSHIFT = 229,
};

// A place on the screen the mouse can click
typedef struct {
    int x, y, w, h;
    int action;       // ACT_*, or one of the CLICK_* below
    int index;
} hotspot;

enum { CLICK_LIST = 100, CLICK_CHANNEL, CLICK_BAR, CLICK_FONT, CLICK_SLOT };

#define HOTSPOTS 64

static struct {
    int page;
    config cfg;
    li_folder folder;     // the list shown
    int sel, top;
    // the song playing, and the folder it is from (for previous and next)
    li_folder playing;
    int playing_index;    // -1: none
    char title[100];
    char where[300];
    char text[MF_TEXT_LINES][MF_TEXT_COLS];
    int text_lines;
    char error[100];      // the song could not be read: shown 2 seconds, then the next plays
    uint32_t error_field;
    int failures;         // songs in a row that could not be read
    int* order;           // shuffle: the playing folder's items in the order they play
    int order_count;
    uint16_t muted, solo;
    // the SoundFonts page
    char font_dir[256];
    font_info* fonts;
    int font_count, font_sel, font_top;
    char slots[FONT_SLOTS][FONT_NAME];
    int focus, slot_sel, putting;
    char message[100];    // what happened last on the page
    // the font the synthesizer plays (or loads), for the instruments
    char font_path[FONT_NAME + 300], font_file[FONT_NAME];
    int font_state;       // MH_SF_*
    font_preset* presets;
    int preset_count;
    // input
    mh_input in, last;
    int have_last;
    uint32_t keys_down[16];
    int held_action;      // the move a held key or direction repeats
    uint32_t repeat_at;   // and the field of its next step
    int mouse_x, mouse_y;
    uint32_t mouse_moved;
    int click_index, click_area;
    uint32_t click_field;
    hotspot hot[HOTSPOTS];
    int hots;
    uint32_t field;
    uint32_t changed;     // when the config last changed: written 2 seconds later
} ui;

static int music_plays(const pl_state* st);

// Fields since `field`, signed: a time kept as `ui.field | 1` (0 is "not
// set") is one field ahead of an even ui.field, and in an interlaced mode
// without field rate the loop saw the counter even at every frame (the cursor
// was never drawn while the mouse moved). Every time here is in fields, never
// in frames: the page takes a frame at every field (field rate) or every
// second one (an older core), and moves at the same speed in both.
static int32_t since(uint32_t field) {
    return (int32_t)(ui.field - field);
}

// --- songs ---------------------------------------------------------------

static int same_folder(const li_folder* a, const li_folder* b) {
    return a->game == b->game && strcmp(a->dir, b->dir) == 0 && strcmp(a->title, b->title) == 0;
}

static int selectable(const li_item* it) {
    return it->kind != LI_HEAD && it->kind != LI_NOTE;
}

static void keep_in_view(void) {
    if (ui.sel < ui.top) {
        ui.top = ui.sel;
    }
    if (ui.sel >= ui.top + LIST_ROWS) {
        ui.top = ui.sel - LIST_ROWS + 1;
    }
    if (ui.top > 0 && ui.sel - ui.top < 1 && ui.folder.items[ui.top - 1].kind == LI_HEAD) {
        // a heading stays with the first item under it
        ui.top--;
    }
}

static void select_item(int i) {
    if (ui.folder.count == 0) {
        return;
    }
    if (i < 0) {
        i = 0;
    }
    if (i >= ui.folder.count) {
        i = ui.folder.count - 1;
    }
    ui.sel = i;
    keep_in_view();
}

// The next selectable item from i in direction d (or i itself)
static int selectable_from(int i, int d) {
    while (i >= 0 && i < ui.folder.count && !selectable(&ui.folder.items[i])) {
        i += d;
    }
    return i >= 0 && i < ui.folder.count ? i : -1;
}

static void move_selection(int by) {
    int d = by < 0 ? -1 : 1, i = ui.sel + by, found;

    if (i < 0) {
        i = 0;
    }
    if (i >= ui.folder.count) {
        i = ui.folder.count - 1;
    }
    found = selectable_from(i, d);
    if (found < 0) {
        found = selectable_from(i, -d);
    }
    if (found >= 0) {
        select_item(found);
    }
}

static void select_name(const char* name) {
    int i;

    for (i = 0; i < ui.folder.count; i++) {
        if (selectable(&ui.folder.items[i]) && strcmp(ui.folder.items[i].name, name) == 0) {
            select_item(i);
            return;
        }
    }
    ui.top = 0;
    select_item(selectable_from(0, 1) >= 0 ? selectable_from(0, 1) : 0);
}

// The folder in the config's words: "", a folder of My songs, "game:<n>:<file>"
static void folder_key(const li_folder* f, char* out, int size) {
    if (f->game >= 0) {
        // the game file is that of its "..": the songs' file
        const char* file = f->count > 1 ? f->items[1].file : "";
        snprintf(out, (size_t)size, "game:%d:%s", f->game, file);
    } else {
        snprintf(out, (size_t)size, "%s", strcmp(f->dir, ".") == 0 ? "" : f->dir);
    }
}

// The folder and the song selected, for the next start: written once the
// player has stopped moving through the list for a while
static void remember(void) {
    folder_key(&ui.folder, ui.cfg.folder, sizeof ui.cfg.folder);
    if (ui.sel >= 0 && ui.sel < ui.folder.count) {
        snprintf(ui.cfg.song, sizeof ui.cfg.song, "%s", ui.folder.items[ui.sel].name);
    }
    ui.changed = ui.field | 1;
}

static void open_key(const char* key) {
    int game;
    char file[512];

    lib_close(&ui.folder);
    if (sscanf(key, "game:%d:%511[^\n]", &game, file) == 2) {
        lib_open_game(&ui.folder, game, file);
        if (ui.folder.count > 1) {
            return;
        }
        lib_close(&ui.folder);
    } else if (key[0] != 0 && strncmp(key, "game:", 5) != 0) {
        lib_open_dir(&ui.folder, key);
        if (ui.folder.count > 1) {
            return;
        }
        lib_close(&ui.folder);
    }
    lib_open_top(&ui.folder);
}

static void enter(const li_item* it) {
    char name[100];

    if (it->kind == LI_DIR) {
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", it->file);
        lib_close(&ui.folder);
        lib_open_dir(&ui.folder, dir);
    } else if (it->kind == LI_GAME) {
        char file[1024];
        int game = it->game;
        snprintf(file, sizeof file, "%s", it->file);
        lib_close(&ui.folder);
        lib_open_game(&ui.folder, game, file);
    } else {
        return;
    }
    snprintf(name, sizeof name, "..");
    ui.top = 0;
    select_name(name);
    move_selection(1);
    remember();
}

// Up a folder: the one it was in, selected
static void back_up(void) {
    char name[100], dir[512];
    int game = ui.folder.game;

    if (game < 0 && strcmp(ui.folder.dir, ".") == 0) {
        return;
    }
    if (game >= 0) {
        snprintf(name, sizeof name, "%s", lib_game_name(game));
        lib_close(&ui.folder);
        lib_open_top(&ui.folder);
    } else {
        char* slash;
        snprintf(dir, sizeof dir, "%s", ui.folder.dir);
        slash = strrchr(dir, '/');
        snprintf(name, sizeof name, "%s", slash ? slash + 1 : dir);
        lib_close(&ui.folder);
        if (slash != NULL) {
            *slash = 0;
            lib_open_dir(&ui.folder, dir);
        } else {
            lib_open_top(&ui.folder);
        }
    }
    ui.top = 0;
    select_name(name);
    remember();
}

static void copy_folder(li_folder* to, const li_folder* from) {
    lib_close(to);
    *to = *from;
    to->items = malloc((size_t)(from->count ? from->count : 1) * sizeof *to->items);
    if (to->items == NULL) {
        to->count = 0;
        return;
    }
    memcpy(to->items, from->items, (size_t)from->count * sizeof *to->items);
}

// A song's name without its file's extension
static void song_title(const li_item* it, char* out, int size) {
    const char* dot = it->size == 0 ? strrchr(it->name, '.') : NULL;

    snprintf(out, (size_t)size, "%.*s", dot ? (int)(dot - it->name) : (int)strlen(it->name), it->name);
}

static void shuffle_order(void) {
    int i, n = 0;

    free(ui.order);
    ui.order = malloc((size_t)(ui.playing.count ? ui.playing.count : 1) * sizeof *ui.order);
    ui.order_count = 0;
    if (ui.order == NULL) {
        return;
    }
    for (i = 0; i < ui.playing.count; i++) {
        if (ui.playing.items[i].kind == LI_SONG && i != ui.playing_index) {
            ui.order[n++] = i;
        }
    }
    for (i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1), t = ui.order[i];
        ui.order[i] = ui.order[j];
        ui.order[j] = t;
    }
    // the song playing first, the others after it
    if (ui.playing_index >= 0) {
        memmove(ui.order + 1, ui.order, (size_t)n * sizeof *ui.order);
        ui.order[0] = ui.playing_index;
        n++;
    }
    ui.order_count = n;
}

// Plays item i of the playing folder; 0 if it cannot be read
static int start_song(int i) {
    li_item* it = &ui.playing.items[i];
    mf_song* song = malloc(sizeof *song);
    char err[80];

    ui.playing_index = i;
    song_title(it, ui.title, sizeof ui.title);
    lib_where(&ui.playing, it, ui.where, sizeof ui.where);
    ui.muted = ui.solo = 0;
    pl_mute(0, 0);
    ui.error[0] = 0;
    ui.text_lines = 0;
    if (song == NULL || !lib_load(it, song, err, sizeof err) || song->notes == 0) {
        if (song != NULL && song->notes == 0 && song->count > 0) {
            mf_free(song);
            snprintf(err, sizeof err, "has no notes");
        }
        free(song);
        snprintf(ui.error, sizeof ui.error, "%s", strcmp(err, "is not a MIDI file") == 0 ? "Not a MIDI file" : err);
        if (ui.error[0] >= 'a' && ui.error[0] <= 'z' && strncmp(ui.error, "Not", 3) != 0) {
            // "is damaged (header)" -> "The file is damaged (header)"
            char e[100];
            snprintf(e, sizeof e, "The file %s", ui.error);
            snprintf(ui.error, sizeof ui.error, "%s", e);
        }
        it->length_state = LEN_BAD;
        ui.error_field = ui.field | 1;
        ui.failures++;
        pl_play(NULL, 0);
        return 0;
    }
    memcpy(ui.text, song->text, sizeof ui.text);
    ui.text_lines = song->text_lines;
    it->length = song->length;
    it->length_state = LEN_KNOWN;
    ui.failures = 0;
    pl_play(song, 0);
    return 1;
}

// Plays the selected song, from the folder shown
static void play_selected(void) {
    copy_folder(&ui.playing, &ui.folder);
    ui.playing_index = ui.sel;
    if (ui.cfg.shuffle) {
        shuffle_order();
    }
    start_song(ui.sel);
}

// The song after (d 1) or before (d -1) the one playing; -1 at the folder's end
static int song_after(int i, int d) {
    int k;

    if (ui.cfg.shuffle && ui.order_count > 0) {
        for (k = 0; k < ui.order_count && ui.order[k] != i; k++) {
        }
        k += d;
        return k >= 0 && k < ui.order_count ? ui.order[k] : -1;
    }
    for (i += d; i >= 0 && i < ui.playing.count; i += d) {
        if (ui.playing.items[i].kind == LI_SONG) {
            return i;
        }
    }
    return -1;
}

static int first_song(int d) {
    int i;

    if (ui.cfg.shuffle && ui.order_count > 0) {
        return ui.order[d > 0 ? 0 : ui.order_count - 1];
    }
    for (i = d > 0 ? 0 : ui.playing.count - 1; i >= 0 && i < ui.playing.count; i += d) {
        if (ui.playing.items[i].kind == LI_SONG) {
            return i;
        }
    }
    return -1;
}

// The next song plays (d 1, or the one before: -1). `wrap` goes round the
// folder; without, the end of it stops the music, the song staying at 0:00.
static void skip(int d, int wrap) {
    int n;

    if (ui.playing_index < 0) {
        return;
    }
    n = song_after(ui.playing_index, d);
    if (n < 0 && !wrap) {
        if (!ui.error[0]) {
            pl_seek(0);
            pl_pause(1);
        }
        return;
    }
    if (n < 0) {
        n = first_song(d);
    }
    if (n >= 0) {
        start_song(n);
    }
}

static int songs_in(const li_folder* f) {
    int i, n = 0;

    for (i = 0; i < f->count; i++) {
        n += f->items[i].kind == LI_SONG;
    }
    return n;
}

static void after_song(void) {
    if (ui.cfg.repeat == REPEAT_ONE && !ui.error[0]) {
        pl_seek(0);
    } else if (!ui.error[0] || ui.failures < songs_in(&ui.playing)) {
        // not round and round a folder of songs that cannot be read
        skip(1, ui.cfg.repeat == REPEAT_ALL);
    }
}

static void go_to_playing(void) {
    if (ui.playing_index < 0) {
        return;
    }
    if (!same_folder(&ui.folder, &ui.playing)) {
        char key[600];
        folder_key(&ui.playing, key, sizeof key);
        open_key(key);
    }
    select_item(ui.playing_index);
    remember();
}

static int playing_here(int i) {
    return ui.playing_index == i && same_folder(&ui.folder, &ui.playing);
}

// --- the synthesizer's font ----------------------------------------------

// The font the synthesizer plays or loads (MH_MidiSoundFont()): its presets
// for the instruments' names. A font the player chose that fails gives the
// choice back to the OSD's slot.
static void check_font(void) {
    char path[FONT_NAME + 300];

    ui.font_state = MH_MidiSoundFont(path, sizeof path);
    if (ui.font_state == MH_SF_FAILED && ui.cfg.font[0] != 0) {
        snprintf(ui.message, sizeof ui.message, "%.60s could not be loaded", ui.cfg.font);
        ui.cfg.font[0] = 0;
        ui.changed = ui.field | 1;
        MH_MidiLoadSoundFont(NULL);
    }
    // while one loads, the last one's names stay
    if (ui.font_state != MH_SF_LOADED || strcmp(path, ui.font_path) == 0) {
        return;
    }
    snprintf(ui.font_path, sizeof ui.font_path, "%s", path);
    snprintf(ui.font_file, sizeof ui.font_file, "%s", strrchr(path, '/') != NULL ? strrchr(path, '/') + 1 : path);
    free(ui.presets);
    ui.presets = NULL;
    ui.preset_count = fonts_presets(path, &ui.presets);
}

// The music goes to the MT32-pi (the player's choice, and one is connected)
static int on_pi(void) {
    return ui.cfg.dest == DEST_PI && MH_MidiPiConnected();
}

static void font_title(const char* file, char* out, int size) {
    size_t len = strlen(file);

    snprintf(out, (size_t)size, "%.*s", (int)(len > 4 && strcasecmp(file + len - 4, ".sf2") == 0 ? len - 4 : len), file);
}

static void scan_fonts(void) {
    free(ui.fonts);
    ui.font_count = fonts_scan(ui.font_dir, &ui.fonts);
    {
        char files[MH_SF_SLOTS][MH_SF_NAME];
        int own[MH_SF_SLOTS], k;

        MH_SoundFontSlots(files, own);
        for (k = 0; k < FONT_SLOTS; k++) {
            snprintf(ui.slots[k], sizeof ui.slots[k], "%s", own[k] ? files[k] : "");
        }
    }
    if (ui.font_sel >= ui.font_count) {
        ui.font_sel = ui.font_count > 0 ? ui.font_count - 1 : 0;
    }
}

// --- drawing --------------------------------------------------------------

static void hot(int x, int y, int w, int h, int action, int index) {
    if (ui.hots < HOTSPOTS) {
        hotspot* s = &ui.hot[ui.hots++];
        s->x = x;
        s->y = y;
        s->w = w;
        s->h = h;
        s->action = action;
        s->index = index;
    }
}

static void clock_text(uint32_t frames, char* out, int size) {
    uint32_t s = frames / MH_AUDIO_RATE;

    snprintf(out, (size_t)size, "%u:%02u", s / 60, s % 60);
}

static void draw_tabs(uint8_t* fb) {
    static const char* const names[2] = { "Player", "SoundFonts" };
    int x = RIGHT - (int)strlen("Player   SoundFonts") * GFX_CW, i;

    gfx_text(fb, MARGIN, 34, "MIDI Player", C_INK, 0);
    for (i = 0; i < 2; i++) {
        int w = (int)strlen(names[i]) * GFX_CW;
        if (i == ui.page) {
            gfx_round(fb, x - 3, 32, w + 6, GFX_PITCH, C_SEL, C_SEL);
        }
        gfx_text(fb, x, 34, names[i], i == ui.page ? C_INK : C_DIM, 0);
        hot(x - 3, 30, w + 6, GFX_PITCH + 4, i == 0 ? ACT_TAB_PLAYER : ACT_TAB_FONTS, 0);
        x += (int)(strlen(names[i]) + 3) * GFX_CW;
    }
    gfx_fill(fb, MARGIN, 54, GFX_W - 2 * MARGIN, 1, C_FAINT);
}

static int draw_button(uint8_t* fb, int x, int y, const char* key, const char* label, int on, int action) {
    int w = (int)(strlen(key) + strlen(label) + 1) * GFX_CW + 10;

    gfx_round(fb, x, y - 3, w, GFX_PITCH + 2, on ? C_INK : C_FAINT, on ? C_SEL : C_BG);
    gfx_text(fb, x + 5, y, key, C_HI, 0);
    gfx_text(fb, x + 5 + (int)(strlen(key) + 1) * GFX_CW, y, label, on ? C_INK : C_DIM, 0);
    hot(x, y - 3, w, GFX_PITCH + 2, action, 0);
    return x + w + 6;
}

static void draw_list(uint8_t* fb) {
    int i;

    gfx_text(fb, MARGIN, 64, ui.folder.game < 0 && strcmp(ui.folder.dir, ".") == 0 ? "" : ui.folder.title, C_DIM,
        LIST_W + 8);
    for (i = ui.top; i < ui.folder.count && i < ui.top + LIST_ROWS; i++) {
        const li_item* it = &ui.folder.items[i];
        int y = LIST_Y + (i - ui.top) * GFX_PITCH, playing = playing_here(i);
        char right[24] = "", name[110];

        hot(LIST_X - 4, y - 2, LIST_W + 8, GFX_PITCH, CLICK_LIST, i);
        if (it->kind == LI_HEAD) {
            int end = gfx_text(fb, LIST_X, y, it->name, C_DIM, 0);
            gfx_fill(fb, end + 6, y + 6, LIST_X + LIST_W - end - 6, 1, C_FAINT);
            continue;
        }
        if (it->kind == LI_NOTE) {
            gfx_text(fb, LIST_X + 10, y, it->name, C_DIM, LIST_W - 10);
            continue;
        }
        if (i == ui.sel) {
            gfx_fill(fb, LIST_X - 4, y - 2, LIST_W + 8, GFX_PITCH, C_SEL);
        }
        if (it->kind == LI_DIR) {
            snprintf(right, sizeof right, it->songs == 1 ? "1 song" : "%d songs", it->songs);
        } else if (it->kind == LI_GAME) {
            snprintf(right, sizeof right, "%s", it->entry);
        } else if (it->kind == LI_SONG && it->length_state == LEN_KNOWN) {
            clock_text(it->length, right, sizeof right);
        } else if (it->kind == LI_SONG && it->length_state == LEN_BAD) {
            snprintf(right, sizeof right, "-");
        }
        snprintf(name, sizeof name, it->kind == LI_DIR ? "%s/" : "%s", it->name);
        if (playing) {
            gfx_text(fb, LIST_X, y, ">", C_HI, 0);
        }
        gfx_text(fb, LIST_X + 10, y, name, playing ? C_HI : it->length_state == LEN_BAD ? C_FAINT : C_INK,
            LIST_W - 10 - (int)(strlen(right) + 1) * GFX_CW);
        if (right[0] != 0) {
            gfx_rtext(fb, LIST_X + LIST_W, y, right, playing ? C_HI : C_DIM);
        }
    }
}

static void draw_pane(uint8_t* fb, const pl_state* st) {
    int x0 = PANE_X, y, i, pitch;
    char now[16], total[16], line[200], font[FONT_NAME];

    gfx_fill(fb, 300, 64, 1, 310, C_FAINT);
    if (ui.playing_index < 0) {
        gfx_text(fb, x0, 64, "Nothing plays", C_DIM, 0);
        gfx_text(fb, x0, 80, "A on a song plays it.", C_DIM, 0);
        return;
    }
    gfx_text(fb, x0, 64, ui.title, C_HI, RIGHT - x0);
    gfx_text(fb, x0, 80, ui.where, C_DIM, RIGHT - x0);
    if (ui.error[0] != 0) {
        gfx_text(fb, x0, 104, ui.error, C_INK, RIGHT - x0);
        return;
    }
    for (i = 0; i < ui.text_lines; i++) {
        gfx_text(fb, x0, 96 + i * 14, ui.text[i], C_INK, RIGHT - x0);
    }
    y = 100 + ui.text_lines * 14;
    clock_text(st->pos, now, sizeof now);
    clock_text(st->length, total, sizeof total);
    snprintf(line, sizeof line, "%s%s", st->paused ? "II " : "", now);
    gfx_text(fb, x0, y, line, st->paused ? C_HI : C_INK, 0);
    gfx_rtext(fb, RIGHT, y, total, C_DIM);
    {
        int bx = x0 + (int)(strlen(line) + 2) * GFX_CW, bw = RIGHT - 36 - bx;
        gfx_bar(fb, bx, y + 4, bw, 5, st->length ? (float)st->pos / st->length : 0, C_INK, C_FAINT);
        hot(bx, y - 2, bw, 16, CLICK_BAR, bw);
    }
    y += 22;
    {
        char path[FONT_NAME + 300];
        const char* file;

        MH_MidiSoundFont(path, sizeof path);
        file = strrchr(path, '/') != NULL ? strrchr(path, '/') + 1 : path;
        font_title(file, font, sizeof font);
    }
    if (on_pi()) {
        snprintf(line, sizeof line, "On: MT32-pi");
    } else if (st->silent) {
        snprintf(line, sizeof line, "Nothing can play: no SoundFont and no MT32-pi");
    } else if (st->waiting || ui.font_state == MH_SF_LOADING) {
        snprintf(line, sizeof line, "Loading %s...", font);
    } else {
        snprintf(line, sizeof line, "On: Synthesizer  -  %s", font[0] ? font : "no SoundFont");
    }
    gfx_text(fb, x0, y, line, C_DIM, RIGHT - x0);
    if (MH_MidiPiConnected()) {
        // P, or a click on the line, switches
        gfx_rtext(fb, RIGHT, y, on_pi() ? "P: synthesizer" : "P: MT32-pi", C_FAINT);
        hot(x0, y - 2, RIGHT - x0, 16, ACT_DEST, 0);
    }
    y += 18;
    snprintf(line, sizeof line, "%s  -  %s", ui.cfg.shuffle ? "Shuffle" : "Folder in order",
        ui.cfg.repeat == REPEAT_OFF ? "Repeat off" : ui.cfg.repeat == REPEAT_ONE ? "Repeat one" : "Repeat all");
    gfx_text(fb, x0, y, line, C_DIM, 0);
    y += 20;
    // the 16 channels, as close as they fit above the buttons
    pitch = (BUTTONS_Y - 16 - y) / 15;
    if (pitch > 13) {
        pitch = 13;
    }
    for (i = 0; i < PL_CHANNELS; i++) {
        int yy = y + i * pitch, m = (ui.muted >> i) & 1, s = (ui.solo >> i) & 1, used = (st->used >> i) & 1;
        const char* name = NULL;

        snprintf(line, sizeof line, "%2d", i + 1);
        gfx_text(fb, x0, yy, line, C_DIM, 0);
        if (m || s) {
            gfx_fill(fb, x0 + 16, yy - 1, 9, 13, s ? C_HI : C_FAINT);
            gfx_text(fb, x0 + 18, yy, s ? "S" : "M", s ? C_SEL : C_INK, 0);
        }
        if (used) {
            int bank = st->drum[i] ? 128 + st->bank[i] : st->bank[i];
            // the font's names on the synthesizer, General MIDI's on the Pi
            name = on_pi() ? NULL : fonts_preset_name(ui.presets, ui.preset_count, bank, st->program[i], st->drum[i]);
            if (name == NULL) {
                name = gm_instrument(st->program[i], st->drum[i]);
            }
        }
        gfx_text(fb, x0 + 30, yy, name ? name : "-", m || !name ? C_FAINT : C_INK, RIGHT - x0 - 30 - 70);
        gfx_bar(fb, RIGHT - 64, yy + 3, 64, 6, m || st->paused ? 0 : st->level[i], used && !m ? C_INK : C_FAINT,
            C_FAINT);
        hot(x0, yy - 1, RIGHT - x0, pitch, CLICK_CHANNEL, i);
    }
}

static void draw_player(uint8_t* fb, const pl_state* st) {
    const li_item* it = ui.sel >= 0 && ui.sel < ui.folder.count ? &ui.folder.items[ui.sel] : NULL;
    const char* a = "play";
    int x;

    draw_list(fb);
    draw_pane(fb, st);
    if (it != NULL && (it->kind == LI_DIR || it->kind == LI_GAME)) {
        a = "open";
    } else if (it != NULL && it->kind == LI_UP) {
        a = "back";
    } else if (it != NULL && playing_here(ui.sel) && !st->paused && !st->ended && !ui.error[0]) {
        a = "pause";
    }
    x = draw_button(fb, MARGIN, BUTTONS_Y, "A", a, 0, ACT_OK);
    x = draw_button(fb, x, BUTTONS_Y, "B", "back", 0, ACT_BACK);
    x = draw_button(fb, x, BUTTONS_Y, "L", "prev", 0, ACT_PREV);
    draw_button(fb, x, BUTTONS_Y, "R", "next", 0, ACT_NEXT);
    x = draw_button(fb, MARGIN, BUTTONS_Y + 22, "X",
        ui.cfg.repeat == REPEAT_OFF ? "repeat off" : ui.cfg.repeat == REPEAT_ONE ? "repeat one" : "repeat all",
        ui.cfg.repeat != REPEAT_OFF, ACT_REPEAT);
    x = draw_button(fb, x, BUTTONS_Y + 22, "Y", "shuffle", ui.cfg.shuffle, ACT_SHUFFLE);
    draw_button(fb, x, BUTTONS_Y + 22, "Start", "now playing", 0, ACT_GOTO);
}

// "games/dnk/SoundFonts/" as the player sees it
static const char* shown_dir(const char* dir) {
    if (strncmp(dir, "/media/fat/", 11) == 0) {
        return dir + 11;
    }
    if (strncmp(dir, "/media/", 7) == 0) {
        return dir + 7;
    }
    return dir;
}

static void draw_fonts(uint8_t* fb, const pl_state* st) {
    int x0 = PANE_X, i, x;
    char line[FONT_NAME + 64], name[FONT_NAME];

    gfx_text(fb, MARGIN, 64, ui.putting ? "Put it in which slot?" : "OSD slots", ui.putting ? C_HI : C_DIM, 0);
    for (i = 0; i < FONT_SLOTS; i++) {
        int y = 84 + i * GFX_PITCH;
        if (ui.focus == FOCUS_SLOTS && i == ui.slot_sel) {
            gfx_fill(fb, MARGIN - 4, y - 2, LIST_W + 12, GFX_PITCH, C_SEL);
        }
        snprintf(line, sizeof line, i == 0 ? "Default" : "  %d", i);
        gfx_text(fb, MARGIN, y, line, C_DIM, 0);
        font_title(ui.slots[i], name, sizeof name);
        gfx_text(fb, MARGIN + 54, y, ui.slots[i][0] ? name : "(empty)", ui.slots[i][0] ? C_INK : C_FAINT, 196);
        hot(MARGIN - 4, y - 2, LIST_W + 12, GFX_PITCH, CLICK_SLOT, i);
    }
    gfx_text(fb, MARGIN, 230, "Every core's SoundFont line shows", C_DIM, 0);
    gfx_text(fb, MARGIN, 246, "these; stored in", C_DIM, 0);
    snprintf(line, sizeof line, "%s/slots.txt", shown_dir(ui.font_dir));
    gfx_text(fb, MARGIN, 262, line, C_DIM, LIST_W + 8);
    if (ui.message[0] != 0) {
        gfx_text(fb, MARGIN, 294, ui.message, C_HI, LIST_W + 8);
    }

    gfx_fill(fb, 300, 64, 1, 310, C_FAINT);
    snprintf(line, sizeof line, "In %s/", shown_dir(ui.font_dir));
    gfx_text(fb, x0, 64, line, C_DIM, RIGHT - x0 - 30);
    gfx_rtext(fb, RIGHT, 64, "Slot", C_DIM);
    if (ui.font_count == 0) {
        gfx_text(fb, x0, 84, "Copy GeneralUser-GS.sf2 to", C_INK, 0);
        snprintf(line, sizeof line, "%s/", shown_dir(ui.font_dir));
        gfx_text(fb, x0, 100, line, C_INK, RIGHT - x0);
    }
    if (ui.font_sel < ui.font_top) {
        ui.font_top = ui.font_sel;
    }
    if (ui.font_sel >= ui.font_top + FONT_ROWS) {
        ui.font_top = ui.font_sel - FONT_ROWS + 1;
    }
    for (i = ui.font_top; i < ui.font_count && i < ui.font_top + FONT_ROWS; i++) {
        const font_info* f = &ui.fonts[i];
        int y = 84 + (i - ui.font_top) * GFX_PITCH, k, n = 0, bad = f->state != FONT_OK;
        char where[64] = "";

        for (k = 0; k < FONT_SLOTS && !bad; k++) {
            if (strcmp(ui.slots[k], f->file) == 0) {
                n += snprintf(where + n, sizeof where - (size_t)n, n ? ", %s" : "%s", k == 0 ? "Default" : "");
                if (k > 0) {
                    n += snprintf(where + n, sizeof where - (size_t)n, "%d", k);
                }
            }
        }
        if (bad) {
            snprintf(where, sizeof where, f->state == FONT_TOO_BIG ? "too big" : "damaged");
        }
        if (i == ui.font_sel && (ui.focus == FOCUS_FONTS || ui.putting)) {
            gfx_fill(fb, x0 - 4, y - 2, RIGHT - x0 + 8, GFX_PITCH, ui.putting ? C_FAINT : C_SEL);
            gfx_text(fb, x0 - 2, y, ">", C_HI, 0);
        }
        font_title(f->file, name, sizeof name);
        gfx_text(fb, x0 + 10, y, name, bad ? C_FAINT : strcmp(f->file, ui.font_file) == 0 ? C_HI : C_INK,
            RIGHT - x0 - 10 - (int)(strlen(where) + 1) * GFX_CW);
        gfx_rtext(fb, RIGHT, y, where, bad ? C_FAINT : C_DIM);
        hot(x0 - 4, y - 2, RIGHT - x0 + 8, GFX_PITCH, CLICK_FONT, i);
    }
    if (ui.font_sel < ui.font_count) {
        const font_info* f = &ui.fonts[ui.font_sel];
        gfx_text(fb, x0, 200, f->file, C_INK, RIGHT - x0);
        if (f->state == FONT_OK) {
            snprintf(line, sizeof line, "%u MB of samples, %d presets", (f->sample_bytes + (1u << 20) - 1) >> 20,
                f->presets);
        } else if (f->state == FONT_TOO_BIG) {
            snprintf(line, sizeof line, "%u MB of samples: more than the 64 MB", (f->sample_bytes + (1u << 20) - 1) >> 20);
        } else {
            snprintf(line, sizeof line, "Damaged, or no SoundFont 2");
        }
        gfx_text(fb, x0, 216, line, C_DIM, RIGHT - x0);
    }
    if (ui.playing_index >= 0 && !ui.error[0]) {
        char now[16];
        font_title(ui.font_file, name, sizeof name);
        if (on_pi()) {
            snprintf(line, sizeof line, "%s on the MT32-pi:", st->paused || st->ended ? "Paused" : "Playing");
        } else {
            snprintf(line, sizeof line, "%s with %s:", st->paused || st->ended ? "Paused" : "Playing",
                name[0] ? name : "...");
        }
        gfx_text(fb, x0, 248, line, C_DIM, RIGHT - x0);
        clock_text(st->pos, now, sizeof now);
        snprintf(line, sizeof line, "%s   %s", ui.title, now);
        gfx_text(fb, x0, 264, line, C_HI, RIGHT - x0);
        gfx_bar(fb, x0, 284, RIGHT - x0, 5, st->length ? (float)st->pos / st->length : 0, C_INK, C_FAINT);
    }

    if (ui.focus == FOCUS_FONTS) {
        x = draw_button(fb, MARGIN, BUTTONS_Y, "A", music_plays(st) ? "play with font" : "put in a slot", 0, ACT_OK);
        draw_button(fb, x, BUTTONS_Y, "B", "back", 0, ACT_BACK);
    } else if (ui.putting) {
        x = draw_button(fb, MARGIN, BUTTONS_Y, "A", "put it here", 0, ACT_OK);
        draw_button(fb, x, BUTTONS_Y, "B", "cancel", 0, ACT_BACK);
    } else {
        x = draw_button(fb, MARGIN, BUTTONS_Y, "A", "put font here", 0, ACT_OK);
        x = draw_button(fb, x, BUTTONS_Y, "X", "clear slot", 0, ACT_CLEAR);
        draw_button(fb, x, BUTTONS_Y, "B", "back", 0, ACT_BACK);
    }
    x = draw_button(fb, MARGIN, BUTTONS_Y + 22, "Select", "Player", 0, ACT_TAB_PLAYER);
    draw_button(fb, x, BUTTONS_Y + 22, "Start", "now playing", 0, ACT_GOTO);
}

static void draw(uint8_t* fb, const pl_state* st) {
    gfx_clear(fb);
    ui.hots = 0;
    draw_tabs(fb);
    if (ui.page == PAGE_PLAYER) {
        draw_player(fb, st);
    } else {
        draw_fonts(fb, st);
    }
    if (ui.mouse_moved != 0 && since(ui.mouse_moved) < 180) {
        gfx_cursor(fb, ui.mouse_x, ui.mouse_y);
    }
}

// --- actions --------------------------------------------------------------

static void toggle_pause(const pl_state* st) {
    if (ui.playing_index < 0 || ui.error[0]) {
        return;
    }
    if (st->ended) {
        pl_seek(0);
        pl_pause(0);
        return;
    }
    pl_pause(!st->paused);
}

static void set_slot(int slot, const char* file) {
    snprintf(ui.slots[slot], sizeof ui.slots[slot], "%s", file);
    if (MH_SetSoundFontSlot(slot, file)) {
        snprintf(ui.message, sizeof ui.message, file[0] ? "Saved: the next game uses it" : "Slot cleared");
    } else {
        snprintf(ui.message, sizeof ui.message, "slots.txt could not be written");
    }
    scan_fonts();
}

static void act_player(int action, const pl_state* st) {
    const li_item* it = ui.sel >= 0 && ui.sel < ui.folder.count ? &ui.folder.items[ui.sel] : NULL;

    switch (action) {
    case ACT_UP: move_selection(-1); break;
    case ACT_DOWN: move_selection(1); break;
    case ACT_PAGE_UP: move_selection(-(LIST_ROWS - 1)); break;
    case ACT_PAGE_DOWN: move_selection(LIST_ROWS - 1); break;
    case ACT_HOME: move_selection(-ui.folder.count); break;
    case ACT_END: move_selection(ui.folder.count); break;
    case ACT_OK:
        if (it == NULL) {
            break;
        }
        if (it->kind == LI_UP) {
            back_up();
        } else if (it->kind == LI_DIR || it->kind == LI_GAME) {
            enter(it);
        } else if (it->kind == LI_SONG) {
            if (playing_here(ui.sel) && !ui.error[0]) {
                toggle_pause(st);
            } else {
                play_selected();
            }
        }
        break;
    case ACT_BACK: back_up(); break;
    case ACT_SEEK_BACK:
    case ACT_SEEK_FORWARD:
        if (playing_here(ui.sel) && !ui.error[0]) {
            int64_t to = (int64_t)st->pos + (action == ACT_SEEK_BACK ? -5 : 5) * MH_AUDIO_RATE;
            pl_seek(to < 0 ? 0 : (uint32_t)to);
        }
        break;
    default:
        break;
    }
    remember();
}

// The font plays now, the music going on with it (docs/midiplayer.md: the
// preview), on the synthesizer; remembered. 0 without a synthesizer.
static int play_font(const char* file) {
    if (!MH_MidiLoadSoundFont(file)) {
        return 0;
    }
    snprintf(ui.cfg.font, sizeof ui.cfg.font, "%s", file);
    if (ui.cfg.dest == DEST_PI) {
        ui.cfg.dest = DEST_SYNTH;
        MH_MidiDest(MH_MIDI_SYNTH);
    }
    ui.changed = ui.field | 1;
    return 1;
}

// The synthesizer or the MT32-pi, while one is connected
static void switch_dest(void) {
    if (!MH_MidiPiConnected()) {
        return;
    }
    ui.cfg.dest = ui.cfg.dest == DEST_PI ? DEST_SYNTH : DEST_PI;
    MH_MidiDest(ui.cfg.dest == DEST_PI ? MH_MIDI_PI : MH_MIDI_SYNTH);
    ui.changed = ui.field | 1;
}

static int music_plays(const pl_state* st) {
    return st->song && !st->paused && !st->ended && !ui.error[0];
}

static void act_fonts(int action, const pl_state* st) {
    switch (action) {
    case ACT_UP:
    case ACT_DOWN: {
        int d = action == ACT_UP ? -1 : 1;
        if (ui.focus == FOCUS_SLOTS) {
            ui.slot_sel = (ui.slot_sel + d + FONT_SLOTS) % FONT_SLOTS;
        } else if (ui.font_count > 0) {
            ui.font_sel += d;
            ui.font_sel = ui.font_sel < 0 ? 0 : ui.font_sel >= ui.font_count ? ui.font_count - 1 : ui.font_sel;
        }
        break;
    }
    case ACT_LEFT:
        if (!ui.putting) {
            ui.focus = FOCUS_SLOTS;
        }
        break;
    case ACT_RIGHT:
        if (!ui.putting) {
            ui.focus = FOCUS_FONTS;
        }
        break;
    case ACT_OK:
        if (ui.putting) {
            set_slot(ui.slot_sel, ui.fonts[ui.font_sel].file);
            ui.putting = 0;
            ui.focus = FOCUS_FONTS;
        } else if (ui.focus == FOCUS_FONTS && ui.font_sel < ui.font_count && ui.fonts[ui.font_sel].state == FONT_OK) {
            if (music_plays(st)) {
                if (play_font(ui.fonts[ui.font_sel].file)) {
                    ui.message[0] = 0;
                } else {
                    snprintf(ui.message, sizeof ui.message, "This core has no synthesizer");
                }
            } else {
                ui.putting = 1;
                ui.focus = FOCUS_SLOTS;
                ui.message[0] = 0;
            }
        } else if (ui.focus == FOCUS_SLOTS && ui.font_sel < ui.font_count && ui.fonts[ui.font_sel].state == FONT_OK) {
            // a slot chosen first: the font selected on the right goes in it
            set_slot(ui.slot_sel, ui.fonts[ui.font_sel].file);
        }
        break;
    case ACT_CLEAR:
        if (ui.focus == FOCUS_SLOTS && !ui.putting) {
            set_slot(ui.slot_sel, "");
        }
        break;
    case ACT_BACK:
        if (ui.putting) {
            ui.putting = 0;
            ui.focus = FOCUS_FONTS;
        } else {
            ui.page = PAGE_PLAYER;
        }
        break;
    default:
        break;
    }
}

static void act(int action, const pl_state* st) {
    switch (action) {
    case ACT_PAUSE: toggle_pause(st); return;
    case ACT_DEST: switch_dest(); return;
    case ACT_PREV:
    case ACT_NEXT:
        if (ui.playing_index >= 0) {
            skip(action == ACT_PREV ? -1 : 1, 1);
        }
        return;
    case ACT_REPEAT:
        ui.cfg.repeat = (ui.cfg.repeat + 1) % 3;
        ui.changed = ui.field | 1;
        return;
    case ACT_SHUFFLE:
        ui.cfg.shuffle = !ui.cfg.shuffle;
        if (ui.cfg.shuffle) {
            shuffle_order();
        }
        ui.changed = ui.field | 1;
        return;
    case ACT_GOTO:
        ui.page = PAGE_PLAYER;
        go_to_playing();
        return;
    case ACT_PAGE:
    case ACT_TAB_PLAYER:
    case ACT_TAB_FONTS:
        ui.page = action == ACT_PAGE ? !ui.page : action == ACT_TAB_FONTS ? PAGE_FONTS : PAGE_PLAYER;
        ui.putting = 0;
        ui.message[0] = 0;
        if (ui.page == PAGE_FONTS) {
            scan_fonts();
        }
        return;
    default:
        break;
    }
    if (ui.page == PAGE_PLAYER) {
        act_player(action, st);
    } else {
        act_fonts(action, st);
    }
}

static void mute_channel(int c, int solo) {
    if (solo) {
        ui.solo ^= (uint16_t)(1u << c);
        ui.muted &= (uint16_t)~(1u << c);
    } else {
        ui.muted ^= (uint16_t)(1u << c);
        ui.solo &= (uint16_t)~(1u << c);
    }
    pl_mute(ui.muted, ui.solo);
}

// --- input ---------------------------------------------------------------

static int key_held(int hid) {
    int k;

    for (k = 0; k < 512; k++) {
        if (((ui.in.keys[k >> 5] >> (k & 31)) & 1) && MH_KeyToHID(k) == hid) {
            return 1;
        }
    }
    return 0;
}

static void click(int x, int y, int right, int dbl, const pl_state* st) {
    int k;

    for (k = ui.hots - 1; k >= 0; k--) {
        const hotspot* s = &ui.hot[k];
        if (x < s->x || y < s->y || x >= s->x + s->w || y >= s->y + s->h) {
            continue;
        }
        switch (s->action) {
        case CLICK_LIST:
            if (selectable(&ui.folder.items[s->index])) {
                select_item(s->index);
                remember();
                if (dbl) {
                    act(ACT_OK, st);
                }
            }
            break;
        case CLICK_CHANNEL:
            mute_channel(s->index, right);
            break;
        case CLICK_BAR:
            if (ui.playing_index >= 0 && !ui.error[0] && s->index > 0) {
                pl_seek((uint32_t)((double)(x - s->x) / s->index * st->length));
            }
            break;
        case CLICK_FONT:
            if (!ui.putting) {
                ui.focus = FOCUS_FONTS;
                ui.font_sel = s->index;
                if (dbl) {
                    act(ACT_OK, st);
                }
            }
            break;
        case CLICK_SLOT:
            ui.focus = FOCUS_SLOTS;
            ui.slot_sel = s->index;
            if (ui.putting) {
                act(ACT_OK, st);
            }
            break;
        default:
            if (!right) {
                act(s->action, st);
            }
            break;
        }
        return;
    }
}

static void read_input(const pl_state* st) {
    static const struct {
        uint32_t bit;
        int action;
    } pad[] = {
        { MH_JOY_UP, ACT_UP }, { MH_JOY_DOWN, ACT_DOWN }, { MH_JOY_LEFT, ACT_LEFT }, { MH_JOY_RIGHT, ACT_RIGHT },
        { MH_JOY_BUTTON(B_X), ACT_REPEAT }, { MH_JOY_BUTTON(B_Y), ACT_SHUFFLE },
        { MH_JOY_BUTTON(B_L), ACT_PREV }, { MH_JOY_BUTTON(B_R), ACT_NEXT },
        { MH_JOY_BUTTON(B_SELECT), ACT_PAGE }, { MH_JOY_BUTTON(B_START), ACT_GOTO },
    };
    static const struct {
        int hid, action;
    } keys[] = {
        { K_UP, ACT_UP }, { K_DOWN, ACT_DOWN }, { K_PAGE_UP, ACT_PAGE_UP }, { K_PAGE_DOWN, ACT_PAGE_DOWN },
        { K_HOME, ACT_HOME }, { K_END, ACT_END }, { K_ENTER, ACT_OK }, { K_BACKSPACE, ACT_BACK },
        { K_ESC, ACT_BACK }, { K_SPACE, ACT_PAUSE }, { K_COMMA, ACT_PREV }, { K_PERIOD, ACT_NEXT },
        { K_R, ACT_REPEAT }, { K_S, ACT_SHUFFLE }, { K_G, ACT_GOTO }, { K_TAB, ACT_PAGE },
    };
    uint32_t joy, pressed, ok_bits = 0, back_bits = 0;
    int ok = 0, back = 0, k, shift, moved, repeat_action = ACT_NONE;
    static int last_ok, last_back;
    int32_t dx, dy;

    ui.last = ui.in;
    MH_ReadInput(&ui.in);
    if (!ui.have_last) {
        ui.last = ui.in;
        ui.have_last = 1;
    }
    joy = ui.in.joystick[0] | ui.in.joystick[1];
    pressed = joy & ~(ui.last.joystick[0] | ui.last.joystick[1]);

    // Menu OK / Menu Back: the MiSTer menu's and the core's own two. Not the
    // core's A and B themselves: on a pad without a mapping the MiSTer menu
    // confirms with the bottom button, which is the core's B, and goes back
    // with the right one, its A, so each was both OK and Back
    MH_MenuButtons(MH_OSDStatus(), &ok_bits, &back_bits);
    MH_MenuButtonsPressed(MH_OSDStatus(), ui.in.joystick[0], &ok, &back);
    ok |= (joy & (MH_JOY_BUTTON(B_MENU_OK) | ok_bits)) != 0;
    back |= (joy & (MH_JOY_BUTTON(B_MENU_BACK) | back_bits)) != 0;
    if (ok && !last_ok) {
        act(ACT_OK, st);
    }
    if (back && !last_back) {
        act(ACT_BACK, st);
    }
    last_ok = ok;
    last_back = back;
    for (k = 0; k < (int)(sizeof pad / sizeof pad[0]); k++) {
        // a button that is also Menu OK or Back does only that
        if ((pressed & pad[k].bit) && !(pad[k].bit & (ok_bits | back_bits))) {
            act(pad[k].action, st);
        }
        if ((joy & pad[k].bit) && pad[k].action <= ACT_RIGHT) {
            repeat_action = pad[k].action;
        }
    }

    // the keyboard: what went down since the last frame
    shift = key_held(K_LSHIFT) || key_held(K_RSHIFT);
    for (k = 0; k < 512; k++) {
        int down = (ui.in.keys[k >> 5] >> (k & 31)) & 1, was = (ui.keys_down[k >> 5] >> (k & 31)) & 1;
        int hid = down ? MH_KeyToHID(k) : 0, i;

        if (down && hid >= K_UP - 3 && hid <= K_UP) {
            // arrows repeat while held
            repeat_action = hid == K_UP ? ACT_UP : hid == K_DOWN ? ACT_DOWN : ACT_NONE;
        }
        if (!down || was) {
            continue;
        }
        if (ui.page == PAGE_PLAYER && (hid == K_LEFT || hid == K_RIGHT)) {
            act(hid == K_LEFT ? ACT_SEEK_BACK : ACT_SEEK_FORWARD, st);
            continue;
        }
        if (hid == K_LEFT || hid == K_RIGHT) {
            act(hid == K_LEFT ? ACT_LEFT : ACT_RIGHT, st);
            continue;
        }
        for (i = 0; i < (int)(sizeof keys / sizeof keys[0]); i++) {
            if (keys[i].hid == hid) {
                act(keys[i].action, st);
                break;
            }
        }
        if (i < (int)(sizeof keys / sizeof keys[0])) {
            continue;
        }
        if (hid >= K_1 && hid <= K_0 && ui.page == PAGE_PLAYER) {
            mute_channel(hid - K_1, shift);
        } else if (hid == K_P) {
            switch_dest();
        } else if (hid == K_X && ui.page == PAGE_FONTS) {
            act(ACT_CLEAR, st);
        }
        // every other key does nothing
    }
    memcpy(ui.keys_down, ui.in.keys, sizeof ui.keys_down);

    // a held direction or arrow repeats after 0.3 seconds, 15 a second: by
    // the time of the next step, not by a count of fields the loop might
    // step over (interlaced it sees every second field, and a slow frame
    // skips more: the steps came unevenly)
    if (repeat_action != ui.held_action) {
        ui.held_action = repeat_action;
        ui.repeat_at = ui.field + REPEAT_DELAY;
    } else if (repeat_action != ACT_NONE && since(ui.repeat_at) >= 0) {
        act(repeat_action, st);
        ui.repeat_at = ui.field + REPEAT_EVERY;
    }

    // the mouse
    dx = ui.in.mouse_x - ui.last.mouse_x;
    dy = ui.in.mouse_y - ui.last.mouse_y;
    moved = dx != 0 || dy != 0;
    if (moved) {
        ui.mouse_x += dx;
        ui.mouse_y -= dy;
        ui.mouse_x = ui.mouse_x < 0 ? 0 : ui.mouse_x >= GFX_W ? GFX_W - 1 : ui.mouse_x;
        ui.mouse_y = ui.mouse_y < 0 ? 0 : ui.mouse_y >= GFX_H ? GFX_H - 1 : ui.mouse_y;
        ui.mouse_moved = ui.field | 1;
    }
    if (ui.in.mouse_wheel != ui.last.mouse_wheel) {
        int lines = ui.in.mouse_wheel - ui.last.mouse_wheel;
        if (ui.page == PAGE_PLAYER) {
            ui.top -= 3 * lines;
            ui.top = ui.top < 0 ? 0 : ui.top > ui.folder.count - LIST_ROWS ? (ui.folder.count > LIST_ROWS ? ui.folder.count - LIST_ROWS : 0) : ui.top;
        } else {
            act_fonts(lines > 0 ? ACT_UP : ACT_DOWN, st);
        }
    }
    for (k = 0; k < 2; k++) {
        if ((ui.in.mouse_buttons >> k) & 1 & ~(ui.last.mouse_buttons >> k)) {
            int dbl = k == 0 && ui.click_field != 0 && since(ui.click_field) < 24 && ui.click_area == ui.mouse_x / 4 + ui.mouse_y / 4 * 1000;
            ui.mouse_moved = ui.field | 1;
            click(ui.mouse_x, ui.mouse_y, k == 1, dbl, st);
            if (k == 0) {
                ui.click_field = dbl ? 0 : ui.field | 1;
                ui.click_area = ui.mouse_x / 4 + ui.mouse_y / 4 * 1000;
            }
        }
    }
}

// --- the loop --------------------------------------------------------------

void ui_init(void) {
    memset(&ui, 0, sizeof ui);
    config_load(&ui.cfg);
    // the font and the synthesizer chosen last (the OSD's slot while none
    // is), before the synthesizer loads one
    if (ui.cfg.font[0] != 0) {
        MH_MidiLoadSoundFont(ui.cfg.font);
    }
    MH_MidiDest(ui.cfg.dest == DEST_PI ? MH_MIDI_PI : MH_MIDI_SYNTH);
}

void ui_run(void) {
    static uint8_t own[GFX_W * GFX_H];
    uint32_t palette[256];
    pl_state st;
    int was_ended = 0;

    ui.playing_index = -1;
    ui.mouse_x = GFX_W / 2;
    ui.mouse_y = GFX_H / 2;
    srand((unsigned)time(NULL));
    snprintf(ui.font_dir, sizeof ui.font_dir, "%s", MH_SoundFontDir());
    open_key(ui.cfg.folder);
    select_name(ui.cfg.song);
    scan_fonts();

    gfx_palette(palette);
    MH_SetFormat(MH_FORMAT_INDEX8);
    MH_SetMode(MH_MODE_640x480);
    // a frame at every field of 480i, 59.6 a second on a CRT (Danik,
    // 2026-10-28: "any cores that have 480i should become 480i/60"; host
    // version 18, hybrid/CLAUDE.md "Field rate"); MH_WaitFrame() follows it
    MH_SetFieldRate(1);
    MH_SetPalette(palette);
    ui.field = MH_FieldCounter();
    while (MH_CheckAlive()) {
        uint8_t* fb = MH_FrameBuffer();

        if (fb == NULL) {
            fb = own;
        }
        pl_state_get(&st);
        check_font();
        read_input(&st);
        pl_state_get(&st);
        if (st.ended && !was_ended && ui.playing_index >= 0) {
            after_song();
            pl_state_get(&st);
        }
        if (ui.error[0] && ui.error_field != 0 && since(ui.error_field) >= 120) {
            ui.error_field = 0;
            after_song();
        }
        was_ended = st.ended;
        // the songs' lengths, a few each frame
        lib_lengths(&ui.folder, 4);
        if (ui.changed != 0 && since(ui.changed) >= 120) {
            config_save(&ui.cfg);
            ui.changed = 0;
        }
        draw(fb, &st);
        if (fb == own) {
            MH_Present(own, GFX_W);
        } else {
            MH_PresentBuffer();
        }
        MH_WaitFrame(ui.field);
        ui.field = MH_FieldCounter();
    }
    MH_SetFieldRate(0);
    remember();
    config_save(&ui.cfg);
    lib_close(&ui.folder);
    lib_close(&ui.playing);
    free(ui.order);
    free(ui.fonts);
    free(ui.presets);
}
