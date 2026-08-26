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

## CURVE integration, attempt 1: attempted, reverted, and what was learned

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

---

## CURVE integration, attempt 2: it runs, and the sweep found a hang under it

*2026-08-26, later. Attempt 1's section is kept above because the cause it did
not find is worth reading beside the cause that was.*

### The cause attempt 1 could not find

The curve table is a **REGISTERED read**: the index presented this cycle is
answered on the NEXT one, which is what an M10K does. Attempt 1 drove it
combinationally — answering the current index in the same evaluation — so the
unit was fed data one cycle early on every lookup and every lane came back zero.

v1's own curve bench states the protocol in a comment. Attempt 1's hypothesis 1
was *close* to it — it re-drove the table after every `eval` — but that is a
different mistake from answering one cycle late, and re-driving more often does
not make an early answer late.

The corrected `tick_tbl` captures `tbl_idx_o` **before** the tick and presents
the answer **after** it. Out-of-range indices are answered with hostile values
(`INT32_MIN`, `0x5A5A5A5A`) so a missing bound check walks off the end loudly
rather than quietly agreeing with the oracle.

Result: CURVE agrees with `zfield::interpret` per lane. DCURVE and SPLINE
followed as evidence rather than RTL — one unit, three modes — but they are
tested separately because the mode travels through the serialiser as part of the
request, and a CURVE-only test passes with the mode hard-wired to zero.

### THE HANG, which no value check could have found

The mutation sweep scored the new seam and **three mutants survived**: a long op
counted at both dispatch and reply (M93), the unit given the LIVE mode rather
than the captured one (M94), and a request retired without the serialiser having
accepted it (M95).

All three share one cause of invisibility: **every section started ONE
wavefront**, so at most one long operation was ever in the machine. That is not
the workload. Eight wavefronts run the same Earth program, drift apart in pc, and
several want the unit in the same window.

Section 8 was written to reach them. It **hung** — 11 of 24 instructions retired
against the 20,000-clock guard.

    dispatch slot filled at STAGE 2      = two cycles after issue
    issue guard read `lq_valid`          = two cycles too early
    => a second long op reaches stage 2 while the first request is still
       pending, and the dispatch overwrites it. The first wavefront waits
       forever for a reply to a request that no longer exists.

This is the failure mode the model did not predict, and it is worth naming
precisely: **the throughput model reasons about initiation intervals, and an
initiation interval says nothing about a request that is destroyed before it is
served.** The model's arithmetic was right; its silence was the problem.

### The interlock

```systemverilog
wire ins_is_long    = (ins_op_i == OP_CURVE) || (ins_op_i == OP_DCURVE) ||
                      (ins_op_i == OP_SPLINE);
wire long_slot_busy = lq_valid || (s1_valid && s1_is_long);
assign issue_fire   = sel_valid && !pc_overrun && !(ins_is_long && long_slot_busy);
```

Only LONG ops are held. Short ops keep issuing past a pending request: they touch
neither the slot nor the serialiser, and stalling the whole machine behind one
curve lookup would give back exactly the throughput v2 exists for. Wavefronts
write disjoint register regions, so a short op retiring beside a long reply
cannot collide with it.

**M96 mutates the interlock back to its two-cycle-late form**, so the defect
cannot return unnoticed.

### Measured

| | |
|---|---|
| ALU-only program, 8 wavefronts | 0.99 instr/clock, **3.97 vertex-instr/clock** (unchanged) |
| CURVE + SPLINE, 8 wavefronts | 24 instructions in 2,020 clocks |
| checks | 23, all green |
| sweep, v2 core + lanemux in full | 17 mutants, all caught |

The long-op figure is a serialiser measurement, not a regression: four lanes go
through one unit one at a time, sixteen long operations do that sixteen times,
and the ALU path's throughput is untouched. It is also the argument for the next
piece of work — **a second lane** — stated as a number rather than a preference.


---

## Next long operation: DIST2/LEN2/LEN3, and the operand problem

*2026-08-26. Written before the RTL, so the design decision is on the record
rather than reconstructed from the diff.*

### The oracle resolves, and the unit already exists

`reference/include/zfield/zfield.hpp` gives `OP_LEN2 = 0x12`, `OP_LEN3 = 0x13`,
`OP_DIST2 = 0x14`, and `zfield_interpret.cpp` implements DIST2 as `len_of` over
the componentwise difference — the same `len_of` LEN2 and LEN3 use.

v1's `zhao_field_len` already takes `mode_i` 0/1/2 for exactly those three, plus
`a0/a1/a2/b0/b1`. So this is the CURVE situation again: **one unit, three modes,
wiring rather than arithmetic.**

### What makes it harder than CURVE

CURVE consumes ONE operand. These consume up to FOUR:

| op | registers read |
|---|---|
| LEN2 | `a`, `a+1` |
| LEN3 | `a`, `a+1`, `a+2` |
| DIST2 | `a`, `a+1`, `b`, `b+1` |

v2's register read walk supplies three values per lane — `rd_a`, `rd_b`, `rd_c`
— addressed by the instruction's `a`/`b`/`c` fields. These ops do not address
their operands that way: they take CONSECUTIVE registers from a base. `a+1` is
not reachable from any current port.

### Two ways out, and why the second one wins

**More read ports.** Three per lane becomes five. The banked register file
measured **12 M10K** at three ports (`zhao_probe_banked_rf`), and port count is
what M10K replication scales with, so this is roughly a 4 M10K increase for an
operation that is a minority of the histogram. It also raises the port count for
every wavefront whether or not it ever executes a length.

**A second read pass.** Spend ONE extra clock: with the long op held in stage 1,
drive the read addresses from `{s1_wf, s1_a + 1}`, `{s1_wf, s1_a + 2}` and
`{s1_wf, s1_b + 1}` on the following cycle, and capture those into the request
alongside the first three. Zero extra M10K.

The second pass wins on the numbers already measured. A long operation costs
about `4 x II` in the serialiser — the CURVE+SPLINE mix measured 2,020 clocks
for 24 instructions — so **one clock is under 1% of the operation it belongs
to**, against a permanent ~4 M10K on a device with 553.

### What it requires, stated so it is not discovered late

* `s1_a` and `s1_b` must be captured at issue. Stage 1 does not keep them today,
  because no short op needs the operand INDEX after the read has happened.
* **Issue must stall completely for the steal cycle** — not just for long ops.
  The existing interlock holds long ops only, deliberately, so short ops keep
  flowing past a pending request. A short op issuing into the steal cycle would
  have its own reads replaced by the length's second pass, and would then compute
  on another instruction's operands. That is a wrong ANSWER rather than a hang,
  which makes it the more dangerous of the two.
* The mutant for it therefore writes itself: **remove the steal-cycle stall and
  the sweep must fail.** If it does not, the test does not run a short op behind
  a length, and that is the gap to close first.

---

## After the length family: RING, and the multiplier stops being a mux

*2026-08-26, written before the RTL.*

### The oracle resolves

`zfield_interpret.cpp` implements `OP_RING` as two `smoothstep`s about the
midpoint of `[r0, r1]` and a product of the first with the complement of the
second, over `reg[a]`, `reg[b]`, `reg[c]`. `OP_RIDGE` is a `noise2_hash` fold.
v1's `zhao_field_ring` covers RING in one unit.

### RING is CHEAPER to reach than the length family

It takes three operands, and it takes them from `a`, `b` and `c` — the natural
ports. **No steal cycle**: it dispatches straight from stage 1 like a curve,
with `a0/a1/a2` of the existing bundle carrying `d/r0/r1`. The bundle built for
DIST2 pays for itself here.

### What it does cost: the multiplier stops being a two-way mux

`zhao_field_ring` drives BOTH the shared multiplier and the shared reciprocal,
and `zhao_field_rcp` drives the multiplier too. So while one RING executes,
**two consumers want the lane** — and the "only one long op is in flight, so
only one unit is active" argument that justified v2's mux no longer covers it.

v1 solved this with a fixed PRIORITY rather than a wider mux:

```systemverilog
if (rcp_mul_issue)  mul <= rcp;      // the reciprocal outranks
else                mul <= unit;     // the executing unit's own request
```

v2 will mirror it exactly, for the reason it should: the same priority, feeding
the same units, checked against the same oracle. Inventing a different
arbitration here would mean re-proving something v1's differential already
covers.

The routing comment in `zhao_field_v2_core` currently says the mux becomes wrong
if two long ops ever run concurrently. RING is the first case where two
CONSUMERS run inside one operation, which is a different thing and does not
break that argument — but it is why the mul selection becomes a priority chain
rather than a widened `to_len ? : ` ternary.

### RIDGE and NOISE2 need something v2 does not have yet

Both read `ins.imm`. v2's instruction interface is `{op, dst, a, b, c}` — there
is **no immediate port at all**. That is an interface change to the core, its
bench and every caller, so it is its own increment and not a rider on RING.
Recorded here so it is not discovered halfway through wiring NOISE2.

### Order, by measured demand

RING, then the immediate port, then NOISE2/RIDGE. NORMALIZE2/3 stay last
regardless of their position in the opcode list: no committed Earth program
calls them, verified against the three shipped program hashes.

---

## THE REST OF THE OPCODE SET IS ONE SEAM CHANGE, NOT FIVE UNIT WIRINGS

*2026-08-26, from reading the oracle before writing anything. This changes the
order of the remaining work, so it is worth stating on its own.*

I had the remaining Field opcodes queued as five separate increments — NOISE2,
RIDGE, ROT2, ROT3, NORMALIZE2/3 — each "wire v1's unit to the seam", the way
CURVE and the length family went. Reading `zfield_interpret.cpp` for all of them
first says that is wrong, and in a way that would have been discovered halfway
through the first one.

### Every remaining opcode writes MORE THAN ONE REGISTER

| op | reads | writes | immediate |
| --- | --- | --- | --- |
| NOISE2 | `a`, `a+1` | `dst`, `dst+1` | seed |
| RIDGE | `a`, `b` | `dst` | seed |
| ROT2 | `a`, `a+1`, `b` | `dst`, `dst+1` | — |
| ROT3 | `a`, `a+1`, `a+2`, `b` | `dst`, `dst+1`, `dst+2` | axis |
| NORMALIZE2 | `a`, `a+1` | `dst`, `dst+1` | — |
| NORMALIZE3 | `a`, `a+1`, `a+2` | `dst` … `dst+2` | — |

**v2's reply path returns ONE value per lane and writes ONE register.** Every
long op built so far — CURVE, DCURVE, SPLINE, LEN2, LEN3, DIST2, and RING next —
is single-result, so the seam has never been asked for more. Five of the six
remaining opcodes ask for more.

### The v1 units are already wide

This is not a units problem. `zhao_field_rot` has `o0_o`, `o1_o`, `o2_o`
(`o2_o` zero for ROT2 by its law 5) and `zhao_field_noise` has `o0_o`, `o1_o`
with `seed_i` for the immediate. The arithmetic exists and is proven. What does
not exist is a way to carry three answers back through
`zhao_field_v2_lanemux` and land them in three consecutive registers.

### So the work is TWO interface changes, then five cheap wirings

1. **An immediate port.** v2's instruction interface is `{op, dst, a, b, c}`.
   RIDGE, NOISE2 and ROT3 all need `imm` — as a hash seed for the first two and
   as an AXIS SELECT for ROT3, where getting it wrong rotates about the wrong
   axis and produces terrain that looks plausible and is wrong.
2. **A multi-result reply.** The serialiser's unit port and vector reply widen
   to three values, and the core's write-back writes `dst`, `dst+1`, `dst+2`
   under a per-op count. The reads are already covered: the operand bundle built
   for DIST2 carries `a0/a1/a2/b0/b1`, and `a+1`/`a+2` arrive on the steal cycle.

After those, NOISE2/RIDGE/ROT2/ROT3/NORMALIZE2/3 are the same "one unit, N
modes" wiring the curve and length families already were.

### What this does NOT change

NORMALIZE stays last. No committed Earth program calls it, verified against the
three shipped program hashes, and being cheap to add once the seam is wide is
not a reason to add it before something that is actually used.

### The hazard the multi-result write-back introduces, named now

Writing `dst`, `dst+1`, `dst+2` means a long op can write registers a LATER
instruction has already read — v2's whole no-forwarding argument rests on "the
previous instruction has written back before the next is fetched", and that
argument was made about ONE register. A three-register write from a long op that
retires late is the first thing that could break it. That is a scoreboard
question, and it is the reason this is its own increment with its own sweep
rather than a rider on an opcode.

---

## The length family's sweep: 31 of 32, and what the one survivor taught

*2026-08-26.*

    attempted=32 expected=32 accounted=32 caught=31
    SURVIVOR: M106 the length's saturation never reaches the ledger

### The survivor is a category, not a mutant

M106 makes a length's saturation register only when a curve also saturated. It
survived all 38 checks, and the reason generalises past this one op:

> **Every section checked VALUES, and saturation is not a value.**

The numbers stay right while the engine's account of what it had to clamp is
silently dropped. That account is half of what this engine returns -- it is the
same thing the dangling `sat_*` pins would have lost when CURVE landed, caught
then by a lint and this time by nothing.

**Any future unit wired to the seam needs a saturation case, not just a value
case.** RING has five ledger lanes (`add`, `mul`, `rescale`, `rcp`, and `rcp0`,
which is a reciprocal of zero rather than a saturation) and every one of them is
a place this can happen again.

### The fix, and why it is two cases

Section 12 builds its expectation from `zref::SatLedger` and `zref::fx_sub` --
the construction `field_len_directed.cpp` already uses for this question, rather
than my own reasoning about when a subtraction overflows.

* a DIST2 whose difference cannot fit, asserting the lane fires;
* a quiet 3-4-5 length asserting it stays clear. Without it, a ledger wired
  stuck-at-one passes the first case.

Both first assert that the ORACLE agrees the operands do, and do not, saturate.
A saturation test whose operands quietly stopped overflowing would otherwise go
vacuous and still report green.

### Two tooling defects the run exposed, both now fixed

* **The preflight had no v2 lint cone.** `zhao_field_v2_core.sv` and
  `zhao_field_v2_lanemux.sv` were in the mutant list and in no cone, so every v2
  mutant was linted against a composition that does not contain it -- it passed
  by not looking. Closing that exposed **twelve malformed mutants**, five of
  which had already been scored CAUGHT. All twelve rewritten to keep the defect
  without orphaning a signal.
* **Guard 8: one sweep at a time.** A second sweep started beside a live one
  corrupts both trees, and the symptom is not an obvious clash: it is anchors
  that are present reporting NOT UNIQUE, a different mutant failing every run,
  and a preflight capturing a MUTATED baseline as gold. The sweep now takes a
  lock recording pid, start time and subset.

---

## RING landed, and the multiplier's rule was wrong in a way worth stating

*2026-08-26.*

RING executes: 52 checks, 16 instructions in 1,556 clocks, matched per lane
against `zfield::interpret` inside the band, at both edges, and outside them.

### The prediction held

The note above said RING would be CHEAPER to reach than the length family
because it takes three operands from the natural a/b/c ports. It was. No steal
cycle, no new read path, and the operand bundle built for DIST2 carried
`d/r0/r1` with no change at all. The expensive increment paid for the cheap one.

### The rule that was wrong

v2's multiplier mux rested on this, written in the core's own comment:

> the interlock guarantees at most ONE long operation is in the machine at a
> time, so at most one unit is ever active

That is true, and it is not sufficient. **One operation can contain two
consumers.** `zhao_field_ring` drives the lane, and the `zhao_field_rcp` it calls
twice drives it too. Nothing about the interlock prevents that, because the
interlock is about operations and this is about users.

The correction is v1's, verbatim in structure: a priority chain with the
reciprocal on top.

```systemverilog
if (rc_mul_issue)       mul <= rcp;     // the reciprocal outranks
else if (to_ring)       mul <= ring;
else if (to_len)        mul <= len;
else                    mul <= curve;
```

Mirroring rather than inventing is the point. v1's differential already proves
this arrangement against this oracle for these units; a different scheme would
be a new thing to prove, for no gain.

**The generalisation for whoever adds the next unit:** ask not "can two
operations overlap" but "does this operation call anything that also needs the
shared lane". NORMALIZE calls the reciprocal too.

### Section 13, and why rcp0 is the interesting lane

Applying M106's rule -- every unit on this seam needs a saturation case -- RING's
five lanes include one that is not a saturation: `rcp0`, a reciprocal of zero.

RING reaches it from an input any caller can supply. `r0 == r1` is a band of zero
width; the midpoint collapses onto both edges; both smoothsteps get
`e1 - e0 == 0` and hit the pinned `field_rcp` zero rule that `zref_trig.hpp`
SS7.3 documents in its own comment. This is not an exotic overflow -- it is a
degenerate band, and a caller computing radii from game state will produce one.

Tested in both directions, because a lane wired stuck-at-one passes the
degenerate case unaided.

---

## Correction to the work order: RIDGE is cheap, and it comes before NOISE2

*2026-08-26. The section above bundled RIDGE with NOISE2 behind two interface
changes. Reading `zhao_field_noise`'s ports says that is wrong and costs an
increment.*

`zhao_field_noise` serves both ops on an `is_ridge_i` select, and RIDGE is the
easy half:

| | reads | writes | needs |
| --- | --- | --- | --- |
| RIDGE | `reg[a]`, `reg[b]` -- **both natural ports** | `o0_o` only | the immediate |
| NOISE2 | `reg[a]`, `reg[a+1]` | `o0_o`, `o1_o` | the immediate, a steal cycle, TWO results |

So **RIDGE needs only the immediate port.** No steal cycle, no multi-result
reply, no priority-chain change (it drives the multiplier but calls no
reciprocal). That makes it the same shape as RING: one unit, a mode select,
operands already in hand.

### Revised order

1. **The immediate port, and RIDGE with it.** One interface change, one opcode,
   one sweep. `ins_imm_i` on the core, captured at issue, carried through the
   request like the mode and the unit selector.
2. **The multi-result write-back**, which is the only thing then standing between
   v2 and NOISE2, ROT2, ROT3, NORMALIZE2/3.

### The steal cycle generalises for free

It is triggered today by `s1_is_len`. NOISE2, ROT2, ROT3 and NORMALIZE2/3 all
read `a+1` (and ROT3/NORMALIZE3 `a+2`), which is exactly what the steal already
fetches. That predicate becomes "needs a second pass" rather than "is a length",
and nothing else about it changes -- including the stall, which is the part that
matters and is already swept by M97.

### So the remaining Field work is smaller than the opcode list suggests

Six opcodes, but only ONE unbuilt mechanism between here and all of them: a reply
that carries more than one value. Everything else is either built (the bundle,
the steal, the interlock, the priority chain) or a mode select on a unit v1
already proved.

---

## RING's sweep, and the sweep learns what "equivalent" means

*2026-08-26.* 39 mutants, **36 caught, 2 proven equivalent, 1 real gap.**

### The gap: M117

RING's multiply saturation reached the ledger only beside a curve's, and nothing
noticed. Sections 11 and 13 do not make a ring saturate a product -- 13 exercises
`rcp0`, which is a different lane.

It is reachable from an input a caller produces: RING divides by the band's
half-span, so a **narrow band** gives a large reciprocal and `(x - e0) * r`
overflows before smoothstep clamps `t`. A band 1/128 wide with the point 1,000
units away does it. That is a thin ring seen from far off, not a contrived
number. Section 14, both directions.

This is the third time the ledger has been the hole and the values have been
fine. The pattern is now explicit enough to state as a rule: **when a unit is
wired to the seam, every ledger lane it can raise needs a case.** RING raises
four; two of them (`rcp0`, `mul`) needed a test that did not exist.

### The two equivalents, and why they are not the same kind

**M114 -- the reciprocal loses its precedence on the multiplier.** Equivalent
UNCONDITIONALLY. `zhao_field_ring` asserts `rcp_valid_o` only in `G_SPAN` and
`mul_issue_o` only in `G_T/G_T2/G_2T/G_CUBE/G_FIN`, and waits in `G_SPANW` with
`mul_issue_o` low while the reciprocal computes. The two cannot drive the lane in
the same cycle. I had predicted this mutant would survive and predicted the wrong
reason: I expected a test gap I could close by forcing contention. **No test can
force it** -- the unit serialises its own demands. The chain stays because it is
v1's and because a future unit may contend.

**M116 -- rcp0 reaches the ledger only if both report it.** Equivalent TODAY.
The ring latches `rcp0_o <= rcp0_o || rcp_zero_i` and is the reciprocal's only
consumer, so the lanes always overlap. The `||` is defensive for a SECOND
consumer, and NORMALIZE will be one. **Re-score M116 the moment a second consumer
is wired**; it should then be caught, and if it is not, the OR is untested.

### The mechanism, because the law was stated and enforced nowhere

The sweep's header has always said survivors carry a proof of equivalence or they
are holes. It printed both identically and, worse, **exited 0 either way** -- a
hole could pass a gate that checked only the exit code.

Now: `EQUIVALENT` in the mutants file maps a mutant's id token to its proof, and

* declared + survived -> reported as equivalent, proof printed, run passes;
* declared + **caught** -> **ABORT**. The proof is false, and a false proof is
  worse than an unproven survivor because it is believed and stops anyone
  looking again;
* undeclared + survived -> the run **fails**, which it did not before.

The last rule is what keeps the category from becoming a way to launder holes:
declaring an equivalent costs writing a proof someone can check.

---

## The immediate port, and RIDGE riding on it

*2026-08-26.* v2's instruction interface has been `{op, dst, a, b, c}` since it
was written, because nothing it executed needed more. `ins_imm_i` is the first
addition to it.

Three opcodes read the immediate and they read it for **two different kinds of
reason**, which is worth separating because only one of them is obvious:

* RIDGE and NOISE2 take it as a hash **seed** -- a wrong seed gives different
  noise, which is visibly wrong terrain and would be caught by any value test;
* ROT3 takes it as an **axis select** -- and a dropped axis rotates about the
  wrong one, which is a *plausible* world rather than a broken one. That is the
  dangerous half, and it is why the immediate is carried through the serialiser
  captured-once like the tag rather than read live.

### RIDGE cost nothing beyond the port

`zhao_field_noise` serves RIDGE on `is_ridge_i = 1`, reads `reg[a]` and `reg[b]`
from the natural ports, and returns one value on `o0_o`. So it dispatches from
stage 1 beside RING and CURVE, and the operand bundle carries `reg[b]` in `a1`
exactly as RING's inner radius does.

**NOISE2 is deliberately NOT wired**, though it is the same unit and one mode
bit away. It writes two registers and the reply carries one. Wiring it now would
mean either dropping `o1_o` -- half an answer, silently -- or bolting a second
write onto a path not designed for it. It stays REFUSED with a status until the
multi-result reply exists, which is the next increment.

`o1_o` is therefore lint-waived as unused with a comment saying why and what
will read it. An unexplained waiver is how a dropped output becomes permanent.

### Measured

RIDGE: 16 instructions in **532 clocks** across 8 wavefronts -- three times
cheaper than RING and two and a half times cheaper than a length. The noise unit
is a hash, not an iterative walk, so it answers in a handful of cycles and the
serialiser's LANES x II is small.

### The seed needed a test that value-matching could not give

A seed dropped to zero, hard-wired, or read live still produces perfectly
plausible noise -- and it AGREES with an oracle handed the same wrong seed, so
the check and the defect cancel and the section reports green.

Section 15 therefore runs **the same coordinates under two seeds and requires
them to disagree on every lane**. That, not the per-lane match against
`zfield::interpret`, is what proves the immediate arrived.

**The same trap is waiting for ROT3**, where the immediate is an axis select:
rotate about the wrong axis and the world is still a world.

### The sweep: 45 mutants, 42 caught, 2 equivalent, and one that FAILED the run

    attempted=45 accounted=45 caught=42 equivalent=2
    SURVIVOR: M119 the immediate is taken LIVE rather than carried
    FAILED: 1 mutant(s) survived without a proof of equivalence

Exit 12. Before this session that run would have exited 0 with the survivor
merely listed, and a gate checking only the exit code would have passed it. The
equivalence rules earned their place on their first outing.

**M119 could not have been caught by either existing test, for two different
reasons.**

The lanemux test *should* have caught it: the tag's own mutants (M85, M86) fall
only because that bench POISONS the request lines after accept, and without the
poison a live-read tag reads the same bits as a carried one. But `req_imm_i` was
added to the RTL **after** that test was written, and the bench had zero
references to it -- it drove nothing and poisoned nothing.

The core test could never have caught it, and that is structural: **the long-op
interlock keeps one request in flight, so `lq_imm` sits stable while the
serialiser works and live equals captured by construction.**

### The rule this produces

> When a field is added to a captured-once request, THE POISON LIST MUST GROW
> WITH IT.

That block is the entire proof that anything is carried rather than read live,
and it is a hand-maintained list. A field added later is a field silently exempt
from the proof, and it will not show up as a missing test -- it shows up as a
mutant that survives for reasons that look like equivalence.

This applies immediately to the next increment: `rsp_y1_o`, `rsp_y2_o` and the
result count all join the same request/reply and all need the same treatment.

---

## NEXT: the multi-result reply, the last mechanism the opcode set needs

*2026-08-26, before any RTL.*

Four opcodes remain -- NOISE2, ROT2, ROT3, NORMALIZE2/3 -- and they are blocked
on one thing between them: **a reply that carries more than one value.**

| op | writes | unit output |
| --- | --- | --- |
| NOISE2 | dst, dst+1 | `o0_o`, `o1_o` |
| ROT2 | dst, dst+1 | `o0_o`, `o1_o` (`o2_o` zero by law 5) |
| ROT3 | dst, dst+1, dst+2 | `o0_o`, `o1_o`, `o2_o` |
| NORMALIZE2/3 | dst .. dst+2 | its own pair/triple |

The v1 units already produce all of it. The seam does not carry it.

### What changes

* `zhao_field_v2_lanemux`: the unit port gains `u_result1_i`/`u_result2_i` and
  the vector reply gains `rsp_y1_o`/`rsp_y2_o`, captured per lane exactly as
  `y_q` is today.
* `zhao_field_v2_core`: the write-back writes `{lm_rsp_wf, lm_rsp_dst + k}` for
  k in 0..count-1, with the count carried in the request like the mode.
* the steal predicate generalises from `s1_is_len` to "needs a second pass",
  since all four read `a+1` and two read `a+2`.

### The hazard, named before it is built

v2 has **no forwarding**, and the argument for that is: a wavefront issues again
only after its previous instruction has written back. That argument was made
about ONE register.

A three-register write-back does not obviously break it -- the writes still land
before the wavefront is released, because the release happens at the reply. But
the reasoning must be re-made rather than assumed, and it wants its own mutant:
**a write-back that lands dst+1 one cycle late** would be invisible to any test
whose next instruction does not read dst+1 immediately.

That mutant is the point of the increment. The RTL is the easy half.

---

## Correction: NORMALIZE does not call the shared reciprocal

*2026-08-26. A committed proof said it did. It was wrong, and a wrong proof is
the failure the equivalence mechanism exists to make expensive.*

M116's declaration named NORMALIZE as the shared reciprocal's future second
consumer, and the routing note above told the next person to ask "does this
operation call anything that also needs the shared lane -- NORMALIZE does".

**`zhao_field_normalize` has no `rcp_*` ports at all.** It carries its own
`zhao_field_rcp24_rom` and shares only the integer square root
(`sqrt_valid_o`/`sqrt_r_i`).

The shared reciprocal's real second consumer is **OP_RCP**, the standalone
reciprocal opcode, and v1 says so in one line:

```systemverilog
assign rcp_valid = op_is_ring ? rg_rcp_valid : (v_valid_i && op_is_rcp);
```

### What this changes and what it does not

The **equivalence still holds**: the ring is the only consumer today, so `&&`
still cannot be told from `||`. What was wrong is the RE-SCORE TRIGGER -- the
event that ends the equivalence. Someone wiring NORMALIZE and dutifully
re-scoring M116 would have found it still surviving and concluded the proof was
unreliable; someone wiring OP_RCP would not have known to re-score it at all.

The declaration now names OP_RCP, cites v1's line, and records that an earlier
version named NORMALIZE and was wrong.

### The lesson about proofs specifically

This error was cheap to make and cheap to catch: I asserted a module's
dependency from memory of a related module rather than from its port list, and
found it by reading `zhao_field_normalize`'s ports for an unrelated reason.

**A proof of equivalence is a claim about the design, and it deserves the same
evidence standard as a claim about behaviour.** "NORMALIZE calls the reciprocal"
was checkable in one grep and I did not run it.


---

## NEXT: ROT2 and ROT3, and the fifth shared resource

*2026-08-26, before any RTL. The multi-result reply exists now, so these are the
first ops it was built for.*

### The oracle resolves and the unit is ready

`zfield_interpret.cpp` gives ROT2 as a plane rotation of `reg[a]`,`reg[a+1]` by
`reg[b] & 0xFFFF`, and ROT3 as the same about an axis chosen by `ins.imm`, with
the third lane carried through unchanged. `zhao_field_rot` implements both on
`is_rot3_i` + `axis_i`, and its header states the four cases explicitly:

    ROT2         p = a0, q = a1                     (two lanes only)
    ROT3 imm=0   p = a1, q = a2,  a0 passes through  (X)
    ROT3 imm=1   p = a2, q = a0,  a1 passes through  (Y)
    ROT3 else    p = a0, q = a1,  a2 passes through  (Z)

### Everything they read is already in hand

| needed | where it comes from |
| --- | --- |
| `reg[a]` | pass 1, saved in `s2_a0` |
| `reg[a+1]`, `reg[a+2]` | the steal cycle |
| angle `reg[b]` | pass 1, saved in `s2_b0` |
| axis | `ins.imm`, which RIDGE already added |
| 2 or 3 results | the reply the last increment widened |

So ROT costs no new front-end mechanism at all. Both take `s1_needs_pass2`.

### What is new: THE SINE TABLE, v2's fifth shared unit

`zhao_field_rot` does not own a sine table -- it borrows the engine's one
`zhao_field_sin` through `sin_angle_o`/`sin_is_cos_o`/`sin_result_i`. v2 has no
sine table at all, because nothing it executes has needed one.

Its contract is stated in its own header and is unusually pleasant: **latency 2,
initiation interval 1** -- registered result, registered table read, and a
request may still be issued every clock.

v1 arbitrates it in one line:

```systemverilog
assign sin_angle = op_is_rot ? rt_sin_angle : a0_i[15:0];
```

ROT is v2's only consumer today. **OP_SIN and OP_COS would be the second** --
and they become nearly free once the table is there: one operand on a natural
port, one result, no steal. That is a bonus this increment buys, not a cost.

It also means the M116 pattern repeats: any "the sine mux is unobservable"
equivalence would be SCOPED to ROT being the only consumer, and re-scored the
moment SIN/COS land.

### The test that matters: THE AXIS

ROT3's `ins.imm` is an axis select, and this is the case flagged when the
immediate was built: **a dropped or hard-wired axis rotates about the wrong
axis, and the result is still a rotation of the right vector by the right
angle.** It is a plausible world. Nothing about the value's shape betrays it.

So ROT3 gets the seed test's structure, sharpened: the SAME vector and angle
under all THREE axes, required to give three different answers, and each matched
against `zfield::interpret` for that axis. A hard-wired axis collapses two of
the three into agreement and fails immediately.

The pass-through lane is the tell: X carries `a0`, Y carries `a1`, Z carries
`a2`. Checking that the untouched lane is the RIGHT untouched lane is the
cheapest possible axis proof, and it is checked separately from the rotated pair.

---

## The multi-result sweep, and three discards that were the ENVIRONMENT

*2026-08-26.*

    attempted=55 expected=55 accounted=52 caught=50 equivalent=2
    CROSS-CHECK FAILED (attempted/accounted must both equal 55)

50 caught, 2 proven equivalent, **3 DISCARDED** -- and the cross-check failed,
which is correct: a run with discards has not tested what it claims to.

### What a discard means, and why it is not a pass

`DISCARDED: a target did not LINK` is guard 5. Without it, a mutant that fails
to compile leaves the previous binary in place and is scored as CAUGHT -- the
most flattering possible way to be wrong. So a discard is the guard working;
what it leaves behind is an UNSCORED mutant, which is a hole in the evidence
rather than a failure of the run.

### My first diagnosis was wrong

Seeing three mutants pass preflight lint and fail the test build, I concluded the
two checks disagreed and the preflight was overstating its coverage.

**They do not disagree.** Applying M120 by hand and building it succeeds. The
preflight was right; the failure was environmental -- almost certainly the
executable being held while the sweep deletes and relinks it, which is a normal
Windows hazard and exactly the kind of thing that strikes three times in 55
iterations and never in one.

The wrong diagnosis was cheap to hold and would have been expensive to act on: I
would have gone looking for a flag difference between two paths that agree.

**The first reproduction attempt was also wrong** and is worth recording: I ran
`cmake --build` alone and saw it succeed. `verilate()` elaborates at CONFIGURE
time -- that is guard 1, written in this sweep's own header -- so a build without
`cmake -S . -B build` does not re-elaborate and proves nothing. I walked into the
guard the file exists to warn about.

### The fix: retry once, and SAY SO

A transient link failure should not cost a mutant's coverage, and a genuine
build failure must still be discarded. Those are distinguishable by trying
again: the environment succeeds on the second attempt, a broken mutation fails
twice.

The retry prints that it happened. Silently retrying would convert a flaky
machine into invisible slowness; printing it keeps the flakiness visible while
recovering the coverage.

---

## ROT2/ROT3 landed; NORMALIZE2/3 is the last, and it needs an ARBITER

*2026-08-26.* 79 checks. ROT cost no new front-end mechanism, as predicted --
`reg[a..a+2]` and the angle in `reg[b]` all arrive on the second read pass, and
two or three results ride the reply NOISE2 opened.

### The selector was widened rather than squeezed

The unit selector was two bits and all four codes were taken; ROT is the fifth.
It is now three bits.

The alternative was to share `UNIT_CURVE`'s code and disambiguate by the mode.
That was rejected on a principle worth writing down: **a selector that needs a
second field to disambiguate it is a selector that will eventually be read
without one.** The cost of widening is a wire; the cost of the squeeze is a
class of bug that only appears when someone reads the field in a new place.

### The axis, tested twice over

ROT3's immediate selects the axis, and a wrong axis still rotates the right
vector by the right angle -- a plausible world. Two independent checks:

* the same vector and angle under **all three axes**, required to disagree;
* the **pass-through lane** -- X carries `a0` untouched, Y carries `a1`, Z
  carries `a2`. That is the cheapest possible proof the axis arrived, and it is
  checked separately from the rotated pair.

ROT2 additionally must NOT write `dst+2`: its third lane is zero by the unit's
law 5, and the register belongs to whatever the program left there. Pre-loaded
with `0xFACEFEED` and checked untouched.

### NORMALIZE2/3: everything is in hand EXCEPT one thing

| needed | status |
| --- | --- |
| `reg[a..a+2]` | the second read pass, built |
| two or three results | the multi-result reply, built |
| `is3_i` mode | the mode field, built |
| `rcp0_o`, `sat_rescale_o` | ledger lanes, and by now a habit |
| the shared multiplier | the priority chain, built |
| **the shared INTEGER SQUARE ROOT** | **wired directly to the length unit** |

`zhao_field_normalize` takes `sqrt_valid_o`/`sqrt_n_o`/`sqrt_r_i` -- and v2's
`zhao_field_isqrt` is connected straight to `u_len`, because the length family
was its only consumer. **NORMALIZE makes it two**, so those four wires become a
mux on the captured unit id, exactly as the multiplier did.

The same argument licenses it: the interlock keeps ONE long op in the machine,
so at most one unit is active. And the same caveat applies -- it is a mux, not an
arbiter, and it becomes wrong the moment two long ops can run concurrently.

**It does NOT use the shared reciprocal.** It carries its own rcp24 ROM. That
was the error corrected earlier today, and it holds: M116's re-score trigger is
OP_RCP, and wiring NORMALIZE does not fire it.

### One oddity to respect rather than smooth over

`rcp0_o` is documented as "set only by NORMALIZE2, see law 3". NORMALIZE3 does
not raise it. That asymmetry is the unit's law and the oracle's; a test that
expects the lane on NORMALIZE3 would be testing my assumption, not the design.

---

## ALL FOURTEEN OPERATIONS RUN ON v2, and M149's two wrong readings

*2026-08-26.* NORMALIZE2/3 landed at 109 checks. The instruction set is complete
on the new engine, and the ALU path is still **3.97 vertex-instructions per
clock**, 27.8x v1 -- unchanged from the day it was measured, which was the thing
to protect while everything else was added.

    sweep: 71 mutants = 69 caught + 2 proven equivalent, 0 unexplained

### The square root got a second consumer

It was wired straight to `u_len` because the length family was the only caller.
NORMALIZE makes it two, so it is muxed on the captured unit id -- the same shape
as the multiplier, licensed by the same interlock, carrying the same caveat.

**Section 18c is the check that earns it.** A mux that fed normalize by
STARVING the length family passes 18a and 18b completely, because both only
exercise the new opcode. 18c runs NORMALIZE3 and LEN3 in one program and checks
both. Two mutants attack it in each direction.

### M149, and why I was wrong twice

    M149 normalize's rescale saturation never reaches the ledger  *** SURVIVED ***

**First reading: "another ledger hole -- write a saturation test for
normalize."** Wrong. A probe of the shipped primitives -- 6,000,000 random
vectors plus the extremes -- gives ZERO rescale saturations, with the worst
output magnitude exactly 65,536. Normalize returns a UNIT VECTOR; the lane fires
above 2^31. It is unreachable by construction, not by omission.

**Second reading: "then it is a proven equivalent."** Also wrong, and this is
the one worth keeping. The mutation is `(cv || ln || rg || nz) && nm`. If `nm`
can never fire, the mutant does not merely neuter normalize -- **it silences
`sat_rescale_o` for EVERY unit in the engine.** It survived because
`sat_rescale_o` appeared NOWHERE in the differential: an entire ledger lane was
untested across all fourteen operations.

Had I stopped at the first reading I would have written a test that can never
fire. Had I stopped at the second I would have signed a FALSE PROOF OF
EQUIVALENCE -- which this sweep's own rules call worse than an open hole,
because it is believed and stops anyone looking again.

The fix goes where the lane is reachable: a DIST2 whose two differences both
saturate to INT32_MAX has length 3,037,000,498, which does not fit in s32. Both
directions, with the oracle asserting first that the operands really do overflow.

### The generalisation

A mutant that ANDs a never-firing term into a condition does not test that term
-- it tests **everything else in the condition**. When a survivor's mutation
touches a signal that cannot fire, the question is not "is this equivalent" but
"what else did I just disable, and was any of it tested".

