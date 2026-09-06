"""Decode a shipped `.webm` back into reel `.rgb` frames. USE THIS ONE.

WHY THIS FILE EXISTS
--------------------
The pass-7 render-and-publish run needed exactly this, wrote it inside its run
folder, and it is gone: `CLAUDE.md` says a run folder is the wrong home for
anything durable, and the pass-7 by-eye review had to write it a second time.
"A probe that does this was written once and thrown away, so its numbers are
unreproducible -- commit the probe."

The point of going BACK to `.rgb` rather than to PNG is that everything
downstream (`rgbframe.load`, `plates.py`) is the committed, header-verified
path. Four diagnostics on these creatures have been confidently wrong and two
of them were hand-rolled frame readers. This adds a decoder, not a reader.

    python webm2rgb.py <in.webm> <outdir> [--frames 12,34,56 | --all]
    python webm2rgb.py selftest

`--frames` is 0-based and matches the reel's own frame numbering, which is what
every clip caption and every run log on this creature quotes.

CAVEAT, stated because it changes what a measurement means: VP9 CRF16 yuv444p
is LOSSY. Frames recovered here are the *shipped* pixels -- which is exactly
right for judging what the site serves -- but they are NOT byte-identical to
the reel's own output. Do not CRC them against a render.
"""
import os
import subprocess
import struct
import sys

import numpy as np

W, H = 384, 240


def _ffmpeg(args):
    p = subprocess.run(args, capture_output=True)
    if p.returncode != 0:
        raise RuntimeError(f"ffmpeg failed rc={p.returncode}: {p.stderr[-800:].decode('utf8','replace')}")
    return p.stdout


def decode(path, expect=(W, H)):
    """Return an (n, h, w, 3) uint8 array of EVERY frame in the file."""
    raw = _ffmpeg(["ffmpeg", "-v", "error", "-i", path,
                   "-f", "rawvideo", "-pix_fmt", "rgb24", "-"])
    w, h = expect
    px = w * h * 3
    if len(raw) == 0:
        raise ValueError(f"{path}: decoded to ZERO bytes -- truncated or not a video. "
                         "(ffprobe reports vp9 on a 2000-byte stub; this does not.)")
    if len(raw) % px:
        raise ValueError(f"{path}: {len(raw)} bytes is not a whole number of {w}x{h} frames")
    return raw_to_array(raw, w, h)


def raw_to_array(raw, w=W, h=H):
    n = len(raw) // (w * h * 3)
    return np.frombuffer(raw, dtype=np.uint8).reshape(n, h, w, 3)


def write_rgb(arr, path):
    """Write one (h, w, 3) frame in the reel's own format: u32 w | u32 h | RGB."""
    h, w, _ = arr.shape
    with open(path, "wb") as fh:
        fh.write(struct.pack("<II", w, h))
        fh.write(np.ascontiguousarray(arr, dtype=np.uint8).tobytes())


def dump(path, outdir, frames=None):
    arr = decode(path)
    os.makedirs(outdir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(path))[0]
    idx = range(len(arr)) if frames is None else frames
    out = []
    for i in idx:
        if i < 0 or i >= len(arr):
            raise IndexError(f"{path}: frame {i} requested, file has {len(arr)}")
        p = os.path.join(outdir, f"{stem}-f{i:04d}.rgb")
        write_rgb(arr[i], p)
        out.append(p)
    return out, len(arr)


def selftest():
    """Prove the decoder can FAIL. Gate item 6: a check that cannot fail is worse
    than one that fails."""
    import tempfile
    ok = True
    d = tempfile.mkdtemp()

    # 1. A truncated stub must RAISE, not pass. This is the documented
    #    ffprobe hole (PASS-7-INPUTS section 8).
    stub = os.path.join(d, "stub.webm")
    with open(stub, "wb") as fh:
        fh.write(b"\x1a\x45\xdf\xa3" + b"\x00" * 2000)
    try:
        decode(stub)
        print("FAIL: a 2004-byte stub decoded without complaint")
        ok = False
    except Exception as e:
        print(f"ok  : stub rejected -- {type(e).__name__}")

    # 2. A real round trip must come back with the right shape AND the right
    #    colours. Build a video whose frame 0 is unmistakably RED, so a
    #    channel-rotating decoder (the exact bug rgbframe.py exists for)
    #    cannot pass.
    src = os.path.join(d, "red.webm")
    _ffmpeg(["ffmpeg", "-v", "error", "-y", "-f", "lavfi",
             "-i", f"color=c=red:s={W}x{H}:d=1:r=10",
             "-c:v", "libvpx-vp9", "-crf", "16", "-b:v", "0",
             "-pix_fmt", "yuv444p", src])
    arr = decode(src)
    if arr.shape[1:] != (H, W, 3):
        print(f"FAIL: shape {arr.shape}")
        ok = False
    r, g, b = arr[0].reshape(-1, 3).mean(axis=0)
    if not (r > 150 and g < 80 and b < 80):
        print(f"FAIL: red frame came back as ({r:.0f},{g:.0f},{b:.0f}) -- channels rotated?")
        ok = False
    else:
        print(f"ok  : red round-trips as ({r:.0f},{g:.0f},{b:.0f}), channels in order")

    # 3. And it must be able to tell red from blue -- a decoder that returns a
    #    constant would pass test 2.
    src2 = os.path.join(d, "blue.webm")
    _ffmpeg(["ffmpeg", "-v", "error", "-y", "-f", "lavfi",
             "-i", f"color=c=blue:s={W}x{H}:d=1:r=10",
             "-c:v", "libvpx-vp9", "-crf", "16", "-b:v", "0",
             "-pix_fmt", "yuv444p", src2])
    r2, g2, b2 = decode(src2)[0].reshape(-1, 3).mean(axis=0)
    if not (b2 > 150 and r2 < 80):
        print(f"FAIL: blue came back as ({r2:.0f},{g2:.0f},{b2:.0f})")
        ok = False
    else:
        print(f"ok  : blue round-trips as ({r2:.0f},{g2:.0f},{b2:.0f}) -- distinguishes hues")

    print("SELFTEST PASS" if ok else "SELFTEST FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "selftest":
        sys.exit(selftest())
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    inp, outdir = sys.argv[1], sys.argv[2]
    frames = None
    if "--frames" in sys.argv:
        frames = [int(x) for x in sys.argv[sys.argv.index("--frames") + 1].split(",")]
    files, total = dump(inp, outdir, frames)
    print(f"{inp}: {total} frames decoded, wrote {len(files)} to {outdir}")
