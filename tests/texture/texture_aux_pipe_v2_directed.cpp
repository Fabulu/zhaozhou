// texture_aux_pipe_v2_directed.cpp — Packet-B TEXTURE.AUX.V2 protocol gate.
//
// Covers complete Surface Sheet READ carriage, issue-before-local-refusal,
// request/return hold, typed HIT/MISS/malformed disposition, owed-response FIFO,
// unsolicited sink, owner identity, lifetime credit, structural idle, and the
// explicit no-AUX-as-sample2 plane layout.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_aux_pipe_v2.h"
#include "zhao_sim.hpp"

namespace {

constexpr uint8_t kHit = 0;
constexpr uint8_t kAllocated = 1;
constexpr uint8_t kOverflow = 2;
constexpr uint8_t kMiss = 3;
constexpr uint8_t kRead = 1;

struct Job {
  int32_t wx = 0;
  int32_t wz = 0;
  int32_t x0 = 0;
  int32_t x1 = 1;
  int32_t z0 = 0;
  int32_t z1 = 1;
  uint32_t handle = 0;
  uint16_t owner = 0;
  bool force_refuse = false;
};

struct SheetReq {
  uint8_t op = 0;
  uint32_t handle = 0;
  uint16_t texel = 0;
  uint16_t src = 0;
};

struct AuxResult {
  uint16_t owner = 0;
  uint8_t status = 0;
  uint8_t tag = 0;
  uint8_t strength = 0;
  uint32_t low24 = 0;
};

struct AuxSnapshot {
  uint32_t accepted = 0;
  uint32_t sheet_reads = 0;
  uint32_t local_refused = 0;
  uint32_t completed = 0;
  uint32_t degenerate = 0;
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint32_t wrong_op = 0;
  uint32_t wrong_status = 0;
  uint32_t wrong_src = 0;
  uint32_t unsolicited = 0;
  uint32_t credit_fault = 0;
  uint16_t issue_owner = 0;
  SheetReq request;
  AuxResult result;
  uint8_t job_ready = 0;
  uint8_t issue_valid = 0;
  uint8_t req_valid = 0;
  uint8_t pg_ready = 0;
  uint8_t out_valid = 0;
  uint8_t refuse_valid = 0;
  uint8_t owed = 0;
  uint8_t idle = 0;
  uint8_t credit = 0;
};

int coord(int32_t w, int32_t lo, int32_t hi) {
  if (hi <= lo) return 0;
  const int64_t numerator = (static_cast<int64_t>(w) - lo) * 64;
  const int64_t denominator = static_cast<int64_t>(hi) - lo;
  if (numerator < 0) return 0;
  if (numerator >= denominator * 64) return 63;
  return static_cast<int>(numerator / denominator);
}

void drive_job(Vzhao_texture_aux_pipe_v2& top, const Job& j) {
  top.job_wx_i = static_cast<uint32_t>(j.wx);
  top.job_wz_i = static_cast<uint32_t>(j.wz);
  top.job_env_x0_i = static_cast<uint32_t>(j.x0);
  top.job_env_x1_i = static_cast<uint32_t>(j.x1);
  top.job_env_z0_i = static_cast<uint32_t>(j.z0);
  top.job_env_z1_i = static_cast<uint32_t>(j.z1);
  top.job_sheet_handle_i = j.handle;
  top.job_owner_i = j.owner;
  top.job_force_refuse_i = j.force_refuse ? 1 : 0;
}

SheetReq read_request(const Vzhao_texture_aux_pipe_v2& top) {
  SheetReq r;
  r.op = static_cast<uint8_t>(top.req_op_o);
  r.handle = top.req_handle_o;
  r.texel = static_cast<uint16_t>(top.req_texel_o);
  r.src = static_cast<uint16_t>(top.req_src_id_o);
  return r;
}

AuxResult read_result(const Vzhao_texture_aux_pipe_v2& top) {
  const uint64_t word = top.out_result_o;
  AuxResult r;
  r.owner = static_cast<uint16_t>(top.out_owner_o);
  r.status = static_cast<uint8_t>(word >> 40);
  r.tag = static_cast<uint8_t>(word >> 32);
  r.strength = static_cast<uint8_t>(word >> 24);
  r.low24 = static_cast<uint32_t>(word) & 0x00FFFFFFu;
  return r;
}

bool same_request(const SheetReq& a, const SheetReq& b) {
  return a.op == b.op && a.handle == b.handle && a.texel == b.texel && a.src == b.src;
}

bool same_result(const AuxResult& a, const AuxResult& b) {
  return a.owner == b.owner && a.status == b.status && a.tag == b.tag &&
         a.strength == b.strength && a.low24 == b.low24;
}

AuxSnapshot snapshot(const Vzhao_texture_aux_pipe_v2& top) {
  AuxSnapshot s;
  s.accepted = top.accepted_o;
  s.sheet_reads = top.sheet_reads_o;
  s.local_refused = top.local_refused_o;
  s.completed = top.completed_o;
  s.degenerate = top.degenerate_o;
  s.hits = top.sheet_hits_o;
  s.misses = top.sheet_misses_o;
  s.wrong_op = top.sheet_rsp_wrong_op_o;
  s.wrong_status = top.sheet_rsp_wrong_status_o;
  s.wrong_src = top.sheet_rsp_wrong_src_o;
  s.unsolicited = top.sheet_rsp_unsolicited_o;
  s.credit_fault = top.credit_fault_o;
  s.issue_owner = static_cast<uint16_t>(top.issue_owner_o);
  s.request = read_request(top);
  s.result = read_result(top);
  s.job_ready = top.job_ready_o;
  s.issue_valid = top.issue_valid_o;
  s.req_valid = top.req_valid_o;
  s.pg_ready = top.pg_ready_o;
  s.out_valid = top.out_valid_o;
  s.refuse_valid = top.refuse_valid_o;
  s.owed = top.sheet_rsp_owed_o;
  s.idle = top.idle_o;
  s.credit = top.credit_in_use_o;
  return s;
}

bool same_snapshot(const AuxSnapshot& a, const AuxSnapshot& b) {
  return a.accepted == b.accepted && a.sheet_reads == b.sheet_reads &&
         a.local_refused == b.local_refused && a.completed == b.completed &&
         a.degenerate == b.degenerate && a.hits == b.hits &&
         a.misses == b.misses && a.wrong_op == b.wrong_op &&
         a.wrong_status == b.wrong_status && a.wrong_src == b.wrong_src &&
         a.unsolicited == b.unsolicited && a.credit_fault == b.credit_fault &&
         a.issue_owner == b.issue_owner &&
         same_request(a.request, b.request) && same_result(a.result, b.result) &&
         a.job_ready == b.job_ready && a.issue_valid == b.issue_valid &&
         a.req_valid == b.req_valid && a.pg_ready == b.pg_ready &&
         a.out_valid == b.out_valid && a.refuse_valid == b.refuse_valid &&
         a.owed == b.owed && a.idle == b.idle && a.credit == b.credit;
}

SheetReq expected_request(const Job& j) {
  const uint8_t u = static_cast<uint8_t>(coord(j.wx, j.x0, j.x1));
  const uint8_t v = static_cast<uint8_t>(coord(j.wz, j.z0, j.z1));
  return SheetReq{kRead, j.handle, static_cast<uint16_t>((v << 6) | u), j.owner};
}

void clear_inputs(Vzhao_texture_aux_pipe_v2& top) {
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
}

void reset(Vzhao_texture_aux_pipe_v2& top) {
  clear_inputs(top);
  top.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

int accept_job(Vzhao_texture_aux_pipe_v2& top, const Job& job, int* cycle) {
  top.job_valid_i = 1;
  drive_job(top, job);
  for (int guard = 0; guard < 100; ++guard) {
    top.eval();
    const bool fire = top.job_ready_o;
    zhao::check(top.issue_valid_o == (fire ? 1 : 0),
                "AUX issue pulse is exactly logical job acceptance", fire ? 1 : 0,
                top.issue_valid_o);
    if (fire) {
      zhao::check(top.issue_owner_o == job.owner,
                  "AUX issue pulse carries the accepted owner", job.owner,
                  top.issue_owner_o);
      const int accepted_cycle = *cycle;
      zhao::tick(top);
      ++*cycle;
      top.job_valid_i = 0;
      return accepted_cycle;
    }
    zhao::tick(top);
    ++*cycle;
  }
  zhao::check(false, "AUX job accepted within bounded wait", 1, 0);
  top.job_valid_i = 0;
  return -1;
}

SheetReq wait_request(Vzhao_texture_aux_pipe_v2& top, int* cycle, bool accept) {
  for (int guard = 0; guard < 100; ++guard) {
    top.eval();
    if (top.req_valid_o) {
      const SheetReq request = read_request(top);
      if (accept) {
        top.req_ready_i = 1;
        top.eval();
        zhao::tick(top);
        ++*cycle;
        top.req_ready_i = 0;
      }
      return request;
    }
    zhao::tick(top);
    ++*cycle;
  }
  zhao::check(false, "AUX Sheet request appears within bounded wait", 1, 0);
  return SheetReq{};
}

void send_response(Vzhao_texture_aux_pipe_v2& top, int* cycle, uint8_t op,
                   uint8_t status, uint8_t tag, uint8_t strength, uint16_t src) {
  top.pg_valid_i = 1;
  top.pg_op_i = op;
  top.pg_status_i = status;
  top.pg_tag_i = tag;
  top.pg_strength_i = strength;
  top.pg_src_id_i = src;
  for (int guard = 0; guard < 100; ++guard) {
    top.eval();
    if (top.pg_ready_o) {
      zhao::tick(top);
      ++*cycle;
      top.pg_valid_i = 0;
      return;
    }
    zhao::tick(top);
    ++*cycle;
  }
  zhao::check(false, "AUX Sheet response accepted within reserved capacity", 1, 0);
  top.pg_valid_i = 0;
}

AuxResult wait_result(Vzhao_texture_aux_pipe_v2& top, int* cycle, bool accept) {
  for (int guard = 0; guard < 100; ++guard) {
    top.eval();
    if (top.out_valid_o) {
      const AuxResult result = read_result(top);
      if (accept) {
        top.out_ready_i = 1;
        top.eval();
        zhao::tick(top);
        ++*cycle;
        top.out_ready_i = 0;
      }
      return result;
    }
    zhao::tick(top);
    ++*cycle;
  }
  zhao::check(false, "AUX typed return appears within bounded wait", 1, 0);
  return AuxResult{};
}

void test_hit_and_holds(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;
  top.eval();
  zhao::check(top.idle_o == 1, "AUX V2 idle after reset", 1, top.idle_o);
  zhao::check(top.sheet_rsp_owed_o == 0, "AUX V2 owes no response after reset", 0,
              top.sheet_rsp_owed_o);

  const Job job{3200, 4750, 0, 6400, -50, 6350, 0x123456A7u, 0x2B7Du, false};
  const int issue_cycle = accept_job(top, job, &cycle);
  top.eval();
  zhao::check(top.idle_o == 0, "accepted AUX credit makes idle low", 0, top.idle_o);

  // Wait for the fixed pipeline, then force a long Sheet request stall.
  SheetReq offered = wait_request(top, &cycle, false);
  const SheetReq want_request = expected_request(job);
  zhao::check(same_request(offered, want_request),
              "AUX emits complete READ/handle/texel/source packet", 1,
              same_request(offered, want_request) ? 1 : 0);

  int request_hold_bad = 0;
  for (int i = 0; i < 20; ++i) {
    top.req_ready_i = 0;
    top.eval();
    if (!top.req_valid_o || !same_request(read_request(top), offered)) ++request_hold_bad;
    zhao::tick(top);
    ++cycle;
  }
  zhao::check(request_hold_bad == 0, "AUX holds every Sheet request bit while stalled", 0,
              request_hold_bad);

  top.req_ready_i = 1;
  top.eval();
  zhao::tick(top);
  ++cycle;
  top.req_ready_i = 0;
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 1,
              "accepted READ raises explicit owed-response observation", 1,
              top.sheet_rsp_owed_o);

  send_response(top, &cycle, kRead, kHit, 0xD3, 0x6E, job.owner);
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 0,
              "consuming the final Sheet response clears owed before owner return", 0,
              top.sheet_rsp_owed_o);

  const AuxResult first = wait_result(top, &cycle, false);
  const AuxResult want{job.owner, 0, 0xD3, 0x6E, 0};
  zhao::check(same_result(first, want),
              "HIT creates typed AUX {status,tag,strength,zero24} for exact owner", 1,
              same_result(first, want) ? 1 : 0);
  zhao::check(cycle > issue_cycle, "AUX return occurs after its logical issue", 1,
              cycle > issue_cycle ? 1 : 0);

  int result_hold_bad = 0;
  for (int i = 0; i < 24; ++i) {
    top.out_ready_i = 0;
    top.eval();
    if (!top.out_valid_o || !same_result(read_result(top), first)) ++result_hold_bad;
    zhao::tick(top);
    ++cycle;
  }
  zhao::check(result_hold_bad == 0, "AUX holds owner and all 48 return bits while stalled", 0,
              result_hold_bad);
  top.eval();
  zhao::check(top.idle_o == 0, "held owner return keeps AUX idle low", 0, top.idle_o);

  top.out_ready_i = 1;
  top.eval();
  zhao::tick(top);
  ++cycle;
  top.out_ready_i = 0;
  top.eval();
  zhao::check(top.idle_o == 1, "AUX becomes idle only after owner accepts return", 1,
              top.idle_o);
  zhao::check(top.accepted_o == 1 && top.sheet_reads_o == 1 &&
                  top.completed_o == 1 && top.local_refused_o == 0 &&
                  top.sheet_hits_o == 1,
              "HIT accounting closes accepted/read/hit/completed exactly once", 1,
              (top.completed_o == 1 && top.sheet_hits_o == 1) ? 1 : 0);
  zhao::check(top.frame_fault_o == 0 && top.credit_fault_o == 0,
              "ordinary HIT leaves frame and reserved-capacity faults clear", 0,
              top.frame_fault_o + top.credit_fault_o);
}

void test_local_refusals(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;
  top.req_ready_i = 1;
  top.out_ready_i = 1;

  Job degenerate{100, 200, 50, 50, 0, 1000, 0x11111101u, 0x0011u, false};
  Job forced{300, 400, 0, 1000, 0, 1000, 0x22222202u, 0x0022u, true};
  std::map<uint16_t, int> issue_cycle;
  issue_cycle[degenerate.owner] = accept_job(top, degenerate, &cycle);
  issue_cycle[forced.owner] = accept_job(top, forced, &cycle);

  std::vector<AuxResult> results;
  int sheet_offers = 0;
  int refuse_seen = 0;
  for (int guard = 0; guard < 200 && results.size() < 2; ++guard) {
    top.eval();
    if (top.req_valid_o) ++sheet_offers;
    if (top.refuse_valid_o) ++refuse_seen;
    if (top.out_valid_o && top.out_ready_i) {
      const AuxResult r = read_result(top);
      if (cycle <= issue_cycle[r.owner])
        zhao::check(false, "local AUX refusal is no earlier than N+1 after issue", 1, 0);
      results.push_back(r);
    }
    zhao::tick(top);
    ++cycle;
  }
  top.out_ready_i = 0;
  top.req_ready_i = 0;

  int bad = 0;
  for (const AuxResult& result : results) {
    if (result.status != 1 || result.tag != 0 || result.strength != 0 ||
        result.low24 != 0 || (result.owner != degenerate.owner && result.owner != forced.owner))
      ++bad;
  }
  zhao::check(results.size() == 2, "both local refusals terminate", 2, results.size());
  zhao::check(bad == 0,
              "local refusal plane is status-only and cannot masquerade as RGB/sample2", 0,
              bad);
  zhao::check(sheet_offers == 0, "degenerate/forced AUX issues no guessed Sheet READ", 0,
              sheet_offers);
  zhao::check(refuse_seen > 0, "local refusal valid observation actually fires", 1,
              refuse_seen > 0 ? 1 : 0);
  zhao::check(top.accepted_o == 2 && top.sheet_reads_o == 0 &&
                  top.local_refused_o == 2 && top.completed_o == 2 &&
                  top.degenerate_o == 1,
              "local-refusal accounting closes after owner acceptance", 1,
              (top.local_refused_o == 2 && top.completed_o == 2) ? 1 : 0);
  zhao::check(top.frame_fault_o == 1, "local refusal raises sticky frame fault", 1,
              top.frame_fault_o);
  top.eval();
  zhao::check(top.idle_o == 1, "local refusals drain to complete idle", 1, top.idle_o);
  zhao::check(top.credit_fault_o == 0,
              "legal local refusals do not trip reserved-capacity counter", 0,
              top.credit_fault_o);
}

void test_clear_live_state(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;

  const Job owed{512, 640, 0, 1024, 0, 1024,
                 0x45678931u, 0x0931u, false};
  const Job local{123, 456, 0, 1024, 0, 1024,
                  0x45678942u, 0x0942u, true};
  accept_job(top, owed, &cycle);
  const SheetReq request = wait_request(top, &cycle, true);
  zhao::check(same_request(request, expected_request(owed)),
              "clear control first seals one owed Sheet request", 1,
              same_request(request, expected_request(owed)) ? 1 : 0);
  accept_job(top, local, &cycle);
  const AuxResult held_local = wait_result(top, &cycle, false);
  top.eval();
  zhao::check(top.frame_fault_o == 1 && top.sheet_rsp_owed_o == 1 &&
                  top.out_valid_o == 1 && top.idle_o == 0,
              "clear control parks owed response and held local return together", 1,
              (top.frame_fault_o == 1 && top.sheet_rsp_owed_o == 1 &&
               top.out_valid_o == 1 && top.idle_o == 0) ? 1 : 0);
  zhao::check(held_local.owner == local.owner && held_local.status == 1,
              "clear control visible payload is the forced local refusal", 1,
              held_local.owner == local.owner ? 1 : 0);

  // Freeze every external handshake and snapshot every counter, every public
  // valid/owed/refuse/credit/idle observation, and both visible payload buses.
  top.job_valid_i = 0;
  top.req_ready_i = 0;
  top.pg_valid_i = 0;
  top.out_ready_i = 0;
  top.eval();
  const AuxSnapshot before = snapshot(top);
  zhao::check(before.accepted == 2 && before.sheet_reads == 1 &&
                  before.local_refused == 0 && before.completed == 0 &&
                  before.degenerate == 0 && before.hits == 0 &&
                  before.misses == 0 && before.wrong_op == 0 &&
                  before.wrong_status == 0 && before.wrong_src == 0 &&
                  before.unsolicited == 0 && before.credit_fault == 0,
              "clear control counter snapshot names every AUX counter exactly", 1,
              1);

  top.frame_fault_clear_i = 1;
  zhao::tick(top);
  ++cycle;
  top.frame_fault_clear_i = 0;
  top.eval();
  const AuxSnapshot after = snapshot(top);
  zhao::check(top.frame_fault_o == 0,
              "live-state frame clear clears the sticky summary", 0,
              top.frame_fault_o);
  zhao::check(same_snapshot(before, after),
              "frame clear preserves every counter/state signal and visible held payload", 1,
              same_snapshot(before, after) ? 1 : 0);
  zhao::check(after.owed == 1 && after.out_valid == 1 &&
                  after.credit == before.credit && after.idle == 0 &&
                  after.refuse_valid == before.refuse_valid,
              "frame clear preserves owed/valid/refuse/credit/idle observations", 1,
              (after.owed == 1 && after.out_valid == 1 &&
               after.credit == before.credit && after.idle == 0 &&
               after.refuse_valid == before.refuse_valid) ? 1 : 0);

  // Drain both obligations and prove the clear did not damage their identities.
  send_response(top, &cycle, kRead, kHit, 0x71, 0x82, owed.owner);
  const AuxResult local_out = wait_result(top, &cycle, true);
  const AuxResult owed_out = wait_result(top, &cycle, true);
  zhao::check(same_result(local_out, held_local) && owed_out.owner == owed.owner &&
                  owed_out.status == 0 && owed_out.tag == 0x71 &&
                  owed_out.strength == 0x82 && owed_out.low24 == 0,
              "live-state clear leaves both terminal identities and payloads intact", 1,
              1);
  top.eval();
  zhao::check(top.completed_o == 2 && top.local_refused_o == 1 &&
                  top.sheet_hits_o == 1 && top.credit_in_use_o == 0 && top.idle_o,
              "live-state clear obligations still close exact accounting", 1,
              1);
}

AuxResult one_sheet_case(Vzhao_texture_aux_pipe_v2& top, int* cycle, uint16_t owner,
                         uint8_t op, uint8_t status, uint16_t response_src,
                         uint8_t tag = 0x61, uint8_t strength = 0xB4) {
  Job job{100 + owner, 200 + owner, 0, 4096, 0, 4096,
          static_cast<uint32_t>(0xA0000000u | owner), owner, false};
  accept_job(top, job, cycle);
  const SheetReq request = wait_request(top, cycle, true);
  zhao::check(same_request(request, expected_request(job)),
              "malformed-case request still carries exact sealed identity", 1,
              same_request(request, expected_request(job)) ? 1 : 0);
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 1, "serialized case owes one response", 1,
              top.sheet_rsp_owed_o);
  send_response(top, cycle, op, status, tag, strength, response_src);
  return wait_result(top, cycle, true);
}

void test_response_verdicts(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;

  // Response with no request is accepted by the sink and creates no owner.
  send_response(top, &cycle, kRead, kHit, 0xAA, 0xBB, 0x1234);
  top.eval();
  zhao::check(top.sheet_rsp_unsolicited_o == 1,
              "response-without-request increments unsolicited counter", 1,
              top.sheet_rsp_unsolicited_o);
  zhao::check(top.out_valid_o == 0 && top.credit_in_use_o == 0,
              "unsolicited response cannot invent owner state", 0,
              top.out_valid_o || top.credit_in_use_o);

  const AuxResult hit = one_sheet_case(top, &cycle, 0x0101, kRead, kHit, 0x0101);
  const AuxResult miss = one_sheet_case(top, &cycle, 0x0102, kRead, kMiss, 0x0102);
  const AuxResult wrong_op = one_sheet_case(top, &cycle, 0x0103, 2, kHit, 0x0103);
  const AuxResult wrong_src = one_sheet_case(top, &cycle, 0x0104, kRead, kHit, 0x7F04);
  const AuxResult allocated = one_sheet_case(top, &cycle, 0x0105, kRead, kAllocated, 0x0105);
  const AuxResult overflow = one_sheet_case(top, &cycle, 0x0106, kRead, kOverflow, 0x0106);

  zhao::check(hit.owner == 0x0101 && hit.status == 0 && hit.tag == 0x61 &&
                  hit.strength == 0xB4 && hit.low24 == 0,
              "legal HIT preserves tag/strength only in typed AUX fields", 1,
              hit.status == 0 ? 1 : 0);
  const AuxResult refused[5] = {miss, wrong_op, wrong_src, allocated, overflow};
  int refused_bad = 0;
  for (const AuxResult& r : refused) {
    if (r.status != 1 || r.tag != 0 || r.strength != 0 || r.low24 != 0) ++refused_bad;
  }
  zhao::check(refused_bad == 0,
              "MISS/wrong-op/wrong-src/ALLOCATED/OVERFLOW all terminally refuse", 0,
              refused_bad);
  zhao::check(miss.owner == 0x0102 && wrong_op.owner == 0x0103 &&
                  wrong_src.owner == 0x0104 && allocated.owner == 0x0105 &&
                  overflow.owner == 0x0106,
              "malformed responses complete the owed FIFO head, never claimed source", 1,
              1);

  zhao::check(top.sheet_hits_o == 1, "one legal HIT counted", 1, top.sheet_hits_o);
  zhao::check(top.sheet_misses_o == 1, "one legal MISS counted", 1, top.sheet_misses_o);
  zhao::check(top.sheet_rsp_wrong_op_o == 1, "wrong opcode detector fires", 1,
              top.sheet_rsp_wrong_op_o);
  zhao::check(top.sheet_rsp_wrong_src_o == 1, "wrong source detector fires", 1,
              top.sheet_rsp_wrong_src_o);
  zhao::check(top.sheet_rsp_wrong_status_o == 2,
              "ALLOCATED and OVERFLOW each fire wrong-status detector", 2,
              top.sheet_rsp_wrong_status_o);
  zhao::check(top.completed_o == 6 && top.sheet_reads_o == 6,
              "every issued read receives exactly one terminal owner result", 6,
              top.completed_o);

  // Duplicate the last response after its head was consumed.  It drains as a
  // second unsolicited fault and cannot create a seventh completion.
  send_response(top, &cycle, kRead, kOverflow, 0, 0, 0x0106);
  for (int i = 0; i < 4; ++i) {
    top.eval();
    zhao::check(top.out_valid_o == 0, "duplicate response creates no owner return", 0,
                top.out_valid_o);
    zhao::tick(top);
    ++cycle;
  }
  zhao::check(top.sheet_rsp_unsolicited_o == 2,
              "duplicate response is the second unsolicited event", 2,
              top.sheet_rsp_unsolicited_o);
  zhao::check(top.completed_o == 6, "duplicate response does not increment completion", 6,
              top.completed_o);
  zhao::check(top.frame_fault_o == 1, "all refusal/protocol verdicts set sticky frame fault", 1,
              top.frame_fault_o);

  // Frame-boundary clear is deliberately narrow: clear the sticky summary while
  // idle and prove every durable counter and all structural state stay intact.
  const uint32_t completed_before_clear = top.completed_o;
  const uint32_t unsolicited_before_clear = top.sheet_rsp_unsolicited_o;
  const uint32_t wrong_op_before_clear = top.sheet_rsp_wrong_op_o;
  top.frame_fault_clear_i = 1;
  top.eval();
  zhao::tick(top);
  ++cycle;
  top.frame_fault_clear_i = 0;
  top.eval();
  zhao::check(top.frame_fault_o == 0, "idle frame clear removes only sticky frame fault", 0,
              top.frame_fault_o);
  zhao::check(top.completed_o == completed_before_clear &&
                  top.sheet_rsp_unsolicited_o == unsolicited_before_clear &&
                  top.sheet_rsp_wrong_op_o == wrong_op_before_clear &&
                  top.credit_in_use_o == 0 && top.idle_o == 1,
              "frame clear changes no counter, credit, valid, payload or idle state", 1,
              1);

  // Same-edge fault wins over clear.  An unsolicited response is the cleanest
  // positive control because it creates no owner or data-plane state.
  top.frame_fault_clear_i = 1;
  top.pg_valid_i = 1;
  top.pg_op_i = kRead;
  top.pg_status_i = kHit;
  top.pg_tag_i = 0xCD;
  top.pg_strength_i = 0xEF;
  top.pg_src_id_i = 0x7777;
  top.eval();
  zhao::check(top.pg_ready_o == 1, "same-edge fault control response is accepted", 1,
              top.pg_ready_o);
  zhao::tick(top);
  ++cycle;
  top.frame_fault_clear_i = 0;
  top.pg_valid_i = 0;
  top.eval();
  zhao::check(top.frame_fault_o == 1, "same-edge fault has set priority over frame clear", 1,
              top.frame_fault_o);
  zhao::check(top.sheet_rsp_unsolicited_o == unsolicited_before_clear + 1 &&
                  top.out_valid_o == 0 && top.credit_in_use_o == 0 &&
                  top.credit_fault_o == 0,
              "same-edge priority records protocol fault without capacity fault", 1,
              1);
}

void test_two_owed(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;
  const Job a{512, 768, 0, 1024, 0, 1024, 0x01020311u, 0x0311u, false};
  // Same 24-bit patch index, different eight-bit generation: the full handle,
  // not only its index, must survive owner sealing and the Sheet offer.
  const Job b{128, 896, -256, 768, 0, 1024, 0x01020322u, 0x0322u, false};
  accept_job(top, a, &cycle);
  accept_job(top, b, &cycle);
  const SheetReq ra = wait_request(top, &cycle, true);
  const SheetReq rb = wait_request(top, &cycle, true);
  zhao::check(same_request(ra, expected_request(a)) && same_request(rb, expected_request(b)),
              "two interleaved owners retain their own handle/envelope/source", 1,
              1);
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 1, "two accepted reads report response owed", 1,
              top.sheet_rsp_owed_o);

  // Independent owed-room detector legal-zero control: a held issued head with
  // actual return capacity must remain quiet for every stalled cycle.
  top.out_ready_i = 0;
  for (int hold = 0; hold < 16; ++hold) {
    top.eval();
    zhao::check(top.sheet_rsp_owed_o == 1 && top.credit_fault_o == 0,
                "held legal owed response has room and does not fault", 0,
                top.credit_fault_o);
    zhao::tick(top);
    ++cycle;
  }

  send_response(top, &cycle, kRead, kHit, 0x11, 0x21, a.owner);
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 1,
              "owed remains high after first of two responses", 1,
              top.sheet_rsp_owed_o);
  send_response(top, &cycle, kRead, kHit, 0x12, 0x22, b.owner);
  top.eval();
  zhao::check(top.sheet_rsp_owed_o == 0,
              "owed falls only after final issued response is consumed", 0,
              top.sheet_rsp_owed_o);
  zhao::check(top.idle_o == 0,
              "AUX remains non-idle while typed returns are held after owed clears", 0,
              top.idle_o);

  const AuxResult oa = wait_result(top, &cycle, true);
  const AuxResult ob = wait_result(top, &cycle, true);
  zhao::check(oa.owner == a.owner && oa.tag == 0x11 && oa.strength == 0x21 &&
                  ob.owner == b.owner && ob.tag == 0x12 && ob.strength == 0x22,
              "issued identity FIFO aligns both responses and owner returns", 1,
              1);
  top.eval();
  zhao::check(top.idle_o == 1 && top.credit_fault_o == 0,
              "two-read sequence reaches quiet without capacity fault", 1,
              (top.idle_o && top.credit_fault_o == 0) ? 1 : 0);
}

int g_credit_terminal_accepted = -1;

void test_credit_terminal_release(Vzhao_texture_aux_pipe_v2& top) {
  reset(top);
  int cycle = 0;
  constexpr int kOffered = 40;
  int accepted = 0;
  int requests = 0;
  int responses = 0;
  std::deque<uint16_t> pending_src;

  top.req_ready_i = 1;
  top.out_ready_i = 0;

  // Keep a HIT response one or more cycles behind each accepted Sheet request.
  // With owner returns blocked, exactly CREDIT=16 jobs may be accepted.  A credit
  // released at request issue instead of owner acceptance accepts more and makes
  // the committed credit mutant fail this ordinary behavior check.
  for (int guard = 0; guard < 300; ++guard) {
    Job job;
    job.wx = accepted * 17;
    job.wz = accepted * 31;
    job.x0 = -1024;
    job.x1 = 8192;
    job.z0 = -2048;
    job.z1 = 8192;
    job.handle = 0x55000000u | static_cast<uint32_t>(accepted);
    job.owner = static_cast<uint16_t>(0x0800 + accepted);
    top.job_valid_i = accepted < kOffered ? 1 : 0;
    if (accepted < kOffered) drive_job(top, job);

    if (!pending_src.empty()) {
      top.pg_valid_i = 1;
      top.pg_op_i = kRead;
      top.pg_status_i = kHit;
      top.pg_tag_i = static_cast<uint8_t>(pending_src.front());
      top.pg_strength_i = static_cast<uint8_t>(pending_src.front() >> 3);
      top.pg_src_id_i = pending_src.front();
    } else {
      top.pg_valid_i = 0;
    }

    top.eval();
    const bool job_fire = top.job_valid_i && top.job_ready_o;
    const bool req_fire = top.req_valid_o && top.req_ready_i;
    const bool pg_fire = top.pg_valid_i && top.pg_ready_o;
    zhao::check(top.issue_valid_o == (job_fire ? 1 : 0),
                "credit run issue pulse equals job acceptance", job_fire ? 1 : 0,
                top.issue_valid_o);
    if (req_fire) {
      pending_src.push_back(static_cast<uint16_t>(top.req_src_id_o));
      ++requests;
    }
    if (pg_fire) {
      pending_src.pop_front();
      ++responses;
    }
    if (job_fire) ++accepted;
    zhao::tick(top);
    ++cycle;
    // The committed mutant releases at Sheet issue and crosses this boundary.
    // Stop immediately so the behavior detector, rather than a later queue-bound
    // assertion caused by deliberately over-admitting, is the evidence that fires.
    if (accepted > 16) break;
  }
  top.job_valid_i = 0;
  top.pg_valid_i = 0;
  top.eval();

  g_credit_terminal_accepted = accepted;
  zhao::check(accepted == 16,
              "AUX credit reserves through terminal owner acceptance (exactly 16 live)", 16,
              accepted);
  if (accepted != 16) return;
  zhao::check(top.credit_in_use_o == 16, "all sixteen terminal credits remain live", 16,
              top.credit_in_use_o);
  zhao::check(top.job_ready_o == 0, "full lifetime credit backpressures logical jobs", 0,
              top.job_ready_o);
  zhao::check(top.idle_o == 0, "full held return set is not quiet", 0, top.idle_o);

  // Finish any response still owed, then drain all owner returns.
  int retired = 0;
  top.out_ready_i = 1;
  for (int guard = 0; guard < 1000 && retired < accepted; ++guard) {
    if (!pending_src.empty()) {
      top.pg_valid_i = 1;
      top.pg_op_i = kRead;
      top.pg_status_i = kHit;
      top.pg_tag_i = static_cast<uint8_t>(pending_src.front());
      top.pg_strength_i = 0x5A;
      top.pg_src_id_i = pending_src.front();
    } else {
      top.pg_valid_i = 0;
    }
    top.eval();
    const bool req_fire = top.req_valid_o && top.req_ready_i;
    const bool pg_fire = top.pg_valid_i && top.pg_ready_o;
    const bool out_fire = top.out_valid_o && top.out_ready_i;
    if (req_fire) {
      pending_src.push_back(static_cast<uint16_t>(top.req_src_id_o));
      ++requests;
    }
    if (pg_fire) {
      pending_src.pop_front();
      ++responses;
    }
    if (out_fire) {
      const AuxResult result = read_result(top);
      if (result.status != 0 || result.low24 != 0)
        zhao::check(false, "credit-run returns stay typed HIT AUX planes", 1, 0);
      ++retired;
    }
    zhao::tick(top);
    ++cycle;
  }
  top.pg_valid_i = 0;
  top.req_ready_i = 0;
  top.out_ready_i = 0;
  top.eval();
  zhao::check(requests == accepted && responses == accepted,
              "credit run closes one Sheet request and response per accepted job", accepted,
              requests == accepted && responses == accepted ? accepted : -1);
  zhao::check(retired == accepted && top.completed_o == static_cast<uint32_t>(accepted),
              "credit run completes every owner exactly once", accepted, retired);
  zhao::check(top.credit_in_use_o == 0, "terminal accepts release every credit", 0,
              top.credit_in_use_o);
  zhao::check(top.sheet_rsp_owed_o == 0 && top.idle_o == 1 &&
                  top.credit_fault_o == 0,
              "credit run drains owed work with no reserved-capacity fault", 1,
              (!top.sheet_rsp_owed_o && top.idle_o && top.credit_fault_o == 0) ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_aux_pipe_v2 top;
  test_hit_and_holds(top);
  test_local_refusals(top);
  test_clear_live_state(top);
  test_response_verdicts(top);
  test_two_owed(top);
  const int failures_before_credit_lifetime = zhao::check_failures();
  test_credit_terminal_release(top);
#ifdef ZHAO_AUX_CREDIT_MUTANT_CONTROL
  const int failures = zhao::check_failures();
  if (failures_before_credit_lifetime == 0 && failures == 1 &&
      g_credit_terminal_accepted == 17) {
    std::printf("AUX credit lifetime mutant FIRED exactly once\n");
    zhao::exit_hard(0);
  }
  std::fprintf(stderr,
               "FAIL: AUX credit lifetime mutant expected zero earlier failures and "
               "one focused failure at the exact 17th acceptance, got "
               "before=%d total=%d accepted=%d\n",
               failures_before_credit_lifetime, failures,
               g_credit_terminal_accepted);
  zhao::exit_hard(1);
#else
  return zhao::report_and_exit("texture_aux_pipe_v2_directed");
#endif
}
