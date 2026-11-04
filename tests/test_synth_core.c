/* Host unit tests for synth_core. No framework: CHECK counts and reports. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define FUZZ_ITERATIONS 20000
#define FUZZ_SEED 0x2545F491u

static const int k_int_edges[] = {
    INT_MIN, INT_MIN + 1, -1, 0, 1, 24, 25, 26, 99, 100, 101, 149, 150,
    399, 400, 401, 999, 1000, 1001, 1049, 1050, 1051, INT_MAX - 1, INT_MAX
};

#define INT_EDGE_COUNT (sizeof(k_int_edges) / sizeof(k_int_edges[0]))

typedef struct {
    int min_hz;
    int max_hz;
} PitchRange;

static const int k_grid_sens[] = {25, 50, 100, 150, 200, 400};
static const PitchRange k_grid_ranges[] = {
    {100, 150}, {100, 1050}, {600, 650}, {1000, 1050}
};
static const float k_pitch_specials[] = {
    NAN, INFINITY, -INFINITY, FLT_MAX, -FLT_MAX, 1e30f, -1e30f,
    1e6f, -1e6f, 1000.0f, -1000.0f, 91.0f, -91.0f, 90.0f, -90.0f,
    -0.0f, 1e-40f
};

#define GRID_SENS_COUNT (sizeof(k_grid_sens) / sizeof(k_grid_sens[0]))
#define GRID_RANGE_COUNT (sizeof(k_grid_ranges) / sizeof(k_grid_ranges[0]))
#define PITCH_SPECIAL_COUNT (sizeof(k_pitch_specials) / sizeof(k_pitch_specials[0]))

static SynthSettings make_settings(int sens, int min_hz, int max_hz) {
    SynthSettings s;
    s.sensitivity_pct = sens;
    s.pitch_min_hz = min_hz;
    s.pitch_max_hz = max_hz;
    return s;
}

static SynthSettings make_grid_settings(size_t sens_index, size_t range_index) {
    return make_settings(k_grid_sens[sens_index], k_grid_ranges[range_index].min_hz,
                         k_grid_ranges[range_index].max_hz);
}

static bool same_settings(const SynthSettings *a, const SynthSettings *b) {
    return a->sensitivity_pct == b->sensitivity_pct &&
           a->pitch_min_hz == b->pitch_min_hz &&
           a->pitch_max_hz == b->pitch_max_hz;
}

static bool settings_in_bounds(const SynthSettings *s) {
    if (s->sensitivity_pct < SENS_MIN_PCT || s->sensitivity_pct > SENS_MAX_PCT) {
        return false;
    }
    if (s->pitch_min_hz < PITCH_FLOOR_HZ || s->pitch_min_hz > PITCH_CEIL_HZ - PITCH_MIN_SPAN_HZ) {
        return false;
    }
    return s->pitch_max_hz >= s->pitch_min_hz + PITCH_MIN_SPAN_HZ &&
           s->pitch_max_hz <= PITCH_CEIL_HZ;
}

/* Reinterprets 32 random bits as a two's complement int without UB. */
static int bits_to_int(uint32_t bits) {
    int out;
    memcpy(&out, &bits, sizeof out);
    return out;
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* One quarter of the draws come from the boundary table, the rest are full range. */
static int next_fuzz_int(uint32_t *rng) {
    uint32_t r = xorshift32(rng);
    if ((r & 3u) == 0u) {
        return k_int_edges[(r >> 2) % INT_EDGE_COUNT];
    }
    return bits_to_int(xorshift32(rng));
}

/* T1 */
static void test_defaults(void) {
    SynthSettings s;
    settings_defaults(&s);
    CHECK(s.sensitivity_pct == 100);
    CHECK(s.pitch_min_hz == 100);
    CHECK(s.pitch_max_hz == 1000);
}

/* T2 */
static void test_sanitize_defaults_and_idempotence(void) {
    SynthSettings defaults;
    SynthSettings s;
    settings_defaults(&defaults);
    s = defaults;
    settings_sanitize(&s);
    CHECK(same_settings(&s, &defaults));
    settings_sanitize(&s);
    CHECK(same_settings(&s, &defaults));
}

static int sanitized_sensitivity(int value) {
    SynthSettings s = make_settings(value, 100, 1000);
    settings_sanitize(&s);
    return s.sensitivity_pct;
}

/* T3 */
static void test_sensitivity_clamp(void) {
    CHECK(sanitized_sensitivity(INT_MIN) == 25);
    CHECK(sanitized_sensitivity(0) == 25);
    CHECK(sanitized_sensitivity(24) == 25);
    CHECK(sanitized_sensitivity(25) == 25);
    CHECK(sanitized_sensitivity(400) == 400);
    CHECK(sanitized_sensitivity(401) == 400);
    CHECK(sanitized_sensitivity(INT_MAX) == 400);
}

static void expect_pitch_range(int min_in, int max_in, int min_out, int max_out) {
    SynthSettings s = make_settings(100, min_in, max_in);
    settings_sanitize(&s);
    CHECK(s.pitch_min_hz == min_out);
    CHECK(s.pitch_max_hz == max_out);
}

/* T4 */
static void test_pitch_range_clamp(void) {
    expect_pitch_range(900, 100, 900, 950);
    expect_pitch_range(INT_MIN, INT_MAX, 100, 1050);
    expect_pitch_range(1050, 0, 1000, 1050);
    expect_pitch_range(1000, 1050, 1000, 1050);
    expect_pitch_range(1000, 1049, 1000, 1050);
    expect_pitch_range(100, 149, 100, 150);
}

/* T5: random 32-bit fields plus boundary values; invariant and idempotence. */
static void test_sanitize_fuzz(void) {
    uint32_t rng = FUZZ_SEED;
    int failures = 0;
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        SynthSettings raw;
        SynthSettings once;
        SynthSettings twice;
        raw.sensitivity_pct = next_fuzz_int(&rng);
        raw.pitch_min_hz = next_fuzz_int(&rng);
        raw.pitch_max_hz = next_fuzz_int(&rng);
        once = raw;
        settings_sanitize(&once);
        twice = once;
        settings_sanitize(&twice);
        if (!settings_in_bounds(&once) || !same_settings(&once, &twice)) {
            failures++;
        }
    }
    CHECK(failures == 0);
}

/* Copied verbatim from the original map_pitch_to_freq in main.c, with the
 * MIN_TARGET_FREQ_HZ constant written as its value (100). */
static int legacy_map_pitch_to_freq(float pitch) {
    if (!isfinite(pitch)) return 100;
    pitch = fmaxf(-1000.0f, fminf(pitch, 1000.0f));
    return 100 + ((pitch + 90.0f) / 180.0f) * 900;
}

/* T6: default settings reproduce the legacy mapping exactly over -90..90 degrees. */
static void test_default_matches_legacy_mapping(void) {
    SynthSettings defaults;
    settings_defaults(&defaults);
    for (int k = -1800; k <= 1800; k++) {
        float pitch = (float)k * 0.05f;
        CHECK(settings_map_pitch_to_freq(pitch, &defaults) == legacy_map_pitch_to_freq(pitch));
    }
}

/* T7: NaN and infinities map to the minimum of the sanitized range. */
static void test_non_finite_maps_to_min(void) {
    const float bad[] = {NAN, INFINITY, -INFINITY};
    for (size_t si = 0; si < GRID_SENS_COUNT; si++) {
        for (size_t ri = 0; ri < GRID_RANGE_COUNT; ri++) {
            SynthSettings s = make_grid_settings(si, ri);
            for (size_t bi = 0; bi < sizeof(bad) / sizeof(bad[0]); bi++) {
                CHECK(settings_map_pitch_to_freq(bad[bi], &s) == k_grid_ranges[ri].min_hz);
            }
        }
    }
}

/* T8: every finite or non-finite pitch lands inside [min, max] for every grid setting. */
static void test_result_bounded_by_range(void) {
    for (size_t si = 0; si < GRID_SENS_COUNT; si++) {
        for (size_t ri = 0; ri < GRID_RANGE_COUNT; ri++) {
            SynthSettings s = make_grid_settings(si, ri);
            for (size_t pi = 0; pi < PITCH_SPECIAL_COUNT; pi++) {
                int hz = settings_map_pitch_to_freq(k_pitch_specials[pi], &s);
                CHECK(hz >= k_grid_ranges[ri].min_hz && hz <= k_grid_ranges[ri].max_hz);
            }
        }
    }
}

/* T9: hand-computed values, including the 22.5 degree half-span at 400 %. */
static void test_known_values(void) {
    SynthSettings s400 = make_settings(400, 100, 1000);
    SynthSettings s25 = make_settings(25, 100, 1000);
    SynthSettings s100 = make_settings(100, 100, 1000);
    CHECK(settings_map_pitch_to_freq(11.25f, &s400) == 775);
    CHECK(settings_map_pitch_to_freq(90.0f, &s25) == 662);
    CHECK(settings_map_pitch_to_freq(-90.0f, &s25) == 437);
    CHECK(settings_map_pitch_to_freq(90.0f, &s100) == 1000);
}

/* T10: pitch -400..400 in steps of 0.1 never decreases the frequency. */
static void test_monotonic_in_pitch(void) {
    for (size_t si = 0; si < GRID_SENS_COUNT; si++) {
        for (size_t ri = 0; ri < GRID_RANGE_COUNT; ri++) {
            SynthSettings s = make_grid_settings(si, ri);
            int previous = settings_map_pitch_to_freq(-400.0f, &s);
            bool ok = true;
            for (int k = -4000; k <= 4000; k++) {
                int hz = settings_map_pitch_to_freq((float)k * 0.1f, &s);
                if (hz < previous) {
                    ok = false;
                }
                previous = hz;
            }
            CHECK(ok);
        }
    }
}

/* T11: corrupt structs are sanitized inside the mapping, not trusted. */
static void test_corrupt_struct_stays_in_range(void) {
    const SynthSettings corrupt[] = {
        {INT_MIN, 2000, -5},
        {0, 0, 0}
    };
    for (size_t ci = 0; ci < sizeof(corrupt) / sizeof(corrupt[0]); ci++) {
        SynthSettings clean = corrupt[ci];
        settings_sanitize(&clean);
        for (size_t pi = 0; pi < PITCH_SPECIAL_COUNT; pi++) {
            int hz = settings_map_pitch_to_freq(k_pitch_specials[pi], &corrupt[ci]);
            CHECK(hz >= clean.pitch_min_hz && hz <= clean.pitch_max_hz);
        }
    }
}

/* T12: full pitch -> Hz path stays within the synth limits for every option
 * combination and every LFO offset, including extreme offsets. */
static void test_target_freq_bounded(void) {
    const int lfos[] = {INT_MIN, INT_MAX, -2100, 0, 2100};
    for (size_t si = 0; si < GRID_SENS_COUNT; si++) {
        for (size_t ri = 0; ri < GRID_RANGE_COUNT; ri++) {
            SynthSettings s = make_grid_settings(si, ri);
            bool ok = true;
            for (size_t pi = 0; pi < PITCH_SPECIAL_COUNT; pi++) {
                for (int snap = 0; snap < 2; snap++) {
                    for (int oct = 0; oct < 2; oct++) {
                        for (size_t li = 0; li < sizeof(lfos) / sizeof(lfos[0]); li++) {
                            int hz = synth_target_freq(k_pitch_specials[pi], snap != 0, oct != 0,
                                                       lfos[li], &s);
                            if (hz < MIN_TARGET_FREQ_HZ || hz > MAX_TARGET_FREQ_HZ) {
                                ok = false;
                            }
                        }
                    }
                }
            }
            CHECK(ok);
        }
    }
}

/* T12 (phase step): the phase step is monotonic over the whole synth range. */
static void test_phase_step_monotonic(void) {
    bool ok = true;
    uint32_t previous = freq_to_phase_step(MIN_TARGET_FREQ_HZ);
    for (int hz = MIN_TARGET_FREQ_HZ + 1; hz <= MAX_TARGET_FREQ_HZ; hz++) {
        uint32_t step = freq_to_phase_step(hz);
        if (step < previous) {
            ok = false;
        }
        previous = step;
    }
    CHECK(ok);
    CHECK(freq_to_phase_step(MIN_TARGET_FREQ_HZ) < freq_to_phase_step(MAX_TARGET_FREQ_HZ));
}

/* Copied verbatim from the original map_roll_to_vol in main.c. */
static int legacy_map_roll_to_vol(float roll) {
    int vol = 255 - (abs((int)roll) * 255 / 180);
    if(vol < 0) vol = 0;
    if(vol > 255) vol = 255;
    return vol;
}

/* T13: non-finite and very large roll values mute; all results are 0..255. */
static void test_roll_edges(void) {
    const float edges[] = {NAN, INFINITY, -INFINITY, FLT_MAX, -FLT_MAX};
    bool ok = true;
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++) {
        CHECK(map_roll_to_vol(edges[i]) == 0);
    }
    for (size_t pi = 0; pi < PITCH_SPECIAL_COUNT; pi++) {
        int vol = map_roll_to_vol(k_pitch_specials[pi]);
        if (vol < 0 || vol > 255) {
            ok = false;
        }
    }
    CHECK(ok);
}

/* T13: roll -180..180 in steps of 0.5 degrees matches the original formula. */
static void test_roll_matches_legacy(void) {
    for (int k = -360; k <= 360; k++) {
        float roll = (float)k * 0.5f;
        CHECK(map_roll_to_vol(roll) == legacy_map_roll_to_vol(roll));
    }
}

/* T12 (target path): hand-checked values and the octave/LFO order. */
static void test_target_freq_order(void) {
    SynthSettings s = make_settings(100, 100, 1000);
    CHECK(synth_target_freq(90.0f, false, false, 0, &s) == 1000);
    CHECK(synth_target_freq(90.0f, false, true, 0, &s) == 2000);
    CHECK(synth_target_freq(90.0f, false, true, 100, &s) == 2100);
    CHECK(synth_target_freq(90.0f, false, true, 200, &s) == 2100);
    CHECK(synth_target_freq(-90.0f, false, false, -200, &s) == 100);
    /* Extreme offsets saturate: they must not wrap around the int range. */
    CHECK(synth_target_freq(90.0f, false, false, INT_MAX, &s) == 2100);
    CHECK(synth_target_freq(0.0f, false, true, INT_MAX, &s) == 2100);
    CHECK(synth_target_freq(90.0f, false, false, INT_MIN, &s) == 100);
}

static bool settings_equal(const SynthSettings *s, int sens, int min_hz, int max_hz) {
    return s->sensitivity_pct == sens && s->pitch_min_hz == min_hz && s->pitch_max_hz == max_hz;
}

/* T14: menu adjustments respect the bounds and the min/max interaction. */
static void test_field_adjust(void) {
    SynthSettings s;

    s = make_settings(100, 100, 1000);
    settings_field_adjust(&s, FIELD_SENS, -1);
    CHECK(settings_equal(&s, 75, 100, 1000));

    s = make_settings(25, 100, 1000);
    settings_field_adjust(&s, FIELD_SENS, -1);
    CHECK(settings_equal(&s, 25, 100, 1000));

    s = make_settings(400, 100, 1000);
    settings_field_adjust(&s, FIELD_SENS, 1);
    CHECK(settings_equal(&s, 400, 100, 1000));

    s = make_settings(INT_MAX, 100, 1000);
    settings_field_adjust(&s, FIELD_SENS, 1);
    CHECK(settings_equal(&s, 400, 100, 1000));

    s = make_settings(100, 950, 1000);
    settings_field_adjust(&s, FIELD_PITCH_MIN, 1);
    CHECK(settings_equal(&s, 100, 1000, 1050));

    s = make_settings(100, 100, 1050);
    settings_field_adjust(&s, FIELD_PITCH_MAX, 1);
    CHECK(settings_equal(&s, 100, 100, 1050));
}

static void test_field_adjust_reset_and_noop(void) {
    SynthSettings s = make_settings(25, 500, 600);
    SynthSettings defaults;
    settings_defaults(&defaults);

    settings_field_adjust(&s, FIELD_RESET, 1);
    CHECK(same_settings(&s, &defaults));

    s = make_settings(200, 300, 700);
    settings_field_adjust(&s, FIELD_BACK, 1);
    CHECK(settings_equal(&s, 200, 300, 700));

    settings_field_adjust(&s, FIELD_SENS, 0);
    CHECK(settings_equal(&s, 200, 300, 700));

    settings_field_adjust(&s, FIELD_RESET, 0);
    CHECK(settings_equal(&s, 200, 300, 700));
}

static bool field_text_equals(const SynthSettings *s, SettingsField field, const char *expected) {
    char buf[64];
    int len = settings_field_text(s, field, buf, sizeof buf);
    return len == (int)strlen(expected) && strcmp(buf, expected) == 0;
}

static bool bytes_all_equal(const unsigned char *p, size_t from, size_t to, unsigned char value) {
    for (size_t i = from; i < to; i++) {
        if (p[i] != value) {
            return false;
        }
    }
    return true;
}

/* T15: exact menu labels, with the value shown after sanitization. */
static void test_field_text_labels(void) {
    SynthSettings s = make_settings(100, 100, 1000);
    CHECK(field_text_equals(&s, FIELD_SENS, "Sensitivity: 100%"));
    CHECK(field_text_equals(&s, FIELD_PITCH_MIN, "Pitch min: 100 Hz"));
    CHECK(field_text_equals(&s, FIELD_PITCH_MAX, "Pitch max: 1000 Hz"));
    CHECK(field_text_equals(&s, FIELD_RESET, "Reset to defaults"));
    CHECK(field_text_equals(&s, FIELD_BACK, "Back"));

    char unknown[4] = {'x', 'x', 'x', 'x'};
    CHECK(settings_field_text(&s, FIELD_COUNT, unknown, sizeof unknown) == 0);
    CHECK(unknown[0] == '\0');

    s = make_settings(INT_MAX, 0, 0);
    CHECK(field_text_equals(&s, FIELD_SENS, "Sensitivity: 400%"));
    CHECK(field_text_equals(&s, FIELD_PITCH_MIN, "Pitch min: 100 Hz"));
    CHECK(field_text_equals(&s, FIELD_PITCH_MAX, "Pitch max: 150 Hz"));
}

/* T15: cap 0 writes nothing; cap 1 and 2 are NUL-terminated and never write
 * past cap (0xA5 guard bytes follow the buffer). */
static void test_field_text_cap(void) {
    SynthSettings s = make_settings(100, 100, 1000);
    unsigned char area[80];
    char *buf = (char *)area;
    const int full = (int)strlen("Sensitivity: 100%");

    memset(area, 0xA5, sizeof area);
    CHECK(settings_field_text(&s, FIELD_SENS, buf, 0) == full);
    CHECK(bytes_all_equal(area, 0, sizeof area, 0xA5));

    memset(area, 0xA5, sizeof area);
    CHECK(settings_field_text(&s, FIELD_SENS, buf, 1) == full);
    CHECK(area[0] == '\0');
    CHECK(bytes_all_equal(area, 1, sizeof area, 0xA5));

    memset(area, 0xA5, sizeof area);
    CHECK(settings_field_text(&s, FIELD_SENS, buf, 2) == full);
    CHECK(area[0] == 'S' && area[1] == '\0');
    CHECK(bytes_all_equal(area, 2, sizeof area, 0xA5));
}

int main(void) {
    test_defaults();
    test_sanitize_defaults_and_idempotence();
    test_sensitivity_clamp();
    test_pitch_range_clamp();
    test_sanitize_fuzz();
    test_default_matches_legacy_mapping();
    test_non_finite_maps_to_min();
    test_result_bounded_by_range();
    test_known_values();
    test_monotonic_in_pitch();
    test_corrupt_struct_stays_in_range();
    test_target_freq_bounded();
    test_phase_step_monotonic();
    test_roll_edges();
    test_roll_matches_legacy();
    test_target_freq_order();
    test_field_adjust();
    test_field_adjust_reset_and_noop();
    test_field_text_labels();
    test_field_text_cap();

    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
