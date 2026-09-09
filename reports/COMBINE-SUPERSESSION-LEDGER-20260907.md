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

---

## ADDENDUM 2026-09-09 — the condition is now fully met, and the two documents disagree

The owner asked, on being shown the combine numbers: *"Can we not find a design
that uses memory instead? We need both ALM and DSPs desperately."* That is
motivation, and it is not authorisation to delete RTL, so nothing has been
deleted.

### What is new since this ledger was written

`zhao_texture_material_combine_v1` has a fresh full-fit row, so the condition the
manifest names is met twice over:

| | ALM | Fmax | registers | DSP | multstyle |
|---|---|---|---|---|---|
| `zhao_texture_combine` | **494** | **100.12** | 524 | **8** | no |
| `material_combine_v1` (2026-09-09) | **1663** | **69.75** | 1269 | **2** | yes |
| `material_combine_v1` (this ledger) | 1,475 | 36.28 | — | 2 | yes |

Deleting `zhao_texture_combine` is **−8 DSP and −494 ALM**, against a whole-machine
DSP census of ~154 on a device with 112 (`DSP-BUDGET-CENSUS-20260908.md`). It is
the cheapest DSP anywhere on the board: no design work, no fit, no risk to a
shipping path, because **nothing instantiates it except the generated resource
top** (verified by grep — the islands use `material_combine_v2`).

### THE TWO DOCUMENTS DISAGREE, and that is worth saying out loud

* `design/prod_manifest.yml:67-69` reads as an INSTRUCTION: *"When that fit lands,
  delete this row, its RTL, and tests/texture/texture_combine_diff.cpp together.
  — zhao_texture_combine # REFUTED (D19q); delete when v1 is measured"*.
* This ledger reads as a REFERRAL: *"Both are DELETIONS of RTL, tests and manifest
  rows, and deletion is outside what this session decides on its own."*

A future agent reading only the manifest would delete; reading only the ledger it
would wait. **The ledger is the safer reading and it is the one being followed**,
because CLAUDE.md's barge-ahead licence explicitly stops at destructive actions.
But the ambiguity is itself a defect: an instruction with a trigger, sitting in a
file that is not the decision record, invites exactly the unilateral deletion the
ledger refused.

### The two calls, ready

1. **Delete `zhao_texture_combine`** — row, `.sv`, and
   `tests/texture/texture_combine_diff.cpp` together. −8 DSP, −494 ALM. Trigger
   met, nothing depends on it, no fit needed. *One word authorises this.*
2. **Switch `zhao_prod_top` to V2 and retire V1** — a further −793 ALM by the
   architect's arithmetic. This one CHANGES THE PRODUCTION TOP, so per this
   ledger it wants its own fit rather than being folded into another change.

And the memory alternative the owner asked for exists and is bit-exact — see
`reports/MEMORY-FOR-ALM-AND-DSP-20260909.md`: quarter-square ROMs, 2 M10K buying
the island's last 2 combiner DSP with zero added latency. That is a design for
`material_combine_v2`, the block that SHIPS, and it is independent of both
deletions above.
