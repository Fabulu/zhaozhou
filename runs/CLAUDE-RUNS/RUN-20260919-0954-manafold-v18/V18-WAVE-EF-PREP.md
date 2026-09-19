# Manafold version 18 — Wave E/F preparation (Qwen relay, coordinator-verified)

**Status:** preparation only. No source was changed. Every item below was produced by
local Qwen jobs (`qwen/Q001`..`Q006`) and **spot-checked against source by the
coordinator**; per-job verdicts are in `qwen/QWEN-LEDGER.md`. The implementer
still authors by eye and chooses every art value from renders.

## Wave E — live presentation cleanup

### Mist history plane (Hasty smear, Drift/Blown relic trail)
- `zhao_reel.cpp:5879` `s.u02_mist = slot != 7;` in `subject_u02_clip` is the only
  default source of the 48×30 persistent mist for live subjects.
- Turning it off keeps the contour shell. `u02_cover` is built under
  `c.u02_mist || c.u02_shell` and gains the ink ring, and `shell_paint` gates on
  `c.u02_shell && !u02_cover.empty()`. Only the `if (c.u02_mist)` plane block
  drops, and the STAGE-A warning cannot fire (verified).
- **Coordinator correction (Q006 overclaimed):** 35 subjects use
  `subject_u02_clip`. Only `manafold-fogprobe-mist` (and the mist-variant sheet)
  explicitly re-set `u02_mist = true`. `inspect`, `still`, `antenna-fixed`,
  `nodule-solo`, `antenna-quarter`, `trio`, `fogprobe-mana` and `fogprobe-off`
  also inherit the builder default. Decide per subject: flip the default for
  the 22 live site subjects, and keep diagnostics' existing behaviour
  explicitly unless a diagnostic is meant to change.

### Crackle → normal mana, day presentation
- Today: `zhao_reel.cpp:8910-8919`: `u02_mana = 4` (lightning-only) and
  `u02_backdrop(s, 58, 96, 132)`, which gives night: `planet = 1` (violet) and
  `planet_sun_mag = kU02NightSunMagPx = 25` via `kU02NightBackdrop`
  (`:5587`, `:5627-5637`).
- Change: drop the `u02_mana = 4` override (the builder gives 9) and the
  `u02_backdrop` call (day). Add `manafold-crackle-legacy`, a verbatim copy of
  today's block, as the exact same-binary control.
- Note: `channel` makes the same `u02_backdrop` call and stays night unless the
  owner says otherwise. The Crackle comment says the pair was kept in sync
  deliberately, so update that comment.

### Particles through the antenna (bounded attempt)
- Fold/surge motes push `depth_test = true, pre = false` (`manafold_fx.h:2191-2208`,
  `3640-3644`). The pre layer (`zhao_reel.cpp:3267`) draws before the creature and
  the post layer (`:3800`) after it.
- **`glow_splat` depth-tests per pixel against the splat's centre depth and
  never writes depth** (`manafold_fx.h:3933-3990`, coordinator-verified).
  Therefore routing motes through `pre = true` makes the creature overpaint
  every one of them, including motes genuinely in front. That flattens depth
  and fails the plan's own acceptance test. Expect the planned A/B to be
  rejected; run it only as a quick confirmation.
- Likely real cause: each mote is a flat disc at its centre depth. Where the
  disc crosses a round stick, the tube surface slices it, which reads as
  "passing through". Narrowest candidate: fade mote opacity as its 3D centre
  nears the antenna centreline (stick radius + mote world radius), as a named
  toggle with an exact-off control. If that is not cheap, record it as a
  declined bounded attempt, per the owner ("don't spend too much effort").

### Blown / Drift
- Blown inherits mana 9, smear 0 and mist from the builder; its only special
  settings are the camera `cam_k 168000 / cam_ps 9000 / cam_pc 64900`. The relic
  read is expected to be the mist plane. Re-look after the mist is removed.

## Wave F — Flight

- Current Flight is one clock: `cyc = K / kFlightBobPeriodKeys = 4`, where
  `bob = sinp(f,K,cyc)` drives root Y (±`kFlightBobAmpMm` 300) and
  `rise = sinp(..., 0x4000)` (its derivative) drives pitch and gaze. Breath
  squashes at the bottom (`kFlightBreathPhase16 = 0x8000`). Root X is hard-zero
  (`manafold_clips.h:4897`); `kFlightSpeedMmPerKey` is dead. The seam is exact for
  every `sinp` channel because `cyc` is an integer (verified).
- Why it reads as a bounce: a symmetric sine, pitch locked to the derivative,
  nadir squash, and no traverse.
- Ladder knobs to add (constexpr; the file has no env-override pattern for
  these): `kFlightCyclesPerLoop` (replaces the period derivation),
  `kFlightBobAmpMm`, plus shape knobs whose neutral values must reproduce
  today's bytes exactly: `kFlightWaveRiseFrac16` (asymmetric rise/fall phase
  remap), `kFlightPitchLead16`, `kFlightTopHang16`, and possibly breath phase.
  Rungs: amplitude 100/200/300/500 and cycles 1/2/4/6, rendered as independent
  ladders. Framing at `kU02CamKFlight = 250000` must be checked on render at the
  extremes.

## Wave F — framing (Q008, coordinator-verified)

- `cam_pitch` (`zhao_reel.cpp:314+`): `x' = k·xv`, `y' = −k·yv`, so **larger `cam_k` =
  larger creature (zoom in)**. `cam_ps`/`cam_pc` form a sin/cos pitch pair: the
  default 28732/58903 is ≈26° down, and 9000/64900 ≈8°. `cam_bias` is an NDC aim
  shift added through the w row. Raster y is down-positive, so a **more negative
  `cam_bias` raises the creature in frame**; the deaths use −5200 to lift a
  low corpse.
- Trick uses the default camera (`cam_bias 14000`). To lift the planted crown
  off the bottom edge, ladder `cam_bias` at 14000 → 0 → −8000 → −16000 and
  choose by eye (fixed camera; no chase).
- Flight (`kU02CamKFlight 250000`, raised from a 148000 "thumbnail"): more
  vertical travel needs headroom, so ladder **smaller** `cam_k` or a shallower
  pitch. Q008's "raise k" advice was wrong. Also note that the Fall comment
  says the camera "pulls back" while raising k 127000→150000; fix that
  misleading comment if you touch it.

## Wave F — Trick 180 → pause → 360 → overshoot → correct

- **Pivot about the SUPPORT, not the root (coordinator-verified P1).**
  `trick_support_center_y_mm` starts at `(kLoopTubeXMm, kLoopNeckExitYMm, 0)` and
  walks JunctionF→Neck→HingeA→HingeB, so the support has an X offset from the
  root. A world-Y yaw pre-multiplied onto `g.q[kBRoot]` without translation
  drags the planted antenna in a circle.
  - Refactor to `trick_support_center_xyz_mm` (same walk; returns x, y, z).
  - Capture `ref_x/ref_z` at spin start.
  - After the `quat_mul(quat_y(spin), g.q[kBRoot])` pre-multiply, set
    `root_x = ref_x - sx` and `root_z = ref_z - sz`, and write
    `c.root[f*3+0]` and `c.root[f*3+2]`.
  - Root X/Z default to 0 in `clip_shell` (verified), so writing 0 outside the
    spin is byte-neutral. A world-Y rotation preserves support Y, so the
    existing Y pivot is unaffected.
- **Owner's overshoot is required** (Q004 dropped it). Progress in unwrapped
  per-mille: 0 → 1000 + `kTrickSpinOvershootPm` → 1000, as two quintic
  segments `S(x) = 10x³ − 15x⁴ + 6x⁵` (C² at every join). Compute the terms in
  **int64** (int32 overflows). Make the last spin key land exactly on
  progress 1000 (divide by `end - start - 1`, or treat end as inclusive).
  1040 per-mille = 68157 angle16.
- The literal `158` in `f >= kTrickPlantKey && f < 158` (the `kBalFade` guard)
  must become a named constant tied to the lift key.
- **Do not change `kTrickKeys` casually.** Every `sinp(f, K, n)`,
  `antenna_knead(..., K, f)` and `front_flex_play(13, f, K)` re-phases over the
  whole clip if K changes. Either fit the spin inside the existing plant hold,
  or grow K while pinning those oscillators to their current absolute period.
  State the choice.
- Gates to add: unwrapped spin progress from the relative root quaternion vs a
  no-spin control; support XZ drift ≤ 1 mm through the spin; C2 of root XZ at
  the spin joins; support ownership held for the whole spin window. The Wave-D
  lift-key question (`156 → 148`) must be settled first.
