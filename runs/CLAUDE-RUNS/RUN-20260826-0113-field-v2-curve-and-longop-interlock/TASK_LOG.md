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

## 2026-08-26 03:05 UTC+02:00 - Length family committed (cc4dbfb), then RING

Gate: 43 checks, sweep 32/32 after the M106 gap was closed, `ctest -L fast`
277/277, `ledger:check` OK. Committed and pushed.

`.sweep_field_dsp.lock` added to `.gitignore` -- it is process state, not source.

### RING went in first try, because the groundwork was already paid for

52 checks (was 43). Three operands on the natural a/b/c ports, so NO steal cycle:
RING dispatches from stage 1 like a curve, and the operand bundle built for
DIST2 carries d/r0/r1 unchanged. 16 instructions in 1,556 clocks.

**What it actually cost was the multiplier.** RING is the first operation with
TWO CONSUMERS INSIDE IT -- `zhao_field_ring` drives the lane, and so does the
`zhao_field_rcp` it calls twice. The argument that justified v2's mux ("one long
op in flight, so one active unit") covers one unit being active, not one unit
making two demands. So selection became a PRIORITY CHAIN with the reciprocal on
top, which is v1's own arrangement in `zhao_field_exec_shared`. Mirroring it
means the same units against the same oracle; inventing different arbitration
would mean re-proving what v1's differential already covers.

### Section 13 applies the rule M106 bought

M106 established: any unit on this seam needs a SATURATION case, not just a
value case. RING has five ledger lanes and the interesting one is not a
saturation at all -- `rcp0` records a RECIPROCAL OF ZERO, and RING reaches it
from an input any caller can supply: **a band of zero width**. r0 == r1 collapses
the midpoint onto both edges, so both smoothsteps get e1 - e0 == 0 and hit the
pinned field_rcp zero rule (`zref_trig.hpp` SS7.3, which states it explicitly).

Both directions tested, since a lane wired stuck-at-one passes the degenerate
case by itself. Expectation from `zref::smoothstep` with a real SatLedger --
the shipped primitive RING is built from.

### Mutants M112-M118, and four re-anchors

New: wrong-unit routing, inner radius handed the outer one, the reciprocal
losing precedence on the multiplier, radii swapped at the unit, both ledger
lanes suppressed, and RING never dispatching at all.

M101, M104, M105 and M106 were re-anchored -- the RING wiring consumed their
anchor lines (the dispatch condition grew `|| s1_is_ring`, `to_len` gained a
sibling, the mul assign became an always_comb chain, and the sat_add line grew a
third term). Rewritten onto the live lines with their defects unchanged.

The v2 lint cone was extended with ring/rcp/rcp_rom, so RING's mutants are
actually linted rather than passing by not being looked at -- the hole that hid
twelve malformed mutants earlier today.

**The one I am watching: M114**, the reciprocal losing its precedence. If it
survives, the priority chain is untested and the test must force real contention
rather than assume the two consumers collide on their own.

## 2026-08-26 03:45 UTC+02:00 - RING's sweep: 37 caught, 2 EQUIVALENT

M114 and M116 survived. Both are provable equivalents rather than test gaps, and
the proofs differ in an important way: one is unconditional, the other is SCOPED.

### M114 -- the reciprocal loses its precedence on the multiplier

**Equivalent, unconditionally, by the ring unit's state encoding.**

`zhao_field_ring` asserts `rcp_valid_o` only in `G_SPAN` (4'd2) and `mul_issue_o`
only in `G_T`/`G_T2`/`G_2T`/`G_CUBE`/`G_FIN` (4, 6, 8, 10, 12). While the
reciprocal computes, the ring sits in `G_SPANW` (3) with `mul_issue_o` low.

So the ring and the reciprocal CANNOT drive the multiplier in the same cycle,
and the order of the priority chain is unobservable. The chain is still right --
it is v1's -- but it is arbitrating a contention that this composition cannot
produce.

I predicted this mutant was the one to watch and predicted the wrong reason. I
expected a test gap ("the test never forces contention"). The truth is stronger:
**no test can force it**, because the unit serialises the two demands itself.

### M116 -- rcp0 reaches the ledger only if BOTH report it

**Equivalent TODAY, and only today.** The ring latches
`rcp0_o <= rcp0_o || rcp_zero_i`, so it holds whatever the reciprocal reported,
and the ring is the reciprocal's ONLY consumer. The two lanes therefore always
overlap and `&&` cannot be told from `||`.

The `||` is defensive for a SECOND consumer, and there will be one: NORMALIZE
calls the reciprocal. **The moment a second consumer is wired, M116 stops being
equivalent and must be re-scored** -- it should then be caught, and if it is not,
the OR is genuinely untested.

### This needs a mechanism, not a comment

The sweep has no notion of an equivalent mutant. Its header already states the
law -- "Survivors are recorded with a PROOF of equivalence or they are holes;
'probably equivalent' is not a category this project has" -- but enforces it
nowhere, so a proven equivalent and a real hole look identical in the output and
both fail the run.

Implementing: an EQUIVALENT table keyed by mutant index carrying the proof text,
with three rules.

* a declared-equivalent mutant that SURVIVES is reported as equivalent, with its
  proof printed, and does not fail the run;
* a declared-equivalent mutant that is CAUGHT is an **ABORT** -- the proof is
  false, and a false proof is worse than an unproven survivor because it is
  believed;
* an undeclared survivor still fails, exactly as now.

The third rule is what keeps this from becoming a way to launder holes.

## 2026-08-26 04:05 UTC+02:00 - All three survivors resolved, three different ways

Re-scored under the new rules:

    M114  equivalent (proven)
    M116  equivalent (proven)   [scoped: re-score when NORMALIZE lands]
    M117  caught
    attempted=3 accounted=3 caught=1 equivalent=2

56 checks (was 52).

### M117 was a real gap, and it is the THIRD ledger hole this session

M106 (length's sat_add), M116/M117 (RING's rcp0 and mul). Every one of them:
values correct, account lost, every test green.

Stated as a rule in the model report rather than rediscovered a fourth time:
**when a unit is wired to the seam, every ledger lane it can raise needs a
case.** RING raises four and two had no test.

The input that reaches it is not exotic. RING divides by the band's half-span,
so a NARROW band gives a large reciprocal and (x - e0) * r overflows before
smoothstep clamps t. A band 1/128 wide with the point 1,000 units away -- a thin
ring seen from far off.

### I was wrong about M114 in an instructive way

I called it "the one I am watching" and expected a TEST GAP I would close by
forcing contention. The truth is stronger and the opposite in character: **no
test can force it.** zhao_field_ring asserts rcp_valid_o only in G_SPAN and
mul_issue_o only in G_T/G_T2/G_2T/G_CUBE/G_FIN, waiting in G_SPANW with
mul_issue_o low. The unit serialises its own two demands.

The prediction was right, the reasoning was wrong, and the difference matters:
one conclusion sends you writing a test, the other sends you writing a proof.

### The sweep now knows what "equivalent" means

Its header has said since it was written that survivors carry a proof or they
are holes. It enforced none of it: both printed identically, and the run **exited
0 either way**, so a hole could pass a gate that checked only the exit code.

`EQUIVALENT` maps a mutant's id token -> proof. Declared+survived reports and
passes; declared+CAUGHT **aborts** (a false proof is believed, which is worse
than an unproven survivor); undeclared+survived now FAILS. The last rule is the
one that keeps the category honest -- declaring costs writing a proof someone can
check.

Keyed by id token, not index: indices move when mutants are inserted, and four
anchors have already had to be rewritten this session for exactly that kind of
drift.

## 2026-08-26 04:35 UTC+02:00 - I committed RING without RING's RTL, and caught it

RING committed as 1faf6da. Before that, I committed it BROKEN and had to undo it.

**What happened.** RING's evidence was complete and the gate was running against
the RING binaries. While waiting I wrote the NEXT increment (the immediate port
and RIDGE) into the same two .sv files. Editing RTL during a ctest run is safe --
ctest runs already-built exes -- so that part was fine.

The mistake was the fix for it. To commit RING without the untested RIDGE work I
ran `git stash push` on the two .sv files. **RING's RTL was uncommitted too**, so
the stash took BOTH increments out of the working tree and reverted those files
to HEAD, which had no RING at all. The commit went in with RING's tests, RING's
mutants and RING's reports -- and no RING hardware. It would not have built.

**Caught by checking, not by luck**: `git show --stat HEAD` plus
`git show HEAD:...zhao_field_v2_core.sv | grep -c OP_RING` returned 0. Not
pushed, so `git reset --soft HEAD~1` and `git stash pop` restored everything.

**The recovery, and why not the easy way.** Both increments were tangled in two
files with no saved RING-only state. The easy route was to finish RIDGE and
commit both together, which would have buried an untested opcode inside a gated
commit. Instead I reversed the RIDGE edits mechanically -- every one of them
new->old, in the same anchored form they were applied -- then PROVED the reversal
by rebuilding: 56 + 14 checks, exactly the pre-RIDGE numbers, and a residue grep
showing no RIDGE token left except a pre-existing header comment.

**The lesson, which is not "be careful with stash".** It is: *`git stash` is only
a safe way to set aside work when the base state is committed.* On a dirty tree
carrying two increments in one file, it does not separate them -- it removes
both. The check that caught it (does HEAD actually contain the thing the commit
message claims?) is cheap and should be habitual for any commit made after a
stash.

`gate.log` also slipped into that first commit via `git add -A`. Now in
`.gitignore` along with `sweep_*.log` and `pf.txt`.

## 2026-08-26 04:50 UTC+02:00 - The immediate port, and RIDGE

63 checks (was 56). RIDGE: 16 instructions in **532 clocks** -- three times
cheaper than RING's 1,556, because the noise unit is a hash rather than an
iterative walk. The first opcode that costs the engine almost nothing.

`ins_imm_i` is the first new field in v2's instruction word since the engine was
written. Captured at issue into `s1_imm`, carried in the request beside the mode
and unit selector, and held once at accept in the serialiser.

### The immediate needed a test shape the value checks could not give

A seed dropped to zero, hard-wired, or read live still produces perfectly
plausible noise. Worse: it would AGREE with an oracle handed the same wrong
seed, so the test and the bug cancel out and the section reports green.

So section 15 runs **the same coordinates under two different seeds and requires
them to disagree on every lane**. That is what proves the immediate arrived --
the per-lane value match against `zfield::interpret` alone does not.

The same trap is waiting for ROT3, where the immediate is an AXIS SELECT: a
dropped axis rotates about the wrong one and the world stays plausible.

### Corrections found by building rather than by reading

* `zhao_field_noise.sv` was not in the v2 target's source list -- caught
  immediately as verilator MODMISSING.
* My first draft of this wiring invented a `.result_o_unused()` port on
  `zhao_field_noise`, which has `o0_o`/`o1_o` and no `result_o` at all. It never
  ran: the heredoc carrying it failed to terminate, and rewriting it directly
  against the real port list avoided the error. A lucky escape, not a method.

### NOISE2 stays refused

Same unit, one mode bit away, and deliberately not wired: it writes two
registers and the reply carries one. Wiring it now means either dropping `o1_o`
silently -- half an answer -- or bolting a second write onto a path not built for
it. `o1_o` is lint-waived WITH the reason and the name of what will read it.

### Anchor drift, again

Six mutants needed re-anchoring: four RING ones (M112, M113, M114, M118) and two
length ones (M101, M106). The RIDGE wiring touched the same shared lines --
`to_ring`, the dispatch condition, the priority chain, the sat_add fold.

This is now routine enough to be a cost worth naming: **every increment that
edits a shared line invalidates the anchors of every mutant on it.** The
preflight catches it every time, which is why it runs before the sweep rather
than after.

## 2026-08-26 05:30 UTC+02:00 - The new rule failed its first real run, correctly

    attempted=45 accounted=45 caught=42 equivalent=2
    EQUIVALENT (proven): M114, M116
    SURVIVOR: M119 the immediate is taken LIVE rather than carried
    FAILED: 1 mutant(s) survived without a proof of equivalence

**Exit 12.** Before today this run would have exited 0 with the survivor listed
in the output, and any gate checking only the exit code would have passed it.
The equivalence rules earned their place on their first outing.

### M119, and why NEITHER existing test could catch it

The immediate is carried through the serialiser exactly like the tag. The tag's
own mutants (M85, M86) are caught only because the lanemux bench POISONS the
request lines after accept -- without that, a live-read tag reads the same value
as a carried one and the difference is invisible.

`req_imm_i` was added to the RTL **after** that test was written. The bench had
zero references to it: it never drove it and never poisoned it.

And the core-level test cannot catch it either, which is structural rather than
an oversight worth fixing there: **the long-op interlock keeps one request in
flight, so the core's `lq_imm` sits stable while the serialiser works and live
equals captured by construction.** No core test can distinguish them.

Fixed where it belongs -- section 5 of the lanemux differential now drives a
distinct immediate, poisons the request line with its complement after accept,
and checks `u_imm_o` on every lane. 15 checks.

### The generalisation

**When a new field is added to a request that is captured-once, the poison list
must grow with it.** The poison block is the entire proof that anything is
carried rather than read live, and it is a hand-maintained list -- so a field
added later is a field silently exempt from the proof.

The same applies to the multi-result reply next: `rsp_y1_o`/`rsp_y2_o` will need
the same treatment, and the count carried with the request will too.

## 2026-08-26 06:20 UTC+02:00 - The multi-result reply, and NOISE2 on it

69 checks (was 63). NOISE2: 16 instructions in 725 clocks.

This was the ONE mechanism standing between v2 and the whole remaining opcode
set. Three things changed shape rather than merely growing:

* **The serialiser carries three result lanes and a COUNT.** The count rides
  with the request, so the reply is self-describing and the core does not
  re-decode an opcode that left stage 1 several cycles earlier.
* **The second read pass stopped being about lengths.** `s1_is_len` became
  `s1_needs_pass2`. The steal already fetched {a+1, a+2, b+1}, which is exactly
  what NOISE2, ROT2, ROT3 and NORMALIZE2/3 all want, so the mechanism
  generalised without changing.
* **The pass-2 slot stopped hard-wiring UNIT_LEN and one result.** It was built
  when lengths were its only user; NOISE2 takes the same path to a different
  unit, with two results and a seed.

### The M119 lesson applied at the right time

`req_nres_i` was added to the lanemux bench's POISON LIST in the same edit that
added it to the RTL -- not after a sweep found it missing, which is how the
immediate went. The three result lanes are driven with distinct values
(`result`, `+0x1000`, `+0x2000`) so a reply carrying one into all three cannot
hide.

### NOISE2's test proves the SECOND register, not just the first

`dst+1` is pre-loaded with `0xD00DBEEF` before the run, so a second write that
never happens fails loudly instead of landing on a convenient zero. The two
lanes are also required to DIFFER, because one value written twice would
otherwise pass.

### Ten mutants (M125-M134), five of which had to be rewritten

Two anchors were consumed by the widened capture and write-back (M87, M91), and
three orphaned a signal under -Wall (M126, M130, M131). M126 became "the two
results are SWAPPED between dst and dst+1", which is a better mutant than the
one I first wrote.

### ROT2/ROT3 checked while the sweep held the tree

They need NO new front-end mechanism -- every operand is already in hand. What
they do need is v2's FIFTH shared unit, `zhao_field_sin`, which
`zhao_field_rot` borrows rather than owns. Latency 2, II 1. OP_SIN and OP_COS
become nearly free once it is there.

## 2026-08-26 06:35 UTC+02:00 - M120 DISCARDED, and that is a hole not a pass

    M120 the immediate loses its low half on the way to the unit
      DISCARDED: a target did not LINK

Guard 5 caught it. Without that guard a mutant that fails to compile leaves the
previous binary in place and gets scored as CAUGHT -- the most flattering
possible way to be wrong.

**It is unscored, which means the mutation is unproven.** Not a failure of the
run, but not evidence either, and it must not be left looking like a pass.

What makes it worth a real look rather than a shrug: **M120 PASSED PREFLIGHT
LINT.** All 134 linted clean, in a cone containing zhao_field_v2_core. So either
the test build applies flags the lint cone does not, or the failure was
transient. Reproduce directly -- apply M120, build the target, read the error --
rather than guessing. To be done when the sweep releases the tree.

## 2026-08-26 07:10 UTC+02:00 - Three discards, two wrong diagnoses, one real cause

    attempted=55 expected=55 accounted=52 caught=50 equivalent=2
    CROSS-CHECK FAILED (attempted/accounted must both equal 55)

The cross-check failing is CORRECT: 3 mutants were DISCARDED, so the run did not
test what it claims to.

### Wrong diagnosis #1

I saw three mutants pass preflight lint and fail the test build, and concluded
the two checks disagreed -- that the preflight was overstating its coverage and
the guard was defective. I said so.

**They do not disagree.** Applying M120 by hand and rebuilding succeeds. The
preflight was right. The cause is environmental: the executable is deleted and
relinked every iteration, and Windows intermittently holds it. Three failures in
55 rebuilds is exactly that shape.

### Wrong diagnosis #2, walked straight into the guard

My first reproduction ran `cmake --build` alone, saw it succeed, and nearly
concluded the mutant was fine. **`verilate()` elaborates at CONFIGURE time** --
which is GUARD 1, written in this sweep's own header -- so a build without
`cmake -S . -B build` does not re-elaborate and proves nothing.

I read that guard aloud earlier today and still walked into it. The lesson is
not "read the header": it is that a reproduction which SKIPS a step the real
process performs is not a reproduction, and the burden is to match the process
step for step before believing the result.

### Two fixes, queued for when the lock releases

1. **Retry a failed link ONCE before discarding.** A transient succeeds on the
   second attempt; a genuinely broken mutation fails twice. The retry PRINTS
   that it happened -- silently retrying converts a flaky machine into invisible
   slowness, and the flakiness is worth seeing.
2. **Preflight only the SELECTED mutants on a subset run.** A 3-mutant re-score
   currently pays for 134 lints, roughly ten minutes to check three things. Re-
   scoring after a fix is the most frequent operation in this loop, so this is
   the cost that actually compounds.

Neither can be applied while a sweep is running: bash re-reads a script it is
executing, and the preflight is invoked by that same script.

## 2026-08-26 08:00 UTC+02:00 - ROT2/ROT3, and the selector widened

79 checks (was 69). Eleven of the fourteen Field operations now execute on v2.

ROT cost no new front-end mechanism, exactly as the design note predicted: the
second read pass already fetches reg[a..a+2] and the angle sits in reg[b], and
two or three results ride the reply NOISE2 opened. What was new is **v2's fifth
shared resource, the sine table**, which `zhao_field_rot` borrows rather than
owns.

### A design call worth recording

The unit selector was TWO BITS with all four codes taken, and ROT is the fifth.
I widened it to three rather than sharing UNIT_CURVE's code disambiguated by the
mode.

**A selector that needs a second field to disambiguate it is one that will
eventually be read without it.** Widening costs a wire; the squeeze costs a
class of bug that only shows up when someone reads the field somewhere new.

### The axis got two independent proofs

Same vector and angle under all three axes, required to DISAGREE; and the
PASS-THROUGH LANE -- X carries a0 untouched, Y carries a1, Z carries a2 -- which
is the cheapest possible proof the axis arrived, checked separately from the
rotated pair.

ROT2 must also NOT write dst+2 (its third lane is zero by law 5 and the register
belongs to the program). Pre-loaded with 0xFACEFEED and checked untouched.

### The tooling fixes paid immediately

* the SUBSET preflight linted 8 mutants in seconds, where it would have linted
  142 for ten minutes;
* the FULL preflight then caught **seven anchor drifts** from the ROT wiring
  before the sweep started -- M106, M114, M117, M124, M128, M129, M130. That is
  why it runs first, and it is the fourth time this session that editing a
  shared line invalidated other mutants' anchors.

### NORMALIZE2/3 needs ONE new thing

Everything is in hand except the shared INTEGER SQUARE ROOT, which v2 wires
straight to `u_len` because the length family was its only consumer. NORMALIZE
makes it two, so those wires become a mux on the captured unit id -- the same
shape, and the same caveat, as the multiplier.

It does NOT use the shared reciprocal (its own rcp24 ROM), which confirms the
correction made earlier today: M116's re-score trigger is OP_RCP, and wiring
NORMALIZE does not fire it.

## 2026-08-26 08:30 UTC+02:00 - M139 SURVIVED, and the fault is in my test

    M139 the sine table is asked for cosine and back again  *** SURVIVED ***

**A real gap, and the irony is exact.**

`zhao_field_sin` implements cosine by ADDING 90 DEGREES to the angle
(`a = is_cos_i ? angle_i + 16'h4000 : angle_i`). So inverting `is_cos_i` swaps
sine and cosine, which must change any rotation.

Except at one angle. I chose `ang = 0x2000` and wrote the comment "an angle with
both sin and cos non-trivial". In a 16-bit angle **0x2000 is exactly 45
degrees**, where sin == cos -- the single angle at which swapping them is
invisible.

    0x1000 = 22.5 deg  sin=+0.3827 cos=+0.9239
    0x2000 = 45.0 deg  sin=+0.7071 cos=+0.7071   <- what I picked
    0x3000 = 67.5 deg  sin=+0.9239 cos=+0.3827

I picked the one value that hides the defect, and justified it in a comment as
the opposite.

### The lesson, which is not "check your trig"

A "nice round" constant in a test is chosen for the tester's convenience, and
convenience correlates with SYMMETRY -- 45 degrees, zero, one, powers of two.
Symmetric inputs are exactly the ones under which distinct things become equal,
which is exactly what a mutation needs to hide.

The fix is not a better constant, it is SEVERAL: the section will sweep a set of
angles including at least one where sin and cos differ in magnitude AND one
where they differ in sign, so no single symmetry can cover the mutant.

That also generalises to what is already written: any test pinned to one
convenient value is one symmetry away from vacuous, and the sweep is the only
thing that finds it.

## 2026-08-26 09:00 UTC+02:00 - M139 closed; ROT's evidence complete

Three angles instead of one: 22.5 deg (sin < cos), 67.5 deg (sin > cos), and
112.5 deg (**cosine negative**), so no single symmetry covers the mutant. The
section went 79 -> **99 checks** and M139 is now CAUGHT.

    ROT sweep: attempted=63 accounted=63 caught=60 equivalent=2 survivor=1
    after the fix:            61 caught + 2 proven equivalent, 0 unexplained

**Zero discards this run**, on the first sweep after the retry landed.

The reason for the angle choice is now written into the test itself, because the
failure mode is reusable and the next person will reach for a round number too.

## 2026-08-26 09:30 UTC+02:00 - NORMALIZE2/3: ALL FOURTEEN FIELD OPS RUN ON v2

106 checks. The last two operations needed no new front-end machinery -- the
second read pass and the multi-result reply already covered them.

The one addition: **the integer square root got a SECOND CONSUMER.** It was
wired straight to `u_len` because the length family was the only caller. It is
now muxed on the captured unit id -- the same shape as the multiplier, licensed
by the same interlock, carrying the same recorded caveat that it is a mux and
not an arbiter.

### Section 18c is the check that actually matters

A mux that fed normalize by STARVING the length family would pass 18a and 18b
completely -- both only exercise the new opcode. So 18c runs NORMALIZE3 and LEN3
in ONE program and checks both: 3,269 clocks, both right. Two of the eight new
mutants attack that in each direction (M145 starves normalize, M147 starves the
length family).

### My non-degeneracy guard caught MY OWN TEST, again

`18.NORMALIZE2's answers actually vary` FAILED: 1 distinct value across 32
wavefront/lane pairs.

I generated vectors as `(idx+1) * (k+3) * 7717`, which scales one direction by
the lane index -- **the ratio stays 3:4 for every lane, and normalize discards
magnitude.** All 32 lanes normalised to the same unit vector.

That is the SAME FAMILY as picking 45 degrees for a rotation: a symmetry
introduced by the convenient way to generate inputs, not by the design. Twice in
one session, which is the argument for the guards paying their cost.

Fixed in all three sub-sections by varying DIRECTION rather than length, with
the reasoning written into the test.

### Eight mutants (M143-M150), seven older ones re-anchored

M114, M116, M128, M129, M137, M142 drifted (the count expression, the pass-2
predicate, the mul chain and the rcp0 fold all grew a normalize arm) and M145
orphaned a signal. Fifth time this session that editing a shared line
invalidated other mutants' anchors; the preflight caught all of them before the
sweep started.

## 2026-08-26 (later) - The sequencer FRONT END: written, lint-clean, NOT WORKING

`fpga/rtl/field/zhao_field_v2_front.sv` and
`tests/differential/field_v2_front_directed.cpp` exist and are **uncommitted on
purpose**. The differential FAILS. Nothing is registered in ctest, so `main`
stays green -- a red gate for everyone else is not an acceptable way to record
unfinished work.

### Rule 1 resolved cleanly

The oracle is the reference's own driving pattern: `zref::terrain`
(reference/src/zrender/terrain.cpp) walks a lattice and calls
`zfield::interpret` ONCE PER POINT, and `zfield.hpp` states the mapping -- "in
lanes map to R0.. in the program's input order; out lanes are read from the
output map at END." So the block owes: program in, point stream in, one answer
per point OUT IN ORDER.

### Two real bugs found and fixed on the way

1. **The ready/valid handshake was wrong.** `pt_ready_o` asserted on the FIRST
   of the n_in write cycles, so a correct producer advanced and lanes 1..n-1
   were written from the NEXT point. Ready now lands on the LAST lane cycle.
   Symptom looked like reordering, not like a handshake fault.
2. **The drain read was phase-ambiguous.** Rewritten as an explicit
   present/settle/capture walk rather than trying to match the register file's
   latency exactly -- one cycle more per lane, and unambiguous.

### The unresolved fault, stated precisely

Sections 1-4 still fail. Point 0 has in=(1000,1803); the program is
`ADD r20 = r0+r1` then `MUL r21 = r0*r1`, out map {20, 21}.

    want = (2803, 28)          2803 = a+b, 28 = a*b in Q16.16
    got  = (28, 1000)

Two single-lane probes on the same setup:

    out_regs = {20}  ->  got 28    (the value that belongs to r21)
    out_regs = {21}  ->  got 1000  (the value that belongs to r0)

**Those two are not consistent with any single off-by-one** on the register
address, the lane index or the output-map index, which is why I stopped
inferring. The next step is ground truth -- a VCD, or temporary debug outputs
exposing `rd_idx`, `drain_lane`, `h_rreg` and `h_rdata` per cycle -- not another
guess. I have made three timing guesses and each one moved the symptom without
explaining it, which is the signal to stop guessing.

What is NOT in doubt: the point COUNTS are right in every section (32, 5, 39,
32), so fill, batch launch, partial batches and the multi-batch loop all
sequence correctly. The fault is confined to which register a drained lane
reads.

## 2026-08-26 (later still) - FOUND IT: pc_o flaps to zero, and that is a CORE
## interface defect, not a front-end bug

I said the next step was a waveform rather than a fourth guess. It was, and it
paid: a per-cycle trace of the front end shows the fault in one line.

    c5: pc=0   c6: pc=1   c7: pc=0   c8: pc=2   c9: pc=0   c10: pc=3

`zhao_field_v2_core` drives

    assign pc_o = sel_valid ? pc[sel] : 8'd0;

and it issues on every OTHER cycle -- one instruction in flight per wavefront.
So on every NON-ISSUE cycle `pc_o` reads **zero**, and a front end that presents
`instruction[pc_o]` combinationally puts INSTRUCTION 0 on the bus half the time.

The core survives this internally because `s1_op`/`s1_dst` are captured at
`issue_fire` and `rd_*` are only consumed when `s1_valid`. But **`ins_is_long`
is read CONTINUOUSLY** -- it gates the long-op interlock -- so the interlock is
being computed from instruction 0 on alternate cycles.

### Why this is the core's problem and not the front end's

`pc_o` has no "this pc means something" companion. Zero is a legal pc, so the
front end cannot tell "wavefront 3 is about to issue instruction 0" from "nobody
is issuing". Any instruction memory driven from `pc_o` inherits the ambiguity.

The fix is an interface change to make deliberately, not a hack to bolt on:
either add `pc_valid_o` (= `sel_valid`) so the front end can hold the bus
steady, or have the core register the fetched instruction itself. The first is
smaller and keeps the instruction memory outside the core, which is the shape
the rest of the engine already has.

**This is exactly the kind of defect the front end existed to find.** The core
passed 109 checks and a 71-mutant sweep while carrying an output whose meaning
is undefined half the time, because every one of those tests drove `ins_*` from
a testbench that already knew which wavefront was issuing.

### State

`zhao_field_v2_front.sv` and `field_v2_front_directed.cpp` remain UNCOMMITTED
and unregistered; `main` stays green. Debug taps and the tracer are removed --
the front end still lints clean across 17 modules with them gone. Two real bugs
were fixed on the way (the ready/valid handshake, the drain phase walk) and both
are worth keeping when this resumes.

## 2026-08-26 - CORRECTION: the pc_o claim was WRONG

The previous entry, and commit 3de62fd, said the flapping `pc_o` was a core
interface defect that broke the long-op interlock. **That is wrong and I am
withdrawing it.**

Checking every use of the instruction bus in `zhao_field_v2_core`:

| use | line | gated by |
| --- | --- | --- |
| `ins_is_long` | 452 | feeds `issue_fire`, which is `sel_valid && ...` |
| `addr_a/b/c` | 489-491 | feed the read walk; `rd_*` consumed only when `s1_valid` |
| `s1_op/dst/a/b/imm` | 559-563 | captured inside `if (issue_fire)` |

`issue_fire = sel_valid && ...`, so on a non-issue cycle it is zero no matter
what `ins_is_long` says. Every consumer is gated. **The flapping is harmless.**

`pc_o` returning zero when nothing is issuing is still an interface wart -- a
front end cannot distinguish it from a real pc of zero -- but it is NOT the
cause of the failing differential, and saying it was is precisely the "false
proof" failure this project treats as worse than an open hole. I applied that
standard to a mutant's equivalence proof this morning and then failed to apply
it to my own root-cause claim eight hours later.

### What is actually known

* fill writes are CORRECT: the trace shows `we=1 reg=0 wd=1000` then
  `we=1 reg=1 wd=1803` into (wf0, lane0);
* the register file afterwards holds r20 = a*b and r21 = a, where the program
  says r20 = a+b and r21 = a*b;
* so a write carried the MUL's VALUE to the ADD's DESTINATION -- `s1_op` and
  `s1_dst` disagreeing, which the source says cannot happen since both are
  captured in the same `if (issue_fire)`.

That contradiction is unresolved. It means one of my assumptions about the trace
is wrong -- most likely that `pc_o == 0` in the trace meant "not issuing", since
I never tapped `sel_valid` and zero is also a legal pc. **The next step is to tap
`sel_valid`, `issue_fire`, `s1_valid`, `s1_op` and `s1_dst` directly** and watch
the two writes, rather than infer from `pc_o` again.

### Status unchanged

Front end and its differential remain uncommitted and unregistered; `main` is
green; all fourteen Field operations remain committed and evidenced.

## 2026-08-26 - The real cause: TWO OPCODE ENCODINGS. 7 of 10 now pass.

Tapping `sel_valid`, `issue_fire`, `s1_valid`, `s1_op`, `s1_dst` and `alu_y[0]`
-- the step I said was next -- answered it in one trace:

    cyc  s1_op s1_dst  alu_y0
      5   03     20      28      "ADD" produced a*b
      7   05     21    1000      "MUL" produced min(a,b)
      9   00      0    1000      "END" WROTE a register

### The cause

`zhao_field_v2_core` has its own compact ALU encoding, and it is NOT the
reference's:

    core:       MOV 0x00  ADD 0x01  SUB 0x02  MUL 0x03  MAD 0x04 ...  END 0xFF
    reference:  END 0x00  MOV 0x01  ADD 0x03  MUL 0x05 ...

They collide in the worst way: **every reference opcode is a VALID but DIFFERENT
core opcode.** Sending reference numbers does not fault -- it silently computes
something else. My front-end test used the reference numbers, so 0x03 became
MUL, 0x05 became MIN, and 0x00 became MOV, which is exactly what the trace shows.

The core's own differential avoids this by writing the DUT program in core
opcodes and the oracle program in reference opcodes. The front-end test now does
the same, with `to_ref()` as the single place the mapping lives and a `default`
that returns a deliberately invalid opcode so a newly added op cannot silently
inherit a wrong translation.

**The RTL was never wrong here.** Two of my three earlier diagnoses were also
wrong, and this is the third correction today; the pattern is that I inferred
from partial observations instead of instrumenting, three times, before
instrumenting settled it in one run.

### Result: 7 of 10 checks pass

Sections 1 (full batch), 2 (partial batch) and 4 (a second program) are green.

### The one real remaining bug, precisely localised

Section 3 (39 points = one full batch + 7) fails on exactly ONE point:

    first mismatch at index 32 (batch slot 0): want=73984 got=65792 prev=73728

Index 32 is the FIRST POINT OF THE SECOND BATCH. `got = 65792 = b + 256`, and
256 is point 0's `a`; point 32's `a` is 8448. Point 31 is correct, so batch 1 is
clean and the fault is at the batch RESTART: the first point of a new batch runs
with the previous batch's operand still in its slot.

That is a genuine defect in `zhao_field_v2_front` -- almost certainly the first
fill write of a batch not landing before the wavefronts are started -- and it is
mine to fix, not the core's.

### State

Front end and its differential remain UNCOMMITTED and UNREGISTERED; `main` is
green. The core was restored byte-identical after tapping (`git diff` empty) and
all taps and the tracer are gone.
