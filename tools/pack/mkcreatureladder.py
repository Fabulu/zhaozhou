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

# zref::creature_page, restated. See the module docstring on why a third
# statement exists and what pins it.
MAGIC = 0x4D46435A  # 'Z','C','F','M' little-endian
VERSION = 1
HEADER_BYTES = 64
RECORD_BYTES = 32
FORM_INDEX_MASK = 0x00FFFFFF

FIELDS = ("form_index", "bound_radius", "micro_error", "splat_error", "glint_error")


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


def build(records, body_off=0):
    """The page bytes. Padded to a multiple of 64, which is what MEM.UPLOAD's
    length rule requires of every upload."""
    check_records(records)
    n = HEADER_BYTES + RECORD_BYTES * len(records)
    if n % 64:
        n += 64 - (n % 64)
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
    return bytes(page)


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


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("source", nargs="?", help="a JSON array of ladder records")
    ap.add_argument("out", nargs="?", help="the page to write")
    ap.add_argument(
        "--body-off",
        type=int,
        default=0,
        help="byte offset of the Phase-12 body. Refused while this packer "
        "writes no body (spec/cartridge.md 4 kind 8 is still frozen there).",
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

    if a.body_off:
        print(
            "mkcreatureladder: --body-off %d refused -- this packer writes no "
            "Phase-12 body, and an offset naming bytes that are not there is "
            "exactly the guessed layout spec/cartridge.md 4 forbids" % a.body_off,
            file=sys.stderr,
        )
        return 2

    if a.write_golden:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_bytes(build(GOLDEN_RECORDS))
        print("mkcreatureladder: wrote %s (%d bytes)" % (GOLDEN, GOLDEN.stat().st_size))
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
        print(
            "mkcreatureladder: golden reproduced byte for byte (%d bytes, %d records), "
            "%d refusals fired" % (len(got), len(GOLDEN_RECORDS), fired)
        )
        return 0

    if not a.source or not a.out:
        ap.error("source and out are required unless --check or --write-golden")
    records = json.loads(Path(a.source).read_text(encoding="utf-8"))
    if not isinstance(records, list):
        print("mkcreatureladder: %s is not a JSON array" % a.source, file=sys.stderr)
        return 2
    try:
        page = build(records)
    except Refusal as e:
        print("mkcreatureladder: REFUSED -- %s" % e, file=sys.stderr)
        return 1
    Path(a.out).write_bytes(page)
    print("mkcreatureladder: %s, %d records, %d bytes" % (a.out, len(records), len(page)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
