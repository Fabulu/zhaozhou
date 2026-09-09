# The terrain brief, read at last — and it says the same thing the current directive says

2026-09-09. Acknowledgement and gate record for
`reports/TERRAIN_31MHZ_REARCHITECTURE.txt` (1,437 lines, commit `1fc4ac8a`,
2026-09-07), which is **the newest owner document in the repository** and had
never been read on this lane.

Read via `git show origin/main:reports/TERRAIN_31MHZ_REARCHITECTURE.txt`. The
working tree was not touched and `origin/main` was not merged — it carries 302
commits of other lanes, which is not the direction job's business.

## Why it went unread for two days

`tools/maintenance/pull_direction.ps1` matched direction-shaped filenames against
`OWNER-DIRECTION`, and the manifest that would have named this file is called
`OWNER-DOCUMENT-INDEX.md`. One noun apart, so the tool printed *"no
direction-shaped filenames differ from HEAD"* every thirty minutes.

The manifest itself surfaced only through the tool's commit-**subject** scan, and
only by luck: its subject is `Add an owner-document manifest: every "Agent please
read" file, chronologically`, so it matched on a **quoted** occurrence of the
phrase rather than on an actual request. A detector that works by accident is not
working.

Both halves are now fixed — the pattern is widened, and the subject scan **names
the files and marks each `present here` or `ABSENT HERE`** instead of printing a
recipe for the reader to run. It immediately listed five owner documents absent
from this lane, this brief among them. That is the third defect found in that one
file today; see `TWO-GITS-DISAGREE-ABOUT-CLEAN-20260909.md` for the first two.

## Its first instruction is the one already in force

The document's opening lines, before any section number:

> Agent should focus on finishing the texture island first.
>
> Do not interrupt that work, move its acceptance criteria, or consume its active
> Quartus lane for this task. This is the queued TERRAIN rearchitecture to
> execute after the texture island is finished.

and §16, Step 0:

> **Step 0** — Finish the texture island. No diversion of that closure effort.

So the newest owner document, the memory-first rescue brief §0.2, and owner
direction `49fc32e9` all sequence the same way, independently. **Terrain hardware
is not startable work today**, and this is now the third written source saying so.

## Two accounting warnings — both checked, both already satisfied

§2 warns specifically against two errors. Both were tested rather than assumed.

**1. Do not add the obsolete 18-DSP `TERRAIN.NORMALS` row to the tessellator "to
claim that the pair mysteriously optimized most of its multipliers away."** The
census does not:

```
zhao_terrain_normals   3 DSP   newer MAP (bfc74710) supersedes an older fit that
                               says 18 DSP; fitted area and timing remain UNRESOLVED
```

It is listed under `SUPERSEDED FITS` with the caveat attached to the row, which is
exactly the disposition §2 asks for — the 18 is not used, and the 3 is not
promoted to a fitted number.

**2. Do not sum the pair wrapper with its leaves.** `design/prod_manifest.yml`
has `zhao_pair_tess_normals` in `excluded:` marked `probe  a leaf-fit pair around
TESS + NORMALS`, with `zhao_terrain_tess` and `zhao_terrain_normals` counted
individually in `top:`. Not double-counted.

## And one gap it names is already closed

§1 records:

> the four pair wrappers previously had no fit targets, so their poor frequency
> results were not being judged. A successful fit status therefore did not imply
> the product clock had been met.

True at its pinned revision `09b6b721`. **Now resolved**: `design/fit_targets.yml`
carries targets for all five pair wrappers — `tess_normals`, `tmu_cache`,
`fragment_tilestore`, `setup_binner`, `pagestream_patch` — plus standalone targets
for `zhao_terrain_tess` and `zhao_terrain_normals`. That came out of the
44-to-99-target authoring pass, before this brief had been read, so it is
convergence rather than compliance.

## The named fit gates, recorded in advance

`CLAUDE.md` requires a plan to say where its few fits are and what question each
answers. §15's comparison matrix is that list, and it is the reason terrain must
not be started casually — it is **six** fits, not one:

| gate | question it answers |
|---|---|
| **B0** | the retained 31.10 MHz receipt — historical sources + historical wrapper. Need not be rebuilt if its complete artifacts exist; missing ones must be **labelled missing, not reconstructed** |
| **B1** | current reviewed sources + historical wrapper — does the product-register repair already close 100 MHz? |
| **B2** | same DUT + improved characterisation wrapper — separates a **DUT** repair from a **harness** repair |
| **B3** | staged morph and ownership rewrite + improved wrapper |
| **B4** | normal operand/rescale and control-path changes, **individually attributed** |
| **B5** | production-representative composition with real memory/consumer seams |

B3 and B4 exist so that no single large patch gets credit for a path that
something else moved.

Further gate conditions, all of which bind:

* **At least three preselected placement seeds, every result published, not only
  the best.** "A best-seed 100.01 MHz headline is not robust evidence of an
  integration reserve."
* Classify **both endpoints and complete path ownership** at 100 MHz — real
  register-to-register, TESS→NORMALS across the actual seam, synthetic
  source/sink, wrapper-only, boundary, and reset/clock-control.
* **Do not simply delete every boundary path**; a declared external timing
  contract still has to pass. Equally, do not call an artificial pin delay the
  geometry core's clock limit.
* **No multicycle exception over registers that still change every clock.**
* Completion requires exact functional equivalence, reset/backpressure/
  conservation/drain tests, timing at the product clock, resources inside the
  approved budget, adequate measured throughput, **and a saved receipt that
  matches the implementation being claimed**.

Its stop conditions: do not silently waive arithmetic equivalence, approve your
own resource overrun, roof over voids, change stitch topology, or manufacture a
clean clock result by excluding the real failing paths.

## First action when texture closes

From §2 and §16 Step 1, and it is deliberately **not** a rewrite:

> refit the CURRENT pair with complete path capture before changing more logic.
> Preserve the old receipt beside it. If the existing product-register change
> already closes 100 MHz, the emergency is smaller than the old ledger number
> suggests.

The repair is **already in the tree** — `m_p_q`, `mp_v_q` and `mp_step_q` split
multiplication from accumulation at review revision `09b6b721`, against the
measured revision `39a650fc` where a signed 33x33 product fed a 67-bit
accumulator with no register between. §2 says explicitly: *"Do not reimplement
`m_p_q` as though it were missing."* So B1 may make most of the remaining
programme unnecessary, and it is one fit.

## What this document does not do

**It does not authorise starting terrain.** Its own Step 0 forbids that, and
nothing here changes the texture sequencing.

**It does not measure anything.** Its own header: *"Deliverable status:
architecture and independently executed Python models. NOT a committed RTL
change, RTL simulation result, or new Quartus measurement."* The 31.10 MHz row is
a valid historical result for the **measured** design and not a measurement of
the changed normal block.

**It does not resolve the texture redline.** The island's 3,336-ALM overage and
the four standing calls remain the owner's.
