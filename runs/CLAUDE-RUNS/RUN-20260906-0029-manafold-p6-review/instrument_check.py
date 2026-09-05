"""Prove the frame reader is honest BEFORE any colour judgement is made.

Gate checklist item 3 (two readers on this project silently rotated channels)
and item 6 (a check that cannot fail manufactures confidence).

The shipped posters are 1152x720 = 3x nearest-neighbour blowups of the native
384x240 frame. Decimating them by 3 recovers the native pixels, so:

  TEST 1  HONEST  -- some ffmpeg-extracted frame must match the decimated
          poster EXACTLY (mean|delta| == 0). A channel rotation, a colour-space
          conversion or a resample would all break this.
  TEST 2  NN      -- the poster's 3x3 blocks must be constant, or the blowup
          was filtered and the decimation is not recovering native pixels.
  TEST 3  CAN-FAIL -- the same search against a channel-rotated poster must
          report a large mismatch. If it does not, TEST 1 proves nothing.
"""
import sys, os
import numpy as np
from PIL import Image

def arr(p):
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)

def decimate(poster):
    a = arr(poster)
    h, w, _ = a.shape
    assert h % 3 == 0 and w % 3 == 0, f"poster {w}x{h} not a 3x blowup"
    blocks = a.reshape(h // 3, 3, w // 3, 3, 3)
    nn = bool((blocks == blocks[:, :1, :, :1, :]).all())
    return a[::3, ::3, :], nn

def search(target, framedir):
    best = (10**9, None)
    for fn in sorted(os.listdir(framedir)):
        f = arr(os.path.join(framedir, fn))
        if f.shape != target.shape:
            continue
        d = float(np.abs(f - target).mean())
        if d < best[0]:
            best = (d, fn)
        if d == 0:
            break
    return best

def main(poster, framedir):
    tgt, nn = decimate(poster)
    d, fn = search(tgt, framedir)
    rot = tgt[:, :, [2, 0, 1]]
    d2, _ = search(rot, framedir)
    print(f"TEST 2 NN blowup      : {'PASS' if nn else 'FAIL'}")
    print(f"TEST 1 HONEST         : best frame {fn}, mean|delta| = {d}")
    print(f"TEST 3 CAN-FAIL       : channel-rotated best mean|delta| = {d2}")
    ok = nn and d == 0 and d2 > 3
    print("INSTRUMENT:", "HONEST and CAPABLE OF FAILING" if ok else "SUSPECT")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
