// texture_v3own_bubble_control.cpp
//
// Preload more completed owners than the output queue can hold, keep the
// consumer closed, then release it. The ordered edge must handshake every clock
// after its first beat until the prepared run drains. This is deliberately
// separate from the owner's conservative full-to-free ADMISSION bubble.
//
// Built a second time against the renamed no-same-edge-reload mutant. That lane
// has inverse polarity and passes only when an alternating drain bubble appears.
#include "Vzhao_texture_v3own.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

namespace {

using Dut = Vzhao_texture_v3own;

uint64_t context_of(unsigned index) {
  return (0xBABB1E00ull + index * 0x101ull) << 32 |
         (0xC0DE0000ull + index * 0x10007ull);
}
uint64_t final_of(uint16_t owner) {
  return (0x5Aull << 32) | (static_cast<uint64_t>(owner) * 0x10203ull + 0x77ull);
}

struct FinalJob {
  uint16_t owner;
  uint64_t result;
  uint64_t due;
};

struct Driver {
  Dut* d;
  uint64_t cycle = 0;
  std::deque<FinalJob> finals;
  std::vector<uint16_t> combined;
  std::vector<uint16_t> emitted_owner;
  std::vector<uint64_t> emitted_ctx;
  std::vector<uint64_t> emitted_result;
  std::vector<uint64_t> emitted_cycle;
  bool out_ready = false;
  bool obs_admit = false;
  uint16_t obs_admit_owner = 0;

  explicit Driver(Dut* dut) : d(dut) {}

  void clear_one_shots() {
    d->adm_valid_i = 0;
    d->iss_tmu_valid_i = 0;
    d->iss_aux_valid_i = 0;
    d->tmu_rvalid_i = 0;
    d->aux_rvalid_i = 0;
    d->fin_valid_i = 0;
  }

  void initialize_inputs() {
    d->adm_ctx_i = 0;
    d->adm_req_i = 0;
    d->iss_tmu_handle_i = 0;
    d->iss_aux_owner_i = 0;
    d->tmu_rhandle_i = 0;
    d->tmu_rresult_i = 0;
    d->aux_rowner_i = 0;
    d->aux_rresult_i = 0;
    d->cmb_ready_i = 1;
    d->src_rd_valid_i = 0;
    d->src_rd_slot_i = 0;
    d->fin_owner_i = 0;
    d->fin_result_i = 0;
    d->out_ready_i = 0;
    clear_one_shots();
  }

  void edge() {
    d->clk = 1;
    d->eval();
    d->clk = 0;
    d->eval();
    ++cycle;
  }

  void reset() {
    initialize_inputs();
    d->rst_n = 0;
    d->clk = 0;
    d->eval();
    for (int i = 0; i < 4; ++i) edge();
    d->rst_n = 1;
    d->eval();
    step();
  }

  void step() {
    d->cmb_ready_i = 1;
    d->out_ready_i = out_ready ? 1 : 0;
    bool drove_final = false;
    if (!finals.empty() && finals.front().due <= cycle) {
      d->fin_valid_i = 1;
      d->fin_owner_i = finals.front().owner;
      d->fin_result_i = finals.front().result;
      drove_final = true;
    }

    d->clk = 0;
    d->eval();
    obs_admit = d->adm_valid_i && d->adm_ready_o;
    obs_admit_owner = static_cast<uint16_t>(d->adm_owner_o);
    const bool cmb_fire = d->cmb_valid_o && d->cmb_ready_i;
    const bool out_fire = d->out_valid_o && d->out_ready_i;
    const uint16_t cmb_owner = static_cast<uint16_t>(d->cmb_owner_o);
    const uint16_t out_owner = static_cast<uint16_t>(d->out_owner_o);
    const uint64_t out_ctx = d->out_ctx_o;
    const uint64_t out_result = d->out_result_o;

    edge();
    if (drove_final) finals.pop_front();
    if (cmb_fire) {
      combined.push_back(cmb_owner);
      // Not the reservation edge: wait until COMBINE acceptance has published
      // combine-issued before returning the terminal final.
      finals.push_back({cmb_owner, final_of(cmb_owner), cycle + 2});
    }
    if (out_fire) {
      emitted_owner.push_back(out_owner);
      emitted_ctx.push_back(out_ctx);
      emitted_result.push_back(out_result);
      emitted_cycle.push_back(cycle - 1);
    }
    clear_one_shots();
  }

  uint16_t admit(uint64_t ctx) {
    for (int guard = 0; guard < 1000; ++guard) {
      d->adm_valid_i = 1;
      d->adm_ctx_i = ctx;
      d->adm_req_i = 0;  // zero-work still takes the full owner/COMBINE/final path
      step();
      if (obs_admit) return obs_admit_owner;
    }
    return 0xFFFFu;
  }
};

}  // namespace

int main(int argc, char** argv) {
#ifdef ZHAO_BYPASS_POINTER_ASSERT_CONTROL
  VerilatedContext context;
  context.commandArgs(argc, argv);
  context.fatalOnError(false);
  auto* dut = new Dut{&context};
#else
  Verilated::commandArgs(argc, argv);
  auto* dut = new Dut;
#endif
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Driver s(dut);
  s.reset();

#ifdef ZHAO_BYPASS_POINTER_ASSERT_CONTROL
  if (context.gotError()) {
    std::fprintf(stderr, "FAIL: bypass-pointer control asserted during reset\n");
    delete dut;
    zhao::exit_hard(2);
  }
  const uint16_t owner = s.admit(context_of(0));
  for (int i = 0; i < 1000 && !context.gotError(); ++i) s.step();
  const bool fired = context.gotError();
  std::printf("V3OWN_BYPASS_POINTER_ASSERT_CONTROL owner=%u fired=%d emitted=%u\n",
              owner, fired ? 1 : 0, dut->ev_emitted_o);
  context.gotError(false);
  context.gotFinish(false);
  delete dut;
  zhao::exit_hard(fired ? 0 : 2);
#endif

  constexpr int kOwners = 24;
  std::vector<uint16_t> owners;
  for (int i = 0; i < kOwners; ++i)
    owners.push_back(s.admit(context_of(static_cast<unsigned>(i))));

  // Keep the output closed until every prepared owner has passed COMBINE and
  // returned its final. The output queue itself fills; younger completed owners
  // remain behind the monotone fetch cursor.
  for (int i = 0; i < 1000 &&
                  (static_cast<int>(s.combined.size()) < kOwners || !s.finals.empty());
       ++i)
    s.step();
  for (int i = 0; i < 32; ++i) s.step();

  int pre_hold_errors = 0;
  uint16_t held_owner = static_cast<uint16_t>(dut->out_owner_o);
  uint64_t held_ctx = dut->out_ctx_o;
  uint64_t held_result = dut->out_result_o;
  for (int i = 0; i < 16; ++i) {
    dut->eval();
    if (!dut->out_valid_o || static_cast<uint16_t>(dut->out_owner_o) != held_owner ||
        dut->out_ctx_o != held_ctx || dut->out_result_o != held_result)
      ++pre_hold_errors;
    s.step();
  }

  const bool preloaded = dut->out_valid_o && dut->ev_live_o == kOwners &&
                         dut->ev_emitted_o == 0 &&
                         static_cast<int>(s.combined.size()) == kOwners &&
                         s.finals.empty();

  s.out_ready = true;
  for (int i = 0; i < 2000 && static_cast<int>(s.emitted_owner.size()) < kOwners; ++i)
    s.step();

  int identity_errors = 0;
  for (int i = 0; i < kOwners && i < static_cast<int>(s.emitted_owner.size()); ++i) {
    if (s.emitted_owner[i] != owners[i]) ++identity_errors;
    if (s.emitted_ctx[i] != context_of(static_cast<unsigned>(i))) ++identity_errors;
    if (s.emitted_result[i] != final_of(owners[i])) ++identity_errors;
  }
  int bubbles = 0;
  for (size_t i = 1; i < s.emitted_cycle.size(); ++i)
    if (s.emitted_cycle[i] != s.emitted_cycle[i - 1] + 1) ++bubbles;

  const uint32_t response_errors =
      dut->ev_err_range_o + dut->ev_err_stale_o + dut->ev_err_unsol_o +
      dut->ev_err_dup_o + dut->ev_err_final_o + dut->ev_err_issue_o;
  std::printf(
      "  preloaded %d combined %zu live %u | emitted %zu | bubbles %d | "
      "identity %d | response errors %u\n",
      preloaded ? 1 : 0, s.combined.size(), dut->ev_live_o,
      s.emitted_owner.size(), bubbles, identity_errors, response_errors);

  zhao::check(preloaded,
              "more completed owners than OUTQD were prepared behind a closed consumer",
              1, preloaded ? 1 : 0);
  zhao::check(pre_hold_errors == 0,
              "the complete ordered output packet held throughout the closed interval",
              0, pre_hold_errors);
  zhao::check(s.emitted_owner.size() == kOwners,
              "every prepared owner drained", kOwners, s.emitted_owner.size());
  zhao::check(identity_errors == 0,
              "drain order, full context and final payload remain exact",
              0, identity_errors);
  zhao::check(response_errors == 0,
              "the bubble control does not exercise or redefine response refusal semantics",
              0, response_errors);

#ifdef ZHAO_NO_SAME_EDGE_RELOAD_CONTROL
  zhao::check(bubbles > 0,
              "POSITIVE CONTROL: removing same-edge reload creates a detected drain bubble",
              1, bubbles > 0 ? 1 : 0);
#else
  zhao::check(bubbles == 0,
              "after release every prepared output handshakes on every clock until drain",
              0, bubbles);
#endif

  const int rc = zhao::report_and_exit("texture_v3own_bubble_control");
  delete dut;
  zhao::exit_hard(rc);
}
