# Q002 geom2-vdecode-deadlock
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

Commit 3dae8f10 (not yet merged) fixes a DEADLOCK in the geometry path. The problem: GEOM.GROUP_SEQ
issues a group of vertices and waits until every one of them has landed in the arena before it
seals the group. VDECODE (upstream) may REFUSE a vertex (bad format), and a refused vertex never
lands. So GROUP_SEQ waited forever, and so did everything behind it. The fix changes
`zhao_geom_group_seq.sv` and `zhao_geom_replay.sv`, and adds a directed test and a committed
mutant whose driver has inverted polarity (it passes when a counter FIRES).

You are shown the full post-fix GROUP_SEQ, the commit's diffs of GROUP_SEQ, of replay and of the
directed test, and the mutant. Judge whether the fix is CORRECT and COMPLETE.

## Questions

1. After the fix, is there ANY sequence of refused vertices (the first, the last, all of them,
   two in a row, a refusal on the same cycle a vertex lands) that still hangs GROUP_SEQ or seals a
   group while one of its vertices is still in flight?
2. What does the sealed group carry for a refused vertex? Could replay later read an arena slot
   that was never written, and produce a triangle from stale data? Check what replay does with it.
3. Is the new counter clocked independently of what it detects (not "a detector wired to two
   operands that move together")? Would it fire on the real fault?
4. Does the directed test assert CORRECT behaviour (the group completes and the refused vertex is
   excluded), rather than asserting that the bug happens?
5. Is the mutant's ONE substantive change the thing its header says it is, and would it really
   reintroduce the hang?

## Inputs

C:\Users\Fabs\AppData\Local\Temp\claude\C--programmieren-zencrifice\6dc16d10-374e-4090-84db-3119abad56bf\scratchpad\q002-group_seq-at-3dae8f10.sv

show:3dae8f10:fpga/rtl/geometry/zhao_geom_group_seq.sv
show:3dae8f10:fpga/rtl/geometry/zhao_geom_replay.sv
show:3dae8f10:tests/geometry/geom_group_seq_directed.cpp
show:3dae8f10:tests/mutants/zhao_geom_group_seq_mutant.sv