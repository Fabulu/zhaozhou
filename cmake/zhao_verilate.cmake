# zhao_verilate.cmake — Verilator integration for the Zhaozhou build.
#
# Ported from the P1-verified example (RUN-20260814-1852-wave1-build-skeleton
# scratch/vtest/, verified 2026-08-14 with oss-cad-suite 20260814 /
# Verilator 5.051 devel + winlibs cmake 4.3.2 / g++ 16.1.0).
#
# Machine rules encoded here (P1 findings 3-4, all reproduced):
#   - VERILATOR_ROOT must be <oss-cad-suite>/share/verilator (the dir that
#     contains include/ and verilator-config.cmake); the suite root fails with
#     "Cannot find verilated_std.sv".
#   - the executable is verilator_bin(.exe) — the perl wrapper `verilator`
#     has no bundled perl on Windows.
#   - no Verilator-internal --build: CMake compiles the generated C++ itself
#     (this is verilate()'s default behaviour).
#   - no spaces anywhere in the build path (verilated.mk hard-fails).
#   - C++17 on every TU including verilated.cpp (set in the root CMakeLists).
#
# After this file is included, verilate() is available for targets.

find_package(verilator HINTS "$ENV{VERILATOR_ROOT}")

if(NOT verilator_FOUND)
  # Fallback: find verilator_bin on PATH, derive VERILATOR_ROOT from the
  # standard suite layout (<suite>/bin beside <suite>/share/verilator).
  find_program(ZHAO_VERILATOR_BIN NAMES verilator_bin verilator_bin.exe)
  if(ZHAO_VERILATOR_BIN)
    get_filename_component(_zhao_bin_dir "${ZHAO_VERILATOR_BIN}" DIRECTORY)
    set(_zhao_derived_root "${_zhao_bin_dir}/../share/verilator")
    if(EXISTS "${_zhao_derived_root}/verilator-config.cmake")
      message(STATUS "zhao: derived VERILATOR_ROOT=${_zhao_derived_root}")
      set(ENV{VERILATOR_ROOT} "${_zhao_derived_root}")
      find_package(verilator HINTS "${_zhao_derived_root}")
    endif()
  endif()
endif()

if(NOT verilator_FOUND)
  message(FATAL_ERROR
    "Verilator not found. Source tools/env/zhao-env.ps1 (or zhao-env.bat) first: "
    "it sets VERILATOR_ROOT=<oss-cad-suite>/share/verilator and puts the suite "
    "bin/ + lib/ on PATH.")
endif()

# The suite's verilator-config.cmake only exports verilator_FOUND + VERILATOR_ROOT
# (it locates verilator_bin internally). Report the kit root as the status pin.
message(STATUS "zhao: Verilator kit found (VERILATOR_ROOT=${VERILATOR_ROOT})")
