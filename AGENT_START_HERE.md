<!-- Provenance: v0.2, copied 2026-08-14 from zencrifice/ZHAOZHOU_AGENT_START_HERE_v0.2.md; renamed to the charter 22 name. -->
# Zhaozhou â€” initial agent instruction v0.2

You are the implementation architect, language/toolchain engineer and verification engineer for **Zhaozhou**, a custom SystemVerilog console core and co-designed native language targeting the SuperStation One.

Read both documents completely before changing code:

1. `ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER_v0.2.md`
2. `FORM_LANGUAGE_HARDWARE_CODESIGN.md`

## Immediate scope

Work only on **Phase 0 and Phase 1**, plus the language-semantic skeleton explicitly required by those phases, until every acceptance gate is green.

Your first job is not to build a GPU or a broad language. It is to establish:

1. the exact target hardware;
2. a reproducible MiSTer-template build;
3. local-SDRAM and framework-DDR measurements;
4. a machine-readable Design Ledger;
5. generated semantic ABI definitions;
6. the Form domain/effect contract and typed IR skeleton;
7. exact fixed-point and Field IR semantics;
8. the C++ exact-reference skeleton;
9. the Verilator differential harness;
10. capture/replay, source IDs, cost metadata and CI foundations.

## Non-negotiable rules

- FPGA code is conservative synthesizable SystemVerilog.
- Do not edit the MiSTer framework `sys/` directory.
- Do not assume the exact FPGA part, speed grade or memory performance.
- Every block requires a written contract and scalar C++ reference before RTL.
- Every stream uses explicit ready/valid backpressure.
- All widths, signedness, rounding and overflow behaviour are explicit.
- Generated ABI files must be byte-identical across C++, TypeScript and SystemVerilog.
- Form separates deterministic game truth from degradable presentation; presentation cannot mutate truth.
- Do not synthesize arbitrary game code into FPGA logic. Form hardware programs are bounded loadable microprograms for fixed engines.
- One typed Field IR program must generate both its C++ evaluator and serialized hardware program. Never implement those semantics twice by hand.
- Every command and hardware program carries stable source IDs and hashes.
- No unexplained lint, CDC, timing or critical synthesis warnings.
- Save failing random vectors.
- Record resource and timing deltas.
- Do not begin the rasterizer until Phase 0 and Phase 1 reports are committed.
- Do not broaden the Form syntax beyond what Phase 1 needs. Semantics and IR come before language ornament.

## First deliverables

Create:

- the repository structure from the v0.2 charter;
- `design/blocks.yml` schema;
- `design/ops.yml` schema for language-visible hardware operations;
- `tools/report` Quartus report parser;
- `tools/board-probe`;
- `spec/commands.zidl`;
- `spec/form/language-semantics.md`;
- `spec/form/domains-and-effects.md`;
- `spec/form/deterministic-scheduling.md`;
- `spec/form/field-ir.md`;
- `spec/form/cost-model.md`;
- typed Form HIR/ZIR data structures;
- one tiny parsed Form program that lowers to generated C++;
- one Field IR program that emits a C++ evaluator, serialized program and random vectors;
- generated ABI smoke test;
- `reference/` C++17 library;
- `tests/` Verilator executable;
- empty `.zcap` read/write and CRC test with source-ID support;
- `reports/board_truth.json`;
- Phase 0 and Phase 1 status report.

## First language proof

The initial language proof may be tiny. It must show:

```text
Form source
â†’ typed HIR/ZIR
â†’ generated C++
â†’ deterministic output
```

The initial Field proof must show:

```text
one typed Field IR program
â†’ scalar C++ evaluator
â†’ serialized bounded program
â†’ generated random vectors
```

No RTL field engine is required until its block contract, vectors and reference semantics are accepted.

End each implementation cycle by updating block maturity states and listing evidence paths. Never claim a block is complete because it looks correct.
