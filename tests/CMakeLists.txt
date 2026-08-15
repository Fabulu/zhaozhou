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
  SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
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

add_executable(test_render_golden render/render_golden.cpp)
target_link_libraries(test_render_golden PRIVATE zhao_zref)
target_include_directories(test_render_golden PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)  # crater_ring wrapper
add_test(NAME render_golden COMMAND test_render_golden)
set_tests_properties(render_golden PROPERTIES LABELS "fast;nightly")

add_executable(test_render_budget render/render_budget.cpp)
target_link_libraries(test_render_budget PRIVATE zhao_zref)
target_include_directories(test_render_budget PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
add_test(NAME render_budget COMMAND test_render_budget)
set_tests_properties(render_budget PROPERTIES LABELS "fast;nightly")
