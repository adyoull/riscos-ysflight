# Source this: . build/env.sh
# Settings for the riscos-ysflight build scripts. Each can be set first.
#   RISCOS_TOOLCHAIN  riscos-crossdev toolchain 1.3 with UnixLib 5.0.3.3's
#                     libunixlib.a copied in (BUILDING.md)
#   RISCOS_DEVKIT     riscos-mesa devkit (20.3.5-14)
#   UNIXLIB_SHA256    the libunixlib.a the build must link (5.0.3.3 release);
#                     =any skips the check
#   WORK              where the upstream clones and the build go (./work)
#   YSCE_REF / PUBLIC_REF  upstream commits; =HEAD uses whatever is checked out
RYS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${WORK:=$PWD/work}"
: "${YSCE_REF:=231e711}"
: "${PUBLIC_REF:=1b13297}"
: "${JOBS:=$(nproc)}"
: "${UNIXLIB_SHA256:=e24f16c3f4612aae3871ceff521ab86b0d3ac4cc689d734e0e4085d02c774adf}"
: "${RISCOS_TOOLCHAIN:?set RISCOS_TOOLCHAIN to the riscos-crossdev toolchain}"
: "${RISCOS_DEVKIT:?set RISCOS_DEVKIT to the riscos-mesa devkit}"
export RYS_DIR WORK YSCE_REF PUBLIC_REF JOBS RISCOS_TOOLCHAIN RISCOS_DEVKIT UNIXLIB_SHA256
export PATH="$RISCOS_TOOLCHAIN/bin:$PATH"

# use_ref DIR REF: check out REF in a clean clone. A clone with changes is
# left alone if it is already at REF (patches applied), else it's an error.
use_ref() {
  local dir=$1 ref=$2
  [ "$ref" = HEAD ] && return 0
  local want have
  want=$(git -C "$dir" rev-parse "$ref^{commit}")
  have=$(git -C "$dir" rev-parse HEAD)
  if [ -n "$(git -C "$dir" status --porcelain)" ]; then
    [ "$want" = "$have" ] && return 0
    echo "$dir has changes and is not at $ref; clean it first" >&2; return 1
  fi
  git -C "$dir" checkout -q "$want"
}

# apply_series DIR PATCHDIR: apply the patches named in PATCHDIR/series, in
# order, skipping any that are already applied.
apply_series() {
  local dir=$1 pdir=$2 p
  while read -r p; do
    [ -n "$p" ] || continue
    if git -C "$dir" apply --reverse --check "$pdir/$p" 2>/dev/null; then
      echo "  already applied: $p"
    else
      git -C "$dir" apply "$pdir/$p"; echo "  applied: $p"
    fi
  done < "$pdir/series"
}
