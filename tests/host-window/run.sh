#!/bin/bash
# Host test of the RISC OS window back end (fssimplewindow/src/riscos)
# against riscos-crossdev's fake RISC OS. Needs g++-multilib (-m32: RISC
# OS code keeps pointers in int registers).
#   tests/host-window/run.sh   (with RCD_DIR, RISCOS_DEVKIT and WORK set,
#                               after build/build.sh has patched WORK)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
: "${RCD_DIR:?set RCD_DIR to a riscos-crossdev checkout}"
: "${RISCOS_DEVKIT:?set RISCOS_DEVKIT}"
: "${WORK:?set WORK (the build work directory)}"
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT/rcdfake"
cp "$RCD_DIR"/fake/*.h "$RCD_DIR"/fake/riscos.c "$OUT/rcdfake/"
S="$WORK/ysce_public/src/fssimplewindow/src"
FLAGS="-m32 -g -O1 -funsigned-char -D__riscos__ -D__FAKE_RISCOS -I$HERE/fakeplus -I$OUT -I$OUT/rcdfake -I$S -I$RISCOS_DEVKIT/include"
gcc $FLAGS -c "$OUT/rcdfake/riscos.c" -o "$OUT/riscos.o"
g++ $FLAGS -std=c++11 -o "$OUT/test_window" "$HERE/test_window.cpp" \
  "$S/riscos/fsriscoswrapper.cpp" "$S/riscos/fsriscoskeymap.cpp" \
  "$S/fssimplewindowcommon.cpp" "$OUT/riscos.o"
"$OUT/test_window"
