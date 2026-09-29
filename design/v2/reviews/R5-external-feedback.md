# Zhaozhou V2 — focused feedback after R5

**Reviewed branch:** `design/zhaozhou-v2-rfc`  
**Reviewed commit:** `75df83dd540e8268f54566b7e01a2f34d9631399`  
**Review date:** 29 September 2026  
**Status:** external review and experiment suggestions, not a ratified new architecture, implementation result or feasibility forecast.

## Verdict

Continue the production-down experiment. The current result is useful: a named 256-by-60-bit instruction store fails RAM inference in the real executor, including its standalone synthesis, while the simplified probes infer. This is an actionable local problem inside the agreed Field replacement boundary. It is not a demonstrated whole-console saving, and the absence of a successful conversion is not evidence against all V2 designs.

R5's withdrawal of the unsupported storage percentages and 51 MHz universal floor is accepted. Keep the complete workload/deadline/reserve relationship and the established 100 MHz design point. The historical approximately 80 MHz assessment belongs to its stated cost model; do not relabel it an architecture-independent lower bound for a redesigned engine. No new whole-machine prediction is requested.

## 1. Separate four deliverables

1. Reproduce and explain the production inference failure.
2. Produce an equivalent implementation of the store with a measured resource delta.
3. Integrate that implementation into the relevant Field execution configuration and measure throughput and timing.
4. Test the broader Field cluster replacement, including preparation, input generation, admission, operands, short/long operations, status/presence, reduction and publication.

Completing item 2 does not complete item 4. Conversely, a stubborn inference issue in V1 should not indefinitely block a properly specified V2 memory implementation.

## 2. Preserve the existing register boundary first

In `fpga/rtl/field/zhao_field_v3_exec.sv`, the production read already assigns:

```systemverilog
s1_uop_r <= store[(int'(issue_ctx_c) * PLAN) + int'(pc_r[issue_ctx_c])];
```

It is inside a clocked process. Therefore a synchronous-RAM implementation does not inherently require an additional architectural cycle. It might require one because of its actual ports, enables, collision semantics or timing, but that is an experimental result, not a premise.

A useful first production-preserving perturbation is to isolate ONLY the store write and read-data register assignments into a dedicated same-clock process or small memory module, leaving all surrounding consumers and control present. Preserve the exact original effective write/read enables, including reset gating, and keep `s1_ctx_r` / `s1_v_r` aligned with the data. Remove the original assignments rather than introducing multiple drivers. Do not retain an extra output register merely because a wrapper provides one.

This is a candidate, not a claim that extraction will infer or fix the cause. Keep the unchanged production version as the control. A second candidate is an explicitly configured single-clock simple-dual-port memory backend under the same semantic interface, with generic RTL retained as its simulation/reference implementation. An explicit primitive is acceptable FPGA implementation detail; incorrect semantics are not.

### Collision and reset law must be explicit

The source's nonblocking read/write assignments read the old word on a simultaneous same-address read and write. Preserve this on legal collisions, or demonstrate that the existing protocol makes collisions unreachable before choosing a don't-care mode. Do not silently impose a new restriction on legal upload/execution behavior.

The existing `zhao_dc_sdp_ram.sv` explicitly excludes same-address read/write collisions from its protocol. It is therefore not automatically a drop-in replacement, even though both clock pins could be connected to one clock. Either prove compatibility at this call site or use a single-clock backend that implements the necessary law.

Preserve data-valid behavior through reset and stalls. Do not compare or require equality of uninitialized payloads unless they are observably defined by the contract. Do check that invalid payloads cannot become a write, a new instruction or a false terminal status.

## 3. Downward reduction must retain the failure being investigated

For every variant keep a small machine-readable receipt with exact source/dependency digest, parameter overrides, device, tool/settings, observed store dimensions, inferred-memory identity and shape, registers, combinational ALUTs, estimated ALMs, MLAB bits and warnings.

The useful predicate is not simply 'total memory increased'. Require that:

- the run completed rather than reusing an old result;
- the intended store is present and its relevant bits remain observable;
- the failure or successful memory mapping is attributed to that store;
- no unrelated RAM is being counted as the replacement;
- the control and experimental configurations are otherwise identified.

Removing the store's consumers can let synthesis delete the object. Deletion is not successful inference. A diagnostic reduction can deliberately change behavior, but must be labelled diagnostic; only a reintroduced, production-equivalent change is a repair.

After finding a small failing case, reverse the relevant edit and recover the original failure. If possible retain a minimal passing/failing pair differing in the causal property. Do not turn the first attractive explanation into a general coding law.

The five passing probes rule out each tested shape as a sufficient explanation in those probes. They do not rule out interactions among reset behavior, independent ports, enables, actual consumers and synthesis transformations.

## 4. Make the probe faithful where fidelity matters

The committed probe is a useful inference-flow control, but it is not a full production-shaped store:

- Its read and write addresses use the same `ctx_i` / `pc_i`; production upload and fetch addresses are independent.
- `dst`, `a`, `b` and `c` are derived from opcode/address inputs rather than independent data inputs.
- In particular `b: RW'(ctx_i)` zero-extends a 3-bit unsigned input into a 5-bit field, forcing two bits to zero. That accounts numerically for 256 * 2 = 512 bits: the reported 14,848-bit inferred memory is 58 bits per entry, not the production 60-bit payload.
- STYLE 4 resets its read-data register, whereas production does not explicitly reset `s1_uop_r`. That is another distinction to retain when comparing reset hypotheses.

These facts do not invalidate the observation that all probe variants infer. They do prevent calling the probe an exact replacement cost/behavior measurement. For the eventual store acceptance test use independent read/write addresses, independent payload fields and a scoreboard over initialized addresses, with back-to-back fetches, holds, reset, distinct contexts, first/last entries and permitted collisions.

Do not start another open-ended catalogue of synthetic styles. Correct the observation boundary and reduce the failing real implementation.

## 5. Test the new scanner repairs, not only the old rules

The existing three self-tests are valuable regression checks, but their survival alone does not test the newly repaired failure modes. Add narrow tests that fail when:

- a `uop_t store[...]` or an `_e` / nonstandard-name typedef array is missed;
- an unresolved element width is silently reported as bits rather than entries;
- an array of module instances is mistaken for storage;
- module-level memory is labelled proof that a particular array inferred.

Describe the supported syntax, not a complete SystemVerilog parser. A regex scan remains a candidate-finding instrument; inference evidence remains per instantiated object. A stable exit code or row count is not a memory-inference certificate.

## 6. Fix live documentation before its false conclusions get reused

At this reviewed commit, `fpga/rtl/synth/zhao_probe_uopstore.sv` still says the cause was found, attributes it to a combinational read-enable loop, and says STYLE 3 must not infer. R5 records that explanation as refuted. Put a clear correction beside the old claim and change the current description to the actual observation.

Likewise `design/v2/evidence/README.md` still ends by asserting a measured 4–7% storage saving and a contractual 51 MHz floor. Both are withdrawn by R5. Preserve historical records but add a conspicuous current-status pointer and mark these conclusions superseded at their use sites. No new long argumentative document is needed.

## 7. Measure the right configuration and the net result

The committed standalone executor row records 17,274 registers, 7,553 combinational ALUTs, 11,946 estimated ALMs and 25,344 memory bits. These are map-only results. They must not be directly subtracted from the composed executor's 24,795 own registers: parameterization, exposed ports, attribution and optimization context may differ.

Record the production `LANES`, `CTX`, `REGS`, `PLAN` and `LONGQ`, not just module defaults. First obtain a paired standalone comparison under identical settings; then measure the relevant integrated Field slice. Quote estimated ALMs as estimates until placed, and record physical RAM blocks separately from logical memory bits.

For a full 256-by-60, one-write/one-read store, two width banks are a reasonable candidate layout on M10K. This is a layout target to verify, not an ALM-saving prediction. Account for read/write controls and any necessary collision logic.

For the same accepted complete workload and clock convention, report both the cycle ceiling and actual elapsed time with reserve. Do not combine a fast wide configuration's throughput with a smaller narrow configuration's area. Map-only does not establish setup/hold or integrated timing.

## 8. Do not let this become an endless V1 inference project

The strategic experiment remains the coherent Field replacement. Retain the diagnostic work because it gives a reproducible example and a potentially valuable local repair. If a verified explicit memory backend resolves the production problem, bank that result and continue; discovering every internal inference heuristic is not a shipping requirement.

The next architectural question is whether program/context ownership, direct input generation, operand supply and retirement can be organized more compactly at the required service rate. Deduplicating identical programs is legitimate; reducing distinct-program capacity is not. Lowering output frequency or omitting expensive operations is not part of this experiment.

No decision from the owner is needed for these bounded tests. Continue joint-envelope extraction in parallel and bring back concrete ambiguities only when they change a product guarantee.

## Requested next report

Return one concise report containing: the real blocker or smallest retained failure; exact before/after configurations; store mapping evidence; semantic test results; net map delta; integrated latency/throughput; timing status; and whether the result is a local V1 repair or an adopted V2 component. Commit the reproducible artifacts on the design branch and non-force push, returning the verified SHA. Do not modify unrelated work or claim a whole-console rescue from this one store.

## Sources reviewed

All repository paths below are pinned to `75df83dd540e8268f54566b7e01a2f34d9631399` unless noted:

- `design/v2/proposals/R5-agent-candidate.md`
- `design/v2/reviews/R4-agent-review.md`
- Commit message and map-row diff of `75df83dd540e8268f54566b7e01a2f34d9631399`
- `fpga/rtl/field/zhao_field_v3_exec.sv`, especially the clocked upload/fetch process and operand-hold explanation
- `fpga/rtl/synth/zhao_probe_uopstore.sv`
- `fpga/rtl/common/zhao_dc_sdp_ram.sv`
- `tools/quartus/check_ram_inference.py`
- `design/v2/evidence/README.md`
- Altera Quartus Help 18.0, **M10K memory block Definition**: registered inputs, optional output registers, independent read/write enables, and supported widths.
- Altera **Cyclone V Device Handbook, Embedded Memory Features**: synchronous memory and mixed-port old-data/don't-care choices.

**Review limits:** committed-source and recorded-report review only. No Quartus run, no complete RTL/reference differential, no board execution and no repository edits were performed by the external reviewer.
