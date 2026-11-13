#!/bin/bash
# Make the RISC OS test zip: dist/YSFlight-<version>.zip with !YSFlight
# (app/ + the upstream runtime data + the game and the console server as
# Absolute files).
#   build/package.sh VERSION      (after build/build.sh)
set -euo pipefail
. "$(dirname "$0")/env.sh"
VERSION=${1:?usage: package.sh VERSION}
OUT="$RYS_DIR/dist"; STAGE=$(mktemp -d)
APP="$STAGE/!YSFlight"
cp -r "$RYS_DIR/app/!YSFlight" "$APP"
# All of upstream's runtime data (aircraft, scenery, sound, document...)
cp -r "$WORK/YSCE/runtime/." "$APP/"
# The menu background pictures are 1920x99x and only ever drawn stretched
# to the window: 1024 wide decodes about 3.5 times faster on the Pi.
python3 - "$APP/misc" <<'PY'
import sys, glob
from PIL import Image
for f in sorted(glob.glob(sys.argv[1] + "/title*.png")):
    im = Image.open(f)
    if im.width > 1024:
        im = im.resize((1024, round(im.height * 1024 / im.width)), Image.LANCZOS)
        im.save(f, optimize=True)
PY
# Absolute (AIF) files, so they need no ELF loader (riscos-crossdev rule 7):
# the game as !RunImage, the console server as Server.
# MIXED=1: the game is a mixed program, kept as an ELF file (&E1F) called
# YSFlight that !SharedLibs runs, with OpenGL, EGL and GLU from !MesaLibs;
# !Run gets the lines that boot and check !SharedLibs and !MesaLibs.
if [ -n "$MIXED" ]; then
  arm-riscos-gnueabihf-readelf -d "$WORK/build/main/ysflight32_gl1" | grep -q "NEEDED.*libEGL" ||
    { echo "the game isn't mixed: build it with MIXED=1" >&2; exit 1; }
  arm-riscos-gnueabihf-strip -o "$APP/YSFlight,e1f" "$WORK/build/main/ysflight32_gl1"
  python3 - "$APP/!Run,feb" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="latin-1").read()
old = """| The program (about 16 MB) is loaded into the slot; its heap goes in
| dynamic areas ("YSFlight Heap").
WimpSlot -min 20480K -max 20480K
Run <YSFlight$Dir>.!RunImage %*0
"""
new = """| This is the mixed build: the program (YSFlight, an ELF file) loads
| OpenGL, EGL and GLU from !MesaLibs, through !SharedLibs. Both go in
| !Boot.Resources. EGL draws with the GPU through RISC3D where there's a
| driver for it, else in software (riscos-mesa decides).
If "<SharedLibs$Dir>" = "" Then IfThere Boot:Resources.!SharedLibs.!Boot Then Run Boot:Resources.!SharedLibs.!Boot
If "<MesaLibs$Dir>" = "" Then IfThere Boot:Resources.!MesaLibs.!Boot Then Run Boot:Resources.!MesaLibs.!Boot
Unset YSFlight$SO2
IfThere SharedLibs:lib.ld-riscos-eabihf/so Then Set YSFlight$SO2 1
If "<YSFlight$SO2>" = "" Then Error YSFlight needs a !SharedLibs with ld-riscos/so/2 (GCCSDK 8 or later, as in Iris) in !Boot.Resources
If "<MesaLibs$Dir>" = "" Then Error YSFlight needs !MesaLibs in !Boot.Resources
Unset YSFlight$SO2
Set YSFlight$LD$Env "LD_LIBRARY_PATH=/MesaLibs:"
| The program is loaded into the slot; its heap goes in dynamic areas
| ("YSFlight Heap").
WimpSlot -min 16000K -max 16000K
Run <YSFlight$Dir>.YSFlight %*0
"""
assert old in s, "!Run: the Run lines have changed"
s = s.replace(old, new).replace("| !Run for !YSFlight: YSFlight Community Edition for RISC OS.",
                                "| !Run for !YSFlight: YSFlight Community Edition for RISC OS (mixed build).")
open(p, "w", encoding="latin-1").write(s)
PY
else
  arm-riscos-gnueabihf-strip -o "$STAGE/gl1.elf" "$WORK/build/main/ysflight32_gl1"
  elf2aif -e "$STAGE/gl1.elf" "$APP/!RunImage,ff8"
  rm "$STAGE/gl1.elf"
fi
arm-riscos-gnueabihf-strip -o "$STAGE/nownd.elf" "$WORK/build/main_consvr/ysflight32_nownd"
elf2aif -e "$STAGE/nownd.elf" "$APP/Server,ff8"
rm "$STAGE/nownd.elf"
if grep -rqa "$WORK" "$APP"; then echo "build path found in the app" >&2; exit 1; fi
mkdir -p "$OUT"; rm -f "$OUT/YSFlight-$VERSION.zip"
python3 "$RISCOS_DEVKIT/bin/mkrozip.py" "$OUT/YSFlight-$VERSION.zip" "$APP"
rm -rf "$STAGE"
sha256sum "$OUT/YSFlight-$VERSION.zip"
