"""Owner ruling R8's side-by-side contact sheet: both LOD deviation readings.

Runs build/tests/terrain_lod_readings_render.exe at each error scale, then lays
the frames out as one PNG:

    columns: morph reading (border excluded, the PROVISIONAL default) | mesh reading
    rows:    per scale, the shaded terrain, then the same frame with triangle
             edges drawn and each subpatch tinted by the level it chose
             (cream L0, green L1, ochre L2, rust L3)

The PNG is the evidence and the only artefact kept; the PPMs are written to a
temporary directory and deleted. Usage:

    python tools/terrain/lod_readings_sheet.py [scale ...]
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT / "build" / "tests" / "terrain_lod_readings_render.exe"
OUT = ROOT / "reports" / "terrain-lod-readings" / "lod_readings_contact.png"


def main(argv):
    scales = [int(a) for a in argv] or [16000, 40000]
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="gz-lod-readings-"))
    rows = []
    try:
        for s in scales:
            d = tmp / str(s)
            d.mkdir()
            res = subprocess.run([str(EXE), str(d), str(s)], capture_output=True, text=True, check=True)
            stats = {m.group(1): m.group(0) for m in re.finditer(r"^(morph|mesh)\s.*$", res.stdout, re.M)}
            differ = re.search(r"level differs on (\d+)", res.stdout).group(1)
            for kind in ("shade", "wire"):
                ims = [Image.open(d / ("lod_%s_%s.ppm" % (r, kind))).convert("RGB") for r in ("morph", "mesh")]
                rows.append((s, kind, differ, stats, ims))
        w, h = rows[0][4][0].size
        band = 36
        sheet = Image.new("RGB", (2 * w, len(rows) * (h + band)), (24, 24, 24))
        draw = ImageDraw.Draw(sheet)
        for i, (s, kind, differ, stats, ims) in enumerate(rows):
            y = i * (h + band)
            for j, (name, im) in enumerate(zip(("morph", "mesh"), ims)):
                sheet.paste(im, (j * w, y + band))
                label = "%s | scale %d | %s | %s" % (
                    "MORPH (border excluded, provisional)" if name == "morph" else "MESH (border included)",
                    s, kind, stats[name].split("  ", 1)[1].strip())
                draw.text((j * w + 6, y + 5), label, fill=(235, 235, 235))
            draw.text((6, y + 20), "scale %d: the two readings choose a different level on %s of 256 subpatches (camera at the near-left corner)" % (s, differ), fill=(255, 200, 120))
        OUT.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(OUT)
        print("wrote %s (%dx%d)" % (OUT.relative_to(ROOT), sheet.size[0], sheet.size[1]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main(sys.argv[1:])
