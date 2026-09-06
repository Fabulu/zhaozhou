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
    args = ap.parse_args()
    if args.cmd == "bright":
        bright_stats(args.dir, args.bright, args.neutral, args.stride)
    else:
        diffpair_stats(args.off_dir, args.mana_dir, args.neutral, args.changed, args.bright)


if __name__ == "__main__":
    main()
