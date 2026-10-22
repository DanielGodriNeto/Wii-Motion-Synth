# Wii Motion Synth

A standalone Nintendo Wii homebrew application written in C that turns the Wiimote into a motion-controlled audio synthesizer. It leverages `libogc`, `libwiiuse`, and `libasnd` to map real-time accelerometer data to audio frequency and amplitude.

## Features
- **Real-time Synthesis:** Double-buffered, integer-math audio processing using Direct Memory Access (DMA) for zero latency and FPU safety.
- **Dual UI Modes:** Switch seamlessly between a real-time hardware-accelerated (GX) oscilloscope and a detailed CPU text statistics overlay.
- **Motion Controls:**
  * **Pitch:** Controls oscillator frequency.
  * **Roll:** Controls amplitude/volume.
  * **Nunchuk LFO:** Analog stick controls vibrato and tremolo.
- **Waveforms & Modifiers:** Toggle between Sine (Default), Square (Hold A), Sawtooth (Hold 1), Triangle (D-Pad Up), White Noise (D-Pad Down), Pulse (D-Pad Left), and Narrow Pulse (D-Pad Right). Hold B to snap frequencies to a C Major Pentatonic scale. Hold 2 for an Octave Shift, and hold PLUS for a Mute Gate.

## Installation (Real Hardware)
1. Format an SD card to FAT32.
2. Download the pre-compiled `WiiMotionSynth-latest.zip` from the [Releases page](https://github.com/CGdiego/WiiMotionSynth/releases).
3. Extract the `.zip` file directly to the root of your SD card.
4. Launch via the Homebrew Channel.

## Building from Source
Prerequisites: [devkitPro](https://devkitpro.org/) with the `wii-dev` package installed.

1. Clone the repository: `git clone https://github.com/CGdiego/WiiMotionSynth.git`
2. Navigate to the directory: `cd WiiMotionSynth`
3. Compile the project: `make`
4. The compiled executable will be located at `build/WiiMotionSynth.dol`.