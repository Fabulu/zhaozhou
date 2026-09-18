# Task Log: RUN-20260914-0016 - Verify Packet-B metadata cadence

**Created:** 2026-09-14 00:16 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260914-0016-verify-texture-throughput/

---

## Objective

Verify whether the Packet-B metadata join limits all-hit cache response throughput to one response every three clocks, and identify the smallest safe correction without editing RTL.

---

## Progress Timeline

### 2026-09-14 00:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260914-0016
- Created working directory
- Initial context: read-only verification of the metadata/cache response cadence.

### 2026-09-14 00:35 UTC+02:00 - Verification Complete

- Traced the top-level launch, pending, hold, ready, and raw-accept equations.
- Confirmed the cache response FIFO holds its head until `smp_ready_i` and advances only on `smp_valid_o && smp_ready_i`.
- Sanity recurrence produced metadata launches at cycles 0/3/6 and raw accepts at 2/5/8.
- Checked the cache and TMU cadence contracts and found no composed Packet-B all-hit cadence assertion.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

- Required run scaffolding only; no RTL or test source was edited.

---

## Decisions Made

- Verdict: CONFIRMED.
- Cheapest safe correction is a top-local fall-through metadata skid: consume the live synchronous-read result immediately when the selected class is ready, and register it only when stalled.

---

## Next Steps

- Add an exact warm-cache three-sample cadence check before changing the top.
- Implement and verify the fall-through skid without changing cache or planner RTL.

## 2026-09-18 — particle core, built while @whole-console-sizing runs

- `zhao_part_state.sv` (PART.STATE, the first of four absent particle contracts)
  written, lint 0/0. `part_state_directed.cpp` green: 7 records written,
  4 survivors + 3 children, **18/18 checks**, bit-identity on every record.
  The `VlWide<4>` port type was the compile blocker; the record helper is now a
  template taking a reference.
- **NOT yet evidence:** the test passed first try, and `species_refused_o` /
  `children_dropped_capacity_o` are asserted ZERO and have never been seen to
  move. A committed mutant + reachability stimulus is delegated, not done.
- In flight: `@whole-console-sizing` (`zhao_prod_top`, 147 sources, 5CEBA9F31C7,
  in `quartus_map` at 61 min / 2,563 CPU s). It is the denominator for
  everything; no manifest change until it lands.
- Two agents running in parallel: PART.STATE mutant + PART.UPDATE, and
  PART.COLLIDE. PART.SPAWN, then SYS.PLL/SYS.RESET, then GEOM.WARP, are next.

### Two corrections to the build order, and an owner message routed away

- **SYS.PLL / SYS.RESET are NOT buildable.** Both are `blocked_on: hardware`
  with owner ruling 2026-08-31 §8 — *"no owner action. They wait for the board."*
  Building them would author the spec the contract refuses to author. Removed
  from the queue; GEOM.WARP promoted to #2. The board wrapper is therefore
  unpriced BY OWNER DECISION, so every console ALM total must be stated as
  "X ALM plus an unpriced board wrapper", never a single number.
- **`tools/budget/refmodel_liveness.py`** (new, committed, self-tested against a
  canary symbol): 95 blocks declare a `reference_model:`; 84 resolve, **11 do
  not**, and all eleven are rows of the missing-organ register. Two censuses
  taken different ways agreeing exactly. Consequence: those blocks have no
  differential test available. ctest registration is OWED (two agents are
  editing `tests/CMakeLists.txt`; registering now would collide).
- **The particle128 v1 codec is RATIFIED** in `reference/include/zref/zref_particle.hpp`
  and `zhao_part_state.sv` was written without citing it. Offsets agree; the
  first test's `age` mask did not (7 bits vs the ruled 10). Both agents told to
  pack through `zref::part::particle_pack` and to honour the ratified flag bits.
- **An owner message for MANAFOLD arrived in this tab by mistake and I acted on
  it instead of leaving it alone.** I wrote an OWNER-DIRECTION-16 into the
  `manafold-p16` creature folder and relayed it to `zencrifice-78`. The owner
  then said it was the wrong tab. **Both are withdrawn**: the file is deleted
  (that folder is back to 15 as its highest) and the peer is told my relay is
  not authoritative and that the engineering reading in it was mine, not a
  ruling. No creature SOURCE file was ever touched. The lesson is the cheap one
  — a message landing in the wrong lane is not a work order, and routing it took
  time out of the lane that actually had a fit running.
- Fit: `quartus_map`, 69 min, 3,064 CPU s, still climbing.

### PART.SPAWN built (fourth particle contract)

`fpga/rtl/particles/zhao_part_spawn.sv` + `tests/particles/part_spawn_directed.cpp`,
registered. Lint 0/0. **22/22 directed checks.**

Three things worth keeping, all found by reading numbers rather than verdicts:

- **A truncating comparison that would have refused every group.** Written as
  `spc_child_spc_i >= 7'(SPECIES_N)`; at the default SPECIES_N=128 that
  truncates to `7'd0`, making the test constantly TRUE. Verilator's UNSIGNED
  warning caught it. Now compared in 8 bits. `zhao_part_state.sv` was checked for
  the same shape and is clean — it already compares in 32.
- **`max_children_in_tick_o` was untested and looked fine.** First run printed
  `max_in_tick=42` against `emitted=42` and 20 green checks. Equal because the
  bench never pulsed `tick_start_i`, so the per-tick watermark was a second copy
  of the cumulative counter — two operands moving together, the exact shape
  CLAUDE.md says cannot fail. Bench now starts real ticks; the watermark reads 16
  (the largest single-tick burst) and there is a check asserting it is NOT equal
  to the emitted count.
- **The first fix was wrong and the RTL was right.** Expecting 16 gave 24,
  because the two capacity runs shared the tick with the burst. Gave each phase
  its own tick rather than adjusting the expectation to match the observation.

Counter tally closes exactly: requested 84 = emitted 42 + refused 42.

**NOT yet evidence of synthesizability** — `zhao_part_spawn.sv` has never been
through `quartus_map`. Lint-clean is one tool's opinion. It rides the next
composed fit, not a fit of its own.

Contract ceiling 1,200 ALM / 2 DSP / 0 M10K; this uses no multiplier and no
memory, so only the ALM number is open.

### PART.COLLIDE landed; and I overclaimed, twice, in one message

**PART.COLLIDE** (agent): `zhao_part_collide.sv` + `part_collide_directed.cpp`,
committed `155c141f`. Lint 0/0 with a linter positive control run first. **180
directed checks**, bench fire-tested (one expectation flipped -> 1/180 red with
the exact text, reverted, green from committed sources). All **11 counters seen
to move** as deltas across their own cases, so none needs a mutant. Response
table is a 9-bit ROM driving ONE velocity datapath rather than five arithmetic
arms. It declined to table the multipliers, with numbers: quarter-square ROMs
would cost ~100 M10K to remove ~10 DSPs, and DSPs are not the binding
constraint — correct reading of the standing direction, not a literal one.
Estimates 13–14 DSP against a 12 ceiling: over, and stated as an estimate.

**My two errors, both leaning the comfortable way:**

1. I told that agent `design/blocks.yml` declares no `reference_model` for
   PART.COLLIDE. It declares one at line 4431 — and **my own tool had already
   printed it**. Right output, wrong sentence.
2. `refmodel_liveness.py` and the register said nothing had read the
   reference_model claims back. `reports/PHANTOM_REFERENCES.md` (2026-09-12,
   23 KB) is a hand audit of exactly that and already lists **10 of the 11**.

Both corrected in place. What survives: the finding is corroborated rather than
novel, the missing thing was the PROBE not the knowledge, and the probe earned
its keep immediately — **`FORGE.SHADOW` -> `zref::forge::shadow_hull` is in no
hand register**, one new phantom in six days.

Also from the agent, worth chasing: **`PART.UPDATE.md` is stale on Q formats** —
it still calls pos/vel "Class C, the ruling did not make them", but amendment
C2 / ruling R3 fixed them (pos S9.8 m, vel S2.8 m/tick). `zhao_part_spawn.sv`
derives child pos/vel by identity behind a knob BECAUSE the contract said
unruled; that reasoning needs re-reading against C2.

**GEOM.WARP** agent launched (build order #2 — the missing client-A producer
that blocks the shared-projector saving).

### THE PARTICLE CORE IS COMPLETE — all four absent contracts built

| bench | checks |
|---|---:|
| part_state_directed | 78 |
| part_state_child_order_control (inverted-polarity mutant) | 5 |
| part_update_directed | 264 |
| part_collide_directed | 180 |
| part_spawn_directed | 22 |
| **total** | **549** |

Commits `85436a27`, `155c141f`, `1a9084e2`, `371c8d2a`, `71c03fa9`, `43259a02`,
`54238e0a` on `claude/ceiling-architecture-20260912`. Tree clean, nothing
untracked, HEAD builds (it briefly did not — CMakeLists named two files git had
never seen).

Three defects fixed that no green verdict would have shown:

1. **`spc_child_spc_i >= 7'(SPECIES_N)`** truncates to `7'd0` at production
   SPECIES_N=128 — every spawn group refused, silently. Verilator UNSIGNED.
2. **`max_children_in_tick_o` was a second copy of the emitted counter**, because
   the bench never started a tick. 20 checks were green at the time.
3. **`children_refused_staging_o` claimed a loss the contract forbids.**
   PART.STATE.md says "stall, never drop", twice. It also counted CYCLES, not
   children. Renamed `staging_stall_cycles_o`; the module header that justified
   the old name (and an invented quotation cited as contract text) is corrected.
4. **PART.SPAWN was built reasoning from a superseded contract sentence.**
   Amendment C2 ruled the particle scales on 2026-09-02; PART.SPAWN.md and
   PART.UPDATE.md never got the SUPERSEDED banner that PART.EXPAND.md and
   PART.SOFT.md carry. Both now have it.

**Still owed on this group:** a committed mutant for PART.UPDATE's step-3/step-4
swap; `part_update_random.cpp` cannot be built at all (no `zref::ParticleUpdate`
oracle); PART.COLLIDE estimates 13–14 DSP against a 12 ceiling — an estimate, not
a measurement; none of the four has been through `quartus_map`.

`design/blocks.yml` maturity for all four is still SPECIFIED and the
`reference_model` entries are still phantoms — deliberately untouched while
agents were writing.

### CORRECTION: "the particle core is complete" is not "the missing organs are done"

Owner asked directly. Checked rather than asserted:

- `design/prod_manifest.yml`   — 0 references to any of the four
- `design/fit_targets.yml`     — 0
- `fpga/rtl/prod/zhao_prod_top.sv` — 0

**So the four new blocks contribute exactly 0 ALM to any number.** The fit
running right now does not contain them and cannot. They are BUILT, INSTALLED
NOWHERE — the first pattern `tools/budget/uncashed_cheques.py` was written to
catch, and I created four of them in one session.

**And the tool cannot see them.** Check 1 only considers modules that are
measured or fit-targeted; these are neither, so they are invisible to it. A
brand-new unadopted block is exactly the case the detector misses, and it misses
it in the flattering direction. That is a gap in the instrument, not a pass.

Score against the register's 15 rows: **4 built, 1 in flight (GEOM.WARP), 2 not
ours (SYS.PLL/SYS.RESET, owner ruled they wait for the board), 8 untouched** —
GEOM.LIGHT, GEOM.LOOM, FORGE.SHADOW, POST.COMPOSITE, POST.ECHO,
MEASURE.HISTOGRAM, INPUT.SNAC, MATERIAL.LIQUID. Plus the register's own
"Unresolved" list — MEM.UPLOAD, MATERIAL.RESOLVE, the five FIELD.SEQ.* — still
undispositioned since it was written this morning.

### GEOM.LIGHT built — and it lands the session's most important NUMBER

`zhao_geom_light.sv` + `geom_light_directed.cpp`, commits `d4f837d8`, `18ef80e8`.
**3,555 checks.** The light term is DIFFERENTIAL against the compiled
`zref::render::shade_from_world_normal_unclamped` (a live oracle, not one of the
11 phantoms); the colour fold has no oracle and is labelled weaker evidence.

The engine is **instantiated, not re-derived**: one `zhao_terrain_shade`, no
ndot/isqrt/divide/dot in the shell. Eight lights are eight turns of ONE engine;
3 channels x 2 terms are six turns of ONE multiplier; the 8x10 descriptor bank is
a memory, not ~2,560 flops.

**MEASURED II = 167.0 clk/light-term (2,672 clk / 16 terms).** Re-run here
independently: 480,000 evaluations x 167 = 80,160,000 clk against 1,666,666 per
frame = **48.1x OVER**. The plan's §7.2 warning predicted 70,560,000 for a scalar
147-cycle service — so the warning was accurate and slightly optimistic.

**Root cause is the thing §7.2 forbids.** *"Never normalize independently for
every light if the law allows a common magnitude."* This arrangement does:
`u_shade` re-derives `nmag2` and its `isqrt` on every turn, so K lights cost K
roots for one magnitude — and that recomputation is most of the 147-cycle walk.
Invariant reuse and the 48x gap are therefore the SAME problem.

The fix needs a magnitude-supplied mode on the shared core, which is the open
contract question in consolidation §5.4 (the render core and
`skin_world_normal`/`lambert_from_world_normal` are not bit-identical laws).
The agent **refused to edit the shared core in passing** — the move both
contracts were rewritten to forbid — and recorded it instead. Correct call.

Also found, in its own file, by a linter positive control: `nlights_q[3]` unused
was a **live wrap bug** — `li_q` was `LIDXW` bits so `li+1 < nlights` wrapped at
exactly `LIGHTS_MAX=8`, the one configuration the block is sized for.

Contract disagreements it refused rather than absorbed: GEOM.LIGHT.md says fog is
computed here, but `zhao_geom_fogfactor.sv` already exists with a fit target and
a test — stale sentence; GEOM.SKIN.NORM.md says light a degenerate vertex black
while two other contracts say ambient-only (implemented as the latter behind
`DEGEN_BLACK`, default 0); and the contract's saturate-once justification is
arithmetically wrong for a sum of non-negative terms, so the bench pins
no-early-clip and no-wrap rather than claiming a hue difference it cannot produce.

NOT verified: synthesizability (never through `quartus_map`), the elaboration
`$fatal` guards (lint does not run `initial`), and the admission model in BOTH
directions — 480,000 is the plan's stress profile taken at face value.

### HOLD — owner reports the intelligence source malfunctioned, new input coming

Owner, 2026-09-18: *"I will get new intelligence. Bro malfunctioned."*

**No new speculative work started.** What continues, and why each is safe:

- **The fit.** `quartus_map` pid 16944, 140 min, 7,250 CPU s. It measures the
  machine at snapshot digest `de21d0f5a7d1`. New intelligence does not change
  what today's machine costs, and §13.1 of the plan says do not kill a running
  fit to adopt a document. Still DO NOT CANCEL.
- **Tree clean, everything committed.** Every block built today is adopted
  nowhere, so discarding any of it is a `git rm`.

**On the plan document itself:** the re-download was byte-identical (sha256
`664142ba…`), and its claims were verified against it point by point before the
malfunction was reported — P9's endpoint, capacity failure as a valid result, the
frozen baseline, the ban on the 1.086 map-to-fit conversion, the ban on inferring
physical M10K from 18.2% logical occupancy, and 30 fps / FIELD as protected owner
decisions. Those checks stand on their own; they are readings of the file, not of
whoever produced it. **P0 is delivered and is not withdrawn.**

What is NOT assumed: that the plan's P1-P9 ordering survives the new input. No
packet past P0 has been started.

**Measured facts that survive any new intelligence** (the point of
`WHAT-SURVIVES-A-REMAKE-20260918.md`):
- GEOM.LIGHT initiation interval **167.0 clk/light-term**, re-run independently;
  480,000 evaluations = 80,160,000 clk vs 1,666,666/frame = **48.1x over**.
- The cause: the shared engine re-derives `nmag2`/`isqrt` per light, so K lights
  cost K roots for one magnitude. Invariant reuse and the 48x gap are one problem.
- 549 particle checks, 3,555 lighting checks, 41 group-seq checks, all green.
- Five blocks + GEOM.LIGHT + POST.COMPOSITE contribute **0 ALM** to any number.

### Duo quarter-plane geometry SETTLED, and the M10K ceiling ruled

**Owner ruling:** *"Using some more M10K is fine, we have enough, particularly if
it saves ALMs."* Recorded `reports/OWNER-RULING-M10K-CEILINGS-20260918.md`.
POST.COMPOSITE's <=8 M10K cap yields; 13 (19 double-banked) against 553 is 3.4%.
The ruling does NOT license spending M10K to remove DSPs, full-frame LUTs, or
treating logical bits as physical M10Ks — all three still stand.

**The Duo geometry, settled arithmetically** —
`reports/DUO-QUARTER-PLANE-GEOMETRY-20260918.md`:

| mode | displayed | rendered | quarter(displayed) | quarter(RENDERED) |
|---|---|---:|---:|---:|
| Z60 | 384x240 | 384x240 | 5,760 | 5,760 |
| Storm | 320x240 | 320x240 | 4,800 | 4,800 |
| Duo | 512x240 | 2x(256x192) | 7,680 | **6,144** |

**Z60 and Storm cannot distinguish the two readings** — displayed IS rendered
there. Duo is the only mode where they differ and the only one that is wrong,
which is exactly why it survived.

**The proof is inside the contract's own throughput table:** Duo main = 98,304 =
2x256x192 = RENDERED pixels; Duo glow = 15,360 = 2x7,680 = DISPLAYED cells. One
row, two surfaces. Corrected: plane **128x48 = 6,144** (two 64x48 views), glow
**12,288**, frame cost **110,592** not 113,664 — overstated 2.7%.

`design/blocks.yml` was already right: *"96x60 Z60 / 2x64x48 Duo"*. The ledger
beat both contracts. Correction banners added to `POST.GATHER.md` and
`POST.COMPOSITE.md`.

**The benefit is NOT the 1,536 cells** — memory is now explicitly cheap. It is
that per-view addressing makes "no bleed between Duo views" **structural**: with
two 64x48 planes there is no address that names the other view, so the clamp
stops being a comparator that has to be right. Bug class removed for certain;
ALM saving real but unmeasured.

Corrected one detail from the worker's report: those 48 rows are **black border**
(`zhao_pkg.sv`, views at y offset 24), not HUD scanlines. Conclusion unchanged —
border is black and HUD bypasses post — but `spec/video_rules.md` and the owner
plan §11.3 describe the same 48 rows differently and someone owns reconciling it.

POST.COMPOSITE worker resumed with both rulings.

### POST.COMPOSITE takes the table and the Duo fix — first real ALM number

Commit `15b3d8c3`. Independently re-verified here: 54/54 directed, 1/1 mutant,
`grade_product_vector` present in `reference/include/zref/zref_post.hpp`,
`duo_i`/`view_split_i` grep count **0** (deleted, not merely unused),
`refmodel_liveness` unchanged at 84 resolve / 11 phantom.

**The generator went into the ORACLE tree, not a test header.** A table generator
living only in a bench is a law with no owner — the shape that produced two
projector cores. Right call, unprompted.

**Equivalence is what §11.2 asked for and it is TOTAL, not sampled:**
32 x 64 x 32 x 3 channels x 4 matrices = **786,432 comparisons**, table against
the nine-multiplier path, with both saturation rails shown reached so it is not
passing because nothing interesting happened.

**THE FIRST CONCRETE ALM NUMBER OF THE SESSION, with its condition named:**

```
nine signed 16x8 multipliers IN LOGIC   ~1,200-1,300 ALM
the table                                  ~250 ALM
net                                       ~-1,000 ALM
```

**But only if those multipliers would have been in logic.** If Quartus would
instead put them in DSP blocks, the same change spends ~250 ALM to free ~5 DSPs
— a bad trade, and explicitly NOT the justification, per the M10K ruling's own
limit. **The first fit must say which happened.** Multiplier sites 21 -> 12.
At ~200 ALM/M10K, six blocks "buy" ~1,200 and this returns ~1,000: it ranks at
the boundary, and the heuristic ranks without closing.

**Duo no-bleed is now structural.** The block composites one view per pass, so
every x is view-local and the ordinary clamp to `[0, frame_w-1]` IS the per-view
clamp. The bench checks it as an **address-space census** — no cell index reaches
the other view's half or past the view's last quarter-row — not as a pixel value.

Two things the new work found, both worth more than the green:

- **A real defect, on the census's first run.** The front pointer walked one
  quarter-row past the last line during drain, presenting a cell outside the
  view. Harmless (the response is discarded) and still wrong, because it made the
  structural claim false. Fixed the DESIGN rather than weakening the assertion.
- **An overclaim in its own test, caught by fire-testing it.** A mutation
  swapping two arguments of `grade_channel_table` stayed GREEN — correctly, since
  the three lanes are summed and addition commutes, so the swap is not a fault.
  "It catches a transpose" was narrowed to "it catches a transpose IN THE
  GENERATOR". Only running the mutation showed the difference.

Still unverified and stated as such: synthesizability, every ALM/DSP/M10K figure
(shape arithmetic), Fmax, and whether the 72-bit memories infer as M10K at all.
Not written: random lane, formal lane, integration captures, and **fixgen does
not yet read `grade_product_vector`** — the generator is committed and named, but
no production asset path consumes it.
