"""Direction 30 eye-compare: vertical stack GEN18 / NEW / CURRENT(v5) per frame.

Native resolution panels, stacked with 2 px separators, then scaled 2x nearest
for the eye. Labels burned as 3 px bars of solid colour on the left edge
(white / yellow / red) so the panels cannot be confused.
"""
import sys, os
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "..", "tools", "reel"))
from rgbframe import load, save_png

def panel(path, tag):
    img = load(path).copy()
    img[:, :3, :] = tag
    return img

def main(gen18_dir, new_dir, v5_dir, subject, frames, outdir):
    os.makedirs(outdir, exist_ok=True)
    for f in frames:
        name = "%04d.rgb" % f
        a = panel(os.path.join(gen18_dir, subject, name), (255, 255, 255))
        b = panel(os.path.join(new_dir, subject, name), (255, 255, 0))
        c = panel(os.path.join(v5_dir, subject, name), (255, 0, 0))
        sep = np.zeros((2, a.shape[1], 3), np.uint8)
        stack = np.concatenate([a, sep, b, sep, c], axis=0)
        big = np.repeat(np.repeat(stack, 2, axis=0), 2, axis=1)
        out = os.path.join(outdir, "%s-f%04d.png" % (subject, f))
        save_png(big, out)
        print("wrote", out)

if __name__ == "__main__":
    g18, new, v5, subject = sys.argv[1:5]
    frames = [int(x) for x in sys.argv[5:]]
    main(g18, new, v5, subject, frames,
         os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "look"))
