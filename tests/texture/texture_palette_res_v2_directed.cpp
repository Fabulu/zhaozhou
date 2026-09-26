// texture_palette_res_v2_directed.cpp — Packet-B palette tuple/alignment gate.
//
// Exercises the complete 66-bit {token18,status8,index8,alpha8,RGB24} record,
// palette generation verdict, load protocol, same-edge reload invalidation,
// elastic output hold, simultaneous pipeline reload, cfg_idle_o and idle_o.
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "Vzhao_texture_palette_res_v2.h"
#include "zhao_sim.hpp"

namespace {

struct Tuple66 {
  uint32_t token = 0;
  uint8_t status = 0;
  uint8_t index = 0;
  uint8_t alpha = 0;
  uint32_t rgb = 0;
};

void drive_tuple(Vzhao_texture_palette_res_v2& top, const Tuple66& t) {
  top.req_tuple_i[0] = (t.rgb & 0x00FFFFFFu) | (static_cast<uint32_t>(t.alpha) << 24);
  top.req_tuple_i[1] = static_cast<uint32_t>(t.index) | (static_cast<uint32_t>(t.status) << 8) |
                       ((t.token & 0xFFFFu) << 16);
  top.req_tuple_i[2] = (t.token >> 16) & 0x3u;
}

Tuple66 read_tuple(const Vzhao_texture_palette_res_v2& top) {
  Tuple66 t;
  t.rgb = top.rsp_tuple_o[0] & 0x00FFFFFFu;
  t.alpha = static_cast<uint8_t>(top.rsp_tuple_o[0] >> 24);
  t.index = static_cast<uint8_t>(top.rsp_tuple_o[1]);
  t.status = static_cast<uint8_t>(top.rsp_tuple_o[1] >> 8);
  t.token = ((top.rsp_tuple_o[1] >> 16) & 0xFFFFu) | ((top.rsp_tuple_o[2] & 0x3u) << 16);
  return t;
}

bool same(const Tuple66& a, const Tuple66& b) {
  return a.token == b.token && a.status == b.status && a.index == b.index && a.alpha == b.alpha &&
         a.rgb == b.rgb;
}

uint32_t expand565(uint16_t value) {
  const uint32_t r5 = (value >> 11) & 31u;
  const uint32_t g6 = (value >> 5) & 63u;
  const uint32_t b5 = value & 31u;
  const uint32_t r8 = (r5 << 3) | (r5 >> 2);
  const uint32_t g8 = (g6 << 2) | (g6 >> 4);
  const uint32_t b8 = (b5 << 3) | (b5 >> 2);
  return (r8 << 16) | (g8 << 8) | b8;
}

void reset(Vzhao_texture_palette_res_v2& top) {
  top.ld_valid_i = 0;
  top.req_valid_i = 0;
  top.rsp_ready_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

void load_op(Vzhao_texture_palette_res_v2& top, int op, int slot, int generation, int index,
             uint16_t value, bool crc_ok) {
  top.ld_valid_i = 1;
  top.ld_op_i = op;
  top.ld_slot_i = slot;
  top.ld_gen_i = generation;
  top.ld_idx_i = index;
  top.ld_rgb565_i = value;
  top.ld_crc_ok_i = crc_ok ? 1 : 0;
  top.eval();
  zhao::check(top.ld_ready_o == 1, "palette-v2 programming port accepts", 1, top.ld_ready_o);
  zhao::tick(top);
  top.ld_valid_i = 0;
}

void load_slot(Vzhao_texture_palette_res_v2& top, int slot, int generation, uint16_t base) {
  load_op(top, 0, slot, generation, 0, 0, true);
  top.eval();
  zhao::check(top.cfg_idle_o == 0, "palette-v2 cfg idle is low during open load", 0,
              top.cfg_idle_o);
  for (int index = 0; index < 256; ++index)
    load_op(top, 1, 0, 0, index, static_cast<uint16_t>(base + index), true);
  load_op(top, 2, slot, generation, 0, 0, true);
  top.eval();
  zhao::check(top.cfg_idle_o == 1, "palette-v2 cfg idle returns after END", 1, top.cfg_idle_o);
}

void accept_request(Vzhao_texture_palette_res_v2& top, const Tuple66& request, int slot,
                    int generation) {
  top.req_valid_i = 1;
  top.req_slot_i = slot;
  top.req_gen_i = generation;
  drive_tuple(top, request);
  for (int guard = 0; guard < 20; ++guard) {
    top.eval();
    if (top.req_ready_o) {
      zhao::tick(top);
      top.req_valid_i = 0;
      return;
    }
    zhao::tick(top);
  }
  zhao::check(false, "palette-v2 request accepted within bounded wait", 1, 0);
  top.req_valid_i = 0;
}

Tuple66 take_response(Vzhao_texture_palette_res_v2& top) {
  top.rsp_ready_i = 1;
  for (int guard = 0; guard < 20; ++guard) {
    top.eval();
    if (top.rsp_valid_o) {
      const Tuple66 result = read_tuple(top);
      zhao::tick(top);
      top.rsp_ready_i = 0;
      return result;
    }
    zhao::tick(top);
  }
  zhao::check(false, "palette-v2 response arrives within bounded wait", 1, 0);
  return Tuple66{};
}

void test_cold_and_fresh(Vzhao_texture_palette_res_v2& top) {
  reset(top);
  top.eval();
  zhao::check(top.idle_o == 1, "palette-v2 data path idle after reset", 1, top.idle_o);
  zhao::check(top.cfg_idle_o == 1, "palette-v2 config path idle after reset", 1, top.cfg_idle_o);

  const Tuple66 cold{0x3ABCDu, 0x00, 0xFF, 0x37, 0x123456};
  accept_request(top, cold, 2, 0);
  const Tuple66 cold_rsp = take_response(top);
  const Tuple66 cold_want{cold.token, 0x01, cold.index, cold.alpha, 0xFF00FF};
  zhao::check(same(cold_rsp, cold_want),
              "cold palette returns aligned token/index/alpha with typed refusal", 1,
              same(cold_rsp, cold_want) ? 1 : 0);

  load_slot(top, 2, 7, 0x4000);
  const Tuple66 fresh{0x20001u, 0x00, 0xA5, 0xE3, 0x654321};
  accept_request(top, fresh, 2, 7);
  const Tuple66 fresh_rsp = take_response(top);
  const Tuple66 fresh_want{fresh.token, 0x00, fresh.index, fresh.alpha,
                           expand565(static_cast<uint16_t>(0x4000 + fresh.index))};
  zhao::check(same(fresh_rsp, fresh_want),
              "fresh palette replaces only RGB and preserves all 42 carried bits", 1,
              same(fresh_rsp, fresh_want) ? 1 : 0);

  // Reserved status bits are carried as eight bits, never narrowed to refused1.
  const Tuple66 reserved{0x30002u, 0x80, 0x42, 0x19, 0x010203};
  accept_request(top, reserved, 2, 7);
  const Tuple66 reserved_rsp = take_response(top);
  zhao::check(reserved_rsp.token == reserved.token && reserved_rsp.status == 0x80 &&
                  reserved_rsp.index == reserved.index && reserved_rsp.alpha == reserved.alpha &&
                  reserved_rsp.rgb == 0xFF00FF,
              "palette-v2 carries all status bits and does not publish partial success", 1,
              reserved_rsp.status == 0x80 ? 1 : 0);
  zhao::check(top.lookups_o == 3 && top.stale_o == 0 && top.cold_o == 1 && top.loads_ok_o == 1,
              "cold/fresh lane has exact lookup/stale/cold/load counts", 1,
              (top.lookups_o == 3 && top.cold_o == 1) ? 1 : 0);
  zhao::check(top.err_write_outside_o == 0 && top.err_same_gen_o == 0 &&
                  top.err_incomplete_o == 0 && top.err_crc_o == 0,
              "cold/fresh lane leaves every programming error counter zero", 0,
              top.err_write_outside_o + top.err_same_gen_o + top.err_incomplete_o + top.err_crc_o);
}

void test_generation_and_hold(Vzhao_texture_palette_res_v2& top) {
  reset(top);
  load_slot(top, 0, 1, 0x1800);

  // A lookup accepted on the exact BEGIN edge is independently invalidated.
  const Tuple66 old{0x355AAu, 0x00, 9, 0x7D, 0xABCDEF};
  top.req_valid_i = 1;
  top.req_slot_i = 0;
  top.req_gen_i = 1;
  drive_tuple(top, old);
  top.ld_valid_i = 1;
  top.ld_op_i = 0;
  top.ld_slot_i = 0;
  top.ld_gen_i = 2;
  top.ld_idx_i = 0;
  top.ld_rgb565_i = 0;
  top.ld_crc_ok_i = 1;
  top.eval();
  zhao::check(top.req_ready_o == 1, "same-edge reload lookup is accepted", 1, top.req_ready_o);
  zhao::tick(top);
  top.req_valid_i = 0;
  top.ld_valid_i = 0;

  const Tuple66 stale_rsp = take_response(top);
  zhao::check(stale_rsp.token == old.token && stale_rsp.status == 1 &&
                  stale_rsp.index == old.index && stale_rsp.alpha == old.alpha &&
                  stale_rsp.rgb == 0xFF00FF,
              "same-edge BEGIN makes old generation refused without misaligning tuple", 1,
              stale_rsp.status == 1 ? 1 : 0);

  // Complete generation 2, then hold a distinguishable full-width tuple for 24
  // clocks.  The upper token bits are deliberately 3.
  for (int index = 0; index < 256; ++index)
    load_op(top, 1, 0, 0, index, static_cast<uint16_t>(0x9000 + index), true);
  load_op(top, 2, 0, 2, 0, 0, true);

  const Tuple66 held{0x3F00Du, 0x00, 0xFE, 0xA6, 0x0BADF0};
  accept_request(top, held, 0, 2);
  top.rsp_ready_i = 0;
  Tuple66 first;
  bool saw = false;
  int hold_bad = 0;
  for (int cycle = 0; cycle < 40; ++cycle) {
    top.eval();
    if (top.rsp_valid_o) {
      const Tuple66 now = read_tuple(top);
      if (!saw) {
        first = now;
        saw = true;
      } else if (!same(first, now)) {
        ++hold_bad;
      }
    }
    zhao::tick(top);
  }
  zhao::check(saw, "palette-v2 produced the response before hold check", 1, saw ? 1 : 0);
  zhao::check(hold_bad == 0, "palette-v2 holds all 66 response bits under stall", 0, hold_bad);
  top.eval();
  zhao::check(top.idle_o == 0, "held palette response keeps data idle low", 0, top.idle_o);
  top.rsp_ready_i = 1;
  top.eval();
  const Tuple66 final_held = read_tuple(top);
  zhao::tick(top);
  top.rsp_ready_i = 0;
  zhao::check(final_held.token == held.token && final_held.index == held.index &&
                  final_held.alpha == held.alpha && final_held.status == 0 &&
                  final_held.rgb == expand565(static_cast<uint16_t>(0x9000 + held.index)),
              "held palette response remains the original accepted tuple", 1,
              final_held.token == held.token ? 1 : 0);
  top.eval();
  zhao::check(top.idle_o == 1, "palette-v2 idle only after held response retires", 1, top.idle_o);
  zhao::check(top.lookups_o == 2 && top.stale_o == 1 && top.cold_o == 0 && top.loads_ok_o == 2,
              "reload lane has exact lookup/stale/cold/load counts", 1,
              (top.lookups_o == 2 && top.stale_o == 1 && top.loads_ok_o == 2) ? 1 : 0);
  zhao::check(top.err_write_outside_o == 0 && top.err_same_gen_o == 0 &&
                  top.err_incomplete_o == 0 && top.err_crc_o == 0,
              "reload lane leaves programming error counters zero", 0,
              top.err_write_outside_o + top.err_same_gen_o + top.err_incomplete_o + top.err_crc_o);
}

void test_programming_error_controls(Vzhao_texture_palette_res_v2& top) {
  reset(top);

  // Each misuse is independent and expected exactly once.  Zero-valued error
  // counters are not cited until every detector has been made to fire here.
  load_op(top, 1, 0, 0, 7, 0x1234, true);  // WRITE outside a load

  load_op(top, 0, 0, 1, 0, 0, true);
  load_op(top, 1, 0, 0, 0, 0x2000, true);
  load_op(top, 2, 0, 1, 0, 0, true);  // incomplete

  load_op(top, 0, 0, 2, 0, 0, true);
  load_op(top, 2, 0, 2, 0, 0, false);  // CRC has priority over incomplete

  load_slot(top, 0, 3, 0x3000);  // one successful load witness

  // THE SAME-GENERATION CONTROL, RE-AUTHORED 2026-09-26 (gz/i13close), and the
  // reason is the thing to keep.  It used to read
  //
  //     load_op(top, 0, 0, 0, 0, 0, true);  // BEGIN reusing reset generation
  //
  // on a COLD slot 0 -- it fired `err_same_gen_o` because `generation_q` RESETS
  // to zero, not because any binding was at risk.  That is a test asserting the
  // bug: it pinned the accident that made generation ZERO unreachable in one
  // pass, and it would have gone red on the repair while reading like a
  // regression in the block.  The fault the counter names is replacing a
  // RESIDENT binding with content under the generation that binding already
  // advertises, so that is what is presented here: slot 0 is resident at
  // generation 3 from the load above, and this BEGIN re-uses 3.  It is REFUSED,
  // so the resident binding is untouched -- which the next check READS.
  load_op(top, 0, 0, 3, 0, 0, true);  // BEGIN over a RESIDENT slot's own generation

  zhao::check(top.err_write_outside_o == 1, "WRITE-outside detector fires exactly once", 1,
              top.err_write_outside_o);
  zhao::check(top.err_same_gen_o == 1, "same-generation BEGIN detector fires exactly once", 1,
              top.err_same_gen_o);
  {
    // A REFUSED BEGIN CHANGES NOTHING.  The refusal above must not have
    // invalidated the binding it refused to replace -- which is exactly what a
    // `begin_same_slot_c` spelled differently from the FSM's acceptance term
    // would do, silently, on the same edge.
    const Tuple66 survivor{0x11111u, 0x00, 0x2A, 0x77, 0x000000};
    accept_request(top, survivor, 0, 3);
    const Tuple66 got = take_response(top);
    const Tuple66 want{survivor.token, 0x00, survivor.index, survivor.alpha,
                       expand565(static_cast<uint16_t>(0x3000 + survivor.index))};
    zhao::check(same(got, want),
                "a refused same-generation BEGIN leaves the resident binding intact", 1,
                same(got, want) ? 1 : 0);
  }
  zhao::check(top.err_incomplete_o == 1, "incomplete END detector fires exactly once", 1,
              top.err_incomplete_o);
  zhao::check(top.err_crc_o == 1, "CRC-failed END detector fires exactly once", 1, top.err_crc_o);
  zhao::check(top.loads_ok_o == 1, "successful-load counter is independent and exact", 1,
              top.loads_ok_o);
  zhao::check(top.lookups_o == 1 && top.stale_o == 0 && top.cold_o == 0,
              "programming controls create no phantom stale or cold verdict", 0,
              top.stale_o + top.cold_o);
  zhao::check(top.cfg_idle_o == 1 && top.idle_o == 1,
              "programming controls finish with both planes idle", 1,
              (top.cfg_idle_o && top.idle_o) ? 1 : 0);
}

// GENERATION ZERO IS NOT A SPECIAL CASE (2026-09-26, gz/i13close).
//
// `generation_q[slot]` resets to zero and `resident_q[slot]` resets low.  The
// BEGIN guard used to difference the GENERATION alone, so a slot that had never
// held anything refused generation zero, and zero became the one value no
// producer could hand this block in a single pass.  `zhao_texture_palette_load`
// allocates its generations from its own reset counter and therefore asks for
// zero FIRST; this case is what says it may.
//
// Three statements, in order, and the third is what makes the first two mean
// something: a cold slot LOADS at generation zero; a lookup at {slot, 0}
// RESOLVES to the loaded colour rather than the SOURCE_REFUSED magenta; and the
// slot is then protected by exactly the same law as every other generation -- a
// second BEGIN at zero over the now-RESIDENT slot is refused.  Without the
// third, "zero works" would be a hole rather than a citizenship.
void test_generation_zero_is_ordinary(Vzhao_texture_palette_res_v2& top) {
  reset(top);

  load_slot(top, 1, 0, 0x5000);
  zhao::check(top.loads_ok_o == 1, "a COLD slot completes a load at generation ZERO", 1,
              top.loads_ok_o);
  zhao::check(top.err_same_gen_o == 0,
              "loading a cold slot at generation zero fires no same-generation fault", 0,
              top.err_same_gen_o);

  const Tuple66 ask{0x2AAAAu, 0x00, 0x5C, 0x91, 0x000000};
  accept_request(top, ask, 1, 0);
  const Tuple66 got = take_response(top);
  const Tuple66 want{ask.token, 0x00, ask.index, ask.alpha,
                     expand565(static_cast<uint16_t>(0x5000 + ask.index))};
  zhao::check(same(got, want),
              "a lookup at generation ZERO resolves to the loaded colour, not magenta", 1,
              same(got, want) ? 1 : 0);
  zhao::check(top.cold_o == 0 && top.stale_o == 0,
              "generation zero is RESIDENT and CURRENT, not cold and not stale", 0,
              top.cold_o + top.stale_o);

  // The same law, at the same value.  Now that slot 1 IS resident at zero, a
  // BEGIN at zero is the ABA hazard the counter exists for, and it fires.
  load_op(top, 0, 1, 0, 0, 0, true);
  zhao::check(top.err_same_gen_o == 1,
              "a RESIDENT slot re-BEGUN at generation zero is refused like any other", 1,
              top.err_same_gen_o);
  zhao::check(top.loads_ok_o == 1, "the refused BEGIN completes no second load", 1,
              top.loads_ok_o);

  // And a DIFFERENT generation on the same slot is still taken, so the refusal
  // above is about the value matching and not about the slot.
  load_slot(top, 1, 1, 0x6000);
  zhao::check(top.loads_ok_o == 2 && top.err_same_gen_o == 1,
              "a resident slot still reloads at a NEW generation", 2, top.loads_ok_o);
}

void test_pipeline_cadence(Vzhao_texture_palette_res_v2& top) {
  reset(top);
  load_slot(top, 1, 9, 0x2800);

  constexpr int kJobs = 192;
  std::deque<Tuple66> expected;
  int offered = 0;
  int retired = 0;
  int wrong = 0;
  int ingress_bubbles = 0;
  int retirement_bubbles = 0;
  int first_accept_cycle = -1;
  int last_accept_cycle = -1;
  int first_retire_cycle = -1;
  int last_retire_cycle = -1;
  bool retirement_started = false;
  top.rsp_ready_i = 1;

  for (int cycle = 0; cycle < 1000 && retired < kJobs; ++cycle) {
    top.req_valid_i = offered < kJobs ? 1 : 0;
    Tuple66 in;
    if (offered < kJobs) {
      in.token = static_cast<uint32_t>((0x20000 + offered * 37) & 0x3FFFF);
      in.status = 0;
      in.index = static_cast<uint8_t>(offered);
      in.alpha = static_cast<uint8_t>(255 - offered);
      in.rgb = static_cast<uint32_t>(offered * 0x010101u) & 0xFFFFFFu;
      top.req_slot_i = 1;
      top.req_gen_i = 9;
      drive_tuple(top, in);
    }
    top.eval();

    if (top.req_valid_i && !top.req_ready_o) ++ingress_bubbles;
    if (retirement_started && retired < kJobs && !top.rsp_valid_o) ++retirement_bubbles;

    if (top.rsp_valid_o && top.rsp_ready_i) {
      if (!retirement_started) {
        retirement_started = true;
        first_retire_cycle = cycle;
      }
      last_retire_cycle = cycle;
      if (expected.empty() || !same(read_tuple(top), expected.front()))
        ++wrong;
      else
        expected.pop_front();
      ++retired;
    }
    if (top.req_valid_i && top.req_ready_o) {
      if (first_accept_cycle < 0) first_accept_cycle = cycle;
      last_accept_cycle = cycle;
      Tuple66 want = in;
      want.rgb = expand565(static_cast<uint16_t>(0x2800 + in.index));
      expected.push_back(want);
      ++offered;
    }
    zhao::tick(top);
  }
  top.req_valid_i = 0;
  top.rsp_ready_i = 0;

  zhao::check(offered == kJobs, "palette-v2 accepts the complete cadence stream", kJobs, offered);
  zhao::check(retired == kJobs, "palette-v2 retires the complete cadence stream", kJobs, retired);
  zhao::check(wrong == 0, "palette-v2 cadence stream keeps every tuple aligned", 0, wrong);
  zhao::check(ingress_bubbles == 0 && (last_accept_cycle - first_accept_cycle) == kJobs - 1,
              "all-ready palette ingress has no acceptance bubble", 0, ingress_bubbles);
  zhao::check(retirement_bubbles == 0 && (last_retire_cycle - first_retire_cycle) == kJobs - 1,
              "all-ready palette retirement has no valid bubble after fill", 0, retirement_bubbles);
  zhao::check(top.lookups_o == kJobs && top.stale_o == 0 && top.cold_o == 0 && top.loads_ok_o == 1,
              "cadence lane has exact lookup/stale/cold/load counts", 1,
              (top.lookups_o == kJobs && top.loads_ok_o == 1) ? 1 : 0);
  zhao::check(top.err_write_outside_o == 0 && top.err_same_gen_o == 0 &&
                  top.err_incomplete_o == 0 && top.err_crc_o == 0,
              "cadence lane leaves every programming error counter zero", 0,
              top.err_write_outside_o + top.err_same_gen_o + top.err_incomplete_o + top.err_crc_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_palette_res_v2 top;
  test_cold_and_fresh(top);
  test_generation_and_hold(top);
  test_programming_error_controls(top);
  test_generation_zero_is_ordinary(top);
  test_pipeline_cadence(top);
  return zhao::report_and_exit("texture_palette_res_v2_directed");
}
