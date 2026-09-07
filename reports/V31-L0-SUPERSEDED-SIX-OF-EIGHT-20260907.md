# §10's L0 inventory is superseded: six of the eight arrays now infer

*2026-09-07, read mid-flight from the island refit's map stage. This corrects
`ISLAND-L0-ARRAY-INVENTORY-20260907.md`, written this morning.*

---

## What L0 said, and why it is now wrong

The morning's L0 walk found all eight named arrays in
`zhao_texture_island_top` single-writer/single-reader, and concluded:

> **Continuous assigns — asynchronous array reads.** That is exactly why Quartus
> reports all eight as *"uninferred due to asynchronous read logic"*, and why the
> 9,568 declared bits sit in fabric instead of an M10K.

That was true of the **Sep 6 fit**. It is no longer true of the tree.

| array | previous fit | running refit |
|---|---|---|
| `fctx_m` (64 × 64) | uninferred | **RAM, 4,096 bits** |
| `flod_m` | uninferred | **RAM, +256** |
| `fpgn_m` | uninferred | **RAM** |
| `fcls_m` | uninferred | **RAM** |
| `fpsl_m` | uninferred | **RAM, +128** |
| `faux_m` | uninferred | **RAM** |
| `uvw_m` | uninferred | **still uninferred** |
| `class_m` | uninferred | **still uninferred** |

**Six of eight have converted.** The map-level effect is exactly the shape §10.1
predicted: **−4,923 registers, +5,312 block memory bits**, and the entity walk
puts the whole register drop in `zhao_texture_island_top` itself rather than in
any child — which is where those arrays live.

## The mistake worth naming

L0 read the **current source** and cited the **previous fit's** uninferred list.
Both halves were individually defensible and the combination was not: it is
CLAUDE.md's *"never compare a current file to an old measurement"* wearing a new
costume, because the comparison was implicit rather than written as one.

The tell was available and unused — `compare_rows.py` already refuses sums
containing rows stale against HEAD, and the island's row was **four commits
stale**, a fact recorded in `V31-ISLAND-BUDGET-BLOCKED-20260907.md` on the same
day. The staleness was known; its consequence for L0's conclusion was not drawn.

## And it was not today's work

Six of the island's fifteen sources changed across ten commits since that fit,
four of them from earlier sessions (`b55959f0`, `d80f29b4`, `3a06a590`,
`a1846867`). Today's contribution to this island refit is `perspuv_svc`, whose
own fit measured registers **up** 59 with M10K unchanged — so it is positively
excluded as the source of the memory rise.

**The conversion is somebody else's win, measured here for the first time.**

## What this means for §10's remaining work

The two survivors are the interesting ones:

* **`uvw_m`** is literally §10.2's subject — *"the old top combinationally selects
  `uvw_m` using a returned RCP token and feeds PERSPUV with the reciprocal
  currently on the service pins"*. Its read is
  `uvw_m[rcp_tok[FCTXW-1:0]]`, and §10.2's credited N0–N4 join exists precisely
  to give that read a registered destination without pairing yesterday's U/V with
  today's mantissa.
* **`class_m`** has the same asynchronous-read shape.

So §10's storage lane is **further along than the handoff's L-series assumes**,
and what remains is not eight arrays but two — one of which already has a
designed solution written out stage by stage.

## What is still unknown

ALM and Fmax; the fitter is still placing. And whether the six conversions cost
**M10K blocks** disproportionately — §12's lesson today was that four-deep queues
each burn a whole block, and `fpsl_m`/`fcls_m` are narrow. The completed fit's
RAM summary answers that, and it should be read before anyone calls this a clean
win.
