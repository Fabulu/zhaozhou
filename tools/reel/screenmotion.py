#!/usr/bin/env python3
"""screenmotion.py -- does a travelling clip actually TRAVEL, on screen?

    python tools/reel/screenmotion.py <frame-dir> [<frame-dir> ...]
                                      [--band-y 150,235] [--max-shift 24]
                                      [--csv out.csv] [--json out.json]
    python tools/reel/screenmotion.py selftest

WHY THIS TOOL EXISTS (pass 26, Hasty)
-------------------------------------
Hasty was reported as "moves across screen" and simultaneously "not very
hasty", and both were true. The reel gives a travelling clip TWO independent
lateral authorities:

  * `Clip::root[f*3+0]` -- the creature's WORLD x. Real locomotion.
  * `SceneSubject::cam_bias_x` (lerped to `cam_bias_x_end`) -- a lateral aim
    shift added into the projection's x row as `bias_x * w`. A CAMERA PAN.

A camera pan carries the creature AND the ground across the frame together, so
the picture moves and the creature is standing still. Speed is read from
RELATIVE motion -- the ground streaming past the body -- so a clip can post a
large screen traverse and a zero speed cue. That is exactly the fault this
measures, and no single-authority number can see it: you have to difference the
two.

WHAT IT MEASURES
----------------
Per frame:
  * `ink_cx`, `ink_cy`   creature silhouette centroid, from the cel contour ink
                         mask (`inkmask.INK_TRIPLES` -- NOT a fresh reader and
                         NOT a fresh ink table; four diagnostics on this
                         creature have been confidently wrong and two were bad
                         readers).
  * `ink_px`             mask population, so an empty/vacuous frame is loud
                         rather than silently contributing a centroid of nan.
  * `bg_dx`              BACKGROUND scroll, px between this frame and the last,
                         by integer cross-correlation of a horizontal terrain
                         band with the creature's own ink columns EXCLUDED.
                         Integer-shift argmin of mean |difference|.

Derived, and these are the numbers that answer the question:
  * `creature_dx`        d(ink_cx)/frame -- how fast the body crosses the frame.
  * `relative_dx`        creature_dx - bg_dx -- how fast the body moves AGAINST
                         THE GROUND. **This is the speed cue.** ~0 means the
                         creature is being panned past, not travelling.

ON THE COMPARISON SIDE ONLY
---------------------------
CLAUDE.md: measurement belongs on the comparison side, never the generation
side. This tool does not choose a traverse rate. It answers "is there a ground-
relative speed cue, and how big is it" and "did the number move between A and
B". The shipped values are still chosen by rendering and looking.

BAND DEFAULT, and why it is a flag. `--band-y 150,235` is the lower terrain in
a 384x240 frame: below the creature's body in the travelling clips' framing and
above the very last rows, which the vignette darkens flat. A band with no
texture cannot be correlated, and the tool says so (`bg_flat`) rather than
reporting a confident 0.

FRAME INDEXING matches wrapseam.py: the reel advances THEN renders, so rendered
frame i shows key (i+1)/2 at sub (i+1)&1, and for n = 2K frames the wrap blend
is frame n-2. Rows are reported in rendered-frame order; `--skip-wrap` drops the
last two so a loop wrap does not dominate a mean.
"""
import argparse
import glob
import json
import os
import struct
import sys

import numpy as np

from rgbframe import load
from inkmask import mask as ink_mask

# A band this featureless cannot be cross-correlated: report it, never a 0.
BG_FLAT_EPS = 0.75

# ---- THE CHROMA MASK ------------------------------------------------------
# The contour-ink mask is the RIGHT default -- it is the creature's own
# authored ink and cannot be confused with terrain. But on a pulled-back
# travelling framing it finds ONE OR TWO PIXELS per frame and is empty on most
# of them, and an empty creature mask does not merely lose the centroid: it
# stops the background correlator excluding the creature's columns, so the
# correlator locks onto the only thing moving in the band -- the creature --
# and reports its motion as the GROUND's. That was observed reporting a
# confident bg_dx of +1.096 px/frame on a render with NO camera pan at all.
#
# So there is a second mask. Manafold is vivid magenta against a desaturated
# brown desert and a dusty rose sky, and a saturation-plus-value rule separates
# them cleanly. It is NOT the default, because a colour rule is exactly the
# kind of presence metric CLAUDE.md warns about -- one scored a known-ABSENT
# frame at 76% of a known-present one.
#
# The discipline that makes it usable: `--mask chroma` is validated on a frame
# where the answer is NO before it is quoted, both by the selftest's E-leg and
# by `python screenmotion.py absent <frame.rgb> [...]`, which prints the
# population on frames the caller asserts are empty.
# ⚠ 60 WAS THE FIRST VALUE AND IT WAS WORTHLESS. The `absent` check scored a
# frame with NO CREATURE IN IT at 29,719 pixels -- 32% of the frame -- because
# the dusty-rose SKY is itself saturated. It is the exact failure CLAUDE.md
# records ("a colour rule scored a known-ABSENT frame at 76% of a known-present
# one"), reproduced within a minute of the check existing, and it is the whole
# argument for running a presence rule on a negative before quoting it.
#
# Measured on this staging: the sky and terrain top out at saturation 82, the
# creature reaches 222. 100 sits above the background's ceiling with margin and
# still finds ~2,200 px of creature.
CHROMA_SAT_MIN = 100     # max(rgb) - min(rgb)
CHROMA_VAL_MIN = 70      # max(rgb): rejects the dark vignette rows


def chroma_mask(img):
    a = img.astype(np.int16)
    mx = a.max(axis=2)
    mn = a.min(axis=2)
    return ((mx - mn) >= CHROMA_SAT_MIN) & (mx >= CHROMA_VAL_MIN)


MASKS = {"ink": ink_mask, "chroma": chroma_mask}


def frame_paths(d):
    fs = sorted(glob.glob(os.path.join(d, "*.rgb")))
    if not fs:
        raise SystemExit("screenmotion: no .rgb frames in %s" % d)
    return fs


def _band_shift(prev, cur, prev_keep, cur_keep, max_shift):
    """Integer lateral shift (px) that best aligns `prev` onto `cur`.

    Positive means the CONTENT MOVED RIGHT. Columns not in *_keep (the
    creature's own ink, dilated) are excluded from the score at both ends, so
    the creature cannot drag the background estimate along with it.
    """
    best, best_cost = 0, None
    w = prev.shape[1]
    flat = float(np.mean(np.abs(np.diff(cur.astype(np.int16), axis=1))))
    for s in range(-max_shift, max_shift + 1):
        # cur[x] compared against prev[x - s]
        if s >= 0:
            a, b = cur[:, s:], prev[:, : w - s]
            ka, kb = cur_keep[s:], prev_keep[: w - s]
        else:
            a, b = cur[:, : w + s], prev[:, -s:]
            ka, kb = cur_keep[: w + s], prev_keep[-s:]
        keep = ka & kb
        if keep.sum() < 16:
            continue
        cost = float(np.mean(np.abs(a[:, keep].astype(np.int32) - b[:, keep].astype(np.int32))))
        if best_cost is None or cost < best_cost:
            best, best_cost = s, cost
    return best, (best_cost if best_cost is not None else float("nan")), flat


def measure(d, band=(150, 235), max_shift=24, skip_wrap=False, lag=8, mask="ink"):
    """Per-frame rows. `lag` is the frame separation the BACKGROUND shift is
    measured over, and it is not cosmetic.

    ⚠ THE SUB-PIXEL TRAP, found on the real Hasty clip and the reason this
    parameter exists. `_band_shift` is an INTEGER argmin. Hasty's camera pan is
    ~0.68 px/frame, so a lag-1 comparison correctly rounds to 0 on every single
    frame while the pan accumulates 163 px across the clip — the tool reported
    `bg_dx` exactly 0.000, 239 times, for a picture that visibly slides. A
    detector whose quantum is larger than the effect cannot fire, and it does
    not fail loudly: it returns a confident zero.

    So the shift is measured between frame i and frame i-lag and divided by lag,
    which makes any rate down to 1/lag px/frame visible. `bg_dx` stays in
    px/frame either way, so the column means the same thing at every lag.
    """
    fs = frame_paths(d)
    if skip_wrap and len(fs) > 2:
        fs = fs[:-2]
    lag = max(1, int(lag))
    mask_fn = MASKS[mask]
    y0, y1 = band
    rows = []
    bands, keeps = [], []
    for i, p in enumerate(fs):
        img = load(p)
        m = mask_fn(img)
        n = int(m.sum())
        if n:
            ys, xs = np.nonzero(m)
            cx, cy = float(xs.mean()), float(ys.mean())
            x0, x1 = int(xs.min()), int(xs.max())
        else:
            cx = cy = float("nan")
            x0, x1 = -1, -1
        # the creature's own columns, dilated by 6 px, are not background
        cols = m.any(axis=0)
        if cols.any():
            k = np.ones(13, bool)
            cols = np.convolve(cols.astype(np.int32), k.astype(np.int32), mode="same") > 0
        keep = ~cols
        bands.append(img[y0:y1, :, :].astype(np.int16).sum(axis=2))
        keeps.append(keep)
        j = i - lag
        if j < 0:
            dx, cost, flat = float("nan"), float("nan"), float("nan")
        else:
            sh, cost, flat = _band_shift(bands[j], bands[i], keeps[j], keeps[i],
                                         max_shift)
            dx = sh / float(lag)
        rows.append(
            dict(frame=i, ink_px=n, ink_cx=cx, ink_cy=cy, ink_x0=x0, ink_x1=x1,
                 bg_dx=dx, bg_cost=cost, bg_flat=flat, bg_lag=lag,
                 width=int(img.shape[1]))
        )
    # derived columns. creature_dx is differenced over the SAME lag as bg_dx,
    # so `relative_dx` subtracts two rates measured the same way -- the house
    # rule about the two sides of a comparison being clocked alike.
    for i, r in enumerate(rows):
        j = i - lag
        if j < 0:
            r["creature_dx"] = float("nan")
        else:
            r["creature_dx"] = (rows[i]["ink_cx"] - rows[j]["ink_cx"]) / float(lag)
        r["relative_dx"] = r["creature_dx"] - r["bg_dx"]
    return rows


def summarise(name, rows):
    cx = np.array([r["ink_cx"] for r in rows], float)
    px = np.array([r["ink_px"] for r in rows], int)
    lag = int(rows[0].get("bg_lag", 1)) if rows else 1
    cdx = np.array([r["creature_dx"] for r in rows], float)[lag:]
    bdx = np.array([r["bg_dx"] for r in rows], float)[lag:]
    rdx = np.array([r["relative_dx"] for r in rows], float)[lag:]
    flat = np.array([r["bg_flat"] for r in rows], float)[lag:]
    vac = int((px == 0).sum())

    # The vacuous leg has NO finite centroid anywhere, and nanmin/nanmean of an
    # all-nan slice is a RuntimeWarning plus a nan. Report "no measurement" in
    # the field instead: a broken render must not read as a number.
    def _nmin(v):
        return float(np.nanmin(v)) if np.isfinite(v).any() else float("nan")

    def _nmax(v):
        return float(np.nanmax(v)) if np.isfinite(v).any() else float("nan")

    def _nmean(v):
        return float(np.nanmean(v)) if np.isfinite(v).any() else float("nan")

    s = dict(
        subject=name,
        frames=len(rows),
        bg_lag=lag,
        vacuous_frames=vac,
        ink_cx_min=_nmin(cx),
        ink_cx_max=_nmax(cx),
        ink_cx_span=_nmax(cx) - _nmin(cx),
        creature_dx_mean=_nmean(cdx),
        creature_dx_absmean=_nmean(np.abs(cdx)),
        bg_dx_mean=_nmean(bdx),
        bg_dx_absmean=_nmean(np.abs(bdx)),
        relative_dx_mean=_nmean(rdx),
        relative_dx_absmean=_nmean(np.abs(rdx)),
        bg_band_flatness=_nmean(flat),
        bg_band_correlatable=bool(_nmean(flat) > BG_FLAT_EPS),
    )
    # EDGE CLEARANCE -- "is the subject in frame", which is the question a
    # travelling clip keeps raising and the one CLAUDE.md says a contact sheet
    # only ever answers as a CANDIDATE. Reported as the worst margin to either
    # side over the clip, plus the count of frames actually touching an edge,
    # so a single bad frame cannot be averaged away.
    w = int(rows[0].get("width", 384)) if rows else 384
    x0 = np.array([r["ink_x0"] for r in rows], float)
    x1 = np.array([r["ink_x1"] for r in rows], float)
    seen = x0 >= 0
    if seen.any():
        s["edge_margin_left"] = float(np.min(x0[seen]))
        s["edge_margin_right"] = float(w - 1 - np.max(x1[seen]))
        s["edge_touch_frames"] = int(((x0 <= 0) | (x1 >= w - 1))[seen].sum())
    else:
        s["edge_margin_left"] = float("nan")
        s["edge_margin_right"] = float("nan")
        s["edge_touch_frames"] = 0
    return s


def _print(s):
    print("-- %s" % s["subject"])
    if s["vacuous_frames"]:
        print("   !! VACUOUS ink mask on %d frame(s) -- centroids there are nan" % s["vacuous_frames"])
    if not s["bg_band_correlatable"]:
        print("   !! background band flatness %.3f <= %.2f: bg_dx is NOT trustworthy"
              % (s["bg_band_flatness"], BG_FLAT_EPS))
    print("   frames            %d   (rates differenced over lag %d)" % (s["frames"], s["bg_lag"]))
    print("   ink_cx            %.1f .. %.1f  (span %.1f px)"
          % (s["ink_cx_min"], s["ink_cx_max"], s["ink_cx_span"]))
    print("   creature dx/frame mean %+.3f px   |mean| %.3f px" % (s["creature_dx_mean"], s["creature_dx_absmean"]))
    print("   background dx/frm mean %+.3f px   |mean| %.3f px" % (s["bg_dx_mean"], s["bg_dx_absmean"]))
    print("   RELATIVE  dx/frame mean %+.3f px  |mean| %.3f px   <-- the speed cue"
          % (s["relative_dx_mean"], s["relative_dx_absmean"]))
    print("   edge margin       L %.0f px  R %.0f px   touching %d frame(s)"
          % (s["edge_margin_left"], s["edge_margin_right"], s["edge_touch_frames"]))


def _write_rgb(path, img):
    """Write a reel-format frame: the same 8-byte <II header rgbframe.load asserts."""
    with open(path, "wb") as f:
        f.write(struct.pack("<II", img.shape[1], img.shape[0]))
        f.write(img.tobytes())


def selftest():
    """Prove the instrument can fail, and that it separates the two authorities.

    Synthetic 240x384 cases built here, not rendered. `cre` and `gnd` are both
    SCREEN-SPACE px/frame, positive = rightward, so the fixture is stated in the
    same units the tool reports:

      A  PAN ONLY      -- creature and textured ground move right together.
                          creature_dx large, relative_dx ~0. THIS IS THE HASTY
                          FAULT and the leg the tool exists for.
      B  TRAVEL ONLY   -- ground held, creature moves right.
                          creature_dx == relative_dx.
      C  FLAT GROUND   -- untextured band: bg_band_correlatable must be False.
      D  VACUOUS       -- no ink at all: reported, never averaged into a mean.
      E  BOTH          -- creature right, ground left. relative_dx must be the
                          DIFFERENCE and not either operand: the leg that would
                          catch a sign error in `_band_shift`.

    ⚠ The first draft of case A built the ground as `gx = 400 + i*step`, which
    slides the SAMPLING WINDOW right and therefore moves the CONTENT LEFT. So
    "pan only" was really pan-and-travel, and the leg failed against a correct
    tool. The house lesson in miniature: a fixture is exactly as capable of
    being wrong as the instrument, and a failing leg is a question, not a
    verdict.
    """
    import tempfile
    rng = np.random.default_rng(7)
    ok = True

    def build(dirpath, n, cre_step, gnd_step, textured):
        """cre_step/gnd_step are SCREEN px/frame, positive = rightward."""
        ground = (rng.integers(40, 200, size=(240, 1600, 3))).astype(np.uint8) if textured \
            else np.full((240, 1600, 3), 90, np.uint8)
        for i in range(n):
            img = np.zeros((240, 384, 3), np.uint8)
            # content moves RIGHT <=> the sampling window moves LEFT
            gx = 600 - i * gnd_step
            img[:, :, :] = ground[:, gx:gx + 384, :]
            cx = 100 + i * cre_step
            img[90:130, cx:cx + 30, :] = (25, 24, 25)  # a quantised-ink blob
            _write_rgb(os.path.join(dirpath, "%04d.rgb" % i), img)

    for label, cre, gnd, tex, want in (
        ("A pan-only", 3, 3, True, "rel0"),
        ("B travel-only", 3, 0, True, "releq"),
        ("C flat-ground", 3, 0, False, "flat"),
        ("E both", 3, -3, True, "reldiff"),
    ):
        with tempfile.TemporaryDirectory() as td:
            build(td, 12, cre, gnd, tex)
            # lag 1: A/B/C/E are INTEGER-rate fixtures, authored per frame.
            # (They broke the moment the default became 8 -- 12 frames at
            # 3 px/frame is 24 px over a lag of 8, past max_shift. A fixture
            # must state the lag it was built for.)
            s = summarise(label, measure(td, band=(150, 235), max_shift=12, lag=1))
            if want == "rel0":
                good = abs(s["relative_dx_mean"]) < 0.5 and s["creature_dx_absmean"] > 2.0
            elif want == "releq":
                good = abs(s["relative_dx_mean"] - s["creature_dx_mean"]) < 0.5 \
                    and s["relative_dx_absmean"] > 2.0
            elif want == "reldiff":
                good = abs(s["bg_dx_mean"] + 3.0) < 0.5 and abs(s["relative_dx_mean"] - 6.0) < 0.5
            else:
                good = not s["bg_band_correlatable"]
            print("%-16s %-6s %s  (creature %+.2f  bg %+.2f  rel %+.2f  flat %.2f)"
                  % (label, "PASS" if good else "FAIL", want,
                     s["creature_dx_mean"], s["bg_dx_mean"], s["relative_dx_mean"],
                     s["bg_band_flatness"]))
            ok &= good

    # and the vacuous leg: an all-background clip must be reported, not averaged
    with tempfile.TemporaryDirectory() as td:
        for i in range(4):
            _write_rgb(os.path.join(td, "%04d.rgb" % i), np.full((240, 384, 3), 90, np.uint8))
        s = summarise("D vacuous", measure(td, band=(150, 235), max_shift=4, lag=1))
        good = s["vacuous_frames"] == 4
        print("%-16s %-6s vacuous  (vacuous_frames %d)" % ("D vacuous", "PASS" if good else "FAIL",
                                                           s["vacuous_frames"]))
        ok &= good
    # F: THE SUB-PIXEL LEG, and the positive control for `lag`.
    #
    # The ground is a SMOOTH low-frequency profile panned a TRUE 0.5 px/frame by
    # linear resampling -- not 1 px every other frame, which would be an integer
    # fixture wearing a sub-pixel label and would have let lag 1 pass. This is
    # the regime the real Hasty clip lives in (~0.68 px/frame over a smooth
    # gradient), where lag 1 reported a confident 0.000 on 239 consecutive
    # frames.
    #
    # At lag 8 the shift is exactly 4 px, it resolves, and 4/8 = 0.5 comes back.
    # THAT is what this leg asserts.
    #
    # ⚠ It deliberately does NOT assert that lag 1 returns 0. "The broken lag is
    # still broken" is a test that asserts the bug: it passes only while the
    # defect exists, and after any improvement to `_band_shift` (sub-pixel
    # interpolation, say) it would fail for the right reason. CLAUDE.md, in as
    # many words. The lag-1 figure is PRINTED beside it as information -- here
    # it comes out a noisy +0.49 rather than the clean 0.000 the real clip gave,
    # because a tie broken by uint8 quantisation is arbitrary, which is itself
    # the argument for not depending on it.
    with tempfile.TemporaryDirectory() as td:
        x = np.arange(1600, dtype=np.float64)
        prof = 128.0 + 60.0 * np.sin(2 * np.pi * x / 90.0) + 40.0 * np.sin(2 * np.pi * x / 37.0)
        for i in range(40):
            off = 600.0 - i * 0.5          # content moves RIGHT by 0.5 px/frame
            xs = off + np.arange(384, dtype=np.float64)
            lo = np.floor(xs).astype(int)
            fr = xs - lo
            row = prof[lo] * (1.0 - fr) + prof[lo + 1] * fr
            img = np.zeros((240, 384, 3), np.uint8)
            img[:, :, :] = np.clip(row, 0, 255).astype(np.uint8)[None, :, None]
            img[90:130, 100:130, :] = (25, 24, 25)
            _write_rgb(os.path.join(td, "%04d.rgb" % i), img)
        s1 = summarise("F lag1", measure(td, band=(150, 235), max_shift=12, lag=1))
        s8 = summarise("F lag8", measure(td, band=(150, 235), max_shift=12, lag=8))
        good = abs(s8["bg_dx_mean"] - 0.5) < 0.1      # lag 8 recovers the rate
        print("%-16s %-6s subpixel (lag8 bg %+.3f want +0.500;  lag1 bg %+.3f, fyi)"
              % ("F subpixel", "PASS" if good else "FAIL", s8["bg_dx_mean"], s1["bg_dx_mean"]))
        ok &= good

    print("selftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main(argv):
    if len(argv) > 1 and argv[1] == "selftest":
        return selftest()
    if len(argv) > 2 and argv[1] == "absent":
        # THE KNOWN-NEGATIVE CHECK. The caller names frames on which the
        # creature is NOT present; every mask must come back empty or nearly so.
        # A presence rule that has only ever been run on frames containing the
        # subject has not been tested at all -- it is the check the ink mask's
        # own predecessor skipped, and it scored an empty frame at 76%.
        rc = 0
        for p in argv[2:]:
            img = load(p)
            for name in sorted(MASKS):
                n = int(MASKS[name](img).sum())
                bad = n > 40
                if bad:
                    rc = 1
                print("%-40s %-7s %6d px %s" % (os.path.basename(p), name, n,
                                                "<-- NOT EMPTY" if bad else "ok"))
        print("absent-check: %s" % ("FAIL" if rc else "PASS"))
        return rc
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--band-y", default="150,235")
    ap.add_argument("--max-shift", type=int, default=24)
    ap.add_argument("--skip-wrap", action="store_true")
    ap.add_argument("--mask", choices=sorted(MASKS), default="ink",
                    help="creature mask. 'ink' is the authored contour ink and is "
                         "trustworthy but sparse on pulled-back framings; 'chroma' "
                         "is a saturation rule and MUST be validated with the "
                         "'absent' subcommand before its numbers are quoted.")
    ap.add_argument("--lag", type=int, default=8,
                    help="frames the background/creature shift is differenced over "
                         "(default 8). Lag 1 is BLIND to any rate below 1 px/frame.")
    ap.add_argument("--csv")
    ap.add_argument("--json")
    a = ap.parse_args(argv[1:])
    y0, y1 = (int(v) for v in a.band_y.split(","))
    out, allrows = [], {}
    for d in a.dirs:
        rows = measure(d, band=(y0, y1), max_shift=a.max_shift, skip_wrap=a.skip_wrap,
                       lag=a.lag, mask=a.mask)
        name = os.path.basename(os.path.normpath(d))
        s = summarise(name, rows)
        _print(s)
        out.append(s)
        allrows[name] = rows
    if a.csv:
        with open(a.csv, "w") as f:
            f.write("subject,frame,ink_px,ink_cx,ink_cy,bg_dx,creature_dx,relative_dx,bg_lag\n")
            for name, rows in allrows.items():
                for r in rows:
                    f.write("%s,%d,%d,%.3f,%.3f,%.4f,%.4f,%.4f,%d\n"
                            % (name, r["frame"], r["ink_px"], r["ink_cx"], r["ink_cy"],
                               r["bg_dx"], r["creature_dx"], r["relative_dx"], r["bg_lag"]))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(dict(summaries=out, rows=allrows), f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
