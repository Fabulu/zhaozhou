# Task Log: RUN-20260828-2111 - [Describe objective here]

**Created:** 2026-08-28 21:11 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-2111-spline-hot-and-ring-prep/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-28 21:11 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-2111
- Created working directory
- Initial context: [brief description]

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

## Rule 1 first: the oracle resolves, and it says what the lookup must fetch

`zfield::steps::exec_op` case `OP_SPLINE`, read before any RTL:

    i    = segment_search(t, src[0])          six compare/select steps, always
    a    = clamp_raw(src[0], x[0], x[n-1])    the table's own ends
    tt   = clamp(rescale((a - x[i]) * dy[i], 16), 0, 1<<16)
    p0   = y[i > 0     ? i - 1 : 0]           ENDS REPLICATED, not wrapped
    p1   = y[i]
    p2   = y[i + 1 < n ? i + 1 : n - 1]
    p3   = y[i + 2 < n ? i + 2 : n - 1]
    C1   = p2 - p0
    C2   = 2*p0 - 5*p1 + 4*p2 - p3            each ONE saturate to s32
    C3   = -p0 + 3*p1 - 3*p2 + p3
    u    = fx_mad(tt, C3, C2); u = fx_mad(tt, u, C1); v = fx_mul(tt, u)
    dst  = fx_add(p1, rescale_s32(v, 1))      the 1/2 of Catmull-Rom

**Everything from `C1` down already exists and is closed at 21/21** in
`zhao_field_v3_spline.sv`, which takes p0..p3 as operands precisely because the
lookup was always going to be a separate half. So this run builds the half
above the line, not the whole op.

## What the curve service actually looks like, before touching it

    tbl_ram[0:255]   96 bits per entry: {x, y, dy}
    two registered read ports, qa <= tbl_ram[ra_addr], qb <= tbl_ram[rb_addr]
    port A serves lanes 0 and 1, port B serves lanes 2 and 3, alternating on
      cyc[0] -- so one lane's registered-read wait is its partner's address
      cycle and both ports issue a useful address every search clock
    6 steps x 2 lanes x 2 cycles = 12 address slots per port; the 13th cycle
      consumes the final read and hands the group on. Designed II = 13.

Per-table meta registers (`meta_n`, `meta_x0`, `meta_xn1`, `meta_y0`,
`meta_dy0`) are latched at table load, which is why the search needs only its
six step reads: the clamp bounds and entry 0 are properties of the TABLE, and
the selected entry is captured ON THE WAY DOWN.

## The widening, costed before it is written

`x[i]`, `y[i]` and `dy[i]` arrive free -- they are the entry captured on the
way down. SPLINE needs **three more y values per lane**: `y[i-1]`, `y[i+1]`,
`y[i+2]`, each with the oracle's end replication.

    3 extra reads x 4 lanes = 12 addresses
    12 addresses / 2 ports  = 6 more cycles
    II 13 -> ~19, FOR SPLINE GROUPS ONLY

CURVE and DCURVE must be untouched by this, which means the extra phase is
entered on mode, not unconditionally. That is also the thing to check rather
than assume: the existing CURVE II is a fitted, measured number and a
regression in it would be a real cost paid for an op that did not need it.

**The clamping is the part to get wrong.** `i-1` clamps to 0 and `i+1`/`i+2`
clamp to `n-1` -- they do NOT wrap, and a table of two entries makes all four
control points collapse onto the same pair. That case belongs in the directed
test from the first commit, not added later when a mutant survives.

## Next, in order

1. Widen the service with the neighbour phase; keep CURVE/DCURVE's path and II.
2. Directed differential against `exec_op` -- including the two-entry table and
   both ends, where replication does all the work.
3. RE-SCORE CURVE.SVC's eighteen. They ride on the current state machine.
4. Join lookup to arithmetic as a spline service on the path.
5. SPLINE into `zhao_field_ops_pkg` -- one line.
6. Then the ring, and the two-service starvation measurement it unlocks.

---

## 2026-08-29 — the SPLINE lookup closes, 6930/6930

The neighbour phase is correct and the differential against the shipped
interpreter is green across every table, every knot, both neighbours of every
knot, every midpoint, and past both ends.

### The defect, and why it took three passes to see

`lookup_complete` fired at `ncyc == 6`. Addresses go out on cycles 0..5 and the
table's read is REGISTERED, so the datum addressed on cycle 5 does not land
until cycle 6 — as a non-blocking assignment, i.e. after the handoff on that
same edge has already sampled `s_p3`. The spline unit was therefore handed a
p3 that had not been written yet.

The phase is seven cycles, not six. This is the same "the thirteenth cycle
consumes the final read" that the search itself already pays, and I wrote that
sentence in the comment for the neighbour phase while getting the constant
wrong two lines below it.

### The symptom was actively misleading

* **96 of 6930**, so it read as an edge case rather than a structural error.
* Only on **midpoint** probes — the only ones where `t != 0`, so the cubic
  actually evaluates and a wrong p3 can move the answer. At a knot the answer
  is p1 regardless, so 3/4 of the probes passed while the bug was fully present.
* Only on the **64-knot** table, because on the small tables the clamps drove
  p3 to an index the previous group had already loaded.
* **The same probe passed in isolation.** The stale register held the correct
  value when the group before it happened to leave the right number there.

That last one is the finding. A value that is right when you test it alone and
wrong in sequence is not a flaky test — it is uninitialised state being read,
and the ordering dependence is the evidence, not the noise. I had already
recorded "one context is a DIFFERENT test, not a weaker one"; here the two
contexts disagreed and the disagreement was the whole diagnosis.

### What actually found it

Not reasoning — five temporary debug ports (`dbg_t3_o`, `dbg_p0..p3_o`) hauling
the lane-3 operands out to the C++ side, printed next to the oracle's. One line
of output settled it:

    DUT t3=1408 p0=7 p1=7 p2=1007 p3=-10345

`t` correct, `p0/p1/p2` correct, `p3` not a number in the table at all. That
localises the fault to one of six captures without a single guess about the
arithmetic. Hand-checking `t` first mattered — had `t` been wrong the whole
rescale path would have been suspect and the search much wider.

The ports and the C++ diagnostics are removed; the count drops 6948 -> 6930
because the DIAG group went with them.

### Standing

* SPLINE lookup + arithmetic: **green, 6930 checks**, lint clean at `-Wall`.
* NOT yet done, and none of it is assumed: CURVE.SVC's 18 mutants must be
  **re-scored** against the new shape (the phase is new logic and the old score
  does not carry), SPLINE into `field_long_width`, the service on the path,
  `UOP_RING_PREP`, and the two-service starvation measurement.

## 2026-08-29 — the sweep, 28 then 29

**First run: 26 caught, 0 discarded, 2 survived (exit 12).**

Before it could run at all, the preflight refused **C01 and C18**: their
anchors named lines the neighbour phase had since edited (`consume[0]` gained
`!n_busy`; `mul_issue_o` gained the SPLINE half of its mux). Both had been
scored CAUGHT in the 18/18 run. Carrying that number forward would have
reported two checks as passing while neither could be applied to the RTL at
all — a stale anchor is not a weaker test, it is NO test, and it looks exactly
like a passing one. Re-scoring was in the plan for the shape change; it paid
for itself on the tooling instead.

Also mine: I piped the first attempt through `tee`, so the shell reported
`tee`'s exit code and a run that aborted at preflight looked like exit 0. The
gate returned 1 correctly. Re-run without the pipe.

### The two survivors, and what proving them showed

Both are genuinely equivalent, and both proofs are recorded in `EQUIVALENT`
with the condition that would end them.

**S06** removes `!n_busy` from `consume[0]`. `cyc` stops incrementing at 12 and
the neighbour phase runs with it pinned there; `consume[0]` requires `cyc[0]`
and 12 is even, so it is already false for the whole phase. The guard is
redundant *on lane 0*.

That is the useful half: on **lane 1** the identical guard IS load-bearing —
`consume[1]` wants `!cyc[0] && cyc >= 2`, both true at 12. So the sweep had a
mutant for the guard that does nothing and none for the guard that does the
work. **S11** is that mutant, and the 29-mutant run exists to score it.

The redundant guard stays in the RTL. It states the intent instead of resting
on a parity argument about a constant three lines away.

**S08** stops `f_spl_offered` latching, so the group is offered every cycle.
The spline unit drives `v_ready_o = (state == P_IDLE)` and stays in `P_OUT`
until `r_valid_o && r_ready_i`, returning to `P_IDLE` on the same edge this
service leaves `F_SPL`. The cycle the unit is ready again is the cycle the
offer has already gone low, so the only cycle both are high is the intended
first one. `f_spl_offered` is defensive, not load-bearing — **given** `r_ready_i`
is tied to `1'b1` here, which is the re-score trigger.

I did not take "it survived" as evidence of equivalence. Surviving 6930 checks
plus a 400-case random lane is strong, but the argument above is what makes it
a declaration rather than a guess.

**Second run: 29 mutants, in flight.** It has to confirm S11 is caught — my own
equivalence text for S06 asserts that, and an unverified claim inside a proof
is the same failure the stale anchors were.

**Second run: SWEEP OK. 29 attempted, 27 caught, 2 proven equivalent, 0
survived, 0 discarded.** S11 is caught, so the claim inside S06's proof is
measured rather than asserted. CURVE.SVC is closed at 29 against the new shape.

## 2026-08-29 — the second service is on the path

The service path had exactly one service since it was built. It now has two:
the noise unit (NOISE2, RIDGE) and the curve service (CURVE, DCURVE, SPLINE).

### The order this had to be done in, and why it is not arbitrary

`zhao_field_ops_pkg::field_long_width` is what makes the executor OFFER an op.
Adding SPLINE to it is one line, and it was tempting to do first because it is
the smallest change on the list.

It would have rebuilt the exact deadlock that file exists to prevent: an op
offered by the executor, accepted by the dispatcher, and answerable by nobody
— a context parked forever with nothing timing out. **The service goes on the
path first; the table entry is the LAST step.**

The width itself was read from the oracle rather than reasoned about:
`zfield_decode.cpp` gives `OP_SPLINE` the same shape as CURVE and DCURVE,
`m = {1, {1,0,0}, 1, 3}`, whose leading field is the destination width.

### What the wiring actually decides

* **Tables come from outside.** CURVE/DCURVE/SPLINE read a knot table the
  PROGRAM supplies. The path now carries a `tl_*` load port straight through
  from the probe top, driven by the differential here and by the command
  stream in the finished machine. Inventing a table inside the service would
  have made the block look self-contained and been a lie.
* **Two services, one response port.** Both can hold an answer at once and the
  dispatcher takes one per cycle, so this is an arbitration and not a mux. The
  curve service wins: its answer sits in the finish registers of a pipelined
  barrel, so making it wait stalls a group behind it, whereas the noise unit's
  answer is its own last stage. The loser holds — a dropped response is a
  wrong VALUE reaching a register, not merely a slower machine.
* **Three bank claimants**: 0 rival/ALU, 1 noise, 2 curve. Under
  `PRIO_SERVICES_FIRST` the highest wins, so the curve service outranks the
  noise unit. **That ordering is a choice, not a consequence of being added
  last**, and it is the arrangement the starvation question is about. The
  number is NOT predicted here — the measurement is its own step.

### The test that had to change, and the one that had to be added

Section 6 asserted SPLINE is refused as unsupported. That was true and is now
false by design, so the expectation moved rather than being deleted: `OP_RING`
(0x21) takes the role, and it is not a convenient stand-in — the brief leaves
the varying-radius ring on the cold lane, so it is genuinely an op the table
does not know.

Section 6b is new and is the one that matters. The cubic is closed at 21/21 and
the lookup at 6930 checks, so what was unproven here is the PATH: 0x1B offered,
accepted, routed to the curve service rather than the noise unit, and the
answer landing in the right register. **That needs a value check** — "it
finished and no flag fired" would pass just as happily if the answer belonged
to somebody else. Four probes, spread below the table, on a knot, between two
knots and past the end, each compared against `exec_op`:

    probe 0  -> 458752  (7<<16, replicated below the first knot)
    probe 1  -> 720896  (11<<16, exactly on a knot)
    probe 2  -> 184320  (between two knots, the cubic actually running)
    probe 3  -> 131072  (2<<16, replicated past the last)

**102 checks pass**, up from 86.

### Two traps hit on the way

* The composed build FAILED and the shell then ran the stale exe, which
  reported 86 green including "SPLINE unsupported = 1" — the old answer, from
  the old binary, for a machine that no longer existed. This is the documented
  cmake/Verilator regeneration race. An explicit `cmake -S . -B` fixed it. The
  tell was that the number did not move after a change that must have moved it.
* Both consumers of the service path needed the new sources, not just the one
  I was building. `sweep_consumers.py` answers that question directly instead
  of guessing.

## 2026-08-29 — a shipped deadlock, found by the traffic nobody had run

The service path sweep scored **28 caught, 4 equivalent, 5 SURVIVED** (exit 12).
All five survivors were in the new logic, and writing the test that should
catch four of them found something much worse.

### The defect

**Two contexts running DIFFERENT long ops at the same time hang the engine
forever.** Not slowly — forever, with nothing timing out and no flag raised.

    1. Context A offers NOISE2. The group fills to 1 and A is marked `waiting`.
    2. Context B offers RIDGE. `same_group_c` is false, so `long_ready_o` is
       low and B is refused -- correctly, a group carries ONE opcode.
    3. But `waiting_r` is set ONLY for a context the dispatcher ACCEPTED, so B
       stays active-and-not-waiting.
    4. `flush_o = ~|(active_r & ~waiting_r)` therefore never asserts.
    5. The group of one never issues, so A is never released, so B is never
       accepted. They wait on each other.

**This is the fifth seam defect in this engine and it has the same shape as the
other four**: two blocks that must agree, with nothing forcing them to. The
dispatcher decides who may join; the executor decides whether anyone else
might; and "refused because the op differs" was invisible to the side computing
the second answer.

### Why it survived nine sweeps and two closed compositions

Every runner in the composed differential gives ALL contexts the same op. A
group therefore always either filled to four or was flushed, and the third
case — somebody asking who cannot join — never occurred. It is not exotic
traffic either: eight contexts run eight independent programs, so reaching
different long ops is the normal case, not a corner.

### It is NOT my second service

Worth proving rather than asserting, because I had just changed that area and
the obvious suspect was the new arbitration. The isolating run used **NOISE2
and RIDGE only** — both served by the pre-existing noise unit, one immediate,
nothing of the curve service involved. It deadlocked identically, 0 of 8
contexts finishing. The defect is in the shipped dispatcher/executor pair and
predates today.

### The repair, and why it is on the dispatcher side

    assign issue_now_c = (state_r == D_GATHER) &&
                         ((fill_r == 3'd4) || (flush_i && (fill_r != 3'd0)) ||
                          (long_valid_i && !same_group_c));

The dispatcher already knows both halves: `long_valid_i` says somebody is
asking, `same_group_c` says they cannot join. Closing the group there needs no
new agreement with anybody. Widening `flush_o` instead would have meant
teaching the executor WHY the dispatcher refused — another copy of the same
seam, which is what produced this bug in the first place.

`!same_group_c` already implies `fill_r != 0`, so no fill test is repeated.

**400001 clocks (hung) -> 191 clocks** on the isolating case, and the full
mixed traffic across both services now runs in 228 clocks with every answer
matching the oracle. 115 checks, up from 102.

### Section 7, which is the test that was missing

Eight contexts split across both services — SPLINE, NOISE2, DCURVE, RIDGE,
CURVE, SPLINE, NOISE2, DCURVE — all started before any finishes, two different
tables with one of them in a NON-ZERO slot, every answer checked against
`exec_op`. It exists because four of the five survivors needed it:

    W02  taking the handshake from the service that was NOT asked is invisible
         while both services are idle and therefore both ready
    W04  dropping the losing service's held response needs BOTH to be holding
         one in the same cycle
    W07  serving DCURVE as CURVE needs a DCURVE to exist, and no program in
         this file contained one
    W09  reading the table index from the wrong bits of the immediate is
         invisible while every table index is ZERO

The two tables are deliberately UNLIKE each other. If they agreed, reading the
wrong one would give the right answer and W09 would survive this section too.

### W06 is equivalent, and that is checked rather than assumed

`rsp_r1` unzeroed for a width-1 answer cannot be observed: the drain selects
`r1_r` only when `d_memb_r == 1`, and `d_memb_r` counts `0 .. width-1`, so a
width-1 op never reaches it. CURVE, DCURVE and SPLINE are all width 1. The tie
is defensive, like `f_spl_offered`. **RE-SCORE IF** any op routed to the curve
service ever has width > 1.

### Still open

The svcpath sweep must be re-run to confirm W02/W04/W07/W09 are now caught, and
the DISPATCH sweep's 28 mutants must be re-scored against the changed issue
condition with a new mutant for the third term. Neither is done yet and neither
is assumed.

## 2026-08-29 — the starvation question has an answer, and it is "it cannot happen yet"

The svcpath re-sweep: **30 caught, 5 equivalent, 2 survived.** W07 and W09 are
now caught, so section 7 did what it was written to do. W02 and W04 survived it
too — and chasing that produced the finding of the day after the deadlock.

### The two survivors are equivalent for one reason, not two

`zhao_field_v3_dispatch` runs `D_GATHER -> D_ISSUE -> D_WAIT -> D_DRAIN` and
back, with `svc_valid_o` asserted ONLY in D_ISSUE and `rsp_ready_o` ONLY in
D_WAIT. **Exactly one group is in flight at any time.**

So at every offer, the previous group has already responded and drained. Both
services are idle. Both assert ready. W02 swaps which one's ready is read and
picks between two signals that are both 1. W04 protects a loser that cannot
exist, because only one service can ever be holding a response.

I did not reach that by writing a third test. Section 7 already runs mixed
traffic across both services and they still survived it — the traffic was never
the problem, the assumption was.

### What this means for the question the owner actually asked

Fabian paid for the hot path partly to make the starvation question measurable:
"one service cannot starve anybody." Two services now exist — **and they still
cannot starve each other**, because the dispatcher never lets both work at
once. The bank's priority ordering I chose (curve above noise) is real silicon
and currently unobservable.

**So the honest answer is not a number.** The measurement as scoped would
report zero starvation and that would be true and useless — it would be
measuring the dispatcher's serialisation, not the bank's priority. Reporting
"no starvation observed" without saying why would be one of the most flattering
possible ways to be wrong.

The contention that DOES exist today is rival/ALU against whichever single
service is live, and that is already measured — the curve service's own bench
section 7 reports 24 groups under refusal with 11 refusals issued, answers
unchanged.

### What would make it a real question

A dispatcher that keeps more than one group outstanding — which is also the
change that would make W02 and W04 load-bearing, and is why both guards stay in
the RTL rather than being deleted as dead logic. Both equivalences carry the
same re-score trigger, in those words.

That is a genuine piece of engine work with its own consequences (tags, the
drain, response ordering) and it is NOT in this run's scope. It is the next
thing worth doing if the owner wants the starvation number to mean anything.

## 2026-08-29 — the dispatch sweep, D30, and a build-tree collision

**Dispatch: 29 caught, 1 survived, 0 discarded.** D29 — the deadlock itself,
reintroduced — is CAUGHT, so that defect now has a regression check.

### D30 and the kill that was not one

D30 drops the `long_valid_i` guard from the new issue term, so the dispatcher
acts on whatever the port carries while nobody is offering.

I was confident the fix was to assert the batch count in the composed
differential: a prematurely closed group means eight groups instead of two, and
that number was already being PRINTED and never checked. I added the assertion
and **the mutant passed it.** In that test all eight contexts carry the same op,
so the idle port always matches the group and the mismatch never arises.

Worth recording plainly: it looked like the obvious kill, the reasoning was
clean, and it was wrong. Applying the mutant and watching the test pass is what
settled it, not the argument.

Section 5d builds the condition directly instead — fill the group by one, then
present a DIFFERENT op with valid LOW for four clocks. Verified by applying D30
and watching it fail, then restoring.

The batch-count assertion stays on its own merits. It asserts the dispatcher's
whole reason for existing, and it had been sitting in the output unchecked.

### A build-tree collision, measured rather than theorised

Both the svcpath and dispatch sweeps defaulted to `BUILD_DIR=build-verify` —
the tree an interactive session runs ctest and hand builds in. Two writers, one
build directory: the exact collision the curve service's sweep was moved off
`build/` to avoid on 2026-08-27, still present in two other drivers.

It bit. The svcpath sweep was killed mid-run while I was using that tree, and
left behind:

* `build-verify` with a deleted target directory — a later compile failed on a
  missing dependency file, which reads like a broken toolchain and is not one;
* **a mutant still applied to the RTL** (W12).

`sweep_check_clean.py` caught the mutant. That guard exists because a killed
run put one into a pushed commit on 2026-08-28, and this is the second time it
has earned its place. Nothing was staged in that state — the check ran before
any add, and `git_add_safe` would have refused anyway.

Each sweep now has its own tree, and the reason is written into both scripts
where the next person will hit it.

### Standing

    curve service (CURVE.SVC)   29/29   closed, 27 caught + 2 proven equivalent
    service path (SVCPATH)      37      30 caught + 5 proven equivalent, needs
                                        one clean end-to-end run to record it
    dispatcher (DISPATCH)       30      29 caught + D30, now killed by 5d,
                                        needs the confirming run
    composed machine                    117 checks
    dispatcher's own bench              341 checks

Both confirming sweeps are running in their new dedicated trees. Neither result
is assumed.

## 2026-08-29 07:30 — DISPATCH closes at 30/30

**SWEEP OK. 30 attempted, 30 caught, 0 equivalent, 0 survived, 0 discarded.**

D30 is caught, which confirms section 5d rather than merely agreeing with it —
I had already applied the mutant by hand and watched it fail, and the sweep is
the independent second look. D29, the deadlock reintroduced, is caught too, so
the defect that hung the machine now has a standing regression check.

This is the first block in the family to close with NO equivalents at all.

### A process note against myself

This result landed at 07:30 and I did not commit it until 08:17, because I was
waiting for the svcpath sweep to finish so I could report both together. That
is batching, and the rule is explicitly the opposite: commit and push as the
work happens. Two independent results are two commits.

The cost is not hypothetical — a green sweep sitting uncommitted for 45 minutes
is 45 minutes in which a crash loses it and nobody outside this session can see
that the dispatcher is closed.

## 2026-08-29 08:2x — SVCPATH closes at 37/37

**SWEEP OK. 37 attempted, 30 caught, 7 proven equivalent, 0 survived, 0
discarded.**

W07 and W09 are caught by section 7's mixed traffic; W02, W04 and W06 are the
new equivalents, each with its proof and its re-score trigger. Both sweeps ran
in their own build trees, so neither could collide with the other or with an
interactive ctest.

### The run's blocks, as they stand

    curve service (CURVE.SVC)   29/29   27 caught + 2 equivalent   CLOSED
    service path (SVCPATH)      37/37   30 caught + 7 equivalent   CLOSED
    dispatcher (DISPATCH)       30/30   30 caught, no equivalents  CLOSED
    composed machine                    117 checks
    dispatcher's own bench              341 checks
    curve service's own bench          6930 checks

Every equivalence was declared AFTER a run that showed the mutant surviving,
never before one, and every one carries the condition that ends it.

## 2026-08-29 — every op the table offers is now actually served

The table gave NORMALIZE2/3 and ROT2/3 a destination width, which is what makes
the executor OFFER them. Nothing on the service path could compute any of them.
They fell into the noise unit's else-branch — `!is_curve_c` — were answered
with NOISE semantics, and the wrong number was written to a real register with
only `wrong_op_o` to say so.

**A raised flag beside a wrong value is not a safe failure.** It is the worst
outcome available: individually plausible, completely wrong, and discoverable
only by someone who thought to read the flag. Four of the nine ops the machine
advertised were in that state.

Both units already existed and were closed (NORMALIZE 26/26, ROT 24), and both
take PER-POINT sources only — so neither needed the uniform path that
UOP_RING_PREP is still blocked on. The work was wiring, not arithmetic.

### What changed

* Four services on the path now, not two: noise, curve, normalize, rot.
* **The noise unit is no longer an else-branch.** Every service is selected by
  its own predicate. "Not curve" is not the same claim as "is noise", and only
  one of those stays true when an op is added — that else-branch IS the defect.
* Five bank claimants, and `rsp_r2` is driven with something other than zero
  for the first time, because NORMALIZE3 and ROT3 write three registers.
* `wrong_op_o` is now the complement of the four predicates rather than a
  hand-written list of opcodes to exclude.

### Section 5 inverted

It used to assert that ROT2 RAISES `wrong_op_o`. That was true and correct to
check. It is now false by design, so the check inverts into the invariant that
actually matters: **run every op the table offers and demand the machine answer
it correctly.** Nine ops, each against `exec_op`, each result register checked.
If either list ever gains an entry the other lacks, this fails.

That also needed a third runner. The existing ones hardwire a = reg0, b = reg1,
destination reg2 — which collides with the SOURCES of a three-member operand,
so NORMALIZE3 and ROT3 could not be expressed in them at all. Nothing in this
file had ever run the two widest ops through the whole machine.

**150 checks pass**, up from 117.

### The mistake, because it is the instructive part

I wired ROT2's angle to `s2` and ROT3's to `s3`, reasoning from the ORACLE,
where `src[]` is flattened and ROT2's angle genuinely is `src[2]` while ROT3's
is `src[3]`. ROT3 passed; ROT2 came back wrong.

The hardware does not flatten. `zhao_probe_v3_exec` drives `long_s0/s1/s2` from
operand **a**'s three members and `long_s3` from operand **b**, whatever a's
width — so ROT2's angle arrives on `s3` with `s2` left idle. Both statements
were true and they were about different things: the same register, a different
index, in two different representations.

I had written a comment asserting the wrong one, confidently, citing the
oracle. **One op right and its near twin wrong is the shape of an indexing
error, not an arithmetic one** — and it was the invariant section running all
nine that made the pair visible at once.

## 2026-08-29 — the scalar bank's geometry, derived rather than chosen

Fabian's direction on prepared RING was option (a), the reusable uniform/scalar
path, with "the physical bank geometry is yours to derive and measure". So it
was measured.

### What the hardware actually has to do

`spec/form/cost-model.md` settles the biggest question: `uniform_ops` are
"executed ONCE per association **on the ARM**". The prep block is host work.
The silicon therefore needs only two things — a bank the host loads, and a read
path for the services. That is the same shape as the curve service's table
cache, which is already built and closed, so there is a proven pattern to
follow rather than a new one to invent.

### Why a single base index does not work

The planner allocates the prepared ring's scalars NON-consecutively:

    s_m  = alloc_slots(1)      the midpoint
    s_d0 = alloc_slots(1)      a temporary
    s_rA = alloc_slots(1)      the first reciprocal
    s_d1 = alloc_slots(1)      another temporary
    s_rB = alloc_slots(1)      the second reciprocal

with `s_r0` coming from wherever the source register already lived. The uop's
four scalar operands are `s_r0, s_m, s_rA, s_rB` — two temporaries sit between
them. So the instruction has to carry four indices, and the index WIDTH is what
the bank depth decides.

### The measurement

A standalone tool (no build tree, no RTL — it cannot collide with a running
sweep) plans every program in the corpus under six varying masks:

    WORST sreg_hwm = 41      impact_wave, mask 0
    Earth mask (x,z vary)    crater_ring 29, impact_wave 23, wave_pool 19
    all inputs varying       6 - 8
    vreg_hwm never above 26  (declared cap 32 -- consistent, a good sanity check)

11 programs decoded, 0 failed.

**The bank is worst when EVERYTHING is uniform**, which is the opposite of the
intuition that a busier program needs more scalars: a value that varies lives
in a vector register, and a value that does not becomes a scalar slot. Sweeping
the mask found that; planning only the Earth mask would have reported 29 and
undersized the bank by 40%.

### The geometry

    depth       64 slots      1.5x the observed worst, and a power of two
    index       6 bits
    ring operand encoding   four indices x 6 bits = 24 bits

**Twenty-four bits fits inside the existing 32-bit immediate with eight to
spare, so the instruction word does not have to grow.** That is the whole
reason to measure before designing: had the worst case come out above 64, the
indices would have needed 7 bits, 28 bits total, still fitting — but above 128
slots the immediate would have been too small and the instruction format would
have had to change.

Above 64 the hardware REFUSES rather than wraps, matching how this engine
already treats an unknown opcode: a width of zero means refuse, and a refusal
fails loudly where a wrap corrupts a value silently.

## 2026-08-29 — the four-service checkpoint, and an accidental acceptance test

**48 attempted, 39 caught, 7 proven equivalent, 2 survived, 0 discarded.**

The checkpoint the owner asked for before more integration lands. It caught
what it should: every routing predicate, both mode bits, the width-3 response
path, the axis immediate, the crossed bank grants, and X04 — the angle taken
from `s2` instead of `s3`, which is the indexing error I actually made and is
now a standing test rather than a lesson.

### The two survivors are one finding, not two

X05 (normalize and rot handed each other's requests) and X11 (rot's response
taken while normalize also holds one) join W02 and W04 in a single equivalence
class with a single root cause: **the dispatcher keeps exactly one group in
flight**, so two services are never active together. At every offer both are
idle and both assert ready, so swapping which ready is read picks between two
1s; and only one service can ever hold a response, so a loser-holds guard has
no loser.

Four mutants, one cause. That is large enough to be a statement about the
architecture rather than a list of excuses: **the response arbitration and the
ready mux across all four services are currently unexercised silicon.**

### Which turns them into the acceptance test for the next job

The owner has authorised a bounded multi-outstanding dispatcher precisely so
more than one service can be active. When it lands, **all four of these must
turn from equivalent to CAUGHT.**

If they stay equivalent, the dispatcher did not actually create concurrency,
and the two-service starvation measurement built on top of it would be
measuring nothing — which is exactly the "true and useless number" failure I
flagged when declining to produce a starvation figure earlier today. Now it is
impossible to miss: four pre-written tests fail to fire.

So the confirming green run is not being spent now. It happens after the
dispatcher, where the result is stronger than a green line today: not
"equivalent, proven" but "caught", which is evidence the machine changed.

### Standing after the checkpoint

    curve service (CURVE.SVC)   29/29   closed
    dispatcher (DISPATCH)       30/30   closed, no equivalents
    service path (SVCPATH)      48      39 caught + 9 equivalent (2 just declared)
    composed machine                    150 checks, all nine table ops served
    dispatcher's own bench              341 checks
    curve service's own bench          6930 checks

Nothing survives without a proof. Nothing was discarded.
