"""Contact sheet of the EYE REGION on every frame of a clip.

The creature moves and the camera orbits, so a fixed crop box wanders off the
face. This LOCATES the eyes per frame (deep-purple lens pixels) and crops a
fixed-size window around their centroid, so every tile shows the same anatomy.

It LOCATES, it does not JUDGE -- the judging is done by looking at the sheet.
Locator failure is loud: a frame with no lens found is filled with magenta and
counted, never silently centred on the middle of the image.

    python eyesheet.py OUT.png SCALE COLS W H dir/*.rgb
    python eyesheet.py selftest
"""
import sys, glob, os
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "zhaozhou", "tools", "reel"))
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-p7-review\zhaozhou\tools\reel")
from rgbframe import load, save_png
import plates


def lens_mask(img):
    """Deep-purple eyeball. The sheet's 'whites, which are not white'.

    ! The first version of this matched `channel`'s DEEP PURPLE NIGHT SKY and
    reported an eye area of 23,990 px per frame -- 27x the real one. It was
    caught by disbelieving the number, not by the selftest, whose sky case was
    the MAUVE day sky only. The project's recorded fault is a creature mask
    that matched the sky; this is that fault, third time. The extra terms are
    the fix: the sky is smooth and fills the frame, the lens is a small
    saturated blob, so require a red floor (the lens is violet, not navy) and
    reject any component bigger than a plausible eye."""
    a = img.astype(int); r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = (b > r + 40) & (r > g + 15) & (b > 90) & (b < 220) & (r > 40) & (g < 70)
    return _drop_oversized(m)


def _drop_oversized(m, max_px=1400):
    """An eye is small. Anything bigger than both eyes put together is the sky."""
    from scipy import ndimage as nd
    lab, n = nd.label(m, structure=np.ones((3, 3)))
    if n == 0:
        return m
    keep = np.zeros_like(m)
    for i in range(1, n + 1):
        b = lab == i
        if b.sum() <= max_px:
            keep |= b
    return keep


def eye_centre(img):
    m = lens_mask(img)
    n = int(m.sum())
    if n < 20:
        return None, n
    ys, xs = np.nonzero(m)
    return (int(round(xs.mean())), int(round(ys.mean()))), n


def tile(img, w, h):
    c, n = eye_centre(img)
    if c is None:
        t = np.zeros((h, w, 3), np.uint8); t[..., 0] = 255; t[..., 2] = 255
        return t, False
    ih, iw = img.shape[:2]
    x = min(max(c[0] - w // 2, 0), iw - w)
    y = min(max(c[1] - h // 2, 0), ih - h)
    return img[y:y + h, x:x + w].copy(), True


def sheet(out, scale, cols, w, h, paths):
    tiles, miss = [], 0
    for p in paths:
        t, ok = tile(load(p), w, h)
        if not ok: miss += 1
        lbl = os.path.basename(p).rsplit("-f", 1)[-1].split(".")[0].lstrip("0") or "0"
        tiles.append((lbl, plates._up(t, scale)))
    plates._emit(out, scale, tiles, cols)
    print(f"eyesheet: {len(paths)} frames, {miss} with NO lens found"
          + (" <-- magenta tiles" if miss else ""))


def selftest():
    ok = True
    # a frame with no creature must NOT return a centre
    blank = np.full((240, 384, 3), (200, 120, 140), np.uint8)
    c, n = eye_centre(blank)
    print("ok  : flat mauve sky -> no lens found" if c is None
          else f"FAIL: flat sky returned a centre {c} from {n} px"); ok &= c is None
    # a synthetic lens must be found where it was put
    img = np.full((240, 384, 3), (200, 120, 140), np.uint8)
    img[100:120, 250:270] = (58, 28, 156)
    c, n = eye_centre(img)
    good = c is not None and abs(c[0] - 259) <= 2 and abs(c[1] - 109) <= 2
    print(f"ok  : synthetic lens found at {c}" if good else f"FAIL: found {c}, wanted ~(259,109)")
    ok &= good
    # and the SKY must not be mistaken for it -- the recorded fault, twice over
    for name, col in [("mauve day sky", (150, 90, 160)),
                      ("channel deep-purple NIGHT sky", (58, 40, 120)),
                      ("channel night sky, darker", (40, 26, 96))]:
        sky = np.full((240, 384, 3), col, np.uint8)
        c2, n2 = eye_centre(sky)
        good = c2 is None
        print(f"ok  : {name} not matched as lens" if good
              else f"FAIL: {name} matched, centre {c2} from {n2} px")
        ok &= good
    print("SELFTEST PASS" if ok else "SELFTEST FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    if sys.argv[1] == "selftest": sys.exit(selftest())
    out, scale, cols, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
    paths = []
    for pat in sys.argv[6:]: paths.extend(sorted(glob.glob(pat)))
    sheet(out, scale, cols, w, h, paths)
