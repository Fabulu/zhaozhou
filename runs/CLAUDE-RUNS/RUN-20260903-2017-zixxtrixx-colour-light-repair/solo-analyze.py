"""Phase 1 analysis: does each solo source change ANY shading, where, how much,
and does its pool track its orb marker? All frames read header-verified."""
import os, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mlrgb import load, save_png, diff_vis

ROOT = os.path.dirname(os.path.abspath(__file__))
SC = os.path.join(ROOT, "scratch")
EV = os.path.join(ROOT, "evidence")
CORE = {  # exact marker core tints from kZixxMovingSourceMarker
    "warm": (255, 232, 170), "blue": (190, 220, 255),
    "orange": (255, 220, 180), "green": (200, 255, 190),
}

def frames(d):
    p = os.path.join(SC, d, "zixxtrixx-moving-light")
    return p, sorted(f for f in os.listdir(p) if f.endswith(".rgb"))

def orb_xy(arr, tint):
    m = np.all(arr == np.array(tint, np.uint8), axis=2)
    if m.sum() < 3: return None  # occluded or off screen
    ys, xs = np.nonzero(m)
    return float(xs.mean()), float(ys.mean())

def analyze(name, src):
    pdir, fl = frames(name)
    ndir, _ = frames("none")
    rows = []
    for f in fl:
        a = load(os.path.join(pdir, f))
        b = load(os.path.join(ndir, f))
        d = np.abs(a.astype(np.int16) - b.astype(np.int16))
        mask = d.sum(axis=2) > 6  # ignore 1-2 count dither noise
        n = int(mask.sum())
        row = {"f": int(f[:4]), "px": n,
               "max": [int(d[:, :, c].max()) for c in range(3)]}
        if n > 20:
            ys, xs = np.nonzero(mask)
            row["cx"], row["cy"] = float(xs.mean()), float(ys.mean())
        o = orb_xy(a, CORE[src])
        if o: row["ox"], row["oy"] = round(o[0], 1), round(o[1], 1)
        rows.append(row)
    return rows

def main():
    out = {}
    for name, src in [("warm1", "warm"), ("blue1", "blue"), ("orange1", "orange"),
                      ("green1", "green"), ("blue8", "blue"), ("orange8", "orange"),
                      ("green8", "green")]:
        if not os.path.isdir(os.path.join(SC, name)): continue
        rows = analyze(name, src)
        pxs = [r["px"] for r in rows]
        peak = max(rows, key=lambda r: r["px"])
        active = sum(1 for p in pxs if p > 50)
        out[name] = {"peak_frame": peak["f"], "peak_px": peak["px"],
                     "peak_maxdelta": peak["max"],
                     "frames_active(>50px)": active, "of": len(rows),
                     "mean_px": round(float(np.mean(pxs)), 1)}
        json.dump(rows, open(os.path.join(EV, f"solo-{name}-perframe.json"), "w"))
        # evidence at the peak frame
        fn = f"{peak['f']:04d}.rgb"
        a = load(os.path.join(SC, name, "zixxtrixx-moving-light", fn))
        b = load(os.path.join(SC, "none", "zixxtrixx-moving-light", fn))
        save_png(a, os.path.join(EV, f"solo-{name}-peak-f{peak['f']:04d}.png"), 2)
        save_png(b, os.path.join(EV, f"solo-{name}-peak-plate.png"), 2)
        save_png(diff_vis(a, b), os.path.join(EV, f"solo-{name}-peak-diff.png"), 2)
        print(name, json.dumps(out[name]))
    json.dump(out, open(os.path.join(EV, "solo-summary.json"), "w"), indent=1)

main()
