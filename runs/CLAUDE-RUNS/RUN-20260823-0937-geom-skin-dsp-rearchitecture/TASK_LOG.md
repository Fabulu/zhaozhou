# TASK LOG — RUN-20260823-0937-geom-skin-dsp-rearchitecture

**Started:** 2026-08-23 09:37 UTC+02:00
**Branch:** main
**Base commit:** 7cd3d8c
**Spec:** SPEC_v1.md

> Everything in this log is **simulation, synthesis or fit**. No hardware has
> run any of it.

---

## 09:37 — run initialized

    C:\programmieren\zencrifice\runs\CLAUDE-RUNS\init-run.ps1 geom-skin-dsp-rearchitecture
    => Run ID: RUN-20260823-0937

`init-run.ps1` writes to `C:\programmieren\zencrifice\runs\CLAUDE-RUNS\`, which
is **outside the zhaozhou repository** (zencrifice is not a git repo). The
tracked copy lives at `zhaozhou/runs/CLAUDE-RUNS/`, which is where every
previous run's FINDINGS were committed, so the folder was mirrored there and
this log is the tracked one. `ARCHIVE.md` exists only in the zencrifice copy
and is updated at the end.

Machine state at start: `quartus_map.exe` (PID 24612) and `quartus_sta.exe`
(PID 2892) already running — the Field agent's constrained `zhao_field_seq`
fit. **No fit started from this run until the machine was free.**

## 09:50 — STEP 1, before any RTL: does the oracle resolve? (ledger rule V17)

    npm run ledger:check
    => ledger: V2 git-history — comparing against HEAD
       ledger: V16 formal registry — 23 recorded run(s)
       ledger: CHECK FAILED — 1 error(s) against 92 blocks / 40 ops
         - V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal
           "tests/formal/field_seq_bound.sby" is recorded as "pending", not green

**V17 is green. The oracle resolves.** `GEOM.SKIN` cites
`zref::creature::skin_vertex`, defined at
`reference/src/zcreature/creature_core.cpp:222`, declared in
`reference/include/zref/zref_creature.hpp`, named in both the contract's
*Scalar reference function* section and the test file — which is the four-part
V17 check (definition exists, contract cite matches `reference_model`, cited
test paths exist, test text mentions the oracle).

The single V16 error is the Field agent's deliberately-pending formal status,
confirmed by the coordinator as not mine. **It is this run's ledger baseline: a
second error would be mine.**

## 09:55–10:10 — the measurement that starts the argument

`reports/synthesis/zhao_block_fit.json`, row `zhao_geom_skin`, sourceCommit
`16df9ee`, `rtlCleanAtHead: true`:

| | measured |
| --- | ---: |
| ALMs | 1,801 |
| **DSP blocks** | **72** |
| registers | 145 |
| virtual pins | 1,038 |
| seconds | 400.2 |

`reports/REMAINING_BLOCKERS.md:1223` had explicitly **not** queued this block
for sequencing, and gave a good reason: vertices are the highest-rate object in
the geometry pipeline, so unlike `geom_lod` (once per instance per frame) or
`terrain_lod` (once per patch per frame) the parallelism might be earned. It
said the answer needed a vertex budget nobody had stated.

**The budget is now stated: ~120,000 skinned vertex instances per 60 Hz frame.**

    gpu_clk                 100 MHz (10.000 ns; zhao_shell_fit.sdc:4, and the
                            per-block SDC run_block_fit.ps1 generates)
    clocks per frame        1,666,666
    demand                  120,000 vertices
    clocks per vertex        13.88
    products per vertex      18 two-weight (2 matrices x 3 rows x 3 terms), 9 rigid
    honest multipliers       18 / 13.88 = 1.30

**The one-clock form was over-provisioned by 13.9x.** The recurring cause named
in the brief — parallel multipliers where the block's rate does not require
them — is true here, by an order of magnitude. Checked rather than assumed, as
instructed, and `REMAINING_BLOCKERS.md` guessed the other way in good faith
without the number.

### The finding that changes what the blend identity is worth

**72 = 18 x 4.** A signed 32x32 in the old combinational cone cost four DSP
blocks; the six 7-bit weight multiplies cost approximately **none** — Quartus
had already put them in logic.

So the `(pb << 6) + w0*(pa - pb)` identity in the brief is exact, is used, and
saves **ALMs, not DSPs**. Recorded here because a report crediting the DSP
reduction to the identity would be crediting the wrong change.

## 10:10 — architecture: lanes by TERM, not by row

The brief proposed three ROW lanes. Evaluated and **rejected in favour of TERM
lanes**:

| | row lanes | term lanes |
| --- | --- | --- |
| coordinate operand | 3:1 mux per lane per cycle | **fixed wire** (lane 0 is always x) |
| matrix operand | 6:1 mux | 6:1 mux |
| a row-product completes | only after the last issue cycle | one cycle per row-product |
| blend can overlap issue | no | **yes**, saves 2 clocks |

With three term lanes a whole row-product is a one-cycle three-lane dot
product, so `pa[r]` and `pb[r]` become final three cycles apart — exactly the
cadence a one-row-per-cycle blend walk consumes them at. That is why the blend
is ONE shared shift-add unit and not three, at no cycle cost.

### The parameter and its legal settings

`MUL_LANES` = TL term lanes x RL row-product lanes:

| `MUL_LANES` | TL | RL | issue slots blend/rigid | latency blend/rigid | vertices/frame |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 1 | 18 / 9 | 22 / 13 | 75,757 — **FAILS demand (63%)** |
| 3 | 3 | 1 | 6 / 3 | 10 / 7 | 166,666 — 1.39x |
| 6 | 3 | 2 | 3 / 2 | 8 / 7 | 208,333 — 1.74x |

`MUL_LANES = 2` is refused at elaboration: 2 divides neither the three terms
nor the three rows, so a cycle would straddle two row-products and every
accumulator would need a masked multi-source adder. The guard is an unresolved
module reference inside a generate-if — the portable static assertion.
`$error` was deliberately not used: Quartus 17.0.2's support for elaboration
system tasks is not something to discover twenty minutes into a fit.

**`MUL_LANES = 1` is kept because it fails.** A frontier with no failing end
does not show where the wall is.

## 10:15 — widths proven before synthesis (QUARTUS_GOTCHAS §5)

§5 cost `zhao_geom_lod` ten DSP blocks. The old skinner carried 67- and 75-bit
lanes on "slack is free" reasoning. Proven from the s32 input widths alone:

| quantity | bound | signed bits | was |
| --- | --- | ---: | ---: |
| `m*x` | <= 2^62 | 64 | 64 |
| `pa` | <= 3*2^62 + 2^47 = 1.3835e19 < 2^64 | **65** | 67 |
| `pa - pb` | <= 2.767e19 < 2^65 | **66** | — |
| `w0*(pa-pb)`, w0<=63 | <= 1.743e21 < 2^71 | **72** | — |
| `(pb<<6) + w0*(pa-pb)` | <= 2.629e21 < 2^72 | **73** | 75 |

These are adders, so the saving is ALMs. The multiplier operands stay a full
signed 32x32 — the audit's "a bone matrix is a bounded rotation" is true of the
content and false of the contract.

## 10:20 — RTL rewritten

`fpga/rtl/geometry/zhao_geom_skin.sv`. Lint, all three settings plus the
illegal one:

    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=1 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=3 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=6 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=2 ...
      => %Error-MODMISSING: Cannot find file containing module:
         'ZHAO_GEOM_SKIN_MUL_LANES_MUST_BE_1_3_OR_6'

One lint fix on the way: `UNUSEDSIGNAL: 'idx'[31:5]` — an `int` index into a
24-entry array. Narrowed to `logic [4:0]`, which is also the honest width.

## 10:25 — the two things the contract said must be closed BEFORE the rewrite

`design/contracts/GEOM.SKIN.md` had recorded the weight identity as **not
done**, with two blockers. Both were real and both are closed.

**1. The identity is FALSE for `w0 > 64`** (223,020 mismatches in the
contract's own sampled sweep) and needed an `ENFORCED-BY:` naming who upholds
`w0 <= 64`. Closed with `require_legal_w0()` in the differential, which runs on
every vertex the test drives and fails the suite if any driver presents
`w0 > 64`. The RTL header names it. **The upstream HARDWARE obligation is
recorded as an owner-docket item on GEOM.VDECODE** (SPECIFIED, no RTL) rather
than papered over — and GEOM.SKIN deliberately does not add a defensive clamp,
because that would invent behaviour in the out-of-contract region and hide the
missing upstream check behind a plausible answer.

**2. The differential did not reach the operand extremes**, and the rewrite
made that worse rather than better: narrowing 67 -> 65 and 75 -> 73 turned an
untested argument into a load-bearing one. Closed with:

- **section 7, the row-product rails** — 9 rail values for matrix elements x 9
  for the vertex, with `B = -A` so `pa - pb` reaches twice `|pa|`, across
  w0 = 0, 1, 2, 32, 62, 63 plus rigid;
- **the near-cancellation family** — `B = -A` at `w0 == 32` makes the blend
  `32*(pa + pb)`, so the ANSWER is small while every intermediate sits at its
  bound. A lane one bit too narrow wraps and gives a large wrong answer here,
  where anywhere else it would give the same saturated rail as the correct one.
  This is the case that distinguishes a correct width from a lucky one;
- **section 7b** — 600 full-range random iterations, a SEPARATE lane rather
  than a widening of `--random`. The pose-range lane is deliberately aimed away
  from the rails and a recorded mutation property depends on that (the
  no-saturation mutation fails 6 directed checks and passes 900 random ones);
  widening it would have destroyed a known property to gain one the directed
  sections can carry.

## 10:30 — the rate is now a test, not a comment (section 8)

The DSP count is justified by a frame budget, so the frame budget is asserted:
accept-to-valid latency, the sustained issue interval of a never-stalling
stream, that the two agree, and that the resulting vertices/frame lands on the
**correct side** of 120,000 — above for `MUL_LANES` 3 and 6, and **below for
1**, which is checked as a failure so the frontier's failing end cannot quietly
start passing.

## 10:35 — the frontier is BUILT, not just parameterised

`tests/CMakeLists.txt` now elaborates `zhao_geom_skin.sv` three times:
`test_geom_skin_directed` (MUL_LANES=3), `test_geom_skin_lanes1`,
`test_geom_skin_lanes6`, each with `-GMUL_LANES=<n>` and a matching
`ZHAO_SKIN_LANES` compile definition carrying the frontier table's latencies.

**This is not ceremony.** `MUL_LANES = 3` collapses the term walk (TSTEPS == 1),
so the default build **never executes the multi-cycle accumulate path at all**.
Only the `MUL_LANES = 1` build does. The two extra targets are written out
rather than generated in a `foreach`, because the mutation sweep's guard 7
derives its consumer set by grepping `verilate(<target> ... <file>)` out of
`tests/CMakeLists.txt` and a `${lanes}` in the target name would leave it
deriving a literal that matches no target.

## 10:40 — coordinator constraints received and honoured

1. `build/` was wiped and is being reconfigured clean; no `cmake`/`ninja`
   against it until cleared. Held.
2. Build via presets, env dot-sourced in the SAME invocation:
   `. .\tools\env\zhao-env.ps1; cmake --preset windows-native; cmake --build build`.
3. Ledger baseline is the one V16 error. Re-checked after the `blocks.yml` and
   contract edits: still exactly one error, still V16. **No error introduced.**

## 10:45 — documents updated

- `design/contracts/GEOM.SKIN.md`: latency (was `fixed:1`), target throughput
  (was "one vertex per clock", now the derived frame budget), backpressure (the
  `!busy` term and the palette latch), clock/reset (the engine state reset
  adds), Q formats (the proven width table), the weight-identity section
  rewritten from *not done* to done with both blockers checked off, the tests
  sections, and the synthesis section with the before numbers.
- `design/blocks.yml`: `latency: variable`, `target_throughput` restated from
  the frame demand.

---

## Open at this point in the run

- **The build gate has not run.** Waiting on the coordinator's clean rebuild.
- **No fit has been run.** `quartus_fit.exe` PID 30196 was still live at 10:45.
- The sweep (`tools/sweep_geom_skin.sh`, 28 mutants, preflight lints every
  mutant at all three MUL_LANES settings) is written but not run: its preflight
  temporarily writes mutated text into the RTL, which must not happen while
  another agent's `cmake` configure is elaborating that file.
