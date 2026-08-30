#!/usr/bin/env bash
# Direct native reel build used by creature authoring workbenches.
# This is the durable form of the proven V14 build recipe. It never invokes
# CMake/Ninja/Verilator and requires an explicit caller-owned output directory.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/reel/build-direct.sh --output <dir> [--clean] [reel|cel|meshcheck|all]

Build the reference reel directly into <dir>/bin with objects in <dir>/obj.
The caller owns <dir>; no output is written beside this script.

Targets:
  reel       zhao-reel.exe
  cel        zhao-reel-cel.exe (default)
  meshcheck  zixx-meshcheck.exe
  all        all three executables
EOF
}

OUTPUT=""
CLEAN=0
TARGET="cel"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --output)
      [ "$#" -ge 2 ] || { printf '%s\n' "--output requires a value" >&2; exit 2; }
      OUTPUT="$2"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    reel|cel|meshcheck|all)
      TARGET="$1"
      shift
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

[ -n "$OUTPUT" ] || { printf '%s\n' "--output is required" >&2; usage >&2; exit 2; }

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if command -v cygpath >/dev/null 2>&1; then
  OUTPUT="$(cygpath -u "$OUTPUT")"
fi
mkdir -p "$OUTPUT"
OUTPUT="$(cd "$OUTPUT" && pwd)"
OBJ="$OUTPUT/obj"
BIN="$OUTPUT/bin"
mkdir -p "$OBJ" "$BIN"

if [ "$CLEAN" -eq 1 ]; then
  rm -f "$OBJ"/*.o "$BIN"/*.exe
fi

CXX="${CXX:-g++}"
FLAGS=(
  -O2
  -std=c++17
  "-I$ROOT/reference/include"
  "-I$ROOT/runtime/include"
  "-I$ROOT/tests/render"
  "-I$ROOT/compiler/tests/generated"
  "-I$ROOT/reference/src"
)
ZREF_SRCS=(
  zref_frame zref zref_audio zref_video
  zfield/zfield_decode zfield/zfield_interpret zfield/zfield_plan
  zterrain/terrain_core
  zrender/render_frame zrender/rast zrender/edgewalk zrender/geom
  zrender/terrain zrender/sprites zrender/resolve zrender/tilestore
  zrender/tileresolve zrender/earlyz zrender/fragment zrender/texture
  zsky/emit_layers zsky/star_gamut zsky/star_bake zsky/star_flare
  zsky/star_field zsky/star_compose zsky/env_state
  zcreature/creature_core zcreature/creature_sim
)

pids=()
stop_children() {
  local pid
  for pid in "${pids[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  for pid in "${pids[@]}"; do
    wait "$pid" 2>/dev/null || true
  done
}
trap 'stop_children; exit 130' INT TERM
for source_name in "${ZREF_SRCS[@]}"; do
  object="$OBJ/${source_name//\//_}.o"
  source="$ROOT/reference/src/$source_name.cpp"
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then
    printf 'CC %s\n' "$source_name"
    "$CXX" "${FLAGS[@]}" -c "$source" -o "$object" &
    pids+=("$!")
  fi
done
failed=0
for pid in "${pids[@]}"; do
  wait "$pid" || failed=1
done
trap - INT TERM
[ "$failed" -eq 0 ] || { printf '%s\n' "COMPILE FAILED" >&2; exit 1; }

LIBOBJS=()
for source_name in "${ZREF_SRCS[@]}"; do
  LIBOBJS+=("$OBJ/${source_name//\//_}.o")
done
T="$ROOT/tools/reel"

build_reel() {
  printf '%s\n' "LD zhao-reel"
  "$CXX" "${FLAGS[@]}" "$T/zhao_reel.cpp" "${LIBOBJS[@]}" \
    -o "$BIN/zhao-reel.exe"
}

build_cel() {
  local header="$T/zixxtrixx_page_cel.h"
  [ -f "$header" ] || { printf 'missing %s\n' "$header" >&2; exit 1; }
  if command -v cygpath >/dev/null 2>&1; then
    header="$(cygpath -m "$header")"
  fi
  printf '%s\n' "LD zhao-reel-cel"
  "$CXX" "${FLAGS[@]}" "-DZIXX_PAGE_VARIANT=\"$header\"" \
    "$T/zhao_reel.cpp" "${LIBOBJS[@]}" -o "$BIN/zhao-reel-cel.exe"
}

build_meshcheck() {
  printf '%s\n' "LD zixx-meshcheck"
  "$CXX" "${FLAGS[@]}" "$T/zixx_meshcheck.cpp" "${LIBOBJS[@]}" \
    -o "$BIN/zixx-meshcheck.exe"
}

case "$TARGET" in
  reel) build_reel ;;
  cel) build_cel ;;
  meshcheck) build_meshcheck ;;
  all)
    build_reel
    build_cel
    build_meshcheck
    ;;
esac
printf 'build-direct: done (%s)\n' "$BIN"
