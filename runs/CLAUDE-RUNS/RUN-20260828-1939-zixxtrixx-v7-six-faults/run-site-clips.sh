#!/bin/bash
# RUN 1939: re-render all 17 canonical site clips at the final state and
# encode them to their canonical names. Deploy is NOT run from here.
# (The reel creates <out>/<subject>/NNNN.rgb itself -- render from
# scratch-reel so tovideo finds the frames; the first cut nested them.)
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SITE="$(cd "$ROOT/../Upheaval/website" && pwd)"
REEL="$ROOT/build/tools/zhao-reel.exe"
SUBJECTS="zixxtrixx-idle zixxtrixx-walk zixxtrixx-attack zixxtrixx-fall zixxtrixx-hit zixxtrixx-death zixxtrixx-balance zixxtrixx-look zixxtrixx-knockdown zixxtrixx-hitfloor zixxtrixx-damage zixxtrixx-run zixxtrixx-death2 zixxtrixx-taunt zixxtrixx-salto-dummy zixxtrixx-salto-fly zixxtrixx-salto-six"
cd "$SITE/scratch-reel"
for s in $SUBJECTS; do
  echo "== $s"
  rm -rf "$s"
  "$REEL" . "$s" > /dev/null 2>&1
  python "$SITE/tools/tovideo.py" "$SITE" "$s"
done
echo "SITE CLIPS DONE"
