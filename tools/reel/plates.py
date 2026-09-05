"""Plate builder for creature by-eye passes: grids, pairs and contact sheets.

Lives here, not in a run folder: `CLAUDE.md` says a run folder orphans anything
durable, and this is the fourth pass in a row to need the same three pictures.
Imports `rgbframe` -- it does NOT re-implement the reader (four diagnostics on
this creature have been confidently wrong; two of them were bad readers).

    python plates.py grid  OUT.png SCALE LABEL:frame.rgb [LABEL:frame.rgb ...]
    python plates.py sheet OUT.png SCALE COLS dir/*.rgb        (contact sheet)
    python plates.py pair  OUT.png SCALE before.rgb after.rgb  (A/B, stacked)

Labels are drawn in a 5x7 bitmap font so a plate is self-describing when it is
looked at three passes later out of context.
"""
import sys, glob, os
import numpy as np
from rgbframe import load, save_png

_F = {
 'A':["01110","10001","10001","11111","10001","10001","10001"],
 'B':["11110","10001","11110","10001","10001","10001","11110"],
 'C':["01110","10001","10000","10000","10000","10001","01110"],
 'D':["11110","10001","10001","10001","10001","10001","11110"],
 'E':["11111","10000","11110","10000","10000","10000","11111"],
 'F':["11111","10000","11110","10000","10000","10000","10000"],
 'G':["01110","10001","10000","10111","10001","10001","01111"],
 'H':["10001","10001","11111","10001","10001","10001","10001"],
 'I':["11111","00100","00100","00100","00100","00100","11111"],
 'J':["00111","00010","00010","00010","10010","10010","01100"],
 'K':["10001","10010","11100","10010","10001","10001","10001"],
 'L':["10000","10000","10000","10000","10000","10000","11111"],
 'M':["10001","11011","10101","10101","10001","10001","10001"],
 'N':["10001","11001","10101","10011","10001","10001","10001"],
 'O':["01110","10001","10001","10001","10001","10001","01110"],
 'P':["11110","10001","10001","11110","10000","10000","10000"],
 'Q':["01110","10001","10001","10001","10101","10010","01101"],
 'R':["11110","10001","10001","11110","10100","10010","10001"],
 'S':["01111","10000","10000","01110","00001","00001","11110"],
 'T':["11111","00100","00100","00100","00100","00100","00100"],
 'U':["10001","10001","10001","10001","10001","10001","01110"],
 'V':["10001","10001","10001","10001","10001","01010","00100"],
 'W':["10001","10001","10001","10101","10101","11011","10001"],
 'X':["10001","10001","01010","00100","01010","10001","10001"],
 'Y':["10001","10001","01010","00100","00100","00100","00100"],
 'Z':["11111","00001","00010","00100","01000","10000","11111"],
 '0':["01110","10011","10101","10101","11001","10001","01110"],
 '1':["00100","01100","00100","00100","00100","00100","01110"],
 '2':["01110","10001","00001","00110","01000","10000","11111"],
 '3':["11111","00010","00100","00010","00001","10001","01110"],
 '4':["00010","00110","01010","10010","11111","00010","00010"],
 '5':["11111","10000","11110","00001","00001","10001","01110"],
 '6':["00110","01000","10000","11110","10001","10001","01110"],
 '7':["11111","00001","00010","00100","01000","01000","01000"],
 '8':["01110","10001","10001","01110","10001","10001","01110"],
 '9':["01110","10001","10001","01111","00001","00010","01100"],
 '-':["00000","00000","00000","11111","00000","00000","00000"],
 '.':["00000","00000","00000","00000","00000","01100","01100"],
 ' ':["00000"]*7, ':':["00000","01100","01100","00000","01100","01100","00000"],
}


def _text(canvas, x0, y0, s, scale=1, col=(255, 255, 80)):
    h, w = canvas.shape[:2]
    cx = x0
    for ch in s.upper():
        g = _F.get(ch, _F[' '])
        for r, row in enumerate(g):
            for c, bit in enumerate(row):
                if bit != '1':
                    continue
                y1, y2 = y0 + r * scale, y0 + (r + 1) * scale
                x1, x2 = cx + c * scale, cx + (c + 1) * scale
                if 0 <= y1 < h and 0 <= x1 < w:
                    canvas[y1:min(y2, h), x1:min(x2, w)] = col
        cx += (len(g[0]) + 1) * scale


def _up(img, k):
    return img if k == 1 else np.repeat(np.repeat(img, k, axis=0), k, axis=1)


BAND = 12  # label strip height


def grid(out, scale, items, cols=None):
    tiles = [(label, _up(load(path), scale)) for label, path in items]
    cols = cols or len(tiles)
    rows = (len(tiles) + cols - 1) // cols
    th, tw = tiles[0][1].shape[:2]
    W, H = cols * (tw + 4) + 4, rows * (th + BAND + 4) + 4
    canvas = np.full((H, W, 3), (24, 24, 28), dtype=np.uint8)
    for i, (label, img) in enumerate(tiles):
        r, c = divmod(i, cols)
        x0, y0 = 4 + c * (tw + 4), 4 + r * (th + BAND + 4)
        canvas[y0 + BAND:y0 + BAND + th, x0:x0 + tw] = img
        _text(canvas, x0, y0 + 2, label, 1)
    save_png(canvas, out)
    print("plates:", out, f"{W}x{H}", len(tiles), "tiles")


def main():
    cmd = sys.argv[1]
    out, scale = sys.argv[2], int(sys.argv[3])
    if cmd == "grid":
        grid(out, scale, [a.split(":", 1) for a in sys.argv[4:]])
    elif cmd == "sheet":
        cols = int(sys.argv[4])
        paths = []
        for pat in sys.argv[5:]:
            paths.extend(sorted(glob.glob(pat)))
        grid(out, scale, [(os.path.basename(p)[:4], p) for p in paths], cols)
    elif cmd == "pair":
        grid(out, scale, [("BEFORE", sys.argv[4]), ("AFTER", sys.argv[5])], 2)
    else:
        raise SystemExit("unknown command " + cmd)


if __name__ == "__main__":
    main()
