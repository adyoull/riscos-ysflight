# Building

Needs a Linux x86-64 host with git, cmake, make, python3 with Pillow
(`python3-pil`, used by package.sh to shrink the menu pictures) and:

1. **riscos-crossdev 1.4 or later's toolchain**
   (`riscos-crossdev-toolchain-1.4-x86_64-linux.tar.xz`, sha256 17e83377…;
   riscos-crossdev 1.8.x uses the same one). It has riscos-unixlib 5.0.3.3
   built in, links statically by default, and its `elf2aif` has the fix for
   big images. `build.sh` stops if the toolchain's UnixLib is older than
   5.0.3.3. `patches/unixlib/unixlib-riscos.diff` is 5.0.3.3's change set.
   (From a toolchain tarball older than riscos-crossdev 1.8.2, run its
   `toolchain/relocate.sh` once after unpacking.)
2. **The riscos-mesa devkit `.riscos-deps` names** (now 20.3.5-17; OSMesa,
   EGL, GLU; `bin/mkrozip.py`). `build/fetch-devkit.sh` (run by `build.sh`)
   downloads it from riscos-mesa's GitHub release into `work/` and checks it
   against `build/SHA256SUMS`, unless `RISCOS_DEVKIT` already points at that
   version (its `VERSION` file).

```
export RISCOS_TOOLCHAIN=/path/to/riscos-crossdev-toolchain-1.4-x86_64-linux
build/build.sh
build/package.sh test
```

Or everything GitHub runs (build, riscos-crossdev's `checks/check-all.sh`
on both programs, package, the Linux tests), with `RCD_DIR` set to a
riscos-crossdev 1.8.2 checkout: `build/ci.sh [VERSION]`.

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

## Building on GitHub

`.github/workflows/` has riscos-crossdev 1.8.2's two workflows (its
`docs/CI.md`), run in the image `ghcr.io/adyoull/riscos-crossdev:<version>`
that the `riscos-crossdev` line of `.riscos-deps` chooses:

- `riscos-build.yml`: on every push and pull request, `build/ci.sh`; the
  zip (`dist/YSFlight-ci.zip`) is kept on the run's page.
- `riscos-deps-update.yml`: every day, if riscos-crossdev or riscos-mesa
  has a newer release than `.riscos-deps` names, moves the pin (and, for
  riscos-mesa, the devkit's line in `build/SHA256SUMS`, through
  `ci/riscos-deps-hook.sh`), builds with it and opens a pull request saying
  whether that passed. Nothing is merged or released by itself.
- `PACKAGES` (Ubuntu packages the image hasn't): `python3-pil` (package.sh),
  `g++-13-multilib` (the -m32 window and joystick tests), `zlib1g-dev` and
  `imagemagick` (the PNG test).
- On GitHub: Settings > Actions > General, "Workflow permissions: Read and
  write" and "Allow GitHub Actions to create and approve pull requests".

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

- riscos-crossdev's `checks/check-all.sh ELF` (unstripped; `build/ci.sh`
  runs it on both programs): statically linked, every frame over 4 KB
  probed, no VFPv4 instructions, UnixLib's ticker block 640 bytes.
- `tools/check-stack-probes.py ELF`: the same probe check on its own.

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
