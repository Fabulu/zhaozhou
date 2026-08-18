# Tests. CTest labels (charter 20.3, P1 findings 4):
#   fast    — every commit (the default gate: ctest --test-dir build -L fast)
#   nightly — full fuzz + capture replay (scheduled CI job)
#   formal  — SymbiYosys lane; wrapper test SKIPs when sby is absent.
#             Since review M1 the smoke (sby --version) also carries `fast`:
#             sby IS in CI's oss-cad-suite, and the smoke costs < 1 s.
#   lint    — per-module verilator --lint-only -Wall (charter 21 step 8) +
#             the charter-27 format / static-analysis tiers (review M1)

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
  LABELS "formal;fast;nightly"
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

# Animation stability (PLAN deformation-reel Tier 1): renders every reel
# subject through zref::render and fails on any sequence-CRC drift. Drift,
# popping and frame-to-frame instability are invisible in a single golden
# frame and obvious across 240; this is the cheap lane that catches them.
add_test(NAME reel_sequence_crc COMMAND zhao-reel --check)
set_tests_properties(reel_sequence_crc PROPERTIES LABELS "fast;nightly")

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

# ---- W6: ZRef shell (spec/capture_format.md 3.2-3.3; session layer) --------
# Golden frame replays green; corrupt variants return the exact fail-safe
# error codes; implemented-but-semantic-free commands count as no-ops.
add_executable(test_zref_shell unit/test_zref_shell.cpp)
target_link_libraries(test_zref_shell PRIVATE zhao_zref)
add_test(NAME zref_shell COMMAND test_zref_shell)
set_tests_properties(zref_shell PROPERTIES LABELS "fast;nightly")

# ---- W6: product stub smoke runs (ZEmu + desktop runtime link ZRef and ------
# ---- replay the golden frame through the shell; exit 0 = green) -------------
add_test(NAME zemu_smoke
         COMMAND zemu ${CMAKE_SOURCE_DIR}/tests/abi/golden/frame_minimal.bin)
set_tests_properties(zemu_smoke PROPERTIES LABELS "fast;nightly")

add_test(NAME desktop_smoke
         COMMAND zhao-desktop ${CMAKE_SOURCE_DIR}/tests/abi/golden/frame_minimal.bin)
set_tests_properties(desktop_smoke PROPERTIES LABELS "fast;nightly")

# ---- W5: field IR (spec/form/field-ir.md; gate (d)) -------------------------
# The differential test compiles the TS-generated wrapper
# (compiler/tests/generated/crater_ring.hpp), owns the golden .zvec from the
# C++ oracle (captures/golden/field/), runs the 8 scripted checks of
# field-ir.md §12 incl. the minimize demo artifact (captures/failures/field/).
add_executable(test_field_crater_ring differential/test_field_crater_ring.cpp)
target_link_libraries(test_field_crater_ring PRIVATE zhao_zref)
target_include_directories(test_field_crater_ring PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
target_compile_definitions(test_field_crater_ring PRIVATE
  ZHAO_CAPTURE_DIR="${CMAKE_SOURCE_DIR}/captures")
add_test(NAME field_crater_ring COMMAND test_field_crater_ring)
set_tests_properties(field_crater_ring PROPERTIES LABELS "fast;nightly")

# fuzz corpus replay: TS-vs-C++ interpretation parity over the committed
# corpus (tests/fuzz/corpus/field, generated + committed by
# tests/fuzz/field_corpus_gen.ts). Nightly label per plan W5.
add_executable(test_field_fuzz_parity fuzz/test_field_fuzz_parity.cpp)
target_link_libraries(test_field_fuzz_parity PRIVATE zhao_zref)
target_compile_definitions(test_field_fuzz_parity PRIVATE
  ZHAO_FIELD_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/fuzz/corpus/field")
add_test(NAME field_fuzz_parity COMMAND test_field_fuzz_parity)
set_tests_properties(field_fuzz_parity PROPERTIES LABELS "nightly")

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

# ---- M1 (review RUN-20260814-1912): charter-27 format + static-analysis ----
# Every-commit tiers, mirrored locally as SKIP-if-absent CTest shims (same
# discipline as formal_lane): clang-format is not shipped in oss-cad-suite,
# so machines without it SKIP and the dedicated CI "format" job carries the
# authoritative gate (pinned npm clang-format = LLVM 15; see .clang-format).
# cppcheck is present in this repo's MinGW toolchain and on CI runners.
find_program(ZHAO_CLANG_FORMAT_BIN NAMES clang-format clang-format.exe)
configure_file(lint/format_check.cmake.in lint/format_check.cmake @ONLY)
add_test(NAME format_check
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/format_check.cmake)
set_tests_properties(format_check PROPERTIES
  LABELS "lint;fast"
  SKIP_REGULAR_EXPRESSION "format tier SKIPPED")

find_program(ZHAO_CPPCHECK_BIN NAMES cppcheck cppcheck.exe)
configure_file(lint/cppcheck_check.cmake.in lint/cppcheck_check.cmake @ONLY)
add_test(NAME cppcheck_check
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/cppcheck_check.cmake)
set_tests_properties(cppcheck_check PROPERTIES
  LABELS "lint;fast"
  SKIP_REGULAR_EXPRESSION "static analysis tier SKIPPED")
# ---- W2.3: input ------------------------------------------------------------
# INPUT.SNAPSHOT + INPUT.RUMBLE (spec/input_rules.md). ZRef oracles are
# header-only (reference/include/zref/zref_input.hpp via zhao_zref's public
# include dir); the two RTL modules verilate against the frozen zhao_pkg +
# generated zhao_abi_pkg. Random differential registers two modes of the
# same binary: 1k frames (fast) and the 100k soak (nightly).
set(ZHAO_ZHAO_PKG ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv)
set(ZHAO_INPUT_SNAPSHOT_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/input/zhao_input_snapshot.sv)
set(ZHAO_INPUT_RUMBLE_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/input/zhao_input_rumble.sv)
set(ZHAO_INPUT_INCLUDES ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                        ${CMAKE_SOURCE_DIR}/fpga/rtl/generated)

add_executable(test_input_snapshot_directed input/input_snapshot_directed.cpp)
target_link_libraries(test_input_snapshot_directed PRIVATE zhao_harness zhao_zref)
verilate(test_input_snapshot_directed
  PREFIX Vzhao_input_snapshot
  TOP_MODULE zhao_input_snapshot
  INCLUDE_DIRS ${ZHAO_INPUT_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_SNAPSHOT_SV})
add_test(NAME input_snapshot_directed COMMAND test_input_snapshot_directed)
set_tests_properties(input_snapshot_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_input_rumble_directed input/input_rumble_directed.cpp)
target_link_libraries(test_input_rumble_directed PRIVATE zhao_harness zhao_zref)
verilate(test_input_rumble_directed
  PREFIX Vzhao_input_rumble
  TOP_MODULE zhao_input_rumble
  INCLUDE_DIRS ${ZHAO_INPUT_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_RUMBLE_SV})
add_test(NAME input_rumble_directed COMMAND test_input_rumble_directed)
set_tests_properties(input_rumble_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_input_random input/input_random.cpp)
target_link_libraries(test_input_random PRIVATE zhao_harness zhao_zref)
verilate(test_input_random
  PREFIX Vzhao_input_snapshot
  TOP_MODULE zhao_input_snapshot
  INCLUDE_DIRS ${ZHAO_INPUT_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_SNAPSHOT_SV})
verilate(test_input_random
  PREFIX Vzhao_input_rumble
  TOP_MODULE zhao_input_rumble
  INCLUDE_DIRS ${ZHAO_INPUT_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_RUMBLE_SV})
add_test(NAME input_random COMMAND test_input_random)
set_tests_properties(input_random PROPERTIES LABELS "fast;nightly")
add_test(NAME input_random_soak COMMAND test_input_random --frames 100000)
set_tests_properties(input_random_soak PROPERTIES LABELS "nightly")

add_test(NAME lint_input_snapshot
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_input_snapshot
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_SNAPSHOT_SV})
set_tests_properties(lint_input_snapshot PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_input_rumble
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_input_rumble
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_RUMBLE_SV})
set_tests_properties(lint_input_rumble PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: INPUT.SNAPSHOT atomicity + sequence-exactly-once (SKIP when sby
# absent — same lane pattern as formal_lane)
configure_file(formal/input_snapshot_atomic.cmake.in
               formal/input_snapshot_atomic.cmake @ONLY)
add_test(NAME input_snapshot_atomic
         COMMAND ${CMAKE_COMMAND} -P
                 ${CMAKE_CURRENT_BINARY_DIR}/formal/input_snapshot_atomic.cmake)
set_tests_properties(input_snapshot_atomic PROPERTIES
  LABELS "formal;nightly"
  RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                   # provers blew the 1800 s wrapper budget on runs that pass
                   # solo � wall time must approximate solo time (2026-08-16)
  SKIP_REGULAR_EXPRESSION "formal SKIPPED")
# ---- W2.4: audio (plan W2.4; spec/audio_rules.md, contracts AUDIO.FIFO ----
# ---- + SW.MIXER) ------------------------------------------------------------
# Tone subset (SW.MIXER): golden-vector directed + PCG tone-switching random,
# both pure ZRef (RTL never generates the tone — mixer-side only).
add_executable(test_mixer_tone_directed audio/mixer_tone_directed.cpp)
target_link_libraries(test_mixer_tone_directed PRIVATE zhao_zref)
target_compile_definitions(test_mixer_tone_directed PRIVATE
  ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden/fixp")
add_test(NAME mixer_tone_directed COMMAND test_mixer_tone_directed)
set_tests_properties(mixer_tone_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_mixer_tone_random audio/mixer_tone_random.cpp)
target_link_libraries(test_mixer_tone_random PRIVATE zhao_zref)
target_compile_definitions(test_mixer_tone_random PRIVATE
  ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden/fixp")
add_test(NAME mixer_tone_random COMMAND test_mixer_tone_random)
set_tests_properties(mixer_tone_random PROPERTIES LABELS "fast;nightly")

# AUDIO.FIFO: directed D4 scenarios + PCG-jittered differential vs the
# cycle-accurate oracle (tests/audio/audio_dev.hpp drives the Verilated
# module and zref::AudioFifo through the identical dual-clock protocol).
add_executable(test_audio_fifo_directed audio/audio_fifo_directed.cpp)
target_link_libraries(test_audio_fifo_directed PRIVATE zhao_zref zhao_harness)
verilate(test_audio_fifo_directed
  PREFIX Vzhao_audio_fifo
  TOP_MODULE zhao_audio_fifo
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/audio/zhao_audio_fifo.sv)
add_test(NAME audio_fifo_directed COMMAND test_audio_fifo_directed)
set_tests_properties(audio_fifo_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_audio_fifo_random audio/audio_fifo_random.cpp)
target_link_libraries(test_audio_fifo_random PRIVATE zhao_zref zhao_harness)
verilate(test_audio_fifo_random
  PREFIX Vzhao_audio_fifo
  TOP_MODULE zhao_audio_fifo
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
  SOURCES ${ZHAO_ABI_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/audio/zhao_audio_fifo.sv)
add_test(NAME audio_fifo_random COMMAND test_audio_fifo_random)
set_tests_properties(audio_fifo_random PROPERTIES LABELS "fast;nightly")
# 100k-schedule nightly mode of the same binary (1k fast above)
add_test(NAME audio_fifo_random_nightly COMMAND test_audio_fifo_random --nightly)
set_tests_properties(audio_fifo_random_nightly PROPERTIES LABELS "nightly")

# per-module lint (charter 21 step 8)
add_test(NAME lint_zhao_audio_fifo
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_audio_fifo
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/audio/zhao_audio_fifo.sv)
set_tests_properties(lint_zhao_audio_fifo PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: audio_fifo_bounds (plan §4) — SKIP when sby absent. The wrapper
# stages the mechanical formal copy of zhao_pkg.sv (see audio_formal_lane)
# and runs the committed property file.
configure_file(formal/audio_formal_lane.cmake.in formal/audio_formal_lane.cmake @ONLY)
add_test(NAME formal_audio_fifo
         COMMAND ${CMAKE_COMMAND} -P
                 ${CMAKE_CURRENT_BINARY_DIR}/formal/audio_formal_lane.cmake)
set_tests_properties(formal_audio_fifo PROPERTIES
  LABELS "formal;nightly"
  RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                   # provers blew the 1800 s wrapper budget on runs that pass
                   # solo � wall time must approximate solo time (2026-08-16)
  SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
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

# MEM.HPS.BRIDGE randomized differential vs zref::HpsBridge (charter §21
# step 5). Previously the block's `tests.random` was an ALIAS of the directed
# file and the cited mem_random.cpp never instantiated the bridge — the
# randomized evidence for its RTL_VERIFIED claim did not exist.
add_executable(test_hps_bridge_random memory/hps_bridge_random.cpp)
target_link_libraries(test_hps_bridge_random PRIVATE zhao_harness zhao_zref)
verilate(test_hps_bridge_random
  PREFIX Vtb_zhao_hps_bridge TOP_MODULE tb_zhao_hps_bridge
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
  SOURCES ${ZHAO_MEM_PKGS}
          ${ZHAO_MEM_RTL}/zhao_hps_bridge.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_zhao_hps_bridge.sv)
add_test(NAME mem_hps_bridge_random COMMAND test_hps_bridge_random)
set_tests_properties(mem_hps_bridge_random PROPERTIES LABELS "fast;nightly")
add_test(NAME mem_hps_bridge_random_long COMMAND test_hps_bridge_random --long)
set_tests_properties(mem_hps_bridge_random_long PROPERTIES LABELS "nightly" TIMEOUT 900)

# formal lane (plan 4): one wrapper per SBY property, SKIP-if-absent like
# the W1 formal lane smoke; full set runs under the formal/nightly labels
if(ZHAO_SBY_BIN)
  # Formal wrappers: all paths baked in at configure time (script-mode cmake
  # has no reliable CMAKE_CURRENT_BINARY_DIR; the sby work root is kept
  # SEPARATE from the lane scripts). The -P script path itself is normalized
  # to backslashes for mingw ctest (same machine rule as the lint lane).
  set(ZHAO_MEM_FORMAL_WORK "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  # mem_sdram_refresh_bound is NOT in this list. It is a depth-900 BMC (the
  # refresh bound itself is ~862 cycles, so the depth cannot be meaningfully
  # reduced), and it had never actually been elaborated before — under the old
  # SKIP it "ran" instantly. With a genuinely free environment it does not
  # finish in any CI-shaped budget (>50 min and still going when measured), so
  # putting it in the routine lane would only manufacture timeouts that look
  # like failures. Measured: its `cover` task PASSES in ~11 min (all three
  # covers reached, so the bound is not vacuous), while the `bmc` task was
  # still running after 50 min — NOT ESTABLISHED, which is an unfinished
  # proof, not a failing one. Kept runnable by hand:
  #     cd tests/formal && sby -f --prefix <dir> mem_sdram_refresh_bound.sby
  # See the wave-3 TASK_LOG.
  # MEM.SDRAM is SPECIFIED / blocked_on: hardware, so no maturity claim rests
  # on it today.
  foreach(sby_prop IN ITEMS mem_vram_arbiter_liveness mem_guard_no_escape)
    set(PROP "${sby_prop}")
    set(FORMAL_WORK_ROOT "${ZHAO_MEM_FORMAL_WORK}")
    set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
    configure_file(formal/mem_formal_lane.cmake.in
                   formal/${sby_prop}_lane.cmake @ONLY)
    set(zhao_formal_script "${CMAKE_CURRENT_BINARY_DIR}/formal/${sby_prop}_lane.cmake")
    string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
    add_test(NAME formal_${sby_prop}
             COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
    # TIMEOUT 2100 (was 900). mem_vram_arbiter_liveness now carries FOUR tasks
    # — bmc plus the two `expect fail` tightness tasks that prove the bound is
    # exact, plus cover — and measures 9m31s wall with all four in parallel on
    # the reference machine. 900s left ~35% headroom, which is how a proof lane
    # starts producing timeouts that get read as failures (or, worse, get
    # "fixed" by deleting tasks). The wrapper's own TIMEOUT is 1800 so it fails
    # with a readable sby message before CTest kills it from outside.
    set_tests_properties(formal_${sby_prop} PROPERTIES
      LABELS "formal;nightly"
      RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                       # provers blew the 1800 s wrapper budget on runs that pass
                       # solo � wall time must approximate solo time (2026-08-16)
      TIMEOUT 2100
      SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
  endforeach()
endif()

# ---- W3.5: render (plan W3.5/D7; library: reference/src/zrender + zsky) ----
# Directed per-command tests with hand-computed fixed-point projections,
# the naive-oracle heightfield differential, the sky emitter laws, the
# committed 2-view golden canvas CRC, and the 600-frame runtime budget
# (fast label: the whole set runs in seconds on the reference machine).
add_executable(test_render_directed render/render_directed.cpp)
target_link_libraries(test_render_directed PRIVATE zhao_zref)
target_include_directories(test_render_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
add_test(NAME render_directed COMMAND test_render_directed)
set_tests_properties(render_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_render_heightfield render/render_heightfield.cpp)
target_link_libraries(test_render_heightfield PRIVATE zhao_zref)
target_include_directories(test_render_heightfield PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render)
add_test(NAME render_heightfield COMMAND test_render_heightfield)
set_tests_properties(render_heightfield PROPERTIES LABELS "fast;nightly")

add_executable(test_render_sky render/render_sky.cpp)
target_link_libraries(test_render_sky PRIVATE zhao_zref)
target_include_directories(test_render_sky PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render)
add_test(NAME render_sky COMMAND test_render_sky)
set_tests_properties(render_sky PROPERTIES LABELS "fast;nightly")

# stars_and_flares.md §13 anchors: hand-computed ramp/boil/corona/ghost/
# fade/LOD/glint values, the starfield harness-equivalence gate (byte-exact
# vs tests/golden/starfield/oracle.bin BEFORE any rendering test), the
# celestial_state round-trip, the compositor determinism + §5 texel budget,
# and the CRC-locked 12-class gamut sheet.
add_executable(test_render_star render/render_star.cpp)
target_link_libraries(test_render_star PRIVATE zhao_zref)
target_compile_definitions(test_render_star
  PRIVATE ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden/starfield")
add_test(NAME render_star COMMAND test_render_star)
set_tests_properties(render_star PROPERTIES LABELS "fast;nightly")

add_executable(test_render_golden render/render_golden.cpp)
target_link_libraries(test_render_golden PRIVATE zhao_zref)
target_include_directories(test_render_golden PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)  # crater_ring wrapper
add_test(NAME render_golden COMMAND test_render_golden)
set_tests_properties(render_golden PROPERTIES LABELS "fast;nightly")

# ---- world-identity wave: dual-heightfield terrain (spec/terrain_rules.md) --
# column_query directed + the physics-equals-pixels differential (§10.1),
# breach/heal/no_bake law (§10.2 at reference scope), the §9.2 bake deferral
# identity, and the renderer's breach/rim semantics with a legacy control.
add_executable(test_terrain_dual terrain/terrain_dual.cpp)
target_link_libraries(test_terrain_dual PRIVATE zhao_zref)
target_include_directories(test_terrain_dual PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
add_test(NAME terrain_dual COMMAND test_terrain_dual)
set_tests_properties(terrain_dual PROPERTIES LABELS "fast;nightly")

# ---- deep-keel wave: the 3.7 keel default (spec/terrain_rules.md) -----------
# R from SOLID cells, the 50 m donor floor, R/2, the height16 headroom cap,
# hand-computed bottom anchors, the deliberate-shallow override, and the
# breach-needs-depth integration.
add_executable(test_terrain_keel terrain/terrain_keel.cpp)
target_link_libraries(test_terrain_keel PRIVATE zhao_zref)
target_include_directories(test_terrain_keel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)
add_test(NAME terrain_keel COMMAND test_terrain_keel)
set_tests_properties(terrain_keel PROPERTIES LABELS "fast;nightly")

# ---- deep-keel wave: FORGE.CLIFF reference (terrain_rules.md 5) -------------
# rim enumeration anchors, the checkerboard structural clamp (2,048 -> 512,
# the TIGHT worst case), near-camera priority, and merge-under-pressure.
add_executable(test_forge_cliff_directed forge/forge_cliff_directed.cpp)
target_link_libraries(test_forge_cliff_directed PRIVATE zhao_zref)
add_test(NAME forge_cliff_directed COMMAND test_forge_cliff_directed)
set_tests_properties(forge_cliff_directed PROPERTIES LABELS "fast;nightly")

# randomized rim differential: 300 random void masks vs an independent
# enumeration oracle + the emitted+dropped==enumerated body identity
add_executable(test_forge_cliff_random forge/forge_cliff_random.cpp)
target_link_libraries(test_forge_cliff_random PRIVATE zhao_zref)
add_test(NAME forge_cliff_random COMMAND test_forge_cliff_random)
set_tests_properties(forge_cliff_random PROPERTIES LABELS "fast;nightly")

# ---- deep-keel wave: TEXTURE.MOSAIC reference (terrain_rules.md 6.2) --------
# the mirrored-repeat fold at wrap boundaries, the frozen pick constants,
# and the rendered dither + exact modulation through the replicated resolve.
add_executable(test_texture_mosaic_directed texture/texture_mosaic_directed.cpp)
target_link_libraries(test_texture_mosaic_directed PRIVATE zhao_zref)
target_include_directories(test_texture_mosaic_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)
add_test(NAME texture_mosaic_directed COMMAND test_texture_mosaic_directed)
set_tests_properties(texture_mosaic_directed PROPERTIES LABELS "fast;nightly")

# randomized pick/fold differential: 100k samples vs the wrapped-u64 oracle
add_executable(test_texture_mosaic_random texture/texture_mosaic_random.cpp)
target_link_libraries(test_texture_mosaic_random PRIVATE zhao_zref)
add_test(NAME texture_mosaic_random COMMAND test_texture_mosaic_random)
set_tests_properties(texture_mosaic_random PROPERTIES LABELS "fast;nightly")

# ---- world-identity wave: creature/character reference lane -------------------
# (spec/creature_rules.md 1-4; the ZREF oracle for the future Phase 8-9 GEOM
# blocks — confers no block promotion: GEOM.POSE stays SPECIFIED until the
# qformats quat-lane amendment freezes the decode numerics).
# Hand-computed anchors: ring stitch counts, quat lane exactness, the 3->2
# clamp gate's analytic bound, pose-cache eviction law, LOD hysteresis.
add_executable(test_creature_core geometry/creature_core.cpp)
target_link_libraries(test_creature_core PRIVATE zhao_zref)
target_include_directories(test_creature_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
add_test(NAME creature_core COMMAND test_creature_core)
set_tests_properties(creature_core PROPERTIES LABELS "fast;nightly")

add_executable(test_render_budget render/render_budget.cpp)
target_link_libraries(test_render_budget PRIVATE zhao_zref)
target_include_directories(test_render_budget PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
add_test(NAME render_budget COMMAND test_render_budget)
set_tests_properties(render_budget PROPERTIES LABELS "fast;nightly")
# ---- W2.6: command + debug (plan W2.6; spec/counters.md D9, contracts ------
# ---- CMD.SCHEDULER / CMD.DMA / DEBUG.COUNTERS / DEBUG.CRC) ------------------
# CMD.SCHEDULER: directed + PCG differential vs zref::CmdScheduler (the
# oracle and the Verilated module share one cycle() driver, cmd_sim.hpp).
set(ZHAO_CMD_INCLUDES ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                      ${CMAKE_SOURCE_DIR}/fpga/rtl/generated)
set(ZHAO_CMD_SCHEDULER_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_scheduler.sv)
set(ZHAO_CMD_DMA_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_dma.sv)
set(ZHAO_DEBUG_COUNTERS_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_counters.sv)
set(ZHAO_DEBUG_CRC_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_crc.sv)

add_executable(test_cmd_scheduler_directed command/cmd_scheduler_directed.cpp)
target_link_libraries(test_cmd_scheduler_directed PRIVATE zhao_harness zhao_zref)
verilate(test_cmd_scheduler_directed
  PREFIX Vzhao_cmd_scheduler
  TOP_MODULE zhao_cmd_scheduler
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_SCHEDULER_SV})
add_test(NAME cmd_scheduler_directed COMMAND test_cmd_scheduler_directed)
set_tests_properties(cmd_scheduler_directed PROPERTIES LABELS "fast;nightly")

# CMD.DMA: directed vectors with the harness-as-HPS bridge responder (D10)
add_executable(test_cmd_dma_directed command/cmd_dma_directed.cpp)
target_link_libraries(test_cmd_dma_directed PRIVATE zhao_harness zhao_zref)
verilate(test_cmd_dma_directed
  PREFIX Vzhao_cmd_dma
  TOP_MODULE zhao_cmd_dma
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_DMA_SV})
add_test(NAME cmd_dma_directed COMMAND test_cmd_dma_directed)
set_tests_properties(cmd_dma_directed PROPERTIES LABELS "fast;nightly")
# random lane of the same binary: PCG packet timelines with predicted
# verdicts (fail-safe order) + the CRC gate on every error path
add_test(NAME cmd_dma_random COMMAND test_cmd_dma_directed --random 400)
set_tests_properties(cmd_dma_random PROPERTIES LABELS "fast;nightly")
add_test(NAME cmd_dma_random_nightly COMMAND test_cmd_dma_directed --random 5000)
set_tests_properties(cmd_dma_random_nightly PROPERTIES LABELS "nightly")

# PCG claim/seal timelines: 1k fast / 100k nightly soak (same binary)
add_executable(test_cmd_random command/cmd_random.cpp)
target_link_libraries(test_cmd_random PRIVATE zhao_harness zhao_zref)
verilate(test_cmd_random
  PREFIX Vzhao_cmd_scheduler
  TOP_MODULE zhao_cmd_scheduler
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_SCHEDULER_SV})
add_test(NAME cmd_random COMMAND test_cmd_random)
set_tests_properties(cmd_random PROPERTIES LABELS "fast;nightly")
add_test(NAME cmd_random_soak COMMAND test_cmd_random --frames 100000)
set_tests_properties(cmd_random_soak PROPERTIES LABELS "nightly")

# DEBUG.COUNTERS: D9 snapshot protocol + read window vs zref::DebugCounters
add_executable(test_debug_counters_directed debug/debug_counters_directed.cpp)
target_link_libraries(test_debug_counters_directed PRIVATE zhao_harness zhao_zref)
verilate(test_debug_counters_directed
  PREFIX Vzhao_debug_counters
  TOP_MODULE zhao_debug_counters
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_DEBUG_COUNTERS_SV})
add_test(NAME debug_counters_directed COMMAND test_debug_counters_directed)
set_tests_properties(debug_counters_directed PROPERTIES LABELS "fast;nightly")
add_test(NAME debug_counters_random COMMAND test_debug_counters_directed --random 200)
set_tests_properties(debug_counters_random PROPERTIES LABELS "fast;nightly")
add_test(NAME debug_counters_random_nightly COMMAND test_debug_counters_directed --random 2000)
set_tests_properties(debug_counters_random_nightly PROPERTIES LABELS "nightly")

# DEBUG.CRC: known vectors + full-canvas modes vs the ABI CRC machine
add_executable(test_debug_crc_directed debug/debug_crc_directed.cpp)
target_link_libraries(test_debug_crc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_debug_crc_directed
  PREFIX Vzhao_debug_crc
  TOP_MODULE zhao_debug_crc
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_DEBUG_CRC_SV})
add_test(NAME debug_crc_directed COMMAND test_debug_crc_directed)
set_tests_properties(debug_crc_directed PROPERTIES LABELS "fast;nightly")
add_test(NAME debug_crc_random COMMAND test_debug_crc_directed --random 300)
set_tests_properties(debug_crc_random PROPERTIES LABELS "fast;nightly")
add_test(NAME debug_crc_random_nightly COMMAND test_debug_crc_directed --random 3000)
set_tests_properties(debug_crc_random_nightly PROPERTIES LABELS "nightly")

# per-module lint (charter 21 step 8)
add_test(NAME lint_cmd_scheduler
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_cmd_scheduler
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_SCHEDULER_SV})
set_tests_properties(lint_cmd_scheduler PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_cmd_dma
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_cmd_dma
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_DMA_SV})
set_tests_properties(lint_cmd_dma PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_debug_counters
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_debug_counters
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_DEBUG_COUNTERS_SV})
set_tests_properties(lint_debug_counters PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_debug_crc
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_debug_crc
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_DEBUG_CRC_SV})
set_tests_properties(lint_debug_crc PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: slot FSM + DMA CRC gate (plan 4) — SKIP only when sby (or the
# slang plugin) is absent; anything else is FATAL. Both properties use the
# read_slang frontend against the REAL packages (the W2.3 lane choice; no
# ABI stub, no mechanical copies — see the .sby headers). Both carry TWO
# tasks (bmc AND cover): every assertion in zhao_cmd_dma is an implication
# on pkt_valid_o / guard_req_o.valid, and the scheduler's fence property is
# guarded by a RUN->DONE transition — without the cover task each bmc could
# pass with those preconditions unreachable (the MEM.GUARD vacuity shape).
# Reuses the hardened mem_formal_lane wrapper (narrow SKIP, --prefix for
# multi-task) rather than a parallel cmd-specific template.
if(ZHAO_SBY_BIN)
  foreach(sby_prop IN ITEMS cmd_scheduler_slot_fsm cmd_dma_crc_gate)
    set(PROP "${sby_prop}")
    set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
    set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
    configure_file(formal/mem_formal_lane.cmake.in
                   formal/${sby_prop}_lane.cmake @ONLY)
    set(zhao_formal_script "${CMAKE_CURRENT_BINARY_DIR}/formal/${sby_prop}_lane.cmake")
    string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
    add_test(NAME formal_${sby_prop}
             COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
    set_tests_properties(formal_${sby_prop} PROPERTIES
      LABELS "formal;nightly"
      RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                       # provers blew the 1800 s wrapper budget on runs that pass
                       # solo � wall time must approximate solo time (2026-08-16)
      TIMEOUT 2100
      SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
  endforeach()
endif()

# ---- W2.2: video -------------------------------------------------------------
# Plan W2.2 (VIDEO subsystem: mode/scanout/scaler/framectl). Banner-append
# block (merge rule, plan §3): everything W2.2 adds lands here, at the end.
# Shared driver: tests/video/video_harness.hpp drives the Verilated
# zhao_video_tb (whole subsystem, tests/video/zhao_video_tb.sv) and the
# zref::Scanout mirror on one gpu-cycle timeline (vid posedges on odd
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

# formal: the three W2.2 VIDEO properties. MERGE RESOLUTION NOTE: the
# branch shipped its own wrapper (tests/formal/video_formal.cmake.in) with
# no PATH hardening (the 0xC0000139 DLL-shadowing trap) and a broad
# fail-on-rc contract; it was DROPPED in favour of the shared hardened
# mem_formal_lane.cmake.in (narrow SKIP only when the slang frontend is
# absent, --prefix for the multi-task bmc+cover pairs V16 requires) — the
# same resolution the W2.6 merge made for the CMD wrappers.
if(ZHAO_SBY_BIN)
  foreach(sby_prop IN ITEMS video_mode_timing video_scanout_linebuf
                            video_framectl_one_fence)
    set(PROP "${sby_prop}")
    set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
    set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
    configure_file(formal/mem_formal_lane.cmake.in
                   formal/${sby_prop}_lane.cmake @ONLY)
    set(zhao_formal_script "${CMAKE_CURRENT_BINARY_DIR}/formal/${sby_prop}_lane.cmake")
    string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
    add_test(NAME formal_${sby_prop}
             COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
    set_tests_properties(formal_${sby_prop} PROPERTIES
      LABELS "formal;nightly"
      RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                       # provers blew the 1800 s wrapper budget on runs that pass
                       # solo � wall time must approximate solo time (2026-08-16)
      TIMEOUT 2100
      SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
  endforeach()
endif()

# ---- Phase 4: RASTER.EDGEWALK (charter phase 4, ZH-022/ZH-023; law -------
# ---- spec/qformats.md §8; contract design/contracts/RASTER.EDGEWALK.md) ----
# The oracle is zref::EdgeWalk, which is itself a view onto the FROZEN
# software raster (reference/src/zrender/rast.cpp), so "RTL == oracle" is
# literally "RTL == the §8 fill law" — no re-derived fill rule anywhere in
# the test.
set(ZHAO_RASTER_EDGEWALK_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv)

add_executable(test_raster_edgewalk_directed raster/raster_edgewalk_directed.cpp)
target_link_libraries(test_raster_edgewalk_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_edgewalk_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_edgewalk_directed
  PREFIX Vzhao_raster_edgewalk
  TOP_MODULE zhao_raster_edgewalk
  SOURCES ${ZHAO_RASTER_EDGEWALK_SV})
add_test(NAME raster_edgewalk_directed COMMAND test_raster_edgewalk_directed)
set_tests_properties(raster_edgewalk_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_edgewalk_random raster/raster_edgewalk_random.cpp)
target_link_libraries(test_raster_edgewalk_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_edgewalk_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_edgewalk_random
  PREFIX Vzhao_raster_edgewalk
  TOP_MODULE zhao_raster_edgewalk
  SOURCES ${ZHAO_RASTER_EDGEWALK_SV})
add_test(NAME raster_edgewalk_random COMMAND test_raster_edgewalk_random)
set_tests_properties(raster_edgewalk_random PROPERTIES LABELS "fast;nightly")
# 60k-tile soak of the same binary (4k fast above)
add_test(NAME raster_edgewalk_random_nightly COMMAND test_raster_edgewalk_random --nightly)
set_tests_properties(raster_edgewalk_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter §21 step 8)
add_test(NAME lint_raster_edgewalk
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_edgewalk
                 ${ZHAO_RASTER_EDGEWALK_SV})
set_tests_properties(lint_raster_edgewalk PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the §8 top-left fill rule (charter §20.4 — "adjacent-triangle
# exactly-once coverage is a SymbiYosys formal property"). Proved on
# zhao_raster_fill, the module the tile walker instantiates 48× per row, so
# the proof and the silicon are the same bytes. TWO tasks (bmc AND cover):
# the covers pin the on-the-edge and sub-unit corners as reachable. Reuses
# the hardened mem_formal_lane wrapper (narrow SKIP, --prefix for multi-task).
if(ZHAO_SBY_BIN)
  set(PROP "raster_edgewalk_top_left")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/raster_edgewalk_top_left_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/raster_edgewalk_top_left_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_raster_edgewalk_top_left
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_raster_edgewalk_top_left PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---- Phase 4: RASTER.TILESTORE (charter phase 4, ZH-021; law charter 8 ------
# ---- "Active tile storage"; contract design/contracts/RASTER.TILESTORE.md) --
# The oracle is zref::TileStore. A memory has no pre-existing law to call, so
# the oracle is a deliberately plain second implementation of the CONTRACT
# (two arrays and a present bit) — what the differential tests exercise is the
# RTL's ping-pong, O(1) clear and write-through-read bypass mechanics.
set(ZHAO_RASTER_TILESTORE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv)

add_executable(test_raster_tilestore_directed raster/raster_tilestore_directed.cpp)
target_link_libraries(test_raster_tilestore_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_tilestore_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_tilestore_directed
  PREFIX Vzhao_raster_tilestore
  TOP_MODULE zhao_raster_tilestore
  SOURCES ${ZHAO_RASTER_TILESTORE_SV})
add_test(NAME raster_tilestore_directed COMMAND test_raster_tilestore_directed)
set_tests_properties(raster_tilestore_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_tilestore_random raster/raster_tilestore_random.cpp)
target_link_libraries(test_raster_tilestore_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_tilestore_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_tilestore_random
  PREFIX Vzhao_raster_tilestore
  TOP_MODULE zhao_raster_tilestore
  SOURCES ${ZHAO_RASTER_TILESTORE_SV})
add_test(NAME raster_tilestore_random COMMAND test_raster_tilestore_random)
set_tests_properties(raster_tilestore_random PROPERTIES LABELS "fast;nightly")
# 300k-cycle soak of the same binary (20k fast above)
add_test(NAME raster_tilestore_random_nightly
         COMMAND test_raster_tilestore_random --nightly)
set_tests_properties(raster_tilestore_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_raster_tilestore
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_tilestore
                 ${ZHAO_RASTER_TILESTORE_SV})
set_tests_properties(lint_raster_tilestore PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- Phase 4: RASTER.RESOLVE (charter phase 4, ZH-024; law -----------------
# ---- reference/src/zrender/resolve.cpp + spec/video_rules.md 3 +
# ---- spec/capture_format.md 2; contract design/contracts/RASTER.RESOLVE.md) -
# The oracle is zref::TileResolve, which is itself a view onto the FROZEN
# software resolve (reference/src/zrender/resolve.cpp), so "RTL == oracle" is
# literally "RTL == the charter 8 ordered dither" — no re-derived dither
# anywhere in the test. The CRC likewise delegates to the one generated
# CRC-32C machine, on both sides.
set(ZHAO_RASTER_RESOLVE_SV
    ${ZHAO_ABI_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv)

add_executable(test_raster_resolve_directed raster/raster_resolve_directed.cpp)
target_link_libraries(test_raster_resolve_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_resolve_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_resolve_directed
  PREFIX Vzhao_raster_resolve
  TOP_MODULE zhao_raster_resolve
  SOURCES ${ZHAO_RASTER_RESOLVE_SV})
add_test(NAME raster_resolve_directed COMMAND test_raster_resolve_directed)
set_tests_properties(raster_resolve_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_resolve_random raster/raster_resolve_random.cpp)
target_link_libraries(test_raster_resolve_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_resolve_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_resolve_random
  PREFIX Vzhao_raster_resolve
  TOP_MODULE zhao_raster_resolve
  SOURCES ${ZHAO_RASTER_RESOLVE_SV})
add_test(NAME raster_resolve_random COMMAND test_raster_resolve_random)
set_tests_properties(raster_resolve_random PROPERTIES LABELS "fast;nightly")
# 18k-tile soak of the same binary (1.2k fast above)
add_test(NAME raster_resolve_random_nightly
         COMMAND test_raster_resolve_random --nightly)
set_tests_properties(raster_resolve_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_raster_resolve
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_resolve
                 ${ZHAO_RASTER_RESOLVE_SV})
set_tests_properties(lint_raster_resolve PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the RGB565 quantizer, proved on zhao_raster_quant — the module
# zhao_raster_resolve instantiates 3x per pixel — at BOTH shipping parameter
# sets. TWO tasks (bmc AND cover): the covers pin green's clamp actually
# firing, without which the no-wrap theorem would also hold for a quantizer
# that never reaches the rail. Reuses the hardened mem_formal_lane wrapper.
if(ZHAO_SBY_BIN)
  set(PROP "raster_resolve_quant")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/raster_resolve_quant_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/raster_resolve_quant_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_raster_resolve_quant
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_raster_resolve_quant PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---- Phase 5: RASTER.EARLYZ (ZH-059; law spec/qformats.md 8's strict -------
# ---- invw24 depth test + charter 8's pass-6 coarse depth binning;
# ---- contract design/contracts/RASTER.EARLYZ.md) ---------------------------
# The oracle is zref::EarlyZ. Like zref::TileStore it is a plainly-written
# model of the CONTRACT rather than a view onto a frozen function - nothing
# earlier here maintains a hierarchical-Z floor - so what the differential
# lanes exercise is the RTL's registered output stage, backpressure and
# saturating counters. The one thing both sides share is the fragment state
# word's bit positions, which come from zref::FragmentPipeline::State.
# Kept a separate block from RASTER.FRAGMENT by architect ruling (1.D); the
# argument for that split is in the RTL header and the contract.
set(ZHAO_RASTER_EARLYZ_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv)

add_executable(test_raster_earlyz_directed raster/raster_earlyz_directed.cpp)
target_link_libraries(test_raster_earlyz_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_earlyz_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_earlyz_directed
  PREFIX Vzhao_raster_earlyz
  TOP_MODULE zhao_raster_earlyz
  SOURCES ${ZHAO_RASTER_EARLYZ_SV})
add_test(NAME raster_earlyz_directed COMMAND test_raster_earlyz_directed)
set_tests_properties(raster_earlyz_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_earlyz_random raster/raster_earlyz_random.cpp)
target_link_libraries(test_raster_earlyz_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_earlyz_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_earlyz_random
  PREFIX Vzhao_raster_earlyz
  TOP_MODULE zhao_raster_earlyz
  SOURCES ${ZHAO_RASTER_EARLYZ_SV})
add_test(NAME raster_earlyz_random COMMAND test_raster_earlyz_random)
set_tests_properties(raster_earlyz_random PROPERTIES LABELS "fast;nightly")
# 15x soak of the same binary
add_test(NAME raster_earlyz_random_nightly COMMAND test_raster_earlyz_random --nightly)
set_tests_properties(raster_earlyz_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_raster_earlyz
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_earlyz
                 ${ZHAO_RASTER_EARLYZ_SV})
set_tests_properties(lint_raster_earlyz PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- Phase 4: RASTER.FRAGMENT (ZH-025; the charter 4 EXEMPLAR block; law ---
# ---- spec/qformats.md 8 (the strict depth test) + 2/3/4 (unit8 and the one
# ---- rounding primitive) + spec/sky_and_beams.md 1.1/2 and
# ---- spec/stars_and_flares.md 1 (the six ratified recipes);
# ---- contract design/contracts/RASTER.FRAGMENT.md) -------------------------
# The oracle is zref::FragmentPipeline, which restates no arithmetic it can
# delegate: the unit8 product is zref::unit_mul (qformats 3's frozen
# `((u32)a*b + 128) >> 8`) and the tile word is zref::TileStore::Word. So
# "RTL == oracle" is "RTL == the 8 depth test, the frozen unit8 lane, and the
# recipes those two specs ratified".
set(ZHAO_RASTER_FRAGMENT_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv)

add_executable(test_raster_fragment_directed raster/raster_fragment_directed.cpp)
target_link_libraries(test_raster_fragment_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_fragment_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_fragment_directed
  PREFIX Vzhao_raster_fragment
  TOP_MODULE zhao_raster_fragment
  SOURCES ${ZHAO_RASTER_FRAGMENT_SV})
add_test(NAME raster_fragment_directed COMMAND test_raster_fragment_directed)
set_tests_properties(raster_fragment_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_fragment_random raster/raster_fragment_random.cpp)
target_link_libraries(test_raster_fragment_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_fragment_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_fragment_random
  PREFIX Vzhao_raster_fragment
  TOP_MODULE zhao_raster_fragment
  SOURCES ${ZHAO_RASTER_FRAGMENT_SV})
add_test(NAME raster_fragment_random COMMAND test_raster_fragment_random)
set_tests_properties(raster_fragment_random PROPERTIES LABELS "fast;nightly")
# 15x soak of the same binary
add_test(NAME raster_fragment_random_nightly COMMAND test_raster_fragment_random --nightly)
set_tests_properties(raster_fragment_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_raster_fragment
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_fragment
                 ${ZHAO_RASTER_FRAGMENT_SV})
set_tests_properties(lint_raster_fragment PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the blend channel, proved on zhao_raster_blend - the module
# zhao_raster_fragment instantiates 3x per fragment. TWO tasks (bmc AND
# cover): the covers pin the additive rail actually firing and the alpha
# lerp actually reaching both endpoints, without which the no-overshoot and
# no-wrap theorems would also hold for a blend that never moved. Reuses the
# hardened mem_formal_lane wrapper.
if(ZHAO_SBY_BIN)
  set(PROP "raster_fragment_blend")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/raster_fragment_blend_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/raster_fragment_blend_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_raster_fragment_blend
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_raster_fragment_blend PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()


# ---- Phase 5: TEXTURE.CACHE (ZH-061) + TEXTURE.TMU (ZH-027) ----------------
# ---- law: charter 15 (formats, layout, the Primary TMU capability list),
# ---- 7.3 (M10K tenancy), 26 (no second unrestricted TMU); spec/qformats.md
# ---- 2/3/4 (unit8, single rounding, rescale) and 8 (perspective UV);
# ---- spec/stars_and_flares.md 1 (nearest mandatory on a palette, the
# ---- palette-page invalidate); spec/sky_and_beams.md 1.1/2 (which recipe
# ---- needs which mode); spec/terrain_rules.md 6.2 (the frozen mirrored fold).
# ---- contracts design/contracts/TEXTURE.CACHE.md + TEXTURE.TMU.md ---------
# The oracles are zref::TextureCache and zref::Tmu. Neither restates arithmetic
# it can delegate: the RGB565 expansion is zref::sky::rgb565::to_rgb888 (the
# frozen stars-2 law) and MIRROR is the generalisation of
# zref::terrain::mirror_texel that texture_tmu_directed pins against the frozen
# helper itself over a full period. The mip level OFFSET is a summation loop in
# the oracle and a base-4-repunit closed form in the RTL, so the differential
# lanes check that identity too. THE BILINEAR LAW IS DERIVED, NOT CITED: no
# spec in this repository defines filter weights or their rounding, and the
# derivation from qformats 2/3/4 is recorded in zhao_texture_bilerp.sv and in
# design/contracts/TEXTURE.TMU.md.
set(ZHAO_TEXTURE_CACHE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache.sv)
set(ZHAO_TEXTURE_TMU_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu.sv)

add_executable(test_texture_cache_directed texture/texture_cache_directed.cpp)
target_link_libraries(test_texture_cache_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_cache_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_cache_directed
  PREFIX Vzhao_texture_cache
  TOP_MODULE zhao_texture_cache
  SOURCES ${ZHAO_TEXTURE_CACHE_SV})
add_test(NAME texture_cache_directed COMMAND test_texture_cache_directed)
set_tests_properties(texture_cache_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_texture_cache_random texture/texture_cache_random.cpp)
target_link_libraries(test_texture_cache_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_cache_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_cache_random
  PREFIX Vzhao_texture_cache
  TOP_MODULE zhao_texture_cache
  SOURCES ${ZHAO_TEXTURE_CACHE_SV})
add_test(NAME texture_cache_random COMMAND test_texture_cache_random)
set_tests_properties(texture_cache_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
# 13x soak of the same binary
add_test(NAME texture_cache_random_nightly COMMAND test_texture_cache_random --nightly)
set_tests_properties(texture_cache_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

add_executable(test_texture_tmu_directed texture/texture_tmu_directed.cpp)
target_link_libraries(test_texture_tmu_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_tmu_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_tmu_directed
  PREFIX Vzhao_texture_tmu
  TOP_MODULE zhao_texture_tmu
  SOURCES ${ZHAO_TEXTURE_TMU_SV})
add_test(NAME texture_tmu_directed COMMAND test_texture_tmu_directed)
set_tests_properties(texture_tmu_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_texture_tmu_random texture/texture_tmu_random.cpp)
target_link_libraries(test_texture_tmu_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_tmu_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_tmu_random
  PREFIX Vzhao_texture_tmu
  TOP_MODULE zhao_texture_tmu
  SOURCES ${ZHAO_TEXTURE_TMU_SV})
add_test(NAME texture_tmu_random COMMAND test_texture_tmu_random)
set_tests_properties(texture_tmu_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
# 15x soak of the same binary
add_test(NAME texture_tmu_random_nightly COMMAND test_texture_tmu_random --nightly)
set_tests_properties(texture_tmu_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_texture_cache
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_cache
                 ${ZHAO_TEXTURE_CACHE_SV})
set_tests_properties(lint_texture_cache PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_texture_tmu
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_tmu
                 ${ZHAO_TEXTURE_TMU_SV})
set_tests_properties(lint_texture_tmu PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the bilinear channel, proved on zhao_texture_bilerp - the module
# zhao_texture_tmu instantiates 4x per filtered sample. TWO tasks (bmc AND
# cover): the covers pin the EXACT rounding tie actually firing and the filter
# actually mixing, without which the exactness and convexity theorems would
# also hold for a truncating filter that never left tap 0. Reuses the hardened
# mem_formal_lane wrapper.
if(ZHAO_SBY_BIN)
  set(PROP "texture_bilerp")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/texture_bilerp_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/texture_bilerp_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_texture_bilerp
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  # 3600, not the 2100 the other formal lanes use: MEASURED, not guessed. The
  # bmc task closes in 741 s standalone on this kit (boolector, depth 2) and
  # 1,264 s as this lane, because the partition-of-unity identity is nonlinear
  # over four bit-blasted 9x9 multipliers.
  set_tests_properties(formal_texture_bilerp PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 3600
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---- Phase 4/5: the composed tile pipe (charter phase 4; the roadmap's -----
# ---- "first exact tile and triangle"). RASTER.EDGEWALK -> RASTER.EARLYZ ->
# ---- RASTER.FRAGMENT -> RASTER.TILESTORE -> RASTER.RESOLVE, the whole
# ---- raster chain instantiated together.
# NOT a ledger block: design/blocks.yml's RASTER group has five entries and
# none of them is "the composition", and registering one is a validator-gated
# ledger edit. The rationale lives in the RTL header
# (fpga/rtl/raster/zhao_raster_tile_pipe.sv), which is also where the three
# laws this file owns -- the tile sequencing/ping-pong, the coverage->write
# expansion, and the framebuffer pixel address -- are stated.
# The oracle is the FIVE oracles wired the same way: zref::EdgeWalk ->
# zref::EarlyZ -> zref::FragmentPipeline -> zref::TileStore ->
# zref::TileResolve, each a view onto a frozen reference or a plainly-written
# contract model, so "RTL == oracle" is "RTL == the five laws, composed".
set(ZHAO_RASTER_TILE_PIPE_SV
    ${ZHAO_ABI_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv)

add_executable(test_raster_tile_pipe_directed raster/raster_tile_pipe_directed.cpp)
target_link_libraries(test_raster_tile_pipe_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_tile_pipe_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_tile_pipe_directed
  PREFIX Vzhao_raster_tile_pipe
  TOP_MODULE zhao_raster_tile_pipe
  SOURCES ${ZHAO_RASTER_TILE_PIPE_SV})
add_test(NAME raster_tile_pipe_directed COMMAND test_raster_tile_pipe_directed)
set_tests_properties(raster_tile_pipe_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_raster_tile_pipe_random raster/raster_tile_pipe_random.cpp)
target_link_libraries(test_raster_tile_pipe_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_raster_tile_pipe_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/raster)
verilate(test_raster_tile_pipe_random
  PREFIX Vzhao_raster_tile_pipe
  TOP_MODULE zhao_raster_tile_pipe
  SOURCES ${ZHAO_RASTER_TILE_PIPE_SV})
add_test(NAME raster_tile_pipe_random COMMAND test_raster_tile_pipe_random)
set_tests_properties(raster_tile_pipe_random PROPERTIES LABELS "fast;nightly")
# 16k-tile soak of the same binary (1.6k fast above)
add_test(NAME raster_tile_pipe_random_nightly
         COMMAND test_raster_tile_pipe_random --nightly)
set_tests_properties(raster_tile_pipe_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# per-module lint (charter 21 step 8)
add_test(NAME lint_raster_tile_pipe
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_tile_pipe
                 ${ZHAO_RASTER_TILE_PIPE_SV})
set_tests_properties(lint_raster_tile_pipe PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- W2.7: console shell + Duo marker demo (plan W2.7; the Phase-2 gate) ----
# zhao_shell_top composes the whole Phase-2 machine; tb_zhao_shell adds the
# behavioural SDRAM model (testbench-only, D2). The harness C++ is the HPS
# (D10): tests/shell/shell_harness.hpp. The gate test is the executable
# demos/wound_lab/duo_markers.cpp (600 capture-exact Duo marker frames).
set(ZHAO_SHELL_RTL
    ${ZHAO_ABI_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_params_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_mode.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_framectl.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scaler.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_fetch.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_linebuf.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_serializer.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scanout.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_scheduler.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_dma.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_counters.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_crc.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/input/zhao_input_snapshot.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/input/zhao_input_rumble.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/audio/zhao_audio_fifo.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_ctrl.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_vram_arbiter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_bridge.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_shell_top.sv)

# synthesizable-cone lint (the model/tb are excluded by construction)
add_test(NAME lint_shell_top
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_shell_top
                 ${ZHAO_SHELL_RTL})
set_tests_properties(lint_shell_top PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(demo_duo_markers
               ${CMAKE_SOURCE_DIR}/demos/wound_lab/duo_markers.cpp)
target_link_libraries(demo_duo_markers PRIVATE zhao_harness zhao_zref)
target_include_directories(demo_duo_markers PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell)
target_compile_definitions(demo_duo_markers PRIVATE
  ZHAO_GOLDEN_ZCAP="${CMAKE_SOURCE_DIR}/captures/golden/wave2/duo_markers.zcap")
verilate(demo_duo_markers
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)

# THE PHASE-2 GATE: two controllers move independent 2D markers in Duo,
# 600 capture-exact marker frames (1,202 displayed frames at the machine's
# sustainable cadence) on the full machine — an executable ctest. ~30 min
# wall, so the full gate carries the gate+nightly labels and a 40-marker
# cut of the SAME binary and assertions guards every commit in fast.
add_test(NAME shell_duo_markers COMMAND demo_duo_markers)
set_tests_properties(shell_duo_markers PROPERTIES
  LABELS "gate;nightly" TIMEOUT 5400)
add_test(NAME shell_duo_markers_fast COMMAND demo_duo_markers --frames 40)
set_tests_properties(shell_duo_markers_fast PROPERTIES
  LABELS "fast" TIMEOUT 1800)

# nightly soak: 10,000 DISPLAYED frames (5,000 marker packets) with PCG
# jitter on publish timing, pads and rumble at the stress corners — the
# scaled Verilator surrogate of the 8-hour hardware stress (hardware lane)
add_test(NAME shell_duo_markers_soak COMMAND demo_duo_markers --soak 5000)
set_tests_properties(shell_duo_markers_soak PROPERTIES
  LABELS "nightly" TIMEOUT 28800)

# bring-up probe (diagnostic binary; NOT a ctest)
add_executable(shell_probe ${CMAKE_CURRENT_SOURCE_DIR}/shell/shell_probe.cpp)
target_link_libraries(shell_probe PRIVATE zhao_harness zhao_zref)
target_include_directories(shell_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shell)
verilate(shell_probe
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)

# per-mode 10-packet golden captures (capture_format.md 6.1): generator +
# byte-identity verifier against the committed .zcap files
add_executable(shell_golden ${CMAKE_CURRENT_SOURCE_DIR}/shell/shell_golden.cpp)
target_link_libraries(shell_golden PRIVATE zhao_harness zhao_zref)
target_include_directories(shell_golden PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shell)
target_compile_definitions(shell_golden PRIVATE
  ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/captures/golden/wave2")
verilate(shell_golden
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_golden_replay COMMAND shell_golden)
set_tests_properties(shell_golden_replay PROPERTIES
  LABELS "fast;nightly" TIMEOUT 1200)
