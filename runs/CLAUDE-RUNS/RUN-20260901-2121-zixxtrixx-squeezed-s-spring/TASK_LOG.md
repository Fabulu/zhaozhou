# Task Log: RUN-20260901-2121 — Zixxtrixx squeezed-S spring (Owner Direction 21)

**Created:** 2026-09-01 21:21 UTC+02:00
**Status:** In Progress
**Lane:** isolated clones at `zixxtrixx-wholebody-s-spring-20260901/`, branch
`zixxtrixx-wholebody-s-spring` in both repos. Shared checkouts untouched.

---

## Objective

Correct the rejected Direction 20 spring per Owner Direction 21: whole-body S,
aimed slightly at the target, squeezed from the top (shorter, broader, the same
S), no rotation through vertical, contact flattens and spreads instead of
clipping, explosive forward launch, and the previously accepted salto restored.
See `SPEC_v1.md`.

---

## Progress Timeline

### 2026-09-01 21:21 — Task started; reading

- Read Directions 21, 20, 19, 17; CLAUDE.md art law; previous run
  RUN-20260901-1738 (rejected) TASK_LOG and its full zixxtrixx.h diff.
- Identified the rejected pass's commits: `6510065` (pose/route/wheel rework)
  and `3d91114` (ground declarations); Archive Generation Eleven code state is
  the parent of `6510065` (= `6f60422`).
- Keep from the rejected pass: the `spring_anchor_offset` unit fix, the single
  shared schedule (golden attack + planned consumers), slow arming, seam
  smoothness devices. Reject: the lean-back/rollover pose tables, the reworked
  wheel (taper + breathe).

---


### 2026-09-01 ~22:10 — Authored, probed green, committed a2f601e

**Authoring (by eye, sketchpad -> render -> look -> adjust, three iterations):**

- New pose route in `zixxtrixx.h`: ABSORB (aim toward target + tail starts to
  gather), ASSEMBLED (whole-body S: the idle hook untouched, the tail carrying
  a real rear curl to -80 deg, every station to the last tail segment in the
  S), COLLAPSED (the S squeezed from the top: three stacked near-level runs --
  pressed neck, folded-flat hook return, grounded belly -- joined by quick
  crossings that grow out of the idle dive; nose at ~64% idle height, footprint
  broader, rear curl pressed low). No station makes a gross rotation; the head
  never passes vertical.
- Knots re-timed: aim+gather owns arm 0..400, the squeeze owns 400..1000.
  springpose `schedule`: gather 2-38 mm/key, squeeze peak 94 easing to 18,
  release 280/125/76/35 -- the release fires ~3x faster than any arming key.
- Release control tables generalised to arm->(entry,squash) (constexpr): the
  old derivation clamped entry at 1320/1000 and slammed 400 mm in one key.
- Airborne wheel RESTORED: kSpringAirCoilTaper=0, kSpringAirWobble=0 makes
  spring_air_coil_pitch return exactly (base*curl)/1000 -- bit-identical to
  the accepted uniform wheel. Knobs stay named with rejection provenance.
- Contact: flatten 26%->31%, spread 8%->12% (the squeeze's visible relief);
  fold light-windows opened until the probe counted ZERO real-surface
  intersections at the living hold's wave peaks (waves damped 1150/780 ->
  750/520 -- their peaks were closing the authored windows).
- Support route re-authored for a pose that PRESSES (old one rose onto the
  tail): slight settle, then -26 mm press at the deepest squeeze. Low-entry
  lifts deepened (-12/-10): the release tail and the retimed variant HOVERED
  at 0..+2 mm.
- Landing absorb arm 330 -> 700: the impact cushion now rides into the same
  squeeze (assembled + 40% squash + flatten), per "that's how all the saltos
  and jumps need to look".

**Probe triage (5 fails -> PASS):** flatten envelope band re-derived
(650..730, provenance comment) for the deliberate 31% flatten; intersection,
bite and retimed-contract gates SATISFIED by authoring, not widened.
One stale-binary scare: `build-direct.sh probe cel` takes ONE target (cel won)
and the old probe reprinted old numbers -- the CLAUDE.md tell. Rebuilt probe
explicitly; numbers moved.

**Owner's tail-bones question, verified:** the rig was never the obstacle.
kSpineBones=20 drives all 19 segments; the tables address 15-18; the deform
metadata includes a tail strength ramp; the renders show the rear curling as
authored. The old tables simply authored the rear flat.

**Evidence:** `evidence/iter3-*` (deep strips, salto sheet, landing strip,
motion energy: smooth arming hump, living hold ~0.4, impulsive release,
launch and landing spikes only, smooth decay to rest), against
`sheet-archive-2026-09-01-generation-eleven-*` and `sheet-zixxtrixx-*` (the
rejected live bank).

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

- Website phase: preserve live bank as Archive Generation Twelve, render the
  22 subjects fresh, encode, assemble, merge mains, publish once, verify.

---

## Outcome

**Status:** Complete. Published.

| | |
|---|---|
| zhaozhou main | `3042cc04` (spring rework `a2f601e`) |
| Upheaval main | `9c874e1` + worklog/assembly record |
| review deployment | https://8cc7c9ee.upheaval.pages.dev |
| production | https://upheaval.pages.dev — HTTP 200, byte-identical index, exactly one noindex meta, twelve archive generations |

Production was byte-compared against the local build: `zixxtrixx-jump-one.webm`
and `archive-2026-09-01-generation-twelve-jump-one.webm` are SHA-256 identical
to what was encoded here; the Generation Twelve file exists only in this
deploy, so its presence is independent proof the new build is live.

`ZIXX PROBE: PASS` before and after merging the hardware lane's main (which
touched only reports/RTL/Quartus, nothing the reel builds from), and the final
fresh clean build re-rendered `zixxtrixx-jump-one` and `zixxtrixx-salto-dummy`
to CRCs bit-identical to the judged iteration renders (0x37293039 /
0x86E72EE8), so the shipped media is exactly what the probe passed on and the
eye accepted.

### Direction 21 acceptance, judged by eye on the every-frame sheets

1. Nothing rotates through or past vertical during the spring: the fold grows
   out of the idle dive; the neck lowers, the hook presses flat, the head
   stays nose-forward throughout.
2. The S is whole-body to the last tail segment: the assembled pose carries a
   real rear curl (-80 deg at the tip) and the renders show it.
3. A small aim (nose dip toward the target, ~55 mm forward head travel), then
   the unmistakable 60%-of-the-arming squeeze.
4. The deep pose reads as the same S pressed from the top: ~64% idle height,
   broader footprint, the pressed loop still showing its light window.
5. Zero real-surface self-intersections (committed probe, full and micro, at
   every pre-lift tick including the living hold's wave peaks); contact
   flattens 31% and spreads 12% instead of clipping; every ground bite inside
   the declared band, never hovering.
6. The release fires ~3x faster than any arming key and the launch carries
   forward; motion energy shows exactly two impulses (release+launch, landing).
7. The salto is the previously accepted wheel, restored bit-identically
   (taper/breathe zeroed) and compared frame-by-frame against Archive
   Generation Eleven.

### Not achieved / left open

* The deep squeeze stops at ~64% of idle height. Pressing lower is bounded by
  the stacked tubes' flattened radii; the knobs are the collapsed table and
  kSpringBodyFlattenQ16 if the owner wants it flatter still.
* The aim is deliberately subtle (a few degrees). If the owner wants a more
  theatrical aim, the nose-dip values in the three tables are the knob.
* The historical reel CRC three-way disagreement was not investigated, as
  standing instructions require.
