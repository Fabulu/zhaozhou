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

# ---- W2.2: video -------------------------------------------------------------
# Plan W2.2 (VIDEO subsystem: mode/scanout/scaler/framectl). Banner-append
# block (merge rule, plan §3): everything W2.2 adds lands here, at the end.
# Shared driver: tests/video/video_harness.hpp drives the Verilated
# zhao_video_tb (whole subsystem, tests/video/zhao_video_tb.sv) and the
# zref::VideoSys mirror on one gpu-cycle timeline (vid posedges on odd
# steps — the frozen 2:1 phase, plan R1).

set(ZHAO_VIDEO_RTL ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_mode.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_framectl.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scaler.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_fetch.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_linebuf.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_serializer.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scanout.sv)

function(zhao_video_test NAME SRC)
  add_executable(${NAME} ${SRC})
  target_link_libraries(${NAME} PRIVATE zhao_harness zhao_zref)
  target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/video)
  verilate(${NAME}
    PREFIX Vzhao_video_tb
    TOP_MODULE zhao_video_tb
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
                 ${CMAKE_CURRENT_SOURCE_DIR}/video
    SOURCES ${ZHAO_ABI_PKG}
            ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
            ${ZHAO_VIDEO_RTL}
            ${CMAKE_CURRENT_SOURCE_DIR}/video/zhao_video_tb.sv)
  add_test(NAME ${NAME} COMMAND ${NAME})
  set_tests_properties(${NAME} PROPERTIES LABELS "fast;nightly")
endfunction()

zhao_video_test(test_video_mode_directed video/video_mode_directed.cpp)
zhao_video_test(test_video_scanout_directed video/video_scanout_directed.cpp)
zhao_video_test(test_video_framectl_directed video/video_framectl_directed.cpp)
zhao_video_test(test_video_mode_random video/video_mode_random.cpp)
zhao_video_test(test_video_scanout_random video/video_scanout_random.cpp)
zhao_video_test(test_video_framectl_random video/video_framectl_random.cpp)

# nightly --full modes of the same binaries (the --rcp-full pattern of W3)
add_test(NAME video_mode_random_full COMMAND test_video_mode_random --full)
set_tests_properties(video_mode_random_full PROPERTIES LABELS "nightly")
add_test(NAME video_scanout_random_full COMMAND test_video_scanout_random --full)
set_tests_properties(video_scanout_random_full PROPERTIES LABELS "nightly")
add_test(NAME video_framectl_random_full COMMAND test_video_framectl_random --full)
set_tests_properties(video_framectl_random_full PROPERTIES LABELS "nightly")

# scaler: own small top (isolated stream tests)
add_executable(test_video_scaler_directed video/video_scaler_directed.cpp)
target_link_libraries(test_video_scaler_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_video_scaler_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/video)
verilate(test_video_scaler_directed
  PREFIX Vzhao_scaler_tb
  TOP_MODULE zhao_scaler_tb
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_CURRENT_SOURCE_DIR}/video
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scaler.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/video/zhao_scaler_tb.sv)
add_test(NAME video_scaler_directed COMMAND test_video_scaler_directed)
set_tests_properties(video_scaler_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_video_scaler_random video/video_scaler_random.cpp)
target_link_libraries(test_video_scaler_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_video_scaler_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/video)
verilate(test_video_scaler_random
  PREFIX Vzhao_scaler_tb
  TOP_MODULE zhao_scaler_tb
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_CURRENT_SOURCE_DIR}/video
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scaler.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/video/zhao_scaler_tb.sv)
add_test(NAME video_scaler_random COMMAND test_video_scaler_random)
set_tests_properties(video_scaler_random PROPERTIES LABELS "fast;nightly")
add_test(NAME video_scaler_random_full COMMAND test_video_scaler_random --full)
set_tests_properties(video_scaler_random_full PROPERTIES LABELS "nightly")

# per-module lint (charter 21 step 8, pattern of lint_zhao_pkg)
foreach(zhao_video_mod IN ITEMS
     zhao_video_mode zhao_video_framectl zhao_video_scaler
     zhao_scanout_fetch zhao_scanout_linebuf zhao_scanout_serializer)
  add_test(NAME lint_${zhao_video_mod}
           COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                   --top-module ${zhao_video_mod}
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/video/${zhao_video_mod}.sv)
  set_tests_properties(lint_${zhao_video_mod} PROPERTIES
    LABELS "lint;fast"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()

# formal: the three W2.2 VIDEO properties as SKIP-if-absent wrappers
# (the formal_lane pattern); full set also nightly
foreach(zhao_sby IN ITEMS video_mode_timing video_scanout_linebuf
                          video_framectl_one_fence)
  set(SBY_NAME ${zhao_sby})
  set(SBY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/formal/${zhao_sby}.sby)
  set(SBY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/formal)
  set(SBY_WORK ${CMAKE_CURRENT_BINARY_DIR}/formal/${zhao_sby}_work)
  configure_file(formal/video_formal.cmake.in
                 formal/${zhao_sby}_run.cmake @ONLY)
  add_test(NAME formal_${zhao_sby}
           COMMAND ${CMAKE_COMMAND} -P
                   ${CMAKE_CURRENT_BINARY_DIR}/formal/${zhao_sby}_run.cmake)
  set_tests_properties(formal_${zhao_sby} PROPERTIES
    LABELS "formal;nightly"
    SKIP_REGULAR_EXPRESSION "formal SKIPPED")
endforeach()

# whole-subsystem lint cone (top + submodules + the tb wrapper)
add_test(NAME lint_zhao_video_scanout
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_video_tb
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${ZHAO_VIDEO_RTL}
                 ${CMAKE_CURRENT_SOURCE_DIR}/video/zhao_video_tb.sv)
set_tests_properties(lint_zhao_video_scanout PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
