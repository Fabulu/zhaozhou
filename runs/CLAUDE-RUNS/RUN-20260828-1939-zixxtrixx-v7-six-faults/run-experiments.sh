#!/bin/bash
# RUN 1939: the texture/lighting experiment factory (pages already
# generated and reels already built by the first invocation; both steps
# are cheap no-ops to re-run). Renders land as the reel's own subject
# dir and are MOVED to the experiment's site name before encoding.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
RUN="$ROOT/runs/CLAUDE-RUNS/RUN-20260828-1939-zixxtrixx-v7-six-faults"
SITE="$(cd "$ROOT/../Upheaval/website" && pwd)"
OBJ="$RUN/obj"
FLAGS="-O2 -std=c++17 -I$ROOT/reference/include -I$ROOT/runtime/include -I$ROOT/tests/render -I$ROOT/compiler/tests/generated -I$ROOT/reference/src"
LIBOBJS=$(ls "$OBJ"/*.o)
for v in contour strokes wax tooth wobble markings misreg; do
  if [ ! -f "$ROOT/tools/reel/exp/zixxtrixx_page_exp_$v.h" ]; then
    echo "== page $v"; python "$ROOT/tools/pack/mkcreaturepage.py" --experiment "$v" > /dev/null
  fi
  if [ ! -f "$ROOT/build/tools/zhao-reel-exp-$v.exe" ]; then
    echo "== reel $v"
    g++ $FLAGS "-DZIXX_PAGE_VARIANT=\"$ROOT/tools/reel/exp/zixxtrixx_page_exp_$v.h\"" \
        "$ROOT/tools/reel/zhao_reel.cpp" $LIBOBJS -o "$ROOT/build/tools/zhao-reel-exp-$v.exe"
  fi
done
render() {  # exe env-name out-name subject
  local exe="$1" env="$2" out="$3" subj="$4"
  echo "== $out"
  local work="$SITE/scratch-reel/exp-work"
  rm -rf "$work"; mkdir -p "$work"
  ( cd "$work" && ZIXX_EXP="$env" "$exe" . "$subj" > /dev/null 2>&1 )
  rm -rf "$SITE/scratch-reel/$out"
  mv "$work/$subj" "$SITE/scratch-reel/$out"
  rm -rf "$work"
  python "$SITE/tools/tovideo.py" "$SITE" "$out"
}
NORM="$ROOT/build/tools/zhao-reel.exe"
render "$ROOT/build/tools/zhao-reel-exp-contour.exe"  contour    zixxtrixx-exp-contour-idle    zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-contour.exe"  contour    zixxtrixx-exp-contour-walk    zixxtrixx-walk
render "$ROOT/build/tools/zhao-reel-exp-strokes.exe"  ""         zixxtrixx-exp-strokes-idle    zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-wax.exe"      ""         zixxtrixx-exp-wax-idle        zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-tooth.exe"    ""         zixxtrixx-exp-tooth-idle      zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-wobble.exe"   ""         zixxtrixx-exp-wobble-idle     zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-markings.exe" ""         zixxtrixx-exp-markings-idle   zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-misreg.exe"   ""         zixxtrixx-exp-misreg-idle     zixxtrixx-idle
render "$NORM"                                        boil       zixxtrixx-exp-boil-idle       zixxtrixx-idle
render "$NORM"                                        boil       zixxtrixx-exp-boil-walk       zixxtrixx-walk
render "$NORM"                                        cel2       zixxtrixx-exp-cel2-idle       zixxtrixx-idle
render "$NORM"                                        cel3       zixxtrixx-exp-cel3-idle       zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-contour.exe"  celcontour zixxtrixx-exp-celcontour-idle zixxtrixx-idle
render "$ROOT/build/tools/zhao-reel-exp-contour.exe"  celcontour zixxtrixx-exp-celcontour-walk zixxtrixx-walk
echo "EXPERIMENTS DONE"
