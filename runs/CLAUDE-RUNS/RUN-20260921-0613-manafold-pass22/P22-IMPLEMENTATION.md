# Manafold pass 22: implementation

**Date:** 2026-09-21
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-23-2026-09-21.md`,
plus two same-day owner additions relayed mid-pass:
* *"When the top nodule moves down, the lightning shape should react more.
  Change form and or rotate around. That is the kneading. The antennae are very
  good now."*
* *"The lightning must not change size (unless it's part of fold and rotate)
  from distance, at least not more than it already does. It is good now as it
  is. The particles must change size from distance kinda like the black cel
  shading outlines."*

**Worker:** Claude (sole Opus worker; no Qwen, no sub-agents)
**Source:** Zhaozhou `manafold-pass22`, commit `379bd8bc`
**Verdict:** DONE for the effects packet. Not rendered here: the 22-subject
bank, the encode, the merge, the deploy.

---

## 1. Item 1 — were the dots scaled before? **No. Not at all.**

Stated plainly, because the direction asked for it stated plainly.

`mana_fold` (manafold_fx.h) ends each mote with two pushes:

| splat | authored radius | source |
|---|---|---|
| additive halo | `kMoteHaloRPxMin..Max` = **7..10 px**, hash-chosen | manafold_art.h:4432 |
| opaque soft core | `halo * kMoteCoreOfHaloPm/1000`, 1600 pm = **11..16 px** | manafold_art.h:4468 |

and the renderer took the authored value verbatim:

```cpp
const int32_t r_px =
    ms.line ? u02::mana_line_r_px(ms.r_px, primary_radius_q8) : ms.r_px;
```

Only pass 19's LINE splats had a distance term. Motes were **fully
screen-space**: the same 16 px core on the 128 px Drift creature and on the
475 px Inspect one — an eighth of the animal per dot at Drift. That is the read
the owner described.

**The projected radii, measured off the reel's own celmain telemetry** (these
matter for every claim below):

| clip | projected radius (px) |
|---|---|
| Drift | **127.7, flat across the whole clip** |
| Hasty | 117.1 – 138.4 |
| Hover | 284.7 – 402.6 |
| Inspect | **363.7 – 514.4** |

So **Inspect is the byte-exact near clip** — its minimum clears the ink's
360 px close radius. Hover is only *partly* exact and must not be quoted as the
near proof; 473 of its 600 frames sit below 360.

## 2. Item 1 — the mechanism

`ManaSplat::dot`, set by a new `mote_push` wrapper on exactly those two pushes.
The renderer's radius decision is now **one three-way function**, and that
function is the split the owner's clarification requires:

```cpp
int32_t u02_splat_r_px(const u02::ManaSplat& ms, int32_t primary_radius_q8) {
  if (ms.line) return u02::mana_line_r_px(ms.r_px, primary_radius_q8);
  if (ms.dot)  return u02::mana_dot_r_px(ms.r_px, primary_radius_q8);
  return ms.r_px;
}
```

**Which law: the INK's, not pass 19's — and they are not the same.** The owner
named the model (*"kinda like the black cel shading outlines"*), and the brief
said to follow the ink if the two differ. They differ:

| | measure | curve | far behaviour |
|---|---|---|---|
| pass-19 line law | `primary_radius_q8` | straight ratio to 360 px | 355 pm at Drift, **no floor** |
| **cel ink** | `primary_radius_q8` | piecewise ramp through 120 / 200 / 360 px carrying 1 / 2 / 4 px | **274 pm** at Drift, **floored at 250** |

So `mana_dot_r_px` evaluates the ink's own ramp. Two details:

* **In per mille, not `cel_main_ink_width` itself.** That function returns whole
  pixels, so as a multiplier it has four rungs (250/500/750/1000). Hover travels
  284.7 → 402.6 px *inside one clip*, which would cross a rung and pop every dot
  by a third in one frame. The integer exists because ink must be drawn in whole
  pixels; it is not part of the law.
* **The mirrored knots are checked.** `zhao_reel.cpp` is the only translation
  unit that sees both the `kCelInk*` constants and `u02::kManaDot*`, so it
  carries five `static_assert`s tying each one to the ink constant it mirrors.

Plus `kManaDotMinRPx = 2` — the same idea as the ink's own 1 px floor, one level
down.

At or beyond the close radius the curve is exactly 1000 and `r' == r` with no
arithmetic at all, so **the close-up read is byte-identical by construction**.

### Toggles

| knob | values | shipping | exact legacy |
|---|---|---|---|
| `ZHAO_U02_MANA_DOT_SCALE` | `ink` / `distance` / `legacy` | **ink** | `legacy` |
| `ZHAO_U02_MANA_DOT_FULL_PX` | 40..2000 | 360 (`kManaDotFullRadiusPx`) | n/a |
| `ZHAO_U02_MANA_DOT_STRENGTH_PM` | 0..1000 | **600** (`kManaDotStrengthPm`) | 0 |
| `ZHAO_U02_FOLD_DIP_SHAPE_PM` | 0..3000 | **1000** (`kFoldDipShapePm`) | 0 |

All strict; a bad value is RC 2. The dot knobs go through a shared parser
(`u02::apply_mana_dot_env`) that the reel *and* mrear call — pass 20 lost a whole
ladder to a knob only the reel read. `ZHAO_U02_FOLD_DIP_PM` was in exactly that
state and is now in `apply_knead_dip_env` too.

## 3. Item 1 — the chosen value, and the visual reason

**`ink` at strength 600.** Ladder `{350, 500, 600, 700, 1000}` plus the pass-19
line law, rendered complete and read at native, magnified 5×/7×, on Drift f289
and Hasty f012 — the worst-change frames, chosen by badness, not by index.

| rung | scale at Drift | what was seen |
|---|---|---|
| legacy | 1000 pm | the complaint: a fused cloud of pale balls, each wider than the antenna loop, burying the green pocket and the white strand |
| 350 | 746 | not a change |
| 500 | 637 | dots become separate dots again; the loop reads on top |
| **600** | **564** | **shipped** |
| 700 | 492 | fine, but the outer wanderers begin to read as hard little squares |
| line law @1000 | 355 | green nearly gone; wanderers are 1–2 px dirt |
| ink @1000 | 274 | dust — the effect is removed |

**The obvious law overshot.** Following the owner's named model at full depth
deleted the thing he wanted smaller. An ink line and a mana dot have different
vanishing points: a 1 px line is still a line, a 2 px dot beside a 3 px
neighbour is dirt. So the ink's **law** is right and its full **depth** is not,
and 600 is the eye's number.

At 600 the dots are 56% of their close-up radius at Drift, about a third of the
area.

**Is it honest? Yes — measured at the SHIPPING rung, not at the ladder's
extreme.** At strength 600, against pass 21:

| clip | frames changed | mean changed px (of 92,160) | worst frame | unique colours |
|---|---|---|---|---|
| Drift | **300 / 300** | 1,886 (2.05%) | 2,310 | 13,654 → 13,356 |
| Hasty | **240 / 240** | 1,965 (2.13%) | 2,145 | 13,946 → 13,444 |

Every frame moves, by about 2% of the whole picture and much more of the mana
pocket itself. For scale, pass 20's *rejected* first reaction moved 12 frames of
600 and 72 pixels. And the small colour-count drop is the point of choosing 600
over 1000: at 1000 Drift's unique colours collapse 13,654 → **9,554** because the
field is being deleted; at 600 the mana keeps its variety and only the dots get
smaller.

Plates: `P22-LOOKS/01`–`04`, `08`. Notes written after each look:
`P22-NOTES/FINDINGS-02-dot-ladder.md`.

## 4. Item 5 — the lightning answers the knead

Pass 20 already gave the whole mana body a squeeze (drop + flatten + spread
about the ring). That is a scale and a translation: same figure, same attitude.
The owner asked for the part a scale cannot give.

Three terms on the placed figure, inside `place()` — the lightning figure's own
transform — applied about the same pivot and after the pass-20 squeeze:

| term | constant | at the reference |
|---|---|---|
| in-plane ROLL | `kFoldDipRollA16` 9000 | ~49° — "rotate around" |
| depth TUMBLE | `kFoldDipTumbleA16` 3200 | ~18°, deliberately well under the roll |
| height SHEAR | `kFoldDipShearPm` 420 | the form change: the figure leans and curls |

**`morph_pm` was deliberately not touched.** Advancing the stencil morph during
the press is the obvious "change form", and it is wrong: the morph would have to
come *back* on release, and msmooth's `--fail-morph-reverse` leg exists because
a reversing morph is a defect. So the form change is geometric, the morph
scheduler is untouched, and "morph, not switch" is true by construction — there
is no switch anywhere in this. Station identities, topology and the persistent-
identity behaviour are all unchanged.

### The instructive part: the first cut measured present and read as a wobble

It changed 127 of 600 frames on Inspect *and* Hover, worst frame 2177 px of
92,160 — **thirty times** pass 20's rejected reaction — and on the strip the loop
only wobbled.

Cause, measured with `U02_FOLD_DEBUG` over ten clips: **`dip_pm` never
approaches 1000.**

| clip | peak `dip_pm` | | clip | peak |
|---|---|---|---|---|
| trick | 208 | | hasty | 85 |
| hover | 152 | | pirouette | 72 |
| inspect | 152 | | drift | 27 |
| rest | 147 | | blown | 18 |
| channel | 111 | | taunt III | 0 (hosts no dip by design) |

`dip_pm = smoothstep(sag) * kFoldDipGainPm(650)/1000`, and the sag never nears
`kFoldDipRefMm = 420 mm`. So "49° at a full dip" was **7.5°** in every frame that
exists. A constant written against a state that never occurs cannot be judged by
eye — and this retro-explains pass 20's very large drop/squash/spread numbers
(330 mm / 430 / 700 are ~15% effective).

The fix is a **declared reference**, `kFoldDipShapeRefPm = 150`, so the three
constants are the values at a press that actually happens; and it **saturates**
there, which is Direction 18's law (express a carrier's gesture, never amplify
it) and bounds any future deeper clip.

### Chosen by eye

Ladder `{400, 700, 1000}` × the reference on Inspect f591 (the dip bottom) at 7×,
then in motion across f567–597 and on Hover f579–597.

* 400 (~20°): the loop visibly turns and narrows. Reads.
* 700 (~34°): the right end swings up; the form becomes a leaning wedge.
* **1000 (~49°): shipped.** The loop turns and curls clearly, while keeping the
  same topology, the same white core, the same navy backing and the same station
  count — a form change, not a restyle. The owner asked for the lightning to
  react *more*, so the strong end of a ladder that reads at every rung is the
  right end.

In motion the gesture arrives and leaves without a step (f567 is
indistinguishable from pass 21 at every rung), and **473 of 600 frames on both
clips are byte-identical to pass 21** — away from the press the lightning is
untouched.

Plates `P22-LOOKS/05`–`07`; notes `P22-NOTES/FINDINGS-03-knead-ladder.md`.

## 5. What is byte-identical

| configuration | hover | inspect | drift | hasty |
|---|---|---|---|---|
| **pass-21 baseline** | 0x200AA3E7 | 0x1CFE8375 | 0xFB17B7D6 | 0x425AA389 |
| exact-off (`DOT_SCALE=legacy` + `FOLD_DIP_SHAPE_PM=0`) | 0x200AA3E7 | 0x1CFE8375 | 0xFB17B7D6 | 0x425AA389 |
| dots on, knead off | 0xE379F918 | **0x1CFE8375** | 0x148ABAF0 | 0x40E54E64 |
| **shipping (both on)** | 0xEFCCD8FA | 0x6B1077D0 | 0xB6AB88AA | 0x6B85677A |

* **Exact-off is 4/4 byte-identical to pass 21.** Drift and Hasty are in the
  witness list deliberately — they are the clips the dot law actually changes, so
  a leg sampling only hover/inspect could not fail. (Pass 20 had precisely that
  defect.)
* **Inspect is byte-identical with the dots ON**, by construction: its projected
  radius never drops below 363.7 px. That is the near-read proof the direction
  asked for, at the shipping strength.
* **The lightning gained no distance dependence.** The coordinator asked for a
  byte test on a far and a near clip; what is below is stronger, because a
  two-clip byte test cannot separate "the lightning did not move" from "the dots
  happened to cover it", and it samples 2 clips of 24.

  **(a) Exhaustive, on real splats.** R6's census over **all 24 clips** at both
  witness distances saw **11,561,258 line splats and 164,900 plain splats, and
  zero of them moved.** Line splats still draw exactly `mana_line_r_px` (pass
  19's law, unchanged); plain splats still draw their authored radius.

  **(b) Structural — the flag cannot reach anything else.** In the whole
  production path `ManaSplat::dot` is declared once, **written in exactly one
  place** (`mote_push`, which only the two fold-mote pushes call) and **read in
  exactly one place**:

  ```
  zhao_reel.cpp:3193:  if (ms.dot) return u02::mana_dot_r_px(ms.r_px, primary_radius_q8);
  ```

  So the only quantity the dot mechanism can change is the drawn radius of a
  splat `mote_push` produced. No lightning, strand, body, glow, bullet, boil or
  pulsar splat is ever marked, and `u02_splat_r_px` returns on `ms.line` before
  it ever asks about `ms.dot`. There is no path by which a distance term can
  reach the lightning's size.
* Untouched entirely: the pass-21 rods rig and its ball joints, the knead/dent
  solver, every rear-chain path, the palettes, the mote count, roles and
  visibility, the shell/fog, the eye lane, the live-history contract and the
  site. No RTL change and no fit.

## 6. Gates

Two new legs on `manafold-rear-audit --gate`.

**R6 DOT (mask 0x20)** — a law sweep *and* a real-splat census, because pass 19
had to be sent back for shipping only the first. The census walks **every clip
in the bank** (mrod was repaired at the pass-21 close for sampling 6 of 24;
pass 19's own census sampled 2), evaluating the renderer's own three-way select
at Drift's 127.7 px and at Inspect's 363.7 px minimum. It asserts:

* both populations exist and no splat claims both flags;
* every dot draws **exactly** its authored radius at the near distance;
* every dot ≥ 4 px draws **strictly smaller** at the far distance, never under
  the floor;
* **every non-dot splat draws exactly what it drew before, at both distances** —
  the owner's lightning constraint, as a positive assertion.

Shipping: 691,182 dot splats, **all** exact at near and **all** shrunk at far;
0 violations.

**R7 KNEAD (mask 0x40)** — measured on the **real 18 fold stations**, by running
`mana_fold` twice over the same frame with the response on and off. It does
*not* trace the knob: reading `dip_roll_a16` back would be a detector wired to
its own operand and would report the constant, not the figure. Two descriptors:

* `rot_deg` — station 0's bearing from the centroid, on vs off. "Rotate around".
* `form_pm` — the largest change in a pairwise station distance after
  normalising each cloud by its own RMS radius. **Rotation- and scale-invariant,
  so a pure roll scores exactly zero on it**; only a genuine form change moves it.

and the window test: both must be **exactly zero** on every frame the reaction is
not running. Shipping: 19 clips reach the press, best rotation **43.00°** (slot 5,
floor 12), best form **239.5 pm** (floor 40), and **0.0000 / 0.0000 outside the
press**. The floors are regression guards set below the measured worsts with
margin; they hold the reaction to being present and confined and choose none of
its size.

### Controls fired, each with its exact declared mask

| control | mask | what moved |
|---|---|---|
| `--fail-dot-scale` | **0x20** alone | dots shrunk at far: 691,182 → **0** |
| `--fail-dot-flag` | **0x20** alone | dot population 691,182 → **0** (routing, not law) |
| `--fail-knead-shape` | **0x40** alone | rotation 43.00° → **0.00**, form 239.5 → **0.0** |

None of the three asserts a bug. `--fail-knead-shape` switches the reaction off
with its own shipping exact-off knob and the leg must notice, so it does not
expire after a repair.

### Full matrix — **179 / 179 PASS, 0 FAIL**

`P22-RECEIPTS/gatematrix_p22.sh`, run in **one invocation**:
`P22-RECEIPTS/gate-matrix.txt`.

| family | legs | |
|---|---|---|
| normals | **14** | every gate binary, plus mrod and both mrear modes |
| mrear mask controls | **10** | each with its exact declared mask, incl. the 3 new |
| mrod | **7** | 4 controls fired + attributed, 3 retired names as hard errors |
| mspan controls | **37** | |
| msmooth controls | **16** | each firing only its own category |
| protected legs | **27** | |
| Wave-F mqa | **5** | |
| selectors (RC 2) | **56** | the 49 carried forward plus 7 new |
| identity + live history | **7** | incl. both pass-21 exact-off legs |

**No bound was relaxed anywhere** — the source diff removes or changes **zero**
`constexpr` lines across all five files (783 insertions, 10 deletions, the 10
being the two renderer radius lines and one debug printf). The four `kGate*`
constants added are all new and belong to the two new legs.

Three script expectations were corrected along the way, each checked against
pass 21's own receipts rather than rubber-stamped:

* `r-rear-frame` 0xB→**0x3** and `r-rear-joint` 0xA→**0x2** — stale copies from
  the pass-20 script. Pass 21's `mrear-ctl-fail-rear-frame.txt` already records
  0x3: under the rods rig the legacy-root frame no longer trips R4 STRAIN.
* The four mrod control legs asserted `rc=1`. **mrod returns 0 for a control
  run by design** — a control is an instrument demonstration, not a gate
  failure, and pass 21's `ctl-*.txt` record `RC=0` beside "the control FIRED".
  Asserting `rc=0` alone would be worthless (a control that silently stopped
  firing would then pass), so the leg now requires the gate's own two claims:
  **"the control FIRED"** *and* **"-> ATTRIBUTED"**.

## 7. Open issues (non-blocking)

1. **The knead reaction is strong on deep-pressing clips and faint on shallow
   ones — declared, not hidden.** R7 prints every hosting clip, and the spread
   is real:

   | rotation at the press | slots |
   |---|---|
   | 35–43° (reads plainly) | 5, 3, 10, 23, 22, 0 |
   | 14–28° (reads) | 2, 11, 12, 6, 8, 4, 9, 19 |
   | **1.7–7.1° (barely perceptible at native)** | **18, 20, 14, 17, 1** |

   That is `dip_pm` being proportional to how deep each clip actually presses,
   and it is the honest behaviour — a clip whose middle nodule barely dips
   should not have its lightning spin. But on slots 18 and 20 the owner will
   likely not see a reaction at all, and calling those "done" would be the
   invisible-change fault. If he wants the beat everywhere, the lever is that
   clip's own `kKneadDipClipPm` share or the dip depth, **not** a bigger roll
   constant — raising the roll would over-drive the clips that already read.

2. **`dip_pm` runs at a sixth of its declared range, bank-wide.** Pass 22 works
   around it with a declared reference. The underlying question is whether
   `kFoldDipRefMm = 420 mm` is the right reference for the *pose* at all — no
   clip sags past ~90 mm over the onset. If the owner ever wants the pass-20
   squeeze stronger, that is the constant to look at, not the gains. Item 1
   above is the same constant seen from the other end.
3. **Hover is not a byte-exact near clip** and should not be quoted as one; its
   radius dips to 284.7 px so 473 of its 600 frames take some dot scaling. The
   byte-exact near proof is Inspect.
4. **The dot strength is one number for all four dot sizes.** A halo and its
   1.6× core shrink by the same factor, so their *ratio* is preserved but the
   halo hits the 2 px floor first at extreme distance. No clip in the bank is
   far enough for that to bite.
5. **The R7 form descriptor is a maximum over pairs**, so it reports the single
   most-changed chord rather than a whole-figure energy. That is the right
   sensitivity for a gate and a poor summary statistic; do not quote 239.5 pm as
   "the figure changed by 24%".
6. **Not rendered here:** the 22-subject bank, the encode, the merge and the
   deploy, by instruction.

## 8. Evidence

| file | what |
|---|---|
| `P22-LOOKS/01-drift-f289-3x.jpg` | the complaint, and the obvious law overshooting |
| `P22-LOOKS/02-drift-f289-ladder-5x.jpg` | six rungs including both laws |
| `P22-LOOKS/03-drift-f289-500-600-700-7x.jpg` | the three that were in contention |
| `P22-LOOKS/04-hasty-f012-ladder-7x.jpg` | the same three on Hasty |
| `P22-LOOKS/05-inspect-knead-strip-4x.jpg` | the knead in motion, three rungs |
| `P22-LOOKS/06-inspect-f591-knead-ladder-7x.jpg` | the knead ladder at the dip bottom |
| `P22-LOOKS/07-hover-knead-strip-6x.jpg` | the same beat on Hover |
| `P22-LOOKS/08-shipping-drift-hasty-native-2x.jpg` | shipping vs pass 21, full frame |
| `P22-NOTES/FINDINGS-01..03` | written after each look, before the next change |
| `P22-RECEIPTS/gate-matrix.txt` + `gatematrix_p22.sh` | the matrix and its script |
| `P22-RECEIPTS/mrear-gate.txt`, `ctl-fail-*.txt` | R6/R7 shipping and the three controls |
| `P22-RECEIPTS/identity-crcs.txt` | the four configurations of §5 |
