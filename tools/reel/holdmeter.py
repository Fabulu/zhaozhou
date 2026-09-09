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

HOLD_RATIO_MIN = 0.45  # floor/median below this = there is a real trough
HOLD_FRAC = 0.5        # "held" = under half the clip's own median motion
HOLD_MIN_RUN = 12      # ...for at least this many consecutive frames


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
    }


def troughs(d, frac=HOLD_FRAC, min_run=HOLD_MIN_RUN):
    """Runs of >= min_run frames whose motion is under frac * median.

    A HOLD is a RUN, not a low frame. 07-MOTION-STYLE: 16+ frames for a beat to
    register, so the default run length is deliberately a little under that --
    the tool reports what is there and the reader judges whether it is long
    enough to be a beat.
    """
    thr = float(np.median(d)) * frac
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
    print("    holds (runs >= %d frames under %.3f = %.2f x median): %d"
          % (HOLD_MIN_RUN, thr, HOLD_FRAC, len(runs)))
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
    ax.axhline(med * HOLD_FRAC, color="#c04040", lw=0.7, ls=":",
               label="%.2f x median" % HOLD_FRAC)
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
    # with two genuine holds in it. The threshold is RELATIVE to the clip's own
    # median, so a fixture that is mostly hold has the hold as its median and
    # reports nothing -- which is correct behaviour and was worth finding here
    # rather than on a render.
    d = np.concatenate([np.full(100, 3.0), np.full(40, 0.2),
                        np.full(100, 3.0), np.full(40, 0.2),
                        np.full(60, 3.0)])
    _, runs = troughs(d)
    assert len(runs) == 2, "expected two held runs, got %r" % (runs,)
    assert summarise(d)["ratio"] < HOLD_RATIO_MIN
    # ...and the FAILABLE LEG: continuous motion must yield NO held run at all.
    # This is the leg that matters -- a detector that never returns "no hold"
    # is not a detector (gate checklist 40).
    d2 = 2.4 + 0.9 * np.sin(np.arange(368) / 9.0)
    _, runs2 = troughs(d2)
    assert not runs2, "a clip that never stops must report no hold, got %r" % (runs2,)
    # ...and "merely slower" must ALSO not register, which is the whole finding:
    # a 30%-slower stretch is not a hold.
    d3 = np.full(368, 2.4)
    d3[100:160] = 1.7
    _, runs3 = troughs(d3)
    assert not runs3, "'slower' must not read as a hold, got %r" % (runs3,)
    print("holdmeter selftest OK (zero on identical frames; catches a held run; "
          "reports NO hold on continuous motion AND on merely-slower motion)")
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
