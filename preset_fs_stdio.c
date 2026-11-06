#include "preset_fs_stdio.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#define MAKE_DIR(p) mkdir((p), 0777)
#endif

static bool stdio_mount(void *ctx) {
    StdioFsCtx *c = ctx;
    return c->mount == NULL || c->mount();
}

static PfsResult stdio_read(void *ctx, const char *path, char *buf, size_t cap, size_t *len) {
    FILE *f;
    size_t n;
    int extra;

    (void)ctx;
    f = fopen(path, "rb");
    if (f == NULL) return errno == ENOENT ? PFS_NOT_FOUND : PFS_IO;
    n = fread(buf, 1, cap, f);
    if (ferror(f)) {
        fclose(f);
        return PFS_IO;
    }
    extra = fgetc(f); /* probe one byte past the cap */
    fclose(f);
    if (extra != EOF) return PFS_TOO_BIG;
    *len = n;
    return PFS_OK;
}

static PfsResult stdio_write(void *ctx, const char *path, const char *data, size_t len) {
    FILE *f;
    int flushed;
    int closed;
    size_t n;

    (void)ctx;
    f = fopen(path, "wb");
    if (f == NULL) return PFS_IO;
    n = fwrite(data, 1, len, f);
    flushed = fflush(f);
    closed = fclose(f); /* cached FAT writes can fail only here */
    return (n == len && flushed == 0 && closed == 0) ? PFS_OK : PFS_IO;
}

static PfsResult stdio_rename(void *ctx, const char *from, const char *to) {
    (void)ctx;
    return rename(from, to) == 0 ? PFS_OK : PFS_IO;
}

static PfsResult stdio_remove(void *ctx, const char *path) {
    (void)ctx;
    return remove(path) == 0 ? PFS_OK : PFS_IO;
}

static PfsResult stdio_mkdir(void *ctx, const char *path) {
    (void)ctx;
    return (MAKE_DIR(path) == 0 || errno == EEXIST) ? PFS_OK : PFS_IO;
}

void preset_fs_stdio_init(PresetFs *fs, StdioFsCtx *ctx, const char *dir) {
    fs->ctx = ctx;
    fs->dir = dir;
    fs->mount = stdio_mount;
    fs->read = stdio_read;
    fs->write = stdio_write;
    fs->rename = stdio_rename;
    fs->remove = stdio_remove;
    fs->mkdir = stdio_mkdir;
}
