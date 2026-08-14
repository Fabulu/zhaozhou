# Contract — SW.ZREF (ZRef scalar oracle library)

> Ledger: `design/blocks.yml` · owner ZH-015 · phase 1 · maturity REFERENCE_COMPLETE (W6; evidence pinned in the ledger)

## Purpose and exclusions

The scalar reference oracle hosting every block's reference_model symbol (frame, fixed-point, field interpreter); the differential anchor for all RTL.

Exclusions: ZRef is not a renderer, not an emulator product (that is SW.ZEMU — ZRef has no main), and not a tool CLI. ZRef FUNCTIONS are not blocks — they are values of rtl blocks' reference_model (P2 §1).

## Input and output packet layouts

Inputs: sealed frame packets (spec/capture_format.md §3; layout constants from the GENERATED runtime/include/zhao_abi.h — never re-derived), `.zcap` capture files (§4), Field IR `.zprog`/`.zvec` binaries (spec/form/field-ir.md §7–8). Outputs: validation/execution verdicts `{status, completion_flags, counters}` (§3.2–3.3), golden vector binaries, `.zcap` files.

## Backpressure rules

Backpressure: `none`.

## Memory ownership

The caller owns every buffer; the library allocates only return values. `ZhaoZrefShell` (the W6 session layer over the single W4 per-frame executor) owns only its cumulative counters — one shell per consumer, mirroring the one-instance RTL stub.

## Q formats and rounding

This library IS the implementation of spec/qformats.md — fx16/fx24/unit8/angle16 strong types, the single-rounding `rescale()` law (A3b), saturating arithmetic with SatLedger counters, `rcp_u24`/`field_rcp` tables, 257-entry sin/cos. Headers cite qformats.md sections by number; where any other text disagrees, qformats.md wins.

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: n/a (oracle).

## Overflow and malformed-input behaviour

Frame packets are validated in the fail-safe order of capture_format.md §3.2 (magic → bounds → header CRC → payload CRC → record walk → debug-flag rule); a malformed record stops the packet with a safe error code and zero side effects. `bytes_consumed` = 36 on header-level abort, else the whole `40 + N` packet (§3.2; conformance-pinned by test_empty_frame_replay and test_zref_shell since W6). Fixed-point overflow saturates and counts in the SatLedger; `rcp(0) = 0x7FFF_FFFF` with sticky RCP0.

## Directed tests

- `tests/unit/test_fixp.cpp` — exhaustive sin/cos over all 2^16 angle16, unit8 2^16 pairs, boundary corpus, rational-oracle property sweeps (fast + nightly `fixp_rcp_full`).
- `tests/unit/test_zref_shell.cpp` — golden frame replay + corrupt-variant error codes + session counters (W6).
- `tests/unit/test_tables_tri.cpp` — C++ == SV `.mem` == TS table byte-identity.
- `tests/unit/test_abi_golden.cpp`, `tests/unit/test_zcap_roundtrip.cpp`, `tests/unit/test_crc.cpp`.

## Randomized differential tests

- `tests/differential/test_empty_frame_replay.cpp` — ZRef vs Verilated stub: status/completion/counter parity incl. corrupt cases (bytes_consumed parity pinned in W6).
- `tests/differential/test_field_crater_ring.cpp` — Field IR oracle: golden `.zvec`, replay determinism, minimize demo.
- `tests/fuzz/test_abi_fuzz_parity.cpp`, `tests/fuzz/test_field_fuzz_parity.cpp` (nightly).

## Integration capture cases

`tests/abi/golden/frame_minimal.bin` (replays green through the shell), `tests/golden/fixp/*`, `captures/golden/field/crater_ring.zvec`, committed failing vector `captures/failures/field/fail-484add8d-0x5A17.zvec` (§29-17).

## Notes

Contract filled (Phase-1-active). W6 ownership ruling: `tools/fixgen` (the tri-language table generator, spec/qformats.md §11) has NO own ledger block — its outputs are ZRef's generated tables (`reference/include/zref/generated/zref_tables.hpp`, `fpga/rtl/generated/tables/*.mem`, `compiler/src/generated/tables.ts`) and its golden vectors, so fixgen is owned by THIS contract and its evidence (byte-identity gate + 14/14 suite) counts under SW.ZREF. Registering a separate SW.TOOLS.FIXGEN block later is a ledger edit, not a code change.
