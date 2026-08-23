# DSP budget — the binding constraint, measured 2026-08-20

> # THE CENSUS BELOW DOES NOT DESCRIBE THIS CONSOLE. Added 2026-08-21.
>
> `reports/synthesis/zhao_block_fit.json` records `rtlCleanAtHead: false` and
> `sourceCommit: 96c0394a` — **93 commits old**, measured against a tree that
> was not clean at HEAD.
>
> Verified module by module 2026-08-21: it measures **42 modules; the
> repository contains 88.** Forty-six have never been fitted, and they include
> the biggest multiplier consumers written since — `zhao_geom_project` (a
> second nine-product projector), `zhao_geom_skin` (whose own header calls it
> the largest multiplier count in the repo), the pose and matrix blocks, and
> the entire Field IR engine.
>
> **So 171 is not the console's DSP count, and 133 is not a plan.** This
> document warned that per-block fits are an UPPER bound; the sharper problem
> is the opposite — the census is also an **under-count**, because half the
> console is missing from it.
>
> **TEXTURE.TMU CLOSED 2026-08-23 (RUN-20260823-1736): 28 → 6 DSPs**, by
> factoring the bilinear law (`A = (t00<<8) + (t10−t00)·fu`, and so on — 3
> products a channel instead of 8) and multiplexing two channels at a time.
> Samples bit-identical. **And the fit taught a general rule worth more than the
> row:** Quartus 17.0.2 Lite packs nothing — **DSP blocks = the number of `*`
> operators**, whatever the operand widths. Twelve products fit at twelve DSPs.
> Plan cuts by counting operators, not by counting DSP-sized multipliers the
> operands would fit into.
>
> **CORRECTION, 2026-08-23 night — "whatever the operand widths" is wrong.**
> The measured half stands: Lite packs nothing, TEXTURE.TMU's 9x9 and 18x9
> operators each took **one** DSP, and twelve products fitted at twelve DSPs. So
> counting operators is the right way to plan a cut.
>
> But the width qualifier does not survive our own evidence.
> `reports/QUARTUS_GOTCHAS.md` §5 records the **same `zhao_geom_lod` source**
> costing **28 DSPs at 72-bit operands and 18 at 64-bit** — impossible if width
> were irrelevant. A 33x33 signed product is several blocks, not one.
>
> > Each nonconstant `*` creates **one physical multiplier structure**. Its DSP
> > cost depends **discontinuously** on operand width and signedness, and Lite
> > has not been observed to pack two narrow operators into one block. **The
> > operator count is a LOWER BOUND**, exact only while every operand stays
> > inside one block's native width.
>
> Practical consequence, which §5 already states: **prove the width, then
> synthesise.** "Free" slack in an operand is the difference between 18 DSPs and
> 28 for identical logic.
>
> `reports/DSP_Audit_2026-08-21.md` carries the owner's audit, the exact
> algebra for cuts this document missed (TERRAIN.LOD 28 to 4-8, SURFACE.STAMP
> 28 to 4-8 rather than 20, TEXTURE.TMU 28 to 8-12, GEOM.BINNER+SETUP 16 to 4,
> RASTER.FRAGMENT 10 to 7), and the order of work. **Step 1 of that order is to
> refresh this census at HEAD. Nothing below should be planned from until that
> is done.**

> Device `5CSEBA6U23I7`: **112 DSP blocks**, read from the fitter's own
> `used / available` line rather than from a datasheet or from memory.

## The finding

The design wants **171 DSP blocks against 112**. It is over by 53%, and DSP is
the one resource with no slack: a multiplier either occupies a DSP or is rebuilt
in logic, which spends the ALMs that are also short.

This went unnoticed because **there was no DSP budget document** and the
per-block fitter reports were never totalled. ALMs had everyone's attention
(25,430 of 41,910, 61%) and were never the problem.

## Measured, per block

Read from `reports/synthesis/zhao_block_fit.json`, provisional per-block fits
against `5CSEBA6U23I7`.

| block | DSP | ALMs |
|---|---:|---:|
| `TERRAIN.PROJECT` | **33** | 6,068 |
| ~~`TEXTURE.TMU`~~ | ~~28~~ → **6** | 1,839 → see below |
| `TERRAIN.LOD` | 28 | 2,086 |
| `SURFACE.STAMP` | 28 | 950 |
| `TERRAIN.NORMALS` | 18 | 789 |
| `GEOM.BINNER` | 12 | 1,303 |
| `RASTER.FRAGMENT` | 10 | 485 |
| `TERRAIN.TESS` | 6 | 1,311 |
| `GEOM.SETUP` | 4 | 743 |
| `GEOM.CLIP`, `RASTER.EDGEWALK` | 2 each | |
| **total (deduplicated)** | **171** | **24,327** |

**The total is deduplicated.** Five measured rows are instantiated inside other
measured rows and would otherwise be counted twice: `TEXTURE.BILERP` inside
`TEXTURE.TMU` (four instances — the TMU's 28 DSP *are* those bilerps),
`RASTER.BLEND` inside `RASTER.FRAGMENT`, `GEOM.ARENA` inside `GEOM.BINNER`,
`SURFACE.BLEND` inside `SURFACE.STAMP`, `RASTER.TILESTORE` inside
`RASTER.RESOLVE`. Counting every row gives 180; the honest figure is 171.

## Why it is 171: every block multiplies in parallel

The cause is one habit repeated, not one bad block. Each block computes all of
its products **simultaneously** to hit a per-clock throughput target:

- **`TERRAIN.PROJECT`, 33.** `mul32` is a full 32x32 signed product, and the
  transform issues **nine of them per vertex** (three matrix rows x three
  terms). At roughly four 18x18 DSPs per 32x32 product that is ~36, which is
  the measured 33. The ledger asks for 1 projected vertex per clock and the
  block delivers it, so the nine are all live at once.
- **`SURFACE.STAMP`, 28.** Six wide multiplies: two 32-bit radius squares
  (`rad_ext*rad_ext`, `rin_ext*rin_ext`), two 41-bit span products, and the
  per-texel `d2 = dxw*dxw + dzw*dzw`.
- **`TERRAIN.NORMALS`, 18.** The cross product is six multiplies issued in one
  cycle (`e1y*e2z - e1z*e2y` and its two siblings).

## The levers, in order of size

**These are estimates from multiply counts, not measurements.** Each needs a
fit to confirm, exactly as the TEXTURE.CACHE fix needed two attempts before the
number moved.

1. **Time-multiplex `TERRAIN.PROJECT`'s rows.** Three `mul32` per cycle instead
   of nine, one matrix row at a time: ~33 to ~12, saving ~21. Costs three cycles
   per vertex instead of one, so it trades directly against the ledger's
   "1 vertex per clock". That target has to be re-argued rather than assumed —
   and the composed frame budget, not the block's own datasheet line, is what
   should decide it.
2. **Serialise `SURFACE.STAMP`'s radius squares.** `r_outer2` and `r_inner2` are
   **per-stamp constants**, computed in parallel with the per-texel distance.
   Either compute them over the stamp's setup cycles with one shared multiplier,
   or have the caller supply r-squared directly. ~8 saved, and the second option
   costs nothing at all in the block.
3. **Serialise `TERRAIN.NORMALS`' cross product.** Six multiplies over two or
   three cycles instead of one: 18 to ~6-9.
4. **Narrow operands where the result is only compared.** `d2` feeds an
   inside/outside test against r-squared. A comparison does not need the full
   32x32 product's precision, and narrowing the operands drops whole DSPs.

Items 2 and 4 are the cheap ones: neither changes a throughput contract.

## What this does NOT settle

Per-block fits are **upper bounds**. There is no cross-block sharing and no
composed-design optimisation, and roughly 9,000 virtual pins across the measured
set become plain wires once composed. The composed fit is the only thing that
answers "does it fit".

> **CORRECTION 2026-08-21: the 28.4 GB figure this paragraph used to cite was a
> BUG, not a requirement, and it was fixed on 2026-08-20.** The project's last
> line was `set_instance_assignment -name VIRTUAL_PIN ON -to *`, and a wildcard
> instance assignment is matched against every node name in the design rather
> than the top-level ports it was written for (`d1a2b8a`). With the 101 ports
> named explicitly, composed analysis and synthesis **run**: 42:33 at a 6.2 GB
> peak (`f3506b6`).
>
> The composed fit is **not blocked on hardware**. It is blocked on Quartus
> Error 276003 — registers that cannot convert to RAM megafunctions because two
> memories have asynchronous read logic. `zhao_scanout_linebuf` has since been
> given a registered read; `zhao_cmd_dma`'s `blit_buf` has not.
>
> Treat 6.2 GB with suspicion too. `9c693a9` measured that parsing the entire
> source cone is free (0.24 GB) and that the cost is in ELABORATION, which is
> superlinear in Quartus 17.0.2 Lite for a top of sixteen ordinary instances
> with no generate blocks and no large arrays. **A newer Quartus is the
> identified lever and has not been tried.**

DSP is less likely than ALMs to shrink on composition, though: a multiplier is a
multiplier whether or not its neighbours are present. Treat 171 as close to
real and 25,430 ALMs as generously padded.

## Standing rule

**Any block that adds a wide multiplier states its DSP cost in its contract**,
and any block claiming a per-clock throughput target states what that target
costs in DSPs. The absence of this document is the reason a 53% overrun
accumulated without anyone deciding to spend it.
