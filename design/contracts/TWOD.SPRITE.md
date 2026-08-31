# Contract — TWOD.SPRITE (HUD sprite engine)

> Ledger: `design/blocks.yml` · owner ZH-069 · phase 11 · maturity SPECIFIED

## Purpose and exclusions

HUD overlay sprites: descriptors, affine, CLUT+direct, text windows, heat maps; composited after glow/distortion.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons work in flight
and clears staging; no descriptor state survives a reset, because descriptors are
re-sent per frame.

## Input and output packet layouts
### The ruling is mostly a set of DELETIONS — owner 2026-08-31 §3.2

> HUD and overlay elements are **ordinary sprite descriptors using the primary
> TMU**. There is **no private HUD sampler and no special text rasterizer**.

    text     = glyph sprites
    windows  = tiled/stretched sprites or simple filled descriptors
    cursors  = sprites
    heat maps = debug sprites, or one of the restricted plane slots

**The game authors layout and text in software.** That single sentence removes a
font engine, a text layout engine, a glyph cache and their asset formats from
the console.

### Sprite descriptor

`{ x, y, w, h, u, v, format, palette_id, affine[4] or none, tint, blend,
   view_mask, order }`

The hardware supports **two player HUD regions** and large descriptor counts;
the regions are a compositing concern, not a second sampler.

## Backpressure rules
Ready/valid to the compositor; the TMU applies its own backpressure upstream.
Sprites are drawn in `order`, so a stall must not reorder them — the block holds
one sprite at a time and does not run them out of order to fill a stall.

## Memory ownership
**None of its own.** Texels come through the **primary TMU** — that is the
ruling — and descriptors arrive per frame. No private cache, no private sampler.

If this block ever grows a texel path, the ruling has been violated.

## Q formats and rounding
Positions and sizes in integer pixels; optional affine in fx16 with the same
nearest-sampling truncation as `TWOD.PLANE`.

Tint and blend follow the existing frozen blend law — this block introduces no
new colour arithmetic, and reuses `zhao_raster_blend`'s semantics so a tinted
sprite and a tinted triangle agree.

## Latency (fixed or variable)
Variable, dominated by TMU latency.

## Target throughput
One sprite pixel per clock, subject to the TMU.

A HUD is a small fraction of the screen. If sprite pixels ever approach the
world's pixel count, the HUD has become a second world and the budget assumption
behind this block is wrong.

## Overflow and malformed-input behaviour
| condition | behaviour |
|---|---|
| descriptor count over the frame budget | **drop the tail, deterministically by `order`, and count** — the HUD must not fault a frame |
| sprite entirely offscreen | not an error; culled and counted |
| unknown format | refuse the descriptor |
| zero width or height | refuse |

Dropping by `order` means the **least important** sprites go first, which is only
true if the game assigns order meaningfully — and that is worth stating so the
game side knows the contract it is relying on.

## Counters and traces
* `sprites_drawn`, `sprites_culled_offscreen`, `sprites_dropped_budget`
* `sprite_pixels`
* `descriptors_refused_by_reason[2]`

## Scalar reference function
`zref::HudSprites` (ledger `reference_model`). Owns descriptor interpretation, ordering, the drop rule and
the refusal taxonomy. It does **not** own sampling or blending — those are the
TMU's and the blend's, already frozen and proved.

## Directed tests
`tests/twod/twod_sprite_directed.cpp`.

* ordering: overlapping sprites composite strictly by `order`, and a stall
  mid-list does not reorder them;
* clipping at all four screen edges, and fully offscreen;
* CLUT8 and direct formats;
* **a tinted sprite matches a tinted triangle** of the same colour — the
  cross-check that keeps sprite blending from drifting from the world's;
* budget overflow: the tail drops by `order`, deterministically, and the same
  sprites drop on a repeat;
* both HUD regions in Duo, with a view-masked sprite in one only.

## Randomized differential tests
`tests/twod/twod_sprite_random.cpp`, RTL against `zref::HudSprites`.

Random descriptor sets with heavy overlap and deliberate budget overruns.
Report the drop mix; a sprite test that never overflows is testing half of it.

## Formal properties
`tests/formal/twod_sprite_order.sby`:

* **composited output order equals descriptor `order`** under arbitrary
  backpressure;
* a dropped sprite draws no pixels at all — never a partial sprite;
* handshake hygiene; reset clears staging.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,400 ALMs, 4 DSPs, ≤ 2 M10K.**

Small on purpose. **Zero texel storage** — a cache appearing here means a private
sampler has been built, against the ruling.

## Integration capture cases
* **a full two-player Duo HUD** — both regions, text as glyph sprites, at a
  realistic descriptor count.
* **a budget-overflow frame** — the tail drops, the frame completes, and the
  drop is repeatable.
* **HUD legibility over a bright world** — the ruling puts HUD after distortion,
  bloom, grading, ink and flash precisely so this is legible. Judge it against a
  flash frame, which is the worst case.

## Notes

Text/heat-map tiles via the TMU's CLUT path only — no private sampler.
