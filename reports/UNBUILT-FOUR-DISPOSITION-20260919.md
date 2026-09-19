# The four capabilities with no RTL — what each one actually is

2026-09-19. The completion register lists four mandatory capabilities with no
RTL at all: `GEOM.LOOM`, `INPUT.SNAC`, `GEOM.WARP`, `POST.ECHO`.

**They are not four of the same thing**, and this document exists because the
difference changes what "zero gaps" should mean. Nothing here changes the
register. The count still says four, and it should keep saying four until the
owner rules, because a reduction taken on my own judgement is precisely the
failure this instrument has now been repaired against five times in one day.

## Why they are all counted as mandatory today

`design/blocks.yml` records the revocation verbatim, on every one of them:

```
deferred: false   # REVOKED by the owner 2026-09-18: "I don't want to defer any
                  # unfinished blocks now". The 2026-08-31 SS6.3 cut is withdrawn.
```

That is unambiguous and it is why all four are in the count. What follows is not
an argument to re-defer them. It is the observation that **three of the four are
not missing FUNCTIONS — they are optional alternates for functions the console
already performs**, which is a different claim from "deferred", and one the
owner's own rulings make rather than one I am making.

---

## 1. GEOM.LOOM — genuinely mandatory, and being built

Owner ruling 2026-08-31 §6.4, in full:

> **Write the contract.**
>
> The ARM/compiler supplies a parent-before-child topologically sorted stream.
> Loom only composes transforms.

The contract is written (`design/contracts/GEOM.LOOM.md`, 276 lines, every
section filled). Nothing else in the tree composes a parent-before-child
transform stream. This is a real absence of a real function.

**Disposition: BUILD.** In progress.

---

## 2. INPUT.SNAC — the function is already performed

Owner ruling §6.6, in full:

> DEFERRED and optional.
>
> The MiSTer input path satisfies the base console contract. Direct PS1/SNAC is
> only built after a physical-board use case proves it worthwhile. **It must
> emit the same canonical PadFrame and may not create a second input
> semantics.**

Three things follow, and none of them is a scheduling preference:

* **The capability is present.** The MiSTer input path satisfies the contract —
  the owner says so. SNAC is a second ROUTE to the same PadFrame, not a
  capability the console lacks.
* **It is forbidden to differ.** "May not create a second input semantics"
  means a correct SNAC block is observationally identical to what exists.
* **Its precondition is unmet and cannot be met from here.** "Only built after a
  physical-board use case proves it worthwhile", and
  `zhaozhou-board-bringup-20260913/reports/board_truth.json` records
  `futurePhysicalLoadsAuthorized: false`.

Its contract is also DELIBERATELY BLANK — every section reads "Deliberately
unwritten", with the reasoning stated there: *"Specifying clocks, packets,
throughput and test plans for a block nobody is building would make the design
look decided when it is not — the same error as building it."*

So building it means first AUTHORING a specification the owner deliberately
declined to write, then implementing a second path to an output that already
exists, for a peripheral that is not authorised to be attached.

**Recommendation: record as capability-present-via-INPUT.SNAPSHOT.** Owner's
call.

---

## 3. GEOM.WARP — an accelerator for work already done

Owner ruling §6.3, in full:

> DEFER dedicated v1 hardware.
>
> Current real needs are covered by:
>
> * the bounded fixed creature-deform path;
> * Loom transforms;
> * HPS/PC preprocessing where necessary.
>
> Keep GEOM.WARP as an optional later accelerator, cut-order 5. Do not allow it
> to block conventional geometry or creature completion.

"Current real needs are covered by" names three existing paths. GEOM.WARP is
faster hardware for a result the console can already produce — the ruling's own
word is **accelerator**. Its contract is DELIBERATELY BLANK for the same
reason INPUT.SNAC's is.

Note the dependency direction: one of the three covering paths is **Loom
transforms**, which is item 1 above and is being built. Building GEOM.LOOM
strengthens the case that GEOM.WARP is redundant, not weakens it.

**Recommendation: record as capability-present-via-GEOM.LOOM + the
creature-deform path.** Owner's call.

---

## 4. POST.ECHO — explicitly not v1 silicon

Its contract's own first line under Purpose:

> **DEFERRED — owner ruling 2026-08-31 §4.** Not part of the v1 silicon.

and `reports/OWNER-RULINGS-20260831.md:133` lists it with the cut set. It is an
optional echo of the composited frame back to a capture buffer, described in the
ledger as "first on the §26 cut list". The frame is already composited and
already scanned out; the echo is a capture convenience.

**Recommendation: record as non-v1 with the ruling cited** — the goal's own
carve-out is "only explicitly deferred/non-v1 features may remain absent, and
each must cite the controlling ruling/spec", and this cites one in its own text.
Owner's call.

---

## What this costs, which is the part that matters for Phase 2

The console measured **47,582 ALM against a 41,910 budget** — 5,672 over, before
this session's compositions. Building three blocks whose functions the console
already performs spends area in the one direction Phase 2 cannot afford, and the
goal is explicit that resources may never be reduced by removing function. The
converse is also true: **adding duplicate function is the most expensive way to
be complete.**

Against that, the goal is equally explicit that only deferred/non-v1 features may
remain absent and each must cite its ruling. All three cite one. That is the
whole of the argument, and it is the owner's to settle.

## What I did NOT do

I did not change the register, the ledger, or the alias table. The count reads
70 with four unbuilt. I started to restore cited deferrals for these three
earlier today, found the revocation recorded in the ledger in the owner's own
words, and reverted it within the minute. A number moved by my judgement on a
question the owner has already answered once is worth less than no number.
