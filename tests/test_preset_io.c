/* Host tests for preset_save / preset_load with an in-memory fake file layer
 * that mimics FAT (rename onto an existing file fails) and injects faults. */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "preset_io.h"
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

#define MAX_FILES 8
#define FILE_CAP 2048
#define DIR_NAME "card/app"

typedef struct {
    bool used;
    char name[PRESET_PATH_MAX];
    char data[FILE_CAP];
    size_t len;
} FakeFile;

typedef struct {
    FakeFile files[MAX_FILES];
    int mount_calls_ok;  /* mount succeeds for this many calls; -1 = always */
    int mount_calls;
    int ops;             /* write/rename/remove/mkdir counter */
    int fail_op;         /* this op number fails (1-based); 0 = none */
    int cut_after;       /* ops after this number do nothing and fail; 0 = none */
    bool mkdir_fails;
    bool read_io_error;
    bool rename_always_fails;
    int mkdir_calls;
    int write_calls;
    int remove_calls;
} Fake;

static FakeFile *find(Fake *f, const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (f->files[i].used && strcmp(f->files[i].name, name) == 0) return &f->files[i];
    }
    return NULL;
}

static FakeFile *create(Fake *f, const char *name) {
    FakeFile *e = find(f, name);
    if (e != NULL) return e;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!f->files[i].used) {
            f->files[i].used = true;
            snprintf(f->files[i].name, sizeof f->files[i].name, "%s", name);
            f->files[i].len = 0;
            return &f->files[i];
        }
    }
    return NULL;
}

static bool op_blocked(Fake *f) {
    f->ops++;
    if (f->cut_after != 0 && f->ops > f->cut_after) return true;
    return f->fail_op != 0 && f->ops == f->fail_op;
}

static bool fk_mount(void *c) {
    Fake *f = c;
    f->mount_calls++;
    return f->mount_calls_ok < 0 || f->mount_calls <= f->mount_calls_ok;
}

static PfsResult fk_read(void *c, const char *p, char *buf, size_t cap, size_t *len) {
    Fake *f = c;
    FakeFile *e = find(f, p);
    if (f->read_io_error) return PFS_IO;
    if (e == NULL) return PFS_NOT_FOUND;
    if (e->len > cap) return PFS_TOO_BIG;
    memcpy(buf, e->data, e->len);
    *len = e->len;
    return PFS_OK;
}

static PfsResult fk_write(void *c, const char *p, const char *d, size_t n) {
    Fake *f = c;
    FakeFile *e;
    f->write_calls++;
    if (op_blocked(f)) return PFS_IO;
    e = create(f, p);
    if (e == NULL || n > FILE_CAP) return PFS_IO;
    memcpy(e->data, d, n);
    e->len = n;
    return PFS_OK;
}

static PfsResult fk_rename(void *c, const char *from, const char *to) {
    Fake *f = c;
    FakeFile *e;
    if (op_blocked(f) || f->rename_always_fails) return PFS_IO;
    e = find(f, from);
    if (e == NULL || find(f, to) != NULL) return PFS_IO; /* FAT: target exists */
    snprintf(e->name, sizeof e->name, "%s", to);
    return PFS_OK;
}

static PfsResult fk_remove(void *c, const char *p) {
    Fake *f = c;
    FakeFile *e;
    f->remove_calls++;
    if (op_blocked(f)) return PFS_IO;
    e = find(f, p);
    if (e == NULL) return PFS_NOT_FOUND;
    e->used = false;
    return PFS_OK;
}

static PfsResult fk_mkdir(void *c, const char *p) {
    Fake *f = c;
    (void)p;
    f->mkdir_calls++;
    return f->mkdir_fails ? PFS_IO : PFS_OK;
}

static PresetFs make_fs(Fake *f) {
    PresetFs fs = {f, DIR_NAME, fk_mount, fk_read, fk_write, fk_rename, fk_remove, fk_mkdir};
    memset(f, 0, sizeof *f);
    f->mount_calls_ok = -1;
    return fs;
}

static SynthSettings sample(int sens) {
    SynthSettings s;
    settings_defaults(&s);
    s.sensitivity_pct = sens;
    s.pitch_min_hz = 150;
    s.pitch_max_hz = 900;
    s.wave_default = 3;
    s.snap_default = 1;
    s.roll_volume = 0;
    return s;
}

static bool same(const SynthSettings *a, const SynthSettings *b) {
    return a->sensitivity_pct == b->sensitivity_pct && a->pitch_min_hz == b->pitch_min_hz &&
           a->pitch_max_hz == b->pitch_max_hz && a->wave_default == b->wave_default &&
           a->snap_default == b->snap_default && a->roll_volume == b->roll_volume;
}

static int file_count(Fake *f) {
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++) n += f->files[i].used ? 1 : 0;
    return n;
}

static void test_f1_no_storage(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(200), out;
    f.mount_calls_ok = 0;
    CHECK(preset_save(&fs, 1, &s) == PRESET_E_NO_STORAGE);
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_NO_STORAGE);
    CHECK(f.write_calls == 0 && f.remove_calls == 0 && f.mkdir_calls == 0);
}

static void test_f1b_card_removed_between_reads(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings out;
    f.mount_calls_ok = 1; /* first mount ok, re-check after NOT_FOUND fails */
    CHECK(preset_load(&fs, 2, &out) == PRESET_E_NO_STORAGE);
}

static void test_f2_bad_slots(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(100), out;
    const int bad[] = {0, 5, -1, 1000};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECK(preset_save(&fs, bad[i], &s) == PRESET_E_BAD_SLOT);
        CHECK(preset_load(&fs, bad[i], &out) == PRESET_E_BAD_SLOT);
    }
    CHECK(f.ops == 0 && f.mount_calls == 0);
}

static void test_f3_round_trip(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(175), out;
    memset(&out, 0, sizeof out);
    CHECK(preset_save(&fs, 2, &s) == PRESET_OK);
    CHECK(find(&f, DIR_NAME "/preset2.cfg") != NULL);
    CHECK(find(&f, DIR_NAME "/preset2.cfg.tmp") == NULL);
    CHECK(file_count(&f) == 1);
    CHECK(preset_load(&fs, 2, &out) == PRESET_OK);
    CHECK(same(&s, &out));
    CHECK(preset_load(&fs, 3, &out) == PRESET_E_NOT_FOUND);
}

static void test_f4_mkdir_failure_ignored(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(100);
    f.mkdir_fails = true;
    CHECK(preset_save(&fs, 1, &s) == PRESET_OK);
}

static void test_f5_write_failure_keeps_old(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings old = sample(100), neu = sample(300), out;
    CHECK(preset_save(&fs, 1, &old) == PRESET_OK);
    f.ops = 0;
    f.fail_op = 1; /* the write of the temporary file */
    CHECK(preset_save(&fs, 1, &neu) == PRESET_E_IO);
    f.fail_op = 0;
    CHECK(find(&f, DIR_NAME "/preset1.cfg.tmp") == NULL);
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK && same(&old, &out));
}

static void test_f6_overwrite_existing(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings old = sample(100), neu = sample(300), out;
    CHECK(preset_save(&fs, 1, &old) == PRESET_OK);
    CHECK(preset_save(&fs, 1, &neu) == PRESET_OK);
    CHECK(file_count(&f) == 1);
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK && same(&neu, &out));
}

static void test_f7_rename_fails_twice(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings old = sample(100), neu = sample(300), out;
    CHECK(preset_save(&fs, 1, &old) == PRESET_OK);
    f.rename_always_fails = true;
    CHECK(preset_save(&fs, 1, &neu) == PRESET_E_IO);
    f.rename_always_fails = false;
    CHECK(find(&f, DIR_NAME "/preset1.cfg.tmp") != NULL);
    CHECK(find(&f, DIR_NAME "/preset1.cfg") == NULL);
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK && same(&neu, &out));
}

static void test_f8_power_cut_after_remove(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings old = sample(100), neu = sample(300), out;
    CHECK(preset_save(&fs, 1, &old) == PRESET_OK);
    f.ops = 0;
    f.cut_after = 3; /* write, failed rename, remove succeed; second rename is lost */
    CHECK(preset_save(&fs, 1, &neu) == PRESET_E_IO);
    f.cut_after = 0;
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK && same(&neu, &out));
}

static void test_f9_read_errors(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(100), out;
    FakeFile *big;
    CHECK(preset_save(&fs, 1, &s) == PRESET_OK);
    f.read_io_error = true;
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_IO);
    f.read_io_error = false;
    big = find(&f, DIR_NAME "/preset1.cfg");
    big->len = PRESET_MAX_BYTES + 1;
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_TOO_BIG);
}

static void test_f10_remove_final_fails(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings old = sample(100), neu = sample(300), out;
    CHECK(preset_save(&fs, 1, &old) == PRESET_OK);
    f.ops = 0;
    f.fail_op = 3; /* write=1, rename=2 (EEXIST), remove(final)=3 */
    CHECK(preset_save(&fs, 1, &neu) == PRESET_E_IO);
    f.fail_op = 0;
    CHECK(find(&f, DIR_NAME "/preset1.cfg.tmp") != NULL);
    CHECK(preset_load(&fs, 1, &out) == PRESET_OK && same(&old, &out));
}

static void test_corrupt_final_not_recovered(void) {
    Fake f;
    PresetFs fs = make_fs(&f);
    SynthSettings s = sample(100), out;
    FakeFile *e;
    CHECK(preset_save(&fs, 1, &s) == PRESET_OK);
    e = find(&f, DIR_NAME "/preset1.cfg");
    e->data[0] = 'x';
    CHECK(preset_load(&fs, 1, &out) == PRESET_E_CORRUPT);
}

int main(void) {
    test_f1_no_storage();
    test_f1b_card_removed_between_reads();
    test_f2_bad_slots();
    test_f3_round_trip();
    test_f4_mkdir_failure_ignored();
    test_f5_write_failure_keeps_old();
    test_f6_overwrite_existing();
    test_f7_rename_fails_twice();
    test_f8_power_cut_after_remove();
    test_f9_read_errors();
    test_f10_remove_final_fails();
    test_corrupt_final_not_recovered();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
