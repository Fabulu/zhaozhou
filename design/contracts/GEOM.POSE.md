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

Variable: hit ≈ palette handle latency; miss = ~12 multiplies/bone decode,
bone-serial (≤32 bones). Working-set derivation: ≤128 distinct tuples/frame
× 32 × 12 ≈ 49k multiplies/frame — noise against the DSP budget.

## Target throughput

1 decoded bone per clock steady-state on miss; 1 palette handle per request
on hit.

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
