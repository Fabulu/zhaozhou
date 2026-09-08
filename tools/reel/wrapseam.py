#!/usr/bin/env python3
"""wrapseam.py -- can you find a clip's loop point by looking at it?

    python tools/reel/wrapseam.py <dir-A> [<dir-B>] [--tail 5] [--plate out.png]

Each <dir> is one rendered subject's frame directory. With two, they are
compared as an A/B of the same subject from two binaries.

WHAT IT MEASURES, AND WHY THIS AND NOT THE OBVIOUS THING
--------------------------------------------------------
`manafold-qa-p12` Q3 already reports a WRAP column, and it reads `Clip::root`
-- the AUTHORED KEYS. That number cannot move for a presentation-only fix, and
it *should* not: a travelling clip's last key really is a whole traverse from
key 0, and snapping back is what looping a travel cycle means.

The fault this measures is a different one, one key finer. The reel shows every
key TWICE (`anim_advance`: sub 0 then sub 1), and the sub-1 pose of the LAST
key is interpolated toward key 0. On a travelling clip that half-key blend is a
position the animation never occupies -- the creature appears mid-air, half way
back across its own traverse, for exactly one frame, and anything deriving
speed from the posed root paints a grey smear ghost beside it.

So: the mean absolute inter-frame difference across the whole clip, with the
wrap frame called out against the clip's OWN interior median and maximum. A
wrap frame that reads as an ordinary frame is the acceptance test, and it is
stated as a ratio to the clip's own motion rather than an absolute band,
because clips move at wildly different rates and a shared band would be a
number nobody derived (10-GATE-CHECKLIST section 0).

FRAME INDEXING, stated because it is easy to get wrong by one and then measure
the wrong frame: the reel advances THEN renders, so rendered frame i shows key
(i+1)/2 at sub (i+1)&1. For a clip of K keys rendered as n = 2K frames, the
WRAP BLEND is frame n-2 (key K-1, sub 1); frame n-1 is key 0 at sub 0, i.e.
the honest start of the next loop.

Frames are read through rgbframe.py. Do not write another frame reader
(PASS-13-PLAN section 0: a raw frombytes read rotates every channel).
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgbframe import load, save_png  # noqa: E402


def frame_paths(d):
    return [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith(".rgb")]


def measure(d):
    paths = frame_paths(d)
    if len(paths) < 6:
        raise SystemExit("%s: %d frames -- too few to talk about a seam" % (d, len(paths)))
    arrs = [load(p) for p in paths]
    n = len(arrs)
    dl = [float(np.abs(arrs[i + 1].astype(np.int16) - arrs[i].astype(np.int16)).mean())
          for i in range(n - 1)]
    interior = np.array(dl[:n - 3])  # everything before the seam pair
    return {
        "dir": d, "n": n, "arrs": arrs, "deltas": dl,
        "median": float(np.median(interior)),
        "p95": float(np.percentile(interior, 95)),
        "max": float(interior.max()), "max_at": int(interior.argmax()),
        "into_wrap": dl[n - 3], "out_of_wrap": dl[n - 2],
    }


def report(m, label):
    med = m["median"] or 1e-9
    print("  %-10s n=%d   interior median %.3f   p95 %.3f   MAX interior %.3f at f%d"
          % (label, m["n"], m["median"], m["p95"], m["max"], m["max_at"]))
    print("             INTO the wrap frame  f%d->f%d  %8.3f  = %6.2fx median  %s"
          % (m["n"] - 3, m["n"] - 2, m["into_wrap"], m["into_wrap"] / med,
             "<-- SEAM VISIBLE" if m["into_wrap"] > max(m["max"], 2 * med) else "in band"))
    print("             out of the wrap      f%d->f%d  %8.3f  = %6.2fx median"
          % (m["n"] - 2, m["n"] - 1, m["out_of_wrap"], m["out_of_wrap"] / med))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--plate")
    ap.add_argument("--tail", type=int, default=5)
    args = ap.parse_args()

    ms = [measure(d) for d in args.dirs]
    for m, d in zip(ms, args.dirs):
        report(m, os.path.basename(os.path.dirname(d)) or "A")

    if args.plate:
        rows = []
        for m in ms:
            picks = m["arrs"][-args.tail:]
            row = np.concatenate(picks, axis=1)
            rows.append(row)
            rows.append(np.full((3, row.shape[1], 3), 255, np.uint8))
        save_png(np.concatenate(rows[:-1], axis=0), args.plate)
        print("  plate: %s  (last %d frames per row, one row per dir)"
              % (args.plate, args.tail))
    return 0


if __name__ == "__main__":
    sys.exit(main())
