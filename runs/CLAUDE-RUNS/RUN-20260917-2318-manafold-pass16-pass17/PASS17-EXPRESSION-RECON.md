# Manafold Pass 17 expression and clip recon

**Date:** 2026-09-18
**Mode:** read-only source and final4 picture audit; no production/source/site files changed
**Questions:** eye form/size acting, Fall seam, Trick identity, taunt3 punchline

## Executive verdict

All four are real Pass-17 work, but they do not share one mechanical fix.

1. **Eyes:** the base lens still reads as the inherited long dagger at oblique views, and there is no size channel at all. Correct the static base form by eye first, then add an identity-default per-eye uniform-size channel so larger/smaller acting scales lens, white star and cyan star as one nested unit.
2. **Fall:** f338 is the authored last key and f339 is the presentation midpoint back toward key 0. `wrap_root_delta` protects only root translation; quaternions, local translations and deformation still wrap. The clean story is a deliberate one-shot: clamp presentation at the last recovery key and stop the website video rather than fabricate a second launch merely to make a loop.
3. **Trick:** the long identity loss is authored by the held half-turn about root Z. That maps the face from +X to -X for the whole plant while a small show-off yaw cannot bring it back. Re-author the flip axis/path by eye (prefer a face-preserving, X-dominant headstand path) and re-solve the existing contact; do not hide it with a crop or delete the hold.
4. **taunt3:** timing and holds now exist, but the punchline still does not land. The final hold presents a low side/back mass with small/oblique eyes. Accepted Zixxtrixx taunts keep a bright readable face through every sampled extreme. Use one integrated punchline: front-readable whole-body arrival, all-five-carrier antenna pose, and asymmetric large/small eyes.

## 1. Eye form and size acting

### Current production path

- Static lens form is authored at `kEyeLongMm=270`, `kEyeWideMm=84` (3.2:1) and `kEyeDeepMm=40` in `tools/reel/manafold_art.h:936-960`. The 11-ring pointed profile is `:961-984`.
- `make_eye_lens` bakes those constants directly into ring Y/width/depth, then marks the part as a body-deformation follower (`tools/reel/manafold_model.h:536-562`). There is no frame-varying form input.
- White and cyan stars are two parts on the same pupil bone, built from one profile and deliberately kept rigid together (`manafold_model.h:565-667`). They must continue scaling/moving as one nested unit.
- The skeleton is `EyeTravel -> Eye -> Pupil`; pupil bind sits at the lens centre (`tools/reel/manafold_rig.h:216-248`). Scaling the Eye transform uniformly would naturally carry its pupil child, white and cyan together.
- `Rig` carries quaternions, local translations, nodule/span tracks and only `eye_lean`; it has no scale/form channel (`manafold_clips.h:128-187`). `face_rest`, carrier travel, per-eye roll/gaze, squint and blink are all rotation channels (`manafold_clips.h:782-795,842-900,973-1072`). Preserve them.
- `Clip` currently offers quats, optional local translation and deformation samples (`reference/include/zref/zref_creature.hpp:243-257`). Pose decode creates a rigid rotation matrix plus translation (`reference/src/zcreature/creature_core.cpp:420-465`). There is no legal scale seam today.

### Current visual fault

Pass-14's native comparison calls the shipped lens a 4.2-4.7:1 dagger against a fat ~2.6-3:1 almond, with a long empty violet point and too-small star (`Upheaval/creature/Manafold/PASS-14-REVIEW.md:56-85`). It also proves oblique presentation turns one eye into a thin white bar (`:91-120`). Pass 15 repaired camera-relative travel/readability, not the underlying form. Final4 protects two-eye visibility but still has no large/small performance.

### Clean implementation seam

**Recommended architecture for ratification:** add an optional identity-default **uniform per-bone scale track** to `zc::Clip`, parallel to `local_translation`.

- Empty track is exact identity for every existing creature.
- Explicit identity is one named scale value (for example a fixed-point/per-mille 1.0), interpolation/baked midpoint included.
- Decode uniformly scales the EyeL/EyeR local 3x3 basis before parent composition. Uniform scale preserves normal direction and propagates structurally to the pupil child, so lens + white + cyan stay registered.
- Manafold's `Rig` owns named left/right eye-size values and writes them with every key; ordinary clips write identity.
- Do **not** exploit non-normalised quaternions for scale: decode/interpolation deliberately normalises them, and prior non-unit matrices caused body pulsing.
- Do **not** simply use spare deform lane 4. Manafold allocates four extra lanes but writes only the three span lanes (`manafold_clips.h:194-210,1077-1091`); however eye vertices already use `kFollower` metadata to ride body deformation. One `DeformVertex` has one role/axis/centre across all lanes (`zref_creature.hpp:493-512`), so lane 4 cannot independently scale the eye without replacing the body-follow contract. Radial deform also contracts one axis while expanding the other two, not uniform large/small acting.

The static base lens still needs an eye-authored `kEyeLongMm` / `kEyeWideMm` ladder selected by looking at front, three-quarter and oblique native crops. Do not derive the shipped values from old ratios. Only after the splinter-free base form is selected should the size channel animate it.

### Controls and acceptance

Green path:

- named left/right size curves in Startle, Curious, Taunt and taunt3;
- plainly larger and smaller native 240p eyes, including asymmetric acting;
- lens, white and cyan remain nested and rigid; blink/travel/roll/gaze/body-follow all remain live;
- no eye-eye contact, disappearance into the body, clipped violet tips or white-only splinter at oblique extremes.

Required red legs:

1. empty scale track and explicit identity render byte-identically to final4;
2. mute left size channel: the named left-eye beat disappears while right remains;
3. mute right size channel: converse;
4. break child propagation in a committed mutant/control: star/lens registration gate fires;
5. restore the old 270/84 base form in a same-binary/control ladder and require the selected native comparison to reject its splinter read.

Final evidence: fixed-camera native and 4x front/three-quarter/oblique grids, every-frame eye-expression sheets, and final4 A/B witnesses for protected travel/blink/roll.

## 2. Fall seam

### Exact root cause

`build_fall` authors 170 keys (`kFallKeys=170`, `kFallCatchKey=130`; `manafold_art.h:2543-2545`). Key 169 is the final grounded recovery (`manafold_clips.h:2394-2434`). Presentation produces 340 frames: f338 is key 169; f339 is subframe 1 between key 169 and wrapped key 0.

The clip sets `wrap_root_delta=true` (`manafold_clips.h:2381-2392`), but that option explicitly extends **only root translation**. Baked/runtime quaternions, local translations and deformation still wrap to key 0 (`creature_core.cpp:240-247,263-346,398-433`). Final4 therefore shows the grounded recovery at f338 and an opening/high-pose read at f339; the sheet confirms the one-frame reset (`final4-sheets/manafold-fall.png`, last row).

This is not a bad fall arc. The descent, contact and recovery pass. It is the presentation contract at the last half-key.

### Smallest correct authored solution

Make Fall explicitly one-shot rather than inventing a launch after its recovery:

- set `c.hold_last=true` so root, quats, local translations and deformation clamp on the final presentation partner;
- add an explicit non-loop media contract to the site manifest/assembler so Fall's `<video>` does not loop automatically;
- preserve keys 0..169 and all accepted contact/recovery values.

A seamless looping alternative would need a newly authored recovery-to-high ascent/launch long enough to read; that is a different action and should not be smuggled into the existing fall merely to satisfy a website loop.

### Controls and acceptance

- Green: last 24 presentation frames settle continuously; f339 equals/continues f338; website stops on recovery; replay is a deliberate user restart.
- Red: same-binary `hold_last=0` control reproduces the f338->f339 reset.
- Existing 3D contact probe remains green; no contact/depth threshold changes.
- Evidence: enlarged last-24 grid, native last-24+first-24 comparison, and generated HTML assertion that the Fall video lacks `loop` while ordinary cycles retain it.

## 3. Trick identity through the headstand

### Exact root cause

`build_trick` holds a half-turn at `kFlip=-1000` from keys 78..148 (`manafold_clips.h:2630-2659`). The code converts that to a root `quat_z(-32768)` (`:2666-2671`). The face lives on root descendants facing roughly +X; a 180-degree Z rotation maps +X to -X. The fixed camera therefore receives the back/rear body throughout the plant.

The planted show-off yaw is only a sinusoidal ±3000 angle16 contribution (`manafold_clips.h:2677-2694`, about 16.5 degrees), far too small to undo a 180-degree face reversal. `face_rest` runs after the root transform and cannot restore world-facing identity (`:2706-2712`). Final4's f115-f295 evidence is therefore authored orientation, not a missing material or renderer fault.

### Implementation seam

Keep the headstand, plant window, root-height/contact schedule, balance and recovery, but re-author the **root flip axis/path** as named art knobs.

Preferred ladder: a face-preserving X-dominant/cartwheel half-turn (or an authored mixed X/Z path), compared by eye against the current Z-half-turn. Rotating about the face axis preserves the face's +X direction while still allowing the antenna to plant, but the exact axis blend is an art choice and must be rendered, not derived. Re-solve root height/contact against the chosen pose with the existing committed 3D probe.

Avoid a camera-only chase as the primary fix: a moving camera can hide the motion, and Manafold's eye travel bake distinguishes fixed versus orbit cameras. A new Trick camera path would create another mirrored schedule that must be kept in lockstep with eye travel.

### Controls and acceptance

- same-binary old-Z-axis control reproduces the featureless rear hold;
- selected axis/path keeps face, both eyes and antenna identity readable across the entire planted phrase at native resolution;
- existing plant depth, approach clearance, continuous antenna, inner-ink and loop gates remain unchanged and green;
- every-frame sheet plus native f115/f160/f220/f295 grid; no shortening/removing the hold.

## 4. taunt3 direct comedy verdict

### Picture verdict: **fails the Pass-17 expression bar**

Final4 now has real attacks and two holds; the timing repair is genuine. But the punchline itself is not unmistakably comic.

The committed-tool grid `PASS17-TAUNT3-PUNCHLINE.png` shows:

- f286: busy energy/antenna lead-in;
- f298: fast body/antenna dismissal arrival;
- f310/f330/f345: a long low side/back hold with tiny oblique eyes and little facial change;
- f360: release.

The hold exists, but its face is not the punchline. The viewer gets a broad pink side/back mass and scattered particles. In contrast, accepted Zixxtrixx grids `PASS17-ZIXX-TAUNT-KEYS.png` and `PASS17-ZIXX-SLOW-TAUNT-KEYS.png` keep the bright eye/face readable through every sampled extreme while neck/tail silhouettes change clearly. Manafold's silhouette is larger and its antenna action is richer, but it does not exceed that facial expression bar.

### Source reason

The clip already uses punch easing, a 3-key dismissal and 24-key hold (`manafold_clips.h:3877-3959`). It is not missing another timing pass. During the dismissal/hold it composes a whole-body roll+yaw (`:4053-4068`), squints both eyes and keeps the eyes at one static physical size (`:4069-4080`). The held orientation de-emphasises the face, so the component choreography does not become a joke.

### One coherent Pass-17 punchline

Do not add a separate fifth gesture. Re-author the existing dismissal/hold so it arrives front-readable, then coordinate:

- a whole-body front-readable extreme/arrival;
- an unmistakable opposed all-five-carrier antenna pose (public Direction-14 proof);
- asymmetric eye-size acting (for example one enlarged, one reduced) on the corrected almond base;
- current fast attack / clean hold / exit timing retained unless the new picture shows otherwise.

Required red legs: mute eye-size, mute each carrier at its peak, and restore the current body orientation. Each must visibly weaken/remove the punchline in a same-frame native A/B. Acceptance is a side-by-side against both accepted Zixxtrixx taunts where Manafold's face, body and antenna communicate one readable joke without explanation.

## Protected wins

Do not regress: camera-relative eye travel; blink, gaze, roll and star+white rigid parenting; bouncing-body follower motion; round body; continuous five-swell antenna, straight return and body-following rear socket; authored ground contacts; Pass-16 depth/inner ink/shell; lasso, deaths, Hasty/Flight; folded-lightning vocabulary and independent particles.
