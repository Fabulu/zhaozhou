// texture_binding_resolver_v2_directed.cpp -- sealed two-bank binding contract.
#if defined(ZHAO_BIND_LATE_PAGE_MUTANT) || defined(ZHAO_BIND_OWNER_QUIET_MUTANT) ||    \
    defined(ZHAO_BIND_ACTIVE_WRITE_MUTANT) || defined(ZHAO_BIND_STALE_CRC_MUTANT) ||   \
    defined(ZHAO_BIND_WITNESS_CLASS_MUTANT) || defined(ZHAO_BIND_SAME_CYCLE_MUTANT) || \
    defined(ZHAO_BIND_WITHOUT_ISSUE_MUTANT)
#include "Vbindv2_mut.h"
using BindingDut = Vbindv2_mut;
#define ZHAO_BIND_MUTANT_BUILD 1
#else
#include "Vzhao_texture_binding_resolver_v2.h"
using BindingDut = Vzhao_texture_binding_resolver_v2;
#endif

#include <array>
#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"
// The seal is modelled once, in one header, because the shell composition now
// needs it too and a second fold of the same CRC is the duplication this
// repository keeps paying for.
#include "../harness/zhao_binding_seal.hpp"

namespace {

using zhao_binding_seal::Row;
using zhao_binding_seal::crc_byte;
using zhao_binding_seal::mode;
using zhao_binding_seal::pack_row;
using zhao_binding_seal::page_crc;

void tick(BindingDut* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

void drive_row(BindingDut* d, const Row& r) {
  const auto words = pack_row(r);
  for (unsigned i = 0; i < words.size(); ++i) d->cfg_row_i[i] = words[i];
}

void clear_row(BindingDut* d) {
  for (unsigned i = 0; i < 3; ++i) d->cfg_row_i[i] = 0;
}

void set_cfg(BindingDut* d, uint8_t op, uint8_t generation, uint8_t selector, const Row* row,
             uint32_t crc) {
  d->cfg_op_i = op;
  d->cfg_page_generation_i = generation;
  d->cfg_selector_i = selector;
  if (row)
    drive_row(d, *row);
  else
    clear_row(d);
  d->cfg_crc32_i = crc;
}

void send_immediate(BindingDut* d, uint8_t op, uint8_t generation, uint8_t selector, const Row* row,
                    uint32_t crc, uint8_t expected_status) {
  const uint32_t errors_before = d->cfg_errors_o;
  d->cfg_rsp_ready_i = 0;
  set_cfg(d, op, generation, selector, row, crc);
  d->cfg_valid_i = 1;
  // Every non-OK response is also a same-edge set-over-clear control.
  d->frame_fault_clear_i = expected_status != 0;
  d->eval();
  zhao::check(d->cfg_ready_o != 0, "configuration command was accepted", 1, d->cfg_ready_o);
  tick(d);
  d->cfg_valid_i = 0;
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->cfg_rsp_valid_o != 0, "each nondeferred command creates one held response", 1,
              d->cfg_rsp_valid_o);
  zhao::check(d->cfg_rsp_op_o == op, "response echoes accepted opcode", op, d->cfg_rsp_op_o);
  zhao::check(d->cfg_rsp_status_o == expected_status,
              "configuration status has the frozen typed value", expected_status,
              d->cfg_rsp_status_o);
  zhao::check(d->cfg_rsp_page_generation_o == generation,
              "response echoes the command page generation", generation,
              d->cfg_rsp_page_generation_o);
  if (expected_status != 0) {
    zhao::check(d->binding_fault_o && d->cfg_errors_o == errors_before + 1,
                "every non-OK response sets binding fault over same-edge clear and counts once", 1,
                (d->binding_fault_o && d->cfg_errors_o == errors_before + 1) ? 1 : 0);
    const uint8_t held_op = d->cfg_rsp_op_o;
    const uint8_t held_status = d->cfg_rsp_status_o;
    const uint8_t held_generation = d->cfg_rsp_page_generation_o;
    d->frame_fault_clear_i = 1;
    tick(d);
    d->frame_fault_clear_i = 0;
    d->eval();
    zhao::check(
        d->cfg_rsp_valid_o && d->cfg_rsp_op_o == held_op && d->cfg_rsp_status_o == held_status &&
            d->cfg_rsp_page_generation_o == held_generation &&
            d->cfg_errors_o == errors_before + 1 && !d->binding_fault_o,
        "clear during held config changes no response bit or reset-only counter", 1,
        (d->cfg_rsp_valid_o && d->cfg_rsp_op_o == held_op && d->cfg_rsp_status_o == held_status &&
         d->cfg_rsp_page_generation_o == held_generation && d->cfg_errors_o == errors_before + 1 &&
         !d->binding_fault_o)
            ? 1
            : 0);
  } else {
    zhao::check(d->cfg_errors_o == errors_before, "OK config response changes no error counter",
                errors_before, d->cfg_errors_o);
  }
  d->cfg_rsp_ready_i = 1;
  tick(d);
  d->cfg_rsp_ready_i = 0;
  d->eval();
}

void start_end(BindingDut* d, uint8_t generation, uint32_t crc) {
  d->cfg_rsp_ready_i = 0;
  set_cfg(d, 2, generation, 0, nullptr, crc);
  d->cfg_valid_i = 1;
  d->eval();
  zhao::check(d->cfg_ready_o != 0, "END was accepted from LOADING", 1, d->cfg_ready_o);
  tick(d);
  d->cfg_valid_i = 0;
  d->eval();
  zhao::check(d->cfg_rsp_valid_o == 0, "a legal END has no premature acknowledgement", 0,
              d->cfg_rsp_valid_o);
}

struct Job {
  uint16_t handle;
  uint8_t page_generation;
  bool overflow, force;
  uint8_t selector;
  int32_t u, v;
  uint8_t lod, cls, palette_slot, palette_generation;
};

uint16_t sample_handle(unsigned slot, unsigned sample, unsigned generation) {
  return static_cast<uint16_t>(((slot & 0x3Fu) << 10) | ((sample & 3u) << 8) |
                               (generation & 0xFFu));
}

void drive_job(BindingDut* d, const Job& j) {
  d->req_sample_handle_i = j.handle;
  d->req_page_generation_i = j.page_generation;
  d->req_selector_overflow_i = j.overflow;
  d->req_force_refuse_i = j.force;
  d->req_binding_selector_i = j.selector;
  d->req_u_i = j.u;
  d->req_v_i = j.v;
  d->req_lod_q4_4_i = j.lod;
  d->req_sample0_class_witness_i = j.cls;
  d->req_sample0_palette_slot_witness_i = j.palette_slot;
  d->req_sample0_palette_generation_witness_i = j.palette_generation;
}

void accept_job(BindingDut* d, const Job& j) {
  drive_job(d, j);
  d->req_valid_i = 1;
  d->eval();
  zhao::check(d->req_ready_o != 0, "logical sample job had reserved disposition capacity", 1,
              d->req_ready_o);
  zhao::check(d->iss_tmu_valid_o != 0 && d->iss_tmu_handle_o == j.handle,
              "logical acceptance atomically presents the exact owner issue", 1,
              (d->iss_tmu_valid_o && d->iss_tmu_handle_o == j.handle) ? 1 : 0);
  zhao::check(d->refuse_valid_o == 0 && d->plan_valid_o == 0,
              "a newly issued job cannot return in its issue cycle", 0,
              static_cast<uint64_t>(d->refuse_valid_o || d->plan_valid_o));
  tick(d);
  d->req_valid_i = 0;
  d->eval();
  zhao::check(d->refuse_valid_o == 0 && d->plan_valid_o == 0,
              "the synchronous read stage still cannot return before N+1", 0,
              static_cast<uint64_t>(d->refuse_valid_o || d->plan_valid_o));
  tick(d);
  d->eval();
}

void retire_plan(BindingDut* d) {
  d->plan_ready_i = 1;
  tick(d);
  d->plan_ready_i = 0;
  d->eval();
}
void retire_refusal(BindingDut* d) {
  d->refuse_ready_i = 1;
  tick(d);
  d->refuse_ready_i = 0;
  d->eval();
}

constexpr uint64_t kRefusal = (uint64_t{1} << 40) | (uint64_t{0xFF} << 24) | uint64_t{0xFF00FF};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* d = new BindingDut;
  d->clk = 0;
  d->rst_n = 0;
  d->frame_fault_clear_i = 0;
  d->cfg_valid_i = 0;
  d->cfg_rsp_ready_i = 0;
  d->data_quiet_i = 0;
#if defined(ZHAO_BIND_MUTANT_BUILD)
  d->owner_quiet_i = 0;
#endif
  d->req_valid_i = 0;
  d->plan_ready_i = 0;
  d->refuse_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  zhao::check(
      d->cfg_loader_idle_o != 0 && d->binding_crc_busy_o == 0 && d->binding_seal_pending_o == 0,
      "reset observation tuple is exactly {idle,busy,pending}={1,0,0}", 1,
      (d->cfg_loader_idle_o && !d->binding_crc_busy_o && !d->binding_seal_pending_o) ? 1 : 0);
  zhao::check(d->active_page_generation_o == 0, "reset intentionally has no sealed active page", 0,
              d->active_page_generation_o);

  // No page still terminates visibly, with issue preceding refusal.
  Job cold{sample_handle(4, 0, 0x34), 0, false, false, 9, 0x10000, -0x20000, 0x11, 0, 0, 0};
#if defined(ZHAO_BIND_SAME_CYCLE_MUTANT)
  drive_job(d, cold);
  d->req_valid_i = 1;
  d->eval();
  zhao::check(d->req_ready_o && d->iss_tmu_valid_o && d->refuse_valid_o,
              "SAME-CYCLE MUTANT FIRE: terminal refusal appeared on issue edge", 1,
              (d->req_ready_o && d->iss_tmu_valid_o && d->refuse_valid_o) ? 1 : 0);
  {
    const int rc = zhao::report_and_exit("binding_v2_same_cycle_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#elif defined(ZHAO_BIND_WITHOUT_ISSUE_MUTANT)
  drive_job(d, cold);
  d->req_valid_i = 1;
  d->eval();
  zhao::check(d->req_ready_o && !d->iss_tmu_valid_o,
              "WITHOUT-ISSUE MUTANT FIRE: accepted logical job omitted owner issue", 1,
              (d->req_ready_o && !d->iss_tmu_valid_o) ? 1 : 0);
  tick(d);
  d->req_valid_i = 0;
  tick(d);
  zhao::check(d->refuse_valid_o,
              "WITHOUT-ISSUE MUTANT FIRE: a terminal nevertheless appeared later", 1,
              d->refuse_valid_o);
  {
    const int rc = zhao::report_and_exit("binding_v2_without_issue_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#else
  accept_job(d, cold);
  zhao::check(d->refuse_valid_o != 0 && d->plan_valid_o == 0,
              "generation zero becomes a reserved local refusal", 1,
              (d->refuse_valid_o && !d->plan_valid_o) ? 1 : 0);
  zhao::check(d->refuse_sample_handle_o == cold.handle,
              "local refusal retains full sample identity", cold.handle, d->refuse_sample_handle_o);
  zhao::check(d->refuse_result_o == kRefusal,
              "local refusal is typed SOURCE_REFUSED/index0/alphaFF/magenta", kRefusal,
              d->refuse_result_o);
  retire_refusal(d);
#endif

  // Isolated configuration fault classes before a legal load begins.
#if !defined(ZHAO_BIND_STALE_CRC_MUTANT)
  Row harmless{0x00001000u, mode(1, false, 0, 0, 2, 2, 0, false), 0, 0, true};
  send_immediate(d, 1, 1, 1, &harmless, 0, 1);  // WRITE while IDLE: BAD_STATE
  send_immediate(d, 0, 0, 0, nullptr, 0, 2);    // zero generation: BAD_GENERATION
#endif

  std::array<Row, 256> page{};
  std::array<bool, 256> present{};
  page[5] = Row{0x00001000u, mode(0, false, 0, 1, 6, 5, 0, false), 2, 0x55, true};
  page[6] = Row{0x00004000u, mode(1, false, 2, 0, 4, 4, 0, false), 0, 0, true};
  page[7] = Row{0x00008000u, mode(4, true, 1, 2, 5, 5, 0, false), 0, 0, true};
  present[5] = present[6] = present[7] = true;

  send_immediate(d, 0, 1, 0, nullptr, 0, 0);   // BEGIN
  send_immediate(d, 1, 1, 7, &page[7], 0, 0);  // out-of-order writes
  send_immediate(d, 1, 1, 5, &page[5], 0, 0);
  send_immediate(d, 1, 1, 6, &page[6], 0, 0);
#if !defined(ZHAO_BIND_STALE_CRC_MUTANT)
  send_immediate(d, 1, 1, 5, &page[5], 0, 4);  // duplicate selector

  // Every independent canonical-row rejection class.
  std::array<Row, 10> bad_rows{};
  bad_rows[0] = Row{0x1001u, mode(1, false, 0, 0, 4, 4, 0, false), 0, 0, true};
  bad_rows[1] = Row{0x2000u, mode(5, false, 0, 0, 4, 4, 0, false), 0, 0, true};
  bad_rows[2] = Row{0x3000u, mode(1, false, 3, 0, 4, 4, 0, false), 0, 0, true};
  bad_rows[3] = Row{0x4000u, mode(1, false, 0, 0, 4, 4, 0, false) | (1u << 21), 0, 0, true};
  bad_rows[4] = Row{0x5000u, mode(1, false, 0, 0, 12, 4, 0, false), 0, 0, true};
  bad_rows[5] = Row{0x6000u, mode(0, true, 0, 0, 4, 4, 0, false), 1, 3, true};
  bad_rows[6] = Row{0x7000u, mode(1, false, 0, 0, 2, 1, 2, true), 0, 0, true};
  bad_rows[7] = Row{0x8000u, mode(1, false, 0, 0, 4, 4, 1, false), 0, 0, true};
  bad_rows[8] = Row{0x9000u, mode(1, false, 0, 0, 4, 4, 0, false), 1, 4, true};
  bad_rows[9] = Row{0xFFFFFFF0u, mode(1, false, 0, 0, 2, 2, 0, false), 0, 0, true};
  for (unsigned i = 0; i < bad_rows.size(); ++i)
    send_immediate(d, 1, 1, static_cast<uint8_t>(20 + i), &bad_rows[i], 0, 3);
#endif

  const uint32_t crc1 = page_crc(1, page, present);
  start_end(d, 1, crc1);
  unsigned crc_cycles = 0;
  while (!d->binding_seal_pending_o && crc_cycles < 4000) {
    tick(d);
    ++crc_cycles;
  }
  zhao::check(d->binding_crc_busy_o == 0 && d->binding_seal_pending_o != 0,
              "canonical sparse CRC reaches SEAL_PENDING", 1,
              (!d->binding_crc_busy_o && d->binding_seal_pending_o) ? 1 : 0);
  zhao::check(crc_cycles == 2561, "serialized CRC scans one preload plus 2560 bytes", 2561,
              crc_cycles);
  zhao::check(d->active_page_generation_o == 0 && d->cfg_rsp_valid_o == 0,
              "CRC success neither activates nor acknowledges END", 0,
              static_cast<uint64_t>(d->active_page_generation_o || d->cfg_rsp_valid_o));
  zhao::check(d->admission_enable_o == 0, "successful seal withholds only new fragment admission",
              0, d->admission_enable_o);
  tick(d);
  zhao::check(d->active_page_generation_o == 0,
              "no quiet source activated the page while both witnesses were low", 0,
              d->active_page_generation_o);
  d->data_quiet_i = 1;
#if defined(ZHAO_BIND_OWNER_QUIET_MUTANT)
  // Establish page A normally before exercising the mutant with held work.
  d->owner_quiet_i = 1;
#endif
  tick(d);
  d->data_quiet_i = 0;
#if defined(ZHAO_BIND_OWNER_QUIET_MUTANT)
  d->owner_quiet_i = 0;
#endif
  d->eval();
  zhao::check(
      d->active_page_generation_o == 1 && d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0,
      "the first page activates and acknowledges on its selected quiet edge", 1,
      (d->active_page_generation_o == 1 && d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0) ? 1 : 0);
  d->cfg_rsp_ready_i = 1;
  tick(d);
  d->cfg_rsp_ready_i = 0;

#if defined(ZHAO_BIND_OWNER_QUIET_MUTANT)
  {
    Job blocker{sample_handle(21, 0, 0xB1), 1, false, false, 6, 0x101000, 0x202000, 0x10, 1, 0, 0};
    accept_job(d, blocker);
    zhao::check(d->plan_valid_o && !d->data_idle_o,
                "early-activation control first holds blocker C in disposition", 1,
                (d->plan_valid_o && !d->data_idle_o) ? 1 : 0);

    Job held_a{
        sample_handle(22, 0, 0xB2), 1, false, false, 5, 0x303000, 0x404000, 0x20, 0, 2, 0x55};
    drive_job(d, held_a);
    d->req_valid_i = 1;
    d->eval();
    zhao::check(d->req_ready_o && d->iss_tmu_valid_o, "A enters read hold behind stalled blocker C",
                1, (d->req_ready_o && d->iss_tmu_valid_o) ? 1 : 0);
    tick(d);
    d->req_valid_i = 0;

    std::array<Row, 256> page_b{};
    std::array<bool, 256> present_b{};
    page_b[5] = Row{0x00B00000u, mode(0, false, 0, 0, 4, 4, 0, false), 1, 0x77, true};
    present_b[5] = true;
    send_immediate(d, 0, 2, 0, nullptr, 0, 0);
    send_immediate(d, 1, 2, 5, &page_b[5], 0, 0);
    start_end(d, 2, page_crc(2, page_b, present_b));
    unsigned wait = 0;
    while (!d->binding_seal_pending_o && wait < 4000) {
      tick(d);
      ++wait;
    }
    zhao::check(d->binding_seal_pending_o && !d->data_idle_o,
                "page B seal waits while C disposition and A read hold remain live", 1,
                (d->binding_seal_pending_o && !d->data_idle_o) ? 1 : 0);
    const uint32_t page_before = d->page_generation_mismatch_o;
    d->owner_quiet_i = 1;
    tick(d);
    d->owner_quiet_i = 0;
    d->eval();
    zhao::check(d->active_page_generation_o == 2 && !d->data_quiet_i && !d->data_idle_o,
                "EARLY-ACTIVATION MUTANT FIRE: B activated while A remained held", 1,
                (d->active_page_generation_o == 2 && !d->data_quiet_i && !d->data_idle_o) ? 1 : 0);
    d->cfg_rsp_ready_i = 1;
    tick(d);
    d->cfg_rsp_ready_i = 0;
    d->plan_ready_i = 1;
    tick(d);
    d->plan_ready_i = 0;
    d->eval();
    zhao::check(
        d->refuse_valid_o && d->refuse_sample_handle_o == held_a.handle &&
            d->page_generation_mismatch_o == page_before + 1 && d->binding_fault_o,
        "EARLY-ACTIVATION MUTANT FIRE: terminal A-vs-live-B comparison fired once and refused A", 1,
        (d->refuse_valid_o && d->refuse_sample_handle_o == held_a.handle &&
         d->page_generation_mismatch_o == page_before + 1 && d->binding_fault_o)
            ? 1
            : 0);
    tick(d);
    zhao::check(d->page_generation_mismatch_o == page_before + 1,
                "held early-activation refusal cannot recount terminal mismatch", page_before + 1,
                d->page_generation_mismatch_o);
    const int rc = zhao::report_and_exit("binding_v2_early_activation_held_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#elif defined(ZHAO_BIND_LATE_PAGE_MUTANT)
  {
    Job late{
        sample_handle(18, 0, 0xA1), 2, false, false, 5, 0x11110000, -0x22220000, 0x44, 0, 2, 0x55};
    accept_job(d, late);
    zhao::check(
        d->plan_valid_o && !d->refuse_valid_o,
        "LATE-PAGE MUTANT FIRE: stale job generation was overwritten by current active page", 1,
        (d->plan_valid_o && !d->refuse_valid_o) ? 1 : 0);
    const int rc = zhao::report_and_exit("binding_v2_late_page_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#elif defined(ZHAO_BIND_ACTIVE_WRITE_MUTANT)
  {
    Row leaked{0x00ABC000u, mode(0, false, 0, 0, 3, 3, 0, false), 1, 0x66, true};
    send_immediate(d, 0, 2, 0, nullptr, 0, 0);
    send_immediate(d, 1, 2, 5, &leaked, 0, 0);
    Job probe{sample_handle(19, 0, 0xA2), 1, false, false, 5, 0x10000, 0x20000, 0x10, 0, 2, 0x55};
    accept_job(d, probe);
    zhao::check(
        d->plan_valid_o && d->plan_base_o == leaked.base && d->active_page_generation_o == 1,
        "ACTIVE-WRITE MUTANT FIRE: staging WRITE leaked into still-active page output", 1,
        (d->plan_valid_o && d->plan_base_o == leaked.base && d->active_page_generation_o == 1) ? 1
                                                                                               : 0);
    const int rc = zhao::report_and_exit("binding_v2_active_write_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#elif defined(ZHAO_BIND_STALE_CRC_MUTANT)
  {
    Row stale_payload{0x00123000u, mode(1, false, 0, 0, 3, 3, 0, false), 0, 0, true};
    send_immediate(d, 0, 2, 0, nullptr, 0, 0);
    send_immediate(d, 1, 2, 99, &stale_payload, 0, 0);
    send_immediate(d, 3, 2, 0, nullptr, 0, 0);
    send_immediate(d, 0, 3, 0, nullptr, 0, 0);
    std::array<Row, 256> empty_rows{};
    std::array<bool, 256> empty_present{};
    start_end(d, 3, page_crc(3, empty_rows, empty_present));
    unsigned wait = 0;
    while (!d->cfg_rsp_valid_o && wait < 4000) {
      tick(d);
      ++wait;
    }
    zhao::check(d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5,
                "STALE-CRC MUTANT FIRE: an invalid selector's stale payload changed canonical CRC",
                1, (d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5) ? 1 : 0);
    const int rc = zhao::report_and_exit("binding_v2_stale_crc_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#elif defined(ZHAO_BIND_WITNESS_CLASS_MUTANT)
  {
    Job routed{sample_handle(20, 2, 0xA3), 1, false, false, 7, 0x12000, 0x34000, 0x20, 3, 3, 0xEE};
    accept_job(d, routed);
    zhao::check(
        d->plan_valid_o && (d->plan_route_token_o >> 16) == 3,
        "WITNESS-CLASS MUTANT FIRE: fragment witness replaced row-derived BIL routing class", 1,
        (d->plan_valid_o && (d->plan_route_token_o >> 16) == 3) ? 1 : 0);
    const int rc = zhao::report_and_exit("binding_v2_witness_class_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#endif

  // A held sample-0 CLUT lookup, then B accepted behind it. B is sample 2 and
  // deliberately carries nonsense witnesses: only sample 0 compares them.
  Job a{sample_handle(10, 0, 0x31), 1, false, false, 5, 0x11223344, -0x01020304, 0xA5, 0, 2, 0x55};
  accept_job(d, a);
  zhao::check(d->plan_valid_o != 0 && d->refuse_valid_o == 0,
              "valid resolved row reaches planner, not refusal", 1,
              (d->plan_valid_o && !d->refuse_valid_o) ? 1 : 0);
  zhao::check(d->plan_route_token_o == ((0u << 16) | a.handle) && d->plan_base_o == page[5].base &&
                  d->plan_mode_o == page[5].mode && d->plan_palette_slot_o == 2 &&
                  d->plan_palette_generation_o == 0x55 &&
                  static_cast<int32_t>(d->plan_u_o) == a.u &&
                  static_cast<int32_t>(d->plan_v_o) == a.v && d->plan_lod_q4_4_o == a.lod,
              "planner record is one atomic owner/row/palette/U/V/LOD join", 1, 1);
  const uint32_t held_token = d->plan_route_token_o;
  const uint32_t held_base = d->plan_base_o;
  const uint32_t held_mode = d->plan_mode_o;
  const uint8_t held_palette_slot = d->plan_palette_slot_o;
  const uint8_t held_palette_generation = d->plan_palette_generation_o;
  const int32_t held_u = static_cast<int32_t>(d->plan_u_o);
  const int32_t held_v = static_cast<int32_t>(d->plan_v_o);
  const uint8_t held_lod = d->plan_lod_q4_4_o;
  const uint32_t held_sj = d->sample_jobs_accepted_o;
  const uint32_t held_pj = d->planner_jobs_accepted_o;
  const uint32_t held_lr = d->local_refused_o;
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(
      d->plan_valid_o && d->plan_route_token_o == held_token && d->plan_base_o == held_base &&
          d->plan_mode_o == held_mode && d->plan_palette_slot_o == held_palette_slot &&
          d->plan_palette_generation_o == held_palette_generation &&
          static_cast<int32_t>(d->plan_u_o) == held_u &&
          static_cast<int32_t>(d->plan_v_o) == held_v && d->plan_lod_q4_4_o == held_lod &&
          d->sample_jobs_accepted_o == held_sj && d->planner_jobs_accepted_o == held_pj &&
          d->local_refused_o == held_lr,
      "clear during held planner work preserves every record bit and all work counters", 1, 1);

  Job b{sample_handle(11, 2, 0xC7), 1, false, false, 7, -0x123456, 0x765432, 0x3C, 3, 3, 0xEE};
  drive_job(d, b);
  d->req_valid_i = 1;
  d->eval();
  zhao::check(d->req_ready_o && d->iss_tmu_valid_o,
              "B can occupy the reserved read stage while planner A is stalled", 1,
              (d->req_ready_o && d->iss_tmu_valid_o) ? 1 : 0);
  tick(d);
  d->req_valid_i = 0;
  d->eval();
  zhao::check(
      d->plan_valid_o && d->plan_route_token_o == held_token && d->plan_base_o == held_base &&
          d->plan_mode_o == held_mode && d->plan_palette_slot_o == held_palette_slot &&
          d->plan_palette_generation_o == held_palette_generation &&
          static_cast<int32_t>(d->plan_u_o) == held_u &&
          static_cast<int32_t>(d->plan_v_o) == held_v && d->plan_lod_q4_4_o == held_lod,
      "held planner A cannot borrow any owner/row/palette/U/V/LOD bit from B", 1,
      (d->plan_valid_o && d->plan_route_token_o == held_token && d->plan_base_o == held_base &&
       d->plan_mode_o == held_mode && d->plan_palette_slot_o == held_palette_slot &&
       d->plan_palette_generation_o == held_palette_generation &&
       static_cast<int32_t>(d->plan_u_o) == held_u && static_cast<int32_t>(d->plan_v_o) == held_v &&
       d->plan_lod_q4_4_o == held_lod)
          ? 1
          : 0);
  d->plan_ready_i = 1;
  tick(d);  // retire A and move already-read B atomically into disposition
  d->plan_ready_i = 0;
  d->eval();
  zhao::check(d->plan_valid_o && d->plan_route_token_o == ((2u << 16) | b.handle) &&
                  d->plan_base_o == page[7].base,
              "sample 2 derives BIL class from its row and ignores fragment witnesses", 1,
              (d->plan_valid_o && d->plan_route_token_o == ((2u << 16) | b.handle) &&
               d->plan_base_o == page[7].base)
                  ? 1
                  : 0);
  retire_plan(d);

  auto expect_refusal = [&](const Job& job, const char* label) {
    accept_job(d, job);
    zhao::check(d->refuse_valid_o && !d->plan_valid_o, label, 1,
                (d->refuse_valid_o && !d->plan_valid_o) ? 1 : 0);
    zhao::check(d->refuse_result_o == kRefusal,
                "all local refusal classes share the typed terminal value", kRefusal,
                d->refuse_result_o);
    retire_refusal(d);
  };

  Job overflow = a;
  overflow.handle = sample_handle(12, 0, 0x52);
  overflow.overflow = true;
  overflow.selector = 0;
  expect_refusal(overflow, "selector carry refuses instead of wrapping to selector zero");
  Job stale = a;
  stale.handle = sample_handle(13, 0, 0x63);
  stale.page_generation = 2;
  expect_refusal(stale, "captured page generation mismatch refuses locally");
  Job invalid = a;
  invalid.handle = sample_handle(14, 0, 0x74);
  invalid.selector = 99;
  expect_refusal(invalid, "an unwritten selector refuses without planner access");
  Job witness = a;
  witness.handle = sample_handle(15, 0, 0x85);
  witness.cls = 2;
  expect_refusal(witness, "sample-0 class/palette witness mismatch refuses");
  Job force = a;
  force.handle = sample_handle(16, 0, 0x96);
  force.force = true;
  expect_refusal(force, "admission-frozen malformed material suppresses lookup");

  // Bad CRC update: old active page remains usable throughout CRC and afterward.
  std::array<Row, 256> page2{};
  std::array<bool, 256> present2{};
  page2[9] = Row{0x00100000u, mode(1, false, 0, 0, 3, 3, 0, false), 0, 0, true};
  present2[9] = true;
  send_immediate(d, 0, 2, 0, nullptr, 0, 0);
  send_immediate(d, 1, 2, 9, &page2[9], 0, 0);
  const uint32_t bad_crc_errors_before = d->cfg_errors_o;
  start_end(d, 2, page_crc(2, page2, present2) ^ 1u);
  tick(d);  // enter scan and preload first row
  Job during_crc = a;
  during_crc.handle = sample_handle(17, 0, 0x97);
  accept_job(d, during_crc);
  zhao::check(d->plan_valid_o && d->plan_base_o == page[5].base,
              "old active page continues serving accepted work during staging CRC", 1,
              (d->plan_valid_o && d->plan_base_o == page[5].base) ? 1 : 0);
  retire_plan(d);
  unsigned bad_crc_cycles = 0;
  d->frame_fault_clear_i = 1;
  while (!d->cfg_rsp_valid_o && bad_crc_cycles < 4000) {
    tick(d);
    ++bad_crc_cycles;
  }
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5, "bad canonical CRC returns BAD_CRC",
              1, (d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5) ? 1 : 0);
  // Four scan clocks elapsed before this counter started: row-0 preload, two
  // accepted data-path stages, and planner retirement. The response must still
  // land on selector 255 byte 9, not merely somewhere inside a loose timeout.
  zhao::check(bad_crc_cycles == 2557, "BAD_CRC response lands on final serialized byte", 2557,
              bad_crc_cycles);
  zhao::check(d->binding_fault_o && d->cfg_errors_o == bad_crc_errors_before + 1,
              "BAD_CRC sets binding fault over same-edge clear and counts once", 1,
              (d->binding_fault_o && d->cfg_errors_o == bad_crc_errors_before + 1) ? 1 : 0);
  zhao::check(d->active_page_generation_o == 1,
              "bad CRC leaves active bank and generation untouched", 1,
              d->active_page_generation_o);
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5 &&
                  d->cfg_errors_o == bad_crc_errors_before + 1 && !d->binding_fault_o,
              "clear during held BAD_CRC preserves response and counter only", 1,
              (d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 5 &&
               d->cfg_errors_o == bad_crc_errors_before + 1 && !d->binding_fault_o)
                  ? 1
                  : 0);
  d->cfg_rsp_ready_i = 1;
  tick(d);
  d->cfg_rsp_ready_i = 0;

  // ABORT discards only staging validity and cannot touch active page.
  send_immediate(d, 0, 3, 0, nullptr, 0, 0);
  send_immediate(d, 1, 3, 9, &page2[9], 0, 0);
  send_immediate(d, 3, 3, 0, nullptr, 0, 0);
  zhao::check(d->active_page_generation_o == 1, "ABORT never changes active page identity", 1,
              d->active_page_generation_o);

  // Data-plane same-edge fault priority and held-refusal clear isolation.
  Job final_force = a;
  final_force.handle = sample_handle(23, 0, 0xC1);
  final_force.force = true;
  const uint32_t force_before = d->forced_refused_o;
  const uint32_t local_before = d->local_refused_o;
  drive_job(d, final_force);
  d->req_valid_i = 1;
  d->eval();
  tick(d);
  d->req_valid_i = 0;
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->refuse_valid_o && d->binding_fault_o && d->forced_refused_o == force_before + 1 &&
                  d->local_refused_o == local_before,
              "terminal refusal creation sets binding fault over same-edge clear exactly once", 1,
              (d->refuse_valid_o && d->binding_fault_o && d->forced_refused_o == force_before + 1 &&
               d->local_refused_o == local_before)
                  ? 1
                  : 0);
  const uint16_t held_refuse_handle = d->refuse_sample_handle_o;
  const uint64_t held_refuse_result = d->refuse_result_o;
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(
      d->refuse_valid_o && d->refuse_sample_handle_o == held_refuse_handle &&
          d->refuse_result_o == held_refuse_result && d->forced_refused_o == force_before + 1 &&
          d->local_refused_o == local_before && !d->binding_fault_o,
      "clear during held refusal preserves payload and every counter", 1,
      (d->refuse_valid_o && d->refuse_sample_handle_o == held_refuse_handle &&
       d->refuse_result_o == held_refuse_result && d->forced_refused_o == force_before + 1 &&
       d->local_refused_o == local_before && !d->binding_fault_o)
          ? 1
          : 0);
  retire_refusal(d);

  zhao::check(d->sample_jobs_accepted_o == 10,
              "every logical sample job is counted exactly at issue", 10,
              d->sample_jobs_accepted_o);
  zhao::check(d->planner_jobs_accepted_o == 3, "only three valid jobs reached planner", 3,
              d->planner_jobs_accepted_o);
  zhao::check(d->local_refused_o == 7, "cold plus six typed failures produce seven local terminals",
              7, d->local_refused_o);
  zhao::check(d->selector_overflow_count_o == 1 && d->page_generation_mismatch_o == 2 &&
                  d->invalid_row_o == 1 && d->witness_mismatch_o == 1 && d->forced_refused_o == 2,
              "independent refusal instruments classify overflow/page/row/witness/force", 1,
              (d->selector_overflow_count_o == 1 && d->page_generation_mismatch_o == 2 &&
               d->invalid_row_o == 1 && d->witness_mismatch_o == 1 && d->forced_refused_o == 2)
                  ? 1
                  : 0);
  zhao::check(d->binding_fault_o == 0,
              "the held-refusal clear remains effective after terminal acceptance", 0,
              d->binding_fault_o);
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->binding_fault_o == 0, "sticky frame state clears independently of all counters", 0,
              d->binding_fault_o);
  zhao::check(d->data_idle_o != 0, "lookup/read/disposition/planner/refusal state fully drained", 1,
              d->data_idle_o);

  std::printf("  CRC cycles good=%u bad=%u | cfg_errors=%u | SJ=%u plan=%u refuse=%u\n", crc_cycles,
              bad_crc_cycles, d->cfg_errors_o, d->sample_jobs_accepted_o,
              d->planner_jobs_accepted_o, d->local_refused_o);

  const int rc = zhao::report_and_exit("texture_binding_resolver_v2_directed");
  delete d;
  zhao::exit_hard(rc);
}
