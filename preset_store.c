#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "preset_store.h"

/* ---- strerror ---------------------------------------------------------- */

const char *preset_strerror(PresetResult r) {
    switch (r) {
    case PRESET_OK:
        return "Preset OK";
    case PRESET_E_NO_STORAGE:
        return "SD card not found: insert a FAT32 card";
    case PRESET_E_NOT_FOUND:
        return "No preset in this slot";
    case PRESET_E_IO:
        return "SD read/write failed: try again, then check the card";
    case PRESET_E_TOO_BIG:
        return "Preset file is too big: re-save it from the app";
    case PRESET_E_CORRUPT:
        return "Preset file is corrupt: re-save it from the app";
    case PRESET_E_VERSION:
        return "Preset version not supported: update the app";
    case PRESET_E_TRUNCATED:
        return "Preset file is incomplete: re-save it from the app";
    case PRESET_E_BAD_SLOT:
        return "Bad preset slot: choose 1 to 4";
    default:
        return "Unknown preset error";
    }
}

/* ---- serialize --------------------------------------------------------- */

/* Counts every byte; writes only while the position is below cap, so the
 * final len tells the caller whether the text fit. */
typedef struct {
    char *data;
    size_t cap;
    size_t len;
} Writer;

static void put_char(Writer *w, char c) {
    if (w->len < w->cap) w->data[w->len] = c;
    w->len++;
}

static void put_str(Writer *w, const char *s) {
    while (*s != '\0') put_char(w, *s++);
}

static void put_uint(Writer *w, unsigned int v) {
    char digits[12];
    size_t n = 0;
    do {
        digits[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0) put_char(w, digits[--n]);
}

static void put_key_str(Writer *w, const char *key, const char *value) {
    put_str(w, key);
    put_char(w, '=');
    put_str(w, value);
    put_char(w, '\n');
}

/* Only called with sanitized, non-negative values. */
static void put_key_int(Writer *w, const char *key, int value) {
    put_str(w, key);
    put_char(w, '=');
    put_uint(w, (unsigned int)value);
    put_char(w, '\n');
}

size_t preset_serialize(const SynthSettings *s, char *buf, size_t cap) {
    SynthSettings safe = *s;
    Writer w = {buf, cap, 0};

    settings_sanitize(&safe);
    put_key_int(&w, "version", 1);
    put_key_str(&w, "wave", settings_wave_name(safe.wave_default));
    put_key_int(&w, "snap", safe.snap_default);
    put_key_int(&w, "roll_volume", safe.roll_volume);
    put_key_int(&w, "sensitivity", safe.sensitivity_pct);
    put_key_int(&w, "pitch_min", safe.pitch_min_hz);
    put_key_int(&w, "pitch_max", safe.pitch_max_hz);
    put_key_int(&w, "buffer_samples", safe.buffer_samples);
    put_key_int(&w, "master_volume", safe.master_volume_pct);
    put_key_int(&w, "end", 1);
    return w.len <= cap ? w.len : 0;
}

/* ---- parse: lexical helpers -------------------------------------------- */

typedef enum {
    KEY_VERSION,
    KEY_WAVE,
    KEY_SNAP,
    KEY_ROLL,
    KEY_SENS,
    KEY_PMIN,
    KEY_PMAX,
    KEY_BUF,
    KEY_MVOL,
    KEY_END,
    KEY_UNKNOWN
} KeyId;

typedef struct {
    const char *name;
    KeyId id;
} KeyEntry;

static const KeyEntry KEY_TABLE[] = {
    {"version", KEY_VERSION},
    {"wave", KEY_WAVE},
    {"snap", KEY_SNAP},
    {"roll_volume", KEY_ROLL},
    {"sensitivity", KEY_SENS},
    {"pitch_min", KEY_PMIN},
    {"pitch_max", KEY_PMAX},
    {"buffer_samples", KEY_BUF},
    {"master_volume", KEY_MVOL},
    {"end", KEY_END}
};

typedef struct {
    const char *key;
    size_t key_len;
    const char *value;
    size_t value_len;
} Pair;

typedef struct {
    SynthSettings values;
    unsigned int seen;   /* one bit per KeyId */
    bool have_version;
    bool ended;
} ParseState;

static bool span_is(const char *s, size_t n, const char *lit) {
    return strlen(lit) == n && memcmp(s, lit, n) == 0;
}

static KeyId classify_key(const char *s, size_t n) {
    for (size_t i = 0; i < sizeof KEY_TABLE / sizeof KEY_TABLE[0]; i++) {
        if (span_is(s, n, KEY_TABLE[i].name)) return KEY_TABLE[i].id;
    }
    return KEY_UNKNOWN;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* Optional '-', then 1..9 digits. No sign '+', no spaces, no empty value. */
static bool parse_strict_int(const char *s, size_t n, int *out) {
    size_t i = 0;
    bool neg = false;
    int value = 0;

    if (n > 0 && s[0] == '-') {
        neg = true;
        i = 1;
    }
    if (n - i < 1 || n - i > 9) return false;
    for (; i < n; i++) {
        if (!is_digit(s[i])) return false;
        value = value * 10 + (s[i] - '0');
    }
    *out = neg ? -value : value;
    return true;
}

static bool match_wave(const char *v, size_t n, int *out) {
    for (int w = 0; w < WAVE_COUNT; w++) {
        if (span_is(v, n, settings_wave_name(w))) {
            *out = w;
            return true;
        }
    }
    return false;
}

/* Range is not checked here; settings_sanitize clamps after parsing. */
static bool store_setting(SynthSettings *s, KeyId id, const char *v, size_t n) {
    int x = 0;

    if (id == KEY_WAVE) return match_wave(v, n, &s->wave_default);
    if (!parse_strict_int(v, n, &x)) return false;
    switch (id) {
    case KEY_SNAP:
        s->snap_default = x;
        return true;
    case KEY_ROLL:
        s->roll_volume = x;
        return true;
    case KEY_SENS:
        s->sensitivity_pct = x;
        return true;
    case KEY_PMIN:
        s->pitch_min_hz = x;
        return true;
    case KEY_PMAX:
        s->pitch_max_hz = x;
        return true;
    case KEY_BUF:
        s->buffer_samples = x;
        return true;
    case KEY_MVOL:
        s->master_volume_pct = x;
        return true;
    default:
        return false;
    }
}

/* ---- parse: line rules ------------------------------------------------- */

static PresetResult apply_version(ParseState *st, const char *v, size_t n) {
    int version = 0;

    if (st->have_version) return PRESET_E_CORRUPT;
    if (!parse_strict_int(v, n, &version)) return PRESET_E_CORRUPT;
    if (version != 1) return PRESET_E_VERSION;
    st->have_version = true;
    return PRESET_OK;
}

static PresetResult apply_end(ParseState *st, const char *v, size_t n) {
    if (!span_is(v, n, "1")) return PRESET_E_CORRUPT;
    st->ended = true;
    return PRESET_OK;
}

static PresetResult apply_setting(ParseState *st, KeyId id, const char *v, size_t n) {
    unsigned int bit = 1u << id;

    if (st->seen & bit) return PRESET_E_CORRUPT;
    if (!store_setting(&st->values, id, v, n)) return PRESET_E_CORRUPT;
    st->seen |= bit;
    return PRESET_OK;
}

static PresetResult apply_pair(ParseState *st, const Pair *p) {
    KeyId id = classify_key(p->key, p->key_len);

    if (st->ended) return PRESET_E_CORRUPT;
    if (!st->have_version && id != KEY_VERSION) return PRESET_E_CORRUPT;
    switch (id) {
    case KEY_VERSION:
        return apply_version(st, p->value, p->value_len);
    case KEY_END:
        return apply_end(st, p->value, p->value_len);
    case KEY_UNKNOWN:
        return PRESET_OK;
    default:
        return apply_setting(st, id, p->value, p->value_len);
    }
}

/* Splits at the first '='. A line without '=' or with an empty key is corrupt. */
static bool split_pair(const char *line, size_t n, Pair *p) {
    size_t i = 0;

    while (i < n && line[i] != '=') i++;
    if (i == n || i == 0) return false;
    p->key = line;
    p->key_len = i;
    p->value = line + i + 1;
    p->value_len = n - i - 1;
    return true;
}

/* Blank (empty, or only spaces and tabs) and '#' comment lines are skipped. */
static bool is_skippable(const char *line, size_t n) {
    if (n > 0 && line[0] == '#') return true;
    for (size_t i = 0; i < n; i++) {
        if (line[i] != ' ' && line[i] != '\t') return false;
    }
    return true;
}

static PresetResult parse_line(ParseState *st, const char *line, size_t n) {
    Pair p;

    if (n > PRESET_LINE_MAX) return PRESET_E_CORRUPT;
    if (is_skippable(line, n)) return PRESET_OK;
    if (!split_pair(line, n, &p)) return PRESET_E_CORRUPT;
    return apply_pair(st, &p);
}

/* Visits lines in order. A trailing '\r' before each '\n' is dropped. */
static PresetResult parse_lines(const char *buf, size_t len, ParseState *st) {
    size_t pos = 0;

    while (pos < len) {
        size_t eol = pos;
        size_t n;
        PresetResult r;

        while (eol < len && buf[eol] != '\n') eol++;
        n = eol - pos;
        if (n > 0 && buf[pos + n - 1] == '\r') n--;
        r = parse_line(st, buf + pos, n);
        if (r != PRESET_OK) return r;
        pos = eol + 1;
    }
    return PRESET_OK;
}

/* Rejects NUL, control bytes other than tab, CR and LF, and bytes above 0x7E. */
static PresetResult check_bytes(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];

        if (c == '\t' || c == '\n' || c == '\r') continue;
        if (c < 0x20u || c > 0x7Eu) return PRESET_E_CORRUPT;
    }
    return PRESET_OK;
}

PresetResult preset_parse(const char *buf, size_t len, SynthSettings *out) {
    ParseState st;
    PresetResult r;

    if (len > PRESET_MAX_BYTES) return PRESET_E_TOO_BIG;
    r = check_bytes(buf, len);
    if (r != PRESET_OK) return r;

    settings_defaults(&st.values);
    st.seen = 0u;
    st.have_version = false;
    st.ended = false;
    r = parse_lines(buf, len, &st);
    if (r != PRESET_OK) return r;
    if (!st.ended) return PRESET_E_TRUNCATED;

    settings_sanitize(&st.values);
    *out = st.values;
    return PRESET_OK;
}
