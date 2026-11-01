# Contributing

Thank you for contributing to Wii Motion Synth. Keep changes focused,
reproducible, and compatible with the Wii hardware.

## Prerequisites

- devkitPro with the `wii-dev` package installed.
- `DEVKITPRO` and `DEVKITPPC` configured in the build environment.
- A Wii and compatible Wiimote for runtime validation when hardware behavior
  changes.

See the [README build instructions](README.md#building-from-source) before
starting.

## Development Workflow

1. Create a focused branch from `main`.
2. Read `README.md`, `AGENTS.md`, `Makefile`, `meta.xml`, and the workflow
   before changing build, controls, audio, or packaging behavior.
3. Keep unrelated worktree changes out of the patch.
4. Update the relevant documentation in the same change as code.
5. Preserve the existing C style and real-time audio constraints.

## Build and Validation

Run a clean build from the repository root:

```sh
make clean
make
```

There is no automated unit-test suite. For changes affecting input, audio,
video, or the Homebrew Channel package, test the resulting
`build/WiiMotionSynth.dol` on real Wii hardware when available.

If the compiler reports that `wii.specs` cannot be found, verify the devkitPro
environment and `wii-dev` installation in the same shell used to run `make`.
Do not solve the issue by adding machine-specific paths to the Makefile.

## Commit Guidelines

- Use a concise, imperative subject, for example:
  `Document devkitPro troubleshooting`.
- Explain behavior changes in the commit body when the subject is not enough.
- Include only related files in each commit.
- AI agents must append themselves using the standard GitHub co-author trailer
  required by `AGENTS.md`.
- Do not include secrets, local machine paths, credentials, or generated build
  artifacts.

## Pull Requests

Describe the user-visible and hardware-visible behavior changed. Include the
commands used for validation and state whether real Wii hardware was tested.
For control or metadata changes, update both `README.md` and `meta.xml`.
