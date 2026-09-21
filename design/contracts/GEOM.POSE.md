# Contract — GEOM.POSE (Pose bank decoder and cache)

> Ledger: `design/blocks.yml` · owner ZH-081 · phase 9 · maturity REFERENCE_COMPLETE (2026-08-17, pinned `bd1c733`)
>
> Introduced by the world-identity wave (RUN-20260816-0046). Law:
> `spec/creature_rules.md` §2.2. Clips live in VRAM compressed (8 B/bone/
> frame quantized quaternions + fx16 root); this block decodes
> (type, clip, frame) → ≤32 bone matrices into a shared decoded-pose cache
> that skinning indexes. Bake-everything-at-load was rejected by
> derivation: ×6 memory (1.97 MB vs 343 KB per creature type; ten types
> would eat 82% of the 24 MB meshlet+LOD+animation pool).

## Purpose and exclusions

Serve bone-matrix palettes to GEOM.SKIN by decoding compressed clip-bank
frames on cache miss and holding decoded poses (≤128 tuples ≈ 192 KiB, VRAM
hot region + M10K staging for the pose in use). Instances of one creature
type playing one clip at one tick share one decoded pose — the type-grouped
army economy (recon S2).

Exclusions: no clip selection or clip clocks (sim truth decides what plays);
no skinning; no event-tag processing (tags are sim-side, creature_rules
§2.1/§4.2).

## Clock and reset semantics

`gpu` domain; synchronous reset invalidates the cache (cold decode is
correctness-neutral, only a latency event).

## Input and output packet layouts

- `pose_requests` (from GEOM.MESHFETCH instance walk): {type_id, clip_id,
  frame_no, bone_count}.

  **THE GAME-FACING CARRIER IS `DrawPosedForm` 0x0305**, ratified by owner
  ruling R229 (D-POSEPAGE-A) on 2026-09-21 and specified in
  `spec/commands.zidl`. Until then this clause named four fields that
  appeared in NO command at any opcode, which is what kept core entry I29
  open after its bytes, its page container and its store had all been
  settled. `zhao_cmd_exec` lowers the record onto
  `draw_posed_o`/`draw_clip_id_o`/`draw_frame_no_o`/`draw_sub_o`, riding the
  same `draw_valid_o` beat as the draw itself.

  **TWO OF THE FOUR FIELDS ABOVE DO NOT COME FROM THE COMMAND, and a reader
  wiring this up owes both of them somewhere else:**

  * `type_id` is the CREATURE TYPE and is deliberately absent from the
    record. The draw names the creature through `form`, a handle32 whose
    24-bit index is `spec/memory_rules.md` 5f.1's directory key. The
    form -> type resolution is owed ONE implementation and a directed check;
    carrying a second identity on the wire beside the first would be the
    two-sources-of-truth shape this tree keeps striking.

    **SETTLED 2026-09-21 (owner ruling on kind-8 / kind-9 ownership;
    `spec/cartridge.md` §4e).** There is no form -> type mapping, because
    there is no second type id: **the 24-bit form index IS the key**, and the
    resource now says which form it belongs to rather than the loader
    guessing. The `BODY` header (v2, bytes 16..19) and the `CLIP_BANK` header
    (v2, bytes 20..23) each carry an explicit `owner_form_index`, and the
    request carries the DRAW'S OWN form index —
    `zhao_geom_drawjob.j_form_idx_o`, which is that block's existing
    `form_idx_q` (`d_form_i[31:8]`) exposed on the accepted job with the job's
    own handshake, **never re-read from an unrelated live register later**.
    `zhao_geom_clipread.p_form_idx_i` is the consumer; a request whose form is
    not the form BOTH resident sections name is refused and counted on
    `owner_mismatch_o`, not recorded as a clip miss and not as an absent
    resident.

    **Three identities, kept apart, and this clause used to conflate two of
    them.** FORM identity is the 24-bit index a page NAMES. RESOURCE identity
    is the publication's own index plus its full 16-bit generation
    (`res_body_index_o`/`res_body_gen_o`, `res_clip_index_o`/`res_clip_gen_o`).
    INSTANCE identity is the draw. A body and its clip bank are published under
    two independent resource indices and still name one form; a publication
    index is **not** a proof of ownership, and neither is a matching bone
    count, loader order, the first ladder row or a physical slot.
  * `bone_count` is a property of the CREATURE PAGE, not of the request --
    `zref::clip_page`'s header carries it at `kOffBoneCount`, and
    `creature_rules` 1.2 ceilings it at 32.

  **AND THE COMMAND CARRIES A FIFTH FIELD THIS CLAUSE OMITS: `sub`, the
  half-key phase.** It is not optional. This block's own cache
  (`zhao_geom_pose_cache`) takes it as `acq_sub_i` and its header records
  what omitting it cost -- with baked 60 Hz data "a key and its midpoint had
  the SAME {type, clip, frame} and aliased ... the cache returned the wrong
  palette and nothing reported an error". A request built from this clause's
  four fields alone would reintroduce that defect.

  **WHAT IS STILL UNDETERMINED**, so nobody reads the above as a green light:
  R229 ratified the COMMAND layer only. Section 4.2 of
  `reports/ZHAOZHOU_ANIMATION_HPS_RESIDENCY_ARCHITECTURE.md` (owner-ratified
  2026-09-03) wants the request side to be "a validated resident handle with
  generation and epoch, not a naked slot and frame", and that is a different
  layer with no ruling. The clip-bank GENERATION the cache keys on
  (`acq_gen_i`, owner ruling D-3) is published by `PublishResource` 0x0030's
  `new_generation` and is NOT on the draw record.

  **THE CACHE KEY IS WIDER THAN THAT CLAUSE SAYS, AS OF 2026-09-21.** The
  ownership ruling's section 4 names two obligations the byte format alone
  does not satisfy, and `zhao_geom_pose_cache` now carries both:

  * **24 BITS OF FORM.** `acq_type_i` and the stored tag were 16 bits against
    a 24-bit form index, so `0x000100` and `0x010100` were ONE cache line.
    `TYPE_W` parameterises the field, **24 in production** with an elaboration
    guard refusing anything outside 16..24 (fired at `-GTYPE_W=8` by
    `geom_pose_cache_elab_guard`, because `--lint-only` does not run `initial`
    blocks). The alias is asserted distinct on the **hit** path as well as the
    miss path — an insert storing 24 bits with a comparison reading 16 passes
    only the second.
  * **ASSET LIFETIME.** Owner equality proves the intended FORM, not which
    version of its skeleton and animation produced a cached palette.
    Republish the BODY alone — same form, same clips, same frame, same
    sub-phase — and `acq_gen_i` cannot see it. So `acq_body_idx_i`,
    `acq_body_gen_i` and `acq_clip_idx_i` join the key, taken from
    `zhao_geom_clipread`'s `res_body_index_o` / `res_body_gen_o` /
    `res_clip_index_o`. **No field is ever compared to another field**: body
    and clip generations belong to independent publications, and two different
    clip resources may carry equal local generation numbers. The full 16-bit
    publication generations are kept, never reduced to the handle's low eight
    bits.

  Cost, recorded: the tag is 192 bits against 120 at TUPLES=128 — +9,216
  LOGICAL bits, of which 1,024 are the eight form bits and 8,192 the two
  resource indices and the body generation. That is not an M10K or ALM figure;
  physical packing, the wider comparator and the four extra ports are what a
  fit measures.
- `clip_pages` (VRAM read via MEM.GUARD): kind-9 clip-bank bytes
  (spec/cartridge.md §4).
- Output `bone_matrices`: palette handle + ≤32 × 3×4 fx16 matrices (48
  B/bone) to GEOM.SKIN.

## Backpressure rules

ready/valid; a miss stalls only the requesting instance stream, hits flow
at full rate.

## Memory ownership

Read-only on clip pages; exclusive writer of the decoded-pose cache region
(MEM.GUARD grant at Phase 9).

## Q formats and rounding

Quantized quaternion lanes s16[4]; decode = the 9-product quat→matrix
formula with qformats §3 single-rounding per element. ~~**The exact lane
format (proposed S 1.0.14) and decode rounding are NOT yet frozen — they
require a spec/qformats.md amendment (QFMT_VERSION discipline §13) before
this block may reach REFERENCE_COMPLETE.**~~ **Frozen 2026-08-17:
`quat16` = S 1.0.14 hemisphere-canonical, one `rescale(·,11)` per matrix
element, no renormalization — qformats §7.6 (amendment C1, QFMT_VERSION 2),
with the declared bounds (element ≤ 0.50 LSB, column-norm drift ≤ 15.86 LSB,
end-to-end column angle ≤ 0.0156°, measurement protocol included).** No
renormalization at decode; the quantization-induced scale error bound is
declared there.

## Latency (fixed or variable)

Variable: hit ≈ palette handle latency; miss = bone-serial decode (≤32 bones).

**Corrected and MEASURED 2026-09-10 (R4 implementation,
`reports/POSE-DECODE-SEQUENCED-20260909.md`):** the old "~12 multiplies/bone"
undercounted the chain by 6.75× — the real work is 9 (quat) + 36 (A_parent·LR)
+ 36 (A_b·inv_rest) = **81 products/bone** (45 for bone 0, which skips MUL1) plus
~34 cycles/bone of non-multiplier walk (13-cycle ancestor read, 12-cycle store,
handshakes). Measured by `geom_pose_decode_directed` on the 32-bone chain:

| MUL_LANES (quat/mat) | DSP | cycles / 32-bone palette | cycles/bone |
|---|---|---|---|
| 1 / 1 (default, R4) | 4 | **3,694** | 115.4 |
| 9 / 3 (pre-R4)      | 18 | 1,799 | 56.2 |

The old working-set sentence ("≈49k multiplies/frame — noise against the DSP
budget") is superseded twice over: the multiply count was wrong, and it used a
utilisation figure to dismiss an area cost (see the R4 note below).

## Target throughput

1 decoded bone per clock steady-state on miss; 1 palette handle per request
on hit.

**RELAXED BY OWNER RULING 2026-09-09** (`reports/OWNER-RULINGS-20260909-2300.md`).
Fabian: *"relax the 1 bone/clock rule. It must have been arbitrary."*

The one-bone-per-clock target forced this block's ~12-product decode to be
SPATIAL, which is where its **18 DSP** come from -- `zhao_geom_quat2mat` (9) plus
`zhao_geom_mat3x4_mul` (9). The relaxed target is **one bone per twelve clocks**,
i.e. a single operand-muxed multiplier lane sequenced across the decode, on the
pattern `zhao_terrain_normals.sv:203` and `zhao_terrain_lod.sv:273` already use.

The demand figure was already in this contract, two paragraphs down: <=128
distinct tuples/frame x 32 x 12 ~= 49k multiplies/frame. Against
`computeClocksPerFrame = 1,666,666` that is **2.9% of a frame on ONE lane**,
against 18 DSP provisioned -- about 34x over. BOTH NUMBERS ARE WRONG; see the
correction immediately below. So the cost of relaxing is 2.9%
of a frame and the return is **17 DSP**.

**CORRECTED 2026-09-09, and the correction is arithmetic I got wrong myself.**
The return is **14 DSP, not 17**, and my own figure was internally inconsistent:
I wrote "18 -> ~3, return 17", and 18 - 3 is 15. Read from
`tools/budget/calibration.json` and corroborated by `zhao_project_core.sv:160`
("1 DSP from 8 to 27 bits and 3 from 28 to 33"): a quat2mat s16 product is
**1 DSP**, a mat3x4 s32 product is **3**, so one lane per engine is **18 -> 4**,
a return of **14**.

And the demand figure in this contract undercounts by **6.75x**. It says ~12
multiplies per bone; the real chain is quat2mat 9 plus TWO mat3x4 multiplies at
3x4x3 = 36 each, i.e. **81 products per bone**. On one lane that is 19.9% of a
frame by product count, and **28.4% measured** with the sequencer's non-multiply
cycles included -- not 2.9%. It still meets the demand, but the headroom is
**3.5x, not 34x**.

One further correction, from the implementer and worth keeping because it changes
what the ruling actually did: **`zhao_geom_mat3x4_mul` was ALREADY element-serial
and shared across both matrix multiplies.** The pre-R4 block measured **56.2
cycles per bone**, so the 1-bone-per-clock target was never met by any RTL in this
tree -- it shaped internal widths, not the schedule. The ruling was still right to
relax it; it simply removed a target nothing was honouring.


**IMPLEMENTED 2026-09-10, and the paragraph above needed THREE corrections**
(`reports/POSE-DECODE-SEQUENCED-20260909.md` carries the derivations):

1. **The demand was 12 multiplies/bone; the chain is 81 products/bone** (9 quat
   + 2 × 36 matrix). Worst legal frame at the new default, MEASURED: 128 tuples
   × 3,694 cycles = 472,832 cycles = **28.4% of computeClocksPerFrame**, not
   2.9% — still meets the clamped worst case with 3.5× headroom, and the ~90%
   hit economy makes the typical frame roughly a tenth of that. 128 is a
   CLAMP (a frame demanding more is a content-tier violation), so it is the
   legal worst case, not a capacity misread as demand.
2. **The return is 14 DSP (18 → 4), not 17.** One 32x32 lane is **3 DSP** by
   the measured calibration cliff (28..33-bit operands → 3;
   `tools/budget/calibration.json`, `zhao_project_core.sv` cost section), and
   the quat lane's 16x16 is 1 more. −15 needs the quat products folded into
   the 32x32 lane across the module boundary; −17 needs the 32x32 built from
   four ≤17-bit partials on ONE 1-DSP lane, ≈2.9× the walk (~81% of a frame
   at full churn). Both are named options in the report, neither is taken.
3. **The pre-R4 RTL never met 1 bone/clock anyway** — measured 56.2
   cycles/bone at the legacy parameters (mat3x4_mul was already
   element-serial and shared across both multiplies). The target shaped
   quat2mat's 9 spatial products and mat3x4_mul's 3-per-cycle width, but the
   block it demanded never existed, which rather supports the owner's
   "arbitrary".

The knobs are `MUL_LANES_QUAT` (1 default, 9 = spatial) and `MUL_LANES_MAT`
(1 default, 3 = element-serial) on `zhao_geom_pose_decode`, per-module
`MUL_LANES` on the submodules. The relaxed target this contract now declares:
**one decoded bone per ≤120 clocks steady-state on miss** at the defaults;
1 palette handle per request on hit (unchanged).

Note what went wrong in the original, because the sentence is still below and
still reads as reassurance: it called 49k multiplies/frame "noise against the DSP
budget", using a UTILISATION figure to dismiss an AREA cost. DSP cost is a count
of `*` sites (`TEXTURE.TMU.md:490`). The utilisation argument is in fact the
argument that these 18 are the most shareable DSP in the machine.

## Overflow and malformed-input behaviour

Cache-full: evict LRU tuple not referenced this frame; a tuple referenced
this frame is never evicted (bounded by the 128-tuple budget — a frame
demanding more is a content-tier violation, counted and clamped
deterministically). Bad clip/frame ids: safe no-op palette (identity bind
pose) + error counter, never a wild read (guard-checked).

## Counters and traces

`cache_hits`, `cache_misses` (pose-cache hit economy is THE health metric
of the creature lane). Trace: decoded tuple ids.

## Scalar reference function

`zref::PoseBank` — decode-on-fetch pose cache per creature_rules §2.2
(128-tuple LRU, referenced-this-frame never evicted, clamped inserts
counted, bad ids → identity bind pose). Landed with the creature reference
core, commit `bd1c733` (`reference/include/zref/zref_creature.hpp`).

## Directed tests

`tests/geometry/geom_quat2mat_directed.cpp`, `geom_mat3x4_mul_directed.cpp`,
`geom_pose_decode_directed.cpp`: bit-identity against `zref::creature` on every
element, walk-length laws (10-cycle quat walk, 37-cycle matrix walk at the
defaults; 0/12 at the legacy parameters, held by the `*_spatial`/`*_elem` CMake
variants), counters seen to fire. The R4 sequencers' checkers are demonstrated
instruments: `tests/mutants/zhao_geom_quat2mat_mutant.sv` (schedule-slot swap)
and `zhao_geom_mat3x4_mul_mutant.sv` (element-boundary accumulator) each break
one line, and the inverted-polarity controls PASS only when the differential
FAILS — on both mutants every counter still balances, so the differential is
the only detector for that fault class.

`tests/geometry/creature_core.cpp` (§1–§2, §7): decode golden vectors with
hand-computed anchors — **identity/180° quats exact; 90° within the
declared bound (3 LSB)**, hemisphere canonicalization, cache hit/miss
counters, eviction law (referenced-this-frame never evicted), clamped
insert, bad-id no-op. (The original reservation named a
`geom_pose_directed.cpp` that was never written and the wording
"identity/90° quats exact" — 90° exactness is unachievable in any
power-of-two quat lane, √2/2 being irrational; both corrected here.)

## Randomized differential tests

`tests/geometry/creature_core.cpp` §2 + §7 (same file, MEM.GUARD
precedent): the 3,600-rotation decode sweep vs a double-precision oracle
(five axes × 720 half-angle steps; the oracle is the test's, not the
implementation's) asserting the §7.6 bounds, and the order-independence
differential — same request multiset in any order yields identical cache
content and palettes.

## Formal properties

Cache never evicts a tuple referenced in the current frame; guard-region
containment rides `mem_guard_no_escape`.

## Synthesis / resource ceiling

`geometry_mantle` group (charter §25, 20% ceiling).

## Integration capture cases

Phase-9 gate: 64–128 active creatures, dozens visible at mixed
representations, animation shared before dual projection, no CPU per-limb
submission (charter §23) — pose-cache hit rate is the gate's counter
evidence.

## Notes

The decode-on-fetch decision is the memory/architecture trade recorded in
creature_rules §2.2; if Phase-9 evidence shows the miss economy failing
(hit rate below ~90% under the content tier), the fallback is baking only
the ACTIVE clip set per type at load — a software policy change, not a
format change (clip pages already carry everything needed).

### The known gap: per-instance pose overrides

**Recorded 2026-09-04 from `reports/CapeProvisions.md` (docket D6), which calls
this "the one genuinely missing feature".** The contract was silent on it, which
is the wrong place for it to be silent: the cache key is this block's central
decision and the gap is a property of that key.

`(type, clip, frame)` is exactly right for an army — a hundred instances of one
creature at one frame share one decoded pose, and that sharing is what makes the
whole scheme affordable. **It is exactly wrong for a cape in wind.** Secondary
motion is per instance by definition; two soldiers standing in the same frame of
the same clip do not have the same cape, and under this key they are forced to.

**What is wanted is a sparse per-instance PATCH over the shared palette**, not a
second cache and not a wider key. Widening the key to `(type, clip, frame,
instance)` would defeat the sharing entirely — every instance would miss, and
the hit rate that the Phase-9 gate measures is the same number that makes the
block worth building.

`CapeProvisions` sizes the affected set deliberately small: **6 bones**
(waist/thigh) or **8** for a long dramatic cape, two columns by three or four
rows so left and right react independently, 150–300 triangles, two-weight
skinning. So the patch is a handful of matrices over a ≤32-matrix palette, and
the shared entry stays shared for every bone the patch does not name.

**Not designed here, and deliberately not.** There is no cloth processor and the
owner has not ruled on where the patch is produced or stored. What this note
fixes is that the constraint now lives beside the cache key it constrains,
rather than only in a report — so the next person to touch the key knows that
one class of content cannot use it as it stands.
