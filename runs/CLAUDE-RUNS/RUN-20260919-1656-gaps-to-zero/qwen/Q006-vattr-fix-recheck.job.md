# Q006 vattr-fix-recheck
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912
continue: Q005

## Brief

Q005 reviewed GEOM.VATTR (commit d52ae6c0) and found: (1) a landing reads u/v for its index with no check
that u/v was staged; (2) a lookup before done_o reads stale rows; (3) done_o rises with dropped rows;
(6)/(8) events on the batch_i cycle update arrays or counters asymmetrically. Commit 94add368 claims to fix
all of them: the u/v join is now a handshake with a firing counter; a dropped or out-of-store row poisons
the batch through REPLAY's poison input; batch-boundary events go to the new batch; REPLAY cannot look up
before the done_o-gated handle.

VERIFY THE FIX, finding by finding. For each of (1), (2), (3), (6), (8): FIXED / PARTLY FIXED / NOT FIXED,
with file:line. Then list anything the fix itself introduced (a new hang, e.g. a landing that waits for
u/v which never comes because its vertex was a hole; a poison that never clears; a new counter that
cannot fire).

## Questions

1. (1)-(8) status, each with evidence.
2. New defects introduced by the fix.
3. Does the directed test now exercise each fixed case, and would each case FAIL on the old RTL?

## Inputs

fpga/rtl/geometry/zhao_geom_vattr.sv
show:94add368:fpga/rtl/geometry/zhao_geom_vattr.sv
show:94add368:fpga/rtl/geometry/zhao_geom_replay.sv
show:94add368:tests/geometry/geom_vattr_directed.cpp