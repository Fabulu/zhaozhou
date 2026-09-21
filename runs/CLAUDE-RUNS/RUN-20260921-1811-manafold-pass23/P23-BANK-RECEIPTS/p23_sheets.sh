#!/usr/bin/env bash
# Complete EVERY-FRAME contact sheets for all 22 accepted subjects, from the
# accepted frame root, via the committed tools/reel/plates.py. No sampling:
# CLAUDE.md is explicit that uniform sampling finds the typical frame and
# misses the broken one.
#
# ⚠ WINDOWS PATHS, deliberately. plates.py globs the pattern in PYTHON, and
# Python cannot resolve an MSYS `/c/...` path -- the glob comes back EMPTY and
# the tool dies on an empty tile list. The first version of this script used
# `$SRC`/*/ from Git Bash and produced 0 sheets and RC 1 for exactly that
# reason. An empty glob that crashes is the good case; one that quietly wrote
# a 0-frame sheet would have been a "complete every-frame sheet" of nothing.
cd /c/programmieren/zencrifice/manafold-p16/zhaozhou
SRC=C:/programmieren/zencrifice/manafold-p16/p23-final-reel-22
OUT=C:/programmieren/zencrifice/manafold-p16/p23-sheets
mkdir -p "$OUT"
rc=0
for d in /c/programmieren/zencrifice/manafold-p16/p23-final-reel-22/*/; do
  s=$(basename "$d")
  n=$(ls "$d"*.rgb | wc -l)
  python tools/reel/plates.py sheet "$OUT/$s-everyframe.png" 1 30 "$SRC/$s/*.rgb" \
    >> "$OUT/sheets.log" 2>&1 || rc=1
  # and prove the sheet really holds EVERY frame, not merely that it was written
  python - "$OUT/$s-everyframe.png" "$n" <<'PY' >> "$OUT/sheets.log" 2>&1 || rc=1
import sys
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
p, n = sys.argv[1], int(sys.argv[2])
w, h = Image.open(p).size
cols = min(30, n); rows = (n + 29) // 30
exp_w, exp_h = cols * 384 + (cols + 1), rows * 240 + 14 * rows + rows + 1
print(f"{p}: {n} frames, {w}x{h} (expect ~{exp_w}x{exp_h})")
assert w >= cols * 384 and h >= rows * 240, f"sheet too small for {n} frames"
PY
done
echo "SHEETS_RC=$rc" | tee -a "$OUT/sheets.log"
