# QA — creature 02 pass 3, the implementer's own gate (Direction 3's eight acceptance items)

**Run:** RUN-20260905-0555-u02-pass3-implementation · **Date:** 2026-09-05
**Judged on:** shipping subjects only, final build, ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross, native 384x240 (+ close-ups where named).
Worked against `Upheaval/creature/10-GATE-CHECKLIST.md` (landed mid-run and
read before closing).

## The eight acceptance items

1. **One clip has the four coloured lights; every other clip is the
   directional sun; no light dot — PASS.** Grep-proof: `creature_moving_light
   = true` appears only in `subject_zixx_moving_light` (Zixxtrixx-owned) and
   the `unnamed02-inspect` builder. Every u02 subject (s4 diagnostics
   included) defaults to `sun + kU02SunRig` in `u02_common` — gotcha §12
   closed by construction. Marker orbs are skipped species-wide
   (`ctx.moving_markers=false` for u02); looked at on inspect frames 150/420:
   four coloured pools, zero orbs. Zixxtrixx keeps its orbs
   (zixxtrixx-moving-light CRC-identical).

2. **Eyes read from the side, correctly angled, white ringing the star, star
   inside the lens — PASS by eye.** Aspect 330x92→250x125 at the shipping
   camera; yaw LADDER (0/1200/2400/3600 at front/tq/side under the shipping
   sun) picked 2400 by eye — the side gains a domed lens-with-star read, the
   front keeps two plump eyes; lens domed (kEyeDeepMm 52→90); crown probed
   164 mm proud vs protected ~166; page white repainted as a compact ring
   around the star's contour; star grown (arm 135→185); gaze clamps cut
   (3000/2000) and the containment PROVEN by rendering curious's held
   extreme — star+ring stay inside the lens ink.

3. **Antennae thin, balls thickest, junction hinges exist — PASS.** Mid-tube
   blades 0.7x (max rx 70 mm) vs hinge balls 100 mm and junction knuckles
   110 mm — the inequality holds per station by constants and reads at
   native (form plates). Two knuckle balls at the neck exit and the re-entry
   surface crossing (placed by eye after the first guess read as a wart);
   the lasso rebuild pivots the whole antenna at the neck so the joints
   ACT (Direction 3 §3 "expressive movement there").

4. **Wobble strong/top-down/whole-body; more teardrop; body angles up and
   down — PASS.** `whole_wobble`: the bend starts at the loop peak, travels
   C/A→neck→body (lean + lagged squash), two incommensurate periods
   (46/102-frame class), root PITCH rides the slow wave (hover/drift/rest;
   curious pitches at its target). Teardrop taper to 450 pm + lean 180 mm.
   Trajectories: hover cx/cy p2p 14/13 px with area breathing 2054 px —
   no flat line on any channel of any clip.

5. **Mist see-through again but thicker than v1 — PASS by ladder.** Ambient
   ladder .40/.32/.26/.20 rendered on the shipping hover, judged at native:
   .32 shipped (rim clearly thicker than v1's .40 class, pink glows through
   at every orbit phase; .26 dusky on the away phase, .20 = pass-2 murk).
   Ladder plates kept as owner-facing evidence (run scratchpad impl/M).
   The inspect rig got its own independent ladder → .20 class.

6. **Mana filled, centred, long glitchy decaying smear, strand lightning,
   variants — PASS, judged over four render-and-look iterations.** Filled =
   opaque non-additive cores under halos; centre = ring-hole middle (the
   neck joined the anchor centroid — the pass-2 A/B/C centroid sat 120 mm
   from ball B, the owner's "edge of the circle"); smear = 96x60 persistence
   plane, quantised stepped decay + per-cell jitter + staggered bounded hard
   clear (a smooth exponential fade is unrepresentable), composited as an
   opaque-leaning blend (the solitaire read — additive whitened twice);
   six variants ship (aqua lead / cyan long rung / deep blue / sea-green /
   boil centre / the stack), drip CUT. Lightning: 2-3 continuous strands,
   3 px cores over calm halos, stamps 22 mm, jitter 60 mm (the dot-cloud
   fix), cores through-the-blade, re-hash 7 frames. Gap check at native and
   4x on the three hottest channel frames: continuous branching filaments.
   Final native plate: evidence-strand-native.png (the two hottest frames at
   native — thin connected filaments, no gaps) + evidence-strand-close.png (4x).

7. **Every §7 clip fixed/rebuilt; hasty directional; tricks exist — PASS.**
   Idle 1.5x slower; hasty straight-line (cx p2p 209 px, no circuit); fall
   170 keys / 3.6 m / extra tumble axis; taunt = wind-up + 24-key HOLD +
   wink + smug settle; lasso rebuilt on the junction hinges (owner judges;
   cut was authorised if it fails his eye); drift = banked lateral glide,
   lazy-S, two over-bank corrections; startle sharper (+wider flare);
   curious double-take; THE HEADSTAND (unnamed02-trick): declared probed
   contact, keys 78..156 (+2-key apron), deepest −20 mm vs declared −25
   (accepted −60..−5), float contract holds everywhere else.

8. **Eyes and antennae visibly expressive in every clip — PASS.** Blink
   floor everywhere (staggered offsets); per-clip gaze scripts (idle glance
   schedule, drift look-into-travel + glance-back, hasty panic glance, fall
   wide, curious double-take, trick gaze-down→around→proud); squint on
   effort, widen on fall/startle; junction gestures (lasso neck circling,
   trick neck flex, curious perk).

## Blast radius

* **Zixxtrixx identity:** all 22 subjects re-rendered on the final build and
  CRC-compared against the pass-2 pristine table —
  ALL-IDENTICAL, 22/22 (evidence-bank-identity-pass3.txt — rendered on the
  final tree by this run, compared against the pristine table)..
* **Gate-off (`ZIXX_SUNS=off`) path:** not re-proven this pass; no change
  reaches it (the new sun_rig hook sits inside the u02-only sun_light branch;
  markers change only behind a u02 species check). Stated, not verified.
* **inkmask.py caveat for QA:** on the full-colour RGB565 u02 clips the
  exact-ink mask matches ZERO pixels — an "ink-mask identical" proof on
  these clips is vacuous. trajplot.py (committed) uses row-median background
  subtraction instead.

## Cost, the honest arithmetic (no fragment counters; unit: one full-screen pass = 92,160 pixel-visits)

Reel-side reference renderer, three-conduit stack case:
* belly glow 46 px + core: ~7.8% per conduit.
* pulsar halo (60–90 px breathing, pre-compose): ~19% per conduit at max.
* bullets 7 × (9 px halo + 7 px core): ~3.1% per conduit.
* strands: ~64 stamps/strand × (7 px halo + 3 px core) × 3 ≈ **38% of one
  pass** — the dominant term, and ~47x the plan's 0.8% estimate: the plan
  costed ribbon segments, the reel stamps overlapping discs every 22 mm.
  The FORGE.PRIM ribbon evaluator ask (already on the record) is exactly
  the answer — a hardware ribbon visits each strand pixel once.
* smear plane: decay 5,760 cells (~6%) + full-frame blend composite (one
  pass of cheap blends). The glow_persist hardware ask rides the existing
  POST.COMPOSITE gather, so the machine's marginal cost stays the recorded
  +0.25%; the fat number is reference-renderer-only.
* Mesh: +2 knuckle balls ≈ +280 tris on 1,444 — fill governs, non-issue.
  Bones stay 11 × 8 B = 88 B/frame.

## What is RIGHT (protect this)

The face at the shipping three-quarter and in profile; the .32 mist rung;
the thin-tube/fat-ball antenna with knuckles; the whole-body wobble; the
headstand; the aqua smear's solid glitchy read; the strand filaments; the
one-sun bank with the single four-light inspect.

## Known residuals / for the reviewer

* The taunt's waggle was cranked (380/1900) after looking but the hold is
  carried mostly by the lean+wink; if it still reads mild, the next knob is
  kTauntWagglePm.
* The strands live in the upper half of the pocket (endpoints straddle the
  centre; the pocket egg is tall) — "middle of the ring" reads, but a lower
  bias is one constant if the owner wants it.
* The mana menu bodies partially overlap the neck tube at the shipping
  camera; the smear plane paints over flesh by design (broken framebuffer).
* Lasso: rebuilt, not axed — the owner explicitly allowed the axe if his
  eye says no.
