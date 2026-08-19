# Contract — TEXTURE.MOSAIC (Terrain Mosaic selector)

> Ledger: `design/blocks.yml` · owner ZH-030 · phase 6 · maturity REFERENCE_COMPLETE

## Purpose and exclusions

Stable procedural pattern picking the terrain texture candidate; yields exactly one primary sample per texel.

World-identity wave (spec/terrain_rules.md §6): candidates are the per-cell
{matA, matB, weight} of Island Patch layer E; the winning id samples a
64×64 CLUT8 tile with per-cell (0,0)–(1,1) UV and MIRRORED repeat (the
donor's zero-blend, zero-seam recipe — recon S1 §3). Authored transition
groups (MAPG heir, terrain_rules §6.3) may restrict the pick to a group's
members. Vertex tint (layer H) is NOT this block's business — it rides the
Gouraud path, keeping the restricted aux sampler free for the surface sheet
(charter §15/§26: one primary + one restricted aux, no second TMU, ever).

**Excluded, deliberately:** no memory port, no table, no texel. The block
reads nothing and produces no colour. It *chooses an id* and *folds a
coordinate*. Address generation, the mip level, the palette, the filter and
the format are `TEXTURE.TMU`'s; the per-primitive modulation (shade × layer-H
tint × sheet strength) is composed once upstream and never enters here; the
group restriction of §6.3 is not implemented (chosen law C1 below).

## Law FOUND versus law CHOSEN

**FOUND (ratified 2026-08-16, frozen, capture-exact — this block obeys it
bit-for-bit; it is REFERENCE_COMPLETE, so the oracle is the answer and not a
guide):**

* the mirrored-repeat texel fold — `m = floor(u × 64)` as the arithmetic
  shift `u_raw >> 10`, `per = m mod 128` floored, `texel = per < 64 ? per :
  127 − per` (spec/terrain_rules.md §6.2, `zref::terrain::mirror_texel`);
* the stable world-space pick — `h = (u32(tx) · 73856093) XOR (u32(ty) ·
  19349663)`, `p = h mod 255`, `pick = (p < weight) ? matA : matB`, on the
  **unfolded** world texel indices (§6.2, `zref::terrain::mosaic_pick`);
* that the two COMPOSE from one word: `raster_tri`'s TextureSpan branch calls
  the fold on `u`/`v` and the pick on `u >> 10`/`v >> 10`, i.e. on the same
  quantity `mirror_texel` computes internally (reference/src/zrender/rast.cpp);
* that the products WRAP mod 2³² before the XOR (`uint32_t` in the oracle);
  the oracle's own random lane records disagreeing on 25 % of samples when a
  reimplementation widened them;
* that the world index is SIGN-EXTENDED into the multiplier, so terrain west
  or north of the world origin has its pattern decided by a negative index;
* that `mosaic` is a real per-span input and not a convenience:
  reference/src/zrender/terrain.cpp builds three `TextureSpan`s — tops pick
  per texel (`span.mosaic = true`), the rim walls are always tile 240 and the
  underside is always tile 241 (`false`), and **all three still fold**
  (terrain_rules §5 "mirrored repeat", §6.6 frozen id assignments);
* the ledger's shape: `latency: fixed:2`, ready/valid, one pick per clock,
  counter `texture_samples`, `source_ids: true`.

**CHOSEN**, each with the alternative that was rejected. The RTL header
carries the same four with the same numbering.

* **C1 — no transition-group restriction port.** §6.3 and this contract's own
  Purpose say Mosaic *may* restrict the pick to a group's members. "May" is a
  permission, and the ratified oracle settles it: `mosaic_pick` takes five
  parameters and none of them is a group. Since the block is
  REFERENCE_COMPLETE, a group port would be hardware no differential test
  could ever exercise. *Rejected:* carrying the 256 B group table (16 × {count
  u8, detail u8, members u8[14]}) and searching it per texel — an M10K page
  and a 14-way compare in the per-texel path, and `p` stops meaning "a
  residue" and starts meaning "an index into a variable-length member list",
  with no committed capture able to tell right from broken. When §6.3 is
  ratified into an oracle it arrives as a new input port and a second
  `pick_tile_o` term; nothing in this block has to move.
* **C2 — the folded texel pair is an output**, even though `TEXTURE.TMU`'s
  `WRAP_MIRROR` (which is §6.2's fold generalised to any power-of-two size)
  reproduces it exactly from the raw UV. Emitting it makes the whole of this
  block's stated reference function observable at its own boundary. *Rejected:*
  emitting only the tile id — twelve fewer wires, and `mirror_texel`, half the
  Scalar reference function below, would have no port at which it could be
  checked at all; it would have to be inferred three blocks downstream through
  a cache and a palette.
* **C3 — a rigid two-stage pipeline**, which is what makes `latency: fixed:2`
  true rather than typical. Stage A is the pair of 32-bit constant multiplies
  and the XOR; stage B is the mod-255 fold, the compare and the select. Every
  stage advances together or none does. *Rejected:* one combinational stage
  with a skid buffer — latency would be 1, or 2 only when the skid was
  occupied, so the ledger line would be a lie; and it puts the multiply, the
  fold and the compare in one path, the deepest path in a block that must keep
  up with one fragment per clock.
* **C4 — `pick_tile_o` is a tile id, not an address.** Turning an id into a
  base address needs the tileset base and the 5,461 B/tile mip stride
  (terrain_rules §6.1), which belong to `TEXTURE.CACHE` and `TEXTURE.TMU`.
  *Rejected:* an address adder here, which would put a second copy of the tile
  stride in the tree and violate charter §29-6.

**EXPLICITLY NOT DECIDED anywhere, and therefore not implemented:** §6.3's
group restriction (C1). This is recorded rather than silently skipped, in the
shape design/contracts/SURFACE.SHEET.md set for terrain_rules §11.

## Clock and reset semantics

Single clock `clk` (`clock_domain: gpu`). Asynchronous active-low reset
`rst_n`, synchronously released; every register — both pipeline stages, both
valid bits and the counter — is initialised in the reset arm. There is no
second clock, no gating and no CDC: the block has no memory port and no
external interface other than its two ready/valid channels.

## Input and output packet layouts

### `mosaic_candidates` (in, ready/valid)

| field | width | meaning |
|---|---:|---|
| `req_u_i` | 32 (signed) | Q16.16 TILE units. One tile period per cell on tops, per STRATA_M on walls/underside (§6.2/§6.6). |
| `req_v_i` | 32 (signed) | as `req_u_i`. |
| `req_mat_a_i` | 8 | layer-E matA — `TERRAIN.PROJECT`'s `out_mat_a_o`, forwarded and never selected. |
| `req_mat_b_i` | 8 | layer-E matB. |
| `req_weight_i` | 8 | layer-E unit8 weight. |
| `req_mosaic_i` | 1 | `TextureSpan::mosaic`. Low = a wall/underside span: tile is matA, the fold still runs. |
| `req_src_id_i` | 16 | `source_ids: true`. |

`req_u_i[9:0]` and `req_v_i[9:0]` — the sub-texel fraction — are DISCARDED by
law: §6.2 floors, and rounding would move the mirror turn half a texel and
shift every committed capture. They are sunk explicitly in the RTL rather than
lint-waived.

### `mosaic_pick` (out, ready/valid)

| field | width | meaning |
|---|---:|---|
| `pick_tile_o` | 8 | `zref::terrain::mosaic_pick(matA, matB, weight, u>>10, v>>10)`, or matA when `req_mosaic_i` is low. |
| `pick_tx_o` | 6 | `zref::terrain::mirror_texel(u)` ∈ [0,63]. |
| `pick_ty_o` | 6 | `zref::terrain::mirror_texel(v)` ∈ [0,63]. |
| `pick_src_id_o` | 16 | echoes `req_src_id_i`. |

`idle_o` is high when neither stage holds a valid packet.

## Backpressure rules

Ready/valid on both channels, with the house hygiene rule: `req_ready_o` and
`pick_valid_o` are functions of registers and of `pick_ready_i` only, and
`req_ready_o` never depends on `req_valid_i`.

The pipeline is RIGID. `advance = !b_valid_r || pick_ready_i`, and
`req_ready_o = advance`: when the consumer stalls with a valid result held,
the whole block freezes. Nothing is dropped, nothing is reordered, no bubble
is squeezed out. A stalled `TEXTURE.TMU` therefore backpressures straight
through to `RASTER.FRAGMENT`, which is the intended shape — this block has no
buffer and is not a place to put one.
ENFORCED-BY: tests/texture/texture_mosaic_directed.cpp:test_rtl_handshake
(four stall patterns, the stream bit-identical under each).

## Memory ownership

**None.** No VRAM port, no cache port, no M10K, no ROM, no table. The block
holds 2 × ~50 flip-flops of pipeline and one 32-bit counter. This is the whole
reason it can sit between `TERRAIN.PROJECT` and `TEXTURE.TMU` without
consuming any of the tile budget group's memory (spec/memory_rules.md is not
engaged by this block at all).

## Q formats and rounding

There is **no rounding anywhere in this block**, and that is a law rather than
an omission (spec/qformats.md §3's single-rounding rule is satisfied
vacuously):

* `u`/`v` are Q16.16 (spec/qformats.md §2 `fx16`). The only operation applied
  to them is `>>> 10`, an arithmetic shift, which is §6.2's stated FLOOR. No
  `rescale`, no round-half-up, no saturation.
* the hash is exact modulo 2³² — the wrap IS the law (see FOUND above).
* `p = h mod 255` is exact, computed by the byte fold in
  `zhao_texture_mod255` (2⁸ ≡ 1 mod 255), not by a divider and not by a
  reciprocal multiply. `p ∈ [0,254]` always, which is what makes weight 255
  mean "always matA": a residue can never reach 255. That range is PROVED, not
  sampled — see Formal properties.
* the compare is unsigned 8-bit and strict (`p < weight`), matching the
  oracle's `<`. Weight 0 therefore selects matB everywhere.

## Latency (fixed or variable)

**Fixed 2 clocks, measured.** A packet accepted on clock N is presented on
`pick_valid_o` on clock N+2, unconditionally, because the pipeline is rigid
(C3) and has no data-dependent path. Under a stalling consumer the *advance*
stops, so the fixed latency is stated in advancing clocks; the first packet's
accept-to-retire distance is asserted to be exactly 2 with a free-running
consumer.
ENFORCED-BY: tests/texture/texture_mosaic_directed.cpp:test_rtl_handshake
(`RTL: accept-to-retire is exactly 2 clocks (ledger fixed:2)`).

## Target throughput

**Met literally: one pick per clock, measured.** With the consumer always
ready the block accepts on 512 consecutive clocks and retires on 512
consecutive clocks; the accept-to-last-retire window is N + latency = 514 for
N = 512, and any bubble makes it larger. The measurement is asserted, not
claimed.
ENFORCED-BY: tests/texture/texture_mosaic_directed.cpp:test_rtl_handshake
(`512 picks, 514-clock window`).

At one fragment per clock the block keeps pace with `RASTER.FRAGMENT`'s peak.
It does NOT keep pace with `TEXTURE.TMU`, which is one sample per four clocks
(direct) or six (CLUT) in its present unpipelined shape — that shortfall is
recorded in design/contracts/TEXTURE.TMU.md and is not this block's to fix.

## Overflow and malformed-input behaviour

**There is no malformed input.** Every bit pattern on every port is a legal
packet, and the block is total over its whole input space:

* every 32-bit `u`/`v`, including `INT32_MIN` and `INT32_MAX`, has a defined
  fold and a defined hash — the shift is arithmetic, the multiply wraps, and
  neither can trap;
* every weight 0..255 is legal, and the two extremes are the authored way to
  say "one material" (§6.2);
* the block has no mode word, no reserved bits and no state, so there is no
  `mode_error_o` analogue to raise. `TEXTURE.TMU` needs one because it decodes
  a 32-bit mode word with reserved bits; this block decodes nothing.

Nothing overflows: the widest intermediate is a 32-bit product whose truncation
is the law, and the residue network's own bound is proved (below).

## Counters and traces

`texture_samples_o` (32-bit, saturating at `0xFFFF_FFFF`, cleared only by
reset) counts **retired** picks — `pick_valid_o && pick_ready_i` — so a
stalled consumer never double-counts one and the total equals what
`TEXTURE.TMU` actually received. `source_ids: true` is honoured by
`pick_src_id_o`, which rides the packet through both stages.

The counter name is shared with `TEXTURE.TMU`'s (`counters: [texture_samples]`
in both ledger entries). They count different events on different blocks —
picks here, samples there — and the catalog will need to disambiguate them
when both are wired to `DEBUG.COUNTERS`. Recorded here rather than discovered
there.

## Scalar reference function

`zref::terrain::mosaic_pick` + `zref::terrain::mirror_texel` (header-only,
reference/include/zref/zref_terrain.hpp; deep-keel wave): the §6.2 frozen
laws verbatim — the mirrored-repeat fold (m = floor(u·64), floored mod 128,
texel = per < 64 ? per : 127 − per) and the stable world-space pick
(h = u32(tx)·73856093 XOR u32(ty)·19349663 — the u32 products WRAP before
the XOR; p = h mod 255; pick A iff p < weight). The renderer samples
through these in raster_tri's TextureSpan path (one primary CLUT8 sample
per fragment, per-texel pick, §6.2 constants frozen capture-exact).

The RTL is differentiated against **both**, composed the way `raster_tri`
composes them, through `tests/texture/texture_mosaic_dev.hpp::oracle` — which
calls the ratified header and never re-derives it.

## Directed tests

`tests/texture/texture_mosaic_directed.cpp` (deep-keel wave): fold anchors
at every wrap boundary (mirror turn, period-2 return, negatives, the
cell-border seam), pinned p values with BOUNDARY weights (one LSB of
constant drift inverts them), weight extremes, the rendered dither (both
tile families in one cell at weight 128, zero colours outside the
families), and the exact modulated pixel through the replicated resolve
(shade ladder × layer-H tint, hand-computed).

RTL lanes added 2026-08-19, in the same file:

* **fold, exhaustive** — all 128 residue classes at four whole-period offsets
  on both sides of zero, each with a non-zero discarded fraction, plus the
  turn (per 63 and per 64 both fold to 63) and the exact 2-tile period.
* **pick at the frozen anchors** — the same four hand-computed p values, driven
  through the block with BOTH sides of the exact-equality boundary
  (`w = p` picks B, `w = p+1` picks A) and both weight extremes.
* **mod-255 boundaries** — a NON-ZERO h whose residue is 0 (the single input
  the fold's conditional subtract exists for) and residue 254 (the top of the
  compare's range), both CONSTRUCTED by search rather than waited for.
* **fold-only spans** — `req_mosaic_i` low across 64 weights: candidate B never
  wins at any weight, and the fold still runs (§5 / §6.6, tiles 240/241).
* **handshake, latency, throughput, counter** — as cited under Latency,
  Target throughput and Backpressure above.

## Randomized differential tests

`tests/texture/texture_mosaic_random.cpp`: 100k random (tx, ty, weight)
picks vs an independent wrapped-u64 oracle (the wrap is the law — the
oracle originally skipped it and disagreed on 25 percent of samples);
fold range/period-2/1-Lipschitz laws over 100k coordinates; stateless
sweep checksums.

TWO RTL differential lanes added 2026-08-19, because one distribution cannot
be both:

* **lane G (gameplay-shaped)** — 40k packets (400k nightly): 32-cell patch UV
  at plausible world origins with a real interpolated sub-texel fraction;
  authored weights biased to the extremes with a graded-transition arm; one
  span in eight a wall/underside; tile ids in the authored range. Stalling
  consumer.
* **lane L (domain limit)** — 40k packets (400k nightly) uniform over the whole
  int32 UV domain, PLUS the 8×8×3 rail grid (`INT32_MIN`, `INT32_MAX` and
  their neighbourhoods), PLUS **510 CONSTRUCTED boundary packets**: for every
  residue 0..254, a world texel producing it, driven at `weight == p` and
  `weight == p + 1`.

Both lanes COUNT their interesting states and **assert the counts**: both
halves of both folds, both texel rails, negative world indices, both weight
extremes, the fold-only spans, both candidates winning, residue 0 and residue
254. This is not decoration — a uniformly random weight lands on `p == weight`
about once in 255 draws in one direction only, and that pair is exactly what
separates the law's `<` from `<=`. Measured on the committed seeds: lane G
reaches the boundary 94/69 times, lane L 401/386 times.

## Formal properties

`tests/formal/texture_mod255.sby` + `texture_mod255_fv.sv` —
**`zhao_texture_mod255` proved TOTAL over all 2³² inputs**, not bounded. The
DUT is the exact module the mosaic instantiates; `h` is 32 free bits, which IS
the port width and IS the width of §6.2's `uint32_t` hash, so the reachable
set is the entire input space and depth 2 is the whole state space rather than
a horizon (the module is one `always_comb`).

* **P1 `a_is_the_residue`** — the output IS `h mod 255`, stated *without a
  divider*: a free 25-bit `q` and 8-bit `r` witness the decomposition
  `h = 255·q + r`, `r < 255` is assumed, and the block's answer must equal `r`.
  Because that decomposition is unique the assumption pins `r` exactly, and the
  property contains only multiply, add and compare — it is not a second
  implementation of the fold.
* **P2 `a_in_range`** — unconditionally `p ≤ 254`. This is the fact
  `mosaic_pick`'s "weight 255 = always matA" rests on. An 8-bit port makes
  `p ≤ 255` a tautology; `p ≤ 254` is a theorem about the conditional subtract.
* **P3 `a_zero`** — `h = 0` gives 0 (the wrong fold with an unconditional
  final subtract dies here first).

The cover task is load-bearing, because P1 is guarded by an assumption: it
demands the witness be satisfiable at `r = 0` with `h ≠ 0` (the correcting
case), at `r = 254`, at `r = 1`, at `h = 0xFFFF_FFFF` (255 × 16,843,009,
where the byte sum reaches the 255 rail), and at both ends of `q`'s range.

**Not proved formally, stated plainly:** the two frozen multiplier constants,
the XOR, the arithmetic shift, the mirrored fold, the `p < weight` compare,
the pipeline, the handshake and the counter. Those are covered by the
differential lanes and by the mutation evidence below. What is proved is the
one piece of arithmetic in the block a reviewer cannot check by eye.

## Synthesis / resource ceiling

Written to the conservative synthesizable subset (charter §2): no indexing of
a function call's result, no dynamic part-selects, no loops without constant
bounds, no arrays on the boundary. Verilator `-Wall` clean on both modules
(`lint_texture_mosaic`, `lint_texture_mod255`).

Expected cost, by inspection rather than measurement — **this block has NOT
been through the Quartus per-block sweep at the time of writing, so nothing
here is a synthesis claim**: two 32-bit multiplies by frozen constants
(73,856,093 = 0x0466_9D9D and 19,349,663 = 0x0126_75DF, both reducible to
shift-add trees since neither operand is variable), a 4-input 8-bit adder tree
plus two narrow folds, six XOR gates per axis for the mirror, one 8-bit
comparator, an 8-bit 2:1 mux, ~100 pipeline flops and a 32-bit saturating
counter. No M10K, no DSP requirement, no ROM.

## Integration capture cases

The block's law is capture-exact by ledger note, and the captures that pin it
already exist: `tests/render/render_golden.cpp` and the `terrain-orbit` /
`terrain-breach` reel subjects render the §6.2 dither through
`zref::terrain::mosaic_pick`. Those goldens pin the ORACLE; this block is
differentiated against the same oracle packet-for-packet, so a drift here
turns the differential lanes red before it could reach a capture.

Not yet composed with a real `TEXTURE.TMU` in one executable: the seam
(`TERRAIN.PROJECT` → here → `TEXTURE.TMU`) needs the TMU's cache model wired
alongside, which is a composition test of the shape
`tests/terrain/terrain_project_chain.cpp` has and is NOT written. Recorded as
missing rather than implied.

## Mutation evidence

Three mutations, one per build, each proved to have RELINKED by the SHA-256 of
the test binary (a hash that did not change means the mutation did not run).
Clean binaries: `test_texture_mosaic_random.exe` a38c01a6…, directed 991ce99d…

| # | mutation | random.exe SHA-256 | caught by |
|---|---|---|---|
| M1 | `p_c < a_weight_r` → `p_c <= a_weight_r` | bc2b29f0… (directed c5fe4e1f…) | directed lane 5 by name (`w == p picks B`) and lane 4; **both** random lanes |
| M2 | `(s2 >= 9'd255)` → `(s2 > 9'd255)` in the residue fold | b8d3cd97… (directed f88a8b29…) | directed lane 6 by name (`residue 0 straddles w=0/w=1 exactly`); both random lanes; **and the formal lane, which failed in 4 s** |
| M3 | `per_u[5:0] ^ {6{per_u[6]}}` → `per_u[5:0]` (the mirror) | a670194b… (directed 90319d2c…) | directed lane 4 by name (`the mirror turn — per 63 and per 64 both fold to texel 63`), at index 64, i.e. exactly the turn; both random lanes |

M1 and M2 are the two the uniformly-random lanes could NOT be relied on to
catch — both live on an exact-equality boundary that random traffic reaches
only by accident — and both are caught by directed cases that were
CONSTRUCTED for them. M2's formal failure is the evidence that P1 is not
vacuous.

## Notes

Determinism of the pattern is a capture-exact requirement.
