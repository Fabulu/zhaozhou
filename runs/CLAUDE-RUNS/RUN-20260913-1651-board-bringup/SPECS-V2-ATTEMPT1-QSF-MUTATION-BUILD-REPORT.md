# SuperStation Specs V2 build report

**Status:** `failed:manifest`. Quartus measurement completed, but the manifest-bound build entry returned failure and the RBF is quarantined.

## Provenance

| Field | Value |
|---|---|
| Accepted source commit | `0ba2eefcd2938cd6a50a1853380ab4d8c9b8a629` |
| Branch | `zhaozhou-board-bringup-20260913` |
| Profile / project | `Specs` / `ZhaozhouSpecs` |
| Build entry point | `tools/board/build_superstation_specs.ps1` |
| Isolated build directory | `build-board-superstation-specs-v2-0ba2eefc` |
| Target device | `5CSEBA6U23I7` (Cyclone V) |
| Patched `sys_top.v` SHA-256 | `24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9` |
| Source-manifest canonical SHA-256 | `956b1a41132167ad0bda418a2e4ce319d2bbb24f35208eda3c9fcab069a961bd` |
| Complete manifest | absent; creation correctly refused |

## Gate failure

Quartus rewrote the copied manifest-bound `ZhaozhouSpecs.qsf` after source
capture. It changed `LAST_QUARTUS_VERSION` from `17.0.2 Standard Edition` to
`17.0.2 Lite Edition` and appended
`RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND "AS INPUT TRI-STATED"`.

| QSF state | Bytes | SHA-256 |
|---|---:|---|
| Captured/compiled source | 3,184 | `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` |
| Post-Quartus working copy | 3,323 | `c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b` |

The complete-manifest verifier reported:

> source manifest failed before completion: build input hash/size mismatch: ZhaozhouSpecs.qsf

No complete V2 candidate manifest exists. Successful measurements below do not
override that failure and do not authorize loading.

## Quartus measurement

| Measurement | Result |
|---|---|
| Flow status | Successful |
| Errors | 0 |
| Warnings | 55 |
| Critical Warnings | 0 |
| ALMs | 7,312 / 41,910 (17%) |
| Registers | 11,173 |
| Block-memory bits | 384,498 / 5,662,720 (7%) |
| DSP blocks | 34 / 112 (30%) |
| Pins / virtual pins | 145 / 314 (46%) / 0 |
| PLLs | 3 / 6 (50%) |

Warning counts were 40 analysis/synthesis, 11 fitter and 4 TimeQuest. Assembler
and build-ID stages reported zero. The repaired all-format scanner found no
Critical Warning.

## Timing

| Check | Worst slack (ns) |
|---|---:|
| Setup | +0.058 |
| Hold | +0.253 |
| Recovery | +3.658 |
| Removal | +0.858 |
| Minimum pulse width | +1.122 |

Illegal clocks and unconstrained clocks are zero. There remain 4 unconstrained
input ports / 14 input paths and 47 unconstrained output ports / 119 output
paths. External I/O timing therefore remains unsigned.

## Safety and hierarchy evidence

- USER_IO permanently disabled output enables: `[0,1,2,3,4,5,6]`.
- Reserved output-driving pins: 0; reserved input pins: 169.
- `zhao_crc32c_fold:u_crc`: 135 combinational ALUTs, 0 DSP.
- `zhao_raster_fill:u_fill`: 1 combinational ALUT, 0 DSP.
- `zhao_dual18_mul:u_mul`: 0 combinational ALUTs, 1 DSP (`cyclonev_mac`).
- `zhao_ssone_spec_tests:u_spec_tests`: 317 combinational ALUTs, 44 registers, 1 DSP.
- Selected-vector preflight passed 16 vectors with signature `e5f1c57f`; the committed detector mutant fired code 1.

## Primary artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `ZhaozhouSpecs.rbf` | 2,448,816 | `31699ff37440f26c8a979f53ce45b02ac63185038a2cc51a139eaba9ecb491eb` |
| `ZhaozhouSpecs.sof` | 6,690,380 | `71afd3baead66cc5990861247c3edb6f0b1e900acf4cea47ef15dfb8aef69dc6` |
| `ZhaozhouSpecs.flow.rpt` | 14,030 | `940c8084b978094b21068d80e9ac90c423814ea00d1f7d632da0d1bff3f3343e` |
| `ZhaozhouSpecs.map.rpt` | 983,346 | `5435bd81594285c8b4b1e06941f47dd863b13e60659d07c177859848cfdf34b0` |
| `ZhaozhouSpecs.fit.rpt` | 1,799,325 | `24064444856e61c73a63e1b38a42f67911218b1e9aabedde7c7c09e0cc41c31e` |
| `ZhaozhouSpecs.sta.rpt` | 225,901 | `68cba751e20befc490aa77c269bce7aadb86d0f5c5728dbfa2ea27d91e4f4b02` |

All 16 Quartus output records and hashes are retained in
`HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT1-QSF-MUTATION.json`. The 11 textual
reports, summaries and message files plus both QSF states are copied verbatim
into `SPECS-V2-ATTEMPT1-QSF-MUTATION-QUARTUS-RAW/`; programming artifacts
remain quarantined only in the ignored build workspace.

## Physical-action boundary

No RBF staging/loading, SSH mutation, watchdog/rollback rehearsal, JTAG, flash,
persistence, pin drive, or board/SD mutation occurred or is authorized.
Physical activity remains HOLD.
