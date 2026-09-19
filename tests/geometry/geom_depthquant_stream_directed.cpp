// geom_depthquant_stream_directed.cpp -- zhao_geom_depthquant_stream, the
// TAGGED schedule of the one depth law (fpga/rtl/geometry/zhao_geom_depthquant.sv).
//
// The bench PLAYS the reciprocal service, answering from `zref::rcp_u24` -- the
// same function `zref::depth_of_raw` calls -- and deliberately OUT OF ORDER
// with random latency, because the stream's whole claim is that each answer is
// combined against ITS OWN side record whatever order the service completes
// in. Every output is differenced, by tag, against `zref::depth_of_raw`.
//
//   1  400 random vertices over the three profiles and their clamps, answers
//      shuffled: every invw24 equals the law, every tag comes back once.
//   2  a token naming no context in flight: tok_stray_o FIRES and nothing is
//      emitted for it (stimulus at this block's ports; no mutant owed).
//   3  the reserved profile 3 is REFUSED to 0, counted -- as the FSM does.
//   4  RATE: a service with one answer per clock and 8 clocks of latency; the
//      stream accepts one vertex per clock while contexts are free.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>
#include <vector>

#include "verilated.h"
#include "Vzhao_geom_depthquant_stream.h"
#include "zhao_sim.hpp"
#include "zref/zref_depth.hpp"
#include "zref/zref_rcp.hpp"

namespace {

int g_checks = 0, g_fail = 0;
void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

struct Pending {
  long due;
  uint32_t d;
  uint32_t tok;
};

struct Bench {
  Vzhao_geom_depthquant_stream t;
  std::mt19937 rng{0xD0C0FFEE};
  std::vector<Pending> svc;      // requests the played service holds
  long clocks = 0;
  int min_lat = 3, max_lat = 30;
  bool stray_next = false;        // answer ONE bogus token next

  void step() {
    zhao::tick(t);
    ++clocks;
  }

  // Drive the service's answer side for this clock; returns true if the DUT
  // took it. Called with inputs for this clock already set.
  void service_drive() {
    t.rcp_rvalid_i = 0;
    if (stray_next) {
      t.rcp_rvalid_i = 1;
      t.rcp_r_i = 0x800000;
      t.rcp_k_i = 1;
      t.rcp_tok_i = 0xEE;       // no such context
      return;
    }
    // pick a due request at RANDOM among the due ones: out of order
    std::vector<size_t> due;
    for (size_t i = 0; i < svc.size(); ++i)
      if (svc[i].due <= clocks) due.push_back(i);
    if (due.empty()) return;
    const size_t k = due[rng() % due.size()];
    const zref::rcp24_result rc = zref::rcp_u24(svc[k].d ? svc[k].d : 1u);
    t.rcp_rvalid_i = 1;
    t.rcp_r_i = rc.r;
    t.rcp_k_i = static_cast<uint32_t>(rc.k);
    t.rcp_tok_i = svc[k].tok;
  }
  void service_after(bool took_answer, bool took_request, uint32_t d, uint32_t tok) {
    if (took_answer && !stray_next) {
      for (size_t i = 0; i < svc.size(); ++i)
        if (svc[i].tok == t.rcp_tok_i && svc[i].due <= clocks - 1) {
          svc.erase(svc.begin() + static_cast<long>(i));
          break;
        }
    }
    if (took_answer && stray_next) stray_next = false;
    if (took_request) {
      const int lat = min_lat + static_cast<int>(rng() % static_cast<unsigned>(max_lat - min_lat + 1));
      svc.push_back({clocks + lat, d, tok});
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* bp = new Bench;  // heap, never deleted: zhao_sim.hpp's exit-time rule
  Bench& b = *bp;
  auto& t = b.t;
  t.v_valid_i = 0; t.d_ready_i = 1; t.rcp_ready_i = 1; t.rcp_rvalid_i = 0;
  t.rst_n = 0;
  for (int i = 0; i < 3; ++i) b.step();
  t.rst_n = 1;
  b.step();

  // A vertex list over all three profiles, with the clamps and the pins.
  auto make = [&](int n, std::vector<uint64_t>& w, std::vector<unsigned>& p) {
    for (int i = 0; i < n; ++i) {
      const unsigned prof = b.rng() % 3;
      const auto& g = zref::gen::DEPTH_PROFILES[prof];
      const unsigned pick = b.rng() % 10;
      uint64_t ww = pick == 0 ? g.wmin_raw : pick == 1 ? g.wmax_raw
                  : pick == 2 ? g.wmin_raw / 3 : pick == 3 ? g.wmax_raw * 2
                  : g.wmin_raw + (uint64_t(b.rng()) << 8) % (g.wmax_raw - g.wmin_raw);
      if (ww > 0x7FFFFFFFull) ww = 0x7FFFFFFFull;
      w.push_back(ww);
      p.push_back(prof);
    }
  };

  // Run a vertex list through; returns clocks from first offer to last output.
  auto run = [&](const std::vector<uint64_t>& w, const std::vector<unsigned>& p,
                 std::map<uint32_t, uint32_t>& got, int& dup) -> long {
    size_t next = 0;
    const long c0 = b.clocks;
    long last = c0;
    for (long guard = 0; guard < 200000 && (got.size() < w.size()); ++guard) {
      t.v_valid_i = next < w.size();
      if (next < w.size()) {
        t.v_w_i = w[next];
        t.v_profile_i = p[next];
        t.v_tag_i = static_cast<uint32_t>(next);
      }
      b.service_drive();
      t.eval();
      const bool acc = t.v_valid_i && t.v_ready_o;
      const bool req = t.rcp_valid_o && t.rcp_ready_i;
      const uint32_t req_d = t.rcp_d_o, req_tok = t.rcp_tok_o;
      const bool ans = t.rcp_rvalid_i && t.rcp_rready_o;
      if (t.d_valid_o && t.d_ready_i) {
        if (got.count(t.d_tag_o)) ++dup;
        got[t.d_tag_o] = t.d_invw24_o;
        last = b.clocks;
      }
      b.step();
      b.service_after(ans, req, req_d, req_tok);
      if (acc) ++next;
    }
    t.v_valid_i = 0;
    return last - c0;
  };

  // ---- 1: the law, out of order, by tag -----------------------------------
  {
    std::vector<uint64_t> w;
    std::vector<unsigned> p;
    make(400, w, p);
    std::map<uint32_t, uint32_t> got;
    int dup = 0;
    run(w, p, got, dup);
    int bad = 0;
    for (size_t i = 0; i < w.size(); ++i) {
      const auto it = got.find(static_cast<uint32_t>(i));
      const uint32_t want = zref::depth_of_raw(w[i], p[i]);
      if (it == got.end() || it->second != want) {
        if (bad < 4)
          std::printf("    tag %zu profile %u w=%llu: rtl %06X law %06X\n", i, p[i],
                      (unsigned long long)w[i], it == got.end() ? 0xFFFFFFFFu : it->second, want);
        ++bad;
      }
    }
    ck(got.size() == w.size() && dup == 0, "1: every tag comes back exactly once");
    ck(bad == 0, "1: every invw24 equals zref::depth_of_raw, answers completing OUT OF ORDER");
    ck(t.tok_stray_o == 0 && t.refused_o == 0, "1: no stray token, no refusal on legal traffic");
  }

  // ---- 2: a stray token is counted and emits nothing ----------------------
  {
    const uint32_t s0 = t.tok_stray_o;
    b.stray_next = true;
    for (int i = 0; i < 4; ++i) {
      b.service_drive();
      t.eval();
      const bool ans = t.rcp_rvalid_i && t.rcp_rready_o;
      const bool out = t.d_valid_o;
      b.step();
      b.service_after(ans, false, 0, 0);
      ck(!out, "2: nothing is emitted for a token naming no context");
    }
    ck(t.tok_stray_o == s0 + 1, "2: tok_stray_o FIRES on an answer for a context not in flight");
  }

  // ---- 3: the reserved profile is refused to zero -------------------------
  {
    std::vector<uint64_t> w = {0x30000};
    std::vector<unsigned> p = {3};
    std::map<uint32_t, uint32_t> got;
    int dup = 0;
    const uint32_t r0 = t.refused_o;
    run(w, p, got, dup);
    ck(got.count(0) && got[0] == 0 && t.refused_o == r0 + 1,
       "3: profile 3 is REFUSED to 0 and counted, exactly as the FSM does");
  }

  // ---- 4: RATE --------------------------------------------------------------
  {
    b.min_lat = 8;
    b.max_lat = 8;
    std::vector<uint64_t> w;
    std::vector<unsigned> p;
    make(256, w, p);
    std::map<uint32_t, uint32_t> got;
    int dup = 0;
    const long c = run(w, p, got, dup);
    ck(got.size() == 256, "4: all 256 retired");
    std::printf("  RATE: 256 vertices in %ld clocks = %.2f clocks/vertex (service: 1 answer/clock, 8 clocks latency)\n",
                c, static_cast<double>(c) / 256.0);
    ck(c < 256 * 2, "4: the stream sustains better than one vertex per two clocks when its service can");
  }

  std::printf("geom_depthquant_stream_directed: %d checks, %d failed (vertices=%u near=%u far=%u sat=%u "
              "refused=%u stray=%u)\n",
              g_checks, g_fail, t.vertices_o, t.clamped_near_o, t.clamped_far_o, t.saturated_o,
              t.refused_o, t.tok_stray_o);
  zhao::exit_hard(g_fail ? 1 : 0);
}
