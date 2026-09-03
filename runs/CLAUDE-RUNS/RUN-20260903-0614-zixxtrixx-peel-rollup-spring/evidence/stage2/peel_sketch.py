#!/usr/bin/env python3
"""Centreline sketchpad for the peel pass (comparison side only).

Runs zixx-springpose `pose <entry> <squash>` for a list of samples and plots
the returned world-mm centrelines side by side with a ground line and body
radii, so tail-stand / collapsed candidates can be judged BY EYE before any
clip render. It chooses nothing; it draws what the authored tables produced.

Usage: python peel_sketch.py <springpose.exe> <out.png> <entry:squash> [...]
"""
import subprocess, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# rough per-station body radii (mm) for silhouette context, head -> tail tip;
# eyeballed from the model taper, context only, decides nothing
RADII = [120,130,140,145,150,150,150,150,145,140,135,130,125,120,110,95,75,55,35,15]

def sample(exe, e, q):
    out = subprocess.run([exe, "pose", str(e), str(q)],
                         capture_output=True, text=True).stdout
    pts = []
    for ln in out.splitlines():
        parts = ln.split()
        if len(parts) >= 3 and parts[0].isdigit():
            pts.append((int(parts[1]), int(parts[2])))
    return pts

def main():
    exe, png = sys.argv[1], sys.argv[2]
    combos = [tuple(int(v) for v in a.split(":")) for a in sys.argv[3:]]
    n = len(combos)
    fig, axes = plt.subplots(1, n, figsize=(6*n, 6), squeeze=False)
    for ax, (e, q) in zip(axes[0], combos):
        pts = sample(exe, e, q)
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        ax.plot(xs, ys, "-", lw=2, color="tab:blue")
        for i, (x, y) in enumerate(pts):
            r = RADII[i] if i < len(RADII) else 20
            ax.add_patch(plt.Circle((x, y), r, fill=False,
                                    color="tab:blue", alpha=0.35, lw=0.8))
            ax.annotate(str(i), (x, y), fontsize=7, color="tab:red")
        ax.axhline(0, color="k", lw=1)
        ax.set_title(f"entry={e} squash={q}")
        ax.set_aspect("equal")
        ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(png, dpi=110)
    print("wrote", png)

if __name__ == "__main__":
    main()
