/*
 * Pure synthesis and settings logic. No libogc or devkitPPC headers: this
 * module builds on the host so tests/ can exercise it without the Wii toolchain.
 */
#ifndef SYNTH_CORE_H
#define SYNTH_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAMPLERATE 48000
#define SINE_LUT_SIZE 2048

/* Safe range for the synth frequency in Hz, applied after quantization, octave
 * shift and Nunchuk LFO. Pitch can exceed the expected -90..90 degree range and
 * the LFO can push the value past its limits. The ceiling is 2100 Hz so the top
 * scale note (1047 Hz) under octave shift (2094 Hz) is not clipped. */
#define MIN_TARGET_FREQ_HZ 100
#define MAX_TARGET_FREQ_HZ 2100

/* Runtime settings bounds. Defaults reproduce the original fixed mapping
 * (100..1000 Hz over -90..90 degrees) at 100 % sensitivity. */
#define SENS_MIN_PCT 25
#define SENS_DEFAULT_PCT 100
#define SENS_MAX_PCT 400
#define SENS_STEP_PCT 25

#define PITCH_FLOOR_HZ 100
#define PITCH_CEIL_HZ 1050
#define PITCH_MIN_SPAN_HZ 50
#define PITCH_STEP_HZ 50
#define PITCH_DEFAULT_MIN_HZ 100
#define PITCH_DEFAULT_MAX_HZ 1000

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_SAWTOOTH,
    WAVE_TRIANGLE,
    WAVE_NOISE,
    WAVE_PULSE,
    WAVE_PULSE_NARROW,
    WAVE_COUNT
} WaveType;

/* Always pass through settings_sanitize before use; the mapping functions do
 * this on a copy, so a corrupt struct cannot escape the bounds. */
typedef struct {
    int sensitivity_pct;
    int pitch_min_hz;
    int pitch_max_hz;
} SynthSettings;

typedef enum {
    FIELD_SENS,
    FIELD_PITCH_MIN,
    FIELD_PITCH_MAX,
    FIELD_RESET,
    FIELD_BACK,
    FIELD_COUNT
} SettingsField;

void settings_defaults(SynthSettings *s);
void settings_sanitize(SynthSettings *s);

/* Maps pitch (degrees) to a frequency in Hz inside [pitch_min, pitch_max] of
 * the sanitized settings. Non-finite pitch returns pitch_min. */
int settings_map_pitch_to_freq(float pitch, const SynthSettings *s);

/* Maps roll (degrees) to volume 0..255. Non-finite roll returns 0. */
int map_roll_to_vol(float roll);

int quantize_frequency(int raw_freq);

/* Pitch -> synth frequency: map, optional snap, optional octave, LFO offset,
 * clamp to MIN..MAX_TARGET_FREQ_HZ. */
int synth_target_freq(float pitch, bool snap, bool octave, int lfo_offset_hz, const SynthSettings *s);

/* Frequency in Hz -> phase accumulator step for the audio callback. */
uint32_t freq_to_phase_step(int hz);

/* Moves one menu field by the sign of dir (+1 increase, -1 decrease, 0 no-op).
 * FIELD_RESET restores defaults when dir != 0. FIELD_BACK does nothing. */
void settings_field_adjust(SynthSettings *s, SettingsField field, int dir);

/* Writes the menu label for field into buf (NUL-terminated, never more than
 * cap bytes). Returns the full length the text needs, as snprintf does. */
int settings_field_text(const SynthSettings *s, SettingsField field, char *buf, size_t cap);

#endif /* SYNTH_CORE_H */
