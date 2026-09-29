# The ledger's twenty-odd RULES have not run at all, and V7 alone has 71 findings waiting

Coordinator, 2026-09-29. **This supersedes a claim I made twice today.** In
`FINDINGS-group-seq-ids.md` and `FINDINGS-veljoin-counter.md` I wrote that a
referential-integrity check on `downstream`/`upstream` "is a rule the ledger does not have"
and "neither rule exists today".

**Rule V7 exists and does exactly that**, in `tools/ledger/src/rules.ts:240-258` — unknown
block references *and* edge symmetry, both directions. I asserted an absence without
grepping for it, which is the failure mode this repository names most often: *"the 'X does
not exist' claims this campaign has killed came overwhelmingly out of DOCUMENTS rather than
out of the tree."* Mine came out of my own document, two hours old.

**The rule has never run.** `tools/ledger/src/cli.ts:113-123`:

```ts
const errors: string[] = [...blocks.schemaErrors, ...ops.schemaErrors];
…
if (errors.length === 0) {
  … V1–V23 …
}
```

The whole rule stage is gated on the schema stage being **empty**. With 42 schema errors
outstanding — 96 this morning — **V1 through V23 do not execute.** `eb5ebfde` recorded this
in passing ("the ledger's schema stage fails first and the rule stage is skipped") and its
consequence was never measured.

## So the schema work was not hygiene

That reframes everything I did to `design/blocks.yml` today. Taking the schema stage from 96
errors to 42 looked like tidying. It is the only way to **switch twenty-odd rules back on**,
and nobody knows what they will say, because they have not said anything.

I measured one of them. A one-off read-only preview of V7's two checks — not committed,
because reimplementing a rule the ledger already owns is exactly the duplication CLAUDE.md
warns about — reports:

| | |
|---|---:|
| **V7a unknown block references** | **11** |
| **V7b asymmetric edges** | **60** |

**From one rule of twenty-plus.** The preview reads the YAML textually and applies only
V7, so 71 is a **lower bound**.

## The 11 unknown references are mostly composed hardware with no row

This is the same pattern as `TERRAIN.GROUP_SEQ`, and it is far wider than the three
instances I had found:

| referenced id | module the naming rule gives | exists | in the console fit's 299-file digest |
|---|---|---|---|
| `CMD.EXEC` | `zhao_cmd_exec` — **227,619 B** | yes | **yes** |
| `TERRAIN.GROUP_SEQ` | `zhao_terrain_group_seq` — 34,653 B | yes | **yes** |
| `GEOM.GROUP_SEQ` | `zhao_geom_group_seq` — 28,605 B | yes | **yes** |
| `TERRAIN.LODFEED` | `zhao_terrain_lodfeed` | yes | **yes** |
| `TERRAIN.HDRREAD` | `zhao_terrain_hdrread` | yes | **yes** |
| `TWOD.SAMPLER` | `zhao_twod_sampler` | yes | **yes** |
| `MEM.HPS` | `zhao_mem_hps` | **no** | no |

`zhao_terrain_veljoin` (19,339 B, also in the digest) belongs to the same class; it is named
`TERRAIN.VELJOIN` in `prod_manifest.yml` and in `TERRAIN.MATJOIN`'s own `purpose`, and has
no row either.

**`CMD.EXEC` is the largest module in the console at 227 KB and has no ledger block.** Six
composed modules and one genuine shorthand, not three.

`MEM.HPS` is the one that is probably just a shorthand — `MEM.HPS.BRIDGE` and
`MEM.HPS.ARBITER` both have rows and contracts, and no `zhao_mem_hps.sv` exists. That is a
reference to repoint, unlike the other six.

## What this says about the gates

**The completion register reads ZERO mandatory gaps and is right.** It checks declared
capabilities against the composition and cannot see a module nobody declared. Seven now
exist.

**The one instrument that would have caught them has been switched off by an unrelated
red** — the exact law this repository wrote down: *"a gate red for an unrelated reason gives
cover to every other red inside it."* V7 is not a missing rule. It is a rule that has been
covered for as long as the schema stage has been failing, and the six composed-without-a-row
modules are what was hiding under it.

## What to do, in order

1. **Finish the schema stage.** 42 errors left, of which 13 are the umbrella conditional.
   Everything else needs a decision, listed in `FINDINGS-ledger-schema.md` — nine blocks
   owing a random-test arm, five a reference model, two a rule that is over-broad. **The
   prize is no longer a green badge; it is V1–V23 running for the first time.**
2. **Then run the rule stage and read it.** Expect ≥71 findings from V7 alone, and an
   unknown number from the other twenty-two. None of them has been seen.
3. **Only then** decide the ownership question for the six composed modules, because the
   rule stage may name more of them.

## Not claimed

* Not that the six modules are wrong, unbuilt or untested. Several are large, mature and
  clearly working; what they lack is a ledger identity.
* Not that 71 is the number. It is a lower bound from a textual preview of one rule, and the
  ledger's own implementation may count differently.
* Not that the 60 asymmetric edges are 60 defects. An asymmetric edge can be a missing
  reverse entry rather than a wrong relationship, and V7 cannot tell those apart either.
* Not that the schema work caused any of this. Every one of these predates today.
