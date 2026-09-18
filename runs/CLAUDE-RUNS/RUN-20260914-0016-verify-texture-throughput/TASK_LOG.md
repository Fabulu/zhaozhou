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
