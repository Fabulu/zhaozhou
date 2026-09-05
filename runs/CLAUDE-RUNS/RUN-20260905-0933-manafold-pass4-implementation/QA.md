# QA — Manafold pass 4, the implementer's own gate (Direction 4's ten acceptance items)

**Run:** RUN-20260905-0933-manafold-pass4-implementation · 2026-09-05
**Judged on:** shipping subjects only, final build, ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross, native 384x240 first, close-ups after.
Worked against `Upheaval/creature/10-GATE-CHECKLIST.md` top to bottom; item 6
(can the check fail?) applied to every instrument cited — inkmask.py,
trajplot.py and inkwidth.py all carry committed selftests that FAIL on
known-bad input, and the two pass-3 vacuous instruments were shown failing
on the real fault inputs before anything rested on them.

## The ten items

1. **The creature is called Manafold and the folder is renamed — PASS.**
   `git mv` in both repos (pure move commit, then reference commits); grep
   finds no live `Unnamed02` outside owner-direction files, archives and
   history narration; `u02::`/`kU02*`/`U02_*`/`u02-s*` kept as creature-02
   shorthand, documented atop manafold_art.h (plan R13). Rename proven
   inert: zixxtrixx-walk CRC identical, all 600 hover frames byte-identical
   across the rename.

2. **It folds mana into recognisable shapes and kneads them into new ones,
   continuously, at a distance — PASS, with the honest caveat below.**
   Mechanism: stencils at fixed integer mean-value-coordinate weights over
   the six posed antenna anchors (junctionF/neck/A/B/C/junctionB) — the
   shape folds because the rig folds, BY CONSTRUCTION; grip (pocket area),
   knead (anchor-speed excess over a slow baseline) and lagged drag are
   pure joint-state functions; no collision, no proximity test exists in
   the code (R1 honoured structurally). The always-on antenna_knead layer
   runs gather-hold-knead-release in every clip with hashed 07-band
   durations, per-clip opener and per-clip gain.
   **THE ABLATION GATE PASSES** — the check that could fail: with
   U02_ABLATE_KNEAD=1 the mana visibly goes limp (loose tall cloud,
   ignoring the antenna) while the knead-on render is gathered, shaped and
   worked (evidence/stageFOLD-ablation-pair.png). The causality strip
   (12 consecutive knead frames) shows the joints working and the mass
   churning and following, nothing touching.
   **Caveat, said loudly:** at native the naming test is honest-but-mixed.
   RING and CRESCENT are nameable; STAR (spokes) and TRIANGLE read as
   angular clumps at native and become nameable at 2x; BAR/S-CURL land
   between. The pocket is ~25x45 px at the house camera and a soft-blob
   stroke has a floor. Five author-render-look iterations are logged with
   what each changed; every value is a named knob (kStencil*, kMote*,
   kGripGamma, kKneadClipPm[15], kStencilFaceYawA16...), so the owner can
   push legibility further by eye. Deliberate compromise: the plan's
   kStencilFacePm fallback is ON as an authored stencil-plane yaw (-27
   degrees; the sign was flipped after a render was looked at).

3. **Tumour balls gone; a ball at back and front junctions; every ball has
   a bone — PASS.** kBJunctionF is a real hinge (the old neck bind — every
   accepted pivot preserved verbatim); its ball sits at the PROBED surface
   crossing (83,735); the back ball re-sited to the probed crossing
   (-328,467); the tumour removed. kBLoopBase2 carries authored rotation
   in the knead layer (the back ball slides on the surface). 12 bones of 32.

4. **Antenna thin, thickening only at the junctions — PASS.** Taper redrawn
   to 8 stations: free tube rx 60-66, junction station 78 with the flare
   confined beside the ball, back end 82; balls (118) are the thickest
   points per station and READ as balls at native (stageB plates).

5. **More, bigger particles, smoother rotation, fewer lightning lines,
   glitchier smear, odd drifters — PASS.** 24 motes/conduit (was 7), halos
   7-10 px with opaque hearts (pass-3 bullets were 9 px halos over 2 px
   cores); each mote one consistent angular velocity, periods 130-260
   frames, the frequency-doubling term gone (R7). ONE strand + 5 surge
   motes + endpoint bursts, brightness floor judged at the MEDIAN frame
   (channel plates: one hot filament through energised particles). Smear:
   clips ship the LONG/GLITCHIER rung with the row tear; a fifth
   BROKEN-BUFFER rung carries the cyan variant. 3 wander motes leave the
   pocket on slow odd walks and decay through the smear.

6. **The smear is depth-correct — PASS.** Per-cell nearest-splat depth,
   glow_splat's own comparison at cell granularity; before/after pair
   shows the creature occluding its trail where pass 3 drew on top; the
   4-px cell edge is declared part of the broken-framebuffer read.
   Hardware asks amended (glow_persist needs persisted per-cell depth).

7. **Eyes: separated teardrops, outward, tracking whites, experiments
   shown — PASS.** X1 (all-polygon teardrop, per-ring width profile +
   apex sharpening, ~176 tris/eye) SHIPPED; X2 rendered beside it
   (U02_EYE=x2 keeps the almond); X3 refused on source lines (vertex UVs
   bake at compile — zref_creature.hpp:393; Tmu::Mode has no UV offset —
   zref_texture.hpp:129-142): a page mechanically cannot track. The white
   is a polygon annulus torus riding the PUPIL bone — whites trace pupils
   BY CONSTRUCTION, shown travelling with the star at the curious clip's
   full gaze extreme with the star inside the lens (stageE plates). Star
   arms are per-axis (150/88) and FIT the 125 half-width (the 185-vs-125
   impossibility ends); containment arithmetic documented at the clamps;
   separation +25 mm (no touching at the top — the pink channel shows);
   experiment artefacts in creature/Manafold/media/eye-experiments/ and on
   the site's Experiments row.

8. **Interior glow gone; outer layer more see-through — PASS.** Belly glow
   OFF behind kBellyGlowGainPm=0 (the revert path; machinery intact, the
   mana uses it); no dark hole on any looked-at clip. Mist one rung up to
   the .36 class from a rendered .32/.36/.40 ladder (the rim survives at
   .36 and flattens at .40); the inspect orbit takes the A26 rung (murk
   lifted, pools intact).

9. **The outline question — ANSWERED WITH MEASUREMENTS.** The cel ink is
   one shared screen-space post pass (cel_main_ink_width over projected
   radius, the same code path for both creatures). Measured with the
   committed inkwidth.py (selftest: 2px/5px rings measure 2.0/5.7,
   dilation detected, vacuous input refused): median perpendicular width
   is **2.0 px for BOTH creatures at every distance tried** (zixx walk at
   cam_k 310000; manafold at 240000 / 185000 / 170000). Zixxtrixx shows a
   p90 of 2.8 vs Manafold's 2.0 — the shared width law widens ink as the
   creature fills more of the frame, and the walk camera is the nearest;
   it is the distance-adaptive design, not a per-creature difference. So:
   yes — same thickness law, same class at matched distance.

10. **Directional hits exist — PASS (not cut).** manafold-damage: four
    authored contact stations (body-front/side/back, LOOP-PEAK), airborne
    recoil (displacement + overshoot + damped settle, no ground brace),
    antenna whip 2 keys late, eye wince toward the blow, mana shatter via
    the coupling with zero per-clip mana authoring. Probed and looked at
    per direction.

## The instruments (checklist A)

- inkmask.py: quantised triples, vacuous = FAIL, selftest committed.
  Found 446k real ink px where the old tool found zero.
- trajplot.py: masks against a creature-free render (ZIXX_HIDE_CREATURE=1,
  proven byte-inert when unset); legacy fallback SAYS SO and is not QA
  evidence; reproduced the reviewer's 2034-px horizon on fall 0000 and
  reports 0 with the plate. A temporal-median plate was tried and REJECTED
  by its own selftest (a hovering creature erases itself).
- inkwidth.py: above.
- manafold-probe: clearance + headstand contact + closure (rim gate
  re-derived 1060 to 1120 after the knead layer legitimately moved the
  arm — worst key rendered and LOOKED at, no visible breakout) + the NEW
  travelling-column check (terrain re-queried along each travelling clip's
  own root path; drift/hasty stage flat per the walk precedent, rise 0 mm).
  The probe CAUGHT two real regressions during the pass (knead lifting the
  headstand off its contact; closure rim past its gate) — it can fail, and
  did.

## Multi-conduit cost (Stage MN gate)

Arithmetic (no counters exist — gotcha section 5): per conduit ~16
crowd-scaled motes x (pi*8.5^2 halo + pi*4.9^2 core) ~ 4.8k px ~ 5.2% of a
92,160-px pass; strand + surge ~ 12%; x3 conduits (trio: fold+strand each)
~ 52-55%, plus the conduit-count-independent smear (~6% decay + one
blended composite). Call it ~0.6 full-screen passes ~ 3-3.5% of a frame's
clock at the placeholder 100 MHz. Observed wall time: the six-subject
batch averaged 134 ms/frame against pass-3 hover's 126 (wall clock stays
non-evidence per the review; recorded as consistency only). The trio plate
shows three conduits each folding (evidence/stageMN-trio-and-cyan-rung.png);
kMoteCrowdPm=700 is the named relief valve, engaged when >1 conduit.

## Zixxtrixx identity and the gate-off path

Recorded in evidence/stageQ-zixx-identity.txt (all 22 sequence CRCs from a
final-build render against the pass-3 shipped table, byte-wise diff on the
5-risk set) and evidence/stageQ-gateoff-identity.txt (ZIXX_SUNS=off on the
pristine baseline vs the final build, byte-wise). Filled in at Stage Q
close; nothing here is claimed until those files carry the numbers.

## Deliberate deviations, said out loud

- Stage B's junction gesture ladder folded into FOLD's iterations (the
  knead layer IS the gesture vocabulary; its amplitudes were tuned against
  the closure/contact probes and looked at).
- kKneadClipPm[trick]=0: the probe showed the grip lifting the planted
  loop peak out of its declared contact; the headstand's own balance flex
  still feeds the mana coupling.
- kPocketBoundPm is enforced structurally by the MVC inward clamp rather
  than as a separate scale; the constant documents the law.
- The smear before/after pair also carries the Stage-B geometry delta (the
  mechanism proof is the code path: the splat's own depth test).
- Fold shapes at native: see item 2's caveat.
- The trio carries one strand PER conduit (candidate 9 on all three);
  kStrandCount stays the owner's knob.
