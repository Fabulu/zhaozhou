"""Locate the eye by its own pigment and crop a face plate around it.

NOT a measurement of the eye -- it only decides where to LOOK. The deep-purple
eyeball is the only strongly blue-violet, mid-dark thing on the creature; the
sky in the two violet-backdrop clips is much darker, and the aqua mana is
green-dominant, so both are excluded rather than tuned away.
"""
import numpy as np
from PIL import Image

def purple_mask(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    return (b > 80) & (b < 235) & (b - g > 45) & (r - g > 20) & (b - r > -10) & (r + g + b > 180)

def cyan_mask(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    return (g > 120) & (b > 120) & (g - r > 45) & (b - r > 45)

def dominant_box(mask, pad=4, frac=0.02):
    """Bounding box of the mask's dense core: drop rows/cols holding < frac of hits."""
    ys, xs = np.nonzero(mask)
    if len(xs) < 12:
        return None
    hx = np.bincount(xs, minlength=mask.shape[1])
    hy = np.bincount(ys, minlength=mask.shape[0])
    kx = np.nonzero(hx >= max(1, frac * hx.max()))[0]
    ky = np.nonzero(hy >= max(1, frac * hy.max()))[0]
    x0, x1 = max(0, kx.min() - pad), min(mask.shape[1], kx.max() + 1 + pad)
    y0, y1 = max(0, ky.min() - pad), min(mask.shape[0], ky.max() + 1 + pad)
    return x0, y0, x1, y1

def face_box(a, pad=6):
    return dominant_box(purple_mask(a), pad)

def native(path):
    im = Image.open(path).convert("RGB")
    if im.size != (384, 240):
        im = im.resize((384, 240), Image.NEAREST)
    return im
