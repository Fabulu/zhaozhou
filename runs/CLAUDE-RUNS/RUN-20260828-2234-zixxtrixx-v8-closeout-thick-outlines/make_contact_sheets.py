#!/usr/bin/env python3
"""Build every-frame 384x240 contact sheets from reel RGB dumps."""
from pathlib import Path
import struct
import sys

import numpy as np
from PIL import Image

SITE = Path(r"C:\programmieren\zencrifice\Upheaval\website")
OUT = Path(__file__).resolve().parent / "evidence"
W, H = 384, 240
THUMB_W, THUMB_H = W // 4, H // 4
PER_ROW = 16
CANONICAL = [
    "zixxtrixx-attack", "zixxtrixx-balance", "zixxtrixx-damage",
    "zixxtrixx-death", "zixxtrixx-death2", "zixxtrixx-fall",
    "zixxtrixx-hit", "zixxtrixx-hitfloor", "zixxtrixx-idle",
    "zixxtrixx-knockdown", "zixxtrixx-look", "zixxtrixx-run",
    "zixxtrixx-salto-dummy", "zixxtrixx-salto-fly",
    "zixxtrixx-salto-six", "zixxtrixx-taunt", "zixxtrixx-walk",
]


def read_frame(path):
    raw = path.read_bytes()
    w, h = struct.unpack_from("<II", raw, 0)
    if (w, h) != (W, H) or len(raw) != 8 + W * H * 3:
        raise ValueError(f"{path}: expected {W}x{H} RGB888 reel dump")
    return np.frombuffer(raw, dtype=np.uint8, offset=8).reshape(H, W, 3)


def sheet(subject):
    frames = sorted((SITE / "scratch-reel" / subject).glob("*.rgb"))
    if not frames:
        raise FileNotFoundError(f"no frames for {subject}")
    thumbs = [np.asarray(Image.fromarray(read_frame(frame)).resize(
        (THUMB_W, THUMB_H), Image.Resampling.NEAREST)) for frame in frames]
    rows = []
    blank = np.zeros((THUMB_H, THUMB_W, 3), dtype=np.uint8)
    for start in range(0, len(thumbs), PER_ROW):
        row = thumbs[start:start + PER_ROW]
        row += [blank] * (PER_ROW - len(row))
        rows.append(np.concatenate(row, axis=1))
    OUT.mkdir(parents=True, exist_ok=True)
    dst = OUT / f"contact-every-frame-{subject}.png"
    Image.fromarray(np.concatenate(rows, axis=0)).save(dst, optimize=True)
    print(f"{subject}: {len(frames)} frames -> {dst}")


if __name__ == "__main__":
    for name in (sys.argv[1:] or CANONICAL):
        sheet(name)
