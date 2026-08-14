cmake_minimum_required(VERSION 3.20)

project(zhaozhou
  VERSION 0.1.0
  DESCRIPTION "Zhaozhou FPGA game console - reference models, Verilator harness, tests"
  LANGUAGES CXX)

# Charter law + P1 findings: C++17 everywhere, no exceptions.
# g++ 16 ABI link errors if any TU (including verilated.cpp) drops below C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static C++ runtime (W4): test executables must run under ANY PATH order.
# The oss-cad-suite lib dir carries an msys2 libstdc++-6.dll that shadows the
# winlibs one and breaks entry points (0xc0000139) when it wins resolution.
add_link_options(-static-libgcc -static-libstdc++)

# Verilator integration. Requires VERILATOR_ROOT = <oss-cad-suite>/share/verilator
# (tools/env/zhao-env.ps1|.bat sets this up; see cmake/zhao_verilate.cmake).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/zhao_verilate.cmake)

enable_testing()

add_subdirectory(reference)  # ZRef C++ library (lands in W3/W6)
add_subdirectory(tools)      # native tool helpers (later waves)
add_subdirectory(tests)      # CTest: labels fast / nightly / formal / lint
