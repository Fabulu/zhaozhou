// geom_desc_crc_directed.cpp -- GEOM.MESHFETCH's descriptor CRC walker
// (fpga/rtl/geometry/zhao_geom_desc_crc.sv), differenced against the ORACLE'S
// OWN CRC: `zhao_abi::zhao_crc32c(0, bytes, 60)`, the exact call
// `zref::meshfetch` refuses on (zref_meshfetch.hpp:139). The law is not
// transcribed here; the generated function is called.
//
// What it asserts, per case:
//   * a descriptor whose stored word at 60..63 is the oracle's CRC over 0..59
//     is ACCEPTED -- the verdict is high on the eighth beat, and only there;
//   * flipping any single bit anywhere in bytes 0..63 is REFUSED, and counted on
//     crc_fail_o (not framing_err_o);
//   * a short burst (last on beat k < 7) and a long one (a ninth beat) are
//     REFUSED and counted on framing_err_o, whatever their CRC word says;
//   * the walk RESTARTS after every burst, so a good descriptor straight after a
//     malformed one is still accepted;
//   * bubbles (beat_valid low) between beats do not move the walk.
// Every counter is seen to MOVE by legal stimulus, so no mutant is owed.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include "verilated.h"
#include "Vzhao_geom_desc_crc.h"
#include "zhao_sim.hpp"
#include "zhao_abi.h"

namespace {

int g_fail = 0, g_checks = 0;

void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

uint64_t word(const uint8_t* d, int beat) {
  uint64_t w = 0;
  for (int k = 7; k >= 0; --k) w = (w << 8) | d[8 * beat + k];
  return w;
}

// Drive `nbeats` beats of `d` (last on the final one, or never if !with_last)
// and return the verdict observed on the cycle `last` was presented. A bubble
// is inserted before each beat when `bubbles`.
bool drive(Vzhao_geom_desc_crc& t, const uint8_t* d, int nbeats, bool with_last,
           bool bubbles, bool* saw_early_ok) {
  bool verdict = false;
  for (int b = 0; b < nbeats; ++b) {
    if (bubbles) {
      t.beat_valid_i = 0;
      t.eval();
      if (t.crc_ok_o) *saw_early_ok = true;
      zhao::tick(t);
    }
    const bool last = with_last && (b == nbeats - 1);
    t.beat_valid_i = 1;
    t.beat_data_i = word(d, b % 8);
    t.beat_last_i = last ? 1 : 0;
    t.eval();
    if (last) verdict = t.crc_ok_o != 0;
    else if (t.crc_ok_o) *saw_early_ok = true;
    zhao::tick(t);
  }
  t.beat_valid_i = 0;
  t.beat_last_i = 0;
  t.eval();
  if (t.crc_ok_o) *saw_early_ok = true;
  return verdict;
}

void seal(uint8_t* d) {
  const uint32_t c = zhao_abi::zhao_crc32c(0, d, 60);
  d[60] = uint8_t(c);
  d[61] = uint8_t(c >> 8);
  d[62] = uint8_t(c >> 16);
  d[63] = uint8_t(c >> 24);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_desc_crc t;
  t.beat_valid_i = 0;
  t.beat_data_i = 0;
  t.beat_last_i = 0;
  t.rst_n = 0;
  zhao::tick(t);
  zhao::tick(t);
  t.rst_n = 1;
  zhao::tick(t);

  std::mt19937 rng(0x5EEDC4C);
  uint8_t d[64];
  uint32_t exp_desc = 0, exp_fail = 0, exp_frame = 0;
  bool early = false;

  // 1. Good descriptors, random bytes, sealed by the oracle.
  for (int n = 0; n < 200; ++n) {
    for (auto& x : d) x = uint8_t(rng());
    seal(d);
    const bool ok = drive(t, d, 8, true, (n & 3) == 1, &early);
    ++exp_desc;
    check(ok, "a descriptor sealed with the oracle's CRC over 0..59 is accepted");
  }
  // The meshfetch smoke fixture's own shape: mostly zero, one CRC word.
  std::memset(d, 0, sizeof d);
  d[0] = 1; d[2] = 3; d[3] = 1;
  seal(d);
  check(drive(t, d, 8, true, false, &early), "a mostly-zero fixture descriptor is accepted");
  ++exp_desc;

  // 2. Every single-bit flip of one sealed descriptor is refused as a CRC failure.
  for (auto& x : d) x = uint8_t(rng());
  seal(d);
  for (int bit = 0; bit < 512; ++bit) {
    uint8_t e[64];
    std::memcpy(e, d, 64);
    e[bit / 8] ^= uint8_t(1u << (bit % 8));
    const bool ok = drive(t, e, 8, true, false, &early);
    ++exp_desc;
    ++exp_fail;
    check(!ok, "a single flipped bit anywhere in 0..63 is refused");
  }
  check(t.crc_fail_o == exp_fail, "crc_fail_o counts exactly the 512 flipped descriptors");
  check(t.framing_err_o == 0, "no framing error was charged for a well-framed burst");

  // 3. Framing: short bursts (last on beat k < 7) and a ninth beat.
  for (int k = 1; k <= 7; ++k) {
    const bool ok = drive(t, d, k, true, false, &early);
    ++exp_desc;
    ++exp_frame;
    check(!ok, "a burst whose last beat is not the eighth is refused");
  }
  {
    const bool ok = drive(t, d, 9, true, false, &early);
    ++exp_desc;
    ++exp_frame;
    check(!ok, "a nine-beat burst is refused");
  }
  check(t.framing_err_o == exp_frame, "framing_err_o counts every mis-framed burst");
  check(t.crc_fail_o == exp_fail, "a mis-framed burst is NOT charged as a CRC failure");

  // 4. The walk restarts: the good descriptor straight after the malformed ones.
  check(drive(t, d, 8, true, true, &early), "a good descriptor after malformed bursts is accepted");
  ++exp_desc;

  check(!early, "the verdict is never high except on the last beat of a burst");
  check(t.descriptors_o == exp_desc, "descriptors_o counts every burst that ended");

  std::printf("geom_desc_crc_directed: %d checks, %d failed (descriptors=%u crc_fail=%u framing=%u)\n",
              g_checks, g_fail, unsigned(t.descriptors_o), unsigned(t.crc_fail_o),
              unsigned(t.framing_err_o));
  zhao::exit_hard(g_fail ? 1 : 0);
}
