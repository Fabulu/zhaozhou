"""THE HOLD METER: per-frame motion, so a HOLD is a trough you can point at.

WHY THIS EXISTS
---------------
Pass 13's by-eye review rejected `taunt3` with a finding no gate in the tree
could express:

    "Frame-to-frame motion across all 368 frames has a median of 2.41 and a
     range of about 1.0-5.5; nothing ever approaches zero. Comedy is
     anticipation -> snap -> HOLD. With no hold there is no beat."

07-MOTION-STYLE 8a says the same thing from the other side: a beat is a fast
attack into a HELD extreme, and the tell of the failure is a clip that reads as
one long move. **A metric that reports "slower" will pass the same failure a
third time** -- the question is whether the motion goes NEAR ZERO, not whether
it goes down.

WHAT IT MEASURES
----------------
The mean absolute difference between consecutive frames, in 0..255 units, over
a region. That is all. It is deliberately not a segmentation: four masks on this
creature have been confidently wrong (see `rgbframe.py`'s docstring and gate
checklist item 40), and this question does not need one -- a clip whose creature
stops moving has a lower frame-to-frame difference than one whose creature does
not, whatever else is in the frame.

WARNING -- WHAT IT CANNOT SEE. The mana fold runs on every shipped clip and
never holds, so it puts a FLOOR under every number here. That floor is why the
tool reports `floor` (the 5th percentile) and `floor/median` rather than an
absolute. **Read the ratio, not the value.** If the floor is close to the
median, the clip has no still moment; a real hold drives a wide separation.

CALIBRATION -- THE POINT OF THE WHOLE FILE (gate checklist 40)
--------------------------------------------------------------
A presence/motion metric must be run on a case whose answer is already known,
and the KNOWN-NEGATIVE is the test. Two are committed here as `calibrate`:

  KNOWN POSITIVE  `trick` -- the handstand, frames 172-286. The pass-13 review
                  found this hold BY EYE, independently of any instrument, and
                  called it "the only laugh in 28 clips ... a fast arrival into
                  a long-held, silhouette-distinct extreme".
  KNOWN NEGATIVE  `taunt3` (the pass-13 build) -- the same review, the same
                  night, looked for a hold and found none anywhere in 368
                  frames.

If the meter cannot separate those two it is not used. Run:

    python holdmeter.py calibrate <trick_dir> <taunt3_before_dir>

Usage:
    python holdmeter.py <subject_dir> [--csv out.csv] [--plot out.png]
                        [--box X,Y,W,H] [--label NAME]
    python holdmeter.py calibrate <known_hold_dir> <known_nohold_dir>
    python holdmeter.py selftest

Imports `rgbframe`. Never write another frame reader.
"""
import glob
import os
import sys

import numpy as np

from rgbframe import load

# --- THE THRESHOLD, AND THE CALIBRATION THAT CHOSE IT (2026-09-09) ---------
#
# The FIRST rule written here -- "held" = under half the clip's own MEDIAN --
# FAILED its own calibration, and that failure is the useful part of this file.
# It reported ZERO holds in BOTH clips:
#
#     trick  (known positive)  median 1.686  floor 0.800  -> 0 runs
#     taunt3 (known negative)  median 2.160  floor 0.966  -> 0 runs
#
# It failed because a median-relative rule asks "is this SLOWER THAN TYPICAL",
# and the docstring above says in as many words that the wrong question is the
# one about slower. trick's handstand sits at 0.86-1.15 against a 0.843
# threshold -- a real, eye-visible, 120-frame hold missed by four hundredths.
#
# THE RIGHT QUESTION IS "IS THIS NEAR THE CLIP'S OWN ZERO". So the threshold is
# a multiple of the FLOOR (p5), not a fraction of the median. Nothing on this
# creature ever reaches literal zero -- the bob and the breath never stop -- so
# the clip's own quietest 5% IS its zero, and a hold is a long run that stays
# down there while the clip elsewhere has peaks 5-8x higher.
#
# ⚠ THERE IS A SECOND FLOOR AND IT CANNOT BE ABLATED: THE MOVING LIGHT RIG.
# `subject_u02_clip` sets `s.creature_moving_light = true` on every clip in the
# bank, so the lamp sweeps across the creature whether or not the creature is
# doing anything. A perfectly frozen pose still changes on screen. This was
# confirmed by looking, not inferred: taunt3 f312/f326/f340 at 4x, twenty-eight
# frames apart inside the punchline hold, have an IDENTICAL outline and a
# highlight band that walks across the antenna
# (`pass14-plates-perf/R4-hold2-lamp-not-pose-4x.png`).
#
# So a real hold does not read as zero here, it reads as the lamp alone, and
# the lamp's own rate varies over a clip -- which is why taunt3's punchline hold
# measures ~0.33 while its shrug hold measures ~0.33 but occupies a different
# fraction of its window. **Do not chase a hold that the picture shows is
# already still.** Check the outline on a 2x-or-better crop before believing a
# number that says a parked pose is moving.
#
# ⚠ AND THE MANA FOLD MUST BE ABLATED (`MANA_ABLATE=1`) FOR THE MEASUREMENT.
# The docstring's warning is not a caveat to read past: the fold runs on every
# shipped clip and never holds, so it adds a moving floor that is a large
# fraction of the signal. Removing it AT THE SOURCE -- an ablation, not a mask
# (four masks on this creature have been confidently wrong) -- is what turns a
# knife-edge separation into a wide one:
#
#     multiple      1.2   1.3   1.4   1.5   1.6   1.7   1.8   2.0
#     mana ON   trick   -     -     H     H     H     H     H     H
#               taunt3  -     -     -     -     X     X     X     X     <- narrow
#     mana OFF  trick   H     H     H     H     H     H     H     H
#               taunt3  -     -     -     -     -     -     -     X     <- wide
#         (H = hold found in the known positive; X = spurious hold in the
#          known negative; - = nothing.  Full sweep in PASS-14-FINDINGS-PERF.)
#
# 1.5 is the MIDDLE of the robust band (1.2-1.8), not an edge of it. That is
# the whole defence against having tuned the instrument to the answer: the
# verdict does not change anywhere across a 1.5x-wide range of the constant.
# A hold is only a meaningful question in a clip that also has an ATTACK
# (07-MOTION-STYLE 8a: fast arrival INTO a held extreme). peak/floor is that,
# measured. It separates enormously -- both real clips sit at 9.1, while every
# degenerate signal a floor-relative rule would otherwise mis-call sits at 2.2
# or below (a slow half-amplitude oscillation 2.2, a merely-slower stretch 1.4,
# constant motion 1.0). 4.0 is the middle of a 4x-wide gap, not an edge of it.
# ⚠ It does NOT gate out the known negative -- taunt3 is 9.1 and is judged on
# its runs like anything else. A guard that excluded the negative would be
# cheating, which is the failure mode item 40 exists to catch.
HOLD_RANGE_MIN = 4.0   # peak/floor below this = no attack, so no beat to hold
HOLD_FLOOR_MULT = 1.5  # "held" = within this multiple of the clip's own floor
HOLD_MIN_RUN = 16      # ...for at least this many consecutive frames.
                       # 07-MOTION-STYLE: 16 frames is where a beat registers.


def series(frame_dir, box=None):
    """Mean |frame[i] - frame[i-1]| in 0..255, one value per frame pair."""
    paths = sorted(glob.glob(os.path.join(frame_dir, "*.rgb")))
    if len(paths) < 2:
        raise ValueError(
            "%s: %d .rgb frames -- nothing to difference" % (frame_dir, len(paths)))
    out = np.zeros(len(paths) - 1, dtype=np.float64)
    prev = load(paths[0]).astype(np.int16)
    if box:
        x, y, w, h = box
        prev = prev[y:y + h, x:x + w]
    for i, p in enumerate(paths[1:]):
        cur = load(p).astype(np.int16)
        if box:
            x, y, w, h = box
            cur = cur[y:y + h, x:x + w]
        out[i] = np.abs(cur - prev).mean()
        prev = cur
    return paths, out


def summarise(d):
    return {
        "n": int(d.size),
        "median": float(np.median(d)),
        "floor": float(np.percentile(d, 5)),
        "peak": float(d.max()),
        "ratio": float(np.percentile(d, 5) / max(np.median(d), 1e-9)),
        "range": float(d.max() / max(np.percentile(d, 5), 1e-9)),
    }


def troughs(d, mult=HOLD_FLOOR_MULT, min_run=HOLD_MIN_RUN):
    """Runs of >= min_run frames whose motion stays within mult * the FLOOR.

    A HOLD is a RUN, not a low frame, and it is measured against the clip's own
    zero (p5), never against its median -- see the long note on the constants.
    The tool reports what is there and the reader judges whether it is long
    enough, and in the right place, to be a beat.
    """
    flr = float(np.percentile(d, 5))
    thr = flr * mult
    if float(d.max()) / max(flr, 1e-9) < HOLD_RANGE_MIN:
        # No attack anywhere: the clip moves at one rate, so "near its floor"
        # describes most of it and would report a hold that nobody can see.
        return thr, []
    runs, start = [], None
    for i, v in enumerate(d):
        if v < thr and start is None:
            start = i
        elif v >= thr and start is not None:
            if i - start >= min_run:
                runs.append((start, i - 1, float(d[start:i].mean())))
            start = None
    if start is not None and len(d) - start >= min_run:
        runs.append((start, len(d) - 1, float(d[start:].mean())))
    return thr, runs


def report(label, d):
    s = summarise(d)
    thr, runs = troughs(d)
    print("%s: %d frame pairs" % (label, s["n"]))
    print("    median %.3f   floor(p5) %.3f   peak %.3f   floor/median %.3f"
          % (s["median"], s["floor"], s["peak"], s["ratio"]))
    print("    peak/floor %.2f%s"
          % (s["range"], "" if s["range"] >= HOLD_RANGE_MIN
             else "   <- NO ATTACK: holds not reported (see HOLD_RANGE_MIN)"))
    print("    holds (runs >= %d frames under %.3f = %.2f x floor): %d"
          % (HOLD_MIN_RUN, thr, HOLD_FLOOR_MULT, len(runs)))
    for a, b, m in runs:
        print("        frames %4d-%4d  (%3d frames)  mean %.3f"
              % (a, b, b - a + 1, m))
    return s, runs


def plot(path, label, d, runs):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(12, 3.2), dpi=110)
    ax.plot(np.arange(d.size), d, lw=0.8, color="#1f4f8f")
    med = float(np.median(d))
    ax.axhline(med, color="#888888", lw=0.7, ls="--", label="median %.2f" % med)
    flr = float(np.percentile(d, 5))
    ax.axhline(flr * HOLD_FLOOR_MULT, color="#c04040", lw=0.7, ls=":",
               label="hold line %.2f (= %.2f x floor)"
                     % (flr * HOLD_FLOOR_MULT, HOLD_FLOOR_MULT))
    for a, b, _ in runs:
        ax.axvspan(a, b, color="#ffd24d", alpha=0.45)
    ax.set_xlabel("frame")
    ax.set_ylabel("mean |delta| (0..255)")
    ax.set_title("%s - per-frame motion; shaded = held runs" % label)
    ax.legend(fontsize=7)
    ax.margins(x=0.005)
    fig.tight_layout()
    fig.savefig(path)
    print("wrote %s" % path)


def calibrate(hold_dir, nohold_dir):
    print("CALIBRATION (gate checklist 40: the KNOWN-NEGATIVE is the test)\n")
    _, dh = series(hold_dir)
    _, rh = report("KNOWN POSITIVE  " + os.path.basename(os.path.normpath(hold_dir)), dh)
    print()
    _, dn = series(nohold_dir)
    _, rn = report("KNOWN NEGATIVE  " + os.path.basename(os.path.normpath(nohold_dir)), dn)
    print()
    ok = bool(rh) and not rn
    print("VERDICT: the meter %s separate the two." % ("DOES" if ok else "DOES NOT"))
    if not ok:
        print("  -> do not use it. Look at the contact sheet instead "
              "(the art law is not only about taste).")
    return 0 if ok else 1


def selftest():
    rng = np.random.default_rng(7)
    a = rng.integers(0, 255, (240, 384, 3), dtype=np.uint8).astype(np.int16)
    assert np.abs(a - a).mean() == 0.0, "identical frames must difference to zero"
    moved = np.abs(np.roll(a, 3, axis=1) - a).mean()
    assert moved > 40, "a shifted frame must differ a lot, got %r" % moved
    # A synthetic clip shaped like the one we are authoring: mostly moving,
    # with two genuine holds in it.
    d = np.concatenate([np.full(100, 3.0), np.full(40, 0.2),
                        np.full(100, 3.0), np.full(40, 0.2),
                        np.full(60, 3.0)])
    _, runs = troughs(d)
    assert len(runs) == 2, "expected two held runs, got %r" % (runs,)
    assert summarise(d)["range"] >= HOLD_RANGE_MIN
    # ...and the FAILABLE LEGS. A detector that never returns "no hold" is not
    # a detector (gate checklist 40), so there are three of them, and each one
    # fails for a DIFFERENT reason -- two through the attack guard, and one
    # through the run length, so neither mechanism can be doing all the work.
    #
    # (1) Continuous motion at one rate. Under a floor-relative rule this is
    #     the dangerous case -- the floor IS the median -- and the attack guard
    #     is what stops it.
    d2 = np.full(368, 2.4)
    _, runs2 = troughs(d2)
    assert not runs2, "constant motion must report no hold, got %r" % (runs2,)
    # (2) "Merely slower" must ALSO not register, which is the whole finding.
    d3 = np.full(368, 2.4)
    d3[100:160] = 1.7
    _, runs3 = troughs(d3)
    assert not runs3, "'slower' must not read as a hold, got %r" % (runs3,)
    # (3) THE LEG THE GUARD CANNOT ANSWER: a clip with a huge attack (so it is
    #     judged on its runs, not gated) whose quiet moments are all BRIEF. A
    #     hold is a RUN. Five-frame dips are not a beat and must not read as one.
    d4 = np.full(368, 2.0)
    d4[10::40] = 9.0
    for a in range(25, 368, 40):
        d4[a:a + 5] = 0.6
    assert summarise(d4)["range"] >= HOLD_RANGE_MIN, "leg 3 must not be gated"
    _, runs4 = troughs(d4)
    assert not runs4, "brief dips must not read as a hold, got %r" % (runs4,)
    print("holdmeter selftest OK (zero on identical frames; catches a held run; "
          "reports NO hold on constant motion, on merely-slower motion, and on "
          "an ungated clip whose quiet moments are all too brief)")
    return 0


def main(argv):
    if len(argv) >= 2 and argv[1] == "selftest":
        return selftest()
    if len(argv) >= 4 and argv[1] == "calibrate":
        return calibrate(argv[2], argv[3])
    if len(argv) < 2:
        print(__doc__)
        return 2
    frame_dir = argv[1]
    box = None
    label = os.path.basename(os.path.normpath(frame_dir))
    out_csv = out_plot = None
    i = 2
    while i < len(argv):
        if argv[i] == "--box":
            box = tuple(int(v) for v in argv[i + 1].split(",")); i += 2
        elif argv[i] == "--csv":
            out_csv = argv[i + 1]; i += 2
        elif argv[i] == "--plot":
            out_plot = argv[i + 1]; i += 2
        elif argv[i] == "--label":
            label = argv[i + 1]; i += 2
        else:
            print("unknown argument %s" % argv[i]); return 2
    _, d = series(frame_dir, box)
    _, runs = report(label, d)
    if out_csv:
        with open(out_csv, "w") as fh:
            fh.write("frame,delta\n")
            for i, v in enumerate(d):
                fh.write("%d,%.5f\n" % (i, v))
        print("wrote %s" % out_csv)
    if out_plot:
        plot(out_plot, label, d, runs)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
