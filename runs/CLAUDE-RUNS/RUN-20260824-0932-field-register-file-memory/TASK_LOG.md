# Task Log: RUN-20260824-0932 — FIELD register file to block memory

**Created:** 2026-08-24 09:32 UTC+02:00
**Status:** In Progress

---

## Objective

`zhao_field_seq`'s 64×32 register file from flops behind four asynchronous 64:1
muxes into inferred block memory. Values bit-identical to `zfield::interpret`.

---

## Progress Timeline

### 09:32 — measured before choosing, and two premises of the ruling failed

Recorded on the docket. **The three constant ROMs the Field ruling names as a
principal cost total 328 ALMs** (87 + 79 + 162 for 16,448 ROM bits) — Quartus
constant-folds a case tree over *constants* very well, and §10's penalty curve
was measured on read/write **arrays**, which is a different problem. And
`progcache`'s 2,237 ALMs over two async, reset-touched arrays is **not a
defect**: they are a tag array and an LRU array, and associative compare is
inherently logic. The scanner agrees at `arraysExpectingRam: 0`.

**Not every array that fails to infer is a bug**, which is a useful counterweight
to the rule this campaign has been applying all week.

### 09:40 — design written BEFORE the RTL, and one part of it was wrong

Two things came from reading rather than assuming:

* **the three lane writes are not simultaneous.** `rf[i_dst]`, `rf[i_dst+1]`,
  `rf[i_dst+2]` sit in an `if / else if` chain across `Q_MWAIT`, `Q_WB1`,
  `Q_WB2` — at most one write per edge. I first read them as three writes on one
  edge, which would have made this wave depend on the ruling's wave 4. **The
  write side was already single-port.**
* **the address mux needed no change.** It already keys off `state`, and
  `Q_LATCH` already addresses from `ins_*_i` because `i_a`/`i_b` latch at the
  END of that state — the existing comment says exactly why.

**And one part of the SPEC was false**, corrected in it: I wrote "safe to clock
in `read_reg`: it is only called once the walk is idle." §14 of the directed test
watches a register **every cycle of a live run**.

### 10:10 — MEASURED

| | before | after |
| --- | ---: | ---: |
| `blockMemoryBits` | **0** | **8,192** |
| inferred memories | 0 | **4** |
| ALMs | 7,958 | **5,142** |
| registers | ~5,288 | **3,305** |
| DSPs | 3 | 3 |
| simple-op latency | 6 clocks | **7** |

**−2,816 ALMs, −35.4%.** Four copies, 8,192 bits, packed by Quartus into **one
M10K** of 502 free.

**I predicted −1,371 and was wrong by a factor of two.** The calibration prices a
*standalone* 64×32 array (1,411 broken against 40 inferred). In situ, removing
the four asynchronous 64:1 muxes also removed the logic feeding them and the
reset fan-out across 2,048 flops — **registers fell by 1,983**, which is most of
the difference.

> **A microbenchmark prices the component. It does not price the component's
> blast radius.** Carry this into the remaining candidates: where the calibration
> predicts a saving from removing a structure, the structure's *fan-out* may be
> worth more than the structure.

That also makes my morning docket entry wrong twice over — I said the ruling's
3,500–5,000 ALM band "does not follow" and predicted 6,587. It is **5,142**,
inside the band. Both corrections are recorded.

### 10:15 — three test changes, each nearly misdiagnosed as an RTL bug

The host read port is synchronous now, and that rippled:

1. **`read_reg()` must clock.** Without it every chained result read back as the
   **previous** instruction's answer — `chain of 5` returning `chain of 2`'s
   value, and so on down the table.
2. **but `read_reg()` must NOT clock inside §14**, which watches `reg[20]` every
   cycle of a live run. Clocking there advanced the clock **twice per iteration**
   and swallowed `instr_retired_o` pulses, reporting *work as lost* — the same
   shape as the `terrain_normals` bug an hour earlier, and this time it was the
   harness. Fixed by holding the address and reading the port directly; `step()`
   does not touch `rf_raddr_i`.
3. **§14's early-write check needs `n > planted_cycle`**, because the port now
   lags a cycle and shows the pre-sentinel value on the cycle the plant retires.

**The give-away for (3) is worth keeping: `change_cycle` was identically 6 for
seven different ops whose retire cycles range from 22 to 85.** A constant where
there should be variance is an observer artefact, not a design one.

`-Wall` also caught four dead address registers I had added speculatively — the
valid bits do the gating, not the addresses.

### 10:30 — the sweep preflight refused to score, correctly

Five mutants anchored on text the change moved, and the preflight **aborted**:
*"fix the mutation, not the guard."* An anchor that no longer matches makes a
mutant unbuildable, and an unbuildable mutant re-runs the previous binary and
scores its own build failure as a **catch** — the error that once turned a real
22/23 into a reported 21/22.

Four moved with the schedule: slot 0 issues in `Q_RD1` now, and the third slot
and group-2 capture moved to the new `Q_RD3`.

**M20 is the one worth recording. Its anchor matched TWICE** —
`ANCHOR NOT UNIQUE (2)`. Factoring the write into a combinational decode put the
same guard condition in two places: the decode that gates the memory write, and
the sequential block that sets the valid bit. Re-anchored on the decode. M33
likewise — the *data* write moved into the decode, so mutating the sequential
block would no longer commit anything early.

> **A mutation can survive not because the test is weak but because the mutation
> no longer does anything.** The preflight catches the unbuildable case; only
> reading the mutation catches the inert one.

38 mutants across 11 files, **0 do not build**.

---

## Decisions Made

**Four replicated copies rather than two dual-port RAMs.** M10Ks are not scarce
here (one used, 502 free) and replication keeps the read paths independent.

**A valid bitmap rather than clearing the array** — required, not merely
cheaper, because M10K contents are **undefined after reset** and a read must
still answer zero.

**The latency assertion updated 6 → 7**, and legitimately: the extra clock was
written into the SPEC *before* the RTL, which is what separates a recorded
consequence from a test bent to fit a result.

---

### 12:40 — SWEEP: 38/38 accounted, 33 caught, and the score is UNCHANGED

    attempted=38  expected=38  accounted=38  caught=33

**All five survivors are documented, proven equivalents**, and they are the same
five as before this change:

* **M01** (lane operand registers not held between issues), **M07** (read-slot
  shadow one stage deep) and **M20** (the write-back guard) — proofs in
  `RUN-20260822-2136/FINDINGS-dsp-field-engine.md:544-558`;
* **M36** (zero-length guard dropped) and **M38** (a leading-zero stage tests the
  wrong half) — proved by the Field agent in the LZ replacement.

**So the register-file conversion introduced no coverage hole.**

**The M20 question needed checking rather than assuming**, because I had
re-anchored it and had written down the risk that *a mutation can survive
because it no longer does anything.* It resolves cleanly: M20's equivalence is
**semantic** — `zhao_field_alu`'s `default:` arm makes the `&& !multi_op` guard
redundant — so the proof is about what the mutation means, not where it sits.
Re-anchoring moved the text and not the argument.

Had the proof been positional, the re-anchor would have invalidated it silently,
and a survivor that *looks* like a known equivalent is the most comfortable
possible way to hide a real hole.

---

## Next Steps

- [ ] contract: record that the host read port is now synchronous
- [ ] a fit, for the timing question this was really about — **no slack number
      may be quoted until a real `.sta.rpt` is read**
- [ ] archive
