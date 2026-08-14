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
add_executable(test_stub_top unit/test_stub_top.cpp)
target_link_libraries(test_stub_top PRIVATE zhao_harness)
verilate(test_stub_top
  PREFIX Vzhao_stub_top      # default derives from first SOURCE (the package!)
  TOP_MODULE zhao_stub_top
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_frame_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_stub_top.sv)

add_test(NAME stub_top COMMAND test_stub_top)
set_tests_properties(stub_top PROPERTIES LABELS "fast;nightly")

# ---- lint: verilator --lint-only -Wall over the RTL (gate per charter 21) -
find_program(ZHAO_VERILATOR_BIN NAMES verilator_bin verilator_bin.exe)
if(NOT ZHAO_VERILATOR_BIN)
  message(FATAL_ERROR "verilator_bin not found on PATH — source tools/env/zhao-env.ps1|.bat")
endif()
# VERILATOR_ROOT comes from the cache variable resolved by zhao_verilate.cmake
# (env hint first, then derived from the binary location) — NOT the raw env, so
# an env-less reconfigure (ninja regenerate) cannot poison the lint script.
configure_file(lint/lint_stub.cmake.in lint/lint_stub.cmake @ONLY)
add_test(NAME lint_stub_top
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/lint_stub.cmake)
set_tests_properties(lint_stub_top PROPERTIES LABELS "lint;fast")

# ---- formal: SymbiYosys lane smoke (SKIP when sby absent) -----------------
# W1 wires the lane only; real properties land with the blocks that own them.
find_program(ZHAO_SBY_BIN NAMES sby sby.exe)
configure_file(formal/formal_lane.cmake.in formal/formal_lane.cmake @ONLY)
add_test(NAME formal_lane
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/formal/formal_lane.cmake)
set_tests_properties(formal_lane PROPERTIES
  LABELS "formal;nightly"
  SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")

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

