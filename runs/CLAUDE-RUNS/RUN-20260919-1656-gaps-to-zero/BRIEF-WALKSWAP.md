# WALKSWAP — I55, whose two preconditions are now BOTH met

**Branch `gz/walkswap`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

## Why now, and not before

`I55` has refused itself three times, and each refusal named the same two
preconditions. **Both landed in the last day.**

* **ARENAID (2026-09-25)** filled the vertex array — *"a walk that reaches a
  descriptor today decodes a real vertex reference rather than one
  `td_illegal_o` must refuse."*
* **CHUNKSER (2026-09-26) closed I54.** There are chunks to start a walk FROM:
  the composed console's smoke now reports **`paramarena chunks=10 frames=1`**
  against 0 at base, with each chunk's ids checked against the arena's
  descriptors through the real walker and independently against the DRAM bytes.

**Read I55 in full** (`grep -n '^// I55\.'` — never a line number) and note that
its text has NOT been amended by CHUNKSER: it still reads *"it needs I54 first"*.
That sentence is now satisfied, not wrong. **You are the packet it was waiting
for.**

**And CHUNKSER engineered to keep your ground clean**: its serialiser reads a
**SECOND binner pass after the raster drain**, so it is **not a term in
`job_ready_i`** and RASTER's timing is unchanged by I54. Whatever you measure is
yours, not inherited.

## JOB 1, AND IT MAY BE THE WHOLE PACKET: the arena has a known stall

CHUNKSER reported this rather than waiving it, and it is yours because it is in
your path:

> Run the test binary with any argument and it sweeps 60 scene phases.
> **Phase 53 never publishes**, stuck in the arena's publication arm on
> `wr_words_q != 0` with **every error counter at zero**. Proven not CHUNKSER's —
> tapped chunks and DRAM bytes correct, hand-driven records at identical counts
> publish. **It must go green before the console is fitted.**

**A walk over an arena that can stall is a walk that can stall.** Fix this first
or establish that it cannot reach your path, and say which. *Every error counter
at zero* while a thing wedges is this campaign's signature failure — the
instrument is not seeing the fault, so do not expect a counter to hand you the
answer.

## JOB 2 — the swap itself

What is tied is **who ASKS** (`walk_valid_i`) and **who TAKES** (`t_ready_i`).
Today the console rasterises from `zhao_geom_binner_v2`'s on-chip chunk arena
through `zhao_geom_bin_pipe_v2`'s `job_*` stream. The swap makes the external
arena the LIVE path instead of a second one beside it.

### The half-measure is named and forbidden

> *Doing it half-way, with the walker's triangles ORed into the live stream,
> would produce a picture and prove nothing.*

An OR of two producers gives you pixels and no evidence about which path made
them. **If you cannot make the external walk the only producer, that is a
finding — report it rather than dissolving the question into a merge.**

### AND YOU MUST PRICE IT, because of where it lands

**CORRECTED 2026-09-26 BY THE PACKET THIS BRIEF WAS WRITTEN FOR, AND THE ERROR
IS MINE TWICE OVER.** This paragraph said `zhao_geom_bin_pipe_v2` is *"the single
largest block in the console"* at *"58,514 ALUTs — 70% of the whole device"*.

* **58,514 is `zhao_shell_top_v2:u_shell`'s SUBTREE**, the whole shell. I
  attributed a container's number to one of its children.
* **It is not the single largest** either: on the sizing map `u_field_host` is
  39,964 against `u_render_bin`'s 32,380.
* **And the figure crossed devices in a brief whose next section forbids exactly
  that.** 58,514 is the SHIPPING-part row; the comparisons around it are
  sizing-part.

The real numbers, both measured and both named by part —
`zhao_geom_bin_pipe_v2:u_render_bin` is **32,380 ALUTs on `5CEBA9F31C7`** and
**42,490 on `5CSEBA6U23I7`**. It is a large block and the caution stands; the
headline was wrong.

*(WALKSWAP's own counter-example — `zhao_geom_drawjob` at 34,031 — is itself
stale: PALRAM took that block to 1,378. The conclusion is unaffected, and it is
worth noting that the correction of an old row quoted another old row.)* The entry's own
refusal says this is *"an area and throughput change to the block the fit budget
is tightest on."*

So the deliverable is not "it works". It is **it works and here is what it
cost**:

* **Area**: does removing the on-chip arena from the raster path actually remove
  it, or does it stay for a second consumer? Measure, do not assume.
* **Throughput**: the external walk reaches DRAM through the guard. Count the
  clocks per triangle both ways on the same stimulus.
* **If it is a net loss, say so.** A closed entry that costs the fit more than it
  buys is a trade, and the owner's directive forbids calling reduced work
  equivalent merely to reach zero.

## Device discipline — this cost the coordinator an hour today

**Every map you quote must name its `-Device`.** `run_block_fit.ps1 -Device`
defaults to the SHIPPING part `5CSEBA6U23I7` (112 DSP); the console's
`@edgeclose` row was run on the SIZING part `5CEBA9F31C7` (342 DSP). **I
differenced the two and read a 247-block DSP fall as a win.** Quartus replaces
multipliers a part cannot hold, so a smaller device reports fewer DSPs and more
ALUTs *for identical RTL*.

`tools/budget/map_entity_attrib.py` now prints the device in every table's
header. **Rows on different devices must not be differenced. Say which part
every number of yours is on.**

## The fences

* **Do NOT start a console or full-device fit.** Those are 01:54:49 and the
  coordinator schedules them. `-MapOnly` on a block is yours.
* **The on-chip arena is not to be deleted speculatively.** If the swap
  succeeds, removing the old path is a separate, measured step — and if anything
  else consumes it, it stays.
* **A prefix of a chain whose last link does not exist is "a tie-off wearing a
  composition's clothes."** If the walk cannot yet take triangles, do not wire
  the request and call it closed.
* **Re-run `completion_register.py` BARE after editing any entry text** — it
  classifies by scanning prose.

## Evidence bar

* **Pixels from the external walk, with the on-chip path NOT contributing** —
  and a demonstration that it is not contributing, not an assertion.
* **A live chain of real production modules** wired as `zhao_console_core` wires
  them. `tests/prod/terrainaux_acceptance.cpp` and
  `tests/geometry/geom_chunkser_directed.cpp` are the patterns.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison** — a checker whose operands move together cannot fire. A guard
  unreachable with legal stimulus needs a **committed mutant** under
  `tests/mutants/`, renamed so no source list elaborates it, driver polarity
  inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* leaves `$LASTEXITCODE` carrying the previous command's value.
* **`gate_sweep` does not run the console smoke controls.** Run them yourself
  with proper switch binding — `& script $sw` binds the switch as a positional
  path and silently does nothing; use `@splat` in a loop.
* **Regenerate `zhao_prod_top.sv` after ANY port change** and re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree**; after killing one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*`.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **Phase 53** — fixed, or proven unable to reach your path. Which.
2. **The register before and after, measured BARE.**
3. **The pixels, and the proof the on-chip path did not make them.**
4. **THE PRICE** — area and clocks-per-triangle, both ways, same stimulus, with
   the device named. If it is a net loss, say so plainly.
5. **Every claim in this brief or the entry you found FALSE.**
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. The branch and commit hash. **Push `gz/walkswap` only.** Never rebase or push
   the shared branch, and never `--force`.
