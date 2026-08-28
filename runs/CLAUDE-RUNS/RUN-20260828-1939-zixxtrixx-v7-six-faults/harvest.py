# RUN 1939 close-out harvest: SEQUENCE-CRCS from the final site frames
# (CRC law verified against a known baseline: file = 8-byte header +
# framebuffer; chained zhao_crc32c == crc32c of the concatenation),
# golden contact sheets refreshed from the same frames, and the final
# sidecmp input copied out.
import glob
import os
import sys

import numpy as np
from PIL import Image

SITE = r"C:\programmieren\zencrifice\Upheaval\website"
G = r"C:\programmieren\zencrifice\Upheaval\creature\Zixxtrixx\golden"
RUN = os.path.dirname(os.path.abspath(__file__))
W, H = 384, 240

poly = 0x82F63B78
tbl = []
for i in range(256):
    c = i
    for _ in range(8):
        c = (c >> 1) ^ poly if c & 1 else c >> 1
    tbl.append(c)


def crc32c(crc, data):
    crc = ~crc & 0xFFFFFFFF
    for b in data:
        crc = tbl[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return ~crc & 0xFFFFFFFF


SUBJECTS = [
    "zixxtrixx-attack", "zixxtrixx-balance", "zixxtrixx-damage",
    "zixxtrixx-death", "zixxtrixx-death2", "zixxtrixx-fall", "zixxtrixx-hit",
    "zixxtrixx-hitfloor", "zixxtrixx-idle", "zixxtrixx-knockdown",
    "zixxtrixx-look", "zixxtrixx-run", "zixxtrixx-salto-dummy",
    "zixxtrixx-salto-fly", "zixxtrixx-salto-six", "zixxtrixx-taunt",
    "zixxtrixx-walk",
]

lines = []
for s in SUBJECTS:
    files = sorted(glob.glob(os.path.join(SITE, "scratch-reel", s, "*.rgb")))
    if not files:
        print(f"MISSING FRAMES: {s}")
        sys.exit(1)
    crc = 0
    for f in files:
        d = open(f, "rb").read()
        crc = crc32c(crc, d[-W * H * 3:])
    lines.append(f"{s} sequence_crc32c=0x{crc:08X}")
    print(lines[-1])
open(os.path.join(G, "SEQUENCE-CRCS.txt"), "w", newline="\n").write(
    "\n".join(lines) + "\n")

# golden contact sheets: idle / walk / attack / fall, every frame
def sheet(sub, out, per=16):
    files = sorted(glob.glob(os.path.join(SITE, "scratch-reel", sub, "*.rgb")))
    ims = []
    for f in files:
        a = np.fromfile(f, dtype=np.uint8)
        a = a[len(a) - W * H * 3:].reshape(H, W, 3)
        ims.append(np.array(Image.fromarray(a).resize((W // 4, H // 4))))
    rows = []
    for r in range(0, len(ims), per):
        row = ims[r:r + per]
        while len(row) < per:
            row.append(np.zeros((H // 4, W // 4, 3), np.uint8))
        rows.append(np.concatenate(row, axis=1))
    Image.fromarray(np.concatenate(rows, axis=0)).save(out)
    print(out, len(ims), "frames")


for sub, name in [("zixxtrixx-idle", "contact-idle-60hz.png"),
                  ("zixxtrixx-walk", "contact-walk-60hz.png"),
                  ("zixxtrixx-attack", "contact-attack-60hz.png"),
                  ("zixxtrixx-fall", "contact-fall-60hz.png")]:
    sheet(sub, os.path.join(G, name))

print("harvest done")
