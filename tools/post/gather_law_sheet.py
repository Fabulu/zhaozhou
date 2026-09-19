"""Owner ruling R37's before/after contact sheet: the PROPOSED tag-to-gather law.

Runs build/tests/gather_law_render.exe, then lays the frames out as one PNG:

    row 0:  the resolved frame, NO gather (the "before")
    rows 1+: one per glow KNEE, the same frame with the law applied, at
             bloom_gain 128 and 255

The KNEE is the constant the picture is most sensitive to (zref::post::gather::
kGlowKnee, proposed 24): below it a texel is lit, above it a texel is a light.
The middle row is the proposal; the rows around it are what a lower and a higher
knee look like, so the owner is choosing between pictures rather than numbers.

The PNG is the evidence and the only artefact kept; the PPMs are written to a
temporary directory and deleted (CLAUDE.md: "the evidence is the PNG contact
sheet, not the frames it was made from"). Usage:

    python tools/post/gather_law_sheet.py [knee ...]
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT / "build" / "tests" / "gather_law_render.exe"
OUT = ROOT / "reports" / "post-gather-law" / "gather_law_contact.png"
LABEL_H = 18
PAD = 6


def load(path):
    with Image.open(path) as im:
        return im.convert("RGB")


def main(argv):
    knees = [int(a) for a in argv] or [16, 24, 32]
    if not EXE.exists():
        print(f"missing {EXE}; build the gather_law_render target first")
        return 2
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="gz-gather-law-"))
    try:
        res = subprocess.run([str(EXE), str(tmp), *[str(k) for k in knees]],
                             capture_output=True, text=True, check=True)
        print(res.stdout.rstrip())
        stats = {int(m.group(1)): m.group(0)
                 for m in re.finditer(r"^knee\s+(\d+).*$", res.stdout, re.M)}

        rows = [[("before: resolved, no gather", load(tmp / "before.ppm"))]]
        for k in knees:
            row = []
            for gain in (128, 255):
                img = load(tmp / f"after_k{k:02d}_g{gain:03d}.ppm")
                row.append((f"knee {k}  bloom_gain {gain}", img))
            rows.append(row)

        # THE BLUR IS THE OTHER JUDGEMENT, and it is a COST rather than a look
        # value: POST.GATHER's Part A is a separable sweep over the 96 x 60 cell
        # plane, so one PASS is two sweeps = 11,520 cell-steps, which is exactly
        # the contract's budgeted glow prep for Z60. Two passes leave BLOCKY
        # haloes (the plane is quarter-res and the compositor samples it per
        # cell); five make them round, and cost five times the prep.
        mid = knees[len(knees) // 2]
        blur_row = []
        for passes in (2, 5):
            d = tmp / f"b{passes}"
            d.mkdir()
            subprocess.run([str(EXE), str(d), str(mid), "-b", str(passes)],
                           capture_output=True, text=True, check=True)
            img = load(d / f"after_k{mid:02d}_g255.ppm")
            blur_row.append((f"knee {mid} gain 255, blur {passes} pass = "
                             f"{passes * 11520:,} cell-steps", img))
        rows.append(blur_row)

        w, h = rows[0][0][1].size
        cols = max(len(r) for r in rows)
        sheet = Image.new("RGB",
                          (cols * w + (cols + 1) * PAD,
                           len(rows) * (h + LABEL_H) + (len(rows) + 1) * PAD),
                          (18, 18, 22))
        draw = ImageDraw.Draw(sheet)
        for ri, row in enumerate(rows):
            y = PAD + ri * (h + LABEL_H + PAD)
            for ci, (label, img) in enumerate(row):
                x = PAD + ci * (w + PAD)
                sheet.paste(img, (x, y + LABEL_H))
                draw.text((x + 2, y + 4), label, fill=(235, 235, 240))
        OUT.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(OUT)
        print(f"wrote {OUT} ({sheet.size[0]}x{sheet.size[1]})")
        for k in knees:
            if k in stats:
                print("  " + stats[k])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
