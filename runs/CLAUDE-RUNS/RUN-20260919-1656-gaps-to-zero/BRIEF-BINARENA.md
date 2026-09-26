# BINARENA — I55: the owner already ruled WALKSWAP's blocker. Build the independent producer.

**Branch `gz/binarena`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is the largest remaining item in the campaign and it is ARCHITECTURE, not
a wire.** Budget accordingly. A packet that builds the producer and stops, with
the swap still refused and honestly declared, is a good packet.

## WALKSWAP refused this entry with measurements, and it was right to

Read entry `I55` in `fpga/rtl/prod/zhao_console_core.sv` (`grep -n '^// I55\.'`,
never a line number). Its last section is WALKSWAP's, 2026-09-26, and it is a
**measured** refusal — do not re-derive it:

* **BLOCKER 1 IS CIRCULAR.** The entry claims the swap *"removes the on-chip
  arena from the raster path"*. **It cannot.** `u_geom_chunkser` is the ONLY
  driver of `u_geom_paramarena`'s `ck_*` intake, and its only input is
  `zhao_geom_binner_v2`'s **serialise pass** — a second read walk over the very
  `tile_ram`/`ref_ram`/`next_ram` the swap would delete. **The on-chip arena is
  what FILLS the external one.** The two are in **series**, not in parallel, so
  the external path cannot become the sole producer by subtraction.
* **BLOCKER 2: the record is the wrong shape by 1,749 bits.** `t_*` is one
  16-byte TriangleDescriptor decoded; `job_*` needs **METAW = 1877 bits** — six
  240-bit plane equations, `tri_area2_i`, `tri_min_x_i`, the 298-bit flat
  request, the 48-bit continuation tail and the 32-bit fragment state. The
  24-byte ProjectedVertex **does** carry what the planes need, so they are
  **recomputable** — but only by standing up a **second setup and attrpack back
  end** fed from SDRAM, plus the vertex-fetch arm. **That is addition, not
  substitution.**
* **THE PRICE, measured on one stimulus across both paths**
  (`geom_chunkser_directed`'s `price_the_swap`): on-chip drain **4.12
  clocks/ref**, external walk **29.89 clocks/ref** — **7.3×**, with 22 SDRAM
  round trips against one on-chip RAM read.

## AND THE OWNER HAS ALREADY RULED ON EXACTLY THAT REFUSAL

**This is why the entry is a BUILD and not a question.**
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §4 decides I53–I55, and two of
its sentences answer WALKSWAP directly:

> **"I55 requires the walker to feed the live raster path from SDRAM. A PARALLEL
> LEGACY ON-CHIP FRAME ARENA THAT STILL SUPPLIES THE ACTUAL PIXELS IS NOT
> CLOSURE."**

> *"Keep only the bounded directories/caches/FIFOs permitted by R7. Generate a
> real producer-finished event, drain outstanding writes, then publish. Exercise
> a complete frame whose output depends on the bytes written and read through the
> real guard/arbiter/controller path, including the corrected BL8 model."*

**So the owner already knows the on-chip path currently supplies every pixel, and
has ruled that this state is not closure.** WALKSWAP's own closing paragraph
names the same shape the directive requires:

> *"a producer that fills the external arena WITHOUT the on-chip one — tile
> binning performed against arena triangle ids directly — after which the walk
> has an independent source."*

**That is the job. The refusal and the ruling agree on it.**

## WHAT CHANGED UNDER YOU ON 2026-09-26 — read this before costing anything

**GIANTREFS landed after this brief was written, and it moved the ground.**

* **The binner now holds 32,768 tile references, not 1,024.**
  `RENDER_CHUNKS=8192 / CHUNK_W=13 / CHUNK_REFS=4`, R7's number exactly, as named
  parameters. Measured `-MapOnly` on **`5CSEBA6U23I7`**, both rows
  `rtlCleanAtHead: true`: `@giantrefs-shipped` 191,296 bits →
  `@giantrefs-32k` **526,592 bits / 2,130 registers**, +335,296 bits ≈ **+33
  M10K, 9.30% of the 553-M10K ceiling.**
* **A giant was demonstrated surviving**, not argued: 45 whole-canvas triangles
  = **25,920 references binned whole, all 25,920 drained, `overflow_o = 0`**,
  with a positive control beside it (`fed=4097 overflow=1 culled_delta=1`). The
  old arena would have held 4.0% of that.
* **`CNT_W` is now derived** (`$clog2(REF_CAP + 1)`), along with `SLOT_W` and a
  third hardcoded width nobody had recorded — a five-bit pad in the max-depth
  compare that encoded `CNT_W == 11` where no reader of the localparam would
  look. Four elaboration guards added.
* **The binner's instruments are partly READ at last.** `tile_references` and
  `max_tile_list_depth` are published on GEOM.BINNER's own long-declared catalog
  ids, and `binner_overflow` is now the seventh whole-frame fault term per
  directive §4. **The smoke had `cnt_snap_ready_i` TIED TO ZERO**, so every
  console counter was unobservable from the gating bench; that is opened.

**So "the wall eats the giant" is FIXED and is not your problem.** What remains
yours is unchanged: the walker still has no independent producer.

**AND TWO NUMBERS IN CIRCULATION ARE WRONG — do not inherit them:**

* **"Raising `CHUNKS` silently wraps every tile count at 2,048" is FALSE.** I
  wrote it, from REFPUSH. `CNT_W` bounds a **per-tile** count whose real ceiling
  is `min(TRI_CAP, REF_CAP) = 128`, so raising `CHUNKS` alone was **always
  safe**. The derivation is correct and worth having; the alarm was not.
* **`@refpush-giantrefs32k` is `rtlCleanAtHead: FALSE`**, and it is the row the
  original +33 M10K figure came from. REFPUSH reported both rows clean. **Read
  `rtlCleanAtHead` before you quote any row** — including the ones above, which
  I verified myself after GIANTREFS flagged it.

## What §4 also binds you to, and it is not optional

* **Namespaces stay distinct.** *"Pre-clip mesh IDs, transient replay slots,
  post-clip vertex IDs and triangle IDs are different namespaces. Never pass one
  off as another."*
* **Identity is structural, not positional.** *"Equal positions alone do not
  prove identity"*, and *"a true shared vertex in the same identity domain must
  not acquire a new ID simply because another triangle or an evicted on-chip
  cache references it."*
* **Clipping lineage is carried explicitly**, intersections identified from
  canonical source edge and operation, with exact shared-edge interpolation and
  a **collision-safe** mapping — *"neither a hash nor a CRC alone proves
  equality."* **State the mapping lifetime and PROVE eviction/reuse cannot change
  a still-referenced identity.**
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.** If the compact records
  cannot carry it, **introduce a versioned extension or immutable sidecar keyed
  by the same identity** — *"the architect has explicit authority to amend record
  schemas for this purpose."* **Do not silently overload a field, truncate a
  handle, or replace a missing attribute with a convenient zero.**
* **R7's 65,536-vertex capacity is retained**, and the **count needs ≥17 bits** —
  *"instead of silently sacrificing a vertex or representing full capacity as
  zero."*
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame. **No arbitrary missing tail**, and no reuse while
  a prior frame or reader owns the allocation.
* **Backing state MAY move to SDRAM** rather than growing a frame-sized FPGA
  register file — that is stated as the architect's choice, and on a device at
  317% of its ALUT budget it is the interesting direction.

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and WALKSWAP did not do it. Either the external path is a real independent
  producer or the entry stays tied and you say so.
* **The 7.3× is a real measurement and a real problem.** If the independent
  producer does not change that arithmetic, **say so with numbers** — the
  directive requires the SDRAM path to feed the live raster, so a cost finding is
  a finding about how, not permission to keep the on-chip path.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`.** Note the entry's own
  warning: there is **no shipping-part figure for `zhao_geom_bin_pipe_v2`
  anywhere in this tree**, so do not difference its sizing-part row against a
  shipping-part one.

## Evidence bar

* **A pixel that depends on bytes that went through SDRAM** — the directive's own
  test: *"a complete frame whose output depends on the bytes written and read
  through the real guard/arbiter/controller path."* Today the smoke reports
  `paramwalk dirs=0 chunks=0 tris=0` beside `raster pixels=2560`. **That triple
  moving off zero is the demonstration.**
* **The identity proof**: eviction and reuse cannot change a still-referenced
  identity. State the lifetime; prove the bound.
* **The price re-measured on the same stimulus**, both paths, same unit —
  `price_the_swap` is committed and is your baseline.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison.** A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's.**
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`.
* **`zhao_geom_binner_v2.sv` and `zhao_console_core.sv` are SHARED HOT FILES.**
  Stage your **hunk**, never `git add <file>`; never `git checkout --` on one;
  and check `git diff --cached --name-only` before every commit.
* **`[IO.File]` ignores `cd`** — absolute paths only, or your writes land in the
  coordinator's tree. The tell is a clean `git status` after a successful write.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The independent producer** — what fills the external arena without the
   on-chip one, and the proof that it does.
3. **The pixel**, and `paramwalk dirs/chunks/tris` off zero in the composed
   console.
4. **The price, re-measured**, both paths, same unit.
5. **Every claim in this brief, the entry, or the directive reading above you
   found FALSE.** Every packet this week found at least one; most were mine.
6. **What you refused**, and if the swap is still not closable, the measurement
   that says so.
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/binarena` only.** Never `--force`.
