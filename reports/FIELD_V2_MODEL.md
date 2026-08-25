# FIELD v2 model — Earth opcode histogram and SIMD/wavefront throughput

*Owner-authorised FIELD v2 redesign, Phase 1 + Phase 2 deliverable (2026-08-25).
This is a model and a report. No RTL was written or modified. FIELD.SEQ v1
remains frozen as the serial reference and differential oracle.*

Evidence discipline: every number below is labelled **measured**, **derived**,
or **assumed**. Analysis scripts (`histogram.mjs`, `model.mjs`, `uniform.mjs`)
are archived in `subagents/20260825-1808-field-v2-model/`.

---

## PHASE 1 — the Earth opcode histogram

### How it was obtained (provenance)

The three Earth exemplars are not hand-listed anywhere; they are **built by the
committed program sources** — the single sanctioned path
(builder → alloc → serialize):

- `compiler/src/field_ir/impact_wave.ts` (compiled: `compiler/dist/src/field_ir/impact_wave.js`)
- `compiler/src/field_ir/wave_pool.ts`
- `compiler/src/field_ir/crater_ring.ts`

I executed `buildImpactWave()`, `buildWavePool()`, `buildCraterRing()` under
Node 20.17.0 and counted opcodes in the **post-allocation physical code**
(`FieldProgram.code`), i.e. after §3.20 smoothstep macro expansion — the
instruction stream the hardware actually sees. Each builder self-validates its
serialized image (`decodeZprog`).

**Cross-check against committed goldens** (`compiler/tests/generated/*.hpp`):
all three program hashes match exactly, so the histogram describes the
committed programs, not a stale build:

| program | built hash | golden hash (`.hpp` header) | instr count |
| --- | --- | --- | ---: |
| impact_wave | 0x82f5f4e4 | 0x82f5f4e4 | 30 |
| wave_pool | 0x8bdceb63 | 0x8bdceb63 | 27 |
| crater_ring | 0x484add8d | 0x484add8d | 28 |

This histogram is therefore **measured** (counted from the committed programs,
hash-verified), not assumed.

### The histogram

| opcode | impact_wave | wave_pool | crater_ring | aggregate |
| --- | ---: | ---: | ---: | ---: |
| MUL | 10 | 11 | 8 | 29 |
| LDC | 7 | 6 | 8 | 21 |
| SUB | 4 | 4 | 3 | 11 |
| CURVE | 3 | — | 1 | 4 |
| DIST2 | 1 | 1 | 1 | 3 |
| RCP | 1 | 1 | 1 | 3 |
| CLAMP | 1 | 1 | 1 | 3 |
| DCURVE | 1 | — | 1 | 2 |
| ADD | 1 | — | — | 1 |
| SIN | — | 1 | — | 1 |
| COS | — | 1 | — | 1 |
| RING | — | — | 1 | 1 |
| CMP | — | — | 1 | 1 |
| SELECT | — | — | 1 | 1 |
| END | 1 | 1 | 1 | 3 |
| **total** | **30** | **27** | **28** | **85** |

What the histogram says, before any modelling:

1. **NORMALIZE count is zero.** In all three committed Earth programs. The
   owner's suspicion is confirmed: NORMALIZE work is **not justified for
   Earth** by any committed evidence. (`FIELD_RESOURCE_MODEL.md`'s
   NORMALIZE-heavy row was a parameter study, not Earth behaviour.)
2. Every program carries exactly **one DIST2** (the distance-to-centre that
   opens every radial terrain program) and exactly **one RCP** (the §3.20
   smoothstep macro's divide — `builder.ts` line 194).
3. Long-op density is 2–5 per program (DIST2 + RCP always; plus 4 table ops in
   impact_wave, RING + 2 table ops in crater_ring, SIN/COS in wave_pool) —
   far above the "one long op per eight instructions" that already broke the
   barrel-only model.
4. ~35–52% of instructions are cheap ALU/LDC; MUL is 27–41% of every program.

### The uniform/varying split (measured by exact dataflow analysis)

Only input registers `x`, `z` vary per vertex; `age`, `phase`, `p0..p7` are
per-association parameters. Forward taint over the physical code (with
register-kill on overwrite) splits each program into per-vertex ("varying")
and per-association ("uniform") work:

| program | varying | uniform | uniform long/table ops |
| --- | ---: | ---: | --- |
| impact_wave | 16 | 13 | 2 CURVE, 1 DCURVE, 1 RCP |
| wave_pool | 17 | 9 | 1 RCP |
| crater_ring | 13 | 14 | 1 CURVE, 1 DCURVE, 1 RCP |

Load-bearing facts from this split:

- **The RCP is uniform in all three programs** (its operand is `e1 − e0`, both
  smoothstep edge parameters). Executed once per association instead of 1,089
  times, RCP vanishes from the per-vertex loop.
- In impact_wave, **only one of the four table ops is per-vertex** (the wavelet
  CURVE over the moving `front` coordinate); ringdecay/bounce CURVEs and the
  DCURVE read only `phase`.
- crater_ring's CMP/SELECT material logic is entirely uniform.

This is a **measured property of the committed programs**; whether v2 exploits
it (a uniform/scalar issue path, GPU-style) is a design choice modelled as
Scenario C below.

---

## PHASE 2 — throughput model

### Inputs

**Measured** (read from `reports/synthesis/zhao_block_fit.json`, commit
`924a48ab`, `newestRunClean: true`, Quartus Prime Lite 17.0.2, 5CSEBA6U23I7):

- `zhao_field_seq` (v1, for comparison): 4,494 ALM, 3 DSP, 5 M10K, 58.99 MHz.
- `zhao_probe_banked_rf`: 375 ALM, 0 DSP, 12 M10K (98,304 bits), 96.54 MHz —
  geometry CONTEXTS=16 × REGS=64, 4 banks × 3 read copies (one M10K per
  replica), single write port.
- NORMALIZE3 II = 59 (measured at the unit boundary, steady across five
  accepts — `FIELD_RESOURCE_MODEL.md`).

**Derived** (from `reports/FIELD_RESOURCE_MODEL.md`; carry a ±2-cycle error
bar, as the NORMALIZE3 measurement demonstrated): per-unit II with
ready-only-when-idle units, II = latency: RING 48, LEN/DIST2 42, SPLINE 39,
NOISE2 23, CURVE 23, ROT3 21, ROT2 20, DCURVE 20, RIDGE 16, RCP 9.
`zhao_field_mul` and `zhao_field_sin` are pipelined at II = 1 (latency 2).
Multiplier lanes map at ~3 DSP per 33×33 lane.

**Unit sharing, read from RTL (not modified):**
`fpga/rtl/field/zhao_field_curve.sv` serves CURVE, DCURVE **and** SPLINE (one
unit); `fpga/rtl/field/zhao_field_sin.sv` serves SIN and COS (one pipelined
unit, `is_cos_i` selects the +quarter-turn phase).

**Frame arithmetic (given):** 128 associations × 1,089 vertices × ≤32 instr;
frame = 1,666,667 clocks at 100 MHz / 60 Hz; 20% reserve leaves **1,333,333**;
per-association budget = **10,416 cycles**.

### Model assumptions (all labelled **assumed**)

- **A1 — front-end:** one SIMD instruction (W lanes) issues per clock when a
  wavefront is ready. Front-end cycles per association
  `F = ceil(1089/W) × I` (W=2 → 545 groups; W=4 → 273 groups; 1,089 = 273×4
  exactly, so W=4 wastes nothing).
- **A2 — long units are shared and tagged:** each per-vertex long op is one
  scalar request; a non-pipelined unit serialises requests at its II. Unit
  demand per association = (varying op count) × 1,089 × II. Steady-state cycles
  per association `C = max(F_eff, max_unit demand)` — valid when the binding
  unit can be kept saturated, which holds here (each wavefront instruction
  contributes W queued lane-requests).
- **A3 — simple ALU ops are lane-private** (ADD/SUB/LDC/CLAMP/CMP/SELECT: W
  cheap ALM datapaths, no shared demand). This is the standard SIMD choice; a
  shared single ALU would instead add up to 14 × 1,089 = 15,246 cycles of
  demand per association and break every configuration — so A3 is
  load-bearing and cheap (these ops are add/compare/mux width-32 logic).
- **A4 — latency hiding:** a wavefront issues in order and stalls until
  operands are ready (producer latency = unit II figure above). Single-
  wavefront makespan `T_wf` computed by exact dependence simulation over the
  real code: **impact_wave 141, wave_pool 79, crater_ring 103 cycles**
  (derived). With N wavefronts, effective front-end cycles
  `F_eff = ceil(1089/W) × max(I, T_wf/N)`.
- **A5:** no extra transport/tag overhead cycles on the request/reply fabric
  (optimistic by a small constant; the tags themselves are wires, not cycles).

### Scenario A — v2 front-end, execution units AS THEY ARE

Per-association cycles are **identical for every (W, N, L)** because a
ready-only-when-idle unit binds everything:

| program | binding unit | unit demand/assoc | C/assoc | 128 assoc | % of 1,333,333 |
| --- | --- | ---: | ---: | ---: | ---: |
| impact_wave | CURVE unit (4 table ops) | 96,921 | 96,921 | 12,405,888 | **930%** |
| wave_pool | LEN/DIST2 | 45,738 | 45,738 | 5,854,464 | **439%** |
| crater_ring | RING | 52,272 | 52,272 | 6,690,816 | **502%** |

(Other demands per association, for scale: DIST2 45,738 in all three;
crater_ring CURVE-unit 46,827; RCP 9,801 everywhere; MUL at one lane
8,712–11,979; SIN 2,178.)

**Conclusion A: no SIMD width, wavefront count, or multiplier lane count
closes Earth60 while the long units accept only when idle.** The redesign's
value is unlocked by unit throughput, not by the front-end alone — the same
conclusion `FIELD_RESOURCE_MODEL.md` reached for the barrel, now confirmed
against the real programs, which are *worse* (2–5 long ops each) than the
"one in eight" parameter study.

### Scenario B — CURVE/DCURVE, RING, LEN/DIST2 pipelined to II=1; RCP kept at II=9

Full sweep (per-association cycles; binding resource in **bold**; % of the
reserved budget for 128 associations):

**impact_wave (I=30, T_wf=141):**

| W | N | L=1 | L=2 | L=3 | L=4 |
| ---: | ---: | --- | --- | --- | --- |
| 2 | 4 | **front-end** 19,212 · 184% | 184% | 184% | 184% |
| 2 | 8 | **front-end** 16,350 · 157% | 157% | 157% | 157% |
| 4 | 4 | **MUL** 10,890 · 104.5% | **RCP** 9,801 · 94.1% | 94.1% | 94.1% |
| 4 | 8 | **MUL** 10,890 · 104.5% | **RCP** 9,801 · 94.1% | 94.1% | 94.1% |

**wave_pool (I=27, T_wf=79):** W=2 → front-end 14,715 · 141% (all N, L).
W=4, L=1 → **MUL** 11,979 · 115%; W=4, L≥2 → **RCP** 9,801 · **94.1%**.

**crater_ring (I=28, T_wf=103):** W=2 → front-end 15,260 · 147%.
W=4, any L → **RCP** 9,801 · **94.1%** (MUL at L=1 is 8,712, under RCP).

Per-unit utilisation at (W=4, N=8, L=2), worst program impact_wave,
C = 9,801: RCP **100%** (binds), front-end 84%, MUL 56%, CURVE 44%, DIST2 11%.

Findings:

- **SIMD width 2 never closes.** Even with every long unit at II=1 the W=2
  front-end needs 545 × I ≥ 14,715 cycles/association (141–184% of budget).
  **Width 4 is required**, and 1,089 = 273 × 4 makes it exact.
- **One multiplier lane never closes** (wave_pool 115%, impact_wave 104.5%).
  **Two lanes (6 DSP) suffice**; lanes 3–4 buy nothing while RCP binds.
- **4 vs 8 wavefronts:** only visible where latency hiding is short —
  impact_wave at W=4 needs `N ≥ T_wf/I = 141/30 ≈ 4.7`; N=4 gives
  F_eff = 9,624 (92.4%, closes on a knife edge), N=8 restores the ideal
  front-end 8,190 (78.6%). **8 wavefronts recommended; 4 is the bare floor.**
- **Everything then converges on RCP at 94.1%** — a *derived* II carrying a
  ±2 error bar. If RCP's true II is 10, demand is 10,890/assoc = 104.5% and
  **Scenario B fails**. RCP's II must be measured at the unit boundary (as
  NORMALIZE3 was) before this closure is claimed — or RCP removed from the
  per-vertex loop entirely (Scenario C).

### Scenario C — Scenario B + uniform-op hoisting (execute the uniform prefix once per association)

Uniform work per association is 9–14 ops ≈ ≤ 70 cycles even at as-is IIs —
negligible. Per-vertex demands become (varying ops only):

| program | varying I | front-end W=4 | DIST2 | RING | CURVE | MUL (L=2) | SIN | C/assoc (binds) | 128 assoc % |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| impact_wave | 16 | 4,368 | 1,089 | — | 1,089 | 4,901 | — | **MUL** 4,901 | **47.1%** |
| wave_pool | 17 | 4,641 | 1,089 | — | — | 5,445 | 2,178 | **MUL** 5,445 | **52.3%** |
| crater_ring | 13 | 3,549 | 1,089 | 1,089 | — | 4,356 | — | **MUL** 4,356 | **41.8%** |

- RCP disappears from the per-vertex loop (uniform in all three programs);
  CURVE-unit per-vertex demand in impact_wave drops from 4 table ops to 1.
- The required unit IIs relax from "CURVE ≤ 2" to **"every long unit ≤ 9"**
  (single occurrence per program: 10,416 / 1,089 = 9.56) — a much cheaper
  pipelining target (e.g. a 5-stage overlap instead of full II=1).
- Margin roughly doubles: worst case 52.3% of the reserved budget, leaving
  headroom for the 32-instruction ceiling, mixed frames, and Fmax shortfall.

Hoisting is a front-end/loader scheduling feature (the taint analysis is ~30
lines and can run at program load); it changes **no** Field IR semantics, no
arithmetic law, and FIELD.SEQ v1 remains the oracle for the identical
program results.

### Acceptance-ceiling check (32 instructions)

At W=4 the front-end ideal is 273 × 32 = 8,736/assoc = **83.9%** of budget for
128 associations even if all 32 instructions are varying — the acceptance
target as stated closes on the front-end at width 4 (and cannot at width 2:
545 × 32 = 17,440 = 167%).

### Resource envelope at the recommended point (W=4, N=8, L=2)

- **DSP (derived):** 2 mul lanes ≈ 6 DSP (measured mapping ~3/lane). The
  pipelined SIN, CURVE-interpolation and isqrt datapaths each need multiplier
  slots; if they own DSPs (~3 each) the total lands 12–18 — inside the
  exploratory ceiling, at/over the 12 preferred. Letting the long units borrow
  tagged slots in the mul pool instead of owning DSPs is the lever to hold 12.
  v1's whole engine fit in 3 DSP by sharing one hub (measured), so sharing is
  proven practice here.
- **M10K (derived from the measured probe):** the probe measured one M10K per
  bank-copy replica (12 for 4 banks × 3 copies at 16 ctx × 64 regs). v2
  lane-private organisation: per SIMD lane a 3-copy (srcA/B/C) RF of
  8 wavefronts × 32 regs = 256 words — one M10K per copy → **4 lanes × 3 =
  12 M10K**, and per-lane write ports dissolve the sequential multi-lane
  write-back v1 had. (At 64 regs/context it doubles to 24; reg high-water of
  the committed programs is 17–18, so 32 suffices for the corpus.) Plus
  progcache/tables/ROMs (v1 carried 5 M10K total): ~17–29 M10K, inside the
  24–32 envelope.
- **ALM:** no measured figure exists for the v2 issue/scoreboard logic; the
  measured banked RF (375) replaces the operand walk and write-back walk of a
  4,494-ALM sequencer. I claim no total against the 8k–10k ceiling — that is
  a fit question.
- **Fmax:** the only measured v2-relevant figure is the banked RF standalone
  at **96.54 MHz — 3.5% below the 100 MHz composed target**. If the composed
  machine lands at 96.5 MHz the reserved budget shrinks to ~1,287,000 cycles:
  Scenario B's RCP bind rises to 97.5% (marginal), Scenario C's worst case to
  ~54% (fine). Another reason to prefer Scenario C.

---

## Answers

**Which units must be pipelined or replicated for Earth60 to close, in demand
order?** (per-frame demand at as-is II, even mix of the three exemplars;
labelled derived — it follows arithmetically from the measured histogram and
the derived IIs)

| rank | unit | frame demand (cycles) | × budget | required after fix |
| ---: | --- | ---: | ---: | --- |
| 1 | **CURVE/DCURVE** (one unit) | 6.13 M | 4.6× | II ≤ 2 without hoisting; II ≤ 9 with |
| 2 | **DIST2/isqrt (LEN)** — in *every* program | 5.85 M | 4.4× | II ≤ 9 |
| 3 | **RING** | 2.23 M | 1.7× | II ≤ 9 |
| 4 | **MUL** | 1.35 M at 1 lane | 1.0× | ≥ 2 lanes (6 DSP) |
| 5 | **RCP** | 1.25 M | 0.94× | measure its II ≥, or hoist (uniform in all three programs) |
| — | NORMALIZE | **0** | 0 | **no work justified for Earth** — confirmed |

The owner's expectation (DIST2/isqrt and CURVE/DCURVE first, RING next,
NORMALIZE not justified) is **confirmed by the histogram**, with two
refinements: the CURVE/DCURVE unit's raw cycle demand actually edges out
DIST2 (impact_wave carries four table ops), while DIST2 is the most universal
(one per program, every program); and RCP is a hidden fifth column — present
in every program via smoothstep, and the residual 94%-of-budget bind after
the big three are fixed, unless uniform hoisting removes it.

**Binding resource per configuration** — Scenario A: the slowest
ready-when-idle unit (CURVE / LEN / RING by program), at every (W, N, L).
Scenario B: W=2 → front-end; W=4, L=1 → MUL; W=4, L≥2 → RCP.
Scenario C: MUL at L=1, front-end at L≥2… MUL remains nominal binder at L=2
at ~50% utilisation.

**Recommended point: width 4, 8 resident wavefronts, 2 multiplier lanes
(6 DSP)** — with CURVE/DCURVE, DIST2/isqrt and RING pipelined, and uniform-op
hoisting in the front-end. Closes the acceptance target at ≤ 52% of the
reserved budget (worst exemplar), tolerates the ±2 II error bar, the
32-instruction ceiling, and a 96.5 MHz composed clock. Without hoisting the
same hardware point closes at 94.1% but stands on RCP's *derived* II=9 —
measure it before believing that margin.

## What would falsify this

1. **RCP's measured II ≠ 9.** At II ≥ 10, Scenario B does not close; only
   Scenario C (hoisting) survives. Highest-value single measurement.
2. **The derived IIs err beyond ±2** on CURVE/RING/DIST2 — changes Scenario A
   magnitudes, not its verdict; changes nothing in B/C where they are re-built.
3. **Earth programs beyond the three exemplars** with different long-op mixes
   (e.g. NOISE2, SPLINE, or a per-vertex NORMALIZE) — the histogram is the
   whole basis; a fourth committed program must be re-counted, and a varying
   NORMALIZE would resurrect the unit ranked "no work justified".
4. **A2's saturation idealisation:** if request/reply arbitration adds per-op
   overhead cycles, the 94.1% Scenario B point fails first; C has 2× slack.
5. **A3 (lane-private ALUs) proving unaffordable in ALMs** would add a shared-
   ALU demand of up to 15,246 cycles/assoc and invalidate every closure —
   no evidence points this way (these are 32-bit add/mux datapaths), but no
   fit measurement of a 4-lane ALU cluster exists yet.
6. **Composed Fmax well below 96.5 MHz** — budgets shrink proportionally;
   below ~89 MHz even Scenario C's impact_wave front-end (4,368 → at
   1,186,000 budget with all-varying 32-instr ceiling) starts eating reserve.

## Sanity checks against prior evidence

- v1 today computes one association of an 8-simple-instruction program in
  60,984 cycles (`FIELD_RESOURCE_MODEL.md`); this model's v2 recommended point
  computes a 30-instruction real program in 8,190–9,801 — a 6–7× per-cycle
  gain on 3.75× more instructions, consistent with that report's 7× barrel
  ceiling for simple programs plus the unit fixes it called for.
- The model reproduces that report's patch-fields/frame rows when fed its
  synthetic "7 simple + 1 long" programs (same arithmetic, same IIs).

---

## RCP's initiation interval is now MEASURED, and the 94.1% point holds

*Added 2026-08-25 after this report was written.*

This model's recommended configuration — width 4, 8 wavefronts, 2 multiplier
lanes — closes at **94.1%** of the reserved budget and **binds on RCP**. Its II
was listed as **9, derived** (a 16-clock total instruction latency minus a
7-clock front-end walk) with a stated ±2 error bar, and the report noted that
**at II=10 the configuration does not close**.

**Measured at the unit boundary: II = 9 clocks, steady across five accepts.**
(`tests/differential/field_rcp_directed.cpp`.) The derivation was exact.

So the 94.1% point stands on a measured binding constraint rather than a derived
one. Two things worth keeping from that:

* **The derivations are not uniformly reliable.** NORMALIZE3's was two clocks
  pessimistic (61 derived, 59 measured); RCP's is exact. The subtraction is a
  reasonable estimator and not a substitute for measurement, and which way it
  errs is not predictable from the outside.
* **This one mattered.** A ±2 error bar on a number that decides whether the
  recommended architecture closes is not a footnote, and it is the kind of thing
  that gets quoted as settled once it has been repeated twice.

The remaining derived IIs — RING 48, LEN/DIST2 42, SPLINE 39, NOISE2 23,
CURVE 23, ROT3 21, ROT2 20, DCURVE 20, RIDGE 16 — are still derived. CURVE and
DIST2 head the work order, so they are the next two that should be measured
rather than assumed.

---

## CURVE integration: attempted, reverted, and what was learned

*2026-08-26. The attempt is recorded because the next session should start from
the finding rather than rediscover it.*

The wiring was built and then **reverted**, so `OP_CURVE` is refused by v2 again.
It is not committed in a half-working state, deliberately: v2 had reached a
condition where it **accepted CURVE and returned zeros**, and an opcode that is
neither executed nor refused produces "a plausible field and a wrong world" —
the exact failure this engine's own law forbids. Accepted-but-broken is strictly
worse than unsupported.

### What was built and does work

* `zhao_field_v2_core` gained a long-op path: detect the curve family, hold the
  request until the serialiser accepts, stall issue while one is pending, keep
  the wavefront IN FLIGHT until the reply lands, and count the instruction as
  retired at REPLY rather than at dispatch.
* `zhao_field_v2_lanemux` + v1's unmodified `zhao_field_curve` + a
  `zhao_field_mul` lane, instantiated inside v2. Lint clean across all five
  modules.
* The **saturation ledger** was exposed as v2 outputs. The lint that flagged the
  dangling `sat_*` pins was right and worth obeying: saturation is part of the
  answer in this engine, and leaving those pins unconnected would have silently
  dropped half the semantics of every long op.
* The ALU path did **not** regress: 15 checks, still **0.99 instr/clock**.

### What does not work, stated exactly

Diagnostic from the run: **98 clocks, 2 instructions retired, status OK,
busy 0.** So the dispatch, the four-lane serialisation, the reply, the tag and
the retirement all function — 98 clocks is about `4 x II` plus overhead, which
matches the model. **Every lane's value comes back 0.**

Two hypotheses were tested and neither was the cause:

1. **Table timing.** `zhao::tick` evaluates three times and the unit's
   `tbl_idx_o` settles combinationally after each, so answering the table only
   before the tick feeds the posedge data addressed by a stale index. Re-driving
   the table after every eval — which is what v1's own curve bench does — did
   **not** fix it.
2. Operand capture (`lq_a <= rd_a` at `s1_valid`) was checked against the ALU
   path's own use of `rd_a` and appears correct.

**The cause is not yet known.** The next thing to try is a unit-level probe:
drive `zhao_field_curve` directly from the v2 testbench with the same table and
operands and confirm it answers there, which separates "the unit is not being
fed" from "the reply is not reaching the register file".

### The one thing that is certain

The failing differential was **not** weakened to pass. It compared against
`zfield::interpret` on a two-instruction program — the same oracle v1's curve
differential uses — and it correctly said no.
