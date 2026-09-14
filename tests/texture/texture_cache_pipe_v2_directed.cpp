// texture_cache_pipe_v2_directed.cpp
//
// Packet-E direct gate for typed cache fill termination. The production branch
// covers Packet-B timing/data/order plus refusal, protocol-fault positive
// controls, exact counters, finite drain, and response-reservation pressure.
// Two compile-time inverse branches drive the committed selector file and
// require one exact mutant signature with no unrelated fault or counter drift.
#if defined(EXPECT_PACKET_E_DENIAL_REPLAY_MUTANT) && \
    defined(EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT)
#error "PACKET_E_DRIVER_SELECTOR_COLLISION: define exactly one inverse branch"
#endif
#if defined(EXPECT_PACKET_E_LANES8_REQN2) && \
    (defined(EXPECT_PACKET_E_DENIAL_REPLAY_MUTANT) || \
     defined(EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT))
#error "PACKET_E_DRIVER_MODE_COLLISION: parameter and inverse controls are exclusive"
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_cache_pipe_v2.h"
#include "zhao_sim.hpp"

#ifdef EXPECT_PACKET_E_LANES8_REQN2
namespace {
using ParamDut = Vzhao_texture_cache_pipe_v2;
constexpr uint32_t kParamLine = 0x006D00A0u;

uint16_t param_memory_at(uint32_t byte_addr) {
  return static_cast<uint16_t>(0x6A00u ^ ((byte_addr >> 1) * 37u));
}

void param_check(bool cond, const char* what, uint64_t expected = 1,
                 uint64_t actual = 0) {
  zhao::check(cond, what, expected, cond ? expected : actual);
}

void param_reset(ParamDut& top) {
  top.clk = 0;
  top.rst_n = 0;
  top.acc_valid_i = 0;
  top.acc_en_i = 0;
  for (int lane = 0; lane < 8; ++lane) top.acc_addr_i[lane] = 0;
  top.acc_src_id_i = 0;
  top.smp_ready_i = 0;
  top.fill_ready_i = 0;
  top.fill_data_valid_i = 0;
  top.fill_data_i = 0;
  top.fill_refused_i = 0;
  top.frame_fault_clear_i = 0;
  top.eval();
  for (int cycle = 0; cycle < 6; ++cycle) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

void param_offer(ParamDut& top, uint32_t src) {
  top.acc_valid_i = 1;
  top.acc_en_i = 0xFF;
  for (int lane = 0; lane < 8; ++lane)
    top.acc_addr_i[lane] = kParamLine + static_cast<uint32_t>(lane) * 2u;
  top.acc_src_id_i = src;
  top.eval();
  param_check(top.acc_ready_o == 1, "LANES8 request accepts", 1, top.acc_ready_o);
  zhao::tick(top);
  top.acc_valid_i = 0;
}

bool param_wait_fill(ParamDut& top) {
  for (int cycle = 0; cycle < 100; ++cycle) {
    top.eval();
    if (top.fill_valid_o) return true;
    zhao::tick(top);
  }
  return false;
}

bool param_wait_response(ParamDut& top) {
  for (int cycle = 0; cycle < 150; ++cycle) {
    top.eval();
    if (top.smp_valid_o) return true;
    zhao::tick(top);
  }
  return false;
}

bool param_response_data_exact(const ParamDut& top) {
  for (int lane = 0; lane < 8; ++lane) {
    const uint32_t word = top.smp_data_o[lane / 2];
    const uint16_t got = static_cast<uint16_t>(word >> (16 * (lane & 1)));
    if (got != param_memory_at(kParamLine + static_cast<uint32_t>(lane) * 2u))
      return false;
  }
  return true;
}

bool param_wait_idle(ParamDut& top) {
  for (int cycle = 0; cycle < 150; ++cycle) {
    top.eval();
    if (top.idle_o) return true;
    zhao::tick(top);
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  ParamDut top;
  param_reset(top);
  param_check(top.idle_o == 1, "LANES8 reset is idle", 1, top.idle_o);

  constexpr uint32_t kColdSrc = 0x34567u;
  param_offer(top, kColdSrc);
  param_check(param_wait_fill(top), "LANES8 cold vector reaches fill");
  param_check(top.fill_addr_o == kParamLine, "LANES8 fill address exact", kParamLine,
              top.fill_addr_o);
  param_check(top.cache_misses_o == 8,
              "LANES8 miss popcount represents eight without wrapping", 8,
              top.cache_misses_o);
  param_check(top.multicast_o == 7,
              "LANES8 multicast popcount subtracts to seven without underflow", 7,
              top.multicast_o);
  param_check(top.fills_o == 1 && top.replays_o == 1,
              "LANES8 cold vector allocates one fill and one replay", 1,
              top.fills_o);

  top.fill_ready_i = 1;
  zhao::tick(top);
  top.fill_ready_i = 0;
  for (int beat = 0; beat < 8; ++beat) {
    top.fill_data_valid_i = 1;
    top.fill_data_i = param_memory_at(kParamLine + static_cast<uint32_t>(beat) * 2u);
    zhao::tick(top);
  }
  top.fill_data_valid_i = 0;
  param_check(param_wait_response(top), "LANES8 filled replay responds finitely");
  param_check(top.smp_status_o == 0 && top.smp_src_id_o == kColdSrc &&
                  param_response_data_exact(top),
              "LANES8 filled replay carries all eight exact lanes", 1,
              top.smp_status_o);
  param_check(top.cache_hits_o == 8,
              "LANES8 replay hit popcount represents eight without wrapping", 8,
              top.cache_hits_o);
  top.smp_ready_i = 1;
  zhao::tick(top);
  top.smp_ready_i = 0;
  param_check(param_wait_idle(top), "LANES8 cold vector drains finitely");

  constexpr uint32_t kWarmSrc = 0x12345u;
  param_offer(top, kWarmSrc);
  param_check(param_wait_response(top), "LANES8 warm vector responds finitely");
  param_check(top.smp_status_o == 0 && top.smp_src_id_o == kWarmSrc &&
                  param_response_data_exact(top),
              "LANES8 warm hit retains all lanes and identity", 1,
              top.smp_status_o);
  param_check(top.cache_hits_o == 16,
              "two LANES8 hit retirements accumulate sixteen exactly", 16,
              top.cache_hits_o);
  top.smp_ready_i = 1;
  zhao::tick(top);
  top.smp_ready_i = 0;
  param_check(param_wait_idle(top), "LANES8 final pointers and credits drain");
  const bool counters = top.cache_misses_o == 8 && top.cache_hits_o == 16 &&
      top.fills_o == 1 && top.multicast_o == 7 && top.replays_o == 1 &&
      top.cache_jobs_accepted_o == 2 && top.cache_jobs_completed_o == 2 &&
      top.fill_jobs_accepted_o == 1 && top.fill_jobs_completed_o == 1 &&
      top.fill_jobs_refused_o == 0 && top.fill_data_beats_o == 8 &&
      top.reservation_count_o == 0 && top.reservation_owner_state_o == 0 &&
      top.cache_work_state_o == 0 && top.fill_protocol_fault_o == 0;
  zhao::check(counters, "LANES8/REQN2 historical, new, reservation, and work evidence exact",
              1, counters ? 1 : 0);
  return zhao::report_and_exit("texture_cache_pipe_v2_lanes8_reqn2");
}

#else
namespace {

using Dut = Vzhao_texture_cache_pipe_v2;
constexpr int kReqDepth = 4;
constexpr int kRspDepth = 4;
constexpr uint8_t kSuccess = 0x00;
constexpr uint8_t kSourceRefused = 0x01;

struct Response {
  uint64_t data = 0;
  uint8_t status = 0;
  uint32_t src = 0;
};

struct CounterSnapshot {
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint32_t fills = 0;
  uint32_t multicast = 0;
  uint32_t replays = 0;
  uint32_t cache_accepted = 0;
  uint32_t cache_completed = 0;
  uint32_t fill_accepted = 0;
  uint32_t fill_completed = 0;
  uint32_t fill_refused = 0;
  uint32_t fill_beats = 0;
};

CounterSnapshot counters_of(const Dut& top) {
  return CounterSnapshot{
      top.cache_hits_o,
      top.cache_misses_o,
      top.fills_o,
      top.multicast_o,
      top.replays_o,
      top.cache_jobs_accepted_o,
      top.cache_jobs_completed_o,
      top.fill_jobs_accepted_o,
      top.fill_jobs_completed_o,
      top.fill_jobs_refused_o,
      top.fill_data_beats_o};
}

bool counters_equal(const CounterSnapshot& a, const CounterSnapshot& b) {
  return a.hits == b.hits && a.misses == b.misses &&
      a.fills == b.fills && a.multicast == b.multicast &&
      a.replays == b.replays && a.cache_accepted == b.cache_accepted &&
      a.cache_completed == b.cache_completed &&
      a.fill_accepted == b.fill_accepted &&
      a.fill_completed == b.fill_completed &&
      a.fill_refused == b.fill_refused && a.fill_beats == b.fill_beats;
}

uint16_t memory_at(uint32_t byte_addr) {
  return static_cast<uint16_t>(0x6A00u ^ ((byte_addr >> 1) * 37u));
}

void expect(bool cond, const char* what, uint64_t expected = 1, uint64_t actual = 0) {
  zhao::check(cond, what, expected, cond ? expected : actual);
}

void drive_access(Dut& top, uint32_t line, uint32_t route,
                  uint8_t enables = 0xF) {
  top.acc_en_i = enables;
  for (int lane = 0; lane < 4; ++lane)
    top.acc_addr_i[lane] = line + static_cast<uint32_t>(lane) * 2u;
  top.acc_src_id_i = route;
}

uint16_t response_lane(const Response& rsp, int lane) {
  return static_cast<uint16_t>((rsp.data >> (16 * lane)) & 0xFFFFu);
}

bool response_matches_line(const Response& rsp, uint32_t line) {
  if (rsp.status != kSuccess) return false;
  for (int lane = 0; lane < 4; ++lane) {
    if (response_lane(rsp, lane) !=
        memory_at(line + static_cast<uint32_t>(lane) * 2u))
      return false;
  }
  return true;
}

void reset(Dut& top) {
  top.clk = 0;
  top.rst_n = 0;
  top.acc_valid_i = 0;
  top.acc_en_i = 0;
  for (int lane = 0; lane < 4; ++lane) top.acc_addr_i[lane] = 0;
  top.acc_src_id_i = 0;
  top.smp_ready_i = 0;
  top.fill_ready_i = 0;
  top.fill_data_valid_i = 0;
  top.fill_data_i = 0;
  top.fill_refused_i = 0;
  top.frame_fault_clear_i = 0;
  top.eval();
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
  top.eval();
  expect(top.idle_o == 1, "reset leaves exact structural idle", 1, top.idle_o);
  expect(top.fill_protocol_fault_o == 0, "reset clears fill protocol fault", 0,
         top.fill_protocol_fault_o);
}

void offer_once(Dut& top, uint32_t line, uint32_t route,
                uint8_t enables = 0xF) {
  top.acc_valid_i = 1;
  drive_access(top, line, route, enables);
  top.eval();
  expect(top.acc_ready_o == 1, "request offer is accepted", 1, top.acc_ready_o);
  zhao::tick(top);
  top.acc_valid_i = 0;
}

bool wait_fill(Dut& top, int limit = 100) {
  for (int cycle = 0; cycle < limit; ++cycle) {
    top.eval();
    if (top.fill_valid_o) return true;
    zhao::tick(top);
  }
  return false;
}

void accept_fill(Dut& top) {
  const uint32_t before = top.fill_jobs_accepted_o;
  top.fill_ready_i = 1;
  top.eval();
  expect(top.fill_valid_o == 1, "FI accepts an actually held fill request", 1,
         top.fill_valid_o);
  zhao::tick(top);
  top.fill_ready_i = 0;
  top.eval();
  expect(top.fill_jobs_accepted_o == before + 1u,
         "fill accepted counter advances exactly on FI", before + 1u,
         top.fill_jobs_accepted_o);
}

void send_beats(Dut& top, uint32_t line, int count) {
  for (int beat = 0; beat < count; ++beat) {
    top.fill_data_valid_i = 1;
    top.fill_data_i = memory_at(line + static_cast<uint32_t>(beat) * 2u);
    zhao::tick(top);
  }
  top.fill_data_valid_i = 0;
}

void pulse_data(Dut& top, uint16_t data, bool clear_same_edge = false) {
  top.fill_data_valid_i = 1;
  top.fill_data_i = data;
  top.frame_fault_clear_i = clear_same_edge ? 1 : 0;
  zhao::tick(top);
  top.fill_data_valid_i = 0;
  top.frame_fault_clear_i = 0;
}

void pulse_refusal(Dut& top, bool with_data = false,
                   bool clear_same_edge = false) {
  top.fill_refused_i = 1;
  top.fill_data_valid_i = with_data ? 1 : 0;
  top.fill_data_i = 0xD15Au;
  top.frame_fault_clear_i = clear_same_edge ? 1 : 0;
  zhao::tick(top);
  top.fill_refused_i = 0;
  top.fill_data_valid_i = 0;
  top.frame_fault_clear_i = 0;
}

bool wait_response(Dut& top, int limit = 150) {
  for (int cycle = 0; cycle < limit; ++cycle) {
    top.eval();
    if (top.smp_valid_o) return true;
    zhao::tick(top);
  }
  return false;
}

Response observe_response(Dut& top) {
  top.eval();
  expect(top.smp_valid_o == 1, "response observation has held valid", 1,
         top.smp_valid_o);
  return Response{static_cast<uint64_t>(top.smp_data_o),
                  static_cast<uint8_t>(top.smp_status_o),
                  static_cast<uint32_t>(top.smp_src_id_o)};
}

void pop_response(Dut& top) {
  top.smp_ready_i = 1;
  zhao::tick(top);
  top.smp_ready_i = 0;
}

bool wait_idle(Dut& top, int limit = 200) {
  for (int cycle = 0; cycle < limit; ++cycle) {
    top.eval();
    if (top.idle_o) return true;
    zhao::tick(top);
  }
  return false;
}

void quiet_clear(Dut& top) {
  top.eval();
  expect(top.idle_o == 1, "quiet clear is presented only at exact idle", 1,
         top.idle_o);
  top.frame_fault_clear_i = 1;
  zhao::tick(top);
  top.frame_fault_clear_i = 0;
  top.eval();
  expect(top.fill_protocol_fault_o == 0, "quiet frame clear clears only the fault", 0,
         top.fill_protocol_fault_o);
}

void expect_typed(const Response& rsp, uint32_t src, uint8_t status,
                  uint64_t data, const char* what) {
  const bool exact = rsp.src == src && rsp.status == status && rsp.data == data;
  zhao::check(exact, what, 1, exact ? 1 : 0);
}

void expect_legal_drain(const Dut& top, const char* what) {
  const uint32_t successful =
      top.fill_jobs_completed_o - top.fill_jobs_refused_o;
  const bool exact = top.idle_o &&
      top.cache_jobs_accepted_o == top.cache_jobs_completed_o &&
      top.fill_jobs_accepted_o == top.fill_jobs_completed_o &&
      top.fill_data_beats_o == 8u * successful &&
      top.reservation_count_o == 0 &&
      top.reservation_owner_state_o == 0 && top.cache_work_state_o == 0;
  zhao::check(exact, what, 1, exact ? 1 : 0);
}

void test_success_younger_multicast_replay_and_warm_hold() {
  Dut top;
  reset(top);
  constexpr uint32_t kA = 0x00624000u;
  constexpr uint32_t kB = 0x00624010u;
  constexpr uint32_t kSrcA = 0x30111u;
  constexpr uint32_t kSrcB = 0x10222u;
  constexpr uint32_t kSrcWarm = 0x20333u;

  // Contiguous offers put A in C2 and younger B in C1 at A's miss edge.
  offer_once(top, kA, kSrcA);
  offer_once(top, kB, kSrcB);
  expect(wait_fill(top), "cold A reaches a finite blocking fill");
  top.eval();
  expect(top.fill_addr_o == kA, "first miss identity is exact A line", kA,
         top.fill_addr_o);
  expect(top.replays_o == 2, "A miss squashes exact C2 plus younger C1 probes", 2,
         top.replays_o);
  expect(top.fills_o == 1, "A multicast allocates one historical fill", 1,
         top.fills_o);
  expect(top.cache_misses_o == 4, "A multicast counts four lane misses", 4,
         top.cache_misses_o);
  expect(top.multicast_o == 3, "one A fill serves three extra lanes", 3,
         top.multicast_o);
  for (int cycle = 0; cycle < 5; ++cycle) {
    top.eval();
    expect(top.fill_valid_o == 1 && top.fill_addr_o == kA,
           "fill valid/address hold together before FI", 1, top.fill_valid_o);
    zhao::tick(top);
  }

  accept_fill(top);
  send_beats(top, kA, 8);
  expect(top.fill_jobs_completed_o == 1, "beat eight is one successful FTERM", 1,
         top.fill_jobs_completed_o);
  expect(top.fill_data_beats_o == 8, "A counts exactly eight legal fill beats", 8,
         top.fill_data_beats_o);

  expect(wait_response(top), "prepaid A replay returns finitely");
  const Response held = observe_response(top);
  expect(held.src == kSrcA && response_matches_line(held, kA),
         "successful A replay has status zero, original token, and exact data", 1,
         held.src);

  // Hold the complete triple while the younger request proceeds to its miss.
  bool b_fill_seen = false;
  for (int cycle = 0; cycle < 20; ++cycle) {
    top.eval();
    const Response now = observe_response(top);
    expect_typed(now, held.src, held.status, held.data,
                 "data/status/token hold together under response backpressure");
    if (top.fill_valid_o) {
      b_fill_seen = true;
      break;
    }
    zhao::tick(top);
  }
  expect(b_fill_seen, "younger B continues into a fill while A response is stalled");
  expect(top.fill_addr_o == kB, "younger B retains its own missed line", kB,
         top.fill_addr_o);
  expect(top.replays_o == 3, "B contributes exactly one later replay probe", 3,
         top.replays_o);
  pop_response(top);

  accept_fill(top);
  send_beats(top, kB, 8);
  expect(wait_response(top), "younger B returns after its own fill");
  const Response b = observe_response(top);
  expect(b.src == kSrcB && response_matches_line(b, kB),
         "younger B response keeps exact order/status/token/data", 1, b.src);
  pop_response(top);

  const uint32_t fills_before_warm = top.fills_o;
  offer_once(top, kA, kSrcWarm);
  expect(wait_response(top), "warm A hit returns without fill");
  const Response warm = observe_response(top);
  expect(warm.src == kSrcWarm && response_matches_line(warm, kA),
         "warm hit preserves Packet-B data and 18-bit order", 1, warm.src);
  expect(top.fills_o == fills_before_warm, "warm hit allocates no new fill",
         fills_before_warm, top.fills_o);
  pop_response(top);

  expect(wait_idle(top), "success/multicast/replay scenario drains finitely");
  expect(top.fill_protocol_fault_o == 0, "legal success path raises no protocol fault", 0,
         top.fill_protocol_fault_o);
  expect(top.cache_hits_o == 12, "three four-lane responses count exact hits", 12,
         top.cache_hits_o);
  expect(top.cache_jobs_accepted_o == 3 && top.cache_jobs_completed_o == 3,
         "CA and CC are exact after success drain", 3,
         top.cache_jobs_completed_o);
  expect(top.fill_jobs_accepted_o == 2 && top.fill_jobs_completed_o == 2 &&
             top.fill_jobs_refused_o == 0 && top.fill_data_beats_o == 16,
         "FI/FTERM/FREF/FB exact for two successful lines", 1,
         top.fill_jobs_completed_o);
  expect_legal_drain(top, "legal success drain proves CA=CC, FI=FTERM, FB=8*FOK");
}

void test_refusal_skips_head_and_resumes_younger() {
  Dut top;
  reset(top);
  constexpr uint32_t kDenied = 0x00631000u;
  constexpr uint32_t kYounger = 0x00631010u;
  constexpr uint32_t kDeniedSrc = 0x312A5u;
  constexpr uint32_t kYoungSrc = 0x055AAu;

  offer_once(top, kDenied, kDeniedSrc);
  offer_once(top, kYounger, kYoungSrc);
  expect(wait_fill(top), "denied head reaches its fill request");
  expect(top.fill_addr_o == kDenied, "denied fill names the original head", kDenied,
         top.fill_addr_o);
  accept_fill(top);
  pulse_refusal(top);
  expect(wait_response(top), "accepted refusal emits one response finitely");
  const Response denied = observe_response(top);
  expect_typed(denied, kDeniedSrc, kSourceRefused, 0,
               "refusal emits zero data, SOURCE_REFUSED, original 18-bit token");
  expect(top.fill_protocol_fault_o == 0, "zero-beat legal refusal is not malformed", 0,
         top.fill_protocol_fault_o);

  // The refused response may remain stalled while the younger head progresses.
  expect(wait_fill(top), "younger head resumes despite stalled refusal response");
  expect(top.fill_addr_o == kYounger,
         "refusal advances past exactly the denied head, never replaying it", kYounger,
         top.fill_addr_o);
  pop_response(top);
  accept_fill(top);
  send_beats(top, kYounger, 8);
  expect(wait_response(top), "younger request completes after denied head");
  const Response younger = observe_response(top);
  expect(younger.src == kYoungSrc && response_matches_line(younger, kYounger),
         "younger success keeps status zero and its own identity", 1, younger.src);
  pop_response(top);

  expect(wait_idle(top), "refusal plus younger work drains finitely");
  expect(top.cache_jobs_accepted_o == 2 && top.cache_jobs_completed_o == 2,
         "refusal is one completed cache job, not a dropped response", 2,
         top.cache_jobs_completed_o);
  expect(top.fill_jobs_accepted_o == 2 && top.fill_jobs_completed_o == 2 &&
             top.fill_jobs_refused_o == 1 && top.fill_data_beats_o == 8,
         "refusal/success FI/FTERM/FREF/FB counters are exact", 1,
         top.fill_jobs_refused_o);
  expect_legal_drain(top, "legal mixed drain proves FI=FTERM and FB=8*(FTERM-FREF)");
}

void test_stalled_response_with_new_inputs() {
  Dut top;
  reset(top);
  constexpr uint32_t kLine = 0x00642020u;

  // Seed one resident multicast line and consume its response.
  offer_once(top, kLine, 0x10001u);
  expect(wait_fill(top), "capacity test seed misses");
  accept_fill(top);
  send_beats(top, kLine, 8);
  expect(wait_response(top), "capacity seed responds");
  pop_response(top);
  expect(wait_idle(top), "capacity seed drains");

  std::vector<uint32_t> issued;
  top.smp_ready_i = 0;
  for (int cycle = 0; cycle < 200; ++cycle) {
    const uint32_t route = 0x20000u + static_cast<uint32_t>(issued.size());
    top.acc_valid_i = 1;
    drive_access(top, kLine, route);
    top.eval();
    const bool took = top.acc_ready_o;
    zhao::tick(top);
    if (took) issued.push_back(route);
  }
  top.acc_valid_i = 0;
  top.eval();
  expect(issued.size() == static_cast<size_t>(kReqDepth + kRspDepth),
         "stalled sink owns exactly request plus response capacity", kReqDepth + kRspDepth,
         issued.size());
  expect(top.fills_o == 1, "stalled all-hit burst preserves warm-cache timing", 1,
         top.fills_o);

  std::vector<uint32_t> returned;
  std::vector<uint8_t> statuses;
  top.smp_ready_i = 1;
  for (int cycle = 0; cycle < 400 && returned.size() < issued.size(); ++cycle) {
    top.eval();
    if (top.smp_valid_o) {
      returned.push_back(top.smp_src_id_o);
      statuses.push_back(top.smp_status_o);
    }
    zhao::tick(top);
  }
  top.smp_ready_i = 0;
  bool exact = returned == issued && statuses.size() == issued.size();
  for (uint8_t status : statuses) exact = exact && status == kSuccess;
  expect(exact, "stalled responses plus new inputs preserve every token/status in order", 1,
         returned.size());
  expect(wait_idle(top), "credit-pressure burst drains all pointers and reservations");
  expect(top.cache_jobs_accepted_o == 9 && top.cache_jobs_completed_o == 9,
         "capacity burst CA/CC includes seed plus exact eight responses", 9,
         top.cache_jobs_completed_o);
  expect_legal_drain(top, "credit-pressure legal drain retains all counter equations");
}

void test_data_before_fi_and_quiet_clear() {
  Dut top;
  reset(top);
  constexpr uint32_t kLine = 0x00653030u;
  constexpr uint32_t kSrc = 0x2A55Au;
  offer_once(top, kLine, kSrc);
  expect(wait_fill(top), "pre-FI data case reaches held fill offer");

  pulse_data(top, 0xDEADu, true);
  expect(top.fill_protocol_fault_o == 1,
         "data before FI sets fault even against same-edge clear", 1,
         top.fill_protocol_fault_o);
  expect(top.fill_data_beats_o == 0, "pre-FI malformed data is not counted", 0,
         top.fill_data_beats_o);
  expect(top.fill_valid_o == 1, "pre-FI malformed data neither accepts nor terminates fill", 1,
         top.fill_valid_o);

  top.frame_fault_clear_i = 1;
  zhao::tick(top);
  top.frame_fault_clear_i = 0;
  expect(top.fill_protocol_fault_o == 1, "clear cannot erase fault while accepted work exists", 1,
         top.fill_protocol_fault_o);

  accept_fill(top);
  send_beats(top, kLine, 8);
  expect(wait_response(top), "fill recovers after malformed pre-FI data");
  const Response rsp = observe_response(top);
  expect(rsp.src == kSrc && response_matches_line(rsp, kLine),
         "malformed pre-FI word never wrote RAM or contaminated legal response", 1,
         rsp.data);

  const uint32_t ca = top.cache_jobs_accepted_o;
  const uint32_t cc = top.cache_jobs_completed_o;
  const uint32_t fi = top.fill_jobs_accepted_o;
  const uint32_t ft = top.fill_jobs_completed_o;
  const uint32_t fb = top.fill_data_beats_o;
  const Response held = rsp;
  top.frame_fault_clear_i = 1;
  zhao::tick(top);
  top.frame_fault_clear_i = 0;
  const Response still = observe_response(top);
  expect_typed(still, held.src, held.status, held.data,
               "busy clear changes neither held payload, status, nor token");
  expect(top.fill_protocol_fault_o == 1, "held response blocks fault clear", 1,
         top.fill_protocol_fault_o);
  expect(top.cache_jobs_accepted_o == ca && top.cache_jobs_completed_o == cc &&
             top.fill_jobs_accepted_o == fi && top.fill_jobs_completed_o == ft &&
             top.fill_data_beats_o == fb,
         "frame clear changes no work counter", 1, top.fill_data_beats_o);

  pop_response(top);
  expect(wait_idle(top), "pre-FI recovery drains before clear");
  quiet_clear(top);
  expect(top.cache_jobs_accepted_o == ca && top.cache_jobs_completed_o == ca &&
             top.fill_jobs_accepted_o == fi && top.fill_jobs_completed_o == ft &&
             top.fill_data_beats_o == fb,
         "quiet clear changes only fault, never counters", 1,
         top.cache_jobs_completed_o);
}

void test_refusal_before_fi_and_no_miss() {
  Dut top;
  reset(top);

  pulse_refusal(top, false, true);
  expect(top.fill_protocol_fault_o == 1,
         "refusal with no miss sets fault and beats same-edge clear", 1,
         top.fill_protocol_fault_o);
  expect(top.fill_jobs_completed_o == 0 && top.fill_jobs_refused_o == 0,
         "no-miss refusal is observation only, never a terminal", 0,
         top.fill_jobs_completed_o);
  quiet_clear(top);

  constexpr uint32_t kLine = 0x00664040u;
  constexpr uint32_t kSrc = 0x15555u;
  offer_once(top, kLine, kSrc);
  expect(wait_fill(top), "pre-FI refusal case reaches fill offer");
  pulse_refusal(top);
  expect(top.fill_protocol_fault_o == 1, "refusal before FI independently sets fault", 1,
         top.fill_protocol_fault_o);
  expect(top.fill_valid_o == 1 && top.fill_jobs_accepted_o == 0 &&
             top.fill_jobs_completed_o == 0,
         "pre-FI refusal leaves request held and counters untouched", 1,
         top.fill_valid_o);

  accept_fill(top);
  pulse_refusal(top);
  expect(wait_response(top), "later legal refusal still terminates exactly once");
  const Response rsp = observe_response(top);
  expect_typed(rsp, kSrc, kSourceRefused, 0,
               "legal refusal after pre-FI fault has exact typed response");
  pop_response(top);
  expect(wait_idle(top), "pre-FI refusal recovery drains finitely");
  quiet_clear(top);
  expect(top.fill_jobs_accepted_o == 1 && top.fill_jobs_completed_o == 1 &&
             top.fill_jobs_refused_o == 1 && top.fill_data_beats_o == 0,
         "only the later accepted refusal changes fill counters", 1,
         top.fill_jobs_refused_o);
}

void test_simultaneous_data_refusal() {
  Dut top;
  reset(top);
  constexpr uint32_t kLine = 0x00675050u;
  constexpr uint32_t kSrc = 0x3ABCDu;
  offer_once(top, kLine, kSrc);
  expect(wait_fill(top), "simultaneous case reaches fill");
  accept_fill(top);
  pulse_refusal(top, true, true);
  expect(top.fill_protocol_fault_o == 1,
         "simultaneous data/refusal sets fault over clear", 1,
         top.fill_protocol_fault_o);
  expect(top.fill_data_beats_o == 0, "refusal wins and simultaneous data is not counted", 0,
         top.fill_data_beats_o);
  expect(top.fill_jobs_completed_o == 1 && top.fill_jobs_refused_o == 1,
         "simultaneous collision still has exactly one refused terminal", 1,
         top.fill_jobs_completed_o);
  expect(wait_response(top), "simultaneous refusal response arrives");
  expect_typed(observe_response(top), kSrc, kSourceRefused, 0,
               "simultaneous collision returns only typed refusal");
  pop_response(top);
  expect(wait_idle(top), "simultaneous collision drains finitely");
  quiet_clear(top);
}

void test_partial_refusal_invalidates_then_refills() {
  Dut top;
  reset(top);
  constexpr uint32_t kLine = 0x00686060u;
  constexpr uint32_t kDeniedSrc = 0x01111u;
  constexpr uint32_t kRetrySrc = 0x32222u;

  offer_once(top, kLine, kDeniedSrc);
  expect(wait_fill(top), "partial-refusal case reaches fill");
  accept_fill(top);
  send_beats(top, kLine, 3);
  expect(top.fill_data_beats_o == 3, "three pre-refusal legal beats are counted", 3,
         top.fill_data_beats_o);
  pulse_refusal(top);
  expect(top.fill_protocol_fault_o == 1, "partial-line refusal independently sets fault", 1,
         top.fill_protocol_fault_o);
  expect(top.fill_jobs_completed_o == 1 && top.fill_jobs_refused_o == 1,
         "partial refusal still terminates exactly once", 1,
         top.fill_jobs_completed_o);
  expect(wait_response(top), "partial refusal emits typed response");
  expect_typed(observe_response(top), kDeniedSrc, kSourceRefused, 0,
               "partial refusal preserves denied identity and zero data");
  pop_response(top);
  expect(wait_idle(top), "partial refusal drains before retry");
  quiet_clear(top);

  // If any partial line was accidentally published, this would hit. It must
  // allocate a fresh fill, then return all eight newly accepted words.
  offer_once(top, kLine, kRetrySrc);
  expect(wait_fill(top), "retry of partial line must miss and refill");
  expect(top.fill_addr_o == kLine, "fresh refill keeps exact invalidated line", kLine,
         top.fill_addr_o);
  expect(top.fills_o == 2, "partial refusal followed by retry allocates two fills", 2,
         top.fills_o);
  accept_fill(top);
  send_beats(top, kLine, 8);
  expect(wait_response(top), "fresh full refill returns");
  const Response retry = observe_response(top);
  expect(retry.src == kRetrySrc && response_matches_line(retry, kLine),
         "fresh refill proves partial content remained invalid", 1, retry.data);
  pop_response(top);
  expect(wait_idle(top), "partial-refill scenario reaches finite idle");
  expect(top.fill_data_beats_o == 11,
         "FB records three legal partial words plus eight successful words", 11,
         top.fill_data_beats_o);
}

void test_ninth_unsolicited_and_duplicate_refusal() {
  {
    Dut top;
    reset(top);
    constexpr uint32_t kLine = 0x00697070u;
    constexpr uint32_t kSrc = 0x24444u;
    offer_once(top, kLine, kSrc);
    expect(wait_fill(top), "ninth-beat case reaches fill");
    accept_fill(top);
    send_beats(top, kLine, 8);
    pulse_data(top, 0xBEEFu, true);
    expect(top.fill_protocol_fault_o == 1,
           "ninth/unsolicited data sets fault over same-edge clear", 1,
           top.fill_protocol_fault_o);
    expect(top.fill_data_beats_o == 8, "ninth data is neither counted nor written", 8,
           top.fill_data_beats_o);
    expect(top.fill_jobs_completed_o == 1 && top.fill_jobs_refused_o == 0,
           "ninth data cannot create a second fill terminal", 1,
           top.fill_jobs_completed_o);
    expect(wait_response(top), "successful response survives ninth malformed beat");
    const Response rsp = observe_response(top);
    expect(rsp.src == kSrc && response_matches_line(rsp, kLine),
           "ninth malformed word cannot alter published successful line", 1, rsp.data);

    const uint32_t ca = top.cache_jobs_accepted_o;
    const uint32_t fi = top.fill_jobs_accepted_o;
    const uint32_t ft = top.fill_jobs_completed_o;
    const uint32_t fb = top.fill_data_beats_o;
    top.frame_fault_clear_i = 1;
    zhao::tick(top);
    top.frame_fault_clear_i = 0;
    expect_typed(observe_response(top), rsp.src, rsp.status, rsp.data,
                 "clear while stalled holds full successful response triple");
    expect(top.fill_protocol_fault_o == 1, "stalled response prevents quiet clear", 1,
           top.fill_protocol_fault_o);
    expect(top.cache_jobs_accepted_o == ca && top.fill_jobs_accepted_o == fi &&
               top.fill_jobs_completed_o == ft && top.fill_data_beats_o == fb,
           "busy clear leaves ninth-case counters untouched", 1,
           top.fill_data_beats_o);
    pop_response(top);
    expect(wait_idle(top), "ninth case drains finitely");
    quiet_clear(top);
  }

  {
    Dut top;
    reset(top);
    constexpr uint32_t kLine = 0x006A8080u;
    constexpr uint32_t kSrc = 0x36666u;
    offer_once(top, kLine, kSrc);
    expect(wait_fill(top), "duplicate-refusal case reaches fill");
    accept_fill(top);
    pulse_refusal(top);
    expect(wait_response(top), "first refusal terminates normally");
    const Response first = observe_response(top);
    pulse_refusal(top);
    expect(top.fill_protocol_fault_o == 1, "duplicate refusal independently sets fault", 1,
           top.fill_protocol_fault_o);
    expect_typed(observe_response(top), first.src, first.status, first.data,
                 "duplicate refusal cannot alter or duplicate held terminal");
    expect(top.fill_jobs_completed_o == 1 && top.fill_jobs_refused_o == 1,
           "duplicate refusal changes no terminal counter", 1,
           top.fill_jobs_completed_o);
    pop_response(top);
    expect(wait_idle(top), "duplicate refusal case drains finitely");
    quiet_clear(top);
  }

  {
    Dut top;
    reset(top);
    pulse_data(top, 0xCAFEu);
    expect(top.fill_protocol_fault_o == 1, "unsolicited idle data independently sets fault", 1,
           top.fill_protocol_fault_o);
    expect(top.fill_data_beats_o == 0 && top.fill_jobs_completed_o == 0,
           "unsolicited idle data changes no fill accounting", 0,
           top.fill_data_beats_o);
    quiet_clear(top);
  }
}

void run_denial_replay_mutant() {
  Dut top;
  reset(top);
  constexpr uint32_t kDenied = 0x006B9000u;
  constexpr uint32_t kYounger = 0x006B9010u;
  constexpr uint32_t kDeniedSrc = 0x31111u;
  offer_once(top, kDenied, kDeniedSrc);
  offer_once(top, kYounger, 0x12222u);
  expect(wait_fill(top), "mutant first denied fill appears");
  accept_fill(top);
  pulse_refusal(top);
  expect(wait_response(top), "mutant still emits the first typed refusal");
  expect_typed(observe_response(top), kDeniedSrc, kSourceRefused, 0,
               "mutant first refusal response remains otherwise exact");
  pop_response(top);
  expect(wait_fill(top), "denial-replay mutant exposes a second fill");
  const CounterSnapshot observed = counters_of(top);
  const CounterSnapshot expected{
      0,  // hits: neither denied probe can retire
      8,  // misses: four lanes on each of two probes
      2,  // fills: original plus replayed denial
      6,  // multicast: three extra lanes per fill
      4,  // replays: C2+C1 on both misses
      2, 1,  // cache accepted/completed
      1, 1, 1, 0  // fill accepted/completed/refused/beats
  };
  const bool exact = top.fill_addr_o == kDenied &&
      counters_equal(observed, expected) &&
      top.reservation_count_o == 1 && top.reservation_owner_state_o == 8 &&
      top.cache_work_state_o == 0x0B1 &&
      top.fill_protocol_fault_o == 0 && top.fill_valid_o == 1 &&
      top.smp_valid_o == 0 && top.idle_o == 0;
  zhao::check(exact,
      "denial-replay mutant has exact historical/new counters and only second denied fill state",
      1, exact ? 1 : 0);
}

void run_prepaid_double_reservation_mutant() {
  Dut top;
  reset(top);
  constexpr uint32_t kLine = 0x006CA000u;
  constexpr uint32_t kSrc = 0x2D00Du;
  offer_once(top, kLine, kSrc);
  expect(wait_fill(top), "double-reservation mutant reaches fill");
  accept_fill(top);
  send_beats(top, kLine, 8);
  expect(wait_response(top), "double-reservation mutant still returns exact payload");
  const Response rsp = observe_response(top);
  expect(rsp.src == kSrc && response_matches_line(rsp, kLine),
         "double-reservation mutant changes no response data/status/token", 1, rsp.data);
  pop_response(top);
  for (int cycle = 0; cycle < 80; ++cycle) zhao::tick(top);
  top.eval();
  const CounterSnapshot expected{
      4, 4, 1, 3, 1,  // hits/misses/fills/multicast/replays
      1, 1, 1, 1, 0, 8  // CA/CC/FI/FTERM/FREF/FB
  };
  const CounterSnapshot first = counters_of(top);
  for (int cycle = 0; cycle < 32; ++cycle) zhao::tick(top);
  top.eval();
  const CounterSnapshot stable = counters_of(top);
  const bool exact = counters_equal(first, expected) &&
      counters_equal(stable, expected) &&
      top.reservation_count_o == 1 &&
      top.reservation_owner_state_o == 0 && top.cache_work_state_o == 0 &&
      top.idle_o == 0 && top.smp_valid_o == 0 && top.fill_valid_o == 0 &&
      top.acc_valid_i == 0 && top.fill_protocol_fault_o == 0;
  zhao::check(exact,
      "double-resv mutant leaves exactly unowned rs_resv=1 after all work/owners/counters drain",
      1, exact ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

#if defined(EXPECT_PACKET_E_DENIAL_REPLAY_MUTANT)
  run_denial_replay_mutant();
  return zhao::report_and_exit("texture_cache_pipe_v2_packet_e_denial_replay_mutant");
#elif defined(EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT)
  run_prepaid_double_reservation_mutant();
  return zhao::report_and_exit("texture_cache_pipe_v2_packet_e_double_resv_mutant");
#else
  test_success_younger_multicast_replay_and_warm_hold();
  test_refusal_skips_head_and_resumes_younger();
  test_stalled_response_with_new_inputs();
  test_data_before_fi_and_quiet_clear();
  test_refusal_before_fi_and_no_miss();
  test_simultaneous_data_refusal();
  test_partial_refusal_invalidates_then_refills();
  test_ninth_unsolicited_and_duplicate_refusal();
  return zhao::report_and_exit("texture_cache_pipe_v2_directed");
#endif
}

#endif  // EXPECT_PACKET_E_LANES8_REQN2
