import importlib.util, sys
def load(p, n):
    s = importlib.util.spec_from_file_location(n, p)
    m = importlib.util.module_from_spec(s); s.loader.exec_module(m); return m
sc = load("stand_candidates.py", "sc"); cn = load(sys.argv[1], "cn")
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
def planted_at(head, plant):
    st, px, py = plant
    pts = sc.walk(head)
    i = int(st); f = st - i
    ax_ = pts[i][0] + ((pts[i+1][0]-pts[i][0])*f if f > 0 else 0)
    ay_ = pts[i][1] + ((pts[i+1][1]-pts[i][1])*f if f > 0 else 0)
    return [(x + px-ax_, y + py-ay_) for (x, y) in pts]
n = len(cn.CANDIDATES)
fig, axes = plt.subplots(1, n, figsize=(8*n, 8), squeeze=False)
raw = sc.walk(sc.STANCE)
sdx = -1239 - raw[14][0]; sdy = 144 - raw[14][1]
stance_pts = [(x+sdx, y+sdy) for (x, y) in raw]
for ax, (name, head, plant) in zip(axes[0], cn.CANDIDATES):
    sc.draw(ax, stance_pts, "lightgray", "stance", radii=False)
    pts = planted_at(head, plant)
    sc.draw(ax, pts, "tab:blue", name)
    nose = pts[0]
    ax.plot([nose[0]], [nose[1]], "r*", ms=14)
    ax.plot([plant[1]], [plant[2]], "gs", ms=8)
    ax.axhline(0, color="k", lw=1); ax.axvline(-1925, color="tab:green", lw=0.8, ls="--")
    ax.set_title(f"{name} nose=({nose[0]:.0f},{nose[1]:.0f})")
    ax.set_aspect("equal"); ax.grid(True, alpha=0.3)
fig.tight_layout(); fig.savefig(sys.argv[2], dpi=100); print("wrote", sys.argv[2])
