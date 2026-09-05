"""Contact sheets for the experimental mana reel.

Two modes, both deliberate (07-MOTION-STYLE: uniform sampling finds the
typical frame and misses the broken one):

  sheet.py grid  <clipdir> <out.png> <scale> f0 f1 f2 ...
      named frames, NATIVE unless a scale is given. The verdict is taken at
      scale 1; a fold that only reads at 4x has not read.

  sheet.py strip <clipdir> <out.png> <scale> <start> <step> <count>
      a run of consecutive frames -- for watching a snap or a knead land.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "..", "zhaozhou", "tools", "reel"))
import rgbframe
from PIL import Image, ImageDraw

def load(d, f):
    a = rgbframe.load(os.path.join(d, "%04d.rgb" % f))
    return Image.fromarray(a, "RGB")

def build(d, frames, scale, cols=4):
    ims = [(f, load(d, f)) for f in frames]
    w, h = ims[0][1].size
    w, h = w * scale, h * scale
    rows = (len(ims) + cols - 1) // cols
    lab = 12
    sheet = Image.new("RGB", (cols * w, rows * (h + lab)), (24, 24, 28))
    dr = ImageDraw.Draw(sheet)
    for i, (f, im) in enumerate(ims):
        if scale != 1:
            im = im.resize((w, h), Image.NEAREST)
        x, y = (i % cols) * w, (i // cols) * (h + lab)
        sheet.paste(im, (x, y + lab))
        dr.text((x + 3, y + 1), "f%d" % f, fill=(210, 210, 220))
    return sheet

if __name__ == "__main__":
    m = sys.argv[1]
    d, out, scale = sys.argv[2], sys.argv[3], int(sys.argv[4])
    if m == "grid":
        frames = [int(x) for x in sys.argv[5:]]
    else:
        st, sp, n = int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7])
        frames = [st + i * sp for i in range(n)]
    build(d, frames, scale).save(out)
    print(out, len(frames), "frames, scale", scale)
