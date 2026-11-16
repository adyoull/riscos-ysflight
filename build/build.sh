#!/bin/bash
# Clone (if needed) YSCE and ysce_public at the pinned commits, apply the
# patch series and build for RISC OS:
#   $WORK/build/main_consvr/ysflight32_nownd   console server
#   $WORK/build/main/ysflight32_gl1            the game (window back end: M2)
# MIXED=1: the game is a mixed program (OpenGL, EGL and GLU from !MesaLibs);
# the console server stays static. Needs a devkit with lib/shared.
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
# Programs link statically; MIXED=1 then links the game again as a mixed
# program (-rdynamic, the devkit's lib/shared first). The link flags are
# given each time, so a later build without MIXED makes it static again.
# configure LDFLAGS [EXTRA_LIBS]: EXTRA_LIBS go at the end of every link line.
configure() {
  cmake ../YSCE/src -DCMAKE_TOOLCHAIN_FILE="$RYS_DIR/build/toolchain-riscos.cmake" \
    -DRISCOS_TOOLCHAIN="$RISCOS_TOOLCHAIN" -DRISCOS_DEVKIT="$RISCOS_DEVKIT" \
    -DCMAKE_BUILD_TYPE=Release -DRISCOS_EXTRA_FLAGS="$MAP" \
    -DCMAKE_EXE_LINKER_FLAGS="$1" \
    -DCMAKE_C_STANDARD_LIBRARIES="${2:-}" -DCMAKE_CXX_STANDARD_LIBRARIES="${2:-}" >/dev/null
}
configure "-static -L$RISCOS_DEVKIT/lib"
# GL2=1 also builds the OpenGL 2 (GLSL) game, for comparison (package-gl2.sh).
GAMES="ysflight32_gl1"; FILES="main_consvr/ysflight32_nownd main/ysflight32_gl1"
if [ -n "${GL2:-}" ]; then GAMES="$GAMES ysflight32_gl2"; FILES="$FILES main/ysflight32_gl2"; fi
if [ -n "$MIXED" ]; then
  STUBS=$(arm-riscos-gnueabihf-gcc -print-file-name=mixed-stubs)
  [ -d "$STUBS" ] || { echo "MIXED needs riscos-crossdev 1.11 or later (no mixed-stubs)" >&2; exit 1; }
  [ -f "$RISCOS_DEVKIT/lib/shared/libEGL.so" ] ||
    { echo "MIXED needs a devkit with shared libraries ($RISCOS_DEVKIT/lib/shared)" >&2; exit 1; }
  make -j"$JOBS" ysflight32_nownd
  # The GPU: with riscos-mesa 20.3.5-20 or later, -lMesaGPU (after OSMesa)
  # makes the program load libMesaGPU.so.1 from !MesaLibs. RISC3D (from
  # !RISC3D) works out how to use the machine's graphics hardware; without
  # it the program draws in software as before. MESA_GPU=0 leaves it out.
  GPULIB=
  if [ "${MESA_GPU:-1}" != 0 ] && [ -f "$RISCOS_DEVKIT/lib/shared/libMesaGPU.so" ]; then
    GPULIB=-lMesaGPU
  fi
  configure "-rdynamic -Wl,-rpath-link=$STUBS -L$RISCOS_DEVKIT/lib/shared -L$RISCOS_DEVKIT/lib" "$GPULIB"
  rm -f main/ysflight32_gl1 main/ysflight32_gl2     # link again with the new flags
  make -j"$JOBS" $GAMES
else
  rm -f main/ysflight32_gl1 main/ysflight32_gl2     # in case the last build was mixed
  make -j"$JOBS" ysflight32_nownd $GAMES
fi
for f in $FILES; do
  python3 "$RYS_DIR/tools/check-stack-probes.py" "$f"
done
echo "built in $WORK/build"
