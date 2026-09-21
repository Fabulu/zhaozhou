"""Every-frame sheets for the pass-20 final bank: one PNG per subject (every frame,
96x60 tiles, frame index on each) plus a <=1600 px JPEG q80 viewing copy."""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, r'C:\programmieren\zencrifice\manafold-p16\zhaozhou\tools\reel')
from rgbframe import load  # noqa: E402

root, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
TW, TH, COLS = 96, 60, 20
rows_out = []
for subj in sorted(os.listdir(root)):
    d = os.path.join(root, subj)
    if not os.path.isdir(d):
        continue
    fs = sorted(f for f in os.listdir(d) if f.endswith('.rgb'))
    n = len(fs)
    rows = (n + COLS - 1) // COLS
    sheet = Image.new('RGB', (COLS * (TW + 2), rows * (TH + 2)), (0, 0, 0))
    dr = ImageDraw.Draw(sheet)
    for i, f in enumerate(fs):
        im = Image.fromarray(load(os.path.join(d, f)).astype(np.uint8)).resize((TW, TH), Image.BILINEAR)
        x, y = (i % COLS) * (TW + 2), (i // COLS) * (TH + 2)
        sheet.paste(im, (x, y))
        dr.text((x + 2, y + 1), str(i), fill=(255, 255, 0))
    png = os.path.join(out, f'P21-BANK-{subj.upper()}-ALLFRAMES.png')
    sheet.save(png, optimize=True)
    v = sheet.copy()
    v.thumbnail((1600, 1600))
    v.save(png[:-4] + '.jpg', quality=80)
    rows_out.append((subj, n, sheet.size, v.size))
    print(subj, n, sheet.size, v.size, flush=True)
print('sheets', len(rows_out), 'frames', sum(r[1] for r in rows_out))
