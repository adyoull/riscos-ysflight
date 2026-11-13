#!/bin/bash
# The whole build, its checks and the Linux tests, as GitHub runs them in the
# riscos-crossdev image (.github/workflows; docs in BUILDING.md):
#   build/ci.sh [VERSION]       (default VERSION: ci)
# MIXED=1 makes the game a mixed program (a devkit with shared libraries,
# RISCOS_DEVKIT_ANY=1 for one that isn't the pinned release).
# Needs RCD_DIR (riscos-crossdev: checks/, fake/) and RISCOS_TOOLCHAIN, as
# the image sets them.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${RCD_DIR:?set RCD_DIR to riscos-crossdev (the image sets it)}"
. build/env.sh
build/fetch-devkit.sh
build/build.sh
for elf in main/ysflight32_gl1 main_consvr/ysflight32_nownd; do
  "$RCD_DIR/checks/check-all.sh" "$WORK/build/$elf"
done
# MIXED=1: everything the game needs is in the devkit's !MesaLibs
if [ -n "$MIXED" ]; then
  "$RCD_DIR/checks/check-mixed-package.sh" "$WORK/build/main/ysflight32_gl1" "$RISCOS_DEVKIT/riscos/!MesaLibs/lib"
fi
build/package.sh "${1:-ci}"
export RCD_DIR RISCOS_DEVKIT WORK
tests/host-window/run.sh
tests/host-joystick/run.sh
tests/host-sound/run.sh
tests/png-decode/run.sh
echo "ci: all passed"
