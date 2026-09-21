#!/usr/bin/env python3
"""mkclipbank.py -- the CLIP_BANK page (cartridge kind 9), emitted.

Owner ruling R90 (reports/OWNER-RULINGS-20260919-EVENING.md, amended
2026-09-20), recommendation item 1: "author/freeze the body section AND A
MINIMAL KIND-9 FRAME WITH A ZREF MODEL". `mkcreatureladder.py` emitted the body
section on 2026-09-21. This file is the other half of that sentence.

WHAT IT REFUSES, WHICH IS THE POINT
-----------------------------------
`spec/cartridge.md` 4 says of kinds 8 and 9: "until then the packer refuses to
emit them (deterministic refusal, never a guessed layout)". R26 lifted that
for four ladder fields, R90 for the bone hierarchy and a clip frame. So this
packer emits a page header, a CLIP DIRECTORY and FRAMES, and it carries event
tag bytes through OPAQUELY -- it records their offset and length and does not
interpret one, because event tags are sim-side (creature_rules 2.1/4.2) and are
not part of the lift. It writes no baked 60 Hz midpoint channel and no
deformation sample, and it will not be extended to before SW.TOOLS.ASSET
freezes them.

WHAT THE LIFT WAS ACTUALLY NEEDED FOR, since a sentence in the tree says it was
not
-------------------------------------------------------------------------------
`zref_creature_page.hpp` says "A kind-9 frame reader therefore has a layout to
read and needs no lift; what it needs is a producer." That is true of a FRAME
and false of a PAGE. `spec/creature_rules.md` 2.1 freezes what one frame
CONTAINS (12 B root displacement then bone_count x 8 B of quat16) and 5 sketches
the page in one clause -- "clip directory {slot_id u16, frame_count u16,
event_count u16} + frames + event tags" -- under a heading reading "layouts
freeze with SW.TOOLS.ASSET at Phase 12 entry". No magic, no version, no field
order, no offsets, no alignment, and no statement of where frame `f` of clip `c`
begins. A reader cannot read a frozen frame it cannot LOCATE.

THE LAYOUT IS NOT DEFINED HERE
------------------------------
`reference/include/zref/zref_clip_page.hpp` is the layout. This file is a
SECOND statement of the same offsets, which is one more than anybody wants --
so `--check` exists: it rebuilds the committed golden
`tests/golden/creature_clip/clip_page_v1.bin` and compares byte for byte, and
`tests/geometry/clip_page_directed.cpp` requires `zref::clip_page::build` to
reproduce that SAME file. Packer and model are then pinned to one artefact
rather than to each other's good intentions, and a layout edit that misses one
of them goes red. When an RTL reader is built it becomes the third.

THE GOLDEN IS THE BONE SOURCE'S OWN FIXTURE, DELIBERATELY. Its six bones are
`tests/geometry/geom_bonesrc_directed.cpp`'s `kGoldenBones` skeleton and its
frame-0 quaternions are that file's `bone_quat(b)`, so the clip page and the
body page describe ONE creature and a reader can be differenced against the
bench that already exists. A fixture invented here would have pinned nothing.

Usage:
    mkclipbank.py bank.json out.bin
    mkclipbank.py --check
    mkclipbank.py --write-golden
"""
import argparse
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
GOLDEN = REPO / "tests" / "golden" / "creature_clip" / "clip_page_v1.bin"

# zref::clip_page, restated. See the module docstring on why a second statement
# exists and what pins it.
MAGIC = 0x504C435A  # 'Z','C','L','P' little-endian
VERSION = 1
HEADER_BYTES = 64
CLIP_BYTES = 32
FRAME_HEADER_BYTES = 64
QUAT_BYTES = 8
ROOT_BYTES = 12
LINE_BYTES = 64
MAX_BONES = 32   # creature_rules 1.2
MAX_CLIPS = 64   # creature_rules 2.1, "64 authored slots per creature"

CLIP_FIELDS = ("slot_id", "event_count", "frames", "event_bytes")
FRAME_FIELDS = ("root", "bones")


class Refusal(Exception):
    """A deterministic refusal, never a guessed layout."""


def round_up_line(n):
    return n if n % LINE_BYTES == 0 else n + (LINE_BYTES - n % LINE_BYTES)


def frame_stride(bone_count):
    """What one frame OCCUPIES on the page's 64-byte read grid.

    NOT what a frame contains: creature_rules 2.1's "<= 268 B/frame at 32
    bones" stays exactly true of the bytes. The 52 spare bytes of the frame
    header buy two properties the silicon needs -- a bone's quaternion is one
    64-bit beat ON a beat boundary (packed after a 12-byte root, every
    quaternion in the page would straddle two beats), and a frame begins on a
    line boundary so "frame f" is a multiply and not a walk. The reasoning is
    `zref_creature_page.hpp`'s, one page kind over.
    """
    return FRAME_HEADER_BYTES + round_up_line(QUAT_BYTES * bone_count)


def _i32(v, where):
    if not isinstance(v, int) or isinstance(v, bool):
        raise Refusal("%s: %r is not an integer" % (where, v))
    if v < -(1 << 31) or v > (1 << 31) - 1:
        raise Refusal("%s: %d does not fit an i32" % (where, v))
    return v


def _i16(v, where):
    if not isinstance(v, int) or isinstance(v, bool):
        raise Refusal("%s: %r is not an integer" % (where, v))
    if v < -(1 << 15) or v > (1 << 15) - 1:
        raise Refusal("%s: %d does not fit an s16 quat16 lane" % (where, v))
    return v


def check_bank(clips, bone_count, rows=MAX_CLIPS):
    """Every legality `zref::clip_page::bank_legal` enforces, plus the JSON
    shape errors a C++ struct cannot have."""
    if not isinstance(bone_count, int) or isinstance(bone_count, bool):
        raise Refusal("bone_count: %r is not an integer" % (bone_count,))
    if bone_count < 1 or bone_count > MAX_BONES:
        raise Refusal(
            "bone_count %d is outside 1..%d (creature_rules 1.2's ceiling)"
            % (bone_count, MAX_BONES)
        )
    if not clips:
        raise Refusal("a bank with no clips")
    if len(clips) > rows or len(clips) > MAX_CLIPS:
        raise Refusal(
            "%d clips is past the bank's %d rows -- a page declaring more rows "
            "than the reader holds is REFUSED WHOLE, because a reader keeping "
            "the first %d would answer some lookups from this page and others "
            "from the last one" % (len(clips), min(rows, MAX_CLIPS), min(rows, MAX_CLIPS))
        )
    seen = {}
    for i, c in enumerate(clips):
        where = "clip %d" % i
        missing = [f for f in ("slot_id", "frames") if f not in c]
        if missing:
            raise Refusal("%s: missing %s" % (where, ", ".join(missing)))
        extra = [k for k in c if k not in CLIP_FIELDS]
        if extra:
            raise Refusal("%s: unknown field(s) %s" % (where, ", ".join(sorted(extra))))
        slot = c["slot_id"]
        if not isinstance(slot, int) or isinstance(slot, bool) or not 0 <= slot <= 0xFFFF:
            raise Refusal("%s: slot_id %r is not a u16" % (where, slot))
        if slot in seen:
            raise Refusal(
                "%s: slot_id %d is already clip %d -- the reader answers the "
                "FIRST match, so two rows for one slot is a page whose meaning "
                "depends on row order" % (where, slot, seen[slot])
            )
        seen[slot] = i
        frames = c["frames"]
        if not isinstance(frames, list) or not frames:
            raise Refusal("%s: a clip with no frames" % where)
        if len(frames) > 0xFFFF:
            raise Refusal("%s: %d frames does not fit the u16 frame_count"
                          % (where, len(frames)))
        for j, f in enumerate(frames):
            fwhere = "%s frame %d" % (where, j)
            fmissing = [k for k in FRAME_FIELDS if k not in f]
            if fmissing:
                raise Refusal("%s: missing %s" % (fwhere, ", ".join(fmissing)))
            fextra = [k for k in f if k not in FRAME_FIELDS]
            if fextra:
                raise Refusal("%s: unknown field(s) %s" % (fwhere, ", ".join(sorted(fextra))))
            root = f["root"]
            if not isinstance(root, list) or len(root) != 3:
                raise Refusal("%s: root is not three fx16 values" % fwhere)
            for k, v in enumerate(root):
                _i32(v, "%s root[%d]" % (fwhere, k))
            bones = f["bones"]
            if not isinstance(bones, list) or len(bones) != bone_count:
                raise Refusal(
                    "%s: %d rotations against bone_count %d -- a frame that is "
                    "not the bank's width cannot be indexed by bone"
                    % (fwhere, len(bones) if isinstance(bones, list) else -1, bone_count)
                )
            for b, q in enumerate(bones):
                if not isinstance(q, list) or len(q) != 4:
                    raise Refusal("%s bone %d: a quat16 is four s16 lanes "
                                  "(w, x, y, z -- qformats 7.6 C1)" % (fwhere, b))
                for lane, v in enumerate(q):
                    _i16(v, "%s bone %d lane %d" % (fwhere, b, lane))
        ev = c.get("event_bytes", [])
        if not isinstance(ev, list):
            raise Refusal("%s: event_bytes is not a byte array" % where)
        for v in ev:
            if not isinstance(v, int) or isinstance(v, bool) or not 0 <= v <= 255:
                raise Refusal("%s: event_bytes holds %r, not a byte" % (where, v))
        ec = c.get("event_count", 0)
        if not isinstance(ec, int) or isinstance(ec, bool) or not 0 <= ec <= 0xFFFF:
            raise Refusal("%s: event_count %r is not a u16" % (where, ec))
        if ec != 0 and not ev:
            raise Refusal(
                "%s: event_count is %d with no event_bytes -- a count naming "
                "bytes that are not there is the truncation a reader cannot "
                "distinguish from a short page" % (where, ec)
            )
    return True


def build(clips, bone_count, rows=MAX_CLIPS):
    check_bank(clips, bone_count, rows)
    stride = frame_stride(bone_count)
    dir_off = HEADER_BYTES
    frames_off = round_up_line(dir_off + CLIP_BYTES * len(clips))

    frame_off = []
    event_off = []
    cur = frames_off
    for c in clips:
        frame_off.append(cur)
        cur += stride * len(c["frames"])
        ev = c.get("event_bytes", [])
        if ev:
            event_off.append(cur)
            cur += round_up_line(len(ev))
        else:
            event_off.append(0)

    page = bytearray(round_up_line(cur))
    struct.pack_into("<IHBBHH", page, 0, MAGIC, VERSION, bone_count, 0, len(clips), 0)
    struct.pack_into("<II", page, 12, dir_off, frames_off)

    for i, c in enumerate(clips):
        d = dir_off + CLIP_BYTES * i
        ev = c.get("event_bytes", [])
        struct.pack_into("<HHHH", page, d, c["slot_id"], len(c["frames"]),
                         c.get("event_count", 0), 0)
        struct.pack_into("<III", page, d + 8, frame_off[i], event_off[i], len(ev))
        for j, f in enumerate(c["frames"]):
            s = frame_off[i] + stride * j
            struct.pack_into("<iii", page, s, f["root"][0], f["root"][1], f["root"][2])
            q = s + FRAME_HEADER_BYTES
            for b, lanes in enumerate(f["bones"]):
                struct.pack_into("<hhhh", page, q + QUAT_BYTES * b, *lanes)
        for k, v in enumerate(ev):
            page[event_off[i] + k] = v
    return bytes(page)


# ---------------------------------------------------------------------------
# THE GOLDEN FIXTURE. Six bones, the skeleton
# `tests/geometry/geom_bonesrc_directed.cpp` already packs a kind-8 body for,
# and frame 0's rotations are that file's `bone_quat(b)` -- so the two goldens
# describe ONE creature and a reader can be differenced against a bench that
# exists. See the module docstring.
#
# THE QUATERNIONS MUST DIFFER PER BONE AND PER FRAME. With every bone carrying
# the same rotation, a reader that fetched bone b-1's lane would produce the
# RIGHT palette and the fixture would discriminate nothing -- the reason
# `geom_bonesrc_directed` gives for its own generator, which is why this is that
# generator and not a new one.
GOLDEN_BONE_COUNT = 6


def _bone_quat(b):
    return [16384 - 300 * b, 1500 * (b + 1), 700 * b, -400 * b]


def _frame(f):
    """Frame f. The root walks so two frames are never one frame, and every
    lane is offset by f so a reader that reads frame 0 for every request is
    caught by the numbers rather than by a counter."""
    return {
        "root": [4096 * f, -2048 * f, 1024 * f],
        "bones": [[q + 11 * f for q in _bone_quat(b)] for b in range(GOLDEN_BONE_COUNT)],
    }


GOLDEN_CLIPS = [
    # Slot 0 is not the first row, on purpose: a reader that returns row zero
    # for a miss, or that assumes slot == row, is caught by the golden itself.
    {"slot_id": 7, "frames": [_frame(0), _frame(1), _frame(2)]},
    {"slot_id": 0, "frames": [_frame(3)], "event_count": 2,
     "event_bytes": [0x01, 0x00, 0x2A, 0x00, 0x02, 0x00, 0x5B, 0x00]},
]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("source", nargs="?", help="a JSON object {bone_count, clips}")
    ap.add_argument("out", nargs="?", help="the .bin to write")
    ap.add_argument("--check", action="store_true",
                    help="rebuild the golden and compare byte for byte")
    ap.add_argument("--write-golden", action="store_true",
                    help="(re)write the committed golden -- a layout change")
    a = ap.parse_args(argv)

    if a.write_golden:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        page = build(GOLDEN_CLIPS, GOLDEN_BONE_COUNT)
        GOLDEN.write_bytes(page)
        print("mkclipbank: wrote %s, %d bytes, %d clips, %d bones"
              % (GOLDEN, len(page), len(GOLDEN_CLIPS), GOLDEN_BONE_COUNT))
        return 0

    if a.check:
        if not GOLDEN.exists():
            print("mkclipbank: %s is missing" % GOLDEN, file=sys.stderr)
            return 1
        want = GOLDEN.read_bytes()
        got = build(GOLDEN_CLIPS, GOLDEN_BONE_COUNT)
        if got != want:
            print(
                "mkclipbank: the packer no longer reproduces %s (%d bytes built, "
                "%d on disk) -- the frozen layout moved in ONE of the places that "
                "state it (this file, reference/include/zref/zref_clip_page.hpp)"
                % (GOLDEN, len(got), len(want)), file=sys.stderr)
            for i in range(min(len(got), len(want))):
                if got[i] != want[i]:
                    print("  first difference at byte %d: %02X vs %02X"
                          % (i, got[i], want[i]), file=sys.stderr)
                    break
            return 1

        # The page's own arithmetic, read back off the bytes rather than
        # recomputed -- an RTL reader's address chain must agree with THIS.
        magic, version, bones, rsv0, clips, rsv1 = struct.unpack_from("<IHBBHH", want, 0)
        if magic != MAGIC or version != VERSION or rsv0 or rsv1:
            print("mkclipbank: the golden's own header does not read back",
                  file=sys.stderr)
            return 1
        stride = frame_stride(bones)
        for i in range(clips):
            d = HEADER_BYTES + CLIP_BYTES * i
            slot, fcount, ecount, drsv = struct.unpack_from("<HHHH", want, d)
            foff, eoff, ebytes = struct.unpack_from("<III", want, d + 8)
            if drsv:
                print("mkclipbank: clip %d's reserved u16 is not zero" % i, file=sys.stderr)
                return 1
            if foff % LINE_BYTES or foff + stride * fcount > len(want):
                print("mkclipbank: clip %d's frames are off the grid or past the page" % i,
                      file=sys.stderr)
                return 1
            if ebytes and (eoff % LINE_BYTES or eoff + ebytes > len(want)):
                print("mkclipbank: clip %d's event bytes are off the grid" % i,
                      file=sys.stderr)
                return 1
            if not ebytes and eoff:
                print("mkclipbank: clip %d declares no event bytes at a non-zero offset" % i,
                      file=sys.stderr)
                return 1
            _ = (slot, ecount)

        # A refusal that never fires is a claim. Fire each one here, where a
        # `--check` run is the only thing that reads them.
        fired = 0
        good = GOLDEN_CLIPS
        for bad_clips, bad_bones, why in (
            ([], GOLDEN_BONE_COUNT, "a bank with no clips"),
            (good, 0, "a zero bone_count"),
            (good, MAX_BONES + 1, "a bone_count past the 32-bone ceiling"),
            ([dict(good[0], frames=[])], GOLDEN_BONE_COUNT, "a clip with no frames"),
            ([good[0], dict(good[0])], GOLDEN_BONE_COUNT, "a duplicate slot_id"),
            ([dict(good[0], frames=[{"root": [0, 0, 0],
                                     "bones": [_bone_quat(0)]}])],
             GOLDEN_BONE_COUNT, "a frame that is not the bank's width"),
            ([dict(good[0], frames=[{"root": [0, 0], "bones":
                                     [_bone_quat(b) for b in range(GOLDEN_BONE_COUNT)]}])],
             GOLDEN_BONE_COUNT, "a root that is not three values"),
            ([dict(good[0], frames=[{"root": [0, 0, 0], "bones":
                                     [[40000, 0, 0, 0]] * GOLDEN_BONE_COUNT}])],
             GOLDEN_BONE_COUNT, "a quat16 lane past s16"),
            ([dict(good[0], event_count=3)], GOLDEN_BONE_COUNT,
             "an event_count naming bytes that are not there"),
        ):
            try:
                build(bad_clips, bad_bones)
            except Refusal:
                fired += 1
            else:
                print("mkclipbank: %s was NOT refused" % why, file=sys.stderr)
                return 1
        try:
            build(good, GOLDEN_BONE_COUNT, rows=1)
        except Refusal:
            fired += 1
        else:
            print("mkclipbank: a bank past the reader's rows was NOT refused",
                  file=sys.stderr)
            return 1

        print("mkclipbank: golden reproduced byte for byte (%d bytes, %d clips, "
              "%d bones, frame stride %d); header and directory read back; "
              "%d refusals fired"
              % (len(got), clips, bones, stride, fired))
        return 0

    if not a.source or not a.out:
        ap.error("source and out are required unless --check or --write-golden")
    doc = json.loads(Path(a.source).read_text(encoding="utf-8"))
    if not isinstance(doc, dict) or "clips" not in doc or "bone_count" not in doc:
        print("mkclipbank: %s is not {\"bone_count\": n, \"clips\": [...]}" % a.source,
              file=sys.stderr)
        return 2
    try:
        page = build(doc["clips"], doc["bone_count"])
    except Refusal as e:
        print("mkclipbank: REFUSED -- %s" % e, file=sys.stderr)
        return 1
    Path(a.out).write_bytes(page)
    print("mkclipbank: %s, %d clips, %d bytes" % (a.out, len(doc["clips"]), len(page)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
