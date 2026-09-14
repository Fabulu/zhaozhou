// Packet-B private directed integration test.
// Built only with an isolated Verilator --Mdir; intentionally not in CMake/CTest.
#define DPI_DLLISPEC
#define PLI_DLLISPEC
#include "Vzhao_texture_island_v3_top.h"
#include "Vzhao_texture_island_v3_top__Dpi.h"
#include "verilated.h"
#include "svdpi.h"
#include "../harness/zhao_sim.hpp"

#ifndef PACKET_B_DPI_SCOPE
#define PACKET_B_DPI_SCOPE "TOP.zhao_texture_island_v3_top"
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

double sc_time_stamp() { return 0.0; }

namespace {

[[noreturn]] void fail(const char* what, uint64_t cycle) {
  std::fprintf(stderr, "packet-b directed FAIL at cycle %llu: %s\n",
               static_cast<unsigned long long>(cycle), what);
  std::abort();
}

void require(bool condition, const char* what, uint64_t cycle) {
  if (!condition) fail(what, cycle);
}

uint32_t crc32_byte(uint32_t crc, uint8_t value) {
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = ((crc ^ (value >> bit)) & 1u) ? ((crc >> 1) ^ 0xedb88320u)
                                         : (crc >> 1);
  return crc;
}

struct BindingRow {
  uint32_t base = 0;
  uint32_t mode = 0;
  uint8_t palette_slot = 0;
  uint8_t palette_generation = 0;
  bool valid = false;
};

uint32_t binding_mode(uint8_t format, bool filter, uint8_t wrap_u,
                      uint8_t wrap_v, uint8_t log2w, uint8_t log2h) {
  return static_cast<uint32_t>(format & 7u) |
         (static_cast<uint32_t>(filter) << 3) |
         (static_cast<uint32_t>(wrap_u & 3u) << 4) |
         (static_cast<uint32_t>(wrap_v & 3u) << 6) |
         (static_cast<uint32_t>(log2w & 15u) << 8) |
         (static_cast<uint32_t>(log2h & 15u) << 12);
}

std::array<uint8_t, 10> row_bytes(const BindingRow& row, bool present) {
  std::array<uint8_t, 10> bytes{};
  if (!present) return bytes;
  bytes[0] = static_cast<uint8_t>(row.base);
  bytes[1] = static_cast<uint8_t>(row.base >> 8);
  bytes[2] = static_cast<uint8_t>(row.base >> 16);
  bytes[3] = static_cast<uint8_t>(row.base >> 24);
  bytes[4] = static_cast<uint8_t>(row.mode);
  bytes[5] = static_cast<uint8_t>(row.mode >> 8);
  bytes[6] = static_cast<uint8_t>(row.mode >> 16);
  bytes[7] = static_cast<uint8_t>(row.mode >> 24);
  const uint16_t high = static_cast<uint16_t>(row.palette_slot & 3u) |
      (static_cast<uint16_t>(row.palette_generation) << 2) |
      (static_cast<uint16_t>(row.valid ? 1u : 0u) << 10);
  bytes[8] = static_cast<uint8_t>(high);
  bytes[9] = static_cast<uint8_t>(high >> 8);
  return bytes;
}

uint32_t binding_crc(uint8_t generation,
                     const std::array<BindingRow, 256>& rows,
                     const std::array<bool, 256>& present) {
  uint32_t crc = crc32_byte(0xffffffffu, generation);
  for (unsigned selector = 0; selector < 256; ++selector) {
    const auto bytes = row_bytes(rows[selector], present[selector]);
    for (uint8_t byte : bytes) crc = crc32_byte(crc, byte);
  }
  return crc ^ 0xffffffffu;
}

struct Retired {
  uint32_t rgb = 0;
  uint8_t alpha = 0;
  uint8_t index = 0;
  uint8_t status = 0;
  uint16_t tag = 0;
  std::array<uint32_t, 5> retire{};
  uint64_t cycle = 0;
};

struct SheetPlan {
  uint8_t status = 0;
  uint8_t tag = 0;
  uint8_t strength = 0;
  unsigned delay = 0;
};

struct Harness {
  VerilatedContext context;
  Vzhao_texture_island_v3_top dut{&context};
  uint64_t cycle = 0;
  uint16_t leaf_idle_seen_busy = 0;
  uint16_t leaf_idle_seen_idle = 0;
  bool fill_active = false;
  uint32_t fill_line = 0;
  unsigned fill_beat = 0;
  bool auto_fill = true;
  bool auto_sheet = true;
  std::deque<SheetPlan> sheet_plans;
  bool sheet_pending = false;
  SheetPlan sheet_plan{};
  uint16_t sheet_source = 0;
  unsigned sheet_delay = 0;
  unsigned sheet_requests = 0;
  std::vector<Retired> retired;

  Harness() {
    dut.clk = 0;
    dut.rst_n = 0;
    dut.frag_valid_i = 0;
    dut.frame_fault_clear_valid_i = 0;
    dut.cfg_valid_i = 0;
    dut.cfg_rsp_ready_i = 1;
    dut.fill_req_ready_i = 1;
    dut.fill_data_valid_i = 0;
    dut.fill_data_i = 0;
    dut.fill_refused_i = 0;
    dut.pal_load_valid_i = 0;
    dut.sheet_req_ready_i = 1;
    dut.pg_valid_i = 0;
    dut.pg_op_i = 1;
    dut.pg_status_i = 0;
    dut.pg_tag_i = 0;
    dut.pg_strength_i = 0;
    dut.pg_src_id_i = 0;
    dut.out_ready_i = 1;
    clear_fragment_inputs();
  }

  void clear_fragment_inputs() {
    dut.frag_invw24_i = 0x800000u;
    dut.frag_u_over_w_i = 0;
    dut.frag_v_over_w_i = 0;
    dut.frag_sample_count_i = 0;
    dut.frag_binding_i = 0;
    dut.frag_lod_i = 0;
    dut.frag_recipe_i = 0;
    dut.frag_weight_i = 0;
    dut.frag_ctx_i = 0;
    for (unsigned word = 0; word < 5; ++word) dut.frag_retire_ctx_i[word] = 0;
    for (unsigned word = 0; word < 7; ++word) dut.frag_aux_ctx_i[word] = 0;
    dut.frag_aux_i = 0;
    dut.frag_base_rgb_i = 0;
    dut.frag_base_a_i = 0;
    dut.frag_class_i = 0;
    dut.frag_pal_slot_i = 0;
    dut.frag_pal_gen_i = 0;
  }

  uint16_t fill_word(uint32_t line, unsigned beat) const {
    if (beat != 0) return 0;
    switch (line) {
      case 0x00001000u: return 0xf800u; // RGB565 red
      case 0x00002000u: return 0x0005u; // CLUT8 raw index 5 in low byte
      case 0x00003000u: return 0x001fu; // RGB565 blue, all bilerp taps
      case 0x00006000u: return 0x0005u; // CLUT4 low nibble index 5
      case 0x00007000u: return 0xfc00u; // ARGB1555 opaque red
      case 0x00008000u: return 0x8f10u; // ARGB4444 A=8, R=F, G=1, B=0
      default: return 0x07e0u;
    }
  }

  struct Events {
    bool frag_fire = false;
    bool cfg_fire = false;
    bool out_fire = false;
    bool fill_req_fire = false;
    bool sheet_req_fire = false;
    bool pg_fire = false;
  };

  void observe_leaf_idles() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    svBitVecVal idle_bits = 0;
    zhao_texture_packet_b_get_leaf_idle_vector(&idle_bits);
    leaf_idle_seen_idle |= static_cast<uint16_t>(idle_bits);
    leaf_idle_seen_busy |= static_cast<uint16_t>(~idle_bits);
  }

  std::array<uint32_t, 6> combine_counters() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    std::array<uint32_t, 6> counters{};
    zhao_texture_packet_b_get_combine_counters(
        &counters[0], &counters[1], &counters[2], &counters[3], &counters[4],
        &counters[5]);
    return counters;
  }

  std::array<uint32_t, 4> hostile_counters() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    std::array<uint32_t, 4> counters{};
    zhao_texture_packet_b_get_hostile_counters(
        &counters[0], &counters[1], &counters[2], &counters[3]);
    return counters;
  }

  void set_material_fault_mode(uint32_t mode) {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    zhao_texture_packet_b_set_material_fault_mode(mode);
  }

  void set_combine_hold(bool enable) {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    zhao_texture_packet_b_set_combine_hold(enable ? 1 : 0);
  }

  Events step() {
    if (auto_fill && fill_active) {
      dut.fill_data_valid_i = 1;
      dut.fill_data_i = fill_word(fill_line, fill_beat);
    } else {
      dut.fill_data_valid_i = 0;
      dut.fill_data_i = 0;
    }

    if (auto_sheet && sheet_pending && sheet_delay == 0) {
      dut.pg_valid_i = 1;
      dut.pg_op_i = 1;
      dut.pg_status_i = sheet_plan.status;
      dut.pg_tag_i = sheet_plan.tag;
      dut.pg_strength_i = sheet_plan.strength;
      dut.pg_src_id_i = sheet_source;
    } else if (auto_sheet) {
      dut.pg_valid_i = 0;
    }

    dut.clk = 0;
    dut.eval();
    observe_leaf_idles();

    Events events;
    events.frag_fire = dut.frag_valid_i && dut.frag_ready_o;
    events.cfg_fire = dut.cfg_valid_i && dut.cfg_ready_o;
    events.out_fire = dut.out_valid_o && dut.out_ready_i;
    events.fill_req_fire = dut.fill_req_valid_o && dut.fill_req_ready_i;
    events.sheet_req_fire = dut.sheet_req_valid_o && dut.sheet_req_ready_i;
    events.pg_fire = dut.pg_valid_i && dut.pg_ready_o;

    if (events.out_fire) {
      Retired row;
      row.rgb = dut.out_rgb_o;
      row.alpha = dut.out_a_o;
      row.index = dut.out_texel_idx_o;
      row.status = dut.out_status_o;
      row.tag = dut.out_tag_o;
      for (unsigned word = 0; word < 5; ++word)
        row.retire[word] = dut.out_retire_ctx_o[word];
      row.cycle = cycle;
      retired.push_back(row);
    }

    const uint32_t accepted_fill_line = dut.fill_req_addr_o;
    const uint16_t accepted_sheet_source = dut.sheet_req_src_id_o;

    dut.clk = 1;
    dut.eval();
    ++cycle;
    context.timeInc(1);

    if (auto_fill) {
      if (fill_active) {
        ++fill_beat;
        if (fill_beat == 8) {
          fill_active = false;
          fill_beat = 0;
        }
      }
      if (events.fill_req_fire) {
        require(!fill_active, "cache offered overlapping blocking fills", cycle);
        fill_active = true;
        fill_line = accepted_fill_line;
        fill_beat = 0;
      }
    }

    if (auto_sheet) {
      if (events.pg_fire) sheet_pending = false;
      if (sheet_pending && sheet_delay != 0) --sheet_delay;
      if (events.sheet_req_fire) {
        require(!sheet_pending, "AUX responder observed overlapping request", cycle);
        require(!sheet_plans.empty(), "AUX request had no response plan", cycle);
        sheet_plan = sheet_plans.front();
        sheet_plans.pop_front();
        sheet_pending = true;
        sheet_delay = sheet_plan.delay;
        sheet_source = accepted_sheet_source;
        ++sheet_requests;
      }
    }
    return events;
  }

  void reset() {
    for (unsigned n = 0; n < 4; ++n) step();
    dut.rst_n = 1;
    for (unsigned n = 0; n < 4; ++n) step();
  }

  void wait_quiet(unsigned limit = 20000) {
    for (unsigned n = 0; n < limit; ++n) {
      dut.clk = 0;
      dut.eval();
      if (dut.quiet_o) return;
      step();
    }
    fail("timeout waiting for public quiet", cycle);
  }

  void wait_outputs(std::size_t target, unsigned limit = 20000) {
    for (unsigned n = 0; n < limit && retired.size() < target; ++n) step();
    require(retired.size() == target, "timeout waiting for ordered output", cycle);
  }

  void offer_fragment() {
    dut.frag_valid_i = 1;
    for (unsigned n = 0; n < 10000; ++n) {
      const Events event = step();
      if (event.frag_fire) {
        dut.frag_valid_i = 0;
        return;
      }
    }
    fail("timeout waiting for fragment admission", cycle);
  }

  void set_retire(uint32_t seed) {
    for (unsigned word = 0; word < 5; ++word)
      dut.frag_retire_ctx_i[word] = seed ^ (0x11111111u * (word + 1));
  }

  void set_aux(bool degenerate, uint32_t handle = 0x12345607u) {
    // Low-first ABI: wx,wz,handle,x0,x1,z0,z1.
    dut.frag_aux_ctx_i[0] = 0x00008000u;
    dut.frag_aux_ctx_i[1] = 0x00004000u;
    dut.frag_aux_ctx_i[2] = handle;
    dut.frag_aux_ctx_i[3] = 0x00000000u;
    dut.frag_aux_ctx_i[4] = degenerate ? 0x00000000u : 0x00010000u;
    dut.frag_aux_ctx_i[5] = 0x00000000u;
    dut.frag_aux_ctx_i[6] = 0x00010000u;
  }
};

void expect_result(const Retired& got, uint32_t rgb, uint8_t alpha,
                   uint8_t index, uint8_t status, uint16_t tag,
                   const std::array<uint32_t, 5>& retire, uint64_t cycle) {
  require(got.rgb == rgb, "retired RGB mismatch", cycle);
  require(got.alpha == alpha, "retired alpha mismatch", cycle);
  if (got.index != index)
    std::fprintf(stderr, "  tag=%04x raw-index expected=%02x got=%02x\n",
                 tag, index, got.index);
  require(got.index == index, "retired raw index mismatch", cycle);
  require(got.status == status, "retired status mismatch", cycle);
  require(got.tag == tag, "legacy tag did not alias frag_ctx[15:0]", cycle);
  require(got.retire == retire, "160-bit retirement context mismatch", cycle);
}

std::array<uint32_t, 5> current_retire(const Harness& h) {
  std::array<uint32_t, 5> value{};
  for (unsigned word = 0; word < 5; ++word)
    value[word] = h.dut.frag_retire_ctx_i[word];
  return value;
}

void pulse_palette(Harness& h, uint8_t op, uint16_t index, uint16_t value) {
  h.dut.pal_load_valid_i = 1;
  h.dut.pal_load_op_i = op;
  h.dut.pal_load_slot_i = 0;
  h.dut.pal_load_gen_i = 1;
  h.dut.pal_load_idx_i = index;
  h.dut.pal_load_rgb565_i = value;
  h.dut.pal_load_crc_ok_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
  require(h.dut.pal_load_ready_o, "palette command was not ready", h.cycle);
  h.step();
  h.dut.pal_load_valid_i = 0;
}

uint8_t binding_command(Harness& h, uint8_t op, uint8_t generation,
                        uint8_t selector, const BindingRow& row,
                        uint32_t crc, bool hold_response = false) {
  h.dut.cfg_valid_i = 1;
  h.dut.cfg_op_i = op;
  h.dut.cfg_page_generation_i = generation;
  h.dut.cfg_selector_i = selector;
  h.dut.cfg_row_i[0] = row.base;
  h.dut.cfg_row_i[1] = row.mode;
  h.dut.cfg_row_i[2] = static_cast<uint32_t>(row.palette_slot & 3u) |
      (static_cast<uint32_t>(row.palette_generation) << 2) |
      (static_cast<uint32_t>(row.valid ? 1u : 0u) << 10);
  h.dut.cfg_crc32_i = crc;

  bool accepted = false;
  for (unsigned n = 0; n < 20000; ++n) {
    const auto event = h.step();
    if (event.frag_fire) fail("config helper saw impossible frag fire", h.cycle);
    if (event.cfg_fire && !accepted) {
      accepted = true;
      h.dut.cfg_valid_i = 0;
    }
    h.dut.clk = 0;
    h.dut.eval();
    if (h.dut.cfg_rsp_valid_o) {
      const uint8_t status = h.dut.cfg_rsp_status_o;
      if (hold_response) {
        h.dut.cfg_rsp_ready_i = 0;
        h.dut.eval();
        require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
                "held config response escaped public quiet", h.cycle);
        const uint8_t held_op = h.dut.cfg_rsp_op_o;
        for (unsigned stall = 0; stall < 3; ++stall) {
          h.step();
          require(h.dut.cfg_rsp_valid_o && h.dut.cfg_rsp_op_o == held_op,
                  "config response failed hold law", h.cycle);
        }
        h.dut.cfg_rsp_ready_i = 1;
      }
      h.step();
      return status;
    }
  }
  fail("timeout waiting for binding response", h.cycle);
}

void program_palette(Harness& h) {
  pulse_palette(h, 0, 0, 0);
  h.dut.clk = 0;
  h.dut.eval();
  require(!h.dut.quiet_o, "palette loading state omitted from quiet", h.cycle);
  for (unsigned index = 0; index < 256; ++index)
    pulse_palette(h, 1, static_cast<uint16_t>(index),
                  index == 5 ? 0x07e0u : 0u);
  pulse_palette(h, 2, 0, 0);
}

void program_bindings(Harness& h) {
  std::array<BindingRow, 256> rows{};
  std::array<bool, 256> present{};
  rows[1] = BindingRow{0x00001000u, 0x00000001u, 0, 0, true};
  rows[2] = BindingRow{0x00002000u, 0x00000000u, 0, 1, true};
  rows[3] = BindingRow{0x00003000u, 0x00000009u, 0, 0, true};
  rows[4] = BindingRow{0x00004000u, 0x00000001u, 0, 0, true};
  rows[5] = BindingRow{0x00001010u, 0x00000001u, 0, 0, true};
  rows[6] = BindingRow{0x00001020u, 0x00000001u, 0, 0, true};
  rows[7] = BindingRow{0x00001030u, 0x00000001u, 0, 0, true};
  rows[8] = BindingRow{0x00009000u,
                       binding_mode(1, false, 0, 0, 3, 3), 0, 0, true};
  rows[9] = BindingRow{0x00006000u, 0x00000002u, 0, 1, true};
  rows[10] = BindingRow{0x00007000u, 0x00000003u, 0, 0, true};
  rows[11] = BindingRow{0x00008000u, 0x00000004u, 0, 0, true};
  for (unsigned selector = 1; selector <= 11; ++selector)
    present[selector] = true;
  const uint32_t crc = binding_crc(1, rows, present);
  BindingRow zero{};

  require(binding_command(h, 0, 1, 0, zero, 0, true) == 0,
          "binding BEGIN failed", h.cycle);
  for (unsigned selector = 1; selector <= 11; ++selector)
    require(binding_command(h, 1, 1, static_cast<uint8_t>(selector),
                            rows[selector], 0) == 0,
            "binding WRITE failed", h.cycle);
  require(binding_command(h, 2, 1, 0, zero, crc) == 0,
          "binding END/CRC/seal failed", h.cycle);
  require(h.dut.active_page_generation_o == 1,
          "binding seal did not atomically activate generation 1", h.cycle);
}

void check_public_quiet_inputs(Harness& h) {
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;

  h.dut.fill_data_valid_i = 1;
  h.dut.clk = 0; h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "unsolicited fill data omitted from public quiet", h.cycle);
  h.dut.fill_data_valid_i = 0;

  h.dut.fill_refused_i = 1;
  h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "fill refusal omitted from public quiet", h.cycle);
  h.dut.fill_refused_i = 0;

  h.auto_sheet = false;
  h.dut.pg_valid_i = 1;
  h.dut.pg_op_i = 1;
  h.dut.pg_status_i = 0;
  h.dut.pg_src_id_i = 0;
  h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "unsolicited Sheet response omitted from public quiet", h.cycle);
  h.step(); // accepted by the AUX protocol-fault sink
  h.dut.pg_valid_i = 0;
  h.auto_sheet = true;

  h.dut.pal_load_valid_i = 1;
  h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "palette command omitted from public quiet", h.cycle);
  h.dut.pal_load_valid_i = 0;

  h.dut.cfg_valid_i = 1;
  h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "binding command omitted from public quiet", h.cycle);
  h.dut.cfg_valid_i = 0;

  h.dut.frag_valid_i = 1;
  h.dut.eval();
  require(!h.dut.quiet_o && !h.dut.frame_fault_clear_ready_o,
          "fragment offer omitted from public quiet", h.cycle);
  h.dut.frag_valid_i = 0;

  h.dut.eval();
  require(h.dut.quiet_o && h.dut.frame_fault_clear_ready_o,
          "fault state or held clear perturbed full quiet", h.cycle);
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o,
          "idle clear did not clear unsolicited Sheet frame fault", h.cycle);
}

void set_fragment(Harness& h, uint16_t tag, uint32_t retire_seed,
                  uint8_t count, uint8_t binding, uint8_t recipe,
                  bool aux, uint8_t response_class, uint8_t pal_gen,
                  uint32_t base_rgb = 0x123456u, uint8_t base_a = 0x7au) {
  h.clear_fragment_inputs();
  h.dut.frag_ctx_i = 0xfeed000000000000ull | tag;
  h.set_retire(retire_seed);
  h.dut.frag_sample_count_i = count;
  h.dut.frag_binding_i = binding;
  h.dut.frag_recipe_i = recipe;
  h.dut.frag_aux_i = aux;
  h.dut.frag_class_i = response_class;
  h.dut.frag_pal_slot_i = 0;
  h.dut.frag_pal_gen_i = pal_gen;
  h.dut.frag_base_rgb_i = base_rgb;
  h.dut.frag_base_a_i = base_a;
}

void run_material_hostile_cases(Harness& h) {
  struct Case { uint32_t mode; const char* name; bool combine_mismatch; };
  const std::array<Case, 4> cases{{
      {1u, "D-invalid", true},
      {2u, "M-invalid", true},
      {3u, "D-and-M-invalid", true},
      {4u, "material-mask-disagree", true},
  }};

  for (unsigned index = 0; index < cases.size(); ++index) {
    h.wait_quiet();
    h.dut.frame_fault_clear_valid_i = 1;
    h.step();
    h.dut.frame_fault_clear_valid_i = 0;
    h.step();
    require(!h.dut.frame_fault_o,
            "hostile material case did not start recoverably clear", h.cycle);

    const auto before = h.hostile_counters();
    const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
    h.set_material_fault_mode(cases[index].mode);
    set_fragment(h, static_cast<uint16_t>(0x3000u + index),
                 0xc0000000u + index, 1, 1, 0, false, 1, 0);
    const auto retire = current_retire(h);
    h.offer_fragment();
    h.wait_outputs(h.retired.size() + 1);
    expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1,
                  static_cast<uint16_t>(0x3000u + index), retire, h.cycle);
    const auto after = h.hostile_counters();
    require(after[0] == before[0] + 1 && after[1] == before[1] + 1 &&
            after[2] == before[2] + 1,
            "hostile material case did not issue/refuse/commit authoritative sample",
            h.cycle);
    require(after[3] == before[3] + (cases[index].combine_mismatch ? 1u : 0u),
            "hostile material mismatch evidence delta was wrong", h.cycle);
    require(h.dut.cnt_plan_accepted_o == planner_before,
            "hostile material refusal reached planner/cache", h.cycle);
    require(h.dut.frame_fault_o,
            "hostile material case did not set recoverable frame fault", h.cycle);

    h.set_material_fault_mode(0);
    h.wait_quiet();
    h.dut.frame_fault_clear_valid_i = 1;
    h.step();
    h.dut.frame_fault_clear_valid_i = 0;
    h.step();
    require(!h.dut.frame_fault_o,
            "hostile material case incorrectly entered reset lifetime", h.cycle);
  }

  // Empty owner masks become combine-ready before any sample/AUX commit. A bad
  // descriptor must therefore be fenced until joined validation and converted
  // directly into a canonical loud/refused combine ticket; there is no source
  // plane on which the expander could carry this refusal.
  h.wait_quiet();
  const auto before = h.hostile_counters();
  const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
  h.set_material_fault_mode(1);
  set_fragment(h, 0x30f0, 0xc00000f0u, 0, 0, 0, false, 0, 0,
               0x2468acu, 0x5du);
  const auto retire = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1,
                0x30f0, retire, h.cycle);
  const auto after = h.hostile_counters();
  require(after[0] == before[0] && after[1] == before[1] &&
          after[2] == before[2] && after[3] == before[3] + 1,
          "count-zero descriptor refusal did not bypass source issue and enter canonical combine",
          h.cycle);
  require(h.dut.cnt_plan_accepted_o == planner_before && h.dut.frame_fault_o,
          "count-zero descriptor refusal reached planner or failed to set frame fault", h.cycle);
  h.set_material_fault_mode(0);
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o,
          "count-zero descriptor refusal incorrectly entered reset lifetime", h.cycle);
}

void run_directed() {
  Harness* const harness = new Harness;
  Harness& h = *harness;
  h.reset();
  require(h.dut.quiet_o, "island did not reset to public quiet", h.cycle);
  check_public_quiet_inputs(h);
  program_palette(h);
  program_bindings(h);
#if !defined(PACKET_B_EXPECT_INDEX_ROUTE_MUTANT) && \
    !defined(PACKET_B_EXPECT_AUX_AS_SAMPLE2) && \
    !defined(PACKET_B_EXPECT_RETIRE_TRUNCATION) && \
    !defined(PACKET_B_EXPECT_OWNER_MASK_LIFETIME) && \
    !defined(PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME) && \
    !defined(PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL) && \
    !defined(PACKET_B_EXPECT_SHADOW_CORRUPTION)
  run_material_hostile_cases(h);
#endif

  // Count-zero PASSTHRU uses admitted base and no TMU/AUX source.
  set_fragment(h, 0x1001, 0xa0000001u, 0, 0, 0, false, 0, 0);
  const auto count0_ctx = current_retire(h);
  const std::size_t count0_out = h.retired.size() + 1;
  h.offer_fragment();
#ifdef PACKET_B_EXPECT_OWNER_MASK_LIFETIME
  for (unsigned n = 0; n < 2000 && !h.dut.frame_fault_o; ++n) h.step();
  require(h.dut.frame_fault_o && !h.dut.frag_ready_o && h.retired.empty(),
          "owner-mask generation mutant did not enter reset lifetime", h.cycle);
  std::printf("packet-b owner-mask-generation mutant FIRED\n");
  return;
#endif
  h.wait_outputs(count0_out);
#ifdef PACKET_B_EXPECT_RETIRE_TRUNCATION
  require(h.retired.back().retire[0] == count0_ctx[0] &&
          h.retired.back().retire[1] == count0_ctx[1] &&
          h.retired.back().retire[2] == 0 &&
          h.retired.back().retire[3] == 0 &&
          h.retired.back().retire[4] == 0,
          "retirement truncation mutant did not zero upper 96 bits", h.cycle);
  std::printf("packet-b retire-context-truncation mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x123456u, 0x7a, 0, 0, 0x1001,
                count0_ctx, h.cycle);

  // NEAR tuple: direct RGB565 decode, opaque alpha, raw index zero.
  set_fragment(h, 0x1002, 0xa0000002u, 1, 1, 0, false, 1, 0);
  const auto near_ctx = current_retire(h);
  h.offer_fragment();
#ifdef PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME
  const std::size_t before_illegal_cache = h.retired.size();
  for (unsigned n = 0; n < 5000 && !h.dut.frame_fault_o; ++n) h.step();
  require(h.dut.frame_fault_o && !h.dut.frag_ready_o &&
          h.retired.size() == before_illegal_cache,
          "cache sample-3 mutant did not consume/drop into reset lifetime", h.cycle);
  require(h.dut.meta_shadow_reads_o == 0,
          "cache sample-3 mutant launched metadata read", h.cycle);
  std::printf("packet-b cache-sidx3 mutant FIRED\n");
  return;
#endif
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_SHADOW_CORRUPTION
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1002,
                near_ctx, h.cycle);
  require(h.dut.shadow_present_o && h.dut.meta_shadow_reads_o == 1 &&
          h.dut.meta_shadow_mismatch_o == 1 &&
          h.dut.meta_near_chk_o == 1 && h.dut.meta_near_err_o == 1 &&
          h.dut.meta_align_chk_o == 0 && h.dut.meta_bil_chk_o == 0,
          "one-sided shadow corruption did not fire exact NEAR counters", h.cycle);
  std::printf("packet-b one-sided-shadow mutant FIRED\n");
  return;
#endif
#ifdef PACKET_B_EXPECT_INDEX_ROUTE_MUTANT
  require(h.retired.back().rgb == 0xff0000u &&
          h.retired.back().index != 0 && h.retired.back().status == 0,
          "dispatcher/index-route mutant did not corrupt first TMU raw index",
          h.cycle);
  std::printf("packet-b dispatcher-index-route mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1002,
                near_ctx, h.cycle);
#ifdef PACKET_B_EXPECT_RSP_DROP_OBSERVATION
  require(h.dut.err_rsp_dropped_o && h.dut.frame_fault_o && !h.dut.frag_ready_o,
          "response hold-violation mutant did not enter shipped reset-lifetime barrier", h.cycle);
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(h.dut.err_rsp_dropped_o && h.dut.frame_fault_o,
          "ordinary frame clear erased response-drop lifetime fault", h.cycle);
  h.dut.rst_n = 0;
  h.step();
  h.dut.rst_n = 1;
  h.step();
  require(!h.dut.err_rsp_dropped_o && !h.dut.frame_fault_o,
          "reset did not clear response-drop lifetime fault", h.cycle);
  std::printf("packet-b response-drop detector mutant FIRED\n");
  return;
#else
  require(!h.dut.err_rsp_dropped_o,
          "healthy cache response path fired dropped-response detector", h.cycle);
#endif

  // CLUT tuple: addressed raw index survives palette latency unchanged.
  set_fragment(h, 0x1003, 0xa0000003u, 1, 2, 0, false, 0, 1);
  const auto clut_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_INDEX_ROUTE_MUTANT
  require(h.retired.back().rgb == 0x00ff00u &&
          h.retired.back().index != 5 && h.retired.back().status == 0,
          "dispatcher/index-route mutant did not corrupt only raw index", h.cycle);
  std::printf("packet-b dispatcher-index-route mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x00ff00u, 0xff, 5, 0, 0x1003,
                clut_ctx, h.cycle);

  // Selected-top format seams removed with the historical composed driver.
  // Exercise CLUT4 nibble selection and both direct alpha formats here instead
  // of relying on leaf decode tests that cannot see top-level wiring.
  set_fragment(h, 0x1013, 0xa0000013u, 1, 9, 0, false, 0, 1);
  const auto clut4_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0x00ff00u, 0xff, 5, 0, 0x1013,
                clut4_ctx, h.cycle);

  set_fragment(h, 0x1014, 0xa0000014u, 1, 10, 0, false, 1, 0);
  const auto argb1555_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1014,
                argb1555_ctx, h.cycle);

  set_fragment(h, 0x1015, 0xa0000015u, 1, 11, 0, false, 1, 0);
  const auto argb4444_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff1100u, 0x88, 0, 0, 0x1015,
                argb4444_ctx, h.cycle);

  // Ingress class 3 is invalid rather than an ordinary raw ERR request. The
  // detector must fire, the owner must retire loudly, and legal traffic must
  // remain possible after the recoverable clear.
  const uint32_t invalid_class_before = h.dut.err_class_invalid_o;
  set_fragment(h, 0x1016, 0xa0000016u, 1, 1, 0, false, 3, 0);
  const auto invalid_class_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1016,
                invalid_class_ctx, h.cycle);
  require(h.dut.err_class_invalid_o == invalid_class_before + 1 &&
          h.dut.frame_fault_o,
          "ingress class-3 detector did not fire exactly once", h.cycle);
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o,
          "recoverable ingress class-3 fault did not clear", h.cycle);

  // BIL tuple: four physical lanes produce RGBA; equal taps stay exact.
  const uint32_t bilerp_jobs_before = h.dut.cnt_bilerp_jobs_o;
  set_fragment(h, 0x1004, 0xa0000004u, 1, 3, 0, false, 2, 0);
  const auto bil_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1004,
                bil_ctx, h.cycle);
  require(h.dut.frame_fault_o,
          "bilerp identity mutant did not set recoverable frame fault", h.cycle);
  require(h.dut.cnt_bilerp_jobs_o == bilerp_jobs_before + 4,
          "bilerp identity mutant changed physical job count", h.cycle);
  std::printf("packet-b bilerp-identity mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x0000ffu, 0xff, 0, 0, 0x1004,
                bil_ctx, h.cycle);
  require(h.dut.cnt_bilerp_jobs_o == bilerp_jobs_before + 4,
          "BIL response did not execute exactly four RGBA lane jobs", h.cycle);

  // Warm three non-conflicting direct-cache lines, then require the metadata
  // join to sustain the TMU's one-response-per-two-clocks hot cadence. The old
  // capture-then-hold path produced gaps of three even with every line resident.
  set_fragment(h, 0x1040, 0xa0000040u, 3, 5, 6, false, 1, 0);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  require(h.retired.back().status == 0,
          "metadata cadence warm-up fragment refused", h.cycle);
  set_fragment(h, 0x1041, 0xa0000041u, 3, 5, 6, false, 1, 0);
  const std::size_t hot_target = h.retired.size() + 1;
  h.offer_fragment();
#ifndef PACKET_B_EXPECT_NO_SHADOWS
  std::vector<uint64_t> metadata_result_cycles;
  uint32_t previous_metadata_reads = h.dut.meta_shadow_reads_o;
  for (unsigned n = 0; n < 20000 && h.retired.size() < hot_target; ++n) {
    h.step();
    if (h.dut.meta_shadow_reads_o != previous_metadata_reads) {
      require(h.dut.meta_shadow_reads_o == previous_metadata_reads + 1,
              "metadata result counter skipped during hot three-sample fragment", h.cycle);
      metadata_result_cycles.push_back(h.cycle);
      previous_metadata_reads = h.dut.meta_shadow_reads_o;
    }
  }
  require(h.retired.size() == hot_target && metadata_result_cycles.size() == 3,
          "hot three-sample fragment did not produce exactly three metadata results", h.cycle);
  require(metadata_result_cycles[1] - metadata_result_cycles[0] == 2 &&
          metadata_result_cycles[2] - metadata_result_cycles[1] == 2,
          "metadata fall-through path did not sustain one cache response every two clocks",
          h.cycle);
#else
  h.wait_outputs(hot_target);
#endif
  require(h.retired.back().status == 0,
          "hot metadata cadence fragment refused", h.cycle);

  // AUX HIT is independently owed and cannot become sample 2 or alter RGBA.
  h.sheet_plans.push_back(SheetPlan{0, 0x99, 0x44, 6});
  h.dut.sheet_req_ready_i = 0;
  set_fragment(h, 0x1005, 0xa0000005u, 1, 1, 0, true, 1, 0);
  h.set_aux(false);
  const auto aux_hit_ctx = current_retire(h);
  const unsigned sheet_before_hit = h.sheet_requests;
  h.offer_fragment();
  for (unsigned n = 0; n < 20000 && !h.dut.sheet_req_valid_o; ++n) h.step();
  require(h.dut.sheet_req_valid_o && !h.dut.quiet_o,
          "held Sheet request omitted from quiet", h.cycle);
  const uint8_t held_sheet_op = h.dut.sheet_req_op_o;
  const uint32_t held_sheet_handle = h.dut.sheet_req_handle_o;
  const uint16_t held_sheet_texel = h.dut.sheet_req_texel_o;
  const uint16_t held_sheet_source = h.dut.sheet_req_src_id_o;
  for (unsigned n = 0; n < 3; ++n) {
    h.step();
    require(h.dut.sheet_req_valid_o && h.dut.sheet_req_op_o == held_sheet_op &&
            h.dut.sheet_req_handle_o == held_sheet_handle &&
            h.dut.sheet_req_texel_o == held_sheet_texel &&
            h.dut.sheet_req_src_id_o == held_sheet_source,
            "Sheet request packet changed while stalled", h.cycle);
  }
  h.dut.sheet_req_ready_i = 1;
  h.step();
  h.dut.clk = 0;
  h.dut.eval();
  require(!h.dut.quiet_o,
          "accepted Sheet READ owing a response escaped quiet", h.cycle);
  h.wait_outputs(h.retired.size() + 1);
  require(h.sheet_requests == sheet_before_hit + 1,
          "AUX HIT did not issue exactly one Sheet READ", h.cycle);
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1005,
                aux_hit_ctx, h.cycle);

  // Sample 2 and AUX are simultaneously required. DETAIL_MASK must take alpha
  // from real sample 2 (blue ARGB/RGB565 => 255), never AUX strength 0x22.
  h.sheet_plans.push_back(SheetPlan{0, 0x7d, 0x22, 3});
  set_fragment(h, 0x1055, 0xa0000055u, 3, 1, 7, true, 1, 0);
  h.set_aux(false, 0x12345617u);
  const auto sample2_aux_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_AUX_AS_SAMPLE2
  expect_result(h.retired.back(), 0x000000u, 0x22, 0, 0, 0x1055,
                sample2_aux_ctx, h.cycle);
  std::printf("packet-b AUX-as-sample2 mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x000000u, 0xfe, 0, 0, 0x1055,
                sample2_aux_ctx, h.cycle);

  // AUX MISS is a typed terminal refusal and still retires normally.
  h.sheet_plans.push_back(SheetPlan{3, 0xaa, 0xbb, 2});
  set_fragment(h, 0x1006, 0xa0000006u, 1, 1, 0, true, 1, 0);
  h.set_aux(false, 0x12345608u);
  const auto aux_miss_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1006,
                aux_miss_ctx, h.cycle);
  require(h.dut.frame_fault_o,
          "typed AUX MISS did not set frame fault", h.cycle);

  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o, "recoverable AUX fault did not clear", h.cycle);

  // Degenerate AUX: logical issue at acceptance, no Sheet access, local return
  // on a later cycle, loud refusal, and ordinary owner release.
  const uint32_t aux_accept_before = h.dut.cnt_aux_accepted_o;
  const unsigned sheet_before_degenerate = h.sheet_requests;
  set_fragment(h, 0x1007, 0xa0000007u, 0, 0, 0, true, 0, 0,
               0x654321u, 0x66);
  h.set_aux(true);
  const auto degenerate_ctx = current_retire(h);
  h.offer_fragment();
  uint64_t aux_issue_cycle = 0;
  const std::size_t degenerate_target = h.retired.size() + 1;
  for (unsigned n = 0; n < 20000 && h.retired.size() < degenerate_target; ++n) {
    h.step();
    if (!aux_issue_cycle && h.dut.cnt_aux_accepted_o != aux_accept_before)
      aux_issue_cycle = h.cycle;
  }
  require(aux_issue_cycle != 0, "degenerate AUX never reached logical issue", h.cycle);
  require(h.retired.size() == degenerate_target,
          "degenerate AUX did not terminally retire", h.cycle);
  require(h.retired.back().cycle + 1 > aux_issue_cycle,
          "local AUX refusal did not follow its logical issue", h.cycle);
  require(h.sheet_requests == sheet_before_degenerate,
          "degenerate AUX illegally issued a Sheet READ", h.cycle);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1007,
                degenerate_ctx, h.cycle);

  // Malformed material still issues its declared sample before local refusal;
  // no planner/cache request is allowed to escape.
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1; h.step();
  h.dut.frame_fault_clear_valid_i = 0; h.step();
  const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
  set_fragment(h, 0x1008, 0xa0000008u, 1, 1, 1, false, 1, 0);
  const auto malformed_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1008,
                malformed_ctx, h.cycle);
  require(h.dut.cnt_plan_accepted_o == planner_before,
          "malformed material reached planner/cache", h.cycle);
  require(h.dut.frame_fault_o,
          "malformed material did not set frame fault", h.cycle);

  // Hold a real fill request and a real Sheet request to exercise their output
  // channel operands, then release them without changing payload.
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1; h.step();
  h.dut.frame_fault_clear_valid_i = 0; h.step();
  h.auto_fill = false;
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x1009, 0xa0000009u, 1, 8, 0, false, 1, 0);
  h.dut.frag_invw24_i = 0x00010000;
  h.dut.frag_u_over_w_i = 0x00005000;
  h.dut.frag_v_over_w_i = 0x00009000;
  h.offer_fragment();
  for (unsigned n = 0; n < 20000 && !h.dut.fill_req_valid_o; ++n) h.step();
  require(h.dut.fill_req_valid_o && !h.dut.quiet_o,
          "held fill request omitted from quiet", h.cycle);
  const uint32_t held_fill_address = h.dut.fill_req_addr_o;
  if (held_fill_address != 0x00009040u)
    std::fprintf(stderr, "  nonzero perspective fill line: %08x\n",
                 held_fill_address);
  require(held_fill_address == 0x00009040u,
          "nonzero perspective coordinates produced the wrong 8x8 RGB565 cache line",
          h.cycle);
  for (unsigned n = 0; n < 3; ++n) {
    h.step();
    require(h.dut.fill_req_valid_o && h.dut.fill_req_addr_o == held_fill_address,
            "fill request payload changed while stalled", h.cycle);
  }
  h.dut.fill_req_ready_i = 1;
  h.step();
  h.auto_fill = true;
  h.fill_active = true;
  h.fill_line = held_fill_address;
  h.fill_beat = 0;
  h.wait_outputs(h.retired.size() + 1);

  // Prepared final tuples: after filling owner output under backpressure, the
  // ordered boundary must retire one result every clock when reopened.
  h.wait_quiet();
  h.dut.out_ready_i = 0;
  h.set_combine_hold(true);
  constexpr unsigned kPrepared = 32;
  const std::size_t prepared_begin = h.retired.size();
  const uint32_t combine_jobs_before = h.combine_counters()[1];
  for (unsigned item = 0; item < kPrepared; ++item) {
    set_fragment(h, static_cast<uint16_t>(0x2000u + item),
                 0xb0000000u + item, 0, 0, 0, false, 0, 0,
                 0x010203u + item, static_cast<uint8_t>(0x80u + item));
    h.offer_fragment();
  }
  for (unsigned n = 0; n < 20000 && h.combine_counters()[5] != 8; ++n)
    h.step();
  require(h.combine_counters()[5] == 8,
          "combine cadence test did not establish guaranteed FIFO backlog", h.cycle);
  uint32_t accepted_each_clock = h.combine_counters()[1];
  h.set_combine_hold(false);
  for (unsigned clock = 0; clock < 8; ++clock) {
    h.step();
    const uint32_t next_accepted = h.combine_counters()[1];
    require(next_accepted == accepted_each_clock + 1,
            "backlogged combine admission was not one job per clock", h.cycle);
    accepted_each_clock = next_accepted;
  }
  for (unsigned n = 0; n < 20000 && !h.dut.out_valid_o; ++n) h.step();
  require(h.dut.out_valid_o && !h.dut.quiet_o,
          "held retirement omitted from quiet", h.cycle);
  // Let every remaining one-phase job finish while the ordered edge is held.
  for (unsigned n = 0; n < 1200; ++n) h.step();
  const auto combine_after_preload = h.combine_counters();
  require(combine_after_preload[1] == combine_jobs_before + kPrepared,
          "one-phase stream did not admit exactly one combine job per fragment",
          h.cycle);
  const uint32_t held_rgb = h.dut.out_rgb_o;
  const uint16_t held_tag = h.dut.out_tag_o;
  for (unsigned n = 0; n < 3; ++n) {
    h.step();
    require(h.dut.out_valid_o && h.dut.out_rgb_o == held_rgb &&
            h.dut.out_tag_o == held_tag,
            "ordered result changed under output backpressure", h.cycle);
  }
  h.dut.out_ready_i = 1;
  uint64_t prior_cycle = 0;
  h.wait_outputs(prepared_begin + kPrepared);
  for (std::size_t index = prepared_begin;
       index < prepared_begin + kPrepared; ++index) {
    if (index != prepared_begin)
      require(h.retired[index].cycle == prior_cycle + 1,
              "prepared owner tuples did not retire one per clock", h.cycle);
    prior_cycle = h.retired[index].cycle;
  }
  const auto combine_drained = h.combine_counters();
  require(combine_drained[1] == combine_drained[2] &&
          combine_drained[3] == combine_drained[4],
          "combine CJ/CD or PI/PC counters did not close", h.cycle);

  require(h.dut.cnt_fragrob_id_errors_o == 0,
          "healthy traffic moved a typed owner protocol counter", h.cycle);
  require(h.dut.err_rcp_q_o == 0,
          "RCP reset-lifetime queue fault moved", h.cycle);
  require(h.dut.meta_genmis_o == 0,
          "metadata generation mismatch moved", h.cycle);
  require(h.leaf_idle_seen_idle == 0xffffu &&
          h.leaf_idle_seen_busy == 0xffffu,
          "not every composed leaf idle had both idle and busy evidence", h.cycle);
#ifdef PACKET_B_EXPECT_NO_SHADOWS
  require(!h.dut.shadow_present_o && h.dut.meta_shadow_reads_o == 0 &&
          h.dut.meta_shadow_mismatch_o == 0,
          "production profile retained migration-shadow capability", h.cycle);
#else
  require(h.dut.shadow_present_o && h.dut.meta_shadow_reads_o != 0,
          "laboratory profile did not execute metadata shadows", h.cycle);
  require(h.dut.meta_shadow_mismatch_o == 0 &&
          h.dut.meta_align_err_o == 0 && h.dut.meta_bil_err_o == 0 &&
          h.dut.meta_near_err_o == 0,
          "laboratory metadata shadow disagreed with functional carriage", h.cycle);
  require(h.dut.meta_align_chk_o != 0 && h.dut.meta_bil_chk_o != 0 &&
          h.dut.meta_near_chk_o != 0,
          "laboratory shadow did not cover all three functional classes", h.cycle);
#endif

  // Reset with live composed work, then reuse the owner namespace. No cache,
  // metadata, class, AUX, combine, dispatcher, or owner record from the old
  // epoch may retire after reset.
  {
    Harness* const reset_storage = new Harness;
    Harness& reset_case = *reset_storage;
    reset_case.reset();
    program_palette(reset_case);
    program_bindings(reset_case);
    reset_case.dut.out_ready_i = 0;
    for (unsigned item = 0; item < 8; ++item) {
      set_fragment(reset_case, static_cast<uint16_t>(0x5000u + item),
                   0xd0000000u + item, 0, 0, 0, false, 0, 0,
                   0x112200u + item, static_cast<uint8_t>(0x40u + item));
      reset_case.offer_fragment();
    }
    for (unsigned n = 0; n < 20000 && !reset_case.dut.out_valid_o; ++n)
      reset_case.step();
    require(reset_case.dut.out_valid_o && !reset_case.dut.quiet_o &&
            reset_case.retired.empty(),
            "mid-flight reset control did not establish held composed work",
            reset_case.cycle);
    reset_case.dut.rst_n = 0;
    reset_case.step();
    reset_case.step();
    reset_case.clear_fragment_inputs();
    reset_case.dut.out_ready_i = 1;
    reset_case.dut.rst_n = 1;
    for (unsigned n = 0; n < 8; ++n) reset_case.step();
    program_palette(reset_case);
    program_bindings(reset_case);
    require(reset_case.retired.empty(),
            "pre-reset composed result escaped into the new owner namespace",
            reset_case.cycle);
    set_fragment(reset_case, 0x50ff, 0xd00000ffu, 0, 0, 0, false, 0, 0,
                 0x334455u, 0x66);
    const auto fresh_ctx = current_retire(reset_case);
    reset_case.offer_fragment();
    reset_case.wait_outputs(1);
    expect_result(reset_case.retired.back(), 0x334455u, 0x66, 0, 0,
                  0x50ff, fresh_ctx, reset_case.cycle);
  }

  // Packet-B cannot terminate a denied cache fill yet.  Any presented refusal is
  // therefore a reset-lifetime unsupported condition, not an inert silent wait.
  {
    Harness* const lifetime_storage = new Harness;
    Harness& lifetime = *lifetime_storage;
    lifetime.reset();
    lifetime.dut.fill_refused_i = 1;
    lifetime.step();
    lifetime.dut.fill_refused_i = 0;
    lifetime.wait_quiet();
    require(lifetime.dut.frame_fault_o,
            "unsupported fill refusal did not latch lifetime fault", lifetime.cycle);
    lifetime.dut.frame_fault_clear_valid_i = 1;
    lifetime.step();
    lifetime.dut.frame_fault_clear_valid_i = 0;
    lifetime.step();
    require(lifetime.dut.frame_fault_o,
            "frame clear erased unsupported fill-refusal lifetime fault",
            lifetime.cycle);
  }

  std::printf("packet-b directed PASS: outputs=%zu near/clut/bil exact, "
              "sheet_requests=%u, bilerp_jobs=%u, active_generation=%u\n",
              h.retired.size(), h.sheet_requests,
              static_cast<unsigned>(h.dut.cnt_bilerp_jobs_o),
              static_cast<unsigned>(h.dut.active_page_generation_o));
}

} // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  run_directed();
  zhao::exit_hard(0);
}
