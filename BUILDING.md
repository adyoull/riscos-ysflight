# Building

Needs a Linux x86-64 host with git, cmake, make, python3 and:

1. **riscos-crossdev toolchain 1.1** (`riscos-crossdev-toolchain-1.1-x86_64-linux.tar.xz`),
   with **riscos-unixlib 5.0.3.1** installed in it:
   - copy `libunixlib.a` from the riscos-unixlib v5.0.3.1 release over
     `<toolchain>/arm-riscos-gnueabihf/lib/libunixlib.a`;
   - the toolchain's UnixLib headers already match 5.0.3.1 (only a comment in
     `sched.h` differs), so they don't need replacing.
   The toolchain's `elf2aif` has the fix for big images.
2. **riscos-mesa devkit 20.3.5-10** (OSMesa, EGL, GLU; `bin/mkrozip.py`).

```
export RISCOS_TOOLCHAIN=/path/to/riscos-crossdev-toolchain-1.1-x86_64-linux
export RISCOS_DEVKIT=/path/to/riscos-mesa-devkit-20.3.5-10
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

## Checks

- `tools/check-stack-probes.py ELF`: must report 0 unprobed frames.
- riscos-crossdev `checks/check-fpu.sh ELF`: no VFPv4 instructions.
- UnixLib's ticker block: 640 bytes for 5.0.3.1 (riscos-crossdev 1.1's
  `check-unixlib.sh` still expects 472, from 5.0.1-5.0.3).

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
