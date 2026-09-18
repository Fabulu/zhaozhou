# Manafold Pass 17 signed-span architecture

**Date:** 2026-09-18
**Scope:** read-only architecture for Owner Direction 16; structure before amplitude art

## Decision

Use **four appended, translation-only skin-delta bones** and the existing two-weight skinner. Do not add a signed generic deformation format.

The new bones are co-located identity children of each span's lower articulation. They do not represent another visible joint; their palette differs from the lower palette only by the signed local-Y delta:

```text
existing pose chain (unchanged)
Root -> JunctionF -> Neck -> HingeA -> HingeB -> HingeC -> HingeD

new skin-only delta palettes (zero rest offset, identity rotation on every key)
Neck   -> SpanDeltaA    local-Y = the same signed delta written to HingeA
HingeA -> SpanDeltaB    local-Y = the same signed delta written to HingeB
HingeB -> SpanDeltaC    local-Y = the same signed delta written to HingeC
HingeD -> SpanDeltaE    local-Y = signed C-to-body-socket length delta
```

A zero-rest-offset child is deliberate: at delta zero its skin palette is exactly its parent's palette, without relying on a translated-rest cancellation. Applied to the upper bind station, its palette lands at the translated endpoint; it need not put the helper bone's own pivot there.

`HingeA/B/C` keep their current hierarchy, local translations, rotations and rigid visible cores. `RearSocket` remains a Root child at the exact deformed-body attachment and keeps its local articulation. `ReturnTip` remains its Root sibling, buried. Existing bone IDs 0..15 remain unchanged; append the four delta bones as 16..19 and set `kBoneCount = 20` with a `<= 32` guard.

This is the smallest correct use of facilities already present:

- no reference-format, interpolation, serializer, renderer or RTL semantic changes;
- still at most two weights per vertex;
- no non-rigid bone matrix;
- four extra identity-quaternion bones, within the donor-proven 32-bone ceiling;
- signed length is already carried losslessly by `local_translation`;
- the visible skin finally consumes that signed value.

The authored-key payload increase is explicit: Manafold always allocates local translation, so four bones add `4 * (8-byte quat + 12-byte local translation) = 80 B/key`, plus 8 B/key only on clips that allocate the optional uniform-scale sidecar. This is an asset-bank budget question, not a Quartus-fit question.

## Why the tempting alternatives are wrong

### Reweight the whole span directly to the present child hinge — reject

A full `Neck -> HingeA`, `HingeA -> HingeB`, or `HingeB -> HingeC` gradient does distribute the child's signed translation. It also distributes the **child's own independent rotation backward over its incoming span**. The span bows, the incoming bend authority smears across the entire run, and a carrier core can cease to read as one carrier. Translation and rotation are inseparable in the present child palette matrix.

The delta sibling separates those two quantities. Across the free span, lower and delta palettes differ only by signed translation. Across the existing 90 mm bend zone, delta and articulated-carrier palettes apply the same endpoint translation and differ only by the carrier rotation.

### Extend `DeformSample` with signed axial scale — reject for this pass

That would touch the generic clip ABI, interpolation, C2 seam normalization, software decoder, hardware format/RTL, every consumer and cross-creature compatibility merely to encode a value already present in `local_translation`. It would also leave two competing length mechanisms unless the old child translation were removed. Four content bones are narrower, hardware-legible, and exactly affine through the existing skinner.

### Keep the current positive lanes and merely raise amplitudes — reject

`write_span_lanes` clamps every negative `span_pm` to zero. The child core moves inward while its visible incoming span does not compact. Raising travel makes that disagreement larger. Amplitude work must wait until signed propagation is green.

## Exact continuous-skin ownership

Stations below are millimetres from the loop part's `y0`; ring indices are the actual 64-ring sampling of `s = floor(2930*i/63)`. Boundary evaluation must use the named station expressions, not copy these sampled numbers into source.

| Station interval | Actual rings | Pair / ownership | Purpose |
|---|---:|---|---|
| `[0,180)` | 0..3 | existing `Root -> JunctionF` ramp | buried/front attachment, unchanged |
| `[180,320)` | 4..6 | rigid `JunctionF` | Front carrier-owned core, unchanged |
| `[320,410)` | 7..8 | existing `JunctionF -> Neck` ramp | Front outgoing bend, unchanged |
| `[410,790)` | 9..16 | `Neck -> SpanDeltaA`, linear 0..64 | signed Front/A free-span length |
| `[790,880)` | 17..18 | `SpanDeltaA -> HingeA`, existing 90 mm ramp | introduce A articulation only near A |
| `[880,980)` | 19..21 | rigid `HingeA` | A carrier-owned core |
| `[980,1130)` | 22..24 | `HingeA -> SpanDeltaB`, linear 0..64 | signed A/B free-span length |
| `[1130,1220)` | 25..26 | `SpanDeltaB -> HingeB`, existing 90 mm ramp | introduce B articulation only near B |
| `[1220,1320)` | 27..28 | rigid `HingeB` | B carrier-owned core |
| `[1320,1510)` | 29..32 | `HingeB -> SpanDeltaC`, linear 0..64 | signed B/C free-span length |
| `[1510,1600)` | 33..34 | `SpanDeltaC -> HingeC`, existing 90 mm ramp | introduce C articulation only near C |
| `[1600,1700)` | 35..36 | rigid `HingeC` | C carrier-owned core |
| `[1700,1790)` | 37..38 | existing `HingeC -> HingeD` ramp | introduce the coincident closure direction |
| `[1790,2450)` | 39..52 | `HingeD -> SpanDeltaE`, linear 0..64 | signed C/End free-span length |
| `[2450,2540)` | 53..54 | `SpanDeltaE -> RearSocket`, existing 90 mm ramp | meet the body-root socket basis smoothly |
| `[2540,2780)` | 55..59 | rigid `RearSocket` | End carrier-owned core; centre body-attached |
| `[2780,2870)` | 60..61 | existing `RearSocket -> ReturnTip` ramp | socket articulation decays into buried tail |
| `[2870,2930]` | 62..63 | rigid `ReturnTip` | buried straight tip, unchanged |

Named boundaries:

```text
outN1 = stNeck + coreHalf[F] + foldBlend[F] = 410
inA0  = stA    - coreHalf[A] - foldBlend[A] = 790
inA1  = stA    - coreHalf[A]                = 880
outA0 = stA    + coreHalf[A]                = 980
inB0  = stB    - coreHalf[B] - foldBlend[B] = 1130
inB1  = stB    - coreHalf[B]                = 1220
outB0 = stB    + coreHalf[B]                = 1320
inC0  = stC    - coreHalf[C] - foldBlend[C] = 1510
inC1  = stC    - coreHalf[C]                = 1600
outC0 = stC    + coreHalf[C]                = 1700
outC1 = outC0 + foldBlend[C]                = 1790
inE0  = stEnd  - coreHalf[E] - foldBlend[E] = 2450
inE1  = stEnd  - coreHalf[E]                = 2540
outE0 = stEnd  + coreHalf[E]                = 2780
outE1 = outE0 + foldBlend[E]                = 2870
```

The Front swell's existing carrier interval is deliberately preserved rather than recentered during a mechanism patch. Its visual swell centre (`kKnuckleAtJfMm = 320`) and the coincident JF/Neck articulation station are historically offset. If that shape is ever re-authored, it is a separate look/render decision.

## Composition law: no crack and no double length

For each free span, let `L` be its bind centre distance, `d` the signed solved length delta, and `u` the 0..1 ring weight across the free-span interval.

- The lower articulation palette is `M`.
- The co-located identity delta-child palette is `M` plus the parent's rotated translation `d`; at `d = 0` it is exactly `M`.
- Existing LBS therefore gives `p'(u) = M p(u) + u*d_axis`: a straight affine extension or compaction.
- At the incoming-bend boundary, the delta child has weight 64 in both adjacent zones, so there is no positional step.
- The delta-child-to-hinge ramp then introduces only the child's articulation around the already translated endpoint.
- The rigid hinge core and every descendant inherit the same endpoint translation once, exactly as today.

`Rig` should expose one helper that writes a span delta to both representations which must agree:

```text
set_span_delta(A, delta): local_t[HingeA].y = local_t[SpanDeltaA].y = delta
set_span_delta(B, delta): local_t[HingeB].y = local_t[SpanDeltaB].y = delta
set_span_delta(C, delta): local_t[HingeC].y = local_t[SpanDeltaC].y = delta
```

Do not leave six assignments at call sites. A duplicated value without one writer is tomorrow's detached ball.

For C/End, `finalize_rear_follow` and its midpoint companion must:

1. walk the signed A/B/C chain exactly as now;
2. aim `HingeD` at the lane-0-deformed socket target;
3. compute `dE = distance(C, socket) - kRearSocketFromCMm`;
4. write `dE` to `SpanDeltaE.local_y`;
5. continue writing the existing Root-local `RearSocket` translation to the same target;
6. continue placing `ReturnTip` `kRearSocketBurialMm` farther along the same line.

Thus `SpanDeltaE` and `RearSocket` map the **bind End centre** to the same posed point but intentionally present different bases there; the helper bone's own pivot remains co-located with HingeD and is not a visible station. Their 90 mm blend turns the continuous band from the closure direction into the socket's independently articulated body-root frame. The End centre remains attached; the tip remains a sibling and cannot inherit the socket bend.

The old deform and the new LBS must never both lengthen Y. That would be double scaling.

## Deform-lane disposition

Retire lanes 1..3 as **axial length** channels. Signed local translation plus skin weights becomes the sole length mechanism.

The lanes may remain as a positive-extension cosmetic only if the render comparison earns it:

- write `flatten > 0`, `spread = 0` for positive delta;
- use it only to narrow the blade's broad bind-X gauge on the corresponding free span;
- authority is zero on every rigid carrier core and every buried attachment/tip ring;
- negative delta writes identity, not an unsigned reinterpretation.

This retains the named `kSpanThinRatioPm` without moving Y or Z. If this asymmetric thinning looks worse, remove lanes 1..3 entirely; lane 4 and lane-0 body deformation remain unrelated. Explicit thickening under compression would require a genuinely signed transverse sidecar and is not justified merely to satisfy length. With the first structural patch, compaction keeps the authored gauge constant, so material density per unit length increases without an inversion-prone second transform.

## Signed limits and non-collapse

Keep two named owner knobs:

```text
kSpanStretchMaxPm       // positive centre-distance ceiling
kSpanCompactionMinPm    // negative centre-distance floor
```

Do not clamp negative values to zero. The solver computes `want - bind_length`, preserves the sign, and advances the forward walk by the same applied signed length that it writes to both the articulated child and delta sibling.

The limits are content-authoring bounds, not a second visual answer. Public art must be authored inside them and the bank gate must fail if a key or midpoint exceeds either bound. In particular, End attachment wins over a limit: never clamp `dE` and detach the socket; reject/re-author the offending pose instead.

A compile/gate invariant must be evaluated from the real named boundaries for every stretch zone:

```text
posed_free_length = bind_free_length + signed_delta
posed_free_length > named_min_free_span_mm > 0
```

The shortest free interval is A/B, not the shortest centre distance, so a centre-distance-only positivity check is insufficient. Walk every actual ring centroid at both signed bounds and require strictly positive ordered steps. The compaction floor and minimum free-span margin are selected by rendering and looking; measurement only rejects collapse/inversion.

## Independent ordering art, after structural green

A/B/C already have separate `{ax,ay,az}`, separate schedules and separate public mute inputs. Do not replace them with one curve plus phases.

Author a public ordering phrase with named per-ball up/down travel and named timing, using the existing target path. Across that public motion:

- A is visibly above both B/C at one named frame and below both at another;
- B is visibly above both A/C and below both A/C;
- C is visibly above both A/B and below both A/B;
- every pairwise height-difference curve crosses repeatedly;
- F and End centres retain their attachment contracts while their local rotations stay individually live;
- F/A, A/B, B/C and C/End each show both positive and negative signed length relative to their own bind distance.

The hierarchy means one ball carries downstream balls. Therefore high/low constants cannot be copied 1:1 between A/B/C. They remain independent named knobs and are chosen in final-resolution motion by eye. A trajectory plot verifies the authored result; it does not generate amplitudes.

Use the existing `PublicJointMute` at the production consumption point for A/B/C controls. If the new ordering phrase is factored out of `swallow_nodules`, route it through the same mute helper rather than inventing a private diagnostic mute.

## Committed gates and failable controls

Refactor `mspan` around the new mechanism rather than preserving lane-centric checks whose premise is gone.

### Structural checks

1. **Exact zone/owner table.** Read the compiled skin and prove the ring ranges above use exactly the named pairs, monotone weights, rigid F/A/B/C/E cores, at most two influences, and no delta bone owns a carrier core.
2. **Identity palette.** At zero delta, each `SpanDeltaA/B/C/E` palette is exactly equal to its parent lower-articulation palette at keys and presentation midpoints. Applying `SpanDeltaE` and `RearSocket` palettes to the bind End centre must produce the same point after rear finalization. A no-offset rest render must retain the accepted pose/pixels even though mesh bone IDs/weights change.
3. **Signed synthetic walk.** Drive each of four spans alone to a positive and negative value. Prove child/delta-bone duplicate translation equality, endpoint/core coincidence, signed centre-distance change, ordered ring centroids and no detached surface.
4. **No silent clamp.** Extract signed deltas from generated key and midpoint local translations. A negative request must remain negative and produce a shorter visible span; a positive request must remain positive and produce a longer one.
5. **Public four-span trace.** Emit one CSV with F-A, A-B, B-C and C-End signed centre-distance deltas for every public frame/subframe. Gate both signs for every connecting segment used by the ordering performance.
6. **Public height trace.** Emit root-local visible-core `A_y,B_y,C_y`, pairwise differences, crossings, and named top/bottom witness frames. Gate one top and one bottom witness per carrier with a named read margin chosen only after render review.
7. **Attachment/closure.** Front stays at its body attachment; End centre equals the deformed body target; ReturnTip remains buried; straight return remains straight; closure and surface-contact checks run at every key and midpoint.
8. **Continuity and mesh.** Run `mmeshcheck` topology/closure, `mprobe`, `mnodule`, `mjointpub`, the revised `mspan`, payload/budget checks and ordinary geometry tests. The new IDs must be included in any predicate that currently assumes loop bones are only contiguous `JunctionF..HingeD` plus socket/tip.

### Committed red legs

Keep these as invocable mutations in the committed gate, not temporary production edits:

- `--fail-rigid-span F-A|A-B|B-C|C-E`: force that free zone back to its lower bone; core translation remains live, so visible propagation/continuity fails.
- `--fail-clamp-negative <span>`: replace only that span's negative delta-bone/child translation with zero; signed trace and contraction witness fail.
- `--fail-delta-drift A|B|C|E`: omit one delta-bone duplicate while leaving the real carrier motion live; endpoint coincidence fails.
- `--fail-overcompact <span>`: apply a legal test mutation beyond the structural floor; ordered-ring/non-collapse check fails.
- `--fail-mute A|B|C`: remove one production authored channel; exactly that carrier's top/bottom/crossing evidence fails while unrelated channels remain live.
- Retain F/E public joint mute legs, RearSocket target error checks and buried-tip controls.

A red leg asserts the correct behavior's absence. Do not write a test whose expected success depends on the production bug still existing.

## Picture evidence

Mechanism gates do not decide likeness or amplitude.

After all structural gates are green:

1. render the public ordering phrase at final 384x240 resolution;
2. make an every-frame contact sheet, not evenly sampled stills;
3. commit one A/B/C height-trajectory plot with crossings and named top/bottom frames;
4. commit one four-span signed-length plot;
5. make same-frame normal versus A-, B-, C-muted controls at each named witness;
6. inspect native scale first, then 4x crops of each ball and adjoining spans;
7. inspect front, three-quarter, side and rear diagnostic cameras for straight return, continuous skin and attached F/End;
8. author named travel/timing/limit knobs by render-look-adjust until the large independent read is convincing.

Only then render the full 28-subject bank and run isolated every-sheet review.

## File and commit boundary

### Structural signed-span commit

Production/gates only:

- `tools/reel/manafold_rig.h` — append four co-located delta bones; preserve IDs 0..15; assert `kBoneCount <= 32`.
- `tools/reel/manafold_model.h` — exact free-span gradients and delta-to-carrier bend zones; remove axial lane authority.
- `tools/reel/manafold_clips.h` — one duplicate-delta writer; signed solve; End-delta key/midpoint finalization; no negative clamp; optional positive-only width thinning.
- `tools/reel/manafold_art.h` — named extension ceiling, compaction floor and minimum free-span safety margin; no public amplitude tuning in this commit.
- `tools/reel/manafold_spangate.cpp` — signed four-span mechanism, zone, identity, midpoint, closure and red-leg gates.
- `tools/reel/manafold_probe.cpp`, `manafold_meshcheck.cpp`, `manafold_nodule.cpp`, `manafold_public_jointgate.cpp` — only the minimum new-bone predicates/attachment assertions required by the changed graph.
- `tools/CMakeLists.txt` and `tools/reel/build-direct.sh` only if a separate ordering gate is added; prefer extending `mspan` to avoid another near-duplicate instrument.
- this run's report/log and text/CSV gate receipts.

Do not mix eye-form/eye-size art, Front/End gain selection, Fall wrap, Trick, taunt3 punchline or body-shell tuning into this structural commit. Stage paths deliberately in the already-dirty tree.

### Ordering-art commit

After structural green:

- named A/B/C up/down amplitudes and timings in `manafold_art.h`;
- public performance consumption in `manafold_clips.h`;
- renderer-only diagnostic selectors in `zhao_reel.cpp` if needed;
- committed trajectory/length plots, every-frame sheets, normal/mute plates and review record.

### Legitimately changed outputs

- Compiled Manafold skeleton/mesh payload and bank size change because four bones and new weights are real data.
- Any clip with A/B/C nodule offsets changes: positive length comes from LBS rather than axial deform, and negative length finally reaches the skin.
- Every clip's C/End free span may change because its exact body-following signed distance now propagates across the visible band.
- Consequently full-bank Manafold CRCs/media are expected to change and require fresh review.
- A zero-offset synthetic pose must remain visually/positionally identical; body, face, shell, texture, root and event channels must remain untouched by the structural packet.
- Other creatures and the generic reference/RTL semantics must remain byte-identical because no shared format changes.
- Pass-16 archived media/evidence remains immutable; website media changes only with the finished Pass-17 publish.

## Acceptance sentence

Direction 16's structure is complete only when the visible four connecting spans consume signed endpoint distance through ordinary two-weight skinning, every A/B/C carrier owns a rigid core and reaches both top and bottom ordering in public motion, F/End remain attached, and the same final-resolution pictures that show large independent travel also show extension **and** compaction without a seam, collapsed band or sliding ball.
