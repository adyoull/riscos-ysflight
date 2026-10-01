# Changelog

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
