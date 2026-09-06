#!/usr/bin/env bash
# STAGE L driver: render ONE particle variant across the knead window.
#   runvar.sh <tag> [ENV=VAL ...]
# Frames written: 250..368, all inside seg=3 (knead) of the rest clip.
set -uo pipefail
LANE=/c/programmieren/zencrifice/manafold-p11-L
TAG="$1"; shift
OUT="$LANE/out/$TAG"
mkdir -p "$OUT/manafold-fogprobe-mana"
env "$@" PL_FRAMES=250:368 "$LANE/build/bin/zhao-reel-cel.exe" "$OUT" \
    manafold-fogprobe-mana > "$OUT/render.log" 2> "$OUT/render.err"
RC=$?
echo "$TAG RENDER_RC=$RC  $*"
