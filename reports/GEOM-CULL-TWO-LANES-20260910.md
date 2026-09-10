# GEOM-CULL TWO LANES — `zhao_geom_cull`'s plane arithmetic on shared multipliers

Run: `RUN-20260910-0631-geom-cull-two-lanes` · lane: `fpga/rtl/geometry/zhao_geom_cull.sv`
+ its tests · **no fit run, no commit made** (both per brief). Brief:
`reports/RESCUE-ROADMAP-CONSOLIDATED-20260909.md` §"`geom_cull`'s lane count,
derived rather than guessed" (−9 DSP, two lanes).

**Headline.** `MUL_LANES` (default **2**) sequences the cull's products through
two shared 33×33 lanes; the legacy spatial arm is `MUL_LANES=4` under `generate`.
Predicted **15 → 6 DSP, return 9** — the brief's number, reached by different
arithmetic (§A1). All three arms pass the same 40,988-check differential
bit-for-bit; the checker was seen to fail on a committed mutant (7,621/39,402).
**Unfitted.** And the brief's throughput derivation is wrong by 10×, which
changes nothing about the answer and everything about the reasoning under it.

## What shipped (working tree, awaiting review)

| File | Change |
|---|---|
| `fpga/rtl/geometry/zhao_geom_cull.sv` | `MUL_LANES` ∈ {1, 2, 4}, default 2. One arm-agnostic FSM; two `generate` arms exporting the same seven signals. `g_spatial` (4) is the fitted circuit moved verbatim into its arm; `g_seq` (1, 2) is the terrain_normals/mat3x4 issue-commit pattern with registered products, extraction squares on lane 0, `outside = sign(dot + slack)`, and the slack product split at bit 32 so **no lane operand exceeds 33 bits**. `initial begin` guards on `MUL_LANES` and on the split's `LEN_W` assumption. Ports unchanged. |
| `tests/differential/geom_cull_directed.cpp` | **Additive only.** Exact walk law per instance (`ZHAO_CULL_WALK`, default 21) and ready-with-valid (II = walk + 1) in `dut_cull`; §12 pins one-write extraction (`ZHAO_CULL_EXTRACT`, default 191) for both views; §13 drives the **domain-violating negative radius** (rails camera, `INT32_MIN`) against the oracle. 17,212 → 40,988 checks. |
| `tests/mutants/zhao_geom_cull_mutant.sv` | committed mutant, renamed module, **two lines differ from the source**: the rename and `psum[0] = first_q ? (acc + kterm_q) : acc` (the plane-boundary clear removed) |
| `tests/differential/geom_cull_mutant_control.cpp` | inverted-polarity positive control, new |
| `tests/CMakeLists.txt` | 5 targets: `geom_cull_spatial` (−GMUL_LANES=4, walk 10/186), `geom_cull_lane1` (−GMUL_LANES=1, 41/191), `lint_zhao_geom_cull_spatial`, `geom_cull_mutant_control`; existing `geom_cull_*` targets untouched (defaults 21/191 via `#ifndef`) |
| `design/contracts/GEOM.MESHFETCH.md` | latency row, "bound arrives as PORTS" bullet, "obvious next candidate" paragraph and resource row brought to the shipped default; a dated note naming the initiation-rate conflict (§H) |
| `runs/CLAUDE-RUNS/RUN-20260910-0631-geom-cull-two-lanes/` | `TASK_LOG.md`, `build-culllane.ps1` (the standalone recipe) |

**Not touched:** `design/fit_targets.yml`, `design/prod_manifest.yml`,
`design/budgets/workloads.yml`, `zhao_prod_top.sv` (the cull's port list is
unchanged and `u20_i` has no parameter overrides, so the new default reaches
production composition with no regeneration; the STALE the manifest checker
reports is the other lane's u29/u57 port changes — 0 `u20` lines in that diff),
`zhao_project_core.sv` and the projection wrappers (other lane),
`tools/sweep_geom_cull.sh` (now stale, §I).

## A. The brief needed corrections — the eleventh claim, and four smaller ones

**A1. "Four products per evaluation" is four products per PLANE-CYCLE.** The
roadmap counted `zhao_geom_cull.sv:369-373` — three `mul_pc` plus one
`mul_slack` — and called that an evaluation. Those lines execute once per
S_EVAL cycle, and S_EVAL runs **ten** cycles per instance (five planes × two
views; the RTL walks both views whatever `active_i` says). An evaluation is
**40 products**. The brief's central arithmetic therefore reads:

    roadmap:    4 × 333,333 = 1,333,332 products  vs 1,333,332 reserved clocks  = 100.0000% on one lane
    actual:    40 × 333,333 = 13,333,320 products  vs 1,333,332 reserved clocks  = 1000%    on one lane
                                                                                  =  500%    on two

The "exactly 100.0000%, a coincidence to design against" does not exist; the
one-lane arrangement is not on an edge, it is off by an order of magnitude —
and so is every other arrangement against that row, including the fitted one
(§C). The recommended lane count survives (§C, §D) for reasons the brief did not
give: the contract's own demand figure, not the workloads row.

**A2. Today's II is 11, not 10.** The contract's "10 cycles on the per-instance
path" counts S_EVAL cycles; the accept cycle in S_IDLE is not overlapped, so a
new instance enters every 11 clocks. Measured on the pristine RTL by the new
walk law at `MUL_LANES=4` (walk 10 ticks to `valid_o`, `ready_o` high in that
same cycle → II 11).

**A3. `workloads.yml`'s cull row is the LOD ladder's rate.** `zhao_geom_cull:
itemsPerFrame: 333333, requiredII: 5, confidence: ruled, source: docket
2026-08-23 "one evaluation per five clocks"`. The docket's five-clock figure is
`zhao_geom_lod`'s (docket line 2880: *"THE LOD LADDER NOW TAKES FIVE CLOCKS"*),
and the contract's target-throughput section says the composed block's 5-clock
rate is *"set by the LOD ladder's sequenced multiplier"*. The cull never met
that row in any arm: the FITTED circuit at II 11 needs 3,666,663 clocks for
333,333 evaluations — 275% of the reserved frame. `measuredII: null` is why
nobody saw. An owner re-ruling is needed; not made here (§H).

**A4. The brief's DSP arithmetic is right by accident of widths.** "5 sites × 3 =
15 → 2 × 3 = 6, −9" assumed a shared lane sized for the widest operand still
costs 3. The widest operand today is `mul_slack`'s **34-bit unsigned** length
bound, and a 33s × 35s lane sits **between the calibration's measured points**
(s33 → 3 DSP, s40 → 4 DSP; nothing at 34..39). Rather than bet on an
unmeasured cliff, the slack product is split at bit 32 (§D) so every lane is a
measured 33×33 shape. The −9 holds, with the widths honestly accounted for.

**A5. The header's `LEN_W` bound is misprinted.** "sqrt(3·2^64) < 2^33.8, so 34
bits": the exponent is 32.79, so 33 bits hold it. Harmless (one spare bit in
ten registers); `LEN_W` left at 34, header corrected, and the split handles
both high bits so the arithmetic is exact for every value the register can hold.

## B. Verification — all standalone, shared `build/` untouched

Recipe: `runs/.../build-culllane.ps1` (verilator_bin `--build` into gitignored
`build-culllane/<name>/`, `-std=gnu++17`, absolute includes, fresh Mdir each
build so no object can be stale). Baseline RTL taken from `git show HEAD:`.

| Build | RTL | Test source | Result |
|---|---|---|---|
| `base` | **pristine** (HEAD) | UNCHANGED | **17,212 checks passed** |
| `l2` | new, `MUL_LANES=2` | UNCHANGED (objects predate the test edit: 06:43 vs 06:45) | **17,212 passed, output byte-identical to `base`** — the brief's "must pass unchanged" |
| `l2w` | new, 2 | + walk laws + §12 + §13 | **40,988 passed**, walk 21 (II 22), extraction 191 |
| `l1` | new, 1 | same, WALK=41 EXTRACT=191 | **40,988 passed**, walk 41 (II 42), extraction 191 |
| `l4` | new, 4 (legacy arm) | same, WALK=10 EXTRACT=186 | **40,988 passed**, walk 10 (II 11), extraction 186 |
| `l2w --random 4000` | new, 2 | (before §13) | 55,411 passed |
| `mutfail` | **mutant**, prefix `Vzhao_geom_cull` | UNCHANGED directed test, unmodified | **7,621 / 39,402 FAILED** — checker SEEN TO FAIL — while walk 21 and extraction 191 STILL HOLD on the mutant |
| `mutctl` | mutant | inverted-polarity control | **1,208 passed**; 268/400 verdicts diverge (31 of 163 oracle-visible spheres); a verdict decided by the FIRST plane is still right (boundary-fault signature) |

Every walk length was **derived before it was measured** (test comment states
the derivation) and matched on the first run for all three arms. §13's check
count was predicted at 40,988 (= 39,402 + 396 instances × 4 + 2) and matched.

Lint `-Wall`: clean at `MUL_LANES` = 2, 1, 4. `check_quartus17_syntax.py`: clean,
220 files (the genvars are declared separately inside generate, the form the
checker accepts). `check_prod_manifest.py`: one error, `zhao_prod_top.sv`
STALE — **not this lane** (§"Not touched"). `tools/budget/scan_rtl.py` on the
new default: **2 nonconstant 33×33 signed multiplies, 0 constant, 0 variable
shifts, 0 combinational loops** (two findings from a first draft — a shift in a
loop and a read-modify-write sum — were removed by hand-unrolling the 2-bit
high term and building the lane sum as a generate prefix chain).

## C. Demand and throughput, in clocks — and how close to the edge

Products per evaluation: **40** (§A1). Frame: 1,666,666 clocks raw, 1,333,332
at the 20% reserve.

| Arm | II | Capacity, evaluations/frame (reserved) | vs `workloads.yml` 333,333 | vs contract demand 6,100 (256 creatures × ~24 meshlets) |
|---|---:|---:|---:|---:|
| `MUL_LANES=4` (fitted) | 11 | 121,212 | **275%** — never met | 67,100 clk = **5.0%** |
| **`MUL_LANES=2` (default)** | **22** | **60,606** | 550% | 134,200 clk = **10.1%** |
| `MUL_LANES=1` | 42 | 31,746 | 1050% | 256,200 clk = 19.2% |

**Closeness to the edge, as the brief asked.** Against the contract's demand,
two lanes saturate the reserved frame at 60,606 evaluations — **9.9× headroom**.
But the "~24 meshlets per creature" is the contract's own soft estimate, and
its own vertex census argues higher: median 2,951 vertices at ≤ 64 per meshlet
is **≥ 47 meshlets per creature → ~12,000 decisions/frame**. At that figure two
lanes cost 20% of the reserved frame (5.0× headroom) and one lane 38% (2.6×).
Two lanes is therefore the sane default; one lane is the "DSPs are the wall"
option, and both are one parameter away. Against the `workloads.yml` row as
written, nothing sequenced — and nothing ever built — fits; that row is the
thing to re-rule (§H).

**A lever not taken:** the RTL evaluates both views regardless of `active_i`. In
Solo (one active camera) skipping the inactive view would halve the walk. It
makes the latency data-dependent and changes the contract's "fixed" statement,
so it is named here and not done.

## D. DSP bill — MEASURED / STRUCTURAL PREDICTION / UNKNOWN

**MEASURED (inherited, clean-tree rows):**
* Today's block: **15 DSP**, 1,102 ALMs (fit `2a711f0`, `rtlCleanAtHead: true`);
  map `de11ce9`: decomposition 10 × "Two Independent 18x18" + 5 × "Sum of two
  18x18", i.e. **five 3-DSP multiplier sites** — `mul_pc` ×3 (33×32), `mul_slack`
  (32×34u), `sq_prod` (33×33). The `MUL_LANES=4` arm is this circuit.
* Calibration (`tools/budget/calibration.json`, `dspBlocks`): s28..s33 → **3**,
  s40 → 4, u33 → 3. `calib_mul_s33_n1_ioreg` = 3 is the point the lanes sit on.
* `zhao_terrain_normals` (map `bfc74710`): ONE muxed, registered 33×33 signed
  lane → **3 DSP**. The precedent for "a lane with an operand mux in front and
  the DSP output register behind still costs 3".

**STRUCTURAL PREDICTION:**
* `MUL_LANES=2`: 2 lanes × 3 = **6 DSP, return 9**. `MUL_LANES=1`: **3, return 12**.
* Why every lane is exactly the measured shape: `r·len` is formed as
  `r·len[31:0] + (r·len[33:32]) << 32`. The lane product is 33s × 33s (low word
  zero-extended, non-negative); the high part is two conditional adds of `r`,
  written without a `*`, so nothing can infer a DSP for it. Exact — it is the
  integer identity it looks like — and §13 drives it with negative `r` against
  a bound with bit 32 set.
* The extraction's square shares lane 0; `state` makes S_SQ and S_EVAL mutually
  exclusive, so the share costs one operand-mux arm and saves a whole 3-DSP site
  that ran only on matrix writes.
* Dependencies of the prediction, each named so the fit can refute it: (i)
  Quartus infers `m_a[l] * m_b[l]` as one 33×33 multiplier with the mux before
  it (terrain_normals precedent); (ii) the sign-extension to 66 bits is peeled
  (`scan_rtl` reports `extensionPeeled: true`, honest width 33 — the pristine's
  65-bit widening idiom mapped at 3/site); (iii) the shift-and-add is ALMs.

**UNKNOWN (need the fit):**
* ALMs. Removed: a 4-input 68-bit adder, a 68-bit comparator, three multiplier
  sites' fabric. Added: `p_q` 2 × 66, `kterm_q` 68, `acc` 68, a 3-input 68-bit
  adder, bookkeeping flops, a 3:1/2:1 33-bit operand mux per lane. Net sign
  unknown; magnitude in the low hundreds of ALMs either way.
* Fmax. **No arm of this block has ever had one** — the fit row carries no
  timing field. The sequenced arm's longest paths are (plane mux → 33-bit add →
  operand mux → 33×33 multiply → register) and (register → 3-input 68-bit add →
  sign → `vis_o`), each shorter than the fitted arm's single-cycle
  mux-add-multiply-4-input-add-compare chain.
* Whether Quartus uses the DSP output register for `p_q` (it should; the
  product is registered unconditionally).

**If the honest number were worse than 9 I would say so. It is not: 9 at the
default, 12 at one lane.** What IS worse than the brief is the argument: the
−9 rests on the contract's demand figure, not on the roadmap's 80%/100% table.

## E. Latency and initiation interval — declared

| Arm | `valid_o` after accept | II | one dirtying write → `ready_o` |
|---|---:|---:|---:|
| `MUL_LANES=2` (default) | **21 ticks** | **22** | **191 ticks** (1 + 5 × (4 + 33 + 1)) |
| `MUL_LANES=1` | 41 | 42 | 191 |
| `MUL_LANES=4` (legacy) | 10 | 11 | 186 (1 + 5 × (3 + 33 + 1)) |

All fixed, one instance in flight, `ready_o` high in the verdict cycle. No
existing test was latency-pinned (the pristine `dut_cull` waits up to 64;
`wait_ready` up to 4000), so **no pin was moved** — pins were ADDED, exact, per
arm. The consumer `zhao_geom_meshfetch` handshakes on `cull_ready_i` /
`cull_valid_i` (S_CULL / S_WAIT) with no cycle count assumed.

**Bit-exactness argument.** Each product is an exact integer whichever cycle it
is formed in; `dot < -slack` is evaluated as `dot + slack < 0` on a 68-bit
accumulator whose every partial sum is under 2^66 in magnitude (three products
≤ 2^63, `d<<16` ≤ 2^48, slack < 2^65), so the predicate is identical on exact
integers. §5b's constructed exact-equality cases (`dot == -r·len`) and §13's
negative radii are where `<` vs `<=` and the split's sign would show, and all
three arms agree with the oracle there.

## F. THE ONE fit gate — named, NOT run

`design/fit_targets.yml:1558` already targets `zhao_geom_cull` (leaf; sole
source `zhao_geom_cull.sv`). No edit to the ledger was needed.

    tools/quartus/run_block_fit.ps1 -Module zhao_geom_cull
    # optional second point of the SAME gate, same run:
    #   -TopParameters MUL_LANES=1 -RowLabel lanes1

**The question:** *does the `MUL_LANES=2` default map to **6 DSP** — two 3-DSP
33×33 lanes, the 2-bit shift-and-add in ALMs, `p_q` in the DSP output registers
— and what are its ALMs against 1,102 and its Fmax (the block's first ever)?*
Per QUARTUS_GOTCHAS 3, if the optional `MUL_LANES=1` point returns the same DSP
count as the default, the parameter was ignored, not irrelevant.

Fit at a subsystem boundary: this block sits in the GEOM closure with the
other lane's `zhao_project_core` change; one fit can carry both.

## G. Not verified — item by item, instrument named

| Claim | Status | Instrument that would settle it |
|---|---|---|
| 6 DSP at `MUL_LANES=2` | STRUCTURAL | the leaf fit (§F) |
| ALM delta vs 1,102 | UNKNOWN | the leaf fit |
| Fmax of either arm | UNKNOWN, never measured | the leaf fit's STA |
| Quartus 17 synthesizability of the new forms (nested generate-for inside generate-if, `genvar` declared in a generate block, `2'(step * MUL_LANES + l)` cast, `68'sd0` in ternaries) | NOT SHOWN — lint-clean is one tool's opinion; `check_quartus17_syntax.py` catches only its three known forms | `quartus_map` (33 s) — first stage of the gate |
| Nightly random (`--random 120000`) | not run; 4,000 run | `ctest -R geom_cull_random_nightly` |
| Mutation sweep (`tools/sweep_geom_cull.sh`, 32 mutants) | NOT re-run; its M06/M07/M17–M20 patterns no longer match the source | regenerate the mutation list against the new text, then run |
| Composed behaviour in `zhao_prod_top` | not built (the top is dirty from the other lane) | the production fit / composed directed |
| `ctest` registration of the 5 new targets | CMake text written, **not configured** — shared `build/` deliberately untouched | `cmake --preset windows-native` then `ctest -R geom_cull` |
| Solo-view skipping would halve the walk | argued, not built | — (named lever, §C) |

## H. Contract findings — the contract wins, so here is where it constrains

1. **`GEOM.MESHFETCH.md` contradicts itself on this lever.** Its synthesis
   section: *"latency may grow; initiation rate and exact arithmetic may not
   regress."* Its "What is already built" section: the cull *"is the obvious
   next candidate for the same lever."* Sequencing IS an initiation-rate
   regression (11 → 22). The brief did not mention the first sentence. The
   resolution taken — and written into the contract as a dated note, reversible
   — is that the rate the sentence protects (one decision per 5 clocks) was
   never met by any arm, and the contract's own DEMAND sizing (§C) is what the
   design is held to. **If the owner wants the initiation-rate sentence read
   literally, `MUL_LANES=4` restores II 11 at +9 DSP — one parameter.**
2. **The contract's latency table said 185 / 10; the truth was 186 / 11 (II).**
   Corrected to the shipped default with the legacy arm's numbers kept beside it.
3. **`workloads.yml` `zhao_geom_cull` row needs re-ruling** (§A3). Not edited: it
   is `confidence: ruled` and lives in `design/budgets/`, which the brief said to
   keep minimal. Suggested content: `itemsPerFrame` from the contract's demand
   (6,100 at 24 meshlets/creature, or ~12,000 at the vertex census's ≥ 47),
   `requiredII: 22`, `confidence: derived`, `measuredII: 22`.
4. **The roadmap's derivation section is wrong** (§A1) and its "−9" table entry
   is right. Not edited; this report is the correction. Whoever reads
   `RESCUE-ROADMAP-CONSOLIDATED-20260909.md:104-145` should be pointed here.

## I. Waste ledger and follow-ups

* `tools/sweep_geom_cull.sh` is stale against the new source text (its `sed`
  patterns reference `mul_pc(...)`, `outside_here = (dot < -slack)`, etc.). The
  committed mutant + control cover the new machinery's characteristic fault;
  the sweep's 32 older mutations still describe faults worth re-running once the
  list is regenerated. ~1 hour of builds; not done here.
* `gen_prod_top.py` has no `--help`; invoking it with that flag REGENERATES the
  top. Done once by accident, reverted immediately with `git checkout`, 0 `u20`
  lines affected. Worth a one-line guard in the tool.
* Standalone exes must be run with winlibs' `bin` ahead of oss-cad-suite's, or
  they die with `0xC0000139` and no message (the suite's older
  `libstdc++-6.dll`). `build-culllane.ps1 -Run` does this; recorded so the next
  lane does not lose the twenty minutes.
* `build-culllane/` (gitignored) is deletable after review.
