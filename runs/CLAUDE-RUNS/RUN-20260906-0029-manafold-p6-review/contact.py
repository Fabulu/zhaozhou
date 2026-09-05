"""Contact sheet of EVERY frame of a clip. Not a sample -- every frame.

Uniform sampling finds the typical frame and misses the broken one
(CLAUDE.md, Seeing the work properly), so this never subsamples.
"""
import os, sys
from PIL import Image, ImageDraw

def sheet(framedir, out, cols=16, scale=1, label_every=8):
    fns = sorted(os.listdir(framedir))
    w, h = Image.open(os.path.join(framedir, fns[0])).size
    tw, th = int(w * scale), int(h * scale)
    rows = (len(fns) + cols - 1) // cols
    im = Image.new("RGB", (cols * tw, rows * th), (12, 12, 14))
    d = ImageDraw.Draw(im)
    for i, fn in enumerate(fns):
        f = Image.open(os.path.join(framedir, fn)).convert("RGB")
        if scale != 1:
            f = f.resize((tw, th), Image.LANCZOS if scale < 1 else Image.NEAREST)
        x, y = (i % cols) * tw, (i // cols) * th
        im.paste(f, (x, y))
        if i % label_every == 0:
            d.text((x + 2, y + 1), str(i + 1), fill=(255, 255, 0))
    im.save(out)
    return len(fns), im.size

if __name__ == "__main__":
    n, sz = sheet(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 16,
                  float(sys.argv[4]) if len(sys.argv) > 4 else 1.0)
    print(n, "frames ->", sz)
