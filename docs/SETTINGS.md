# Settings

Wii Motion Synth has an in-app settings menu. Open or close it by pressing
`+` and `-` together on the Wiimote: hold `+`, then press `-` (it is not
available on the welcome screen). The synth is silent while the menu is open.
The `1` and `2` buttons are not used for this because holding `1` (sawtooth)
with `2` (octave shift) is an existing playing combination.

| Input | Action |
| --- | --- |
| D-pad up/down | Select a row |
| D-pad left/right | Decrease/increase the selected value |
| `A` | Confirm `Reset to defaults` or `Back` |
| Hold `+`, press `-` | Close the menu |

## Motion settings

| Setting | Range | Step | Default |
| --- | --- | --- | --- |
| Sensitivity | 25-400 % | 25 | 100 % |
| Pitch min | 100-1000 Hz | 50 | 100 Hz |
| Pitch max | (min + 50)-1050 Hz | 50 | 1000 Hz |

The defaults reproduce the original fixed mapping: -90 to +90 degrees of
tilt map linearly to 100-1000 Hz.

How the values are used:

- Sensitivity sets how much tilt covers the whole range. The window is
  +-(90 degrees * 100 / sensitivity): 200 % reaches the full range with +-45
  degrees, 50 % needs +-180 degrees (the top is then not reachable by tilt
  alone). Sensitivity applies to the pitch mapping only; roll-to-volume is
  unchanged.
- Tilt beyond the window saturates at the minimum or maximum. Before this
  change a tilt past 90 degrees extrapolated above 1000 Hz (up to the 2100 Hz
  clamp). The octave shift and the Nunchuk LFO are still applied afterwards,
  so the output can reach the 100-2100 Hz safe clamp as before.
- Pitch min and max keep at least 50 Hz between them. Raising the minimum
  past `max - 50` pushes the maximum up (or stops at 1000 Hz), so one change
  can move the other value.
- Snap-to-scale quantizes to the nearest note of the fixed C-major pentatonic
  table (131-1047 Hz). With a range outside that table, the snapped note can
  fall outside the configured range; the 100-2100 Hz clamp still applies.

## How the values stay bounded

Every settings value goes through `settings_sanitize` (in `synth_core.c`)
before use, and the mapping functions sanitize a private copy again, so a
corrupted settings struct cannot escape the bounds. Sensor input is also
bounded:

- A non-finite pitch (NaN, infinity) maps to the pitch minimum.
- A non-finite roll maps to volume 0; finite roll is clamped before the
  float-to-int conversion.
- The Nunchuk LFO offset is clamped before it is added.
- The final frequency is clamped to 100-2100 Hz before the phase step is
  computed.

## Real-time safety

The settings struct is read and written by the main loop only. The audio
callback never reads it. The callback consumes the already bounded
`g_phase_step`, `g_vol_scale`, and `g_current_wave`, each written as a single
aligned 32-bit store. `tests/check_callback.sh` fails the build checks if
`audio_callback` mentions allocation, file I/O, `rand()`, locks, floating
point, or the settings module.

## Persistence

Four preset slots are stored on the SD card as `sd:/apps/WiiMotionSynth/preset<1-4>.cfg`
(plain `key=value` text). In the settings menu choose a slot, then "Save preset"
or "Load preset". Slot 1 is loaded silently at startup; a missing card or file
keeps the defaults. The card is mounted on first use, never in the audio
callback.

Format: `version=1` first, `end=1` last, keys `sensitivity`, `pitch_min`, `pitch_max`,
`wave`, `snap`, `roll_volume` (see `preset_store.c`). The file is capped at 1024 bytes
and 80 characters per line. Numbers must be plain integers; out-of-range values
are clamped, unknown keys are ignored, duplicate or malformed keys make the file
corrupt and nothing is applied. Errors are shown for about 4 seconds.

Saving writes `preset<n>.cfg.tmp` and renames it over the final file (FAT
refuses to rename onto an existing file, so the old file is removed first and
the load falls back to the `.tmp` file if the power fails in between). Deleting
the app folder deletes the presets.


## Not verified

The host tests prove the arithmetic and bounds. In a user-reported Wii test,
the existing features and new settings/presets appeared to work. The exact
menu feel, exhaustive input combinations, and SD-card behavior across cards
have not been independently verified. FAT rename behavior and power-loss
recovery are tested against a fake file layer and host directory, not under
real hardware power-loss conditions.

The user also reported a brief beep before the welcome screen; it stops when
the welcome screen appears. Its cause is unknown and is tracked in
[issue #11](https://github.com/Wiirizon/Wii-Motion-Synth/issues/11).
