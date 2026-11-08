#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth_core.h"

static const int SCALE_FREQS[] = {
    131, 147, 165, 196, 220, 262, 294, 330, 392, 440, 523, 587, 659, 784, 880, 1047
};
#define NUM_NOTES (sizeof(SCALE_FREQS) / sizeof(SCALE_FREQS[0]))

#define ROLL_FIXED_VOLUME 204
#define VOLUME_MAX 255

static const char *const WAVE_NAMES[WAVE_COUNT] = {
    "sine", "square", "sawtooth", "triangle", "noise", "pulse", "narrow"
};

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int clamp_target_freq(int hz) {
    return clamp_int(hz, MIN_TARGET_FREQ_HZ, MAX_TARGET_FREQ_HZ);
}

static int wave_or_default(int wave) {
    return (wave >= 0 && wave < WAVE_COUNT) ? wave : WAVE_SINE;
}

static const char *on_off(int flag) {
    return flag != 0 ? "on" : "off";
}

static const int BUFFER_CHOICES[] = {256, 512, 1024, 2048};
#define BUFFER_CHOICE_COUNT (sizeof(BUFFER_CHOICES) / sizeof(BUFFER_CHOICES[0]))

/* Index of the choice nearest to samples. Ties keep the earlier, smaller one
 * because the scan only replaces the best on a strictly smaller distance. */
static size_t nearest_buffer_index(int samples) {
    size_t best = 0;
    for (size_t i = 1; i < BUFFER_CHOICE_COUNT; i++) {
        int d_best = abs(samples - BUFFER_CHOICES[best]);
        int d_here = abs(samples - BUFFER_CHOICES[i]);
        if (d_here < d_best) best = i;
    }
    return best;
}

/* Clamping first keeps the distance computation inside int range. */
static int snap_buffer_samples(int samples) {
    int clamped = clamp_int(samples, BUFFER_SAMPLES_MIN, BUFFER_SAMPLES_MAX);
    return BUFFER_CHOICES[nearest_buffer_index(clamped)];
}

/* One choice up (sign > 0) or down (sign < 0), stopping at both ends. */
static int step_buffer_samples(int samples, int sign) {
    size_t index = nearest_buffer_index(clamp_int(samples, BUFFER_SAMPLES_MIN, BUFFER_SAMPLES_MAX));
    if (sign > 0 && index + 1 < BUFFER_CHOICE_COUNT) index++;
    if (sign < 0 && index > 0) index--;
    return BUFFER_CHOICES[index];
}

void settings_defaults(SynthSettings *s) {
    s->sensitivity_pct = SENS_DEFAULT_PCT;
    s->pitch_min_hz = PITCH_DEFAULT_MIN_HZ;
    s->pitch_max_hz = PITCH_DEFAULT_MAX_HZ;
    s->wave_default = WAVE_SINE;
    s->snap_default = 0;
    s->roll_volume = 1;
    s->buffer_samples = BUFFER_SAMPLES_DEFAULT;
    s->master_volume_pct = MASTER_VOL_DEFAULT;
}

/* Minimum is clamped first so that min + PITCH_MIN_SPAN_HZ cannot overflow and
 * the maximum interval is never empty. Idempotent. */
void settings_sanitize(SynthSettings *s) {
    s->sensitivity_pct = clamp_int(s->sensitivity_pct, SENS_MIN_PCT, SENS_MAX_PCT);
    s->pitch_min_hz = clamp_int(s->pitch_min_hz, PITCH_FLOOR_HZ, PITCH_CEIL_HZ - PITCH_MIN_SPAN_HZ);
    s->pitch_max_hz = clamp_int(s->pitch_max_hz, s->pitch_min_hz + PITCH_MIN_SPAN_HZ, PITCH_CEIL_HZ);
    s->wave_default = wave_or_default(s->wave_default);
    s->snap_default = s->snap_default != 0;
    s->roll_volume = s->roll_volume != 0;
    s->buffer_samples = snap_buffer_samples(s->buffer_samples);
    s->master_volume_pct = clamp_int(s->master_volume_pct, MASTER_VOL_MIN_PCT, MASTER_VOL_MAX_PCT);
}

const char *settings_wave_name(int wave) {
    return WAVE_NAMES[wave_or_default(wave)];
}

bool synth_snap_active(const SynthSettings *s, bool b_held) {
    SynthSettings safe = *s;

    settings_sanitize(&safe);
    return b_held ? safe.snap_default == 0 : safe.snap_default != 0;
}

int synth_volume_for_roll(const SynthSettings *s, float roll) {
    SynthSettings safe = *s;

    settings_sanitize(&safe);
    if (safe.roll_volume == 0) return ROLL_FIXED_VOLUME;
    return map_roll_to_vol(roll);
}

/* Half-span is 90 degrees at 100 %: 9000 / sensitivity. Float division keeps
 * 22.5 degrees at 400 % (integer division would give 22). Operation order
 * matches the original mapping so default settings give identical integers. */
int settings_map_pitch_to_freq(float pitch, const SynthSettings *s) {
    SynthSettings safe = *s;
    float half;
    float t;

    settings_sanitize(&safe);
    if (!isfinite(pitch)) return safe.pitch_min_hz;

    half = 9000.0f / (float)safe.sensitivity_pct;
    if (pitch > half) pitch = half;
    if (pitch < -half) pitch = -half;

    t = (pitch + half) / (2.0f * half);
    return (int)((float)safe.pitch_min_hz + t * (float)(safe.pitch_max_hz - safe.pitch_min_hz));
}

int quantize_frequency(int raw_freq) {
    int closest_freq = SCALE_FREQS[0];
    int min_diff = 9999;
    for (size_t i = 0; i < NUM_NOTES; i++) {
        int diff = abs(raw_freq - SCALE_FREQS[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_freq = SCALE_FREQS[i];
        }
    }
    return closest_freq;
}

/* Order: map, quantize, octave shift, LFO, clamp. The LFO is clamped first so
 * that the addition cannot overflow for extreme inputs. */
int synth_target_freq(float pitch, bool snap, bool octave, int lfo_offset_hz, const SynthSettings *s) {
    int freq = settings_map_pitch_to_freq(pitch, s);
    int lfo = clamp_int(lfo_offset_hz, -MAX_TARGET_FREQ_HZ, MAX_TARGET_FREQ_HZ);

    if (snap) freq = quantize_frequency(freq);
    if (octave) freq *= 2;
    return clamp_target_freq(freq + lfo);
}

uint32_t freq_to_phase_step(int hz) {
    return (uint32_t)(((float)hz * SINE_LUT_SIZE / SAMPLERATE) * 65536.0f);
}

/* Clamping vol first bounds the product; the sanitized percent is 10..100, so
 * the result stays in 0..255 and is monotonic in both arguments. */
int synth_scaled_volume(int vol, const SynthSettings *s) {
    SynthSettings safe = *s;

    settings_sanitize(&safe);
    return clamp_int(vol, 0, VOLUME_MAX) * safe.master_volume_pct / 100;
}

/* Rounded to the nearest tenth: 512 samples is 10.67 ms and shows as 10.7.
 * Samples are clamped before the multiply so it cannot overflow. */
int synth_buffer_latency_tenths_ms(int samples) {
    int clamped;

    if (samples <= 0) return 0;
    clamped = clamp_int(samples, 1, BUFFER_SAMPLES_MAX);
    return (clamped * 10000 + SAMPLERATE / 2) / SAMPLERATE;
}

/* Non-finite roll mutes. Finite roll is clamped to +-1e6 so the integer
 * conversion below cannot overflow. */
int map_roll_to_vol(float roll) {
    int vol;

    if (!isfinite(roll)) return 0;
    if (roll > 1e6f) roll = 1e6f;
    if (roll < -1e6f) roll = -1e6f;

    vol = 255 - (abs((int)roll) * 255 / 180);
    return clamp_int(vol, 0, 255);
}

/* Works on a sanitized copy. dir is reduced to its sign, so dir 0 changes
 * nothing and any positive or negative magnitude moves one step. Wave wraps. */
void settings_field_adjust(SynthSettings *s, SettingsField field, int dir) {
    SynthSettings next = *s;
    int sign = (dir > 0) - (dir < 0);

    settings_sanitize(&next);
    switch (field) {
    case FIELD_SENS:
        next.sensitivity_pct += sign * SENS_STEP_PCT;
        break;
    case FIELD_PITCH_MIN:
        next.pitch_min_hz += sign * PITCH_STEP_HZ;
        break;
    case FIELD_PITCH_MAX:
        next.pitch_max_hz += sign * PITCH_STEP_HZ;
        break;
    case FIELD_WAVE:
        next.wave_default = (next.wave_default + sign + WAVE_COUNT) % WAVE_COUNT;
        break;
    case FIELD_SNAP:
        if (sign != 0) next.snap_default = 1 - next.snap_default;
        break;
    case FIELD_ROLL:
        if (sign != 0) next.roll_volume = 1 - next.roll_volume;
        break;
    case FIELD_BUFFER:
        next.buffer_samples = step_buffer_samples(next.buffer_samples, sign);
        break;
    case FIELD_MASTER_VOL:
        next.master_volume_pct += sign * MASTER_VOL_STEP_PCT;
        break;
    case FIELD_RESET:
        if (sign != 0) settings_defaults(&next);
        break;
    default:
        break;
    }
    settings_sanitize(&next);
    *s = next;
}

/* Shows the sanitized value, so the label matches what the synth uses. */
int settings_field_text(const SynthSettings *s, SettingsField field, char *buf, size_t cap) {
    SynthSettings shown = *s;

    settings_sanitize(&shown);
    switch (field) {
    case FIELD_SENS:
        return snprintf(buf, cap, "Sensitivity: %d%%", shown.sensitivity_pct);
    case FIELD_PITCH_MIN:
        return snprintf(buf, cap, "Pitch min: %d Hz", shown.pitch_min_hz);
    case FIELD_PITCH_MAX:
        return snprintf(buf, cap, "Pitch max: %d Hz", shown.pitch_max_hz);
    case FIELD_WAVE:
        return snprintf(buf, cap, "Wave: %s", settings_wave_name(shown.wave_default));
    case FIELD_SNAP:
        return snprintf(buf, cap, "Snap: %s", on_off(shown.snap_default));
    case FIELD_ROLL:
        return snprintf(buf, cap, "Roll volume: %s", on_off(shown.roll_volume));
    case FIELD_BUFFER: {
        int tenths = synth_buffer_latency_tenths_ms(shown.buffer_samples);
        return snprintf(buf, cap, "Audio buffer: %d samples (%d.%d ms)", shown.buffer_samples,
                        tenths / 10, tenths % 10);
    }
    case FIELD_MASTER_VOL:
        return snprintf(buf, cap, "Output volume: %d%%", shown.master_volume_pct);
    case FIELD_SLOT:
        return snprintf(buf, cap, "Preset slot");
    case FIELD_SAVE:
        return snprintf(buf, cap, "Save preset");
    case FIELD_LOAD:
        return snprintf(buf, cap, "Load preset");
    case FIELD_RESET:
        return snprintf(buf, cap, "Reset to defaults");
    case FIELD_BACK:
        return snprintf(buf, cap, "Back");
    default:
        if (cap > 0) buf[0] = '\0';
        return 0;
    }
}
