#!/bin/bash
# OpenGL 2 (GLSL) build, for comparison only: dist/YSFlight-gl2-<version>.zip
# holds !YSFlight.RunImageGL2 (the gl2 game) and !YSFlight.RunGL2 (an Obey
# file like !Run that starts it). Unzip it over an !YSFlight of the same
# version and run RunGL2.
#   GL2=1 build/build.sh; build/package-gl2.sh VERSION
set -euo pipefail
. "$(dirname "$0")/env.sh"
VERSION=${1:?usage: package-gl2.sh VERSION}
OUT="$RYS_DIR/dist"; STAGE=$(mktemp -d)
APP="$STAGE/!YSFlight"; mkdir -p "$APP"
arm-riscos-gnueabihf-strip -o "$STAGE/gl2.elf" "$WORK/build/main/ysflight32_gl2"
elf2aif -e "$STAGE/gl2.elf" "$APP/RunImageGL2,ff8"
rm "$STAGE/gl2.elf"
{ echo "| RunGL2: starts the OpenGL 2 (GLSL) build, for comparing speed with"
  echo "| the normal OpenGL 1 build (!Run). It must be inside !YSFlight."
  sed 's/^Run <YSFlight\$Dir>\.!RunImage /Run <YSFlight$Dir>.RunImageGL2 /' "$RYS_DIR/app/!YSFlight/!Run,feb" |
    grep -v '^| !Run for !YSFlight'; } > "$APP/RunGL2,feb"
grep -q 'RunImageGL2 %\*0' "$APP/RunGL2,feb" || { echo "RunGL2 not made" >&2; exit 1; }
if grep -rqa "$WORK" "$APP"; then echo "build path found in the app" >&2; exit 1; fi
mkdir -p "$OUT"; rm -f "$OUT/YSFlight-gl2-$VERSION.zip"
python3 "$RISCOS_DEVKIT/bin/mkrozip.py" "$OUT/YSFlight-gl2-$VERSION.zip" "$APP"
rm -rf "$STAGE"
sha256sum "$OUT/YSFlight-gl2-$VERSION.zip"
