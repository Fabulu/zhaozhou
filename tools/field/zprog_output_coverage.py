#!/usr/bin/env python3
"""Which of a .zprog's DECLARED output lanes does its code actually WRITE?

WHY THIS EXISTS. `zhao_field_host` used to call a run successful when ANY lane
of the declared output window was written; owner ruling R101 made a declared
lane that was never written a counted refusal (status 0xF3 ST_PARTIAL). The
question that decides whether that defect was producing wrong values TODAY or
only could is a measurement, not an inference: do the programs this repo
actually ships write every output their profile declares?

This is the probe for it, committed rather than thrown away (CLAUDE.md's
ground-contact rule: "a probe that does this was written once and thrown away,
so its numbers are unreproducible -- commit the probe").

WHAT IT MEASURES, AND WHAT IT DOES NOT. `spec/form/field-ir.md` 1.2 binds an
output to "the register's final value at END", which a legal program may
satisfy with a register no instruction ever writes (an input, or a scratch that
the zeroing law left at 0). The HOST cannot see that: `out_hit_c` watches the
register-file write port, so its notion of "produced" is A WRITE. The gap
between those two definitions is exactly what this tool reports. A lane listed
below as UNWRITTEN is legal IR and is invisible to the host.

It is deliberately CONSERVATIVE about adjacent-register ops (1.3): DOT/LEN/
NORMALIZE/ROT write `dst..dst+k`, and the widths are per-op. Rather than
re-stating that table -- a second implementation of a ratified law, which is
the duplication this repo has shipped twice -- a lane whose register is
`dst + 1` or `dst + 2` of some instruction is reported as MAYBE and counted
apart. A MAYBE is never counted as a finding.

The parse is checked against four facts each file states about itself in its
generated wrapper (magic, instr_count, table bytes, program hash), so a silent
mis-parse cannot read in the flattering direction.

Usage:  python tools/field/zprog_output_coverage.py [paths or globs]
        (default: every .zprog under compiler/tests/generated/)
"""

import glob
import os
import struct
import sys

HEADER_BYTES = 28
IO_LANE_BYTES = 12  # reg u8 | kind u8 | type u8 | name_id u8 | min i32 | max i32
SRC_REF_BYTES = 8  # source_id u32 | line u16 | col u16

# spec/form/field-ir.md 7.1, output-record widths per profile.
PROFILE_OUTPUTS = {
    0: ("earth", 4),
    1: ("warp", 6),
    2: ("flow", 7),
    3: ("formation", 6),
    4: ("stamp", 3),
}


def parse(path):
    with open(path, "rb") as fh:
        b = fh.read()
    if len(b) < HEADER_BYTES or b[0:4] != b"ZFIP":
        raise ValueError("%s: not a .zprog (magic is %r)" % (path, b[0:4]))
    version, = struct.unpack_from("<H", b, 4)
    profile = b[6]
    instr_count, = struct.unpack_from("<H", b, 12)
    io_lane_count = b[15]
    table_bytes, = struct.unpack_from("<H", b, 16)
    map_bytes, = struct.unpack_from("<H", b, 18)
    program_hash, = struct.unpack_from("<I", b, 20)

    # V1: the size law. If this disagrees the offsets below are meaningless, so
    # it is an error rather than a warning -- a probe that keeps going after its
    # own frame check failed reports confident nonsense.
    expect = HEADER_BYTES + 8 * instr_count + table_bytes + map_bytes
    if expect != len(b):
        raise ValueError(
            "%s: size law fails -- 28 + 8*%d + %d + %d = %d against %d bytes"
            % (path, instr_count, table_bytes, map_bytes, expect, len(b)))

    code_off = HEADER_BYTES
    io_off = code_off + 8 * instr_count + table_bytes
    if io_off + IO_LANE_BYTES * io_lane_count > len(b):
        raise ValueError("%s: io map runs off the end" % path)

    words = [struct.unpack_from("<Q", b, code_off + 8 * i)[0] for i in range(instr_count)]
    lanes = []
    for i in range(io_lane_count):
        o = io_off + IO_LANE_BYTES * i
        lanes.append({"reg": b[o], "kind": b[o + 1], "type": b[o + 2], "ord": i})

    return {
        "path": path, "version": version, "profile": profile,
        "instr_count": instr_count, "io_lane_count": io_lane_count,
        "table_bytes": table_bytes, "program_hash": program_hash,
        "words": words, "lanes": lanes,
    }


def main(argv):
    roots = argv[1:]
    if not roots:
        here = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        roots = [os.path.join(here, "compiler", "tests", "generated", "*.zprog")]
    paths = []
    for r in roots:
        paths.extend(sorted(glob.glob(r)) if any(c in r for c in "*?[") else [r])
    if not paths:
        print("no .zprog files matched", file=sys.stderr)
        return 2

    findings = 0
    for path in paths:
        p = parse(path)
        pname, pouts = PROFILE_OUTPUTS.get(p["profile"], ("profile%d" % p["profile"], None))
        # dst is bits 8..13 (1.1). srcA/B/C are not writes.
        dsts = set((w >> 8) & 0x3F for w in p["words"])
        adj = set()
        for d in dsts:
            adj.add(d + 1)
            adj.add(d + 2)
        adj -= dsts

        outs = [l for l in p["lanes"] if l["kind"] == 1]
        ins = [l for l in p["lanes"] if l["kind"] == 0]
        written = [l for l in outs if l["reg"] in dsts]
        maybe = [l for l in outs if l["reg"] not in dsts and l["reg"] in adj]
        unwritten = [l for l in outs if l["reg"] not in dsts and l["reg"] not in adj]

        print("%s" % os.path.basename(path))
        print("  profile %d (%s), hash 0x%08X, %d instrs, %d in / %d out lanes"
              % (p["profile"], pname, p["program_hash"], p["instr_count"], len(ins), len(outs)))
        if pouts is not None and len(outs) != pouts:
            print("  NOTE: 7.1 declares %d outputs for %s; this file declares %d"
                  % (pouts, pname, len(outs)))
        print("  output regs written by an instruction : %s"
              % (sorted(l["reg"] for l in written) or "none"))
        if maybe:
            print("  output regs reachable only as dst+1/dst+2 (NOT a finding, see header): %s"
                  % sorted(l["reg"] for l in maybe))
        if unwritten:
            findings += len(unwritten)
            for l in unwritten:
                print("  UNWRITTEN: output lane %d names R%d, which no instruction writes."
                      % (l["ord"] - len(ins), l["reg"]))
            print("  -> legal IR (1.2 binds the register's VALUE, not a write), and INVISIBLE")
            print("     to zhao_field_host, whose window watches the write port. Such a lane")
            print("     needs an explicit write, or the header's R101 mask must not name it.")
        else:
            print("  every declared output lane is written. R101's mask can name all of them.")

        # THE HOST'S WINDOW IS CONTIGUOUS and the IR's output registers are not
        # required to be. `zhao_field_host` captures [out_base, out_base+
        # OUT_LANES) and nothing else, so a program whose outputs straddle a
        # wider span is either uncapturable or captured with HOLES -- and a hole
        # is precisely a window lane that reads the zero cleared at grant. This
        # is the measurement that says which side of R101 each program lands on.
        if outs:
            regs = sorted(l["reg"] for l in outs)
            span = regs[-1] - regs[0] + 1
            mask = 0
            for r in regs:
                mask |= 1 << (r - regs[0])
            print("  output regs %s -> contiguous window needs %d lanes from R%d"
                  % (regs, span, regs[0]))
            print("     required-output mask at out_base=R%d: 0x%X (%s)"
                  % (regs[0], mask, format(mask, "0%db" % span)))
            for nlanes, who in ((4, "bench default"), (7, "zhao_console_core")):
                if span > nlanes:
                    print("     OUT_LANES=%d (%s): DOES NOT FIT -- %d lanes short, so %d of the"
                          % (nlanes, who, span - nlanes, sum(1 for r in regs if r - regs[0] >= nlanes)))
                    print("        declared outputs fall outside the capture window entirely.")
                elif mask != (1 << nlanes) - 1:
                    holes = [i for i in range(nlanes) if not (mask >> i) & 1]
                    print("     OUT_LANES=%d (%s): fits, with HOLES at window lane(s) %s --"
                          % (nlanes, who, holes))
                    print("        before R101 those read the cleared zero under a SUCCESS status.")
                else:
                    print("     OUT_LANES=%d (%s): exactly full, no holes." % (nlanes, who))
        print("")

    print("%d declared output lane(s) with no write, across %d program(s)"
          % (findings, len(paths)))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
