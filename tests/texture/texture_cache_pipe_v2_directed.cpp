// texture_cache_pipe_v2_directed.cpp
//
// Packet-B successor gate for exact 18-bit cache identity, held outputs,
// response-slot credits, and structural idle.  The Packet-E fill_refused_i pin
// is exercised as an explicitly inert ABI reservation: it neither fabricates a
// response nor clears the blocking miss in Packet B; eight real beats still owe
// the line.  Denial-safe completion remains Packet E's gate.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_cache_pipe_v2.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_texture_cache_pipe_v2;
constexpr int kReqDepth = 4;
constexpr int kRspDepth = 4;

uint16_t memory_at(uint32_t byte_addr) {
  return static_cast<uint16_t>(0x6A00u ^ ((byte_addr >> 1) * 37u));
}

void drive_access(Dut& top, uint32_t line, uint32_t route) {
  top.acc_en_i = 0xF;
  for (int lane = 0; lane < 4; ++lane)
    top.acc_addr_i[lane] = line + static_cast<uint32_t>(lane) * 2u;
  top.acc_src_id_i = route;
}

uint16_t lane(const Dut& top, int index) {
  return static_cast<uint16_t>((top.smp_data_o >> (16 * index)) & 0xFFFFu);
}

void reset(Dut& top) {
  top.acc_valid_i = 0;
  top.smp_ready_i = 1;
  top.fill_ready_i = 0;
  top.fill_data_valid_i = 0;
  top.fill_data_i = 0;
  top.fill_refused_i = 0;
  drive_access(top, 0, 0);
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

void send_fill(Dut& top, uint32_t base) {
  top.fill_refused_i = 0;
  top.fill_ready_i = 1;
  for (int beat = 0; beat < 8; ++beat) {
    top.fill_data_valid_i = 1;
    top.fill_data_i = memory_at(base + static_cast<uint32_t>(beat) * 2u);
    zhao::tick(top);
  }
  top.fill_data_valid_i = 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;

  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "reset leaves no accepted cache work or reserved response credit", 1,
              top.idle_o);

  // An unsolicited reserved Packet-E pulse is external presentation, not
  // accepted cache work.  Island public quiet observes it separately.
  top.fill_refused_i = 1;
  top.eval();
  zhao::check(top.idle_o == 1,
              "the reserved refusal input does not masquerade as accepted work in Packet B", 1,
              top.idle_o);
  zhao::tick(top);
  top.fill_refused_i = 0;

  // ---- two back-to-back cold probes: C3 miss squashes C3 plus younger C1 --
  {
    reset(top);
    top.acc_valid_i = 1;
    drive_access(top, 0x00610000u, 0x10111u);
    top.eval();
    zhao::check(top.acc_ready_o == 1, "first cold probe enters C0", 1, top.acc_ready_o);
    zhao::tick(top);

    // Keep the stream contiguous so the first request reaches C3 while the
    // second occupies C1.  Both probes already own response reservations and
    // both are rewound by that one miss.
    drive_access(top, 0x00620000u, 0x20222u);
    top.eval();
    zhao::check(top.acc_ready_o == 1, "second cold probe enters on the following clock", 1,
                top.acc_ready_o);
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int cycle = 0; cycle < 30 && !top.fill_valid_o; ++cycle) zhao::tick(top);
    top.eval();
    zhao::check(top.fill_valid_o == 1, "the first cold probe reaches the blocking miss", 1,
                top.fill_valid_o);
    zhao::check(top.replays_o == 2,
                "one miss counts the exact two-probe C3+C1 squash, not one miss event", 2,
                top.replays_o);
    zhao::check(top.fills_o == 1,
                "the two replay units came from one allocated line fill", 1, top.fills_o);
  }

  // ---- miss/fill request hold, inert refusal boundary, response hold --------
  {
    reset(top);
    constexpr uint32_t kLine = 0x00624000u;
    constexpr uint32_t kRoute = 0x312A5u;  // exercises both class bits and handle
    top.acc_valid_i = 1;
    drive_access(top, kLine, kRoute);
    top.eval();
    zhao::check(top.acc_ready_o == 1, "cold access enters the local request FIFO", 1,
                top.acc_ready_o);
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int cycle = 0; cycle < 30 && !top.fill_valid_o; ++cycle) zhao::tick(top);
    top.eval();
    zhao::check(top.fill_valid_o == 1, "a cold line produces a held fill request", 1,
                top.fill_valid_o);
    zhao::check(top.fill_addr_o == kLine, "fill address is the exact aligned missed line", kLine,
                top.fill_addr_o);
    zhao::check(top.idle_o == 0, "blocking miss and held fill request keep idle low", 0,
                top.idle_o);

    const uint32_t held_addr = top.fill_addr_o;
    for (int cycle = 0; cycle < 7; ++cycle) {
      top.fill_ready_i = 0;
      top.eval();
      zhao::check(top.fill_valid_o == 1 && top.fill_addr_o == held_addr,
                  "fill request valid/address hold under guard-side backpressure", 1,
                  (top.fill_valid_o && top.fill_addr_o == held_addr) ? 1 : 0);
      zhao::tick(top);
    }

    // Accept the request, then present the future denial pulse.  Packet B must
    // not claim the Packet-E behavior: no result and no cleared miss.
    top.fill_ready_i = 1;
    top.eval();
    zhao::tick(top);
    top.fill_ready_i = 0;
    top.fill_refused_i = 1;
    zhao::tick(top);
    top.fill_refused_i = 0;
    top.eval();
    zhao::check(top.smp_valid_o == 0,
                "reserved fill_refused_i does not fabricate a Packet-B terminal response", 0,
                top.smp_valid_o);
    zhao::check(top.idle_o == 0,
                "and it does not falsely clear the still-owed eight-beat fill", 0, top.idle_o);

    send_fill(top, kLine);
    for (int cycle = 0; cycle < 50 && !top.smp_valid_o; ++cycle) zhao::tick(top);
    top.smp_ready_i = 0;
    top.eval();
    zhao::check(top.smp_valid_o == 1, "eight real beats complete the reserved miss", 1,
                top.smp_valid_o);

    const uint64_t held_data = top.smp_data_o;
    const uint32_t held_route = top.smp_src_id_o;
    for (int cycle = 0; cycle < 9; ++cycle) {
      top.eval();
      zhao::check(top.smp_valid_o && top.smp_data_o == held_data &&
                      top.smp_src_id_o == held_route,
                  "data and complete 18-bit route identity hold as one response", 1,
                  (top.smp_valid_o && top.smp_data_o == held_data &&
                   top.smp_src_id_o == held_route) ? 1 : 0);
      zhao::tick(top);
    }

    bool data_ok = held_route == kRoute;
    for (int k = 0; k < 4; ++k)
      data_ok = data_ok && lane(top, k) == memory_at(kLine + static_cast<uint32_t>(k) * 2u);
    zhao::check(data_ok,
                "the stalled response keeps its own token and four requested halfwords", 1,
                data_ok ? 1 : 0);

    top.smp_ready_i = 1;
    zhao::tick(top);
    top.eval();
    zhao::check(top.idle_o == 1,
                "cache idle rises after request, pipeline, fill, response, and credit all drain", 1,
                top.idle_o);
  }

  // ---- warm hit burst: response reservations prevent silent identity loss --
  {
    // The preceding case left this line resident in all four lanes.
    constexpr uint32_t kLine = 0x00624000u;
    const uint32_t fills_before = top.fills_o;
    top.smp_ready_i = 0;
    std::vector<uint32_t> issued;
    constexpr int kCapacity = kReqDepth + kRspDepth;

    for (int cycle = 0; cycle < 200; ++cycle) {
      const uint32_t route = 0x20000u | static_cast<uint32_t>(0x500 + issued.size());
      top.acc_valid_i = 1;
      drive_access(top, kLine, route);
      top.eval();
      const bool took = top.acc_ready_o;
      zhao::tick(top);
      if (took) issued.push_back(route);
    }
    top.acc_valid_i = 0;
    top.eval();

    zhao::check(static_cast<int>(issued.size()) == kCapacity,
                "stalled consumer owns exactly request-FIFO plus response-FIFO credits", kCapacity,
                static_cast<int>(issued.size()));
    zhao::check(top.fills_o == fills_before,
                "the credit test is genuinely all-hit and never serialized by a fill", fills_before,
                top.fills_o);
    zhao::check(top.idle_o == 0, "queued responses or their reservations keep idle low", 0,
                top.idle_o);

    std::vector<uint32_t> returned;
    top.smp_ready_i = 1;
    for (int cycle = 0; cycle < 300 && returned.size() < issued.size(); ++cycle) {
      top.eval();
      if (top.smp_valid_o) returned.push_back(top.smp_src_id_o);
      zhao::tick(top);
    }
    top.eval();

    bool order_ok = returned.size() == issued.size();
    for (size_t i = 0; i < returned.size() && i < issued.size(); ++i)
      order_ok = order_ok && returned[i] == issued[i];
    zhao::check(returned.size() == issued.size(),
                "every reserved warm response returns exactly once", issued.size(),
                returned.size());
    zhao::check(order_ok,
                "18-bit route identities return in issue order with no younger substitution", 1,
                order_ok ? 1 : 0);
    zhao::check(top.idle_o == 1, "all response credits are returned at final drain", 1, top.idle_o);
  }

  std::printf("  Packet-E fill refusal pin present and intentionally inert; denial completion not claimed\n");
  return zhao::report_and_exit("texture_cache_pipe_v2_directed");
}
