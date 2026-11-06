/* Host tests for the stdio file layer against a real temporary directory. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "preset_fs_stdio.h"
#include "synth_core.h"

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        g_checks++;                                                  \
        if (!(cond)) {                                               \
            g_failed++;                                              \
            printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                            \
    } while (0)

#define TMP_DIR "build/presets_tmp"
#define P1 TMP_DIR "/preset1.cfg"

static bool g_mounted = true;
static bool mount_stub(void) { return g_mounted; }

static void cleanup(void) {
    remove(P1);
    remove(P1 ".tmp");
    remove(TMP_DIR "/preset2.cfg");
    remove(TMP_DIR);
}

static bool write_raw(const char *path, size_t n) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    for (size_t i = 0; i < n; i++) fputc('#', f);
    return fclose(f) == 0;
}

static bool exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f != NULL) fclose(f);
    return f != NULL;
}

static void test_save_load_overwrite(void) {
    StdioFsCtx ctx = {mount_stub};
    PresetFs fs;
    SynthSettings a, b, out;
    cleanup();
    preset_fs_stdio_init(&fs, &ctx, TMP_DIR);
    settings_defaults(&a);
    b = a;
    b.sensitivity_pct = 250;
    b.wave_default = 4;
    CHECK(preset_save(&fs, 1, &a) == PRESET_OK); /* creates the directory */
    CHECK(preset_save(&fs, 1, &b) == PRESET_OK); /* replaces an existing file */
    CHECK(!exists(P1 ".tmp"));
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK);
    CHECK(out.sensitivity_pct == 250 && out.wave_default == 4);
    CHECK(preset_load(&fs, 2, &out) == PRESET_E_NOT_FOUND);
    cleanup();
}

static void test_missing_parent(void) {
    StdioFsCtx ctx = {NULL};
    PresetFs fs;
    SynthSettings s;
    settings_defaults(&s);
    preset_fs_stdio_init(&fs, &ctx, "build/no_such_parent/deeper");
    CHECK(preset_save(&fs, 1, &s) == PRESET_E_IO);
}

static void test_size_boundary(void) {
    StdioFsCtx ctx = {NULL};
    PresetFs fs;
    SynthSettings out;
    cleanup();
    preset_fs_stdio_init(&fs, &ctx, TMP_DIR);
    {
        SynthSettings s;
        settings_defaults(&s);
        CHECK(preset_save(&fs, 1, &s) == PRESET_OK);
    }
    CHECK(write_raw(P1, 1025));
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_TOO_BIG);
    CHECK(write_raw(P1, 1024));
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_CORRUPT); /* at the cap: parsed, not TOO_BIG (line over 80) */
    cleanup();
}

static void test_unmounted(void) {
    StdioFsCtx ctx = {mount_stub};
    PresetFs fs;
    SynthSettings s;
    settings_defaults(&s);
    preset_fs_stdio_init(&fs, &ctx, TMP_DIR);
    g_mounted = false;
    CHECK(preset_save(&fs, 1, &s) == PRESET_E_NO_STORAGE);
    CHECK(preset_load(&fs, 1, &s) == PRESET_E_NO_STORAGE);
    g_mounted = true;
}

int main(void) {
    test_save_load_overwrite();
    test_missing_parent();
    test_size_boundary();
    test_unmounted();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
