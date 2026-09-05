"""Track the creature per frame. A flat line is a finding (checklist item 15).

⚠ THE FIRST VERSION OF THIS FILE WAS WRONG AND IS KEPT AS A WARNING.
It located the creature by "saturated red-dominant pink". The sky in these
clips is a mauve-pink whose lower rows measure r-g=69, sat=69 against a
threshold of sat>70 -- so the mask swallowed the sky, reported the creature as
8,455 px moving 38 px in `hasty` (it crosses the whole frame), and declared
100% of frames edge-clipped in every clip. That is EXACTLY the fault
PASS-6-INPUTS.md §9 records from the last pass: "a creature mask that matched
the orange horizon band, claiming 15 of 16 clips were edge-clipped in every
frame." It was caught by disbelieving the number against a contact sheet, not
by the tool.

THE LOCATOR IS NOW THE CEL INK OUTLINE, calibrated per frame rather than
assumed. Measured on hasty f1: ink lum-sum ~65-140, ground 187, sky 327-390.
Nothing in the scene except the creature is that dark, and the outline is a
property of the creature rather than of its colour, so it survives the body
going into shadow and the mana going white over it.

CAN IT FAIL? The calibration is asserted every frame: if the fraction of the
frame classified as creature exceeds 25%, the mask has leaked into scenery and
the frame is reported as SUSPECT rather than measured.
"""
import os, sys
import numpy as np
from PIL import Image
from scipy import ndimage

INK_MAX = 140
LEAK_FRAC = 0.25


def creature_mask(a):
    l = a.astype(int).sum(2)
    ink = l < INK_MAX
    # the outline plus everything it encloses, when it encloses anything
    filled = ndimage.binary_fill_holes(ndimage.binary_closing(ink, np.ones((5, 5), bool)))
    return ink, filled


def track(framedir):
    rows = []
    for fn in sorted(os.listdir(framedir)):
        a = np.asarray(Image.open(os.path.join(framedir, fn)).convert("RGB"))
        ink, filled = creature_mask(a)
        m = filled if filled.sum() > ink.sum() else ink
        n = int(m.sum())
        suspect = n > LEAK_FRAC * m.size
        if n < 15 or suspect:
            rows.append(dict(fn=fn, n=n, suspect=suspect, cx=None, cy=None, box=None))
            continue
        ys, xs = np.nonzero(m)
        rows.append(dict(fn=fn, n=n, suspect=False, cx=float(xs.mean()), cy=float(ys.mean()),
                         box=(int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))))
    return rows


def summarise(name, rows, w=384, h=240):
    live = [r for r in rows if r["cx"] is not None]
    gone = [i for i, r in enumerate(rows) if r["n"] < 15]
    susp = [i for i, r in enumerate(rows) if r["suspect"]]
    clip = [i for i, r in enumerate(rows) if r["box"] and
            (r["box"][0] <= 0 or r["box"][1] <= 0 or r["box"][2] >= w - 1 or r["box"][3] >= h - 1)]
    xs = np.array([r["cx"] for r in live]); ys = np.array([r["cy"] for r in live])
    px = np.array([r["n"] for r in live])
    dy = np.diff(ys); sgn = np.sign(dy); sgn = sgn[sgn != 0]
    rev = int((np.diff(sgn) != 0).sum()) if len(sgn) > 1 else 0
    print(f"{name}: {len(rows)} frames   ink+fill px {px.min()}-{px.max()} (med {int(np.median(px))})")
    print(f"   centroid x {xs.min():.0f}..{xs.max():.0f} (span {xs.max()-xs.min():.0f})"
          f"   y {ys.min():.0f}..{ys.max():.0f} (span {ys.max()-ys.min():.0f})")
    print(f"   net drift  x {xs[-1]-xs[0]:+.0f}px   y {ys[-1]-ys[0]:+.0f}px   y-reversals {rev}")
    print(f"   OFF-FRAME {len(gone)}   SUSPECT-MASK {len(susp)}   EDGE-TOUCHING {len(clip)}"
          f" ({100*len(clip)/len(rows):.0f}%) {clip[:10]}")
    return rows


if __name__ == "__main__":
    for fd in sys.argv[1:]:
        summarise(os.path.basename(fd), track(fd))
