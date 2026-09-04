"""Direction 30 diagnostic: where does the sun clamp, and what colour is it?

Compares suns-ON frames against suns-OFF frames of the same subject/index.
The sun only touches creature compositing, so changed pixels ARE the sun's
influence region. Reports, per sampled frame and per subject:
  - changed pixel count
  - fraction of changed pixels NEWLY pegged per channel (on==255, off<255)
  - mean per-channel delta over changed pixels (the sun's average colour)
  - the 90th-percentile delta colour (the sun at its strongest)
Measurement guides; the eye chooses (CLAUDE.md art law).

PROMOTED to tools/reel/ on 2026-09-04. It was written into a run folder, which
CLAUDE.md names as the wrong home for anything durable -- every pass creates a
new run and orphans the last, which is how this project ended up with five
separate frame readers, two of them silently wrong. Import it; do not rewrite it.
"""
import glob
import os
import sys

import numpy as np

from rgbframe import load

def frames(d):
    return sorted(glob.glob(os.path.join(d, "*.rgb")))

def analyze(on_dir, off_dir, name, sample=6):
    fon, foff = frames(on_dir), frames(off_dir)
    if len(fon) != len(foff):
        print(f"{name}: FRAME COUNT MISMATCH on={len(fon)} off={len(foff)}")
        return
    n = len(fon)
    idxs = sorted(set(int(round(i * (n - 1) / (sample - 1))) for i in range(sample)))
    print(f"\n== {name} ({n} frames, sampling {idxs}) ==")
    agg_sat = []
    for i in idxs:
        a = load(fon[i]).astype(np.int16)
        b = load(foff[i]).astype(np.int16)
        changed = np.any(a != b, axis=2)
        nc = int(changed.sum())
        if nc == 0:
            print(f"  f{i:4d}: no change"); continue
        ca, cb = a[changed], b[changed]
        d = ca - cb
        sat = [((ca[:, c] == 255) & (cb[:, c] < 255)).mean() for c in range(3)]
        sat_all = np.all(ca == 255, axis=1).mean()
        mean_d = d.mean(axis=0)
        # strongest tenth of the sun's influence by delta magnitude
        mag = np.abs(d).sum(axis=1)
        top = mag >= np.percentile(mag, 90)
        top_d = d[top].mean(axis=0)
        agg_sat.append((sat, sat_all))
        print(f"  f{i:4d}: changed={nc:6d}  sat R/G/B/all="
              f"{sat[0]:.1%}/{sat[1]:.1%}/{sat[2]:.1%}/{sat_all:.1%}  "
              f"meanD=({mean_d[0]:+5.1f},{mean_d[1]:+5.1f},{mean_d[2]:+5.1f})  "
              f"top10%D=({top_d[0]:+5.1f},{top_d[1]:+5.1f},{top_d[2]:+5.1f})")

if __name__ == "__main__":
    on_root, off_root = sys.argv[1], sys.argv[2]
    subs = sys.argv[3:]
    for s in subs:
        analyze(os.path.join(on_root, s), os.path.join(off_root, s), s)
