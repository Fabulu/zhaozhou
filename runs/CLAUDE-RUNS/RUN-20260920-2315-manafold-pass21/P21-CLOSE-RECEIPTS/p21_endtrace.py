#!/usr/bin/env python3
"""The End ball's motion trace, drawn from mrod's per-ring CSV.

Owner Direction 22 item 2: "the END part still spazzes". A peak number cannot
answer that -- a spazz is a SHAPE, a jagged run of tall isolated spikes, and a
single authored beat can be taller than a spazz while being perfectly smooth.
CLAUDE.md: "trajectory plots of tracked points over time (a flat line IS 'it
never bobs')". So this draws the trace, one row per clip, per 60 Hz sample:

    speed   |dP| of the End ball's centroid (rings 53..59), mm/sample
    jerk    |d3P|, the quantity a spazz actually is

It draws with PIL rather than matplotlib, which is not installed here.

⚠ IT MEASURES THE POSED SKIN, not a rendered frame, and it chooses nothing:
the clips are named on the command line and the scale of every row is printed
on the row.

  usage: p21_endtrace.py <mrod --csv output> <out.jpg> <slot:name> [slot:name ...]
"""
import csv
import sys
from PIL import Image, ImageDraw

END_RINGS = range(53, 60)
W, H, PAD = 1500, 150, 34


def centroid(row):
    return tuple(sum(float(row[f'c{a}{i}']) for i in END_RINGS) / len(END_RINGS)
                 for a in 'xyz')


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__.strip().splitlines()[-1].strip())
    csv_path, out = sys.argv[1], sys.argv[2]
    want = {}
    for spec in sys.argv[3:]:
        slot, name = spec.split(':')
        want[int(slot)] = name

    series = {s: [] for s in want}
    for row in csv.DictReader(open(csv_path)):
        s = int(row['slot'])
        if s in want:
            series[s].append(centroid(row))

    rows = []
    for slot, name in want.items():
        p = series[slot]
        if len(p) < 8:
            continue
        d1 = [tuple(b[i] - a[i] for i in range(3)) for a, b in zip(p, p[1:])]
        spd = [sum(c * c for c in v) ** 0.5 for v in d1]
        d2 = [tuple(b[i] - a[i] for i in range(3)) for a, b in zip(d1, d1[1:])]
        d3 = [tuple(b[i] - a[i] for i in range(3)) for a, b in zip(d2, d2[1:])]
        jrk = [sum(c * c for c in v) ** 0.5 for v in d3]
        rows.append((name, spd, jrk))

    img = Image.new('RGB', (W, H * len(rows) + PAD), (18, 18, 24))
    dr = ImageDraw.Draw(img)
    for r, (name, spd, jrk) in enumerate(rows):
        y0 = r * H + PAD
        dr.rectangle([0, y0, W, y0 + H - 2], outline=(70, 70, 90))
        smax, jmax = max(spd) or 1.0, max(jrk) or 1.0
        n = len(spd)
        for series_vals, vmax, col, half in ((spd, smax, (120, 220, 255), 0),
                                             (jrk, jmax, (255, 150, 120), 1)):
            base = y0 + (H // 2) * half + (H // 2) - 6
            pts = [(6 + i * (W - 12) / max(1, len(series_vals) - 1),
                    base - v / vmax * (H // 2 - 12))
                   for i, v in enumerate(series_vals)]
            dr.line(pts, fill=col, width=1)
        dr.text((8, y0 + 3),
                f'{name}  {n} samples   speed peak {smax:.2f} mm (cyan, upper)'
                f'   |jerk| peak {jmax:.2f} (orange, lower)', fill=(235, 235, 245))
    dr.text((8, 8), 'End-ball motion per 60 Hz sample, from the POSED skin. '
                    'Each row is autoscaled to its own peak -- read the SHAPE, '
                    'not the height.', fill=(235, 235, 245))
    img.thumbnail((1600, 1600))
    img.save(out, quality=82)
    print(out, img.size)
    for name, spd, jrk in rows:
        srt = sorted(jrk)
        med = srt[len(srt) // 2]
        print(f'  {name:14s} speed peak {max(spd):7.2f}  |jerk| peak {max(jrk):7.2f}'
              f'  median |jerk| {med:6.3f}  peak/median {max(jrk)/max(med,1e-9):7.1f}')


if __name__ == '__main__':
    main()
