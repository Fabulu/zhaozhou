# SPEC v1: Zixxtrixx v14 top-diagonal lighting modes

**Run ID:** RUN-20260830-1137
**Created:** 2026-08-30 11:37 UTC+02:00
**Status:** Active
**Previous Version:** v13 `Corrected Toplight 1` / source main `c954ad63c0b251bb3525d0ef0ef4911894af8dec`

---

## Objective

Preserve the corrected outward-normal renderer and the pleasing v13 cel look,
then author exactly ten genuinely different, fixed world-space top-diagonal
lighting modes. Every mode uses the same held signature-S and slow ten-second
360-degree view-only orbit. Most body and side planes must remain readable while
directional shape survives. Publish the completed experimental comparison once
on the exact-noindex bestiary and stop for owner feedback.

---

## Scope

**In Scope:**

- Exactly ten named, editable `CreatureLightRig` variants with clearly diagonal
  upper-hemisphere keys and coherent fill/ambient response.
- One existing common 600-frame held-pose orbit rendered under each variant.
- Native 384x240 visual comparison, author-look-adjust loops and one final
  ten-option comparison presentation.
- Ten VP9 WebMs and posters, manifest integration, exact noindex, one deployment.
- Logical commits/pushes, latest-main integration, cleanup and stopped-job proof.

**Out of Scope:**

- Reopening v13 normal orientation, light-vector semantics or transform diagnosis
  absent a new concrete contradiction.
- Geometry, proportions, pose, animation assets, pigments, texture, toon
  thresholds, outline, framing, renderer architecture or hardware paths.
- Spring implementation, probes, renders or catalogue regeneration.
- Broad regression suites and unrelated FPGA/test work.

---

## Constraints

- Fresh ordinary clones in
  `C:/programmieren/zencrifice/zixxtrixx-v14-diagonal-lighting-lane`; no worktree
  and no shared checkout changes.
- Direct lane-local reel compilation only. Never `cmake --build` or Sacengine;
  rebuild every dependent TU after declaration/layout changes.
- Never `git add -A`; stage exact paths and push logical milestones as they land.
- The ten options are authored by eye, not generated as a mechanical parameter
  sweep. Numeric checks describe the authored result but never choose it.
- Keep light vectors in the v13 surface-to-source world convention and leave the
  orbit view-only.
- Retain v13, rejected v12, v11, every earlier generation and every archive.
- Deploy exactly once with
  `website/deploy.ps1 -Project upheaval -Branch main`.
- No spring work.

### Bounded validation budget — `V14-DIAGONAL-MODES-1`

| ID | Acceptance question | Bounded evidence |
|---|---|---|
| V14-SOURCE | Are there exactly ten new rigs, each clearly top-diagonal, named/editable and fixed in world space? | Source/value audit plus unchanged view-only orbit trace |
| V14-VISUAL | Does each mode keep most body/sides readable while retaining a directional diagonal key? | Native quarter-turn/all-frame sheets and complete playback of these ten identical orbits only; reauthor by eye if needed |
| V14-MEDIA | Are the ten final assets exactly 600 frames, 60 Hz, 384x240 and ten seconds? | `ffprobe` all ten and complete decode of the final set |
| V14-WEB | Is the ten-option experiment usable while v10 initial selection, history and exact noindex survive? | Static manifest/media check plus one desktop and one 390px browser pass locally and after deployment |

Stop when these four questions are answered. No catalogue render, broad creature
suite, exhaustive unrelated clips or renewed renderer diagnosis without a new
specific failure signal.

---

## Don't Retry

- V12 trusted comments and two agreeing inward lanes; v13 proved and fixed both.
  Preserve that repair rather than flipping rig vectors or normal signs.
- V13's key read nearly vertical in the final image and left most sides shaded.
  Do not answer by raising ambient until direction disappears or by another
  near-zenith key with a diagonal label.
- Do not derive artistic values from measurements or construct a ten-step numeric
  sweep. Name a lighting mode, author it, render it, look, and adjust.
- Do not let media-heavy `networkidle` gate the site; use DOM content plus bounded
  candidate metadata waits.
- Do not leave renderer, encoder, browser, server, Wrangler or build children
  alive after stopping their parent command.

---

## Open Questions

None. Artistic choices remain deliberately reversible through named constants.
