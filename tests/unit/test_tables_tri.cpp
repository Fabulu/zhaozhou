// test_tables_tri.cpp — tri-language table byte-identity (plan W3, R11).
//
// Asserts that the ONE frozen table set is numerically identical across:
//   1. the C++ constexpr header  reference/include/zref/generated/zref_tables.hpp
//   2. the SV $readmemh files    fpga/rtl/generated/tables/*.mem
//   3. the TS const module       compiler/src/generated/tables.ts
// (spec/qformats.md 11: same hex digit strings in all three languages.)
//
// The C++ side compares against the files on disk; `npm run tables:check`
// separately proves those files equal a fresh generator run, closing the
// loop: generator == disk == compiled-in constants.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "zref/zref_rcp.hpp"  // pulls in generated/zref_tables.hpp

using namespace zref;

static int g_failures = 0;

static void fail(const std::string& msg) {
  ++g_failures;
  std::printf("FAIL %s\n", msg.c_str());
}

static std::string read_text(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::printf("FATAL: cannot open %s\n", path.c_str());
    std::exit(2);
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Parse whitespace/comma-separated hex words (0x optional) from a text blob.
static std::vector<uint32_t> parse_hex_words(const std::string& text) {
  std::vector<uint32_t> out;
  std::string tok;
  for (char c : text) {
    if (c == ',' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      if (!tok.empty()) {
        out.push_back((uint32_t)std::strtoull(tok.c_str(), nullptr, 16));
        tok.clear();
      }
    } else if (c == 'x' || c == 'X') {
      if (tok == "0") tok = "";  // strip 0x prefix
    } else {
      tok += c;
    }
  }
  if (!tok.empty()) out.push_back((uint32_t)std::strtoull(tok.c_str(), nullptr, 16));
  return out;
}

// Extract the hex word sequence following "NAME ... = [" in a generated TS file.
// (NB: search for "= [" — the type annotation "number[]" also contains brackets.)
static std::vector<uint32_t> array_after(const std::string& text, const std::string& name) {
  const size_t at = text.find(name);
  if (at == std::string::npos) {
    fail("marker " + name + " not found");
    return {};
  }
  const size_t open = text.find("= [", at);
  if (open == std::string::npos) {
    fail("array open after " + name + " not found");
    return {};
  }
  const size_t close = text.find(']', open);
  return parse_hex_words(text.substr(open + 3, close - open - 3));
}

static void check_set(const char* label, const uint32_t* cpp_arr, size_t n,
                      const std::vector<uint32_t>& sv, const std::vector<uint32_t>& ts,
                      int hex_width) {
  if (sv.size() != n)
    fail(std::string(label) + ": SV mem has " + std::to_string(sv.size()) + " words, C++ has " +
         std::to_string(n));
  if (ts.size() != n)
    fail(std::string(label) + ": TS has " + std::to_string(ts.size()) + " words, C++ has " +
         std::to_string(n));
  for (size_t i = 0; i < n; ++i) {
    if (sv[i] != cpp_arr[i]) {
      fail(std::string(label) + ": SV[" + std::to_string(i) + "]=" + std::to_string(sv[i]) +
           " != C++ " + std::to_string(cpp_arr[i]));
    }
    if (ts[i] != cpp_arr[i]) {
      fail(std::string(label) + ": TS[" + std::to_string(i) + "]=" + std::to_string(ts[i]) +
           " != C++ " + std::to_string(cpp_arr[i]));
    }
  }
  if (g_failures == 0) {
    std::printf("  %s: C++ == SV .mem == TS const (%zu words, %d hex digits) OK\n", label, n,
                hex_width);
  }
}

int main() {
  const std::string root = ZHAO_SOURCE_DIR;
  const std::string sv_sin = read_text(root + "/fpga/rtl/generated/tables/sin_q16.mem");
  const std::string sv_rcp = read_text(root + "/fpga/rtl/generated/tables/rcp24_t0.mem");
  const std::string sv_frcp = read_text(root + "/fpga/rtl/generated/tables/field_rcp_t0.mem");
  const std::string ts = read_text(root + "/compiler/src/generated/tables.ts");

  // SV mem: whole file is the word list.
  // TS: extract each array body (excludes QFMT_VERSION scalar).
  check_set("SIN_Q16", gen::SIN_Q16, 257, parse_hex_words(sv_sin), array_after(ts, "SIN_Q16"), 5);
  check_set("RCP24_T0", gen::RCP24_T0, 256, parse_hex_words(sv_rcp), array_after(ts, "RCP24_T0"),
            8);
  check_set("FIELD_RCP_T0", gen::FIELD_RCP_T0, 256, parse_hex_words(sv_frcp),
            array_after(ts, "FIELD_RCP_T0"), 5);

  // TS must carry the same QFMT_VERSION as the C++ header and the spec.
  if (ts.find("QFMT_VERSION = 1") == std::string::npos) {
    fail("tables.ts QFMT_VERSION marker missing");
  }

  std::printf("test_tables_tri: %s\n", g_failures == 0 ? "OK" : "FAILURES");
  return g_failures == 0 ? 0 : 1;
}
