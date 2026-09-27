# PVSCHEMA — I55's real blocker, and it is a RECORD that widens for FREE

**Branch `gz/pvschema`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES.** Read, in full and first:

1. `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` — SWAPBUILD's decision.
   **It refuted the premise five packets carried and it chose the architecture.**
2. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-swapbuild.md` —
   §6.1 and §6.2 especially.
3. Entry `I55` (`grep -n '^// I55\.'`, never a line number).

## THE PREMISE FIVE PACKETS QUOTED IS FALSE

The entry said, five times, that the 24-byte ProjectedVertex *"carries what
those need … so the planes are **RECOMPUTABLE**"*.

**IT DOES NOT.** `zhao_geom_vertid.sv:499` stores colour through
`unit8_of_fx16` — `(v+128)>>8`, clamped at both ends — **while its inputs are
the full 32-bit attribute slots attrpack itself reads.** The R/G/B Gouraud
planes R234 D1 added **cannot be rebuilt from that record by ANY back end.**

**Note the direction, because it is why it survived five readings:** the false
claim made the work look **simpler** — "a second back end" rather than "a wider
record AND a back end" — and it listed `rgba` beside `x`, `y` and `u/w` as
though the four were the same kind of quantity. **Nobody audits good news.**

**This blocks the second-instance architecture EXACTLY as hard as it blocks the
multiplex.** It is not an argument about which back end to build.

## YOUR FIRST COMMIT: ProjectedVertex v2, AND IT COSTS NO ADDRESS SPACE

**`PV_STRIDE_B` is 32 and the record is 24 — every slot already carries EIGHT
BYTES OF DECLARED SLACK.**

```
v2:  x 21 + y 21 + invw 24 + status 8 + u/w 32 + v/w 32
     + r 32 + g 32 + b 32 + alpha 8            = 242 bits = 30.25 bytes
                                                 (14 bits spare in the stride)
```

* **`VERT_CAP_B` does not move** and **no region in `spec/memory_rules.md` §5c
  changes.**
* **Narrowing x/y to s21 is NOT a truncation.** `zhao_geom_parambuf`'s
  `pv_illegal_o` **already refuses** any vertex failing `fits_s21`, so **s21 is
  the declared domain** and you are writing down a law the hardware enforces.
* Directive §4 **authorises this amendment by name** — a versioned extension or
  immutable sidecar keyed by the same identity — and forbids the shortcut:
  *"never silently overload a field, truncate a handle, or substitute a
  convenient zero."*

**THE OPEN SUB-QUESTION, STATED SO YOU DECIDE IT RATHER THAN DISCOVER IT:**
**alpha at its full 32 bits makes the record 266 bits and does NOT fit.** Decide
alpha's width explicitly, in the record, with the reason beside it. **Do not
discover this at the end.**

**And refresh the THREE committed arena mutants** — they are **copies**, and a
copy of a record that no longer exists is a positive control for a block that
does not exist. Three-way merge them; do not transplant.

## THE ARCHITECTURE IS DECIDED: A TIME MULTIPLEX, NOT A SECOND INSTANCE

**Do not build a second back end.** SWAPBUILD priced it — `zhao_geom_attrpack`
**had zero rows in either synthesis database**, so the thing five packets
demanded had **never been mapped**. With `zhao_geom_setup`'s clean row a second
instance is **+1,621 ALUT and +40 DSP — 35.7% of the device's entire multiplier
budget, and 4.4× the bill LANESCOST refused for I34 this week.** Refused.

**Why the multiplex is available**, measured from the binner's own FSM rather
than from a schedule argument:

* `zhao_geom_binner_v2.sv:818` — `assign tri_ready_o = (state == S_IDLE) &&
  !drain_req_r;`
* `:931`, unconditional, outside the case — `if (frame_end_i) drain_req_r <= 1'b1;`
* `:941` — `drain_req_r` is tested **before** `tri_valid_i`.
* **Bin states are 0..5, drain states 6..11. The ranges are disjoint and ONE FSM
  owns both.** Once `frame_end_i` pulses, no further triangle is accepted until
  `D_DONE` clears `drain_req_r`.

**So `zhao_geom_setup` and `zhao_geom_attrpack` are provably idle for the whole
raster drain**, and neither can be corrupted by a second source because **neither
carries state across a triangle boundary** — measured: `grep -c frame
zhao_geom_setup.sv` → **0**, and attrpack's three `frame` hits are **all comment
text**.

## THE CONSTRAINT THAT WILL BITE YOU, AND IT IS NEW

**The handover says one SDRAM share "carries two". IT CARRIES TEN, and NEITHER
SHARE HAS A FREE SLOT.** SWAPBUILD found this and refused to widen either.

**You are feeding a back end from SDRAM. Confront this before you design the
feed, not after.** If it needs a slot that does not exist, that is a finding and
a refusal with a number — a full deliverable — not something to route around
quietly.

**And `spec/memory_rules.md` contradicts itself about 22 MiB.** Read it with
that in mind; do not resolve it by picking the convenient half.

## TWO INHERITED CLAIMS THAT ARE NOT SAFE TO TRUST

* **`check_ram_inference.py` rule 6 is NOT complete.** SWAPBUILD found a **live
  counterexample in the binner** to ARENAINFER's *"the killer is the generate
  for-loop and nothing else"*, and **recorded it without guessing at the cause**.
  Do the same if you meet one. The checker was once **100% false alarms and 100%
  miss** on the one file that mattered — **a clean line in it is not proof an
  array will infer.** I corrected that tool's printed remedy today; it used to
  recommend the killer by name.
* **Binner rows quoted as shipping cost were mapped at the default
  `METAW=1157` while the console ships `1877`.** Any area figure you inherit for
  that block is for a narrower machine.

**DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — it cost
`zhao_geom_arenabin` **146,414 registers against 1,010** for the identical
circuit. Generate-**IF** infers; module scope infers; **the LOOP is the killer.**
The correct remedy is one flat array **at a module's own scope**, its own module
instantiated inside the loop — `zhao_dc_sdp_ram` is that module.

## What §4 still binds you to

* **Namespaces stay distinct** — pre-clip mesh IDs, transient replay slots,
  post-clip vertex IDs and triangle IDs. **Never pass one off as another.**
* **Identity is structural**: equal positions do not prove identity, and a shared
  vertex must not gain a new ID because a cache evicted it.
* **Carry clipping lineage explicitly**, with a **collision-safe** mapping —
  *"neither a hash nor a CRC alone proves equality."*
* **Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
  material-set, material-record and fragment metadata.**
* **R7's 65,536-vertex capacity is retained**, count ≥17 bits.
* **Overflow is a whole-frame fault** with drain, source attribution and repeat
  of the prior complete frame.
* **A parallel legacy on-chip frame arena that still supplies the actual pixels
  is not closure.**

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and **four** packets have refused it.
* **Do not fake `paramwalk`** — still `dirs=0 chunks=0 tris=0`, refused four
  times. A sequencer with every `t_*` dangling counts triangles and drops them.
* **Do not build any part standalone.** A module nothing instantiates needs a
  `pending_compose` disposition, converting a clean **`BUILT BUT NOT CONNECTED:
  0`** into a declared deferral.
* **Do not regress the id repair.** `geom_tidq_directed` is 83 checks; it must
  still pass and its 21 base-RTL failures must still fail.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map must name its `-Device`**, rows on different devices must never be
  differenced, and **read `rtlCleanAtHead` before quoting ANY row.**

## LAND IT INCREMENTALLY. THE SCHEMA ALONE IS A GOOD PACKET.

**A packet that lands ProjectedVertex v2, decides alpha, refreshes the three
mutants and declares the multiplex open is a good packet.** The schema is the
blocker; everything else is downstream of it.

## Evidence bar

* **The colour round-trips at full precision** — a value that is **not**
  recoverable under the old 8-bit store, so the test fails against the v1 record.
  **`{255,255,255}` proves nothing.**
* **The three refreshed mutants still FIRE**, each on its own mutation.
* **A pixel that depends on bytes that went through SDRAM**, if you get that far
  — §4's own test. Today `paramwalk dirs=0 chunks=0 tris=0` sits beside
  `raster pixels=2816`.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Ten packets this week found their own controls vacuous — one measured
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
* **A struct-layout change means recompiling every `.cpp` that uses it** — a
  stale object with an old layout looks exactly like a rendering bug. **This is
  a struct-layout change.**
* **GATE 31: `check_console_closure_lint.py`** — run after any port change. **It
  deduplicates its source list** while `run_console_board_lint.ps1` does not.
* **If you add a port to `zhao_console_core`, update BOTH `.*` wrapper mutants.**
* **Regenerating a generated file is part of the change** — six went stale this
  week, three on merges, and one was a report handing out a known-fatal remedy.
* **`git show` hands back INDEX content at LF while the working copy is CRLF**,
  so **normalise the base's line endings before believing merge conflicts** —
  this bites specifically when refreshing committed mutants, which you are doing.
* **Do not edit RTL while a build or a suite is reading it.**
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree**, and the other lane's tree is live.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **ProjectedVertex v2** — the layout, where it is declared once, and the
   round-trip test that fails against v1.
3. **Alpha's width, DECIDED**, with the reason recorded in the record itself.
4. **The three arena mutants refreshed and still firing.**
5. **What you found about the SDRAM share** — whether a slot exists, and if not,
   what that costs.
6. **Every claim in this brief or the records you found FALSE.** Every packet
   this week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/pvschema` only.** Never `--force`.
