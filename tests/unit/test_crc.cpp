// test_crc.cpp — CRC-32C conformance (spec/capture_format.md 2.1) + SHA-256
// vectors. Normative table:
//   * the six check vectors (incl. empty input 0x00000000 — the recon's
//     "derived, verify in-suite" value, now evidence),
//   * the check constant 0x48674BC7: crc32c(0, msg || LE(crc)) is the SAME
//     for every message (this replaces the recon's unreproducible
//     0x1C2D19ED; the underlying register residue is the catalogue's
//     0xB798B438),
//   * incremental == one-shot,
//   * sha256 FIPS 180-4 vectors (locked against the TS copy by goldens).

#include "zhao_abi.h"  // generated

#include "zhao_sim.hpp"
#include "zref/zref_sha256.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using zhao_abi::zhao_crc32c;

struct Vector {
  const char* name;
  std::vector<uint8_t> data;
  uint32_t crc;
};

std::vector<Vector> vectors() {
  return {
      {"empty", {}, 0x00000000u},
      {"123456789", {'1', '2', '3', '4', '5', '6', '7', '8', '9'}, 0xE3069283u},
      {"32x00", std::vector<uint8_t>(32, 0x00), 0x8A9136AAu},
      {"32xFF", std::vector<uint8_t>(32, 0xFF), 0x62A8AB43u},
      {"00..1F",
       [] {
         std::vector<uint8_t> v;
         for (int i = 0; i < 32; i++) v.push_back(uint8_t(i));
         return v;
       }(),
       0x46DD794Eu},
      {"1F..00",
       [] {
         std::vector<uint8_t> v;
         for (int i = 0; i < 32; i++) v.push_back(uint8_t(31 - i));
         return v;
       }(),
       0x113FDB5Cu},
  };
}

}  // namespace

int main() {
  constexpr uint32_t CHECK_CONSTANT = 0x48674BC7u;  // spec/capture_format.md 2.1

  for (const auto& v : vectors()) {
    const uint32_t got = zhao_crc32c(0, v.data.data(), v.data.size());
    zhao::check(got == v.crc, v.name, v.crc, got);

    // check constant: append the stored (LE) CRC and re-run — same for EVERY
    // message. This is the cross-language self-test contract.
    std::vector<uint8_t> with_crc = v.data;
    for (int i = 0; i < 4; i++) {
      with_crc.push_back(uint8_t((v.crc >> (8 * i)) & 0xFF));
    }
    const uint32_t cc = zhao_crc32c(0, with_crc.data(), with_crc.size());
    zhao::check(cc == CHECK_CONSTANT, "check constant (msg||LE crc)", CHECK_CONSTANT, cc);
  }

  // incremental == one-shot
  {
    const auto v = vectors()[4];
    const uint32_t whole = zhao_crc32c(0, v.data.data(), v.data.size());
    uint32_t running = zhao_crc32c(0, v.data.data(), 7);
    running = zhao_crc32c(running, v.data.data() + 7, v.data.size() - 7);
    zhao::check(running == whole, "incremental equals one-shot", whole, running);
  }

  // table sanity: entry 0x80 of a reflected table is the reflected poly
  // itself (0x80 shifts down to 0x01, whose single round trips the poly)
  {
    zhao::check(zhao_abi::ZHAO_CRC32C_TABLE[0x80] == 0x82F63B78u,
                "crc table[0x80] == reflected poly", 0x82F63B78u,
                zhao_abi::ZHAO_CRC32C_TABLE[0x80]);
    zhao::check(zhao_abi::ZHAO_CRC32C_TABLE[0] == 0u, "crc table[0] == 0", 0,
                zhao_abi::ZHAO_CRC32C_TABLE[0]);
  }

  // SHA-256 (FIPS 180-4) — the C++ half of the golden .zcap lock
  {
    const auto h0 = zhao::sha256(nullptr, 0);
    const std::string empty_hex =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    std::string got;
    for (uint8_t b : h0) {
      char hex[3];
      std::snprintf(hex, sizeof(hex), "%02x", b);
      got += hex;
    }
    zhao::check(got == empty_hex, "sha256(empty)", 0, got == empty_hex ? 0 : 1);

    const std::string abc = "abc";
    const auto h1 = zhao::sha256(reinterpret_cast<const uint8_t*>(abc.data()), abc.size());
    const std::string abc_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    got.clear();
    for (uint8_t b : h1) {
      char hex[3];
      std::snprintf(hex, sizeof(hex), "%02x", b);
      got += hex;
    }
    zhao::check(got == abc_hex, "sha256(abc)", 0, got == abc_hex ? 0 : 1);
  }

  return zhao::report_and_exit("test_crc");
}
