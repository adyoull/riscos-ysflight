#!/bin/bash
# tools/profile/run.sh NAME STARTPOS "KEY VALUE;KEY VALUE..."
# One free flight (F-18E, AOMORI, STARTPOS e.g. MISAWA_RW10 or
# NORTH10000_01) under callgrind with the given flight.cfg lines; prints
# the instructions per measured frame. Fixed 50 ms steps (PROF_FIXEDDT), so
# a run under valgrind flies like a native one. Out: $PROF/out/NAME-POS.*
# (callgrind file, log, last frame as PPM). GAME=<binary> profiles another
# build (e.g. ysflight64_gl2). VALGRIND= runs natively and
# prints the time per frame instead.
set -euo pipefail
: "${PROF:=/tmp/ysprof}"
name=$1 pos=$2 keys=${3:-}
out=$PROF/out; cfg=$out/cfg-$name; rm -rf "$cfg"; mkdir -p "$cfg"
: > "$cfg/flight.cfg"
IFS=';' read -ra kv <<< "$keys"
for k in "${kv[@]}"; do [ -n "$k" ] && echo "$k" >> "$cfg/flight.cfg"; done
mkdir -p "$out/user"
cd "$PROF/src/YSCE/runtime"
export LD_LIBRARY_PATH=$PROF/gl PROF_FIXEDDT=50 PROF_SIZE=${SIZE:-640x480}
# Enter closes the joystick message, Space starts the flight
export PROF_KEYS="30:57:1,60:1:2" PROF_SKIP=${SKIP:-200} PROF_FRAMES=${FR:-20}
export PROF_PPM=$out/$name-$pos.ppm
VG=${VALGRIND-valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file=$out/$name-$pos.cg}
$VG "${GAME:-$PROF/build/main/ysflight64_gl1}" -configdir "$cfg" -userdir "$out/user" \
  -freeflight F-18E_SUPERHORNET AOMORI "$pos" > "$out/$name-$pos.log" 2>&1 || true
if [ -n "$VG" ]; then
  ir=$(callgrind_annotate "$out/$name-$pos.cg" | awk '/PROGRAM TOTALS/{gsub(",","",$1); print $1}')
  echo "$name $pos: $((ir/${FR:-20}/1000000))M instructions/frame"
else
  grep "PROF: [0-9]" "$out/$name-$pos.log"
fi
