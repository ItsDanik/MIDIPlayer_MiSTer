// midiplayer.cfg: "key=value" lines
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_NAME "midiplayer.cfg"

static config saved;

void config_load(config* c) {
    char line[700];
    FILE* f = fopen(FILE_NAME, "r");

    memset(c, 0, sizeof *c);
    c->repeat = REPEAT_ALL;
    while (f != NULL && fgets(line, sizeof line, f) != NULL) {
        char* eq = strchr(line, '=');
        char* v;

        line[strcspn(line, "\r\n")] = 0;
        if (eq == NULL) {
            continue;
        }
        *eq = 0;
        v = eq + 1;
        if (strcmp(line, "folder") == 0) {
            snprintf(c->folder, sizeof c->folder, "%s", v);
        } else if (strcmp(line, "song") == 0) {
            snprintf(c->song, sizeof c->song, "%s", v);
        } else if (strcmp(line, "font") == 0) {
            snprintf(c->font, sizeof c->font, "%s", v);
        } else if (strcmp(line, "shuffle") == 0) {
            c->shuffle = atoi(v) != 0;
        } else if (strcmp(line, "repeat") == 0) {
            c->repeat = atoi(v) >= REPEAT_OFF && atoi(v) <= REPEAT_ALL ? atoi(v) : REPEAT_ALL;
        } else if (strcmp(line, "dest") == 0) {
            c->dest = atoi(v) == DEST_PI ? DEST_PI : DEST_SYNTH;
        }
    }
    if (f != NULL) {
        fclose(f);
    }
    saved = *c;
}

void config_save(const config* c) {
    FILE* f;

    if (memcmp(c, &saved, sizeof *c) == 0) {
        return;
    }
    saved = *c;
    f = fopen(FILE_NAME ".new", "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "folder=%s\nsong=%s\nfont=%s\nshuffle=%d\nrepeat=%d\ndest=%d\n", c->folder, c->song, c->font,
        c->shuffle, c->repeat, c->dest);
    if (fclose(f) == 0) {
        rename(FILE_NAME ".new", FILE_NAME);
    } else {
        remove(FILE_NAME ".new");
    }
}
