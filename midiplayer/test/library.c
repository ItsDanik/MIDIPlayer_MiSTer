// The song list on the PC: run in a My songs folder, MIDIPLAYER_DNK set
#include <stdio.h>
#include "library.h"

static void show(li_folder* f) {
    int i;
    while (!lib_lengths(f, 50)) {
    }
    printf("== %s\n", f->title);
    for (i = 0; i < f->count; i++) {
        li_item* it = &f->items[i];
        char where[256] = "";
        if (it->kind == LI_SONG)
            lib_where(f, it, where, sizeof where);
        printf("  %d %-34s %s %s\n", it->kind, it->name,
               it->kind == LI_GAME ? it->entry : it->kind == LI_SONG ? (it->length_state == LEN_KNOWN ? "len" : "BAD") : "", where);
        if (it->kind == LI_SONG && it->length_state == LEN_KNOWN)
            printf("      %d:%02d\n", it->length / 44100 / 60, it->length / 44100 % 60);
    }
}

int main(void) {
    li_folder top, sub;
    int i;
    lib_open_top(&top);
    show(&top);
    for (i = 0; i < top.count; i++) {
        if (top.items[i].kind == LI_GAME) {
            lib_open_game(&sub, top.items[i].game, top.items[i].file);
            show(&sub);
            lib_close(&sub);
        } else if (top.items[i].kind == LI_DIR) {
            lib_open_dir(&sub, top.items[i].file);
            show(&sub);
            lib_close(&sub);
        }
    }
    lib_close(&top);
    return 0;
}
