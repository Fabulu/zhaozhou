#!/bin/bash
# RUN 1939 close-out: runs AFTER the experiment factory completes.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
RUN="$ROOT/runs/CLAUDE-RUNS/RUN-20260828-1939-zixxtrixx-v7-six-faults"
SITE="$(cd "$ROOT/../Upheaval/website" && pwd)"

echo "== harvest: SEQUENCE-CRCS + golden contact sheets"
python "$RUN/harvest.py"

echo "== restore the shipping atlas preview (the experiment gens overwrote it)"
python "$ROOT/tools/pack/mkcreaturepage.py" > /dev/null

echo "== assemble the site page"
python "$SITE/tools/assemble.py" "$SITE"

echo "CLOSEOUT DONE"
