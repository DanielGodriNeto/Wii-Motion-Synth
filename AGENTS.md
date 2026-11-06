# Wii Motion Synth Agent Guidelines

These instructions apply to AI agents and human contributors working in this
repository.

## Project Context

Wii Motion Synth is a Nintendo Wii homebrew application written in C. The
main source file, `main.c`, combines:

- `libogc` and GX for video initialization and the oscilloscope;
- `libwiiuse`/WPAD for Wiimote and Nunchuk input;
- `libasnd` for 48 kHz mono audio playback;
- a sine lookup table and integer phase accumulation for the audio callback.

Audio buffers and the lookup table are 32-byte aligned and flushed with
`DCFlushRange` before DMA use. Avoid floating-point work, allocation, or
blocking operations in `audio_callback`.

## Coding Standards

- Keep C code compatible with devkitPPC and the Wii's PowerPC 750/Gekko CPU.
- Preserve 32-byte alignment for DMA-visible buffers.
- Keep audio callback work bounded and real-time safe.
- Use the existing naming and `u32`/`s16`/`float` conventions from `main.c`.
- Keep all source code comments, user-facing text, metadata descriptions, and
  repository documentation in clear, technical English.
- Attribute code derived from external projects and preserve its license.
- Do not add secrets, machine-specific paths, or personal data.

English is the repository's documentation and communication language. Write
source comments, user-facing text, metadata descriptions, documentation,
release notes, and commit messages in clear, technical English.

## Build and Test

Install devkitPro with the `wii-dev` package, then configure `DEVKITPRO` and
`DEVKITPPC` in the environment. From the repository root:

```sh
make
```

The build produces `build/WiiMotionSynth.elf` and
`build/WiiMotionSynth.dol`. Remove generated output with:

```sh
make clean
```

Pure logic lives in `synth_core.c` and is covered by host tests: run
`make -C tests` (gcc and make only). Validate console changes with a clean
`make`. Hardware behavior must be checked on a Wii through the Homebrew
Channel when input, audio, video, or packaging behavior changes.

## Workflow Guidelines

1. Read `README.md`, `Makefile`, `.github/workflows/build.yml`, `meta.xml`, and
   the affected C code before editing.
2. Keep the repository's manual release workflow in mind: it builds inside
   `devkitpro/devkitppc:latest`, packages `boot.dol`, `meta.xml`, and
   `icon.png`, and publishes the rolling `latest` prerelease.
3. Use a focused change and preserve unrelated worktree modifications.
4. Update relevant documentation in the same change as code.
5. Run `make` when the required devkitPro toolchain is available and report
   toolchain limitations explicitly when it is not.

## Strict AI Co-Author Rule

Any time an AI agent modifies, creates, or refactors code, it MUST append itself
as a co-author in the git commit message using the standard GitHub co-author
format. If the AI agent does not know its official GitHub contributor routing
email, it must check the repository's commit history to find previous examples
of its own attribution, or search the web to find its correct official
co-author tag.

## Strict Documentation Sync Rule

Any time an AI agent changes, adds, or removes code, it MUST simultaneously
update the relevant documentation (README, comments, or wikis) to reflect
those changes.
