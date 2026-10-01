#!/bin/bash
# Make the RISC OS test zip: dist/YSFlight-<version>.zip with !YSFlight
# (app/ + the upstream runtime data + the console server as an Absolute file).
#   build/package.sh VERSION      (after build/build.sh)
set -euo pipefail
. "$(dirname "$0")/env.sh"
VERSION=${1:?usage: package.sh VERSION}
OUT="$RYS_DIR/dist"; STAGE=$(mktemp -d)
APP="$STAGE/!YSFlight"
cp -r "$RYS_DIR/app/!YSFlight" "$APP"
for d in aircraft ground scenery misc mission language config sound; do
  cp -r "$WORK/YSCE/runtime/$d" "$APP/"
done
arm-riscos-gnueabihf-strip -o "$STAGE/nownd.elf" "$WORK/build/main_consvr/ysflight32_nownd"
# Absolute (AIF) file, so it needs no ELF loader (riscos-crossdev rule 7).
elf2aif -e "$STAGE/nownd.elf" "$APP/!RunImage,ff8"
rm "$STAGE/nownd.elf"
if grep -rqa "$WORK" "$APP"; then echo "build path found in the app" >&2; exit 1; fi
mkdir -p "$OUT"; rm -f "$OUT/YSFlight-$VERSION.zip"
python3 "$RISCOS_DEVKIT/bin/mkrozip.py" "$OUT/YSFlight-$VERSION.zip" "$APP"
rm -rf "$STAGE"
sha256sum "$OUT/YSFlight-$VERSION.zip"
