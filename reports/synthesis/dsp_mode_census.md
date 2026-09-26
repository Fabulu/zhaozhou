# DSP MODE census — `@dsp-census-20260926`

Each row is a block mapped **on its own** by `quartus_map`, so nothing in
the composition is inflating or sharing it. Modes come from Quartus's own
`DSP Block Usage Summary`. Derived by `tools/budget/dsp_mode_census.py`.

**A `Two Independent 18x18` block is doing two multiplies; an
`Independent 27x27` block is doing one.** So the 27x27 column is the
expensive one, and it is the column that sorts blocks worth looking at.

**IT IS NOT A DIAGNOSIS, AND "DECLARED WIDTH" IS THE WRONG ONE.** This
header used to say the 27x27 column separates blocks that multiply wide
VALUES from blocks that merely DECLARE wide ones, and that the second is
free to fix because inference follows declared width. Measured on
2026-09-26, that is false on Quartus 17.0.2: narrowing
`zhao_geom_attrsetup`'s declared widths one group at a time (96x96 ->
46x32, 46x46 -> 22x21, 72x72 -> 22x32) moved the row by ZERO blocks each
time. Quartus already strips the plain `WIDE'(narrow) * WIDE'(narrow)`
sign extension. What cost 9 of that block's 24 wide blocks was one
operand written `(-(72'(cy_by))) * 72'(va_i)` -- **the negation taken
INSIDE the cast** -- and moving the minus sign outside the multiply, at
unchanged declared width, recovered all nine.

So the pattern to grep for is **an arithmetic operation applied to a
widened value before the multiply**, not a wide literal. Evidence:
`tests/probes/zhao_attrsetup_mul_probe.sv`, eight arms, rows
`zhao_attrsetup_mul_probe@probe-m0..m7`.

**A ROW HERE IS A LABELLED SNAPSHOT, NOT THE CURRENT DESIGN.** The table
below is whatever `--label` selected. `zhao_geom_attrsetup` was repaired
to **36 DSP / 15 wide / 836 ALUTs** on 2026-09-26 (`@gz-after`); any row
above showing it at 45 is the pre-repair measurement and is correct as
history, not as a budget.

| block | DSP | % of part | registers | Two Independent 18x18 | Sum of two 18x18 | Independent 18x18 plus 36 | Independent 27x27 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `zhao_geom_attrsetup` | 45 | 40% | 225 | 15 | 6 |  | 24 |
| `zhao_geom_skin_norm` | 21 | 19% | 920 | 6 |  | 7 | 8 |
| `zhao_geom_skin` | 9 | 8% | 2146 | 6 | 3 |  |  |
| `zhao_twod_plane` | 8 | 7% | 746 | 4 |  | 4 |  |
| `zhao_geom_cull` | 6 | 5% | 1825 | 4 | 2 |  |  |
| `zhao_geom_meshfetch` | 6 | 5% | 1688 | 4 | 2 |  |  |

**Census total: 95 DSP across 6 block(s), 85% of the 112-DSP part.**
**32 of those 95 are in a 27x27 mode**, which is the shape to ask about
first.

This is a census of what was mapped, not of the console. For the whole
machine see `reports/synthesis/console_entity_attrib.md`.
