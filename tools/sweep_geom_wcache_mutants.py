#!/usr/bin/env python3
"""The mutant table for zhao_vertex_arena.sv (GEOM.WCACHE).

WHY THIS SWEEP EXISTS
---------------------
GEOM.WCACHE was PROVED this morning: `prove` (abc pdr) returns an inductive
invariant in four seconds, with covers for non-vacuity, and the lane is
registered. That is the strongest evidence in this tree.

Its SIMULATION lane had still never been mutated, and
`reports/SWEEP_COVERAGE_AUDIT.md` names it for exactly that reason. The two ask
different questions:

  the proof   -- do the five shipped properties hold at every depth?
  the sweep   -- would `geom_wcache_directed` NOTICE a change the properties do
                 not constrain?

A proof only covers what it asserts. The directed lane checks payload values,
refusal classes, origins and counters that no assertion in the file mentions,
and nothing has ever tested whether those checks have teeth. Where a mutation
IS caught by the proof rather than the lane, that is worth knowing too -- it
says which evidence kind is actually carrying that law.

WHAT THIS BLOCK IS FOR, WHICH DECIDES WHAT TO MUTATE
----------------------------------------------------
The arena holds projected vertices per arena, and its contract leads with:

    A LOOKUP MUST NEVER WRAP INTO ANOTHER VERTEX.

Around that sit the laws that make it true: an arena is OPENED (bumping its
generation and clearing its validity), FILLED, then SEALED; a lookup is refused
unless the arena is sealed and the caller's generation still matches; a fill
outside the bounds, or into a sealed arena, is DROPPED and counted rather than
written. Every mutation below breaks one of those.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/geometry/zhao_vertex_arena.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the bounds, which are what "never wrap into another vertex" means
    ("W01 a fill index one past the end is accepted",
     "  wire fill_bad_index = (fill_index_i >= INDEX_W'(DEPTH));",
     "  wire fill_bad_index = (fill_index_i > INDEX_W'(DEPTH));"),
    ("W02 a fill arena one past the end is accepted",
     "  wire fill_bad_arena = (fill_arena_i >= ARENA_W'(ARENAS));",
     "  wire fill_bad_arena = (fill_arena_i > ARENA_W'(ARENAS));"),
    ("W03 a lookup index one past the end is answered",
     "  wire look_bad_index = (look_index_i >= INDEX_W'(DEPTH));",
     "  wire look_bad_index = (look_index_i > INDEX_W'(DEPTH));"),
    ("W04 a lookup arena one past the end is answered",
     "  wire look_bad_arena = (look_arena_i >= ARENA_W'(ARENAS));",
     "  wire look_bad_arena = (look_arena_i > ARENA_W'(ARENAS));"),

    # ---- the seal, which is what stops a half-built arena being read -----
    ("W05 a fill into a SEALED arena is written instead of dropped",
     "  wire fill_sealed    = !fill_bad_arena && sealed_q[fill_arena_i[AW-1:0]];",
     "  wire fill_sealed    = 1'b0;"),
    ("W06 a lookup into an UNSEALED arena is answered",
     "  wire look_unsealed  = look_bad_arena || !sealed_q[look_arena_i[AW-1:0]];",
     "  wire look_unsealed  = look_bad_arena;"),
    ("W07 opening an arena leaves it SEALED",
     "        sealed_q[open_arena_i[AW-1:0]] <= 1'b0;",
     "        sealed_q[open_arena_i[AW-1:0]] <= 1'b1;"),
    # The three reshaped mutants below first orphaned a signal (or changed how
    # rst_n is classified) and so failed the LINTER rather than the tests. A
    # mutant that cannot build is a discard, not evidence.
    ("W08 a seal of an out-of-range arena is applied anyway",
     "      if (seal_i && !seal_bad_arena) sealed_q[seal_arena_i[AW-1:0]] <= 1'b1;",
     "      if (seal_i || !seal_bad_arena) sealed_q[seal_arena_i[AW-1:0]] <= 1'b1;"),

    # ---- the generation, which is the ABA law ----------------------------
    ("W09 opening an arena does NOT move its generation",
     "        gen_q[open_arena_i[AW-1:0]]    <= gen_q[open_arena_i[AW-1:0]] + GEN_W'(1);",
     "        gen_q[open_arena_i[AW-1:0]]    <= gen_q[open_arena_i[AW-1:0]];"),
    ("W10 the generation is compared against the WRONG arena's",
     "  wire look_stale     = look_bad_arena || (look_gen_i != gen_q[look_arena_i[AW-1:0]]);",
     "  wire look_stale     = look_bad_arena || (look_gen_i != gen_q[fill_arena_i[AW-1:0]]);"),
    ("W11 the generation test is inverted",
     "  wire look_stale     = look_bad_arena || (look_gen_i != gen_q[look_arena_i[AW-1:0]]);",
     "  wire look_stale     = look_bad_arena || (look_gen_i == gen_q[look_arena_i[AW-1:0]]);"),

    # ---- validity: a hit must mean the slot was actually written ---------
    ("W12 a lookup HITS a slot that was never filled",
     "  wire look_present   = look_valid_i && !look_refuse && valid_q[rd_addr];",
     "  wire look_present   = look_valid_i && !look_refuse;"),
    ("W13 opening an arena does not clear its validity",
     "        for (i = 0; i < DEPTH; i = i + 1) valid_q[open_arena_i * DEPTH + i] <= 1'b0;",
     "        for (i = 0; i < DEPTH; i = i + 1) valid_q[open_arena_i * DEPTH + i] <= valid_q[open_arena_i * DEPTH + i];"),
    # W14 may NOT reach for rst_n: fill_ok carries it, but fill_ok is consumed
    # in the clock-only memory block, whereas valid_q lives in the async-reset
    # block. Naming rst_n there makes it both async and synchronous in one
    # process and Verilator refuses it (SYNCASYNCNET). Widening the enable with
    # a term already in scope is the same defect without touching reset.
    ("W14 a fill dropped for a SEALED arena still marks the slot valid",
     "      if (fill_ok)   valid_q[wr_addr]        <= 1'b1;",
     "      if (fill_ok || (fill_valid_i && fill_sealed)) valid_q[wr_addr] <= 1'b1;"),

    # ---- the addresses themselves ----------------------------------------
    ("W15 the write address uses the LOOKUP index",
     "  assign wr_addr = {fill_arena_i[AW-1:0], fill_index_i[IW-1:0]};",
     "  assign wr_addr = {fill_arena_i[AW-1:0], look_index_i[IW-1:0]};"),
    ("W16 the read address uses the FILL arena",
     "  assign rd_addr = {look_arena_i[AW-1:0], look_index_i[IW-1:0]};",
     "  assign rd_addr = {fill_arena_i[AW-1:0], look_index_i[IW-1:0]};"),
    ("W17 the payload register reads the WRITE address",
     "    mem_q <= mem[rd_addr];",
     "    mem_q <= mem[wr_addr];"),

    # ---- the handshake and the fault -------------------------------------
    ("W18 a dropped fill is not counted as an overflow",
     "  wire fill_drop      = fill_valid_i && (fill_bad_index || fill_bad_arena || fill_sealed);",
     "  wire fill_drop      = 1'b0;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique.

    MIXED LINE ENDINGS ARE REAL AND THEY DEFEAT A SINGLE GUESS. This used to
    pick one ending -- CRLF if the file contained any -- and translate the
    anchor to it. A file edited by a tool that writes LF into an otherwise
    CRLF file then has BOTH, and a multi-line anchor silently matches zero
    times in the region that differs. Two engine mutants failed exactly that
    way on 2026-08-28 while every single-line anchor in the same table worked.

    So both forms are tried. A multi-line anchor that matches under either is
    accepted; one that matches under neither still raises, and one that
    matches under both is still ambiguous and raises too.
    """
    for nl in ("\r\n", "\n"):
        o = old.replace("\n", nl)
        n = new.replace("\n", nl)
        count = gold.count(o)
        if count == 1:
            if o == n:
                raise ValueError("mutant identical to base")
            return gold.replace(o, n, 1)
        if count > 1:
            raise ValueError("anchor matches %d times" % count)
    raise ValueError("anchor matches 0 times (tried CRLF and LF)")


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {}


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
