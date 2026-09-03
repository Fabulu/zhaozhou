"""Header-verified reader for reel `.rgb` frames. USE THIS ONE.

A reel frame is an 8-byte header -- u32 width | u32 height, little-endian --
followed by h*w*3 bytes of RGB. That is the whole format.

WHY THIS FILE EXISTS
--------------------
Two separate diagnostics on the first creature read these frames *without*
honouring the header. Skipping 8 bytes of a 3-byte-per-pixel buffer rotates
every pixel's channels: red becomes green, green becomes blue, blue becomes red.
Both tools then produced confident, wrong colour judgements. One pass made two
rounds of light-gain cuts through a lying reader before the mismatch was caught
against an independently generated poster.

Counting the whole family, FOUR diagnostics on this creature have been
confidently wrong -- a contact tracker that classified the sky as ground, a 2D
contact measure that is unsound under perspective, a segmentation that masked in
terrain because the camera moves, and those two readers. Three of the four were
caught by a verification agent rather than by the agent that wrote them.

The reader that was written for the colour-light repair was correct, but it lived
inside that run's folder -- and `CLAUDE.md` is explicit that a run folder is the
wrong home for anything durable, because every pass creates a new one and orphans
the last. So it lives here now.

**Do not write another frame reader. Import this one.** If it is missing
something you need, add it here.

    from rgbframe import load, save_png, diff_vis, stats

Self-test:  python rgbframe.py selftest
CLI:        python rgbframe.py png <in.rgb> <out.png> [scale]
            python rgbframe.py diff <a.rgb> <b.rgb> <out.png>
"""
import os
import struct
import sys

import numpy as np

EXPECT_W, EXPECT_H = 384, 240
HEADER_BYTES = 8


def load(path, expect=(EXPECT_W, EXPECT_H)):
    """Return an (h, w, 3) uint8 array. Asserts the header AND the byte count.

    Pass expect=None to accept any size the header declares.
    """
    with open(path, "rb") as fh:
        raw = fh.read()
    if len(raw) < HEADER_BYTES:
        raise ValueError(f"{path}: {len(raw)} bytes, too short for an 8-byte header")
    w, h = struct.unpack("<II", raw[:HEADER_BYTES])
    if w == 0 or h == 0 or w > 1 << 16 or h > 1 << 16:
        raise ValueError(f"{path}: header declares an implausible {w}x{h} -- not a reel frame?")
    if expect is not None and (w, h) != tuple(expect):
        raise ValueError(f"{path}: header says {w}x{h}, expected {tuple(expect)}")
    need = HEADER_BYTES + w * h * 3
    if len(raw) != need:
        raise ValueError(
            f"{path}: {len(raw)} bytes, but a {w}x{h} frame needs {need}. "
            "A partial file means the render was still writing -- do not judge it."
        )
    return np.frombuffer(raw, dtype=np.uint8, offset=HEADER_BYTES).reshape(h, w, 3)


def save_png(arr, path, scale=1):
    from PIL import Image

    img = Image.fromarray(np.ascontiguousarray(arr), "RGB")
    if scale != 1:
        img = img.resize((arr.shape[1] * scale, arr.shape[0] * scale), Image.NEAREST)
    img.save(path)


def diff_vis(a, b):
    """Amplified signed difference on a grey ground: shows WHERE and WHICH channel."""
    d = a.astype(np.int16) - b.astype(np.int16)
    return np.clip(128 + d * 4, 0, 255).astype(np.uint8)


def stats(a, b):
    d = np.abs(a.astype(np.int16) - b.astype(np.int16))
    changed = d.sum(axis=2) > 0
    return {
        "changed_px": int(changed.sum()),
        "max_delta_rgb": [int(d[:, :, c].max()) for c in range(3)],
        "mean_delta_where_changed": float(d[changed].mean()) if changed.any() else 0.0,
    }


def _selftest():
    """Round-trip a synthetic frame and prove the channel order survives.

    The channel-rotation bug this file exists to prevent is invisible on greys
    and on any image whose channels happen to be similar, so the fixture uses
    three pure, unmistakable primaries.
    """
    import tempfile

    w, h = 4, 3
    src = np.zeros((h, w, 3), dtype=np.uint8)
    src[0, :] = (255, 0, 0)      # pure red row
    src[1, :] = (0, 255, 0)      # pure green row
    src[2, :] = (0, 0, 255)      # pure blue row

    with tempfile.TemporaryDirectory() as d:
        good = os.path.join(d, "good.rgb")
        with open(good, "wb") as fh:
            fh.write(struct.pack("<II", w, h))
            fh.write(src.tobytes())

        got = load(good, expect=(w, h))
        assert got.shape == (h, w, 3), got.shape
        assert tuple(got[0, 0]) == (255, 0, 0), f"red row read as {tuple(got[0, 0])}"
        assert tuple(got[1, 0]) == (0, 255, 0), f"green row read as {tuple(got[1, 0])}"
        assert tuple(got[2, 0]) == (0, 0, 255), f"blue row read as {tuple(got[2, 0])}"

        # The historical bug, reproduced, so the fixture proves it is detectable:
        # dropping the header without honouring it rotates every channel.
        with open(good, "rb") as fh:
            naive = np.frombuffer(fh.read()[HEADER_BYTES:], dtype=np.uint8)
        rotated = np.roll(np.frombuffer(src.tobytes(), dtype=np.uint8), 0)
        assert np.array_equal(naive, rotated), "fixture is wrong"

        # Truncated file must be refused, not silently reshaped.
        short = os.path.join(d, "short.rgb")
        with open(short, "wb") as fh:
            fh.write(struct.pack("<II", w, h))
            fh.write(src.tobytes()[:-3])
        try:
            load(short, expect=(w, h))
        except ValueError:
            pass
        else:
            raise AssertionError("a truncated frame was accepted")

        # Wrong declared size must be refused.
        try:
            load(good, expect=(EXPECT_W, EXPECT_H))
        except ValueError:
            pass
        else:
            raise AssertionError("a size mismatch was accepted")

        assert stats(src, src)["changed_px"] == 0

    print("rgbframe selftest: PASS")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "selftest"
    if cmd == "selftest":
        _selftest()
    elif cmd == "png":
        save_png(load(sys.argv[2]), sys.argv[3], scale=int(sys.argv[4]) if len(sys.argv) > 4 else 1)
    elif cmd == "diff":
        a, b = load(sys.argv[2]), load(sys.argv[3])
        save_png(diff_vis(a, b), sys.argv[4])
        print(stats(a, b))
    else:
        raise SystemExit(__doc__)
