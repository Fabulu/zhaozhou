# Manafold Pass 17 — Q2/Q3 continuity repair plan

**Date:** 2026-09-18
**Scope:** read-only design; no source edit, build or render in this packet
**Input:** `PASS17-MQA-Q2-Q3-TRIAGE.md`

## Decision

Repair both defects in their authorship, not in their thresholds:

1. **Q2 death-eye snap:** split the camera-facing eye-carrier schedule out of `antenna_knead`, then keep only that static base alive through each dead tail with glance/drift/lean at zero. Do **not** call the whole knead layer on a corpse.
2. **Q3 Startle root step:** render a same-binary timing ladder whose primary candidate gives the snap one additional authored key while retaining a full readable hold. Do not raise the 260 mm ceiling and do not reduce the payoff amplitude unless the timing candidate looks worse.

The fixes are independent. They may share one targeted render/build wave but should remain separately controllable and attributable.

## Q2 — keep the dead face base, not the living knead

### Cause in current source

`antenna_knead` currently owns two unrelated responsibilities in one function:

- the camera-relative EyeTravel carrier at `manafold_clips.h:1924-1954`;
- nodule scheduling and all ambient antenna knead rotations from `:1956` onward.

Its `eye_pm` deliberately gains only glance/drift and eye lean; it does **not** gain `eye_face_base_a16` (`:1935-1953`). That is correct: fading the base would add a 31.8-degree eye rotation while a death is trying to remove motion.

Both death builders call `g.reset()` every key, call `antenna_knead` only while `!dead`, and skip it at their settle keys (`build_death_drop`, current `:3396-3414`; `build_death_gutter`, `:3570-3587`). The final living key therefore has the fixed-camera base, while the first dead key has identity EyeTravel carriers. Q2 observes the resulting 31.80/31.86-degree switch.

Calling `antenna_knead(..., eye_pm=0)` after settle is **not** the repair: slots 17/18 have live `kKneadClipPm` gains of 650/600 (`manafold_art.h:3781-3798`), so that would restart ambient hinge motion under the corpse. It would fix the eyes by reanimating the antenna.

### Narrow source change

Extract the existing eye-only block into one helper beside `antenna_knead`, for example:

```cpp
inline void apply_eye_schedule(Rig& g, uint32_t slot, EyeCam cam,
                               int keys, int f, int32_t eye_pm) {
  // Move the existing pin/base/glance/lean block here verbatim.
}
```

`antenna_knead` calls this helper first, then continues with nodules/knead exactly as today. Each death builder becomes mechanically explicit:

```cpp
if (!dead) {
  antenna_knead(g, death_slot, EyeCam::kFixed, K, f,
                1000 - fold_ease(gone));
} else {
  apply_eye_schedule(g, death_slot, EyeCam::kFixed, K, f, 0);
}
```

At `eye_pm=0`, the helper writes only `eye_face_base_a16`; glance, drift and `g.eye_lean` are zero. `face_rest` later writes Eye/Pupil-local expression but does not overwrite their EyeTravel parents. The corpse therefore holds a readable camera-facing pair without looking around, leaning or moving its antenna.

Keep `g_u02_death_fail == 5` as the committed old-fault mutation:

- living keys use the unfaded `eye_pm=1000` as today;
- dead keys **skip** `apply_eye_schedule`, restoring the identity reset.

This leg alters the production builder before static bank construction and reproduces the actual switch-off defect.

### Strengthen Q2 while touching it

Q2 currently differences scalar angle magnitudes `abs(a[f]-a[f-1])` (`manafold_qa_p12.cpp:223-287`). It sees the present base-to-identity bug, but equal-magnitude direction reversals would cancel. Compare consecutive carrier quaternions by their relative angular distance instead (absolute normalized quaternion dot, then `2*acos`), for both L and R carriers. Keep the existing 8 deg/key criterion and production self-check; only make the step reader sign/axis-safe.

Add dedicated counters:

- `q2_fails` for Q2 only;
- an attributed death-snap count for slots 17 and 18 at their named settle keys.

`--fail-eyesnap` must return success only after Q2 itself rejects both mutated deaths. Add `--eyes-only` if needed for isolated iteration, analogous to Q5's `--fall-only`; unrelated Q1/Q3 rows must not certify or block the Q2 control.

### Q2 picture evidence

Render normal and the same-binary legacy-eye control from one clean binary:

- `manafold-death-drop`: authored settle key 117, presentation window approximately f0226–f0242;
- `manafold-death-gutter`: authored settle key 191, presentation window approximately f0374–f0390.

For both:

- complete every-presentation-frame sheet;
- native full-frame settle window;
- exact 4x nearest-neighbour face crop;
- normal/control A/B at the last living, first dead and held-dead frames.

Acceptance: normal eyes do not jump around the body at settle and then remain still/readable; the mutation visibly resets them. Body contact, droop, lids, gaze, deform, root and long corpse hold remain unchanged.

## Q3 — re-time the Startle attack, preserve its size

### Cause in current source

`build_startle` uses:

```text
kBack: key 8 +140 pm -> key 11 -1300 pm
kUp:   key 8 -170 pm -> key 11 +1300 pm
```

Both root axes traverse their whole payoff in three authored keys (`manafold_clips.h:2327-2351`), producing the measured 290 mm key step. Pass 14 sharpened this after Q3's 260 mm Startle ceiling was authored. The ten-key held arrival is good and must survive.

The root is driven only by `kBack`, `kUp`, `kStartleJumpMm=520`, `kStartleLiftMm=300` and the small bob (`:2394-2400`; constants at `manafold_art.h:2533-2534`). Eye size, antenna spans and outline cannot cause Q3.

### Minimal same-binary ladder

Expose named Startle timing constants plus a pre-bank diagnostic selector. Keep the current amplitude constants unchanged for the primary ladder.

| rung | anticipation | arrival | hold end | aligned eye-wide arrival | purpose |
|---|---:|---:|---:|---:|---|
| Legacy | 8 | 11 | 21 | 11 | reproduces 290 mm rejection |
| **Late arrival** | **8** | **12** | **22** | **12** | recommended first look: four-key attack plus the full ten-key/20-frame hold |
| Early launch (conditional) | 7 | 11 | 21 | 11 (or a separately pictured 7) | only if late arrival visibly weakens the snap |

The late-arrival rung is structurally coherent with existing acting:

- gaze already starts at key 12 (`build_startle`, current `:2380`);
- shifting `kWide`'s -430 arrival from 11 to 12 keeps large-eye scale, lids and brow on the body's arrival;
- `kWhip` and `kSquash` already land their next peaks at keys 14/22, so they remain overlapping secondary action rather than being retimed wholesale;
- moving hold end to 22 preserves the promised ten-key/20-frame payoff rather than shortening it to fit a number;
- recoil knots at 30/42/52 and home at 74/79 stay untouched.

Do not choose this rung from the predicted step. Render it. If it loses the hard startled read, render the conditional early-launch rung. Only if both timing options look worse should a modest named displacement ladder be considered; never solve backward to exactly 260 mm.

Implementation should use editable constants for the shipping tuple and a validated one-binary renderer selector such as `ZHAO_U02_STARTLE_TIMING=legacy|late|early`, parsed before `u02::type()` is constructed. Default remains the selected authored tuple. Invalid values return RC 2.

### Q3 gate controls

Keep the existing 260 mm Startle ceiling and existing death-gutter `--fail-rootstep` leg. Add a distinct `--fail-startle-step` that selects the legacy 8/11/21 timing before static bank construction. Q3 must track the slot-4 result separately and certify this mutation only when **Startle itself** exceeds the ceiling; another clip's failure cannot prove it.

Normal acceptance requires useful headroom, not 259.x mm. The chosen animation comes from native review; Q3 then records its resulting margin without moving the ceiling.

### Q3 picture evidence

From the same binary, render full `manafold-startle` for legacy and each candidate actually considered:

- complete 160-presentation-frame sheet per rung;
- native sequence around f0014–f0064 (anticipation through first recoil);
- exact 4x body/face/antenna crops at anticipation, arrival, held extreme and release;
- same-frame legacy/candidate A/B at authored keys 7/8/11/12/21/22/30 (presentation frames `2*k` and the adjacent midpoint).

Look for the thing, not the metric:

- anticipation still compresses before release;
- the recoil remains hard and immediate rather than floaty;
- the extreme holds for at least 16 frames and reads clearly;
- body, large eyes, brow and gaze arrive as one startled expression;
- antenna whip remains secondary and continuous;
- no new one-frame pop appears at hold release or first recoil.

Run `mqa` normal, `--fail-startle-step`, existing `--fail-rootstep`, `meyesize`, `mprobe`, `mnodule`, `mspan`, and the full every-frame visual sheet. Startle has no authored ground-contact exception, so its existing clearance result must remain green.

## Eye-size interaction

Neither repair changes uniform-scale infrastructure.

- Death clips allocate no eye-size track; the Q2 change acts only on EyeTravel parents and remains orthogonal to lids/gaze.
- Startle's `kStartleEyeLargePm=1350` track derives from `kWide`. Any chosen body arrival change must shift `kWide`'s arrival consistently so the visible large-eye payoff does not lead or lag the body accidentally.
- The scale-aware eye leash and broken-inverse control must remain green after the timing change.

## File and commit boundary

Expected implementation files:

- `tools/reel/manafold_clips.h` — extracted eye-only schedule, death dead-tail base, named Startle timing consumption;
- `tools/reel/manafold_art.h` — named Startle timing tuples/runtime selector;
- `tools/reel/zhao_reel.cpp` — validated same-binary visual selectors;
- `tools/reel/manafold_qa_p12.cpp` — relative-quaternion Q2 step, isolated Q2/Startle controls;
- current run reports/evidence.

No generic creature-format, mesh, camera, contact, shell, outline or site-media change is needed.

## Acceptance summary

Q2 closes only when both death settle windows hold a readable fixed-camera eye base without living glance/knead, normal Q2 is green, and `--fail-eyesnap` visibly and numerically restores both snaps.

Q3 closes only when a native every-frame ladder selects a hard readable Startle with a real held arrival, Q3 has useful margin under the unchanged 260 mm ceiling, and the attributed legacy timing control turns slot 4 red.

As I always say, the smoothest field trip is the one where the eyes stay on the destination and the bus saves its speed for the big reveal!
