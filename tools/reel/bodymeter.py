"""Body-pigment meter for creature passes: how much of the lit skin is CLIPPED,
and where the hue actually sits — measured inside an EXACT creature mask.

WHY THE MASK IS BUILT THIS WAY
------------------------------
The obvious mask is a colour rule ("pinkish pixels are the body"). Four
diagnostics on this creature have already been confidently wrong, and two of
them were exactly that: a mask that matched the orange horizon band and claimed
15 of 16 clips were edge-clipped in every frame, and a badness metric that
measured hue when the question was coverage. A colour mask cannot tell a hot-red
lit flank from the terrain, which is the very pixel this meter exists to count.

So the mask is DIFFERENTIAL and exact: render the subject twice from one binary,
once normally and once with ZIXX_HIDE_CREATURE=1, and take every pixel that
changed. That is the creature, by construction, with no threshold to tune.

THIS IS A COMPARISON-SIDE TOOL. It reports "you are N% clipped"; it never
chooses a colour. The shipped pink is chosen by looking at it in scene, at
native, against the sheets (CLAUDE.md, the art law).

    python bodymeter.py <lit-dir> <hidden-dir> [frames...]
"""
import sys
import numpy as np
from rgbframe import load


def mask_of(lit, bg):
    """Exact creature mask: the pixels the creature hook changed."""
    return (lit.astype(np.int16) - bg.astype(np.int16)).any(axis=2)


def measure(lit, bg):
    m = mask_of(lit, bg)
    n = int(m.sum())
    if n == 0:
        return None
    px = lit[m].astype(np.int32)
    r, g, b = px[:, 0], px[:, 1], px[:, 2]
    mx, mn = px.max(axis=1), px.min(axis=1)
    sat = np.where(mx > 0, (mx - mn) * 255 // np.maximum(mx, 1), 0)
    return {
        "px": n,
        # the clip fraction, per channel and any-channel: the pass-5 finding was
        # 49-57% of lit pink at red 255, which carries no form at all
        "clip_r_pct": round(100.0 * float((r >= 255).sum()) / n, 1),
        "clip_any_pct": round(100.0 * float((mx >= 255).sum()) / n, 1),
        # how dark the skin has gone -- the moving rig's other failure mode
        "dark_pct": round(100.0 * float((mx < 90).sum()) / n, 1),
        "mean_rgb": [int(r.mean()), int(g.mean()), int(b.mean())],
        "mean_val": int(mx.mean()),
        "mean_sat": int(sat.mean()),
        # is it still MAGENTA-pink, or has it swung to fire-red? on the sheet
        # the pink is a strong magenta: blue clearly above green.
        "b_minus_g": int(b.mean() - g.mean()),
    }


def main():
    lit_dir, hid_dir = sys.argv[1], sys.argv[2]
    frames = sys.argv[3:] or ["0000", "0100", "0210"]
    for f in frames:
        try:
            res = measure(load(f"{lit_dir}/{f}.rgb"), load(f"{hid_dir}/{f}.rgb"))
        except FileNotFoundError as e:
            print(f"{f}: missing ({e.filename})")
            continue
        print(f"{f}: {res}")


if __name__ == "__main__":
    main()
