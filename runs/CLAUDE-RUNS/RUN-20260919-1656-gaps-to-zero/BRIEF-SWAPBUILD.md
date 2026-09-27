# SWAPBUILD — I55's blocker 2. The prerequisite landed; the ids are correct now.

**Branch `gz/swapbuild`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES.** Read entry `I55`
(`grep -n '^// I55\.'`, never a line number) and
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-rasterswap.md` **in
full** — that packet is your immediate predecessor and it cleared your path.

## WHAT RASTERSWAP LANDED, SO YOU START FROM A CORRECT MACHINE

**The arena descriptor ids were 99% WRONG until yesterday.** `u_geom_tidq` was
permanently one behind — `popped[k] == pushed[k-1]` — so **74 of 75 triangles
were binned under their predecessor's arena descriptor index**, with ids in
range and decoding cleanly so every range guard passed.

**REPAIRED AND PROVEN.** `popped[k] == pushed[k]`, and the external arena now
**agrees with the binner exactly**:

```
before:  tris=74  unnamed=1  refs=97    (SHORTFALL 4)
after:   tris=75  unnamed=0  refs=101   (AGREES EXACTLY)
```

Seven smoke forms at five scene sizes, `raster pixels=2816`, whole-frame
`sdram_busy` unmoved (645,849 → 645,846). Three blocks: the door gated on queue
occupancy (**valid AND both upstream readys** — gating one half *drops* a
triangle instead of misnaming it); seal-abort push via `busy_o` with **zero new
leaf ports**; flush **poisoning in place rather than discarding**, because
discarding an owed entry turns a wrong id into a permanent stall once the door
is gated. `geom_tidq_directed`, 83 checks, **21 fail against the base RTL**.

**This matters to you specifically: the whole point of the swap is that the
walker reads descriptors BY ARENA ID.** Before yesterday a swap would have
rendered the wrong geometry with every gate green. **That is no longer true, and
it is why this packet can exist.**

**One counter carries a warning label:** `vertid stall` reads **1774 → 0**, and
**that is NOT 1,774 clocks saved** — gating the door moved one of the counter's
own operands. Do not quote it as a saving.

## BLOCKER 1 IS CLOSED. BLOCKER 2 IS THE WORK.

**ARENACOMPOSE composed `u_geom_arenabin`** — it bins the post-clip stream
against the arena's own `td_id_o` and writes 64-byte chunks straight into
`zhao_geom_paramarena`, **reading no binner RAM** — and **retired
`u_geom_chunkser` and the binner's whole serialise pass BY REMOVAL**, never
tie-off.

**What closes the entry, in the owner's words (directive §4):** *"I55 requires
the walker to feed the live raster path from SDRAM. **A parallel legacy on-chip
frame arena that still supplies the actual pixels is not closure.**"* **Every
pixel still comes from the on-chip drain.**

**Blocker 2, measured and re-measured:** `t_*` is one 16-byte TriangleDescriptor
decoded; `job_*` needs **METAW = 1,877 bits**, a **1,749-bit** shortfall. The
24-byte ProjectedVertex **does** carry what the planes need — x, y, `invw24`,
status, u/w, v/w, rgba — **so the planes are RECOMPUTABLE**, but only by standing
up a **second setup and attrpack back end fed from SDRAM**, plus the vertex-fetch
arm `zhao_geom_paramwalk` deliberately does not drive. **That is ADDITION, not
substitution**, and `zhao_forge_assemble`'s `u_dq` / `u_rcp` / `pack_attr` is the
template.

**The price, re-run by RASTERSWAP rather than quoted** (`tb_zhao_geom_paramarena.sv`
instantiates the queue): **4.12 / 29.58 / 5.88 clocks per ref, 549 checks, 0
failures — unmoved, still 7.18×.** The producer is not the expensive half.

## RASTERSWAP REFUSED THIS AS A **BUILD** REFUSAL, NOT A DECISION REFUSAL

That distinction is the reason you exist. **Nothing is unsettled.** It ran out
of packet, having spent itself on the prerequisite. It also refused two things
you should refuse identically:

* **Building any part of the back end STANDALONE.** A module nothing
  instantiates needs a `pending_compose` disposition, which converts a clean
  **`BUILT BUT NOT CONNECTED: 0`** into a declared deferral. **The swap is
  all-or-nothing by this campaign's own accounting.**
* **Faking `paramwalk`.** It is still `dirs=0 chunks=0 tris=0`, refused a
  **third** time: every `t_*` output dangles, so a sequencer would count
  triangles and drop them. *"A producer driving into nothing: logic added to
  make a counter move."*

## What §4 binds you to

* **Namespaces stay distinct** — pre-clip mesh IDs, transient replay slots,
  post-clip vertex IDs and triangle IDs. **Never pass one off as another.**
* **Identity is structural**: equal positions do not prove identity, and a
  shared vertex must not gain a new ID because a cache evicted it. **State the
  mapping lifetime and PROVE eviction cannot change a still-referenced
  identity.**
* **Carry clipping lineage explicitly**, intersections identified from canonical
  source edge and operation, with a **collision-safe** mapping — *"neither a
  hash nor a CRC alone proves equality."*
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.** You have **explicit
  authority to amend record schemas** via a versioned extension or immutable
  sidecar keyed by the same identity — but **never silently overload a field,
  truncate a handle, or substitute a convenient zero.**
* **R7's 65,536-vertex capacity is retained**, and the count needs **≥17 bits**.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame. **No arbitrary missing tail**, and no reuse while
  a prior frame or reader owns the allocation.
* **Backing state MAY move to SDRAM** rather than growing a frame-sized FPGA
  register file — and on a console at **350% ALUTs** that is the interesting
  direction. **Note it is the ONLY direction that helps**: LANESCOST has just
  refused I34's front at +11,979 ALUTs, so a swap that costs comparable logic
  will be refused on the same arithmetic. **Design for SDRAM, not for registers.**

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and **four** packets have now refused it.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — the fourth Quartus
  17.0.2 inference killer, which cost `zhao_geom_arenabin` **146,414 registers
  against 1,010** for the identical circuit. Generate-**IF** infers; module scope
  infers; **the LOOP is the killer.** Rule 6 catches it now, and the checker was
  **100% false alarms and 100% miss** on that file before — **its silence is not
  a verdict.**
* **Do not regress the id repair.** `geom_tidq_directed` is 83 checks and it is
  new; if you change the door, the seal or the flush, **it must still pass and
  its 21 base-RTL failures must still fail.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## LAND IT INCREMENTALLY AND DECLARE WHERE YOU STOPPED

**A packet that stands up the second setup back end, feeds it from SDRAM,
measures it, and declares the last link open is a good packet.** So is one that
refuses on a measured area or throughput number. **What is not acceptable is a
counter moved without a consumer, or a swap that leaves the on-chip drain
supplying the pixels while claiming closure.**

## Evidence bar

* **A pixel that depends on bytes that went through SDRAM** — §4's own test.
  Today `paramwalk dirs=0 chunks=0 tris=0` sits beside `raster pixels=2816`.
* **If you refuse: the area or throughput number that decided it**, on the
  shipping part, `rtlCleanAtHead` true, device named.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Nine packets this week found their own controls vacuous — one measured
  **refusals** and called it a 62× speed-up, one asserted three counters fired
  when its stimulus could not reach one, and one printed *"expected 0x1, got
  0x1"* **on failure**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — `| tail`
  reports TAIL's exit code, which LANESCOST hit on its very first use of it.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **`UNUSEDSIGNAL` is waived across whole directories** by
  `tests/shell/v3_closure_inherited.vlt` — **a dead wire you create raises
  nothing.** Count readers by hand.
* **GATE 31: `check_console_closure_lint.py`** — run after any port change; it
  caught 7 PINMISSING for BINARENA. **It deduplicates its source list** while
  `run_console_board_lint.ps1` does not, so a duplicated line in
  `fit_targets.yml` makes it say OK twice.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper mutants.**
* **Regenerating a generated file is part of the change** — five went stale this
  week, three on merges.
* **`git show` hands back INDEX content at LF while the working copy is CRLF.**
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED** — check the row you wrote.
* **Do not edit RTL while a build or a suite is reading it.**
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree**, and the other lane's tree is live.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **What you built of the second setup back end**, fed from where, and what
   remains.
3. **Whether `paramwalk` moved off zero, and whether that was REAL.**
4. **The price re-measured**, same stimulus, same unit, against the 7.18×.
5. **That the id repair still holds** — `geom_tidq_directed` green, its base-RTL
   failures still failing.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/swapbuild` only.** Never `--force`.
