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
# The formal WRAPPER's own budget, consumed by formal/mem_formal_lane.cmake.in.
# It is the one that BINDS: a lane's CTest TIMEOUT is only a backstop, and when
# the two disagreed (formal_texture_bilerp: CTest 3600, wrapper 1800) the lane
# died at 1,800.88 s and reported FAILED rather than "timed out". Default
# unchanged; a lane that needs more sets it before its configure_file and puts
# it back afterwards.
set(ZHAO_FORMAL_WRAPPER_TIMEOUT 1800)
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

# Every committed .zcap golden must carry the CURRENT ABI constants. The
# duo_markers golden was regenerated in the same commit that bumped
# ZHAO_ABI_VERSION 2 -> 3 and came out carrying version 2 and the two previous
# SHA-256s -- the regeneration ran against a build that had not picked up the
# new header. Nothing caught it for three days, because the only test comparing
# that file is `shell_duo_markers`, which is gate;nightly and takes 22 minutes.
# A check that expensive is a check that runs rarely, and one that runs rarely
# is one whose failures age. The ABI-info section is built entirely from
# compile-time constants, so it can be checked in a millisecond -- in the FAST
# lane, where a stale constant is reported in a second.
add_executable(test_golden_abi_info unit/test_golden_abi_info.cpp)
target_link_libraries(test_golden_abi_info PRIVATE zhao_zref zhao_harness)
add_test(NAME golden_abi_info COMMAND test_golden_abi_info
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(golden_abi_info PROPERTIES LABELS "fast;nightly")

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

# FIELD v3 Phase 2 gate (reports/Fieldv3.md): full canonical interpret ==
# uniform preparation + FPLAN vector reference executor, on every output,
# every saturation lane, rcp0, boundaries, random legal programs and the
# three committed Earth programs. THREE ctest lanes over ONE binary — a
# mutation sweep must run all of the fast ones (tools/sweep_field_plan.sh).
add_executable(test_field_fplan_diff differential/field_fplan_diff.cpp)
target_link_libraries(test_field_fplan_diff PRIVATE zhao_zref)
target_include_directories(test_field_fplan_diff PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
add_test(NAME field_fplan_diff COMMAND test_field_fplan_diff)
set_tests_properties(field_fplan_diff PROPERTIES LABELS "fast;nightly")
add_test(NAME field_fplan_random COMMAND test_field_fplan_diff --random 40)
set_tests_properties(field_fplan_random PROPERTIES LABELS "fast;nightly")
add_test(NAME field_fplan_random_nightly COMMAND test_field_fplan_diff --random 600)
set_tests_properties(field_fplan_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)
# The generated canonical-operation table (reference/include/zfield/generated/
# zfield_optable.hpp) is pinned against OP_INFO by compiler/tests/
# optable.test.ts — the same place the other TS-generated committed artifacts
# (crater_ring.hpp etc.) are pinned. A ctest lane was deliberately NOT
# registered: compiler/dist is gitignored and CI does not build the compiler,
# and a SKIP-if-absent gate is the drift-hiding failure the repo has already
# paid for once. The C++ side is guarded by the header's own static_asserts.

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
# discipline as formal_lane). cppcheck is present in this repo's MinGW
# toolchain and on CI runners.
#
# THE FORMAT TIER MUST NOT SILENTLY SKIP. It did, for weeks: clang-format is
# not shipped in oss-cad-suite, every local run reported "Skipped", and CI
# went red on every push against a locally green suite. Both readings were
# accurate and neither was useful.
#
# The fix is not to fail when the tool is missing -- it is to make the tool
# present. `clang-format` is a devDependency pinned to the SAME 1.8.0 (LLVM
# 15) the CI job installs, so `npm install` puts the authoritative binary in
# node_modules and the local gate becomes the CI gate rather than a
# lookalike. A system clang-format of a different LLVM version formats
# differently, which is why the pinned path is searched FIRST and why the
# skip existed at all.
if(WIN32)
  set(_zhao_cf_platform "win32")
elseif(APPLE)
  set(_zhao_cf_platform "darwin_x64")
else()
  set(_zhao_cf_platform "linux_x64")
endif()
find_program(ZHAO_CLANG_FORMAT_BIN
  NAMES clang-format clang-format.exe
  PATHS "${CMAKE_SOURCE_DIR}/node_modules/clang-format/bin/${_zhao_cf_platform}"
  NO_DEFAULT_PATH)
find_program(ZHAO_CLANG_FORMAT_BIN NAMES clang-format clang-format.exe)
configure_file(lint/format_check.cmake.in lint/format_check.cmake @ONLY)
add_test(NAME format_check
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/format_check.cmake)
set_tests_properties(format_check PROPERTIES
  LABELS "lint;fast"
  SKIP_REGULAR_EXPRESSION "format tier SKIPPED")

# ---- the shell has ONE file list, not two --------------------------------
# Verilator reads ZHAO_SHELL_RTL below; Quartus reads
# fpga/quartus/shell_fit/zhao_shell_fit.qsf. Step 4 of the FRAMEBLIT
# integration updated one and not the other, so the suite was green for hours
# while Quartus could not elaborate the shell at all. This asserts the sets
# match. No tool needed, so it never skips.
configure_file(lint/source_list_parity.cmake.in lint/source_list_parity.cmake @ONLY)
add_test(NAME source_list_parity
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/source_list_parity.cmake)
set_tests_properties(source_list_parity PROPERTIES LABELS "lint;fast")

# The per-beat CRC users may not chain the bit-serial byte step. Three lines
# to write, obviously correct, passes every functional test, and costs 45 ns
# on a path only a Quartus run would show. No tool needed, so it never skips.
configure_file(lint/no_serial_crc.cmake.in lint/no_serial_crc.cmake @ONLY)
add_test(NAME no_serial_crc
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lint/no_serial_crc.cmake)
set_tests_properties(no_serial_crc PROPERTIES LABELS "lint;fast")

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
set(ZHAO_FORGE_CLIFF_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff.sv)

add_executable(test_forge_cliff_directed forge/forge_cliff_directed.cpp)
target_link_libraries(test_forge_cliff_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_directed
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff
  SOURCES ${ZHAO_FORGE_CLIFF_SV})
add_test(NAME forge_cliff_directed COMMAND test_forge_cliff_directed)
set_tests_properties(forge_cliff_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_forge_cliff
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_cliff
                 ${ZHAO_FORGE_CLIFF_SV})
set_tests_properties(lint_forge_cliff PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# randomized rim differential: 300 random void masks vs an independent
# enumeration oracle + the emitted+dropped==enumerated body identity
add_executable(test_forge_cliff_random forge/forge_cliff_random.cpp)
target_link_libraries(test_forge_cliff_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_random
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff
  SOURCES ${ZHAO_FORGE_CLIFF_SV})
add_test(NAME forge_cliff_random COMMAND test_forge_cliff_random)
set_tests_properties(forge_cliff_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME forge_cliff_random_nightly COMMAND test_forge_cliff_random --nightly)
set_tests_properties(forge_cliff_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# ---- deep-keel wave: TEXTURE.MOSAIC (terrain_rules.md 6.2) ------------------
# The oracle lanes (the mirrored-repeat fold at wrap boundaries, the frozen
# pick constants, the rendered dither + exact modulation through the
# replicated resolve) AND, since 2026-08-19, the RTL lanes: the same two files
# now also drive `zhao_texture_mosaic`, which is why both targets verilate.
# `zhao_texture_mod255` rides along because the mosaic instantiates it.
set(ZHAO_TEXTURE_MOSAIC_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic.sv)

add_executable(test_texture_mosaic_directed texture/texture_mosaic_directed.cpp)
target_link_libraries(test_texture_mosaic_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_mosaic_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_texture_mosaic_directed
  PREFIX Vzhao_texture_mosaic
  TOP_MODULE zhao_texture_mosaic
  SOURCES ${ZHAO_TEXTURE_MOSAIC_SV})
add_test(NAME texture_mosaic_directed COMMAND test_texture_mosaic_directed)
set_tests_properties(texture_mosaic_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# randomized pick/fold differential: the u64 oracle lanes, then TWO RTL
# differential lanes (gameplay-shaped and domain-limit, the latter with the
# exact-equality boundary CONSTRUCTED for all 255 residues).
add_executable(test_texture_mosaic_random texture/texture_mosaic_random.cpp)
target_link_libraries(test_texture_mosaic_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_mosaic_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_mosaic_random
  PREFIX Vzhao_texture_mosaic
  TOP_MODULE zhao_texture_mosaic
  SOURCES ${ZHAO_TEXTURE_MOSAIC_SV})
add_test(NAME texture_mosaic_random COMMAND test_texture_mosaic_random)
set_tests_properties(texture_mosaic_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME texture_mosaic_random_nightly COMMAND test_texture_mosaic_random --nightly)
set_tests_properties(texture_mosaic_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# ---- TEXTURE.AUX (phase 6, ZH-060, cut-order 2) ------------------------------
# The restricted auxiliary source: `zref::render::sample_sheet`'s world ->
# sheet-texel mapping in hardware, mastering SURFACE.SHEET's read port. The
# oracle `zref::AuxSource` (reference/include/zref/zref_aux.hpp) did not exist
# before 2026-08-19 — the ledger named it and nothing under reference/ defined
# it — so texture_aux_directed's FIRST lane proves the new header is a faithful
# view onto the executed law before anything else runs.
#
# texture_aux_directed also composes the two REAL blocks (TEXTURE.AUX mastering
# zhao_surface_sheet) and reads back through the port what the sheet's own
# write side put there; that is the only place the port semantics are pinned.
set(ZHAO_TEXTURE_AUX_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux.sv)

add_executable(test_texture_aux_directed texture/texture_aux_directed.cpp)
target_link_libraries(test_texture_aux_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_aux_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: sample_sheet IS the law
verilate(test_texture_aux_directed
  PREFIX Vzhao_texture_aux
  TOP_MODULE zhao_texture_aux
  SOURCES ${ZHAO_TEXTURE_AUX_SV})
verilate(test_texture_aux_directed
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sheet.sv)
add_test(NAME texture_aux_directed COMMAND test_texture_aux_directed)
set_tests_properties(texture_aux_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_texture_aux_random texture/texture_aux_random.cpp)
target_link_libraries(test_texture_aux_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_aux_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_aux_random
  PREFIX Vzhao_texture_aux
  TOP_MODULE zhao_texture_aux
  SOURCES ${ZHAO_TEXTURE_AUX_SV})
add_test(NAME texture_aux_random COMMAND test_texture_aux_random)
set_tests_properties(texture_aux_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME texture_aux_random_nightly COMMAND test_texture_aux_random --nightly)
set_tests_properties(texture_aux_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_texture_aux
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_aux
                 ${ZHAO_TEXTURE_AUX_SV})
set_tests_properties(lint_texture_aux PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_texture_mosaic
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_mosaic
                 ${ZHAO_TEXTURE_MOSAIC_SV})
set_tests_properties(lint_texture_mosaic PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_texture_mod255
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_mod255
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv)
set_tests_properties(lint_texture_mod255 PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the mod-255 residue network, proved TOTAL over all 2^32 inputs
# rather than sampled. See tests/formal/texture_mod255.sby.
if(ZHAO_SBY_BIN)
  set(PROP "texture_mod255")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/texture_mod255_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/texture_mod255_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_texture_mod255
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_texture_mod255 PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

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
# CMD.DMA now instantiates zhao_crc32c_fold, so the leaf travels with it
set(ZHAO_CMD_DMA_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_dma.sv)
set(ZHAO_DEBUG_COUNTERS_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_counters.sv)
# DEBUG.CRC folds two bytes per vid clock through zhao_crc32c_fold (one pixel
# per clock is two bytes), so the leaf travels with it — same rule as CMD.DMA.
set(ZHAO_DEBUG_CRC_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                      ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_crc.sv)

add_executable(test_cmd_scheduler_directed command/cmd_scheduler_directed.cpp)
target_link_libraries(test_cmd_scheduler_directed PRIVATE zhao_harness zhao_zref)
verilate(test_cmd_scheduler_directed
  PREFIX Vzhao_cmd_scheduler
  TOP_MODULE zhao_cmd_scheduler
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_CMD_SCHEDULER_SV})
add_test(NAME cmd_scheduler_directed COMMAND test_cmd_scheduler_directed)
set_tests_properties(cmd_scheduler_directed PROPERTIES LABELS "fast;nightly")

# GEOM.POSE's cache half: tags, LRU and the four counters, against
# zref::creature::PoseBank. The differential is on the five numbers both sides
# expose -- what a cache can get wrong is which tuple it thinks is resident and
# which one it discards, and those decisions show up exactly there.
add_executable(test_geom_pose_cache_directed geometry/geom_pose_cache_directed.cpp)
target_link_libraries(test_geom_pose_cache_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_pose_cache_directed
  PREFIX Vzhao_geom_pose_cache
  TOP_MODULE zhao_geom_pose_cache
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_cache.sv)
add_test(NAME geom_pose_cache_directed COMMAND test_geom_pose_cache_directed)
set_tests_properties(geom_pose_cache_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_pose_cache_random COMMAND test_geom_pose_cache_directed --random 20)
set_tests_properties(geom_pose_cache_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_pose_cache_random_nightly COMMAND test_geom_pose_cache_directed --random 300)
set_tests_properties(geom_pose_cache_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_geom_pose_cache
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_pose_cache
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_cache.sv)
set_tests_properties(lint_zhao_geom_pose_cache PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.POSE's decode chain: the first WHOLE-CHAIN differential in the creature
# path -- one quaternion engine and one shared multiply engine over a real
# skeleton, against zref::creature::decode_pose. The chain failures are
# structural rather than arithmetic (bone 0 is not a multiply, the root
# displacement is bone 0's alone, the ancestor store must survive depth).
add_executable(test_geom_pose_decode_directed geometry/geom_pose_decode_directed.cpp)
target_link_libraries(test_geom_pose_decode_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_pose_decode_directed
  PREFIX Vzhao_geom_pose_decode
  TOP_MODULE zhao_geom_pose_decode
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_decode.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv)
add_test(NAME geom_pose_decode_directed COMMAND test_geom_pose_decode_directed)
set_tests_properties(geom_pose_decode_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_pose_decode_random COMMAND test_geom_pose_decode_directed --random 40)
set_tests_properties(geom_pose_decode_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_pose_decode_random_nightly COMMAND test_geom_pose_decode_directed --random 600)
set_tests_properties(geom_pose_decode_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_geom_pose_decode
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_pose_decode
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_decode.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv)
set_tests_properties(lint_zhao_geom_pose_decode PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.POSE's chain multiply: sequential, one element per cycle, three 32x32
# products instead of thirty-six. The twelve-cycle walk is asserted as its own
# law (section 7) -- correct arithmetic delivered in the wrong number of cycles,
# or offered while still walking, would pass every differential case.
add_executable(test_geom_mat3x4_mul_directed geometry/geom_mat3x4_mul_directed.cpp)
target_link_libraries(test_geom_mat3x4_mul_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_mat3x4_mul_directed
  PREFIX Vzhao_geom_mat3x4_mul
  TOP_MODULE zhao_geom_mat3x4_mul
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv)
add_test(NAME geom_mat3x4_mul_directed COMMAND test_geom_mat3x4_mul_directed)
set_tests_properties(geom_mat3x4_mul_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_mat3x4_mul_random COMMAND test_geom_mat3x4_mul_directed --random 300)
set_tests_properties(geom_mat3x4_mul_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_mat3x4_mul_random_nightly COMMAND test_geom_mat3x4_mul_directed --random 5000)
set_tests_properties(geom_mat3x4_mul_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_geom_mat3x4_mul
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_mat3x4_mul
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv)
set_tests_properties(lint_zhao_geom_mat3x4_mul PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_NORMALIZE2 and OP_NORMALIZE3. The SECOND reciprocal table now lives in this
# engine -- RCP24_T0 seeds a 24-bit reciprocal with TWO correction steps, while
# FIELD_RCP_T0 next door seeds a 32-bit one with ONE. Swapping them would be
# invisible until some vector came out slightly short, so every entry is checked.
# The zero case is deliberately asymmetric between the two ops and both halves
# are pinned.
add_executable(test_field_normalize_directed differential/field_normalize_directed.cpp)
target_link_libraries(test_field_normalize_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_normalize_directed
  PREFIX Vzhao_field_normalize_tb
  TOP_MODULE zhao_field_normalize_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_normalize_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME field_normalize_directed COMMAND test_field_normalize_directed)
set_tests_properties(field_normalize_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_normalize_random COMMAND test_field_normalize_directed --random 3000)
set_tests_properties(field_normalize_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_normalize_random_nightly COMMAND test_field_normalize_directed --random 50000)
set_tests_properties(field_normalize_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_normalize
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_normalize
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
set_tests_properties(lint_zhao_field_normalize PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# MEM.HPS.ARBITER, composed with the REAL zhao_hps_bridge. Splitting the blit
# out of CMD.DMA gave the machine two things that want one bridge port, and the
# bridge treats a request arriving while it is busy as a PROTOCOL VIOLATION --
# the loser of an unarbitrated race is not made to wait, it is told its transfer
# failed. The composition is deliberate: the property under test is an agreement
# BETWEEN the arbiter and the bridge, and a fake bridge is exactly the thing
# that agrees with whatever the arbiter does. Each client's read data is unique
# to its own address, so a beat routed to the wrong client is a wrong VALUE and
# not merely a wrong count.
add_executable(test_hps_arbiter_directed memory/hps_arbiter_directed.cpp)
target_link_libraries(test_hps_arbiter_directed PRIVATE zhao_harness zhao_zref)
verilate(test_hps_arbiter_directed
  PREFIX Vzhao_hps_arb_compose
  TOP_MODULE zhao_hps_arb_compose
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/memory/zhao_hps_arb_compose.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_bridge.sv)
add_test(NAME hps_arbiter_directed COMMAND test_hps_arbiter_directed)
set_tests_properties(hps_arbiter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_hps_arbiter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_hps_arbiter
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv)
set_tests_properties(lint_zhao_hps_arbiter PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# VIDEO.SLOTMGR -- the framebuffer slot ownership machine. This is the authority
# DEBUG.FRAMEBLIT's whole safety argument rested on and which did not exist:
# something has to decide that the slot being written speculatively is not the
# one on screen. The shell used to grant a guard window at dispatch and clear it
# at done, with no generation, no notion of DISPLAYED and no way to refuse a
# stale event.
#
# The generation increments on every ENTRY INTO WRITING, which is what makes an
# event attributable -- a publication carrying the previous generation is
# refused even though the slot number and the state look identical. That is the
# ABA case, and closing it in the blitter is worthless if this end does not
# close it too. A refused event changes nothing AND is counted: a silent refusal
# turns a lease bug into a frame that mysteriously never appears.
add_executable(test_video_slotmgr_directed video/video_slotmgr_directed.cpp)
target_link_libraries(test_video_slotmgr_directed PRIVATE zhao_harness zhao_zref)
verilate(test_video_slotmgr_directed
  PREFIX Vzhao_video_slotmgr
  TOP_MODULE zhao_video_slotmgr
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_slotmgr.sv)
add_test(NAME video_slotmgr_directed COMMAND test_video_slotmgr_directed)
set_tests_properties(video_slotmgr_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME video_slotmgr_random COMMAND test_video_slotmgr_directed --random 4000)
set_tests_properties(video_slotmgr_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME video_slotmgr_random_nightly COMMAND test_video_slotmgr_directed --random 60000)
set_tests_properties(video_slotmgr_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_video_slotmgr
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_video_slotmgr
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_slotmgr.sv)
set_tests_properties(lint_zhao_video_slotmgr PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE FIELD IR SEQUENCER -- the register file and the instruction walk that turns
# a decoded program into a run. Every other test in this engine checks one op;
# this one checks the thing that runs them.
#
# It does NOT validate, and that is the design: `interpret` runs only on DECODED
# programs and its default case is __builtin_unreachable(). The decoder proves
# every register in range, no use-before-def, no dst overlapping an input or its
# own sources (which is why the write-back needs no bypass network), and exactly
# one OP_END, last. Re-checking here would be a SECOND implementation of those
# rules, and two implementations of a rule is how they drift.
#
# Three read ports, walked: the widest op reads seven registers, and seven 64:1
# muxes of 32 bits is thousands of ALMs to save four cycles in a design that has
# already failed a fit once.
add_executable(test_field_seq_directed differential/field_seq_directed.cpp)
target_link_libraries(test_field_seq_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_seq_directed
  PREFIX Vzhao_field_seq
  TOP_MODULE zhao_field_seq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_seq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_exec_shared.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv)
add_test(NAME field_seq_directed COMMAND test_field_seq_directed)
set_tests_properties(field_seq_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_seq_random COMMAND test_field_seq_directed --random 600)
set_tests_properties(field_seq_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_seq_random_nightly COMMAND test_field_seq_directed --random 8000)
set_tests_properties(field_seq_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_seq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_seq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_seq.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_exec_shared.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv)
set_tests_properties(lint_zhao_field_seq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_RING -- the deepest op in the engine: nine products, two reciprocals, two
# smoothsteps and FIVE ledger lanes. The midpoint is an EXACT 64-bit average
# (rescale_s32(r0+r1, 1)), not fx_add then halve: with both radii at the rail a
# saturating add would put the midpoint at half the radius and the ring in the
# wrong place. The reciprocal is field_rcp INCLUDING its zero, so a degenerate
# ring (r0 == r1) has a defined answer and a sticky rcp0 lane that says so. The
# clamp is applied to the PRODUCT, not to the input. One multiplier and one
# combinational reciprocal serve both smoothsteps, walked.
add_executable(test_field_ring_directed differential/field_ring_directed.cpp)
target_link_libraries(test_field_ring_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_ring_directed
  PREFIX Vzhao_field_ring_tb
  TOP_MODULE zhao_field_ring_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_ring_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv)
add_test(NAME field_ring_directed COMMAND test_field_ring_directed)
set_tests_properties(field_ring_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_ring_random COMMAND test_field_ring_directed --random 3000)
set_tests_properties(field_ring_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_ring_random_nightly COMMAND test_field_ring_directed --random 50000)
set_tests_properties(field_ring_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_ring
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_ring
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv)
set_tests_properties(lint_zhao_field_ring PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_ROT2 and OP_ROT3. The law that matters here is the one that looks like a
# defect: EACH PRODUCT IS ROUNDED SEPARATELY. fx_sub(fx_mul(c,p), fx_mul(s,q))
# rounds TWICE and then saturates the difference, while everywhere else in this
# design a row of products is summed exactly and rescaled ONCE (A3b) -- because
# double rounding is normally the bug. Here the reference does the opposite, and
# an implementation "improved" into the house style is wrong. The test counts
# the inputs where a fused form would actually differ and asserts that count is
# large, because a sweep on which the two happen to agree proves nothing about
# which one is implemented. Four products and two table lookups walk through ONE
# multiplier and ONE sine ROM.
add_executable(test_field_rot_directed differential/field_rot_directed.cpp)
target_link_libraries(test_field_rot_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_rot_directed
  PREFIX Vzhao_field_rot_tb
  TOP_MODULE zhao_field_rot_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_rot_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_rot_directed COMMAND test_field_rot_directed)
set_tests_properties(field_rot_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_rot_random COMMAND test_field_rot_directed --random 4000)
set_tests_properties(field_rot_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_rot_random_nightly COMMAND test_field_rot_directed --random 60000)
set_tests_properties(field_rot_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_rot
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_rot
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
set_tests_properties(lint_zhao_field_rot PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_NOISE2 and OP_RIDGE -- the lattice hash (zref::noise2_hash, PCG RXS-M-XS,
# constants frozen verbatim). The lattice index is an ARITHMETIC shift of a
# signed register reinterpreted as unsigned, so a logical shift agrees for every
# non-negative coordinate and disagrees for every negative one; the RXS shift
# amount is DATA-DEPENDENT (4..19) and the test asserts it actually reached ten
# distinct values, because a sweep that only produces one cannot tell a variable
# shifter from a constant. NOISE2 and RIDGE also disagree about their second
# operand -- an adjacent pair versus two named registers -- and nothing in the
# encoding hints at it. Six 32x32 products walk through ONE multiplier.
add_executable(test_field_noise_directed differential/field_noise_directed.cpp)
target_link_libraries(test_field_noise_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_noise_directed
  PREFIX Vzhao_field_noise_tb
  TOP_MODULE zhao_field_noise_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_noise_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_noise_directed COMMAND test_field_noise_directed)
set_tests_properties(field_noise_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_noise_random COMMAND test_field_noise_directed --random 4000)
set_tests_properties(field_noise_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_noise_random_nightly COMMAND test_field_noise_directed --random 80000)
set_tests_properties(field_noise_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_noise
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_noise
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv)
set_tests_properties(lint_zhao_field_noise PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_CURVE, OP_DCURVE and OP_SPLINE -- the first ops in the engine that read a
# TABLE, so the block takes a table port instead of owning a ROM: tables are
# per-program data carried in the .zprog image, not constants of the hardware.
# The segment search is SIX steps for every table size, not ceil(log2(n)), and it
# runs on the CLAMPED value. SPLINE's closing term is rescale_s32(v, 1) -- the
# one-half of Catmull-Rom -- not v << 16, which is a fixed defect the test keeps
# fixed. The test carries two oracles: zfield::interpret decides every value, and
# a restatement decides the per-lane saturation attribution that Status.sat
# collapses into one bit.
add_executable(test_field_curve_directed differential/field_curve_directed.cpp)
target_link_libraries(test_field_curve_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_curve_directed
  PREFIX Vzhao_field_curve_tb
  TOP_MODULE zhao_field_curve_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_curve_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_curve_directed COMMAND test_field_curve_directed)
set_tests_properties(field_curve_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_curve_random COMMAND test_field_curve_directed --random 3000)
set_tests_properties(field_curve_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_curve_random_nightly COMMAND test_field_curve_directed --random 40000)
set_tests_properties(field_curve_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_curve
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_curve
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv)
set_tests_properties(lint_zhao_field_curve PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_LEN2, OP_LEN3 and OP_DIST2, over the shared exact integer square root.
# The sum of squares is UNSIGNED because three squares reach 3*2^62 -- which
# overflows s64 and fits u64, exactly why the reference uses uint64_t -- and the
# root is a FLOOR, never rounded.
# FIELD v3 Phase 3 probe: ready-context FIFO scheduler with registered plan
# fetch. The directed lane carries the rate gate (1 issue/clock with eight
# resident contexts) and the dynamic form of the Fieldv3 context invariants.
add_executable(test_field_ctx_fifo_directed differential/field_ctx_fifo_directed.cpp)
target_link_libraries(test_field_ctx_fifo_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_ctx_fifo_directed
  PREFIX Vzhao_probe_ctx_fifo
  TOP_MODULE zhao_probe_ctx_fifo
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_ctx_fifo.sv)
add_test(NAME field_ctx_fifo_directed COMMAND test_field_ctx_fifo_directed)
set_tests_properties(field_ctx_fifo_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_ctx_fifo_random COMMAND test_field_ctx_fifo_directed --random 60)
set_tests_properties(field_ctx_fifo_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_ctx_fifo_random_nightly COMMAND test_field_ctx_fifo_directed --random 800)
set_tests_properties(field_ctx_fifo_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 3 probe: two-bank exact distance service. The directed lane
# carries the acceptance GATE (measured four-point II <= 20); the random lane
# is the exactness differential against zref::isqrt_u64.
add_executable(test_field_dist_svc_directed differential/field_dist_svc_directed.cpp)
target_link_libraries(test_field_dist_svc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_dist_svc_directed
  PREFIX Vzhao_probe_dist_svc
  TOP_MODULE zhao_probe_dist_svc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_dist_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv)
add_test(NAME field_dist_svc_directed COMMAND test_field_dist_svc_directed)
set_tests_properties(field_dist_svc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_dist_svc_random COMMAND test_field_dist_svc_directed --random 400)
set_tests_properties(field_dist_svc_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_dist_svc_random_nightly COMMAND test_field_dist_svc_directed --random 5000)
set_tests_properties(field_dist_svc_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 3 probe: barrel curve service. The directed lane carries the
# acceptance GATE (measured four-point CURVE II <= 14); the random lane is the
# exactness differential against zfield::exec_op (the one semantic layer).
add_executable(test_field_curve_svc_directed differential/field_curve_svc_directed.cpp)
target_link_libraries(test_field_curve_svc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_curve_svc_directed
  PREFIX Vzhao_probe_curve_svc
  TOP_MODULE zhao_probe_curve_svc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv)
add_test(NAME field_curve_svc_directed COMMAND test_field_curve_svc_directed)
set_tests_properties(field_curve_svc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_curve_svc_random COMMAND test_field_curve_svc_directed --random 400)
set_tests_properties(field_curve_svc_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_curve_svc_random_nightly COMMAND test_field_curve_svc_directed --random 5000)
set_tests_properties(field_curve_svc_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: the four-wide multiplier bank and its arbiter. The bank
# is shared by the ALU lanes AND the curve and distance services, which own
# no multiplier of their own. The oracle is the arithmetic -- every reply must
# equal the requester's own a*b -- so what is under test is ROUTING under
# contention, not a C model of the same arbitration.
add_executable(test_field_v3_mulbank_directed differential/field_v3_mulbank_directed.cpp)
target_link_libraries(test_field_v3_mulbank_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_mulbank_directed
  PREFIX Vzhao_field_v3_mulbank
  TOP_MODULE zhao_field_v3_mulbank
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_v3_mulbank_directed COMMAND test_field_v3_mulbank_directed)
set_tests_properties(field_v3_mulbank_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_mulbank_random COMMAND test_field_v3_mulbank_directed --random 2000)
set_tests_properties(field_v3_mulbank_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_mulbank_random_nightly COMMAND test_field_v3_mulbank_directed --random 20000)
set_tests_properties(field_v3_mulbank_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: the vector executor DATAPATH, one lane. The differential
# runs the SAME FPLAN through the RTL and through zfield::execute_point --
# the interpreter the whole Field IR is defined by, not a model written
# alongside the RTL.
add_executable(test_field_v3_exec_directed differential/field_v3_exec_directed.cpp)
target_link_libraries(test_field_v3_exec_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_exec_directed
  PREFIX Vzhao_probe_v3_engine
  TOP_MODULE zhao_probe_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rf.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_v3_exec_directed COMMAND test_field_v3_exec_directed)
set_tests_properties(field_v3_exec_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_exec_random COMMAND test_field_v3_exec_directed --random 40)
set_tests_properties(field_v3_exec_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_exec_random_nightly COMMAND test_field_v3_exec_directed --random 400)
set_tests_properties(field_v3_exec_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: NOISE2 and RIDGE for four points at once, on the shared
# four-wide multiplier bank. ONE four-wide bank request is the SAME hash step
# for all four points, so a four-point NOISE2 is six requests and RIDGE is
# four. The differential is against zfield::steps::exec_op -- the shipped
# interpreter -- never a hash rewritten in the test, which would only prove
# two rewrites agree.
#
# The cone is ONE FILE: this block instantiates nothing, because the bank is
# the engine's and is reached through ports.
add_executable(test_field_v3_noise_directed differential/field_v3_noise_directed.cpp)
target_link_libraries(test_field_v3_noise_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_noise_directed
  PREFIX Vzhao_field_v3_noise
  TOP_MODULE zhao_field_v3_noise
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv)
add_test(NAME field_v3_noise_directed COMMAND test_field_v3_noise_directed)
set_tests_properties(field_v3_noise_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_noise_random COMMAND test_field_v3_noise_directed --random 40)
set_tests_properties(field_v3_noise_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_noise_random_nightly COMMAND test_field_v3_noise_directed --random 400)
set_tests_properties(field_v3_noise_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: the long-op dispatcher. Gathers four CONTEXTS into one
# four-point service request, holds what the reply needs, drains the results
# one register per clock (the register file has ONE write port), and releases
# each context after its last write.
#
# This block computes no answer, so there is nothing in zfield to differ
# against -- the oracle is the CONTRACT, checked as properties. The test says
# so rather than pretending otherwise. Design: reports/FIELD_V3_DISPATCH.md.
add_executable(test_field_v3_dispatch_directed differential/field_v3_dispatch_directed.cpp)
target_link_libraries(test_field_v3_dispatch_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_dispatch_directed
  PREFIX Vzhao_field_v3_dispatch
  TOP_MODULE zhao_field_v3_dispatch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv)
add_test(NAME field_v3_dispatch_directed COMMAND test_field_v3_dispatch_directed)
set_tests_properties(field_v3_dispatch_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD v3 Phase 4: the writeback arbiter. The register file has ONE write
# port and, once the services are attached, three streams want it: the ALU
# lanes, and one dispatcher per service. The POLICY is a runtime input rather
# than a parameter, so one model can measure the policies against each other on
# the same traffic -- which is the whole reason the choice is still open. Tie
# it to a constant in the engine and synthesis prunes the unreachable arms.
add_executable(test_field_v3_wbarb_directed differential/field_v3_wbarb_directed.cpp)
target_link_libraries(test_field_v3_wbarb_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_wbarb_directed
  PREFIX Vzhao_field_v3_wbarb
  TOP_MODULE zhao_field_v3_wbarb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv)
add_test(NAME field_v3_wbarb_directed COMMAND test_field_v3_wbarb_directed)
set_tests_properties(field_v3_wbarb_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD v3 Phase 4: ROT2 and ROT3 for four points at once. Eight sine lookups
# stream at one per clock (the table is II 1), then the four products are FOUR
# four-wide bank requests, because c, s, p and q are all per point.
#
# The sine table is PRIVATE here, unlike the multiplier: sharing is for scarce
# resources -- 79 DSPs of 112 -- and the device has 553 M10K while the
# quarter-wave table is about one. See reports/FIELD_V3_REMAINING_OPS.md.
add_executable(test_field_v3_rot_directed differential/field_v3_rot_directed.cpp)
target_link_libraries(test_field_v3_rot_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_rot_directed
  PREFIX Vzhao_field_v3_rot
  TOP_MODULE zhao_field_v3_rot
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_v3_rot_directed COMMAND test_field_v3_rot_directed)
set_tests_properties(field_v3_rot_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_rot_random COMMAND test_field_v3_rot_directed --random 40)
set_tests_properties(field_v3_rot_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_rot_random_nightly COMMAND test_field_v3_rot_directed --random 400)
set_tests_properties(field_v3_rot_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: the long-op path COMPOSED -- dispatcher, service, shared
# multiplier bank and writeback arbiter, with a rival on the bank so refusal is
# real. Every piece is green alone; this is the only place their handovers are
# tested, and every defect that has cost real time in this engine lived exactly
# there. Design: reports/FIELD_V3_DISPATCH.md.
add_executable(test_field_v3_full_directed differential/field_v3_full_directed.cpp)
target_link_libraries(test_field_v3_full_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_full_directed
  PREFIX Vzhao_probe_v3_full
  TOP_MODULE zhao_probe_v3_full
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_full.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rf.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv)
add_test(NAME field_v3_full_directed COMMAND test_field_v3_full_directed)
set_tests_properties(field_v3_full_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_field_v3_earth_directed differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_directed
  PREFIX Vzhao_probe_v3_full
  TOP_MODULE zhao_probe_v3_full
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_full.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rf.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv)
add_test(NAME field_v3_earth_directed COMMAND test_field_v3_earth_directed --points 256
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/crater_ring.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/impact_wave.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/wave_pool.zprog)
set_tests_properties(field_v3_earth_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# THE SAME PROGRAMS ON A WIDER MACHINE. Little's law, not a guess: a point
# costs ~250 clocks of latency and only eight are ever in flight, so the engine
# idles waiting rather than running out of arithmetic. This target scales the
# two parameters that bound points-in-flight and measures what falls out.
# CTX=8 keeps its own target above so every existing tally still means what it
# meant when it was taken.
add_executable(test_field_v3_earth_wide differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_wide PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_wide
  PREFIX Vzhao_probe_v3_full
  TOP_MODULE zhao_probe_v3_full
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_full.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rf.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
  VERILATOR_ARGS -GCTX=32 -GOUTSTANDING=12)
target_compile_definitions(test_field_v3_earth_wide PRIVATE ZHAO_EARTH_CTX=32)
add_test(NAME field_v3_earth_wide COMMAND test_field_v3_earth_wide --points 256
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/impact_wave.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/wave_pool.zprog)
set_tests_properties(field_v3_earth_wide PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# THE QUAD MACHINE. A context is FOUR POINTS: one instruction, four ALUs, and
# one register write carrying all four results. CTX=8 therefore holds the same
# 32 points in flight as the CTX=32 scalar target above, which is what makes
# the two comparable -- the difference between them is the WIDTH, not the
# occupancy.
add_executable(test_field_v3_earth_quad differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_quad PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_quad
  PREFIX Vzhao_probe_v3_full
  TOP_MODULE zhao_probe_v3_full
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_full.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rf.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
  VERILATOR_ARGS -GCTX=32 -GOUTSTANDING=12 -GLANES=4 -GDIST_BANKS=4)
target_compile_definitions(test_field_v3_earth_quad PRIVATE ZHAO_EARTH_CTX=32 ZHAO_EARTH_LANES=4)
add_test(NAME field_v3_earth_quad COMMAND test_field_v3_earth_quad --points 256
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/impact_wave.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/wave_pool.zprog)
set_tests_properties(field_v3_earth_quad PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_field_alu_vec_directed differential/field_alu_vec_directed.cpp)
target_link_libraries(test_field_alu_vec_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_alu_vec_directed
  PREFIX Vzhao_field_alu_vec
  TOP_MODULE zhao_field_alu_vec
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv)
add_test(NAME field_alu_vec_directed COMMAND test_field_alu_vec_directed)
set_tests_properties(field_alu_vec_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_field_v3_svcpath_directed differential/field_v3_svcpath_directed.cpp)
target_link_libraries(test_field_v3_svcpath_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_svcpath_directed
  PREFIX Vzhao_field_v3_svcpath
  TOP_MODULE zhao_field_v3_svcpath
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_curve_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_mulbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_wbarb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_v3_svcpath_directed COMMAND test_field_v3_svcpath_directed)
set_tests_properties(field_v3_svcpath_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD v3 Phase 4: UOP_RING_PREP for four points at once -- nine
# separately-rounded products on the shared bank and NO reciprocal, because the
# planner computes both smoothstep reciprocals ONCE in the prep whenever the
# radii are uniform. The oracle is zfield::steps::ring_prepared, which is the
# step the plan actually emits, not exec_op(OP_RING) which is a different
# lowering.
add_executable(test_field_v3_ring_directed differential/field_v3_ring_directed.cpp)
target_link_libraries(test_field_v3_ring_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_ring_directed
  PREFIX Vzhao_field_v3_ring
  TOP_MODULE zhao_field_v3_ring
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv)
add_test(NAME field_v3_ring_directed COMMAND test_field_v3_ring_directed)
set_tests_properties(field_v3_ring_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_ring_random COMMAND test_field_v3_ring_directed --random 40)
set_tests_properties(field_v3_ring_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_ring_random_nightly COMMAND test_field_v3_ring_directed --random 400)
set_tests_properties(field_v3_ring_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: NORMALIZE2 and NORMALIZE3 for four points at once. Owns an
# isqrt (walked four times -- the documented slow path, and NORMALIZE is COLD in
# the op table) and an rcp24 seed ROM, and takes ten four-wide bank requests.
# ---- the prepared ring, joined to its uniforms ----------------------------
# The arithmetic is closed at 23/23 in zhao_field_v3_ring; what this proves is
# the JOIN -- four scalars fetched by index over a registered read port.
add_executable(test_field_v3_ring_svc_directed differential/field_v3_ring_svc_directed.cpp)
target_link_libraries(test_field_v3_ring_svc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_ring_svc_directed
  PREFIX Vzhao_field_v3_ring_svc
  TOP_MODULE zhao_field_v3_ring_svc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv)
add_test(NAME field_v3_ring_svc_directed COMMAND test_field_v3_ring_svc_directed)
set_tests_properties(field_v3_ring_svc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- OP_SIN and OP_COS ---------------------------------------------------
# Two of the three opcodes measure_earth_ops.cpp found the Earth programs need
# and the hardware did not serve. The arithmetic is zhao_field_sin, already
# trusted by the rotation service; what is new is the four-lane walk around it.
add_executable(test_field_v3_trig_directed differential/field_v3_trig_directed.cpp)
target_link_libraries(test_field_v3_trig_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_trig_directed
  PREFIX Vzhao_field_v3_trig
  TOP_MODULE zhao_field_v3_trig
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_trig.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_v3_trig_directed COMMAND test_field_v3_trig_directed)
set_tests_properties(field_v3_trig_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- LEN2, LEN3 and DIST2 ------------------------------------------------
# The last opcode the Earth programs need. One block for three, because len_of
# is the whole of all three in the reference too.
add_executable(test_field_v3_len_directed differential/field_v3_len_directed.cpp)
target_link_libraries(test_field_v3_len_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_len_directed
  PREFIX Vzhao_field_v3_len
  TOP_MODULE zhao_field_v3_len
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv)
add_test(NAME field_v3_len_directed COMMAND test_field_v3_len_directed)

# THE SAME SERVICE WITH FOUR ROOT BANKS. The root is 32 fixed iterations and
# cannot be pipelined, so throughput is bought only by having more of them:
# II 22 at two banks, 12 at four. At 2 the distance service alone consumed 90%
# of the whole Earth admission budget. BOTH are tested, because the shipped
# default is still 2 and a parameter nobody exercises is a parameter that
# rots.
add_executable(test_field_v3_len_banks4 differential/field_v3_len_directed.cpp)
target_link_libraries(test_field_v3_len_banks4 PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_len_banks4
  PREFIX Vzhao_field_v3_len
  TOP_MODULE zhao_field_v3_len
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
  VERILATOR_ARGS -GBANKS=4)
add_test(NAME field_v3_len_banks4 COMMAND test_field_v3_len_banks4)
set_tests_properties(field_v3_len_banks4 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
set_tests_properties(field_v3_len_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- the uniform (scalar) bank -------------------------------------------
# 64 slots because tools/field/measure_sreg_hwm.cpp measured a worst case of 41
# across the corpus; the probe is committed so the number can be rechecked.
add_executable(test_field_v3_sbank_directed differential/field_v3_sbank_directed.cpp)
target_link_libraries(test_field_v3_sbank_directed PRIVATE zhao_harness)
verilate(test_field_v3_sbank_directed
  PREFIX Vzhao_field_v3_sbank
  TOP_MODULE zhao_field_v3_sbank
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_sbank.sv)
add_test(NAME field_v3_sbank_directed COMMAND test_field_v3_sbank_directed)
set_tests_properties(field_v3_sbank_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_field_v3_normalize_directed differential/field_v3_normalize_directed.cpp)
target_link_libraries(test_field_v3_normalize_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_normalize_directed
  PREFIX Vzhao_field_v3_normalize
  TOP_MODULE zhao_field_v3_normalize
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME field_v3_normalize_directed COMMAND test_field_v3_normalize_directed)
set_tests_properties(field_v3_normalize_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_normalize_random COMMAND test_field_v3_normalize_directed --random 40)
set_tests_properties(field_v3_normalize_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_normalize_random_nightly COMMAND test_field_v3_normalize_directed --random 400)
set_tests_properties(field_v3_normalize_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 4: SPLINE's Catmull-Rom arithmetic for four points at once --
# the coefficients, a two-step Horner and the half. The LOOKUP half (six-step
# search, clamp, parameter, and FOUR neighbour reads) belongs to a widened
# curve service and does not exist yet: CURVE captures ONE entry on the way
# down, so SPLINE is a second READER of that cache rather than another mode.
add_executable(test_field_v3_spline_directed differential/field_v3_spline_directed.cpp)
target_link_libraries(test_field_v3_spline_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_spline_directed
  PREFIX Vzhao_field_v3_spline
  TOP_MODULE zhao_field_v3_spline
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv)
add_test(NAME field_v3_spline_directed COMMAND test_field_v3_spline_directed)
set_tests_properties(field_v3_spline_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_v3_spline_random COMMAND test_field_v3_spline_directed --random 40)
set_tests_properties(field_v3_spline_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD v3 Phase 4: the Earth lattice walker -- the front of the composed
# Earth machine. The differential is against the reference walk itself
# (zref::terrain::lattice_lerp + the 9.1 closed-interval coverage test),
# not against a hand-written expectation.
add_executable(test_field_walk_earth_directed differential/field_walk_earth_directed.cpp)
target_link_libraries(test_field_walk_earth_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_walk_earth_directed
  PREFIX Vzhao_probe_walk_earth
  TOP_MODULE zhao_probe_walk_earth
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_walk_earth.sv)
add_test(NAME field_walk_earth_directed COMMAND test_field_walk_earth_directed)
set_tests_properties(field_walk_earth_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_walk_earth_random COMMAND test_field_walk_earth_directed --random 40)
set_tests_properties(field_walk_earth_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_walk_earth_random_nightly COMMAND test_field_walk_earth_directed --random 400)
set_tests_properties(field_walk_earth_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# FIELD v3 Phase 3 probe: four-bank patch accumulator with exact
# command-order reducers. The directed lane carries the acceptance GATE (one
# vector update per clock, measured); the random lane is the differential
# against zref::terrain::compose_vertex + the declared reducer laws.
add_executable(test_field_patch_acc_directed differential/field_patch_acc_directed.cpp)
target_link_libraries(test_field_patch_acc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_patch_acc_directed
  PREFIX Vzhao_probe_patch_acc
  TOP_MODULE zhao_probe_patch_acc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_patch_acc.sv)
add_test(NAME field_patch_acc_directed COMMAND test_field_patch_acc_directed)
set_tests_properties(field_patch_acc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_patch_acc_random COMMAND test_field_patch_acc_directed --random 40)
set_tests_properties(field_patch_acc_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_patch_acc_random_nightly COMMAND test_field_patch_acc_directed --random 400)
set_tests_properties(field_patch_acc_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_field_len_directed differential/field_len_directed.cpp)
target_link_libraries(test_field_len_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_len_directed
  PREFIX Vzhao_field_len_tb
  TOP_MODULE zhao_field_len_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_len_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv)
add_test(NAME field_len_directed COMMAND test_field_len_directed)
set_tests_properties(field_len_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_len_random COMMAND test_field_len_directed --random 3000)
set_tests_properties(field_len_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_len_random_nightly COMMAND test_field_len_directed --random 60000)
set_tests_properties(field_len_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_len
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_len
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv)
set_tests_properties(lint_zhao_field_len PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_SIN and OP_COS. EXHAUSTIVE -- an angle16 is a u16 of turns, so 65,536
# angles times two opcodes is the whole input space and there is no sampling
# argument to make. The quarter-wave table is generated into
# zhao_field_sin_rom.sv and checked entry by entry.
add_executable(test_field_sin_directed differential/field_sin_directed.cpp)
target_link_libraries(test_field_sin_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_sin_directed
  PREFIX Vzhao_field_sin
  TOP_MODULE zhao_field_sin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_sin_directed COMMAND test_field_sin_directed)
set_tests_properties(field_sin_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_field_sin
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_sin
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
set_tests_properties(lint_zhao_field_sin PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# OP_RCP, the first Field IR op that needs a table. The seed table is GENERATED
# into zhao_field_rcp_rom.sv from zref_tables.hpp rather than transcribed, and
# section 1 checks all 256 entries against the reference -- a generated file goes
# stale as easily as a copied one, the generator is not run by the build, and a
# wrong seed does not fail loudly.
add_executable(test_field_rcp_directed differential/field_rcp_directed.cpp)
target_link_libraries(test_field_rcp_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_rcp_directed
  PREFIX Vzhao_field_rcp_tb
  TOP_MODULE zhao_field_rcp_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_rcp_tb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv)
add_test(NAME field_rcp_directed COMMAND test_field_rcp_directed)
set_tests_properties(field_rcp_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_rcp_random COMMAND test_field_rcp_directed --random 20000)
set_tests_properties(field_rcp_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_rcp_random_nightly COMMAND test_field_rcp_directed --random 400000)
set_tests_properties(field_rcp_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_field_rcp
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_rcp
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv)
set_tests_properties(lint_zhao_field_rcp PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# DEBUG.FRAMEBLIT, the block split out of CMD.DMA by
# reports/CMD.DMA_Redesign_Proposal.md Part 2. Its reason to exist is that the
# old blit path carried a 245,760-byte staging buffer and stopped the shell
# fitting; the amended atomicity law lets the framebuffer slot itself be the
# transaction buffer and the staging buffer collapses to one 64-byte chunk.
add_executable(test_debug_frameblit_directed debug/debug_frameblit_directed.cpp)
target_link_libraries(test_debug_frameblit_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_debug_frameblit_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
verilate(test_debug_frameblit_directed
  PREFIX Vzhao_debug_frameblit
  TOP_MODULE zhao_debug_frameblit
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_frameblit.sv)
add_test(NAME debug_frameblit_directed COMMAND test_debug_frameblit_directed)
set_tests_properties(debug_frameblit_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_debug_frameblit
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_debug_frameblit
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_frameblit.sv)
set_tests_properties(lint_zhao_debug_frameblit PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The Field IR arithmetic core. One file covers FIELD.MOV, FIELD.ADD and
# FIELD.SUB -- the three op differentials V10 names as blockers for ALL FOUR
# FIELD.SEQ.* blocks -- plus the other twelve arithmetic ops, because they share
# one ALU. The oracle is zfield::interpret's own arithmetic (the zref::fx_*
# primitives it calls), so nothing is re-derived from spec prose and the block
# stays on the right side of field-ir.md's grep-audit law.
# ---- CRC-32C parallel fold ------------------------------------------------
# Equivalence against the SHIPPED zhao_crc32c_step is the module's entire
# contract: it exists to shorten a 64-level combinational chain that the
# composed fit measured at ~38 ns against a 10 ns budget, and a faster CRC that
# computes a different number than the rest of the machine is worse than the
# slow one. The property is combinational, so the lane can be driven hard.
add_executable(test_crc32c_fold differential/crc32c_fold_directed.cpp)
target_link_libraries(test_crc32c_fold PRIVATE zhao_harness zhao_zref)
verilate(test_crc32c_fold
  PREFIX Vzhao_crc32c_fold
  TOP_MODULE zhao_crc32c_fold
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv)
add_test(NAME crc32c_fold_directed COMMAND test_crc32c_fold)
set_tests_properties(crc32c_fold_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME crc32c_fold_random COMMAND test_crc32c_fold --random 200000)
set_tests_properties(crc32c_fold_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME crc32c_fold_random_nightly COMMAND test_crc32c_fold --random 4000000)
set_tests_properties(crc32c_fold_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)
add_test(NAME lint_zhao_crc32c_fold
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_crc32c_fold
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv)
set_tests_properties(lint_zhao_crc32c_fold PROPERTIES LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_field_alu_ops differential/field_alu_ops.cpp)
target_link_libraries(test_field_alu_ops PRIVATE zhao_harness zhao_zref)
verilate(test_field_alu_ops
  PREFIX Vzhao_field_alu_tb
  TOP_MODULE zhao_field_alu_tb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/tests/rtl/zhao_field_alu_tb.sv)
add_test(NAME field_alu_ops COMMAND test_field_alu_ops)
set_tests_properties(field_alu_ops PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_alu_random COMMAND test_field_alu_ops --random 20000)
set_tests_properties(field_alu_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_alu_random_nightly COMMAND test_field_alu_ops --random 400000)
set_tests_properties(field_alu_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_field_alu
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_alu
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv)
set_tests_properties(lint_zhao_field_alu PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# PART.SOFT, in the same two layers as PART.EXPAND and for the same reason: its
# law is the `points` branch of draw_population plus blit_pattern_block, computed
# and painted inline, so the reference restates it and layer 1 checks the
# restatement against the renderer pixel for pixel.
add_executable(test_part_soft_directed particles/part_soft_directed.cpp)
target_link_libraries(test_part_soft_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_part_soft_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_part_soft_directed
  PREFIX Vzhao_part_soft
  TOP_MODULE zhao_part_soft
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_soft.sv)
add_test(NAME part_soft_directed COMMAND test_part_soft_directed)
set_tests_properties(part_soft_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME part_soft_random COMMAND test_part_soft_directed --random 2000)
set_tests_properties(part_soft_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME part_soft_random_nightly COMMAND test_part_soft_directed --random 40000)
set_tests_properties(part_soft_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_part_soft
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_soft
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_soft.sv)
set_tests_properties(lint_zhao_part_soft PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# PART.EXPAND, in TWO layers. Its reference had to RESTATE a law that lives
# inline inside zref::render::draw_population, and a restated law is a second
# implementation -- so layer 1 renders the same particles both ways, through
# draw_population and through expand_polygon + the same raster_tri, and requires
# identical surfaces. Layer 2 is the ordinary RTL differential. Layer 1 is what
# makes layer 2 worth anything.
add_executable(test_part_expand_directed particles/part_expand_directed.cpp)
target_link_libraries(test_part_expand_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_part_expand_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_part_expand_directed
  PREFIX Vzhao_part_expand
  TOP_MODULE zhao_part_expand
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_expand.sv)
add_test(NAME part_expand_directed COMMAND test_part_expand_directed)
set_tests_properties(part_expand_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME part_expand_random COMMAND test_part_expand_directed --random 2000)
set_tests_properties(part_expand_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME part_expand_random_nightly COMMAND test_part_expand_directed --random 40000)
set_tests_properties(part_expand_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_part_expand
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_expand
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_expand.sv)
set_tests_properties(lint_zhao_part_expand PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# FIELD.PROGCACHE against zref::field::ProgCache. Driven with the three real
# committed .zprog images and CORRUPTED copies of them, so the reject law is
# exercised by the loader's own verdicts rather than by an invented flag. The
# cache is instantiated with TWO entries here: three real programs then exercise
# every residency law there is, which a sixteen-entry cache driven by made-up
# hashes would not.
add_executable(test_field_progcache_directed field/field_progcache_directed.cpp)
target_link_libraries(test_field_progcache_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_field_progcache_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
verilate(test_field_progcache_directed
  PREFIX Vzhao_field_progcache
  TOP_MODULE zhao_field_progcache
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
  VERILATOR_ARGS -GENTRIES=2)
add_test(NAME field_progcache_directed COMMAND test_field_progcache_directed)
set_tests_properties(field_progcache_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progcache_random COMMAND test_field_progcache_directed --random 60)
set_tests_properties(field_progcache_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progcache_random_nightly COMMAND test_field_progcache_directed --random 800)
set_tests_properties(field_progcache_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_field_progcache
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_progcache
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv)
set_tests_properties(lint_zhao_field_progcache PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.PROJECT against zref::render::project_vertex -- the real law, replacing
# the phantom zref::GeomProject the ledger declared. White-box include of
# reference/src for zrender/internal.hpp, the same access TERRAIN.PROJECT's
# suite already has.
#
# GEOM.PROJECT and TERRAIN.PROJECT are two shells around ONE projector,
# fpga/rtl/common/zhao_project_core.sv (RUN-20260824-0522). Every target that
# verilates or lints either shell needs the core in its source list, and the
# path is written ONCE here for the same reason source_list_parity.cmake
# exists: two statements of one fact, and the one that is not exercised rots.
set(ZHAO_PROJECT_CORE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_core.sv)
add_executable(test_geom_project_directed geometry/geom_project_directed.cpp)
target_link_libraries(test_geom_project_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_project_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_geom_project_directed
  PREFIX Vzhao_geom_project
  TOP_MODULE zhao_geom_project
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_project.sv
          ${ZHAO_PROJECT_CORE_SV})
add_test(NAME geom_project_directed COMMAND test_geom_project_directed)
set_tests_properties(geom_project_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME geom_project_random COMMAND test_geom_project_directed --random 100)
set_tests_properties(geom_project_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME geom_project_random_nightly COMMAND test_geom_project_directed --random 2000)
set_tests_properties(geom_project_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_geom_project
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_project
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_project.sv
                 ${ZHAO_PROJECT_CORE_SV})
set_tests_properties(lint_zhao_geom_project PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.POSE's quaternion step: quat16 -> 3x4 rotation, against
# zref::creature::quat16_to_mat3. The no-renormalization rule (creature_rules
# 2.2) is a law an "improved" implementation would break, so section 5 drives
# deliberately non-unit quaternions.
add_executable(test_geom_quat2mat_directed geometry/geom_quat2mat_directed.cpp)
target_link_libraries(test_geom_quat2mat_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_quat2mat_directed
  PREFIX Vzhao_geom_quat2mat
  TOP_MODULE zhao_geom_quat2mat
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv)
add_test(NAME geom_quat2mat_directed COMMAND test_geom_quat2mat_directed)
set_tests_properties(geom_quat2mat_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_quat2mat_random COMMAND test_geom_quat2mat_directed --random 500)
set_tests_properties(geom_quat2mat_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_quat2mat_random_nightly COMMAND test_geom_quat2mat_directed --random 20000)
set_tests_properties(geom_quat2mat_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_geom_quat2mat
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_quat2mat
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv)
set_tests_properties(lint_zhao_geom_quat2mat PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.SKIN: rigid and two-weight skinning against zref::skin_vertex -- the
# shipped function every creature the reference renderer has drawn was skinned
# with. The single-rounding law (one rescale by 22, never two by 16) is what
# the directed boundaries defend.
add_executable(test_geom_skin_directed geometry/geom_skin_directed.cpp)
target_link_libraries(test_geom_skin_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_skin_directed
  PREFIX Vzhao_geom_skin
  TOP_MODULE zhao_geom_skin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_skin.sv)
add_test(NAME geom_skin_directed COMMAND test_geom_skin_directed)
set_tests_properties(geom_skin_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_skin_random COMMAND test_geom_skin_directed --random 500)
set_tests_properties(geom_skin_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_skin_random_nightly COMMAND test_geom_skin_directed --random 8000)
set_tests_properties(geom_skin_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

# THE OTHER TWO POINTS ON THE FRONTIER, BUILT AND DIFFERENTIATED.
#
# zhao_geom_skin's MUL_LANES parameter is the deliverable, not one setting of
# it: the block was rearchitected from 72 DSPs by sizing its multiplier farm to
# the owner's ~120,000 skinned vertices per 60 Hz frame, and a parameter with
# measured fits at each setting is what the survey of Raster I, RasterIX,
# Vortex, eGPU and SIMTight asked for instead of "discovering the right point
# one emergency at a time".
#
# Elaborating the other settings is not the same as verifying them, and one of
# them is genuinely different code: MUL_LANES = 3 issues all three terms in one
# cycle (TSTEPS == 1), so the DEFAULT BUILD NEVER EXECUTES the multi-cycle term
# walk at all. Only MUL_LANES = 1 does. The same differential runs against all
# three, with the frontier table's latencies as compile-time expectations.
# Written out rather than looped: the mutation sweep's guard 7 derives the set
# of consumers of a mutated .sv by grepping `verilate(<target> ... <file>)` out
# of THIS file, and a `${lanes}` in the target name would leave it deriving a
# literal that matches no build target. A guard that cannot see a consumer
# cannot clean and re-score it.
add_executable(test_geom_skin_lanes1 geometry/geom_skin_directed.cpp)
target_link_libraries(test_geom_skin_lanes1 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_skin_lanes1 PRIVATE ZHAO_SKIN_LANES=1)
verilate(test_geom_skin_lanes1
  PREFIX Vzhao_geom_skin
  TOP_MODULE zhao_geom_skin
  VERILATOR_ARGS -GMUL_LANES=1
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_skin.sv)
add_test(NAME geom_skin_lanes1 COMMAND test_geom_skin_lanes1)
set_tests_properties(geom_skin_lanes1 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_skin_lanes1_random COMMAND test_geom_skin_lanes1 --random 500)
set_tests_properties(geom_skin_lanes1_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_geom_skin_lanes6 geometry/geom_skin_directed.cpp)
target_link_libraries(test_geom_skin_lanes6 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_skin_lanes6 PRIVATE ZHAO_SKIN_LANES=6)
verilate(test_geom_skin_lanes6
  PREFIX Vzhao_geom_skin
  TOP_MODULE zhao_geom_skin
  VERILATOR_ARGS -GMUL_LANES=6
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_skin.sv)
add_test(NAME geom_skin_lanes6 COMMAND test_geom_skin_lanes6)
set_tests_properties(geom_skin_lanes6 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_skin_lanes6_random COMMAND test_geom_skin_lanes6 --random 500)
set_tests_properties(geom_skin_lanes6_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_geom_skin
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_skin
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_skin.sv)
set_tests_properties(lint_zhao_geom_skin PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# FIELD v2: the tagged lane serialiser. v2 issues VECTOR instructions and every
# long-operation unit in the engine is SCALAR, so something must serialise the
# lanes and carry the {wavefront, destination} tag back. v1 needed no tags --
# one instruction in flight meant a reply could only belong to the one thing
# waiting -- so this is a property the new architecture has to prove.
add_executable(test_field_v2_lanemux_directed differential/field_v2_lanemux_directed.cpp)
target_link_libraries(test_field_v2_lanemux_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v2_lanemux_directed
  PREFIX Vzhao_field_v2_lanemux
  TOP_MODULE zhao_field_v2_lanemux
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_lanemux.sv)
add_test(NAME field_v2_lanemux_directed COMMAND test_field_v2_lanemux_directed)
set_tests_properties(field_v2_lanemux_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD v2: the SIMD barrel core. v1 (zhao_field_seq) is FROZEN as the serial
# reference and oracle; v2 is built alongside it and answers to the same
# semantics. This increment covers the single-cycle ALU ops; long operations
# are refused rather than skipped until their tagged interfaces land.
add_executable(test_field_v2_core_directed differential/field_v2_core_directed.cpp)
target_link_libraries(test_field_v2_core_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v2_core_directed
  PREFIX Vzhao_field_v2_core
  TOP_MODULE zhao_field_v2_core
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_lanemux.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rf_ram.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
# FIELD v2's FRONT END. Program in, points in, answers out one per point IN
# ORDER -- order is a correctness property: zref::terrain accumulates out[0]
# into a lattice index taken from its loop counters.
add_executable(test_field_v2_front_directed differential/field_v2_front_directed.cpp)
target_link_libraries(test_field_v2_front_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v2_front_directed
  PREFIX Vzhao_field_v2_front
  TOP_MODULE zhao_field_v2_front
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_front.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v2_lanemux.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rf_ram.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_curve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_len.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ring.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rot.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_mul.sv)
add_test(NAME field_v2_front_directed COMMAND test_field_v2_front_directed)
set_tests_properties(field_v2_front_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME field_v2_core_directed COMMAND test_field_v2_core_directed)
set_tests_properties(field_v2_core_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD.WRITE.{MATERIAL,NAV,HAZARD}: the three Earth sinks, RTL against
# zref::fieldir::compose_*. field_write_earth_sinks.cpp pins the same laws but
# drives the REFERENCE only -- it was unit evidence with no DUT in it, because
# there was no RTL. This is the hardware differential.
add_executable(test_field_sinks_directed differential/field_sinks_directed.cpp)
target_link_libraries(test_field_sinks_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_sinks_directed
  PREFIX Vzhao_field_sinks
  TOP_MODULE zhao_field_sinks
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sinks.sv)
add_test(NAME field_sinks_directed COMMAND test_field_sinks_directed)
set_tests_properties(field_sinks_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# FIELD.STAMP.{MAX,ADD,SUB,REPLACE,AGE}: the five stamp-blend ops, exhaustive
# against zref::surface::blend_apply. ops.yml named this file as their
# differential coverage and it did not exist -- V10 only surfaced that once
# SURFACE.STAMP advanced past SPECIFIED.
add_executable(test_field_stamp_modes differential/field_stamp_modes.cpp)
target_link_libraries(test_field_stamp_modes PRIVATE zhao_harness zhao_zref)
verilate(test_field_stamp_modes
  PREFIX Vzhao_surface_blend
  TOP_MODULE zhao_surface_blend
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_blend.sv)
add_test(NAME field_stamp_modes COMMAND test_field_stamp_modes)
set_tests_properties(field_stamp_modes PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# DEBUG.TRACE: the reference pinned. No RTL yet -- this fixes the three
# questions capture_format and the charter leave open (stage numbering, arming
# as a mask, zeroing a decoder event's raster fields) so they cannot drift
# before the RTL exists to be compared against them.
# DEBUG.TRACE's RTL against zref::trace::Ring. The reference-only laws live in
# debug_trace_directed.cpp; this is the hardware differential, word for word in
# capture order. The drop-never-stall law has no ready signal to test, so it is
# tested by offering events every cycle past the ring's depth and requiring the
# ring's contents to depend on the events and not on their timing.
add_executable(test_debug_trace_rtl_directed debug/debug_trace_rtl_directed.cpp)
target_link_libraries(test_debug_trace_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_debug_trace_rtl_directed
  PREFIX Vzhao_debug_trace
  TOP_MODULE zhao_debug_trace
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_trace.sv)
add_test(NAME debug_trace_rtl_directed COMMAND test_debug_trace_rtl_directed)
set_tests_properties(debug_trace_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME debug_trace_rtl_random COMMAND test_debug_trace_rtl_directed --random 200)
set_tests_properties(debug_trace_rtl_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME debug_trace_rtl_random_nightly COMMAND test_debug_trace_rtl_directed --random 3000)
set_tests_properties(debug_trace_rtl_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_debug_trace
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_debug_trace
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_trace.sv)
set_tests_properties(lint_zhao_debug_trace PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_debug_trace_directed debug/debug_trace_directed.cpp)
target_link_libraries(test_debug_trace_directed PRIVATE zhao_harness zhao_zref)
add_test(NAME debug_trace_directed COMMAND test_debug_trace_directed)
set_tests_properties(debug_trace_directed PROPERTIES LABELS "fast;nightly")
# property-based random lane in the same binary (the CMD.DMA shape)
add_test(NAME debug_trace_random COMMAND test_debug_trace_directed --random 500)
set_tests_properties(debug_trace_random PROPERTIES LABELS "fast;nightly")

# CMD.DECODER: the streaming validator, differential against
# zref::cmd::validate -- a thin view onto zhao_frame_validate, which the stub
# shell, the capture tooling and tests/abi/golden/ have agreed with since
# wave 1. The oracle was NOT written for this test.
add_executable(test_cmd_decoder_directed command/cmd_decoder_directed.cpp)
target_link_libraries(test_cmd_decoder_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_cmd_decoder_directed PRIVATE
  ZHAO_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/abi/golden")
verilate(test_cmd_decoder_directed
  PREFIX Vzhao_cmd_decoder
  TOP_MODULE zhao_cmd_decoder
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv)
add_test(NAME cmd_decoder_directed COMMAND test_cmd_decoder_directed)
set_tests_properties(cmd_decoder_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
# random lane of the same binary: single-bit flips at uniformly random offsets
# in well-formed packets, verdict compared against the oracle every time.
add_test(NAME cmd_decoder_random COMMAND test_cmd_decoder_directed --random 300)
set_tests_properties(cmd_decoder_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME cmd_decoder_random_nightly COMMAND test_cmd_decoder_directed --random 4000)
set_tests_properties(cmd_decoder_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_cmd_decoder
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_cmd_decoder
                 ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv)
set_tests_properties(lint_zhao_cmd_decoder PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

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
  foreach(sby_prop IN ITEMS cmd_scheduler_slot_fsm cmd_dma_crc_gate debug_frameblit_safety
                           video_slotmgr_invariants field_seq_bound sat_add
                           cdc_snapshot)
    set(PROP "${sby_prop}")
    set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
    set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
    # field_seq_bound needs FAR more than the 1800 s default, and the number is
    # MEASURED (2026-08-24) rather than chosen: bmc at the derived depth 172
    # passes in 2:02:17 (7,337 s) and cover in 15.9 s. That 7,337 was taken with
    # a duplicate proof contending for ~30 min of it, so it is an UPPER bound on
    # the solo time -- the safe direction for a budget.
    #
    # Before this the lane died at exactly 1800.20 s, at roughly k=130 of 172,
    # and reported FAILED. It was never failing a property: every bound it
    # reached was UNSATISFIABLE. It was never FINISHING. That is exactly the
    # shape the mem_vram_arbiter_liveness comment above warns about -- a proof
    # lane that times out gets read as a failure, or gets "fixed" by deleting
    # tasks.
    #
    # Per the ZHAO_FORMAL_WRAPPER_TIMEOUT contract at the top of this file, a
    # lane needing more sets it before its configure_file and puts it back.
    if(sby_prop STREQUAL "field_seq_bound")
      set(ZHAO_FORMAL_WRAPPER_TIMEOUT 12000)
    endif()
    configure_file(formal/mem_formal_lane.cmake.in
                   formal/${sby_prop}_lane.cmake @ONLY)
    if(sby_prop STREQUAL "field_seq_bound")
      set(ZHAO_FORMAL_WRAPPER_TIMEOUT 1800)
    endif()
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

  # sat_add ALSO carries `fast`. Every other property in this lane is a heavy
  # prover kept to nightly; this one is purely combinational at depth 1 and
  # measures about a second, so there is no reason the cheapest lane should not
  # run it. It is also the ONLY evidence the saturating-counter law has: the arm
  # it proves is unreachable in simulation (u64 counters, ~2^64 events to the
  # rail), and the mutant that deletes the headroom test passes cmd_dma_directed
  # AND its 5,000-packet random lane while this proof fails it. Leaving it in
  # nightly alone would mean the law is unguarded in the lane people watch.
  set_tests_properties(formal_sat_add PROPERTIES LABELS "formal;nightly;fast")

  # CTest TIMEOUT must sit ABOVE the wrapper budget so the wrapper fails first
  # with a readable sby message rather than being killed from outside -- the
  # rule the ZHAO_FORMAL_WRAPPER_TIMEOUT comment records from
  # formal_texture_bilerp, where CTest 3600 against wrapper 1800 produced a
  # bare FAILED at 1,800.88 s.
  set_tests_properties(formal_field_seq_bound PROPERTIES TIMEOUT 12600)
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
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv
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
                   # zhao_scanout_linebuf instantiates the shared inferred-RAM
                   # primitive; harmless for the modules that do not.
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv
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
# Must match zhao_texture_tmu.sv's FILT_LANES default. The throughput case
# asserts the exact cycle count the parameter implies, so a drift between the
# two fails the lane rather than silently measuring the wrong shape.
target_compile_definitions(test_texture_tmu_directed PRIVATE ZHAO_FILT_LANES=2)
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

# THE FILT_LANES FRONTIER — 4 and 1 beside the default 2.
#
# zhao_texture_tmu time-multiplexes its four colour channels through
# FILT_LANES instances of zhao_texture_bilerp, in 4/FILT_LANES passes. The
# SAMPLES ARE BIT-IDENTICAL at every setting -- the same directed suite runs
# against the same oracle -- so these two targets are not extra behaviour.
# They are COVERAGE, and the reason is structural. At FILT_LANES = 4 there is
# exactly ONE pass, so `pass_c` is the constant PASSES-1, `sel_base` is
# constantly zero, ST_FILT is unreachable and the channel mux degenerates to a
# wire: six of the thirty sweep mutants are textually live and behaviourally
# INVISIBLE there. At FILT_LANES = 1 the pass counter runs its full range, and
# one mutant (LAST_FILT_PASS forced to 0) is invisible at BOTH 4 and 2 and a
# real defect only at 1. Neither end can be dropped.
#
# zhao_surface_sq's S03/S04 were the identical shape on SQ_RADIX, and the
# SURFACE.STAMP sweep would have scored them as survivors and sent someone
# hunting a test gap that was not there.
add_executable(test_texture_tmu_lanes4 texture/texture_tmu_directed.cpp)
target_link_libraries(test_texture_tmu_lanes4 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_texture_tmu_lanes4 PRIVATE ZHAO_FILT_LANES=4)
target_include_directories(test_texture_tmu_lanes4 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_tmu_lanes4
  PREFIX Vzhao_texture_tmu
  TOP_MODULE zhao_texture_tmu
  VERILATOR_ARGS -GFILT_LANES=4
  SOURCES ${ZHAO_TEXTURE_TMU_SV})
add_test(NAME texture_tmu_lanes4 COMMAND test_texture_tmu_lanes4)
set_tests_properties(texture_tmu_lanes4 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_texture_tmu_lanes1 texture/texture_tmu_directed.cpp)
target_link_libraries(test_texture_tmu_lanes1 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_texture_tmu_lanes1 PRIVATE ZHAO_FILT_LANES=1)
target_include_directories(test_texture_tmu_lanes1 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_tmu_lanes1
  PREFIX Vzhao_texture_tmu
  TOP_MODULE zhao_texture_tmu
  VERILATOR_ARGS -GFILT_LANES=1
  SOURCES ${ZHAO_TEXTURE_TMU_SV})
add_test(NAME texture_tmu_lanes1 COMMAND test_texture_tmu_lanes1)
set_tests_properties(texture_tmu_lanes1 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

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
  # 3300, INSIDE the 3600 CTest property below so the wrapper still fails with a
  # readable sby message before CTest kills it from outside. The two used to
  # disagree -- CTest 3600, wrapper 1800 -- and on 2026-08-23 this lane died at
  # 1,800.88 s under a loaded machine and was reported as a FAILED proof. It was
  # not a failed proof. The cover task had already passed in 2 s; the bmc task
  # was still solving.
  set(ZHAO_FORMAL_WRAPPER_TIMEOUT 3300)
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/texture_bilerp_lane.cmake @ONLY)
  set(ZHAO_FORMAL_WRAPPER_TIMEOUT 1800)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/texture_bilerp_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_texture_bilerp
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  # 3600, not the 2100 the other formal lanes use: MEASURED, not guessed. The
  # bmc task closes in 741 s standalone on this kit (boolector, depth 2) and
  # 1,264 s as this lane, because the partition-of-unity identity is nonlinear
  # over four bit-blasted 9x9 multipliers.
  #
  # AND THE WRAPPER HAS TO AGREE, which it did not until 2026-08-23: this
  # property said 3600 while formal/mem_formal_lane.cmake.in hard-coded 1800,
  # so the real budget was 1800 and the comment above described a headroom the
  # lane did not have. See ZHAO_FORMAL_WRAPPER_TIMEOUT at the top of this file.
  set_tests_properties(formal_texture_bilerp PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 3600
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---- Phase 5: the GEOMETRY FRONT — GEOM.CLIP / GEOM.SETUP / GEOM.BINNER ----
# ---- (charter phase 5 build order 14/15 "screen-space triangle setup in
# ---- FPGA" + "tile binner in FPGA", and item 3 "chunked tile lists and safe
# ---- overflow"; ZH-056 / ZH-057 / ZH-058 / ZH-026). Law spec/qformats.md 8;
# ---- contracts design/contracts/GEOM.CLIP.md / GEOM.SETUP.md / GEOM.BINNER.md
# The oracles are zref::Clip, zref::Setup and zref::Binner. None of them
# restates a law rast.cpp already owns: the 2A cross product is
# zref::EdgeWalk::area2 (which IS rast.cpp's orient()), and the scissored
# pixel-centre scan box is zref::render::scan_bbox — the very function
# raster_tri calls, EXTRACTED from it for this increment so the bbox law has
# one site. The binning law itself (grid anchor, enumeration order, the affine
# corner trivial-reject, the overflow wall) is CHOSEN, not found; it is argued
# in fpga/rtl/geometry/zhao_geom_binner.sv and recorded in the contract.
set(ZHAO_GEOM_CLIP_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clip.sv)
set(ZHAO_GEOM_SETUP_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_setup.sv)
set(ZHAO_GEOM_BINNER_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_arena.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_binner.sv)

add_executable(test_geom_clip_directed geometry/geom_clip_directed.cpp)
target_link_libraries(test_geom_clip_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_clip_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_clip_directed
  PREFIX Vzhao_geom_clip
  TOP_MODULE zhao_geom_clip
  SOURCES ${ZHAO_GEOM_CLIP_SV})
add_test(NAME geom_clip_directed COMMAND test_geom_clip_directed)
set_tests_properties(geom_clip_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_geom_clip_random geometry/geom_clip_random.cpp)
target_link_libraries(test_geom_clip_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_clip_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_clip_random
  PREFIX Vzhao_geom_clip
  TOP_MODULE zhao_geom_clip
  SOURCES ${ZHAO_GEOM_CLIP_SV})
add_test(NAME geom_clip_random COMMAND test_geom_clip_random)
set_tests_properties(geom_clip_random PROPERTIES LABELS "fast;nightly")
# 15x soak of the same binary
add_test(NAME geom_clip_random_nightly COMMAND test_geom_clip_random --nightly)
set_tests_properties(geom_clip_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

add_executable(test_geom_setup_directed geometry/geom_setup_directed.cpp)
target_link_libraries(test_geom_setup_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_setup_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_setup_directed
  PREFIX Vzhao_geom_setup
  TOP_MODULE zhao_geom_setup
  SOURCES ${ZHAO_GEOM_SETUP_SV})
add_test(NAME geom_setup_directed COMMAND test_geom_setup_directed)
set_tests_properties(geom_setup_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_geom_setup_random geometry/geom_setup_random.cpp)
target_link_libraries(test_geom_setup_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_setup_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_setup_random
  PREFIX Vzhao_geom_setup
  TOP_MODULE zhao_geom_setup
  SOURCES ${ZHAO_GEOM_SETUP_SV})
add_test(NAME geom_setup_random COMMAND test_geom_setup_random)
set_tests_properties(geom_setup_random PROPERTIES LABELS "fast;nightly")
add_test(NAME geom_setup_random_nightly COMMAND test_geom_setup_random --nightly)
set_tests_properties(geom_setup_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# GEOM.WCACHE -- the projected-vertex arena. Differential against
# zref::geom::VertexArena, whose refusal semantics are the interesting half.
add_executable(test_geom_wcache_directed geometry/geom_wcache_directed.cpp)
target_link_libraries(test_geom_wcache_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_wcache_directed
  PREFIX Vzhao_vertex_arena
  TOP_MODULE zhao_vertex_arena
  VERILATOR_ARGS -GARENAS=2 -GDEPTH=16 -GPAYLOAD_W=64 -GGEN_W=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
add_test(NAME geom_wcache_directed COMMAND test_geom_wcache_directed)
set_tests_properties(geom_wcache_directed PROPERTIES LABELS "fast;nightly")

# formal: the arena bounds law -- a lookup must never wrap into another vertex.
# REGISTERED 2026-08-27, and it was deliberately unregistered before that: the
# proof failed for three days and the notes in design/formal_runs.yml said so
# rather than quietly shipping a lane nobody ran. It now passes by `prove`
# (abc pdr), an INDUCTIVE result that holds at every depth, plus `cover` for
# non-vacuity. See tests/formal/geom_wcache_arena_bounds.sby.
if(ZHAO_SBY_BIN)
  set(PROP "geom_wcache_arena_bounds")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/geom_wcache_arena_bounds_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/geom_wcache_arena_bounds_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_geom_wcache_arena_bounds
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_geom_wcache_arena_bounds PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

add_executable(test_geom_binner_directed geometry/geom_binner_directed.cpp)
target_link_libraries(test_geom_binner_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_binner_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_binner_directed
  PREFIX Vzhao_geom_binner
  TOP_MODULE zhao_geom_binner
  SOURCES ${ZHAO_GEOM_BINNER_SV})
add_test(NAME geom_binner_directed COMMAND test_geom_binner_directed)
set_tests_properties(geom_binner_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_geom_binner_random geometry/geom_binner_random.cpp)
target_link_libraries(test_geom_binner_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_binner_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_binner_random
  PREFIX Vzhao_geom_binner
  TOP_MODULE zhao_geom_binner
  SOURCES ${ZHAO_GEOM_BINNER_SV})
add_test(NAME geom_binner_random COMMAND test_geom_binner_random)
set_tests_properties(geom_binner_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_binner_random_nightly COMMAND test_geom_binner_random --nightly)
set_tests_properties(geom_binner_random_nightly PROPERTIES
  LABELS "nightly" TIMEOUT 3600)

# ---------------------------------------------------------------------------
# TERRAIN.NORMALS (phase 6, ZH-038)
#
# The oracle is a thin view onto shade_flat_tri's ratified cross product
# (reference/src/zrender/terrain.cpp), so RTL == oracle means RTL == the law the
# golden captures already pin. The Q-format shift is 16, NOT 32: at 32 a
# sub-metre lattice shades solid black, a defect this project already paid for
# and which terrain_normals_directed pins against.
set(ZHAO_TERRAIN_NORMALS_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normals.sv)

add_executable(test_terrain_normals_directed terrain/terrain_normals_directed.cpp)
target_link_libraries(test_terrain_normals_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_normals_directed
  PREFIX Vzhao_terrain_normals
  TOP_MODULE zhao_terrain_normals
  SOURCES ${ZHAO_TERRAIN_NORMALS_SV})
add_test(NAME terrain_normals_directed COMMAND test_terrain_normals_directed)
set_tests_properties(terrain_normals_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_terrain_normals_random terrain/terrain_normals_random.cpp)
target_link_libraries(test_terrain_normals_random PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_normals_random
  PREFIX Vzhao_terrain_normals
  TOP_MODULE zhao_terrain_normals
  SOURCES ${ZHAO_TERRAIN_NORMALS_SV})
add_test(NAME terrain_normals_random COMMAND test_terrain_normals_random)
set_tests_properties(terrain_normals_random PROPERTIES LABELS "fast;nightly")
add_test(NAME terrain_normals_random_nightly COMMAND test_terrain_normals_random --nightly)
set_tests_properties(terrain_normals_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_terrain_normals
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_normals
                 ${ZHAO_TERRAIN_NORMALS_SV})
set_tests_properties(lint_terrain_normals PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.PATCH (phase 6, ZH-033)
#
# The Mantle entry point: terrain_rules 3.4's per-vertex composition chain and
# 9.1's frozen live-field intake (MAX_PATCH_FIELDS = 16, reject the tail, never
# evict). The oracle is a THIN VIEW onto compose_lattice, and
# terrain_patch_directed proves that by composing a real 33x33 dual patch with
# two real earth programs both ways and requiring all 1,089 vertices to agree —
# without that cross-check the oracle would be a second implementation of 3.4
# and charter 29-6 would be broken before the RTL was written.
# ---- GEOM.MESHFETCH: the creature representation ladder --------------------
# The LOD third of GEOM.MESHFETCH, and the only third with a law to implement.
# `zref::MeshFetch` is a phantom (reports/PHANTOM_REFERENCES.md) but the ladder
# is not: `zref::creature::lod_raw` / `lod_update` are shipped and are what the
# reference simulation itself calls, so this is a real differential rather than
# two implementations written by the same hand agreeing with each other.
#
# The RTL does not transcribe the reference -- it removes both of the
# reference's divides with an exact integer identity -- so the differential is
# checking an ALGEBRAIC REWRITE, not a retyping. That is why the directed lane
# crowds the legality edge, the rounding half-step and the 9/11 hysteresis seam
# rather than sampling uniformly.
add_executable(test_geom_lod_directed differential/geom_lod_directed.cpp)
target_link_libraries(test_geom_lod_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_lod_directed
  PREFIX Vzhao_geom_lod
  TOP_MODULE zhao_geom_lod
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_lod.sv)
add_test(NAME geom_lod_directed COMMAND test_geom_lod_directed)
set_tests_properties(geom_lod_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_lod_random COMMAND test_geom_lod_directed --random 4000)
set_tests_properties(geom_lod_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_lod_random_nightly COMMAND test_geom_lod_directed --random 200000)
set_tests_properties(geom_lod_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# ---- GEOM.MESHFETCH: the conservative frustum cull -------------------------
# The SECOND third of GEOM.MESHFETCH, ruled by the owner on 2026-08-22 when
# "camera visibility sectors" was deleted from the purpose line. The remaining
# third -- the descriptor fetch -- has no format yet and is not here.
#
# `zref::cull` is written for this and is therefore the law (kind 2), which is
# a weaker position than the LOD ladder's shipped oracle. It is shored up two
# ways. The CAMERA half is kind 1: sections 2/3 pin the five planes to
# `zref::render::project_vertex`, `mat4_vec4` and `fx_div_exact` -- the shipped
# renderer -- so the planes cannot describe a volume the renderer disagrees
# with. The SPHERE half is defended by the direction of its rounding, and
# section 5 measures that the test can actually distinguish the ceiling bound
# from the floor rather than assuming a boundary was reached.
#
# reference/src for zrender/internal.hpp: white-box access to project_vertex,
# the same access geom_project_directed already takes.
add_executable(test_geom_cull_directed differential/geom_cull_directed.cpp)
target_link_libraries(test_geom_cull_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_cull_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_geom_cull_directed
  PREFIX Vzhao_geom_cull
  TOP_MODULE zhao_geom_cull
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_cull.sv)
add_test(NAME geom_cull_directed COMMAND test_geom_cull_directed)
set_tests_properties(geom_cull_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_cull_random COMMAND test_geom_cull_directed --random 4000)
set_tests_properties(geom_cull_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME geom_cull_random_nightly COMMAND test_geom_cull_directed --random 120000)
set_tests_properties(geom_cull_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_zhao_geom_cull
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_cull
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_cull.sv)
set_tests_properties(lint_zhao_geom_cull PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

set(ZHAO_TERRAIN_PATCH_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch.sv)

# FIELD.OUT.HEIGHT: the op ops.yml declares with "no dedicated opcode" -- the op
# IS the routing of the height out-lane into the §3.4 compose chain. Narrow on
# purpose: terrain_patch_directed.cpp already covers composition as a whole, so
# this covers only what the op owns (clamp-once, command order, the exact <<8,
# and the covers gate).
add_executable(test_field_out_height differential/field_out_height.cpp)
target_link_libraries(test_field_out_height PRIVATE zhao_harness zhao_zref)
verilate(test_field_out_height
  PREFIX Vzhao_terrain_patch
  TOP_MODULE zhao_terrain_patch
  SOURCES ${ZHAO_TERRAIN_PATCH_SV})
add_test(NAME field_out_height COMMAND test_field_out_height)
set_tests_properties(field_out_height PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_out_height_random COMMAND test_field_out_height --random 300)
set_tests_properties(field_out_height_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_out_height_random_nightly COMMAND test_field_out_height --random 4000)
set_tests_properties(field_out_height_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_terrain_patch_directed terrain/terrain_patch_directed.cpp)
target_link_libraries(test_terrain_patch_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_patch_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_terrain_patch_directed
  PREFIX Vzhao_terrain_patch
  TOP_MODULE zhao_terrain_patch
  SOURCES ${ZHAO_TERRAIN_PATCH_SV})
add_test(NAME terrain_patch_directed COMMAND test_terrain_patch_directed)
set_tests_properties(terrain_patch_directed PROPERTIES LABELS "fast;nightly")

add_executable(test_terrain_patch_random terrain/terrain_patch_random.cpp)
target_link_libraries(test_terrain_patch_random PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_patch_random
  PREFIX Vzhao_terrain_patch
  TOP_MODULE zhao_terrain_patch
  SOURCES ${ZHAO_TERRAIN_PATCH_SV})
add_test(NAME terrain_patch_random COMMAND test_terrain_patch_random)
set_tests_properties(terrain_patch_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME terrain_patch_random_nightly COMMAND test_terrain_patch_random --nightly)
set_tests_properties(terrain_patch_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_test(NAME lint_terrain_patch
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_patch
                 ${ZHAO_TERRAIN_PATCH_SV})
set_tests_properties(lint_terrain_patch PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.TESS (phase 6, ZH-034)
#
# The unstitched path is the reference path: terrain_rules 4.3's emit order
# verbatim, so "RTL == oracle" is "RTL == the geometry the golden captures pin".
# The stitched path is an annulus whose outer boundary uses ONLY the per-side
# coarse vertices, which makes the shared-edge vertex sets identical by
# construction — the contract's crack invariant, checked on real emitted
# geometry in terrain_tess_directed rather than argued in prose.
set(ZHAO_TERRAIN_TESS_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tess.sv)

add_executable(test_terrain_tess_directed terrain/terrain_tess_directed.cpp)
target_link_libraries(test_terrain_tess_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_tess_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_tess_directed
  PREFIX Vzhao_terrain_tess
  TOP_MODULE zhao_terrain_tess
  SOURCES ${ZHAO_TERRAIN_TESS_SV})
add_test(NAME terrain_tess_directed COMMAND test_terrain_tess_directed)
set_tests_properties(terrain_tess_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_tess_random terrain/terrain_tess_random.cpp)
target_link_libraries(test_terrain_tess_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_tess_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_tess_random
  PREFIX Vzhao_terrain_tess
  TOP_MODULE zhao_terrain_tess
  SOURCES ${ZHAO_TERRAIN_TESS_SV})
add_test(NAME terrain_tess_random COMMAND test_terrain_tess_random)
set_tests_properties(terrain_tess_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME terrain_tess_random_nightly COMMAND test_terrain_tess_random --nightly)
set_tests_properties(terrain_tess_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_terrain_tess
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_tess
                 ${ZHAO_TERRAIN_TESS_SV})
set_tests_properties(lint_terrain_tess PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.TESS -> TERRAIN.NORMALS composition (phase 6)
#
# Real tessellated cells drive the real normals block. Composing GEOM.BINNER
# with the real rasterizer immediately exposed a 16x tile-index-versus-pixel
# error no isolated test could see; this is the same move for the Mantle chain.
add_executable(test_terrain_tess_normals terrain/terrain_tess_normals.cpp)
target_link_libraries(test_terrain_tess_normals PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_tess_normals PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_tess_normals
  PREFIX Vzhao_terrain_tess
  TOP_MODULE zhao_terrain_tess
  SOURCES ${ZHAO_TERRAIN_TESS_SV})
verilate(test_terrain_tess_normals
  PREFIX Vzhao_terrain_normals
  TOP_MODULE zhao_terrain_normals
  SOURCES ${ZHAO_TERRAIN_NORMALS_SV})
add_test(NAME terrain_tess_normals COMMAND test_terrain_tess_normals)
set_tests_properties(terrain_tess_normals PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# TERRAIN.PROJECT (phase 6, ZH-051)
#
# The oracle is the SHIPPED function: reference/src/zrender/rast.cpp's
# project_vertex, which every golden capture in this tree was drawn with. No
# model of it is written anywhere in this lane, so "RTL == oracle" is "RTL ==
# the pictures the project already ships". White-box include of
# reference/src is what lets the test call it.
set(ZHAO_TERRAIN_PROJECT_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_project.sv
    ${ZHAO_PROJECT_CORE_SV})

add_executable(test_terrain_project_directed terrain/terrain_project_directed.cpp)
target_link_libraries(test_terrain_project_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_project_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_terrain_project_directed
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_project_directed COMMAND test_terrain_project_directed)
set_tests_properties(terrain_project_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_project_random terrain/terrain_project_random.cpp)
target_link_libraries(test_terrain_project_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_project_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_project_random
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_project_random COMMAND test_terrain_project_random)
set_tests_properties(terrain_project_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME terrain_project_random_nightly COMMAND test_terrain_project_random --nightly)
set_tests_properties(terrain_project_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_terrain_project
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_project
                 ${ZHAO_TERRAIN_PROJECT_SV})
set_tests_properties(lint_terrain_project PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.LOD (phase 6, ZH-050)
#
# The oracle here is MOSTLY DEFINITION, not view: no ratified terrain LOD
# arithmetic exists (charter 11.5 lists ingredients, 9 lists required stability
# properties, MEASURE.GOVERNOR's contract is a stub), so zref_terrain_lod.hpp
# states a law and says at every line that it is choosing one. The directed
# lane therefore checks the law's OWN properties by hand as well as agreement
# with the oracle.
set(ZHAO_TERRAIN_LOD_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lod.sv)

add_executable(test_terrain_lod_directed terrain/terrain_lod_directed.cpp)
target_link_libraries(test_terrain_lod_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_lod_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_lod_directed
  PREFIX Vzhao_terrain_lod
  TOP_MODULE zhao_terrain_lod
  SOURCES ${ZHAO_TERRAIN_LOD_SV})
add_test(NAME terrain_lod_directed COMMAND test_terrain_lod_directed)
set_tests_properties(terrain_lod_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_lod_random terrain/terrain_lod_random.cpp)
target_link_libraries(test_terrain_lod_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_lod_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_lod_random
  PREFIX Vzhao_terrain_lod
  TOP_MODULE zhao_terrain_lod
  SOURCES ${ZHAO_TERRAIN_LOD_SV})
add_test(NAME terrain_lod_random COMMAND test_terrain_lod_random)
set_tests_properties(terrain_lod_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME terrain_lod_random_nightly COMMAND test_terrain_lod_random --nightly)
set_tests_properties(terrain_lod_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_terrain_lod
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_lod
                 ${ZHAO_TERRAIN_LOD_SV})
set_tests_properties(lint_terrain_lod PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.LOD -> TERRAIN.TESS composition (phase 6)
#
# The decisions drive the real tessellator. TERRAIN.LOD's output packet IS
# zhao_terrain_tess's job port, so the two are wired with no adapter and the
# stitch law is checked on the geometry the pair actually emits.
add_executable(test_terrain_lod_tess terrain/terrain_lod_tess.cpp)
target_link_libraries(test_terrain_lod_tess PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_lod_tess PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_lod_tess
  PREFIX Vzhao_terrain_lod
  TOP_MODULE zhao_terrain_lod
  SOURCES ${ZHAO_TERRAIN_LOD_SV})
verilate(test_terrain_lod_tess
  PREFIX Vzhao_terrain_tess
  TOP_MODULE zhao_terrain_tess
  SOURCES ${ZHAO_TERRAIN_TESS_SV})
add_test(NAME terrain_lod_tess COMMAND test_terrain_lod_tess)
set_tests_properties(terrain_lod_tess PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# TERRAIN.PROJECT -> GEOM.CLIP -> GEOM.SETUP -> the real rasterizer (phase 6)
#
# Four Verilated models wired to each other with no adapter: real tessellated
# terrain goes in as fx16 world vertices and RGB565 framebuffer words come out.
# Composing GEOM.BINNER with the rasterizer exposed a silent 16x tile-index
# error no isolated test could see; TERRAIN.PROJECT -> GEOM.CLIP is the seam
# this increment created, and nothing else checks that the block which CLAMPS
# to the guard band and the block which ASSUMES the clamp agree on 21 bits.
#
# Registered after ZHAO_GEOM_BIN_PIPE_SV is defined; see below.

# per-module lint (charter 21 step 8)
add_test(NAME lint_geom_clip
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_clip
                 ${ZHAO_GEOM_CLIP_SV})
set_tests_properties(lint_geom_clip PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_geom_setup
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_setup
                 ${ZHAO_GEOM_SETUP_SV})
set_tests_properties(lint_geom_setup PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_geom_binner
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_binner
                 ${ZHAO_GEOM_BINNER_SV})
set_tests_properties(lint_geom_binner PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- the composition: GEOM.BINNER -> zhao_raster_tile_pipe --------------
# REAL TILE LISTS DRIVING THE REAL RASTERIZER. Not a ledger block (the
# GEOMETRY group has no entry for "the composition", exactly as the RASTER
# group has none for zhao_raster_tile_pipe); the rationale and the two laws it
# owns live in fpga/rtl/geometry/zhao_geom_bin_pipe.sv. Verilator only: not in
# fpga/files.qip, no Quartus fit, no capture, never on hardware.
set(ZHAO_GEOM_BIN_PIPE_SV
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_arena.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_binner.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_bin_pipe.sv)

add_executable(test_geom_bin_pipe_directed geometry/geom_bin_pipe_directed.cpp)
target_link_libraries(test_geom_bin_pipe_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_bin_pipe_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_geom_bin_pipe_directed
  PREFIX Vzhao_geom_bin_pipe
  TOP_MODULE zhao_geom_bin_pipe
  SOURCES ${ZHAO_GEOM_BIN_PIPE_SV})
add_test(NAME geom_bin_pipe_directed COMMAND test_geom_bin_pipe_directed)
set_tests_properties(geom_bin_pipe_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_project_chain terrain/terrain_project_chain.cpp)
target_link_libraries(test_terrain_project_chain PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_project_chain PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_terrain_project_chain
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
verilate(test_terrain_project_chain
  PREFIX Vzhao_geom_clip
  TOP_MODULE zhao_geom_clip
  SOURCES ${ZHAO_GEOM_CLIP_SV})
verilate(test_terrain_project_chain
  PREFIX Vzhao_geom_setup
  TOP_MODULE zhao_geom_setup
  SOURCES ${ZHAO_GEOM_SETUP_SV})
verilate(test_terrain_project_chain
  PREFIX Vzhao_geom_bin_pipe
  TOP_MODULE zhao_geom_bin_pipe
  SOURCES ${ZHAO_GEOM_BIN_PIPE_SV})
add_test(NAME terrain_project_chain COMMAND test_terrain_project_chain)
set_tests_properties(terrain_project_chain PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_geom_bin_pipe
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_bin_pipe
                 ${ZHAO_GEOM_BIN_PIPE_SV})
set_tests_properties(lint_geom_bin_pipe PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the arena bound the ledger names (tests/formal/
# geom_binner_arena_bounds.sby), proved on zhao_geom_arena — the module
# zhao_geom_binner instantiates, so the proof and the silicon are the same
# bytes. TWO tasks (bmc AND cover): the covers pin the arena actually FILLING
# and the wall actually REFUSING, without which the bound would also hold for
# an allocator that never grants anything.
if(ZHAO_SBY_BIN)
  set(PROP "geom_binner_arena_bounds")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/geom_binner_arena_bounds_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/geom_binner_arena_bounds_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_geom_binner_arena_bounds
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_geom_binner_arena_bounds PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_cdc_snapshot.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_mode.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_framectl.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_scaler.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_scanout_fetch.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_debug_frameblit.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_slotmgr.sv
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

# ---------------------------------------------------------------------------
# SURFACE.SHEET (phase 6, ZH-031) and SURFACE.STAMP (phase 6, ZH-052)
#
# Layer F — the 64x64 {tag u8, strength u8} per-patch surface sheet of charter
# 12 — and the Scar Scribe stamp engine that is its only writer
# (spec/terrain_rules.md 7). The ratified law is
# `zref::render::stamp_surface`, executed today by the software console and
# pinned by tests/render/render_golden.cpp's crack ring; the oracle
# `zref/zref_surface.hpp` is a VIEW onto it and surface_stamp_directed proves
# the view faithful before anything else runs.
#
# surface_stamp_chain wires the two REAL blocks together and then reads the
# sheet back out through SURFACE.SHEET's own port into
# `zref::render::sample_sheet`, the draw-time consumer, whose world->texel
# mapping is independent of the stamp's.
# ---------------------------------------------------------------------------

set(ZHAO_SURFACE_SHEET_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sheet.sv)
set(ZHAO_SURFACE_BLEND_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_blend.sv)
set(ZHAO_SURFACE_STAMP_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_blend.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sq.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_stamp.sv)

# FIELD.WRITE.TAG, the op ops.yml declares and had no coverage for. Its cited
# reference_function zref::fieldir::sink_write_tag is one of the twenty-five
# phantoms; the law is real and lives in zref::surface::SheetStore, so the op is
# tested against that rather than against a second implementation.
# The three Earth sink composition laws. No verilate(): these are REFERENCE
# laws with no RTL yet -- FIELD.WRITE.MATERIAL/NAV/HAZARD had reference_function
# names in ops.yml that resolved to nothing until the owner ruling of
# 2026-08-24 supplied the behaviour. The RTL differential joins this file when
# the sink RTL exists; until then the law is pinned where it lives.
add_executable(test_field_write_earth_sinks differential/field_write_earth_sinks.cpp)
target_link_libraries(test_field_write_earth_sinks PRIVATE zhao_harness zhao_zref)
add_test(NAME field_write_earth_sinks COMMAND test_field_write_earth_sinks)
set_tests_properties(field_write_earth_sinks PROPERTIES LABELS "fast;nightly")

add_executable(test_field_write_tag differential/field_write_tag.cpp)
target_link_libraries(test_field_write_tag PRIVATE zhao_harness zhao_zref)
target_include_directories(test_field_write_tag PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface)
verilate(test_field_write_tag
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${ZHAO_SURFACE_SHEET_SV})
add_test(NAME field_write_tag COMMAND test_field_write_tag)
set_tests_properties(field_write_tag PROPERTIES LABELS "fast;nightly" TIMEOUT 3600)

# SURFACE.SHEET measured against its DECLARED oracle. surface_sheet_directed.cpp
# checks what the block promises about itself and never touches SheetStore --
# ledger V17 refused it as "an alias, not evidence", correctly. This runs the
# real RTL and the real zref::surface::SheetStore over one operation stream and
# requires them to agree on verdict, occupancy and every texel.
add_executable(test_surface_sheet_store_diff surface/surface_sheet_store_diff.cpp)
target_link_libraries(test_surface_sheet_store_diff PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_sheet_store_diff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface)
verilate(test_surface_sheet_store_diff
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${ZHAO_SURFACE_SHEET_SV})
add_test(NAME surface_sheet_store_diff COMMAND test_surface_sheet_store_diff)
set_tests_properties(surface_sheet_store_diff PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME surface_sheet_store_random COMMAND test_surface_sheet_store_diff --random 12)
set_tests_properties(surface_sheet_store_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME surface_sheet_store_random_nightly COMMAND test_surface_sheet_store_diff --random 150)
set_tests_properties(surface_sheet_store_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_surface_sheet_directed surface/surface_sheet_directed.cpp)
target_link_libraries(test_surface_sheet_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_sheet_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface)
verilate(test_surface_sheet_directed
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${ZHAO_SURFACE_SHEET_SV})
add_test(NAME surface_sheet_directed COMMAND test_surface_sheet_directed)
set_tests_properties(surface_sheet_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_surface_sheet_random surface/surface_sheet_random.cpp)
target_link_libraries(test_surface_sheet_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_sheet_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface)
verilate(test_surface_sheet_random
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${ZHAO_SURFACE_SHEET_SV})
add_test(NAME surface_sheet_random COMMAND test_surface_sheet_random)
set_tests_properties(surface_sheet_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME surface_sheet_random_nightly COMMAND test_surface_sheet_random --nightly)
set_tests_properties(surface_sheet_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 1800)

add_executable(test_surface_stamp_directed surface/surface_stamp_directed.cpp)
target_link_libraries(test_surface_stamp_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_stamp_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_surface_stamp_directed
  PREFIX Vzhao_surface_stamp
  TOP_MODULE zhao_surface_stamp
  SOURCES ${ZHAO_SURFACE_STAMP_SV})
add_test(NAME surface_stamp_directed COMMAND test_surface_stamp_directed)
set_tests_properties(surface_stamp_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# ---------------------------------------------------------------------------
# THE SQ_RADIX FRONTIER, BUILT AS COVERAGE AND NOT ONLY AS FITTED NUMBERS.
#
# zhao_surface_sq retires SQ_RADIX magnitude bits per cycle through a chain of
# SQ_RADIX conditional 64-bit adds. At the default SQ_RADIX = 1 that chain has
# EXACTLY ONE LINK, so `chain[b+1] = chain[b] + (bits_q[b] ? sh_q << b : 0)` is
# only ever instantiated at b = 0 -- the b > 0 arms of the generate, and every
# way of getting the per-bit place value wrong inside them, are NEVER
# ELABORATED by the default build.
#
# tools/sweep_geom_skin.sh learned this the expensive way on MUL_LANES and
# stated the rule: A PARAMETERISED BLOCK'S FRONTIER BUILDS ARE COVERAGE, NOT
# JUST DATA POINTS. These two targets exist so the mutation sweep can score
# every setting the parameter admits.
#
# The target names carry the setting LITERALLY, and SOURCES is spelled out
# rather than using ${ZHAO_SURFACE_STAMP_SV}: the sweep derives its consumer
# set out of this file, and a name or a path it cannot see is a target it
# cannot clean and re-score.
# The shared sequential squarer, checked WHERE ITS VALUE IS VISIBLE.
#
# zhao_surface_stamp reduces this module's output to one boolean —
# `covered = !(d2 > r_outer2 || d2 < r_inner2)` — and all three of those terms
# come out of the SAME instance, so a comparison cannot see a uniform scaling of
# it. The 2026-08-23 sweep proved that concretely: three mutants that double or
# halve the squarer survived the entire stamp suite. They are defects in the
# module, not in the stamp, and this is where a module is checked.
#
# Same argument zhao_surface_blend already won by being factored out. Built at
# all three SQ_RADIX settings, because the adder chain has one link at the
# default and its b > 0 arms are not elaborated there at all.
add_executable(test_surface_sq_directed surface/surface_sq_directed.cpp)
target_link_libraries(test_surface_sq_directed PRIVATE zhao_harness)
verilate(test_surface_sq_directed
  PREFIX Vzhao_surface_sq
  TOP_MODULE zhao_surface_sq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sq.sv)
add_test(NAME surface_sq_directed COMMAND test_surface_sq_directed)
set_tests_properties(surface_sq_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_surface_sq_radix4 surface/surface_sq_directed.cpp)
target_link_libraries(test_surface_sq_radix4 PRIVATE zhao_harness)
target_compile_definitions(test_surface_sq_radix4 PRIVATE ZHAO_SQ_RADIX=4)
verilate(test_surface_sq_radix4
  PREFIX Vzhao_surface_sq
  TOP_MODULE zhao_surface_sq
  VERILATOR_ARGS -GSQ_RADIX=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sq.sv)
add_test(NAME surface_sq_radix4 COMMAND test_surface_sq_radix4)
set_tests_properties(surface_sq_radix4 PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_surface_stamp_radix2 surface/surface_stamp_directed.cpp)
target_link_libraries(test_surface_stamp_radix2 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_surface_stamp_radix2 PRIVATE ZHAO_SQ_RADIX=2)
target_include_directories(test_surface_stamp_radix2 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_surface_stamp_radix2
  PREFIX Vzhao_surface_stamp
  TOP_MODULE zhao_surface_stamp
  VERILATOR_ARGS -GSQ_RADIX=2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_blend.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_stamp.sv)
add_test(NAME surface_stamp_radix2 COMMAND test_surface_stamp_radix2)
set_tests_properties(surface_stamp_radix2 PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_surface_stamp_radix4 surface/surface_stamp_directed.cpp)
target_link_libraries(test_surface_stamp_radix4 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_surface_stamp_radix4 PRIVATE ZHAO_SQ_RADIX=4)
target_include_directories(test_surface_stamp_radix4 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_surface_stamp_radix4
  PREFIX Vzhao_surface_stamp
  TOP_MODULE zhao_surface_stamp
  VERILATOR_ARGS -GSQ_RADIX=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_blend.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_stamp.sv)
add_test(NAME surface_stamp_radix4 COMMAND test_surface_stamp_radix4)
set_tests_properties(surface_stamp_radix4 PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_surface_stamp_random surface/surface_stamp_random.cpp)
target_link_libraries(test_surface_stamp_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_stamp_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface)
verilate(test_surface_stamp_random
  PREFIX Vzhao_surface_stamp
  TOP_MODULE zhao_surface_stamp
  SOURCES ${ZHAO_SURFACE_STAMP_SV})
add_test(NAME surface_stamp_random COMMAND test_surface_stamp_random)
set_tests_properties(surface_stamp_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME surface_stamp_random_nightly COMMAND test_surface_stamp_random --nightly)
set_tests_properties(surface_stamp_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_surface_stamp_chain surface/surface_stamp_chain.cpp)
target_link_libraries(test_surface_stamp_chain PRIVATE zhao_harness zhao_zref)
target_include_directories(test_surface_stamp_chain PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/surface
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_surface_stamp_chain
  PREFIX Vzhao_surface_stamp
  TOP_MODULE zhao_surface_stamp
  SOURCES ${ZHAO_SURFACE_STAMP_SV})
verilate(test_surface_stamp_chain
  PREFIX Vzhao_surface_sheet
  TOP_MODULE zhao_surface_sheet
  SOURCES ${ZHAO_SURFACE_SHEET_SV})
add_test(NAME surface_stamp_chain COMMAND test_surface_stamp_chain)
set_tests_properties(surface_stamp_chain PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_surface_sheet
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_surface_sheet
                 ${ZHAO_SURFACE_SHEET_SV})
set_tests_properties(lint_surface_sheet PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_surface_stamp
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_surface_stamp
                 ${ZHAO_SURFACE_STAMP_SV})
set_tests_properties(lint_surface_stamp PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_surface_blend
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_surface_blend
                 ${ZHAO_SURFACE_BLEND_SV})
set_tests_properties(lint_surface_blend PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the surface blend, proved TOTAL rather than bounded — the harness is
# combinational and its 22 free input bits are the whole input space of the
# module (mode 3, dst 8, src 8, age_shift 3 = 4,194,304 inputs). The cover task
# is load-bearing: without it the saturation theorems would also hold for a
# blend that never reached a rail. Reuses the hardened mem_formal_lane wrapper.
if(ZHAO_SBY_BIN)
  set(PROP "surface_blend")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/surface_blend_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/surface_blend_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_surface_blend
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_surface_blend PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---------------------------------------------------------------------------
# TERRAIN.BAKE (phase 7, ZH-036)
#
# The only writer of layers B and D (terrain_rules 7). The oracle is the
# EXECUTED reference — `zref::terrain::bake_dig` then
# `zref::terrain::apply_breach_law` — rather than a second implementation of
# 3.4, so "RTL == oracle" is "RTL == the deformation the software console
# already ships". The block owns its own 33x33 sweep, so the tests drive it as
# a MEMORY answering its read addresses, which is also what makes the scan
# order a checkable fact instead of an assumption.
set(ZHAO_TERRAIN_BAKE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake_delta.sv)

add_executable(test_terrain_bake_directed terrain/terrain_bake_directed.cpp)
target_link_libraries(test_terrain_bake_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_bake_directed
  PREFIX Vzhao_terrain_bake
  TOP_MODULE zhao_terrain_bake
  SOURCES ${ZHAO_TERRAIN_BAKE_SV})
add_test(NAME terrain_bake_directed COMMAND test_terrain_bake_directed)
set_tests_properties(terrain_bake_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_bake_random terrain/terrain_bake_random.cpp)
target_link_libraries(test_terrain_bake_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_bake_random
  PREFIX Vzhao_terrain_bake
  TOP_MODULE zhao_terrain_bake
  SOURCES ${ZHAO_TERRAIN_BAKE_SV})
add_test(NAME terrain_bake_random COMMAND test_terrain_bake_random)
set_tests_properties(terrain_bake_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME terrain_bake_random_nightly COMMAND test_terrain_bake_random --nightly)
set_tests_properties(terrain_bake_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# BAKE -> PATCH: the `baked_scars` seam, both blocks REAL. This is the first
# time layer B has had a producer in RTL, so it is the first time the Mantle
# entry point can be fed the plane its contract says TERRAIN.BAKE owns.
add_executable(test_terrain_bake_chain terrain/terrain_bake_chain.cpp)
target_link_libraries(test_terrain_bake_chain PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_chain PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_terrain_bake_chain
  PREFIX Vzhao_terrain_bake
  TOP_MODULE zhao_terrain_bake
  SOURCES ${ZHAO_TERRAIN_BAKE_SV})
verilate(test_terrain_bake_chain
  PREFIX Vzhao_terrain_patch
  TOP_MODULE zhao_terrain_patch
  SOURCES ${ZHAO_TERRAIN_PATCH_SV})
add_test(NAME terrain_bake_chain COMMAND test_terrain_bake_chain)
set_tests_properties(terrain_bake_chain PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_terrain_bake
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_bake
                 ${ZHAO_TERRAIN_BAKE_SV})
set_tests_properties(lint_terrain_bake PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_terrain_bake_delta
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_bake_delta
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake_delta.sv)
set_tests_properties(lint_terrain_bake_delta PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the incremental-scaling arithmetic, proved on the module
# zhao_terrain_bake actually instantiates. The deferral identity of
# terrain_rules 9.2 law 3 is the theorem the frozen BAKE_PATCH_BUDGET rests on,
# and it is proved here rather than sampled.
if(ZHAO_SBY_BIN)
  set(PROP "terrain_bake_delta")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/terrain_bake_delta_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/terrain_bake_delta_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_terrain_bake_delta
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_terrain_bake_delta PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---------------------------------------------------------------------------
# TERRAIN.VELOCITY (phase 7, ZH-039)
#
# The §4.2 velocity lattice: earth out-lane 1 accumulated at lattice vertices
# and baked back to height16. The oracle is cross-checked against the EXECUTED
# reference (`compose_lattice`'s recorded velocity samples, grouped by vertex
# in command order) before the RTL is compared to it, so "RTL == oracle" means
# "RTL == the velocity lane the software console already records". The block
# owns its own 33x33 sweep, so the tests drive it as a MEMORY answering the
# address it asks for — which is what makes the z-then-x scan order a checkable
# fact instead of an assumption.
set(ZHAO_TERRAIN_VELOCITY_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_velocity.sv)

# FIELD.OUT.VELOCITY: the op ops.yml declares, and TERRAIN.VELOCITY's only
# remaining blocker. Narrow on purpose -- each law it checks is a place the
# velocity lane behaves DIFFERENTLY from the height lane beside it (no clamp,
# one bake-back, skipped-not-zero, two separate saturation ledgers), which is
# what an implementer copying across gets wrong.
add_executable(test_field_out_velocity differential/field_out_velocity.cpp)
target_link_libraries(test_field_out_velocity PRIVATE zhao_harness zhao_zref)
target_include_directories(test_field_out_velocity PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_field_out_velocity
  PREFIX Vzhao_terrain_velocity
  TOP_MODULE zhao_terrain_velocity
  SOURCES ${ZHAO_TERRAIN_VELOCITY_SV})
add_test(NAME field_out_velocity COMMAND test_field_out_velocity)
set_tests_properties(field_out_velocity PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_out_velocity_random COMMAND test_field_out_velocity --random 6)
set_tests_properties(field_out_velocity_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME field_out_velocity_random_nightly COMMAND test_field_out_velocity --random 60)
set_tests_properties(field_out_velocity_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_terrain_velocity_directed terrain/terrain_velocity_directed.cpp)
target_link_libraries(test_terrain_velocity_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_velocity_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_CURRENT_SOURCE_DIR}/render      # white-box: rtest::make_earth_prog
  ${CMAKE_SOURCE_DIR}/reference/src)      # white-box: zrender/internal.hpp
verilate(test_terrain_velocity_directed
  PREFIX Vzhao_terrain_velocity
  TOP_MODULE zhao_terrain_velocity
  SOURCES ${ZHAO_TERRAIN_VELOCITY_SV})
add_test(NAME terrain_velocity_directed COMMAND test_terrain_velocity_directed)
set_tests_properties(terrain_velocity_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_velocity_random terrain/terrain_velocity_random.cpp)
target_link_libraries(test_terrain_velocity_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_velocity_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_velocity_random
  PREFIX Vzhao_terrain_velocity
  TOP_MODULE zhao_terrain_velocity
  SOURCES ${ZHAO_TERRAIN_VELOCITY_SV})
add_test(NAME terrain_velocity_random COMMAND test_terrain_velocity_random)
set_tests_properties(terrain_velocity_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME terrain_velocity_random_nightly COMMAND test_terrain_velocity_random --nightly)
set_tests_properties(terrain_velocity_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# PATCH -> VELOCITY: the `fld_covers_o` seam, both blocks REAL. TERRAIN.PATCH
# owns the §9.1 field list and decides the closed-interval footprint test once;
# TERRAIN.VELOCITY consumes that answer for out-lane 1. Driving both from ONE
# field list is the only way the seam is proved rather than asserted, and it is
# also where the wake workload (one moving field lane, every frame) is measured
# end to end.
add_executable(test_terrain_velocity_chain terrain/terrain_velocity_chain.cpp)
target_link_libraries(test_terrain_velocity_chain PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_velocity_chain PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_CURRENT_SOURCE_DIR}/render
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_velocity_chain
  PREFIX Vzhao_terrain_velocity
  TOP_MODULE zhao_terrain_velocity
  SOURCES ${ZHAO_TERRAIN_VELOCITY_SV})
verilate(test_terrain_velocity_chain
  PREFIX Vzhao_terrain_patch
  TOP_MODULE zhao_terrain_patch
  SOURCES ${ZHAO_TERRAIN_PATCH_SV})
add_test(NAME terrain_velocity_chain COMMAND test_terrain_velocity_chain)
set_tests_properties(terrain_velocity_chain PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_terrain_velocity
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_velocity
                 ${ZHAO_TERRAIN_VELOCITY_SV})
set_tests_properties(lint_terrain_velocity PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# MEASURE.TOKENS (phase 8, ZH-048)
#
# The global token guard and the charter §9 Duo fairness split. The grant is
# COMBINATIONAL because GEOM.BINNER's law E already committed to that seam in
# phase 6 ("`tok_grant_i` is sampled on that same edge"); the ledger's
# `latency: fixed:1` is honoured by the registered `token_denial`, and the
# directed lane MEASURES both rather than asserting them. The oracle is a first
# implementation, not a view onto an executed reference — nothing in
# reference/src allocates tokens — so the contract carries every law it makes.
set(ZHAO_MEASURE_TOKENS_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_tokens.sv)

add_executable(test_measure_tokens_directed measure/measure_tokens_directed.cpp)
target_link_libraries(test_measure_tokens_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_tokens_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_tokens_directed
  PREFIX Vzhao_measure_tokens
  TOP_MODULE zhao_measure_tokens
  SOURCES ${ZHAO_MEASURE_TOKENS_SV})
add_test(NAME measure_tokens_directed COMMAND test_measure_tokens_directed)
set_tests_properties(measure_tokens_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_measure_tokens_random measure/measure_tokens_random.cpp)
target_link_libraries(test_measure_tokens_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_tokens_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_tokens_random
  PREFIX Vzhao_measure_tokens
  TOP_MODULE zhao_measure_tokens
  SOURCES ${ZHAO_MEASURE_TOKENS_SV})
add_test(NAME measure_tokens_random COMMAND test_measure_tokens_random)
set_tests_properties(measure_tokens_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME measure_tokens_random_nightly COMMAND test_measure_tokens_random --nightly)
set_tests_properties(measure_tokens_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_test(NAME lint_measure_tokens
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_measure_tokens
                 ${ZHAO_MEASURE_TOKENS_SV})
set_tests_properties(lint_measure_tokens PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The Duo fairness guarantee, proved on the SHIPPING module (ZH-048;
# design/blocks.yml names measure_tokens_fairness.sby as this block's formal
# lane, and this is it populated rather than promised). TWO tasks: `prove`
# (temporal induction — the guarantee holds for ALL time, not for a bounded
# prefix) AND `cover`, on a deliberately tiny TOK_W=4 second instance, because
# without the covers every property would also hold for a guard that never
# granted anything. Arithmetic-flat by design: five 32-bit compare/add/subtract
# lanes, no divider, nothing unrolled.
if(ZHAO_SBY_BIN)
  set(PROP "measure_tokens_fairness")
  set(FORMAL_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/formal-work")
  set(FORMAL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/formal")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/measure_tokens_fairness_lane.cmake @ONLY)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/measure_tokens_fairness_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_measure_tokens_fairness
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  set_tests_properties(formal_measure_tokens_fairness PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 2100
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---------------------------------------------------------------------------
# MEASURE.GOVERNOR (phase 8, ZH-047)
#
# The per-camera screen-error policy and per-view degradation. Charter §9
# Version 1 puts the THRESHOLD PREDICTION on the ARM, so this block converts
# (two camera numbers -> the ONE ratio TERRAIN.LOD's Notes law 8 already
# committed to consuming) and degrades. The division lives here because
# TERRAIN.LOD explicitly refused to carry it, and it costs one 33-step
# restoring divide per camera per frame.
set(ZHAO_MEASURE_GOVERNOR_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_governor.sv)

add_executable(test_measure_governor_directed measure/measure_governor_directed.cpp)
target_link_libraries(test_measure_governor_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_governor_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_governor_directed
  PREFIX Vzhao_measure_governor
  TOP_MODULE zhao_measure_governor
  SOURCES ${ZHAO_MEASURE_GOVERNOR_SV})
add_test(NAME measure_governor_directed COMMAND test_measure_governor_directed)
set_tests_properties(measure_governor_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_measure_governor_random measure/measure_governor_random.cpp)
target_link_libraries(test_measure_governor_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_governor_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_governor_random
  PREFIX Vzhao_measure_governor
  TOP_MODULE zhao_measure_governor
  SOURCES ${ZHAO_MEASURE_GOVERNOR_SV})
add_test(NAME measure_governor_random COMMAND test_measure_governor_random)
set_tests_properties(measure_governor_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME measure_governor_random_nightly COMMAND test_measure_governor_random --nightly)
set_tests_properties(measure_governor_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# GOVERNOR -> TERRAIN.LOD, both blocks REAL. TERRAIN.LOD landed in phase 6
# against a STUB contract for the governor and had to CHOOSE the policy shape
# itself (its Notes law 8). This is the only way "the governor agrees with what
# LOD assumed" is proved rather than asserted: the governor's published targets
# are wired to LOD's target ports with no adapter, and if that stops being true
# this file stops compiling.
add_executable(test_measure_governor_lod measure/measure_governor_lod.cpp)
target_link_libraries(test_measure_governor_lod PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_governor_lod PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_measure_governor_lod
  PREFIX Vzhao_measure_governor
  TOP_MODULE zhao_measure_governor
  SOURCES ${ZHAO_MEASURE_GOVERNOR_SV})
verilate(test_measure_governor_lod
  PREFIX Vzhao_terrain_lod
  TOP_MODULE zhao_terrain_lod
  SOURCES ${ZHAO_TERRAIN_LOD_SV})
add_test(NAME measure_governor_lod COMMAND test_measure_governor_lod)
set_tests_properties(measure_governor_lod PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- phase 8: MEASURE.TOKENS x GEOM.BINNER composition ---------------------
# The seam GEOM.BINNER's law E deferred to phase 8. Both blocks real, wired
# cycle-accurately through the driver's live token authority; see the file
# header for the six unratified bindings this exposes and the return-path
# defect it reports.
add_executable(test_measure_tokens_binner measure/measure_tokens_binner.cpp)
target_link_libraries(test_measure_tokens_binner PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_tokens_binner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_measure_tokens_binner
  PREFIX Vzhao_measure_tokens
  TOP_MODULE zhao_measure_tokens
  SOURCES ${ZHAO_MEASURE_TOKENS_SV})
verilate(test_measure_tokens_binner
  PREFIX Vzhao_geom_binner
  TOP_MODULE zhao_geom_binner
  SOURCES ${ZHAO_GEOM_BINNER_SV})
add_test(NAME measure_tokens_binner COMMAND test_measure_tokens_binner)
set_tests_properties(measure_tokens_binner PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_measure_governor
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_measure_governor
                 ${ZHAO_MEASURE_GOVERNOR_SV})
set_tests_properties(lint_measure_governor PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
