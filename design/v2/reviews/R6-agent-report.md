# Report after 335eae36: the store is repaired, in production, measured

Coordinator, 2026-09-29, branch `design/zhaozhou-v2-rfc`.

Answering the request in `R6-external-feedback-after-335eae36.md`: *"either a
production-shaped store replacement with matched before/after and differential
evidence, or a minimized still-failing production case plus the next explicit-memory
candidate."* It is the first of those. Full evidence in
[`../evidence/store_repair.md`](../evidence/store_repair.md).

## 1. The real blocker

The fetch was nested inside `if (!hold_c && !mul_denied_c)`, and **that gate is
redundant for that one assignment**: `issue_c`, the inner condition, already contains
both of its terms (`zhao_field_v3_exec.sv:378`). So
`(!hold_c && !mul_denied_c) && issue_c` is identically `issue_c`, and Quartus refused
the array over a structure that carried no logic.

Not an explicit-memory backend, not an addressing change, not a schedule change. The
repair is one statement hoisted out of a redundant gate.

## 2. Exact before/after configuration

Identical in both rows: device `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2, map-only,
standalone, `sourceListHash 47f29d31d3332652`, 408 source files, `rtlCleanAtHead:
true`. Parameters `LANES=1, CTX=8, REGS=32, LONGQ=4, PLAN=48` — `PLAN` overridden to
the console's `.INSTR_N(48)`, the rest at the values the console composes.

| ledger row | source commit | registers | comb ALUTs | est. ALMs | memory bits |
|---|---|---:|---:|---:|---:|
| `zhao_field_v3_exec@prodparams` | `45b880cc` | 24,962 | 10,287 | 17,100 | 25,344 |
| `zhao_field_v3_exec@repaired` | `4c807992` | **1,862** | **2,110** | **2,016** | **48,384** |
| delta | | −23,100 | −8,177 | **−15,084** | +23,040 |

Both rows are maps of **production RTL**, not of a probe. The diagnostic variant
`@R3d` produced byte-identical numbers, which is the cross-check that the reduction
and the repair are the same change.

## 3. Store mapping evidence

`altsyncram:store_rtl_0|altsyncram_o3q1:auto_generated|ALTSYNCRAM`, **width 60, depth
384, Simple Dual Port**. The width and depth are the production payload and capacity,
so the object is identified rather than inferred from a total. `+23,040` memory bits
is exactly 384 × 60. The baseline row carries no inferred memory named `store`, and
its register count is unchanged from the pre-repair source, so the array was not
deleted in either direction.

Classified by name with `tools/budget/store_variant_receipt.py`, whose verdicts are
`INFERRED / UNINFERRED / ABSENT / AMBIGUOUS` — `ABSENT` existing because a reduction
can make synthesis delete the array, and deletion is not inference.

**Physical M10K count is not established.** A map report gives logical memory bits;
60 bits per entry exceeds one M10K's widest mode, so the array needs width banking,
and how many of the device's 553 blocks it occupies is a fit question. Your two-bank
suggestion is the layout to verify, not something this measurement confirms.

## 4. Semantic test results, and a finding inside them

Your §1 asked for the resolved production parameters rather than an override string.
Applying that to the **harness** produced a finding: `tests/CMakeLists.txt` verilated
the executor's differential with no `-G`, so it ran at the module default
`PLAN = 32`. **The differential had never run at the parameterisation the console
ships**, and could not have: `up_pc_i` is `[$clog2(PLAN)-1:0]`, five bits at 32, so
indices 32..47 are inexpressible.

`test_field_v3_exec_plan48` now runs the identical source with `-DZHAO_TEST_PLAN=48`
and `-GPLAN=48`. The oracle is `zfield::execute_point`, the interpreter the Field IR
is defined by.

| arm | deepest index written | before | after |
|---|---|---|---|
| PLAN=32 | 31 of 32 | 49 checks passed | 49 checks passed |
| PLAN=48 | **47 of 48** | 49 checks passed | 49 checks passed |

`test_store_depth_and_context_isolation` drives a deep program into contexts 0 and 7
with independent inputs, compares each against the interpreter for **its own** data,
and separately checks that neither context received the other's answer. It is its own
negative control for the hazard in your §2: the store is addressed `ctx * PLAN + pc`,
so a depth that silently stayed at 32 — or a `{ctx, pc}` concatenation — truncates
pc=47 to 15, clobbers that context's own uop 15, and the comparison fails.

**Collision law preserved by construction, not by argument.** Read and write are both
nonblocking in one process on one clock, so a same-address upload and fetch reads the
OLD word either way. That is the same law as an M10K in Simple Dual Port with
old-data read-during-write, so no caller guarantee was needed and `don't care` was
never selected. `zhao_dc_sdp_ram.sv` was not used and remains not a drop-in.

Your `check_plan48_addressing.py` reproduces here at RC=0 with output identical to the
supplied JSON, and is committed at `design/v2/inputs/`. Its constraint is recorded for
any future layout change; production already uses the arithmetic form
`int'(ctx) * PLAN + int'(pc)`, so the concatenation hazard is not present today.

## 5. Integrated throughput and latency

From the differential, before and after the repair, **identical**:

* one context: 13 uops in 69 clocks; eight contexts: 104 uops in 190 clocks
* 12 programs × 8 contexts under a refusing write port: 2,311 clocks refused
* `desync_o` low throughout, `unsupported_o` low, 0 wrong results

For a behaviour-preserving change unchanged numbers are the prediction rather than a
suspicious silence — and the verilated model was confirmed rebuilt after the edit
rather than assumed, because a stale binary produces this exact signature for the
opposite reason.

Engine-level regression, rebuilt and re-run after the edit: 10 of 10 field tests pass,
including `field_v3_full_directed` and `field_v3_earth_directed`.

## 6. Timing status

**None.** Map-only establishes no setup/hold and no Fmax. An array moving from
flip-flops into an M10K changes the paths around it; a 384-way read mux is not a fast
structure, so it could plausibly help, but that is a hypothesis and this campaign has
already been wrong three times about hypotheses. Timing belongs at the integration
checkpoint, as you say.

## 7. Local V1 repair or adopted V2 component

**A local V1 repair, banked.** It is in the shipped executor, it changes no interface
and no contract, and it needs no V2 to be useful. It is not evidence for any V2
architecture, and I am claiming no console saving from it: V1 measured 293,886
combinational ALUTs against 83,820 available, and one array does not change the order
of that problem.

## 8. Is the cause elsewhere? Measured: not cleanly

`tools/budget/nested_read_candidates.py` cross-references the committed
uninferred-array ranking against the RTL for the same shape:

| threshold | candidates | that already carry another named cause |
|---|---:|---:|
| ≥ 4,096 bits | 4 | **4** |
| ≥ 512 bits | 19 | **19** |

Every one is also multidimensional, or has two distinct write addresses, or is read
combinationally. **No array in the tree has un-nesting as its plausible binding
repair.** An earlier throwaway version of that triage reported twenty-odd candidates
including three larger than the store; it counted `begin`/`end` depth, which includes
the process's own `begin` and the reset `else begin`. The committed tool counts
conditions, excludes the reset by name, and its self-test caught the error on the
store's own before/after shapes.

## 9. Three of my predictions were refuted getting here

Recorded because the pattern was identical each time — a real correlation promoted to
a mechanism before it was tested.

1. *"The combinational read address is the blocker."* `@R1` inferred with the address
   untouched. I had read a sufficient condition as a necessary one.
2. *"The six probe styles eliminated five causes."* They eliminated nothing. A
   synthetic probe that INFERS shows only that some arrangement works; STYLE=3 gave a
   false elimination of the very hypothesis `@R3a` later had to test. Your §3's
   sentence about the five passing probes was the correct reading and mine was not.
3. *"Statement order is the cause."* `@R3c` came back byte-identical to the baseline
   — 24,962 / 10,287 / 17,100 / 25,344, not one ALUT different. That negative is what
   made the nesting unambiguous, so the wrong prediction was the useful experiment.

## 10. Also done in parallel: joint-envelope extraction

`design/v2/envelope/EXTRACTION.md`, per your §5 and R4 §8. Extraction with
authorities, three combined profiles stated as consequences, four genuinely
unresolved choices for the owner, nothing reduced.

Its first finding is that the block budgets **cannot be summed as written**:
`tools/budget/envelope_extract.py` over all 140 contracts finds 13 pricing themselves
against 1,666,666 clocks/frame, 9 against 1,333,333, and 6 stating no frame length;
of 18 percent-of-frame claims only 4 name their denominator. The ratified figure is
`computeClocksPerFrame: 1666666` in `design/budgets/workloads.yml`. The nine at
1,333,333 are *pessimistic* against it, so this is a tidiness defect rather than a
hidden overrun — but it has to be fixed before an addition means anything.

Second finding, and the one that guards against a wrong sum: **per-block occupancy is
not additive.** Four ledger rows are at or near 100% of a frame on their own, which is
correct for four separate pipelines. What is additive is area, shared-arbiter
grant-clocks, and the clock as a MINIMUM rather than a sum.

The highest-leverage ambiguity for the owner is whether the 120,000-vertex demand is
per **frame** or per **active view**, which decides whether Duo doubles geometry and
whether the shipped `MUL_LANES=6` skinning arm is adequate or 44% short. Nothing in
the tree states it.

## Paths and verification

* production repair — `fpga/rtl/field/zhao_field_v3_exec.sv`
* evidence — `design/v2/evidence/store_repair.md`
* tests — `tests/differential/field_v3_exec_directed.cpp`, `tests/CMakeLists.txt`
* tools — `tools/budget/nested_read_candidates.py`, `tools/budget/envelope_extract.py`,
  `tools/budget/store_variant_receipt.py`
* envelope — `design/v2/envelope/EXTRACTION.md`
* ledger rows — `reports/synthesis/zhao_block_map.json`:
  `zhao_field_v3_exec@prodparams`, `@repaired`, `zhao_probe_execstore@R1/@R3a/@R3c/@R3d`

Non-force pushed to `design/zhaozhou-v2-rfc`. The diagnostic probe is deleted: it was
a 1,400-line copy of production with two lines changed, and a copy kept past its
question is a drift hazard rather than evidence.
