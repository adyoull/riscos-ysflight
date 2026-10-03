#!/bin/bash
# Checks that YS_PNG_ZLIB (zlib inflate + row-wise unfiltering in
# ysbitmap/src/yspng.cpp, used on RISC OS) decodes exactly like the original
# YsPngUncompressor: every PNG in the runtime data plus test images in all
# colour types, bit depths and interlacing, made with Python/Pillow.
#   WORK=<patched clones> tests/png-decode/run.sh   (needs zlib1g-dev, python3-pil)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
: "${WORK:?}"
SRC=$WORK/ysce_public/src/ysbitmap/src
OUT=${OUT:-/tmp/png-decode}; mkdir -p "$OUT/img"
python3 - "$OUT/img" <<'P'
import sys, random
from PIL import Image
d=sys.argv[1]; random.seed(1)
def noise(mode,w,h):
    im=Image.new(mode,(w,h))
    n={'L':1,'LA':2,'RGB':3,'RGBA':4,'P':1,'1':1}[mode]
    px=[tuple(random.randrange(256) if mode!='1' else random.randrange(2)*255 for _ in range(n)) if n>1 else (random.randrange(256) if mode!='1' else random.randrange(2)*255) for _ in range(w*h)]
    im.putdata(px); return im
k=0
for mode in ['L','LA','RGB','RGBA','P','1']:
    for (w,h) in [(1,1),(7,5),(33,17),(256,64)]:
        for il in [0,1]:
            im=noise(mode,w,h)
            if mode=='P': im.putpalette([random.randrange(256) for _ in range(768)])
            # smooth gradient too, so every filter type gets used
            im.save('%s/t%03d.png'%(d,k), interlace=il, optimize=False); k+=1
    g=Image.linear_gradient('L').resize((97,61))
    g.convert(mode if mode!='P' else 'P').save('%s/t%03d.png'%(d,k)); k+=1
im=noise('RGB',20,20); im.save('%s/trns-rgb.png'%d, transparency=im.getpixel((3,3)))
im=noise('L',20,20); im.save('%s/trns-l.png'%d, transparency=im.getpixel((3,3)))
P
# Interlaced and 16-bit versions (the original Output path, after zlib),
# when ImageMagick is there.
if command -v convert >/dev/null; then
  for f in "$OUT"/img/t0[0-4]?.png; do
    # (not below 8x8: the original decoder leaves those partly unwritten
    # when an Adam7 pass is empty, so the result is undefined either way;
    # nor 1-bit greyscale, which it doesn't support interlaced)
    [ "$(identify -format '%w %h %z' "$f" | awk '{print ($1>=8 && $2>=8 && $3>=8)}')" = 1 ] || continue
    convert "$f" -interlace PNG "${f%.png}-adam7.png"
    convert "$f" -depth 16 "${f%.png}-16.png" 2>/dev/null || true
  done
fi
FILES=$(ls "$OUT"/img/*.png; find "$WORK/YSCE/runtime" -name '*.png' | sort)
g++ -O2 -I"$SRC" "$HERE/pngdump.cpp" "$SRC/yspng.cpp" -o "$OUT/old"
g++ -O2 -DYS_PNG_ZLIB -I"$SRC" "$HERE/pngdump.cpp" "$SRC/yspng.cpp" -lz -o "$OUT/new"
"$OUT/old" $FILES > "$OUT/old.txt" 2>/dev/null
"$OUT/new" $FILES > "$OUT/new.txt" 2>/dev/null
n=$(wc -l < "$OUT/old.txt")
if diff -q "$OUT/old.txt" "$OUT/new.txt" >/dev/null; then
  echo "png-decode: $n images, YS_PNG_ZLIB identical"
else
  diff "$OUT/old.txt" "$OUT/new.txt" | head -20 || true; echo "png-decode: DIFFERENCES"; exit 1
fi
