"""Review crops for the pass-20 final bank (viewing aid only; chooses nothing).

  p20crops.py strip OUT.jpg SCALE X0 Y0 W H  SUBJ:F[,F...] [SUBJ:F,...]
      one row per SUBJ group, every listed frame cropped to (X0,Y0,W,H) of the
      384x240 frame and nearest-upscaled by SCALE; labels on each tile.
      A crop box of 0 0 0 0 means the whole frame.
  p20crops.py grid OUT.jpg SCALE SUBJ F0 F1 STEP [X0 Y0 W H]
      consecutive frames F0..F1 step STEP, wrapped into rows.
Output is a JPEG q80 no larger than 1600 px on its long side.
"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, r'C:\programmieren\zencrifice\manafold-p16\zhaozhou\tools\reel')
from rgbframe import load  # noqa: E402

ROOT = os.environ.get('P20_ROOT', r'C:\programmieren\zencrifice\manafold-p16\p20-final-reel-22')


# The pass-19 raw bank was deleted after pass 19's production verification. The
# same-binary both-mechanisms-off render (KNEAD_DIP_PM=0 + REAR_BOW=legacy)
# reproduces it byte for byte (scope-attribution.txt), so it stands in as the
# pass-19 frames. The dip-off bank (shipping bow, no beat) is the control for
# the knead and its particle reaction (shipping bow, no beat, no reaction).
V18_ROOT = r'C:\programmieren\zencrifice\manafold-p16\p20-scope-pass19'
DIPOFF_ROOT = r'C:\programmieren\zencrifice\manafold-p16\p20-scope-rearonly'


def tile(subj, f, box, scale):
    root = ROOT
    if '/' in subj:  # "manafold-p19/inspect" / "manafold-off/inspect" prefixes
        tag, name = subj.split('/', 1)
        tag = tag.replace('manafold-', '')
        root = DIPOFF_ROOT if tag == 'off' else V18_ROOT
        subj = 'manafold-' + name
        subj_label = ('dip-off ' if tag == 'off' else 'p19 ') + name
    else:
        subj_label = subj.replace('manafold-', '')
    im = Image.fromarray(load(os.path.join(root, subj, f'{f:04d}.rgb')).astype(np.uint8))
    if box[2] and box[3]:
        im = im.crop((box[0], box[1], box[0] + box[2], box[1] + box[3]))
    im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    ImageDraw.Draw(im).text((3, 2), f'{subj_label} f{f}', fill=(255, 255, 0))
    return im


def save(rows, out):
    w = max(sum(t.width + 2 for t in r) for r in rows)
    h = sum(max(t.height for t in r) + 2 for r in rows)
    sheet = Image.new('RGB', (w, h), (0, 0, 0))
    y = 0
    for r in rows:
        x = 0
        for t in r:
            sheet.paste(t, (x, y))
            x += t.width + 2
        y += max(t.height for t in r) + 2
    sheet.thumbnail((1600, 1600))
    sheet.save(out, quality=80)
    print(out, sheet.size)


def main():
    mode, out, scale = sys.argv[1], sys.argv[2], int(sys.argv[3])
    if mode == 'strip':
        box = tuple(int(v) for v in sys.argv[4:8])
        rows = []
        for group in sys.argv[8:]:
            subj, frames = group.split(':')
            rows.append([tile('manafold-' + subj, int(f), box, scale) for f in frames.split(',')])
        save(rows, out)
    else:
        subj, f0, f1, step = sys.argv[4], int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7])
        box = tuple(int(v) for v in sys.argv[8:12]) if len(sys.argv) > 8 else (0, 0, 0, 0)
        tiles = [tile('manafold-' + subj, f, box, scale) for f in range(f0, f1 + 1, step)]
        per = max(1, 1600 // (tiles[0].width + 2))
        save([tiles[i:i + per] for i in range(0, len(tiles), per)], out)


if __name__ == '__main__':
    main()
