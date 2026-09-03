"""Contact sheet of every Nth frame at native 1x, tiled."""
import sys, os
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mlrgb import load
ver, step = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 12
d = os.path.join("scratch", ver, "zixxtrixx-moving-light")
fl = sorted(f for f in os.listdir(d) if f.endswith(".rgb"))[::step]
cols = 5
rows = (len(fl) + cols - 1) // cols
W, H = 384, 240
sheet = np.zeros((rows*H, cols*W, 3), np.uint8)
for i, f in enumerate(fl):
    r, c = divmod(i, cols)
    sheet[r*H:(r+1)*H, c*W:(c+1)*W] = load(os.path.join(d, f))
Image.fromarray(sheet).save(os.path.join("evidence", f"{ver}-contact-sheet.png"))
print("sheet:", len(fl), "frames")
