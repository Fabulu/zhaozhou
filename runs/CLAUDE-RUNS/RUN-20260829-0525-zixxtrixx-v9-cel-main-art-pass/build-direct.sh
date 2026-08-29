#!/bin/bash
# Direct g++ build for the reel and Zixxtrixx tools. Never use cmake --build:
# the repository's build.ninja regeneration race can leave a stale binary.
# Usage: build-direct.sh [clean|reel|cel|frozenpupil|probe|golden|choreo|planner|headaim|sideprofile|meshcheck|striketip|all]
# Objects and executables are run-local. After a struct-layout change, run clean then all.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
RUN="$ROOT/runs/CLAUDE-RUNS/RUN-20260829-0525-zixxtrixx-v9-cel-main-art-pass"
OBJ="$RUN/obj"
BIN="$RUN/bin"
mkdir -p "$OBJ" "$BIN"
CXX=g++
FLAGS="-O2 -std=c++17 -I$ROOT/reference/include -I$ROOT/runtime/include -I$ROOT/tests/render -I$ROOT/compiler/tests/generated -I$ROOT/reference/src"
ZREF_SRCS="zref_frame zref zref_audio zref_video zfield/zfield_decode zfield/zfield_interpret zfield/zfield_plan zterrain/terrain_core zrender/render_frame zrender/rast zrender/edgewalk zrender/geom zrender/terrain zrender/sprites zrender/resolve zrender/tilestore zrender/tileresolve zrender/earlyz zrender/fragment zrender/texture zsky/emit_layers zsky/star_gamut zsky/star_bake zsky/star_flare zsky/star_field zsky/star_compose zsky/env_state zcreature/creature_core zcreature/creature_sim"
[ "$1" = clean ] && rm -f "$OBJ"/*.o "$BIN"/*.exe
for s in $ZREF_SRCS; do
  o="$OBJ/$(printf '%s' "$s" | tr / _).o"
  src="$ROOT/reference/src/$s.cpp"
  if [ ! -f "$o" ] || [ "$src" -nt "$o" ]; then
    printf 'CC %s\n' "$s"
    $CXX $FLAGS -c "$src" -o "$o" &
  fi
done
for j in $(jobs -p); do wait "$j" || { printf '%s\n' "COMPILE FAILED"; exit 1; }; done
LIBOBJS=$(for s in $ZREF_SRCS; do printf '%s ' "$OBJ/$(printf '%s' "$s" | tr / _).o"; done)
want() { [ -z "$1" ] || [ "$1" = all ] || [ "$1" = "$2" ]; }
T="$ROOT/tools/reel"
CEL_HEADER=$(cygpath -m "$T/zixxtrixx_page_cel.h")
if want "$1" reel;        then printf '%s\n' "LD zhao-reel";     $CXX $FLAGS "$T/zhao_reel.cpp"      $LIBOBJS -o "$BIN/zhao-reel.exe" & fi
if want "$1" cel; then
  if [ ! -f "$T/zixxtrixx_page_cel.h" ]; then
    printf '%s\n' "missing tools/reel/zixxtrixx_page_cel.h; run mkcreaturepage.py --cel-main first" >&2
    exit 1
  fi
  printf '%s\n' "LD zhao-reel-cel"
  $CXX $FLAGS "-DZIXX_PAGE_VARIANT=\"$CEL_HEADER\"" "$T/zhao_reel.cpp" $LIBOBJS -o "$BIN/zhao-reel-cel.exe" &
fi
if want "$1" frozenpupil; then
  if [ ! -f "$T/zixxtrixx_page_cel.h" ]; then
    printf '%s\n' "missing tools/reel/zixxtrixx_page_cel.h; run mkcreaturepage.py --cel-main first" >&2
    exit 1
  fi
  printf '%s\n' "LD zhao-reel-cel-frozenpupil"
  $CXX $FLAGS -DZIXX_PUPIL_MOTION=0 "-DZIXX_PAGE_VARIANT=\"$CEL_HEADER\"" "$T/zhao_reel.cpp" $LIBOBJS -o "$BIN/zhao-reel-cel-frozenpupil.exe" &
fi
if want "$1" probe;       then printf '%s\n' "LD zixx-probe";    $CXX $FLAGS "$T/zixx_probe.cpp"     $LIBOBJS -o "$BIN/zixx-probe.exe" & fi
if want "$1" golden;      then printf '%s\n' "LD zixx-golden";   $CXX $FLAGS "$T/zixx_golden.cpp"    $LIBOBJS -o "$BIN/zixx-golden.exe" & fi
if want "$1" choreo;      then printf '%s\n' "LD zixx-choreo";   $CXX $FLAGS "$T/zixx_choreo.cpp"    $LIBOBJS -o "$BIN/zixx-choreo.exe" & fi
if want "$1" planner;     then printf '%s\n' "LD zixx-planner";  $CXX $FLAGS "$T/zixx_planner.cpp"   $LIBOBJS -o "$BIN/zixx-planner.exe" & fi
if want "$1" headaim;     then printf '%s\n' "LD zixx-headaim";  $CXX $FLAGS "$T/zixx_headaim.cpp"   $LIBOBJS -o "$BIN/zixx-headaim.exe" & fi
if want "$1" sideprofile; then printf '%s\n' "LD zixx-sideprofile"; $CXX $FLAGS "$T/zixx_sideprofile.cpp" $LIBOBJS -o "$BIN/zixx-sideprofile.exe" & fi
if want "$1" meshcheck;   then printf '%s\n' "LD zixx-meshcheck"; $CXX $FLAGS "$T/zixx_meshcheck.cpp" $LIBOBJS -o "$BIN/zixx-meshcheck.exe" & fi
if want "$1" striketip;   then printf '%s\n' "LD zixx-striketip"; $CXX $FLAGS "$T/zixx_striketip.cpp" $LIBOBJS -o "$BIN/zixx-striketip.exe" & fi
for j in $(jobs -p); do wait "$j" || { printf '%s\n' "LINK FAILED"; exit 1; }; done
printf '%s\n' "build-direct: done ($BIN)"
