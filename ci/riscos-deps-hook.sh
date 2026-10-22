#!/bin/bash
# Run by .github/workflows/riscos-deps-update.yml as
#   ci/riscos-deps-hook.sh NAME OLD_TAG NEW_TAG
# before it moves NAME's pin in .riscos-deps. For riscos-mesa, the devkit's
# sha256 in build/SHA256SUMS moves with it: the new release's devkit is
# downloaded and its sha256 written there (build/fetch-devkit.sh checks it).
set -euo pipefail
name=$1 old=$2 new=$3
case $name in
riscos-mesa|adyoull/riscos-mesa)
  F=riscos-mesa-devkit-${new#v}.tgz
  tmp=$(mktemp)
  curl -fsSL -o "$tmp" "https://github.com/adyoull/riscos-mesa/releases/download/$new/$F"
  sum=$(sha256sum < "$tmp" | cut -d' ' -f1)
  rm -f "$tmp"
  awk -v f="$F" -v s="$sum" '$2 !~ /^riscos-mesa-devkit-/ {print} END {print s "  " f}' build/SHA256SUMS > build/SHA256SUMS.new
  mv build/SHA256SUMS.new build/SHA256SUMS
  echo "build/SHA256SUMS: $F $sum (was ${old})"
  ;;
esac
