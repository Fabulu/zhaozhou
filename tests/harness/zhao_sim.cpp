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
  std::printf("FAIL: %s: expected 0x%llX, got 0x%llX\n", what,
              static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
}

int report_and_exit(const char* suite_name) {
  int rc;
  if (g_failures == 0) {
    std::printf("[%s] %d checks passed\n", suite_name, g_checks);
    rc = 0;
  } else {
    std::printf("[%s] %d/%d checks FAILED\n", suite_name, g_failures, g_checks);
    rc = 1;
  }
  std::fflush(nullptr);
  // Terminate without static destructors (P-machine finding, 2026-08-15):
  // Verilator 5.051 + winlibs libwinpthread intermittently deadlocks in
  // VlThreadPool::~VlThreadPool() during exit-time static destruction of
  // the default VerilatedContext — observed on every Verilated exe on this
  // toolchain, including pre-existing test_stub_top (hang with ~0 CPU,
  // WaitForSingleObject in the pool dtor). Every observable side effect
  // (stdout/stderr above, failing-vector files, which are closed inside
  // save_failing_vector) is flushed before this point, so skipping static
  // teardown is safe and makes process exit deterministic.
  std::_Exit(rc);
}

// --- failing-vector serializer (charter 20.3 / 29-17 shape) ----------------
void save_failing_vector(const std::string& name, const std::vector<uint8_t>& input,
                         const std::string& expected, const std::string& actual) {
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
