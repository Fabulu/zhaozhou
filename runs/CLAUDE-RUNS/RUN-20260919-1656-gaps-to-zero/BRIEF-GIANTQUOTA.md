# GIANTQUOTA — I56, the last gap that needs neither the owner nor a new refusal

**Branch `gz/giantquota`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Of the register's five, this is the one that is specified, unblocked and
buildable.** `I34` is escalated to the owner. `I13` has been refused three times
on measured grounds and now turns on two unsettled arithmetic laws. `I55` was
refused with numbers — the path is circular and 7.3× slower. **`I56` is a
build, and the architecture is already decided.**

## Read these two, in this order

1. Entry **`I56`** in `fpga/rtl/prod/zhao_console_core.sv` (`grep -n '^// I56\.'`).
   CHUNKSER derived the whole requirement on 2026-09-26 **so that you do not
   re-discover it.** Its list is reproduced below but **the entry is the
   authority**.
2. **`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §5, lines 347-359.** That
   is not background — **it is the specification**, and the entry says so.

## What is already TRUE, because two absence claims were measured false

* **The publication port EXISTS and is ENFORCED.** `zhao_geom_paramarena` takes
  `seal_valid_i` / `seal_ready_o` / `seal_verts_i` / `seal_tris_i` /
  `seal_chunks_i` / `frame_gen_i`, latches them with the view, enforces them
  (`q_chunks_q`, `ck_fits_c`), faults the frame on overrun and counts it at
  `quota_overflow_o`. **What is missing is a PRODUCER OF THE NUMBER, not a port
  to publish it into.** The old "waits on a Measure with somewhere to publish"
  was half true — `zhao_measure_tokens` really has no tile-reference port — and
  the load-bearing half was stale.
* **The declared LOD priority ALREADY EXISTS IN THE SHIPPED ABI.**
  `spec/commands.zidl` gives `DrawForm`, `DrawPopulation`, `DrawPosedForm` and
  `DrawWarpedForm` a `u8 semantic_weight` whose own comment says it *"feeds the
  Measure policy (degrade order)"*. It is decoded
  (`zhao_cmd_exec.draw_semantic_weight_o`), carried (`zhao_geom_drawjob.j_side_o`)
  and rides the meshlet through MESHFETCH and ASSETFETCH. **Nothing reads those
  eight bits.** An uncashed cheque, not a missing input — **"demote by declared
  LOD priority" needs no new ABI field.**

## The four items, derived and not estimated

1. **A SELECTOR** — a streaming max over the draw stream on
   `{semantic_weight, instance_id}`: highest weight wins, lowest instance id
   breaks the tie, which is the directive's *"stable instance identity"*.
   **One comparator. NOT a priority heap** — charter §9 forbids it and
   `zhao_measure_tokens` refuses it by name.
2. **A CLASS BIT ON THE ALLOCATION.** The arena has ONE chunk cursor and cannot
   tell a giant's chunk from an ordinary one, so **a reservation it cannot see is
   a reservation it cannot enforce.** It needs `ck_giant_i` beside `ck_valid_i`
   and a `seal_giant_chunks_i`, with `ck_fits_c` refusing ORDINARY chunks past
   `q_chunks_q - q_giant_q` while giant chunks may enter the reserve. **Only then
   is "the giant is never silently truncated" a property of the allocator instead
   of a sentence.**
3. **A DEMOTION CONSUMER, or the law is half enforced** — and half a law that
   *looks* enforced is worse than none. `zhao_geom_lodstate` **is composed**
   (`u_geom_lodstate`; older comments saying it is "NOT composed, anywhere" are
   STALE) and already holds a per-instance rung with hysteresis under R74. The
   demotion is a per-instance **coarsest-allowed-rung floor** written into it, so
   `c_rung_o` becomes `max(ladder, floor)`. **That is the existing declared
   deterministic order, not a second ladder.**
4. **THE PER-OBJECT REFERENCE COST — and this is why you have gates CHUNKSER did
   not.** It is the one quantity the console does not hold. The directive
   authorises computing the plan on the HPS and validating it in hardware, **so
   it arrives as a command field — an ABI change.**

## YOUR GATES INCLUDE THE ABI CHECK. That is the point of this packet.

**`npm run abi:check` is IN your gate set.** CHUNKSER declared item 4 rather than
doing it precisely because that check was outside its gates; I am authorising it
here, under the directive's standing authority over technical decisions.

Run it, and run the rest of the normal set. **If the ABI change turns out to need
something the directive does not cover, stop and write it up** — an ABI is a
contract with software that is not in this repo.

## THE UNIT TRAP, which is 14× in the flattering direction

**`seal_chunks_i` is CHUNKS and R7's 32,768 is REFERENCES.** The directive says
it outright: *"Do not confuse references with chunks or assume a reference budget
pays for every other structure."* `ceil(32768/14) = 2,341` chunks, which this
block's own parameter comment already computes — **and the reservation must also
cover the required vertices, descriptors and metadata, which the directive names
in the same sentence.**

**A reservation expressed in the wrong unit is 14× wrong and looks generous.**

## The fences

* **THE GUARANTEED GIANT MAY NOT BE TRIMMED, RESCOPED OR REDEFINED.** The
  directive lists *"shrink the guaranteed giant"* among the things this
  campaign's authority does not cover. 32,768 references is the number.
* **Do not renegotiate a sealed frame**, and **do not borrow from the giant's
  reservation because an early primitive happens to fit** — both are the
  directive's words. A frame explicitly containing **no** guaranteed giant may
  release the reservation **before** sealing.
* **Illegal or overflowing plans are REFUSED before the seal**, not clamped
  after it.
* **Do NOT half-build it.** CHUNKSER's reason for stopping is your standing
  instruction too: *"an unconnected quota block would move this entry from a
  tie-off to a DISCONNECTED IMPLEMENTATION, which the register counts as a gap
  and the campaign's first rule refuses as a closure."* **If you cannot finish
  it, leave the seal at capacity — which is the neutral choice and is declared
  rather than hidden — and report why.**
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`.**

## Evidence bar

* **The reservation ENFORCED, not asserted**: a frame whose ordinary allocation
  would eat the giant's reserve must be refused at `ck_fits_c` with the giant
  still whole — demonstrated, with the counter that fires.
* **The demotion actually demoting**: `c_rung_o` moving because the floor moved,
  on a real instance, through the composed `zhao_geom_lodstate`.
* **The selector picking the right object** under a tie, so the "stable instance
  identity" rule is exercised and not merely present.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison**. A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  driver polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* leaves `$LASTEXITCODE` carrying the previous command's value.
* **`gate_sweep` does not run the console smoke controls.** Run them with
  `@splat` — and note that a hashtable splat through `powershell -File`
  stringifies the switches so they never start. That trap has produced false
  GREENS and false REDS in this campaign on the same day.
* **Regenerate `zhao_prod_top.sv` after ANY port change** (you will have port
  changes) and re-run `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable** — Quartus 17.0 needs an
  elaboration check inside `initial begin ... end` and explicit
  `generate`/`endgenerate`.
* **One `ctest` at a time per build tree.**

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **The register before and after, measured BARE**, and whether `I56` closed.
2. **The enforcement evidence** — the refused frame, the giant intact, the
   counter that fired.
3. **The ABI change**, what it adds, and the `npm run abi:check` result.
4. **The units**, explicitly: references, chunks, and what the reservation
   actually covers.
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one, several of them mine.
6. **What you refused.** If you could not finish, say so and leave the seal at
   capacity.
7. **Anything you got wrong and caught yourself.**
8. The branch and commit hash. **Push `gz/giantquota` only.** Never rebase or
   push the shared branch, and never `--force`.
