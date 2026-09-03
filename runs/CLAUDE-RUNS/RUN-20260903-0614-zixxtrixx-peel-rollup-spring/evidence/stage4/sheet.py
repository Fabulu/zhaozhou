#!/usr/bin/env python3
"""Every-frame contact sheet from raw .rgb frames (384x240)."""
import sys, os, glob
import numpy as np
from PIL import Image

def load(p):
    a = np.fromfile(p, dtype=np.uint8)
    w, h = a[:8].view(np.uint32)
    return a[8:].reshape(int(h), int(w), 3)

def main():
    d, out = sys.argv[1], sys.argv[2]
    f0 = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    f1 = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
    step = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    crop = sys.argv[6] if len(sys.argv) > 6 else None  # "x0:x1:y0:y1"
    files = sorted(glob.glob(os.path.join(d, "*.rgb")))[f0:f1+1:step]
    imgs = [load(p) for p in files]
    if crop:
        x0, x1, y0, y1 = map(int, crop.split(":"))
        imgs = [im[y0:y1, x0:x1] for im in imgs]
    cols = 8
    rows = (len(imgs) + cols - 1) // cols
    h, w, _ = imgs[0].shape
    sheet = np.zeros((rows * (h + 12), cols * (w + 2), 3), dtype=np.uint8)
    from PIL import ImageDraw
    for i, im in enumerate(imgs):
        r, c = divmod(i, cols)
        sheet[r*(h+12):r*(h+12)+h, c*(w+2):c*(w+2)+w] = im
    img = Image.fromarray(sheet)
    dr = ImageDraw.Draw(img)
    for i in range(len(imgs)):
        r, c = divmod(i, cols)
        dr.text((c*(w+2)+2, r*(h+12)+h), f"f{f0+i*step}", fill=(255,255,0))
    img.save(out)
    print("wrote", out, len(imgs), "frames")

if __name__ == "__main__":
    main()
