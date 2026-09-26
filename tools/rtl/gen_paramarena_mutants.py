"""PARAMARENA: regenerate GEOM.PARAMBUF's arena positive controls from current
production.  Each is a COPY with a rename and ONE substantive change.

THREE mutants, for three counters that no legal stimulus can move:

  drain  -- `addr_view_bad_o`.  The drain precondition is dropped, so a seal
            takes effect with writes still outstanding and a request outlives
            the view selector that admitted it.
  align  -- `burst_unaligned_o`.  The two lines that make every request
            address a multiple of the SDRAM's burst-alignment quantum go back
            to their pre-2026-09-23 values, which is exactly the layout that
            shipped with owner item 4 and that NO TEST IN THIS TREE CAN FAIL
            ON -- the behavioural SDRAM model reads LINEARLY where the part
            wraps inside its aligned eight-column block.
  reserve -- `giant_reserve_breach_o`.  `ck_fits_c`'s `<` becomes `<=`, so the
            allocator admits ONE chunk past the frame's ordinary quota.  With
            the seal at exactly MAX_CHUNKS - reservation -- which is what
            `zhao_measure_sealplan` produces for a frame declaring R7's
            guaranteed giant -- that one chunk is the first chunk of the
            giant's reserve, and the breach detector is the only thing in the
            tree that can say so.

`zhao_geom_paramarena`'s `addr_view_bad_o` differences the view the HELD
address lies in against the view the LEASE names.  Its two operands load on
different enables -- `m_addr_q` when the engine takes an op, `view_q` at the
frame seal -- so it is not the lockstep-blind kind of check CLAUDE.md's
metadata-bank law is about.  But while the DRAIN PRECONDITION is correct the
view cannot move while anything is in flight, so the two are equal by
construction and NO LEGAL STIMULUS CAN MOVE THE COUNTER.  That leaves "it can
fire" as an argument forever.

`burst_unaligned_o` is unreachable for a different reason: the allocator
cannot compute a misaligned address, so the only way to see the counter move
is to break the allocator.

`giant_reserve_breach_o` is unreachable for a THIRD reason, and it is the one
worth stating because it spans two blocks.  The plan validator guarantees
`ordinary quota <= MAX_CHUNKS - reservation` and `ck_fits_c` guarantees
`cursor < ordinary quota`; compose them and the cursor can never reach the
reserve.  Neither guarantee alone is enough and neither block can demonstrate
the counter on its own, so the witness has to break one of them.  Breaking
`ck_fits_c` is the honest choice: it leaves the VALIDATOR correct, so the
frame that fires the counter is one whose plan was legal -- which is exactly
the fault the detector exists to catch, an allocator that overruns a plan it
agreed to.

So each break lives HERE, in a committed file, renamed so no production source
list can elaborate it, and driven with INVERTED POLARITY.

Run from the worktree root, or with the root as argv[1].
"""
import pathlib
import sys

ROOT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1
        else pathlib.Path(__file__).resolve().parents[2])
PROD = ROOT / "fpga/rtl/geometry/zhao_geom_paramarena.sv"
src = PROD.read_text(encoding="utf-8")


def emit(pairs, head, suffix):
    """Copy production, apply each (old, new) EXACTLY ONCE, rename, write.

    The uniqueness check is the load-bearing part: a `replace` that matched
    nothing would write a renamed copy of production and the positive control
    would measure the unmutated block, passing or failing for reasons that have
    nothing to do with the counter.
    """
    body = src
    for old, new in pairs:
        if body.count(old) != 1:
            raise SystemExit(
                "PRODUCTION TEXT NOT FOUND (or not unique) -- the copy cannot"
                " be generated from a version it does not match:\n" + old[:120])
        body = body.replace(old, new, 1)
    renames = (("module zhao_geom_paramarena\n",
                "module zhao_geom_paramarena_%s\n" % suffix),
               ("endmodule : zhao_geom_paramarena\n",
                "endmodule : zhao_geom_paramarena_%s\n" % suffix))
    for old, new in renames:
        if body.count(old) != 1:
            raise SystemExit("module rename failed: " + old.strip())
        body = body.replace(old, new, 1)
    out = ROOT / ("tests/mutants/zhao_geom_paramarena_%s.sv" % suffix)
    out.write_text(head + body, encoding="utf-8", newline="\n")
    print("wrote", out)


# ---------------------------------------------------------------- drain ----
DRAIN_OLD = """  wire drained_c   = (wr_words_q == '0) && (mstate_q == M_IDLE);
  wire seal_ok_c   = drained_c && !reader_busy_i && !pub_pending_q;
"""

DRAIN_NEW = """  // ============ THE MUTATION, AND IT IS THIS ONE TERM ============
  // Production writes
  //     wire seal_ok_c = drained_c && !reader_busy_i && !pub_pending_q;
  // and `drained_c` is the whole of item 4's "a chunk must not be reused while
  // a previous reader or outstanding transaction still owns it".  This copy
  // drops it, so a seal takes effect WITH WRITES STILL OUTSTANDING: `view_q`
  // flips while `m_addr_q` still holds an address allocated under the previous
  // view, and `pb_wr_view` -- a GLOBAL selector the guard is combinational
  // over -- moves under a request the guard has not yet accepted.  That is
  // precisely the hazard `zhao_mem_guard`'s header says MEM.GUARD cannot close
  // on its own and that this block closes at the producer.
  //
  // `drained_c` itself is KEPT, so the copy differs from production in one
  // expression rather than in a deleted declaration and a refresh merge has one
  // hunk to carry forward.  It has no reader here, hence the pragma.
  /* verilator lint_off UNUSEDSIGNAL */
  wire drained_c   = (wr_words_q == '0) && (mstate_q == M_IDLE);
  /* verilator lint_on UNUSEDSIGNAL */
  wire seal_ok_c   = !reader_busy_i && !pub_pending_q;
"""

DRAIN_HEAD = """// zhao_geom_paramarena_drain_mutant.sv -- A POSITIVE CONTROL, NOT A DESIGN.
//
// DRIVEN-BY: tests/geometry/geom_paramarena_drainmut.cpp, INVERTED POLARITY:
//            the `geom_paramarena_drainmut_fires` ctest PASSES when
//            `addr_view_bad_o` FIRES against this copy, and the
//            `geom_paramarena_drainmut_silent` ctest beside it builds the SAME
//            driver against UNMUTATED production and requires SILENCE.  That
//            pair is the negative control CLAUDE.md asks for: without it a seam
//            that never engaged would compile production twice and both runs
//            would agree, saying nothing.
//
// WHY THE COUNTER NEEDS ONE.  `addr_view_bad_o` differences the view the HELD
// request address lies in against the view the LEASE names.  Its two operands
// move on DIFFERENT enables -- `m_addr_q` when the engine takes an op, `view_q`
// at the frame seal -- so the comparison is structurally able to see a request
// that outlived its selector, which is the whole reason it was written down.
// But production's own drain precondition makes that state UNREACHABLE: a seal
// is held pending while `wr_words_q != 0` or the engine is not idle, so the two
// are equal by construction and NO LEGAL STIMULUS CAN MOVE THE COUNTER.  A
// counter asserted zero that has never been seen to move is a claim, not an
// instrument.
//
// WHAT ONE TERM WAS CHANGED: `drained_c` is dropped from `seal_ok_c`, so the
// view flips with writes still outstanding.  Everything else in this file is
// production, character for character.
//
// GENERATED by tools/rtl/gen_paramarena_mutants.py from production, so a
// refresh is a command rather than an act of transcription.
//
// REGENERATE IT if zhao_geom_paramarena.sv changes shape: it is a COPY, and
// tools/budget/mutant_copy_drift.py will say when it is stale -- a copy of an
// old version is a positive control for a block that no longer exists.
// RENAMED so no production source list can elaborate it.
// ===========================================================================
"""

# ---------------------------------------------------------------- align ----
ALIGN_OLD = """  localparam int unsigned PV_SLOT_B      = PV_STRIDE_B;    // w=32 bytes
  localparam int unsigned LAYOUT_ALIGN_B = BURST_ALIGN_B;  // w=16 bytes
"""

ALIGN_NEW = """  // ============ THE MUTATION, AND IT IS THESE TWO LINES ============
  // Production writes
  //     localparam int unsigned PV_SLOT_B      = PV_STRIDE_B;
  //     localparam int unsigned LAYOUT_ALIGN_B = BURST_ALIGN_B;
  // and those two together are the whole of the 2026-09-23 burst-alignment
  // repair.  This copy puts both back to the values the layout carried when
  // owner item 4 merged: the 24-byte vertex stride, and no round-up of the
  // sub-region offsets.  TRI_OFF_B is then 65,535 * 24 = 1,572,840, which is
  // 8 mod 16, so every TriangleDescriptor request starts at column 4 of an
  // aligned eight-column block and its eight-word burst wraps in silicon.
  //
  // BOTH lines are needed, and that is informative rather than tidy: with the
  // round-up alive a 24-byte stride still lands TRI_OFF_B on 1,572,848, so the
  // region base is repaired even when the stride is not.  The two halves of
  // the repair cover different records.
  //
  // `BURST_ALIGN_B` itself is NOT touched, so `ALIGN_LSB` and the detector go
  // on measuring against the real SDRAM quantum while the layout is wrong --
  // which is the only arrangement in which the counter is evidence.
  localparam int unsigned PV_SLOT_B      = PV_B;
  localparam int unsigned LAYOUT_ALIGN_B = 1;
"""

ALIGN_HEAD = """// zhao_geom_paramarena_align_mutant.sv -- A POSITIVE CONTROL, NOT A DESIGN.
//
// DRIVEN-BY: tests/geometry/geom_paramarena_alignmut.cpp, INVERTED POLARITY:
//            the `geom_paramarena_alignmut_fires` ctest PASSES when
//            `burst_unaligned_o` FIRES against this copy, and the
//            `geom_paramarena_alignmut_silent` ctest beside it builds the SAME
//            driver against UNMUTATED production and requires SILENCE.  That
//            pair is the negative control CLAUDE.md asks for: without it a seam
//            that never engaged would compile production twice and both runs
//            would agree, saying nothing.
//
// WHY THE COUNTER NEEDS ONE, AND WHY THIS IS THE ONLY WITNESS THIS TREE CAN
// HAVE.  `zhao_sdram_ctrl` sets BL8 SEQUENTIAL and derives its column straight
// from the byte address, so a JEDEC sequential burst wraps inside its aligned
// sixteen-byte block; `zhao_vram_arbiter` chops to min(rem, 8, row_tail) and
// therefore aligns only to the 2048-word ROW.  The behavioural SDRAM model
// reads and writes LINEARLY, so a misaligned request is served CORRECTLY in
// simulation and WRONGLY by the part.  No functional test in this repository
// can fail on it, in either polarity -- which is exactly why the invariant is
// COUNTED rather than assumed, and why the counter's silence is worth nothing
// until it has been seen to move.
//
// WHAT WAS CHANGED: two localparams, back to the pre-repair layout.  See the
// block comment at the mutation.  Everything else in this file is production,
// character for character.
//
// GENERATED by tools/rtl/gen_paramarena_mutants.py from production, so a
// refresh is a command rather than an act of transcription.
//
// REGENERATE IT if zhao_geom_paramarena.sv changes shape: it is a COPY, and
// tools/budget/mutant_copy_drift.py will say when it is stale.
// RENAMED so no production source list can elaborate it.
// ===========================================================================
"""


# -------------------------------------------------------------- reserve ----
RESERVE_OLD = """  wire ck_fits_c = (n_chunks_q < q_chunks_q);
"""

RESERVE_NEW = """  // ============ THE MUTATION, AND IT IS ONE CHARACTER ============
  // Production writes
  //     wire ck_fits_c = (n_chunks_q < q_chunks_q);
  // -- the frame may hold at most `q_chunks_q` chunks, the ORDINARY quota the
  // plan validator sealed.  This copy writes `<=`, so the allocator admits one
  // chunk MORE than the plan it agreed to.
  //
  // WHY ONE CHUNK IS THE WHOLE DEMONSTRATION.  `zhao_measure_sealplan` seals a
  // frame that declares R7's guaranteed giant at exactly
  // MAX_CHUNKS - ceil(GIANT_REFS/CHUNK_IDS), so chunk number `q_chunks_q` --
  // the first one this mutation admits -- is the first chunk of the giant's
  // reserved region.  `giant_reserve_breach_o` fires on it and nothing else in
  // the tree can see it: `quota_overflow_o` does NOT fire, because under this
  // mutation the allocation is (wrongly) considered to fit, and the frame is
  // never faulted.  A silent overrun into the reservation is precisely the
  // shape the detector was written for.
  //
  // NOTE THE DIRECTION, because it is this tree's own law.  The mutation makes
  // the allocator MORE permissive, so every functional test goes on passing --
  // more chunks are accepted, nothing is refused, no frame faults, no golden
  // output moves.  The defect is invisible to every result-checking test by
  // construction, which is why the invariant is COUNTED rather than assumed.
  wire ck_fits_c = (n_chunks_q <= q_chunks_q);
"""

RESERVE_HEAD = """// zhao_geom_paramarena_reserve_mutant.sv -- A POSITIVE CONTROL, NOT A DESIGN.
//
// DRIVEN-BY: tests/geometry/geom_paramarena_reservemut.cpp, INVERTED POLARITY:
//            the `geom_paramarena_reservemut_fires` ctest PASSES when
//            `giant_reserve_breach_o` FIRES against this copy, and the
//            `geom_paramarena_reservemut_silent` ctest beside it builds the
//            SAME driver against UNMUTATED production and requires SILENCE.
//            That pair is the negative control CLAUDE.md asks for: without it
//            a seam that never engaged would compile production twice and both
//            runs would agree, saying nothing.
//
// WHY THE COUNTER NEEDS ONE.  `giant_reserve_breach_o` fires when an ACCEPTED
// ordinary chunk carries total allocation into the region reserved for R7's
// guaranteed giant.  That state needs TWO guarantees to be unreachable and
// they live in two different blocks: `zhao_measure_sealplan` refuses any plan
// whose ordinary chunk demand exceeds MAX_CHUNKS minus the reservation, and
// `ck_fits_c` here refuses any allocation past the sealed quota.  Compose them
// and no legal stimulus can move the counter.  "It can fire" would otherwise
// stay an argument forever, and a counter asserted zero that has never been
// seen to move is a claim rather than an instrument.
//
// WHAT ONE CHARACTER WAS CHANGED: `ck_fits_c`'s `<` became `<=`.  The
// VALIDATOR is left correct on purpose, so the frame that fires the counter is
// one whose plan was legal -- an allocator overrunning a plan it agreed to,
// which is the fault the detector is for.  Everything else in this file is
// production, character for character.
//
// GENERATED by tools/rtl/gen_paramarena_mutants.py from production, so a
// refresh is a command rather than an act of transcription.
//
// REGENERATE IT if zhao_geom_paramarena.sv changes shape: it is a COPY, and
// tools/budget/mutant_copy_drift.py will say when it is stale.
// RENAMED so no production source list can elaborate it.
// ===========================================================================
"""

emit([(DRAIN_OLD, DRAIN_NEW)], DRAIN_HEAD, "drain_mutant")
emit([(ALIGN_OLD, ALIGN_NEW)], ALIGN_HEAD, "align_mutant")
emit([(RESERVE_OLD, RESERVE_NEW)], RESERVE_HEAD, "reserve_mutant")
