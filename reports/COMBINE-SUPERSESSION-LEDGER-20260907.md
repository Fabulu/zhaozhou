# COMBINE: two supersessions pending, one with a trigger that already fired

Measured rows, all from `reports/synthesis/zhao_block_fit.json`:

| block | status | ALM | DSP | Fmax |
|---|---|---|---|---|
| `zhao_texture_combine` | **failed:structure** | 494 | **8** | 100.12 |
| `zhao_texture_material_combine_v1` | ok | 1,475 | 2 | 36.28 |
| `zhao_texture_material_combine_v2` | ok | **870** | 2 | **114.04** |

## 1. `zhao_texture_combine` — the deletion condition is MET and unexecuted

`design/prod_manifest.yml` carries a standing instruction, written when the
block was refuted as docket D19q for spending 8 DSP against §3.4's "reject
DSP > 2":

> The refuted block stays until its replacement has a MEASUREMENT, so the island
> is never left with no combiner and no record of why the shape changed. **When
> that fit lands, delete this row, its RTL, and
> `tests/texture/texture_combine_diff.cpp` together.**

That fit has landed. `zhao_texture_material_combine_v1` is `ok` at 1,475 ALM /
2 DSP / 36.28 MHz. **The trigger fired and the deletion was never performed**,
so a block the architecture's own tripwire refuted is still registered as
production and still instantiated by `zhao_prod_top`.

This is the same shape as the `.gitignore` lesson in CLAUDE.md: a rule that
records what should happen is not the thing that makes it happen. The row even
says when to act, and the acting is the half nobody did.

## 2. `material_combine_v1` — superseded in the island, still in the production top

The manifest already knows:

> `zhao_texture_material_combine_v2`: successor — the paired-phase combiner; the
> ISLAND instantiates V2, the production top still instantiates V1

And V2 is not a reduced V2. Its own header, quoting the owner's recovery brief
§0, is explicit that **the arithmetic is unchanged**:

> "COMBINE needs a different EXECUTION ORGANIZATION, not different material
> math. Preserve all eight recipes." … "every equation here is V1's and the
> oracle's, byte for byte. What is replaced is HOW work is chosen and where
> operands live."

**Checked rather than taken on trust**, because "the replacement is smaller AND
faster" is exactly the comfortable claim that deserves the extra five minutes —
a smaller block can be smaller because it does less.
`tests/texture/material_combine_v2_diff.cpp:test_every_recipe_matches_the_oracle`
runs **200 fragments per recipe across all eight recipes**, three samples each so
the terrain recipes are exercised rather than refused, and diffs every result
against the oracle. 27 checks pass.

So V2 is a like-for-like replacement measured at **41% fewer ALM and 3.1× the
Fmax**, and V1 is replaced hardware still carried in production.

## Recommendation, not action

Both are DELETIONS of RTL, tests and manifest rows, and deletion is outside what
this session decides on its own. The evidence is assembled so the call is cheap:

1. **`zhao_texture_combine`** — delete the row, `zhao_texture_combine.sv`, and
   `tests/texture/texture_combine_diff.cpp`, exactly as the manifest instructs.
   Its condition is met and its 8-DSP violation is live in the meantime.
2. **`zhao_texture_material_combine_v1`** — retire to `excluded: superseded`
   once `zhao_prod_top` is switched to V2. Note this changes the production top,
   so it wants its own fit rather than being folded into another change.

Both matter to the rearchitecture brief's own warning about building better
hardware alongside the old expensive machinery: the island is already on V2, and
the production top is still paying for V1 **and** for a block the architecture
refuted.
