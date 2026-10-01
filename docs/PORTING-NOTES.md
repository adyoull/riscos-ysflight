# Porting notes

YSCE `231e711` (2026-02-08) and ysce_public `1b13297` (2025-05-19), built with
GCCSDK GCC 10 (riscos-crossdev 1.1), riscos-unixlib 5.0.3.1 and the
riscos-mesa devkit 20.3.5-10.

## Patches

| Patch | Files | Why | Upstream? |
|---|---|---|---|
| ysce 01 missing-includes | core/fshud2.cpp, gui/fsguinetdialog.h | `std::string` without `<string>` | yes |
| ysce 02 unaligned-network-floats | util/fsnetutil.h | unaligned float store in every packet: abort on ARM | yes |
| ysce 03 riscos-cmake | CMakeLists.txt, main/, ysjoystick/ | RISC OS targets and libraries | |
| ysce 04 optional-server-list | gui/CMakeLists.txt, gui/fsguinetdialog.cpp | no curl/nlohmann-json; `find_package(CURL)` without CONFIG | the CONFIG part |
| ysce 05 riscos-paths | filename/fsfilename.cpp | user files in `<Choices$Write>.YSFlight` | |
| ysce 06 riscos-platform | platform/riscos/ | platform library without X11/dlopen | |
| ysce 07 riscos-error-report | main/fsmain.cpp, main_consvr/fsmainsvr.cpp | install the crash report | |
| ysce_public 01 unaligned-stl | ysclass/src/ysshellstl.cpp | unaligned float load in binary STL | yes |
| ysce_public 02 thread-fixes | ysclass11/src/ysthreadpool.cpp, ysport/src/ysthread.cpp | missing `<cstdio>`; `RunParallel` joined one thread too many | yes |
| ysce_public 03 riscos-cmake | fssimplewindow, yssimplesound, yssystemfont, ysgl CMakeLists | RISC OS back ends; no GLSL host tools when cross-compiling | |
| ysce_public 04 riscos-ysport | ysport/ | paths, URI_Dispatch, crash report | |
| ysce_public 05 nownd-console | fssimplewindow/src/nownd/fssimplenowindow.cpp | keys via OS_Byte 129; missing `FsCheckWindowOpen` | the fix |

## Build set-up

- The toolchain file keeps `CMAKE_SYSTEM_NAME Linux` (upstream's UNIX
  branches mostly suit RISC OS) and sets `RISCOS=1`; every difference is under
  `if(RISCOS)` in CMake and `__riscos__` in C++.
- CMake's Linux platform adds `-rdynamic`, which GCCSDK GCC rejects.
  `riscos-rules.cmake` (CMAKE_USER_MAKE_RULES_OVERRIDE) clears it; setting
  policy CMP0065's default doesn't work, as the project asks for CMake 3.2.
- Flags: `-mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection`, plus
  `-ffile-prefix-map` so no build paths end up in the programs.
- Programs ship as Absolute files (`elf2aif -e`); `!Run` must give a slot that
  holds the whole image (~6 MB for the console server). The first Pi test
  stopped with "Wimpslot not big enough to run ELF program" in a 1 MB task
  window.

## UnixLib

- Linked with riscos-unixlib 5.0.3.1 (`patches/unixlib/unixlib-riscos.diff`
  is its change set). Heap in dynamic areas past 128 MB; ticker block 640 bytes.
- Threads run as upstream (the owner's call, 2026-10-01): riscos-unixlib
  5.0.3.1 keeps switching threads through Wimp_Poll, so the simulation's
  thread pool (4 threads, handed work every frame) and the drawing buffer's
  per-load threads work. UnixLib's threads share one core, so they don't
  make it faster. PThreadTicker 0.03 ships and `!Run` loads it. Test 1 had a
  patch that ran everything on one thread; it was dropped.
- Crash report (ysce_public 04): SIGEMT/SIGOSERROR print the RISC OS error and
  `__write_backtrace` to stderr, then `_exit(2)`, as riscos-openttd's patch 15.

## Still null (later milestones)

- M2: the window. `fssimplewindow` is the console stand-in; the game links
  but can't run. Wimp/EGL back end in `fssimplewindow/src/riscos/`; held keys
  by OS_Byte 121 scans (the Wimp has no key-up events).
- M3: sound (link the plug-ins in; SDL2 audio) and the system font (the GUI
  draws all text with it; FontManager back end).
- Joystick: null. Server list: compiled out.
