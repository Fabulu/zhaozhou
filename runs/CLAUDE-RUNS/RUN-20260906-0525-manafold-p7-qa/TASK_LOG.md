# Task Log: RUN-20260906-0525 - [Describe objective here]

**Created:** 2026-09-06 05:25 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0525-manafold-p7-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 05:25 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0525
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

# PASS 7 QA — adversarial audit

## Lane
Own clones from GitHub, NOT any other agent's checkout:
`C:\programmieren\zencrifice\manafold-p7-qa\{zhaozhou,Upheaval}`
- `zhaozhou` @ `6f840909` (the published SHA)
- `Upheaval` @ `097ffbe`
`git diff 9a97c4f3 6f840909` touches ONLY run logs, so my tree is byte-identical
to the one the shipped CRCs were rendered from in the reel closure.

## CALIBRATION FIRST (gate item 4)
`bash tools/reel/build-direct.sh --output <lane>/build cel` -> **BUILD_RC=0**,
read from the recorded exit code, not from a pipeline tail.
`zhao-reel-cel.exe` = **2,739,759 B** — byte-for-byte the size the publish run
recorded. md5 `982d2375dfa3edeea710b523d96a3d16`, recorded BEFORE any render and
re-checked after every subsequent compile: unchanged throughout, so every number
below belongs to one binary (gate item 20).

## The committed probe, from MY OWN build — every headline reproduces
`manafold-probe.exe`, PROBE_RC=0. Identical to the published numbers:
  trick slot 13 declared contact deepest -23 mm (declared -25)      OK
  CLEARANCE CONTRACT HOLDS (>= 40 mm everywhere)
  5c rule 1  worst 29 mm / cap 29        OK
  5c rule 2  worst 760 pm / floor 600    OK
  5c rule 3  1499 violations             REPORTED-NOT-ENFORCED
  5d gate A  22 mm / floor 12            OK
  5d gate B  801 pm / floor 1000         OK
  eyeball-shift NOT SHIPPED, declared by the probe itself
Slot histogram also reproduces exactly: s0:368 s1:90 s2:18 s3:202 s5:176
s6:476 s12:169 = 1499.

## Slot -> clip map (from manafold_clips.h)
s0 hover-idle · s1 drift · s2 channel · s3 curious · s5 rest · s6 pirouette ·
s12 taunt-lasso. **s6 pirouette, "one slow full yaw", is the worst offender.**

## §2a (claim 7) — the instrument measures the SKELETON, not the camera
- `manafold_hinge_traj.cpp` reads `zc::decode_pose`, never pixels. The
  fixed-camera subject `manafold-antenna-fixed` (zhao_reel.cpp:7277) really does
  hold camera AND root still, and kills mana/smear/planet. Both honest.
- `hinge_trajplot.py selftest` PASSES here and can fail (flat 0.00 mm vs moving
  80.00 mm; locked r=1.000 vs independent r=0.000).
- Correlations reproduce EXACTLY on slot 2: adjacent 0.482–0.717, distant
  -0.011..0.198. **Claim's 0.48–0.72 confirmed.**
- BUT the tool defaults to slot 2 only, and the range is clip-specific: on
  slot 11 (taunt — the source's OWN "hinge-play showcase") adjacent pairs run
  0.259–0.540. The published band is under-windowed (gate item 7).
- **NO COMMITTED TOOL EMITS DEGREES.** The "20–30 deg each" headline is not
  reproducible from any instrument in the tree. Derived it myself from
  chord/segment-length: channel = 20.2/23.8/31.8/25.0/12.1/45.9 deg. Substance
  holds (hinges do move separately with real range) but the true spread is
  **12–46 deg**, not 20–30: hingeC is only 12 deg and hingeD is 46.
- `hingeD`'s reported 3D range is 2085 mm against 118–198 mm for every other
  joint, because "own" motion is measured at each bone's own lever arm. The mm
  ranges are NOT comparable between joints and the tool does not say so.

## The experiment probes (committed, per CLAUDE.md's "commit the probe")
`tools/reel/manafold_qa_p7.cpp` (E1/E1b/E3) and `manafold_qa_p7b.cpp` (E4).
Compiled directly against the objects `build-direct.sh` had already produced, so
`zhao-reel-cel.exe`'s md5 never moved. NO creature constant was changed.

### E1 — IS THERE A FOURTH BUG? A candidate, and it is IMMATERIAL. (my own hypothesis, REFUTED)
5c rule 3 removes the root TRANSLATION but not the root ROTATION, while the
clearance block 200 lines above it records in its own words that "subtracting
only the translation lied twice ... the TUMBLE rotates the body". Pass 7 fixed
half of the lesson it quoted.
Recomputed rule 3 in the FULL root-local frame with the view directions carried
into it:
    shipped (translation only)      1499 violations
    rotation-corrected (R^T(p-t))   1512 violations
**13 samples, 0.9%.** Real as a defect, immaterial as a number, because the body
spheroid has equal x/z radii and is therefore invariant under the yaw that
dominates this bank. Worth fixing for correctness; NOT a fourth headline.

### E1b — WHAT THE 1499 ACTUALLY IS (claim 3)
Bucketed the shipped violations by how side-on the fixed world view is to the
creature's own facing:
    front-ish (<40 deg)   270
    oblique (40-70)       982
    side-on (>70)         247
I expected the count to be contaminated by the side-on case the probe declares
out of scope. **It is not** — only 16.5% is side-on. 82% sits at front-to-oblique,
which IS the shipping camera's range. So the count is not an artefact of the
declared approximation.

### E3 — A GATE THAT CANNOT FAIL (new, unreported)
All three 5c rules select the star by a hard-coded literal `246,242,250`. Fed a
colour no meshlet carries:
    star verts walked 0
    rule 1 0 mm / cap 29 -> OK | rule 2 1000 pm / 600 -> OK | rule 3 0 violations
**All three rules pass VACUOUSLY and the probe prints no warning.** The 5d block
prints an instrument census (153216 white-star, 73920 lens verts); the 5c block
has none. Gate checklist item 6: "a verification tool that finds ZERO of the
thing it counts should say so loudly." Change the star's colour constant and
5c silently goes green.

### E2 — GATE A's UNPROVEN MONOTONICITY (my own hypothesis, REFUTED)
Gate A sweeps roll but PINS gaze at its signed extremes, asserting gaze is
monotonic — the same assumption roll had just violated. Swept gaze too
(3696 corners = 16 signs x 21 roll x 11 gaze):
    closest approach 22 mm, at roll FULL and gaze ZERO -> OK
Identical to the shipped 22 mm. My sweep is a strict superset of the shipped
gate and finds nothing it missed. **Gate A's headline is correct and robust.**

### E4 — CAN GATE A FAIL? YES, PROVED — and there is a SECOND basin (new)
`apply_eye_roll` hard-clamps pm to +/-1000, so the gate structurally cannot be
driven past the cap. Drove the two roll joints directly at raw angle16 (exactly
what raising kEyeRollMaxA16 would do) without touching a constant:
     a16    deg   closest_mm
       0   0.00      91  OK
     900   4.94      22  OK   <-- SHIPPED CAP
    1000   5.49      14  OK
    1040   5.71      11  FAIL   <-- first failure
    1260   6.92       0  FAIL   <-- minimum
    1700   9.34      24  OK     <-- back OUT again
    1800   9.89      19  OK
    1940  10.66      11  FAIL   <-- SECOND basin
* **Gate A can fail. CONFIRMED**, with a known-bad input.
* **The non-monotonicity is real and independently reproduced**: 91 mm at 0 deg
  (pass 7 said 98), 0 mm at 6.9 deg (said 0 at 7), back out to 19-24 mm at
  9.3-9.9 deg (said 18 at 10). Pass 7's diagnosis is sound.
* **NEW: there is a SECOND collision basin past 10.66 deg that pass 7 did not
  report.** Pass 6's shipped cap was 1820 a16 = 10.0 deg, which reads 19 mm OK —
  sitting in the narrow safe window BETWEEN two collisions. Pass 6 was not merely
  unlucky; its cap was on a knife-edge.
* **Step fineness**: the shipped sweep is 45 a16 = 0.247 deg. Over 0..900 the
  profile is smooth (~7.7 mm per 100 a16, ~3.5 mm per step) with no narrow
  feature. The step IS fine enough for the range it covers. CONFIRMED.
* **Margin is thin**: failure begins 140 a16 (0.77 deg) above the shipped cap —
  15.6% headroom in roll. Rank for pass 8.

## THE 22 CRCs — ALL REPRODUCE EXACTLY (claim: calibration)
One binary invocation, `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`,
22 subjects named explicitly (gotcha §8), **RENDER_RC=0** read from the exit code.
`sequence_crc32c`, frame count AND unique-colour count compared for all 22:
**22 of 22 MATCH on all three fields. Zero mismatches.**
Including the mana menu: aqua 0x8B070C49, blue 0x719A34C1, boil 0xF114D862,
cyan 0x7F9219C1, green 0x97FC84CD, stack 0xE638C8C8.
`manafold-hover` and `manafold-inspect` both 0xF8468CC4, and byte-compared
frame by frame: **0 of 600 frames differ.** The Inspect caption's byte-identity
claim holds, verified from my own build.

## THE FOURTH BUG — FOUND, AND PASS 7 SHIPPED IT ITSELF
`tools/reel/manafold_clips.h:860-862, 890`
    static const Key kBrow[] = {{0,0},{8,-250},{16,-900},{38,-900},{44,150},
                                {50,150},{54,-1000},{66,-900},{78,0},{89,0}};
                                                     // <-- TEN keys
    apply_eye_roll(g, curve(kBrow, 9, f), curve(kBrow, 9, f));  // NINE passed
The sibling `kSide[]` has the identical 10-key beat structure and is correctly
called `curve(kSide, 10, f)` 12 lines above; the comment even says the brow is
"keyed against the SAME beats as kSide".
`git log -S` says BOTH the array and the call arrived in **783b2700 — "Manafold
p7: the eyes"**. So **pass 7 shipped a NEW instance of the exact off-by-one class
it committed `boundscan.py` to prevent, in the same pass, in the same
subsystem** — and boundscan.py structurally cannot see it, because the bound is
a FUNCTION ARGUMENT, not a `<`/`<=` literal, and `kBrow[` never appears on that
line.
Currently LATENT: k[8].v == k[9].v == 0, so simulating curve() with n=9 vs n=10
over all 90 frames of the curious clip gives 0 differing frames. Change that
last brow key to anything non-zero and 11 of 90 frames go silently wrong.
This bug class has now shipped FOUR passes running.

## Claim 3 (rule 3) — the count is real; "INTO SKY" is NOT established
* The 1499 reproduces exactly, and E1b shows it is NOT an artefact of the
  declared side-on approximation (only 16.5% is side-on).
* **But I looked, and the star is not drawn against sky.** pirouette f6/f14/f20
  at 9x and f14 at 12x: the far eye's lens sits AT the silhouette and the cel
  ink outline WRAPS AROUND the protruding eye. The star sits inside that ink.
  The creature's drawn silhouette is the UNION OF ALL ITS PARTS, not the body
  ellipsoid, and the ink follows the union.
* So rule 3's premise — "a star vertex outside the BODY outline is drawn against
  sky" — omits the renderer's own silhouette rule. **This is a FIFTH defect in
  the same measurement, of the same family as the other four: a mismatch between
  the frame that is measured and the thing that is drawn.**
* Pass 7's own publish log describes the pass-6 `channel` "long white spike off
  the far eye into the sky" as the visible manifestation, and says it is gone. I
  agree it is gone. What remains at 1499 is the star outside the body ELLIPSOID,
  which is not the same fault.
* **I tried to make this quantitative in pixel space and FAILED TWICE**, which is
  itself the finding. v1 flood-filled from a sky predicate that required g>=b,
  but this sky is mauve (156,93,107) so nothing seeded: 0% coverage, and it
  returned a confident "0 star px on sky" — DEAD AND PASSING, caught only by
  planting a known-bad blob. v2 fixed the seed but leaked through the ink's
  anti-aliased edge (60<max<100) and its star mask caught the MANA MOTES, so it
  returned "100% on sky". CLAUDE.md is explicit that measuring a silhouette off
  a rendered frame is unsound; the probe's own comment says rule 3 is done in 3D
  precisely to stay clear of that trap. I reproduced the trap twice in ten
  minutes. **Verdict: CONFIRMED by looking, UNVERIFIED by measurement, and the
  right instrument is a 3D one that models the silhouette as the union of parts.**

## Claim 5 (the black notches) — ADJUDICATED
* **The instrument fault is real and BIGGER than pass 7 said.** A lum<90 "black"
  threshold flags every authored lens colour: EYE_PURPLE (104,42,168) Rec601
  lum 74.9; EYE_PURPLE_DEEP (76,26,128) lum 52.6; the kLens fallback
  (116,58,178) lum 89.0. All three BELOW 90.
  Measured on my own render of hover f160: lum<90 covers **19.8% of the whole
  frame and 47.5% of the eye crop**, while lum<15 is **1 pixel**. A threshold
  that calls half the eye black is not a black detector.
* **An EIGHTH false comment.** `tools/pack/mkmanafoldpage.py:178` cites "the
  authored deep purple (58,28,156) has luminance 51". That triple appears
  NOWHERE else in the tree and is not any authored constant. The conclusion is
  right (the real EYE_PURPLE_DEEP is lum 52.6) but the cited evidence is
  invented. Same class as claim 10.
* **The degeneracy was real and is now structurally guarded**: kEyeLensTipPm=45
  and kStarTipPm=25 replace the zero end-rings, and `static_assert(kStarTipPm <
  50)` holds the star tip. Good work.
* **Looked at it**: hover f160 eye crop at 8x — no black wedges at the lens
  tips. The ink reads as a contour, which is what it is. CONFIRMED fixed.

## Claim: mana saturation "channel 8.3% -> 7.9%" — NOT REPRODUCIBLE
The committed `tools/reel/mana_hue_probe.py`, on MY render of the shipped
`manafold-channel`:
    bright=140 neutral=18 (the defaults) : hue-neutral 0.7%, mean sat 90.8
Swept: b140/n30 1.4% · b180/n18 1.0% · b180/n30 1.8% · b200/n18 1.1% ·
       b200/n30 2.0% · b220/n18 1.2% · b220/n30 2.1%
Nothing approaches 7.9%. `bright` on the fogprobe-mana render: 0.6%.
The tool's other mode, `diffpair` on the committed fogprobe pair (rendered here,
FOG_RC=0, 400 frame pairs): **10.3% hue-neutral, mean saturation 88.9**.
So neither committed mode on the shipped renders reproduces either half of
"8.3% -> 7.9%". **The single number quantifying the #1-RANKED fault of the whole
creature is not reproducible from the committed instrument.** I cannot say the
partial win is smaller than declared; I can say it is UNEVIDENCED.

## §2a, by looking at the diagnostic nobody rendered
`manafold-antenna-fixed` is NOT among the 22 published subjects, so the pass-7
publish never rendered the very diagnostic §2a demanded. I rendered it (FOG_RC=0,
420 frames). Six frames at 2x with camera and root held still: the loop plainly
reshapes — open reach, closed loop, tight hook, droop. **The hinges move, and
they move separately. §2a's substance CONFIRMED by looking**, on the instrument
built for it. Also visible and unchanged: the antenna is one continuous surface,
no countable spheres (pass 6's win survives), and the near eye's star is a bar
while the far eye's is a clean 4-point star (the declared limitation).
Minor: at cam_k 620000 the loop is CROPPED by the top of frame on early frames.
