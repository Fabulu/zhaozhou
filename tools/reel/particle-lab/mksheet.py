"""STAGE L: build one axis sheet from a list of variant tags.

  python mksheet.py NATIVE_OUT ZOOM_OUT TAG[=LABEL] TAG[=LABEL] ...

Rows are variants, columns are four frames spanning the knead window of the
rest clip (seg=3, frames 239-371, read off the U02_FOLD_DEBUG log):
  f250 the CRESCENT nearly pure (morph 19)
  f287 PEAK AGITATION (agit 740, the window's maximum) -- the folding test
  f320 mid-morph (663), the shape between two identities
  f368 the RING nearly pure (morph 996)
A treatment that only reads at f250 and f368 is one that does not fold.
The native sheet is the verdict; the zoom is a supplement (CLAUDE.md).
"""
import subprocess, sys, os
LANE = "C:/programmieren/zencrifice/manafold-p11-L"
PLATES = LANE + "/zhaozhou/tools/reel/plates.py"
FRAMES = [("250", "F250 CRESCENT"), ("287", "F287 KNEAD PEAK"),
          ("320", "F320 MID MORPH"), ("368", "F368 RING")]
BOX = "45,18,150,104"

native_out = os.path.abspath(sys.argv[1])
zoom_out = os.path.abspath(sys.argv[2])
items_n, items_z = [], []
for spec in sys.argv[3:]:
    tag, _, label = spec.partition("=")
    label = (label or tag).upper()
    for fr, frlabel in FRAMES:
        p = f"{LANE}/out/{tag}/manafold-fogprobe-mana/0{fr}.rgb"
        if not os.path.exists(p):
            raise SystemExit("missing " + p)
        items_n.append(f"{label} {frlabel}:{p}@0,0,384,240")
        items_z.append(f"{label} {frlabel}:{p}@{BOX}")
subprocess.run([sys.executable, PLATES, "crop", native_out, "1", "4"] + items_n,
               check=True, cwd=os.path.dirname(PLATES))
subprocess.run([sys.executable, PLATES, "crop", zoom_out, "3", "4"] + items_z,
               check=True, cwd=os.path.dirname(PLATES))
