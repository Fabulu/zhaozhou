# Tests. CTest labels (charter 20.3, P1 findings 4):
#   fast    — every commit (the default gate: ctest --test-dir build -L fast)
#   nightly — full fuzz + capture replay (scheduled CI job)
#   formal  — SymbiYosys lane; wrapper test SKIPs when sby is absent
#   lint    — per-module verilator --lint-only -Wall (charter 21 step 8)

# ---- shared harness library (clock/eval loop, ready/valid stimulus,  -------
# ---- bit-exact compare, failing-vector serializer, sc_time_stamp shim) ----
add_library(zhao_harness STATIC harness/zhao_sim.cpp)
target_include_directories(zhao_harness PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/harness)

# ---- unit: Verilated zhao_stub_top (Phase-1 gate (a) stub test) -----------
# W4: the stub now consumes the GENERATED zhao_abi_pkg (the W1 placeholder
# zhao_frame_pkg.sv was removed when the swap landed).
set(ZHAO_ABI_PKG ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv)

add_executable(test_stub_top unit/test_stub_top.cpp)
target_link_libraries(test_stub_top PRIVATE zhao_harness zhao_zref)
verilate(test_stub_top
  PREFIX Vzhao_stub_top      # default derives from first SOURCE (the package!)
  TOP_MODULE zhao_stub_top
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_stub_top.sv)

add_test(NAME stub_top COMMAND test_stub_top)
set_tests_properties(stub_top PROPERTIES LABELS "fast;nightly")

# ---- lint: verilator --lint-only -Wall over the RTL (gate per charter 21) -
# Direct test command (no nested-cmake wrapper): the ENVIRONMENT property
# carries VERILATOR_ROOT from the cache variable resolved by zhao_verilate.cmake
# (env hint first, then derived from the binary location), so an env-less
# reconfigure (ninja regenerate) cannot poison it.
find_program(ZHAO_VERILATOR_BIN NAMES verilator_bin verilator_bin.exe)
if(NOT ZHAO_VERILATOR_BIN)
  message(FATAL_ERROR "verilator_bin not found on PATH — source tools/env/zhao-env.ps1|.bat")
endif()
set(VERILATOR_ROOT "$ENV{VERILATOR_ROOT}")
# zhao-env.bat exports the backslash form (C:\...\share\verilator); embedded
# verbatim into an ENVIRONMENT property those backslashes must be normalized
# so either env form works.
string(REPLACE "\\" "/" VERILATOR_ROOT "${VERILATOR_ROOT}")
# mingw ctest treats forward-slash drive paths in raw test commands as
# relative; normalize to the native backslash form
set(ZHAO_VERILATOR_LINT "${ZHAO_VERILATOR_BIN}")
string(REPLACE "/" "\\" ZHAO_VERILATOR_LINT "${ZHAO_VERILATOR_LINT}")
add_test(NAME lint_stub_top
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_stub_top
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_stub_top.sv
                 ${CMAKE_SOURCE_DIR}/tests/differential/zhao_abi_probe.sv)
set_tests_properties(lint_stub_top PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- formal: SymbiYosys lane smoke (SKIP when sby absent) -----------------
# W1 wires the lane only; real properties land with the blocks that own them.
find_program(ZHAO_SBY_BIN NAMES sby sby.exe)
configure_file(formal/formal_lane.cmake.in formal/formal_lane.cmake @ONLY)
add_test(NAME formal_lane
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/formal/formal_lane.cmake)
set_tests_properties(formal_lane PROPERTIES
  LABELS "formal;nightly"
  SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")

# ---- W2: ledger npm checks (plan R11) --------------------------------------
# The design-ledger validator + staleness gate (npm run ledger:check) also
# runs locally via ctest so byte-identity/maturity rules cannot rot between
# CI runs. Wrapped in cmake -P (same pattern as the lint lane): CTest cannot
# launch npm.cmd directly on Windows. Self-contained append; merges cleanly
# with the W3/W4 shims.
find_program(ZHAO_NPM_BIN NAMES npm.cmd npm)
if(NOT ZHAO_NPM_BIN)
  message(STATUS "npm not found on PATH — ledger_check test not registered (source tools/env/zhao-env.ps1|.bat)")
else()
  configure_file(npm/ledger_check.cmake.in npm/ledger_check.cmake @ONLY)
  add_test(NAME ledger_check
           COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/npm/ledger_check.cmake)
  set_tests_properties(ledger_check PROPERTIES LABELS "fast")
endif()

# ---- W3: fixed-point (spec/qformats.md; library: reference/include/zref) --
add_executable(test_fixp unit/test_fixp.cpp)
target_link_libraries(test_fixp PRIVATE zhao_zref)
target_compile_definitions(test_fixp PRIVATE ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden/fixp")
add_test(NAME fixp COMMAND test_fixp)
set_tests_properties(fixp PROPERTIES LABELS "fast;nightly")

# full 2^24 rcp_u24 sweep + frozen full-domain hash (nightly-only mode of the
# same binary; plan W3: "full sweep behind a nightly-labeled or tool-mode run")
add_test(NAME fixp_rcp_full COMMAND test_fixp --rcp-full)
set_tests_properties(fixp_rcp_full PROPERTIES LABELS "nightly")

# tri-language table byte identity: C++ constexpr == SV .mem == TS const
add_executable(test_tables_tri unit/test_tables_tri.cpp)
target_link_libraries(test_tables_tri PRIVATE zhao_zref)
target_compile_definitions(test_tables_tri PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME tables_tri COMMAND test_tables_tri)
set_tests_properties(tables_tri PROPERTIES LABELS "fast;nightly")

# npm tables:check as a CTest fast shim (plan R11: local ctest enforces the
# generated-file staleness gate, not just CI)
find_program(NPM_EXECUTABLE NAMES npm.cmd npm)
if(NPM_EXECUTABLE)
  add_test(NAME tables_check COMMAND ${NPM_EXECUTABLE} run -w tools/fixgen check
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(tables_check PROPERTIES LABELS "fast")
else()
  message(WARNING "npm not found on PATH: tables_check CTest shim not registered")
endif()
# ---- W4: abi/frame/zcap -----------------------------------------------------
# Self-contained block (clean merge): CRC conformance, byte-identity matrix
# (C++ testee), .zcap round-trips, the zhao-capture CLI smoke, the fuzz-corpus
# parity (C++ + Verilated SV probe vs oracle expectations), and the npm
# staleness shim (plan R11: local ctest enforces abi:check too).

add_executable(test_crc unit/test_crc.cpp)
target_link_libraries(test_crc PRIVATE zhao_zref zhao_harness)
add_test(NAME crc COMMAND test_crc)
set_tests_properties(crc PROPERTIES LABELS "fast;nightly")

add_executable(test_abi_golden unit/test_abi_golden.cpp)
target_link_libraries(test_abi_golden PRIVATE zhao_zref zhao_harness)
add_test(NAME abi_golden COMMAND test_abi_golden)
set_tests_properties(abi_golden PROPERTIES LABELS "fast;nightly")

add_executable(test_zcap_roundtrip unit/test_zcap_roundtrip.cpp)
target_link_libraries(test_zcap_roundtrip PRIVATE zhao_zref zhao_harness)
add_test(NAME zcap_roundtrip COMMAND test_zcap_roundtrip)
set_tests_properties(zcap_roundtrip PROPERTIES LABELS "fast;nightly")

add_test(NAME capture_cli_verify
         COMMAND zhao-capture verify ${CMAKE_SOURCE_DIR}/tests/abi/golden/zcap_minimal.zcap)
set_tests_properties(capture_cli_verify PROPERTIES LABELS "fast;nightly")

# npm staleness shim (R11): regenerates all abi outputs and diffs them.
# npm must be the Windows launcher (npm.cmd), not the bundled sh script
unset(ZHAO_NPM_BIN CACHE)
find_program(ZHAO_NPM_BIN NAMES npm.cmd npm)
if(ZHAO_NPM_BIN)
  add_test(NAME abi_staleness
           COMMAND ${ZHAO_NPM_BIN} run abi:check --prefix ${CMAKE_SOURCE_DIR})
  set_tests_properties(abi_staleness PROPERTIES LABELS "fast;nightly")
else()
  message(STATUS "zhao: npm not found; abi_staleness ctest shim disabled")
endif()

# differential: empty frame replays through ZRef AND the Verilated stub
# (status/completion/counters parity + tri-language error parity + .zcap
# source-id/program-hash round trip)
add_executable(test_empty_frame_replay differential/test_empty_frame_replay.cpp)
target_link_libraries(test_empty_frame_replay PRIVATE zhao_harness zhao_zref)
verilate(test_empty_frame_replay
  PREFIX Vzhao_stub_top
  TOP_MODULE zhao_stub_top
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_stub_top.sv)
add_test(NAME empty_frame_replay COMMAND test_empty_frame_replay)
set_tests_properties(empty_frame_replay PROPERTIES LABELS "fast;nightly")

# fuzz: tri-language parity over the committed corpus (C++ + SV via the
# probe; TS asserts the same corpus in the compiler workspace) + the SV
# pack/unpack byte-identity matrix for every command
add_executable(test_abi_fuzz_parity fuzz/test_abi_fuzz_parity.cpp)
target_link_libraries(test_abi_fuzz_parity PRIVATE zhao_harness zhao_zref)
verilate(test_abi_fuzz_parity
  PREFIX Vzhao_abi_probe
  TOP_MODULE zhao_abi_probe
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/tests/differential
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/tests/differential/zhao_abi_probe.sv)
add_test(NAME abi_fuzz_parity COMMAND test_abi_fuzz_parity)
set_tests_properties(abi_fuzz_parity PROPERTIES LABELS "fast;nightly")


# ---- W2.1: frozen interface package lint (charter 21 step 8) ---------------
# fpga/rtl/common/zhao_pkg.sv is the FROZEN wave-2 interface package (plan
# §3: changes require architect sign-off). Lint it standalone with its only
# dependency (the generated ABI package) so interface drift is caught the
# moment it is written, before any consumer module exists. Banner-append
# block (merge rule, plan §3).
add_test(NAME lint_zhao_pkg
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv)
set_tests_properties(lint_zhao_pkg PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- W2.5: memory -----------------------------------------------------------
# Plan W2.5: synthesizable modules lint individually (charter 21 step 8);
# the BEHAVIOURAL SDRAM MODEL (sim/models/zhao_sdram_model.sv) is a
# TESTBENCH-ONLY component (spec/memory_rules.md 1, plan D2) and is
# DELIBERATELY EXCLUDED from every lint target and from fpga/files.qip —
# it is compiled only into the Verilated testbench wrappers below.
# Test tops live in tests/memory/ (also never linted/synthesized).
set(ZHAO_MEM_RTL ${CMAKE_SOURCE_DIR}/fpga/rtl/memory)
set(ZHAO_MEM_PKGS
    ${ZHAO_ABI_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${ZHAO_MEM_RTL}/zhao_sdram_params_pkg.sv)

# per-module lint (synthesizable subset only)
foreach(mem_mod IN ITEMS sdram_ctrl vram_arbiter mem_guard hps_bridge)
  add_test(NAME lint_mem_${mem_mod}
           COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                   --top-module zhao_${mem_mod}
                   ${ZHAO_MEM_PKGS} ${ZHAO_MEM_RTL}/zhao_${mem_mod}.sv)
  set_tests_properties(lint_mem_${mem_mod} PROPERTIES
    LABELS "lint;fast"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()
add_test(NAME lint_mem_params
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 ${ZHAO_MEM_RTL}/zhao_sdram_params_pkg.sv)
set_tests_properties(lint_mem_params PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# directed/random/budget tests (Verilated wrappers; the sim model rides
# along only where the chain is instantiated)
set(ZHAO_MEM_SOURCES_CHAIN
    ${ZHAO_MEM_PKGS}
    ${ZHAO_MEM_RTL}/zhao_sdram_ctrl.sv
    ${ZHAO_MEM_RTL}/zhao_vram_arbiter.sv
    ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_zhao_mem_chain.sv)

add_executable(test_sdram_directed memory/sdram_directed.cpp)
target_link_libraries(test_sdram_directed PRIVATE zhao_harness zhao_zref)
verilate(test_sdram_directed
  PREFIX Vtb_zhao_mem_chain TOP_MODULE tb_zhao_mem_chain
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_MEM_SOURCES_CHAIN})
add_test(NAME mem_sdram_directed COMMAND test_sdram_directed)
set_tests_properties(mem_sdram_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_vram_arbiter_directed memory/vram_arbiter_directed.cpp)
target_link_libraries(test_vram_arbiter_directed PRIVATE zhao_harness zhao_zref)
verilate(test_vram_arbiter_directed
  PREFIX Vtb_zhao_mem_chain TOP_MODULE tb_zhao_mem_chain
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_MEM_SOURCES_CHAIN})
add_test(NAME mem_vram_arbiter_directed COMMAND test_vram_arbiter_directed)
set_tests_properties(mem_vram_arbiter_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_mem_random memory/mem_random.cpp)
target_link_libraries(test_mem_random PRIVATE zhao_harness zhao_zref)
verilate(test_mem_random
  PREFIX Vtb_zhao_mem_chain TOP_MODULE tb_zhao_mem_chain
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_MEM_SOURCES_CHAIN})
add_test(NAME mem_random COMMAND test_mem_random)
set_tests_properties(mem_random PROPERTIES LABELS "fast;nightly")
# nightly long mode of the same binary: 100,000-request PCG streams
add_test(NAME mem_random_long COMMAND test_mem_random --long)
set_tests_properties(mem_random_long PROPERTIES LABELS "nightly")

add_executable(test_mem_bandwidth_budget memory/mem_bandwidth_budget.cpp)
target_link_libraries(test_mem_bandwidth_budget PRIVATE zhao_harness zhao_zref)
verilate(test_mem_bandwidth_budget
  PREFIX Vtb_zhao_mem_chain TOP_MODULE tb_zhao_mem_chain
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_MEM_SOURCES_CHAIN})
add_test(NAME mem_bandwidth_budget COMMAND test_mem_bandwidth_budget)
set_tests_properties(mem_bandwidth_budget PROPERTIES LABELS "fast;nightly")

add_executable(test_mem_guard_directed memory/mem_guard_directed.cpp)
target_link_libraries(test_mem_guard_directed PRIVATE zhao_harness zhao_zref)
verilate(test_mem_guard_directed
  PREFIX Vtb_zhao_mem_guard TOP_MODULE tb_zhao_mem_guard
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_MEM_PKGS}
          ${ZHAO_MEM_RTL}/zhao_sdram_ctrl.sv
          ${ZHAO_MEM_RTL}/zhao_vram_arbiter.sv
          ${ZHAO_MEM_RTL}/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_zhao_mem_guard.sv)
add_test(NAME mem_guard_directed COMMAND test_mem_guard_directed)
set_tests_properties(mem_guard_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_hps_bridge_directed memory/hps_bridge_directed.cpp)
target_link_libraries(test_hps_bridge_directed PRIVATE zhao_harness zhao_zref)
verilate(test_hps_bridge_directed
  PREFIX Vtb_zhao_hps_bridge TOP_MODULE tb_zhao_hps_bridge
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
  SOURCES ${ZHAO_MEM_PKGS}
          ${ZHAO_MEM_RTL}/zhao_hps_bridge.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_zhao_hps_bridge.sv)
add_test(NAME mem_hps_bridge_directed COMMAND test_hps_bridge_directed)
set_tests_properties(mem_hps_bridge_directed PROPERTIES LABELS "fast;nightly")

# formal lane (plan 4): one wrapper per SBY property, SKIP-if-absent like
# the W1 formal lane smoke; full set runs under the formal/nightly labels
if(ZHAO_SBY_BIN)
  foreach(sby_prop IN ITEMS mem_vram_arbiter_liveness mem_guard_no_escape mem_sdram_refresh_bound)
    configure_file(formal/mem_formal_lane.cmake.in
                   formal/${sby_prop}_lane.cmake @ONLY)
    add_test(NAME formal_${sby_prop}
             COMMAND ${CMAKE_COMMAND}
                     -DPROP=${sby_prop}
                     -DSRC=${CMAKE_SOURCE_DIR}
                     -P ${CMAKE_CURRENT_BINARY_DIR}/formal/${sby_prop}_lane.cmake)
    set_tests_properties(formal_${sby_prop} PROPERTIES
      LABELS "formal;nightly"
      TIMEOUT 900
      SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
  endforeach()
endif()

