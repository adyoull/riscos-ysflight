#!/bin/bash
# Host test of the RISC OS joystick reader (ysjoystick/src/riscos) against
# a fake Joystick module. Needs g++-multilib (-m32).
#   WORK=<patched clones> tests/host-joystick/run.sh
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
: "${WORK:?set WORK (the build work directory)}"
S="$WORK/YSCE/src/ysjoystick/src"
OUT=${OUT:-$(mktemp -d)}
g++ -m32 -std=c++11 -g -O1 -D__riscos__ -I"$HERE/fake" -I"$S" -o "$OUT/test_joystick" \
  "$HERE/test_joystick.cpp" "$S/riscos/ysjoyreader.cpp" "$S/ysjoyreadercommon.cpp"
"$OUT/test_joystick"
