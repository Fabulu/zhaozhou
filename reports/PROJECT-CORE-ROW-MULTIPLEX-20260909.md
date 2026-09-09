# dsp.md lever 1, cashed: `ROWS_PER_PASS` in the projection core — 33 → 15 DSP per instance, and the Fmax objection answered structurally

2026-09-09. The last uncashed lever of the four `design/budgets/dsp.md:118-134`
opened the campaign with. Levers 3 and 4 were already cashed
(`zhao_terrain_normals` measures 3 DSP; the comparison-narrowing shipped with
it); lever 2 was cashed in SURFACE.STAMP's lane. This report closes the list.

**The verdict up front: pull the lever — in the shared service, after the
arena, at `ROWS_PER_PASS=1` — and the census's Fmax objection does not survive
the implementation shape chosen here.** The RTL is built, parameterised,
lint- and gate-clean, proven byte-identical to the shipping shape across four
stall patterns, and nothing that ships changes until someone flips a
parameter: the default is 3, and at 3 the block is cycle-identical to HEAD
(measured, not argued — same suite, same 423-cycle batch on both).

One number in the brief that opened this lever is corrected below: the honest
DSP at `ROWS_PER_PASS=1` is **15 per instance, not the "~12"** dsp.md
estimated — dsp.md's arithmetic predates the calibration cliff and forgot the
two viewport products. The saving per instance is 18, not 21.

---

## 1. DSP at each `ROWS_PER_PASS`, reconstructed from call sites

The calibration line (`zhao_project_core.sv` COST section;
`tools/budget/calibration.json`, this tool, this device): a signed product is
1 DSP with both operands 8..27 bits and **3** at 28..33. The shipping core
measures 33 = 11 sites x 3, and the map agrees exactly — that reconstruction
is what makes the rows below an account rather than a story.

| `ROWS_PER_PASS` | row `mul32` sites | viewport `fx_mad` sites | total sites | DSP / instance | status |
|---|---:|---:|---:|---:|---|
| 3 (default) | 9 | 2 | 11 | **33** | measured (map + fit, twice) |
| 1 | 3 | 2 | 5 | **15** | structural; the named gate prices it |

* **Why not "~12":** dsp.md's "~33 to ~12, saving ~21" was written 2026-08-20,
  before the 27-bit cliff was measured, using ~4 DSP per 32x32 (3 x 4 = 12) and
  counting only the row products. The two `fx_mad` sites (32x27 — and
  `GEOM.WCACHE.md:81` measured that a wide operand on *either* side forfeits
  the cheap band, so they cost 3 each) do not sequence with the rows and stay.
  Honest saving: **18 per instance**, 36 across two instances, 18 in the
  shared service.
* **Reaching 12 exists as a follow-on**, not in this change: at II=3 the two
  viewport products have three cycles of slack, so one shared `fx_mad`
  multiplier over two rigid stages would drop 2 sites to 1 (−3 DSP). It needs
  an invariant the row sequencer provides for free ("at most one vertex in any
  three consecutive stages") asserted where the mad stage uses it, and it is
  the kind of complexity that should ride the SAME fit gate, or none.
* **`ROWS_PER_PASS=2` was considered and rejected**: three rows do not divide
  by two, so a 2-row pass gives an asymmetric bank or an operand mux back in
  the cone, II=2, 8 sites, 24 DSP — most of the complexity for half the
  saving. The legal values are 3 and 1, and elaboration `$fatal`s on anything
  else (fired deliberately; transcript in §6).

## 2. The Fmax question — the crux, and why the objection dissolves

`DSP-BUDGET-CENSUS-20260908.md:437` dismissed this lever as adding "control
depth to a cone that misses 100 MHz by 39%". Two things have moved since that
sentence:

1. **The 39% is stale.** It cites the 61.09 MHz D22 fit. The stage-5b cut
   landed and re-measured: **73.62 MHz** (`PROJECT-CORE-CLOCK-20260907.md`,
   RESULT section), and the worst path is now the ROW stage itself:
   `mat register -> view mux (row_x~0, ~3.3 ns of IC into the mux leg) ->
   Mult0 (3.938 ns, DSP output register unused) -> 68-bit row adder -> s1`.
2. **"Control depth" assumes the control lands in the datapath.** It does in
   the obvious implementation — a 3:1 phase mux on the matrix operand in
   front of the DSP, the `zhao_terrain_lod` shape. That implementation was
   NOT built, for exactly the census's reason.

**The shape built instead buys its routing with registers.** On the accept
edge the three rows' matrix words are captured into a shifting hold bank
(`ha/hb/hc`, 12 words), view-muxed ONCE on the capture path — a
mat-register-to-hold-register hop with a whole cycle and no arithmetic behind
it. Each cycle the bank shifts the next row under the multipliers. The
multiplier cone at `ROWS_PER_PASS=1` is:

    hold register -> mul32 -> 68-bit row adder -> s1 register

which is the spatial cone **minus the view mux** — a strict structural subset.
The FSM (2 bits, 4 states) drives register enables only. The per-row adder is
unchanged because the three rows never shared adders: cutting nine products to
three removes silicon, not logic depth. So the structural answer is:

* the critical cone **cannot get longer** and plausibly gets slightly shorter
  (the ~3.3 ns of interconnect into `row_x~0`'s mux leg leaves the cone);
* what this lever does NOT do is close the 73.62 -> 100 gap. That limiter is
  the unregistered row product, the clock report's own named next cut, and it
  is orthogonal: it composes at either parameter setting, and at II=3 its +1
  latency is even cheaper.

**What would measure it — the ONE gate, named in advance and not run**
(owner's standing instruction; also `design/fit_targets.yml`'s new comment at
the `zhao_project_service` entry):

    tools/quartus/run_block_fit.ps1 -Module zhao_project_service `
      -Params ROWS_PER_PASS=1 -RowLabel '@ROWS_PER_PASS=1'

Question it answers, exactly: *does DSP land at 15, and does Fmax hold the
73.62 MHz baseline (i.e. is the hold bank genuinely off the cone)?* A
`quartus_map` alone would price the DSP but says nothing about Fmax — timing
needs the fit. Everything else this change touches — correctness, latency,
II, stall semantics, cfg tearing, arbitration — is settled in Verilator below
and needs no Quartus time.

Honest residue the structural argument cannot reach: placement. 15 DSPs
instead of 33 changes column pressure and routing in ways no argument
predicts; that is precisely what the gate is for.

## 3. The composed frame budget, re-derived

From `design/budgets/workloads.yml`: `computeClocksPerFrame = 1,666,666` (the
conservative floor of 100 MHz / 60). Demand: geometry 120,000 (owner-ruled
population); terrain **through the arena** 256 patches x 1,089 unique lattice
vertices = 278,784 (without the arena: 256 x 6,144 = 1,572,864 corner
projections). Sum with arena: **398,784**.

| configuration | projections x II | clocks | of frame | headroom |
|---|---|---:|---:|---:|
| shared service, RPP=3 | 398,784 x 1 | 398,784 | **23.9%** | 4.18x |
| shared service, RPP=1 | 398,784 x 3 | 1,196,352 | **71.8%** | 1.39x |
| shared service, RPP=1, + workloads.yml's own 20% reserve | — | 1,435,622 | 86.1% | fits |
| two cores, RPP=1: geometry lane | 120,000 x 3 | 360,000 | 21.6% | 4.63x |
| two cores, RPP=1: terrain lane | 278,784 x 3 | 836,352 | 50.2% | 1.99x |
| any RPP=1 terrain **without** the arena | 1,572,864 x 3 | 4,718,592 | 283% | **does not fit** |

So the brief's arithmetic ("roughly 71.7%") re-derives to **71.8%** and holds.
The ledger's "1 vertex per clock" is re-argued as follows: at the composed
demand the frame needs an *average* of 0.24 projections per clock; II=3
supplies 0.33; the datasheet line was never the binding constraint once the
arena removed terrain's 5.6x re-projection. Two caveats stated rather than
hidden:

* **The arena is a prerequisite for RPP=1 in the terrain lane, full stop** —
  not only for the shared service. Undeduplicated terrain at II=3 is 2.8
  frames of work per frame.
* These are frame-aggregate numbers. They assume projection demand can spread
  across the frame; an intra-frame deadline (project everything before some
  downstream stage may start) would tighten them, and no such deadline is
  currently ruled. `contended_o` on real traffic is the named measurement, and
  the service's header already says so.

## 4. How it composes with the shared projector and width narrowing

All three levers act on the SAME eleven-site population, so their savings
**multiply; they never add**. Summing the three headline numbers
(33 + 21 + 22 = 76 of 66) is arithmetically impossible and quoting any lever's
headline after another has landed double-counts. The lattice, priced per the
calibration:

| configuration | instances | sites each | DSP total | delta from 66 |
|---|---:|---:|---:|---:|
| today (two wrappers) | 2 | 11 | **66** | — |
| shared service, RPP=3 (built 2026-09-09) | 1 | 11 | 33 | −33 |
| two cores, geometry alone at RPP=1 | 2 | 5 + 11 | 48 | −18, **arena-independent** |
| two cores, both RPP=1 | 2 | 5 | 30 | −36 |
| shared service, RPP=1 | 1 | 5 | **15** | **−51** |
| shared service, RPP=1, both operands ≤27 | 1 | 5 | 5 | −61, owner contract call |

Marginal value in the adoption order that the prerequisites force
(arena -> service -> rows -> width): −33, then −18, then −10.

**And a correction to the width lever's framing, both the docket's and the
brief's.** `GEOM.WCACHE.md:81` measured 32x27 = 3 DSP — same as 32x32 — from
which the brief concludes "only ≤18 bits pays, the target is 18 not 27". The
calibration table says otherwise: a **27x27** product (both operands ≤27)
measures **1 DSP** (`calib_mul` width-27 rows), and the cliff to 3 is at 28.
What WCACHE:81 proves is that *one* wide operand forfeits the band — so the
width lever pays only if BOTH the matrix word and the coordinate narrow, which
is exactly why it is the owner's world-size call
(`PROJECT-CORE-OPERAND-WIDTH-20260909.md`). ≤18 buys a second, smaller
dividend: pair-packing (4 ops at ≤18 map to 3 DSP, not 4), worth about one
more DSP per three sites, not the difference between paying and not paying.

## 5. What was built (all in the working tree, uncommitted per instruction)

| file | change |
|---|---|
| `fpga/rtl/common/zhao_project_core.sv` | `ROWS_PER_PASS` parameter (3/1, elaboration-guarded), `in_ready_o` port, stage 1 split into `g_rows_spatial` (verbatim historical shape) / `g_rows_seq` (shift-bank sequencer), `busy_o` covers the sequencer's holding state, COST header updated per dsp.md's standing rule |
| `fpga/rtl/common/zhao_project_service.sv` | `ROWS_PER_PASS` pass-through; grants, round-robin flip and `contended_o` gated on the core's `in_ready_o` (identical behaviour at 3, where it is constant 1) |
| `fpga/rtl/geometry/zhao_geom_project.sv`, `fpga/rtl/terrain/zhao_terrain_project.sv` | connect the new `in_ready_o` pin (constant 1 at their default 3; commented) — external ports unchanged, so `zhao_prod_top.sv` needs no regeneration (verified: the top instantiates the wrappers, never the core; manifest gate re-run clean, 212 modules) |
| `tests/geometry/tb_proj_rowmux.sv` + `proj_rowmux_directed.cpp` | the differential: RPP=1 against RPP=3, byte-for-byte |
| `tests/geometry/tb_proj_service_rowmux.sv` + `proj_service_rowmux_smoke.cpp` | the service's arbiter under II=3 gating (the service previously had NO test at all — pre-existing gap, now narrowed) |
| `tests/CMakeLists.txt` | both tests wired in (`proj_rowmux_directed`, `proj_service_rowmux_smoke`, labels fast;nightly) |
| `design/fit_targets.yml` | the one fit gate named at the `zhao_project_service` entry, with its exact question and command |

**Sequencing law preserved, mechanically:** the rigid pipeline still has ONE
enable (`en_i`) driving every stage including the sequencer; `in_ready_o` is a
state function the caller ANDs, never a second enable. A vertex reads its
matrix exactly once, on its accept edge, at either setting — the hold bank is
what makes a mid-sequence configuration write unable to tear a transform, and
that case is directly tested.

## 6. Evidence ledger (every command run, with its answer)

```
# the differential: RPP=1 == RPP=3, byte-for-byte
verilator_bin -cc --exe --build -Wall --top-module tb_proj_rowmux ...   # 0 warnings
./Vtb_proj_rowmux.exe
  -> 38 checks passed
  -> streams byte-identical under 4 stall patterns (each instance under its OWN pattern)
  -> measured: L(RPP=3)=37 en-cycles, L(RPP=1)=40 = L3+3 declared, II(RPP=1)=3 exact
  -> cfg write landed mid-sequence (SeqMx): streams still identical, and the
     write verifiably changed later records (the section proves itself non-inert)
  -> POSITIVE CONTROL: +1 raw on DUT view-0 m[5] -> 5 of 64 records differ; the
     comparator fired
  -> busy_o high across the sequencer's holding window, low after drain

# oracle regression of the DEFAULT (the transitivity anchor)
geom_project_directed          -> 900 checks passed
geom_project --random 100      -> 8,218 checks passed
terrain_project_directed       -> 2,313 checks passed; 128 triangles in 423 cycles
HEAD-baseline rebuild of the same terrain suite (git show HEAD: both files)
                               -> 423 cycles, 2,313 checks: RPP=3 is CYCLE-identical to HEAD

# the service arbiter under II=3
./Vtb_proj_service_rowmux.exe  -> 413 checks passed
  -> routing/order/payload intact both clients; grants split 60/60; aggregate
     acceptance span exactly 3*(N-1); solo client accepts every 3rd cycle
  -> contended_o SEEN TO MOVE: 119 under dual saturation (= 2N-1: the final
     grant's rival had drained — the first run of the check expected 120 and
     the DESIGN's 119 was correct), and SEEN TO STAY ZERO under solo load

# the elaboration guard, fired deliberately (--lint-only does not run initial)
verilator_bin --binary -GROWS_PER_PASS=2 zhao_project_core.sv && run
  -> [0] %Fatal: zhao_project_core.sv:387: ... ROWS_PER_PASS (2) must be 3 or 1

# gates
verilator_bin --lint-only -Wall  core | service | geom wrapper | terrain wrapper,
  each also at -GROWS_PER_PASS=1    -> 0 diagnostics, all six
python tools/quartus/check_quartus17_syntax.py  -> clean, 217 files (initial
  begin form and explicit generate/endgenerate used throughout)
python tools/maintenance/no_control_bytes.py <8 files> -> clean
python tools/quartus/check_prod_manifest.py -> OK, every module counted once
```

No new counters were added to production RTL, so no committed mutant was
required: the two detectors this change relies on (the elaboration guard and
the differential comparator) are both reachable by legal stimulus, and both
were fired — the transcript above is the record.

## 7. The latency declaration, and a stale number found on the way

**Declared:** at `ROWS_PER_PASS=1` the core's fixed latency is **L3 + 3
en-cycles** (measured 40 against 37) at initiation interval 3; the test
asserts both as exact per-vertex invariants, not averages. Variable latency
does not occur and cannot: the FSM has no data-dependent path.

**Found stale:** `GEOM.PROJECT.md:97` still declares "fixed 36" and
`TERRAIN.PROJECT.md:175` "fixed 38" — but the 2026-09-07 stage-5b cut added a
cycle (its own report says so: "only the drain grows by one") and the measured
core is 37 accept-to-result. Both contracts predate the cut and were never
updated; this is the fixed-never-remeasured shape, one level down, in prose.
Not edited here — a contract latency line is owner-visible and belongs with
the wrapper lane's next pass — but whoever adopts `ROWS_PER_PASS=1` must write
the new numbers into whichever contract governs the adopting configuration
(shared service: 37+3 from the ACCEPTED cycle, which `*_ready_o` defines).

## 8. Implementation order (for the adoption pass; nothing here blocks on a fit)

1. **Review and commit this working tree** (owner step; nothing is committed).
   The default ships identical silicon and identical cycles, so this commit is
   risk-bounded to the review itself.
2. **Land the arena in the terrain path** (prerequisite; `zhao_proj_arena3`
   exists and the service header sequences it first). Nothing about this lever
   accelerates or waits on that work.
3. **Geometry-lane early slice (optional, arena-independent):** if DSP
   pressure bites before the arena lands, `zhao_geom_project` can take
   `ROWS_PER_PASS=1` alone: −18 DSP at 21.6% of frame in that lane. Costs a
   wrapper edit (wire `core_in_ready` into `advance`/`v_ready_o`) plus its
   contract's rate line — a real change to a ruled target, so it is an owner
   call, priced here and not made.
4. **Adopt the shared service at `ROWS_PER_PASS=1`** once the arena carries
   terrain (−51 from today's 66). Update the adopting contract's latency and
   rate lines (§7).
5. **Spend the ONE named fit gate** (§2) at the next subsystem boundary fit,
   batched per the fitting law — it answers DSP=15? and Fmax>=73.62? in a
   single labelled row.
6. **Separately and unhurried:** the row-product registration (the clock
   report's named next cut, toward 100 MHz) and the −3 `fx_mad` pairing (§1)
   both compose with everything above and each deserves its own measured row.

## 9. Corrections to the brief and the surrounding documents

* **dsp.md lever 1's "~33 to ~12, saving ~21"** → 33 to **15**, saving 18 per
  instance (§1). The direction and the verdict survive; the magnitude was an
  artifact of pre-calibration arithmetic.
* **The census's Fmax objection** (":437 — control depth on a cone that misses
  by 39%") is doubly stale: the cone now misses by 26% (73.62), and the
  objection presumes an operand-mux implementation this change deliberately
  avoids (§2). It was the right objection to raise and is answered, not
  waved off — the residual (placement) is exactly what the named gate measures.
* **The brief's "only ≤18 bits pays"** for the width lever overstates
  WCACHE:81: both-operands-≤27 measures 1 DSP in the calibration table; what
  ≤18 additionally buys is pair-packing worth ~1 DSP per 3 sites (§4).
* **The brief's "71.7%"** re-derives to 71.8% (1,196,352 / 1,666,666); same
  conclusion.
* **Both projection contracts' latency lines are one cycle stale** from the
  stage-5b cut, independent of this change (§7).

## 10. Not verified, by item, with the instrument that would verify each

| claim | status | instrument |
|---|---|---|
| DSP = 15 at RPP=1 | structural (sites x calibration) | the named `@ROWS_PER_PASS=1` labelled fit — a MapOnly would also price DSP if the gate must be split |
| Fmax >= 73.62 at RPP=1 | structural argument (§2): cone is a subset | the same fit; nothing cheaper measures timing |
| the hold bank infers as registers, not RAM | unverified assumption | the fit's map report (`blockMemoryBits` unchanged) |
| 71.8%-of-frame composure under REAL traffic (bursts, intra-frame deadlines) | derived from frame aggregates only | `contended_o` + grant counters on composed workloads; a deadline ruling from the owner if one exists |
| the service routes correctly at RPP=3 (pre-existing) | still untested beyond lint — the smoke covers RPP=1 only | one more smoke instantiation at 3; cheap, out of this change's scope |
| RPP=1 behaviour inside the two production WRAPPERS | not applicable yet — both pin the default; the wrappers ignore `in_ready_o` by construction at 3 | the wrapper edit in §8 step 3/4 brings its own directed sections |
| Quartus synthesizability of the new generate/initial forms | gate-checked (`check_quartus17_syntax`), not map-proven | the named fit's `quartus_map` stage fails fast (~33 s) if the checker missed a form |

The default configuration is NOT on this list: at `ROWS_PER_PASS=3` the block
is proven cycle-identical to HEAD by rebuilding HEAD's own files against the
same suite (423 cycles, 2,313 checks, both).
