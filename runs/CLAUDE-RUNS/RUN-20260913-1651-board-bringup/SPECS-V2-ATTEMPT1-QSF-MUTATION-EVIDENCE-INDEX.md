# SuperStation Specs V2 compile evidence index

**Status:** `failed:manifest`; Quartus measurement completed, but the overall evidence gate failed and no load is authorised.

## Immutable source

- profile: `Specs`
- project: `ZhaozhouSpecs`
- accepted source commit: `0ba2eefcd2938cd6a50a1853380ab4d8c9b8a629`
- target device: `5CSEBA6U23I7`
- build entry point: `tools/board/build_superstation_specs.ps1`
- isolated workspace: `build-board-superstation-specs-v2-0ba2eefc`

## Preserved attempt-1 evidence

- `HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT1-QSF-MUTATION.json` — source-phase manifest copied from the build workspace.
- `HARDWARE-SPECS-BUILD-MANIFEST-V2.json` — intentionally absent: complete-manifest creation refused the post-Quartus QSF mutation.
- `HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT1-QSF-MUTATION.json` — final `failed:manifest` audit tied to the source commit and measured RBF record; it does not authorize loading.
- `HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT1-QSF-MUTATION.json` — compile command, timing, return status and evidence paths.
- `SPECS-V2-ATTEMPT1-QSF-MUTATION-COMPILE-TRANSCRIPT.txt` — raw committed build-entry output.
- `SPECS-V2-ATTEMPT1-QSF-MUTATION-VERIFY.json` — raw repaired post-build verifier result.
- `SPECS-V2-ATTEMPT1-QSF-MUTATION-QUARTUS-RAW/` — raw Quartus reports, summaries, message files and both QSF states copied without modification.

## Acceptance gates

1. Build entry point returns success without bypass or rerun.
2. Source and complete manifests verify against the accepted source and isolated workspace.
3. Profile/project/device are exactly `Specs` / `ZhaozhouSpecs` / `5CSEBA6U23I7`.
4. Quartus flow is successful with zero errors and zero Critical Warnings in every scanned report format.
5. All USER_IO output enables 0 through 6 are permanently disabled.
6. The expected Zhaozhou block hierarchy is present and `zhao_dual18_mul:u_mul` maps one DSP.
7. Setup, hold, recovery, removal and minimum-pulse-width slacks are all nonnegative; illegal and unconstrained clocks are zero.
8. Local RBF bytes and SHA-256 agree across the actual file, complete manifest and audit.
9. All evidence hashes are recorded before commit and independently rechecked.

## Outcome

Gates 3–7 passed as measurements, but gates 1–2 and 8 did not close. Quartus
rewrote the copied `ZhaozhouSpecs.qsf` after the source manifest captured it,
changing its SHA-256 from
`0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` to
`c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b`.
The complete-manifest creator refused the mismatch. No complete candidate
manifest exists; the measured RBF is quarantined and cannot be loaded.

This compile-only authorization does not permit RBF staging/loading, SSH mutation, watchdog or rollback rehearsal, JTAG, flash, persistence, pin drive, or board/SD mutation. Physical activity remains HOLD.
