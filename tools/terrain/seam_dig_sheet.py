"""Owner ruling R65's contact sheet: a dig across a patch seam, before/after.

R56 asked for a 65x65 vertex-aligned layer F. R65 found that pricing wrong
(+38.3% of the page stride, not +1.2%) and authorised the nearest-texel
fallback with the error MEASURED and DECLARED -- but it owes the owner a
picture first, because a half-cell step at every seam is an art defect and only
looking settles whether it reads.

Runs build/tests/terrain_seam_dig_render.exe, then lays the frames out as one
PNG:

    columns: BEFORE (undug) | SHIPPED (nearest texel) | ALIGNED (vertex-aligned)
    rows:    the wide camera at 1:1, the close camera at 1:1, and the close
             camera again at 3x nearest-neighbour

The 1:1 rows are the ones that answer the question -- 384x240 is VIDEO_Z60's
canvas and the read at final resolution is the whole point (CLAUDE.md: the
dorsal pink measured right and read wrong). The 3x row is there so that
whatever the owner sees at 1:1 can be identified, not so it can be judged.

The PNG is the evidence and the only artefact kept; the PPMs go to a temporary
directory and are deleted (CLAUDE.md: the evidence is the contact sheet, not
the frames it was made from). Usage:

    python tools/terrain/seam_dig_sheet.py [dig_centre_x ...]
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageEnhance

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT / "build" / "tests" / "terrain_seam_dig_render.exe"
OUT = ROOT / "reports" / "terrain-seam-dig" / "seam_dig_contact.png"

COLS = ("before", "shipped", "aligned")
PANELS = COLS + ("diff",)
TITLE = {
    "before": "BEFORE -- no dig. The seam is invisible by construction.",
    "shipped": "SHIPPED -- nearest texel (sheet_texel_for_vertex), what v1 draws.",
    "aligned": "ALIGNED -- 65x65 vertex-aligned, what R56 wanted.",
    "diff": "DIFF (4x amplified, diagnostic) -- where the two laws disagree.",
}


def run(tmp, cx):
    d = tmp / ("cx%s" % cx)
    d.mkdir()
    args = [str(EXE), str(d)]
    if cx is not None:
        args.append(str(cx))
    res = subprocess.run(args, capture_output=True, text=True, check=True)
    m = re.search(r"disagree: (\d+) of (\d+)", res.stdout)
    depth = re.search(r"full strength \d+: (-?[\d.]+) m", res.stdout)
    worst = re.search(r"worst height tear at a shared vertex: ([\d.]+) m", res.stdout)
    centre = re.search(r"centre x=([\d.]+)", res.stdout)
    return d, (m.group(1), m.group(2)), depth.group(1), worst.group(1), centre.group(1), res.stdout


def main(argv):
    centres = argv or [None]
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="gz-seam-dig-"))
    try:
        blocks = []
        for cx in centres:
            d, tear, depth, worst, centre, stdout = run(tmp, cx)
            sys.stdout.write(stdout)
            for cam, zoom in (("wide", 1), ("close", 1), ("close", 2)):
                ims = [Image.open(d / ("seam_%s_%s.ppm" % (cam, c))).convert("RGB") for c in COLS]
                # A FOURTH COLUMN: where the two laws actually differ, amplified.
                # It is a DIAGNOSTIC and is labelled as one -- the owner's
                # question is answered by the 1:1 frames, not by this. It is here
                # because "they look a bit different" is not a finding, and a
                # difference image says WHERE and HOW MUCH in one look.
                diff = ImageChops.difference(ims[1], ims[2])
                diff = ImageEnhance.Brightness(diff).enhance(4.0)
                ims = ims + [diff]
                if zoom != 1:
                    ims = [im.resize((im.width * zoom, im.height * zoom), Image.NEAREST)
                           for im in ims]
                blocks.append((centre, cam, zoom, tear, depth, worst, ims))

        gap, band, head = 10, 34, 82
        sheet_w = max(len(PANELS) * b[6][0].size[0] + (len(PANELS) + 1) * gap for b in blocks)
        sheet_h = head + sum(b[6][0].size[1] + band + gap for b in blocks)
        sheet = Image.new("RGB", (sheet_w, sheet_h), (22, 22, 24))
        draw = ImageDraw.Draw(sheet)
        draw.text((gap, 8),
                  "OWNER RULING R65 -- does the half-cell seam step READ? "
                  "Frames are 384x240, VIDEO_Z60's canvas. One key light, low and to the side.",
                  fill=(255, 210, 130))
        draw.text((gap, 26),
                  "The dig's rim is placed TANGENT to the seam at x=64 m: the worst case the law "
                  "allows. If it does not read here, it does not read.",
                  fill=(200, 200, 205))
        draw.text((gap, 44),
                  "NOTE: the declared 1/2 CELL is a HORIZONTAL sampling offset. Because coverage is "
                  "a threshold, the VERTICAL tear at a shared vertex is the dig's FULL DEPTH, "
                  "%s m -- not %s m." % (blocks[0][5], "0.50"),
                  fill=(255, 150, 150))
        draw.text((gap, 58),
                  "The few black specks on the crater walls are THIS TOOL dropping sub-pixel "
                  "slivers where a wall is edge-on. They are the rasteriser, not the terrain.",
                  fill=(150, 160, 175))
        y = head
        for cx, cam, zoom, tear, depth, worst, ims in blocks:
            iw, ih = ims[0].size
            label = ("%s camera%s | dig centre x=%s m | depth %s m | "
                     "%s of %s shared border vertices tear, worst %s m") % (
                cam, "" if zoom == 1 else "  (%dx nearest-neighbour, for identification only)" % zoom,
                cx, depth, tear[0], tear[1], worst)
            draw.text((gap, y + 6), label, fill=(240, 240, 245))
            for j, (c, im) in enumerate(zip(PANELS, ims)):
                x = gap + j * (iw + gap)
                sheet.paste(im, (x, y + band))
                draw.text((x + 3, y + band - 12), TITLE[c][:64], fill=(180, 190, 200))
            y += ih + band + gap
        OUT.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(OUT)
        print("wrote %s (%dx%d)" % (OUT.relative_to(ROOT), sheet.size[0], sheet.size[1]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main(sys.argv[1:])
