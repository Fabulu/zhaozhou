# What survives an architecture remake, and what does not

Written 2026-09-18, immediately after owner notice that a large architecture
document is inbound and *"we might remake any of what we're making here very
soon."*

The point of writing this BEFORE the document arrives is that afterwards it is
not possible to write honestly. Once a new architecture is on the table, every
piece of today's work acquires a reason to be kept.

## Survives regardless — these are facts, not plans

| | why it survives |
|---|---|
| **`@whole-console-sizing`** (in `quartus_map`, 107 min) | A measurement of the machine as it stood at source digest `de21d0f5a7d1`, 147 files. A new architecture does not change what today's machine costs. This is the denominator the whole ALM question needs and it is the single most expensive thing in flight — **do not cancel it.** |
| **Defects fixed in shipped blocks** | A truncating `7'(SPECIES_N)` comparison, a counter that claimed a loss its contract forbids, an `age` mask 3 bits short of the ruled width. These are wrong in any architecture. |
| **The C2 banners** on `PART.SPAWN.md` / `PART.UPDATE.md` | A ruling from 2026-09-02 that two contracts still contradicted. True whoever builds the blocks. |
| **`tools/budget/refmodel_liveness.py`** | Resolves ledger claims against the oracle. A new architecture makes MORE phantom references, not fewer. |
| **The disposition table** in `MISSING-ORGAN-REGISTER-20260918.md` | `deferred` / `cut_order` / `blocked_on` read mechanically per block. Re-runnable against any ledger. |
| **The finding that GEOM.WARP is not the client-A producer** | A reading of `GEOM.PROJECT.md:67` and `prod_manifest.yml`. It corrects a claim, and corrections do not expire. |

## Does NOT survive — do not defend it

* **The five RTL blocks built today** (four `zhao_part_*`, `zhao_geom_group_seq`)
  and whatever GEOM.LIGHT and POST.COMPOSITE come back as. They implement
  specific contracts; a remake can replace any of them. They cost hours, not
  days, and they are **adopted nowhere** — zero references in
  `prod_manifest.yml`, `fit_targets.yml` and `zhao_prod_top.sv` — so discarding
  one costs a `git rm` and nothing else. That isolation was an accident of
  sequencing and is now an asset.
* **The build order.** Already wrong twice today (SYS.PLL/RESET are blocked on
  hardware, not schedulable; GEOM.WARP was neither the bottleneck nor buildable).
  It has no claim on anyone.

## DELIBERATELY NOT STARTED, because of this notice

**GEOM.WARP, INPUT.SNAC and POST.ECHO need their contracts AUTHORED** — all
twelve sections of each read "Deliberately unwritten". That is the most
speculative work on the queue and the most expensive to throw away: writing
clocks, packet layouts, throughput targets and test plans for three blocks, any
of which an incoming architecture may define differently or delete.

**So it is parked, not forgotten.** The owner's revocation of the deferrals
stands; what waits is only the spec-authoring, and only until the document is
read. The five blocks that already HAVE contracts are unaffected.

`MEASURE.HISTOGRAM` and `FORGE.SHADOW` are also held — both have written
contracts and could start now, but starting them buys little against the chance
the dump reshapes the subsystems they sit in.

## When the document arrives

1. Read it before acting on any part of it. CLAUDE.md: *"Instructions are not
   delivered until they are read"* — owner direction has been posted four times
   and missed before.
2. Index it in `reports/OWNER-DOCUMENT-INDEX.md` **with a disposition**. That
   index currently lists 33 owner documents of which **20 have no recorded
   disposition**, and its own sentence is the law: *"an unread instruction and a
   satisfied instruction look identical from here."*
3. Diff it against the table above. Anything in the first table it contradicts
   is worth arguing about; anything in the second, let go of immediately.
