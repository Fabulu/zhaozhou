# SPEC v1: Zixxtrixx v8 closeout and thick outlines

**Run ID:** RUN-20260828-2234
**Created:** 2026-08-28 22:34 UTC+02:00
**Status:** Active
**Previous Version:** v7 handoff in `RUN-20260828-1939-zixxtrixx-v7-six-faults/`

---

## Objective

Produce a complete, reproducible, published Zixxtrixx v8 pass: a strong dedicated modelling agent re-audits and finishes every outstanding job in form, rigging, animation, design, texturing and presentation; all canonical and isolated experiment videos/posters are present, including normal+thick-outline and cel3+thick-outline; the site is assembled and deployed; shipping output remains deterministic; all model/golden/sequence gates are green; evidence is committed and both repos are pushed. Publication is deliberately two-stage: first publish the completed normal canonical set and initial experiment menu, then and only then build a complete cel3+thick alternate of every canonical clip behind one collection tab, commit/push again, and publish a second time.

---

## Scope

**In Scope:**

- Re-audit all 17 owner-queue items visually, not only mechanically, and finish anything that does not actually read as complete.
- Finish any remaining head/neck form, systemic rigidity, salto-family, fall, impact, taunt, fin, eye/mouth, and skinning/model defects in the owner priority order.
- Verify completion of the inherited v7 experiment factory.
- Add separate render-side normal-shading + thick-outline and faceted cel3-shading + thick-outline variants; neither may substitute for the other.
- Keep a clearly separate smooth-toon3 + thick experiment if it reads distinctly at 240p: interpolate coherent shared-normal light first, threshold per fragment second. Never replace or relabel the required faceted cel3+thick output.
- Expand the experiment menu creatively beyond the inherited isolated effects, keeping each variant's visual axis intelligible and preserving fixed creature pigments.
- Use the existing contour atlas for interior ink where it serves the read; author additional deterministic pages/post effects when a genuinely distinct treatment needs them.
- Raise bestiary tab capacity and all three CSS selector families together as required.
- Regenerate sequence CRCs, restore the shipping page/atlas preview, assemble the site, and visually inspect experiment output at final 384x240/site-camera presentation.
- Run probe, choreo, planner, strike-tip, meshcheck, golden comparison, and `zhao-reel --check` redirected to a file.
- Commit evidence, make logical commits, push both repositories, then publish the finished normal canonical set plus initial experiment menu to `upheaval.pages.dev` (first publication).
- **Task #8, blocked by that first publication:** generate cel3+thick-outline versions of every canonical animation, expose the whole alternate set behind one extra experimental collection tab, preserve gameplay/site-distance comparison evidence, commit/push the second delivery, and publish production a second time. The complete alternate set must not be folded into the first publication.

**Out of Scope:**

- Sacengine execution or hardware redesign.
- Unrelated FPGA lane working-tree changes.

---

## Constraints

- Side.png owns form; Front.png owns marking placement.
- Art decisions are made by rendering and looking; measurements only compare.
- Shipping creature output must remain byte-identical through all experiment variants.
- Outline thickness is a render-side silhouette radius so it stays screen-consistent through distance and mips.
- Effects remain isolated rather than pre-mixed, except explicitly requested combinations.
- Preserve independently selectable presentation styles through ONE shared renderer: normal, faceted cel3+thick, and the separate smooth-toon experiment. Geometry compilation, animation, skinning, visibility, projection, texture sampling and camera logic stay common; only lighting quantisation, contour-page selection and the screen-space silhouette branch. Do not fork or rebuild the creature renderer as divergent codebases.
- The normal style is the default and remains byte-identical; experimental style selection never overwrites the shipping atlas or shipping outputs.
- Build only with the run/direct scripts, never `cmake --build`.
- Goldens compare against `Upheaval/creature/Zixxtrixx/golden/`, never staging.
- Sequence check stdout is redirected to a file.
- Stage explicit paths only; never broad-add raw frame workdirs.
- Publish only after the full first-stage job is finished and verified, using `Upheaval/website/deploy.ps1 -Project upheaval -Branch main`; retain `noindex`.
- Publication order is binding: normal canonical set + initial menu first; full all-clips cel3+thick collection only after that production state is live, in a separate commit/push/deploy cycle.

---

## Don't Retry

- Do not bake thick silhouette into the atlas: it shrinks at distance and mips away.
- Do not use even blur kernels in texture experiments; the tooth page broadcast-crashes. Helpers already force odd kernels.
- Do not relink `zhao-reel.exe` while a background renderer holds it; Windows leaves the old binary in place after link failure.
- Do not infer ink presence with exact RGB equality after dither; compare against a no-experiment frame.
- Do not run site assembly before every manifest source exists; it intentionally hard-errors.
- Do not stage the known unrelated RTL/field sweep/capture changes.

---

## Open Questions

None blocking. Default judgement: use contour page interior ink unchanged, radius 5 for the thick post-process silhouette, and three bands for the thick cel variant; revise only if the site-camera render reads wrong.
