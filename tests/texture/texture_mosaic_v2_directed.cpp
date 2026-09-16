// texture_mosaic_v2_directed.cpp — Packet-B Mosaic successor differential.
//
// Pins the frozen fold/hash law, exact two-clock latency, II=1, complete payload
// hold under backpressure, retirement counting, and the new structural idle_o.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_mosaic_v2.h"
#include "zhao_sim.hpp"

namespace {

struct Req {
  int32_t u = 0;
  int32_t v = 0;
  uint8_t mat_a = 0;
  uint8_t mat_b = 0;
  uint8_t weight = 0;
  bool mosaic = true;
  uint16_t src = 0;
};

struct Pick {
  uint8_t tile = 0;
  uint8_t tx = 0;
  uint8_t ty = 0;
  uint16_t src = 0;
};

uint32_t rng_next(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

Pick oracle(const Req& r) {
  const int32_t mu = r.u >> 10;
  const int32_t mv = r.v >> 10;
  const uint32_t pu = static_cast<uint32_t>(mu) & 127u;
  const uint32_t pv = static_cast<uint32_t>(mv) & 127u;
  const uint32_t hash =
      static_cast<uint32_t>(mu) * 73856093u ^ static_cast<uint32_t>(mv) * 19349663u;
  const uint32_t residue = hash % 255u;

  Pick p;
  p.tx = static_cast<uint8_t>((pu & 63u) ^ ((pu & 64u) ? 63u : 0u));
  p.ty = static_cast<uint8_t>((pv & 63u) ^ ((pv & 64u) ? 63u : 0u));
  p.tile = (!r.mosaic || residue < r.weight) ? r.mat_a : r.mat_b;
  p.src = r.src;
  return p;
}

void drive(Vzhao_texture_mosaic_v2& top, const Req& r) {
  top.req_u_i = static_cast<uint32_t>(r.u);
  top.req_v_i = static_cast<uint32_t>(r.v);
  top.req_mat_a_i = r.mat_a;
  top.req_mat_b_i = r.mat_b;
  top.req_weight_i = r.weight;
  top.req_mosaic_i = r.mosaic ? 1 : 0;
  top.req_src_id_i = r.src;
}

bool equal(const Vzhao_texture_mosaic_v2& top, const Pick& p) {
  return top.pick_tile_o == p.tile && top.pick_tx_o == p.tx && top.pick_ty_o == p.ty &&
         top.pick_src_id_o == p.src;
}

void reset(Vzhao_texture_mosaic_v2& top) {
  top.req_valid_i = 0;
  top.pick_ready_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

void test_stream(Vzhao_texture_mosaic_v2& top) {
  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "mosaic-v2 is structurally idle after reset", 1, top.idle_o);
  zhao::check(top.texture_samples_o == 0, "mosaic-v2 counter resets", 0, top.texture_samples_o);

  std::vector<Req> requests;
  // Exhaust every fold residue on both sides of zero, with a nonzero discarded
  // fraction.  Alternate fold-only spans so the enable cannot gate coordinates.
  for (int period = -2; period < 2; ++period) {
    for (int residue = 0; residue < 128; ++residue) {
      Req r;
      r.u = (period * 128 + residue) * 1024 + 511;
      r.v = (-period * 128 + 127 - residue) * 1024 + 1001;
      r.mat_a = static_cast<uint8_t>(17 + residue);
      r.mat_b = static_cast<uint8_t>(201 - residue);
      r.weight = static_cast<uint8_t>(residue * 29);
      r.mosaic = (residue & 7) != 0;
      r.src = static_cast<uint16_t>(requests.size());
      requests.push_back(r);
    }
  }

  // Add broad signed-domain vectors and the strict p/p+1 boundary anchors.
  uint32_t seed = 0x51A7C0DEu;
  for (int i = 0; i < 256; ++i) {
    Req r;
    r.u = static_cast<int32_t>(rng_next(&seed));
    r.v = static_cast<int32_t>(rng_next(&seed));
    r.mat_a = static_cast<uint8_t>(rng_next(&seed));
    r.mat_b = static_cast<uint8_t>(rng_next(&seed));
    r.weight = static_cast<uint8_t>(rng_next(&seed));
    r.mosaic = (rng_next(&seed) & 3u) != 0;
    r.src = static_cast<uint16_t>(requests.size());
    requests.push_back(r);
  }
  for (int weight : {0, 1, 8, 9, 84, 85, 188, 189, 254, 255}) {
    Req r;
    r.u = (weight & 1) ? (1 << 10) : (2 << 10);
    r.v = (weight & 1) ? 0 : (3 << 10);
    r.mat_a = 0x3A;
    r.mat_b = 0xC5;
    r.weight = static_cast<uint8_t>(weight);
    r.src = static_cast<uint16_t>(requests.size());
    requests.push_back(r);
  }

  std::deque<Pick> expected;
  size_t offered = 0;
  size_t retired = 0;
  int cycle = 0;
  int first_accept = -1;
  int first_retire = -1;
  int hold_checks = 0;
  int wrong = 0;
  bool prev_stalled = false;
  Pick prev_pick;

  // First run all-ready long enough to prove exact fixed latency and II=1, then
  // apply a deterministic stall pattern while continuing to exercise full hold.
  while ((retired < requests.size() || offered < requests.size()) && cycle < 10000) {
    const bool ready = cycle < 96 ? true : ((cycle % 11) != 3 && (cycle % 11) != 4);
    top.pick_ready_i = ready ? 1 : 0;
    top.req_valid_i = offered < requests.size() ? 1 : 0;
    if (offered < requests.size()) drive(top, requests[offered]);
    top.eval();

    if (prev_stalled) {
      ++hold_checks;
      Pick now{static_cast<uint8_t>(top.pick_tile_o), static_cast<uint8_t>(top.pick_tx_o),
               static_cast<uint8_t>(top.pick_ty_o), static_cast<uint16_t>(top.pick_src_id_o)};
      if (!top.pick_valid_o || now.tile != prev_pick.tile || now.tx != prev_pick.tx ||
          now.ty != prev_pick.ty || now.src != prev_pick.src)
        ++wrong;
    }

    if (top.pick_valid_o && top.pick_ready_i) {
      if (first_retire < 0) first_retire = cycle;
      if (expected.empty() || !equal(top, expected.front())) {
        ++wrong;
      } else {
        expected.pop_front();
      }
      ++retired;
    }

    const bool accept = top.req_valid_i && top.req_ready_o;
    if (accept) {
      if (first_accept < 0) first_accept = cycle;
      expected.push_back(oracle(requests[offered]));
      ++offered;
    }

    prev_stalled = top.pick_valid_o && !top.pick_ready_i;
    if (prev_stalled) {
      prev_pick =
          Pick{static_cast<uint8_t>(top.pick_tile_o), static_cast<uint8_t>(top.pick_tx_o),
               static_cast<uint8_t>(top.pick_ty_o), static_cast<uint16_t>(top.pick_src_id_o)};
    }

    zhao::tick(top);
    ++cycle;
  }

  top.req_valid_i = 0;
  top.pick_ready_i = 1;
  top.eval();
  zhao::check(offered == requests.size(), "mosaic-v2 accepts every request", requests.size(),
              offered);
  zhao::check(retired == requests.size(), "mosaic-v2 retires every request once", requests.size(),
              retired);
  zhao::check(wrong == 0, "mosaic-v2 payload and hold match the frozen oracle", 0, wrong);
  zhao::check(first_retire - first_accept == 2, "mosaic-v2 fixed latency remains two clocks", 2,
              first_retire - first_accept);
  zhao::check(hold_checks > 0, "mosaic-v2 output stall actually exercised hold", 1,
              hold_checks > 0 ? 1 : 0);
  zhao::check(top.texture_samples_o == requests.size(),
              "mosaic-v2 counts accepted retirements, not offers", requests.size(),
              top.texture_samples_o);
  zhao::check(top.idle_o == 1, "mosaic-v2 returns to structural idle", 1, top.idle_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_mosaic_v2 top;
  test_stream(top);
  return zhao::report_and_exit("texture_mosaic_v2_directed");
}
