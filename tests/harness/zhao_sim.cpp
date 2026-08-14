// zhao_sim.cpp — model-independent harness helpers (see zhao_sim.hpp).
// Also hosts the sc_time_stamp() shim (P1 gotcha 6: verilated.cpp of this
// Verilator 5.051 devel build references it when linked into our mains).

#include "zhao_sim.hpp"

#include <filesystem>
#include <fstream>

// --- sc_time_stamp shim ---------------------------------------------------
// Required by verilated.cpp in this build (oss-cad-suite 20260814 /
// Verilator 5.051 devel). We simulate without SystemC time: constant 0.
double sc_time_stamp() { return 0.0; }

namespace zhao {

// --- compare registry -----------------------------------------------------
namespace {
int g_failures = 0;
int g_checks = 0;
}  // namespace

int check_failures() { return g_failures; }

void check(bool cond, const char* what, uint64_t expected, uint64_t actual) {
  ++g_checks;
  if (cond) {
    return;
  }
  ++g_failures;
  std::printf("FAIL: %s: expected 0x%llX, got 0x%llX\n",
              what,
              static_cast<unsigned long long>(expected),
              static_cast<unsigned long long>(actual));
}

int report_and_exit(const char* suite_name) {
  if (g_failures == 0) {
    std::printf("[%s] %d checks passed\n", suite_name, g_checks);
    return 0;
  }
  std::printf("[%s] %d/%d checks FAILED\n", suite_name, g_failures, g_checks);
  return 1;
}

// --- failing-vector serializer (charter 20.3 / 29-17 shape) ----------------
void save_failing_vector(const std::string& name,
                         const std::vector<uint8_t>& input,
                         const std::string& expected,
                         const std::string& actual) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path repo_root = fs::current_path();  // tests run from build/; walk up
  for (int i = 0; i < 3 && !fs::exists(repo_root / "package.json"); ++i) {
    repo_root = repo_root.parent_path();
  }
  fs::path dir = repo_root / "captures" / "failures";
  fs::create_directories(dir, ec);
  fs::path file = dir / (name + ".txt");

  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << "# zhaozhou failing vector (charter 20.3)\n";
  out << "name: " << name << "\n";
  out << "input_bytes: " << input.size() << "\n";
  out << "input_hex:";
  for (uint8_t b : input) {
    char hex[5];
    std::snprintf(hex, sizeof(hex), " %02X", b);
    out << hex;
  }
  out << "\nexpected: " << expected << "\n";
  out << "actual: " << actual << "\n";
}

}  // namespace zhao
