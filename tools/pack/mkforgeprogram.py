#!/usr/bin/env python3
"""mkforgeprogram.py -- the FORGE_PROGRAM page (kind 14), emitted.

Owner decision R234 D2 (reports/OWNER-RULINGS-20260919-EVENING.md, 2026-09-21,
`(owner, explicit)`): "R199 IS REVERSED by this decision ... The page kind is to
be frozen and FORGE.PRIM / FORGE.PRIM_EVAL built."  `spec/cartridge.md` 4d is
that freeze; this file emits it.

THE LAYOUT IS NOT DEFINED HERE
------------------------------
`reference/include/zref/zref_forge_page.hpp` is the layout and `spec/cartridge.md`
4d states it normatively. This file is a THIRD statement of the same offsets,
which is one more than anybody wants -- so `--check` exists: it rebuilds the
committed golden `tests/golden/forge_program/forge_page_v1.bin` and compares byte
for byte, and `tests/forge/forge_page_directed.cpp` requires
`zref::forge_page::build` to reproduce that SAME file. Packer, model and the
future hardware reader are then pinned to one artefact rather than to each
other's good intentions, and a layout edit that misses one of the three goes red.

That discipline is `tools/pack/mkcreatureladder.py`'s, deliberately copied
rather than re-invented.

THE FAMILY BYTE IS THE SILICON ENCODING, NOT `forge_kind`
---------------------------------------------------------
`spec/commands.zidl` says in capitals that `forge_kind` is NOT
`zhao_forge_prim`'s `j_family_i` encoding: `forge_kind = (family + 1) mod 6`,
and "a straight-through assignment is silently wrong for all six values". The
page carries the SILICON family. `--kinds` prints the rotation both ways so an
author checking a draw against a page does not compute it by hand.

INPUT
-----
A JSON array of objects. Only `program_index` and `family` are required; every
other field defaults to the neutral value its legality rule permits.

    {"program_index": 256, "family": 2, "sweep": 0,
     "segments": 64, "sides": 8, "view_mask": 3, "src_id": 7,
     "anchor0": [0, 0, 0], "anchor1": [0, 655360, 0],
     "axis_u": [65536, 0, 0], "axis_v": [0, 0, 65536],
     "radius0": 32768, "radius1": 16384}

Every value is an integer; the vectors and radii are fx16 (Q16.16) world metres.
Refusals are deterministic and name the record and the rule -- the taxonomy is
`zref::forge_page::Illegality`, in the order a reader applies it.

Usage:
    mkforgeprogram.py programs.json out.bin
    mkforgeprogram.py --check
    mkforgeprogram.py --kinds
"""
import argparse
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
GOLDEN = REPO / "tests" / "golden" / "forge_program" / "forge_page_v1.bin"

# ---- the frozen constants (spec/cartridge.md 4d) ---------------------------
MAGIC = 0x4746505A  # 'Z','F','P','G' little-endian
VERSION = 1
HEADER_BYTES = 64
RECORD_BYTES = 192  # THREE MEM.GUARD lines
PROGRAM_INDEX_MASK = 0x00FFFFFF

FAM_RIBBON, FAM_FAN, FAM_TUBE, FAM_SHELL, FAM_BILLBOARD, FAM_CLIFF = range(6)
FAMILY_COUNT = 6
FAMILY_NAMES = ["ribbon", "radial fan", "tube", "radial shell", "billboard sheet",
                "terrain cliff / skirt"]
# `zhao_forge_prim.sv`'s own localparam names, spelled out rather than derived
# from FAMILY_NAMES -- two of the six would collide ("radial fan" and "radial
# shell" both start "radial"), which is how a printed table quietly lies.
FAM_SYMBOLS = ["FAM_RIBBON", "FAM_FAN", "FAM_TUBE", "FAM_SHELL", "FAM_BILLBOARD", "FAM_CLIFF"]
KIND_NAMES = ["FORGE_HEIGHTFIELD_PATCH", "FORGE_RIBBON", "FORGE_RADIAL_FAN",
              "FORGE_TUBE", "FORGE_RADIAL_SHELL", "FORGE_BILLBOARD_SHEET"]

SWEEP_LINEAR, SWEEP_DOME = 0, 1
SWEEP_COUNT = 2

MAX_SEGMENTS = 64
MAX_SIDES = 8
MAX_RIBBON_SEGMENTS = 24
MAX_BRANCHES = 2
MAX_BRANCH_SEGMENTS = 8

# Record offsets, frozen. Line 0.
OFF_PROGRAM_INDEX = 0
OFF_FAMILY = 4
OFF_SWEEP = 5
OFF_SEGMENTS = 6
OFF_SIDES = 7
OFF_VIEW_MASK = 8
OFF_BRANCH_COUNT = 9
OFF_SRC_ID = 10
OFF_ANCHOR0 = 16
OFF_ANCHOR1 = 28
OFF_AXIS_U = 40
OFF_AXIS_V = 52
# Line 1.
OFF_RADIUS0 = 64
OFF_RADIUS1 = 68
OFF_AMP = 72
OFF_BRANCH_AMP = 76
OFF_BRANCH_RADIUS = 80
OFF_SEED = 84
OFF_TICK_PHASE_BASE = 88
OFF_AXIS_W = 92
# Line 2.
OFF_BR0_ATTACH = 128
OFF_BR0_SEGMENTS = 129
OFF_BR0_END = 132
OFF_BR1_ATTACH = 144
OFF_BR1_SEGMENTS = 145
OFF_BR1_END = 148


def kind_of_family(family):
    """`forge_kind` for a silicon family. THE ONE rotation, mirrored from
    `zref::forge_page::kind_of_family`."""
    if not 0 <= family < FAMILY_COUNT:
        return None
    return (family + 1) % FAMILY_COUNT


def family_of_kind(kind):
    if not 0 <= kind < FAMILY_COUNT:
        return None
    return (kind + FAMILY_COUNT - 1) % FAMILY_COUNT


def family_is_ribbon(family):
    return family == FAM_RIBBON


def family_ring_closed(family):
    """`zhao_forge_prim`'s `closed_c`, restated."""
    return family in (FAM_TUBE, FAM_SHELL, FAM_FAN)


def record_legal(r):
    """Return None when legal, else the rule that refused it. The ORDER is the
    order a reader applies it, so hardware, model and packer refuse for the same
    reason and not merely at the same time."""
    if r["program_index"] & ~PROGRAM_INDEX_MASK:
        return "program_index has a nonzero high byte (a handle32's low byte is its GENERATION)"
    if not 0 <= r["family"] < FAMILY_COUNT:
        return "family outside 0..5"
    if not 0 <= r["sweep"] < SWEEP_COUNT:
        return "sweep outside 0..1"
    if r["sweep"] == SWEEP_DOME and r["family"] != FAM_SHELL:
        return "DOME sweep off FAM_SHELL (it would be a second, silent family selector)"
    cap = MAX_RIBBON_SEGMENTS if family_is_ribbon(r["family"]) else MAX_SEGMENTS
    if not 1 <= r["segments"] <= cap:
        return "segments outside 1..%d for this family" % cap
    if not 1 <= r["sides"] <= MAX_SIDES:
        return "sides outside 1..%d" % MAX_SIDES
    if not family_ring_closed(r["family"]) and r["sides"] != 1:
        return "sides != 1 on an OPEN family (its ring is the width-axis pair)"
    if r["view_mask"] == 0 or (r["view_mask"] & 0xFC):
        return "view_mask zero, or a bit above bit 1 set"
    if r["radius0"] < 0 or r["radius1"] < 0:
        return "a negative radius"
    if family_is_ribbon(r["family"]) and r["radius0"] != r["radius1"]:
        return ("radius0 != radius1 on FAM_RIBBON -- zhao_forge_prim_eval carries ONE "
                "half_width, so a tapering ribbon is a capability it does not have")
    if not 0 <= r["branch_count"] <= MAX_BRANCHES:
        return "branch_count outside 0..%d" % MAX_BRANCHES
    for b in range(r["branch_count"]):
        br = r["branches"][b]
        if not 1 <= br["segments"] <= MAX_BRANCH_SEGMENTS:
            return "branch %d segments outside 1..%d" % (b, MAX_BRANCH_SEGMENTS)
        if br["attach"] > r["segments"]:
            return "branch %d attach past the main polyline" % b
    if not family_is_ribbon(r["family"]):
        if (r["branch_count"] or r["amp"] or r["branch_amp"] or r["branch_radius"]
                or r["seed"] or r["tick_phase_base"] or any(r["axis_w"])):
            return "a ribbon-only field is set on a family with no ribbon law"
        for b in range(MAX_BRANCHES):
            if r["branches"][b]["attach"] or r["branches"][b]["segments"]:
                return "a branch descriptor is set on a family with no ribbon law"
    return None


def _vec(d, key, default=(0, 0, 0)):
    v = d.get(key, list(default))
    if not isinstance(v, list) or len(v) != 3 or not all(isinstance(c, int) for c in v):
        raise SystemExit("mkforgeprogram: `%s` must be three integers" % key)
    return v


def normalise(obj, where):
    """JSON object -> the full record dict, defaults applied."""
    if "program_index" not in obj or "family" not in obj:
        raise SystemExit("mkforgeprogram: %s needs `program_index` and `family`" % where)
    r = {
        "program_index": int(obj["program_index"]),
        "family": int(obj["family"]),
        "sweep": int(obj.get("sweep", SWEEP_LINEAR)),
        "segments": int(obj.get("segments", 1)),
        "sides": int(obj.get("sides", 1)),
        "view_mask": int(obj.get("view_mask", 3)),
        "branch_count": int(obj.get("branch_count", 0)),
        "src_id": int(obj.get("src_id", 0)),
        "anchor0": _vec(obj, "anchor0"),
        "anchor1": _vec(obj, "anchor1"),
        "axis_u": _vec(obj, "axis_u"),
        "axis_v": _vec(obj, "axis_v"),
        "radius0": int(obj.get("radius0", 0)),
        "radius1": int(obj.get("radius1", 0)),
        "amp": int(obj.get("amp", 0)),
        "branch_amp": int(obj.get("branch_amp", 0)),
        "branch_radius": int(obj.get("branch_radius", 0)),
        "seed": int(obj.get("seed", 0)),
        "tick_phase_base": int(obj.get("tick_phase_base", 0)),
        "axis_w": _vec(obj, "axis_w"),
        "branches": [],
    }
    raw = obj.get("branches", [])
    for b in range(MAX_BRANCHES):
        src = raw[b] if b < len(raw) else {}
        r["branches"].append({
            "attach": int(src.get("attach", 0)),
            "segments": int(src.get("segments", 0)),
            "end": _vec(src, "end"),
        })
    return r


def _put_vec(buf, off, v):
    struct.pack_into("<iii", buf, off, v[0], v[1], v[2])


def build(records):
    """Serialise a page. Mirrors `zref::forge_page::build` byte for byte."""
    seen = {}
    for i, r in enumerate(records):
        why = record_legal(r)
        if why is not None:
            raise SystemExit("mkforgeprogram: record %d (program_index %d) refused: %s"
                             % (i, r["program_index"], why))
        if r["program_index"] in seen:
            raise SystemExit(
                "mkforgeprogram: record %d duplicates program_index %d (record %d) -- the bank "
                "answers the FIRST match, so a page with two rows for one program is a page "
                "whose meaning depends on row order"
                % (i, r["program_index"], seen[r["program_index"]]))
        seen[r["program_index"]] = i
    if len(records) > 0xFFFF:
        raise SystemExit("mkforgeprogram: more than 65535 records")

    buf = bytearray(HEADER_BYTES + RECORD_BYTES * len(records))
    struct.pack_into("<IHH", buf, 0, MAGIC, VERSION, len(records))
    for i, r in enumerate(records):
        o = HEADER_BYTES + RECORD_BYTES * i
        struct.pack_into("<I", buf, o + OFF_PROGRAM_INDEX, r["program_index"])
        struct.pack_into("<BBBB", buf, o + OFF_FAMILY,
                         r["family"], r["sweep"], r["segments"], r["sides"])
        struct.pack_into("<BB", buf, o + OFF_VIEW_MASK, r["view_mask"], r["branch_count"])
        struct.pack_into("<H", buf, o + OFF_SRC_ID, r["src_id"])
        _put_vec(buf, o + OFF_ANCHOR0, r["anchor0"])
        _put_vec(buf, o + OFF_ANCHOR1, r["anchor1"])
        _put_vec(buf, o + OFF_AXIS_U, r["axis_u"])
        _put_vec(buf, o + OFF_AXIS_V, r["axis_v"])

        struct.pack_into("<iiiii", buf, o + OFF_RADIUS0, r["radius0"], r["radius1"],
                         r["amp"], r["branch_amp"], r["branch_radius"])
        struct.pack_into("<IH", buf, o + OFF_SEED, r["seed"], r["tick_phase_base"])
        _put_vec(buf, o + OFF_AXIS_W, r["axis_w"])

        struct.pack_into("<BB", buf, o + OFF_BR0_ATTACH,
                         r["branches"][0]["attach"], r["branches"][0]["segments"])
        _put_vec(buf, o + OFF_BR0_END, r["branches"][0]["end"])
        struct.pack_into("<BB", buf, o + OFF_BR1_ATTACH,
                         r["branches"][1]["attach"], r["branches"][1]["segments"])
        _put_vec(buf, o + OFF_BR1_END, r["branches"][1]["end"])
    return bytes(buf)


# ---------------------------------------------------------------------------
# THE GOLDEN's OWN CONTENT, here rather than in a data file so that `--check`
# depends on this file and the layout and on nothing else. One record per
# FAMILY, both SWEEPS, the contract's worst case, and a bolt with two branches.
# ---------------------------------------------------------------------------
ONE = 65536  # 1.0 in fx16


def golden_records():
    def rec(**kw):
        return normalise(kw, "golden")

    return [
        # A lightning bolt: every ribbon field live, both branches, max segments.
        rec(program_index=0x000100, family=FAM_RIBBON, segments=MAX_RIBBON_SEGMENTS,
            sides=1, view_mask=3, src_id=0x0101,
            anchor0=[0, 0, 0], anchor1=[0, 8 * ONE, 0],
            axis_u=[ONE, 0, 0], axis_v=[0, 0, ONE], axis_w=[0, ONE, 0],
            radius0=ONE // 8, radius1=ONE // 8,
            amp=ONE // 2, branch_amp=ONE // 4, branch_radius=ONE // 16,
            seed=0x1234ABCD, tick_phase_base=0x0555, branch_count=2,
            branches=[{"attach": 8, "segments": 6, "end": [3 * ONE, 5 * ONE, -ONE]},
                      {"attach": 17, "segments": 8, "end": [-2 * ONE, 7 * ONE, 2 * ONE]}]),
        # A ring: one segment, eight sides, hub at the origin.
        rec(program_index=0x000200, family=FAM_FAN, segments=1, sides=8, view_mask=3,
            src_id=0x0202, anchor0=[0, 0, 0], anchor1=[0, 0, 0],
            axis_u=[ONE, 0, 0], axis_v=[0, 0, ONE], radius0=0, radius1=2 * ONE),
        # The contract's worst case: 64 x 8 = 512 quads = 1,024 triangles.
        rec(program_index=0x000300, family=FAM_TUBE, segments=MAX_SEGMENTS, sides=MAX_SIDES,
            view_mask=3, src_id=0x0303, anchor0=[0, 0, 0], anchor1=[0, 16 * ONE, 0],
            axis_u=[ONE, 0, 0], axis_v=[0, 0, ONE], radius0=ONE, radius1=ONE // 4),
        # The DOME sweep, legal here and nowhere else.
        rec(program_index=0x000400, family=FAM_SHELL, sweep=SWEEP_DOME, segments=16, sides=8,
            view_mask=3, src_id=0x0404, anchor0=[0, 0, 0], anchor1=[0, 4 * ONE, 0],
            axis_u=[ONE, 0, 0], axis_v=[0, 0, ONE], radius0=4 * ONE, radius1=4 * ONE),
        # A single quad.
        rec(program_index=0x000500, family=FAM_BILLBOARD, segments=1, sides=1, view_mask=1,
            src_id=0x0505, anchor0=[0, 0, 0], anchor1=[0, 2 * ONE, 0],
            axis_u=[ONE, 0, 0], axis_v=[0, 0, ONE], radius0=ONE, radius1=ONE),
        # A skirt: open, one side, second view only.
        rec(program_index=0x000600, family=FAM_CLIFF, segments=32, sides=1, view_mask=2,
            src_id=0x0606, anchor0=[-8 * ONE, 0, 0], anchor1=[8 * ONE, 0, 0],
            axis_u=[0, ONE, 0], axis_v=[0, 0, ONE], radius0=ONE, radius1=ONE),
    ]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", nargs="?", help="JSON array of program records")
    ap.add_argument("output", nargs="?", help="output .bin page body")
    ap.add_argument("--check", action="store_true",
                    help="rebuild the committed golden and compare byte for byte")
    ap.add_argument("--kinds", action="store_true",
                    help="print the forge_kind <-> FAM_* rotation, both ways")
    args = ap.parse_args(argv)

    if args.kinds:
        print("spec/commands.zidl: forge_kind = (family + 1) mod 6 -- a ROTATION, not an identity")
        print("%-24s %5s   %-24s %s" % ("forge_kind", "value", "FAM_*", "family"))
        for k in range(FAMILY_COUNT):
            f = family_of_kind(k)
            print("%-24s %5d   %-24s %d   (%s)"
                  % (KIND_NAMES[k], k, FAM_SYMBOLS[f], f, FAMILY_NAMES[f]))
        return 0

    if args.check:
        want = GOLDEN.read_bytes()
        got = build(golden_records())
        if got != want:
            n = min(len(got), len(want))
            first = next((i for i in range(n) if got[i] != want[i]), n)
            print("mkforgeprogram --check FAILED: %d bytes built, %d bytes committed, first "
                  "difference at byte %d" % (len(got), len(want), first), file=sys.stderr)
            return 1
        print("mkforgeprogram --check OK: %d bytes, %d records, matches %s"
              % (len(want), (len(want) - HEADER_BYTES) // RECORD_BYTES,
                 GOLDEN.relative_to(REPO).as_posix()))
        return 0

    if not args.input or not args.output:
        ap.error("need INPUT and OUTPUT, or --check, or --kinds")
    obj = json.loads(Path(args.input).read_text(encoding="utf-8"))
    if not isinstance(obj, list):
        raise SystemExit("mkforgeprogram: input must be a JSON array")
    recs = [normalise(o, "record %d" % i) for i, o in enumerate(obj)]
    body = build(recs)
    Path(args.output).write_bytes(body)
    print("mkforgeprogram: %d records, %d bytes -> %s" % (len(recs), len(body), args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
