"""THE NEAR-EYE BAR, counted rather than argued about.

The bar was originally reported as a rate -- "the near eye's star collapses
into a BAR on 96.1% of `taunt`'s frames, 78% of `taunt2`, 74% of `rest`" -- and
that is the right shape of number, because the fault is intermittent and a
handful of stills will show you whichever answer you went looking for.

WHY MEASURING PIXELS IS LEGITIMATE HERE, when this project's own law says not
to derive 3D form from a projection: the bar IS a projection. The question is
not "how thick is the star" but "how much star survives on screen", and screen
is where it lives. Measuring the rendered frame is measuring the thing itself.

WHAT IS COUNTED, per frame, per connected star blob:
    area      how many cyan pixels
    elongation  the blob's principal-axis ratio from its own second moments

A blob is A BAR when it is both SMALL and LONG -- either alone is innocent. A
star seen face-on and far away is small and round; a star that is merely large
and elongated is a star drawn at an angle. The scratch is the conjunction, and
the thresholds are named constants because they are a judgement, not a
discovery: they were set by looking at the frames the eye calls broken and
picking a line that separates them, then checked against the frames the eye
calls fine.

    python barcensus.py render/eyelab-VARIANT [render/eyelab-OTHER ...]

==== WHERE THIS INSTRUMENT IS VALID, AND WHERE IT LIES ====================

It was built for this lane's question and it RANKED A VARIANT FIRST THAT THE
EYE REJECTS. That is recorded here rather than quietly worked around, because
it is the project's own law arriving on schedule -- measurement never trumps
actually looking at things.

  * `bar-domed` at a 62 mm drop scores the BEST bar rate in the whole table,
    28.1% against the control's 55.4%. Looking at the render, its star has lost
    its arms entirely: the dome pulled the tips 48 mm INSIDE the purple lens and
    what survives is the stubby centre. A stub is COMPACT, and compactness is
    half of what this census rewards, so destroying the star scores as curing
    the scratch. The number is not wrong; the question it answers is not the
    question that was asked.

  * `bar-thicker` scores a better rate AND a lower mean area (72 px against 96)
    -- because the white slab swallowed the cyan and this counts CYAN. A
    variant that removes the thing being measured always measures better.

SO: this census is trustworthy for comparing variants that leave the star's
GEOMETRY alone and change only where the eye is pointed -- the travel ladder,
which is what it was really needed for, and where it agrees with the eye
(control 55.4%, 14 deg 32.7%, 22 deg 42.7%, 45 deg 62.1%: a minimum at 14).
Across variants that reshape the star it is a description, never a ranking.
"""
import sys, os, glob
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgbframe import load
from eyemaskpaint import star_mask

# --- the two thresholds, and they are authored ---------------------------
# A star at this camera occupies roughly 60-160 px face-on. Under 26 px it has
# stopped being a shape at 384x240 whatever its aspect.
BAR_AREA_PX = 26
# Elongation above this and the blob is a stroke, not a four-point star. The
# star's own drawn aspect is 1.70 (pass 7 measured it off the Front sheet), so
# 3.0 leaves real headroom for honest foreshortening before anything is called
# broken.
BAR_ELONG = 3.0
# Under this the star has effectively VANISHED, which is a different and worse
# failure than being a scratch -- the eye has no pupil at all.
GONE_PX = 6


def blobs(m):
    from scipy import ndimage as nd
    lab, n = nd.label(m, structure=np.ones((3, 3)))
    out = []
    for i in range(1, n + 1):
        ys, xs = np.nonzero(lab == i)
        a = len(xs)
        if a < 3:
            continue
        x = xs - xs.mean()
        y = ys - ys.mean()
        cov = np.array([[(x * x).mean(), (x * y).mean()],
                        [(x * y).mean(), (y * y).mean()]])
        w = np.linalg.eigvalsh(cov)
        w = np.clip(w, 1e-6, None)
        out.append((a, float(np.sqrt(w[1] / w[0]))))
    return out


def census(d):
    files = sorted(glob.glob(os.path.join(d, "*.rgb")))
    n = len(files)
    if n == 0:
        return None
    bar = gone = both_ok = 0
    areas = []
    for f in files:
        bs = blobs(star_mask(load(f)))
        vis = [b for b in bs if b[0] >= GONE_PX]
        areas.append(sum(b[0] for b in bs))
        if len(vis) == 0:
            gone += 1
            continue
        # the WORST visible star on the frame -- the near eye is the one that
        # breaks, and averaging the two hides exactly the eye in question
        worst = min(vis, key=lambda b: (b[0], -b[1]))
        if worst[0] < BAR_AREA_PX or worst[1] > BAR_ELONG:
            bar += 1
        if len(vis) >= 2:
            both_ok += 1
    return dict(n=n, bar=bar, gone=gone, both=both_ok,
                mean_area=float(np.mean(areas)))


def main(dirs):
    print("%-34s %6s %8s %8s %9s %10s" %
          ("variant", "frames", "BAR%", "GONE%", "2-eyes%", "mean_px"))
    for d in dirs:
        c = census(d)
        if c is None:
            print("%-34s  no frames" % os.path.basename(d))
            continue
        print("%-34s %6d %7.1f%% %7.1f%% %8.1f%% %10.1f" %
              (os.path.basename(d), c["n"], 100.0 * c["bar"] / c["n"],
               100.0 * c["gone"] / c["n"], 100.0 * c["both"] / c["n"],
               c["mean_area"]))


if __name__ == "__main__":
    main(sys.argv[1:])
