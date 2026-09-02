#!/usr/bin/env python3
"""zixx_legibility.py -- per-frame legibility/smoothness probe for rendered clips.

COMPARISON SIDE ONLY (art law): this tool asserts nothing about what to author.
It measures rendered frames so the eye's judgement can be checked, and so a
regression is visible. Introduced by RUN-20260902-1816 (Direction 23), from
Recon 5's instrument definitions.

Input: a render directory of raw NNNN.rgb frames (384x240 RGB24, the reel's
output) or a .webm/.mp4 (decoded via ffmpeg). Segmentation is COLOUR-based,
never a median background plate: most clips have a moving camera, and a plate
silently masks in terrain (Recon 5 section 0, a declared measurement trap).

Outputs, in --out:
  <label>.csv            per frame: area, solidity, hole%, closure, spine px,
                         shape rate %/f (translation-compensated), half-life,
                         head found/x/y, weber contrast, centroid x/y
  <label>-panels.png     four-panel time series (shape rate, half-life,
                         solidity+hole, closure)      [--plots]
  <label>-jerk.txt       max |delta-of-delta| for centroid/head/area over the
                         window (Recon 2 section 3b's instrument)
  <label>-beats.txt      beat segmentation printout (activity runs and gaps)
  <label>-overlay.png    frame-beside-mask spot-check sheet   [--sheets]
  <label>-contact.png    every-frame contact sheet            [--sheets]
  <label>-zoom.png       centroid-locked 2x zoom sheet        [--sheets]

Usage:
  python zixx_legibility.py <render-dir-or-video> --out DIR [--label NAME]
      [--window A B] [--fps 60] [--plots] [--sheets] [--scale 0.5]
"""
import argparse
import io
import os
import subprocess
import sys
from collections import deque

import numpy as np
from PIL import Image

try:
    from scipy import ndimage
except ImportError:
    sys.exit("scipy is required (pip install scipy)")

W, H = 384, 240  # reel output; verified against frame byte size for .rgb input


# ---------------------------------------------------------------- loading

def load_frames(path):
    if os.path.isdir(path):
        names = sorted(n for n in os.listdir(path) if n.endswith(".rgb"))
        if not names:
            sys.exit(f"no .rgb frames in {path}")
        frames = []
        for n in names:
            raw = np.fromfile(os.path.join(path, n), dtype=np.uint8)
            # reel frames carry an 8-byte header: uint32 LE width, height
            w, h = raw[:8].view(np.uint32)
            if raw.size != 8 + w * h * 3:
                sys.exit(f"{n}: {raw.size} bytes, header says {w}x{h}")
            if (w, h) != (W, H):
                sys.exit(f"{n}: {w}x{h}, expected {W}x{H}")
            frames.append(raw[8:].reshape(h, w, 3))
        return frames
    # video: decode via ffmpeg to raw RGB24
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
         "stream=width,height", "-of", "csv=p=0", path],
        capture_output=True, text=True, check=True)
    w, h = (int(x) for x in probe.stdout.strip().split(","))
    dec = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo",
         "-pix_fmt", "rgb24", "-"],
        capture_output=True, check=True)
    raw = np.frombuffer(dec.stdout, dtype=np.uint8)
    n = raw.size // (w * h * 3)
    return list(raw[: n * w * h * 3].reshape(n, h, w, 3))


# ---------------------------------------------------------------- segmentation

def segment(img):
    """Colour segmentation per Recon 5: dark outline, green/cyan body,
    magenta/pink highlight. Largest connected component."""
    r = img[:, :, 0].astype(np.int32)
    g = img[:, :, 1].astype(np.int32)
    b = img[:, :, 2].astype(np.int32)
    lum = (r * 299 + g * 587 + b * 114) // 1000
    mask = (lum < 52) | (g > r + 10) | (b > r + 10) | ((r > g + 30) & (b > g + 30))
    lab, n = ndimage.label(mask)
    if n == 0:
        return np.zeros_like(mask)
    sizes = ndimage.sum(mask, lab, range(1, n + 1))
    keep = int(np.argmax(sizes)) + 1
    out = lab == keep
    # pull in any other sizeable component within a small gap (fins can detach
    # by a pixel of antialiasing); threshold: >=1% of main blob, within 12 px
    main_dil = ndimage.binary_dilation(out, iterations=12)
    for i in range(1, n + 1):
        if i != keep and sizes[i - 1] >= 0.01 * sizes[keep - 1]:
            if np.any((lab == i) & main_dil):
                out |= lab == i
    return out


def head_blob(img, mask):
    """The gold eye: bright warm yellow inside the silhouette."""
    r = img[:, :, 0].astype(np.int32)
    g = img[:, :, 1].astype(np.int32)
    b = img[:, :, 2].astype(np.int32)
    eye = (r > 140) & (g > 90) & (b < 100) & (r > b + 60) & (g > b + 20)
    eye &= ndimage.binary_dilation(mask, iterations=2)
    if not np.any(eye):
        return None
    ys, xs = np.nonzero(eye)
    return float(xs.mean()), float(ys.mean())


# ---------------------------------------------------------------- geometry

def centroid(mask):
    ys, xs = np.nonzero(mask)
    return float(xs.mean()), float(ys.mean())


def shift_mask(mask, dx, dy):
    out = np.zeros_like(mask)
    h, w = mask.shape
    dx, dy = int(round(dx)), int(round(dy))
    xs0, xs1 = max(0, dx), min(w, w + dx)
    ys0, ys1 = max(0, dy), min(h, h + dy)
    out[ys0:ys1, xs0:xs1] = mask[ys0 - dy:ys1 - dy, xs0 - dx:xs1 - dx]
    return out


def aligned_iou_xor(a, b):
    """Translation-compensated IoU and XOR/union between two masks."""
    if not a.any() or not b.any():
        return 0.0, 1.0
    ax, ay = centroid(a)
    bx, by = centroid(b)
    b2 = shift_mask(b, ax - bx, ay - by)
    inter = np.logical_and(a, b2).sum()
    union = np.logical_or(a, b2).sum()
    if union == 0:
        return 0.0, 1.0
    return inter / union, (union - inter) / union


def convex_solidity(mask):
    ys, xs = np.nonzero(mask)
    if len(xs) < 3:
        return 1.0
    # hull candidates: per-row min/max x only (<= 2*H points)
    pts = []
    for y in np.unique(ys):
        row = xs[ys == y]
        pts.append((float(row.min()), float(y)))
        pts.append((float(row.max()), float(y)))
    pts = sorted(set(pts))
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    def half(points):
        st = []
        for p in points:
            while len(st) >= 2 and cross(st[-2], st[-1], p) <= 0:
                st.pop()
            st.append(p)
        return st
    hull = half(pts)[:-1] + half(pts[::-1])[:-1]
    if len(hull) < 3:
        return 1.0
    area_h = 0.0
    for i in range(len(hull)):
        x0, y0 = hull[i]
        x1, y1 = hull[(i + 1) % len(hull)]
        area_h += x0 * y1 - x1 * y0
    area_h = abs(area_h) / 2
    if area_h <= 0:
        return 1.0
    return float(mask.sum()) / area_h


def hole_fraction(mask):
    filled = ndimage.binary_fill_holes(mask)
    holes = filled.sum() - mask.sum()
    a = mask.sum()
    return holes / a if a else 0.0


def skeletonize(mask):
    """Zhang-Suen thinning, vectorised."""
    img = mask.astype(np.uint8).copy()
    def neighbours(p):
        return [np.roll(np.roll(p, dy, 0), dx, 1) for dy, dx in
                [(-1, 0), (-1, 1), (0, 1), (1, 1), (1, 0), (1, -1), (0, -1), (-1, -1)]]
    changed = True
    while changed:
        changed = False
        for step in (0, 1):
            p2, p3, p4, p5, p6, p7, p8, p9 = neighbours(img)
            bn = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9
            seq = [p2, p3, p4, p5, p6, p7, p8, p9, p2]
            an = np.zeros_like(img)
            for i in range(8):
                an += ((seq[i] == 0) & (seq[i + 1] == 1)).astype(np.uint8)
            if step == 0:
                cond = (p2 * p4 * p6 == 0) & (p4 * p6 * p8 == 0)
            else:
                cond = (p2 * p4 * p8 == 0) & (p2 * p6 * p8 == 0)
            kill = (img == 1) & (bn >= 2) & (bn <= 6) & (an == 1) & cond
            if kill.any():
                img[kill] = 0
                changed = True
    return img.astype(bool)


def spine_and_closure(mask):
    """Skeleton pixel count (spine arc proxy) and closure = euclidean distance
    between the two ends of the longest geodesic skeleton path / that path's
    length. 1.0 straight, ~0 head touching tail."""
    sk = skeletonize(mask)
    npx = int(sk.sum())
    if npx < 10:
        return npx, 1.0
    pts = np.column_stack(np.nonzero(sk))  # (y,x)
    index = {(int(y), int(x)): i for i, (y, x) in enumerate(pts)}
    def bfs(start):
        dist = {start: 0.0}
        q = deque([start])
        far, fard = start, 0.0
        while q:
            y, x = q.popleft()
            d = dist[(y, x)]
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dy == 0 and dx == 0:
                        continue
                    nb = (y + dy, x + dx)
                    if nb in index and nb not in dist:
                        nd = d + (1.4142135 if dy and dx else 1.0)
                        dist[nb] = nd
                        if nd > fard:
                            far, fard = nb, nd
                        q.append(nb)
        return far, fard
    start = (int(pts[0][0]), int(pts[0][1]))
    a, _ = bfs(start)
    b, geo = bfs(a)
    if geo <= 0:
        return npx, 1.0
    eu = float(np.hypot(a[0] - b[0], a[1] - b[1]))
    return npx, eu / geo


def weber_contrast(img, mask):
    lum = (img[:, :, 0].astype(np.float64) * 0.299
           + img[:, :, 1] * 0.587 + img[:, :, 2] * 0.114)
    ring = ndimage.binary_dilation(mask, iterations=5) & ~mask
    if not ring.any() or not mask.any():
        return 0.0
    lb, lr = lum[mask].mean(), lum[ring].mean()
    return abs(lb - lr) / max(lr, 1.0)


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--out", required=True)
    ap.add_argument("--label", default=None)
    ap.add_argument("--window", nargs=2, type=int, default=None,
                    help="frame window [A,B) for jerk/beats summaries")
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--plots", action="store_true")
    ap.add_argument("--sheets", action="store_true")
    ap.add_argument("--scale", type=float, default=0.5)
    args = ap.parse_args()

    label = args.label or os.path.splitext(os.path.basename(args.input.rstrip("/\\")))[0]
    os.makedirs(args.out, exist_ok=True)
    frames = load_frames(args.input)
    n = len(frames)
    print(f"{label}: {n} frames")

    masks = [segment(f) for f in frames]
    areas = np.array([m.sum() for m in masks], dtype=np.float64)
    cents = np.array([centroid(m) if m.any() else (0.0, 0.0) for m in masks])
    heads = [head_blob(frames[i], masks[i]) for i in range(n)]
    solid = np.array([convex_solidity(m) for m in masks])
    holes = np.array([hole_fraction(m) * 100 for m in masks])
    weber = np.array([weber_contrast(frames[i], masks[i]) for i in range(n)])

    spines = np.zeros(n)
    closure = np.zeros(n)
    for i, m in enumerate(masks):
        spines[i], closure[i] = spine_and_closure(m)

    # translation-compensated shape rate (%/frame, vs previous frame)
    rate = np.zeros(n)
    for i in range(1, n):
        _, x = aligned_iou_xor(masks[i - 1], masks[i])
        rate[i] = x * 100

    # pose half-life: frames until compensated IoU < 0.5
    half = np.full(n, 0.0)
    for i in range(n):
        hl = n - i  # cap
        for d in range(1, n - i):
            iou, _ = aligned_iou_xor(masks[i], masks[i + d])
            if iou < 0.5:
                hl = d
                break
        half[i] = hl

    csv_path = os.path.join(args.out, f"{label}.csv")
    with open(csv_path, "w") as f:
        f.write("frame,area,cx,cy,solidity,hole_pct,closure,spine_px,"
                "shape_rate_pct,half_life,head_found,head_x,head_y,weber\n")
        for i in range(n):
            hx, hy = (heads[i] if heads[i] else (-1.0, -1.0))
            f.write(f"{i},{areas[i]:.0f},{cents[i][0]:.2f},{cents[i][1]:.2f},"
                    f"{solid[i]:.3f},{holes[i]:.2f},{closure[i]:.3f},"
                    f"{spines[i]:.0f},{rate[i]:.2f},{half[i]:.0f},"
                    f"{int(heads[i] is not None)},{hx:.2f},{hy:.2f},{weber[i]:.3f}\n")
    print("wrote", csv_path)

    a, b = (args.window if args.window else (0, n))
    b = min(b, n)
    win = slice(a, b)

    # jerk table (Recon 2 section 3b): max |delta of delta| over the window
    def jerk(series):
        d = np.diff(series)
        return float(np.abs(np.diff(d)).max()) if len(d) > 1 else 0.0
    hx = np.array([h[0] if h else np.nan for h in heads])
    hy = np.array([h[1] if h else np.nan for h in heads])
    hxw = hx[win]; hyw = hy[win]
    hx_ok = hxw[~np.isnan(hxw)]; hy_ok = hyw[~np.isnan(hyw)]
    with open(os.path.join(args.out, f"{label}-jerk.txt"), "w") as f:
        f.write(f"window frames [{a},{b})\n")
        f.write(f"centroid x max jerk : {jerk(cents[win, 0]):.1f} px\n")
        f.write(f"centroid y max jerk : {jerk(cents[win, 1]):.1f} px\n")
        f.write(f"area      max jerk : {jerk(areas[win]):.0f} px^2\n")
        f.write(f"head x    max jerk : {jerk(hx_ok):.1f} px\n")
        f.write(f"head y    max jerk : {jerk(hy_ok):.1f} px\n")
        f.write(f"centroid y max speed: {np.abs(np.diff(cents[win,1])).max() if b-a>1 else 0:.1f} px/f\n")
        f.write(f"area      max speed: {np.abs(np.diff(areas[win])).max() if b-a>1 else 0:.0f} px^2/f\n")
        f.write(f"shape rate med/p90/max: {np.median(rate[win]):.1f} / "
                f"{np.percentile(rate[win], 90):.1f} / {rate[win].max():.1f} %/f\n")
        f.write(f"half-life med/min: {np.median(half[win]):.0f} / {half[win].min():.0f}\n")
        f.write(f"solidity max: {solid[win].max():.3f}  hole%% max: {holes[win].max():.1f}  "
                f"closure min: {closure[win].min():.3f}\n")
        sp_med = np.median(spines[spines > 0]) if (spines > 0).any() else 1
        f.write(f"spine min/median(clip): {spines[win].min():.0f} / {sp_med:.0f} "
                f"(ratio {spines[win].min()/sp_med:.2f})\n")

    # beat segmentation: activity runs above the window's 35th percentile
    thresh = max(np.percentile(rate[win], 35), 1.0)
    active = rate[win] > thresh
    runs, gaps = [], []
    i = 0
    seg = []
    while i < len(active):
        j = i
        while j < len(active) and active[j] == active[i]:
            j += 1
        (runs if active[i] else gaps).append(j - i)
        seg.append(("RUN" if active[i] else "gap", a + i, a + j))
        i = j
    with open(os.path.join(args.out, f"{label}-beats.txt"), "w") as f:
        f.write(f"window [{a},{b}) activity threshold {thresh:.1f} %/f (35th pct)\n")
        for kind, s, e in seg:
            f.write(f"{kind} f{s}-f{e} ({e-s} frames)\n")
        big = [r for r in runs if r >= 16]
        f.write(f"\nruns>=16f: {len(big)}  ({big})\nall runs: {runs}\ngaps: {gaps}\n")

    # jolts: shape-rate maxima with prominence >= 4 %/f
    r = rate[win]
    jolts = []
    for i in range(1, len(r) - 1):
        if r[i] >= r[i - 1] and r[i] >= r[i + 1]:
            base = min(r[max(0, i - 4):i].min() if i > 0 else r[i],
                       r[i + 1:i + 5].min() if i + 1 < len(r) else r[i])
            if r[i] - base >= 4.0:
                jolts.append(a + i)
    with open(os.path.join(args.out, f"{label}-beats.txt"), "a") as f:
        secs = (b - a) / args.fps
        f.write(f"\njolts (prom>=4): {len(jolts)} in {secs:.2f}s = "
                f"{len(jolts)/secs:.1f}/s at frames {jolts}\n")
        if len(jolts) > 1:
            gaps_j = np.diff(jolts)
            f.write(f"min jolt gap: {gaps_j.min()} frames\n")

    if args.plots:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axes = plt.subplots(4, 1, figsize=(12, 10), sharex=True)
        t = np.arange(n)
        axes[0].plot(t, rate, lw=0.8); axes[0].set_ylabel("shape %/f")
        axes[0].set_ylim(0, 60)
        axes[1].plot(t, half, lw=0.8); axes[1].set_ylabel("half-life f")
        axes[1].axhline(16, color="r", ls=":", lw=0.7); axes[1].set_ylim(0, 80)
        axes[2].plot(t, solid, lw=0.8, label="solidity")
        axes[2].plot(t, holes / 100, lw=0.8, label="hole frac")
        axes[2].axhline(0.70, color="r", ls=":", lw=0.7)
        axes[2].legend(loc="upper right", fontsize=7); axes[2].set_ylim(0, 1)
        axes[3].plot(t, closure, lw=0.8); axes[3].set_ylabel("closure")
        axes[3].axhline(0.40, color="r", ls=":", lw=0.7); axes[3].set_ylim(0, 1)
        axes[3].set_xlabel("frame")
        if args.window:
            for ax in axes:
                ax.axvspan(a, b, color="y", alpha=0.08)
        fig.suptitle(label)
        fig.tight_layout()
        fig.savefig(os.path.join(args.out, f"{label}-panels.png"), dpi=110)
        plt.close(fig)

    if args.sheets:
        s = args.scale
        tw, th = int(W * s), int(H * s)
        cols = 12
        rows = (n + cols - 1) // cols
        sheet = Image.new("RGB", (cols * tw, rows * th), (24, 24, 24))
        for i, fr in enumerate(frames):
            im = Image.fromarray(fr).resize((tw, th))
            sheet.paste(im, ((i % cols) * tw, (i // cols) * th))
        sheet.save(os.path.join(args.out, f"{label}-contact.png"))
        # centroid-locked 2x zoom, window only
        zw, zh = 128, 96
        idxs = list(range(a, b))
        rows = (len(idxs) + cols - 1) // cols
        zoom = Image.new("RGB", (cols * zw * 2, rows * zh * 2), (24, 24, 24))
        for k, i in enumerate(idxs):
            cx, cy = cents[i]
            x0 = int(np.clip(cx - zw // 2, 0, W - zw))
            y0 = int(np.clip(cy - zh // 2, 0, H - zh))
            crop = Image.fromarray(frames[i][y0:y0 + zh, x0:x0 + zw])
            crop = crop.resize((zw * 2, zh * 2), Image.NEAREST)
            zoom.paste(crop, ((k % cols) * zw * 2, (k // cols) * zh * 2))
        zoom.save(os.path.join(args.out, f"{label}-zoom.png"))
        # overlay spot-check: every 12th frame, frame beside mask
        picks = list(range(0, n, max(1, n // 24)))
        ov = Image.new("RGB", (2 * tw, len(picks) * th), (24, 24, 24))
        for k, i in enumerate(picks):
            ov.paste(Image.fromarray(frames[i]).resize((tw, th)), (0, k * th))
            mimg = (masks[i].astype(np.uint8) * 255)
            ov.paste(Image.fromarray(np.stack([mimg] * 3, -1)).resize((tw, th)),
                     (tw, k * th))
        ov.save(os.path.join(args.out, f"{label}-overlay.png"))

    print("done", label)


if __name__ == "__main__":
    main()
