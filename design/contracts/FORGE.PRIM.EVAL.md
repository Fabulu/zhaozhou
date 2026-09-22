# Contract — FORGE.PRIM.EVAL (Procedural primitive position evaluator)

> Ledger: `design/blocks.yml` · owner ZH-044 · phase 11 · maturity UNIT_VERIFIED
>
> Written 2026-09-09 alongside the first RTL. No prior contract existed for
> this block — `reports/ADDLIGHTNING.md` (owner, 2026-09-04) is its charter,
> and this document makes that charter's law exact. Where FORGE.PRIM.md and
> this file speak about the same seam, FORGE.PRIM.md's topology statements
> govern topology and this file governs positions.

## Purpose and exclusions

`zhao_forge_prim` owns topology only. This block owns the POSITIONS for the
ribbon family — the owner's lightning law, verbatim from ADDLIGHTNING.md:

    P0 = start
    PN = end
    Pi = lerp(start, end, i/N)
       + perpendicular_1 x jitter(seed,   tick_phase, i)
       + perpendicular_2 x jitter(seed^2, tick_phase, i)

bounded exactly as the owner bounded it: **one deterministic ribbon with at
most 24 segments and at most two bounded branches (each ≤ 8 segments)** — the
explicit refusal of "any number of branching antialiased electrical lines
with arbitrary widths".

**Exclusions, deliberate:** the other five prim families' position laws
(tube/shell rings from SIN_Q16, fan, billboard, cliff) are NOT here yet.
Lightning needs none of them — the far/tiny LOD rungs are PART.SOFT and
glint, not Forge — and growing five laws speculatively is how a block misses
its one required function. When a family's positions are needed, they extend
this contract by ratification, not by drift.

> **RATIFIED 2026-09-21 — OWNER DECISION R234 D2, `(owner, explicit)`.** The
> paragraph above is kept exactly as written because it was right, and because
> the sentence it ends with is the one that just fired: *"when a family's
> positions are needed, they extend this contract by ratification, not by
> drift."* **This is that ratification.** R199 deferred the forge page kind on
> the grounds that *"four of the six forge families have no evaluator at all …
> a page ruling buys one of six"*; D2 reverses it because *"the owner has
> chosen to pay for the evaluators rather than accept the deferral."*
>
> **Four of the five are now built**, as a SECOND MODULE and not as growth
> inside this one: `fpga/rtl/forge/zhao_forge_ring_eval.sv`, reference
> `zref::forge_ring::eval_job`
> (`reference/include/zref/zref_forge_ring.hpp`), test
> `tests/forge/forge_ring_eval_directed.cpp`. It serves **fan, tube, radial
> shell and billboard sheet** — exactly the four `spec/commands.zidl` names as
> having no evaluator — and it **refuses the ribbon and the cliff on their own
> counter**, because this block owns the ribbon and FORGE.CLIFF owns the cliff.
>
> **Two modules rather than one, deliberately.** The lightning law is a jitter
> law with branches and a hash stream; the ring law is a swept ring with a
> quarter-wave table. Folding them together would put two unrelated laws behind
> one FSM and force every ribbon job to carry ring state it never reads. They
> share the ONE thing they genuinely share — `eval_lerp_off`, the exact
> rational `floor((2Di + N)/(2N))` — by the ring reference **calling** this
> one's, never by copying it.
>
> **They agree about the OPEN ring without either importing the other.** A
> ribbon pair is `(P − wvec)` then `(P + wvec)`; a billboard's open ring is
> `C − R·U` then `C + R·U`, in that order. Same two vertices, same order, and
> the ring test asserts it against a hand-computed expectation.
>
> **The fifth, cliff, is NOT covered and is not owed here.** Its positions come
> from the terrain lattice and belong to FORGE.CLIFF, whose own adoption landed
> under R142. Re-deriving them from a parameter block would be the second
> implementation of ratified arithmetic this file already refuses.
>
> **What the pair still lacks is a DISPATCH, not a law.** See "Notes" below.

## Clock and reset semantics

Single `gpu_clk`, asynchronous active-low `rst_n` (house pattern of
`zhao_forge_prim`). Reset abandons the job in flight; nothing partial is
retried or resumed.

## Input and output packet layouts

### Job in (the params block — every knob NAMED, owner-editable)

    { start[3], end[3],                 fx16 anchors
      perp1[3], perp2[3],               fx16 jitter axes (caller-normalised)
      waxis[3],                         fx16 ribbon width axis
      half_width, branch_half_width,    fx16
      amp, branch_amp,                  fx16 jitter amplitudes
      seed[32], tick_phase[16],
      segments (1..24),
      branch_count (0..2),
      per branch: attach (0..segments), segments (1..8), end[3],
      view_mask[2], src_id[16] }

The perpendiculars and width axis are SUPPLIED, not derived: deriving a unit
frame in hardware needs a square root and a divider this block has no
business owning, and the CPU computes the frame once per bolt anyway.

Caps are parameters: `MAX_MAIN_SEGMENTS` (24), `MAX_BRANCHES` (2),
`MAX_BRANCH_SEGMENTS` (8). Elaboration refuses caps that break the divider
width or exceed FORGE.PRIM's frozen 64 (checks live inside `initial begin` —
Quartus 17 law).

### Out

A position stream `{x, y, z (fx16), poly[2], last, src_id}`, ready/valid, in
THE declared deterministic order: polylines main → branch0 → branch1; points
`i = 0..N` per polyline; per point the ribbon pair `(P − wvec)` then
`(P + wvec)`. That is ring-major `vidx(s, k, ring=2) = 2s + k` — exactly the
walk `zhao_forge_prim`'s ribbon indices reference, so one eval job pairs with
one prim ribbon job per polyline (`segments = N, sides = 1`). Worst legal
job: 86 vertices → 84 triangles, inside the owner's "roughly 64–128
triangles" envelope.

### Branches grow from the JITTERED bolt

A branch's start is the main polyline's centre point at its attach index —
captured in flight, because the caller cannot know a jittered position. Its
hash streams are re-seeded (`seed ^ BR_SALT[b]`, `seed² ^ BR_SALT[b]`) so a
branch is not a phase-shifted copy of the main bolt.

## The exact arithmetic (determinism is the whole point)

* **Lerp** — `off_c = rhu(D_c·i / N)` computed exactly as
  `floor((2·D_c·i + N) / (2N))`: an exact rational via one bit-serial
  restoring divide (40 cycles, zero DSP), numerator maintained by exact
  integer accumulation (`+= 2D` per point). `off_0 = 0` and `off_N = D` hold
  identically — P0 = start and PN = end are arithmetic facts, and the
  endpoints are additionally emitted by assignment with jitter masked off.
* **Jitter** — two xorshift32 streams seeded from `(seed, tick_phase)` and
  `(seed² lsalted, tick_phase)`; they advance ONCE PER POINT in the walk FSM,
  never per clock, so no stall pattern can reach them. The low 8 bits index
  JITTER_Q16 (`zhao_forge_jitter_rom`, generated by
  `tools/forge/gen_jitter_rom.py`, 256 × 18 bit = 4,608 bits in one M10K of
  the 553 owned / ~147 used). `seed²` is the owner's literal law: low 32 bits
  of `seed*seed`, salted so seeds 0 and 1 still decorrelate.
* **Scale and sum** — `jA = fx_mul(amp, T)`; `disp_c` is the FUSED
  `rescale(perp1_c·jA + perp2_c·jB, 16)` — exact 66-bit sum, one rounding,
  the qformats single-rounding law. `P_c = sat(S_c + off_c + disp_c)`, one
  saturation, counted.
* **All products through ONE operand-muxed 33×33 multiplier** with a
  registered product (the `zhao_terrain_normals` mseq pattern). There is no
  second multiplier site in the block.

## Q formats and rounding

Positions fx16 S15.16 (spec/qformats.md). Rescale is qformats §4:
`(x + 2^15) >>> 16`, **round-half-up**. NOTE: FORGE.PRIM.md says "round-half
away from zero"; qformats is the machine-wide law and SIN_Q16 consumers
already round half-up, so half-up governs here — discrepancy recorded in
`reports/FORGE-PRIM-EVAL-IMPLEMENTATION-20260909.md` for the owner to settle.
Every overflow saturates and increments `sat_events`; nothing wraps.

## Backpressure rules

Ready/valid on both faces. `j_ready_o` never depends on `j_valid_i`. A stall
mid-job holds the walk's cursor; emission registers are stable for the whole
stall, so the presented vertex cannot change under a waiting consumer.

## Memory ownership

**One M10K** (the jitter table) and nothing else. Parameters arrive in the
job; positions are generated into the stream and buffered nowhere — the same
property that makes FORGE.PRIM cheap.

## Latency and throughput

Variable, bounded: ~150 clocks per interior point (divider-dominated), ~6 per
endpoint. Worst legal bolt (24 + 8 + 8 segments): **~6,300 clocks = 0.38 % of
`computeClocksPerFrame` (1,666,666)**. Sixteen simultaneous worst-case bolts:
~6 %. Rate is not the constraint and buying it with parallel multipliers
would spend exactly the resource (DSP) the console is rescuing.

## Overflow and malformed-input behaviour

| condition | behaviour |
|---|---|
| `segments` 0 or > 24 | **refuse** before emitting anything, count |
| `branch_count` > 2 | refuse, count |
| active branch `segments` 0 or > 8 | refuse, count |
| active branch `attach` > `segments` | refuse, count |
| inactive branch fields | **not inspected** (caller may leave them stale) |
| job outside `view_mask` | **skip**, count separately — a well-formed job for another view is not a caller error |
| `start == end` | **LEGAL** — D = 0 is a working domain point (a zero-length main bolt with live branches is a meaningful effect). Prim's zero-length refusal guards zero-area *triangles*; that check stays at the topology/dispatch level |
| fx16 overflow in jitter/width/sum | saturate, count `sat_events` |

Refused means NOTHING emitted — never a clamped job that ships a quietly
wrong bolt.

## Counters and traces

`primeval_jobs, primeval_points, primeval_vertices, primeval_refused_limit,
primeval_skipped_view, primeval_sat_events, primeval_walk_overrun` (ports
`jobs_o … walk_overrun_o`, mapping in the ledger). Every one is SEEN TO MOVE
by `tests/forge/forge_prim_eval_directed.cpp` except `walk_overrun`, which is
unreachable by legal stimulus while the walk is correct — its positive
control is the committed mutant
`tests/mutants/zhao_forge_prim_eval_mutant.sv` +
`tests/forge/forge_prim_eval_overrun_control.cpp` (passes when the counter
FIRES).

## Scalar reference function

`zref::forge::eval_job` (`reference/include/zref/zref_forge_eval.hpp`),
sharing the generated jitter table
(`reference/include/zref/generated/zref_forge_jitter_table.hpp`). The formula
lives once, in `tools/forge/gen_jitter_rom.py`;
`tests/forge/forge_jitter_rom_directed.cpp` holds the two emissions equal
entry by entry.

## Directed tests

`tests/forge/forge_prim_eval_directed.cpp` — WRITTEN, 967 checks: bit-exact
against the oracle (directed + 150-job randomized full-domain sweep including
`start == end` and extreme-saturation params); byte-identical streams under
four stall patterns and reruns; tick animates the interior while anchors
hold; every cap refused at its boundary with nothing emitted and the legal
boundary accepted; view-skip; seed corners 0/1/0xFFFFFFFF; counters equal to
the oracle's counts cumulatively; the comparator SEEN TO FAIL on a planted
one-bit corruption. `tests/forge/forge_jitter_rom_directed.cpp` — WRITTEN,
512 checks. `tests/forge/forge_prim_eval_overrun_control.cpp` — WRITTEN,
inverse polarity, fired.

## Formal properties

Planned, not written: whole-job emission count (an accepted job emits exactly
`Σ 2(N_p+1)` vertices, a refused job zero) and handshake hygiene, the
FORGE.PRIM `never a partial primitive` law restated for positions.

## Synthesis / resource ceiling

**Shares FORGE.PRIM's contract ceiling: prim + eval together ≤ 2,800 ALM,
10 DSP, ≤ 2 M10K.** Structural prediction for eval alone: one 33×33
multiplier site (2–4 DSP with the output register in the DSP block), one
M10K, ~1,000–1,800 ALM (≈900 flops of latched params + walk + divider).
UNMEASURED until fit gate **F-EVAL1** (named in `design/fit_targets.yml`)
runs — batched with the next forge-subsystem fit per the 2026-09-08 batching
law. The gate's question: multiplier in DSP with its output register used,
table inferred as M10K, pair inside the ceiling.

## Integration capture cases

* one near-LOD bolt per view, seeds differing, capture-CRC across two runs;
* a frame of 16 worst-case bolts (the 6 %-of-frame composition case);
* a refused bolt in a frame that completes — the refusal attributable to its
  command;
* the FX.LIGHTNING seam itself once it exists: eval stream + prim indices
  through GEOM.SETUP, additive material, glow tag (the ADDLIGHTNING
  composition).

## Notes

The FX.LIGHTNING effects-level contract (start/end anchors, tick, seed, LOD
ladder near-ribbon → mid-ribbon → PART.SOFT streak → glint, ≤ 2 branches,
1–3 spell-light samples) is SKETCHED in reports/ADDLIGHTNING.md and not yet
ratified; it is dispatch/composition, not a seventh Forge family, and it is
the remaining work between this block and a bolt on screen.

## What the evaluator pair still lacks — measured 2026-09-21, not inherited

Both position laws now exist and neither is composed. The remaining work is
**dispatch and a door**, and it is named here so the next packet inherits a map
rather than an argument. Each item was verified by reading the port map, not the
ledger — `upstream:` in `design/blocks.yml` is DESIGN INTENT and not a wiring
claim (R180).

1. **A FORGE DISPATCH.** `spec/commands.zidl`'s `DrawProcedural 0x0302` is
   ratified and **`fpga/rtl/command/zhao_cmd_exec.sv` has no arm for it** —
   zero hits for the opcode anywhere in `fpga/rtl/command/`. The record falls
   into the catch-all and increments `unsupported_o`. `zhao_geom_drawjob`
   decodes DrawForm/DrawPosedForm only and emits a MESH job (`j_format_o`
   checked against the mesh descriptor format); nothing it emits could carry a
   family, a subdivision or an anchor.
2. **A PAGE READER.** The page format itself is no longer missing — owner
   decision R234 D2 froze it as `spec/cartridge.md` §4d, kind 14, model
   `zref::forge_page`, packer `tools/pack/mkforgeprogram.py`, golden
   `tests/golden/forge_program/forge_page_v1.bin`. **What is missing is the
   staging path**, on the terrain pattern: `zhao_terrain_pageloader` moves a
   body from HPS DDR into a pool and `zhao_terrain_hdrread` turns a header into
   registers. A forge bank copies that shape. `zhao_geom_ladderbank` is the
   closer precedent for the TABLE half.
3. **THE ROTATION THAT CONVERSION OWES.** `DrawProcedural.kind` is `forge_kind`
   and the job field is `j_family_i`; they are two numberings of the same six
   and `forge_kind = (family + 1) mod 6`. `zref::forge_page::kind_of_family` /
   `family_of_kind` are the one place it is written and
   `tests/forge/forge_page_directed.cpp` walks all six both ways.
4. **THE DOOR, and it is at GEOM.CLIP's INPUT rather than GEOM.SETUP's.**
   `zhao_geom_setup` has **exactly one** triangle arm, `signed [20:0]` SCREEN
   subpixels with edge functions, and it is a tine of a three-way ordered join
   with `zhao_geom_attrpack` and `zhao_material_window` that pairs by ARRIVAL
   ORDER with no tag. `zhao_geom_clip` drives it port for port and there is no
   arbiter. Entering at one tine alone deadlocks combinationally. Entering at
   GEOM.CLIP's input instead yields winding normalisation, `2A`, the bounding
   box and the zero-area reject for free and keeps all three tines in step.
5. **A WORLD → SCREEN HOP.** Both evaluators emit `signed [31:0]` WORLD fx16.
   `zhao_project_service` performs exactly that transform to `signed [20:0]`
   screen — and has **exactly two client arms, both taken**: A by
   `zhao_part_project` (itself already time-multiplexing geometry and
   particles through it) and B by TERRAIN.GROUP_SEQ. There is no free arm; a
   forge requester needs a third client or a second guest on A's pattern.
   **Check for a free ARM, not for a matching port list.**
6. **THE ATTRIBUTE LAW, and it is the binding one.** `GEOM_CLIP_ATTRS = 7` per
   corner — invw24, u/w, v/w, lit r, g, b, alpha. A forge ribbon has no
   ratified value for those seven, and **this is the same wall TERRAIN and
   PARTICLES each hit independently**; `zhao_part_expand` is composed, emits
   `signed [21:0]` screen triangles, and leaves the core as boundary **I24**
   for exactly this reason. Whoever settles it settles it for three subsystems
   at once, which is the argument for doing it as a subsystem packet rather
   than as forge wiring.

**A live `tick_phase` is the one OWNER question in the list.** A cartridge page
is immutable, so the ribbon's animating phase cannot come from it: §4d freezes
`tick_phase_base` and says the evaluator's phase is `base + frame_tick` with
`frame_tick` sourced at dispatch. Whether that arrives by reinterpreting
`DrawProcedural`'s `pad[11]` (the precedent `forge_kind` itself used) or by a
console frame-sequence broadcast is an ABI decision and is left open
deliberately. With `frame_tick == 0` the page alone is complete and
deterministic, so nothing is blocked on the answer.


## COMPOSED 2026-09-21 (packet FORGECOMP) -- what happened to the six blockers

**FORGE.PRIM and FORGE.PRIM.EVAL are CONNECTED.** The completion register went
17 -> 14 and the three modules that moved are `zhao_forge_prim`,
`zhao_forge_prim_eval` and `zhao_geom_clipdoor`. No tie-off was created,
narrowed or relocated.

**THE BLOCKER THAT MATTERED WAS NOT ON THE LIST.** Six were recorded above and
five passes before this one re-verified them. The thing actually missing was a
**VERTEX STORE**: this block emits WORLD POSITIONS and `zhao_forge_prim` emits
INDEX TRIPLES, joined only by the ring-major ORDERING CONVENTION that
`zref_forge_page.hpp` names, and nothing in the tree held the vertices an index
could look up. `zhao_forge_assemble` is that store. Once it exists the rest is
wiring -- which is the reverse of what the list implied, and worth saying
plainly because the list was honest and still sent five packets to the wrong
question.

Blocker by blocker, at the composed head:

1. **CMD.EXEC's missing arm for 0x0302 -- CLOSED.** `zhao_cmd_exec` gained it,
   built to the TerrainField arm's shape. Owner ruling R241 D-TICK-A is
   executed: `frame_tick` is two bytes of `pad[11]`, little-endian, and the
   live phase is `tick_phase_base + frame_tick` with a WRAPPING add, because a
   phase is an angle16 whose whole turn is the WIDTH of the field.
   **`screen_error` is deliberately NOT decoded** and the arm says why: the
   page authors `segments` and `sides`, `zhao_forge_prim` refuses out-of-range
   values on its own port, and no screen-error LOD clamp exists anywhere. A
   register nothing reads is the uncashed-cheque shape.

2. **No bank stages a forge page -- CLOSED.** `zhao_forge_pagebank` reads the
   header once at PUBLICATION and performs a BOUNDED linear scan of record line
   0 per draw, worst case count+2 line reads. It holds ONE resident record
   rather than the whole page, because 192 bytes times sixteen rows is ~15,000
   flops on a device where ALMs are the binding constraint. **The reader and
   the dispatch are ONE BLOCK**, for the reason this contract's own scoping
   gave: the reader's request side has exactly one asker and the dispatch's
   answer has exactly one use.

3. **GEOM.SETUP's single arm -- SIDESTEPPED, exactly as ruling R187 says to.**
   Nothing enters at GEOM.SETUP. `zhao_geom_clipdoor` is at GEOM.CLIP's INPUT
   with two clients, which feeds all three tines of the ordered join through
   the existing fork and bypasses none.

4. **The three-way ordered join -- UNTOUCHED, and that is the point.**

5. **"No free projector arm" -- TRUE OF THE SERVICE, FALSE OF THE PATH, and
   this is the one that had been read too pessimistically.** `zhao_part_project`
   is a FRONT MUX on client A, not the service, and its encoding has RESERVED
   `OWNER_FORGE = 2'd2` since R68 sub-build 4 with its header commissioning the
   arm in writing. A third CLIENT is not a third PORT. **Owner ruling R3 is not
   touched** -- it withholds a third port and NAMES the time multiplex as the
   thing to keep.

6. **"THE BINDING ONE: GEOM_CLIP_ATTRS = 7" -- NOT BINDING, and it had already
   stopped being so before this packet started.** Owner ruling R197 discharged
   the u/w and v/w half on the day it was written, and `zhao_geom_attrpack`
   branches on `tri_untex_i` and substitutes the zero operand. Owner ruling
   R234 D1 settled the other half: the Gouraud lanes are NOT branched on
   `tri_untex_i`, deliberately, because "an untextured primitive is still lit"
   and in the reference oracle it is the UNTEXTURED case that carries pre-lit
   colour there. So the packet has a ratified home for every slot. What is
   AUTHORED rather than ratified is the colour's VALUE, and that is a knob.

### WHAT IS OWED AND NOT DISCHARGED

* **R3'S WRITTEN SCHEDULE PROOF.** The arm exists, so the FAIRNESS half is
  measurable for the first time -- before it, `tb_part_project` drove the block
  STANDALONE and composed multi-client throughput had never been measured from
  any bench in this tree. The RATE half needs a per-client per-frame demand
  figure this composition does not produce. **Neither half is claimed here.**
* **THE AREA QUESTION.** No Quartus was run. The two vertex stores are 520 x 43
  and 520 x 24 bits and whether they infer M10K or land in flops is a fit
  question, not an argument; depth rather than bit count is the risk, as
  TERRAIN.SHEETSEAM's row already records for a 1,089-word plane.
* **THE MATERIAL MAPPING -- RULED, 2026-09-22, AND THE DOCKET IS CLOSED.**
  Owner completion ruling 2: `material_set` is the **complete**
  `handle32[material_set]` and `material_id` is an **independent u16** carried
  in `DrawProcedural`'s own reserved payload bytes (40..41). The field that was
  `handle32[material] material` is now `handle32[material_set] material_set` at
  the same offset and width, and the orphan resource kind is gone.
  `FORGE_MATERIAL_ID`, the interpretation parameter this section used to
  describe, is **removed**: the docket it held open has been paid.

  **THE OWNER CAUGHT THE MECHANISM, and it is worth keeping written down.** A
  handle32 is `{index[31:8], generation[7:0]}`, so reading the word's low
  sixteen bits as the record id made a **residency event** -- an ordinary
  generation bump -- silently select a different material and repaint the
  geometry. "Do not read the existing word both as the complete set handle and
  as its low-16-bit material ID."

  The rendering difference is disclosed rather than absorbed: a draw whose set
  handle has a nonzero generation or nonzero index bits 8..15 now selects a
  **different record** than it used to, and a legacy record with zero
  `material_id` bytes selects **record 0, a valid index**. Acceptance:
  `tests/prod/procmat_acceptance.cpp`, command bytes to GEOM.CLIP's input.
* **THE COLOUR HAS NOT BEEN LOOKED AT.** `FORGE_LIT_R/G/B` and `FORGE_ALPHA` are
  named `zhao_console_core` parameters chosen BY REASONING, and CLAUDE.md is
  explicit that measurement cannot choose a value and that the read at final
  resolution against what it sits on is the thing. Nothing has been rendered.
  Expect to move them. DECLARED: the colour is per PRIMITIVE, so a forge
  triangle's Gouraud plane is FLAT -- a per-corner colour is three inputs
  instead of one, the day the page carries one.
