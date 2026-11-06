/*
 * Preset file format: serialize and parse only. No file I/O lives here, so the
 * module builds and tests on the host.
 */
#ifndef PRESET_STORE_H
#define PRESET_STORE_H

#include <stddef.h>

#include "synth_core.h"

#define PRESET_DIR "sd:/apps/WiiMotionSynth"
#define PRESET_MAX_BYTES 1024
#define PRESET_LINE_MAX 80
#define PRESET_PATH_MAX 96

typedef enum {
    PRESET_OK,
    PRESET_E_NO_STORAGE,
    PRESET_E_NOT_FOUND,
    PRESET_E_IO,
    PRESET_E_TOO_BIG,
    PRESET_E_CORRUPT,
    PRESET_E_VERSION,
    PRESET_E_TRUNCATED,
    PRESET_E_BAD_SLOT
} PresetResult;

/* Static, human-readable message that names the problem and the fix. */
const char *preset_strerror(PresetResult r);

/* Writes the sanitized settings as LF-terminated key=value lines. Returns the
 * byte count, or 0 when cap is smaller than the text. No terminating NUL. */
size_t preset_serialize(const SynthSettings *s, char *buf, size_t cap);

/* Parses a preset file of len bytes. *out is written only on PRESET_OK. */
PresetResult preset_parse(const char *buf, size_t len, SynthSettings *out);

#endif /* PRESET_STORE_H */
