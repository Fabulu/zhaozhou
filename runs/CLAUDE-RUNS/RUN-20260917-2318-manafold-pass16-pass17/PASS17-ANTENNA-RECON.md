# Manafold Pass 17 zero-trust antenna audit

Task: trace Front/A/B/C/End from bind through production pose and visible skin, verify the existing controls, and define the missing public-clip proof required by Direction 14.

**Date:** 2026-09-18
**Mode:** read-only source/evidence audit; no production files changed

## Executive verdict

No present production-rig defect is proven by this audit. The Pass-16 graph is structurally plausible and the existing committed nodule diagnostic already has five separate mute controls, including B and C; all five were fired in this audit and each made the gate fail as intended.

Direction 14 is nevertheless not discharged. The current five controls mutate a reconstructed copy of the private slot-16 solo diagnostic. The gate proves that copy equals the banked diagnostic, but it does not ablate a public clip, and its Front/End measurement is a synthetic marker 100 mm along each fixed-centre carrier rather than the visible swell skin. The final4 review sees combined staggered poses, not five separately attributable public performances. The remaining fault is therefore an **evidence gap at the public-picture boundary**, plus several source comments that now describe architectures the code no longer has.

## Production graph

### Bones and parents

Actual skeleton, parent-before-child (`tools/reel/manafold_rig.h:43-125,180-209`):

- `Root -> JunctionF -> Neck -> A -> B -> C -> D` is the continuous outgoing/return chain.
- `LoopBase2` is a body-root closure target and skins no visible ball (`manafold_rig.h:69-85`; `manafold_clips.h:443-466`).
- `RearSocket` and `ReturnTip` are body-root children in the actual skeleton (`manafold_rig.h:196-209`). `finalize_rear_follow` solves D toward the deformed body target and writes exact local translations for socket/tip (`manafold_clips.h:511-599`); midpoint samples are independently re-solved (`:602-713`).
- Runtime pose decode applies the authored local translation before parent composition, then multiplies by inverse rest (`reference/src/zcreature/creature_core.cpp:420-465`).

### Visible skin ownership

The antenna is one continuous ring skin with five broad swells. Pass 16 changed the weight ladder so every visible core is wholly owned by its named carrier (`tools/reel/manafold_model.h:192-259`):

- Front core: `JunctionF`;
- A core: `HingeA`;
- B core: `HingeB`;
- C core: `HingeC`;
- End core: `RearSocket`.

Transitions remain two-bone ramps; the spans below A/B/C stay on Neck/A/B respectively and receive independent lane stretch only while that lower carrier owns them (`manafold_model.h:349-394`).

### Authored movement path

`Rig::write` emits quaternions, local translations and span lanes together (`manafold_clips.h:128-211`). `loop_pose` composes the hinges and End rotation (`:313-357`), then the A/B/C target solver rotates Neck/A/B and writes the matching child translations on A/B/C (`:369-417`). Thus a station's own rotation changes its outgoing span, while its visible centre is carried by the preceding frame and its translated child carrier.

The public five-beat performance is `swallow_press` + `swallow_nodules` (`manafold_clips.h:1340-1386`): Front and End articulate at fixed body attachments by carrier rotation; A/B/C add independent target offsets and translated child carriers. Taunt and Taunt II call this five-entry path before `loop_pose`, then compose the body response after it (`manafold_clips.h:2504-2539,2588-2614`). Their public subjects are slots 11 and 12 (`tools/reel/zhao_reel.cpp:8251-8252`). Hover/Channel also use five-beat swallow phrases (`manafold_clips.h:1968-1976,2086-2104`).

## Per-carrier table

| Visible carrier | Visible core owner | Direct authored source | What actually moves the visible core | Public final4 witness | Existing mute | Missing Direction-14 evidence |
|---|---|---|---|---|---|---|
| Front / JunctionF | `kBJunctionF` (`manafold_model.h:228-233`) | ambient grip/wag in `antenna_knead`; `swallow_nodules[0]` rotates JF (`manafold_clips.h:1870,1911-1913,1370-1376`) | centre remains body-attached; JF rotation reorients the swell and carries the entire downstream chain | Taunt staggered grid shows front/downstream configurations (`FINAL4-BATCH-07.md:8-10`) | `mnodule --fail-mute F` | same-frame public Taunt/II A/B render; visible Front skin response, not only a +100 mm synthetic marker |
| A | `kBHingeA` (`manafold_model.h:234-241`) | `g.nod.a*` from schedule + five-beat index 1 (`manafold_clips.h:1835-1839,1377-1378`) | Neck aims the JF->A span; A receives exact local-y endpoint translation (`manafold_clips.h:369-399`) | combined Taunt/Taunt-II staggered poses (`FINAL4-BATCH-07.md:8-14`) | `mnodule --fail-mute A` | public-clip A-only ablation measured on visible swell centroid relative to Front, with downstream carry distinguished from A authority |
| B | `kBHingeB` (`manafold_model.h:238-245`) | `g.nod.b*` + five-beat index 2 (`manafold_clips.h:1379-1380`) | HingeA aims A->B; B receives exact child translation (`manafold_clips.h:399-407`) | combined Taunt/Taunt-II staggered poses | `mnodule --fail-mute B` | same public-clip ablation; Pass-16 prose omitted this already-existing red leg, so record its actual fired result |
| C | `kBHingeC` (`manafold_model.h:242-249`) | `g.nod.c*` + five-beat index 3 (`manafold_clips.h:1381-1382`) | HingeB aims B->C; C receives exact child translation (`manafold_clips.h:408-416`) | combined Taunt/Taunt-II staggered poses | `mnodule --fail-mute C` | same public-clip ablation; separate C authority from closure D and prove the straight return remains intact |
| End / socket | `kBRearSocket` (`manafold_model.h:250-257`) | ambient End wag and five-beat index 4 rotate the socket (`manafold_clips.h:1923-1929,1383-1385`) | local translation pins centre to the lane-0-deformed body surface; carrier rotation reorients the End swell without sliding its centre (`manafold_clips.h:558-599`) | lasso/taunt review reports attached rear socket and staggered chain (`FINAL4-BATCH-04.md:8-18`; `FINAL4-BATCH-07.md:8-14`) | `mnodule --fail-mute E` | public A/B render must make the anchored End articulation visible on its anisotropic skin; centre travel alone is intentionally the wrong metric |

## Existing gate results, fired in this audit

The exact Pass-16 D13 `manafold-nodule.exe` was run with `zhao-env.ps1` sourced. Normal gate: RC 0, `PASS: 0 failure(s)`. It proves its locally authored slot-16 pose equals the banked slot-16 pose: 4,608 quaternions plus 13,824 translations (`manafold_nodule.cpp:164-267`).

Normal solo measurements through production `decode_pose + skin_vertex`:

- Front-alone marker: Front 29 mm; downstream A/B/C carry 193/261/232 mm; End 0.
- A-alone: A 202 mm; downstream B/C 279/261 mm; upstream Front 0.
- B-alone: B 204 mm; C 415 mm; upstream Front/A 0/2 mm.
- C-alone: C 205 mm; upstream Front/A/B 0/2/4 mm.
- End-alone marker: End 29 mm; all others 0.
- opposed pose: A +79 mm, B -70 mm, C +118 mm.
- rear socket: 99 influenced vertices, actual rho 1.035..1.162, worst target deviation 1.4 mm.

Every existing red leg fired and returned the gate's inverted success RC 0:

| Control | Gate result |
|---|---|
| `--fail-mute F` | FAIL, 1 failure |
| `--fail-mute A` | FAIL, 1 failure |
| `--fail-mute B` | FAIL, 2 failures |
| `--fail-mute C` | FAIL, 2 failures |
| `--fail-mute E` | FAIL, 1 failure |
| `--fail-ignore` | FAIL, 6 failures |

This corrects the Pass-16 summary, which mentioned only Front/A/End. The implementation has all five controls at `manafold_nodule.cpp:192-230`; its header is stale and still describes only two leg classes / A-B-C usage (`:46-56`).

## Real defects and blind spots

1. **Public shipping evidence is not independently attributable.** `manafold_nodule` reconstructs and verifies diagnostic slot 16, not public slots 11/12. Final4 confirms combined staggered shapes but no public per-carrier mute.
2. **Front/End gate a synthetic marker, not the visible swell.** `posed_ball` offsets anchored carriers by 100 mm (`manafold_nodule.cpp:110-128`). It proves rotation exists; it does not prove the low, rounded carrier-owned skin visibly reads that rotation at 240p.
3. **A/B/C skin proof is partial.** `manafold_spangate` G5/G6 reads visible A/B/C skin on shipped clips and proves differential span motion, but its mechanism control is lane ablation; it neither isolates each public authored target nor includes Front/End (`manafold_spangate.cpp:544-729`).
4. **The diagnostic control and public phrase are different compositions.** The solo clip uses pure per-segment gestures; public Taunt/II combine ambient knead, overlapping five-beat swallow, fold, root/body motion, lighting and effects. A private red leg can fire while the public ball remains visually lost.
5. **Stale comments now contradict production:**
   - rig header says 12 bones and an owed joint (`manafold_rig.h:1-24`) while code has 16 plus RearSocket/ReturnTip (`:118-125`);
   - enum prose says socket/tip are children of the C solver, while the actual skeleton parents both to Root (`manafold_rig.h:118-123,196-209`);
   - model prose says JF is unweighted/pure parent, but Pass 16 assigns the Front core to it (`manafold_model.h:425-435` versus `:228-233`);
   - meshcheck still introduces “four balls” (`manafold_meshcheck.cpp:1-16`);
   - nodule gate header understates its five existing mute legs (`manafold_nodule.cpp:46-56`).
6. **End-centre immobility is intentional but easy to misreport.** The socket centre follows the breathing body surface; individual authored End motion is orientation about that centre. Any new gate that demands centre translation would reject the correct attachment contract.

## Minimum Direction-14 acceptance plan

1. Add one committed public-phrase control that can mute exactly one of `F/A/B/C/E` at the production five-beat consumption point while leaving all other Taunt/Taunt-II inputs unchanged. The five controls must be separately invocable.
2. For A/B/C, measure the visible swell skin centroid relative to its upstream visible carrier in the named public beat, not absolute world travel. Each mute must collapse its own differential while leaving the other four beats live.
3. For Front/End, measure an anisotropic visible-skin orientation marker or selected skin vertices, not centre displacement. Their centres must remain attached while their public orientation/read disappears under the corresponding mute.
4. Render same-frame Taunt and Taunt-II shipping/control pairs at each carrier's authored peak. Build native and 4x grids with committed `plates.py`; review the ball itself and the carried span. A mute control passes only by making the correct public beat visibly absent.
5. Preserve and rerun the existing structural guards: normal plus all five `mnodule` red legs; `mspan` normal and its lane/ramp controls; `mprobe` closure/tip/B2-retirement; `mmeshcheck` continuity/bind agreement. Do not weaken thresholds to admit new art.
6. Correct the stale comments only after the traced graph and public controls agree.

## Antenna surface-finish check

A preliminary native/3x check made with committed `plates.py` on quiet Taunt and orbit frames shows one continuous band with five broad authored swells; it does not show the dense repeated ribbing of the old “corrugated hose” report, and no detached base seam is obvious. Energy obscures some orbit views, so this is not enough to declare the old report closed.

Before changing texture or topology, commit one focused plate containing quiet final4 front, three-quarter, side and rear views at native and 4x scale. Judge only: repeated narrow ribs versus broad swells, continuity across each carrier core, and black/colour seam at both body attachments. If the hose/seam does not reproduce, mark it closed with that plate. If it does, author the finish by eye; do not derive new widths from the old report's measurements.
