# Packet queue — what fills the next free slot

Ceiling is THREE concurrent packets. When one lands, the next one down starts.
This file is the coordinator's, and it is a WORK LIST: delete a line when the
gap it names is closed, never when it is merely attempted.

**Rewritten 2026-09-20 evening (third revision today).** A stale queue is worse
than none, because the next free slot gets filled from fiction. The previous
version was accurate for about four hours.

**Register: 21** = 9 tie-offs + 11 disconnected + 1 unbuilt. Measure it yourself
with `python tools/budget/completion_register.py`; this line goes stale.

## THE PRE-FIT BLOCKERS ARE GONE

`superseded check: 71 production roots CLEAN`. R86's check reads **zero** —
ATTRDIV cleared the last one by adopting `zhao_raster_attrdiv_v2` at both sites.

Of the **seven** preconditions in `reports/FIT-PLAN-AT-ZERO.md`, **exactly one
is unmet: the register itself.** Clean tree, manifest, generators, worst paths,
syntax and census are all in place. **The gaps are now the only thing between
this run and the fit**, which is a different situation from this morning and
should change how the next slots are spent.

## Running

*(Updated 2026-09-20 late evening. The previous table listed three lanes that had
all landed -- exactly the fiction this file exists to prevent. Verify against
`git ls-remote origin` before filling a slot from it.)*

| lane | owns | branch |
|---|---|---|
| FIELD-H1 | the new host: FH02/03/05/06/08/09/20, **R126's elaboration guard**, and the `sat_o` widening `rcp0` needs | `gz/fieldh1` |
| FIELD-D1 | doorbell + loader: FH13/FH14, the `cfg_plan_base_i` live-pin defect, and the doorbell mutant refresh | `gz/fieldd1` |
| FIELD-A1 | adapters: FH17/FH26, the two named stamp bindings, and GEOM.WARP's P2 warp adapter | `gz/fielda1` |

**Landed today:** field, geomlod, texmat2, terrain6, projinput, post3, terrain7,
forge, carriers, warp, projadopt, forgeconnect, fieldp4, terrain9, post3b,
attrdiv, geompay4, engine1, forgeshadow, zidl, and FIELD Wave 1 (S1/L1/F1).

## Queued, in the order I would start them

### 1. ~~The zidl bundle~~ -- LANDED 2026-09-20 (`968d414e`)

R108's five additive `forge_kind` members, the DebugTraceArm over-broad
guarantee and R77's `tmu_mode` comment, all in ONE commit. All five golden
captures regenerated through their real producers and verified byte-wise to
differ **only** in the container CRC `[56..59]` and the two 32-byte sha fields.
`abi_version` unmoved, `abi:check` clean.

**Kept here rather than deleted, because it carries a landmine worth
remembering: `forge_kind` is NOT `j_family_i`.** They differ by
`(FAM_* + 1) mod 6`, so a straight-through assignment is silently wrong for all
six values. The mapping table is in the zidl.

**Still open: D-ZIDL-1** -- the five-for-six arithmetic works only if member 0
already names a family, and nothing written says which. Recommendation accepted
as implemented; if the owner rules otherwise, cliff/skirt appends at `6` under
the same additive law, no renumber, nothing shipped changes.

### 2. FORGE.CLIFF adoption — R117, and the data is already in hand

**Do not re-run F-CLIFF1. It ran on 18 September** — a full fit on the target
part, `.sources.sha256` matching the current source. Five inferred memories,
MLAB bits 0, **976 ALM fitted, 2% of device**, against a golden that is
**18.3% of the entire ALM budget**.

Adoption is blocked on exactly two named items, neither of them a gate to run:

1. the **four `Warning (276020)`** pass-through insertions — the gate demanded
   `ramConversionWarnings 0`, so either accept them in writing with their cost
   or match the RAM's native read-during-write behaviour;
2. the **bit-0 inferred latch** on `triangles_submitted_o` — cosmetic (the
   counter increments by 2, so bit 0 is provably constant), but a latch cell is
   real area.

Then `console_inventory.yml:109-127` stops giving the two rivals the identical
boilerplate disposition, and the golden becomes `superseded`.

**And `zhao_forge_cliff` needs its own FITTED row (F-CLIFF-GOLDEN) before anyone
quotes a saving** — 976 is a fit and 7,664 is an unfitted estimate, and setting
those against each other is the mismatched comparison this repo keeps landing in
the flattering direction.

### 3. FIELD Wave 2 — requires S1 + L1 + F1 all merged

**D1** (doorbell and loader; FH13, FH14; and it must **refresh
`zhao_field_doorbell_mutant.sv`**, which has already drifted once today),
**H1** (the new host; FH02/03/05/06/08/09/20 — and it owns **R126's elaboration
guard** tying `ZFH_WINDOW_MASK_BITS` to composed `OUT_LANES`), **A1** (adapters;
FH17, FH26).

Full briefs are cuttable from `reports/FIELD-REPAIR-PLAN-20260920.md` §3.

### 4. FIELD Wave 3 — E1, W1, then C1

**E1** closes **I34** — but only with a **port change**, because
`zhao_terrain_patch.sv:154-156` has `fld_valid_i / fld_ready_o / fld_height_i`
and **nothing else**, so wiring only height is the only thing the current ports
permit. Its brief must open with `zhao_console_core.sv:2732`'s capitalised
warning to read `fpga/rtl/synth/zhao_probe_walk_earth.sv` first — that walker
already exists, is field-major, is differentially tested and **already emits the
corrected 297 groups**, and the owner's directive never mentions it.

**W1** closes **GEOM.WARP**, and only if it composes live. **A tie-off does not
count and must not be attempted** — that converts an honestly-absent entry into
a tie-off, which the warp lane already refused once.

**C1** is coordinator-owned, serialised, never concurrent: core, prod_top and
board regenerated, the three yml files, the smoke bench, `tests/CMakeLists.txt`.

### 5. The remaining boundary tie-offs

**I13/I14** (PROJ — I14 needs the smoke's viewport case moved to DUO with the
reference-derived pixel count regenerated in the SAME commit, both numbers
stated, plus a `proj_en_i` producer), **I17** (POST — now priced at 153/553
M10K, 27.7%, see R120), **I20**, **I21**, **I27**, **I29**, **I32**.

### 5b. LEDGER DEBT surfaced today -- two small packets, neither on the critical path

**(a) 153 undeclared counter ports (R140).** `check_counters.py` now looks both
ways and reports **153 self-incrementing 32-bit outputs across 35 blocks that no
ledger row names**, against 252 that it does -- roughly 38% of the counter
surface. They are QUESTIONS, not defects: either the ledger owes each a name, or
it is not a counter and the row is noise. Needs one pass with the contracts open.
`design/counter_catalog` is APPEND-ONLY with ids equal to positions, so **exactly
one lane may hold it**.

**(b) The six unresolved `reference_model:` rows (R139).** The expensive half is
done: `zref::part::` and `zref::post::` DO exist and already serve PART.LADDER,
PART.EXPAND, PART.SOFT and the grade/echo paths. The six that do not resolve name
laws that are **genuinely absent, not renamed** -- no collision, spawn or
integration law in `zref::part::`, no composite in `zref::post::`. Five look like
removals with a stated reason; **PART.STATE could go either way**, because
`particle_pack`/`particle_unpack` may be a FORMAT rather than its state law. That
one needs the contract in hand, and guessing it in the "name a plausible symbol"
direction is exactly the defect R94 exists to prevent.

### 6. Whatever the running three refuse

Every packet that refuses must name its exact blocker. **Those blockers are the
real queue and they outrank this list** — and they have been right more often
than this page has.

## Owed to the OWNER — five, and R65 is the expensive one

1. **`reports/terrain-seam-dig/seam_dig_contact.png` (R65).** **One look
   unblocks four packets.** It gates I32, TERRAIN.BAKE's option A and the
   terrain page format. **Six terrain lanes have closed zero of the same four
   blocks** — that is one blocker seen six times, not six failures. The packer
   does not exist yet, so the format is cheaper to change now than it ever will
   be. And the render changed the question: spec §9.3(c) says the half-cell step
   "does not read as a seam"; the sheet shows a rim wrong by up to one vertex,
   everywhere. Under the art law only the owner's look settles it.
2. **FH22 sequencing.** `REGS=64` doubles `E_ZERO` from 32 to 64 clocks on a
   path already at 481% of allowance. FH08 dissolves the reason — but only after
   H1 lands, so **the intermediate state is worse than either endpoint.**
3. **FH11 lane width.** Semantics adopted in F1; the width is ~+6,000 ALM and
   +12 DSP against a budget already ~5,672 ALM over, and
   `zhao_block_fit.json`'s console row **does not contain FIELD at all**.
4. **R115.** TERRAIN.NORMALMAP has a contract, a ledger row, an oracle and a
   4,738-check suite and **no ratified spec sentence**. Ratify or supersede —
   writing one now to match the implementation would be ratifying whatever got
   built.
5. **`reports/post-gather-law/gather_law_contact.png` (R37).** Blocks
   `zhao_post_gather` and I17, and nothing else.

Plus, lower stakes: the six unresolved `reference_model:` rows
(`zref::MeasureHistogram`, `zref::PostComposite`, four PART.*) each need the
per-row call R94 defined — name the law that exists, or remove the key and say
why. **Inventing a plausible symbol is the same defect with a better name.**

## The fit

`reports/FIT-PLAN-AT-ZERO.md` is the plan and it names the gates in advance.
Three runs: **F-CLIFF-GOLDEN** (a leaf fit, the only honest way to state the
cliff saving), **F-CONSOLE-TARGET** (the verdict on `5CSEBA6U23I7`) and
**F-CONSOLE-SIZE** (the map on `5CEBA9F31C7`, because **a refusal is not a
map**).

The only composed number that exists is 47,582 ALM / 151 DSP / 306 M10K on the
sizing device, from a **dirty tree** carrying a live metadata-swap defect,
before this run's repairs. **It is a starting estimate. Do not quote it as the
console's size.**
