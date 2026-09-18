# Manafold Pass 17 public A/B/C ordering plan

**Date:** 2026-09-18
**Scope:** Direction 16 motion architecture after the 23-bone signed-span structure
**Status:** read-only plan; no art value selected and no source edited

## Decision

Put the public ordering performance in **Taunt III's existing shimmy window, keys 100..146**. Do not add a diagnostic-only clip and do not overload Taunt/Taunt II:

- Taunt already has a wind-up, frozen wink/cross-eye hold and five-carrier travelling beat.
- Taunt II is a lasso action; a second ordering performance would compete with the lasso.
- Taunt III already has `kNoduleClipPm[21] = 0`, so its nodule performance is wholly authored rather than mixed with ambient oscillators. Its shimmy is explicitly the three-ball beat and currently fails because three positive presses never give every carrier top and bottom ownership.

The ordering phrase is the **set-up**, not the final held punchline. Keys 149..173 must remain a readable, still dismissal with the face and asymmetric eyes visible. Making the punchline itself cycle through rankings would destroy the hold that motion-style §8a requires.

## Mechanical action: the crown shuffle

Manafold plays a deliberate three-cup shell game with its antenna crown:

1. **A steals the crown; C ducks** — `A > B > C`.
2. **B steals it; A ducks** — `B > C > A`.
3. **C steals it; B ducks** — `C > A > B`.
4. **A steals it back and holds** — `A > B > C`.
5. The creature dismisses the viewer with the existing snap/hold.

Four tableaux are the mathematical minimum that gives every carrier a top and bottom witness **and** makes every pairwise height curve cross at least twice:

```text
A/B signs: + - + +  -> two changes
A/C signs: + - - +  -> two changes
B/C signs: + + - +  -> two changes
```

This is not one curve sampled at three phases. Each tableau is a separate authored A/B/C target, with one monotone transition into a flat hold. The repeated first tableau closes the shuffle before the release.

## Timing table

Keep the existing 46-key shimmy allocation and the accepted dismissal key at 146:

| Keys | Presentation frames | Action |
|---|---:|---|
| 100..104 | 200..208 | punch into tableau 0 |
| 104..110 | 208..220 | hold `A > B > C` |
| 110..114 | 220..228 | punch into tableau 1 |
| 114..120 | 228..240 | hold `B > C > A` |
| 120..124 | 240..248 | punch into tableau 2 |
| 124..130 | 248..260 | hold `C > A > B` |
| 130..134 | 260..268 | punch into tableau 3 |
| 134..140 | 268..280 | hold `A > B > C` again |
| 140..146 | 280..292 | smooth release to zero |

Each tableau owns 20 presentation frames from transition start through hold, above the 16-frame registration floor. Each hold itself is 12 frames. Transitions use `punch_ease`; release uses `fold_ease`. There is no sine and no free-running clock.

Named witness frames are the centre of each hold:

- **f0214 / key 107:** A top, C bottom (`A > B > C`).
- **f0234 / key 117:** B top, A bottom (`B > C > A`).
- **f0254 / key 127:** C top, B bottom (`C > A > B`).
- **f0274 / key 137:** A top, C bottom again (`A > B > C`).

The existing shrug nodule tail must reach zero by `kTaunt3ShimmyKey`; otherwise the ordering table does not own the quantity it claims. The slow body lean may continue underneath, but prior nodule offsets may not bias the ranking.

## Named art controls

Keep values owner-editable and choose them from native renders. The source shape should be:

```cpp
struct Taunt3OrderTiming {
  int attack_begin;
  int attack_end;
  int hold_end;
};

constexpr Taunt3OrderTiming kTaunt3OrderTiming[4];
constexpr int8_t kTaunt3OrderRank[4][3];  // high/middle/low assignment
constexpr int32_t kTaunt3OrderHighMm[3];
constexpr int32_t kTaunt3OrderMidMm[3];
constexpr int32_t kTaunt3OrderLowMm[3];
constexpr int32_t kTaunt3OrderEndpointMm[4][2]; // Front/End rotational punctuation
constexpr int32_t kTaunt3OrderBodyRollA16[4];
constexpr int32_t kTaunt3OrderBodyLiftMm[4];
constexpr int32_t kTaunt3OrderLeanPm;
```

High, middle and low are per carrier because hierarchy carries downstream balls; copying one amplitude to A/B/C is structurally wrong. Do not generate these values from target rankings or a concept projection.

The whole-body table is required by motion-style §8b:

- the A-crown tableaux get a body tip that supports A's arrival;
- B-crown gets its own root lift/squash punctuation rather than a visually empty zero-roll state;
- C-crown tips the body the other way;
- every body component parks during its tableau hold.

Front and End remain body-attached. Their table entries feed their existing local rotations so both endpoints visibly answer the shuffle without translating their centres.

## One-binary authoring ladder

Add validated, default-identity renderer controls parsed before `u02::type()`:

```text
ZHAO_U02_ORDER_GAIN_PM      // 0 is the exact no-order control
ZHAO_U02_ORDER_A_PM         // per-ball correction for hierarchy
ZHAO_U02_ORDER_B_PM
ZHAO_U02_ORDER_C_PM
ZHAO_U02_ORDER_BODY_PM
ZHAO_U02_ORDER_ENDPOINT_PM
```

Suggested first comparison rungs are 750 / 1000 / 1250 pm, but no rung is selected by this plan. Native pictures choose the value. Invalid ranges fail RC 2; no silent clamp.

Route the five authored inputs through `swallow_nodules` (or a factored helper that calls the same `apply_public_joint_mute`) so `ZHAO_U02_JOINT_MUTE=F|A|B|C|E` remains the one production control. Do not invent a private ordering mute.

## Production consumption

Replace the current positive-only shimmy block in `build_taunt3` with a small table evaluator:

1. compute the current and target tableau;
2. interpolate each carrier monotonically with `punch_ease` during attacks;
3. return the exact target during holds;
4. `fold_ease` the last tableau to zero over keys 140..146;
5. populate signed Front/A/B/C/End inputs;
6. call the existing public mute at the real consumption point;
7. add the authored body tableau after the existing root attitude, before `g.write`.

Do not change `kTaunt3Keys`, anticipation, first shrug hold, lean, dismissal attack/hold, release or loop seam. Taunt III's face-orientation repair remains a separate body-orientation decision in the dismissal window.

## Height and signed-span gate

Extend `mspan`; do not add a near-duplicate ordering executable.

### Production measurements

Build shipping slot 21 normal and A/B/C-muted controls through the real builder. At every key and presentation midpoint:

- decode the production pose;
- compute root-local centroids of the **actual rigid visible cores** for A/B/C;
- emit `A_y`, `B_y`, `C_y`, `A-B`, `A-C`, `B-C`;
- emit signed F-A, A-B, B-C and C-End length deltas from the real translation tracks;
- preserve the existing full-bank posed-ring, closure and attachment checks.

A bone request or authored offset is not a height witness; only visible-core skin is.

### Acceptance after looking

After a native candidate is accepted, record one named `kTaunt3OrderReadMarginMm` below the visible witness with real headroom. Gate:

- f0214: A above both by the margin; C below both by the margin;
- f0234: B above both; A below both;
- f0254: C above both; B below both;
- f0274: A above both; C below both;
- A-B, A-C and B-C each cross the deadband at least twice inside f0200..f0280;
- every connecting span shows one positive and one negative signed-delta witness inside the authored phrase, with a render-selected non-dither margin;
- Front/End centres retain their attachment contracts while their local rotations remain live;
- normal slot 21 remains byte-identical to the bank's direct build.

CSV columns:

```text
frame,key,sub,A_y,B_y,C_y,AB_y,AC_y,BC_y,FA_mm,AB_mm,BC_mm,CE_mm
```

### Failable controls

`--fail-mute A|B|C` must remove that carrier's named top/bottom evidence and crossing contribution while unrelated carriers stay live. Add a `--fail-order`/gain-zero control that removes the whole four-tableau phrase while preserving Taunt III's other channels. A red leg asserts correct behavior's absence; it must not pass merely because another structural check failed.

Keep existing F/E public mute, staged-fraction, posed-order and closure mutants.

## Render evidence

Shipping evidence is mandatory:

1. complete 368-frame `manafold-taunt3` sheet at 384×240;
2. native four-tableau witness grid f0214/f0234/f0254/f0274;
3. exact 4× nearest-neighbour crops containing all three carriers, adjoining spans and face;
4. normal versus A/B/C mute at both the named top and bottom frame for that carrier;
5. accepted gain plus no-order, low and high same-binary ladder controls;
6. A/B/C height-trajectory and four-span signed-length plots from the committed CSV tool.

Add effects-off fixed-camera diagnostic subjects using the same slot-21 bank:

- `manafold-ordering-fixed` (house judging view);
- `manafold-ordering-quarter` (~35°);
- `manafold-ordering-side`;
- `manafold-ordering-rear`.

Do not freeze body/root channels: the body participation is part of the performance. Diagnostics explain mechanism; only the shipping subject can accept public motion.

## Stop conditions

Reject and iterate before a full bank if any of these is true:

- a ranking needs labels or a trajectory plot to be seen at native scale;
- any carrier lacks an unmistakable top and bottom witness;
- any pair crosses fewer than twice or only jitters around zero;
- any one of F-A/A-B/B-C/C-End lacks visible extension **or** compaction;
- muting a carrier leaves its named tableau visually unchanged;
- a carrier core slides, a span buckles/collapses, an O edge opens, or Front/End detach;
- a transition registers in fewer than 16 presentation frames, buzzes, overshoots or adds incidental reversals;
- body punctuation hides the eyes or overwhelms the carrier order;
- only an effects-off diagnostic reads while shipping Taunt III does not;
- the phrase consumes or weakens the keys 149..173 punchline hold;
- the final C-up pose crops against the top of frame.

## File boundary

Ordering art belongs in a separate commit after the 23-bone structural packet:

- `manafold_art.h`: named ordering tables/knobs;
- `manafold_clips.h`: table evaluator and production consumption;
- `zhao_reel.cpp`: validated same-binary controls and diagnostic views;
- `manafold_spangate.cpp`: visible-core height/crossing/span acceptance and mutes;
- direct/CMake source lists only if a durable plotting helper is added;
- run-folder CSV, plots, every-frame sheets, mute plates and by-eye verdict.

Do not mix Trick-axis selection, dismissal orientation, eye-size retuning, outline work or media publication into this ordering-art commit.

## Acceptance sentence

Direction 16's public-motion half is complete only when the shipping Taunt III visibly performs the four-tableau crown shuffle—A/B/C each top and bottom, every pair crossing twice, all four sticks extending and compacting—while F/End remain attached and independently alive, the whole body supports each arrival, and the pictures stay continuous and readable without the gate's labels.
