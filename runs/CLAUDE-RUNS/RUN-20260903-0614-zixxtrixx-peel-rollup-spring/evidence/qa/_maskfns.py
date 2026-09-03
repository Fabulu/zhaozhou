import numpy as np
from scipy import ndimage
import os
W,H=384,240
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

