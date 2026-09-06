"""Manafold pass 7 -- the mana-saturation comparison probe.

COMPARISON SIDE ONLY (CLAUDE.md: "measurement belongs on the comparison side,
checking what was authored against the reference -- never on the generation
side"). This tool does not choose any constant; it reports two numbers, run
before and after a change, so the change can be judged against itself.

Two measurements, both over every rendered frame of a clip:

  1. ABSOLUTE, on "bright" pixels (max channel >= --bright, default 140) --
     a proxy for "the mana glow, wherever it sits in the frame", since the
     shipping renders have no ground-truth mana mask. hue-neutral = the
     channel spread (max-min) is <= --neutral (default 18): a pixel with all
     three channels within 18 of each other reads as grey/white regardless of
     which hue it nominally carries.

  2. ABLATION-PAIR DIFF, for the committed manafold-fogprobe-mana /
     manafold-fogprobe-off pair (same base clip, same camera, same pose --
     architecture Sec 1.1). Only pixels that CHANGED between the two isolate
     exactly what the smear+mote system painted, with the creature and sky
     held constant. This is the honest attribution tool the fogprobe pair
     exists for.

Usage:
    python mana_hue_probe.py bright <dir_of_rgb> [--bright 140] [--neutral 18] [--stride 1]
    python mana_hue_probe.py diffpair <off_dir> <mana_dir> [--neutral 18] [--changed 10]
  python mana_hue_probe.py dominant <off_dir> <fx_dir> [--dom 250]   (PASS 8)
  python mana_hue_probe.py selftest

PASS 8 -- THE ABLATION LATTICE IS NOW COMPLETE. Pass 7 shipped a fogprobe PAIR
with THE SMEAR OFF ON BOTH SIDES, so `diffpair` over it was structurally
incapable of seeing the smear plane, which was that pass's largest visual
change. `manafold-fogprobe-smear` (creature + smear, no mana bodies) is the
third leg; with it:
    motes alone = fogprobe-off  -> fogprobe-mana
    smear alone = fogprobe-off  -> fogprobe-smear
"""
import argparse
import glob
import os
import sys

import numpy as np

from rgbframe import load


def _frames(d, stride=1):
    paths = sorted(glob.glob(os.path.join(d, "*.rgb")))
    return paths[::stride]


def bright_stats(d, bright=140, neutral=18, stride=1):
    paths = _frames(d, stride)
    if not paths:
        raise SystemExit(f"no .rgb frames in {d}")
    tot_bright = 0
    tot_neutral = 0
    sat_sum = 0.0
    sat_n = 0
    for p in paths:
        img = load(p).astype(np.int16)
        mx = img.max(axis=2)
        mn = img.min(axis=2)
        mask = mx >= bright
        n = int(mask.sum())
        if n == 0:
            continue
        spread = (mx - mn)[mask]
        tot_bright += n
        tot_neutral += int((spread <= neutral).sum())
        # HSV saturation*255 = (max-min)*255/max (max>0 guaranteed: mask on mx>=bright)
        sat = spread.astype(np.float64) * 255.0 / mx[mask].astype(np.float64)
        sat_sum += float(sat.sum())
        sat_n += n
    frac = tot_neutral / tot_bright if tot_bright else 0.0
    mean_sat = sat_sum / sat_n if sat_n else 0.0
    print(f"{d}")
    print(f"  frames scanned      : {len(paths)}")
    print(f"  bright px total     : {tot_bright}")
    print(f"  hue-neutral px      : {tot_neutral} ({100.0*frac:.1f}%)")
    print(f"  mean saturation     : {mean_sat:.1f} (0-255 scale)")
    return frac, mean_sat


def diffpair_stats(off_dir, mana_dir, neutral=18, changed=10, bright=60):
    """bright: among the pixels the mana system CHANGED, only count ones the
    mana-side frame drew reasonably bright (max channel >= bright) as "a mana
    pixel" -- a dim antialiased fringe a few counts above the off-frame is a
    changed pixel but not a glow, and its near-zero channel spread is
    hue-neutral by being near-black, not by being white steam. This is the
    same distinction the by-eye review's "hue-neutral MANA pixel" phrase
    requires: bright and colourless (white) vs bright and coloured (aqua),
    not "any pixel the ablation touched at all"."""
    off_paths = _frames(off_dir)
    mana_paths = _frames(mana_dir)
    n = min(len(off_paths), len(mana_paths))
    if n == 0:
        raise SystemExit("empty pair")
    tot_changed = 0
    tot_neutral = 0
    sat_sum = 0.0
    for i in range(n):
        a = load(off_paths[i]).astype(np.int16)
        b = load(mana_paths[i]).astype(np.int16)
        d = np.abs(b - a).sum(axis=2)
        mx_all = b.max(axis=2)
        mask = (d > changed) & (mx_all >= bright)
        cnt = int(mask.sum())
        if cnt == 0:
            continue
        # measure the ADDED pixel's own colour (the mana-side frame, b)
        mx = mx_all[mask]
        mn = b.min(axis=2)[mask]
        spread = mx - mn
        tot_changed += cnt
        tot_neutral += int((spread <= neutral).sum())
        sat = spread.astype(np.float64) * 255.0 / np.maximum(mx.astype(np.float64), 1.0)
        sat_sum += float(sat.sum())
    frac = tot_neutral / tot_changed if tot_changed else 0.0
    mean_sat = sat_sum / tot_changed if tot_changed else 0.0
    print(f"{off_dir} -> {mana_dir}")
    print(f"  frame pairs         : {n}")
    print(f"  bright changed px   : {tot_changed}")
    print(f"  hue-neutral of those: {tot_neutral} ({100.0*frac:.1f}%)")
    print(f"  mean saturation     : {mean_sat:.1f} (0-255 scale)")
    return frac, mean_sat


def dominant_stats(off_dir, fx_dir, dom=250, neutral=18, corner=24, expect=(384, 240)):
    """PASS 8 -- the mode the pass-7 by-eye review had to hand-roll, committed.

    `bright` is taken over EVERY bright pixel in the frame, a population
    dominated by the pink sky and the pink body, in which the mana is a small
    minority: it reported 8.3% -> 7.9% hue-neutral for a mote system that was
    really 48.7%. `diffpair` fixed the population but still counts every pixel
    the effect TOUCHED, so a faint halo lying over pink sky votes with the
    sky's hue.

    This mode keeps only the pixels the effect DOMINATES -- |dR|+|dG|+|dB| >=
    `dom` -- and reports the mean RGB of those pixels as well as the neutral
    fraction, because "48.7% neutral at mean (215,229,236)" says "white steam"
    and a percentage alone does not.

    VALIDITY CHECK FIRST, and it is not optional: a `corner` x `corner` block
    of the top-left, which no version of this creature reaches, must be
    BYTE-IDENTICAL between the two directories. If it is not, the two renders
    differ by something other than the ablated effect and every number below is
    attributing that something to the mana. Pass 7's review did this check by
    hand and it is the reason its numbers can be believed."""
    off_paths = _frames(off_dir)
    fx_paths = _frames(fx_dir)
    n = min(len(off_paths), len(fx_paths))
    if n == 0:
        raise SystemExit("empty pair")
    bad_corner = 0
    tot = 0
    tot_neutral = 0
    sat_sum = 0.0
    rgb_sum = np.zeros(3, dtype=np.float64)
    for i in range(n):
        a_img = load(off_paths[i], expect).astype(np.int16)
        b_img = load(fx_paths[i], expect).astype(np.int16)
        if not np.array_equal(a_img[:corner, :corner], b_img[:corner, :corner]):
            bad_corner += 1
        d = np.abs(b_img - a_img).sum(axis=2)
        mask = d >= dom
        cnt = int(mask.sum())
        if cnt == 0:
            continue
        px = b_img[mask]
        mx = px.max(axis=1)
        mn = px.min(axis=1)
        spread = mx - mn
        tot += cnt
        tot_neutral += int((spread <= neutral).sum())
        sat_sum += float((spread.astype(np.float64) * 255.0 /
                          np.maximum(mx.astype(np.float64), 1.0)).sum())
        rgb_sum += px.sum(axis=0)
    frac = tot_neutral / tot if tot else 0.0
    mean_sat = sat_sum / tot if tot else 0.0
    mean_rgb = (rgb_sum / tot) if tot else np.zeros(3)
    print(f"{off_dir} -> {fx_dir}   (effect-dominated, |d| >= {dom})")
    print(f"  frame pairs         : {n}")
    print(f"  VALIDITY corner     : {'OK, byte-identical' if bad_corner == 0 else f'FAIL on {bad_corner}/{n} frames -- the pair differs by something other than the effect; STOP'}")
    print(f"  dominated px total  : {tot}")
    print(f"  hue-neutral of those: {tot_neutral} ({100.0*frac:.1f}%)")
    print(f"  mean saturation     : {mean_sat:.1f} (0-255 scale)")
    print(f"  mean RGB            : ({mean_rgb[0]:.0f},{mean_rgb[1]:.0f},{mean_rgb[2]:.0f})")
    return frac, mean_sat, tuple(mean_rgb)


def selftest():
    """Prove the mode can FAIL, on both axes. A metric that has never returned
    'white' on white and 'coloured' on colour has not been shown to work -- and
    this file's whole history is metrics that could not see their own subject."""
    import tempfile
    rc = 0
    with tempfile.TemporaryDirectory() as td:
        base = np.zeros((16, 16, 3), dtype=np.uint8)
        base[:, :] = (200, 140, 150)  # a pink sky
        for name, fill in (("white", (240, 245, 250)), ("aqua", (40, 210, 190))):
            d0 = os.path.join(td, name + "-off")
            d1 = os.path.join(td, name + "-fx")
            os.makedirs(d0)
            os.makedirs(d1)
            b = base.copy()
            b[8:14, 8:14] = fill  # away from the top-left validity corner
            _write(os.path.join(d0, "0000.rgb"), base)
            _write(os.path.join(d1, "0000.rgb"), b)
            frac, sat, rgb = dominant_stats(d0, d1, dom=60, corner=4, expect=(16, 16))
            if name == "white" and frac < 0.99:
                print("selftest FAIL: white patch not reported neutral")
                rc = 1
            if name == "aqua" and frac > 0.01:
                print("selftest FAIL: aqua patch reported neutral")
                rc = 1
        # and the validity check itself must be able to fail
        d0 = os.path.join(td, "v-off")
        d1 = os.path.join(td, "v-fx")
        os.makedirs(d0)
        os.makedirs(d1)
        b = base.copy()
        b[0, 0] = (0, 0, 0)  # a difference INSIDE the corner
        _write(os.path.join(d0, "0000.rgb"), base)
        _write(os.path.join(d1, "0000.rgb"), b)
        print("  (expect a corner FAIL on the next block)")
        dominant_stats(d0, d1, dom=60, corner=4, expect=(16, 16))
    print("mana_hue_probe selftest:", "OK" if rc == 0 else "FAILED")
    return rc


def _write(path, img):
    """Write a synthetic reel frame -- HEADER INCLUDED. rgbframe.load exists
    because two diagnostics on this project skipped the 8-byte header and
    rotated every pixel's channels; a selftest fixture that omitted it would be
    reproducing that exact fault inside the tool meant to catch it."""
    import struct
    h, w = img.shape[0], img.shape[1]
    with open(path, "wb") as fh:
        fh.write(struct.pack("<II", w, h))
        fh.write(np.ascontiguousarray(img, dtype=np.uint8).tobytes())


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("bright")
    b.add_argument("dir")
    b.add_argument("--bright", type=int, default=140)
    b.add_argument("--neutral", type=int, default=18)
    b.add_argument("--stride", type=int, default=1)
    d = sub.add_parser("diffpair")
    d.add_argument("off_dir")
    d.add_argument("mana_dir")
    d.add_argument("--neutral", type=int, default=18)
    d.add_argument("--changed", type=int, default=10)
    d.add_argument("--bright", type=int, default=60)
    m = sub.add_parser("dominant")
    m.add_argument("off_dir")
    m.add_argument("fx_dir")
    m.add_argument("--dom", type=int, default=250)
    m.add_argument("--neutral", type=int, default=18)
    m.add_argument("--corner", type=int, default=24)
    sub.add_parser("selftest")
    args = ap.parse_args()
    if args.cmd == "bright":
        bright_stats(args.dir, args.bright, args.neutral, args.stride)
    elif args.cmd == "dominant":
        dominant_stats(args.off_dir, args.fx_dir, args.dom, args.neutral, args.corner)
    elif args.cmd == "selftest":
        sys.exit(selftest())
    else:
        diffpair_stats(args.off_dir, args.mana_dir, args.neutral, args.changed, args.bright)


if __name__ == "__main__":
    main()
