// texture_v3own_adversarial.cpp -- the adversarial bench for the V3
// owner/completion/retire experiment (zhao_texture_v3own).
//
// reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 26.1 asks for
// "an adversarial testbench" alongside the RTL, and sections 8, 19 and
// Appendix D say what adversarial means here. Every case below is one of the
// document's own named hazards, and the ones marked D.n are its worked
// counterexamples turned into stimulus.
//
//   1  end to end, three samples + AUX, in order, full context
//   2  D.1  consecutive duplicate responses during a pipelined commit
//   3       spaced duplicate (after the forwarding window closes)
//   4       stale generation
//   5       unsolicited: required but never issued
//   6       unsolicited: not required at all
//   7       out-of-range sample_index 3 on the texture port
//   8  D.2  simultaneous TMU + AUX completing one owner -> ONE ticket
//   9  D.2  simultaneous TMU + AUX completing TWO owners -> TWO tickets
//  10       simultaneous TMU + AUX identity faults count TWO, not one
//  11  D.4  an unstoppable terminal producer against a fully stalled output
//  12       finite backpressure: nothing destroyed, order preserved
//  13  D.6  the final write does not free the owner; stall through ring wrap
//  14       more than 64 admissions under backpressure -- ready must fall
//  15       reset mid-transaction: no ghost output, full function afterwards
//  16       ordered retirement under out-of-order readiness
//  17  D.3  the COMBINE read reservation lasts through queue residence
//  18       duplicate and unsolicited FINAL returns
//  19       generation wrap takes the baseline drain, on a quiet island
//  20  18.3 sustained one fragment per clock through the retirement path
//
// EVERY EVIDENCE PORT IS ASSERTED ON. reports/V3-DIAGNOSIS-VERIFICATION-
// 20260906.md section 4 item 2: twelve island signals are declared,
// port-connected and consumed by nothing, three of them FRAGROB's and AUX's
// own tripwires. A counter that no test reads is decoration, so the rejection
// counters here are checked by class, exactly, in the cases that provoke them.
#include "Vzhao_texture_v3own.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

namespace {

constexpr int OWNERS = 64;

// ---- handle packing, Appendix B.1 ------------------------------------------
inline uint16_t owner_of(uint32_t slot, uint32_t gen) {
  return static_cast<uint16_t>(((slot & 0x3F) << 8) | (gen & 0xFF));
}
inline uint32_t slot_of(uint16_t owner) { return (owner >> 8) & 0x3F; }
inline uint32_t gen_of(uint16_t owner) { return owner & 0xFF; }
// sample_handle[15:10]=slot, [9:8]=sample_index, [7:0]=generation
inline uint16_t smp(uint16_t owner, uint32_t sidx) {
  return static_cast<uint16_t>((slot_of(owner) << 10) | ((sidx & 3) << 8) |
                               gen_of(owner));
}
inline uint64_t mkres(uint64_t seed) {
  return (seed * 0x9E3779B97F4A7C15ULL >> 17) & 0xFFFFFFFFFFULL;  // result40
}
inline uint64_t final_of(uint16_t owner) { return mkres(0xF1000u + owner); }
inline uint64_t ctx_of(uint32_t n) {
  // High bits deliberately distinctive: Appendix D.6 says to detect a stale
  // context independently of RGB, so the top 32 bits carry the sequence.
  return (static_cast<uint64_t>(0xC0DE0000u + n) << 32) | (0xA5A50000u + n);
}

struct Emit {
  uint16_t owner;
  uint64_t res;
  uint64_t ctx;
  uint64_t cyc;
};
struct CmbPkt {
  uint16_t owner;
  uint64_t s[3];
  uint64_t ax;
};

// The bench driver. It is the island's other half: it accepts COMBINE
// packets, returns a final result after a configurable latency, and consumes
// the ordered output under a configurable ready policy.
class Ob {
 public:
  Vzhao_texture_v3own* d;
  uint64_t cyc = 0;

  bool cmb_ready = true;
  bool out_ready = true;
  bool auto_fin = true;
  int fin_lat = 1;
  bool fin_manual = false;

  struct FinJob {
    uint16_t owner;
    uint64_t res;
    uint64_t due;
  };
  std::deque<FinJob> finq;
  std::vector<Emit> emitted;
  std::vector<CmbPkt> combined;

  // observations taken on the settled pre-edge values
  bool obs_adm_ready = false;
  bool obs_adm_fire = false;
  uint16_t obs_adm_owner = 0;
  bool obs_out_fire = false;
  bool obs_cmb_fire = false;

  explicit Ob(Vzhao_texture_v3own* dut) : d(dut) {}

  void settle() {
    d->clk = 0;
    d->eval();
  }
  void edge() {
    d->clk = 1;
    d->eval();
    d->clk = 0;
    d->eval();
    cyc++;
  }

  void reset(int cycles = 4) {
    d->rst_n = 0;
    clear_inputs();
    d->cmb_ready_i = 0;
    d->out_ready_i = 0;
    d->eval();
    for (int i = 0; i < cycles; ++i) edge();
    d->rst_n = 1;
    d->eval();
    step();
  }

  void clear_inputs() {
    d->adm_valid_i = 0;
    d->adm_ctx_i = 0;
    d->adm_req_i = 0;
    d->iss_tmu_valid_i = 0;
    d->iss_tmu_handle_i = 0;
    d->iss_aux_valid_i = 0;
    d->iss_aux_owner_i = 0;
    d->tmu_rvalid_i = 0;
    d->tmu_rhandle_i = 0;
    d->tmu_rresult_i = 0;
    d->aux_rvalid_i = 0;
    d->aux_rowner_i = 0;
    d->aux_rresult_i = 0;
    if (!fin_manual) {
      d->fin_valid_i = 0;
      d->fin_owner_i = 0;
      d->fin_result_i = 0;
    }
  }

  // One clock. Inputs staged by the caller are sampled at the rising edge,
  // exactly like hardware, and the one-shot valids are dropped afterwards.
  void step() {
    d->cmb_ready_i = cmb_ready ? 1 : 0;
    d->out_ready_i = out_ready ? 1 : 0;

    bool drove_fin = false;
    if (auto_fin && !fin_manual) {
      d->fin_valid_i = 0;
      if (!finq.empty() && finq.front().due <= cyc) {
        d->fin_valid_i = 1;
        d->fin_owner_i = finq.front().owner;
        d->fin_result_i = finq.front().res;
        drove_fin = true;
      }
    }

    settle();

    obs_adm_ready = d->adm_ready_o != 0;
    obs_adm_fire = (d->adm_valid_i != 0) && obs_adm_ready;
    obs_adm_owner = static_cast<uint16_t>(d->adm_owner_o);

    obs_cmb_fire = (d->cmb_valid_o != 0) && (d->cmb_ready_i != 0);
    CmbPkt cp{};
    if (obs_cmb_fire) {
      cp.owner = static_cast<uint16_t>(d->cmb_owner_o);
      cp.s[0] = d->cmb_s0_o;
      cp.s[1] = d->cmb_s1_o;
      cp.s[2] = d->cmb_s2_o;
      cp.ax = d->cmb_aux_o;
    }

    obs_out_fire = (d->out_valid_o != 0) && (d->out_ready_i != 0);
    Emit e{};
    if (obs_out_fire) {
      e.owner = static_cast<uint16_t>(d->out_owner_o);
      e.res = d->out_result_o;
      e.ctx = d->out_ctx_o;
      e.cyc = cyc;
    }

    edge();

    if (drove_fin) finq.pop_front();
    if (obs_cmb_fire) {
      combined.push_back(cp);
      if (auto_fin) finq.push_back({cp.owner, final_of(cp.owner), cyc + static_cast<uint64_t>(fin_lat)});
    }
    if (obs_out_fire) emitted.push_back(e);

    clear_inputs();
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) step();
  }

  // Admit, waiting for ready. Returns the stamped owner handle.
  uint16_t admit(uint64_t ctx, uint8_t req, int budget = 4000) {
    for (int i = 0; i < budget; ++i) {
      d->adm_valid_i = 1;
      d->adm_ctx_i = ctx;
      d->adm_req_i = req;
      step();
      if (obs_adm_fire) return obs_adm_owner;
    }
    return 0xFFFF;
  }

  void issue_tmu(uint16_t owner, uint32_t sidx) {
    d->iss_tmu_valid_i = 1;
    d->iss_tmu_handle_i = smp(owner, sidx);
    step();
  }
  void issue_aux(uint16_t owner) {
    d->iss_aux_valid_i = 1;
    d->iss_aux_owner_i = owner;
    step();
  }
  void ret_tmu(uint16_t owner, uint32_t sidx, uint64_t res) {
    d->tmu_rvalid_i = 1;
    d->tmu_rhandle_i = smp(owner, sidx);
    d->tmu_rresult_i = res;
    step();
  }
  void ret_aux(uint16_t owner, uint64_t res) {
    d->aux_rvalid_i = 1;
    d->aux_rowner_i = owner;
    d->aux_rresult_i = res;
    step();
  }
  // Both terminal ports firing on the SAME clock (section 8.6).
  void ret_both(uint16_t towner, uint32_t sidx, uint64_t tres, uint16_t aowner,
                uint64_t ares) {
    d->tmu_rvalid_i = 1;
    d->tmu_rhandle_i = smp(towner, sidx);
    d->tmu_rresult_i = tres;
    d->aux_rvalid_i = 1;
    d->aux_rowner_i = aowner;
    d->aux_rresult_i = ares;
    step();
  }

  // Drive a whole owner's obligations and let it flow to the output.
  void issue_and_return_all(uint16_t owner, uint8_t req) {
    for (uint32_t s = 0; s < 3; ++s)
      if (req & (1u << s)) issue_tmu(owner, s);
    if (req & 8u) issue_aux(owner);
    for (uint32_t s = 0; s < 3; ++s)
      if (req & (1u << s)) ret_tmu(owner, s, mkres(owner * 4 + s));
    if (req & 8u) ret_aux(owner, mkres(owner * 4 + 3));
  }
};

void hdr(const char* s) { std::printf("\n--- %s\n", s); }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // UNBUFFERED STDOUT, and it is evidence machinery rather than tidiness.
  // A firing assertion ends the process through abort(), which does NOT flush
  // a redirected stdout -- so the first fire-test pass recorded four mutations
  // as "exit code 1, no output at all". That is indistinguishable from a
  // process that died before printing anything, and CLAUDE.md's build note
  // names the trap: "Buffered output lost in a crash makes a late fault look
  // like an early one". Unbuffered, the last line printed is the case that
  // actually tripped.
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  auto* dut = new Vzhao_texture_v3own;

  // =========================================================================
  hdr("case 1: end to end, three samples + AUX, in order, full 64-bit context");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    const int N = 8;
    std::vector<uint16_t> owners;
    for (int i = 0; i < N; ++i) {
      uint16_t o = s.admit(ctx_of(i), 0xF);
      owners.push_back(o);
      s.issue_and_return_all(o, 0xF);
    }
    s.idle(400);
    zhao::check(s.emitted.size() == static_cast<size_t>(N),
                "case1 emitted count", N, s.emitted.size());
    for (int i = 0; i < N && i < static_cast<int>(s.emitted.size()); ++i) {
      zhao::check(s.emitted[i].owner == owners[i], "case1 emission order",
                  owners[i], s.emitted[i].owner);
      zhao::check(s.emitted[i].ctx == ctx_of(i), "case1 full 64-bit context",
                  ctx_of(i), s.emitted[i].ctx);
      zhao::check(s.emitted[i].res == final_of(owners[i]), "case1 final result",
                  final_of(owners[i]), s.emitted[i].res);
    }
    // The COMBINE packet must carry THIS owner's real sample rows.
    zhao::check(s.combined.size() == static_cast<size_t>(N),
                "case1 combine packets", N, s.combined.size());
    for (int i = 0; i < N && i < static_cast<int>(s.combined.size()); ++i) {
      uint16_t o = owners[i];
      zhao::check(s.combined[i].owner == o, "case1 combine owner", o,
                  s.combined[i].owner);
      for (uint32_t k = 0; k < 3; ++k)
        zhao::check(s.combined[i].s[k] == mkres(o * 4 + k),
                    "case1 combine sample row", mkres(o * 4 + k),
                    s.combined[i].s[k]);
      zhao::check(s.combined[i].ax == mkres(o * 4 + 3), "case1 combine aux row",
                  mkres(o * 4 + 3), s.combined[i].ax);
    }
    zhao::check(dut->ev_admitted_o == static_cast<uint32_t>(N),
                "case1 admitted counter", N, dut->ev_admitted_o);
    zhao::check(dut->ev_emitted_o == static_cast<uint32_t>(N),
                "case1 emitted counter", N, dut->ev_emitted_o);
    zhao::check(dut->ev_commits_o == static_cast<uint32_t>(N * 4),
                "case1 commit counter", N * 4, dut->ev_commits_o);
    zhao::check(dut->ev_tickets_o == static_cast<uint32_t>(N),
                "case1 ticket counter (exactly one per owner)", N,
                dut->ev_tickets_o);
    zhao::check(dut->ev_err_range_o == 0, "case1 no range errors", 0,
                dut->ev_err_range_o);
    zhao::check(dut->ev_err_stale_o == 0, "case1 no stale errors", 0,
                dut->ev_err_stale_o);
    zhao::check(dut->ev_err_unsol_o == 0, "case1 no unsolicited errors", 0,
                dut->ev_err_unsol_o);
    zhao::check(dut->ev_err_dup_o == 0, "case1 no duplicate errors", 0,
                dut->ev_err_dup_o);
    zhao::check(dut->ev_err_final_o == 0, "case1 no final errors", 0,
                dut->ev_err_final_o);
    zhao::check(dut->ev_err_issue_o == 0, "case1 no issue errors", 0,
                dut->ev_err_issue_o);
    zhao::check(dut->ev_quiet_o == 1, "case1 island quiescent at end", 1,
                dut->ev_quiet_o);
    zhao::check(dut->ev_live_o == 0, "case1 no live owners at end", 0,
                dut->ev_live_o);
  }

  // =========================================================================
  hdr("case 2 (D.1): consecutive duplicate responses during a pipelined commit");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0x2);  // sample1 only
    s.issue_tmu(o, 1);
    // A at t0, B (identical) at t0+1. B's C1 snapshot cannot see A's claim;
    // only the recent-claim forwarding rejects it.
    s.ret_tmu(o, 1, mkres(0x11));
    s.ret_tmu(o, 1, mkres(0x22));  // the duplicate, one clock later
    s.idle(200);
    zhao::check(dut->ev_err_dup_o == 1, "D.1 duplicate count is exactly one", 1,
                dut->ev_err_dup_o);
    zhao::check(dut->ev_commits_o == 1, "D.1 payload write count is one", 1,
                dut->ev_commits_o);
    zhao::check(dut->ev_tickets_o == 1, "D.1 terminal count is one", 1,
                dut->ev_tickets_o);
    zhao::check(s.combined.size() == 1, "D.1 one combine packet", 1,
                s.combined.size());
    if (!s.combined.empty())
      zhao::check(s.combined[0].s[1] == mkres(0x11),
                  "D.1 the FIRST payload survives, not the duplicate",
                  mkres(0x11), s.combined[0].s[1]);
    zhao::check(s.emitted.size() == 1, "D.1 one emission", 1, s.emitted.size());
  }

  // =========================================================================
  hdr("case 3: spaced duplicate, after the forwarding window has closed");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0x1);
    s.issue_tmu(o, 0);
    s.ret_tmu(o, 0, mkres(0x31));
    s.idle(6);                     // claimed and committed are both visible now
    s.ret_tmu(o, 0, mkres(0x32));  // rejected by claimed/committed, not by fwd
    s.idle(200);
    zhao::check(dut->ev_err_dup_o == 1, "spaced duplicate rejected once", 1,
                dut->ev_err_dup_o);
    zhao::check(dut->ev_commits_o == 1, "spaced duplicate: one commit", 1,
                dut->ev_commits_o);
    if (!s.combined.empty())
      zhao::check(s.combined[0].s[0] == mkres(0x31),
                  "spaced duplicate did not overwrite", mkres(0x31),
                  s.combined[0].s[0]);
  }

  // =========================================================================
  hdr("case 4: stale generation");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0x1);
    uint16_t stale = owner_of(slot_of(o), gen_of(o) - 1);
    s.issue_tmu(o, 0);
    s.ret_tmu(stale, 0, mkres(0x41));  // right slot, wrong generation
    s.idle(4);
    zhao::check(dut->ev_err_stale_o == 1, "stale generation rejected", 1,
                dut->ev_err_stale_o);
    zhao::check(dut->ev_commits_o == 0, "stale generation wrote nothing", 0,
                dut->ev_commits_o);
    // A response for a slot that is not live at all is the same class.
    s.ret_tmu(owner_of(40, 7), 0, mkres(0x42));
    s.idle(4);
    zhao::check(dut->ev_err_stale_o == 2, "not-live owner rejected as stale", 2,
                dut->ev_err_stale_o);
    // and the owner still completes normally afterwards
    s.ret_tmu(o, 0, mkres(0x43));
    s.idle(200);
    zhao::check(s.emitted.size() == 1, "owner still completes after stale", 1,
                s.emitted.size());
  }

  // =========================================================================
  hdr("case 5/6: unsolicited -- required-but-not-issued, and not-required");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0x1);  // requires sample0 only
    s.ret_tmu(o, 0, mkres(0x51));          // required, never issued
    s.idle(4);
    zhao::check(dut->ev_err_unsol_o == 1, "required-but-not-issued rejected", 1,
                dut->ev_err_unsol_o);
    s.issue_tmu(o, 0);
    s.ret_tmu(o, 2, mkres(0x52));  // sample2 is not required at all
    s.idle(4);
    zhao::check(dut->ev_err_unsol_o == 2, "not-required source rejected", 2,
                dut->ev_err_unsol_o);
    s.ret_aux(o, mkres(0x53));  // AUX not required
    s.idle(4);
    zhao::check(dut->ev_err_unsol_o == 3, "unsolicited AUX rejected", 3,
                dut->ev_err_unsol_o);
    zhao::check(dut->ev_commits_o == 0, "no unsolicited packet wrote a bank", 0,
                dut->ev_commits_o);
    // An ISSUE for a source that was never required is refused too.
    s.issue_tmu(o, 2);
    s.idle(2);
    zhao::check(dut->ev_err_issue_o == 1, "issue for a non-required source", 1,
                dut->ev_err_issue_o);
    s.ret_tmu(o, 0, mkres(0x54));
    s.idle(200);
    zhao::check(s.emitted.size() == 1, "owner completes after unsolicited", 1,
                s.emitted.size());
  }

  // =========================================================================
  hdr("case 7: out-of-range sample_index 3 on the three-bank texture port");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0x1);
    s.issue_tmu(o, 0);
    s.ret_tmu(o, 3, mkres(0x71));  // encoding 3 is reserved (section 5.1)
    s.idle(4);
    zhao::check(dut->ev_err_range_o == 1, "sample_index 3 rejected by range", 1,
                dut->ev_err_range_o);
    zhao::check(dut->ev_err_unsol_o == 0,
                "range fault is NOT counted as unsolicited", 0,
                dut->ev_err_unsol_o);
    zhao::check(dut->ev_commits_o == 0, "index 3 wrote no bank", 0,
                dut->ev_commits_o);
    // An issue with index 3 is refused at its own port.
    s.d->iss_tmu_valid_i = 1;
    s.d->iss_tmu_handle_i = smp(o, 3);
    s.step();
    s.idle(2);
    zhao::check(dut->ev_err_issue_o == 1, "issue with index 3 refused", 1,
                dut->ev_err_issue_o);
    s.ret_tmu(o, 0, mkres(0x72));
    s.idle(200);
    zhao::check(s.emitted.size() == 1, "owner completes after range fault", 1,
                s.emitted.size());
  }

  // =========================================================================
  hdr("case 8 (D.2): simultaneous TMU + AUX complete ONE owner -> ONE ticket");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0xF);  // required = 1111
    for (uint32_t k = 0; k < 3; ++k) s.issue_tmu(o, k);
    s.issue_aux(o);
    s.ret_tmu(o, 0, mkres(0x80));
    s.ret_tmu(o, 1, mkres(0x81));  // committed becomes 0011
    s.idle(6);
    // this edge publishes texture source2 AND AUX for the same full handle
    s.ret_both(o, 2, mkres(0x82), o, mkres(0x83));
    s.idle(200);
    zhao::check(dut->ev_tickets_o == 1, "D.2 exactly ONE ready ticket", 1,
                dut->ev_tickets_o);
    zhao::check(dut->ev_commits_o == 4, "D.2 four commits", 4,
                dut->ev_commits_o);
    zhao::check(s.combined.size() == 1, "D.2 one combine admission", 1,
                s.combined.size());
    if (!s.combined.empty()) {
      zhao::check(s.combined[0].s[2] == mkres(0x82), "D.2 sample2 row",
                  mkres(0x82), s.combined[0].s[2]);
      zhao::check(s.combined[0].ax == mkres(0x83), "D.2 aux row", mkres(0x83),
                  s.combined[0].ax);
    }
    zhao::check(s.emitted.size() == 1, "D.2 one emission", 1,
                s.emitted.size());
  }

  // =========================================================================
  hdr("case 9 (D.2): simultaneous TMU + AUX complete TWO owners -> TWO tickets");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t h = s.admit(ctx_of(0), 0x1);  // H needs sample0
    uint16_t j = s.admit(ctx_of(1), 0x8);  // J needs AUX
    s.issue_tmu(h, 0);
    s.issue_aux(j);
    s.ret_both(h, 0, mkres(0x90), j, mkres(0x91));
    s.idle(300);
    zhao::check(dut->ev_tickets_o == 2, "D.2 two tickets, separate queues", 2,
                dut->ev_tickets_o);
    zhao::check(s.emitted.size() == 2, "D.2 two emissions", 2,
                s.emitted.size());
    if (s.emitted.size() == 2) {
      zhao::check(s.emitted[0].owner == h, "D.2 H emitted first (admit order)",
                  h, s.emitted[0].owner);
      zhao::check(s.emitted[1].owner == j, "D.2 J emitted second", j,
                  s.emitted[1].owner);
    }
  }

  // =========================================================================
  hdr("case 10: simultaneous TMU + AUX identity faults count TWO, not one");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(0), 0xF);
    uint16_t bad = owner_of(slot_of(o), gen_of(o) + 3);
    s.ret_both(bad, 0, mkres(0xA0), bad, mkres(0xA1));
    s.idle(6);
    zhao::check(dut->ev_err_stale_o == 2,
                "the lost-update defect: two faults, two counts", 2,
                dut->ev_err_stale_o);
    // and the same for the unsolicited class on one edge
    s.ret_both(o, 0, mkres(0xA2), o, mkres(0xA3));  // required, never issued
    s.idle(6);
    zhao::check(dut->ev_err_unsol_o == 2, "two unsolicited faults, two counts",
                2, dut->ev_err_unsol_o);
    zhao::check(dut->ev_commits_o == 0, "no faulted packet wrote a bank", 0,
                dut->ev_commits_o);
  }

  // =========================================================================
  hdr("case 11 (D.4): an unstoppable terminal producer vs a stalled output");
  // =========================================================================
  // D.4's shape: a producer with no return-ready interface pulses while the
  // completion path is stalled. Here the terminal ports are unconditionally
  // ready by construction (section 19.1 reserved-fixed-latency), and the place
  // the pulse waits is the owner's own bank row. The hazard is that a stalled
  // OUTPUT could make a terminal return disappear. It must not.
  {
    Ob s(dut);
    s.reset();
    s.out_ready = false;  // output completely stopped from the start
    std::vector<uint16_t> owners;
    for (int i = 0; i < 16; ++i) {
      uint16_t o = s.admit(ctx_of(i), 0xF);
      owners.push_back(o);
    }
    // Terminal returns at the maximum rate the ports allow, one per clock.
    for (int i = 0; i < 16; ++i)
      for (uint32_t k = 0; k < 3; ++k) s.issue_tmu(owners[i], k);
    for (int i = 0; i < 16; ++i) s.issue_aux(owners[i]);
    for (int i = 0; i < 16; ++i) {
      for (uint32_t k = 0; k < 3; ++k) s.ret_tmu(owners[i], k, mkres(owners[i] * 4 + k));
      s.ret_aux(owners[i], mkres(owners[i] * 4 + 3));
    }
    s.idle(400);
    zhao::check(dut->ev_commits_o == 64, "D.4 every terminal return committed",
                64, dut->ev_commits_o);
    zhao::check(dut->ev_err_dup_o == 0, "D.4 no spurious duplicate rejection", 0,
                dut->ev_err_dup_o);
    zhao::check(dut->ev_err_unsol_o == 0, "D.4 no spurious unsolicited", 0,
                dut->ev_err_unsol_o);
    zhao::check(s.emitted.empty(), "D.4 nothing emitted while stalled", 0,
                s.emitted.size());
    zhao::check(dut->ev_live_o == 16, "D.4 all 16 owners still live", 16,
                dut->ev_live_o);
    // release
    s.out_ready = true;
    s.idle(400);
    zhao::check(s.emitted.size() == 16, "D.4 all 16 emitted after release", 16,
                s.emitted.size());
    for (size_t i = 0; i < s.emitted.size(); ++i)
      zhao::check(s.emitted[i].ctx == ctx_of(static_cast<uint32_t>(i)),
                  "D.4 context intact through the stall",
                  ctx_of(static_cast<uint32_t>(i)), s.emitted[i].ctx);
  }

  // =========================================================================
  hdr("case 12: finite backpressure -- nothing destroyed, order preserved");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    const int N = 40;
    std::vector<uint16_t> owners;
    // out_ready toggles on an awkward period so the handshake is exercised in
    // both directions rather than staying in one regime.
    for (int i = 0; i < N; ++i) {
      s.out_ready = ((i % 7) < 2);
      uint16_t o = s.admit(ctx_of(i), (i % 2) ? 0x7 : 0xB);
      owners.push_back(o);
      s.issue_and_return_all(o, (i % 2) ? 0x7 : 0xB);
      s.out_ready = ((i % 5) != 0);
    }
    s.out_ready = false;
    s.idle(120);
    s.out_ready = true;
    s.idle(2000);
    zhao::check(s.emitted.size() == static_cast<size_t>(N),
                "backpressure: every owner emitted exactly once", N,
                s.emitted.size());
    for (int i = 0; i < N && i < static_cast<int>(s.emitted.size()); ++i) {
      zhao::check(s.emitted[i].owner == owners[i], "backpressure: strict order",
                  owners[i], s.emitted[i].owner);
      zhao::check(s.emitted[i].ctx == ctx_of(i), "backpressure: context", ctx_of(i),
                  s.emitted[i].ctx);
      zhao::check(s.emitted[i].res == final_of(owners[i]),
                  "backpressure: result", final_of(owners[i]), s.emitted[i].res);
    }
    zhao::check(dut->ev_emitted_o == static_cast<uint32_t>(N),
                "backpressure: emitted counter", N, dut->ev_emitted_o);
  }

  // =========================================================================
  hdr("case 13 (D.6): the final write does not free the owner; stall at wrap");
  // =========================================================================
  // "Stall through wrap and use unique high context bits to detect this
  // independently of RGB." If credit were returned at the final write, the
  // ring could wrap and a new owner would replace OWNER_CONTEXT[H] before the
  // old output context is read.
  {
    Ob s(dut);
    s.reset();
    s.out_ready = false;
    std::vector<uint16_t> owners;
    for (int i = 0; i < OWNERS; ++i) {
      uint16_t o = s.admit(ctx_of(1000 + i), 0x1);
      owners.push_back(o);
      s.issue_tmu(o, 0);
      s.ret_tmu(o, 0, mkres(o));
    }
    s.idle(600);  // every owner has been combined and written FINAL_RESULT
    zhao::check(dut->ev_live_o == OWNERS,
                "D.6 all 64 still live after their final writes", OWNERS,
                dut->ev_live_o);
    // The ring is full: admission must refuse, whatever the caller does.
    for (int i = 0; i < 8; ++i) {
      s.d->adm_valid_i = 1;
      s.d->adm_ctx_i = ctx_of(0xDEAD);
      s.d->adm_req_i = 0x1;
      s.step();
      zhao::check(!s.obs_adm_fire,
                  "D.6 admission refused while owners hold their outputs", 0,
                  s.obs_adm_fire ? 1 : 0);
    }
    s.out_ready = true;
    s.idle(600);
    zhao::check(s.emitted.size() == static_cast<size_t>(OWNERS),
                "D.6 all 64 emitted", OWNERS, s.emitted.size());
    for (int i = 0; i < OWNERS && i < static_cast<int>(s.emitted.size()); ++i)
      zhao::check(s.emitted[i].ctx == ctx_of(1000 + i),
                  "D.6 context belongs to the OLD owner, not a new admission",
                  ctx_of(1000 + i), s.emitted[i].ctx);
  }

  // =========================================================================
  hdr("case 14: more than 64 admissions under backpressure -- ready must fall");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    s.out_ready = false;
    int accepted = 0;
    std::vector<uint16_t> owners;
    for (int i = 0; i < 200; ++i) {
      s.d->adm_valid_i = 1;
      s.d->adm_ctx_i = ctx_of(2000 + i);
      s.d->adm_req_i = 0x1;
      s.step();
      if (s.obs_adm_fire) {
        owners.push_back(s.obs_adm_owner);
        ++accepted;
      }
    }
    zhao::check(accepted == OWNERS,
                "exactly 64 admissions accepted before ready fell", OWNERS,
                accepted);
    zhao::check(dut->ev_live_peak_o == OWNERS, "peak live is exactly 64", OWNERS,
                dut->ev_live_peak_o);
    for (size_t i = 0; i < owners.size(); ++i)
      s.issue_tmu(owners[i], 0);
    for (size_t i = 0; i < owners.size(); ++i)
      s.ret_tmu(owners[i], 0, mkres(owners[i]));
    s.out_ready = true;
    s.idle(1200);
    zhao::check(s.emitted.size() == static_cast<size_t>(OWNERS),
                "all 64 emitted after release", OWNERS, s.emitted.size());
    for (int i = 0; i < OWNERS && i < static_cast<int>(s.emitted.size()); ++i)
      zhao::check(s.emitted[i].ctx == ctx_of(2000 + i),
                  "no live row was overwritten", ctx_of(2000 + i),
                  s.emitted[i].ctx);
  }

  // =========================================================================
  hdr("case 15: reset mid-transaction -- no ghost output, full function after");
  // =========================================================================
  {
    Ob s(dut);
    s.reset();
    std::vector<uint16_t> owners;
    for (int i = 0; i < 6; ++i) {
      uint16_t o = s.admit(ctx_of(3000 + i), 0xF);
      owners.push_back(o);
      for (uint32_t k = 0; k < 3; ++k) s.issue_tmu(o, k);
      s.issue_aux(o);
    }
    // returns land, then reset arrives with work in every stage
    for (int i = 0; i < 6; ++i) {
      s.ret_tmu(owners[i], 0, mkres(1));
      s.ret_tmu(owners[i], 1, mkres(2));
    }
    s.ret_both(owners[0], 2, mkres(3), owners[0], mkres(4));
    size_t before = s.emitted.size();
    s.reset();  // ASSERTED MID-TRANSACTION
    s.idle(50);
    zhao::check(s.emitted.size() == before, "reset produced no ghost output",
                before, s.emitted.size());
    zhao::check(dut->ev_live_o == 0, "reset cleared live owners", 0,
                dut->ev_live_o);
    zhao::check(dut->ev_quiet_o == 1, "reset left the island quiescent", 1,
                dut->ev_quiet_o);
    // Section 6.5: reset does NOT clear the payload arrays, and it must not
    // matter -- the machine works completely on the next epoch.
    s.finq.clear();
    uint16_t o = s.admit(ctx_of(4242), 0x7);
    s.issue_and_return_all(o, 0x7);
    s.idle(300);
    zhao::check(s.emitted.size() == before + 1,
                "full function after a mid-transaction reset", before + 1,
                s.emitted.size());
    if (s.emitted.size() == before + 1) {
      zhao::check(s.emitted.back().ctx == ctx_of(4242),
                  "post-reset context correct", ctx_of(4242),
                  s.emitted.back().ctx);
      zhao::check(s.emitted.back().res == final_of(o), "post-reset result",
                  final_of(o), s.emitted.back().res);
    }
  }

  // =========================================================================
  hdr("case 16: ordered retirement under OUT-OF-ORDER readiness");
  // =========================================================================
  // Section 9.5: a fully ready younger fragment may use idle COMBINE capacity,
  // but section 18.3 says external emission cannot skip the hole.
  {
    Ob s(dut);
    s.reset();
    const int N = 12;
    std::vector<uint16_t> owners;
    for (int i = 0; i < N; ++i) {
      uint16_t o = s.admit(ctx_of(5000 + i), 0x1);
      owners.push_back(o);
      s.issue_tmu(o, 0);
    }
    // Complete them BACKWARDS. Owner 0 -- the emit head -- is completed last.
    for (int i = N - 1; i >= 1; --i) s.ret_tmu(owners[i], 0, mkres(owners[i]));
    s.idle(300);
    zhao::check(s.emitted.empty(),
                "nothing emitted while the emit head is incomplete", 0,
                s.emitted.size());
    zhao::check(s.combined.size() == static_cast<size_t>(N - 1),
                "younger fragments DID use COMBINE out of order", N - 1,
                s.combined.size());
    s.ret_tmu(owners[0], 0, mkres(owners[0]));
    s.idle(400);
    zhao::check(s.emitted.size() == static_cast<size_t>(N),
                "the hole filled, all emitted", N, s.emitted.size());
    for (int i = 0; i < N && i < static_cast<int>(s.emitted.size()); ++i)
      zhao::check(s.emitted[i].owner == owners[i],
                  "emission is in ADMISSION order, not readiness order",
                  owners[i], s.emitted[i].owner);
  }

  // =========================================================================
  hdr("case 17 (D.3): the COMBINE read reservation lasts through residence");
  // =========================================================================
  // "The queue alone appears to have a free slot... It does not release a
  // reservation. Only a consumer pop makes reserved=3." Transposed: with the
  // COMBINE consumer stopped, at most CMBQD owners may be popped from the
  // ready queues, and none of the rest may be lost.
  {
    Ob s(dut);
    s.reset();
    s.cmb_ready = false;  // COMBINE consumer stopped
    const int N = 20;
    std::vector<uint16_t> owners;
    for (int i = 0; i < N; ++i) {
      uint16_t o = s.admit(ctx_of(6000 + i), 0x1);
      owners.push_back(o);
      s.issue_tmu(o, 0);
      s.ret_tmu(o, 0, mkres(o));
    }
    s.idle(400);
    zhao::check(s.combined.empty(), "D.3 nothing combined while stopped", 0,
                s.combined.size());
    zhao::check(dut->ev_tickets_o == static_cast<uint32_t>(N),
                "D.3 every owner still got its ticket", N, dut->ev_tickets_o);
    s.cmb_ready = true;
    s.idle(800);
    zhao::check(s.combined.size() == static_cast<size_t>(N),
                "D.3 all owners combined after release, none stranded", N,
                s.combined.size());
    zhao::check(s.emitted.size() == static_cast<size_t>(N),
                "D.3 all emitted", N, s.emitted.size());
    for (int i = 0; i < N && i < static_cast<int>(s.emitted.size()); ++i)
      zhao::check(s.emitted[i].owner == owners[i], "D.3 order held", owners[i],
                  s.emitted[i].owner);
  }

  // =========================================================================
  hdr("case 18: duplicate and unsolicited FINAL returns");
  // =========================================================================
  // Section 18.4: "A duplicate final write is a protocol error and is rejected
  // by the narrow final_claimed/combine_issued state, not allowed to overwrite
  // a queued output."
  {
    Ob s(dut);
    s.reset();
    s.auto_fin = false;
    s.fin_manual = true;
    uint16_t o = s.admit(ctx_of(7000), 0x1);
    s.issue_tmu(o, 0);
    s.ret_tmu(o, 0, mkres(o));
    s.idle(30);
    zhao::check(s.combined.size() == 1, "final: owner reached COMBINE", 1,
                s.combined.size());
    // A final return for an owner that has NOT been combine-issued.
    uint16_t never = owner_of(30, 1);
    dut->fin_valid_i = 1;
    dut->fin_owner_i = never;
    dut->fin_result_i = mkres(0xBAD);
    s.step();
    dut->fin_valid_i = 0;
    s.idle(6);
    zhao::check(dut->ev_err_final_o == 1, "unsolicited final rejected", 1,
                dut->ev_err_final_o);
    // The real final, then two duplicates: one inside the forwarding window,
    // one after it.
    dut->fin_valid_i = 1;
    dut->fin_owner_i = o;
    dut->fin_result_i = final_of(o);
    s.step();
    dut->fin_valid_i = 1;
    dut->fin_owner_i = o;
    dut->fin_result_i = mkres(0xDEAD);  // +1: caught by recent-claim forwarding
    s.step();
    // +2 IS THE WINDOW WHERE final_claimed IS THE ONLY DEFENCE, and the first
    // version of this case did not cover it. At +1 the C3 forwarding record
    // catches the duplicate; at +10 final_done is already set and catches it.
    // Only at +2 has the snapshot advanced past the forwarding record while
    // final_done has not yet risen -- exactly the gap section 18.4 puts
    // final_claimed in to close ("final_done is not used as a substitute for
    // the earlier claim while a final write is still in flight").
    //
    // Found by a fire test: mutating final_claimed away did not make this case
    // fail, which meant the case was not testing it.
    dut->fin_valid_i = 1;
    dut->fin_owner_i = o;
    dut->fin_result_i = mkres(0xFEED);  // +2: only final_claimed rejects this
    s.step();
    dut->fin_valid_i = 0;
    s.idle(10);
    dut->fin_valid_i = 1;
    dut->fin_owner_i = o;
    dut->fin_result_i = mkres(0xBEEF);  // spaced duplicate, caught by final_done
    s.step();
    dut->fin_valid_i = 0;
    s.idle(200);
    zhao::check(dut->ev_err_final_o == 4, "all three final duplicates rejected",
                4, dut->ev_err_final_o);
    zhao::check(s.emitted.size() == 1, "one emission", 1, s.emitted.size());
    if (!s.emitted.empty())
      zhao::check(s.emitted[0].res == final_of(o),
                  "the FIRST final result survives both duplicates",
                  final_of(o), s.emitted[0].res);
  }

  // =========================================================================
  hdr("case 19: generation wrap takes the baseline drain, on a quiet island");
  // =========================================================================
  // Section 5.5: eight generation bits do not abolish wrap; the enforceable
  // policy does. Slot 0 must reach generation 255 and its next allocation must
  // be refused until the whole island is quiescent.
  {
    Ob s(dut);
    s.reset();
    // 255 reuses of every slot brings slot 0 to generation 255.
    const int TOTAL = OWNERS * 255;
    int admitted = 0;
    bool saw_block_while_busy = false;
    bool quiet_when_wrap_admitted = true;
    uint32_t drains_before = 0;
    // Run the ring hot: admit whenever ready, complete immediately.
    std::deque<uint16_t> inflight;
    uint64_t guard = 0;
    while (admitted < TOTAL + OWNERS + 4 && guard < 4000000ULL) {
      ++guard;
      bool want_admit = (admitted < TOTAL + OWNERS + 4);
      if (want_admit) {
        s.d->adm_valid_i = 1;
        s.d->adm_ctx_i = ctx_of(static_cast<uint32_t>(admitted));
        s.d->adm_req_i = 0x1;
      }
      // one issue/return per clock for the oldest owner needing it
      if (!inflight.empty()) {
        uint16_t o = inflight.front();
        inflight.pop_front();
        s.d->iss_tmu_valid_i = 1;
        s.d->iss_tmu_handle_i = smp(o, 0);
        s.d->tmu_rvalid_i = 1;
        s.d->tmu_rhandle_i = smp(o, 0);
        s.d->tmu_rresult_i = mkres(o);
      }
      uint32_t drains_now = dut->ev_wrap_drains_o;
      bool was_quiet = dut->ev_quiet_o != 0;
      bool adm_ready_pre;
      s.step();
      adm_ready_pre = s.obs_adm_ready;
      if (s.obs_adm_fire) {
        inflight.push_back(s.obs_adm_owner);
        ++admitted;
      } else if (want_admit && !adm_ready_pre && dut->ev_live_o != OWNERS) {
        // refused for a reason other than a full ring: that is the wrap gate
        saw_block_while_busy = true;
      }
      if (dut->ev_wrap_drains_o != drains_now && !was_quiet)
        quiet_when_wrap_admitted = false;
      drains_before = drains_now;
    }
    (void)drains_before;
    // The issue/return above is one clock behind admission, so drain the rest.
    while (!inflight.empty()) {
      uint16_t o = inflight.front();
      inflight.pop_front();
      s.d->iss_tmu_valid_i = 1;
      s.d->iss_tmu_handle_i = smp(o, 0);
      s.d->tmu_rvalid_i = 1;
      s.d->tmu_rhandle_i = smp(o, 0);
      s.d->tmu_rresult_i = mkres(o);
      s.step();
    }
    s.idle(2000);
    zhao::check(dut->ev_wrap_drains_o > 0,
                "the generation-wrap drain actually fired", 1,
                dut->ev_wrap_drains_o > 0 ? 1 : 0);
    zhao::check(saw_block_while_busy,
                "admission was blocked for wrap while the ring was NOT full", 1,
                saw_block_while_busy ? 1 : 0);
    zhao::check(quiet_when_wrap_admitted,
                "every wrapping admission happened on a QUIESCENT island", 1,
                quiet_when_wrap_admitted ? 1 : 0);
    zhao::check(dut->ev_admitted_o == dut->ev_emitted_o,
                "wrap run: admitted == emitted, nothing lost",
                dut->ev_admitted_o, dut->ev_emitted_o);
    zhao::check(dut->ev_err_stale_o == 0, "wrap run: no stale rejections", 0,
                dut->ev_err_stale_o);
    zhao::check(dut->ev_err_dup_o == 0, "wrap run: no duplicate rejections", 0,
                dut->ev_err_dup_o);
    zhao::check(dut->ev_err_unsol_o == 0, "wrap run: no unsolicited", 0,
                dut->ev_err_unsol_o);
  }

  // =========================================================================
  hdr("case 20 (18.3): sustained emission rate through the retirement path");
  // =========================================================================
  // "With contiguous completed owners and an always-ready consumer, the
  // pipelined read/output path must sustain one fragment per clock after
  // warmup. An FSM that returns to IDLE after every handshake ... is not
  // accepted as an equivalent throughput implementation."
  {
    Ob s(dut);
    s.reset();
    s.out_ready = true;
    const int N = 64;
    for (int i = 0; i < N; ++i) s.admit(ctx_of(8000 + i), 0x0);  // zero-work
    s.idle(1500);
    zhao::check(s.emitted.size() == static_cast<size_t>(N),
                "throughput run: all emitted", N, s.emitted.size());
    if (s.emitted.size() == static_cast<size_t>(N)) {
      uint64_t span = s.emitted.back().cyc - s.emitted.front().cyc + 1;
      std::printf("    emission span for %d owners: %llu cycles\n", N,
                  static_cast<unsigned long long>(span));
      zhao::check(span <= 96,
                  "64 owners emit within 96 cycles (>=0.66/clock sustained)", 96,
                  span);
      // The strong form: at least one run of 8 back-to-back emissions.
      int best = 0, run = 0;
      for (size_t i = 1; i < s.emitted.size(); ++i) {
        if (s.emitted[i].cyc == s.emitted[i - 1].cyc + 1) {
          ++run;
          if (run > best) best = run;
        } else {
          run = 0;
        }
      }
      std::printf("    longest back-to-back emission run: %d\n", best + 1);
      zhao::check(best + 1 >= 8,
                  "the output path emits back to back, not one-per-FSM-trip", 8,
                  best + 1);
    }
  }

  // =========================================================================
  hdr("case 21 (C11): a legal ERROR completion satisfies its required bit");
  // =========================================================================
  // Section 8.3: "A genuine service failure with a valid owner is still a
  // first completion. Its status is nonzero and its declared fallback payload
  // is committed. A stale, duplicate, or unsolicited packet is not a second
  // completion for that owner. Do not confuse these two classes of error."
  //
  // So the check that matters is not just that the owner finishes -- it is
  // that NO identity counter moves. An implementation that routed error
  // completions through the rejection path would leave the owner stuck
  // forever with its required bit unsatisfiable (Appendix D.10).
  {
    Ob s(dut);
    s.reset();
    uint16_t o = s.admit(ctx_of(9000), 0x3);
    s.issue_tmu(o, 0);
    s.issue_tmu(o, 1);
    // STATUS8 = bit3 TRANSPORT_TERMINAL_ERROR, with a declared fallback payload
    const uint64_t err_res = (0x08ULL << 32) | 0x00FF00FFULL;
    s.ret_tmu(o, 0, err_res);
    s.ret_tmu(o, 1, mkres(0xC11));
    s.idle(300);
    const uint32_t id_faults = dut->ev_err_stale_o + dut->ev_err_unsol_o +
                               dut->ev_err_dup_o + dut->ev_err_range_o;
    zhao::check(id_faults == 0,
                "C11 a service error is NOT an identity fault", 0, id_faults);
    zhao::check(dut->ev_commits_o == 2, "C11 the error completion committed", 2,
                dut->ev_commits_o);
    zhao::check(dut->ev_tickets_o == 1,
                "C11 the error satisfied its required bit", 1,
                dut->ev_tickets_o);
    zhao::check(s.combined.size() == 1, "C11 the owner reached COMBINE", 1,
                s.combined.size());
    if (!s.combined.empty()) {
      zhao::check(s.combined[0].s[0] == err_res,
                  "C11 STATUS8 and the fallback payload both survive to COMBINE",
                  err_res, s.combined[0].s[0]);
      zhao::check((s.combined[0].s[0] >> 32) == 0x08,
                  "C11 the status byte is readable at COMBINE admission", 0x08,
                  s.combined[0].s[0] >> 32);
    }
    zhao::check(s.emitted.size() == 1, "C11 emitted exactly once", 1,
                s.emitted.size());
  }

  const int rc = zhao::report_and_exit("texture_v3own_adversarial");
  delete dut;
  zhao::exit_hard(rc);
}
