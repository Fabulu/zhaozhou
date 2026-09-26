# REFPUSH — I56 at the seam GIANTQUOTA measured, and the ABI problem may dissolve

**Branch `gz/refpush`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**GIANTQUOTA refused this entry on 2026-09-26 and was right to.** It was sent to
build a four-item list I wrote; **two of the four are measured false**, and
building them as written would have put the reservation at a seam that cannot
carry it and re-invented a floor that already ships. **Its refusal is your
specification.** Read the `I56` entry (`grep -n '^// I56\.'`, never a line
number) — its last third is GIANTQUOTA's, with file and line for every claim.

## What it settled, so you do not re-derive it a third time

* **A chunk has NO owning instance, and the severing is deliberate.** The binner
  walks tiles in raster order and drains each tile's FIFO in frame-wide
  **triangle submission** order — the painter's algorithm requires it — and
  `zhao_geom_chunkser` flushes on staging-full or last-ref-of-tile, inspecting no
  identity. **A 14-id chunk routinely straddles two instances.** The binner holds
  `tri_src_id_i` and exports it on the **raster** port; the serialise port reads
  a different register. A class bit there would have to be **fabricated**.
* **The correct seam is the BINNER'S REFERENCE PUSH**, where identity survives
  and which is the **directive's own unit**. Refuse ORDINARY reference pushes
  past `(ref_cap − giant_reserve)`; admit the giant's. **The arena then needs no
  class bit at all**, and the chunk count follows, because chunks are only
  serialised references.
* **The demotion floor ALREADY SHIPS.** `zhao_forge_shadow.sv:255` is literally
  `max(ladder, floor)` with a live floor from MEASURE.GOVERNOR's `deg0_o`/`deg1_o`
  (`core:15165`). It is **per-camera**; item (3) wants **per-instance**. **Copy
  the proven pattern; do not design one.** (The comment near `core:10858` saying
  `deg0_o`/`deg1_o` "name NONE" is STALE. What is genuinely unwired is
  PART.LADDER's `p_gov_floor_i`.)
* **`ck_fits_c` now has a positive control** — GIANTQUOTA's directed case 3b,
  counter 1→2 with a negative control beside it, 345/345. **That is the exact
  expression a reservation modifies**, and before that packet it had never been
  seen to fire anywhere in the tree.
* **Rung demotion DOES reach tile references.** GIANTQUOTA drafted the opposite,
  ran a sweep against its own claim, and falsified itself: shadow hulls are
  submitted geometry on the same path to the binner. The surviving limitation is
  narrow — **no rung selects a coarser creature MESH.**

## THE ONE THING IT DID NOT DRAW, AND IT IS YOURS TO CHECK FIRST

GIANTQUOTA concluded *"R7 rules no number for the vertices/descriptors/metadata,
so the reservation CANNOT be a hardware constant"*, and therefore declined an ABI
change on the sound ground that **a field whose only consumer is an enforcement
seam that does not exist is an uncashed cheque with an ABI's blast radius.**

**That was measured at the SEAL, where the bundle is chunks AND verts AND
descriptors.** At the **reference push** the unit is **references** — and
**32,768 references is precisely the number R7 rules.**

**So my reading is that the reference reservation IS a hardware constant, and no
ABI field is needed at all.** That would remove the whole item (4) and its blast
radius.

**CHECK IT BEFORE YOU BUILD ON IT. It is my claim, not a measurement**, and this
campaign's briefs — including the one GIANTQUOTA just refuted — have a poor
record on exactly this kind of confident one-liner. **If reserving references
alone does not actually guarantee the giant** — because the arena can still
exhaust verts or descriptors while references remain — **then the constant is
insufficient, say so with the measurement, and the ABI question is live again.**
You have `npm run abi:check` in your gates if it is.

## The job

1. **Reserve at the reference push**, with the instance identity the binner
   already holds.
2. **The per-instance rung floor**, copying `zhao_forge_shadow.sv:255`'s
   `max(ladder, floor)` into `zhao_geom_lodstate`, whose per-instance rung with
   hysteresis under R74 already exists and **is composed** (`u_geom_lodstate`).
3. **The selector** — a streaming max over `{semantic_weight, instance_id}`,
   highest weight wins, lowest instance id breaks the tie. **One comparator, NOT
   a priority heap**; charter §9 forbids it and `zhao_measure_tokens` refuses it
   by name. `semantic_weight` already rides the meshlet and **nothing reads those
   eight bits.**

**If (1) lands and (2)/(3) do not, that is a good packet** — the reservation is
the law the directive names.

## The fences

* **THE GUARANTEED GIANT MAY NOT BE TRIMMED, RESCOPED OR REDEFINED.** The
  owner's directive lists *"shrink the guaranteed giant"* among what this
  campaign's authority does not cover. **32,768 references is the number.**
* **Do not renegotiate a sealed frame**, and **do not borrow from the giant's
  reservation because an early primitive happens to fit** — the directive's own
  words. A frame explicitly containing **no** guaranteed giant may release the
  reservation **before** sealing.
* **Illegal or overflowing plans are REFUSED before the seal**, not clamped after.
* **Do NOT half-build it.** If you cannot finish, **leave the seal at capacity**
  — the neutral, declared choice — and report why. An unconnected quota block
  moves this entry from a tie-off to a DISCONNECTED IMPLEMENTATION, which the
  register counts as a gap either way.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`.**

## Evidence bar

* **The reservation ENFORCED, not asserted**: a frame whose ordinary allocation
  would eat the giant's reserve **refused, with the giant still whole**, and the
  counter that fires. GIANTQUOTA's case 3b is your starting point.
* **The floor actually demoting** a real instance through the composed
  `zhao_geom_lodstate`, and — since rung reductions **do** reach tile references
  — **the reference count moving as a result.**
* **The selector picking the right object under a TIE**, so "stable instance
  identity" is exercised and not merely present.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison.** A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — it
  classifies by scanning prose.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* leaves `$LASTEXITCODE` carrying the previous command's value.
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`; a
  hashtable splat through `powershell -File` stringifies the switches so they
  never start. That has produced false GREENS and false REDS here on one day.
* **`zhao_geom_binner_v2` is a SHARED, HOT file.** Stage your hunk, not the file
  — `git add <file>` sweeps in work you did not write, and `git checkout --`
  destroys it with no reflog.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable** — Quartus 17.0 needs an
  elaboration check inside `initial begin ... end` and explicit
  `generate`/`endgenerate`.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I56` closed.
2. **Whether the reference reservation is a hardware constant** — my claim above,
   confirmed or refuted with a measurement.
3. **The enforcement evidence**: the refused frame, the giant intact, the counter.
4. **The units**, explicitly: references, chunks, and what the reservation covers.
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one, most of them mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.** GIANTQUOTA ran a sweep against
   its own draft finding and killed it; that is the standard.
8. Branch and commit hash. **Push `gz/refpush` only.** Never `--force`.
