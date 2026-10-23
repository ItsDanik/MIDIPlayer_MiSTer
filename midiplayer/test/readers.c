// The readers on the PC: midifile, archive, fonts.
// usage: readers <file.mid|.wad|.grp|.sf2|dir of .sf2> ...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "archive.h"
#include "fonts.h"
#include "midifile.h"

static void song(const char* what, mf_song* s) {
    int i;
    printf("%-24s %3d:%02d  %6d events %5d notes", what, s->length / 44100 / 60, s->length / 44100 % 60, s->count, s->notes);
    for (i = 0; i < s->text_lines; i++)
        printf("  [%s]", s->text[i]);
    printf("\n");
}

int main(int argc, char** argv) {
    char err[100];
    int a, i;

    for (a = 1; a < argc; a++) {
        const char* p = argv[a];
        size_t n = strlen(p);
        ar_dir d;
        if (n > 4 && strcasecmp(p + n - 4, ".sf2") == 0) {
            font_info fi;
            font_preset* pr;
            int c = fonts_presets(p, &pr);
            fonts_check(p, &fi);
            printf("%s: state %d, %u bytes, %d presets (%d read)", fi.file, fi.state, fi.sample_bytes, fi.presets, c);
            if (c)
                printf(", 0:0 %s, drums 128:0 %s, 0:30 %s", fonts_preset_name(pr, c, 0, 0, 0), fonts_preset_name(pr, c, 128, 0, 1), fonts_preset_name(pr, c, 0, 30, 0));
            printf("\n");
            free(pr);
        } else if (ar_open(p, &d)) {
            int songs = 0;
            for (i = 0; i < d.count; i++) {
                uint8_t* b = ar_read(p, d.entries[i].offset, d.entries[i].size);
                mf_song s;
                if (b && mf_is_music(b, d.entries[i].size)) {
                    if (mf_parse(b, d.entries[i].size, &s, err, sizeof err)) {
                        if (songs++ < 6) song(d.entries[i].name, &s);
                        mf_free(&s);
                    } else
                        printf("%s %s\n", d.entries[i].name, err);
                } else
                    free(b);
            }
            printf("%s: %d entries, %d songs\n", p, d.count, songs);
            ar_close(&d);
        } else {
            mf_song s;
            if (mf_load(p, &s, err, sizeof err)) {
                song(strrchr(p, '/') ? strrchr(p, '/') + 1 : p, &s);
                mf_free(&s);
            } else
                printf("%s %s\n", p, err);
        }
    }
    if (argc > 1 && getenv("SLOTS")) {
        char slots[FONT_SLOTS][FONT_NAME];
        fonts_slots(getenv("SLOTS"), slots);
        for (i = 0; i < FONT_SLOTS; i++)
            printf("slot %d: %s\n", i, slots[i]);
    }
    return 0;
}
