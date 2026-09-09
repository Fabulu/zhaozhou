# V3 REARCHITECTURE ROADMAP — the texture island, decrufted at fit-gate granularity

2026-09-08. Architect's ordered work plan, written so a weaker agent can follow
it step by step without re-deriving anything. Authority chain, strongest first:

1. Owner ruling (2026-09-08): **fits are the dominant cost. Fit only at BIG
   ARCHITECTURAL SUBSYSTEM boundaries, never after every small change.** An
   island fit costs 1.5–4 h. This roadmap spends exactly the fits listed in
   THE FIT GATES section and no others.
2. Owner direction `49fc32e9`: finish the texture island. No terrain, no
   projection, no broad fit-sheet evacuation, no measurement-tool expansion.
3. `reports/ZHAOZHOU_THE_DECRUFTER_FPGA_TEXTURE_ISLAND_2026-09-08.txt` — the
   architecture brief. Sections referenced as §N below. Read the packet's brief
   section before executing the packet; this roadmap orders and concretises,
   it does not replace.
4. `C:\programmieren\zencrifice\CLAUDE.md` and the repo `CLAUDE.md` — the laws.
   The ones that bind hardest here: *a detector wired to two operands that move
   together cannot fire* (check what each side is clocked by); *a gate that
   cannot reach the state is not evidence about the state* (every test needs a
   falsifier shown red); *compare like with like or do not compare* (no
   mismatched-parameter fit comparisons); *predict what MOVES, not how far*
   (magnitude predictions from source were falsified twice on 2026-09-08);
   *read `rtlCleanAtHead` before `status`* (and labelled fit rows are NEVER
   rule-checked — their empty `ruleViolations` is silence, not compliance).

MapOnly (`run_block_fit.ps1 -MapOnly`, ~minutes) is **not a fit** for the
purpose of the owner's rule. Use it freely for structural evidence (§12.2):
did the bank infer, are the deleted arrays absent, is the DSP shape plausible.
It never substitutes for a fit gate's area/Fmax answer, and a full fit never
substitutes for MapOnly's named-structure attribution — run MapOnly first.

---

## STATUS UPDATE 2026-09-08 (late) — read this before the packet list

### PACKET 1 IS COMPLETE IN SIMULATION. Only its fit is owed.

| profile | `shadow_present_o` | comparators | checks |
|---|---|---|---|
| lab (default) | 1 | shadow 1176/0, align 792/0, bil 768/0, near 192/0 | **125** |
| production `-GMIGRATION_SHADOWS=0` | 0 | not elaborated, counters asserted 0 | **124** |
| oracle | n/a | untouched | **119** |

Both falsifiers run — `reports/PACKET1-FALSIFIERS-20260908.md`. The one that
mattered: with the laboratory absent, the reference differential still catches a
swapped-fraction mutation (3 colour checks, 32/32/29). **The laboratory is
apparatus, not enforcement**, so §4.3's boundary is drawn in the right place and
the packet may proceed.

**All of packet 1's deletions are done.** `class_m`, `f_class_in_c`, and the
three dead duplicate arrays `fpsl_m`/`fpgn_m`/`frec_m` with their aliases
`f_pal_slot_c`/`f_pal_gen_c`/`f_recipe_c` — nine code lines, word-boundary
matched. `palslot_m`/`palgen_m` untouched as specified (packet 3 owns them).

Re-verified after the deletions: **22/22** across the texture families including
`island_v3_paired` (gate 3's 392 byte-identical records), all 96 lint targets,
and 18 / 125 / 124 / 119 on the four composed gates — every count identical to
before.

A note for whoever reads the freeze below: **finishing packet 1 was correct even
while the island was "frozen".** The freeze is against applying PACKET 2 early,
because gate 1 would then measure the wrong packet. These deletions were
deliberately moved INTO packet 1 so gate 2 measures the descriptor bank alone, so
they had to land BEFORE gate 1's MapOnly. I nearly read my own constraint too
broadly and left the packet half-done.

### Detector sweep — no hopeful zeros left in this session's work

| detector | how it was made to fire |
|---|---|
| `uv_join.gen_mismatch_o` | stimulus: a token whose gen is not the bank's row |
| `early_desc` layout `$fatal` | `-GGENW=9`; lint alone passes RC=0, so the fatal is load-bearing |
| `metajoin` layout `$fatal` | `-GGENW=9`; lint refuses it outright, so the fatal is a Quartus backstop |
| pairpipe depth-zero U/V | asserted exactly zero instead of merely excluded |
| `frag_expand.wq_overflow_o` | **committed mutant** — unreachable by any legal stimulus |

The last one is the pattern worth reusing: `tests/mutants/`, renamed module,
inverted polarity (passes when the counter fires). See CLAUDE.md.

### FIT GATE 0 IS LOST. Do not wait for it.

The `@d0fixed` island fit and the chained expander refit were both killed at
~195 and ~174 minutes by an external stop that took the wrappers and their
Quartus children together. No row was written; the receipt is intact at 119 rows.

**It is not being re-run**, and that is a deliberate call under the owner's fit
ruling: three hours to attribute a one-line register-enable change is not what a
fit is for. The D0 repair folds into gate 1's measurement, and the baseline is
`@pktC-fixed` (15,483 ALM / 62.83 MHz reported, 77.45 internal-only, clean tree,
`1b81c013`). State the confound when quoting gate 1: its delta covers the D0
gate plus the shadow gating plus the dead-array deletions.

### The gate plan as it now stands

| gate | what | status |
|---|---|---|
| ~~0~~ | `@d0fixed` baseline | **LOST, folded into gate 1** |
| 4 | RCP pair at matched NCTX=12/TOKW=14 | **RUNNING** — owner approved "halve the DSPs even if it costs" |
| 1-pre | MapOnly pair, `@g1-lab` vs `@g1-prod` | **QUEUED** behind gate 4 |
| 1 | island fit, production profile | after 1-pre |
| 2 | island fit, descriptor + join + palette | after packets 2-3 |
| 3 | pairpipe + fresh svc leaf pair | candidate ready, 22 checks green |
| 5 | checkpoint C | last |

### PACKETS 2 AND 3 ARE COMPLETE IN SIMULATION (2026-09-09)

The island runs the descriptor bank and the UV join. The expander and Mosaic are
fed from one captured record; the palette pair is CARRIED rather than looked up;
the metajoin exports the owner generation and the queued record carries it
instead of a literal `8'd0` (D0d closed).

| | lab | production | oracle |
|---|---|---|---|
| composed | **125** | **124** | **119** |
| fault | 18 | — | — |

Plus `island_v3_paired`, `desc_join_expand` 15 (palette checked on the EMITTED
REQUEST, falsifier 380/384), `frag_expand` 12, `metajoin` 7, seam 7, `early_desc`
11, `uv_join` 13, `pairpipe` 25, `perspuv_lockstep` 9.

#### Three departures from this plan, all recorded where they were made

1. **`tmu_plan`'s carriage is behind `PAL_CARRY`, default OFF.** The plan did not
   note that `zhao_texture_island_top` — THE ORACLE — instantiates that block.
   Unconditional carriage adds fifty flip-flops to the block gate 3's 392-record
   comparison rests on. Gated at the source so `PAL_CARRY=0` folds to nothing.
2. **`palslot_m`/`palgen_m` are NOT deleted**, they moved inside
   `MIGRATION_SHADOWS`. Their remaining readers are the shadow and the CLUT
   alignment check, so keeping them makes the shadow into D3's migration proof:
   it now demonstrates on live traffic that the CARRIED pair equals what the
   sidecar would have said. 1,176 comparisons, 0 mismatches.
3. **Instrument ports on the bank and join are unconnected.** Wiring them adds
   island OUTPUT PORTS, and `island_composed_directed` is one file shared between
   this top and the oracle on the strength of their port lists matching. A
   separate guarded step; not worth coupling to the rewire.

#### What packet 3 still owes

Nothing structural. The remaining items are small and listed in the run log: the
untested island-level hop from `exp_wq_overflow` to its sticky bit, and
`frag_expand`'s pre-existing dead `binding` field (reserved for a resolver
contract that does not exist — §6 says so of `binding_selector` too).

---

### GATE 1 IS SETTLED BY THE MapOnly PAIR. Its full island fit is FOLDED into gate 2.

Decided 2026-09-09, and it deviates from the gate table above, so it is written
here rather than left implicit.

Gate 1 asked what the migration laboratory costs. The controlled MapOnly pair
answered it: **−4,432 registers, −640 memory bits, 0 DSP**, 98.1% attributable to
named structures, same bytes on both sides by digest. That is the question gate 1
existed for and it is answered.

What the pair cannot give is ALM and Fmax at production config. Spending a
three-hour island fit for those two numbers alone is precisely what the owner's
ruling forbids -- *"only fit at big architectural subsystem"* -- and this session
has already lost a 195-minute island fit, a 174-minute expander refit and a
102-minute leaf fit to external stops. A long fit is a bet against the next one.

**So the next island fit is gate 2**, covering packets 1, 2 and 3 together, and
its delta is read against the anchored `@pktC-fixed` row. The confound is stated
rather than hidden: that delta contains the D0 gate, the laboratory removal, the
dead-array deletions, the descriptor bank, the join and palette carriage.

The register component is NOT confounded, because the MapOnly pair already
isolated it. That is the point of having spent minutes instead of hours.

**The island is therefore UNFROZEN.** The constraint below is discharged.

---

### ~~A LIVE CONSTRAINT — do not edit the island right now~~ (DISCHARGED)

*Kept for the reasoning, which generalises: a queued measurement snapshots the
live tree when it STARTS, so applying later work first makes it measure the wrong
thing — and the row looks perfectly normal. That is the live-tree trap in its
quiet form: not a fit that fails, a measurement that succeeds and describes
something else.*


`tools/quartus/queue_gate1_maponly.ps1` is waiting for the toolchain and will
snapshot the live tree the moment gate 4 exits. **Applying Packet 2 before that
pair has run would silently make it measure Packet 2 instead of Packet 1**, and
the row would look perfectly normal. Wait for `GATE1MAPONLY DONE`.

This is the live-tree trap in its quiet form: not a fit that fails, a
measurement that succeeds and describes something else.

---

## STATE AT TIME OF WRITING (verify before starting — §14.1)

* HEAD `273b1354` ("the post-PERSPUV join (Decrufter 5.3), composed with the
  real bank"). Working tree carries 4 modified test files (uncommitted test
  polish) and one untracked fit artifact
  `reports/synthesis/blockpaths/zhao_texture_island_v3_top@d0fixed.sources.sha256`.
* **An island fit labelled `@d0fixed` is RUNNING** (two `quartus_fit.exe`
  processes observed alive). Its 19-file closure is the `zhao_texture_island_v3_top`
  entry at `design/fit_targets.yml:1079-1099`. **Do not edit any file in that
  list until the fit exits** (QUARTUS_GOTCHAS §11; the Stop hook enforces it).
  `zhao_texture_early_desc.sv` and `zhao_texture_uv_join.sv` are NOT in that
  closure — they are safe to edit at any time, as are all tests and tools.
* The D0 metadata-swap repair is landed (`ccbb6d8c`, gate in
  `fpga/rtl/texture/zhao_texture_metajoin.sv`), seam test
  `tests/texture/metajoin_seam_directed.cpp` is 7/7 with three mutant-catcher
  checks that go red if the gate is removed.
* Last completed island receipts: `@pktC-fixed` 15,483 ALM / 22,219 reg /
  48 M10K / 17 DSP / 62.83 MHz reported (internal-only 77.45), clean tree —
  but it predates the D0 repair. `@pktC` (15,911/73.98) is UNQUOTABLE (dirty
  tree AND defective circuit). The unlabelled V3 row (13,133 ALM, clean,
  `failed:structure`) is the trustworthy pre-packet-C comparator — remember
  `failed:structure` means the fit COMPLETED and budget rules rejected it.

### Packet ordering vs the live-tree trap

Logical order is Packet 0 → 1 → 2 → 3 → 4 → 5 → 6. But packets 1–3 edit files
inside the island fit closure and packet 4 does not. **Whenever an island fit
is in flight, advance Packet 0 or Packet 4** — that is the fits-never-block-work
law applied to this roadmap. Before reading any fit result, write down in the
run's `TASK_LOG.md` where you were and what the next step was.

---

## PACKET 0 — close the `@d0fixed` receipt; land the deferred tool guards

**Goal.** Bank the anchored post-D0-repair baseline the running fit is buying,
preserve its perishable evidence, and land two small tool changes that were
explicitly deferred because the running fit's own script was executing. No RTL.

**Preconditions.** The `@d0fixed` fit has exited (check: no `quartus_fit.exe`
in `tasklist //FI "IMAGENAME eq quartus_fit.exe"`). Everything else in this
packet's first two steps can happen before it exits.

**Steps.**

1. Commit the 4 modified test files in the working tree (they are today's test
   polish for the new modules; read the diff first, commit with an honest
   message). Push. A dirty tree is what made `@pktC` unquotable — do not let
   the next receipt inherit one.
2. When the fit exits, read the new row in
   `reports/synthesis/zhao_block_fit.json`: **`rtlCleanAtHead` first**, then
   the numbers. `@d0fixed` is a labelled row, so `ruleViolations: []` is
   silence — run `tools/quartus/check_fit_rules.ps1` judgement manually against
   the island's rules (max_alms 7500 / max_registers 9000 / max_m10k 64 /
   max_dsp 14 at `design/fit_targets.yml:1100-1104`) and record the breaches in
   the run log as the historical-rule result, without treating them as the
   product decision (that is the owner's).
3. Immediately run `tools/quartus/worst_path_index.py` to bank the worst-path
   census (port-origin vs internal split) **before any later fit overwrites the
   setup reports** — this evidence is perishable; it was nearly lost once
   already today.
4. Land the `-TopParameters` guard in `tools/quartus/run_block_fit.ps1`: reject
   any element whose value contains `=` or `,`, with an error message naming
   the array-versus-string mistake (see
   `reports/RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE-20260908.md` for the mechanism —
   `'NCTX=8,TOKW=14'` as one string set NCTX to garbage and never set TOKW).
   This guard is a precondition for Packets 1 and 5, both of which pass
   parameters.
5. Record `@d0fixed`'s account with `tools/quartus/packet_accounting.py`
   against `@pktC-fixed` (both clean trees). Expect the delta to be the D0
   gate only — a handful of lines. If the numbers move materially for a
   one-gate change, that is a finding, not noise to smooth over.

**Closure hazard.** Steps 1–3 touch nothing in any closure. Step 4 edits the
fit runner itself — only after the fit exits.

**Acceptance in simulation.** None needed beyond the already-green suites; this
packet creates no behaviour. Falsifier for the step-4 guard: invoke
`run_block_fit.ps1` with `-TopParameters 'A=1,B=2'` (one string) and confirm it
refuses immediately with the naming message; then with `@('A=1','B=2')` and
confirm it emits two `set_parameter` lines into the QSF (inspect, do not fit).

**Fit gate.** No fit (the running one was already paid for; it is FIT GATE 0
in the table below).

**Rollback.** Trivial — each step is one small commit; revert individually.

---

## PACKET 1 — D1: put the migration laboratory behind `MIGRATION_SHADOWS`

**Goal.** One functional island source, two elaborations (§4.2): the lab
profile keeps every shadow comparator; the production profile must not
elaborate the reference table, comparison cones, counters or first-error
captures. This prices the laboratory (against FIT GATE 0's shadows-on
specimen) and establishes the production baseline every later deletion is
measured against. Also delete the one structure that is already dead.

**Preconditions.** Packet 0 steps 1–4 done. `@d0fixed` fit exited. No island
fit running.

**Exact changes.** All in `fpga/rtl/texture/zhao_texture_island_v3_top.sv`
(closure member) unless stated:

1. Add `parameter bit MIGRATION_SHADOWS = 1'b1` to the module header. Default
   1 so every existing test builds unchanged.
2. Wrap in `generate if (MIGRATION_SHADOWS) begin : g_shadows ... end
   endgenerate` — **explicit `generate`/`endgenerate` keywords; Quartus 17
   rejects the implicit form** (repo CLAUDE.md build note, cost a fit on
   2026-09-08):
   * `sampmeta_m` declaration (line ~1391) and its write (~1402);
   * the shadow reference registers and comparator: `mj_ref_q`,
     `mj_ref_pslot_q`, `mj_ref_pgen_q`, `mj_ref_v_q` and the
     `meta_shadow_mismatch_o` block (lines ~2740-2762);
   * the CLUT alignment check block (`meta_align_err_o`/`meta_align_chk_o`,
     lines ~2777-2790);
   * the bilinear/nearest per-queue comparators and first-error captures
     (`meta_bil_*`, `meta_near_*`, lines ~2795-2848).
   In the `else` arm, drive the shadow output ports to constant zero.
3. Add one output `shadow_present_o`, assigned the constant
   `MIGRATION_SHADOWS`. This is the capability contract §4.2 demands: a test
   asserts `shadow_present_o == 1` before trusting any shadow counter, so a
   disabled shadow reading zero can never again be read as "healthy" (the M9
   failure mode). Tests that assert shadow counters must first assert the
   capability; the production-config build must assert `shadow_present_o == 0`
   and skip those checks *by that evidence*, not by assumption.
4. Delete `class_m` outright — declaration (line 1239) and write (line 1268).
   **All-reader evidence: a whole-file grep finds 11 mentions, of which the
   only non-comment ones are the declaration and the write. It has zero
   readers today.** This is the brief's §6 "class_m if an all-reader audit
   confirms" — the audit is done and it confirms. Record it in the packet's
   deletion receipt.
5. Do NOT touch `palslot_m`/`palgen_m` — `u_metajoin`'s write side still reads
   them (lines 2707-2708). That real dependency is Packet 3's job (§4.2 last
   paragraph says exactly this).
6. Register the new configuration in `design/fit_targets.yml`: the island
   entry keeps its source list (no new files in this packet) — only the fit
   invocation changes, via `-TopParameters @('MIGRATION_SHADOWS=0')`.
7. The island is NOT instantiated in `zhao_prod_top.sv` (verified by grep), so
   no `gen_prod_top.py` regeneration is needed. State that check in the commit
   message so the next agent does not re-litigate the PINMISSING law.

**Closure hazard.** Edits `zhao_texture_island_v3_top.sv` — the island closure.
No island fit may be running.

**Acceptance in simulation** (all runnable in minutes, before any fit):

* Lab config (default): `island_composed_directed` (124 checks), the oracle
  build (119), `island_v3_fault_directed` (18), `island_v3_paired` gate 3 —
  all green, identical counts to the pre-packet run. This is the "same
  functional stream, errors, ordering and declared counters" half of §4.2.
* Production config: a second verilate target of `island_composed_directed`
  with `-GMIGRATION_SHADOWS=0` — identical retired stream, identical
  non-shadow counters, `shadow_present_o == 0` asserted, shadow-counter
  assertions skipped on that evidence.
* **Falsifier (must be shown red, then reverted):** in the lab config,
  temporarily swap `u_metajoin`'s `.wr_frac_u_i`/`.wr_frac_v_i` wires. The
  shadow mismatch counter must fire and the composed test must fail. This
  proves the laboratory still detects in the configuration that claims to
  carry it. (Do not commit the mutation; the positive control lives in the
  run log with its output.)
* **Second falsifier:** build the production config with the same swapped
  wires — the shadow counter must NOT fire (it does not exist) but the
  downstream colour checks in `island_composed_directed` must catch the wrong
  fractions. If nothing catches it in production config, STOP: the laboratory
  was load-bearing, and §4.3's boundary between migration proof and
  correctness enforcement is drawn in the wrong place. Report before
  proceeding.

**Fit gate.** **FIT GATE 1** ends this packet: MapOnly first
(`-MapOnly -TopParameters @('MIGRATION_SHADOWS=0')`), confirm in the RAM
summary and resource-by-entity report that no `sampmeta_m` RAM/registers and
no `meta_*` compare cones elaborate; then one full fit, production config,
labelled `@d1-production`. See THE FIT GATES.

**Rollback.** Revert the packet's commit; default parameter 1 means partial
states still build. The falsifier evidence lives in the run log, not the tree.

---

## PACKET 2 — D2: wire the early descriptor bank + UV join in; delete the early attribute forest

**Goal.** Replace the eleven live early-attribute arrays and their
combinational post-PERSPUV reads (island lines 663-728, 783-795, 884-900) with
the already-built, already-tested `zhao_texture_early_desc` (written once on
the owner admission handshake) and `zhao_texture_uv_join` (one enable moves
the whole record). The expander and Mosaic are then fed exclusively from the
captured descriptor — §5.3's sentence that is the whole point. `uvw_m` STAYS
(§5.4: already registered, do not undo).

**Preconditions.** Packet 1 landed and FIT GATE 1 receipts recorded. No island
fit running. Both new modules' leaf tests green (`early_desc_directed`,
`uv_join_directed` — they are, as of `273b1354`).

**Exact changes.**

1. `fpga/rtl/texture/zhao_texture_island_v3_top.sv`:
   * Instantiate `zhao_texture_early_desc` (`SLOTW=6, GENW=8, SLICEW=40`).
     Write side on the **owner admission handshake**: `wr_valid_i(own_adm_accept)`,
     `wr_slot_i(own_adm_owner[13:8])`, `wr_owner_gen_i(own_adm_owner[7:0])`,
     fields from the input pins of that same beat:
     `wr_aux_context_i(frag_ctx_i)`, `wr_lod_q4_4_i(frag_lod_i)`,
     `wr_raw_class_i(frag_class_i)`, `wr_needs_aux_i(frag_aux_i)`,
     `wr_sample_count_i(frag_sample_count_i)`,
     `wr_palette_slot_i(frag_pal_slot_i)`, `wr_palette_gen_i(frag_pal_gen_i)`,
     `wr_mosaic_mat_a_i(frag_base_rgb_i[23:16])`,
     `wr_mosaic_mat_b_i(frag_base_rgb_i[15:8])` — **SLICE VERIFIED 2026-09-08, these two are correct.** The check was
     the current `fbase_m` packing before wiring**: today Mosaic reads
     `fbase_rd[31:24]` and `fbase_rd[23:16]` of `{frag_base_rgb_i, frag_base_a_i}`,
     i.e. the TOP two bytes of `frag_base_rgb_i`. Preserve exactly those two
     bytes; a one-byte shift here is the five-stale-slices defect reborn —
     `wr_mosaic_weight_i(frag_weight_i)`, `wr_binding_sel_i(frag_binding_i)`.
   * ~~**Add the admission-agreement assertion.**~~ **STRUCK 2026-09-08 — the
     proposed detector CANNOT FIRE. Do not build it.** Two write events do
     exist (early arrays on `frag_valid_i && frag_ready_o`, line 784;
     `palslot_m`/`mat_m` on `own_adm_accept`, lines 1252/2492), and §5.2's
     concern is real. But the two predicates are algebraically the same
     expression, resolved from source:

     ```
     v3own:571  assign adm_accept_o = adm_valid_i && adm_ready_o;   (combinational)
     island:572 assign own_adm_valid_c = frag_valid_i && rcp_v_ready;
     island:550 wire   credit_available = own_adm_ready;
     island:620 assign frag_ready_o = rcp_v_ready && credit_available;

       own_adm_accept            = frag_valid_i && rcp_v_ready && own_adm_ready
       frag_valid_i && frag_ready_o = frag_valid_i && rcp_v_ready && own_adm_ready
     ```

     `adm_accept_o` is NOT registered inside v3own — it is a bare `assign` of
     the same handshake the island's `frag_ready_o` is built from. There is no
     second cone. This roadmap's own justification ("clocked by different
     cones … so it CAN fire") is wrong, and building the counter would produce
     a permanent zero that a later reader would quote as evidence the beats
     agree. That is precisely the defect D0b taught: **a detector whose two
     operands are the same signal is not a check, it is a decoration that
     reads reassuring.**

     **Do this instead**: make the invariant structural rather than watched.
     Derive both write enables from ONE named wire
     (`wire adm_beat_c = own_adm_accept;` used at 784, 1252 and 2492), so they
     cannot diverge by construction. §0's delete-don't-wrap. If a future edit
     genuinely gives the frontend a term the owner lacks, THAT is the moment a
     detector becomes meaningful — and it will have two real operands then.
   * **WIRING PRE-VERIFIED 2026-09-08 — every signal below exists at the width
     the bank and join expect. Do not re-derive; do check anything you change.**

     | island | width | connects to | width |
     |---|---|---|---|
     | `frag_ctx_i` | `CTXW=64` | `wr_aux_context_i` | 64 |
     | `frag_lod_i` | `LODW=8` | `wr_lod_q4_4_i` | 8 |
     | `frag_binding_i` | `BINDW=8` | `wr_binding_sel_i` | 8 |
     | `frag_pal_slot_i` | `PSW=$clog2(4)=2` | `wr_palette_slot_i` | 2 |
     | `frag_pal_gen_i` | `GENW=8` | `wr_palette_gen_i` | 8 |
     | `f_weight_c` | 8 | `wr_mosaic_weight_i` | 8 |
     | `pu_u`/`pu_v` | 32 | `p_u_i`/`p_v_i` | signed 32 |
     | `pu_sat`/`pu_dzero` | 1 | `p_sat_i`/`p_dz_i` | 1 |

     `pu_tag` is 16 bits and `p_tag_i` is 14, and the slice is SAFE: the
     perspuv instance is driven `tag_i({2'd0, px_tok_q})` (line 872, its own
     comment: *"16-bit tag, 14-bit handle: it fits"*), so `pu_tag[15:14]` is
     hard zero and `pu_tag[13:0]` is the whole owner handle. That also makes
     the Mosaic rewire exact rather than approximate: today Mosaic gets
     `req_src_id_i(pu_tag)`, and `{2'd0, join.f_owner_o}` reproduces those same
     sixteen bits bit for bit.

     Inside the join the handle is split `d_rd_slot_o = p_tag_i[13:8]` and
     `d_rd_owner_gen_o = p_tag_i[7:0]`, matching v3own's `{slot[5:0],
     gen[7:0]}`. `f_owner_o` returns the full 14 bits to the expander's
     `f_owner_i[13:0]`.

     One consequence worth seeing: `fc_rp = pu_tag[13:8]` (line 883) is the
     early arrays' read pointer, derived from the same slot field. When those
     arrays go, so does that pointer — it has no other use.

   * Instantiate `zhao_texture_uv_join` (`TAGW=14, SLOTW=6, GENW=8, CTXW=64`,
     `DZ_FORCES_ZERO_SAMPLES=0` — behaviour-preserving default) between
     PERSPUV and the expander:
     `p_valid_i(pu_valid)`, `p_ready_o` drives `pu_ready`,
     `p_u_i(pu_u)`, `p_v_i(pu_v)`, `p_tag_i(pu_tag[13:0])`,
     `p_sat_i(pu_sat)`, `p_dz_i(pu_dzero)`; `d_*` ports point at the bank.
   * Rewire the expander instance (lines 1094-1127) to the join's `f_*`
     outputs: `f_valid_i(join.f_valid_o)`, `f_ready_o` into `join.f_ready_i`,
     `f_owner_i(join.f_owner_o)` (replaces `exp_owner_c` at line 1079),
     `f_u_i/f_v_i(join.f_u_o/f_v_o)`, `f_binding_i(join.f_binding_o)`,
     `f_lod_i(join.f_lod_o)`, `f_count_i(join.f_count_o)`,
     `f_aux_i(join.f_aux_o)`, `f_ctx_i(join.f_ctx_o)`, and
     `f_class_i((join.f_class_o == CLS_ERR) ? CLS_NEAR : join.f_class_o)` —
     the sanitisation stays at the read point, applied to the CAPTURED class
     (the join passes raw through by design; its header says the rule lives at
     one place — this is that place). Keep the invalid-class counter behaviour
     unchanged (`err_class_invalid_o` at line 1290 already counts at admission
     from the ingress beat; leave it).
   * Rewire Mosaic (lines 916-927) to the join's Mosaic branch:
     `req_valid_i(join.m_valid_o)`, `req_ready_o` into `join.m_ready_i`,
     `req_mat_a_i(join.m_mat_a_o)`, `req_mat_b_i(join.m_mat_b_o)`,
     `req_weight_i(join.m_weight_o)`. **The join has no `m_u/m_v/m_src`
     ports** — wire `req_u_i(join.f_u_o)`, `req_v_i(join.f_v_o)`,
     `req_src_id_i({2'd0, join.f_owner_o})`. This is correct because all three
     come from the same held record (`r_u_q`/`r_v_val_q`/`r_tag_q`) as the
     `m_*` fields — one enable loaded them all. Say so in a comment; it looks
     like cross-branch borrowing and is not.
   * Wire the join's instruments (`joined_o`, `saturated_o`, `depth_zero_o`,
     `gen_mismatch_o`) and the bank's (`writes_o`, `reads_o`,
     `rd_gen_mismatch_o`) to new island counter outputs. Note: `pu_sat` and
     `pu_dzero` currently have NO consumer in the island — the flags were
     being dropped on the floor. The join's counters are their first real
     home; the composed test should assert `depth_zero_o` matches the
     workload's known zero-depth count (an exact-count check, per the
     counters-see-what-pictures-cannot law).
   * **Delete** (declarations, writes at 783-795, read wires at 884-900):
     `fctx_m`, `fbase_m`, `fbind_m`, `flod_m`, `fcls_m`, `faux_m`, `fpsl_m`,
     `fpgn_m`, `fsc_m`, `frec_m`, `fwt_m`, and the wires `fctx_rd`, `fbase_rd`,
     `f_binding_c`, `f_lod_c`, `f_class_raw_c`, `f_class_bad_c`, `f_class_c`,
     `f_aux_c`, `f_pal_slot_c`, `f_pal_gen_c`, `f_scount_c`, `f_recipe_c`,
     `f_weight_c`, `fr_f_ctx`, `exp_owner_c`, `fc_rp`. Keep `fc_wp` only if
     something still uses it (after this packet, `uvw_m`'s write does — check).
     Reader audit done for this roadmap: `f_pal_slot_c`/`f_pal_gen_c`/
     `f_recipe_c` are ALREADY dead (no consumer anywhere; the palette pair's
     one-time consumer was removed when `palslot_m`'s write moved to the input
     pins), `fbase_rd`'s only consumer is Mosaic, `fctx_rd`'s only consumer is
     the expander. §5.1's "audit readers first" is discharged; re-verify with
     grep before deleting, since the tree may have moved.
   * The `f_class_bad_c` planner-stage counter disappears with the wire; the
     admission-beat counter (line 1290) remains the class-invalid evidence.
     Note this diagnostic-definition change in the commit (per §7.3's rule:
     document changed diagnostics, do not tie them to zero and call them
     preserved).
2. `design/fit_targets.yml`: add
   `fpga/rtl/texture/zhao_texture_early_desc.sv` and
   `fpga/rtl/texture/zhao_texture_uv_join.sv` to the island target's source
   list (they are not there today — a fit launched without this edit dies at
   analysis or, worse, elaborates a stale library copy).
3. `tests/texture/island_composed_directed.cpp` (and/or a new
   `island_desc_directed.cpp` if the file is getting unwieldy): add §5.6's
   coverage —
   * every descriptor field DIFFERENT between adjacent admitted fragments
     (the current workload varies few fields; constant-palette phases prove
     nothing about identity);
   * more than 64 owner admissions forcing slot reuse, comparing the FULL
     64-bit context at retirement, not the low-16 tag;
   * independent stalls: RCP return, PERSPUV output, expander acceptance, AUX;
   * mixes of zero-work, AUX-only, and three-sample fragments;
   * exact-count assertions on `joined_o` (= fragments through PERSPUV) and
     the bank's `reads_o` (= `joined_o`) and `writes_o` (= admissions).

**Closure hazard.** Island top and `fit_targets.yml` — island closure. The two
new RTL files are about to JOIN the closure; after this packet they are hot
whenever an island fit runs.

**Acceptance in simulation.**

* All existing composed suites green in both MIGRATION_SHADOWS configs:
  `island_composed_directed` 124, oracle 119, `island_v3_fault_directed` 18,
  `island_v3_paired` gate 3 byte-identical records, `frag_expand_directed`,
  `metajoin_seam_directed` 7/7.
* The new §5.6 checks green, with their exact counts printed.
* `err_adm_disagree_o == 0` across every composed workload.
* `gen_mismatch_o == 0` (join) and `rd_gen_mismatch_o == 0` (bank) across the
  slot-reuse workload — AND the positive control: the leaf test
  `early_desc_directed` already fires the bank's mismatch counter deliberately
  (its "after_stale > after_match" check); cite that as the proof the zero is
  a live instrument, not a broken one.
* **Falsifiers (each shown red, then reverted, output in the run log):**
  1. Feed the expander `f_ctx_i(frag_ctx_i)` (live ingress) — the
     varied-context slot-reuse test must fail. `tools/rtl/check_ingress_capture.py`
     should also flag it; run it and record both.
  2. Remove the `if (rd_valid_i)` read gate in `zhao_texture_early_desc.sv` —
     `uv_join_directed`'s stall/interleave checks must fail (this is D0's
     mutant transplanted to the new bank).
  3. Write the bank with a free-running local pointer instead of
     `own_adm_owner[13:8]` — the slot-reuse test must fail (§5.2's drift
     hazard made falsifiable).

**Fit gate.** **No fit.** MapOnly only, labelled `@d2-map`: confirm the three
40-bit descriptor slices infer (64×40 simple dual port each), the generation
side stays out of M10K (register/MLAB territory — the bank's header explains
why), and none of the eleven deleted arrays appears in the RAM summary or
register attribution. If the payload bank stays as thousands of flops, STOP —
§5.4 says that is not acceptable; fix the template (check the enable/reset
shape against V01) before spending anything further.

**Rollback.** Revert the packet's commits; the deleted arrays return with the
revert. Keep the packet to at most three commits (bank+join wiring / array
deletion / tests) so a partial revert is possible — the deletion commit must
be separate from the wiring commit, because the wiring can be right while a
deletion is premature.

---

## PACKET 3 — D3: carry palette identity; delete the owner palette sidecars; give D0d its port

**Goal.** The metajoin's write side stops reading `palslot_m`/`palgen_m` by
owner slot (island lines 2707-2708) and instead consumes a palette pair that
travelled WITH the request: descriptor → join → expander queue → planner
stages → accepted-sample metadata write (§6). Then the sidecars are deleted.
The same packet lands D0d: the owner generation gets an output port from the
metajoin and rides the queued record instead of the literal `8'd0`.

**Preconditions.** Packet 2 landed, its composed suites green. No island fit
running (this packet edits four closure files).

**Exact changes.**

1. `fpga/rtl/texture/zhao_texture_uv_join.sv` — **the roadmap's first
   discovered gap: the join does not currently expose palette identity.** The
   bank outputs `rd_palette_slot_o`/`rd_palette_gen_o`, but the join neither
   consumes nor forwards them. Add inputs `d_palette_slot_i[1:0]`,
   `d_palette_gen_i[GENW-1:0]` and outputs `f_pal_slot_o`, `f_pal_gen_o`
   assigned from them (same pattern as `f_lod_o` — the bank's held output IS
   the captured record, per the module's own header). Extend
   `tests/texture/tb_uv_join_pair.sv` and `uv_join_directed.cpp`: the `Desc`
   struct already varies `pslot`/`pgen` per slot; add the two fields to the
   retirement comparison.
2. `fpga/rtl/texture/zhao_texture_frag_expand.sv`: add `f_pal_slot_i[1:0]`,
   `f_pal_gen_i[7:0]`; store them in the FQD=4 input queue and the
   current-fragment record **with the same capture enables as `f_binding_i`**
   (§6: "Use the same capture enables as the request fields already in each
   stage"); output `req_pal_slot_o`, `req_pal_gen_o` beside `req_src_id_o`.
   All three samples of one fragment carry the same pair (it is per-fragment
   state; the declared material rule). Do NOT pack it into the 18-bit
   class/sample token — §6 is explicit there are no free bits there.
3. `fpga/rtl/texture/zhao_texture_tmu_plan.sv`: add `req_pal_slot_i`,
   `req_pal_gen_i`, carried through every accepted stage with the same
   enables as `req_src_id` (t0→t3), out as `acc_pal_slot_o`, `acc_pal_gen_o`.
   On stall it holds; on the stages' existing flush behaviour it follows the
   request. Advance when THAT request advances, never merely when input valid
   is high.
4. `fpga/rtl/texture/zhao_texture_metajoin.sv`: add output
   `rd_owner_gen_o[GENW-1:0]` driven from `rd_gen_q` (which, post-D0-repair,
   belongs to the same read as the row — that is what makes this port
   meaningful). No other change; the write ports already take the pair.
5. `fpga/rtl/texture/zhao_texture_island_v3_top.sv`:
   * `u_metajoin`: `.wr_pal_slot_i(plan_acc_pslot)`, `.wr_pal_gen_i(plan_acc_pgen)`
     (the planner's new accepted-stage outputs), replacing the
     `palslot_m`/`palgen_m` reads.
   * `mj_meta_packed_c` (line ~2683): replace the literal `8'd0` with the new
     `rd_owner_gen_o`. The queued record's §3.4 "40-bit story" is then honest:
     the generation the bank validated travels to the consumers.
   * Add ONE downstream observer: at CLUT dispatch (or all three lanes if
     cheap), compare `disp_*_meta[39:32]` against `disp_*_tok[GENW-1:0]`.
     **Clocking analysis, required by the lockstep-detector law:** the two
     operands ride the same FIFO entry, but they ORIGINATE in different
     domains — the stored generation was written at planning from the
     planner's token; the queue token came from the cache response. A stale
     row therefore differs from its token, so this detector CAN fire; fire it
     once deliberately (drive a read for a slot whose row was rewritten) in
     the seam test as its positive control.
   * **Delete** `palslot_m`/`palgen_m` (declarations 1249-1250, writes
     1269-1270). Their remaining readers at this point are only the
     lab-config shadow references (`mj_ref_pslot_q`/`mj_ref_pgen_q`, line
     2748-2749) and the CLUT alignment check (2785-2786) — both inside
     `g_shadows`. Rework those: the palette half of the shadow comparison
     moves to the TEST's scoreboard (the testbench knows each fragment's
     binding; §2: "one verification scoreboard can remember the expected
     metadata for a token without a second production copy"). The 21-bit
     `sampmeta_m` half of the lab shadow stays as-is until a later explicit
     retirement of the whole laboratory.
6. `tests/texture/metajoin_seam_directed.cpp` + composed test: §6's required
   test — ALTERNATING palette slot/generation on adjacent fragments under
   variable planner stalls, checked at the response side per record.

**Closure hazard.** Four closure files (`island_v3_top`, `frag_expand`,
`tmu_plan`, `metajoin`) plus `uv_join` (in closure after Packet 2). Nothing in
this packet may start while an island fit runs.

**Acceptance in simulation.**

* All composed suites green, both shadow configs.
* The alternating-palette-under-stall test green with exact per-record
  comparison (not counts).
* The D0d observer at zero across all workloads, plus its positive control
  shown firing.
* **Falsifier (the brief names this one explicitly, §6 DELIVERABLE):** mutate
  the metajoin write to take `frag_pal_slot_i`/`frag_pal_gen_i` (live ingress
  pair) — the alternating-palette test MUST fail. Show red, revert, log.
* **Second falsifier:** advance the planner's palette sideband on
  `req_valid_i` alone (ignoring ready) — the stall variation of the test MUST
  fail (this is the "advance when THAT request advances" clause made
  falsifiable).

**Fit gate.** **FIT GATE 2** ends this packet: MapOnly `@d23-map` first —
confirm absent: all eleven early arrays, `palslot_m`, `palgen_m`, `class_m`;
present: three descriptor slices, metajoin bank unchanged shape. Then one full
fit, production config, labelled `@d23-production`. This is the brief's
checkpoint B, combined D2+D3 as §12.3 allows — label it combined; do NOT
apportion its ALM/MHz delta between D2 and D3 (no evidence would support the
split, and fabricating one is the exact sin §12.3 names).

**Rollback.** Revert in reverse commit order. The packet must be committed as:
(a) uv_join+bank port additions with leaf tests, (b) expander sideband,
(c) planner sideband, (d) island rewire + sidecar deletion, (e) tests. If the
fit gate rejects (see §13.2 criteria), the fallback that preserves correctness
is reverting (d) alone — carriage machinery without the deletion is inert but
harmless, and the sidecars return.

---

## PACKET 4 — D5: the paired PERSPUV candidate, standalone

**Goal.** Build `zhao_raster_perspuv_pairpipe` beside the frozen service
(§8.2): ONE scheduler (licensed by the inductive lockstep proof in
`reports/PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md` — one pointer pair, one
emptiness test, one token select), TWO arithmetic lanes (the proof licenses no
narrower shape; do not serialize the axes), 64-bit exact arithmetic copied
verbatim from the service (§8.6 — the k=32 counterexample forbids 56-bit
narrowing at this port), terminal credits reserved at ACCEPTANCE
(owned = pipeline + terminal FIFO + held output ≤ **17**: NTOK=16 plus the
registered output stage the old service releases contexts into — §8.4's
source-based capacity audit; do not silently shrink it).

**This packet touches no island file and can run WHILE any island fit is in
flight.** It is the designated parallel work.

**Preconditions.** None beyond the lockstep proof (done). Can start
immediately.

**Exact changes.**

1. New `fpga/rtl/raster/zhao_raster_perspuv_pairpipe.sv`. Parameters
   `NTOK=16`, `TAGW=16` (keep 16 for the like-for-like leaf comparison; a
   TAGW=14 island instantiation later is a deliberate, documented wrapper
   contract, not this packet's business — §8.5). Structure per §8.2:
   accept-with-credit → capture operands → two parallel products → rounding
   add → exact rescale (copy the `sh = six_bit(32-k)` / 64-bit signed
   round-and-shift expressions from `zhao_raster_perspuv_svc.sv` EXACTLY —
   same widths, same wrap) → saturation/flags → one paired 82-bit record
   {U32, V32, tag16, sat1, dz1} into the terminal FIFO → stable ready/valid
   output that holds the COMPLETE packet while stalled (the D0 counterexample
   is the FIFO-head checklist, §8.5). Zero-denominator requests take the SAME
   ordered pipeline with a carried flag — no bypass path that could let a zero
   overtake a nonzero (§8.2). Count zero products separately.
2. New empirical lockstep backstop on the OLD service (deferred from the proof
   report because the svc file sat in a running fit's closure — reading
   internals needs only a verilate flag, no source edit): a new test target
   verilating `zhao_raster_perspuv_svc` with `--public-flat-rd`, asserting
   every cycle across a workload that includes depth-zero fragments:
   `wp[0]==wp[1]`, `rp[0]==rp[1]`, `pk_v[0]==pk_v[1]`,
   `pk_v[0] -> pk_i[0]==pk_i[1]`. This is the assertion set §8.1 requires
   BEFORE the rewrite is trusted.
3. New `tests/raster/perspuv_pairpipe_directed.cpp` + a differential pair
   bench (pattern: `tests/raster/tb_perspuv_pair.sv`, which is a starting
   reference, not automatic coverage — §8.7): same stimulus into svc and
   pairpipe, results compared in ACCEPTED ORDER (the interface order is the
   contract; v3own restores fragment order — no reorder buffer here, §8.2).
   Required coverage: zero/nonzero mixtures, saturation, signed extremes
   (signed minimum, maximum, halfway rounding, negatives — §8.6 mandatory),
   repeated tags, reset mid-flight, full-window reuse, long output stalls,
   sustained one-pair-per-clock after fill (measured, printed), output-stall
   burst capacity = 17 exactly (accepted-minus-emitted ceiling).
4. Register `zhao_raster_perspuv_pairpipe` as a new top in
   `design/fit_targets.yml` (required before any MapOnly/fit can target it —
   §12.2), with the same rules block as the svc entry.
5. Wire the new tests into `tests/CMakeLists.txt` (pattern: the
   `uv_join_directed` block at lines 2232-2248) with `fast;nightly` labels and
   a lint target.

**Closure hazard.** None for the island. Once registered, the pairpipe file is
in ITS OWN fit closure during FIT GATE 3 — do not edit it while its leaf fit
runs.

**Acceptance in simulation.**

* Differential: every accepted pair appears exactly once, never separates its
  axes, matches svc bit-for-bit in value and flags, in accepted order, across
  all listed stimulus classes. Exact job counts asserted (counters see what
  pictures cannot: assert products = 2× nonzero accepts, zero-jobs = zero
  accepts).
* Rate: ≥ 1 pair/clock sustained with consumer ready after fill; print the
  measured rate — no magnitude prediction, the number is whatever it is.
* Credit ceiling: with output held, exactly 17 accepts then `p_ready` low;
  release one, accept one.
* **Falsifiers:**
  1. Free the credit at pipeline-writeback instead of at external acceptance —
     the long-stall burst test must fail (this is the cache's documented
     lost-response bug; §8.4 says do not reintroduce it, so prove the test
     would catch it).
  2. Drop one arithmetic lane and serialize — the rate test must fail.
  3. Replace the 64-bit rescale with a 56-bit one — the k=32 directed vector
     (zero product, sh wrap to 63 → negative saturated result) must fail.
     Take the vector from [M04]'s counterexample.

**Fit gate.** **FIT GATE 3** ends this packet: MapOnly on pairpipe first —
the §8.3 deletion ledger checked structurally: NO `e_num_u/v`, `e_mant_u/v`,
`wq[0]/wq[1]` pointer banks, `e_have` join, `e_q_u/e_q_v` result tables in the
netlist. Then TWO leaf fits, same profile (NTOK=16, TAGW=16), same virtual-pin
observation boundary, same constraints: `zhao_raster_perspuv_pairpipe` and a
fresh `zhao_raster_perspuv_svc` row from the SAME commit (the standing svc row
predates today's tree; never compare against a stale measurement). Leaf fits
are ~20-40 min each — this is the subsystem boundary for this candidate, and
two cheap leaf fits here prevent a wasted 4-hour island fit later.

**Rollback.** The candidate is additive — nothing depends on it until
Packet 6. If FIT GATE 3 says not materially smaller or timing-worse: keep svc,
record the result plainly (§8.7 — a fallback experiment on svc's duplicate
scheduling alone is permitted, but do NOT keep adding infrastructure to rescue
the candidate), and strike Packet 6's PERSPUV half.

---

## PACKET 5 — D4: the RCP decision memo (owner), then the matched pair if approved

**Goal.** Put the RCP V3 question to the owner in decision-ready form, and if
— only if — the owner approves pursuing it, produce the like-for-like leaf
measurements that today do not exist. **Do not swap RCP into the island in
this packet under any outcome.**

**Why this is gated on the owner.** Two of today's reports settle the facts:
the recorded comparison was mismatched twice over (NCTX 16-vs-8, 8 M10K
unmentioned — `RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE`), and V3's throughput
advantage requires NCTX ≥ 12 while the island instantiates svc at NCTX=8; at
the island's profile V3 measures 5.78 clk/recip vs serial 6.96
(`RCP-V3-THROUGHPUT-IS-NCTX-DEPENDENT`). Buying V3's advertised behaviour
means buying 12-16 contexts plus M10Ks nobody has costed. The brief (§7.2)
says start at NCTX=16; the measurements say the island as-built does not need
what NCTX=16 buys unless the composed workload shows the reciprocal is a
limiter. That is a capacity/allocation trade — §9.4-class, owner's call.

**Steps.**

1. Write the one-page decision memo from the two reports (no new measurement
   needed): options are (a) keep svc, revisit if FIT GATE 2/5 attribution
   shows the reciprocal cone gating; (b) qualify V3 at NCTX=12/TOKW=14 (the
   measured cheapest profile that meets V3's own 4.6 clk/recip criterion) and
   svc at the SAME profile, then decide on the numbers. Include the standing
   fact that the composed `@pktC` worst path entered `rcp24_svc` — but that
   receipt is unquotable, so cite FIT GATE 0/1's worst-path census instead
   once banked.
2. If (b) approved: two leaf fits, `zhao_raster_rcp24_v3` and
   `zhao_raster_rcp24_svc`, both `-TopParameters @('NCTX=12','TOKW=14')` (the
   Packet-0 guard makes the array mistake impossible), reporting ALM,
   registers, DSP **and M10K** together (§7.2's four-axis rule — the missing
   M10K column is what made the original comparison a trap). Functional side
   is already covered: `raster_rcp24_v3_island_profile` runs in the fast lane,
   `_nctx10`/`_nctx12` nightly, all green with the shortfall printed.

**Closure hazard.** None (leaf fits of raster files; check nothing else has a
fit running on those files).

**Acceptance in simulation.** Already established (the four-point NCTX curve,
all arithmetic checks passing at every profile). Nothing new required.

**Fit gate.** **FIT GATE 4 (conditional)** — the matched pair, only on owner
approval. Otherwise this packet spends zero fits.

**Rollback.** Nothing to roll back; the memo is a report.

---

## PACKET 6 — compose the accepted candidates; checkpoint C

### PORT-COMPATIBILITY PRE-CHECK, done 2026-09-09 — the swap is nearly free

Every one of `zhao_raster_perspuv_svc`'s twenty ports exists on
`zhao_raster_perspuv_pairpipe`, so a swap leaves **nothing unconnected**. Two
differences, both now known instead of discovered later:

1. **`occupancy_o` widens from `[3:0]` to `[4:0]`, and this one can bite
   silently.** The pairpipe owns up to `CAP = NTOK + 1 = 17` items, which needs
   five bits. Both islands declare `logic [3:0] pu_occ;`
   (`island_v3_top.sv:882`, `island_top.sv:786`) and connect
   `.occupancy_o(pu_occ)`. Wiring a 5-bit output to a 4-bit signal **truncates**:
   16 reads as 0, 17 reads as 1. Verilator flags it as a WIDTH warning; Quartus
   simply accepts it.

   It is **harmless today** — `pu_occ` has exactly two code mentions in each
   island, its declaration and this connection, so nothing reads it. But
   "harmless because nobody reads it" is a latent trap, not a design. **Widen
   `pu_occ` to `[4:0]` in the same commit as the swap.**

2. **`zero_products_o [31:0]` is new** and purely additive — the count of
   depth-zero fragments that took the ordered path and produced no product. The
   island may leave it `()` or wire it to a counter output; nothing breaks
   either way.

Not asserted here: whether svc's own 4-bit `occupancy_o` can already misreport a
full queue at NTOK=16. It might be 0..15 by construction. That is svc's question,
not the swap's, and guessing at it is how a real finding gets diluted with a
speculative one.


**Goal.** Swap `u_persp` for the pairpipe (if FIT GATE 3 accepted it), and RCP
V3 at the owner-approved profile (if FIT GATE 4 happened and the owner said
yes). Rerun everything. One island fit measures the moved frontier (§12.3
checkpoint C).

**Preconditions.** Packet 3 landed (FIT GATE 2 recorded). FIT GATE 3 accepted
the pairpipe. Owner decisions from Packet 5 recorded. No island fit running.

**Exact changes.**

1. `zhao_texture_island_v3_top.sv`: replace the `u_persp` instantiation
   (line ~867) with `zhao_raster_perspuv_pairpipe`. The surrounding `px_*`
   skid stage and `uvw_m` registered read stay untouched (§10.1-adjacent —
   they solve a real latency problem; do not "clean them up" in passing).
   Tag wiring unchanged (`{2'd0, px_tok_q}` in, `[13:0]` out) unless the
   TAGW=14 wrapper contract was explicitly documented in Packet 4 — if it was
   not, keep 16.
2. If RCP approved: change `u_rcp` to `zhao_raster_rcp24_v3` at the approved
   NCTX/TOKW, mapping diagnostics BY MEANING per §7.3's dictionary
   (accepted/completed preserved; `mul_busy` has no exact equivalent — 
   document the changed definition, do not tie it to zero and call it
   preserved; occupancy is wider — widen the port, do not truncate).
3. `design/fit_targets.yml`: swap the closure entries accordingly
   (`zhao_raster_perspuv_svc.sv` → `zhao_raster_perspuv_pairpipe.sv`, etc.).
   The svc/V3 files that leave the closure stay in the tree — they are the
   verification references (§8.2: "beside the frozen service").

**Closure hazard.** Island closure again.

**Acceptance in simulation.** Full composed suites in both shadow configs,
plus the paired-record gate 3 workload — byte-identical records against the
oracle. The RCP swap, if taken, must pass identity-based comparison (completion
order may legally differ; the owner restores order — §7.3; a test that assumes
order would report false mismatches).

**Fit gate.** **FIT GATE 5**: MapOnly `@c-map` (DSP shape: 17→14 is a
HYPOTHESIS to check, not arithmetic to assume — §7.4), then one full island
fit, production config, labelled `@checkpoint-c`. If both candidates went in
together, label it combined and do not fabricate per-candidate contributions;
retain the snapshots needed for a controlled split if it regresses (§12.3).

**Rollback.** Instantiation swaps are single-commit reverts; the old modules
never left the tree. If checkpoint C regresses, §13.2 governs: go back to the
last measured smaller correct specimen and determine which representation
failed — do not ask for a bigger budget.

---

## DEFERRED — D6 owner locality

Per §9's own DELIVERABLE clause: **if owner control is not a top remaining
contributor after D1–D5, defer.** Decide from FIT GATE 2 and FIT GATE 5
resource attribution (resource-by-entity, not leaf-row folklore). If it IS a
top contributor, the bounded candidate is §9.2's local-row ISSUE form,
representation-only, with the full adversarial owner suite preserved — but
that is a new roadmap entry written against the attribution evidence, not
this one.

---

## THE FIT GATES — every fit this roadmap spends

| # | label | what changed since the previous gate | the question it answers | what failure means |
|---|-------|--------------------------------------|--------------------------|--------------------|
| 0 | `@d0fixed` (ALREADY RUNNING — costs nothing more) | D0 repair only, vs `@pktC-fixed` | the anchored cost/timing of the CORRECT circuit with the laboratory still aboard | n/a — whatever it reads is the baseline; if `rtlCleanAtHead` is false, refit before Packet 1 |
| 1 | `@d1-production` (island, MIGRATION_SHADOWS=0) | shadow gating + `class_m` deletion; no functional change | what the migration laboratory costs (vs gate 0, same functional source — §4.2's controlled pair), and the production baseline for all later deltas | if the delta is negligible, the laboratory was not the regression's cause — record it and proceed anyway (the baseline is still needed); if production config is functionally different, Packet 1's split is wrong — stop and fix |
| 2 | `@d23-production` (island) | early descriptor + UV join wired, 11 early arrays + palette sidecars + `class_m` gone, palette carried, D0d port | did replacing live tables with the synchronous bank make the machine smaller/not slower at the same feature contract (checkpoint B) | §13.2: if old payload was already pruned and the bank only adds cost, or a larger ready/enable family appeared — revert Packet 3's deletion commit (and Packet 2's if needed) per their rollback plans, keep the correctness parts, report |
| 3 | pairpipe + fresh svc LEAF PAIR (same commit, same profile NTOK=16/TAGW=16) | new candidate exists | is the paired PERSPUV materially smaller without timing/throughput loss, with the §8.3 tables provably absent | keep svc; strike Packet 6's PERSPUV half; optionally the §8.7 fallback experiment |
| 4 | *(conditional on owner)* rcp24_v3 + rcp24_svc LEAF PAIR at NCTX=12/TOKW=14 | nothing — this measures existing modules at a matched profile for the first time | the four-axis (ALM/reg/DSP/M10K) price of V3's real throughput profile vs svc at the same profile | the swap stays not-recommended; svc stays |
| 5 | `@checkpoint-c` (island, production config) | accepted candidates composed | did the area/timing frontier move with known components (checkpoint C) | §13.2 again; controlled split using retained snapshots; never argue from which component "looks prettier" |

Island-scale fits: 0 (already in flight), 1, 2, 5 — three NEW island fits for
the whole campaign. Leaf pairs: 3, and 4 only if the owner approves. Every
other physical question is answered by MapOnly, which is not rationed.

For every gate: read `rtlCleanAtHead` first; a labelled row's empty
`ruleViolations` is silence, so apply the island rules by hand and record the
historical-rule result separately from the product decision; bank the
worst-path census immediately (the setup reports are overwritten by the next
fit); publish the §12.3 checkpoint bundle (hashes, config, all corners
including hold, resource attribution, named RAM inference, workload counts).

---

## WHAT IS ALREADY TRUE — do not re-derive, do not re-litigate

* **D0 is repaired.** One gate in `zhao_texture_metajoin.sv`
  (`if (rd_valid_i && rd_legal_c)` around the read registers), commit
  `ccbb6d8c`. Evidence: `metajoin_seam_directed` 7/7 with the A-held/B-offered
  swap, the un-blinded generation counter firing on genuine staleness, and
  the illegal-read hold all checked; three of its checks are standing mutant
  catchers that go red if the gate is removed.
  (`reports/D0-REPAIRED-AND-PACKET-C-ACCOUNTED-20260908.md`)
* **PERSPUV's two schedulers cannot diverge.** Closed induction over the
  exhaustive four-assignment writer table in `zhao_raster_perspuv_svc.sv`;
  queue contents elementwise equal too. Licenses ONE scheduler; does NOT
  license merging the two arithmetic lanes (independent saturation per axis).
  (`reports/PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md`)
* **RCP V3 needs NCTX ≥ 12 to meet its own 4.6 clk/recip criterion; the
  island runs NCTX=8, where V3 measures 5.78 vs serial 6.96.** The recorded
  16-vs-8-context comparison was mismatched and its 8-M10K cost unquoted. The
  swap is unmeasured at any matched profile — not recommended, not refused.
  Two magnitude predictions about this were falsified in one afternoon;
  predict what moves, not how far.
  (`reports/RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE-20260908.md`,
  `reports/RCP-V3-THROUGHPUT-IS-NCTX-DEPENDENT-20260908.md`)
* **`@pktC` (15,911/73.98) is unquotable** — dirty tree AND defective circuit.
  `@pktC-fixed` (15,483 / 62.83 reported, 77.45 internal-only, clean)
  supersedes it. Packet C's honest account: −26 lines, +923 lines, +2,350 ALM,
  +1,658 registers, −19.58 MHz headline of which ~3/4 sits on a virtual pin
  that does not exist composed (−5.25 MHz internal-to-internal). No Fmax
  attribution to any single change is claimed or claimable.
  (`reports/PKTC-RECEIPT-CANNOT-BE-QUOTED-20260908.md`,
  `reports/D0-REPAIRED-AND-PACKET-C-ACCOUNTED-20260908.md`)
* **Labelled fit rows are never rule-checked** (0 of 26 labelled rows carry
  violations vs 12 of 92 unlabelled); 51 of 118 ledger rows have
  `rtlCleanAtHead: false`. Read the flag, then the numbers.
* **The new bank and join exist, tested, standalone**: `zhao_texture_early_desc.sv`
  (119-bit typed descriptor, 3×40-bit slices, D0 hold law, generation ON A
  PORT — D0d's answer built in), `zhao_texture_uv_join.sv` (atomic by one
  shared enable, reserved destination, Mosaic ready in the fork), with
  directed tests including stall/interleave sweeps and a fired mismatch
  counter. Registered in `tests/CMakeLists.txt:2213-2262`; NOT yet in any fit
  closure; NOT yet instantiated by the island.

### Findings made for this roadmap (new evidence, checked against source)

**VERIFIED AND EXTENDED 2026-09-08.** Every deletion claim below was re-checked
against source with comments stripped, and the ledger got bigger.

#### Three arrays are dead DUPLICATES, not merely superseded

`fpsl_m`, `fpgn_m` and `frec_m` each have exactly three non-comment mentions —
declaration, one write, one read into a `_c` alias — and those aliases
(`f_pal_slot_c` :896, `f_pal_gen_c` :897, `f_recipe_c` :899) have **no consumers
at all**. Deleting the alias makes the array write-only, so array and alias go
together.

The important part is *why* they are dead. The state they hold is kept a SECOND
time and that copy is the live one:

| dead FCTXN copy | live copy | live copy's reader |
|---|---|---|
| `fpsl_m` ← `frag_pal_slot_i` :791 | `palslot_m` :1269 | metajoin `.wr_pal_slot_i` :2707 |
| `fpgn_m` ← `frag_pal_gen_i` :792 | `palgen_m` :1270 | metajoin `.wr_pal_gen_i` :2708 |
| `frec_m` ← `frag_recipe_i` :794 | `mat_m` :2494 | the material path |

So the island stores the palette pair and the recipe twice — once keyed by
FCTXN slot and once keyed by owner slot — and reads only the owner-keyed copy.
The FCTXN set is an earlier attempt at CARRIAGE that was abandoned when the
sidecar won, and never removed. That is exactly §0's "partial implementations of
v3own's lifetime", and Packet 3 is the same idea done properly.

**Measurement consequence, and it matters for FIT GATE 2.** Deleting dead
duplicates and adding the descriptor bank in one packet makes the fit delta
uninterpretable — a shrink could be the deletion and a growth could be the bank,
with no way to tell. Delete the three dead arrays in **Packet 1** (which is
already a no-functional-change packet ending at a gate) and let Packet 2/3's
gate measure the bank alone. Same total work, one attributable number instead of
a confounded one.

#### Confirmed as previously stated

* `class_m` — two mentions, declaration :1239 and write :1268. Write-only.
* `uvw_m` — LIVE, keep. Real read at :851 (`px_uvw_q <= uvw_m[rcp_tok[13:8]]`),
  as §5.4 says.
* The Mosaic byte slice — verified correct, see Packet 2.
* The admission-agreement detector — struck, cannot fire, see Packet 2.


* **`class_m` is write-only.** Declaration `island_v3_top.sv:1239`, write
  :1268, zero readers (all other mentions are comments). Deletable in
  Packet 1 with the grep as the deletion receipt.
* **`f_pal_slot_c`, `f_pal_gen_c`, `f_recipe_c` are dead read aliases**
  (island :896-899) — no consumer anywhere. The brief's §5.1 suspicion about
  the early palette duplicates is confirmed; `fbase_m`'s only reader is the
  Mosaic byte feed; `fctx_rd`'s only reader is the expander.
* **The new `uv_join` does not forward palette identity** — no
  `d_palette_slot_i/gen_i` inputs, no `f_pal_*` outputs — and has no
  `m_u/m_v/m_src` ports for Mosaic. Packet 3 adds the palette ports; Packet 2
  wires Mosaic's u/v/src from the join's f-branch outputs of the same held
  record (correct by the one-enable construction; comment it).
* **Two admission write events coexist unchecked.** Early arrays load on
  `frag_valid_i && frag_ready_o` (:784); `palslot_m`/`class_m`/`mat_m` load on
  `own_adm_accept` (:1252, :2492). §5.2 requires the predicates to agree on
  the accepted beat; nothing verifies it today. Packet 2 adds the
  `err_adm_disagree_o` sticky.
* **`pu_sat`/`pu_dzero` are dropped on the floor today** — PERSPUV's
  saturation and depth-zero flags have no island consumer. The join's
  counters become their first home (Packet 2).
* **An island fit `@d0fixed` is in flight right now** (two `quartus_fit.exe`
  alive at time of writing), and the working tree carries four uncommitted
  test-file edits — Packet 0 exists because of both.

---

## OPEN OWNER DECISIONS — do not decide these unilaterally

1. **RCP V3 adoption and its profile** (Packet 5). NCTX=12-or-16 costs M10Ks
   and registers for throughput the composed workload may or may not need.
   Options and evidence are in the two RCP reports; the memo is Packet 5
   step 1.
2. **The island's resource allocation.** The historical redlines (7,500 ALM /
   9,000 reg / 14 DSP) stand in `fit_targets.yml` and every current specimen
   breaches them. §0: any revision needs count-once whole-console accounting
   and an explicit owner decision. Keep stamping the historical-rule result;
   never paint it green.
3. **Retiring the migration laboratory entirely** (deleting the lab config
   rather than gating it). D1 makes it elaboration-selected; whether the
   shadow apparatus is ever deleted from source is a separate call, after the
   new joins have history.
4. **TAGW 16 → 14 narrowing at the PERSPUV boundary** (§8.5): legal only as a
   deliberate wrapper contract. Default in this roadmap: keep 16.
5. **`DZ_FORCES_ZERO_SAMPLES`** (uv_join knob): whether a depth-zero fragment
   should force zero samples is policy; the default (0) preserves today's
   behaviour. Flag it when the flags first become observable (Packet 2).
6. **D6 owner locality**: only if post-D5 attribution names owner control a
   top contributor, and then as its own reviewed plan.
