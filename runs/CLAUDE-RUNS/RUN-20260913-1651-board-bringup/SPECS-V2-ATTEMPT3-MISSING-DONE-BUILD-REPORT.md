# SuperStation Specs V2 attempt-3 build report

**Status:** `failed:artifact-set`. Compilation and QSF isolation passed, but complete-manifest closure failed and the RBF is quarantined.

## Provenance

| Field | Value |
|---|---|
| Accepted source | `5666bce4985b9cfa9c5e0139b540364893b08450` |
| Profile / project | `Specs` / `ZhaozhouSpecs` |
| Target | `5CSEBA6U23I7` (Cyclone V) |
| Build directory | `build-board-superstation-specs-v2-attempt3-5666bce4` |
| Projectless build-ID SHA-256 | `e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6` |
| Patched sys_top SHA-256 | `24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9` |
| Source-manifest canonical SHA-256 | `c2e61fdf3b7b87e5b04cabe60fdaa0b118257ef5d62a6ea5f0d64db66c7a1eb1` |
| Complete manifest | absent; exact artifact-set check refused |

## Stage and QSF result

| Stage | RC | QSF guard |
|---|---:|---|
| Projectless build-ID | 0 | passed |
| Analysis/synthesis | 0 | passed |
| Fitter | 0 | passed |
| Assembler | 0 | passed |
| TimeQuest | 0 | passed |
| Stage helper | 0 | all stage guards passed |
| Post-build verifier | 0 | passed |
| Complete-manifest creation | 1 | missing required artifact |
| Build entry | 1 | failed closed |

QSF remained exactly 3,184 bytes with SHA-256
`0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5`
from source capture through all five stage guards.

## Gate failure

The direct stage sequence produced 15 of the exact 16 required Quartus outputs.
`ZhaozhouSpecs.done`, normally emitted by the `quartus_sh --flow` wrapper, was
absent. Complete-manifest creation reported:

> Quartus output record set mismatch: missing=['output_files/ZhaozhouSpecs.done'] extra=[]

No complete candidate manifest exists. Measurements below do not override the
failed closure and do not authorize loading.

## Quartus measurement

| Measurement | Result |
|---|---|
| Errors | 0 |
| Warnings | 55 total: map 40, fit 11, assembly 0, STA 4 |
| Critical Warnings | 0 |
| ALMs | 7,312 / 41,910 (17%) |
| Registers | 11,173 |
| Block-memory bits | 384,498 / 5,662,720 (7%) |
| DSP blocks | 34 / 112 (30%) |
| Pins / virtual pins | 145 / 314 (46%) / 0 |
| PLLs | 3 / 6 (50%) |

All USER_IO output enables 0–6 are permanently disabled; reserved output pins
are zero. The expected Zhaozhou hierarchy remains present, with one packed
`cyclonev_mac` DSP in `zhao_dual18_mul:u_mul`.

## Timing

| Check | Worst slack (ns) |
|---|---:|
| Setup | +0.058 |
| Hold | +0.253 |
| Recovery | +3.658 |
| Removal | +0.858 |
| Minimum pulse width | +1.122 |

Illegal and unconstrained clocks are zero. External I/O timing remains unsigned:
4 input ports / 14 input paths and 47 output ports / 119 output paths remain
unconstrained.

## Primary artifact identities

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `ZhaozhouSpecs.rbf` | 2,448,816 | `31699ff37440f26c8a979f53ce45b02ac63185038a2cc51a139eaba9ecb491eb` |
| `ZhaozhouSpecs.sof` | 6,690,380 | `02cebdea6f2bdf552845baba64c1e3004a9f4d3b5d69f39e3548a9723572b6b9` |
| `ZhaozhouSpecs.flow.rpt` | 14,028 | `5f9781ea73172a550c5358161c11c5b9814b36a12d5ffdb358748d193765472b` |
| `ZhaozhouSpecs.map.rpt` | 986,896 | `5e6b392143fc036c55abef153559c72d1215ea02a42e9dd699a4ecc1213a1e3a` |
| `ZhaozhouSpecs.fit.rpt` | 1,799,604 | `31f55b0609da73eda11835727628bf719b79e200ea66fa3aa604e5640aee053f` |
| `ZhaozhouSpecs.sta.rpt` | 226,959 | `baa5f955045aeef68ecdd98e7a3cfe2641874a01bb0438a6e668d33e349e6bcf` |

The failed audit records all 15 actual output hashes. Eleven raw textual reports,
both QSF states and build-ID files are preserved; programming artifacts remain
only in the ignored quarantined workspace.

## Physical boundary

No RBF staging/load, SSH mutation, JTAG, flash, persistence, pin drive,
watchdog/rollback or board/SD mutation occurred. Physical HOLD remains.
