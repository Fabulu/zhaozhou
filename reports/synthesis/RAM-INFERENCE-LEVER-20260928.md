> ## CORRECTION, SAME DAY, BEFORE ANYONE ACTS ON THIS
>
> **The static scan OVER-PREDICTS, and this note originally leaned on it as the
> lever. Measured against the map, it is wrong more often than it is right.**
>
> `--rank` (which already existed -- I proposed building it before finding it)
> classifies 24 arrays as `[MECHANICAL]`, the generate-for-loop killer with a
> known remedy. Seven of those are actually ELABORATED in the console. Reading
> the map's own per-entity columns for those seven:
>
> ```
> zhao_terrain_residency_v2   reg=1029    mem=150528   flagged 41,984 bits
> zhao_geom_binner_v2         reg=2713    mem=277824   flagged  5,120 bits
> zhao_field_v3_rf            reg=6       mem=24576    flagged    384 bits
> zhao_texture_island_v3_top  reg=17408   mem=136344   flagged  7,680 bits
> zhao_light_stream           reg=7070    mem=5016     flagged  4,096 bits
> ```
>
> **FIVE OF THE SEVEN ALREADY INFER AS MEMORY.** `zhao_terrain_residency_v2`
> holds 150,528 memory bits against 1,029 registers; if its three flagged arrays
> were really in flip-flops it would show ~42,000. The prediction is refuted by
> the measurement, in the flattering direction for anyone planning work from it.
>
> **So: do not plan phase 3 from the static scan.** It is a source-text
> heuristic about what Quartus 17.0.2 MIGHT refuse; the `.map.rpt` is what
> Quartus actually did. The scan stays useful for DIAGNOSING a module already
> known to be flop-heavy, because it names the offending construct -- but it must
> not be used to FIND them.
>
> **The reliable instrument is the map's `reg` against `mem` per entity**, which
> `console_entity_attrib_shipping.md` already ranks, and which the table under
> "Where to point it first" below is drawn from. High registers with ZERO memory
> bits is a measurement; "will not infer" is a guess. The section headed "What
> the scan says" is kept because its arithmetic is right, but its 584/455 counts
> are counts of FLAGS -- not of opportunities, and now demonstrably not of
> defects either.
>
> Three separate inflations caught in one sitting, every one pointing the same
> way: the tree-wide bit total (1.9x, from files the console never elaborates),
> the finding count (two-entry arrays that belong in flops), and now the flags
> themselves.

# The dominant RAM-inference blocker is an INIT LOOP acting as a second write port

Coordinator, 2026-09-28, for the standing goal's phase 3 (damage control and
optimization). Derived from `tools/quartus/check_ram_inference.py` over the
current tree, cross-referenced against
`reports/synthesis/console_entity_attrib_shipping.md` (`@post-palram`,
`5CSEBA6U23I7`, A&S successful 2026-09-26 05:21).

## Why this matters

The shipping-part attribution says the binding constraint is **registers**, not
combinational logic:

| | measured | against `5CSEBA6U23I7` |
|---|---:|---:|
| combinational ALUTs | 301,446 | 360% of ~83,820 |
| dedicated logic registers | 312,114 | **186%** of 167,640 |
| block memory bits | 3,207,741 | **57%** of 5,662,720 |
| DSP blocks | 128 | 114% of 112 |

**The registers alone need ~78,028 ALM with the combinational logic at zero,
while memory sits at 57%.** Storage held in flip-flops is what overflows this
device, and M10K is the slack — which is the EARTHRAM lever the attribution
table already names.

## What the scan says

2,281 findings across 184 files. By pattern:

| pattern | count |
|---|---:|
| written from an ASYNC-RESET process (declared WEAK) | 988 |
| **TWO OR MORE distinct write addresses** | **584** |
| read COMBINATIONALLY through dynamic index | 441 |
| read at MODULE SCOPE through dynamic index | 172 |
| MULTIDIMENSIONAL unpacked array | 67 |
| element written through a bit/part-select | 17 |

**455 of the 584 (78%) have one address that is a short loop-style index**, and
the shape is an initialisation loop sitting beside the real functional write.
Verified by hand on `zhao_cmd_exec.sv`:

```systemverilog
for (vi = 0; vi < 2; vi = vi + 1) begin     // :2035  the init loop
  sv_eyex[vi] <= 32'd0;                     // :2041
...
sv_eyex[sv_view] <= {pkt_byte_i, ...};      // :2239  the real write
```

Quartus sees two write addresses, so it will not infer a memory and the array
becomes flip-flops.

## THE CAVEAT THAT STOPS THIS BEING A 584-SITE WIN

**The very case that confirmed the pattern also disqualifies itself.**
`sv_eyex` is `vi < 2` — **two entries deep**. A two-entry array *should* be
flip-flops; there is no M10K worth spending on it and removing its init loop
buys nothing. The count of findings is NOT a count of opportunities.

So this note deliberately does not claim a number. **The lever is the
intersection of three things, and only the first is measured so far:**

1. the array has two write addresses because of an init loop — 455 sites;
2. the array is **DEEP ENOUGH** to be worth an M10K (this is unmeasured, and it
   is the filter that decides whether any given site is a saving or a no-op);
3. removing the init is **semantically safe** — a RAM does not reset, so the
   reader must already gate on a validity bit, or one must be added. Where the
   code relies on power-on zeros this is a behaviour change, not a refactor.
   `zhao_cmd_exec.sv:2038` says zero is a *defined legal eye*, so its init is
   load-bearing prose, not dead code.

**Next step is a depth filter, not a repair pass.** Rank the 455 by declared
array depth x width, keep only those whose bits justify an M10K, and check each
survivor's readers for a validity gate. Anything else is churn.

## Where to point it first

From the register ranking, the subtrees holding registers with **zero block
memory bits** — i.e. arrays certainly in flip-flops today:

| entity | registers | % of part | mem bits |
|---|---:|---:|---:|
| `zhao_geom_lodstate` | 10,825 | 6% | 0 |
| `zhao_geom_ladderbank` | 5,951 | 4% | 0 |
| `zhao_terrain_devstore` | 4,418 | 3% | 0 |
| `zhao_field_loader` | 2,815 | 2% | 0 |
| `zhao_terrain_fieldlist` | 2,800 | 2% | 0 |
| `zhao_part_terrain_tap` | 2,783 | 2% | 0 |
| `zhao_geom_clipread` | 2,590 | 2% | 0 |
| `zhao_terrain_pagestream` | 2,470 | 1% | 0 |

~34,650 registers, about **21% of the part's register sites**, in blocks with
literally no block memory. Every one of them is flagged by the scan except
`zhao_terrain_devstore`, which has findings of a different kind.

And separately, the largest single holder: **`zhao_forge_assemble`, 39,023
registers (23% of the part) against 2,048 memory bits** — 15,970 ALUTs, so it is
storage-dominated rather than logic-dominated. Its flagged arrays are read at
module scope through a dynamic index (`dqf_*_q` via `dqf_rp_q`), which is a
different blocker from the init loop and wants its own look.

## Method note

My first extraction of the address pairs used
`grep -o "...\[[^]]*\]"`, which stops at the first `]` — the addresses contain
`]`, so it truncated every pair and reported **0 matches** for the init-loop
shape. A confident zero from a broken pattern, which is this repository's own
first law; it was caught only by printing the extracted text instead of the
count. The numbers above come from a parser that reads the whole bracketed list.

## THE ACTUAL TARGET LIST, BY REGISTERS *OWNED*

Added after the correction above, and this is the section to act on. The
ranking in "Where to point it first" uses the AGGREGATED register column, which
charges a parent for storage its children declare -- so it points at wrappers.
`reg_own` attributes storage to the module whose own body declares it, which is
the module you would actually edit.

From `@post-palram` (`5CSEBA6U23I7`, A&S 2026-09-26 05:21):

| module | `reg_own` | % of 167,640 sites | `mem` bits |
|---|---:|---:|---:|
| `zhao_forge_assemble` | 37,662 | 22.5% | 2,048 |
| `zhao_field_v3_exec` | 24,795 | 14.8% | 25,344 |
| `zhao_cmd_exec` | 12,149 | 7.2% | 18,472 |
| `zhao_geom_lodstate` | 10,009 | 6.0% | **0** |
| `zhao_project_core` | 6,561 | 3.9% | 3,532 |
| `zhao_geom_ladderbank` | 5,951 | 3.6% | **0** |
| `zhao_material_resolve` | 5,207 | 3.1% | 896 |
| `zhao_field_host_v2` | 5,115 | 3.1% | 85,282 |

**Eight modules own 107,449 registers -- 64% of the device's register sites.**
The registers are not spread thin; they are concentrated, and that is good news
for phase 3.

### `zhao_forge_assemble` is the one to look at first, and it is not a wrapper

**37,662 registers owned, 22.5% of the part, against 2,048 memory bits** -- with
only 14,104 owned ALUTs. It is storage-dominated rather than logic-dominated,
which is the exact profile the M10K trade exists for.

**And it is NOT a composition artefact**, which was the first thing worth ruling
out. Its standalone census (`@flop-census-20260926`, same device) reads 15,648
ALUTs / **39,167 registers** / 2,198 memory bits, against the console's 15,970 /
39,023 / 2,048. Identical within noise, so the storage is intrinsic to the block
and can be worked on standalone -- no console fit needed to evaluate a change,
which matters when the console cannot place at all.

Its named subtrees account for barely 1,400 of those registers
(`zhao_raster_rcp24_v4` 1,045, `zhao_geom_depthquant_stream` 384), so **~37,700
sit directly in `zhao_forge_assemble`'s own body.**

One thing already known about it, and it cuts against a quick win: its flagged
arrays (`dqf_slot_q`, `dqf_prof_q`, `dqf_w_q`) are *"read at MODULE SCOPE through
a dynamic index `dqf_rp_q`"* -- a continuous assignment, which is combinational.
That is a DIFFERENT blocker from the init-loop shape this note opened with, and
a harder one: a combinational read through a dynamic index forces a per-bit mux
the width of the array, and a memory cannot serve it without a registered read
port. **So the repair is a pipeline change, not a declaration change**, and it
needs the block's throughput contract read first.

Note also that `dqf_slot_q` DID infer standalone -- the census shows
`altsyncram:dqf_slot_q_rtl_0` holding 640 bits -- so part of the array is
already memory and the flop count is the rest. Do not assume the whole 37,662 is
addressable.

