# Building

Needs a Linux x86-64 host with git, cmake, make, python3 with Pillow
(`python3-pil`, used by package.sh to shrink the menu pictures) and:

1. **riscos-crossdev toolchain 1.3** (`riscos-crossdev-toolchain-1.3-x86_64-linux.tar.xz`,
   sha256 bcdb35fb…). It links statically by default and its `elf2aif` has
   the fix for big images. It carries riscos-unixlib 5.0.3.2; until a
   riscos-crossdev release packages 5.0.3.3, copy **riscos-unixlib 5.0.3.3**'s
   release `libunixlib.a` (sha256 e24f16c3…) over the toolchain's (in a copy
   of the toolchain):
   ```
   cp -a riscos-crossdev-toolchain-1.3-x86_64-linux riscos-crossdev-toolchain-1.3-unixlib-5.0.3.3
   cp libunixlib.a riscos-crossdev-toolchain-1.3-unixlib-5.0.3.3/arm-riscos-gnueabihf/lib/libunixlib.a
   ```
   5.0.3.3's public headers are the same as 5.0.3.2's. `build.sh` stops if
   the toolchain's `libunixlib.a` isn't the one pinned in `build/env.sh`
   (`UNIXLIB_SHA256`). `patches/unixlib/unixlib-riscos.diff` is 5.0.3.3's
   change set.
2. **riscos-mesa devkit 20.3.5-14** (`riscos-mesa-devkit-20.3.5-14.tgz`,
   sha256 3c3c9bb0…; OSMesa, EGL, GLU; `bin/mkrozip.py`).

```
export RISCOS_TOOLCHAIN=/path/to/riscos-crossdev-toolchain-1.3-unixlib-5.0.3.3
export RISCOS_DEVKIT=/path/to/riscos-mesa-devkit-20.3.5-14
build/build.sh
build/package.sh m1-2
```

`build.sh` clones YSCE and ysce_public into `work/` (or `$WORK`), checks out
the pinned commits (`YSCE_REF`, `PUBLIC_REF` in `build/env.sh`; `=HEAD` to use
what's checked out), applies `patches/*/series` (skipping patches already
applied), builds `ysflight32_nownd` and `ysflight32_gl1` and checks stack
probes. About 5 minutes on 2 cores. Re-running is safe.

`package.sh` makes `dist/YSFlight-<version>.zip`: `app/!YSFlight`, the
upstream `runtime/` data and the console server as an Absolute file
(`elf2aif -e`). It stops if a build path is found in the app.

To compare with the OpenGL 2 (GLSL) game: `GL2=1 build/build.sh` also
builds `ysflight32_gl2`, and `build/package-gl2.sh VERSION` makes
`dist/YSFlight-gl2-<version>.zip` (RunImageGL2 and an Obey file RunGL2) to
unzip over an !YSFlight of the same version.

## Checks

- `tests/host-joystick/run.sh` (WORK as for the build; g++-multilib): the
  RISC OS joystick reader against a fake Joystick module (USBJoystick slot
  API, Joystick_Read only, no module).
- `tests/png-decode/run.sh` (WORK as for the build; needs zlib1g-dev,
  python3-pil, ImageMagick for the interlaced cases): the RISC OS PNG path
  (zlib, ysce_public patch 10) must decode every runtime PNG and ~200 test
  images byte for byte like the original decoder.
- `tests/host-sound/run.sh` (WORK as for the build): the RISC OS sound
  back end (ysce patch 14) against a fake `/dev/dsp` with a playback clock,
  using the game's WAV files.

- `tools/check-stack-probes.py ELF`: must report 0 unprobed frames.
- riscos-crossdev `checks/check-fpu.sh ELF`: no VFPv4 instructions.
- UnixLib's ticker block: 640 bytes (5.0.3.1 to 5.0.3.3; riscos-crossdev
  1.3's `check-unixlib.sh` expects 640).
- riscos-crossdev `checks/check-static.sh ELF`: statically linked.

## Window code test on Linux

`tests/host-window/run.sh` (RCD_DIR = a riscos-crossdev checkout, plus
RISCOS_DEVKIT and WORK as for the build; needs g++-multilib) builds the
window back end against riscos-crossdev's fake RISC OS with a scripted
Wimp, keyboard and pointer, and runs 61 checks. Note: on Ubuntu,
g++-multilib and g++-arm-linux-gnueabihf can't be installed together.

## Alignment test on Linux (QEMU)

RISC OS faults unaligned loads that Linux allows. To catch them, build the
console server for `arm-linux-gnueabihf` with the RISC OS CMake branches and
run it under riscos-crossdev's `qemu/aligntrap.sh`:

```
cmake <work>/YSCE/src -DRISCOS=1 -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DCMAKE_EXE_LINKER_FLAGS="-no-pie -L<stubs>"
# F = -O2 -g -mcpu=cortex-a8 -mfpu=vfpv3 -mfloat-abi=hard -mno-unaligned-access
#     -I$RISCOS_DEVKIT/include -I<riscos-crossdev>/fake
# <stubs> = empty libEGL.a libOSMesa.a libGLU.a libz.a (arm-linux-gnueabihf-ar rc)
make ysflight32_nownd
cd main_consvr && <riscos-crossdev>/qemu/aligntrap.sh ./ysflight32_nownd -server tester
```

Exit status 135 is an unaligned access; `QEMU_ARGS="-g 1234"` and
`gdb-multiarch` show where.

## Changing a patch

Edit the files in `work/YSCE` or `work/ysce_public`, then regenerate only
that patch: keep its text header and replace the diff with
`git diff <pinned commit> -- <that patch's files>` (new files need
`git add -N` first). No file is in two patches, so each patch's files are the
ones in its own diff. Add a CHANGELOG entry.
