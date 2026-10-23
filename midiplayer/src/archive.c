// WAD and GRP directories
#include "archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static long file_size(FILE* f) {
    long size;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

static int open_wad(FILE* f, long size, const uint8_t* head, ar_dir* dir) {
    uint32_t count = le32(head + 4), at = le32(head + 8), i;
    uint8_t* raw;

    if (count == 0 || count > 65536 || at > (uint32_t)size || (uint32_t)size - at < count * 16) {
        return 0;
    }
    raw = malloc(count * 16);
    dir->entries = calloc(count, sizeof *dir->entries);
    if (raw == NULL || dir->entries == NULL || fseek(f, (long)at, SEEK_SET) != 0
        || fread(raw, 16, count, f) != count) {
        free(raw);
        free(dir->entries);
        dir->entries = NULL;
        return 0;
    }
    for (i = 0; i < count; i++) {
        ar_entry* e = &dir->entries[dir->count];

        e->offset = le32(raw + i * 16);
        e->size = le32(raw + i * 16 + 4);
        memcpy(e->name, raw + i * 16 + 8, 8);
        e->name[8] = 0;
        if (e->size > 0 && e->offset <= (uint32_t)size && e->size <= (uint32_t)size - e->offset) {
            dir->count++;
        }
    }
    free(raw);
    return 1;
}

static int open_grp(FILE* f, long size, const uint8_t* head, ar_dir* dir) {
    uint32_t count = le32(head + 12), at, i;
    uint8_t* raw;

    if (count == 0 || count > 65536 || (uint32_t)size < 16 + count * 16) {
        return 0;
    }
    raw = malloc(count * 16);
    dir->entries = calloc(count, sizeof *dir->entries);
    if (raw == NULL || dir->entries == NULL || fread(raw, 16, count, f) != count) {
        free(raw);
        free(dir->entries);
        dir->entries = NULL;
        return 0;
    }
    // the files follow the directory, one after the other
    at = 16 + count * 16;
    for (i = 0; i < count; i++) {
        ar_entry* e = &dir->entries[dir->count];

        memcpy(e->name, raw + i * 16, 12);
        e->name[12] = 0;
        e->offset = at;
        e->size = le32(raw + i * 16 + 12);
        if (e->size > (uint32_t)size - at) {
            break;
        }
        at += e->size;
        dir->count++;
    }
    free(raw);
    return 1;
}

int ar_open(const char* path, ar_dir* dir) {
    FILE* f = fopen(path, "rb");
    uint8_t head[16];
    long size;
    int ok = 0;

    memset(dir, 0, sizeof *dir);
    if (f == NULL) {
        return 0;
    }
    size = file_size(f);
    if (size >= 16 && fread(head, 1, 16, f) == 16) {
        if (memcmp(head, "IWAD", 4) == 0 || memcmp(head, "PWAD", 4) == 0) {
            ok = open_wad(f, size, head, dir);
        } else if (memcmp(head, "KenSilverman", 12) == 0) {
            ok = open_grp(f, size, head, dir);
        }
    }
    fclose(f);
    return ok;
}

void ar_close(ar_dir* dir) {
    free(dir->entries);
    memset(dir, 0, sizeof *dir);
}

int ar_find(const ar_dir* dir, const char* name) {
    int i;

    for (i = 0; i < dir->count; i++) {
        if (strcasecmp(dir->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

uint8_t* ar_read(const char* path, uint32_t offset, uint32_t size) {
    FILE* f = fopen(path, "rb");
    uint8_t* bytes = malloc(size ? size : 1);

    if (f == NULL || bytes == NULL || fseek(f, (long)offset, SEEK_SET) != 0
        || fread(bytes, 1, size, f) != size) {
        if (f != NULL) {
            fclose(f);
        }
        free(bytes);
        return NULL;
    }
    fclose(f);
    return bytes;
}
