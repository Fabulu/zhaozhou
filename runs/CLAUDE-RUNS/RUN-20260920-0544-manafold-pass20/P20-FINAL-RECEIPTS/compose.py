"""Stack whole-bank sheets into one composite viewing JPEG (<=1600 px, q80).

Viewing aid only. It chooses nothing and measures nothing; it exists so several
short subjects can be looked at in one image instead of one each. The composite
SCALE is printed, because that is what decides what the look can and cannot
judge.
"""
import sys
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parents[1] / "P20-BANK-SHEETS"
out = sys.argv[1]
ims = [Image.open(HERE / f"P20-BANK-MANAFOLD-{s.upper()}-ALLFRAMES.png") for s in sys.argv[2:]]
w = max(i.width for i in ims)
h = sum(i.height + 6 for i in ims)
c = Image.new("RGB", (w, h), (255, 255, 255))
y = 0
for i in ims:
    c.paste(i, (0, y))
    y += i.height + 6
scale = min(1600 / c.width, 1600 / c.height)
c.thumbnail((1600, 1600))
c.save(out, quality=80)
print(out, c.size, f"scale={scale:.3f} tile~{round(96*scale)}px")
