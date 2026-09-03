#!/usr/bin/env python3
"""Tail-stand / collapsed candidate sketchpad (authoring side: BY EYE).

Walks candidate absolute-heading tables (degrees, head->tail) exactly the way
the reel walks them (station b -> b+1 advances (-cos h[b], -sin h[b]) * seg),
plants the TAIL TIP at the stance tip's baseline x with the tip resting on
the ground, and draws the result with body radii and the stance S for
comparison. Pure sketchpad: the chosen values are then authored as named
constants in zixxtrixx.h and verified through springpose, the probe and
rendered pixels.
"""
import math, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SEG = 3050.0 / 19.0
TIP_X, TIP_Y = -1925.0, 25.0   # stance tip baseline x; tip radius ~25 on the dirt
RADII = [120,130,140,145,150,150,150,150,145,140,135,130,125,120,110,95,75,55,35,15]

STANCE = [4.9,7.0,13.1,23.2,37.4,80.2,117.6,138.4,109.9,63.7,
          0.0,1.2,1.0,1.7,6.4,9.3,8.2,-30.8,-62.6]

def walk(head_deg):
    pts = [(0.0, 0.0)]
    x, y = 0.0, 0.0
    for h in head_deg:
        r = math.radians(h)
        x -= SEG * math.cos(r)
        y -= SEG * math.sin(r)
        pts.append((x, y))
    return pts

def planted(head_deg):
    pts = walk(head_deg)
    dx = TIP_X - pts[-1][0]
    dy = TIP_Y - pts[-1][1]
    return [(x + dx, y + dy) for (x, y) in pts]

def draw(ax, pts, color, label, radii=True):
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    ax.plot(xs, ys, "-", lw=2, color=color, label=label)
    if radii:
        for i, (x, y) in enumerate(pts):
            r = RADII[i] if i < len(RADII) else 20
            ax.add_patch(plt.Circle((x, y), r, fill=False, color=color,
                                    alpha=0.3, lw=0.8))
        for i, (x, y) in enumerate(pts):
            ax.annotate(str(i), (x, y), fontsize=7, color="black")

def main():
    import importlib.util
    spec = importlib.util.spec_from_file_location("cand", sys.argv[1])
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    combos = m.CANDIDATES  # list of (name, headings)
    n = len(combos)
    fig, axes = plt.subplots(1, n, figsize=(7*n, 7), squeeze=False)
    stance_pts = planted(STANCE)  # stance planted at same tip for scale ref? no:
    # stance belongs on its own baseline: root at (0, ~1083-ish). Use raw walk
    # shifted so station-14 sits at its baseline (-1239, 144).
    raw = walk(STANCE)
    sdx = -1239 - raw[14][0]; sdy = 144 - raw[14][1]
    stance_pts = [(x + sdx, y + sdy) for (x, y) in raw]
    for ax, (name, head) in zip(axes[0], combos):
        draw(ax, stance_pts, "lightgray", "stance", radii=False)
        pts = planted(head)
        draw(ax, pts, "tab:blue", name)
        nose = pts[0]
        ax.plot([nose[0]], [nose[1]], "r*", ms=14)
        ax.axhline(0, color="k", lw=1)
        ax.axvline(TIP_X, color="tab:green", lw=0.8, ls="--")
        ax.set_title(f"{name}  nose=({nose[0]:.0f},{nose[1]:.0f}) "
                     f"nose-tip dx={nose[0]-TIP_X:.0f}")
        ax.set_aspect("equal")
        ax.grid(True, alpha=0.3)
        ax.legend(loc="upper left", fontsize=8)
    fig.tight_layout()
    fig.savefig(sys.argv[2], dpi=100)
    print("wrote", sys.argv[2])

if __name__ == "__main__":
    main()
