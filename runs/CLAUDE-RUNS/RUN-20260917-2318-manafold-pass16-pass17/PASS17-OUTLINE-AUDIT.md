# Pass 17 Direction 17 — production outline audit

Date: 2026-09-18
Branch audited: `manafold-pass17`
Scope: read-only audit of the current dirty working tree. No production source was edited.

## Verdict

The defect is a coverage-ownership and ordering error, not an art-value problem.

- The accepted upper/head-top line is produced from a **body-only auxiliary render**. Its initial selection is depth-correct: it inks only body pixels whose auxiliary body depth is still the full composed frame depth.
- The lower/inside antenna-stick edge has **no owner**. The ordinary contour grows only through viewport-connected exterior background, while the auxiliary internal contour admits only body pixels. Visible antenna pixels bordering the enclosed O opening satisfy neither path.
- The O is normally a zero bit in the full creature mask, but the flood-fill classifies it as **non-exterior** because it is enclosed. In topological terms the contour code treats it as inside the creature union. This is the precise meaning of “the O was treated as creature fill”; it should not be reported as though the opening itself were literally written as `mask[i] = 1` in every frame.
- The upper line can overpaint later, nearer energy geometry because its post-energy restore is an unconditional RGB write. The boolean was depth-approved before effects, but it carries no effect depth or final owner, and the later particle/effect passes deliberately do not update the shared depth buffer.

The narrow production repair is therefore:

1. preserve the existing body-only branch and its depth equality for the accepted top line;
2. make that branch depend on Manafold + cel-outline ownership, not on whether the shell happens to be enabled;
3. add the full union's **creature-side boundary against enclosed negative space** to the same initial ink mask, using the existing posed full mask and existing `ink_width`;
4. leave the O pixels untouched; and
5. remove the unconditional post-energy repaint. Paint internal ink once before effects and allow the already depth-tested later passes to occlude it only when they are actually in front.

No geometry, texture, palette, shell, mist, lightning, camera, animation, or ink-width value needs to change.

## Production ownership trace

### Model and type ownership

- `tools/reel/manafold_model.h:58-77` builds the body as one `RingPart` on `kBRoot`.
- `tools/reel/manafold_model.h:80-91` builds the antenna as one continuous chain part. It is a closed surface, but the projected space inside its loop is still negative space; closed tube caps do not make the projected O a filled face.
- `tools/reel/manafold.h:102-125` defines the full production type. Its ordered part list is body, continuous loop, two lenses, two white stars, and two cyan stars.
- `tools/reel/manafold.h:205-228` defines `body_type()`, a reel-only auxiliary containing only `make_body(kBRoot)` while sharing the full skeleton, clip bank, deformation, and page. This type was created specifically to recover the body/head boundary lost by a union silhouette.
- `reference/src/zcreature/creature_sim.cpp:768-785` initializes composition from the supplied RGB/depth frame; mesh triangles use ordinary opaque depth test/write (`reference/src/zcreature/creature_sim.cpp:928-956`), and the composed RGB and depth are copied back together at `reference/src/zcreature/creature_sim.cpp:1190-1191`.

### Pass-by-pass ownership

| Requested ownership | Current producer | Exact behavior |
|---|---|---|
| Pre-creature baseline | `tools/reel/zhao_reel.cpp:3216-3244` | Pre-compose mana splats run first. `pre` and `pre_depth` are then captured, so those splats cannot become creature outline coverage. |
| Full creature silhouette | `tools/reel/zhao_reel.cpp:3246-3274`, then `3316-3325` | `compose_creatures` draws the full visible type and writes nearest creature depth. `mask[i]` is set when RGB changed, or—under `g_cel_main`—when depth changed from `pre_depth`. This is the visible full-creature union mask. |
| Body-only auxiliary mask/depth | `tools/reel/zhao_reel.cpp:3283-3302` | The same instance, pose, camera, and LOD are recomposed with `u02::body_type()`. `u02_body_cover` is every pixel whose body-only depth differs from `pre_depth`; `u02_body_depth` retains the posed body depth. The current construction is incorrectly gated by `c.u02_shell` at line 3289, although it owns an outline, not shell optics. |
| Exterior silhouette ink | `tools/reel/zhao_reel.cpp:3400-3461` | A four-neighbour flood marks only non-mask pixels connected to the viewport border as `exterior`. An eight-neighbour ring then grows outward from all full-mask pixels through that exterior for the existing projected `ink_width`. It never writes a creature pixel and never enters an enclosed O. |
| Existing upper/head-top internal ink | `tools/reel/zhao_reel.cpp:3462-3497` | Candidate pixels must be in `u02_body_cover` and must satisfy `u02_body_depth[i] == depth[i]`. A nearer antenna therefore suppresses the body line at that pixel. A candidate becomes internal ink when a neighbour within the existing width is outside the body auxiliary but not viewport exterior. This is the accepted top line. |
| Lower/inside antenna O ink | **Unowned** | The exterior pass rejects the O because `exterior == 0`; the auxiliary pass rejects the lower/inside stick because its candidate pixel is antenna, so `u02_body_cover == 0`. No later pass discovers the full-mask-to-enclosed-hole boundary. |
| Initial ink paint and shared masks | `tools/reel/zhao_reel.cpp:3499-3517` | All current `edge` pixels are painted with the existing ink RGB. `edge` is added to `u02_cover` for mist exclusion and copied to `u02_ink` so the shell can spare the exact painted line. |
| Shell | `tools/reel/zhao_reel.cpp:3521-3538`; mechanism at `tools/reel/manafold_fx.h:669-702` | Shell paints after ink and before mana, using the same coverage and ink masks. It is allowed to straddle the line with its authored ink protection; it is not trailing mist. |
| Particle populations / lightning geometry | `tools/reel/zhao_reel.cpp:3541-3562` | Opaque, additive, and triangle populations draw after ink into a temporary `WorkSurface`. Only its RGB is copied back at line 3561. Particle points and triangles depth-test but do not write depth (`reference/src/zrender/sprites.cpp:71-100`, `154-188`). Thus a nearer particle can become the visible owner without appearing in the shared `depth` array afterward. |
| Creature-relative mist | `tools/reel/zhao_reel.cpp:3592-3665`; compositor at `tools/reel/manafold_fx.h:3594-3656` | Mist runs after the initial ink. It rejects every `u02_cover` pixel before its own depth test. The O remains eligible negative space and may contain haze; creature and ink pixels stay crisp. This is correct and should remain. |
| Smear/history plane | `tools/reel/zhao_reel.cpp:3681-3789`; compositor at `tools/reel/manafold_fx.h:3343-3389` | The code path still exists and depth-tests remembered effect depth against frame depth. It deliberately has no creature-cover exclusion. Live Manafold subjects currently set smear to zero, but this ordering must remain correct if the path is exercised by a diagnostic. It does not write shared depth. |
| Post mana splats / glow / folded energy | `tools/reel/zhao_reel.cpp:3791-3803`; primitive at `tools/reel/manafold_fx.h:3164-3217` | Each splat normally tests its projected centre depth against the composed surface and never writes depth. `lightning_push` routes every lightning layer through that policy and retains the same-binary force-through control (`tools/reel/manafold_fx.h:1714-1731`). |
| “Depth-approved” post-energy repaint | `tools/reel/zhao_reel.cpp:3805-3816` | Every earlier `u02_inner_ink` bit is rewritten to ink RGB with no current depth, coverage, or owner test. The term “depth-approved” describes only the earlier body-vs-full-creature selection; it does not make this final write depth-aware. |
| Final post-energy order | `tools/reel/zhao_reel.cpp:3817-3827` | The no-depth belly core, if enabled, draws after the unconditional repaint. Normal frames then return before gib rendering. Consequently current order is: initial ink → shell → populations → mist → smear → post splats → unconditional body-inner ink → belly core. |

## Exact failure mechanisms

### Why the O is treated as creature fill

The full mask is built correctly enough to distinguish visible creature pixels from the projected opening. The classification error happens one stage later:

1. `admit_exterior` refuses every full-mask pixel and flood-fills only from viewport borders (`zhao_reel.cpp:3408-3430`).
2. A negative-space component enclosed by the loop cannot reach that border, so it remains `mask == 0, exterior == 0`.
3. The exterior contour grows only where `exterior == 1` (`zhao_reel.cpp:3432-3461`), so the O cannot receive the ordinary contour.
4. The internal auxiliary interprets `!exterior` as an internal region, but its candidate side is restricted to `u02_body_cover` (`zhao_reel.cpp:3472-3487`). It can draw the body/head arc facing the O, but it cannot select the antenna sticks that bound the same O.

Thus the opening is negative in the pixel mask but positive/inside in the topology used by the contour. The missing lower line is not caused by too little ink width; the lower stick surface never enters either owner set.

### Why the lower stick edges get no ink

At a visible lower/inside stick pixel:

- it is part of the full union `mask`, so the outward ring starts from it;
- the adjacent O pixel is not viewport `exterior`, so the outward ring cannot grow into that neighbour;
- it is not a body-only pixel, so the body internal-boundary loop skips it before examining neighbours.

Changing the body-only neighbour predicate or increasing line width cannot fix that ownership hole. The candidate domain must include the visible full-creature surface bordering enclosed negative space.

### How the top line draws through nearer geometry

The first top-line paint is safe with respect to composed body/antenna geometry: `u02_body_depth[i] == depth[i]` proves the body is the visible full-creature surface at that pixel (`zhao_reel.cpp:3472-3473`). This prevents the initial line from painting on a pixel already owned by a nearer antenna surface.

The later restore loses that guarantee:

- `u02_inner_ink` stores only a boolean, not the selected body depth or a current owner.
- Populations and splats render later and depth-test correctly, so a nearer effect can legitimately replace the ink in RGB.
- Those effects either explicitly do not write depth or render into a temporary depth surface that is discarded when only RGB is copied back.
- The final loop at `zhao_reel.cpp:3808-3815` tests only the stale boolean and overwrites RGB.

Therefore the current code can erase a correctly depth-tested foreground particle/lightning result with stale body ink. Adding a second depth comparison at the final repaint would still be insufficient because later effect ownership is intentionally absent from the shared depth buffer.

## Narrow production fix

### 1. Keep and decouple the accepted top-line owner

Retain `u02::body_type()` and the existing body-side test unchanged. Change only its activation condition at `zhao_reel.cpp:3289-3290` from shell-enabled Manafold to cel-outline-enabled Manafold. The internal outline contract must remain live when a diagnostic disables shell; `manafold-antenna-fixed` and `manafold-antenna-quarter` explicitly disable shell at `zhao_reel.cpp:8454-8482` and `8612-8625`.

This is a dependency correction, not a new visual treatment. It leaves shipping subjects with shell enabled unchanged while making the top line belong to the outline pipeline.

### 2. Add the missing enclosed-opening owner on the creature side

After `exterior` is known and before `edge` is painted, add one additional internal-edge source:

- candidate: `mask[i] == 1`, i.e. a currently visible pixel of the fully composed creature;
- boundary witness: within the existing `ink_width`, at least one neighbour has `mask[j] == 0 && exterior[j] == 0`;
- destination: set `edge[i]`, never the neighbour in the opening.

Use the existing four-direction internal-width walk unless a focused native render demonstrates a connectivity break; do not invent a new radius, colour, threshold, or morphology. Union this source with the existing body-only source. The existing body-only source remains necessary because it also describes visible body/antenna overlap boundaries where both sides may be full-mask pixels.

This gives the complete lower/inside O boundary to the visible full-creature surface. Because only `mask[i]` pixels are painted, the O remains negative space. Because `mask` came from the depth-resolved full composition, hidden stick/body surfaces cannot become candidates.

No loop-only auxiliary `CreatureType` is needed. Adding one would duplicate the full type's pose/LOD/deformation plumbing without providing information that the visible full mask and enclosed component already contain.

### 3. End unconditional post-energy ownership

Delete the RGB restore at `zhao_reel.cpp:3805-3816`. Keep the initial paint at `3499-3504`, and include both body-inner and enclosed-O edges in the existing `edge`, `u02_ink`, and `u02_cover` products.

This preserves the top line whenever it remains the visible owner:

- the shell reads `u02_ink` and uses its existing line-protection policy;
- mist rejects `u02_cover`;
- a behind-creature population, smear cell, or splat fails its existing depth test;
- a nearer effect that passes the depth test remains in front, as Direction 17 requires;
- intentionally no-depth layers such as the belly core retain their existing explicit ordering.

Do not replace the deleted block with a fresh unconditional loop, an RGB-colour test, or a comparison against unchanged shared depth. Correct final ownership comes from painting ink before the already depth-tested later passes.

## Explicit implementation boundaries

### Production edits in scope

- `tools/reel/zhao_reel.cpp`
  - decouple body auxiliary creation from `c.u02_shell`;
  - add the full-mask-to-enclosed-negative-space creature-side edge source;
  - union it into the existing initial `edge`/`u02_ink`/`u02_cover` path;
  - remove the unconditional final inner-ink repaint;
  - correct comments that call the final restore depth-aware.
- A small shared pure helper may be extracted for mask classification only if the committed gate calls that exact helper. It must accept masks/depth/width as inputs and must not own art constants.
- Add one committed outline gate target to both the direct builder and CMake, following the existing gate integration patterns at `tools/reel/build-direct.sh:16-48,227-290` and `tools/CMakeLists.txt:19-33`.

### Out of scope / must remain unchanged

- No changes to `make_body`, `make_loop`, the skeleton, skin weights, carrier motion, or any mesh topology.
- No new `loop_type()` or duplicated clip bank.
- No changes to `kCelInk*`, `cel_main_ink_width`, shell values, mist values, lightning gains/radii, materials, textures, cameras, or animation.
- No filling, tinting, or cover-marking of the O interior.
- No changes to `glow_splat`, `mist_composite`, `smear_composite`, particle depth law, or renderer depth format.
- No threshold or art value may be inferred from pixel measurements. Reuse the existing authored ink colour and projected-width function; the gate checks ownership/invariants, not taste.

## Committed gate and evidence contract

Add `tools/reel/manafold_outlinegate.cpp` (direct target `moutline`, CMake target `manafold-outlinegate`) and have production plus gate call the same pure mask-classification helper. The gate should stop after one focused structural and one focused render/order check; no broad suite is warranted.

### Normal assertions

1. **O stays open.** On the full-mask fixture/render, the enclosed component contains at least one pixel and every one of its pixels remains `mask == 0` and unpainted by ink.
2. **Complete inner boundary.** Every creature-side full-mask pixel within the existing width of that enclosed component is in the internal edge output. This covers upper, lower, and side segments without defining a new artistic width.
3. **Top line preserved.** Every previously valid body-only/internal pixel—body-covered, depth-equal to the full composed frame, adjacent to non-body/non-exterior—is still in the final initial edge union.
4. **Nearer creature coverage wins.** A body-only candidate whose full-frame depth is owned by nearer antenna geometry is absent from internal ink.
5. **Behind effect loses.** A depth-tested effect behind an internal-ink surface cannot alter that line.
6. **Nearer effect wins.** A depth-tested population/splat in front of an internal-ink surface remains the final RGB owner; no later outline write may replace it.
7. **Mist contract survives.** Ink/creature pixels are in `u02_cover`, while the O interior is not. Mist remains free to exist in the negative-space opening and forbidden from softening the sticks or ink.

### Required fired positive controls

The gate invocation should run its own controls and fail if either control does not turn the relevant assertion red:

- `--selftest-no-opening-owner`: suppress only the new full-mask/enclosed-space edge source. It must fail complete-inner-boundary while top-line preservation and O-negative-space assertions remain green. This proves the gate detects the original missing-lower-edge defect rather than merely detecting any ink.
- `--selftest-force-post-repaint`: reapply the old boolean internal mask after a nearer sentinel effect. It must fail nearer-effect-wins while the behind-effect case remains green. This proves the gate detects the original through-foreground ordering defect.
- Retain the existing `ZHAO_U02_FORCE_LIGHTNING_THROUGH` control (`manafold_fx.h:1714-1731`) as the lightning depth-law control; do not overload it as the outline-repaint control because it tests a different failure mechanism.

Normal returns zero. Each deliberate control must produce the named nonzero failure, and `--selftest` returns zero only after witnessing both expected failures.

### Native and 4× crop evidence

Commit one evidence sheet at native 384×240 and one exact 4× nearest-neighbour sheet. Do not smooth, resample with interpolation, or use the enlarged sheet as a substitute for native judgment.

The sheet must contain:

- effects-off front, three-quarter, and side witnesses from the committed diagnostic family (`manafold-antenna-fixed`, `manafold-antenna-quarter`, and the `manafold-hover` orbit); use multiple posed frames so opening shape/occlusion changes are represented;
- at least one effects-on crossing from `manafold-pirouette` or `manafold-channel`, where energy intersects the existing top/internal line;
- the normal result beside the no-opening-owner control for the lower/inside O;
- the normal result beside the forced-post-repaint control at the top-line/foreground-effect crossing.

The already committed/current `PASS17-ANTENNA-FINISH-NATIVE.png` and `PASS17-ANTENNA-FINISH-4X.png` are suitable **before** witnesses: frames labelled 0000 and 0300 show the open loop and accepted head-top arc while the inner/lower sticks lack coherent black ownership; the oblique frames show why a single frontal pose is insufficient. They do not substitute for after/control evidence.

Accompany the PNGs with a short text manifest containing the source commit, dirty-state declaration if applicable, binary hash, exact subject/frame labels, explicit environment, render command, and sequence/crop CRCs. Evidence acceptance is visual at native and 4×; the gate's structural assertions establish cause and failability. Neither step may select or derive art values from measured pixels.

## Acceptance summary

Direction 17 is complete only when all of the following hold in the same production binary:

- the current upper/head-top line is unchanged when unobscured;
- the entire visible lower/inside boundary of the antenna O has coherent ink;
- the opening remains true negative space;
- nearer posed antenna/body geometry suppresses hidden body ink;
- nearer lightning/energy/particle geometry remains in front;
- behind effects remain occluded by the creature/ink;
- shell and mist retain their present distinct ownership; and
- both deliberate controls fire the committed gate and are visibly wrong in the native/4× crops.
