"""eyesweep -- THE EYE GATE THAT READS PIXELS, NOT THE CHANNEL.

WHY THIS FILE EXISTS
--------------------
Three passes built an eye-travel channel, gated it, and reported it working.
The owner looked at the result and said the eyes do not move left and right at
all. Every one of those gates was HONEST -- pass 13's numbers reproduce to this
day -- and every one of them measured the mechanism's own quantity: a carrier
angle, in degrees, in body space, in a lane nobody can see.

    A measurement of the mechanism is not evidence about the picture.
                                    -- OWNER-DIRECTION-11, closing line

So this gate measures the only thing that has ever settled this question: where
the eye's INDIGO LENS is on screen, in native pixels, on frames rendered from
the shipping build under the shipping env.

WHAT IT MEASURES, per frame
---------------------------
  lens_px   indigo lens pixels. This IS the foreshortening measurement: an eye
            turned edge-on is a blade, and a blade is a small number. `hit` f0
            reads 871 with the fix and 239 at the pass-14 peak.
  star_px   cyan star pixels inside the lens region (see the mote note).
  eyes      lens-plus-star blobs above kMinBlobPx: 2 when both eyes read, 1
            when one has gone round the ball.
  u         the lens centroid ACROSS THE BODY, 0 at the body's left edge and
            1000 at its right, measured IN THE LENS'S OWN ROWS. Body-relative
            because a clip that walks the creature across the frame would
            otherwise score a huge "sweep" for eyes that never moved; and
            restricted to those rows because the antenna is the same magenta
            as the ball and swings, which would make the denominator wobble.

and per clip the peak-to-peak of `u` -- THE SWEEP -- plus the share of frames
showing two eyes.

⚠ THE MOTE CONFOUND, AND HOW IT IS BEATEN. The mana motes in the antenna loop
are aqua and outnumber the star pixels roughly two to one, so a plain cyan rule
measures the antenna. The star is discriminated by ADJACENCY: every star sits
inside an indigo lens and nothing else on this creature does, so the cyan mask
is intersected with a dilation of the lens mask. That is a property of the
model, not a tuned threshold.

⚠ WHAT THIS GATE IS *NOT* VALID FOR, SAID FIRST RATHER THAN BURIED
------------------------------------------------------------------
**The fixed-camera clips only** -- which is 26 of the 28 shipped subjects, and
where the owner's complaint is sharpest. On the ORBITING idle the camera passes
through a backlit quadrant where the lens renders near-black and drops under
this mask entirely: `hover` f150 shows two large open eyes with whole stars to
the eye and reads 44 lens pixels here. The number is wrong and the picture is
right, so the picture governs and this tool declines the clip.

That limit is stated because FOUR masks were built for this gate in one sitting
and THREE of them were confidently wrong -- 10-GATE-CHECKLIST item 40, arriving
exactly on schedule:

  1. A loose indigo rule, `(b>g+45) & (b>70) & (r>g+10) & (r<b)`. Clean on the
     sunset backdrop: 1137 px on `hit` f0, 25 of them off the eyes. On
     `channel` it selected FORTY-FOUR THOUSAND, because that clip's violet
     night sky satisfies every clause. The gate ran, separated both its legs
     and printed PASS while measuring the sky.
  2. A "red dominant, green low" body rule. The sunset sky is (165, 97, 107) --
     red 68 over green -- so `r > g + 70` selected 58,000 px spanning the FULL
     FRAME WIDTH, and `u` was being normalised by the width of the sky. The
     separator that works is BLUE OVER GREEN, the thing that makes a colour
     magenta rather than orange: the body carries 30-71, the sky 8-18, the
     terrain negative.
  3. A brightness-free indigo rule written to rescue the backlit quadrant. It
     rescued the quadrant and let the night sky straight back in.

  The first two were caught by running them somewhere the answer was known.
  The third was caught by a PICTURE disagreeing with a number. None was caught
  by the numbers looking wrong -- every one of them separated its legs cleanly.

⚠ AND THE LEGS ARE PART OF THE GATE, not a note about it. `gate` refuses to
report a verdict without them:

    U02_EYE_TRAVEL_PIN=0     travel pinned OFF. This isolates the TRAVEL and
                             nothing else: the pass-15 deform-follow lane is
                             still running in this leg, so the two legs differ
                             in exactly one channel, which is what makes the
                             sweep comparison mean something. (With
                             kEyeDeformFollowPm ALSO at 0 the same pin
                             reproduces the pass-14 bank byte for byte -- 0
                             differing frames of 140 on `hit` -- which is how
                             both negatives were proved rather than asserted.)
                             The SWEEP must collapse against live; if it does
                             not, this tool is measuring the body.
    U02_EYE_TRAVEL_PIN=867   the pass-14 shipped peak, the picture the owner
                             complained about. The two-eye share and the lens
                             area must BOTH collapse. A gate that cannot see
                             the fault it was built for is not a gate.

USAGE
  python eyesweep.py scan <dir-of-rgb>                    per-clip summary
  python eyesweep.py csv  <dir-of-rgb>                    per-frame CSV
  python eyesweep.py gate <live-dir> <pin0-dir> <pin867-dir>
"""
import glob
import os
import sys

import numpy as np

from rgbframe import load

# ---- the masks -----------------------------------------------------------

#: a lens+star blob smaller than this is a sliver, not a readable eye.
kMinBlobPx = 20
#: how far a cyan pixel may sit from lens indigo and still be that lens's star.
kStarReachPx = 3


def dilate(m, n):
    out = m.copy()
    for _ in range(n):
        p = np.zeros_like(out)
        p[1:, :] |= out[:-1, :]
        p[:-1, :] |= out[1:, :]
        p[:, 1:] |= out[:, :-1]
        p[:, :-1] |= out[:, 1:]
        out = out | p
    return out


def lens_mask(a):
    """The eye lens: BRIGHT, deeply saturated indigo.

    `b > 140` is the clause that keeps the violet night sky out (it runs
    41-99) and it is also the clause that loses the idle's backlit quadrant.
    That trade is deliberate, and it is why this gate declines orbiting clips
    rather than quietly reporting a wrong number for them.
    """
    r, g, b = a[..., 0].astype(np.int32), a[..., 1].astype(np.int32), a[..., 2].astype(np.int32)
    return (b > 140) & (g < 90) & (r < b - 60) & (r > g)


def star_mask(a, lens):
    """The cyan star, by HUE rather than by brightness, inside its own lens.

    Hue-relative because the star's absolute brightness swings with the light
    rig; adjacency-gated because the antenna's aqua motes share its hue.
    """
    r, g, b = a[..., 0].astype(np.int32), a[..., 1].astype(np.int32), a[..., 2].astype(np.int32)
    return (g > r + 15) & (b > r + 15) & (b > 40) & dilate(lens, kStarReachPx)


def body_mask(a):
    """The pink ball -- MAGENTA, and magenta is the word doing the work.

    Blue over green separates the animal from an orange sunset; `r > b` keeps
    the indigo lens out of the body it sits on.
    """
    r, g, b = a[..., 0].astype(np.int32), a[..., 1].astype(np.int32), a[..., 2].astype(np.int32)
    return (r > g + 40) & (b > g + 25) & (r > b)


def blobs(m, min_px):
    """Connected components, 4-neighbour. Returns (count >= min_px, sizes)."""
    ys, xs = np.nonzero(m)
    pts = set(zip(ys.tolist(), xs.tolist()))
    sizes = []
    while pts:
        stack = [pts.pop()]
        n = 1
        while stack:
            cy, cx = stack.pop()
            for nb in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                if nb in pts:
                    pts.discard(nb)
                    stack.append(nb)
                    n += 1
        sizes.append(n)
    return sum(1 for s in sizes if s >= min_px), sizes


def measure(path):
    a = load(path)
    lens = lens_mask(a)
    lens_px = int(lens.sum())
    if lens_px == 0:
        return 0, 0, 0, -1.0
    star = star_mask(a, lens)
    # ⚠ COUNT ON lens|star, DILATED ONCE. The cyan star sits INSIDE its lens
    # and cuts it into two or three pieces, so a bare lens count reports three
    # and four eyes on a creature that has two.
    eyes, _ = blobs(dilate(lens | star, 1), kMinBlobPx)
    ly, lx = np.nonzero(lens)
    body = body_mask(a)
    rows = body[max(0, int(ly.min()) - 4):int(ly.max()) + 5, :]
    u = -1.0
    if rows.sum() > 60:
        bx = np.nonzero(rows)[1]
        x0, x1 = int(bx.min()), int(bx.max())
        if x1 > x0:
            u = 1000.0 * (lx.mean() - x0) / (x1 - x0)
    return lens_px, int(star.sum()), eyes, u


def scan(d):
    files = sorted(glob.glob(os.path.join(d, "*.rgb")))
    if not files:
        raise SystemExit("eyesweep: no .rgb frames under " + d)
    us, lp, sp, ey = [], [], [], []
    for f in files:
        a, b, c, u = measure(f)
        lp.append(a)
        sp.append(b)
        ey.append(c)
        if u >= 0:
            us.append(u)
    return {
        "frames": len(files),
        "sweep": (max(us) - min(us)) if us else 0.0,
        "u_min": min(us) if us else 0.0,
        "u_max": max(us) if us else 0.0,
        "two_eye_pct": 100.0 * sum(1 for e in ey if e >= 2) / len(ey),
        "lens_px_med": float(np.median(lp)),
        "star_px_med": float(np.median(sp)),
    }


def show(name, s):
    print("%-22s frames %4d  sweep %6.1f  u %6.1f..%6.1f  2-eye %5.1f%%  "
          "lens px med %6.1f  star px med %5.1f"
          % (name, s["frames"], s["sweep"], s["u_min"], s["u_max"],
             s["two_eye_pct"], s["lens_px_med"], s["star_px_med"]))


# ---- the gate ------------------------------------------------------------
# Floors authored AFTER looking at the plates, off the margin the legs leave.
# Never fitted to the answer they were about to bless.

#: per-mille of body width. Live measures 200+ on the fixed-camera clips.
kSweepFloorPm = 120
#: live measures 90-100 on the fixed-camera clips; the +867 leg measures well
#: below that, so the floor sits between the two rather than under both.
kTwoEyeFloorPct = 85.0


def gate(live, pin0, pin867):
    if not pin0 or not pin867:
        print("eyesweep: REFUSED -- both known-negative legs are required.")
        print("  A presence metric that has not been run where the answer is NO")
        print("  is not evidence (10-GATE-CHECKLIST item 40).")
        return 2
    L, Z, P = scan(live), scan(pin0), scan(pin867)
    show("LIVE", L)
    show("KNOWN-NEG pin 0", Z)
    show("KNOWN-NEG pin +867", P)
    ok = True
    if Z["sweep"] >= L["sweep"]:
        print("FAIL calibration: pinning travel OFF did not reduce the sweep.")
        print("     This tool is measuring the body, not the eyes.")
        ok = False
    else:
        print("  calib ok: pin 0 sweeps %.1f against live %.1f" % (Z["sweep"], L["sweep"]))
    if P["lens_px_med"] >= L["lens_px_med"] or P["two_eye_pct"] >= L["two_eye_pct"]:
        print("FAIL calibration: the pass-14 peak reads as well as the fix does.")
        ok = False
    else:
        print("  calib ok: pin +867 shows %.0f lens px and two eyes in %.1f%% of frames,"
              % (P["lens_px_med"], P["two_eye_pct"]))
        print("            against live %.0f and %.1f%%" % (L["lens_px_med"], L["two_eye_pct"]))
    if L["sweep"] < kSweepFloorPm:
        print("FAIL sweep %.1f < floor %d (per-mille of body width)" % (L["sweep"], kSweepFloorPm))
        ok = False
    if L["two_eye_pct"] < kTwoEyeFloorPct:
        print("FAIL two-eye share %.1f%% < floor %.1f%%" % (L["two_eye_pct"], kTwoEyeFloorPct))
        ok = False
    print("eyesweep: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd = argv[1]
    if cmd == "scan":
        show(os.path.basename(argv[2].rstrip("/\\")), scan(argv[2]))
        return 0
    if cmd == "csv":
        print("frame,lens_px,star_px,eyes,u")
        for i, f in enumerate(sorted(glob.glob(os.path.join(argv[2], "*.rgb")))):
            a, b, c, u = measure(f)
            print("%d,%d,%d,%d,%.1f" % (i, a, b, c, u))
        return 0
    if cmd == "gate":
        return gate(argv[2], argv[3] if len(argv) > 3 else None,
                    argv[4] if len(argv) > 4 else None)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
