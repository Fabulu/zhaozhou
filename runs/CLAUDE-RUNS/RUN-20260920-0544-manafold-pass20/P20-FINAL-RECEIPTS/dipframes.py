#!/usr/bin/env python3
"""Find the frames to LOOK at by badness, not by index.

For each subject, the per-frame byte difference between the shipping bank and
the `rearonly` control (shipping bow, no beat, no reaction) is the size of the
kneading beat's effect on that frame. The largest is the deepest press. This
CHOOSES A FRAME TO LOOK AT; it judges nothing.
"""
import sys
from pathlib import Path

BASE = Path(r"C:\programmieren\zencrifice\manafold-p16")
SHIP = BASE / "p20-final-reel-22"
CTRL = BASE / "p20-scope-rearonly"

for subj in sys.argv[1:]:
    a, b = SHIP / f"manafold-{subj}", CTRL / f"manafold-{subj}"
    rows = []
    for f in sorted(a.glob("*.rgb")):
        x, y = f.read_bytes(), (b / f.name).read_bytes()
        n = sum(1 for i in range(0, len(x), 3) if x[i:i+3] != y[i:i+3])
        rows.append((n, int(f.stem)))
    rows.sort(reverse=True)
    worst = rows[0]
    moved = sum(1 for n, _ in rows if n)
    print(f"{subj}: frames moved {moved}/{len(rows)}, worst f{worst[1]} "
          f"({worst[0]} px of 92160), top5 {[f'f{i}:{n}' for n, i in rows[:5]]}")
