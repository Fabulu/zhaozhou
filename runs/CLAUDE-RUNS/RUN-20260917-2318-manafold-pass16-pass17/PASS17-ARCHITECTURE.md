# Manafold Pass 17 implementation architecture

**Date:** 2026-09-18
**Inputs:** Owner Direction 14; Pass-17 history, antenna and expression recons; motion-style §§8a/8b
**Decision:** one expression-and-public-proof pass, not six unrelated repairs

## 1. Ratified result

Pass 17 has one centre: **the creature's expression must be true in the shipping picture**.

- Front/A/B/C/End must each own and visibly contribute a public antenna beat, proven by five same-frame visible-skin controls rather than inherited names or the private slot-16 diagnostic.
- The inherited dagger lens is first re-authored by eye into a wider, splinter-free almond. A new identity-default uniform scale track then lets the left and right complete eye assemblies become plainly larger and smaller, including asymmetric acting.
- `taunt3` spends that eye channel, a front-readable whole-body arrival and an opposed five-carrier pose on one unmistakable punchline; it keeps the already-correct fast attack/hold/exit timing.
- `Fall` becomes a deliberate one-shot that holds its recovered last pose and does not loop on the site.
- `Trick` keeps the headstand, plant and long hold but replaces the face-reversing Z half-turn with a rendered, face-preserving root-axis/path choice and re-proves contact.
- The old corrugated-hose/base-seam report gets one quiet current plate. If it does not reproduce, no antenna finish or topology change is allowed.

No accepted Pass-16 effect, shell, death, lasso, Hasty/Flight, body or folded-lightning work is reopened.

## 2. Critical data-path decision: uniform eye scale

### Options tested adversarially

| Path | Storage / state | Runtime / hardware shape | Structural problems | Verdict |
|---|---|---|---|---|
| Reuse spare deform lane 4 unchanged | one shared `{flatten,spread}` sample/key | existing pre-skin deform path | one global unsigned sample cannot express independent signed L/R large/small values; each vertex has one role/axis/centre; eye vertices already need `kFollower`; radial deform is anisotropic and would distort rather than uniformly resize; pupil/lens registration depends on marking every part identically | Reject |
| Add a special eye/vertex sidecar | two small eye values/key plus new per-vertex eye-group/centre metadata | extra per-visible-vertex scaling before skinning, compounded with follower motion | lower asset bytes, but adds metadata and arithmetic on the hot vertex path, duplicates eye parenting in vertex tags, and can silently omit a lens/star part | Reject |
| Optional uniform per-bone scale track | `frame_count * bone_count` u16 samples only on clips that author scale, plus generated midpoints | scale the decoded local 3×3 basis on pose-cache miss; ordinary skin path and vertex payload unchanged | more asset bytes on four hero clips, but matrix hierarchy naturally carries pupil children and needs no second per-vertex ownership system | **Choose** |

### Chosen representation

Add to `zc::Clip` in `reference/include/zref/zref_creature.hpp`:

- `std::vector<uint16_t> uniform_scale_q15`; optional, frame-major, `frame_count * bone_count`;
- `std::vector<uint16_t> mid_uniform_scale_q15`; generated presentation companion;
- Q1.15 identity is exactly `32768`; valid authored range is `1..65535` (positive, below 2×).

This is deliberately **uniform**, not XYZ scale. It changes size without changing normal direction. `skin_normal_lambert` normalises the transformed direction, so the common uniform factor cancels. The existing matrix palette already carries a full 3×3 basis; skin vertices and meshlets require no new field.

Only clips with a non-identity eye-size performance allocate the track. Planned first users are `Curious`, `Startle`, `Taunt` and `taunt3`; all other Manafold clips and every existing creature keep an empty vector and execute the old path bit-for-bit. At 16 bones the uncompressed cost is 32 bytes/key on those clips; generated 60 Hz companions add the same amount. That memory is preferable to permanent vertex payload/arithmetic, and the eye channel can later be packed sparsely without changing semantics.

### Decode and midpoint semantics

In `reference/src/zcreature/creature_core.cpp`:

1. Validate optional scale shape beside `local_translation`.
2. `bake_presentation_midpoints` clears stale mids when the source track is absent/malformed; for a valid track it emits one scalar per bone/segment using the existing held-endpoint rule, event-adjacent average, otherwise clamped Catmull-Rom. `hold_last` clamps the last segment; loops wrap.
3. `decode_pose` reads key/mid Q1.15 scale. After `quat16_to_mat3`, multiply only the nine basis entries by the scale and shift 15 before adding bind/local/root translation. Explicit `32768` therefore produces the exact old matrix bits.
4. Parent composition propagates EyeL/EyeR scale to each Pupil child structurally. The pupil bind is at the lens centre; white and cyan parts remain rigidly registered on that child.
5. Keep scale out of quaternions. Quaternion normalisation is a rotation invariant and must never be abused as a size channel.

Update the optional-sidecar contract in `spec/creature_rules.md` and clarify that `clip_frame_bytes()` is the base rigid-pose payload, not the total optional hero asset size. No RTL changes are part of this pass.

### Core tests

Extend `tests/geometry/creature_core.cpp` and the `creature_core` target:

- empty track equals the old pose byte-for-byte;
- explicit all-identity track equals empty byte-for-byte at keys and midpoints;
- authored child scale reaches the exact Q1.15 matrix basis;
- generated midpoint is held/monotone and `hold_last` clamps the final midpoint;
- parent Eye scale carries a child Pupil offset/geometry while not scaling sibling/root bones;
- zero scale and malformed array are rejected by asset validation;
- a test-local broken-parent control makes the pupil-registration assertion fail.

No Quartus fit is a Pass-17 gate: this changes the reference/oracle asset path, not RTL. When GEOM.POSE later consumes this sidecar, it needs its own subsystem implementation and fit; the likely implementation is a time-multiplexed Q1.15 multiply on cache miss, not nine parallel multipliers.

## 3. Manafold eye authoring

### Files and named controls

- `tools/reel/manafold_art.h`: retain all eye dimensions as named knobs; author the selected wider/splinter-free `kEyeLongMm`, `kEyeWideMm`, profile and, only if looking requires it, star-size constants. Preserve a `kEyeLegacyForm` tuple for diagnostics.
- `tools/reel/manafold_model.h`: `make_eye_lens` consumes a selected `EyeForm` struct; lens, white and cyan construction remain separate parts on Eye/Pupil bones.
- `tools/reel/manafold_clips.h`: `Rig` gains `scale_q15[kBoneCount]`, reset to identity and written only when a clip has an allocated scale track. Add named per-eye setters in per-mille art units converted to Q1.15 at authoring. Allocate tracks lazily in the four expression clips.
- `tools/reel/zhao_reel.cpp`: parse diagnostic-only same-binary controls for legacy eye form and left/right size mute before Manafold compilation. Unset values are shipping defaults.
- Add `tools/reel/manafold_eyesize.cpp` and target `meyesize` in `tools/reel/build-direct.sh`/CMake source lists.

### Art sequence

1. Render a base-form ladder at fixed native front, three-quarter and oblique cameras. Choose by looking; old aspect ratios are comparison evidence only.
2. Stop unless the selected static form removes the long empty violet point and white-only splinter while keeping both stars readable.
3. Author clearly large/small beats: Startle may open both; Curious may focus/shrink asymmetrically; Taunt and taunt3 use asymmetric large/small acting. Exact values remain editable constants selected from renders.
4. Do not change `kEyeSurfaceFollowPm` merely because it is zero. Rejudge it only if the selected form still fails oblique views.

### Eye controls

`meyesize` and same-binary render controls require:

- empty and explicit-identity tracks reproduce final4 output CRCs on unaffected clips;
- mute L removes only the left size beat; mute R removes only right;
- legacy base-form control restores the dagger/splinter read and is rejected on the committed native plate;
- child-registration control fires if lens/white/cyan cease to remain nested;
- `meyecam`, blink/travel/roll tests and selected final4 A/B witnesses remain green.

Evidence: fixed-camera native + 4× form grids; every-frame sheets for Curious/Startle/Taunt/taunt3; named oblique extremes. Component gates do not decide likeness.

## 4. Public five-carrier antenna proof

### Production control seam

Add a diagnostic enum (`None,F,A,B,C,E`) in the Manafold clip diagnostics, default `None`. Apply it at the **public five-entry `swallow_nodules` / `swallow_press` consumption point**, not in a copied private pose. Parse `ZHAO_U02_JOINT_MUTE` in `zhao_reel.cpp` before `make_manafold`.

Normal shipping is unchanged. The same binary can compile/render Taunt (slot 11), Taunt II/Lasso (slot 12) and taunt3 with one named carrier's public beat removed while all body, light, effect and other-carrier inputs stay identical.

### New committed gate

Add `tools/reel/manafold_public_jointgate.cpp`, target `mjointpub`.

For each carrier at a named authored public peak:

- identify actual visible rigid/core vertices from compiled skin ownership, never bone-name proxies;
- A/B/C: compare the visible swell centroid relative to its upstream visible carrier and the carried span;
- Front/End: compare a visible-skin principal orientation/selected vertex pair because centre translation is intentionally not their contract;
- compile normal plus F/A/B/C/E public-muted banks;
- require the selected carrier's public differential/orientation to collapse under its mute while the other four named beats remain live;
- keep body-follow/socket centre, straight return and buried-tip checks green.

Each mute is an inverted red leg: it passes only when the public visible-skin check fails for the right reason. Keep and rerun existing `mnodule` normal + five mutes, `mspan` normal/lane/ramp controls, `mprobe`, and `mmeshcheck`.

### Picture proof and possible art repair

Use one renderer binary to make same-frame native/4× Taunt and Taunt-II grids for shipping plus F/A/B/C/E. A carrier is accepted only when its own missing beat is visible in the control picture. If one ball remains visually lost despite correct structure, strengthen only its named public art constant and rerender; do not rebuild bones or weight ladders pre-emptively.

Correct stale architecture prose after the traced graph and controls agree:

- `manafold_rig.h` (16 bones; Root-parented RearSocket/ReturnTip);
- `manafold_model.h` (JunctionF owns Front core);
- `manafold_meshcheck.cpp` (five visible carriers, not four balls);
- `manafold_nodule.cpp` header (all five existing mutes);
- Pass/card rear-socket rho prose, regenerated from the accepted gate. Do not preserve the stale `0.960 / 0.950..1.081`; the current accepted saved gate reports `1.043 / 1.035..1.162`, which must be re-derived on the final source.

## 5. Antenna finish stop gate

Before any texture/topology edit, commit a quiet plate made with existing `plates.py`:

- front, three-quarter, side and rear;
- native and 4×;
- quiet Taunt/Hover frames with energy absent or minimal;
- inspect only repeated narrow ribbing versus broad swells, core continuity and black/colour seams at both body attachments.

If dense corrugation/base seam does not reproduce, mark the old report closed and stop. If it does, author finish constants by eye and rerun mesh/bind/public controls; never derive new widths from the old measured ratio.

## 6. Fall as a deliberate one-shot

- `tools/reel/manafold_clips.h::build_fall`: set `hold_last=true`; remove `wrap_root_delta` or leave it documented as inert because hold-last wins. Keys 0..169, descent, contact and recovery remain unchanged. The last presentation partner clamps, so f339 equals/continues f338.
- Add same-binary/test control restoring `hold_last=false`; it must reproduce the f338→f339 reset.
- Existing 3D contact/clearance gate must pass unchanged; no threshold movement.
- `Upheaval/website/creatures.json`: set `"loop": false` on the live Fall entry.
- `website/tools/assemble.py::media`: validate `loop` is boolean. Default/missing remains current looping behaviour. `loop:false` emits `controls muted playsinline` with neither `autoplay` nor `loop`, so replay is a deliberate user action and hidden tabs cannot finish before selection.
- Add an assemble self-check/test asserting Fall's generated tag has `controls`, lacks `loop`/`autoplay`, while an ordinary live cycle retains `autoplay loop muted`.

Acceptance: enlarged last-24 and last-24+first-24 grids, old-wrap red control, generated-HTML gate, full media freshness/decode.

## 7. Trick face-preserving headstand

Expose named art knobs in `manafold_art.h` for Trick root X and Z flip amplitudes/path. In `build_trick`, replace the hard-coded held `quat_z(-32768)` with a composed X/Z root path driven by the existing `kFlip` schedule. Render at least:

- legacy Z half-turn control;
- X-dominant face-preserving half-turn;
- one authored mixed X/Z candidate if the pure X plant loses the intended silhouette.

Choose by eye at full every-frame/native f115/f160/f220/f295 evidence. Keep the plant window, root-height/contact schedule, show-off hold and recovery. Re-run the 3D contact probe and clearance; fix the authored root/contact path if needed, never the thresholds. Avoid a camera chase as the primary fix because it hides the action and creates another eye-camera mirror.

Stop unless the selected path keeps face, both eyes and antenna identity readable for the whole planted phrase and the legacy control visibly restores the rear-mass fault.

## 8. One integrated taunt3 punchline

The timing architecture is already correct: punch arrival, three-key dismissal and long hold. Do not add another gesture or retime by default.

Re-author the existing dismissal/hold using named knobs so one frame family contains:

1. a front-readable whole-body arrival (current body-orientation control retained as the red leg);
2. an unmistakable opposed Front/A/B/C/End public pose through the same production path used by `mjointpub`;
3. asymmetric eye size on the selected almond (one visibly large, one visibly small), while blink/travel/roll remain composed;
4. a held readable face with current fast attack/clean exit retained.

Same-frame red controls: mute eye size, restore current body orientation, and mute F/A/B/C/E separately. Each must weaken/remove the intended joke in native A/B evidence. Final acceptance is a committed side-by-side against `PASS17-ZIXX-TAUNT-KEYS.png` and `PASS17-ZIXX-SLOW-TAUNT-KEYS.png`; Manafold's face/body/antenna must communicate the joke without explanatory prose.

## 9. Targeted build and review sequence

1. **Baseline already established:** clean direct renderer; `manafold-hit` CRC matches final4; fresh `mexpress`/`meyecam`; existing five `mnodule` mutes fired.
2. **Surface stop gate:** quiet antenna finish plate; either close or authorize one bounded finish correction.
3. **Reference channel:** implement Q1.15 per-bone scale + core tests; direct `creature_core`; exact identity CRC on unaffected Manafold and Zixxtrixx subjects.
4. **Public antenna proof:** `mjointpub`, existing antenna gates, Taunt/Taunt-II five-control grids. Repair public art only if the picture/control says a ball is lost.
5. **Eye base ladder:** fixed cameras, select wider splinter-free almond by eye; legacy control remains.
6. **Expression clips:** Curious, Startle, Taunt, taunt3; run eye controls and compare taunt3 against Zixxtrixx.
7. **Fall:** hold-last/control seam grid; site playback generation test; contact probe.
8. **Trick:** axis ladder; every-frame/native identity review; contact probe.
9. **Integrated targeted bank:** Hover, Curious, Startle, Taunt, Taunt II, taunt3, Fall, Trick plus controls. Run `mprobe`, `mnodule`, `mjointpub`, `mspan`, `mmeshcheck`, `meyesize`, `meyecam`, `mexpress`, `creature_core`.
10. Only after all targeted questions stop: one exact-generation 28-subject render, isolated every-frame review, encode, freshness, full decode, commit/push/main integration, publish and production-byte verification.

## 10. Commit boundaries

1. **Inventory/architecture/archive:** durable Direction-14 scope, reports, Pass-16 byte archive (archive commits already started).
2. **Identity scale primitive:** `Clip`/decode/midpoint/validation/spec/core tests only; prove empty/explicit identity before Manafold art consumes it.
3. **Public antenna instrument:** diagnostics, `mjointpub`, five red legs, stale-comment corrections, quiet finish disposition; no unrelated art.
4. **Authored expression/clip packet:** selected eye form/size curves, public carrier adjustments if required, taunt3 punchline, Fall hold-last and Trick path, plus targeted evidence/gates.
5. **Final generation/publication:** findings/card, Pass-16 archive declaration, exact media, gate/deploy records and production verification.

Push every boundary when green. Publishing occurs only for the finished reviewed Pass 17, not intermediate ladders.

## 11. Protected wins and stop conditions

Protect byte/output behaviour on unaffected clips and creatures; round bouncy body; camera-relative travel, blink, gaze, roll and star+white parenting; continuous five-swell antenna; straight return; body-following rear socket and buried tip; all authored contacts; Pass-16 lightning depth, no smear, shell, inner ink, lasso, deaths, Hasty/Flight, folded-shape vocabulary/turns and independent particles; violet-night lighting.

Stop/escalate when:

- public carrier mutes do not isolate the intended visible ball — repair proof/rig before eye work;
- quiet finish plate does not reproduce corrugation — no finish edit;
- base lens still splinters at any required angle — do not animate its scale;
- uniform-scale identity changes one protected byte — do not author against it;
- Trick axis breaks contact — re-author contact/path, never thresholds or camera concealment;
- taunt3 does not beat the Zixxtrixx expression bar — do not run the full bank;
- any targeted control is structurally blind or cannot be fired — repair/commit the instrument first.

A gate passing is not likeness evidence. Final-resolution pictures decide the authored values; gates preserve the result afterward.
