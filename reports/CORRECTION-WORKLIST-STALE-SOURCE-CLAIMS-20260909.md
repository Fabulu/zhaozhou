# Correction worklist: source headers that assert withdrawn or false claims

Brief section 2.6.G. **This is a worklist, not a set of edits.** The brief is
explicit: amend HDL comments *"after the protected fit has released those bytes
or in an isolated future worktree"*, and a change to a generated top, manifest or
measurement runner can affect a queued run even outside the texture RTL cone.
Nothing here is applied yet.

The rule being applied: *"An old latency, 'nothing instantiates this', 'two
schedulers', or 'no fit exists' claim is not law merely because it remains in a
source header."*

## VERIFIED FALSE -- "Nothing instantiates this yet"

Checked by matching actual instantiation syntax, not mere textual mention,
because a file naming a module in a comment is not a caller.

| file | header says | actually instantiated by |
|---|---|---|
| `zhao_raster_perspuv_svc.sv:4` | "Nothing instantiates this yet." | **3** -- `island_top:788`, `island_v3_top:974`, `prod_top:2897` |
| `zhao_raster_rcp24_svc.sv:4` | "ORACLE. Nothing instantiates this yet." | **3** -- `island_top:537`, `island_v3_top:625`, `prod_top:2941` |
| `zhao_raster_texjoin_v2.sv:5` | "Nothing instantiates this yet." | **1** -- `prod_top:3025` |
| `zhao_texture_aux_div6.sv:4` | "Nothing instantiates it yet." | **1** -- `aux_pipe:309` |

The first two are the consequential ones. `zhao_raster_rcp24_svc` is the
reciprocal tile **inside both islands**, carrying the gate-4 comparison and the
whole DSP argument about `rcp24_v3`. A reader taking its header at face value
would conclude the block is dead code.

## STILL TRUE -- do not "correct" these

| file | claim | status |
|---|---|---|
| `zhao_terrain_residency.sv:5` | "Nothing instantiates it yet." | **TRUE.** Zero real instantiations. `zhao_terrain_residency_v2.sv` only mentions it in prose. |

Listed because a worklist that only records failures teaches the next reader that
every such comment is stale, and the next one might not be.

## Costs quoted from withdrawn totals

Searched `fpga/rtl/`, `design/` and the contracts for the withdrawn figures.
**Neither "154" as a DSP floor nor "~505 free M10K" appears in any HDL header or
contract** -- both lived only in the reports, which are corrected. No HDL edit is
needed on that account.

## Already recorded elsewhere, and I should have found it first

`reports/DOCKET.md:4659` already carries the normals withdrawal:

> `zhao_terrain_normals`'s 18 DSP predates `bfc74710 "one shared ..."`

So the obsolete-fit problem the owner brief identifies was **already known and
written down in the docket** before my census repeated it. The census read one
JSON file and no prose. That is the same failure as reading `status` without
`rtlCleanAtHead`: the evidence was present and the reader was too narrow.

## Not on this list

`zhao_texture_v3own.sv:1385` carries a stray 0x08 byte in a comment (a
backslash-b eaten by a shell). It is cosmetic, has no functional effect, and the
file is inside the `@g2-prod` measurement closure -- editing it breaks the
provenance tie between the freshest island measurement and the tree. It goes with
the next change that touches `v3own` for a real reason.
