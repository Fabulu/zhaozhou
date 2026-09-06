"""PAINT THE EYE MASKS GREEN AND LOOK AT WHAT THEY SELECTED.

The gate checklist's rule, and it has caught an instrument fault in three
consecutive reviews: a mask is a hypothesis until you have SEEN what it picked.
`eyesheet.py`'s selftest proves the lens mask rejects three known skies and
finds one synthetic lens. That is necessary and it is not sufficient -- it was
never run against a TRAVELLED eye, which is the whole subject of this lane, and
a locator tuned on a face-on lens can quietly stop finding one that is edge-on.

So this paints, per frame, three overlays side by side at native scale:

    left    the frame, untouched
    middle  LENS mask in green, STAR mask in orange, over a dimmed frame
    right   the masks alone on black -- the shape, with nothing to look at
            except what was actually selected

Read it by asking two questions in order: does the green sit on the purple and
nowhere else, and does the green DISAPPEAR on any frame where the eye is
plainly still visible. The second is the failure that matters here: an eye that
has travelled edge-on shrinks to a few pixels, and a locator that gives up
there would report "the near eye vanished" as a finding when it is only the
instrument blinking.

    python eyemaskpaint.py OUT.png SCALE dir/*.rgb          (all frames)
    python eyemaskpaint.py OUT.png SCALE 3 dir 0,40,90,160  (named frames)
"""
import sys, os, glob
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgbframe import load, save_png
from eyesheet import lens_mask


def star_mask(img):
    """The cyan inner star. Bright, blue-green, and NOT the purple.

    Authored (64, 220, 240): green and blue both far above red. The mauve sky
    and the deep-purple lens both fail the g-over-r term, which is the term
    doing the work -- the lens is violet (r > g) and the star is cyan (g > r).
    """
    a = img.astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (g > r + 40) & (b > r + 40) & (g > 110) & (b > 110)


def white_mask(img):
    """The white outer star: bright and near-neutral, on a creature that has
    no other white. Kept separate from the cyan so a plate can show whether
    the white has separated from the blue -- §5b's one hard prohibition."""
    a = img.astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx = a.max(axis=2)
    mn = a.min(axis=2)
    return (mx > 200) & ((mx - mn) < 40)


def panel(img):
    lens = lens_mask(img)
    star = star_mask(img)
    whit = white_mask(img)
    dim = (img.astype(int) * 45 // 100).astype(np.uint8)
    over = dim.copy()
    over[lens] = (40, 255, 40)     # GREEN: the purple lens
    over[star] = (255, 150, 20)    # ORANGE: the cyan star
    over[whit] = (255, 60, 255)    # MAGENTA: the white star
    solo = np.zeros_like(img)
    solo[lens] = (40, 255, 40)
    solo[star] = (255, 150, 20)
    solo[whit] = (255, 60, 255)
    return over, solo, int(lens.sum()), int(star.sum()), int(whit.sum())


def main(argv):
    out, scale = argv[1], int(argv[2])
    if len(argv) >= 5 and not argv[3].endswith(".rgb") and "*" not in argv[3]:
        d = argv[3]
        files = [os.path.join(d, "%04d.rgb" % int(k)) for k in argv[4].split(",")]
    else:
        files = []
        for pat in argv[3:]:
            files.extend(sorted(glob.glob(pat)) if "*" in pat else [pat])
    rows = []
    print("%-28s %8s %8s %8s" % ("frame", "lens_px", "cyan_px", "white_px"))
    for f in files:
        img = load(f)
        over, solo, nl, ns, nw = panel(img)
        print("%-28s %8d %8d %8d" % (os.path.basename(f), nl, ns, nw))
        rows.append(np.concatenate([img, over, solo], axis=1))
    sheet = np.concatenate(rows, axis=0)
    if scale > 1:
        sheet = sheet.repeat(scale, axis=0).repeat(scale, axis=1)
    save_png(sheet, out)
    print("wrote", out, sheet.shape)


if __name__ == "__main__":
    main(sys.argv)
