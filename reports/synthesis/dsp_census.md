# DSP census — `@dsp-census-20260926`

Each row is a block mapped **on its own** by `quartus_map`, so nothing in
the composition is inflating or sharing it. Modes come from Quartus's own
`DSP Block Usage Summary`. Derived by `tools/budget/dsp_census.py`.

**A `Two Independent 18x18` block is doing two multiplies; an
`Independent 27x27` block is doing one.** So the 27x27 column is the
expensive one, and it is the column that says whether a block is
multiplying wide VALUES or merely declaring wide ones.

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
