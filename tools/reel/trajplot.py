"""Trajectory plots + contact sheets for creature clips (the committed
"seeing the work properly" instruments -- CLAUDE.md: a flat line IS "it
never bobs", and uniform sampling misses the broken frame, so look at ALL
of them).

For each subject directory of .rgb frames this emits:
  <out>/<subject>-traj.png     creature-mask centroid x/y, mask-pixel count
                               and bounding box height per frame, plotted
                               over time, with the per-channel peak-to-peak
                               printed (the flat-line detector)
  <out>/<subject>-sheet.png    a contact sheet of EVERY frame (downscaled)

THE MASK, REBUILT FOR PASS 4 (gate checklist item 6). The old mask was
row-median background subtraction alone, and the pass-3 review proved it
returns ~2000 px of TERRAIN HORIZON on a frame with no creature in it at
all (unnamed02-fall frame 0000) -- every published series carried that
contamination. The exact fix is a CREATURE-FREE BACKGROUND RENDER of the
same subject: the reel's ZIXX_HIDE_CREATURE=1 gate renders the identical
stage, camera and sky with the creature hook skipped, and the mask then
requires a pixel to deviate from that plate (as well as from its row
median). A temporal-median plate was tried and rejected by its own
selftest: a slow-moving or hovering creature votes itself into the median
and erases itself -- the measurement lying in a new way.

    ZIXX_HIDE_CREATURE=1 zhao-reel-cel.exe <bgdir> <subject> ...
    python trajplot.py --bg <bgdir>/<subject> <out_dir> <subject_dir> ...

Without --bg the tool falls back to the legacy row-median mask and SAYS SO
LOUDLY on every subject -- the numbers may carry static-stage
contamination and are not QA evidence.

`python trajplot.py selftest` proves the mask can fail: a creature-free
horizon frame must produce (near) zero mask pixels against its plate, a
blob (moving or hovering) must be caught, and the legacy fallback must
exhibit the horizon fault on the synthetic stage.

Import rgbframe; never write another frame reader.

Usage: python trajplot.py [--bg <bg_dir>] <out_dir> <subject_dir> [subject_dir...]
       python trajplot.py selftest
"""
import glob
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from rgbframe import load

DEV_L1 = 90   # deviation from the row median (compactness)
BG_L1 = 45    # deviation from the creature-free background plate


def load_bg(bg_dir):
    """Map basename -> loaded background frame for a creature-free render."""
    if bg_dir is None:
        return None
    files = sorted(glob.glob(os.path.join(bg_dir, "[0-9]" * 4 + ".rgb")))
    if not files:
        raise SystemExit("trajplot: --bg %s holds no .rgb frames" % bg_dir)
    return {os.path.basename(f): load(f).astype(np.int32) for f in files}


def creature_mask(im, bg=None):
    """bg: the matching creature-free frame (int32), or None for legacy."""
    med = np.median(im.reshape(im.shape[0], -1, 3), axis=1).astype(int)
    dev = np.abs(im.astype(int) - med[:, None, :]).sum(axis=2)
    m = dev > DEV_L1
    if bg is not None:
        m &= np.abs(im.astype(int) - bg).sum(axis=2) > BG_L1
    return m


def series(frame_dir, bgs=None):
    files = sorted(glob.glob(os.path.join(frame_dir, "[0-9]" * 4 + ".rgb")))
    cx, cy, n, bh, imgs = [], [], [], [], []
    for f in files:
        im = load(f)
        bg = None
        if bgs is not None:
            bg = bgs.get(os.path.basename(f))
            if bg is None and len(bgs) == 1:
                bg = next(iter(bgs.values()))  # a single still plate
        m = creature_mask(im, bg)
        ys, xs = np.nonzero(m)
        if len(xs) == 0:
            cx.append(np.nan); cy.append(np.nan); n.append(0); bh.append(0)
        else:
            cx.append(xs.mean()); cy.append(ys.mean()); n.append(len(xs))
            bh.append(ys.max() - ys.min())
        imgs.append(im[::4, ::4])
    return np.array(cx), np.array(cy), np.array(n), np.array(bh), imgs


def emit(out_dir, frame_dir, bgs=None):
    name = os.path.basename(os.path.normpath(frame_dir))
    if bgs is None:
        print("%s: WARNING legacy row-median mask (no --bg plate): numbers "
              "may include static-stage contamination; not QA evidence" % name)
    cx, cy, n, bh, imgs = series(frame_dir, bgs)
    fig, axes = plt.subplots(4, 1, figsize=(10, 8), sharex=True)
    for ax, (label, v) in zip(
        axes,
        [("centroid x (px)", cx), ("centroid y (px)", cy),
         ("mask px count", n), ("bbox height (px)", bh)],
    ):
        ax.plot(v, lw=0.8)
        p2p = np.nanmax(v) - np.nanmin(v) if len(v) else 0
        ax.set_ylabel(label, fontsize=8)
        ax.set_title("p2p %.1f" % p2p, fontsize=8, loc="right")
    axes[-1].set_xlabel("frame")
    fig.suptitle(name + ("" if bgs is not None else "  [LEGACY MASK]"))
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


def selftest():
    """The can-fail proof (gate checklist item 6): with a plate, a
    creature-free horizon frame masks to (near) zero and both a moving and
    a HOVERING blob are caught; the legacy mask must exhibit the horizon
    fault on the same stage (proving this selftest can detect it)."""
    import struct
    import tempfile

    def write_rgb(path, img):
        h, w, _ = img.shape
        with open(path, "wb") as f:
            f.write(struct.pack("<II", w, h))
            f.write(img.tobytes())

    # a static stage with a ragged sky/terrain horizon (the fault class:
    # boundary rows deviate from their row median)
    stage = np.zeros((240, 384, 3), np.uint8)
    stage[:180] = (231, 183, 150)                      # peach sky
    stage[180:] = (96, 72, 48)                         # terrain
    for x in range(384):
        stage[178 + (x * 7 % 13) % 4:182, x] = (140, 110, 80)
    ok = True
    with tempfile.TemporaryDirectory() as td:
        clip, bgd = os.path.join(td, "clip"), os.path.join(td, "bg")
        os.makedirs(clip), os.makedirs(bgd)
        for i in range(12):
            im = stage.copy()
            if 1 <= i <= 6:              # a moving blob
                x0 = 100 + i * 12
                im[100:140, x0:x0 + 60] = (240, 60, 140)
            elif i >= 7:                 # a HOVERING blob (the median-killer)
                im[100:140, 200:260] = (240, 60, 140)
            write_rgb(os.path.join(clip, "%04d.rgb" % i), im)
            write_rgb(os.path.join(bgd, "%04d.rgb" % i), stage)
        bgs = load_bg(bgd)
        files = sorted(glob.glob(os.path.join(clip, "*.rgb")))
        get = lambda i, bg: int(creature_mask(load(files[i]), bg).sum())
        plate = next(iter(bgs.values()))
        n_empty = get(0, plate)
        n_move = get(4, plate)
        n_hover = get(9, plate)
        legacy_empty = get(0, None)
        print("selftest: creature-free frame, plate mask: %d px "
              "(legacy row-median on the same frame: %d px)" % (n_empty, legacy_empty))
        print("selftest: moving blob %d px, hovering blob %d px (truth 2400)"
              % (n_move, n_hover))
        if n_empty > 50:
            print("SELFTEST FAILURE: horizon leaked through the plate mask")
            ok = False
        for nm, v in (("moving", n_move), ("hovering", n_hover)):
            if not 1800 <= v <= 3200:
                print("SELFTEST FAILURE: the %s blob was not caught" % nm)
                ok = False
        if legacy_empty < 100:
            print("SELFTEST FAILURE: the legacy mask did not exhibit the "
                  "horizon fault -- this selftest can no longer prove the fix")
            ok = False
    print("SELFTEST:", "PASS (the mask can fail)" if ok else "BROKEN")
    return ok


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "selftest":
        sys.exit(0 if selftest() else 1)
    args = sys.argv[1:]
    bgs = None
    if args and args[0] == "--bg":
        bgs = load_bg(args[1])
        args = args[2:]
    out = args[0]
    os.makedirs(out, exist_ok=True)
    for d in args[1:]:
        emit(out, d, bgs)
