#!/bin/bash
# Stage two, deliberately run only after the first production deployment:
# render every canonical Zixxtrixx clip through the faceted cel3 + five-pixel
# contour presentation, encode under separate names, and make every-frame sheets.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
RUN="$ROOT/runs/CLAUDE-RUNS/RUN-20260828-2234-zixxtrixx-v8-closeout-thick-outlines"
SITE="$(cd "$ROOT/../Upheaval/website" && pwd)"
REEL="$ROOT/build/tools/zhao-reel-exp-contour.exe"
TMP="$SITE/scratch-celthick-stage2"
SUBJECTS="zixxtrixx-idle zixxtrixx-walk zixxtrixx-attack zixxtrixx-fall zixxtrixx-hit zixxtrixx-death zixxtrixx-balance zixxtrixx-look zixxtrixx-knockdown zixxtrixx-hitfloor zixxtrixx-damage zixxtrixx-run zixxtrixx-death2 zixxtrixx-taunt zixxtrixx-salto-dummy zixxtrixx-salto-fly zixxtrixx-salto-six"
mkdir -p "$TMP" "$SITE/scratch-reel"
for subject in $SUBJECTS; do
  clip="${subject#zixxtrixx-}"
  alias="zixxtrixx-exp-celthick-$clip"
  echo "== $subject -> $alias"
  rm -rf "$TMP/$subject" "$SITE/scratch-reel/$alias"
  ZIXX_EXP=celthick "$REEL" "$TMP" "$subject"
  mv "$TMP/$subject" "$SITE/scratch-reel/$alias"
  python "$SITE/tools/tovideo.py" "$SITE" "$alias"
  python "$RUN/make_contact_sheets.py" "$alias"
  echo "DONE $alias"
done
echo "CELTHICK COLLECTION DONE"
