# Contract — TERRAIN.NORMALMAP (Terrain detail normals, per-fragment term)

> Ledger: **not yet in `design/blocks.yml`** — this contract is written before
> the ledger entry so the architecture has somewhere to be, the same order
> GEOM.PARAMBUF was registered in. Proposed: subsystem terrain, gpu clock,
> phase 6, maturity SPECIFIED.
> RTL: `fpga/rtl/terrain/zhao_terrain_normalmap.sv` — **REBUILT TO THIS
> CONTRACT 2026-09-09** (the known-wrong draft this line used to warn about is
> superseded; its history is in `reports/NORMALMAP-ARCHITECTURE.md`). The
> rebuild implements the dated amendment at the end of this file: rescale 22,
> zero DSP via epoch coefficient tables, the mip tail, fixed latency 6.
> Directed suite: `tests/texture/terrain_normalmap_directed.cpp` (4,738
> checks; checker and shift-law positive controls both seen to fail).
> Owner ruling 2026-09-03: build the real per-pixel path, measure it, cut it
> if the number is bad — "we make it and see how bad it is … Normal maps would
> be a huge gain though."

## Purpose and exclusions

Compute the PER-FRAGMENT half of the terrain detail-lighting split

    dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
                            [per triangle]    [PER FRAGMENT — this block]

for a heightfield whose tangent frame is axis-aligned in world space, so the
detail normal `d = (dx, 0, dz)` perturbs the surface normal in world XZ with
no tangent frame built and nothing extra interpolated.

The block is a **bump-in-wire on the fragment stream** between the
perspective-correct UV producer and RASTER.TEXJOIN's accept port: it takes the
fragment's already-computed terrain UV, reads ONE texel from its own
always-resident detail tile, and emits one signed shade delta per fragment.

**Exclusions, each one a specific refusal:**

* **It does not compute the per-triangle base term.** `dot(n,L)/|n|` is
  TERRAIN.SHADE (PLANNED, no contract yet — see Notes 1); the base rides to
  the raster folded into the flat vertex colour exactly as the ratified
  vertex-light law (`spec/sky_and_beams.md` §4a) folds lighting today. This
  block never sees a face normal.
* **It does not touch the TMU, the texture cache, or TEXJOIN's sample slots.**
  The detail texel comes from this block's own M10K tile. Terrain texture
  bandwidth does not change by one access.
* **It does not apply the shade.** The delta is applied to the vertex-colour
  lanes at the fragment-packet assembly seam (Notes 2) as three saturating
  adds; RASTER.FRAGMENT's arithmetic is untouched.
* **No Y component in the detail normal.** A detail texel that could point the
  surface downward is a dent in the geometry, not a texture (kept from the
  draft — this part of it is right).
* **It does not decide the detail pattern, the strength, or the tile scale.**
  All three are authored knobs (charter art law: never remove the owner's
  control in the name of fidelity).

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain. Reset clears
the pipeline valids, the config registers to the **bit-exact-off** state
(`strength = 0`), and the counters. The tile RAM contents are NOT cleared by
reset; a tile is valid only after upload. No CDC in this block.

## Input and output packet layouts

Config (register writes, one word at a time, the `zhao_terrain_project`
cfg idiom):

| addr | field | width | meaning |
|---|---|---|---|
| 0 | `sun_x`, `sun_z` (sun 0) | 2 x s16 | s1.15 unit-vector XZ, FROM surface TOWARD light |
| 1 | `sun_x`, `sun_z` (sun 1) | 2 x s16 | present only when SUNS >= 2; else ignored |
| 2 | `strength` | u8 | u0.8, value = raw/256; **0 = the path is bit-exact off** |
| 3 | `uv_shift` | u4 | tile-scale knob: texel index = `u_raw[uv_shift+5 : uv_shift]` |

`detail_fragment` in, ready/valid — tapped from the SAME stream that feeds
TEXJOIN's `f_*` accept port, same clock, same order:

| field | width | meaning |
|---|---|---|
| `f_u_i`, `f_v_i` | signed 32 | perspective-correct terrain UV, S 15.16 (TEXJOIN's `f_u_i[0]`/`f_v_i[0]`) |
| `f_detail_i` | 1 | 1 = terrain fragment eligible for detail; 0 = force delta 0, no RAM read |
| `f_src_id_i` | 16 | rides the packet |

`detail_delta` out, ready/valid, **in input order** (fixed latency, II=1):

| field | width | meaning |
|---|---|---|
| `d_delta_o` | signed 9 | shade delta, value = raw/256, range -256..+255 |
| `d_src_id_o` | 16 | echo |

Tile upload (write-only, CPU/CMD path):

| field | width | meaning |
|---|---|---|
| `tw_we_i` | 1 | |
| `tw_addr_i` | 12 | `{v_idx[5:0], u_idx[5:0]}` |
| `tw_data_i` | 16 | `{s8 dz, s8 dx}`, value = raw/128 each |

Plus evidence: `fragments_o` (32), `zeroed_o` (32, `f_detail_i` low),
`railed_o` (32, delta saturated s9), `idle_o`.

## Backpressure rules

Ready/valid on both ports, skid-buffered so `f_ready_o` is registered. The
block is fixed-latency and in-order; a stalled consumer stalls the pipe and no
packet is dropped or reordered. The downstream alignment FIFO (Notes 2) is
sized to TEXJOIN's DEPTH so it can never overflow while TEXJOIN can still
accept — same by-construction argument as TEXJOIN's `wq_overflow_o`, and like
that one it is asserted out loud, not assumed.

## Memory ownership

**One 64x64x16-bit detail tile, always resident, in M10K** — 4,096 texels,
65,536 bits, 8 M10K blocks (e.g. two 4Kx8 banks of four 4Kx2 blocks each).
True dual port: read = fragment path, write = upload port. No tile store
port, no cache, no residency traffic, no miss path — that is the point of the
block. The tile is REUSED across the whole island (terrain detail is tiling
noise, not unique per texel); uniqueness is the albedo's job.

## Q formats and rounding

**This is the section that matters** (`spec/qformats.md` §2/§3/§4).

Formats are chosen so the scaling cancels to a bare product:
`strength/256 * d/128 * L/32768` expressed with 8 fraction bits is
`strength*d*L / 2^23` — one shift, no correction multiply.

The whole fragment term is ONE rounding (§3 single-rounding law):

    raw_dot_i = dx*sun_x_i + dz*sun_z_i          // s8 x s1.15, exact, per sun
    delta     = rescale_s( strength * SUM_i raw_dot_i , 23 )   // round-half-up
    delta     = saturate to s9

Widths, stated rather than assumed: `raw_dot` per sun is |2*127*32768| < 2^23,
so signed 24; the strength product is signed 32; with SUNS = 2 the sum before
rescale is signed 33. The rescale add of 2^22 cannot overflow that. One sun
cannot rail the s9 result (|delta| <= 255 by construction); two suns can reach
510, which saturates and counts in `railed_o`.

**The delta's meaning downstream:** at the fragment-packet assembly seam each
vertex-colour lane becomes `v'_c = clamp_u8(v_c + delta)` BEFORE the existing
`unit_mul(texel, vertex)` modulate — algebraically `texel*(v + delta)`, which
is the split's `albedo * (base + detail)` with the detail lit in white. The
sun-colour approximation is DECLARED: the delta is monochrome, so detail
relief is not tinted by the sun colour. At 240p under near-white suns this is
chosen deliberately; if the look demands tinted detail the escalation is three
per-channel deltas (three multipliers), costed in the architecture report.

**Zenith behaviour, declared:** `d` has no Y component, so a zenith sun
(`sun_xz = 0`) produces delta = 0 — detail contrast fades as a sun approaches
noon, exactly as flat-light photography flattens relief. This is the dropped
second-order term of the linearisation, and it reads naturally.

**The approximation and its knob:** `n/|n| + s*d` is not re-normalised. The
error grows with `strength` and is zero at zero. `strength` is a LOOK-TUNED
named register — author by eye, render, look, adjust.

## Latency (fixed or variable)

Fixed 3: address register -> M10K synchronous read -> product/rescale
register. II = 1.

## Target throughput

One fragment per clock, matching TEXJOIN's accept rate. The terrain fragment
budget is 276,480/frame (`zhao_texture_tmu_pipe.sv` workload table); this
block adds ZERO texture samples to that budget.

## Overflow and malformed-input behaviour

* `strength == 0` (the reset state) -> `delta == 0` for every fragment,
  **bit-exact**: `rescale_s(0, 23) = 0`, and `clamp_u8(v + 0) = v`, so the
  machine with this block at reset renders bit-for-bit what it renders with
  the block absent. That identity is the cut plan's foundation and is a
  directed test, not an assumption.
* `f_detail_i == 0` -> delta forced 0 without a RAM read; counted in
  `zeroed_o`.
* Two-sun saturation -> s9 rails, counted in `railed_o`, never wrapped.
* There is no malformed input: every 16-bit texel, every UV bit pattern and
  every config value is a legal (if ugly) authored state. Garbage in the tile
  before upload produces garbage shading and nothing else — no memory outside
  the block can be touched by construction (no master ports).

## Counters and traces

`fragments_o` counts accepted fragments, `zeroed_o` the non-detail ones,
`railed_o` the saturated deltas. No counter-catalog id is bound — minting one
is a `spec/counters.md` amendment, not an RTL decision.

## Scalar reference function

`reference/include/zref/zref_terrain_normalmap.hpp` **exists but requires
amendment before it is the law** — the required changes are recorded in
`reports/NORMALMAP-ARCHITECTURE.md` and are, in one line each:

* `zref::terrain::normalmap_decode` — EXISTS, correct, kept as is.
* `zref::terrain::normalmap_detail` — EXISTS but uses truncating `/ 32768`;
  must become `rescale_s(., 23)` per §4, single rounding over the summed-suns
  product, s9 saturation added.
* `zref::terrain::normalmap_shade` / `normalmap_shade_multi` — the
  ambient-FLOOR semantics conflict with the ratified additive-ambient
  vertex-light law (`spec/sky_and_beams.md` §4a) and the application point has
  moved to the vertex-colour lanes; both functions are superseded by
  `zref::terrain::normalmap_apply` (PLANNED AND NOT WRITTEN):
  `v'_c = clamp_u8(v_c + delta)`.
* `zref::terrain::normalmap_length` / `normalmap_base` — **MOVED 2026-09-03**,
  as this section required. TERRAIN.SHADE is now contracted
  (`design/contracts/TERRAIN.SHADE.md`) and they live in
  `reference/include/zref/zref_terrain_shade.hpp` as `shade_length` and
  `shade_base`. Both defects are fixed there: the sum of squares accumulates in
  **unsigned 64** (three Q16.16 components at the fx16 rail reach 1.38e19
  against signed 64's 9.22e18 — undefined behaviour in C++, a silent wrap in
  RTL giving a small length and therefore a huge wrong shade), and rounding is
  round-half-up through the shared `rshift_round`.

**Status of the amendments above, 2026-09-03:** `normalmap_detail` now rounds
half-up rather than truncating; the ambient-FLOOR functions are gone and
ambient is additive per §4a; the base light has moved out. What remains
planned is `normalmap_apply` at the vertex-colour lanes — the application
point — which needs the fragment seam that does not exist yet.

## THIS BLOCK IS NOT PRECEDENT — D-8, RULED 2026-09-03

**General tangent-space normal maps are REFUSED in v1.**

This block is cheap for one reason and one reason only: **a heightfield's
tangent frame is world-axis-aligned**, so a detail normal perturbs the surface
normal in world XZ directly, with no frame to build and nothing extra to
interpolate.

**That does not generalise** to arbitrary props, skinned creatures or twisted
surfaces, which would each need a real tangent basis per vertex.

For v1:

* terrain **may** use this specialised detail-normal path;
* **creatures and props use authored/skinned vertex normals**;
* fine appearance comes from geometry, textures, toon treatment and bounded
  lighting;
* **no tangent/bitangent vertex attributes**;
* **no arbitrary mesh normal texture sample**;
* **no tangent-frame reconstruction in the fragment path.**

Reconsidering it requires an explicit later architecture with a tangent-basis
format, skinning rules, interpolation cost, a sample budget and a measured
visual case.

> **The terrain block is not precedent.**

The refusal is written here, in the file someone would read while reasoning
"the terrain does it, so we can" — which is the inference the ruling exists to
prevent, and the reason it was worth ruling explicitly rather than leaving
unstated.

## Directed tests

**PLANNED AND NOT WRITTEN** (`reports/PHANTOM-CITATIONS-AUDIT.md` — named
without paths until the file exists; note the draft RTL's ENFORCED-BY line
already cites a nonexistent test and must be fixed):

* bit-exact-off — strength 0 and `f_detail_i` 0 both give delta exactly 0.
* format cancellation — full-scale texel, full-scale sun, strength 255:
  delta exact against the amended oracle at the corners.
* rounding — both halves of the round-half-up boundary at the rescale, on a
  NEGATIVE product (the draft RTL's floor-shift and the draft oracle's
  truncation disagree exactly here; the test pins the §4 law).
* two-sun saturation railing s9 and counting.
* tile addressing — `uv_shift` at both extremes; wrap at the 64-texel seam;
  a written texel read back through the fragment path.
* backpressure — stalled consumer, no drop, no reorder.

## Randomized differential tests

**PLANNED AND NOT WRITTEN.** Random UV, random tile contents, random config
against the amended oracle; the lane must assert it sampled negative deltas,
saturated deltas and `f_detail_i = 0` packets, or a green run means nothing
(the TERRAIN.NORMALS lesson).

## Formal properties

None planned. The block is a ROM lookup and one MAC; the directed rounding
cases cover the only subtle arithmetic, and there is no state a solver could
see that the tests do not walk.

## Synthesis / resource ceiling

Not synthesized. Ceiling, with the arithmetic:

| resource | ceiling | reasoning |
|---|---|---|
| ALM | **500** | addr mux ~30, MAC control + rescale/saturate ~80, skid + pipeline regs ~90, cfg ~40, alignment FIFO 16x9 ~75, margin ~185 |
| DSP | **3** | `dx*sun_x + dz*sun_z` packs into one 18x18 pair; x strength one more; third is margin/SUNS=2 |
| M10K | **9** | 8 for the 64x64x16 tile + 1 margin; the s4-packed cut halves the tile to 4 |

A fit above the ceiling is a defect in the estimate, to be reported, not
absorbed.

## Integration capture cases

None yet. Nothing feeds this block and nothing consumes it; the seams it
needs are named in Notes 2 and costed in the architecture report. The
composition test that matters: one terrain triangle through
TESS -> NORMALS -> SHADE -> PROJECT -> raster with a nonzero tile, against the
zref renderer with the same tile — the first moment the delta is seen applied.

## Notes

**LAWS CHOSEN, NOT FOUND.**

1. **The per-triangle term lives in TERRAIN.SHADE (PLANNED), not here.** The
   production tree computes face normals (TERRAIN.NORMALS) and then NOTHING
   consumes them — there is no terrain lighting block in
   `design/prod_manifest.yml` at all, and `zhao_terrain_project` law D
   deliberately drops the normal. TERRAIN.SHADE is therefore required with or
   without normal maps, and its cost must not be billed to this feature. Its
   recommended shape (dot / |n| via CLZ-normalise + M10K rsqrt table + one
   Newton step, II=1, the §6.1 `rcp_u24` precedent) is in the architecture
   report.
2. **The application seam is the fragment-packet assembly glue** between
   TEXJOIN v2 retirement and RASTER.FRAGMENT's `frag_*` port: a 16x9
   alignment FIFO written when TEXJOIN accepts the fragment and popped when it
   retires (retirement is allocation order, so FIFO order matches by
   construction), then `v'_c = clamp_u8(v_c + delta)` on the three
   vertex-colour lanes. RASTER.FRAGMENT's ports, state word and arithmetic are
   untouched; `frag_vert_rgb_i`'s documented meaning gains the word
   "detail-adjusted".
3. **The tile is world-anchored through UV.** Terrain top-cell UV is
   `u = wx/pitch, v = wz/pitch` (`reference/src/zrender/terrain.cpp` UV law),
   so the SAME perspective-correct UV that addresses the albedo addresses the
   detail tile — no new interpolant, no second attribute plane, continuous
   across cell boundaries. This is the heightfield trick doing the work.
4. **Nearest sampling, deliberately.** Terrain albedo is CLUT8 nearest; a
   bilinear detail tap would be smoother than the surface it perturbs. If the
   look demands it, four banked reads + the filter cost ~2 DSPs and ~150 ALMs.
   Minification aliasing under motion is a known risk; the mitigations
   (strength fade by patch LOD tier, or a 32x32 second level, +2 M10K) are in
   the report. Look first.
5. **SUNS is a build parameter, default 1.** The ratified environment record
   (`SetEnvironment 0x0311`) carries ONE sun; multiple moving suns are an
   identity ask without an ABI home yet. The datapath accumulates per-sun
   products before the single rounding so SUNS=2 is a parameter change plus
   one config word, not a redesign.

---

## AMENDMENT 2026-09-09 — the rebuild's four corrections and one extension

Made with the RTL rebuild answering the owner's `bumomapping.md` ("we need
detail bump mapping for terrain ... set it up for production"). Full
argument: `reports/TERRAIN-BUMP-MAPPING-ARCHITECTURE-20260909.md`. Where this
section disagrees with the body above, THIS SECTION IS THE LAW.

### A1. The rescale is 22, not 23 — the factor-of-two, closed

The body's own format sentence proves it: strength has 8 fraction bits, d has
7, an s1.15 sun has 15 — 30 in, 8 out, so the ONE rounding is

    delta = sat_s9( rescale_s( strength * SUM_suns(dx*sun_x + dz*sun_z), 22 ) )

The 23 above is the s1.15-vs-Q16.16 slip the oracle header already documents
("getting that wrong is a factor of two in the relief") — the oracle side was
fixed 2026-09-03 and this contract never was. Consequences that change:

* full-scale one-sun delta is ~253 (essentially full colour scale), not ~126;
* **one sun CAN rail s9** at the legal-but-ugly register corners
  (`sun_x = sun_z = -32768` with `d = (-128,-128)` gives +510 → +255,
  counted in `railed_o`); the "two suns can reach 510" sentence now describes
  one sun's corner too.

`zref::terrain::normalmap_delta_s9` (added to the oracle header the same day)
is the executable form; the directed suite pins the tie-rounding on negative
products with literal vectors, and a `-GDELTA_SHIFT=23` build of the real RTL
fails 1,830 checks — the law is instrumented, not asserted. The sun config
words stay s1.15, derived per frame from the ratified Q16.16 light by
`sun15 = clamp(rshift_round(L16, 1), -32768, 32767)` on the HPS.

### A2. Zero DSP: the per-fragment MAC is two epoch coefficient tables

Per the rescue ruling (`ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt`
§14.3, "for a fixed per-epoch light coefficient, byte tables offer exact
products"): the block holds

    tblx[dx] = dx * Kx,   Kx = strength * SUM_suns(sun_x)   (exact, s33)
    tblz[dz] = dz * Kz,   Kz = strength * SUM_suns(sun_z)

in two 256x33 M10Ks, filled by an internal sequencer with NO multiplier
(K by 8-step shift-add over strength's bits, the tables by pure accumulation
from -128*K). Every product is exact and the sum-over-suns folds into K by
distributivity BEFORE the single rounding, so the result is bit-identical to
the body's per-sun-accumulate law at any SUNS.

Observable semantics this adds — the COLD window: from reset, and from any
cfg write to addrs 0–2 until the ~267-cycle refill completes, `table_ready_o`
is low and every `f_detail_i=1` fragment emits delta 0, counted in the new
`cold_o` counter (fragments already in flight at the write are forced cold
too — they must not read a table being rewritten under them). This is what
lets "reset = bit-exact off" hold without resetting a RAM, which an M10K
cannot do. Mixed-port read-during-write on the K tables is declared
don't-care: it is reachable only inside the cold window, where the read
result is never used.

### A3. The mip tail (the 2026-09-05 addendum, adopted)

Detail that never coarsens aliases into shimmer, so the tile grows the
seven-level pyramid: 64..1 square levels, 5,461 words, level bases
0/4096/5120/5376/5440/5456/5460, addressed

    addr = base[L] + ((v6 >> L) << (6 - L)) + (u6 >> L),  u6 = u_raw[uv_shift+5:uv_shift]

(`zref::terrain::normalmap_pyramid_addr` is the single definition; the
offline packer averages SIGNED dx/dz and never re-normalises). Port and
config changes:

* `tw_addr_i` widens 12 → 13 (flat pyramid word address; writes ≥ 5,461 are
  ignored as upload-tool faults);
* new fragment input `f_lod_i` (unsigned INTEGER mip level, LODW=4 — the
  addendum's preferred integer contract, NOT the TMU's Q4.4 high-nibble);
* new cfg word 4: `{lod_bias[8:4] (s5), max_level[2:0]}`. Sampled level =
  `clamp(f_lod_i + lod_bias, 0, min(max_level, LEVELS-1))`. **Reset state
  max_level = 0 reproduces the un-mipped body behaviour bit-exactly.**
* `LEVELS` is a build parameter (1..7); LEVELS=1 is the body's bare 4,096-word
  tile.

### A4. Latency and ceilings

* Fixed latency is **6** (in-regs, level/wrap, address, tile read, K-table
  reads, round/clamp out-regs), II=1, in order — the body's "fixed 3"
  described the multiplier shape A2 removed. The alignment-FIFO sizing
  argument in Backpressure is unchanged (it depends on order and boundedness,
  not the constant).
* Ceilings: ALM **500** (unchanged), DSP **3 → 0** (a DSP appearing in this
  block's fit is now a defect by definition), M10K **9 → 15** (pyramid tile
  predicted 12 + two K tables + margin; the s4-packed cut and LEVELS knob
  both shrink it).

### A5. Status corrections to the body

* Directed tests: **WRITTEN**, `tests/texture/terrain_normalmap_directed.cpp`
  (placed there because `tests/terrain/` was a live rearchitecture lane on
  the rebuild day). Every planned bullet in the body's list is covered, plus
  cold, epoch-refill, mip and II cases. Counters seen to fire: all four.
* Randomized differential: **WRITTEN**, same file, with the required coverage
  asserts (negative, positive, railed, zeroed, nonzero-level all sampled or
  the run fails).
* `normalmap_apply`: **WRITTEN** in the oracle header (plain u8 form; the
  seam may adopt the lit-range clamp variant — see the architecture report's
  seam section).
* Evidence outputs now: `fragments_o`, `zeroed_o`, `railed_o`, `cold_o`,
  `table_ready_o`, `idle_o`.
