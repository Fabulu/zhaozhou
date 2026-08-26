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

## 2026-08-26 02:10 UTC+02:00 - Committed 86083e6, pushed

`ctest -L fast` 277/277, `ledger:check` OK. Gates run SEPARATELY -- chaining them
once produced exit 0 over a red format check.

Note on the format gate: there is no `npm run format:check` in this repo. I ran
it, got exit 1, and nearly read that as a formatting failure; it was npm
reporting a missing script. The real gate is the `format_check` CTest test,
which carries the `fast` label, so `ctest -L fast` already covers it.

## 2026-08-26 02:15 UTC+02:00 - The length family: LEN2, LEN3, DIST2

Rule 1 first: `reference/include/zfield/zfield.hpp` gives OP_LEN2 0x12, OP_LEN3
0x13, OP_DIST2 0x14, and `zfield_interpret.cpp` implements DIST2 as `len_of` over
the componentwise difference -- the same `len_of` LEN2 and LEN3 use. v1's
`zhao_field_len` already covers all three in `mode_i` 0/1/2. So again: wiring,
not arithmetic.

**What made it harder than CURVE.** CURVE takes one operand. These take up to
five: LEN3 reads a, a+1, a+2 and DIST2 also reads b, b+1. v2's three read ports
are addressed by the instruction's a/b/c fields, and `a+1` is reachable from
none of them.

The decision was written into `reports/FIELD_V2_MODEL.md` BEFORE the RTL: one
stolen read cycle rather than two more register-file ports. The banked file
measured 12 M10K at three ports, port count is what M10K replication scales
with, so five ports is roughly +4 M10K permanently for a minority of the opcode
histogram -- against one clock on an operation that already costs about
LANES x II.

Built:

* `zhao_field_v2_lanemux` carries an OPERAND BUNDLE (a0,a1,a2,b0,b1) and a UNIT
  SELECTOR, both captured once at accept like the tag. The block stays
  unit-agnostic; the core routes.
* `zhao_field_v2_core` instantiates v1's unmodified `zhao_field_len` and
  `zhao_field_isqrt` beside the curve unit, muxes the single multiplier on the
  captured unit id -- an arbiter is NOT needed while the interlock keeps one long
  op in the machine, and the comment says what makes it wrong if that changes --
  and folds the length's saturation lanes into the sticky ledger.
* The steal cycle, with a stall STRICTER than the long-op interlock: it stalls
  everything, because an instruction issuing into a stolen read cycle computes on
  another instruction's operands. That is a wrong ANSWER, not a hang.
* `long_slot_busy` gained `steal_now || steal_q`. A length is long for THREE
  cycles, and between stage 1 and its delayed dispatch there is a window where
  the old interlock would have let a second long op in -- the same hang, through
  a different door.

Sections 9 and 10 of the differential: 9 checks all three ops against
`zfield::interpret` across 8 wavefronts x 4 lanes, and 10 surrounds a LEN3 with
ADDs so a stolen read shows up as a wrong SUM.

**Section 9 passed first try, which I did not trust.** Every check in it is "DUT
equals oracle", and that passes just as happily when both are zero. So the
oracle is now required to be non-degenerate: no zero answers, and 32 distinct
answers across 32 wavefront/lane pairs. It is.

    38 checks green (was 23)
    LEN2 / LEN3 / DIST2: 16 instructions in 1,365 clocks each
    mixed short/long program: 1,377 clocks

Fifteen mutants added (M97-M111) for the addresses, the saved first pass, the
steal stall, the routing, the multiplier mux, the saturation lanes and the
serialiser's bundle. M90 and M96 were re-anchored again -- the mode case grew a
unit selector and `issue_fire` grew the steal stall.

**Not done, and recorded rather than assumed:** v2 has NO LEDGER ENTRY. It is not
a registered block, because it has no fit. That is why `ledger:check` is green
without it and why it must not be read as "v2 is accounted for in the census".

## 2026-08-26 02:50 UTC+02:00 - The trap earned its place the same day

The 32-mutant v2 sweep DIED silently. Its output file was 0 bytes and no
verilator, python, ninja or cmake process was alive.

Two separate faults, worth separating:

1. **I could not SEE it die.** I had launched it as `bash sweep.sh 2>&1 | tail
   -45`, and `tail` buffers everything until the command exits. A sweep that
   dies mid-run through that pipe produces an empty file, which is
   indistinguishable from a sweep that has not printed yet. Relaunched without
   the pipe, and put a Monitor on the file instead so survivors and aborts
   surface as they happen rather than at the end.

2. **The tree was CLEAN.** `git diff` on both v2 files showed only my own
   intended changes -- no mutant residue. That is the `trap on_exit EXIT INT
   TERM` added a few hours earlier doing exactly the job it was added for.

The precedent it was written against: a killed sweep once stranded a SURVIVING
mutant in `zhao_field_mul.sv`, and the next gate ran green over a deliberate
defect. Today the same class of event happened and cost nothing but the rerun.

I did not diagnose WHY the sweep process died. It is a long PowerShell-hosted
bash job and the likeliest cause is the host being torn down, not the script.
Recorded as unexplained rather than assumed benign: if it recurs, the pipe is no
longer a candidate and the host is.

## 2026-08-26 02:55 UTC+02:00 - RING, checked before writing anything

Rule 1 for the increment after this one. `OP_RING` resolves in
`zfield_interpret.cpp` (two smoothsteps about the midpoint of [r0,r1]) and v1's
`zhao_field_ring` implements it.

**RING is cheaper to reach than the length family, not dearer.** Three operands,
all from the natural a/b/c ports, so no steal cycle -- it dispatches straight
from stage 1 and the operand bundle built for DIST2 carries it unchanged.

**What it does cost:** `zhao_field_ring` drives the shared multiplier AND the
shared reciprocal, and `zhao_field_rcp` drives the multiplier too. Two consumers
inside ONE operation, which the "one long op in flight means one active unit"
argument does not cover. v1 solved it with a fixed priority -- the reciprocal
outranks the executing unit -- and v2 will mirror it exactly rather than invent
arbitration that would need re-proving.

**RIDGE and NOISE2 are blocked on an interface change:** both read `ins.imm`, and
v2's instruction port is `{op, dst, a, b, c}` with no immediate at all. Its own
increment, recorded now so it is not discovered halfway through NOISE2.

## 2026-08-26 03:40 UTC+02:00 - I ran a second sweep on top of a live one

**My error, and it cost the session's afternoon.**

I checked whether the first 32-mutant sweep was alive with `ps | grep -c
verilator` and got 0, so I declared it dead and started a second one. It was not
dead. It was BETWEEN ITERATIONS -- a sweep spends most of its time in cmake,
ninja and the test exes, and only briefly in verilator. The process was PID
29096, started 01:48:20, and it kept running for another two hours while I ran
preflights and edited files beside it.

What that looks like from the outside is not an obvious clash. It looks like:

* anchors that are demonstrably present in the file reporting `NOT UNIQUE (0)`;
* a DIFFERENT mutant failing on every run (M80, then M84+M92, then M97);
* `zhao_field_v2_core.sv` changing between two commands in the same shell call;
* and finally the preflight capturing a MUTATED tree as its `gold` baseline and
  reporting **32 of 111 mutants as broken** -- every one of them a signal
  orphaned by the stuck mutation, not by the mutant under test.

I chased three wrong explanations first (a Windows write-visibility race, mixed
line endings, an off-by-one in the mutant list) before checking process START
TIMES rather than process names.

**Recovery.** Killed the sweep, waited for the tree to settle (three identical
hashes), found ONE stranded mutation -- M106 on line 548 -- and removed it. Then
scanned EVERY mutant's replacement text against the tree to prove nothing else
was stranded: none. Rebuilt and re-ran: 38 + 14 checks, exactly as before.

Note the stranded mutation survived because I killed the PowerShell HOST, so the
script's trap never ran. The trap protects against the script dying, not against
its host being killed out from under it.

### Guard 8: one sweep at a time

`tools/sweep_field_dsp.sh` now takes a lock recording pid, start time and the
subset, and refuses to start if one exists. The trap drops it. The abort message
says how to check for a stranded mutation, because that is the state a stale
lock implies.

### The preflight was lying, twice over

1. **It had NO v2 CONE.** `zhao_field_v2_core.sv` and
   `zhao_field_v2_lanemux.sv` were in the mutant list but in no lint cone, so
   every v2 mutant was linted against a composition that does not contain it.
   The lint passed by not looking. Closing that gap immediately exposed **twelve
   genuinely malformed mutants** -- M80, M85, M86, M94, M95, M96, M104, M105,
   M106, M108, M109, M110 -- each orphaning a signal under -Wall. They had been
   SCORED AS CAUGHT in earlier runs, which was true but unearned: a mutant that
   trips -Wall is malformed by this project's rule, and five of them were sitting
   in the ledger of "proven" evidence.

   All twelve rewritten to keep the defect and stop orphaning the signal --
   `ready = active & ~(inflight & finished) & ~finished` rather than dropping
   `inflight`; `(state == S_DONE) ? req_wf_i : wf_q` rather than dropping `wf_q`;
   and so on. Fix the mutation, not the guard.

2. **It trusted the working tree as its baseline.** It now lints the baseline in
   both cones BEFORE scoring anything, and aborts with the `git diff` command to
   run if it is dirty. Thirty-two false failures are worse than none: they read
   like a bad mutant list and send you editing correct mutants.

## 2026-08-26 02:35 UTC+02:00 - Rule 1 on the WHOLE remaining opcode set, not one op at a time

While the sweep held the tree I read the oracle for every remaining Field
opcode rather than just the next one. That was worth doing: the plan was wrong.

I had NOISE2, RIDGE, ROT2, ROT3 and NORMALIZE2/3 queued as five separate
"wire v1's unit to the seam" increments, like CURVE and the length family.
**Five of the six write MORE THAN ONE REGISTER**, and v2's reply path returns one
value per lane and writes one register. Every long op built so far is
single-result, so the seam has never been asked for more.

| op | reads | writes | imm |
| --- | --- | --- | --- |
| NOISE2 | a, a+1 | dst, dst+1 | seed |
| RIDGE | a, b | dst | seed |
| ROT2 | a, a+1, b | dst, dst+1 | - |
| ROT3 | a, a+1, a+2, b | dst, dst+1, dst+2 | axis |
| NORMALIZE2 | a, a+1 | dst, dst+1 | - |
| NORMALIZE3 | a..a+2 | dst..dst+2 | - |

The v1 units are ALREADY wide -- `zhao_field_rot` has o0/o1/o2 and
`zhao_field_noise` has o0/o1 with a `seed_i`. The arithmetic is proven. What is
missing is v2's seam.

So the remaining work is TWO interface changes (an immediate port; a
multi-result reply and multi-register write-back) and then five cheap wirings --
not five medium ones. Reads are already covered by the DIST2 bundle plus the
steal cycle.

**Named now rather than discovered later:** a three-register write-back is the
first thing that can break v2's no-forwarding argument, which rests on "the
previous instruction has written back before the next is fetched" and was made
about ONE register. That is why it gets its own increment and its own sweep
instead of riding along with an opcode.

Recorded in `reports/FIELD_V2_MODEL.md`.

## 2026-08-26 02:45 UTC+02:00 - The v2 sweep: 32 mutants, 31 caught, one real gap

    attempted=32 expected=32 accounted=32 caught=31
    SURVIVOR: M106 the length's saturation never reaches the ledger

**M106 is a genuine test gap, and its shape is worth naming.** The mutant makes a
length's saturation register only when a curve also saturated. It survived all 38
checks for one reason: **every section checks VALUES, and saturation is not a
value.** It is the engine's account of what it had to clamp, and the values stay
correct while the account is lost.

That is the same failure the dangling `sat_*` pins nearly caused when CURVE
landed -- caught then by a lint, and by nothing at all this time.

**Section 12** closes it. Expectation built from `zref::SatLedger` and
`zref::fx_sub`, which is the construction `field_len_directed.cpp` already uses
for this question -- not from my own reasoning about when a subtraction
overflows. Two cases:

* a DIST2 whose difference cannot fit, asserting the lane fires;
* a quiet 3-4-5 length asserting it stays clear -- without which a ledger wired
  stuck-at-one passes the first case.

Both first assert that the ORACLE agrees the operands do, and do not, saturate.
Without that the section could quietly become vacuous if the chosen numbers ever
stopped overflowing, and would still report green.

43 checks (was 38). M106 re-scoring now.

### The other 31

All caught, including every one of the twelve rewritten after the v2 lint cone
was added, and the three (M93/M94/M95) that survived before section 8 existed.
M96 and M102 -- the two forms of the long-op hang -- are both caught, so that
defect cannot return unnoticed through either door.
