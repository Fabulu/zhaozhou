"""Pass 21 architect: turn the curvature probe's CSV into the pictures and
tables that LOCATE the extra hinges.

    python curvature_report.py curv.csv OUTDIR

For every slot in the CSV:
  * OUTDIR/turn-heat-<slot>.png   ring (x) by 60 Hz sample (y): centreline
                                  turning angle per ring. Bright columns are
                                  where the band bends; a smooth run is a dim
                                  even band, a hinge is a bright stripe.
  * OUTDIR/rate-heat-<slot>.png   |d turn / d sample| -- where the bend MOVES.
                                  A spazz is a bright stripe here.
  * OUTDIR/shear-heat-<slot>.png  ring plane vs centreline tangent.
  * a per-ring table on stdout: station mm, max turn, mean turn, max rate,
    max shear, with the carrier rings marked.
Carrier rings are the rings nearest the authored stations (JF 320, A 930,
B 1270, C 1650, End 2660 on a 2930 mm band of 64 rings).
"""
import sys, os, csv
import numpy as np

RINGS = 64
TOTAL = 2930
STATIONS = {"JF": 320, "A": 930, "B": 1270, "C": 1650, "End": 2660}


def ring_of(mm):
    return int((mm * (RINGS - 1) + TOTAL / 2) // TOTAL)


def station_of(r):
    return TOTAL * r // (RINGS - 1)


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    rows = list(csv.reader(open(src)))
    hdr, rows = rows[0], rows[1:]
    ti = hdr.index("turn0")
    si = hdr.index("shear0")
    ci = hdr.index("cx0")
    by_slot = {}
    for r in rows:
        by_slot.setdefault(int(r[0]), []).append(r)
    carriers = {name: ring_of(mm) for name, mm in STATIONS.items()}
    print("carrier rings:", carriers)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception:
        plt = None
    summary = {}
    for slot, rs in sorted(by_slot.items()):
        turn = np.array([[float(v) for v in r[ti:ti + RINGS]] for r in rs])
        shear = np.array([[float(v) for v in r[si:si + RINGS]] for r in rs])
        cen = np.array([[float(v) for v in r[ci:ci + 3 * RINGS]] for r in rs]).reshape(-1, RINGS, 3)
        rate = np.abs(np.diff(turn, axis=0))
        # per-ring centroid speed, mm per 60 Hz sample (position jitter)
        speed = np.linalg.norm(np.diff(cen, axis=0), axis=2)
        print(f"\n=== slot {slot}: {len(rs)} samples ===")
        print("ring  station  maxturn meanturn  maxrate  meanrate  maxshear  maxspeed  carrier")
        for i in range(1, RINGS - 1):
            mark = ""
            for name, cr in carriers.items():
                if cr == i:
                    mark = "<-- " + name
            print(f"{i:4d} {station_of(i):8d} {turn[:, i].max():8.1f} {turn[:, i].mean():8.1f}"
                  f" {rate[:, i].max():8.2f} {rate[:, i].mean():9.3f} {shear[:, i].max():9.1f}"
                  f" {speed[:, i].max():9.1f}  {mark}")
        # concentration per run: how much of the run's total turn sits in its
        # single worst ring, averaged over samples
        runs = [("JF..A", carriers["JF"] + 1, carriers["A"] - 3),
                ("A", carriers["A"] - 3, carriers["A"] + 3),
                ("A..B", carriers["A"] + 3, carriers["B"] - 3),
                ("B", carriers["B"] - 3, carriers["B"] + 3),
                ("B..C", carriers["B"] + 3, carriers["C"] - 3),
                ("C", carriers["C"] - 3, carriers["C"] + 3),
                ("C..End", carriers["C"] + 3, carriers["End"] - 3),
                ("End", carriers["End"] - 3, carriers["End"] + 3)]
        print("\nwindow      rings    mean total turn   max total turn   mean max-ring share")
        for name, a, b in runs:
            w = turn[:, a:b]
            tot = w.sum(axis=1)
            share = np.where(tot > 1e-6, w.max(axis=1) / np.maximum(tot, 1e-6), 0)
            print(f"{name:10s} {a:3d}-{b:<3d} {tot.mean():14.1f} {tot.max():16.1f} {share.mean():16.2f}")
        # worst samples by between-carrier turn (the extra-hinge score)
        between = np.zeros(len(rs))
        for name, a, b in runs:
            if ".." in name:
                between = np.maximum(between, turn[:, a:b].max(axis=1))
        order = np.argsort(-between)[:12]
        print("\nworst samples by max BETWEEN-carrier ring turn (frame.sub, deg, ring):")
        for k in order:
            r = rs[k]
            i = int(np.argmax([turn[k, a:b].max() if ".." in n else 0 for n, a, b in runs]))
            n, a, b = runs[i]
            ring = a + int(np.argmax(turn[k, a:b]))
            print(f"  {r[1]}.{r[2]}  {between[k]:6.1f}  ring {ring} ({station_of(ring)} mm, {n})")
        endrate = rate[:, carriers["C"] + 3:RINGS - 2]
        order = np.argsort(-endrate.max(axis=1))[:8]
        print("\nworst samples by REAR turn rate (frame.sub, deg/sample, ring):")
        for k in order:
            r = rs[k + 1]
            ring = carriers["C"] + 3 + int(np.argmax(endrate[k]))
            print(f"  {r[1]}.{r[2]}  {endrate[k].max():6.2f}  ring {ring} ({station_of(ring)} mm)")
        summary[slot] = (turn, rate, shear)
        if plt is None:
            continue
        for name, arr, vmax in (("turn", turn, 60), ("rate", rate, 8), ("shear", shear, 60)):
            fig, ax = plt.subplots(figsize=(10, 6), dpi=100)
            im = ax.imshow(arr, aspect="auto", cmap="magma", vmin=0, vmax=vmax,
                           interpolation="nearest")
            for cname, cr in carriers.items():
                ax.axvline(cr, color="cyan", lw=0.6, alpha=0.7)
                ax.text(cr, -2, cname, color="cyan", fontsize=8, ha="center")
            ax.set_xlabel("ring (0 = buried base ... 63 = buried tip); cyan = carrier ball")
            ax.set_ylabel("60 Hz sample")
            ax.set_title(f"slot {slot}: per-ring {name} (deg{'/sample' if name == 'rate' else ''}), clip {vmax} deg")
            fig.colorbar(im, ax=ax)
            fig.tight_layout()
            fig.savefig(os.path.join(outdir, f"{name}-heat-{slot}.png"))
            plt.close(fig)
        # per-ring profile: max and mean turn along the band
        fig, ax = plt.subplots(figsize=(10, 4), dpi=100)
        x = np.arange(RINGS)
        ax.plot(x, turn.max(axis=0), label="max turn over clip")
        ax.plot(x, turn.mean(axis=0), label="mean turn")
        ax.plot(x, rate.max(axis=0) * 5, label="max rate x5 (deg/sample)")
        for cname, cr in carriers.items():
            ax.axvline(cr, color="grey", lw=0.6)
            ax.text(cr, ax.get_ylim()[1] * 0.95, cname, ha="center", fontsize=8)
        ax.set_xlabel("ring")
        ax.set_ylabel("deg")
        ax.set_title(f"slot {slot}: where the band bends and where the bend moves")
        ax.legend(fontsize=8)
        fig.tight_layout()
        fig.savefig(os.path.join(outdir, f"turn-profile-{slot}.png"))
        plt.close(fig)


if __name__ == "__main__":
    main()
