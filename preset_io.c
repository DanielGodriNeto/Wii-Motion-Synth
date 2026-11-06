#include "preset_io.h"

#include <stdio.h>

#define TMP_SUFFIX ".tmp"

static bool slot_valid(int slot) {
    return slot >= PRESET_SLOT_MIN && slot <= PRESET_SLOT_MAX;
}

static bool make_path(char *buf, size_t cap, const PresetFs *fs, int slot, const char *suffix) {
    int n = snprintf(buf, cap, "%s/preset%d.cfg%s", fs->dir, slot, suffix);
    return n > 0 && (size_t)n < cap;
}

/* FAT rename fails when the target exists, so remove it and try again. */
static PresetResult replace_final(const PresetFs *fs, const char *tmp, const char *final_path) {
    if (fs->rename(fs->ctx, tmp, final_path) == PFS_OK) return PRESET_OK;
    if (fs->remove(fs->ctx, final_path) != PFS_OK) return PRESET_E_IO;
    if (fs->rename(fs->ctx, tmp, final_path) != PFS_OK) return PRESET_E_IO;
    return PRESET_OK;
}

PresetResult preset_save(const PresetFs *fs, int slot, const SynthSettings *s) {
    char final_path[PRESET_PATH_MAX];
    char tmp_path[PRESET_PATH_MAX];
    char data[PRESET_MAX_BYTES];
    size_t len;

    if (!slot_valid(slot)) return PRESET_E_BAD_SLOT;
    if (!fs->mount(fs->ctx)) return PRESET_E_NO_STORAGE;
    if (!make_path(final_path, sizeof final_path, fs, slot, "") ||
        !make_path(tmp_path, sizeof tmp_path, fs, slot, TMP_SUFFIX)) {
        return PRESET_E_IO;
    }
    len = preset_serialize(s, data, sizeof data);
    if (len == 0) return PRESET_E_IO;

    (void)fs->mkdir(fs->ctx, fs->dir); /* exists already is fine */
    if (fs->write(fs->ctx, tmp_path, data, len) != PFS_OK) {
        (void)fs->remove(fs->ctx, tmp_path);
        return PRESET_E_IO;
    }
    return replace_final(fs, tmp_path, final_path);
}

static PresetResult read_and_parse(const PresetFs *fs, const char *path, SynthSettings *out,
                                   PfsResult *fs_result) {
    char data[PRESET_MAX_BYTES];
    size_t len = 0;

    *fs_result = fs->read(fs->ctx, path, data, sizeof data, &len);
    if (*fs_result == PFS_TOO_BIG) return PRESET_E_TOO_BIG;
    if (*fs_result == PFS_NOT_FOUND) return PRESET_E_NOT_FOUND;
    if (*fs_result != PFS_OK) return PRESET_E_IO;
    return preset_parse(data, len, out);
}

PresetResult preset_load(const PresetFs *fs, int slot, SynthSettings *out) {
    char final_path[PRESET_PATH_MAX];
    char tmp_path[PRESET_PATH_MAX];
    PfsResult fr;
    PresetResult r;

    if (!slot_valid(slot)) return PRESET_E_BAD_SLOT;
    if (!fs->mount(fs->ctx)) return PRESET_E_NO_STORAGE;
    if (!make_path(final_path, sizeof final_path, fs, slot, "") ||
        !make_path(tmp_path, sizeof tmp_path, fs, slot, TMP_SUFFIX)) {
        return PRESET_E_IO;
    }
    r = read_and_parse(fs, final_path, out, &fr);
    if (fr != PFS_NOT_FOUND) return r;
    if (!fs->mount(fs->ctx)) return PRESET_E_NO_STORAGE; /* card removed meanwhile */
    return read_and_parse(fs, tmp_path, out, &fr);
}
