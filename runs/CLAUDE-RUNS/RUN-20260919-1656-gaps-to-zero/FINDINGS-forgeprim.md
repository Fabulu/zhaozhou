<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
0d33c434 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- FORGEPRIM


## `0d33c434` -- FINDINGS-forgeprim.md (transcribe): the forge page kind FROZEN, four evaluators BUILT, composition REFUSED with a map

```
FINDINGS-forgeprim.md (transcribe): the forge page kind FROZEN, four evaluators BUILT, composition REFUSED with a map

Branch `gz/forgeprim`, three commits, head 3517d9dc. Register 22 -> 22.

> TRANSCRIBED BY THE COORDINATOR. The harness refuses a lane report file; this
> commit message IS the document. It is the sixth lane in a row and the sixth
> time it has been worth fixing at the harness.

---

## 1. THE PAGE KIND, AND WHAT IT NOW IS

`spec/cartridge.md` 4d. **Page kind 14, section type 0x0012, FORGE_PROGRAM.**
64-byte header (magic 'ZFPG', version, record count) and **192-byte records --
THREE whole MEM.GUARD lines.** Not 32 like sections 4b and 4c: a forge program
does not fit in 32 bytes and no packing will make it. The property that matters
is not "two per line", it is **no record ever straddles a read**, and every
multiple of 64 has it.

A record is ONE primitive, topology and positions together, because that is
what `DrawProcedural.program` names. Line 0 is identity, topology and the
common frame; line 1 the radii and the ribbon's jitter law; line 2 the
branches.

**Keyed by the handle32 INDEX and nothing else** -- R26's decision for the
ladder table and R45's for the stamp, taken a third time so nothing can
disagree.

**THE PAGE'S `family` GOVERNS AND THE COMMAND'S `kind` MUST AGREE**, refusing
the draw and counting it otherwise. A ribbon's parameters and a tube's are not
the same parameters, so a reader cannot interpret a record without knowing its
family -- the family belongs in the page. The command declares it a second time
and the two are authored in DIFFERENT PLACES (page by the packer, command by
the game's draw), so comparing them is a real check and not two operands
moving together.

**THE ROTATION IS DISCHARGED.** `spec/commands.zidl` warns in capitals that
`forge_kind = (family + 1) mod 6` and "a straight-through assignment is
silently wrong for all six values". `zref::forge_page::kind_of_family` /
`family_of_kind` are the ONE place it is written; the test walks all six BOTH
ways against the zidl's own hand-written table and asserts the rotation agrees
with the identity on NO family -- so a future straight-through edit goes red.

**TWO DECLARED HOLES**, named so they are not read as oversights:
  * a ribbon's two radii MUST be equal, because `zhao_forge_prim_eval` carries
    ONE half_width. The page refuses a tapering ribbon rather than carrying a
    number nothing reads; when one is built the refusal lifts and no byte moves.
  * `tick_phase_base` is a BASE. A cartridge page is immutable and a bolt
    animates per frame, so the live phase is base + frame_tick sourced at
    DISPATCH. **That is the one OWNER DECISION this packet opens** -- see 5.

THREE STATEMENTS, ONE ARTEFACT: the spec section,
`reference/include/zref/zref_forge_page.hpp` and
`tools/pack/mkforgeprogram.py`, all pinned to
`tests/golden/forge_program/forge_page_v1.bin` (1,216 bytes, 6 records, one per
family, both sweeps, the 64x8 worst case, a two-branch bolt). `--check` and
`forge_page_directed` approach it from DIFFERENT SIDES deliberately: one test
building both halves from one source would be a comparator wired to two
operands that move together.

## 2. THE EVALUATORS

`fpga/rtl/forge/zhao_forge_ring_eval.sv` -- **fan, tube, radial shell and
billboard sheet**, exactly the four `spec/commands.zidl` names as having none.
Reference `zref::forge_ring::eval_job`.

**The fifth, cliff, is FORGE.CLIFF's and was never owed here** (its positions
come from the terrain lattice, not a parameter block). So 5 of 6 families now
have a position law; the sixth has one in a different subsystem.

ONE SWEPT RING: `P(s,k) = C(s) + R(s)*W(k)` over the same (rings x ring
vertices) grid `zhao_forge_prim` walks -- N and K are that block's own
`eff_seg_c` and `ring_q` RESTATED, not a second topology law. Two sweeps,
LINEAR and DOME, DOME legal on the SHELL alone so `sweep` cannot become a
silent second family selector.

An OPEN family's ring is the width-axis PAIR, C-R*U then C+R*U -- **exactly the
pair and the order the ribbon evaluator emits**, asserted against a
hand-computed expectation rather than against the thing under test.

ARITHMETIC BORROWED, NOT INVENTED: the lerp is FORGE.PRIM.EVAL's exact rational
CALLED from the reference; the ring angle is that same function (so a 3-, 5-,
6- or 7-sided ring is uniform to the ulp, and a ring closes by INDEX because a
whole turn is the WIDTH of angle16); sine and cosine are `zhao_field_sin`
INSTANTIATED, which is FORGE.PRIM.md's own "no new trigonometry is introduced";
one operand-muxed 33x33 multiplier, one bit-serial divider at zero DSP, fused
single-rounding displacement, saturate-and-count everywhere.

**NO `walk_overrun` COUNTER, AND THAT IS A DECISION.** Its sibling has one and
needs a committed mutant because no legal stimulus reaches it. Rather than ship
a second unfireable counter, the whole-job property is asserted DIRECTLY and
positively: an accepted job emits EXACTLY (N+1)*K vertices and a refused one
ZERO, on every vector under every stall pattern. **Every counter this block does
export is fired by legal stimulus**, saturation included -- none is asserted
zero and left unexercised.

**AND THE REFUSALS DISCRIMINATE (R95).** Family 6 is a caller who has not read
the ruling; family 0 is a DISPATCH that sent the job to the wrong evaluator.
Different ports, and the test asserts that a ribbon job leaves
`refused_family_o` PUT.

## 3. THE COST, MEASURED -- AND IT CORRECTED MY OWN HEADER

R236: a cost is a fact to RECORD. So it was taken, not argued, and the test
prints it on every run (always-ready consumer, 520 vertices each):

    LINEAR  64 x 8 tube   18,145 clocks   1.09% of computeClocksPerFrame
    DOME    64 x 8 shell  13,140 clocks   0.79%

**THE DOME IS THE CHEAPER ONE, which is the opposite of what the header said
before the number was taken.** The comfortable reasoning arrives first and is
wrong: a dome does a quarter-wave lookup AS WELL AS a sweep, so surely it costs
more -- but DOME replaces THREE 43-iteration divides per ring with three
two-clock products and one trig pair, so it does strictly less work. Anyone
optimising this block from the old header would have worked on the wrong case.
Corrected in place with the reason beside it, and the DIRECTION is now pinned by
a check (`dome_worst < linear_worst`), because the direction is what was wrong.

Area is UNMEASURED and says so: `design/fit_targets.yml` gains a
`- top: zhao_forge_ring_eval` row with four named questions. **No Quartus was
run.**

## 4. COMPOSITION: REFUSED, WITH A MAP. SIX BLOCKERS, EVERY ONE READ FROM A PORT MAP

Recorded in `design/contracts/FORGE.PRIM.EVAL.md`'s new closing section and in
`zhao_console_core.sv`'s FORGE entry. `upstream:` was not consulted -- R180.

1. **`zhao_cmd_exec` HAS NO ARM FOR 0x0302.** Zero hits for the opcode anywhere
   in `fpga/rtl/command/`; the one `forge` hit there is prose. The record falls
   into the catch-all and increments `unsupported_o`. `zhao_geom_drawjob`
   decodes DrawForm/DrawPosedForm only and emits a MESH job.
2. **No bank stages a forge page.** The format is no longer the gap; the
   STAGING PATH is. Design already scoped for whoever takes it: one resident
   record, linear scan of record line 0 for the index (bounded at N+2 reads),
   on the `zhao_terrain_hdrread` pattern rather than `zhao_geom_ladderbank`'s,
   because 16 x 192 bytes of register file is ~15,000 flops and one resident
   record is ~1,000. **NOT BUILT HERE DELIBERATELY**: its request side depends
   on the dispatch that does not exist, and guessing it would be R199's own
   objection reappearing one layer down.
3. **`zhao_geom_setup` has exactly ONE triangle arm**, `signed [20:0]` screen
   subpixels, driven port for port by `zhao_geom_clip`, with **no arbiter**.
   The ledger's three `inputs:` are design intent, not a port count.
4. **That arm is a TINE of a three-way ordered join** with `zhao_geom_attrpack`
   and `zhao_material_window`, pairing by ARRIVAL ORDER with no tag. Entering at
   one tine alone **deadlocks combinationally**. The door is at **GEOM.CLIP's
   INPUT**, which yields winding normalisation, 2A, the bounding box and the
   zero-area reject for free and keeps all three tines in step.
5. **`zhao_project_service` has TWO client arms and BOTH ARE TAKEN** -- A by
   `zhao_part_project` (already time-multiplexing geometry and particles
   through it), B by TERRAIN.GROUP_SEQ. The world->screen arithmetic is exactly
   the right shape and there is **no free arm**. SHADOWSUB's warning applied
   and it held: check for a free ARM, not a matching port list.
6. **THE BINDING ONE: `GEOM_CLIP_ATTRS = 7`.** A forge ribbon has no ratified
   invw24, u/w, v/w, lit r/g/b or alpha. **This is the same wall TERRAIN and
   PARTICLES each hit independently** -- `zhao_part_expand` is composed, emits
   screen triangles, and sits at boundary I24 for exactly this reason. Whoever
   settles it settles three subsystems at once, which is the argument for a
   SUBSYSTEM packet rather than forge wiring.

**SHADOWSUB's other finding also held here.** `zhao_geom_drawjob` emits no form
index and no view index, and nothing it emits could carry a family, a
subdivision or an anchor -- verified by reading its port list, not inferred.

**FORGE.SHADOW was not touched.**

## 5. THE ONE OWNER DECISION THIS PACKET OPENS

**Where does a live `tick_phase` come from?** A cartridge page is immutable, so
the ribbon's animating phase cannot come from it. 4d freezes `tick_phase_base`
and rules that the evaluator's phase is `base + frame_tick`, `frame_tick`
sourced at DISPATCH. The two candidates are reinterpreting `DrawProcedural`'s
`pad[11]` (the same mandatory-zero precedent `forge_kind` itself used) or a
console frame-sequence broadcast. **It is an ABI question, so it is escalated
rather than decided** -- and NOTHING IS BLOCKED ON IT: with `frame_tick == 0` a
page alone is a complete, deterministic, static primitive.

**`spec/commands.zidl` WAS NOT TOUCHED.** No opcode was taken, so 0x0304 and
0x0305 are untouched and `abi:check` was not required.

## 6. FOUR SMALLER FINDINGS

* **"F-EVAL1" IS A HALF-PHANTOM, AND I ALMOST REPORTED IT AS A WHOLE ONE.**
  `design/prod_manifest.yml` says twice that the gate is "named in
  design/fit_targets.yml"; the string F-EVAL1 appears **nowhere** in that file,
  and `reports/FORGE-PRIM-EVAL-IMPLEMENTATION-20260909.md` cites it four times.
  **But the gate ENTRY is real** -- the `- top: zhao_forge_prim_eval` row, with
  its question written out. Only the LABEL is missing. R225's rule ("check one
  case by hand before believing any total") is what caught the difference
  between a phantom citation and a missing label, and the two would have sent
  the next reader to very different work. Recorded at both rows; not "fixed" by
  inventing a label nothing reads.
* **`spec/cartridge.md` 5's packing ORDER is stale for SEVEN kinds, and six of
  them predate me.** It names eleven section types; the registry holds
  eighteen. CREATURE_FORM, CLIP_BANK, TEXTURE_PAGE, MATERIAL_SET, MESH_STREAM
  and SPECIES_TABLE were already absent. I did not insert FORGE_PROGRAM:
  choosing a position is a ruling about pack determinism, `tools/pack` has no
  forge writer yet, and picking one would freeze a second thing nobody asked
  for. Recorded in 4d; whoever writes the forge packer extends 5 for all seven.
* **A 16-BIT LITERAL HOLDING A SEVENTEEN-BIT VALUE**, caught in my own draft
  before it shipped: `16'h1_0000` elaborates to ZERO, so every ring angle would
  have come out 0 -- **a perfectly uniform, perfectly wrong ring** that no
  vertex-count or handshake check could see. Both turn constants are plain
  integers now, with the reason beside them.
* **COMMIT f8f33566 REGISTERS A TEST FILE THAT COMMIT 6a2c5c98 ADDS.** Both
  CMake registrations went in with the page commit. The BRANCH HEAD is
  consistent and the coordinator merges branches, not commits, but a bisect
  landing on f8f33566 would fail to configure. Stated rather than rebased
  (the protocol forbids rebasing).

## 7. TIE-OFFS: NONE. Nothing was removed, narrowed, stubbed or disconnected.

No port was tied off, so nothing is owed to the core's `INCOMPLETE -- TIED OFF,
AND WHY` block (R159). No capability was narrowed. No older version was
composed. `zhao_forge_ring_eval` takes **no `design/blocks.yml` row**, on
purpose: FORGE.PRIM.EVAL is the capability and this extends that contract's
coverage. R214's test for a legitimate row is a capability with a written
contract and NO row; this is not that, and adding one would be manufacturing a
register rise.

## 8. GATES AT 3517d9dc, AND WHY THIS SET

All static gates RC 0: console_inventory (363 modules, OK -- it is what proves
the new file has a declared disposition), prod_manifest (363/73/122/168, OK),
quartus17_syntax (569 files, 13 fire / 22 no-fire self-test), case_labels,
mutant_copy_drift **run AFTER the commit** (R121), wrapper_port_parity
**1270 = 1270**, mutant_drivers, uncashed_cheques, check_counters,
refmodel_liveness, duplicate_functions, check_findings_citations (**no NEW
dangling citations** -- it reads YAML as well as RTL, which is why the two
`FINDINGS-forgeprim.md` references I had written into the ledgers were replaced
with the contract section before committing), packet_h_tieoff_audit (unchanged),
completion_register **22**.

All three generators FRESH: `gen_prod_top --check` (73 instances),
`gen_console_board --check` (1285 core ports), `gen_shell_paired_diff --check`
AND `--check --mutant`.

Verilator `--lint-only -Wall` on the new module: **0 diagnostics** -- and it is
quoted as one tool's opinion, not as synthesizability. The elaboration guards
sit inside `initial begin` and the FSM declares no module-scope loop variable,
because FORM 7 is aimed at code exactly like this.

**Directed tests BUILT AND RUN (R60), not merely linted:**
  * `forge_page_directed` -- **82 checks**, RC 0
  * `forge_ring_eval_directed` -- **307 checks**, RC 0
  * `mkforgeprogram.py --check` -- 1,216 bytes, matches the golden

**SMOKE: ONE FORM, and the choice is defended.** I ran the PLAIN form: PASS,
`raster pixels=2560`, `frames_admitted=1`, RC 0. I did not run the other nine.
R227 says a gate earns its runtime by being able to change its answer, and the
only console-core change in this branch is a COMMENT BLOCK -- comments do not
simulate. The one question a comment edit CAN change is "does the core still
verilate", and the plain form answers exactly that. No port changed, so no
wrapper mutant, board or prod-top regeneration was needed, and all three
`--check` generators confirm it.

**THE COUNTER CHECK THAT EARNED ITS PLACE.** The cumulative counter comparison
was the ONLY one of 297 to fail on the first run -- by exactly **130 rings and
770 vertices**, which is exactly the five determinism jobs and the one
comparator job the TEST had submitted to hardware and forgotten to count. The
RTL was right. An off-by-a-whole-job is what that check exists for, and the
agreement is only evidence BECAUSE it is exact.

## 9. FOR THE COORDINATOR -- what remains, and for whom

1. **A FORGE DISPATCH PACKET**: a CMD.EXEC arm for 0x0302 and a page reader
   (design scoped in 4 above). Owes the kind<->family rotation a directed check;
   `zref::forge_page::kind_of_family` is the only place to call.
2. **THE SEVEN-SLOT ATTRIBUTE LAW** -- the binding blocker, and it is NOT
   forge's. It blocks forge, `zhao_part_expand` (boundary I24) and terrain
   alike. **This is a subsystem packet and the highest-leverage one on the
   queue**, because it is the only item here that closes a register entry.
3. **THE `tick_phase` ABI QUESTION** for the owner (5 above).
4. **F-EVAL1's missing label** and 5's stale packing order -- both documented
   in place, both cheap, neither urgent.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
