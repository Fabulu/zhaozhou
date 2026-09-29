# Feedback after 335eae36 — turn the confirmed store into a measured repair

**Date:** 29 September 2026.  
**Reviewed branch:** `design/zhaozhou-v2-rfc`.  
**Reviewed commit:** `335eae36f4e8ed361397376ddd56aef40f2ac9ef`.  
**Status:** focused review and experimental advice, not a new architecture ruling. No repository modification, Quartus execution, or production RTL simulation was performed by the external reviewer.

## 1. Accept the progress; do not restart the forecast debate

The production-depth correction is important and well supported. The source trace is `.PROGS(8)` / `.INSTR_N(48)` into the executor's context/program parameters: 384 entries of a 60-bit uop, or 23,040 logical payload bits.

The recorded standalone pair is:

| Configuration | Registers | Combinational ALUTs | Estimated ALMs | Memory bits |
|---|---:|---:|---:|---:|
| Defaults / PLAN=32 | 17,274 | 7,553 | 11,946 | 25,344 |
| PLAN=48 | 24,962 | 10,287 | 17,100 | 25,344 |

The added 7,688 registers closely track the 7,680 additional payload bits, with memory unchanged. Alongside the named Quartus UNINFERRED diagnostic and parameter trace, this is strong evidence for the diagnosis. The aggregate delta alone is not a proof of the identity of every net; there is no need to overstate it.

I also used the default PLAN=32 in earlier feedback; 23,040 supersedes my earlier 15,360 figure for production. The store represents approximately 92% of the standalone executor's reported registers, not 92% of the whole Field system or the whole console, and not 92% of its ALMs. No conversion saving has been measured yet.

The inference receipts and new scanner tests are useful. They have now done enough to support the next bounded experiment. Do not commission another general scanner rewrite before testing the real repair.

## 2. New practical hazard: PLAN=48 is not a power of two

The existing probe retains five-bit `pc_i` / `wpc_i` ports and several styles use `{ctx_i, pc_i}`. It remains a PLAN=32 flow control, as its corrected header now says. Merely overriding its PLAN parameter does not turn it into a faithful PLAN=48 probe.

For a tightly packed production store:

```
logical_index = context * 48 + pc
context: 0..7
pc:      0..47
index:   0..383, requiring 9 bits
```

A six-bit PC concatenated with the context instead gives `context * 64 + pc`. It is not the same indexing expression. For `(context=1, pc=0)`, the two addresses are 48 and 64. Inserting the concatenation into an unchanged 384-word array sends 96 legal context/PC pairs outside the declared array.

An exact narrow-address candidate can retain `context * PLAN + pc`, with sufficient expression width. For the fixed 48 case, `(wide_context << 5) + (wide_context << 4) + wide_pc` is equivalent. In SystemVerilog, widen the operands BEFORE shifting; shifting a three-bit expression does not create the required width by itself.

An explicitly different internal representation is also legitimate: allocate 512 physical words, use `context * 64 + pc` at BOTH upload and fetch, and preserve the logical maximum of 48 instructions per context. The unused PC encodings 48..63 must not silently become legal or another context's program. Preserve existing validator/host obligations and explicitly adjudicate any malformed-input discrepancy; do not inadvertently preserve an unsafe alias as a new design feature.

Neither layout is a diagnosis of the inference failure. Production already fails at PLAN=32, so non-power-of-two depth cannot be the sole explanation for both failures. This is a correctness constraint on the next repair.

`check_plan48_addressing.py` in this package exhaustively checks the 384 legal address pairs, validates explicit packed and padded layouts, and catches blind concatenation and five-bit-PC negative controls. It tests arithmetic, not the RTL or its synthesis.

## 3. Next experiment: a real executor with a different store boundary

Retain a failing baseline at the actual production configuration. Record the complete resolved parameter tuple, not just an override string. Preserve the production consumers, independently variable uop fields, and separately addressed upload/fetch ports.

Try extracting only the store and its registered read into a clean same-clock memory boundary. The existing assignment to `s1_uop_r` is already clocked. Try to have the memory implement that boundary rather than accidentally adding another stage in front of it.

Preserve:

- effective write enable, including reset gating;
- effective read enable and holding behavior during pipeline stalls;
- instruction/context/valid alignment;
- all eight contexts and all 48 legal instruction positions;
- every independently variable uop bit;
- permitted read-during-write behavior, or the actual proof that collisions are excluded;
- reset/initialization validity rather than inventing observable initialized data.

The existing dual-clock RAM wrapper explicitly excludes same-address read/write collisions. It is not automatically a drop-in. A same-clock implementation with the required old-data semantics is a different contract. Do not select `don't care` without an established caller guarantee.

Reduce the failing production module in a bounded batch when needed, retaining data observability. A disappearing store is not inferred memory. A small passing/failing pair is useful only when the successful transformation also works back in the real executor.

If implicit inference remains obstructed, an explicit M10K implementation behind the same tested interface is a legitimate comparison candidate. The project needs a correct compact implementation, not an indefinite attempt to persuade a particular inference heuristic. Its behavioral model and target implementation must be compared under the same timing and collision contract.

## 4. Acceptance evidence — focused, not a new instrumentation campaign

Publish the BEFORE and AFTER at PLAN=48 with all other relevant parameters held fixed. Include the store's named mapping, capacity, independently observable data, physical memory configuration, and total resource changes. Map-only ALMs remain estimates; no timing conclusion comes from that stage.

The diagnostic receipt is an improvement but is not yet a freshness/identity proof. In the reviewed tool:

- source hashing is optional and reads current files rather than an immutable pre-run snapshot;
- the ledger row is selected independently of the report;
- attribution keeps the local array name, not a complete unique hierarchy binding.

For this experiment, attach an immutable run manifest and the exact target instance/report association. Reject an inferred match coming from a different `store` instance or from a prefix-neighbor such as `store_aux`. Do not build a general framework to solve this; make this pair auditable.

The tests need programs reaching indices 31, 32 and 47 in multiple contexts, not only short programs that happen to fit the old depth. Include upload/fetch address independence, consecutive fetches, stalls and releases, reset, and the permitted collision behavior. If a direct store test uses arbitrary data, distinguish its storage proof from a legal-program semantic execution test.

Then run the real executor differential and a representative integrated Field workload. Report actual cycles, stalls, result identity and status. Run fitting/timing at the agreed integration checkpoint, rather than call inference success a fitted engine.

## 5. Preserve the bigger objective

The first successful store repair is a valuable calibration result. It does not require a whole-console forecast. Nor should its success be extrapolated to unrelated blocks.

Once measured, use that result in the coherent Field-slice replacement: program ownership, context admission, operands, short/long services, export/status/presence, command-ordered reduction and publication. The full-capability V2 question remains open until replacement configurations demonstrate semantic equivalence, useful throughput and physical feasibility together.

**Requested next push:** either a production-shaped store replacement with matched before/after and differential evidence, or a minimized still-failing production case plus the next explicit-memory candidate. Preserve V1. Commit and non-force push only the intended experiment files on the isolated design branch, verify the remote SHA, and return paths and evidence. No new owner decision is needed for this bounded work under the existing authorization.

## Source index

All repository references below were read at `335eae36f4e8ed361397376ddd56aef40f2ac9ef`:

- Commit `335eae36`: paired map results and receipt tool.
- Commit `45b880cc`: parameter trace and corrected probe/evidence documentation.
- `design/v2/evidence/uop_store_probe.md`: paired table and scope caveats.
- `fpga/rtl/field/zhao_field_v3_exec.sv`: store upload and registered instruction fetch, especially source lines 1030–1215.
- `fpga/rtl/synth/zhao_probe_uopstore.sv`: fixed five-bit PC interfaces and concatenation-based probe styles.
- `tools/budget/store_variant_receipt.py`: name attribution, ledger lookup and optional live-source digest.

Vendor reference: Altera, Cyclone V Device Handbook, Embedded Memory Features. M10Ks are synchronous and support mixed-port old-data or don't-care read-during-write; an actual configured mapping and cycle-level check remain required:
https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/embedded-memory-features

This feedback does not claim a synthesis repair, measured memory saving, changed clock, or a fitted V2.
