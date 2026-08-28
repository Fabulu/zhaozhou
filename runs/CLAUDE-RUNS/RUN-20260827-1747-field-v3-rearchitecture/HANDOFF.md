# HANDOFF — RUN-20260827-1747-field-v3-rearchitecture

**Status: ACTIVE.** Written 2026-08-28 evening so another agent can take this
over cold. `TASK_LOG.md` is the narrative and the reasoning; this file is the
state of play and the commands.

---

## What this run is

The Field IR engine, v3: a four-wide vector fabric sharing one multiplier bank,
with every block checked against the shipped interpreter and then mutation-swept.

`SPEC_v1.md` holds the original objective and has not drifted.

---

## Where it stands

### Blocks closed — sweep passed, nothing unaccounted for

| block | mutants | result |
| --- | ---: | --- |
| FIELD.V3.EXEC | 42 | **re-scoring as this was written** — see "In flight" |
| CURVE.SVC | 18 | 18 caught |
| NOISE | 23 | 23 caught |
| DISPATCH | 28 | 28 caught |
| WBARB | 17 | 17 caught |
| ROT | 24 | 24 caught |
| RING | 23 | 23 caught |
| SPLINE | 21 | 20 caught, 1 proven equivalent |
| NORMALIZE | 26 | 25 caught, 1 proven equivalent |
| SVCPATH | 25 | 21 caught, 4 proven equivalent |

Every equivalence carries a written proof and a **re-score trigger** — the
condition under which it stops being equivalent. They are in the `EQUIVALENT`
dict of each `tools/sweep_*_mutants.py`, machine-readable, and the driver
refuses to accept a survivor without one.

### Gates

    npm run ledger:check                      green
    ctest -L fast                             305/306
    python tools/sweep_anchors_check.py       675 anchors, 26 tables

The one failing test is `creature_core`, which belongs to the creature session
and is **deliberately not touched** — Fabian's direction #3 tells that agent to
break the S-curve intentionally and re-pin its goldens, so red there is
plausibly expected mid-rework.

### Defects fixed in shipped RTL this run

Both were invisible to every test that existed that morning, and both needed
**more than one context** to reproduce:

1. **The register file's read is a pipeline stage a freeze does not freeze.**
   On a bank denial the front of the pipe holds, but the read already in flight
   does not — so the stalled instruction was paired with its SUCCESSOR's
   operands. 21 of 48 context-programs wrong; 0 after.
2. **The multiplier's accounting sat inside the frozen block** while the
   multiplier kept delivering. Its own desync alarm latched on 12 of 12
   programs; 0 after.

---

## In flight at handoff

A 42-mutant re-score of FIELD.V3.EXEC:

    runs/CLAUDE-RUNS/RUN-20260827-1747-field-v3-rearchitecture/field_v3_exec_sweep.log

**Read the tally at the end of that file before doing anything else.** The
previous run of it left one survivor, `X46`, and the test was widened to eight
contexts specifically to kill it — that is the question this run answers.

If X46 survives again: it says the issue gate may stop a clock late without any
test noticing. It is a real fragility, not a nuisance. Either find traffic that
exposes it or prove it equivalent WITH a measurement; do not delete it.

---

## The two decisions that are Fabian's, not an agent's

Both are recorded in `STATUS.md` with their prices. **Do not guess at either.**

1. **SPLINE hot or cold.** A four-point SPLINE block exists and is closed at
   21/21 — but `Fieldv3.md` section 6 puts spline on the COLD service lane, and
   `zhao_probe_curve_svc.sv` says so in its own header. Cold means the existing
   scalar path in `zhao_field_curve.sv` already implements the whole op and
   nothing further is needed. Hot means widening a shared service that has
   eighteen mutants riding on its current shape, all of which need re-scoring.
2. **`UOP_RING_PREP` (0xF1) is a genuine gap.** The brief costs the PREPARED
   ring as its hot path, `zhao_field_v3_ring.sv` implements it and is swept
   23/23, and `dst_width_of` in the dispatcher cannot reach it. Note it is
   0xF1, a uop — NOT `OP_RING` (0x21), which is the varying-radius form and
   stays cold.

---

## Next concrete steps

1. Read the exec sweep tally (above).
2. Re-run `ctest -L fast` — the skid landed after the last full lane run.
3. Wire `zhao_field_v3_svcpath` into `zhao_probe_v3_engine`. **This is now
   unblocked**: the executor's write port can be refused safely (see the skid,
   below), which was the thing that would have silently dropped writes.
4. The two-service starvation question on the bank still needs the curve
   service and the noise unit attached at once. One service cannot starve
   anybody, so it cannot be answered before then.

---

## Traps this run actually hit — all of them cost real time

`docs/BUILD.md` has the full versions. The short list:

* **A heredoc eats one backslash.** It broke five separate edits here. Use
  `BS = chr(92)` and a placeholder, or write the script to a file with the
  Write tool instead of piping it through the shell.
* **`/tmp` differs between Git bash and Python.** Absolute paths only.
* **A build tree can be poisoned by the CASE of a path** and it fails at LINK,
  on the one target that builds two models. Proved stale rather than a CMake
  bug by configuring a throwaway tree; **clearing the cache does NOT repair it**
  — that is measured, not assumed. Delete the tree.
* **One build tree, one writer.** Two `ninja`/`ctest` in the same tree, or a
  repair attempt beside a cold build, and both lose.
* **A stopped background task is not a stopped process.**
* **`git add` during a sweep** freezes whatever the file says at that instant.
  Always `python tools/git_add_safe.py`, never bare `git add`.
* **Ask `python tools/git_add_safe.py --check` before ANY multi-file RTL edit.**
  A sweep reconfigures the whole project before every rebuild, so a moment when
  the tree does not configure kills it — in a file the sweep never touches.

## Lessons that are about judgement, not tooling

These cost more than the traps, and none is in a build note:

* **A result that was true when measured is not true now.** `FIELD.V3.EXEC
  31/31` was quoted for seven hours after the RTL it described had moved.
  `tools/sweep_anchors_check.py` exists to catch exactly that, in a second.
* **A gate you do not run is worse than one that skips** — a skip at least
  leaves a line in a log. The ledger was red for eight hours unnoticed.
* **Testing two contentions one at a time proves nothing about either.** The
  write port alone passed 37 checks over broken silicon; it took port refusal
  AND bank contention together to see it.
* **One context is not a weaker test, it is a different one.** Two shipped
  defects needed four contexts, and a third needed eight.
* **A comparison between two counts the same bug touches is nearly worthless.**
  Both sides were short by the same amount for hours. Only an ABSOLUTE law —
  "one register written per uop, per context" — has an outside reference, and
  it caught both a duplicate write and a test that ticked the clock unobserved.
* **Change one thing.** The skid was reverted for a defect it did not cause;
  the isolation pass that proved it innocent took four minutes and should have
  come first.

---

## The skid, because it is the newest and least obvious piece

`zhao_probe_v3_exec.sv` retires a write into a four-deep queue rather than
stalling, because **the multiplier is fixed-latency and cannot be stalled** — a
product issued at T arrives at T+2 whatever the pipe does, so freezing an
instruction while its product arrives desynchronises the two. That was measured
wrong twice before the queue existed.

Three properties are load-bearing, and each has a mutant:

* **the depth is DERIVED** — issue stops when the queue is non-empty, S1..S3 may
  hold three more instructions, one plus three is four;
* **the issue gate is COMBINATIONAL** — the registered form stops a clock late
  and admits a fifth instruction (mutant X46);
* **`sk_overflow_o` exists** because a dropped write changes a VALUE and not a
  COUNT, so no counting law can see an overflow.

The bypass keeps the uncontended case free, and that is checked rather than
claimed: barrel occupancy is unchanged at 69 and 190 clocks.
