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

/* Audio buffer size in samples per buffer. Only these four choices are valid;
 * any other value snaps to the nearest one. The default keeps the original
 * 1024-sample buffer. */
#define BUFFER_SAMPLES_MIN 256
#define BUFFER_SAMPLES_MAX 2048
#define BUFFER_SAMPLES_DEFAULT 1024

/* Output volume in percent of the synth level. 100 is the original level. */
#define MASTER_VOL_MIN_PCT 10
#define MASTER_VOL_MAX_PCT 100
#define MASTER_VOL_DEFAULT 100
#define MASTER_VOL_STEP_PCT 10

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
    int wave_default;   /* WaveType, 0..WAVE_COUNT-1 */
    int snap_default;   /* 0 or 1 */
    int roll_volume;    /* 0 or 1; 0 fixes the volume at 204 */
    int buffer_samples; /* one of BUFFER_SAMPLES_MIN..MAX doubling steps */
    int master_volume_pct; /* MASTER_VOL_MIN_PCT..MASTER_VOL_MAX_PCT */
} SynthSettings;

typedef enum {
    FIELD_SENS,
    FIELD_PITCH_MIN,
    FIELD_PITCH_MAX,
    FIELD_WAVE,
    FIELD_SNAP,
    FIELD_ROLL,
    FIELD_BUFFER,
    FIELD_MASTER_VOL,
    FIELD_SLOT,
    FIELD_SAVE,
    FIELD_LOAD,
    FIELD_RESET,
    FIELD_BACK,
    FIELD_COUNT
} SettingsField;

void settings_defaults(SynthSettings *s);
/* Clamps numbers into their ranges, maps an out-of-range wave to WAVE_SINE and
 * any nonzero flag to 1. Idempotent. */
void settings_sanitize(SynthSettings *s);

/* Lowercase name of a wave ("sine", ..., "narrow"); out-of-range maps to "sine". */
const char *settings_wave_name(int wave);

/* Snap is on when held XOR snap_default. */
bool synth_snap_active(const SynthSettings *s, bool b_held);

/* Volume 204 when roll_volume is 0 (any roll), else map_roll_to_vol(roll). */
int synth_volume_for_roll(const SynthSettings *s, float roll);

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

/* Volume 0..255 scaled by the sanitized master volume percent. vol is clamped
 * to 0..255 first. 100 % returns vol unchanged. */
int synth_scaled_volume(int vol, const SynthSettings *s);

/* Latency of one audio buffer in tenths of a millisecond, rounded to the
 * nearest tenth. Returns 0 for samples <= 0; larger values are clamped to
 * BUFFER_SAMPLES_MAX. */
int synth_buffer_latency_tenths_ms(int samples);

/* Moves one menu field by the sign of dir (+1 increase, -1 decrease, 0 no-op).
 * FIELD_RESET restores defaults when dir != 0. FIELD_BACK does nothing. */
void settings_field_adjust(SynthSettings *s, SettingsField field, int dir);

/* Writes the menu label for field into buf (NUL-terminated, never more than
 * cap bytes). Returns the full length the text needs, as snprintf does. */
int settings_field_text(const SynthSettings *s, SettingsField field, char *buf, size_t cap);

#endif /* SYNTH_CORE_H */
