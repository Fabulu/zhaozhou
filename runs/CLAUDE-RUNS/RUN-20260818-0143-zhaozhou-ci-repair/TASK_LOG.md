# Zhaozhou CI repair

**Run:** RUN-20260818-0143-zhaozhou-ci-repair  
**Repository:** `C:\programmieren\zencrifice\zhaozhou`  
**Outcome:** Complete; required push CI is green.

## Changes

- `e4132b5554887b36857bd09b7c58203235019800` — Decouple Nanquan from Zhaozhou CI
- `03f0f106163a506e02025c431529de06d20f2b53` — Apply pinned LLVM 15 formatting
- `61e772b790f6013292da3f7d19ba87ddf853615c` — Repair cppcheck findings without suppressions
- `171276e49de8bf559e3322b8e13ba424071b90ad` — Provision pinned Windows CI lint tools

Zhaozhou root npm now owns only ledger, fixgen, and ABI tooling. The 76 tracked C/C++ files with drift were formatted under LLVM 15. Cppcheck findings were fixed concretely through named-field matrix conversion, explicit state initialization, and bounds-clear RGB indexing; no finding was suppressed and nonzero results remain fatal. Windows CTest receives exact LLVM-15 and cppcheck-2.19 paths, and cppcheck wrapper failures report both streams. No Nanquan files or compiler semantics changed.

## Verification

Local clean-HEAD verification:

- LLVM clang-format 15.0.0: 107/107 files passed.
- cppcheck 2.20.0: 107 files, zero findings.
- cppcheck 2.19.0 with complete cfg payload: 107 files, rc 0, zero diagnostics.
- Focused renderer CTest: 3/3 passed (`render_directed`, `render_sky`, `render_star`).
- Complete fast CTest: 87/87 passed, including format and cppcheck wrappers.
- Hardware npm tooling: 75/75 passed (ledger 40, fixgen 14, ABI generator 21); ledger, ten fixgen outputs, and 25 ABI outputs passed staleness checks.

GitHub Actions run: https://github.com/Fabulu/zhaozhou/actions/runs/32080732141

- `cmake + ctest (fast)`: success, 87/87; LLVM 15.0.0 and cppcheck 2.19.0.
- `format + static analysis (charter 27)`: success, 107 files; LLVM 15.0.0 and Ubuntu cppcheck 2.13.0.
- `npm tooling (ledger/fixgen/abi)`: success.
- `ctest (nightly + formal)`: skipped as designed for a push event. The suite download step was separately skipped on a cache hit.

## Preservation and close-out

All 43 pre-existing dirty/staged/untracked records remain present. The framed fingerprint includes status/path/rename source, index entry bytes, and worktree bytes or symlink targets. After normalizing only the two clean-base index entries deliberately advanced by these commits, it exactly matches the baseline:

- Baseline and final normalized SHA-256: `f88eb76ca4bba53e6a96df414d10874fca6d6c6da7c3997acddb6b66a2468685`
- Final raw SHA-256: `b8b98dff62f1b7aa91f112e348f39ba5ed85e1fc852e09a75d53779d691e889d`
- `reference/include/zref/zref_frame.hpp`: index `259df7ab1a74757d22111a75c95e3f64e0a14e0b`; preserved worktree SHA-256 `d6ab5cf912b75370f8f954944db31a5bda401a963a65b77f9db50743f3f1c084`
- `tests/render/render_sky.cpp`: index `3479630bf0a50bfd652f410e6b538a5c98344da7`; preserved worktree SHA-256 `1dcabe287ec208c8abab83d74547beb8c83cc4eaf0cd897d0d478fab3d0e6f4c`

`HEAD == origin/main == remote main == 171276e49de8bf559e3322b8e13ba424071b90ad`.
