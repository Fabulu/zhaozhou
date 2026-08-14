# Contract — SW.TOOLS.ABIDOC (ABI generator)

> Ledger: `design/blocks.yml` · owner ZH-013 · phase 1 · maturity UNIT_VERIFIED (W6; evidence pinned in the ledger)

## Purpose and exclusions

spec/*.zidl → byte-identical C++/TS/SV ABI packages + docs + fuzz corpus (W4's tool).

Exclusions: no command SEMANTICS (generated layouts only — execution lives in the decoder/ZRef shell), no capture container (SW.TOOLS.CAPTURE), no CRC invention (CRC-32C constants per plan A3d, emitted identically in all three languages).

## Input and output packet layouts

In: `spec/commands.zidl` (P5 §1.2 grammar; implemented/reserved status keyword is mandatory — plan 1.E). Out: `runtime/include/zhao_abi.h`, `compiler/src/generated/abi.ts` (+ frame/zcap mirrors), `fpga/rtl/generated/zhao_abi_pkg.sv` (reverse-field-order packed structs — R1 hazard, guarded by goldens), `spec/generated/abi.md`, `tests/fuzz/abi_corpus_gen.ts`, golden binaries `tests/abi/golden/*`.

## Backpressure rules

Backpressure: `none`.

## Memory ownership

Zero external parser deps; pure TS functions. Regeneration is idempotent: `npm run abi:check` = regenerate + byte-diff vs committed (staleness fails CI, plan R11; also the CTest `fast` shim `abi_staleness`).

## Q formats and rounding

fx16 = 4-byte Q16.16 container, fx32 = 8-byte Q32.32 (capture_format.md §1.1 rule 6) — the generator encodes container widths, qformats.md owns the arithmetic.

## Latency (fixed or variable)

Latency: `batch`.

## Target throughput

Target throughput: n/a (tool).

## Overflow and malformed-input behaviour

The generator hard-errors on its own inputs: implicit padding, record size not a multiple of command_alignment, opcodes outside ratified ranges, duplicate opcodes, missing status keyword, bits-container overflow. The emitted validators (oracle.ts / frame.ts) implement the §3.2 fail-safe order and agree with C++/SV on every error code over the fuzz corpus (tri-language parity).

## Directed tests

`tools/abi-gen/test/abi_gen.test.ts` — 17/17 via `npm run -w tools/abi-gen test`: grammar accept/reject (incl. the layout-law mutation tests), CRC-32C test vectors + residue, determinism, computed record sizes vs the ratified table.

## Randomized differential tests

`tests/abi/golden/abi_corpus.zcorpus` replayed through C++ and the Verilated SV probe (`tests/fuzz/test_abi_fuzz_parity.cpp`, `abi_fuzz_parity`) with the TS oracle asserting the same corpus in the compiler workspace — the byte-identity matrix across C++/TS/SV (gate (e)).

## Integration capture cases

`tests/abi/golden/frame_minimal.bin` and `zcap_minimal.zcap` regenerate byte-identically every run; `cmd_*.bin` per-command goldens.

## Notes

Contract filled (Phase-1-active). W6 defect correction: mutation-test source loader now normalizes CRLF so the suite is green on autocrlf checkouts (17/17).
