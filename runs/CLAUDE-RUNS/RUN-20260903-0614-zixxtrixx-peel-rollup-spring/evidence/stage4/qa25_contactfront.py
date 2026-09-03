#!/usr/bin/env python3
"""THE CONTACT-FRONT TRACKER -- the peel pass's central diagnostic.

Per frame of a fixed-camera spring-side render: segment the creature (color
mask, from the committed qa2 instruments), find the terrain's top edge per
column, and mark the columns where the creature's bottom edge sits within
BAND px of the terrain. The HEAD-MOST (right-most) such column is the
contact front. Direction 25's acceptance 1-2 in pixels: the front recedes
monotonically from the front of the grounded stretch to the tail tip and
never re-advances, and the final ground contact is a single small patch.

Usage: qa25_contactfront.py <render-dir> <out.csv> <out.png> [last_frame]
"""
import sys, os
import numpy as np
from scipy import ndimage
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BAND = 4  # px above the terrain edge that counts as touching

def load(p, i):
    r = np.fromfile(os.path.join(p, "%04d.rgb" % i), dtype=np.uint8)
    w, h = r[:8].view(np.uint32)
    return r[8:].reshape(int(h), int(w), 3)

def creature_mask(f):
    f = f.astype(np.int16)
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
    s = ndimage.sum(m, lab, range(1, n + 1))
    return lab == int(np.argmax(s)) + 1

def terrain_top(f):
    # terrain = the dark brown ground; its top edge per column. Use the sky/
    # ground luminance boundary on a creature-free frame region: compute on
    # the whole frame but only outside the creature mask.
    fi = f.astype(np.int16)
    ground = (fi[:, :, 0] < 190) & (fi[:, :, 1] < 150) & (fi[:, :, 2] < 130)
    h, w = ground.shape
    top = np.full(w, h - 1, dtype=int)
    for x in range(w):
        col = np.nonzero(ground[:, x])[0]
        if len(col):
            top[x] = col[0]
    return top

def main():
    d, out_csv, out_png = sys.argv[1], sys.argv[2], sys.argv[3]
    last = int(sys.argv[4]) if len(sys.argv) > 4 else 120
    top = terrain_top(load(d, 0))  # camera + terrain are FIXED
    rows = []
    for i in range(last + 1):
        f = load(d, i)
        m = creature_mask(f)
        h, w = m.shape
        touch = []
        for x in range(w):
            col = np.nonzero(m[:, x])[0]
            if not len(col):
                continue
            bottom = col[-1]
            if bottom >= top[x] - BAND:
                touch.append(x)
        if touch:
            rows.append((i, min(touch), max(touch), len(touch)))
        else:
            rows.append((i, -1, -1, 0))
    with open(out_csv, "w") as fh:
        fh.write("frame,contact_x_min,contact_front_x,contact_cols\n")
        for r in rows:
            fh.write("%d,%d,%d,%d\n" % r)
    fr = [r[0] for r in rows]
    front = [r[2] for r in rows]
    left = [r[1] for r in rows]
    fig, ax = plt.subplots(figsize=(12, 5))
    ax.plot(fr, front, "-", color="tab:red", label="contact FRONT (head-most touching column)")
    ax.plot(fr, left, "-", color="tab:blue", alpha=0.6, label="contact rear edge")
    ax.fill_between(fr, left, front, color="tab:orange", alpha=0.25, label="contact patch")
    ax.set_xlabel("frame"); ax.set_ylabel("screen x (px)")
    ax.set_title("THE PEEL in pixels: the contact patch recedes to the tail tip")
    ax.legend(); ax.grid(alpha=0.3)
    fig.tight_layout(); fig.savefig(out_png, dpi=110)
    # the verdict numbers
    ground = [r for r in rows if r[3] > 0]
    fronts = [r[2] for r in ground]
    worst_readvance = max((fronts[i+1] - fronts[i]) for i in range(len(fronts)-1))
    print("frames with contact:", len(ground), "of", len(rows))
    print("contact front: first", fronts[0], "px -> last", fronts[-1], "px")
    print("worst single-frame re-advance:", worst_readvance, "px")
    print("final patch width:", ground[-1][3], "columns")
    print("wrote", out_csv, "and", out_png)

if __name__ == "__main__":
    main()
