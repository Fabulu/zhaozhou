# Task Log: RUN-20260814-1852 - [Describe objective here]

**Created:** 2026-08-14 18:52 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260814-1852-wave1-build-skeleton/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-14 18:52 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260814-1852
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

### 2026-08-14 ~19:40 UTC+02:00 - P1 RECON complete (subagent)

**Agent:** RECON for piece P1 (repo skeleton, build system, verification harness)

**Done:**
- Read charter v0.2, START_HERE v0.2, Form codesign §3/§18; surveyed zhaozhou scaffold + nanquan + untitled-game.
- Online: mapped MiSTer Template_MiSTer structure (sys/ read-only, sys_top top entity, files.qip hand-maintained, Quartus 17.0.2 pin); oss-cad-suite Windows usage; Verilator CMake official examples; GH Actions Windows CI options.
- Extracted the downloaded oss-cad-suite exe (it is a tar.gz, 591MB -> 2.1GB) to .tools/oss-cad-suite/. Version 20260814: Verilator 5.051 devel, Yosys 0.68+64, SBY 0.68, iverilog.
- VERIFIED END-TO-END ON THIS MACHINE: verilator_bin.exe --cc -> C++ -> winlibs g++ 16.1.0 -> exe -> CTest pass; and CMake find_package(verilator) + verilate() + Ninja + CTest (100% pass). Evidence in scratch/vtest/.

**Key machine facts (verified):**
- VERILATOR_ROOT must be <suite>/share/verilator (contains include/); call verilator_bin.exe directly.
- yosys/sby need <suite>/bin AND <suite>/lib on PATH (libreadline8.dll in lib/).
- Use C:\programmieren\dsstuff\mingw64\bin\cmake.exe (4.3.2 native). devkitPro msys2 cmake 4.0.2 is INCOMPATIBLE with native g++ (POSIX path ABI) - reproduced.
- Harness needs sc_time_stamp() shim; C++17 everywhere; NO spaces in build paths.

**Flags for architect:**
- NAMING TENSION: nanquan README claims to be "the language for the Zhaozhou console"; charter names the language Form (compiler/ lives inside zhaozhou per §22). Decision needed.
- Scaffold .gitignore ignores *.json globally -> would exclude reports/board_truth.json, CMakePresets.json, capture manifests. Must fix in first P1 commit.
- Scaffold tree (rtl/sim/constraints/tools) must be restructured to charter §22; README rewrite needed.
- sys/ vendor step (ZH-000) deferred OK: Verilator path does not need sys/. Pin Template_MiSTer hash when imported.

**Findings:** returned as subagent final report (full FINDINGS content; harness policy prevents subagent from writing FINDINGS.md - orchestrator should persist it to FINDINGS.md in this run dir).

---

## 2026-08-14 — W1 IMPLEMENTER: repository skeleton, toolchain, Verilator harness, CI

**Agent:** IMPLEMENTER (W1), plan RUN-20260814-1912-wave1-architecture-plan 2/W1, 5/1-5.

**Commits on zhaozhou main (local only, not pushed):**

1. `e039a63` — .gitignore fix: removed global `*.json`/`*.log` (and `*.bin` — deviation, see below), kept `obj_dir/`,`*.vcd`,`*.fst`, added `build/`,`build-*/`,`node_modules/`.
2. `3e67b62` — skeleton: full charter 22 tree w/ .gitkeep; fpga Quartus placeholders (Zhaozhou.sv stub, .qpf/.qsf/.sdc, files.qip, sys/PROVENANCE.md); CMakeLists + CMakePresets.json (windows-native pins winlibs g++ 16.1.0/ninja; windows-ci for runners); cmake/zhao_verilate.cmake; tools/env/zhao-env.ps1|.bat; package.json workspaces + package-lock.json; legacy rtl/sim/constraints/docs removed.
3. `6b5f4db` — fpga/rtl/common/zhao_frame_pkg.sv (PLACEHOLDER frame package, 36-byte header per plan W4/P5 3.1) + zhao_stub_top.sv (ready/valid byte stream, magic/abi/length validation, status/completion_flags/counters, hdr_parity observation port); tests/harness (tick/reset/send_byte/idle, check registry, failing-vector serializer, sc_time_stamp shim); tests/unit/test_stub_top.cpp; tests/lint + tests/formal CTest wrappers. Labels fast/nightly/formal(SHIFTY skip)/lint.
4. `da82d41` — .github/workflows/ci.yml: build (pinned oss-cad-suite 20260814, SHA256 4A3891A642681AB087FD9F6324519247B4C3761AFDC74DC1683067A2DAABD663, cached; g++ discovered on runner; cmake --preset windows-ci + ctest -L fast), npm lane, scheduled nightly (-L nightly).
5. `f1d5bb6` — docs: README rewrite, docs/naming.md (Form/nanquan hybrid, docs-only), charter copies w/ provenance headers (ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md, FORM_LANGUAGE_HARDWARE_CODESIGN.md, AGENT_START_HERE.md).

**Verification (all green, clean-build re-run):**
- `verilator_bin --lint-only -Wall` on zhao_stub_top.sv + zhao_frame_pkg.sv: clean, ZERO warnings (one UNUSEDPARAM found + fixed by making ZHAO_COMMAND_ALIGNMENT load-bearing).
- Fresh build dir: `cmake --preset windows-native && cmake --build build && ctest --test-dir build --output-on-failure` -> **100% tests passed, 0 tests failed out of 3** (stub_top, lint_stub_top, formal_lane); label summary fast=2 nightly=2 formal=1 lint=1.
- git status clean; only build/ ignored and untracked-generated.
- npm placeholders pass; package-lock.json/CMakePresets.json confirmed not ignored.

**Gotchas found (for W2-W6):**
- verilate() PREFIX defaults to the first SOURCE file name — with the package listed first the model became Vzhao_frame_pkg; tests/CMakeLists pins `PREFIX Vzhao_stub_top TOP_MODULE zhao_stub_top`.
- The suite ships NO g++ — CI discovers one from the runner image.
- Suite's verilator-config.cmake exports only `verilator_FOUND` + `VERILATOR_ROOT` (no Verilator_VERSION/EXECUTABLE vars).
- The devkitPro msys2 ctest on PATH silently breaks add_test paths (BAD_COMMAND) — always source tools/env first.

**Deviations:** dropped the global `*.bin` ignore alongside `*.json`/`*.log` (W3/W4 golden vector .bin files are committed evidence — same failure mode 1.F was fixing). `cmake -P` scripts cannot exit 77, so the formal lane skips via SKIP_REGULAR_EXPRESSION instead of SKIP_RETURN_CODE.

**W4 swap point:** replace `import zhao_frame_pkg::*;` with `import zhao_abi_pkg::*;` in zhao_stub_top.sv + the tests/CMakeLists SOURCES list — documented in both SV files.
