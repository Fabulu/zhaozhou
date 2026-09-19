# Manafold version 18 — Wave D smaller swells and public Front flex

> **SUPERSEDED 2026-09-19 by `V18-SWELL-FRONT-REPAIR.md`.** This report describes the pre-review tree (final taper key `0/0`, Trick root `1690`, duplicate Front API, default-presentation sheets), which the independent review BLOCKED. Its selections (400 pm swells, 1500 pm Front gain) stand; its build, gate, CRC and picture receipts do not.

**Date:** 2026-09-19
**Direction:** Owner Direction 19
**Source base:** root/material acceptance `6f0201e3`
**Verdict:** **PASS — five smaller continuous swells and real public JunctionF X/Y performances selected by complete motion**

## Swell ladder and selection

The existing strict `ZHAO_U02_SWELL_PM` diagnostic rendered complete Hover at `1000 / 850 / 700 / 550 / 400 / 250 pm`, then complete Taunt III at `1000 / 700 / 550 / 400 pm`. The ladder changed only the five swell additions; stick taper, stations, profile half-widths, topology, weights and signed-span laws remained fixed.

**Selected by eye: the 400 pm family.**

- `1000` retains the version-17 protruding-bead read;
- `700` and `550` improve it but the free A/B/C stations still dominate the sticks;
- `400` keeps all five stations plainly thicker than adjacent runs while making them read as local thickening in one continuous antenna;
- `250` approaches a nearly uniform strap at several side/rear angles and was rejected.

Shipping keeps independent named values, equal to the integer output of the selected 400 pm rung:

| Carrier | Selected rx/rz mm | Exact version-17 control rx/rz mm |
|---|---:|---:|
| Front | `10 / 12` | `26 / 31` |
| A | `26 / 34` | `64 / 86` |
| B | `25 / 33` | `62 / 82` |
| C | `24 / 31` | `60 / 78` |
| End | `20 / 25` | `50 / 62` |

`ZHAO_U02_SWELL_MODE=selected|legacy` is strict and selects those two named families before type construction. The existing `ZHAO_U02_SWELL_PM=0..1000` remains an independent diagnostic multiplier applied after the family choice. `--fail-swell-size` restores the legacy family in the real model, fails only the attributed swell-size category, and leaves taper/topology unchanged.

## Public Front/JunctionF art

`HingePlay::tilt_front/yaw_front` now carries real, clip-specific C2 performances rather than a generic oscillator. Timings live in per-mille of clip progress and return exactly to zero at both ends. Public curves are authored for:

- Hover and fixed idle;
- Rest;
- Drift and Blown;
- Hasty and Flight;
- Taunt and Taunt III;
- Trick approach/righting.

The selected global gain is the named `1500 pm` owner knob. Each clip still owns its own editable X/Y key table. Front acts on JunctionF after the existing fold-Z authority; Neck, A/B/C/End and signed length remain independent.

Trick required a specific contact correction: version-18 Front art is C2-released to zero before the planted interval and resumes only during righting. The accepted plant therefore keeps its physical antenna support. `kTrickPlantRootMm` moved `1706 -> 1690` after the committed 3D probe found the smaller selected swell reduced actual penetration from the declared `-25 mm` to `-9 mm`; the named root-height knob restores exact `-25 mm` contact without image-space inference.

`ZHAO_U02_FRONT_FLEX=normal|mute` strictly removes only the version-18 JunctionF X/Y curves. `ZHAO_U02_FRONT_FLEX_GAIN_PM=0..2000` is the strict one-binary authoring ladder. Ten named public clips clear the unchanged 20 mm visible-core floor; weakest maximum response is Rest at `24.37 mm`. The mute changes only JunctionF local quaternions and returns every clip seam to identity.

## Hidden ReturnTip blocker and repair

The larger Front envelope exposed a previously hidden structural dependency: the version-17 terminal rear ring could leave the body even though the visible RearSocket and all signed spans remained correct. No burial gate moved.

The repair is confined to the deliberately invisible ReturnTip-only terminal cap:

- final taper key `42/26 -> 0/0`, without changing any visible stick key;
- tail centre remains on the exact straight C-End continuation at a named `200 mm` burial;
- key/midpoint target translation is stored at Q16 sub-millimetre precision instead of truncating to whole millimetres.

Normal receipts: 97,000 terminal posed samples, worst ellipsoid rho `1105.43 pm` under the unchanged `1120 pm` gate; rear endpoint `5.664 mm`, tip-length error `0.510 mm`, straightness sine `0.007673`; 339,500 ring steps with zero reversal/pinch and minimum projection/separation `5.496 / 7.110 mm`. The visible RearSocket, C-End signed gradient and body target did not move.

## Checker coverage

`mspan` adds independent `kCatFrontFlex` and `kCatSwellSize` categories.

- Public Front gate rebuilds normal/muted production clips, verifies the compiled bank matches the selected mode, requires X/Y motion in ten clips, compares visible Front-core response and proves no direct local-quaternion collateral.
- Swell gate calls the production `make_loop()` under selected, legacy and zero-swell modes, proving selected/legacy geometry differs, selected swell remains live and actual topology/weights use the selected family.
- `--fail-front-flex` and `--fail-swell-size` are attributed RC 1 controls.
- Existing causal masks were expanded only where an inherited structural mutant now also reaches the independently clocked root detector; all 34 span/root/crown/continuity controls remain attributed.

The effect gate was also made structurally independent: mote positions are sampled independently of rendered visibility while `life_pm == 0` remains the explicit absent-state boundary. The visibility-only positive control now fires only visibility (`0x80000`) instead of changing which position samples exist. Normal `msmooth` and all 16 attributed controls are green.

## Build and gate receipts

Fresh direct output: `.tmp/v18-waveD-accepted`.

- renderer MD5 `18D3D94F0D0C11C12A17EBA9934186FB`, SHA-256 `8176514DF67C8B009E6A8C4356FA2785F4A3126F94440D4DD582E239B8E4447E`;
- `mspan` MD5 `D8E4453AD6DEF185EE5F2AFA83735404`;
- final `msmooth` MD5 `1980EBB75F1901ED3139B1B8C5B9274C`;
- direct clean renderer plus `mspan`, public-joint, nodule, probe, mesh, outline, shell, QA, eye-size and effect targets build;
- all protected normal gates RC 0;
- `mspan` 34/34 controls attributed RC 1;
- `msmooth` 16/16 controls attributed RC 1;
- public-joint and nodule F/A/B/C/E red legs green by their inverted convention;
- probe scale/outline/mirror and outline owner/repaint controls fire;
- QA eye/Startle/Fall/lane/seam/root controls and eye-size L/R/wrong-bone legs pass;
- nine malformed/wrong-case/trailing/out-of-range swell/front selectors return RC 2;
- unrelated Zixxtrixx Idle remains `576/576` byte-identical.

The touched registered CMake targets had already configured and built successfully without cleaning. A later repository-wide `cmake --build ... --clean-first` did **not** provide a final clean-CMake receipt: regenerating `build.ninja` expanded into 427 generated dependencies and failed at step 239 because numerous Verilator-produced `*.cmake` copy sources did not exist (the documented stale generated-graph failure mode). The command returned RC 1, not a timeout-success, and left no lane process running. This is recorded as an infrastructure failure rather than represented as source acceptance; every touched native target was independently clean-built directly and its executed binary is identified above.

## Exact rendered selection

One accepted binary rendered selected, Front-mute and legacy-swell versions of nine complete subjects: Hover, Rest, Flight, Hasty, Drift, Blown, Taunt, Taunt III and Trick. Each set contains **3,232 contiguous frames**. Selected sequence CRC32C:

| Subject | Frames | Selected | Front mute | Legacy swell |
|---|---:|---|---|---|
| Hover | 600 | `0x47CD2048` | `0x6D03643E` | `0xAE68B25D` |
| Rest | 400 | `0x638453DB` | `0xB63CA5B4` | `0x80B8174B` |
| Flight | 352 | `0x5B1A69C6` | `0xC6FFB27C` | `0xFA44B1EC` |
| Hasty | 240 | `0x6E2A04E3` | `0xCCD1FC88` | `0xFA25EF59` |
| Drift | 300 | `0x1DAF702C` | `0x2808C374` | `0x0342D095` |
| Blown | 292 | `0x143CBE0C` | `0xC3BDDEBB` | `0xB5C43AFF` |
| Taunt | 280 | `0x74F68E76` | `0x28A123F7` | `0x2FEF74E9` |
| Taunt III | 368 | `0x7853B5C4` | `0x520203A0` | `0x0202744A` |
| Trick | 400 | `0x84A6528B` | `0xB6C6F4EF` | `0xBA2F1928` |

Every selected frame was reviewed. The smaller swells remain readable under large crown travel but no longer dominate as beads; Front visibly changes the connection angle on both axes; body-integrated root material remains smooth; no root shear, kink, threaded-sphere waist, outline hole, span pinch, socket exposure, eye/contact or effect regression appears. Hasty/Drift/Blown still show the separately scoped persistent history mist, Flight still needs its version-18 vertical phrase, and Trick still awaits Wave F’s planted full turn; Wave D does not claim those later repairs.

## Durable evidence

- `V18-SWELL-LADDER-HOVER-2X.png`;
- `V18-SWELL-SELECTED-VS-LEGACY-2X.png`;
- `V18-FRONT-FLEX-AB-{LIFE,MOTION,ACTING}-2X.png`;
- `V18-SWELL-FRONT-MANAFOLD_{HOVER,REST,FLIGHT,HASTY,DRIFT,BLOWN,TAUNT,TAUNT3,TRICK}-ALLFRAMES.png`.

Raw frames, intermediate gain/radius rungs, stale pre-contact sheets and exploratory crops remain under `.tmp` and must not be staged.

## Modified production paths

- `tools/reel/manafold_art.h`;
- `tools/reel/manafold_model.h`;
- `tools/reel/manafold_clips.h`;
- `tools/reel/manafold_spangate.cpp`;
- `tools/reel/manafold_motiongate.cpp`;
- `tools/reel/zhao_reel.cpp`.

No root helper/material/page/ink, live mist/mana/particle layer, Flight cadence/amplitude, Trick spin, site/media or RTL source changed.
