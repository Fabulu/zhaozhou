# comparison-side tool: convert raw 384x240 RGB frames to PNG, optionally a contact sheet
import sys, os
from PIL import Image
W, H = 384, 240
def load(p):
    # THE FAULT THIS RUN NEARLY SHIPPED ON: %04d.rgb carries an 8-byte
    # u32 w | u32 h header (zhao_reel.cpp:29). Reading raw pixels without
    # skipping it rotates every pixel's channels (8 mod 3 = 2) and shifts the
    # image ~2.7 px: every colour judgement made through the unfixed reader saw
    # a psychedelic rotation of the real palette. Skip and VERIFY the header.
    import struct
    with open(p, 'rb') as f:
        b = f.read()
    w, h = struct.unpack('<II', b[:8])
    assert (w, h) == (W, H) and len(b) == 8 + W*H*3, (p, w, h, len(b))
    return Image.frombytes('RGB', (W, H), b[8:])
def main():
    src = sys.argv[1]; out = sys.argv[2]
    frames = sorted(f for f in os.listdir(src) if f.endswith('.rgb') and f[0].isdigit())
    sel = sys.argv[3] if len(sys.argv) > 3 else 'all'
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    if sel == 'sheet':
        step = int(sys.argv[5]) if len(sys.argv) > 5 else 1
        pick = frames[::step]
        cols = 8
        rows = (len(pick)+cols-1)//cols
        sheet = Image.new('RGB', (W*cols//2, H*rows//2))
        for i, fn in enumerate(pick):
            im = load(os.path.join(src, fn)).resize((W//2, H//2), Image.NEAREST)
            sheet.paste(im, ((i%cols)*W//2, (i//cols)*H//2))
        sheet.save(out); print('sheet', len(pick), 'frames ->', out); return
    idxs = [int(x) for x in sel.split(',')]
    for i in idxs:
        im = load(os.path.join(src, frames[i]))
        if scale != 1: im = im.resize((W*scale, H*scale), Image.NEAREST)
        p = out % i if '%' in out else out
        im.save(p); print(frames[i], '->', p)
main()
