"""Eye plate: a hard zoom on the face with sub-luminance pixels marked, so
black notches cannot hide, plus a whole-frame contact strip.

Uses the COMMITTED reader (tools/reel/rgbframe.py). Four diagnostics on this
creature have been confidently wrong from home-rolled frame readers; the file
says "do not write another one, import this one", so this imports it.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, r'C:\programmieren\zencrifice\manafold-p7-impl\zhaozhou\tools\reel')
from rgbframe import load  # noqa: E402

DARK = 90  # QA's own luminance threshold for "a black notch"


def face_box(a):
    """Bounding box of purple-lens + cyan-star pixels: the face."""
    r = a[:, :, 0].astype(np.int16)
    g = a[:, :, 1].astype(np.int16)
    b = a[:, :, 2].astype(np.int16)
    purple = (b > 90) & (b > r + 25) & (g < b - 30) & (r < 170)
    cyan = (g > 130) & (b > 150) & (r < 130)
    m = purple | cyan
    if not m.any():
        return None
    ys, xs = np.nonzero(m)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def crop_pair(a, box, pad=7):
    h, w = a.shape[:2]
    x0 = max(0, box[0] - pad)
    y0 = max(0, box[1] - pad)
    x1 = min(w, box[2] + pad + 1)
    y1 = min(h, box[3] + pad + 1)
    c = a[y0:y1, x0:x1].copy()
    ci = c.astype(np.int32)
    lum = (ci[:, :, 0] * 299 + ci[:, :, 1] * 587 + ci[:, :, 2] * 114) // 1000
    dark = lum < DARK
    m = c.copy()
    m[dark] = (255, 0, 255)
    return c, m, int(dark.sum())


def up(a, s):
    return Image.fromarray(np.kron(a, np.ones((s, s, 1), dtype=np.uint8)))


def plate(paths, out, label, scale=9):
    rows = []
    total = 0
    for p in paths:
        a = load(p)
        box = face_box(a)
        if box is None:
            continue
        c, m, n = crop_pair(a, box)
        total += n
        rows.append((os.path.basename(p), n, up(c, scale), up(m, scale),
                     up(a, 2)))
    if not rows:
        print('NO FACE FOUND in', paths)
        return -1
    zw = max(r[2].size[0] for r in rows)
    zh = max(r[2].size[1] for r in rows)
    fw = rows[0][4].size[0]
    fh = rows[0][4].size[1]
    rowh = max(zh, fh) + 20
    sheet = Image.new('RGB', (zw * 2 + fw + 24, rowh * len(rows) + 20), (16, 16, 20))
    d = ImageDraw.Draw(sheet)
    d.text((6, 4), '%s   magenta = luminance < %d' % (label, DARK),
           fill=(255, 220, 60))
    for i, (name, n, z, zm, full) in enumerate(rows):
        y = 20 + i * rowh
        sheet.paste(full, (4, y + 14))
        sheet.paste(z, (fw + 12, y + 14))
        sheet.paste(zm, (fw + zw + 18, y + 14))
        d.text((6, y + 2), '%s    dark px in face box: %d' % (name, n),
               fill=(210, 210, 220))
    sheet.save(out)
    print('%-42s tiles=%d  TOTAL DARK PX=%d' % (out, len(rows), total))
    return total


if __name__ == '__main__':
    src, out, label = sys.argv[1], sys.argv[2], sys.argv[3]
    picks = sys.argv[4].split(',')
    plate([os.path.join(src, p + '.rgb') for p in picks], out, label)
