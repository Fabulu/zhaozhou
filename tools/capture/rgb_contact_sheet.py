#!/usr/bin/env python3
"""Contact sheet of EVERY frame in a reel subject directory.

CLAUDE.md, "Seeing the work properly": judging an animation from a handful of
evenly-spaced stills does not work -- uniform sampling finds the typical frame
and misses the broken one. So this lays out *all* of them, in order, with frame
numbers burned in, and never subsamples. If a sheet is unwieldy at 96 frames
that is the honest cost of looking at 96 frames.

The reel writes one `NNNN.rgb` per frame: an 8-byte header of width and height
as little-endian u32, then interleaved RGB888. GEOMETRY IS READ FROM THE FILE,
never assumed -- a tool that hardcodes 384x240 silently reinterprets its input
the day the capture size moves, and this one refuses instead. `palette.rgb`
shares the directory and is not a frame, so only all-digit names are collected.

    python3 tools/capture/rgb_contact_sheet.py <subject-dir> <out.png>
                                               [--cols N] [--scale K]

Exit codes: 0 sheet written, 2 bad arguments or no frames, 3 a frame is
truncated or disagrees with the first frame's geometry -- both are findings,
not something to pad over.
"""
import os
import sys

from PIL import Image, ImageDraw

HEADER = 8  # width u32le, height u32le


def parse_args(argv):
    if len(argv) < 3:
        sys.stderr.write(__doc__)
        return None
    a = {"src": argv[1], "out": argv[2], "cols": 0, "scale": 1}
    for i in range(3, len(argv)):
        if argv[i] in ("--cols", "--scale"):
            if i + 1 >= len(argv):
                sys.stderr.write("zhao: %s needs a value\n" % argv[i])
                return None
            a[argv[i][2:]] = int(argv[i + 1])
    return a


def main(argv):
    a = parse_args(argv)
    if a is None:
        return 2

    frames = sorted(f for f in os.listdir(a["src"])
                    if f.endswith(".rgb") and f[:-4].isdigit())
    if not frames:
        sys.stderr.write("zhao: no NNNN.rgb frames in %s\n" % a["src"])
        return 2

    def read_frame(name):
        """((w, h, pixels), None) or (None, message)."""
        with open(os.path.join(a["src"], name), "rb") as fh:
            blob = fh.read()
        if len(blob) < HEADER:
            return None, "%s is %d bytes, too short for the 8-byte header" % (
                name, len(blob))
        fw = int.from_bytes(blob[0:4], "little")
        fht = int.from_bytes(blob[4:8], "little")
        want = fw * fht * 3
        if len(blob) - HEADER != want:
            return None, ("%s says %dx%d (%d payload bytes) but carries %d -- "
                          "truncated capture"
                          % (name, fw, fht, want, len(blob) - HEADER))
        return (fw, fht, blob[HEADER:]), None

    first, err = read_frame(frames[0])
    if err:
        sys.stderr.write("zhao: %s\n" % err)
        return 3
    w, h = first[0], first[1]
    scale = a["scale"]

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
        got, err = read_frame(name)
        if err:
            sys.stderr.write("zhao: %s\n" % err)
            return 3
        if (got[0], got[1]) != (w, h):
            sys.stderr.write("zhao: %s is %dx%d but %s was %dx%d -- the subject "
                             "changes size mid-sequence\n"
                             % (name, got[0], got[1], frames[0], w, h))
            return 3
        im = Image.frombytes("RGB", (w, h), got[2])
        if scale != 1:
            im = im.resize((tw, th), Image.NEAREST)
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + label + pad)
        sheet.paste(im, (x, y))
        draw.text((x + 2, y + th + 1), name[:-4], fill=(180, 180, 190))

    sheet.save(a["out"])
    print("%s: %d frames at %dx%d, %dx%d sheet -> %s"
          % (a["src"], len(frames), w, h, sheet.width, sheet.height, a["out"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
