#!/bin/sh
# Static guard for startup silence: ASND_SetVoice starts playback, and the welcome
# screen only silences the synth later in the main loop. Until then g_vol_scale must
# never hold a nonzero value, or a beep is audible (issue: beep before welcome screen).
# Checks the initializer and every assignment before the first ASND_SetVoice call.
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

# Code lines with comments stripped, up to and including the first ASND_SetVoice call.
code=$(sed -e 's#//.*$##' -e 's#/\*.*\*/##' "$src" | tr -d '\r' | awk '{ print } /ASND_SetVoice\(/ { exit }')

if ! printf '%s\n' "$code" | grep -q 'ASND_SetVoice('; then
    echo "check_startup_silence: ASND_SetVoice not found in '$src'" >&2
    exit 1
fi
if ! printf '%s\n' "$code" | grep -qE 'g_vol_scale[[:space:]]*='; then
    echo "check_startup_silence: no g_vol_scale initializer found" >&2
    exit 1
fi

bad=$(printf '%s\n' "$code" | grep -E 'g_vol_scale[[:space:]]*=' | grep -vE 'g_vol_scale[[:space:]]*=[[:space:]]*0u?[[:space:]]*;' || true)
if [ -n "$bad" ]; then
    echo "check_startup_silence: g_vol_scale is nonzero before the voice starts:" >&2
    printf '%s\n' "$bad" >&2
    exit 1
fi
echo "check_startup_silence: g_vol_scale stays 0 until ASND_SetVoice"
exit 0
