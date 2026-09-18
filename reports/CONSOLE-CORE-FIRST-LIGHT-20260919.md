# `zhao_console_core` — first connected-machine synthesis

**Analysis & Synthesis: Successful**, 2026-09-19 01:20:39. Source digest
`0d686d14ff7b` over 106 snapshotted files. The Fitter is running; ALMs follow.

This is the first time a CONNECTED Zhaozhou has been through Quartus. Every
previous number came from `zhao_prod_top`, which the completion plan requires be
labelled `RESOURCE_CENSUS_DISCONNECTED` — blocks side by side, LFSR-driven, not
wired to each other.

## The numbers, and the census beside them

| | census (`zhao_prod_top`) | **connected (`zhao_console_core`)** |
|---|---:|---:|
| combinational ALUTs | 1,605,869 | **63,436** |
| registers | 105,881 | **58,138** |
| block memory bits | 1,029,005 | **1,103,702** |
| **DSP blocks** | **297** | **151** |
| virtual pins | 4 | 10,833 |

The ALUT self-check closes exactly — sum of every row's OWN equals the top's
TOTAL, 63,436 both ways.

**Two of those movements mean different things and must not be blurred:**

* **ALUTs 1,605,869 → 63,436 is mostly the ARENA REPAIR**, not composition.
  `zhao_vertex_arena` was 1,545,804 ALUTs of selection network built around a
  bank clear written as 1,089 runtime-indexed writes; it is now 3,735, measured
  matched. That single fix accounts for the overwhelming majority.
* **DSP 297 → 151 IS composition.** The census summed blocks that never coexist;
  the connected machine instantiates what it actually uses. 151 is the first
  honest DSP figure this project has had.

**Memory went UP** (1,029,005 → 1,103,702) and that is the right direction: the
connected machine contains real buffers the census's disconnected blocks did not
wire.

## Where the cost sits, by owner

```
 ALUT_own  ALUT_tot   REG_own       BITS   DSP  module
     3023     37456      2861     465744    63  zhao_shell_top_v2
     2250      2250       687          0     6  zhao_part_update
     2117      2117      2134          0     9  zhao_geom_skin
     1707      1707       487          0    20  zhao_part_collide
     1177      1177      1475      64512    12  zhao_post_composite
      722       722       413       3200     0  zhao_measure_histogram
      467       467       557          0     0  zhao_part_spawn
      430       430       345          0     0  zhao_geom_group_seq
      324       324       665       7360     0  zhao_part_state
        0      7615         0     457380     2  zhao_geom_proj_lane
        0      9135         0     105506    39  zhao_proj_subsystem
```

Nothing pathological. The largest single owner is the shell at 3,023 own ALUTs
and 63 DSP; the particle core totals 4,748 ALUTs and 35 DSP across four blocks.

`zhao_measure_histogram` shows **722 ALUTs and 3,200 memory bits** — the bins
went to RAM as intended, which is the owner's spend-M10K-not-ALMs direction
landing in silicon rather than in a plan.

**The shared projector is composed and working:** `zhao_proj_subsystem` at 39 DSP
with ONE `zhao_project_core`, where the census carried two at 33 each. That
saving is now structural, not a proposal.

## DSP is now the binding constraint, and by a knowable margin

```
151 against the 112-DSP target device  =  1.35x over
```

Against the census's 297 that read as 2.65x and hopeless. 1.35x is an
engineering problem with named owners: the shell's 63, the projector's 39,
PART.COLLIDE's 20, POST.COMPOSITE's 12.

## What this measurement is NOT

Stated before the ALM figure lands, so it cannot be quietly dropped afterwards.

* **112 of ~288 modules.** No lighting at all — the II2 stream exists, is
  verified at 960,086 clocks for the ruled fixture, and is NOT composed here.
  No geometry vertex front end. Terrain only via the wcache riding in with the
  projector. FIELD only one ROM.
* **20 documented tie-offs**, two of which UNDER-COUNT: `part_collisions_applied_o`
  is structurally zero, and synthesis folds PART.UPDATE's step-6 datapath away.
* **10,833 virtual pins.** A large boundary inflates a block's apparent cost and
  its timing; this is not a pin-accurate measurement.
* **No ALMs, no Fmax, no physical M10K count yet** — the Fitter is still running.
* Fitted on the **sizing** device, not the 41,910-ALM target.

So this is a **FLOOR for a connected Zhaozhou**, and it will rise when lighting,
the vertex front end and the descriptor tables arrive. It is not a lower bound on
the optimised machine in either direction (plan §14.1).
