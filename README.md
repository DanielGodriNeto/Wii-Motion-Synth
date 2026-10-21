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
2. Download the pre-compiled `WiiMotionSynth-latest.zip` from the [Releases page](https://github.com/CGdiego/Wii-Motion-Synth/releases).
3. Extract the `.zip` file directly to the root of your SD card.
4. Insert the SD card into your Wii and launch **Wii Motion Synth** via the Homebrew Channel.
