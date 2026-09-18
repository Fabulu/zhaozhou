# Manafold Pass 17 — Trick axis render plan

**Date:** 2026-09-18
**Mode:** read-only art preparation; no source/build/render performed
**Question:** which already-exposed root X/Z path keeps the face readable without sacrificing the authored antenna headstand?

## Protected action

Keep the existing 200 authored keys and their timing:

- anticipation/gather through key 41 (presentation f0000–f0083);
- pitch-over keys 42–77 (f0084–f0155);
- declared antenna plant keys 78–156 (f0156–f0312, including presentation midpoints);
- balance hold through key 148 (through f0296), with its current wobble, show-off yaw and junction flex;
- lift/righting keys 149–185 (f0298–f0371);
- home at key 186 and pleased settle through key 199 (f0372–f0399).

Do not shorten the hold, alter root-height/contact values before a rendered candidate earns that work, or chase the face with the camera. `manafold-trick` is already fixed-camera (`subject_u02_clip(..., false, ...)`), so every candidate compares the same action from the same view.

## Minimal same-binary ladder

The current renderer parses both variables before `u02::type()` and composes the result as `quat_x(flip_x) * quat_z(flip_z)` on the unchanged `kFlip` schedule.

| ID | `ZHAO_U02_TRICK_FLIP_X_A16` | `ZHAO_U02_TRICK_FLIP_Z_A16` | Purpose |
|---|---:|---:|---|
| `legacy-z` | `0` | `-32768` | Required rejected control; reproduces the long +X-to−X rear hold. |
| `pure-x` | `-32768` | `0` | Face-axis/cartwheel half-turn; strongest face-preserving hypothesis. |
| `mixed-x32-z6` | `-32000` | `-6000` | Primary mixed candidate if pure X loses the planted silhouette/contact read. |
| `mixed-x28-z12` | `-28000` | `-12000` | **Conditional tie-break only.** Render only if the first three leave a real visual ambiguity; do not spend a fourth render by habit. |

This is an authored comparison, not an axis derived from the old picture. No winner is selected here.

## Exact build and render commands

Run only after concurrent C–End/outline edits have settled. Use one fresh direct binary for every rung; do not rebuild between candidates.

```powershell
$root = 'C:\programmieren\zencrifice\manafold-p16\zhaozhou'
. "$root\tools\env\zhao-env.ps1"
$gitBash = 'C:\Program Files\Git\bin\bash.exe'
$build = "$root\out\p17-trick-axis"
& $gitBash "$root\tools\reel\build-direct.sh" --output $build --clean cel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = "$build\bin\zhao-reel-cel.exe"
$env:ZIXX_EXP = 'celmain'
$env:ZIXX_LIGHT = 'diagonal-cool-cross'

$env:ZHAO_U02_TRICK_FLIP_X_A16 = '0'
$env:ZHAO_U02_TRICK_FLIP_Z_A16 = '-32768'
& $exe "$root\out\p17-trick-axis-legacy-z" manafold-trick
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$env:ZHAO_U02_TRICK_FLIP_X_A16 = '-32768'
$env:ZHAO_U02_TRICK_FLIP_Z_A16 = '0'
& $exe "$root\out\p17-trick-axis-pure-x" manafold-trick
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$env:ZHAO_U02_TRICK_FLIP_X_A16 = '-32000'
$env:ZHAO_U02_TRICK_FLIP_Z_A16 = '-6000'
& $exe "$root\out\p17-trick-axis-mixed-x32-z6" manafold-trick
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Only if the three-rung review is genuinely unresolved:

```powershell
$env:ZHAO_U02_TRICK_FLIP_X_A16 = '-28000'
$env:ZHAO_U02_TRICK_FLIP_Z_A16 = '-12000'
& $exe "$root\out\p17-trick-axis-mixed-x28-z12" manafold-trick
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Record binary SHA-256/MD5, source commit/dirty declaration, environment and each sequence CRC before looking. Environment validation itself is a control: values outside ±32768 must return RC 2.

## Every-frame evidence

Each subject has 400 presentation frames. Build one complete sheet per rendered candidate with the committed reader; a handful of stills cannot certify the entire planted phrase.

```powershell
$plates = "$root\tools\reel\plates.py"
$run = "$root\runs\CLAUDE-RUNS\RUN-20260917-2318-manafold-pass16-pass17"
python $plates sheet "$run\PASS17-TRICK-LEGACY-ALLFRAMES.png" -4 20 "$root\out\p17-trick-axis-legacy-z\manafold-trick\*.rgb"
python $plates sheet "$run\PASS17-TRICK-PURE-X-ALLFRAMES.png" -4 20 "$root\out\p17-trick-axis-pure-x\manafold-trick\*.rgb"
python $plates sheet "$run\PASS17-TRICK-MIXED-X32-Z6-ALLFRAMES.png" -4 20 "$root\out\p17-trick-axis-mixed-x32-z6\manafold-trick\*.rgb"
```

If the optional fourth rung is rendered, give it the same complete sheet. Review every tile for discontinuity, face loss, body/antenna identity, ground interaction, outline/effect ordering and recovery—not just for the four comparison stills.

## Native same-frame comparisons

The mandatory architecture witnesses are presentation f0115/f0160/f0220/f0295:

- **f0115:** mid pitch-over, before the declared plant;
- **f0160:** early planted balance;
- **f0220:** middle of the long planted hold;
- **f0295:** last held extreme before righting.

Add boundary/recovery witnesses so a flattering middle frame cannot hide a broken transition:

- f0084 (flip starts), f0140 (late approach), f0155/f0156 (pre-plant/plant boundary);
- f0296/f0312 (hold end/contact-window end), f0332 and f0356 (righting/overshoot);
- f0372 and f0399 (home/final settle).

Use `plates.py grid ... 1` for native same-frame A/Bs, arranged frame-major: legacy, pure X, mixed at the same frame before advancing. The primary native plate must include at least f0115/f0160/f0220/f0295. Make a second transition/contact plate for f0155/f0156/f0296/f0312/f0332/f0356/f0372.

The committed rejected reference remains `FINAL4-BATCH-07-trick-rotation.png` (and the complete `final4-sheets/manafold-trick.png`). The new `legacy-z` output must visibly reproduce that rear-mass fault; otherwise the ladder is not comparing the same binary/path and must stop.

## Exact 4× evidence

After the native grid identifies one crop that contains the complete face, eye pair and adjoining antenna/body silhouette in **all** candidates, declare that one native `(X,Y,W,H)` box in the evidence manifest and reuse it without movement. Then run:

```powershell
python $plates crop "$run\PASS17-TRICK-AXIS-FACE-4X.png" 4 3 `
  "L160:$root\out\p17-trick-axis-legacy-z\manafold-trick\0160.rgb@X,Y,W,H" `
  "X160:$root\out\p17-trick-axis-pure-x\manafold-trick\0160.rgb@X,Y,W,H" `
  "M160:$root\out\p17-trick-axis-mixed-x32-z6\manafold-trick\0160.rgb@X,Y,W,H" `
  "L220:$root\out\p17-trick-axis-legacy-z\manafold-trick\0220.rgb@X,Y,W,H" `
  "X220:$root\out\p17-trick-axis-pure-x\manafold-trick\0220.rgb@X,Y,W,H" `
  "M220:$root\out\p17-trick-axis-mixed-x32-z6\manafold-trick\0220.rgb@X,Y,W,H" `
  "L295:$root\out\p17-trick-axis-legacy-z\manafold-trick\0295.rgb@X,Y,W,H" `
  "X295:$root\out\p17-trick-axis-pure-x\manafold-trick\0295.rgb@X,Y,W,H" `
  "M295:$root\out\p17-trick-axis-mixed-x32-z6\manafold-trick\0295.rgb@X,Y,W,H"
```

Do not invent coordinates before looking at the native frames, do not silently clamp a crop, and do not let the 4× plate substitute for the native judgement. Add a separate fixed crop around the antenna/terrain contact at f0155/f0156/f0311/f0312 if the full-frame native plate leaves contact visually ambiguous.

## Selection order

1. Review the three every-frame sheets at native-sheet scale.
2. Reject any candidate that loses both eyes/face for a sustained section, ceases to read as an inverted antenna balance, introduces a snap, or makes the body/antenna contact visually implausible.
3. Compare the native f0115/f0160/f0220/f0295 plate.
4. Use fixed 4× crops only to confirm suspected eye/outline/contact failures.
5. Render `mixed-x28-z12` only if pure X and the primary mixed rung each pass all stop conditions but differ in a real unresolved artistic tradeoff.
6. Select by the whole moving read. Do not average axes or derive a new pair from image measurements.

## Production gate after selection

The renderer environment ladder is visual only; `mprobe` does not consume those environment variables. After a winner is selected, write its exact X/Z values into the named shipping constants, leave legacy 0/−32768 as the same-binary control, rebuild from the settled source, and run:

```powershell
. "$root\tools\env\zhao-env.ps1"
& $gitBash "$root\tools\reel\build-direct.sh" --output "$root\out\p17-trick-selected" --clean mprobe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$root\out\p17-trick-selected\bin\manafold-probe.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Do not move the existing declaration to admit the result. Slot 13 must retain:

- contact window keys 78..156 (presentation f0156..f0312, including midpoint checks);
- declared penetration 25 mm and the probe's existing accepted −60..−5 mm band;
- approach clearance before the window (the current known baseline bottoms at key 75 subframe 1, not inside the declaration);
- unchanged root-height schedule unless the selected rendered pose demonstrates that an authored contact correction is needed.

Then build/run `mmeshcheck`, `mspan`, `mnodule`, `meyesize`, `meyecam`, `mqa`, and a fresh `cel` renderer from one settled tree. `meyecam` is the camera-relative face instrument; its clean output is comparison evidence, not a replacement for the every-frame pictures. Re-render the selected normal and legacy control from that final binary and require the selected normal to match the chosen ladder rung while the legacy control restores the rear hold.

## Objective stop conditions

Stop and re-author rather than selecting if any applies:

1. The legacy rung does not reproduce the broad featureless rear/back mass across f0160–f0295.
2. The candidate loses either eye/face or makes the antenna identity unreadable for a sustained portion of the planted phrase f0156–f0295.
3. The action no longer reads as a headstand balanced on the antenna—e.g. it reads as a sideways tumble, body roll, or body-ground plant.
4. The f0155→f0156 plant or f0311→f0312 lift boundary snaps, slides, floats, or changes support abruptly.
5. The existing 3D contact/depth/clearance probe fails. Re-author the path/root contact; never widen thresholds or move the camera.
6. The continuous antenna, repaired C–End run, complete lower/inside O outline, top-line depth ordering, body shell or eye nesting regresses.
7. The righting/overshoot/recovery f0296–f0399 is shortened, frozen, discontinuous or fails to return to the opening pose family.
8. A candidate looks acceptable only in a 4× crop but not at native 384×240.
9. Pure X and primary mixed both fail: do not blend their numbers mechanically. Diagnose the visible failure, then author a new named candidate if warranted.

**Acceptance sentence:** Trick is ready only when one unchanged-camera, full-length headstand keeps the face, both eyes and antenna identity readable through the entire planted hold, retains authored antenna contact and recovery under the committed 3D probe, and the same final binary's legacy control visibly restores the rejected rear-mass fault.
