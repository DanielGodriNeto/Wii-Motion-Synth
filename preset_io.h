/*
 * Preset save and load on top of an injectable file layer. The console build
 * plugs in a stdio adapter on the SD card; tests plug in an in-memory fake.
 */
#ifndef PRESET_IO_H
#define PRESET_IO_H

#include <stdbool.h>
#include <stddef.h>

#include "preset_store.h"

#define PRESET_SLOT_MIN 1
#define PRESET_SLOT_MAX 4

typedef enum { PFS_OK, PFS_NOT_FOUND, PFS_IO, PFS_TOO_BIG } PfsResult;

typedef struct PresetFs {
    void *ctx;
    const char *dir;
    bool (*mount)(void *ctx);
    /* Reads up to cap bytes. Returns PFS_TOO_BIG when the file is larger. */
    PfsResult (*read)(void *ctx, const char *path, char *buf, size_t cap, size_t *len);
    PfsResult (*write)(void *ctx, const char *path, const char *data, size_t len);
    PfsResult (*rename)(void *ctx, const char *from, const char *to);
    PfsResult (*remove)(void *ctx, const char *path);
    PfsResult (*mkdir)(void *ctx, const char *path);
} PresetFs;

/* Writes dir/preset<N>.cfg through a temporary file. On a failed write the
 * existing preset is untouched. */
PresetResult preset_save(const PresetFs *fs, int slot, const SynthSettings *s);

/* Reads dir/preset<N>.cfg; falls back to the temporary file when the final
 * file is missing (interrupted save). *out is written only on PRESET_OK. */
PresetResult preset_load(const PresetFs *fs, int slot, SynthSettings *out);

#endif /* PRESET_IO_H */
