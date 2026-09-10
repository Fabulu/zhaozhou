# The terrain shell is built and bit-identical; the service is composed in RTL; nothing is adopted yet, and here is exactly what stands between

2026-09-10, run RUN-20260910-0846-projection-adoption. Branch zixxtrixx-v8-closeout.
**Nothing committed; no Quartus fit run.** Every number below is labelled
MEASURED (an instrument ran this session), STRUCTURAL (follows from the RTL's
shape; the named instrument verifies it), INHERITED (read from a document or
a row this session did not reproduce) or UNKNOWN (with the instrument named).
§9 is the not-verified ledger; §10 is where the brief was wrong.

The one-paragraph answer to "can `zhao_project_service` be adopted in this
packet": **no, and it should not be.** What this packet does is cash the part
of the cheque that a Verilator lane can cash: the terrain shell on the
sanctioned primitive, a level-0 replay walker, the service wired with terrain
as client B **in RTL** (`zhao_proj_subsystem`), and a differential that holds
**8,192 replayed triangles bit-identical to the retained `zhao_terrain_project`**
in three elaborations. What it deliberately does not do is flip the manifest,
because three things the composed top needs do not exist yet (§7) and one fit
has not run (§8). `uncashed_cheques.py` now reports the cheque one level up:
`zhao_project_service` is no longer rootless because `zhao_proj_subsystem`
instantiates it, and `zhao_proj_subsystem` is PENDING. That is the truthful
state, and the manifest note says so in words the tool reads as deferral.

---

## 0. What is in the working tree

| file | status | what |
|---|---|---|
| `fpga/rtl/terrain/zhao_terrain_wcache.sv` | NEW | the terrain shell: 3 copies of `zhao_vertex_arena` (4x81x106, DENSE_SEAL), broadcast open/fill/seal, three corner lookups per clock, credit-gated 2-deep output skid, corner + triangle counters, refused/missed corners zeroed and flagged |
| `fpga/rtl/terrain/zhao_terrain_topo.sv` | NEW | the level-0 unstitched walker: 128 corner-reference triples per sealed group, one per clock, sec 4.3 order, underside b/c swap; three adders, no ROM; hold/done |
| `fpga/rtl/common/zhao_proj_subsystem.sv` | NEW | service + shell + walker composed in RTL; client B's result port drives the fill, the {arena,index} rider is the fill address; the fit gate's top |
| `tests/terrain/tb_terrain_wcache.sv` | NEW | the subsystem beside the retained `zhao_terrain_project` (own cfg bus) |
| `tests/terrain/terrain_wcache_differential.cpp` | NEW | the differential, §5 |
| `tests/terrain/terrain_identity_probe.cpp` | NEW | the 81-vertex identity obligation, measured on the zref tessellator, §2 |
| `tests/CMakeLists.txt` | MODIFIED | three elaborations of the differential, the probe, a lint test (another lane also has edits in this file; mine are the TERRAIN.WCACHE block only) |
| `design/prod_manifest.yml` | MODIFIED | `zhao_proj_subsystem`, `zhao_terrain_wcache`, `zhao_terrain_topo`: `not-yet-adopted`, with the adoption condition written beside them |
| `design/fit_targets.yml` | MODIFIED | `zhao_proj_subsystem` (the gate, `max_dsp: 33`) and `zhao_terrain_wcache` (leaf, no rules) |
| `runs/CLAUDE-RUNS/RUN-20260910-0846-projection-adoption/` | NEW | TASK_LOG, SPEC |

Untouched, as instructed: `zhao_proj_arena3.sv`, everything under
`fpga/rtl/texture/`, `zhao_prod_top.sv` (no existing block changed a port;
`gen_prod_top.py --check`: fresh, 66 instances). `zhao_vertex_arena.sv`,
`zhao_geom_wcache.sv`, `zhao_project_service.sv`, `zhao_project_core.sv`,
`zhao_terrain_project.sv`: read, not modified.

## 1. The shell parameters, RE-DERIVED

The brief proposed `ARENAS=4, DEPTH=81, PAYLOAD_W=106, VALID_MODE=DENSE_SEAL`
and asked for every one to be re-derived. Verdict per parameter:

**DEPTH = 81 — holds as an IDENTITY SPACE (measured, §2); does NOT hold as a
per-subpatch vertex COUNT (measured, §2).** A subpatch is 8x8 cells = 9x9 = 81
lattice vertices at level 0 only. At levels 1/2/3 a job uses at most 25/9/4.
Dense seal at a fixed DEPTH forces 81 fills per subpatch at EVERY level. See
§3 for what that costs.

**PAYLOAD_W = 106 — holds, and is now a localparam derived from the field
widths (21+21+32+31+1), guarded at elaboration.** The field map is
`zhao_geom_wcache`'s, bit for bit, so one unpack law serves both shells. `w`
is carried (the brief's item 4): `zhao_geom_wcache` is ALREADY at 106 in the
working tree (the 2026-09-09 dense-seal packet widened it); the brief's
wording reads as if that were still owed. It is done and this shell matches it.

**VALID_MODE = DENSE_SEAL — holds as the default, and the shell is
mode-blind.** The same RTL elaborates at VALID_MODE=0 with no port change,
which is how the miss path was fired (§6). The fill discipline it demands —
in order, complete — is satisfied by construction by a lattice walk; the
differential's producer walks `k = 0..80`, `vi = ox + k%9`, `vj = oz + k/9`.

**ARENAS = 4 — PROPOSED, not derived, and I could not derive it.** "2 views x
2 working generations" is a reasonable pairing (fill one group of a view in
81 clocks while its predecessor replays in 128), and the differential runs
exactly that scheduler and shows the overlap works. But the number that
decides it is how TERRAIN.SEQ sequences views and subpatches, and no
composition of the terrain pipeline exists in `fpga/rtl` (§10.1). It stays an
owner-editable parameter at the instantiation, as the brief asked, with the
honest label: a guess that the differential shows to be sufficient for ITS
scheduler.

**CORNERS = 3 copies (one triangle per clock) — chosen, with the alternative
named in the header.** The primitive has ONE lookup port. One copy replays a
triangle per THREE clocks, which is exactly the legacy projector's rate and
exactly the rate the two-view stress fails (roadmap §8.4: 3,145,728 corner
reads through one port). Three copies with a broadcast fill is the roadmap's
own preferred layout. What I did NOT add is a `REPLICAS` parameter: whether
one triangle per clock is worth 12 extra M10K depends on what GEOM.CLIP can
consume, and nothing in the tree measures that (§7.4). A knob nobody
consumes is a guess baked into silicon.

**Not a parameter, but re-derived: the walker's "table" is three adders.**
With idx(vi,vj) = 9*vj + vi, cell (a,b) has i00 = 9b+a and the pair is
{i00, i00+10, i00+1}, {i00, i00+9, i00+10}. The roadmap's "21 bits x 128
rows -- one M10K or LUTRAM" buys nothing at level 0.

## 2. The 81-vertex identity question, ANSWERED BY MEASUREMENT

The rescue roadmap (§8.5 of `ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt`;
the brief cites it as §3.2) makes 81 an *obligation to prove over the
tessellator's stitch, coarse-parent/morph and surface cases*. Two properties
must hold for a record keyed by (group lifetime, lattice index) to be a
complete identity:

1. every corner of every emitted triangle is a lattice vertex INSIDE the
   subpatch's 9x9 window (no midpoint, no snapped vertex, no vertex borrowed
   from beyond the shared edge);
2. within one job, one lattice index has ONE final world position — the
   geomorph, the annulus fans and the surface select cannot put the same
   (vi, vj) in two places for two triangles of the same job.

By reading `zhao_terrain_tess.sv` both hold: `inner_v`/`outer_v`/`proj_t`
return lattice (vi, vj); `mcase_f` and the morph blend are pure functions of
(vi, vj, job). But the art law says measure on the comparison side, so
`tests/terrain/terrain_identity_probe.cpp` runs `zref::terrain::tessellate`
(the oracle `terrain_tess_directed` proves the TESS RTL reproduces) over the
whole case space — 4 own levels x 4^4 neighbour levels x 6 morph factors
(0, 1, 1/4, 1/2, 0xFFFF, full) x 2 surfaces x 3 lattices (dual, dual with void
cells, legacy page) x 3 origins — inverts every corner's placed coordinate
back to a lattice index and checks both properties.

**MEASURED, 10/10:** 110,592 jobs (16,080 rejected as void+stitch, 24,576
legacy-page undersides empty, both as the law says), 2,509,920 triangles,
7,529,760 corners. **0 off-lattice, 0 outside the window, 0 position
conflicts.** Distinct vertices per job:

| level | triangles | distinct per job (min .. max) | dense fills at DEPTH=81 |
|---|---:|---:|---:|
| 0 | 1,878,480 | 53 .. **81** | 81 |
| 1 | 467,712 | 13 .. **25** | 81 |
| 2 | 129,936 | 5 .. **9** | 81 |
| 3 | 33,792 | 4 .. **4** | 81 |

(The level-0 minimum of 53 is the void lattice: skipped run-cells drop
vertices; a stitched job never uses MORE than its own stride's set because
the ring's outer vertices are coarser, never finer.)

So the roadmap's obligation is discharged: **81 is the complete identity space
for one job lifetime**, across stitch, morph, underside and void. The rider
`{arena, index}` plus the primitive's generation IS the canonical key the
roadmap asks for — the "final morph state / deformation epoch / view / config
epoch" components live in the group lifetime, which is exactly where the
design study said they belong and why no key bits are stored per row.

## 3. What dense fill at 81 COSTS per level — the brief's premise, qualified

The identity space is 81 but the used set is 81/25/9/4. DENSE_SEAL at a fixed
DEPTH projects 81 vertices per subpatch regardless. Against the legacy
corner path (3 projections per triangle):

| level | legacy corner projections | dense fills | dense vs legacy |
|---|---:|---:|---|
| 0 | 384 | 81 | **-79%** (the number the whole campaign is built on) |
| 1 | 96 | 81 | -16% |
| 2 | 24 | 81 | **+3.4x** |
| 3 | 6 | 81 | **+13.5x** |

STRUCTURAL, from the table above. Three consequences, stated rather than
smoothed over:

* Against the roadmap's stress (every subpatch at level 0: 2 x 256 x 16 x 81
  = 663,552 terrain fills two-view) the shell is EXACTLY on budget, because
  that budget already assumed 81 per subpatch. Nothing here breaks the
  arithmetic that justified one core.
* LOD coarsening buys NO projection saving under dense fill; it buys replay
  saving only (fewer triangles). Under a real LOD distribution — far patches
  at level 2/3 — the legacy path does FEWER projections for those patches
  than the dense shell does. The trace, not the example, decides whether that
  matters; no trace exists.
* The remedy, if the trace says it matters, is one of: fill only the stride
  vertices under VALID_MODE=0 (the shell already elaborates that way, at 324
  bitmap flops), or a per-open fill TARGET in the primitive so dense seal
  can close at (n+1)^2 (a primitive change; the sby proof would be extended,
  not abandoned). Neither is built; the parameter is there to choose.

## 4. M10K arithmetic, shown (GEOMETRY, not a receipt)

Per copy: 4 x 81 = 324 rows x 106 bits = 34,344 bits.

| aspect | width slices | depth banks | blocks / copy |
|---|---:|---:|---:|
| 512 x 20 | ceil(106/20) = 6 | 1 | **6** |
| 256 x 40 | ceil(106/40) = 3 | ceil(324/256) = 2 | 6 |
| 1024 x 10 | 11 | 1 | 11 |
| bit floor | 34,344 / 10,240 | | 3.4 -> >= 4, unreachable with 1W+1R at any legal aspect |

Six per copy, **18 for the three copies**. Linear addressing (`arena*81 +
index`, the primitive's 2026-09-09 repair) uses 324 of 512 rows; padding to a
128-row stride would ALSO fit 512 rows at this shape and cost nothing extra —
the design study's observation — but the primitive is linear and correct at
every depth, and 512 rows is the block anyway.

Two things the brief's memory picture did not include, both found by reading
the fit rows rather than the summaries:

* **`zhao_project_core` itself holds 23–29 M10K.** `zhao_terrain_project`'s
  row: 3,822 memory bits in 23 RAM blocks; `zhao_geom_project`'s: 3,287 bits
  in 29 (`reports/synthesis/zhao_block_fit.json`), and the geom target's own
  comment records that every one of them is inside `u_core` — pipeline stage
  storage inferred as memory at ~11% utilisation. So **retiring one core frees
  ~23–29 M10K**, which more than pays for the shell's 18. INHERITED from the
  rows; the gate confirms.
* The geometry shell at 2 x 1089 x 106 = 230,868 bits is 27 M10K at 256x40
  (9 deep x 3 wide; bit floor 22.5). It is NOT on the adoption's critical
  path: the service header's throughput argument needs terrain replay only;
  geometry's 120,000 vertices were affordable without a cache. It is in the
  same fit batch because it changed width, not because adoption needs it.

Projection-domain memory after adoption, STRUCTURAL: today 23 + 29 = 52
(both cores); after, 29 (one core) + 18 (terrain shell) = 47, plus 27 if the
geometry shell is counted = 74 against the roadmap's 48 envelope. The
geometry shell is the item that breaks the envelope, not the terrain one.

## 5. The differential — the acceptance criterion, and the evidence

`tests/terrain/tb_terrain_wcache.sv` instantiates `zhao_proj_subsystem` (one
service: client A geometry noise, client B terrain; the shell fed straight off
`b_*_o` with `{arena, index}` in the rider; the walker) beside the retained
`zhao_terrain_project` on its own cfg bus. `terrain_wcache_differential.cpp`
produces the 81 fill vertices with `zref::terrain::detail::vertex_at` (the
function `tessellate` calls per corner) and the oracle triangles with
`tessellate` — same lattice, same job, same morph, differing only in WHEN the
projection happens — and compares in order, triangle for triangle.

**ONE driver, THREE elaborations, all MEASURED this session:**

| elaboration | checks | result |
|---|---:|---|
| ROWS_PER_PASS=3, VALID_MODE=1 (the defaults) | 18,131 | **all passed** |
| ROWS_PER_PASS=1 | 9,871 | **all passed** (the producer honours `b_ready_o`; nothing else changes) |
| VALID_MODE=0 (bitmap under the same shell) | 18,134 | **all passed** |

What each run holds, per the brief's case list:

* **Main run, 64 groups** = 4x4 subpatches x {top, underside} x {view 0,
  view 1}, level-0 geomorph on odd columns, two groups in flight per view
  (ARENAS=4 as 2x2), client A saturating the other arbiter port, 30% random
  stalls on both consumers: **8,192 triangles bit-identical to
  `zhao_terrain_project`** on x, y, d, behind, src_id, view, mat_a, mat_b,
  weight — and equal to `project_vertex` on all of those PLUS the three `w`
  the legacy packet never carried. Shared edges: adjacent subpatches project
  their shared boundary vertices in different groups and the oracle projects
  every triangle independently, so equality on every triangle IS the
  shared-edge check. Underside winding: 32 of the groups; the walker's b/c
  swap against the oracle's. Behind-eye: 1,152 corners (lattice rows at
  w <= 0, including the w == 0 boundary row). Guard-band rails: 36 corners.
* **Stale handles:** gen-1 on a sealed arena, the old gen after a reopen, and
  the right gen on an unsealed arena: 128/128 refused each time, every
  refused triangle carrying ZEROS, `replay_refused_o` = 128 and
  `corner_refusals_o` = 384 exactly.
* **Dense law:** a seal at 80 of 81 refused and sticky (`arena_seal_short_o`
  SEEN TO FIRE), the arena refusing every triangle until completed, then
  exact after completion; a misordered fill dropped and sticky
  (`arena_overflow_o` SEEN TO FIRE) leaving no trace in the replay.
* **Bitmap elaboration:** the same 80-of-81 seal is ACCEPTED and exactly the
  two triangles of cell (7,7) — the only ones referencing index 80 — MISS
  (`replay_missed_o` = 2, `corner_misses_o` = 2, `out_missed_o` set, the
  missed corner zeroed); the other 126 exact. The miss path is SEEN, by
  parameter, without a mutant.
* **Mid-frame reconfiguration:** a group sealed under M0 replays the M0 law
  after the service's matrix moves to M2 (records freeze at fill); re-filled
  under M2 it equals both oracles at M2; and a fill TORN by a matrix write
  after vertex 40 replays each corner equal to `project_vertex` under the
  matrix in force at THAT corner's accept edge (the tear did change corners:
  checked). The arena is faithful to the core; the epoch discipline is the
  producer's — a rule for TERRAIN.SEQ's contract (§7.6).
* **Positive control:** one raw LSB added to one product word of the
  service's view-1 matrix (oracle untouched): **38 of 128** view-1 triangles
  differ. The comparator has been seen to fire on a one-LSB fault.
* **Throughput, MEASURED:** 128 triangles in **131 clocks** with the consumer
  always ready — one triangle per clock plus the fill-in.
* **Client A** (17,942 / 9,691 vertices): every result to its own client, in
  order, payload intact, equal to `project_vertex`; `contended_o` fired
  (3,515 / 3,553 / 3,515), `b_grants_o` = 64 x 81 exactly.

**The harness's own first bug, recorded:** the first run reported 71
failures, every one the same line — `wait_fills(n)` counted fills landing
AFTER the last vertex was presented, and with a 36-cycle core about 60 of 81
land during presentation. Every substantive check had passed. Made cumulative
(accepted-on-B vs landed-in-shell); in the SPEC's Don't Retry.

## 6. Every counter, with how it was seen to move

| counter / flag | fired by | run |
|---|---|---|
| `replay_triangles_o` | every landed triangle (= 8,192 after the main run) | all |
| `replay_refused_o`, `corner_refusals_o` | stale gen / reopened / unsealed | all |
| `replay_missed_o`, `corner_misses_o`, `out_missed_o` | sealed-but-incomplete BITMAP arena (parameter control; dense provably cannot miss — `a_dense_no_miss`, and the primitive's committed mutant covers that path) | bitmap |
| `corner_hits_o` | = 3 x triangles | all |
| `arena_seal_short_o` | seal at 80/81, dense | dense runs |
| `arena_overflow_o` | misordered fill, dense | dense runs |
| `out_refused_o` | stale handles | all |
| `jobs_done_o`, `hold_o`/`done_o` | 64 walks; the scheduler waits on hold before every open | all |
| `contended_o`, `a_grants_o`, `b_grants_o` | dual load | all |
| `mat_refused_o` | NOT fired here: structurally 0 at MATW=32; its control is `proj_matw_directed` + `tests/mutants/zhao_project_core_mutant.sv` (INHERITED) | — |
| `zhao_terrain_topo` elaboration guard | `--binary -GINDEX_W=7`: `$fatal` at time 0, rc 1, the named message (MEASURED; `--lint-only` does not run initial blocks) | — |
| `zhao_terrain_wcache` field-map guard | NOT fired: it guards localparams no parameter can move. It is a tripwire for the next edit, not a detector | — |

## 7. What stands between here and instantiating the service in a composed top

In order. Items 1–3 are RTL nobody has written; 4 is a measurement nobody
has; 5 is the fit; 6–8 are bookkeeping that must move together.

1. **The tessellator's VERTEX MODE.** `zhao_terrain_tess` emits world
   coordinates; it holds (vi, vj) internally and expands them. The shell
   needs, per job, the 81 lattice vertices in index order with the geomorph
   applied (dense fill) — which is `vertex_at` in hardware, i.e. the tess's
   own lattice read + parent reads + blend, walked over the window instead of
   over triangles. The differential's producer is the zref `vertex_at`; the
   hardware one does not exist. Reimplementing the morph law in a separate
   fill block would be the second-copy pattern this repository forbids, so
   this is a TESS change, not a new block.
2. **Corner references for stitched and coarse jobs.** The walker covers the
   level-0 unstitched topology and nothing else (its header says so). The
   annulus and the coarse run-cell walks are job-dependent and already
   enumerated inside the tess (`inner_v`, `outer_v`, `proj_t`, the run-cell
   scan); the tess emitting index TRIPLES instead of coordinates is the
   correct owner. Also: the walker has no cell-state port, so at level 0 on
   a dual page it emits void cells the tess would skip — either a 64-bit
   solidity mask into the walker or the tess as the reference source.
3. **View and arena sequencing.** Who runs a job twice (per view), which
   arena it lands in, when a group is released — TERRAIN.SEQ-level
   composition. No terrain pipeline (tess -> normals -> project) is composed
   anywhere in `fpga/rtl`; `zhao_terrain_project` is instantiated only by the
   generated resource top. ARENAS=4 is sized for the differential's scheduler
   (§1).
4. **The downstream rate.** One triangle per clock is 3x the legacy rate. If
   GEOM.CLIP cannot take it, three copies buy nothing and one copy at 6 M10K
   is the right shell. Unmeasured.
5. **THE ONE FIT — §8.**
6. **The epoch discipline as a contract rule:** no product-word write between
   a group's first accept and its seal; and note that the core samples the
   MATRIX at accept but the VIEWPORT at stage 5b (~34 cycles later), so a
   viewport write's epoch boundary is not one cycle. Pre-existing core
   property, exposed by the reconfiguration case.
7. **DEPTHQUANT wiring for `w`** — the whole reason the record is 106 bits.
   The shell emits `out_*w_o`; nothing consumes it yet.
8. **Bookkeeping that must move as ONE edit:** `zhao_proj_subsystem` to
   `top:`, `zhao_geom_project` + `zhao_terrain_project` to `excluded`
   (`gen_prod_top.py` regenerated), ledger entries + contract files for
   TERRAIN.WCACHE / the walker / the subsystem (not written — convention
   wants them; nothing tooling-side requires them today), and the DSP census
   re-run against the receipt.

None of these is a design question. All of them are work, and items 1–3 are
more than one packet.

## 8. The one fit gate — named, not run

**Target:** `zhao_proj_subsystem` (registered in `design/fit_targets.yml`,
`max_dsp: 33`), batched with `zhao_geom_wcache` at its new 2x1089x106 shape
and, for the M10K question alone, `zhao_terrain_wcache` as a leaf. One gate,
two or three leaves, one Quartus session.

**Its exact question:** *Does the composed subsystem measure 33 DSP and not
66; do the three 324x106 arena copies infer as block RAM (blockMemoryBits > 0
per instance — the contract's pass/fail line; 18 M10K is the geometry, 6 per
copy at 512x20); what ALM does the subsystem cost against the 12,267 the two
spatial projectors measure today; and does the arena read cone (mem_q ->
zero-mux -> skid) hold 100 MHz?*

**What the two headline numbers are until it runs:**

* **-33 DSP: STRUCTURAL.** One `zhao_project_core` instead of two. Eleven
  product sites per core at 3 DSP each on this device's calibration — read
  this session from `tools/budget/calibration.json`: every s28..s33 single
  product maps to 3 DSP, s27 and below to 1 — so 11 x 3 = 33 per core,
  matching both measured rows. The shell, walker and subsystem contain no
  multiplier (the `arena*81` address is a constant shift-add). The rule
  `max_dsp: 33` is what turns this into a measurement.
* **"~6,100 ALM": NOT A NET FIGURE, and the brief presented it as one
  (§10.3).** Retiring `zhao_terrain_project` removes its 6,068 ALM row — but
  that row is from a DIRTY tree (`rtlCleanAtHead: false`, `status: ok`),
  while `zhao_geom_project`'s 6,199 is from a CLEAN tree stamped
  `failed:structure` (a register-budget rule, not a failed measurement).
  CLAUDE.md's warning about reading `status` alone applies to the baseline
  itself. And adoption ADDS the shell (3 copies' control, a 722-flop skid,
  counters), the walker and the service's arbiter. The net is UNKNOWN; no
  arena shape has ever been fitted, so there is no baseline to subtract.
  `zhao_terrain_project`'s framing (sequencer + reassembly) is the only part
  whose removal is a pure gain, and it is small.

Nothing else in this packet needs Quartus: correctness, refusal semantics,
throughput in clocks, mode behaviour and RPP=1 behaviour were answered by
Verilator in seconds each.

## 9. Not verified, with the instrument for each

| claim | instrument |
|---|---|
| the 324x106 copies INFER as block RAM, and are 6 M10K each | the §8 fit's RAM summary; Verilator is structurally silent on inference |
| ALM / DSP / Fmax of shell, walker, subsystem; the NET ALM change of adoption | the same fit; no arena shape has a fit row, so no baseline exists either |
| `quartus_map` ELABORATES the new files (explicit generate/endgenerate, `$fatal` in `initial begin` — both Quartus-17 forms were used; the syntax gate passed on 226 files) | `quartus_map`, ~33 s smoke; "a block that has never been through quartus_map has not been shown synthesizable" |
| the 8-task SymbiYosys suite on `zhao_vertex_arena` still passes | INHERITED from `reports/VERTEX-ARENA-DENSE-SEAL-20260909.md`; not rerun (the primitive was not modified) |
| ~~the CMake registrations configure and build~~ | **VERIFIED this session, moved out of this table:** lane-local `cmake --preset windows-native -B build-projadopt/cmake` configured (258 s, RC 0), the four new targets built (RC 0), and `ctest -R "terrain_wcache_differential\|terrain_identity_probe"` in that tree ran **4/4 passed** with the same check counts as the standalone builds (18,131 / 9,871 / 18,134 / 10). The shared `build/` graph was not touched |
| behaviour with the REAL tessellator as producer | does not exist (§7.1); the producer here is the zref `vertex_at`, which `terrain_tess_directed` proves the TESS RTL reproduces per corner — but a vertex-mode TESS is new RTL |
| ARENAS=4 against the real view sequencing; the downstream triangle rate | no terrain composition, no CLIP rate measurement |
| the dense-fill cost under a REAL LOD distribution (§3) | a workload trace; the roadmap's stress is all-level-0 by construction |
| gate-level equivalence of anything | no LEC tool in the tree |
| the differential under a DIFFERENT lattice seed / camera | one seed, two cameras; the randomised lane (`--nightly` sweep) is not written |

## 10. Where the brief was wrong, or under-specified — the fourteenth-claim hunt

1. **"Wire terrain as a client of `zhao_project_service`" has no live datapath
   to enter.** `zhao_terrain_project` is instantiated by NO RTL except the
   generated `zhao_prod_top` harness; no tess -> normals -> project chain is
   composed. The wiring exists now, in `zhao_proj_subsystem`, but it is a new
   composition, not a re-plumbing.
2. **"A static subpatch topology table driving the read ports"** exists only
   for the level-0 unstitched case, and there it is three adders, not a
   table. Stitched (annulus) and coarse topologies are job-dependent and the
   tessellator already owns them.
3. **"~6,100 ALM" is a gross, not a net** (§8), and both rows it is built
   from carry the flags CLAUDE.md says to read first (clean+failed:structure;
   dirty+ok). `uncashed_cheques.py` CHECK 2 already flags
   `zhao_terrain_project`'s row as "DIRTY TREE; file moved 21.1 d after the
   fit". The 12,267 baseline is real work measured on RTL that has since
   moved.
4. **"81 unique vertices per subpatch"** — the identity space is 81
   (measured), the used set is 81/25/9/4 (measured), and dense fill pays 81
   at every level (§3). The brief's own caution ("valid only if stitch/morph/
   underside cases really fit") pointed at the identity question, which
   passes; the COST question it did not ask is the one that bites.
5. **"Say whether it works at ROWS_PER_PASS=1"** — bit-identical: yes,
   measured. Affordable: only single-view. The 71.8% in the service header
   is `3 x 398,784` — one view, whole-patch (1,089) dedup. Two views at the
   subpatch catalogue are 2 x 256 x 16 x 81 = 663,552 terrain fills alone;
   at II=3 that is 1,990,656 of 1,666,667 clocks before a single geometry
   vertex. RPP=1's -18 DSP is not compatible with the two-view subpatch
   workload the arena was sized for.
6. **"The payload must carry `w`... 75 + 31 = 106"** — already done in the
   working tree for `zhao_geom_wcache` on 2026-09-09; the brief's item reads
   as if it were still owed. This shell matches it; nothing had to be
   widened.
7. Minor: the "roadmap §3.1 / §3.2" citations are §8.3–8.5 in the consolidated
   rescue text; MATW landed 2026-09-10 per its own header, ROWS_PER_PASS on
   09-09 — "both landed today" is off by a day for one of them.
8. **Not wrong, but worth writing down:** the core samples the MATRIX at
   accept and the VIEWPORT at stage 5b. A "configuration epoch" for a
   projected vertex is therefore two edges ~34 cycles apart. Any epoch rule a
   producer contract states must say which words it covers.

## 11. Evidence ledger (all run this session unless marked INHERITED)

* `verilator_bin --lint-only -Wall`: `zhao_terrain_wcache` (+ primitive),
  `zhao_terrain_topo`, `zhao_proj_subsystem` (full closure),
  `tb_terrain_wcache` (full closure incl. the legacy projector): **0
  diagnostics** each.
* `tools/quartus/check_quartus17_syntax.py`: self-test 3-fire/6-no-fire first;
  **clean, 226 files**.
* `tools/maintenance/no_control_bytes.py`: clean on the five new files.
* `tools/quartus/check_prod_manifest.py`: **OK — 221 modules, 66 tops, 78
  inside, 77 excluded**, every module counted once or declared absent.
* `tools/quartus/gen_prod_top.py --check`: **fresh** (no existing port moved;
  the new blocks are excluded, so the resource top does not change).
* `tools/budget/uncashed_cheques.py`: `zhao_project_service` no longer
  rootless; **`zhao_proj_subsystem` PENDING** ("fit target, never measured")
  — the cheque moved up one level and is still uncashed. Stated in the
  manifest in deferral words on purpose.
* `terrain_identity_probe`: **10/10** — 110,592 jobs, 7,529,760 corners, 0/0/0.
* `terrain_wcache_differential`: **18,131 / 9,871 / 18,134 checks passed** at
  RPP=3 / RPP=1 / VALID_MODE=0; zero FAIL lines in all three logs
  (`build-projadopt/run_*.log`). The same four tests, built by the REGISTERED
  CMake targets in a lane-local tree and run under ctest: **4/4 passed**,
  identical counts.
* Walker elaboration guard: `--binary -GINDEX_W=7` **$fatal at time 0, rc 1**.
* Standalone builds: `verilator_bin -cc --exe --build -Wall`, `-std=gnu++17`,
  absolute include paths, space-free lane-local Mdir under `build-projadopt/`
  (rm -rf between builds), linked against the existing
  `build/reference/libzhao_zref.a` + `build/tests/libzhao_harness.a`
  read-only — the shared `build/` graph was not touched.
* `tools/budget/calibration.json` read for the DSP-per-product rows (s32 = 3,
  s27 = 1, s28..s33 = 3, s40/s48 = 4, s64 = 9) rather than assumed.
* Fit rows read from `reports/synthesis/zhao_block_fit.json` with their
  `rtlCleanAtHead` and `status` fields, not from a summary.

## 12. Implementation order, restated

1. **This packet** — owner review; commit is the owner's call.
2. TESS vertex mode (fill producer) and index-triple emission (stitched and
   coarse references) — Verilator; the differential's zref producer becomes
   the oracle for both.
3. TERRAIN.SEQ-level view/arena sequencing; DEPTHQUANT on `out_*w_o`.
4. **The one fit (§8).**
5. Manifest flip as one edit; regenerate `zhao_prod_top.sv`; DSP census
   against the receipt.
