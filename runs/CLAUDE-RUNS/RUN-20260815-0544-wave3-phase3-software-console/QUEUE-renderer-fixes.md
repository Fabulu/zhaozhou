# QUEUED RENDERER WORK (next repo agent)

*Assembled 2026-08-15 by the orchestrator. Three items: one ratified architectural change and two defects discovered by the site agent while producing gallery renders. All touch `reference/src/zrender/` + `reference/src/zsky/`, so they belong to ONE agent, after the current fixer finishes.*

## 1. Apply the Duo layout ratification
Full decision and rationale: `RATIFICATION-duo-layout.md` (same folder). Summary:
- `spec/video_rules.md` §1 mode table: relabel Duo — slot **allocation** `0x3C000` (sized for the largest canvas so mode switches don't reallocate) vs Duo **occupancy** `0x30000`. One clarifying sentence so §1 and §3.1 can't be read as contradictory again.
- Renderer Duo path → packed two-block layout (view0 `[0,0x18000)`, view1 `[0x18000,0x30000)`), each view contiguous. This is what the RTL already does; the renderer moves to meet it.
- `displayed_crc32c` must cover the full 512×240 displayed stream **including** the 48 black border rows (§3.1 says so explicitly); currently it CRCs rows 0..191 of a 512-wide canvas — wrong on both axes.
- Regenerate render goldens in the same commit; add a regression test pinning view1's first pixel to slot byte `0x18000`.

## 2. DEFECT — heightfield normal quantised to whole world-units² (patch shades black below ~1 m spacing)
`reference/src/zrender/terrain.cpp`, `draw_heightfield`: the shading normal is built with `rescale_s32(cross, 32)`. The cross product of two fx16 edge vectors is **Q32.32**, so a shift of 32 yields **Q32.0** — not the Q16.16 the surrounding comment claims. Every component rounds to 0 once grid spacing drops below ~1 m, the `nmag2 == 0` guard fires, and the **entire patch renders black**.

Demonstrated by the site agent: a 41×41 patch over a ±12 m envelope (0.6 m spacing) renders as a black silhouette; 25×25 over the same envelope (1.0 m spacing) renders correctly. Existing render tests only use 2×2 and 17×17 patches at ≥1 m spacing, so nothing catches it.

This will bite hard at Phase 6 (Mantle terrain patches are 32×32 cells over a world patch — finer than 1 m spacing by design) and it is exactly the shape of bug the adversarial reviews keep finding: correct-looking code, green tests, wrong math.

**Fix:** rescale by 16, not 32 (or normalise with the correct Q-format), fix the comment, and add a render test at sub-metre spacing that asserts the patch is NOT uniformly black.

## 3. DEFECT (latent) — sky `rot_proj` orientation can be exactly inverted and no test notices
Screen Y grows downward in the viewport map, so a naively-signed rotation-only matrix puts the **zenith cap at the bottom** of the frame and the under-plane at the top — an upside-down sky. The site agent's first sky render came out inverted for exactly this reason.

Every existing sky test passes on an inverted sky: they check layer census, UVs, scroll determinism, and "some cap pixels exist" — never which way is **up**.

**Fix:** add an orientation assertion — e.g. with an identity/neutral `rot_proj`, cap pixels must land in the upper half of the frame and under-plane pixels in the lower half. Cheap, and it pins a law that is currently only folklore.

## Note for gallery captions (not a defect)
The charter calls Duo's 48 lines "cheap 2D interface space"; in the reference implementation today they are 48 **black border** lines (24 top / 24 bottom) — real, part of the displayed CRC, but nothing is drawn into them yet. The site describes them as border scanlines, correctly.

---

## 4. DEFECT — 1px vertical cracks at every drum-column / grid boundary (user-reported, orchestrator-measured)

The user looked at the gallery renders and reported "weird lines" in all sky images and the island. Measured directly from the PNG pixels (orchestrator, 2026-08-15):

- **Full-height 1-pixel vertical lines.** `sky-dusk.png`: 6 of them at x = 105, 139, 174, 311, 342, 370. `island-terrain.png`: 7 at x = 21, 81, 105, 131, 185, 354, 370. Present on every sampled row (72 and 56 anomalies across rows sampled every 8th line).
- **They are BRIGHTER and WARMER than the surround**, e.g. neighbours `(132,77,107)` cool purple, line `(214,117,90)` warm orange, with the two neighbours nearly identical to each other (so it is a thin insertion, not a shading step).
- **Spacing ≈ 32–35 px** matches the sky drum's 48-column boundaries under the render FOV.
- **They are NOT the previously-fixed fill rule.** I regenerated all renders against `main` AFTER commits `57f1639` (fill rule), `2237766` (sun), `6cf33be` (marker), `b8db7e8` (static build) — the lines are unchanged, same count, same positions.

**Reading of the evidence:** the warm colour is almost certainly the sky-set background / fallback clear showing through, i.e. these are **cracks** (uncovered pixels) between adjacent drum column quads, not double-fills. Adjacent columns should share an edge exactly (column N's `a_r` and column N+1's `a_l` are the same expression), so suspect the shared edge is being dropped by both quads at certain sub-pixel positions, a T-junction/rounding mismatch in the projected vertices, or the seam falling between pixel centres.

**Required:** find the true cause (do not guess — instrument the rasteriser: for a failing x column, dump which primitives were tested and what the edge functions evaluated to), fix it, and add a **crack regression test**: render a full-screen drum and assert NO pixel equals the background colour inside the band's screen extent. The same class of test should cover the terrain grid, since the island shows it too.

## 5. FEATURE (user request) — terrain must scale much bigger and more complex
The current island render is a 25×25 heightfield capped at 1 m spacing *because* of defect #2 above (sub-metre spacing shades the whole patch black). Fix #2 first, then raise the render's terrain to a substantially larger and more detailed patch (target: at least 128×128 cells with sub-metre detail, multiple stacked field programs, visible relief). Keep it honest about cost — record triangle/fragment counts in the provenance block so the gallery shows what it actually costs.

## 6. FEATURE (user request) — sun lens flare
The user wants the sun to produce lens flares. Design it the way the sky/beams work was done: a short ratified spec section first, then implementation.
- It belongs to the **Mirror Gate compositor** (charter §16) alongside bloom/glow, NOT to the geometry path. Charter §26 refuses unrestricted render-to-texture graphs, so it must be one bounded, frozen-constant mode like the ratified `radial_decay` sun-shaft mode in `spec/sky_and_beams.md` §3.
- Classic fixed-function form: a chain of N sprites placed along the line from the sun's projected screen position through the screen centre, at fixed parametric offsets, each with a frozen size/tint/alpha from a constant table, additively blended, the whole chain scaled by the sun's visibility (reuse the per-beam DDA occlusion idea: if the sun is occluded, the flare fades).
- Deliverables: a spec section (frozen constants: element count, offsets, sizes, tints, falloff), a ZRef implementation, a directed test with hand-computed sprite positions, determinism (tick-exact, no host floats), and a cost line. Then a gallery render showing it.
