# SuperStation Specs V2 attempt 2 — build-ID QSF mutation

**Status:** `failed:qsf-preflow-mutation`; no compiler stage ran and no load is authorised.

## Provenance

- accepted clean source: `8c91b784ed1f2a3079b7d7802273203be336af5c`
- profile/project/target: `Specs` / `ZhaozhouSpecs` / `5CSEBA6U23I7`
- build entry: `tools/board/build_superstation_specs.ps1`
- guarded stage helper: `tools/board/run_superstation_quartus.py`
- isolated workspace: `build-board-superstation-specs-v2-attempt2-8c91b784`
- source-manifest canonical digest: `9ff77ce42eef6a633af3e048863a8f0011d947a363442f52e40a056bc1f9b017`
- patched `sys_top.v`: `24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9`

## Preflight

Source verification passed. The isolated Verilator test passed 16 selected
vectors with signature `e5f1c57f`; the committed detector mutant fired code 1.
The source manifest was captured successfully.

## Exact failure

The helper invoked the pinned upstream build-ID pre-flow through `quartus_sh`.
That script opens and closes the project to read device/output assignments.
Quartus 17 Lite returned RC 0, generated `build_id.v`, `jtag.cdf` and a database
info file, but rewrote the manifest-bound copied QSF before `quartus_map`:

| QSF state | Bytes | SHA-256 |
|---|---:|---|
| Before build-ID | 3,184 | `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` |
| After build-ID | 3,323 | `c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b` |

The mutation changed the edition string and appended
`RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND`. The new byte guard fired immediately
and the build entry returned RC 1.

## Stage return codes

| Stage | RC |
|---|---:|
| Build-ID `quartus_sh` | 0 |
| QSF identity guard | 1 |
| `quartus_map` | not run |
| `quartus_fit` | not run |
| `quartus_asm` | not run |
| `quartus_sta` | not run |
| Build entry | 1 |

No output reports, complete manifest, resource/timing measurement, SOF or RBF
exist for attempt 2. The useful Quartus measurement remains attempt 1 and remains
quarantined by its failed manifest gate.

## Physical boundary

No RBF staging/load, SSH mutation, watchdog/rollback, JTAG use, flash or
persistent boot action, pin drive, or board/SD mutation occurred. The generated
`jtag.cdf` was only a local file and was not used. Physical HOLD remains.
