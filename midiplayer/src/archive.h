// The music inside the installed ports' game files: the lumps of a WAD
// (Doom's) and the files of a GRP (Build's: Shadow Warrior), read in place.
#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>

typedef struct {
    char name[13];
    uint32_t offset, size;
} ar_entry;

typedef struct {
    ar_entry* entries;
    int count;
} ar_dir;

// The directory of a WAD (IWAD or PWAD) or a GRP; 0 when it is neither
int ar_open(const char* path, ar_dir* dir);
void ar_close(ar_dir* dir);
// The entry of that name (case ignored), -1 when there is none
int ar_find(const ar_dir* dir, const char* name);
// An entry's bytes (malloc'ed), NULL when they cannot be read
uint8_t* ar_read(const char* path, uint32_t offset, uint32_t size);

#endif
