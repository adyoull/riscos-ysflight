# Porting notes

YSCE `231e711` (2026-02-08) and ysce_public `1b13297` (2025-05-19), built with
GCCSDK GCC 10 (riscos-crossdev 1.1), riscos-unixlib 5.0.3.1 and the
riscos-mesa devkit 20.3.5-11.

## Patches

| Patch | Files | Why | Upstream? |
|---|---|---|---|
| ysce 01 missing-includes | core/fshud2.cpp, gui/fsguinetdialog.h | `std::string` without `<string>` | yes |
| ysce 02 unaligned-network-floats | util/fsnetutil.h | unaligned float store in every packet: abort on ARM | yes |
| ysce 03 riscos-cmake | CMakeLists.txt, main/, ysjoystick/ | RISC OS targets and libraries | |
| ysce 04 optional-server-list | gui/CMakeLists.txt, gui/fsguinetdialog.cpp | no curl/nlohmann-json; `find_package(CURL)` without CONFIG | the CONFIG part |
| ysce 05 riscos-paths | filename/fsfilename.cpp | user files in `<Choices$Write>.YSFlight` | |
| ysce 06 riscos-platform | platform/riscos/ | platform library without X11/dlopen | |
| ysce 07 main-entry | main/fsmain.cpp, main_consvr/fsmainsvr.cpp | install the crash report; console lists exit instead of waiting in the menu | the list part |
| ysce 08 riscos-title-picture | graphics/gl/fsopengl.cpp, ui/fsrunloop.cpp | menu picture scaled once; on RISC OS one picture per session (a PNG decode is ~1 s on a Pi) | the scaling part |
| ysce 09 quiet-formation-autopilot | autopilot/fsformationautopilot.cpp | two debug printf calls on every step | yes |
| ysce 10 riscos-graphics-defaults | config/fsconfig.cpp | particle smoke/clouds and ground texture off in a new RISC OS config (speed, below) | |
| ysce 11 riscos-gl2-target | main/CMakeLists.txt | optional OpenGL 2 (GLSL) game, to compare (GL2=1) | |
| ysce 12 riscos-software-gl-drawing | graphics/gl/fsgroundskygl.cpp, fsopengl.*, core/fssimulation.cpp, scenery/ysscenerygl.cpp (+ no-ops in gl2/null/d3d9) | sky/ground drawn with per-vertex fog and no blend; blending off for the opaque passes (speed, below) | maybe (as an option) |
| ysce_public 01 unaligned-stl | ysclass/src/ysshellstl.cpp | unaligned float load in binary STL | yes |
| ysce_public 02 thread-fixes | ysclass11/src/ysthreadpool.cpp, ysport/src/ysthread.cpp | missing `<cstdio>`; `RunParallel` joined one thread too many | yes |
| ysce_public 03 riscos-cmake | yssimplesound, ysgl CMakeLists | silent sound for now; no GLSL host tools when cross-compiling | |
| ysce_public 04 riscos-ysport | ysport/ | paths, URI_Dispatch, crash report | |
| ysce_public 05 nownd-console | fssimplewindow/src/nownd/fssimplenowindow.cpp | keys via OS_Byte 129; missing `FsCheckWindowOpen` | the fix |
| ysce_public 06 riscos-window | fssimplewindow/src/riscos/, its CMakeLists | the game window: Wimp + riscos-mesa EGL (below) | |
| ysce_public 07 riscos-bitmap-font | yssystemfont/src/riscos/, its CMakeLists | GUI text with YSFlight's bitmap fonts until a FontManager version | |
| ysce_public 08 riscos-timing | fslazywindow/src/linux_win/fslazywindow.cpp | time per loop (poll, simulation, draw, swap, sleep) in the log every 5 s | |
| ysce_public 09 riscos-loop-cap | fslazywindow/src/linux_win/fslazywindow.cpp | at most 100 loop passes a second (a screen drawing nothing took all the CPU) | |

## Build set-up

- The toolchain file keeps `CMAKE_SYSTEM_NAME Linux` (upstream's UNIX
  branches mostly suit RISC OS) and sets `RISCOS=1`; every difference is under
  `if(RISCOS)` in CMake and `__riscos__` in C++.
- CMake's Linux platform adds `-rdynamic`, which GCCSDK GCC rejects.
  `riscos-rules.cmake` (CMAKE_USER_MAKE_RULES_OVERRIDE) clears it; setting
  policy CMP0065's default doesn't work, as the project asks for CMake 3.2.
- Flags: `-mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection`, plus
  `-ffile-prefix-map` so no build paths end up in the programs, and
  **`-fno-delete-null-pointer-checks`**: YSFlight's older classes (YsList etc.,
  32 places) call methods on null pointers and test `this==NULL`; GCC 6+
  deletes those tests (M2 test 1 crashed in `YsList::Append`). Worth telling
  upstream: their GCC/Clang Linux builds are exposed too.
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

## The game window (ysce_public 06)

`fssimplewindow/src/riscos/fsriscoswrapper.cpp`; its header comment lists the
design. Taken from riscos-mesa's freeglut back end and the other ports:

- **One Wimp task, one window**; `FsPollDevice` empties the Wimp queue to the
  first null event every frame (at most 64 events). Redraws go to
  `eglRedrawWindowRISCOS`, which replots the last frame.
- **`FsSleep` = `Wimp_PollIdle`**: UnixLib's sleeps busy-wait in a Wimp task
  and would stop the desktop. Waits under 5 ms: one poll, then the clock.
- **Keys:** the Wimp sends no key-ups. Held keys come from an OS_Byte 121
  scan (only while the window has the caret); a Key_Pressed also counts as
  held for that one poll, so taps between two frames aren't lost. The
  physical key of a Key_Pressed is the key the scan finds newly down
  (internal key numbers from SDL 2.26's `scancodes_riscos.h`); if none
  (auto-repeat, software), the Wimp code is mapped. Ctrl/Alt: no character.
  Unused keys go back to the Wimp (`Wimp_ProcessKey`), so F12 etc. work.
- **Mouse:** presses from Mouse_Click at the click's own position, releases
  and movement from Wimp_GetPointerInfo, the wheel from OS_Pointer 2
  (Scroll_Request isn't sent on the Pi). Select/Menu/Adjust = left/middle/right.
- **Full screen** = a borderless window the size of the screen (no mode
  change; the program stays a Wimp task). Message_ModeChange reopens it.
- **Sizes:** pixels for the program, OS units for the Wimp (EX0 EY0 modes).
  `YSFlight$Size` sets the window size, `YSFlight$Render` the size drawn at
  (EGL_RENDER_*_RISCOS: stretched to the window; the program sees that size
  and the mouse is scaled to it). Overlays (`EGL$Overlay on`) are checked
  every poll.
- **Quitting:** Message_Quit exits; Message_PreQuit is objected to and treated
  as the close icon, and a desktop shutdown is restarted with Ctrl-Shift-F12
  (`Wimp_ProcessKey &1FC`) if the program closes within 30 s. The close icon
  asks the program (`fsCloseWindowCallBack`), as on Windows.
- **Started in a TaskWindow:** refused with a message (`*WimpTask`).
- `GL_PERSPECTIVE_CORRECTION_HINT` = `GL_FASTEST` (riscos-mesa's fast texture
  path); EGL config asks for `EGL_OPENGL_BIT` (EGL's default is ES).
- `FsSubSecondTimer` = `CLOCK_MONOTONIC` (HAL counter in UnixLib 5.0.3.1).
- Output: `!Run` sets `YSFlight$Log`; `YsRiscOsInstallErrorReport` opens it
  once and `dup2`s it onto stdout and stderr (a file can't be opened for
  writing twice; printing from a Wimp task would open a command window).
- **Host test:** `tests/host-window/run.sh` (riscos-crossdev's fake RISC OS,
  scripted Wimp/keyboard/pointer, stub EGL/GL): 61 checks; breaking the tap
  latch, the render scaling, the click position or the shutdown restart each
  fails it.

## Still null (later milestones)

- Sound (link the plug-ins in; SDL2 audio) and a FontManager system font
  (the bitmap-font stand-in is Latin-1 only).
- Loading a flight doesn't poll the Wimp, so the desktop pauses while a
  map loads (OpenTTD/Warzone lesson: poll every 50 ms during loading).
- Joystick: null. Server list: compiled out.

## Speed (M2 test 4)

Profiled like Warzone 2100: the game built for Linux with a headless
fssimplewindow on riscos-mesa's own patched Mesa (OSMesa, BGRA, 16-bit
depth, GL_FASTEST), instructions counted with callgrind (`tools/profile/`).
Per frame at 640x480, F-18E, Aomori field:

| Settings | Misawa runway | 10000 ft |
|---|---|---|
| upstream defaults | 603M | 527M |
| particles off ("Draw Burning Smoke by Particles") | 358M | 192M |
| and shadows off | 343M | 186M |
| and ground texture off | 228M | 147M |
| Pi test 2 log, owner's settings | draw 61-111 ms | |

- The particle clouds (many overlapping blended sprites) were 40-64% of the
  frame. With particles off the clouds are drawn as solid shapes.
- Textures already meet riscos-mesa's fast-path checklist (RGBA8, power of
  two, GL_REPEAT and equal filters at bind time, GL_MODULATE, one unit, no
  separate specular, GL_FASTEST): `persp_textured_triangle` is used.
- What's left is mostly pixels: the fogged sky/ground cylinder
  (FsGroundSky::DrawByFog, untextured, `_swrast_fog_rgba_span` 16%),
  the ground mesh, scenery, about 7 clears a frame. Smaller render size
  (`YSFlight$Render`) scales it down.
- **Hot paths in the gl1 code (M2 test 5, ysce 12):** with particles and
  the ground texture off, the full-screen sky cylinder (FsGroundSky::DrawByFog)
  and the ground mesh were drawn with GL fog (general span path) and, like
  everything, with GL_BLEND on: YSFlight enables blending once at start-up
  and never turns it off, so every opaque pixel was read back and blended
  (`_swrast_get_dest_rgba` + `blend_transparency_ubyte` were ~22% of a frame
  at 10000 ft). Now the background takes per-vertex fog colours with fog,
  blending and depth test off (Mesa's `smooth_rgba_triangle`), and blending
  is off around the map and the opaque aircraft/ground passes. Same seed
  (PROF_SEED) A/B: runway 237M -> 175M, 10000 ft 153M -> 102M; pictures
  match to rounding, plus depth-buffer ties (16-bit depth) at the horizon.
  Left: fog spans on objects (~9%), Mesa's per-vertex colour-material update
  (~10%, `update_materials`: the shells are drawn with per-vertex colours
  and GL_COLOR_MATERIAL), span packing (~10%), clears.
- **OpenGL 2 (GLSL) build, for comparison (ysce 11):** same runway frame,
  12161M instructions (gl1: 603M) and 1556 ms a frame natively on the
  host against 86 ms: about 20 times slower. 80% is Mesa's GLSL
  interpreter (`_mesa_execute_program_batch`) running the per-pixel
  shaders; it also samples a shadow depth texture. The picture is right.
- The menu redecoded a 1920x990 title PNG every 8 s (~1200M instructions)
  and rescaled it on every redraw: ysce 08.
