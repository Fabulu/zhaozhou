# RASTERSWAP — I55's second half, and the ids it would read are WRONG TODAY

**Branch `gz/rasterswap`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES.** `I13` closed,
`zhao_terrain_normalmap` came off the disconnected list, `BUILT BUT NOT
CONNECTED: 0`. Only `I34` and `I55` are left, and this is the larger.

Read entry `I55` (`grep -n '^// I55\.'`, never a line number) — the WALKSWAP,
BINARENA and ARENACOMPOSE sections are measurements. **Do not re-derive them.**

## BLOCKER 1 IS CLOSED. BLOCKER 2 IS YOURS.

**ARENACOMPOSE composed `u_geom_arenabin`** — it bins the post-clip stream
against the arena's own `td_id_o` and writes 64-byte chunks straight into
`zhao_geom_paramarena`, **reading no binner RAM** — and **retired
`u_geom_chunkser` and the binner's whole serialise pass BY REMOVAL, never
tie-off.**

**What closes the entry, in the owner's words (directive §4):** *"I55 requires
the walker to feed the live raster path from SDRAM. **A parallel legacy on-chip
frame arena that still supplies the actual pixels is not closure.**"* **Every
pixel still comes from the on-chip drain.**

**Blocker 2, measured:** `t_*` is one 16-byte TriangleDescriptor decoded;
`job_*` needs **METAW = 1,877 bits**, a **1,749-bit** shortfall. The 24-byte
ProjectedVertex **does** carry what the planes need — x, y, `invw24`, status,
u/w, v/w, rgba — **so the planes are RECOMPUTABLE**, but only by standing up a
**second setup and attrpack back end fed from SDRAM**, plus the vertex-fetch arm
`zhao_geom_paramwalk` deliberately does not drive. **That is ADDITION, not
substitution**, and `zhao_forge_assemble`'s `u_dq` / `u_rcp` / `pack_attr` is
the template.

**The consumer side is 7.18×** — 4.12 against 29.58 clocks/ref, re-measured by
ARENACOMPOSE after its chain patch. **The producer is not the expensive half**
(5.88 clocks/ref at R7's giant).

## STEP ZERO, AND IT IS NOT OPTIONAL: THE IDS ARE WRONG TODAY

**ARENAINFER measured that `u_geom_tidq` is permanently ONE BEHIND.**

```
tidqids  pushed=0 1 2 3 4 5 6 7 | popped=262143 0 1 2 3 4 5 6
```

262143 is `ID_POISON`; after it, **`popped[k] == pushed[k−1]` for every k.**
**Triangle 1 is dropped, and every triangle after it is binned under its
PREDECESSOR's arena descriptor index — 74 of 75.**

**That is entry I54's named failure, live in the composed console, in an entry
the register counts as CLOSED.** It is invisible because the ids stay **in
range** and **decode cleanly**, so every range guard passes and the smoke's own
refs comparison differences two totals.

**YOU CANNOT BUILD A RASTER SWAP ON TOP OF IT.** The whole point of the swap is
that the walker reads descriptors by arena id — and **99% of those ids name the
wrong triangle.** A swap built today would render the wrong geometry and every
gate would stay green.

**So: fix it first, or prove it does not affect the path you build.** The cause
is measured and is **NOT** the flush/seal story: `vid_seal_abort=0`, pushes ==
pops, nothing lost. **The first door beat precedes the first push — structural
startup skew**, because SETUP is 3 stages while VERTID needs `S_PUB×3 + S_TD`.

**Its costed repair, from ARENAINFER:** gate the door on `level_o != 0` (the
port exists, tied to `()`) — **but that alone deadlocks** if VERTID aborts on a
seal while SETUP holds the triangle (`vid_seal_abort_o` is also unconnected and
reads 0 today, **a gate that cannot reach the state**). So it also needs VERTID
to push poison on the abort, and the queue's `else if (flush_i)` priority
re-thought. **Three blocks, a door handshake, a deadlock mode.**

**ARENAINFER deliberately did NOT add `$fatal` on the counter** — it would turn
the plain smoke red immediately and read as its own regression. **What is
committed is the measurement**: both id streams printed in order. **Overturn
that only if you are taking the defect now.**

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
  register file — and on a console measured at **350% ALUTs** that is the
  interesting direction.

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and three packets have refused it.
* **Do not fake `paramwalk`.** Wiring `walk_valid_i` to a tile sequencer while
  every `t_*` output dangles is *"a producer driving into nothing: logic added
  to make a counter move."* Two packets refused it; so do you.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — a fourth Quartus
  17.0.2 inference killer measured this week, which cost `zhao_geom_arenabin`
  **146,414 registers against 1,010** for the identical circuit. Generate-**IF**
  infers; module scope infers; **the LOOP is the killer.** Rule 6 catches it
  now, and the checker was **100% false alarms and 100% miss** on that file
  before — **its silence is not a verdict.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## THIS IS BIG. LAND IT INCREMENTALLY AND DECLARE WHERE YOU STOPPED.

**A packet that repairs the tidq join, proves the ids correct, and declares the
swap still open is a good packet.** So is one that additionally stands up the
second setup back end and measures it. **What is not acceptable is a swap built
on ids that are 99% wrong, or a counter moved without a consumer.**

## Evidence bar

* **The ids correct**, if you take step zero: `popped[k] == pushed[k]`, with the
  startup skew shown handled and **the deadlock mode exercised**, not argued.
* **A pixel that depends on bytes that went through SDRAM** — §4's own test.
  Today `paramwalk dirs=0 chunks=0 tris=0` sits beside `raster pixels=2816`.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Seven packets this week found their own controls vacuous — including one that
  measured **refusals** and called it a 62× speed-up, caught only by the status
  check, and one whose agreement check's guard was **never true**.
* A guard unreachable with legal stimulus needs a **committed mutant**, renamed
  so no source list elaborates it, polarity inverted so it passes when the
  counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — `| tail`
  reports TAIL's exit code, which two packets hit this week, and it applies to
  **every gate**.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **GATE 31: `check_console_closure_lint.py`** (294 sources) — run after any
  port change; it caught 7 PINMISSING for BINARENA. **It deduplicates its source
  list** while `run_console_board_lint.ps1` does not, so a duplicated line in
  `fit_targets.yml` makes it say OK twice.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper
  mutants.**
* **Regenerating a generated file is part of the change** — five went stale this
  week, three on merges.
* **`git show` hands back INDEX content at LF while the working copy is CRLF**,
  so a diff of a correct file can claim thousands of changed lines.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED.**
* **Do not edit RTL while a build or a control is verilating it.**
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree**, and other lanes' trees are live.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The tidq ids** — repaired and proven, or measured not to affect your path.
3. **What you built of the second setup back end**, and what remains.
4. **Whether `paramwalk` moved off zero, and whether that was REAL.**
5. **The price re-measured**, same stimulus, same unit.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/rasterswap` only.** Never `--force`.
