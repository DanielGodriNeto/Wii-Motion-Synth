# Changelog

All notable changes to Wii Motion Synth are documented here.

## [Unreleased]

### Added

- Settings menu (hold `+`, press `-`) with adjustable motion
  sensitivity (25-400 %) and pitch range (minimum 100-1000 Hz, maximum up to
  1050 Hz, at least 50 Hz apart). Defaults reproduce the previous mapping.
  Settings are kept in RAM only and reset on restart.
- Host unit tests (`make -C tests`) for the pure logic, now in
  `synth_core.c`, and a static guard for `audio_callback`.

### Changed

- Pitch beyond the sensitivity window now saturates at the configured
  maximum or minimum instead of extrapolating (at the default settings: tilt
  past 90 degrees no longer exceeds 1000 Hz before octave shift and LFO).
- Non-finite or extreme roll values now give volume 0 instead of an
  undefined float-to-int conversion.

### Fixed

- Tremolo from the Nunchuk could subtract more than the roll volume and wrap the
  volume to a huge unsigned value. Volume is now floored at 0.
- Clamped the synth frequency to 100-2100 Hz after quantization, octave shift,
  and Nunchuk LFO, so out-of-range or noisy pitch values can no longer produce
  an invalid phase step. Tilt maps to 100-1000 Hz; octave shift and the LFO can
  push the output up to the 2100 Hz ceiling. The displayed frequency uses the
  same clamped value.
- Non-finite pitch values now map to the 100 Hz floor instead of an undefined
  float-to-int conversion.
- White-noise output no longer calls `rand()` once per sample inside the
  audio callback. It now uses a xorshift32 generator whose state lives in
  `main.c`. The seed still comes from the clock at startup.

## [1.1.1] - 2026-11-01

### Changed

- Updated the Homebrew Channel developer metadata to `Wiirizon`.
- Bumped the application version metadata to `1.1.1`.

## [1.1.0] - 2026-10-22

### Added

- Triangle, white-noise, pulse, and narrow-pulse waveforms on the D-pad.
- Octave shift while holding `2`.
- Mute gate while holding `+`.
- Nunchuk joystick vibrato and tremolo controls.
- Random seed initialization for distinct white-noise output.

### Changed

- Updated the startup and statistics screens to document the expanded control
  scheme.
- Removed the non-functional Wiimote speaker behavior.
- Corrected Nunchuk LFO phase progression for the 60 FPS main loop.
- Synchronized `README.md` and `meta.xml` with the 1.1.0 controls.

## [1.0.1] - 2026-10-21

### Fixed

- Corrected Homebrew Channel metadata in `meta.xml`.

## [1.0.0] - 2026-10-21

### Added

- Motion-controlled synthesis using Wiimote pitch and roll.
- Sine, square, and sawtooth waveforms.
- C-major pentatonic pitch quantization with `B`.
- GX oscilloscope and text telemetry views toggled with `-`.
- Nunchuk LFO support.
- Double-buffered, DMA-backed audio output.

### Notes

- Initial release for Wii homebrew hardware.
