#!/usr/bin/env python3
"""The mutant table for zhao_debug_frameblit.sv, and the apply/restore primitives.

WHY THIS SWEEP EXISTS
---------------------
DEBUG.FRAMEBLIT is recorded CLOSED in `reports/REMAINING_BLOCKERS.md` --
RTL_VERIFIED on the composed path, with a directed lane, a lint lane and a
formal safety proof, all green. It had never been mutated.

`reports/SWEEP_COVERAGE_AUDIT.md` found it among 38 modules in that position.
It is first on the list because "closed" is the strongest claim in the ledger
and the one most worth being able to defend.

WHAT THIS BLOCK IS FOR, WHICH DECIDES WHAT TO MUTATE
----------------------------------------------------
FRAMEBLIT takes a framebuffer image from the HPS and publishes it to a slot the
shell has leased. Almost everything it does is REFUSAL: it validates a length,
a lease, a slot and a generation, it walks the transfer in 64-byte chunks past
a memory guard that can deny any write, it accumulates a CRC over what actually
landed, and only then does it publish.

So the failure that matters is not a wrong pixel. It is **publishing a frame
that should have been refused** -- a torn or foreign image presented as good,
with a status of OK. Every mutation below aims at one of the gates that stands
between a bad transfer and `publish_valid_o`:

  * the four validation checks, and the DISTINCT status each must report;
  * the lease still being valid, and still being OURS, at the publishing edge;
  * every byte issued AND retired before publish -- there is deliberately no
    timeout, because a blit that never retires is a broken machine and
    publishing anyway would hide it behind a picture that looks fine;
  * the CRC, which is the only thing that can tell a complete transfer from a
    complete transfer of the wrong bytes;
  * the chunk arithmetic, where an off-by-one publishes a frame missing its
    last beat.

Structure follows `sweep_terrain_patch_mutants.py` and, before it,
`sweep_terrain_normals_mutants.py`: the table is a Python module rather than a
bash array so that no shell ever expands a mutation.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/debug/zhao_debug_frameblit.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the four validation gates, and their DISTINCT statuses ----------
    # The five reshaped mutants below first orphaned a signal and so failed the
    # LINTER rather than the tests. A mutant that cannot build is a discard, not
    # evidence. Each keeps its defect and its operands.
    ("F01 a request that is TOO LONG passes the length check",
     "          if (r_len != canvas_bytes(r_mode)) begin",
     "          if (r_len < canvas_bytes(r_mode)) begin"),
    ("F02 a wrong length is reported OK unless the length is zero",
     "            fail <= ST_BAD_LEN;",
     "            fail <= (r_len == 32'd0) ? ST_BAD_LEN : ST_OK;"),
    ("F03 a blit with NO lease is allowed to proceed",
     "          end else if (!fb_lease_valid_i) begin",
     "          end else if (1'b0) begin"),
    ("F04 a slot that does not match the leased one is accepted",
     "          end else if (r_slot[7:1] != 7'd0 || fb_lease_slot_i != r_slot[0]) begin",
     "          end else if (r_slot[7:1] != 7'd0) begin"),
    ("F05 a stale generation is not noticed at validation",
     "          end else if (fb_lease_generation_i != r_gen) begin",
     "          end else if (1'b0) begin"),
    ("F06 the lease is acquired even on the failing paths",
     "          end else begin\n"
     "            owns_lease <= 1'b1;\n"
     "            state <= B_READ_REQUEST;\n"
     "          end",
     "          end else begin\n"
     "            state <= B_READ_REQUEST;\n"
     "          end"),

    # ---- the publishing edge --------------------------------------------
    ("F07 the lease is not re-checked at the publishing edge",
     "          if (abort_pending || !lease_ok_now) begin",
     "          if (abort_pending) begin"),
    ("F08 a lease lost mid-transaction publishes with status OK",
     "            if (fail == ST_OK) fail <= ST_LEASE_LOST;",
     "            if (fail == ST_OK) fail <= ST_OK;"),
    ("F09 a frame publishes without every byte having RETIRED",
     "          end else if (issued != r_len || retired != r_len) begin",
     "          end else if (issued != r_len) begin"),
    ("F10 a frame publishes without every byte having been ISSUED",
     "          end else if (issued != r_len || retired != r_len) begin",
     "          end else if (retired != r_len) begin"),
    # F11 wants a WEAKENED check, not a broken one -- F12 already covers "every
    # frame fails". Masking to 16 bits orphaned the upper half, so instead the
    # equality becomes a subset test: any accumulator carrying at least the
    # expected bits passes. All 32 bits stay read and corruptions that only
    # ADD bits slip through, which is the shape of a real mis-written check.
    ("F11 the CRC check accepts any SUPERSET of the expected bits",
     "          end else if ((crc_acc ^ 32'hFFFF_FFFF) != r_crc) begin",
     "          end else if (((crc_acc ^ 32'hFFFF_FFFF) & r_crc) != r_crc) begin"),
    ("F12 the CRC is compared unfolded, so every frame fails",
     "          end else if ((crc_acc ^ 32'hFFFF_FFFF) != r_crc) begin",
     "          end else if (crc_acc != r_crc) begin"),
    ("F13 a CRC failure is reported OK unless the expected CRC is zero",
     "            fail <= ST_CRC;",
     "            fail <= (r_crc == 32'd0) ? ST_CRC : ST_OK;"),
    ("F14 the published slot is the ABI's, not the LEASED one",
     "            publish_slot_o <= r_slot[0];",
     "            publish_slot_o <= r_slot[1];"),
    # F15 needs more than the assignment line: `owns_lease <= 1'b0` appears
    # three times (reset, the accept path, and the publish). Anchored on the
    # publish by carrying the two lines above it.
    ("F15 the lease is still held after a successful publish",
     "            done_o <= 1'b1;\n"
     "            owns_lease <= 1'b0;",
     "            done_o <= 1'b1;\n"
     "            owns_lease <= 1'b1;"),

    # ---- the lease predicate itself --------------------------------------
    ("F16 lease_ok_now ignores whether the lease is even valid",
     "  assign lease_ok_now = fb_lease_valid_i && (fb_lease_slot_i == r_slot[0]) &&",
     "  assign lease_ok_now = (fb_lease_slot_i == r_slot[0]) &&"),

    # ---- the chunk walk --------------------------------------------------
    ("F17 the remaining count is decremented by a whole chunk, not this one",
     "  assign rem_next = rem_q - 32'(tlen_q);",
     "  assign rem_next = rem_q - 32'(CHUNK_BYTES);"),
    ("F18 the last partial beat is one byte too long",
     "  assign beat_bytes = (beat_left >= 32'd8) ? 4'd8 : 4'(beat_left[3:0]);",
     "  assign beat_bytes = (beat_left >= 32'd8) ? 4'd8 : 4'(beat_left[3:0] + 4'd1);"),
    ("F19 the beat cursor is off by one, dropping the chunk's first beat",
     "  assign beat_left  = 32'(this_len) - ({29'd0, beat} * 32'd8);",
     "  assign beat_left  = 32'(this_len) - ({29'd0, beat} * 32'd8) - 32'd8;"),

    # ---- the request handshake -------------------------------------------
    ("F20 a new request is accepted while a blit is still running",
     "  assign req_ready_o = (state == B_IDLE);",
     "  assign req_ready_o = 1'b1;"),
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


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)  # NOW, never the future -- see sweep_debug_frameblit.sh


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
