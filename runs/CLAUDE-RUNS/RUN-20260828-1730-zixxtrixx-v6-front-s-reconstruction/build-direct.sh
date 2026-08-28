#!/bin/bash
# Direct g++ build for the reel + zixx tools. NEVER cmake --build (stale-binary
# race, zhaozhou CLAUDE.md). Usage: build-direct.sh [clean|reel|probe|golden|choreo|all]
# Objects live in the run-local objdir; a struct-layout change => run 'clean'.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OBJ="$ROOT/runs/CLAUDE-RUNS/RUN-20260828-1730-zixxtrixx-v6-front-s-reconstruction/obj"
mkdir -p "$OBJ" "$ROOT/build/tools"
CXX=g++
FLAGS="-O2 -std=c++17 -I$ROOT/reference/include -I$ROOT/runtime/include -I$ROOT/tests/render -I$ROOT/compiler/tests/generated -I$ROOT/reference/src"
ZREF_SRCS="zref_frame zref zref_audio zref_video zfield/zfield_decode zfield/zfield_interpret zfield/zfield_plan zterrain/terrain_core zrender/render_frame zrender/rast zrender/edgewalk zrender/geom zrender/terrain zrender/sprites zrender/resolve zrender/tilestore zrender/tileresolve zrender/earlyz zrender/fragment zrender/texture zsky/emit_layers zsky/star_gamut zsky/star_bake zsky/star_flare zsky/star_field zsky/star_compose zsky/env_state zcreature/creature_core zcreature/creature_sim"
[ "$1" = clean ] && rm -f "$OBJ"/*.o
for s in $ZREF_SRCS; do
  o="$OBJ/$(echo $s | tr / _).o"
  src="$ROOT/reference/src/$s.cpp"
  if [ ! -f "$o" ] || [ "$src" -nt "$o" ]; then
    echo "CC $s"; $CXX $FLAGS -c "$src" -o "$o" &
  fi
done
for j in $(jobs -p); do wait "$j" || { echo "COMPILE FAILED"; exit 1; }; done
LIBOBJS=$(for s in $ZREF_SRCS; do echo "$OBJ/$(echo $s | tr / _).o"; done)
want() { [ -z "$1" ] || [ "$1" = all ] || [ "$1" = "$2" ]; }
T="$ROOT/tools/reel"
if want "$1" reel;   then echo "LD zhao-reel";   $CXX $FLAGS "$T/zhao_reel.cpp"   $LIBOBJS -o "$ROOT/build/tools/zhao-reel.exe" & fi
if want "$1" probe;  then echo "LD zixx-probe";  $CXX $FLAGS "$T/zixx_probe.cpp"  $LIBOBJS -o "$ROOT/build/tools/zixx-probe.exe" & fi
if want "$1" golden; then echo "LD zixx-golden"; $CXX $FLAGS "$T/zixx_golden.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-golden.exe" & fi
if want "$1" choreo; then echo "LD zixx-choreo"; $CXX $FLAGS "$T/zixx_choreo.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-choreo.exe" & fi
if want "$1" planner; then echo "LD zixx-planner"; $CXX $FLAGS "$T/zixx_planner.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-planner.exe" & fi
if want "$1" headaim; then echo "LD zixx-headaim"; $CXX $FLAGS "$T/zixx_headaim.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-headaim.exe" & fi
if want "$1" sideprofile; then echo "LD zixx-sideprofile"; $CXX $FLAGS "$T/zixx_sideprofile.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-sideprofile.exe" & fi
if want "$1" striketip; then echo "LD zixx-striketip"; $CXX $FLAGS "$T/zixx_striketip.cpp" $LIBOBJS -o "$ROOT/build/tools/zixx-striketip.exe" & fi
for j in $(jobs -p); do wait "$j" || { echo "LINK FAILED"; exit 1; }; done
echo "build-direct: done"
