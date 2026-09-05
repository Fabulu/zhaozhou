"""Channel-ceiling headroom meter (RECON B, Manafold pass 6).

The documented failure mode (08-LIGHTING.md, 09-ENGINE-GOTCHAS §4): additive
elements stacked past the channel ceiling clamp and go hue-neutral WHITE,
erasing their own colour. `glow_splat` adds saturating at 255, so the question
"how many more particles before it whitens" is answerable by asking how much
additive headroom the mana's own pixels still have.

Method, per frame:
  * MANA MASK = pixels whose colour is cyan/aqua dominant (B and G both well
    above R) OR already hue-neutral and very bright. This is the mana's own
    palette family (kManaAqua/Cyan/White ramps); the creature's pigment is
    pink/grey and the ground is brown, so they do not qualify.
  * For those pixels report: fraction with any channel >= 250 (CLAMPED),
    fraction hue-neutral-bright (min>=190 and max-min<=34: WHITENED),
    and the mean additive headroom = 255 - max(channel).
  * HEADROOM MULTIPLIER: the mana is additive and linear until it clamps, so
    scaling the additive load by N multiplies each mana pixel's excess over
    the local background. Reported as the N at which the MEDIAN mana pixel's
    brightest channel reaches 255, taking the frame's own 10th-percentile
    mana-pixel brightness as the stand-in background.

This is a METER on the comparison side, per CLAUDE.md: it says how far from
the ceiling we are. It does not choose a particle count.
"""
import sys, os, glob
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'tools', 'reel'))
from rgbframe import load

def mana_mask(a):
    r = a[:, :, 0].astype(np.int32); g = a[:, :, 1].astype(np.int32); b = a[:, :, 2].astype(np.int32)
    mx = np.maximum(np.maximum(r, g), b); mn = np.minimum(np.minimum(r, g), b)
    aqua = (g > r + 25) & (b > r + 15) & (mx >= 110)
    hot = (mn >= 190) & ((mx - mn) <= 34)
    return (aqua | hot), r, g, b, mx, mn

def run(d, stride=5):
    fs = sorted(glob.glob(os.path.join(d, '*.rgb')))[::stride]
    tot = clamp = white = 0
    hrs = []; mults = []
    for f in fs:
        a = load(f)
        m, r, g, b, mx, mn = mana_mask(a)
        n = int(m.sum())
        if n < 40:
            continue
        tot += n
        clamp += int(((mx >= 250) & m).sum())
        white += int(((mn >= 190) & ((mx - mn) <= 34) & m).sum())
        v = mx[m]
        hrs.append(255.0 - float(v.mean()))
        bg = float(np.percentile(v, 10)); med = float(np.median(v))
        if med > bg + 1:
            mults.append((255.0 - bg) / (med - bg))
    if tot == 0:
        print(f"{os.path.basename(d)}: NO mana pixels found -- the mask found ZERO of the thing it counts. Do not read a verdict from this.")
        return
    print(f"{os.path.basename(d)}: frames={len(fs)} mana_px_total={tot} "
          f"mean_px/frame={tot/max(1,len(hrs)):.0f}")
    print(f"  clamped (any ch >=250): {100.0*clamp/tot:.2f}%   "
          f"whitened (min>=190, spread<=34): {100.0*white/tot:.2f}%")
    print(f"  mean additive headroom to 255: {np.mean(hrs):.1f} counts")
    print(f"  additive-load multiplier at which the MEDIAN mana pixel clamps: "
          f"x{np.median(mults):.2f}  (p10 x{np.percentile(mults,10):.2f})")

for d in sys.argv[1:]:
    run(d)
