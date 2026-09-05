"""Trajectory plots + contact sheets for creature clips (the committed
"seeing the work properly" instruments -- CLAUDE.md: a flat line IS "it
never bobs", and uniform sampling misses the broken frame, so look at ALL
of them).

For each subject directory of .rgb frames this emits:
  <out>/<subject>-traj.png     ink-silhouette centroid x/y, ink-pixel count
                               and bounding box height per frame, plotted
                               over time, with the per-channel peak-to-peak
                               printed (the flat-line detector)
  <out>/<subject>-sheet.png    a contact sheet of EVERY frame (downscaled)

The ink mask is the exact cel ink colour -- the same law inkmask.py uses.
Import rgbframe; never write another frame reader.

Usage: python trajplot.py <out_dir> <subject_dir> [subject_dir...]
"""
import glob
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from rgbframe import load

# The creature mask is ROW-MEDIAN BACKGROUND SUBTRACTION, not the exact
# cel-ink colour: on the full-colour RGB565 path the ink does not survive
# quantisation as one exact colour (measured: zero exact-ink pixels on a
# u02 clip frame), and this creature is pink under a pink sky, so colour
# rules alone also fail. The sky and terrain vary smoothly per row; the
# creature is the compact thing that deviates from its row's median.
DEV_L1 = 90


def creature_mask(im):
    med = np.median(im.reshape(im.shape[0], -1, 3), axis=1).astype(int)
    dev = np.abs(im.astype(int) - med[:, None, :]).sum(axis=2)
    return dev > DEV_L1


def series(frame_dir):
    files = sorted(glob.glob(os.path.join(frame_dir, "*.rgb")))
    cx, cy, n, bh, imgs = [], [], [], [], []
    for f in files:
        im = load(f)
        m = creature_mask(im)
        ys, xs = np.nonzero(m)
        if len(xs) == 0:
            cx.append(np.nan); cy.append(np.nan); n.append(0); bh.append(0)
        else:
            cx.append(xs.mean()); cy.append(ys.mean()); n.append(len(xs))
            bh.append(ys.max() - ys.min())
        imgs.append(im[::4, ::4])
    return np.array(cx), np.array(cy), np.array(n), np.array(bh), imgs


def emit(out_dir, frame_dir):
    name = os.path.basename(os.path.normpath(frame_dir))
    cx, cy, n, bh, imgs = series(frame_dir)
    fig, axes = plt.subplots(4, 1, figsize=(10, 8), sharex=True)
    for ax, (label, v) in zip(
        axes,
        [("centroid x (px)", cx), ("centroid y (px)", cy),
         ("ink px count", n), ("bbox height (px)", bh)],
    ):
        ax.plot(v, lw=0.8)
        p2p = np.nanmax(v) - np.nanmin(v) if len(v) else 0
        ax.set_ylabel(label, fontsize=8)
        ax.set_title("p2p %.1f" % p2p, fontsize=8, loc="right")
    axes[-1].set_xlabel("frame")
    fig.suptitle(name)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, name + "-traj.png"), dpi=90)
    plt.close(fig)
    # the contact sheet: every frame, quarter scale
    if imgs:
        h, w, _ = imgs[0].shape
        cols = max(1, int(np.ceil(np.sqrt(len(imgs) * h / float(w)))))
        rows = int(np.ceil(len(imgs) / float(cols)))
        sheet = np.zeros((rows * h, cols * w, 3), np.uint8)
        for i, im in enumerate(imgs):
            r, c = divmod(i, cols)
            sheet[r * h:(r + 1) * h, c * w:(c + 1) * w] = im
        from rgbframe import save_png
        save_png(sheet, os.path.join(out_dir, name + "-sheet.png"), scale=1)
    print("%s: %d frames  cx p2p %.1f  cy p2p %.1f  area p2p %d  bh p2p %d"
          % (name, len(imgs), np.nanmax(cx) - np.nanmin(cx),
             np.nanmax(cy) - np.nanmin(cy), int(n.max() - n.min()),
             int(bh.max() - bh.min())))


if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for d in sys.argv[2:]:
        emit(out, d)
