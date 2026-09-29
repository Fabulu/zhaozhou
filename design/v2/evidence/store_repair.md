# The uop store: cause found, repair landed, and what it does NOT mean

Coordinator, 2026-09-29. **This file supersedes every earlier claim in this folder
about the CAUSE.** `store_cause_hypothesis.md` and `uop_store_probe.md` remain for
the record with correction banners; read this one for the answer.

---

## The answer, in one line

`uop_t store[0:(CTX*PLAN)-1]` — 384 × 60 = 23,040 bits at the console's
`.INSTR_N(48)` — stayed in flip-flops because its fetch was nested inside
`if (!hold_c && !mul_denied_c)`, **a gate that was redundant for that one
assignment**. Hoisting the statement out of it, and nothing else, makes Quartus
infer the array as a Simple Dual Port M10K.

## How it was isolated: five maps of the real module

Every row is `zhao_probe_execstore`, a generated diagnostic reduction of
`zhao_field_v3_exec`, mapped at PLAN=48 and classified **by array name** with
`tools/budget/store_variant_receipt.py`. Rows are in
`reports/synthesis/zhao_block_map.json`.

| variant | read position | enable on the read | verdict |
|---|---|---|---|
| production | after the write | nested: `!hold_c && !mul_denied_c` then `issue_c` | **UNINFERRED** |
| `@R1` | before | none | INFERRED |
| `@R3a` | before | `issue_c` alone | INFERRED |
| `@R3c` | before | unchanged nested pair | **UNINFERRED** |
| `@R3d` | after | `issue_c` alone, hoisted | INFERRED |

**`@R3a` eliminated the enable by arithmetic rather than by measurement.** From
`zhao_field_v3_exec.sv:378`:

```systemverilog
issue_c = |ready_c && !dot_inflight_c && !hold_c && !mul_denied_c && !sk_busy_c;
```

so `(!hold_c && !mul_denied_c) && issue_c` is *identically* `issue_c`. Production's
effective read enable and R3a's are the same Boolean, and one infers while the other
does not. R3a also refutes — in the real module, not in a probe — the hypothesis that
an enable derived from the read's own output blocks inference: `issue_c` depends on
`s1_uop_r.op` through `dot_inflight_c`, and it inferred anyway.

**`@R3c` is the decisive row, and it is a perfect negative.** It moved the write
below the read, changed nothing else, and came back byte-identical to the baseline:

| | registers | comb ALUTs | est. ALMs | memory bits |
|---|---:|---:|---:|---:|
| baseline `@prodparams` | 24,962 | 10,287 | 17,100 | 25,344 |
| `@R3c` | 24,962 | 10,287 | 17,100 | 25,344 |

Not one ALUT of difference. **Statement order is free; the nesting is the cause.**

## Why the repair is free

The hoist is **provably behaviour-preserving**. `s1_uop_r` updates on exactly the
clocks it updated on before, because the enable is the same function of the same
signals. There is no extra architectural cycle — the fetch was already inside a
clocked process — no schedule change, no addressing change, and no new collision
contract: read and write are both nonblocking in one process on one clock, so a
same-address upload and fetch still reads the **old** word, which is precisely what
an M10K in Simple Dual Port mode with old-data read-during-write does.

`s1_v_r` and `inflight_r[issue_ctx_c]` deliberately **stay inside** the freeze,
where the gate is *not* redundant. Hoisting `s1_v_r <= issue_c` would drive it to 0
during a hold instead of holding its value, breaking the retry the freeze exists to
provide. That asymmetry is the whole reason the outer gate is there at all.

## The measurement

PLAN=48, map-only, standalone:

| | registers | comb ALUTs | est. ALMs | memory bits |
|---|---:|---:|---:|---:|
| before | 24,962 | 10,287 | 17,100 | 25,344 |
| after | 1,862 | 2,110 | 2,016 | 48,384 |
| **delta** | **−23,100** | **−8,177** | **−15,084** | **+23,040** |

+23,040 is exactly 384 × 60, and the inferred object is named: `store_rtl_0`,
**60 wide, 384 deep, Simple Dual Port** — the production payload and depth. The ALUT
fall is larger than the flip-flops alone because a 384-entry flop array also needs a
384-way read mux, and both go.

## The semantic evidence, and a finding inside it

The differential is against `zfield::execute_point`, the interpreter the Field IR is
defined by. Checking the *harness* against the production parameters, as R6 §1 asks,
produced a finding of its own: `tests/CMakeLists.txt` verilated
`test_field_v3_exec_directed` with no `-G`, so it ran at the module default
`PLAN = 32` while the console composes `.INSTR_N(48)`. **The executor's differential
had never run at the parameterisation the console ships** — and could not have,
because `up_pc_i` is `[$clog2(PLAN)-1:0]`, five bits at 32, so indices 32..47 are
not expressible.

`test_field_v3_exec_plan48` now runs the identical source with
`-DZHAO_TEST_PLAN=48` and `-GPLAN=48`, and `test_store_depth_and_context_isolation`
drives a deep program in contexts 0 and 7 with independent inputs, comparing each
against the interpreter for *its own* data and checking that neither context
received the other's answer.

| arm | deepest index written | result — before the repair | after |
|---|---|---|---|
| PLAN=32 | 31 of 32 | 49 checks passed | 49 checks passed |
| PLAN=48 | 47 of 48 | 49 checks passed | 49 checks passed |

with every measured number identical across the repair: 13 uops in 69 clocks on one
context, 104 in 190 on eight, 12 programs × 8 contexts with 2,311 clocks refused, 0
desyncs. For a behaviour-preserving change, **unchanged numbers are the prediction**
rather than a suspicious silence — and the verilated model was confirmed rebuilt
after the edit rather than assumed, because CLAUDE.md's stale-binary trap produces
exactly this signature for the opposite reason.

**The depth test is its own negative control.** The store is addressed
`ctx * PLAN + pc`. If the depth silently stayed at 32 — or if a future layout
replaced that arithmetic with the `{ctx, pc}` concatenation the external checker
warns about — pc=47 truncates to 15, overwrites that context's own uop 15, and the
comparison fails. Nothing has to be asserted about the parameter; a wrong depth
cannot pass.

## Is the cause anywhere else? Measured: not cleanly

`tools/budget/nested_read_candidates.py` cross-references the committed
uninferred-array ranking against the RTL for the same shape — a clocked read under
two or more conditions.

| threshold | candidates | with another cause already named |
|---|---:|---:|
| ≥ 4,096 bits | 4 | **4** |
| ≥ 512 bits | 19 | **19** |

Every one of them is also multidimensional, or has two distinct write addresses, or
is read combinationally — causes no amount of hoisting touches. **There is no array
in the tree for which un-nesting is plausibly the binding repair.** The lever is
specific to this module.

An earlier throwaway version of that triage reported twenty-odd candidates including
three arrays *larger* than the store, and it was wrong: it counted `begin`/`end`
depth, which includes the process's own `begin` and the reset `else begin`, so
anything in the plain body scored 2 and looked identical to the broken store. The
committed tool counts **conditions**, excludes the reset by name, and carries a
dangling `if (c)` forward. Its self-test caught the error on the store's own
before/after shapes, inlined rather than read from the tree because the repair
deleted the "before".

## What this is NOT

* **Not a placed saving.** Estimated ALMs at map-only are estimates. −15,084 has
  not been through a fit.
* **Not a timing result.** Map-only says nothing about setup/hold, and an array
  moving from flip-flops into an M10K changes the paths around it. It could plausibly
  help — a 384-way mux is not a fast structure — but that is a hypothesis, and this
  file has already been wrong three times about hypotheses.
* **Not a physical block count.** A map report gives logical memory bits. 60 bits
  per entry exceeds a single M10K's widest mode, so the array needs width banking;
  how many of the device's 553 blocks it occupies is a fit question.
* **Not a console rescue.** One array in one module. V1 measured 293,886
  combinational ALUTs against 83,820 available — 351% — and this does not change the
  order of that problem.
* **Not a V2 result.** It is a local V1 repair, banked, exactly as R6 §5 frames it.
  The Field-slice replacement question is untouched by it.

## Three predictions of mine were refuted getting here

Worth keeping, because the pattern is the same each time: a real correlation
promoted to a mechanism before it was tested.

1. **"The combinational read address is the blocker."** Found that the arrays which
   infer have registered addresses and the one that fails does not — then read a
   *sufficient* condition as a *necessary* one. `@R1` inferred with the address
   untouched.
2. **"The six probe styles eliminated five causes."** They eliminated nothing. A
   synthetic probe that INFERS can only show that some arrangement works; only a
   probe that FAILS isolates a cause. STYLE=3 produced a false elimination of the
   very hypothesis `@R3a` later had to test.
3. **"Statement order is the cause."** `@R3c` came back byte-identical.

Only `@R3d`'s prediction held.
