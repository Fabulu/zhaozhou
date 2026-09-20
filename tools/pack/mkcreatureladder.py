#!/usr/bin/env python3
"""mkcreatureladder.py -- the CREATURE_FORM page's LADDER TABLE, emitted.

Owner ruling R26 (reports/OWNER-RULINGS-20260919-EVENING.md): "Lift the kind-8
freeze for GEOM.LOD's FOUR constants only (bound radius, micro/splat/glint
error). Their layout is frozen now and THE PACKER EMITS THEM." R68 schedules
that as its own sub-build. This file is it.

WHAT IT REFUSES, WHICH IS THE POINT
-----------------------------------
`spec/cartridge.md` 4 still says of kind 8: "until then the packer refuses to
emit them (deterministic refusal, never a guessed layout)". R26 lifted that
sentence for four fields, so this packer emits a page carrying a HEADER and a
LADDER TABLE and nothing else, and sets `body_off = 0` -- "there is no
Phase-12 body in this page". It does not write parts, meshlet ids, bones,
attachments or hitboxes, and it will not be extended to do so before
SW.TOOLS.ASSET freezes them. A `--body-off` argument exists so that a later
packer which DOES write a body can hand this one the offset; passing it
without a body is refused.

THE LAYOUT IS NOT DEFINED HERE
------------------------------
`reference/include/zref/zref_creature_page.hpp` is the layout, and
`fpga/rtl/geometry/zhao_geom_ladderbank.sv` is its reader. This file is a
THIRD statement of the same offsets, which is one more than anybody wants --
so `--check` exists: it rebuilds the committed golden
`tests/golden/creature_ladder/ladder_page_v1.bin` and compares byte for byte,
and `tests/geometry/geom_ladderbank_directed.cpp` requires
`zref::creature_page::build` to reproduce that SAME file. Packer, model and
RTL are then pinned to one artefact rather than to each other's good
intentions, and a layout edit that misses one of the three goes red.

INPUT
-----
A JSON array of objects, each:

    {"form_index": 256, "bound_radius": 65536,
     "micro_error": 1024, "splat_error": 32768, "glint_error": 65536}

Every value is an integer; the four constants are fx16 (Q16.16) world metres.
Refusals are deterministic and name the record:
  * `form_index` outside 0..0x00FFFFFF (a handle32's low byte is its
    GENERATION and is not part of the key);
  * a duplicate `form_index` (the bank answers the FIRST match, so a page with
    two rows for one creature is a page whose meaning depends on row order);
  * `bound_radius <= 0` (zref::creature::lod_raw divides by it);
  * a negative error (the ladder's divide-free identities hold only for a
    non-negative numerator).

Usage:
    mkcreatureladder.py ladder.json out.bin
    mkcreatureladder.py --check
"""
import argparse
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
GOLDEN = REPO / "tests" / "golden" / "creature_ladder" / "ladder_page_v1.bin"
# The SECOND golden, added with R90's body lift. The first is NOT regenerated
# -- that is the whole proof that the append is an append.
GOLDEN_BODY = REPO / "tests" / "golden" / "creature_ladder" / "ladder_page_body_v1.bin"

# zref::creature_page, restated. See the module docstring on why a third
# statement exists and what pins it.
MAGIC = 0x4D46435A  # 'Z','C','F','M' little-endian
VERSION = 1
HEADER_BYTES = 64
RECORD_BYTES = 32
FORM_INDEX_MASK = 0x00FFFFFF

FIELDS = ("form_index", "bound_radius", "micro_error", "splat_error", "glint_error")

# ---------------------------------------------------------------------------
# THE BODY SECTION -- owner ruling R90's SECOND PARTIAL LIFT, emitted.
#
# R90 grants exactly R26's shape for the BONE HIERARCHY, and the mechanism 4c
# built for it is `body_off`: "the byte offset at which that body will begin,
# so the unfrozen half can be appended later without moving a byte of the
# frozen half". This is the first packer to write a non-zero one.
#
# THE APPEND IS PROVABLY APPEND-ONLY, and that is what makes this freeze cheap.
# `tests/golden/creature_ladder/ladder_page_v1.bin` is NOT regenerated: a page
# with no body still carries `body_off == 0` and is byte-for-byte what it was.
# The body arrives as a SECOND golden, `ladder_page_body_v1.bin`, and `--check`
# requires both. So the ABI fare owner ruling R108 attaches to a layout change
# -- all five Duo golden captures regenerated, ~600 frames and about an hour --
# IS NOT PAYABLE HERE, and the reason is structural rather than lucky:
# `spec/commands.zidl` carries no creature-page layout at all (searched), so
# this is not a zidl edit and no capture depends on it.
#
# THE BODY HEADER, 64 bytes at `body_off`. Little-endian, the page's own
# convention; 64 so the first bone record is 32-byte aligned and so the clip
# bank's descriptor has somewhere declared to go.
#
#   +0   u32  BODY_MAGIC 'ZCB8'
#   +4   u16  BODY_VERSION = 1
#   +6   u8   bone_count       1..32 (creature_rules 1.2 ceiling)
#   +7   u8   body_flags       bit0 RIGID_REST, must be 1 in v1
#   +8   u32  bones_off        bytes from body_off to bone 0; 64 in v1
#   +12  u32  reserved         0
#   +16..63   reserved         0 -- where the kind-9 clip-bank descriptor goes
#
# THE BONE RECORD, 32 bytes, and it is `fpga/rtl/geometry/zhao_geom_bonesrc.sv`'s
# record field for field:
#
#   +0   u8   parent           parent-before-child REQUIRED; bone 0 carries 0
#   +1   u8   flags            bit0 RIGID_REST, must be 1 in v1
#   +2   u16  reserved         0
#   +4   s32  rest_tx          LOCAL rest translation, fx16 (Q16.16)
#   +8   s32  rest_ty
#   +12  s32  rest_tz
#   +16  s32  inv_rest_tx      = -world_rest_x, BAKED HERE
#   +20  s32  inv_rest_ty
#   +24  s32  inv_rest_tz
#   +28  u32  reserved         0
#
# WHY inv_rest IS THREE NUMBERS AND NOT A 3x4 MATRIX. `zref::creature::Bone`
# is `{uint8_t parent; int32_t tx, ty, tz;}` and carries no rest rotation at
# all; `bake_skeleton` therefore builds every inverse as identity rotation with
# m[3]/m[7]/m[11] = -world_rest[b], and `zref_creature.hpp` states the reason as
# a bind convention -- "rings are authored in rest orientation, so B_rest is a
# pure translation chain and its inverse is EXACT (translate(-world_rest_pos),
# zero rounding)". Nine of the twelve elements are the constants 65536 and 0.
# Storing them would be storing nine constants per bone, and the RTL rebuilds
# them for nothing. `flags` bit0 declares the invariant per record so a future
# skeleton that breaks it is REFUSED here and COUNTED there
# (`bone_rest_nonrigid_o`), never decoded wrongly in silence.
#
# WHY THE PACKER BAKES `world_rest` AND THE RTL DOES NOT. It is a running sum
# down the parent chain, the reference does it once at load in `bake_skeleton`,
# and a second implementation of it inside a combinational source block would
# be exactly the hidden adapter `zhao_console_core` forbids. This file is the
# third statement of that sum (with the reference and the golden), which is the
# same three-way pin the ladder table already lives under -- see the module
# docstring.
BODY_MAGIC = 0x38424354  # 'T','C','B','8' little-endian -- 'ZCB8' minus the Z
BODY_VERSION = 1
BODY_HEADER_BYTES = 64
BONE_BYTES = 32
MAX_BONES = 32
FLAG_RIGID_REST = 0x01

BONE_FIELDS = ("parent", "tx", "ty", "tz")


class Refusal(Exception):
    """A deterministic refusal, never a guessed layout."""


def _i32(v, where):
    if not isinstance(v, int) or isinstance(v, bool):
        raise Refusal("%s: %r is not an integer" % (where, v))
    if v < -(1 << 31) or v > (1 << 31) - 1:
        raise Refusal("%s: %d does not fit an i32" % (where, v))
    return v


def check_records(records):
    """Every legality `zref::creature_page::record_legal` enforces, plus the
    duplicate-key rule the RTL cannot see (it answers the first match)."""
    seen = {}
    for i, r in enumerate(records):
        where = "record %d" % i
        missing = [f for f in FIELDS if f not in r]
        if missing:
            raise Refusal("%s: missing %s" % (where, ", ".join(missing)))
        extra = [k for k in r if k not in FIELDS]
        if extra:
            raise Refusal("%s: unknown field(s) %s" % (where, ", ".join(sorted(extra))))
        form = _i32(r["form_index"], where + " form_index")
        if form < 0 or form > FORM_INDEX_MASK:
            raise Refusal(
                "%s: form_index 0x%X is outside 0..0x%06X -- a handle32's low "
                "byte is its GENERATION and is not part of the key"
                % (where, form, FORM_INDEX_MASK)
            )
        if form in seen:
            raise Refusal(
                "%s: form_index 0x%06X is already record %d -- the bank answers "
                "the FIRST match, so two rows for one creature is a page whose "
                "meaning depends on row order" % (where, form, seen[form])
            )
        seen[form] = i
        bound = _i32(r["bound_radius"], where + " bound_radius")
        if bound <= 0:
            raise Refusal(
                "%s: bound_radius is %d -- zref::creature::lod_raw divides by it"
                % (where, bound)
            )
        for f in ("micro_error", "splat_error", "glint_error"):
            e = _i32(r[f], where + " " + f)
            if e < 0:
                raise Refusal(
                    "%s: %s is %d -- the ladder's divide-free identities hold "
                    "only for a non-negative numerator" % (where, f, e)
                )
    if len(records) > 0xFFFF:
        raise Refusal("%d records; the header's count is a u16" % len(records))


def check_bones(bones):
    """Every legality `bake_skeleton` enforces, refused deterministically here
    rather than baked into a page the RTL would decode into a wrong palette."""
    if not isinstance(bones, list):
        raise Refusal("the bone list is not a JSON array")
    if len(bones) == 0:
        raise Refusal(
            "a body with zero bones -- emit no body at all instead, which is "
            "what body_off == 0 means"
        )
    if len(bones) > MAX_BONES:
        raise Refusal(
            "%d bones; creature_rules 1.2 caps a creature at %d"
            % (len(bones), MAX_BONES)
        )
    for b, bone in enumerate(bones):
        where = "bone %d" % b
        missing = [f for f in BONE_FIELDS if f not in bone]
        if missing:
            raise Refusal("%s: missing %s" % (where, ", ".join(missing)))
        extra = [k for k in bone if k not in BONE_FIELDS]
        if extra:
            raise Refusal("%s: unknown field(s) %s" % (where, ", ".join(sorted(extra))))
        parent = _i32(bone["parent"], where + " parent")
        if parent < 0 or parent > MAX_BONES - 1:
            raise Refusal("%s: parent %d is outside 0..%d" % (where, parent, MAX_BONES - 1))
        if b == 0 and parent != 0:
            raise Refusal(
                "%s: the root's parent is %d -- bake_skeleton requires 0" % (where, parent)
            )
        if parent > b:
            raise Refusal(
                "%s: parent %d comes AFTER it -- parent-before-child is required "
                "(creature_rules 1.2), and zhao_geom_pose_decode reads A_parent "
                "out of its ancestor store without re-checking the order, so a "
                "child decoded first reads a stale matrix rather than hanging"
                % (where, parent)
            )
        for f in ("tx", "ty", "tz"):
            _i32(bone[f], where + " " + f)


def bake_bones(bones):
    """`zref::creature::bake_skeleton`, restated: the world-rest running sum
    down the parent chain, and the exact inverse that falls out of it. Returns
    the list of (parent, tx, ty, tz, inv_tx, inv_ty, inv_tz)."""
    check_bones(bones)
    wx, wy, wz, out = [], [], [], []
    for b, bone in enumerate(bones):
        px = 0 if b == 0 else wx[bone["parent"]]
        py = 0 if b == 0 else wy[bone["parent"]]
        pz = 0 if b == 0 else wz[bone["parent"]]
        x, y, z = px + bone["tx"], py + bone["ty"], pz + bone["tz"]
        for v, n in ((x, "world_x"), (y, "world_y"), (z, "world_z")):
            if v < -(1 << 31) or v > (1 << 31) - 1:
                raise Refusal(
                    "bone %d: %s overflows an i32 at %d -- the rest chain is "
                    "longer than fx16 can name" % (b, n, v)
                )
        wx.append(x)
        wy.append(y)
        wz.append(z)
        out.append((bone["parent"], bone["tx"], bone["ty"], bone["tz"], -x, -y, -z))
    return out


def build_body(bones):
    """The BODY section bytes: a 64-byte header then one 32-byte record per
    bone. Padded to 64, as every uploaded run must be."""
    baked = bake_bones(bones)
    n = BODY_HEADER_BYTES + BONE_BYTES * len(baked)
    if n % 64:
        n += 64 - (n % 64)
    body = bytearray(n)
    struct.pack_into(
        "<IHBBII", body, 0,
        BODY_MAGIC, BODY_VERSION, len(baked), FLAG_RIGID_REST, BODY_HEADER_BYTES, 0,
    )
    for i, (parent, tx, ty, tz, ix, iy, iz) in enumerate(baked):
        o = BODY_HEADER_BYTES + BONE_BYTES * i
        struct.pack_into(
            "<BBHiiiiiiI", body, o,
            parent, FLAG_RIGID_REST, 0, tx, ty, tz, ix, iy, iz, 0,
        )
    return bytes(body)


def build(records, body_off=0, bones=None):
    """The page bytes. Padded to a multiple of 64, which is what MEM.UPLOAD's
    length rule requires of every upload.

    `bones` appends a BODY SECTION and computes `body_off` itself; passing both
    is refused, because an offset naming bytes the packer did not place is the
    guessed layout spec/cartridge.md 4 exists to forbid."""
    check_records(records)
    if bones is not None and body_off:
        raise Refusal(
            "both --body-off and a bone list were given -- this packer computes "
            "the offset of the body it writes, and an offset supplied beside it "
            "can only disagree"
        )
    n = HEADER_BYTES + RECORD_BYTES * len(records)
    if n % 64:
        n += 64 - (n % 64)
    body = b""
    if bones is not None:
        body = build_body(bones)
        body_off = n
    page = bytearray(n)
    struct.pack_into("<IHHI", page, 0, MAGIC, VERSION, len(records), body_off)
    for i, r in enumerate(records):
        o = HEADER_BYTES + RECORD_BYTES * i
        struct.pack_into(
            "<Iiiii",
            page,
            o,
            r["form_index"] & FORM_INDEX_MASK,
            r["bound_radius"],
            r["micro_error"],
            r["splat_error"],
            r["glint_error"],
        )
    return bytes(page) + body


# The golden's contents, here rather than in a data file, so that `--check`
# regenerates from SOURCE and cannot pass by reading the thing it is checking.
# Three creatures at the shapes `compile_creature` produces
# (reference/src/zcreature/creature_core.cpp:569-570): splat = bound/2,
# glint = bound, micro measured. Values are fx16: 65536 == 1.000 m.
GOLDEN_RECORDS = [
    # 0.75 m -- a small flier
    {
        "form_index": 0x000100,
        "bound_radius": 49152,
        "micro_error": 1024,
        "splat_error": 24576,
        "glint_error": 49152,
    },
    # 1.00 m -- the ordinary creature
    {
        "form_index": 0x000101,
        "bound_radius": 65536,
        "micro_error": 1311,
        "splat_error": 32768,
        "glint_error": 65536,
    },
    # 2.50 m -- a hero
    {
        "form_index": 0x00A017,
        "bound_radius": 163840,
        "micro_error": 4096,
        "splat_error": 81920,
        "glint_error": 163840,
    },
]


# The body golden's skeleton. SIX bones in a deliberate shape rather than a
# chain: bone 0 root, 1 and 4 both children of 0 (a BRANCH, so a sibling cannot
# be mistaken for a parent), 2 a child of 1, 3 a child of 2 (depth 3, so the
# world-rest running sum accumulates more than once), and 5 a child of 4 with a
# NEGATIVE local translation (so a sign error in the bake shows). Values are
# fx16: 65536 == 1.000 m.
#
# The resulting world_rest, which is what inv_rest negates, is therefore
# bone 2 = (0.25, 1.50, 0) and bone 3 = (0.25, 2.25, 0) -- two different sums
# down one chain, which a bake that forgot the accumulation would get wrong.
GOLDEN_BONES = [
    {"parent": 0, "tx": 0, "ty": 0, "tz": 0},               # 0 root
    {"parent": 0, "tx": 0, "ty": 65536, "tz": 0},           # 1 spine   (0, 1.00, 0)
    {"parent": 1, "tx": 16384, "ty": 32768, "tz": 0},       # 2 arm     (0.25, 1.50, 0)
    {"parent": 2, "tx": 0, "ty": 49152, "tz": 0},           # 3 hand    (0.25, 2.25, 0)
    {"parent": 0, "tx": -32768, "ty": 0, "tz": 16384},      # 4 leg.l   (-0.50, 0, 0.25)
    {"parent": 4, "tx": 0, "ty": -65536, "tz": -8192},      # 5 foot.l  (-0.50, -1.00, 0.125)
]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("source", nargs="?", help="a JSON array of ladder records")
    ap.add_argument("out", nargs="?", help="the page to write")
    ap.add_argument(
        "--body-off",
        type=int,
        default=0,
        help="byte offset of a body written by some OTHER packer. Refused "
        "beside --bones, which computes its own.",
    )
    ap.add_argument(
        "--bones",
        help="a JSON array of {parent, tx, ty, tz}. Appends the BODY SECTION "
        "and emits a non-zero body_off (owner ruling R90's second partial "
        "lift of the kind-8 freeze).",
    )
    ap.add_argument(
        "--check",
        action="store_true",
        help="rebuild the committed golden from source and compare byte for byte",
    )
    ap.add_argument(
        "--write-golden",
        action="store_true",
        help="(re)write the committed golden. Use when the FROZEN LAYOUT moves, "
        "which needs a ruling, and never to make --check pass.",
    )
    a = ap.parse_args(argv)

    if a.body_off and not a.bones:
        print(
            "mkcreatureladder: --body-off %d refused -- no bone list was given, "
            "so this packer writes no body, and an offset naming bytes that are "
            "not there is exactly the guessed layout spec/cartridge.md 4 forbids"
            % a.body_off,
            file=sys.stderr,
        )
        return 2

    if a.write_golden:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_bytes(build(GOLDEN_RECORDS))
        GOLDEN_BODY.write_bytes(build(GOLDEN_RECORDS, bones=GOLDEN_BONES))
        print(
            "mkcreatureladder: wrote %s (%d bytes) and %s (%d bytes)"
            % (GOLDEN, GOLDEN.stat().st_size, GOLDEN_BODY, GOLDEN_BODY.stat().st_size)
        )
        return 0

    if a.check:
        if not GOLDEN.exists():
            print("mkcreatureladder: %s is missing" % GOLDEN, file=sys.stderr)
            return 1
        want = GOLDEN.read_bytes()
        got = build(GOLDEN_RECORDS)
        if got != want:
            print(
                "mkcreatureladder: the packer no longer reproduces %s (%d bytes "
                "built, %d on disk) -- the frozen layout moved in ONE of the "
                "three places that state it (this file, "
                "reference/include/zref/zref_creature_page.hpp, "
                "fpga/rtl/geometry/zhao_geom_ladderbank.sv)" % (GOLDEN, len(got), len(want)),
                file=sys.stderr,
            )
            for i in range(min(len(got), len(want))):
                if got[i] != want[i]:
                    print("  first difference at byte %d: %02X vs %02X" % (i, got[i], want[i]),
                          file=sys.stderr)
                    break
            return 1
        # THE BODY GOLDEN, and the append proof. `got` above is the bodyless
        # page; if the body golden does not START with those exact bytes then
        # the append moved something in the frozen half, which is the one thing
        # `body_off` exists to make impossible.
        if not GOLDEN_BODY.exists():
            print("mkcreatureladder: %s is missing" % GOLDEN_BODY, file=sys.stderr)
            return 1
        want_body = GOLDEN_BODY.read_bytes()
        got_body = build(GOLDEN_RECORDS, bones=GOLDEN_BONES)
        if got_body != want_body:
            print(
                "mkcreatureladder: the packer no longer reproduces %s (%d bytes "
                "built, %d on disk)" % (GOLDEN_BODY, len(got_body), len(want_body)),
                file=sys.stderr,
            )
            for i in range(min(len(got_body), len(want_body))):
                if got_body[i] != want_body[i]:
                    print("  first difference at byte %d: %02X vs %02X"
                          % (i, got_body[i], want_body[i]), file=sys.stderr)
                    break
            return 1
        head_len = len(got)
        # The frozen half, byte for byte, except the header's own `body_off`
        # word -- which is the FIELD 4c added for exactly this and the only
        # thing an append is allowed to move.
        if want_body[:8] != want[:8] or want_body[12:head_len] != want[12:head_len]:
            print(
                "mkcreatureladder: the BODY page's frozen half differs from the "
                "bodyless page outside the body_off word -- an append moved a "
                "byte of the frozen half, which is what 4c's mechanism exists "
                "to prevent", file=sys.stderr,
            )
            return 1
        body_off = struct.unpack_from("<I", want_body, 8)[0]
        if body_off != head_len:
            print("mkcreatureladder: body_off is %d, expected %d"
                  % (body_off, head_len), file=sys.stderr)
            return 1
        if struct.unpack_from("<I", want, 8)[0] != 0:
            print("mkcreatureladder: the BODYLESS golden's body_off is not 0",
                  file=sys.stderr)
            return 1

        # A refusal that never fires is a claim. Fire each one here, where a
        # `--check` run is the only thing that reads them.
        fired = 0
        for bad, why in (
            ([dict(GOLDEN_RECORDS[0], bound_radius=0)], "a zero bound radius"),
            ([dict(GOLDEN_RECORDS[0], micro_error=-1)], "a negative error"),
            ([dict(GOLDEN_RECORDS[0], form_index=0x1000000)], "a form_index past 24 bits"),
            ([GOLDEN_RECORDS[0], dict(GOLDEN_RECORDS[0])], "a duplicate form_index"),
        ):
            try:
                build(bad)
            except Refusal:
                fired += 1
            else:
                print("mkcreatureladder: %s was NOT refused" % why, file=sys.stderr)
                return 1

        # The BODY's refusals, the same way. Each one is a legality
        # `bake_skeleton` enforces, and an unfired refusal is a claim about a
        # guard nobody has ever reached.
        for badbones, why in (
            ([], "a zero-bone body"),
            ([{"parent": 0, "tx": 0, "ty": 0, "tz": 0}] * (MAX_BONES + 1),
             "a body past the 32-bone ceiling"),
            ([{"parent": 1, "tx": 0, "ty": 0, "tz": 0},
              {"parent": 0, "tx": 0, "ty": 0, "tz": 0}],
             "a root whose parent is not 0"),
            ([{"parent": 0, "tx": 0, "ty": 0, "tz": 0},
              {"parent": 0, "tx": 0, "ty": 0, "tz": 0},
              {"parent": 3, "tx": 0, "ty": 0, "tz": 0},
              {"parent": 0, "tx": 0, "ty": 0, "tz": 0}],
             "a child whose parent comes after it"),
            ([{"parent": 0, "tx": 0, "ty": 0, "tz": 0},
              {"parent": 0, "tx": 2 ** 31 - 1, "ty": 0, "tz": 0},
              {"parent": 1, "tx": 2 ** 31 - 1, "ty": 0, "tz": 0}],
             "a rest chain that overflows i32"),
            ([{"parent": 0, "tx": 0, "ty": 0}], "a bone missing a field"),
        ):
            try:
                build(GOLDEN_RECORDS, bones=badbones)
            except Refusal:
                fired += 1
            else:
                print("mkcreatureladder: %s was NOT refused" % why, file=sys.stderr)
                return 1
        try:
            build(GOLDEN_RECORDS, body_off=4096, bones=GOLDEN_BONES)
        except Refusal:
            fired += 1
        else:
            print("mkcreatureladder: --body-off beside --bones was NOT refused",
                  file=sys.stderr)
            return 1

        print(
            "mkcreatureladder: golden reproduced byte for byte (%d bytes, %d records); "
            "body golden reproduced (%d bytes, %d bones, body_off=%d); frozen half "
            "byte-identical across the append; %d refusals fired"
            % (len(got), len(GOLDEN_RECORDS), len(got_body), len(GOLDEN_BONES),
               body_off, fired)
        )
        return 0

    if not a.source or not a.out:
        ap.error("source and out are required unless --check or --write-golden")
    records = json.loads(Path(a.source).read_text(encoding="utf-8"))
    if not isinstance(records, list):
        print("mkcreatureladder: %s is not a JSON array" % a.source, file=sys.stderr)
        return 2
    bones = None
    if a.bones:
        bones = json.loads(Path(a.bones).read_text(encoding="utf-8"))
    try:
        page = build(records, body_off=a.body_off, bones=bones)
    except Refusal as e:
        print("mkcreatureladder: REFUSED -- %s" % e, file=sys.stderr)
        return 1
    Path(a.out).write_bytes(page)
    print("mkcreatureladder: %s, %d records, %d bytes" % (a.out, len(records), len(page)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
