"""One frame from every variant, side by side, so the ten are compared like
with like. Direction 6: the verdict is taken at NATIVE 384x240; the zoom
sheet exists only to explain WHY a native read failed.

  compare.py <out.png> <scale> name=frame name=frame ...
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "..", "zhaozhou", "tools", "reel"))
import rgbframe
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(__file__), "..", "out")

if __name__ == "__main__":
    out, scale = sys.argv[1], int(sys.argv[2])
    items = []
    for a in sys.argv[3:]:
        n, f = a.split("=")
        p = os.path.join(ROOT, "manalab-" + n, "%04d.rgb" % int(f))
        items.append((n, int(f), Image.fromarray(rgbframe.load(p), "RGB")))
    w, h = items[0][2].size
    w, h = w * scale, h * scale
    cols = 2 if scale > 2 else 3
    rows = (len(items) + cols - 1) // cols
    lab = 13
    sh = Image.new("RGB", (cols * w, rows * (h + lab)), (20, 20, 24))
    dr = ImageDraw.Draw(sh)
    for i, (n, f, im) in enumerate(items):
        if scale != 1:
            im = im.resize((w, h), Image.NEAREST)
        x, y = (i % cols) * w, (i // cols) * (h + lab)
        sh.paste(im, (x, y + lab))
        dr.text((x + 3, y + 2), "%s  f%d" % (n, f), fill=(225, 225, 235))
    sh.save(out)
    print(out, len(items), "panels, scale", scale)
