# SuperStation Specs V2 attempt-3 evidence index

**Status:** `failed:artifact-set`; preserved and stopped without repeat or bypass.

## Bound source

- accepted commit: `5666bce4985b9cfa9c5e0139b540364893b08450`
- profile/project/device: `Specs` / `ZhaozhouSpecs` / `5CSEBA6U23I7`
- build directory: `build-board-superstation-specs-v2-attempt3-5666bce4`
- source-manifest digest: `c2e61fdf3b7b87e5b04cabe60fdaa0b118257ef5d62a6ea5f0d64db66c7a1eb1`
- QSF before/final: 3,184 bytes / `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5`

## Preserved evidence

- `HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT3-MISSING-DONE.json`
- `HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT3-MISSING-DONE.json`
- `HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT3-MISSING-DONE.json`
- `SPECS-V2-ATTEMPT3-MISSING-DONE-COMPILE-TRANSCRIPT.txt`
- `SPECS-V2-ATTEMPT3-MISSING-DONE-VERIFY.json`
- `SPECS-V2-ATTEMPT3-MISSING-DONE-BUILD-REPORT.md`
- `SPECS-V2-ATTEMPT3-MISSING-DONE-RAW/`

All compiler stages and QSF guards passed; repaired post-build verification also
passed. The direct stage sequence omitted required `ZhaozhouSpecs.done`, so the
exact artifact-set gate refused complete-manifest creation. No
`HARDWARE-SPECS-BUILD-MANIFEST-V2.json` exists and the measured RBF is
quarantined.

No RBF staging/load, SSH mutation, JTAG, flash, persistence, pin drive,
watchdog/rollback or board/SD mutation occurred. Further compile and physical
activity remain HOLD.
