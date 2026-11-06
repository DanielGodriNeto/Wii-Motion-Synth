/* PresetFs backed by stdio. Used on the console (SD card) and in host tests. */
#ifndef PRESET_FS_STDIO_H
#define PRESET_FS_STDIO_H

#include <stdbool.h>

#include "preset_io.h"

typedef struct StdioFsCtx {
    bool (*mount)(void); /* NULL means always mounted */
} StdioFsCtx;

/* ctx must outlive fs. */
void preset_fs_stdio_init(PresetFs *fs, StdioFsCtx *ctx, const char *dir);

#endif /* PRESET_FS_STDIO_H */
