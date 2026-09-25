#!/usr/bin/env python3
"""seamdisp.py -- WHERE does a looping clip actually jump, and by how much?

    python tools/reel/seamdisp.py <frame-dir> [<frame-dir> ...] [--top 5]
    python tools/reel/seamdisp.py absent <frame.rgb> [<frame.rgb> ...]
    python tools/reel/seamdisp.py selftest

WHY THIS TOOL EXISTS (pass-26 review)
-------------------------------------
The pass-25 loop-seam figure -- 38.8 px, quoted in two run folders, accepted by
the owner in Direction 27 -- is the horizontal centroid of a "pink ink" colour
rule, `(r>150) & (r-g>60) & (b-g>20)`, at frame 0 versus the last frame.

THAT RULE FAILS ITS KNOWN-NEGATIVE. Paint the creature's bounding box out with a
sky pixel and re-score, and on a pass-25 hasty frame it still returns **4,073 of
its 4,977 pixels** -- 82% of what it measures is the dusty-rose sky and terrain,
not the creature. On the pass-26 framing it returns MORE with the creature gone
(17,421 px) than with it present (5,695 px). The number it produces is a
background centroid with a creature-shaped perturbation in it.

This is CLAUDE.md's standing failure, and note how it arrived: the pass-26 pass
correctly ran a known-negative on its NEW chroma mask, found it broken, and
fixed it -- and then inherited the old pink-ink mask unchecked, because it was
"the pass-25 method copied exactly". **Copying a method exactly copies its
defects exactly.** A metric does not become trustworthy by being the one used
last time.

AND THE TWO-FRAME METRIC CANNOT SEE THE JUMP ANY MORE
-----------------------------------------------------
`f0 vs f_last` samples exactly two frames. The reel advances then renders, so
rendered frame i shows key (i+1)/2 and the LAST frame shows key 0 -- the start
of the next lap. Clock the camera's aim by the animation's phase (pass 26's
`SceneSubject::cam_bias_x_wrap_keys`) and those two frames come into agreement,
so the metric reports 0.0 px -- while the discontinuity simply moves to
`f238 -> f239`, one frame earlier, where the metric never looks.

That is the "gate that cannot reach the state" pattern. So this tool walks
EVERY adjacent pair, including the wrap, and reports the largest -- it cannot be
satisfied by making two chosen frames agree.

WHAT IT MEASURES
----------------
  * `creature dx`  horizontal centroid of a saturation mask (sat >= SAT_MIN),
                   which scores **0 px** on a creature-removed frame.
  * `terrain dy`   horizon row per column with the creature's columns excluded,
                   so a ground snap is reported SEPARATELY from a body jump.
                   "The creature holds and the ground moves" is a claim this
                   tool can check; a whole-frame difference cannot.

Rows are rendered-frame order, and the pair `(n-1, 0)` is the loop point.

ON THE COMPARISON SIDE ONLY (CLAUDE.md). This chooses no traverse and no seam
budget. It answers "where is the jump, how big is it, and is it the creature or
the ground".
"""
import argparse
import glob
import os
import sys

import numpy as np

from rgbframe import load

# Saturation floor for the creature mask. Manafold is vivid magenta with cyan
# eyes and a white-blue bolt; the staging's dusty-rose sky and brown terrain top
# out well below this. VALIDATED by the `absent` subcommand -- do not move it
# without re-running that.
SAT_MIN = 100

# A sky->ground step in the green channel. The terrain is darker than the sky in
# every Manafold staging.
HORIZON_DROP = 12


def creature_mask(a):
    return (a.max(2) - a.min(2)) >= SAT_MIN


def creature_cx(a):
    m = creature_mask(a)
    ys, xs = np.nonzero(m)
    if len(xs) == 0:
        return float("nan"), 0
    return float(xs.mean()), int(len(xs))


def horizon(a, step=4, guard=6):
    """Terrain horizon row per column, creature columns EXCLUDED.

    Excluding them matters: with the creature's columns left in, the only
    strongly moving thing in the picture is the creature, and a ground measure
    that includes it reports the body's motion as the ground's.
    """
    g = a[:, :, 1].astype(np.int32)
    cre = creature_mask(a).any(0)
    out = {}
    for x in range(0, a.shape[1], step):
        lo = max(0, x - guard)
        if cre[lo:x + guard].any():
            continue
        col = g[:, x]
        d = np.nonzero(col[1:] - col[:-1] < -HORIZON_DROP)[0]
        if len(d):
            out[x] = float(d[0])
    return out


def scan(d):
    files = sorted(glob.glob(os.path.join(d, "*.rgb")))
    if not files:
        raise SystemExit("seamdisp: no .rgb frames in %s" % d)
    frames = [load(p).astype(np.int16) for p in files]
    n = len(frames)
    cx = [creature_cx(a) for a in frames]
    hz = [horizon(a) for a in frames]
    rows = []
    for i in range(n):
        j = (i + 1) % n
        dx = cx[j][0] - cx[i][0]
        ks = sorted(set(hz[i]) & set(hz[j]))
        dy = [hz[j][k] - hz[i][k] for k in ks]
        rows.append({
            "i": i, "j": j,
            "creature_dx": dx,
            "terrain_dy": float(np.mean(dy)) if dy else 0.0,
            "terrain_cols": len(ks),
            "is_loop": j == 0,
        })
    return n, cx, rows


def report(d, top):
    n, cx, rows = scan(d)
    interior = [r for r in rows if not r["is_loop"]]
    mags = sorted(rows, key=lambda r: -abs(r["creature_dx"]))
    med = float(np.median([abs(r["creature_dx"]) for r in rows]))
    print("-- %s" % d)
    print("   frames            %d" % n)
    print("   creature mask     %d .. %d px, empty on %d frame(s)"
          % (min(p for _, p in cx), max(p for _, p in cx),
             sum(1 for _, p in cx if p == 0)))
    print("   median |dx|       %.2f px   (the clip's own motion)" % med)
    print("   LOOP POINT f%d->f0  creature dx %+7.2f px   terrain dy %+5.2f px"
          % (n - 1, rows[-1]["creature_dx"], rows[-1]["terrain_dy"]))
    print("   largest jumps, ANY adjacent pair (the wrap included):")
    for r in mags[:top]:
        print("      f%03d->f%03d  creature dx %+8.2f px  = %5.1fx median"
              "   terrain dy %+6.2f px %s"
              % (r["i"], r["j"], r["creature_dx"],
                 abs(r["creature_dx"]) / med if med else 0.0,
                 r["terrain_dy"], "  <-- LOOP POINT" if r["is_loop"] else ""))


def absent(paths):
    """THE KNOWN-NEGATIVE. Score the mask on frames with the creature removed.

    CLAUDE.md: before trusting any presence metric, run it on a frame where the
    answer is NO. This is the check the pink-ink seam mask never had.
    """
    bad = 0
    for p in paths:
        a = load(p).astype(np.int16)
        full = int(creature_mask(a).sum())
        b = a.copy()
        ys, xs = np.nonzero(creature_mask(a))
        if len(xs):
            b[max(0, ys.min() - 6):ys.max() + 7,
              max(0, xs.min() - 6):xs.max() + 7] = a[2, 2]
        gone = int(creature_mask(b).sum())
        ok = gone == 0
        bad += 0 if ok else 1
        print("%-52s full %7d px   creature REMOVED %7d px   %s"
              % (os.path.basename(p), full, gone, "ok" if ok else "*** LEAKS ***"))
    return 1 if bad else 0


def selftest():
    """Three legs. The middle one is the reason the tool exists."""
    rng = np.random.default_rng(7)
    W, H, N = 384, 240, 24

    def frame(cx_, ground_row):
        a = np.zeros((H, W, 3), np.int16)
        a[:, :] = (198, 150, 158)                       # dusty-rose sky
        a[ground_row:, :] = (110, 86, 64)               # terrain
        a[100:140, int(cx_) - 20:int(cx_) + 20] = (220, 30, 150)  # creature
        return a

    fails = []

    # A. a clip that never jumps -- the largest pair must be ~ the per-frame rate
    rows = scan_frames([frame(100 + 2 * i, 150) for i in range(N)])
    mx = max(abs(r["creature_dx"]) for r in rows[:-1])
    print("A steady travel   %s  max interior |dx| %.2f (want ~2)"
          % ("PASS" if 1.5 < mx < 2.5 else "FAIL", mx))
    if not 1.5 < mx < 2.5:
        fails.append("A")

    # B. THE LEG THIS TOOL EXISTS FOR. f0 and f_last agree exactly -- so the
    # two-frame metric reports a perfect seam -- while a 60 px jump sits at
    # f(n-2)->f(n-1), where that metric never looks. The tool must find it.
    seq = [frame(100 + 2 * i, 150) for i in range(N - 1)] + [frame(100, 150)]
    rows = scan_frames(seq)
    two_frame = abs(seq_cx(seq[0]) - seq_cx(seq[-1]))
    worst = max(rows, key=lambda r: abs(r["creature_dx"]))
    hit = worst["i"] == N - 2 and abs(worst["creature_dx"]) > 30
    print("B hidden jump     %s  two-frame metric says %.1f px; "
          "tool finds %.1f px at f%d->f%d"
          % ("PASS" if hit else "FAIL", two_frame,
             worst["creature_dx"], worst["i"], worst["j"]))
    if not hit:
        fails.append("B")

    # C. the ground moves and the creature does not -- creature dx ~0, terrain
    # dy large. "The creature holds and the ground snaps" must be checkable.
    seq = [frame(100, 150 + (10 if i == N - 1 else 0)) for i in range(N)]
    rows = scan_frames(seq)
    worstc = max(abs(r["creature_dx"]) for r in rows)
    worstt = max(abs(r["terrain_dy"]) for r in rows)
    ok = worstc < 0.5 and worstt > 5
    print("C ground-only     %s  creature max |dx| %.2f   terrain max |dy| %.2f"
          % ("PASS" if ok else "FAIL", worstc, worstt))
    if not ok:
        fails.append("C")

    # D. the known-negative, on a synthetic frame
    a = frame(100, 150)
    b = a.copy()
    b[100:140, 80:120] = (198, 150, 158)
    leak = int(creature_mask(b).sum())
    print("D known-negative  %s  creature removed -> %d px"
          % ("PASS" if leak == 0 else "FAIL", leak))
    if leak:
        fails.append("D")

    print("selftest: %s" % ("PASS" if not fails else "FAIL " + ",".join(fails)))
    return 1 if fails else 0


def seq_cx(a):
    return creature_cx(a)[0]


def scan_frames(frames):
    n = len(frames)
    cx = [creature_cx(a) for a in frames]
    hz = [horizon(a) for a in frames]
    rows = []
    for i in range(n):
        j = (i + 1) % n
        ks = sorted(set(hz[i]) & set(hz[j]))
        dy = [hz[j][k] - hz[i][k] for k in ks]
        rows.append({"i": i, "j": j,
                     "creature_dx": cx[j][0] - cx[i][0],
                     "terrain_dy": float(np.mean(dy)) if dy else 0.0,
                     "is_loop": j == 0})
    return rows


def main(argv):
    if len(argv) > 1 and argv[1] == "selftest":
        return selftest()
    if len(argv) > 2 and argv[1] == "absent":
        return absent(argv[2:])
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--top", type=int, default=5)
    a = ap.parse_args(argv[1:])
    for d in a.dirs:
        report(d, a.top)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
