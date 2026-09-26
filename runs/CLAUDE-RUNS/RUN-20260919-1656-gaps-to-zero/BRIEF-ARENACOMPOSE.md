# ARENACOMPOSE — I55: the producer is BUILT and PROVEN. Compose it.

**Branch `gz/arenacompose`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is gap work, not optimization.** `I55` is one of the last four entries and
it is the largest. Read it (`grep -n '^// I55\.'`, never a line number) — the
WALKSWAP and BINARENA sections are measurements, not opinions.

## What BINARENA already built and proved

**`zhao_geom_arenabin` (GEOM.ARENABIN) exists and is independent.** It bins the
post-clip stream against the arena's own `td_id_o` and writes 64-byte chunks
straight into `zhao_geom_paramarena`, **reading no binner RAM.**

**The proof is STRUCTURAL, not a counter at zero:** `geom_arenabin_directed`
elaborates with `-GHAVE_ONCHIP=0` from a source list containing **no binner, no
chunkser, no tidq, no geom_arena** — and `tests/CMakeLists.txt` **fails the
CONFIGURE** if one leaks back in. 275 checks through the real
guard/arbiter/controller/SDRAM path, ids compared against the arena's own
descriptors **with decoys so a local index would decode wrong while every range
guard passed**, and the chain patch verified in the DRAM bytes. Plus 470 in
`geom_arenabin_price`, where the legacy path is compiled in and **live** and
produced nothing — the one place that assertion is not vacuous.

**The transpose moved to SDRAM as directive §4 asks:** 177,984 bits on chip
against the binner's 360,064, **and the bound is per-tile and constant, so the
giant costs what one triangle costs.**

**The price, solved as a LINE rather than sampled once:** 2,176 clocks/frame
fixed + **5.82 clocks/ref marginal → 5.88 at R7's 32,768-ref giant.** A single
point would have lied at 86.41, because `busy_o` spans two 576-entry directory
sweeps. **`price_the_swap` re-ran byte-identically at 4.12 / 29.89, so the 7.3×
is the CONSUMER side and the producer is not the expensive half.**

**Declared cost:** 8,093 clocks of `intake_stall_o` over a 1,600-ref frame
(~5 clocks/ref of backpressure), because unlike chunkser this block bins the
**live** stream. **No FIFO was added — the measurement comes first.**

## WHY IT REFUSED TO COMPOSE, and what has changed

BINARENA refused for three reasons. **Two still stand; one is gone.**

* **GONE — "I am forbidden a fit, so composing would be an unmeasured claim
  about the tightest budget."** The owner's standing authorization of
  2026-09-26 permits a synthesis/map or diagnostic fit **whenever it answers a
  concrete engineering question, without asking first.** *"If the hardware is
  ready for a useful complete-hardware measurement, take that measurement rather
  than idle or manufacture a closure."* **So measure what you compose.**
* **STANDS — composing retires chunkser AND orphans the binner's serialise
  pass.** That is a subsystem retirement on the tightest block in the design.
  **Do it deliberately and declare it, or do not do it.**
* **STANDS — blocker 2, the record shape.** `t_*` is one 16-byte
  TriangleDescriptor decoded; `job_*` needs **METAW = 1877 bits**. The
  ProjectedVertex carries what the planes need so they are **recomputable**, but
  only by standing up a second setup and attrpack back end. **That is ADDITION,
  not substitution.**

## What closes I55, in the owner's words

Directive §4: **"I55 requires the walker to feed the live raster path from
SDRAM. A parallel legacy on-chip frame arena that still supplies the actual
pixels is not closure."**

**So composing the producer is NECESSARY and may not be SUFFICIENT.** Be honest
about which you achieved. **A packet that composes the independent producer,
measures it, retires chunkser cleanly and declares the raster swap still open is
a good packet** — it is the first half of a two-half entry and it is the half
nobody has done.

## The job

1. **Compose `zhao_geom_arenabin`**, retiring chunkser and the binner's
   serialise pass deliberately.
2. **Measure it.** `-MapOnly` on the block is free; a diagnostic fit is
   authorised if it answers a question you can state in advance. **Name the
   question before you run it.**
3. **Move `paramwalk dirs/chunks/tris` off 0/0/0 ONLY if it is real.** BINARENA
   refused to wire `walk_valid_i` to a tile sequencer because every `t_*` output
   dangles — **"a producer driving into nothing: logic added to make a counter
   move."** That refusal binds you too.
4. **If the raster swap is reachable, take it. If not, say so with the
   measurement.**

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden.
* **Namespaces stay distinct** — pre-clip mesh IDs, replay slots, post-clip
  vertex IDs and triangle IDs are different; never pass one off as another.
* **Identity is structural.** Equal positions do not prove identity, and a
  shared vertex must not gain a new ID because a cache evicted it. **State the
  mapping lifetime and PROVE eviction cannot change a still-referenced
  identity.**
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.** You have explicit
  authority to amend record schemas via a versioned extension or immutable
  sidecar — but **do not silently overload a field, truncate a handle, or
  substitute a convenient zero.**
* **R7's 65,536-vertex capacity is retained**, and the count needs **≥17 bits**.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame. **No arbitrary missing tail.**
* **`zhao_geom_binner_v2.sv` and `zhao_console_core.sv` are SHARED HOT FILES**
  and another packet is live in the tree. Stage your **hunk**; never
  `git add <file>`; never `git checkout --`.

## Evidence bar

* **A pixel that depends on bytes that went through SDRAM** — the directive's
  own test. Today `paramwalk dirs=0 chunks=0 tris=0` sits beside
  `raster pixels=2816`.
* **The price re-measured**, same stimulus, same unit, as a **line** not a
  point — BINARENA's single-point figure would have lied by 15×.
* **Whatever you compose, measured** with the device named and
  `rtlCleanAtHead` read **before** you quote any row.
* **Prove every counter you quote.** A guard unreachable with legal stimulus
  needs a **committed mutant**, renamed so no source list elaborates it,
  polarity inverted so it passes when the counter FIRES.
* **Check any control you add CAN FAIL** — a `$fatal` guard as a build target
  returns RC=0, and a packet found one of its own negative controls vacuous
  because a line number moved underneath it.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change; it caught 7 PINMISSING for BINARENA.
* **Regenerating a generated file is part of the change.** `zhao_prod_top.sv`,
  the console wrapper port blocks and the texture-v3 interface manifest have all
  been found stale this week; the manifest went red on a merge today.
* **Three concurrent smoke forms is this box's ceiling** — five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`reports/synthesis/zhao_block_fit.json` reserialises** — verify no row is
  lost, count before and after.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **What you composed and what you retired**, declared explicitly.
3. **The measurement**, with the question it answered stated in advance.
4. **Whether `paramwalk` moved off zero, and whether that was real.**
5. **Whether the raster swap is reachable**, with the number.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/arenacompose` only.** Never `--force`.
