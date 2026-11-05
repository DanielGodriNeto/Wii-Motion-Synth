/* Host unit tests for preset_store: serialize, parse and the file format rules.
 * No framework: CHECK counts and reports. No file I/O. */
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "preset_store.h"
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
#define MAX_TEXT 1200

/* Golden bytes of the serialized defaults (version 1, LF line ends). */
static const char GOLDEN_DEFAULTS[] =
    "version=1\n"
    "wave=sine\n"
    "snap=0\n"
    "roll_volume=1\n"
    "sensitivity=100\n"
    "pitch_min=100\n"
    "pitch_max=1000\n"
    "end=1\n";

#define GOLDEN_LEN (sizeof GOLDEN_DEFAULTS - 1)

/* Shared text pieces for the variant files below. */
#define HEAD "version=1\nwave=sine\nsnap=0\nroll_volume=1\n"
#define TAIL "pitch_min=100\npitch_max=1000\nend=1\n"
#define BODY "wave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n" TAIL

static const char *const WAVE_NAME_TABLE[] = {
    "sine", "square", "sawtooth", "triangle", "noise", "pulse", "narrow"
};

static void set_sentinel(SynthSettings *s) {
    s->sensitivity_pct = 777;
    s->pitch_min_hz = 888;
    s->pitch_max_hz = 999;
    s->wave_default = 5;
    s->snap_default = 1;
    s->roll_volume = 0;
}

static bool same_settings(const SynthSettings *a, const SynthSettings *b) {
    return a->sensitivity_pct == b->sensitivity_pct &&
           a->pitch_min_hz == b->pitch_min_hz &&
           a->pitch_max_hz == b->pitch_max_hz &&
           a->wave_default == b->wave_default &&
           a->snap_default == b->snap_default &&
           a->roll_volume == b->roll_volume;
}

/* Literal expectations, not taken from the implementation. */
static bool is_golden_defaults(const SynthSettings *s) {
    return s->wave_default == 0 && s->snap_default == 0 && s->roll_volume == 1 &&
           s->sensitivity_pct == 100 && s->pitch_min_hz == 100 && s->pitch_max_hz == 1000;
}

static bool flags_in_bounds(const SynthSettings *s) {
    return s->wave_default >= 0 && s->wave_default < WAVE_COUNT &&
           (s->snap_default == 0 || s->snap_default == 1) &&
           (s->roll_volume == 0 || s->roll_volume == 1);
}

static bool pitch_in_bounds(const SynthSettings *s) {
    return s->sensitivity_pct >= SENS_MIN_PCT && s->sensitivity_pct <= SENS_MAX_PCT &&
           s->pitch_min_hz >= PITCH_FLOOR_HZ &&
           s->pitch_min_hz <= PITCH_CEIL_HZ - PITCH_MIN_SPAN_HZ &&
           s->pitch_max_hz >= s->pitch_min_hz + PITCH_MIN_SPAN_HZ &&
           s->pitch_max_hz <= PITCH_CEIL_HZ;
}

static bool in_bounds(const SynthSettings *s) {
    return flags_in_bounds(s) && pitch_in_bounds(s);
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Parses with a sentinel in out. On any error out must still hold the sentinel. */
static PresetResult parse_with_sentinel(const char *buf, size_t len, SynthSettings *out) {
    SynthSettings before;
    PresetResult r;
    set_sentinel(out);
    before = *out;
    r = preset_parse(buf, len, out);
    if (r != PRESET_OK) {
        CHECK(same_settings(out, &before));
    }
    return r;
}

static void expect_parse(const char *buf, size_t len, PresetResult want) {
    SynthSettings out;
    PresetResult got = parse_with_sentinel(buf, len, &out);
    if (got != want) {
        printf("%s:%d: parse of %lu bytes gave %d, want %d\n", __FILE__, __LINE__,
               (unsigned long)len, (int)got, (int)want);
    }
    CHECK(got == want);
}

static void expect_text(const char *text, PresetResult want) {
    expect_parse(text, strlen(text), want);
}

/* Appends raw bytes after the golden file. */
static size_t golden_plus_bytes(char *buf, const char *extra, size_t extra_len) {
    memcpy(buf, GOLDEN_DEFAULTS, GOLDEN_LEN);
    memcpy(buf + GOLDEN_LEN, extra, extra_len);
    return GOLDEN_LEN + extra_len;
}

static void expect_after_golden(const char *extra, size_t extra_len, PresetResult want) {
    char buf[MAX_TEXT];
    size_t len = golden_plus_bytes(buf, extra, extra_len);
    expect_parse(buf, len, want);
}

#define EXPECT_AFTER_GOLDEN(lit, want) expect_after_golden((lit), sizeof(lit) - 1, (want))

/* Golden file followed by one line of head + 'x' fill, total content_len bytes. */
static size_t golden_plus_line(char *buf, const char *head, size_t content_len) {
    size_t head_len = strlen(head);
    memcpy(buf, GOLDEN_DEFAULTS, GOLDEN_LEN);
    memcpy(buf + GOLDEN_LEN, head, head_len);
    memset(buf + GOLDEN_LEN + head_len, 'x', content_len - head_len);
    buf[GOLDEN_LEN + content_len] = '\n';
    return GOLDEN_LEN + content_len + 1;
}

/* Version line, one line of head + 'x' fill (content_len bytes), then the
 * remaining known keys and end=1. The line sits before end=1, so it is legal. */
static size_t version_plus_line(char *buf, const char *head, size_t content_len) {
    static const char REST[] =
        "wave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\npitch_min=100\npitch_max=1000\nend=1\n";
    size_t head_len = strlen(head);
    size_t len = 0;
    memcpy(buf, "version=1\n", 10);
    len = 10;
    memcpy(buf + len, head, head_len);
    memset(buf + len + head_len, 'x', content_len - head_len);
    len += content_len;
    buf[len++] = '\n';
    memcpy(buf + len, REST, sizeof REST - 1);
    return len + (sizeof REST - 1);
}

/* Appends one comment line that uses at most room bytes, or a blank line when
 * room is 1. Comment content is kept at 80 bytes or less. */
static size_t append_comment(char *buf, size_t len, size_t room) {
    size_t body;
    if (room == 1) {
        buf[len] = '\n';
        return len + 1;
    }
    body = room - 2;
    if (body > 79) {
        body = 79;
    }
    buf[len] = '#';
    memset(buf + len + 1, 'x', body);
    buf[len + 1 + body] = '\n';
    return len + 2 + body;
}

/* Golden file padded with comment lines to exactly total bytes. */
static size_t build_padded(char *buf, size_t total) {
    size_t len = golden_plus_bytes(buf, "", 0);
    while (len < total) {
        len = append_comment(buf, len, total - len);
    }
    return len;
}

static void expect_defaults_value(const char *buf, size_t len) {
    SynthSettings out;
    PresetResult r = parse_with_sentinel(buf, len, &out);
    CHECK(r == PRESET_OK);
    CHECK(is_golden_defaults(&out));
}

/* P1: serialize of defaults equals the golden bytes; sanitizes a copy; deterministic. */
static void test_p1_serialize_golden(void) {
    SynthSettings s;
    char buf[256];
    char again[256];
    size_t n;
    settings_defaults(&s);
    n = preset_serialize(&s, buf, sizeof buf);
    CHECK(n == GOLDEN_LEN);
    CHECK(memcmp(buf, GOLDEN_DEFAULTS, GOLDEN_LEN) == 0);
    CHECK(preset_serialize(&s, again, sizeof again) == n);
    CHECK(memcmp(buf, again, n) == 0);

    s.sensitivity_pct = 9999;
    s.wave_default = 42;
    s.roll_volume = -3;
    n = preset_serialize(&s, buf, sizeof buf);
    CHECK(memcmp(buf, "version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=400\n"
                      "pitch_min=100\npitch_max=1000\nend=1\n", n) == 0);
    CHECK(s.sensitivity_pct == 9999);
}

/* P2: a cap one byte short writes nothing past cap and returns 0. */
static void test_p2_capacity(void) {
    SynthSettings s;
    char buf[256];
    settings_defaults(&s);
    memset(buf, 0xA5, sizeof buf);
    CHECK(preset_serialize(&s, buf, GOLDEN_LEN - 1) == 0);
    CHECK(buf[GOLDEN_LEN - 1] == (char)0xA5);
    CHECK(preset_serialize(&s, buf, 0) == 0);
    CHECK(preset_serialize(&s, buf, GOLDEN_LEN) == GOLDEN_LEN);
    CHECK(preset_serialize(&s, buf, sizeof buf) == GOLDEN_LEN);
}

/* P3: serialize then parse returns every field, over the grid of valid values. */
static void test_p3_round_trip_grid(void) {
    const int sens[] = {25, 100, 400};
    const int pitch_min[] = {100, 100, 150};
    const int pitch_max[] = {1000, 1050, 1050};
    char buf[256];
    for (int w = 0; w < WAVE_COUNT; w++) {
    for (int snap = 0; snap <= 1; snap++) {
    for (int roll = 0; roll <= 1; roll++) {
    for (size_t si = 0; si < sizeof sens / sizeof sens[0]; si++) {
    for (size_t pi = 0; pi < sizeof pitch_min / sizeof pitch_min[0]; pi++) {
        SynthSettings in;
        SynthSettings out;
        size_t n;
        in.wave_default = w;
        in.snap_default = snap;
        in.roll_volume = roll;
        in.sensitivity_pct = sens[si];
        in.pitch_min_hz = pitch_min[pi];
        in.pitch_max_hz = pitch_max[pi];
        n = preset_serialize(&in, buf, sizeof buf);
        CHECK(n > 0);
        CHECK(parse_with_sentinel(buf, n, &out) == PRESET_OK);
        CHECK(same_settings(&in, &out));
    }
    }
    }
    }
    }
}

/* P4: 1024 bytes is accepted, 1025 is TOO_BIG; the size check runs before any byte scan. */
static void test_p4_size_boundary(void) {
    char buf[MAX_TEXT];
    size_t len = build_padded(buf, PRESET_MAX_BYTES);
    CHECK(len == PRESET_MAX_BYTES);
    expect_parse(buf, len, PRESET_OK);
    expect_defaults_value(buf, len);

    len = build_padded(buf, PRESET_MAX_BYTES + 1);
    CHECK(len == PRESET_MAX_BYTES + 1);
    expect_parse(buf, len, PRESET_E_TOO_BIG);

    memset(buf, 0, sizeof buf);
    expect_parse(buf, PRESET_MAX_BYTES + 1, PRESET_E_TOO_BIG);
}

/* P5: a line of 80 content bytes is accepted, 81 is CORRUPT (comment and key lines). */
static void test_p5_line_length(void) {
    char buf[MAX_TEXT];
    size_t len = golden_plus_line(buf, "#", 80);
    expect_parse(buf, len, PRESET_OK);
    len = golden_plus_line(buf, "#", 81);
    expect_parse(buf, len, PRESET_E_CORRUPT);
    len = version_plus_line(buf, "zz=", 80);
    expect_parse(buf, len, PRESET_OK);
    len = version_plus_line(buf, "zz=", 81);
    expect_parse(buf, len, PRESET_E_CORRUPT);
}

/* P6: NUL, 0x01, 0x7F, 0xFF are CORRUPT; tab and CRLF are accepted. */
static void test_p6_byte_rules(void) {
    EXPECT_AFTER_GOLDEN("#a\0b\n", PRESET_E_CORRUPT);
    EXPECT_AFTER_GOLDEN("#\x01\n", PRESET_E_CORRUPT);
    EXPECT_AFTER_GOLDEN("#\x7f\n", PRESET_E_CORRUPT);
    EXPECT_AFTER_GOLDEN("#\xff\n", PRESET_E_CORRUPT);
    EXPECT_AFTER_GOLDEN("#\ttab\n", PRESET_OK);
    EXPECT_AFTER_GOLDEN("\t\n", PRESET_OK);
    expect_text("version=1\r\nwave=sine\r\nsnap=0\r\nroll_volume=1\r\n"
                "sensitivity=100\r\npitch_min=100\r\npitch_max=1000\r\nend=1\r\n",
                PRESET_OK);
    expect_defaults_value("version=1\r\nwave=sine\r\nsnap=0\r\nroll_volume=1\r\n"
                          "sensitivity=100\r\npitch_min=100\r\npitch_max=1000\r\nend=1\r\n",
                          strlen("version=1\r\nwave=sine\r\nsnap=0\r\nroll_volume=1\r\n"
                                 "sensitivity=100\r\npitch_min=100\r\npitch_max=1000\r\nend=1\r\n"));
}

/* P7: version rules. */
static void test_p7_version(void) {
    expect_text(GOLDEN_DEFAULTS, PRESET_OK);
    expect_text("version=2\n" BODY, PRESET_E_VERSION);
    expect_text("version=0\n" BODY, PRESET_E_VERSION);
    expect_text("version=-1\n" BODY, PRESET_E_VERSION);
    expect_text("version=10\n" BODY, PRESET_E_VERSION);
    expect_text("version=abc\n" BODY, PRESET_E_CORRUPT);
    expect_text("version=\n" BODY, PRESET_E_CORRUPT);
    expect_text("version=+1\n" BODY, PRESET_E_CORRUPT);
    expect_text("wave=sine\nversion=1\n"
                "snap=0\nroll_volume=1\nsensitivity=100\npitch_min=100\npitch_max=1000\nend=1\n",
                PRESET_E_CORRUPT);
    expect_text("version=1\nversion=1\n" BODY, PRESET_E_CORRUPT);
    expect_text("wave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n"
                "pitch_min=100\npitch_max=1000\nend=1\n",
                PRESET_E_CORRUPT);
    expect_text("", PRESET_E_TRUNCATED);
    expect_text("# only a comment\n", PRESET_E_TRUNCATED);
    expect_text("\n\n", PRESET_E_TRUNCATED);
}

/* P8: end marker rules. */
static void test_p8_end_marker(void) {
    expect_text("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n"
                "pitch_min=100\npitch_max=1000\n",
                PRESET_E_TRUNCATED);
    expect_text("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n"
                "pitch_min=100\npitch_max=1000\n# end=1\n",
                PRESET_E_TRUNCATED);
    expect_text("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n"
                "pitch_min=100\npitch_max=1000\nend=0\n",
                PRESET_E_CORRUPT);
    expect_text("version=1\n" BODY "snap=1\n", PRESET_E_CORRUPT);
    expect_text("version=1\n" BODY "end=1\n", PRESET_E_CORRUPT);
    expect_text("version=1\n" BODY "# trailing note\n\n", PRESET_OK);
    expect_text("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=100\n"
                "pitch_min=100\npitch_max=1000\nend=1",
                PRESET_OK);
}

/* P9: every prefix of a valid file. Prefixes that stop before the last byte of
 * end=1 are TRUNCATED or CORRUPT; longer ones (the whole end line) are OK. */
static void test_p9_every_cut(void) {
    for (size_t cut = 0; cut <= GOLDEN_LEN; cut++) {
        SynthSettings out;
        PresetResult r = parse_with_sentinel(GOLDEN_DEFAULTS, cut, &out);
        if (cut + 1 >= GOLDEN_LEN) {
            CHECK(r == PRESET_OK);
            CHECK(r != PRESET_OK || is_golden_defaults(&out));
        } else {
            if (r != PRESET_E_TRUNCATED && r != PRESET_E_CORRUPT) {
                printf("%s:%d: cut %lu gave %d\n", __FILE__, __LINE__, (unsigned long)cut, (int)r);
            }
            CHECK(r == PRESET_E_TRUNCATED || r == PRESET_E_CORRUPT);
        }
    }
}

/* P10: unknown keys, duplicates, bad integers, bad names, clamping. */
static void test_p10_keys_and_values(void) {
    static const char *const BAD_SENS[] = {
        HEAD "sensitivity=abc\n" TAIL,
        HEAD "sensitivity=\n" TAIL,
        HEAD "sensitivity=99999999999\n" TAIL,
        HEAD "sensitivity=+5\n" TAIL,
        HEAD "sensitivity= 5\n" TAIL,
        HEAD "sensitivity=-\n" TAIL,
        HEAD "sensitivity=1234567890\n" TAIL,
        HEAD "sensitivity=-1234567890\n" TAIL,
    };
    SynthSettings out;

    expect_text("version=1\nfoo=bar\nwave=sine\nsnap=0\nroll_volume=1\nx=abc\n"
                "sensitivity=100\npitch_min=100\npitch_max=1000\nend=1\n",
                PRESET_OK);
    expect_text(HEAD "sensitivity=100\nsensitivity=100\n" TAIL, PRESET_E_CORRUPT);
    expect_text(HEAD "sensitivity=100\n" TAIL "pitch_min=100\n", PRESET_E_CORRUPT);
    for (size_t i = 0; i < sizeof BAD_SENS / sizeof BAD_SENS[0]; i++) {
        expect_text(BAD_SENS[i], PRESET_E_CORRUPT);
    }
    expect_text("version=1\nwave=foo\nsnap=0\nroll_volume=1\nsensitivity=100\n" TAIL,
                PRESET_E_CORRUPT);
    expect_text(HEAD "sensitivity=999999999\n" TAIL, PRESET_OK);

    expect_text("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=-5\n" TAIL, PRESET_OK);
    CHECK(parse_with_sentinel("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=-5\n" TAIL,
                              strlen("version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=-5\n" TAIL),
                              &out) == PRESET_OK);
    CHECK(out.sensitivity_pct == 25);

    CHECK(parse_with_sentinel("version=1\nwave=sine\nsnap=7\nroll_volume=-3\nsensitivity=100\n"
                              "pitch_min=900\npitch_max=100\nend=1\n",
                              strlen("version=1\nwave=sine\nsnap=7\nroll_volume=-3\nsensitivity=100\n"
                                     "pitch_min=900\npitch_max=100\nend=1\n"), &out) == PRESET_OK);
    CHECK(out.snap_default == 1);
    CHECK(out.roll_volume == 1);
    CHECK(out.pitch_min_hz == 900);
    CHECK(out.pitch_max_hz == 950);
}

/* P10 (names): every wave name maps to its WaveType; other spellings fail. */
static void test_p10_wave_names(void) {
    for (int w = 0; w < WAVE_COUNT; w++) {
        char text[256];
        SynthSettings out;
        size_t len;
        int n = snprintf(text, sizeof text,
                         "version=1\nwave=%s\nsnap=0\nroll_volume=1\nsensitivity=100\n" TAIL,
                         WAVE_NAME_TABLE[w]);
        CHECK(n > 0 && (size_t)n < sizeof text);
        len = (size_t)n;
        CHECK(parse_with_sentinel(text, len, &out) == PRESET_OK);
        CHECK(out.wave_default == w);
    }
    expect_text("version=1\nwave=Sine\n" BODY, PRESET_E_CORRUPT);
    expect_text("version=1\nwave=\n" BODY, PRESET_E_CORRUPT);
}

/* P11: every error leaves the caller's struct bit-identical. */
static void test_p11_errors_leave_out_untouched(void) {
    static char big[PRESET_MAX_BYTES + 1];
    static const char *const CASES[] = {
        "version=2\n" BODY,
        "wave=sine\n",
        "version=1\nsensitivity=100\npitch_min=100\npitch_max=1000\n",
        "version=1\nwave=sine\nsnap=0\nroll_volume=1\nsensitivity=abc\n" TAIL,
        "version=1\n" BODY "end=0\n",
        "version=1\n" BODY "snap=1\n",
        "version=1\nwave=sine\nsnap=0\n\x01 roll_volume=1\n" TAIL,
    };
    memset(big, 'a', sizeof big);
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        SynthSettings out;
        PresetResult r = parse_with_sentinel(CASES[i], strlen(CASES[i]), &out);
        CHECK(r != PRESET_OK);
    }
    CHECK(parse_with_sentinel(big, sizeof big, &(SynthSettings){0}) == PRESET_E_TOO_BIG);
}

/* Replaces a random byte, flips one bit, inserts a byte or deletes a byte. */
static size_t mutate_once(char *buf, size_t len, size_t cap, uint32_t *rng) {
    uint32_t op = xorshift32(rng);
    uint32_t bits = xorshift32(rng);
    size_t pos = len == 0 ? 0 : xorshift32(rng) % len;
    char value = (char)(bits & 0xFFu);
    switch (op % 3u) {
    case 0:
        if (len > 0) {
            buf[pos] = (char)(buf[pos] ^ (1 << (bits & 7u)));
        }
        return len;
    case 1:
        if (len + 1 > cap) {
            return len;
        }
        memmove(buf + pos + 1, buf + pos, len - pos);
        buf[pos] = value;
        return len + 1;
    default:
        if (len == 0) {
            return 0;
        }
        memmove(buf + pos, buf + pos + 1, len - pos - 1);
        return len - 1;
    }
}

/* P12: seeded random mutations never crash; accepted results are in bounds. */
static void test_p12_fuzz(void) {
    uint32_t rng = FUZZ_SEED;
    int out_of_bounds = 0;
    int accepted = 0;
    int rejected = 0;
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        char buf[MAX_TEXT];
        size_t len = golden_plus_bytes(buf, "", 0);
        uint32_t muts = 1u + xorshift32(&rng) % 3u;
        SynthSettings out;
        PresetResult r;
        for (uint32_t m = 0; m < muts; m++) {
            len = mutate_once(buf, len, sizeof buf, &rng);
        }
        r = preset_parse(buf, len, &out);
        if (r == PRESET_OK) {
            accepted++;
            if (!in_bounds(&out)) {
                out_of_bounds++;
            }
        } else {
            rejected++;
        }
    }
    CHECK(out_of_bounds == 0);
    CHECK(accepted > 0);
    CHECK(rejected > 0);
}

/* strerror: every code has a distinct, non-empty message. */
static void test_strerror_messages(void) {
    const PresetResult codes[] = {
        PRESET_OK, PRESET_E_NO_STORAGE, PRESET_E_NOT_FOUND, PRESET_E_IO, PRESET_E_TOO_BIG,
        PRESET_E_CORRUPT, PRESET_E_VERSION, PRESET_E_TRUNCATED, PRESET_E_BAD_SLOT
    };
    const size_t count = sizeof codes / sizeof codes[0];
    for (size_t i = 0; i < count; i++) {
        const char *msg = preset_strerror(codes[i]);
        CHECK(msg != NULL && msg[0] != '\0');
        for (size_t j = 0; j < i; j++) {
            CHECK(strcmp(msg, preset_strerror(codes[j])) != 0);
        }
    }
}

int main(void) {
    test_p1_serialize_golden();
    test_p2_capacity();
    test_p3_round_trip_grid();
    test_p4_size_boundary();
    test_p5_line_length();
    test_p6_byte_rules();
    test_p7_version();
    test_p8_end_marker();
    test_p9_every_cut();
    test_p10_keys_and_values();
    test_p10_wave_names();
    test_p11_errors_leave_out_untouched();
    test_p12_fuzz();
    test_strerror_messages();

    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
