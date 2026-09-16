// texture_aux_pipe_v2_random.cpp — randomized Packet-B AUX differential.
//
// The coordinate/data oracle is literally zref::aux::AuxSource from zref_aux.hpp;
// this driver does not carry a second copy of axis_texel arithmetic.  It varies
// legal/extreme/degenerate envelopes, two generation-distinct sheet handles,
// unique live owners, HIT/MISS/malformed responses, and independently stalled
// job, Sheet request, Sheet response, and owner-return channels.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>

#include "verilated.h"
#include "Vzhao_texture_aux_pipe_v2.h"
#include "zhao_sim.hpp"
#include "zref/zref_aux.hpp"

namespace {

constexpr uint8_t kRead = 1;
constexpr uint8_t kHit = 0;
constexpr uint8_t kAllocated = 1;
constexpr uint8_t kMiss = 3;
constexpr int kJobs = 384;
constexpr uint32_t kHandleA = 0x00ABC011u;
constexpr uint32_t kHandleB = 0x00ABC022u;  // same index24, new generation8

enum class Verdict : uint8_t { Hit, Miss, WrongOp, WrongStatus, WrongSource };

struct Job {
  int32_t wx = 0;
  int32_t wz = 0;
  zref::aux::Envelope envelope;
  uint32_t handle = 0;
  uint16_t owner = 0;
  Verdict verdict = Verdict::Hit;
  zref::aux::Sample oracle;
  int issue_cycle = -1;
};

struct ExpectedRequest {
  Job job;
  uint16_t texel = 0;
};

struct Response {
  int due_cycle = 0;
  uint8_t op = kRead;
  uint8_t status = kHit;
  uint8_t tag = 0;
  uint8_t strength = 0;
  uint16_t src = 0;
};

struct ExpectedResult {
  uint8_t status = 0;
  uint8_t tag = 0;
  uint8_t strength = 0;
  int issue_cycle = -1;
};

uint32_t next(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

void drive_job(Vzhao_texture_aux_pipe_v2& top, const Job& job) {
  top.job_wx_i = static_cast<uint32_t>(job.wx);
  top.job_wz_i = static_cast<uint32_t>(job.wz);
  top.job_env_x0_i = static_cast<uint32_t>(job.envelope.x0);
  top.job_env_x1_i = static_cast<uint32_t>(job.envelope.x1);
  top.job_env_z0_i = static_cast<uint32_t>(job.envelope.z0);
  top.job_env_z1_i = static_cast<uint32_t>(job.envelope.z1);
  top.job_sheet_handle_i = job.handle;
  top.job_owner_i = job.owner;
  top.job_force_refuse_i = 0;
}

uint64_t result_word(const Vzhao_texture_aux_pipe_v2& top) {
  return static_cast<uint64_t>(top.out_result_o);
}

bool request_matches(const Vzhao_texture_aux_pipe_v2& top, const ExpectedRequest& expected) {
  return top.req_op_o == kRead && top.req_handle_o == expected.job.handle &&
         top.req_texel_o == expected.texel && top.req_src_id_o == expected.job.owner;
}

void reset(Vzhao_texture_aux_pipe_v2& top) {
  top.frame_fault_clear_i = 0;
  top.job_valid_i = 0;
  top.req_ready_i = 0;
  top.pg_valid_i = 0;
  top.pg_op_i = 0;
  top.pg_status_i = 0;
  top.pg_tag_i = 0;
  top.pg_strength_i = 0;
  top.pg_src_id_i = 0;
  top.out_ready_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

Job make_job(int index, uint32_t* random, const std::array<uint8_t, 4096>& tags_a,
             const std::array<uint8_t, 4096>& strengths_a, const std::array<uint8_t, 4096>& tags_b,
             const std::array<uint8_t, 4096>& strengths_b) {
  Job job;
  job.handle = (index & 1) ? kHandleB : kHandleA;
  const uint16_t slot = static_cast<uint16_t>(index & 63);
  const uint16_t generation = static_cast<uint16_t>(((index >> 6) + 1) & 255);
  job.owner = static_cast<uint16_t>((slot << 8) | generation);

  switch (index % 12) {
    case 0:
      job.envelope = {100, -500, 100, 900};  // degenerate X
      job.wx = 100;
      job.wz = 200;
      break;
    case 1:
      job.envelope = {-700, 42, 900, 42};  // degenerate Z
      job.wx = -100;
      job.wz = 42;
      break;
    case 2:
      job.envelope = {INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX};
      job.wx = INT32_MIN;
      job.wz = INT32_MAX;
      break;
    case 3:
      job.envelope = {-100000, -200000, 100000, 200000};
      job.wx = INT32_MIN;
      job.wz = 0;
      break;
    case 4:
      job.envelope = {-100000, -200000, 100000, 200000};
      job.wx = INT32_MAX;
      job.wz = INT32_MAX;
      break;
    default: {
      const int32_t x0 = static_cast<int32_t>(next(random) & 0x3FFFFFu) - 0x200000;
      const int32_t z0 = static_cast<int32_t>(next(random) & 0x3FFFFFu) - 0x200000;
      const int32_t dx = 1 + static_cast<int32_t>(next(random) & 0x1FFFFu);
      const int32_t dz = 1 + static_cast<int32_t>(next(random) & 0x1FFFFu);
      job.envelope = {x0, z0, x0 + dx, z0 + dz};
      const int32_t xspread = dx * 2;
      const int32_t zspread = dz * 2;
      job.wx = x0 - dx / 2 + static_cast<int32_t>(next(random) % xspread);
      job.wz = z0 - dz / 2 + static_cast<int32_t>(next(random) % zspread);
      break;
    }
  }

  switch (index % 10) {
    case 5:
    case 6:
      job.verdict = Verdict::Miss;
      break;
    case 7:
      job.verdict = Verdict::WrongOp;
      break;
    case 8:
      job.verdict = Verdict::WrongStatus;
      break;
    case 9:
      job.verdict = Verdict::WrongSource;
      break;
    default:
      job.verdict = Verdict::Hit;
      break;
  }

  const bool resident = job.verdict != Verdict::Miss;
  const uint8_t* tags = job.handle == kHandleA ? tags_a.data() : tags_b.data();
  const uint8_t* strengths = job.handle == kHandleA ? strengths_a.data() : strengths_b.data();
  // The one arithmetic/data authority for this randomized test.
  job.oracle =
      zref::aux::AuxSource::sample(job.envelope, job.wx, job.wz, tags, strengths, resident);
  return job;
}

Response response_for(const ExpectedRequest& expected, int cycle, uint32_t* random) {
  Response response;
  response.due_cycle = cycle + 1 + static_cast<int>(next(random) % 9);
  response.src = expected.job.owner;
  response.tag = expected.job.oracle.tag;
  response.strength = expected.job.oracle.strength;
  switch (expected.job.verdict) {
    case Verdict::Hit:
      response.op = kRead;
      response.status = kHit;
      break;
    case Verdict::Miss:
      response.op = kRead;
      response.status = kMiss;
      response.tag = 0;
      response.strength = 0;
      break;
    case Verdict::WrongOp:
      response.op = 2;
      response.status = kHit;
      break;
    case Verdict::WrongStatus:
      response.op = kRead;
      response.status = kAllocated;
      break;
    case Verdict::WrongSource:
      response.op = kRead;
      response.status = kHit;
      response.src ^= 0x0001u;
      break;
  }
  return response;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_aux_pipe_v2 top;

  std::array<uint8_t, 4096> tags_a{};
  std::array<uint8_t, 4096> strengths_a{};
  std::array<uint8_t, 4096> tags_b{};
  std::array<uint8_t, 4096> strengths_b{};
  for (int i = 0; i < 4096; ++i) {
    tags_a[i] = static_cast<uint8_t>((i * 17 + 0x21) & 255);
    strengths_a[i] = static_cast<uint8_t>((i * 29 + 0x43) & 255);
    tags_b[i] = static_cast<uint8_t>((i * 31 + 0x65) & 255);
    strengths_b[i] = static_cast<uint8_t>((i * 47 + 0x87) & 255);
  }

  reset(top);
  uint32_t data_random = 0xA17E5EEDu;
  uint32_t valid_random = 0x13572468u;
  uint32_t request_random = 0x24681357u;
  uint32_t output_random = 0xC001D00Du;
  uint32_t response_random = 0x5EEDBEEFu;

  bool job_pending = false;
  Job offered_job;
  int made = 0;
  int accepted = 0;
  int completed = 0;
  int cycle = 0;
  int mismatches = 0;
  int job_stalls = 0;
  int request_stalls = 0;
  int response_delay_cycles = 0;
  int response_hold_cycles = 0;
  int output_stalls = 0;
  int degenerate_count = 0;
  int hit_count = 0;
  int miss_count = 0;
  int wrong_op_count = 0;
  int wrong_status_count = 0;
  int wrong_src_count = 0;
  int handle_a_requests = 0;
  int handle_b_requests = 0;
  int clamp_zero = 0;
  int clamp_sixty_three = 0;
  int interior = 0;

  std::deque<ExpectedRequest> requests_expected;
  std::deque<Response> responses;
  std::map<uint16_t, ExpectedResult> results_expected;

  bool previous_req_stall = false;
  uint8_t previous_req_op = 0;
  uint32_t previous_req_handle = 0;
  uint16_t previous_req_texel = 0;
  uint16_t previous_req_src = 0;
  bool previous_out_stall = false;
  uint16_t previous_out_owner = 0;
  uint64_t previous_out_result = 0;
  bool previous_pg_stall = false;
  Response previous_response;

  for (; cycle < 30000; ++cycle) {
    if (!job_pending && made < kJobs && ((next(&valid_random) >> 28) < 12)) {
      offered_job = make_job(made, &data_random, tags_a, strengths_a, tags_b, strengths_b);
      job_pending = true;
      ++made;
    }
    top.job_valid_i = job_pending ? 1 : 0;
    if (job_pending) drive_job(top, offered_job);

    bool request_ready = ((next(&request_random) >> 28) < 10);
    if ((cycle >= 90 && cycle < 125) || (cycle >= 600 && cycle < 625)) request_ready = false;
    top.req_ready_i = request_ready ? 1 : 0;

    bool output_ready = ((next(&output_random) >> 28) < 11);
    if ((cycle >= 180 && cycle < 250) || (cycle >= 900 && cycle < 940)) output_ready = false;
    top.out_ready_i = output_ready ? 1 : 0;

    if (!responses.empty() && responses.front().due_cycle > cycle) ++response_delay_cycles;
    if (!responses.empty() && responses.front().due_cycle <= cycle) {
      const Response& response = responses.front();
      top.pg_valid_i = 1;
      top.pg_op_i = response.op;
      top.pg_status_i = response.status;
      top.pg_tag_i = response.tag;
      top.pg_strength_i = response.strength;
      top.pg_src_id_i = response.src;
    } else {
      top.pg_valid_i = 0;
    }

    top.eval();

    if (previous_req_stall &&
        (!top.req_valid_o || top.req_op_o != previous_req_op ||
         top.req_handle_o != previous_req_handle || top.req_texel_o != previous_req_texel ||
         top.req_src_id_o != previous_req_src))
      ++mismatches;
    if (previous_out_stall && (!top.out_valid_o || top.out_owner_o != previous_out_owner ||
                               result_word(top) != previous_out_result))
      ++mismatches;
    if (previous_pg_stall) {
      if (!top.pg_valid_i || top.pg_op_i != previous_response.op ||
          top.pg_status_i != previous_response.status || top.pg_tag_i != previous_response.tag ||
          top.pg_strength_i != previous_response.strength ||
          top.pg_src_id_i != previous_response.src)
        ++mismatches;
      ++response_hold_cycles;
    }

    const bool job_fire = top.job_valid_i && top.job_ready_o;
    const bool req_fire = top.req_valid_o && top.req_ready_i;
    const bool pg_fire = top.pg_valid_i && top.pg_ready_o;
    const bool out_fire = top.out_valid_o && top.out_ready_i;

    if (top.issue_valid_o != (job_fire ? 1 : 0)) ++mismatches;
    if (job_pending && !top.job_ready_o) ++job_stalls;
    if (top.req_valid_o && !top.req_ready_i) ++request_stalls;
    if (top.out_valid_o && !top.out_ready_i) ++output_stalls;

    if (job_fire) {
      offered_job.issue_cycle = cycle;
      if (top.issue_owner_o != offered_job.owner) ++mismatches;
      const bool degenerate = offered_job.oracle.degenerate;
      if (degenerate) {
        ++degenerate_count;
      } else {
        const uint16_t texel =
            static_cast<uint16_t>(offered_job.oracle.v * 64u + offered_job.oracle.u);
        requests_expected.push_back(ExpectedRequest{offered_job, texel});
        if (offered_job.handle == kHandleA)
          ++handle_a_requests;
        else
          ++handle_b_requests;
        if (offered_job.oracle.u == 0 || offered_job.oracle.v == 0) ++clamp_zero;
        if (offered_job.oracle.u == 63 || offered_job.oracle.v == 63) ++clamp_sixty_three;
        if (offered_job.oracle.u > 0 && offered_job.oracle.u < 63 && offered_job.oracle.v > 0 &&
            offered_job.oracle.v < 63)
          ++interior;
      }

      ExpectedResult expected;
      expected.issue_cycle = cycle;
      if (degenerate || offered_job.verdict != Verdict::Hit) {
        expected.status = 1;
      } else {
        expected.status = 0;
        expected.tag = offered_job.oracle.tag;
        expected.strength = offered_job.oracle.strength;
      }
      results_expected[offered_job.owner] = expected;
      job_pending = false;
      ++accepted;
    }

    if (req_fire) {
      if (requests_expected.empty()) {
        ++mismatches;
      } else {
        const ExpectedRequest expected = requests_expected.front();
        requests_expected.pop_front();
        if (!request_matches(top, expected)) ++mismatches;
        Response response = response_for(expected, cycle, &response_random);
        if (!responses.empty())
          response.due_cycle = std::max(response.due_cycle, responses.back().due_cycle + 1);
        responses.push_back(response);
        switch (expected.job.verdict) {
          case Verdict::Hit:
            ++hit_count;
            break;
          case Verdict::Miss:
            ++miss_count;
            break;
          case Verdict::WrongOp:
            ++wrong_op_count;
            break;
          case Verdict::WrongStatus:
            ++wrong_status_count;
            break;
          case Verdict::WrongSource:
            ++wrong_src_count;
            break;
        }
      }
    }

    if (pg_fire) {
      if (responses.empty())
        ++mismatches;
      else
        responses.pop_front();
    }

    if (out_fire) {
      const uint16_t owner = static_cast<uint16_t>(top.out_owner_o);
      const auto found = results_expected.find(owner);
      if (found == results_expected.end()) {
        ++mismatches;
      } else {
        const uint64_t word = result_word(top);
        const uint8_t status = static_cast<uint8_t>(word >> 40);
        const uint8_t tag = static_cast<uint8_t>(word >> 32);
        const uint8_t strength = static_cast<uint8_t>(word >> 24);
        const uint32_t low24 = static_cast<uint32_t>(word) & 0xFFFFFFu;
        if (status != found->second.status || tag != found->second.tag ||
            strength != found->second.strength || low24 != 0 || cycle <= found->second.issue_cycle)
          ++mismatches;
        results_expected.erase(found);
      }
      ++completed;
    }

    previous_req_stall = top.req_valid_o && !top.req_ready_i;
    previous_req_op = static_cast<uint8_t>(top.req_op_o);
    previous_req_handle = top.req_handle_o;
    previous_req_texel = static_cast<uint16_t>(top.req_texel_o);
    previous_req_src = static_cast<uint16_t>(top.req_src_id_o);
    previous_out_stall = top.out_valid_o && !top.out_ready_i;
    previous_out_owner = static_cast<uint16_t>(top.out_owner_o);
    previous_out_result = result_word(top);
    previous_pg_stall = top.pg_valid_i && !top.pg_ready_o;
    if (top.pg_valid_i && !responses.empty()) previous_response = responses.front();

    zhao::tick(top);

    if (accepted == kJobs && completed == kJobs && !job_pending && requests_expected.empty() &&
        responses.empty()) {
      top.eval();
      if (top.idle_o) break;
    }
  }

  top.job_valid_i = 0;
  top.req_ready_i = 0;
  top.pg_valid_i = 0;
  top.out_ready_i = 0;
  top.eval();

  zhao::check(accepted == kJobs && completed == kJobs,
              "random AUX accepts and completes every oracle job", kJobs,
              accepted == kJobs && completed == kJobs ? kJobs : -1);
  zhao::check(mismatches == 0, "random AUX Sheet identity/U/V and typed results match AuxSource", 0,
              mismatches);
  zhao::check(results_expected.empty() && requests_expected.empty() && responses.empty(),
              "random AUX scoreboards drain without missing or duplicate work", 1,
              (results_expected.empty() && requests_expected.empty() && responses.empty()) ? 1 : 0);
  zhao::check(handle_a_requests > 100 && handle_b_requests > 100,
              "both same-index/different-generation handles were exercised", 1,
              (handle_a_requests > 100 && handle_b_requests > 100) ? 1 : 0);
  zhao::check(degenerate_count > 40 && hit_count > 100 && miss_count > 40 && wrong_op_count > 20 &&
                  wrong_status_count > 20 && wrong_src_count > 20,
              "degenerate/HIT/MISS/all malformed response classes are interesting", 1, 1);
  zhao::check(clamp_zero > 20 && clamp_sixty_three > 20 && interior > 20,
              "oracle coordinates cover low clamp, high clamp, and interior", 1,
              (clamp_zero > 20 && clamp_sixty_three > 20 && interior > 20) ? 1 : 0);
  zhao::check(job_stalls > 0 && request_stalls > 20 && output_stalls > 20,
              "job, Sheet request, and owner-return backpressure all bite", 1,
              (job_stalls > 0 && request_stalls > 20 && output_stalls > 20) ? 1 : 0);
  zhao::check(response_delay_cycles > 100,
              "Sheet responses were independently delayed before presentation", 1,
              response_delay_cycles > 100 ? 1 : 0);
  zhao::check(response_hold_cycles == 0,
              "reserved legal responses are accepted immediately once presented", 0,
              response_hold_cycles);
  zhao::check(
      top.accepted_o == static_cast<uint32_t>(accepted) &&
          top.sheet_reads_o == static_cast<uint32_t>(hit_count + miss_count + wrong_op_count +
                                                     wrong_status_count + wrong_src_count) &&
          top.local_refused_o == static_cast<uint32_t>(degenerate_count) &&
          top.completed_o == static_cast<uint32_t>(completed) &&
          top.degenerate_o == static_cast<uint32_t>(degenerate_count),
      "random AUX accepted/read/local/completed/degenerate counters are exact", 1, 1);
  zhao::check(top.sheet_hits_o == static_cast<uint32_t>(hit_count) &&
                  top.sheet_misses_o == static_cast<uint32_t>(miss_count) &&
                  top.sheet_rsp_wrong_op_o == static_cast<uint32_t>(wrong_op_count) &&
                  top.sheet_rsp_wrong_status_o == static_cast<uint32_t>(wrong_status_count) &&
                  top.sheet_rsp_wrong_src_o == static_cast<uint32_t>(wrong_src_count),
              "random AUX HIT/MISS/malformed verdict counters are exact", 1, 1);
  zhao::check(top.sheet_rsp_unsolicited_o == 0 && top.credit_fault_o == 0,
              "random legal scheduling creates no unsolicited or capacity fault", 0,
              top.sheet_rsp_unsolicited_o + top.credit_fault_o);
  zhao::check(top.frame_fault_o == 1 && top.sheet_rsp_owed_o == 0 && top.credit_in_use_o == 0 &&
                  top.idle_o == 1,
              "random refusals set frame fault while owed/credit/idle drain exactly", 1,
              (top.frame_fault_o && top.idle_o) ? 1 : 0);

  std::printf(
      "  random AUX: jobs=%d degen=%d hit=%d miss=%d malformed=%d/%d/%d "
      "stalls=%d/%d/%d cycles=%d\n",
      accepted, degenerate_count, hit_count, miss_count, wrong_op_count, wrong_status_count,
      wrong_src_count, job_stalls, request_stalls, output_stalls, cycle);
  return zhao::report_and_exit("texture_aux_pipe_v2_random");
}
