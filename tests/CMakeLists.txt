# Tests. CTest labels (charter 20.3, P1 findings 4):
#   fast    â€” every commit (the default gate: ctest --test-dir build -L fast)
#   nightly â€” full fuzz + capture replay (scheduled CI job)
#   formal  â€” SymbiYosys lane; wrapper test SKIPs when sby is absent.
#             Since review M1 the smoke (sby --version) also carries `fast`:
#             sby IS in CI's oss-cad-suite, and the smoke costs < 1 s.
#   lint    â€” per-module verilator --lint-only -Wall (charter 21 step 8) +
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
find_program(ZHAO_VERILATOR_BIN NAMES verilator_bin.exe)
if(NOT ZHAO_VERILATOR_BIN)
  message(FATAL_ERROR "verilator_bin.exe not found on PATH â€” source tools/env/zhao-env.ps1|.bat")
endif()
get_filename_component(_zhao_verilator_filename "${ZHAO_VERILATOR_BIN}" NAME)
if(NOT _zhao_verilator_filename STREQUAL "verilator_bin.exe")
  message(FATAL_ERROR "Packet B requires the exact verilator_bin.exe executable")
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
  message(STATUS "npm not found on PATH â€” ledger_check test not registered (source tools/env/zhao-env.ps1|.bat)")
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

# R230 / packet GOURAUDLOOK: the POSITIVE CONTROL for the flat per-face
# stand-in lane (ZIXX_SHADE=flat). The comparison the owner rules R230 from is
# only evidence if the switch is known to reach the framebuffer -- two
# identical galleries would otherwise read as "the +24 DSP buys nothing",
# which is the stale-binary trap's shape and the flattering direction. This
# asserts the knob FIRES, that both readings drew identical content, and that
# the Gouraud reading is untouched by the knob's existence.
add_test(NAME reel_shade_lane COMMAND zhao-reel --shade-lane-check)
set_tests_properties(reel_shade_lane PROPERTIES LABELS "fast;nightly")

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

# The bare positional this used to pass stopped being accepted when the desktop
# host grew a real CLI (--ticks / --seed / --cart / --record / --replay); an
# unrecognised argument prints usage and returns 2, so the smoke test failed
# with the binary working perfectly. Invoked the documented way instead: load
# the golden frame as a cartridge and run a few ticks, which is what "the host
# starts and draws" was always meant to check.
add_test(NAME desktop_smoke
         COMMAND zhao-desktop --ticks 4
                 --cart ${CMAKE_SOURCE_DIR}/tests/abi/golden/frame_minimal.bin)
set_tests_properties(desktop_smoke PROPERTIES LABELS "fast;nightly")

# ---- W5: field IR (spec/form/field-ir.md; gate (d)) -------------------------
# The differential test compiles the TS-generated wrapper
# (compiler/tests/generated/crater_ring.hpp), owns the golden .zvec from the
# C++ oracle (captures/golden/field/), runs the 8 scripted checks of
# field-ir.md Â§12 incl. the minimize demo artifact (captures/failures/field/).
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
# three committed Earth programs. THREE ctest lanes over ONE binary â€” a
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

# R156 / owner ruling R172: the ADMISSION classifier at its boundary. Built
# by packet CMDFIELD, which could not register it because tests/CMakeLists.txt
# was another packet's file that pass -- so the test existed, passed, and DID
# NOT RUN IN ctest. A committed test nobody runs is the uncashed-cheque shape
# applied to evidence.
add_executable(test_zfield_plan_classify differential/zfield_plan_classify_directed.cpp)
target_link_libraries(test_zfield_plan_classify PRIVATE zhao_zref)
target_include_directories(test_zfield_plan_classify PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
add_test(NAME zfield_plan_classify COMMAND test_zfield_plan_classify)
set_tests_properties(zfield_plan_classify PROPERTIES LABELS "fast;nightly")

# ---- packet L1: the production lowerer (zfield/zfield_host_plan.hpp) -------
# Directed cases for every opcode shape the owner directive section 5.3
# enumerates, EACH WITH A DELIBERATE MISWIRE CONTROL; the ordinal<->window
# translation on a real program; the directive 6.2 initialisation proof; and
# R111's required_mask==0 gate shown failing with its positive control
# separate. Needs compiler/tests/generated for crater_ring.hpp, exactly as
# field_fplan_diff above does.
add_executable(test_field_host_plan_directed differential/field_host_plan_directed.cpp)
target_link_libraries(test_field_host_plan_directed PRIVATE zhao_zref)
target_include_directories(test_field_host_plan_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
add_test(NAME field_host_plan_directed COMMAND test_field_host_plan_directed)
set_tests_properties(field_host_plan_directed PROPERTIES LABELS "fast;nightly")

# The ZFH2 packer (directive 5.2 "and a tool: tools/field/pack_field_host.cpp").
# Built by the test lane so it cannot rot unbuilt -- a packer that does not
# compile is the "BUILT, INSTALLED NOWHERE" shape with the sign reversed.
add_executable(pack_field_host ${CMAKE_SOURCE_DIR}/tools/field/pack_field_host.cpp)
target_link_libraries(pack_field_host PRIVATE zhao_zref)
# The generated canonical-operation table (reference/include/zfield/generated/
# zfield_optable.hpp) is pinned against OP_INFO by compiler/tests/
# optable.test.ts â€” the same place the other TS-generated committed artifacts
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
# Â§3: changes require architect sign-off). Lint it standalone with its only
# dependency (the generated ABI package) so interface drift is caught the
# moment it is written, before any consumer module exists. Banner-append
# block (merge rule, plan Â§3).
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
#
# Its POSITIVE CONTROLS are registered as `source_list_parity_controls`, and
# they cannot live here: they need Python3_EXECUTABLE, which find_package does
# not provide until much further down this file.

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

# INPUT.SNAC (owner ruling R7). The bench parameterisation is SMALL on
# purpose -- CLK_DIV 200 -> 4, ACK_TIMEOUT 10000 -> 64, IDLE_GAP 1600 -> 4 --
# because a directed run would otherwise spend millions of cycles shifting
# bits. NO DECODED VALUE DEPENDS ON ANY OF THE THREE: they set the bus rate
# and the timeout, never a button, an axis or a presence bit, which is what
# makes the fast bench evidence about the shipping configuration. PORTS stays
# at its production default so the round-robin under test is the real one.
set(ZHAO_INPUT_SNAC_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/input/zhao_input_snac.sv)
add_executable(test_input_snac_directed input/input_snac_directed.cpp)
target_link_libraries(test_input_snac_directed PRIVATE zhao_harness zhao_zref)
verilate(test_input_snac_directed
  PREFIX Vzhao_input_snac
  TOP_MODULE zhao_input_snac
  INCLUDE_DIRS ${ZHAO_INPUT_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_SNAC_SV}
  VERILATOR_ARGS -GCLK_DIV=4 -GACK_TIMEOUT=64 -GIDLE_GAP=4)
add_test(NAME input_snac_directed COMMAND test_input_snac_directed)
set_tests_properties(input_snac_directed PROPERTIES LABELS "fast;nightly")

add_test(NAME lint_input_snac
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_input_snac
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${ZHAO_ZHAO_PKG} ${ZHAO_INPUT_SNAC_SV})
set_tests_properties(lint_input_snac PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

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
# absent â€” same lane pattern as formal_lane)
configure_file(formal/input_snapshot_atomic.cmake.in
               formal/input_snapshot_atomic.cmake @ONLY)
add_test(NAME input_snapshot_atomic
         COMMAND ${CMAKE_COMMAND} -P
                 ${CMAKE_CURRENT_BINARY_DIR}/formal/input_snapshot_atomic.cmake)
set_tests_properties(input_snapshot_atomic PROPERTIES
  LABELS "formal;nightly"
  RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                   # provers blew the 1800 s wrapper budget on runs that pass
                   # solo ï¿½ wall time must approximate solo time (2026-08-16)
  SKIP_REGULAR_EXPRESSION "formal SKIPPED")
# ---- W2.4: audio (plan W2.4; spec/audio_rules.md, contracts AUDIO.FIFO ----
# ---- + SW.MIXER) ------------------------------------------------------------
# Tone subset (SW.MIXER): golden-vector directed + PCG tone-switching random,
# both pure ZRef (RTL never generates the tone â€” mixer-side only).
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

# formal: audio_fifo_bounds (plan Â§4) â€” SKIP when sby absent. The wrapper
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
                   # solo ï¿½ wall time must approximate solo time (2026-08-16)
  SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
# ---- W2.5: memory -----------------------------------------------------------
# Plan W2.5: synthesizable modules lint individually (charter 21 step 8);
# the BEHAVIOURAL SDRAM MODEL (sim/models/zhao_sdram_model.sv) is a
# TESTBENCH-ONLY component (spec/memory_rules.md 1, plan D2) and is
# DELIBERATELY EXCLUDED from every lint target and from fpga/files.qip â€”
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

# MEM.HPS.BRIDGE randomized differential vs zref::HpsBridge (charter Â§21
# step 5). Previously the block's `tests.random` was an ALIAS of the directed
# file and the cited mem_random.cpp never instantiated the bridge â€” the
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
  # reduced), and it had never actually been elaborated before â€” under the old
  # SKIP it "ran" instantly. With a genuinely free environment it does not
  # finish in any CI-shaped budget (>50 min and still going when measured), so
  # putting it in the routine lane would only manufacture timeouts that look
  # like failures. Measured: its `cover` task PASSES in ~11 min (all three
  # covers reached, so the bound is not vacuous), while the `bmc` task was
  # still running after 50 min â€” NOT ESTABLISHED, which is an unfinished
  # proof, not a failing one. Kept runnable by hand:
  #     cd tests/formal && sby -f --prefix <dir> mem_sdram_refresh_bound.sby
  # See the wave-3 TASK_LOG.
  # MEM.SDRAM is SPECIFIED / blocked_on: hardware, so no maturity claim rests
  # on it today.
  # mem_guard_resbound_mutant is INVERTED (`expect fail`): the positive control
  # for owner ruling R32's resource-write bound. It passes only when the
  # no-escape proof FAILS on tests/mutants/zhao_mem_guard_resbound_mutant.sv
  # (pool containment removed). Its wrapper message says "bmc + cover"; it
  # carries bmc only.
  # mem_guard_devbound_mutant is the SECOND inverted control (`expect fail`),
  # for owner ruling R242's TERRAIN.DEVSTORE bound. It passes only when the
  # no-escape proof FAILS on tests/mutants/zhao_mem_guard_devbound_mutant.sv
  # (the store's UPPER bound removed). Measured 2026-09-22: production PASSES
  # with both c_forward_devstore_wr and c_forward_devstore_rd reached, and this
  # copy fails at a1_resource_bounded -- the pair that shows the `ifdef seam
  # engaged rather than silently compiling production twice.
  # mem_guard_pbview_mutant and mem_guard_pbunion_mutant are the THIRD and
  # FOURTH inverted controls (`expect fail`), for the owner's completion ruling
  # of 2026-09-22 item 4 (GEOM.PARAMBUF). pbview removes the WRITE arm's view
  # SELECTION so the producer may overwrite the view the walker is reading;
  # pbunion collapses the three region containment tests into their union, so a
  # burst spanning the view0/view1 seam is admitted -- item 4's forbidden
  # reading written as RTL, and the subtler of the two, since every request
  # that stays inside one region behaves identically to production.
  # Measured 2026-09-22: production PASSES with all five c_forward_pb_* covers
  # reached (rd_v0, rd_v1, wr_v0, wr_v1, scr) at step 4, and BOTH copies fail
  # at a1_pb_wr_view0. The same run moved the OTHER two controls' reported
  # theorems -- resbound a1_region -> a1_map, devbound a1_resource_bounded ->
  # a1_region -- because bmc reports the first assertion it violates and the
  # theorem set grew. Nothing about those two faults changed; their headers say
  # so rather than being quietly re-edited.
  foreach(sby_prop IN ITEMS mem_vram_arbiter_liveness mem_guard_no_escape mem_guard_resbound_mutant
                            mem_guard_devbound_mutant mem_guard_pbview_mutant
                            mem_guard_pbunion_mutant)
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
    # â€” bmc plus the two `expect fail` tightness tasks that prove the bound is
    # exact, plus cover â€” and measures 9m31s wall with all four in parallel on
    # the reference machine. 900s left ~35% headroom, which is how a proof lane
    # starts producing timeouts that get read as failures (or, worse, get
    # "fixed" by deleting tasks). The wrapper's own TIMEOUT is 1800 so it fails
    # with a readable sby message before CTest kills it from outside.
    set_tests_properties(formal_${sby_prop} PROPERTIES
      LABELS "formal;nightly"
      RUN_SERIAL TRUE  # heavy provers must not co-schedule: two 4-way-contended
                       # provers blew the 1800 s wrapper budget on runs that pass
                       # solo ï¿½ wall time must approximate solo time (2026-08-16)
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

# stars_and_flares.md Â§13 anchors: hand-computed ramp/boil/corona/ghost/
# fade/LOD/glint values, the starfield harness-equivalence gate (byte-exact
# vs tests/golden/starfield/oracle.bin BEFORE any rendering test), the
# celestial_state round-trip, the compositor determinism + Â§5 texel budget,
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
# column_query directed + the physics-equals-pixels differential (Â§10.1),
# breach/heal/no_bake law (Â§10.2 at reference scope), the Â§9.2 bake deferral
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

# ---- FORGE.CLIFF bitmap-RAM CANDIDATE (Roadmap s14 Commit6) ------------------
# zhao_forge_cliff_ram beside the golden: the 34x34 SOLID window in one RAM
# behind three row registers, the alive bitmap deleted by span-walk
# compaction, the algorithm untouched. Three kinds of evidence, each a target:
#   1. the golden suites UNMODIFIED against the candidate -- the candidate is
#      verilated under the golden's PREFIX so the suites compile untouched;
#   2. the three-way differential (oracle / golden / candidate in ONE process,
#      752 pages, cycle counts printed) -- two verilate() calls on one target;
#   3. the committed mutants with inverted-polarity controls: two fire the
#      candidate's walk_fault_o (zero span / overshoot), one shows the
#      differential FAILING on a prefetch one row off.
set(ZHAO_FORGE_CLIFF_RAM_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_ram.sv)

add_test(NAME lint_forge_cliff_ram
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_cliff_ram
                 ${ZHAO_FORGE_CLIFF_RAM_SV})
set_tests_properties(lint_forge_cliff_ram PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_forge_cliff_ram_directed forge/forge_cliff_directed.cpp)
target_link_libraries(test_forge_cliff_ram_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_ram_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_ram_directed
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff_ram
  SOURCES ${ZHAO_FORGE_CLIFF_RAM_SV})
add_test(NAME forge_cliff_ram_directed COMMAND test_forge_cliff_ram_directed)
set_tests_properties(forge_cliff_ram_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_forge_cliff_ram_random forge/forge_cliff_random.cpp)
target_link_libraries(test_forge_cliff_ram_random PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_ram_random PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_ram_random
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff_ram
  SOURCES ${ZHAO_FORGE_CLIFF_RAM_SV})
add_test(NAME forge_cliff_ram_random COMMAND test_forge_cliff_ram_random)
set_tests_properties(forge_cliff_ram_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_forge_cliff_ram_differential forge/forge_cliff_ram_differential.cpp)
target_link_libraries(test_forge_cliff_ram_differential PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_ram_differential PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_ram_differential
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_FORGE_CLIFF_SV})
verilate(test_forge_cliff_ram_differential
  PREFIX Vzhao_forge_cliff_ram
  TOP_MODULE zhao_forge_cliff_ram
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_FORGE_CLIFF_RAM_SV})
add_test(NAME forge_cliff_ram_differential COMMAND test_forge_cliff_ram_differential)
set_tests_properties(forge_cliff_ram_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME forge_cliff_ram_differential_nightly COMMAND test_forge_cliff_ram_differential --nightly)
set_tests_properties(forge_cliff_ram_differential_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

# the mutants: each verilated under the golden's PREFIX so the shared driver
# header compiles; each control PASSES when the instrument FIRES.
foreach(mut IN ITEMS mutant over_mutant)
  add_executable(test_forge_cliff_ram_${mut}_control forge/forge_cliff_ram_mutant_control.cpp)
  target_link_libraries(test_forge_cliff_ram_${mut}_control PRIVATE zhao_harness zhao_zref)
  target_include_directories(test_forge_cliff_ram_${mut}_control PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/forge)
  verilate(test_forge_cliff_ram_${mut}_control
    PREFIX Vzhao_forge_cliff
    TOP_MODULE zhao_forge_cliff_ram_${mut}
    VERILATOR_ARGS --assert
    SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_forge_cliff_ram_${mut}.sv)
  add_test(NAME forge_cliff_ram_${mut}_control COMMAND test_forge_cliff_ram_${mut}_control)
  set_tests_properties(forge_cliff_ram_${mut}_control PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
endforeach()

add_executable(test_forge_cliff_ram_rowoff_control forge/forge_cliff_ram_rowoff_control.cpp)
target_link_libraries(test_forge_cliff_ram_rowoff_control PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_ram_rowoff_control PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_ram_rowoff_control
  PREFIX Vzhao_forge_cliff
  TOP_MODULE zhao_forge_cliff_ram_rowoff_mutant
  VERILATOR_ARGS --assert
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_forge_cliff_ram_rowoff_mutant.sv)
add_test(NAME forge_cliff_ram_rowoff_control COMMAND test_forge_cliff_ram_rowoff_control)
set_tests_properties(forge_cliff_ram_rowoff_control PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ---- FORGE.CLIFF PRODUCER TO CONSUMER, on real terrain (CLIFFPROD) ----------
# The adopted evaluator had no producer for any of its three inputs and no
# consumer for its output -- an LFSR in the pricing top was the whole of it.
# `tb_forge_cliff_chain.sv` elaborates the real chain in one process: a
# behavioural copy of the compose cache's two read services (one clock,
# registered, untagged, refusing with 2'd3 when no patch is served), the two
# service sharers, the window feed, the UNMODIFIED `zhao_forge_cliff_ram`, and
# the emission stage, with the driver playing `zhao_forge_assemble`.
set(ZHAO_FORGE_CLIFF_CHAIN_SV
    ${CMAKE_SOURCE_DIR}/tests/forge/tb_forge_cliff_chain.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_srvshare.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_feed.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_emit.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_ram.sv)

add_test(NAME lint_forge_cliff_srvshare
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_cliff_srvshare
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_srvshare.sv)
set_tests_properties(lint_forge_cliff_srvshare PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_forge_cliff_feed
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_cliff_feed
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_feed.sv)
set_tests_properties(lint_forge_cliff_feed PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_forge_cliff_emit
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_cliff_emit
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_emit.sv)
set_tests_properties(lint_forge_cliff_emit PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_forge_cliff_chain forge/forge_cliff_chain.cpp)
target_link_libraries(test_forge_cliff_chain PRIVATE zhao_harness zhao_zref)
target_include_directories(test_forge_cliff_chain PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_chain
  PREFIX Vtb_forge_cliff_chain
  TOP_MODULE tb_forge_cliff_chain
  SOURCES ${ZHAO_FORGE_CLIFF_CHAIN_SV})
add_test(NAME forge_cliff_chain COMMAND test_forge_cliff_chain)
set_tests_properties(forge_cliff_chain PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# The sharer at its own port, and THE POSITIVE CONTROL for `poison1_o`. The
# composed chain reads that counter as zero because `zhao_forge_cliff_feed`
# never asks while the patch is gone -- which is evidence about the feed and
# none whatever about the detector. Legal stimulus reaches the state here,
# because a client that does not guard is a legal client, so this is a driver
# rather than a committed mutant.
add_executable(test_forge_cliff_srvshare_unit forge/forge_cliff_srvshare_unit.cpp)
target_link_libraries(test_forge_cliff_srvshare_unit PRIVATE zhao_harness)
target_include_directories(test_forge_cliff_srvshare_unit PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/forge)
verilate(test_forge_cliff_srvshare_unit
  PREFIX Vzhao_forge_cliff_srvshare
  TOP_MODULE zhao_forge_cliff_srvshare
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_cliff_srvshare.sv)
add_test(NAME forge_cliff_srvshare_unit COMMAND test_forge_cliff_srvshare_unit)
set_tests_properties(forge_cliff_srvshare_unit PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ---- deep-keel wave: TEXTURE.MOSAIC (terrain_rules.md 6.2) ------------------
# The oracle lanes (the mirrored-repeat fold at wrap boundaries, the frozen
# pick constants, the rendered dither + exact modulation through the
# replicated resolve) AND, since 2026-08-19, the RTL lanes: the same two files
# now also drive `zhao_texture_mosaic`, which is why both targets verilate.
# `zhao_texture_mod255` rides along because the mosaic instantiates it.
set(ZHAO_TEXTURE_MOSAIC_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic.sv)

# ---------------------------------------------------------------------------
# The shared console runtime boundary (software lane, 2026-09-05).
# runtime/ held one 64-line stub that replays packets through the empty shell;
# it loads no cartridge, owns no resources, polls nothing and advances nothing.
# zcon.hpp is the middle of the three boundaries -- game truth / console runtime
# / platform backend -- and this pins the day-one properties the roadmap asks
# for: a fixed-tick loop, inputs and state hashes recorded from the first tick,
# exact replay, and a divergence LOCATED at the tick it happened.
add_executable(test_zcon_frame_build_directed runtime/zcon_frame_build_directed.cpp)
target_link_libraries(test_zcon_frame_build_directed PRIVATE zhao_zref)
target_include_directories(test_zcon_frame_build_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME zcon_frame_build_directed COMMAND test_zcon_frame_build_directed)
set_tests_properties(zcon_frame_build_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_wizards_directed runtime/wizards_directed.cpp)
target_include_directories(test_wizards_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME wizards_directed COMMAND test_wizards_directed)
set_tests_properties(wizards_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_zcon_session_directed runtime/zcon_session_directed.cpp)
target_include_directories(test_zcon_session_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME zcon_session_directed COMMAND test_zcon_session_directed)
set_tests_properties(zcon_session_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The host's frame actually DRAWS, through the reference renderer, in the place
# the projection says. Two defects made it here that no counter could see: the
# resource table keyed by a bare index instead of the wire handle32 (400 frames
# "rendered" onto a blank canvas, status 0, because a resource miss is not an
# error), and a near-zero-mean input script that left the markers in the same
# pixel for the whole run. Both are pinned in this file.
add_executable(test_desktop_render_directed runtime/desktop_render_directed.cpp)
target_link_libraries(test_desktop_render_directed PRIVATE zhao_zref)
target_include_directories(test_desktop_render_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME desktop_render_directed COMMAND test_desktop_render_directed)
set_tests_properties(desktop_render_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ---------------------------------------------------------------------------
# TEXTURE.COMBINE (G1-C, 2026-09-05). The material combiner did not exist: the
# surviving TEXJOIN returns sample 0 for every recipe, so the island fetched up
# to three samples per fragment and combined none of them.
#
# Two tests, because the oracle is the authority and the RTL is the claim:
#   material_combine_directed -- the SCALAR law at the unit8 corners, incl. a
#     sweep of the modulate-by-255 identity boundary across all 256 inputs;
#   texture_combine_diff      -- the RTL against that oracle, with a coverage
#     guard that every recipe and both refusal classes were actually reached.
add_executable(test_fjournal_directed terrain/fjournal_directed.cpp)
target_link_libraries(test_fjournal_directed PRIVATE zhao_zref)
add_test(NAME fjournal_directed COMMAND test_fjournal_directed)
set_tests_properties(fjournal_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_hps_boundary_directed memory/hps_boundary_directed.cpp)
target_link_libraries(test_hps_boundary_directed PRIVATE zhao_zref)
add_test(NAME hps_boundary_directed COMMAND test_hps_boundary_directed)
set_tests_properties(hps_boundary_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_residency_directed memory/residency_directed.cpp)
target_link_libraries(test_residency_directed PRIVATE zhao_zref)
add_test(NAME residency_directed COMMAND test_residency_directed)
set_tests_properties(residency_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# AUDIT R8: a single-page allocator validated an upload against the WHOLE
# arena, so a length longer than a page was accepted and overlapped its
# pinned neighbour with every pin counter reading zero.
add_executable(test_residency_oversize_directed memory/residency_oversize_directed.cpp)
target_link_libraries(test_residency_oversize_directed PRIVATE zhao_zref)
add_test(NAME residency_oversize_directed COMMAND test_residency_oversize_directed)
set_tests_properties(residency_oversize_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_material_resolve_directed texture/material_resolve_directed.cpp)
target_link_libraries(test_material_resolve_directed PRIVATE zhao_zref)
add_test(NAME material_resolve_directed COMMAND test_material_resolve_directed)
set_tests_properties(material_resolve_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# AUDIT R1/R2: the test that crosses the resolve/combine boundary. Both suites
# above were green while a legal terrain-recipe record was refused before it
# could reach the combiner, and no translation unit could include both headers
# to notice. This file compiling AT ALL is the R2 proof.
add_executable(test_material_resolve_combine_bridge texture/material_resolve_combine_bridge.cpp)
target_link_libraries(test_material_resolve_combine_bridge PRIVATE zhao_zref)
add_test(NAME material_resolve_combine_bridge COMMAND test_material_resolve_combine_bridge)
set_tests_properties(material_resolve_combine_bridge PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# --------------------------------------------------------------------------
# MATERIAL.RESOLVE's RTL, against the oracle two files above.
#
# The suite above proves the LOOKUP RULES and has since 2026-09-05; what had
# never existed is the silicon. `zhao_material_resolve` is that block, and this
# target differences it against `zref::material::Resolver` rather than
# restating the rules a third time.
#
# TWO INSTANCES, ONE STIMULUS. `tb_zhao_material_resolve.sv` wraps the shipping
# resolver (RECIPE_COUNT = 8) beside a copy at the HISTORICAL ceiling of 6. At
# 8 the recipe arm of `refused_record_o` is structurally unreachable -- the
# field is three bits -- so the control is the parameter, which is one of the
# three firings CLAUDE.md accepts as stimulus rather than a committed mutant.
# The difference between the two response streams on a terrain recipe IS audit
# R1's defect, asserted rather than narrated.
add_executable(test_material_resolve_rtl_directed texture/material_resolve_rtl_directed.cpp)
target_link_libraries(test_material_resolve_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_material_resolve_rtl_directed
  PREFIX Vtb_zhao_material_resolve
  TOP_MODULE tb_zhao_material_resolve
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_zhao_material_resolve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_resolve.sv)
add_test(NAME material_resolve_rtl_directed COMMAND test_material_resolve_rtl_directed)
set_tests_properties(material_resolve_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_material_resolve
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_material_resolve
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_resolve.sv)
set_tests_properties(lint_zhao_material_resolve PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE POSITIVE CONTROL FOR OWNER RULING D-3. Inverted polarity: it PASSES when
# the coherence case BREAKS. D-3's tag is a STRUCTURAL property -- the stale
# line is "structurally unable to match" -- so no legal stimulus can make the
# shipping resolver fail case 5 of the suite above, and that case had therefore
# never been seen to go red. The committed mutant narrows the tag comparison to
# the low eight bits of the residency generation (the width a handle32 carries)
# and this driver REQUIRES the stale hit.
add_executable(test_material_resolve_gen8tag_control
  texture/material_resolve_gen8tag_control.cpp)
target_link_libraries(test_material_resolve_gen8tag_control PRIVATE zhao_harness zhao_zref)
verilate(test_material_resolve_gen8tag_control
  PREFIX Vzhao_material_resolve_gen8tag_mutant
  TOP_MODULE zhao_material_resolve_gen8tag_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_material_resolve_gen8tag_mutant.sv)
add_test(NAME material_resolve_gen8tag_control
         COMMAND test_material_resolve_gen8tag_control)
set_tests_properties(material_resolve_gen8tag_control PROPERTIES
  LABELS "fast;nightly;mutant" TIMEOUT 300)

add_executable(test_material_combine_directed texture/material_combine_directed.cpp)
target_link_libraries(test_material_combine_directed PRIVATE zhao_zref)
add_test(NAME material_combine_directed COMMAND test_material_combine_directed)
set_tests_properties(material_combine_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_texture_combine_diff texture/texture_combine_diff.cpp)
target_link_libraries(test_texture_combine_diff PRIVATE zhao_harness zhao_zref)
target_include_directories(test_texture_combine_diff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_texture_combine_diff
  PREFIX Vzhao_texture_combine
  TOP_MODULE zhao_texture_combine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_combine.sv)
add_test(NAME texture_combine_diff COMMAND test_texture_combine_diff)
set_tests_properties(texture_combine_diff PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# MATERIAL.COMBINE.V1 -- the two-lane scheduled combiner that replaces the
# block above. The II=1 form fit at 8 DSP against the architecture's rule of 2
# (docket D19q), so this one is variant A `LOGIC2`: two multipliers attributed
# to ALM logic, tokenized microjobs, queued continuations. The differential
# matches results BY TAG rather than by arrival, because Â§15.6 has the combiner
# retire out of allocation order -- a position-keyed test would pass only while
# the scheduler failed to do its job.
add_executable(test_material_combine_v1_diff texture/material_combine_v1_diff.cpp)
target_link_libraries(test_material_combine_v1_diff PRIVATE zhao_harness zhao_zref)
target_include_directories(test_material_combine_v1_diff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_material_combine_v1_diff
  PREFIX Vzhao_texture_material_combine_v1
  TOP_MODULE zhao_texture_material_combine_v1
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v1.sv)
add_test(NAME material_combine_v1_diff COMMAND test_material_combine_v1_diff)
set_tests_properties(material_combine_v1_diff PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# COMBINE V2 -- the paired-phase replacement, against the same oracle and the
# same workload as V1, plus the exact phase count the fixed schedule owes.
add_executable(test_material_combine_v2_diff texture/material_combine_v2_diff.cpp)
target_link_libraries(test_material_combine_v2_diff PRIVATE zhao_harness zhao_zref)
target_include_directories(test_material_combine_v2_diff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_material_combine_v2_diff
  PREFIX Vzhao_texture_material_combine_v2
  TOP_MODULE zhao_texture_material_combine_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v2.sv)
add_test(NAME material_combine_v2_diff COMMAND test_material_combine_v2_diff)
set_tests_properties(material_combine_v2_diff PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# READ-LATE (roadmap 4.2 / Commit4, 2026-09-10): the SAME combiner elaborated
# with READ_LATE=1, behind four REAL owner planes (zhao_texture_v3bank, as v3own
# wires them), against the SAME oracle with V2's workload -- plus the seam's own
# laws: publication-before-ready, release-after-last-reader, and every plane read
# naming an in-flight slot. reports/TEXTURE-READLATE-COMBINE-20260910.md.
add_executable(test_material_combine_readlate_diff texture/material_combine_readlate_diff.cpp)
target_link_libraries(test_material_combine_readlate_diff PRIVATE zhao_harness zhao_zref)
target_include_directories(test_material_combine_readlate_diff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_material_combine_readlate_diff
  PREFIX Vtb_combine_readlate
  TOP_MODULE tb_combine_readlate
  VERILATOR_ARGS --assert
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_combine_readlate.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v2.sv)
add_test(NAME material_combine_readlate_diff COMMAND test_material_combine_readlate_diff)
set_tests_properties(material_combine_readlate_diff PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THE POSITIVE CONTROL: the same driver and wrapper against the committed
# slot-swap mutant (tests/mutants/zhao_texture_material_combine_v2_slotswap_mutant.sv).
# It PASSES WHEN THE CHECKERS FIRE -- inverse polarity, evidence about the
# instruments. A differential that has never been seen to catch a difference has
# not been tested; this is where the read-late one is seen to.
add_executable(test_material_combine_readlate_mutant_control
  texture/material_combine_readlate_diff.cpp)
target_compile_definitions(test_material_combine_readlate_mutant_control PRIVATE
  READLATE_EXPECT_MISMATCH=1)
target_link_libraries(test_material_combine_readlate_mutant_control PRIVATE zhao_harness zhao_zref)
target_include_directories(test_material_combine_readlate_mutant_control PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_material_combine_readlate_mutant_control
  PREFIX Vtb_combine_readlate
  TOP_MODULE tb_combine_readlate
  VERILATOR_ARGS --assert -DREADLATE_MUTANT
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_combine_readlate.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_material_combine_v2_slotswap_mutant.sv)
add_test(NAME material_combine_readlate_mutant_control
         COMMAND test_material_combine_readlate_mutant_control)
set_tests_properties(material_combine_readlate_mutant_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 900)

# THE COMPOSED ISLAND, functionally. The composed FIT answers capacity; it
# cannot answer whether the wiring is right, because a top that connects eleven
# blocks through mismatched handshakes still synthesises and still reports an
# ALM count -- it simply never moves a fragment. The roadmap's rule for every
# integration step is a composed test that ACTUALLY DRAWS THROUGH the added
# hardware, so this drives fragments in at the boundary and requires each
# block's counter to have moved on the way out.
add_executable(test_island_composed_directed texture/island_composed_directed.cpp)
target_link_libraries(test_island_composed_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_island_composed_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/texture)
verilate(test_island_composed_directed
  PREFIX Vzhao_texture_island_top
  TOP_MODULE zhao_texture_island_top
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_island_top.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_fragrob.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_svc.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_svc.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_pairpipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_plan.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v2.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME island_composed_directed COMMAND test_island_composed_directed)
set_tests_properties(island_composed_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

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
# before 2026-08-19 â€” the ledger named it and nothing under reference/ defined
# it â€” so texture_aux_directed's FIRST lane proves the new header is a faithful
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
# blocks â€” confers no block promotion: GEOM.POSE stays SPECIFIED until the
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
# per clock is two bytes), so the leaf travels with it â€” same rule as CMD.DMA.
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

# THE TYPE_W GUARD, PROVEN TO FIRE. The owner ruling of 2026-09-21 section 4a
# permits a TYPE_W parameter for legacy BENCHES and requires 24 bits in
# production; the module refuses anything outside 16..24 from an `initial`
# block, and `--lint-only` does not run those, so the clean lint above is not
# evidence about it. WILL_FAIL inverts the polarity: GREEN when the design
# refuses to elaborate at TYPE_W=8.
add_executable(test_geom_pose_cache_elab_guard geometry/geom_pose_cache_elab_guard.cpp)
target_link_libraries(test_geom_pose_cache_elab_guard PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_pose_cache_elab_guard PRIVATE VL_USER_STOP VL_USER_FATAL)
verilate(test_geom_pose_cache_elab_guard
  PREFIX Vzhao_geom_pose_cache_elab
  TOP_MODULE zhao_geom_pose_cache
  VERILATOR_ARGS -GTYPE_W=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_cache.sv)
add_test(NAME geom_pose_cache_elab_guard COMMAND test_geom_pose_cache_elab_guard)
set_tests_properties(geom_pose_cache_elab_guard PROPERTIES
  LABELS "fast;geometry" TIMEOUT 120 WILL_FAIL TRUE)

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

# GEOM.POSE's chain multiply: sequential. Since owner ruling R4 (2026-09-09)
# the default is MUL_LANES=1 -- ONE 32x32 product per cycle, a 37-cycle walk,
# 3 DSP -- with the previous one-element-per-cycle arrangement kept as
# MUL_LANES=3 (12-cycle walk, 9 DSP; the *_elem variant below holds it to the
# same oracle). The walk length is asserted as its own law (section 7) --
# correct arithmetic delivered in the wrong number of cycles, or offered while
# still walking, would pass every differential case.
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

# ---- Ruling R4 reversibility: the legacy spatial/elem arms, same oracle ----
# MUL_LANES is a named, editable knob (CLAUDE.md: every timing value stays a
# knob), so the pre-R4 arrangements stay buildable and stay HELD TO THE SAME
# ORACLE -- a knob whose alternative position is never exercised is a knob
# that has silently stopped turning. The walk-length macro moves with the
# parameter; every bit-exactness check is byte-identical between variants.
add_executable(test_geom_quat2mat_spatial geometry/geom_quat2mat_directed.cpp)
target_compile_definitions(test_geom_quat2mat_spatial PRIVATE ZHAO_QUAT2MAT_WALK=0)
target_link_libraries(test_geom_quat2mat_spatial PRIVATE zhao_harness zhao_zref)
verilate(test_geom_quat2mat_spatial
  PREFIX Vzhao_geom_quat2mat
  TOP_MODULE zhao_geom_quat2mat
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
  VERILATOR_ARGS -GMUL_LANES=9)
add_test(NAME geom_quat2mat_spatial COMMAND test_geom_quat2mat_spatial)
set_tests_properties(geom_quat2mat_spatial PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_geom_mat3x4_mul_elem geometry/geom_mat3x4_mul_directed.cpp)
target_compile_definitions(test_geom_mat3x4_mul_elem PRIVATE ZHAO_MAT3X4_WALK=12)
target_link_libraries(test_geom_mat3x4_mul_elem PRIVATE zhao_harness zhao_zref)
verilate(test_geom_mat3x4_mul_elem
  PREFIX Vzhao_geom_mat3x4_mul
  TOP_MODULE zhao_geom_mat3x4_mul
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
  VERILATOR_ARGS -GMUL_LANES=3)
add_test(NAME geom_mat3x4_mul_elem COMMAND test_geom_mat3x4_mul_elem)
set_tests_properties(geom_mat3x4_mul_elem PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_geom_pose_decode_spatial geometry/geom_pose_decode_directed.cpp)
target_link_libraries(test_geom_pose_decode_spatial PRIVATE zhao_harness zhao_zref)
verilate(test_geom_pose_decode_spatial
  PREFIX Vzhao_geom_pose_decode
  TOP_MODULE zhao_geom_pose_decode
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_decode.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
  VERILATOR_ARGS -GMUL_LANES_QUAT=9 -GMUL_LANES_MAT=3)
add_test(NAME geom_pose_decode_spatial COMMAND test_geom_pose_decode_spatial)
set_tests_properties(geom_pose_decode_spatial PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- THE POSITIVE CONTROLS for the R4 sequencers, driving committed MUTANTS.
# The directed suites assert bit-identity, and on the sequenced arms that
# claim leans on machinery the spatial arms never had: the operand-mux
# SCHEDULE (quat2mat) and the element-boundary ACCUMULATOR CLEAR (mat3x4).
# No pre-R4 run of either suite ever demonstrated its checker can see those
# faults. Each mutant breaks exactly one line; THESE TESTS PASS WHEN THE
# DIFFERENTIAL FAILS -- inverse polarity, evidence about the instrument, not
# about the design. Every counter still balances on both mutants, which is
# the point: the differential is the ONLY detector for this class.
add_executable(test_geom_quat2mat_mutant_control geometry/geom_quat2mat_mutant_control.cpp)
target_link_libraries(test_geom_quat2mat_mutant_control PRIVATE zhao_harness zhao_zref)
verilate(test_geom_quat2mat_mutant_control
  PREFIX Vzhao_geom_quat2mat_mutant
  TOP_MODULE zhao_geom_quat2mat_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_quat2mat_mutant.sv)
add_test(NAME geom_quat2mat_mutant_control COMMAND test_geom_quat2mat_mutant_control)
set_tests_properties(geom_quat2mat_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_geom_mat3x4_mul_mutant_control geometry/geom_mat3x4_mul_mutant_control.cpp)
target_link_libraries(test_geom_mat3x4_mul_mutant_control PRIVATE zhao_harness zhao_zref)
verilate(test_geom_mat3x4_mul_mutant_control
  PREFIX Vzhao_geom_mat3x4_mul_mutant
  TOP_MODULE zhao_geom_mat3x4_mul_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_mat3x4_mul_mutant.sv)
add_test(NAME geom_mat3x4_mul_mutant_control COMMAND test_geom_mat3x4_mul_mutant_control)
set_tests_properties(geom_mat3x4_mul_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

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

# MEM.HPS.ARBITER at N=3 -- owner ruling R4 widened the arbiter to N clients
# and asked for the starvation law to be preserved AND RE-PROVED. The two-client
# test above now runs through `zhao_hps_arbiter`, the N core's N=2 wrapper; this
# one puts a third client on the same core with the REAL bridge, including the
# middle-client case (outranked and outranking) no two-port test can reach, and
# fires client 2's wait counter by stimulus.
add_executable(test_hps_arbiter_n_directed memory/hps_arbiter_n_directed.cpp)
target_link_libraries(test_hps_arbiter_n_directed PRIVATE zhao_harness)
verilate(test_hps_arbiter_n_directed
  PREFIX Vzhao_hps_arb_n_compose
  TOP_MODULE zhao_hps_arb_n_compose
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_bridge.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/zhao_hps_arb_n_compose.sv)
add_test(NAME hps_arbiter_n_directed COMMAND test_hps_arbiter_n_directed)
set_tests_properties(hps_arbiter_n_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# -Wall at N=3 and N=5: the N core with a non-power-of-two count, where an
# owner index can name a client that does not exist.
foreach(_zhao_arb_n 3 5)
  add_test(NAME lint_zhao_hps_arbiter_n${_zhao_arb_n}
           COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                   --top-module zhao_hps_arbiter_n -GN=${_zhao_arb_n}
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv)
  set_tests_properties(lint_zhao_hps_arbiter_n${_zhao_arb_n} PROPERTIES
    LABELS "lint;fast"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()
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
  PREFIX Vzhao_field_v3_curve
  TOP_MODULE zhao_field_v3_curve
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
  PREFIX Vzhao_field_v3_core
  TOP_MODULE zhao_field_v3_core
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
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
  PREFIX Vzhao_field_v3_engine
  TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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

# THE PRODUCTION FIELD v3 ENGINE, LINTED AS ONE UNIT at -Wall.
#
# The composed machine had five differential gates on it and no LINT gate,
# which is how four -Wall VARHIDDEN errors in zhao_field_v3_mulbank and
# zhao_field_v3_ring_svc survived: every target that elaborates them is a
# verilate() build, and verilate() does not pass -Wall. The errors were
# found only when the whole engine was linted by hand, and one of them was
# MASKING the svcpath mutation preflight, which had been dying on lint
# before it checked a single anchor.
#
# UNUSEDSIGNAL is deliberately live here. A composition is exactly where a
# response port carried and never read shows up, and it cannot show up in
# the block that drives it.
add_test(NAME lint_field_v3_engine
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_v3_engine
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
set_tests_properties(lint_field_v3_engine PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# AND AT THE SHIPPED CONFIGURATION. field_v3_earth_quad gates the engine at
# CTX=32 LANES=4 and the defaults below it are the SCALAR bench point, so a
# lint of the defaults alone says nothing about the widths the console
# instantiates. Both are cheap; only one of them is the machine.
add_test(NAME lint_field_v3_engine_shipped
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 -GCTX=32 -GOUTSTANDING=16 -GLANES=4 -GLONGQ=16
                 -GDIST_BANKS=8 -GRING_UNITS=8 -GREGS=64
                 --top-module zhao_field_v3_engine
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
set_tests_properties(lint_field_v3_engine_shipped PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")


add_executable(test_field_v3_earth_directed differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_directed
  PREFIX Vzhao_field_v3_engine
  TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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

# THE GROUP CAP, PROVED EXACT (gz/pfs, 2026-09-19). The same real Earth programs
# -- DIST2-heavy, which is the distance service a cap narrows -- on the same
# eight-context engine, with every long-op group capped at ONE point. At that
# cap zhao_field_v3_svcpath builds the distance service one lane wide (two
# roots at DIST_BANKS=2 instead of eight), and the dispatcher cannot put a real
# point in a lane it no longer computes. The gate is the test's own: every value
# against the oracle. The clocks it prints are SLOWER by design at eight
# contexts; the console's front holds one point in flight and loses nothing.
add_executable(test_field_v3_earth_group1 differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_group1 PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_group1
  PREFIX Vzhao_field_v3_engine
  TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
  VERILATOR_ARGS -GGROUP_PTS=1)
add_test(NAME field_v3_earth_group1 COMMAND test_field_v3_earth_group1 --points 256
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/crater_ring.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/impact_wave.zprog
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/wave_pool.zprog)
set_tests_properties(field_v3_earth_group1 PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_field_v3_len_lanes1
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall -GLANES=1
                 --top-module zhao_field_v3_len
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_len.sv)
set_tests_properties(lint_zhao_field_v3_len_lanes1 PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# THE SAME PROGRAMS ON A WIDER MACHINE. Little's law, not a guess: a point
# costs ~250 clocks of latency and only eight are ever in flight, so the engine
# idles waiting rather than running out of arithmetic. This target scales the
# two parameters that bound points-in-flight and measures what falls out.
# CTX=8 keeps its own target above so every existing tally still means what it
# meant when it was taken.
add_executable(test_field_v3_earth_wide differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_wide PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_wide
  PREFIX Vzhao_field_v3_engine
  TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
# ---------------------------------------------------------------------------
# THE FIELD SWEEP, off by default: -DZHAO_FIELD_SWEEP=ON
# ---------------------------------------------------------------------------
# Every parameter of the Earth engine is a Verilator -G, so a sweep is a
# REBUILD per point and cannot be done from the command line of a shipped
# binary. The optimisation notes record several sweeps whose numbers are now
# unreproducible because the configurations lived in somebody's shell history.
# These targets are that shell history, committed.
#
# Each point builds `test_field_v3_earth_sweep_<name>` and registers no test --
# a sweep is a measurement, not a gate, and the gate is
# `field_v3_earth_quad` at the SHIPPED configuration.
#
#   cmake -S . -B build -DZHAO_FIELD_SWEEP=ON
#   cmake --build build --target field_sweep
#   tools/field/run_sweep.ps1        # runs every point and tabulates
option(ZHAO_FIELD_SWEEP "Build the Field v3 Earth parameter sweep targets" OFF)

set(ZHAO_FIELD_SWEEP_POINTS
  # name              CTX OUTSTANDING LANES LONGQ DIST_BANKS RING_UNITS REGS GATHERS
  #
  # ROUND 7. Every earlier round is stale AGAIN: the ring descriptor cache, the
  # DIST2 front-end pipeline and the shortened curve search have each moved the
  # bottleneck since. LEN/DIST2 is now the top refuser on all three programs --
  # 4,266 clocks on impact_wave, 3,359 on crater_ring, 1,369 on wave_pool -- so
  # this round is about the root banks, which two earlier rounds rejected while
  # the wall was somewhere else entirely.
  "base|32|12|4|16|4|8|64|4"
  "dist8|32|12|4|16|8|8|64|4"
  "dist16|32|12|4|16|16|8|64|4"
  "dist8out16|32|16|4|16|8|8|64|4"
  "dist8longq8|32|12|4|8|8|8|64|4"
  "dist8ctx48|48|12|4|16|8|8|64|4"
)

if(ZHAO_FIELD_SWEEP)
  add_custom_target(field_sweep)
  foreach(pt IN LISTS ZHAO_FIELD_SWEEP_POINTS)
    string(REPLACE "|" ";" f "${pt}")
    list(GET f 0 sw_name)
    list(GET f 1 sw_ctx)
    list(GET f 2 sw_out)
    list(GET f 3 sw_lanes)
    list(GET f 4 sw_longq)
    list(GET f 5 sw_dist)
    list(GET f 6 sw_ring)
    list(GET f 7 sw_regs)
    list(GET f 8 sw_gath)
    set(tgt test_field_v3_earth_sweep_${sw_name})
    add_executable(${tgt} differential/field_v3_earth_directed.cpp)
    target_link_libraries(${tgt} PRIVATE zhao_harness zhao_zref)
    verilate(${tgt}
      PREFIX Vzhao_field_v3_engine
      TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
      VERILATOR_ARGS -GCTX=${sw_ctx} -GOUTSTANDING=${sw_out} -GLANES=${sw_lanes}
                     -GLONGQ=${sw_longq} -GDIST_BANKS=${sw_dist}
                     -GRING_UNITS=${sw_ring} -GREGS=${sw_regs} -GGATHERS=${sw_gath})
    target_compile_definitions(${tgt} PRIVATE
      ZHAO_EARTH_CTX=${sw_ctx} ZHAO_EARTH_LANES=${sw_lanes} ZHAO_EARTH_REGS=${sw_regs})
    add_dependencies(field_sweep ${tgt})
  endforeach()
endif()

add_executable(test_field_v3_earth_quad differential/field_v3_earth_directed.cpp)
target_link_libraries(test_field_v3_earth_quad PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_earth_quad
  PREFIX Vzhao_field_v3_engine
  TOP_MODULE zhao_field_v3_engine
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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
  # DIST_BANKS 8 and OUTSTANDING 16, adopted 2026-08-30 from sweep round 7.
  # Both were REJECTED in earlier rounds -- DIST_BANKS 8 twice, as "double the
  # roots for no change" -- while the wall was the ring service's eight-clock
  # accept path and then the curve search. With those gone, LEN/DIST2 became
  # the top refuser on all three programs and the root banks finally matter:
  #
  #   base (DIST 4, OUT 12)   crater 444,578  impact 553,680  wave 555,586
  #   DIST 8                         427,434         456,000         540,655
  #   DIST 8 + OUT 16                420,669         505,030         461,604
  #   DIST 16                  identical to DIST 8, so eight is the stopping point
  #
  # AREA IS UNMEASURED. Eight banks of four lanes is thirty-two floor-exact
  # roots, and the LEN header prices eight at roughly 2,000 ALMs. Re-fit in
  # Quartus before quoting any resource number for this configuration.
  VERILATOR_ARGS -GCTX=32 -GOUTSTANDING=16 -GLANES=4 -GLONGQ=16 -GDIST_BANKS=8 -GRING_UNITS=8 -GREGS=64)
target_compile_definitions(test_field_v3_earth_quad PRIVATE ZHAO_EARTH_CTX=32 ZHAO_EARTH_LANES=4 ZHAO_EARTH_REGS=64)
# --points 1024, NOT 256. The admission law is about an INITIATION INTERVAL,
# which is a steady-state quantity, and at 256 points a CTX=32 LANES=4 machine
# spends most of the run filling and draining its contexts. The same binary
# reads 28 clocks per group at 256 points and 19 at 1024 -- the difference
# between reporting 1.23x OVER and reporting a 22% margin. The gate measured
# the ramp for as long as it was registered at 256.
add_test(NAME field_v3_earth_quad COMMAND test_field_v3_earth_quad --points 1024
         ${CMAKE_SOURCE_DIR}/compiler/tests/generated/crater_ring.zprog
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

# ---------------------------------------------------------------------------
# FH11's per-lane saturation attribution, in BOTH polarities from ONE driver.
# ---------------------------------------------------------------------------
# `zhao_field_alu_vec` used to OR its per-lane saturation flags into a single
# group flag, so one point's overflow labelled its neighbours and nothing could
# say WHICH lane saturated. The narrow flags are now carried out per lane.
#
# THE DEFECT THAT REPLACES IT IS INVISIBLE TO AGGREGATES. Re-ORing the group
# flag back into every lane leaves `sat_add_o` bit-identical, so only a check
# that names a lane can catch it. That is what these two targets are for:
# differential/field_alu_vec_flag_attribution_control.cpp is compiled twice
# from one source with one set of expectations.
add_executable(test_field_alu_vec_flag_attribution
               differential/field_alu_vec_flag_attribution_control.cpp)
target_link_libraries(test_field_alu_vec_flag_attribution PRIVATE zhao_harness zhao_zref)
verilate(test_field_alu_vec_flag_attribution
  PREFIX Vzhao_field_alu_vec
  TOP_MODULE zhao_field_alu_vec
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu_vec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv)
add_test(NAME field_alu_vec_flag_attribution COMMAND test_field_alu_vec_flag_attribution)
set_tests_properties(field_alu_vec_flag_attribution PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# THE SAME DRIVER, THE SAME EXPECTATIONS, AGAINST THE BROKEN COPY. It PASSES
# WHEN THE DRIVER FAILS -- inverse polarity, deliberately, because this is
# evidence about the instrument and not about the design.
#
# If the `ZHAO_ALUVEC_MUTANT` selector ever failed to engage, this build would
# compile against the production class, the driver would pass, and ctest would
# report THIS test as failed because it expected a failure. The silent-`-D`
# trap CLAUDE.md records therefore cannot hide here.
add_executable(test_field_alu_vec_flag_broadcast_control
               differential/field_alu_vec_flag_attribution_control.cpp)
target_compile_definitions(test_field_alu_vec_flag_broadcast_control
                           PRIVATE ZHAO_ALUVEC_MUTANT=1)
target_link_libraries(test_field_alu_vec_flag_broadcast_control
                      PRIVATE zhao_harness zhao_zref)
verilate(test_field_alu_vec_flag_broadcast_control
  PREFIX Vzhao_field_alu_vec_flag_broadcast_mutant
  TOP_MODULE zhao_field_alu_vec_flag_broadcast_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_alu_vec_flag_broadcast_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_alu.sv)
add_test(NAME field_alu_vec_flag_broadcast_control
         COMMAND test_field_alu_vec_flag_broadcast_control)
set_tests_properties(field_alu_vec_flag_broadcast_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300 WILL_FAIL TRUE)

# THE GEOMETRY -> RASTER SEAM. Both sides are individually green and have
# never been joined: the binner decides which tiles a triangle touches and
# the raster pipe renders exactly those jobs in exactly that order. Every
# composed machine this project has built found something its blocks could
# not.
add_executable(test_render_pipe_directed render/render_pipe_directed.cpp)
target_link_libraries(test_render_pipe_directed PRIVATE zhao_harness zhao_zref)
verilate(test_render_pipe_directed
  PREFIX Vzhao_geom_bin_pipe
  TOP_MODULE zhao_geom_bin_pipe
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_bin_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_binner.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_arena.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv)
add_executable(test_render_fb_directed render/render_fb_directed.cpp)
target_link_libraries(test_render_fb_directed PRIVATE zhao_harness zhao_zref)
verilate(test_render_fb_directed
  PREFIX Vzhao_probe_render_fb
  TOP_MODULE zhao_probe_render_fb
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_render_fb.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fbwrite.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_bin_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_binner.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_arena.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv)
add_executable(test_geom_attrsetup_directed geometry/geom_attrsetup_directed.cpp)
target_link_libraries(test_geom_attrsetup_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_attrsetup_directed
  PREFIX Vzhao_geom_attrsetup
  TOP_MODULE zhao_geom_attrsetup
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrsetup.sv)
add_executable(test_raster_attrdiv_directed raster/raster_attrdiv_directed.cpp)
target_link_libraries(test_raster_attrdiv_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_attrdiv_directed
  PREFIX Vzhao_raster_attrdiv
  TOP_MODULE zhao_raster_attrdiv
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv.sv)
add_test(NAME raster_attrdiv_directed COMMAND test_raster_attrdiv_directed)
set_tests_properties(raster_attrdiv_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The exact quotient/remainder stepping proof (owner ruling 2026-08-31 #2).
# Pure arithmetic, no RTL: it answers whether the per-pixel divide can be
# replaced without moving a rendered bit, and whether the ruling's stated
# formula is the shipped law. It is a GATE rather than a loose file because its
# first finding is a rounding difference that would otherwise be rediscovered.
add_executable(test_attribute_step_equivalence proofs/attribute_step_equivalence.cpp)
add_test(NAME attribute_step_equivalence COMMAND test_attribute_step_equivalence)
set_tests_properties(attribute_step_equivalence PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ---- RASTER.ATTRDIV V2's published floor remainder (owner ruling R104) ------
# v2 gained `rem_o` and `rem_range_err_o` on 2026-09-20 so the two remaining
# superseded-in-a-production-root sites could move off v1. Three targets:
#
#   raster_attrdiv_v2_rem_directed     the pair's Euclidean invariant, the width
#                                      claim R104 asked to be MEASURED, and the
#                                      continuity across zero that lets a
#                                      consumer drop v1's second sign branch.
#   raster_attrdiv_v2_rem_r4           the same case list at radix 4, because a
#                                      divider whose answer moves with the radix
#                                      is two dividers.
#   raster_attrdiv_v2_remwidth_mutant  the guard's POSITIVE CONTROL, inverted
#                                      polarity: it passes when the counter
#                                      fires. The mutant source is compiled
#                                      IMMEDIATELY BEFORE the production file so
#                                      its `ifndef` seam sees the macro defined.
#
# The unmutated targets above are that mutant's negative control: they build the
# identical C++ against unmutated RTL and require the counter to stay at zero.
add_executable(test_raster_attrdiv_v2_rem_directed raster/raster_attrdiv_v2_rem_directed.cpp)
target_link_libraries(test_raster_attrdiv_v2_rem_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_attrdiv_v2_rem_directed
  PREFIX Vzhao_raster_attrdiv_v2
  TOP_MODULE zhao_raster_attrdiv_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv)
add_test(NAME raster_attrdiv_v2_rem_directed COMMAND test_raster_attrdiv_v2_rem_directed)
set_tests_properties(raster_attrdiv_v2_rem_directed
  PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_raster_attrdiv_v2_rem_r4 raster/raster_attrdiv_v2_rem_directed.cpp)
target_link_libraries(test_raster_attrdiv_v2_rem_r4 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_raster_attrdiv_v2_rem_r4 PRIVATE ZHAO_ATTR_RADIX=4)
verilate(test_raster_attrdiv_v2_rem_r4
  PREFIX Vzhao_raster_attrdiv_v2
  TOP_MODULE zhao_raster_attrdiv_v2
  VERILATOR_ARGS -GRADIX=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv)
add_test(NAME raster_attrdiv_v2_rem_r4 COMMAND test_raster_attrdiv_v2_rem_r4)
set_tests_properties(raster_attrdiv_v2_rem_r4 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_raster_attrdiv_v2_remwidth_mutant raster/raster_attrdiv_v2_rem_directed.cpp)
target_link_libraries(test_raster_attrdiv_v2_remwidth_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_raster_attrdiv_v2_remwidth_mutant PRIVATE EXPECT_REM_WIDTH_MUTANT=1)
verilate(test_raster_attrdiv_v2_remwidth_mutant
  PREFIX Vzhao_raster_attrdiv_v2
  TOP_MODULE zhao_raster_attrdiv_v2
  VERILATOR_ARGS -DZHAO_ATTR_V2_MUTANT_REM_WIDTH
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv)
add_test(NAME raster_attrdiv_v2_remwidth_mutant COMMAND test_raster_attrdiv_v2_remwidth_mutant)
set_tests_properties(raster_attrdiv_v2_remwidth_mutant
  PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The depth-profile derivation (owner ruling 2026-08-31 #1). It derives `scale`
# from wmin/wmax through the frozen rcp_u24 law and proves the four properties
# the ruling demands: the pin at wmin, monotonicity, a non-zero floor at wmax,
# and no intermediate wrap. A gate, because a depth mapping that stops being
# monotonic swaps two surfaces and nothing else would notice.
add_executable(test_depth_profile_law proofs/depth_profile_law.cpp)
target_link_libraries(test_depth_profile_law PRIVATE zhao_zref)
add_test(NAME depth_profile_law COMMAND test_depth_profile_law)
set_tests_properties(depth_profile_law PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The depth ORACLE (DEPTH_PROFILE_NEXT_STEPS.md step 4). depth_profile_law
# derives the constants; this proves zref::depth_of evaluates them correctly,
# so RTL can be differentially tested against a reference instead of against a
# restatement of the law inside its own test.
add_executable(test_depth_oracle_directed proofs/depth_oracle_directed.cpp)
target_link_libraries(test_depth_oracle_directed PRIVATE zhao_zref)
add_test(NAME depth_oracle_directed COMMAND test_depth_oracle_directed)
set_tests_properties(depth_oracle_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The depth-profile ABI (owner ruling 2026-08-31 section 1). Proves flags[1:0]
# decodes, that 0 still means WORLD_LONG so archived captures keep their
# meaning, and that profile 3 is refused rather than aliased.
add_executable(test_depth_profile_abi_directed proofs/depth_profile_abi_directed.cpp)
target_link_libraries(test_depth_profile_abi_directed PRIVATE zhao_zref)
target_include_directories(test_depth_profile_abi_directed PRIVATE ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME depth_profile_abi_directed COMMAND test_depth_profile_abi_directed)
set_tests_properties(depth_profile_abi_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# RASTER.ATTRSTEP: the recurrence in RTL, checked against the divider it
# replaces. The oracle is the shipped law, and the cases are the ones where a
# two-branch recurrence can go wrong -- sign crossings inside a row and exact
# halves.
add_executable(test_raster_attrstep_directed raster/raster_attrstep_directed.cpp)
target_link_libraries(test_raster_attrstep_directed PRIVATE zhao_harness zhao_zref)
# The oracle is the compiled reference, not a restatement of it (R100/R104).
target_include_directories(test_raster_attrstep_directed PRIVATE ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_raster_attrstep_directed
  PREFIX Vzhao_raster_attrstep
  TOP_MODULE zhao_raster_attrstep
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrstep.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv)
add_test(NAME raster_attrstep_directed COMMAND test_raster_attrstep_directed)
set_tests_properties(raster_attrstep_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# RASTER.TOON: the cel band. The oracle is rast.cpp's own apply_toon_ramp and
# the constants are Zixxtrixx's shipped celmain presentation, so a change that
# alters the creature's appearance fails here rather than in a screenshot.
add_executable(test_raster_toon_directed raster/raster_toon_directed.cpp)
target_link_libraries(test_raster_toon_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_toon_directed
  PREFIX Vzhao_raster_toon
  TOP_MODULE zhao_raster_toon
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_toon.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_toon_div.sv)
add_test(NAME raster_toon_directed COMMAND test_raster_toon_directed)
set_tests_properties(raster_toon_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# RASTER.FOG -- owner ruling D-5's step 5, differential against the reference's
# own mix expression rather than against the spec prose.
add_executable(test_raster_fog_directed raster/raster_fog_directed.cpp)
target_link_libraries(test_raster_fog_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_fog_directed
  PREFIX Vzhao_raster_fog
  TOP_MODULE zhao_raster_fog
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fog.sv)
add_test(NAME raster_fog_directed COMMAND test_raster_fog_directed)
set_tests_properties(raster_fog_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The geometry half of D-5: the per-vertex fog factor, differential against
# zref_fog's frozen law rather than against the spec prose.
add_executable(test_geom_fogfactor_directed geometry/geom_fogfactor_directed.cpp)
target_link_libraries(test_geom_fogfactor_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_fogfactor_directed
  PREFIX Vzhao_geom_fogfactor
  TOP_MODULE zhao_geom_fogfactor
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_fogfactor.sv)
add_test(NAME geom_fogfactor_directed COMMAND test_geom_fogfactor_directed)
set_tests_properties(geom_fogfactor_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# P0-C Stage B: the fragment->sample expander. Its acceptance test checks the
# request SEQUENCE element for element AND the exact per-fragment issue counts,
# because the architecture's falsifier says output equality alone has twice
# hidden a machine doing double work.
add_executable(test_frag_expand_directed texture/frag_expand_directed.cpp)
target_link_libraries(test_frag_expand_directed PRIVATE zhao_harness)
verilate(test_frag_expand_directed
  PREFIX Vzhao_texture_frag_expand
  TOP_MODULE zhao_texture_frag_expand
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv)
# THE POSITIVE CONTROL FOR wq_overflow_o, driving a committed MUTANT.
#
# frag_expand_directed asserts wq_overflow_o == 0 under correct operation, and on
# its own that is a hopeful zero: the counter watches a state violation that is
# unreachable while the full-guard is right, so NO stimulus can move it. The only
# way to show it alive is to break the guard.
#
# tests/mutants/zhao_texture_frag_expand_mutant.sv is that break, kept as a
# separate renamed file rather than a temporary edit -- a temporary edit to
# production RTL is a live-tree hazard and leaves no evidence behind. This is what
# "keep the failing mutant" means when the mutant cannot live in shipped source.
#
# THIS TEST PASSES WHEN THE COUNTER FIRES. Inverse polarity, deliberately: it is
# evidence about the instrument, not about the design.
add_executable(test_frag_expand_overflow_control texture/frag_expand_overflow_control.cpp)
target_link_libraries(test_frag_expand_overflow_control PRIVATE zhao_harness)
verilate(test_frag_expand_overflow_control
  PREFIX Vzhao_texture_frag_expand_mutant
  TOP_MODULE zhao_texture_frag_expand_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_frag_expand_mutant.sv)
add_test(NAME frag_expand_overflow_control
         COMMAND test_frag_expand_overflow_control)
set_tests_properties(frag_expand_overflow_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME frag_expand_directed COMMAND test_frag_expand_directed)
set_tests_properties(frag_expand_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The historical island_composed_directed.cpp remains registered above against
# zhao_texture_island_top.  Its former V3 elaborations are intentionally absent:
# that driver still names frag_depth_i and the old global binding/AUX boundary,
# while Packet B exposes frag_invw24_i, sealed contexts, configuration, and the
# recoverable frame-fault ABI.  Registering it against the repaired top would be
# a compatibility shim in CMake rather than a test of the new interface.

# P0-C Stage C GATE 3: the paired run. Both tops, identical stimulus, retired
# streams compared byte for byte on rgb/a/tag/refused AND on ORDER.
#
# It runs the two EXECUTABLES rather than instantiating both tops in a third
# harness, because a third harness is a second driver and a second driver is a
# copy that drifts. See the docstring in tools/texture/gate3_paired.py.
# Packet C's metadata bank: 256 x 40 = exactly one M10K, one writer, one
# SYNCHRONOUS reader. Not wired into the island -- a leaf the brief asks for
# before the surgery ("use small functional/leaf experiments first").
add_executable(test_metajoin_directed texture/metajoin_directed.cpp)
target_link_libraries(test_metajoin_directed PRIVATE zhao_harness)
verilate(test_metajoin_directed
  PREFIX Vzhao_texture_metajoin
  TOP_MODULE zhao_texture_metajoin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin.sv)
# Decrufter D0: the RAM/join seam. The dispatcher backpressure test stalls the
# CLASS lanes, which are downstream; this exercises bank -> join -> dispatcher,
# which is upstream, and the brief is explicit that one is not a substitute for
# the other.
add_executable(test_metajoin_seam_directed texture/metajoin_seam_directed.cpp)
target_link_libraries(test_metajoin_seam_directed PRIVATE zhao_harness)
verilate(test_metajoin_seam_directed
  PREFIX Vzhao_texture_metajoin
  TOP_MODULE zhao_texture_metajoin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin.sv)
# The typed early descriptor bank (Decrufter Â§5.2/Â§5.4), built with D0's hold
# law from the start rather than after a defect, and with the owner generation
# on an OUTPUT PORT -- the gap that leaves metajoin's island packing 8'd0 where
# the identity would travel.
add_executable(test_early_desc_directed texture/early_desc_directed.cpp)
target_link_libraries(test_early_desc_directed PRIVATE zhao_harness)
verilate(test_early_desc_directed
  PREFIX Vzhao_texture_early_desc
  TOP_MODULE zhao_texture_early_desc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv)
# The post-PERSPUV join (Decrufter Â§5.3), composed with the REAL descriptor
# bank. A stubbed descriptor source would let the test author decide when the
# descriptor changes, which is the property under test.
add_executable(test_uv_join_directed texture/uv_join_directed.cpp)
target_link_libraries(test_uv_join_directed PRIVATE zhao_harness)
verilate(test_uv_join_directed
  PREFIX Vtb_uv_join_pair
  TOP_MODULE tb_uv_join_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_uv_join_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv)
# PACKET 2's acceptance test, run BEFORE packet 2: bank -> join -> the REAL
# expander. When the island rewire lands, a red composed suite then means the
# WIRING by elimination, instead of a bisect through a 2,900-line file.
add_executable(test_desc_join_expand_directed texture/desc_join_expand_directed.cpp)
target_link_libraries(test_desc_join_expand_directed PRIVATE zhao_harness)
verilate(test_desc_join_expand_directed
  PREFIX Vtb_desc_join_expand
  TOP_MODULE tb_desc_join_expand
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_desc_join_expand.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv)
add_test(NAME desc_join_expand_directed COMMAND test_desc_join_expand_directed)
set_tests_properties(desc_join_expand_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# TEXJOIN retirement control: independently scoreboard every field of the
# accepted descriptor at the real join->expander handshake and every emitted
# {slot,generation,sample} request while all consumers stall independently.
add_executable(test_desc_identity_stall_control
  texture/desc_identity_stall_control.cpp)
target_link_libraries(test_desc_identity_stall_control PRIVATE zhao_harness)
verilate(test_desc_identity_stall_control
  PREFIX Vtb_desc_join_expand
  TOP_MODULE tb_desc_join_expand
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_desc_join_expand.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv)
add_test(NAME desc_identity_stall_control
         COMMAND test_desc_identity_stall_control)

# Positive control, inverse polarity. The renamed mutant fetches the adjacent
# descriptor row while its generation and aggregate counters still describe the
# requested slot. This passes only when the independent scoreboard fires.
add_executable(test_desc_identity_slotswap_control
  texture/desc_identity_stall_control.cpp)
target_compile_definitions(test_desc_identity_slotswap_control PRIVATE
  ZHAO_DESC_SLOTSWAP_CONTROL=1)
target_link_libraries(test_desc_identity_slotswap_control PRIVATE zhao_harness)
verilate(test_desc_identity_slotswap_control
  PREFIX Vtb_desc_join_expand
  TOP_MODULE tb_desc_join_expand_slotswap_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_desc_join_expand.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_early_desc_slotswap_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv
  VERILATOR_ARGS -DZHAO_DESC_SLOTSWAP_MUTANT)
add_test(NAME desc_identity_slotswap_control
         COMMAND test_desc_identity_slotswap_control)

# Persistence positive control, inverse polarity. This renamed wrapper withdraws
# request valid after it was observed stalled; the checker must retain its hold
# obligation through the missing-valid cycle and fire before any timeout.
add_executable(test_desc_identity_valid_withdraw_control
  texture/desc_identity_stall_control.cpp)
target_compile_definitions(test_desc_identity_valid_withdraw_control PRIVATE
  ZHAO_DESC_VALID_WITHDRAW_CONTROL=1)
target_link_libraries(test_desc_identity_valid_withdraw_control PRIVATE zhao_harness)
verilate(test_desc_identity_valid_withdraw_control
  PREFIX Vtb_desc_join_expand
  TOP_MODULE tb_desc_join_expand_valid_withdraw_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/tb_desc_join_expand_valid_withdraw_mutant.sv
          ${CMAKE_SOURCE_DIR}/tests/texture/tb_desc_join_expand.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv)
add_test(NAME desc_identity_valid_withdraw_control
         COMMAND test_desc_identity_valid_withdraw_control)

# Conservation positive control, inverse polarity. The renamed wrapper consumes
# one newly-presented AUX beat internally without exposing its valid. No stalled
# valid is withdrawn, so only exact queue/count conservation may license this lane.
add_executable(test_desc_identity_substream_drop_control
  texture/desc_identity_stall_control.cpp)
target_compile_definitions(test_desc_identity_substream_drop_control PRIVATE
  ZHAO_DESC_SUBSTREAM_DROP_CONTROL=1)
target_link_libraries(test_desc_identity_substream_drop_control PRIVATE zhao_harness)
verilate(test_desc_identity_substream_drop_control
  PREFIX Vtb_desc_join_expand
  TOP_MODULE tb_desc_join_expand_substream_drop_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/tb_desc_join_expand_substream_drop_mutant.sv
          ${CMAKE_SOURCE_DIR}/tests/texture/tb_desc_join_expand.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand.sv)
add_test(NAME desc_identity_substream_drop_control
         COMMAND test_desc_identity_substream_drop_control)

set_tests_properties(
  desc_identity_stall_control desc_identity_slotswap_control
  desc_identity_valid_withdraw_control desc_identity_substream_drop_control
  PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME uv_join_directed COMMAND test_uv_join_directed)
set_tests_properties(uv_join_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_texture_uv_join
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_uv_join
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join.sv)
set_tests_properties(lint_texture_uv_join PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE LAYOUT GUARD, SHOWN TO FIRE.
#
# zhao_texture_early_desc carries an elaboration check -- `if (DESCW != 119)
# $fatal(...)` -- whose whole purpose is to refuse a field added or resized
# without updating the reviewed profile. A guard that has never been seen to fire
# is not a guard, and this one is easy to get wrong twice over:
#
#   * it sits inside `// synthesis translate_off`, which reads like "simulators
#     skip this". They do NOT: Verilator parses and elaborates the block, proven
#     by planting a syntax error inside it and watching lint reject it. So the
#     check IS live in every simulation of this module.
#   * `--lint-only` does not run `initial` blocks, so linting at a broken
#     parameter passes cleanly and proves nothing.
#
# GENW=9 pushes the layout to 120 bits, so the first $fatal must fire at time 0.
# The test PASSES when the fatal text appears -- it is a positive control, not a
# functional test, and the C++ never gets far enough for its own checks to matter.
add_executable(test_early_desc_layout_guard texture/early_desc_directed.cpp)
target_link_libraries(test_early_desc_layout_guard PRIVATE zhao_harness)
verilate(test_early_desc_layout_guard
  PREFIX Vzhao_texture_early_desc
  TOP_MODULE zhao_texture_early_desc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv
  VERILATOR_ARGS -GGENW=9)
# Run through the expect-fatal wrapper rather than directly. The control dies
# on purpose, and Verilator's abort path does not return under ctest's pipes on
# Windows -- this printed its expected text in 12 ms standalone and then sat at
# 0 CPU for the full 300 s timeout under ctest, which read as a guard that had
# stopped firing. The wrapper closes stdin, bounds the run, and judges it on
# what it printed.
add_test(NAME early_desc_layout_guard
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_early_desc_layout_guard>
    -DEXPECT=layout\ is\ 120\ bits,\ not\ the\ reviewed\ 119
    -DRUN_TIMEOUT=10
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(early_desc_layout_guard PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120)

# The wrapper's OWN negative control. It reports a pass on a process that never
# exits, so the branch that must be shown to work is the one where the expected
# diagnostic is ABSENT -- otherwise the wrapper would launder every hang into a
# green. This points it at the healthy directed build, which runs the same
# module without the broken parameter and therefore never prints the fatal.
add_test(NAME early_desc_layout_guard_wrapper_control
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_early_desc_directed>
    -DEXPECT=layout\ is\ 120\ bits,\ not\ the\ reviewed\ 119
    -DRUN_TIMEOUT=60
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(early_desc_layout_guard_wrapper_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120 WILL_FAIL TRUE)

add_test(NAME early_desc_directed COMMAND test_early_desc_directed)
set_tests_properties(early_desc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_texture_early_desc
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_early_desc
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc.sv)
set_tests_properties(lint_texture_early_desc PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME metajoin_seam_directed COMMAND test_metajoin_seam_directed)
set_tests_properties(metajoin_seam_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# metajoin's layout break, shown to be REFUSED -- and by a different mechanism
# than early_desc's, which is the interesting part.
#
# Both blocks derive their field offsets and both carry an elaboration $fatal.
# Break each with GENW=9 and they fail differently:
#
#   early_desc  lint passes CLEAN (RC=0). PADW absorbs the extra bit, no select
#               goes out of range, and the ONLY thing that catches it is the
#               $fatal at time 0. That guard is load-bearing --
#               see early_desc_layout_guard.
#   metajoin    lint REFUSES it outright: "Selection index out of range: 41:33
#               outside 39:0", plus a WIDTHTRUNC on the packed write. Verilator
#               catches this one at elaboration, so here the $fatal is a
#               BACKSTOP for Quartus rather than the primary detection.
#
# So this is a lint test, not an executable: the point is that the broken
# parameterisation cannot even elaborate. A binary target would fail to BUILD,
# which ctest cannot distinguish from a broken test.
#
# What neither control establishes: whether QUARTUS honours the $fatal. That
# needs a fit and is not claimed.
add_test(NAME metajoin_layout_guard
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall -GGENW=9
                 --top-module zhao_texture_metajoin
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin.sv)
set_tests_properties(metajoin_layout_guard PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}"
  PASS_REGULAR_EXPRESSION "Selection index out of range")

add_test(NAME metajoin_directed COMMAND test_metajoin_directed)
set_tests_properties(metajoin_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# Owner brief 4.1: the identity encoding freeze, checked bit-for-bit.
#
# This one IS registered: the package is new, nothing depends on it yet, and
# the test passes today. Its job is to make the NEXT width change edit one
# file instead of being rediscovered in five places.
add_executable(test_ident_encoding_directed texture/ident_encoding_directed.cpp)
target_link_libraries(test_ident_encoding_directed PRIVATE zhao_harness)
verilate(test_ident_encoding_directed
  PREFIX Vzhao_texture_ident_probe
  TOP_MODULE zhao_texture_ident_probe
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_ident_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_ident_probe.sv)
add_test(NAME ident_encoding_directed COMMAND test_ident_encoding_directed)
set_tests_properties(ident_encoding_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The pre-Packet-B island_v3_fault_directed.cpp is retained as source history but
# is not registered against the repaired top: it targets the removed V3 boundary.
# Packet-B fault registration belongs to a driver that names the canonical clear,
# status, context, and Surface Sheet ports and uses the exact closure below.

# Packet-B tooling and every Python registration below are fail-closed.  A
# configure without the environment's exact CPython is not a reduced gate.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME geom_attrsetup_directed COMMAND test_geom_attrsetup_directed)
set_tests_properties(geom_attrsetup_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The divide service, one elaborated build per UNITS. The sweep is the point:
# a single divider measures 36 clocks and is ~6x short of a frame's depth
# interpolation on its own, so how much divide the renderer needs is a
# measurement against real hardware at each point, not an argument.
# The divider itself at both radices: the answers must be IDENTICAL and only
# the clock count may move, which is what makes radix 4 a speed-up rather than
# a different divider.
add_executable(test_raster_attrdiv_r4 raster/raster_attrdiv_directed.cpp)
target_link_libraries(test_raster_attrdiv_r4 PRIVATE zhao_harness zhao_zref)
verilate(test_raster_attrdiv_r4
  PREFIX Vzhao_raster_attrdiv
  TOP_MODULE zhao_raster_attrdiv
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv.sv
  VERILATOR_ARGS -GRADIX=4)
add_test(NAME raster_attrdiv_r4 COMMAND test_raster_attrdiv_r4)
set_tests_properties(raster_attrdiv_r4 PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

foreach(div_units 1 2 4 8)
  set(div_tgt test_raster_attrdiv_svc_u${div_units})
  add_executable(${div_tgt} raster/raster_attrdiv_svc_directed.cpp)
  target_link_libraries(${div_tgt} PRIVATE zhao_harness zhao_zref)
  target_compile_definitions(${div_tgt} PRIVATE ZHAO_SVC_UNITS=${div_units})
  # The oracle is the compiled reference, not a restatement of it (R100/R104).
  target_include_directories(${div_tgt} PRIVATE ${CMAKE_SOURCE_DIR}/reference/src)
  verilate(${div_tgt}
    PREFIX Vzhao_raster_attrdiv_svc
    TOP_MODULE zhao_raster_attrdiv_svc
    SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_svc.sv
            ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv
    VERILATOR_ARGS -GUNITS=${div_units})
  add_test(NAME raster_attrdiv_svc_u${div_units} COMMAND ${div_tgt})
  set_tests_properties(raster_attrdiv_svc_u${div_units}
    PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

  # The same points at radix 4, so the sweep is a GRID: more units and shorter
  # units cost different things, and only a fit can say which is cheaper.
  set(div_tgt4 test_raster_attrdiv_svc_u${div_units}_r4)
  add_executable(${div_tgt4} raster/raster_attrdiv_svc_directed.cpp)
  target_link_libraries(${div_tgt4} PRIVATE zhao_harness zhao_zref)
  target_compile_definitions(${div_tgt4} PRIVATE ZHAO_SVC_UNITS=${div_units} ZHAO_SVC_RADIX=4)
  target_include_directories(${div_tgt4} PRIVATE ${CMAKE_SOURCE_DIR}/reference/src)
  verilate(${div_tgt4}
    PREFIX Vzhao_raster_attrdiv_svc
    TOP_MODULE zhao_raster_attrdiv_svc
    SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_svc.sv
            ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv
    VERILATOR_ARGS -GUNITS=${div_units} -GRADIX=4)
  add_test(NAME raster_attrdiv_svc_u${div_units}_r4 COMMAND ${div_tgt4})
  set_tests_properties(raster_attrdiv_svc_u${div_units}_r4
    PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
endforeach()

add_executable(test_raster_attrinterp_directed raster/raster_attrinterp_directed.cpp)
target_link_libraries(test_raster_attrinterp_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_attrinterp_directed
  PREFIX Vzhao_raster_attrinterp
  TOP_MODULE zhao_raster_attrinterp
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrinterp.sv)
add_test(NAME raster_attrinterp_directed COMMAND test_raster_attrinterp_directed)
set_tests_properties(raster_attrinterp_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_geom_clip_attrswap_directed geometry/geom_clip_attrswap_directed.cpp)
target_link_libraries(test_geom_clip_attrswap_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_clip_attrswap_directed
  PREFIX Vzhao_geom_clip
  TOP_MODULE zhao_geom_clip
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clip.sv)
add_test(NAME geom_clip_attrswap_directed COMMAND test_geom_clip_attrswap_directed)
set_tests_properties(geom_clip_attrswap_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# rcp_u24 for the raster: walked over its ENTIRE 16.7M input domain against the
# frozen RCP24_FULL_HASH, so it is nightly rather than fast.
add_executable(test_raster_rcp24_directed raster/raster_rcp24_directed.cpp)
target_link_libraries(test_raster_rcp24_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_rcp24_directed
  PREFIX Vzhao_raster_rcp24
  TOP_MODULE zhao_raster_rcp24
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME raster_rcp24_directed COMMAND test_raster_rcp24_directed)
set_tests_properties(raster_rcp24_directed PROPERTIES LABELS "nightly" TIMEOUT 1800)

# The EMPIRICAL backstop for the axis-lockstep proof (roadmap packet 4 step 2).
# reports/PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md proves by induction that the
# two per-axis work queues cannot diverge, which is what licenses the paired
# rewrite to keep one scheduler. A proof that licenses a deletion is the
# comfortable kind, so the invariant is asserted every cycle against the real
# RTL as well.
#
# --public-flat-rw is needed to see wq_wp/wq_rp/pk_v/pk_i. It is a VERILATE flag,
# not a source edit, which is why this could be written while the svc file sat
# inside a running island fit closure.
# The paired PERSPUV candidate against the frozen service (roadmap packet 4).
# One scheduler, two lanes, one paired record -- the operand tables, the e_have
# join and the e_q_u/e_q_v result tables are gone because there is nothing left
# to rejoin.
add_executable(test_perspuv_pairpipe_directed raster/perspuv_pairpipe_directed.cpp)
target_link_libraries(test_perspuv_pairpipe_directed PRIVATE zhao_harness)
verilate(test_perspuv_pairpipe_directed
  PREFIX Vtb_perspuv_pairpipe
  TOP_MODULE tb_perspuv_pairpipe
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_perspuv_pairpipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_pairpipe.sv)
add_test(NAME perspuv_pairpipe_directed COMMAND test_perspuv_pairpipe_directed)
set_tests_properties(perspuv_pairpipe_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 600)

add_test(NAME lint_raster_perspuv_pairpipe
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_perspuv_pairpipe
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_pairpipe.sv)
set_tests_properties(lint_raster_perspuv_pairpipe PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_perspuv_lockstep_directed raster/perspuv_lockstep_directed.cpp)
target_link_libraries(test_perspuv_lockstep_directed PRIVATE zhao_harness)
verilate(test_perspuv_lockstep_directed
  PREFIX Vzhao_raster_perspuv_svc
  TOP_MODULE zhao_raster_perspuv_svc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_svc.sv
  VERILATOR_ARGS --public-flat-rw)
add_test(NAME perspuv_lockstep_directed COMMAND test_perspuv_lockstep_directed)
set_tests_properties(perspuv_lockstep_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_raster_perspuv_directed raster/raster_perspuv_directed.cpp)
target_link_libraries(test_raster_perspuv_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_perspuv_directed
  PREFIX Vzhao_raster_perspuv
  TOP_MODULE zhao_raster_perspuv
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME raster_perspuv_directed COMMAND test_raster_perspuv_directed)
set_tests_properties(raster_perspuv_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_raster_texjoin_directed raster/raster_texjoin_directed.cpp)
target_link_libraries(test_raster_texjoin_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_texjoin_directed
  PREFIX Vzhao_raster_texjoin
  TOP_MODULE zhao_raster_texjoin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_texjoin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME raster_texjoin_directed COMMAND test_raster_texjoin_directed)
set_tests_properties(raster_texjoin_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TEXJOIN v2 lands BESIDE v1 (REARCHITECTUREADVICE.md), so it gets its own
# executable and its own test rather than replacing the golden one.
add_executable(test_raster_texjoin_v2_directed raster/raster_texjoin_v2_directed.cpp)
target_link_libraries(test_raster_texjoin_v2_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_texjoin_v2_directed
  PREFIX Vzhao_raster_texjoin_v2
  TOP_MODULE zhao_raster_texjoin_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_texjoin_v2.sv)
add_test(NAME raster_texjoin_v2_directed COMMAND test_raster_texjoin_v2_directed)
set_tests_properties(raster_texjoin_v2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# MEM.UPLOAD's acceptance law, before its RTL. The contract was written first
# and the owner brief then found two holes in it -- a generation bit does not
# make an IN-PLACE upload atomic, and the HPS source was unguarded while its
# width disagreed with the bridge. Both corrections are pinned here so the RTL
# is written against them rather than around them.
add_executable(test_mem_upload_oracle memory/mem_upload_oracle.cpp)
target_link_libraries(test_mem_upload_oracle PRIVATE zhao_harness zhao_zref)
add_test(NAME mem_upload_oracle COMMAND test_mem_upload_oracle)
set_tests_properties(mem_upload_oracle PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# GEOM.ASSEMBLE -- the arrow nothing owned. BORING_3D_FUNDAMENTALS_AUDIT R1:
# MESHFETCH emits index_offset and triangle_count, VDECODE accepts neither,
# SETUP expects a complete triangle, and tri_ax_i was driven only from a
# harness. The arithmetic is one addition; what is worth testing is the Duo
# per-view offset, the refusal of an index AT the vertex count, and that a
# stalled consumer does not lose position mid-mesh.
add_executable(test_geom_assemble_directed geometry/geom_assemble_directed.cpp)
target_link_libraries(test_geom_assemble_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_assemble_directed
  PREFIX Vzhao_geom_assemble
  TOP_MODULE zhao_geom_assemble
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_assemble.sv)
add_test(NAME geom_assemble_directed COMMAND test_geom_assemble_directed)
set_tests_properties(geom_assemble_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The terrain light law, before any RTL. Owner brief 2026-09-03: put the law in
# ZRef and LOOK at the island under a moving sun first. This cannot say whether
# it looks right -- only the render can -- but it pins the ways the first draft
# was wrong: an int64 overflow at the fx16 rail, a three-way rounding
# disagreement, and ambient made a floor when ratified law makes it an addend.
add_executable(test_terrain_shade_oracle terrain/terrain_shade_oracle.cpp)
target_link_libraries(test_terrain_shade_oracle PRIVATE zhao_harness zhao_zref)
add_test(NAME terrain_shade_oracle COMMAND test_terrain_shade_oracle)
set_tests_properties(terrain_shade_oracle PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# TEX.FRAGROB against the block it replaces. Two DUTs, one stimulus, one model
# of the outside world -- and the comparison is the SEQUENCE of retired
# fragments, not cycle-by-cycle: fragrob has an extra stage on issue and one on
# retire because a banked descriptor cannot be picked combinationally, and that
# latency is the price of the storage being memory rather than flip-flops.
add_executable(test_fragrob_differential texture/fragrob_differential.cpp)
target_link_libraries(test_fragrob_differential PRIVATE zhao_harness)
verilate(test_fragrob_differential
  PREFIX Vzhao_raster_texjoin_v2
  TOP_MODULE zhao_raster_texjoin_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_texjoin_v2.sv)
verilate(test_fragrob_differential
  PREFIX Vzhao_texture_fragrob
  TOP_MODULE zhao_texture_fragrob
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_fragrob.sv)
add_test(NAME fragrob_differential COMMAND test_fragrob_differential)
set_tests_properties(fragrob_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ---------------------------------------------------------------------------
# TEXTURE V3 -- the owner/completion/retire experiment
#
# reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 26.1: the FIRST
# implementation task, and deliberately not a top-level rewrite. It replaces
# the FRAGROB return-to-RAM-write-enable cone (zhao_texture_fragrob.sv:626,
# `if (tmu_ok_c) res_rgb_m[...][...] <= ...`) with a C0..C4 capture / control /
# claim / commit / publish pipeline whose payload write enables are all bare
# flop outputs, and it puts every wide payload into a synchronous M10K-backed
# bank with a declared port and collision law.
#
# THE LINT GATE IS NOT OPTIONAL HERE. A sibling block reached "committed"
# without ever lexing because it had none, and this very file hit
# BADVLTPRAGMA on its first lint -- a comment line beginning with the
# simulator's name is read as a pragma and the file silently stops lexing.
set(ZHAO_TEX_V3OWN_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3own.sv)

add_executable(test_texture_v3own_adversarial texture/texture_v3own_adversarial.cpp)
target_link_libraries(test_texture_v3own_adversarial PRIVATE zhao_harness)
verilate(test_texture_v3own_adversarial
  PREFIX Vzhao_texture_v3own
  TOP_MODULE zhao_texture_v3own
  SOURCES ${ZHAO_TEX_V3OWN_SV}
  # --assert IS LOAD-BEARING, not decoration. Twenty-three Appendix B.7
  # contracts are compiled into the RTL (one-hot bank enables,
  # committed-after-write, ticket-once, in-order emission, output packet
  # stability under stall). Without this flag the simulator silently ignores
  # every one of them and the suite still prints a comforting pass -- the
  # "detector that has not been shown to fire" failure, one level down.
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3own_adversarial COMMAND test_texture_v3own_adversarial)

# Timing4 event boundaries: corrupt only the registered owner generation and
# require the independently clocked identity assertion to fire exactly once.
function(zhao_v3own_timing4_event_control TARGET NAME SELECTOR EXPECTED)
  add_executable(${TARGET} texture/texture_v3own_adversarial.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  verilate(${TARGET}
    PREFIX Vzhao_texture_v3own
    TOP_MODULE zhao_texture_v3own
    SOURCES
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
      ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_v3own_timing4_event_mutants.sv
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3own.sv
    VERILATOR_ARGS --assert -D${SELECTOR})
  add_test(NAME texture_v3own_timing4_${NAME}_control
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_v3own_timing4_event_control.py
      --exe $<TARGET_FILE:${TARGET}> --expected ${EXPECTED})
  set_tests_properties(texture_v3own_timing4_${NAME}_control PROPERTIES
    LABELS "fast;nightly;mutant;packet-b;packet-f" TIMEOUT 600)
endfunction()
zhao_v3own_timing4_event_control(
  t_v3own_t4a admission_identity ZHAO_V3OWN_T4_MUTANT_ADMISSION_STALE
  a_admission_event_identity)
zhao_v3own_timing4_event_control(
  t_v3own_t4r reservation_identity ZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE
  a_reservation_event_identity)
add_test(NAME texture_v3own_timing4_event_selector_collision
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_v3own_timing4_event_control.py
    --collision --verilator "${ZHAO_VERILATOR_LINT}")
set_tests_properties(texture_v3own_timing4_event_selector_collision PROPERTIES
  LABELS "fast;mutant;packet-b;packet-f" TIMEOUT 180
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- source_list_parity is shown to FIRE, not merely to pass --------------
# That gate carries two sanctioned asymmetries -- the generated zhao_abi_pkg
# and the generated Quartus-only top zhao_shell_fit_top -- and an exclusion is
# where a gate goes quietly blind. These are its positive controls: five faults
# it must reject plus the unmutated negative control, run against SUBSTITUTE
# lists in a temp dir, so it never touches the real QSF or this file. CLAUDE.md:
# a detector that has not been shown to fire has not been tested.
add_test(NAME source_list_parity_controls
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/lint/fire_source_list_parity.py)
set_tests_properties(source_list_parity_controls PROPERTIES
  LABELS "lint;fast" TIMEOUT 300)

# ---- every Verilated main must leave through a hard exit ------------------
# tests/harness/zhao_sim.hpp has said since 2026-08-16 that a plain `return`
# from a Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU,
# AFTER every check has passed. NOTHING WAS WATCHING: on 2026-09-16
# proj_service_rowmux_smoke hung a ctest run for 166 s, had been read as an RTL
# parallelism flake for weeks, and a sweep found 26 more mains breaking the
# same rule. A hung test is neither a pass nor a fail and it costs a whole
# suite run. The gate carries its own self-check and knows that
# `return zhao::report_and_exit(...)` is safe (it ends in std::_Exit), as is a
# file-local helper that does the same.
add_test(NAME verilated_exit_path
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/lint/verilated_exit_path.py)
set_tests_properties(verilated_exit_path PROPERTIES
  LABELS "lint;fast" TIMEOUT 300)

# READ_LATE=1 (roadmap 4.2 / Commit4): the adversarial suite deliberately keeps
# elaborating the default, so this elaborates the seam and does what the suite
# cannot -- reads a committed sample back through the plane port, fires
# `ev_src_unpub_o` on every class it names by stimulus at THIS boundary (the
# combiner is outside, so an illegal read is legal input here), and PINS the
# counter's blind spot. A different PREFIX so the two models can coexist.
add_executable(test_texture_v3own_readlate_directed texture/texture_v3own_readlate_directed.cpp)
target_link_libraries(test_texture_v3own_readlate_directed PRIVATE zhao_harness)
verilate(test_texture_v3own_readlate_directed
  PREFIX Vzhao_texture_v3own_rl
  TOP_MODULE zhao_texture_v3own
  SOURCES ${ZHAO_TEX_V3OWN_SV}
  VERILATOR_ARGS --assert -GREAD_LATE=1)
add_test(NAME texture_v3own_readlate_directed COMMAND test_texture_v3own_readlate_directed)
set_tests_properties(texture_v3own_readlate_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
set_tests_properties(texture_v3own_adversarial PROPERTIES
  LABELS "fast;nightly" TIMEOUT 900)

# TEXJOIN retirement bubble gate. Fill the V3 owner's ordered output queue,
# complete a longer prepared run behind it, release ready, then require one
# handshake every clock. This is not the intentional full-to-free admission
# bubble: no new admission occurs during the measured drain.
add_executable(test_texture_v3own_bubble_control
  texture/texture_v3own_bubble_control.cpp)
target_link_libraries(test_texture_v3own_bubble_control PRIVATE zhao_harness)
verilate(test_texture_v3own_bubble_control
  PREFIX Vzhao_texture_v3own
  TOP_MODULE zhao_texture_v3own
  SOURCES ${ZHAO_TEX_V3OWN_SV}
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3own_bubble_control
         COMMAND test_texture_v3own_bubble_control)

# Positive control, inverse polarity. One source line removes output-fire credit
# from the logical retirement head; the renamed mutant must create a visible
# bubble while still draining every owner with exact payload and order.
add_executable(test_texture_v3own_no_same_edge_reload_control
  texture/texture_v3own_bubble_control.cpp)
target_compile_definitions(test_texture_v3own_no_same_edge_reload_control PRIVATE
  ZHAO_NO_SAME_EDGE_RELOAD_CONTROL=1)
target_link_libraries(test_texture_v3own_no_same_edge_reload_control PRIVATE zhao_harness)
verilate(test_texture_v3own_no_same_edge_reload_control
  PREFIX Vzhao_texture_v3own
  TOP_MODULE zhao_texture_v3own_no_same_edge_reload_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_v3own_no_same_edge_reload_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3own_no_same_edge_reload_control
         COMMAND test_texture_v3own_no_same_edge_reload_control)

# Positive control for the new structural reservation identity. The committed
# mutant fails to advance the body read pointer when a direct bypass skips its
# duplicate physical row; the assertion-control driver passes only when the
# retained a_out_structure assertion fires.
add_executable(test_texture_v3own_bypass_pointer_assertion_control
  texture/texture_v3own_bubble_control.cpp)
target_compile_definitions(test_texture_v3own_bypass_pointer_assertion_control PRIVATE
  ZHAO_BYPASS_POINTER_ASSERT_CONTROL=1)
target_link_libraries(test_texture_v3own_bypass_pointer_assertion_control PRIVATE zhao_harness)
verilate(test_texture_v3own_bypass_pointer_assertion_control
  PREFIX Vzhao_texture_v3own
  TOP_MODULE zhao_texture_v3own_bypass_pointer_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_v3own_bypass_pointer_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3own_bypass_pointer_assertion_control
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_v3own_assertion_control.py
    --exe $<TARGET_FILE:test_texture_v3own_bypass_pointer_assertion_control>)
set_tests_properties(
  texture_v3own_bubble_control texture_v3own_no_same_edge_reload_control
  texture_v3own_bypass_pointer_assertion_control
  PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# M6's CORRECTNESS TEST, RUN AS A KNOWN FAILURE UNTIL M6 LANDS.
#
# The owner's V3.1 rearchitecture Â§0: "M6's correctness test must be written at
# M0, even if the interface change lands later. Any existing behavior that
# accepts a final result before COMBINE has accepted the corresponding input is
# not something to preserve for parity."
#
# Case 22 (M6) ran here as a ZHAO_M6-guarded WILL_FAIL lane while the contract
# was recorded but unmet. T4 landed it on 2026-09-07 (383e45df): cbi is now set
# on cmb_fire_c, the real acceptance, and a separate crs bit carries the
# reservation. A WILL_FAIL test that passes is itself a ctest failure -- that was
# the designed signal, it fired, and both the guard and this entry are gone.
# Case 22 now runs in the default bench.

# The default lint models synthesis preprocessing; READ_LATE lint below keeps the
# verification-only assertion cone live, so both sides of the preprocessor seam
# are compiled by a registered gate.
add_test(NAME lint_texture_v3own
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall -DSYNTHESIS
                 --top-module zhao_texture_v3own
                 ${ZHAO_TEX_V3OWN_SV})
set_tests_properties(lint_texture_v3own PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The READ_LATE=1 elaborations are a different netlist; lint them as such.
add_test(NAME lint_texture_v3own_readlate
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_v3own -GREAD_LATE=1
                 ${ZHAO_TEX_V3OWN_SV})
set_tests_properties(lint_texture_v3own_readlate PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_texture_material_combine_v2_readlate
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_material_combine_v2 -GREAD_LATE=1
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v2.sv)
set_tests_properties(lint_texture_material_combine_v2_readlate PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_texture_v3bank
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_v3bank
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv)
set_tests_properties(lint_texture_v3bank PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE READY QUEUE ON ITS OWN. It had a lint lane and no directed test, and the
# owner's control-fabric recovery architecture (2026-09-07 Â§5) names a defect
# in it by hand: occ_o omits ld_q, the outstanding synchronous body read, so it
# can report zero while it still owns a ticket. Â§5.2 asks for "the three-edge
# counterexample" in the queue's directed test; this is that test, and it was
# written to FAIL on the unrepaired block before the repair landed.
add_executable(test_texture_v3rq_directed texture/texture_v3rq_directed.cpp)
target_link_libraries(test_texture_v3rq_directed PRIVATE zhao_harness)
verilate(test_texture_v3rq_directed
  PREFIX Vzhao_texture_v3rq
  TOP_MODULE zhao_texture_v3rq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3rq_directed COMMAND test_texture_v3rq_directed)
set_tests_properties(texture_v3rq_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# ---- T2's identity-only comparison, before any RTL moves -------------------
# The recovery handoff's T2 asks for "independent literal-owner/generation
# checks, THEN replace per-slot generation access", and T1 asks to "preserve a
# tested identity-only comparison rather than one giant patch".
#
# NO verilate(): this compares two REPRESENTATIONS of the same live set, which
# is a mathematical claim rather than a timing one. Keeping it free of a DUT is
# the point -- it stays valid while the owner RTL is being rebuilt underneath.
add_executable(test_texture_v3_window_identity texture/texture_v3_window_identity.cpp)
target_link_libraries(test_texture_v3_window_identity PRIVATE zhao_harness)
add_test(NAME texture_v3_window_identity COMMAND test_texture_v3_window_identity)
set_tests_properties(texture_v3_window_identity PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_texture_v3rq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_texture_v3rq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv)
set_tests_properties(lint_texture_v3rq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The AUX divide as a six-stage pipeline (reports/Addendum). Standalone and
# verified before it is swapped into zhao_texture_aux.sv.
add_executable(test_texture_aux_div6_directed texture/texture_aux_div6_directed.cpp)
target_link_libraries(test_texture_aux_div6_directed PRIVATE zhao_harness)
verilate(test_texture_aux_div6_directed
  PREFIX Vzhao_texture_aux_div6
  TOP_MODULE zhao_texture_aux_div6
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv)
add_test(NAME texture_aux_div6_directed COMMAND test_texture_aux_div6_directed)
set_tests_properties(texture_aux_div6_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The scheduled reciprocal, checked against the SERIAL one in hardware.
# tb_rcp24_pair instantiates both so 'keep the exact existing arithmetic'
# is a comparison rather than a claim.
add_executable(test_raster_rcp24_svc_directed raster/raster_rcp24_svc_directed.cpp)
target_link_libraries(test_raster_rcp24_svc_directed PRIVATE zhao_harness)
verilate(test_raster_rcp24_svc_directed
  PREFIX Vtb_rcp24_pair
  TOP_MODULE tb_rcp24_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_rcp24_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME raster_rcp24_svc_directed COMMAND test_raster_rcp24_svc_directed)
set_tests_properties(raster_rcp24_svc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ---- V3: the RCP arithmetic tile, standalone -------------------------------
# reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt S26.1: "the RCP arithmetic
# tile can establish its own exact-tool packing. Do not merge those changes into
# the composition until the shared record and credit contracts are fixed." So
# this is a THIRD reciprocal in the tree, tested against the first, and nothing
# instantiates it yet.
#
# tb_rcp24_v3_pair carries three instances, not two. The serial block is the
# behavioural oracle as before, but S10.5's high-word correction only engages
# when w > 2^31 and MEASURED over all 16,777,215 denominators the reciprocal's
# own domain tops out at w = 0x401FEF88. A denominator-driven pair test would
# therefore pass with the correction deleted, so zhao_raster_rcp24_mul is
# exposed on its own port and driven over the same (x, w) families
# tools/rtl/architecture_numeric_checks.py uses.
# S5.7's fit fixture, checked for DEGENERACY before it is trusted.
# A registered-hash-sink wrapper fails silently in one way: if the sink is
# constant the fitter deletes the DUT and the gate reports a clean number for an
# empty design. The first version of this probe also drove writes into a full
# queue -- illegal per a_rq_no_write_when_full, invisible in a fit because
# Quartus drops assertions, and caught here in seconds.
add_executable(test_texture_v3rq_probe_sanity texture/texture_v3rq_probe_sanity.cpp)
target_link_libraries(test_texture_v3rq_probe_sanity PRIVATE zhao_harness)
verilate(test_texture_v3rq_probe_sanity
  PREFIX Vzhao_probe_v3rq_queue
  TOP_MODULE zhao_probe_v3rq_queue
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/synth/zhao_probe_v3rq_queue.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_v3rq_probe_sanity COMMAND test_texture_v3rq_probe_sanity)
set_tests_properties(texture_v3rq_probe_sanity PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# S16.3's registered-head wrapper, tested on its OWN contract rather than only
# through rcp24's bench. That bench never saturates the DONE queue, so an
# off-by-two in full_o -- S5.3's "body plus two heads must not advertise D+2" --
# would not be reached by it; and the justification for a head/spare PAIR rather
# than a single registered head is a RATE, which no correctness check sees.
add_executable(test_raster_ticketq_rh_directed raster/raster_ticketq_rh_directed.cpp)
target_link_libraries(test_raster_ticketq_rh_directed PRIVATE zhao_harness)
verilate(test_raster_ticketq_rh_directed
  PREFIX Vzhao_raster_ticketq_rh
  TOP_MODULE zhao_raster_ticketq_rh
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
  VERILATOR_ARGS --assert -GW=8 -GD=16)
add_test(NAME raster_ticketq_rh_directed COMMAND test_raster_ticketq_rh_directed)
set_tests_properties(raster_ticketq_rh_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_raster_rcp24_v3_directed raster/raster_rcp24_v3_directed.cpp)
# zhao_zref too: this test includes zref/zref_rcp.hpp as its oracle, and
# without the zref target it never gets reference/include. The omission made
# a FULL `cmake --build` return rc=1 while every targeted build passed, which
# is the worst shape of breakage here -- it turns the exit code that guards
# against stale binaries into noise everybody learns to skip past.
target_link_libraries(test_raster_rcp24_v3_directed PRIVATE zhao_harness zhao_zref)
verilate(test_raster_rcp24_v3_directed
  PREFIX Vtb_rcp24_v3_pair
  TOP_MODULE tb_rcp24_v3_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_rcp24_v3_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
# THE ISLAND PROFILE, which no fit has ever actually measured.
#
# The standing comparison put zhao_raster_rcp24_v3 at NCTX=16/TOKW=14 against
# zhao_raster_rcp24_svc at NCTX=8/TOKW=8 and read off "smaller, half the DSP,
# 25 MHz faster". Two parameters differ, both flattering v3 -- it carried TWICE
# the contexts -- and eight M10Ks against svc's zero never entered the sentence.
# reports/RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE-20260908.md has the full retraction.
#
# The fit that WOULD have been like-for-like (@island-profile) died in 38
# seconds to a -TopParameters quoting defect, not to anything about the block.
# Before spending another 25-minute fit finding that out, establish the cheap
# half here: is the tile even FUNCTIONALLY correct at the island's NCTX=8 with
# a 14-bit token? A tile that miscomputes at eight contexts is not a candidate
# for the swap whatever it fits at.
add_executable(test_raster_rcp24_v3_island_profile raster/raster_rcp24_v3_directed.cpp)
target_link_libraries(test_raster_rcp24_v3_island_profile PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_raster_rcp24_v3_island_profile PRIVATE ZHAO_RCP_NCTX=8)
verilate(test_raster_rcp24_v3_island_profile
  PREFIX Vtb_rcp24_v3_pair
  TOP_MODULE tb_rcp24_v3_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_rcp24_v3_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  VERILATOR_ARGS -GNCTX=8 -GTOKW=14)
# THE MECHANISM, TESTED RATHER THAN ASSERTED. The claim is that the tile needs
# NCTX >= the ten-clock feedback loop depth to launch every clock. That is a
# prediction, and this repository has scored itself on exactly this distinction:
# predictions about STRUCTURE held, predictions about MAGNITUDE inferred from
# reading source were falsified four times in one day. So NCTX=10 is built and
# run: if it restores the rate the mechanism is measured, and if it does not
# then the loop-depth story is wrong and the report has to say so.
add_executable(test_raster_rcp24_v3_nctx10 raster/raster_rcp24_v3_directed.cpp)
target_link_libraries(test_raster_rcp24_v3_nctx10 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_raster_rcp24_v3_nctx10 PRIVATE ZHAO_RCP_NCTX=10)
verilate(test_raster_rcp24_v3_nctx10
  PREFIX Vtb_rcp24_v3_pair
  TOP_MODULE tb_rcp24_v3_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_rcp24_v3_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  VERILATOR_ARGS -GNCTX=10 -GTOKW=14)
add_test(NAME raster_rcp24_v3_nctx10 COMMAND test_raster_rcp24_v3_nctx10)
set_tests_properties(raster_rcp24_v3_nctx10 PROPERTIES LABELS "nightly" TIMEOUT 900)

# NCTX=12: the PREDICTED sufficient point. Fitting rate = 4L/NCTX to the two
# under-provisioned measurements (8 -> 5.78, 10 -> 4.69) gives L = 11.56 and
# 11.72 clocks. If twelve contexts restore the design rate, the model is a
# tested prediction; if they do not, it is a curve fitted to two points and the
# report has to say so.
add_executable(test_raster_rcp24_v3_nctx12 raster/raster_rcp24_v3_directed.cpp)
target_link_libraries(test_raster_rcp24_v3_nctx12 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_raster_rcp24_v3_nctx12 PRIVATE ZHAO_RCP_NCTX=12)
verilate(test_raster_rcp24_v3_nctx12
  PREFIX Vtb_rcp24_v3_pair
  TOP_MODULE tb_rcp24_v3_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_rcp24_v3_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  VERILATOR_ARGS -GNCTX=12 -GTOKW=14)
add_test(NAME raster_rcp24_v3_nctx12 COMMAND test_raster_rcp24_v3_nctx12)
set_tests_properties(raster_rcp24_v3_nctx12 PROPERTIES LABELS "nightly" TIMEOUT 900)

add_test(NAME raster_rcp24_v3_island_profile
         COMMAND test_raster_rcp24_v3_island_profile)
set_tests_properties(raster_rcp24_v3_island_profile PROPERTIES
  LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME raster_rcp24_v3_directed COMMAND test_raster_rcp24_v3_directed)
set_tests_properties(raster_rcp24_v3_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# S10.9: "An exhaustive sweep of all 2^24 denominator values is practical as a
# repository regression once the implementation exists. This review did not run
# that sweep against the compiled repository reference or the new RTL." It runs
# here, nightly only, because the fast lane has to stay a gate and not a soak.
add_test(NAME raster_rcp24_v3_exhaustive
         COMMAND test_raster_rcp24_v3_directed --exhaustive)
set_tests_properties(raster_rcp24_v3_exhaustive PROPERTIES LABELS "nightly" TIMEOUT 5400)

add_test(NAME lint_raster_rcp24_v3
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_rcp24_v3
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
set_tests_properties(lint_raster_rcp24_v3 PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The arithmetic core linted ALONE as well. It is the module the differential
# drives directly and the module that owns the four multiplier sites; a warning
# that only appears when the tile is elaborated around it would be missed.
add_test(NAME lint_raster_rcp24_mul
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_rcp24_mul
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv)
set_tests_properties(lint_raster_rcp24_mul PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_raster_ticketq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_ticketq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv)
set_tests_properties(lint_raster_ticketq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The scheduled perspective multiply lane. tb_perspuv_pair carries a THIRD
# instance -- a standalone rcp24 -- to derive the same mantissa/exponent the
# reference computes internally, so the two scopes can be compared honestly.
add_executable(test_raster_perspuv_svc_directed raster/raster_perspuv_svc_directed.cpp)
target_link_libraries(test_raster_perspuv_svc_directed PRIVATE zhao_harness)
verilate(test_raster_perspuv_svc_directed
  PREFIX Vtb_perspuv_pair
  TOP_MODULE tb_perspuv_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/raster/tb_perspuv_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME raster_perspuv_svc_directed COMMAND test_raster_perspuv_svc_directed)
set_tests_properties(raster_perspuv_svc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The TMU address planner as five ELASTIC stages, against the shipped pipe.
# tb_tmu_plan_pair carries a one-deep cache model so the pipe keeps flowing;
# the addresses do not depend on the data that comes back.
add_executable(test_texture_tmu_plan_directed texture/texture_tmu_plan_directed.cpp)
target_link_libraries(test_texture_tmu_plan_directed PRIVATE zhao_harness)
verilate(test_texture_tmu_plan_directed
  PREFIX Vtb_tmu_plan_pair
  TOP_MODULE tb_tmu_plan_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_tmu_plan_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_plan.sv)
add_test(NAME texture_tmu_plan_directed COMMAND test_texture_tmu_plan_directed)
set_tests_properties(texture_tmu_plan_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The cache-response FIFO and decode dispatcher: removes the brief's named
# `cache response ready = filter not busy` coupling.
add_executable(test_texture_rsp_dispatch_directed texture/texture_rsp_dispatch_directed.cpp)
target_link_libraries(test_texture_rsp_dispatch_directed PRIVATE zhao_harness)
verilate(test_texture_rsp_dispatch_directed
  PREFIX Vzhao_texture_rsp_dispatch
  TOP_MODULE zhao_texture_rsp_dispatch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch.sv)
# Packet C: the SAME dispatcher with META_EN=1, so the metadata payload is
# exercised at the leaf where it has exactly one possible home. The default-off
# case above is what gate 3's oracle relies on and is covered by that target.
add_executable(test_rsp_dispatch_meta_directed texture/rsp_dispatch_meta_directed.cpp)
target_link_libraries(test_rsp_dispatch_meta_directed PRIVATE zhao_harness)
verilate(test_rsp_dispatch_meta_directed
  PREFIX Vzhao_texture_rsp_dispatch
  TOP_MODULE zhao_texture_rsp_dispatch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch.sv
  # TOKW=18 to match the island. The default is 16, and this test builds
  # 18-bit tokens with the class in [17:16] -- at the default they truncate
  # and the test reports 180 token mismatches that are its own doing.
  VERILATOR_ARGS -GMETA_EN=1 -GTOKW=18)
add_test(NAME rsp_dispatch_meta_directed COMMAND test_rsp_dispatch_meta_directed)
set_tests_properties(rsp_dispatch_meta_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME texture_rsp_dispatch_directed COMMAND test_texture_rsp_dispatch_directed)
set_tests_properties(texture_rsp_dispatch_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# Resident palette slots with generations: removes the 16-way page search
# from the hot path, and closes the reload-under-flight hole a valid bit
# cannot express.
add_executable(test_texture_palette_res_directed texture/texture_palette_res_directed.cpp)
target_link_libraries(test_texture_palette_res_directed PRIVATE zhao_harness)
verilate(test_texture_palette_res_directed
  PREFIX Vzhao_texture_palette_res
  TOP_MODULE zhao_texture_palette_res
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res.sv)
add_test(NAME texture_palette_res_directed COMMAND test_texture_palette_res_directed)
set_tests_properties(texture_palette_res_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The bilinear channel lane: II=1, and bit-identical to the shipped
# combinational bilerp, which tb_bilerp_pair instantiates as the oracle.
add_executable(test_texture_bilerp_lane_directed texture/texture_bilerp_lane_directed.cpp)
target_link_libraries(test_texture_bilerp_lane_directed PRIVATE zhao_harness)
verilate(test_texture_bilerp_lane_directed
  PREFIX Vtb_bilerp_pair
  TOP_MODULE tb_bilerp_pair
  SOURCES ${CMAKE_SOURCE_DIR}/tests/texture/tb_bilerp_pair.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane.sv)
add_test(NAME texture_bilerp_lane_directed COMMAND test_texture_bilerp_lane_directed)
set_tests_properties(texture_bilerp_lane_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# AUX as a pipeline: II=1 acceptance, and a degenerate envelope keeps its
# place in the ordering machinery without emitting a sheet read.
add_executable(test_texture_aux_pipe_directed texture/texture_aux_pipe_directed.cpp)
target_link_libraries(test_texture_aux_pipe_directed PRIVATE zhao_harness)
verilate(test_texture_aux_pipe_directed
  PREFIX Vzhao_texture_aux_pipe
  TOP_MODULE zhao_texture_aux_pipe
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv)
add_test(NAME texture_aux_pipe_directed COMMAND test_texture_aux_pipe_directed)
set_tests_properties(texture_aux_pipe_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The staged texture cache with same-line fill multicast: four taps inside
# one line must cause ONE fetch, and the consumer's ready must not reach
# the accept line.
add_executable(test_texture_cache_pipe_directed texture/texture_cache_pipe_directed.cpp)
target_link_libraries(test_texture_cache_pipe_directed PRIVATE zhao_harness)
verilate(test_texture_cache_pipe_directed
  PREFIX Vzhao_texture_cache_pipe
  TOP_MODULE zhao_texture_cache_pipe
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache_pipe.sv)
add_test(NAME texture_cache_pipe_directed COMMAND test_texture_cache_pipe_directed)
set_tests_properties(texture_cache_pipe_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The island page directory -- first block of the terrain world layer.
# Covers the same-cycle fin+claim hazard that a generation guard alone
# does NOT close, and stale-handle reporting under residency churn.
# The 8 km floating island: representable AND resident-affordable, which are
# different claims. 15,625 patches dense against a 1,024 residency -- only the
# sparse directory makes the island possible at all.
# The island STREAMED: eviction and return, which is the audit's closing ask.
add_executable(test_island_stream_directed terrain/island_stream_directed.cpp)
target_link_libraries(test_island_stream_directed PRIVATE zhao_zref)
add_test(NAME island_stream_directed COMMAND test_island_stream_directed)
set_tests_properties(island_stream_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# SW.STREAM's frame policy at ISLAND SCALE -- the working set (T7), the 32-page
# budget and what is dropped when a camera outruns it, T5's canonical order and
# the seal, staging complete before publication, and a dirty F sheet surviving
# eviction and return. Step 4 of the world-layer build sequence.
# `tools/sw_stream_firetest.py` perturbs the reference model ten ways and
# records what fires; see design/contracts/SW.STREAM.md.
add_executable(test_sw_stream_directed terrain/sw_stream_directed.cpp)
target_link_libraries(test_sw_stream_directed PRIVATE zhao_zref)
add_test(NAME sw_stream_directed COMMAND test_sw_stream_directed)
set_tests_properties(sw_stream_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_island_8km_directed terrain/island_8km_directed.cpp)
target_link_libraries(test_island_8km_directed PRIVATE zhao_zref)
add_test(NAME island_8km_directed COMMAND test_island_8km_directed)
set_tests_properties(island_8km_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# The island RENDERED, and flown across. The directory, the streamer and the
# software reference renderer's terrain path in one place: an island-scale frame
# whose DrawProcedural records come from island::visible_set rather than from a
# hand-registered patch, and the 8 km traversal reports/Missingterrain asks for
# ("the crucial validation should be an 8 km traversal capture, not a static
# image"). Every previous frame in this tree emitted exactly ONE DrawProcedural.
# `--fire=N` perturbs one input so exactly one claim breaks; see the file header.
add_executable(test_island_scene_traversal terrain/island_scene_traversal.cpp)
target_link_libraries(test_island_scene_traversal PRIVATE zhao_zref)
add_test(NAME island_scene_traversal COMMAND test_island_scene_traversal)
set_tests_properties(island_scene_traversal PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# TERRAIN.ISLAND -- the island directory RTL against zref::island::Directory.
# The block's whole job is a MAPPING from a store's hit/miss plus two gates to
# one of four answers, so the test compares outcomes, handles, tags AND the
# four evidence counters against the oracle's ledger.
add_executable(test_island_dir_rtl_directed terrain/island_dir_rtl_directed.cpp)
target_link_libraries(test_island_dir_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_island_dir_rtl_directed
  PREFIX Vtb_island_dir
  TOP_MODULE tb_island_dir
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_island_dir.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_island_dir.sv)
add_test(NAME island_dir_rtl_directed COMMAND test_island_dir_rtl_directed)
set_tests_properties(island_dir_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.COMPCACHE -- the on-chip patch front, against zref::terrain::ComposedLattice.
# The block is a STORE, so the oracle is its CONTENTS AND ADDRESSING: a transposed
# index, a swapped surface plane or a parity served while it is still filling all
# return REAL composed heights from the wrong place, and a lattice wrong by one
# vertex is a terrain that renders. So this compares EVERY vertex of BOTH surfaces
# and EVERY cell against the reference structure rather than sampling, with the
# expectation stream shifted one cycle to pin the registered port, and with both
# the producer and the two request streams stalling so a dropped or duplicated
# answer cannot hide behind a consumer that is always ready.
#
# IT CARRIES BOTH PHASES, and the second is not decoration. Sections 1-15 are the
# exhaustive-directed suite above; section 16 is a RANDOMISED phase over 18,000
# cycles -- six patch generations, drawn stall patterns on the producer and both
# consumers, out-of-range requests mixed into in-range ones, both dual_i values,
# release pulses of every width 1..5 -- checking every answer against
# zref::terrain::ComposedLattice and all seven counters against an expectation
# maintained from the STIMULUS, on every cycle. Its draws come from the HIGH bits
# of a 64-bit LCG and it PRINTS AND ASSERTS its own measured distribution, because
# a phase that reports thousands of cases while testing four is the failure mode
# it exists to avoid.
#
# The two phases catch different things, and that is demonstrated rather than
# claimed: an overrun counter qualified on the rising edge of st_valid_i passes
# every one of the directed checks and fails only here, on the producer that
# streams back to back and never lowers valid. So the ledger may honestly name
# this one file for both `directed:` and `random:`.
add_executable(test_compcache_front_rtl_directed terrain/compcache_front_rtl_directed.cpp)
target_link_libraries(test_compcache_front_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_compcache_front_rtl_directed
  PREFIX Vtb_compcache_front
  TOP_MODULE tb_compcache_front
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_compcache_front.sv
  VERILATOR_ARGS --assert)
add_test(NAME compcache_front_rtl_directed COMMAND test_compcache_front_rtl_directed)
set_tests_properties(compcache_front_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ...and the lint gate every other terrain block already has. This one is not
# decoration: the block was committed with a comment line that BEGAN with the
# simulator's name, which the lexer reads as a pragma, so the file did not lex
# at all -- "%Error-BADVLTPRAGMA: Unknown verilator comment". -Wall then named
# both of the real defects outright ("Bit extraction of array[2047:0] requires
# 11 bit index, not 10 bits" and "Comparison is constant due to unsigned
# arithmetic"). An RTL file with no lint test is an RTL file nothing has read.
add_test(NAME lint_terrain_compcache_front
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_compcache_front
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv)
set_tests_properties(lint_terrain_compcache_front PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# TERRAIN.VISIBLE -- the visible-patch builder, against zref::island::visible_set.
# It COMPOSES TERRAIN.ISLAND rather than reimplementing its gates, so this test
# also watches the directory's own counters: this block is its only client, and
# a query invented or an answer swallowed shows as a divergence between the two
# ledgers that no comparison of the emitted list could see.
# THE ORDER IS ASSERTED, not just the set -- a stream consumer depends on it and
# a permutation draws the same ground.
add_executable(test_visible_rtl_directed terrain/visible_rtl_directed.cpp)
target_link_libraries(test_visible_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_visible_rtl_directed
  PREFIX Vtb_island_visible
  TOP_MODULE tb_island_visible
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_island_dir.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_visible.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_island_visible.sv)
add_test(NAME visible_rtl_directed COMMAND test_visible_rtl_directed)
set_tests_properties(visible_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

add_executable(test_terrain_residency_directed terrain/terrain_residency_directed.cpp)
target_link_libraries(test_terrain_residency_directed PRIVATE zhao_harness)
verilate(test_terrain_residency_directed
  PREFIX Vzhao_terrain_residency
  TOP_MODULE zhao_terrain_residency
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency.sv)
add_test(NAME terrain_residency_directed COMMAND test_terrain_residency_directed)
set_tests_properties(terrain_residency_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# FORGE.PRIM -- six families, one bounded topology generator. The ruling's
# interesting half is what it DELETED: shard burst is a particle population,
# and building it here would put a second particle system in the geometry path.
# The deleted four are not reserved for later, so a seventh family encoding is
# refused -- the way a deleted feature comes back is a generator that quietly
# accepts it.
add_executable(test_forge_prim_directed forge/forge_prim_directed.cpp)
target_link_libraries(test_forge_prim_directed PRIVATE zhao_harness zhao_zref)
verilate(test_forge_prim_directed
  PREFIX Vzhao_forge_prim
  TOP_MODULE zhao_forge_prim
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim.sv)
add_test(NAME forge_prim_directed COMMAND test_forge_prim_directed)
set_tests_properties(forge_prim_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# FORGE.PRIM.EVAL -- the lightning position evaluator (reports/ADDLIGHTNING.md:
# "the missing work is chiefly the procedural position evaluator"). prim owns
# topology; this block owns positions, bit-exact against zref::forge::eval_job.
# The directed test holds determinism under four stall patterns, the caps
# refused-not-clamped, degenerate anchors legal, saturation counted, and the
# comparator SEEN TO FAIL on a planted one-bit lie.
add_executable(test_forge_prim_eval_directed forge/forge_prim_eval_directed.cpp)
target_link_libraries(test_forge_prim_eval_directed PRIVATE zhao_harness zhao_zref)
verilate(test_forge_prim_eval_directed
  PREFIX Vzhao_forge_prim_eval
  TOP_MODULE zhao_forge_prim_eval
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim_eval.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jitter_rom.sv)
add_test(NAME forge_prim_eval_directed COMMAND test_forge_prim_eval_directed)
set_tests_properties(forge_prim_eval_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# JITTER_Q16 -- every generated table entry against the generated oracle copy,
# both read ports, the SIN_Q16 discipline: a generated file goes stale as
# easily as a copied one, because the generator is not run by the build.
add_executable(test_forge_jitter_rom_directed forge/forge_jitter_rom_directed.cpp)
target_link_libraries(test_forge_jitter_rom_directed PRIVATE zhao_harness zhao_zref)
verilate(test_forge_jitter_rom_directed
  PREFIX Vzhao_forge_jitter_rom
  TOP_MODULE zhao_forge_jitter_rom
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jitter_rom.sv)
add_test(NAME forge_jitter_rom_directed COMMAND test_forge_jitter_rom_directed)
set_tests_properties(forge_jitter_rom_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# THE POSITIVE CONTROL FOR walk_overrun_o, driving a committed MUTANT.
# forge_prim_eval_directed asserts walk_overrun_o == 0, and on its own that is
# a hopeful zero: the counter watches a state violation unreachable while the
# S_E1 advance compare is right, so no stimulus can move it. The mutant breaks
# that compare; THIS TEST PASSES WHEN THE COUNTER FIRES -- inverse polarity,
# evidence about the instrument, not about the design.
add_executable(test_forge_prim_eval_overrun_control forge/forge_prim_eval_overrun_control.cpp)
target_link_libraries(test_forge_prim_eval_overrun_control PRIVATE zhao_harness)
verilate(test_forge_prim_eval_overrun_control
  PREFIX Vzhao_forge_prim_eval_mutant
  TOP_MODULE zhao_forge_prim_eval_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_forge_prim_eval_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jitter_rom.sv)
add_test(NAME forge_prim_eval_overrun_control COMMAND test_forge_prim_eval_overrun_control)
set_tests_properties(forge_prim_eval_overrun_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ---- the FORGE_PROGRAM page, spec/cartridge.md 4d (owner decision R234 D2) --
# There are THREE statements of that layout -- the spec section,
# `zref::forge_page` and `tools/pack/mkforgeprogram.py` -- and ONE artefact,
# tests/golden/forge_program/forge_page_v1.bin. This test pins the model to the
# artefact and the packer's `--check` pins the packer to it, so a layout edit
# that misses one of the three goes red. No RTL yet: the page READER is named as
# remaining work in design/contracts/FORGE.PRIM.md, and a test that waited for
# it would leave the frozen format unpinned in the meantime.
add_executable(test_forge_page_directed forge/forge_page_directed.cpp)
target_link_libraries(test_forge_page_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_forge_page_directed PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME forge_page_directed COMMAND test_forge_page_directed)
set_tests_properties(forge_page_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# The packer's own staleness gate, the same shape as `pack:check` and
# `abi:check`: rebuild the committed golden from the packer and compare byte for
# byte. It is a separate test from the one above BECAUSE they approach the same
# artefact from different sides -- a single test that built both halves from one
# source would be a comparator wired to two operands that move together.
add_test(NAME forge_page_packer_check
         COMMAND "${Python3_EXECUTABLE}" ${CMAKE_SOURCE_DIR}/tools/pack/mkforgeprogram.py --check)
set_tests_properties(forge_page_packer_check PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# ---- the ring/sheet position evaluator, FORGE.PRIM.EVAL's other four --------
# `zhao_forge_prim_eval` is the LIGHTNING evaluator and serves the RIBBON family
# alone; owner decision R234 D2 bought the rest. This block is fan, tube, shell
# and billboard -- the (rings x ring vertices) grid `zhao_forge_prim` walks,
# placed. Differenced against zref::forge_ring::eval_job entirely, including the
# determinism properties (byte-identical streams under stall patterns) that a
# capture CRC depends on.
add_executable(test_forge_ring_eval_directed forge/forge_ring_eval_directed.cpp)
target_link_libraries(test_forge_ring_eval_directed PRIVATE zhao_harness zhao_zref)
verilate(test_forge_ring_eval_directed
  PREFIX Vzhao_forge_ring_eval
  TOP_MODULE zhao_forge_ring_eval
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_ring_eval.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME forge_ring_eval_directed COMMAND test_forge_ring_eval_directed)
set_tests_properties(forge_ring_eval_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_forge_ring_eval
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_ring_eval
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_ring_eval.sv)
set_tests_properties(lint_zhao_forge_ring_eval PROPERTIES LABELS "lint;fast")

# GEOM.PARAMBUF -- the external geometry arena's record layer (ruling R7). The
# interesting field is frame_generation: a chunk from LAST frame is valid in
# every other respect -- sane count, in-range next pointer, real triangle ids --
# so the generation is the only thing that says it is old, which is why it is
# per chunk and not per arena.
add_executable(test_geom_parambuf_directed geometry/geom_parambuf_directed.cpp)
target_link_libraries(test_geom_parambuf_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_parambuf_directed
  PREFIX Vzhao_geom_parambuf
  TOP_MODULE zhao_geom_parambuf
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_parambuf.sv)
add_test(NAME geom_parambuf_directed COMMAND test_geom_parambuf_directed)
set_tests_properties(geom_parambuf_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TWOD.PLANE -- two slots on ONE engine, and most of its contract is a list of
# things it may not do. A restriction is not tested by exercising the allowed
# path: it is tested by asking for the forbidden one and requiring a refusal,
# and where the restriction is STRUCTURAL (no depth port, no fractional texel
# port) by recording that there is no port with which to ask.
add_executable(test_twod_plane_directed compositor/twod_plane_directed.cpp)
target_link_libraries(test_twod_plane_directed PRIVATE zhao_harness zhao_zref)
verilate(test_twod_plane_directed
  PREFIX Vzhao_twod_plane
  TOP_MODULE zhao_twod_plane
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_plane.sv)
add_test(NAME twod_plane_directed COMMAND test_twod_plane_directed)
set_tests_properties(twod_plane_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ...and the randomized lane. What it buys is the COORDINATE SPACE: negative
# coordinates are where `r += size` is right for one size out and wrong for
# two, and that does not appear until a plane scrolls left past its own origin
# -- which every scrolling sky does, once.
add_executable(test_twod_plane_random compositor/twod_plane_random.cpp)
target_link_libraries(test_twod_plane_random PRIVATE zhao_harness zhao_zref)
verilate(test_twod_plane_random
  PREFIX Vzhao_twod_plane
  TOP_MODULE zhao_twod_plane
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_plane.sv)
add_test(NAME twod_plane_random COMMAND test_twod_plane_random)
set_tests_properties(twod_plane_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# TWOD.SPRITE -- HUD sprites, a ruling that is mostly DELETIONS: no private
# sampler, no text rasterizer, no glyph cache. The property that is easy to get
# nearly right is the affine walk: stepping UV serpentine-style is also "just
# adds", also looks correct on a 1xN sprite, and is wrong for every sprite
# wider than one pixel. So the check is every pixel against the closed form
# with a NON-ZERO cross term.
add_executable(test_twod_sprite_directed compositor/twod_sprite_directed.cpp)
target_link_libraries(test_twod_sprite_directed PRIVATE zhao_harness zhao_zref)
verilate(test_twod_sprite_directed
  PREFIX Vzhao_twod_sprite
  TOP_MODULE zhao_twod_sprite
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_sprite.sv)
add_test(NAME twod_sprite_directed COMMAND test_twod_sprite_directed)
set_tests_properties(twod_sprite_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ...and the randomized lane. What it buys is SHAPE: the affine stepper's
# failure modes live at 1-pixel-wide and 1-pixel-tall sprites, where a
# serpentine accumulation and a row-origin off-by-one are each invisible. The
# shapes are drawn to include them on purpose, and the coverage guard requires
# that they were.
add_executable(test_twod_sprite_random compositor/twod_sprite_random.cpp)
target_link_libraries(test_twod_sprite_random PRIVATE zhao_harness zhao_zref)
verilate(test_twod_sprite_random
  PREFIX Vzhao_twod_sprite
  TOP_MODULE zhao_twod_sprite
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_sprite.sv)
add_test(NAME twod_sprite_random COMMAND test_twod_sprite_random)
set_tests_properties(twod_sprite_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# TWOD.SAMPLER -- the block between TWOD.PLANE/TWOD.SPRITE and POST.COMPOSITE.
# Header entry I17 refused this seam on the grounds that building it would mean
# "inventing a colour law". It would not: POST.COMPOSITE consumes RGB565, a
# CLUT8 palette entry IS RGB565, and the sampler performs no colour arithmetic
# at all. What was genuinely missing was a texel PAGE STORE, which is here.
#
# THIS BENCH IS NOT A DIFFERENTIAL and the registration says so, because there
# is nothing for an oracle to disagree with -- no expansion, no rounding, no
# multiply. What it checks is ADDRESSING, PAIRING, RESIDENCY and BACKPRESSURE
# against hand-computed expectations, with TWOD.PLANE and POST.COMPOSITE
# MODELLED from their own published handshakes so the bench can violate them
# on purpose.
#
# Case 9 is the positive-control block: every counter the earlier cases assert
# is zero is fired there with legal stimulus. No committed mutant is needed --
# each guard watches a CALLER's mistake rather than an internal invariant, so
# each is reachable from the ports.
add_executable(test_twod_sampler_directed compositor/twod_sampler_directed.cpp)
target_link_libraries(test_twod_sampler_directed PRIVATE zhao_harness zhao_zref)
verilate(test_twod_sampler_directed
  PREFIX Vzhao_twod_sampler
  TOP_MODULE zhao_twod_sampler
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_sampler.sv)
add_test(NAME twod_sampler_directed COMMAND test_twod_sampler_directed)
set_tests_properties(twod_sampler_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# TWOD.BAND -- the descriptor-order-to-raster-order bridge (owner rulings R233
# and R235, console entry I17). Case 4 is the one that matters: R235's refusal
# counter must DISCRIMINATE, so the bench packs the bucket EXACTLY (zero
# refusals), then adds one row (exactly one refusal), and then requires the
# refused sprite to have drawn nothing in ANY band.
add_executable(test_twod_band_directed compositor/twod_band_directed.cpp)
target_link_libraries(test_twod_band_directed PRIVATE zhao_harness)
verilate(test_twod_band_directed
  PREFIX Vzhao_twod_band
  TOP_MODULE zhao_twod_band
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_band.sv)
add_test(NAME twod_band_directed COMMAND test_twod_band_directed)
set_tests_properties(twod_band_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# TWOD.CMD -- the frame-scoped descriptor producer (owner completion ruling
# 2026-09-22 item 3). It drives the block's OWN ports, so the states the
# composed console cannot reach -- a stalled consumer, two seals in consecutive
# cycles, a sixty-fifth descriptor -- are ordinary stimulus and every counter
# fires. THE RULING'S ACCEPTANCE TEST IS `twod_cmd_chain_directed` BELOW, which
# drives real packet bytes to composited pixels; this one deliberately injects
# descriptors at a downstream boundary, because that is the only way to reach a
# refusal.
add_executable(test_twod_cmd_directed compositor/twod_cmd_directed.cpp)
target_link_libraries(test_twod_cmd_directed PRIVATE zhao_harness)
verilate(test_twod_cmd_directed
  PREFIX Vzhao_twod_cmd
  TOP_MODULE zhao_twod_cmd
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_cmd.sv)
add_test(NAME twod_cmd_directed COMMAND test_twod_cmd_directed)
set_tests_properties(twod_cmd_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# THE OWNER'S ACCEPTANCE TEST for the completion ruling's item 3, and it is a
# CHAIN rather than a block bench: zhao_cmd_decoder and zhao_cmd_exec on one
# forked byte stream, zhao_twod_cmd's sealed list, zhao_twod_asset reading an
# HPS arena, the plane, the band, the walker, the sampler and
# zhao_post_composite. The driver touches THREE things -- the packet bytes, the
# bridge and the composited pixels -- and no descriptor, texel, binding or HUD
# colour is written at a downstream boundary.
#
# "An otherwise green smoke whose upstream fixture never reaches the new path
#  does not prove the path." This is the fixture that reaches it.
add_executable(test_twod_cmd_chain_directed compositor/twod_cmd_chain_directed.cpp)
target_link_libraries(test_twod_cmd_chain_directed PRIVATE zhao_harness zhao_zref)
verilate(test_twod_cmd_chain_directed
  PREFIX Vtb_zhao_twod_chain
  TOP_MODULE tb_zhao_twod_chain
  VERILATOR_ARGS -I${CMAKE_SOURCE_DIR}/tests/compositor
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_cmd.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_asset.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_plane.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_band.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_sprite.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_sampler.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_composite.sv
          ${CMAKE_SOURCE_DIR}/tests/compositor/tb_zhao_twod_chain.sv)
# The GENERATED packers live in runtime/include/zhao_abi.h and the driver packs
# every record with them -- a record typed by hand in a bench is a second
# opinion about the ABI, and the two would disagree the first time a field
# moved.
target_include_directories(test_twod_cmd_chain_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/runtime/include)
add_test(NAME twod_cmd_chain_directed COMMAND test_twod_cmd_chain_directed)
set_tests_properties(twod_cmd_chain_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# The positive control for the LAW rather than for a counter: `BURST_PX` raised
# past the FIFO slack is "admit everything", and the driver passes when
# `band_underrun_o` FIRES and `sprites_refused_budget_o` stays ZERO. It is a
# WRAPPER, not a copy -- it instantiates production with `.*` -- so it cannot
# drift in its body, and `tools/design/wrapper_port_parity.py` checks the half
# it can get wrong.
add_executable(test_twod_band_burst_mutant mutants/twod_band_burst_mutant_driver.cpp)
target_link_libraries(test_twod_band_burst_mutant PRIVATE zhao_harness)
verilate(test_twod_band_burst_mutant
  PREFIX Vzhao_twod_band_burst_mutant
  TOP_MODULE zhao_twod_band_burst_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_twod_band_burst_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_twod_band.sv)
add_test(NAME twod_band_burst_mutant COMMAND test_twod_band_burst_mutant)
set_tests_properties(twod_band_burst_mutant PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# PART.LADDER -- the representation ladder of the 2026-08-31 ruling. Its two
# halves pull against each other: a particle MAY change representation while
# alive (so there is hysteresis) and the simulation NEVER changes because of it
# (so the block has no feedback port at all). The interesting cases are the
# OVERLAPPING bands -- 7 px is both a triangle and a sprite, and the ladder's
# order is what resolves it.
add_executable(test_part_ladder_directed particles/part_ladder_directed.cpp)
target_link_libraries(test_part_ladder_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_ladder_directed
  PREFIX Vzhao_part_ladder
  TOP_MODULE zhao_part_ladder
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_ladder.sv)
add_test(NAME part_ladder_directed COMMAND test_part_ladder_directed)
set_tests_properties(part_ladder_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ...and the randomized walk. Hysteresis is a STATE MACHINE, and a state
# machine with one input pattern has been looked at, not tested. Sixty
# particles walk across thresholds for 120 frames each, carrying prev_rung and
# hold_count forward exactly as the real record would.
add_executable(test_part_ladder_random particles/part_ladder_random.cpp)
target_link_libraries(test_part_ladder_random PRIVATE zhao_harness zhao_zref)
verilate(test_part_ladder_random
  PREFIX Vzhao_part_ladder
  TOP_MODULE zhao_part_ladder
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_ladder.sv)
add_test(NAME part_ladder_random COMMAND test_part_ladder_random)
set_tests_properties(part_ladder_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# POST.GATHER -- the effect-plane gather of ruling R5. The clamps it enforces
# are POST.COMPOSITE's bound, not its own: X to [-8,+8] and Y to [-4,+4] are
# what the nine-line ring is built against, and nothing in the compositor can
# detect a gather that exceeds them. So the bound is asserted here, at the only
# place that could break it.
add_executable(test_post_gather_directed compositor/post_gather_directed.cpp)
target_link_libraries(test_post_gather_directed PRIVATE zhao_harness zhao_zref)
verilate(test_post_gather_directed
  PREFIX Vzhao_post_gather
  TOP_MODULE zhao_post_gather
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_gather.sv)
add_test(NAME post_gather_directed COMMAND test_post_gather_directed)
set_tests_properties(post_gather_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ...and the randomized lane. What it buys over directed is ACCUMULATION over a
# whole tile in an unpredictable order, and specifically the saturation cases:
# a glow channel past 65,535 and a displacement past the s16 lane are both
# reachable in one bright tile and both fail SILENTLY -- the picture is merely
# dimmer or less displaced than it should be.
add_executable(test_post_gather_random compositor/post_gather_random.cpp)
target_link_libraries(test_post_gather_random PRIVATE zhao_harness zhao_zref)
verilate(test_post_gather_random
  PREFIX Vzhao_post_gather
  TOP_MODULE zhao_post_gather
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_gather.sv)
add_test(NAME post_gather_random COMMAND test_post_gather_random)
set_tests_properties(post_gather_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# POST.GATHER's other two thirds, built 2026-09-21 (owner ruling R195, core
# entry I17 (c)). R60: a directed test must BUILD AND RUN, not merely lint --
# `--lint-only` does not execute an `initial` block, so the elaboration guards
# in both of these blocks are checked by these targets and by nothing else.
#
# THE TAG LAW sweeps the (tag, rgb565) space against `zref::post::gather`
# rather than sampling it, because the space is 2^24 and every point is
# independent. The defect that motivates a sweep is specific: the model does
# `unit_mul(unit_mul(channel, gain), tint)` -- TWO roundings, in that order --
# and folding gain with tint first is algebraically identical, arithmetically
# different, and agrees with the model on most inputs.
add_executable(test_post_gather_tag_directed compositor/post_gather_tag_directed.cpp)
target_link_libraries(test_post_gather_tag_directed PRIVATE zhao_harness zhao_zref)
verilate(test_post_gather_tag_directed
  PREFIX Vzhao_post_gather_tag
  TOP_MODULE zhao_post_gather_tag
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_gather_tag.sv)
add_test(NAME post_gather_tag_directed COMMAND test_post_gather_tag_directed)
set_tests_properties(post_gather_tag_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# THE PLANE STORE. Its most valuable case is not an address case: it streams
# tile A, closes it, STARTS TILE B and only then flushes A's cells, requiring
# them at A's address. That is CLAUDE.md's metadata-swap shape, and a store
# with one origin register passes every other case in the file and fails that
# one.
#
# It is also where both of the composed console's POST.GATHER tripwires are
# FIRED. `tb_zhao_console_core_smoke.sv` quotes `flush_overrun_o` and
# `rdw_collide_o` at zero and $fatals otherwise; a detector reading zero is
# only evidence once it has been seen to fire, and these are the firings.
add_executable(test_post_gather_store_directed compositor/post_gather_store_directed.cpp)
target_link_libraries(test_post_gather_store_directed PRIVATE zhao_harness)
verilate(test_post_gather_store_directed
  PREFIX Vzhao_post_gather_store
  TOP_MODULE zhao_post_gather_store
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_gather_store.sv)
add_test(NAME post_gather_store_directed COMMAND test_post_gather_store_directed)
set_tests_properties(post_gather_store_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
# POST.COMPOSITE's framebuffer lease (core entries I15/I16, 2026-09-19): the
# SOURCE read-back and POST.ECHO (owner ruling R7). Both benches play MEM.GUARD's
# real two-cycle law from tests/compositor/post_mem_model.hpp -- ready a level,
# the verdict a pulse a cycle late -- because every guard client in this tree
# that was only ever tested against a same-cycle model shipped reading every
# pass as a refusal (tools/rtl/check_guard_verdict.py).
add_executable(test_post_fbread_directed compositor/post_fbread_directed.cpp)
target_link_libraries(test_post_fbread_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_post_fbread_directed PRIVATE ${CMAKE_SOURCE_DIR}/tests/compositor)
verilate(test_post_fbread_directed
  PREFIX Vzhao_post_fbread
  TOP_MODULE zhao_post_fbread
  VERILATOR_ARGS -GYW=9
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_fbread.sv)
add_test(NAME post_fbread_directed COMMAND test_post_fbread_directed)
set_tests_properties(post_fbread_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The same bench at FIFO_BEATS=8, the block's legal minimum. Q004 F3 (a phantom
# owed beat when the first beat lands on the verdict cycle) is invisible at 16 --
# the head register's one beat of slack absorbs it -- and fatal at 8, so the
# depth the defect lives at is a build of its own (post pass 2, 2026-09-19).
add_executable(test_post_fbread_directed_q8 compositor/post_fbread_directed.cpp)
target_link_libraries(test_post_fbread_directed_q8 PRIVATE zhao_harness zhao_zref)
target_include_directories(test_post_fbread_directed_q8 PRIVATE ${CMAKE_SOURCE_DIR}/tests/compositor)
target_compile_definitions(test_post_fbread_directed_q8 PRIVATE POST_FBREAD_FIFO=8)
verilate(test_post_fbread_directed_q8
  PREFIX Vzhao_post_fbread
  TOP_MODULE zhao_post_fbread
  VERILATOR_ARGS -GYW=9 -GFIFO_BEATS=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_fbread.sv)
add_test(NAME post_fbread_directed_q8 COMMAND test_post_fbread_directed_q8)
set_tests_properties(post_fbread_directed_q8 PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

add_executable(test_post_echo_directed compositor/post_echo_directed.cpp)
target_link_libraries(test_post_echo_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_post_echo_directed PRIVATE ${CMAKE_SOURCE_DIR}/tests/compositor)
verilate(test_post_echo_directed
  PREFIX Vzhao_post_echo
  TOP_MODULE zhao_post_echo
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fbwrite.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_echo.sv)
add_test(NAME post_echo_directed COMMAND test_post_echo_directed)
set_tests_properties(post_echo_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The post LEASE standalone (post pass 2, 2026-09-19): the sequencer, the ENGINE0
# share and the retire ledger with the REAL RASTER.FBWRITE on its socket, against
# an in-order memory that returns credits for reads and writes. Q004 F1/F2/F4.
add_executable(test_post_lease_directed compositor/post_lease_directed.cpp)
target_link_libraries(test_post_lease_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_post_lease_directed PRIVATE ${CMAKE_SOURCE_DIR}/tests/compositor)
verilate(test_post_lease_directed
  PREFIX Vtb_post_lease_top
  TOP_MODULE tb_post_lease_top
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fbwrite.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_fbread.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_echo.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_lease.sv
          ${CMAKE_SOURCE_DIR}/tests/compositor/tb_post_lease_top.sv)
add_test(NAME post_lease_directed COMMAND test_post_lease_directed)
set_tests_properties(post_lease_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# GEOM.VDECODE format 0 -- RAW/CANONICAL, authorised by ruling R11. Format 0
# is the DIFFERENTIAL REFERENCE: packed formats 1 and 2 are bake-off gated and
# must decode to bit-identical output, so an error here does not make one wrong
# mesh, it makes a wrong definition of correct that the later formats are then
# built to match.
add_executable(test_geom_vdecode_directed geometry/geom_vdecode_directed.cpp)
target_link_libraries(test_geom_vdecode_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_vdecode_directed
  PREFIX Vzhao_geom_vdecode
  TOP_MODULE zhao_geom_vdecode
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_vdecode.sv)
add_test(NAME geom_vdecode_directed COMMAND test_geom_vdecode_directed)
set_tests_properties(geom_vdecode_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# PART.STATE's record codec -- the particle128 layout of qformats amendment C2.
# Twelve fields at twelve offsets, written twice (pack and unpack), taken on
# faith by every other particle block. A round trip cannot catch a wrong
# offset: pack and unpack can agree with each other while both disagree with
# the spec, so the suite checks against zref AND checks field isolation.
add_executable(test_part_record_directed particles/part_record_directed.cpp)
target_link_libraries(test_part_record_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_record_directed
  PREFIX Vzhao_part_record
  TOP_MODULE zhao_part_record
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
add_test(NAME part_record_directed COMMAND test_part_record_directed)
set_tests_properties(part_record_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.MIPGEN -- the page height mips of ruling T8. The property worth the
# file is EXACTNESS: nested decimation keeps shared vertices bit-identical, and
# an averaging mip would crack every seam between two LODs. No tolerance
# appears anywhere in that test, because the law admits none.
add_executable(test_terrain_mipgen_directed terrain/terrain_mipgen_directed.cpp)
target_link_libraries(test_terrain_mipgen_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_mipgen_directed
  PREFIX Vzhao_terrain_mipgen
  TOP_MODULE zhao_terrain_mipgen
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipgen.sv)
add_test(NAME terrain_mipgen_directed COMMAND test_terrain_mipgen_directed)
set_tests_properties(terrain_mipgen_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.MIPREQ and TERRAIN.PSMUX -- the two owners that make the SECOND
# residency completion possible, composed into zhao_console_core 2026-09-19.
# Both were named as missing by `tb_terrain_world.sv` rather than discovered:
# the mip request had no owner in fpga/rtl at all, and the bench gave the mip
# pass a second read engine because sharing one "would put a scheduler in the
# bench".
#
# WHAT EACH TEST IS FOR, because neither block does anything interesting on its
# own and the failure modes are silent:
#   MIPREQ  -- a request never made, made twice, or made for the wrong page
#     produces a page that is loaded, CRC-verified and NEVER CALLED GROUND, and
#     nothing else in the machine says so. So: count, order, identity, and the
#     held-offer trap that a level-triggered wiring would fall into.
#   PSMUX   -- a beat delivered to the wrong client composes a real lattice
#     from the wrong page, with every handshake legal and every counter
#     balanced. So: ownership of every beat, and the fairness case, because
#     starving the mip pass starves the residency the compose door depends on
#     and presents as a machine that STOPS.
#
# Both suites FIRE their own zero-reading counters (`drops_o`, `stray_v_o`,
# `stray_done_o`) with legal stimulus rather than asserting the zero, so
# neither owes a committed mutant.
add_executable(test_terrain_mipreq_directed terrain/terrain_mipreq_directed.cpp)
target_link_libraries(test_terrain_mipreq_directed PRIVATE zhao_harness)
verilate(test_terrain_mipreq_directed
  PREFIX Vzhao_terrain_mipreq
  TOP_MODULE zhao_terrain_mipreq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipreq.sv)
add_test(NAME terrain_mipreq_directed COMMAND test_terrain_mipreq_directed)
set_tests_properties(terrain_mipreq_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_terrain_psmux_directed terrain/terrain_psmux_directed.cpp)
target_link_libraries(test_terrain_psmux_directed PRIVATE zhao_harness)
verilate(test_terrain_psmux_directed
  PREFIX Vzhao_terrain_psmux
  TOP_MODULE zhao_terrain_psmux
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_psmux.sv)
add_test(NAME terrain_psmux_directed COMMAND test_terrain_psmux_directed)
set_tests_properties(terrain_psmux_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# Residency v2 -- the SET-ASSOCIATIVE directory of ruling T9/T10. Beside the
# prototype, not replacing it: the prototype is what this is measured against.
# Two of these cases cannot be written against the direct map at all, because
# the thing they check is a distinction it does not have -- two islands at the
# same local coordinates, and the journal writeback barrier.
add_executable(test_terrain_residency_v2_directed terrain/terrain_residency_v2_directed.cpp)
target_link_libraries(test_terrain_residency_v2_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_residency_v2_directed
  PREFIX Vzhao_terrain_residency_v2
  TOP_MODULE zhao_terrain_residency_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency_v2.sv)
add_test(NAME terrain_residency_v2_directed COMMAND test_terrain_residency_v2_directed)
set_tests_properties(terrain_residency_v2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# Residency v2, RANDOMIZED. A multi-island traversal against a model written
# from the CONTRACT and from zref::terrain::residency_set_index -- not from the
# RTL, which would agree with it about the same mistakes. The property worth
# the file is dirty eviction: layer F has no canonical HPS mirror, so a dirty
# page displaced without being flagged means the player returns to a crater
# they made and finds it gone.
add_executable(test_terrain_residency_v2_random terrain/terrain_residency_v2_random.cpp)
target_link_libraries(test_terrain_residency_v2_random PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_residency_v2_random
  PREFIX Vzhao_terrain_residency_v2
  TOP_MODULE zhao_terrain_residency_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency_v2.sv)
add_test(NAME terrain_residency_v2_random COMMAND test_terrain_residency_v2_random)
set_tests_properties(terrain_residency_v2_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# Randomized traversal against an INDEPENDENT contract model. The property
# worth the file is dirty eviction: a dirty page displaced without being
# flagged means permanent scars silently heal, and no single-frame test
# can see it.
add_executable(test_terrain_residency_random terrain/terrain_residency_random.cpp)
target_link_libraries(test_terrain_residency_random PRIVATE zhao_harness)
verilate(test_terrain_residency_random
  PREFIX Vzhao_terrain_residency
  TOP_MODULE zhao_terrain_residency
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency.sv)
add_test(NAME terrain_residency_random COMMAND test_terrain_residency_random)
set_tests_properties(terrain_residency_random PROPERTIES LABELS "fast;nightly" TIMEOUT 300)


add_test(NAME render_fb_directed COMMAND test_render_fb_directed)
set_tests_properties(render_fb_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME render_pipe_directed COMMAND test_render_pipe_directed)
set_tests_properties(render_pipe_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_field_v3_svcpath_directed differential/field_v3_svcpath_directed.cpp)
target_link_libraries(test_field_v3_svcpath_directed PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_svcpath_directed
  PREFIX Vzhao_field_v3_svcpath
  TOP_MODULE zhao_field_v3_svcpath
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
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

# THE SAME SERVICE WITH FOUR RING UNITS. The nine products inside one unit
# are a dependency chain and cannot be pipelined, so throughput is bought only
# by having more units. Both counts are tested because the shipped default is
# still 2.
add_executable(test_field_v3_ring_svc_units4 differential/field_v3_ring_svc_directed.cpp)
target_link_libraries(test_field_v3_ring_svc_units4 PRIVATE zhao_harness zhao_zref)
verilate(test_field_v3_ring_svc_units4
  PREFIX Vzhao_field_v3_ring_svc
  TOP_MODULE zhao_field_v3_ring_svc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring_svc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_ring.sv
  VERILATOR_ARGS -GUNITS=4)
add_test(NAME field_v3_ring_svc_units4 COMMAND test_field_v3_ring_svc_units4)
set_tests_properties(field_v3_ring_svc_units4 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
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
  PREFIX Vzhao_terrain_field_walk
  TOP_MODULE zhao_terrain_field_walk
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_field_walk.sv)
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
  PREFIX Vzhao_terrain_patch_acc
  TOP_MODULE zhao_terrain_patch_acc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_acc.sv)
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

# ---------------------------------------------------------------------------
# FIELD program directory as a SCANNED MEMORY (roadmap 2026-09-09 s5): the
# candidate zhao_field_progdir (scanner + old-interface adapter) against the
# RETAINED oracle zhao_field_progcache, both elaborated in one wrapper
# (tests/field/tb_field_progdir_diff.sv) and compared transaction by
# transaction. -DTB_ENTRIES/-DTB_LRUW must match -GENTRIES/-GLRUW; the test's
# case 0 checks they do. Three elaborations, each for a law the others cannot
# reach: the shipping shape (and the timing probe the report quotes), a
# two-row / two-bit-stamp directory where the counter wraps every four stamps
# so LRU TIES are constructible, and a non-power-of-two directory under wrap.
# ---------------------------------------------------------------------------
set(ZHAO_PROGDIR_DIFF_SV
    ${CMAKE_SOURCE_DIR}/tests/field/tb_field_progdir_diff.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir_scan.sv)

add_executable(test_field_progdir_diff16 field/field_progdir_differential.cpp)
target_link_libraries(test_field_progdir_diff16 PRIVATE zhao_harness)
target_compile_definitions(test_field_progdir_diff16 PRIVATE TB_ENTRIES=16 TB_LRUW=48)
verilate(test_field_progdir_diff16
  PREFIX Vtb_field_progdir_diff
  TOP_MODULE tb_field_progdir_diff
  SOURCES ${ZHAO_PROGDIR_DIFF_SV}
  VERILATOR_ARGS -GENTRIES=16 -GLRUW=48)
add_test(NAME field_progdir_diff16_directed COMMAND test_field_progdir_diff16)
set_tests_properties(field_progdir_diff16_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progdir_diff16_random COMMAND test_field_progdir_diff16 --random 60)
set_tests_properties(field_progdir_diff16_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progdir_diff16_random_nightly COMMAND test_field_progdir_diff16 --random 800)
set_tests_properties(field_progdir_diff16_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_field_progdir_diff2 field/field_progdir_differential.cpp)
target_link_libraries(test_field_progdir_diff2 PRIVATE zhao_harness)
target_compile_definitions(test_field_progdir_diff2 PRIVATE TB_ENTRIES=2 TB_LRUW=2)
verilate(test_field_progdir_diff2
  PREFIX Vtb_field_progdir_diff
  TOP_MODULE tb_field_progdir_diff
  SOURCES ${ZHAO_PROGDIR_DIFF_SV}
  VERILATOR_ARGS -GENTRIES=2 -GLRUW=2)
add_test(NAME field_progdir_diff2_ties COMMAND test_field_progdir_diff2 --no-probe)
set_tests_properties(field_progdir_diff2_ties PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progdir_diff2_random COMMAND test_field_progdir_diff2 --random 100)
set_tests_properties(field_progdir_diff2_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_field_progdir_diff3 field/field_progdir_differential.cpp)
target_link_libraries(test_field_progdir_diff3 PRIVATE zhao_harness)
target_compile_definitions(test_field_progdir_diff3 PRIVATE TB_ENTRIES=3 TB_LRUW=6)
verilate(test_field_progdir_diff3
  PREFIX Vtb_field_progdir_diff
  TOP_MODULE tb_field_progdir_diff
  SOURCES ${ZHAO_PROGDIR_DIFF_SV}
  VERILATOR_ARGS -GENTRIES=3 -GLRUW=6)
add_test(NAME field_progdir_diff3_random COMMAND test_field_progdir_diff3 --random 100 --no-probe)
set_tests_properties(field_progdir_diff3_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THE POSITIVE CONTROL: the committed tie mutant on the candidate side,
# inverted polarity -- passes when the differential FAILS on the tie and
# AGREES on every non-tie op. Evidence about the instrument, not the design.
add_executable(test_field_progdir_mutant_control field/field_progdir_mutant_control.cpp)
target_link_libraries(test_field_progdir_mutant_control PRIVATE zhao_harness)
target_compile_definitions(test_field_progdir_mutant_control PRIVATE TB_ENTRIES=2 TB_LRUW=2)
verilate(test_field_progdir_mutant_control
  PREFIX Vtb_field_progdir_diff
  TOP_MODULE tb_field_progdir_diff
  SOURCES ${CMAKE_SOURCE_DIR}/tests/field/tb_field_progdir_diff.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_progdir_scan_mutant.sv
  VERILATOR_ARGS -GENTRIES=2 -GLRUW=2 -DPROGDIR_MUTANT)
add_test(NAME field_progdir_mutant_control COMMAND test_field_progdir_mutant_control)
set_tests_properties(field_progdir_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The REAL-PROGRAM suite, unchanged, re-pointed at the candidate through the
# one-line compat shim tests/field/progdir_compat/Vzhao_field_progcache.h: the
# same 123 checks and random lane against zref::field::ProgCache that the
# oracle passes, now on the scanned directory. Same source, two tops.
add_executable(test_field_progdir_realprog field/field_progcache_directed.cpp)
target_link_libraries(test_field_progdir_realprog PRIVATE zhao_harness zhao_zref)
target_include_directories(test_field_progdir_realprog BEFORE PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/field/progdir_compat
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
verilate(test_field_progdir_realprog
  PREFIX Vzhao_field_progdir
  TOP_MODULE zhao_field_progdir
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir_scan.sv
  VERILATOR_ARGS -GENTRIES=2)
add_test(NAME field_progdir_realprog_directed COMMAND test_field_progdir_realprog)
set_tests_properties(field_progdir_realprog_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
add_test(NAME field_progdir_realprog_random COMMAND test_field_progdir_realprog --random 60)
set_tests_properties(field_progdir_realprog_random PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

foreach(m zhao_field_progdir_scan zhao_field_progdir)
  add_test(NAME lint_${m}
           COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                   --top-module ${m}
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir_scan.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progdir.sv)
  set_tests_properties(lint_${m} PROPERTIES
    LABELS "lint;fast"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()

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

# ROWS_PER_PASS=1 against ROWS_PER_PASS=3, byte-for-byte, under four stall
# patterns -- dsp.md lever 1, the time-multiplexed matrix rows
# (reports/PROJECT-CORE-ROW-MULTIPLEX-20260909.md). The default configuration
# is pinned to the shipped oracle by geom_project_directed above and by
# terrain_project_directed; this differential pins the sequenced shape to the
# default, so the oracle is inherited by transitivity without a second
# projection implementation existing anywhere. Also asserts the declared
# fixed-latency delta (+3 en-cycles) and initiation interval (exactly 3), the
# mid-sequence cfg-write tear case the capture-at-accept law closes, and a
# POSITIVE CONTROL that skews one DUT matrix word by a raw LSB and requires
# the comparator to fire -- a checker never seen to fail has not been tested.
add_executable(test_proj_rowmux_directed geometry/proj_rowmux_directed.cpp)
verilate(test_proj_rowmux_directed
  PREFIX Vtb_proj_rowmux
  TOP_MODULE tb_proj_rowmux
  SOURCES ${CMAKE_SOURCE_DIR}/tests/geometry/tb_proj_rowmux.sv
          ${ZHAO_PROJECT_CORE_SV})
add_test(NAME proj_rowmux_directed COMMAND test_proj_rowmux_directed)
set_tests_properties(proj_rowmux_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# The shared projector's arbiter under the row sequencer's backpressure. The
# service previously had NO test at all; this smoke covers the three lines
# that changed for ROWS_PER_PASS (grants gated on the core's in_ready_o, the
# round-robin flip and contended_o gated on a grant the core can take):
# routing, order, payload integrity, exact II=3 aggregate rate, fairness, and
# contended_o counting contended GRANTS (2N-1 under dual saturation -- the
# final slot's rival has always drained) while staying zero under solo load.
add_executable(test_proj_service_rowmux_smoke geometry/proj_service_rowmux_smoke.cpp)
verilate(test_proj_service_rowmux_smoke
  PREFIX Vtb_proj_service_rowmux
  TOP_MODULE tb_proj_service_rowmux
  SOURCES ${CMAKE_SOURCE_DIR}/tests/geometry/tb_proj_service_rowmux.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_service.sv
          ${ZHAO_PROJECT_CORE_SV})
add_test(NAME proj_service_rowmux_smoke COMMAND test_proj_service_rowmux_smoke)
set_tests_properties(proj_service_rowmux_smoke PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# MATW=18 against MATW=32, byte-for-byte, over the whole legal coefficient
# domain -- owner ruling R1 (reports/OWNER-RULINGS-20260909-2300.md): the
# view-projection matrix's perspective coefficients capped at +-2.0, so the
# nine PRODUCT words narrow to signed 18-bit Q16.16 (32x18 = 2 DSP against 3,
# measured; 32x19 = 4, worse than 32). The default MATW=32 is pinned to the
# shipped oracle by geom_project_directed and the terrain suites (the terrain
# random suite sweeping matrix words over the full s32); this differential pins
# MATW=18 to MATW=32 as an output STREAM at ROWS_PER_PASS=3 AND 1, so the
# narrowed core inherits the oracle by transitivity. It also asserts THE
# REFUSAL LAW -- an out-of-range product word is refused, counted on
# `mat_refused_o`, and the register KEEPS its value (never a clamp); the
# translation column and row 2 accept anything without counting -- the fixed
# latencies, and a POSITIVE CONTROL that skews one narrowed word by a raw LSB.
add_executable(test_proj_matw_directed geometry/proj_matw_directed.cpp)
verilate(test_proj_matw_directed
  PREFIX Vtb_proj_matw
  TOP_MODULE tb_proj_matw
  SOURCES ${CMAKE_SOURCE_DIR}/tests/geometry/tb_proj_matw.sv
          ${ZHAO_PROJECT_CORE_SV})
add_test(NAME proj_matw_directed COMMAND test_proj_matw_directed)
set_tests_properties(proj_matw_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# The refusal law's COMMITTED MUTANT. tests/mutants/zhao_project_core_mutant.sv
# removes the fits-check (one line): every write is accepted and the
# multiplier reads the low 18 bits of whatever arrived, so +2.0 wraps to -2.0
# while `mat_refused_o` stays at zero. THIS TEST PASSES WHEN THE DIFFERENTIAL
# FAILS -- inverse polarity, evidence about the instruments (the counter AND
# the comparison), not about the design. On legal content the mutant is
# indistinguishable from the real core, which is why the directed test's
# refusal section exists at all.
add_executable(test_proj_matw_mutant_control geometry/proj_matw_mutant_control.cpp)
verilate(test_proj_matw_mutant_control
  PREFIX Vtb_proj_matw_mutant
  TOP_MODULE tb_proj_matw_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/geometry/tb_proj_matw_mutant.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_project_core_mutant.sv
          ${ZHAO_PROJECT_CORE_SV})
add_test(NAME proj_matw_mutant_control COMMAND test_proj_matw_mutant_control)
set_tests_properties(proj_matw_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

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

# ---------------------------------------------------------------------------
# HOST.REGWIN -- the HPS lightweight-bridge CSR aperture (owner ruling R51),
# and the carrier that closed zhao_console_core header entries I19 and I45.
#
# The APERTURE ALONE, against tenant MODELS. The composed smoke proves it
# answers two real tenants with real values; only this target can prove the
# three things a carrier has to get right and a live console hides: that a
# tenant is never offered another tenant's address (measured on the wire, not
# inferred from a plausible answer), that `refused_timeout_o` can move at all
# (both real tenants always acknowledge, so no console stimulus reaches it --
# but `t_ack_i` is an INPUT here, so holding it low is legal and no mutant is
# owed), and that reads/writes are counted as ASKED rather than as succeeded.
add_executable(test_host_regwin_directed debug/host_regwin_directed.cpp)
target_link_libraries(test_host_regwin_directed PRIVATE zhao_harness zhao_zref)
verilate(test_host_regwin_directed
  PREFIX Vzhao_host_regwin
  TOP_MODULE zhao_host_regwin
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_host_regwin.sv)
add_test(NAME host_regwin_directed COMMAND test_host_regwin_directed)
set_tests_properties(host_regwin_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_host_regwin
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_host_regwin
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_host_regwin.sv)
set_tests_properties(lint_zhao_host_regwin PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_zhao_host_reg_hist
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_host_reg_hist
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_host_reg_hist.sv)
set_tests_properties(lint_zhao_host_reg_hist PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_zhao_host_reg_trace
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_host_reg_trace
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/debug/zhao_host_reg_trace.sv)
set_tests_properties(lint_zhao_host_reg_trace PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

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

# CMD.EXEC: the executor, driven through the REAL pair. The DUT top is
# tests/command/tb_cmd_exec_pair.sv -- CMD.DECODER and CMD.EXEC on ONE forked
# byte stream -- so the verdict the executor gates on, and the cycle it arrives
# on, are the shipped decoder's and not the test's opinion of them. Driving the
# executor alone with a C++ verdict would pass while proving nothing about the
# ready fork or the one-cycle staging race.
add_executable(test_cmd_exec_directed command/cmd_exec_directed.cpp)
target_link_libraries(test_cmd_exec_directed PRIVATE zhao_harness zhao_zref)
# white-box: zrender/internal.hpp. Cases 32-35 differential the viewport
# lowering against `zref::render::viewports_of()` -- the ORACLE video_rules
# 3.2 names -- rather than against a retyped copy of the table.
target_include_directories(test_cmd_exec_directed PRIVATE ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_cmd_exec_directed
  PREFIX Vtb_cmd_exec_pair
  TOP_MODULE tb_cmd_exec_pair
  INCLUDE_DIRS ${ZHAO_CMD_INCLUDES}
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_exec.sv
          ${CMAKE_SOURCE_DIR}/tests/command/tb_cmd_exec_pair.sv)
add_test(NAME cmd_exec_directed COMMAND test_cmd_exec_directed)
set_tests_properties(cmd_exec_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_cmd_exec
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_cmd_exec
                 ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_exec.sv)
set_tests_properties(lint_zhao_cmd_exec PROPERTIES
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

# formal: slot FSM + DMA CRC gate (plan 4) â€” SKIP only when sby (or the
# slang plugin) is absent; anything else is FATAL. Both properties use the
# read_slang frontend against the REAL packages (the W2.3 lane choice; no
# ABI stub, no mechanical copies â€” see the .sby headers). Both carry TWO
# tasks (bmc AND cover): every assertion in zhao_cmd_dma is an implication
# on pkt_valid_o / guard_req_o.valid, and the scheduler's fence property is
# guarded by a RUN->DONE transition â€” without the cover task each bmc could
# pass with those preconditions unreachable (the MEM.GUARD vacuity shape).
# Reuses the hardened mem_formal_lane wrapper (narrow SKIP, --prefix for
# multi-task) rather than a parallel cmd-specific template.
if(ZHAO_SBY_BIN)
  # geom_meshfetch_refuse added 2026-09-04: the refusal law ("emits no
  # meshlet") proved rather than sampled. It is cheap -- bmc and cover both
  # return in under two seconds at depth 24 -- and it earned its place by
  # finding a real defect: the block recomputed its refusal from the LIVE
  # `crc_ok_i` instead of latching the verdict, so the input could drop after
  # the decision and a refused descriptor's bound reached the cull service.
  # BMC found it at step 15 of a properly reset design.
  foreach(sby_prop IN ITEMS cmd_scheduler_slot_fsm cmd_dma_crc_gate debug_frameblit_safety
                           video_slotmgr_invariants field_seq_bound sat_add
                           cdc_snapshot geom_meshfetch_refuse)
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
                       # solo ï¿½ wall time must approximate solo time (2026-08-16)
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
# block (merge rule, plan Â§3): everything W2.2 adds lands here, at the end.
# Shared driver: tests/video/video_harness.hpp drives the Verilated
# zhao_video_tb (whole subsystem, tests/video/zhao_video_tb.sv) and the
# zref::Scanout mirror on one gpu-cycle timeline (vid posedges on odd
# steps â€” the frozen 2:1 phase, plan R1).

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
# absent, --prefix for the multi-task bmc+cover pairs V16 requires) â€” the
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
                       # solo ï¿½ wall time must approximate solo time (2026-08-16)
      TIMEOUT 2100
      SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
  endforeach()
endif()

# ---- Phase 4: RASTER.EDGEWALK (charter phase 4, ZH-022/ZH-023; law -------
# ---- spec/qformats.md Â§8; contract design/contracts/RASTER.EDGEWALK.md) ----
# The oracle is zref::EdgeWalk, which is itself a view onto the FROZEN
# software raster (reference/src/zrender/rast.cpp), so "RTL == oracle" is
# literally "RTL == the Â§8 fill law" â€” no re-derived fill rule anywhere in
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

# What ruling 4 would actually buy: the setup cost, measured, priced against the
# four scenes count_bin_load reports. Evidence for a decision, not the decision.
add_executable(test_raster_edgewalk_setupcost raster/raster_edgewalk_setupcost_directed.cpp)
target_link_libraries(test_raster_edgewalk_setupcost PRIVATE zhao_harness zhao_zref)
verilate(test_raster_edgewalk_setupcost
  PREFIX Vzhao_raster_edgewalk
  TOP_MODULE zhao_raster_edgewalk
  SOURCES ${ZHAO_RASTER_EDGEWALK_SV})
add_test(NAME raster_edgewalk_setupcost COMMAND test_raster_edgewalk_setupcost)
set_tests_properties(raster_edgewalk_setupcost PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# THE SEAM. Five of the attribute blocks in one executable, driven in sequence
# over one textured triangle. Each of them already passes alone with inputs it
# supplied itself; this is the first thing that makes them agree with EACH OTHER
# about what the formats mean.
add_executable(test_render_textured_seam render/render_textured_seam.cpp)
target_link_libraries(test_render_textured_seam PRIVATE zhao_harness zhao_zref)
verilate(test_render_textured_seam
  PREFIX Vzhao_geom_attrsetup
  TOP_MODULE zhao_geom_attrsetup
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrsetup.sv)
verilate(test_render_textured_seam
  PREFIX Vzhao_raster_edgewalk
  TOP_MODULE zhao_raster_edgewalk
  SOURCES ${ZHAO_RASTER_EDGEWALK_SV})
verilate(test_render_textured_seam
  PREFIX Vzhao_raster_attrinterp
  TOP_MODULE zhao_raster_attrinterp
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrinterp.sv)
verilate(test_render_textured_seam
  PREFIX Vzhao_raster_attrdiv
  TOP_MODULE zhao_raster_attrdiv
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv.sv)
verilate(test_render_textured_seam
  PREFIX Vzhao_raster_perspuv
  TOP_MODULE zhao_raster_perspuv
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv)
add_test(NAME render_textured_seam COMMAND test_render_textured_seam)
set_tests_properties(render_textured_seam PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

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

# per-module lint (charter Â§21 step 8)
add_test(NAME lint_raster_edgewalk
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_raster_edgewalk
                 ${ZHAO_RASTER_EDGEWALK_SV})
set_tests_properties(lint_raster_edgewalk PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the Â§8 top-left fill rule (charter Â§20.4 â€” "adjacent-triangle
# exactly-once coverage is a SymbiYosys formal property"). Proved on
# zhao_raster_fill, the module the tile walker instantiates 48Ã— per row, so
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
# (two arrays and a present bit) â€” what the differential tests exercise is the
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
# literally "RTL == the charter 8 ordered dither" â€” no re-derived dither
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

# formal: the RGB565 quantizer, proved on zhao_raster_quant â€” the module
# zhao_raster_resolve instantiates 3x per pixel â€” at BOTH shipping parameter
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
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
# The v2 sampler against the SERIAL block's own suite. Same ports, same checks,
# same untouched zref::Tmu oracle -- so a difference is a defect in the new
# implementation and nothing else.
add_executable(test_texture_tmu_pipe texture/texture_tmu_directed.cpp)
target_compile_definitions(test_texture_tmu_pipe PRIVATE ZHAO_TMU_PIPE=1 ZHAO_FILT_LANES=2)
target_link_libraries(test_texture_tmu_pipe PRIVATE zhao_harness zhao_zref)
verilate(test_texture_tmu_pipe
  PREFIX Vzhao_texture_tmu_pipe
  TOP_MODULE zhao_texture_tmu_pipe
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_pipe.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp.sv)
# Green as of 2026-08-30: the v2 sampler passes all 79 of the serial block's
# checks against the same untouched zref::Tmu, so it joins the shared gates.
add_test(NAME texture_tmu_pipe COMMAND test_texture_tmu_pipe)
set_tests_properties(texture_tmu_pipe PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

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

# THE FILT_LANES FRONTIER â€” 4 and 1 beside the default 2.
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

# ---- Phase 5: the GEOMETRY FRONT â€” GEOM.CLIP / GEOM.SETUP / GEOM.BINNER ----
# ---- (charter phase 5 build order 14/15 "screen-space triangle setup in
# ---- FPGA" + "tile binner in FPGA", and item 3 "chunked tile lists and safe
# ---- overflow"; ZH-056 / ZH-057 / ZH-058 / ZH-026). Law spec/qformats.md 8;
# ---- contracts design/contracts/GEOM.CLIP.md / GEOM.SETUP.md / GEOM.BINNER.md
# The oracles are zref::Clip, zref::Setup and zref::Binner. None of them
# restates a law rast.cpp already owns: the 2A cross product is
# zref::EdgeWalk::area2 (which IS rast.cpp's orient()), and the scissored
# pixel-centre scan box is zref::render::scan_bbox â€” the very function
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
# zhao_geom_proj_lane computes nothing, which is exactly why it is tested: a
# module whose whole content is a field order and an address fails by
# transposing a field or landing one slot over, and neither looks like a crash.
# The third case is the one that earns the file -- a lane that ignored the
# rider entirely would pass the round-trip checks and still put every vertex in
# one place.
# PART.STATE is the first of the four absent particle-core contracts to be
# built. Small parameters on purpose: CAPACITY=8 and SPECIES_N=4 make capacity
# exhaustion and an out-of-range species REACHABLE, which at the required tier
# of 32,768 they are not -- and a guard that cannot be reached is not evidence
# about the guard.
add_executable(test_part_state_directed particles/part_state_directed.cpp)
# zhao_zref for zref/zref_particle.hpp: the stimulus is built by the RATIFIED
# particle128 codec (amendment C2 / ruling R3), not by hand-written shifts. The
# hand-written version masked `age` to seven bits where the oracle says ten.
target_link_libraries(test_part_state_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_state_directed
  PREFIX Vzhao_part_state
  TOP_MODULE zhao_part_state
  VERILATOR_ARGS -GCAPACITY=8 -GSPECIES_N=4 -GCHILD_D=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_state.sv)
add_test(NAME part_state_directed COMMAND test_part_state_directed)
set_tests_properties(part_state_directed PROPERTIES LABELS "fast;particles")

# PART.STATE's HPS DDR face (entry I1, closed 2026-09-19 on gz/pfs): the REAL
# streamer + PART.STATE + N-client arbiter + bridge, with the C++ playing
# UPDATE, SPAWN and the HPS. A record crosses DDR twice and is compared bit for
# bit each time; an empty generation finishes its tick; every counter fires.
add_executable(test_part_hps_directed particles/part_hps_directed.cpp)
target_link_libraries(test_part_hps_directed PRIVATE zhao_harness)
verilate(test_part_hps_directed
  PREFIX Vtb_part_hps_chain
  TOP_MODULE tb_part_hps_chain
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_bridge.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_state.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_hps.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/particles/tb_part_hps_chain.sv)
add_test(NAME part_hps_directed COMMAND test_part_hps_directed)
set_tests_properties(part_hps_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_hps
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_hps
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_hps.sv)
set_tests_properties(lint_zhao_part_hps PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# ...and the POSITIVE CONTROL for its ordering check.
#
# zhao_part_state.sv carries NO assertion for "survivors compacted first,
# children strictly after": it says so in its own ZHAO_ASSERT block, because
# the property is structural and the first draft's assertion ended in `&& 1'b0`.
# The whole defence of the law is part_state_directed.cpp comparing the emitted
# stream position by position -- and that suite passed on its first run, which
# is a claim, not evidence.
#
# tests/mutants/zhao_part_state_child_order_mutant.sv is a committed copy whose
# only substantive difference is that children may enter the write stream DURING
# the survivor pass. Every count still balances on it -- 7 records, 4 survivors,
# 3 children -- so a counter-based test passes on a block with the ordering
# destroyed. Only the per-position comparison sees it.
#
# THIS TEST PASSES WHEN THE ORDERING IS SEEN TO BE WRONG. Inverse polarity,
# deliberately: it is evidence about the instrument, not about the design.
add_executable(test_part_state_child_order_control
  mutants/part_state_child_order_control.cpp)
target_link_libraries(test_part_state_child_order_control PRIVATE zhao_harness zhao_zref)
verilate(test_part_state_child_order_control
  PREFIX Vzhao_part_state_child_order_mutant
  TOP_MODULE zhao_part_state_child_order_mutant
  VERILATOR_ARGS -GCAPACITY=8 -GSPECIES_N=4 -GCHILD_D=8
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_part_state_child_order_mutant.sv)
add_test(NAME part_state_child_order_control
         COMMAND test_part_state_child_order_control)
set_tests_properties(part_state_child_order_control PROPERTIES
  LABELS "fast;particles" TIMEOUT 300)

# PART.UPDATE -- the second of the four absent particle-core contracts.
#
# SPECIES_N=8 for the same reason PART.STATE uses 4: at the 7-bit field's full
# 128 an out-of-range species is UNREACHABLE, and a guard that cannot be reached
# is not evidence about the guard.
#
# zhao_part_record is in SOURCES because the block INSTANTIATES it rather than
# restating twelve bit offsets -- the layout exists in one place and PART.UPDATE
# cannot drift from it.
#
# Note what this suite is NOT: differential. blocks.yml names
# `reference_model: zref::ParticleUpdate` and that symbol does not exist in the
# tree. Every expected number is hand-computed from the RTL header's own
# formulae. tests/particles/part_update_random.cpp, which blocks.yml also names,
# is deliberately NOT built: a randomized differential needs the reference that
# does not exist, and a randomized self-comparison asserts the RTL equals itself.
add_executable(test_part_update_directed particles/part_update_directed.cpp)
target_link_libraries(test_part_update_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_update_directed
  PREFIX Vzhao_part_update
  TOP_MODULE zhao_part_update
  VERILATOR_ARGS -GSPECIES_N=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_update.sv)
add_test(NAME part_update_directed COMMAND test_part_update_directed)
set_tests_properties(part_update_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_update
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_update
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_update.sv)
set_tests_properties(lint_zhao_part_update PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_geom_proj_lane_directed geometry/geom_proj_lane_directed.cpp)
target_link_libraries(test_geom_proj_lane_directed PRIVATE zhao_harness)
verilate(test_geom_proj_lane_directed
  PREFIX Vzhao_geom_proj_lane
  TOP_MODULE zhao_geom_proj_lane
  # DEPTH=8 for the same reason geom_wcache_directed uses 16: the arena's
  # walk-clear cost is not what this suite tests, and a DENSE seal means every
  # slot must be filled, so a small depth is what makes "fill them all" cheap.
  VERILATOR_ARGS -GARENAS=2 -GDEPTH=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_wcache.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_proj_lane.sv)
add_test(NAME geom_proj_lane_directed COMMAND test_geom_proj_lane_directed)
set_tests_properties(geom_proj_lane_directed PROPERTIES LABELS "fast;geometry")
add_executable(test_geom_wcache_directed geometry/geom_wcache_directed.cpp)
target_link_libraries(test_geom_wcache_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_wcache_directed
  PREFIX Vzhao_vertex_arena
  TOP_MODULE zhao_vertex_arena
  VERILATOR_ARGS -GARENAS=2 -GDEPTH=16 -GPAYLOAD_W=64 -GGEN_W=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
add_test(NAME geom_wcache_directed COMMAND test_geom_wcache_directed)
set_tests_properties(geom_wcache_directed PROPERTIES LABELS "fast;nightly")

# The randomized half. design/blocks.yml has declared this path since the block
# was registered and it had never been written -- the ledger's V6 check tolerates
# that at SPECIFIED and refuses it from REFERENCE_COMPLETE up, so advancing the
# maturity is what surfaced it.
add_executable(test_geom_wcache_random geometry/geom_wcache_random.cpp)
target_link_libraries(test_geom_wcache_random PRIVATE zhao_harness zhao_zref)
verilate(test_geom_wcache_random
  PREFIX Vzhao_vertex_arena
  TOP_MODULE zhao_vertex_arena
  VERILATOR_ARGS -GARENAS=2 -GDEPTH=16 -GPAYLOAD_W=64 -GGEN_W=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
add_test(NAME geom_wcache_random COMMAND test_geom_wcache_random)
set_tests_properties(geom_wcache_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# VALID_MODE=1 (DENSE_SEAL), the 2026-09-09 consolidation of the
# zhao_proj_arena3 design study into the sanctioned primitive. Differential
# against the same oracle in dense mode, at TERRAIN'S shape -- 4x81, and 81 is
# deliberately not a power of two, so this suite also holds the linear
# addressing repair at a depth the old concat addressing corrupted.
add_executable(test_vertex_arena_dense_directed geometry/vertex_arena_dense_directed.cpp)
target_link_libraries(test_vertex_arena_dense_directed PRIVATE zhao_harness zhao_zref)
verilate(test_vertex_arena_dense_directed
  PREFIX Vzhao_vertex_arena
  TOP_MODULE zhao_vertex_arena
  VERILATOR_ARGS -GARENAS=4 -GDEPTH=81 -GPAYLOAD_W=64 -GGEN_W=8 -GVALID_MODE=1
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
add_test(NAME vertex_arena_dense_directed COMMAND test_vertex_arena_dense_directed)
set_tests_properties(vertex_arena_dense_directed PROPERTIES LABELS "fast;nightly")

# THE POSITIVE CONTROL FOR arena_misses_o IN DENSE MODE, driving a committed
# MUTANT. In VALID_MODE=1 a legal lookup structurally cannot miss (proved:
# a_dense_no_miss, prove_dense task), so the miss counter is unreachable by
# any stimulus while the seal guard is right -- a hopeful zero. The mutant
# breaks the guard (seal_gate_c `==` -> `<=`, accepting a short seal) in a
# separate renamed file, never a temporary edit to production RTL.
#
# THIS TEST PASSES WHEN THE COUNTER FIRES. Inverse polarity, deliberately: it
# is evidence about the instrument, not about the design. (The directed suite
# above, compiled unchanged against the same mutant, FAILS 149 of 394 checks
# -- the checker's own positive control; run standalone, not registered,
# because a committed test must pass.)
add_executable(test_vertex_arena_dense_seal_control
               geometry/vertex_arena_dense_seal_control.cpp)
target_link_libraries(test_vertex_arena_dense_seal_control PRIVATE zhao_harness)
verilate(test_vertex_arena_dense_seal_control
  PREFIX Vzhao_vertex_arena_dense_mutant
  TOP_MODULE zhao_vertex_arena_dense_mutant
  VERILATOR_ARGS -GARENAS=4 -GDEPTH=81 -GPAYLOAD_W=64 -GGEN_W=8 -GVALID_MODE=1
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_vertex_arena_dense_mutant.sv)
add_test(NAME vertex_arena_dense_seal_control
         COMMAND test_vertex_arena_dense_seal_control)
set_tests_properties(vertex_arena_dense_seal_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# VALID_BITMAP's UPDATE LAW, at a shape where getting it wrong is visible.
# Added 2026-09-18 with the bitmap-process rewrite (DEPTH runtime-variable bit
# selects -> ARENAS*DEPTH compile-time-constant per-bit flops, after the census
# attributed 92.2% of the design's combinational ALUTs to this module).
#
# The suites above could not see the property that rewrite is most likely to
# invert. Neither geom_wcache_directed nor geom_wcache_random ever drives
# `open_i` and `fill_valid_i` on the SAME EDGE, so "an accepted fill wins its
# own bit against an open of its own arena" -- last-assignment-wins in the old
# process, set-before-clear in the new -- was never asked. A rewrite that tests
# the clear first loses that fill silently and passes every existing test.
#
# ARENAS=3 and DEPTH=11 are both deliberately NOT powers of two. At a power of
# two a padded partition and the linear one coincide bit-for-bit, which is
# exactly how the addressing defect repaired on 2026-09-09 survived a directed
# suite at DEPTH=16 and a formal shape at DEPTH=4; and ARENAS=3 makes arena 3
# expressible, so the bad-arena guards can be FIRED rather than assumed.
#
# It asserts the CORRECT behaviour, never the defect, so it passes on both the
# old and the new process -- which is what makes it equivalence evidence rather
# than a change log.
add_executable(test_vertex_arena_bitmap_open_fill_race
               geometry/vertex_arena_bitmap_open_fill_race.cpp)
target_link_libraries(test_vertex_arena_bitmap_open_fill_race PRIVATE zhao_harness)
verilate(test_vertex_arena_bitmap_open_fill_race
  PREFIX Vzhao_vertex_arena
  TOP_MODULE zhao_vertex_arena
  VERILATOR_ARGS -GARENAS=3 -GDEPTH=11 -GPAYLOAD_W=64 -GGEN_W=8 -GVALID_MODE=0
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
add_test(NAME vertex_arena_bitmap_open_fill_race
         COMMAND test_vertex_arena_bitmap_open_fill_race)
set_tests_properties(vertex_arena_bitmap_open_fill_race PROPERTIES
  LABELS "fast;geometry" TIMEOUT 300)

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

# TERRAIN.SHADE (phase 6, ZH-081)
#
# The oracle is the COMPILED ratified law itself â€” zhao_zref's
# `shade_flat_tri_dir_unclamped` on the vertices (tier 1) and the law's own
# pieces (shade_nmag2/shade_ndot/isqrt_u64 + the compiled div_rhu_s128) on
# raw normals (tier 2). Sun and output are Q16.16, NOT s1.15 â€” the format
# error the oracle's own history records catching. The suite's positive
# control is `--break-oracle` (must FAIL); every DUT counter is asserted
# with an exact count, so no counter can be a hopeful zero.
# ---------------------------------------------------------------------------
# PACKET H PREREQUISITE -- the lease path, composed and driven.
#
# zhao_shell_top_v2.sv does not exist, and the obstacle is not its size: the
# V2 manager keeps EIGHT of zhao_video_slotmgr's twenty-two ports and the
# lease flow has to be re-plumbed through three new organs. So this composes
# the three blocks that carry a renderer frame from request to published-ready
# and drives one legal transaction, per
# reports/PACKET-H-CHANNEL-MAP-20260917.md.
#
# It is a PREREQUISITE and not the packet: the frame-clear handshake ordering,
# the reset-barrier entry for five structural faults and the sequence-abort
# RELEASE are behaviour with their own gate clauses and none is exercised here.
# What it establishes is that the channels carry one clean frame, which is what
# makes composing them into a 2,000-line top safe rather than speculative.
# THE BIN PIPE CLOSURE COMES FROM PACKET D'S OWN MANIFEST, not retyped here.
# zhao_geom_bin_pipe_v2 instantiates zhao_geom_binner_v2 and
# zhao_raster_tile_pipe_v2, which drag in the whole V3 texture island -- 54
# files. A second copy of that list would go stale silently, which is this
# repository's most-repeated lesson. Read from the same file Packet D reads.
#
# THREE ENTRIES ARE DROPPED ON PURPOSE, and one of them is a trap worth
# naming. `tb_geom_bin_pipe_v2.sv` is Packet D's own top. The two mutant shims
# are selector files -- and `zhao_raster_tile_pipe_v2_mutants.sv` **defines**
# ZHAO_PACKET_D_TEST_HOOKS, which appends three ports to
# zhao_geom_bin_pipe_v2. So a mutant shim sitting in a shared source list
# silently changes a production module's INTERFACE: with it in, this harness
# fails to elaborate with PINMISSING on ports no shipped build has; with it
# out, naming those ports is PINNOTFOUND. The difference between 165 and 168
# ports is entirely that one line.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/geometry/geom_bin_pipe_v2.sources.txt"
  ZHAO_LEASE_PATH_PD_RELATIVE)
set(ZHAO_LEASE_PATH_PD_SOURCES)
foreach(relative_source IN LISTS ZHAO_LEASE_PATH_PD_RELATIVE)
  if(NOT relative_source MATCHES "^tests/")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
      message(FATAL_ERROR "lease-path closure: missing ${relative_source}")
    endif()
    list(APPEND ZHAO_LEASE_PATH_PD_SOURCES
      "${CMAKE_SOURCE_DIR}/${relative_source}")
  endif()
endforeach()
list(LENGTH ZHAO_LEASE_PATH_PD_SOURCES ZHAO_LEASE_PATH_PD_COUNT)
if(ZHAO_LEASE_PATH_PD_COUNT LESS 50)
  message(FATAL_ERROR
    "lease-path closure has only ${ZHAO_LEASE_PATH_PD_COUNT} files; expected 50+")
endif()

set(ZHAO_SHELL_V2_LEASE_PATH_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/shell/v3_closure_inherited.vlt
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
  ${ZHAO_LEASE_PATH_PD_SOURCES}
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_slotmgr_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_renderer_lease_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_terminal_adapter_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_blit_lease_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_ready_cdc_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_ready_bridge_v2.sv
  ${CMAKE_CURRENT_SOURCE_DIR}/shell/zhao_shell_v2_lease_path.sv)

add_executable(test_shell_v2_lease_path_directed
  shell/shell_v2_lease_path_directed.cpp)
target_link_libraries(test_shell_v2_lease_path_directed PRIVATE zhao_harness)
verilate(test_shell_v2_lease_path_directed
  PREFIX Vzhao_shell_v2_lease_path
  TOP_MODULE zhao_shell_v2_lease_path
  SOURCES ${ZHAO_SHELL_V2_LEASE_PATH_SOURCES}
  VERILATOR_ARGS --assert)
add_test(NAME shell_v2_lease_path_directed
  COMMAND test_shell_v2_lease_path_directed)
set_tests_properties(shell_v2_lease_path_directed PROPERTIES
  LABELS "fast;nightly;packet-h" TIMEOUT 300
  FAIL_REGULAR_EXPRESSION "FAIL")

# THE GATE CLAUSE -- "every new port is connected" -- checked mechanically.
#
# A port map entry reading (1'b0) and one reading (some_net) look equally
# deliberate afterwards, and the first is how an input nobody decided about
# ships AS a decision. CLAUDE.md puts it exactly: a tie-off that looks
# deliberate is worse than a missing connection, because a missing one is a
# PINMISSING and a tie-off is silence.
#
# The audit demands a `// TIE: <why>` beside every literal in the composition.
# It found 36 silent ones the first time it ran.
# PACKET-H GATE CLAUSE: the nested V3 elaborates with NO migration shadows.
#
# The clause is "MIGRATION_SHADOWS=1'b0 with shadow_present=0 and no shadow
# comparator/counters". The island DEFAULTS shadows ON, so the property is
# not "one site passes 1'b0" -- it is "no site anywhere asks for them", and
# a check written against the one site everybody knows about would pass
# forever after somebody adds a second.
#
# The witness is itself tested: tests/tools/test_packet_h_shadow_witness.py
# fires it at a synthetic tree that turns shadows on, and at one with the
# $fatal guard removed, and requires it to fail both -- plus a control that
# an extra LEGAL site does not fire it, so the first test cannot pass merely
# because a third site exists.
add_test(NAME packet_h_shadow_witness
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/packet_h_shadow_witness.py)
set_tests_properties(packet_h_shadow_witness PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

add_test(NAME packet_h_shadow_witness_controls
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tests/tools/test_packet_h_shadow_witness.py)
set_tests_properties(packet_h_shadow_witness_controls PROPERTIES
  LABELS "fast;mutant;packet-h" TIMEOUT 120)

# PACKET-H GATE CLAUSE: unaffected behaviour matches the historical shell.
#
# The full clause is a paired-traffic differential and is still owed. This is
# the structural half, and it catches what paired traffic would catch late:
# the sibling was SEEDED from the protected V1 so the instances Packet H does
# not touch carry across byte-for-byte, and nothing was checking that they
# still do. A hand-maintained copy of a protected file drifts silently on both
# sides -- this repository has a whole chapter about that.
#
# Every differing region is classified COMMENT-ONLY (Packet H added `// TIE:`
# reasons) or SUBSTANTIVE, and a substantive one must be attributable to a
# DECLARED marker nearby. The twenty instances the seed carried over are
# SEALED against that attribution window, because a control found the window
# reaching seven lines into a neighbouring instance and excusing drift there.
#
# Itself tested: tests/tools/test_packet_h_sibling_diff.py fires it on drift
# in a carried-over instance, on a moved seed and on a declared marker that
# matches nothing -- plus the control that matters, that a COMMENT added to a
# carried-over instance does not fire it, so the drift test cannot pass merely
# because something changed.
add_test(NAME packet_h_sibling_diff
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/packet_h_sibling_diff.py)
set_tests_properties(packet_h_sibling_diff PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

add_test(NAME packet_h_sibling_diff_controls
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tests/tools/test_packet_h_sibling_diff.py)
set_tests_properties(packet_h_sibling_diff_controls PROPERTIES
  LABELS "fast;mutant;packet-h" TIMEOUT 120)

# The harness's `bin_fault_w` is a hand-written COPY of the shell's
# `v2_fault_level_c`. If one grows a fault term and the other does not, the
# directed test composes a machine whose fault attribution is weaker than the
# one that ships -- and it stays GREEN, because no check reads the list.
#
# Its controls include the one that matters: two empty sets compare equal, so
# an extractor that stops matching would report perfect agreement. It must
# refuse instead.
add_test(NAME packet_h_fault_or_parity
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/packet_h_fault_or_parity.py)
set_tests_properties(packet_h_fault_or_parity PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

add_test(NAME packet_h_fault_or_parity_controls
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tests/tools/test_packet_h_fault_or_parity.py)
set_tests_properties(packet_h_fault_or_parity_controls PROPERTIES
  LABELS "fast;mutant;packet-h" TIMEOUT 120)

add_test(NAME packet_h_tieoff_audit
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/packet_h_tieoff_audit.py)
set_tests_properties(packet_h_tieoff_audit PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

# THE SAME AUDIT, POINTED AT THE CONSOLE CORE -- and this one is the
# enforcement owner ruling R159 asked for.
#
# R159: `completion_register.py` counts DECLARED tie-offs, because it parses a
# hand-maintained comment block. A tie-off nobody writes down is invisible, so
# **composing a block with undeclared tie-offs makes the gap count go DOWN** --
# the flattering direction, by construction, and the one move the completion
# campaign forbids. R159's own conclusion was that the fix is not a smarter
# register (a tool inferring intent would be guessing) but a RULE plus a
# reviewer check. This is the reviewer check, and it is a gate rather than
# prose because `CLAUDE.md` has already learned that "advisory prose loses to
# the pull of reporting a status".
#
# The core passes at 0 SILENT as of 2026-09-20. A composition that adds a
# literal to a port map without a reason turns this red, which is the whole
# point: the author states why, or connects it.
#
# READ OWNER RULING R166 BEFORE BELIEVING A RED HERE. Pointed at this file for
# the first time the audit reported 19 silent connections and ALL NINETEEN were
# its own blindness -- it saw 18 of 88 instantiations, knew the marker `TIE:`
# but not this file's `REAL:`, and did not understand a reason written above a
# RUN of connections. It is calibrated now, and the habit it bought is worth
# more than the gate: read three rows by hand before believing the total.
add_test(NAME console_core_tieoff_audit
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/packet_h_tieoff_audit.py
          ${CMAKE_SOURCE_DIR}/fpga/rtl/prod/zhao_console_core.sv)
set_tests_properties(console_core_tieoff_audit PROPERTIES
  LABELS "fast;lint" TIMEOUT 120)

# THE HALF mutant_copy_drift IS BLIND TO, BY DESIGN.
#
# Owner ruling R162: "a wrapper cannot drift in its BODY, but its PORT LIST
# can, and mutant_copy_drift is blind to that half by design." On 2026-09-21
# two lanes landing in the same window broke both wrappers in OPPOSITE
# directions at once -- seventeen ports added to the core, fourteen removed --
# and every static gate stayed green while the control would not elaborate
# (owner ruling R220). It was found by RUNNING the smoke form, which is the
# expensive way.
#
# Proven in both directions before registration: RC 1 on a planted missing
# port AND on a planted stale one, RC 0 restored.
add_test(NAME wrapper_port_parity
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/wrapper_port_parity.py)
set_tests_properties(wrapper_port_parity PROPERTIES
  LABELS "fast;lint;mutant" TIMEOUT 120)

# EVERY `FINDINGS-<lane>.md` CITED IN THE TREE MUST EXIST. The harness refuses
# to let a packet write its own findings file, so transcribing is the
# coordinator's job -- and a job that depends on somebody remembering is a job
# that silently does not get done.
#
# Found 2026-09-21 by packet GOURAUDLOOK, which noticed that
# `zhao_geom_attrpack.sv` and `zhao_raster_tile_pipe_v2.sv` BOTH cited
# `FINDINGS-attrlane.md` for the cost working behind a +24 DSP decision, and
# the file was in no commit. Sweeping the general case found EIGHT more, none
# present in any of the 88 run folders, four of them in PRODUCTION RTL.
#
# The eight are GRANDFATHERED because their lanes closed in earlier sessions
# and the files cannot be written now without inventing them -- but the tool
# PRINTS them on every run, pass or fail. An allowlist that goes quiet is how a
# debt becomes invisible. New dangling citations FAIL.
#
# Proven in both directions before registration: RC 1 on a planted citation,
# naming both the missing file and the file citing it, RC 0 restored.
add_test(NAME findings_citations
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/check_findings_citations.py)
set_tests_properties(findings_citations PROPERTIES
  LABELS "fast;lint" TIMEOUT 120)

add_test(NAME lint_shell_v2_lease_path
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
          --top-module zhao_shell_v2_lease_path
          ${ZHAO_SHELL_V2_LEASE_PATH_SOURCES})
set_tests_properties(lint_shell_v2_lease_path PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# R1 -- zhao_qsq_bytemul, the shared quarter-square byte multiplier.
#
# One M10K, no DSP, exact for every byte pair. Extracted from
# zhao_terrain_shade, which had built it correctly and kept it to itself while
# R1 and R4 both recorded the reusable primitive as absent.
#
# The test drives ALL 65,536 pairs. That is not thoroughness for its own sake:
# the quarter-square identity is exact for a reason that depends on the PARITY
# of a+b, so a defect shows on half the domain and a sampling test can land
# entirely on the other half and report clean.
add_executable(test_qsq_bytemul_directed common/qsq_bytemul_directed.cpp)
target_link_libraries(test_qsq_bytemul_directed PRIVATE zhao_harness)
verilate(test_qsq_bytemul_directed
  PREFIX Vzhao_qsq_bytemul
  TOP_MODULE zhao_qsq_bytemul
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv
  VERILATOR_ARGS --assert)
add_test(NAME qsq_bytemul_directed COMMAND test_qsq_bytemul_directed)
set_tests_properties(qsq_bytemul_directed PROPERTIES LABELS "fast;nightly")

add_test(NAME lint_qsq_bytemul
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_qsq_bytemul
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv)
set_tests_properties(lint_qsq_bytemul PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# zhao_terrain_shade is NO LONGER SELF-CONTAINED: its quarter-square table
# moved out to the shared zhao_qsq_bytemul primitive (R1), and a source list
# that named the contents rather than the wrapper has to move with it. That is
# the founding defect of source_list_parity and the reason
# formal_raster_fragment_blend reported ERROR instead of a proof for days.
set(ZHAO_TERRAIN_SHADE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_shade.sv)

add_executable(test_terrain_shade_rtl_directed terrain/terrain_shade_rtl_directed.cpp)
target_link_libraries(test_terrain_shade_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_shade_rtl_directed
  PREFIX Vzhao_terrain_shade
  TOP_MODULE zhao_terrain_shade
  SOURCES ${ZHAO_TERRAIN_SHADE_SV})
add_test(NAME terrain_shade_rtl_directed COMMAND test_terrain_shade_rtl_directed)
set_tests_properties(terrain_shade_rtl_directed PROPERTIES LABELS "fast;nightly")
add_test(NAME terrain_shade_break_oracle
         COMMAND test_terrain_shade_rtl_directed --break-oracle)
set_tests_properties(terrain_shade_break_oracle PROPERTIES
  LABELS "fast;nightly" WILL_FAIL TRUE)

add_test(NAME lint_terrain_shade
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_shade
                 ${ZHAO_TERRAIN_SHADE_SV})
set_tests_properties(lint_terrain_shade PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN's LIT NORMALS (owner ruling R21): the world-vertex store beside the
# projector's arena, the ratified face normal and the ratified flat shade, in
# one lane driven by the reference stream at the replay's own rate. This is
# what makes zhao_terrain_normals and zhao_terrain_shade -- both BUILT and
# both disconnected since they were written -- part of the machine.
#
# It proves the value TRAVERSES (a random lattice in, a bit-identical base
# light out), that the count and the order hold, that the stale-generation
# refusal FIRES, and R21's throughput demand IN CLOCKS: the measured interval
# per triangle times the ruled 2,000 terrain triangles must fit the frame.
add_executable(test_terrain_lightlane_directed terrain/terrain_lightlane_directed.cpp)
target_link_libraries(test_terrain_lightlane_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_lightlane_directed
  PREFIX Vtb_terrain_lightlane
  TOP_MODULE tb_terrain_lightlane
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normals.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_shade.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lightlane.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_lightlane.sv)
add_test(NAME terrain_lightlane_directed COMMAND test_terrain_lightlane_directed)
set_tests_properties(terrain_lightlane_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME terrain_lightlane_break_oracle
         COMMAND test_terrain_lightlane_directed --break-oracle)
set_tests_properties(terrain_lightlane_break_oracle PROPERTIES
  LABELS "fast;nightly" WILL_FAIL TRUE TIMEOUT 600)

add_test(NAME lint_terrain_lightlane
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_lightlane
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normals.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_shade.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lightlane.sv)
set_tests_properties(lint_terrain_lightlane PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.UV (the terrain texture-coordinate law, terrain_rules 6.2/6.6)
#
# The frozen law checked TWO independent ways -- the shift itself, and the
# placement algebra in which the pitch cancels -- because `zref` does not
# export the law (it is inline in `draw_terrain`'s loop), so a driver that
# replayed the same shift would be one implementation checking itself. Also
# proves the arithmetic shift on NEGATIVE world coordinates, the oracle's
# `top_shift < 0` clamp as a value AND as a counter, one tile period per cell
# through `zref::terrain::mirror_texel`, and both refusal counters by stimulus.
add_executable(test_terrain_uvlane_directed terrain/terrain_uvlane_directed.cpp)
target_link_libraries(test_terrain_uvlane_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_uvlane_directed
  PREFIX Vtb_terrain_uvlane
  TOP_MODULE tb_terrain_uvlane
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_uvlane.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_uvlane.sv)
add_test(NAME terrain_uvlane_directed COMMAND test_terrain_uvlane_directed)
set_tests_properties(terrain_uvlane_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME terrain_uvlane_break_oracle
         COMMAND test_terrain_uvlane_directed --break-oracle)
set_tests_properties(terrain_uvlane_break_oracle PROPERTIES
  LABELS "fast;nightly" WILL_FAIL TRUE TIMEOUT 600)

add_test(NAME lint_terrain_uvlane
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_uvlane
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_uvlane.sv)
set_tests_properties(lint_terrain_uvlane PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# ---------------------------------------------------------------------------
# TERRAIN.PATCH (phase 6, ZH-033)
#
# The Mantle entry point: terrain_rules 3.4's per-vertex composition chain and
# 9.1's frozen live-field intake (MAX_PATCH_FIELDS = 16, reject the tail, never
# evict). The oracle is a THIN VIEW onto compose_lattice, and
# terrain_patch_directed proves that by composing a real 33x33 dual patch with
# two real earth programs both ways and requiring all 1,089 vertices to agree â€”
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

# The cull's other MUL_LANES arms, on the SAME source and the SAME oracle
# (2026-09-10). The default target above is MUL_LANES=2 (two shared 33x33
# lanes, walk 21, extraction 191). MUL_LANES=4 is the ORIGINAL spatial arm --
# the fitted 15-DSP circuit, kept under generate -- with its own walk laws
# (10 / 186); MUL_LANES=1 is the one-lane arm (41 / 191). Same checks, same
# 17,212-check suite; only the pinned walk lengths differ, which is the point:
# sequencing moves cycles, never bits.
add_executable(test_geom_cull_spatial differential/geom_cull_directed.cpp)
target_compile_definitions(test_geom_cull_spatial PRIVATE
  ZHAO_CULL_WALK=10 ZHAO_CULL_EXTRACT=186)
target_link_libraries(test_geom_cull_spatial PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_cull_spatial PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_geom_cull_spatial
  PREFIX Vzhao_geom_cull
  TOP_MODULE zhao_geom_cull
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_cull.sv
  VERILATOR_ARGS -GMUL_LANES=4)
add_test(NAME geom_cull_spatial COMMAND test_geom_cull_spatial)
set_tests_properties(geom_cull_spatial PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_geom_cull_lane1 differential/geom_cull_directed.cpp)
target_compile_definitions(test_geom_cull_lane1 PRIVATE
  ZHAO_CULL_WALK=41 ZHAO_CULL_EXTRACT=191)
target_link_libraries(test_geom_cull_lane1 PRIVATE zhao_harness zhao_zref)
target_include_directories(test_geom_cull_lane1 PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp
verilate(test_geom_cull_lane1
  PREFIX Vzhao_geom_cull
  TOP_MODULE zhao_geom_cull
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_cull.sv
  VERILATOR_ARGS -GMUL_LANES=1)
add_test(NAME geom_cull_lane1 COMMAND test_geom_cull_lane1)
set_tests_properties(geom_cull_lane1 PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_zhao_geom_cull_spatial
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_cull -GMUL_LANES=4
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_cull.sv)
set_tests_properties(lint_zhao_geom_cull_spatial PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE POSITIVE CONTROL for the cull differential's checker. The lane arms'
# issue/commit accumulator must start a fresh sum at every plane boundary;
# tests/mutants/zhao_geom_cull_mutant.sv is a committed copy with that clear
# removed. THIS TEST PASSES WHEN THE DIFFERENTIAL FIRES -- inverse polarity,
# evidence about the instrument, not the design. It also asserts the exact walk
# law still holds on the mutant: the timing pin is blind to this fault.
add_executable(test_geom_cull_mutant_control differential/geom_cull_mutant_control.cpp)
target_link_libraries(test_geom_cull_mutant_control PRIVATE zhao_harness zhao_zref)
verilate(test_geom_cull_mutant_control
  PREFIX Vzhao_geom_cull_mutant
  TOP_MODULE zhao_geom_cull_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_cull_mutant.sv)
add_test(NAME geom_cull_mutant_control COMMAND test_geom_cull_mutant_control)
set_tests_properties(geom_cull_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

set(ZHAO_TERRAIN_PATCH_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch.sv)

# FIELD.OUT.HEIGHT: the op ops.yml declares with "no dedicated opcode" -- the op
# IS the routing of the height out-lane into the Â§3.4 compose chain. Narrow on
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

# FORGE.SHADOW -- contact shadows as ordinary geometry. The bench checks the
# frozen ladder, the governor floor as a MAX (never an override), one
# unit-circle table shared by stride, the three CORRECT refusals (void, zero
# radius, far rung) counted apart from the one real FAULT, and that the depth
# bias is applied per rung. It deliberately does NOT judge the bias VALUES:
# the contract puts those on a look-gate and CLAUDE.md says measurement cannot
# choose a value.
set(ZHAO_FORGE_SHADOW_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_shadow.sv)
add_executable(test_forge_shadow_directed forge/forge_shadow_directed.cpp)
target_link_libraries(test_forge_shadow_directed PRIVATE zhao_harness)
verilate(test_forge_shadow_directed
  PREFIX Vzhao_forge_shadow
  TOP_MODULE zhao_forge_shadow
  SOURCES ${ZHAO_FORGE_SHADOW_SV})
add_test(NAME forge_shadow_directed COMMAND test_forge_shadow_directed)
set_tests_properties(forge_shadow_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# FORGE.SHADOW's TERMINAL LINK -- the two blocks that let a shadow hull RIDE
# `zhao_forge_assemble` instead of cloning it. `zhao_forge_fanindex` turns the
# hull's RING into a triangle fan (the only genuinely new geometry the shadow
# subsystem needed); `zhao_forge_jobarb` arbitrates FORGE.PRIM and FORGE.SHADOW
# onto that one assembler at JOB granularity, with FORGE.PRIM at absolute
# priority because the contract says a stalled shadow must never delay a
# creature. Both benches carry the positive controls for their own fault
# counters -- reachable from the port, so neither needs a committed mutant.
set(ZHAO_FORGE_FANINDEX_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_fanindex.sv)
add_executable(test_forge_fanindex_directed forge/forge_fanindex_directed.cpp)
target_link_libraries(test_forge_fanindex_directed PRIVATE zhao_harness)
verilate(test_forge_fanindex_directed
  PREFIX Vzhao_forge_fanindex
  TOP_MODULE zhao_forge_fanindex
  SOURCES ${ZHAO_FORGE_FANINDEX_SV})
add_test(NAME forge_fanindex_directed COMMAND test_forge_fanindex_directed)
set_tests_properties(forge_fanindex_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_forge_fanindex
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_fanindex
                 ${ZHAO_FORGE_FANINDEX_SV})
set_tests_properties(lint_forge_fanindex PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

set(ZHAO_FORGE_JOBARB_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jobarb.sv)
add_executable(test_forge_jobarb_directed forge/forge_jobarb_directed.cpp)
target_link_libraries(test_forge_jobarb_directed PRIVATE zhao_harness)
verilate(test_forge_jobarb_directed
  PREFIX Vzhao_forge_jobarb
  TOP_MODULE zhao_forge_jobarb
  SOURCES ${ZHAO_FORGE_JOBARB_SV})
add_test(NAME forge_jobarb_directed COMMAND test_forge_jobarb_directed)
set_tests_properties(forge_jobarb_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_forge_jobarb
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_forge_jobarb
                 ${ZHAO_FORGE_JOBARB_SV})
set_tests_properties(lint_forge_jobarb PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# MEM.UPLOAD -- the HPS->VRAM resource upload engine. Its oracle
# (zref::mem::upload_verdict) was written BEFORE the RTL, so the bench is a
# DIFFERENTIAL against that oracle rather than a second transcription of the
# rules. Several cases carry TWO faults at once, because the oracle's ORDER is
# part of the law and single-fault cases pass against a backwards priority.
set(ZHAO_MEM_UPLOAD_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/mem/zhao_mem_upload.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/mem/tb_mem_upload.sv)
add_executable(test_mem_upload_directed mem/mem_upload_directed.cpp)
target_link_libraries(test_mem_upload_directed PRIVATE zhao_harness)
target_include_directories(test_mem_upload_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/include)
verilate(test_mem_upload_directed
  PREFIX Vtb_mem_upload
  TOP_MODULE tb_mem_upload
  SOURCES ${ZHAO_MEM_UPLOAD_SV})
add_test(NAME mem_upload_directed COMMAND test_mem_upload_directed)
set_tests_properties(mem_upload_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.PLACE -- the placement owner entry I27 said did not exist. The block is
# pure shifts of the lattice index, so the test checks the ARITHMETIC against
# spec/terrain_rules.md 1.3/2.1 written out by hand, and fires all four censuses:
# a corrupt envelope, an illegal pitch, an out-of-range patch, and the accept
# count. No mutant is needed -- every refusal is reachable from the header port,
# which is the point of the envelope redundancy the format carries.
set(ZHAO_TERRAIN_PLACE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place_law_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place.sv)
add_executable(test_terrain_place_directed terrain/terrain_place_directed.cpp)
target_link_libraries(test_terrain_place_directed PRIVATE zhao_harness)
verilate(test_terrain_place_directed
  PREFIX Vzhao_terrain_place
  TOP_MODULE zhao_terrain_place
  SOURCES ${ZHAO_TERRAIN_PLACE_SV})
add_test(NAME terrain_place_directed COMMAND test_terrain_place_directed)
set_tests_properties(terrain_place_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.HDRREAD -- the compose-path patch header reader that closed
# zhao_console_core entry I35. The DUT is wrapped by tb_terrain_hdrread.sv only
# to flatten `zhao_guard_req_t` / `zhao_guard_rsp_t`, so a C++ driver names the
# fields instead of indexing bit positions it would be silently wrong about.
#
# The driver builds each page header BY BYTE OFFSET from spec/terrain_rules.md
# 2.1 rather than from the RTL's beat cases, fires all five counters with legal
# stimulus (a denied guard, a short burst, a header naming another patch, an
# out-of-pool slot, a stale epoch), and asserts one-job-in / one-header-out /
# one-job-forwarded after every case -- a refusal that swallowed the job would
# leave the page pinned in the directory forever.
add_executable(test_terrain_hdrread_directed terrain/terrain_hdrread_directed.cpp)
target_link_libraries(test_terrain_hdrread_directed PRIVATE zhao_harness)
verilate(test_terrain_hdrread_directed
  PREFIX Vtb_terrain_hdrread
  TOP_MODULE tb_terrain_hdrread
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_hdrread.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_hdrread.sv)
add_test(NAME terrain_hdrread_directed COMMAND test_terrain_hdrread_directed)
set_tests_properties(terrain_hdrread_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ITS POSITIVE CONTROL, and it is the one check above that no legal stimulus can
# fire. A correct block has no input that makes it forward the wrong patch, so
# "the ordering check can fail" would stay an argument; the mutant drives the
# five forwarding assignments from the LIVE job port -- which is the wiring
# zhao_console_core actually had before this block existed -- and this driver's
# polarity is INVERTED, so it PASSES when the fault is visible.
#
# `-DZHAO_HDRREAD_LIVE_FORWARD_MUTANT` selects the DUT through a PLAIN `ifdef in
# the shared testbench, which is a form Verilator's -D reaches. The negative
# control is recorded in the driver's header: built WITHOUT the define, against
# production, this same driver FAILS -- which is the proof the selector engaged
# rather than silently compiling the unmutated block. UNUSEDSIGNAL is waived
# because the mutation is precisely what leaves the latched registers unread.
add_executable(test_terrain_hdrread_mutant_control terrain/terrain_hdrread_mutant_control.cpp)
target_link_libraries(test_terrain_hdrread_mutant_control PRIVATE zhao_harness)
verilate(test_terrain_hdrread_mutant_control
  PREFIX Vtb_terrain_hdrread
  TOP_MODULE tb_terrain_hdrread
  VERILATOR_ARGS --assert -DZHAO_HDRREAD_LIVE_FORWARD_MUTANT -Wno-UNUSEDSIGNAL
  SOURCES ${ZHAO_ABI_PKG} ${ZHAO_ZHAO_PKG}
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_hdrread_live_forward_mutant.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_hdrread.sv)
add_test(NAME terrain_hdrread_mutant_control COMMAND test_terrain_hdrread_mutant_control)
set_tests_properties(terrain_hdrread_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# THE PLACEMENT SEAM: TERRAIN.PLACE's 66-write burst, read back out of
# TERRAIN.COMPCACHE's serve port. Registered 2026-09-19 with the composition
# that closed core header entry I22 and the door half of I27.
#
# WHY IT IS A SEPARATE TARGET FROM THE TWO ABOVE. `terrain_place_directed`
# checks the shifter against spec/terrain_rules.md 1.3/2.1 and fires all four of
# its counters; `compcache_front_rtl_directed` checks the store with a harness
# writing `pos_*` by hand. Both pass with the two blocks WIRED WRONG -- a wx/wz
# transposition, or a burst landing in the wrong buffer parity, is invisible
# from either end alone and is exactly what `zhao_console_core` now depends on.
# Component checks passing is not composition evidence.
add_executable(test_terrain_place_cache_directed terrain/terrain_place_cache_directed.cpp)
target_link_libraries(test_terrain_place_cache_directed PRIVATE zhao_harness)
verilate(test_terrain_place_cache_directed
  PREFIX Vtb_terrain_place_cache
  TOP_MODULE tb_terrain_place_cache
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_place_cache.sv)
add_test(NAME terrain_place_cache_directed COMMAND test_terrain_place_cache_directed)
set_tests_properties(terrain_place_cache_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# THE POSITIVE CONTROL for the two placement checks above. It swaps the axis bit
# on the ONE wire between the two blocks -- neither block is edited, the
# mutation is the WIRING, which is the fault this bench is about -- and inverts
# the driver's polarity, so it passes when the checks FAIL. Without it, "0
# failed" is a claim that these checks can tell the column plane from the row
# plane and nothing has tested that claim.
#
# The macro selector is a plain `ifdef` and its NEGATIVE control is the target
# above: the same C++ with the macro undefined passes. CLAUDE.md records why
# that pairing is required -- a command-line -D cannot override a function-like
# `define and says nothing when it fails to, so two mutants once passed while
# measuring unmutated production.
add_executable(test_terrain_place_cache_axis_swap terrain/terrain_place_cache_directed.cpp)
target_link_libraries(test_terrain_place_cache_axis_swap PRIVATE zhao_harness)
target_compile_definitions(test_terrain_place_cache_axis_swap PRIVATE ZHAO_EXPECT_FAIL)
verilate(test_terrain_place_cache_axis_swap
  PREFIX Vtb_terrain_place_cache
  TOP_MODULE tb_terrain_place_cache
  VERILATOR_ARGS +define+ZHAO_PLACE_CACHE_AXIS_SWAP
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_place_cache.sv)
add_test(NAME terrain_place_cache_axis_swap_control
         COMMAND test_terrain_place_cache_axis_swap)
set_tests_properties(terrain_place_cache_axis_swap_control
  PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

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
# construction â€” the contract's crack invariant, checked on real emitted
# geometry in terrain_tess_directed rather than argued in prose.
set(ZHAO_TERRAIN_TESS_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
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

# TERRAIN.TESS vertex mode + reference mode (2026-09-10, RUN-20260910-0934).
# The tessellator's `job_mode_i` = 1 emits the 81 window vertices per job with
# the geomorph applied (the arena's dense fill, `vertex_at` in hardware); = 2
# emits index triples for every triangle of the SAME walk (stitched and coarse
# included). The differential runs the identity probe's whole case space
# (110,592 jobs) and holds three things per job: vertices == vertex_at,
# triples == tessellate's corners as window indices, and triples applied to
# the vertices REBUILD tessellate's triangles. Mode 0 is the unchanged block;
# terrain_tess_directed/_random/_normals above run against the same source.
# See reports/TERRAIN-TESS-VERTEX-MODE-20260910.md.
add_executable(test_terrain_tess_modes_directed terrain/terrain_tess_modes_directed.cpp)
target_link_libraries(test_terrain_tess_modes_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_tess_modes_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_tess_modes_directed
  PREFIX Vzhao_terrain_tess
  TOP_MODULE zhao_terrain_tess
  SOURCES ${ZHAO_TERRAIN_TESS_SV})
add_test(NAME terrain_tess_modes_directed COMMAND test_terrain_tess_modes_directed)
set_tests_properties(terrain_tess_modes_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

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
# TERRAIN.WCACHE + the projection subsystem (2026-09-10, RUN-20260910-0846)
#
# zhao_proj_subsystem composes ONE zhao_project_service (client A geometry,
# client B terrain) and the terrain arena shell fed straight off the client-B
# result port. Its generic reference input is driven here by a bench-local copy
# of the retained level-0 walker; production composition instead drives it from
# zhao_terrain_tess ModeRef through zhao_terrain_group_seq. The differential
# requires every replayed triangle to be bit-identical to the retained projector
# (and, for the three `w` lanes the legacy packet never carried, to
# project_vertex). Three elaborations of ONE unchanged driver: ROWS_PER_PASS=3
# (default), ROWS_PER_PASS=1 (the producer honours b_ready_o), VALID_MODE=0
# (bitmap primitive under the same shell -- the miss-path positive control,
# since a dense arena provably cannot miss).
# See reports/PROJECTION-ADOPTION-20260910.md.
set(ZHAO_PROJ_SUBSYSTEM_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_proj_subsystem.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_service.sv
    ${ZHAO_PROJECT_CORE_SV}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_wcache.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
set(ZHAO_TB_TERRAIN_WCACHE_SV
    ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_wcache.sv
    ${ZHAO_PROJ_SUBSYSTEM_SV}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_topo.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_project.sv)

add_executable(test_terrain_wcache_differential terrain/terrain_wcache_differential.cpp)
target_link_libraries(test_terrain_wcache_differential PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_wcache_differential PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp (project_vertex, w)
verilate(test_terrain_wcache_differential
  PREFIX Vtb_terrain_wcache
  TOP_MODULE tb_terrain_wcache
  SOURCES ${ZHAO_TB_TERRAIN_WCACHE_SV})
add_test(NAME terrain_wcache_differential COMMAND test_terrain_wcache_differential)
set_tests_properties(terrain_wcache_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_terrain_wcache_differential_rpp1 terrain/terrain_wcache_differential.cpp)
target_link_libraries(test_terrain_wcache_differential_rpp1 PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_wcache_differential_rpp1 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_wcache_differential_rpp1
  PREFIX Vtb_terrain_wcache
  TOP_MODULE tb_terrain_wcache
  VERILATOR_ARGS -GROWS_PER_PASS=1
  SOURCES ${ZHAO_TB_TERRAIN_WCACHE_SV})
add_test(NAME terrain_wcache_differential_rpp1 COMMAND test_terrain_wcache_differential_rpp1)
set_tests_properties(terrain_wcache_differential_rpp1 PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_terrain_wcache_differential_bitmap terrain/terrain_wcache_differential.cpp)
target_link_libraries(test_terrain_wcache_differential_bitmap PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_terrain_wcache_differential_bitmap PRIVATE ZHAO_TWC_VALID_MODE=0)
target_include_directories(test_terrain_wcache_differential_bitmap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_wcache_differential_bitmap
  PREFIX Vtb_terrain_wcache
  TOP_MODULE tb_terrain_wcache
  VERILATOR_ARGS -GVALID_MODE=0
  SOURCES ${ZHAO_TB_TERRAIN_WCACHE_SV})
add_test(NAME terrain_wcache_differential_bitmap COMMAND test_terrain_wcache_differential_bitmap)
set_tests_properties(terrain_wcache_differential_bitmap PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# The 81-vertex identity obligation (rescue roadmap 8.5), measured on the zref
# tessellator over every level x neighbour-level x morph x surface x page case:
# every emitted corner is a lattice vertex inside the 9x9 window and one index
# has one final position per job. No RTL; the instrument is the oracle.
add_executable(test_terrain_identity_probe terrain/terrain_identity_probe.cpp)
target_link_libraries(test_terrain_identity_probe PRIVATE zhao_harness zhao_zref)
add_test(NAME terrain_identity_probe COMMAND test_terrain_identity_probe)
set_tests_properties(terrain_identity_probe PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_proj_subsystem
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_proj_subsystem
                 ${ZHAO_PROJ_SUBSYSTEM_SV})
set_tests_properties(lint_proj_subsystem PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN projection pipeline composition (not production-adopted)
#
# Real ModeVtx + ModeRef tessellation, group/view/arena sequencing, one shared
# projector, and the three-copy arena shell. The same driver compares both
# views against retained zhao_terrain_project, exercises bitmap sparse fill,
# and gives release_unsafe_o an inverted-polarity committed-mutant control.
set(ZHAO_TERRAIN_PIPE_COMMON_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tess.sv
    ${ZHAO_PROJ_SUBSYSTEM_SV})
set(ZHAO_TERRAIN_PIPE_SV
    ${ZHAO_TERRAIN_PIPE_COMMON_SV}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_group_seq.sv)
set(ZHAO_TERRAIN_PIPE_MUTANT_SV
    ${ZHAO_TERRAIN_PIPE_COMMON_SV}
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_group_seq_mutant.sv)

add_executable(test_terrain_pipe_differential terrain/terrain_pipe_differential.cpp)
target_link_libraries(test_terrain_pipe_differential PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_pipe_differential PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_pipe_differential
  PREFIX Vzhao_terrain_pipe
  TOP_MODULE zhao_terrain_pipe
  SOURCES ${ZHAO_TERRAIN_PIPE_SV})
verilate(test_terrain_pipe_differential
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_pipe_differential COMMAND test_terrain_pipe_differential)
set_tests_properties(terrain_pipe_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# PACKET I PREREQUISITE: the pipe at the exact G8B parameters.
#
# The G8B target is `zhao_terrain_pipe #(.ROWS_PER_PASS(3), .MATW(18))`, and the
# architecture is explicit that the wrapper "must not rely on
# zhao_terrain_pipe's default MATW=32, a fit-target comment, or a runtime
# convention to establish the parameters", and that G8B runs only once "all
# non-fit RPP3/MATW18 tests pass". There were none: every terrain_pipe test ran
# at the DEFAULT MATW=32, so the configuration G8B is meant to characterise had
# never been elaborated, let alone compared against the oracle.
#
# proj_matw_directed already pins MATW=18 against MATW=32 at the projector CORE,
# at ROWS_PER_PASS 3 and 1, with the refusal law and a positive control that
# skews one narrowed word by a raw LSB. This is the same question one level up:
# the whole pipe, against the retained zhao_terrain_project oracle.
#
# WHAT THIS TEST DOES AND DOES NOT ESTABLISH, because the division of labour
# between it and proj_matw_directed is the whole reason it is not redundant:
#
#   DOES  -- the pipe ELABORATES at ROWS_PER_PASS=3/MATW=18 and is bit-exact
#            against the oracle on the dense workload, with zero matrix
#            refusals, identical packet/vertex/cycle counts and the identical
#            37 checks the MATW=32 build runs.
#   DOES NOT -- distinguish 18 from 32 by its own output. It cannot: on this
#            stimulus the two are bit-identical, which is the RESULT, not an
#            oversight. That the flag took effect is Verilator's own build
#            record (-GMATW=18 -GROWS_PER_PASS=3), and that 18 BEHAVES
#            differently from 32 is proven by proj_matw_directed's control.
#
# Said plainly so nobody later reads a green here as "the narrowing is proven
# harmless everywhere". It is proven harmless on this workload.
add_executable(test_terrain_pipe_rpp3_matw18 terrain/terrain_pipe_differential.cpp)
target_link_libraries(test_terrain_pipe_rpp3_matw18 PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_terrain_pipe_rpp3_matw18 PRIVATE ZHAO_PIPE_MATW=18)
target_include_directories(test_terrain_pipe_rpp3_matw18 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_pipe_rpp3_matw18
  PREFIX Vzhao_terrain_pipe
  TOP_MODULE zhao_terrain_pipe
  VERILATOR_ARGS -GROWS_PER_PASS=3 -GMATW=18
  SOURCES ${ZHAO_TERRAIN_PIPE_SV})
verilate(test_terrain_pipe_rpp3_matw18
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_pipe_rpp3_matw18 COMMAND test_terrain_pipe_rpp3_matw18)
set_tests_properties(terrain_pipe_rpp3_matw18 PROPERTIES
  LABELS "fast;nightly;packet-i" TIMEOUT 1800)

add_executable(test_terrain_pipe_differential_bitmap terrain/terrain_pipe_differential.cpp)
target_link_libraries(test_terrain_pipe_differential_bitmap PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_terrain_pipe_differential_bitmap PRIVATE ZHAO_PIPE_VALID_MODE=0)
target_include_directories(test_terrain_pipe_differential_bitmap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_pipe_differential_bitmap
  PREFIX Vzhao_terrain_pipe
  TOP_MODULE zhao_terrain_pipe
  VERILATOR_ARGS -GVALID_MODE=0
  SOURCES ${ZHAO_TERRAIN_PIPE_SV})
verilate(test_terrain_pipe_differential_bitmap
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_pipe_differential_bitmap COMMAND test_terrain_pipe_differential_bitmap)
set_tests_properties(terrain_pipe_differential_bitmap PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_executable(test_terrain_pipe_release_mutant terrain/terrain_pipe_differential.cpp)
target_link_libraries(test_terrain_pipe_release_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_terrain_pipe_release_mutant PRIVATE ZHAO_EXPECT_RELEASE_MUTANT=1)
target_include_directories(test_terrain_pipe_release_mutant PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_terrain_pipe_release_mutant
  PREFIX Vzhao_terrain_pipe
  TOP_MODULE zhao_terrain_pipe
  VERILATOR_ARGS -DZHAO_MUTANT_GROUP_SEQ
  SOURCES ${ZHAO_TERRAIN_PIPE_MUTANT_SV})
verilate(test_terrain_pipe_release_mutant
  PREFIX Vzhao_terrain_project
  TOP_MODULE zhao_terrain_project
  SOURCES ${ZHAO_TERRAIN_PROJECT_SV})
add_test(NAME terrain_pipe_release_mutant COMMAND test_terrain_pipe_release_mutant)
set_tests_properties(terrain_pipe_release_mutant PROPERTIES LABELS "fast;mutant" TIMEOUT 1800)

add_test(NAME lint_terrain_pipe
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_pipe
                 ${ZHAO_TERRAIN_PIPE_SV})
set_tests_properties(lint_terrain_pipe PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_terrain_pipe_release_mutant
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 -DZHAO_MUTANT_GROUP_SEQ
                 --top-module zhao_terrain_pipe
                 ${ZHAO_TERRAIN_PIPE_MUTANT_SV})
set_tests_properties(lint_terrain_pipe_release_mutant PROPERTIES
  LABELS "lint;mutant"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# Downstream rate probes recovered from the composition campaign. CLIP->SETUP
# measures the one-triangle/clock seam; the separate real-binner probe records
# today's five-clock wall and TRI_CAP=128 without pretending it is composed.
set(ZHAO_TERRAIN_DOWNSTREAM_SV
    ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_downstream.sv
    ${ZHAO_GEOM_CLIP_SV}
    ${ZHAO_GEOM_SETUP_SV})
add_executable(test_terrain_downstream_rate terrain/terrain_downstream_rate.cpp)
target_link_libraries(test_terrain_downstream_rate PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_downstream_rate
  PREFIX Vtb_terrain_downstream
  TOP_MODULE tb_terrain_downstream
  SOURCES ${ZHAO_TERRAIN_DOWNSTREAM_SV})
add_test(NAME terrain_downstream_rate COMMAND test_terrain_downstream_rate)
set_tests_properties(terrain_downstream_rate PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_terrain_downstream_binner terrain/terrain_downstream_binner.cpp)
target_link_libraries(test_terrain_downstream_binner PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_downstream_binner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_terrain_downstream_binner
  PREFIX Vzhao_geom_binner
  TOP_MODULE zhao_geom_binner
  SOURCES ${ZHAO_GEOM_BINNER_SV})
add_test(NAME terrain_downstream_binner COMMAND test_terrain_downstream_binner)
set_tests_properties(terrain_downstream_binner PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_terrain_downstream
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module tb_terrain_downstream
                 ${ZHAO_TERRAIN_DOWNSTREAM_SV})
set_tests_properties(lint_terrain_downstream PROPERTIES
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv
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

# GEOM.WCACHE's shell. It adds no logic -- the mechanism, the formal proof and
# the 73-check differential all live on zhao_vertex_arena -- so a lint is the
# whole gate it needs. What the shell carries is the projected-vertex parameters
# and the payload's MEANING, which the primitive must not know.
add_test(NAME lint_geom_wcache
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_wcache
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_wcache.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv)
set_tests_properties(lint_geom_wcache PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# formal: the arena bound the ledger names (tests/formal/
# geom_binner_arena_bounds.sby), proved on zhao_geom_arena â€” the module
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv)

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
    # D22 step 1: GEOM.SETUP is now composed into the shell bench, so the
    # boundary can move back from precomputed edge equations to vertices.
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
    # RENDER: the shell drives the arbiter ENGINE0 port now, so its whole cone
    # is part of the synthesizable shell.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fbwrite.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_bin_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_binner.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_arena.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv
    # D22 step 1: GEOM.SETUP composed into the shell bench, so the input
    # boundary can move back from precomputed edge equations to VERTICES.
    # Listed AFTER the packages it depends on -- Verilator resolves in list
    # order, and a module placed before its package is simply not found.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_setup.sv
    # D22 step 2: GEOM.DEPTHQUANT and the reciprocal service it calls.
    # DEPTHQUANT does not own a reciprocal -- "the console already has ONE
    # rcp_u24 law; a second ROM would be a second law" -- so the service and
    # its ROM come with it.
    # D22 step 6: GEOM.MESHFETCH, which reads the meshlet descriptor. The
    # bench plays its guard, beat stream and cull service -- the real asset
    # fetcher over GEOM.ASSET_POOL is D22's remaining work.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_meshfetch.sv
    # D22 step 5: GEOM.ASSEMBLE, which names the three vertices PROJECT then
    # transforms. The bench still holds the vertex TABLE -- that is
    # GEOM.VDECODE's job and step 5 moves the SELECTION, not the decode.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_assemble.sv
    # TREAD 7: the record decoder the bench used to stand in for.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_vdecode.sv
    # TREAD 8: the fetcher that reads the meshlet footprint out of the pool.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_assetfetch.sv
    # D22 step 4: GEOM.PROJECT, in front of CLIP. It feeds CLIP the screen
    # vertices and behind flags, and DEPTHQUANT the w.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_project.sv
    # D22 step 3: GEOM.CLIP, in front of SETUP.
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clip.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_svc.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_shell_top.sv)

# synthesizable-cone lint (the model/tb are excluded by construction)
add_test(NAME lint_shell_top
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_shell_top
                 ${ZHAO_SHELL_RTL})

# THE SIBLING SHELL ITSELF. Its closure is the union of the historical shell
# list and Packet D's -- the bin-pipe swap drags in the whole V3 texture
# island -- minus the two blocks it replaces, plus the six V2 organs. Built
# from the same manifests both sides already read, so there is no third copy
# to go stale.
set(ZHAO_SHELL_V2_TOP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/shell/v3_closure_inherited.vlt)
foreach(relative_source IN LISTS ZHAO_LEASE_PATH_PD_RELATIVE)
  if(NOT relative_source MATCHES "^tests/")
    list(APPEND ZHAO_SHELL_V2_TOP_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
  endif()
endforeach()
foreach(shell_source IN LISTS ZHAO_SHELL_RTL)
  if(NOT shell_source MATCHES "zhao_shell_top\\.sv$" AND
     NOT shell_source MATCHES "zhao_geom_bin_pipe\\.sv$" AND
     NOT shell_source MATCHES "zhao_video_slotmgr\\.sv$")
    if(NOT "${shell_source}" IN_LIST ZHAO_SHELL_V2_TOP_SOURCES)
      list(APPEND ZHAO_SHELL_V2_TOP_SOURCES "${shell_source}")
    endif()
  endif()
endforeach()
list(APPEND ZHAO_SHELL_V2_TOP_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_slotmgr_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_renderer_lease_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_terminal_adapter_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_blit_lease_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_ready_cdc_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_ready_bridge_v2.sv
  # POST.COMPOSITE's lease inside the shell (core I15/I16, 2026-09-19): the
  # ENGINE0 share, the source read-back and POST.ECHO.
  ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_fbread.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_echo.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_lease.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_shell_top_v2.sv)

add_test(NAME lint_shell_top_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
          --top-module zhao_shell_top_v2
          ${ZHAO_SHELL_V2_TOP_SOURCES})
set_tests_properties(lint_shell_top_v2 PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE SIBLING'S TEN-PIN INSTRUMENT, gated the same way V1's is.
#
# `fpga/rtl/generated/zhao_shell_v2_fit_top.sv` is generated from
# `design/shell_fit_ports_v2.yml` and the sibling's declaration. Without these
# two gates it is a generated file that nobody regenerates -- which this
# repository has now been bitten by twice in one session, both times as a stale
# artifact carrying a reassuring provenance line at the top of it.
#
# The freshness gate re-runs the generator in --check mode, so an edit to the
# policy, to gen_shell_fit_top.py, or to the shell's port list that is not
# followed by a regeneration goes red here instead of at the next fit.
add_test(NAME shell_v2_fit_generated_freshness
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_SOURCE_DIR}/tools/quartus/gen_shell_fit_top.py --check
          --shell ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_shell_top_v2.sv
          --module zhao_shell_top_v2
          --name-prefix shell_v2
          --policy ${CMAKE_SOURCE_DIR}/design/shell_fit_ports_v2.yml
          --rtl-out ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_v2_fit_top.sv
          --manifest-out
            ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_v2_fit_top.manifest.json)

# And a synthesis-mode lint, because a clean generator run says nothing about
# whether the thing it wrote elaborates. Top module is `shell_v2_top` -- the
# --name-prefix names it -- and the closure is the sibling shell's own sources
# plus the wrapper.
add_test(NAME lint_shell_v2_fit_top
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wno-fatal
          --top-module shell_v2_top
          -DSYNTHESIS=1 -DQUARTUS_SYNTHESIS=1
          ${ZHAO_SHELL_V2_TOP_SOURCES}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_v2_fit_top.sv)

set_tests_properties(shell_v2_fit_generated_freshness lint_shell_v2_fit_top
  PROPERTIES LABELS "lint;fast;packet-h" TIMEOUT 600
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

set_tests_properties(lint_shell_top PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The sibling shell's fit port policy is GENERATED from the V1 policy plus the
# V2 declaration -- 154 inherited entries whose ordinals all shift, and 63
# authored ones. A generated file nobody regenerates is a stale file with a
# reassuring provenance line at the top, and this session lost an hour to
# exactly that failure in two separate reports.
# Packet H's composed-fit rows get a receipt law, as G8B's already had. It
# enforces the condition a row must satisfy the moment it is stamped `ok` --
# clean tree, DSP inside the ruled 85 -- and deliberately does NOT gate on the
# 30,000-ALM target, which is a whole-machine number that this composition is
# only a part of.
add_test(NAME packet_h_composed_receipt
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tests/tools/test_packet_h_composed_receipt.py -q)
set_tests_properties(packet_h_composed_receipt PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

add_test(NAME packet_h_fit_ports_v2_fresh
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/quartus/gen_shell_fit_ports_v2.py --check)
set_tests_properties(packet_h_fit_ports_v2_fresh PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

# PACKET-H GATE CLAUSE: unaffected behaviour matches under PAIRED TRAFFIC.
#
# `packet_h_sibling_diff` is the structural half and compares TEXT. This is the
# behavioural half: both shells, one stimulus, every output outside the changed
# path compared cycle for cycle.
#
# The closure is the sibling's list PLUS the two blocks the sibling replaced --
# `zhao_geom_bin_pipe` and `zhao_video_slotmgr` are filtered OUT of
# ZHAO_SHELL_V2_TOP_SOURCES above, and the historical shell still instantiates
# both, so a paired build needs them back. Derived from the same two lists
# rather than written out again, so there is no third copy to go stale.
set(ZHAO_SHELL_PAIRED_SOURCES ${ZHAO_SHELL_V2_TOP_SOURCES})
foreach(shell_source IN LISTS ZHAO_SHELL_RTL)
  if(NOT "${shell_source}" IN_LIST ZHAO_SHELL_PAIRED_SOURCES)
    list(APPEND ZHAO_SHELL_PAIRED_SOURCES "${shell_source}")
  endif()
endforeach()
list(APPEND ZHAO_SHELL_PAIRED_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/shell/zhao_shell_paired_diff.sv)

add_test(NAME lint_shell_paired_diff
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
          --top-module zhao_shell_paired_diff
          ${ZHAO_SHELL_PAIRED_SOURCES})
set_tests_properties(lint_shell_paired_diff PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The harness is GENERATED from the two shells' port lists. A generated file
# nobody regenerates is a stale file with a reassuring provenance line at the
# top -- `zhao_prod_top.sv` was found stale for two separate port changes made
# days apart, and this one would go stale the same silent way.
add_test(NAME packet_h_paired_diff_fresh
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/gen_shell_paired_diff.py --check)
set_tests_properties(packet_h_paired_diff_fresh PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

# THE OTHER HALF OF THIS CHECK -- `--check --mutant`, over
# `tests/mutants/zhao_shell_paired_diff_mutant.sv` -- IS ALREADY REGISTERED,
# as `packet_h_paired_diff_mutant_fresh`, about eighty lines below, next to
# the mutant control it guards.
#
# This comment exists because on 2026-09-21 that gate was RED and nobody could
# have known. `zhao_shell_top_v2` gained the seven `gth_*` gather outputs; the
# generated harness above picked them up and the committed mutant did not, so
# its `u_v2` instantiation was short seven pins and
# `verilate(test_shell_paired_diff_mutant)` failed with 7 PINMISSING promoted
# to an error -- **which stops `cmake --preset windows-native` BEFORE it
# reaches the `add_test` that registers the detector.**
#
# So the gate was not missing and was not skipped. It was UNREGISTERABLE, by
# the very defect it exists to report, from a target twenty lines above it.
# CLAUDE.md's "a gate that cannot reach the state is not evidence about the
# state" has a second form: a gate that cannot be INSTALLED is not evidence
# either, and the two are indistinguishable from a green gate list -- because
# a test that was never registered cannot fail.
#
# Every static gate was green throughout. `mutant_copy_drift.py` skipped the
# file entirely (it compares COMMIT ORDER against a PRODUCTION module, and
# this copy's subject is a GENERATED harness, so it is one of that tool's own
# "16 files matched no production module"); `wrapper_port_parity.py` covers
# the two `zhao_console_core` wrappers and compares PORT LISTS, while this
# fault is in an INSTANTIATION's PIN LIST; `mutant_drivers.py` asks whether
# something BUILDS the mutant, and something does.
#
# Repaired by regeneration: `python tools/design/gen_shell_paired_diff.py
# --mutant`. R220's shape, in a third mirror file.

add_executable(test_shell_paired_diff_directed
  shell/shell_paired_diff_directed.cpp)
target_link_libraries(test_shell_paired_diff_directed PRIVATE zhao_harness)
verilate(test_shell_paired_diff_directed
  PREFIX Vzhao_shell_paired_diff
  TOP_MODULE zhao_shell_paired_diff
  SOURCES ${ZHAO_SHELL_PAIRED_SOURCES})
add_test(NAME shell_paired_diff_directed
  COMMAND test_shell_paired_diff_directed)
set_tests_properties(shell_paired_diff_directed PROPERTIES
  LABELS "nightly;packet-h" TIMEOUT 900
  FAIL_REGULAR_EXPRESSION "FAIL")

# THE POSITIVE CONTROL, and this differential needs one more than most.
#
# Its PASS is the ABSENCE of a signal, produced by 91 generated compare lines
# nobody reads. One of them mistyped -- comparing v1 against v1, say -- would
# agree forever and look exactly like the green run above. "A detector that has
# not been shown to FIRE has not been tested."
#
# The mutant is a committed file under tests/mutants/, renamed so no source
# list can elaborate it by mistake, differing in ONE substantive line: the V2
# side of `px_valid_o`'s comparison is negated. The driver is the same binary
# with the polarity inverted -- WILL_FAIL means this test passes when the
# differential correctly reports a divergence that is not really there.
set(ZHAO_SHELL_PAIRED_MUT_SOURCES ${ZHAO_SHELL_V2_TOP_SOURCES})
foreach(shell_source IN LISTS ZHAO_SHELL_RTL)
  if(NOT "${shell_source}" IN_LIST ZHAO_SHELL_PAIRED_MUT_SOURCES)
    list(APPEND ZHAO_SHELL_PAIRED_MUT_SOURCES "${shell_source}")
  endif()
endforeach()
list(APPEND ZHAO_SHELL_PAIRED_MUT_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/mutants/zhao_shell_paired_diff_mutant.sv)

add_executable(test_shell_paired_diff_mutant
  shell/shell_paired_diff_directed.cpp)
target_compile_definitions(test_shell_paired_diff_mutant PRIVATE
  ZHAO_PAIRED_DIFF_MUTANT=1)
target_link_libraries(test_shell_paired_diff_mutant PRIVATE zhao_harness)
verilate(test_shell_paired_diff_mutant
  PREFIX Vzhao_shell_paired_diff
  TOP_MODULE zhao_shell_paired_diff_mut
  SOURCES ${ZHAO_SHELL_PAIRED_MUT_SOURCES})
add_test(NAME shell_paired_diff_mutant_control
  COMMAND test_shell_paired_diff_mutant)
set_tests_properties(shell_paired_diff_mutant_control PROPERTIES
  LABELS "nightly;mutant;packet-h" TIMEOUT 900
  WILL_FAIL TRUE)

add_test(NAME packet_h_paired_diff_mutant_fresh
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/tools/design/gen_shell_paired_diff.py
          --check --mutant)
set_tests_properties(packet_h_paired_diff_mutant_fresh PROPERTIES
  LABELS "fast;lint;packet-h" TIMEOUT 120)

# D3 Packet A: generated ten-bit characterization wrapper. The shell source
# pool remains byte-for-byte/order-for-order the existing ZHAO_SHELL_RTL list;
# only the generated wrapper and simulation-only monitor are appended.
add_test(NAME lint_shell_fit_top
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_shell_fit_top
                 ${ZHAO_SHELL_RTL}
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_fit_top.sv)
set_tests_properties(lint_shell_fit_top PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_shell_fit_smoke
               ${CMAKE_CURRENT_SOURCE_DIR}/shell/shell_fit_smoke.cpp)
verilate(test_shell_fit_smoke
  PREFIX Vzhao_shell_fit_smoke_tb
  TOP_MODULE zhao_shell_fit_smoke_tb
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_fit_top.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/generated/zhao_shell_fit_smoke_tb.sv)
add_test(NAME shell_fit_smoke COMMAND test_shell_fit_smoke)
set(SHELL_FIT_SMOKE_CONTROL_TESTS)
foreach(domain IN ITEMS gpu video audio)
  foreach(arm IN ITEMS edges activity wraps misr snapshot epoch capture serializer payload)
    set(test_name shell_fit_smoke_control_${domain}_${arm})
    add_test(NAME ${test_name} COMMAND test_shell_fit_smoke --control ${domain} ${arm})
    list(APPEND SHELL_FIT_SMOKE_CONTROL_TESTS ${test_name})
  endforeach()
endforeach()
foreach(fault IN ITEMS ring hps counter render guard sdr guard-verdict guard-early guard-late guard-extra hps-timing render-stability guard-verdict-extra guard-post-denial guard-preownership)
  set(test_name shell_fit_smoke_protocol_${fault})
  add_test(NAME ${test_name} COMMAND test_shell_fit_smoke --protocol-control ${fault})
  list(APPEND SHELL_FIT_SMOKE_CONTROL_TESTS ${test_name})
endforeach()
set_tests_properties(
  shell_fit_smoke ${SHELL_FIT_SMOKE_CONTROL_TESTS}
  PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

if(Python3_Interpreter_FOUND)
  add_test(NAME render_texture_packet_a
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_a.py -q)
  add_test(NAME texjoin_accounting_retirement
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_texjoin_accounting.py -q)
  add_test(NAME shell_fit_tools
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_shell_fit_tools.py -q)
  add_test(NAME shell_fit_preflight_fixtures
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_shell_fit_preflight.py -q)
  add_test(NAME shell_fit_generated_freshness
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_SOURCE_DIR}/tools/quartus/gen_shell_fit_top.py --check)
  add_test(NAME shell_fit_smoke_monitor_freshness
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_SOURCE_DIR}/tools/quartus/shell_fit_smoke.py --check)
  add_test(NAME shell_fit_elaboration_census
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_SOURCE_DIR}/tools/quartus/shell_elab_ports.py
                   --check-shell-fit
                   --repo-root ${CMAKE_SOURCE_DIR}
                   --wrapper ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_fit_top.sv
                   --manifest ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_fit_top.manifest.json
                   --work-dir ${CMAKE_CURRENT_BINARY_DIR}/shell-fit-elaboration
                   --verilator "${ZHAO_VERILATOR_LINT}"
                   --sources ${ZHAO_SHELL_RTL})
  set_tests_properties(
    render_texture_packet_a texjoin_accounting_retirement
    shell_fit_tools shell_fit_preflight_fixtures shell_fit_generated_freshness
    shell_fit_smoke_monitor_freshness shell_fit_elaboration_census
    PROPERTIES LABELS "fast;lint" TIMEOUT 120)
  # Packet A compiles and executes sixteen isolated detector models rather than
  # accepting lint-only failures; keep its CTest backstop above the shared lane.
  set_tests_properties(render_texture_packet_a PROPERTIES TIMEOUT 600)
  # MEASURED, not guessed: 51 tests in 176.9 s on this kit, unloaded, exiting 0.
  # The shared 120 s above was killing a PASSING gate, and a killed test reports
  # "***Timeout" -- which reads as "this accounting is broken" when all it meant
  # was "we did not wait". That is the same mistake tools/quartus/run_block_fit
  # .ps1's own timeout comment records twice over, one lane down.
  # 600 leaves real headroom rather than sitting just above the measurement,
  # because this lane regenerates zhao_prod_top three times and will grow with
  # the block count.
  #
  # AND IT SAT AT 300 UNTIL 2026-09-16, when it killed the gate again. The
  # paragraph above argues for 600 in its own last sentence; the value beside
  # it said 300. Nobody had to be careless for that -- the reasoning was
  # written, the number was not carried across, and nothing in the tree
  # compares a comment to the constant it justifies. It is this repository's
  # uncashed-cheque shape in four lines of CMake.
  #
  # RE-MEASURED the same day, idle machine, nothing else running:
  # 21 tests in 256.3 s, exit 0. Note the direction -- FEWER tests, and 45%
  # LONGER -- because this lane does not lint, it runs Verilator and then a
  # native compiler on each generated model, so its cost tracks the number of
  # elaborations rather than the number of test methods. 256 s against 300
  # is 17% of headroom, which is why it died the moment a Quartus fit took
  # two processors. 600 is 2.3x the idle measurement and survives that.
  set_tests_properties(texjoin_accounting_retirement PROPERTIES TIMEOUT 600)
  # Same lesson, same shared 120 s: MEASURED at 114-115 s unloaded, which passes
  # alone and is killed under -j 4. A budget that only holds on an idle machine
  # is a flake generator, and the report it produces ("***Timeout") says nothing
  # about the 148 checks it was running.
  set_tests_properties(shell_fit_preflight_fixtures PROPERTIES TIMEOUT 600)
  set_tests_properties(shell_fit_elaboration_census PROPERTIES
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}" TIMEOUT 300)
endif()

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

# THE FIRST TEST THAT DRAWS THROUGH zhao_shell_top (docket D19e, 2026-09-04).
# Until this file existed, `render_tri_valid_i` was driven to 1'b0 tree-wide
# and the shell's composition of geometry and raster -- the path D1 spent
# eleven rounds closing 100 MHz on -- was elaborated, fitted and timed but
# never once simulated.
#
# NOT a picture test: render_pipe_directed already drives zhao_geom_bin_pipe
# and the whole raster chain against zref and proves the arithmetic. What only
# this path can see is the SHELL'S OWN WIRING, which is invisible to every
# block-level test and to the fitter, which analyses delay and not meaning.
# D22 step 1's evidence: the SAME triangle drawn through the shell twice --
# once from precomputed edge equations, once from vertices through the composed
# GEOM.SETUP -- and required to produce an identical framebuffer.
# D22 STEP 2: the composed DEPTH path. GEOM.DEPTHQUANT turns w into the
# canonical invw24 the fragment pipe tests against, inside the composed design,
# instead of the bench hand-packing it into render_fill_word_i[31:8].
#
# The precomputed pass uses zref::depth_of_raw and NOT the DUT's own answer --
# feeding the DUT's value back would make both passes agree whatever DEPTHQUANT
# computed. And in depth mode the fill word's depth field is filled with all
# ones, so a shell that ignored depth_mode_i draws a visibly different picture
# rather than passing by coincidence.
# D22 STEP 3: GEOM.CLIP in front of SETUP. The bench stops supplying 2A and the
# scan box; CLIP computes them, and normalises winding on the way.
#
# The test draws BOTH windings. A step-3 test fed only counter-clockwise
# triangles exercises the flip zero times and passes with the whole
# normalisation path dead -- which is the failure GEOM.CLIP's own header warns
# about: a triangle "geometrically correct and shaded wrong, on exactly the
# back-facing half of the scene".
# D22 STEP 4: GEOM.PROJECT in front of CLIP. The step that joins the front end
# up -- PROJECT feeds CLIP the screen vertices AND the behind flags, and feeds
# DEPTHQUANT the w its own header insists on ("the quotient has already lost the
# precision reconstruction would need").
#
# The precomputed pass is built from zref::render::project_vertex, NOT from the
# DUT's own answer: feeding the DUT's value back would make both passes agree
# whatever PROJECT computed.
# D22 STEP 5: GEOM.ASSEMBLE names the three vertices. Steps 1-4 moved
# per-triangle ARITHMETIC backwards; this moves the triangle's SELECTION.
#
# The index stream is a PERMUTATION (2, 0, 3 out of a four-entry table with a
# decoy at index 1) on purpose. With the identity triplet (0, 1, 2) a shell
# that ignored ASSEMBLE entirely and used the first three vertices in order
# would draw the same picture and the check would be green with the block dead.
# D22 STEP 6, the last tread: GEOM.MESHFETCH reads the meshlet descriptor.
#
# The bench PLAYS three interfaces for it -- the memory guard, the beat stream
# carrying the descriptor bytes, and the cull service -- because MESHFETCH is
# the only zhao_guard_req_t client in the geometry subsystem. So this proves
# the DESCRIPTOR PATH inside the composed shell, and not the asset fetcher over
# GEOM.ASSET_POOL that D22 names as its remaining work.
#
# The sensitivity probe is the contract's own rule: a descriptor whose CRC does
# not check must emit NO meshlet, so nothing is drawn from it.
add_executable(test_shell_meshfetch_path_directed shell/shell_meshfetch_path_directed.cpp)
target_link_libraries(test_shell_meshfetch_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_meshfetch_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_meshfetch_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_meshfetch_path_directed COMMAND test_shell_meshfetch_path_directed)
set_tests_properties(shell_meshfetch_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_shell_assemble_path_directed shell/shell_assemble_path_directed.cpp)
target_link_libraries(test_shell_assemble_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_assemble_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_assemble_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_assemble_path_directed COMMAND test_shell_assemble_path_directed)
set_tests_properties(shell_assemble_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# D22 TREAD 7: GEOM.VDECODE. The bench stops supplying decoded coordinates and
# supplies the 32-byte format-0 RECORDS they came from. Labelled like its six
# siblings; note that each of these draws two full frames through the composed
# shell and costs minutes, which is why the TIMEOUT is 1200 and not a default.
add_executable(test_shell_vdecode_path_directed shell/shell_vdecode_path_directed.cpp)
target_link_libraries(test_shell_vdecode_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_vdecode_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_vdecode_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_vdecode_path_directed COMMAND test_shell_vdecode_path_directed)
set_tests_properties(shell_vdecode_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# D22 TREAD 8: GEOM.ASSETFETCH. The bench stops synthesising vertex records and
# supplies raw pool bytes; the fetcher reads the footprint and streams records.
add_executable(test_shell_assetfetch_path_directed shell/shell_assetfetch_path_directed.cpp)
target_link_libraries(test_shell_assetfetch_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_assetfetch_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_assetfetch_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_assetfetch_path_directed COMMAND test_shell_assetfetch_path_directed)
set_tests_properties(shell_assetfetch_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# D22 TREAD 9: the u8 INDEX STREAM. ASSETFETCH was already reading the index
# run and discarding it; this connects the port GEOM.ASSEMBLE was asking on.
add_executable(test_shell_indexfetch_path_directed shell/shell_indexfetch_path_directed.cpp)
target_link_libraries(test_shell_indexfetch_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_indexfetch_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_indexfetch_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_indexfetch_path_directed COMMAND test_shell_indexfetch_path_directed)
set_tests_properties(shell_indexfetch_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# D22 tread 10: the geometry fetch reads REAL memory through the shell's own
# guard, arbiter and SDRAM controller. Two full frames through a cycle-accurate
# DRAM model, so it is nightly rather than fast.
add_executable(test_shell_realmem_path_directed shell/shell_realmem_path_directed.cpp)
target_link_libraries(test_shell_realmem_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_realmem_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_realmem_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_realmem_path_directed COMMAND test_shell_realmem_path_directed)
set_tests_properties(shell_realmem_path_directed PROPERTIES LABELS "nightly" TIMEOUT 2400)

add_executable(test_shell_project_path_directed shell/shell_project_path_directed.cpp)
target_link_libraries(test_shell_project_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_project_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry
  ${CMAKE_SOURCE_DIR}/reference/src)
verilate(test_shell_project_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_project_path_directed COMMAND test_shell_project_path_directed)
set_tests_properties(shell_project_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_shell_clip_path_directed shell/shell_clip_path_directed.cpp)
target_link_libraries(test_shell_clip_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_clip_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_shell_clip_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_clip_path_directed COMMAND test_shell_clip_path_directed)
set_tests_properties(shell_clip_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

add_executable(test_shell_depth_path_directed shell/shell_depth_path_directed.cpp)
target_link_libraries(test_shell_depth_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_depth_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_shell_depth_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_depth_path_directed COMMAND test_shell_depth_path_directed)
set_tests_properties(shell_depth_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_shell_setup_path_directed shell/shell_setup_path_directed.cpp)
target_link_libraries(test_shell_setup_path_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_setup_path_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_shell_setup_path_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
# D22 step 1's composed evidence, and it PASSES: the same triangle drawn twice
# through the same shell -- once from precomputed edge equations, once from
# vertices with the coefficients CLEARED so a shell ignoring setup_mode would
# draw nothing -- produces a byte-identical framebuffer, 3,328 non-zero words
# each. Registered only now that it earns it.
add_test(NAME shell_setup_path_directed COMMAND test_shell_setup_path_directed)
set_tests_properties(shell_setup_path_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_executable(test_shell_draw_directed shell/shell_draw_directed.cpp)
target_link_libraries(test_shell_draw_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_shell_draw_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shell
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry)
verilate(test_shell_draw_directed
  PREFIX Vtb_zhao_shell TOP_MODULE tb_zhao_shell
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
               ${CMAKE_SOURCE_DIR}/sim/models
  SOURCES ${ZHAO_SHELL_RTL}
          ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/shell/tb_zhao_shell.sv)
add_test(NAME shell_draw_directed COMMAND test_shell_draw_directed)
set_tests_properties(shell_draw_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THE PHASE-2 GATE: two controllers move independent 2D markers in Duo,
# 600 capture-exact marker frames (1,202 displayed frames at the machine's
# sustainable cadence) on the full machine â€” an executable ctest. ~30 min
# wall, so the full gate carries the gate+nightly labels and a 40-marker
# cut of the SAME binary and assertions guards every commit in fast.
add_test(NAME shell_duo_markers COMMAND demo_duo_markers)
set_tests_properties(shell_duo_markers PROPERTIES
  LABELS "gate;nightly" TIMEOUT 5400)
add_test(NAME shell_duo_markers_fast COMMAND demo_duo_markers --frames 40)
set_tests_properties(shell_duo_markers_fast PROPERTIES
  LABELS "fast" TIMEOUT 1800)

# nightly soak: 10,000 DISPLAYED frames (5,000 marker packets) with PCG
# jitter on publish timing, pads and rumble at the stress corners â€” the
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
# Layer F â€” the 64x64 {tag u8, strength u8} per-patch surface sheet of charter
# 12 â€” and the Scar Scribe stamp engine that is its only writer
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

# ---- SURFACE.DISPATCH (owner ruling R45, core entry I30) --------------------
# The world->patch law, differenced against `zhao_terrain_place`'s placement
# restated in C++ -- NOT against this block's own RTL. Negative coordinates
# FLOOR, which is the trap `zhao_terrain_heighttap`'s header names by example.
add_executable(test_surface_dispatch_directed surface/surface_dispatch_directed.cpp)
target_link_libraries(test_surface_dispatch_directed PRIVATE zhao_harness)
verilate(test_surface_dispatch_directed
  PREFIX Vzhao_surface_dispatch
  TOP_MODULE zhao_surface_dispatch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_dispatch.sv)
add_test(NAME surface_dispatch_directed COMMAND test_surface_dispatch_directed)
set_tests_properties(surface_dispatch_directed PROPERTIES LABELS "fast;surface" TIMEOUT 300)

add_test(NAME lint_zhao_surface_dispatch
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_surface_dispatch
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_dispatch.sv)
set_tests_properties(lint_zhao_surface_dispatch PROPERTIES LABELS "lint;fast")

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
# zhao_surface_stamp reduces this module's output to one boolean â€”
# `covered = !(d2 > r_outer2 || d2 < r_inner2)` â€” and all three of those terms
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

# formal: the surface blend, proved TOTAL rather than bounded â€” the harness is
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
# EXECUTED reference â€” `zref::terrain::bake_dig` then
# `zref::terrain::apply_breach_law` â€” rather than a second implementation of
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

# ---------------------------------------------------------------------------
# THE SAME SUITE AGAINST V2, WHICH IS THE BLOCK A COMPOSITION MAY INSTANTIATE
#
# Added 2026-09-20 (terrain6), and the reason is a defect in the EVIDENCE, not
# in the RTL. `design/console_inventory.yml` records `zhao_terrain_bake:
# superseded_by: zhao_terrain_bake_v2`, so under the owner's 2026-09-19
# only-the-latest-version ruling v2 is the block core entry I32 must one day
# compose. `design/prod_manifest.yml` quotes it at "267/267 against the zref
# oracle with a committed mutant seen to fail".
#
# NO CMAKE TARGET BUILT IT. The suite source carries the recipe -- three -D
# flags, in its own header comment -- and the v2 file's header names
# `terrain_bake_v2_directed` as what holds it to the oracle, and nothing in
# this file ever produced that target. So the 267/267 was UNREPRODUCIBLE FROM
# THE TREE, which is exactly the fault `design/blocks.yml` already records
# against TERRAIN.NORMALMAP ("no CMake target compiled the named suite") --
# twice now, in the same subsystem, found by two different passes.
#
# It reads in the flattering direction, which is why nobody looked: a number in
# a manifest with no way to re-run it is indistinguishable from a number that
# was re-run yesterday.
#
# The rate bounds are V2's OWN declared price (24 covered / 5 uncovered against
# v1's 19 / 2 -- the sequenced shared multiplier), so a regression past its own
# declaration fails loudly.
# `zhao_terrain_stampdepth.sv` joined this list 2026-09-20 under OWNER RULING
# R194: v2 instantiated NO child before that, so every list naming only the
# parent was complete and none of them is any more. Verilator answers a missing
# source with MODMISSING, which is loud; Quartus answers it hours into a fit.
set(ZHAO_TERRAIN_BAKE_V2_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake_v2.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_stampdepth.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bake_delta.sv)

add_executable(test_terrain_bake_v2_directed terrain/terrain_bake_directed.cpp)
target_link_libraries(test_terrain_bake_v2_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_v2_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
target_compile_definitions(test_terrain_bake_v2_directed PRIVATE
  ZHAO_BAKE_DUT_HEADER="Vzhao_terrain_bake_v2.h"
  ZHAO_BAKE_DUT_CLASS=Vzhao_terrain_bake_v2
  ZHAO_BAKE_SUITE_NAME="terrain_bake_v2_directed"
  ZHAO_BAKE_RATE_COVERED_MAX=25.0
  ZHAO_BAKE_RATE_UNCOVERED_MAX=6.0)
verilate(test_terrain_bake_v2_directed
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2
  SOURCES ${ZHAO_TERRAIN_BAKE_V2_SV})
add_test(NAME terrain_bake_v2_directed COMMAND test_terrain_bake_v2_directed)
set_tests_properties(terrain_bake_v2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# OPTION A's SECOND DEPTH MODE -- the per-vertex layer-F dig, added 2026-09-20
# under OWNER RULING R194 ("Shipped is fine. Slightly different but not off.").
#
# The target above holds the PARAMETRIC DISC to `zref::terrain::bake_dig`, 267
# checks, and must go on passing UNCHANGED -- that is the evidence the new mode
# is additive rather than a rewrite. This one holds the sheet mode to
# `zref::terrain::stamp_depth_at_vertex` and fires `sheet_vertices_dug_o`, with
# a disc record as the negative control in the same executable.
#
# It does NOT share `bake_dev.hpp`: that driver is a template over BOTH v1 and
# v2 and naming a port only v2 has would break the v1 targets, so this file
# carries its own driver deliberately.
add_executable(test_terrain_bake_v2_sheet_directed terrain/terrain_bake_v2_sheet_directed.cpp)
target_link_libraries(test_terrain_bake_v2_sheet_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_v2_sheet_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_bake_v2_sheet_directed
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2
  VERILATOR_ARGS -Wall
  SOURCES ${ZHAO_TERRAIN_BAKE_V2_SV})
add_test(NAME terrain_bake_v2_sheet_directed COMMAND test_terrain_bake_v2_sheet_directed)
set_tests_properties(terrain_bake_v2_sheet_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# OWNER RULING R231 -- THE DEPTH LAW IS A DELTA, AND THE SAME STAMP ISSUED
# TWICE MUST DIG ONCE. Added 2026-09-21 (deltalaw).
#
# The target above holds the sheet mode against the ABSOLUTE oracle and must go
# on passing unchanged -- that is the evidence the delta is a GENERALISATION
# and not a rewrite, and it holds because `kStampDepthTable[0]` is 0, which is
# now static_asserted in zref_terrain_page.hpp.
#
# This one is the instrument the defect proved was missing. Nine counters on
# the seam and six on the bake all read correctly while the crater doubled,
# because every one of them measures the ANSWER and the fault was in the
# QUESTION. So it reads no counter to decide anything: it bakes, carries the
# scar forward as the page would, bakes again, and DIFFERENCES against zref --
# R215's shape, which caught three PAGEIO defects no counter could see.
#
# Its case 3 is the positive control and is not optional: it re-runs the second
# bake with `before` forced to zero and REQUIRES the crater to double. A test
# that passes on an inert machine looks exactly like one that passes on a
# correct machine, and this is what separates them.
add_executable(test_bake_delta_idempotence_directed terrain/bake_delta_idempotence_directed.cpp)
target_link_libraries(test_bake_delta_idempotence_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_bake_delta_idempotence_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_bake_delta_idempotence_directed
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2
  VERILATOR_ARGS -Wall
  SOURCES ${ZHAO_TERRAIN_BAKE_V2_SV})
add_test(NAME bake_delta_idempotence_directed COMMAND test_bake_delta_idempotence_directed)
set_tests_properties(bake_delta_idempotence_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
# ---------------------------------------------------------------------------
# THE POSITIVE CONTROL FOR V2'S MEETS-PLANE ROW WINDOW -- ruling R93.
#
# `tests/mutants/zhao_terrain_bake_v2_mutant.sv` was committed 2026-09-09 and
# refreshed 2026-09-20 and NO TARGET EVER ELABORATED IT: one grep hit in
# tests/, the file itself. R93 -- "an uninstantiated MODULE costs the fitter
# nothing, but an unrun MUTANT is cited as evidence" -- gives the next terrain
# packet a driver or retirement. This is the driver (terrain8, 2026-09-20).
#
# Verilated under the GOLDEN's prefix so `bake_dev.hpp` and the control
# compile unchanged; in this executable the class Vzhao_terrain_bake_v2 IS the
# mutant. Same arrangement as the forge cliff-RAM controls above.
#
# INVERTED POLARITY: it passes when the apply_breach_law comparison FAILS.
# The CORRECT behaviour is asserted by `terrain_bake_v2_directed` against the
# real block -- two separate targets, per CLAUDE.md's "do not write a test that
# asserts the bug".
add_executable(test_terrain_bake_v2_mutant_control terrain/terrain_bake_v2_mutant_control.cpp)
target_link_libraries(test_terrain_bake_v2_mutant_control PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_v2_mutant_control PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_bake_v2_mutant_control
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2_mutant
  # The mutant is a COPY of the parent and instantiates the same real child --
  # the layer-F reader is not what is mutated and must not be duplicated.
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_bake_v2_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_stampdepth.sv)
add_test(NAME terrain_bake_v2_mutant_control COMMAND test_terrain_bake_v2_mutant_control)
set_tests_properties(terrain_bake_v2_mutant_control PROPERTIES
                     LABELS "fast;nightly;mutant" TIMEOUT 600)

# THE NEGATIVE CONTROL: the SAME driver against the REAL block, WILL_FAIL.
#
# CLAUDE.md's rule for every macro-selected mutant -- "compile the mutant with
# the macro UNDEFINED and confirm the output differs" -- applied to a
# prefix-selected one. Without this, "the control passed" is only evidence that
# the executable ran; it says nothing about WHICH module it ran, and two
# combiner mutants once passed while measuring unmutated production.
#
# It is not a test that asserts the bug. It asserts that the real block does
# NOT present the fault, by the only route that also proves the driver can tell
# the difference -- and it is a live regression guard in its own right: if
# production ever acquires the row-window lag, this driver SUCCEEDS on it and
# WILL_FAIL turns that into a red.
add_executable(test_terrain_bake_v2_mutant_negative terrain/terrain_bake_v2_mutant_control.cpp)
target_link_libraries(test_terrain_bake_v2_mutant_negative PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_bake_v2_mutant_negative PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/terrain)
verilate(test_terrain_bake_v2_mutant_negative
  PREFIX Vzhao_terrain_bake_v2
  TOP_MODULE zhao_terrain_bake_v2
  SOURCES ${ZHAO_TERRAIN_BAKE_V2_SV})
add_test(NAME terrain_bake_v2_mutant_negative COMMAND test_terrain_bake_v2_mutant_negative)
set_tests_properties(terrain_bake_v2_mutant_negative PROPERTIES
                     WILL_FAIL TRUE LABELS "fast;nightly;mutant" TIMEOUT 600)

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
  # THE COVER TASK ONLY, AND THE PROPERTY FILE ASKED FOR THIS. Its own header
  # says "NOT in the automated lane", and design/formal_runs.yml records the
  # reason: the bmc task ran 10.7 HOURS in "waiting for solver" before it was
  # killed, and terminated without status on a shorter attempt. It was
  # registered here anyway with TIMEOUT 2100, so it manufactured a timeout on
  # every nightly -- a test wired to fail every run, which is noise that hides
  # real failures.
  #
  # That is the exact failure the MEM lane's own comment warns against for
  # mem_sdram_refresh_bound ("putting it in the routine lane would only
  # manufacture timeouts that look like failures"), and that property is
  # deliberately NOT registered for it. This one was, and the contradiction
  # stood between a .sby that said "not automated" and a CMakeLists that
  # automated it.
  #
  # DROPPING THE LANE ENTIRELY WOULD HAVE THROWN SOMETHING AWAY. The cover
  # task passes in ~3 s and reaches every corner it claims, INCLUDING
  # c_sat_reachable -- it is what stops the bmc task's absence being mistaken
  # for "nothing here needed proving". Running the half that finishes is better
  # than running none of it or all of it.
  #
  # The bmc task stays runnable by hand and the registry keeps it BANKED:
  #     cd tests/formal && sby -f --prefix <dir> terrain_bake_delta.sby bmc
  # TERRAIN.BAKE is SPECIFIED and cites no formal evidence, so no maturity
  # claim rests on the half that does not run.
  set(FORMAL_TASKS "cover")
  configure_file(formal/mem_formal_lane.cmake.in
                 formal/terrain_bake_delta_lane.cmake @ONLY)
  unset(FORMAL_TASKS)
  set(zhao_formal_script
      "${CMAKE_CURRENT_BINARY_DIR}/formal/terrain_bake_delta_lane.cmake")
  string(REPLACE "/" "\\" zhao_formal_script "${zhao_formal_script}")
  add_test(NAME formal_terrain_bake_delta
           COMMAND "${CMAKE_COMMAND}" -P "${zhao_formal_script}")
  # 300, not 2100: the cover task measures ~3 s and the old budget was sized for
  # a bmc task that is no longer run. A timeout two orders above the work is a
  # timeout that will never fire for the right reason.
  set_tests_properties(formal_terrain_bake_delta PROPERTIES
    LABELS "formal;nightly"
    RUN_SERIAL TRUE
    TIMEOUT 300
    SKIP_REGULAR_EXPRESSION "formal lane SKIPPED")
endif()

# ---------------------------------------------------------------------------
# TERRAIN.VELOCITY (phase 7, ZH-039)
#
# The Â§4.2 velocity lattice: earth out-lane 1 accumulated at lattice vertices
# and baked back to height16. The oracle is cross-checked against the EXECUTED
# reference (`compose_lattice`'s recorded velocity samples, grouped by vertex
# in command order) before the RTL is compared to it, so "RTL == oracle" means
# "RTL == the velocity lane the software console already records". The block
# owns its own 33x33 sweep, so the tests drive it as a MEMORY answering the
# address it asks for â€” which is what makes the z-then-x scan order a checkable
# fact instead of an assumption.
set(ZHAO_TERRAIN_VELOCITY_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
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
# owns the Â§9.1 field list and decides the closed-interval footprint test once;
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
# The global token guard and the charter Â§9 Duo fairness split. The grant is
# COMBINATIONAL because GEOM.BINNER's law E already committed to that seam in
# phase 6 ("`tok_grant_i` is sampled on that same edge"); the ledger's
# `latency: fixed:1` is honoured by the registered `token_denial`, and the
# directed lane MEASURES both rather than asserting them. The oracle is a first
# implementation, not a view onto an executed reference â€” nothing in
# reference/src allocates tokens â€” so the contract carries every law it makes.
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
# (temporal induction â€” the guarantee holds for ALL time, not for a bounded
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
# The per-camera screen-error policy and per-view degradation. Charter Â§9
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

# The per-camera PIXEL-ERROR THRESHOLD (owner ruling R26, scheduled by R68).
# A SEPARATE suite rather than more cases in measure_governor_directed, for the
# reason part_state_capacity_backstop is separate: that suite's check count is
# quoted as evidence, and a number that moves whenever somebody adds a case is
# a number nobody can use. It links zhao_zref and runs the published threshold
# through zref::creature::lod_raw -- the law zhao_geom_lod implements -- so the
# number means something rather than merely being self-consistent.
add_executable(test_measure_governor_thresh measure/measure_governor_thresh.cpp)
target_link_libraries(test_measure_governor_thresh PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_governor_thresh PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_governor_thresh
  PREFIX Vzhao_measure_governor
  TOP_MODULE zhao_measure_governor
  SOURCES ${ZHAO_MEASURE_GOVERNOR_SV})
add_test(NAME measure_governor_thresh COMMAND test_measure_governor_thresh)
set_tests_properties(measure_governor_thresh PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

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

# ---------------------------------------------------------------------------
# MEASURE.GOVERNOR's two INPUT-SIDE producers (post3, 2026-09-20).
#
# Both exist because `zhao_console_core.sv`'s I18 entry names them as the
# reason the governor cannot be composed. Neither is composed yet -- the
# governor's OUTPUT side is still the blocker (see FINDINGS-post3.md) -- so
# these are built and tested standalone, which is CLAUDE.md's "build new blocks
# standalone, wire several in at once".
# ---------------------------------------------------------------------------
set(ZHAO_MEASURE_STARVE_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_starve.sv)

add_executable(test_measure_starve_directed measure/measure_starve_directed.cpp)
target_link_libraries(test_measure_starve_directed PRIVATE zhao_harness)
target_include_directories(test_measure_starve_directed PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_starve_directed
  PREFIX Vzhao_measure_starve
  TOP_MODULE zhao_measure_starve
  SOURCES ${ZHAO_MEASURE_STARVE_SV})
add_test(NAME measure_starve_directed COMMAND test_measure_starve_directed)
set_tests_properties(measure_starve_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_measure_starve
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_measure_starve
                 ${ZHAO_MEASURE_STARVE_SV})
set_tests_properties(lint_measure_starve PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The elaboration guard, FIRED. `--lint-only` does not run `initial` blocks
# (CLAUDE.md), so a clean lint says nothing whatever about this check; the only
# way to know it works is to break the parameter on purpose and watch it stop
# the build. Run through the expect-fatal wrapper for the reason
# early_desc_layout_guard records: Verilator's abort path does not return under
# ctest's pipes on Windows.
add_executable(test_measure_starve_mask_guard measure/measure_starve_directed.cpp)
target_link_libraries(test_measure_starve_mask_guard PRIVATE zhao_harness)
target_include_directories(test_measure_starve_mask_guard PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_starve_mask_guard
  PREFIX Vzhao_measure_starve
  TOP_MODULE zhao_measure_starve
  SOURCES ${ZHAO_MEASURE_STARVE_SV}
  VERILATOR_ARGS -GSTARVE_REASON_MASK=0)
add_test(NAME measure_starve_mask_guard
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_measure_starve_mask_guard>
    -DEXPECT=STARVE_REASON_MASK\ is\ zero
    -DRUN_TIMEOUT=10
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(measure_starve_mask_guard PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120)

# The wrapper's own negative control, exactly as early_desc does it: the
# wrapper reports a pass on a process that never exits, so the branch that must
# be shown to work is the one where the diagnostic is ABSENT. Pointed at the
# healthy build, which runs the same module with a legal mask.
add_test(NAME measure_starve_mask_guard_wrapper_control
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_measure_starve_directed>
    -DEXPECT=STARVE_REASON_MASK\ is\ zero
    -DRUN_TIMEOUT=60
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(measure_starve_mask_guard_wrapper_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120 WILL_FAIL TRUE)

set(ZHAO_VIEW_PROJQ88_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_projq88.sv)

add_executable(test_view_projq88_directed measure/view_projq88_directed.cpp)
target_link_libraries(test_view_projq88_directed PRIVATE zhao_harness)
target_include_directories(test_view_projq88_directed PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_view_projq88_directed
  PREFIX Vzhao_view_projq88
  TOP_MODULE zhao_view_projq88
  SOURCES ${ZHAO_VIEW_PROJQ88_SV})
add_test(NAME view_projq88_directed COMMAND test_view_projq88_directed)
set_tests_properties(view_projq88_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_view_projq88
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_view_projq88
                 ${ZHAO_VIEW_PROJQ88_SV})
set_tests_properties(lint_view_projq88 PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.DEPTHQUANT â€” w to canonical invw24 under the selected depth profile.
# Checked against `zref::depth_of_raw`, the ratified law the golden captures
# already pin, rather than against a second implementation of it.
add_executable(test_geom_depthquant_directed geometry/geom_depthquant_directed.cpp)
target_link_libraries(test_geom_depthquant_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_depthquant_directed
  PREFIX Vzhao_geom_depthquant
  TOP_MODULE zhao_geom_depthquant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv)
add_test(NAME geom_depthquant_directed COMMAND test_geom_depthquant_directed)
set_tests_properties(geom_depthquant_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# GEOM.MESHFETCH -- the ORACLE, with no DUT yet.
#
# zhao_geom_meshfetch.sv does not exist; compose_order.py puts this block at
# position 1 of the declared geometry order with nothing built behind it, which
# is why DOCKET D22 names it as the blocker for the whole front end. Maturity
# advances one rung at a time, so this is REFERENCE_COMPLETE evidence: when the
# RTL lands it gains a DUT in this same file and these cases become the
# differential. No verilate() call because there is nothing to verilate.
add_executable(test_geom_meshfetch_directed geometry/geom_meshfetch_directed.cpp)
target_link_libraries(test_geom_meshfetch_directed PRIVATE zhao_harness zhao_zref)
add_test(NAME geom_meshfetch_directed COMMAND test_geom_meshfetch_directed)
set_tests_properties(geom_meshfetch_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# GEOM.ASSETFETCH -- the oracle's EDGES, not a sweep.
# design/contracts/GEOM.ASSETFETCH.md. plan() is short enough that a random
# sweep would pass first try and prove nothing, so every case here is a
# boundary a plausible wrong implementation sits on the far side of: 64 vs 65
# vertices, a footprint ending on the pool's last byte, an offset that wraps
# 32 bits (the guard's blit-wrap defect in a new place), and a record stream
# starting mid-line so the beat count is not bytes/64. No verilate() call --
# the RTL is not written yet, and this is the REFERENCE_COMPLETE evidence it
# will become the differential for.
add_executable(test_assetfetch_directed geometry/assetfetch_directed.cpp)
target_link_libraries(test_assetfetch_directed PRIVATE zhao_harness zhao_zref)
add_test(NAME assetfetch_directed COMMAND test_assetfetch_directed)
set_tests_properties(assetfetch_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# GEOM.ASSETFETCH RTL against zref::assetfetch. The testbench PLAYS MEM.GUARD
# rather than instantiating it -- that block has its own differential and a
# formal no-escape proof, and instantiating it here would re-prove the region
# check while hiding whether THIS block asked for the right addresses. The
# played guard is deliberately STRICTER than the real one: it fails the test on
# any request that is not an aligned 64-byte read inside the pool, because a
# denial is easy to mistake for correct behaviour when the block should never
# have asked. tb_assetfetch.sv flattens the struct ports so the harness never
# guesses at Verilator's packing.
add_executable(test_assetfetch_rtl_directed geometry/assetfetch_rtl_directed.cpp)
target_link_libraries(test_assetfetch_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_assetfetch_rtl_directed
  PREFIX Vtb_assetfetch
  TOP_MODULE tb_assetfetch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_assetfetch.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_assetfetch.sv)
add_test(NAME assetfetch_rtl_directed COMMAND test_assetfetch_rtl_directed)
set_tests_properties(assetfetch_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# GEOM.MEM.ADAPTER -- two logical geometry requesters over the ONE permitted
# ENGINE1 client (owner recovery brief section 12). The properties under test are
# ownership ones: whose beats these are, whose verdict that was, and who is
# starved -- plus the two burst scales, 32 and 64 bytes, against a downstream
# generator that returns a constant eight words either way.
add_executable(test_geom_mem_adapter_directed geometry/geom_mem_adapter_directed.cpp)
target_link_libraries(test_geom_mem_adapter_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_mem_adapter_directed
  PREFIX Vtb_geom_mem_adapter
  TOP_MODULE tb_geom_mem_adapter
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mem_adapter.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_mem_adapter.sv)
add_test(NAME geom_mem_adapter_directed COMMAND test_geom_mem_adapter_directed)
set_tests_properties(geom_mem_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# MEM.SHARE at N=3 -- the third ENGINE1 reader (MATERIAL.RESOLVE's record
# fetch, core material seam 2). The share's body is `zhao_mem_share_n`; the
# test above is its N=2 proof through `zhao_mem_share2`/the adapter. This one
# proves the N=3 round-robin bound (N-1), ownership by value, the guard's
# two-cycle law with a LATE verdict, denial routing, trusted identity and
# read-only from every index, and fires SHORT/LONG/UNOWNED by stimulus.
add_executable(test_mem_share_n_directed memory/mem_share_n_directed.cpp)
target_link_libraries(test_mem_share_n_directed PRIVATE zhao_harness)
verilate(test_mem_share_n_directed
  PREFIX Vtb_mem_share_n
  TOP_MODULE tb_mem_share_n
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_mem_share_n.sv)
add_test(NAME mem_share_n_directed COMMAND test_mem_share_n_directed)
set_tests_properties(mem_share_n_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# MEM.SHARE with WRITERS -- the TERRAIN.BUILD socket's upstream share (core
# entry I26): MEM.UPLOAD and TERRAIN.PAGELOADER write and the terrain read share
# reads, all on VRAM slot 6. Proves write-data order across writers, per-
# requester retirement attribution, refusal releasing the channel, identity,
# ledger backpressure, and FIRES retire_unowned / wbeat_unowned by stimulus.
add_executable(test_mem_share_wr_directed memory/mem_share_wr_directed.cpp)
target_link_libraries(test_mem_share_wr_directed PRIVATE zhao_harness)
verilate(test_mem_share_wr_directed
  PREFIX Vtb_mem_share_wr
  TOP_MODULE tb_mem_share_wr
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share_wr.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_mem_share_wr.sv)
add_test(NAME mem_share_wr_directed COMMAND test_mem_share_wr_directed)
set_tests_properties(mem_share_wr_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_test(NAME lint_zhao_mem_share_wr
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_mem_share_wr
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share_wr.sv)
set_tests_properties(lint_zhao_mem_share_wr PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
foreach(_zhao_share_n 3 5)
  add_test(NAME lint_zhao_mem_share_n${_zhao_share_n}
           COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                   --top-module zhao_mem_share_n -GN=${_zhao_share_n}
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                   ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv)
  set_tests_properties(lint_zhao_mem_share_n${_zhao_share_n} PROPERTIES
    LABELS "lint;fast"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()
# GEOM.ASSETFETCH -- randomized DIFFERENTIAL. Unlike the meshfetch random file
# there IS something to differ against here, so this compares the RTL to the
# oracle over 240 meshlets rather than checking properties.
# What it buys that the directed files cannot: the block RE-ARMING (every
# failure mode of a single-buffered fetcher is state that survives one meshlet
# into the next), backpressure on the guard AND both service ports, and
# counters that accumulate over a long run. The generator is biased small on
# purpose -- uniform 0..64 draws would almost never produce the 0- and 1-vertex
# cases where an off-by-one in the stream's end condition lives -- and it
# asserts that it actually produced the mix it claims.
add_executable(test_assetfetch_random geometry/assetfetch_random.cpp)
target_link_libraries(test_assetfetch_random PRIVATE zhao_harness zhao_zref)
verilate(test_assetfetch_random
  PREFIX Vtb_assetfetch
  TOP_MODULE tb_assetfetch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_assetfetch.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_assetfetch.sv)
add_test(NAME assetfetch_random COMMAND test_assetfetch_random)
set_tests_properties(assetfetch_random PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# GEOM.MESHFETCH -- randomized PROPERTIES, not a differential.
# There is one implementation, so there is nothing to differ against yet. When
# the RTL lands the DUT goes in beside these generators and the properties
# become the differential's coverage guard.
add_executable(test_geom_meshfetch_random geometry/geom_meshfetch_random.cpp)
target_link_libraries(test_geom_meshfetch_random PRIVATE zhao_harness zhao_zref)
add_test(NAME geom_meshfetch_random COMMAND test_geom_meshfetch_random)
set_tests_properties(geom_meshfetch_random PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# GEOM.MESHFETCH RTL against zref::MeshFetch. The testbench PLAYS the cull
# service rather than instantiating zhao_geom_cull: that block has its own
# differential, and instantiating it here would re-prove the frustum test while
# hiding whether THIS block presented the right sphere to it.
add_executable(test_geom_meshfetch_rtl_directed geometry/geom_meshfetch_rtl_directed.cpp)
target_link_libraries(test_geom_meshfetch_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_meshfetch_rtl_directed
  PREFIX Vzhao_geom_meshfetch
  TOP_MODULE zhao_geom_meshfetch
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_meshfetch.sv)
add_test(NAME geom_meshfetch_rtl_directed COMMAND test_geom_meshfetch_rtl_directed)
# GEOM.MESHFETCH's DESCRIPTOR CRC WALKER -- closes zhao_console_core entry I37.
# `zhao_geom_desc_crc` runs the one `zhao_crc32c_fold` implementation across
# the eight returning beats and presents the verdict on the last one. The test
# differences it against the generated `zhao_abi::zhao_crc32c` ITSELF (the
# call zref_meshfetch.hpp:139 refuses on) and fires all three counters with
# legal stimulus: 200 sealed descriptors, all 512 single-bit flips, seven
# short bursts and a long one.
add_executable(test_geom_desc_crc_directed geometry/geom_desc_crc_directed.cpp)
target_link_libraries(test_geom_desc_crc_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_desc_crc_directed
  PREFIX Vzhao_geom_desc_crc
  TOP_MODULE zhao_geom_desc_crc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_desc_crc.sv)
add_test(NAME geom_desc_crc_directed COMMAND test_geom_desc_crc_directed)
set_tests_properties(geom_desc_crc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# -Wall, and warnings FATAL: `verilate()` does not pass -Wall.
add_test(NAME lint_geom_desc_crc
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_desc_crc
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_desc_crc.sv)
set_tests_properties(lint_geom_desc_crc PROPERTIES
  LABELS "fast;nightly" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.WARP's PER-DRAW DESCRIPTOR BOOK (packet WARPCOMP, 2026-09-22). One file,
# no dependencies: directive 7.1's "compact descriptor cookie" store, four
# entries with a 5-bit allocation stamp so the ring's own overwrite is DETECTED
# rather than prevented. Every counter in it is reached by LEGAL stimulus --
# five warped draws and a read of the first one's cookie -- so no mutant is
# owed, and `stale_o` is discriminated in BOTH directions inside one trace.
add_executable(test_geom_warpbook_directed geometry/geom_warpbook_directed.cpp)
target_link_libraries(test_geom_warpbook_directed PRIVATE zhao_harness)
verilate(test_geom_warpbook_directed
  PREFIX Vzhao_geom_warpbook
  TOP_MODULE zhao_geom_warpbook
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warpbook.sv)
add_test(NAME geom_warpbook_directed COMMAND test_geom_warpbook_directed)
set_tests_properties(geom_warpbook_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# -Wall, and warnings FATAL: `verilate()` does not pass -Wall.
add_test(NAME lint_geom_warpbook
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_warpbook
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warpbook.sv)
set_tests_properties(lint_geom_warpbook PROPERTIES
  LABELS "fast;nightly" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.REPLAY -- the geometry replay customer (core entries I11/I12/I24/I38/I39).
# The arena and the vertex-attribute store are modelled with the arena's
# contract. Since owner ruling R31 (2026-09-19) it computes no depth: GEOM.VATTR
# answers invw24 with the vertex's attributes, so no reciprocal is in its closure.
set(ZHAO_GEOM_REPLAY_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_replay.sv)
add_executable(test_geom_replay_directed geometry/geom_replay_directed.cpp)
target_link_libraries(test_geom_replay_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_replay_directed
  PREFIX Vzhao_geom_replay
  TOP_MODULE zhao_geom_replay
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_GEOM_REPLAY_SOURCES})
add_test(NAME geom_replay_directed COMMAND test_geom_replay_directed)
set_tests_properties(geom_replay_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_test(NAME lint_geom_replay
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_replay ${ZHAO_GEOM_REPLAY_SOURCES})
set_tests_properties(lint_geom_replay PROPERTIES
  LABELS "fast;nightly" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.VATTR -- the vertex-attribute store and its writer (core entry I46, owner
# rulings R11 and R31): invw24 per landed vertex on the streaming DEPTHQUANT,
# u/v_over_w by zref::geom_over_w, the lit colour, alpha. Every row differenced
# against the reference; every counter fired by stimulus; case H measures rate.
set(ZHAO_GEOM_VATTR_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_vattr.sv)
add_executable(test_geom_vattr_directed geometry/geom_vattr_directed.cpp)
target_link_libraries(test_geom_vattr_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_vattr_directed
  PREFIX Vzhao_geom_vattr
  TOP_MODULE zhao_geom_vattr
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_GEOM_VATTR_SOURCES})
add_test(NAME geom_vattr_directed COMMAND test_geom_vattr_directed)
set_tests_properties(geom_vattr_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_test(NAME lint_geom_vattr
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_vattr ${ZHAO_GEOM_VATTR_SOURCES})
set_tests_properties(lint_geom_vattr PROPERTIES
  LABELS "fast;nightly" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.DEPTHQUANT's TAGGED STREAM -- the same law on a schedule that keeps many
# vertices in flight, answers completing out of order (owner ruling R31).
add_executable(test_geom_depthquant_stream_directed geometry/geom_depthquant_stream_directed.cpp)
target_link_libraries(test_geom_depthquant_stream_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_depthquant_stream_directed
  PREFIX Vzhao_geom_depthquant_stream
  TOP_MODULE zhao_geom_depthquant_stream
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv)
add_test(NAME geom_depthquant_stream_directed COMMAND test_geom_depthquant_stream_directed)
set_tests_properties(geom_depthquant_stream_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# GEOM.LIGHT's ENVIRONMENT LOADER: SetEnvironment -> the light bank (owner
# ruling R25, entry I48), every word against zref::light_env::bank_of.
add_executable(test_light_env_directed geometry/light_env_directed.cpp)
target_link_libraries(test_light_env_directed PRIVATE zhao_harness zhao_zref)
verilate(test_light_env_directed
  PREFIX Vzhao_light_env
  TOP_MODULE zhao_light_env
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_light_env.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME light_env_directed COMMAND test_light_env_directed)
set_tests_properties(light_env_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# The raster_state u32 layout (owner ruling R28) against its model and against
# GEOM.CLIP's own cull encoding, read from the RTL source.
add_executable(test_raster_state_directed geometry/raster_state_directed.cpp)
target_link_libraries(test_raster_state_directed PRIVATE zhao_zref)
add_test(NAME raster_state_directed COMMAND test_raster_state_directed
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(raster_state_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 60)

# THE SMOKE BENCH'S GEOMETRY FIXTURE, and every number the bench asserts about
# it, derived from the reference (project_vertex -> Clip -> Setup -> Binner).
# The bench `include`s the generated header; this test fails if it is stale.
add_executable(smoke_geom_fixture_gen prod/smoke_geom_fixture_gen.cpp)
target_link_libraries(smoke_geom_fixture_gen PRIVATE zhao_zref)
target_include_directories(smoke_geom_fixture_gen PRIVATE ${CMAKE_SOURCE_DIR}/reference/src)
add_test(NAME smoke_geom_fixture_fresh
  COMMAND smoke_geom_fixture_gen --check ${CMAKE_CURRENT_SOURCE_DIR}/prod/smoke_geom_fixture.svh)
set_tests_properties(smoke_geom_fixture_fresh PROPERTIES LABELS "fast;nightly" TIMEOUT 60)
set_tests_properties(geom_meshfetch_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# SKIN.NORM's licence: one transformed-and-renormalised normal reused across
# N lights agrees with N independent skin_normal_lambert calls. That is the
# equivalence docket D5 asserts when it rules the hardware must not repeat the
# normal work per light, and nothing checked it before.
add_executable(test_skin_norm_split_directed geometry/skin_norm_split_directed.cpp)
target_link_libraries(test_skin_norm_split_directed PRIVATE zhao_harness zhao_zref)
add_test(NAME skin_norm_split_directed COMMAND test_skin_norm_split_directed)
set_tests_properties(skin_norm_split_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# SKIN.NORM against zref::creature::skin_world_normal. The testbench PLAYS
# zhao_field_isqrt: that block has its own differential against zref::isqrt_u64,
# and instantiating it here would re-prove the square root while hiding whether
# THIS block handed it the right sum of squares.
add_executable(test_skin_norm_rtl_directed geometry/skin_norm_rtl_directed.cpp)
target_link_libraries(test_skin_norm_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_skin_norm_rtl_directed
  PREFIX Vzhao_geom_skin_norm
  TOP_MODULE zhao_geom_skin_norm
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_skin_norm.sv)
add_test(NAME skin_norm_rtl_directed COMMAND test_skin_norm_rtl_directed)
set_tests_properties(skin_norm_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.PAGELOADER -- one whole 21,376-byte page, HPS DDR -> local SDRAM,
# against zref::terrain::page_load (which composes zref::mem::upload_verdict
# rather than restating it).
#
# The bench plays MEM.HPS.BRIDGE and MEM.GUARD, and INSTANTIATES the real
# zhao_mem_guard alongside as an observer: `tools/rtl/check_guard_verdict.py`
# records that two geometry fetchers read every guard PASS as a denial for
# months precisely because every played guard raised `ready` and `ok` together.
# The played model here is transcribed from the real block's two-cycle
# protocol, and the real block's own verdict on these requests is measured
# rather than assumed.
#
# Stalls are a first-class phase, not a footnote: bridge latency, inter-beat
# gaps, guard `ready` hold and write-beat pacing are all knobs, and the exact
# 334 / 334 / 2,672 burst / request / beat counts are asserted under every one
# of them -- a machine doing its work twice produces byte-identical output and
# only a count can see it.
add_executable(test_pageloader_rtl_directed terrain/pageloader_rtl_directed.cpp)
target_link_libraries(test_pageloader_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_pageloader_rtl_directed
  PREFIX Vtb_pageloader
  TOP_MODULE tb_pageloader
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageloader.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_pageloader.sv)
add_test(NAME pageloader_rtl_directed COMMAND test_pageloader_rtl_directed)
set_tests_properties(pageloader_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ...and the lint gate. A sibling block reached "committed" without ever having
# been lexed, because it had no lint test and a comment line beginning with the
# simulator's name is read as a pragma. An RTL file with no lint test is an RTL
# file nothing has read.
add_test(NAME lint_terrain_pageloader
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_pageloader
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageloader.sv)
set_tests_properties(lint_terrain_pageloader PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.WRITEBACK -- the F sheet out to the HPS journal, behind an ACK.
#
# The mirror image of TERRAIN.PAGELOADER, and its bench inherits the same two
# disciplines. The guard is PLAYED with the real block's two-cycle protocol
# (`ready` a LEVEL, `ok` a PULSE one cycle later) AND instantiated as an
# observer -- but here the observer must REFUSE every request, because MEM.GUARD
# gives TERRAIN.BUILD a WRITE-ONLY window over the page pool and this block
# READS it. `shadow_viol_count == 130` with `shadow_ok_count == 0` is the
# evidence for the read arm the contract asks for; when that arm lands, those
# two checks invert and that inversion is its acceptance test.
#
# And the counts are exact, not "at least": 130 guard requests, 1,040 read
# beats, 128 bridge bursts, 1,024 write beats, under every stall profile. A
# machine doing its work twice produces a byte-identical journal entry.
add_executable(test_writeback_rtl_directed terrain/writeback_rtl_directed.cpp)
target_link_libraries(test_writeback_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_writeback_rtl_directed
  PREFIX Vtb_writeback
  TOP_MODULE tb_writeback
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_writeback.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_writeback.sv)
add_test(NAME writeback_rtl_directed COMMAND test_writeback_rtl_directed)
set_tests_properties(writeback_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ...and the lint gate. An RTL file with no lint test is an RTL file nothing has
# read -- a comment line beginning with the simulator's name is parsed as a
# pragma and stops the file lexing, which happened five times in one day.
add_test(NAME lint_terrain_writeback
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_writeback
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_writeback.sv)
set_tests_properties(lint_terrain_writeback PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# TERRAIN.WRITEBACK's journal DOORBELL (owner ruling R14;
# design/contracts/TERRAIN.WRITEBACK.DOORBELL.md). SW.STREAM's grants give each
# writeback job its journal address and ticket; every ticket comes back FINAL
# exactly once. The mutant control FIRES `ret_overflow_o`, which the credit makes
# unreachable with legal stimulus. INVERTED POLARITY on the control.
add_test(NAME lint_terrain_jdoorbell
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_jdoorbell
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jdoorbell.sv)
set_tests_properties(lint_terrain_jdoorbell PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_jdoorbell_directed terrain/jdoorbell_directed.cpp)
target_link_libraries(test_jdoorbell_directed PRIVATE zhao_harness)
verilate(test_jdoorbell_directed
  PREFIX Vzhao_terrain_jdoorbell
  TOP_MODULE zhao_terrain_jdoorbell
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jdoorbell.sv)
add_test(NAME jdoorbell_directed COMMAND test_jdoorbell_directed)
set_tests_properties(jdoorbell_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_executable(test_jdoorbell_mutant_control terrain/jdoorbell_mutant_control.cpp)
target_link_libraries(test_jdoorbell_mutant_control PRIVATE zhao_harness)
verilate(test_jdoorbell_mutant_control
  PREFIX Vzhao_terrain_jdoorbell_mutant
  TOP_MODULE zhao_terrain_jdoorbell_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_jdoorbell_mutant.sv)
add_test(NAME jdoorbell_mutant_control COMMAND test_jdoorbell_mutant_control)
set_tests_properties(jdoorbell_mutant_control PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.JOBISSUE -- the SUBPATCH JOB ISSUER, core entry I21's missing block.
# It joins zhao_terrain_lod's `lod_target` stream to the patch's own draw
# context and drives zhao_terrain_group_seq's THIRTEEN-field job port, plus
# zhao_terrain_compcache_front's retirement pulse. No mutant is registered here
# and that is a measured claim, not an omission: all seven of its fault
# counters are reachable from the block's own boundary with legal stimulus, and
# terrain_jobissue_directed fires each one and shows each silent beside it.
add_test(NAME lint_terrain_jobissue
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_jobissue
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jobissue.sv)
set_tests_properties(lint_terrain_jobissue PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_jobissue_directed terrain/terrain_jobissue_directed.cpp)
target_link_libraries(test_terrain_jobissue_directed PRIVATE zhao_harness)
verilate(test_terrain_jobissue_directed
  PREFIX Vzhao_terrain_jobissue
  TOP_MODULE zhao_terrain_jobissue
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jobissue.sv)
add_test(NAME terrain_jobissue_directed COMMAND test_terrain_jobissue_directed)
set_tests_properties(terrain_jobissue_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.SPDESC -- the SUBPATCH DESCRIPTOR ASSEMBLER, the other half of entry
# I21. It builds zhao_terrain_lod's `sp_*` patch_state input from the three
# places its fields live: zhao_terrain_devstore's deviations and history,
# zhao_terrain_compcache_front's placed column/row positions, and the
# {slot, src_id} pair captured at the compose door. No mutant is registered
# here and that is a measured claim, not an omission: all five of its fault
# counters are reachable from the block's own boundary with legal stimulus,
# and terrain_spdesc_directed fires each one with a negative control beside it.
add_test(NAME lint_terrain_spdesc
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_spdesc
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_spdesc.sv)
set_tests_properties(lint_terrain_spdesc PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_spdesc_directed terrain/terrain_spdesc_directed.cpp)
target_link_libraries(test_terrain_spdesc_directed PRIVATE zhao_harness)
verilate(test_terrain_spdesc_directed
  PREFIX Vzhao_terrain_spdesc
  TOP_MODULE zhao_terrain_spdesc
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_spdesc.sv)
add_test(NAME terrain_spdesc_directed COMMAND test_terrain_spdesc_directed)
set_tests_properties(terrain_spdesc_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.EDGERECON -- the NEIGHBOUR-EDGE LEVEL PRODUCER, owner ruling
# 2026-09-22 item 6. It buffers the frame's decided subpatch levels and hands
# `zhao_terrain_lod` the four `edge_*` words that instance has driven from the
# literal 8'h00 since entry I21 was written.
#
# The directed suite's case 2 is the reason the block exists: the SYMMETRY
# THEOREM checked by exhaustion over a 3 x 3 patch block, with the equality
# computed the way `zhao_terrain_tess` computes it (max(neighbour, own)), so a
# failure IS a crack rather than a proxy for one. Its positive control shows
# the same comparison DISAGREEING under the retained 8'h00 tie-off, which is
# the measured statement that the conservative constant is not crack-free
# across a patch border.
#
# No mutant is registered here and that is a measured claim, not an omission:
# all nine counters are reachable from the block's own boundary with legal
# stimulus, and terrain_edgerecon_directed fires each with a control beside it.
add_test(NAME lint_terrain_edgerecon
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_edgerecon
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_edgerecon.sv)
set_tests_properties(lint_terrain_edgerecon PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_edgerecon_directed terrain/terrain_edgerecon_directed.cpp)
target_link_libraries(test_terrain_edgerecon_directed PRIVATE zhao_harness)
verilate(test_terrain_edgerecon_directed
  PREFIX Vzhao_terrain_edgerecon
  TOP_MODULE zhao_terrain_edgerecon
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_edgerecon.sv)
add_test(NAME terrain_edgerecon_directed COMMAND test_terrain_edgerecon_directed)
set_tests_properties(terrain_edgerecon_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ===========================================================================
# EDGEPREP, 2026-09-25: the two links between TERRAIN.LOD and TERRAIN.EDGERECON
# ===========================================================================
# P2 `zhao_terrain_prepwalk` -- the REPLAYABLE sealed-list PREPARE reader, and
# P3 `zhao_terrain_lodshare` -- the LOD time-share, the frame-scoped governor
# freeze, and the suppression of PREPARE's history writeback.
#
# Neither is composed. Owner directive 2026-09-23 section 2 stages this as four
# packets and says "Close I21 and the disconnected EDGERECON entry only when the
# same complete production path satisfies both"; P4 composes all three blocks
# together and spends the one fit.
#
# THE MUTANT AND WHY THERE IS EXACTLY ONE. Eleven of lodshare's twelve counters
# are reachable from the block's own boundary with legal stimulus and
# terrain_lodshare_directed fires each with a control beside it. The twelfth,
# `idq_overflow_o`, counts a push into a FULL identity queue -- mutually
# exclusive with the full-guard, so UNREACHABLE BY CONSTRUCTION. Its
# demonstration is the committed mutant, driven with INVERTED polarity by
# terrain_lodshare_mutant: that test PASSES when the counter FIRES. Directed
# case 9 is its positive control, proving a HEALTHY block reaches full with the
# same stimulus, so a mutant run reading zero cannot be excused as "the state
# was never reached".
add_test(NAME lint_terrain_lodshare
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_lodshare
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodshare.sv)
set_tests_properties(lint_terrain_lodshare PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_lodshare_directed terrain/terrain_lodshare_directed.cpp)
target_link_libraries(test_terrain_lodshare_directed PRIVATE zhao_harness)
verilate(test_terrain_lodshare_directed
  PREFIX Vzhao_terrain_lodshare
  TOP_MODULE zhao_terrain_lodshare
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodshare.sv)
add_test(NAME terrain_lodshare_directed COMMAND test_terrain_lodshare_directed)
set_tests_properties(terrain_lodshare_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_terrain_lodshare_mutant terrain/terrain_lodshare_mutant.cpp)
target_link_libraries(test_terrain_lodshare_mutant PRIVATE zhao_harness)
verilate(test_terrain_lodshare_mutant
  PREFIX Vzhao_terrain_lodshare_idqoverflow_mutant
  TOP_MODULE zhao_terrain_lodshare_idqoverflow_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_lodshare_idqoverflow_mutant.sv)
add_test(NAME terrain_lodshare_mutant COMMAND test_terrain_lodshare_mutant)
set_tests_properties(terrain_lodshare_mutant PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_terrain_prepwalk
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_prepwalk
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place_law_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_prepwalk.sv)
set_tests_properties(lint_terrain_prepwalk PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_prepwalk_directed terrain/terrain_prepwalk_directed.cpp)
target_link_libraries(test_terrain_prepwalk_directed PRIVATE zhao_harness)
verilate(test_terrain_prepwalk_directed
  PREFIX Vtb_terrain_prepwalk
  TOP_MODULE tb_terrain_prepwalk
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_place_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_prepwalk.sv
          ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_prepwalk.sv)
add_test(NAME terrain_prepwalk_directed COMMAND test_terrain_prepwalk_directed)
set_tests_properties(terrain_prepwalk_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# TERRAIN.LOD's three deviations (owner ruling R8), zhao_terrain_loddev.
# Differential against zref::terrain::lod_deviation, built TWICE from one
# source: at the block's DEFAULT selector (which must equal
# zref::terrain::kLodDevIncludeBoundary, so the owner's pick cannot move in one
# language only, and which is the MESH reading since owner ruling R22) and with
# DEV_INCLUDE_BOUNDARY=0 (the morph reading), so both readings stay covered.
add_test(NAME lint_terrain_loddev
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_loddev
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv)
set_tests_properties(lint_terrain_loddev PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_loddev_directed terrain/terrain_loddev_directed.cpp)
target_link_libraries(test_terrain_loddev_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_loddev_directed
  PREFIX Vzhao_terrain_loddev
  TOP_MODULE zhao_terrain_loddev
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv)
add_test(NAME terrain_loddev_directed COMMAND test_terrain_loddev_directed)
set_tests_properties(terrain_loddev_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_executable(test_terrain_loddev_morph_directed terrain/terrain_loddev_directed.cpp)
target_compile_definitions(test_terrain_loddev_morph_directed PRIVATE LODDEV_MORPH=1)
target_link_libraries(test_terrain_loddev_morph_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_loddev_morph_directed
  PREFIX Vzhao_terrain_loddev_morph
  TOP_MODULE zhao_terrain_loddev
  VERILATOR_ARGS -GDEV_INCLUDE_BOUNDARY=0
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv)
add_test(NAME terrain_loddev_morph_directed COMMAND test_terrain_loddev_morph_directed)
set_tests_properties(terrain_loddev_morph_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# The two laws TERRAIN.BAKE was missing (owner rulings R15/R56, spec 9.3):
# zref::terrain::stamp_depth (the ART table) and sheet_texel_for_vertex (the
# 64x64 -> 33x33 nearest-texel resample). Pure zref, no RTL: the laws are what
# had to be RATIFIED before the block's per-vertex depth mode can be written.
add_executable(test_stamp_to_bake_laws_directed terrain/stamp_to_bake_laws_directed.cpp)
target_link_libraries(test_stamp_to_bake_laws_directed PRIVATE zhao_zref)
add_test(NAME stamp_to_bake_laws_directed COMMAND test_stamp_to_bake_laws_directed)
set_tests_properties(stamp_to_bake_laws_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)
# ...AND THE SAME TWO LAWS IN SILICON, added 2026-09-20 under OWNER RULING R194.
# The target above is pure zref and proves the REFERENCE is right; it says
# nothing about RTL, and until R194 accepted the nearest-texel rim by looking
# ("Shipped is fine. Slightly different but not off.") there was no RTL to
# check -- the core's entry I32 records a search of all of fpga/rtl for these
# law names returning ZERO HITS. `zhao_terrain_stampdepth` is that silicon and
# this is the exhaustive differential: every one of the 33x33 vertices against
# the address law, all 256 strengths against the art table, and the composed
# law over a rim-crossing disc sheet.
add_executable(test_terrain_stampdepth_directed terrain/terrain_stampdepth_directed.cpp)
target_link_libraries(test_terrain_stampdepth_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_stampdepth_directed
  PREFIX Vzhao_terrain_stampdepth
  TOP_MODULE zhao_terrain_stampdepth
  VERILATOR_ARGS -Wall
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_stampdepth.sv)
add_test(NAME terrain_stampdepth_directed COMMAND test_terrain_stampdepth_directed)
set_tests_properties(terrain_stampdepth_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
# ---------------------------------------------------------------------------
# THE PAGE-LOAD DEVIATION PATH (owner rulings R22/R24/R59):
#   zhao_terrain_lodfeed (the mip pass's fine stream, buffered, walked)
#   -> zhao_terrain_devstore (the per-page deviation and history store)
# tests/terrain/tb_terrain_lodpath.sv is a WRAPPER, not a model: both blocks
# are the production files and every wire is port to port.
# ---------------------------------------------------------------------------
# The store imports zhao_pkg as of owner ruling R242 (2026-09-22): it holds
# its records in SDRAM through a MEM.GUARD socket, so it needs
# zhao_guard_req_t / zhao_client_e and the region constants. It inferred two
# memories before that change and infers none now.
add_test(NAME lint_terrain_devstore
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_devstore
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_devstore.sv)
set_tests_properties(lint_terrain_devstore PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_terrain_lodfeed
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_lodfeed
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodfeed.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv)
set_tests_properties(lint_terrain_lodfeed PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_terrain_lodpath_directed terrain/terrain_lodpath_directed.cpp)
target_link_libraries(test_terrain_lodpath_directed PRIVATE zhao_harness zhao_zref)
# Owner ruling R242 put the store's records in SDRAM, so this bench now carries
# a PLAYED fabric and a REAL `zhao_mem_guard` as an observer: the value
# traversal it exists to prove now runs through the memory system, and whether
# the requests are legal is answered by the block that owns that question.
verilate(test_terrain_lodpath_directed
  PREFIX Vtb_terrain_lodpath
  TOP_MODULE tb_terrain_lodpath
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
  # THE PACKAGES COME FIRST.  Verilator parses this list in order and a
  # `zhao_guard_req_t` in the wrapper is "referenced before declaration" if the
  # wrapper is read before zhao_pkg -- which is what a list that grew a package
  # at its end does.
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_params_pkg.sv
          ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_lodpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodfeed.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_devstore.sv)
add_test(NAME terrain_lodpath_directed COMMAND test_terrain_lodpath_directed)
set_tests_properties(terrain_lodpath_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ---------------------------------------------------------------------------
# `slot_addr_bad_o`'s POSITIVE CONTROL AND ITS NEGATIVE CONTROL (ruling R242).
# The counter is unreachable with legal stimulus -- the engine's held address
# and the read FSM's required address are equal by construction while the block
# is correct -- so it needs a COMMITTED MUTANT, and the mutant needs a run
# against UNMUTATED production beside it or a seam that never engaged would
# compile production twice and both runs would agree.
# ONE source, TWO executables, opposite expectations.
# ---------------------------------------------------------------------------
foreach(mut_variant fires silent)
  set(mut_target test_terrain_devstore_addrmut_${mut_variant})
  add_executable(${mut_target} terrain/terrain_devstore_addrmut.cpp)
  target_link_libraries(${mut_target} PRIVATE zhao_harness zhao_zref)
  set(mut_sources ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_params_pkg.sv
                  ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_lodpath.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodfeed.sv
                  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv)
  if(mut_variant STREQUAL "fires")
    # The macro reaches BOTH the RTL seam and the C++ expectation, so the two
    # cannot drift apart into a driver that expects one block and measures the
    # other.
    target_compile_definitions(${mut_target} PRIVATE ZHAO_DEVSTORE_ADDRLATCH_MUT)
    set(mut_vargs -DZHAO_DEVSTORE_ADDRLATCH_MUT)
    list(APPEND mut_sources
         ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_devstore_addrlatch_mutant.sv)
  else()
    set(mut_vargs "")
    list(APPEND mut_sources
         ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_devstore.sv)
  endif()
  verilate(${mut_target}
    PREFIX Vtb_terrain_lodpath
    TOP_MODULE tb_terrain_lodpath
    VERILATOR_ARGS ${mut_vargs}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
    SOURCES ${mut_sources})
  add_test(NAME terrain_devstore_addrmut_${mut_variant} COMMAND ${mut_target})
  set_tests_properties(terrain_devstore_addrmut_${mut_variant}
    PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
endforeach()

# ---------------------------------------------------------------------------
# GEOM.PARAMBUF's ACCEPTANCE TEST (owner completion ruling ITEM 4, 2026-09-22)
# ---------------------------------------------------------------------------
# The arena PRODUCER and the chunk WALKER against a REAL memory path -- the
# real zhao_mem_guard, the real zhao_vram_arbiter, the real zhao_sdram_ctrl and
# the SDRAM model. No played fabric anywhere, and that is the whole point: the
# two defects this test found on its first run (the lease low under the
# directory write, and the scratch never released on a faulted frame) are
# invisible to every per-block gate, because neither is a fault in the guard or
# in the arena SEPARATELY. It is the arena asking the guard for something at a
# moment when the arena itself has said no, and only a bench holding both with
# a real memory between them has the state to reach it.
#
# The bench also transcribes the shell's slot-3 write glue -- the `gq` queue,
# the exact room gate, the owner mux -- rather than inventing one, so what it
# exercises is the arrangement the console actually composes.
set(ZHAO_PARAMARENA_SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_params_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share2.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_share_wr.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_vram_arbiter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_sdram_ctrl.sv
    ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_parambuf.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_paramwalk.sv)

add_executable(test_geom_paramarena_directed geometry/geom_paramarena_directed.cpp)
target_link_libraries(test_geom_paramarena_directed PRIVATE zhao_harness)
verilate(test_geom_paramarena_directed
  PREFIX Vtb_zhao_geom_paramarena
  TOP_MODULE tb_zhao_geom_paramarena
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
  SOURCES ${ZHAO_PARAMARENA_SOURCES}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_paramarena.sv
          ${CMAKE_SOURCE_DIR}/tests/geometry/tb_zhao_geom_paramarena.sv)
add_test(NAME geom_paramarena_directed COMMAND test_geom_paramarena_directed)
set_tests_properties(geom_paramarena_directed
  PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# ---------------------------------------------------------------------------
# `addr_view_bad_o`'s POSITIVE CONTROL AND ITS NEGATIVE CONTROL (item 4).
# The counter watches for a request whose held address is not in the view the
# lease names -- the queued-request fault item 4 names by hand -- and it is
# UNREACHABLE with legal stimulus while the drain precondition is correct, so
# it needs a COMMITTED MUTANT. The mutant needs a run against UNMUTATED
# production beside it, or a seam that never engaged would compile production
# twice and both runs would agree: CLAUDE.md's negative control.
# ONE source, TWO executables, opposite expectations.
# ---------------------------------------------------------------------------
foreach(pa_variant fires silent)
  set(pa_target test_geom_paramarena_drainmut_${pa_variant})
  add_executable(${pa_target} geometry/geom_paramarena_drainmut.cpp)
  target_link_libraries(${pa_target} PRIVATE zhao_harness)
  set(pa_sources ${ZHAO_PARAMARENA_SOURCES})
  if(pa_variant STREQUAL "fires")
    # The macro reaches BOTH the RTL seam and the C++ expectation, so the two
    # cannot drift into a driver that expects one block and measures the other.
    target_compile_definitions(${pa_target} PRIVATE ZHAO_PARAMARENA_DRAIN_MUT)
    set(pa_vargs -DZHAO_PARAMARENA_DRAIN_MUT)
    list(APPEND pa_sources
         ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_paramarena_drain_mutant.sv)
  else()
    set(pa_vargs "")
    list(APPEND pa_sources
         ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_paramarena.sv)
  endif()
  list(APPEND pa_sources
       ${CMAKE_SOURCE_DIR}/tests/geometry/tb_zhao_geom_paramarena.sv)
  verilate(${pa_target}
    PREFIX Vtb_zhao_geom_paramarena
    TOP_MODULE tb_zhao_geom_paramarena
    VERILATOR_ARGS ${pa_vargs}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
    SOURCES ${pa_sources})
  add_test(NAME geom_paramarena_drainmut_${pa_variant} COMMAND ${pa_target})
  set_tests_properties(geom_paramarena_drainmut_${pa_variant}
    PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
endforeach()

# ---------------------------------------------------------------------------
# `burst_unaligned_o`'s POSITIVE CONTROL AND ITS NEGATIVE CONTROL.
# The counter watches for a request address that is not a multiple of the
# SDRAM's burst-alignment quantum.  A JEDEC BL8 SEQUENTIAL burst wraps inside
# its aligned eight-column block and the BEHAVIOURAL MODEL READS LINEARLY, so
# a misaligned request is served correctly in simulation and wrongly by the
# part -- NO functional test in this tree can fail on it, in either polarity.
# The invariant is therefore counted, and a counted invariant that has never
# been seen to move is a claim.  Unreachable with legal stimulus while the
# allocator is correct, so it owes a COMMITTED MUTANT.
# ONE source, TWO executables, opposite expectations.
# ---------------------------------------------------------------------------
foreach(pal_variant fires silent)
  set(pal_target test_geom_paramarena_alignmut_${pal_variant})
  add_executable(${pal_target} geometry/geom_paramarena_alignmut.cpp)
  target_link_libraries(${pal_target} PRIVATE zhao_harness)
  set(pal_sources ${ZHAO_PARAMARENA_SOURCES})
  if(pal_variant STREQUAL "fires")
    # The macro reaches BOTH the RTL seam and the C++ expectation, so the two
    # cannot drift into a driver that expects one block and measures the other.
    target_compile_definitions(${pal_target} PRIVATE ZHAO_PARAMARENA_ALIGN_MUT)
    set(pal_vargs -DZHAO_PARAMARENA_ALIGN_MUT)
    list(APPEND pal_sources
         ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_paramarena_align_mutant.sv)
  else()
    set(pal_vargs "")
    list(APPEND pal_sources
         ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_paramarena.sv)
  endif()
  list(APPEND pal_sources
       ${CMAKE_SOURCE_DIR}/tests/geometry/tb_zhao_geom_paramarena.sv)
  verilate(${pal_target}
    PREFIX Vtb_zhao_geom_paramarena
    TOP_MODULE tb_zhao_geom_paramarena
    VERILATOR_ARGS ${pal_vargs}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
    SOURCES ${pal_sources})
  add_test(NAME geom_paramarena_alignmut_${pal_variant} COMMAND ${pal_target})
  set_tests_properties(geom_paramarena_alignmut_${pal_variant}
    PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
endforeach()

# ---------------------------------------------------------------------------
# THE SAME PRODUCER INTO THE OTHER CONSUMER -- core entry I18, ruling R70:
#   zhao_terrain_lodfeed -> the 24 -> 32 widening -> zhao_measure_histogram
# The path above ends at the STORE and proves the deviation is CORRECT; this
# one ends at the ORGAN THE CONSOLE COMPOSES and proves it ARRIVES, in the
# right bin. The console smoke cannot show it: every page it plays fails its
# CRC, so the fine stream never moves (measured -- the bench prints the lane).
# tests/terrain/tb_terrain_lodhist.sv is a wrapper and its widening is copied
# from zhao_console_core.sv character for character.
# ---------------------------------------------------------------------------
add_executable(test_terrain_lodhist_directed terrain/terrain_lodhist_directed.cpp)
target_link_libraries(test_terrain_lodhist_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_terrain_lodhist_directed PRIVATE ${CMAKE_SOURCE_DIR}/tests/measure)
verilate(test_terrain_lodhist_directed
  PREFIX Vtb_terrain_lodhist
  TOP_MODULE tb_terrain_lodhist
  SOURCES ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_lodhist.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodfeed.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_histogram.sv
          # The histogram's double bank. It is the reason the block is RAM and
          # not 3,072 registers, so it is a real dependency, not glue.
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv)
add_test(NAME terrain_lodhist_directed COMMAND test_terrain_lodhist_directed)
set_tests_properties(terrain_lodhist_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# THE WIDTH GUARD'S POSITIVE CONTROL. The same wrapper at EW=16, where the
# 24-bit deviations would be TRUNCATED -- a fault that reads LOW in an organ
# that is metric-agnostic and therefore cannot notice. WILL_FAIL: it passes
# when the elaboration $fatal stops the process. The driver's header records
# why this cannot be a lint (`--lint-only` does not run `initial` blocks) and
# why the guard's first version could never have fired at all.
add_executable(test_terrain_lodhist_widthguard terrain/terrain_lodhist_widthguard_control.cpp)
target_link_libraries(test_terrain_lodhist_widthguard PRIVATE zhao_harness)
verilate(test_terrain_lodhist_widthguard
  PREFIX Vtb_terrain_lodhist
  TOP_MODULE tb_terrain_lodhist
  VERILATOR_ARGS -GEW=16
  SOURCES ${CMAKE_SOURCE_DIR}/tests/terrain/tb_terrain_lodhist.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_lodfeed.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loddev.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_histogram.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv)
add_test(NAME terrain_lodhist_widthguard COMMAND test_terrain_lodhist_widthguard)
# TIMEOUT 60, deliberately short. The driver's own watchdog exits at 2 seconds
# and the honest failure path exits in milliseconds, so anything past a minute
# is the watchdog itself having wedged -- and a TIMEOUT is the one result
# WILL_FAIL cannot invert, so it must surface in a minute rather than five.
set_tests_properties(terrain_lodhist_widthguard PROPERTIES
  LABELS "fast;nightly" TIMEOUT 60 WILL_FAIL TRUE)

# ---------------------------------------------------------------------------
# THE EYE (owner ruling R63). SetView 0x0010 carries `fx16 eye[3]`, zhao_cmd_exec
# commits it as steps 17/18/19 of the view walk onto projector configuration
# addresses 19/20/21, and zhao_view_eye is what holds those three words per view
# for zhao_terrain_lod. cmd_exec_directed cases 22-24 are the producer half;
# this is the store half.
# ---------------------------------------------------------------------------
add_test(NAME lint_view_eye
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_view_eye
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_eye.sv)
set_tests_properties(lint_view_eye PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_view_eye_directed common/view_eye_directed.cpp)
target_link_libraries(test_view_eye_directed PRIVATE zhao_harness)
verilate(test_view_eye_directed
  PREFIX Vzhao_view_eye
  TOP_MODULE zhao_view_eye
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_eye.sv)
add_test(NAME view_eye_directed COMMAND test_view_eye_directed)
set_tests_properties(view_eye_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# ---------------------------------------------------------------------------
# PROJ.CFGVALID -- the shared projector's config-valid arm, which is the
# producer entry I14 says `proj_en_i` has never had. FINDINGS-projinput.md
# decision D-1 recommended exactly this reading of the port; this is the block
# and its lane. Both counters are fired BY STIMULUS here (R95): `arm_events_o`
# is the arm's positive control and `held_offers_o` counts vertices the gate
# actually withheld, with its negative control in the same case.
# ---------------------------------------------------------------------------
set(ZHAO_PROJ_CFGVALID_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_proj_cfgvalid.sv)
add_test(NAME lint_proj_cfgvalid
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_proj_cfgvalid
                 ${ZHAO_PROJ_CFGVALID_SV})
set_tests_properties(lint_proj_cfgvalid PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_executable(test_proj_cfgvalid_directed common/proj_cfgvalid_directed.cpp)
target_link_libraries(test_proj_cfgvalid_directed PRIVATE zhao_harness)
verilate(test_proj_cfgvalid_directed
  PREFIX Vzhao_proj_cfgvalid
  TOP_MODULE zhao_proj_cfgvalid
  SOURCES ${ZHAO_PROJ_CFGVALID_SV})
add_test(NAME proj_cfgvalid_directed COMMAND test_proj_cfgvalid_directed)
set_tests_properties(proj_cfgvalid_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 120)

# The elaboration guard, FIRED. `--lint-only` does not run `initial` blocks
# (CLAUDE.md), so a clean lint says nothing whatever about this check. The
# parameterisation broken here is the one that HANGS the console: a bank wider
# than the address bus has addresses nobody can write, so the mask never fills,
# so the projector never arms and no frame is ever drawn.
#
# THE FIRST VERSION OF THIS FIRE TEST BROKE A DIFFERENT GUARD, and it fired
# cleanly while the guard asserted something FALSE -- see the block's own note.
# A guard firing is evidence that it fires and no evidence that its claim is
# true, which is the broken-instrument law inside a positive control.
add_executable(test_proj_cfgvalid_addrw_guard common/proj_cfgvalid_directed.cpp)
target_link_libraries(test_proj_cfgvalid_addrw_guard PRIVATE zhao_harness)
verilate(test_proj_cfgvalid_addrw_guard
  PREFIX Vzhao_proj_cfgvalid
  TOP_MODULE zhao_proj_cfgvalid
  SOURCES ${ZHAO_PROJ_CFGVALID_SV}
  VERILATOR_ARGS -GMAT_WORDS=64)
add_test(NAME proj_cfgvalid_addrw_guard
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_proj_cfgvalid_addrw_guard>
    -DEXPECT=can\ never\ be\ written
    -DRUN_TIMEOUT=10
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(proj_cfgvalid_addrw_guard PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120)

# The wrapper's own negative control, exactly as measure_starve does it: the
# wrapper reports a pass on a process that never exits, so the branch that must
# be shown to work is the one where the diagnostic is ABSENT. Pointed at the
# healthy build, which runs the same module with a legal MAT_WORDS.
add_test(NAME proj_cfgvalid_addrw_guard_wrapper_control
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:test_proj_cfgvalid_directed>
    -DEXPECT=can\ never\ be\ written
    -DRUN_TIMEOUT=60
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_expect_fatal.cmake)
set_tests_properties(proj_cfgvalid_addrw_guard_wrapper_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 120 WILL_FAIL TRUE)
# R8's side-by-side render of both deviation readings (a tool, not a test):
# tools/terrain/lod_readings_sheet.py builds it, runs it and assembles the PNG.
add_executable(terrain_lod_readings_render ${CMAKE_SOURCE_DIR}/tools/terrain/lod_readings_render.cpp)
target_link_libraries(terrain_lod_readings_render PRIVATE zhao_zref)

# Owner ruling R65's before/after of a dig across a PATCH SEAM, at VIDEO_Z60's
# 384x240 canvas. Same shape as the LOD readings above: the tool writes PPMs,
# tools/terrain/seam_dig_sheet.py lays them out, and the PNG is the only
# artefact kept. R65 owes the owner this picture before the nearest-texel
# fallback is final.
add_executable(terrain_seam_dig_render ${CMAKE_SOURCE_DIR}/tools/terrain/seam_dig_render.cpp)
target_link_libraries(terrain_seam_dig_render PRIVATE zhao_zref)

# Owner ruling R37's before/after for the PROPOSED tag-to-gather law. Same
# shape as the LOD readings above: the tool writes PPMs, tools/post/
# gather_law_sheet.py lays them out, and the PNG is the only artefact kept.
add_executable(gather_law_render ${CMAKE_SOURCE_DIR}/tools/post/gather_law_render.cpp)
target_link_libraries(gather_law_render PRIVATE zhao_zref)


# ---------------------------------------------------------------------------
# TERRAIN.SEQ -- the terrain command path, against zref::terrain::seq::Sequencer.
#
# The block is a PUMP, so the thing under test is a SEQUENCE OF ACTIONS and this
# test compares the whole observed action log -- kind, order and every payload
# field -- against the reference's, not just the fourteen counters. A counter
# cannot see order, and the two orderings that matter here are both invisible in
# a count: T4's writeback-before-load barrier for a dirty victim, and list order
# itself, which the determinism ledger names as the whole world layer's anchor.
#
# EVERY FRAME IS REPLAYED UNDER FOUR STALL PATTERNS, one of them MOSTLY ready
# (three cycles in four). TERRAIN.ISLAND's differential passed 21 checks over a
# full 15,625-patch sweep and 3,000 random draws and still missed a dropped
# answer, because every phase drove ready HIGH on every cycle -- and the sibling
# that found it found it at three-in-four. The four logs must be identical.
add_executable(test_seq_rtl_directed terrain/seq_rtl_directed.cpp)
target_link_libraries(test_seq_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_seq_rtl_directed
  PREFIX Vtb_terrain_seq
  TOP_MODULE tb_terrain_seq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_seq.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_seq.sv)
add_test(NAME seq_rtl_directed COMMAND test_seq_rtl_directed)
set_tests_properties(seq_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ...and the lint gate. A sibling block reached "committed" without ever having
# been lexed, because it had no lint test and a comment line beginning with the
# simulator's name is read as a pragma. An RTL file with no lint test is an RTL
# file nothing has read.
#
# It runs the module on its OWN DEFAULTS, which is also how COMPOSE_SLOTS = 256
# gets elaborated: the bench instantiates it at 16 so T6's fault is reachable in
# a short frame, and this is what proves the shipped width still builds.
add_test(NAME lint_terrain_seq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_seq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_seq.sv)
set_tests_properties(lint_terrain_seq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.PAGESTREAM -> TERRAIN.PATCH -- section 3.4 on real page bytes.
#
# `tb_terrain_world.sv`'s header called this chain "reachable only from a
# harness on BOTH ends, which is decoration rather than composition", and named
# the missing block: nothing turned a resident page slot into TERRAIN.PATCH's
# lattice intake. That block landed on 2026-09-07, and the two go together with
# NO GLUE on the height path -- base/scar/bottom/vi/vj cross directly, which is
# why the streamer emits three planes on one beat instead of one plane per pass.
#
# THE FIXTURE HAS TO REACH THE CLAMP OR THE SUITE IS VACUOUS. Section 3.4 is
# `max(fx(base) + fx(scar), fx(bottom))`, and a page whose bottom is always far
# below its top would pass against a block that dropped the max entirely. The
# page reaches a positive add, a negative add, the clamp, the boundary and
# fx_add's saturation -- and the count the ORACLE actually clamped is asserted,
# because the fixture's intent is not evidence, its effect is.
add_executable(test_compose_rtl_directed terrain/compose_rtl_directed.cpp)
target_link_libraries(test_compose_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_compose_rtl_directed
  PREFIX Vtb_terrain_compose
  TOP_MODULE tb_terrain_compose
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pagestream.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tess.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normals.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_compose.sv)
add_test(NAME compose_rtl_directed COMMAND test_compose_rtl_directed)
set_tests_properties(compose_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1200)

# ---------------------------------------------------------------------------
# TERRAIN.CMD -- SubmitTerrainSet, turned into a frame.
#
# THE ORACLE IS `zref::swstream::encode_record`, READ BACKWARDS: encode a
# sealed patch list, put it in the played HPS arena, and require that the ten
# fields the block hands TERRAIN.SEQ are field-for-field the records that went
# in, IN ORDER. A round trip against a real oracle and no new law.
#
# Every way this block can be wrong survives a count -- a sheared field is
# terrain in the wrong place, a reordered stream is a determinism failure that
# renders correctly on the machine that recorded it, a truncated stream is
# invisible because `patch_count` is the seal TERRAIN.SEQ stops at anyway, and
# a list acted on before its CRC was checked has already issued loads. So the
# whole stream is compared, and the corrupt-list phase requires ZERO records
# rather than "some records and then a fault".
add_executable(test_terrain_cmd_rtl_directed terrain/terrain_cmd_rtl_directed.cpp)
target_link_libraries(test_terrain_cmd_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_cmd_rtl_directed
  PREFIX Vtb_terrain_cmd
  TOP_MODULE tb_terrain_cmd
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_cmd.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_cmd.sv)
add_test(NAME terrain_cmd_rtl_directed COMMAND test_terrain_cmd_rtl_directed)
set_tests_properties(terrain_cmd_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_terrain_cmd
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_cmd
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_cmd.sv)
set_tests_properties(lint_terrain_cmd PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.PAGESTREAM -- a resident page slot, as a lattice.
#
# EVERY WAY THIS BLOCK CAN BE WRONG PRODUCES 1,089 VERTICES: the wrong slot, the
# wrong lane, the wrong plane, the wrong scan order, a stale staging buffer.
# All of them render as terrain and none of them move a count. So the bench
# compares the whole stream against `zref::terrain::page_lattice`, vertex by
# vertex and field by field, with a fixture in which each of those confusions
# produces a different number -- distinct injective values per plane, negative
# scars so a zero-extending read cannot hide, and neighbouring slots poisoned
# with patterns that are not this slot's.
#
# The REAL zhao_mem_guard watches the DUT's request wires as an observer:
# TERRAIN.PAGE_POOL gained its read arm on 2026-09-06, so every request must
# PASS, and the observer's count is the evidence rather than the played model's
# blanket verdict.
add_executable(test_pagestream_rtl_directed terrain/pagestream_rtl_directed.cpp)
target_link_libraries(test_pagestream_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_pagestream_rtl_directed
  PREFIX Vtb_pagestream
  TOP_MODULE tb_pagestream
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pagestream.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_pagestream.sv)
add_test(NAME pagestream_rtl_directed COMMAND test_pagestream_rtl_directed)
set_tests_properties(pagestream_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_terrain_mipfeed
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_mipfeed
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipfeed.sv)
set_tests_properties(lint_terrain_mipfeed PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_terrain_pagestream
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_pagestream
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pagestream.sv)
set_tests_properties(lint_terrain_pagestream PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# TERRAIN.PAGEIO -- bake's page window (design/contracts/TERRAIN.PAGEIO.md).
#
# Layer D had ZERO readers under fpga/ and layers B and D had no writer, so
# TERRAIN.BAKE could compute a scar and a breach and had nowhere to put either.
# This block serves all four of those ports.
#
# THE ACCEPTANCE TEST IS THAT LAYERS A, C AND E DO NOT MOVE. The guard has no
# byte enables -- `be_ok = (req.be == mask_of(req.len))` -- so a whole 64-byte
# burst is the smallest writable thing, and layer B's and layer D's edge bursts
# carry 2, 4, 6 and 58 foreign bytes. NOTHING IN THE MACHINE READS THOSE LAYERS
# BACK, so a block that clobbered them would pass every other gate forever.
# The bench reads the image back and compares byte for byte.
#
# It also checks the `nb_o` SCATTER against `zref::terrain::nobake_corner_shadow`
# (the gather), fires all five fault counters by stimulus, and asserts every
# one of them ZERO on the clean bake.
add_test(NAME lint_terrain_pageio
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_pageio
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageio.sv)
set_tests_properties(lint_terrain_pageio PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_pageio_rtl_directed terrain/pageio_rtl_directed.cpp)
target_link_libraries(test_pageio_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_pageio_rtl_directed
  PREFIX Vtb_pageio
  TOP_MODULE tb_pageio
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageio.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_pageio.sv)
add_test(NAME pageio_rtl_directed COMMAND test_pageio_rtl_directed)
set_tests_properties(pageio_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# TERRAIN.SHEETSEAM -- the layer-F read path, against the REAL SURFACE.SHEET.
#
# Owner ruling R221: when a bake asks zhao_surface_sheet for a layer-F page and
# gets ST_MISS, the record falls back to the RATIFIED parametric disc and the
# fallback is COUNTED. This suite is where that law is held.
#
# THE STORE IS REAL AND SO IS THE SHARE. `tb_sheetseam.sv` puts
# zhao_surface_sheet, zhao_surface_sheetshare and zhao_terrain_sheetseam in one
# chain, so ST_HIT and ST_MISS come out of the block that DEFINES them rather
# than from a played model -- which is what makes the seam's mirrored `StHit`
# localparam a checked claim instead of a copy agreeing with itself. The
# address law is real too: the bench carries a zhao_terrain_stampdepth instance
# and the C++ asks IT for every texel instead of writing `2*v`.
#
# It also fires all nine of the seam's counters plus both of the share's, and
# measures the two numbers the design was chosen on: 1,089 addressable texels
# of 4,096, and 1,091 prefetch cycles per record.
add_test(NAME lint_terrain_sheetseam
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_sheetseam
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_stampdepth.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_sheetseam.sv)
set_tests_properties(lint_terrain_sheetseam PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_surface_sheetshare
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_surface_sheetshare
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sheetshare.sv)
set_tests_properties(lint_surface_sheetshare PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_sheetseam_rtl_directed terrain/sheetseam_rtl_directed.cpp)
target_link_libraries(test_sheetseam_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_sheetseam_rtl_directed
  PREFIX Vtb_sheetseam
  TOP_MODULE tb_sheetseam
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sheet.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/surface/zhao_surface_sheetshare.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_stampdepth.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_sheetseam.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_sheetseam.sv)
add_test(NAME sheetseam_rtl_directed COMMAND test_sheetseam_rtl_directed)
set_tests_properties(sheetseam_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# TERRAIN.BAKEREC -- the patch-bake record producer (core entry I32, closed)
# ---------------------------------------------------------------------------
# NO BENCH WRAPPER, DELIBERATELY. This block has no submodule and all four of
# its neighbours are ready/valid, so the DUT is the real module at its own
# ports -- which is what lets a case hold `io_serving_i` LOW and require the
# seam to be offered nothing. No composition of the real blocks can hold the
# page face dark on purpose, and that interlock is the one whose absence is
# invisible: a dig started before TERRAIN.PAGEIO reaches S_SERVE reads an EMPTY
# no-bake plane, so section 3.3's corner shadow vanishes with every handshake
# and every counter agreeing.
#
# All eleven counters are asserted SILENT and then FIRED from the block's own
# boundary with legal stimulus, so none owes a committed mutant.
add_test(NAME lint_terrain_bakerec
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_bakerec
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bakerec.sv)
set_tests_properties(lint_terrain_bakerec PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_executable(test_bakerec_rtl_directed terrain/bakerec_rtl_directed.cpp)
target_link_libraries(test_bakerec_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_bakerec_rtl_directed
  PREFIX Vzhao_terrain_bakerec
  TOP_MODULE zhao_terrain_bakerec
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_bakerec.sv)
add_test(NAME bakerec_rtl_directed COMMAND test_bakerec_rtl_directed)
set_tests_properties(bakerec_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---------------------------------------------------------------------------
# TERRAIN.LOADQ -- the load queue, against its reference model.
#
# A queue is the block whose test writes itself badly: push some, pop some,
# check the count. That passes against a queue that REORDERS, one that drops a
# job when full while ready lies, and one that hands the loader a record with
# its CRC sheared off -- none of those move a count. So this bench compares the
# SEQUENCE and every FIELD against zref::terrain::LoadQueue, under the same four
# stall patterns the sequencer's differential uses, with both sides busy on the
# same cycle at every occupancy from empty to full.
#
# It verilates the module directly: it has no fabric, no memory and no
# neighbours to play, so a bench around it would be a wrapper with nothing in
# it. The job payload is 243 bits serialised into eight 40-bit M10K words, and
# every field of every job is distinct -- a fixture whose upper words are zero
# cannot tell a working serialiser from one that stops after word 5, and words
# 6 and 7 are where the CRC and the source id live.
add_executable(test_loadq_rtl_directed terrain/loadq_rtl_directed.cpp)
target_link_libraries(test_loadq_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_loadq_rtl_directed
  PREFIX Vzhao_terrain_loadq
  TOP_MODULE zhao_terrain_loadq
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loadq.sv)
add_test(NAME loadq_rtl_directed COMMAND test_loadq_rtl_directed)
set_tests_properties(loadq_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ...and the lint gate, on the module's OWN defaults. An RTL file with no lint
# test is an RTL file nothing has read: a comment line beginning with the
# simulator's name is parsed as a pragma and stops the file lexing, which bit
# this repo six times in one day.
add_test(NAME lint_terrain_loadq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_loadq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loadq.sv)
set_tests_properties(lint_terrain_loadq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# STEP 8 -- THE COMPOSED TERRAIN WORLD PATH.
#
# TERRAIN.SEQ + TERRAIN.RESIDENCY(v2) + TERRAIN.PAGELOADER + TERRAIN.WRITEBACK
# + MEM.HPS.ARBITER in ONE Verilator top, with the harness supplying only the
# frame ring, HPS DDR, the page pool's fabric and the journal doorbell. Every
# block is the real RTL; nothing that exists is modelled.
#
# WHY IT IS EXPECTED TO FIND THINGS. reports/G1D-COMPOSED-ISLAND-20260905.md:
# the first composed test of individually-verified texture blocks found five
# real defects no unit suite could see. Every block composed here already has a
# green suite; what none of them had was a NEIGHBOUR. The three seams this
# reaches for the first time are TERRAIN.SEQ against a real directory,
# TERRAIN.PAGELOADER's completion into that directory, and -- the one that
# matters -- the loader's page-pool WRITES against the writeback's page-pool
# READS, which is the only place T4's barrier can be measured in bytes rather
# than in job order.
#
# The oracle is fed the REAL directory's answers rather than invented ones, so
# the claim is exactly "whatever zhao_terrain_residency_v2 said,
# zhao_terrain_seq did with it what zref::terrain::seq::Sequencer says to do".
# The directory's own answers are checked separately against
# zref::terrain::residency_set_index, so a directory fault cannot be laundered
# into a sequencing verdict.
add_executable(test_world_composed_directed terrain/world_composed_directed.cpp)
target_link_libraries(test_world_composed_directed PRIVATE zhao_harness zhao_zref)
verilate(test_world_composed_directed
  PREFIX Vtb_terrain_world
  TOP_MODULE tb_terrain_world
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_seq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency_v2.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageloader.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_writeback.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jdoorbell.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loadq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pagestream.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipgen.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipfeed.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_cmd.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_world.sv)
add_test(NAME world_composed_directed COMMAND test_world_composed_directed)
set_tests_properties(world_composed_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# ...and the lint gate every other terrain bench has. A comment line beginning
# with the simulator's name is parsed as a pragma and stops the file lexing,
# which happened six times in one day; an RTL file with no lint test is an RTL
# file nothing has read. This one lints the BENCH's top, because the bench is
# where the composition lives and the blocks under it have their own gates.
add_test(NAME lint_terrain_world
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module tb_terrain_world
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_mem_guard.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory/zhao_hps_arbiter.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_seq.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_residency_v2.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pageloader.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_writeback.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_jdoorbell.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_loadq.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pagestream.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipgen.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_mipfeed.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_cmd.sv
                 ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_world.sv)
set_tests_properties(lint_terrain_world PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# Packet B: repaired Texture-V3 boundary registrations
# ---------------------------------------------------------------------------
# This is the single source of truth for every full Packet-B elaboration in
# CMake.  Its order is frozen by architecture Â§12.1: package first, selected
# root last.  Leaf tests below use only their own minimal closures; historical
# island/V1/V2 tests above retain their unversioned source lists.
set(ZHAO_TEXTURE_V3_PACKET_B_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3own.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_island_v3_top.sv
)

# A reduced Packet-B configure is a failed configure, never a skipped test.
foreach(packet_b_source IN LISTS ZHAO_TEXTURE_V3_PACKET_B_SOURCES)
  if(NOT EXISTS "${packet_b_source}")
    message(FATAL_ERROR "required Packet-B source is missing: ${packet_b_source}")
  endif()
endforeach()
set(ZHAO_TEXTURE_V3_INTERFACE_MANIFEST
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_texture_island_v3_top.interface.json)
foreach(packet_b_artifact IN ITEMS
    ${ZHAO_TEXTURE_V3_INTERFACE_MANIFEST}
    ${CMAKE_SOURCE_DIR}/tools/rtl/gen_texture_v3_interface_manifest.py
    ${CMAKE_SOURCE_DIR}/tools/rtl/texture_v3_interface_parser.py
    ${CMAKE_SOURCE_DIR}/tools/rtl/check_texture_v3_interface_manifest.py
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_texture_v3_interface_manifest.py)
  if(NOT EXISTS "${packet_b_artifact}")
    message(FATAL_ERROR "required Packet-B interface artifact is missing: ${packet_b_artifact}")
  endif()
endforeach()

# Closed-schema tests, a temp-only generator execution, and the committed
# artifact checker are independent mandatory gates.  All three run through the
# REQUIRED absolute CPython, and elaboration receives the exact environment
# verilator_bin.exe path rather than an extensionless PATH lookup.
add_test(NAME texture_v3_interface_manifest_tests
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_texture_v3_interface_manifest.py -q)
add_test(NAME texture_v3_registration_static
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_a.py
          PacketAOwnershipAndClosureTests.test_packet_b_source_registrations_are_exact_ordered_and_unique
          PacketAOwnershipAndClosureTests.test_packet_b_source_list_parsers_fire_on_every_structural_mutation
          PacketAOwnershipAndClosureTests.test_aux_assertion_control_mapping_is_exact_unique_and_exclusive
          PacketAOwnershipAndClosureTests.test_observation_successor_cmake_uses_exact_durable_manifest
          PacketAOwnershipAndClosureTests.test_top_profiles_mutants_and_quiet_accounting_are_exact
          PacketAOwnershipAndClosureTests.test_aux_registered_clamp_transport_reaches_all_committed_mutants
          PacketAOwnershipAndClosureTests.test_v3_owner_combine_feedback_cut_keeps_generation_witness
          PacketAOwnershipAndClosureTests.test_v3_owner_retirement_head_is_bounded_and_controlled
          PacketAOwnershipAndClosureTests.test_v3_owner_timing3_notifications_preserve_event_moments
          PacketAOwnershipAndClosureTests.test_v3_owner_timing4_event_boundaries_are_registered
          PacketAOwnershipAndClosureTests.test_material_writeback_cut_reaches_all_committed_mutants
          PacketAOwnershipAndClosureTests.test_rcp_v4_balanced_lzc_direct_oracle_and_mutant_are_exact
          -q)
add_test(NAME texture_v3_interface_manifest_generator
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_SOURCE_DIR}/tools/rtl/gen_texture_v3_interface_manifest.py
          --repo-root ${CMAKE_SOURCE_DIR}
          --verilator "${ZHAO_VERILATOR_LINT}"
          --output ${CMAKE_CURRENT_BINARY_DIR}/texture-v3-interface.generated.json)
add_test(NAME texture_v3_interface_manifest_checker
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_SOURCE_DIR}/tools/rtl/check_texture_v3_interface_manifest.py
          --repo-root ${CMAKE_SOURCE_DIR}
          --manifest ${ZHAO_TEXTURE_V3_INTERFACE_MANIFEST}
          --verilator "${ZHAO_VERILATOR_LINT}")
set_tests_properties(
  texture_v3_interface_manifest_tests
  texture_v3_registration_static
  texture_v3_interface_manifest_generator
  texture_v3_interface_manifest_checker
  PROPERTIES LABELS "fast;lint;packet-b" TIMEOUT 900
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# Full Packet-B integration driver.  The laboratory ABI and production profile
# elaborate the identical 26-source closure at explicit shadow settings.
add_executable(tpb_dir
  texture/texture_island_v3_packet_b_directed.cpp)
target_link_libraries(tpb_dir PRIVATE zhao_harness)
verilate(tpb_dir
  PREFIX Vzhao_texture_island_v3_top
  TOP_MODULE zhao_texture_island_v3_top
  SOURCES ${ZHAO_TEXTURE_V3_PACKET_B_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=1)
add_test(NAME texture_island_v3_packet_b_directed
         COMMAND tpb_dir)

add_executable(tpb_prod
  texture/texture_island_v3_packet_b_directed.cpp)
target_compile_definitions(tpb_prod PRIVATE
  PACKET_B_EXPECT_NO_SHADOWS=1)
target_link_libraries(tpb_prod PRIVATE zhao_harness)
verilate(tpb_prod
  PREFIX Vzhao_texture_island_v3_top
  TOP_MODULE zhao_texture_island_v3_top
  SOURCES ${ZHAO_TEXTURE_V3_PACKET_B_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=0)
add_test(NAME texture_island_v3_packet_b_production_profile
         COMMAND tpb_prod)
set_tests_properties(
  texture_island_v3_packet_b_directed
  texture_island_v3_packet_b_production_profile
  PROPERTIES LABELS "fast;nightly;packet-b" TIMEOUT 1800)

# Top mutants select one production seam immediately before the real top source;
# no copied top module exists.  Every driver has inverse polarity and must emit
# its exact focused diagnostic without any ordinary FAIL diagnostic.
set(ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES
  ${ZHAO_TEXTURE_V3_PACKET_B_SOURCES})
list(LENGTH ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES
  ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCE_COUNT)
math(EXPR ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_INDEX
  "${ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCE_COUNT} - 1")
list(GET ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES
  ${ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_INDEX}
  ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_SOURCE)
if(NOT ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_SOURCE STREQUAL
   "${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_island_v3_top.sv")
  message(FATAL_ERROR "Packet-B mutant closure lost its top-last invariant")
endif()
list(REMOVE_AT ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES
  ${ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_INDEX})
list(APPEND ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES
  ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_island_v3_packet_b_mutants.sv
  ${ZHAO_TEXTURE_V3_PACKET_B_MUTANT_TOP_SOURCE})

function(zhao_packet_b_top_mutant_control
    NAME DRIVER EXPECT_MACRO SELECTOR DIAGNOSTIC)
  string(MD5 target_hash "${NAME}")
  string(SUBSTRING "${target_hash}" 0 8 target_hash)
  set(target tpbm_${target_hash})
  add_executable(${target} texture/${DRIVER})
  target_compile_definitions(${target} PRIVATE ${EXPECT_MACRO}=1)
  verilate(${target}
    PREFIX Vzhao_texture_island_v3_top
    TOP_MODULE zhao_texture_island_v3_top
    SOURCES ${ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES}
    VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=1 -D${SELECTOR})
  add_test(NAME texture_island_v3_packet_b_${NAME}_control COMMAND ${target})
  set_tests_properties(texture_island_v3_packet_b_${NAME}_control PROPERTIES
    LABELS "fast;nightly;packet-b;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL")
endfunction()
zhao_packet_b_top_mutant_control(
  dispatch_index_route texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_INDEX_ROUTE_MUTANT
  ZHAO_PACKET_B_MUTANT_DISPATCH_INDEX_ROUTE
  "packet-b dispatcher-index-route mutant FIRED")
zhao_packet_b_top_mutant_control(
  aux_as_sample2 texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_AUX_AS_SAMPLE2
  ZHAO_PACKET_B_MUTANT_AUX_AS_SAMPLE2
  "packet-b AUX-as-sample2 mutant FIRED")
zhao_packet_b_top_mutant_control(
  retire_context_truncation texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_RETIRE_TRUNCATION
  ZHAO_PACKET_B_MUTANT_RETIRE_CONTEXT_TRUNCATION
  "packet-b retire-context-truncation mutant FIRED")
zhao_packet_b_top_mutant_control(
  frame_clear_over_fault texture_island_v3_packet_b_fullctx.cpp
  PACKET_B_EXPECT_CLEAR_OVER_FAULT
  ZHAO_PACKET_B_MUTANT_FRAME_CLEAR_OVER_FAULT
  "packet-b clear-over-fault mutant FIRED")
zhao_packet_b_top_mutant_control(
  owner_mask_generation texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_OWNER_MASK_LIFETIME
  ZHAO_PACKET_B_MUTANT_OWNER_MASK_GENERATION
  "packet-b owner-mask-generation mutant FIRED")
zhao_packet_b_top_mutant_control(
  cache_sidx3 texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME
  ZHAO_PACKET_B_MUTANT_CACHE_SIDX3
  "packet-b cache-sidx3 mutant FIRED")
zhao_packet_b_top_mutant_control(
  bilerp_identity texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL
  ZHAO_PACKET_B_MUTANT_BILERP_IDENTITY
  "packet-b bilerp-identity mutant FIRED")
zhao_packet_b_top_mutant_control(
  shadow_one_sided texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_SHADOW_CORRUPTION
  ZHAO_PACKET_B_MUTANT_SHADOW_ONE_SIDED
  "packet-b one-sided-shadow mutant FIRED")
zhao_packet_b_top_mutant_control(
  rsp_drop_observation texture_island_v3_packet_b_directed.cpp
  PACKET_B_EXPECT_RSP_DROP_OBSERVATION
  ZHAO_PACKET_B_MUTANT_RSP_DROP_OBSERVATION
  "packet-b response-drop detector mutant FIRED")

# Math/join leaves for which an executable driver exists.  METJOIN V2 is an
# observation-only successor with the predecessor's public data interface, so
# the same directed source is compiled under its established generated prefix.
add_executable(pb_metaj texture/metajoin_directed.cpp)
target_link_libraries(pb_metaj PRIVATE zhao_harness)
verilate(pb_metaj
  PREFIX Vzhao_texture_metajoin
  TOP_MODULE zhao_texture_metajoin_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_metajoin_v2_directed COMMAND pb_metaj)

add_executable(pb_uvj
  texture/texture_uv_join_v2_directed.cpp)
target_link_libraries(pb_uvj PRIVATE zhao_harness)
verilate(pb_uvj
  PREFIX Vtb_uv_join_v2_pair
  TOP_MODULE tb_uv_join_v2_pair
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join_v2.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_uv_join_v2_mutants.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_uv_join_v2_pair.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_uv_join_v2_directed COMMAND pb_uvj)

# Durable standalone differential for Packet-B's two observation-only math
# successors.  The durable manifest owns the nine ordered SystemVerilog paths;
# the C++ driver is the pair's tenth artifact.  Shared queue/arithmetic leaves
# precede V3/V4, both perspective implementations precede the test top, and the
# test top is last.
set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/raster/packetb_observation_successors.sources.txt)
if(NOT EXISTS "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-B observation source manifest is missing: ${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}")
endif()

# file(STRINGS) is the source authority.  The raw view independently rejects
# blank records, because file(STRINGS) would otherwise silently discard them.
file(READ "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}"
  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW
  "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW
  "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_BODY
  "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_RAW}")
if(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_BODY STREQUAL "" OR
   ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST_BODY MATCHES
     "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR
    "Packet-B observation source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}"
  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES)

set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES)
set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR
      "invalid Packet-B observation source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SEEN
    "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR
      "duplicate Packet-B observation source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-B observation source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
list(LENGTH ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES
  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCE_COUNT)
if(NOT ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCE_COUNT EQUAL 9)
  message(FATAL_ERROR
    "Packet-B observation source manifest must contain exactly 9 SV paths")
endif()
list(GET ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES -1
  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_LAST_SOURCE)
if(NOT ZHAO_PACKETB_OBSERVATION_SUCCESSOR_LAST_SOURCE STREQUAL
   "tests/raster/tb_packetb_observation_successors.sv")
  message(FATAL_ERROR
    "Packet-B observation source manifest must place its test top last")
endif()
add_executable(pb_obs
  raster/packetb_observation_successors.cpp)
verilate(pb_obs
  PREFIX Vtb_packetb_observation_successors
  TOP_MODULE tb_packetb_observation_successors
  SOURCES ${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES}
  VERILATOR_ARGS --assert)
add_test(NAME packetb_observation_successors
         COMMAND pb_obs)
set_tests_properties(packetb_observation_successors PROPERTIES
  LABELS "fast;nightly;packet-b" TIMEOUT 1200)

set(ZHAO_RCP24_V4_TIMING_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/raster/rcp24_v4_timing.sources.txt)
file(STRINGS "${ZHAO_RCP24_V4_TIMING_MANIFEST}"
  ZHAO_RCP24_V4_TIMING_RELATIVE_SOURCES)
if(NOT ZHAO_RCP24_V4_TIMING_RELATIVE_SOURCES STREQUAL
   "fpga/rtl/common/zhao_render_texture_pkg.sv;fpga/rtl/field/zhao_field_rcp24_rom.sv;fpga/rtl/raster/zhao_raster_ticketq.sv;fpga/rtl/raster/zhao_raster_ticketq_rh.sv;fpga/rtl/raster/zhao_raster_rcp24_mul.sv;tests/mutants/zhao_raster_rcp24_v4_timing_mutants.sv;fpga/rtl/raster/zhao_raster_rcp24_v4.sv")
  message(FATAL_ERROR "RCP24 V4 timing source manifest is not the exact seven-file closure")
endif()
set(ZHAO_RCP24_V4_TIMING_SOURCES)
foreach(relative_source IN LISTS ZHAO_RCP24_V4_TIMING_RELATIVE_SOURCES)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing RCP24 V4 timing source: ${relative_source}")
  endif()
  list(APPEND ZHAO_RCP24_V4_TIMING_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

add_executable(pb_rcp4time raster/raster_rcp24_v4_timing_directed.cpp)
target_link_libraries(pb_rcp4time PRIVATE zhao_harness zhao_zref)
verilate(pb_rcp4time
  PREFIX Vzhao_raster_rcp24_v4
  TOP_MODULE zhao_raster_rcp24_v4
  SOURCES ${ZHAO_RCP24_V4_TIMING_SOURCES}
  VERILATOR_ARGS --assert)
add_test(NAME raster_rcp24_v4_timing_directed COMMAND pb_rcp4time)

add_executable(pb_rcp4time_mut raster/raster_rcp24_v4_timing_directed.cpp)
target_compile_definitions(pb_rcp4time_mut PRIVATE
  ZHAO_RCP_V4_EXPECT_LZC_MUTANT)
target_link_libraries(pb_rcp4time_mut PRIVATE zhao_harness zhao_zref)
verilate(pb_rcp4time_mut
  PREFIX Vzhao_raster_rcp24_v4
  TOP_MODULE zhao_raster_rcp24_v4
  SOURCES ${ZHAO_RCP24_V4_TIMING_SOURCES}
  VERILATOR_ARGS --assert -DZHAO_RCP_V4_LZC_MUTANT_REVERSE)
add_test(NAME raster_rcp24_v4_lzc_orientation_mutant COMMAND pb_rcp4time_mut)
set_tests_properties(
  raster_rcp24_v4_timing_directed
  raster_rcp24_v4_lzc_orientation_mutant
  PROPERTIES LABELS "fast;nightly;packet-b;g8a-timing" TIMEOUT 1200)
set_tests_properties(raster_rcp24_v4_lzc_orientation_mutant PROPERTIES
  LABELS "fast;nightly;packet-b;g8a-timing;mutant"
  PASS_REGULAR_EXPRESSION "DETECTED")

# Descriptor / expander / resolver production leaves.
add_executable(pb_desc
  texture/texture_early_desc_v2_directed.cpp)
target_link_libraries(pb_desc PRIVATE zhao_harness)
verilate(pb_desc
  PREFIX Vzhao_texture_early_desc_v2
  TOP_MODULE zhao_texture_early_desc_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_early_desc_v2_directed COMMAND pb_desc)

add_executable(pb_exp
  texture/texture_frag_expand_v2_directed.cpp)
target_link_libraries(pb_exp PRIVATE zhao_harness)
verilate(pb_exp
  PREFIX Vzhao_texture_frag_expand_v2
  TOP_MODULE zhao_texture_frag_expand_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_frag_expand_v2_directed COMMAND pb_exp)

add_executable(pb_bind
  texture/texture_binding_resolver_v2_directed.cpp)
target_link_libraries(pb_bind PRIVATE zhao_harness)
verilate(pb_bind
  PREFIX Vzhao_texture_binding_resolver_v2
  TOP_MODULE zhao_texture_binding_resolver_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_binding_resolver_v2_directed
         COMMAND pb_bind)

# Planner / cache / terminal collector production leaves.
add_executable(pb_plan
  texture/texture_tmu_plan_v2_directed.cpp)
target_link_libraries(pb_plan PRIVATE zhao_harness)
verilate(pb_plan
  PREFIX Vzhao_texture_tmu_plan_v2
  TOP_MODULE zhao_texture_tmu_plan_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_tmu_plan_v2_directed COMMAND pb_plan)

add_executable(pb_cache
  texture/texture_cache_pipe_v2_directed.cpp)
target_link_libraries(pb_cache PRIVATE zhao_harness)
verilate(pb_cache
  PREFIX Vzhao_texture_cache_pipe_v2
  TOP_MODULE zhao_texture_cache_pipe_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_cache_pipe_v2_directed COMMAND pb_cache)

add_executable(pb_rsp
  texture/texture_rsp_dispatch_v2_directed.cpp)
target_link_libraries(pb_rsp PRIVATE zhao_harness)
verilate(pb_rsp
  PREFIX Vzhao_texture_rsp_dispatch_v2
  TOP_MODULE zhao_texture_rsp_dispatch_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_rsp_dispatch_v2_directed
         COMMAND pb_rsp)

# Response / AUX production leaves and the cycle-exact legacy/V2 controls.
add_executable(pb_mos
  texture/texture_mosaic_v2_directed.cpp)
target_link_libraries(pb_mos PRIVATE zhao_harness)
verilate(pb_mos
  PREFIX Vzhao_texture_mosaic_v2
  TOP_MODULE zhao_texture_mosaic_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_mosaic_v2_directed COMMAND pb_mos)

add_executable(pb_mosp
  texture/texture_mosaic_v2_pair_directed.cpp)
target_link_libraries(pb_mosp PRIVATE zhao_harness)
verilate(pb_mosp
  PREFIX Vtb_texture_mosaic_v2_pair
  TOP_MODULE tb_texture_mosaic_v2_pair
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic_v2.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl/tb_texture_mosaic_v2_pair.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_mosaic_v2_pair_directed
         COMMAND pb_mosp)

add_executable(pb_bil
  texture/texture_bilerp_lane_v2_directed.cpp)
target_link_libraries(pb_bil PRIVATE zhao_harness)
verilate(pb_bil
  PREFIX Vzhao_texture_bilerp_lane_v2
  TOP_MODULE zhao_texture_bilerp_lane_v2
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_bilerp_lane_v2_directed
         COMMAND pb_bil)

add_executable(pb_bilp
  texture/texture_bilerp_lane_v2_pair_directed.cpp)
target_link_libraries(pb_bilp PRIVATE zhao_harness)
verilate(pb_bilp
  PREFIX Vtb_texture_bilerp_lane_v2_pair
  TOP_MODULE tb_texture_bilerp_lane_v2_pair
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl/tb_texture_bilerp_lane_v2_pair.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_bilerp_lane_v2_pair_directed
         COMMAND pb_bilp)

add_executable(pb_pal
  texture/texture_palette_res_v2_directed.cpp)
target_link_libraries(pb_pal PRIVATE zhao_harness)
verilate(pb_pal
  PREFIX Vzhao_texture_palette_res_v2
  TOP_MODULE zhao_texture_palette_res_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_palette_res_v2_directed
         COMMAND pb_pal)

add_executable(pb_aux
  texture/texture_aux_pipe_v2_directed.cpp)
target_link_libraries(pb_aux PRIVATE zhao_harness)
verilate(pb_aux
  PREFIX Vzhao_texture_aux_pipe_v2
  TOP_MODULE zhao_texture_aux_pipe_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_aux_pipe_v2_directed COMMAND pb_aux)

# Packet B/Timing4: AUX registered-fault and borrow controls.
set(ZHAO_AUX_T4_PIPE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/aux_pipe_v2_timing4.sources.txt)
set(ZHAO_AUX_T4_DIV_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/aux_div6_timing4.sources.txt)
file(STRINGS "${ZHAO_AUX_T4_PIPE_MANIFEST}" ZHAO_AUX_T4_PIPE_RELATIVE_SOURCES)
file(STRINGS "${ZHAO_AUX_T4_DIV_MANIFEST}" ZHAO_AUX_T4_DIV_RELATIVE_SOURCES)
if(NOT ZHAO_AUX_T4_PIPE_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_texture_aux_timing4_mutants.sv;fpga/rtl/texture/zhao_texture_aux_div6.sv;fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv")
  message(FATAL_ERROR "Timing4 AUX pipe source manifest is not exact and ordered")
endif()
if(NOT ZHAO_AUX_T4_DIV_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_texture_aux_timing4_mutants.sv;fpga/rtl/texture/zhao_texture_aux_div6.sv")
  message(FATAL_ERROR "Timing4 AUX divider source manifest is not exact and ordered")
endif()
set(ZHAO_AUX_T4_PIPE_SOURCES)
set(ZHAO_AUX_T4_DIV_SOURCES)
foreach(relative_source IN LISTS ZHAO_AUX_T4_PIPE_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)" OR
     NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "invalid Timing4 AUX pipe source: ${relative_source}")
  endif()
  list(APPEND ZHAO_AUX_T4_PIPE_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
foreach(relative_source IN LISTS ZHAO_AUX_T4_DIV_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)" OR
     NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "invalid Timing4 AUX divider source: ${relative_source}")
  endif()
  list(APPEND ZHAO_AUX_T4_DIV_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_aux_t4_pipe_control TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} texture/texture_aux_pipe_v2_directed.cpp)
  target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  verilate(${TARGET}
    PREFIX Vzhao_texture_aux_pipe_v2
    TOP_MODULE zhao_texture_aux_pipe_v2
    SOURCES ${ZHAO_AUX_T4_PIPE_SOURCES}
    VERILATOR_ARGS --assert -D${SV_SELECTOR})
  add_test(NAME texture_aux_pipe_v2_timing4_${NAME}_control COMMAND ${TARGET})
  # NO FAIL_REGULAR_EXPRESSION here, deliberately. This is an inverse-polarity
  # control: the mutant makes exactly one zhao::check fail, and that helper
  # prints "FAIL: <what>". The driver's own signature test is the gate -- it
  # requires exactly one failure with the exact observed-fact pair, and prints
  # the named diagnostic only then. A "FAIL" regex here fails the control for
  # containing the evidence it exists to produce.
  set_tests_properties(texture_aux_pipe_v2_timing4_${NAME}_control PROPERTIES
    LABELS "fast;nightly;mutant;packet-b;packet-f" TIMEOUT 600
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}")
endfunction()
zhao_aux_t4_pipe_control(
  pb_aux_t4d degenerate ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP
  ZHAO_AUX_T4_DEGENERATE_MUTANT_CONTROL
  "AUX Timing4 A0 degenerate mutant FIRED exactly once")
zhao_aux_t4_pipe_control(
  pb_aux_t4f input_fault ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP
  ZHAO_AUX_T4_INPUT_FAULT_MUTANT_CONTROL
  "AUX Timing4 A0 input-fault mutant FIRED exactly once")

add_executable(auxdiv_t4b texture/texture_aux_div6_directed.cpp)
target_compile_definitions(auxdiv_t4b PRIVATE ZHAO_EXPECT_AUX_T4_BORROW_MUTANT=1)
target_link_libraries(auxdiv_t4b PRIVATE zhao_harness)
verilate(auxdiv_t4b
  PREFIX Vzhao_texture_aux_div6
  TOP_MODULE zhao_texture_aux_div6
  SOURCES ${ZHAO_AUX_T4_DIV_SOURCES}
  VERILATOR_ARGS --assert -DZHAO_AUX_T4_MUTANT_BORROW_REVERSE)
add_test(NAME texture_aux_div6_timing4_borrow_control COMMAND auxdiv_t4b)
set_tests_properties(texture_aux_div6_timing4_borrow_control PROPERTIES
  LABELS "fast;nightly;mutant;packet-b;packet-f" TIMEOUT 600
  PASS_REGULAR_EXPRESSION "AUX Timing4 divider borrow mutant DETECTED"
  FAIL_REGULAR_EXPRESSION "FAIL")

foreach(aux_t4_collision IN ITEMS sv cpp)
  add_test(NAME texture_aux_timing4_${aux_t4_collision}_selector_collision
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_aux_timing4_controls.py
      --collision ${aux_t4_collision}
      --verilator "${ZHAO_VERILATOR_LINT}" --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(texture_aux_timing4_${aux_t4_collision}_selector_collision
    PROPERTIES LABELS "fast;mutant;packet-b;packet-f" TIMEOUT 180
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()
add_test(NAME texture_aux_timing4_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_aux_timing4_controls.py -q)
set_tests_properties(texture_aux_timing4_registration_static PROPERTIES
  LABELS "fast;packet-b;packet-f" TIMEOUT 180)
set(ZHAO_AUX_TIMING4_REQUIRED_TESTS
  texture_aux_pipe_v2_timing4_degenerate_control
  texture_aux_pipe_v2_timing4_input_fault_control
  texture_aux_div6_timing4_borrow_control
  texture_aux_timing4_sv_selector_collision
  texture_aux_timing4_cpp_selector_collision
  texture_aux_timing4_registration_static)
list(LENGTH ZHAO_AUX_TIMING4_REQUIRED_TESTS ZHAO_AUX_TIMING4_REQUIRED_TEST_COUNT)
if(NOT ZHAO_AUX_TIMING4_REQUIRED_TEST_COUNT EQUAL 6)
  message(FATAL_ERROR "Timing4 AUX CTest inventory must contain exactly 6 names")
endif()
foreach(required_aux_t4_test IN LISTS ZHAO_AUX_TIMING4_REQUIRED_TESTS)
  if(NOT TEST "${required_aux_t4_test}")
    message(FATAL_ERROR "required Timing4 AUX CTest is absent: ${required_aux_t4_test}")
  endif()
endforeach()

# Material V3 against R9 and the corrected zref authority.
add_executable(pb_mat
  texture/material_combine_v3_diff.cpp)
target_link_libraries(pb_mat PRIVATE zhao_harness zhao_zref)
verilate(pb_mat
  PREFIX Vzhao_texture_material_combine_v3
  TOP_MODULE zhao_texture_material_combine_v3
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
  VERILATOR_ARGS --assert)
add_test(NAME material_combine_v3_diff COMMAND pb_mat)

set_tests_properties(
  texture_metajoin_v2_directed texture_uv_join_v2_directed
  texture_early_desc_v2_directed texture_frag_expand_v2_directed
  texture_binding_resolver_v2_directed texture_tmu_plan_v2_directed
  texture_cache_pipe_v2_directed texture_rsp_dispatch_v2_directed
  texture_mosaic_v2_directed texture_mosaic_v2_pair_directed
  texture_bilerp_lane_v2_directed texture_bilerp_lane_v2_pair_directed
  texture_palette_res_v2_directed texture_aux_pipe_v2_directed
  material_combine_v3_diff
  PROPERTIES LABELS "fast;nightly;packet-b" TIMEOUT 1200)

# Palette elaboration guards are executable controls, not lint-only claims.
function(zhao_palette_v2_parameter_control NAME PARAMETER FIRE_LABEL)
  string(MD5 target_hash "${NAME}")
  string(SUBSTRING "${target_hash}" 0 8 target_hash)
  set(target tpv2_${target_hash})
  add_executable(${target}
    texture/texture_palette_res_v2_parameter_control.cpp)
  target_link_libraries(${target} PRIVATE zhao_harness)
  verilate(${target}
    PREFIX Vzhao_texture_palette_res_v2
    TOP_MODULE zhao_texture_palette_res_v2
    SOURCES
      ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res_v2.sv
    VERILATOR_ARGS --assert ${PARAMETER})
  add_test(NAME texture_palette_res_v2_${NAME}_control COMMAND ${target})
  set_tests_properties(texture_palette_res_v2_${NAME}_control PROPERTIES
    LABELS "fast;nightly;packet-b;mutant" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ZHAO_PALETTE_V2_PARAM_FIRE\\[${FIRE_LABEL}\\]"
    FAIL_REGULAR_EXPRESSION "FAIL:")
endfunction()
zhao_palette_v2_parameter_control(tuple66 -GTOKW=17 TUPLE66)
zhao_palette_v2_parameter_control(slots_power_of_two -GSLOTS=3 SLOTS_POWER_OF_TWO)
zhao_palette_v2_parameter_control(entries256_idxw8 -GENTRIES=128 ENTRIES256_IDXW8)
zhao_palette_v2_parameter_control(genw8 -GGENW=7 GENW8)

# The AUX lifetime-credit mutant is driven by the ordinary directed suite.  Its
# CTest polarity is inverted and pinned to the focused boundary diagnostic, so
# an unrelated later failure cannot masquerade as the intended fire control.
add_executable(pb_auxcred
  texture/texture_aux_pipe_v2_directed.cpp)
target_compile_definitions(pb_auxcred PRIVATE
  ZHAO_AUX_CREDIT_MUTANT_CONTROL=1)
target_link_libraries(pb_auxcred PRIVATE zhao_harness)
verilate(pb_auxcred
  PREFIX Vzhao_texture_aux_pipe_v2
  TOP_MODULE zhao_texture_aux_pipe_v2_credit_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_aux_pipe_v2_credit_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_aux_pipe_v2_credit_control
         COMMAND pb_auxcred)
set_tests_properties(texture_aux_pipe_v2_credit_control PROPERTIES
  LABELS "fast;nightly;packet-b;mutant" TIMEOUT 600
  PASS_REGULAR_EXPRESSION "AUX credit lifetime mutant FIRED exactly once")

# Each unreachable AUX detector has its own production-shaped committed mutant.
# The shared driver is compiled under one common class prefix, while the test's
# runtime control selects the stimulus which must emit that mutant's unique
# production assertion label and increment the shipped counter exactly.
set(ZHAO_AUX_V2_ASSERT_FIRE_LABELS
  CREDIT_BOUND
  OFFER_BOUND
  ISSUED_BOUND
  RETURN_BOUND
  FIXED_PRODUCER_ROOM
  OWED_RESPONSE_ROOM
  ISSUE_BEFORE_LOCAL_RETURN
)
function(zhao_aux_v2_assertion_control NAME CONTROL FIRE_LABEL TOP SOURCE)
  # Keep CMake target and generated prefix short: Quartus/Verilator dependencies
  # live under an already-long absolute checkout, and Windows' 260-character
  # path ceiling otherwise rejects generated __Syms__Slow.cpp files.
  set(target t_auxv2_a${CONTROL})
  set(wrong_fire_labels ${ZHAO_AUX_V2_ASSERT_FIRE_LABELS})
  list(REMOVE_ITEM wrong_fire_labels "${FIRE_LABEL}")
  list(JOIN wrong_fire_labels "|" wrong_fire_label_pattern)
  add_executable(${target} texture/texture_aux_pipe_v2_assertion_control.cpp)
  target_link_libraries(${target} PRIVATE zhao_harness)
  verilate(${target}
    PREFIX Vauxv2_assert
    TOP_MODULE ${TOP}
    SOURCES
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
      ${CMAKE_SOURCE_DIR}/tests/mutants/${SOURCE}
    VERILATOR_ARGS --assert)
  add_test(NAME texture_aux_pipe_v2_assert_${NAME}_control
           COMMAND ${target} --control=${CONTROL})
  set_tests_properties(texture_aux_pipe_v2_assert_${NAME}_control PROPERTIES
    LABELS "fast;nightly;packet-b;mutant" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ZHAO_AUX_V2_ASSERT_FIRE\\[${FIRE_LABEL}\\]"
    FAIL_REGULAR_EXPRESSION
      "FAIL:;ZHAO_AUX_V2_ASSERT_FIRE\\[(${wrong_fire_label_pattern})\\]")
endfunction()
zhao_aux_v2_assertion_control(
  credit_bound 1 CREDIT_BOUND
  zhao_texture_aux_pipe_v2_assertion_control_mutant
  zhao_texture_aux_pipe_v2_assertion_control_mutant.sv)
zhao_aux_v2_assertion_control(
  offer_bound 2 OFFER_BOUND
  zhao_texture_aux_pipe_v2_offer_bound_mutant
  zhao_texture_aux_pipe_v2_offer_bound_mutant.sv)
zhao_aux_v2_assertion_control(
  issued_bound 3 ISSUED_BOUND
  zhao_texture_aux_pipe_v2_issued_bound_mutant
  zhao_texture_aux_pipe_v2_issued_bound_mutant.sv)
zhao_aux_v2_assertion_control(
  return_bound 4 RETURN_BOUND
  zhao_texture_aux_pipe_v2_return_bound_mutant
  zhao_texture_aux_pipe_v2_return_bound_mutant.sv)
zhao_aux_v2_assertion_control(
  fixed_producer_room 5 FIXED_PRODUCER_ROOM
  zhao_texture_aux_pipe_v2_fixed_room_mutant
  zhao_texture_aux_pipe_v2_fixed_room_mutant.sv)
zhao_aux_v2_assertion_control(
  owed_response_room 6 OWED_RESPONSE_ROOM
  zhao_texture_aux_pipe_v2_owed_room_mutant
  zhao_texture_aux_pipe_v2_owed_room_mutant.sv)
zhao_aux_v2_assertion_control(
  issue_before_local_return 7 ISSUE_BEFORE_LOCAL_RETURN
  zhao_texture_aux_pipe_v2_issue_order_mutant
  zhao_texture_aux_pipe_v2_issue_order_mutant.sv)

# Material R9 controls: each compile definition selects the correspondingly
# renamed top declared by the committed multi-mutant source.
function(zhao_material_v3_mutant_control NAME MACRO TOP)
  string(MD5 target_hash "${TOP}")
  string(SUBSTRING "${target_hash}" 0 8 target_hash)
  set(target tmv3_${target_hash})
  add_executable(${target} texture/material_combine_v3_diff.cpp)
  target_compile_definitions(${target} PRIVATE ${MACRO}=1)
  target_link_libraries(${target} PRIVATE zhao_harness zhao_zref)
  verilate(${target}
    PREFIX Vmatv3_mut
    TOP_MODULE ${TOP}
    SOURCES
      ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
      ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_material_combine_v3_mutants.sv
    VERILATOR_ARGS --assert)
  add_test(NAME material_combine_v3_${NAME}_control COMMAND ${target})
  set_tests_properties(material_combine_v3_${NAME}_control PROPERTIES
    LABELS "fast;nightly;packet-b;mutant" TIMEOUT 900)
endfunction()
zhao_material_v3_mutant_control(
  stale_alpha MATERIAL_V3_MUTANT_ALPHA
  zhao_texture_material_combine_v3_stale_alpha_arithmetic_mutant)
zhao_material_v3_mutant_control(
  binary_mask MATERIAL_V3_MUTANT_MASK
  zhao_texture_material_combine_v3_binary_mask_mutant)
zhao_material_v3_mutant_control(
  double_round_mod2x MATERIAL_V3_MUTANT_MOD2X
  zhao_texture_material_combine_v3_double_round_mod2x_mutant)
zhao_material_v3_mutant_control(
  unit_detail_first MATERIAL_V3_MUTANT_DETAIL
  zhao_texture_material_combine_v3_unit_detail_first_mutant)
zhao_material_v3_mutant_control(
  universal_count_zero MATERIAL_V3_MUTANT_COUNT_ZERO
  zhao_texture_material_combine_v3_universal_count_zero_mutant)
zhao_material_v3_mutant_control(
  aux_as_sample2 MATERIAL_V3_MUTANT_AUX_SAMPLE2
  zhao_texture_material_combine_v3_aux_as_sample2_mutant)
zhao_material_v3_mutant_control(
  omit_required_status MATERIAL_V3_MUTANT_STATUS_OMIT
  zhao_texture_material_combine_v3_omit_required_status_mutant)
zhao_material_v3_mutant_control(
  drop_raw_index MATERIAL_V3_MUTANT_INDEX_DROP
  zhao_texture_material_combine_v3_drop_raw_index_mutant)
zhao_material_v3_mutant_control(
  reconstruct_raw_index MATERIAL_V3_MUTANT_INDEX_RGB
  zhao_texture_material_combine_v3_reconstruct_raw_index_mutant)
zhao_material_v3_mutant_control(
  phase_drop_writeback MATERIAL_V3_MUTANT_PHASE_DROP
  zhao_texture_material_combine_v3_phase_drop_writeback_mutant)
zhao_material_v3_mutant_control(
  phase_reissue_schedule MATERIAL_V3_MUTANT_PHASE_REISSUE
  zhao_texture_material_combine_v3_phase_reissue_schedule_mutant)
zhao_material_v3_mutant_control(
  skip_s_capture MATERIAL_V3_MUTANT_SKIP_S
  zhao_texture_material_combine_v3_skip_s_capture_mutant)
zhao_material_v3_mutant_control(
  skip_f_finish MATERIAL_V3_MUTANT_SKIP_F
  zhao_texture_material_combine_v3_skip_f_finish_mutant)

# THE POSITIVE CONTROL for the M3 WB->Q continuation bypass, and the only one
# in this file that mutates PRODUCTION rather than a renamed copy.
#
# The bypass forwards a retiring non-final phase straight to Q when the
# continuation queue is empty, and must therefore SUPPRESS the push that would
# otherwise enqueue the same phase. Flipping CONT_BYPASS_SUPPRESS_PUSH enqueues
# the very phase it also forwards, so the phase is issued twice. Every colour
# still matches -- recomputing a product is idempotent -- which is precisely the
# "counters see what pictures cannot" case: only the phase accounting sees it.
#
# The mutation is selected by a plain `ifdef, not a function-like `define, so
# Verilator's -D actually reaches it. The negative control that law requires is
# the thirteen tests above plus the healthy differential, which compile these
# same sources with the macro UNDEFINED and pass.
#
# Polarity is inverted: this runs the HEALTHY test list against a mutated
# design, so it PASSES when the accounting fires. The regex names the DIRECT
# signature of the fault -- nine tags retired twice -- rather than any failure
# or a nonzero exit, so a build break, a missing DLL or an unrelated regression
# cannot masquerade as detection. Observed firing before this was registered:
#   FAIL: no tag retired twice: expected 0x0, got 0x9
#   FAIL: every typed result matches independent R9: expected 0x0, got 0x2
#   [material_combine_v3_diff] 18/46 checks FAILED
add_executable(tmv3_cont_bypass texture/material_combine_v3_diff.cpp)
target_link_libraries(tmv3_cont_bypass PRIVATE zhao_harness zhao_zref)
verilate(tmv3_cont_bypass
  PREFIX Vzhao_texture_material_combine_v3
  TOP_MODULE zhao_texture_material_combine_v3
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
  VERILATOR_ARGS --assert -DZHAO_MATV3_MUTANT_CONT_BYPASS_DOUBLE_ISSUE)
add_test(NAME material_combine_v3_cont_bypass_double_issue_control
  COMMAND tmv3_cont_bypass)
set_tests_properties(material_combine_v3_cont_bypass_double_issue_control
  PROPERTIES LABELS "fast;nightly;packet-b;mutant" TIMEOUT 900
  PASS_REGULAR_EXPRESSION "FAIL: no tag retired twice")

# THE POSITIVE CONTROL for the section 8.4 WB-final -> completion-read
# forwarding, built the same way and for the same reason as the one above.
#
# The bypass launches a completion read directly from the final phase retiring
# on this edge, and must suppress the doneq push that would otherwise enqueue
# the same context. Flipping DONE_BYPASS_SUPPRESS_PUSH enqueues the context it
# also forwards, so the JOB completes twice -- the tail counterpart of the
# continuation double-issue above.
#
# Watched fire before being registered:
#   FAIL: jobs_completed counts the output handshake exactly: expected 0x7, got 0xB
#   FAIL: no tag retired twice: expected 0x0, got 0x4
#   FAIL: no tag was lost: expected 0x0, got 0xEC
#
# Seven jobs accepted, eleven completed. The regex names the JOB-level
# signature, which is this fault's own -- the continuation control above is
# pinned to the phase-level one, so the two cannot pass on each other's
# evidence.
#
# Negative control: the same sources with the macro UNDEFINED give 201 checks
# passed and exit 0.
add_executable(tmv3_done_bypass texture/material_combine_v3_diff.cpp)
target_link_libraries(tmv3_done_bypass PRIVATE zhao_harness zhao_zref)
verilate(tmv3_done_bypass
  PREFIX Vzhao_texture_material_combine_v3
  TOP_MODULE zhao_texture_material_combine_v3
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
  VERILATOR_ARGS --assert -DZHAO_MATV3_MUTANT_DONE_BYPASS_DOUBLE_ISSUE)
add_test(NAME material_combine_v3_done_bypass_double_issue_control
  COMMAND tmv3_done_bypass)
set_tests_properties(material_combine_v3_done_bypass_double_issue_control
  PROPERTIES LABELS "fast;nightly;packet-b;mutant" TIMEOUT 900
  PASS_REGULAR_EXPRESSION
    "FAIL: jobs_completed counts the output handshake exactly")

# Committed mutant copies go stale silently: production is edited, the copy
# keeps compiling, keeps passing its own control, and quietly starts describing
# a machine that no longer exists. Found twice on 2026-09-16, both by accident,
# and the second time the stale copy reported "exactly 16 live: expected 0x10,
# got 0x11" -- which reads like a credit overflow in shipped RTL and is nothing
# of the kind. Nothing in the tree was watching. Now something is.
# `uncashed_cheques.py` has existed since 2026-09-09, has caught real money
# every time it was run, and was NEVER REGISTERED -- so nothing ran it back.
# That is the exact shape the tool itself detects, one level up: a thing built
# is not a thing installed, and a detector nobody invokes is a detector that
# reports nothing forever. Registered 2026-09-18, the day its new check 4
# found the shared projector composed, fitted at 102.19 MHz, and adopted by
# no production top.
#
# `--gate` is a RATCHET and is deliberately green today: it refuses only
# UNDECLARED rows, never the 16 PENDING ones, because a written deferral is
# what the manifest is for. Its fire case is asserted at import (see
# gate_failures' controls), so this is not a test that has never been seen to
# fail.
# setup_path_census.py parses a Quartus *.setup.rpt. Running it with no
# arguments prints usage and returns 2, which is NOT what this test is for --
# the value is that its import-time self_test() runs, and that self-test
# asserts the parser still finds `Summary of Paths` AND still rejects the two
# delay-detail tables whose rows share the same column shape. Confusing those
# three tables turned 200 negative paths into "1,600 paths, 1,400 of them
# meeting timing" on 2026-09-18. The report is run by hand; this test exists
# so the parser cannot rot between runs.
# closure_liveness audits every fit receipt for sources it DECLARES and never
# ELABORATES. Registered for its import-time controls, which pin the one
# distinction the audit rests on: Info 12128 "Elaborating entity" means the
# module is in the design; Info 12023 "Found entity" only means the file was
# analysed. Conflating them is how eight geometry blocks sat in
# zhao_shell_top_v2's 97-source closure reading as measured.
add_test(NAME closure_liveness_selftest
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/closure_liveness.py)
set_tests_properties(closure_liveness_selftest PROPERTIES
  LABELS "fast;lint;budget" TIMEOUT 300)
add_test(NAME setup_path_census_selftest
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/setup_path_census.py
    ${CMAKE_SOURCE_DIR}/reports/synthesis/blockpaths/zhao_shell_top_v2@packet-h-mulstage.setup.rpt)
set_tests_properties(setup_path_census_selftest PROPERTIES
  LABELS "fast;lint;budget" TIMEOUT 120)
add_test(NAME uncashed_cheques
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/uncashed_cheques.py
    --gate)
set_tests_properties(uncashed_cheques PROPERTIES
  LABELS "fast;lint;budget" TIMEOUT 300)
add_test(NAME mutant_copy_drift
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/mutant_copy_drift.py
    --repo-root ${CMAKE_SOURCE_DIR})
set_tests_properties(mutant_copy_drift PROPERTIES
  LABELS "fast;lint;nightly" TIMEOUT 300)

# THE OTHER QUESTION ABOUT A COMMITTED MUTANT (owner ruling R93, 2026-09-20).
# `mutant_copy_drift` proves a copy is CURRENT. It says nothing whatever about
# whether anything RUNS it -- and `zhao_terrain_bake_v2_mutant.sv` was found
# with ONE grep hit, the file itself: never elaborated, never asserted, and
# refreshed on a merge anyway. An uninstantiated MODULE costs the fitter
# nothing; an unrun MUTANT is cited as evidence.
#
# This gate is GREEN on the three known-undriven files, which sit on a debt
# list inside the tool with a reason and an owner, and RED the moment a NEW
# undriven mutant is committed -- the regression worth catching -- or when a
# debt-list entry gains a driver and the list should shrink. Fired on purpose
# by planting an undriven file under tests/mutants/: RC 1, and RC 0 again when
# it was removed.
add_test(NAME mutant_drivers
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/mutant_drivers.py
    --repo-root ${CMAKE_SOURCE_DIR})
set_tests_properties(mutant_drivers PROPERTIES
  LABELS "fast;lint;nightly" TIMEOUT 300)

# A directed test's case labels must identify their own block (ruling R62).
# Three blocks were numbered 17 and one label named 35 checks across all three;
# nothing in the compiler can see that, so it is a gate or it is nothing.
add_test(NAME case_labels
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/budget/check_case_labels.py)
set_tests_properties(case_labels PROPERTIES
  LABELS "fast;lint;nightly" TIMEOUT 120)

# A number written beside a DERIVED localparam is a claim about its value, and
# nothing read it back.  `zhao_terrain_devstore`'s record gained 16 bits; the
# two localparams computed from it kept their old comments (`// 576` against
# 704), and the block's whole owner-ruling-R59 size report was then computed
# from the stale figure and published at 116 M10K against a real 141.  It was
# quoted onward into `zhao_console_core.sv` and two packet reports as the price
# of composing the block -- understated, which is the direction nobody audits.
# The script carries its own positive control (`--self-test`) and asserts it at
# import, because a self-check that matches nothing prints reassurance forever.
add_test(NAME localparam_comments
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/design/check_localparam_comments.py
    --root ${CMAKE_SOURCE_DIR})
set_tests_properties(localparam_comments PROPERTIES
  LABELS "fast;lint;nightly" TIMEOUT 120)

# counter_id = catalog index, APPEND-ONLY (spec/counters.md 2, ruling R19):
# design/blocks.yml's counter_catalog must begin with design/counter_ids.lock,
# and every zhao_pkg ZHAO_CNT_* id must be its name's index. The script
# plants a prepend, a wrong RTL id, a duplicate and an undeclared name first,
# and refuses to run unless all four fire.
add_test(NAME counter_ids_append_only
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/design/check_counter_ids.py)
set_tests_properties(counter_ids_append_only PROPERTIES
  LABELS "fast;lint;nightly" TIMEOUT 120)

# V3 BANK GEOMETRY AND FABRIC STATE -- registered 2026-09-23, and the reason it
# was not registered before is the point. tools/rtl/check_v3_banks.py has an
# exemplary self-test: it plants a primitive that is not in the tree and REFUSES
# TO RUN unless the hand-list audit fires. Nothing ever ran it, because it was
# not a test. On 2026-09-14 commit 096c7139 gave
# zhao_texture_material_combine_v3.sv a `localparam` written across three lines;
# the gate could not resolve it, returned rc=2 CANNOT PARSE, and went dark FOR
# NINE DAYS -- self-test included, which is how its own alarm ended up being the
# symptom that found it. A gate nobody runs is indistinguishable from a gate
# that does not exist, so it is a test now.
#
# IT IS RED AT REGISTRATION, INHERITED AND PRE-EXISTING: two fabric stores
# (uvw_m 4,096 bits, shadow_metadata_m 7,680 bits), a MATERIAL bank 46 bits wide
# against a declared 48, and a two-read-address array that cannot infer an M10K.
# None of that is new; it was merely unprintable while the parser was stuck.
add_test(NAME v3_bank_geometry
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/rtl/check_v3_banks.py)
set_tests_properties(v3_bank_geometry PROPERTIES
  LABELS "lint;nightly" TIMEOUT 180)

# The short finish forms are an arithmetic claim over the whole raw-product
# domain, including products no legal byte pair can make. The differential above
# drives legal texels and so cannot reach that domain; this compares the two
# formulas directly and carries its own negative control.
add_test(NAME material_combine_v3_finish_equivalence
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_material_finish_equivalence.py -q)
set_tests_properties(material_combine_v3_finish_equivalence PROPERTIES
  LABELS "fast;nightly;packet-b;packet-f" TIMEOUT 300)

# Terminal-collector raw-index controls.
add_executable(pb_rspdi
  texture/texture_rsp_dispatch_v2_directed.cpp)
target_compile_definitions(pb_rspdi PRIVATE
  ZHAO_DROP_INDEX_MUTANT=1)
target_link_libraries(pb_rspdi PRIVATE zhao_harness)
verilate(pb_rspdi
  PREFIX Vzhao_texture_rsp_dispatch_v2_drop_index_mutant
  TOP_MODULE zhao_texture_rsp_dispatch_v2_drop_index_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_rsp_dispatch_v2_index_mutants.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_rsp_dispatch_v2_drop_index_control
         COMMAND pb_rspdi)

add_executable(pb_rspri
  texture/texture_rsp_dispatch_v2_directed.cpp)
target_compile_definitions(pb_rspri PRIVATE
  ZHAO_RECONSTRUCT_INDEX_MUTANT=1)
target_link_libraries(pb_rspri PRIVATE zhao_harness)
verilate(pb_rspri
  PREFIX Vzhao_texture_rsp_dispatch_v2_reconstruct_index_mutant
  TOP_MODULE zhao_texture_rsp_dispatch_v2_reconstruct_index_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_rsp_dispatch_v2_index_mutants.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_rsp_dispatch_v2_reconstruct_index_control
         COMMAND pb_rspri)

# Descriptor pad and held-row identity controls.
add_executable(pb_descpad
  texture/texture_early_desc_v2_directed.cpp)
target_compile_definitions(pb_descpad PRIVATE
  ZHAO_DESC_PAD_MUTANT=1)
target_link_libraries(pb_descpad PRIVATE zhao_harness)
verilate(pb_descpad
  PREFIX Vzhao_texture_early_desc_v2_pad_mutant
  TOP_MODULE zhao_texture_early_desc_v2_pad_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc_v2.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_early_desc_v2_pad_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_early_desc_v2_pad_control
         COMMAND pb_descpad)

add_executable(pb_descsw
  texture/texture_early_desc_v2_directed.cpp)
target_compile_definitions(pb_descsw PRIVATE
  ZHAO_DESC_SLOTSWAP_MUTANT=1)
target_link_libraries(pb_descsw PRIVATE zhao_harness)
verilate(pb_descsw
  PREFIX Vzhao_texture_early_desc_v2_slotswap_mutant
  TOP_MODULE zhao_texture_early_desc_v2_slotswap_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_early_desc_v2_slotswap_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_early_desc_v2_slotswap_control
         COMMAND pb_descsw)

# Expander selector-carry and otherwise-unreachable queue-overflow controls.
add_executable(pb_expsel
  texture/texture_frag_expand_v2_directed.cpp)
target_compile_definitions(pb_expsel PRIVATE
  ZHAO_FRAG_EXPAND_SELECTOR_MUTANT=1)
target_link_libraries(pb_expsel PRIVATE zhao_harness)
verilate(pb_expsel
  PREFIX Vzhao_texture_frag_expand_v2_mutant
  TOP_MODULE zhao_texture_frag_expand_v2_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_frag_expand_v2_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_frag_expand_v2_selector_control
         COMMAND pb_expsel)

add_executable(pb_expq
  texture/texture_frag_expand_v2_directed.cpp)
target_compile_definitions(pb_expq PRIVATE
  ZHAO_FRAG_EXPAND_QUEUE_MUTANT=1)
target_link_libraries(pb_expq PRIVATE zhao_harness)
verilate(pb_expq
  PREFIX Vzhao_texture_frag_expand_v2_queue_guard_mutant
  TOP_MODULE zhao_texture_frag_expand_v2_queue_guard_mutant
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_frag_expand_v2_mutant.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_frag_expand_v2_queue_control
         COMMAND pb_expq)

set_tests_properties(
  texture_rsp_dispatch_v2_drop_index_control
  texture_rsp_dispatch_v2_reconstruct_index_control
  texture_early_desc_v2_pad_control texture_early_desc_v2_slotswap_control
  texture_frag_expand_v2_selector_control texture_frag_expand_v2_queue_control
  PROPERTIES LABELS "fast;nightly;packet-b;mutant" TIMEOUT 900)

# Full 224-bit owner context and 160-bit retirement context under independently
# randomized output backpressure.  Stable synthesis-excluded DPI tasks expose
# the owner context, idle components, counters, and fault injection without any
# generated-class suffix or VPI/public-flat dependency.
add_executable(tpb_ctx
  texture/texture_island_v3_packet_b_fullctx.cpp)
verilate(tpb_ctx
  PREFIX Vzhao_texture_island_v3_top
  TOP_MODULE zhao_texture_island_v3_top
  SOURCES ${ZHAO_TEXTURE_V3_PACKET_B_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=1)
add_test(NAME texture_island_v3_packet_b_fullctx
         COMMAND tpb_ctx)
set_tests_properties(texture_island_v3_packet_b_fullctx PROPERTIES
  LABELS "fast;nightly;packet-b" TIMEOUT 1800)

# Descriptor -> expander -> binding resolver seam, including the inverse-polarity
# committed bad-pad path.  Package is first because all three import its widths.
set(ZHAO_TEXTURE_DESC_EXPAND_BIND_V2_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc_v2.sv
  ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_early_desc_v2_pad_mutant.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_texture_desc_expand_bind_v2_composed.sv)
add_executable(tdbv2
  texture/texture_desc_expand_bind_v2_composed.cpp)
target_link_libraries(tdbv2 PRIVATE zhao_harness)
verilate(tdbv2
  PREFIX Vtb_texture_desc_expand_bind_v2_composed
  TOP_MODULE tb_texture_desc_expand_bind_v2_composed
  SOURCES ${ZHAO_TEXTURE_DESC_EXPAND_BIND_V2_SOURCES}
  VERILATOR_ARGS --assert -GPAD_MUTANT=0
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/texture)
add_test(NAME texture_desc_expand_bind_v2_composed
         COMMAND tdbv2)

add_executable(tdbv2_pad
  texture/texture_desc_expand_bind_v2_composed.cpp)
target_compile_definitions(tdbv2_pad PRIVATE
  ZHAO_COMPOSED_PAD_MUTANT=1)
target_link_libraries(tdbv2_pad PRIVATE zhao_harness)
verilate(tdbv2_pad
  PREFIX Vtb_texture_desc_expand_bind_v2_composed
  TOP_MODULE tb_texture_desc_expand_bind_v2_composed
  SOURCES ${ZHAO_TEXTURE_DESC_EXPAND_BIND_V2_SOURCES}
  VERILATOR_ARGS --assert -GPAD_MUTANT=1
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/texture)
add_test(NAME texture_desc_expand_bind_v2_pad_control
         COMMAND tdbv2_pad)
set_tests_properties(
  texture_desc_expand_bind_v2_composed
  texture_desc_expand_bind_v2_pad_control
  PROPERTIES LABELS "fast;nightly;packet-b" TIMEOUT 1200)
set_tests_properties(texture_desc_expand_bind_v2_pad_control PROPERTIES
  LABELS "fast;nightly;packet-b;mutant")

# Randomized AUX differential against the committed zref coordinate oracle.
add_executable(pb_auxrnd
  texture/texture_aux_pipe_v2_random.cpp)
target_link_libraries(pb_auxrnd PRIVATE zhao_harness zhao_zref)
verilate(pb_auxrnd
  PREFIX Vzhao_texture_aux_pipe_v2
  TOP_MODULE zhao_texture_aux_pipe_v2
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv
  VERILATOR_ARGS --assert)
add_test(NAME texture_aux_pipe_v2_random COMMAND pb_auxrnd)
set_tests_properties(texture_aux_pipe_v2_random PROPERTIES
  LABELS "fast;nightly;packet-b" TIMEOUT 1800)

# Resolver controls use the exact final driver macros and renamed wrapper tops.
function(zhao_binding_v2_mutant_control NAME MACRO TOP)
  string(MD5 target_hash "${TOP}")
  string(SUBSTRING "${target_hash}" 0 8 target_hash)
  set(target tbv2_${target_hash})
  add_executable(${target} texture/texture_binding_resolver_v2_directed.cpp)
  target_compile_definitions(${target} PRIVATE ${MACRO}=1)
  target_link_libraries(${target} PRIVATE zhao_harness)
  verilate(${target}
    PREFIX Vbindv2_mut
    TOP_MODULE ${TOP}
    SOURCES
      ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
      ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
      ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_binding_resolver_v2_mutants.sv
    VERILATOR_ARGS --assert)
  add_test(NAME texture_binding_resolver_v2_${NAME}_control COMMAND ${target})
  set_tests_properties(texture_binding_resolver_v2_${NAME}_control PROPERTIES
    LABELS "fast;nightly;packet-b;mutant" TIMEOUT 900)
endfunction()
zhao_binding_v2_mutant_control(
  late_page_generation ZHAO_BIND_LATE_PAGE_MUTANT
  zhao_texture_binding_resolver_v2_late_page_generation_mutant)
zhao_binding_v2_mutant_control(
  early_activation_while_held ZHAO_BIND_OWNER_QUIET_MUTANT
  zhao_texture_binding_resolver_v2_early_activation_while_held_mutant)
zhao_binding_v2_mutant_control(
  active_bank_write ZHAO_BIND_ACTIVE_WRITE_MUTANT
  zhao_texture_binding_resolver_v2_active_bank_write_mutant)
zhao_binding_v2_mutant_control(
  stale_invalid_crc ZHAO_BIND_STALE_CRC_MUTANT
  zhao_texture_binding_resolver_v2_stale_invalid_crc_mutant)
zhao_binding_v2_mutant_control(
  witness_class_route ZHAO_BIND_WITNESS_CLASS_MUTANT
  zhao_texture_binding_resolver_v2_witness_class_route_mutant)
zhao_binding_v2_mutant_control(
  same_cycle_refusal ZHAO_BIND_SAME_CYCLE_MUTANT
  zhao_texture_binding_resolver_v2_same_cycle_refusal_mutant)
zhao_binding_v2_mutant_control(
  refusal_without_issue ZHAO_BIND_WITHOUT_ISSUE_MUTANT
  zhao_texture_binding_resolver_v2_refusal_without_issue_mutant)

# The second material implementation mode is not inferred from copy-mode results:
# four real owner planes feed READ_LATE=1 beside the same R9 stream in copy mode.
add_executable(pb_matrl
  texture/material_combine_v3_readlate_diff.cpp)
target_link_libraries(pb_matrl PRIVATE
  zhao_harness zhao_zref)
verilate(pb_matrl
  PREFIX Vtb_material_combine_v3_copy_readlate
  TOP_MODULE tb_material_combine_v3_copy_readlate
  SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/tb_material_combine_v3_copy_readlate.sv
  VERILATOR_ARGS --assert)
add_test(NAME material_combine_v3_readlate_diff
         COMMAND pb_matrl)
set_tests_properties(material_combine_v3_readlate_diff PROPERTIES
  LABELS "fast;nightly;packet-b" TIMEOUT 1200)

# Packet C: one test-only 34-source authority composes the exact Packet-B V3
# root with the 490-bit skid and real RASTER.FRAGMENT.  This is deliberately
# absent from production fit sources and remains excluded:not-yet-adopted.
set(ZHAO_PACKET_C_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/raster/raster_texture_stage_v3.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_C_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-C source manifest is missing: ${ZHAO_PACKET_C_SOURCE_MANIFEST}")
endif()
file(READ "${ZHAO_PACKET_C_SOURCE_MANIFEST}" ZHAO_PACKET_C_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_PACKET_C_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_C_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_PACKET_C_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_C_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_PACKET_C_SOURCE_MANIFEST_BODY
  "${ZHAO_PACKET_C_SOURCE_MANIFEST_RAW}")
if(ZHAO_PACKET_C_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_PACKET_C_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "Packet-C source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_PACKET_C_SOURCE_MANIFEST}"
  ZHAO_PACKET_C_RELATIVE_SOURCES)
set(ZHAO_PACKET_C_SOURCES)
set(ZHAO_PACKET_C_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_C_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-C source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_C_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-C source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-C source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_C_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_C_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
list(LENGTH ZHAO_PACKET_C_RELATIVE_SOURCES ZHAO_PACKET_C_SOURCE_COUNT)
if(NOT ZHAO_PACKET_C_SOURCE_COUNT EQUAL 34)
  message(FATAL_ERROR "Packet-C source manifest must contain exactly 34 SV paths")
endif()
list(GET ZHAO_PACKET_C_RELATIVE_SOURCES 0 ZHAO_PACKET_C_FIRST_SOURCE)
list(GET ZHAO_PACKET_C_RELATIVE_SOURCES 31 ZHAO_PACKET_C_MUTANT_SOURCE)
list(GET ZHAO_PACKET_C_RELATIVE_SOURCES 32 ZHAO_PACKET_C_STAGE_SOURCE)
list(GET ZHAO_PACKET_C_RELATIVE_SOURCES 33 ZHAO_PACKET_C_LAST_SOURCE)
if(NOT ZHAO_PACKET_C_FIRST_SOURCE STREQUAL
   "fpga/rtl/common/zhao_render_texture_pkg.sv" OR
   NOT ZHAO_PACKET_C_MUTANT_SOURCE STREQUAL
   "tests/mutants/zhao_raster_texture_stage_v3_mutants.sv" OR
   NOT ZHAO_PACKET_C_STAGE_SOURCE STREQUAL
   "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv" OR
   NOT ZHAO_PACKET_C_LAST_SOURCE STREQUAL
   "tests/raster/tb_raster_texture_stage_v3.sv")
  message(FATAL_ERROR
    "Packet-C source manifest lost package-first/mutant-stage/top-last order")
endif()

add_executable(pc_dir raster/raster_texture_stage_v3_directed.cpp)
target_link_libraries(pc_dir PRIVATE zhao_harness zhao_zref)
verilate(pc_dir
  PREFIX Vtb_raster_texture_stage_v3
  TOP_MODULE tb_raster_texture_stage_v3
  SOURCES ${ZHAO_PACKET_C_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=0)
add_test(NAME raster_texture_stage_v3_directed COMMAND pc_dir)

add_executable(pc_seq raster/raster_texture_stage_v3_directed.cpp)
target_compile_definitions(pc_seq PRIVATE PACKET_C_EXPECT_IDENTITY_ABORT=1)
target_link_libraries(pc_seq PRIVATE zhao_harness zhao_zref)
verilate(pc_seq
  PREFIX Vtb_raster_texture_stage_v3
  TOP_MODULE tb_raster_texture_stage_v3
  SOURCES ${ZHAO_PACKET_C_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=0
    -DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY)
add_test(NAME raster_texture_stage_v3_identity_abort_control COMMAND pc_seq)

add_executable(pc_old raster/raster_texture_stage_v3_directed.cpp)
target_compile_definitions(pc_old PRIVATE PACKET_C_EXPECT_OLD_READY=1)
target_link_libraries(pc_old PRIVATE zhao_harness zhao_zref)
verilate(pc_old
  PREFIX Vtb_raster_texture_stage_v3
  TOP_MODULE tb_raster_texture_stage_v3
  SOURCES ${ZHAO_PACKET_C_SOURCES}
  VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=0
    -DZHAO_PACKET_C_MUTANT_OLD_READY)
add_test(NAME raster_texture_stage_v3_old_ready_control COMMAND pc_old)

add_test(NAME raster_texture_stage_v3_registration_static
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_c.py -q)
set_tests_properties(
  raster_texture_stage_v3_directed
  raster_texture_stage_v3_registration_static
  PROPERTIES LABELS "fast;nightly;packet-c" TIMEOUT 1800)
set_tests_properties(
  raster_texture_stage_v3_identity_abort_control
  PROPERTIES LABELS "fast;nightly;packet-c;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "packet-c identity-only mismatch detector FIRED"
    FAIL_REGULAR_EXPRESSION "packet-c directed FAIL")
set_tests_properties(
  raster_texture_stage_v3_old_ready_control
  PROPERTIES LABELS "fast;nightly;packet-c;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "packet-c old-ready deadlock mutant FIRED"
    FAIL_REGULAR_EXPRESSION "packet-c directed FAIL")

# Packet D1: current-rast.cpp signed row-gradient law. The manifest keeps each
# selector immediately before the exact two production leaves and the test top
# last; all four profiles compile the same source authority.
set(ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/raster/raster_attrgrad_v2.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-D attribute source manifest is missing: ${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST}")
endif()
file(READ "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_BODY
  "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_RAW}")
if(ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "Packet-D attribute source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_ATTR_RELATIVE_SOURCES)
set(ZHAO_PACKET_D_ATTR_SOURCES)
set(ZHAO_PACKET_D_ATTR_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_D_ATTR_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR
      "invalid Packet-D attribute source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_D_ATTR_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR
      "duplicate Packet-D attribute source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-D attribute source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_D_ATTR_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_D_ATTR_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
if(NOT ZHAO_PACKET_D_ATTR_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_raster_attr_v2_mutants.sv;fpga/rtl/raster/zhao_raster_attrdiv_v2.sv;fpga/rtl/raster/zhao_raster_attrgrad_v2.sv;tests/raster/tb_raster_attrgrad_v2.sv")
  message(FATAL_ERROR
    "Packet-D attribute source manifest is not the exact ordered four-file closure")
endif()

function(zhao_packet_d_attr_target TARGET RADIX)
  set(selector_args ${ARGN})
  add_executable(${TARGET} raster/raster_attrgrad_v2_directed.cpp)
  target_compile_definitions(${TARGET} PRIVATE ZHAO_ATTR_RADIX=${RADIX})
  target_link_libraries(${TARGET} PRIVATE zhao_zref)
  target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/reference/src)
  verilate(${TARGET}
    PREFIX Vtb_raster_attrgrad_v2
    TOP_MODULE tb_raster_attrgrad_v2
    SOURCES ${ZHAO_PACKET_D_ATTR_SOURCES}
    VERILATOR_ARGS --assert -GRADIX=${RADIX} ${selector_args})
endfunction()

zhao_packet_d_attr_target(pd_a2 2)
add_test(NAME raster_attrgrad_v2_directed COMMAND pd_a2)
zhao_packet_d_attr_target(pd_a4 4)
add_test(NAME raster_attrgrad_v2_r4 COMMAND pd_a4)

zhao_packet_d_attr_target(pd_aneg 2 -DZHAO_ATTR_V2_MUTANT_NEG_HALF)
target_compile_definitions(pd_aneg PRIVATE EXPECT_NEG_HALF_MUTANT=1)
add_test(NAME raster_attrgrad_v2_neg_half_mutant COMMAND pd_aneg)

zhao_packet_d_attr_target(pd_amin 2 -DZHAO_ATTR_V2_MUTANT_OMIT_MIN_X_ACCUM)
target_compile_definitions(pd_amin PRIVATE EXPECT_OMIT_MIN_X_ACCUM_MUTANT=1)
add_test(NAME raster_attrgrad_v2_omit_min_x_accum_mutant COMMAND pd_amin)

set_tests_properties(
  raster_attrgrad_v2_directed
  raster_attrgrad_v2_r4
  PROPERTIES LABELS "fast;nightly;packet-d" TIMEOUT 1800)
set_tests_properties(
  raster_attrgrad_v2_neg_half_mutant
  PROPERTIES LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "PASS: negative exact-half mutant fired"
    FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(
  raster_attrgrad_v2_omit_min_x_accum_mutant
  PROPERTIES LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "PASS: omitted global-min-X tile offset mutant fired exactly"
    FAIL_REGULAR_EXPRESSION "FAIL")

# DSPR1 ATTR3 candidate: cycle-exact old/new differential at both supported
# divider radices. The exact seven-source manifest is the only source authority.
set(ZHAO_ATTR3_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/raster/raster_attrgrad_dsp3.sources.txt)
if(NOT EXISTS "${ZHAO_ATTR3_SOURCE_MANIFEST}")
  message(FATAL_ERROR "required ATTR3 source manifest is missing")
endif()
file(READ "${ZHAO_ATTR3_SOURCE_MANIFEST}" ZHAO_ATTR3_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_ATTR3_SOURCE_MANIFEST_RAW
  "${ZHAO_ATTR3_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_ATTR3_SOURCE_MANIFEST_RAW
  "${ZHAO_ATTR3_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_ATTR3_SOURCE_MANIFEST_BODY
  "${ZHAO_ATTR3_SOURCE_MANIFEST_RAW}")
if(ZHAO_ATTR3_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_ATTR3_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "ATTR3 source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_ATTR3_SOURCE_MANIFEST}" ZHAO_ATTR3_RELATIVE_SOURCES)
if(NOT ZHAO_ATTR3_RELATIVE_SOURCES STREQUAL
   "fpga/rtl/common/zhao_mul27_exact.sv;fpga/rtl/raster/zhao_raster_attrdiv_v2.sv;fpga/rtl/raster/zhao_raster_attrgrad_v2.sv;tests/mutants/zhao_attr_dsp3_mutants.sv;fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv;fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv;tests/raster/tb_raster_attrgrad_dsp3_pair.sv")
  message(FATAL_ERROR "ATTR3 source manifest is not the exact ordered seven-file closure")
endif()
set(ZHAO_ATTR3_SOURCES)
set(ZHAO_ATTR3_SEEN)
foreach(relative_source IN LISTS ZHAO_ATTR3_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid ATTR3 source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_ATTR3_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate ATTR3 source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing ATTR3 source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_ATTR3_SEEN "${relative_source}")
  list(APPEND ZHAO_ATTR3_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_attr3_target TARGET RADIX)
  set(selector_args ${ARGN})
  add_executable(${TARGET} raster/raster_attrgrad_dsp3_diff.cpp)
  verilate(${TARGET}
    PREFIX Vtb_raster_attrgrad_dsp3_pair
    TOP_MODULE tb_raster_attrgrad_dsp3_pair
    SOURCES ${ZHAO_ATTR3_SOURCES}
    VERILATOR_ARGS -GRADIX=${RADIX} ${selector_args})
endfunction()

zhao_attr3_target(dspr_attr3_r2 2 --assert)
add_test(NAME dsp_rescue_attr3_radix2 COMMAND dspr_attr3_r2)
zhao_attr3_target(dspr_attr3_r4 4 --assert)
add_test(NAME dsp_rescue_attr3_radix4 COMMAND dspr_attr3_r4)
zhao_attr3_target(dspr_attr3_mid 2 -DZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE)
target_compile_definitions(dspr_attr3_mid PRIVATE EXPECT_ATTR3_MUTANT=1)
add_test(NAME dsp_rescue_attr3_signed_middle_mutant COMMAND dspr_attr3_mid)
zhao_attr3_target(dspr_attr3_shift 2 -DZHAO_ATTR_DSP3_MUTANT_NARROW_HIGH_SHIFT)
target_compile_definitions(dspr_attr3_shift PRIVATE EXPECT_ATTR3_MUTANT=1)
add_test(NAME dsp_rescue_attr3_narrow_shift_mutant COMMAND dspr_attr3_shift)
zhao_attr3_target(dspr_attr3_omit 2 -DZHAO_ATTR_DSP3_MUTANT_OMIT_BASE_Y)
target_compile_definitions(dspr_attr3_omit PRIVATE EXPECT_ATTR3_MUTANT=1)
add_test(NAME dsp_rescue_attr3_omit_base_y_mutant COMMAND dspr_attr3_omit)
zhao_attr3_target(dspr_attr3_idle 2 -DZHAO_ATTR_DSP3_MUTANT_IDLE_EARLY)
target_compile_definitions(dspr_attr3_idle PRIVATE EXPECT_ATTR3_MUTANT=1)
add_test(NAME dsp_rescue_attr3_idle_early_mutant COMMAND dspr_attr3_idle)
zhao_attr3_target(dspr_attr3_offset_guard 2
  -DZHAO_ATTR_DSP3_MUTANT_DELAY_OFFSET_READY)
target_compile_definitions(dspr_attr3_offset_guard PRIVATE
  EXPECT_ATTR3_OFFSET_GUARD=1)
add_test(NAME dsp_rescue_attr3_offset_ready_assertion COMMAND dspr_attr3_offset_guard)
set_tests_properties(
  dsp_rescue_attr3_radix2 dsp_rescue_attr3_radix4
  PROPERTIES LABELS "fast;nightly;g8a-dsp" TIMEOUT 1800
  FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(
  dsp_rescue_attr3_signed_middle_mutant
  dsp_rescue_attr3_narrow_shift_mutant
  dsp_rescue_attr3_omit_base_y_mutant
  dsp_rescue_attr3_idle_early_mutant
  PROPERTIES LABELS "fast;nightly;g8a-dsp;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "ATTR3 MUTANT FIRED")
set_tests_properties(dsp_rescue_attr3_offset_ready_assertion PROPERTIES
  LABELS "fast;nightly;g8a-dsp;mutant" TIMEOUT 180
  PASS_REGULAR_EXPRESSION "ATTR3 OFFSET GUARD FIRED"
  FAIL_REGULAR_EXPRESSION "FAIL")

# DSPR2 BIL2 candidate: one all-signed dual18 for the simultaneous horizontal
# pair, with the old lane and an independent host oracle in the same miter.
set(ZHAO_BIL2_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/texture_bilerp_lane_dsp2.sources.txt)
if(NOT EXISTS "${ZHAO_BIL2_SOURCE_MANIFEST}")
  message(FATAL_ERROR "required BIL2 source manifest is missing")
endif()
file(READ "${ZHAO_BIL2_SOURCE_MANIFEST}" ZHAO_BIL2_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_BIL2_SOURCE_MANIFEST_RAW
  "${ZHAO_BIL2_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_BIL2_SOURCE_MANIFEST_RAW
  "${ZHAO_BIL2_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_BIL2_SOURCE_MANIFEST_BODY
  "${ZHAO_BIL2_SOURCE_MANIFEST_RAW}")
if(ZHAO_BIL2_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_BIL2_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "BIL2 source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_BIL2_SOURCE_MANIFEST}" ZHAO_BIL2_RELATIVE_SOURCES)
if(NOT ZHAO_BIL2_RELATIVE_SOURCES STREQUAL
   "fpga/rtl/common/zhao_dual18_mul.sv;fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv;tests/mutants/zhao_texture_bilerp_lane_dsp2_mutant.sv;fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv;tests/texture/tb_texture_bilerp_lane_dsp2.sv")
  message(FATAL_ERROR "BIL2 source manifest is not the exact ordered five-file closure")
endif()
set(ZHAO_BIL2_SOURCES)
set(ZHAO_BIL2_SEEN)
foreach(relative_source IN LISTS ZHAO_BIL2_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid BIL2 source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_BIL2_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate BIL2 source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing BIL2 source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_BIL2_SEEN "${relative_source}")
  list(APPEND ZHAO_BIL2_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

add_executable(dspr_bil2 texture/texture_bilerp_lane_dsp2_diff.cpp)
target_link_libraries(dspr_bil2 PRIVATE zhao_harness)
verilate(dspr_bil2
  PREFIX Vtb_texture_bilerp_lane_dsp2
  TOP_MODULE tb_texture_bilerp_lane_dsp2
  SOURCES ${ZHAO_BIL2_SOURCES}
  VERILATOR_ARGS --assert -DZHAO_DUAL18_BEHAVIORAL)
add_test(NAME dsp_rescue_bil2_diff COMMAND dspr_bil2)

add_executable(dspr_bil2_mut texture/texture_bilerp_lane_dsp2_diff.cpp)
target_compile_definitions(dspr_bil2_mut PRIVATE EXPECT_BIL2_COLLAPSE_RESULTB=1)
target_link_libraries(dspr_bil2_mut PRIVATE zhao_harness)
verilate(dspr_bil2_mut
  PREFIX Vtb_texture_bilerp_lane_dsp2
  TOP_MODULE tb_texture_bilerp_lane_dsp2
  SOURCES ${ZHAO_BIL2_SOURCES}
  VERILATOR_ARGS --assert -DZHAO_DUAL18_BEHAVIORAL
    -DZHAO_BIL2_MUTANT_COLLAPSE_RESULTB)
add_test(NAME dsp_rescue_bil2_collapse_resultb_mutant COMMAND dspr_bil2_mut)

add_executable(dspr_bil2_b2_mut texture/texture_bilerp_lane_dsp2_diff.cpp)
target_compile_definitions(dspr_bil2_b2_mut PRIVATE
  EXPECT_BIL2_BYPASS_VERTICAL_CAPTURE=1)
target_link_libraries(dspr_bil2_b2_mut PRIVATE zhao_harness)
verilate(dspr_bil2_b2_mut
  PREFIX Vtb_texture_bilerp_lane_dsp2
  TOP_MODULE tb_texture_bilerp_lane_dsp2
  SOURCES ${ZHAO_BIL2_SOURCES}
  VERILATOR_ARGS --assert -DZHAO_DUAL18_BEHAVIORAL
    -DZHAO_BIL2_MUTANT_BYPASS_VERTICAL_CAPTURE)
add_test(NAME dsp_rescue_bil2_vertical_capture_mutant COMMAND dspr_bil2_b2_mut)
set_tests_properties(dsp_rescue_bil2_vertical_capture_mutant PROPERTIES
  LABELS "fast;nightly;g8a-dsp;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "BIL2 vertical-capture bypass mutant FIRED"
  FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(dsp_rescue_bil2_diff PROPERTIES
  LABELS "fast;nightly;g8a-dsp" TIMEOUT 1800 FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(dsp_rescue_bil2_collapse_resultb_mutant PROPERTIES
  LABELS "fast;nightly;g8a-dsp;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "BIL2 collapse-resultb mutant FIRED"
  FAIL_REGULAR_EXPRESSION "FAIL")

function(zhao_g8a_dsp_selector_control NAME PROFILE)
  add_test(NAME ${NAME}
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_g8a_dsp_rescue.py
      --collision ${PROFILE}
      --verilator "${ZHAO_VERILATOR_LINT}")
  set_tests_properties(${NAME} PROPERTIES
    LABELS "fast;nightly;g8a-dsp;mutant" TIMEOUT 180
    FAIL_REGULAR_EXPRESSION "FAIL")
endfunction()

zhao_g8a_dsp_selector_control(
  g8a_dsp_attr_mid_shift_selector_collision attr-mid-shift)
zhao_g8a_dsp_selector_control(
  g8a_dsp_attr_mid_omit_selector_collision attr-mid-omit)
zhao_g8a_dsp_selector_control(
  g8a_dsp_attr_shift_omit_selector_collision attr-shift-omit)
zhao_g8a_dsp_selector_control(
  g8a_dsp_attr_mid_delay_selector_collision attr-mid-delay)
zhao_g8a_dsp_selector_control(
  g8a_dsp_attr_mid_idle_selector_collision attr-mid-idle)
zhao_g8a_dsp_selector_control(
  g8a_dsp_bil2_selector_collision bil2)

add_test(NAME g8a_dsp_rescue_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_g8a_dsp_rescue.py -q)
set_tests_properties(g8a_dsp_rescue_registration_static PROPERTIES
  LABELS "fast;nightly;g8a-dsp" TIMEOUT 180)

set(ZHAO_G8A_DSP_REQUIRED_TESTS
  dsp_rescue_attr3_radix2
  dsp_rescue_attr3_radix4
  dsp_rescue_attr3_signed_middle_mutant
  dsp_rescue_attr3_narrow_shift_mutant
  dsp_rescue_attr3_omit_base_y_mutant
  dsp_rescue_attr3_idle_early_mutant
  dsp_rescue_attr3_offset_ready_assertion
  dsp_rescue_bil2_diff
  dsp_rescue_bil2_collapse_resultb_mutant
  dsp_rescue_bil2_vertical_capture_mutant
  g8a_dsp_attr_mid_shift_selector_collision
  g8a_dsp_attr_mid_omit_selector_collision
  g8a_dsp_attr_shift_omit_selector_collision
  g8a_dsp_attr_mid_delay_selector_collision
  g8a_dsp_attr_mid_idle_selector_collision
  g8a_dsp_bil2_selector_collision
  g8a_dsp_rescue_registration_static)
list(LENGTH ZHAO_G8A_DSP_REQUIRED_TESTS ZHAO_G8A_DSP_REQUIRED_TEST_COUNT)
if(NOT ZHAO_G8A_DSP_REQUIRED_TEST_COUNT EQUAL 17)
  message(FATAL_ERROR
    "G8A DSP required CTest inventory must contain exactly 17 names")
endif()
foreach(required_g8a_dsp_test IN LISTS ZHAO_G8A_DSP_REQUIRED_TESTS)
  if(NOT TEST "${required_g8a_dsp_test}")
    message(FATAL_ERROR
      "G8A DSP required CTest target is absent: ${required_g8a_dsp_test}")
  endif()
endforeach()

# Packet D2: old/V2 binner parity plus exact accepted-triangle metadata identity.
set(ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry/geom_binner_v2.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-D binner source manifest is missing: ${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST}")
endif()
file(READ "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_BODY
  "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_RAW}")
if(ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "Packet-D binner source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_BINNER_RELATIVE_SOURCES)
set(ZHAO_PACKET_D_BINNER_SOURCES)
set(ZHAO_PACKET_D_BINNER_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_D_BINNER_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR
      "invalid Packet-D binner source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_D_BINNER_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR
      "duplicate Packet-D binner source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-D binner source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_D_BINNER_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_D_BINNER_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
if(NOT ZHAO_PACKET_D_BINNER_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_geom_binner_v2_mutants.sv;fpga/rtl/geometry/zhao_geom_arena.sv;fpga/rtl/raster/zhao_raster_fill.sv;fpga/rtl/geometry/zhao_geom_binner.sv;fpga/rtl/geometry/zhao_geom_binner_v2.sv;tests/geometry/tb_geom_binner_v2_pair.sv")
  message(FATAL_ERROR
    "Packet-D binner source manifest is not the exact ordered six-file closure")
endif()

function(zhao_packet_d_binner_target TARGET)
  set(selector_args ${ARGN})
  add_executable(${TARGET} geometry/geom_binner_v2_directed.cpp)
  verilate(${TARGET}
    PREFIX Vtb_geom_binner_v2_pair
    TOP_MODULE tb_geom_binner_v2_pair
    SOURCES ${ZHAO_PACKET_D_BINNER_SOURCES}
    VERILATOR_ARGS --assert
      -GTRI_CAP=8 -GTRI_W=3 -GCHUNKS=6 -GCHUNK_W=3
      ${selector_args})
endfunction()

zhao_packet_d_binner_target(pd_bin)
add_test(NAME geom_binner_v2_directed COMMAND pd_bin)

zhao_packet_d_binner_target(pd_bim
  -DZHAO_GEOM_BINNER_V2_MUTANT_META_ADDR_SWAP)
target_compile_definitions(pd_bim PRIVATE
  EXPECT_GEOM_BINNER_V2_META_ADDR_SWAP_MUTANT=1)
add_test(NAME geom_binner_v2_meta_addr_swap_mutant COMMAND pd_bim)

set_tests_properties(geom_binner_v2_directed PROPERTIES
  LABELS "fast;nightly;packet-d" TIMEOUT 1800)
set_tests_properties(geom_binner_v2_meta_addr_swap_mutant PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "PASS: metadata-address mutant fired exactly"
  FAIL_REGULAR_EXPRESSION "FAIL")

# Packet D3: exact 57-source binner-to-resolve composition. Packet-C and
# Packet-D selectors precede the exact production modules they alter.
set(ZHAO_PACKET_D_FULL_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/geometry/geom_bin_pipe_v2.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-D full source manifest is missing: ${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST}")
endif()
file(READ "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW)
string(REPLACE "\r\n" "\n" ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW}")
string(REPLACE "\r" "\n" ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW
  "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW}")
string(REGEX REPLACE "\n$" "" ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_BODY
  "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_RAW}")
if(ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_BODY STREQUAL "" OR
   ZHAO_PACKET_D_FULL_SOURCE_MANIFEST_BODY MATCHES "(^|\n)[ \t]*(\n|$)")
  message(FATAL_ERROR "Packet-D full source manifest contains a blank record")
endif()
file(STRINGS "${ZHAO_PACKET_D_FULL_SOURCE_MANIFEST}"
  ZHAO_PACKET_D_FULL_RELATIVE_SOURCES)
set(ZHAO_PACKET_D_FULL_SOURCES)
set(ZHAO_PACKET_D_FULL_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_D_FULL_RELATIVE_SOURCES)
  if(relative_source STREQUAL "" OR IS_ABSOLUTE "${relative_source}" OR
     relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR
      "invalid Packet-D full source-manifest path: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_D_FULL_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR
      "duplicate Packet-D full source-manifest path: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-D full source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_D_FULL_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_D_FULL_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
list(LENGTH ZHAO_PACKET_D_FULL_RELATIVE_SOURCES ZHAO_PACKET_D_FULL_SOURCE_COUNT)
if(NOT ZHAO_PACKET_D_FULL_SOURCE_COUNT EQUAL 57)
  message(FATAL_ERROR "Packet-D full source manifest must contain exactly 57 SV paths")
endif()
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 0 ZHAO_PACKET_D_FULL_FIRST)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 31 ZHAO_PACKET_D_FULL_PC_MUTANT)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 32 ZHAO_PACKET_D_FULL_PC_STAGE)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 53 ZHAO_PACKET_D_FULL_TILE_MUTANT)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 54 ZHAO_PACKET_D_FULL_TILE_STAGE)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 55 ZHAO_PACKET_D_FULL_BIN_STAGE)
list(GET ZHAO_PACKET_D_FULL_RELATIVE_SOURCES 56 ZHAO_PACKET_D_FULL_TOP)
if(NOT ZHAO_PACKET_D_FULL_FIRST STREQUAL
   "fpga/rtl/generated/zhao_abi_pkg.sv" OR
   NOT ZHAO_PACKET_D_FULL_PC_MUTANT STREQUAL
   "tests/mutants/zhao_raster_texture_stage_v3_mutants.sv" OR
   NOT ZHAO_PACKET_D_FULL_PC_STAGE STREQUAL
   "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv" OR
   NOT ZHAO_PACKET_D_FULL_TILE_MUTANT STREQUAL
   "tests/mutants/zhao_raster_tile_pipe_v2_mutants.sv" OR
   NOT ZHAO_PACKET_D_FULL_TILE_STAGE STREQUAL
   "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv" OR
   NOT ZHAO_PACKET_D_FULL_BIN_STAGE STREQUAL
   "fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv" OR
   NOT ZHAO_PACKET_D_FULL_TOP STREQUAL
   "tests/geometry/tb_geom_bin_pipe_v2.sv")
  message(FATAL_ERROR
    "Packet-D full source manifest lost package/selector/stage/top order")
endif()

function(zhao_packet_d_full_target TARGET)
  set(selector_args ${ARGN})
  add_executable(${TARGET} geometry/geom_bin_pipe_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_zref)
  verilate(${TARGET}
    PREFIX Vtb_geom_bin_pipe_v2
    TOP_MODULE tb_geom_bin_pipe_v2
    SOURCES ${ZHAO_PACKET_D_FULL_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
endfunction()

zhao_packet_d_full_target(pd_full)
add_test(NAME geom_bin_pipe_v2_directed COMMAND pd_full)

zhao_packet_d_full_target(pd_coord -DZHAO_PACKET_D_MUTANT_COORDINATE)
target_compile_definitions(pd_coord PRIVATE PACKET_D_EXPECT_COORDINATE=1)
add_test(NAME geom_bin_pipe_v2_coordinate_mutant COMMAND pd_coord)

zhao_packet_d_full_target(pd_quiet -DZHAO_PACKET_D_MUTANT_OMIT_V3_QUIET)
target_compile_definitions(pd_quiet PRIVATE PACKET_D_EXPECT_OMIT_V3_QUIET=1)
add_test(NAME geom_bin_pipe_v2_omit_v3_quiet_mutant COMMAND pd_quiet)

zhao_packet_d_full_target(pd_ident -DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY)
target_compile_definitions(pd_ident PRIVATE PACKET_D_EXPECT_IDENTITY_ABORT=1)
add_test(NAME geom_bin_pipe_v2_identity_cancel_control COMMAND pd_ident)

zhao_packet_d_full_target(pd_old -DZHAO_PACKET_C_MUTANT_OLD_READY)
target_compile_definitions(pd_old PRIVATE PACKET_D_EXPECT_OLD_READY=1)
add_test(NAME geom_bin_pipe_v2_old_ready_mutant COMMAND pd_old)

zhao_packet_d_full_target(pd_cancel
  -DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY
  -DZHAO_PACKET_D_MUTANT_SKIP_CANCEL)
target_compile_definitions(pd_cancel PRIVATE PACKET_D_EXPECT_SKIP_CANCEL=1)
add_test(NAME geom_bin_pipe_v2_skip_cancel_mutant COMMAND pd_cancel)

set_tests_properties(geom_bin_pipe_v2_directed PROPERTIES
  LABELS "fast;nightly;packet-d" TIMEOUT 1800)
set_tests_properties(geom_bin_pipe_v2_coordinate_mutant PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "Packet-D coordinate mutant FIRED"
  FAIL_REGULAR_EXPRESSION "packet-d directed FAIL")
set_tests_properties(geom_bin_pipe_v2_omit_v3_quiet_mutant PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "Packet-D omit-V3-quiet mutant FIRED"
  FAIL_REGULAR_EXPRESSION "packet-d directed FAIL")
set_tests_properties(geom_bin_pipe_v2_identity_cancel_control PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "Packet-D identity/cancel control FIRED"
  FAIL_REGULAR_EXPRESSION "packet-d directed FAIL")
set_tests_properties(geom_bin_pipe_v2_old_ready_mutant PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "Packet-D old-ready mutant FIRED"
  FAIL_REGULAR_EXPRESSION "packet-d directed FAIL")
set_tests_properties(geom_bin_pipe_v2_skip_cancel_mutant PROPERTIES
  LABELS "fast;nightly;packet-d;mutant" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "Packet-D skip-cancel mutant FIRED"
  FAIL_REGULAR_EXPRESSION "packet-d directed FAIL")

add_test(NAME packet_d_registration_static
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_d.py -q)
set_tests_properties(packet_d_registration_static PROPERTIES
  LABELS "fast;nightly;packet-d" TIMEOUT 1800)

# Independent configure-time inventory: deleting the static checker registration
# cannot make the remaining Packet-D tests look complete.
set(ZHAO_PACKET_D_REQUIRED_TESTS
  raster_attrgrad_v2_directed
  raster_attrgrad_v2_r4
  raster_attrgrad_v2_neg_half_mutant
  raster_attrgrad_v2_omit_min_x_accum_mutant
  geom_binner_v2_directed
  geom_binner_v2_meta_addr_swap_mutant
  geom_bin_pipe_v2_directed
  geom_bin_pipe_v2_coordinate_mutant
  geom_bin_pipe_v2_omit_v3_quiet_mutant
  geom_bin_pipe_v2_identity_cancel_control
  geom_bin_pipe_v2_old_ready_mutant
  geom_bin_pipe_v2_skip_cancel_mutant
  packet_d_registration_static)
list(LENGTH ZHAO_PACKET_D_REQUIRED_TESTS ZHAO_PACKET_D_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_D_REQUIRED_TEST_COUNT EQUAL 13)
  message(FATAL_ERROR "Packet-D required CTest inventory must contain exactly 13 names")
endif()
foreach(required_packet_d_test IN LISTS ZHAO_PACKET_D_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_d_test}")
    message(FATAL_ERROR
      "required Packet-D CTest is not registered: ${required_packet_d_test}")
  endif()
endforeach()

# Packet E1: cache refusal is a typed terminal, and its inherited response
# reservation has exactly one owner through fill, replay, or refused response.
set(ZHAO_PACKET_E_CACHE_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/texture_cache_pipe_v2_packet_e.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_E_CACHE_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-E cache source manifest is missing: ${ZHAO_PACKET_E_CACHE_SOURCE_MANIFEST}")
endif()
file(STRINGS "${ZHAO_PACKET_E_CACHE_SOURCE_MANIFEST}"
  ZHAO_PACKET_E_CACHE_RELATIVE_SOURCES
  REGEX "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$")
if(NOT ZHAO_PACKET_E_CACHE_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_texture_cache_pipe_v2_packet_e_mutants.sv;fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv")
  message(FATAL_ERROR
    "Packet-E cache source manifest is not the exact ordered two-file closure")
endif()
set(ZHAO_PACKET_E_CACHE_SOURCES)
foreach(relative_source IN LISTS ZHAO_PACKET_E_CACHE_RELATIVE_SOURCES)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-E cache source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_E_CACHE_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_e_cache_target TARGET ASSERTIONS)
  set(selector_args ${ARGN})
  set(assert_args)
  if(ASSERTIONS)
    list(APPEND assert_args --assert)
  else()
    list(APPEND assert_args --no-assert)
  endif()
  add_executable(${TARGET} texture/texture_cache_pipe_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  verilate(${TARGET}
    PREFIX Vzhao_texture_cache_pipe_v2
    TOP_MODULE zhao_texture_cache_pipe_v2
    SOURCES ${ZHAO_PACKET_E_CACHE_SOURCES}
    VERILATOR_ARGS ${assert_args} ${selector_args})
endfunction()

zhao_packet_e_cache_target(pe_c0 ON)
add_test(NAME packet_e_cache_directed COMMAND pe_c0)

zhao_packet_e_cache_target(pe_c8 ON -GLANES=8 -GREQN=2)
target_compile_definitions(pe_c8 PRIVATE EXPECT_PACKET_E_LANES8_REQN2=1)
add_test(NAME packet_e_cache_lanes8_reqn2 COMMAND pe_c8)

zhao_packet_e_cache_target(pe_cd ON
  -DZHAO_PACKET_E_MUTANT_DENIAL_REPLAYS)
target_compile_definitions(pe_cd PRIVATE
  EXPECT_PACKET_E_DENIAL_REPLAY_MUTANT=1)
add_test(NAME packet_e_cache_denial_replay_mutant COMMAND pe_cd)

# The same committed unreachable-state mutant has two controls: assertions-on
# proves the shipped exact-ownership assertion fires; assertions-off lets the
# focused inverse driver observe the wrong double reservation behavior itself.
zhao_packet_e_cache_target(pe_ca ON
  -DZHAO_PACKET_E_MUTANT_PREPAID_DOUBLE_RESV)
target_compile_definitions(pe_ca PRIVATE
  EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT=1)
add_test(NAME packet_e_cache_double_resv_assertion
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_e_assertion_control.py
    --exe $<TARGET_FILE:pe_ca>)

zhao_packet_e_cache_target(pe_cb OFF
  -DZHAO_PACKET_E_MUTANT_PREPAID_DOUBLE_RESV)
target_compile_definitions(pe_cb PRIVATE
  EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT=1)
add_test(NAME packet_e_cache_double_resv_behavior COMMAND pe_cb)

set_tests_properties(
  packet_e_cache_directed packet_e_cache_lanes8_reqn2
  PROPERTIES LABELS "fast;nightly;packet-e" TIMEOUT 1200
    FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(packet_e_cache_denial_replay_mutant PROPERTIES
  LABELS "fast;nightly;packet-e;mutant" TIMEOUT 1200
  PASS_REGULAR_EXPRESSION
    "texture_cache_pipe_v2_packet_e_denial_replay_mutant.*checks passed"
  FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(packet_e_cache_double_resv_assertion PROPERTIES
  LABELS "fast;nightly;packet-e;mutant" TIMEOUT 60
  PASS_REGULAR_EXPRESSION
    "PACKET_E_ASSERTION_CONTROL\\[double-reservation\\] FIRED"
  FAIL_REGULAR_EXPRESSION "FAIL")
set_tests_properties(packet_e_cache_double_resv_behavior PROPERTIES
  LABELS "fast;nightly;packet-e;mutant" TIMEOUT 1200
  PASS_REGULAR_EXPRESSION
    "texture_cache_pipe_v2_packet_e_double_resv_mutant.*checks passed"
  FAIL_REGULAR_EXPRESSION "FAIL")

# Packet E2: the public V3 closure remains Packet B's exact 26 ordered sources.
# Existing lab, production, and full-context targets are the healthy Packet-E
# profiles; four private selectors are inserted immediately before the top.
set(ZHAO_PACKET_E_TOP_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/texture/texture_island_v3_packet_e.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_E_TOP_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-E top source manifest is missing: ${ZHAO_PACKET_E_TOP_SOURCE_MANIFEST}")
endif()
file(STRINGS "${ZHAO_PACKET_E_TOP_SOURCE_MANIFEST}"
  ZHAO_PACKET_E_TOP_RELATIVE_SOURCES
  REGEX "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$")
list(LENGTH ZHAO_PACKET_E_TOP_RELATIVE_SOURCES ZHAO_PACKET_E_TOP_SOURCE_COUNT)
if(NOT ZHAO_PACKET_E_TOP_SOURCE_COUNT EQUAL 26)
  message(FATAL_ERROR "Packet-E top source manifest must contain exactly 26 SV paths")
endif()
set(ZHAO_PACKET_E_TOP_SOURCES)
foreach(relative_source IN LISTS ZHAO_PACKET_E_TOP_RELATIVE_SOURCES)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-E top source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_E_TOP_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()
if(NOT ZHAO_PACKET_E_TOP_SOURCES STREQUAL ZHAO_TEXTURE_V3_PACKET_B_SOURCES)
  message(FATAL_ERROR
    "Packet-E top source manifest diverges from Packet B's exact 26-source authority")
endif()
set(ZHAO_PACKET_E_TOP_MUTANT_SOURCES ${ZHAO_PACKET_E_TOP_SOURCES})
list(INSERT ZHAO_PACKET_E_TOP_MUTANT_SOURCES 25
  ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_texture_island_v3_packet_b_mutants.sv)

function(zhao_packet_e_top_mutant NAME CPP_SELECTOR SV_SELECTOR DIAGNOSTIC)
  string(MD5 target_hash "${NAME}")
  string(SUBSTRING "${target_hash}" 0 8 target_hash)
  set(target pe_t${target_hash})
  add_executable(${target}
    texture/texture_island_v3_packet_b_directed.cpp)
  target_compile_definitions(${target} PRIVATE ${CPP_SELECTOR}=1)
  target_link_libraries(${target} PRIVATE zhao_harness)
  verilate(${target}
    PREFIX Vzhao_texture_island_v3_top
    TOP_MODULE zhao_texture_island_v3_top
    SOURCES ${ZHAO_PACKET_E_TOP_MUTANT_SOURCES}
    VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=1 -D${SV_SELECTOR})
  add_test(NAME packet_e_top_${NAME}_mutant COMMAND ${target})
  set_tests_properties(packet_e_top_${NAME}_mutant PROPERTIES
    LABELS "fast;nightly;packet-e;mutant" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "packet-b directed FAIL")
endfunction()

zhao_packet_e_top_mutant(
  historical_fill_lifetime PACKET_E_EXPECT_PRE_E_FILL_LIFETIME
  ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
  "packet-e historical-pre-E-fill-lifetime mutant FIRED")
zhao_packet_e_top_mutant(
  relabel_refusal PACKET_E_EXPECT_RELABEL_REFUSAL_ERR
  ZHAO_PACKET_E_MUTANT_RELABEL_REFUSAL_ERR
  "packet-e refusal-relabel-to-ERR mutant FIRED")
zhao_packet_e_top_mutant(
  refusal_native_arith PACKET_E_EXPECT_REFUSAL_NATIVE_ARITH
  ZHAO_PACKET_E_MUTANT_REFUSAL_ENTERS_NATIVE
  "packet-e refusal-enters-native-arithmetic mutant FIRED")
zhao_packet_e_top_mutant(
  drop_held_refusal PACKET_E_EXPECT_DROP_HELD_REFUSAL
  ZHAO_PACKET_E_MUTANT_DROP_HELD_REFUSAL
  "packet-e dropped-held-refusal mutant FIRED")

set_property(TEST
  texture_island_v3_packet_b_directed
  texture_island_v3_packet_b_production_profile
  texture_island_v3_packet_b_fullctx
  APPEND PROPERTY LABELS "packet-e")

# Packet E3: excluded local geometry/texture-fill owners share real ENGINE1
# only behind MEM.GUARD. The raw return includes independent last explicitly.
set(ZHAO_PACKET_E_MUX_SOURCE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/memory/render_asset_mux.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_E_MUX_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "required Packet-E mux source manifest is missing: ${ZHAO_PACKET_E_MUX_SOURCE_MANIFEST}")
endif()
file(STRINGS "${ZHAO_PACKET_E_MUX_SOURCE_MANIFEST}"
  ZHAO_PACKET_E_MUX_RELATIVE_SOURCES
  REGEX "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$")
if(NOT ZHAO_PACKET_E_MUX_RELATIVE_SOURCES STREQUAL
   "fpga/rtl/generated/zhao_abi_pkg.sv;fpga/rtl/common/zhao_pkg.sv;fpga/rtl/memory/zhao_mem_guard.sv;tests/mutants/zhao_render_asset_mux_mutants.sv;fpga/rtl/memory/zhao_render_asset_mux.sv;tests/memory/tb_render_asset_mux.sv")
  message(FATAL_ERROR
    "Packet-E mux source manifest is not the exact ordered six-file closure")
endif()
set(ZHAO_PACKET_E_MUX_SOURCES)
foreach(relative_source IN LISTS ZHAO_PACKET_E_MUX_RELATIVE_SOURCES)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR
      "missing Packet-E mux source-manifest path: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_E_MUX_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_e_mux_target TARGET)
  set(selector_args ${ARGN})
  add_executable(${TARGET} memory/render_asset_mux_directed.cpp)
  verilate(${TARGET}
    PREFIX Vtb_render_asset_mux
    TOP_MODULE tb_render_asset_mux
    SOURCES ${ZHAO_PACKET_E_MUX_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
endfunction()

zhao_packet_e_mux_target(pe_m0)
add_test(NAME packet_e_render_asset_mux_directed COMMAND pe_m0)

function(zhao_packet_e_mux_mutant NAME TARGET CPP_SELECTOR SV_SELECTOR SUITE)
  zhao_packet_e_mux_target(${TARGET} -D${SV_SELECTOR})
  target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  add_test(NAME packet_e_render_asset_mux_${NAME}_mutant COMMAND ${TARGET})
  set_tests_properties(packet_e_render_asset_mux_${NAME}_mutant PROPERTIES
    LABELS "fast;nightly;packet-e;mutant" TIMEOUT 1200
    PASS_REGULAR_EXPRESSION "${SUITE}.*checks passed"
    FAIL_REGULAR_EXPRESSION "FAIL")
endfunction()

zhao_packet_e_mux_mutant(
  hold_guard_valid pe_m1 EXPECT_RENDER_ASSET_HOLD_GUARD_VALID_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_HOLD_GUARD_VALID
  render_asset_mux_hold_guard_valid_mutant)
zhao_packet_e_mux_mutant(
  drift_subowner pe_m2 EXPECT_RENDER_ASSET_DRIFT_SUBOWNER_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_DRIFT_SUBOWNER
  render_asset_mux_drift_subowner_mutant)
zhao_packet_e_mux_mutant(
  texture_pack64 pe_m3 EXPECT_RENDER_ASSET_TEXTURE_PACK64_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_TEXTURE_PACK64
  render_asset_mux_texture_pack64_mutant)
zhao_packet_e_mux_mutant(
  terminate_beat7 pe_m4 EXPECT_RENDER_ASSET_TERMINATE_BEAT7_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_TERMINATE_BEAT7
  render_asset_mux_terminate_beat7_mutant)
zhao_packet_e_mux_mutant(
  accept_beat9 pe_m5 EXPECT_RENDER_ASSET_ACCEPT_BEAT9_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_ACCEPT_BEAT9
  render_asset_mux_accept_beat9_mutant)
zhao_packet_e_mux_mutant(
  denial_silence pe_m6 EXPECT_RENDER_ASSET_DENIAL_SILENCE_MUTANT
  ZHAO_RENDER_ASSET_MUTANT_DENIAL_SILENCE
  render_asset_mux_denial_silence_mutant)

set_tests_properties(packet_e_render_asset_mux_directed PROPERTIES
  LABELS "fast;nightly;packet-e" TIMEOUT 1200
  FAIL_REGULAR_EXPRESSION "FAIL")

# Dual-selector detectors are executable controls. The helper gives every raw
# compiler subprocess a hard timeout and checks its exact collision diagnostic;
# CTest therefore observes a normal PASS instead of owning a hung child process.
function(zhao_packet_e_selector_control NAME PROFILE)
  add_test(NAME ${NAME}
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_e_selector_collision.py
      --profile ${PROFILE}
      --verilator "${ZHAO_VERILATOR_LINT}"
      --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(${NAME} PROPERTIES
    LABELS "fast;nightly;packet-e;mutant" TIMEOUT 180
    PASS_REGULAR_EXPRESSION "PACKET_E_SELECTOR_COLLISION\\[${PROFILE}\\] FIRED"
    FAIL_REGULAR_EXPRESSION "FAIL")
endfunction()

zhao_packet_e_selector_control(packet_e_cache_sv_selector_collision cache-sv)
zhao_packet_e_selector_control(packet_e_cache_cpp_selector_collision cache-cpp)
zhao_packet_e_selector_control(packet_e_top_sv_selector_collision top-sv)
zhao_packet_e_selector_control(packet_e_top_cpp_selector_collision top-cpp)
zhao_packet_e_selector_control(packet_e_mux_sv_selector_collision mux-sv)
zhao_packet_e_selector_control(packet_e_mux_cpp_selector_collision mux-cpp)

add_test(NAME packet_e_registration_static
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_e.py -q)
set_tests_properties(packet_e_registration_static PROPERTIES
  LABELS "fast;nightly;packet-e" TIMEOUT 1800)

# Independent configure-time inventory: deleting the static checker registration
# or any executable detector cannot make the remaining Packet-E set look complete.
set(ZHAO_PACKET_E_REQUIRED_TESTS
  packet_e_cache_directed
  packet_e_cache_lanes8_reqn2
  packet_e_cache_denial_replay_mutant
  packet_e_cache_double_resv_assertion
  packet_e_cache_double_resv_behavior
  texture_island_v3_packet_b_directed
  texture_island_v3_packet_b_production_profile
  texture_island_v3_packet_b_fullctx
  packet_e_top_historical_fill_lifetime_mutant
  packet_e_top_relabel_refusal_mutant
  packet_e_top_refusal_native_arith_mutant
  packet_e_top_drop_held_refusal_mutant
  packet_e_render_asset_mux_directed
  packet_e_render_asset_mux_hold_guard_valid_mutant
  packet_e_render_asset_mux_drift_subowner_mutant
  packet_e_render_asset_mux_texture_pack64_mutant
  packet_e_render_asset_mux_terminate_beat7_mutant
  packet_e_render_asset_mux_accept_beat9_mutant
  packet_e_render_asset_mux_denial_silence_mutant
  packet_e_cache_sv_selector_collision
  packet_e_cache_cpp_selector_collision
  packet_e_top_sv_selector_collision
  packet_e_top_cpp_selector_collision
  packet_e_mux_sv_selector_collision
  packet_e_mux_cpp_selector_collision
  packet_e_registration_static)
list(LENGTH ZHAO_PACKET_E_REQUIRED_TESTS ZHAO_PACKET_E_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_E_REQUIRED_TEST_COUNT EQUAL 26)
  message(FATAL_ERROR "Packet-E required CTest inventory must contain exactly 26 names")
endif()
foreach(required_packet_e_test IN LISTS ZHAO_PACKET_E_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_e_test}")
    message(FATAL_ERROR
      "required Packet-E CTest is not registered: ${required_packet_e_test}")
  endif()
endforeach()

# Packet F / G8A: exact generated physical-pin/MISR wrapper around the connected
# Packet-D tile hierarchy. The same 48-source order is frozen in its generated
# manifest and design/fit_targets.yml; the static gate compares all three.
set(ZHAO_G8A_RASTER_TEXTURE_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_render_texture_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_skid2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_mul27_exact.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dual18_mul.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mod255.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_div6.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_mosaic_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_palette_res_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3bank.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3rq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_v3own.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_metajoin_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_uv_join_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_early_desc_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_material_combine_v3.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_texture_island_v3_top.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_texture_stage_v3.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fill.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_edgewalk.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrgrad_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_earlyz.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_prod.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_blend_fin.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_fragment.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tilestore.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_div255.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_quant.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_resolve.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv)
list(LENGTH ZHAO_G8A_RASTER_TEXTURE_SOURCES ZHAO_G8A_SOURCE_COUNT)
if(NOT ZHAO_G8A_SOURCE_COUNT EQUAL 48)
  message(FATAL_ERROR "G8A raster/texture fit closure must contain exactly 48 sources")
endif()
foreach(g8a_source IN LISTS ZHAO_G8A_RASTER_TEXTURE_SOURCES)
  if(NOT EXISTS "${g8a_source}")
    message(FATAL_ERROR "required G8A raster/texture source is missing: ${g8a_source}")
  endif()
endforeach()

add_executable(pf_g8a raster/raster_texture_v3_fit_top_directed.cpp)
verilate(pf_g8a
  PREFIX Vzhao_raster_texture_v3_fit_top
  TOP_MODULE zhao_raster_texture_v3_fit_top
  SOURCES ${ZHAO_G8A_RASTER_TEXTURE_SOURCES}
  VERILATOR_ARGS --assert -GATTR_DSP3=1 -GBILERP_DSP2=1
    -DZHAO_DUAL18_BEHAVIORAL)
add_test(NAME raster_texture_v3_fit_top_directed COMMAND pf_g8a)

add_test(NAME lint_raster_texture_v3_fit_top
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall -Wno-fatal
    --top-module zhao_raster_texture_v3_fit_top
    -GATTR_DSP3=1 -GBILERP_DSP2=1
    -DQUARTUS_SYNTHESIS=1 -DSYNTHESIS=1 -DZHAO_DUAL18_BEHAVIORAL
    ${ZHAO_G8A_RASTER_TEXTURE_SOURCES})
set_tests_properties(lint_raster_texture_v3_fit_top PROPERTIES
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME raster_texture_v3_fit_top_generated_freshness
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/quartus/gen_raster_texture_v3_fit_top.py --check)
add_test(NAME packet_f_g8a_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_raster_texture_v3_fit_top.py -q)
set_tests_properties(
  raster_texture_v3_fit_top_directed
  lint_raster_texture_v3_fit_top
  raster_texture_v3_fit_top_generated_freshness
  packet_f_g8a_registration_static
  PROPERTIES LABELS "fast;nightly;packet-f;g8a" TIMEOUT 1800
    FAIL_REGULAR_EXPRESSION "FAIL")

# ---------------------------------------------------------------------------
# PACKET I / G8B -- the parameter-fixed terrain characterization wrapper.
#
# Same four gates as Packet F's, for the same reasons: a directed activity
# witness so the fit cannot characterise an idle box, a synthesis-mode lint, a
# generated-freshness check so the committed wrapper cannot drift from its
# generator, and a static registration check.
set(ZHAO_G8B_TERRAIN_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_core.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_service.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_proj_subsystem.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_vertex_arena.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_wcache.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tess.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_group_seq.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_pipe.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv)
list(LENGTH ZHAO_G8B_TERRAIN_SOURCES ZHAO_G8B_SOURCE_COUNT)
if(NOT ZHAO_G8B_SOURCE_COUNT EQUAL 10)
  message(FATAL_ERROR "G8B terrain fit closure must contain exactly 10 sources")
endif()
foreach(g8b_source IN LISTS ZHAO_G8B_TERRAIN_SOURCES)
  if(NOT EXISTS "${g8b_source}")
    message(FATAL_ERROR "required G8B terrain source is missing: ${g8b_source}")
  endif()
endforeach()

add_executable(pi_g8b terrain/terrain_pipe_rpp3_matw18_fit_top_directed.cpp)
target_link_libraries(pi_g8b PRIVATE zhao_harness)
verilate(pi_g8b
  PREFIX Vzhao_terrain_pipe_rpp3_matw18_fit_top
  TOP_MODULE zhao_terrain_pipe_rpp3_matw18_fit_top
  SOURCES ${ZHAO_G8B_TERRAIN_SOURCES}
  VERILATOR_ARGS --assert)
add_test(NAME terrain_pipe_rpp3_matw18_fit_top_directed COMMAND pi_g8b)

add_test(NAME lint_terrain_pipe_rpp3_matw18_fit_top
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall -Wno-fatal
    --top-module zhao_terrain_pipe_rpp3_matw18_fit_top
    -DQUARTUS_SYNTHESIS=1 -DSYNTHESIS=1
    ${ZHAO_G8B_TERRAIN_SOURCES})
set_tests_properties(lint_terrain_pipe_rpp3_matw18_fit_top PROPERTIES
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME terrain_pipe_rpp3_matw18_generated_freshness
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_SOURCE_DIR}/tools/quartus/gen_terrain_pipe_rpp3_matw18_fit_top.py
    --check)

# THE FOURTH GATE, which the paragraph above has promised since Packet I was
# registered and which nothing delivered. Packet F has its static check at
# tests/tools/test_raster_texture_v3_fit_top.py; Packet I had three gates and
# a comment claiming four, so the four places that state G8B's source closure
# could drift apart in silence and the architecture's own receipt conditions
# were enforced by nobody. It also carries the G8B RECEIPT LAW: a row that
# stops being stamped `failed` must carry clean-tree, zero-virtual-pin and
# 100 MHz together, so a promoted diagnostic row turns it red.
add_test(NAME packet_i_g8b_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_terrain_pipe_rpp3_matw18_fit_top.py -q)

set_tests_properties(
  terrain_pipe_rpp3_matw18_fit_top_directed
  lint_terrain_pipe_rpp3_matw18_fit_top
  terrain_pipe_rpp3_matw18_generated_freshness
  packet_i_g8b_registration_static
  PROPERTIES LABELS "fast;nightly;packet-i;g8b" TIMEOUT 1800
    FAIL_REGULAR_EXPRESSION "FAIL")

# The same inventory guard Packet F carries: registering a test and listing it
# as required are two different acts, and a gate that is quietly dropped from
# the file leaves no trace anywhere else.
set(ZHAO_PACKET_I_REQUIRED_TESTS
  terrain_pipe_rpp3_matw18_fit_top_directed
  lint_terrain_pipe_rpp3_matw18_fit_top
  terrain_pipe_rpp3_matw18_generated_freshness
  packet_i_g8b_registration_static)
list(LENGTH ZHAO_PACKET_I_REQUIRED_TESTS ZHAO_PACKET_I_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_I_REQUIRED_TEST_COUNT EQUAL 4)
  message(FATAL_ERROR "Packet-I required CTest inventory must contain exactly 4 names")
endif()
foreach(required_packet_i_test IN LISTS ZHAO_PACKET_I_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_i_test}")
    message(FATAL_ERROR
      "required Packet-I CTest is not registered: ${required_packet_i_test}")
  endif()
endforeach()

set(ZHAO_PACKET_F_REQUIRED_TESTS
  raster_texture_v3_fit_top_directed
  lint_raster_texture_v3_fit_top
  raster_texture_v3_fit_top_generated_freshness
  packet_f_g8a_registration_static)
list(LENGTH ZHAO_PACKET_F_REQUIRED_TESTS ZHAO_PACKET_F_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_F_REQUIRED_TEST_COUNT EQUAL 4)
  message(FATAL_ERROR "Packet-F required CTest inventory must contain exactly 4 names")
endif()
foreach(required_packet_f_test IN LISTS ZHAO_PACKET_F_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_f_test}")
    message(FATAL_ERROR
      "required Packet-F CTest is not registered: ${required_packet_f_test}")
  endif()
endforeach()

# Packet G: writer-aware framebuffer lease authority and generation-bearing,
# bidirectional READY/swap CDC. Both V2 blocks remain unselected until Packet H.
function(zhao_packet_g_load_sources MANIFEST EXPECTED_COUNT OUT_VAR)
  if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "required Packet-G source manifest is missing: ${MANIFEST}")
  endif()
  file(STRINGS "${MANIFEST}" zhao_packet_g_relative_sources)
  list(LENGTH zhao_packet_g_relative_sources zhao_packet_g_source_count)
  if(NOT zhao_packet_g_source_count EQUAL ${EXPECTED_COUNT})
    message(FATAL_ERROR
      "Packet-G source manifest ${MANIFEST} must contain exactly ${EXPECTED_COUNT} rows")
  endif()
  set(zhao_packet_g_sources)
  set(zhao_packet_g_seen)
  foreach(relative_source IN LISTS zhao_packet_g_relative_sources)
    if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
       NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
       relative_source MATCHES "(^|/)\\.\\.?(/|$)")
      message(FATAL_ERROR "invalid Packet-G source path: ${relative_source}")
    endif()
    list(FIND zhao_packet_g_seen "${relative_source}" duplicate_index)
    if(NOT duplicate_index EQUAL -1)
      message(FATAL_ERROR "duplicate Packet-G source path: ${relative_source}")
    endif()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
      message(FATAL_ERROR "missing Packet-G source: ${relative_source}")
    endif()
    list(APPEND zhao_packet_g_seen "${relative_source}")
    list(APPEND zhao_packet_g_sources "${CMAKE_SOURCE_DIR}/${relative_source}")
  endforeach()
  set(${OUT_VAR} "${zhao_packet_g_sources}" PARENT_SCOPE)
endfunction()

set(ZHAO_PACKET_G_SLOTMGR_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/packet_g_slotmgr_v2.sources.txt)
set(ZHAO_PACKET_G_CDC_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/packet_g_cdc_v2.sources.txt)
set(ZHAO_PACKET_G_CONNECTED_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/packet_g_connected.sources.txt)
zhao_packet_g_load_sources(
  "${ZHAO_PACKET_G_SLOTMGR_MANIFEST}" 4 ZHAO_PACKET_G_SLOTMGR_SOURCES)
zhao_packet_g_load_sources(
  "${ZHAO_PACKET_G_CDC_MANIFEST}" 3 ZHAO_PACKET_G_CDC_SOURCES)
zhao_packet_g_load_sources(
  "${ZHAO_PACKET_G_CONNECTED_MANIFEST}" 11 ZHAO_PACKET_G_CONNECTED_SOURCES)

function(zhao_packet_g_slot_test TARGET MUTANT EXPECTED)
  add_executable(${TARGET} video/video_slotmgr_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${EXPECTED}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${EXPECTED})
  endif()
  set(zhao_slot_mutant_arg)
  if(NOT "${MUTANT}" STREQUAL "")
    set(zhao_slot_mutant_arg -D${MUTANT})
  endif()
  verilate(${TARGET}
    PREFIX Vzhao_video_slotmgr_v2
    TOP_MODULE zhao_video_slotmgr_v2
    VERILATOR_ARGS --assert ${zhao_slot_mutant_arg}
    SOURCES ${ZHAO_PACKET_G_SLOTMGR_SOURCES})
  add_test(NAME ${TARGET} COMMAND ${TARGET})
  set_tests_properties(${TARGET} PROPERTIES
    LABELS "fast;nightly;packet-g;slotmgr" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "passed|DETECTED")
endfunction()

zhao_packet_g_slot_test(packet_g_slotmgr_directed "" "")
zhao_packet_g_slot_test(packet_g_slotmgr_term_omit_writer_mutant
  ZHAO_SLOT_V2_MUTANT_TERM_OMIT_WRITER ZHAO_EXPECT_SLOT_MUTANT_TERM_OMIT_WRITER)
zhao_packet_g_slot_test(packet_g_slotmgr_term_slot_only_mutant
  ZHAO_SLOT_V2_MUTANT_TERM_SLOT_ONLY ZHAO_EXPECT_SLOT_MUTANT_TERM_SLOT_ONLY)
zhao_packet_g_slot_test(packet_g_slotmgr_fault_publishes_mutant
  ZHAO_SLOT_V2_MUTANT_FAULT_PUBLISHES ZHAO_EXPECT_SLOT_MUTANT_FAULT_PUBLISHES)
zhao_packet_g_slot_test(packet_g_slotmgr_slot0_base_mutant
  ZHAO_SLOT_V2_MUTANT_SLOT0_BASE ZHAO_EXPECT_SLOT_MUTANT_SLOT0_BASE)
zhao_packet_g_slot_test(packet_g_slotmgr_swap_omit_generation_mutant
  ZHAO_SLOT_V2_MUTANT_SWAP_OMIT_GENERATION
  ZHAO_EXPECT_SLOT_MUTANT_SWAP_OMIT_GENERATION)
zhao_packet_g_slot_test(packet_g_slotmgr_live_ready_writer_mutant
  ZHAO_SLOT_V2_MUTANT_LIVE_READY_WRITER
  ZHAO_EXPECT_SLOT_MUTANT_LIVE_READY_WRITER)

function(zhao_packet_g_cdc_test TARGET MUTANT EXPECTED)
  add_executable(${TARGET} video/fb_ready_cdc_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${EXPECTED}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${EXPECTED})
  endif()
  set(zhao_cdc_mutant_arg)
  if(NOT "${MUTANT}" STREQUAL "")
    set(zhao_cdc_mutant_arg -D${MUTANT})
  endif()
  # The full-guard behavioral inverse must observe the fifth acceptance. The
  # separate assertion control below compiles the same mutant with assertions.
  if("${MUTANT}" STREQUAL "ZHAO_FB_CDC_MUTANT_IGNORE_FULL")
    list(APPEND zhao_cdc_mutant_arg -DSYNTHESIS)
  endif()
  verilate(${TARGET}
    PREFIX Vzhao_fb_ready_cdc_v2
    TOP_MODULE zhao_fb_ready_cdc_v2
    VERILATOR_ARGS --assert ${zhao_cdc_mutant_arg}
    SOURCES ${ZHAO_PACKET_G_CDC_SOURCES})
  add_test(NAME ${TARGET} COMMAND ${TARGET})
  set_tests_properties(${TARGET} PROPERTIES
    LABELS "fast;nightly;packet-g;cdc" TIMEOUT 1800
    PASS_REGULAR_EXPRESSION "passed|DETECTED")
endfunction()

zhao_packet_g_cdc_test(packet_g_cdc_directed "" "")
zhao_packet_g_cdc_test(packet_g_cdc_ignore_full_mutant
  ZHAO_FB_CDC_MUTANT_IGNORE_FULL ZHAO_EXPECT_CDC_MUTANT_IGNORE_FULL)
zhao_packet_g_cdc_test(packet_g_cdc_bypass_barrier_mutant
  ZHAO_FB_CDC_MUTANT_BYPASS_BARRIER ZHAO_EXPECT_CDC_MUTANT_BYPASS_BARRIER)
zhao_packet_g_cdc_test(packet_g_cdc_zero_ready_generation_mutant
  ZHAO_FB_CDC_MUTANT_ZERO_READY_GENERATION
  ZHAO_EXPECT_CDC_MUTANT_ZERO_READY_GENERATION)
zhao_packet_g_cdc_test(packet_g_cdc_repeat_read_mutant
  ZHAO_FB_CDC_MUTANT_REPEAT_READ ZHAO_EXPECT_CDC_MUTANT_REPEAT_READ)

add_executable(packet_g_cdc_ignore_full_assertion_exe
  video/fb_ready_cdc_v2_directed.cpp)
target_link_libraries(packet_g_cdc_ignore_full_assertion_exe PRIVATE zhao_harness)
target_compile_definitions(packet_g_cdc_ignore_full_assertion_exe PRIVATE
  ZHAO_EXPECT_CDC_ASSERT_IGNORE_FULL)
verilate(packet_g_cdc_ignore_full_assertion_exe
  PREFIX Vzhao_fb_ready_cdc_v2
  TOP_MODULE zhao_fb_ready_cdc_v2
  VERILATOR_ARGS --assert -DZHAO_FB_CDC_MUTANT_IGNORE_FULL
  SOURCES ${ZHAO_PACKET_G_CDC_SOURCES})
add_test(NAME packet_g_cdc_ignore_full_assertion
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_g_assertion_control.py
    --exe $<TARGET_FILE:packet_g_cdc_ignore_full_assertion_exe>)
set_tests_properties(packet_g_cdc_ignore_full_assertion PROPERTIES
  LABELS "fast;mutant;packet-g;cdc" TIMEOUT 30)

add_executable(packet_g_connected_directed
  video/packet_g_connected_directed.cpp)
target_link_libraries(packet_g_connected_directed PRIVATE zhao_harness)
verilate(packet_g_connected_directed
  PREFIX Vtb_packet_g_lease_cdc
  TOP_MODULE tb_packet_g_lease_cdc
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_PACKET_G_CONNECTED_SOURCES})
add_test(NAME packet_g_connected_directed COMMAND packet_g_connected_directed)
set_tests_properties(packet_g_connected_directed PROPERTIES
  LABELS "fast;nightly;packet-g;connected" TIMEOUT 1800)

add_executable(packet_g_connected_raw_terminal_mutant
  video/packet_g_connected_directed.cpp)
target_compile_definitions(packet_g_connected_raw_terminal_mutant PRIVATE
  ZHAO_EXPECT_PACKET_G_CONNECTED_RAW_TERMINAL)
target_link_libraries(packet_g_connected_raw_terminal_mutant PRIVATE zhao_harness)
verilate(packet_g_connected_raw_terminal_mutant
  PREFIX Vtb_packet_g_lease_cdc
  TOP_MODULE tb_packet_g_lease_cdc
  VERILATOR_ARGS --assert -DZHAO_PACKET_G_CONNECTED_MUTANT_RAW_TERMINAL_READY
  SOURCES ${ZHAO_PACKET_G_CONNECTED_SOURCES})
add_test(NAME packet_g_connected_raw_terminal_mutant
  COMMAND packet_g_connected_raw_terminal_mutant)
set_tests_properties(packet_g_connected_raw_terminal_mutant PROPERTIES
  LABELS "fast;nightly;mutant;packet-g;connected" TIMEOUT 1800
  PASS_REGULAR_EXPRESSION "DETECTED")

foreach(packet_g_collision_profile IN ITEMS slot-sv slot-cpp cdc-sv cdc-cpp)
  string(REPLACE "-" "_" packet_g_collision_name
    "${packet_g_collision_profile}")
  add_test(NAME packet_g_${packet_g_collision_name}_selector_collision
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_g_selector_collision.py
      --profile ${packet_g_collision_profile}
      --verilator "${ZHAO_VERILATOR_LINT}"
      --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(packet_g_${packet_g_collision_name}_selector_collision
    PROPERTIES LABELS "fast;mutant;packet-g" TIMEOUT 180)
endforeach()

add_test(NAME packet_g_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_render_texture_packet_g.py -q)
set_tests_properties(packet_g_registration_static PROPERTIES
  LABELS "fast;packet-g" TIMEOUT 180)

add_test(NAME lint_zhao_video_slotmgr_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_video_slotmgr_v2 ${ZHAO_PACKET_G_SLOTMGR_SOURCES})
add_test(NAME lint_zhao_fb_ready_cdc_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_fb_ready_cdc_v2 ${ZHAO_PACKET_G_CDC_SOURCES})
set_tests_properties(lint_zhao_video_slotmgr_v2 lint_zhao_fb_ready_cdc_v2
  PROPERTIES LABELS "lint;fast;packet-g"
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

set(ZHAO_PACKET_G_REQUIRED_TESTS
  packet_g_slotmgr_directed
  packet_g_slotmgr_term_omit_writer_mutant
  packet_g_slotmgr_term_slot_only_mutant
  packet_g_slotmgr_fault_publishes_mutant
  packet_g_slotmgr_slot0_base_mutant
  packet_g_slotmgr_swap_omit_generation_mutant
  packet_g_slotmgr_live_ready_writer_mutant
  packet_g_cdc_directed
  packet_g_cdc_ignore_full_mutant
  packet_g_cdc_bypass_barrier_mutant
  packet_g_cdc_zero_ready_generation_mutant
  packet_g_cdc_repeat_read_mutant
  packet_g_cdc_ignore_full_assertion
  packet_g_connected_directed
  packet_g_connected_raw_terminal_mutant
  packet_g_slot_sv_selector_collision
  packet_g_slot_cpp_selector_collision
  packet_g_cdc_sv_selector_collision
  packet_g_cdc_cpp_selector_collision
  packet_g_registration_static
  lint_zhao_video_slotmgr_v2
  lint_zhao_fb_ready_cdc_v2)
list(LENGTH ZHAO_PACKET_G_REQUIRED_TESTS ZHAO_PACKET_G_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_G_REQUIRED_TEST_COUNT EQUAL 22)
  message(FATAL_ERROR "Packet-G required CTest inventory must contain exactly 22 names")
endif()
foreach(required_packet_g_test IN LISTS ZHAO_PACKET_G_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_g_test}")
    message(FATAL_ERROR
      "required Packet-G CTest is not registered: ${required_packet_g_test}")
  endif()
endforeach()

# ---------------------------------------------------------------------------
# Packet H: independent ENGINE1 raw16 LAST
# ---------------------------------------------------------------------------
set(ZHAO_PACKET_H_RAW_LAST_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/memory/engine1_raw_last_v2.sources.txt)
if(NOT EXISTS "${ZHAO_PACKET_H_RAW_LAST_MANIFEST}")
  message(FATAL_ERROR "Packet-H raw-LAST source manifest is missing")
endif()
file(STRINGS "${ZHAO_PACKET_H_RAW_LAST_MANIFEST}"
  ZHAO_PACKET_H_RAW_LAST_RELATIVE_SOURCES)
if(NOT ZHAO_PACKET_H_RAW_LAST_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_engine1_raw_last_v2_mutants.sv;fpga/rtl/memory/zhao_engine1_raw_last_v2.sv")
  message(FATAL_ERROR
    "Packet-H raw-LAST source manifest is not the exact two-file ordered closure")
endif()
set(ZHAO_PACKET_H_RAW_LAST_SOURCES)
set(ZHAO_PACKET_H_RAW_LAST_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_H_RAW_LAST_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-H raw-LAST source: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_H_RAW_LAST_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-H raw-LAST source: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-H raw-LAST source: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_H_RAW_LAST_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_H_RAW_LAST_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_h_raw_last_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} memory/engine1_raw_last_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vzhao_engine1_raw_last_v2
    TOP_MODULE zhao_engine1_raw_last_v2
    SOURCES ${ZHAO_PACKET_H_RAW_LAST_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME engine1_raw_last_v2_${NAME} COMMAND ${TARGET})
  set_tests_properties(engine1_raw_last_v2_${NAME} PROPERTIES
    LABELS "fast;nightly;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|checks failed")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST engine1_raw_last_v2_${NAME}
      APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_packet_h_raw_last_test(ph_raw directed "" ""
  "engine1_raw_last_v2: all checks passed")
zhao_packet_h_raw_last_test(ph_raw_e early_last_control
  ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_EARLY_LAST
  EXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT
  "engine1_raw_last_v2: inverse EARLY_LAST control")
zhao_packet_h_raw_last_test(ph_raw_m missing_last_control
  ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_MISSING_LAST
  EXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT
  "engine1_raw_last_v2: inverse MISSING_LAST control")
zhao_packet_h_raw_last_test(ph_raw_l late_last_control
  ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_LATE_LAST
  EXPECT_ENGINE1_RAW_LAST_V2_LATE_LAST_MUTANT
  "engine1_raw_last_v2: inverse LATE_LAST control")
zhao_packet_h_raw_last_test(ph_raw_s state_invalid_control
  ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_STATE_INVALID
  EXPECT_ENGINE1_RAW_LAST_V2_STATE_INVALID_MUTANT
  "engine1_raw_last_v2: inverse STATE_INVALID control")

foreach(raw_last_collision IN ITEMS sv cpp)
  add_test(NAME engine1_raw_last_v2_${raw_last_collision}_selector_collision
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_engine1_raw_last_v2_static.py
      --collision ${raw_last_collision}
      --verilator "${ZHAO_VERILATOR_LINT}"
      --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(engine1_raw_last_v2_${raw_last_collision}_selector_collision
    PROPERTIES LABELS "fast;mutant;packet-h" TIMEOUT 180)
endforeach()

add_test(NAME engine1_raw_last_v2_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_engine1_raw_last_v2_static.py -q)
set_tests_properties(engine1_raw_last_v2_registration_static PROPERTIES
  LABELS "fast;packet-h" TIMEOUT 180)

add_test(NAME lint_zhao_engine1_raw_last_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_engine1_raw_last_v2
    ${ZHAO_PACKET_H_RAW_LAST_SOURCES})
set_tests_properties(lint_zhao_engine1_raw_last_v2 PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

set(ZHAO_PACKET_H_RAW_LAST_REQUIRED_TESTS
  engine1_raw_last_v2_directed
  engine1_raw_last_v2_early_last_control
  engine1_raw_last_v2_missing_last_control
  engine1_raw_last_v2_late_last_control
  engine1_raw_last_v2_state_invalid_control
  engine1_raw_last_v2_sv_selector_collision
  engine1_raw_last_v2_cpp_selector_collision
  engine1_raw_last_v2_registration_static
  lint_zhao_engine1_raw_last_v2)
list(LENGTH ZHAO_PACKET_H_RAW_LAST_REQUIRED_TESTS
  ZHAO_PACKET_H_RAW_LAST_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_H_RAW_LAST_REQUIRED_TEST_COUNT EQUAL 9)
  message(FATAL_ERROR
    "Packet-H raw-LAST required CTest inventory must contain exactly 9 names")
endif()
foreach(required_raw_last_test IN LISTS ZHAO_PACKET_H_RAW_LAST_REQUIRED_TESTS)
  if(NOT TEST "${required_raw_last_test}")
    message(FATAL_ERROR
      "required Packet-H raw-LAST CTest is not registered: ${required_raw_last_test}")
  endif()
endforeach()

# ---------------------------------------------------------------------------
# Packet H: renderer lease and READY/blank bridge leaves
# ---------------------------------------------------------------------------
set(ZHAO_PACKET_H_RENDERER_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/renderer_lease_v2.sources.txt)
file(STRINGS "${ZHAO_PACKET_H_RENDERER_MANIFEST}"
  ZHAO_PACKET_H_RENDERER_RELATIVE_SOURCES)
if(NOT ZHAO_PACKET_H_RENDERER_RELATIVE_SOURCES STREQUAL
   "fpga/rtl/generated/zhao_abi_pkg.sv;fpga/rtl/common/zhao_pkg.sv;tests/mutants/zhao_renderer_lease_v2_mutants.sv;fpga/rtl/video/zhao_renderer_lease_v2.sv;tests/video/tb_renderer_lease_v2.sv")
  message(FATAL_ERROR "Packet-H renderer source manifest is not exact and ordered")
endif()
set(ZHAO_PACKET_H_RENDERER_SOURCES)
set(ZHAO_PACKET_H_RENDERER_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_H_RENDERER_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-H renderer source: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_H_RENDERER_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-H renderer source: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-H renderer source: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_H_RENDERER_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_H_RENDERER_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_h_renderer_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} video/renderer_lease_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vtb_renderer_lease_v2
    TOP_MODULE tb_renderer_lease_v2
    SOURCES ${ZHAO_PACKET_H_RENDERER_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME renderer_lease_v2_${NAME} COMMAND ${TARGET})
  set_tests_properties(renderer_lease_v2_${NAME} PROPERTIES
    LABELS "fast;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|FAILED")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST renderer_lease_v2_${NAME} APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_packet_h_renderer_test(ph_renderer directed "" ""
  "renderer_lease_v2_directed.*passed")
zhao_packet_h_renderer_test(ph_renderer_g lease_open_control
  ZHAO_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS
  "renderer_lease_lease_open_bypass.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_s slot_choice_control
  ZHAO_RENDERER_LEASE_MUTANT_SLOT_CHOICE
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SLOT_CHOICE
  "renderer_lease_slot_choice.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_h request_hold_control
  ZHAO_RENDERER_LEASE_MUTANT_REQUEST_HOLD
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_REQUEST_HOLD
  "renderer_lease_request_hold.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_w writer_response_control
  ZHAO_RENDERER_LEASE_MUTANT_WRITER_RESPONSE
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_WRITER_RESPONSE
  "renderer_lease_writer_response.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_c skip_clear_control
  ZHAO_RENDERER_LEASE_MUTANT_SKIP_CLEAR
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SKIP_CLEAR
  "renderer_lease_skip_clear.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_t bad_stride_control
  ZHAO_RENDERER_LEASE_MUTANT_BAD_STRIDE
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_BAD_STRIDE
  "renderer_lease_bad_stride.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_d duo_boundary_control
  ZHAO_RENDERER_LEASE_MUTANT_DUO_BOUNDARY
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_DUO_BOUNDARY
  "renderer_lease_duo_boundary.*DETECTED")
zhao_packet_h_renderer_test(ph_renderer_i stale_identity_control
  ZHAO_RENDERER_LEASE_MUTANT_STALE_IDENTITY
  ZHAO_EXPECT_RENDERER_LEASE_MUTANT_STALE_IDENTITY
  "renderer_lease_stale_identity.*DETECTED")

# ---------------------------------------------------------------------------
# Packet H: the writer-0 lease front end.
#
# zhao_renderer_lease_v2 acquires writer 1's lease and
# zhao_video_terminal_adapter_v2 already carries BOTH writers' terminal events
# back to the manager -- but nothing acquired the BLITTER's lease, so the
# retained zhao_debug_frameblit had no way to be told which slot and generation
# it owns. This leaf is that missing path.
#
# Its controls are the six ways the seam can be wired wrong, and the one worth
# naming is ISSUE_EARLY: the blitter latches fb_lease_generation_i on the SAME
# edge it accepts a request, so offering the request while the answer is still
# landing makes it latch the PRE-GRANT generation and every publication is then
# refused as stale by a slot manager that is working perfectly. The historical
# shell carries a four-state sequencer and a long comment about exactly this;
# the comment is now a test.
set(ZHAO_PACKET_H_BLIT_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/blit_lease_v2.sources.txt)
file(STRINGS "${ZHAO_PACKET_H_BLIT_MANIFEST}" ZHAO_PACKET_H_BLIT_RELATIVE_SOURCES)
if(NOT ZHAO_PACKET_H_BLIT_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_video_blit_lease_v2_mutants.sv;fpga/rtl/video/zhao_video_blit_lease_v2.sv")
  message(FATAL_ERROR "Packet-H blit source manifest is not exact and ordered")
endif()
set(ZHAO_PACKET_H_BLIT_SOURCES)
set(ZHAO_PACKET_H_BLIT_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_H_BLIT_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-H blit source: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_H_BLIT_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-H blit source: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-H blit source: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_H_BLIT_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_H_BLIT_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_h_blit_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} video/blit_lease_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vzhao_video_blit_lease_v2
    TOP_MODULE zhao_video_blit_lease_v2
    SOURCES ${ZHAO_PACKET_H_BLIT_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME blit_lease_v2_${NAME} COMMAND ${TARGET})
  set_tests_properties(blit_lease_v2_${NAME} PROPERTIES
    LABELS "fast;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|FAILED")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST blit_lease_v2_${NAME} APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_packet_h_blit_test(ph_blit directed "" "" "blit_lease_v2_directed.*passed")
zhao_packet_h_blit_test(ph_blit_a admission_bypass_control
  ZHAO_BLIT_LEASE_MUTANT_ADMISSION_BYPASS
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_ADMISSION_BYPASS
  "blit_lease_admission_bypass.*DETECTED")
zhao_packet_h_blit_test(ph_blit_w writer_response_control
  ZHAO_BLIT_LEASE_MUTANT_WRITER_RESPONSE
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_WRITER_RESPONSE
  "blit_lease_writer_response.*DETECTED")
zhao_packet_h_blit_test(ph_blit_h request_hold_control
  ZHAO_BLIT_LEASE_MUTANT_REQUEST_HOLD
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_REQUEST_HOLD
  "blit_lease_request_hold.*DETECTED")
zhao_packet_h_blit_test(ph_blit_r live_record_control
  ZHAO_BLIT_LEASE_MUTANT_LIVE_RECORD
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_LIVE_RECORD
  "blit_lease_live_record.*DETECTED")
zhao_packet_h_blit_test(ph_blit_e issue_early_control
  ZHAO_BLIT_LEASE_MUTANT_ISSUE_EARLY
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_ISSUE_EARLY
  "blit_lease_issue_early.*DETECTED")
zhao_packet_h_blit_test(ph_blit_f refused_valid_control
  ZHAO_BLIT_LEASE_MUTANT_REFUSED_VALID
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_REFUSED_VALID
  "blit_lease_refused_valid.*DETECTED")
zhao_packet_h_blit_test(ph_blit_g rsp_gated_control
  ZHAO_BLIT_LEASE_MUTANT_RSP_GATED
  ZHAO_EXPECT_BLIT_LEASE_MUTANT_RSP_GATED
  "blit_lease_rsp_gated.*DETECTED")

add_test(NAME lint_zhao_video_blit_lease_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
          --top-module zhao_video_blit_lease_v2
          ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_video_blit_lease_v2.sv)
set_tests_properties(lint_zhao_video_blit_lease_v2 PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# Packet H: the 84-bit READY/swap tuple layout.
#
# zhao_fb_ready_cdc_v2 carries "one frozen 84-bit tuple" and never looks
# inside; zhao_video_slotmgr_v2 presents the same information as six ports
# whose widths sum to exactly 84. The correspondence was certain and the FIELD
# ORDER was defined nowhere, so both the pack and the unpack belonged to a
# shell that does not exist yet and nothing would have caught them disagreeing.
#
# The mutant is the reason the directed test carries literals instead of a
# round trip: swapping the writer and slot bits tiles the 84 bits exactly, so
# the elaboration coverage check passes, and pack and unpack still agree with
# each other because both read the swapped constants. Only bit positions
# written somewhere else can see it.
set(ZHAO_FB_TUPLE_SOURCES
  ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_fb_tuple_mutants.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_pkg.sv
  ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_contract.sv
  ${CMAKE_CURRENT_SOURCE_DIR}/video/tb_fb_tuple.sv)

function(zhao_fb_tuple_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} video/fb_tuple_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vtb_fb_tuple
    TOP_MODULE tb_fb_tuple
    SOURCES ${ZHAO_FB_TUPLE_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME fb_tuple_${NAME} COMMAND ${TARGET})
  set_tests_properties(fb_tuple_${NAME} PROPERTIES
    LABELS "fast;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|FAILED")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST fb_tuple_${NAME} APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_fb_tuple_test(ph_tuple directed "" "" "fb_tuple_directed.*passed")
zhao_fb_tuple_test(ph_tuple_s swap_writer_slot_control
  ZHAO_FB_TUPLE_MUTANT_SWAP_WRITER_SLOT
  ZHAO_EXPECT_FB_TUPLE_MUTANT_SWAP_WRITER_SLOT
  "fb_tuple_swap_writer_slot.*DETECTED")

add_test(NAME lint_zhao_fb_tuple_pkg
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
          --top-module zhao_fb_tuple_contract
          ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/video/zhao_fb_tuple_contract.sv)
set_tests_properties(lint_zhao_fb_tuple_pkg PROPERTIES
  LABELS "lint;fast;packet-h"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")


set(ZHAO_PACKET_H_BRIDGE_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/video_ready_bridge_v2.sources.txt)
file(STRINGS "${ZHAO_PACKET_H_BRIDGE_MANIFEST}"
  ZHAO_PACKET_H_BRIDGE_RELATIVE_SOURCES)
if(NOT ZHAO_PACKET_H_BRIDGE_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_video_ready_bridge_v2_mutants.sv;fpga/rtl/generated/zhao_abi_pkg.sv;fpga/rtl/common/zhao_pkg.sv;fpga/rtl/video/zhao_fb_tuple_pkg.sv;fpga/rtl/video/zhao_video_ready_bridge_v2.sv;tests/video/tb_video_ready_bridge_v2.sv")
  message(FATAL_ERROR "Packet-H video bridge source manifest is not exact and ordered")
endif()
set(ZHAO_PACKET_H_BRIDGE_SOURCES)
set(ZHAO_PACKET_H_BRIDGE_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_H_BRIDGE_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-H video bridge source: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_H_BRIDGE_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-H video bridge source: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-H video bridge source: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_H_BRIDGE_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_H_BRIDGE_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_h_bridge_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} video/video_ready_bridge_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vtb_video_ready_bridge_v2
    TOP_MODULE tb_video_ready_bridge_v2
    SOURCES ${ZHAO_PACKET_H_BRIDGE_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME video_ready_bridge_v2_${NAME} COMMAND ${TARGET})
  set_tests_properties(video_ready_bridge_v2_${NAME} PROPERTIES
    LABELS "fast;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|FAILED")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST video_ready_bridge_v2_${NAME} APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_packet_h_bridge_test(ph_bridge directed "" ""
  "video_ready_bridge_v2_directed.*passed")
zhao_packet_h_bridge_test(ph_bridge_d drop_held_tuple_control
  ZHAO_VIDEO_BRIDGE_MUTANT_DROP_HELD_TUPLE
  ZHAO_EXPECT_VIDEO_BRIDGE_DROP_HELD_TUPLE
  "video_bridge_drop_held_tuple.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_c change_held_tuple_control
  ZHAO_VIDEO_BRIDGE_MUTANT_CHANGE_HELD_TUPLE
  ZHAO_EXPECT_VIDEO_BRIDGE_CHANGE_HELD_TUPLE
  "video_bridge_change_held_tuple.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_o wrong_onehot_control
  ZHAO_VIDEO_BRIDGE_MUTANT_WRONG_ONEHOT
  ZHAO_EXPECT_VIDEO_BRIDGE_WRONG_ONEHOT
  "video_bridge_wrong_onehot.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_b blank_ack_bypass_control
  ZHAO_VIDEO_BRIDGE_MUTANT_BLANK_ACK_BYPASS
  ZHAO_EXPECT_VIDEO_BRIDGE_BLANK_ACK_BYPASS
  "video_bridge_blank_ack_bypass.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_l early_lease_open_control
  ZHAO_VIDEO_BRIDGE_MUTANT_EARLY_LEASE_OPEN
  ZHAO_EXPECT_VIDEO_BRIDGE_EARLY_LEASE_OPEN
  "video_bridge_early_lease_open.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_r retain_reset_tuple_control
  ZHAO_VIDEO_BRIDGE_MUTANT_RETAIN_RESET_TUPLE
  ZHAO_EXPECT_VIDEO_BRIDGE_RETAIN_RESET_TUPLE
  "video_bridge_retain_reset_tuple.*DETECTED")
zhao_packet_h_bridge_test(ph_bridge_u unblank_scanout_only_control
  ZHAO_VIDEO_BRIDGE_MUTANT_UNBLANK_SCANOUT_ONLY
  ZHAO_EXPECT_VIDEO_BRIDGE_UNBLANK_SCANOUT_ONLY
  "video_bridge_unblank_scanout_only.*DETECTED")

# Negative-polarity control: the healthy RTL must miss the scanout-only defect.
add_executable(ph_bridge_unblank_negative video/video_ready_bridge_v2_directed.cpp)
target_link_libraries(ph_bridge_unblank_negative PRIVATE zhao_harness)
target_compile_definitions(ph_bridge_unblank_negative PRIVATE
  ZHAO_EXPECT_VIDEO_BRIDGE_UNBLANK_SCANOUT_ONLY=1)
verilate(ph_bridge_unblank_negative
  PREFIX Vtb_video_ready_bridge_v2
  TOP_MODULE tb_video_ready_bridge_v2
  SOURCES ${ZHAO_PACKET_H_BRIDGE_SOURCES}
  VERILATOR_ARGS --assert)
add_test(NAME video_ready_bridge_v2_unblank_negative_control
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_h_shell_prereqs.py
    --negative-exe "$<TARGET_FILE:ph_bridge_unblank_negative>"
    --diagnostic "[video_bridge_unblank_scanout_only] MISSED")
set_tests_properties(video_ready_bridge_v2_unblank_negative_control PROPERTIES
  LABELS "fast;mutant;packet-h" TIMEOUT 180)

# One-entry terminal join for pulse-only blitter and held renderer events.
set(ZHAO_PACKET_H_TERMINAL_MANIFEST
  ${CMAKE_CURRENT_SOURCE_DIR}/video/video_terminal_adapter_v2.sources.txt)
file(STRINGS "${ZHAO_PACKET_H_TERMINAL_MANIFEST}"
  ZHAO_PACKET_H_TERMINAL_RELATIVE_SOURCES)
if(NOT ZHAO_PACKET_H_TERMINAL_RELATIVE_SOURCES STREQUAL
   "tests/mutants/zhao_video_terminal_adapter_v2_mutants.sv;fpga/rtl/video/zhao_video_terminal_adapter_v2.sv;tests/video/tb_video_terminal_adapter_v2.sv")
  message(FATAL_ERROR "Packet-H terminal-adapter source manifest is not exact and ordered")
endif()
set(ZHAO_PACKET_H_TERMINAL_SOURCES)
set(ZHAO_PACKET_H_TERMINAL_SEEN)
foreach(relative_source IN LISTS ZHAO_PACKET_H_TERMINAL_RELATIVE_SOURCES)
  if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "\\\\" OR
     NOT relative_source MATCHES "^(fpga|tests)/[A-Za-z0-9_./-]+\\.sv$" OR
     relative_source MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "invalid Packet-H terminal-adapter source: ${relative_source}")
  endif()
  list(FIND ZHAO_PACKET_H_TERMINAL_SEEN "${relative_source}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "duplicate Packet-H terminal-adapter source: ${relative_source}")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")
    message(FATAL_ERROR "missing Packet-H terminal-adapter source: ${relative_source}")
  endif()
  list(APPEND ZHAO_PACKET_H_TERMINAL_SEEN "${relative_source}")
  list(APPEND ZHAO_PACKET_H_TERMINAL_SOURCES
    "${CMAKE_SOURCE_DIR}/${relative_source}")
endforeach()

function(zhao_packet_h_terminal_test TARGET NAME SV_SELECTOR CPP_SELECTOR DIAGNOSTIC)
  add_executable(${TARGET} video/video_terminal_adapter_v2_directed.cpp)
  target_link_libraries(${TARGET} PRIVATE zhao_harness)
  if(NOT "${CPP_SELECTOR}" STREQUAL "")
    target_compile_definitions(${TARGET} PRIVATE ${CPP_SELECTOR}=1)
  endif()
  set(selector_args)
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    list(APPEND selector_args -D${SV_SELECTOR})
  endif()
  verilate(${TARGET}
    PREFIX Vtb_video_terminal_adapter_v2
    TOP_MODULE tb_video_terminal_adapter_v2
    SOURCES ${ZHAO_PACKET_H_TERMINAL_SOURCES}
    VERILATOR_ARGS --assert ${selector_args})
  add_test(NAME video_terminal_adapter_v2_${NAME} COMMAND ${TARGET})
  set_tests_properties(video_terminal_adapter_v2_${NAME} PROPERTIES
    LABELS "fast;packet-h" TIMEOUT 300
    PASS_REGULAR_EXPRESSION "${DIAGNOSTIC}"
    FAIL_REGULAR_EXPRESSION "FAIL:|FAILED")
  if(NOT "${SV_SELECTOR}" STREQUAL "")
    set_property(TEST video_terminal_adapter_v2_${NAME} APPEND PROPERTY LABELS "mutant")
  endif()
endfunction()

zhao_packet_h_terminal_test(ph_term directed "" ""
  "video_terminal_adapter_v2_directed.*passed")
zhao_packet_h_terminal_test(ph_term_d drop_held_control
  ZHAO_VIDEO_TERM_MUTANT_DROP_HELD ZHAO_EXPECT_VIDEO_TERM_DROP_HELD
  "video_term_drop_held.*DETECTED")
zhao_packet_h_terminal_test(ph_term_c change_held_control
  ZHAO_VIDEO_TERM_MUTANT_CHANGE_HELD ZHAO_EXPECT_VIDEO_TERM_CHANGE_HELD
  "video_term_change_held.*DETECTED")
zhao_packet_h_terminal_test(ph_term_w wrong_writer_control
  ZHAO_VIDEO_TERM_MUTANT_WRONG_WRITER ZHAO_EXPECT_VIDEO_TERM_WRONG_WRITER
  "video_term_wrong_writer.*DETECTED")
zhao_packet_h_terminal_test(ph_term_p publish_wins_control
  ZHAO_VIDEO_TERM_MUTANT_PUBLISH_WINS ZHAO_EXPECT_VIDEO_TERM_PUBLISH_WINS
  "video_term_publish_wins.*DETECTED")
zhao_packet_h_terminal_test(ph_term_r render_ready_control
  ZHAO_VIDEO_TERM_MUTANT_RENDER_READY ZHAO_EXPECT_VIDEO_TERM_RENDER_READY
  "video_term_render_ready.*DETECTED")
zhao_packet_h_terminal_test(ph_term_s silent_blit_drop_control
  ZHAO_VIDEO_TERM_MUTANT_SILENT_BLIT_DROP
  ZHAO_EXPECT_VIDEO_TERM_SILENT_BLIT_DROP
  "video_term_silent_blit_drop.*DETECTED")
zhao_packet_h_terminal_test(ph_term_n no_pop_replace_control
  ZHAO_VIDEO_TERM_MUTANT_NO_POP_REPLACE ZHAO_EXPECT_VIDEO_TERM_NO_POP_REPLACE
  "video_term_no_pop_replace.*DETECTED")

foreach(packet_h_collision IN ITEMS
    renderer-sv renderer-cpp bridge-sv bridge-cpp terminal-sv terminal-cpp)
  string(REPLACE "-" ";" collision_parts "${packet_h_collision}")
  list(GET collision_parts 0 collision_family)
  list(GET collision_parts 1 collision_language)
  if(collision_family STREQUAL "bridge")
    set(collision_test "video_ready_bridge_v2_${collision_language}_selector_collision")
  elseif(collision_family STREQUAL "terminal")
    set(collision_test "video_terminal_adapter_v2_${collision_language}_selector_collision")
  else()
    set(collision_test "renderer_lease_v2_${collision_language}_selector_collision")
  endif()
  add_test(NAME ${collision_test}
    COMMAND "${Python3_EXECUTABLE}"
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_h_shell_prereqs.py
      --collision ${packet_h_collision}
      --verilator "${ZHAO_VERILATOR_LINT}"
      --cxx "${CMAKE_CXX_COMPILER}")
  set_tests_properties(${collision_test} PROPERTIES
    LABELS "fast;mutant;packet-h" TIMEOUT 180
    ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
endforeach()

add_test(NAME lint_zhao_renderer_lease_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall --assert
    --top-module tb_renderer_lease_v2 ${ZHAO_PACKET_H_RENDERER_SOURCES})
set_tests_properties(lint_zhao_renderer_lease_v2 PROPERTIES
  LABELS "lint;fast;packet-h" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_zhao_video_ready_bridge_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall --assert
    --top-module tb_video_ready_bridge_v2 ${ZHAO_PACKET_H_BRIDGE_SOURCES})
set_tests_properties(lint_zhao_video_ready_bridge_v2 PROPERTIES
  LABELS "lint;fast;packet-h" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
add_test(NAME lint_zhao_video_terminal_adapter_v2
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall --assert
    --top-module tb_video_terminal_adapter_v2 ${ZHAO_PACKET_H_TERMINAL_SOURCES})
set_tests_properties(lint_zhao_video_terminal_adapter_v2 PROPERTIES
  LABELS "lint;fast;packet-h" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME packet_h_shell_prereqs_registration_static
  COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_packet_h_shell_prereqs.py -q)
set_tests_properties(packet_h_shell_prereqs_registration_static PROPERTIES
  LABELS "fast;packet-h" TIMEOUT 180)

set(ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TESTS
  renderer_lease_v2_directed
  renderer_lease_v2_lease_open_control
  renderer_lease_v2_slot_choice_control
  renderer_lease_v2_request_hold_control
  renderer_lease_v2_writer_response_control
  renderer_lease_v2_skip_clear_control
  renderer_lease_v2_bad_stride_control
  renderer_lease_v2_duo_boundary_control
  renderer_lease_v2_stale_identity_control
  video_ready_bridge_v2_directed
  video_ready_bridge_v2_drop_held_tuple_control
  video_ready_bridge_v2_change_held_tuple_control
  video_ready_bridge_v2_wrong_onehot_control
  video_ready_bridge_v2_blank_ack_bypass_control
  video_ready_bridge_v2_early_lease_open_control
  video_ready_bridge_v2_retain_reset_tuple_control
  video_ready_bridge_v2_unblank_scanout_only_control
  video_ready_bridge_v2_unblank_negative_control
  video_terminal_adapter_v2_directed
  video_terminal_adapter_v2_drop_held_control
  video_terminal_adapter_v2_change_held_control
  video_terminal_adapter_v2_wrong_writer_control
  video_terminal_adapter_v2_publish_wins_control
  video_terminal_adapter_v2_render_ready_control
  video_terminal_adapter_v2_silent_blit_drop_control
  video_terminal_adapter_v2_no_pop_replace_control
  video_terminal_adapter_v2_sv_selector_collision
  video_terminal_adapter_v2_cpp_selector_collision
  lint_zhao_video_terminal_adapter_v2
  renderer_lease_v2_sv_selector_collision
  renderer_lease_v2_cpp_selector_collision
  video_ready_bridge_v2_sv_selector_collision
  video_ready_bridge_v2_cpp_selector_collision
  lint_zhao_renderer_lease_v2
  lint_zhao_video_ready_bridge_v2
  packet_h_shell_prereqs_registration_static)
list(LENGTH ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TESTS
  ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TEST_COUNT)
if(NOT ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TEST_COUNT EQUAL 36)
  message(FATAL_ERROR
    "Packet-H shell prerequisite CTest inventory must contain exactly 36 names")
endif()
foreach(required_packet_h_test IN LISTS ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TESTS)
  if(NOT TEST "${required_packet_h_test}")
    message(FATAL_ERROR
      "required Packet-H shell prerequisite CTest is not registered: ${required_packet_h_test}")
  endif()
endforeach()

# ---------------------------------------------------------------------------
# PART.SPAWN -- deterministic child spawn.
#
# SPECIES_N is cut to 8 so an OUT-OF-RANGE CHILD SPECIES is reachable with legal
# stimulus. At the production 128 no 7-bit species is out of range, so the
# refusal path would be unreachable and its counter would be a claim rather than
# a measurement. MAX_CHILD stays at the ruled 16 -- the 17-child refusal must be
# tested against the REAL bound, not a lowered one.
add_executable(test_part_spawn_directed particles/part_spawn_directed.cpp)
# zhao_zref for zref/zref_particle.hpp: records are built and read with the
# RATIFIED particle128 codec, never hand-written shifts.
target_link_libraries(test_part_spawn_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_spawn_directed
  PREFIX Vzhao_part_spawn
  TOP_MODULE zhao_part_spawn
  VERILATOR_ARGS -GSPECIES_N=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_spawn.sv)
add_test(NAME part_spawn_directed COMMAND test_part_spawn_directed)
set_tests_properties(part_spawn_directed PROPERTIES LABELS "fast;particles")

# PART.COLLIDE -- one collision response per particle per tick, against the LIVE
# deformed terrain and one plane. There is NO oracle: the ledger declares
# zref::ParticleCollide and that symbol is phantom #10, so this bench checks the
# contract's PROPERTIES (anti-jitter re-test, STICK idempotence,
# BOUNCE(e=0)==SLIDE(f=0), refusal byte-identity) plus hand-computed outcomes.
# zhao_part_record is in SOURCES because the collider INSTANTIATES the ratified
# codec rather than restating the frozen bit offsets.
add_executable(test_part_collide_directed particles/part_collide_directed.cpp)
# zhao_zref for zref/zref_particle.hpp: records are built and read with the
# RATIFIED particle128 codec, never hand-written shifts.
target_link_libraries(test_part_collide_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_collide_directed
  PREFIX Vzhao_part_collide
  TOP_MODULE zhao_part_collide
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_collide.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
add_test(NAME part_collide_directed COMMAND test_part_collide_directed)
set_tests_properties(part_collide_directed PROPERTIES LABELS "fast;particles")
# ==============================================================================
# GEOM client-A producer -- zhao_geom_group_seq
# ==============================================================================
# The absent organ the shared projector's adoption actually waits on.
# design/prod_manifest.yml says of zhao_geom_proj_lane: "Selecting one shared
# service instead of two wrappers (6,598 ALM / 33 DSP against ~12,400 / 66)
# needs this composed AND a producer driving it; the producer is still absent".
# This is that producer. It is NOT GEOM.WARP, which is deferred by owner ruling
# 2026-08-31 6.3, excluded from v1, and which GEOM.PROJECT.md line 67 places in
# the ALTERNATIVE position to GEOM.WCACHE rather than on the fill path.
#
# ARENAS=4 so a dual-view job can be starved deliberately (case 5); DEPTH=8 for
# the same reason geom_proj_lane_directed uses 8 -- the arena's walk-clear cost
# is not what this suite tests.
add_executable(test_geom_group_seq_directed geometry/geom_group_seq_directed.cpp)
target_link_libraries(test_geom_group_seq_directed PRIVATE zhao_harness)
verilate(test_geom_group_seq_directed
  PREFIX Vzhao_geom_group_seq
  TOP_MODULE zhao_geom_group_seq
  VERILATOR_ARGS -GARENAS=4 -GDEPTH=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_group_seq.sv)
add_test(NAME geom_group_seq_directed COMMAND test_geom_group_seq_directed)
set_tests_properties(geom_group_seq_directed PROPERTIES LABELS "fast;geometry")

add_test(NAME lint_geom_group_seq
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_group_seq
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_group_seq.sv)
set_tests_properties(lint_geom_group_seq PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE DETECTOR'S POSITIVE CONTROL, at inverted polarity.
# seal_early_o watches for a seal issued before the landings arrive. That state
# is unreachable while the drain rule is correct, so no legal stimulus moves it
# and its silence on production proves nothing on its own. This build swaps in
# tests/mutants/zhao_geom_group_seq_mutant.sv -- one substantive line, StSealWait
# advancing without the drain condition -- and PASSES when the counter FIRES.
add_executable(test_geom_group_seq_seal_early_mutant geometry/geom_group_seq_directed.cpp)
target_link_libraries(test_geom_group_seq_seal_early_mutant PRIVATE zhao_harness)
target_compile_definitions(test_geom_group_seq_seal_early_mutant PRIVATE ZHAO_EXPECT_SEAL_EARLY_MUTANT=1)
verilate(test_geom_group_seq_seal_early_mutant
  PREFIX Vzhao_geom_group_seq
  TOP_MODULE zhao_geom_group_seq
  VERILATOR_ARGS -GARENAS=4 -GDEPTH=8
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_group_seq_mutant.sv)
add_test(NAME geom_group_seq_seal_early_mutant COMMAND test_geom_group_seq_seal_early_mutant)
set_tests_properties(geom_group_seq_seal_early_mutant PROPERTIES LABELS "fast;mutant")

# ---------------------------------------------------------------------------
# GEOM.LIGHT (phase 5, ZH-085) -- the vertex-RGB COMPOSITION.
#
# THE SOURCE LIST IS THREE FILES AND THAT IS THE POINT. This block contains no
# light-term arithmetic: it instantiates zhao_terrain_shade, which since
# TERRAIN.SHADE.md A7 is the SHARED lighting core, and that engine in turn
# instantiates the zhao_qsq_bytemul primitive. A list naming only the shell
# gets MODMISSING; a list naming only the shell and the engine gets it one
# level deeper. That is the founding defect of source_list_parity, hit twice
# already in this tree.
set(ZHAO_GEOM_LIGHT_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_qsq_bytemul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_shade.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_light.sv)

add_executable(test_geom_light_directed geometry/geom_light_directed.cpp)
target_link_libraries(test_geom_light_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_light_directed
  PREFIX Vzhao_geom_light
  TOP_MODULE zhao_geom_light
  SOURCES ${ZHAO_GEOM_LIGHT_SV}
  VERILATOR_ARGS --assert)
add_test(NAME geom_light_directed COMMAND test_geom_light_directed)
set_tests_properties(geom_light_directed PROPERTIES LABELS "fast;nightly")

# The positive control. --break-oracle corrupts ONE expected value in the
# DIFFERENTIAL tier by +1; a checker never seen to fail is not a checker.
add_test(NAME geom_light_break_oracle
         COMMAND test_geom_light_directed --break-oracle)
set_tests_properties(geom_light_break_oracle PROPERTIES
  LABELS "fast;nightly" WILL_FAIL TRUE)

add_test(NAME lint_geom_light
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_light
                 ${ZHAO_GEOM_LIGHT_SV})
set_tests_properties(lint_geom_light PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# ===========================================================================
# POST.COMPOSITE -- the compositor, one bounded line stream (ruling R6).
#
# THIS BENCH IS NOT A DIFFERENTIAL, AND THE REGISTRATION SAYS SO BECAUSE THE
# LEDGER DOES NOT. design/blocks.yml declares reference_model
# zref::PostComposite; tools/budget/refmodel_liveness.py lists that symbol
# among eleven PHANTOM citations -- it has never been written. So
# post_composite_directed checks SELF-CONSISTENCY against contract prose plus
# hand-computed expectations, and its header says so in the first paragraph.
# A future reader must not quote it as "RTL matches the oracle".
#
# The weight-bearing half is structural and owes nothing to the model: every
# mandatory stage is shown to do positive work AND to change the picture when
# muted (completion plan 11.5); the bench is shown able to tell grade-after-
# bloom from grade-before-bloom BEFORE it asserts which one ships; world, glow
# and ink are shown to move by the SAME displacement, which is the case that
# would read as a printing error if it broke.
add_executable(test_post_composite_directed compositor/post_composite_directed.cpp)
target_link_libraries(test_post_composite_directed PRIVATE zhao_harness zhao_zref)
verilate(test_post_composite_directed
  PREFIX Vzhao_post_composite
  TOP_MODULE zhao_post_composite
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_composite.sv)
add_test(NAME post_composite_directed COMMAND test_post_composite_directed)
set_tests_properties(post_composite_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ...and the positive control for the one counter no legal stimulus can move.
#
# ring_hazard_o watches for a displaced sample reaching a ring column the write
# pointer has already passed. That state is unreachable while the output delay
# queue is correct, so "it can fire" would stay an argument forever.
#
# tests/mutants/zhao_post_composite_ring_hazard_mutant.sv is a WRAPPER, not a
# copy -- it instantiates the production module with LAG_PX = 0 and nothing
# else, so it cannot go stale the way the thirteen combiner copies did. If
# production gains or loses a port the wrapper's `.*` fails to elaborate, which
# is a LOUD failure rather than a control that quietly keeps passing.
#
# THIS TEST PASSES WHEN THE COUNTER FIRES. Inverse polarity, deliberately: it
# is evidence about the instrument, not about the design. It doubles as the
# only proof that the nine-pixel delay queue is load-bearing.
add_executable(test_post_composite_ring_hazard_mutant
  mutants/post_composite_ring_hazard_mutant.cpp)
target_link_libraries(test_post_composite_ring_hazard_mutant PRIVATE zhao_harness zhao_zref)
target_include_directories(test_post_composite_ring_hazard_mutant PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/compositor)
verilate(test_post_composite_ring_hazard_mutant
  PREFIX Vzhao_post_composite_ring_hazard_mutant
  TOP_MODULE zhao_post_composite_ring_hazard_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_post_composite_ring_hazard_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/compositor/zhao_post_composite.sv)
add_test(NAME post_composite_ring_hazard_mutant
         COMMAND test_post_composite_ring_hazard_mutant)
set_tests_properties(post_composite_ring_hazard_mutant PROPERTIES
  LABELS "fast;nightly" TIMEOUT 600)
# ---------------------------------------------------------------------------
# MEASURE.HISTOGRAM (phase 8, ZH-049) -- the RAM-backed error-bucket histogram.
#
# Built 2026-09-18 after the owner revoked every deferral
# (reports/MISSING-ORGAN-REGISTER-20260918.md). Its contract is a 186-line
# REFUSAL rather than a specification; the module header records which of the
# four inventions it names were made and which one was refused.
#
# NOT A DIFFERENTIAL LANE. `reference_model: zref::MeasureHistogram` resolves to
# nothing (tools/budget/refmodel_liveness.py names it among eleven unresolved
# models), so the directed lane is hand-computed and structural, and both the
# header and the test file say so rather than implying an oracle exists.
set(ZHAO_MEASURE_HISTOGRAM_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/measure/zhao_measure_histogram.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv)

add_executable(test_measure_histogram_directed measure/measure_histogram_directed.cpp)
target_link_libraries(test_measure_histogram_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_histogram_directed PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_histogram_directed
  PREFIX Vzhao_measure_histogram
  TOP_MODULE zhao_measure_histogram
  SOURCES ${ZHAO_MEASURE_HISTOGRAM_SV})
add_test(NAME measure_histogram_directed COMMAND test_measure_histogram_directed)
set_tests_properties(measure_histogram_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# Lane 13: saturation. THE SAME RTL FILE with -GCW=8, because reaching the
# shipped 24-bit ceiling needs 16.7 million events. CW is the only override.
add_executable(test_measure_histogram_sat measure/measure_histogram_sat.cpp)
target_link_libraries(test_measure_histogram_sat PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_histogram_sat PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_histogram_sat
  PREFIX Vzhao_measure_histogram
  TOP_MODULE zhao_measure_histogram
  SOURCES ${ZHAO_MEASURE_HISTOGRAM_SV}
  VERILATOR_ARGS -GCW=8)
add_test(NAME measure_histogram_sat COMMAND test_measure_histogram_sat)
set_tests_properties(measure_histogram_sat PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# The committed mutant for `frozen_write_o`, which no legal stimulus can move.
# INVERTED POLARITY: it passes when the counter FIRES on the mutant, and it
# carries its own negative control -- the shipped module, same stimulus, in the
# same executable, required to stay at zero.
add_executable(test_measure_histogram_drain_mutant measure/measure_histogram_drain_mutant.cpp)
target_link_libraries(test_measure_histogram_drain_mutant PRIVATE zhao_harness zhao_zref)
target_include_directories(test_measure_histogram_drain_mutant PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/measure)
verilate(test_measure_histogram_drain_mutant
  PREFIX Vzhao_measure_histogram_drain_mutant
  TOP_MODULE zhao_measure_histogram_drain_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_measure_histogram_drain_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_dc_sdp_ram.sv)
verilate(test_measure_histogram_drain_mutant
  PREFIX Vzhao_measure_histogram
  TOP_MODULE zhao_measure_histogram
  SOURCES ${ZHAO_MEASURE_HISTOGRAM_SV})
add_test(NAME measure_histogram_drain_mutant COMMAND test_measure_histogram_drain_mutant)
set_tests_properties(measure_histogram_drain_mutant PROPERTIES
  LABELS "fast;nightly" TIMEOUT 600)

# Verilator lint-clean is NOT Quartus-synthesizable. This gate settles one
# tool's opinion about syntax and width; the block has never been through
# quartus_map and no synthesizability claim is made for it.
add_test(NAME lint_measure_histogram
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_measure_histogram
                 ${ZHAO_MEASURE_HISTOGRAM_SV})
set_tests_properties(lint_measure_histogram PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# ===========================================================================
# GEOM.LIGHT STREAMED -- the 2026-09-18 lighting throughput rescue.
#
# The scalar owner (`zhao_geom_light`, above) is MEASURED at II = 167.0
# clocks per light term, which is 48.1x over the frame for the ruled
# 120,000-normal / 480,000-term stress profile. These blocks are the
# replacement owner, not a second law: the same rounded quotient, reached
# through two adapters, with the descriptor walk, the square root and the
# colour fold all given enough bandwidth to keep an II2 divider fed.
#
# MEASURED on the ruled fixture: 960,086 clocks, II = 2.0002 clk/term.
# The eight-light workload is a DIFFERENT workload and is OVER BUDGET at
# 1,920,086 clocks; `light_stream_directed` asserts that overrun rather than
# letting "supports eight lights" stand in for "meets rate at eight lights".
#
# NONE OF THESE HAVE BEEN THROUGH quartus_map. Lint-clean is one tool's
# opinion about syntax and is not synthesizability; area, Fmax, DSP mapping
# and RAM inference are all unmeasured. The multiplier count IS countable
# from the source and is five operators: two signed 32x32 and three
# unsigned 20x17. See reports/DSP-SAVINGS-REGISTER-20260918.md row D1.
# ===========================================================================
set(ZHAO_LIGHT_DIV_SV  ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_light_div32_ii2.sv)
set(ZHAO_LIGHT_ROOT_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_light_isqrt64_ii8.sv)
set(ZHAO_LIGHT_SKIN_SV ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_light_skin_adapter.sv)
set(ZHAO_LIGHT_STREAM_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_light_stream.sv
    ${ZHAO_LIGHT_DIV_SV}
    ${ZHAO_LIGHT_ROOT_SV})

# ---- the folded II2 quotient ---------------------------------------------
add_executable(test_light_div32_ii2_directed geometry/light_div32_ii2_directed.cpp)
target_link_libraries(test_light_div32_ii2_directed PRIVATE zhao_harness zhao_zref)
verilate(test_light_div32_ii2_directed
  PREFIX Vzhao_light_div32_ii2
  TOP_MODULE zhao_light_div32_ii2
  SOURCES ${ZHAO_LIGHT_DIV_SV})
add_test(NAME light_div32_ii2_directed COMMAND test_light_div32_ii2_directed)
set_tests_properties(light_div32_ii2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
# POSITIVE CONTROL: one expected quotient corrupted by +1; the suite must FAIL.
add_test(NAME light_div32_ii2_break_oracle
         COMMAND test_light_div32_ii2_directed --break-oracle)
set_tests_properties(light_div32_ii2_break_oracle PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME lint_light_div32_ii2
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_light_div32_ii2 ${ZHAO_LIGHT_DIV_SV})
set_tests_properties(lint_light_div32_ii2 PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- the folded II8 square root ------------------------------------------
add_executable(test_light_isqrt64_ii8_directed geometry/light_isqrt64_ii8_directed.cpp)
target_link_libraries(test_light_isqrt64_ii8_directed PRIVATE zhao_harness zhao_zref)
verilate(test_light_isqrt64_ii8_directed
  PREFIX Vzhao_light_isqrt64_ii8
  TOP_MODULE zhao_light_isqrt64_ii8
  SOURCES ${ZHAO_LIGHT_ROOT_SV})
add_test(NAME light_isqrt64_ii8_directed COMMAND test_light_isqrt64_ii8_directed)
set_tests_properties(light_isqrt64_ii8_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME light_isqrt64_ii8_break_oracle
         COMMAND test_light_isqrt64_ii8_directed --break-oracle)
set_tests_properties(light_isqrt64_ii8_break_oracle PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME lint_light_isqrt64_ii8
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_light_isqrt64_ii8 ${ZHAO_LIGHT_ROOT_SV})
set_tests_properties(lint_light_isqrt64_ii8 PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- the creature seam ----------------------------------------------------
# Differential against the COMPILED producer `zref::creature::skin_world_normal`,
# not against a model of it: if the range-reduction claim were wrong, the real
# producer is what would generate the counterexample.
add_executable(test_light_skin_adapter_directed geometry/light_skin_adapter_directed.cpp)
target_link_libraries(test_light_skin_adapter_directed PRIVATE zhao_harness zhao_zref)
verilate(test_light_skin_adapter_directed
  PREFIX Vzhao_light_skin_adapter
  TOP_MODULE zhao_light_skin_adapter
  SOURCES ${ZHAO_LIGHT_SKIN_SV}
  VERILATOR_ARGS --assert)
add_test(NAME light_skin_adapter_directed COMMAND test_light_skin_adapter_directed)
set_tests_properties(light_skin_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME light_skin_adapter_break_oracle
         COMMAND test_light_skin_adapter_directed --break-oracle)
set_tests_properties(light_skin_adapter_break_oracle PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 600)
add_test(NAME lint_light_skin_adapter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_light_skin_adapter ${ZHAO_LIGHT_SKIN_SV})
set_tests_properties(lint_light_skin_adapter PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- the streamed service, and the fixture that decides it ---------------
add_executable(test_light_stream_directed geometry/light_stream_directed.cpp)
target_link_libraries(test_light_stream_directed PRIVATE zhao_harness zhao_zref)
verilate(test_light_stream_directed
  PREFIX Vzhao_light_stream
  TOP_MODULE zhao_light_stream
  SOURCES ${ZHAO_LIGHT_STREAM_SV}
  VERILATOR_ARGS --assert)
add_test(NAME light_stream_directed COMMAND test_light_stream_directed)
# nightly only for the full fixture: it runs the complete 480,000-term and
# 960,000-term workloads, which is ~2.9M simulated clocks.
set_tests_properties(light_stream_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME light_stream_break_oracle COMMAND test_light_stream_directed --break-oracle)
set_tests_properties(light_stream_break_oracle PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME lint_light_stream
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_light_stream ${ZHAO_LIGHT_STREAM_SV})
set_tests_properties(lint_light_stream PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- the elaboration guard, FIRED --------------------------------------
# `--lint-only` does not run `initial` blocks, so a clean lint says nothing
# whatever about these. GAINW is pinned at 20 because six coefficients at 24
# bits is 144 and the two-half record does not exist; refusing to elaborate is
# the honest form of "a nondefault wider GAINW requires a newly priced record
# and port schedule, not truncation". This target builds the SAME bench with
# GAINW = 24 and passes when the simulation dies:
#   %Fatal: zhao_light_stream.sv:282: Assertion failed in TOP.zhao_light_stream:
#   zhao_light_stream: GAINW != 20 needs a repriced two-half record, not a truncation
add_executable(test_light_stream_gainw_guard geometry/light_stream_directed.cpp)
target_link_libraries(test_light_stream_gainw_guard PRIVATE zhao_harness zhao_zref)
verilate(test_light_stream_gainw_guard
  PREFIX Vzhao_light_stream
  TOP_MODULE zhao_light_stream
  SOURCES ${ZHAO_LIGHT_STREAM_SV}
  # -Wno-SELRANGE is REQUIRED, not cosmetic. GAINW=24 makes
  # `lB_cur_q[100 +: GAINW]` select 123:100 out of 120 bits, and verilate runs
  # at CMAKE CONFIGURE time -- so without this the SELRANGE warning is fatal
  # during configure and NO TARGET IN THE TREE can be generated. It broke every
  # build in the repo on 2026-09-18 until this line was added.
  #
  # Suppressing it is correct rather than convenient: the out-of-range select is
  # a CONSEQUENCE of the bad parameter this target deliberately injects, and the
  # thing under test is the elaboration $fatal at zhao_light_stream.sv:282 --
  # which --lint-only can never reach, because it does not run initial blocks.
  # Let the guard be what fires, not a warning about the bait.
  VERILATOR_ARGS --assert -GGAINW=24 -Wno-SELRANGE)
add_test(NAME light_stream_gainw_guard COMMAND test_light_stream_gainw_guard)
set_tests_properties(light_stream_gainw_guard PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 600)

# ---- the two guards no legal stimulus can reach -------------------------
# Both are asserted ZERO by light_stream_directed. A detector reading zero is
# a claim, and it is the claim to check hardest, so each is fired here on a
# committed mutant before its silence is quoted.
#
# THESE ARE WRAPPERS, NOT COPIES. They instantiate PRODUCTION with `.*`, so a
# port change fails to elaborate rather than quietly testing a module that no
# longer exists -- the exact failure that left thirteen combiner copies and
# eight AUX-pipe copies passing green against machines two weeks stale.
#
# BOTH TESTS PASS WHEN THEIR COUNTER FIRES. Inverse polarity, deliberately:
# this is evidence about the INSTRUMENT, not about the design.
set(ZHAO_LIGHT_MUTANT_SV
    ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_light_stream_guard_mutants.sv
    ${ZHAO_LIGHT_STREAM_SV})

add_executable(test_light_stream_rqfull_mutant mutants/light_stream_guard_mutant.cpp)
target_link_libraries(test_light_stream_rqfull_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_light_stream_rqfull_mutant PRIVATE ZHAO_MUT_COUNTER=0)
verilate(test_light_stream_rqfull_mutant
  PREFIX Vzhao_light_stream_rqfull_mutant
  TOP_MODULE zhao_light_stream_rqfull_mutant
  SOURCES ${ZHAO_LIGHT_MUTANT_SV})
add_test(NAME light_stream_rqfull_mutant COMMAND test_light_stream_rqfull_mutant)
set_tests_properties(light_stream_rqfull_mutant PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

add_executable(test_light_stream_skew_mutant mutants/light_stream_guard_mutant.cpp)
target_link_libraries(test_light_stream_skew_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_light_stream_skew_mutant PRIVATE ZHAO_MUT_COUNTER=1)
verilate(test_light_stream_skew_mutant
  PREFIX Vzhao_light_stream_skew_mutant
  TOP_MODULE zhao_light_stream_skew_mutant
  SOURCES ${ZHAO_LIGHT_MUTANT_SV})
add_test(NAME light_stream_skew_mutant COMMAND test_light_stream_skew_mutant)
set_tests_properties(light_stream_skew_mutant PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ===========================================================================
# SEVEN COMMITTED TESTS THAT NO TARGET COMPILED (registered 2026-09-19)
# ---------------------------------------------------------------------------
# reports/MODULE-DISPOSITION-REGISTER-20260918.md section 5.1 found EIGHT
# committed test .cpp files that this file never names. There are 452
# add_executable calls here and zero file(GLOB), so a test is compiled only if
# it is named -- and four ENFORCED-BY citations plus one quoted measurement
# (design/blocks.yml: "4,738 checks, positive controls fired") stood on gates
# that could not fire. A citation that resolves as a PATH while naming a
# binary nobody builds is worse than no citation: it reads as coverage.
#
# island_v3_fault_directed.cpp is the eighth and is already accounted for at
# line 2674 as retained source history. The other seven are registered below.
# ===========================================================================

# ---- the two proofs that take no DUT --------------------------------------
# Pure C++: each states a law twice, independently, and differences the two.
# No Verilated model, so neither is in verilated_exit_path.py's scope and a
# plain `return bad != 0` from main is correct here.
#
# curve_kstart_equivalence's own header calls itself "a standalone proof, not
# a CTest gate ... run it when the search changes". It is self-checking and
# already returns non-zero on disagreement, and nothing in the tree was
# keeping that promise. A proof nobody re-runs is a proof about an old file.
add_executable(test_attribute_plane_equivalence proofs/attribute_plane_equivalence.cpp)
add_test(NAME attribute_plane_equivalence COMMAND test_attribute_plane_equivalence)
set_tests_properties(attribute_plane_equivalence PROPERTIES LABELS "fast;nightly")

add_executable(test_curve_kstart_equivalence proofs/curve_kstart_equivalence.cpp)
add_test(NAME curve_kstart_equivalence COMMAND test_curve_kstart_equivalence)
set_tests_properties(curve_kstart_equivalence PROPERTIES LABELS "fast;nightly")

# ---- zhao_proj_arena3's directed differential ------------------------------
# fpga/rtl/common/zhao_proj_arena3.sv:96 declares
#   ENFORCED-BY: tests/geometry/proj_arena3_directed.cpp
# and the test's own header says "promoting it into tests/CMakeLists.txt is a
# ten-line block copied from geom_wcache_directed's". This is that block.
# The module defaults (GROUPS=4, DEPTH=81, STRIDE=128) are exactly what the
# suite's literal expectations are written against -- kGroups/kDepth in the
# .cpp -- so no -G overrides: an override here would silently retune the
# expectations the test asserts.
add_executable(test_proj_arena3_directed geometry/proj_arena3_directed.cpp)
target_link_libraries(test_proj_arena3_directed PRIVATE zhao_harness zhao_zref)
verilate(test_proj_arena3_directed
  PREFIX Vzhao_proj_arena3
  TOP_MODULE zhao_proj_arena3
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_proj_arena3.sv)
add_test(NAME proj_arena3_directed COMMAND test_proj_arena3_directed)
set_tests_properties(proj_arena3_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 600)

# ---- TERRAIN.NORMALMAP's acceptance suite ----------------------------------
# fpga/rtl/terrain/zhao_terrain_normalmap.sv lines 9 and 363 both declare
#   ENFORCED-BY: tests/texture/terrain_normalmap_directed.cpp
# and design/blocks.yml quotes its check count as evidence for the block.
# Until this block existed, none of that ran.
add_executable(test_terrain_normalmap_directed texture/terrain_normalmap_directed.cpp)
target_link_libraries(test_terrain_normalmap_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_normalmap_directed
  PREFIX Vzhao_terrain_normalmap
  TOP_MODULE zhao_terrain_normalmap
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normalmap.sv)
add_test(NAME terrain_normalmap_directed COMMAND test_terrain_normalmap_directed)
set_tests_properties(terrain_normalmap_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# The suite's own positive control, registered rather than described: the
# --break-oracle run corrupts ONE expected delta and the suite MUST then fail.
# Inverted polarity on purpose -- this is evidence about the INSTRUMENT. The
# report quoting "positive controls fired" was quoting a control nothing ran.
add_test(NAME terrain_normalmap_break_oracle
         COMMAND test_terrain_normalmap_directed --break-oracle)
set_tests_properties(terrain_normalmap_break_oracle PROPERTIES
  WILL_FAIL TRUE LABELS "fast;nightly" TIMEOUT 600)
# ---- the ATTRSTEP/ATTRWALK proof pair --------------------------------------
# fpga/rtl/raster/zhao_raster_attrwalk.sv:4 declares
#   ENFORCED-BY: tests/proofs/attrwalk_rtl_differential.cpp:main
# These two are the only files in tests/ that link the ACTUAL reference
# divide, zref::render::div_rhu_s128, rather than restating it -- which is the
# entire point of the pair (see attrstep_qr_differential.cpp's header: four
# prior documents each re-derived the law and one of them re-derived it
# WRONG). internal.hpp is a private header of reference/src/zrender, so that
# directory goes on the include path; the symbol itself comes from rast.cpp,
# which is already a member of zhao_zref.
add_executable(test_attrstep_qr_differential proofs/attrstep_qr_differential.cpp)
target_link_libraries(test_attrstep_qr_differential PRIVATE zhao_harness zhao_zref)
target_include_directories(test_attrstep_qr_differential PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src/zrender)
verilate(test_attrstep_qr_differential
  PREFIX Vzhao_raster_attrdiv
  TOP_MODULE zhao_raster_attrdiv
  VERILATOR_ARGS -GRADIX=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv.sv)
add_test(NAME attrstep_qr_differential COMMAND test_attrstep_qr_differential)
set_tests_properties(attrstep_qr_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THREE Verilated models in one executable, which no other target here needs:
# the walker at each of the two tie laws, plus the shipped divider. That is
# the whole architecture of the proof -- one parameter must reach the RTL
# boundary's law (ties away from zero) and the other must reach the reference's
# (ties toward +infinity), and both are compared against the ACTUAL machine
# rather than against a restatement. Vattrwalk_away / Vattrwalk_pos are NOT
# separate sources: they are zhao_raster_attrwalk verilated twice under
# different prefixes, so there is no copy here to go stale.
add_executable(test_attrwalk_rtl_differential proofs/attrwalk_rtl_differential.cpp)
target_link_libraries(test_attrwalk_rtl_differential PRIVATE zhao_harness zhao_zref)
target_include_directories(test_attrwalk_rtl_differential PRIVATE
  ${CMAKE_SOURCE_DIR}/reference/src/zrender)
verilate(test_attrwalk_rtl_differential
  PREFIX Vattrwalk_away
  TOP_MODULE zhao_raster_attrwalk
  VERILATOR_ARGS -GTIE_TOWARD_POSITIVE=0
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrwalk.sv)
verilate(test_attrwalk_rtl_differential
  PREFIX Vattrwalk_pos
  TOP_MODULE zhao_raster_attrwalk
  VERILATOR_ARGS -GTIE_TOWARD_POSITIVE=1
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrwalk.sv)
verilate(test_attrwalk_rtl_differential
  PREFIX Vzhao_raster_attrdiv
  TOP_MODULE zhao_raster_attrdiv
  VERILATOR_ARGS -GRADIX=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_attrdiv.sv)
add_test(NAME attrwalk_rtl_differential COMMAND test_attrwalk_rtl_differential)
set_tests_properties(attrwalk_rtl_differential PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# ---- DSP.DUAL18: a script-driven suite made visible to ctest ---------------
# tests/dsp/dual18_physical_pack_directed.cpp is compiled by
# tests/dsp/run_dual18_verilator.py, which builds EIGHT models in a temp dir
# (deliberately: it must not touch this build tree, and it sweeps parameter
# signedness combinations a single add_executable cannot express). It was
# therefore half-live -- real, self-checking, and invisible to a suite run.
#
# It stayed broken for eight days because of that invisibility: 24bc6b47 put
# the harness include inside ONE backend arm, so seven of the eight cases did
# not compile at all. Registering the runner is what makes that a red line
# instead of a silence.
#
# Its four positive controls (backend_missing, backend_conflict, ce_hold,
# lane_swap) are internal to the script and each prints FIRED.
#
# The full corpus, NOT --quick, and it carries `fast`: measured 2026-09-19 at
# 59 s full against 57 s quick. Almost all of the wall time is the eight
# Verilator builds, so bounding the arithmetic saves two seconds and gives up
# the exhaustive 8-bit classes. A second cheaper entry would have doubled the
# cost of the every-commit gate to buy nothing.
add_test(NAME dual18_physical_pack_directed
         COMMAND "${Python3_EXECUTABLE}"
                 ${CMAKE_CURRENT_SOURCE_DIR}/dsp/run_dual18_verilator.py)
set_tests_properties(dual18_physical_pack_directed PROPERTIES
  LABELS "fast;nightly" TIMEOUT 1800)

# ---------------------------------------------------------------------------
# PART.TABLE -- the species descriptor table and the size/colour curve table.
#
# zhao_console_core.sv's gaps I2 and I3: three built blocks read a table that
# nothing implemented. `fpga/rtl/particles/zhao_part_table.sv` is that owner.
#
# SPECIES_N=8 and CRV_N=8 on purpose, for the reason PART.STATE's bench uses
# CAPACITY=8: at the production tier the index fields address the table EXACTLY
# (7 bits -> 128 species, 4 bits -> 16 buckets), so an out-of-range index is
# UNREACHABLE and the refusal that handles it is not evidence about anything.
# Shrinking the table is the only way to reach `load_refused_o` with legal
# stimulus -- and it also makes the two DISCRIMINATING cases reachable, because
# index 11 then masks onto species 3, which the bench keeps loaded throughout.
#
# Note what this suite is NOT: differential. No species descriptor layout exists
# anywhere in reference/ or spec/ -- zref_particle.hpp says so in its own words
# -- so the load word's offsets are AUTHORED in the RTL and have no oracle. The
# packing walk (one field all-ones, every other field required to read zero) is
# the structural check that stands in for one.
add_executable(test_part_table_directed particles/part_table_directed.cpp)
target_link_libraries(test_part_table_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_table_directed
  PREFIX Vzhao_part_table
  TOP_MODULE zhao_part_table
  VERILATOR_ARGS -GSPECIES_N=8 -GCRV_N=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_table.sv)
add_test(NAME part_table_directed COMMAND test_part_table_directed)
set_tests_properties(part_table_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_table
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_table
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_table.sv)
set_tests_properties(lint_zhao_part_table PROPERTIES LABELS "lint;fast")

# ---- PART.POP (owner ruling R41, core entry I7) -----------------------------
# The population descriptor bank, differenced field for field against
# zref::population after every record. CAPACITY = 8 for the same reason
# PART.STATE's bench runs at 8: at the production tier an `active_count`
# refusal needs a four-billion-record population to reach.
add_executable(test_part_pop_directed particles/part_pop_directed.cpp)
target_link_libraries(test_part_pop_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_pop_directed
  PREFIX Vzhao_part_pop
  TOP_MODULE zhao_part_pop
  VERILATOR_ARGS -GCAPACITY=8 -GCNT_W=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_pop.sv)
add_test(NAME part_pop_directed COMMAND test_part_pop_directed)
set_tests_properties(part_pop_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_pop
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_pop
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_pop.sv)
set_tests_properties(lint_zhao_part_pop PROPERTIES LABELS "lint;fast")

# ---- PART.TABLE's species-page loader (owner ruling R42, core entry I33) ----
# Differenced against zref::species_page after every entry: the page under test
# is built by the REFERENCE, so a layout disagreement is visible. The exact
# READ COUNT is asserted, because a loader that read a line twice would load
# the right descriptors twice and every content check would still pass.
add_executable(test_part_table_loader_directed particles/part_table_loader_directed.cpp)
target_link_libraries(test_part_table_loader_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_table_loader_directed
  PREFIX Vtb_part_table_loader
  TOP_MODULE tb_part_table_loader
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_table_loader.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/particles/tb_part_table_loader.sv)
add_test(NAME part_table_loader_directed COMMAND test_part_table_loader_directed)
set_tests_properties(part_table_loader_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_table_loader
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_table_loader
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_table_loader.sv)
set_tests_properties(lint_zhao_part_table_loader PROPERTIES LABELS "lint;fast")

# ---- TERRAIN.NORMALMAP's detail-page loader (owner ruling R243 D-NORMALS-A) -
# The same discipline as the species-page loader above, plus the one thing that
# bench does NOT do: its guard responder is the REAL two-cycle one. MEM.GUARD
# drives `ready` as a level and pulses `ok` the cycle AFTER the accept, and
# `tools/rtl/check_guard_verdict.py`'s header records that every played
# responder in the tree raised them together -- "the RTL, the harnesses and the
# measurements all agreed with each other about a machine that does not exist".
# This responder models zhao_mem_guard.sv:604-680, and it ASSERTS on every
# cycle of every test that the two are never high together, so a drift back
# toward the fiction fails loudly instead of quietly re-blessing the defect.
add_executable(test_terrain_normalloader_directed terrain/normalloader_directed.cpp)
target_link_libraries(test_terrain_normalloader_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_normalloader_directed
  PREFIX Vtb_terrain_normalloader
  TOP_MODULE tb_terrain_normalloader
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normalloader.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_normalloader.sv)
add_test(NAME terrain_normalloader_directed COMMAND test_terrain_normalloader_directed)
set_tests_properties(terrain_normalloader_directed PROPERTIES LABELS "fast;terrain" TIMEOUT 300)

add_test(NAME lint_zhao_terrain_normalloader
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_normalloader
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_normalloader.sv)
set_tests_properties(lint_zhao_terrain_normalloader PROPERTIES LABELS "lint;fast")

# The POSITIVE CONTROL for the responder above, kept SEPARATE from the directed
# test exactly as CLAUDE.md requires: "Do not write a test that asserts the bug
# ... keep the detector's positive control separate."
#
# `tests/mutants/zhao_terrain_normalloader_onecycle_mutant.sv` is the loader
# with its two verdict states collapsed back into the broken one-cycle form.
# No legal stimulus can produce that defect -- it is a property of the RTL's
# SHAPE, not a reachable state -- so the only demonstration is a committed
# mutant, and the driver's polarity is INVERTED: it passes when the load FAILS.
#
# MEASURED: pages=0 words=0 writes=0 busy=1 denied=0. The driver asserts the
# SILENT SIGNATURE, not merely a failure -- a block with this defect reports
# nothing at all and looks idle, which is why it survived in two geometry
# fetchers until a tool went looking for the shape.
add_executable(test_normalloader_onecycle_mutant
               mutants/normalloader_onecycle_mutant_driver.cpp)
target_link_libraries(test_normalloader_onecycle_mutant PRIVATE zhao_harness zhao_zref)
verilate(test_normalloader_onecycle_mutant
  PREFIX Vtb_terrain_normalloader_onecycle_mutant
  TOP_MODULE tb_terrain_normalloader_onecycle_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/mutants/zhao_terrain_normalloader_onecycle_mutant.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/mutants/tb_terrain_normalloader_onecycle_mutant.sv)
add_test(NAME normalloader_onecycle_mutant COMMAND test_normalloader_onecycle_mutant)
set_tests_properties(normalloader_onecycle_mutant
                     PROPERTIES LABELS "fast;terrain;mutant" TIMEOUT 300)

# ---- GEOM.LOD's ladder-constant bank (owner rulings R26/R68) ---------------
# The same discipline as the species-page loader above, plus one case that only
# a DOUBLE-BANKED store can pass: an illegal record FIVE rows into a seven-row
# page, with the previous page's rows required to still answer bit for bit
# afterwards. "Fewer rows" is not the test; "the same rows" is.
add_executable(test_geom_ladderbank_directed geometry/geom_ladderbank_directed.cpp)
target_link_libraries(test_geom_ladderbank_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_ladderbank_directed PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
verilate(test_geom_ladderbank_directed
  PREFIX Vtb_geom_ladderbank
  TOP_MODULE tb_geom_ladderbank
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_ladderbank.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_ladderbank.sv)
add_test(NAME geom_ladderbank_directed COMMAND test_geom_ladderbank_directed)
set_tests_properties(geom_ladderbank_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

# GEOM.POSE's kind-8 BODY and kind-9 CLIP FRAME page reader -- entry I29's
# consumer (POSEREAD). The bench is both ends of the block: MEM.UPLOAD's
# publication (spec/memory_rules.md 5f.1's row) and the asset window it reads
# through, with every page built by the REFERENCE so a layout disagreement can
# show up at all. It also asserts the LATE-FETCH LAW the committed
# zhao_geom_bonesrc_latefetch_mutant states -- no read after src_req_o.
add_executable(test_geom_clipread_directed geometry/geom_clipread_directed.cpp)
target_link_libraries(test_geom_clipread_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_clipread_directed PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
verilate(test_geom_clipread_directed
  PREFIX Vtb_geom_clipread
  TOP_MODULE tb_geom_clipread
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipread.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_clipread.sv)
add_test(NAME geom_clipread_directed COMMAND test_geom_clipread_directed)
set_tests_properties(geom_clipread_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

add_test(NAME lint_zhao_geom_clipread
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_clipread
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipread.sv)
set_tests_properties(lint_zhao_geom_clipread PROPERTIES LABELS "lint;fast")

# THE ELABORATION GUARD, PROVEN TO FIRE. `--lint-only` does not run `initial`
# blocks (CLAUDE.md, 2026-09-09), so the clean lint above says nothing about
# the CLIP_ROWS guard. This builds the SAME bench with -GCLIP_ROWS=65 and does
# nothing but eval the model; WILL_FAIL inverts the polarity, so it is GREEN
# when the design refuses to elaborate and RED when the guard stops firing.
#
# 65 AND NOT 0, WHICH IS A FACT ABOUT THE TOOL RATHER THAN THE DESIGN.
# `-GCLIP_ROWS=0` makes Verilator reject `logic [31:0] row_foff_q [0]` -- a
# zero-sized range -- and fail the BUILD, which is not a red test but a broken
# tree. 65 is a perfectly legal elaboration that only the $fatal objects to, so
# the refusal under test is the design's and nothing else's.
add_executable(test_geom_clipread_elab_guard geometry/geom_clipread_elab_guard.cpp)
target_link_libraries(test_geom_clipread_elab_guard PRIVATE zhao_harness zhao_zref)
# MEASURED, not anticipated: the first run of this test printed the %Fatal and
# then HUNG for the full 120 s timeout, and a ctest TIMEOUT is a failure whatever
# WILL_FAIL says -- so the guard fired and the test went red anyway. $fatal never
# reaches zhao::exit_hard, and zhao_sim.hpp documents at length why every
# Verilated exe on this toolchain must. These two defines let the test supply its
# own vl_stop/vl_fatal with that same tail; they go on the TARGET because
# Verilator's #ifndef lives in verilated.cpp, which the .cpp's own define could
# never reach.
target_compile_definitions(test_geom_clipread_elab_guard PRIVATE VL_USER_STOP VL_USER_FATAL)
verilate(test_geom_clipread_elab_guard
  PREFIX Vtb_geom_clipread_elab
  TOP_MODULE tb_geom_clipread
  VERILATOR_ARGS -GCLIP_ROWS=65
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipread.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_clipread.sv)
add_test(NAME geom_clipread_elab_guard COMMAND test_geom_clipread_elab_guard)
set_tests_properties(geom_clipread_elab_guard PROPERTIES
  LABELS "fast;geometry" TIMEOUT 120 WILL_FAIL TRUE)

# THE OWNERSHIP COMPARISON'S POSITIVE CONTROL -- owner ruling of 2026-09-21
# (kind-8 / kind-9 ownership), section 6 test J: "At least one deliberate
# mutation bypassing each new ownership comparison must turn the corresponding
# negative test red, while valid traffic stays green."
#
# NOT a WILL_FAIL test. The POLARITY IS IN THE DRIVER: it passes when the
# foreign frame IS served and `owner_mismatch_o` STAYS ZERO, which is the
# defect. It proves the PLANT first -- it reads the mutant source and requires
# the mutated line present AND the production expression absent -- because a
# fire test compiled against an unmutated copy reports success while measuring
# nothing.
add_executable(test_geom_clipread_ownerblind_mutant
  mutants/geom_clipread_ownerblind_mutant.cpp)
target_link_libraries(test_geom_clipread_ownerblind_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_clipread_ownerblind_mutant
  PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
verilate(test_geom_clipread_ownerblind_mutant
  PREFIX Vtb_geom_clipread_ownerblind
  TOP_MODULE tb_geom_clipread_ownerblind
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/mutants/zhao_geom_clipread_ownerblind_mutant.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/mutants/tb_geom_clipread_ownerblind.sv)
add_test(NAME geom_clipread_ownerblind_mutant
         COMMAND test_geom_clipread_ownerblind_mutant)
set_tests_properties(geom_clipread_ownerblind_mutant PROPERTIES
  LABELS "fast;geometry;mutant" TIMEOUT 300)

# GEOM.POSE's bone source -- entry I29's producer (POSEABI, owner ruling R90).
# The bench holds BOTH zhao_geom_bonesrc and the REAL zhao_geom_pose_decode,
# because the claim under test is about the JOIN between them: that the
# decoder's combinational source contract is met by a prefetch in front of
# SYNCHRONOUS memories. A stubbed decoder cannot see a prefetch a cycle late.
add_executable(test_geom_bonesrc_directed geometry/geom_bonesrc_directed.cpp)
target_link_libraries(test_geom_bonesrc_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_bonesrc_directed PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
verilate(test_geom_bonesrc_directed
  PREFIX Vtb_geom_bonesrc
  TOP_MODULE tb_geom_bonesrc
  THREADS 1
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_bonesrc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_decode.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_bonesrc.sv)
add_test(NAME geom_bonesrc_directed COMMAND test_geom_bonesrc_directed)
set_tests_properties(geom_bonesrc_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 600)

# THE INVERTED-POLARITY MUTANT DRIVER. It PASSES WHEN bone_prefetch_late_o
# FIRES, and it is evidence about the instrument rather than about the design.
# The counter is unreachable by legal stimulus (the decoder spends ~115 cycles
# per bone and the refill takes seven), which CLAUDE.md names as exactly the
# case needing a committed mutant. Its NEGATIVE control is separate, in
# geom_bonesrc_directed cases 3 and 4, where the same counter must read ZERO.
add_executable(test_geom_bonesrc_latefetch_mutant geometry/geom_bonesrc_latefetch_mutant.cpp)
target_link_libraries(test_geom_bonesrc_latefetch_mutant PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_geom_bonesrc_latefetch_mutant PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
verilate(test_geom_bonesrc_latefetch_mutant
  PREFIX Vtb_geom_bonesrc_mutant
  TOP_MODULE tb_geom_bonesrc_mutant
  THREADS 1
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_bonesrc_latefetch_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_decode.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_quat2mat.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_bonesrc_mutant.sv)
add_test(NAME geom_bonesrc_latefetch_mutant COMMAND test_geom_bonesrc_latefetch_mutant)
set_tests_properties(geom_bonesrc_latefetch_mutant PROPERTIES LABELS "fast;geometry;mutant" TIMEOUT 900)

# The CLIP_BANK page (cartridge kind 9) -- owner ruling R90's recommendation
# item 1, whose kind-9 half POSEABI did not reach. Model-only, deliberately:
# the layout is stated twice (zref_clip_page.hpp and tools/pack/mkclipbank.py)
# and this is what pins both to the committed golden, exactly as
# geom_ladderbank_directed pins the kind-8 ladder table. An RTL reader becomes
# the third statement when it is built and joins this same artefact.
add_executable(test_clip_page_directed geometry/clip_page_directed.cpp)
target_link_libraries(test_clip_page_directed PRIVATE zhao_harness zhao_zref)
target_compile_definitions(test_clip_page_directed PRIVATE ZHAO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME clip_page_directed COMMAND test_clip_page_directed)
set_tests_properties(clip_page_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

# The packer's own `--check`: it rebuilds the golden from its constants and
# fires every refusal. The C++ test above proves the MODEL matches the golden;
# this proves the PACKER does. Two statements, one artefact, two gates.
add_test(NAME mkclipbank_check
         COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/pack/mkclipbank.py" --check)
set_tests_properties(mkclipbank_check PROPERTIES LABELS "fast;tools" TIMEOUT 120)

add_test(NAME lint_zhao_geom_ladderbank
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_ladderbank
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_ladderbank.sv)
set_tests_properties(lint_zhao_geom_ladderbank PROPERTIES LABELS "lint;fast")

# ---- THE WHOLE R68 CHAIN (owner ruling R68 sub-build 3) --------------------
# zhao_view_projscale + zhao_geom_ladderbank + zhao_geom_lodstate (which holds
# zhao_geom_projradius and zhao_geom_lod), every block REAL, against
# zref::creature::lod_update run as a LIVE MODEL alongside -- one LodState per
# instance on the C++ side, stepped with the same projected radius, so the
# hysteresis and the minimum hold are compared across sixty frames rather than
# at one point. A block that lost its state, cleared it on the frame pulse or
# held the wrong instance's only shows up over time.
add_executable(test_geom_lodstate_directed geometry/geom_lodstate_directed.cpp)
target_link_libraries(test_geom_lodstate_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_lodstate_directed
  PREFIX Vtb_geom_lodstate
  TOP_MODULE tb_geom_lodstate
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_projscale.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_ladderbank.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_projradius.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_lod.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_lodstate.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_lodstate.sv)
add_test(NAME geom_lodstate_directed COMMAND test_geom_lodstate_directed)
set_tests_properties(geom_lodstate_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 900)

add_test(NAME lint_zhao_geom_lodstate
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_lodstate
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_lodstate.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_projradius.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_lod.sv)
set_tests_properties(lint_zhao_geom_lodstate PROPERTIES LABELS "lint;fast")

# ---- GEOM.LOD's projected bound radius (owner ruling R68 sub-build 4) ------
# The REAL zhao_view_projscale feeding the REAL zhao_geom_projradius, so the
# driver writes a view MATRIX onto the projector configuration bus and the
# projected radius comes out the far end. Every expected value is
# zref::creature::projected_bound_radius_q8's own -- the oracle is CALLED, not
# transcribed. Four cameras put the largest row-0 magnitude in a different
# element each time, including one with m[0][0] == 0, which is the case an
# implementation reading m[0][0] alone gets silently and catastrophically wrong.
add_executable(test_geom_projradius_directed geometry/geom_projradius_directed.cpp)
target_link_libraries(test_geom_projradius_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_projradius_directed
  PREFIX Vtb_geom_projradius
  TOP_MODULE tb_geom_projradius
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_projscale.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_projradius.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_projradius.sv)
add_test(NAME geom_projradius_directed COMMAND test_geom_projradius_directed)
set_tests_properties(geom_projradius_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 600)

add_test(NAME lint_zhao_geom_projradius
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_projradius
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_projradius.sv)
set_tests_properties(lint_zhao_geom_projradius PROPERTIES LABELS "lint;fast")

# ...and the POSITIVE CONTROL for the one guard legal stimulus cannot reach.
# `saturated_o` needs a quotient past 2^31 subpixels -- 8.4 million -- which no
# legal camera produces, so it is fired through a COMMITTED MUTANT whose
# overflow test is narrowed from bit 31 to bit 12. INVERTED POLARITY: it passes
# when the counter fires and the answer clamps.
add_executable(test_geom_projradius_mutant_control mutants/geom_projradius_mutant_control.cpp)
target_link_libraries(test_geom_projradius_mutant_control PRIVATE zhao_harness)
verilate(test_geom_projradius_mutant_control
  PREFIX Vzhao_geom_projradius_mutant
  TOP_MODULE zhao_geom_projradius_mutant
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mutants/zhao_geom_projradius_mutant.sv)
add_test(NAME geom_projradius_mutant_control COMMAND test_geom_projradius_mutant_control)
set_tests_properties(geom_projradius_mutant_control PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

add_test(NAME lint_zhao_view_projscale
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_view_projscale
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_view_projscale.sv)
set_tests_properties(lint_zhao_view_projscale PROPERTIES LABELS "lint;fast")

# ...and the PACKER, which is the third statement of the same layout. It
# rebuilds the committed golden from its own constants and fires each of its
# four refusals; geom_ladderbank_directed requires zref::creature_page::build
# to reproduce that SAME file and the RTL to read it. Three statements, one
# artefact.
add_test(NAME pack_creature_ladder_check
         COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/pack/mkcreatureladder.py" --check)
set_tests_properties(pack_creature_ladder_check PROPERTIES LABELS "fast;tools" TIMEOUT 120)

# ---------------------------------------------------------------------------
# PART.STATE's capacity backstop -- gap I8, `capacity_full_o`.
#
# A SEPARATE suite rather than more cases in part_state_directed, because that
# suite's check count (78) is quoted as evidence in the ledger and in the run
# log, and a number that moves whenever somebody adds a case is a number nobody
# can use.
#
# Same parameters as part_state_directed for the same reason: at the required
# tier of 32,768 a full generation is not reachable in a test at all.
add_executable(test_part_state_capacity_backstop particles/part_state_capacity_backstop.cpp)
target_link_libraries(test_part_state_capacity_backstop PRIVATE zhao_harness zhao_zref)
verilate(test_part_state_capacity_backstop
  PREFIX Vzhao_part_state
  TOP_MODULE zhao_part_state
  VERILATOR_ARGS -GCAPACITY=8 -GSPECIES_N=4 -GCHILD_D=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_state.sv)
add_test(NAME part_state_capacity_backstop COMMAND test_part_state_capacity_backstop)
set_tests_properties(part_state_capacity_backstop PROPERTIES LABELS "fast;particles" TIMEOUT 300)

# ---------------------------------------------------------------------------
# PART.STATE's TICK BOUNDARY -- the child that arrives as the tick is closing.
#
# `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md`: six children emitted into
# the composed console, five written, and every counter that exists to explain a
# missing child reading ZERO. The composed smoke bench shows it and cannot gate
# against it, because it exercises exactly ONE arrival time -- whichever one
# PART.SPAWN happens to produce.
#
# This suite sweeps the arrival cycle across the whole tick and holds the offer
# the way a real producer holds `chl_valid_o`, so the append-phase edge and the
# S_DONE window are visited BY CONSTRUCTION rather than by luck. Two of its
# checks are positive controls on the sweep itself: a sweep that quietly missed
# the window would satisfy every other check while testing nothing.
#
# A SEPARATE suite again, for the reason the backstop suite gives above:
# part_state_directed's check count (78) is quoted as evidence.
#
# Same parameters as the other two PART.STATE suites -- at the required tier of
# 32,768 neither capacity nor the boundary is reachable in a test at all.
add_executable(test_part_state_tick_boundary particles/part_state_tick_boundary.cpp)
# zhao_zref for zref/zref_particle.hpp: records are built with the RATIFIED
# particle128 codec, never hand-written shifts.
target_link_libraries(test_part_state_tick_boundary PRIVATE zhao_harness zhao_zref)
verilate(test_part_state_tick_boundary
  PREFIX Vzhao_part_state
  TOP_MODULE zhao_part_state
  VERILATOR_ARGS -GCAPACITY=8 -GSPECIES_N=4 -GCHILD_D=8
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_state.sv)
add_test(NAME part_state_tick_boundary COMMAND test_part_state_tick_boundary)
set_tests_properties(part_state_tick_boundary PROPERTIES LABELS "fast;particles" TIMEOUT 300)
# ---------------------------------------------------------------------------
# GEOM.POSE's BONE-MATRIX PALETTE STORE -- the block entry I10 was missing.
#
# `zhao_geom_pose_decode` streams the palette one bone per beat and
# `zhao_geom_skin` wants two whole matrices latched with the vertex. Nothing in
# the tree joined them, so GEOM.SKIN's matrix ports were a console BOUNDARY.
# This block stores the palette in RAM and answers two reads per vertex.
#
# It does NO arithmetic, so the failures are all routing and holding: the wrong
# matrix for the right bone, a walk long enough to drop GEOM.SKIN below the
# owner-ruled 120,000 vertices/frame, or a stalled offer whose matrices drift
# away from the vertex beside them. Every counter is checked as a DELTA.
add_executable(test_geom_pose_palette_directed geometry/geom_pose_palette_directed.cpp)
target_link_libraries(test_geom_pose_palette_directed PRIVATE zhao_harness)
verilate(test_geom_pose_palette_directed
  PREFIX Vzhao_geom_pose_palette
  TOP_MODULE zhao_geom_pose_palette
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_palette.sv)
add_test(NAME geom_pose_palette_directed COMMAND test_geom_pose_palette_directed)
set_tests_properties(geom_pose_palette_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_geom_pose_palette
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_pose_palette
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_pose_palette.sv)
set_tests_properties(lint_zhao_geom_pose_palette PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE INSTRUMENT, DEMONSTRATED. Every counter this block owns balances exactly
# when bone1's matrix is fetched from bone0's address: the walk is still seven
# clocks, the handshake still completes, `vertices_served_o` still advances once
# per vertex, and a two-weight vertex is silently skinned with one bone twice.
# The section 1 differential is the ONLY detector for that fault class, so the
# mutant below proves it fires. INVERTED POLARITY: the control PASSES when the
# differential FAILS. Evidence about the instrument, not about the design.
add_executable(test_geom_pose_palette_mutant_control
               geometry/geom_pose_palette_mutant_control.cpp)
target_link_libraries(test_geom_pose_palette_mutant_control PRIVATE zhao_harness)
verilate(test_geom_pose_palette_mutant_control
  PREFIX Vzhao_geom_pose_palette_mutant
  TOP_MODULE zhao_geom_pose_palette_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_pose_palette_mutant.sv)
add_test(NAME geom_pose_palette_mutant_control
         COMMAND test_geom_pose_palette_mutant_control)
set_tests_properties(geom_pose_palette_mutant_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)
# ---------------------------------------------------------------------------
# zhao_console_core SMOKE, and its slot-overflow POSITIVE CONTROL.
#
# Registered by the integration owner 2026-09-19. Both existed and neither was
# a ctest, so both ran only when somebody remembered the script -- which is the
# uncashed-cheque shape this repo has a chapter about, and the agent that built
# the control said so in its own report rather than leaving it for someone to
# find.
#
# These are PowerShell-driven rather than verilated targets: the script builds
# the 120-source closure itself because `tests/CMakeLists.txt` cannot carry a
# 3,800-line composer's parameter forwarding.
#
# The mutant runs with INVERTED POLARITY -- it passes only when
# `terr_pl_slot_overflow_o` FIRES. Production under identical stimulus reads 0.
# It is a WRAPPER, not a copy, so `mutant_copy_drift.py` skips it and there is
# no copied body to go stale.
add_test(NAME console_core_smoke
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/prod/run_console_core_smoke.ps1)
set_tests_properties(console_core_smoke PROPERTIES LABELS "prod;smoke")

add_test(NAME console_core_slot_overflow_control
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/prod/run_console_core_smoke.ps1 -Mutant)
set_tests_properties(console_core_slot_overflow_control PROPERTIES
  LABELS "prod;mutant")

# The second inverted-polarity control on the same bench, added 2026-09-19 with
# the composition that closed core header entries I2 and I3. The smoke now
# proves PART.TABLE by what comes OUT of three consumers -- a STICK contact, one
# child per collision, an exact colour byte -- and none of that is evidence
# until the checks have been seen to fail. -NoTableLoad skips the four loads and
# changes nothing else; the script inverts the verdict, so this test PASSES when
# the bench fails. It is evidence about the instrument, not about the design.
add_test(NAME console_core_table_load_control
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/prod/run_console_core_smoke.ps1 -NoTableLoad)
set_tests_properties(console_core_table_load_control PROPERTIES
  LABELS "prod;mutant")
# ===========================================================================
# PLATFORM: SYS.PLL and SYS.RESET. Registered 2026-09-19, the pass that wrote
# them.
#
# Both blocks were `blocked_on: hardware` until board_truth.json (2026-09-13)
# answered the reference clock, the part and the reset contract. Each bench
# verilates a WRAPPER that instantiates its production block TWICE from one
# stimulus -- shipping defaults beside a moved parameterisation -- because a
# parameter is only a knob if moving it moves the machine, and two instances in
# one process cannot differ by a stimulus accident the way two runs can. The
# wrappers copy nothing, so `mutant_copy_drift.py` correctly ignores them.
#
# WHAT sys_pll_directed IS NOT EVIDENCE ABOUT: `altera_pll`. The vendor body
# sits behind `ifdef ZHAO_SYS_PLL_VENDOR`, which is never defined in any
# Verilator lane, so these targets measure the behavioural model only.
# ===========================================================================
add_executable(test_sys_pll_directed platform/sys_pll_directed.cpp)
target_link_libraries(test_sys_pll_directed PRIVATE zhao_harness)
verilate(test_sys_pll_directed
  PREFIX Vtb_zhao_sys_pll
  TOP_MODULE tb_zhao_sys_pll
  SOURCES ${CMAKE_SOURCE_DIR}/tests/platform/tb_zhao_sys_pll.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/sys/zhao_sys_pll.sv)
add_test(NAME sys_pll_directed COMMAND test_sys_pll_directed)
set_tests_properties(sys_pll_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_sys_reset_directed platform/sys_reset_directed.cpp)
target_link_libraries(test_sys_reset_directed PRIVATE zhao_harness)
verilate(test_sys_reset_directed
  PREFIX Vtb_zhao_sys_reset
  TOP_MODULE tb_zhao_sys_reset
  SOURCES ${CMAKE_SOURCE_DIR}/tests/platform/tb_zhao_sys_reset.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/sys/zhao_sys_reset.sv)
add_test(NAME sys_reset_directed COMMAND test_sys_reset_directed)
set_tests_properties(sys_reset_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_sys_pll
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_sys_pll
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/sys/zhao_sys_pll.sv)
set_tests_properties(lint_zhao_sys_pll PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_zhao_sys_reset
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_sys_reset
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/sys/zhao_sys_reset.sv)
set_tests_properties(lint_zhao_sys_reset PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# The board framework, and since 2026-09-19 it DOES instantiate
# zhao_console_core -- so this target no longer names three files.
#
# The comment here used to say the board "does NOT instantiate
# zhao_console_core", and the three-file closure was correct for as long as that
# was true. The board's closure is now the console's 170-odd sources plus the
# two platform blocks, and a hand-typed list of them in this file would be a
# SECOND copy of a source list the tree already keeps in one place. The runner
# reads it from design/fit_targets.yml, exactly as run_console_core_smoke.ps1
# does, so the board lint, the smoke bench and the fit measure the same files.
#
# It asserts SILENCE, not just RC 0: no %Warning and no %Error over 184
# elaborated modules. That is a real gate on 1,038 generated port connections --
# mistype one and it is IMPLICIT + UNDRIVEN + UNUSEDSIGNAL here in seconds
# instead of a PINMISSING hours into a fit. Proven by doing exactly that.
add_test(NAME lint_zhao_console_board
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/prod/run_console_board_lint.ps1)
set_tests_properties(lint_zhao_console_board PROPERTIES
  LABELS "lint;fast" TIMEOUT 900
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# And the staleness gate, which is the entire reason the board may be generated
# at all. fpga/rtl/prod/zhao_console_board.sv is emitted from
# zhao_console_core.sv's own port list by tools/quartus/gen_console_board.py; a
# port added to the core and not regenerated here turns this RED IN SECONDS,
# where the alternative -- a 1,038-port hand transcription -- goes stale
# silently and in the flattering direction. This repository has a chapter about
# generated files nobody regenerates, and it has been bitten by one three times.
add_test(NAME console_board_generated_freshness
  COMMAND "${Python3_EXECUTABLE}"
          ${CMAKE_SOURCE_DIR}/tools/quartus/gen_console_board.py --check)
set_tests_properties(console_board_generated_freshness PROPERTIES
  LABELS "lint;fast")
# ---------------------------------------------------------------------------
# PART.PROJECT -- the particle's seat at the SHARED projector.
#
# zhao_console_core.sv header entry I24: "NOTHING IN THIS CORE PROJECTS A
# PARTICLE", which blocked PART.EXPAND, PART.SOFT and PART.LADDER. The block
# under test contains no projection arithmetic -- it time-multiplexes client A
# of the one zhao_project_service so that a third CLIENT does not have to be a
# third PORT.
#
# The bench MODELS the projector rather than instantiating it. That is not a
# shortcut: what this block depends on is the client-A contract (in order, one
# accept per clock, no backpressure on the way out), and a model states that
# contract where an instance would merely satisfy it. Whether the projection is
# RIGHT is zhao_project_core's law and geom_project_directed.cpp's bench; a
# second opinion here would be a second implementation.
#
# SLOTS=4 for the reason part_table_directed runs SPECIES_N=8: the in-flight
# backpressure is reachable with legal stimulus only below the projector's
# latency, and above it the refusal that handles it is not evidence about
# anything.
#
# zhao_zref for zref/zref_particle.hpp -- records are built with the RATIFIED
# particle128 codec and the size oracle calls zref::particle_radius, never
# hand-written shifts.
add_executable(test_part_project_directed particles/part_project_directed.cpp)
target_link_libraries(test_part_project_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_project_directed
  PREFIX Vzhao_part_project
  TOP_MODULE zhao_part_project
  VERILATOR_ARGS -GSLOTS=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_project.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
add_test(NAME part_project_directed COMMAND test_part_project_directed)
set_tests_properties(part_project_directed PROPERTIES LABELS "fast;particles" TIMEOUT 300)

# THE POSITIVE CONTROL FOR `owner_unroutable_o`, which moved OUT of the bench
# above on 2026-09-23 and had to. That counter was fired from `a_payload_i` by
# forcing an UNCLAIMED owner encoding; the fourth request arm claims 2'd3, the
# last code the two-bit field holds, so every code now routes and no stimulus
# at any port can move it. A guard you cannot reach with legal stimulus needs a
# committed mutant (CLAUDE.md), and this is it: the same block with the fourth
# RESULT arm removed, driven with INVERTED POLARITY -- it passes when the
# counter FIRES.
add_executable(test_part_project_owner_mutant_control
  particles/part_project_owner_mutant_control.cpp)
target_link_libraries(test_part_project_owner_mutant_control PRIVATE zhao_harness)
verilate(test_part_project_owner_mutant_control
  PREFIX Vzhao_part_project_owner_mutant
  TOP_MODULE zhao_part_project_owner_mutant
  VERILATOR_ARGS -GSLOTS=4
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_part_project_owner_mutant.sv)
add_test(NAME part_project_owner_mutant_control
  COMMAND test_part_project_owner_mutant_control)
set_tests_properties(part_project_owner_mutant_control
  PROPERTIES LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_project
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_project
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_project.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
set_tests_properties(lint_zhao_part_project PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# PROJSHARE -- owner ruling R3's owed schedule proof, MEASURED.
#
# THE ENTRY DIRECTLY ABOVE IS WHY THIS EXISTS. It says, correctly and in terms,
# that part_project_directed "MODELS the projector rather than instantiating
# it" -- which is the right call for the question that bench asks. But it means
# the COMPOSED multi-client throughput has never been measured anywhere in this
# tree, and design/contracts/FORGE.SHADOW.md records exactly that as the reason
# half of R3's proof could not be produced:
#
#   "The RATE half needs a bench holding zhao_part_project against the real
#    zhao_proj_subsystem with every arm saturated. tb_part_project drives the
#    block STANDALONE ... so the composed multi-client throughput has never
#    been measured, and cannot be from any bench in this tree."
#
# This is that bench. It instantiates zhao_project_service rather than
# zhao_proj_subsystem, and tb_projshare.sv's header carries the argument for
# why those are the same arbitration (the subsystem passes both client arms
# straight through, zhao_proj_subsystem.sv:203-234) -- with the citation to
# check if the shell ever registers one.
#
# NO zhao_harness: this bench counts clocks and reads counters, uses none of
# the harness's helpers, and defines its own sc_time_stamp shim. Linking a
# library for nothing is how a test's closure stops describing its subject.
#
# The numbers it produces are written up in
# reports/R3-CLIENT-A-SCHEDULE-PROOF-20260923.md.
add_executable(test_projshare_contention common/projshare_contention.cpp)
verilate(test_projshare_contention
  PREFIX Vtb_projshare
  TOP_MODULE tb_projshare
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_project.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_service.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_core.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/common/tb_projshare.sv)
add_test(NAME projshare_contention COMMAND test_projshare_contention)
set_tests_properties(projshare_contention PROPERTIES
  LABELS "fast;particles;geometry" TIMEOUT 300)

add_test(NAME lint_tb_projshare
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module tb_projshare
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_project.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_service.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_project_core.sv
                 ${CMAKE_CURRENT_SOURCE_DIR}/common/tb_projshare.sv)
set_tests_properties(lint_tb_projshare PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# ===========================================================================
# GEOM.LOOM -- the Transform Loom (fpga/rtl/geometry/zhao_geom_loom.sv)
# ===========================================================================
# THREE BUILDS OF ONE SOURCE, and each exists to reach something the others
# cannot:
#
#   geom_loom_directed  the shipping parameters (MAX_NODES=1024, MUL_LANES=1).
#                       Carries the 1,024-deep DRIFT MEASUREMENT the contract
#                       asks for -- "a NUMBER for a question nobody has asked
#                       yet" -- and the per-node clock cost the header's
#                       frame-occupancy arithmetic is built on.
#
#   geom_loom_small     MAX_NODES=16 with IDXW still 10. The OVERFLOW refusal
#                       is UNREACHABLE by legal stimulus at 1024/10 (the index
#                       is ten bits and the bound is 1024), so this is its
#                       POSITIVE CONTROL -- a legal parameterisation rather
#                       than a committed mutant, which is why IDXW is
#                       deliberately not derived from MAX_NODES.
#
#   geom_loom_lanes3    MUL_LANES=3. The knob's other position, held to the
#                       SAME oracle. A knob whose alternative is never
#                       exercised is a knob that has silently stopped turning
#                       (the R4 note above zhao_geom_mat3x4_mul says the same).
#                       The walk is 12 instead of 37, so the per-node cost
#                       macro moves with it.
#
# The block's closure is its own file plus the two it REUSES rather than
# reimplements: zhao_geom_mat3x4_mul (the compose and its rounding law) and
# zhao_field_sin + its generated ROM.
set(ZHAO_LOOM_SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_loom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)

add_executable(test_geom_loom_directed geometry/geom_loom_directed.cpp)
target_link_libraries(test_geom_loom_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_loom_directed
  PREFIX Vzhao_geom_loom
  TOP_MODULE zhao_geom_loom
  SOURCES ${ZHAO_LOOM_SOURCES})
add_test(NAME geom_loom_directed COMMAND test_geom_loom_directed)
set_tests_properties(geom_loom_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME geom_loom_random COMMAND test_geom_loom_directed --random 60)
set_tests_properties(geom_loom_random PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)
add_test(NAME geom_loom_random_nightly COMMAND test_geom_loom_directed --random 600)
set_tests_properties(geom_loom_random_nightly PROPERTIES LABELS "nightly" TIMEOUT 3600)

add_executable(test_geom_loom_small geometry/geom_loom_directed.cpp)
target_compile_definitions(test_geom_loom_small PRIVATE ZHAO_LOOM_MAX_NODES=16)
target_link_libraries(test_geom_loom_small PRIVATE zhao_harness zhao_zref)
verilate(test_geom_loom_small
  PREFIX Vzhao_geom_loom
  TOP_MODULE zhao_geom_loom
  VERILATOR_ARGS -GMAX_NODES=16
  SOURCES ${ZHAO_LOOM_SOURCES})
add_test(NAME geom_loom_small COMMAND test_geom_loom_small)
set_tests_properties(geom_loom_small PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

add_executable(test_geom_loom_lanes3 geometry/geom_loom_directed.cpp)
target_compile_definitions(test_geom_loom_lanes3 PRIVATE ZHAO_LOOM_NODE_CLOCKS=23)
target_link_libraries(test_geom_loom_lanes3 PRIVATE zhao_harness zhao_zref)
verilate(test_geom_loom_lanes3
  PREFIX Vzhao_geom_loom
  TOP_MODULE zhao_geom_loom
  VERILATOR_ARGS -GMUL_LANES=3
  SOURCES ${ZHAO_LOOM_SOURCES})
add_test(NAME geom_loom_lanes3 COMMAND test_geom_loom_lanes3)
set_tests_properties(geom_loom_lanes3 PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

add_test(NAME lint_zhao_geom_loom
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_loom
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_loom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
set_tests_properties(lint_zhao_geom_loom PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# GEOM.LOOM's NODE-STREAM CARRIER -- zhao_console_core entry I50, owner
# rulings R58 and R69.
#
# SW.STREAM stages the topologically sorted node stream in HPS DDR; a CSR
# mailbox names it; hardware acknowledges. The DUT is the carrier WITH THE REAL
# LOOM behind it, because both of its hard laws are statements about the loom's
# own S_RUN/S_DRAIN behaviour and a mock would have been written to agree with
# whatever the carrier does -- which is exactly how the first draft of law 4
# came to be backwards. What IS played here is the HPS-DDR bridge.
set(ZHAO_LOOMFEED_SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_loomfeed.sv
    ${ZHAO_LOOM_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_loomfeed_chain.sv)

add_executable(test_geom_loomfeed_directed geometry/geom_loomfeed_directed.cpp)
target_link_libraries(test_geom_loomfeed_directed PRIVATE zhao_harness)
verilate(test_geom_loomfeed_directed
  PREFIX Vtb_geom_loomfeed_chain
  TOP_MODULE tb_geom_loomfeed_chain
  SOURCES ${ZHAO_LOOMFEED_SOURCES})
add_test(NAME geom_loomfeed_directed COMMAND test_geom_loomfeed_directed)
set_tests_properties(geom_loomfeed_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THE POSITIVE CONTROL for `ret_overflow_o`. INVERTED POLARITY: it passes when
# the counter FIRES. Its DUT is the committed mutant, whose one substantive
# change is the removal of the return credit.
add_executable(test_geom_loomfeed_mutant_control geometry/geom_loomfeed_mutant_control.cpp)
target_link_libraries(test_geom_loomfeed_mutant_control PRIVATE zhao_harness)
verilate(test_geom_loomfeed_mutant_control
  PREFIX Vtb_geom_loomfeed_mutant
  TOP_MODULE tb_geom_loomfeed_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_loomfeed_mutant.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/mutants/tb_geom_loomfeed_mutant.sv)
add_test(NAME geom_loomfeed_mutant_control COMMAND test_geom_loomfeed_mutant_control)
set_tests_properties(geom_loomfeed_mutant_control PROPERTIES LABELS "fast;mutant" TIMEOUT 600)

add_test(NAME lint_zhao_geom_loomfeed
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_loomfeed
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_loomfeed.sv)
set_tests_properties(lint_zhao_geom_loomfeed PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# --------------------------------------------------------------------------
# --------------------------------------------------------------------------
# MATERIAL.RESOLVE's REQUEST ISSUE POINT and RESPONSE JOIN (core entry I49).
#
# The RESOLVE is owned by `material_resolve_rtl_directed`; this driver plays
# the resolver's role and tests the JOIN, which is a timing property: that the
# material published at the shell's triangle door is the one belonging to the
# triangle at the door, across a material change, in both directions (a late
# publication and an early one), and that a repeat of the same material costs
# no second resolve. Both of the block's structural guards are FIRED here with
# legal stimulus at its own ports -- the disposal events are inputs, so a
# departure with no arrival is a legal thing to present, and neither guard
# owes a committed mutant.
# ---------------------------------------------------------------------------
# THE ACCEPTANCE TEST OWNER RULING 1 NAMES (2026-09-22)
# ---------------------------------------------------------------------------
# "Interleave textured mesh A, a particle batch, and textured mesh B, with
#  different material sets and forced backpressure ... The particles draw with
#  their own state and cause no missing-material faults; both meshes preserve
#  their state; a deliberately incompatible untextured mesh is still refused and
#  counted."
#
# A SUBSYSTEM bench over the PRODUCTION modules in the composer's arrangement --
# zhao_geom_clipdoor -> zhao_material_window -> R197's gate. It is NOT a
# rendered-pixel comparison, and the driver's header says so in terms rather
# than letting a green tick imply one.
add_executable(test_partmat_acceptance prod/partmat_acceptance.cpp)
target_link_libraries(test_partmat_acceptance PRIVATE zhao_harness zhao_zref)
verilate(test_partmat_acceptance
  PREFIX Vtb_partmat_acceptance
  TOP_MODULE tb_partmat_acceptance
  VERILATOR_ARGS --assert
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipdoor.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_window.sv
          ${CMAKE_SOURCE_DIR}/tests/prod/tb_partmat_acceptance.sv)
add_test(NAME partmat_acceptance COMMAND test_partmat_acceptance)
set_tests_properties(partmat_acceptance PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

# ---------------------------------------------------------------------------
# PROCMAT -- the acceptance test owner completion ruling 2 (2026-09-22) names
# ---------------------------------------------------------------------------
# COMMAND BYTES TO GEOM.CLIP'S INPUT, over nine production modules: the real
# decoder and executor, the real page bank reading a real forge page through its
# own MEM.GUARD port, both position evaluators, the assembler, the clip door and
# the material window. Nothing is injected at a downstream boundary -- every
# draw is a packed DrawProcedural record and every forge record is bytes in a
# memory image the bank reads for itself.
#
# THE QUESTION IT ANSWERS is the ruling's own: does the (set handle, record id)
# pair the game declared reach the raster path -- including when the set's
# GENERATION changes, which under the reading this packet removed would have
# selected a DIFFERENT material record. The driver's header says what it is
# not: it is not a rendered-pixel comparison, and the console smoke drives no
# procedural draw at all, so a green smoke says nothing about this path.
add_executable(test_procmat_acceptance prod/procmat_acceptance.cpp)
target_link_libraries(test_procmat_acceptance PRIVATE zhao_harness zhao_zref)
verilate(test_procmat_acceptance
  PREFIX Vtb_procmat_acceptance
  TOP_MODULE tb_procmat_acceptance
  VERILATOR_ARGS --assert
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
               ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
               ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry
               ${CMAKE_SOURCE_DIR}/fpga/rtl/raster
               ${CMAKE_SOURCE_DIR}/fpga/rtl/field
               ${CMAKE_CURRENT_SOURCE_DIR}/compositor
  SOURCES
    ${ZHAO_ABI_PKG}
    ${ZHAO_ZHAO_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_exec.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_pagebank.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jitter_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim_eval.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_ring_eval.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_assemble.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipdoor.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_window.sv
    ${CMAKE_SOURCE_DIR}/tests/prod/tb_procmat_acceptance.sv)
add_test(NAME procmat_acceptance COMMAND test_procmat_acceptance)
set_tests_properties(procmat_acceptance PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

add_test(NAME lint_tb_procmat_acceptance
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module tb_procmat_acceptance
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/common
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/generated
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/geometry
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/raster
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/field
    -I${CMAKE_CURRENT_SOURCE_DIR}/compositor
    ${ZHAO_ABI_PKG}
    ${ZHAO_ZHAO_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_decoder.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/command/zhao_cmd_exec.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_pagebank.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_jitter_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_prim_eval.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_ring_eval.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_assemble.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipdoor.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_window.sv
    ${CMAKE_SOURCE_DIR}/tests/prod/tb_procmat_acceptance.sv)
set_tests_properties(lint_tb_procmat_acceptance PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# PART.CLIPFEED -- the particle arm of GEOM.CLIPDOOR (owner ruling 1, 2026-09-22)
# ---------------------------------------------------------------------------
# The depth ARITHMETIC is geom_depthquant_directed's and is not re-derived here;
# this block holds a second INSTANCE of that law and no arithmetic of its own.
# What this driver tests is everything around the instance: that each triangle
# carries ITS OWN particle's canonical depth (the converter answers in
# COMPLETION order, so a block pairing by arrival order would swap them with
# every count still balancing), that beats leave in OFFER order (particles are
# depth-tested and never written, so overlap is resolved by draw order alone),
# and that the 22 -> 21 narrowing refuses a vertex that breaks the clamp premise
# rather than wrapping it. Sections 3 and 5 force backpressure, which is the
# only condition under which the pairing can go wrong at all.
# The converter's own closure is the SAME LIST GEOM.VATTR uses, which is the
# point: a second INSTANCE of one law elaborates the same files.
set(ZHAO_PART_CLIPFEED_SOURCES
  ${ZHAO_GEOM_VATTR_SOURCES}
  ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_clipfeed.sv)
add_executable(test_part_clipfeed_directed particles/part_clipfeed_directed.cpp)
target_link_libraries(test_part_clipfeed_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_clipfeed_directed
  PREFIX Vzhao_part_clipfeed
  TOP_MODULE zhao_part_clipfeed
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_PART_CLIPFEED_SOURCES})
add_test(NAME part_clipfeed_directed COMMAND test_part_clipfeed_directed)
set_tests_properties(part_clipfeed_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
add_test(NAME lint_part_clipfeed
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_part_clipfeed ${ZHAO_PART_CLIPFEED_SOURCES})
set_tests_properties(lint_part_clipfeed PROPERTIES
  LABELS "fast;nightly" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---- PART.CLIPFEED's fragment-state carriage: THE POSITIVE CONTROL --------
# `part_clipfeed_directed` section 6 asserts that the pass-7 law emitted at the
# door is the ACCEPTED particle's, not the one on the input ports at the emit.
# In this console PART.EXPAND drives that law as the constant pair 1/0, so a
# block that read the ports at the emit would be byte-identical under every
# stimulus the console can produce -- section 6 is the only thing that can tell
# them apart, and a green whose red has never been seen is a claim.
#
# This is the red. Same block, ONE line changed, INVERTED polarity: it passes
# when the emitted word disagrees with the record. The negative control is
# `part_clipfeed_directed` itself.
add_executable(test_part_clipfeed_emitstate_mutant_control
  particles/part_clipfeed_emitstate_mutant_control.cpp)
target_link_libraries(test_part_clipfeed_emitstate_mutant_control PRIVATE zhao_harness)
verilate(test_part_clipfeed_emitstate_mutant_control
  PREFIX Vzhao_part_clipfeed_emitstate_mutant
  TOP_MODULE zhao_part_clipfeed_emitstate_mutant
  VERILATOR_ARGS --assert
  SOURCES ${ZHAO_GEOM_VATTR_SOURCES}
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_part_clipfeed_emitstate_mutant.sv)
add_test(NAME part_clipfeed_emitstate_mutant_control
  COMMAND test_part_clipfeed_emitstate_mutant_control)
set_tests_properties(part_clipfeed_emitstate_mutant_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

add_executable(test_material_window_directed texture/material_window_directed.cpp)
target_link_libraries(test_material_window_directed PRIVATE zhao_harness zhao_zref)
verilate(test_material_window_directed
  PREFIX Vzhao_material_window
  TOP_MODULE zhao_material_window
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_window.sv)
add_test(NAME material_window_directed COMMAND test_material_window_directed)
set_tests_properties(material_window_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_material_window
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_material_window
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/texture/zhao_material_window.sv)
set_tests_properties(lint_material_window PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.ATTRPACK -- the three-plane front end over one shared GEOM.ATTRSETUP.
#
# The arithmetic is NOT re-tested here; `geom_attrsetup_directed` owns it. This
# driver tests what the front end adds: which attribute slot reaches which
# plane, that lane k's answer lands in plane k, and that a second triangle
# inherits nothing from the first. Its counters are read as DELTAS, and the
# invariant it asserts is `planes == 3 * triangles` -- a lane that stopped
# asking breaks the ratio while every handshake still looks healthy.
add_executable(test_geom_attrpack_directed geometry/geom_attrpack_directed.cpp)
target_link_libraries(test_geom_attrpack_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_attrpack_directed
  PREFIX Vzhao_geom_attrpack
  TOP_MODULE zhao_geom_attrpack
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrpack.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrsetup.sv)
add_test(NAME geom_attrpack_directed COMMAND test_geom_attrpack_directed)
set_tests_properties(geom_attrpack_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_geom_attrpack
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_geom_attrpack
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrpack.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_attrsetup.sv)
set_tests_properties(lint_geom_attrpack PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# THE PACKET-D ATTRIBUTE CARRIAGE'S POSITIVE CONTROL. Registered 2026-09-19
# with GEOM.ATTRPACK.
#
# The smoke bench now asserts that GEOM.ATTRPACK packed three planes per
# triangle and that its count agrees with GEOM.SETUP's. Neither of those can
# tell whether the PLANES are what the rasteriser interpolates -- zero planes
# interpolate to zero without setting any error, which is precisely how the
# missing producer went unnoticed for so long, and a producer emitting
# rubbish would write the same 1,536 pixels.
#
# THIS TEST RAN `-BadAttribute` AND MEASURED NOTHING FROM 2026-09-19 TO
# 2026-09-21. Found by FIELDLANE. `-BadAttribute` was RETIRED on 2026-09-19
# when core entry I46 closed -- the smoke script's own line 121 records the
# retirement -- and the ctest was never updated. It stayed GREEN and LABELLED
# the whole time, because `powershell -File` binds an undeclared switch to
# NOTHING and says NOTHING: no error, no warning, RC 0. The retired flag simply
# ran the PLAIN smoke, which passes.
#
# A test that cannot fail is worse than a missing one: a missing test is
# visibly absent, while this one appeared in every `ctest` summary as evidence
# about a control that no longer existed. The script now REJECTS unrecognised
# arguments with RC 2, so this class cannot recur silently.
#
# REPLACED RATHER THAN DELETED, because the same audit found the opposite
# defect one line over: `-BadDescriptor` is a COMMITTED positive control -- the
# smoke script calls it "THE POSITIVE CONTROL FOR THE GEOMETRY ASSET PATH" and
# implements it in two places -- that NO ctest and no gate list ever ran. One
# built-and-uninstalled control, one installed-and-dead test; this edit
# settles both.
#
# -BadDescriptor corrupts ONE byte of the descriptor's reserved span (36..59).
# The script inverts the verdict, so this test PASSES when the bench fails: if
# the descriptor were not really reaching the asset path, changing a reserved
# byte could not change the outcome.
#
# THIS FORM PASSES **WITH ONE `%Fatal` LINE, BY DESIGN**, and that is not a
# footnote -- it is the form's whole mechanism. The bench is SUPPOSED to die on
# the corrupted descriptor; the `%Fatal` IS the evidence, and the inverted
# driver converts it into a pass.
#
# So a blanket "`%Fatal` must be 0" sweep REDS A WORKING CONTROL. Owner ruling
# R207 is written about forms where a stray `%Fatal` means an arm ran that
# should not have -- a `$finish` is not a `return`, and one form once passed
# while printing nine of them. Applied to an INVERTED control the same rule
# inverts: here zero `%Fatal` lines would mean the corruption changed nothing,
# which is the failure.
#
# Reported by POSEPAGE, 2026-09-21, after running every form and reading the
# logs rather than the exit codes. Recorded here because this test is new and
# the next reader to sweep for `%Fatal` will find it first.
add_test(NAME console_core_baddescriptor_control
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/prod/run_console_core_smoke.ps1 -BadDescriptor)
set_tests_properties(console_core_baddescriptor_control PROPERTIES
  LABELS "prod;mutant")
# --------------------------------------------------------------------------
# THE CONSOLE'S FIELD HOST -- zhao_field_host: the loader, the residency
# directory, the arbiter and the lane front around the FIELD v3 fabric.
#
# The ARITHMETIC is not re-tested here. `zhao_field_v3_engine` is the fabric and
# tests/differential/field_v3_full_directed.cpp is its differential against
# `zfield::interpret`. Every case in this driver is about the FRONT, and each is
# a counter that must move: runs, noprog, loads, load_defers, grants,
# contended_grants and ld_oob. The fabric's own alarms are asserted zero here as
# a composition regression guard, and their positive controls live with the
# block that raises them -- which is where a fault can actually be constructed.
add_executable(test_field_host_directed field/field_host_directed.cpp)
target_link_libraries(test_field_host_directed PRIVATE zhao_harness)
verilate(test_field_host_directed
  PREFIX Vzhao_field_host
  TOP_MODULE zhao_field_host
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_host.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
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
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_host_directed COMMAND test_field_host_directed)
set_tests_properties(field_host_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)
# --------------------------------------------------------------------------
# THE ASSOCIATION-AWARE FIELD HOST (packet H1) -- zhao_field_host_v2.
#
# `zhao_field_host` above is RETAINED as the named oracle (FH02) and keeps its
# own driver. It appears in THIS file and never in a production source list:
# `completion_register.py`'s `superseded_in_closure()` reports every
# INSTANTIATED module for which a higher-versioned sibling exists on disk, so
# composing the unversioned host beside `zhao_field_host_v2` would breach the
# "only the latest version composes" ruling. Two drivers, two hosts, one
# production closure.
#
# Every case is about the OUTPUT CONTRACT -- which canonical output ordinals a
# point owes, whether it produced them, and whether a caller can tell a value
# from a hole. The arithmetic stays with the fabric's own differential.
set(ZHAO_FIELD_HOST_V2_SOURCES
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_host_v2.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
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
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)

add_executable(test_field_host_v2_directed field/field_host_v2_directed.cpp)
target_link_libraries(test_field_host_v2_directed PRIVATE zhao_harness)
verilate(test_field_host_v2_directed
  PREFIX Vzhao_field_host_v2
  TOP_MODULE zhao_field_host_v2
  SOURCES ${ZHAO_FIELD_HOST_V2_SOURCES})
add_test(NAME field_host_v2_directed COMMAND test_field_host_v2_directed)
set_tests_properties(field_host_v2_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# --------------------------------------------------------------------------
# R126's GUARD, SEEN TO FIRE. Owner ruling R126 assigns this to H1 and names
# both traps: Quartus 17.0 rejects a bare module-scope `if`, so the guard lives
# in `initial begin ... end`; and **`--lint-only` does not run `initial`
# blocks**, so a clean Verilator lint is "no evidence whatever" about it.
#
# This target is the evidence. It is the SAME host, elaborated with
# OUT_LANES=5 while the generated schema fixes ZFH_WINDOW_MASK_BITS=7 -- the
# exact disagreement R126 describes, which is invisible at runtime because a
# mask of the wrong width still packs, still transmits and still compares.
#
# THE DRIVER'S POLARITY IS INVERTED: it PASSES when elaboration FATALs. It is
# evidence about the instrument, not about the design, which is why it is a
# separate target with its own build directory tag rather than a case inside
# the directed test.
add_executable(test_field_host_v2_r126 field/field_host_v2_r126_guard.cpp)
target_link_libraries(test_field_host_v2_r126 PRIVATE zhao_harness)
verilate(test_field_host_v2_r126
  PREFIX Vzhao_field_host_v2_r126
  TOP_MODULE zhao_field_host_v2
  VERILATOR_ARGS -GOUT_LANES=5
  SOURCES ${ZHAO_FIELD_HOST_V2_SOURCES})
#
# The ctest runs the INVERTING DRIVER, not the binary. The driver requires the
# fatal text to name BOTH quantities, because "the process exited non-zero" is
# satisfied by a segfault or a kill from another lane sharing this machine, and
# a control that accepts any death would go on passing after somebody deleted
# the guard.
add_test(NAME field_host_v2_r126_guard
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/field/run_r126_guard.ps1
          -Exe $<TARGET_FILE:test_field_host_v2_r126>)
set_tests_properties(field_host_v2_r126_guard PROPERTIES LABELS "fast;mutant" TIMEOUT 300)

# --------------------------------------------------------------------------
# R101's DEFECT, RE-PLANTED AGAINST THE NEW HOST. A COMMITTED POSITIVE CONTROL.
#
# `tests/mutants/zhao_field_host_v2_any_not_all_mutant.sv` is a COPY of the
# production host, renamed so no source list can elaborate it by mistake, with
# exactly one substantive line changed: the completion rule's `== req_mask_c`
# (ALL) becomes `!= '0` (ANY). That is R101 verbatim.
#
# The driver's polarity is INVERTED -- it passes when the mutant ACCEPTS an
# incomplete result. Its NEGATIVE CONTROL is the identical stimulus in
# field_host_v2_directed case 1d polarity B, where production answers 0xF3.
# Neither run is evidence alone.
#
# NOTE THE SOURCE LIST: the production host is ABSENT and the mutant stands in
# its place. A build that carried both would elaborate whichever the top-module
# name selected and the other would sit there looking like coverage.
add_executable(test_field_host_v2_any_not_all_mutant
               field/field_host_v2_any_not_all_mutant.cpp)
target_link_libraries(test_field_host_v2_any_not_all_mutant PRIVATE zhao_harness)
verilate(test_field_host_v2_any_not_all_mutant
  PREFIX Vzhao_field_host_v2_any_not_all_mutant
  TOP_MODULE zhao_field_host_v2_any_not_all_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_host_v2_any_not_all_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
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
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
add_test(NAME field_host_v2_any_not_all_mutant
         COMMAND test_field_host_v2_any_not_all_mutant)
set_tests_properties(field_host_v2_any_not_all_mutant
  PROPERTIES LABELS "fast;mutant" TIMEOUT 900)
# --------------------------------------------------------------------------
# THE ZFH2 HOST-IMAGE ENVELOPE (packet S1). spec/form/field-host-image.md.
#
# Three lanes, because the schema owes three different kinds of evidence and
# a single green tick would hide two of them.
#
#   field_host_schema_roundtrip        every record packed and read back
#                                      through the GENERATED offsets, plus the
#                                      ordinal-versus-window translation on
#                                      crater_ring's measured register set.
#   field_host_schema_generated_freshness
#                                      the committed .hpp and .sv still match
#                                      the schema, AND the C++/SV offset
#                                      parity, the bidirectional FH21 opcode
#                                      parity, and the profile-arity checks
#                                      against field-ir.md 7.1.
#   field_host_mask_type_separation    the COMPILE-FAIL control: assigning a
#                                      RequiredMask to a WindowMask must not
#                                      compile, and the legal use must. Both
#                                      polarities are required -- a
#                                      compile-fail case that has never been
#                                      seen to compile might be failing for a
#                                      typo.
#
# The roundtrip target needs no RTL and no zref: it is a statement about the
# generated header alone, which is exactly what makes it fast enough to run on
# every change to the schema.
add_executable(test_field_host_schema_roundtrip
               differential/field_host_schema_roundtrip.cpp)
target_include_directories(test_field_host_schema_roundtrip PRIVATE
                           ${CMAKE_SOURCE_DIR}/reference/include)
add_test(NAME field_host_schema_roundtrip
         COMMAND test_field_host_schema_roundtrip)
set_tests_properties(field_host_schema_roundtrip PROPERTIES
                     LABELS "fast;nightly")

add_test(NAME field_host_schema_generated_freshness
         COMMAND "${Python3_EXECUTABLE}"
                 ${CMAKE_SOURCE_DIR}/tools/field/gen_field_host_schema.py
                 --check)
set_tests_properties(field_host_schema_generated_freshness PROPERTIES
                     LABELS "lint;fast")

# CXX is passed explicitly rather than trusted to the PATH: this lane's whole
# job is to read a COMPILER's exit code, so it must be the same compiler the
# rest of the tree builds with, not whichever g++ a shell happens to resolve.
add_test(NAME field_host_mask_type_separation
         COMMAND "${Python3_EXECUTABLE}"
                 ${CMAKE_SOURCE_DIR}/tools/field/gen_field_host_schema.py
                 --compile-fail-control)
set_tests_properties(field_host_mask_type_separation PROPERTIES
                     ENVIRONMENT "CXX=${CMAKE_CXX_COMPILER}"
                     LABELS "fast;nightly;mutant" TIMEOUT 300)
# --------------------------------------------------------------------------

add_test(NAME lint_zhao_field_host
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_host
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_ops_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_host.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_progcache.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_engine.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_core.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_exec.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_svcpath.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_dispatch.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_noise.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_curve.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_spline.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_rot.sv
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
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_sin_rom.sv)
set_tests_properties(lint_zhao_field_host PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# THE FIELD PROGRAM DOORBELL -- zhao_console_core entry I42, owner ruling R43.
#
# SW.STREAM's program-load mailbox, on the R14 pattern. The DUT is the doorbell
# ALONE: the loader's ready and both directory phases are PLAYED by the driver,
# which is what lets a case construct a hit, an eviction or a stalled drain on
# cue. What the COMPOSED article does is checked by the console smoke bench.
#
# Every case is a counter that must move, and each asserts the CORRECT
# behaviour rather than the defect. `ret_overflow_o` is the one that cannot be
# fired with legal stimulus -- the return credit is what makes the state
# unreachable -- so it has a committed mutant beside it.
add_executable(test_field_doorbell_directed field/field_doorbell_directed.cpp)
target_link_libraries(test_field_doorbell_directed PRIVATE zhao_harness)
verilate(test_field_doorbell_directed
  PREFIX Vzhao_field_doorbell
  TOP_MODULE zhao_field_doorbell
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_doorbell.sv)
add_test(NAME field_doorbell_directed COMMAND test_field_doorbell_directed)
set_tests_properties(field_doorbell_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# THE POSITIVE CONTROL for `ret_overflow_o`. INVERTED POLARITY: it passes when
# the counter FIRES. Its DUT is the committed mutant, whose one substantive
# change is the removal of the return credit.
add_executable(test_field_doorbell_mutant_control field/field_doorbell_mutant_control.cpp)
target_link_libraries(test_field_doorbell_mutant_control PRIVATE zhao_harness)
verilate(test_field_doorbell_mutant_control
  PREFIX Vzhao_field_doorbell_mutant
  TOP_MODULE zhao_field_doorbell_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_doorbell_mutant.sv)
add_test(NAME field_doorbell_mutant_control COMMAND test_field_doorbell_mutant_control)
set_tests_properties(field_doorbell_mutant_control PROPERTIES LABELS "fast;mutant" TIMEOUT 300)

add_test(NAME lint_zhao_field_doorbell
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_doorbell
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_doorbell.sv)
set_tests_properties(lint_zhao_field_doorbell PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ==========================================================================
# PACKET D1 -- FH14 (op 3 decoded), FH13/FH15/FH16 (the FH2 loader), FT061.
# ==========================================================================
#
# FT060's NEGATIVE CONTROL. `zhao_field_doorbell_op3_alias_mutant.sv` restores
# the pre-FH14 catch-all LOAD arm; this driver runs FT060's own stimulus
# against it and PASSES WHEN THE OLD BEHAVIOUR APPEARS. Unlike its sibling
# above it is not a positive control for an unreachable counter -- op-3
# aliasing is reachable with an ordinary post -- it is the evidence that FT060
# would have NOTICED the defect it claims to catch (ruling R123).
add_executable(test_field_doorbell_op3_alias_control field/field_doorbell_op3_alias_control.cpp)
target_link_libraries(test_field_doorbell_op3_alias_control PRIVATE zhao_harness)
verilate(test_field_doorbell_op3_alias_control
  PREFIX Vzhao_field_doorbell_op3_alias_mutant
  TOP_MODULE zhao_field_doorbell_op3_alias_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv)
add_test(NAME field_doorbell_op3_alias_control COMMAND test_field_doorbell_op3_alias_control)
set_tests_properties(field_doorbell_op3_alias_control PROPERTIES LABELS "fast;mutant" TIMEOUT 300)

add_test(NAME lint_zhao_field_doorbell_op3_alias_mutant
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_doorbell_op3_alias_mutant
                 ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv)
set_tests_properties(lint_zhao_field_doorbell_op3_alias_mutant PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE FH2 TRANSACTIONAL LOADER -- owner decisions FH13/FH15/FH16, install FSM
# of directive section 10.3.
#
# The capsule bytes are produced by the PRODUCTION PACKER LIBRARY
# (`zfield::host_plan::serialize_program_image`), not hand-rolled in the test,
# and they enter the DUT only through the `zhao_hps_burst_req_t` bridge port --
# directive 20.5: "A testbench secretly writing the engine's private
# uop/table/scalar ports is not that path." Hence BOTH `zhao_harness` and
# `zhao_zref`, plus compiler/tests/generated for crater_ring.hpp.
add_executable(test_field_loader_directed field/field_loader_directed.cpp)
target_link_libraries(test_field_loader_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_field_loader_directed PRIVATE
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
verilate(test_field_loader_directed
  PREFIX Vtb_field_loader
  TOP_MODULE tb_field_loader
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_loader.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/field/tb_field_loader.sv)
add_test(NAME field_loader_directed COMMAND test_field_loader_directed)
set_tests_properties(field_loader_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# The loader's lint. `field_image_pkg_inherited.vlt` waives UNUSEDPARAM for
# S1's generated schema package ONLY -- see that file's header for why the
# waiver is narrow and for the measurement showing it is not a no-op.
add_test(NAME lint_zhao_field_loader
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_loader
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_loader.sv
                 ${CMAKE_CURRENT_SOURCE_DIR}/field/field_image_pkg_inherited.vlt)
set_tests_properties(lint_zhao_field_loader PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# FIELD.SEQ.FLOW's STREAM ADAPTER -- zhao_console_core entry I5, ruling R40.
#
# The engine is PLAYED here for the same reason the stamp adapter's is mocked:
# checking R40's mapping needs an exact velocity output, and getting one from
# the real engine would mean choosing a program -- which is the ABI decision
# entry I5 refused to make inside a composition. `zhao_part_record` is in the
# closure because the adapter INSTANTIATES the one codec rather than re-slicing
# the particle128 layout.
add_executable(test_field_flow_adapter_directed field/field_flow_adapter_directed.cpp)
target_link_libraries(test_field_flow_adapter_directed PRIVATE zhao_harness)
verilate(test_field_flow_adapter_directed
  PREFIX Vzhao_field_flow_adapter
  TOP_MODULE zhao_field_flow_adapter
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_flow_adapter.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
add_test(NAME field_flow_adapter_directed COMMAND test_field_flow_adapter_directed)
set_tests_properties(field_flow_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# --------------------------------------------------------------------------
# FIELD.SEQ.EARTH's STREAM ADAPTER -- the terrain height lane.
# zhao_console_core entry I34 build item (c), owner directive section 20.8.
# Driven STANDALONE with the field engine PLAYED, so a case can hand it an
# exact out-lane and check the answer word for word. Every one of its nine
# counters is FIRED here by legal stimulus, including all THREE arms of the
# lane shadow guard -- so no committed mutant is owed (R95). Arm (c) is the
# synchronous uniform bank's pairing check, added by packet EARTHRAM and fired
# by case 9d with the other two arms held silent so the count is attributable.
# --------------------------------------------------------------------------
add_executable(test_field_earth_adapter_directed field/field_earth_adapter_directed.cpp)
target_link_libraries(test_field_earth_adapter_directed PRIVATE zhao_harness)
verilate(test_field_earth_adapter_directed
  PREFIX Vzhao_field_earth_adapter
  TOP_MODULE zhao_field_earth_adapter
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_earth_adapter.sv)
add_test(NAME field_earth_adapter_directed COMMAND test_field_earth_adapter_directed)
set_tests_properties(field_earth_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# --------------------------------------------------------------------------
# THE POSITIVE CONTROL FOR EARTHLOCK'S INTAKE/REPLAY INTERLOCK, AND ITS
# POLARITY IS INVERTED: it PASSES WHEN THE RACE IS OBSERVED.
#
# Case 13 of the directed test above asserts the CORRECT behaviour -- that a
# replay's resident flag survives an in-flight intake, and that a vertex in
# that window is handed THIS frame's uniforms. That is the right way round and
# it keeps its meaning after the repair, but by construction it can no longer
# say whether the repair was ever needed. This target is the half that can.
#
# It elaborates `tests/mutants/zhao_field_earth_adapter_intake_race_mutant.sv`
# -- production with the repair reverted in five marked hunks and the module
# renamed so no source list can elaborate it by mistake. The SILENT half is
# what it exists for: a real engine run on the previous frame's age, phase and
# parameters with resident already set, moving NOT ONE COUNTER, which no
# instrument in the tree could see.
# --------------------------------------------------------------------------
add_executable(test_field_earth_adapter_intake_race_control
  mutants/field_earth_adapter_intake_race_control.cpp)
target_link_libraries(test_field_earth_adapter_intake_race_control PRIVATE zhao_harness)
verilate(test_field_earth_adapter_intake_race_control
  PREFIX Vzhao_field_earth_adapter_intake_race_mutant
  TOP_MODULE zhao_field_earth_adapter_intake_race_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_earth_adapter_intake_race_mutant.sv)
add_test(NAME field_earth_adapter_intake_race_control
  COMMAND test_field_earth_adapter_intake_race_control)
set_tests_properties(field_earth_adapter_intake_race_control
  PROPERTIES LABELS "fast;mutant" TIMEOUT 300)

# --------------------------------------------------------------------------
# THE CYCLE CENSUS. Packet EARTHRAM made the uniform bank's read SYNCHRONOUS,
# and the owner's instruction allowed "one more state on an ~80-100-cycle field
# run" for it. The measured cost is ZERO clocks, which is a surprising enough
# result to owe a committed probe rather than a paragraph -- CLAUDE.md's
# "commit the probe", applied to a latency instead of a ground contact.
#
# It asserts the CORRECT behaviour (accept-to-offer is one clock, and the
# payload is already valid on that clock), never the bug. A future change that
# inserts a state goes red here and names the number.
# --------------------------------------------------------------------------
add_executable(test_field_earth_adapter_cycle_census field/field_earth_adapter_cycle_census.cpp)
verilate(test_field_earth_adapter_cycle_census
  PREFIX Vzhao_field_earth_adapter
  TOP_MODULE zhao_field_earth_adapter
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_earth_adapter.sv)
add_test(NAME field_earth_adapter_cycle_census COMMAND test_field_earth_adapter_cycle_census)
set_tests_properties(field_earth_adapter_cycle_census PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_field_earth_adapter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_earth_adapter
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_earth_adapter.sv)
set_tests_properties(lint_zhao_field_earth_adapter PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_zhao_field_flow_adapter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_flow_adapter
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_flow_adapter.sv
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv)
set_tests_properties(lint_zhao_field_flow_adapter PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")
# --------------------------------------------------------------------------
# FIELD.SEQ.STAMP's STREAM ADAPTER -- the stencil walk.
#
# The engine is a MOCK here on purpose. This block computes nothing, so the
# only thing it can get wrong is ALIGNMENT with `zhao_surface_stamp`'s cursor,
# and a mock is what makes a FAULTING response reachable on cue. The mock is
# written to the shared engine's real protocol -- one request in flight, the
# answer HELD until taken -- because a mock that dropped an untaken answer
# would have hidden the restart defect this driver found.
#
# THE TOP IS A WRAPPER HOLDING BOTH NAMED BINDINGS, and that is structural
# rather than tidy. `STAMP_BINDING` is a PARAMETER, so a verilated top can only
# ever be ONE binding -- and owner directive 15.2 is explicit that "the legacy
# bridge is not evidence that the full canonical Stamp binding works". A single
# elaboration reporting "the stamp adapter passes" would be a green that is
# structurally unable to see half of what it claims. `tb_field_stamp_bindings`
# elaborates LEGACY_STAMP_BRUSH and CANONICAL_STAMP side by side with
# independent stimulus, so the driver can assert that exercising one leaves the
# other's counters at rest -- and can hand both the SAME response and show them
# delivering different records.
add_executable(test_field_stamp_adapter_directed field/field_stamp_adapter_directed.cpp)
target_link_libraries(test_field_stamp_adapter_directed PRIVATE zhao_harness)
verilate(test_field_stamp_adapter_directed
  PREFIX Vtb_field_stamp_bindings
  TOP_MODULE tb_field_stamp_bindings
  SOURCES ${CMAKE_SOURCE_DIR}/tests/field/tb_field_stamp_bindings.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_stamp_adapter.sv)
add_test(NAME field_stamp_adapter_directed COMMAND test_field_stamp_adapter_directed)
set_tests_properties(field_stamp_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 600)

add_test(NAME lint_zhao_field_stamp_adapter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_stamp_adapter
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_stamp_adapter.sv)
set_tests_properties(lint_zhao_field_stamp_adapter PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# FIELD.SEQ.WARP's STREAM ADAPTER -- GEOM.WARP prerequisite P2.
#
# The engine is a MOCK for the same reason as the two sibling adapters:
# checking a fifteen-lane binding needs an exact chosen response, and getting
# one from the real host would mean choosing a Warp program -- an ABI decision
# that does not belong in a test. `zhao_geom_warp.sv` does not exist at this
# commit, so the CONSUMER is modelled here too and the driver says so.
#
# The module elaborates at its OWN defaults (IN_LANES 15 / OUT_LANES 7), which
# is the pair the console does NOT yet compose. That is deliberate: the
# adapter's elaboration guard refuses the composed 13, so this test measures
# the configuration the block is for, and the composition gap stays visible in
# the completion register rather than being papered over by a narrow build.
add_executable(test_field_warp_adapter_directed field/field_warp_adapter_directed.cpp)
target_link_libraries(test_field_warp_adapter_directed PRIVATE zhao_harness)
verilate(test_field_warp_adapter_directed
  PREFIX Vzhao_field_warp_adapter
  TOP_MODULE zhao_field_warp_adapter
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv)
add_test(NAME field_warp_adapter_directed COMMAND test_field_warp_adapter_directed)
set_tests_properties(field_warp_adapter_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_field_warp_adapter
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_field_warp_adapter
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv)
set_tests_properties(lint_zhao_field_warp_adapter PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# --------------------------------------------------------------------------
# THE POSITIVE CONTROL for the stamp adapter's FH26 elaboration guard.
#
# THE POLARITY IS INVERTED: the program is SUPPOSED TO DIE. It elaborates a
# WRAPPER (not a copy -- nothing here goes stale) that instantiates the
# production adapter with `STAMP_BINDING` OMITTED, which FH26 requires to be
# refused. PASS_REGULAR_EXPRESSION makes the FATAL TEXT the pass condition, so
# a guard that silently stopped firing turns this red instead of green.
#
# It cannot be a lint: `verilator --lint-only` DOES NOT RUN `initial` BLOCKS,
# so linting the mutant returns success and says nothing about the guard --
# measured on this exact wrapper, 2026-09-20.
#
# VL_USER_FATAL: the control supplies its OWN `vl_fatal`. The runtime's
# default one fired the guard correctly and then HUNG AT ZERO CPU inside
# abort() on this toolchain (measured 2026-09-20, no WerFault dialog visible
# to a non-interactive session). A control that wedges the suite is not a
# passing control, and its timeout would have read as an infrastructure flake
# rather than as evidence.
add_executable(test_field_stamp_binding_unbound_control
  mutants/field_stamp_binding_unbound_control.cpp)
target_compile_definitions(test_field_stamp_binding_unbound_control PRIVATE
  VL_USER_FATAL VL_USER_STOP)
target_link_libraries(test_field_stamp_binding_unbound_control PRIVATE zhao_harness)
verilate(test_field_stamp_binding_unbound_control
  PREFIX Vtb_field_stamp_binding_unbound_mutant
  TOP_MODULE tb_field_stamp_binding_unbound_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/tb_field_stamp_binding_unbound_mutant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_stamp_adapter.sv)
add_test(NAME field_stamp_binding_unbound_control
         COMMAND test_field_stamp_binding_unbound_control)
set_tests_properties(field_stamp_binding_unbound_control PROPERTIES
  LABELS "fast;nightly"
  TIMEOUT 120
  PASS_REGULAR_EXPRESSION "STAMP_BINDING is UNBOUND")

# TERRAIN.HEIGHTTAP -- the INVERSE map. The console header says of it: "NOTHING
# IN THE TREE PERFORMS THE INVERSE. That is the missing half of a tap service,
# and it is a block rather than a wrapper."  This is that block, and the test is
# DIFFERENTIAL against `zref::terrain::column_query` itself rather than against a
# transcription of spec/terrain_rules.md 4.3 into the test -- which is why it
# links zhao_zref. A second implementation of a ratified law living in a test is
# the failure CLAUDE.md records for the terrain shade header.
#
# All six censuses are FIRED here with legal stimulus (void cell, off-patch, a
# lattice placed at the wrong pitch, an illegal pitch, an interpolation that
# does not fit s32, and the owner starving the tap of read cycles), so no
# committed mutant is owed. The overflow guard exists BECAUSE the explicit -Wall
# lint reported quot_c[63:32] unused -- verilate() does not pass -Wall.
# R1 (2026-09-19): the tap now emits normalize3_approx(face_normal) of the
# picked triangle, through THE one implementation of that law,
# zhao_field_v3_normalize, which brings its isqrt and its rcp24 seed ROM.
set(ZHAO_TERRAIN_HEIGHTTAP_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_heighttap.sv)
add_executable(test_terrain_heighttap_directed terrain/terrain_heighttap_directed.cpp)
target_link_libraries(test_terrain_heighttap_directed PRIVATE zhao_harness zhao_zref)
verilate(test_terrain_heighttap_directed
  PREFIX Vzhao_terrain_heighttap
  TOP_MODULE zhao_terrain_heighttap
  SOURCES ${ZHAO_TERRAIN_HEIGHTTAP_SV})
add_test(NAME terrain_heighttap_directed COMMAND test_terrain_heighttap_directed)
set_tests_properties(terrain_heighttap_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

add_test(NAME lint_zhao_terrain_heighttap
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_terrain_heighttap
                 ${ZHAO_TERRAIN_HEIGHTTAP_SV})
set_tests_properties(lint_zhao_terrain_heighttap PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# COMPOSEPUB -- A LIVE EARTH FIELD, ALL THE WAY TO A CONSUMER.
#
#   zhao_field_earth_adapter -> zhao_terrain_patch
#     -> zhao_terrain_compcache_front -> zhao_terrain_heighttap
#
# FOUR REAL BLOCKS, wired port-for-port as zhao_console_core.sv wires them.
#
# WHAT IT CLOSES. tb_terrain_compose.sv -- the nearest existing bench -- ties
# the field lane off in its own wiring (.fld_valid_i(1'b0), .fld_height_i(0),
# :443,445) and says so in its header: "The field list is EMPTY ... The field
# half needs FIELD.SEQ.EARTH." So terrain_rules section 3.4's
# `live_top = max(compose_top + SUM field lanes, fx(bottom))` had NEVER had a
# non-empty term in any bench of the real chain, and the composed height had
# never been shown to reach anything that reads it. Entry I34's note that the
# sum "now has a non-empty sum" describes arithmetic reaching a value; this is
# the gate that watches the value arrive at a consumer.
#
# THE CONSUMER IS REAL: zhao_terrain_heighttap is TERRAIN.TAPSHARE's single
# service and its two clients are PART.COLLIDE and FORGE.SHADOW.
#
# THE ORACLE IS zref::terrain::column_query, evaluated on a ComposedLattice
# built from the very words the hardware streamed into the compose cache
# (captured off st_top_o as they land, not recomputed). The separate assertion
# that st_top_o != st_compose_top_o is what says the field moved the lattice --
# split deliberately, because a tap-vs-reference differential alone passes
# perfectly on a lattice no field ever touched.
#
# Case 7 is the saturation evidence compose_rtl_directed.cpp commissioned when
# it proved "NOTHING SOURCED FROM A PAGE CAN SATURATE THIS ADD ... Only the
# FIELD lanes can ... a different lane's evidence". This is that lane.
set(ZHAO_TERRAIN_COMPOSEPUB_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_earth_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_patch.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_compcache_front.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_heighttap.sv
    ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_composepub.sv)
add_executable(test_composepub_acceptance terrain/composepub_acceptance.cpp)
target_link_libraries(test_composepub_acceptance PRIVATE zhao_harness zhao_zref)
verilate(test_composepub_acceptance
  PREFIX Vtb_terrain_composepub
  TOP_MODULE tb_terrain_composepub
  SOURCES ${ZHAO_TERRAIN_COMPOSEPUB_SV})
add_test(NAME composepub_acceptance COMMAND test_composepub_acceptance)
set_tests_properties(composepub_acceptance PROPERTIES LABELS "fast;nightly" TIMEOUT 1800)

# THE HEIGHTTAP OVERFLOW GUARD, POSITIVE CONTROL. `interp_overflow_o` is
# UNREACHABLE with legal stimulus -- the interpolation is a convex combination of
# three corner heights, so it cannot leave the interval they span, and the
# directed test tried with corners at both s32 rails and could not move it. This
# is the committed mutant CLAUDE.md requires for exactly that case: one shift
# removed (the divide by the cell width D), so the expression extrapolates and
# an ordinary relief lattice overshoots the rail. INVERTED POLARITY: the control
# PASSES when the counter FIRES. Evidence about the instrument, not the design.
add_executable(test_terrain_heighttap_mutant_control
               terrain/terrain_heighttap_mutant_control.cpp)
target_link_libraries(test_terrain_heighttap_mutant_control PRIVATE zhao_harness)
verilate(test_terrain_heighttap_mutant_control
  PREFIX Vzhao_terrain_heighttap_mutant
  TOP_MODULE zhao_terrain_heighttap_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_isqrt.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_v3_normalize.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_heighttap_mutant.sv)
add_test(NAME terrain_heighttap_mutant_control
         COMMAND test_terrain_heighttap_mutant_control)
set_tests_properties(terrain_heighttap_mutant_control PROPERTIES
  LABELS "fast;nightly" TIMEOUT 300)

# PART.TERRAIN_TAP -- console entry I6, owner ruling R1 (2026-09-19). The chain
# TERRAIN.HEIGHTTAP -> PART.TERRAIN_TAP -> PART.COLLIDE, all three real, with
# only the compose cache's read ports as a fixture. Every sample differenced
# against zref::terrain::column_query and ::collision_normal through the
# particle frame (qformats 10); a particle below the reference surface must
# leave PART.COLLIDE standing on it. All eleven censuses fired with legal
# stimulus, so no mutant is owed.
set(ZHAO_PART_TERRAIN_TAP_SV
    ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_record.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_terrain_tap.sv)
add_executable(test_part_terrain_tap_directed particles/part_terrain_tap_directed.cpp)
target_link_libraries(test_part_terrain_tap_directed PRIVATE zhao_harness zhao_zref)
verilate(test_part_terrain_tap_directed
  PREFIX Vtb_part_terrain_chain
  TOP_MODULE tb_part_terrain_chain
  SOURCES ${ZHAO_TERRAIN_HEIGHTTAP_SV}
          ${ZHAO_PART_TERRAIN_TAP_SV}
          ${CMAKE_SOURCE_DIR}/fpga/rtl/particles/zhao_part_collide.sv
          ${CMAKE_SOURCE_DIR}/tests/particles/tb_part_terrain_chain.sv)
add_test(NAME part_terrain_tap_directed COMMAND test_part_terrain_tap_directed)
set_tests_properties(part_terrain_tap_directed PROPERTIES
  LABELS "fast;particles" TIMEOUT 300)

add_test(NAME lint_zhao_part_terrain_tap
         COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
                 --top-module zhao_part_terrain_tap
                 ${ZHAO_PART_TERRAIN_TAP_SV})
set_tests_properties(lint_zhao_part_terrain_tap PROPERTIES
  LABELS "lint;fast"
  ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ===========================================================================
# GEOM.DRAWJOB -- one DrawForm becomes GEOM.MESHFETCH's jobs (owner ruling R29)
# ===========================================================================
# The resolver core entries I36 and I41 both named. It is differentiated
# against `zref::drawjob`, which is R29's FREEZE of the layout -- the test asks
# that header for the law rather than restating it. Every refusal ORDINAL is
# compared, not just the verdict: each reason is its own counter.
add_executable(test_geom_drawjob_directed geometry/geom_drawjob_directed.cpp)
target_link_libraries(test_geom_drawjob_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_drawjob_directed
  PREFIX Vzhao_geom_drawjob
  TOP_MODULE zhao_geom_drawjob
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_desc_crc.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_drawjob.sv)
add_test(NAME geom_drawjob_directed COMMAND test_geom_drawjob_directed)
set_tests_properties(geom_drawjob_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# -Wall, and warnings FATAL: `verilate()` does not pass -Wall.
add_test(NAME lint_geom_drawjob
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_drawjob
    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_abi_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_crc32c_fold.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_desc_crc.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_drawjob.sv)
set_tests_properties(lint_geom_drawjob PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# GEOM.WARP -- the REFERENCE half only (owner directive
# reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt, owner commit
# 4c256137; ratified in reports/OWNER-RATIFICATION-20260920-WARP.md).
#
# This was the directive's COMMIT A gate: "reference tests/decoder/program
# hash/map tests pass; NO RTL CLAIM YET".
#
# CORRECTED 2026-09-20 by packet W1: this comment used to end "there is
# deliberately no verilate() here, because there is no zhao_geom_warp.sv at
# this commit". THERE IS NOW -- fpga/rtl/geometry/zhao_geom_warp.sv -- and its
# RTL differential is registered below as geom_warp_rtl_directed. The block is
# still NOT composed into zhao_console_core; see FINDINGS-fieldw1.md for which
# prerequisites actually block that and which had already landed.
#
# It links zhao_zref only (the pattern of field_crater_ring / field_fplan_diff):
# every fixture is assembled as a real .zprog image and pushed through the REAL
# zfield::decode validator, so nothing here is asserted past the decoder.
add_executable(test_geom_warp_reference_directed geometry/geom_warp_reference_directed.cpp)
target_link_libraries(test_geom_warp_reference_directed PRIVATE zhao_zref)
add_test(NAME geom_warp_reference_directed COMMAND test_geom_warp_reference_directed)
set_tests_properties(geom_warp_reference_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# GEOM.WARP, THE RTL HALF. `zhao_geom_warp` differentially against
# `zref::geom_warp::apply_outputs` -- the SAME function the reference test uses,
# fed the HARDWARE's six output words. Nothing in the test restates the
# application law in C++, because a restatement is a second implementation and
# two implementations by the same hand agree on exactly the cases its author
# thought of. `apply_outputs` was split out of `apply` for this.
#
# The bench PLAYS the Field adapter. The real `zhao_field_warp_adapter` against
# the real `zhao_field_host_v2` is a separate bench, because W13 requires the
# application claim and the transport claim to stay apart.
add_executable(test_geom_warp_rtl_directed geometry/geom_warp_rtl_directed.cpp)
target_link_libraries(test_geom_warp_rtl_directed PRIVATE zhao_harness zhao_zref)
verilate(test_geom_warp_rtl_directed
  PREFIX Vzhao_geom_warp
  TOP_MODULE zhao_geom_warp
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv)
add_test(NAME geom_warp_rtl_directed COMMAND test_geom_warp_rtl_directed)
set_tests_properties(geom_warp_rtl_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 300)

# -Wall, and warnings FATAL: `verilate()` does not pass -Wall.
add_test(NAME lint_geom_warp
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_warp
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv)
set_tests_properties(lint_geom_warp PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# GEOM.WARP's NORMAL-SHIFT MUTANT. A COMMITTED POSITIVE CONTROL.
#
# `tests/mutants/zhao_geom_warp_shift_oneliner_mutant.sv` is a COPY of the
# production block, renamed so no source list can elaborate it by mistake, with
# exactly one substantive line changed: the range-reduction's second stage reads
# the ORIGINAL magnitude instead of the one the first stage produced.
#
# The fault is reachable by EXACTLY ONE 32-bit value, -2147483647. The driver's
# polarity is INVERTED -- it passes when the mutant is CAUGHT -- and it also
# measures a 20,000-triple RANDOM sweep, which must find NOTHING. Those two
# numbers together are the argument for geom_warp_rtl_directed FT096 naming the
# value explicitly rather than trusting a fuzzer. Ruling R123 is why the rate is
# measured at all: a mutant that fires on nothing is a green control attached to
# nothing.
#
# Its NEGATIVE CONTROL is the identical stimulus in geom_warp_rtl_directed
# FT096, where PRODUCTION agrees with the oracle. Neither run is evidence alone.
#
# NOTE THE SOURCE LIST: production `zhao_geom_warp.sv` is ABSENT and the mutant
# stands in its place. A build carrying both would elaborate whichever the
# top-module name selected and the other would sit there looking like coverage.
add_executable(test_geom_warp_shift_oneliner_mutant
  mutants/geom_warp_shift_oneliner_mutant_driver.cpp)
target_link_libraries(test_geom_warp_shift_oneliner_mutant PRIVATE zhao_harness zhao_zref)
verilate(test_geom_warp_shift_oneliner_mutant
  PREFIX Vzhao_geom_warp_shift_oneliner_mutant
  TOP_MODULE zhao_geom_warp_shift_oneliner_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_warp_shift_oneliner_mutant.sv)
add_test(NAME geom_warp_shift_oneliner_mutant COMMAND test_geom_warp_shift_oneliner_mutant)
set_tests_properties(geom_warp_shift_oneliner_mutant PROPERTIES
  LABELS "fast;nightly;mutant" TIMEOUT 300)

# ---------------------------------------------------------------------------
# GEOM.WARP AS A REAL CLIENT OF THE REAL SHARED FIELD FABRIC.
#
# `zhao_geom_warp` -> `zhao_field_warp_adapter` -> `zhao_field_host_v2`, all
# three REAL, joined in tests/field/tb_warp_field_chain.sv. Before this target,
# each of the three had a green directed test against a hand-built stand-in for
# its neighbour, and NOTHING in the tree instantiated any two of them together.
# Three suites can all be right about their own half and say nothing about the
# seam -- and the seam is where ordinal-versus-window lives.
#
# The differential is DOUBLE: each program is built once as a real .zprog pushed
# through the real zfield::decode and evaluated by zref::geom_warp::apply, and
# once as host loader words. So the comparison is software engine versus silicon
# engine on the same program, not hardware versus a C++ restatement.
#
# `zhao_field_warp_adapter.sv` is A1's file, carried here at blob e078caa6 from
# gz/fielda1 0e6b1a93 so this bench can build and run. Identical content, so a
# merge of both branches resolves trivially.
add_executable(test_warp_field_chain_directed field/warp_field_chain_directed.cpp)
target_link_libraries(test_warp_field_chain_directed PRIVATE zhao_harness zhao_zref)
verilate(test_warp_field_chain_directed
  PREFIX Vtb_warp_field_chain
  TOP_MODULE tb_warp_field_chain
  SOURCES
    ${ZHAO_FIELD_HOST_V2_SOURCES}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv
    ${CMAKE_SOURCE_DIR}/tests/field/tb_warp_field_chain.sv)
add_test(NAME warp_field_chain_directed COMMAND test_warp_field_chain_directed)
set_tests_properties(warp_field_chain_directed PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# -Wall on the chain. UNUSEDPARAM and UNUSEDSIGNAL are waived because they are
# INHERITED from the field closure's own modules, not produced by anything this
# packet wrote -- the single UNUSEDSIGNAL is zhao_field_host_v2.sv:987. The
# wrapper's own PINCONNECTEMPTY is waived INSIDE the wrapper with its reason
# beside it, so it cannot silently cover a production file, and `lint_geom_warp`
# still lints zhao_geom_warp.sv at full -Wall with no waiver at all.
add_test(NAME lint_warp_field_chain
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    -Wno-UNUSEDPARAM -Wno-UNUSEDSIGNAL
    --top-module tb_warp_field_chain
    ${ZHAO_FIELD_HOST_V2_SOURCES}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv
    ${CMAKE_SOURCE_DIR}/tests/field/tb_warp_field_chain.sv)
set_tests_properties(lint_warp_field_chain PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ---------------------------------------------------------------------------
# OWNER RULING R168 -- THE SPARSE ORDINAL DISCRIMINATOR, AND ITS CONTROL
# ---------------------------------------------------------------------------
# R168, adopting W1: `zhao_field_warp_adapter`'s host-side ports match the OLD
# `zhao_field_host` name-for-name and the NEW `zhao_field_host_v2` in MEANING.
# Wire it to the old host and it elaborates, runs and passes every gate while
# reading window positions as ordinals. R168 action 1, "and the first is not
# optional": the SPARSE case must be a NAMED test, not a case inside a bench.
#
# WHY A NAMED TARGET RATHER THAN LEAVING IT IN warp_field_chain_directed. That
# bench is a systems test with a dozen concerns; this is the ONE stimulus in
# the tree that separates a correct wiring from a wrong one. A discriminator
# buried among other cases is one refactor away from being dropped as
# redundant, and nothing would report the loss -- the remaining cases would all
# still pass, because they are contiguous and therefore blind.
add_executable(test_warp_sparse_ordinal_directed field/warp_sparse_ordinal_directed.cpp)
target_link_libraries(test_warp_sparse_ordinal_directed PRIVATE zhao_harness zhao_zref)
target_include_directories(test_warp_sparse_ordinal_directed
  PRIVATE ${CMAKE_SOURCE_DIR}/tests/field)
verilate(test_warp_sparse_ordinal_directed
  PREFIX Vtb_warp_field_chain
  TOP_MODULE tb_warp_field_chain
  SOURCES
    ${ZHAO_FIELD_HOST_V2_SOURCES}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv
    ${CMAKE_SOURCE_DIR}/tests/field/tb_warp_field_chain.sv)
add_test(NAME warp_sparse_ordinal_directed COMMAND test_warp_sparse_ordinal_directed)
set_tests_properties(warp_sparse_ordinal_directed
  PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# THE POSITIVE CONTROL, WITH ITS POLARITY INVERTED: it passes when the defect
# is DETECTED. The source list is the production one with the host SUBSTITUTED
# -- note that `zhao_field_host_v2.sv` is ABSENT from it, so the mutant cannot
# silently be measuring production. Everything else, `zhao_geom_warp` and
# `zhao_field_warp_adapter` included, is the REAL production file.
set(ZHAO_FIELD_HOST_V2_WINIDX_MUTANT_SOURCES ${ZHAO_FIELD_HOST_V2_SOURCES})
list(REMOVE_ITEM ZHAO_FIELD_HOST_V2_WINIDX_MUTANT_SOURCES
  ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_host_v2.sv)
list(APPEND ZHAO_FIELD_HOST_V2_WINIDX_MUTANT_SOURCES
  ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_field_host_v2_winidx_mutant.sv)

add_executable(test_warp_sparse_ordinal_winidx_mutant
  mutants/warp_sparse_ordinal_mutant_driver.cpp)
target_link_libraries(test_warp_sparse_ordinal_winidx_mutant
  PRIVATE zhao_harness zhao_zref)
target_include_directories(test_warp_sparse_ordinal_winidx_mutant
  PRIVATE ${CMAKE_SOURCE_DIR}/tests/field)
verilate(test_warp_sparse_ordinal_winidx_mutant
  PREFIX Vtb_warp_field_chain_winidx_mutant
  TOP_MODULE tb_warp_field_chain_winidx_mutant
  SOURCES
    ${ZHAO_FIELD_HOST_V2_WINIDX_MUTANT_SOURCES}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/tb_warp_field_chain_winidx_mutant.sv)
add_test(NAME warp_sparse_ordinal_winidx_mutant
         COMMAND test_warp_sparse_ordinal_winidx_mutant)
set_tests_properties(warp_sparse_ordinal_winidx_mutant
  PROPERTIES LABELS "fast;nightly" TIMEOUT 900)

# Same waivers as lint_warp_field_chain, and for the same reason: UNUSEDPARAM
# and UNUSEDSIGNAL are INHERITED from the field closure, not produced by the
# mutation.
add_test(NAME lint_warp_field_chain_winidx_mutant
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    -Wno-UNUSEDPARAM -Wno-UNUSEDSIGNAL
    --top-module tb_warp_field_chain_winidx_mutant
    ${ZHAO_FIELD_HOST_V2_WINIDX_MUTANT_SOURCES}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_warp_adapter.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_warp.sv
    ${CMAKE_SOURCE_DIR}/tests/mutants/tb_warp_field_chain_winidx_mutant.sv)
set_tests_properties(lint_warp_field_chain_winidx_mutant PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ===========================================================================
# TERRAIN.HEIGHTTAP's ARBITER -- zhao_terrain_tapshare
# ===========================================================================
# The tap has exactly ONE requester port and PART.TERRAIN_TAP already holds
# every wire of it, so FORGE.SHADOW's `tap_*` -- which the contract had marked
# the one clear port group in that subsystem -- needs an arbiter in front. The
# service's response carries NO tag and NO rider, so the arbiter holds the
# owner itself; `terrain_tapshare_directed` is what says it holds the RIGHT one
# across a multi-cycle walk while another client is asking.
add_executable(test_terrain_tapshare_directed terrain/terrain_tapshare_directed.cpp)
target_link_libraries(test_terrain_tapshare_directed PRIVATE zhao_harness)
verilate(test_terrain_tapshare_directed
  PREFIX Vtb_terrain_tapshare
  TOP_MODULE tb_terrain_tapshare
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tapshare.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/terrain/tb_terrain_tapshare.sv)
add_test(NAME terrain_tapshare_directed COMMAND test_terrain_tapshare_directed)
set_tests_properties(terrain_tapshare_directed PROPERTIES LABELS "fast;terrain" TIMEOUT 300)

# ===========================================================================
# TERRAIN.FIELDLIST -- zhao_terrain_fieldlist
# ===========================================================================
# The sealed per-FRAME TerrainField association list and its per-PATCH replay,
# composed 2026-09-22 (FIELDARM) to close the section 9.1 half of core entry
# I34. What this test is FOR, in one line: zhao_cmd_exec publishes TerrainField
# records once per command PACKET and zhao_terrain_patch clears its list once
# per patch JOB, so the fault a pass-through would ship is "the first patch of
# the frame gets the list and no other one does" -- silently, with every
# counter on both blocks balancing. Every replay case here therefore pulses
# patch_open_i more than once and requires the same records both times.
add_executable(test_terrain_fieldlist_directed terrain/terrain_fieldlist_directed.cpp)
target_link_libraries(test_terrain_fieldlist_directed PRIVATE zhao_harness)
verilate(test_terrain_fieldlist_directed
  PREFIX Vzhao_terrain_fieldlist
  TOP_MODULE zhao_terrain_fieldlist
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_fieldlist.sv)
add_test(NAME terrain_fieldlist_directed COMMAND test_terrain_fieldlist_directed)
set_tests_properties(terrain_fieldlist_directed PROPERTIES LABELS "fast;terrain" TIMEOUT 300)

# ==========================================================================
# THE FORGE CHAIN, composed 2026-09-21 (packet FORGECOMP)
# ==========================================================================
# `zhao_forge_assemble` is the JOIN the chain never had: zhao_forge_prim emits
# index triples and no coordinate, the two evaluators emit world positions and
# no topology, and `zref_forge_page.hpp` names the ring-major ORDERING
# CONVENTION as the only thing relating them. Nothing held the vertices an
# index could name.
#
# The bench carries a FAKE PROJECTOR rather than the real one, and the fake
# reproduces the three properties the assembler is built against: the result
# port has NO READY, the rider comes back verbatim, and the order is FIFO. Its
# arithmetic is INVERTIBLE on purpose, so the test can assert that corner B
# carries vertex i1's numbers -- a fake returning a constant would let a
# completely broken join pass every check.
add_executable(test_forge_assemble_directed forge/forge_assemble_directed.cpp)
target_link_libraries(test_forge_assemble_directed PRIVATE zhao_harness)
verilate(test_forge_assemble_directed
  PREFIX Vtb_forge_assemble
  TOP_MODULE tb_forge_assemble
  # The reciprocal's own closure, copied from geom_vattr_rtl_directed's list
  # rather than re-derived: this block instantiates the SAME pair GEOM.VATTR
  # does, which is ruling D-4's canonical law used twice, so it owes the same
  # sources. A shorter list here would mean the two are not the same pair.
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
          ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_assemble.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/forge/tb_forge_assemble.sv)
add_test(NAME forge_assemble_directed COMMAND test_forge_assemble_directed)
set_tests_properties(forge_assemble_directed PROPERTIES LABELS "fast;forge" TIMEOUT 300)

add_test(NAME lint_zhao_forge_assemble
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_forge_assemble
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/geometry
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/raster
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/field
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_depthquant.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_mul.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_ticketq_rh.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/field/zhao_field_rcp24_rom.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v4.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_assemble.sv)
set_tests_properties(lint_zhao_forge_assemble PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# ${ZHAO_ABI_PKG} IS REQUIRED AND WAS MISSING. `zhao_pkg.sv` line 35 is
# `import zhao_abi_pkg::*;`, so without it this gate has failed with "Import
# package not found" SINCE THE DAY IT WAS REGISTERED -- a lint target that has
# never once passed, which is a broken instrument rather than a gate. Found by
# running it (PROCMAT, 2026-09-22); reproduced at the base commit before being
# called pre-existing.
add_test(NAME lint_zhao_forge_pagebank
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_forge_pagebank
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/common
    -I${CMAKE_SOURCE_DIR}/fpga/rtl/generated
    ${ZHAO_ABI_PKG}
    ${CMAKE_SOURCE_DIR}/fpga/rtl/common/zhao_pkg.sv
    ${CMAKE_SOURCE_DIR}/fpga/rtl/forge/zhao_forge_pagebank.sv)
set_tests_properties(lint_zhao_forge_pagebank PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

add_test(NAME lint_zhao_terrain_tapshare
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_terrain_tapshare
    ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tapshare.sv)
set_tests_properties(lint_zhao_terrain_tapshare PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE POSITIVE CONTROL, POLARITY INVERTED: it passes when `stray_rsp_o` FIRES.
# That counter is unreachable with legal stimulus while the owner latch is
# correct, so its silence in the directed test is a NEGATIVE control and this
# is the other half. Evidence about the instrument, not about the design.
add_executable(test_terrain_tapshare_mutant_control terrain/terrain_tapshare_mutant_control.cpp)
target_link_libraries(test_terrain_tapshare_mutant_control PRIVATE zhao_harness)
verilate(test_terrain_tapshare_mutant_control
  PREFIX Vtb_terrain_tapshare_mutant
  TOP_MODULE tb_terrain_tapshare_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_terrain_tapshare_mutant.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/tb_terrain_tapshare_mutant.sv)
add_test(NAME terrain_tapshare_mutant_control
         COMMAND test_terrain_tapshare_mutant_control)
set_tests_properties(terrain_tapshare_mutant_control
  PROPERTIES LABELS "fast;terrain" TIMEOUT 300)

# ---------------------------------------------------------------------------
# GEOM.CLIP's INPUT DOOR -- zhao_geom_clipdoor (packet CLIPDOOR, 2026-09-21)
# ---------------------------------------------------------------------------
# Owner ruling R187 named GEOM.CLIP's input as "the honest door" for every
# non-mesh producer. The door carries the MATERIAL half (into
# zhao_material_window) and the TRIANGLE half (into zhao_geom_clip) of ONE beat,
# because the window is a pure combinational gate between them -- so one arbiter
# owns both and splitting them would be the metadata-swap shape.
#
# The check a plain arbiter test would not make is section 3: with both clients
# offering forever the grant must NOT alternate, because the material window
# DRAINS the whole span on every material change.
add_executable(test_geom_clipdoor_directed geometry/geom_clipdoor_directed.cpp)
target_link_libraries(test_geom_clipdoor_directed PRIVATE zhao_harness)
verilate(test_geom_clipdoor_directed
  PREFIX Vtb_geom_clipdoor
  TOP_MODULE tb_geom_clipdoor
  SOURCES ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipdoor.sv
          ${CMAKE_CURRENT_SOURCE_DIR}/geometry/tb_geom_clipdoor.sv)
add_test(NAME geom_clipdoor_directed COMMAND test_geom_clipdoor_directed)
set_tests_properties(geom_clipdoor_directed PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

add_test(NAME lint_zhao_geom_clipdoor
  COMMAND "${ZHAO_VERILATOR_LINT}" --lint-only -Wall
    --top-module zhao_geom_clipdoor
    ${CMAKE_SOURCE_DIR}/fpga/rtl/geometry/zhao_geom_clipdoor.sv)
set_tests_properties(lint_zhao_geom_clipdoor PROPERTIES
  LABELS "lint;fast" ENVIRONMENT "VERILATOR_ROOT=${VERILATOR_ROOT}")

# THE POSITIVE CONTROL, POLARITY INVERTED: it passes when `err_hold_broken_o`
# FIRES. That counter is unreachable with legal stimulus while the hold law is
# correct -- `take_new_c` requires `!owner_offering_c` and `o_valid_o` IS
# `owner_offering_c` -- so its silence in the directed test is a NEGATIVE
# control and this is the other half. Evidence about the instrument, not about
# the design.
add_executable(test_geom_clipdoor_mutant_control geometry/geom_clipdoor_mutant_control.cpp)
target_link_libraries(test_geom_clipdoor_mutant_control PRIVATE zhao_harness)
verilate(test_geom_clipdoor_mutant_control
  PREFIX Vtb_geom_clipdoor_mutant
  TOP_MODULE tb_geom_clipdoor_mutant
  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_geom_clipdoor_mutant.sv
          ${CMAKE_SOURCE_DIR}/tests/mutants/tb_geom_clipdoor_mutant.sv)
add_test(NAME geom_clipdoor_mutant_control
         COMMAND test_geom_clipdoor_mutant_control)
set_tests_properties(geom_clipdoor_mutant_control
  PROPERTIES LABELS "fast;geometry" TIMEOUT 300)

# ---------------------------------------------------------------------------
# THE BL8 BLOCK CLAMP'S POSITIVE CONTROL AND ITS NEGATIVE CONTROL
# (owner ruling R243 / D-SDRAM-A).
#
# `sim/models/zhao_sdram_model.sv` now wraps like the part -- a JEDEC BL8
# SEQUENTIAL burst holds col[10:3] and advances col[2:0] -- and
# `zhao_vram_arbiter.burst_words` now clamps to the aligned eight-column block,
# so NO burst the arbiter offers can reach that wrap. The repair is correct and
# it makes the wrap UNREACHABLE WITH LEGAL STIMULUS, which is exactly the state
# CLAUDE.md says owes a COMMITTED MUTANT: a repair nobody has watched fail is
# indistinguishable from a repair to nothing.
#
# `tests/mutants/zhao_vram_arbiter_blkalign_mutant.sv` is production with ONE
# substantive change -- `burst_words` reverted to the ROW clamp it carried
# before R243 -- renamed so no production source list can elaborate it.
#
# ONE source, TWO executables, opposite expectations. The `silent` half asserts
# the CORRECT behaviour (a misaligned write lands word for word) rather than
# the defect, so it survives the repair instead of being deleted by it.
# ---------------------------------------------------------------------------
foreach(ba_variant fires silent)
  set(ba_target test_mem_blkalign_mut_${ba_variant})
  add_executable(${ba_target} memory/mem_blkalign_mut.cpp)
  target_link_libraries(${ba_target} PRIVATE zhao_harness zhao_zref)
  set(ba_sources ${ZHAO_MEM_PKGS}
                 ${ZHAO_MEM_RTL}/zhao_sdram_ctrl.sv)
  if(ba_variant STREQUAL "fires")
    # The macro reaches BOTH the source list and the C++ expectation, so the
    # driver cannot end up compiled against one arbiter while asserting the
    # other's behaviour.
    target_compile_definitions(${ba_target} PRIVATE ZHAO_BLKALIGN_MUT)
    set(ba_vargs -DZHAO_BLKALIGN_MUT)
    list(APPEND ba_sources
         ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_vram_arbiter_blkalign_mutant.sv)
  else()
    set(ba_vargs "")
    list(APPEND ba_sources ${ZHAO_MEM_RTL}/zhao_vram_arbiter.sv)
  endif()
  list(APPEND ba_sources
       ${CMAKE_SOURCE_DIR}/sim/models/zhao_sdram_model.sv
       ${CMAKE_CURRENT_SOURCE_DIR}/memory/tb_zhao_mem_chain.sv)
  verilate(${ba_target}
    PREFIX Vtb_zhao_mem_chain TOP_MODULE tb_zhao_mem_chain
    VERILATOR_ARGS ${ba_vargs}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/fpga/rtl/common
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/generated
                 ${CMAKE_SOURCE_DIR}/fpga/rtl/memory
                 ${CMAKE_SOURCE_DIR}/sim/models
    SOURCES ${ba_sources})
  add_test(NAME mem_blkalign_mut_${ba_variant} COMMAND ${ba_target})
  set_tests_properties(mem_blkalign_mut_${ba_variant}
    PROPERTIES LABELS "fast;nightly" TIMEOUT 300)
endforeach()
