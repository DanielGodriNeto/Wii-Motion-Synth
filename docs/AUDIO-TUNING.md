# Audio tuning

Two settings in the settings menu control the audio output path. Both are
saved in presets (see [SETTINGS.md](SETTINGS.md) for the menu and the preset
slots).

| Menu row | Values | Step |
|----------|--------|------|
| Audio buffer | 256, 512, 1024, 2048 samples | one choice per press, stops at both ends |
| Output volume | 10 % to 100 % | 10 % per press |

## Audio buffer

The audio callback fills one buffer of samples per call at 48 kHz. The menu
shows the buffer size and the latency one buffer adds:

| Samples | Latency per buffer | Worst case (two buffers) |
|---------|--------------------|--------------------------|
| 256     | 5.3 ms             | about 10.7 ms            |
| 512     | 10.7 ms            | about 21.3 ms            |
| 1024    | 21.3 ms (default)  | about 42.7 ms            |
| 2048    | 42.7 ms            | about 85.3 ms            |

The latency is computed from the sample count and the 48 kHz rate, rounded to
the nearest 0.1 ms. The worst case is about two buffers because the audio
system plays one buffer while the next one is being filled.

Tradeoff:

- A smaller buffer gives a lower delay between a motion and the sound. The
  callback runs more often, so it takes more CPU time per second, and if the
  main loop is slow the next buffer can be late. A late buffer causes an
  underrun, which is heard as a click or a gap.
- A larger buffer is safer against slow frames and gives more time per
  callback, at the cost of more delay.

The default stays at 1024 samples, which is the size the app has always used.
It is the safe choice; 256 and 512 are for users who want a faster response
and accept the risk of clicks on a slow frame.

When the settings menu closes, a changed buffer size restarts the audio voice.
This causes a short silence. Changing the value inside the menu does not
restart the voice; the restart happens once, when the menu closes (or when a
preset is loaded or reset).

## Output volume

Output volume scales the final level after roll, the Nunchuk tremolo and the
mute gate. At 100 % the level is the same as before this setting existed. At
lower values the whole output is quieter by that percentage, which is useful
when the Wii is connected to a loud output.

## Presets

Two keys are added to the preset file, before `end=1`:

```
buffer_samples=1024
master_volume=100
```

- The file format version stays at `version=1`.
- Both keys are optional. A file saved by an older version has neither key
  and loads with the defaults (1024 samples, 100 %).
- Out-of-range values are fixed on load: the buffer size snaps to the nearest
  choice and the volume is clamped to 10..100.
- A key that appears twice, or a value that is not a plain integer, makes the
  file corrupt, as with the other keys.

## Not verified

- Nothing was measured on a Wii. The latency values are computed from the
  sample count, not measured on the hardware.
- Underrun and click behaviour at 256 and 512 samples is untested. Whether the
  main loop keeps up at these sizes depends on the frame timing, which was not
  measured.
- `main.c` was not built here because the devkitPPC and libogc toolchain is not
  installed. Only the host tests in `tests/` were run.
