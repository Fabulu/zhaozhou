#!/usr/bin/env python3
"""The mutant table for zhao_cmd_dma.sv (CMD.DMA).

WHY THIS SWEEP EXISTS
---------------------
`reports/SWEEP_COVERAGE_AUDIT.md` lists CMD.DMA among the modules with a test
lane and no mutation sweep, and puts it in the second tier -- after the blocks
declared closed, before the rest -- because it is on the hot path everything
else depends on. Every command the machine executes arrives through here.

WHAT THIS BLOCK IS FOR, WHICH DECIDES WHAT TO MUTATE
----------------------------------------------------
CMD.DMA fetches a command frame from the HPS and decides whether to let it into
the machine. It is a GATE CHAIN, and the chain is the block: magic, ABI
version, reserved flags, four separate length laws, the header CRC, the
resource epoch -- and then, per record in the payload walk, the record length,
the opcode, its size agreement, truncation, and the debug-opcode permission.

Twelve distinct status codes exist because each gate has to be DISTINGUISHABLE:
telling a caller "bad length" when the real fault was a stale epoch sends them
looking in the wrong place. So the mutations attack two things -- gates that
stop refusing, and gates that refuse under the WRONG NAME.

The order matters too, and is itself a law (the "fail-safe order" the random
lane predicts): a frame that is bad in two ways must report the EARLIER fault.
Several mutants below move a gate past its neighbour rather than deleting it.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/command/zhao_cmd_dma.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the header gates -------------------------------------------------
    ("D01 a frame with the wrong MAGIC is accepted",
     "          end else if (hget32(0) != zhao_abi_pkg::ZHAO_FRAME_MAGIC) begin",
     "          end else if (hget32(0) == 32'hFFFF_FFFF) begin"),
    ("D02 a frame from another ABI VERSION is accepted",
     "          end else if (hget16(zhao_abi_pkg::ZHAO_OFF_ABI_VERSION)\n"
     "                       != 16'(zhao_abi_pkg::ZHAO_ABI_VERSION)) begin",
     "          end else if (hget16(zhao_abi_pkg::ZHAO_OFF_ABI_VERSION)\n"
     "                       == 16'hFFFF) begin"),
    ("D03 RESERVED flag bits are ignored",
     "          end else if (|(fl_v & 16'hFFFE)) begin",
     "          end else if (|(fl_v & 16'h0000)) begin"),
    ("D04 a command block that is not a multiple of 16 is accepted",
     "          end else if ((cb_v & 32'd15) != 32'd0) begin",
     "          end else if ((cb_v & 32'd7) != 32'd0) begin"),
    ("D05 the command COUNT may exceed what the bytes can hold",
     "          end else if (cc_v > (cb_v >> 5'd4)) begin",
     "          end else if (cc_v > (cb_v >> 5'd3)) begin"),
    ("D06 a packet longer than its descriptor is accepted",
     "          end else if ((32'd40 + cb_v) > f_len) begin",
     "          end else if ((32'd40 + cb_v) > (f_len + 32'd16)) begin"),
    ("D07 the staging bound is one record too generous",
     "          end else if ((32'd40 + cb_v) > SLOT_BUF_BYTES) begin",
     "          end else if ((32'd40 + cb_v) > (SLOT_BUF_BYTES + 32'd16)) begin"),
    ("D08 the FRAME_SLOT_BYTES law is off by the header",
     "          end else if ((32'd40 + cb_v)\n"
     "                       > (zhao_abi_pkg::FRAME_SLOT_BYTES - 32'd40)) begin",
     "          end else if ((32'd40 + cb_v)\n"
     "                       > zhao_abi_pkg::FRAME_SLOT_BYTES) begin"),
    ("D09 the header CRC gate is compared UNFOLDED, so every frame fails",
     "          end else if ({~crc_hdr_r} != hget32(zhao_abi_pkg::ZHAO_OFF_HEADER_CRC)) begin",
     "          end else if (crc_hdr_r != hget32(zhao_abi_pkg::ZHAO_OFF_HEADER_CRC)) begin"),
    ("D10 a frame carrying a STALE resource epoch is executed",
     "          end else if (hget32(zhao_abi_pkg::ZHAO_OFF_RESOURCE_EPOCH) != f_epoch)\n"
     "          begin",
     "          end else if ((hget32(zhao_abi_pkg::ZHAO_OFF_RESOURCE_EPOCH) >> 1)\n"
     "                       != (f_epoch >> 1))\n"
     "          begin"),

    # ---- the statuses must stay DISTINGUISHABLE ---------------------------
    ("D11 a bad magic is reported as a bad LENGTH",
     "            ok_v = 1'b0; st_v = ST_BAD_MAGIC;",
     "            ok_v = 1'b0; st_v = (cb_v == 32'd0) ? ST_BAD_MAGIC : ST_BAD_LENGTH;"),
    ("D12 a stale epoch is reported as a bad HEADER CRC",
     "            ok_v = 1'b0; st_v = ST_EPOCH;           // drop before payload",
     "            ok_v = 1'b0; st_v = (cb_v == 32'd0) ? ST_EPOCH : ST_BAD_HEADER_CRC;  // drop before payload"),
    ("D13 a reserved flag is reported as a bad ABI VERSION",
     "            ok_v = 1'b0; st_v = ST_RESERVED_FLAG;",
     "            ok_v = 1'b0; st_v = (cb_v == 32'd0) ? ST_RESERVED_FLAG : ST_BAD_ABI_VER;"),

    # ---- the record walk --------------------------------------------------
    ("D14 a record length that is not a multiple of 16 is walked",
     "            if ((rb_v & 16'h000F) != 16'd0 || rb_v < 16'd16) begin",
     "            if (rb_v < 16'd16) begin"),
    ("D15 a zero-length record is walked, so the walk cannot advance",
     "            if ((rb_v & 16'h000F) != 16'd0 || rb_v < 16'd16) begin",
     "            if ((rb_v & 16'h000F) != 16'd0) begin"),
    ("D16 an UNKNOWN opcode is executed",
     "            end else if (rsz_q == 16'd0) begin",
     "            end else if (rsz_q == 16'hFFFF) begin"),
    ("D17 a record whose size disagrees with its opcode is accepted",
     "            end else if (rsz_q != rb_v) begin",
     "            end else if (rsz_q > rb_v) begin"),
    ("D18 a record running past the command block is walked",
     "            end else if ((walk_off + 32'(rb_v)) > cb) begin",
     "            end else if ((walk_off + 32'(rb_v)) > (cb + 32'd16)) begin"),
    # The reshaped mutants here first orphaned a signal or a parameter and so
    # failed the LINTER rather than the tests. A mutant that cannot build is a
    # discard, not evidence. The status swaps stay keyed on a real signal so the
    # status constant is still referenced; D19 INVERTS the permission rather
    # than deleting it.
    ("D19 the debug-opcode permission is INVERTED",
     "                         && !h_debug) begin",
     "                         && h_debug) begin"),

    # ---- the fail-safe that precedes any fetch ----------------------------
    ("D20 a descriptor too short to hold a header is fetched anyway",
     "            if (fetch_byte_len_i < 32'd36) begin",
     "            if (fetch_byte_len_i < 32'd8) begin"),
    ("D21 a new descriptor is accepted while a fetch is running",
     "  assign fetch_req_ready_o = (m == M_IDLE);",
     "  assign fetch_req_ready_o = 1'b1;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
    nl = "\r\n" if "\r\n" in gold else "\n"
    o = old.replace("\n", nl)
    n = new.replace("\n", nl)
    count = gold.count(o)
    if count != 1:
        raise ValueError("anchor matches %d times" % count)
    if o == n:
        raise ValueError("mutant identical to base")
    return gold.replace(o, n, 1)


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {
    "D08":
        "UNREACHABLE BY ARITHMETIC. The FRAME_SLOT_BYTES law fires when "
        "40 + command_bytes exceeds FRAME_SLOT_BYTES - 40 = 1,048,536, so it "
        "needs command_bytes > 1,048,496. But the STAGING bound two lines "
        "above it fires at 40 + command_bytes > SLOT_BUF_BYTES = 4,096, i.e. "
        "command_bytes > 4,056 -- and it is checked FIRST. Every frame large "
        "enough to trip the FRAME_SLOT_BYTES law has already been refused by "
        "the staging bound, so both the shipped expression and the mutated one "
        "are dead code and no test can tell them apart. "
        "The check is defensive for a LARGER staging buffer, which is a "
        "reasonable thing to keep. RE-SCORE THIS THE MOMENT SLOT_BUF_BYTES "
        "RISES ABOVE 1,048,536 -- that parameter, not the mutant, is the thing "
        "to watch.",
}


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--equiv":
        proof = EQUIVALENT.get(argv[2])
        if proof is None:
            return 1
        print(proof)
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
