"""Per-frame eye probe -- it picks WHICH FRAME TO LOOK AT. Nothing else.

CLAUDE.md's art law: measurement belongs on the comparison side and never
decides a value. So this probe answers exactly two questions that a reviewer's
eye already asked, and it answers them so the eye can be aimed at the worst
frame instead of the typical one:

  1. IS THE STAR STILL A STAR?  A star is roughly as wide as it is tall
     (major/minor ~ 1.2-1.8). The defect seen by eye in `rest` and `taunt` is
     the star collapsing into a thin BAR lying along the lens. Elongation is
     that defect, stated as a number.

  2. HAS THE STAR LEFT THE ANIMAL?  Direction 5 §5c's hard rule: the star may
     overhang the purple rim onto the pink, but it must NEVER cross the body
     outline into the sky. So: star pixels outside the creature silhouette.

SILHOUETTE is the filled cel-ink outline. Ink is near-black; the darkest
background (the ground band) is far lighter, and the threshold is checked
against both on every frame rather than assumed.

STAR is the cyan core plus its pale rim, restricted to components that touch
the deep-purple eyeball -- the mana motes are the same colours but float in the
loop window and touch no purple.

CAN IT FAIL?  Demonstrated in evidence/probe-canfail.txt on frames judged by
eye FIRST: `curious` (two clean stars) must return low elongation, `rest` (a
star collapsed to a bar on the silhouette) must return high. A probe that
scored them alike would be worthless, and the first two versions of this file
did exactly that -- see the run log.
"""
import numpy as np
from scipy import ndimage

S8 = np.ones((3, 3), int)


def silhouette(a, ink_sum=140):
    ink = a.astype(int).sum(2) < ink_sum
    return ndimage.binary_fill_holes(ndimage.binary_closing(ink, np.ones((3, 3), bool)))


def star_and_purple(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    lum = r + g + b
    purple = (b - r > 18) & (b - g > 45) & (b > 85) & (lum > 170) & (lum < 620)
    cyan = (g - r > 30) & (b - r > 30) & (g > 105)
    pale = (lum > 555) & (np.abs(b - g) < 55) & (b - r > -15)
    star_any = cyan | pale
    lab, _ = ndimage.label(star_any, structure=S8)
    near = ndimage.binary_dilation(purple, np.ones((5, 5), bool))
    keep = [i for i in np.unique(lab[near & (lab > 0)]) if i]
    return (np.isin(lab, keep) if keep else np.zeros_like(purple)), purple, lab, keep


def elongation(mask):
    ys, xs = np.nonzero(mask)
    if len(xs) < 6:
        return 1.0
    c = np.cov(np.vstack([xs - xs.mean(), ys - ys.mean()]))
    ev = np.linalg.eigvalsh(c)
    lo, hi = max(ev[0], 1e-6), max(ev[1], 1e-6)
    return float(np.sqrt(hi / lo))


def frame_report(a):
    star, purple, lab, keep = star_and_purple(a)
    sil = silhouette(a)
    out = {"stars": [], "outside_px": int((star & ~sil).sum()), "star_px": int(star.sum())}
    for i in keep:
        m = lab == i
        if m.sum() < 8:
            continue
        ys, xs = np.nonzero(m)
        out["stars"].append(dict(
            cx=float(xs.mean()), cy=float(ys.mean()), px=int(m.sum()),
            elong=elongation(m), outside=int((m & ~sil).sum()),
        ))
    out["stars"].sort(key=lambda d: d["cx"])
    out["max_elong"] = max([s["elong"] for s in out["stars"]], default=1.0)
    return out
