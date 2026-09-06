"""Antenna hinge trajectory plot (Manafold pass 7, Owner Direction 5 SS2a:
"do the antenna hinges move separately, with a real range of motion?").

Companion to trajplot.py, but reads bone positions from the POSED 3D
SKELETON (manafold_hinge_traj.exe's CSV, itself built on zc::decode_pose)
instead of from rendered pixels. CLAUDE.md records a fault where a
measurement taken off a rendered frame gave a confident, wrong answer (the
"94.8% submerged" terrain-clearance case) -- a camera-space read cannot even
separate hinge rotation from an orbiting camera, which is exactly why SS2a
was refused a score twice. This tool cannot make that mistake because it
never looks at a frame: the input CSV is decode_pose output.

Input: the CSV emitted by manafold-hinge-traj.exe --
  frame,sub,bone,x_world_mm,y_world_mm,z_world_mm,
    x_local_mm,y_local_mm,z_local_mm,x_own_mm,y_own_mm,z_own_mm
for the six antenna bones (junctionF, neck, hingeA, hingeB, hingeC, hingeD).

THREE columns, not two -- WORLD, LOCAL and OWN -- because a bone's own
rotation never moves its own origin (only its DESCENDANTS'), so a bone's
ROOT-LOCAL position is the CUMULATIVE effect of every ANCESTOR's rotation,
not a clean read of that one joint. OWN re-bases a point one segment further
out into the bone's immediate PARENT's frame, isolating that joint's own
contribution (see manafold_hinge_traj.cpp's header for the full derivation).
OWN is the number this tool trusts for "does hinge X move, and separately
from its neighbours" -- LOCAL and WORLD are kept on the plot because seeing
the cumulative and raw reads is how the OWN finding gets checked.

Output: <out_png> -- one figure, three columns (OWN, LOCAL, WORLD), six rows
(one per hinge), each row plotting that hinge's x/y/z over presentation time
(frame*2+sub, i.e. the real 60 Hz sample order including sub-key
midpoints). A flat OWN line IS "this hinge never rotates".

Also prints, per hinge, the OWN peak-to-peak travel per axis and the
combined 3D range (max pairwise distance) in mm, plus pairwise OWN-motion
correlation between hinges -- the numbers Owner Direction 5 SS2a asks for.

`python hinge_trajplot.py selftest` proves this can distinguish a
non-flat/independent signal from a flat/correlated one on synthetic input
(the can-fail proof CLAUDE.md's "Seeing the work properly" section asks
every trajectory instrument to carry).

Usage: python hinge_trajplot.py <csv> <out_png>
       python hinge_trajplot.py selftest
"""
import csv
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BONES = ["junctionF", "neck", "hingeA", "hingeB", "hingeC", "hingeD"]
COLS = ["own", "local", "world"]


def load(csv_path):
    """-> {bone: {'t': [...], 'own': Nx3, 'local': Nx3, 'world': Nx3}},
    sorted by (frame, sub)."""
    rows = []
    with open(csv_path, newline="") as f:
        for r in csv.DictReader(f):
            rows.append(r)
    rows.sort(key=lambda r: (int(r["frame"]), int(r["sub"])))
    out = {}
    for b in BONES:
        brows = [r for r in rows if r["bone"] == b]
        if not brows:
            continue
        t = np.array([int(r["frame"]) * 2 + int(r["sub"]) for r in brows])
        entry = {"t": t}
        for col in COLS:
            keys = ["x_%s_mm" % col, "y_%s_mm" % col, "z_%s_mm" % col]
            if keys[0] not in brows[0]:
                continue  # older CSVs (or the selftest fixture) may omit a column
            entry[col] = np.array([[float(r[k]) for k in keys] for r in brows])
        out[b] = entry
    if not out:
        raise SystemExit("hinge_trajplot: %s named none of %s" % (csv_path, BONES))
    return out


def range3d_mm(xyz):
    """max pairwise distance between any two samples -- the honest 'range of
    motion' number (peak-to-peak per axis can understate a diagonal swing)."""
    if len(xyz) < 2:
        return 0.0
    d = xyz[:, None, :] - xyz[None, :, :]
    return float(np.sqrt((d ** 2).sum(axis=2)).max())


def correlation(a, b):
    """Mean per-AXIS Pearson r between two hinges' motion, the 'do they move
    SEPARATELY' number -- 1.0 is perfectly locked together. Per-axis (not
    flattened-xyz) deliberately: an axis one hinge never exercises is a
    constant column, and flattening it in with a moving axis lets that
    shared constant dominate the correlation and read as coupling that is
    not really there (caught by this tool's own selftest -- two signals 90
    degrees out of phase on the same single axis read as r=0.999 before
    this fix, because the two other, both-zero axes swamped it)."""
    rs = []
    for axi in range(3):
        ca, cb = a[:, axi], b[:, axi]
        if ca.std() < 1e-6 or cb.std() < 1e-6:
            continue  # a constant axis has no correlation to report
        rs.append(np.corrcoef(ca, cb)[0, 1])
    if not rs:
        return float("nan")
    return float(np.mean(rs))


def emit(data, out_png, title=""):
    rows = len(BONES)
    fig, axes = plt.subplots(rows, 3, figsize=(16, 2.1 * rows), sharex=True)
    if rows == 1:
        axes = axes.reshape(1, 3)
    print("hinge_trajplot: OWN range of motion (isolated per-joint, mm)")
    for i, b in enumerate(BONES):
        if b not in data:
            continue
        d = data[b]
        t = d["t"]
        for col_i, col in enumerate(COLS):
            if col not in d:
                continue
            arr = d[col]
            ax = axes[i, col_i]
            for axi, (nm, c) in enumerate([("x", "tab:red"), ("y", "tab:green"), ("z", "tab:blue")]):
                ax.plot(t, arr[:, axi], lw=0.9, color=c, label=nm)
            p2p = arr.max(axis=0) - arr.min(axis=0)
            r3 = range3d_mm(arr)
            ax.set_ylabel(b, fontsize=8)
            ax.set_title(
                "p2p x/y/z %.0f/%.0f/%.0f mm, 3D range %.0f mm"
                % (p2p[0], p2p[1], p2p[2], r3),
                fontsize=7, loc="right")
            if i == 0 and col_i == 0:
                ax.legend(fontsize=6, loc="upper left")
        if "own" in d:
            p2p = d["own"].max(axis=0) - d["own"].min(axis=0)
            r3 = range3d_mm(d["own"])
            print("  %-10s x %6.0f  y %6.0f  z %6.0f  mm   3D range %6.0f mm"
                  % (b, p2p[0], p2p[1], p2p[2], r3))
    for c in range(3):
        axes[-1, c].set_xlabel("presentation sample (frame*2+sub)")
    axes[0, 0].set_title("OWN (isolated joint rotation)", fontsize=9)
    axes[0, 1].set_title("LOCAL (root-local, cumulative)", fontsize=9)
    axes[0, 2].set_title("WORLD (raw posed)", fontsize=9)
    fig.suptitle(title or "antenna hinge trajectories")
    fig.tight_layout()
    fig.savefig(out_png, dpi=100)
    plt.close(fig)
    # pairwise correlation of OWN motion -- "do they move separately?"
    print("hinge_trajplot: pairwise OWN-motion correlation (1.0 = locked together)")
    names = [b for b in BONES if b in data and "own" in data[b]]
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            r = correlation(data[a]["own"], data[b]["own"])
            print("  %-10s vs %-10s  r = %.3f" % (a, b, r))
    print("hinge_trajplot: wrote %s" % out_png)


def selftest():
    """Can-fail proof: a FLAT synthetic hinge must read ~0 mm range, an
    INDEPENDENT pair of sinusoids (90 deg out of phase) must read a low
    correlation, and a LOCKED pair (identical signal) must read ~1.0."""
    import tempfile
    import os

    ok = True
    n = 60
    frames = np.arange(n)
    rows = []
    # flat: never moves
    for f in frames:
        rows.append((f, 0, "junctionF", 0, 2000, 0, 0, 700, 0, 0, 700, 0))
    # independent (a, 90 deg phase): a real, separate range of motion
    for f in frames:
        a = 40 * np.sin(2 * np.pi * f / n)
        rows.append((f, 0, "neck", 0, 2000 + a, 0, 0, 1000 + a, 0, 0, 1000 + a, 0))
    # locked to "neck" (same signal, different bone name): should look glued
    for f in frames:
        a = 40 * np.sin(2 * np.pi * f / n)
        rows.append((f, 0, "hingeA", 0, 2000 + a, 0, 0, 1000 + a, 0, 0, 1000 + a, 0))
    # a third, phase-shifted 90 deg from "neck": low correlation with it
    for f in frames:
        b = 40 * np.sin(2 * np.pi * f / n + np.pi / 2)
        rows.append((f, 0, "hingeB", 0, 2000 + b, 0, 0, 1000 + b, 0, 0, 1000 + b, 0))
    with tempfile.TemporaryDirectory() as td:
        csv_path = os.path.join(td, "synthetic.csv")
        with open(csv_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["frame", "sub", "bone", "x_world_mm", "y_world_mm", "z_world_mm",
                        "x_local_mm", "y_local_mm", "z_local_mm",
                        "x_own_mm", "y_own_mm", "z_own_mm"])
            w.writerows(rows)
        data = load(csv_path)
        flat_r3 = range3d_mm(data["junctionF"]["own"])
        moving_r3 = range3d_mm(data["neck"]["own"])
        r_locked = correlation(data["neck"]["own"], data["hingeA"]["own"])
        r_indep = correlation(data["neck"]["own"], data["hingeB"]["own"])
        print("selftest: flat hinge 3D range %.2f mm (want ~0)" % flat_r3)
        print("selftest: moving hinge 3D range %.2f mm (want ~80)" % moving_r3)
        print("selftest: locked-pair correlation %.3f (want ~1.0)" % r_locked)
        print("selftest: independent-pair (90deg) correlation %.3f (want ~0.0)" % r_indep)
        if flat_r3 > 1.0:
            print("SELFTEST FAILURE: a flat signal was not read as flat")
            ok = False
        if not (60 <= moving_r3 <= 100):
            print("SELFTEST FAILURE: a moving signal was not read as moving")
            ok = False
        if r_locked < 0.95:
            print("SELFTEST FAILURE: an identical pair did not read as locked")
            ok = False
        if abs(r_indep) > 0.3:
            print("SELFTEST FAILURE: a 90-degree-phase pair did not read as independent")
            ok = False
    print("SELFTEST:", "PASS (the instrument can fail)" if ok else "BROKEN")
    return ok


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "selftest":
        sys.exit(0 if selftest() else 1)
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = load(sys.argv[1])
    emit(data, sys.argv[2], title=sys.argv[1])
