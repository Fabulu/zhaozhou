# Terrain rearchitecture: spend memory, stop spending ALM and DSP

2026-09-09, rescue phase. Architect's report against
`reports/terrain-recon/ARCHITECT-BRIEF-terrain-rearchitecture.md`, the three
committed recons, and the owner's queued rearchitecture
`reports/TERRAIN_31MHZ_REARCHITECTURE.txt` (read this session from the
`origin/main` copy; it outranks the brief wherever they differ, and its §16
Step 0 — "finish the texture island first" — is superseded by the 2026-09-09
rescue phase, per direction received mid-task).

Every number below is labelled **MEASURED** (an instrument ran this session, or
a receipt read through the census's own selector), **STRUCTURAL PREDICTION**
(follows from the RTL's shape; a named fit verifies it), or **UNKNOWN** (no
instrument has answered; the instrument is named). Nothing was committed;
everything sits in the working tree for review. No Quartus fit was run.

---

## 0. What is in the working tree

| file | status | what |
|---|---|---|
| `fpga/rtl/terrain/zhao_terrain_bake_v2.sv` | **NEW** | the rebuilt bake: ONE shared multiplier, meets plane in RAM |
| `fpga/rtl/terrain/zhao_terrain_residency_v2.sv` | **MODIFIED** | statram split 57 → 40+17 at the M10K port width (missing-bits repair) |
| `tests/terrain/bake_dev.hpp` | MODIFIED | driver templated over the DUT type (v1 and v2 are port-identical) |
| `tests/terrain/terrain_bake_directed.cpp` | MODIFIED | DUT-generic behind macros; built bare it is byte-for-byte the v1 suite |
| `tests/mutants/zhao_terrain_bake_v2_mutant.sv` | **NEW** | row-window off-by-one mutant, inverted polarity, seen to fire (§2.4) |
| `reports/TERRAIN-REARCHITECTURE-20260909.md` | NEW | this report |

`build/standalone/*` holds this session's throwaway Verilator builds.

CMake registration block for the v2 suite, ready to paste after the v1 block at
`tests/CMakeLists.txt:6576` (not applied, to keep the build graph stable for
other lanes):

```cmake
add_executable(test_terrain_bake_v2_directed terrain/terrain_bake_directed.cpp)
target_link_libraries(test_terrain_bake_v2_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_terrain_bake_v2_directed PRIVATE
  ZHAO_BAKE_DUT_HEADER="Vzhao_terrain_bake_v2.h"
  ZHAO_BAKE_DUT_CLASS=Vzhao_terrain_bake_v2
  ZHAO_BAKE_SUITE_NAME="terrain_bake_v2_directed"
  ZHAO_BAKE_RATE_COVERED_MAX=25.0
  ZHAO_BAKE_RATE_UNCOVERED_MAX=6.0)
target_include_directories(test_terrain_bake_v2_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/terrain)
verilate(test_terrain_bake_v2_directed
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake_v2.sv)
add_test(NAME terrain_bake_v2_directed COMMAND test_terrain_bake_v2_directed)
set_tests_properties(terrain_bake_v2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
```

---

## 1. The thesis applied to terrain, in one table

Terrain's bill today, read through `tools/budget/dsp_census.py` (its own
selector; census banner: *PARTIAL MIXED EVIDENCE — not a floor, not a
ceiling*):

| block | DSP | ALM | reg | mem bits | status |
|---|---:|---:|---:|---:|---|
| `zhao_terrain_project` | 33 | UNKNOWN | | | **spoken for** — projection lane, §4 |
| `zhao_terrain_bake` (+delta) | 17 | est 2,324 (map-only) | 1,928 | **0** | **rebuilt this session, §2** |
| `zhao_terrain_residency_v2` | 0 | 2,234 | 1,226 | 150,528 of 167,936 | **defect repaired, §3** |
| `zhao_terrain_pagestream` | 0 | 1,649 | 2,043 | 0 | deprioritised — brief's premise refuted, §6.2 |
| `zhao_terrain_cmd` | 0 | 1,069 | 869 | 0 | composed nowhere; untouched |
| `zhao_pair_tess_normals` | 9 | 1,574 | 1,577 | 2,048 | owner's own architecture stands, §5 |
| `zhao_terrain_lod` / `normals` | 3 / 3 | | | | already the shared-multiplier model; untouched |
| `zhao_terrain_patch` / `velocity` | — | UNKNOWN | | | unmeasured; named fit only, §7 |

The device: 41,910 ALM (139% committed), 112 DSP (171%), 553 M10K (~27% used).
The rescue shape — small arithmetic factories, RAM for state, scheduling — is
already shipped inside this domain (`zhao_terrain_normals.sv:203`'s mseq,
`zhao_terrain_lod.sv:273`'s one multiplier, `zhao_terrain_tess.sv:317`'s
span_mask). Everything below extends that in-repo pattern rather than inventing
one.

---

## 2. BUILT: `zhao_terrain_bake_v2` — seven multipliers become one, 1,089 flops become one M10K

### 2.1 What moved

V1 (`zhao_terrain_bake.sv` + `zhao_terrain_bake_delta.sv`) holds seven
multiplier sites — `dx*dx`, `dz*dz` (signed 33×33), `radius²` (32×32, the
3-DSP exact table match), two `lat_lerp` call sites (33×7), and bake_delta's
two 32×18 products — and **zero memory bits**, with the 1,089-bit `meets`
plane in flops. Its FSM is strictly sequential (one vertex through a 17-cycle
restoring divide), so the seven sites are busy in different states.

V2 is **port-identical** and makes two structural moves:

* **M1 — one 34×34 signed multiplier**, product registered with an enable,
  operands muxed by state. Every v1 product fits inside it. The per-vertex
  schedule: `span_x*vi` → lerp finish → `dx*dx` → (handshake, divide) →
  `from*s` → `to*s` → emit; `span_z*vj` and `dz*dz` recompute once per ROW,
  `radius²` once per record. The `lat_lerp` arithmetic is v1's function body
  operator-for-operator, split across the product register; the delta
  arithmetic is `zhao_terrain_bake_delta.sv`'s post-multiply chain
  operator-for-operator (only its two products moved into the shared site;
  the uncovered-vertex `delta = 0` equivalence is proven by substitution in
  the RTL comment).
* **M2 — the meets plane in RAM.** This *revisits* v1's chosen B2 rather than
  ignoring it: B2 rejected taking meets bits **from the caller**, which would
  move the §3.4 breach equality outside the block terrain_rules §7 names as
  its only owner. Moving the **storage** to a RAM inside the same block moves
  no law: the equality (`composed18 <= bottom18`) is still computed here, per
  vertex. B2's argument was ownership; this change is substrate.

**M10K arithmetic, shown:** 34 words × 33 bits = 1,122 bits (33 rows + one pad
row for the prefetch clamp). One M10K at the 256×40 aspect holds it 7×.
Dig writes one full row per 33 vertices (bits accumulate in a 32-bit `wrow`;
bit 32 rides the write). Breach reads through a two-row register window with a
one-cycle prefetch of row `cj+2` during each ≥32-cycle row scan. The RAM is
never reset: a breach can only run after a full dig sweep has written every
row of the current record, so pre-sweep contents are unreachable (and a reset
loop would break M10K inference, QUARTUS_GOTCHAS 10).

### 2.2 The price, MEASURED (both DUTs, same suite, same stimulus, this session)

| | v1 | v2 |
|---|---:|---:|
| covered vertex | **19.00** clk | **24.12** clk |
| uncovered vertex | **2.02** clk | **5.14** clk |
| breach cell | **1.00** clk | **1.01** clk |
| suite | 267/267 pass | 267/267 pass |
| coverage stats | 3957 texels, 1572 transitions, 4 clamps, 2010 rails | identical |

### 2.3 Throughput argument against `computeClocksPerFrame = 1,666,666`

**`zhao_terrain_bake` has NO demand row** — it is in `unruled:` in
`design/budgets/workloads.yml:342`. The ledger's "1 bake texel per clock" is
an aspiration v1 never met (measured 19 clk/texel covered). So the honest
argument is built from the §9.2 capability cap, `BAKE_PATCH_BUDGET = 64`:

| corner | v1 | v2 | of frame (v2) |
|---|---:|---:|---:|
| 64 records, **every** vertex covered + breach | 1,389,760 clk (83.4%) | 1,747,000 clk | **104.8%** |
| 64 records, sweep-dominated (small stencils) | ~206,000 clk (12.4%) | ~424,000 clk | 25.4% |

Two things make the worst corner legal rather than alarming, with citations:

1. **v1 already saturates the frame at that corner** (83.4% for one block) —
   the frozen cap was never sized for in-frame completion.
2. **The §9.2 law is built for carry-over**: the budget counts *acceptances*
   (`zhao_terrain_bake.sv` chosen B4), and terrain_rules §9.2 law 2 carries
   the remainder "to the head of the next frame's window, ahead of newly
   issued bakes (FIFO)" — structurally, by backpressure. The block's contract
   is `ready_valid, latency: variable` (`design/blocks.yml`). No II, order or
   fixed-latency contract exists on this block to violate.

If the worst corner ever matters, the architected (not built) v2.1 overlaps
the three per-vertex geometry multiplies under the previous vertex's 17-cycle
divide — restoring ~21 clk covered — at the cost of double-buffered geometry
registers and a visibly different vtx/sc interleaving. Not built because the
simple version's correctness evidence is complete and the corner is capacity,
not demand.

### 2.4 Evidence ledger (all run this session)

* `verilator_bin --lint-only -Wall` on bake_v2: **0 diagnostics**.
* `tools/quartus/check_quartus17_syntax.py`: clean, 217 files, self-test
  3-fire/6-no-fire passing first. `tools/maintenance/no_control_bytes.py`:
  clean on all five new/modified files.
* **v2 oracle suite: 267/267.** The suite is the *existing*
  `terrain_bake_directed.cpp` — the zref-oracle law with constructed
  boundaries (`d2 == r2` closed/open edge, `d2 == 0` at s = 65,536, height16
  rails, `bottom + 1 - base` clamp, fourth-corner breach, inverted-envelope
  truncating divide, §9.2 budget window, the 512 m radius refusal) — made
  DUT-generic behind compile-time macros. Built bare it is bit-for-bit the v1
  test; **the v1 build was rerun this session: 267/267**, so the refactor is
  behaviour-preserving in both directions.
* **Every counter fires**: the suite itself asserts breach events > 0,
  clamps > 0, rails > 0, exercises the budget window closing/reopening and
  the radius reject. No new counters were added.
* **The checker was seen to fail, twice.** Once by accident with diagnostic
  value: the first v2 build captured `radius²` in a state that recurs per row
  (`StVzM`), clobbering `c_r2` with the previous row's last product — 25
  checks failed, exactly the stencil/coverage family. Repaired with a
  dedicated once-per-record state (`StR2C`); the bug and its fix are recorded
  in the state's comment. And once deliberately:
* **The committed mutant** `tests/mutants/zhao_terrain_bake_v2_mutant.sv`
  (renamed module, one substantive line: breach prefetch `cj+2` → `cj+1`)
  drives the same suite with inverted polarity. Run this session: **fails
  exactly and only in the breach-law family** — layer-D compares, transition
  order, the fourth-corner birth, the protected halo — while every layer-B
  check and every counter still balances. That is the demonstration that the
  new row-window machinery has a live detector: the fault class it could
  introduce is observable, and observable *only* through the oracle
  differential, which is why the mutant is committed evidence.

### 2.5 The saving, separated

| quantity | value | class |
|---|---|---|
| multiplier sites 7 → 1 | counted in source | MEASURED (site count) |
| DSP 17 → cost of one 34×34 | **saving 11–14 predicted** (the 32×32 site measured 3 DSP; a 34×34 decomposition lands 3–6) | STRUCTURAL PREDICTION — the named fit answers the exact count |
| registers ≈ −610 (−1,089 meets_row, +~480 sequencer/product/window) | | STRUCTURAL PREDICTION |
| ALM | direction down (six multiplier fabrics and 1,089 packed flops out; operand muxes in) | UNKNOWN — v1 has only a map-only estimate (2,324), so even the baseline is soft |
| M10K +1 (1,122 bits shown in §2.1) | | STRUCTURAL PREDICTION (inference verified at the fit) |

---

## 3. BUILT: the residency statram split — the missing-17-bits defect gets a structural repair

`reports/RESIDENCY-V2-MISSING-BITS-20260907.md`: Quartus inferred `WIDTH_A=40`
for the 57-bit `statram` **without any warning**, and the missing 17
bits/entry — `pin[6], bd, f, mips, crc[31:24]` — are in no RAM, no MLAB and no
flop of the fitted netlist. Eight bits of the page CRC, silently absent, on
the block whose job is validating pages.

The mechanism was never diagnosed (the tool emitted nothing to diagnose
from), so the repair **removes the shape the silent split happened on**: the
stat bank is now two arrays per way, `statram_lo[39:0]` (exactly the slice
Quartus already inferred correctly) and `statram_hi[16:0]` (its own named
inference target), same enable, same address, reassembled by concatenation —
bit-identical by construction. If Quartus ever drops the high slice again, it
is now a *named*, *visible* hole in the RAM Summary instead of a width nobody
reads.

* MEASURED this session: `terrain_residency_v2_directed` 37/37 and
  `terrain_residency_v2_random` 6/6 pass on the modified RTL; lint 0; the
  Quartus-17 gate clean.
* STRUCTURAL PREDICTION: +17,408 memory bits land (167,936 total, finally
  matching `min_memory_bits` in `design/fit_targets.yml`); statram goes from
  4 to ~8 M10K (one `_hi` block per way — ways cannot share ports).
* UNKNOWN until the named fit: the inference itself. **A Verilator run cannot
  verify RAM inference** — the gate is the next terrain fit's map report, and
  the question is stated in advance (§7).

**Architected, not built — the −6.597 ns failing path.**
`reports/synthesis/worst_path_index.json` names it: `s0_set[1]` →
`g_bank[3].keyram write-port registers`, 16.5 ns of data delay through the
event-arbitration/victim-selection cone into the RAM write port. The repair
shape: a registered write stage between event resolution and the RAM write
(one cycle of added mutation latency), with `hazard_c` widened to also compare
against the pending-write register — the block's own read-during-write guard
comment at `:270` already carries the argument this extends. Not built,
because the repair changes hazard semantics on a block whose timing only a fit
can re-measure, and the fit-boundary law says batch it; the Verilator suites
that would hold the widened hazard are the two that ran today.

---

## 4. ARCHITECTED: the terrain projection lane — a shell on `zhao_vertex_arena`, and `w` carried at last

**Correction absorbed mid-task:** the brief said to build on
`zhao_proj_arena3.sv`; that module is a second copy of a mechanism the
2026-08-24 owner ruling says must exist once
(`fpga/rtl/geometry/zhao_vertex_arena.sv`, 58 formal assertions,
`tests/formal/geom_wcache_arena_bounds.sby`). See
`reports/ARENA-I-BUILT-A-SECOND-ONE-20260909.md`. The terrain design below
therefore names the primitive, not the copy.

* **Shape:** a TERRAIN SHELL instantiating `zhao_vertex_arena` with terrain's
  own parameters — exactly what the ruling anticipated ("Terrain may later
  instantiate the same primitive with its own depth/payload"). Suggested
  parameters, all owner-editable at the instantiation: `ARENAS = 4` (2 views ×
  2 working generations), `DEPTH = 81` (9×9 subpatch), `PAYLOAD_W = 106`.
* **The payload carries `w`.** `zhao_terrain_project` exposes **no `w` port at
  all** (its `out_*d_o` are Q16.16 1/w) — the same staleness
  `reports/WCACHE-DROPS-W-20260909.md` establishes for the 75-bit wcache
  payload, with the same resolution: GEOM.DEPTHQUANT's correction of
  2026-09-03 ("THE INPUT IS `w`, NOT `1/w`") makes the 106-bit record
  {x 21, y 21, d 32, w 31, behind 1} the correct one. Any terrain replay path
  must carry `w` forward at the projector's width.
* **The 31-vs-40-bit `w` reconciliation, resolved with citations:** the core
  emits `out_w_o[30:0]` (`zhao_project_core.sv:235`, guarded clip w, fx16 raw,
  zeroed behind the eye at `:701`); DEPTHQUANT accepts `v_w_i[39:0]` but its
  own port comment (`zhao_geom_depthquant.sv:61-62`) says *"w in fx16 raw
  (S15.16). Wide because wmax for WORLD_LONG is 1,073,741,824 — 16384 m in
  fx16 — which needs 31 bits"*, and its first act is a clamp to
  `[WMIN, WMAX]` per profile (`:133-141`) where the largest WMAX is 2^30
  (`spec/qformats.md` depth-profile table: WORLD_LONG wmax = 16,384 m).
  **Same quantity, same format; the 40-bit port is input headroom the clamp
  immediately bounds; a zero-extend of the 31-bit guarded w is lossless.**
  Storing 31 bits in the arena payload is correct and sufficient.
* **DENSE_SEAL, proposed as a parameterised mode, not a fork.** The one
  genuinely new idea in the superseded copy: dense fill (the write address IS
  the fill counter) plus seal-refused-unless-count==DEPTH makes "was this row
  written this lifetime" a property of the GROUP, deleting the flop valid
  bitmap the primitive's own header prices at ARENAS×DEPTH (2,178 flops for
  the geometry shell's 2×1089; 4×81 = 324 for the terrain shell above).
  Proposal: `VALID_MODE ∈ {BITMAP, DENSE_SEAL}` on `zhao_vertex_arena`, so
  the SymbiYosys proof extends to the new mode rather than being abandoned.
  STRUCTURAL PREDICTION: −324 flops for the terrain shell, −2,178 if the
  geometry shell adopts it; UNKNOWN until the mode exists and the .sby cover
  tasks pass on it.
* The demand arithmetic that motivated the arena (5.64× corner-reference
  redundancy; two-view naive projection failing the frame 2.4×; replay at
  ~79% of the reserve window with three read replicas) was re-derived in
  `reports/PROJ-ARENA3-ARCHITECTURE-20260909.md` §1/§6 and is unaffected by
  which primitive hosts the records. Those numbers are ARITHMETIC on the
  roadmap's workload model — the workload itself is UNKNOWN until traced.

`zhao_terrain_project`'s 33 DSP remain **spoken for** by the shared projection
service + arena lane; nothing here re-opens that.

---

## 5. ARCHITECTED BY THE OWNER, DEFERRED TO: the tess/normals 31 MHz rebuild

The owner's `TERRAIN_31MHZ_REARCHITECTURE.txt` §§7–11 specifies the interior:
tagged M0–M7 registered geomorph pipeline, two reserved triangle-assembly
banks with oldest-complete publication, the 64/16/4/1 registered solidity
hierarchy, registered segment/descriptor setup. **That document is the tess
architecture; I checked it against the RTL and recon 3 rather than authoring a
rival.** Points of contact:

* **Independent corroboration:** recon 3 found the worst path launching from
  the bench wrapper's `lat_mem` write-enable into `vy[]`, and §6.2/§6.3 of the
  owner doc identify the same geomorph cone and the same wrapper hazard,
  from the source side. Two investigations, same conclusion: the arithmetic
  anyone would optimise is not the limiter, and part of the measured path is
  harness. Which part is real: the reply→`m_dab→…→m_prod→…→m_y`→capture cone
  (`zhao_terrain_tess.sv:592-608, 815-850`) exists in the console wherever
  the lattice supplier is synchronous RAM — i.e., always; the raw-stimulus
  write path and hash-sink feedback are wrapper-only.
* **The brief's "31.10 vs 32.42 must be reconciled" is resolved**, not by me
  but by the owner doc §1: 31.10 is the ledger fit of the PRE-`m_p_q`-repair
  revision; 32.42 is the post-repair fit whose `sources.sha256` matches the
  current tree. Two fits of two designs. Consequence honoured: the repair is
  done, bought ~1.3 MHz of ~69 needed, and is not to be reimplemented.
* **Not built this session, deliberately:** §16 orders the rebuild in seven
  steps with fits between (B1/B2 harness-vs-DUT separation before any rewrite
  is credited), and its §15 acceptance matrix requires per-entity attribution
  and multi-seed timing I am forbidden to produce today (no fits). A partial
  tess rewrite without its B1/B2 baseline would be exactly the "one enormous
  patch gets all the credit" §15 warns about. The §13 guardrails (flag ALM
  +600 / FF +1,200 over a matched wrapper; no new multiply operators from
  pipelining) are recorded here as the review thresholds for whoever builds
  Steps 3–6.

---

## 6. Where the brief and recons turned out to be wrong — findings, not complaints

1. **"Recon 4" does not exist.** The brief's Invariants section cites "recon 4,
   with citations"; no fourth recon is committed anywhere (`grep -rl` over
   `reports/`). The invariants themselves check out at their contract
   citations (`TERRAIN.PATCH.md:294` live_top law; `TERRAIN.NORMALMAP.md:79`
   in-order/fixed-latency/II=1; `TERRAIN.WRITEBACK.md` 1,024 journal words),
   so the *content* stands — but the cited evidence file is phantom, and the
   next reader should know the citations were re-verified directly.
2. **Pagestream's "the register count has some other home" is refuted by
   addition.** `buf_q` (1,536) + `cov_q` (96) + `covv_q` (3) = 1,635 of 2,043
   registers = **80%** — the registers live exactly in the staging structure
   recon 2 correctly ruled un-movable (combinational byte-lane read at emit;
   `max_m10k: 0` deliberate). The realistic ceiling for register savings here
   is the ~400 remaining, in a block that is **instantiated nowhere in
   production** (recon 2). Deprioritised; any effort here is spent better
   almost anywhere else in the domain.
3. **The ledger's "1 bake texel per clock" was never true.** v1 measures 19.00
   clocks per covered texel (2.02 uncovered) under its own directed suite.
   The v2 rate gates now encode each DUT's *declared* price instead, and the
   MEASURED line prints either way (§2.2). `blocks.yml`'s line should be
   corrected when the ledger is next touched.
4. **`zhao_terrain_bake` is unruled** (workloads.yml `unruled:` list), so any
   throughput claim for it — including mine — is against a capability cap,
   not a demand. Writing a demand row requires tracing stamp rates from
   SURFACE.STAMP; inventing one here would be worse than the gap.
5. **Prod-top instances are harness, not integration** — bake's instance at
   `zhao_prod_top.sv:3587` is LFSR-driven (recon 1 said so; the same caution
   the coordinator verified for `zhao_geom_wcache` at `:2257`). No "composed"
   claim in this report rests on a prod-top instance.

---

## 7. Implementation order, with the fit gates named in advance

**Gate T1 — one terrain subsystem fit** (batched per the fit-boundary law),
after review lands bake_v2 + the residency split. The questions, stated now:

1. bake_v2's DSP count — is one 34×34 site 3–6 DSP (saving 11–14 of 17)?
2. bake_v2's `meets_ram` — inferred as 1 M10K, not flops?
3. bake_v2's ALM/registers against v1's map row (both from the same map,
   same virtual-pin regime).
4. residency statram — **eight** altsyncram rows (`statram_lo`+`statram_hi`
   per way), `min_memory_bits: 167936` finally passing?
5. residency Fmax — unchanged-failing is the expected answer (the write-stage
   repair is not yet built); the fresh path report attributes the cone for
   step 3 below.

A fit for which none of these questions is open should not run.

Order:

1. **Review + register** the v2 suite (CMake block in §0); run the mutant's
   inverted-polarity check in CI or as a documented manual step.
2. **Wire-in decision for bake_v2**: it is a drop-in (port-identical). The
   swap itself is one instantiation change + `gen_prod_top.py` regeneration
   (mandatory after any port-set change to the top's closure) +
   `check_prod_manifest.py`.
3. **Residency write-stage timing repair** (architected in §3), held to the
   two existing suites plus a widened-hazard directed case, then measured at
   the *next* subsystem fit, not a private one.
4. **Terrain projection shell** on `zhao_vertex_arena` (§4), with the
   `VALID_MODE=DENSE_SEAL` proposal made against the primitive so the formal
   proof extends. Its fit rides the projection subsystem's single gate
   (service + arena + replay) already named in the arena report.
5. **The tess rebuild** belongs to the owner's own §16 order with its B1/B2
   baselines — its fits are its own, already specified, and not part of T1.

---

## 8. Not verified, with the instrument for each

| claim | instrument that would verify it |
|---|---|
| bake_v2 DSP = 3–6, regs ≈ −610, ALM direction | Gate T1 (quartus_map per-entity attribution) |
| `meets_ram` and `statram_hi` actually infer as M10K | Gate T1 map RAM Summary — **Verilator is structurally silent on inference** |
| residency −6.597 ns repaired by a write-stage register | build the stage, then T1's successor fit's setup report |
| the §9.2 cap corner (104.8%) ever occurring in play | a traced stamp-rate workload row for workloads.yml (none exists; §6.4) |
| DENSE_SEAL deletes the bitmap without breaking the arena laws | the extended `geom_wcache_arena_bounds.sby` prove+cover on the new mode |
| terrain replay demand numbers (5.64×, 79% window) | the roadmap's workload trace — the numbers are re-derived arithmetic on an untraced model |
| tess geomorph cone is the post-B2 limiter | the owner doc's B1/B2 fit pair |
| v1's own ALM (2,324) | it is a map-only estimate; T1 fits v1's replacement instead — v1's exact ALM dies unmeasured unless someone fits it for its own sake |

---

## 9. Session evidence commands (reproducible)

```
# lint + gates
verilator_bin --lint-only -Wall fpga/rtl/terrain/zhao_terrain_bake_v2.sv        # 0 diagnostics
verilator_bin --lint-only -Wall fpga/rtl/terrain/zhao_terrain_residency_v2.sv   # 0 diagnostics
python tools/quartus/check_quartus17_syntax.py                                  # clean, 217 files
python tools/maintenance/no_control_bytes.py <the five files>                   # clean

# v2 oracle suite (standalone, space-free path, -std=gnu++17 against this g++ 16.1.0)
verilator_bin -cc --exe --build -j 4 -Wall --Mdir build/standalone/bake_v2_obj \
  --prefix Vzhao_terrain_bake_v2 fpga/rtl/terrain/zhao_terrain_bake_v2.sv \
  tests/terrain/terrain_bake_directed.cpp \
  -CFLAGS "-std=gnu++17 -I tests/harness -I tests/terrain -I reference/include \
           -I runtime/include -DZHAO_BAKE_DUT_HEADER='\"Vzhao_terrain_bake_v2.h\"' \
           -DZHAO_BAKE_DUT_CLASS=Vzhao_terrain_bake_v2 \
           -DZHAO_BAKE_SUITE_NAME='\"terrain_bake_v2_directed\"' \
           -DZHAO_BAKE_RATE_COVERED_MAX=25.0 -DZHAO_BAKE_RATE_UNCOVERED_MAX=6.0" \
  -LDFLAGS "build/reference/libzhao_zref.a build/tests/libzhao_harness.a"
# -> 267 checks passed; MEASURED 5.14 / 24.12 / 1.01

# v1 regression of the genericised suite (bare macros)      -> 267 checks passed; 2.02 / 19.00 / 1.00
# mutant, inverted polarity (--top-module zhao_terrain_bake_v2_mutant,
#   tests/mutants/zhao_terrain_bake_v2_mutant.sv)           -> FAILS, breach-law family only: instrument proven
# residency suites on the split RTL                          -> directed 37/37, random 6/6
```

Build notes worth keeping: this g++ (16.1.0) defaults to a newer `-std` than
its own libstdc++ import stubs resolve — link dies on `basic_string(&&)`
undefined; `-std=gnu++17` (what the repo's CMake pins) fixes it. And a
half-regenerated Verilator obj dir reproduces the stale-binary trap exactly as
CLAUDE.md describes — the tell was a measurement that did not move after a fix
that must have moved it; `rm -rf` the Mdir and rebuild through `--build`.
