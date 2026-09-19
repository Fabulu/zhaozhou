# Manafold version 18 architecture

**Date:** 2026-09-19
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md`
**Inputs:** `V18-ANTENNA-ROOT-RECON.md`, `V18-CLIP-SITE-RECON.md`
**Verdict:** **RATIFIED — content-side staged repair; no generic format, collision system, renderer architecture or RTL change**

## Governing split

Version 18 has two different kinds of work and they may not be mixed when values are chosen:

1. **Structural ownership repairs** make the two roots and Front channel capable of producing the requested form without shearing.
2. **Authored art ladders** choose root profile, material transition, swell size, Front motion, particle layer, Flight motion and Trick timing by complete native motion.

A gate can reject a split palette, snapped turn or lost contact. It cannot decide how small a ball should look or how broad a flight should feel.

## 1. Root authority — align the visual support, add no bone

No helper bone is added. The existing JunctionF and RearSocket bones already own the correct semantic pivots and attachments. The defect is that the authored *visual support* of each long root swell is larger and differently centred than its rigid carrier window.

Add named per-carrier core-centre authorship beside the existing half-width table:

```cpp
kLoopCarrierCoreAtMm[5]
kLoopCarrierCoreHalfMm[5]
```

For A/B/C, preserve the accepted station-centred windows. For Front and End, derive the integrated structural windows from the authored swell support itself:

```text
core start = swell station - swell half-width
core end   = swell station + swell half-width
```

The incoming blend ends at the core start and the outgoing blend begins at the core end. The complete visible thickening is therefore rigidly JunctionF-owned at the front and RearSocket-owned at the return. The transitions stay outside the zero-slope profile rim. The deeply buried Root and ReturnTip remain unchanged.

Why this is the selected architecture:

- it aligns one visible form with one existing semantic carrier;
- it preserves Root→JunctionF burial, C→End signed closure, RearSocket body follow and ReturnTip burial;
- it does not create a second length/rotation authority;
- it keeps the seven signed helpers and bone IDs stable;
- zero rotation/translation still yields the authored bind surface, although the intended palette assignment changes in the root zones.

A strict diagnostic `ZHAO_U02_ROOT_AUTHORITY=integrated|legacy-split` is parsed before type construction. `legacy-split` restores the version-17 station-centred Front/End core windows exactly.

Extend `mspan` rather than adding another near-duplicate gate. It must walk compiled root rings and require:

- every ring under each visible root profile support is rigidly owned by the named carrier;
- incoming/outgoing blends lie wholly outside that support;
- buried base/tip, RearSocket target, signed runs, posed ordering and closure remain green;
- the legacy-split control fires only the new root-authority category.

`mjointpub` continues to prove visible public response. Its Front witness gains a Front-flex mute described below; RearSocket semantics remain attached-rotation, never centre travel.

## 2. Front flexibility — append-only JunctionF axes

Append these fields to the end of `HingePlay`:

```cpp
int32_t tilt_front = 0;
int32_t yaw_front = 0;
```

Existing aggregate/call semantics remain exact because defaults are zero and the fields are append-only.

Composition at JunctionF is fixed and shared:

```text
rest yaw -> authored fold Z -> Front tilt X -> Front yaw Y
```

The existing `neck_pm` remains JunctionF's in-plane Z authority. The new fields add only the missing X/Y axes. Neck retains its separate `tilt_neck/yaw_neck` and continues to articulate the post-Front stick. Signed length still begins at Front→A; no body→Front length channel is added.

Public clips author named C2 Front X/Y curves through their existing `HingePlay` object. Start with the clips that expose the root in normal play (Hover/Rest, Flight/Hasty, Drift/Blown, Taunt/Taunt III, Trick); do not add an ambient oscillator under authored gestures. Each clip's Front amplitudes/timing remain independent owner knobs.

Add `ZHAO_U02_FRONT_FLEX=normal|mute` as the exact same-binary control. `mjointpub` and `mspan` require:

- current public clips produce a visible Front-core response under the shared metric;
- muting Front X/Y removes only that extra Front articulation while Neck/A/B/C/End remain live;
- root ring order, socket follow and all signed spans remain green;
- position/angular derivatives remain within the existing full-bank continuity categories.

## 3. Root material, normals and ink — staged, not guessed

### Production atlas

`tools/pack/mkmanafoldpage.py` remains the sole author of `tools/reel/manafold_page.h`; correct the generated header's stale “do not track” comment because the generated production page is committed.

Within the existing loop V band, generate a body-style field (body pigments, body grain amplitude and body stroke amplitude) and blend it into the loop field at the front and rear V ranges with a named quintic transition. The root blend widths are editable generator constants. The middle antenna keeps its own cooler/coarser character.

Do not infer blend widths from the concept sheet. Render a deterministic generator ladder, record page digest/source mode, then commit only the selected generated header and generator. The exact legacy generator mode remains a reproducible comparison control; material comparison may use separate deterministic page builds because embedding a second production atlas would waste console memory merely for a diagnostic.

### Normals and internal ink

Do not change generic `RingSpec`, compiled normal format or creature-wide normal generation in the first pass. First render the integrated core/profile plus body-like root atlas at full motion. Those two proven causes are expected to remove the dominant shear/material seam.

Only if native A/B still shows a dark seam may `add_visible_body_inner_edge()` receive a second-stage **posed root exclusion** derived from projected visible root support/depth. It must suppress only the body-inner-edge owner under the two root supports. It may not be a hand-painted screen box, alter the exterior union contour, repaint after effects, or remove the accepted body/head line elsewhere.

A `normal|legacy-root-ink` same-binary control restores the old owner only if this second stage is actually needed. If the material/profile result already reads fused, no ink code is changed.

No welded topology or generic normal override is authorised without a new native picture proving that the staged repair failed.

## 4. Smaller carriers — swell-only art ladder

The five visible balls remain the five existing smooth swell additions. Stick taper, station positions, profile half-widths, topology, bone palettes and signed spans are protected.

Add a strict diagnostic multiplier:

```text
ZHAO_U02_SWELL_PM = 0..1000
```

It multiplies only the five `rx/rz` swell amplitudes before `max()` composition. Shipping retains independent named Front/A/B/C/End amplitudes; the diagnostic multiplier creates one-binary identity/reduction ladders without erasing per-carrier ownership.

Render complete native motion for identity and several reductions. The picture selects the value. Acceptance requires:

- every carrier remains visibly thicker than both adjacent stick runs;
- none reads as a threaded sphere or protruding bead;
- root regions still widen smoothly into the body;
- five public mute/response controls remain attributable;
- no ring reversal/pinch, outline hole, socket slide or buried-tip exposure.

Never shrink `kLoopBladeRxMm/RzMm` as a proxy.

## 5. Particle/antenna overlap — one bounded layer A/B

The sole experiment routes only fold/surge mote halo and soft-core splats through the existing pre-creature layer. Connected lightning edges, free strands, endpoints, body glows and menu/archive media are unchanged.

Add:

```text
ZHAO_U02_MOTE_LAYER=post|behind-creature
```

with strict parsing. The selected shipping default is chosen from complete Channel, Crackle-normal, Hover, Taunt III, both deaths and Lasso motion. `msmooth` traces the stable mote ID's selected layer as a role operand so a frame-local layer switch is impossible; the diagnostic is constant for a process.

Ship `behind-creature` only if sticks/body cleanly occlude the overlap while motes remain spatially rich in the open O. Stop and retain `post` if the field looks pasted behind the whole animal or loses foreground depth. No repulsion, nearest-segment collision, per-pixel particle clipping or broad gain reduction is permitted.

## 6. Live history retirement and mana normalisation

### Retire the actual history plane

The 96x60 `u02_smear` path is already zero. Version 18 retires the separate 48x30 persistent `u02_mist` plane from **every live Manafold subject**.

Factor one shared presentation selector used by `subject_u02_clip()`:

```text
ZHAO_U02_LIVE_MIST=off|legacy
```

Default `off`; `legacy` reproduces version 17 exactly. Form/mist diagnostic subjects may opt in explicitly and archived bytes remain immutable. The contour shell/mist (`u02_shell`) and current mana particles are untouched.

Normal validation must enumerate every live subject and prove neither history plane allocates/feeds/composites. The legacy control must reproduce Hasty/Drift/Blown trails. Render all three plus Hover/Rest to guard against accidental loss of contour mist.

### Relic mana

- Drift and Blown already use ordinary candidate 9. Make no candidate/seed change unless their complete no-history renders still show a distinct defect.
- Crackle defaults to ordinary candidate 9 on the ordinary shipping backdrop.
- `ZHAO_U02_CRACKLE_PRESENTATION=normal|legacy-night` restores candidate 4 + night exactly.

Review candidate-9/day against candidate-9/night only as a rejected/secondary art rung; the requested normal presentation is the default unless looking disproves it.

## 7. Flight — independent amplitude and cadence knobs

Replace the implicit `176 / 44 = 4` law with named shipping knobs:

```cpp
kFlightBobAmpMm
kFlightBobCycles
```

and strict same-binary selectors:

```text
ZHAO_U02_FLIGHT_BOB_MM
ZHAO_U02_FLIGHT_BOB_CYCLES
```

The periodic phase is computed from exact clip progress and an integer cycle count, so first/last pose and all derivatives close. Root Y, pitch derivative, breath and lagged carrier response continue to derive from that one phase; no independent retiming is introduced.

Render amplitude-only and cadence-only ladders, then combine only the selected dimensions. Full 352-frame native motion chooses values. `mqa` adds a Flight trace for root-Y extrema, declared cycles, seam and step/acceleration/jerk; existing `mspan/msmooth` protect carriers/effects. The high/low framing crop and no-history contract are hard stops.

## 8. Trick — planted unwrapped yaw revolution

Keep the accepted pure-X 180-degree plant, +16384 local face correction, fixed camera, plant root height and existing righting. Do not add another pitch revolution.

Add a separate authored unwrapped progress table in per-mille-of-one-turn units:

```text
0 = no planted yaw
1000 = one full 360-degree yaw
>1000 = named overshoot
```

Timeline ownership:

1. arrive at the existing 180-degree contact;
2. hold an editable pause at progress 0;
3. advance monotonically with quintic C2 timing to 1000;
4. advance to a small named overshoot;
5. C2-correct back to exactly 1000 (orientation identity);
6. begin the existing pure-X righting only after correction completes.

Convert unwrapped progress to angle16 only at quaternion construction. Never infer completion by differencing wrapped angle16 values.

Strict controls/selectors:

```text
ZHAO_U02_TRICK_SPIN=normal|none
ZHAO_U02_TRICK_SPIN_GAIN_PM
ZHAO_U02_TRICK_SPIN_OVERSHOOT_PM
```

`none` restores version 17 exactly. Timing keys remain named constants and receive a rendered ladder before selection.

`mqa` independently extracts the relative root quaternion against a no-spin control, unwraps the planted yaw and proves one full monotone revolution, only the authored overshoot reversal, identity before righting, and bounded angular derivatives. It must not compare the source progress table to itself.

Strengthen `mprobe` contact evidence: during the full planted interval, require the declared deepest/contact vertices to belong to the antenna support region around the loop peak, not merely any creature vertex. Preserve the unchanged penetration band. Run all `mspan`, `msmooth` and eye/outline gates plus complete native/4x 400-frame selected/no-spin comparisons.

## 9. Site archive — one immutable version-17 generation

Do this before any version-18 encode can overwrite live names.

1. Copy all 28 live version-17 WebMs/posters byte-for-byte to immutable `archive-v17-manafold-*` names and verify pairwise SHA-256.
2. Build one archive generation labelled **`Version 17 + mana experiments — 2026-09-19`**.
3. The general version-17 archive collection references the 22 non-menu clips.
4. The existing six-item Mana menu object becomes `archive:true`, shares that generation label and references the six `archive-v17-manafold-mana-*` copies.
5. The ten-item Mana lab object becomes `archive:true`, shares the generation label and keeps its existing immutable lab paths. It is not cloned.
6. No clip appears in both the 22-item archive collection and archived menu. The 28 copied version-17 clips are each declared exactly once.
7. Update archive introduction count from twelve to thirteen generations.

Assembler behavior is retained: archived videos use controls + loop + muted, no autoplay and `preload="none"`. Gate exact archive pair hashes, one declaration per media path, playback, noindex, freshness and full decode. Commit/push this site-only closure before production rendering.

Version-18 live presentation and exact render bank contain 22 subjects; the six menu subjects no longer participate in freshness or encode.

## 10. Implementation waves, manifests and gates

### Wave A — immutable archive (Upheaval only)

Paths:

- `website/creatures.json`
- `website/public/renders/archive-v17-manafold-*` (56 files)
- generated `website/public/index.html`
- `creature/Manafold/VERSION-18-INVENTORY.md`
- `creature/Manafold/VERSION-18-PLAN.md`
- archive hash/playback test if existing tools cannot express duplicate-path/pair-hash checks

Gate: JSON, assemble, archive pair SHA, unique declarations, playback, noindex and full decode. Commit/push independently.

### Wave B — root instruments and structural authority

Paths:

- `tools/reel/manafold_art.h`
- `tools/reel/manafold_model.h`
- `tools/reel/manafold_clips.h`
- `tools/reel/manafold_spangate.cpp`
- `tools/reel/manafold_public_jointgate.cpp`
- `tools/reel/zhao_reel.cpp`
- direct/CMake lists only if an existing target genuinely needs registration changes

Land root core-centre/support ownership, legacy control, append-only Front X/Y channels, Front mute and diagnostic swell multiplier. No shipping art value is selected in this wave. Gate compile, `mspan`, `mjointpub`, `mnodule`, mesh/probe/outline, all old controls and protected version-17 bytes under identity diagnostics. Commit/push before material/art.

### Wave C — root material and conditional ink

Paths:

- `tools/pack/mkmanafoldpage.py`
- generated `tools/reel/manafold_page.h`
- `tools/reel/zhao_reel.cpp` / outline gate only if the rendered first-stage result proves root-inner-ink exclusion necessary

Render deterministic material ladders on the settled root authority. Select by native orbit. Add posed ink exclusion only after a visible residual seam. Commit generator + selected generated output + bounded evidence.

### Wave D — swell size and Front-flex art

Paths:

- `tools/reel/manafold_art.h`
- `tools/reel/manafold_clips.h`
- existing root/public/span gates as required for selected-value receipts

Render complete identity/reduction and Front-motion ladders; select by eye. Validate all public carriers, signed spans, roots, outline and shell. Commit/push selected art and evidence.

### Wave E — presentation/effect cleanup

Paths:

- `tools/reel/manafold_fx.h`
- `tools/reel/zhao_reel.cpp`
- `tools/reel/manafold_motiongate.cpp`
- `tools/reel/manafold_qa_p12.cpp` only if shared presentation assertions fit its scope

Retire live history plane, normalise Crackle and run bounded mote-layer A/B. Drift/Blown candidate state remains unchanged. Render full Drift/Crackle/Blown/Hasty plus particle witnesses. Ship mote-layer change only if pictures improve. Commit/push with all 16 existing effect controls plus new constant-layer/history controls.

### Wave F — Flight and Trick art

Paths:

- `tools/reel/manafold_art.h`
- `tools/reel/manafold_clips.h`
- `tools/reel/manafold_qa_p12.cpp`
- `tools/reel/manafold_probe.cpp`
- `tools/reel/zhao_reel.cpp`
- existing span/effect gates only for necessary trace extensions

Land instruments/selectors first, then render Flight amplitude/cadence and Trick pause/spin/overshoot ladders. Select values by full native motion. Protect framing, seams, support-region contact, face, spans and effects. Commit/push selected source and curated evidence.

### Integrated and release gates

- clean direct and CMake builds of every touched gate;
- complete normal/control matrix with strict attribution;
- one targeted binary: root orbits, all root-heavy public clips, Drift/Crackle/Blown/Hasty, Flight, Trick, deaths, Channel, Taunt III and site-live subjects;
- independent source/checker review;
- one exact 22-subject version-18 bank from one clean renderer;
- isolated every-frame review, with native/4x root, particle, Flight and Trick witnesses;
- exact encode, 22/22 freshness, archive playback, exact noindex and full declared-media decode;
- commit/push both feature branches, fast-forward both mains;
- publish with `deploy.ps1 -Project upheaval -Branch main`;
- cache-bypassed exact index + all 44 live version-18 media bytes on unique deployment and production alias;
- commit/push production verification records.

No Quartus fit is required because this architecture changes content, tooling and site data only. If implementation touches RTL, stop and define the subsystem-boundary fit before continuing.

## Blocking unknowns

There is no architecture blocker. Three art decisions remain deliberately unresolved until rendered ladders exist: final root material blend widths, final five swell amplitudes/Front motion values, and Flight/Trick timing values. The mote pre-layer is explicitly optional and must be rejected if it flattens depth.
