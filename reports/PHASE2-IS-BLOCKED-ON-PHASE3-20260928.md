# The full console fit cannot be finished until the design shrinks 23%

Coordinator, 2026-09-28. This is a sequencing finding about the standing goal
("close the gaps, then get the real full console fit finished, then damage
control and optimization"), and it is measured rather than argued.

## The finding

**Phase 2 cannot complete before phase 3 does part of its work.** The owner's
sequence assumes the fit will finish and report where we stand. It will not
finish, on any device this installation has.

* The 2026-09-25 placed fit died in `quartus_fit`:
  `Error (170011): Design contains 336023 blocks of type combinational node.
  However, the device contains only 227120 blocks.`
* **227,120 is the LARGEST available**, not a small target. Only `cyclonev` is
  installed; its biggest die is the 300K-LE class, and 227,120 is exactly the
  number Quartus reports for it.
* The console at HEAD maps to **293,886 combinational ALUTs**
  (`@current-20260928`, shipping part, A&S successful, 0 errors).

**So the design must shed 66,766 ALUTs -- 23% -- before a placed fit can
produce a single ALM or Fmax number.** Until then the synthesis estimate is the
ceiling on what can be known, and 222,666 estimated ALMs against 41,910 (5.31x)
is that estimate.

This is a measured engineering impossibility, which this repository's rules call
a finding rather than permission to invent a pass. It is not a refusal to run
the fit; the fit has been run and it stops.

## The metric changes: ALUTs, not registers

`Error (170011)` counts **combinational nodes**. Placement is blocked by ALUTs
(351% of the part), not by registers (167%). Most of the campaign's attention,
including my own earlier note, has been on registers because that is where the
attribution table's prose points. **For PLACEMENT, ALUTs are the gate.**

The two are linked, which is the encouraging part: FLOPARRAY's array
conversions cut ALUTs as hard as registers, because moving an array into M10K
deletes the address mux and the mux is where the ALUTs live.

```
zhao_forge_assemble   ALUTs 15,648 -> 2,803   (-12,845, -82%)
zhao_geom_lodstate    ALUTs  6,261 -> 3,398   ( -2,863, -46%)
```

## Where the ALUTs are

By subsystem, owned (the module whose own body holds them):

| subsystem | `alut_own` | % of 293,886 |
|---|---:|---:|
| field | 50,866 | 17.3% |
| terrain | 42,170 | 14.3% |
| geom | 39,248 | 13.4% |
| raster | 19,566 | 6.7% |
| texture | 16,808 | 5.7% |
| part | 10,680 | 3.6% |

FIELD, TERRAIN and GEOM own **45%** between them.

**The ALUTs are far less concentrated than the registers.** 815 entities own
ALUTs; the top 12 sum to roughly the 66,766 needed, but you cannot zero a
module -- a realistic 30-50% cut on a target yields a fraction of its row. **This
is a multi-packet programme, not a pass.**

### The candidate set with the best evidence

Blocks with **zero block memory bits** and heavy in both columns -- the shape
FLOPARRAY converted twice with measured results:

| module | `alut_own` | `reg_own` |
|---|---:|---:|
| `zhao_terrain_devstore` | 7,070 | 4,418 |
| `zhao_geom_ladderbank` | 2,574 | 5,951 |
| `zhao_field_loader` | 3,495 | 2,815 |
| `zhao_part_terrain_tap` | 2,494 | 2,783 |
| `zhao_terrain_pagestream` | 2,266 | 2,470 |
| `zhao_geom_clipread` | 2,016 | 2,590 |
| `zhao_geom_skin` | 2,122 | 2,146 |
| `zhao_field_v3_spline` | 2,806 | 1,298 |
| `zhao_field_v3_normalize` | 2,739 | 1,322 |
| `zhao_raster_edgewalk` | 2,983 | 942 |

Fourteen such blocks own **38,638 ALUTs and 32,798 registers** with no block
memory at all.

**BUT THE TOP ONE IS NOT A FLOP-ARRAY CASE, and that matters for planning.**
`zhao_terrain_devstore` declares **no unpacked arrays**; its storage is two
packed 1024-bit vectors (`slot_valid_q`, `hist_valid_q`), which
`check_ram_inference.py` deliberately excludes as "a register file by
construction". Its 7,070 ALUTs are combinational logic over 512-bit burst
datapaths, not address muxes. **The M10K trade does not obviously apply to it**,
and "zero memory + heavy = convert it" is an inference that fails on the very
first row. Each candidate needs reading before it is scheduled.

## The free levers are ALREADY TAKEN -- checked, not assumed

Both documented sources of no-engineering area were tested against this map:

* **Superseded modules elaborated in the closure: ZERO.**
  `completion_register.superseded_in_closure()` returns nothing. The v1-FIELD
  accident that cost a whole fit is not present.
* **The projector duplication is CONSOLIDATED.** CLAUDE.md records a
  ~33-DSP/~6,000-ALM saving whose last step "never happened". The map shows
  **one** `zhao_project_core` row. That cheque has been cashed and the example
  in the rules file is stale -- which that file warns about itself.

So there is no cleanup left that yields area for free. **The 23% is engineering.**

## What this means for the standing goal

The honest reading is that phases 2 and 3 are **inverted by the measurement**:
damage control is the PREREQUISITE for the full fit, not its successor. Nothing
here changes the goal; it changes the order, and the owner should know that the
"so we know where we stand" answer already exists in synthesis form and cannot
be improved upon until the design is 23% smaller.
