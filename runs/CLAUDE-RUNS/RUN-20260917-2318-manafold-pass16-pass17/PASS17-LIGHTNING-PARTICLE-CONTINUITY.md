# Manafold Pass 17 lightning and particle continuity

**Date:** 2026-09-19
**Direction:** Owner Direction 18
**Architecture:** `PASS17-SMOOTH-MOTION-ARCHITECTURE.md`

## Production repair

- Free lightning strand IDs and every path-point ID persist. Deterministic path `n` morphs to `n+1` on a clip-periodic C2 clock; endpoints, brightness, surge motes, endpoint energy and centre glint remain continuous.
- Fold-edge jag points use the same persistent C2 path morph. Source/destination topology links cross-fade, the alive edge has a named nonzero presence floor multiplied by the continuous death-life envelope, and the non-strand core now obeys that envelope too.
- Fold shimmer uses stable per-stamp phase and a loop-periodic sine rather than a per-frame gain hash.
- Mean-value coordinates use signed, high-precision Q12 storage with exact sum/range invariants; a committed wrapped-weight mutant reproduces the original tens-of-metres station fault.
- Figure-station identity is explicit across topology changes. HEART uses a named canonical reversed station order rather than resetting IDs after a local pair morph; shifts 14/15 remain a one-binary visual ladder and final choice remains by eye.
- Slow figure rotations/knead clocks use integer-cycle clip-periodic phases, so every loop seam is adjacent rather than resetting an unrelated phase.
- Lasso figure ownership C2-morphs the resolved current figure/stations/topology/shape motes to the ring and back through `shape_mix_pm`; its Q4 transform scale/spin share that envelope and its offset/scale evaluate without key staircasing.
- Particle IDs and roles are fixed for the clip lifetime. Crowd/life/garnish change per-ID visibility continuously instead of integer-truncating/reclassifying the population.
- Particle knead motion is stable loop-periodic motion per ID; the old `frame/2` reseed exists only as an attributed mutant. Release fades knead and drag to the seam state.
- The experimental lab uses the same smooth bolt paths, topology cross-fade and persistent mote jitter. Its private hard-gated tremor is retired; all ten variants use fixed identity/count domains, one C2 envelope, a 720-key timeline with long morph/hold/release phases, a looped C2 root return, and a continuously followed world-hold leash/ring/drag handoff.
- `fx_anchors_from_pose` is the shared anchor extractor used by renderer and continuity gates.
- The stateful fold-coherence authority now C2-hands to the named rest coherence at release/opening and is visibly low-pass filtered; Lasso's coherence floor shares its `shape_mix_pm` handoff instead of switching the whole figure bright in one frame.
- Blue/Green filled bodies, Boil, Pulsar and Stack use clip-periodic wobble/breath clocks with persistent body IDs. Their centres, radius, gain and loop seam are traced through their actual `mana_fill` candidates.
- Fold-edge presence plus navy/shimmer/core layer gains are traced per stable edge at every adjacent frame and through complete cyclic derivatives; the brightness-seam mutant restores the rejected raw-EMA handoff.

## Committed gate packet

`manafold_motiongate.cpp` / `msmooth` evaluates every Manafold clip at all 60 Hz key/midpoint samples, replays frames 0–2 after every looping last frame, and replays three exact final samples after every `hold_last` clip. Seam and parked-state velocity, acceleration and jerk therefore use production reset/persistent-state semantics. Stable IDs separately cover free-strand points, folded stations/motes, attached surge motes, per-edge presence, actual emitted stamp population, per-stamp gains and radius-weighted accumulated layer energy. The exact live Blue/Green/Boil/Pulsar/Stack candidates run through production `mana_fill`, tracing body centres, radii and gains. The gate also checks shape-chain handoffs, morph monotonicity, role/count continuity, exact signed-MVC range/sums and named minimum morph durations. All ten site lab variants run through the same adjacent/seam derivative and per-layer energy trace, including their extra strands, topology identities and up-to-72-mote populations.

Attributed controls:

- `--fail-lightning-switch`
- `--fail-shape-blackout`
- `--fail-particle-reseed`
- `--fail-surge-reseed`
- `--fail-loop-seam`
- `--fail-mote-count`
- `--fail-mote-role`
- `--fail-weight-wrap`
- `--fail-morph-reverse`
- `--fail-brightness-seam`
- `--fail-stamp-count`
- `--fail-final-dwell`
- `--fail-mote-visibility`

Crown/clip controls for antenna snap, accent switching and release identity remain in the carrier gate owned by the choreography packet.

## Verification status

**Overall mechanism verdict: PASS / RC 0.** The original off/reappear, blackout, reseed, count/role-pop, unsigned-weight and non-periodic seam mechanisms are removed. The independent carrier gate is also green.

Fresh direct builds pass for `msmooth` and `zhao-reel-cel` from the final brightness-complete tree.

- reviewed renderer MD5 before checker-only closure: `16A898233C599BA4BB1ACC443B9C1872`;
- checker-complete motion-gate MD5: `60651579CF0508F068F1DFC43C290703`.

The final integration lane is responsible for proving default renderer bytes unchanged or regenerating the visual evidence from the frozen checker-complete source.

The decisive defect was not timing: `fold_mvc` stored a tiny negative mean-value coordinate in `uint16_t`, turning `-4` into `65532`. HEART station 1 therefore sat tens of metres away and made ordinary morphs look like teleports. Signed high-precision Q12 MVC weights now preserve affine unity exactly; normal invariants report zero range/sum failures.

Final full-bank results:

- 50 folded-figure morphs, minimum 16 presentation frames, zero short morphs;
- persistent synthetic mote-noise step: 14.457 mm;
- zero living-edge blackouts;
- lab station step/accel/jerk: 73.79 / 68.26 / 122.57 mm;
- lab mote step/accel/jerk: 126.46 / 138.35 / 270.09 mm;
- lab free-strand step/accel/jerk: 125.46 / 21.32 / 13.88 mm;
- lab surge-mote step/accel/jerk: 171.07 / 61.33 / 34.96 mm;
- lab loop seam maximum: 130.33 mm; edge-presence step/accel/jerk 10/3/6 pm; emitted stamp and accumulated energy derivatives 4/4/8; zero blackouts/count/role changes;
- shipping free-lightning step/accel/jerk: 187.69 / 183.76 / 188.97 mm;
- shipping fold-station step/accel/jerk: 425.02 / 431.42 / 857.73 mm;
- shipping fold-particle step/accel/jerk: 237.27 / 182.83 / 188.24 mm;
- shipping surge-mote step/accel/jerk: 225.28 / 190.04 / 191.06 mm;
- fold-edge presence and per-stamp gain step/accel/jerk: 166 / 82 / 85 pm;
- emitted fold stamp population step/accel/jerk: 24 / 24 / 48, with radius-weighted accumulated energy 21 / 21 / 39 pm;
- free-strand stamp population remains invariant (0 / 0 / 0 derivatives), with accumulated energy 1 / 1 / 2 pm;
- nine repeated-final held-state samples are included in the same histories;
- loop seam maximum: 225.28 mm across 1,560 folded/free/mote entity samples;
- shipping mana-body maxima across Pulsar, Blue, Green, Boil and Stack: position 37.95 / 17.33 / 17.57 mm, seam 34.22 mm; radius 7 / 3 / 4 px; zero count/role changes;
- zero blackouts, role/count changes, shape-chain breaks or morph reversals.

The acceleration/jerk bands were named only after native every-frame/badness review and retain structural headroom: free `250/250/300`, fold station `500/600/1200`, fold mote `420/300/350`, surge mote `420/300/350` mm for step/accel/jerk. Edge presence and per-layer gains use `300/300/500 pm`; shipping body position uses `80/80/120 mm` and Pulsar radius uses `12/8/10 px`.

Complete CLI controls, all thirteen invoked and returning RC 1 with their attributed detectors:

- `--fail-lightning-switch`
- `--fail-shape-blackout`
- `--fail-particle-reseed`
- `--fail-surge-reseed`
- `--fail-loop-seam`
- `--fail-mote-count`
- `--fail-mote-role`
- `--fail-weight-wrap`
- `--fail-morph-reverse`
- `--fail-brightness-seam`
- `--fail-stamp-count`
- `--fail-final-dwell`
- `--fail-mote-visibility`
- `--star-heart-shift N` remains the strict 0..17 correspondence ladder.

## Every-frame visual evidence

The final brightness-complete binary (`16A89823…`) regenerated every presentation frame for Channel (420) plus the fixed-idle Blue, Green, Boil and Stack subjects (600 each). All 2,820 raw frames are byte-identical to the generation reviewed tile-by-tile before the lab-copy-only rebuild. Channel's exact f0414–f0419→f0000–f0005 native and exact 4× sequences show geometry and white/cyan/navy layer energy carrying continuously through the loop. The four mana-menu sheets show smooth persistent body motion and breathing with no final→first reset:

- `PASS17-BRIGHTNESS-FINAL-CHANNEL-ALLFRAMES.png`
- `PASS17-BRIGHTNESS-FINAL-MANA-BLUE-ALLFRAMES.png`
- `PASS17-BRIGHTNESS-FINAL-MANA-GREEN-ALLFRAMES.png`
- `PASS17-BRIGHTNESS-FINAL-MANA-BOIL-ALLFRAMES.png`
- `PASS17-BRIGHTNESS-FINAL-MANA-STACK-ALLFRAMES.png`
- `PASS17-BRIGHTNESS-SEAM-CHANNEL-NATIVE.png`
- `PASS17-BRIGHTNESS-SEAM-CHANNEL-4X.png`

The earlier `F9404CA5…` contact sheets remain valid history for signed MVC, identity, topology and particle mechanisms, but predate the brightness-seam and shipping-candidate trace closure:

- `PASS17-SMOOTH-FINAL-MANAFOLD-CHANNEL-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-PIROUETTE-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-DAMAGE-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-STARTLE-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-TAUNT3-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-CRACKLE-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-TRICK-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-LASSO-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANAFOLD-DEATH-GUTTER-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-MANALAB-HELD-STILL-ALLFRAMES.png`
- `PASS17-SMOOTH-FINAL-BADNESS-NATIVE.png`
- `PASS17-SMOOTH-FINAL-BADNESS-4X.png`
- `PASS17-SMOOTH-LAB-HELD-BADNESS-NATIVE.png`

Every tile was reviewed at native contact-sheet scale. The figure remains continuously present through shape handoffs; particles and free strands follow persistent paths without alternate-frame reseed; Trick and Death no longer show the giant wrapped-weight excursions. The actual Lasso and Trick acceleration/jerk maxima and the held-lab handoff were then inspected as adjacent native sequences and exact 4× crops; all read as smooth, deliberate motion. Older shift-14/15 and blocker plates predate the signed MVC/periodic-clock fix and are history only.

## Final checker-audit closure

A later independent audit found three remaining instrument/source gaps. They are closed in checker generation `F558ECE81519B2F1DB2746A4E0D81480`:

- fold and surge mote visibility is now a first-class per-ID scalar history at every adjacent frame, loop seam and held-final dwell; normal shipping visibility step/acceleration/jerk is `8/8/16 pm` for fold motes and `0/0/0 pm` for surge motes, while all lab visibility histories are `0/0/0 pm`;
- `--fail-mote-visibility` changes rendered visibility only—IDs, roles and positions stay fixed—and independently drives both fold/surge plus lab visibility histories beyond the reviewed `300/300/500 pm` bands;
- all thirteen effect mutants now use an expected/allowed failure-category contract. The named category must fire and unrelated categories make attribution fail; documented secondary categories are only those causally downstream of the mutated production operand;
- the lab coherence handoff consumes `lab_phase().amp_pm` directly. The gather/release envelope is C2-eased exactly once rather than compressing its authority transition through a second ease.

Fresh normal `msmooth` is RC 0. All thirteen controls return RC 1 and print `attributed detector fired` with exact observed/expected/allowed masks: the prior twelve plus `--fail-mote-visibility`. `git diff --check` is clean. These checker-only changes postdate the brightness-complete render, and the lab single-ease correction changes production output; therefore the prior contact sheets remain decision/provenance history, not final checker-complete visual evidence. One clean post-art integration render must regenerate the affected evidence.

## Palette-complete amendment

Final combined-source review found one more visible state outside those thirteen
controls: the Boil Blue/Violet CLUT used raw frame time, reset rotation `10 -> 0`
at its 600-frame seam, and was absent from `msmooth`. The repaired production
ramp is clip-periodic and quintic-C2 between byte-exact authored entries. The
palette gate reads every production CLUT entry plus actual bloom-histogram-
weighted emitted energy. Focused native 1/2/3-cycle review selected one broad
revolution; faster rungs read as repeated palette flicker.

Palette-complete `msmooth` MD5 `E1DD063275637C1F4FC2D5525D7482AB`
returns RC 0. The two additional strictly attributed controls
`--fail-palette-raw-clock` and `--fail-palette-hard-switch` return RC 1, bringing
the current effect-control total to **15/15**. Exact metrics, final renderer hash
and focused pictures are in `PASS17-BOIL-PALETTE-CONTINUITY.md`. All earlier
13-control text above is checker history; one clean combined integration build
remains the final receipt.

## Acceptance boundary

Direction 18's mechanism and focused visual gate are green. Final Pass-17 acceptance still requires the exact complete bank to re-run both `mspan` (31 attributed mutants) and `msmooth` (12 attributed mutants) from its final renderer, followed by isolated every-frame review; smooth motion does not by itself accept unfinished Trick identity, eye acting or Taunt-III punchline art.
