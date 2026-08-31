#!/usr/bin/env python3
"""Contact sheet of EVERY frame in a reel subject directory.

CLAUDE.md, "Seeing the work properly": judging an animation from a handful of
evenly-spaced stills does not work -- uniform sampling finds the typical frame
and misses the broken one. So this lays out *all* of them, in order, with frame
numbers burned in, and never subsamples. If a sheet is unwieldy at 96 frames
that is the honest cost of looking at 96 frames.

The reel writes raw interleaved RGB888 at 384x240 (Z60), one `NNNN.rgb` per
frame, plus `meta.txt`. Nothing here parses meta -- geometry is a flag so the
tool keeps working if the capture size ever moves.

    python3 tools/capture/rgb_contact_sheet.py <subject-dir> <out.png>
                                               [--cols N] [--w 384] [--h 240]
                                               [--scale K]

Exit codes: 0 sheet written, 2 bad arguments or no frames, 3 a frame is the
wrong size (a truncated capture is a finding, not something to pad over).
"""
import os
import sys

from PIL import Image, ImageDraw


def parse_args(argv):
    if len(argv) < 3:
        sys.stderr.write(__doc__)
        return None
    a = {"src": argv[1], "out": argv[2], "cols": 0, "w": 384, "h": 240, "scale": 1}
    it = iter(range(3, len(argv)))
    for i in it:
        if i >= len(argv):
            break
        f = argv[i]
        if f in ("--cols", "--w", "--h", "--scale"):
            if i + 1 >= len(argv):
                sys.stderr.write("zhao: %s needs a value\n" % f)
                return None
            a[f[2:]] = int(argv[i + 1])
    return a


def main(argv):
    a = parse_args(argv)
    if a is None:
        return 2

    frames = sorted(f for f in os.listdir(a["src"]) if f.endswith(".rgb"))
    if not frames:
        sys.stderr.write("zhao: no .rgb frames in %s\n" % a["src"])
        return 2

    w, h, scale = a["w"], a["h"], a["scale"]
    expect = w * h * 3
    # Default to a squarish sheet rather than one long strip.
    cols = a["cols"] or max(1, int(len(frames) ** 0.5 + 0.5))
    rows = (len(frames) + cols - 1) // cols

    tw, th = w * scale, h * scale
    label = 12
    pad = 2
    sheet = Image.new("RGB", (cols * (tw + pad) + pad, rows * (th + label + pad) + pad),
                      (24, 24, 28))
    draw = ImageDraw.Draw(sheet)

    for i, name in enumerate(frames):
        with open(os.path.join(a["src"], name), "rb") as fh:
            raw = fh.read()
        if len(raw) != expect:
            sys.stderr.write("zhao: %s is %d bytes, expected %d (%dx%d RGB888) -- "
                             "truncated capture\n" % (name, len(raw), expect, w, h))
            return 3
        im = Image.frombytes("RGB", (w, h), raw)
        if scale != 1:
            im = im.resize((tw, th), Image.NEAREST)
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + label + pad)
        sheet.paste(im, (x, y))
        draw.text((x + 2, y + th + 1), name[:-4], fill=(180, 180, 190))

    sheet.save(a["out"])
    print("%s: %d frames, %dx%d sheet -> %s" % (a["src"], len(frames), sheet.width,
                                                sheet.height, a["out"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
