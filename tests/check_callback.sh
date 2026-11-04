#!/bin/sh
# Static guard for audio_callback: the function body must not allocate, do I/O,
# take locks, call rand, touch the settings/preset/synth modules, or use
# floating point. Usage: sh check_callback.sh <path/to/main.c>
set -u

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <source.c>" >&2
    exit 2
fi

src=$1
if [ ! -r "$src" ]; then
    echo "check_callback: cannot read '$src'" >&2
    exit 2
fi

# Extract from the definition line up to the first column-0 closing brace.
body=$(awk '
    /^void audio_callback\(/ { inside = 1 }
    inside { print NR ": " $0 }
    inside && /^}/ { exit }
' "$src")

if [ -z "$body" ]; then
    echo "check_callback: audio_callback not found in '$src'" >&2
    exit 1
fi

# Whole identifiers (libc, libm, float types).
ident_tokens='malloc|calloc|realloc|free|memalign|fopen|fread|fwrite|fclose|fprintf|printf|puts|rand|srand|float|double|exp|log|pow|sin|cos|tan|sqrt|fabs|fmax|fmin|expf|logf|powf|sinf|cosf|tanf|sqrtf|fabsf|fmaxf|fminf'
# Identifier prefixes (start boundary only).
prefix_tokens='lock|mutex|sem_|pthread|settings_|preset_|synth_|g_settings|SynthSettings'
pattern='(^|[^A-Za-z0-9_])(('"$ident_tokens"')([^A-Za-z0-9_]|$)|('"$prefix_tokens"'))'
# Floating-point literals such as 0.5 or 1.0f contain no keyword to match.
float_literal='[0-9]\.[0-9]'

# Comments may mention numbers or words; only scan code.
offenders=$(printf '%s\n' "$body" | sed -e 's#//.*$##' -e 's#/\*.*\*/##' | grep -E "$pattern|$float_literal" || true)

if [ -n "$offenders" ]; then
    echo "check_callback: forbidden token(s) in audio_callback of '$src':" >&2
    printf '%s\n' "$offenders" >&2
    exit 1
fi

lines=$(printf '%s\n' "$body" | wc -l | tr -d ' ')
echo "check_callback: audio_callback clean ($lines lines scanned)"
exit 0
