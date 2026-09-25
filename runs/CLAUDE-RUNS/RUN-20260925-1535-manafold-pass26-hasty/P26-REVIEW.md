# Manafold pass 26 — independent review

**Run:** `RUN-20260925-1535-manafold-pass26-hasty`
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-27-2026-09-25.md`
**Reviewed at:** `e9364e64`
**Reviewer built everything from source and fired every control personally.**
Nothing below is quoted from the implementer's receipts except where a row is
explicitly labelled *reported*.

---

## VERDICT: **FIXED**

The pass does what Direction 27 asked. Hasty reads as being in a hurry, in both
halves the owner named — speed and face — and the diagnosis underneath it is
correct, reproducible and important. Every byte-identity and containment claim
survives an independent rebuild exactly. The stale-gate finding is real, is this
project's recurring fault caught in the act, and both of its halves check out.

**One claim did not survive review and I corrected it rather than publishing
it: the loop seam did not close.** §4. The code is right; the *sentence about
the code* was wrong, and it was wrong in the specific way CLAUDE.md warns about
hardest — a measurement that is arithmetically correct on an instrument that
cannot see the thing. The correction is to the claim and the receipt, not to a
constant. No render changed; the shipping CRCs are untouched by the fix.

---

## What I built and ran myself

| | |
|---|---|
| reviewer pass-26 binary | `build-direct.sh`, own output tree, g++ 16.1.0 MinGW-W64 ucrt |
| reviewer pass-25 reference | **my own detached worktree at `f66d107c`**, md5 `f02323737fafa51fd2945d582751c8a9` |
| banks rendered | 2 × 22 subjects × one invocation each, `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` |
| controls fired | 11 fire + 17 refusal + registration ×2 + back-ball positive control |

The reviewer binary's md5 differs from the implementer's (the build path is
embedded). **The render output does not:** all 22 sequence CRCs are identical to
`P26-RECEIPTS/crcs-ship.txt`, `diff` clean. Hasty `0xD7FA75A4`.

---

## 1. The diagnosis — VERIFIED, on both sides, independently

The whole pass rests on it, so I reproduced it from my own render of my own
pass-25 build, not from the receipt.

**Measured** (`tools/reel/screenmotion.py`, chroma mask, lag 16, `--skip-wrap`):

| | reported | **reviewer** |
|---|---|---|
| creature dx/frame | −0.684 | **−0.684** |
| background dx/frame | −0.750 | **−0.750** |
| **relative — the speed cue** | **+0.066** (\|mean\| 0.147) | **+0.066** (\|mean\| 0.147) |

Exact to three decimals. The creature and the ground slid together: over 238
frames the relative cue totals ~16 px, which is nothing.

**Structural**, read out of the pass-25 tree itself — and this is the better
half of the evidence, because it does not depend on any instrument:

* `build_hasty` ends `c.root[f*3+0] = 0`, under a comment saying Direction 12
  removed the traverse.
* `kU02HastyBiasX = 28000` survives, **with the comment `// hasty traverses
  8400 mm`** — a camera compensation whose own comment describes a journey the
  clip no longer takes — and slot 8 pans it `+28000 → −28000`.

A comment asserting the property that was deleted is exactly what let this sit
for fourteen passes. The implementer's reading is right in every particular.

I also confirmed the face half of §1.4: pass 25 gave Hasty **one** 16-key glance
(`f >= 56 && f < 72`) and a flat `apply_squint(220 + blink)`. Two channels,
neither hard.

**The instrument.** `screenmotion.py selftest` — six legs, RC 0, including
**leg A, the pan-only leg**, which is the one that makes the +0.066 meaningful:
creature and textured ground moved together → `relative_dx` +0.00.

---

## 2. Speed — VERIFIED. It reads as hurrying, and it is hurried, not frantic.

**At native, looking at every frame** (`P26-REVIEW-LOOKS/01`, 240 tiles): all
240 frames present, none broken, none empty, the subject never leaves frame.

**The before/after at native** (`02`, `03a`/`03b`) settles it. Pass 25's strip
of 18 consecutive frames is the diagnosis as a picture: the creature holds the
same place against the same ground, one lean, the antenna near-static. Pass 26's
same 18 frames carry a complete bob cycle, a visible body pitch, and the
creature crossing the ground.

**Hurried rather than frantic — yes.** Adjacent frames are continuous
everywhere; nothing snaps. ~18 rendered frames per pulse is a brisk scurry, and
the antenna arcs rather than chatters.

**Containment — VERIFIED exactly:**

| | reported | **reviewer** |
|---|---|---|
| left margin | 45 px | **45 px** |
| right margin | 93 px | **93 px** |
| frames touching an edge | 0 | **0** |

**Ladder honesty — VERIFIED, and the rungs are real.**

*Cadence*, by spectrum on the vertical centroid of a mask that passes its
known-negative:

| rung | dominant cycles/clip | vertical span |
|---|---|---|
| pass 25 | **5** | 18.0 px |
| 9 | **9** | 23.0 px |
| **13 (ships)** | **13** | 29.9 px |
| 17 | **17** | 28.5 px |

5 → 13 is exactly as claimed. By eye (`11`): 9 is one long smooth arc — it
ambles; 17 agitates the antenna frame-to-frame; 13 drives. I agree with the
choice; the frantic line does sit between 13 and 17.

*Surge* (`13`): at 0 the bulb holds one attitude for the whole strip and only
the antenna moves — the §1.2 posture, isolated and obvious. At 2600 the body
rolls and throws the neck; it lurches. 1400 drives. **All three rungs keep 13
cycles**, confirming the surge is a pitch channel on the cadence clock and not a
second rate. Honest ladder, defensible pick.

*The bound*, fired against `mqa` Q3 myself:

| bob amp | slot-8 worst root step (ceiling 135 mm) | gate |
|---|---|---|
| 130 | 123.5 mm | rc 0 |
| **140 (ships)** | **129.0 mm** | rc 0 |
| 150 | 134.7 mm | rc 0 |
| 165 | 143.3 mm | **rc 1** |

Every figure reproduced. **No bound was relaxed** — I diffed the ceiling table in
`manafold_qa_p12.cpp` against `f66d107c`: byte-identical, and slot 8 declares no
ceiling of its own, so it runs against the default. Rejecting 150 *because* it
clears by 0.3 mm is the right call and is recorded as such.

`mqa` is green at the shipping values (rc 0) and green at `HURRY=off` (rc 0).

---

## 3. Face — VERIFIED. Urgent, not alarmed, and it reads at viewing size.

**Against Startle and Curious** (`07`, the four states side by side):

* **Hasty drive** — short, sharply tapered wedges, small compressed pupils,
  tops angled *inward*, sitting low. Squinting forward under effort.
* **Hasty check** — the plates open to a clear lozenge with a full four-point
  pupil and the gaze throws sideways. Still symmetric, still tops-together.
* **Startle** — a broad, blunt, near-square plate with an enormous pupil and the
  tops **splayed apart**. Alarm.
* **Curious** — long slender plates, tops together, and plainly **asymmetric**
  between the two eyes. The double-take.

The implementer's claim is visually correct in every part: Hasty's baseline is
narrower than any state either neighbour reaches, it is symmetric where Curious
is not, and its tops converge where Startle's splay. **It is urgent without
becoming alarm**, and it is its own read rather than a copy of either.

**Does it read at the size the owner watches?** (`08`, matched frames, both
passes.) Pass 26: yes — the drive/check difference is legible as a change in the
face. Pass 25: no — the eyes are an indistinct smudge. This is the enabling
change, and it is why §1.4 belonged in the diagnosis rather than in a list of
nice-to-haves.

**The ambient layer is still underneath at its shipped gain — VERIFIED.**
`kEyeAmbientClipPm[]` diffed against `f66d107c`: **identical**, and slot 8 carries
`kEyeAmbientOrdinaryPm` (800). Nothing about the pass-24/25 layer moved. The
implementer's §2.5 is accurate and correctly declared: enabling a scale track is
what lets `Rig::write` apply the ambient **size** here for the first time, and
that is the existing layer arriving, not a new one.

---

## 4. THE SEAM — the claim does not survive. Corrected, not published.

Direction 27 **accepted** the hitch and asked only for a new number, so **none of
this is a blocker**. But "38.8 px → 0.0 px" and "the seam effectively vanished"
were about to go onto the owner's page, and they are not true.

### 4.1 I reproduced both quoted numbers exactly — the arithmetic is right

Pass-25 pink-ink method (`r>150`, `r−g>60`, `b−g>20`), centroid f0 vs f239:
pass 25 **38.8 px** (reproducing the accepted figure to the decimal), pass 26
**0.0 px**. `P26-RECEIPTS/loop-seam.txt` computes what it says it computes.

### 4.2 But the mask fails its known-negative, badly

Paint the creature's bbox out with a sky pixel and re-score:

| mask | full frame | **creature REMOVED** |
|---|---|---|
| pink-ink, pass 25 f120 | 4,977 px | **4,073 px — 82% is background** |
| pink-ink, pass 26 f120 | 5,695 px | **17,421 px — more than with it present** |
| saturation ≥ 100, pass 25 | 1,076 px | **0 px** |
| saturation ≥ 100, pass 26 | 4,385 px | **0 px** |

The pink-ink centroid is a **background** centroid with a creature-shaped
perturbation in it. This is precisely the failure the implementer *correctly*
caught and fixed on the new chroma mask (§5.2 of the implementation) — and then
inherited unchecked on the old one, because it was "the pass-25 method copied
exactly". **Copying a method exactly copies its defects exactly.** A metric does
not become trustworthy by being the one used last time.

### 4.3 And the two-frame metric can no longer reach the jump

`f0 vs f_last` samples exactly two frames. Clocking the aim by the animation's
phase brings those two into agreement — correctly, that is the repair — while
the discontinuity moves to **f238→f239**, one frame earlier, where the metric
never looks. A gate that cannot reach the state is not evidence about the state.

### 4.4 What actually happens, on an instrument that passes its known-negative

`tools/reel/seamdisp.py` — **new, committed**, four selftest legs green, walking
*every* adjacent pair including the wrap, and reporting creature and terrain
**separately** so "the creature holds and the ground snaps" is a checkable claim
rather than a rhetorical one. Leg B is its positive control and is exactly this
defect: a fixture where the two-frame metric reports 0.0 px while a 44 px jump
sits one frame earlier.

| | jump | where | × the clip's own median motion | terrain |
|---|---|---|---|---|
| pass 25 | **+163.4 px** | **f239→f0 (loop point)** | **237.5×** | +0.05 px |
| pass 26 | **−110.7 px** | **f238→f239** | **35.4×** | +3.47 px |

Pass 26's loop point itself is now clean: +3.24 px against a clip median of
3.12 px — *indistinguishable from an ordinary frame*. Confirmed by eye at native
(`09a`, `09b`, `10`).

### 4.5 The honest finding, which is a better story than the one it replaces

1. **The hitch did not vanish. It moved** off the loop point to one frame
   earlier. One discontinuity per lap, before and after.
2. **It got substantially smaller: 163 px → 111 px** — and, in the figure the
   eye actually judges by, **from 238× the clip's own motion down to 35×, a
   6.7-fold improvement.** In pass 25 the clip barely moved, so the jump was a
   teleport against a static picture. In pass 26 the creature is visibly
   travelling, so the same kind of event reads as a stride hitch. That is
   exactly what the owner described and accepted: *"it moves across screen, it
   makes sense it hitches."*
3. **Which is more visible — plainly: the creature.** ~111 px is 29% of frame
   width and carries a pose change with it. The terrain's horizon shift at the
   wrap is a few pixels (mean 3.47 px against ~0.5 px interior) and on this
   featureless gradient it is genuinely near-invisible. **The implementer is
   right about the terrain and wrong that "the creature holds".** The trade as
   described did not happen; what happened instead is a straightforward
   improvement, which did not need the framing.
4. `mqa` Q3 prints slot 8's wrap as **8330.6 mm `<-- SEAM POP`** (beside drift's
   existing 6854.4 mm). The gate's own word for it is *pop*. Reported and not
   gated, correctly, because a travelling clip's seam is authored — but it is
   not the word "vanished".

**Corrections made** (claims only — no constant, no render, no CRC moved):
`P26-IMPLEMENTATION.md` §6 and §9, and `P26-RECEIPTS/loop-seam.txt`, now carry
the measured figures above and the known-negative result. The site copy states
the improvement and does **not** say the seam closed.

---

## 5. Byte-identity and containment — VERIFIED exactly

Binary against binary, never against a committed receipt — which, as §7 of the
implementation notes, is why none of this was exposed to the stale-receipt
problem.

| | reported | **reviewer** |
|---|---|---|
| subjects | 22 | **22** |
| frames | 7,992 | **7,992** |
| frames differing | 240 | **240** |
| subjects differing | hasty only | **hasty only** |

Every other subject 0 differing, per-subject, by whole-file comparison.

**The owner's closed items were not touched:**

* **Crackle** — 600 frames, **0 differing**. Byte-identical. `kBackBallDampClipPm[23]`
  is still 0; the declined offer was not smuggled in.
* **Hover's front spin** — 600 frames, **0 differing**. Channel unchanged.
* **Inspect** — 600 frames, 0 differing.

**The exact-off control:** `ZHAO_U02_HASTY_HURRY=off` on the pass-26 binary
against hasty from **my own** pass-25 build — 240 frames, **0 differing**,
sequence sha256 `fb8d7b60c192074047522da29bc1e4eff104b7b7ef59fcfba53a34e97b0d68bf`
on both sides, matching the claimed value.

---

## 6. The stale-gate finding — VERIFIED, both halves. This is the important one.

### Half 1: pass 25's shipped bank is genuinely unaffected

Four independent paths now agree, the fourth being my own rebuild of `f66d107c`:

| source | hover | inspect | crackle | hasty |
|---|---|---|---|---|
| `P25-BB-RECEIPTS/crcs-backball.txt` | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |
| `P25-REVIEW-RECEIPTS/crcs-reviewer-ship.txt` | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |
| implementer's rebuild | `0x89EBA648` | `0x3A179F08` | `0x370F7F3E` | `0xDC044A02` |
| **reviewer's rebuild** | **`0x89EBA648`** | **`0x3A179F08`** | **`0x370F7F3E`** | **`0xDC044A02`** |
| `P25-RECEIPTS/crcs-ship.txt` (the matrix's) | `0x8124751D` | `0xEF7FBD6D` | ✔ | ✔ |

One file disagrees, on exactly the two subjects that share slot 0 and carry the
back-ball damping. **A stale file, not a stale creature.**

### Half 2: the repair and its positive control are real — I fired it myself

On my pass-25 binary, floor + `ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0`:

> hover `0x8124751D`, inspect `0xEF7FBD6D` — **the stale receipt's CRCs exactly,
> both subjects.**

That is conclusive. `P25-RECEIPTS/crcs-ship.txt` was written from a tree without
the back-ball damping; the packet landed after it (`af8c97c1`/`b3c5760e`) and the
matrix was never re-run. The floors carried a PASS from a tree that no longer
existed, through pass 25's close, **its review, and its production
verification** — including mine.

The repair is applied consistently: `$BBOFF` is spliced into every pre-back-ball
ladder (`e-p24off`, `e-p23off`, `e-item1/2/3`) and correctly **not** into
`e-p25off`, whose bank includes the damping. `e-backball-live` is its positive
control against the shipping bank and is ordered **after** `bank e-ship` so it
can read its own baseline — the self-inflicted fault the implementer found and
recorded rather than quietly fixing.

**This is the finding of the pass**, more valuable than the animation work: a red
gate that shipped unnoticed is this project's recurring fault, and the standing
rule it produced — *a pass that adds a mechanism must add its off-flag to every
identity floor in the same pass* — belongs in CLAUDE.md.

---

## 7. Controls — every one fired, each failing for its own reason

**11 fire controls**, rendered against the shipping hasty. Every count matches
the receipt exactly:

| control | reviewer | | control | reviewer |
|---|---|---|---|---|
| `HURRY=off` | 240/240 | | `CAM_K=200000` | 240/240 |
| `BOB_CYCLES=9` | 240/240 | | `EYE_DRIVE_PM=1100` | 238/240 |
| `BOB_AMP_MM=260` | 238/240 | | `EYE_CHECK_PM=900` | **41**/240 |
| `SURGE_A16=0` | 240/240 | | `SQUINT_PM=0` | 238/240 |
| `TRAVERSE_PM=500` | 239/240 | | `BROW_PM=800` | 238/240 |
| `CAM_FOLLOW_PM=1000` | 238/240 | | | |

`EYE_CHECK` at 41 is a consistency check, not a weak control: it acts only inside
three windows whose symmetric triangle is zero at both ends. It reproduces
exactly, which is the point.

**17 malformed / out-of-range values — all refused, RC 2, none clamped.** Fired
individually; zero exceptions.

**The registration leg, both directions:**

* `n-meyesize` shipping — rc 0, `PASS: 0 failure(s)`.
* `h-eyesize-off` — rc 0. The leg expects the *other* answer with the hurry off;
  it does not stop looking at slot 8.
* `h-eyesize-flat` — **rc 1**, `FAIL: expression slot 8 lacks an active
  eye-scale track`. It fires on legal stimulus, so no committed mutant is owed
  here.

**No gate default samples a subset of the bank.** There is exactly one `bank()`
function in the matrix and it always renders `$LIVE22`; no render call anywhere
uses a hand-picked list. The matrix reads 275 rows, 275 PASS, 0 FAIL, no
malformed rows.

---

## 8. Open issues carried forward

1. **The pink-ink seam mask is background-dominated** (§4.2) and the 38.8 px
   figure in the pass-25 records is not a creature displacement. The figure was
   accepted by the owner and nothing needs re-deciding, but no future pass should
   quote it as a body measurement. `tools/reel/seamdisp.py` is the replacement.
2. **The f238→f239 jump is real and is the clip's one hitch.** Accepted by
   Direction 27. If it is ever reopened, the lever is `kHastyHurryCamFollowPm`
   or a shorter traverse, not the metric.
3. The implementer's own list (§10) stands: `kU02HastyBiasX` deliberately
   retained for the exact-off path; **drift (slot 1) carries the same 45°
   traverse fault** and is untouched and byte-identical; `screenmotion.py`'s band
   correlator is unreliable on this staging and its AFTER-row `bg_dx` must not be
   quoted; the pass-24 ambient eye **size** still cannot reach 17 of 22 clips.
4. **`mqa` Q3's wrap column is printed and not gated.** Correct for authored
   seams, but it means no gate anywhere bounds a loop discontinuity. `seamdisp.py`
   could become one if the owner ever wants it.
