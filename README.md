# Zhaozhou

A game console. FPGA-based hardware, written in a conservative SystemVerilog
subset, verified against a bit-exact C++ reference (ZRef) before any RTL
matures. Nothing here is designed by guesswork: the
[engineering charter](ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md) is law.

## Status

Wave 1 (Phase 1 foundation) in progress: repository skeleton, CMake/Ninja +
CTest build, Verilator harness, CI, and the Phase-1-gate stub RTL top are in.
No ISA commits beyond the ratified plan, no synthesis, no board — every
maturity claim in `design/blocks.yml` (W2) must carry commit-pinned evidence.

## Naming: Form and Nanquan

The programming language for this console is **Form** (charter 3.2; see
[FORM_LANGUAGE_HARDWARE_CODESIGN.md](FORM_LANGUAGE_HARDWARE_CODESIGN.md)).
The *nanquan* sibling repo is intended to become the standalone toolchain
**distribution** for Form — same language, separate packaging. Inside this
repository the vocabulary is always Form. Details: [docs/naming.md](docs/naming.md).

## Layout

```
spec/            specifications (qformats, ABI .zidl, capture format, form/)
design/          machine-readable ledger: blocks, ops, diagrams, budgets
fpga/            MiSTer-packaged core: Zhaozhou.sv glue, rtl/ (the actual core)
reference/       ZRef: the C++17 golden-reference library
compiler/        Form compiler (frontend, HIR, ZIR, Field IR, backends)
emulator/        ZEmu (desktop console shell)
runtime/         desktop / MiSTer runtimes
tools/           abi-gen, fixgen, ledger, capture, pack, inspect, report, env
tests/           harness (shared Verilator helpers), unit, differential, fuzz, formal
captures/        golden vectors and saved failing vectors (evidence)
reports/         synthesis/timing/bandwidth/coverage evidence
demos/           wound_lab and friends
```

## Quickstart (Windows, oss-cad-suite)

```powershell
. .\tools\env\zhao-env.ps1     # VERILATOR_ROOT + suite bin/lib + winlibs cmake/ninja/g++
cmake --preset windows-native  # Ninja, g++ 16.1.0, C++17 -> build/
cmake --build build
ctest --test-dir build -L fast --output-on-failure
```

Git Bash equivalent:

```bash
export PATH="/c/programmieren/dsstuff/mingw64/bin:$PATH"
export VERILATOR_ROOT="C:/programmieren/zencrifice/.tools/oss-cad-suite/share/verilator"
export PATH="/c/programmieren/zencrifice/.tools/oss-cad-suite/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/lib:$PATH"
```

Node lane (compiler + tools): `npm ci && npm test`.

Machine rules (hard failures, see `tools/env/`): never the devkitPro msys2
cmake (broken with native g++); no spaces in the repo or build path; invoke
`verilator_bin.exe`, not the perl wrapper; C++17 on every TU.

## Related

- [Nanquan](../nanquan) — future standalone Form toolchain distribution
- [Tribute Upheaval](../Upheaval) — the game that runs on this console
- [AGENT_START_HERE.md](AGENT_START_HERE.md) — agent onboarding (charter pointers)
