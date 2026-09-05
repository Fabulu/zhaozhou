# REVIEW — creature 02, pass 3

**Reviewer run against** `RUN-20260905-0555-u02-pass3-implementation` (shipped
and published at zhaozhou `eae651dc` / Upheaval `75cd96d`).
**Standard:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-3-2026-09-05.md`, its
eight acceptance items.
**Worked through:** `Upheaval/creature/10-GATE-CHECKLIST.md`, every item.
**Judged on:** the shipping subjects, native 384x240 first and close-ups after,
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, on a `build-direct.sh cel`
build this reviewer made in its own output directory. Frames read only with
`tools/reel/rgbframe.py`.
**Evidence:** `evidence/review/` — 21 plates plus five text files.

---

## The one-line verdict

**Real pass, real regression-fixing, one fault that must not ship again.** The
lighting regression is genuinely killed, the mist rung is right, the antennae
are right, the headstand is a delight, and Zixxtrixx is untouched — I proved
that myself, byte for byte. But **the eye still does the exact thing the owner
called "super weird and wrong": the star leaves the lens.** It is not a tuning
residual, it is arithmetic — the star's arm is 185 mm against a 125 mm lens
half-width, so it cannot fit. And the two clips that were given real travel
this pass now walk into the hillside, because the reel snaps the root to one
terrain column — the same fault that produced the "massive sink" Fabian
rejected on Zixxtrixx's walk a week ago.

---

## Verdict per acceptance item

| # | Item | Verdict |
|---|------|---------|
| 1 | One clip four lights, rest directional sun, no light dot | **PASS** (verified in code and by eye) |
| 2 | Eyes read from the side, angled right, white rings the star, star inside the lens | **FAIL** — three of four clauses |
| 3 | Antennae thin, balls thickest, junction hinges | **PASS** |
| 4 | Wobble strong, top-down, whole-body; teardrop; pitch | **PARTIAL — owner's eye** |
| 5 | Mist see-through again, thicker than v1 | **PASS** |
| 6 | Mana filled, centred, long glitchy decaying smear, strand lightning, variants | **PARTIAL — owner's eye**, with one clear miss |
| 7 | Every §7 clip fixed; hasty directional; tricks exist | **PARTIAL** — hasty and the headstand land; drift and fall regress on staging |
| 8 | Eyes and antennae visibly expressive in **every** clip | **FAIL** on fall and trick; PASS elsewhere |

---

### 1. Lighting — PASS

Verified by **reading the subject builders**, not by trusting the claim, which
is the specific thing gotcha §12 demands.

* `u02_common()` sets `s.sun = &kU02SunCalm` for **every** u02 subject, the s4
  diagnostics included. Stage L landed: a diagnostic plate on this creature now
  cannot be judged under a rig it does not ship with. This is the fault that
  made pass 2's whole visual verdict wrong, and it is closed by construction,
  not by convention.
* `creature_moving_light = true` appears on exactly two subjects in the whole
  file: `subject_zixx_moving_light` (Zixxtrixx's own, untouched) and the
  `unnamed02-inspect` builder.
* `cr_ctx.moving_markers = false` is set inside `if (species == kUnnamed02)` —
  species-scoped, so Zixxtrixx keeps its orbs. Confirmed by eye: no marker dot
  on any u02 frame I rendered (plate 20 shows the four coloured pools on
  inspect with no orbs).

*One thing for the owner's eye:* the inspect rig ships at the .20 ambient
class, and on roughly a third of its orbit the creature goes to a murky
blue-purple with the pink gone entirely (plate 20, tiles 3 and 6). The
showcase clip is the one place the owner will look longest.

### 2. The eyes — FAIL

This is fault 1 and it is the fault. Evidence: plates 02–06,
`evidence/review/eye-containment.txt`.

* **White ringing the star — FAIL.** At every angle the white reads as a long
  bright crescent along the lens's outer rim, which is exactly what Direction 3
  §2 forbids ("not an outline of the whole lens"). The *mechanism* is worth
  more than the observation: `mku02page.py` paints the ring "TIGHT around where
  the star rests (the outward station)" — one fixed spot on the lens page —
  while the star is separate geometry riding the pupil bone. The moment gaze
  moves, the star leaves its ring behind, and what is left is a ring around
  where the star used to be.
* **Star inside the lens — FAIL.** 37–39% of star pixels are directly adjacent
  to bare flesh or to ink at front and three-quarter; 71% in profile. Plate 05
  (curious, sampled at its four gaze extremes) shows the far star sitting
  almost wholly off its lens on bare pink skin. **This cannot be tuned out:**
  `kPupilStarArmMm` is 185 against `kEyeWideMm` 125, so the star overhangs the
  lens's short axis by 60 mm before the gaze moves at all. The plan's R3 was
  right to grow the star toward the sheet's ~20% share; it grew against the
  long axis while the short axis stayed put.
* **Reads from the side — FAIL.** 278 star pixels at front, 194 at
  three-quarter, **28** at profile (plate 04) — a 5x16 purple slit with a blue
  edge. The side sheet makes the eye the largest feature on the bulb.
* **Angle — needs the owner's eye.** The pair reads as a shallow, near-horizontal
  V; the front sheet's lenses are much steeper. The owner has now said "the
  angle is slightly wrong, still" twice.

**A refutation of my own first read, recorded per checklist item 9:** at 8x I
called the far star "floating on the sky outside the ink". It is not. A
flood-fill from the frame border, stopping at ink, finds **zero** star pixels
outside the silhouette on all three views. The eye assembly stands proud and
the ink wraps around it. The star breaks the *lens*, not the *outline*.

**What the QA claimed:** "PASS by eye … containment PROVEN by rendering
curious's held extreme — star+ring stay inside the lens ink." I rendered
curious and sampled it by star-pixel count rather than by index; on the frames
that measure most and least, the star is off the lens. One held extreme is not
the window.

### 3. The antennae — PASS

Thin tube, fat balls, and the junction knuckles are there and read as joints at
native (plates 01, 08). The inequality holds by construction (mid-tube blades
0.7x, max rx 70 mm; hinge balls 100 mm; knuckles 110 mm) and — more to the
point — it reads. Protect this.

Minor: from behind, the neck knuckle still reads a little like a wart rather
than a hinge (plate 08, bottom row). Cosmetic.

### 4. The body — PARTIAL, owner's eye

* **Wobble.** The mechanism is new and correct in shape: a bend starting at the
  loop peak, travelling to neck and body with a lag, on two incommensurate
  periods, with root pitch riding the slow wave. Every constant is new this
  pass (`kWobbleAmpPm`, `kWobbleLeanA16`, `kWobblePitchA16`, `kWobbleLagKeys`,
  `kWobblePerAKeys/BKeys`), so "stronger" is against a different mechanism, not
  a number the owner can compare. The amplitudes are small in absolute terms —
  lean 5.2°, pitch 4.3°. The owner asked for **stronger**; whether 5° is
  stronger than what he saw is his call, and the knobs are named and editable,
  which is what matters.
* **Teardrop.** Under-delivered. The whole change is `kBodyLeanXMm` 150 → 180
  (+20%) and sub-1% moves in `kBodyTaperPm`. The bulb still reads as a sphere
  with a neck attached at a shoulder, not the sheet's continuous onion.

### 5. The mist — PASS

I re-ran the ladder myself rather than inheriting the pick, using the shipped
`U02_AMBIENT` lane on `u02-s4-tq` — which, thanks to Stage L, is now on the
shipping rig (plates 18, 19). At .40 (v1's class) the rim is a narrow dark
edge; at the shipped .32 it is visibly wider and darker while the pink still
glows through the lit field; .26 and .20 march toward pass 2's murk. The
shipped default renders identical to the A32 rung but for one LSB in the green
ambient channel (23593 vs 23592, a truncation in the ladder helper), which
confirms the shipped value *is* the .32 class.

**Thicker than v1 and still see-through: both true, and demonstrable side by
side.** This is the cleanest piece of work in the pass. Protect the rung and
protect the ladder lane that found it.

### 6. The mana — PARTIAL, owner's eye; one clear miss

* **Filled — mostly.** There is a solid opaque mass now, not a transparent
  wash. Real progress. But the aqua lead reads as pale mint against the peach
  sky (plates 14, 15), not the saturated aquamarine the owner asked for. Owner's
  eye — and this is precisely the kind of value CLAUDE.md says is chosen by
  looking at it in scene, so it is a knob turn, not a rebuild.
* **Glitchy, decaying, long — YES.** Chunky square dropouts and stepped decay,
  clearly not a smooth exponential fade. The named failure is avoided.
* **In the middle of the ring — NO.** This is the clear miss. On the lead
  variant the plasma mass sits in the **top** of the pocket and a substantial
  lobe spills **outside the loop entirely**, above and right of the antenna
  (plate 14). Direction 3 §6c: "always in the middle of the circle and stays
  there … must not drift to the edge or wander off." The implementer flagged
  "upper pocket" as a residual; it is further out than that.
* **Strands not sparks — HALF.** Sampled by brightness rather than by index
  (plate 17: the two hottest frames, the median frame, the coldest). On the
  hottest frames they are genuinely beautiful continuous branching filaments —
  the claim is true there. On the **median** frame they are a scattered dot
  cloud, which is the owner's rejected "sparks". The QA's own gate was "gap
  check … on the three hottest channel frames", i.e. the frames where it works.
  At native (plate 16) the typical frame reads as glitter around the antenna,
  not lightning across the ring.
* **Variants — PASS.** Six ship, all alive, drip cut honestly.

### 7. The clips — PARTIAL

* **Hasty — PASS.** Genuinely directional, one continuous traverse, no circuit
  (plate 11).
* **Headstand — PASS, and it is a delight.** It tips, rolls, and stands on the
  antenna loop (plate 13). The declared contact reproduces exactly on the
  committed probe, which I built and ran myself: *deepest vertex −20 mm against
  a declared −25, inside the accepted −60..−5*, with the clearance contract
  holding at ≥40 mm everywhere else. Verified, not inherited.
* **Idle slower, startle sharper, curious double-take, lasso rebuilt** — present.
* **Fall — REGRESSION.** Made longer (170 keys / 3.6 m) by starting higher, and
  the start is now **above the frame**. 190 of 340 frames carry no face because
  for the opening of the clip there is no creature on screen at all (plate 10).
  The viewer watches an empty sky.
* **Drift — REGRESSION, and the worst staging fault in the pass.** It no longer
  just rotates, which was the ask — but over the last third the creature sinks
  into the hillside and ends as a nub at the frame's bottom-left corner, with
  the antenna loop gone entirely (plate 09).

  **Cause, in 3D, not from pixels.** `zhao_reel.cpp` ground-snaps the root with
  **one** `column_query` at `dog_inst.x/z`, which for u02 is the fixed stage
  centre. u02 renders at `bump_ext = 6`, a mound. Pass 3 gave drift 205 px and
  hasty 209 px of real lateral travel. So the creature's snap plane stays at the
  centre column's height while the ground rises under it as it travels. This
  is the *same* fault, with the *same* mechanism, that produced the "massive
  sink" Fabian rejected on Zixxtrixx's walk — where the fix, recorded in the
  subject's own note, was `bump_ext = 18`, flat ground, "so one column's snap
  speaks for the whole body".

  **And the committed probe cannot see it.** `u02_probe` measures clearance
  against a flat plane at the snap column and justifies that in its own comment:
  "terrain sloping away from the snap column only increases clearance, so the
  flat-plane bound is the conservative one." That holds only while the creature
  stays near the column. It reports **432 mm** of clearance for drift.
  Checklist item 14 asks what the probe under-tests; this is the answer.

### 8. Eyes and antennae expressive in every clip — FAIL on two

Genuinely good on curious (plate 05: the stars sweep, widen and nearly vanish
across the gaze range) and taunt. **Fall**: no face at all for 56% of the clip
because the creature is off-frame. **Trick**: the face is turned away or
inverted for most of the headstand — median 31 star pixels — so whatever eye
work is authored there is not seen.

**Refutation of my own read, recorded:** from a uniform 10-frame sample I
judged the taunt "essentially static". Measured per-frame pose change says
otherwise — taunt 0.132 mean, against curious 0.052 and hover 0.057. It is the
*most* per-frame-active fixed-camera clip. It is low-amplitude, not frozen, and
`kTauntWagglePm` is the right next knob exactly as the implementer said.

---

## The three flags the implementer raised

### Flag 1 — `inkmask.py` is vacuous. **Confirmed, and it is worse than declared.**

The implementer reported it as a u02-only caveat. It is project-wide. I
censused exact-ink pixels on Zixxtrixx clips too:

```
zixxtrixx-walk / idle / moving-light   exact-ink px per frame: min 0 med 0 max 0
unnamed02-hover / trick / taunt        exact-ink px per frame: min 0 med 0 max 0
```

The ink is written as exactly (26,24,22) and then quantised to RGB565: the
outline colours actually present are (25,24,25) and (25,24,16). The tool holds
the pre-quantisation value, so its mask is empty **on every path**, and
`np.array_equal(empty, empty)` is always True — **the check cannot fail.**

**What rested on it:** `RUN-20260903-2309-zixxtrixx-suns-every-clip` published
"ink-mask silhouette identity 6674/6674 frames" as the headline motion-untouched
proof for Direction 30, and `RUN-20260904-0437-zixxtrixx-suns-calmed` repeated
it as "22/22 subjects, every frame -> the animation bank is untouched". Both are
comparisons of two empty sets. Nothing needs re-doing — both passes also carried
CRC identity, which is sound and does the job better — but the silhouette leg of
that evidence was never evidence. Detail in
`evidence/review/inkmask-is-vacuous-everywhere.txt`.

**A second instrument fault found while checking the first.** The committed
`trajplot.creature_mask` returns 2034 pixels on `unnamed02-fall` frame 0000, a
frame with **no creature in it** — a full-width band at y=175..195, which is the
terrain horizon (plate 21). Row-median subtraction flags the row where sky meets
ground. Every trajplot series therefore carries ~2000 px of horizon, and the
QA's headline "area breathing 2054 px — no flat line on any channel of any clip"
is stated in the same units as the contamination.

### Flag 2 — the strand cost. **Confirmed; and the affordability question is untested by anyone.**

The pixel-visit arithmetic checks out: 3 strands x 16 segments x ~3.5 stamps x
(π·7² halo + π·3² core) ≈ 30,600 visits ≈ **33%** of a 92,160-pixel pass. The
QA's ~38% is the right order and the plan's 0.8% was wrong by ~40x. Credit for
surfacing it.

Two things to add. First, the note omits a real cost: every splat with
`gain_pm != 1000` rebuilds a 64x3 palette before drawing, and **twice** per
splat when the smear plane is on, because `smear_feed` runs its own copy — ~65k
extra operations per frame, the same order as the pixel visits. Second, the
reason it is expensive is **overlap, not coverage**: ~56 stamps over ~70 px of
screen is ~1.2 px between centres, each stamping a 14 px disc, so every filament
pixel is visited on the order of ten times. That is the ribbon argument made
concrete.

**Wall clock cannot settle this and should not be tried.** The orbit camera
costs more than the whole mana stack: `unnamed02-hover` with *no* mana runs 126
ms/frame while `unnamed02-mana-aqua` with the full filled plasma and the
persistence plane runs 90.

**And the plain answer asked for.** `zhao_reel.cpp` fills mana for
`ii == 0` only. `u02-trio` — "THREE phase-offset conduits … all effects" — draws
**one** conduit's mana and two plain bodies, which is why it times *faster* than
the single-conduit channel. **No subject has ever rendered several conduits'
mana.** So: as shipped, the mana is affordable for **one** conduit in the
reference renderer, measured. For several conduits it is **unmeasured**, the
subject that claims to test it does not, and the stamped-disc construction is
the wrong shape for it — the ~10x overdraw is intrinsic to stamping, not to the
look the owner picked. **It only becomes affordable at scale once the ribbon
block exists.** Numbers in `evidence/review/cost-measurements.txt`.

### Flag 3 — the residuals, confirmed and ranked

All five are real. Ranked among everything else below. The strand position
residual ("upper pocket") is understated: on the lead mana variant a lobe sits
outside the loop entirely. The taunt residual is correctly diagnosed. The lasso
is on probation fairly.

---

## Faults, ranked by damage to the read

1. **The star leaves the lens.** The owner's own words are "super weird and
   wrong", he has now said it twice, it is at the shipping camera, and it is
   visible at native. Arithmetic, not tuning: `kPupilStarArmMm` 185 vs
   `kEyeWideMm` 125 — the star cannot fit across the lens's short axis. The
   white must move with the star (or the star must stop moving relative to the
   ring), because the ring is painted at one fixed station on the page.
2. **Drift walks into the hillside, and fall starts off-screen.** Two of the
   clips the owner named are worse-staged after their rebuild than before it.
   The drift cause is known and precedented — one-column ground snap under a
   travelling root, on `bump_ext = 6` terrain. The probe cannot see it and says
   432 mm.
3. **The mana is not in the middle of the ring.** A named §6c ask, missed, with
   part of the mass outside the loop.
4. **The strands are lightning only on the hottest frames.** Typical frames read
   as glitter. Judge by median, not by peak.
5. **The eye vanishes in profile** (28 px vs 278 at front). Improved at
   three-quarter, which is the shipping camera, so this is fourth-order.
6. **No face for half of fall, and almost none through the headstand hold** —
   acceptance 8 not met on those two.
7. **Two committed instruments report confidently on nothing** — `inkmask.py`
   on every subject, `trajplot`'s mask on the horizon. No shipped art is wrong
   because of them, but the next agent will trust them.
8. **The teardrop barely moved**; the bulb still reads as a sphere on a
   shoulder.
9. **The inspect showcase goes murky** for part of its orbit at the .20 rung.

---

## What is RIGHT and must be protected

* **Stage L's construction.** Every u02 subject defaults to the shipping
  presentation *in `u02_common`*, so a diagnostic cannot lie about the rig ever
  again on this creature. This is the correct kind of fix — structural, not a
  convention someone must remember. **Do not let a future subject set its own
  rig outside this path.**
* **The one-sun bank with a single four-light inspect, and no marker dots.**
  The most visible regression the owner named is gone.
* **The .32 mist rung and the `U02_AMBIENT` ladder lane that found it.** The
  only place in the pass where a contested value was settled by rendering the
  alternatives and looking — and it survives an independent re-run.
* **The antennae**: thin tube, fat balls, real junction knuckles that read at
  native.
* **The headstand.** It reads, it is charming, and its ground contact is
  declared, probed and reproducible — a committed probe with a named window and
  an accepted range, which is exactly what the ground-contact law asks for.
* **The whole-body travelling wobble as a mechanism** (bend starts at the loop
  peak, arrives late in the body, two incommensurate periods, pitch as well as
  yaw), with every value a named editable constant.
* **The filled opaque core under the additive halo** — the right answer to
  "everything is too transparent", and the glitchy stepped decay, which
  correctly avoids the named smooth-fade failure.
* **Zixxtrixx, untouched.** Byte-identical, proven from a build I made myself.
* **The implementer's honesty.** It declared the vacuous checker, declared that
  its cost estimate was 47x the plan's, declared the gate-off path as stated
  rather than verified, and listed five residuals it could have stayed quiet
  about. Two of my three biggest findings started from its own flags. That is
  the behaviour that makes a gate cheap; it should be said out loud.

---

## Verified vs inherited

**Verified by this reviewer, first-hand:**

* Stage L, by reading `u02_common`, `subject_u02_clip`, `subject_u02_s4` and the
  dispatch in `render_scene` — not by grep-count alone.
* Zixxtrixx identity: a fresh `build-direct.sh` at `6d45f267`, a fresh build of
  the shipped tree, and `diff -rq` over every frame of 5 subjects — 2,196 frames,
  0 differing files. Byte-for-byte, deliberately not CRC and not ink mask.
* The headstand's ground contact and the whole clearance contract: `u02_probe`
  built and run by me, output saved.
* The mist ladder: all four rungs re-rendered under `U02_AMBIENT` and compared
  against the shipped default.
* The eye geometry and containment: measured on the comparison side, then
  explained from the shipped constants.
* The strand cost arithmetic, the palette-rebuild omission, the overlap factor,
  and the `ii == 0` mana scope.
* `inkmask.py`'s emptiness on both paths, and the trajplot mask's horizon.
* Site: 451 media references, 0 missing; exactly one `robots` `noindex` meta;
  the pass-2 archive files present.

**Inherited, not re-checked:**

* 17 of the 22 Zixxtrixx subjects (I re-rendered 5, chosen for risk).
* The `ZIXX_SUNS=off` gate-off path — the implementer states it, does not verify
  it, and neither do I. Checklist item 16 wants it proven; it is the revert path.
* The moving-rig ambient ladder pick for inspect (.20), judged by them alone.
* Anything about the *published webm encodes* — I judged `.rgb` frames from my
  own renders, not the shipped video.

**Could not confirm:**

* Whether drift's disappearance is penetration or occlusion **in the shipped
  frames specifically** — the 2D image cannot separate them and CLAUDE.md
  forbids guessing from pixels. What I *can* state is the mechanism (one-column
  snap, travelling root, `bump_ext = 6`) and that the probe's flat-plane bound
  is not conservative for a travelling clip. **A probe that re-queries the
  terrain column along the clip's own travel would settle it, and does not
  exist.** That is the first thing the next pass should build.
* Whether 5° of lean and 4.3° of pitch is "stronger" than what the owner saw —
  the mechanism changed, so there is no like-for-like comparison to make.

## Background work

Every render, build and probe this review started was run in a named output
directory and confirmed finished; `tasklist` shows no `zhao-reel`, `g++`,
`cc1plus` or stray `python` process at close. A `git worktree` at `6d45f267`
was created at `C:\zrev` for the pristine baseline (the repo path is too long
for a worktree under the scratchpad) and should be pruned when this lane closes.
No authored art value was changed, and nothing was published.
