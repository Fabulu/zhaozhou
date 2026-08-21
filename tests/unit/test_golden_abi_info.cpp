// test_golden_abi_info.cpp — every committed .zcap golden must carry the
// CURRENT ABI constants.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// `captures/golden/wave2/duo_markers.zcap` was regenerated on 2026-08-18 in the
// same commit that bumped `ZHAO_ABI_VERSION` from 2 to 3 — and it came out
// carrying version 2 and the two PREVIOUS SHA-256s. The regeneration ran
// against a build that had not picked up the new header. The golden and the
// generated ABI have disagreed ever since.
//
// Nothing caught it for three days, and the reason is the part worth fixing:
// the only test that compares that file is `shell_duo_markers`, which is
// labelled `gate;nightly` and takes **twenty-two minutes**. A check that
// expensive is a check that runs rarely, and one that runs rarely is one whose
// failures age.
//
// The ABI-info section of a capture is built entirely from compile-time
// constants — `ZHAO_ABI_VERSION`, `ZHAO_ZCAP_SCHEMA_VERSION`, the generator
// name, and two SHA-256 digests. Nothing about the simulation can influence it.
// So it can be checked in a millisecond by decoding the committed file, and
// this test does exactly that, in the FAST lane.
//
// It does not replace the full byte-identity gate. It separates the one failure
// mode that has nothing to do with the hardware from the many that do — so a
// stale constant is reported in a second and never again hides behind a
// twenty-two minute run.

#include "zhao_abi.h"  // generated

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using zhao::check;

fs::path repo_root() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 5 && !fs::exists(dir / "spec" / "commands.zidl"); i++) {
    dir = dir.parent_path();
  }
  return dir;
}

std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

uint32_t rd32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

/**
 * Find the ABI-info section body by scanning for the generator name.
 *
 * A full section walk would be better, but it would also duplicate the reader
 * that already exists; what is needed here is a check that CANNOT be satisfied
 * by the same mistake twice. The generator name sits at body+8 and is a
 * distinctive 16-byte string, so locating it locates the body — and if the
 * layout ever moves, this test fails loudly rather than silently passing.
 */
bool find_abi_info(const std::vector<uint8_t>& f, size_t* body_off) {
  const std::string name(zhao_abi::ZHAO_GENERATOR_NAME);
  if (name.empty() || f.size() < 88) return false;
  for (size_t i = 0; i + name.size() <= f.size(); ++i) {
    if (std::memcmp(f.data() + i, name.data(), name.size()) == 0) {
      if (i < 8) continue;
      *body_off = i - 8;
      return true;
    }
  }
  return false;
}

void check_one(const fs::path& p) {
  const std::string tag = p.filename().string();
  const std::vector<uint8_t> f = read_file(p);
  check(!f.empty(), (tag + ": golden exists").c_str(), 1, f.empty() ? 0 : 1);
  if (f.empty()) return;

  size_t off = 0;
  const bool found = find_abi_info(f, &off);
  check(found, (tag + ": carries an ABI-info section").c_str(), 1, found ? 1 : 0);
  if (!found) return;

  const uint8_t* b = f.data() + off;
  check(rd32(b) == zhao_abi::ZHAO_ABI_VERSION, (tag + ": ABI version is current").c_str(),
        zhao_abi::ZHAO_ABI_VERSION, rd32(b));
  check(rd32(b + 4) == zhao_abi::ZHAO_ZCAP_SCHEMA_VERSION,
        (tag + ": zcap schema version is current").c_str(), zhao_abi::ZHAO_ZCAP_SCHEMA_VERSION,
        rd32(b + 4));
  check(std::memcmp(b + 24, zhao_abi::ZHAO_GENERATOR_SHA256, 32) == 0,
        (tag + ": generator SHA-256 is current").c_str(), 0,
        std::memcmp(b + 24, zhao_abi::ZHAO_GENERATOR_SHA256, 32) == 0 ? 0 : 1);
  check(std::memcmp(b + 56, zhao_abi::ZHAO_ZIDL_SHA256, 32) == 0,
        (tag + ": zidl SHA-256 is current").c_str(), 0,
        std::memcmp(b + 56, zhao_abi::ZHAO_ZIDL_SHA256, 32) == 0 ? 0 : 1);
}

}  // namespace

int main() {
  const fs::path root = repo_root();

  // Every committed .zcap under captures/golden and tests/abi/golden.
  int seen = 0;
  for (const fs::path dir : {root / "captures" / "golden", root / "tests" / "abi" / "golden"}) {
    if (!fs::exists(dir)) continue;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
      if (!e.is_regular_file()) continue;
      if (e.path().extension() != ".zcap") continue;
      check_one(e.path());
      ++seen;
    }
  }

  // A run that found no goldens would pass every check above while checking
  // nothing at all -- the exact shape of the failure this test exists to catch.
  check(seen > 0, "at least one committed golden was examined", 1, static_cast<uint32_t>(seen));
  std::printf("[golden_abi_info] examined %d capture(s)\n", seen);

  return zhao::report_and_exit("golden_abi_info");
}
