# Manafold pass 26 — implementation

**Run:** `RUN-20260925-1535-manafold-pass26-hasty`
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-27-2026-09-25.md`
**Scope:** `manafold-hasty` and nothing else. Crackle's rear, Hover's front spin
and "the loop hitch is a fault" were all closed by the owner.

> *"It is, however, not very hasty. Make it look like it's actually in a hurry,
> both in facial expression and speed."*

---

## 1. Why it did not read hasty

Established by rendering the pass-25 clip and looking at all 240 frames *before
a value was touched* (`P26-NOTES/FINDINGS-01-why-not-hasty.md`). Four causes, and
three of them are the same leftover.

### 1.1 The creature did not move relative to anything

`build_hasty` ended with `c.root[f*3+0] = 0`. Direction 12 deleted the net
traverse when it deleted the frame-history smear — the smear was the only thing
the traverse existed to feed — **and the camera's traverse compensation was
never removed with it.** Slot 8 still panned `cam_bias_x` +28000 → −28000, which
`cam_pitch` folds in as NDC `x = k·X/w + bias_x`: a constant offset at every
depth, so it slides the creature and the ground *together*.

The owner saw motion and there **was** motion. It was a camera pan over a
stationary creature, and a pan is not speed — speed is read from relative
motion. Measured (`tools/reel/screenmotion.py`, chroma mask, lag 16):

| | px/frame |
|---|---|
| creature | −0.684 |
| background | −0.750 |
| **relative (the speed cue)** | **+0.066** (\|mean\| 0.147) |

Two independently measured operands that agree: the pure-pan signature, and
exactly what the tool's selftest leg A reproduces synthetically.

### 1.2 The pose was a posture, not an effort

`kHastyPitchA16` (2400) and `kHastyBankA16` (1900) are constant offsets applied
identically on every key. The creature had been leaning for the whole clip,
which reads as an attitude rather than as pushing against anything.

### 1.3 The cadence was leisurely

`kHastyBobCycles = 5` over 120 keys is **48 rendered frames per bob** — 0.8 s at
60 Hz. That is a float, not a stride.

### 1.4 The face could not act

Slot 8 inherited `kU02CamKTraverse` (148000) against the house 360000 — a camera
pulled back for an 8.4 m journey the clip no longer took. The creature's contour
ink was **1–2 pixels per frame**. Direction 27 asks for facial expression; there
was no face to author on.

**1.1, 1.3 and 1.4 are one fault**: the staging still compensated for a traverse
that had been deleted, and nobody took the compensation out.

---

## 2. What changed

Five authored channels plus the staging they need, every value a named constant
with a same-binary control. `HastyHurry::kOff` takes pass 25's arithmetic on
every one of them.

### 2.1 Cadence — `kHastyHurryBobCycles` 5 → **13**, `kHastyHurryBobAmpMm` 210 → **140**

13 cycles is ~18 rendered frames per pulse. **Ladder 9 / 13 / 17, judged on a
strip of 24 consecutive frames:** 9 is one long smooth arc — an easy lope;
17 agitates the antenna frame-to-frame and the read tips from *hurried* into
*panicked*; 13 drives. The frantic line lives between 13 and 17.

The amplitude is 140 and **the bound chose it** — see §4.1. Re-looked at 140
against 165: indistinguishable in the read.

### 2.2 The surge — `kHastyHurrySurgeA16` = **1400**

The pitch stops being a held posture and gains a sinusoid on the cadence clock,
phase-led by `0x6000` so the body digs in a beat *before* it rises — anticipation
rather than report. **Ladder 0 / 1400 / 2600:** at 0 the body holds one lean for
the whole strip while only the antenna moves (this is §1.2 isolated); 2600 rolls
and lurches; 1400 drives.

### 2.3 The traverse, restored — `kHastyHurryTraversePm` = 1000 (the full 8.4 m)

…**and travelling along the screen-lateral world axis, not +X.** See §4.2; this
was wrong in every previous version of this traverse.

### 2.4 The staging — `kU02CamKHasty` = **280000**, `kHastyHurryCamFollowPm` = **820**

The camera is Hasty's own constant at last. Bigger is better for the face *and*
for the speed cue (relative motion in pixels scales with `cam_k`), so the binding
constraint is edge clearance: 280000 at follow 820 gives **45 px left / 93 px
right margin, 0 frames touching an edge**, with 113.7 px of creature travel
across the frame. Full follow pins the body dead centre and throws away the screen
motion the owner liked; 650 leaves 15 px of margin, one bob from clipping.

### 2.5 The face — Hasty's own read

| constant | value | what it does |
|---|---|---|
| `kHastyEyeDrivePm` | 880 | the held, narrowed driving eye |
| `kHastyEyeCheckPm` | 1210 | snaps wide on a check |
| `kHastySquintDrivePm` | 430 | lid narrowed into the wind (pass 25 held 220 flat) |
| `kHastyBrowPm` | −620 | tops drawn together — **effort, not alarm** |
| `kHastyGazeCheckPm` | 900 | how far a check throws the gaze |
| `kHastyChecks[3]` | keys 18–27, 57–66, 88–95 | three short, unevenly spaced |

Pass 25 had **one** 16-key glance in a 120-key clip, which is a creature looking
around rather than one hurrying, and it used **two** of the five expressive
channels, neither hard.

**It is in the family of Startle and Curious without copying either.** Startle's
payoff is the eyes flying wide with the brow tops *apart*; Curious's is an
*asymmetric* double-take with the tops *together*. Both are **events**. Hurry is
a **state** — committed forward gaze, eyes narrowed with effort, brow tops
together (Curious's sign, deliberately not Startle's) — with three events cut
into it where the size channel does the acting. Every face channel reads one
`check` curve, so the size, gaze, lid and brow cannot drift out of agreement.

Ladder 800/520, **880/430**, 960/300: at 800 the drive eye nearly vanishes and
risks reading as *eyes shut*; at 960 drive and check are too close and the acting
flattens.

**A check that cut instead of leaving — caught by reading the code against its
own comment.** The first curve was `tri = (t <= half) ? t : span - t`, under a
comment reading *"a triangular in/out so a check ARRIVES and LEAVES rather than
cutting — at 2 rendered frames per key a cut is a pop"*. Printed, the 9-key
window runs `0, 250, 500, 750, 1000, 1000, 750, 500, 250` and then **cuts to 0**
on the next key: a 25% step in all four face channels at once (~5.7° of gaze in
one key), because they all ride this one curve. Replaced with a symmetric
triangle in permille of the window, which is exactly 0 at both ends. **The
windows are now all odd-span** (the middle one moved 57–65 → 57–66) so the
midpoint is actually sampled — on an even span the peak falls between two keys
and the check reaches only ~86% of the amplitude its constant names, which
would make `kHastyEyeCheckPm` a number that does not mean what it says.

A comment asserting a property the code does not have is worse than no comment:
it would have been read as evidence the arrival had been handled.

**Declared, not smuggled:** Hasty never called `enable_eye_scale_track()`, and
`Rig::write` only applies the pass-24/25 **ambient eye size** to a clip that has
a scale track — so that half of the shipped ambient layer could not reach this
clip at all. Authoring size turns the track on, and the ambient size therefore
starts landing here **at its shipped gain**. That is the pass-24 layer arriving
where it was always meant to, not a change to it.

### 2.6 One engine field: `SceneSubject::cam_bias_x_wrap_keys`

Defaults 0 = the existing frame-index lerp, so every other subject is
byte-identical. See §4.3.

---

## 3. Ladder rungs and chosen values

Full rung-by-rung notes with what each one *looked* like:
`P26-NOTES/FINDINGS-02-ladders.md`.

| channel | rungs run | chosen | the visual reason |
|---|---|---|---|
| bob cycles | 9 / 13 / 17 | **13** | 9 ambles, 17 is frantic |
| bob amp | 165 / 150 / 140 / 130 | **140** | the bound refused 165; 140 reads the same |
| surge | 0 / 1400 / 2600 | **1400** | 0 is a posture, 2600 lurches |
| cam_k | 148000 … 320000 | **280000** | tightest rung the antenna clears at both ends |
| cam follow | 650 / 750 / 800 / 820 / 900 / 940 / 1000 | **820** | keeps both the screen travel and the ground scroll |
| eye drive / squint | 800/520, 880/430, 960/300 | **880/430** | drive eye present, check clearly snaps wide |

---

## 4. Four things that were wrong, and how they were found

Every one was found by **rendering and looking**, and every one produced a
picture that looked like a different problem.

### 4.1 mqa's root-continuity ceiling refused the bob — and was right

A bob's peak per-key step is `amp · 2π·cycles / keys`, so tripling the cadence
triples what a given amplitude costs. 165 mm at 13 cycles gave a **143.3 mm**
root step against the **135 mm** default ceiling.

**No bound was relaxed.** Five clips declare their own ceiling and slot 8 could
have joined them; declaring a ceiling to fit a value *is* relaxing a bound and
the direction did not ask for it. Laddered against the gate instead:
165 → 143.3, 150 → 134.7, 140 → 129.0, 130 → 123.5.

**150 passes by 0.3 mm and was rejected for it.** A value that clears a bound by
a rounding error is not a passing value; it is a leg waiting to go red on an
unrelated change. 140 keeps 6 mm.

### 4.2 The traverse was running 45° into the lens

The u02 creature camera is a **fixed three-quarter** (`kU02FixedCamYawA16` =
0x2000), applied as `vp · rot_world_yaw(θ)`, whose rows give
`X_rot = c·X − s·Z`, `Z_rot = s·X + c·Z`. **World +X is therefore half lateral
and half depth.** Travelling along it, the creature visibly *grew and sank* as it
went — it was running diagonally into the camera — and no lateral follow could
ever hold it, because the bias corrects the horizontal half while the depth half
changes the creature's **scale** and its height in frame.

R5's standing comment in `zhao_reel.cpp` claims a linear lateral lerp "tracks
them exactly". **That is true only for travel parallel to the screen**, and this
camera is at 45° to it. Travelling `L` along `(cos, 0, −sin)` makes `Z_rot`
constant (`s·c + c·(−s) = 0`) and `X_rot` exactly `L`. Size and height then hold
flat across the clip, visibly. Both components are derived from
`kU02FixedCamYawA16` so the traverse cannot fall out of agreement with the
camera it was authored against.

### 4.3 The follow was clocked by the frame index, not the animation phase

The reel advances *then* renders, so rendered frame `i` shows key `(i+1)/2` —
and the last frame shows **key 0**, the start of the next lap. The creature's
root has already wrapped to the beginning of its journey while a frame-index lerp
has the aim at the far end of the sweep. They are a **whole traverse apart** and
the subject is simply **gone** for the seam frames.

Pass 25 survived this only because its root was pinned at centre, so the
mismatch was the aim's own 28000 rather than a journey. Fixed by clocking the aim
by `((f+1)/2) % keys` — the same phase the animation runs on. Two sides of a
follow, now clocked alike.

### 4.4 `kU02HastyBiasX` was never a calibrated follow, and the obvious derivation is wrong

That constant's own comment says it was picked off a ladder *"at f235, the frame
where it used to be gone"* — it is the sweep that was **enough to keep the
creature in shot**, not a tracker, and it is short of full follow by ~2.7×.

And calibrating it from the projection is seductive and wrong. `cam_pitch` adds
`bias_x` straight into NDC, so "65536 units = 1 NDC = 192 px of a 384 px frame"
gives 89,170 for full follow — **rendered, that left the creature drifting
164 px where the arithmetic promised 40.** The viewport's horizontal NDC is
~2.49 wide, not 2.0; a unit of bias buys ~154 px, not 192. A factor of 1.25
hiding in a step nobody measured.

`kHastyFullFollowBiasX = 81753` is solved from **two measured rungs** — follow 0
(+2.177 px/frame) and follow 800 (+0.771) — and **verified with a third** at
follow 1000, which came back **−0.167 px/frame** against +2.177 unfollowed. Two
rungs give a line; the third says whether the line was real.

It is a **measured bias removal**, not a chosen value: it fixes the tracker to
the traverse. What the shot looks like is then chosen by eye with
`kHastyHurryCamFollowPm`, which is the knob with an opinion in it.

---

## 5. Instruments

### 5.1 `tools/reel/screenmotion.py` — new, committed

Answers the question no single-authority number can: *is there ground-relative
motion, or is the picture just being panned?* It differences the creature's
centroid against a cross-correlated terrain band.

**Six selftest legs, all green** (`P26-RECEIPTS/screenmotion-selftest.txt`),
including **leg A, the pan-only leg that is the entire reason the tool exists**:
creature and textured ground moved together → `relative_dx` +0.00.

Three faults were found in the instrument or its fixtures, each by insisting it
be able to fail:

* **The first pan-only fixture was wrong, not the tool.** It built the ground as
  `gx = 400 + i·step`, which slides the *sampling window* right and therefore
  moves the *content* left — so "pan only" was really pan-and-travel and the leg
  failed against a correct tool. A fixture is exactly as capable of being wrong
  as the instrument, and a failing leg is a question, not a verdict.
* **A detector whose quantum exceeds the effect returns a confident zero.** The
  integer band correlator reported `bg_dx` **exactly 0.000 on 239 consecutive
  frames** for a picture that visibly slides, because the pan is ~0.68 px/frame —
  sub-pixel. Fixed with `--lag` (differencing over N frames). Leg F is the
  positive control, on a *true* sub-pixel fixture (smooth profile, linear
  resampling) rather than an integer one wearing the label. **It asserts the
  correct behaviour at lag 8 and deliberately does not assert that lag 1 returns
  0** — "the broken lag is still broken" is a test that asserts the bug.
* **An empty creature mask corrupts the background estimate.** With the ink mask
  vacuous on 155/240 frames the correlator stopped excluding the creature's
  columns and locked onto the only moving thing in the band — reporting
  +1.096 px/frame of "ground" on a render with **no camera pan at all**.

### 5.2 The chroma mask, and the known-negative that saved it

`--mask chroma` was added for the pulled-back framing. Its first threshold
(saturation ≥ 60) scored a frame **with no creature in it at 29,719 px — 32% of
the frame** — because the dusty-rose sky is itself saturated. That is the exact
CLAUDE.md failure ("a colour rule scored a known-ABSENT frame at 76% of a
known-present one"), reproduced within a minute of the check existing.

`python screenmotion.py absent <frames…>` is that check, committed. Measured
ceiling: sky and terrain top out at saturation 82, the creature reaches 222.
Threshold 100 sits above the background with margin and still finds ~2,200 px of
creature. Re-validated: **0, 0 and 7 px on three known-empty frames.**

**No number from the chroma mask was quoted before it passed that check.**

### 5.3 What this instrument cannot do here, stated

**Do not quote the AFTER row's `bg_dx` or `relative_dx`.** It reports +0.133 for
a background that must be scrolling *left* while the creature goes right — the
wrong sign. The cause is in the subject: this staging's terrain is a
near-featureless brown gradient whose only strong feature is a **screen-anchored
ordered dither**, and an integer band correlator cannot track a smooth ramp
carrying a pattern that does not move.

The ground-relative rate does not need the instrument, because it is **authored**:
the root traverses 8.4 m along the screen-lateral axis. Measured directly at
follow 0 — the configuration where the ground *is* static and `bg_dx` read
**+0.001 px/frame**, the positive control that the band correlator was reading
the ground and not the creature — the creature crosses at **+2.177 px/frame at
cam_k 200000**, hence **+3.05 px/frame at the shipping cam_k 280000**.

Pass 25's was **zero**.

### 5.4 `tools/reel/framediffcount.py` — new, committed

One integer: how many same-index frames differ. It is a file rather than a
heredoc inside the gate matrix because a python heredoc nested in the matrix's
own heredoc is how a generated script acquires a quoting bug that surfaces as a
leg silently reporting an empty string — which `[ "" -gt 0 ]` then turns into a
FAIL that looks like a *dead control*. It exits 0 either way; the caller decides
what the count means.

---

## 6. The loop seam

Direction 27 accepted the hitch and asked only that a new figure be reported.

Measured with the **pass-25 method, copied exactly** so the two numbers are
comparable — pink-ink mask `(r>150, r−g>60, b−g>20)`, horizontal centroid, frame
0 vs the last frame (`P26-RECEIPTS/loop-seam.txt`). The method reproduces the
pass-25 receipt's 38.8 px exactly, which is what licenses the comparison.

| | pass 25 | pass 26 |
|---|---|---|
| centroid f0 | 211.0 px | 158.8 px |
| centroid f239 | 172.2 px | 158.8 px |
| **seam displacement** | **38.8 px** | **0.0 px** |
| largest interior step | 1.47 px | 3.62 px |
| mask empty on | 0 frames | 0 frames |

> ### ⚠ CORRECTED BY THE PASS-26 REVIEW. READ `P26-REVIEW.md` §4.
>
> **What this section originally said — "the seam effectively vanished" and
> "the creature holds; what snaps instead is the terrain" — is not true**, and
> the review corrected it before it reached the owner's page. The arithmetic
> above is right; the *instrument* cannot see the thing it was being quoted for.
>
> **Two faults, both of them CLAUDE.md's standing ones:**
>
> 1. **The pink-ink mask fails its known-negative.** Paint the creature's bbox
>    out with a sky pixel and re-score: on a pass-25 frame it still returns
>    **4,073 of 4,977 px — 82% of it is sky and terrain**; on the pass-26
>    framing it returns *more* with the creature gone (17,421 px) than present
>    (5,695 px). It is a background centroid with a creature-shaped ripple in
>    it. I ran that check on the NEW chroma mask (§5.2), found it broken and
>    fixed it — and then inherited this one unchecked, because it was "the
>    pass-25 method copied exactly". **Copying a method exactly copies its
>    defects exactly.**
> 2. **`f0` vs `f_last` samples exactly two frames**, and clocking the aim by
>    the animation's phase brings precisely those two into agreement. The
>    discontinuity did not go away — it moved to **f238→f239**, one frame
>    earlier, where this metric never looks. A gate that cannot reach the state
>    is not evidence about the state.
>
> **What actually happens**, on `tools/reel/seamdisp.py` (new, committed, four
> selftest legs green, mask scores 0 px with the creature removed), walking
> *every* adjacent pair and separating creature from terrain:
>
> | | jump | where | × the clip's own median motion | terrain |
> |---|---|---|---|---|
> | pass 25 | **+163.4 px** | **f239→f0 (loop point)** | **237.5×** | +0.05 px |
> | pass 26 | **−110.7 px** | **f238→f239** | **35.4×** | +3.47 px |
>
> Pass 26's loop point is now clean (+3.24 px against a 3.12 px clip median —
> an ordinary frame).
>
> **The honest finding, which is a better story than the one it replaces:** the
> hitch did not vanish, it **moved** off the loop point and got **substantially
> smaller — 163 px → 111 px, and from 238× the clip's own motion down to 35×, a
> 6.7-fold improvement in how much it stands out.** In pass 25 the clip barely
> moved, so the jump was a teleport against a static picture; now the creature
> is visibly travelling and the same event reads as a stride hitch — which is
> exactly what the owner described and accepted.
>
> **Which is more visible: the CREATURE, decisively.** ~111 px is 29% of frame
> width and carries a pose change. The terrain's horizon shift at the wrap is
> a few pixels (3.47 px against ~0.5 px interior) and on this featureless
> gradient it really is near-invisible — that half of the original claim was
> right. The trade as described did not happen; a plain improvement did, and it
> never needed the framing.

**The measured figures above are correct arithmetic and the wrong conclusion.**
The pink-ink centroid lands on 158.8 px at both ends because the aim now wraps
in phase with the animation (§4.3) — which is a real and valuable repair, just
not the one the number was read as proving.

**The terrain does also snap.** `mqa` Q3 reports slot 8's root wrap at
**8330.6 mm** — the authored traverse returning — alongside drift's existing
6854.4 mm, and it prints it as `<-- SEAM POP`. Q3 reports the wrap column and
does not gate it, precisely because a travelling clip's seam is authored. That
ground shift is genuinely near-invisible here. It is simply not what the
creature is doing at the same instant.

---

## 7. Byte identity

`P26-RECEIPTS/byte-identity-bank.txt` — the **full 22-subject bank**, one
invocation per binary, rendered by the pass-25 binary (built from git
`f66d107c` in a detached worktree, md5 `2749bb7a48a35572f608ccb73a33cb25`) and
by the pass-26 binary (md5 `a6cc688b451e800150ce571cf51e4363`), both at
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.

> **22 subjects, 7,992 frames, 240 frames differing — all of them
> `manafold-hasty`. 21 of 22 subjects byte-identical. No unexpected changes.**

### The exact-off control

`P26-RECEIPTS/exact-off-identity.txt` — `ZHAO_U02_HASTY_HURRY=off` on the
pass-26 binary against `manafold-hasty` from the **pass-25 binary**:

> **240 frames, 0 differing. Sequence sha256 identical on both sides:**
> `fb8d7b60c192074047522da29bc1e4eff104b7b7ef59fcfba53a34e97b0d68bf`

Both legs are in the gate matrix (`e-p25off`, and `e-item-hasty` asserting the
hurry changes **exactly** hasty). **Both or neither:** a bank that matches pass
25 with the knob off proves nothing on its own about whether the knob does
anything — that is the lesson pass 25's own item 3 recorded, carried here.

The pass-24 and pass-23 identity floors gained `ZHAO_U02_HASTY_HURRY=off`,
because the hurry is on by default and those ladders reproduce banks that
predate it.

---

## 8. Gates

> ### **275 / 275, zero failures.**

`P26-RECEIPTS/gate-matrix.txt` — the pass-25 matrix **carried forward in full**
(every normal, mute, attributed control, mask leg and the whole per-item
identity ladder) plus this pass's legs. Script: `gatematrix_p26.sh`; the run is
launched from a frozen copy in `.tmp/` (see *Self-inflicted 2*).

**No gate default samples a subset of the bank:** every identity leg renders the
same `LIVE22` list of all 22 live subjects.

This pass's own legs, all green:

| leg | result |
|---|---|
| `e-p25off` | hurry=off ≡ the pass-25 **reviewer's** 22-subject bank |
| `e-item-hasty` | the hurry alone changes `[manafold-hasty]` and nothing else |
| `e-ship` | the shipping bank ≡ `P26-RECEIPTS/crcs-ship.txt` |
| `e-p24off` / `e-p23off` | the repaired historic floors, both exact |
| `e-backball-live` | the repair's positive control: changes `[hover, inspect]` |
| `h-fire-*` ×11 | every pass-26 control moves the clip's pixels |
| `h-rej-*` ×17 | every malformed or out-of-range value refused, RC 2 |
| `h-eyesize-off` / `h-eyesize-flat` | the registration check, both directions |
| `h-mqa-off` | root continuity green in the exact-off configuration too |
| `n-screenmotion-selftest` | the new instrument's six legs |

### Two gates went red and both were right

* **`n-meyesize`** — *"non-expression slot 8 unexpectedly allocates eye scale"*.
  The expression-slot list was a hardcode **the gate held alone** (`3 || 4 || 11
  || 21`) while the clips each called `enable_eye_scale_track()` in their own
  source: two copies of one fact, and this pass moved one of them. The list is
  now `u02::eye_expression_slot()` in `manafold_clips.h`, read by the gate and
  by the clips. Slot 8's answer depends on the hurry, because with it off the
  clip allocates no track and a gate expecting one unconditionally would go red
  on the one configuration whose purpose is to reproduce a shipped bank.
* **`n-mqa` Q3** — the root-continuity ceiling, §4.1. Fixed by moving the
  *value*, not the bound.

### And two identity floors were ALREADY broken, on subjects this pass never touched

`e-p24off` and `e-p23off` went red on **hover and inspect** while **crackle
matched exactly**. Full chase in `P26-NOTES/FINDINGS-04-inherited-matrix-fault.md`;
four steps, each ruling something out rather than arguing:

1. Does the hurry knob leak into hover? Hover plain vs `HASTY_HURRY=off`:
   **0 of 600 frames differ.** No.
2. Do the two binaries disagree under the floor? Pass-25 reference binary (git
   `f66d107c`, detached worktree) vs pass-26 binary, same floor:
   **0 of 600 frames differ.** Pass 26 changes nothing here.
3. Is the renderer non-deterministic for the orbiting subjects? Hover twice from
   one binary: `0x89EBA648` both times, **0 frames differ.** No.
4. So **the pass-25 binary itself misses the receipt** — hover `0xB7BE913A`
   against `0x8EDC6DE3`.

**Crackle matching is the tell.** `kBackBallDampClipPm` is **700 on slot 0 and 0
on slot 23**, and *"slot 0 is two subjects: hover AND inspect, one bake under
two names"*. The owner declined the slot-23 offer, so crackle carries no damping.

**The floors have no back-ball off-flag.** The pass-25 matrix receipt was
committed at `82218e2a`; the back-ball packet landed **after** it
(`af8c97c1` / `b3c5760e`, 1,134 insertions in `tools/reel/`), and **the full
matrix was never re-run**. The floors kept a PASS from a tree that no longer
existed, and it was carried through pass 25's close, its review and its
production verification.

Proof, on the pass-25 binary, floor + `ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0`:
hover `0x8EDC6DE3`, inspect `0xA7972F35` — **the receipt exactly, both**.

Repaired: `$BBOFF` is spliced into every ladder that reproduces a
pre-back-ball bank, exactly as `$HASTYOFF` is into every pre-pass-26 one, with
`e-backball-live` as its **positive control** — the same flag against the
shipping bank must change exactly hover and inspect, or "the floors pass now"
would be indistinguishable from the flag being dead.

**The rule both are instances of:** a floor must switch off *every* mechanism
added since the bank it claims to reproduce, and each new mechanism has to be
added to the floors on the pass that introduces it.

### The same staleness reaches the SHIPPING row, and pass 25's bank is still fine

`P25-RECEIPTS/crcs-ship.txt` was written by the pass-25 **matrix** (commit
`82218e2a`) and records hover `0x8124751D`, where the pass-25 tree actually
renders `0x89EBA648`. So the exact-off leg could not pass against it either.

**Pass 25's shipped bank is not in doubt, and this is the part to get right.**
Two *later* receipts certify it, they agree with each other, and an independent
rebuild here agrees with both:

| | hover | inspect | crackle | hasty |
|---|---|---|---|---|
| `P25-BB-RECEIPTS/crcs-backball.txt` | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |
| `P25-REVIEW-RECEIPTS/crcs-reviewer-ship.txt` | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |
| **this pass's rebuild of `f66d107c`** | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |

Four for four, on three independent paths. What is stale is one file — the
matrix's own `crcs-ship.txt` — not the creature. `e-p25off` therefore compares
against the **reviewer's** receipt, whose provenance is an independent build.

This also says something about the byte-identity evidence in §7: it is
**binary against binary**, never against a committed receipt, which is precisely
why it was unaffected by any of this.

### Self-inflicted 1: a positive control that could not read its baseline

`e-backball-live` first ran *before* `bank e-ship`, so it diffed against a file
that did not exist yet. `diff` wrote *"No such file or directory"* to stderr,
the leg captured stdout, and the empty result was reported as `changed=[]` —
**which is exactly what a dead flag looks like.** A positive control that cannot
read its own baseline is worse than no control: it fails in the shape of the
finding it exists to rule out. Moved after `bank e-ship`.

### Self-inflicted 2: editing a shell script while it was executing

The run that found this died with `syntax error near unexpected token ')'`
because **I edited the matrix script while it was executing.** `bash` parses a
script incrementally from a byte offset, so rewriting the file under a running
shell makes it resume in the wrong place. That is CLAUDE.md's live-tree trap
applied to a shell script, which is its own input. The re-run is launched from a
**frozen copy** in `.tmp/`, so the committed receipt can be edited freely while
it runs.

### Controls fired

`P26-RECEIPTS/controls-fired.txt` — **all 11 pass-26 controls move the clip's
pixels**, against the shipping render:

| control | frames differing |
|---|---|
| `ZHAO_U02_HASTY_HURRY=off` | 240/240 |
| `ZHAO_U02_HASTY_BOB_CYCLES=9` | 240/240 |
| `ZHAO_U02_HASTY_BOB_AMP_MM=260` | 238/240 |
| `ZHAO_U02_HASTY_SURGE_A16=0` | 240/240 |
| `ZHAO_U02_HASTY_TRAVERSE_PM=500` | 239/240 |
| `ZHAO_U02_HASTY_CAM_FOLLOW_PM=1000` | 238/240 |
| `ZHAO_U02_HASTY_CAM_K=200000` | 240/240 |
| `ZHAO_U02_HASTY_EYE_DRIVE_PM=1100` | 238/240 |
| `ZHAO_U02_HASTY_EYE_CHECK_PM=900` | **41**/240 |
| `ZHAO_U02_HASTY_SQUINT_PM=0` | 238/240 |
| `ZHAO_U02_HASTY_BROW_PM=800` | 238/240 |

`EYE_CHECK` firing on **41** frames rather than ~240 is a consistency check, not
a weak control: it acts only inside the three check windows (9+9+7 = 25 keys),
and the symmetric triangle's zero endpoints mean the first and last key of each
window carry no check at all — so 41 is what the authored curve predicts.

`P26-RECEIPTS/controls-strict.txt` — **17 malformed or out-of-range values, all
refused with RC 2**, never clamped. A knob that silently clamps reports a rung
it did not run.

### The registration leg, fired with legal stimulus

A registration check that has only ever been seen green is a claim. Both
directions are asserted in the matrix:

* `h-eyesize-off` — with the hurry off, slot 8 is *not* an expression slot and
  allocates no track. Still green: the leg expects the **other answer**, it does
  not stop looking.
* `h-eyesize-flat` — flatten hasty's authored eye size to identity **and** mute
  its ambient eye layer (`ZHAO_U02_EYE_AMBIENT_CLIP_PM=8:0`). The track is then
  allocated but all-identity — a registered clip that does not act — and the leg
  **fires**: `FAIL: expression slot 8 lacks an active eye-scale track`, RC 1.

No committed mutant was needed: this guard **is** reachable with legal input, so
it was reached.

---

## 9. Receipts

| file | what |
|---|---|
| `P26-RECEIPTS/gate-matrix.txt` | the full matrix |
| `P26-RECEIPTS/gatematrix_p26.sh` | the script that produced it |
| `P26-RECEIPTS/byte-identity-bank.txt` | 22 subjects, 7,992 frames |
| `P26-RECEIPTS/exact-off-identity.txt` | hurry=off ≡ the pass-25 binary |
| `P26-RECEIPTS/controls-fired.txt` | 11 controls, all fired |
| `P26-RECEIPTS/controls-strict.txt` | 17 bad values, all refused |
| `P26-RECEIPTS/loop-seam.txt` | the pass-25 method, **and why its mask cannot be quoted** (§6, corrected) |
| `P26-RECEIPTS/wrapseam.txt` | the presentation seam, before and after |
| `P26-RECEIPTS/screenmotion-before-after.txt` | the speed cue, with its bounds stated |
| `P26-RECEIPTS/screenmotion-selftest.txt` | six legs, including pan-only |
| `P26-RECEIPTS/mask-known-negative.txt` | both masks run on frames with NO creature |
| `P26-RECEIPTS/crcs-ship.txt` | the 22-subject shipping CRCs |
| `P26-RECEIPTS/binaries-md5.txt` | every binary, and the pass-25 reference |
| `P26-SHEETS/P26-{BEFORE,AFTER}-MANAFOLD_HASTY-ALLFRAMES.jpg` | every frame, both |
| `P26-LOOKS/` | the ladders, the face, the seam, the check leaving |
| `P26-NOTES/FINDINGS-0*.md` | findings, written after each look |

**Build:** `tools/reel/build-direct.sh` with the `zhao-env.ps1` toolchain
(g++ 16.1.0 MinGW-W64 ucrt). No CMake was used and no CMake result is claimed.
Exit codes read directly from the command, never through a pipe.

**Shipping renderer:** `zhao-reel-cel.exe` md5 `a6cc688b451e800150ce571cf51e4363`,
sha256 `1fd6b02e562b40f67c93162956575f4f3d7e4be5417d473941ab1d108d1ece36`.
**Hasty's shipping sequence CRC:** `0xD7FA75A4` (240 frames).

---

## 10. Open issues

1. **`kU02HastyBiasX` is now unused on the hurry path** but is still the
   exact-off value and is referenced by R5's comment block. Left in place
   deliberately: deleting it would break the exact-off path and erase the
   provenance of a constant whose history is the reason §4.4 exists.
2. **Drift (slot 1) has the same 45° traverse fault as §4.2** — it travels along
   +X against the same three-quarter camera, so it also grows and sinks as it
   goes. Out of scope this pass (only Hasty was reopened) and **not touched**;
   drift is byte-identical. Worth a pass of its own.
3. **R5's comment in `zhao_reel.cpp`** still claims a linear lateral lerp
   "tracks them exactly" for both travelling clips. It is now true for hasty by
   construction and remains false for drift. Annotated, not rewritten.
4. **`screenmotion.py`'s band correlator is unreliable on this staging** (§5.3).
   A terrain with any real feature would fix it; so would sub-pixel
   interpolation in `_band_shift`. Neither was needed here because the
   ground-relative rate is authored.
5. **The loop hitch MOVED and shrank; it did not vanish** (§6, corrected by the
   review). f239→f0 at 163 px becomes f238→f239 at 111 px — 238× the clip's own
   motion down to 35×. The terrain also snaps (8330.6 mm of root wrap), and that
   half really is near-invisible. Reported, per Direction 27; accepted, not a fault.
   **And the pass-25 pink-ink seam mask is background-dominated** — the 38.8 px
   figure in the pass-25 records is not a creature displacement. Use
   `tools/reel/seamdisp.py`.
6. **The historic floors were repaired late, not on the pass that broke them.**
   `$BBOFF` should have been added to the ladders by the back-ball packet
   itself. The general form of this is worth a standing habit rather than a
   note: **a pass that adds a mechanism must add its off-flag to every identity
   floor in the same pass**, or the floors quietly stop meaning anything and
   keep reporting green. Nothing here re-verifies pass 25's *shipped* bank —
   that was and remains correct; what was broken was only the ladder that claims
   to reproduce *older* banks.
7. **The pass-24 ambient eye SIZE layer still cannot reach 17 of 22 clips**,
   because `Rig::write` only applies it where a scale track exists and only five
   clips enable one. Hasty joining them is declared in §2.5. Whether the other
   clips *should* have it is a question for the owner, not a defect this pass
   should have silently fixed.
