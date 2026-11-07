#!/bin/sh
# Static guard for startup silence: ASND_SetVoice starts playback, and the welcome
# screen only silences the synth later in the main loop. Until then g_vol_scale must
# never hold a nonzero value, or a beep is audible (issue: beep before welcome screen).
# Checks the declaration and every use of g_vol_scale in main() up to the first
# ASND_SetVoice call: each must be exactly a plain 'g_vol_scale = 0;' statement.
# Usage: sh check_startup_silence.sh <path/to/main.c>
set -u

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <source.c>" >&2
    exit 2
fi
src=$1
if [ ! -r "$src" ]; then
    echo "check_startup_silence: cannot read '$src'" >&2
    exit 2
fi

# Comments stripped (single-line only), carriage returns removed.
stripped=$(sed -e 's#//.*$##' -e 's#/\*.*\*/##' "$src" | tr -d '\r')
decl=$(printf '%s\n' "$stripped" | grep -E '^[[:space:]]*static.*g_vol_scale' || true)
code=$(printf '%s\n' "$stripped" | awk '/^int main\(/ { inside = 1 } inside { print } inside && /ASND_SetVoice\(/ { exit }')

if ! printf '%s\n' "$code" | grep -q 'ASND_SetVoice('; then
    echo "check_startup_silence: ASND_SetVoice not found in main() of '$src'" >&2
    exit 1
fi
if [ -z "$decl" ]; then
    echo "check_startup_silence: g_vol_scale declaration not found" >&2
    exit 1
fi

# Anything else (other values, +=, aliases, two statements on one line) is rejected.
bad=$(printf '%s\n%s\n' "$decl" "$code" | grep 'g_vol_scale' | grep -vE '^[[:space:]]*(static volatile u32 )?g_vol_scale[[:space:]]*=[[:space:]]*0u?;[[:space:]]*$' || true)
if [ -n "$bad" ]; then
    echo "check_startup_silence: g_vol_scale is not plain zero before the voice starts:" >&2
    printf '%s\n' "$bad" >&2
    exit 1
fi
echo "check_startup_silence: g_vol_scale stays 0 until ASND_SetVoice"
exit 0
