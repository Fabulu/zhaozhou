# A registered read turns the 256-arm case into an M10K ROM. No rewrite needed.

Coordinator, 2026-09-28. Phase 3's first lever, **measured** rather than argued.
Five `quartus_map` runs, Quartus Prime Lite 17.0.2, device `5CSEBA6U23I7`, all at
source-list hash `01c9a16ce8122984` with `rtlCleanAtHead: true`.

## The result

| row | comb ALUTs | mem bits | inferred memories | mode |
|---|---|---|---|---|
| `zhao_field_rcp24_rom@base20260928` — **the shipped module** | **145** | 0 | 0 | — |
| `@style0` — case, register AFTER the table | **0** | 7,936 | 1 | ROM |
| `@style1` — case INSIDE `always_ff` | **0** | 7,936 | 1 | ROM |
| `@style2` — indexed array + `initial` + clocked read | **0** | 7,936 | 1 | ROM |
| `@style3` — plain RAM, **POSITIVE CONTROL** | 0 | 7,936 | 1 | Simple Dual Port |

**The control fired**, so the other rows are evidence. The inferred memory is
`altsyncram … depth 256, width 31` in every case.

**The answer: a registered read is SUFFICIENT.** Quartus infers a ROM directly
from the 256-arm `unique case`. The table does **not** need restructuring into an
indexed array — `@style1` and `@style2` are equally good, and `@style0` shows the
register may even sit *outside* the table and still be absorbed.

**Per instance: 145 comb ALUTs → 0, plus one 256×31 ROM.** Across the thirteen
instances that is ~1,885 ALUTs by this row, or the 1,727 the console map
attributes to them — either way about **2.6% of the 66,766** that stand between
the design and placement on the largest installed die. Thirteen M10K of 246 free.

## This corrects the phase-3 report, twice

`PHASE3-THE-LEVER-IS-DUPLICATION-20260928.md` calls this row *"the cheapest thing
on this list to reason about, because a ROM has no state and no throughput
contract — **only a read port count**."*

1. **That ROM has no read port at all.** It is combinational: no clock, no
   register, nothing to infer. The measurement above is the proof — 145 ALUTs
   and *zero* memory bits.
2. **But the fix is CHEAPER than the correction implied.** Having found the
   table was not a table, I expected the repair to need a structural rewrite of
   a generated file *plus* a pipeline stage. It needs **only the pipeline
   stage.** `@style1` settles it.

So the packet is: **one cycle of latency at each of 13 call sites**, and nothing
else. That is a real cost — `zhao_field_v3_normalize` declares `latency: fixed:N`
in `design/blocks.yml` and its consumers are entitled to it — but it is a
contract change, not a rewrite.

## And it corrects my own probe design, which matters more

**`@style0` was labelled BASELINE and is not one.** I intended it to reproduce
the shipped shape; I registered its output, and *that register is the treatment*.
Styles 0 and 1 are two spellings of the same experiment.

I got a usable result only because the shipped module exists and was mapped
separately. **Had it not, the sweep would have returned four ROMs and no
contrast** — which reads as "everything infers", is uninterpretable, and would
have looked like a successful run. A positive control proves the instrument can
fire; it does nothing to protect a baseline that quietly contains the treatment.
**A baseline that contains the treatment is not a baseline.**

The shipped module was therefore re-mapped at the *current* source list
(`@base20260928`) rather than quoted from the older row, so the comparison is
like-with-like. Both read 145; the older row's hash was `9a53f278725c7224`.

## What is NOT claimed

* **`estimatedAlms` 162 → 36 is not a like-for-like ALM claim.** The rows have
  different virtual-pin counts (39 against 72), and this file's own
  `limitations` say map estimates are pre-placement and not comparable to
  fitter ALM counts. **`combAluts` is the number that carries the finding.**
* The 13 instances are not all in the console's closure by this measurement;
  the 1,727 figure comes from the console map and the 145×13 from this one.
  They agree closely enough to act on and are not the same measurement.
* `critical: 1` on every probe row is **explained and benign**:
  `Critical Warning (15725): clock port is fed by virtual pin "clk~input"`. It
  is a standalone-map artefact of giving the probe a clock — which is why the
  shipped ROM, having none, reads `critical: 0`. The warning is *caused by* the
  treatment and says nothing about inference.

## What this licenses next

This is the **lookup-for-computation** lever, now measured instead of assumed,
and the measurement generalises: **any combinational case-table in this tree
becomes memory for the price of registering its read.** The phase-3 report counts
twelve blocks with zero memory and ALUTs far above registers holding **32,784
ALUTs, 11.2% of the design**. Not all of that is table-shaped — much is genuine
arithmetic that no table can replace — but the ones that ARE tables are now known
to convert, and the device has 246 M10K doing nothing.

**The next question is which of those twelve are tables**, and that is a read of
the RTL rather than a fit. It does not need Quartus and it does not need the
owner.

## Reproducing this

```
python tools/quartus/gen_probe_rcp24_rom.py --check       # freshness gate
tools/quartus/run_block_map.ps1 -Module zhao_probe_rcp24_rom \
    -TopParameters STYLE=1 -RowLabel '@style1'
```

The probe is committed (`fpga/rtl/synth/zhao_probe_rcp24_rom.sv`) and generated
from the shipped ROM, so its 256 constants cannot drift silently. Read
**STYLE=3 first, always.**
