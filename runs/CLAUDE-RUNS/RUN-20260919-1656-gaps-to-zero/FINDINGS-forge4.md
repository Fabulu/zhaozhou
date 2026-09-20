# FINDINGS — FORGE4 (the four forge modules, and the cliff decision)

**Branch `gz/forge4`, three commits, head `2cbd3f12`. Register 21 → 21.**

> **TRANSCRIBED BY THE COORDINATOR.** The harness refuses a subagent writing a
> report `.md` — the fifth lane in a row. The packet noted that this refusal is
> **already recorded inside `zhao_console_core.sv`**, as the cause of a citation
> pointing at a `FINDINGS-forge.md` that was never written. **A known tooling
> gap has now produced a dangling citation in production RTL and cost five lanes
> their report file.** That is worth fixing at the harness, not working around a
> sixth time.

---

## THE RESULT: ADOPT `zhao_forge_cliff_ram` — 5,698 ALM, 13.6% OF THE DEVICE

**The largest area result of the campaign, and it required no new engineering.
R142 ruled "adopt" and nobody executed it.**

`prod_manifest.yml`'s row named its own discharge condition — *"adopt … only
after that gate"* — and **the gate had already run twice.** `CLAUDE.md`'s
uncashed cheque, exactly: the knowledge was written down, correctly, by someone
who knew what they were deferring, and nothing ever read it back.
**`uncashed_cheques.py` was reporting it correctly the entire time.**

FORGE4 verified from the **primary receipts**, not from the ruling — both
`.sources.sha256` digests (`445a89ba…`, `1d6c6d07…`) match the files in its tree
exactly, same device `5CSEBA6U23I7`:

| | golden | RAM candidate | delta |
|---|---:|---:|---:|
| fitted ALM | 6,674 | 976 | **−5,698** |
| fitted registers | 4,025 | 939 | **−3,086** |
| DSP / RAM blocks | 2 / 14 | 2 / 15 | — / +1 |

It also **re-ran the equivalence half rather than quoting it**, on the correct
grounds that R142's fit settles *area* and not behaviour:
`forge_cliff_ram_differential` built and run — **246 lattices, 752 pages, 0
mismatches**, every coverage counter nonzero, reproducing the 2026-09-10 cycle
figures to the digit.

### Why nobody saw it: a THIRD register blind spot, pointing the other way

**`successor_in()` requires a module's tail to `fullmatch` `v\d+`.** `_ram` is a
**RIVAL**, not a version — so the candidate was **structurally invisible** to the
completion register, and `FORGE.CLIFF` went on quietly resolving to the
6,674-ALM module.

R159 found the register blind to an **undeclared tie-off** (hiding a *gap*).
This is the same register blind to a **competing implementation** (hiding a
*saving*). The `implementation:` line in `blocks.yml` is therefore load-bearing
and not documentation.

Four ledgers moved together — `prod_manifest` `top:`, `console_inventory`
`superseded_by`, `blocks.yml` `implementation:`, `fit_targets` source list —
plus a regenerated `zhao_prod_top.sv` **whose output was read** (R151/R161):
`u08` is the candidate and its extra `walk_fault_o[7:0]` is connected and
folded, not dangling.

### Accounting, stated so it is not misread

`superseded check` goes **73 → 72** production roots with **70 → 71** further
fit-target tops. **Total unchanged at 143, both lists CLEAN.** One module
changed category; no coverage was lost.

---

## FOUR REFUSALS, EVERY BLOCKER RE-MEASURED

**ADOPTION IS NOT COMPOSITION**, and the packet says so plainly — which is why
the register does not move.

* **FORGE.CLIFF** — no lattice-walking page issuer. `solid\w*_o` as an *output
  port* across all of `fpga/rtl` including `synth/` and the probes: **zero
  hits.** `vdist` in exactly four files, all the wrong end.
* **FORGE.SHADOW** — refused under **standing owner instruction R133
  (D-FORGESHADOW-B)**. All four blockers re-verified live, and note the method:
  the chain was checked **by instantiation**, not by grep, because *every hit in
  the core is prose*. `cast_strength_i`'s only driver is the pricing LFSR;
  Route A still deadlocks at `zhao_geom_vattr.sv:553`; `tri_continuation_tail_i`
  is still a boundary.
* **FORGE.PRIM / PRIM_EVAL** — **the ABI half EXPIRED.** The core said, in
  capitals, that `forge_kind` has *"EXACTLY ONE member"*; **R108 granted five
  more on 2026-09-20**, and the citation `:133-136` had rotted to `:177`.
  Corrected in place. The **page half is live and sharper than the record
  claimed**: there is no forge program page kind in `cartridge.md` §4, and
  `handle32[forge_program]` occurs in exactly **two** places tree-wide — that
  one command and its generated ABI table.

---

## NEW FINDING: GEOM.SETUP's DOOR DOES NOT EXIST, and `upstream:` is not a wiring claim

No earlier pass measured the far end of this edge.

`zhao_geom_setup` **is** composed, has **one** triangle arm, and that arm is
**fully occupied** by GEOM.CLIP through the ATTRPACK fork **with no arbiter** —
and it is **screen space** (`signed [20:0]`), while PRIM emits index triples,
PRIM_EVAL world fx16, and the cliff a rim edge.

**`zhao_part_expand` is already composed, already emits the right shape
(`signed [21:0]`), and already leaves the core as boundary I24 for want of that
same door.**

So `GEOM.SETUP`'s `upstream:` names **six producers, of which one is a port** —
the **third** ledger-edge-read-as-a-port in this single entry. The conclusion is
the general one: **in this ledger `upstream:` is DESIGN INTENT, not a wiring
claim**, and reading it as a claim about the tree has now misled three separate
passes. Recorded on the `blocks.yml` row where the claim lives, and summarised
in the core.

---

## GATES at `2cbd3f12`

All thirteen green — inventory, prod_manifest, quartus17_syntax, case_labels,
`mutant_copy_drift` (run **after** commit, R121), mutant_drivers,
uncashed_cheques, check_counters, refmodel_liveness, all three `--check`
generators fresh, tie-off audit unchanged. `completion_register` RC 1 = 21 gaps,
which is normal. **All six smoke forms PASS** at the final commit
(`raster pixels=2560`, `frames_admitted=1`; mutant fired once; bad-vertex,
no-echo and bad-trace-arm controls all behaved) **and separately at `c252f379`**
before the comment-only commit.

On R172: it ran the corrected regex **from a scratch copy**, leaving the shipped
tool untouched so as not to conflict with the coordinator's fix. Result: the
core reads **identically, `7/1/10/1`** — so **the signed-literal blindness costs
zero on `zhao_console_core.sv`** and R172 does not move that ctest's baseline.
It also confirmed the four wrong gate paths **independently, by running them**,
before the coordinator's correction arrived.

---

## FOR THE COORDINATOR

1. **OPEN OWNER DECISION — a forge PROGRAM PAGE KIND**: a frozen layout in
   `spec/cartridge.md` §4 plus a staging path on the terrain pattern. **R108
   left this open explicitly.** It unblocks FORGE.PRIM; PRIM_EVAL would still
   cover one family of six.
2. **`BUDGET_HEATMAP.md`'s #1 ALM row is `zhao_forge_cliff` at 7,664 / 18.3%** —
   a module the console has now decided **not to ship**. It is generated by
   `build_manifest.py`, which has **no notion of `prod_manifest`'s `superseded`
   rows**, so the packet flagged it rather than hand-editing a generated file.
   **This is pre-existing, not introduced here**: `zhao_terrain_bake` and
   `zhao_shell_top` are already in those tables the same way, both superseded on
   2026-09-19/20. In the packet's words, *"it is wrong in the direction nobody
   questions"* — the budget's headline target is a module already dealt with.
