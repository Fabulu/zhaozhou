#!/usr/bin/env python3
"""Per-frame changed-pixel trajectory, shipping bank vs exact-off, per subject.

CLAUDE.md: "trajectory plots of tracked points over time (a flat line IS 'it
never bobs')". For pass 23 the SHAPE of the curve is the claim: on a raised
clip the reaction rises into the press and returns to zero, and outside the
press window it is exactly zero. "N frames changed" cannot show that, and a
contact sheet at thumbnail scale cannot either.

Two series per clip: ANY change, and VISIBLE change (per-channel delta > 32).
The pass-23 implementation found a 2-frame any-change spike on Damage whose
VISIBLE count was LOWER than its neighbours' -- a sub-perceptual dither shift.
Plotting only one of the two would have hidden that, in either direction.

Usage: p23_trajectory.py <shipping-root> <exactoff-root> <out.txt> <out.png> <subject...>
"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

W, H, PAD = 900, 120, 26


def series(a: Path, b: Path):
    anyc, vis, mx = [], [], []
    for f in sorted(p.name for p in a.glob("*.rgb")):
        x = np.frombuffer((a / f).read_bytes()[8:], dtype=np.uint8).astype(np.int16)
        y = np.frombuffer((b / f).read_bytes()[8:], dtype=np.uint8).astype(np.int16)
        d = np.abs(x - y).reshape(-1, 3).max(axis=1)
        anyc.append(int((d > 0).sum())); vis.append(int((d > 32).sum()))
        mx.append(int(d.max()))
    return np.array(anyc), np.array(vis), np.array(mx)


def main(ship, off, out_txt, out_png, subjects):
    ship, off = Path(ship), Path(off)
    img = Image.new("RGB", (W + 2 * PAD, (H + PAD) * len(subjects) + PAD), "white")
    dr = ImageDraw.Draw(img)
    lines = ["# Pass 23 per-frame trajectory: shipping vs exact-off (pass-22 depths).",
             "# The SHAPE is the claim: rise into the press, return to zero.",
             "# subject\tframes\tchanged\tfirst\tlast\tpeak_any\tpeak_vis\tmax_delta"]
    for i, s in enumerate(subjects):
        anyc, vis, mx = series(ship / s, off / s)
        nz = np.nonzero(anyc)[0]
        first = int(nz[0]) if len(nz) else -1
        last = int(nz[-1]) if len(nz) else -1
        lines.append(f"{s}\t{len(anyc)}\t{int((anyc>0).sum())}\t{first}\t{last}\t"
                     f"{int(anyc.max())}\t{int(vis.max())}\t{int(mx.max())}")
        top = PAD + i * (H + PAD)
        dr.rectangle([PAD, top, PAD + W, top + H], outline="#cccccc")
        dr.text((PAD + 2, top - 12),
                f"{s}  {len(anyc)} frames  peak any {int(anyc.max())}  "
                f"peak visible {int(vis.max())}  first {first} last {last}",
                fill="#222222")
        peak = max(1, int(anyc.max()))
        for x in range(W):
            j = int(x * (len(anyc) - 1) / max(1, W - 1))
            dr.line([(PAD + x, top + H),
                     (PAD + x, top + H - int(anyc[j] * (H - 2) / peak))],
                    fill="#9dc0e0")
        pts = [(PAD + x, top + H - int(vis[int(x * (len(vis) - 1) / max(1, W - 1))]
                                       * (H - 2) / peak)) for x in range(W)]
        dr.line(pts, fill="#c2410c", width=2)
    img.save(out_png)
    Path(out_txt).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4],
                          sys.argv[5:]))
