// zhao_sim.hpp — shared Verilator harness helpers (charter 20.3).
//
// Header-only template part (clock/eval loop, ready/valid stimulus) +
// model-independent helpers linked from zhao_sim.cpp (bit-exact compare
// registry, failing-vector serializer per charter 20.3/29-17).
//
// Machine pins (P1 findings, verified 2026-08-14):
//   - `double sc_time_stamp()` shim lives in zhao_sim.cpp: verilated.cpp of
//     this Verilator 5.051 devel build references it (P1 gotcha 6).
//   - C++17 on every TU including verilated.cpp (P1 gotcha 7); set at the
//     top-level CMakeLists.
//
// W1 scope: generic enough for the stub-top test; W3-W6 tests reuse it.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace zhao {

// ---------------------------------------------------------------- checks --
// Model-independent bit-exact compare registry. Tests accumulate failures
// via zhao_check(); main() finishes with zhao_report_and_exit().
int  check_failures();
void check(bool cond, const char* what, uint64_t expected, uint64_t actual);
int  report_and_exit(const char* suite_name);  // prints summary, returns exit code

// ------------------------------------------------------- failing vectors --
// Charter 29-17: every minimal failing vector is saved, not just printed.
// Writes captures/failures/<name>.txt with the charter 20.3 shape:
//   input bytes (hex), expected, actual. Creates the directory if needed.
void save_failing_vector(const std::string& name,
                         const std::vector<uint8_t>& input,
                         const std::string& expected,
                         const std::string& actual);

// ------------------------------------------------- clock / eval loop -----
// One full clock cycle: settle low, rising edge (sequential logic fires),
// settle low. Inputs must be set before tick(); they are sampled at the
// rising edge like real hardware.
template <typename Top>
inline void tick(Top& top) {
  top.clk = 0;
  top.eval();
  top.clk = 1;
  top.eval();
  top.clk = 0;
  top.eval();
}

// Async-reset (negedge rst_n) for `cycles` ticks, then release.
template <typename Top>
inline void reset(Top& top, int cycles = 2) {
  top.rst_n = 0;
  top.in_valid = 0;
  top.in_data = 0;
  top.eval();
  for (int i = 0; i < cycles; ++i) {
    tick(top);
  }
  top.rst_n = 1;
  top.eval();
  tick(top);  // one clean post-reset cycle
}

// ------------------------------------------- ready/valid byte stimulus ---
// Offer `byte` with valid=1, holding data stable until the model accepts it
// (true ready/valid). Returns false if the model stalls ready longer than
// max_wait cycles (a hang, not a backpressure failure).
template <typename Top>
inline bool send_byte(Top& top, uint8_t byte, int max_wait = 10000) {
  top.clk = 0;
  top.in_data = byte;
  top.in_valid = 1;
  top.eval();
  int waited = 0;
  while (!top.in_ready) {
    tick(top);
    if (++waited >= max_wait) {
      top.in_valid = 0;
      return false;
    }
  }
  tick(top);  // rising edge consumes the byte
  return true;
}

// Deassert valid and idle for `cycles` ticks.
template <typename Top>
inline void idle(Top& top, int cycles = 1) {
  top.in_valid = 0;
  for (int i = 0; i < cycles; ++i) {
    tick(top);
  }
}

}  // namespace zhao
