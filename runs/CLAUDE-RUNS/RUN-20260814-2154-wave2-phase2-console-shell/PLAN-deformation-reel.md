# PLAN — the deformation reel (Tier 1), then the terrain-renderer wave (Tier 2)

*Orchestrator, 2026-08-16. Approved by the project owner: Tier 1 as the next dispatch after W2.7, Tier 2 as the wave behind it. Companion to `QUEUE-animated-gifs-and-site.md`, which holds the palette-exact GIF recipe and the hazard.*

## Why Tier 1 is cheap: the chain already exists

Verified present in-repo, 2026-08-16:
- `compiler/src/field_ir/crater_ring.ts` — a field program that deforms terrain (the FIELD.RING "annular distance falloff" shape, per `design/ops.yml:361-375`)
- `reference/src/zfield/zfield_interpret.cpp` — the field IR interpreter (one of the only two sanctioned implementations)
- `reference/src/zrender/terrain.cpp` — terrain rendering, with the heightfield-normal quantisation bug already fixed (`f7ad8d8`)
- `tools/capture/zhao_capture.cpp` — the capture tool
- ffmpeg 8.1 on PATH
- The existing static `field-crater` render proves the whole chain end to end

**The only missing piece is driving the field program across successive time values and dumping N frames instead of one.** That is a capture loop plus an encode — not new architecture.

## Tier 1 — the deformation reel (next dispatch, one agent session)

Deliver 2–4 animated subjects on the **current** single-heightfield renderer:
1. **Crater impact** — the strike, the annular wave expanding, the settle. The donor's most famous screenshot, in motion.
2. **Erupt-style theatre** — per S4's measurements: the wave plus polygon debris, catapult impulse, expanding stun ring, screen shake. Every garnish is an existing lane.
3. **Repeated/overlapping deformation** — several stamps landing in sequence so the terrain visibly accumulates damage. Shows the composed lattice doing its job.
4. *(if cheap)* a slow camera move over already-deformed ground, so the scarring reads.

**Requirements**
- **Deterministic**: the same capture must regenerate byte-identically. No host floats in the deterministic path (charter §29-7).
- **Palette-exact encode**: export the frame set's own CLUT as a 16×16 palette PNG; `ffmpeg -framerate <fps> -i frames/%04d.png -i clut.png -lavfi "paletteuse=dither=none" -loop 0 out.gif`. **Never `palettegen`** — it rebuilds the palette and dithers, destroying CLUT/ramp identity. Verify by decoding the GIF back and comparing against the source frames; a per-frame CRC-32C check fits the repo's existing habit.
- **One canonical GIF per subject, overwritten, never accumulating.** Keep the stills alongside.
- Cap ~48–96 frames; state actual byte size per GIF. If too heavy, shorten the loop — never re-quantise.
- Land the reel in `zhaozhou-site/renders/` (and `public/renders/`) beside the existing stills, then redeploy the Pages site (https://zhaozhou.pages.dev). The site-side embedding may run as its own agent in `zhaozhou-site/` — the owner authorised a separate agent there since it does not touch the console repo.

**Secondary value, not to be undersold:** an animated deterministic capture is a far better regression artifact than a still. Drift, popping, and instability across frames are invisible in a single frame. Consider wiring the reel's frame CRCs into the test lane as a cheap animation-stability check.

## Tier 2 — the terrain-renderer wave (behind Tier 1)

Move `reference/src/zrender/terrain.cpp` onto the **dual-heightfield island format** frozen this morning (`91006c2`): top + bottom surface, per-cell substance state, bake-time thickness/breach law, cell pitch {0.5,1,2,4} m canonical 2 m, cartridge kinds 6/7.

Unlocks, in owner-priority order:
1. **Breaches** — holes punched clean through an island, with debris falling into open sky. The thing a single heightfield cannot express.
2. **Rim edge-bites and undercuts** — true local thickness, FORGE.CLIFF strata.
3. **Bigger terrain, all polygons textured** — the zero-blend diet: one primary CLUT8 tile, mirrored repeat, authored transition tiles, tint moved to per-vertex so the single restricted aux stays with the surface sheet (§26's no-second-TMU never comes under pressure).
4. **Bore** — S4's crown finding. Sacrifice designed a spiral-mole spell that opens a hole and never shipped it; sacengine has nothing. Our breach law does it natively: spline scorch stamp + deepening bake + fall-through. **This is the definitive "more deformable than Sacrifice" demo** and should be the reel's headline once Tier 2 lands.

**Open item carried from S4, for the terrain owner:** `terrain_rules.md:410` promises "bounded lists per patch with bake/compose/reject on overflow" but **the bound's numeric value was never frozen.** Size it against the donor worst case — 8 wizards each holding an Erupt, footprint radius 90 m ⇒ a 3×3-to-4×4 patch footprint at 2 m pitch and 64 m patches — plus overlapping Quakes and a Volcano. Also missing: a cost-model line for "patches × lattice × transient programs per frame". Not costed by S4; do not invent a number.

## Sequencing

W2.7 (running) → **Tier 1 reel** → Tier 2 terrain-renderer wave → Bore demo → regenerate gallery, redeploy site. Noctis star gamut + lens flares slots in alongside Tier 2, since it is what makes `sky-dusk` worth animating. Serial discipline throughout: one repo-touching agent at a time; the site agent in `zhaozhou-site/` is the sanctioned exception.
