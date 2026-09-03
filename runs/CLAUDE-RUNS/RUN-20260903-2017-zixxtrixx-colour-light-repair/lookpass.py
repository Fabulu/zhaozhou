"""Export a look set: named frames at 2x native for eye judgement."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mlrgb import load, save_png
ver = sys.argv[1]
frames = [int(x) for x in sys.argv[2:]] or [0, 60, 100, 150, 200, 225, 300, 360, 400, 450, 525, 570]
d = os.path.join("scratch", ver, "zixxtrixx-moving-light")
os.makedirs(os.path.join("evidence", ver), exist_ok=True)
for f in frames:
    save_png(load(os.path.join(d, f"{f:04d}.rgb")), os.path.join("evidence", ver, f"f{f:04d}.png"), 2)
print("exported", len(frames), "frames for", ver)
