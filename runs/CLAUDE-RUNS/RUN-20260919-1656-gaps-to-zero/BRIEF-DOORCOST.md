# DOORCOST — I55's door. Price it, then open it or refuse with the number.

**Branch `gz/doorcost`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I55` is ONE OF TWO REMAINING REGISTER ENTRIES.** Read first, in full:

1. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-muxbuild.md` —
   **your immediate predecessor. It found the blocker nobody had measured.**
2. `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` and
   `reports/DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md`.
3. Entry `I55` (`grep -n '^// I55\.'`, never a line number).

## EVERYTHING EXCEPT THE DOOR IS BUILT

**The back end exists and is proven.** MUXBUILD built the **ProjectedVertex
fetch arm** inside `zhao_geom_paramwalk` — four states, three times per
descriptor, between the last beat and the emit, so **a triangle is offered
complete or not at all**. `geom_paramarena_directed` went **351 → 550 checks, 0
failures**: every field of all three vertices of all six triangles returns
**bit-identical through the real guard, arbiter, controller and SDRAM**, with
the discriminating premise asserted first (frame A's eight reds all collapse to
one byte under v1's quantiser).

**The record carries the colour.** PVSCHEMA's schema v2 stores r/g/b at full
32-bit precision in stride slack that cost no address space.

**The ids are correct.** RASTERSWAP repaired `u_geom_tidq`; the arena agrees
with the binner exactly.

**The SDRAM question is closed BOTH WAYS.** No new slot is needed — the arm
rides `guard_req_o`, the socket the walker already owns, so a fourth request
kind is a **state-machine addition, not a client addition**. And **no slot
exists**: adapter `a..j` all driven with no `N` parameter, `u_geom_wshare` `N=3`
all driven. **Directive §7's "use the reserved client slot if the live design
still has it" evaluates FALSE** — `zhao_vram_arbiter.sv:353` forces
`port_grant[RESERVED_ID] = 1'b0`. **The reserved slot is structurally dead.**

**The three fetch-arm counters are now exported and READ** by the console smoke,
which asserts the arm's own invariant `vread == 3 * tris` and fatals on a
refused record. It holds trivially at zero today. **That is the line that moves
when you open the door.**

## THE BLOCKER, AND IT IS THE ONLY ONE LEFT

**`job_*` IS NOT A PORT.**

`zhao_raster_tile_pipe_v2` is a **CHILD** of `zhao_geom_bin_pipe_v2`, fed over
**internal wires** from the binner in that same module. **The multiplex has a
back end and NO DOOR.**

**Six packets — including both of 2026-09-27's decision records and my own
briefs — said "take `job_*` from that path" as though it were a wire. Nobody had
opened the module.** MUXBUILD did, and measured the two ways through:

* **2,065 wires** through two module boundaries, or
* **lift `u_tile` out** of `zhao_geom_bin_pipe_v2` — **a subsystem retirement on
  the tightest block.**

**It refused because it had no area number and was fenced off Quartus.** **You
are not.**

## YOUR JOB, IN ORDER

1. **PRICE BOTH OPTIONS**, on the **shipping part `5CSEBA6U23I7`**, rows
   `rtlCleanAtHead` true, **device named on every row**. Leaf `-MapOnly` is
   yours. **This is explicitly authorised**: the owner's standing authorization
   permits *"a synthesis/map or diagnostic fit before every software task is
   finished when it answers a concrete engineering question"*, and **this is
   that question.** Label the snapshot honestly.
2. **THEN DECIDE, AND SAY WHICH YOU DID:**
   * **Affordable** → **open the door and close the entry.** Every pixel must
     come from bytes that went through SDRAM.
   * **Not affordable** → **refuse with the number**, and say what the design
     would have to give up. **That is a full deliverable.**

**Do not build first and measure after.** Two packets this week refused
correctly because they priced before spending, and one (LANESCOST) predicted
**+90,000 ALUTs from arithmetic and measured +11,979 — 7.5× high.** **Refusing
on an estimate would have been right by accident and wrong in its reason.**

## THE ARITHMETIC YOU ARE MEASURING AGAINST

* The console needs **293,352 ALUTs against 227,120 present** on the sizing part
  — and against the **shipping** part's 83,820 it is **350%**, with DSP at
  **335%**. **209,532 ALUTs must come OUT.**
* **LANESCOST was refused at +11,979 ALUTs (14.3% of the part) and +9 DSP.**
* **A second attrpack/setup instance was refused at +1,621 ALUT and +40 DSP —
  35.7% of the entire multiplier budget.**
* **The campaign's largest single ALUT saving ever landed is −1,531.**

**A door that costs like either refusal will be refused.** **A door that is
mostly wires may be nearly free, and nobody knows which because nobody has
measured it.** That asymmetry is the whole reason this packet exists — and note
which direction the comfortable guess points: *"2,065 wires sounds huge"* is an
estimate, and **the last estimate on this campaign was 7.5× wrong in the
alarming direction.**

## The second blocker, deferred and NOT yours to force

`GEOM.SETUP` consumes `tri_area2_i` and the four scissored box bounds; **the
descriptor carries none**, and `zhao_geom_vertid` is not even handed them.
**The elegant escape is CIRCULAR** — `zhao_geom_setup.sv:386` **defines** `kc2`
as `area2 - kc0 - kc1`, so the barycentric identity recovers nothing and **a
back end built on it would be correct for any garbage `area2`.**

`DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md` decides the record (32 bytes, fits
with 524,288 spare) and **defers it for the right reason: it costs live write
bandwidth every frame for a consumer that cannot exist until the door opens.**
**If your door opens, say what that makes of the deferral. Do not build it
speculatively.**

## The fences

* **Do NOT half-do the swap.** ORing the walk into the live stream is forbidden
  and **six** packets have refused it.
* **Do not fake `paramwalk`** — a sequencer with dangling outputs counts
  triangles and drops them.
* **Do not regress the id repair or schema v2.** `geom_tidq_directed` is 83
  checks and **its 21 base-RTL failures must still fail**;
  `geom_paramarena_directed` is 550.
* **Do not touch `zhao_geom_clipdoor`, `zhao_geom_clip` or `zhao_geom_setup`.**
  A concurrent packet (`MATCARRY`) owns those and is **widening `tri_src_id`**.
  **You own `bin_pipe_v2`, `tile_pipe_v2` and `binner_v2`.**
* **`fpga/rtl/prod/zhao_console_core.sv` is SHARED with that packet.** **Stage
  the HUNK, not the file** — `git add <file>` stages work you did not write, and
  `git checkout --` DISCARDS theirs with no reflog.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010** for the identical circuit. Generate-**IF** infers; module
  scope infers; **the LOOP is the killer.** Correct remedy: one flat array **at
  a module's own scope**. **Rule 6 is NOT complete** — a live counterexample
  exists in the binner, cause unestablished.
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED** — check the row you wrote.
* **No console or full-device fit.** Leaf `-MapOnly` only.

## Evidence bar

* **Two priced options, like for like**, same device, same tree state.
* **If you open it: a pixel that depends on bytes that went through SDRAM** —
  §4's own test, and the thing that closes the entry. Today `paramwalk dirs=0
  chunks=0 tris=0` sits beside `raster pixels=2816`, and the smoke's `fetcharm`
  line reads `vread=0`.
* **If you refuse: the number**, and what would have to give.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Twelve packets this week found their own controls vacuous — including a
  positive control that had **silently become a no-op** because arithmetic moved
  one file away, and a test that printed a **hardcoded `0 failures`**.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **RUN GATE 31.** `check_console_closure_lint.py` caught **31 PINMISSING** on
  the last merge because that packet's gate list omitted it. **An intentionally
  unconnected output is `.port_o ()`, never an omission.**
* **A new core port costs four things**: the port, **BOTH** `.*` wrapper mutants
  (R220 — fix the wrapper, never the module), the smoke bench's wire, and a
  **reader**.
* **`UNUSEDSIGNAL` is waived across whole directories** — a dead wire raises
  nothing.
* **`zhao_console_core.sv` is LF.** **`git show` hands back INDEX content at
  LF** — normalise before believing conflicts.
* **Backticks in `git commit -m` get COMMAND-SUBSTITUTED** and silently eat your
  words. Use `git commit -F <file>`.
* **Three concurrent smoke forms is this box's ceiling**, another lane is live,
  and **other repos' suites run on this machine** — classify by command line and
  parent PID, **touch none**.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I55` closed.
2. **The two priced options**, device named, `rtlCleanAtHead` stated.
3. **Opened or refused — and the number that decided it.**
4. **If opened: a pixel whose bytes went through SDRAM**, and the `fetcharm`
   invariant moving off zero.
5. **What your answer makes of the TriangleDescriptor v2 deferral.**
6. **That the id repair and schema v2 both still hold**, controls re-fired.
7. **Every claim in this brief or the records you found FALSE.** Every packet
   this week found at least one; most were mine.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/doorcost` only.** Never `--force`.
