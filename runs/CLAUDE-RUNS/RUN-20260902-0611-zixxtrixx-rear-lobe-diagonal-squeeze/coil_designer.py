#!/usr/bin/env python3
"""Collapsed-coil pose designer (SKETCH SIDE ONLY).

Draws candidate whole-body heading tables as centreline + tube sections so the
eye can choose the pose before it goes into zixxtrixx.h. The chain math mirrors
spring_support_origin_raw: segment k (bone k -> k+1) steps
(-seg*cos(h_k), -seg*sin(h_k)); positive heading descends going tailward.
Bone 14 is pinned at x = -1239 (the planted support); its y is a designer
input (becomes kSpringCollapsedSupportLiftMm - 34 in the header).

Nothing here generates shipped values; the chosen table is typed into the
header by hand and verified with zixx-springpose (the committed truth).
"""
import math, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

SEG = 3050 / 19.0            # 160.5 mm
SUPPORT_X = -1239.0
IDLE_NOSE_Y = 1083.0

# per-bone half-thickness (r_mm) from zixx-springpose dump (bone 0..19)
R = [41,125,161,166,168,164,158,156,155,153,151,148,145,142,138,121,96,73,46,23]

def chain(headings_deg, bone14_y):
    """headings: 19 values, degrees. Returns bone positions 0..19 with bone14
    pinned at (SUPPORT_X, bone14_y)."""
    assert len(headings_deg) == 19
    pts = [(0.0, 0.0)]
    for h in headings_deg:
        a = math.radians(h)
        x, y = pts[-1]
        pts.append((x - SEG * math.cos(a), y - SEG * math.sin(a)))
    dx = SUPPORT_X - pts[14][0]
    dy = bone14_y - pts[14][1]
    return [(x + dx, y + dy) for (x, y) in pts]

def plot(tables, out, flatten=0.43, spread=0.18, title=""):
    """tables: list of (name, headings_deg, bone14_y)."""
    n = len(tables)
    fig, axes = plt.subplots(1, n, figsize=(7.2 * n, 7.2), squeeze=False)
    for ax, (name, hs, y14) in zip(axes[0], tables):
        pts = chain(hs, y14)
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        # flattened tube sections (ellipse half-height r*(1-flatten))
        for b, (x, y) in enumerate(pts):
            rz = R[b] * (1 - flatten)
            rx = R[b] * (1 + spread)
            e = matplotlib.patches.Ellipse((x, y), 2 * rx, 2 * rz,
                                           fill=False, lw=0.7, color="tab:blue",
                                           alpha=0.6)
            ax.add_patch(e)
        ax.plot(xs, ys, "-", color="tab:red", lw=1.4)
        for b, (x, y) in enumerate(pts):
            c = "black" if b < 15 else "tab:green"
            ax.plot([x], [y], marker="o", ms=3, color=c)
            ax.annotate(str(b), (x, y), fontsize=7, xytext=(2, 3),
                        textcoords="offset points")
        ax.axhline(0, color="saddlebrown", lw=2)
        ax.axhline(IDLE_NOSE_Y, color="grey", lw=0.7, ls="--")
        ax.axhline(0.64 * IDLE_NOSE_Y, color="orange", lw=0.7, ls="--",
                   label="shipped 64%")
        ax.plot([SUPPORT_X], [y14], marker="x", ms=10, color="purple")
        # underside of the tail tubes (flattened) and worst non-neighbour gap
        under = min(pts[b][1] - R[b] * (1 - flatten) for b in range(14, 20))
        worst = 1e9
        wpair = None
        for a in range(20):
            for b in range(a + 3, 20):
                d = math.hypot(pts[a][0] - pts[b][0], pts[a][1] - pts[b][1])
                margin = d - (R[a] + R[b]) * (1 - flatten)
                if margin < worst:
                    worst = margin
                    wpair = (a, b)
        nose_pct = 100.0 * max(ys) / IDLE_NOSE_Y
        top = max(ys[b] + R[b] * (1 - flatten) for b in range(20))
        ax.set_title(f"{name}\nnose_y={ys[0]:.0f} top={top:.0f} "
                     f"({100*top/IDLE_NOSE_Y:.0f}% idle) tip=({xs[19]:.0f},{ys[19]:.0f})\n"
                     f"tail_under={under:.0f} worst_gap={worst:.0f} @{wpair}")
        ax.set_aspect("equal"); ax.grid(alpha=0.2)
        ax.set_xlim(-2400, 400); ax.set_ylim(-250, 1250)
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print("wrote", out)

if __name__ == "__main__":
    print("import as module; see design scripts")
