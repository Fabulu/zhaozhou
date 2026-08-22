# QUEUE — representative motion as GIFs, on the site

*Orchestrator brief, 2026-08-16. Requested by the project owner: "If we get representative video, let's turn it into gifs and put it on the site."*

## Why GIF is the right format here, not a compromise

The console is **CLUT8 — a 256-entry palette**. GIF is a 256-colour palette format. The mapping is exact: a captured frame's indices become GIF indices with **no quantisation and no dithering**. Animated CLUT effects (Noctis sun ramps, beam falloff, the 2-bit ramp selector + 6-bit intensity discipline) reproduce as the hardware means them. This is one of the rare cases where the lo-fi format is the *faithful* one.

## The hazard — do not let ffmpeg re-quantise

The common recipe (`palettegen` → `paletteuse`) **builds a new palette from the pixels and dithers to it**. That destroys the CLUT discipline: ramps get dithered, index identity is lost, and the output no longer shows what the hardware produces. It would look "fine" and be wrong — the exact failure mode this project keeps having to unwind.

**Required recipe instead:**
1. Export the frame set's own CLUT as a 16×16 palette PNG (256 entries, one pixel per entry, in index order).
2. Encode with that palette supplied and dithering off:
   `ffmpeg -framerate <fps> -i frames/%04d.png -i clut.png -lavfi "paletteuse=dither=none" -loop 0 out.gif`
3. **Verify, do not assume:** decode the GIF back and assert the index/RGB values match the source frames exactly. If any pixel differs, the encode is wrong. A per-frame CRC comparison against the capture is the natural check and fits the repo's existing CRC-32C habit.

Each subject has its own palette — do not share one CLUT across subjects.

## Prerequisite

The reference renderer must emit **frame sequences**, not just single stills. `tools/capture` and `reference/src/zref_frame.cpp` exist; establish what sequence capture already does before writing new code (charter §29-6 — never implement semantics twice). Frames must be produced deterministically so a GIF regenerates byte-identically from the same capture.

## Subjects — motion earns its place

Current stills: `sky-dusk`, `island-terrain`, `field-crater`, `duo-frame` (each also at native res). Ranked by what motion actually adds:

1. **`field-crater`** — the money shot. This is a *deformation* engine and a still cannot show deformation. The stamp landing, the terrain responding, debris shedding.
2. **`sky-dusk`** — a sun sweep with the Noctis lens flares; ramp animation is the whole point and is invisible in a still.
3. **`island-terrain`** — a slow orbit around a floating island, showing **sky below the horizon**, cliff strata, and the rim. Sells the identity in a way no static frame does.
4. **`duo-frame`** — both views updating together, proving the packed two-block layout live.
5. **beams** (new subject) — god beams sweeping.

## Discipline

- **One canonical GIF per subject, overwritten, never accumulating** — same rule as the stills.
- Keep page weight sane: cap at roughly 48–96 frames, loop cleanly. State the actual byte size per GIF; if a subject is too heavy, shorten the loop rather than re-quantising it.
- Keep the still alongside the GIF — stills stay useful for detail, and load instantly.
- Regenerate and redeploy the Pages site (`zhaozhou-site/`, live at https://zhaozhou.pages.dev) when a subject changes substantially.

## Sequencing

Blocked on the `wp/w2.2-video` merge landing. Then, in the queue order already set: W3.3 relaunch → W2.7 → **star gamut + lens flares** (which is what makes `sky-dusk` worth animating) → terrain at the new dual-heightfield format (which is what makes `island-terrain` and `field-crater` worth animating) → capture sequences → GIFs → site.

The site work itself may run as its own agent in `zhaozhou-site/` (the owner previously authorised a separate agent there, since it does not touch the console repo). GIF **generation** touches the repo and stays serial.
