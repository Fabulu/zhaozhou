# QUEUE — the owner's accumulated ideas, their investigation status, and the fit-without-ruin rule

*Consolidated 2026-08-16 evening, at the owner's request: "make sure you've written all my new ideas down and investigate them so we can adjust the hardware so it fits them best without ruining what we have. Terrain effects and 3d character LOD and deformation are still highest priority."*

Also mirrored to persistent memory (`zhaozhou-game-identity-requirements`).

## Standing priority order

**1. Terrain effects. 2. 3D character LOD and deformation.** Everything else queues behind these two when they contend for the serial implementation lane. Recons and spec work for lower items may run in parallel (read-only allowance).

## Idea register

| # | Idea (owner's intent) | Status | Where it lives |
|---|---|---|---|
| 1 | Noctis suns, whole gamut, from space | **DONE (reference)** — 12 classes, 9 implemented + reels | `spec/stars_and_flares.md`, star_field/star_flare.cpp |
| 2 | Lens flares ("crazy good") | **DONE (reference)** — ghost chains, occlusion | same |
| 3 | Sky seam fix (was "two ovals") | **DONE** — elevation-ramp continuity law | `sky_and_beams.md` §1.2 + kBandRows 16 |
| 4 | Wibbly-wobbly rubbery terrain | **DONE (reference)** — wave math captured + implemented | FINDINGS-S5, `wave_pool`/`impact_wave` |
| 5 | Bore (spiral crumble, everything falls) | **SPEC'D + format supports it; demo pending** | FINDINGS-S5 §3, terrain_rules breach law |
| 6 | Massive volcano rising fluidly, spitting fire/rocks | **SPEC'D** (flatten term + stencil + rise law captured); demo pending | FINDINGS-S5 §2 |
| 7 | Effect library: all suns + terrain effects, screens for all, renderable by id | **IN PROGRESS** (task #15) | effects-library.yaml + zhao-reel --list |
| 8 | Clouds in front of sky/sun; god beams piercing through | **QUEUED** (task #16) — cloud layer already ratified | sky_and_beams.md §1.1 |
| 9 | Rain that darkens sky as clouds come in; spells cause weather | **QUEUED + spec questions written** | QUEUE-atmospheric-rain… (sky run dir) |
| 10 | Lighting: cheap impressive global + local, NO ray tracing, "cheat" | **RECON RUNNING** (S6) | — |
| 11 | Rotated terrain sheets (skyscrapers: 4 walls + top, deformable) | **QUEUED, feasible verdict written** | QUEUE-rotated-sheets-and-deep-keel.md |
| 12 | Deep textured keel (islands read solid, not flimsy) | **QUEUED — sequenced BEFORE #11** | same doc |
| 13 | GIF diary of development | **POLICY SET** (promotion rule) | zhaozhou-site/RENDER-POLICY.md |
| 14 | Character LOD + deformation (S2's whole study) | **SPEC'D** (mesh→micro-mesh→splat→glint collapse, pose tables, 2-weight skinning) — implementation is Wave 7 | creature_rules.md, GEOM.POSE |

## The fit-without-ruin rule (how hardware gets adjusted)

1. **Investigate before amending.** Every idea gets a recon or a spec pass before any frozen format/ISA/interface changes. The pattern that works: evidence → ratification doc (with alternatives considered and rejected) → struck-through superseded text, never silent rewrites.
2. **Prefer riding existing blocks.** Item 9 needed zero new hardware (overcast sky-set = asset + params). Item 11 rides the terrain-class-giant's specced capability. Item 10's recon is explicitly mapping cheats onto existing recipes/samplers.
3. **Amend only while young.** The terrain format took its breach law and no_bake corner-shadow amendments BEFORE the renderer depended on them; the reel CRC discipline caught anything that moved.
4. **Nothing that violates §26 refusals** — the recurring finding is that the donors' looks were achieved without the forbidden machinery (Sacrifice never sorted transparency; its holes were a void bit, not topology). "We cheat" is the native idiom.
5. **Never break the two crown jewels**: the composed console shell (Phase-2 gate, soak-proven) and the deterministic capture/replay law. Any hardware change must keep `reel_sequence_crc` green or re-pin deliberately with a loud note.

## Open investigation threads (what runs next)

- S6 lighting recon (running) → then an architect pass consolidating lighting into the spec the way the world addendum did for terrain/creatures
- Effect library completion (running) → then clouds/beams (#8), then rain (#9, shares sky-set crossfade machinery with #8)
- Character LOD/deformation: enter the serial implementation lane as Wave 7 (phases 8-9); its spec is done and waiting
- Deep keel + texturing before rotated sheets, per the sequencing in QUEUE-rotated-sheets-and-deep-keel.md
