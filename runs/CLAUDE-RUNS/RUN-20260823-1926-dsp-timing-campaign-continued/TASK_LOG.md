# Task Log: RUN-20260823-1926 - The DSP and timing campaign, continued

**Created:** 2026-08-23 19:26 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260823-1926-dsp-timing-campaign-continued/

---

## Objective

Census **160 -> 85-90**, and close timing on the blocks that miss it. Continues
RUN-20260823-0934, which is archived; its SPEC (census reconciliation and
GEOM.SKIN) was complete and the work had outgrown it.

---

## Progress Timeline

### 2026-08-23 19:26 UTC+02:00 - Run opened

Entry state:

| | |
| --- | ---: |
| census | **160** DSPs against 112 |
| ceiling | 85-90 |
| blocks with a real Fmax | **4 of 47** |
| `ctest -L fast` | 269/270, sole failure the V16 `FIELD.SEQ.CORE` gate |

Measured Fmax so far — **three of four are comfortably fast, only Field is
genuinely slow**:

| block | Fmax |
| --- | ---: |
| `zhao_texture_tmu` (pre-rearchitecture) | **199.72 MHz** |
| `zhao_geom_skin` | 89.65 MHz |
| `zhao_surface_stamp` | 87.54 MHz |
| `zhao_field_seq` | **33.86 MHz** |

In flight: `zhao_texture_tmu`, 28 -> 6-9 DSPs against a demand of 850,000
samples/frame derived from Sacrifice. Its `@pre-rearch` baseline is already
recorded, which is the SURFACE.STAMP lesson applied on the first move.

### 21:30 - TEXTURE.TMU complete: 28 -> 6 DSPs, census 134

Both fits under the corrected SDC, so like-for-like:

| | before | after |
| --- | ---: | ---: |
| DSPs | **28** | **6** (-79%) |
| ALMs | 1,844 | 1,921 (+4.2%) |
| Fmax | 36.92 MHz | 36.11 MHz |
| worst path | `q_fv_r[1]->smp_a_o[1]` 20.913 ns | `q_fmt_r[0]->smp_a_o[4]` 21.432 ns |

Frontier **28 / 12 / 6 / 3** at pre-rearch and `FILT_LANES` 4/2/1, with **Fmax
flat at 35.62-36.92 MHz** — none of these settings touches the limiting cone, so
**the DSP axis is nearly free in time on this block.** `zhao_texture_bilerp`
alone 7 -> 3.

Method: factor the contract's four-weight law —
`A=(t00<<8)+(t10-t00)*fu`, `B` likewise, `S=(A<<8)+(B-A)*fv`, one rescale. Same
integer, three products per channel instead of eight, two channels multiplexed.
**Not** the staged-rounding form `qformats §3` refuses; nothing intermediate is
rounded.

Sweep 30/30/30, caught 29. Run 1 (28/30) kept as the evidence for the harness
change: **M27 was a real hole** — the modelled cache ignored `cac_en_o` and was
strictly more generous than the real block. M18 is a true equivalent argued
against a named line. `ctest -L fast` **271/272**, the one failure the
pre-existing V16 baseline.

### Three findings from this block that outlive it

**1. THE SDC DEFECT HAD A SECOND HALF, and I published the bad number.**
The harness declared no `set_input_delay`/`set_output_delay`, and TimeQuest
**silently excludes every pin-to-register and register-to-pin path when none is
declared.** Same RTL, same tool, same device: **199.72 MHz clock-only vs 36.92
MHz with I/O** — a factor of **5.4**. This block's arithmetic runs pin-to-pin, so
199.72 was **the sample counter's speed** and the arithmetic had never been timed
at all. Harness fixed (`QUARTUS_GOTCHAS` §9). **Every Fmax measured before the
fix is suspect**; DSP counts are unaffected, since synthesis never reads the SDC.

**2. A post-hoc timing query on an unoptimised placement is not a critical
path.** I then published *37.004 ns, the address generator* as the honest worst
path. It was measured by applying I/O constraints to a database **placed with no
I/O objective at all**, so the fitter had never once optimised those paths. It is
an upper bound. Fitted properly the filter/output cone leads at 21.4 ns and the
address generator comes second. **Left uncorrected, the next implementer would
have pipelined the second-worst cone.**

**3. A GREEN HARNESS IS NOT A GREEN THEOREM.** The formal proof **does not close
on the factored form** — cover passes in ~1 s, bmc ran 3,300 s with no answer
against 741 s for the arithmetic it replaced. The opposite had been committed in
three places, all derived from "the harness needed no edit", which is true and is
a different claim. The run log had even noticed the proof was *slow* and read it
as more work rather than as the solver failing.

The cause is the same fact that makes the theorem worth having: the checker
states the law as `t00*w00 + ... + t11*w11`, and until now the DUT computed
**that same expression**, so the assertion was nearly a syntactic identity. It is
now a real distributive-law identity across three multiplies of three widths, one
feeding another.

**What replaced it is stronger and total, not sampled:** (i) no lane truncates —
every intermediate is monotone in each texel, so extremes over the byte domain
fall at texel **corners**, and 16 corners x 65,536 `(fu,fv)` bounds the whole
domain exactly, 0 violations; (ii) given no truncation the pre-rounding sum is
**exactly linear** in the four texels, so four basis vectors per `(fu,fv)`
determine the entire map, and the recovered coefficients equal `w00/w10/w01/w11`
exactly — settling it for **every integer texel quadruple**, not merely every
byte one. Script kept and runnable.

### Deliberately left open, named rather than closed quietly

1. the formal lane **left failing** so the regression stays visible — banked in
   `formal_runs.yml`; fixing the proof versus removing the lane is the owner's
   call;
2. the block **does not close 100 MHz** (36.11) — pre-existing, unrelated to the
   DSPs, fix specified;
3. it delivers **0.33x its derived demand** — the II=2 design is written into the
   contract, **not built**.

The agent also disclosed five process failures, including **three of five
predictions wrong** (Fmax direction, ALM direction, DSP packing) — all, by its
own diagnosis, from reasoning about what was *removed* rather than what
*replaced* it. And it re-ran its own tamper check after HEAD moved under it
twice, which is how it had earlier caught a silent revert of its own
rearchitecture (`git checkout <rev> -- <paths>` **stages**).

### 21:45 - RULING: the next run is a repo-wide audit, not another rescue

Docketed (`1390b7d`). Premise verified before acceptance:

* census covers **41 of 94** RTL files;
* **`zhao_geom_project` is not in the report at all**, despite duplicating
  `terrain_project`'s 33-DSP projection law;
* **`FORGE.CLIFF` confirmed** — three `assign x = mem_r[idx]` async reads over
  ~120 kbit written from an async-reset process, which is why its fit timed out.
  Diagnosable from source without spending another fit.

**And the fourth check refuted my own method:** counting nonconstant multiplies
in `geom_project` with grep gave 0, then gave line-counts. Both useless. That is
the argument for an **elaborated-AST scanner** rather than pattern matching, and
it is recorded as such rather than quietly dropped.

Honest total is **~180-185 DSP, not 134** — hidden projector plus pose
arithmetic. **Treat this as a 180-DSP design that must reach 85-90.** Worse than
it looked, and still credible: the two projectors alone are ~50 DSPs of pure
duplication.

`design/budgets/dsp.md` corrected too: "DSP blocks = the number of `*` operators,
**whatever the operand widths**" is contradicted by our own §5 evidence — the
same `zhao_geom_lod` source cost **28 DSPs at 72-bit operands and 18 at 64-bit**.
Operator count is a **lower bound**, exact only while operands stay inside one
block's native width.

### 22:26 - Budget audit wave 1 launched; the method changes here

With the machine idle and no implementation agent running, started the audit
rather than the next rescue. Its map-only pass is long unattended Quartus time,
which is exactly what should occupy an idle machine overnight.

**Scope is deliberately narrow: this run does NOT optimise anything.** The
deliverable is evidence and ranked work — an elaborated-AST scanner, Quartus
calibration microbenches, and a map-only pass to close the 41-of-94 gap.

**The acceptance test is falsifiable on purpose:** run the heatmap against
`zhao_field_seq` (zero M10Ks while spending 8,901 ALMs on a register file and
three ROMs built from logic) and `zhao_texture_tmu` (II=6 against a demand
needing II=1) and **confirm both light up red without anyone telling it the
answer.** If the tool cannot rediscover what we already know, it will not find
what we do not.

### 22:40 - STATUS written for the day  (`770618b`)

327 -> 134 measured; the honest ~180 stated rather than the flattering 134; the
seven recurring mistakes; and both corrections I owe — the 199.72 MHz figure
that was timing a counter, and the proof reported as passing that had not run.

### Closing position

| | |
| --- | ---: |
| census, measured | **134** DSPs / 112 |
| census, honest source-level estimate | **~180-185** |
| ceiling | 85-90 |
| blocks rebuilt today | 4 — Field 79->3, skin 72->9, stamp 28->0, TMU 28->6 |
| blocks meeting their frame budget | 1 — `zhao_geom_skin`, 124,514 vs 120,000 |
| modules with any fit | **41 of 94** |
| `ctest -L fast` | 271/272, sole failure the V16 baseline |

**Two blocks have specified, unbuilt fixes**: the Field engine must stop
building memories out of logic (6 M10Ks from 502 idle), and the TMU must accept
more than one request at a time (the cache's four lanes make one CLUT sample per
clock available, 1.67 M/frame against a 0.33x delivery today).

**The lesson the day kept teaching, now at twelve instances:** an artifact can be
real and still be an artifact of something other than what it was read as — an
SDC that constrained no clock, then one that excluded every I/O path; a sweep
that re-ran the previous binary; a preflight that scored an empty set; a proof
whose sources predated the edit; a stopped sweep still rewriting RTL; a contract
whose "met" meant cycles; a timing query on an unoptimised placement; and a
green harness read as a green theorem.

**A green result from a tool nobody has watched run is not evidence — and
neither is a number from a tool that was asked the wrong question.**

### 2026-08-24 00:30 - Audit wave 1: coverage closed, and the hidden 33 confirmed exactly

**89 of 91 modules mapped** against HEAD's exact RTL (`34be87f`), where the fit
report covered **41 of 94**. About four hours of unattended Quartus, against
roughly a day per block for the reactive method.

**`zhao_geom_project` = 33 DSPs — unit-identical to `zhao_terrain_project`'s
measured 33.** The prediction was 30-33 from reading the source; the answer is
exact. Its own header calls the duplication "a cost, not a feature", and that
cost is precisely 33 multipliers. **The two projectors together are more
multipliers than everything removed yesterday.**

Also newly visible: `zhao_geom_bin_pipe` **21**, `zhao_terrain_bake` **17**,
`zhao_geom_pose_decode` **18** (inside the 14-18 predicted from source).

**A number I deliberately did not publish as a headline:** the 89 mapped modules
sum to 262, and that figure is meaningless — mapping each module standalone
double-counts hierarchy (`geom_bin_pipe` contains the binner; `field_mul` and
`field_exec_shared` sit inside `field_seq`). **Containment must be resolved
before any total means anything.** Said so in `STATUS.md` rather than handing
over a big number that looks like bad news and is arithmetic done wrong.

### 00:45 - THE RAM INFERENCE LAW, measured rather than inferred

The calibration microbenches produced the strongest artefact of the campaign,
because it is a **controlled experiment** rather than a deduction from a block:

| shape | bits | sync + norst | sync + RST | async + norst | penalty |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64x32 | 2,048 | **40 ALM** *(infers)* | 1,411 | 1,427 | **35x** |
| 256x16 | 4,096 | **26 ALM** *(infers)* | 2,801 | 2,818 | **108x** |
| 1024x32 | 32,768 | **44 ALM** *(infers)* | 22,071 | 22,071 | **502x** |

> **Synchronous read AND no reset touching the array -> infers, tens of ALMs.
> EITHER an asynchronous read OR a reset on the array -> ZERO memory bits.**
> The two conditions kill inference **independently**; either alone suffices.

**The penalty is superlinear** — 35x at 2 kbit, 502x at 32 kbit — because the
memory is not merely dearer, it **is not there**, and every bit becomes a flop
behind a mux tree. So the rule bites hardest exactly where the array is largest
and the temptation to "just use registers" is strongest.

It retro-explains two findings we paid to learn:

* `zhao_field_seq` — **0 M10Ks while spending 8,901 ALMs**, a 64x32 register
  file plus three ROMs built in logic;
* `zhao_forge_cliff` — three `assign x = mem_r[idx]` **async** reads over
  ~120 kbit, fit **timed out at 5,000+ s**. On this curve that is expected, and
  it means **nobody ever needed to fit it**: the source shape is the diagnosis.

**`memBits = 0` is now a detection signal, not a policy.** A block that plainly
contains storage and reports zero memory bits is a failed implementation
whatever its tests say — which was already the acceptance rule in the Field
ruling, and is now a measurement.

Handed the analysis to the audit agent to write as `QUARTUS_GOTCHAS` §10 rather
than writing it myself: it owns that file and the full 102-bench dataset
including variants I cannot see, and **editing a file under a running agent is
the collision that cost me earlier today.**

### 00:50 - Field core count: answered as far as evidence allows

Costed both remaining profiles against repo figures rather than waiting for a
ruling:

* **EARTH** — `terrain_rules.md` states the worst legal patch at 557,568 field
  instructions, "~33% of a frame", **at 1 instr/cycle, which it honestly labels a
  placeholder.** Measured today: **six clocks**. So the true figure is **201% of
  a frame for ONE patch**. Even a modest case — 1 program, 8 instructions,
  across the 270 visible patches — is **8.5x a frame.** No core count fixes
  that; three do not and thirty do not.
* **FLOW** — and here **I corrected my own flag.** I had called it the profile
  most likely to need its own core. It is genuinely per-frame, and it is
  **cheap**: at Sacrifice's measured scale (largest burst 1,375 particles,
  standing budget 2,000-3,000) it is **4.0-8.6% of a frame**, and double the
  peak with double-length programs is 40%.

**About 98x apart, and both are "per-frame profiles".** The difference is not the
engine: **a particle is an ENTITY (thousands); a patch vertex is a SAMPLE OF A
FUNCTION (hundreds of thousands).** Entity-scaled work fits. Sample-scaled work
must be cached.

**So: one core, provided EARTH becomes event-driven** — which Sacrifice's own
engine supports, since its displacement is permanent, its deformers touch 9-25
cells, and its backend only re-runs displacement when the hash changes. *Free
until something actually digs.* Same shape as `SURFACE.STAMP`'s 36,864 texels
**once per impact**: a per-frame cost model applied to an event-driven quantity.

**WARP is the remaining risk** — vertex deformation is sample-scaled, and
120,000 vertices x 8 instrs x 6 clocks is **3.5x a frame**. If it runs on whole
meshes every frame it has EARTH's problem and needs EARTH's answer.

**The question is no longer "how many cores" but "which profiles are
sample-scaled".** Three of five are answered.

### 2026-08-24 03:00 - Audit closed. The waves it ranked are now the work.

The audit's deliverables are landed (`BUDGET_HEATMAP.md`, `budget_manifest.json`,
`rtl_inventory.json`, `calibration.json` 102/102, `QUARTUS_GOTCHAS` §10) and its
run is archived. **It built the map. None of the territory is walked yet**, and
saying otherwise would be the same category error as reading a green harness as
a green theorem.

**Started wave 1 of implementation: `zhao_surface_sheet` storage inference.**

Chosen over the larger DSP prizes deliberately. It is **229% of the device on its
own** — 131,072 declared bits inferring as **zero**, 131,258 registers, 95,947
ALMs — so it is not expensive, it is **unplaceable**. A block that cannot be
placed outranks a block that is merely wasteful, and the fix is now a **measured
law** rather than a hypothesis (three independent killers, penalty superlinear to
808x).

It is also the cheapest possible proof that the audit's central claim is
actionable: the diagnosis came from a map, not a fit, and the fix is verified by
`blockMemoryBits > 0` rather than by a test suite that would pass either way.

### The queue, explicit rather than implied

| # | work | return | state |
| --- | --- | ---: | --- |
| 1 | `zhao_surface_sheet` storage | **229% -> fits** | **running** |
| 2 | `zhao_forge_cliff` `edge_mem_r` | 79% -> fits | stretch of wave 1; two of its three tables already infer |
| 3 | projector merge — `GEOM.PROJECT` + `TERRAIN.PROJECT` | **~50 DSPs** | docketed; byte-identical arithmetic confirmed, 33 each |
| 4 | Field memories rebuild, waves 0-5 | 8,901 -> ~3,500-5,000 ALMs, 100-120 MHz | docketed with acceptance criteria |
| 5 | TMU pipeline, waves 0-5 | II 6 -> 1, 36 -> >=100 MHz | docketed; arithmetic already correct and pinned |
| 6 | `TERRAIN.NORMALS` width narrowing + sequencing | 18 -> 6 -> ~3 | the 27-bit band makes the first 3x free |
| 7 | demand figures for the remaining 84 blocks | separates "expensive" from "wrong" | the audit's own "next cheap win" |

**Three limits carried forward, stated by the audit rather than discovered
later:**

1. **none of the 41 stored fits describes HEAD** — every trustworthy resource
   number in the heatmap came from the map lane, and the heatmap says so per row;
2. **WNS/TNS/hold extraction is fixed but UNPROVEN** — no fit has run since the
   rewrite. **Do not quote a slack number until a real `.sta.rpt` has been read;**
3. 84 blocks still have no demand figure.

**The 27-bit band is the cheapest lever on the board and applies to several of
these**: 8-27 bits is one DSP per product, 28-33 is three. Narrowing a 32-bit
operand to 27 cuts a product threefold with **no other change** — no sequencing,
no extra clocks, no interface change. It needs only a proof of width, which is
exactly what `QUARTUS_GOTCHAS` §5 has demanded since it was written.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 17:36 | `acc49f0` | TEXTURE.TMU 28 -> 6-9 DSPs | **COMPLETE** — 6 DSPs, census 134, archived | `runs/CLAUDE-RUNS/RUN-20260823-1736-texture-tmu-dsp-rearchitecture/` |
| 22:26 | `af363d9` | Budget audit wave 1 — scanner, calibration, map-only pass | **COMPLETE**, archived | `runs/CLAUDE-RUNS/RUN-20260823-2226-budget-audit-wave1/` |
| 03:00 | `a54e57b` | SURFACE.SHEET storage inference, 229% -> fits | Running | own run dir |

---

## Files Created

- this run directory

---

## Decisions Made

**Closed the previous run rather than letting it accrete a whole day.** Its
SPEC was census reconciliation and GEOM.SKIN; both were complete while the work
had moved on to three other blocks and an architectural ruling. A run whose log
no longer matches its SPEC stops being a record and becomes a diary.

---

## Next Steps

- [ ] `zhao_texture_tmu` result
- [ ] `zhao_terrain_normals` 18 -> 1-2 (derived demand 2,000 normals/frame)
- [ ] `zhao_terrain_project` 33, cache-then-sequence
- [ ] the Field rearchitecture, wave 0 first: preserve the 33.86 MHz netlist and
      group the top 200 setup paths by family
- [ ] derive per-frame demand for the five Field profiles
- [ ] re-measure blocks whose rows predate the SDC fix
- [ ] archive this run when the campaign closes
