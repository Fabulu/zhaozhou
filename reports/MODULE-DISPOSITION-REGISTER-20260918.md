# Module disposition register -- 2026-09-18

**Question asked.** `fpga/rtl` grew from 12 `.sv` files on 2026-08-15 to 285
today, `zhao_console_core` elaborates only a fraction of them, and the owner's
reading was *"so they're just old stales. Welp, account for it and get it
sorted."* This register gives every module that nothing instantiates exactly one
disposition, with the evidence that decided it.

**Answer, up front: there are no stales.** Every one of the 288 modules under
`fpga/rtl` has a live consumer -- a fit gate, a ctest target, a formal proof, a
committed mutant, a tool, or a parent module. The residue is **zero**. Nothing
was deleted, and the reason is not caution: no deletion candidate survived
contact with the evidence.

Two things were found that *do* need fixing, and both are the CLAUDE.md pattern
of an instrument that cannot fire. They are in section 5.

---

## 1. The premise had two errors in it, and they matter

### 1.1 The "63 tops" are not orphans

`tools/quartus/check_prod_manifest.py` prints

```
prod manifest: 288 modules, 63 tops, 78 inside, 134 excluded, 3 retired census slots
```

`top` in `design/prod_manifest.yml` does **not** mean "nothing instantiates it".
It means "selected census root" -- the list from which
`tools/quartus/gen_prod_top.py` generates `fpga/rtl/prod/zhao_prod_top.sv`. That
file contains exactly 63 instantiation blocks, and walking the graph:

```
manifest tops: 63   excluded: 134
tops NOT reachable from zhao_prod_top: []
tops that are also module-graph roots: []
```

All 63 are instantiated. **Zero of the 63 are orphans.** Reading the manifest's
`top:` section as a list of un-instantiated modules inverts its meaning: it is
the list of things that ARE counted, not the list of things nobody wired up.
The file's own header says so -- *"one hierarchy that instantiates exactly one
chosen implementation of every intended production block"*.

### 1.2 `tools/quartus/module_graph.py` over-reports roots by 8

`module_graph.py` says 75 roots. Its edge scan contains

```python
for mod in names:
    if decl[mod] == p:
        continue
```

-- it skips every module declared in the file it is scanning, so a generated
wrapper that instantiates its own sinks beside itself reports those sinks as
roots. The eight false roots are all of that shape:

`shell_v2_stimulus`, `shell_v2_gpu_sink`, `shell_v2_video_sink`,
`shell_v2_audio_sink` -- all instantiated by `shell_v2_top` at lines
391/733/742/750 of `fpga/rtl/generated/zhao_shell_v2_fit_top.sv` -- and the same
four for `zhao_shell_fit_top` in `fpga/rtl/generated/zhao_shell_fit_top.sv`.

Recomputed with each module's own body isolated, **the true root count is 67**.
That is the population this register dispositions. The false-root effect makes
`module_graph.py` generous about roots, which is the SAFE direction for its own
purpose (not double-counting in the census) and the DANGEROUS direction for a
deletion sweep. It is not a bug to fix there; it is a reason not to feed that
tool's output straight into `rm`.

---

## 2. The population, measured

| quantity | value | how |
| --- | --- | --- |
| `.sv` files under `fpga/rtl` | 285 | file count |
| modules declared | 288 | `module_graph.build()` |
| modules nothing instantiates (true roots) | **67** | intra-file-aware rebuild of the edge set |
| roots reported by `module_graph.py` | 75 | 8 are same-file children (1.2) |
| reachable from `zhao_console_core` | 96 | text-scan closure (112 by elaboration -- packages and generate scopes) |
| reachable from `zhao_prod_top` (census) | 144 | same closure walk |
| in **neither** the machine nor the census | 116 | -- |
| modules with **no consumer anywhere** | **0** | section 4 |

## 3. Dispositions

| disposition | count |
| --- | --- |
| SUPERSEDED-BY | 16 |
| INFRASTRUCTURE | 30 |
| CANDIDATE | 17 |
| pending manifest adoption (built today, not this pass's to classify) | 4 |
| **DEAD** | **0** |

The mapping is not invented here. `design/prod_manifest.yml` already carries a
reason code and a prose disposition for every excluded module, and the reason
codes line up one-for-one with the four asked-for dispositions:

* `superseded` -> SUPERSEDED-BY (the note names the successor in every case)
* `probe`, `harness`, `frozen` -> INFRASTRUCTURE
* `not-yet-adopted`, `unused` -> CANDIDATE

Reason-code census across the 134 excluded rows: `probe` 35, `not-yet-adopted`
34, `superseded` 31, `frozen` 17, `unused` 13, `harness` 4.

**The distinction CLAUDE.md asks for is already visible in the reason code
itself.** A `not-yet-adopted` row names what decides it -- *"adopt in place of
`zhao_forge_cliff` only after fit gate F-CLIFF1"*, *"adopt after fit gate
F-PROGDIR1 prices the 16x80 memory and single comparators"*, *"adoption needs
the owner's ruling on the tie law"*. A bare `unused` row does not: *"the
visible-patch builder; TERRAIN.LOD does not consume it yet"*. Both are
CANDIDATE, but only the first has a **named decider**. Of the 17 CANDIDATE
roots, 8 name a decider (`not-yet-adopted`) and **9 are bare deferrals**
(`unused`) -- those 9 are the open questions this register leaves open on
purpose, and they are the ones worth an owner ruling:

```
zhao_terrain_cmd          zhao_terrain_compcache_front  zhao_terrain_loadq
zhao_terrain_mipfeed      zhao_terrain_normalmap        zhao_terrain_pageloader
zhao_terrain_seq          zhao_terrain_visible          zhao_terrain_writeback
```

All nine are the terrain world layer. They are not stale; they are one
composition step that was never taken -- `zhao_shell_top` composes neither end
of the SEQ -> RESIDENCY -> PAGELOADER -> COMPCACHE chain, and the manifest says
so in each row. Three of them are already fitted (`zhao_terrain_cmd` 1,069 ALM,
`zhao_terrain_loadq` 734 ALM / 1 M10K / 107.33 MHz, `zhao_terrain_mipfeed` 343
ALM). This is a single deferred subsystem wearing nine rows, not nine
independent decisions.

`tools/budget/uncashed_cheques.py` reaches the same split independently, and it
is the standing instrument for this question -- this register does not replace
it:

```
scanned 288 modules, 113 fit targets, 197 manifest entries
CHECK 1: 40 rootless measured/targeted modules, 33 CLOSED by the manifest, 7 STILL OPEN
CHECK 4: zhao_prod_top reaches 144 of 288 modules; 11 measured/targeted module(s)
         outside that hierarchy carry an OPEN disposition
```

---

## 4. Nothing is dead -- the DEAD list is empty

One pass over `design/`, `tests/`, `tools/`, `scripts/`, `reports/` and `fpga/`,
matching all 288 module names at once with a single alternation and tolerating
the Verilator `V` prefix, excluding each module's own file and
`design/prod_manifest.yml` (so that being named in the manifest does not count
as being used):

```
RESIDUE (no parent module, no mention anywhere outside its own file and the manifest):
residue: 0
```

Exactly two modules are mentioned **only** in `reports/`:

| module | file | what mentions it | disposition |
| --- | --- | --- | --- |
| `zhao_probe_compose_pipe` | `synth/zhao_probe_compose_pipe.sv` | `reports/synthesis/zhao_block_fit.json` plus 13 characterization evidence bundles | INFRASTRUCTURE -- spent characterization probe |
| `zhao_raster_attrwalk` | `raster/zhao_raster_attrwalk.sv` | `reports/ATTRIBUTE_INTERPOLATION_LAW.md`, `reports/ATTRSTEP_QR_REARCHITECTURE_20260909.md` and 2 more | CANDIDATE -- blocked on an owner ruling |

`zhao_probe_banked_rf` is the third weakest: outside the manifest it is named
only in `reports/` and in the prose headers of three RTL files
(`zhao_field_v2_core.sv`, `zhao_field_v3_rf.sv`, `zhao_probe_v3_exec.sv`), never
in an instantiation.

**None of the three is deleted, and the reasons are law here:**

* `zhao_probe_banked_rf` and `zhao_probe_compose_pipe` are 2026-08-25
  characterization probes. Both headers open with *"CHARACTERIZATION PROBE, not
  a console block"*, and `banked_rf`'s says plainly what it is for: the owner
  directive estimated ~12 M10Ks and added *"Don't trust my count until Quartus
  proves it."* Their measurements are quoted in `reports/FIELD_V3_PROBES.md`,
  `reports/FIELD_V2_MODEL.md`, `reports/FIELD_RESOURCE_MODEL.md` and the fit
  ledger. CLAUDE.md, Ground contact: *"a probe that does this was written once
  and thrown away, so its numbers are unreproducible -- commit the probe."*
  Deleting a committed probe whose numbers are cited makes those numbers
  unreproducible. They stay, and they are INFRASTRUCTURE: the gate that consumes
  them is the fit ledger row that quotes them.
* `zhao_raster_attrwalk` is `not-yet-adopted` with a decider named in the
  manifest -- the owner's ruling on the tie law, where *"the repo's stated law
  and zref disagree on negative exact halves"*, spelled out at length in the
  module's own header. A pending decision is precisely not DEAD.

### 4.1 And a structural reason no deletion was possible in this pass anyway

`tools/quartus/check_prod_manifest.py` enforces exact accounting in **both**
directions:

```python
for e in excluded:
    if e not in decl:
        errors.append("excluded '%s' is not a module under fpga/rtl" % e)
```

Every one of the 67 orphans is either a manifest `excluded` row or one of the 13
blocks built today and still `UNACCOUNTED`. Deleting **any** of them therefore
requires removing its row from `design/prod_manifest.yml` in the same commit --
and that file is off limits this pass, because the integration owner is adding
13 rows to it. So even had a module been DEAD, the deletion would have had to
wait for the manifest edit. The register is the deliverable; a deletion, if one
is ever justified, is one manifest edit away and belongs to whoever owns that
file.

---

## 5. Two things that ARE wrong, both of the same shape

Neither is a stale file. Both are instruments that look like enforcement and are
not -- the pattern CLAUDE.md names as *"a gate that cannot reach the state is
not evidence about the state"*.

### 5.1 Eight committed test `.cpp` files that no build target compiles

`tests/CMakeLists.txt` has **452 `add_executable` calls and zero `file(GLOB)`**,
so a test is built only if it is named. These eight are not named by any
`CMakeLists.txt` or `.cmake` in the tree:

| file | referenced by | consequence |
| --- | --- | --- |
| `tests/proofs/attrwalk_rtl_differential.cpp` | nothing | `fpga/rtl/raster/zhao_raster_attrwalk.sv` line 4 declares `ENFORCED-BY: tests/proofs/attrwalk_rtl_differential.cpp:main`. **The named enforcer never runs.** |
| `tests/texture/terrain_normalmap_directed.cpp` | `design/blocks.yml` | TERRAIN.NORMALMAP's ledger enforcement points at a file nothing builds. |
| `tests/geometry/proj_arena3_directed.cpp` | nothing (`tests/CMakeLists.txt` names `zhao_proj_arena3` only inside a comment) | the retained arena design study has no running differential. |
| `tests/proofs/curve_kstart_equivalence.cpp` | nothing | -- |
| `tests/proofs/attribute_plane_equivalence.cpp` | only `attrstep_qr_differential.cpp`, itself unbuilt | a closed cluster of unbuilt proofs. |
| `tests/proofs/attrstep_qr_differential.cpp` | only `attrwalk_rtl_differential.cpp`, itself unbuilt | ditto. |
| `tests/dsp/dual18_physical_pack_directed.cpp` | `tests/dsp/run_dual18_verilator.py` | driven by a script, not by ctest -- half-live, and invisible to a suite run. |
| `tests/texture/island_v3_fault_directed.cpp` | `tests/CMakeLists.txt:2674`, in a comment: *"retained as source history"* | **accounted.** This one is correct as it stands. |

Seven of the eight are unaccounted, and two of those are cited elsewhere as the
enforcing evidence for a module's correctness. That is a claim standing on a
gate that cannot fire.

`tests/CMakeLists.txt` is being written by another agent this pass, so no target
was added here. **Recommended action:** either register these targets, or mark
them retained-as-history the way line 2674 already does for
`island_v3_fault_directed.cpp` -- and repair the two `ENFORCED-BY` citations
that currently point at nothing.

#### RESOLVED 2026-09-19: all seven were LOST GATES, none was dead code

Every one of the seven compiles against current RTL, runs, and passes. Nothing
here had gone stale -- no renamed module, no changed port, no removed signal.
They were simply never named, and the two RTL directories this register quoted
are worth correcting for the next reader: `zhao_terrain_normalmap.sv` lives
under `fpga/rtl/terrain/`, not `fpga/rtl/texture/`, and `zhao_proj_arena3.sv`
under `fpga/rtl/common/`, not `fpga/rtl/geometry/`.

| test | registered as | result |
| --- | --- | --- |
| `attribute_plane_equivalence.cpp` | `attribute_plane_equivalence` | 32,805 pixel-attributes, 0 mismatches |
| `curve_kstart_equivalence.cpp` | `curve_kstart_equivalence` | 28,032 comparisons, 0 disagreements |
| `proj_arena3_directed.cpp` | `proj_arena3_directed` | ALL CHECKS PASSED, all nine counters moved |
| `terrain_normalmap_directed.cpp` | `terrain_normalmap_directed` + `terrain_normalmap_break_oracle` | **4,738 checks, 0 failures** -- the ledger's number to the digit |
| `attrstep_qr_differential.cpp` | `attrstep_qr_differential` | 4,204 law checks, 11,636 RTL divides, wrong-tie control 8/8 |
| `attrwalk_rtl_differential.cpp` | `attrwalk_rtl_differential` | 3,015 covered pixels both tie builds, range detector 0 -> 4 |
| `dual18_physical_pack_directed.cpp` | `dual18_physical_pack_directed` (runs the script) | 8 models PASS, 4 positive controls FIRED |

No `ENFORCED-BY` needed repointing and none needed downgrading to an assumption.
All four name tests that now genuinely run, and each sits beside a claim the
test actually covers -- checked, not assumed: `zhao_terrain_normalmap.sv:363`
claims II=1 by construction and section 8 of the suite measures 64 fragments in
64 cycles; `zhao_proj_arena3.sv:96` claims one key per GROUP rather than per
row, and every accepted read is compared against the key its group was opened
with. **The repair was to make the enforcers run**, which is the strongest of
V20's three options and was available the whole time.

**And the invisibility was already costing something.** `dual18_physical_pack_
directed.cpp` did not compile. Commit `24bc6b47` ("26 Verilated test mains
could each have hung a suite; a gate now watches") put its
`#include "../harness/zhao_sim.hpp"` inside the *last* `#elif` arm of the
backend selector, so `zhao::exit_hard` was declared for exactly one of the
eight cases `run_dual18_verilator.py` drives; the other seven died with
`'zhao' has not been declared`. That stood for eight days.

The instructive part is why nothing caught it. `tests/lint/verilated_exit_path.py`
passed this file the whole time -- it is a **text scan**, so it read the three
`zhao::exit_hard` calls, called the file safe, and never compiled anything. A
gate that fixed 26 files could not tell that its own fix did not build in seven
of eight configurations, because no target built the file and the script that
did was not a ctest. The fix-up gate and the thing it fixed were both invisible
to the compiler at once. This is the broken-instrument law: the failure was
silent and in the flattering direction.

Registering the runner as a ctest is what converts that from a silence into a
red line. `island_v3_fault_directed.cpp` remains the one correct
retained-as-history case, unchanged.

### 5.2 Twenty-two modules the connected machine instantiates are marked `excluded`

`zhao_console_core` is the machine. Its closure contains 22 modules that
`design/prod_manifest.yml` accounts for as **excluded** -- the census says "not
production" about blocks that are in the machine:

```
zhao_attr_mul72x13_dsp3        zhao_dual18_mul              zhao_fb_ready_cdc_v2
zhao_geom_bin_pipe_v2          zhao_geom_binner_v2          zhao_geom_proj_lane
zhao_mul27_exact               zhao_proj_subsystem          zhao_project_service
zhao_raster_attrdiv_v2         zhao_raster_attrgrad_dsp3    zhao_raster_attrgrad_v2
zhao_raster_texture_stage_v3   zhao_raster_tile_pipe_v2     zhao_renderer_lease_v2
zhao_shell_top_v2              zhao_terrain_wcache          zhao_texture_bilerp_lane_dsp2
zhao_video_blit_lease_v2       zhao_video_ready_bridge_v2   zhao_video_slotmgr_v2
zhao_video_terminal_adapter_v2
```

Several carry `not-yet-adopted` notes whose text is now out of date -- e.g.
`zhao_proj_subsystem`'s reason is that the projector *"is not yet adoptable"*
because its producer was absent, while `design/fit_targets.yml`'s own
`zhao_console_core` entry says that row is *"the composition in which it is
not"*. `zhao_console_core` itself is one of the 13 `UNACCOUNTED` errors the
manifest check currently reports.

This is precisely the accounting the integration owner's 13 new rows exist to
close. It is flagged here and not touched.

---

## 6. Gate results -- before and after

No file in the repository was modified by this pass except the creation of this
register, so all three gates are unchanged by construction. Both failing gates
were **already failing before this pass**, on other agents' in-flight work.

| gate | before | after | delta |
| --- | --- | --- | --- |
| `python tools/quartus/check_prod_manifest.py` | FAIL -- 13 errors, all `UNACCOUNTED`: the lighting, particle, compositor, measure and console-core blocks built today and awaiting manifest rows | FAIL -- 13 errors, identical | **0** |
| `npm run ledger:check` | FAIL -- 1 error against 118 blocks / 40 ops: `V20: fpga/rtl/geometry/zhao_light_stream.sv:795 states an invariant claim with no ENFORCED-BY` | FAIL -- 1 error, identical | **0** |
| `verilator_bin.exe --lint-only -Wall --top-module zhao_console_core` over its 106-file closure from `design/fit_targets.yml` | exit 1, **114 warnings**, 0 errors | exit 1, 114 warnings | **0** |

Neither failing gate is this pass's to fix: the `V20` error is in
`zhao_light_stream.sv`, and the 13 `UNACCOUNTED` errors are the blocks the
manifest owner is about to declare.

No Quartus fit was started and no full `ctest` was run, per the constraint that
another agent may have a `quartus_map` live.

---

## 7. Register

Evidence column names the gates and tests that consume the module; the second
line of each cell is `design/prod_manifest.yml`'s own disposition note, which is
the citation for a SUPERSEDED successor.

### SUPERSEDED -- 16

| module | file | manifest reason | evidence that decided it |
| --- | --- | --- | --- |
| `zhao_proj_arena3` | `common/zhao_proj_arena3.sv` | `superseded` | tests/mutants/zhao_terrain_bake_v2_mutant.sv; tests/mutants/zhao_terrain_group_seq_mutant.sv; tests/mutants/zhao_vertex_arena_dense_mutant.sv; 2 test file(s); 2 tool(s)<br>a SECOND copy of a mechanism the 2026-08-24 ruling says must live once -- zhao_vertex_arena already owns it, with the committed geom_wcache_arena_bounds.sby proof (8 labelled assertions + 6 covers before the consolidation; the "58 formal as... |
| `zhao_raster_blend` | `raster/zhao_raster_blend.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; tests/formal/raster_fragment_blend.sby; 9 test file(s); 3 tool(s)<br>split into blend_fin + blend_prod, which zhao_raster_fragment instantiates |
| `zhao_raster_perspuv_svc` | `raster/zhao_raster_perspuv_svc.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 6 test file(s); 5 tool(s)<br>by zhao_raster_perspuv_pairpipe_v2 inside the Packet B selected V3 root; earlier FIT GATE 3 pairpipe figures are historical only |
| `zhao_raster_rcp24_v3` | `raster/zhao_raster_rcp24_v3.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 6 test file(s); 6 tool(s)<br>retained pre-Packet-B reciprocal-tile oracle; zhao_raster_rcp24_v4 is reachable inside the selected V3 root |
| `zhao_raster_texjoin` | `raster/zhao_raster_texjoin.sv` | `superseded` | tests/CMakeLists.txt; 3 test file(s); 1 tool(s)<br>by the retained zhao_raster_texjoin_v2 behavioural oracle |
| `zhao_raster_texjoin_v2` | `raster/zhao_raster_texjoin_v2.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 8 test file(s); 2 tool(s)<br>redundant accounting root; lifecycle is owned by zhao_texture_v3own inside selected zhao_texture_island_v3_top; retained as the behavioural oracle and leaf-fit specimen; not shell-connected and no physical saving claimed |
| `zhao_terrain_residency` | `terrain/zhao_terrain_residency.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 3 test file(s)<br>by zhao_terrain_residency_v2 (256x4 ways) |
| `zhao_terrain_topo` | `terrain/zhao_terrain_topo.sv` | `superseded` | tests/CMakeLists.txt; tests/mutants/zhao_terrain_group_seq_mutant.sv; 3 test file(s)<br>retained level-0 legacy/test walker; topology-aware ModeRef from zhao_terrain_tess now drives the generic subsystem reference ports, and no production composition instantiates this module |
| `zhao_texture_aux` | `texture/zhao_texture_aux.sv` | `superseded` | tests/CMakeLists.txt; 5 test file(s); 1 tool(s)<br>by zhao_texture_aux_pipe_v2 inside the Packet B selected V3 root |
| `zhao_texture_combine` | `texture/zhao_texture_combine.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 3 test file(s); 3 tool(s)<br>refuted II=1 form; material_combine_v3 is the combiner inside the Packet B selected V3 root |
| `zhao_texture_early_desc` | `texture/zhao_texture_early_desc.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_texture_early_desc_slotswap_mutant.sv; 6 test file(s); 1 tool(s)<br>retained pre-Packet-B early-descriptor oracle; zhao_texture_early_desc_v2 is reachable inside the selected V3 root |
| `zhao_texture_frag_expand` | `texture/zhao_texture_frag_expand.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_texture_frag_expand_mutant.sv; 5 test file(s); 4 tool(s)<br>retained pre-Packet-B fragment-expander oracle; zhao_texture_frag_expand_v2 is reachable inside the selected V3 root |
| `zhao_texture_material_combine_v1` | `texture/zhao_texture_material_combine_v1.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 6 test file(s); 6 tool(s)<br>retained pre-Packet-B combiner oracle; zhao_texture_material_combine_v3 is reachable inside the selected V3 root |
| `zhao_texture_metajoin` | `texture/zhao_texture_metajoin.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_texture_early_desc_slotswap_mutant.sv; 4 test file(s); 3 tool(s)<br>retained pre-Packet-B metadata-join oracle; zhao_texture_metajoin_v2 is reachable inside the selected V3 root |
| `zhao_texture_tmu_pipe` | `texture/zhao_texture_tmu_pipe.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 7 test file(s); 5 tool(s)<br>retained pre-Packet-B texture-pipe oracle; the selected accounting root is zhao_texture_island_v3_top |
| `zhao_texture_uv_join` | `texture/zhao_texture_uv_join.sv` | `superseded` | design/fit_targets.yml; tests/CMakeLists.txt; 7 test file(s); 1 tool(s)<br>retained pre-Packet-B UV-join oracle; zhao_texture_uv_join_v2 is reachable inside the selected V3 root |

### INFRASTRUCTURE -- 30

| module | file | manifest reason | evidence that decided it |
| --- | --- | --- | --- |
| `shell_v2_top` | `generated/zhao_shell_v2_fit_top.sv` | `probe` | tests/CMakeLists.txt; 1 test file(s); 1 tool(s)<br>generated isolated sibling shell-fit wrapper; characterization only, never production |
| `zhao_debug_trace` | `debug/zhao_debug_trace.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 1 tool(s)<br>the trace port; a bring-up instrument, not shipped logic |
| `zhao_field_seq` | `field/zhao_field_seq.sv` | `frozen` | tests/CMakeLists.txt; tests/formal/field_seq_bound.sby; 4 test file(s); 10 tool(s)<br>FIELD v1 sequencer |
| `zhao_field_sinks` | `field/zhao_field_sinks.sv` | `frozen` | tests/CMakeLists.txt; 2 test file(s); 1 tool(s)<br>FIELD v1 |
| `zhao_field_v2_front` | `field/zhao_field_v2_front.sv` | `frozen` | tests/CMakeLists.txt; 2 test file(s); 4 tool(s)<br>FIELD v2 front end |
| `zhao_pair_fragment_tilestore` | `synth/zhao_pair_fragment_tilestore.sv` | `probe` | design/fit_targets.yml<br>a leaf-fit pair around FRAGMENT + TILESTORE |
| `zhao_pair_pagestream_patch` | `synth/zhao_pair_pagestream_patch.sv` | `probe` | design/fit_targets.yml<br>characterisation wrapper, PAGESTREAM+PATCH; both blocks counted individually |
| `zhao_pair_setup_binner` | `synth/zhao_pair_setup_binner.sv` | `probe` | design/fit_targets.yml<br>a leaf-fit pair around SETUP + BINNER |
| `zhao_pair_tess_normals` | `synth/zhao_pair_tess_normals.sv` | `probe` | design/fit_targets.yml<br>a leaf-fit pair around TESS + NORMALS |
| `zhao_pair_tmu_cache` | `synth/zhao_pair_tmu_cache.sv` | `probe` | design/fit_targets.yml<br>a leaf-fit pair around the OLD TMU + OLD cache |
| `zhao_probe_banked_rf` | `synth/zhao_probe_banked_rf.sv` | `probe` | **none**<br>register-file banking experiment |
| `zhao_probe_compose_pipe` | `synth/zhao_probe_compose_pipe.sv` | `probe` | **none**<br>composition timing experiment |
| `zhao_probe_ctx_fifo` | `synth/zhao_probe_ctx_fifo.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 3 tool(s)<br>context FIFO sizing experiment |
| `zhao_probe_dist_svc` | `synth/zhao_probe_dist_svc.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 4 tool(s)<br>distance service fit probe |
| `zhao_probe_patch_acc` | `synth/zhao_probe_patch_acc.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 3 tool(s)<br>patch accumulator fit probe |
| `zhao_probe_ram_infer` | `synth/zhao_probe_ram_infer.sv` | `probe` | design/fit_targets.yml<br>RAM-inference discriminator probe; five/three array variants at one geometry |
| `zhao_probe_render_fb` | `synth/zhao_probe_render_fb.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 1 tool(s)<br>renderer+framebuffer fit probe |
| `zhao_probe_v3_full` | `synth/zhao_probe_v3_full.sv` | `probe` | tests/CMakeLists.txt; 3 test file(s); 4 tool(s)<br>FIELD v3 whole-engine probe |
| `zhao_probe_v3rq_queue` | `synth/zhao_probe_v3rq_queue.sv` | `probe` | design/fit_targets.yml; tests/CMakeLists.txt; 2 test file(s); 1 tool(s)<br>characterisation wrapper for one production-shaped ready queue (5.7) |
| `zhao_probe_walk_earth` | `synth/zhao_probe_walk_earth.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s); 3 tool(s)<br>Earth60 walk probe |
| `zhao_prod_top` | `prod/zhao_prod_top.sv` | `harness` | design/fit_targets.yml; 12 test file(s); 8 tool(s)<br>the GENERATED resource top itself; it holds the instances, it is not one |
| `zhao_raster_quant` | `raster/zhao_raster_quant.sv` | `probe` | design/fit_targets.yml; tests/CMakeLists.txt; tests/formal/raster_resolve_quant.sby; 7 test file(s); 5 tool(s)<br>the COMPOSITION of zhao_quant_num and zhao_raster_quant_fin, retained as the subject formal_raster_resolve_quant proves. RASTER.RESOLVE instantiates the two halves directly, across a register, so nothing instantiates this module any more --... |
| `zhao_raster_texture_v3_fit_top` | `generated/zhao_raster_texture_v3_fit_top.sv` | `probe` | design/fit_targets.yml; tests/CMakeLists.txt; 4 test file(s); 13 tool(s)<br>Packet-F G8A generated physical-pin/MISR raster-texture characterization wrapper; never a production root |
| `zhao_render_texture_layout_guard` | `common/zhao_render_texture_pkg.sv` | `probe` | tests/mutants/zhao_render_texture_wrong_layout_mutant.sv; 2 test file(s); 1 tool(s)<br>Packet-A exact packed-layout static/runtime instrument for zhao_render_texture_pkg; uninstantiated by every selected top and exercised only by the committed layout fixture/mutant |
| `zhao_shell_fit_top` | `generated/zhao_shell_fit_top.sv` | `probe` | tests/CMakeLists.txt; 18 test file(s); 8 tool(s)<br>generated isolated shell-fit wrapper; characterization only, never production |
| `zhao_stub_top` | `common/zhao_stub_top.sv` | `harness` | tests/CMakeLists.txt; 4 test file(s); 2 tool(s)<br>the blank top used to measure the fitter's own floor |
| `zhao_synth_probe` | `common/zhao_synth_probe.sv` | `harness` | 2 tool(s)<br>the generic synthesis probe wrapper |
| `zhao_terrain_pipe_rpp3_matw18_fit_top` | `generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv` | `probe` | design/fit_targets.yml; tests/CMakeLists.txt; 3 test file(s); 2 tool(s)<br>Packet-I G8B generated physical-pin/MISR terrain characterization wrapper at ROWS_PER_PASS=3/MATW=18; never a production root |
| `zhao_texture_ident_probe` | `texture/zhao_texture_ident_probe.sv` | `probe` | tests/CMakeLists.txt; 2 test file(s)<br>exposes the identity encoding functions for ident_encoding_directed |
| `zhao_texture_island_top` | `texture/zhao_texture_island_top.sv` | `probe` | design/fit_targets.yml; tests/CMakeLists.txt; 2 test file(s); 9 tool(s)<br>retained G1-D composition oracle; not a selected accounting root and its descendants do not enter the census through it |

### CANDIDATE -- 17

| module | file | manifest reason | evidence that decided it |
| --- | --- | --- | --- |
| `zhao_engine1_raw_last_v2` | `memory/zhao_engine1_raw_last_v2.sv` | `not-yet-adopted` | tests/CMakeLists.txt; 5 test file(s); 2 tool(s)<br>Packet-H independent ENGINE1 controller-retirement validator and held raw16 framer for the excluded asset mux; exact 16/32/64-byte counts, 1-8-halfword credits, missing-final detection, FINAL_HELD backpressure, and early/missing/late/state ... |
| `zhao_fb_tuple_contract` | `video/zhao_fb_tuple_contract.sv` | `not-yet-adopted` | tests/CMakeLists.txt; 2 test file(s)<br>Packet-H elaboration check that the six READY/swap fields tile the 84-bit CDC tuple exactly -- no hole and no overlap; carries no logic and exists so a wrong width is a compile failure rather than a test failure |
| `zhao_field_progdir` | `field/zhao_field_progdir.sv` | `not-yet-adopted` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_field_progdir_scan_mutant.sv; 5 test file(s)<br>the scanned FIELD program directory (roadmap s5 candidate); transaction-identical to zhao_field_progcache under tests/field/field_progdir_differential.cpp at ENTRIES=16/2/3 with a committed tie mutant seen to fail; 20 clocks per transaction... |
| `zhao_forge_cliff_ram` | `forge/zhao_forge_cliff_ram.sv` | `not-yet-adopted` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_forge_cliff_ram_mutant.sv; tests/mutants/zhao_forge_cliff_ram_over_mutant.sv; tests/mutants/zhao_forge_cliff_ram_rowoff_mutant.sv; 3 test file(s)<br>the FORGE.CLIFF bitmap-RAM CANDIDATE beside the golden (ALM-Liberation Roadmap s6 / s14 Commit6; reports/FORGE-CLIFF-BITMAP-RAM-20260910.md): same law, same ports plus walk_fault_o, the 34x34 SOLID window in one RAM behind three row registe... |
| `zhao_forge_prim_eval` | `forge/zhao_forge_prim_eval.sv` | `not-yet-adopted` | design/fit_targets.yml; tests/CMakeLists.txt; tests/mutants/zhao_forge_prim_eval_mutant.sv; 2 test file(s)<br>the ADDLIGHTNING evaluator; unit-verified against zref::forge::eval_job (967 checks, determinism under 4 stall patterns, caps refused, mutant-fired overrun guard); adopt when the FX.LIGHTNING dispatch seam composes it with zhao_forge_prim; ... |
| `zhao_raster_attrwalk` | `raster/zhao_raster_attrwalk.sv` | `not-yet-adopted` | **none**<br>divider-free exact attribute stepping; differentials pass against the compiled reference and the verilated divider, but adoption needs the owner's ruling on the tie law (the repo's stated law and zref disagree on negative exact halves) |
| `zhao_render_asset_mux` | `memory/zhao_render_asset_mux.sv` | `not-yet-adopted` | tests/CMakeLists.txt; tests/mutants/zhao_render_asset_mux_mutants.sv; 5 test file(s); 1 tool(s)<br>Packet-E excluded local geometry/texture-fill owner mux behind the unchanged ENGINE1 MEM.GUARD client; real accept-then-verdict and explicit raw-last routing are unit-verified, but the protected shell cannot supply raw-last and G8A must pri... |
| `zhao_terrain_bake_v2` | `terrain/zhao_terrain_bake_v2.sv` | `not-yet-adopted` | tests/mutants/zhao_terrain_bake_v2_mutant.sv; 2 test file(s)<br>seven multiplier sites collapsed to one operand-muxed 34x34 and the 1,089-flop meets plane moved to one M10K; 267/267 against the zref oracle with a committed mutant seen to fail, but DSP 17 -> 3-6 is a STRUCTURAL PREDICTION until fit gate ... |
| `zhao_terrain_cmd` | `terrain/zhao_terrain_cmd.sv` | `unused` | design/fit_targets.yml; tests/CMakeLists.txt; 4 test file(s); 1 tool(s)<br>SubmitTerrainSet -> TERRAIN.SEQ's frame ring; fitted (1,069 ALM, no memory), and the command seam it would sit behind is an open ruling -- the shell's record framer carries 16 payload bytes and the command is 32 |
| `zhao_terrain_compcache_front` | `terrain/zhao_terrain_compcache_front.sv` | `unused` | tests/CMakeLists.txt; 4 test file(s)<br>TERRAIN.COMPCACHE's on-chip patch front; nothing composes PATCH -> TESS through it yet, and putting it in the production top before that composition exists would fit a store nothing reads |
| `zhao_terrain_loadq` | `terrain/zhao_terrain_loadq.sv` | `unused` | design/fit_targets.yml; tests/CMakeLists.txt; 4 test file(s)<br>the load queue between SEQ and PAGELOADER; fitted (1 M10K, 734 ALM, 107.33 MHz) but the shell composes neither end |
| `zhao_terrain_mipfeed` | `terrain/zhao_terrain_mipfeed.sv` | `unused` | design/fit_targets.yml; tests/CMakeLists.txt; 3 test file(s)<br>runs the streamer twice into MIPGEN for the second residency completion; fitted (343 ALM, no memory, no DSP) |
| `zhao_terrain_normalmap` | `terrain/zhao_terrain_normalmap.sv` | `unused` | 1 test file(s)<br>rebuilt to contract 2026-09-09 (zero-DSP, mip tail), directed-tested standalone; OPEN DEFERRAL until the fragment-seam wiring lands -- reports/TERRAIN-BUMP-MAPPING-ARCHITECTURE-20260909.md |
| `zhao_terrain_pageloader` | `terrain/zhao_terrain_pageloader.sv` | `unused` | tests/CMakeLists.txt; 4 test file(s); 2 tool(s)<br>the terrain page mover; the guard window and the client id now exist, but zhao_shell_top does not compose it yet |
| `zhao_terrain_seq` | `terrain/zhao_terrain_seq.sv` | `unused` | tests/CMakeLists.txt; 5 test file(s); 1 tool(s)<br>the terrain command sequencer; it drives RESIDENCY, PAGELOADER and COMPCACHE, none of which zhao_shell_top composes yet -- that composition is step 8 of the world-layer build sequence |
| `zhao_terrain_visible` | `terrain/zhao_terrain_visible.sv` | `unused` | tests/CMakeLists.txt; 2 test file(s); 1 tool(s)<br>the visible-patch builder; TERRAIN.LOD does not consume it yet |
| `zhao_terrain_writeback` | `terrain/zhao_terrain_writeback.sv` | `unused` | tests/CMakeLists.txt; 9 test file(s); 1 tool(s)<br>dirty-page evacuation; same reason |

### UNACCOUNTED -- 4

| module | file | manifest reason | evidence that decided it |
| --- | --- | --- | --- |
| `zhao_console_core` | `prod/zhao_console_core.sv` | `unaccounted` | design/fit_targets.yml; 3 test file(s)<br> |
| `zhao_geom_light` | `geometry/zhao_geom_light.sv` | `unaccounted` | tests/CMakeLists.txt; 2 test file(s)<br> |
| `zhao_light_skin_adapter` | `geometry/zhao_light_skin_adapter.sv` | `unaccounted` | tests/CMakeLists.txt; 2 test file(s)<br> |
| `zhao_light_stream` | `geometry/zhao_light_stream.sv` | `unaccounted` | tests/CMakeLists.txt; tests/mutants/light_stream_guard_mutant.cpp; tests/mutants/zhao_light_stream_guard_mutants.sv; 3 test file(s)<br> |
