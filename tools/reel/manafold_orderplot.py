"""Plot Manafold's committed public-order trace from `mspan --csv` output.

The gate remains the measurement authority. This tool only extracts its
`frame,key,sub,...` table, writes a clean CSV companion and makes two static
comparison plots:

    python manafold_orderplot.py mspan.log OUT_PREFIX

Outputs: OUT_PREFIX.csv, OUT_PREFIX-heights.png, OUT_PREFIX-spans.png.
No optional plotting dependency is required; it uses the reel's PNG/font tools.
"""

import csv
import os
import sys

import numpy as np

from plates import _text
from rgbframe import save_png

HEADER = [
    "frame", "key", "sub", "A_y", "B_y", "C_y",
    "AB_y", "AC_y", "BC_y", "FA_mm", "AB_mm", "BC_mm", "CE_mm",
]
WITNESS = [142, 178, 212, 268]
SURFACE = (252, 252, 251)
INK = (11, 11, 11)
MUTED = (137, 135, 129)
GRID = (225, 224, 217)
AXIS = (195, 194, 183)
# Validated with dataviz/scripts/validate_palette.js --mode light --pairs all.
HEIGHT_COLORS = [(42, 120, 214), (235, 104, 52), (27, 175, 122)]
SPAN_COLORS = [(42, 120, 214), (235, 104, 52), (27, 175, 122), (74, 58, 167)]


def read_rows(path):
    rows = []
    in_order = False
    with open(path, encoding="utf-8-sig") as src:
        for raw in src:
            line = raw.strip()
            if line == ",".join(HEADER):
                in_order = True
                continue
            if not in_order or not line or not line[0].isdigit():
                continue
            fields = line.split(",")
            if len(fields) != len(HEADER):
                continue
            row = dict(zip(HEADER, fields))
            for name in ("frame", "key", "sub", "FA_mm", "AB_mm", "BC_mm", "CE_mm"):
                row[name] = int(row[name])
            for name in ("A_y", "B_y", "C_y", "AB_y", "AC_y", "BC_y"):
                row[name] = float(row[name])
            rows.append(row)
    if not rows:
        raise SystemExit(f"no public-order CSV rows found in {path}")
    return rows


def write_csv(path, rows):
    with open(path, "w", newline="", encoding="utf-8") as dst:
        writer = csv.DictWriter(dst, fieldnames=HEADER)
        writer.writeheader()
        writer.writerows(rows)


def line(img, x0, y0, x1, y1, color, width=2):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        for oy in range(-(width // 2), width - width // 2):
            for ox in range(-(width // 2), width - width // 2):
                x, y = x0 + ox, y0 + oy
                if 0 <= y < img.shape[0] and 0 <= x < img.shape[1]:
                    img[y, x] = color
        if x0 == x1 and y0 == y1:
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def chart_xy(frames, values, rect, y_min=None, y_max=None):
    x0, y0, x1, y1 = rect
    lo = min(values) if y_min is None else y_min
    hi = max(values) if y_max is None else y_max
    if hi <= lo:
        hi = lo + 1
    f0, f1 = frames[0], frames[-1]
    xs = [x0 + int((f - f0) * (x1 - x0) / max(1, f1 - f0)) for f in frames]
    ys = [y1 - int((v - lo) * (y1 - y0) / (hi - lo)) for v in values]
    return xs, ys, lo, hi


def axes(img, rect, lo, hi, frames, zero=False):
    x0, y0, x1, y1 = rect
    line(img, x0, y1, x1, y1, AXIS, 1)
    line(img, x0, y0, x0, y1, AXIS, 1)
    for i in range(5):
        y = y0 + int(i * (y1 - y0) / 4)
        line(img, x0, y, x1, y, GRID, 1)
        value = hi - i * (hi - lo) / 4
        _text(img, 6, y - 4, f"{value:.0f}", 1, MUTED)
    for f in WITNESS:
        if frames[0] <= f <= frames[-1]:
            x = x0 + int((f - frames[0]) * (x1 - x0) / max(1, frames[-1] - frames[0]))
            for y in range(y0, y1 + 1, 6):
                line(img, x, y, x, min(y + 2, y1), MUTED, 1)
    if zero and lo < 0 < hi:
        y = y1 - int((0 - lo) * (y1 - y0) / (hi - lo))
        line(img, x0, y, x1, y, AXIS, 1)


def draw_series(img, xs, ys, color):
    for i in range(1, len(xs)):
        line(img, xs[i - 1], ys[i - 1], xs[i], ys[i], color, 2)


def plot_heights(path, rows):
    img = np.full((700, 1400, 3), SURFACE, np.uint8)
    rect = (80, 80, 1280, 610)
    frames = [r["frame"] for r in rows]
    series = [[r[name] for r in rows] for name in ("A_y", "B_y", "C_y")]
    lo = min(min(v) for v in series) - 25
    hi = max(max(v) for v in series) + 25
    axes(img, rect, lo, hi, frames)
    for name, values, color, dy in zip("ABC", series, HEIGHT_COLORS, (-10, 0, 10)):
        xs, ys, _, _ = chart_xy(frames, values, rect, lo, hi)
        draw_series(img, xs, ys, color)
        _text(img, 1290, ys[-1] + dy - 4, name, 2, INK)
    _text(img, 80, 24, "MANAFOLD TAUNT III CROWN SHUFFLE - VISIBLE CORE HEIGHTS", 2, INK)
    _text(img, 80, 642, "PRESENTATION FRAME", 1, INK)
    _text(img, 1050, 642, "A BLUE  B ORANGE  C AQUA", 1, INK)
    save_png(img, path)


def plot_spans(path, rows):
    img = np.full((920, 1400, 3), SURFACE, np.uint8)
    frames = [r["frame"] for r in rows]
    names = ("FA_mm", "AB_mm", "BC_mm", "CE_mm")
    labels = ("F-A", "A-B", "B-C", "C-END")
    _text(img, 80, 18, "MANAFOLD TAUNT III CROWN SHUFFLE - SIGNED SPAN DELTAS", 2, INK)
    for panel, (name, label, color) in enumerate(zip(names, labels, SPAN_COLORS)):
        top = 62 + panel * 205
        rect = (80, top + 24, 1280, top + 175)
        values = [r[name] for r in rows]
        pad = max(10, int((max(values) - min(values)) * 0.08))
        lo, hi = min(values) - pad, max(values) + pad
        axes(img, rect, lo, hi, frames, zero=True)
        xs, ys, _, _ = chart_xy(frames, values, rect, lo, hi)
        draw_series(img, xs, ys, color)
        _text(img, 80, top, f"{label}: {min(values):+d} .. {max(values):+d} MM", 1, INK)
    _text(img, 80, 892, "PRESENTATION FRAME", 1, INK)
    save_png(img, path)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: manafold_orderplot.py MSPAN_LOG OUT_PREFIX")
    rows = read_rows(sys.argv[1])
    prefix = sys.argv[2]
    parent = os.path.dirname(prefix)
    if parent:
        os.makedirs(parent, exist_ok=True)
    write_csv(prefix + ".csv", rows)
    plot_heights(prefix + "-heights.png", rows)
    plot_spans(prefix + "-spans.png", rows)
    print(f"orderplot: {len(rows)} rows -> {prefix}.csv / heights / spans")


if __name__ == "__main__":
    main()
