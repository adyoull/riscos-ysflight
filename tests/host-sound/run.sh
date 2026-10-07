#!/bin/bash
# Host test of the RISC OS sound back end (platform/riscos/fsairsoundriscos.cpp)
# against a fake /dev/dsp, with the game's own WAV files.
#   WORK=<patched clones> tests/host-sound/run.sh
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
: "${WORK:?set WORK (the build work directory)}"
S="$WORK/YSCE/src"
OUT=${OUT:-$(mktemp -d)}
g++ -std=c++11 -g -O1 -DYS_RISCOS_SOUND_TEST -I"$S/common" -I"$S/sounddll/yswavfile" \
  -o "$OUT/test_sound" "$HERE/test_sound.cpp" "${SOUND_SRC:-$S/platform/riscos/fsairsoundriscos.cpp}" \
  "$S/sounddll/yswavfile/yswavfile.cpp"
cd "$WORK/YSCE/runtime"
# YsWavFile prints a few lines per file it loads; show only the results.
if "$OUT/test_sound" > "$OUT/log" 2>&1; then r=0; else r=1; fi
grep -E '^(FAIL|Sound:|  mix)|checks' "$OUT/log"
exit $r
