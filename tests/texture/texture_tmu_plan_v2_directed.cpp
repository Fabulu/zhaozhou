// texture_tmu_plan_v2_directed.cpp
//
// Packet-B successor gate: the planner keeps the predecessor's address law, but
// every accepted request must carry the exact 18-bit route token and palette
// tuple through all five elastic stages.  Structural idle is checked against
// those stages, and the complete cache-access output is held under backpressure.
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>

#include "verilated.h"
#include "Vzhao_texture_tmu_plan_v2.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_texture_tmu_plan_v2;

struct Req {
  uint32_t u;
  uint32_t v;
  uint32_t base;
  uint32_t mode;
  uint8_t lod;
  uint32_t route;
  uint8_t pal_slot;
  uint8_t pal_gen;
};

struct Acc {
  std::array<uint32_t, 4> addr{};
  uint32_t route{};
  uint8_t pal_slot{};
  uint8_t pal_gen{};
  uint8_t en{};
  uint8_t filter{};
  uint8_t err{};
  uint8_t nib{};
  uint8_t fu{};
  uint8_t fv{};
  uint8_t fmt{};
};

uint32_t mode(uint32_t fmt, uint32_t filter, uint32_t wrap_u, uint32_t wrap_v,
              uint32_t log2w, uint32_t log2h, uint32_t max_level, uint32_t mip) {
  return (fmt & 7u) | ((filter & 1u) << 3) | ((wrap_u & 3u) << 4) |
         ((wrap_v & 3u) << 6) | ((log2w & 15u) << 8) |
         ((log2h & 15u) << 12) | ((max_level & 15u) << 16) | ((mip & 1u) << 20);
}

Req make_req(int serial) {
  Req r{};
  const uint32_t x = static_cast<uint32_t>((serial * 3) & 15);
  const uint32_t y = static_cast<uint32_t>((serial * 5 + 1) & 15);
  // x/16 and y/16 in signed Q16.16; selected level is 16x16, so lane zero
  // resolves exactly to integer texel (x,y).
  r.u = x << 12;
  r.v = y << 12;
  r.base = 0x00100000u + static_cast<uint32_t>(serial & 3) * 0x1000u;
  r.mode = mode(1, 0, 0, 0, 4, 4, 0, 0);  // RGB565, nearest, repeat
  r.lod = static_cast<uint8_t>(serial * 17);
  r.route = (static_cast<uint32_t>(serial & 3) << 16) |
            static_cast<uint32_t>((0x2100 + serial * 37) & 0xFFFF);
  r.pal_slot = static_cast<uint8_t>(serial & 3);
  r.pal_gen = static_cast<uint8_t>(0x80 + serial * 13);
  return r;
}

Acc expect_acc(const Req& r) {
  Acc a{};
  const uint32_t x = r.u >> 12;
  const uint32_t y = r.v >> 12;
  const uint32_t total0 = y * 16u + x;
  const uint32_t total1 = y * 16u + ((x + 1u) & 15u);
  const uint32_t total2 = ((y + 1u) & 15u) * 16u + x;
  const uint32_t total3 = ((y + 1u) & 15u) * 16u + ((x + 1u) & 15u);
  a.addr = {r.base + 2u * total0, r.base + 2u * total1,
            r.base + 2u * total2, r.base + 2u * total3};
  a.route = r.route;
  a.pal_slot = r.pal_slot;
  a.pal_gen = r.pal_gen;
  a.en = 1;
  a.filter = 0;
  a.err = 0;
  a.nib = 0;
  a.fu = 0;
  a.fv = 0;
  a.fmt = 1;
  return a;
}

void drive(Dut& top, const Req& r) {
  top.req_u_i = r.u;
  top.req_v_i = r.v;
  top.req_base_i = r.base;
  top.req_mode_i = r.mode;
  top.req_lod_i = r.lod;
  top.req_src_id_i = r.route;
  top.req_pal_slot_i = r.pal_slot;
  top.req_pal_gen_i = r.pal_gen;
}

Acc sample(const Dut& top) {
  Acc a{};
  for (int k = 0; k < 4; ++k) a.addr[k] = top.acc_addr_o[k];
  a.route = top.acc_src_id_o;
  a.pal_slot = top.acc_pal_slot_o;
  a.pal_gen = top.acc_pal_gen_o;
  a.en = top.acc_en_o;
  a.filter = top.acc_filter_o;
  a.err = top.acc_err_o;
  a.nib = top.acc_nib_o;
  a.fu = top.acc_fu_o;
  a.fv = top.acc_fv_o;
  a.fmt = top.acc_fmt_o;
  return a;
}

bool exact(const Acc& a, const Acc& b) {
  return a.addr == b.addr && a.route == b.route && a.pal_slot == b.pal_slot &&
         a.pal_gen == b.pal_gen && a.en == b.en && a.filter == b.filter &&
         a.err == b.err && a.nib == b.nib && a.fu == b.fu && a.fv == b.fv &&
         a.fmt == b.fmt;
}

void reset(Dut& top) {
  top.req_valid_i = 0;
  top.acc_ready_i = 1;
  drive(top, Req{});
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;

  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "reset empties all five planner stages", 1, top.idle_o);
  zhao::check(top.occupancy_o == 0, "reset occupancy is zero", 0, top.occupancy_o);

  // ---- full payload identity and hold under randomized sink stalls ----------
  {
    constexpr int kCount = 192;
    std::deque<Acc> expected;
    int sent = 0;
    int returned = 0;
    int wrong = 0;
    int hold_wrong = 0;
    bool holding = false;
    Acc held{};
    uint32_t rng = 0x519D3A27u;

    for (int cycle = 0; cycle < 10000; ++cycle) {
      const bool feeding = sent < kCount;
      const Req r = make_req(sent);
      top.req_valid_i = feeding;
      if (feeding) drive(top, r);
      rng = rng * 1664525u + 1013904223u;
      top.acc_ready_i = ((rng >> 17) & 3u) != 0u;
      top.eval();

      // A stall creates an obligation which survives into the NEXT cycle.  Do
      // not guard that obligation with current valid: withdrawing valid is one
      // of the failures this check exists to catch.
      if (holding && !top.acc_valid_o) {
        ++hold_wrong;
        holding = false;  // count one withdrawal once; the suite is already red
      }
      if (top.acc_valid_o) {
        const Acc got = sample(top);
        if (holding && !exact(got, held)) ++hold_wrong;
        if (!top.acc_ready_i) {
          if (!holding) held = got;
          holding = true;
        } else {
          holding = false;
          if (expected.empty()) {
            ++wrong;
          } else {
            if (!exact(got, expected.front())) ++wrong;
            expected.pop_front();
            ++returned;
          }
        }
      }

      const bool accepted = feeding && top.req_ready_o;
      if (accepted) expected.push_back(expect_acc(r));
      zhao::tick(top);
      if (accepted) ++sent;
      if (sent == kCount && expected.empty() && top.idle_o) break;
    }
    top.req_valid_i = 0;
    top.eval();

    zhao::check(sent == kCount, "all planner requests were accepted", kCount, sent);
    zhao::check(returned == kCount, "all accepted requests reached the cache boundary", kCount,
                returned);
    zhao::check(wrong == 0,
                "addresses and exact route/palette identity survive every elastic stage", 0,
                wrong);
    zhao::check(hold_wrong == 0, "the complete cache-access packet holds while stalled", 0,
                hold_wrong);
    zhao::check(top.accepted_o == kCount, "accepted counter matches logical issues", kCount,
                top.accepted_o);
    zhao::check(top.idle_o == 1, "planner idle returns only after the held output drains", 1,
                top.idle_o);
  }

  // ---- all five stage credits, including the held output -------------------
  {
    reset(top);
    top.acc_ready_i = 0;
    int accepted = 0;
    for (int cycle = 0; cycle < 40; ++cycle) {
      const Req r = make_req(500 + accepted);
      top.req_valid_i = 1;
      drive(top, r);
      top.eval();
      const bool took = top.req_ready_o;
      zhao::tick(top);
      if (took) ++accepted;
    }
    top.req_valid_i = 0;
    top.eval();

    zhao::check(accepted == 5, "a stalled cache consumes exactly five elastic stage credits", 5,
                accepted);
    zhao::check(top.occupancy_o == 5, "all five stages are represented in occupancy", 5,
                top.occupancy_o);
    zhao::check(top.idle_o == 0, "a held access keeps structural idle low", 0, top.idle_o);

    top.acc_ready_i = 1;
    int drained = 0;
    for (int cycle = 0; cycle < 30 && drained < accepted; ++cycle) {
      top.eval();
      if (top.acc_valid_o) ++drained;
      zhao::tick(top);
    }
    top.eval();
    zhao::check(drained == accepted, "releasing the cache returns every stage credit", accepted,
                drained);
    zhao::check(top.idle_o == 1, "idle rises after the fifth held record leaves", 1, top.idle_o);
  }

  std::printf("  route identity width exercised through bit 17; palette pair held beside it\n");
  return zhao::report_and_exit("texture_tmu_plan_v2_directed");
}
