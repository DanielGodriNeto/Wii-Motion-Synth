# Wii Motion Synth

Wii Motion Synth is a standalone Nintendo Wii homebrew application written in
C. It turns Wiimote motion into a real-time audio synthesizer with selectable
waveforms and a GX oscilloscope.

## Features

- 48 kHz mono synthesis with double-buffered audio output through `libasnd`.
- Sine, square, sawtooth, triangle, white-noise, pulse, and narrow-pulse
  waveforms.
- Pitch mapped from Wiimote tilt and volume mapped from roll.
- Optional C-major pentatonic quantization.
- Nunchuk joystick vibrato and tremolo.
- GX waveform oscilloscope and a text statistics screen.
- DMA-safe, 32-byte-aligned buffers and lookup-table-based sine generation.

## Controls

| Input | Action |
| --- | --- |
| Tilt up/down | Adjust pitch |
| Roll left/right | Adjust volume |
| Hold `A` | Square wave |
| Hold `1` | Sawtooth wave |
| D-pad up | Triangle wave |
| D-pad down | White noise |
| D-pad left | Pulse wave |
| D-pad right | Narrow pulse |
| Hold `B` | Quantize pitch to the C-major pentatonic scale |
| Hold `2` | Octave shift |
| Hold `+` | Mute gate |
| `-` | Toggle oscilloscope/statistics UI |
| Nunchuk joystick | Vibrato and tremolo LFO |
| `HOME` | Exit |

When no waveform selector is held, the application uses the sine waveform.

## Installation on Real Hardware

1. Format an SD card as FAT32.
2. Download `WiiMotionSynth-latest.zip` from the
   [GitHub Releases page](https://github.com/Wiirizon/Wii-Motion-Synth/releases).
3. Extract the archive at the SD-card root. The resulting application directory
   is `apps/WiiMotionSynth/` and contains `boot.dol`, `meta.xml`, and
   `icon.png`.
4. Launch Wii Motion Synth through the Homebrew Channel.

The application expects a compatible Wiimote. A Nunchuk is optional; without
one, the Nunchuk LFO remains inactive.

## Building from Source

### Prerequisites

- [devkitPro](https://devkitpro.org/)
- The `wii-dev` package, including devkitPPC, libogc, wiiuse, and elf2dol
- `make`

Set `DEVKITPRO` to the devkitPro installation directory and `DEVKITPPC` to the
devkitPPC directory. The Makefile intentionally uses these environment
variables instead of machine-specific paths.

### Build

```sh
git clone https://github.com/Wiirizon/Wii-Motion-Synth.git
cd Wii-Motion-Synth
make
```

Output files:

- `build/WiiMotionSynth.elf`: linked development executable.
- `build/WiiMotionSynth.dol`: Wii executable for the Homebrew Channel.

Clean generated output with:

```sh
make clean
```

### Manual Packaging

To reproduce the release archive after building:

```sh
mkdir -p apps/WiiMotionSynth
cp build/WiiMotionSynth.dol apps/WiiMotionSynth/boot.dol
cp meta.xml apps/WiiMotionSynth/
cp icon.png apps/WiiMotionSynth/
zip -r WiiMotionSynth-latest.zip apps/
```

The GitHub Actions workflow performs these steps automatically when manually
dispatched. It builds in the `devkitpro/devkitppc:latest` container and
publishes a rolling `latest` prerelease.

## Architecture

The project is intentionally small:

- `main.c` initializes video, input, and audio; maps motion to synthesis
  parameters; renders the UI; and implements the audio callback.
- `Makefile` compiles `main.c` with devkitPPC and converts the ELF to a DOL.
- `meta.xml` supplies Homebrew Channel metadata and documents the controls.
- `.github/workflows/build.yml` defines the manual build-and-release workflow.
- `icon.png` is the Homebrew Channel icon.

The audio callback uses a phase accumulator and a 2048-entry sine lookup table.
The main loop updates `g_phase_step`, `g_vol_scale`, and the selected waveform;
the callback consumes those values while filling the alternating audio buffers.

## Development Notes

The Wii audio callback is real-time code. Do not introduce blocking calls,
dynamic allocation, or expensive floating-point calculations there. Keep
buffers aligned and flush cache ranges before passing data to DMA-backed audio
 APIs. Changes to controls must be reflected in this README and `meta.xml`.

There is currently no automated unit-test suite. A successful `make` is the
available build validation; input and audio changes also require testing on
real Wii hardware.

## License

This project is distributed under the MIT License. See [LICENSE](LICENSE).
