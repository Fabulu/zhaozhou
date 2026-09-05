"""Per-frame read of the three things the owner named: mana, pink, fog.

Every frame, never a sample. All three are stated as READS, not as targets --
the values here exist so the reviewer's eye can be aimed at the worst frame and
so a claim can be checked, never to choose a constant (CLAUDE.md art law).

MANA       see manaread.py. white_share = the fraction of mana pixels that have
           clamped to hue-neutral. High = it reads as steam, not as plasma.
PINK       the body's own pigment: strongly red-dominant, saturated. Reported as
           the fraction CLIPPED (red at 255, carrying no form) and the fraction
           DARK (lum-sum < 300, form lost at the other end). Direction 5 §4 says
           the pink reads weak BECAUSE it is clipped, so both ends matter.
FOG        Direction 5 §3: the gassy band inside the black line, "thickened by a
           lot, still see-through, very visible". Measured as the width of the
           transition band between the cel ink and the sky, sampled along the
           silhouette: for each boundary crossing, how many pixels are neither
           clean sky nor clean ink. A hard-edged creature returns ~0-1.
"""
import os, sys, json
import numpy as np
from PIL import Image
from scipy import ndimage


def split(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    mx = np.maximum(np.maximum(r, g), b); mn = np.minimum(np.minimum(r, g), b)
    sat = mx - mn; lum = a.astype(int).sum(2)
    mana = ((b >= r) | (g >= r)) & (lum > 430) & (mx > 150)
    pink = (r - g > 70) & (sat > 90) & (r > b)
    ink = lum < 140
    return dict(r=r, g=g, b=b, sat=sat, lum=lum, mana=mana, pink=pink, ink=ink)


def frame(a):
    d = split(a)
    out = {}
    n = int(d["mana"].sum())
    if n:
        s = d["sat"][d["mana"]]
        out["mana_px"] = n
        out["mana_sat"] = float(s.mean())
        out["mana_white"] = float((s < 60).mean())
    else:
        out["mana_px"] = 0; out["mana_sat"] = 0.0; out["mana_white"] = 0.0
    p = int(d["pink"].sum())
    out["pink_px"] = p
    if p:
        out["pink_clip"] = float((d["r"][d["pink"]] >= 255).mean())
        out["pink_dark"] = float((d["lum"][d["pink"]] < 300).mean())
    else:
        out["pink_clip"] = 0.0; out["pink_dark"] = 0.0
    # fog: dilate the ink outward and ask how far the "neither sky nor creature"
    # transition extends. Sky is smooth; a fog band is a graded ring.
    ink = d["ink"]
    if ink.sum() > 20:
        grow = ndimage.binary_dilation(ink, np.ones((3, 3), bool), iterations=6)
        halo = grow & ~ndimage.binary_dilation(ink, np.ones((3, 3), bool))
        # a fog pixel is measurably darker/greyer than the sky at the same row
        skyref = np.median(a.astype(int)[:, :6, :].sum(2), axis=1)[:, None]
        dev = np.abs(d["lum"] - skyref)
        out["fog_px"] = int((halo & (dev > 18)).sum())
        out["halo_px"] = int(halo.sum())
        out["fog_frac"] = out["fog_px"] / max(1, out["halo_px"])
    else:
        out["fog_px"] = 0; out["halo_px"] = 0; out["fog_frac"] = 0.0
    return out


def scan(framedir):
    rows = []
    for fn in sorted(os.listdir(framedir)):
        rows.append(frame(np.asarray(Image.open(os.path.join(framedir, fn)).convert("RGB"))))
    return rows


if __name__ == "__main__":
    W = sys.argv[1]; clips = sys.argv[2:]
    res = {}
    hdr = f"{'clip':<24} {'manaWHITE% med(min-max)':>26} {'manaSat med':>12} {'pinkCLIP% med(max)':>20} {'pinkDARK% med(max)':>20} {'fog%':>6}"
    print(hdr)
    for c in clips:
        rows = scan(os.path.join(W, "frames", c))
        g = lambda k: np.array([r[k] for r in rows])
        mw, ms, pc, pd, fg = g("mana_white") * 100, g("mana_sat"), g("pink_clip") * 100, g("pink_dark") * 100, g("fog_frac") * 100
        res[c] = dict(n=len(rows), mana_white_med=float(np.median(mw)), mana_sat_med=float(np.median(ms)),
                      pink_clip_med=float(np.median(pc)), pink_clip_max=float(pc.max()),
                      pink_dark_med=float(np.median(pd)), pink_dark_max=float(pd.max()),
                      fog_med=float(np.median(fg)))
        print(f"{c:<24} {np.median(mw):9.1f} ({mw.min():.0f}-{mw.max():.0f})     {np.median(ms):8.1f}     "
              f"{np.median(pc):8.1f} ({pc.max():.0f})     {np.median(pd):8.1f} ({pd.max():.0f})   {np.median(fg):5.1f}")
    json.dump(res, open(os.path.join(W, "evidence", "clipscan.json"), "w"), indent=1)
