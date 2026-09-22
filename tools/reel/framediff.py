"""Where two renders of the same clip differ MOST -- by frame, and by region.

Sampling frames by index finds the typical frame and misses the one that moved
(CLAUDE.md, "Seeing the work properly"). Every by-eye ladder on this creature
needs the same question answered first: which frames does this knob actually
change, and where on the screen? This answers it from the frames themselves.

    python framediff.py A_DIR B_DIR [X,Y,W,H] [--top N]

A_DIR and B_DIR each hold 0000.rgb, 0001.rgb, ... for the same subject. The box
is in NATIVE 384x240 pixels and defaults to the whole frame. Prints the top N
frames by changed-pixel count inside the box, then the bounding box of the
changes on the worst frame -- so the crop that follows is chosen by the
difference rather than by guesswork.

It compares; it decides nothing. Reads frames through rgbframe, which honours
the header (see that file for why that sentence is here).
"""
import sys, os
import numpy as np
from rgbframe import load


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    a_dir, b_dir = argv[1], argv[2]
    box = None
    top = 10
    rest = argv[3:]
    i = 0
    while i < len(rest):
        if rest[i] == "--top":
            top = int(rest[i + 1])
            i += 2
            continue
        box = tuple(int(v) for v in rest[i].split(","))
        i += 1
    names = sorted(n for n in os.listdir(a_dir) if n.endswith(".rgb"))
    rows = []
    for n in names:
        pa, pb = os.path.join(a_dir, n), os.path.join(b_dir, n)
        if not os.path.exists(pb):
            continue
        fa, fb = load(pa), load(pb)
        if box is not None:
            x, y, w, h = box
            fa = fa[y:y + h, x:x + w]
            fb = fb[y:y + h, x:x + w]
        d = np.any(fa != fb, axis=2)
        rows.append((int(d.sum()), n, d))
    rows.sort(key=lambda r: -r[0])
    print("frames compared: %d   box: %s" % (len(rows), box or "full"))
    for cnt, n, _ in rows[:top]:
        print("  %-10s %6d changed px" % (n, cnt))
    if rows and rows[0][0] > 0:
        d = rows[0][2]
        ys, xs = np.nonzero(d)
        ox, oy = (box[0], box[1]) if box else (0, 0)
        print("worst frame %s change bbox (native): x=%d..%d y=%d..%d" %
              (rows[0][1], ox + xs.min(), ox + xs.max(), oy + ys.min(),
               oy + ys.max()))
    total = sum(c for c, _, _ in rows)
    changed = sum(1 for c, _, _ in rows if c > 0)
    print("frames changed: %d of %d   total changed px: %d" %
          (changed, len(rows), total))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
