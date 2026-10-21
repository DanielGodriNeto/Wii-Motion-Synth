# Wii Motion Synth

A standalone Nintendo Wii homebrew application written in C that turns the Wiimote into a motion-controlled audio synthesizer. It leverages `libogc`, `libwiiuse`, and `libasnd` to map real-time accelerometer data (Pitch and Roll) to audio frequency and amplitude.

## Features
* **Real-time Synthesis:** Double-buffered, integer-math audio processing using Direct Memory Access (DMA) for zero latency and FPU safety.
* **Motion Controls:**
  * **Pitch:** Controls oscillator frequency (100Hz to 1000Hz).
  * **Roll:** Controls amplitude/volume.
* **Waveforms:** Toggle between Sine (A), Square (B), and Sawtooth (1) waves dynamically.

## Building from Source
Prerequisites: [devkitPro](https://devkitpro.org/) with the `wii-dev` package installed.

1. Clone the repository: `git clone https://github.com/CGdiego/WiiMotionSynth.git`
2. Navigate to the directory: `cd WiiMotionSynth`
3. Compile the `.dol` file: `make`
4. The compiled executable will be located at `build/WiiMotionSynth.dol`.

## Installation (Real Hardware)
1. Format an SD card to FAT32.
2. Create the directory path: `SD:/apps/WiiMotionSynth/`.
3. Copy `build/WiiMotionSynth.dol` to that folder and rename it to `boot.dol`.
4. Copy `meta.xml` and `icon.png` into the same folder.
5. Launch via the Homebrew Channel.