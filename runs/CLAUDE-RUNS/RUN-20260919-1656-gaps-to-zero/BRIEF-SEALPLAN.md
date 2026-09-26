# SEALPLAN — I56's admission plan, and the field the owner PRE-AUTHORISED

**Branch `gz/sealplan`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Three packets have touched `I56` and none built the seal.** Each refusal was
right about what it refused and each left the same thing undone. **You are
building it.** Read entry `I56` (`grep -n '^// I56\.'`, never a line number) —
the GIANTQUOTA, REFPUSH and GIANTREFS sections are measurements, not opinions.

## The ground is now clear, and it was cleared by measurement

* **GIANTQUOTA** proved the arena's publication port already exists and is
  enforced (`seal_valid_i`/`seal_ready_o`/`seal_verts_i`/`seal_tris_i`/
  `seal_chunks_i`/`frame_gen_i`, `q_chunks_q`, `ck_fits_c`, faulting the frame
  and counting at `quota_overflow_o`). **What was missing is a PRODUCER OF THE
  NUMBER, not a port.** It also landed the chunk arm's positive control, which
  had never been seen to fire anywhere in the tree.
* **REFPUSH** proved the reference push is not the seam: it has the identity but
  **3.1% of the capacity** — 1,024 references against a 32,768 reserve.
* **GIANTREFS** then **fixed that**: the binner now holds **32,768 references**,
  R7's number exactly, measured on the shipping part with both rows
  `rtlCleanAtHead: true` (526,592 bits / 2,130 registers, +33 M10K, 9.30% of the
  ceiling). **A giant was demonstrated surviving: 25,920 references binned
  whole, all drained, `overflow_o = 0`.**

**So the capacity is real and the enforcement port is real. What does not exist
is the PLAN.**

## WHAT THE OWNER ALREADY DECIDED — this is your specification

`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` **§5**, quoted because it is
the spec and because two packets avoided the thing it authorises:

> *"Commission a real per-view pre-frame allocation/admission plan and an
> immutable quota-seal record consumed by the arena. The Measure owns the
> validated plan. **The architect may compute the plan on the authorized HPS
> path and validate/seal it in hardware; it is not required to duplicate a large
> policy engine merely to avoid adding a command or mailbox field.**"*

> *"The seal names frame/view/resource generation, capacities,
> vertex/triangle/reference/chunk limits, and the designated giant's
> reservation. **Keep those units distinct.** **Authorize the necessary
> generated command, publication or service transport and connect its real
> producer and consumer.** **A constant equal to arena capacity is not an
> admission plan.**"*

**Read that last sentence against the current state: the seal IS a constant
equal to arena capacity.** That is the gap, named by the owner, in the owner's
own words.

**GIANTQUOTA declined the ABI change on a sound ground** — a field whose only
consumer is an enforcement seam that does not exist is an uncashed cheque with
an ABI's blast radius. **§5 answers it: build the consumer and the field
together and the objection dissolves.** `npm run abi:check` is in your gates for
exactly this.

## The four things the seal must carry, and the unit trap

1. **Generations** — frame, view, resource.
2. **Capacities and limits** — vertex, triangle, **reference**, **chunk**, kept
   as **distinct units**.
3. **The designated giant's reservation.**
4. **A real producer and a real consumer**, both connected.

**THE UNIT TRAP IS 14× AND IT FLATTERS.** `seal_chunks_i` is **CHUNKS** and R7's
32,768 is **REFERENCES**. `ceil(32768/14) = 2,341` chunks. The directive says it
outright: *"Do not confuse references with chunks or assume a reference budget
pays for every other structure."* **A reservation expressed in the wrong unit is
14× wrong and looks generous.**

**And REFPUSH measured the opposite error too**: 32,768 read as *chunks* is 200%
of `MAX_CHUNKS` and **refuses itself loudly** — which is the safe direction. The
flattering error is reserving 2,341 chunks and calling the giant covered.

## Also available, and unread

**The declared LOD priority already exists in the shipped ABI.**
`spec/commands.zidl` gives `DrawForm`, `DrawPopulation`, `DrawPosedForm` and
`DrawWarpedForm` a `u8 semantic_weight` whose own comment says it *"feeds the
Measure policy (degrade order)"*. It is decoded, carried and rides the meshlet
through MESHFETCH and ASSETFETCH — **and nothing reads those eight bits.** If
your plan needs a degrade order, **it needs no new ABI field for it.**

**And the demotion floor already ships as a pattern**: `zhao_forge_shadow.sv:255`
is `max(ladder, floor)` under a live per-camera floor. `zhao_geom_lodstate` is
composed and holds a per-instance rung with hysteresis under R74. **Copy the
proven pattern; do not design one.**

## The fences

* **THE GUARANTEED GIANT MAY NOT BE TRIMMED, RESCOPED OR REDEFINED.** *"Shrink
  the guaranteed giant"* is on the directive's list of what the delegation does
  not cover. **32,768 references is the number.**
* **Do not renegotiate a sealed frame**, and **do not borrow from the giant's
  reservation because an early primitive happens to fit** — both the directive's
  words. A frame explicitly containing **no** guaranteed giant may release the
  reservation **before** sealing.
* **Illegal or overflowing plans are REFUSED before the seal**, not clamped
  after it.
* **NOT a priority heap.** Charter §9 forbids it and `zhao_measure_tokens`
  refuses it by name. A streaming max over `{semantic_weight, instance_id}` is
  one comparator.
* **If the ABI change needs something the directive does not cover, STOP and
  write it up.** An ABI is a contract with software that is not in this repo.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours;
  **every map you quote must name its `-Device`**, and **read `rtlCleanAtHead`
  before quoting ANY row** — a dirty row carried a live +33 M10K claim this week.

## Evidence bar

* **The plan PRODUCED and the seal CONSUMED** — a real per-view number reaching
  `seal_*_i`, not a constant. **"A constant equal to arena capacity is not an
  admission plan"** is the acceptance test, in the owner's words.
* **The reservation ENFORCED, not asserted**: a frame whose ordinary allocation
  would eat the giant's reserve **refused at `ck_fits_c` with the giant still
  whole**, and the counter that fires. GIANTQUOTA's directed case 3b is your
  starting point and is committed.
* **The units demonstrated distinct** — references, chunks, vertices,
  triangles — with a case that would pass under the 14× confusion and fails.
* **`npm run abi:check`**, and note it is a **drift** gate, not a semantic one:
  it goes green on a field nothing reads. **Your evidence that the field is read
  is the consumer, not the checker.**
* **Prove every counter you quote**, and **check what clocks the two sides of
  any comparison.** A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's** — REFPUSH misread the register through `| tail` today.
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change; it caught 7 PINMISSING for BINARENA today.
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **A `$fatal` elaboration guard WEDGES ctest at 0.00 CPU** — GIANTREFS nearly
  registered one. Make it a build target, not a test.
* **A suite reads the LIVE TREE** — do not edit the core while controls verilate
  it; freeze and re-run.
* **`reports/synthesis/zhao_block_fit.json` reserialises** — verify no row is
  lost, count before and after.
* **Stage your HUNK on shared files**, never `git add <file>`.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I56` closed.
2. **The plan** — who produces it, what transport carries it, who consumes it.
3. **The enforcement evidence**: the refused frame, the giant intact, the
   counter that fired.
4. **The units, explicitly**: references, chunks, vertices, triangles, and what
   the reservation actually covers.
5. **The ABI change and `npm run abi:check`** — and what proves the field is
   READ.
6. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
7. **What you refused.** If you cannot finish, leave the seal at capacity and
   say why — noting the directive names that state *"not an admission plan"*, so
   it is a declared shortfall, not a resting place.
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/sealplan` only.** Never `--force`.
