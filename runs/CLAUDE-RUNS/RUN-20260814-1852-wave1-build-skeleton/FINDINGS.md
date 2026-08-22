# FINDINGS — P1 Recon: Repo Skeleton, Build System, Verification Harness (ZH-000/ZH-015/ZH-016 context)

**Run:** RUN-20260814-1852-wave1-build-skeleton — **Date:** 2026-08-14 — **Agent:** RECON (P1)
*Persisted by orchestrator (subagent file-write blocked by harness policy). Working evidence in `scratch/vtest/` of this run dir (CMakeLists.txt, counter.sv, sim_main.cpp).*

## 0. Summary

The charter §20.3/§27 verification stack works natively on this Windows machine. All "verified" items below were executed 2026-08-14 in this run. Key machine facts, MiSTer structure, CMake patterns, CI skeleton, and a recommended §22-adapted tree follow, with sources in §8.

## 1. Offline: scaffold and siblings

### 1.1 `zhaozhou/` (branch main, remote github.com/Fabulu/zhaozhou.git)
- Tree: README.md, docs/SPEC.md (placeholder), empty rtl/, sim/, tools/, constraints/ (.gitkeep). One commit `3faecac`. Clean.
- Does not match charter §22 (`spec/`, `design/`, `fpga/rtl/…`, `reference/`, `tests/`, …). Restructure + README rewrite required (README even says specs land in `docs/`).
- `.gitignore` defects (fix first commit): global `*.json` would silently exclude `reports/board_truth.json` (Phase 0 deliverable), `CMakePresets.json`, capture manifests; `*.log` too broad for evidence files. Good: `obj_dir/`, `*.vcd`, `*.fst`.

### 1.2 Naming tension (architect decision needed)
- nanquan README: "The programming language for the Zhaozhou console." untitled-game: "written in Nanquan." Charter §3.2 + FORM_LANGUAGE doc name the language **Form**, with `compiler/` inside zhaozhou (§22). Options: (1) Nanquan = project codename, Form = language (clarify READMEs); (2) alias Form→Nanquan (bad: charter uses "Form" pervasively); (3) Form in zhaozhou/compiler, nanquan repo repurposed as standalone distribution. **Recommend 1+3 hybrid.**
- Ecosystem conventions to respect: README with Status/Layout/Related sections + relative links; placeholder SPEC.md listing "Sections to fill in"; `.gitkeep` in empty dirs.

### 1.3 Charter constraints shaping P1
- SV subset §2 (packages/structs/enums/always_ff; no classes/queues). §20.3: Verilator model **linked into the same test executable as ZRef** — drives CMake design (one exe per test family; CTest labels). Phase 1 gate: one empty frame replays through ZRef + stub RTL model → skeleton needs `zhao_stub_top.sv` that verilates today. Tools present: Verilator/Yosys/SBY (suite), CMake/Ninja/g++16.1/Node20/Py3.14. Quartus out of scope locally.

## 2. MiSTer template (online)

Source: `MiSTer-devel/Template_MiSTer` Readme.md + Template.qsf + sys/sys.qip + sys/ listing (fetched 2026-08-14).

Standard core layout: `<core>.qpf` (copy, change PROJECT_REVISION), `<core>.qsf` (minimal changes; revert if Quartus "spits" settings into it), optional `<core>.srf` (warning suppressions, free), optional `<core>.sdc` (constraints, free), `<core>.sv` (framework↔core glue = charter's Zhaozhou.sv), `files.qip` (hand-maintained core file list), `clean.bat`, `releases/` (`<core>_YYYYMMDD.rbf`), `rtl/` (core sources; must contain `pll/` with pll.v + pll.qip), `sys/` (framework — "prohibited to change"; updates erase customizations).

`sys/` (~55 files): `sys_top.v` (framework top), `sys.qip`/`sys.tcl`/`sys_analog.tcl`/`sys_dual_sdram.tcl`/`sys_top.sdc`, `hps_io.sv`, `video_mixer.sv`, `ascal.vhd` (scaler), `gamma_corr.sv`, `osd.v`, audio (`audio_out.sv`, `i2s.v`, `spdif.v`, `sigma_delta_dac.sv`, `alsa.sv`), `sd_card.sv`, `mcp23009.sv`, `ltc2308.sv`, `mt32pi.sv`, PLL IPs (`pll_q17.qip`, `pll_hdmi*`, `pll_audio*`), `build_id.tcl`, video extras (hq2x, scanlines, shadowmask, scandoubler, video_freak/freezer/cleaner).

Inclusion: `.qsf` sets `TOP_LEVEL_ENTITY sys_top` and does `source sys/sys.tcl`, `source sys/sys_analog.tcl`, `source files.qip`. Feature macros: `MISTER_FB`, `MISTER_FB_PALETTE`, `MISTER_DUAL_SDRAM`, `MISTER_DEBUG_NOHDMI`, `MISTER_SMALL_VBUF`, `MISTER_DOWNSCALE_NN`, `MISTER_DISABLE_ADAPTIVE`. Quartus pinned **17.0.2**.

Implications: keep charter §22 shape (`fpga/` with `Zhaozhou.sv`, `files.qip`, `sys/`, `rtl/` nested) — hand-maintained files.qip makes depth harmless. **`sys/` import deferred**: Verilator path needs only `fpga/rtl/`; vendor Template_MiSTer `sys/` verbatim + record upstream hash in `fpga/sys/PROVENANCE.md`; CI check `git diff` vs pin (charter §29 rule 3). Repo is small — multi-GB concern applies only to Main_MiSTer release assets.

## 3. oss-cad-suite on Windows (verified locally)

The download is a gzip tar (591 MB → ~2.1 GB). Extracted with `tar xzf` (no execution needed). Versions verified: suite `20260814`; Verilator `5.051 devel rev v5.050-176-g7da43d830 (mod)`; Yosys `0.68+64`; `SBY v0.68`; iverilog present. Contains `environment.bat/.ps1/start.bat`, `bin/`, `lib/` (DLLs, bundled python3.exe), `share/verilator/` (include/, bin/, examples/, **verilator-config.cmake**).

Verified env recipe (minimal, for scripts/CMake):
```
VERILATOR_ROOT = <suite>\share\verilator     (contains include/verilated_std.sv and verilator-config.cmake)
PATH prepend  : <suite>\bin ; <suite>\lib
call          : verilator_bin.exe (not the perl wrapper `verilator`)
```

Gotchas (each reproduced + solved):
1. `VERILATOR_ROOT` must be `share\verilator` (contains `include/verilated_std.sv`); suite root → "Cannot find verilated_std.sv" for --cc flows (cf. YosysHQ/oss-cad-suite-build#142).
2. `bin\verilator` is perl; no bundled perl → use `verilator_bin.exe`.
3. `yosys.exe` needs `lib` on PATH (`libreadline8.dll`).
4. No spaces in build paths — verilated.mk hard-fails (reproduced under `C:\Users\Fabian Trunz\...`; zhaozhou path safe; GH runners `D:\a\...` safe).
5. Windows-style source paths when invoking exes from Git Bash (`cygpath -w`).
6. Harness must define `double sc_time_stamp(){return 0;}` — verilated.cpp references it (this build).
7. C++17 everywhere or g++16 ABI link errors (`std::string` move ctor undefined) — reproducible when verilated.cpp compiled without `-std=c++17`.
8. Skip `--build` (Verilator-internal make) in the CMake flow; CMake compiles generated C++ itself.

Fallbacks (validated equivalents, not needed): MSYS2 `mingw-w64-x86_64-verilator` (packages.msys2.org); WSL + linux-x64 suite (YosysHQ's own "best experience" advice). Verilator native-Windows tracking: verilator/verilator#4255.

## 4. CMake + Verilator + CTest (verified)

Working example kept at `…RUN-20260814-1852-wave1-build-skeleton/scratch/vtest/` (counter.sv, sim_main.cpp, CMakeLists.txt). Verified commands:
```
export VERILATOR_ROOT="C:\\…\\share\\verilator"
export PATH="/c/programmieren/dsstuff/mingw64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # winlibs cmake 4.3.2
cmake --build build ; ctest --test-dir build --output-on-failure  # → 100% passed
```

CMakeLists pattern (from Verilator's official `examples/cmake_hello_c`):
```cmake
cmake_minimum_required(VERSION 3.20)
project(zhao CXX)
set(CMAKE_CXX_STANDARD 17); set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(verilator HINTS "$ENV{VERILATOR_ROOT}")
add_executable(vsim sim_main.cpp)
verilate(vsim INCLUDE_DIRS … SOURCES counter.sv)
enable_testing(); add_test(NAME counter COMMAND vsim)
```

Toolchain table: devkitPro msys2 cmake 4.0.2 — **broken with native g++** (emits `/c/...` paths native ninja can't consume; reproduced); `C:\programmieren\dsstuff\mingw64\bin\cmake.exe` 4.3.2 — **use this** (verified); `C:\Program Files\CMake` 3.29.2 — native, likely fine, verify once. Pin via CMakePresets.json.

Differential-harness layout: one exe per block family linking ZRef + verilated model + shared harness lib (`tests/harness/`: clock/eval loop, ready/valid stimulus, bit-exact compare, failing-vector serialization per §20.3/§29-17, CRC). Corpus as committed data (`captures/golden/*.zcap`, seeded vectors in `tests/differential/corpus/`). CTest labels: `fast` (every commit), `nightly` (full fuzz+captures), `formal` (sby via script-wrapper test with SKIP when suite absent), `lint` (per-module `--lint-only -Wall` as tests, gateable per §21 step 8).

## 5. CI skeleton

`windows-latest` ships CMake/Node/Python; no Quartus → gate on Verilator lint/diff/formal + C++/TS units. `YosysHQ/setup-oss-cad-suite@v4` is Linux-only per README — for Windows, download release asset and `tar -xzf` it (proven: the asset is a tar.gz), cache with actions/cache, set VERILATOR_ROOT + bin/lib PATH, then cmake/ninja/ctest `-L fast`; scheduled nightly job `-L nightly`; npm lane (`npm ci && npm test`) for compiler/abi-gen; hash-compare generated ABI outputs (SV/C++/TS byte-identical gate). Pin suite version + SHA256.

## 6. Recommended skeleton — charter §22, Windows-first

```
zhaozhou/
├── AGENT_START_HERE.md / CHARTER / FORM_LANGUAGE doc copies (charter-mandated in-repo)
├── README.md (rewritten: Form naming note, quickstart)
├── CMakeLists.txt (project CXX, C++17, options; add_subdirectory reference/tests/…)
├── CMakePresets.json ("windows-native": winlibs cmake+ninja, VERILATOR_ROOT)
├── package.json (npm workspaces: compiler, tools/abi-gen, tools/report)
├── .gitignore (FIXED: no global *.json)
├── .github/workflows/ci.yml
├── cmake/ (FindVerilator wrapper → verilator_bin.exe fallback; zhao_verilate_model + lint-test helpers)
├── tools/env/zhao-env.ps1|.bat (encode §3 recipe)
├── spec/ … design/ (blocks.yml, ops.yml schemas + generators) — per charter §22
├── fpga/ (Zhaozhou.sv glue stub; .qpf/.qsf/.sdc/files.qip placeholders; sys/ vendored later w/ PROVENANCE.md)
│   └── rtl/{generated,common,platform,command,…}/ + zhao_stub_top.sv (Phase-1-gate stub, verilates today)
├── reference/{include,src}/  (ZRef C++17 static lib)
├── emulator/ runtime/{desktop,mister}/ (empty Phase 1)
├── compiler/src/{frontend,hir,zir,field_ir,backends/{cpp,zdl},generated}/ + tests/
├── tools/{abi-gen,pack,capture,inspect,report,board-probe}/
├── tests/{harness,unit,differential,fuzz,formal}/ (CTest labels fast/nightly/formal/lint)
├── captures/{golden,failures}/  reports/…  demos/wound_lab/
```

Build commands for this machine: see §4 (with tools/env sourced). package.json scripts: `test`, `lint`, `abi:gen` (emits `fpga/rtl/generated/zhao_abi_pkg.sv`, `runtime/include/zhao_abi.h`, `compiler/src/generated/abi.ts`), plus `tools/abi-gen/consistency.mjs` hashing the three outputs for the byte-identical gate.

## 7. Risks for the architect

1. Nanquan-vs-Form decision blocks README + compiler docs (§1.2). 2. `.gitignore` `*.json` — trivial fix, high latent damage. 3. devkitPro cmake shadowing — encode toolchain in presets/env. 4. Verilator is a devel nightly — pin suite `20260814`; fallbacks §3. 5. sby not smoke-tested end-to-end yet. 6. ZH-000 sys/ vendoring needs pin strategy (hash in PROVENANCE.md). 7. sc_time_stamp shim is version-quirky — keep in tests/harness with comment. 8. Space-free path invariant — document as machine rule. 9. Quartus absent → Phase 0 hardware items BLOCKED-on-hardware in ledger; P1 local scope all green.

## 8. Sources

- https://github.com/MiSTer-devel/Template_MiSTer (Readme.md, Template.qsf, files.qip, sys/sys.qip, sys/ listing)
- https://github.com/YosysHQ/oss-cad-suite-build (README; releases)
- https://github.com/YosysHQ/oss-cad-suite-build/issues/142 ; https://github.com/verilator/verilator/issues/4255
- https://github.com/verilator/verilator/tree/master/examples (cmake_hello_c, cmake_protect_lib)
- https://stackoverflow.com/questions/77585454/ (verilate() + SV packages)
- https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%2520With%2520CMake%2520and%2520CTest.html
- https://github.com/YosysHQ/setup-oss-cad-suite ; https://github.com/marketplace/actions/setup-oss-cad-suite ; https://github.com/actions/runner-images
- https://packages.msys2.org/package/mingw-w64-x86_64-verilator ; https://spinalhdl.github.io/SpinalDoc-RTD/master/SpinalHDL/Simulation/install/Verilator.html
