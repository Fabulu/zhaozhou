// light_isqrt64_ii8_directed.cpp -- qualification of the folded II=8 root.
//
// THE ORACLE IS THE SHIPPED ONE. `zref::isqrt_u64` is the function the
// renderer law itself calls (`shade_from_world_normal_unclamped` is
// `div_rhu_s128(ndot, isqrt_u64(nmag2))`) and the one `skin_world_normal`
// calls to make the magnitude it hands out. So this bench compares the
// silicon against the compiled law, not against a second reading of the
// same paragraph. No floor-sqrt is restated in this file.
//
// WHAT THE EDGE SET IS FOR. A restoring root has exactly one interesting
// failure region and it is a WIDTH: the invariant remainder reaches 2^33 - 2
// after the last step, and 2^32 - 2 after the step before it. A build that
// narrowed either register would be right for every small radicand and wrong
// only near the top of the range -- which is the flattering direction, and
// the reason sections 2 and 3 walk every power of two and both perfect-square
// neighbours rather than sampling uniformly.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected root by +1; the
// suite must then FAIL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_light_isqrt64_ii8.h"

#include "zhao_sim.hpp"
#include "zref/zref_trig.hpp"

using zhao::check;

namespace {

uint64_t g_cycles = 0;
void tk(Vzhao_light_isqrt64_ii8& d) {
  zhao::tick(d);
  ++g_cycles;
}

void reset_dut(Vzhao_light_isqrt64_ii8& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.radicand_i = 0;
  d.tag_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tk(d);
  d.rst_n = 1;
  d.eval();
  tk(d);
}

struct Stats {
  uint64_t first_accept = 0, last_accept = 0;
  uint64_t first_retire = 0, last_retire = 0;
  uint64_t accepted = 0, retired = 0, first_latency = 0;
};

struct Got {
  uint32_t root;
  uint16_t tag;
};

std::vector<Got> stream(Vzhao_light_isqrt64_ii8& d, const std::vector<uint64_t>& rad,
                        const std::vector<uint16_t>& tags, int stall_mod, int bubble_mod,
                        Stats* st) {
  std::vector<Got> out;
  out.reserve(rad.size());
  size_t sent = 0;
  uint64_t guard = 0;
  Stats s;
  while (out.size() < rad.size()) {
    const bool consumer_ready =
        (stall_mod == 0) || ((g_cycles % static_cast<uint64_t>(stall_mod)) != 0);
    const bool want_offer =
        sent < rad.size() &&
        (bubble_mod == 0 || ((g_cycles % static_cast<uint64_t>(bubble_mod)) != 0));
    d.r_ready_i = consumer_ready ? 1 : 0;
    d.v_valid_i = want_offer ? 1 : 0;
    if (want_offer) {
      d.radicand_i = rad[sent];
      d.tag_i = tags[sent];
    }
    d.eval();
    const bool accept_now = want_offer && d.v_ready_o;
    const bool retire_now = d.r_valid_o && consumer_ready;
    Got g{};
    if (retire_now) {
      g.root = static_cast<uint32_t>(d.root_o);
      g.tag = static_cast<uint16_t>(d.tag_o);
    }
    const uint64_t now = g_cycles;
    tk(d);
    if (accept_now) {
      if (s.accepted == 0) s.first_accept = now;
      s.last_accept = now;
      ++s.accepted;
      ++sent;
    }
    if (retire_now) {
      if (s.retired == 0) { s.first_retire = now; s.first_latency = now - s.first_accept; }
      s.last_retire = now;
      ++s.retired;
      out.push_back(g);
    }
    if (++guard > 8000000) {
      check(false, "root stream never completed", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("light_isqrt64_ii8_directed"));
    }
  }
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.eval();
  if (st) *st = s;
  return out;
}

void check_batch(Vzhao_light_isqrt64_ii8& d, const std::vector<uint64_t>& rad, int stall_mod,
                 int bubble_mod, const char* what, int inject_index, int32_t inject) {
  std::vector<uint16_t> tags(rad.size());
  for (size_t i = 0; i < rad.size(); ++i) tags[i] = static_cast<uint16_t>(0xA000u + (i & 0xFFF));
  const std::vector<Got> got = stream(d, rad, tags, stall_mod, bubble_mod, nullptr);
  check(got.size() == rad.size(), "every radicand produced exactly one root", rad.size(),
        got.size());
  for (size_t i = 0; i < got.size() && i < rad.size(); ++i) {
    const uint64_t want = zref::isqrt_u64(rad[i]) +
                          ((static_cast<int>(i) == inject_index) ? inject : 0);
    check(got[i].root == want, what, static_cast<uint32_t>(want), got[i].root);
    check(got[i].tag == tags[i], "the tag came back with its own root", tags[i], got[i].tag);
  }
}

uint64_t g_rng = 0x2545F4914F6CDD1DULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);
  Vzhao_light_isqrt64_ii8* dutp = new Vzhao_light_isqrt64_ii8;  // heap, harness rule
  Vzhao_light_isqrt64_ii8& dut = *dutp;
  reset_dut(dut);

  // ---- 1: SMALL AND PENCIL-CHECKABLE --------------------------------------
  {
    std::vector<uint64_t> r = {0, 1, 2, 3, 4, 5, 8, 9, 15, 16, 17, 24, 25, 26, 99, 100, 101};
    check(zref::isqrt_u64(0) == 0, "pencil: isqrt(0) == 0", 0, zref::isqrt_u64(0));
    check(zref::isqrt_u64(15) == 3, "pencil: isqrt(15) == 3", 3, zref::isqrt_u64(15));
    check(zref::isqrt_u64(16) == 4, "pencil: isqrt(16) == 4", 4, zref::isqrt_u64(16));
    check(zref::isqrt_u64(99) == 9, "pencil: isqrt(99) == 9", 9, zref::isqrt_u64(99));
    check_batch(dut, r, 0, 0, "small exact roots", -1, 0);
  }

  // ---- 2: EVERY POWER OF TWO AND ITS NEIGHBOURS ---------------------------
  // The 34-bit remainder only matters near the top of the range, so a width
  // defect is invisible under uniform sampling and loud here.
  {
    std::vector<uint64_t> r;
    for (int k = 0; k < 64; ++k) {
      const uint64_t p = 1ull << k;
      r.push_back(p);
      if (p > 0) r.push_back(p - 1);
      if (k < 63) r.push_back(p + 1);
    }
    r.push_back(UINT64_MAX);
    r.push_back(UINT64_MAX - 1);
    check_batch(dut, r, 0, 0, "powers of two and their neighbours", -1, 0);
    // The single most dangerous radicand: root == UINT32_MAX, remainder at
    // its widest.
    std::vector<uint64_t> top = {UINT64_MAX};
    check(zref::isqrt_u64(UINT64_MAX) == 0xFFFFFFFFull,
          "pencil: isqrt(UINT64_MAX) is exactly UINT32_MAX", 0xFFFFFFFFu,
          static_cast<uint32_t>(zref::isqrt_u64(UINT64_MAX)));
    check_batch(dut, top, 0, 0, "the widest remainder in the domain", -1, 0);
  }

  // ---- 3: PERFECT SQUARES AND BOTH NEIGHBOURS ------------------------------
  // s*s - 1, s*s, s*s + 1 for roots spanning the whole 32-bit range. The
  // -1 case is the one a >= / > slip in the trial subtraction gets wrong.
  {
    std::vector<uint64_t> r;
    for (int k = 0; k < 32; ++k) {
      const uint64_t s = 1ull << k;
      const uint64_t sq = s * s;
      if (sq > 0) r.push_back(sq - 1);
      r.push_back(sq);
      r.push_back(sq + 1);
    }
    for (int i = 0; i < 300; ++i) {
      const uint64_t s = rnd() & 0xFFFFFFFFull;
      const uint64_t sq = s * s;
      if (sq > 0) r.push_back(sq - 1);
      r.push_back(sq);
      if (sq < UINT64_MAX) r.push_back(sq + 1);
    }
    check_batch(dut, r, 0, 0, "perfect squares and both neighbours", -1, 0);
  }

  // ---- 4: THE ACTUAL PRODUCER DOMAIN --------------------------------------
  // What the lighting service really feeds this block is the exact u64 sum of
  // three s32 squares. That is not uniform over u64 -- it is bounded by
  // 3 * 2^62 and clusters near it for rail normals -- so it is sampled as
  // itself rather than approximated by random 64-bit words.
  int cov_over32 = 0;
  {
    std::vector<uint64_t> r;
    for (int i = 0; i < 600; ++i) {
      int32_t n[3];
      for (int k = 0; k < 3; ++k) {
        switch (rnd() & 3) {
          case 0: n[k] = static_cast<int32_t>(rnd() & 7) - 3; break;
          case 1: n[k] = static_cast<int32_t>(rnd() % 131073) - 65536; break;
          case 2: n[k] = (rnd() & 1) ? INT32_MAX - static_cast<int32_t>(rnd() & 3)
                                     : INT32_MIN + static_cast<int32_t>(rnd() & 3); break;
          default: n[k] = static_cast<int32_t>(rnd()); break;
        }
      }
      const uint64_t s = static_cast<uint64_t>(n[0]) * static_cast<uint64_t>(n[0]) +
                         static_cast<uint64_t>(n[1]) * static_cast<uint64_t>(n[1]) +
                         static_cast<uint64_t>(n[2]) * static_cast<uint64_t>(n[2]);
      if (zref::isqrt_u64(s) > 0xFFFFFFFFull) ++cov_over32;  // cannot happen; asserted below
      r.push_back(s);
    }
    // Three s32 squares sum to at most 3 * 2^62 < 2^64, so the sum never wraps
    // and the root never exceeds UINT32_MAX. That is the sentence that makes a
    // u64 radicand port and a u32 root port sufficient, and it is asserted
    // rather than asserted-in-a-comment.
    check(cov_over32 == 0, "three s32 squares never produce a root above UINT32_MAX", 0,
          static_cast<uint32_t>(cov_over32));
    const int inject_at = break_oracle ? 11 : -1;
    check_batch(dut, r, 0, 0, "DIFFERENTIAL: the producer domain against zref::isqrt_u64",
                inject_at, 1);
  }

  // ---- 5: BUBBLES AND STALLS DO NOT MOVE A ROOT ---------------------------
  {
    std::vector<uint64_t> r;
    for (int i = 0; i < 150; ++i) r.push_back(rnd());
    check_batch(dut, r, 0, 0, "root is independent of the traffic pattern", -1, 0);
    check_batch(dut, r, 4, 0, "root under a stalling consumer", -1, 0);
    check_batch(dut, r, 0, 3, "root under input bubbles", -1, 0);
    check_batch(dut, r, 5, 7, "root under both", -1, 0);
  }

  // ---- 6: RESET WITH EVERY CELL OCCUPIED ----------------------------------
  {
    for (int i = 0; i < 20; ++i) {
      dut.v_valid_i = 1;
      dut.radicand_i = 0xFEDCBA9876543210ull;
      dut.tag_i = static_cast<uint16_t>(0x0600 + i);
      dut.r_ready_i = 0;
      dut.eval();
      tk(dut);
    }
    dut.v_valid_i = 0;
    dut.eval();
    reset_dut(dut);
    check(dut.r_valid_o == 0, "reset mid-flight leaves no root offered", 0, dut.r_valid_o);
    std::vector<uint64_t> r = {123456789ull, UINT64_MAX, 0};
    check_batch(dut, r, 0, 0, "the batch after a mid-flight reset is correct", -1, 0);
  }

  // ---- 7: THE SERVICE INTERVAL ---------------------------------------------
  // The rate the stress fixture needs is one root per 8 clocks: 120,000
  // normals inside a 960,000-clock term schedule. Accept-to-accept and
  // retire-to-retire are measured separately from the latency.
  {
    const int kN = 2048;
    std::vector<uint64_t> r(kN);
    std::vector<uint16_t> t(kN);
    for (int i = 0; i < kN; ++i) { r[i] = rnd(); t[i] = static_cast<uint16_t>(i & 0xFFFF); }
    Stats st;
    const std::vector<Got> got = stream(dut, r, t, 0, 0, &st);
    for (int i = 0; i < kN; ++i)
      check(got[i].root == zref::isqrt_u64(r[i]), "II stream root is still exact",
            static_cast<uint32_t>(zref::isqrt_u64(r[i])), got[i].root);
    const double ii_a = static_cast<double>(st.last_accept - st.first_accept) / (kN - 1);
    const double ii_r = static_cast<double>(st.last_retire - st.first_retire) / (kN - 1);
    std::printf(
        "[isqrt64] accept-to-accept II = %.4f clk | retire-to-retire II = %.4f clk | "
        "first accept->consume latency = %llu clk | %d roots\n",
        ii_a, ii_r, static_cast<unsigned long long>(st.first_latency), kN);
    check(ii_a > 7.999 && ii_a < 8.001, "sustained ACCEPT interval is exactly 8", 8,
          static_cast<uint64_t>(ii_a * 1000.0));
    check(ii_r > 7.999 && ii_r < 8.001, "sustained RETIRE interval is exactly 8", 8,
          static_cast<uint64_t>(ii_r * 1000.0));
    check(st.first_latency >= 32 && st.first_latency <= 34,
          "latency is ~33 clocks and is NOT the service interval", 33,
          static_cast<uint64_t>(st.first_latency));
    // The capacity sentence the schedule depends on, stated as arithmetic
    // rather than as a promise.
    const double need = 120000.0 * ii_a;
    std::printf("[isqrt64] 120,000 normals at this interval = %.0f clocks against the "
                "960,000-clock term schedule (%.1f%% occupancy).\n",
                need, 100.0 * need / 960000.0);
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("light_isqrt64_ii8_directed"));
}
