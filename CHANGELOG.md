# Changelog

All notable changes to Wii Motion Synth are documented here.

## [Unreleased]

### Fixed

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
