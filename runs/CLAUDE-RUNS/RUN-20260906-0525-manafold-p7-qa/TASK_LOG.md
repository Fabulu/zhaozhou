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

## Claim 8 (Zixxtrixx untouched) — PROVED BY CRC FROM A BASELINE I BUILT (gate item 21)
Pass 7 proved this "by inspection" (no zixx file in the diff). The checklist asks
for CRC from a baseline you built yourself, so I built one.
Second clone at `manafold-p7-qa/base-zhaozhou` @ **b4e9a311** — the merge
immediately BEFORE pass 7's first commit (1ab3f91e). `BASE_BUILD_RC=0`,
binary 2,735,868 B (pass 7's is 2,739,759 B, so they are genuinely different
builds). Same six subjects, same env, from each binary:

| subject | pass 7 | baseline b4e9a311 | |
|---|---|---|---|
| zixxtrixx-attack  | 0x86A2B30B | 0x86A2B30B | IDENTICAL |
| zixxtrixx-balance | 0x301EFBA6 | 0x301EFBA6 | IDENTICAL |
| zixxtrixx-bow     | 0xEC2CAD71 | 0xEC2CAD71 | IDENTICAL |
| zixxtrixx-corpse  | 0x4B449AB5 | 0x4B449AB5 | IDENTICAL |
| zixxtrixx-damage  | 0x6C224D56 | 0x6C224D56 | IDENTICAL |
| zixxtrixx-death   | 0x2E8755B3 | 0x2E8755B3 | IDENTICAL |

**CONFIRMED.** The shared `zhao_reel.cpp` edit (the smear clamp and the new
Manafold subject block) does not reach any Zixxtrixx path. This is the check
that catches "a stray edit that silently retimed five cameras", and it is clean.

## EVERY GATE FED A KNOWN-BAD INPUT
| gate | can it fail? | evidence |
|---|---|---|
| 5c rule 1 (overhang) | **NO — vacuous** | E3: 0 star verts -> 0 mm vs cap 29, OK |
| 5c rule 2 (on-purple) | **NO — vacuous** | E3: 0 verts -> 1000 pm vs floor 600, OK |
| 5c rule 3 (outline) | not enforced anyway | E3: 0 verts -> 0 violations |
| 5d gate A (eyes touch) | **YES** | E4: FAILs from 1040 a16; also vacuous at 0 verts, but 5d prints a census |
| 5d gate B (clipping) | YES, fails safe | no lens verts -> worst stays 1<<30, `< 1000` false -> FAIL |
| eye-protrusion | YES, fails safe | no verts -> max_e 0, `>= 1215` false -> FAIL |
| clearance contract | YES, fails safe | `T.mesh.empty()` returns 1 up front |
| hinge_trajplot | **YES** | committed selftest: flat 0.00 vs moving 80.00 mm; r 1.000 vs 0.000 |
| boundscan.py | YES for its one shape | fires on the p6 tree, silent on the fix — but 16 proven blind spots |
| checkmedia.py | (see media audit) | |
| my own skycross v1 | **NO — dead** | 0% coverage; caught by a planted blob |

**The 5c rules are the dead ones.** Three gates pass with zero vertices walked
and print no warning. The 5d block prints an instrument census; 5c does not.
Fix: assert a non-zero star-vertex count, and derive the star colour from
`u02::kStar*` rather than the literal `246,242,250`.

## MINOR / LATENT
* `manafold_probe.cpp` uses `T.bank.clips[7]` by POSITION with a comment saying
  "slot 7". It is correct today (manafold.h:138) but it is an index-by-position
  where every other read is by slot_id. One reorder and the protrusion gate
  silently measures a different clip.
* The 5c rules walk `bank.clips` including **slot 15, `lab::build_manalab()`,
  marked LANE-ONLY** — a clip that never ships is inside the shipping gate's
  population.
* `manafold-antenna-fixed` is committed but was NOT among the 22 rendered
  subjects, so §2a's own diagnostic was never rendered by the pass that built it.

## WHAT IS RIGHT (gate item 24 — the protected list)
Pass 7 is the most honest pass this creature has had. Protect these:
1. **The three stacked bugs are real, correctly diagnosed, and correctly fixed.**
   The units bug, the bind-space frame bug and the Y-only root subtraction are
   all genuine, and the peeling order described is the order they hide in. The
   final numbers reproduce EXACTLY from an independent build.
2. **All 22 CRCs reproduce bit-for-bit**, with frame counts and colour counts,
   from one binary whose md5 I recorded before the render and re-checked after
   every later compile. hover == inspect at 0 of 600 frames.
3. **Gate A's roll sweep was the right call and its diagnosis is sound.** I
   reproduced the non-monotonic profile independently and it is worse than
   reported (a second basin). The 21-step resolution is adequate for the range
   it covers. Do not go back to corner sampling.
4. **The star re-proportioning is real work, honestly derived**, and the sheet
   measurement is legitimately like-for-like (the star is a flat object,
   `kStarThinMm=16`, drawn as a flat graphic; PCA aspect is rotation-invariant).
   1.70/1.72 reproduces from the sheet independently; shipped is 1.695.
5. **Zixxtrixx is untouched, proven by CRC across the pass boundary.**
6. **The tip degeneracy fix is structural**, with a `static_assert` guarding it.
7. **The eyes read at native.** At 384x240 both eyes read as small stars where
   pass 6 had streaks. That is the pass's headline and it clears its own bar.
8. **The §2a instrument reads the posed skeleton, never pixels**, and its
   selftest genuinely can fail. The fixed-camera diagnostic really is fixed.
9. **The publish run's self-reporting is exemplary** — it recorded its own
   gotcha-§8 mistake, refused to inherit numbers, and captioned the cross-eyed
   taunt as wired-but-not-reading rather than as delivered.

## RANKED FOR PASS 8
1. **The 5c rules cannot fail.** Three gates pass with zero vertices walked, on a
   hard-coded literal colour, with no census. Highest leverage: it silently
   protects nothing, and the star's colour is exactly the sort of thing this
   creature keeps changing. (E3)
2. **`curve(kBrow, 9, f)` on a ten-key array** — manafold_clips.h:890. Latent
   today only because the last two key values are both 0. Fix the call AND give
   the file the `sizeof`-derived count convention `zixxtrixx.h` already uses; all
   36 `curve()` calls there pass hand-written literals.
3. **Rule 3 measures the wrong outline.** The creature's silhouette is the union
   of its parts and the cel ink follows it, so "outside the body ellipsoid" is
   not "against sky". Either model the silhouette as the union in 3D, or retire
   the rule — but do not tune the creature to satisfy 1499.
4. **Gate A's margin is 15.6%.** Failure begins 0.77 deg above the shipped cap.
5. **The mana number is unevidenced.** 8.3% -> 7.9% does not reproduce from the
   committed tool in either mode (0.7% bright / 10.3% diffpair). The #1-ranked
   fault of the creature has no reproducible measurement. Fix the tool or the
   claim before anyone tunes against it.
6. **Seven-plus false comments**, three inside the block pass 7 rewrote, one of
   which pass 6 already announced it had deleted; plus a cited colour
   (58,28,156) that exists nowhere in the tree. The pass-5 containment
   derivation now yields a NEGATIVE allowed gaze from current inputs.
7. **boundscan.py has 16 proven blind spots** and a permanent floor of 18
   suspects, so going non-empty signals nothing.
8. Rule 3's root-ROTATION omission (13 of 1499 — correctness, not urgency).
9. `clips[7]` by position; slot 15 (LANE-ONLY) inside the shipping gate.
10. `manafold-antenna-fixed` is never rendered by the publish set.

## VERDICT PER CLAIM
1. three stacked bugs / final numbers — **CONFIRMED**; a FOURTH exists
   (`kBrow`, shipped BY pass 7) and a FIFTH (rule 3's outline model)
2. gate A sweeps, can fail, step fine enough — **CONFIRMED**, + a second basin
3. rule 3 reported-not-enforced, fault real — **PARTLY REFUTED**: the count is
   real, "into sky" is not established and is contradicted by the frames
4. boundscan proved failable, 18 false positives — **CONFIRMED with caveats**;
   16 blind spots, 2 dissents, and a new real bug it cannot see
5. black notches: instrument vs real — **CONFIRMED**, instrument fault is larger
   than stated; plus an eighth false comment
6. star proportion 1.70 -> 1.695 — **CONFIRMED** independently from the sheet
7. §2a scoring — **CONFIRMED in substance**; the degrees are not emitted by any
   committed tool and the true spread is 12-46 deg, not 20-30
8. Zixxtrixx untouched — **CONFIRMED by CRC from a baseline I built**
9. media/site — see the media audit section
10. kFogThicknessPm the only false comment — **REFUTED**, at least seven more

## Claim 9 (media / site) — CONFIRMED, with one misleading proof and 7 blind spots
`Upheaval/website/tools/checkmedia.py`.
* **(a) 981 decoded — CONFIRMED**, and 981 reconciles THREE ways: manifest
  declares 981, index.html references 981, set difference 0 both directions.
  Independent full-bank sweep: all 486 declared webms decoded, 0 nonzero rc,
  0 stderr.
* **(b) "proved failable against a 2000-byte stub" — CONFIRMED BUT MISLEADING.**
  It fails, but **the SIZE FLOOR rejects it, not the decoder**: `2000 B is below
  the 8192 B floor`, `2 declared files, 1 decoded` — ffmpeg never touched the
  stub. The docstring justifies the tool by "ffprobe returns 0 on a truncated
  stub, so we decode every frame", and then proves it with a fixture that
  exercises `os.path.getsize`. The claimed capability is untested by the cited
  test.
* **(c) production byte-identical — CONFIRMED.** `curl | cmp` against the
  committed `public/index.html`: exit 0. And the committed page is itself the
  exact regeneration of `creatures.json` — re-running `assemble.py` differs on
  ONE line, the build timestamp. Not stale, not hand-edited.
* **(d) 808 Zixxtrixx decoded — CONFIRMED** (173 + 808 = 981, no overlap).
* **(e) noindex / ordering / generations — CONFIRMED**, and at 20 generations
  `assemble.py` HARD-ERRORS rather than silently dropping — proved on a doctored
  manifest.
* **Every frame count on disk matches the declared lengths exactly.** No dead
  media, no 0-frame file, no missing poster.

### checkmedia.py's proven blind spots (fixtures)
DETECTED: 0-byte; 2000-byte truncation; declared-but-absent; missing poster.
**SLIPS:** a 400 KB truncation of a 516 KB clip (loses 51 of 240 frames and
**ffmpeg still exits 0**); a valid webm with 10 frames instead of 240; a valid
240-frame ENTIRELY BLACK clip; the WRONG CLIP under the right name; a poster
mismatched to its webm; a 1x1 poster; a file on disk never referenced.
**Most actionable: `decode_webm` checks only `returncode` while ffmpeg prints
"File ended prematurely" to the stderr the function already captures and
DISCARDS.** One line — fail on non-empty stderr — closes it. This is the exact
shape of the historical dead-`inspect` fault.

### A second gate that cannot fail — `check_css_wiring`
Its regex matches BOTH the tab family and the archive-generation family, then
asserts only `max(...) >= MAX_TABS`, so the generation family topping out at 19
is invisible. Proved: with `MAX_ARCHIVE_GENERATIONS = 20` the check still
reports PASS and assemble builds a 20-generation page whose generation-20 label
and panel select nothing — a silent dead tab, the precise failure the guard
exists to prevent. (The separate assemble-time check does catch it, so the page
is safe today; the GUARD is the dead thing.)

### Noted, not faults
* hover and inspect webms have identical byte sizes (3,604,337) but different
  md5s — coincidence, flagged only because it mimics the historical dead-inspect
  fault.
* Archive generations Sixteen/Seventeen/Eighteen share 20 of 22 clips
  byte-identically: 19 generations structurally, 444 distinct contents of 486.
* 12 unreferenced orphan files on disk; posters are 1152x720 against 384x240
  clips (a deliberate 3x supersample).

### Media audit, addenda from the final report
* `checkmedia.py` **never opens `index.html`** — it is manifest-driven only, so
  nothing binds the page's references to the files it validates. The three-way
  981 reconciliation that makes the count trustworthy is MY check, not the
  tool's.
* It routes `.gif` through `decode_png`, which has no frame-count check.
* Poster outlier: `renders/zixxtrixx-v11-light-comparison.png` is 768x520 where
  every other poster is 1152x720.
* The 12 orphans, named: four `renders/SHEET-*.png`, `renders/u02-fx-tour.
  {webm,png}`, `renders/u02-trio.{webm,png}`, and
  `renders/native/zixxtrixx-{slither,strike}.png` alongside
  `renders/zixxtrixx-{slither,strike}.png`.
* The strict sweep worth keeping: over all 486 declared webms, "webms with
  nonzero rc OR any stderr: **0**" — so there is no silent truncation anywhere
  on the site today. That is the check `checkmedia.py` should be doing, and the
  one-line fix (fail on non-empty stderr) makes it do it.

## Gate item 28 — BACKGROUND WORK, AND THE TRAP FIRED AGAIN HERE
Checked with `Get-CimInstance Win32_Process`, not by assumption. Found ALIVE:
* `python tools/checkmedia.py .` (PID 16908), actively spawning ffmpeg children
  and still decoding `archive-2026-09-01-generation-eleven-attack.webm`, long
  after the sub-investigation that launched it had stopped returning tool output.
* An orphaned `ffprobe` (PID 23796) from the same sweep.
Killed the tree and re-verified: no `zhao-reel`, `manafold-probe`,
`manafold-qa-p7`, `ffmpeg`, `ffprobe`, `wrangler` or `zixx` process alive.

**The instructive part is the coupling.** That live child was ALSO why the media
agent's completion notification had not reached me: the notification fires only
when an agent stops with no live background children. So from my side the
investigation looked finished while its work was still running, and the evidence
that it was still running was the same process that was suppressing the notice.
Killing it delivered the notification.

CLAUDE.md: "stopping an agent does not stop its background work ... kill the
background tasks too, then verify nothing is running before assuming a lane is
closed." Reproduced inside the QA run that was auditing for it. **A tool call
returning is not a process exiting, and an agent going quiet is not an agent
being done.**

CORRECTION, entered after the media agent's final report arrived: an earlier
draft of this entry quoted that agent as having reported the run "was killed".
It reports no such thing — it records an earlier full-site run that exited 127
without executing (wrong cwd in a backgrounded shell) which it initially misread
as a timeout, and states that the re-run with absolute paths is the real result.
The finding above is what I OBSERVED directly; the quote was mine, not its, and
is withdrawn. Logged rather than silently edited, since this run's whole subject
is claims that outrun their evidence.

## Lane hygiene
`C:\programmieren\zencrifice\zhaozhou` and `...\Upheaval` were never touched:
both still clean at their own HEADs (2f98562a / 2ad25aa). No other
`manafold-*` lane was entered. NO creature constant was changed anywhere; the
only files I added are the two QA experiment probes and this run folder.
NOTHING WAS PUBLISHED.
