// SoundFont files and the OSD's slots
#include "fonts.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// A chunk's header at `at`, inside [at, end): its id and size; 0 at the end
static int chunk(FILE* f, long at, long end, char id[5], uint32_t* size) {
    uint8_t h[8];

    if (end - at < 8 || fseek(f, at, SEEK_SET) != 0 || fread(h, 1, 8, f) != 8) {
        return 0;
    }
    memcpy(id, h, 4);
    id[4] = 0;
    *size = le32(h + 4);
    return *size <= (uint32_t)(end - at - 8);
}

// The places of the parts of a SoundFont 2: smpl, and pdta's phdr; with
// pdta's nine parts each a whole number of records (a preset and the
// last record in phdr, at least the last in the others)
typedef struct {
    uint32_t smpl_size;
    long phdr;
    uint32_t phdr_size;
} layout;

static int find_layout(FILE* f, layout* l) {
    static const char* const parts[9] = { "phdr", "pbag", "pmod", "pgen", "inst", "ibag", "imod", "igen", "shdr" };
    static const uint32_t records[9] = { 38, 4, 10, 4, 22, 4, 10, 4, 46 };
    uint8_t h[12];
    char id[5], list[5];
    uint32_t size, riff, sub;
    long at, end;
    int found = 0, k;

    memset(l, 0, sizeof *l);
    if (fseek(f, 0, SEEK_SET) != 0 || fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) != 0
        || memcmp(h + 8, "sfbk", 4) != 0) {
        return 0;
    }
    riff = le32(h + 4);
    end = 8 + (long)riff;
    for (at = 12; chunk(f, at, end, id, &size); at += 8 + (long)((size + 1) & ~1u)) {
        long sat, send = at + 8 + (long)size;

        if (strcmp(id, "LIST") != 0 || size < 4 || fread(list, 1, 4, f) != 4) {
            continue;
        }
        list[4] = 0;
        for (sat = at + 12; chunk(f, sat, send, id, &sub); sat += 8 + (long)((sub + 1) & ~1u)) {
            if (strcmp(list, "sdta") == 0 && strcmp(id, "smpl") == 0) {
                l->smpl_size = sub;
                found |= 1 << 9;
            }
            if (strcmp(list, "pdta") != 0) {
                continue;
            }
            for (k = 0; k < 9; k++) {
                if (strcmp(id, parts[k]) == 0 && sub >= (k == 0 ? 2u : 1u) * records[k] && sub % records[k] == 0) {
                    found |= 1 << k;
                    if (k == 0) {
                        l->phdr = sat + 8;
                        l->phdr_size = sub;
                    }
                }
            }
        }
    }
    return found == 0x3FF;
}

int fonts_check(const char* path, font_info* info) {
    FILE* f = fopen(path, "rb");
    layout l;
    const char* slash = strrchr(path, '/');

    memset(info, 0, sizeof *info);
    snprintf(info->file, sizeof info->file, "%s", slash ? slash + 1 : path);
    info->state = FONT_DAMAGED;
    if (f == NULL) {
        return info->state;
    }
    if (find_layout(f, &l)) {
        info->sample_bytes = l.smpl_size;
        info->presets = (int)(l.phdr_size / 38) - 1;
        info->state = l.smpl_size > FONT_MAX_SAMPLE_BYTES ? FONT_TOO_BIG : FONT_OK;
    }
    fclose(f);
    return info->state;
}

int fonts_presets(const char* path, font_preset** list) {
    FILE* f = fopen(path, "rb");
    layout l;
    uint8_t* raw = NULL;
    int n = 0, k;

    *list = NULL;
    if (f == NULL) {
        return 0;
    }
    if (find_layout(f, &l) && fseek(f, l.phdr, SEEK_SET) == 0) {
        n = (int)(l.phdr_size / 38) - 1;
        raw = malloc(l.phdr_size);
        *list = calloc((size_t)n, sizeof **list);
        if (raw == NULL || *list == NULL || fread(raw, 1, l.phdr_size, f) != l.phdr_size) {
            free(*list);
            *list = NULL;
            n = 0;
        }
        for (k = 0; k < n; k++) {
            const uint8_t* p = raw + k * 38;
            font_preset* pr = &(*list)[k];
            int i;

            for (i = 0; i < 20 && p[i] != 0; i++) {
                pr->name[i] = p[i] >= 32 && p[i] < 127 ? (char)p[i] : '?';
            }
            while (i > 0 && pr->name[i - 1] == ' ') {
                pr->name[--i] = 0;
            }
            pr->program = (uint16_t)(p[20] | p[21] << 8);
            pr->bank = (uint16_t)(p[22] | p[23] << 8);
        }
    }
    free(raw);
    fclose(f);
    return n;
}

static const char* find_preset(const font_preset* list, int count, int bank, int program) {
    int k;

    for (k = 0; k < count; k++) {
        if (list[k].bank == bank && list[k].program == program) {
            return list[k].name;
        }
    }
    return NULL;
}

const char* fonts_preset_name(const font_preset* list, int count, int bank, int program, int drum) {
    const char* name = find_preset(list, count, bank, program);

    if (name == NULL) {
        name = find_preset(list, count, drum ? 128 : 0, program);
    }
    if (name == NULL) {
        name = find_preset(list, count, drum ? 128 : 0, 0);
    }
    return name;
}

static int name_cmp(const void* a, const void* b) {
    return strcasecmp((const char*)a, (const char*)b);
}

// The .sf2 file names of the folder, by name; the count
static int sf2_names(const char* dir, char (**names)[FONT_NAME]) {
    DIR* d = opendir(dir);
    struct dirent* e;
    int n = 0, size = 0;

    *names = NULL;
    if (d == NULL) {
        return 0;
    }
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);

        if (len > 4 && len < FONT_NAME && e->d_name[0] != '.' && strcasecmp(e->d_name + len - 4, ".sf2") == 0) {
            if (n == size) {
                char (*more)[FONT_NAME] = realloc(*names, (size_t)(size = size ? size * 2 : 16) * FONT_NAME);
                if (more == NULL) {
                    break;
                }
                *names = more;
            }
            memcpy((*names)[n++], e->d_name, len + 1);
        }
    }
    closedir(d);
    if (n > 0) {
        qsort(*names, (size_t)n, FONT_NAME, name_cmp);
    }
    return n;
}

int fonts_scan(const char* dir, font_info** list) {
    char (*names)[FONT_NAME];
    char path[FONT_NAME + 128];
    int n = sf2_names(dir, &names), k;

    *list = calloc((size_t)(n ? n : 1), sizeof **list);
    if (*list == NULL) {
        free(names);
        return 0;
    }
    for (k = 0; k < n; k++) {
        snprintf(path, sizeof path, "%s/%s", dir, names[k]);
        fonts_check(path, &(*list)[k]);
    }
    free(names);
    return n;
}
