# Changelog

## Unreleased: M2 test 11, A/B (2026-10-06)

- Two builds of the same source (test 10's), for riscos-mesa's A/B of its
  release candidate: **A** linked with the riscos-mesa 20.3.5-13 release,
  **B** with **20.3.5-14rc1** (NEON textures, blending and colour spans; a
  GLSL JIT for the OpenGL 2 add-on). B is a test build only, not for
  release. Each has a GL2 add-on.
- Notes: the runway profile frame isn't bit-reproducible on the host;
  compare pictures at 10000 ft.

## Unreleased: M2 test 10 (2026-10-05)

- **Linked with the riscos-mesa 20.3.5-12 release** (was its pre-release
  devkit 12f). Lit models are faster (Mesa no longer recomputes the
  material for every vertex unless its colour changes), fog is quicker
  again, and the EGL and overlay fixes from riscos-mesa's code audit come
  with it. On the host: 6-7% fewer instructions a frame than test 9, same
  picture.

## Unreleased: M2 test 9 (2026-10-05)

- **Built with the updated libraries:**
  - riscos-crossdev 1.3 (static-only toolchain) with **riscos-unixlib
    5.0.3.2** built in; `patches/unixlib/unixlib-riscos.diff` is now
    5.0.3.2's. Nothing has to be copied into the toolchain any more.
    PThreadTicker stays 0.03; the ticker block stays 640 bytes.
  - riscos-mesa devkit 12f (pre-release of 20.3.5-12): the integer fog of
    devkit 12d, Mesa's glPushMatrix flush fix, and the review fixes.
- Upstream YSCE and ysce_public are unchanged (still 231e711 / 1b13297).

## Unreleased: M2 test 8 (2026-10-04)

- The same game as test 7, linked with riscos-mesa devkit 12d (on the way
  to 20.3.5-12): Mesa's fog is now worked out in integers. On the host it
  saves 1-3% of a frame; the Pi should gain more (no floating point per
  pixel any more). For an A/B on the Pi against test 7.

## Unreleased: M2 test 7 (2026-10-03)

- **Joysticks** (ysce patch 13): through Richard Walker's USBJoystick
  module, 0.23 or later: up to 8 devices, every axis (X, Y, Z, RZ, RX, RY,
  slider, up to 6), the hat or D-pad as YSFlight's POV hat, and 32 buttons.
  With another joystick module, Joystick_Read's stick 0 (X, Y, buttons).
  Assign them in Option > Joystick/Key Assignments; calibration is saved in
  `<Choices$Write>.YSFlight.joycalib`. Host test `tests/host-joystick`.

## Unreleased: M2 test 6 (2026-10-03)

- **Faster image loading:** PNGs are decoded with zlib (from the
  riscos-mesa devkit) and unfiltered a row at a time instead of byte by
  byte (ysce_public patch 10). Same pixels, checked byte for byte by
  `tests/png-decode`; about 5 times fewer instructions.
- The menu pictures are shrunk to 1024 pixels wide when packaging (they are
  only shown stretched to the window). With the new decoder, loading one
  takes about 13 times less work than before.

## Unreleased: M2 test 5 (2026-10-02)

- **Faster drawing of the sky, ground and solid objects** with software
  OpenGL (ysce patch 12): the sky/ground background is fogged per vertex
  and drawn on Mesa's fast path, and blending (which YSFlight leaves on
  for everything) is off while opaque things are drawn. On the host, with
  riscos-mesa's renderer: 26% fewer instructions a frame on the runway,
  33% in the air, the same picture.
- Profiling: `PROF_SEED` gives every run the same weather, for exact A/B
  comparisons; host builds use `-DYS_SOFTWARE_GL`.

## Unreleased: M2 test 4 (2026-10-02)

- **Faster defaults on RISC OS** for a new configuration: particle smoke
  and clouds off, ground texture off (ysce patch 10). Measured on
  riscos-mesa's renderer: 2.6x fewer instructions per frame on the runway,
  3.6x in the air. An existing `flight.cfg` keeps its settings: untick
  "Draw Burning Smoke by Particles" (Option > Config, Graphics tab) and
  "Use Ground Texture" (OpenGL tab).
- Menu: the background picture is scaled once instead of on every redraw,
  and on RISC OS one picture is kept for the session instead of decoding a
  new 1920x990 PNG every 8 seconds (ysce patch 08).
- At most 100 main-loop passes a second, so screens that draw nothing don't
  take all the CPU (ysce_public patch 09).
- No more "MakeDecision ... INPOSITION" lines in the log (ysce patch 09).
- `tools/profile/`: host profiling with riscos-mesa's Mesa.
- OpenGL 2 (GLSL) game for comparison: `GL2=1 build/build.sh` and
  `build/package-gl2.sh` make an add-on zip (RunGL2 + RunImageGL2) to
  unzip over !YSFlight (ysce patch 11). About 20 times slower than the
  normal build on the host.

## Unreleased: M2 test 3 (2026-10-02)

- Timing: every 5 seconds the log gets a line with the time per loop spent
  polling, in the simulation, drawing (software OpenGL) and showing the
  frame (ysce_public patch 08).
- `YSFlight$Threads 0` (a commented line in `!Run`) runs the simulator's
  thread pool tasks in order, to compare (patch 02, now "threads").
- Pi results with test 2: 2.44 fps with a near full screen window and the
  default settings; 10.89 fps drawing at 640x480 (`YSFlight$Render`) with
  ground texture, transparency and solid clouds off.

## Unreleased: M2 test 2 (2026-10-02)

- **Crash at the opening demo fixed**: built with
  `-fno-delete-null-pointer-checks`. YSFlight's older classes (YsList and
  others) call methods on null pointers and test `this==NULL` inside; GCC 6
  and later delete those tests. Test 1 aborted at &004065C4 in
  `YsList<YsSwordPolygon>::Append` as soon as the demo started.
- Only the game writes to `YSFlight$Log`; the console server (and
  `ServerTests`, which unsets it) no longer overwrite the game's log.

## Unreleased: M2 test 1 (2026-10-02)

- **The game in a desktop window**: fssimplewindow back end for RISC OS
  (Wimp window, riscos-mesa EGL; ysce_public patch 06). Held keys by
  OS_Byte 121 with a tap latch, clicks with their own positions, wheel,
  full screen as a borderless window, EX0 EY0, `YSFlight$Size` and
  `YSFlight$Render`, Wimp_PollIdle sleeps, desktop shutdown handling.
- **GUI text** with YSFlight's bitmap fonts (ysce_public patch 07): the
  menus were blank with the null font.
- `!Run` starts the game; the console server is `Server`, its tests
  `ServerTests`. Messages go to `<Wimp$ScrapDir>.YSFlightLog`
  (`YSFlight$Log`, opened once for stdout and stderr).
- riscos-mesa devkit 20.3.5-11.
- Host test of the window code: `tests/host-window/run.sh`.

## Unreleased: M1 test 3 (2026-10-01)

- The console server exits after printing a list (-listairplane,
  -listfield, -liststartpos) instead of waiting in its menu for a key. In
  test 2 the list went to a file, so the menu couldn't be seen and the first
  step looked hung (ysce patch 07, now "main-entry").
- Tests shows each step's output in the task window, prints the time
  between steps, and keeps stderr in Logs.

## Unreleased: M1 test 2 (2026-10-01)

- Same layout as the other ports: patch series per upstream repo
  (`patches/ysce/`, `patches/ysce_public/`), `build/` scripts with pinned
  commits, `app/!YSFlight`, docs.
- The console server is an Absolute file (`elf2aif -e`), run in a 16 MB task
  window. Test 1 stopped with "Wimpslot not big enough to run ELF program".
- Crash report: a data abort or RISC OS error writes the RISC OS error and a
  backtrace to the Logs files (ysce_public patch 04, ysce patch 07).
- `!Run` loads PThreadTicker 0.03 (shipped), as the other ports do.
- Each test's stderr goes to its own Logs file (`2>`); `Do Run` so the
  `<YSFlight$Dir>` in the arguments is expanded.
- No build paths in the program (`-ffile-prefix-map`).
- Threads are back, as upstream (riscos-unixlib 5.0.3.1 runs them through
  Wimp_Poll; PThreadTicker 0.03): the single-thread patch is dropped.
  Kept: `#include <cstdio>` in ysthreadpool.cpp, and RunParallel no longer
  joins one thread too many.

## M1 test 1 (2026-10-01)

- First RISC OS build: console server and game, linked with riscos-unixlib
  5.0.3.1. Threads ran in order (dropped in test 2); paths in `<Choices$Write>.YSFlight`;
  URI_Dispatch; server list compiled out; null window, font, sound, joystick.
- Fixed unaligned float access in network packets and binary STL (found with
  the QEMU alignment trap).
