# Phase 1 / Wave 1 — status report

**Date:** 2026-08-15 · **Scope:** plan W1–W6 (RUN-20260814-1912-wave1-architecture-plan)
**Machine:** Windows 11, oss-cad-suite 20260814 (Verilator 5.051, Yosys/SBY 0.68), mingw64 g++ 16.1.0, Node 20.17.
**Decisions log:** `runs/CLAUDE-RUNS/RUN-20260814-1912-wave1-architecture-plan/PLAN.md` (all ratified rulings A1–A3e, P4-ten, Q1–Q5, P2-six, P5-four, P1-three; this file is the wave's single source for "why").

Charter §23 Phase 1 lists **build** items and **gate** items. The plan
translated them into gates (a)–(f). Both are tabled below — honestly:
what is green with named evidence, and what is PENDING with an owner.

## Gate table — plan (a)–(f)

| Gate | Item | Status | Evidence (named tests + artifacts) |
|---|---|---|---|
| (a) | Build + stub RTL: byte-reproducible cmake/ninja/CTest with a Verilated stub frame validator | **GREEN (local)** | `cmake --preset windows-native && cmake --build build` clean; CTest `stub_top` (valid frame → completion + zero error; corrupt magic → safe error), `lint_stub_top` (verilator `--lint-only -Wall` clean over zhao_abi_pkg + stub + probe). Stub consumes the GENERATED ABI package (placeholder removed in W4). |
| (b) | Design ledger: 87 blocks + 40 ops, validator, generators | **GREEN** | `npm run ledger:check` (schemas + V1–V14 + git-history V2 + staleness V14) green; `ledger_check` CTest fast shim; ledger unit suite 10/10 incl. the "rejects advance-without-evidence" negative test; `design/diagrams/{architecture.mmd,dashboard.md}` regenerate byte-identically. |
| (c) | Fixed-point library: one law, exhaustive + golden tests | **GREEN** | CTest `fixp` (exhaustive sin/cos 2^16, unit8 pairs, boundary corpus, __int128 rational oracle), `tables_tri` (C++==SV==TS byte-identity), nightly `fixp_rcp_full` (2^24 sweep + frozen hash); goldens `tests/golden/fixp/`; `npm run tables:check` + `tables_check` CTest shim. |
| (d) | Field IR: builder → .zprog → C++ interpreter → golden .zvec → TS differential → minimize demo | **GREEN** | CTest `field_crater_ring` (8 scripted checks incl. double-replay determinism, source-map PC resolution, minimize artifact), `field_ts_differential` + `field_ir` + `crater_ring` + `generated_conformance` (compiler suite 30/30), nightly `field_fuzz_parity` over `tests/fuzz/corpus/field/` (8 committed random programs). Goldens: `captures/golden/field/crater_ring.zvec`; saved failure: `captures/failures/field/fail-484add8d-0x5A17.zvec` + report (§29-17). |
| (e) | ABI byte-identity, capture container, empty-frame replay | **GREEN** | CTest `crc` (spec vectors + residue), `abi_golden` (12 command goldens + frame + zcap), `zcap_roundtrip` (incl. skip-unknown-section), `capture_cli_verify` (zhao-capture CLI), `empty_frame_replay` (ZRef vs Verilated stub: status/completion/COUNTERS parity, corrupt-CRC/reserved-field error parity, .zcap source-ID + program-hash survival), `abi_fuzz_parity` (21-case corpus, C++/SV/TS identical codes), `abi_staleness` + `abi:check`; abi-gen suite 17/17. W6 additions: `zref_shell` (golden replay green + exact fail-safe error codes + session counters), `zemu_smoke`, `desktop_smoke`. |
| (f) | CI workflow present and passing | **GREEN (remote, push lane)** | `.github/workflows/ci.yml` (windows-latest, pinned+SHA256'd oss-cad-suite, cmake+ctest fast lane, npm lane, nightly job). First-ever remote runs failed on three latent gaps (see defect table — the file had never executed before W6 pushed); after fixes f47ec02, 353c1c2, 8cd3339 the push lane is **green on GitHub Actions run 31862780101 @ 8cd3339** (cmake+ctest fast + npm jobs). The nightly/formal job is schedule/workflow_dispatch only — its first remote run is dispatched and noted in PENDING below. |

## Charter §23 Phase-1 gate items, one by one

| Charter gate item | Status | Where |
|---|---|---|
| one empty frame replays through ZRef and a stub RTL model | **GREEN** | `empty_frame_replay`, `stub_top`, `zref_shell` |
| CRC, error and completion semantics match | **GREEN** | `crc`; tri-language error parity in `abi_fuzz_parity` + compiler conformance tests |
| generated C++/TS/SV ABI layouts are byte-identical | **GREEN** | `abi_golden`, `abi_staleness`, `tables_tri`, `generated_conformance.test.ts` |
| one tiny Form program parses, type-checks and lowers to deterministic C++ | **PENDING — deferred by plan §4** (owner: SW.COMPILER.FORM / ZH-019, Phase 3). Wave-1 substitute per FORM §18-L0: the typed Field IR builder drives everything (crater_ring emits a deterministic C++ evaluator wrapper — the "lowers to deterministic C++" half without the parser). | `compiler/tests/crater_ring.test.ts`, `compiler/tests/generated/crater_ring.hpp` |
| one typed Field IR program emits a scalar C++ evaluator, serialized program and random vectors | **GREEN** | `field_crater_ring`, `crater_ring.test.ts`; program hash 0x484add8d, .zvec seed 0x5A17 |
| Field IR interpretation is deterministic and covered by golden vectors | **GREEN** | double-replay identical; `field_ts_differential` (TS byte-identical to C++ golden); fuzz corpus parity |
| source IDs and program hashes survive capture round-trips | **GREEN** | `empty_frame_replay` case 4, `zcap_roundtrip` |

## Maturity advancements (all commit-pinned, ledger-validated)

| Block | From → To | Evidence commits | Ledger rule exercised |
|---|---|---|---|
| SW.ZREF | SPECIFIED → REFERENCE_COMPLETE | 7279493, f0edffa, 9d8dc79, 4131647, e8d652e, 5db7844 | V2 one-step, V3 on-disk evidence |
| SW.FIELDIR | SPECIFIED → REFERENCE_COMPLETE | 500965d, 681a0b6, e8d652e | V2, V3 |
| SW.TOOLS.LEDGER | SPECIFIED → REFERENCE_COMPLETE → UNIT_VERIFIED | f036f75, 8bdeac8 → f036f75 (suite) | V2 one-step-per-commit (two commits, b1e321c-era → 22b9c14 → 7d6cfe2) |
| SW.TOOLS.ABIDOC | SPECIFIED → REFERENCE_COMPLETE → UNIT_VERIFIED | 562787f, 0383ed1 → 4493b9b (17/17) | V2, V3 |
| SW.TOOLS.CAPTURE | SPECIFIED → REFERENCE_COMPLETE → UNIT_VERIFIED | 9d8dc79 → 0383ed1 (golden CLI smoke) | V2, V3 |
| SW.TOOLS.FIXGEN | **not a block** — ruling: owned by SW.ZREF's contract (qformats.md §11; outputs are ZRef tables/goldens). Recorded in blocks.yml note + contract. | f0edffa (counted under SW.ZREF) | documented |
| CMD.DECODER / CMD.SCHEDULER | stay SPECIFIED (by design — stub RTL is not their contract); dashboard notes added | — | plan W6 instruction |
| All hardware-blocked | unchanged, cannot advance | — | V2 |

## Spec bugs and latent defects caught before RTL (the payoff column)

| When | Defect | Resolution |
|---|---|---|
| Plan (architect) | P4's MAD double-rounding; P3's screenXY "s20" off-by-one | A3b single-rounding law; A3c S 12.8 (21-bit) — frozen correctly in qformats.md |
| Plan (architect) | P3 §25 field sum 134 ≠ 128; P2 headline 83 vs own enumeration 87 | sums corrected; ledger registers 87 with the arithmetic note |
| W3 | (none in-spec; exhaustive tests passed against frozen constants) | — |
| W4 | oss-cad-suite `libstdc++-6.dll` shadows winlibs runtime → entry-point failures | static libstdc++/libgcc link |
| W4→W6 | merge EOL churn broke abi byte-identity once (0a384fb re-pin) | byte-identity gates held; root cause (autocrlf) finally closed in W6 (see below) |
| W6 | **C++ validator reported bytes_consumed=0 on header-level aborts** vs spec §3.2 "36", the SV stub (pinned 36) and the TS mirror (default 36) — hidden because the W4 differential compared status only on corrupt frames | fixed (5db7844); parity check added to `test_empty_frame_replay` |
| W6 | abi-gen mutation tests silently no-op'd on CRLF checkouts (2 phantom failures) | loader normalizes (4493b9b); suite 17/17 |
| W6 | CI suite URL 404 (wrong release tag form + .exe suffix); `.gitattributes` missed `compiler/tests/generated/**` so fresh clones failed the wrapper staleness gate | f47ec02 + 353c1c2; verified by fresh-clone simulation (npm ci + all checks green on the clone) |
| W6 | CI cmake/nightly jobs never ran `npm ci`, but the CTest lanes shell out to npm workspaces ('tsc is not recognized') | 8cd3339 (setup-node + npm ci in both jobs); remote push lane green |
| W6 | ledger maturity_log YAML: all-digit commit hashes parsed as integers; `": "` in compact notes broke YAML | quoted hashes; block-scalar notes — both caught by ledger:check |

## Deferred (plan §4) — the ledger says so

Form parser/type-checker/C++ backend (SW.COMPILER.FORM, Phase 3) · MiSTer sys/ (ZH-000) · Quartus/report/resource_actual (ZH-002) · board probes/board_truth.json (ZH-003…006) · rasterizer (Phases 4–5) · Z60/Storm/Duo/audio/particle RTL (Phase 2+).

## BLOCKED on hardware

See `reports/blocked_on_hardware.md` (ZH-000, ZH-002…006, SYNTHESIZED+ states, 8-hour/24-hour stress, each with the exact unblocking action).

## Honest PENDING list

| Item | Owner | Unblocking action |
|---|---|---|
| First REMOTE nightly+formal run | ci.yml nightly job (schedule/workflow_dispatch) | dispatched at wave close; its conclusion is whatever the Actions tab says — do not take this report's word for it |
| Form parser gate item | SW.COMPILER.FORM (Phase 3) | per plan §4; not wave-1 scope |

*Generated by hand (not a tool artifact); every claim above names a test, a
file, or a commit. Where something is not yet true, it says so.*
