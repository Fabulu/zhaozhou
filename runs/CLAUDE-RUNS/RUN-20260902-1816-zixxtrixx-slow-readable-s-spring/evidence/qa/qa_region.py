#!/usr/bin/env python3
"""qa_region.py -- QA's independent fixed-camera region instrument.

COMPARISON SIDE ONLY (art law). Rebuilt from the method recorded in
evidence/review/reviewer-measurements.txt so the reviewer's own numbers can be
reproduced against a new build, and committed so they stay reproducible.

Segmentation: colour classifier (ink lum<130 | G>R+18 | B>R+18 | lum>470 |
yellow eye), 3x3 closing x2, fill holes, largest component.  NO median plate.

Usage: python qa_region.py <render-dir> --out DIR --label NAME
                           [--ground A B] [--beats a b c d e]
"""
import argparse, os, sys
import numpy as np
from scipy import ndimage

W, H = 384, 240

def load(path):
    names = sorted(n for n in os.listdir(path) if n.endswith(".rgb"))
    out = []
    for n in names:
        raw = np.fromfile(os.path.join(path, n), dtype=np.uint8)
        w, h = raw[:8].view(np.uint32)
        out.append(raw[8:].reshape(int(h), int(w), 3).astype(np.int16))
    return out

def mask_of(f):
    R, G, B = f[:, :, 0], f[:, :, 1], f[:, :, 2]
    lum = R + G + B
    m = (lum < 130) | (G > R + 18) | (B > R + 18) | (lum > 470) | \
        ((R > 150) & (G > 130) & (B < 110))
    st = np.ones((3, 3), bool)
    m = ndimage.binary_closing(m, st)
    m = ndimage.binary_closing(m, st)
    m = ndimage.binary_fill_holes(m)
    lab, n = ndimage.label(m)
    if n == 0:
        return m
    sizes = ndimage.sum(m, lab, range(1, n + 1))
    return lab == (int(np.argmax(sizes)) + 1)

def centroid(m):
    ys, xs = np.nonzero(m)
    if len(xs) == 0:
        return (np.nan, np.nan)
    return (xs.mean(), ys.mean())

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("--out", required=True)
    ap.add_argument("--label", required=True)
    ap.add_argument("--ground", nargs=2, type=int, default=None)
    ap.add_argument("--split", nargs=2, type=int, default=[170, 210],
                    help="screen-x region boundaries tail|mid|head")
    ap.add_argument("--beats", nargs="*", type=int, default=None,
                    help="frame boundaries: settle_end b1_end dwell_end b2_end hold_end")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    frames = load(a.render)
    N = len(frames)
    g0, g1 = a.ground if a.ground else (0, N - 1)
    masks = [mask_of(f) for f in frames]

    # raw changed pixels (no segmentation at all)
    raw = [0]
    for i in range(1, N):
        raw.append(int(np.any(frames[i] != frames[i - 1], axis=2).sum()))

    L = []
    P = L.append
    P(f"QA region instrument -- {a.label}  ({N} frames, ground {g0}-{g1})")
    P("")
    P("RAW CHANGED PIXELS PER FRAME (no segmentation):")
    med = float(np.median(raw[1:]))
    P(f"  clip median {med:.0f}/f   p90 {np.percentile(raw[1:],90):.0f}/f   max {max(raw[1:])}")
    ident = [i for i in range(1, N) if raw[i] == 0]
    P(f"  byte-identical frames (vs previous): {len(ident)} -> {ident[:20]}")
    P("")
    if a.beats:
        b = [0] + list(a.beats)
        names = ["settle", "beat1", "dwell", "beat2", "hold"]
        for i, nm in enumerate(names):
            if i + 1 < len(b):
                seg = raw[b[i] + 1:b[i + 1] + 1]
                if seg:
                    P(f"  {nm:7s} f{b[i]:3d}-{b[i+1]:3d}  {np.mean(seg):7.1f}/f")
        if b[-1] + 4 < N:
            P(f"  launch  f{b[-1]+1:3d}-{b[-1]+4:3d}  " +
              " ".join(str(raw[j]) for j in range(b[-1] + 1, min(b[-1] + 5, N))))
        P("")

    # smoothness on the segmented body
    cx = np.array([centroid(m)[0] for m in masks])
    cy = np.array([centroid(m)[1] for m in masks])
    area = np.array([m.sum() for m in masks], float)
    def stats(v, lo, hi):
        d = np.diff(v[lo:hi + 1]); j = np.diff(d)
        return (np.median(np.abs(d)), np.max(np.abs(d)), np.max(np.abs(j)))
    for nm, v in (("centroid x", cx), ("centroid y", cy), ("area", area)):
        s = stats(v, g0, g1)
        P(f"  {nm:10s} |v| med {s[0]:7.3f}  |v| max {s[1]:7.3f}  jerk max {s[2]:7.3f}")
    # silhouette XOR
    xor = [np.nan]
    for i in range(1, N):
        u = (masks[i] | masks[i - 1]).sum()
        xor.append(100.0 * (masks[i] ^ masks[i - 1]).sum() / max(u, 1))
    P(f"  silhouette XOR med {np.nanmedian(xor[g0+1:g1+1]):.2f} %/f  max {np.nanmax(xor[g0+1:g1+1]):.2f} %/f")
    # odd/even staircase parity
    dy = np.abs(np.diff(cy[g0:g1 + 1]))
    P(f"  30Hz staircase odd/even |step| ratio (cy): {np.mean(dy[0::2])/max(np.mean(dy[1::2]),1e-9):.2f}")
    P("")

    # head / tail extremes
    P("HEAD vs TAIL (rightmost / leftmost body pixel), ground phase:")
    nose_x, nose_y, rear_x = [], [], []
    for i in range(g0, g1 + 1):
        ys, xs = np.nonzero(masks[i])
        nx = xs.max(); rear_x.append(xs.min())
        nose_x.append(nx); nose_y.append(ys[xs == nx].mean())
    P(f"  nose tip x {min(nose_x)}..{max(nose_x)}   rearmost x {min(rear_x)}..{max(rear_x)}")
    P(f"  min gap nose-rear: {min(n - r for n, r in zip(nose_x, rear_x))} px")
    P(f"  nose tip travel f{g0}->f{g1}: {nose_x[-1]-nose_x[0]:+d} px x, {nose_y[-1]-nose_y[0]:+.1f} px y")
    fwd = max(np.array(nose_x) - nose_x[0])
    P(f"  nose max FORWARD excursion en route: {fwd - nose_x[0]:+.0f} px")
    P("")

    # region shares of silhouette change + region centroid descent
    x0, x1 = a.split
    cols = np.arange(W)
    regions = {"tail<%d" % x0: cols < x0,
               "mid%d-%d" % (x0, x1): (cols >= x0) & (cols < x1),
               "head>=%d" % x1: cols >= x1}
    if a.beats:
        b = [0] + list(a.beats)
        windows = [("beat1", b[1], b[2]), ("beat2", b[3], b[4])]
    else:
        windows = [("ground", g0, g1)]
    P("WHERE THE MOTION LIVES (cumulative silhouette XOR share by screen region):")
    for nm, lo, hi in windows:
        tot = {}
        for rn, sel in regions.items():
            s = 0
            for i in range(lo + 1, hi + 1):
                s += int((masks[i] ^ masks[i - 1])[:, sel].sum())
            tot[rn] = s
        T = max(sum(tot.values()), 1)
        P(f"  {nm} f{lo}-{hi}: " + "  ".join(f"{rn} {100.0*v/T:5.1f}%" for rn, v in tot.items()))
    P("")
    P("REGION CENTROID Y (down is +) across the compression:")
    if a.beats:
        b = [0] + list(a.beats)
        lo, hi = b[2], b[5] if len(b) > 5 else g1
    else:
        lo, hi = g0, g1
    for rn, sel in regions.items():
        def rc(i):
            m = masks[i].copy(); m[:, ~sel] = False
            ys, xs = np.nonzero(m)
            return ys.mean() if len(ys) else np.nan
        P(f"  {rn:12s} f{lo}->f{hi}: {rc(hi)-rc(lo):+6.2f} px")
    P("")
    txt = "\n".join(L)
    open(os.path.join(a.out, a.label + "-region.txt"), "w").write(txt + "\n")
    np.savetxt(os.path.join(a.out, a.label + "-raw.csv"),
               np.array(raw), fmt="%d", header="raw_changed_px")
    print(txt)

main()
