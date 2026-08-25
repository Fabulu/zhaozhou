# Task Log: RUN-20260826-0113 - FIELD v2 long operations: CURVE/DCURVE/SPLINE and the dispatch interlock

**Created:** 2026-08-26 01:13 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260826-0113-field-v2-curve-and-longop-interlock/

---

## Objective

Land the FIELD v2 engine's FIRST long operation -- CURVE, and with it DCURVE and
SPLINE, which are the same unit in three modes -- reached over the tagged
request/reply seam built in RUN-20260825. Prove it against the shipped oracle
(`zfield::interpret`), sweep it, and close whatever the sweep exposes.

The evidence order is the measured Earth histogram, not preference: curves and
distance first, rings next, NORMALIZE never (no committed Earth program calls
it).

---

## Progress Timeline

### 2026-08-26 01:13 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260826-0113
- Created working directory
- Initial context: HEAD at 6d960ff, where CURVE had been ATTEMPTED, REVERTED and
  recorded rather than half-shipped. The first attempt failed because the curve
  table is a REGISTERED read -- the index presented this cycle is answered on the
  NEXT one -- and I drove it combinationally. v1's own bench states the protocol
  in a comment I had not read.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-08-26 01:15 UTC+02:00 - CURVE, and then DCURVE/SPLINE

- Re-applied the v2 long-op path with the registered-read protocol corrected in
  `tests/differential/field_v2_core_directed.cpp` (`tick_tbl`). Section 6 compares
  each lane against `zfield::interpret` on a two-instruction program. 17 checks.
- Section 7 added for DCURVE and SPLINE. They are ONE unit in three modes, so they
  needed evidence rather than RTL -- but the mode travels through the serialiser
  as part of the request, and section 6 alone would pass with the mode hard-wired
  to zero. 19 checks.

## 2026-08-26 01:20 UTC+02:00 - Sweep harness: a trap, and subset selection

Two changes to `tools/sweep_field_dsp.sh`, both fixing real defects rather than
adding conveniences:

- **`trap on_exit EXIT INT TERM`.** The sweep had NO trap. That is exactly how a
  killed sweep once stranded a surviving mutant in `zhao_field_mul.sv` and let the
  next gate run green over a deliberate defect. Guarded on `GOLDDIR` still
  existing, because the successful path deletes it and every clean run would
  otherwise compare against nothing and cry wolf.
- **`SWEEP_ONLY`**, to score one increment's mutants without a full re-sweep. It
  prints `!! SUBSET RUN: n of N mutants -- NOT a full sweep`, because "the sweep
  was green" is the claim the file exists to support and a subset must never be
  readable as a full one in a log.

Also: **M84's anchor had been consumed** by the CURVE change -- the in-flight
clear now carries the long-op guard. Rewritten onto the live line, keeping the
defect it names, rather than relaxed.

## 2026-08-26 01:35 UTC+02:00 - THE SWEEP FOUND A HANG

Added M90-M95 for the request/reply seam. First subset run: **4 caught, 3
SURVIVED**.

| mutant | first run | why it survived |
|---|---|---|
| M93 a long op counted at dispatch AND at reply | SURVIVED | no long-op program checked the retire count |
| M94 the unit given the LIVE mode, not the captured one | SURVIVED | one long op in flight, so live == captured |
| M95 a request retired without the serialiser accepting | SURVIVED | the serialiser was never busy |

All three are unreachable with a single long operation in the machine, and every
section started ONE wavefront. The real workload is eight wavefronts on one
program, drifting apart in pc, several wanting the unit at once.

**Section 8 was written to reach them, and it HUNG on the shipped RTL** -- 11 of
24 instructions retired, 20,000-clock guard hit.

Root cause, in `fpga/rtl/field/zhao_field_v2_core.sv`: the dispatch slot is
filled at stage 2, TWO CYCLES after the instruction issues, so the `!lq_valid`
term in `issue_fire` was two cycles late. A second long op reached stage 2 while
the first request still waited for the serialiser and overwrote it. The first
wavefront then waited forever for a reply to a request that no longer existed.

Not a wrong answer -- a stop. On terrain it would have looked like the machine
dying.

**Fix:** a long-op interlock. `ins_is_long && (lq_valid || (s1_valid &&
s1_is_long))` holds only LONG ops; short ops keep issuing past a pending request,
since they touch neither the slot nor the serialiser and stalling the whole
machine behind one curve lookup gives back the throughput v2 exists for.
Wavefronts write disjoint register regions, so a short op retiring beside a long
reply cannot collide with it.

Re-scored: **7/7 caught.** M96 added so the two-cycle-late guard can never come
back unnoticed. 23 checks green.

## Files Changed

- `fpga/rtl/field/zhao_field_v2_core.sv` - CURVE/DCURVE/SPLINE dispatch, saturation
  ledger outputs, **the long-op interlock**
- `tests/differential/field_v2_core_directed.cpp` - sections 6, 7, 8
- `tools/sweep_field_dsp_mutants.py` - M84 re-anchored, M90-M96 added (96 total)
- `tools/sweep_field_dsp.sh` - restore trap, `SWEEP_ONLY`
- `STATUS.md` - 2026-08-26 section for Fabian
