#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth_core.h"

static const int SCALE_FREQS[] = {
    131, 147, 165, 196, 220, 262, 294, 330, 392, 440, 523, 587, 659, 784, 880, 1047
};
#define NUM_NOTES (sizeof(SCALE_FREQS) / sizeof(SCALE_FREQS[0]))

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int clamp_target_freq(int hz) {
    return clamp_int(hz, MIN_TARGET_FREQ_HZ, MAX_TARGET_FREQ_HZ);
}

void settings_defaults(SynthSettings *s) {
    s->sensitivity_pct = SENS_DEFAULT_PCT;
    s->pitch_min_hz = PITCH_DEFAULT_MIN_HZ;
    s->pitch_max_hz = PITCH_DEFAULT_MAX_HZ;
}

/* Minimum is clamped first so that min + PITCH_MIN_SPAN_HZ cannot overflow and
 * the maximum interval is never empty. Idempotent. */
void settings_sanitize(SynthSettings *s) {
    s->sensitivity_pct = clamp_int(s->sensitivity_pct, SENS_MIN_PCT, SENS_MAX_PCT);
    s->pitch_min_hz = clamp_int(s->pitch_min_hz, PITCH_FLOOR_HZ, PITCH_CEIL_HZ - PITCH_MIN_SPAN_HZ);
    s->pitch_max_hz = clamp_int(s->pitch_max_hz, s->pitch_min_hz + PITCH_MIN_SPAN_HZ, PITCH_CEIL_HZ);
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
 * nothing and any positive or negative magnitude moves one step. */
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
    case FIELD_RESET:
        return snprintf(buf, cap, "Reset to defaults");
    case FIELD_BACK:
        return snprintf(buf, cap, "Back");
    default:
        if (cap > 0) buf[0] = '\0';
        return 0;
    }
}
