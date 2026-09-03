"""Header-verified .rgb frame tools for the colour-light repair run.

Every .rgb frame carries an 8-byte header: u32 w | u32 h (little-endian),
then h*w*3 bytes of RGB. Four diagnostics on this creature have lied; this
reader refuses to guess. All loads assert the header AND the byte count.
"""
import struct, sys, os
import numpy as np
from PIL import Image

EXPECT_W, EXPECT_H = 384, 240

def load(path, expect=(EXPECT_W, EXPECT_H)):
    raw = open(path, "rb").read()
    if len(raw) < 8:
        raise ValueError(f"{path}: too short for header")
    w, h = struct.unpack("<II", raw[:8])
    if expect is not None and (w, h) != expect:
        raise ValueError(f"{path}: header says {w}x{h}, expected {expect}")
    need = 8 + w * h * 3
    if len(raw) != need:
        raise ValueError(f"{path}: {len(raw)} bytes, header implies {need}")
    return np.frombuffer(raw, dtype=np.uint8, offset=8).reshape(h, w, 3)

def save_png(arr, path, scale=1):
    img = Image.fromarray(arr, "RGB")
    if scale != 1:
        img = img.resize((arr.shape[1]*scale, arr.shape[0]*scale), Image.NEAREST)
    img.save(path)

def diff_vis(a, b):
    """Amplified signed difference on grey: shows WHERE and WHICH colour."""
    d = a.astype(np.int16) - b.astype(np.int16)
    amp = np.clip(128 + d * 4, 0, 255).astype(np.uint8)
    return amp

def stats(a, b):
    d = np.abs(a.astype(np.int16) - b.astype(np.int16))
    changed = (d.sum(axis=2) > 0)
    return {
        "changed_px": int(changed.sum()),
        "max_delta_rgb": [int(d[:, :, c].max()) for c in range(3)],
        "mean_delta_where_changed": (float(d[changed].mean()) if changed.any() else 0.0),
    }

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "png":
        arr = load(sys.argv[2]); save_png(arr, sys.argv[3], scale=int(sys.argv[4]) if len(sys.argv) > 4 else 1)
    elif cmd == "diff":
        a, b = load(sys.argv[2]), load(sys.argv[3])
        save_png(diff_vis(a, b), sys.argv[4])
        print(stats(a, b))
    else:
        raise SystemExit("png <in> <out> [scale] | diff <a> <b> <out>")
