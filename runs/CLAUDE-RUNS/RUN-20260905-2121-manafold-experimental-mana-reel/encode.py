"""Encode one lab clip to webm, with the project's own settings.

Matches Upheaval/website/tools/tovideo.py exactly -- 60 fps (the reel renders
one 60 Hz sim tick per frame; encoding at 30 played every clip at half speed),
libvpx-vp9 crf 16 / b:v 0, yuv444p, row-mt. And it VERIFIES: a failed ffmpeg
has shipped a 0-byte webm on this creature before, and the poster was the only
thing that noticed.

  encode.py <clipdir> <out.webm> [poster.png]
"""
import sys, os, tempfile, subprocess
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "..", "zhaozhou", "tools", "reel"))
import rgbframe
from PIL import Image

FPS = 60

def main(d, webm, poster=None, poster_frame=292):
    frames = sorted(f for f in os.listdir(d) if f.endswith(".rgb"))
    os.makedirs(os.path.dirname(webm) or ".", exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        for i, f in enumerate(frames):
            Image.fromarray(rgbframe.load(os.path.join(d, f)), "RGB").save(
                os.path.join(td, "%04d.png" % i))
        cmd = ["ffmpeg", "-v", "error", "-y", "-framerate", str(FPS),
               "-i", os.path.join(td, "%04d.png"),
               "-c:v", "libvpx-vp9", "-crf", "16", "-b:v", "0",
               "-pix_fmt", "yuv444p", "-row-mt", "1", "-an", webm]
        rc = subprocess.call(cmd)
    if rc != 0:
        raise SystemExit("ffmpeg FAILED rc=%d for %s" % (rc, webm))
    sz = os.path.getsize(webm)
    if sz < 4096:
        raise SystemExit("ffmpeg produced a %d-byte webm for %s" % (sz, webm))
    if poster:
        a = rgbframe.load(os.path.join(d, "%04d.rgb" % poster_frame))
        im = Image.fromarray(a, "RGB")
        im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(poster)
    print("%-46s %8d bytes  %d frames" % (os.path.basename(webm), sz, len(frames)))

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
