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
arm-riscos-gnueabihf-strip -o "$STAGE/gl1.elf" "$WORK/build/main/ysflight32_gl1"
elf2aif -e "$STAGE/gl1.elf" "$APP/!RunImage,ff8"
arm-riscos-gnueabihf-strip -o "$STAGE/nownd.elf" "$WORK/build/main_consvr/ysflight32_nownd"
elf2aif -e "$STAGE/nownd.elf" "$APP/Server,ff8"
rm "$STAGE/gl1.elf" "$STAGE/nownd.elf"
if grep -rqa "$WORK" "$APP"; then echo "build path found in the app" >&2; exit 1; fi
mkdir -p "$OUT"; rm -f "$OUT/YSFlight-$VERSION.zip"
python3 "$RISCOS_DEVKIT/bin/mkrozip.py" "$OUT/YSFlight-$VERSION.zip" "$APP"
rm -rf "$STAGE"
sha256sum "$OUT/YSFlight-$VERSION.zip"
