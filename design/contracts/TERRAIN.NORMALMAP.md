# Contract — TERRAIN.NORMALMAP (Terrain detail normals, per-fragment term)

> Ledger: **not yet in `design/blocks.yml`** — this contract is written before
> the ledger entry so the architecture has somewhere to be, the same order
> GEOM.PARAMBUF was registered in. Proposed: subsystem terrain, gpu clock,
> phase 6, maturity SPECIFIED.
> RTL: `fpga/rtl/terrain/zhao_terrain_normalmap.sv` — **the file on disk is a
> DRAFT that does not implement this contract and carries two confirmed
> defects** (see `reports/NORMALMAP-ARCHITECTURE.md`); it must be rewritten to
> these ports before any maturity claim.
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
* `zref::terrain::normalmap_length` / `normalmap_base` — belong to
  TERRAIN.SHADE, not this block, and both carry defects (int64 overflow at
  the legal fx16 rails; truncating division against the round-half-up law).
  They move out of this header when TERRAIN.SHADE is contracted.

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
