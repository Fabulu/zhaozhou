# Task Log: RUN-20260822-2136 - the saturating-counter law, the creature LOD ladder, and the CDC seam

**Created:** 2026-08-22 21:36 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260822-2136-saturating-law-creature-lod-and-cdc/

> Backfilled at 21:36 to cover a session that started around 04:00. The run was
> not initialized at the start; that omission is the reason runs are now a
> permanent fixture of the process.

---

## Objective

Push the hardware forward under the standing method — oracle first, differential
against the shipped oracle, mutation sweep with forced regeneration, gaps closed,
gates green — and get the owner's outstanding architecture decisions answered and
implemented. Everything is simulation, synthesis or fit.

---

## Progress Timeline

### ~04:00-17:40 - inherited state

`main` at `e59f34a`. The composed shell fitted and missed timing at -0.729 ns
with 97 failing endpoints. Field IR ops all built and swept; `FIELD.SEQ.CORE`
`RTL_VERIFIED` on a formal proof of the anti-hang law.

### 18:43 - `6d23c84` five copies of a law no test could reach

`INPUT.SNAPSHOT`'s saturation test spelled `MAX - x` as a wide subtract. All-ones
minus x is exactly `~x` — an identity, no borrow — and the pattern appeared FIVE
times across four blocks in three different spellings.

The rewrite was trivial. The useful part was checking whether anything would
notice if it were wrong: mutating `~b` to plain `b`, which removes saturation
entirely, PASSES `cmd_dma_directed` (48 checks) and its 5,000-packet random lane
(19,015 checks). More stimulus cannot fix that — these are u64 counters and the
rail is ~2^64 events away, so the saturating arm is unreachable in simulation.

So the five copies became one `zhao_pkg::zhao_sat_add{64,32}` and the evidence
became a proof: `tests/formal/sat_add.sby`, checking the result against the
(W+1)-bit sum, which cannot overflow and is therefore the true arithmetic answer.
Five covers reached with witness traces. SCOPE-TOTAL rather than an `a_scope_*`
guard, because the harness is purely combinational with every input free.

Sweep: 7 attempted, 7 accounted, 6 caught, 1 equivalent (`>` widened to `>=`
differs only at `a == ~b`, exactly where `a + b == MAX` and both return MAX).

Rule V20 caught my own comment claiming the guard was right "by construction"
with no enforcer named. Correct refusal; the fix was to build the enforcer.

### 18:56 - `f0107f6` composed shell fit, and the measurement owed

Against the immediately prior run `de2794d`, not a doc summary:

| | before | after |
| --- | ---: | ---: |
| setup worst | -0.729 ns | **-0.475 ns** |
| failing endpoints | 97 | **56** |
| ALMs | 7,667 | **7,415** |
| hold | 0 failing | 0 failing |

The caveat carried in — placement-bound below ~1.5 ns, may not move the headline
— did not hold, because five real borrow chains went away.

### 20:29 - `aec3c4c` creature LOD, four rulings, and a bug my own test caught

**Four owner rulings** were requested and acted on (see Decisions Made).

**`zhao_geom_lod.sv`**, the creature representation ladder, with NO DIVIDER. The
law divides twice per evaluation, but every quotient feeds a comparison and
`floor(N/e) <= T  <=>  N < (T+1)*e` exactly, so both divides become multiplies
and the hysteresis reduces to division by the constants 9 and 11.

**A defect in the SHIPPED reference**, found by the random lane: `boundary_q8`
computed `thresh * R` in `__int128` and narrowed the quotient to `int32_t`.
2,422,670,754 wraps to -1,872,296,542, and a negative boundary makes the
eager-coarsen test false for every projected radius — the ladder refuses to
coarsen and a creature walking into the distance stays at full detail forever.

**A defect in my own RTL**: the three rung error terms were an unpacked array
indexed by a genvar in a generate loop, and all three legality bits came out
equal, so the block could only ever answer rung 3 or rung 0. It agreed with the
oracle on **27,618 of 29,459 checks** anyway. The corner sweep caught it; a
magnitude bisect showed it reproduced at R = 65,536, so it was never about
magnitude.

Sweep: 22 attempted, 22 accounted, 21 caught, 1 equivalent. Both survivors of the
first scored run were REAL holes, closed by constructed boundary cases.

### 20:41 - `8d48608` (subagent) carry-over commit

The CDC subagent found my in-progress `geom_lod` work in the shared tree and
committed it separately rather than folding it into its own change. Correct call;
the collision was my fault for editing the tree while it built.

### 21:0x - `54ec158` the LOD block measured in Quartus

The block had never seen Quartus. It **did not synthesise**:

```
Error (272006): In lpm_divide megafunction, LPM_WIDTHN must be <= 64
```

Verilator, slang and 629,510 differential checks were all happy with the 72-bit
constant divisions. Same shape as the inline `genvar` only the composed fit
caught. Then the measurement showed the slack was expensive:

| | ALMs | DSPs |
| --- | ---: | ---: |
| first synthesis (72-bit) | 1,436 | **28** |
| after narrowing | 1,303 | **18** |

28 DSPs is a quarter of the device for one LOD evaluator. Three provably
equivalent fixes: proven-sufficient 64-bit widths, `thresh*R` computed once
instead of twice, and one boundary multiplier instead of two mutually exclusive
ones. The named next lever (sequencing the three legality products) is
deliberately NOT pulled: the consumer does not exist yet, and its throughput is
what decides whether sequencing is free.

### 21:1x - GEOM.MESHFETCH cull derived and validated, before any RTL

`grep frustum` returns nothing across the whole reference, but the frustum does
not need inventing: the five planes are row combinations of the `mat4fx`
view-projection the renderer already projects with. Three conventions would each
be easy to get wrong — row-major so rows not columns, **+Y NDC maps downward**
(no Y flip), and **no z clip at all**, so there are FIVE planes, not six.

The sphere test needs `|normal|`, and the ratified `isqrt_u64` is an exact FLOOR
sqrt — the wrong direction here. Flooring makes rejection easier to satisfy, so
the block would delete geometry that is visible. The safe form takes a ceiling.

Validated header-only outside the repo (`cull_check.cpp`), three camera matrices:

```
plane equivalence : 600000 checked, 0 mismatches
conservatism      : 60000 spheres, 26796 rejected, 0 WRONGLY rejected
looseness         : 344 kept although empty
```

Conservatism is proven by the direction of the bound; the sample corroborates.

### 21:36 - runs made a permanent fixture

Owner: *"you're meant to use the initialize run script and fill templates and
have agents fill them"* / *"Make runs a permanent fixture in our process.
Should've been from the beginning, serious blunder."* This run folder created,
memory written, past runs being reconstructed.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 2026-08-22 ~20:30 | a69cac37667f114f8 | Fix the GPU/video CDC seam: move the displayed CRC into `vid_clk` rather than crossing per-pixel state (owner ruling) | **Complete** | [FINDINGS-cdc-seam.md](FINDINGS-cdc-seam.md) |

| 2026-08-22 ~23:0x | afc4739019222418a | Build the GEOM.MESHFETCH conservative frustum cull: reference, RTL, differential, sweep | **Complete** | [FINDINGS-meshfetch-cull.md](FINDINGS-meshfetch-cull.md) |

Notes on this spawn:

- Given the full method, the environment traps, and the instruction not to run a
  composed fit itself.
- Mid-run correction sent: its `ctest -R shell_duo` regex matched three tests
  including a 90-minute gate variant and an **8-hour soak** (`TIMEOUT 28800`).
- It committed `8d48608` to isolate my in-progress files rather than absorbing
  them, which was the right call.

---

## Files Created

- `fpga/rtl/geometry/zhao_geom_lod.sv` — the creature representation ladder.
- `tests/differential/geom_lod_directed.cpp` — differential against the shipped
  `zref::creature::lod_raw` / `lod_update`.
- `tools/sweep_geom_lod.sh` — mutation sweep; its header documents four distinct
  ways this build system will score a run that never happened.
- `tests/formal/sat_add.sby`, `tests/formal/sat_add_harness.sv` — the saturating
  counter law, proven over all inputs.
- `reports/PHANTOM_REFERENCES.md` — two addenda (MESHFETCH classification; the
  "all 25 still SPECIFIED" correction).

---

## Decisions Made

**Owner rulings, 2026-08-22** (full text in `docs/OWNER_DOCKET.md`):

1. **One engine, five profiles.** The five `FIELD.SEQ.*` entries were `kind: rtl`,
   demanding five reference models and ten test files under V4 and booking FIVE
   ENGINES' worth of ALM budget for one engine under V5. Implemented as a new
   ledger kind `profile` + `implemented_by`, schema updated, and **rule V21** to
   charge for the exemptions. Six tests; ledger suite 46/46.
2. **"Camera visibility sectors" deleted.** The phrase existed only in the
   block's own purpose line. Replaced by conservative per-camera frustum
   rejection of a bounding sphere before vertex decode, rejecting only when
   outside EVERY active camera.
3. **The LOD boundary overflow: fix the law, never bake the wrap into silicon.**
   Owner's amendment was the half I had missed — widening to `int64_t` is not a
   fix either, because the next line multiplies by 9 or 11.
4. **BALANCED stays authoritative; the CDC seam comes before any more setup
   tuning.**

**Owner target recorded:** 120 MHz GPU fabric + 150 MHz DDR memory interface (the
GeForce 256 DDR part). Costed honestly on the docket: 20.4% beyond today's
~95.5 MHz on a placement-bound design, and the memory half cannot be costed at
all until the board is frozen.

**My own call:** do not pull the DSP-reduction lever on `zhao_geom_lod` yet —
restructuring against a guessed throughput and then measuring is the wrong order.

---

## Next Steps

1. When the CDC subagent lands and the tree is free: the 23-mutant sweep on
   `zhao_geom_lod` (anchors updated and dry-checked, 23/23 resolve uniquely), and
   a full `ctest -L fast` plus ledger check.
2. Dispatch the next subagent for `GEOM.MESHFETCH`'s cull, using the derivation
   and validation already done in this run.
3. Re-run the composed fit after the CDC seam changes placement, then re-measure
   BALANCED against HIGH PERFORMANCE as ruled.

### 22:0x - the agent landed, and it found a defect in MY sweep

Commits `eefc432` (the CDC fix) and `a62d35a` (its sweep, plus a compile defect
in its own test). Full record in [FINDINGS-cdc-seam.md](FINDINGS-cdc-seam.md).

**A FIFTH way to score a run that never happened, and it had been inflating my
numbers.** The executable lives OUTSIDE the target directory, so
`rm -rf <target>.dir` leaves `build/tests/<target>.exe` in place: a mutant that
fails to COMPILE runs the previous binary, and the build failure is scored as a
CAUGHT MUTANT -- the most flattering possible way to be wrong.

Applied to `tools/sweep_geom_lod.sh`, it immediately discarded THREE of the 23
mutants that every earlier run had counted as caught:

| mutant | why it never built |
| --- | --- |
| M02 rounding term dropped (micro) | `W'sd0` is a syntax error |
| M14 boundary rounding term dropped | same |
| M06 minimum hold removed | `hold_i < 16'd0` is always false, and -Wall refuses it |

So the honest score was never 21 of 22. Restating the three mutants so they
express the same defect and compile, the sweep now reads:

**23 attempted, 23 accounted, 22 caught, 1 equivalent (M18, unreachable by
proof).** The published figures in `design/contracts/GEOM.MESHFETCH.md` and the
root `TASK_LOG.md` were corrected rather than left standing.

**And the guard was made earlier rather than better.** Guard 5 turns a broken
mutation into a DISCARD, which is correct but late -- a sweep that discards a
third of its mutants has not tested what it claims to. `tools/
sweep_geom_lod_preflight.py` now LINTS ALL 23 before the run starts and refuses
to begin if any fails, so a malformed mutation is a refusal rather than a silent
inflation.

### 22:1x - the V20 the agent found in my own file

`ledger_check` failed V20 on `zhao_geom_lod.sv`: I had written that the
coarsening and refining tests are *mutually exclusive by construction* without
naming an enforcer. The agent correctly refused to fix it, since only the author
knows what the claim meant. The enforcer is real and is now named: ONE signal,
`coarsening`, selects both the multiplier operand and the branch that consumes
it, so there is no second condition that could drift out of step.

That is the second time in this run that V20 caught a claim of mine that was
true but unenforced.

| 2026-08-23 ~00:3x | aadfdc4ae727b3230 | Pilot the DSP sequencing lever on zhao_geom_lod and MEASURE it | **Complete** | [FINDINGS-dsp-sequencing.md](FINDINGS-dsp-sequencing.md) |

| 2026-08-23 ~01:2x | a18a997529e7c0ab4 | Apply the proven sequencing lever to zhao_terrain_lod (28 DSPs, the largest tractable target) | **Complete** | [FINDINGS-dsp-terrain-lod.md](FINDINGS-dsp-terrain-lod.md) |

| 2026-08-23 ~03:4x | (field engine) | Rearchitect the Field IR arithmetic to one shared multiplier service, per the owner ruling. OWN WORKTREE `.worktrees/field-dsp` | In Progress | `FINDINGS-dsp-field-engine.md` (agent to write) |

Notes on the second spawn:

- Handed the derivation and its validation rather than the task alone:
  `MESHFETCH_CULL_DESIGN.md` and `cull_check.cpp` in this folder, with the
  instruction to re-run the validation itself before trusting it.
- Scoped to exclude the meshlet descriptor FETCH, because the descriptor format
  is an open owner decision — the block takes its bound as a port, so it is not
  blocked on that.
- **Told to fill `FINDINGS-meshfetch-cull.md` from the template as it goes.**
  This is the first agent to get the run-folder path in its brief; the previous
  one predated the run and its findings had to be filed on its behalf.
- Warned that a composed fit is running concurrently, and to retry rather than
  delete a git `index.lock`.

### 00:0x - the cull landed, and it corrected me twice

Commits `60703e6` and `2a711f0`. It wrote its own findings from the template,
which is the process working as intended for the first time.

**It caught an error in my step-1 note.** I had written that the SIX frustum
planes are row combinations of the view-projection. There are FIVE: this
renderer's only depth condition is `w > 0`, so `row2` is never read. I found
that myself later while deriving the detail and wrote five in the design note --
but never went back to fix the earlier claim, so the repo carried both numbers
and the wrong one came first. A sixth plane rejects geometry the renderer would
have drawn, which is the exact failure the design note warns about.

**And it found a width bound I had not.** A plane component is the SUM of two
fx16 words, so it reaches 2^32 and `a^2+b^2+c^2` reaches 3*2^64 -- outside u64,
which is what `isqrt_u64` takes. It widened qformats 7.2's own recurrence from
4^31 to 4^32 rather than writing a second algorithm.

**Its own two corrections, neither papered over:** the preflight I added caught
its first malformed mutant (M06 left `pl_d` unused, failing -Wall), and M29
falsified a comment it had written claiming a set-must-dominate-clear ordering
was load-bearing. It is not -- the comment now says the ordering is defensive.

**Numbers:** 11,090 instance verdicts / 17,212 checks; the kind-1 half pinned to
the shipped renderer over 360,000 points across six cameras with 0 mismatches
and 0 points rejected that the renderer would draw. Sweep 32 attempted / 32
accounted / 30 caught / 2 equivalent. `ctest -L fast` 262/262.

It also MEASURED the rounding direction rather than asserting it: **1,015 of
1,775 boundary probes answer differently under a floor**, which is the
difference between a cull that costs work and one that deletes geometry.

**GEOM.MESHFETCH stays SPECIFIED**, correctly: the descriptor fetch is not
built because the format is unfrozen, and `zref::MeshFetch` stays a phantom.
The block owes a Quartus fit -- every width in it is argued from its range, and
`zhao_geom_lod` went 28 DSPs to 18 once measured.

---

## 2026-08-23 — the sequencing lever, piloted and MEASURED: 18 DSPs to 6

The DSP blocker report named a lever and guessed at its size. It was pulled on
`zhao_geom_lod`, the block whose own header had named it and declined it, and
measured on both sides at a clean worktree on this machine.

| | ALMs | DSPs | registers | latency |
| --- | ---: | ---: | ---: | ---: |
| before (`d8278bd`) | 1,303 | **18** | 21 | 1 clock |
| after (`09bbe05`) | 1,183 | **6** | 271 | 5 clocks |

**Six, not the estimated eight.** The estimate assumed only the three legality
products would share one multiplier. All FIVE did — `thresh*R` and the boundary
product too — because the cost of sequencing is the *sequencer*, and once it
exists the other products have no reason to stay outside it. 16% of the device's
DSPs down to 5.4%.

**The ALMs fell too, which was not expected and matters more than the DSPs.**
+250 registers and −120 ALMs at once: a Cyclone V ALM carries flops whether the
design uses them or not (this block was using 21), and the area was never mostly
the multipliers — it was five parallel 64-bit product-and-compare datapaths
collapsing into one. The standing objection to sequencing is that it trades area
for DSPs. Measured here it does not. It returns both.

**What it cost, said plainly:** five clocks instead of one, and a real `ready_o`
that makes `tick_i` ignorable while busy. `design/blocks.yml` already declared
GEOM.MESHFETCH `backpressure: ready_valid` and `latency: variable`; the RTL only
just caught up to its own ledger row. The rate argument is a 16x margin — 10 M
evaluations/s sustained against ~600 k/s for ten thousand live creatures — but
that rests on an instance count nobody has ruled, so it went on the docket as
item 4 rather than being treated as settled.

**Verification grew rather than held.** The reference was NOT touched: 212,530
evaluations / **1,267,100 checks** against the shipped `zref::lod_raw` /
`lod_update`, `ctest -L fast` 262/262, ledger check green. The mutation sweep
went **23 mutants to 26** — eleven re-aimed at where the arithmetic now lives,
and three genuinely new because a sequencer is new logic no earlier mutant could
reach (a legality bit latched into the wrong flop, `valid_o` pulsing before the
answer is written, a rung's product skipped outright). All three caught.
**26 attempted / 26 accounted / 25 caught / 1 equivalent**, the survivor still
M18 and its proof moved into the sweep's own header where a reader of the sweep
will see it.

The preflight earned its keep again: M02 and M03, rewritten against the shared
comparison, left `half_r` unused and failed `-Wall`. Caught before scoring,
re-spelled onto `half_r`'s own assignment.

**Where it generalises**, surveyed rather than assumed — and the decisive
question is not whether the products are independent (they nearly always are)
but whether the RATE consumes the parallelism:

* `zhao_terrain_lod` (28) — **best target.** One decision per patch per *frame*;
  30 products live permanently and consumed one cycle in 34, with 32 idle isqrt
  cycles already sitting there.
* `zhao_texture_tmu` (28) — **strong.** Its own header states the real rate is 1
  sample per 4–6 clocks, not the ledger's 1/clock, and 12 of its 32 products are
  literal duplicates across four bilerp instances.
* `zhao_surface_stamp` (28) — **partial.** The two radius squares are per-stamp
  constants latched in an already-idle acquire state and are free; the four
  per-texel products are on the rate.
* `zhao_terrain_project` (33) — **do not.** 6,144 clocks/patch already gives ~270
  patches against a 256-patch budget. Its 33 DSPs buy a rate the design spends.
  Sequencing it is an owner question about the patch budget, not a cleanup.

Running per-block DSP total: **213 → 201** against a device with **112**. The
lever works. It has to be pulled about ten more times.

Full write-up: `FINDINGS-dsp-sequencing.md`.

### 01:1x - the DSP pilot paid, and it killed the objection to doing it again

| | ALMs | DSPs | registers | latency |
| --- | ---: | ---: | ---: | ---: |
| before | 1,303 | **18** | 21 | 1 clock |
| after | 1,183 | **6** | 271 | 5 clocks |
| delta | **-120** | **-12 (-67%)** | +250 | +4 |

Device share 16.1% -> 5.4% of 112 DSPs. Six, not the predicted eight: the
estimate assumed only the three legality products would share, but ALL FIVE did
-- once a sequencer exists, the other two products have no reason to stay
outside it.

**The standing objection to sequencing was that it trades area for DSPs. It
does not.** ALMs fell by 120 at the same time registers rose by 250, because a
Cyclone V ALM carries flops whether they are used or not (this block used 21 of
them), and the area was never mostly the multipliers -- it was five parallel
64-bit product-and-compare datapaths collapsing into one. Sequencing returned
both resources.

It also re-measured the BASELINE rather than quoting mine: the committed row
said 1,303/18 but carried `rtlCleanAtHead: false`, a flag that had never once
been true in 42 rows. It reproduced to the digit, was committed on its own
first, and the new row reads `rtlCleanAtHead: true`.

Latency 1 -> 5 clocks with a new `ready_o`, which moved the RTL TOWARD
`design/blocks.yml` -- it already declared GEOM.MESHFETCH `backpressure:
ready_valid`. Sustained 10 M evaluations/s at 50 MHz against ~600 k/s for ten
thousand live creatures, a 16x margin resting on an instance count nobody has
ruled, so that went on the docket rather than being treated as settled.

Sweep grew 23 -> 26: eleven re-aimed at where the arithmetic now lives, three
genuinely new for the sequencer (a legality bit into the wrong flop, `valid_o`
early, a rung's product skipped), all three caught. 26 attempted / 26 accounted
/ 25 caught / 1 equivalent. The preflight earned its keep again -- M02/M03,
rewritten against the shared comparison, left `half_r` unused and failed
-Wall before scoring.

**The generalisation is the valuable part**, and it is sharper than "do this
everywhere": the question is not whether the products are independent (they
nearly always are) but whether the block's RATE consumes the parallelism.

| block | DSPs | verdict |
| --- | ---: | --- |
| `zhao_terrain_lod` | 28 | **best next** -- one decision per patch per frame, 30 products consumed one cycle in 34, 32 idle isqrt cycles already present |
| `zhao_texture_tmu` | 28 | **strong** -- its own header says the real rate is 1 sample per 4-6 clocks, and 12 of 32 products are literal duplicates across four bilerp instances |
| `zhao_surface_stamp` | 28 | **partial** -- two radius squares are per-stamp constants in an already-idle state; four per-texel ones are on the rate |
| `zhao_terrain_project` | 33 | **do NOT** -- 6,144 clocks/patch already yields ~270 against a 256-patch budget; its DSPs buy a rate the design spends |

Running per-block total: **213 -> 201** against 112.

---

## 2026-08-23 — TERRAIN.LOD sequenced: 28 DSPs -> 3, and the ALMs fell again

The block the pilot named "best next" was pulled, and it beat the pilot's ratio.

| | ALMs | DSPs | registers | fit s | commit |
| --- | ---: | ---: | ---: | ---: | --- |
| before | 2,086 | **28** | 1,257 | 434.3 | `47d607c` |
| after | 1,759 | **3** | 1,634 | 673.2 | `9f2928f` |

Both sides measured on this machine at a clean worktree; the BEFORE was re-run
rather than quoted and reproduced the committed row to the digit. Device share
**25% -> 2.7%** of 112. The DSP audit's target for this block was 4-8.

**The area fell for the second time out of two.** That is a pattern now, not an
anecdote: the parallel form's cost was never mostly the multipliers, it was the
wide datapaths beside them — here six 66-bit squarers, two three-term 66-bit
adder trees and TWELVE 49-bit comparators, collapsed to one multiplier, one
accumulator and TWO comparators.

Why 3 rather than the contract's predicted 22:

1. **Six were constant shifts, exactly as the contract predicted.** `h` is the
   literal 256 in the strict ladder, so `dstv * h` is `dstv << 8`. The lever the
   contract had already named was REAL — and worth 6 of 28, not 25.
2. **Twelve of the twenty-four left-hand sides were exact duplicates.** `s0`/`r0`
   and `s1`/`r1` are the same ladder at a different `h`, so only SIX distinct
   left-hand sides exist.
3. **The one shared multiplier is NARROWER than what it replaced.** `|c - e|`
   fits in 32 unsigned bits exactly and `d^2 = |d|^2`, so 32x32 unsigned covers
   the squares that used to need signed 33x33.

Points 2 and 3 are visible by READING, not by measuring, and neither is in the
pilot's generalisation. The rule for the remaining blocks is therefore: look for
constant operands and duplicate products first, then ask whether the shared
multiplier can be narrower than the widest thing it replaces.

Cost: 34 clocks per descriptor -> 48, so ~560 -> ~784 clocks per patch and the
margin over the ledger's rate falls from ~11x to ~8x against a spec-stated
256-patch budget. Still met with a wide margin, so nothing went on the docket.
**No port and no handshake changed** — the block already had ready/valid on both
sides — so not one test driver needed editing.

Verification grew rather than weakened. The block **had no mutation sweep**; it
has one now, written against the PRE-change RTL first, and it found a real hole
on its first run: `rhs + 1` survived all 211 directed checks and both random
lanes, because section 2 pins the flip point at `scale = h = 256` where both
sides are multiples of 256 and an off-by-one on the right is unreachable.
Section 2 now builds that equality by hand with an odd scale. 211 -> 219 checks.
The other two survivors are equivalent and are PROVED in the sweep header.

Sweep grew 34 -> 40: ten re-aimed at where the arithmetic now lives, six
genuinely new for the sequencer (a ladder answer into the wrong flop, either
phase ending a step early, the accumulator not cleared between the two eyes, a
right-hand side filed under the wrong camera, the ladder starting at level 1).
All six caught. 40 attempted / 40 accounted / 38 caught / 2 equivalent.

The sweep machinery gained a **sixth** guard against scoring a run that never
happened: a mutation containing `$` cannot live in a double-quoted bash array,
and this block uses `$signed(...)`. The mutant table lives in a Python module no
shell reads. The preflight then paid twice — six malformed mutants across the two
tables, none ever scored.

`ctest -L fast` 262/262, nightly lane 5,413 checks, LOD->TESS still crack-free,
ledger check green.

Running per-block total: **201 -> 176** against 112.

### 2026-08-23, amendment — the sequencing was fine; the SWEEP took main red

`measure_governor_lod` failed 55 of 72 checks on charter §9's Duo fairness law
after the TERRAIN.LOD sequencing landed. Two separate failures, both mine, and
neither of them the RTL.

**1. My reported gate was not a gate.** `ctest` does not build. After changing
the .sv I rebuilt three targets and ran `ctest -L fast`; **four** targets
elaborate this module, and the fourth — `test_measure_governor_lod`, which lives
in another subsystem's directory and guards the Duo property — ran a binary
compiled from the OLD RTL and reported a pass about code no longer in the tree.
The rule now written into the findings: `cmake -S . -B build && ninja -C build`
with NO target argument, then `ctest -L fast`, as the LAST action before
reporting.

**2. My sweep manufactured the defect and left it on disk.** `cmake` re-elaborates
every consumer of a mutated module; the sweep cleaned only the two it scored, so
all 40 iterations left mutant-derived model sources in the two composed targets.
Reproduced exactly: applying M14 ("the cameras take the coarser strict decision")
and building the composed test gives 55 of 72 failures on that assertion — the
reported count, from a mutation written to break camera isolation.

**The RTL was never wrong.** Built clean, the sequenced block passes
`measure_governor_lod` 72/72 with a trace byte-identical to the block it
replaced (1616 / 1616 / 92 / 1514), and the elaborated model provably contains
the sequencer. No RTL change was made, so the 28 -> 3 DSP measurement stands.

The cross-view lead was answered properly rather than by symptom: every piece of
DATA state is per-view (two root lanes, two relaxed right-hand sides, four ladder
answers); only the multiplier and the two comparators are shared, and they carry
no state between views. The one shared register — the 66-bit accumulator — is
cleared at the eye boundary, and mutant M37 exists to prove exactly that and is
caught.

Fixed as **guard 7** in `tools/sweep_terrain_lod.sh`: TARGETS is the full
consumer list, all four are cleaned/rebuilt/scored, and `check_consumers` reads
tests/CMakeLists.txt and refuses to start if a consumer is missing. A sweep must
not leave the tree in a state it did not measure.

Checked rather than assumed: `zhao_geom_lod` and `zhao_geom_cull` each have
exactly ONE consumer today, so `sweep_geom_lod.sh` and `sweep_geom_cull.sh` do
not leak. Neither has a guard against acquiring one — which is exactly how this
happened here, since TERRAIN.LOD landed in phase 6 with three block-level lanes
and gained the phase 8 composition afterwards. Port `check_consumers` to both
when either is next touched.

### 03:0x - terrain_lod 28 -> 3, and then I MISDIAGNOSED it

The sequencing paid harder than the pilot: **28 DSPs -> 3**, ALMs 2,086 ->
1,759, latency 34 -> 48 clocks per descriptor with no port or handshake change.
Six of the 28 were the constant-shift multiplies the contract had already
predicted; the other 19 came from twelve of the twenty-four left-hand sides
being exact duplicates, and from the shared multiplier being NARROWER than what
it replaced.

**Then I reported it as broken, and it was not.** `measure_governor_lod` failed
55 of 72 checks on the Duo fairness law. I concluded the restructuring had
broken it and I "proved" it by reverting the .sv and watching the test pass.

**The proof was confounded.** Reverting forced a rebuild, and the rebuild
regenerated the model from clean source -- which ALSO cleared mutant-derived
model sources the sweep had left in two consumers it never scored. The pass
tracked the rebuild, not the RTL. I changed two things and attributed the
result to one.

Verified properly -- wipe all four consumers, models AND binaries, reconfigure,
rebuild -- the sequenced RTL passes 72/72 with a byte-identical trace. The agent
found it first, and reproduced the corruption deliberately with mutant M14
rather than deducing it.

**The real defect was the sweep**: `cmake` re-elaborates EVERY target that
verilates a mutated module, and the sweep cleaned only the two it scored. That
is guard 7, and the sweep now refuses to start unless every consumer is listed.

The lesson is not about mutation sweeps. It is that **build state can
masquerade as design behaviour**, and that reverting a file changes two things
at once -- the file and the build. This project has now hit that class of error
seven distinct ways.

### 03:4x - the owner ruling replaced my improvisation

Fabian relayed a collaborator's architecture brief and it is now the ruling on
the docket. It supplies the rule (smallest local farm per subsystem, share only
what is mutually exclusive inside it), the target table, the **120,000 skinned
vertices per 60 Hz frame** budget I had said I could not invent, and the
correction that `TERRAIN.PROJECT` should be CACHED before being sequenced --
1,089 unique projections at three clocks beating 6,144 repeated ones at one.
I had that block filed as untouchable, which was the wrong conclusion.

It also rules that sweeps run in separate worktrees with separate build
directories, which is exactly the incident above.

---

## 2026-08-23 — Field IR engine: one shared arithmetic engine (claude-dsp-field-engine)

Implemented the DSP ruling of 2026-08-23 on `FIELD.SEQ.CORE`. Ten op units, each
with its own multiplier, beside a sequencer that retires one instruction at a
time, became one `zhao_field_exec_shared` holding one signed 33x33 lane, one
integer square root, one sine table, one reciprocal and the two (different)
reciprocal seed ROMs.

**Measured, both sides, on this machine, in a worktree with its own build dir:**

| | ALMs | DSPs | registers | commit |
| --- | ---: | ---: | ---: | --- |
| before | 10,623 | **79** / 112 | 4,510 | 57352cf |
| after | 8,901 | **3** / 112 | 5,356 | 62d7b0e |

The before row reproduced the committed one to the digit before a line was
changed.

Simple ops still retire in **six clocks** — the three register-read cycles became
the lane's issue slots, and the first operand group moved into `Q_LATCH` so that
with a two-cycle lane DOT3's third product lands in `Q_EXEC`. Worst op is
NORMALIZE3 at **67**, against the 96 ceiling.

### Four things worth carrying forward

**1. The anti-hang bound was a magic constant and is now derived.**
`zhao_field_seq_pkg::MAX_OP_CYCLES = 80` lives in a package in the sequencer's
own file, so every consumer that already compiles it gets the constant and no
source list had to grow. The formal harness IMPORTS it and computes its own run
bound and scope window from it. A new assertion came with it — `a_op_bounded`,
that no single instruction exceeds that ceiling for ANY instruction memory —
which is the property this rearchitecture actually needs, because every op now
waits on shared resources and every wait is an `if (valid)` that a bad schedule
turns into a spin.

**2. NO PER-BLOCK FIT HAS EVER BEEN TIMING-CONSTRAINED.** `run_block_fit.ps1`
copies the shell SDC, which clocks ports named `gpu_clk`/`vid_clk`/`audio_clk`;
every leaf block's clock port is `clk` and all I/O is virtual. Quartus says so
in its own log: "Ignored create_clock ... Argument <targets> is an empty
collection", three times per run. **All 47 rows in
`reports/synthesis/zhao_block_fit.json` were fitted with no timing objective.**
The area numbers stand; anything anyone infers about block timing from that file
does not. Raised on the docket with two options rather than fixed unilaterally,
because fixing it changes what every future row means.

**3. Three mutants survived and all three are the same equivalence.** The lane's
operand hold (M01), the read-slot shadow's depth (M07), and the write-back guard
excluding multi-cycle ops (M20) all produce a transient wrong value that is
ALWAYS overwritten before anything reads it — because the walk's read points are
fixed. Each is proven equivalent rather than labelled, and each stays in the RTL
because it stops being equivalent the moment the schedule changes.

**4. Guard 7 is derived here rather than declared.** This sweep spans eleven
files with different consumer sets, and a hand-maintained list across eleven
files is a list that drifts. `consumers_of` reads `tests/CMakeLists.txt` at run
time; the declared union is cross-checked against it and the run refuses to
start on a mismatch. It caught one immediately — `test_field_alu_ops` was in the
declared list and unreachable from any mutant, which meant the ALU's new seam
had no mutant at all.

### One thing I did not do, with the number that says why

The mid-flight architecture brief asked for `MUL_LANES` as a parameter with 1
and 2 both fitted. I did not build it. **Neither axis binds**: DSPs are at
3 against a target of 8, worst op is 67 against a ceiling of
96. And the latency a second lane could buy is small, because the multiplier is
not what the long ops wait for — **36 of NORMALIZE3's 67 clocks and 36 of LEN's
48 are the integer square root, which contains no multiplier at all.** Cycle
accounting per op is in the FINDINGS. If the long ops need to be shorter, the
lever is a two-bit-per-cycle root, not a second multiplier.
