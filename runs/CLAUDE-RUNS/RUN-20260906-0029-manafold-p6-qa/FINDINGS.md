# Manafold pass 6 - QA FINDINGS

**Run:** RUN-20260906-0029-manafold-p6-qa
**Lane:** manafold-p6-qa/{zhaozhou,Upheaval} - own clones of the implementer
lane at zhaozhou 83be0c66 / Upheaval bd80b5c. Build tree manafold-p6-qa/build,
never `cmake --build`. **No creature constant changed. Nothing published.**

---

## 0. CALIBRATION - is the instrument honest?

**Built the shipped SHA myself and reproduced its published numbers.**
`bash tools/reel/build-direct.sh --output <lane>/build mprobe`, then ran
manafold-probe.exe. Every published probe number reproduced exactly: per-slot
clearance, closure sweep 1059 pm, closure over the bank 1064 pm, eye crown
lens 1235 / cyan 1247 / white 1256 pm, trick contact -23 mm. The implementer
figures are honest transcriptions of the tool.

**I could NOT calibrate the fog against pass 5.** Both fog-probe subjects and
manafold-still were introduced *in* pass 6, so the pass-5 SHA cannot render
them. Per-clip CRC calibration against pass 5 was delegated (section 9).

Frames read with `tools/reel/rgbframe.py` throughout (checklist item 3).

---

## 1. THE COMPOSED-EXTREMES PROBE (5d)

**Verdict: the implementer is RIGHT that the gate is broken and WRONG about
why. The geometry at the composed extremes is NOT safe. Nothing ships broken.**

### The cause that was never found: a UNITS bug

`u02::fxu(mm) = mm * 65536 / 1000` - Q16.16 in which **1.0 is one METRE** while
the argument is millimetres. manafold_probe.cpp converts back with `>> 16`,
which yields **metres**, and prints it as "mm". Everything under 1000 mm
truncates to zero.

    QA units: fxu(1000mm)=65536 ; correct (raw*1000)>>16 = 1000 mm ; (raw>>16) = 1

That is the entirety of "gate A returns 0 mm at every amplitude including zero
roll". The lenses are ~98 mm apart at rest: 98 mm -> 0.098 m -> shift -> 0.
Two candidate causes were eliminated by rebuilding; the third was never
suspected because the symptom LOOKED like a classification bug.

**The sv.b0 bone-id read that was replaced was in fact correct.** I ran the bone
split and the z-sign split side by side over all 16 corners: they agree
everywhere below the collision amplitude (cross-centre verts = 0).

### What the geometry actually does (correct units)

Composed extremes, 16 corners of roll x gaze-side x gaze-lift x eyeball-shift:

| Roll | Min vertex gap | LEFT verts inside the RIGHT lens ellipsoid |
|---|---|---|
| 0.0 deg | 98 mm | 0 |
| 4.0 deg | 41 mm | 0 |
| 6.0 deg | 14 mm | 0 |
| 6.5 deg | 7 mm | 0 |
| **7.0 deg** | **0 mm** | 0 (grazing) |
| 7.5 deg | 6 mm | 80 (at the tapered tips - see caveat) |
| 8.0 deg | 13 mm | 80 (at the tapered tips) |
| **10.0 deg (the shipped clamp)** | 18 mm | 24 (at |ly| 829 pm) |

The min-vertex gap falls monotonically from 98 mm to **0 mm at 7.0 deg** and
then rebounds - the signature of two sparse point clouds passing THROUGH each
other, which is why nearest-vertex distance is not by itself a penetration test
(checklist item 16: what does the probe under-test?).

**What is DEFINITE:** the two eye assemblies close from 98 mm to **under 1 mm**
(integer truncation floors it at 0) at 7.0 deg of roll, against the gate own
12 mm floor. That is a direct vertex-to-vertex measurement and depends on no
surface model. The eyes touch, at 7 deg, inside the 10 deg the creature is
clamped to. **Gate A was reporting a real fault and was switched off on the
belief that the instrument was wrong. The instrument WAS wrong - and its
verdict was right.**

**What I must NOT overstate, having gone looking for the refutation of my own
number:** the ellipsoid containment test flags 80 vertices inside at 7.5 deg and
24 at 10 deg, but those vertices sit at **|ly| = 829-994 pm of the lens
half-length** - at the tapered tips, where kEyeLensWidthPm falls to 260 and then
0 and my ellipsoid therefore OVERSTATES the real lens. So sustained
interpenetration past 7 deg is *indicated, not proven*. The contact at 7 deg
stands; the depth beyond it does not. A pass-7 gate should test against the real
swept profile, not an ellipsoid.

**My instrument can fail.** Known-bad: translate each eye 40 mm toward the
centre plane. Separation moves 98 -> 18 mm at roll 0 and 13 -> 3 mm at 8 deg,
and the inside-count rises. It responds.

### But nothing ships broken

Over the whole shipped bank - every clip, every key, 2204 frames:

    QA TOTAL clip frames with an eye-vs-eye intersection: 0

The closest any shipped frame comes is **1514 pm of the lens surface** (51%
clear) with a **42 mm minimum vertex gap**. The reason is 1b.

*Declared limit of my own scan:* I walked authored KEYS, not the 60 Hz
presentation midpoints the clearance gate also walks. Immaterial here only
because no clip drives the eye channels at all (1b), so there is nothing for
interpolation to reach between keys - but a pass-7 version of this scan should
cover both subs, like the clearance gate does.

### 1b. THE REASON - and it refutes a "WHAT SHIPPED" claim

`apply_eye_roll()` has **zero callers in any clip builder**. Its only callers
are manafold_probe.cpp:531 (the gate) and my own probe.

**Implementer FINDINGS section 1 lists "5d the eye roll, shipped at 10 degrees"
under WHAT SHIPPED. REFUTED.** The mechanism is built and clamped; no clip ever
rolls an eye. The owner asked for it "just for expressiveness" on a creature
with no mouth and no nose, and the creature never uses it. That is why every
clip eye separation sits at essentially the rest value.

Two more pass-6 eye features are in the same state:

| Feature | Direction | Callers in clip builders | Disclosed as a gap? |
|---|---|---|---|
| `apply_eye_roll` | 5d | **0** | **No - listed as SHIPPED** |
| `apply_eye_shift` | 5c | **0** | Yes (item 11) |
| `apply_gaze_lr` | 5b rule 4 | **0** | **No - not mentioned** |

### 1c. kEyeShiftMaxPm - confirmed absent, but WORSE than absent

**CONFIRMED not shipped in any clip; REFUTED as cleanly absent.** It is
half-wired, and the surviving half is degenerate:

    constexpr int32_t kEyeShiftPivotMm = 0;   // NOT SHIPPED
    return shift_mm * 10430 / (kEyeShiftPivotMm > 0 ? kEyeShiftPivotMm : 1);

The fallback divides by one millimetre instead of the pivot radius:

    QA: eye_shift_a16(1000) = 166880 a16 = 916.7 deg  (196.7 deg mod one turn)

A "10% of the eyeball width" slide evaluates to a **916.7-degree rotation**. No
clip calls it, so no shipped frame is affected - **but the committed 5d gate
calls it at full amplitude in all 16 corners**, posing both eyes through a
~197 deg flip before measuring them. Gate B, the one that IS enforced and is
reported as "proved responsive", measures those corrupted poses. Its pass is
not evidence of anything.

---

## 2. 5c CONTAINMENT - all three rules are gated, and all three are INERT

**Verdict: the rules are in the committed probe, not in a comment - that half
of the claim is CONFIRMED. But none of the three can fail, and rule 3 never
executes at all.**

The same shift-by-16 units bug:

    off_mm = (abs lz) >> 16;                      // METRES - always 0
    rim_mm = kEyeWideMm * w_pm / 1000;            // real mm, <= 84
    over   = off_mm - rim_mm;                     // therefore always <= 0

Three consequences at once:

1. **Rule 1** - worst_overhang_mm starts at 0 and only rises on `over > 0`, so
   it can never leave 0. The probe prints `worst 0 mm`.
2. **Rule 2** - `if (over <= 0) ++on` counts every vertex, so the on-purple
   fraction is pinned at 1000 pm.
3. **Rule 3** - the body-outline test is guarded by `if (over > 0)`. **The loop
   never runs.** Its violation counter is structurally zero.

The implementer read rule 1 zero as "an honest finding, not an inert gauge -
clips author gaze as FRACTIONS of the clamp, so none reaches it" (item 13).
**REFUTED.** The zero is the units bug. This eye containment has now been
redesigned three times and gone wrong silently three times - this time inside
the gate written to stop exactly that.

### What the leash says when measured correctly

Re-measured with (raw*1000)>>16 AND the correct frame. `inv_point` against a
SKINNING matrix returns BIND space, not eye-bone space, so the eye bind offset
must be subtracted - the shipped probe omits this too, a second independent
error in the same expression.

Sanity check first: at rest the star sits **9 mm inside** the rim, which is what
a star drawn to fit a lens should read - so the frame is right.

    QA gaze   0% of kGazeMaxA16 ( 0.00 deg): worst overhang   -9 mm  on-purple 1000 pm  OK
    QA gaze  50%               (12.63 deg): worst overhang   10 mm  on-purple  982 pm  OK
    QA gaze 100%               (25.27 deg): worst overhang   25 mm  on-purple  903 pm  BREACH (cap 24)

**Gaze alone is well tuned** - it exceeds the 24 mm cap by 1 mm at full
amplitude. A knob, not a fault.

Over the shipped bank, however:

    QA 5c rule 1: worst overhang 142 mm (cap 24 mm, star half-width 80 mm) at slot 2 key 115 - FAIL
    QA 5c rule 2: worst fraction of the star on the purple 220 pm (floor 600)              - FAIL

**The driver is `apply_twinkle`, not gaze.** `build_channel()` spins the star up
to **119 degrees** across keys 56-139 (kBlazeTwinkleA16 = 10923 = 60 deg,
doubled). The star is deliberately **asymmetric** - kStarArmBottomMm = 216, top
167, side 68 - so spinning it swings a 205 mm arm from along the lens (270 mm
long) to across it (84 mm half-width). At key 115 the spin is 84 deg:
near-perpendicular, the worst case. 142 mm is **5.9x the cap** and 1.8x the
star own half-width, with **22% of the star still on the purple** against a
60% floor.

### And it is visible - I looked

manafold-channel and manafold-rest rendered from the shipped binary, read with
rgbframe.py; plates in manafold-p6-qa/plates/:

* **The far eye star hangs off the purple onto the pink body and, at the top,
  against the SKY** - at rest, with no gaze extreme at all. That is rule 3 exact
  prohibition ("overhanging into the sky is a detached artefact"), and rule 3 is
  the loop that never runs.
* channel at key 139 shows the star fully clear of the lens outline.

---

## 3. A DEFECT NEITHER SIDE REPORTED: BLACK NOTCHES IN BOTH EYES, EVERY FRAME

Found by looking, not by a gate.

Every lens carries hard near-black wedges at both tips and along the rim, on
**100% of the frames of both clips I rendered**:

    manafold-rest      frames 400 | frames with dark-notch px: 400 (100%) | mean 22.1 px | max 41
    manafold-channel   frames 420 | frames with dark-notch px: 420 (100%)

Values around (16,8,33) - luminance ~57 - against a lens body mean of
(84,36,136), luminance ~256. Not shading: a 4.5x step with a hard edge.

**Cause.** `make_eye_lens()` sets `p.caps = kCapTop | kCapBot`, and the width
profile is

    kEyeLensWidthPm[11] = {0, 260, 505, 715, 880, 1000, 880, 715, 505, 260, 0}

zero at both ends. A cap polygon on a zero-radius ring is degenerate, its normal
is undefined, and the toon lighting dots to nothing. This is the project own
documented ghost - "every automated gate passed while a stray triangle sat in a
creature eye" - shipped again, on the published clips, on the only facial
feature this creature has. Plate: plates/rest-eye-notches.png (magenta =
luminance < 90).

---

## 6. THE FOG - the regression is real; the diagnosis is wrong

**Verdict: the implementer declares the fog "thinner where Direction 5 asked for
thicker" and blames coupling with E.5. The real cause is an off-by-one that
switches the smear plane OFF ENTIRELY on 12 of 15 clips. This is the most
damaging finding of the pass.**

zhao_reel.cpp:3264:

    const u02::SmearPreset& sp =
        u02::kSmearPresets[c.u02_smear_preset >= 0 && c.u02_smear_preset < 5
                               ? c.u02_smear_preset : 0];
    if (sp.gain_pm > 0) { ...the entire smear plane... }

`kSmearPresets` has **SIX** entries. Pass 6 stage E.5 added rung 5, SHORT/TORN.
The hand-written bound is still `< 5`, so index 5 fails the test and falls back
to **index 0 - {0,1,0,1,0,0}, gain_pm = 0** - and `if (sp.gain_pm > 0)` then
skips the smear plane completely.

Rung assignment, zhao_reel.cpp:5151:

    s.u02_smear = slot == 7 ? 0 : ((slot == 1 || slot == 8) ? 3 : 5);

| Slot | Clip | Authored rung | Actually used |
|---|---|---|---|
| 0 | hover | 5 | **0 - smear OFF** |
| 1 | drift | 3 | 3 |
| 2 | **channel** (the house look) | 5 | **0 - smear OFF** |
| 3 | curious | 5 | **0 - smear OFF** |
| 4 | startle | 5 | **0 - smear OFF** |
| 5 | rest | 5 | **0 - smear OFF** |
| 6 | pirouette | 5 | **0 - smear OFF** |
| 7 | still | 0 | 0 |
| 8 | hasty | 3 | 3 |
| 9 | fall | 5 | **0 - smear OFF** |
| 10 | hit | 5 | **0 - smear OFF** |
| 11 | taunt | 5 | **0 - smear OFF** |
| 12 | taunt2 | 5 | **0 - smear OFF** |
| 13 | trick | 5 | **0 - smear OFF** |
| 14 | damage | 5 | **0 - smear OFF** |

**Twelve of fifteen shipped clips have no smear plane at all** - including
channel, the mana treatment the owner personally chose, live on the site now.

### Proved by a ONE-BYTE A/B, one binary per arm

I compiled a copy of zhao_reel.cpp differing in exactly one byte (`< 5` to
`< 6`; verified "bytes differ: 1 of 361958") against the SAME object files, and
rendered both arms:

| Subject | Shipped CRC | One-byte-fixed CRC | Unique colours |
|---|---|---|---|
| manafold-channel | 0x938FC7E6 | **0x8274224A** | 17019 -> 17338 |
| manafold-rest | 0xADF5C764 | **0x10009131** | 17749 -> 17946 |

Both move. Since the binaries differ only in that bounds check, this proves
index 5 was being rejected and the clips were running preset 0.

**Honest magnitude:** restoring the rung changes 2.3% of pixels on rest
(mean absolute delta 0.47 of 765). Real and provable, but **subtle** - and rung
5 is the LOW-accumulation rung (keep 620 / gain 420 against rung 3 900 / 520),
so even correctly wired it will not deliver the owner "thicker by a lot" on its
own. Both facts matter for pass 7.

### Consequences

* The owner "some of that glitchy smear we had in others" (Direction 5, 0-BIS)
  is **unmet on 12 of 15 clips**, not partially met.
* Stage E "a new SHORT/TORN smear rung chosen by motion class", listed under
  WHAT SHIPPED, **did not ship**. It is unreachable code.
* The fog IS the smear plane. On those clips it is not thinner - it is
  **absent**. So D.2 (a fog shell with its own knobs) would be built on top of a
  disabled feature.

**And it is the exact bug class this same pass wrote a lesson about.** From
manafold_art.h:879, written in pass 6 for kKneadClipPm:

> "the guard over this array is DERIVED from the array, never hand-written. The
> literal `< 14` orphaned slot 14 once (damage silently ran at 700 against its
> authored 250); `< 15` was the same bug one index later, waiting for the next
> slot to be added."

The lesson was applied to one array and the identical bug committed against
another in the same pass. kSmearPresets is the only other hand-written bound of
this shape I found in the creature-02 paths.

### Measured from the shipped binary

Ablation pair manafold-fogprobe-{mana,off} over rest, 40 sampled frames:

    mean absolute delta per pixel (RGB sum): 4.33 / 765
    coverage (>6/765): 4.5% mean, 6.8% max
    rest f200: 3533 changed px = 3.8% of the frame

**UNVERIFIED against pass 5** - the fog-probe subjects post-date it, so a
like-for-like ablation cannot be run there.

kBellyGlowGainPm is **not** double-duty now: it is 0, read in exactly one place
(`s.u02_glow = kBellyGlowGainPm > 0`), so the belly glow is simply off.

---

## 5. manafold-inspect COLLAPSED INTO manafold-hover

**Duplicate CONFIRMED, caption CONFIRMED, and the shipping light rig was NOT
lost - it was universalised.**

`subject_u02_clip()` now sets `s.creature_moving_light = true` for **every**
clip subject (zhao_reel.cpp:5095); manafold-inspect sets the same flag
redundantly. The per-clip kU02Sun* suns stop firing under the moving rig, so
hover passing &kU02SunHover and inspect passing nullptr makes no difference -
hence the byte-identical render. The rig is now on all 15 clips.

The site caption is explicit and honest: "NOW IDENTICAL TO HOVER, and that is
the point... every clip above now has what only this one had -- and the two
clips render byte-for-byte the same."

**But one inherited caption is now false.** The hover card still reads "under
the hover sun over the Cool Cross base rig - the shipping presentation." The
hover sun does not fire any more.

---

## 7. MEDIA INTEGRITY - CONFIRMED, and the claim understates itself

Verified by **full decode** (`ffmpeg -v error -i <f> -f null -`), never by byte
length.

| Set | Count | Present | Non-zero | Decoding |
|---|---|---|---|---|
| creatures.json manifest | 479 | 479 | 479 | **479** |
| What public/index.html actually declares | 949 | 949 | 949 | **949** |

Zero missing, zero zero-byte, zero non-decoding; 162,954 video frames decoded,
minimum 46 frames in any clip. The manifest 479 is a strict subset of the 949 -
it omits **470 poster PNGs** the page depends on, so a manifest-only check has a
470-file blind spot.

**A gate that cannot fail, found in the verification tooling itself:** ffprobe
returned **exit 0 and reported codec_name=vp9** for a deliberately truncated
2000-byte webm stub. Only the full `-f null -` decode caught it (rc=183, "File
ended prematurely"). Any future media gate built on ffprobe would pass
truncated files. 0-byte files fail both checks.

Ten manalab-* variants: all present, 800 decoded frames each, all HTTP 200
video/webm live, two re-downloaded and hashed identical to disk.
**Naming correction:** manalab-edge-snap-held-still and
manalab-edge-snap-past-the-wall do not exist; the real names are
manalab-held-still and manalab-past-the-wall.

Archives intact: 9 generations, 344 clips, identical counts live and in the
clone, three full downloads byte-identical and decoding. The live page is
byte-identical to the committed one (both 319,899 bytes, same sha256), so the
deploy matches bd80b5c. 12 undeclared orphan media files sit on disk; all
decode; harmless.

---

## 8. GATES THAT CANNOT FAIL

Each fed a known-bad input or shown structurally inert.

| Gate | Status | Load-bearing? |
|---|---|---|
| 5d gate A (eyes never touch) | Inert - units bug returns 0 m; disabled by hand | **YES - it was right** |
| my own ellipsoid depth test | Over-reports near the lens tips - stated, not relied on | n/a |
| 5d gate B (nothing clips) | Runs, on poses corrupted by the 916.7 deg eye-shift | **YES** |
| 5c rule 1 (overhang cap) | Cannot exceed 0 - units bug | **YES** - real breach 142 mm |
| 5c rule 2 (>=60% on purple) | Pinned at 1000 pm | **YES** - real value 220 pm |
| 5c rule 3 (never cross the body outline) | **Loop never executes** | **YES** - visibly violated |
| Smear rung selection | **No gate at all**; silent fallback to "no smear" | **YES** - 12/15 clips |
| ffprobe as a media check | Passes truncated files (exit 0, reports vp9) | Yes, if adopted |

**The clearance, closure, protrusion and travel gates are sound** and reproduce
exactly. The units bug is confined to the two blocks added this pass; the older
probe code converts correctly.

---

## 10. WHAT IS RIGHT - protect these

1. **The clearance / closure / protrusion / travel probe is honest** and its
   numbers reproduce from a build I made myself. The thin closure margin (bank
   1064 vs gate 1120) is correctly reported as thin.
2. **kU02CamK = 360000** is the cheapest win in the pass. The eyes are legible
   at native for the first time and the loop window reads as a pocket. Keep it.
3. **The many-colour rig on every clip** is a correct, complete delivery of
   Direction 5 section 8, and collapsing inspect into hover was right - honestly
   captioned rather than shipped as a silent duplicate.
4. **The implementer disclosure discipline is excellent.** Fourteen gaps listed
   unprompted, including the one that led me to the units bug. The failures in
   this pass are instrument failures, not honesty failures.
5. **kKneadClipSlots derived from its own array** is the right fix - it simply
   was not applied to the one other array that needed it.
6. **Media and site hygiene are clean**: 949/949 decoding, archives intact, live
   page byte-identical to the committed one.
7. **The star is authored asymmetric** (216/167/68) rather than a symmetric
   cartoon star - the artist drawing, kept. The overhang problem is the twinkle
   spinning it, not the shape.

---

## 11. RANKED - WHAT PASS 7 MUST FIX

1. **The smear-preset off-by-one** (zhao_reel.cpp:3264, `< 5` to a bound derived
   from kSmearPresets). Twelve of fifteen clips ship with no smear plane, the
   owner explicit "glitchy smear" ask is unmet, and the "fog is thinner" report
   has the wrong cause. One character - then re-judge D.2/E.5 from a build where
   the feature actually runs. **Derive the bound; do not retype it.**
2. **The shift-by-16 units bug in manafold_probe.cpp** (5c rules 1-3, 5d gates
   A/B). Five gates on real constraints, all inert. Fix, then re-enable gate A.
3. **The eye-lens black notches.** Both eyes, every frame, on the published
   clips, on the only face this creature has. Cap a zero-radius ring, clamp the
   kEyeLensWidthPm ends off zero, or drop the caps.
4. **Star containment under apply_twinkle.** 142 mm overhang vs a 24 mm cap,
   22% on-purple vs a 60% floor, visibly detached on channel and rest. Either
   the twinkle stops spinning an asymmetric star through 119 deg, or the leash
   constants are re-authored BY EYE and the gate re-derived.
5. **The eye_shift_a16 divide-by-1 fallback** (916.7 deg). Wire kEyeShiftPivotMm
   properly or make the unshipped path return 0 - never a silent 197 deg flip
   that only the gate exercises.
6. **Actually USE the eye roll.** 5d is built, clamped and never called. The
   safe budget is **under ~6 deg**, not the 10 deg the constant allows - the
   eyes make contact at 7. kEyeRollMaxA16 should come down and clips should
   spend what remains.
7. **apply_gaze_lr** (5b rule 4, "asymmetry is allowed and wanted") is built and
   never called. Use it or report it.
8. **Correct the hover caption** - it names a sun that no longer fires.
9. Carried from the implementer own list, not re-litigated: D.2 fog shell, E.4
   saturation, F.2-F.6 clip inventory, C.5 deform fill.

---

## 12. VERIFIED vs INHERITED

**Verified by me, from builds I made:** every probe number at the shipped SHA;
the units bug and its five consequences; the true eye separation and the 7 deg
contact (and the limit of my own ellipsoid depth test); zero intersections across the shipped bank; the dead apply_eye_roll /
apply_eye_shift / apply_gaze_lr; eye_shift_a16 = 916.7 deg; the star overhang
and its twinkle cause; the black lens notches; the smear off-by-one and its
one-byte A/B; the inspect/hover rig collapse; the fog ablation magnitude.

**Verified by delegated agents:** the media decode sweep and live site
(section 7); per-clip CRCs and the Zixxtrixx baseline (section 9).

**Could NOT verify:** the fog against pass 5 (the probe subjects post-date it);
the rejected-rung reasoning (white:cyan 1.33/1.35/1.40) - not re-derived;
inkmask.py current state - not re-tested this pass.

**Instruments written this run** (committed, per the no-orphan-probe rule):
tools/reel/manafold_qa_extremes.cpp - correct-units eye separation, ellipsoid
interpenetration, roll and gaze sweeps, whole-bank scan, known-bad injection;
tools/reel/manafold_qa_stray.cpp - per-meshlet vertex census.
