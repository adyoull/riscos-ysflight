#!/bin/bash
# Make sure $RISCOS_DEVKIT is the riscos-mesa devkit .riscos-deps names:
# download it from riscos-mesa's GitHub release into $WORK if it isn't
# there, checked against build/SHA256SUMS. (The riscos-crossdev image has
# its own devkit; build/env.sh uses that one when the versions match.)
set -euo pipefail
. "$(dirname "$0")/env.sh"
if [ "$(cat "$RISCOS_DEVKIT/VERSION" 2>/dev/null)" = "riscos-mesa devkit $MESA_VERSION" ]; then
  exit 0
fi
F=riscos-mesa-devkit-$MESA_VERSION.tgz
want=$(awk -v f="$F" '$2 == f {print $1}' "$RYS_DIR/build/SHA256SUMS")
[ -n "$want" ] || { echo "build/SHA256SUMS has no line for $F" >&2; exit 1; }
mkdir -p "$WORK/dl"
if [ ! -f "$WORK/dl/$F" ] || [ "$(sha256sum < "$WORK/dl/$F" | cut -d' ' -f1)" != "$want" ]; then
  echo "downloading $F"
  curl -fsSL -o "$WORK/dl/$F.part" "https://github.com/adyoull/riscos-mesa/releases/download/$MESA_TAG/$F"
  mv "$WORK/dl/$F.part" "$WORK/dl/$F"
fi
have=$(sha256sum < "$WORK/dl/$F" | cut -d' ' -f1)
[ "$have" = "$want" ] || { echo "$F: sha256 $have, expected $want" >&2; exit 1; }
rm -rf "$RISCOS_DEVKIT"
tar xzf "$WORK/dl/$F" -C "$WORK"
[ "$(cat "$RISCOS_DEVKIT/VERSION")" = "riscos-mesa devkit $MESA_VERSION" ] ||
  { echo "$F didn't unpack to $RISCOS_DEVKIT" >&2; exit 1; }
echo "devkit $MESA_VERSION in $RISCOS_DEVKIT"
