# The tessellator now has a VERTEX MODE and a REFERENCE MODE; both are bit-identical to the oracle over the identity probe's whole case space; mode 0 is unchanged to the cycle; the geomorph cone was not touched; nothing is fitted and nothing is committed

2026-09-10, run RUN-20260910-0934-terrain-tess-vertex-mode. Branch
zixxtrixx-v8-closeout. **Nothing committed; no Quartus fit run.** Every number
is labelled MEASURED (an instrument ran this session), STRUCTURAL (follows
from the RTL's shape; the named instrument verifies it), INHERITED (read, not
reproduced) or UNKNOWN (with the instrument). §8 is the not-verified ledger;
§9 is where the brief was wrong.

The one-paragraph answer: `reports/PROJECTION-ADOPTION-20260910.md` §7 items
1 and 2 are **built, as a TESS change and not a new block**. `zhao_terrain_tess`
takes a per-job `job_mode_i`: 0 is today's block (6,751 + 2,277 checks pass on
UNCHANGED test sources, 456 and 936 cycles unchanged), 1 emits the 81 window
vertices per job with the geomorph applied through the block's own lattice
read, parent reads and blend, and 2 emits index triples for every triangle of
the same walk — stitched, coarse, void-skipped and underside included. A new
differential runs the identity probe's case space (110,592 jobs, 6,162,480
vertices, 2,509,920 triples) and holds vertices == `vertex_at`, triples ==
`tessellate`'s corners as indices, and **triples applied to vertices rebuild
`tessellate`'s triangles bit for bit** — the arena's replay done in software
on the two hardware streams. `zhao_terrain_topo` is now redundant (§6); its
retirement is recommended and not performed. One thing in the brief was
wrong in a way that changed the design (§9.1), and one label in the previous
report was wrong (§9.2).

---

## 0. What is in the working tree

| file | status | what |
|---|---|---|
| `fpga/rtl/terrain/zhao_terrain_tess.sv` | MODIFIED (+384/−46) | `job_mode_i`, `IDX_W` parameter + `initial` guard, `vtx_*` (9 ports, 2-deep credit-gated skid), `ref_*` (7 ports, 1-deep register), 3 saturating counters, job registers `j_vtx/j_ref/j_vshift/j_plain_hi`, `pend_idx/pend_stride`; header: the three modes, chosen laws 6 and 7 |
| `tests/terrain/tess_harness.hpp` | MODIFIED | `Driver::set_mode()`, collects `verts` / `refs`; the same stall schedule gates all three ready inputs |
| `tests/terrain/terrain_tess_modes_directed.cpp` | NEW | the differential, §3 |
| `tests/CMakeLists.txt` | MODIFIED | `terrain_tess_modes_directed` registered (fast; nightly) |
| `fpga/rtl/synth/zhao_pair_tess_normals.sv` | MODIFIED | the characterisation pair pins `job_mode_i = 0`; new streams tied off, NOT folded into the hash (owner doc §6.3: harness changes separately) |
| `tests/terrain/tb_terrain_compose.sv` | MODIFIED | the PAGESTREAM → PATCH → TESS → NORMALS bench: the third instantiation (§9.8), same tie-off |
| `fpga/rtl/prod/zhao_prod_top.sv` | REGENERATED | `gen_prod_top.py`, +54/−17, `--check` fresh |
| `design/contracts/TERRAIN.TESS.md` | MODIFIED | the two ports, laws 6/7, the mode 1/2 measurements, the new test |
| `runs/CLAUDE-RUNS/RUN-20260910-0934-terrain-tess-vertex-mode/` | NEW | TASK_LOG, SPEC |

**Untouched, deliberately:** `zhao_terrain_topo.sv` (§6 recommends retiring it;
the owner decides), `zhao_terrain_wcache.sv`, `zhao_proj_subsystem.sv`,
`tb_terrain_wcache.sv`, `terrain_wcache_differential.cpp` (item 3, the
composition, is where the tess meets the shell — not this packet), the zref
oracle, `terrain_tess_directed.cpp`, `terrain_tess_random.cpp`,
`terrain_tess_normals.cpp` (their sources are byte-identical; they are the
"unchanged" evidence). No stitch topology moved: `inner_v`, `outer_v`,
`proj_t`, the fan walk and the run-cell scan are the same functions; mode 2
reads their (vi, vj) instead of expanding it.

## 1. The design, and why each choice

**A per-job mode input, not an elaboration parameter.** The composed flow
presents the SAME job twice — once to fill the arena, once to walk its
references — so one block must do both; and a block whose mode is tied to a
constant IS the parameterised instance (synthesis propagates the constant), so
the parameter case comes free. Two instances (one per mode) remain possible and
would let a fill overlap the previous job's references; §5 prices when that is
needed.

**Mode 1 reuses the fetch machinery with slot 0 as the only slot.** The
enumerator walks `(ea, eb)` over 0..8 × 0..8 at stride 1 — `j_vshift` = 0
where mode 0 shifts by the level, `j_plain_hi` = 8 where mode 0 stops at
n − 1 — and `f_kind` steps 0 → 1 → 2 (vertex, parent A, parent B) exactly as
for a triangle's slot. The morph case is `mcase_f`, the parent addresses are
`rd_vi/rd_vj`, the blend is `m_y`. Nothing about the morph law was written
twice.

**The vertex output is a two-deep credit-gated skid, not the output register.**
Mode 0 issues a triangle's last read only when the output register is free
NOW, and that is safe because a triangle needs ≥ 3 reads, so the next last
read is ≥ 2 cycles away and the consumer has had its chance. A vertex needs as
few as ONE read: the next vertex's last read can be issued the cycle the
previous one lands, and with one register that is a lost vertex under a
stall — or, gating on "free now", one vertex per TWO clocks. The skid uses
`zhao_terrain_wcache`'s own rule (`issue_ok = cnt + land − pop ≤ 1`) and
holds one vertex per clock with the consumer ready (MEASURED, §4).

**Mode 2 is mode 0's walk with the read side removed.** `want_issue` is
masked, the enumerator advances when the triple register loads, and the three
indices are `(vj − oz)·9 + (vi − ox)` of `tv_i/tv_j` — a constant multiply —
with the underside's b/c swap applied to indices by the same single mux the
world-coordinate path uses. One triangle per clock, no lattice read.

**Chosen law 6 — an off-grid vertex is the plain lattice vertex.** See §9.1;
this is where the brief's criterion could not be implemented as written.

**Chosen law 7 — a rejected job is rejected in every mode; an unstitched
mode-1 job skips the cell-state scan.** The scan feeds void skips (which mode 1
does not do) and the stitched+void reject (which mode 1 must reproduce so the
sequencer learns of it at the first presentation). Unstitched mode-1 jobs
therefore run without it: 65 cycles is 45% of an 81-vertex fill.

**Mode 3 is counted, never silent.** `mode_invalid_o` increments and the job
runs as mode 0 (SEEN TO FIRE, §3).

## 2. The geomorph cone: what this change did to it — NOTHING, by construction

The owner document (§6.2) names `m_dab → m_half → m_hc → m_d → m_prod →
m_step → m_y` — captured straight into the output register through `last_y`
on `pend_last` — as the live arithmetic cone of the 31 MHz pair. **The cone is
byte-identical after this change**, and mode 1 lands `m_y` into the skid
register through the SAME `last_y` wire mode 0 lands it into `o_cy`. Neither
lengthened nor shortened. STRUCTURAL; the instrument is the pair fit (§7).

**Could the vertex walk register it without changing results?** Yes for mode
1 alone: a vertex's landing may be delayed one cycle (register `m_prod`, 52
flops, and the skid credit counts the landing a cycle later), and no result
changes because nothing downstream reads the vertex until it is in the skid.
**It was not done**, for two reasons stated rather than smoothed over: (a) the
tail `rescale16 → fx_add_sat` would then exist twice — once combinational
for mode 0's bypass, once behind the register for mode 1 — or be muxed, and a
mux on the select of the mode-0 tail LENGTHENS the cone the brief forbids
lengthening; (b) it would fix nothing for mode 0, which is the path the 31 MHz
row measures. The owner document's Step 3 (an independent, registered morph
unit with owned assembly banks) is the change that registers the cone for
every mode; a mode-1-only cut would be a half-copy of it. Price if wanted
anyway: 52 flops + 1 delayed valid, and one more skid slot or a stricter credit.

**What DID enter mode 0's cones**, every item (the `git diff` is the proof):

| where | what | why it is not the blend |
|---|---|---|
| `mc[0]` | `if (j_vtx && !on_grid_c) mc[0] = 0` — one AND on a registered bit at `mcase_f`'s output, slot 0 only | address-generation cone (owner §11.2), not arithmetic |
| `iss_last` | `f_slot == last_slot` (registered) for `== 2'd2`; `mc[f_slot]` for `mc[2]` (the mux `rd_vi` already has) | control |
| `cell_skip`, `want_issue`, the etri branch, the `o_*` load enable, exit/idle | AND terms on `j_vtx` / `j_ref` | enables |
| `do_issue` | `last_blocked = j_vtx ? !vtx_room : out_busy` | the issue gate; `vtx_room` depends on `vtx_ready_i` exactly as `out_busy` depends on `tri_ready_i` |
| `cell_hi` | `j_plain_hi` register for `j_n − 1` | removes a subtractor |
| `i0/j0` | shift by `j_vshift` (register) for `j_level` (register) | same structure |
| `last_x/last_z` | `j_vtx ? vx[0] : vx[2]` | D-inputs of `o_cx/o_cz`, not in the blend |

New state: skid 2 × (96 + 7 + 1), triple register 22, `pend_idx/stride` 8, job
bits 8, counters 96 — about 340 flops. Mode-0 cycle identity is MEASURED
(456 / 936 cycles, 6,751 + 2,277 checks on unchanged sources); mode-0 Fmax is
STRUCTURALLY unchanged and UNKNOWN until the pair is fitted (§7).

## 3. The differential — the acceptance criterion, and the evidence

`tests/terrain/terrain_tess_modes_directed.cpp`, standalone build
(`build-tessvtx/build_standalone.sh`: `verilator_bin --cc --exe --build -Wall`,
`-std=gnu++17`, absolute include paths, space-free Mdir, `rm -rf` first),
**33/33 checks, 29.8 s.** The case space is the identity probe's, verbatim:
3 lattices (dual; dual with four void cells; legacy) × 3 origins ((0,0), (8,16),
(24,24)) × 4 own levels × 4⁴ neighbour levels × 6 morphs (0, 1, 1/4, 1/2,
0xFFFF, 65536) × 2 surfaces = **110,592 jobs**. Backpressure schedules rotate
through the sweep (7 masks, from none to 30-in-32).

| check | MEASURED |
|---|---|
| mode 1: 81 vertices per non-rejected, non-legacy-underside job; index k in order; `vertex_at` on the stride grid, plain elsewhere; `vtx_stride_o` correct; surface, src_id | **6,162,480 vertices, 0 mismatches** |
| mode 2: triples == `tessellate`'s corners inverted to window indices, in emitted order, underside b/c swapped | **2,509,920 triples, 0 mismatches** — the count equals the probe's triangle count to the digit |
| mode 2 ∘ mode 1: triples applied to vertices rebuild `tessellate`'s triangles | **0 mismatches** |
| mode 0 on the probe's lattices, every eighth job | 13,824 jobs, 227,884 triangles, 0 mismatches |
| the reject verdict (stitched + void) agrees in every mode | 0 disagreements over 16,080 rejected jobs |
| `vtx_stride_o` seen at both values | yes |
| `terrain_vertices_emitted_o` == 6,162,480; `terrain_refs_emitted_o` == 2,509,920; `terrain_triangles_emitted_o` == 227,884 (mode 0 only); `subpatch_rejected_o` == presentations (2 × 16,080 + the mode-0 ones) | all equal |
| **positive controls**: the vertex comparator fires on one +1 LSB in ONE vertex (reports exactly 1); the triple comparator on one flipped index (exactly 1); the rebuild comparator on that flip (≥ 1) | all fired |
| `mode_invalid_o` on `job_mode_i = 3`; the job equals mode 0; no other port emits | SEEN TO FIRE |
| `lod_clamped_o` in mode 1 on morph 0x1FFFF; vertices == `vertex_at` at 65536 | SEEN TO FIRE |
| `IDX_W` guard with `-GIDX_W=6` | `%Fatal` at time 0 with the named message. The exe then hung at exit — the `zhao_sim.hpp` VlThreadPool deadlock — and was killed; the message is the evidence, the rc is not obtainable |

A hand check on the headline count, because a total is a claim: 110,592 −
16,080 rejected − 18,432 legacy undersides = 76,080 jobs × 81 = 6,162,480. It
agrees. (And it exposed §9.2.)

**Unchanged suites on unchanged sources, same edited RTL:**
`terrain_tess_directed` **6,751 passed**, printing 456 cycles / 128 triangles
and 936 at morph 0.5 — the same two numbers the contract records;
`terrain_tess_random` **2,277 passed**. `-Wall` lint: 0 diagnostics on the tess
and on the pair wrapper's closure.

**Registered path:** see §10 for the lane-local CMake tree result.

## 4. Throughput, MEASURED (consumer always ready, dual page, origin (8,8))

| mode, job | cycles | for | note |
|---|---:|---|---|
| 1, level 0, morph 0 | **87** | 81 vertices | 0.93 vertices/clock; no scan (law 7) |
| 1, level 0, morph 0.5 | **167** | 81 vertices | 40 morphing × 3 reads + 41 × 1 = 161 reads, one per clock |
| 1, level 1, morph 0 | 87 | 81 vertices | 56 fillers cost one read each |
| 1, level 0, STITCHED | 153 | 81 vertices | the 65-cycle scan ran (law 7 is implemented, not just written) |
| 2, level 0 | **199** | 128 triples | 65 scan + 128 + drain |
| 0, level 0 | 456 | 128 triangles | unchanged |

**The fill-and-reference pair is FASTER than the triangle path even run
serially in one instance:** 87 + 199 = 286 cycles against 456 at morph 0, and
167 + 199 = 366 against 936 at morph 0.5. The reason is structural: mode 0
reads every vertex once per triangle it appears in (384 reads for 81
vertices), mode 1 reads each once. MEASURED per job; the frame total is §5.

## 5. The per-level fill cost, carried forward from the adoption report §3

Mean cycles per mode-1 job over the sweep's unstalled jobs (which include
stitched jobs carrying the scan), MEASURED: level 0 **201.4**, level 1
**144.3**, level 2 **118.1**, level 3 **87.0**. Always 81 vertices, 81–161
lattice reads. The adoption report's table stands: dense fill projects 81 at
every level, where the used set is 81/25/9/4. `vtx_stride_o` is the lever the
report's remedy needs — a VALID_MODE = 0 shell can drop fillers and fill only
the stride set with no primitive change. Not built; the flag exists.

**Two structural observations for item 3 (the composition), stated so the
next packet does not rediscover them:**

* **Mode 1's output is VIEW-INDEPENDENT.** It is world coordinates; only the
  projection is per view. The roadmap's "2 × 256 × 16 × 81 = 663,552 terrain
  fills" are PROJECTIONS; the tessellations are half that. One mode-1 pass
  per job, presented to client B once per view (or projected into two arenas),
  is the right shape. STRUCTURAL.
* **All-level-0, one tess instance, serialised:** 4,096 jobs × (87 + 199) =
  1.17 M clocks of the 1.67 M frame at morph 0 — under budget with 30%
  headroom; at morph 0.5 everywhere, 4,096 × 366 = 1.50 M, 10% headroom. Two
  instances (fill of job N+1 during references of job N) bring it to
  4,096 × 199 = 0.82 M. The scan is 65 of the 199 and is paid once per job by
  mode 2 — sharing it with the mode-1 presentation of the same job (scan once,
  use twice) is a 27% saving on mode 2 that is not built. STRUCTURAL from the
  measured per-job numbers; no composition exists to measure.

## 6. What `zhao_terrain_topo` should become: RETIRED

Mode 2 does everything the walker does — level-0 unstitched, 128 triples, the
b/c swap — and everything it cannot: stitched annuli, coarse run-cells, and
void cells on a dual page (the walker has no cell-state port and emits cells
the tess skips; the adoption report §7.2 named this). It also moves the
inverted winding back to the ONE place the tess header says it lives; the
walker was a second copy of that mux.

What the walker carries that mode 2 does not is not topology: `arena`, `gen`,
`view`, `mat_a/b`, `weight` ride each reference from the job. With the tess as
the source those come from the sequencer as a per-job tag on the `ref_*`
stream — about 30 flops of composition in `zhao_proj_subsystem`, which is item
3. `hold_o` becomes "the tess is in mode 2 for arena X"; `done_o` its last
accepted reference.

**Recommendation: retire it in the item-3 packet, when the subsystem is
rewired to the tess; not here.** Its differential (`terrain_wcache_differential`)
instantiates it today and is the shell's evidence; deleting the walker before
the tess is wired in would leave the shell unproven for the gap. The manifest
row already says `not-yet-adopted ... stitched/coarse topologies must come
from the tessellator` — the deferral was written down, and this packet is the
thing it deferred to. `uncashed_cheques.py` will keep reporting the subsystem
PENDING until the flip, which is correct.

## 7. The one fit gate — named, not run

**Target: `zhao_pair_tess_normals`** (registered, `min_fmax_mhz: 100`), which
is also the owner document's Step 1 refit, so it should be that session.
**Its exact question:** *with `job_mode_i` pinned to 0, does the pair's Fmax
and its worst-200 path census by endpoint (`reports/TERRAIN-TESS-CLOCK-20260907.md`'s
method) change from the row it replaces — in particular, does ANY path in the
census now start at `j_vtx`, `j_ref`, `j_vshift`, `j_plain_hi`, `on_grid_c` or
`vtx_room`, or end at `vo_*`, `vs_*`, `r_*`? None should: the pin strips the
new streams and the mode-0 cone is structurally unchanged.*

Two honest limits of that gate, stated. (1) With the mode pinned and the new
outputs unconsumed, synthesis removes the skid, the triple register and the
counters — so the pair measures the mode-0 IMPACT (the hazard question) and
not the COST of the new logic. The cost question needs the new outputs
observed: the leaf `zhao_terrain_tess` target (virtual pins keep them) in the
same Quartus session, per the batching rule — one gate, one leaf, one session.
(2) The tess has no clean baseline to difference against: `uncashed_cheques.py`
reports its rows as DIRTY (96c0394a) and BEHIND (7395d793, file moved 14.4 d
after the fit). Whatever the leaf measures is a new baseline, not a delta.

Nothing else here needs Quartus. Correctness in every mode, the reject law,
rate in clocks, backpressure, the credit, the scan skip: Verilator, seconds.

## 8. Not verified, with the instrument for each

| claim | instrument |
|---|---|
| mode-0 Fmax unchanged; the new logic's ALM/flop cost | the §7 fit (pair + leaf); Verilator cannot measure timing |
| `quartus_map` elaborates the edited file. The three forms new to THIS file — a parameter-sized cast `IDX_W'(...)`, `$clog2` in a header parameter default, an `initial begin ... $fatal` guard — each have precedent in RTL in `fpga/quartus/prod_fit_sources.txt` that HAS been fitted (`zhao_vertex_arena.sv`, `zhao_project_core.sv`, `zhao_geom_wcache.sv`; grep this session), and the syntax gate passed on 226 files. What has not been shown is this file's use of them | `quartus_map`, ~33 s; "a block that has never been through `quartus_map` has not been shown synthesizable" |
| behaviour against the REAL shell — the tess feeding `zhao_proj_subsystem` client B and the wcache's reference port | does not exist (item 3); the software rebuild check is the strongest available substitute and it holds |
| the two-instance / view-independent budget arithmetic of §5 | a composed TERRAIN.SEQ; the per-job numbers it is built from are measured |
| the skid under a consumer that toggles ready EVERY cycle at 1 vertex/clock steady state (the 7 schedules include 0xAAAAAAAA, which is that) | covered by the sweep — listed because it is the case the credit exists for |
| `terrain_tess_normals` (composition) | the lane-local CMake tree, §10 |
| gate-level equivalence of mode 0 | no LEC tool in the tree; the evidence is 9,028 checks on unchanged sources plus identical cycle counts |
| a different lattice seed / other origins than the probe's three | the `--nightly` random lane is not extended to modes 1/2 (not written) |

## 9. Where the brief was wrong, or under-specified — the sixteenth-claim hunt

1. **"Bit-identical against the zref `vertex_at` producer ... over the same
   case space the identity probe covered" cannot be implemented literally at
   level ≥ 1, and the design changed because of it.** `vertex_at` on a vertex
   OFF the job's stride grid computes a morph toward parents at vi ± s;
   for ox = 0, vi = 1, s = 2 the parent is at −1, outside the lattice (an
   out-of-bounds read in the reference, garbage in hardware). The identity
   probe never hit it because `tessellate` only calls `vertex_at` on corners,
   and corners are on the stride grid (that is the probe's result). The
   differential's own producer never hit it because it runs at level 0 only,
   where every window vertex is on the grid. So: on-grid vertices ARE
   `vertex_at` (the law), off-grid vertices are the plain lattice vertex
   (law 6, flagged), and the criterion is met in that restated form over the
   full space. A test that asserted the literal criterion would have had to
   read outside its own lattice to compute the expectation.
2. **The previous report's "24,576 legacy-page undersides empty" is a wrong
   label on a right number.** Legacy undersides in that space are 1 × 3 × 4 ×
   256 × 6 × 1 = 18,432. The other 6,144 are level-3 jobs on the void lattice
   whose single 8×8 run-cell covers a void cell — origins (0,0) (void at
   (3,3)) and (8,16) (void at (12,20)): 2 × 256 × 6 × 2. Legal, correct,
   and not undersides. The identity probe's counter has the same loose name.
   Found because the vertex total did not factor as (jobs − rejected −
   "undersides") × 81 until the extra 6,144 were accounted for.
3. **"The tess emitting index TRIPLES instead of coordinates."** As a third
   mode, not instead: the triangle path stays for TERRAIN.NORMALS, which
   consumes world coordinates, and for the golden captures.
4. **"If a test pins latency per mode, it will need a matching macro."** No
   test pins latency by macro; the modes test measures cycles and asserts
   bounds, and mode 0's bound is the directed suite's own (3 × 128 + 75).
5. **"`zhao_terrain_tess.sv:592-608`"** for the blend: 593–602 in the
   pre-edit file. Cosmetic.
6. **"This pair measures ~31–32 MHz"** is INHERITED from a row the owner
   document itself says is stale (§2: a normals repair is present and not
   re-fitted). This packet does not move that number and does not claim to.
7. **Not wrong, but a tool hazard found the hard way:** `python
   tools/quartus/gen_prod_top.py --help` REGENERATES `zhao_prod_top.sv` —
   the tool ignores unknown flags and writes by default. On the clean tree it
   was a byte-identical no-op (verified by `git diff`, empty, before
   continuing). The flag that compares is `--check`. Written into the run's
   Don't Retry.
8. **MY OWN, refused by the build:** the task log's "both tess instantiations
   (pair wrapper, prod top)" was a grep over `fpga/ design/ tools/` and not
   `tests/`. `tests/terrain/tb_terrain_compose.sv` is the third, and the
   lane-local configure failed on it with exactly 20 PINMISSING warnings (3
   inputs + 14 stream outputs + 3 counters). The confident one-line summary is
   where the error lives; the fix is the same tie-off, and §10 records the
   configure that FAILED before the one that passed. The wrapper that launched
   it reported "exit code 0" — the pipeline's status, not the configure's —
   and the planted `CONFIGURE_RC=` line is what caught it (CLAUDE.md, "read
   the build's exit code, not the pipeline's").
9. **Item 1 of §7 says the shell needs "the 81 lattice vertices ... with the
   geomorph applied".** At level 0 all 81 can morph; at levels 1/2/3 the
   geomorph applies to at most 9/1/0 interior stride vertices, and 56/72/77
   of the 81 are fillers no triangle references. True as written, and the
   cost it implies is §5's, already in the adoption report §3.

## 10. Evidence ledger (all run this session)

* `verilator_bin --lint-only -Wall`: `zhao_terrain_tess` **0**;
  `zhao_pair_tess_normals` + tess + normals **0**.
* `tools/quartus/check_quartus17_syntax.py`: self-test 3 fire / 6 no-fire;
  **clean, 226 files**.
* `tools/quartus/gen_prod_top.py`: regenerated (66 instances), `--check`
  **fresh**. `tools/quartus/check_prod_manifest.py`: **OK — 221 modules, 66
  tops, 78 inside, 77 excluded.**
* `tools/maintenance/no_control_bytes.py`: clean on the 5 touched files.
* `tools/budget/uncashed_cheques.py`: `zhao_proj_subsystem` still PENDING
  (correct until item 3); tess rows DIRTY / BEHIND (§7).
* Standalone builds (`build-tessvtx/build_standalone.sh`): modes, directed,
  random, guard (`-GIDX_W=6`) — all RC 0, 0 warnings.
* `terrain_tess_modes_directed` **33/33** (`build-tessvtx/run_modes.log`);
  `terrain_tess_directed` **6,751** unchanged source; `terrain_tess_random`
  **2,277** unchanged source; guard **%Fatal** with the named message
  (`run_guard.log`), process killed at the exit deadlock.
* Lane-local CMake tree `build-tessvtx/cmake` (`cmake --preset
  windows-native`, PowerShell with `zhao-env.ps1` sourced): **first configure
  FAILED** (RC 1, the third instantiation, §9.8); after the tie-off, configure
  RC 0, build of the five targets RC 0 (733 steps), **ctest 6/6 passed** —
  `terrain_tess_directed`, `terrain_tess_random`, `lint_terrain_tess`,
  `terrain_tess_modes_directed` (40.9 s), `terrain_tess_normals` (the
  TESS → NORMALS composition, 41,731 checks' source unchanged),
  `compose_rtl_directed` (PAGESTREAM → PATCH → TESS → NORMALS). RCs read from
  planted `CONFIGURE_RC=/BUILD_RC=/CTEST_RC=` lines in the log, not from the
  wrapper, which said 0 both times. The shared `build/` tree was not touched.

## 11. Where item 2 stands, and what item 3 still needs — the honest remaining list

Item 1: built, differential held. Item 2: built, differential held, including
the void-cell case the walker could not see. **Both in this packet.** What
remains for the service to be instantiated in a composed top is unchanged
from the adoption report §7 items 3–8, with two additions this packet made
concrete:

* a per-job **reference tagger** in `zhao_proj_subsystem` (arena, gen, view,
  materials onto the `ref_*` stream) replacing `zhao_terrain_topo`;
* a sequencer that presents each job in mode 1 once (view-independent) and
  mode 2 once, honours `job_reject_o` at the first presentation, and either
  presents the 81 vertices to client B once per view or opens one arena per
  view.

Neither is a design question. Both are work.
