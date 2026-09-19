# The true total: `zhao_console_core` fitted at 47,582 ALM

**Fitter Status: Successful**, 2026-09-19 01:55:23, 2,382 s. Device
`5CEBA9F31C7` (sizing). Source digest `0d686d14ff7b` over 106 snapshotted files.

This is the first fitted number for a CONNECTED Zhaozhou. Every previous figure
came from `zhao_prod_top`, which the completion plan requires be labelled
`RESOURCE_CENSUS_DISCONNECTED`.

## The receipt

```
Logic utilization (in ALMs) : 47,582 / 113,560 ( 42 % )
  * Logic utilization warning : A large number of ALMs contain virtual pins
    and count towards logic utilization
  * ALMs containing virtual pins : 5,417 / 113,560 ( 5 % )
Total registers             : 56,031
Total pins                  : 0 / 480
Total virtual pins          : 10,833
Total block memory bits     : 1,103,456 / 12,492,800 ( 9 % )
Total RAM Blocks            : 306 / 1,220 ( 25 % )
Total DSP Blocks            : 151 / 342 ( 44 % )
```

## Against the real target, 5CSEBA6U23I7

| | fitted | target device | over by |
|---|---:|---:|---:|
| **ALM** | **47,582** | 41,910 | **1.135x** |
| **DSP** | **151** | 112 | **1.35x** |
| **M10K** | **306** | 553 | **55% used — FITS** |

**DSP is the binding constraint, not ALMs.** That inverts the assumption this
campaign has run on all along, including the standing direction that memory is
the only weapon. Memory is not the problem — 306 of 553 blocks, with room to
spare. The two problems are 13.5% too much logic and 35% too many multipliers.

## The virtual-pin caveat, which Quartus raised itself

**5,417 of the 47,582 ALMs contain virtual pins.** Quartus prints its own
warning that these count toward logic utilisation. With 10,833 virtual pins on a
block-level fit, a meaningful fraction of the ALM total is boundary, not
machine.

**Do not subtract 5,417 and call it 42,165.** An ALM holding a virtual pin can
also hold real logic, so the true figure is somewhere between 42,165 and 47,582
and nobody here knows where. What is defensible: **the boundary inflates this
number, and a pin-accurate or composed measurement will read lower.** The
honest statement of the ALM position is "47,582 fitted, with a known upward bias
of up to 5,417".

The DSP and RAM-block counts carry no such caveat — those are hard resources and
a virtual pin does not consume one.

## What this machine is, and what it is not

**112 of ~288 modules.** It contains: the V2 shell (CMD, MEM, VIDEO, INPUT,
AUDIO, DEBUG, binner->raster->texture->fbwrite), the complete particle ring
(STATE->UPDATE->COLLIDE->STATE, SPAWN->STATE), the geometry client closed onto
the SHARED projector for the first time, the compositor, and the histogram.

**It does NOT contain:**

* **any lighting at all** — the II2 stream is built and verified at 960,086
  clocks for the ruled fixture, and is not composed here;
* the geometry vertex front end (SETUP, CLIP, DEPTHQUANT, MESHFETCH, VDECODE);
* terrain beyond the wcache that rides in with the projector;
* FIELD beyond one ROM;
* the particle species and curve tables, which **no RTL implements anywhere**.

And **20 documented tie-offs**, two of which make this an UNDER-count:
`part_collisions_applied_o` is structurally zero, and synthesis folds
PART.UPDATE's step-6 datapath away.

> **CORRECTED 2026-09-19, LATER THE SAME DAY.** The paragraph above describes
> the tree the 47,582 was measured from, and is left intact because the number
> belongs to that tree. It is no longer true of the tree. Owner ruling
> `RULING-I4-COLLISION-SPAWN-20260919.md` RETIRED PART.UPDATE's four `col_*_i`
> ports and the counter that watched them: there is no folded step-6 datapath
> and no tie-off on it, `part_collisions_applied_o` is driven by PART.COLLIDE
> and moves, and the count is **19 tie-offs**. That correction moves PART.UPDATE
> DOWN, not up — it is the one place this report's "FLOOR" reading does not
> apply, and the next fit should be expected to shrink there.

**So 47,582 is a FLOOR.** It rises when lighting, the front end and the
descriptor tables arrive. It is not a lower bound on the optimised machine in
either direction (plan §14.1).

## How the number got here

| | ALUTs | DSP |
|---|---:|---:|
| census `zhao_prod_top`, 2026-09-18 | 1,605,869 | 297 |
| after the `zhao_vertex_arena` repair | — | — |
| connected `zhao_console_core` | **63,436** | **151** |

Two distinct causes, which must not be blurred:

* the ALUT collapse is **the arena repair** — one `always_ff` expressing a bank
  clear as 1,089 runtime-indexed writes into a packed vector, 1,545,804 ALUTs
  reduced to 3,735 with registers and memory bits identical either side;
* the DSP drop 297 -> 151 is **composition** — the census summed blocks that
  never coexist, and the shared projector now carries ONE `zhao_project_core`
  where the census carried two at 33 DSP each.

## What to attack, in order

1. **DSP, 151 -> 112.** Needs 39 removed. Named owners from the hierarchy:
   shell 63, `zhao_proj_subsystem` 39, PART.COLLIDE 20, POST.COMPOSITE 12.
   PART.COLLIDE's quarter-square ROM candidate (~10 DSP for ~100 M10K) was
   declined on a premise that has since been struck, and memory is now known to
   have room — that candidate should be re-decided first.
2. **ALM, ~47.6k -> 41.9k.** But measure a composed or pin-accurate fit before
   spending effort, because up to 5,417 of the gap is boundary.
3. **Only then** the optimisation queue — and note that both numbers will move
   UP as the missing organs arrive, so this ordering is provisional.
