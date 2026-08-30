#!/bin/bash
# Fresh direct build for bounded Zixxtrixx v13 diagnosis and one candidate.
# Never use cmake --build: stale generated state can run old binaries.
# Usage: build-direct.sh [clean|reel|cel|meshcheck|all]
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
RUN="$ROOT/runs/CLAUDE-RUNS/RUN-20260830-1049-zixxtrixx-v13-light-direction-fix"
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
if want "$1" reel; then
  printf '%s\n' "LD zhao-reel"
  $CXX $FLAGS "$T/zhao_reel.cpp" $LIBOBJS -o "$BIN/zhao-reel.exe" &
fi
if want "$1" cel; then
  if [ ! -f "$T/zixxtrixx_page_cel.h" ]; then
    printf '%s\n' "missing tools/reel/zixxtrixx_page_cel.h" >&2
    exit 1
  fi
  CEL_HEADER=$(cygpath -m "$T/zixxtrixx_page_cel.h")
  printf '%s\n' "LD zhao-reel-cel"
  $CXX $FLAGS "-DZIXX_PAGE_VARIANT=\"$CEL_HEADER\"" "$T/zhao_reel.cpp" $LIBOBJS -o "$BIN/zhao-reel-cel.exe" &
fi
if want "$1" meshcheck; then
  printf '%s\n' "LD zixx-meshcheck"
  $CXX $FLAGS "$T/zixx_meshcheck.cpp" $LIBOBJS -o "$BIN/zixx-meshcheck.exe" &
fi
for j in $(jobs -p); do wait "$j" || { printf '%s\n' "LINK FAILED"; exit 1; }; done
printf '%s\n' "build-direct: done ($BIN)"
