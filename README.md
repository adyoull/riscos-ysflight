# YSFlight Community Edition for RISC OS

A port of [YSFlight Community Edition](https://github.com/YSCEDC/YSCE), the
free flight simulator, to RISC OS 5, using UnixLib (riscos-unixlib 5.0.3.3)
and the riscos-mesa OpenGL devkit.

**Status: first release (riscos1).** The game runs in a desktop window on a
Raspberry Pi (Wimp + software OpenGL through riscos-mesa), about 22 frames a
second at 640x480 on a Pi 4 with the RISC OS default settings. Sound goes
through UnixLib's `/dev/dsp` (SharedSoundBuffer); USB joysticks through
USBJoystick. Not done yet: the desktop's fonts (text uses YSFlight's own
bitmap font, Latin-1 only) and the online server list. See CHANGELOG.md.

This is an unofficial port. It isn't made, endorsed or supported by the YSCE
developers or by Soji Yamakawa. AI (Anthropic's Claude) has been used as a
coding assistant on this port.

## Requirements

- RISC OS 5 on an ARMv7 or later machine with VFP (Raspberry Pi 2, 3, 4, 400).
- `SharedUnixLibrary` 1.16 or later and `ARMEABISupport` (PackMan).
- `PThreadTicker` 0.03 (from UnixLib 5.0.3.3): included in `!YSFlight` and
  loaded by `!Run`.
- For sound, best: John Duffell's SharedSoundBuffer modules (SharedSound
  1.07, StreamManager 0.03, SharedSoundBuffer 0.07; `ssb.zip` from
  [RDPClient's page](https://orac.co.uk/software/rdpclient/rdpclient.html)).
  Without them UnixLib uses DigitalRenderer.
- Optional: USBJoystick 0.23 or later for USB joysticks and gamepads.

## Installing

Unpack the zip on RISC OS (SparkFS or !InfoZip, so filetypes are kept) and
double-click `!YSFlight`. Messages go to `<Wimp$ScrapDir>.YSFlightLog`;
settings to `<Choices$Write>.YSFlight`. `!YSFlight.ServerTests` runs the
console server tests in a task window.

## This repository

| Path | What |
|---|---|
| `patches/ysce/`, `patches/ysce_public/` | One patch per change, applied in `series` order to YSCE `231e711` and ysce_public `1b13297` |
| `patches/unixlib/unixlib-riscos.diff` | The riscos-unixlib 5.0.3.3 change set (the UnixLib linked into the programs) |
| `build/` | `env.sh`, `build.sh`, `package.sh`, the CMake toolchain files |
| `app/!YSFlight/` | `!Run`, `!Help`, `Tests`, PThreadTicker |
| `tools/check-stack-probes.py` | Finds big stack frames without probes |
| `tests/` | Host tests: window code (riscos-crossdev's fake RISC OS), joystick, sound, PNG decoding |
| `docs/` | PORTING-NOTES.md (every patch and why), LICENSING.md |

Building: BUILDING.md.

## Licence

The port's own files: BSD 3-clause (LICENSE), the same as YSCE. YSCE and its
data: BSD 3-clause, Copyright 2022 Soji Yamakawa and the YSFlight Community
Edition Development Committee (`app/!YSFlight/Licence`). Details:
docs/LICENSING.md.
