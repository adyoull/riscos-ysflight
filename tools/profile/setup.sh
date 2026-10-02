#!/bin/bash
# Host build for profiling: riscos-mesa's patched Mesa (classic OSMesa) and
# the Linux gl1 game with the headless fsosmesawrapper.cpp window back end.
#
#   MESA_TGZ=<mesa-20.3.5.tar.gz, sha256 adabbe01...>  RISCOS_MESA=<riscos-mesa repo>
#   PROF=<work dir, default /tmp/ysprof>  WORK=<patched clones from build.sh>
#   tools/profile/setup.sh
#
# Needs meson 0.63 + mako (pip), ninja, bison, flex, valgrind, cmake and the
# Linux build packages in BUILDING.md.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
: "${MESA_TGZ:?}" "${RISCOS_MESA:?}" "${WORK:?}"
: "${PROF:=/tmp/ysprof}"
mkdir -p "$PROF" && cd "$PROF"
if [ ! -e mesa/build/src/mesa/drivers/osmesa/libOSMesa.so.8 ]; then
  rm -rf mesa && mkdir mesa && tar xzf "$MESA_TGZ" -C mesa --strip-components=1
  # the order of riscos-mesa's build/build-mesa.sh
  for p in $(sed -n 's/^PATCHES="\(.*\)"/\1/p' "$RISCOS_MESA/build/build-mesa.sh"); do
    patch -d mesa -p1 -s < "$RISCOS_MESA/patches/mesa/mesa-20.3.5-$p.patch"
  done
  (cd mesa && CFLAGS="-O2 -g" CXXFLAGS="-O2 -g" meson setup build -Dbuildtype=plain \
     -Dosmesa=classic -Ddri-drivers= -Dgallium-drivers= -Dvulkan-drivers= -Dglx=disabled \
     -Degl=disabled -Dgbm=disabled -Dplatforms= -Dgles1=disabled -Dgles2=disabled \
     -Dllvm=disabled -Dshared-glapi=enabled && ninja -C build)
fi
# libGL.so.1 -> libOSMesa, so the game's and GLU's gl* calls reach it
mkdir -p gl
for l in libGL.so libGL.so.1 libOSMesa.so.8; do ln -sf "$PROF/mesa/build/src/mesa/drivers/osmesa/libOSMesa.so.8" gl/$l; done
ln -sf "$PROF/mesa/build/src/mapi/shared-glapi/libglapi.so.0" gl/libglapi.so.0
rm -rf src && mkdir src && cp -a "$WORK/YSCE" "$WORK/ysce_public" src/
cp "$HERE/fsosmesawrapper.cpp" src/ysce_public/src/fssimplewindow/src/glx/fsglxwrapper.cpp
F="-O2 -g -fno-omit-frame-pointer -fno-delete-null-pointer-checks -I$PROF/mesa/include"
cmake -S src/YSCE/src -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS="$F" \
  -DCMAKE_CXX_FLAGS="$F" -DCMAKE_EXE_LINKER_FLAGS="-L$PROF/gl -Wl,-rpath,$PROF/gl" >/dev/null
make -C build -j"$(nproc)" ysflight64_gl1 >/dev/null
echo "built $PROF/build/main/ysflight64_gl1"
