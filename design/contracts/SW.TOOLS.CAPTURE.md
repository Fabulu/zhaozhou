# Contract — SW.TOOLS.CAPTURE (Capture tooling)

> Ledger: `design/blocks.yml` · owner ZH-080 · phase 1 · maturity UNIT_VERIFIED (W6; evidence pinned in the ledger)

## Purpose and exclusions

Own the .zcap container spec plus reader/writer and round-trip tests (tools/capture + tools/inspect); replay captures through ZEmu.

Exclusions: the frame-packet format itself and the C++/.TS readers/writers are owned by spec/capture_format.md §4 + SW.ZREF (`reference/src/zref_frame.cpp`); this block owns the CLI tooling and the round-trip guarantees.

## Input and output packet layouts

`.zcap` per capture_format.md §4: 32-byte header (magic 'ZCAP', format_version 1, u64 lengths from day one — plan P5), 32-byte section entries (types 0x0001–0x000A), per-section CRC-32C, header CRC backpatched last; unknown section types are skipped forward-compatibly (tested).

## Backpressure rules

Backpressure: `none`.

## Memory ownership

The CLI streams via the SW.ZREF writer/reader classes (seekable file, backpatch at close); no format logic is duplicated in the tool.

## Q formats and rounding

N/A (container layer).

## Latency (fixed or variable)

Latency: `batch`.

## Target throughput

Target throughput: n/a (tool).

## Overflow and malformed-input behaviour

Reader validates magic/flags/header-CRC/table bounds/total-length before any body access; section bodies are CRC-verified on fetch. Bad input returns a typed error (kBadMagic/kBadFlags/kBadHeaderCrc/kBadTable/kIo), never a partial read.

## Directed tests

`tests/unit/test_zcap_roundtrip.cpp` (write→read→resolve incl. skip-unknown-section and source-ID/program-hash survival) and the CTest CLI smoke `capture_cli_verify` (`zhao-capture verify` over the committed golden `tests/abi/golden/zcap_minimal.zcap`).

## Randomized differential tests

The .zcap corpus rides the ABI fuzz corpus lanes (abi_corpus.zcorpus tri-language parity); round-trips are exercised against freshly built frames in test_empty_frame_replay case 4.

## Integration capture cases

`tests/abi/golden/zcap_minimal.zcap` (committed, verifies green), `captures/golden/field/` + `captures/failures/field/` (the §29-17 discipline artifacts).

## Notes

Contract filled (Phase-1-active). Registered as its OWN block per plan 1.D (P2's +1 over 83).
