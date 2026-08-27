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


# Mutants proved EQUIVALENT rather than closed with a test. The sweep driver
# does not read this table -- these are recorded here because the alternative
# is a survivor that looks like a hole forever.
#
# F17  "rem_next = rem_q - CHUNK_BYTES" instead of "- tlen_q".
# F18  "the last partial beat is one byte too long".
#
#   EVERY LEGAL CANVAS IS AN EXACT MULTIPLE OF 64. zhao_pkg gives
#       Z60   184,320   STORM 153,600   DUO   196,608
#   and 184320 % 64 == 153600 % 64 == 196608 % 64 == 0. A blit whose length is
#   not one of those three is refused by ST_BAD_LEN before a single chunk is
#   walked, so inside the walk `rem_q` is always a whole number of chunks and
#   `clamp_chunk(rem)` therefore always returns exactly CHUNK_BYTES. The two
#   expressions are equal at every reachable state.
#   The same fact makes `beat_left` always a multiple of 8 and never less than
#   8 within a chunk, so the partial-beat branch F18 mutates is UNREACHABLE.
#
#   Both partial paths are defensive code for a canvas size that does not
#   exist. That is worth knowing on its own: if a future mode is added whose
#   canvas is not a multiple of 64, these two stop being equivalent and MUST
#   be re-scored. That is the condition to watch, not the mutants.
#
# F10  dropping "issued != r_len" from the publish gate.
#
#   `retired` only advances on credits for writes that were ISSUED, and
#   `issued` sums beat_bytes across the chunks, which sum to exactly r_len.
#   So retired <= issued <= r_len at every state, and retired == r_len
#   therefore implies issued == r_len. The remaining term already decides the
#   branch. F09, which drops the OTHER term, is NOT equivalent and is caught:
#   issued can reach r_len while credits are still outstanding, which is the
#   whole reason the conjunction is written.

# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. A proof that lives only in a comment is indistinguishable from a hole
# to everything except a careful reader.
EQUIVALENT = {
    "F10":
        "retired only advances on credits for writes that were ISSUED, and "
        "issued sums beat_bytes across chunks that sum to exactly r_len. So "
        "retired <= issued <= r_len at every reachable state, and "
        "retired == r_len therefore IMPLIES issued == r_len -- the remaining "
        "term already decides the branch. F09, which drops the OTHER term, is "
        "NOT equivalent and IS caught: issued can reach r_len while credits "
        "are still outstanding, which is the whole reason the conjunction is "
        "written that way.",
    "F15":
        "owns_lease is cleared UNCONDITIONALLY at every accept (the B_IDLE "
        "arm), so a value left set by a publish cannot survive into the next "
        "transaction. Between the publish and that accept the only reader is "
        "the per-cycle lease watch, and the abort_pending/fail it would set "
        "are cleared by the same accept. No reachable observation differs. "
        "CHECKED AGAINST BOTH EVIDENCE KINDS, not just argued: the mutation "
        "was applied and formal_debug_frameblit_safety PASSES with it too, so "
        "neither the simulation lane nor the proof can see it.",
    "F17":
        "EVERY legal canvas is an exact multiple of 64: zhao_pkg gives Z60 "
        "184,320, STORM 153,600 and DUO 196,608, and 184320 % 64 == 153600 % "
        "64 == 196608 % 64 == 0. A length that is not one of the three is "
        "refused by ST_BAD_LEN before a single chunk is walked, so inside the "
        "walk rem_q is always a whole number of chunks and clamp_chunk(rem) "
        "always returns exactly CHUNK_BYTES. The two expressions are equal at "
        "every reachable state. RE-SCORE THIS THE MOMENT A MODE IS ADDED "
        "WHOSE CANVAS IS NOT A MULTIPLE OF 64 -- that condition, not the "
        "mutant, is the thing to watch.",
    "F18":
        "The same fact: with every canvas a multiple of 64, beat_left inside "
        "a chunk is always a multiple of 8 and never below 8, so the "
        "partial-beat branch this mutates is UNREACHABLE. Both partial paths "
        "are defensive code for a canvas size that does not exist. Same "
        "re-score condition as F17.",
}

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
    sys.stderr.write("usage: --count | --name N | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
