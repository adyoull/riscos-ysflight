#!/bin/bash
# Clone (if needed) YSCE and ysce_public at the pinned commits, apply the
# patch series and build for RISC OS:
#   $WORK/build/main_consvr/ysflight32_nownd   console server
#   $WORK/build/main/ysflight32_gl1            the game (window back end: M2)
# Re-running is safe: applied patches are skipped and make carries on.
set -euo pipefail
. "$(dirname "$0")/env.sh"
# UnixLib 5.0.3.3 or later (riscos-crossdev 1.4 or later): its
# __pthread_held_wait isn't in 5.0.3.2 or earlier.
UL="$RISCOS_TOOLCHAIN/arm-riscos-gnueabihf/lib/libunixlib.a"
if ! arm-riscos-gnueabihf-nm "$UL" 2>/dev/null | grep " T __pthread_held_wait$" >/dev/null; then
  echo "$UL is older than UnixLib 5.0.3.3: use riscos-crossdev 1.4 or later (BUILDING.md)" >&2; exit 1
fi
"$RYS_DIR/build/fetch-devkit.sh"
mkdir -p "$WORK"; cd "$WORK"
[ -d YSCE ] || git clone -q https://github.com/YSCEDC/YSCE.git
[ -d ysce_public ] || git clone -q https://github.com/YSCEDC/ysce_public.git
use_ref YSCE "$YSCE_REF"; use_ref ysce_public "$PUBLIC_REF"
echo "YSCE:"; apply_series YSCE "$RYS_DIR/patches/ysce"
echo "ysce_public:"; apply_series ysce_public "$RYS_DIR/patches/ysce_public"
# -ffile-prefix-map: no build paths (__FILE__ in asserts) in shipped programs.
MAP="-ffile-prefix-map=$WORK/YSCE=YSCE -ffile-prefix-map=$WORK/ysce_public=ysce_public"
mkdir -p build; cd build
cmake ../YSCE/src -DCMAKE_TOOLCHAIN_FILE="$RYS_DIR/build/toolchain-riscos.cmake" \
  -DRISCOS_TOOLCHAIN="$RISCOS_TOOLCHAIN" -DRISCOS_DEVKIT="$RISCOS_DEVKIT" \
  -DCMAKE_BUILD_TYPE=Release -DRISCOS_EXTRA_FLAGS="$MAP" >/dev/null
# GL2=1 also builds the OpenGL 2 (GLSL) game, for comparison (package-gl2.sh).
TARGETS="ysflight32_nownd ysflight32_gl1"; FILES="main_consvr/ysflight32_nownd main/ysflight32_gl1"
if [ -n "${GL2:-}" ]; then TARGETS="$TARGETS ysflight32_gl2"; FILES="$FILES main/ysflight32_gl2"; fi
make -j"$JOBS" $TARGETS
for f in $FILES; do
  python3 "$RYS_DIR/tools/check-stack-probes.py" "$f"
done
echo "built in $WORK/build"
