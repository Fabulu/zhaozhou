# Pass 17 held-punch art diagnosis

**Date:** 2026-09-19
**Scope:** read-only diagnosis of Taunt III f0324 public-carrier attribution and the 53 signed-span bound failures

## Verdict

Two different problems were conflated:

1. **Front/End are red because the new held-punch checker uses the wrong observable for anchored rotational carriers.** The current art is not disproven by the reported `1.18 / 1.69 mm` centroid deltas.
2. **The 53 signed-span failures are real art-budget failures.** Every one is Taunt III F–A extension during the dismissal/hold. The broad upward throw and A's held punctuation are added together, producing `+227..+253 mm` against the existing authored F–A extension limit.

Do not increase F/End authority in response to the centroid numbers, and do not widen the F–A bound. Correct the checker first; then re-author only the held A contribution through a small same-binary visual ladder.

## Why Front and End appear dead to the new checker

### Production consumption is live and not overwritten

`kTaunt3PunchCarrierMm = {90, 140, -180, 160, -90}` is added to the crown output at `tools/reel/manafold_clips.h:4425-4429` and consumed once through `swallow_nodules` at `:4430-4432`.

Inside that shared production path (`manafold_clips.h:1517-1540`):

- Front rotates `kBJunctionF` by `swal[0] * swallow_front_joint_per_mm()`;
- A/B/C write independent target offsets;
- End rotates `kBRearSocket` by `-swal[4] * swallow_end_joint_per_mm()`;
- `PublicJointMute` zeros exactly one input before consumption.

With the selected held array and current gains (`manafold_art.h:2391-2403`), f0324 carries approximately:

- Front: `90 * 24 = 2160` angle16, about 11.9 degrees;
- End: `-(-90 * 20) = +1800` angle16, about 9.9 degrees.

The subsequent `loop_pose` **composes** onto the existing Front/End quaternions (`manafold_clips.h:413-418`); it does not replace them. Call order is therefore correct.

### The checker measures translation at a rotational pivot

The held-punch gate computes `punchline_core_centres` entirely through `visible_core_centroid` (`manafold_spangate.cpp:996-1034`). That is valid for A/B/C, whose public channels translate their rigid visible cores. It is structurally blind to Front/End's intended read:

- Front and End centres are body-attached by contract;
- their public channel is local rotation about that attachment;
- a centred or near-symmetric rigid swell can rotate visibly while its centroid moves only rounding/noise distance.

That is exactly why the already committed public gate distinguishes anchored carriers. `manafold_public_jointgate.cpp:164-213` computes both centroid and **maximum rigid visible-vertex displacement**, then `:303-316` uses max-vertex displacement for anchored F/End and centroid+max for translating A/B/C.

The held gate's `F 1.18 mm / E 1.69 mm` values therefore measure attachment stability, not visual silence. The selected native evidence independently reports that both endpoint mutes alter the held silhouette (`PASS17-TAUNT3-PUNCHLINE-IMPLEMENTATION.md:59-65`).

### Required checker correction

Reuse the public gate's production-visible core-delta law rather than maintaining a second, contradictory metric:

- F/End: gate maximum rigid visible-vertex displacement under named mute;
- A/B/C: gate centroid and maximum displacement;
- retain the fixed 20 mm mechanism floor;
- keep the input guard proving only the named authored input was zeroed;
- rerun attachment, signed-span, ring-order and closure checks in the same invocation;
- require each mute's named criterion to fail, not merely omnibus red.

If the corrected F/End metric and the native normal/mute plate pass, preserve the current `+90 / -90`. Only if either picture or correct anchored metric fails should art authority change.

## Exact 53 bound failures

Source trace: `.tmp/p17-checker-final-fork/mspan-punch-csv.log`, 9,700 shipping key/midpoint rows from the same binary as `mspan-punch-normal.log`.

All 53 failures are:

- clip slot: **21, Taunt III only**;
- span: **F–A only**;
- category: **positive extension only**;
- keys/subframes:

| Authored key | Presentation frame(s) | F–A delta mm | F–A pm | Count |
|---:|---:|---:|---:|---:|
| 150 | 300 / 301 | 227 / 237 | 333 / 348 | 2 |
| 151 | 302 / 303 | 248 / 250 | 364 / 367 | 2 |
| 152–173 | 304–347 | 253 on every key/midpoint | 372 | 44 |
| 174 | 348 / 349 | 253 / 251 | 372 / 369 | 2 |
| 175 | 350 / 351 | 250 / 241 | 367 / 354 | 2 |
| 176 | 352 | 233 | 342 | 1 |
| **Total** | **f0300–f0352** |  |  | **53** |

No A–B, B–C or C–End row fails. No compaction or minimum-run row fails. Posed-ring results remain zero reversed / zero pinched.

## Why the held pose exceeds F–A

The crown table is not responsible. `taunt3_order_pose` returns exact zero from key 144 onward (`manafold_clips.h:1620-1641`), before the first failure at key 150.

The dismissal composes two A translations:

1. broad throw in `manafold_clips.h:4409-4423`:
   - `kTaunt3FlickMm = 132`;
   - lift share `900 pm`, about 118 mm upward at full flick;
   - back/side components remain part of the same target distance;
2. held A punctuation in `:4425-4429`:
   - another +140 mm at full flick.

The target solver sees their vector sum. It reports a maximum F–A centre-distance increase of 253 mm. The authored F–A ceiling remains `+320 pm` (`manafold_art.h:2168`), about +217 mm on its 680 mm centre distance. Widening that ceiling would merely relabel the selected illegal pose and is not a repair.

## Narrow art-path repair

Preserve:

- the broad dismissal (`kTaunt3FlickMm` and lift/back/side shares);
- B/C held punctuation;
- selected front-facing body orientation;
- 1450/750 eyes;
- crown-shuffle tables and witnesses;
- Front/End body attachment;
- all existing span limits.

Make the held A term independently authorable instead of hiding five roles behind array indices. Suggested named knobs:

```cpp
kTaunt3PunchFrontMm
kTaunt3PunchAMm
kTaunt3PunchBMm
kTaunt3PunchCMm
kTaunt3PunchEndMm
```

Add strict same-binary authoring overrides for A, and only diagnostic endpoint overrides if the corrected anchored gate/picture actually needs them:

```text
ZHAO_U02_TAUNT3_PUNCH_A_MM
ZHAO_U02_TAUNT3_PUNCH_FRONT_MM
ZHAO_U02_TAUNT3_PUNCH_END_MM
```

### A ladder

Use the current `140 mm` as the rejected bound control. Test a compact legal-candidate ladder of `60 / 80 / 100 mm` through one binary. The span gate rejects any candidate still outside the fixed limits **before** visual judgment; among candidates with zero breaches, native pictures choose the value. These numbers are candidate art knobs, not a value derived from the 217 mm limit.

The smaller held A term still composes with the broad ~118 mm throw, so the final A pose remains large. It does not reduce the crown shuffle's separate A top/bottom vocabulary.

### Endpoint ladder—conditional only

First run the corrected anchored metric at current `Front +90 / End -90`. If it and the existing native mute sheet are clear, stop and retain them. If not, compare endpoint magnitudes `90 / 120 / 150 mm` at fixed A/B/C/body/eyes. Endpoint increases rotate about attached centres and do not spend F–A/A–B/B–C/C–End signed length.

## Exact evidence loop

For each A candidate that passes structural bounds:

1. build Taunt III through the production clip builder;
2. run normal `mspan --csv` and require:
   - 0 signed-bound/margin breaches;
   - 0 reversed/pinched posed rings;
   - crown witnesses/crossings unchanged;
   - held A/B/C attribution above the existing floor;
3. render complete 368-frame Taunt III from one binary;
4. inspect every-frame sheet, then native/4× f0300–f0352;
5. commit native f0324 normal plus A mute and both eye-size controls;
6. run F/A/B/C/E held mutes through the corrected anchored/translating metrics;
7. keep `mspan`, `msmooth`, `mjointpub`, `mnodule`, `mprobe`, `moutline`, eye and contact gates green.

Required witness frames:

- f0300: arrival begins and first current bound failure;
- f0304: full held shape begins;
- f0324: named held-punchline gate and normal/mute comparison;
- f0344: late hold;
- f0352: final currently failing release sample.

## Stop conditions

Do not accept the held punchline if any of these remains:

- any shipping signed-bound or minimum-run breach;
- any reversed/pinched ring;
- corrected F/End max-vertex mute delta below the public floor;
- A/B/C centroid/max mute delta below the public floor;
- endpoint centre detachment or closure/burial regression;
- crown-shuffle top/bottom/crossing regression;
- visible loss of either eye, O opening, continuous lightning, outline or smoothness;
- a mute gate passing only because unrelated checks also failed.

## Acceptance sentence

The selected held punchline is structurally acceptable only when the real anchored metric—not centroid drift—proves F/End rotation, all five mutes are specifically attributed, and a by-eye-selected legal A rung removes all 53 Taunt III F–A excursions without weakening the already accepted crown ordering or held comic read.
