// texture_rsp_dispatch_v2_directed.cpp
//
// Packet-B terminal collector gate for the exact final tuple:
//   {route_token18,status8,raw_index8,alpha8,RGB24}.
//
// Four class inputs can complete simultaneously; each owns one held reservation.
// The test checks fixed {ERR,BIL,NEAR,CLUT} pending bits, same-edge replacement,
// strict round-robin service while all classes remain active, full output holds,
// exact multi-accept/emission counters, and token-class mismatch detection without
// relabelling.  The same driver is the inverse-polarity control for independent
// dropped/reconstructed-index mutants.
#include <array>
#include <cstdint>
#include <cstdio>
#include <map>

#include "verilated.h"

#if defined(ZHAO_DROP_INDEX_MUTANT)
#include "Vzhao_texture_rsp_dispatch_v2_drop_index_mutant.h"
using Dut = Vzhao_texture_rsp_dispatch_v2_drop_index_mutant;
constexpr bool kExpectIndexCorruption = true;
constexpr const char* kTestName = "texture_rsp_dispatch_v2_drop_index_control";
#elif defined(ZHAO_RECONSTRUCT_INDEX_MUTANT)
#include "Vzhao_texture_rsp_dispatch_v2_reconstruct_index_mutant.h"
using Dut = Vzhao_texture_rsp_dispatch_v2_reconstruct_index_mutant;
constexpr bool kExpectIndexCorruption = true;
constexpr const char* kTestName = "texture_rsp_dispatch_v2_reconstruct_index_control";
#else
#include "Vzhao_texture_rsp_dispatch_v2.h"
using Dut = Vzhao_texture_rsp_dispatch_v2;
constexpr bool kExpectIndexCorruption = false;
constexpr const char* kTestName = "texture_rsp_dispatch_v2_directed";
#endif

#include "zhao_sim.hpp"

namespace {

constexpr int kClasses = 4;
constexpr int kClut = 0;
constexpr int kNear = 1;
constexpr int kBil = 2;
constexpr int kErr = 3;

struct Tuple {
  uint32_t route;
  uint8_t status;
  uint8_t index;
  uint8_t alpha;
  uint32_t rgb;
};

Tuple make_tuple(int serial, int token_class) {
  Tuple t{};
  t.route = (static_cast<uint32_t>(token_class) << 16) |
            static_cast<uint32_t>((0x3100 + serial * 73) & 0xFFFF);
  // Reserved status bits are zero at Packet-B producers, but the collector must
  // transport all eight rather than silently narrowing the type.
  t.status = static_cast<uint8_t>(0x80u ^ (serial * 29u));
  t.index = static_cast<uint8_t>((serial % 255) + 1);  // distinguishes DROP
  t.alpha = static_cast<uint8_t>(0xF1u - serial * 11u);
  t.rgb = (0x240000u + static_cast<uint32_t>(serial) * 0x010305u) & 0xFFFFFFu;
  if (t.index == static_cast<uint8_t>(t.rgb))
    t.rgb = (t.rgb & 0xFFFF00u) | static_cast<uint8_t>(t.index + 1u);  // distinguishes RECONSTRUCT
  return t;
}

void pack(VlWide<3>& words, const Tuple& t) {
  words[0] = (t.rgb & 0xFFFFFFu) | (static_cast<uint32_t>(t.alpha) << 24);
  words[1] = static_cast<uint32_t>(t.index) |
             (static_cast<uint32_t>(t.status) << 8) |
             ((t.route & 0xFFFFu) << 16);
  words[2] = (t.route >> 16) & 0x3u;
}

Tuple unpack(const VlWide<3>& words) {
  Tuple t{};
  t.rgb = words[0] & 0xFFFFFFu;
  t.alpha = static_cast<uint8_t>(words[0] >> 24);
  t.index = static_cast<uint8_t>(words[1]);
  t.status = static_cast<uint8_t>(words[1] >> 8);
  t.route = ((words[2] & 0x3u) << 16) | (words[1] >> 16);
  return t;
}

bool same_except_index(const Tuple& got, const Tuple& want) {
  return got.route == want.route && got.status == want.status &&
         got.alpha == want.alpha && got.rgb == want.rgb;
}

bool exact(const Tuple& a, const Tuple& b) {
  return same_except_index(a, b) && a.index == b.index;
}

void set_valid(Dut& top, int channel, bool value) {
  switch (channel) {
    case kClut: top.clut_valid_i = value; break;
    case kNear: top.near_valid_i = value; break;
    case kBil: top.bil_valid_i = value; break;
    default: top.err_valid_i = value; break;
  }
}

bool ready(const Dut& top, int channel) {
  switch (channel) {
    case kClut: return top.clut_ready_o != 0;
    case kNear: return top.near_ready_o != 0;
    case kBil: return top.bil_ready_o != 0;
    default: return top.err_ready_o != 0;
  }
}

void drive_tuple(Dut& top, int channel, const Tuple& tuple) {
  switch (channel) {
    case kClut: pack(top.clut_tuple_i, tuple); break;
    case kNear: pack(top.near_tuple_i, tuple); break;
    case kBil: pack(top.bil_tuple_i, tuple); break;
    default: pack(top.err_tuple_i, tuple); break;
  }
}

void reset(Dut& top) {
  top.clut_valid_i = 0;
  top.near_valid_i = 0;
  top.bil_valid_i = 0;
  top.err_valid_i = 0;
  top.out_ready_i = 1;
  for (int c = 0; c < kClasses; ++c) drive_tuple(top, c, Tuple{});
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

struct Score {
  int records = 0;
  int foreign = 0;
  int non_index_wrong = 0;
  int index_wrong = 0;
  std::map<uint32_t, Tuple> expected;

  void accept(const Tuple& value) { expected[value.route] = value; }

  void emit(const Tuple& got) {
    ++records;
    const auto it = expected.find(got.route);
    if (it == expected.end()) {
      ++foreign;
      return;
    }
    if (!same_except_index(got, it->second)) ++non_index_wrong;
    if (got.index != it->second.index) ++index_wrong;
    expected.erase(it);
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  Score score;

  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "reset empties all four reservations and the output hold", 1,
              top.idle_o);
  zhao::check(top.pending_valid_o == 0, "reset pending vector is zero", 0,
              top.pending_valid_o);

  // ---- simultaneous four-way reservation and output hold -------------------
  {
    reset(top);
    std::array<Tuple, kClasses> offered{};
    top.out_ready_i = 0;
    for (int c = 0; c < kClasses; ++c) {
      offered[c] = make_tuple(10 + c, c);
      set_valid(top, c, true);
      drive_tuple(top, c, offered[c]);
    }
    top.eval();
    bool all_ready = true;
    for (int c = 0; c < kClasses; ++c) all_ready = all_ready && ready(top, c);
    zhao::check(all_ready, "all four independent completion slots reserve simultaneously", 1,
                all_ready ? 1 : 0);
    zhao::tick(top);
    for (int c = 0; c < kClasses; ++c) {
      score.accept(offered[c]);
      set_valid(top, c, false);
    }
    top.eval();
    zhao::check(top.pending_valid_o == 0xF,
                "pending bits are exactly {ERR,BIL,NEAR,CLUT}", 0xF,
                top.pending_valid_o);
    zhao::check(top.out_valid_o == 0,
                "live input valid cannot bypass its reservation into output valid", 0,
                top.out_valid_o);

    // Empty output may reserve CLUT even while the external sink is closed.
    zhao::tick(top);
    top.eval();
    zhao::check(top.out_valid_o == 1 && top.pending_valid_o == 0xE,
                "first round-robin pick moves CLUT into the separate held output", 1,
                (top.out_valid_o && top.pending_valid_o == 0xE) ? 1 : 0);
    const Tuple held = unpack(top.out_tuple_o);
    int hold_wrong = 0;
    for (int cycle = 0; cycle < 9; ++cycle) {
      top.eval();
      if (!top.out_valid_o || !exact(unpack(top.out_tuple_o), held)) ++hold_wrong;
      zhao::tick(top);
    }
    zhao::check(hold_wrong == 0,
                "valid and every one of 66 output bits hold while ready is low", 0,
                hold_wrong);

    top.out_ready_i = 1;
    int local_outputs = 0;
    int class_order_wrong = 0;
    for (int cycle = 0; cycle < 30 && local_outputs < 4; ++cycle) {
      top.eval();
      if (top.out_valid_o) {
        const Tuple got = unpack(top.out_tuple_o);
        if (static_cast<int>(got.route >> 16) != local_outputs) ++class_order_wrong;
        score.emit(got);
        ++local_outputs;
      }
      zhao::tick(top);
    }
    top.eval();
    zhao::check(local_outputs == 4, "all four simultaneous reservations emit", 4,
                local_outputs);
    zhao::check(class_order_wrong == 0,
                "initial round-robin order is CLUT, NEAR, BIL, ERR", 0,
                class_order_wrong);
    zhao::check(top.accepted_o == 4, "multi-accept counter advances by four on one edge", 4,
                top.accepted_o);
    zhao::check(top.emitted_o == 4, "emission counter advances once per owner handshake", 4,
                top.emitted_o);
    zhao::check(top.class_mismatch_o == 0, "matching channels produce no identity fault", 0,
                top.class_mismatch_o);
    zhao::check(top.idle_o == 1, "idle rises only after reservations and output drain", 1,
                top.idle_o);
  }

  // ---- sustained all-class traffic: fair arbitration and same-edge refill --
  {
    reset(top);
    constexpr int kPerClass = 48;
    constexpr int kTotal = kPerClass * kClasses;
    std::array<int, kClasses> fed{};
    int outputs = 0;
    int rr_wrong = 0;
    int output_hold_wrong = 0;
    bool holding = false;
    Tuple held{};
    uint32_t rng = 0xD15A7C4Bu;

    for (int cycle = 0; cycle < 20000; ++cycle) {
      std::array<Tuple, kClasses> offered{};
      for (int c = 0; c < kClasses; ++c) {
        const bool active = fed[c] < kPerClass;
        set_valid(top, c, active);
        if (active) {
          offered[c] = make_tuple(1000 + c * kPerClass + fed[c], c);
          drive_tuple(top, c, offered[c]);
        }
      }
      rng = rng * 1664525u + 1013904223u;
      top.out_ready_i = ((rng >> 19) & 3u) != 0u;
      top.eval();

      if (top.out_valid_o) {
        const Tuple got = unpack(top.out_tuple_o);
        if (holding && !exact(got, held)) ++output_hold_wrong;
        if (!top.out_ready_i) {
          holding = true;
          held = got;
        } else {
          holding = false;
          // Until the final tail, every class has a replacement waiting and the
          // retained cursor must produce strict 0,1,2,3 service.
          if (outputs < kTotal - kClasses &&
              static_cast<int>(got.route >> 16) != (outputs & 3))
            ++rr_wrong;
          score.emit(got);
          ++outputs;
        }
      } else {
        holding = false;
      }

      std::array<bool, kClasses> accepted{};
      for (int c = 0; c < kClasses; ++c) {
        accepted[c] = fed[c] < kPerClass && ready(top, c);
        if (accepted[c]) score.accept(offered[c]);
      }
      zhao::tick(top);
      for (int c = 0; c < kClasses; ++c)
        if (accepted[c]) ++fed[c];

      bool all_fed = true;
      for (int c = 0; c < kClasses; ++c) all_fed = all_fed && fed[c] == kPerClass;
      if (all_fed && outputs == kTotal && top.idle_o) break;
    }
    for (int c = 0; c < kClasses; ++c) set_valid(top, c, false);
    top.eval();

    bool all_fed = true;
    for (int c = 0; c < kClasses; ++c) all_fed = all_fed && fed[c] == kPerClass;
    zhao::check(all_fed, "all four class producers completed their held streams", 1,
                all_fed ? 1 : 0);
    zhao::check(outputs == kTotal, "every accepted terminal tuple emitted exactly once", kTotal,
                outputs);
    zhao::check(rr_wrong == 0,
                "continuously occupied classes receive strict retained round-robin service", 0,
                rr_wrong);
    zhao::check(output_hold_wrong == 0,
                "random owner backpressure never changes the held final tuple", 0,
                output_hold_wrong);
    zhao::check(top.accepted_o == kTotal, "accepted counter includes same-edge slot replacement",
                kTotal, top.accepted_o);
    zhao::check(top.emitted_o == kTotal, "emitted counter closes exactly at drain", kTotal,
                top.emitted_o);
    zhao::check(top.class_mismatch_o == 0, "all correctly typed streams validate", 0,
                top.class_mismatch_o);
    zhao::check(top.idle_o == 1, "collector reaches structural idle after the mixed stream", 1,
                top.idle_o);
  }

  // ---- independent identity checker: count mismatch, never relabel ---------
  {
    reset(top);
    std::map<uint32_t, Tuple> local_expected;
    for (int channel = 0; channel < kClasses; ++channel) {
      const Tuple wrong_class = make_tuple(3000 + channel, (channel + 1) & 3);
      set_valid(top, channel, true);
      drive_tuple(top, channel, wrong_class);
      local_expected[wrong_class.route] = wrong_class;
      score.accept(wrong_class);
    }
    top.out_ready_i = 1;
    top.eval();
    zhao::tick(top);
    for (int c = 0; c < kClasses; ++c) set_valid(top, c, false);

    int outputs = 0;
    int relabelled = 0;
    for (int cycle = 0; cycle < 30 && outputs < 4; ++cycle) {
      top.eval();
      if (top.out_valid_o) {
        const Tuple got = unpack(top.out_tuple_o);
        if (local_expected.erase(got.route) != 1) ++relabelled;
        score.emit(got);
        ++outputs;
      }
      zhao::tick(top);
    }
    top.eval();
    zhao::check(outputs == 4, "mismatched inputs still receive terminal disposition", 4, outputs);
    zhao::check(top.class_mismatch_o == 4,
                "each token/channel disagreement increments the independently clocked checker", 4,
                top.class_mismatch_o);
    zhao::check(relabelled == 0 && local_expected.empty(),
                "the checker reports but never rewrites the original route token", 1,
                (relabelled == 0 && local_expected.empty()) ? 1 : 0);
    zhao::check(top.accepted_o == 4 && top.emitted_o == 4,
                "mismatch does not create a lifecycle hole or duplicate", 1,
                (top.accepted_o == 4 && top.emitted_o == 4) ? 1 : 0);
    zhao::check(top.idle_o == 1, "mismatched-but-preserved records drain normally", 1,
                top.idle_o);
  }

  zhao::check(score.foreign == 0, "no output tuple was invented or duplicated", 0,
              score.foreign);
  zhao::check(score.non_index_wrong == 0,
              "route, all status bits, alpha, and RGB preserve exact input alignment", 0,
              score.non_index_wrong);
  zhao::check(score.expected.empty(), "every accepted tuple was observed at output", 0,
              score.expected.size());
  if (kExpectIndexCorruption) {
    zhao::check(score.index_wrong == score.records,
                "inverse-polarity control: every discriminating raw index was corrupted",
                score.records, score.index_wrong);
  } else {
    zhao::check(score.index_wrong == 0,
                "sample-0 raw index survives class reservation and final arbitration", 0,
                score.index_wrong);
  }

  std::printf("  final tuple: route[65:48] status[47:40] index[39:32] alpha[31:24] RGB[23:0]\n");
  return zhao::report_and_exit(kTestName);
}
