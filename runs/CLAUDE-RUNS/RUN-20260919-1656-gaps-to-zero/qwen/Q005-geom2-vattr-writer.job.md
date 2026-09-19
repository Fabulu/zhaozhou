# Q005 geom2-vattr-writer
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

Owner ruling R11: per-vertex u/v/rgb/alpha live in a vertex-attribute store KEYED EXACTLY LIKE THE
ARENA (the on-chip store of projected vertex positions) and read by the same three per-triangle
lookups GEOM.REPLAY makes into the arena. The store must be written "at the same moment and from the
same producer that writes the arena position": VDECODE/SKIN for u/v (times the view's 1/w, for the
over-w form), and `zhao_light_stream` for rgb/alpha, WITHOUT widening the palette/skin/group
payloads. The hard part: the value exists at decode time, but the arena INDEX exists only when
GEOM.GROUP_SEQ issues the vertex, so the join must be owned by one block rather than being two
independent streams that happen to line up.

`zhao_geom_vattr.sv` (commit d52ae6c0, not yet merged) is that writer. You are shown it and its
directed test.

## Questions

1. Is the value written to arena index K ALWAYS the attribute of the vertex whose position lands at
   index K? Look for any path where the index and the attribute are advanced by different
   handshakes: a stall on one side, a refused vertex (a "hole", which never lands in the arena), dual
   view (two positions per vertex), a light result arriving late.
2. Can a lookup read a slot the writer has not yet written for this group (read-before-write), and
   what does it get then?
3. u/v × 1/w: check widths, rounding and signedness, and whether the 1/w used is the SAME view's.
4. Does the directed test compare against an INDEPENDENT reference, or against a transcription of the
   RTL? Does it cover holes, stalls and dual view?
5. Any counter or guard wired so that it cannot fire (both operands behind one enable)?

## Inputs

C:\Users\Fabs\AppData\Local\Temp\claude\C--programmieren-zencrifice\6dc16d10-374e-4090-84db-3119abad56bf\scratchpad\q005-zhao_geom_vattr.sv
C:\Users\Fabs\AppData\Local\Temp\claude\C--programmieren-zencrifice\6dc16d10-374e-4090-84db-3119abad56bf\scratchpad\q005-geom_vattr_directed.cpp