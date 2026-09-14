# SuperStation Specs V2 attempt-2 evidence index

**Status:** `failed:qsf-preflow-mutation`; preserved and stopped without repeat or bypass.

## Evidence

- `HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-COMPILE-TRANSCRIPT.txt`
- `SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-BUILD-REPORT.md`
- `SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-RAW/`
  - exact source and post-build-ID QSF copies;
  - generated `build_id.v` and unused `jtag.cdf`;
  - `ZhaozhouSpecs.db_info` created during project open/close.

No `HARDWARE-SPECS-BUILD-MANIFEST-V2.json` exists. The byte guard fired before
`quartus_map`, so there are no attempt-2 reports, resources, timing, SOF or RBF.
The compile-only authorization is spent. Physical activity and any further
compile remain HOLD pending independent review.
