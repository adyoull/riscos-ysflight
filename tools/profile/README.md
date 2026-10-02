# Profiling the game's drawing on a Linux host

The Pi draws with riscos-mesa's patched classic Mesa (software OpenGL).
The same Mesa built for Linux gives the same code paths, so where the time
goes can be measured on the host. This is the method the Warzone 2100 port
used.

- `fsosmesawrapper.cpp`: a fssimplewindow back end without a window.
  - The game draws into an OSMesa buffer set up like the RISC OS EGL one:
    OSMESA_BGRA, 16-bit depth, the same initial GL state, GL_FASTEST.
  - It can play scripted keys.
  - Callgrind instruments only the measured frames.
  - `PROF_FIXEDDT` gives the game a fixed 50 ms step per frame, so a run
    under valgrind flies the same as a native one.
  - The last frame is written as a PPM, to check what was drawn.
  - It is not part of the RISC OS build.
- `setup.sh`: builds riscos-mesa's Mesa (classic OSMesa, the patches in
  riscos-mesa's order) and the Linux gl1 game with that back end.
  - It puts `libGL.so.1` → `libOSMesa.so.8` first on the library path, so the
    game's and GLU's `gl*` calls reach OSMesa.
- `run.sh NAME STARTPOS "KEY VALUE;..."`: a free flight (F-18E, AOMORI) with
  those `flight.cfg` lines, under callgrind. It prints the instructions per
  frame.
  - Instruction counts are the same from run to run; wall-clock time in a
    shared container is not.
  - `PROF_SEED` (default 1) seeds the random numbers before the first
    frame, so the weather and clouds are the same in every run and two
    builds can be compared exactly, picture included.
  - `setup.sh` builds with `-DYS_SOFTWARE_GL`, which turns on the RISC OS
    drawing changes (ysce patch 12) on the host.

```
MESA_TGZ=.../mesa-20.3.5.tar.gz RISCOS_MESA=.../riscos-mesa WORK=.../work tools/profile/setup.sh
tools/profile/run.sh base MISAWA_RW10 ""
tools/profile/run.sh nopart NORTH10000_01 "SMKPARTCL FALSE"
callgrind_annotate --inclusive=yes /tmp/ysprof/out/base-MISAWA_RW10.cg | less
```

The results so far are in `docs/PORTING-NOTES.md` under "Speed".
