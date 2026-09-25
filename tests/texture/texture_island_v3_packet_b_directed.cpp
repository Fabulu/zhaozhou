// Packet-B private directed integration test.
// Built only with an isolated Verilator --Mdir; intentionally not in CMake/CTest.
// Packet-E mutant drivers select exactly one expected inverse. Production selects
// none. Keep this guard before every include so the C++ side fails independently
// and with one exact diagnostic even when generated headers are unavailable.
#if ((defined(PACKET_E_EXPECT_PRE_E_FILL_LIFETIME) +       \
      defined(PACKET_E_EXPECT_RELABEL_REFUSAL_ERR) +       \
      defined(PACKET_E_EXPECT_REFUSAL_NATIVE_ARITH) +      \
      defined(PACKET_E_EXPECT_DROP_HELD_REFUSAL)) != 0) && \
    ((defined(PACKET_E_EXPECT_PRE_E_FILL_LIFETIME) +       \
      defined(PACKET_E_EXPECT_RELABEL_REFUSAL_ERR) +       \
      defined(PACKET_E_EXPECT_REFUSAL_NATIVE_ARITH) +      \
      defined(PACKET_E_EXPECT_DROP_HELD_REFUSAL)) != 1)
#error PACKET_E_EXPECT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
#else

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
    crc = ((crc ^ (value >> bit)) & 1u) ? ((crc >> 1) ^ 0xedb88320u) : (crc >> 1);
  return crc;
}

struct BindingRow {
  uint32_t base = 0;
  uint32_t mode = 0;
  uint8_t palette_slot = 0;
  uint8_t palette_generation = 0;
  bool valid = false;
};

uint32_t binding_mode(uint8_t format, bool filter, uint8_t wrap_u, uint8_t wrap_v, uint8_t log2w,
                      uint8_t log2h) {
  return static_cast<uint32_t>(format & 7u) | (static_cast<uint32_t>(filter) << 3) |
         (static_cast<uint32_t>(wrap_u & 3u) << 4) | (static_cast<uint32_t>(wrap_v & 3u) << 6) |
         (static_cast<uint32_t>(log2w & 15u) << 8) | (static_cast<uint32_t>(log2h & 15u) << 12);
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

uint32_t binding_crc(uint8_t generation, const std::array<BindingRow, 256>& rows,
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

enum class FillPlan { Success, Refuse, PartialRefuse, SimultaneousRefuse };

struct Tuple66 {
  std::array<uint32_t, 3> words{};

  uint32_t rgb() const { return words[0] & 0x00ffffffu; }
  uint8_t alpha() const { return static_cast<uint8_t>(words[0] >> 24); }
  uint8_t index() const { return static_cast<uint8_t>(words[1]); }
  uint8_t status() const { return static_cast<uint8_t>(words[1] >> 8); }
  uint32_t token() const { return ((words[1] >> 16) & 0xffffu) | ((words[2] & 3u) << 16); }
  bool operator==(const Tuple66& rhs) const { return words == rhs.words; }
};

struct MergeState {
  bool native_valid = false;
  bool refusal_valid = false;
  bool merged_valid = false;
  bool merged_ready = false;
  bool selected_refusal = false;
  bool rr_refusal = false;
  Tuple66 native_tuple{};
  Tuple66 refusal_tuple{};
  Tuple66 merged_tuple{};
};

struct MergeAccept {
  unsigned response_class = 0;
  bool refusal = false;
  Tuple66 tuple{};
  uint64_t cycle = 0;
  bool contended = false;
};

struct CacheCounters {
  uint32_t cache_accepted = 0;
  uint32_t cache_completed = 0;
  uint32_t fill_accepted = 0;
  uint32_t fill_completed = 0;
  uint32_t fill_refused = 0;
  uint32_t fill_beats = 0;
  bool protocol_fault = false;
  uint32_t reservations = 0;
  uint8_t reservation_owners = 0;
  uint16_t work_state = 0;
};

struct QuietObservation {
  bool q_dispatch_valid = false;
  bool data_quiet = false;
  bool public_quiet = false;
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
  FillPlan active_fill_plan = FillPlan::Success;
  std::deque<FillPlan> fill_plans;
  bool observe_packet_e = false;
  std::vector<MergeAccept> merge_accepts;
  uint32_t cache_refusal_accepts = 0;
  uint32_t last_cache_refusal_token = 0;
  uint8_t last_cache_refusal_status = 0;
  uint64_t last_cache_refusal_data = 0;
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
      case 0x00001000u:
        return 0xf800u;  // RGB565 red
      case 0x00002000u:
        return 0x0005u;  // CLUT8 raw index 5 in low byte
      case 0x00003000u:
        return 0x001fu;  // RGB565 blue, all bilerp taps
      case 0x00006000u:
        return 0x0005u;  // CLUT4 low nibble index 5
      case 0x00007000u:
        return 0xfc00u;  // ARGB1555 opaque red
      case 0x00008000u:
        return 0x8f10u;  // ARGB4444 A=8, R=F, G=1, B=0
      case 0x0000a000u:
        return 0x0005u;  // Packet-E CLUT native
      case 0x0000c000u:
        return 0xf800u;  // Packet-E NEAR native red
      case 0x0000e000u:
        return 0x001fu;  // Packet-E BIL native blue
      case 0x00012000u:
        return 0x0005u;  // Packet-E CLUT native refill
      case 0x00014000u:
        return 0xf800u;  // Packet-E NEAR native refill
      case 0x00016000u:
        return 0x001fu;  // Packet-E BIL native refill
      default:
        return 0x07e0u;
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
    zhao_texture_packet_b_get_combine_counters(&counters[0], &counters[1], &counters[2],
                                               &counters[3], &counters[4], &counters[5]);
    return counters;
  }

  std::array<uint32_t, 4> hostile_counters() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
    std::array<uint32_t, 4> counters{};
    zhao_texture_packet_b_get_hostile_counters(&counters[0], &counters[1], &counters[2],
                                               &counters[3]);
    return counters;
  }

  void select_dpi_scope() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
  }

  void set_merge_hold(uint32_t mask) {
    select_dpi_scope();
    zhao_texture_packet_e_set_merge_hold(mask);
  }

  void set_cache_class_override(bool enable, unsigned response_class) {
    select_dpi_scope();
    zhao_texture_packet_e_set_cache_class_override(enable ? 1 : 0, response_class);
  }

  MergeState merge_state(unsigned response_class) {
    select_dpi_scope();
    svBit native_valid = 0;
    svBit refusal_valid = 0;
    svBit merged_valid = 0;
    svBit merged_ready = 0;
    svBit selected_refusal = 0;
    svBit rr_refusal = 0;
    svBitVecVal native_tuple[3]{};
    svBitVecVal refusal_tuple[3]{};
    svBitVecVal merged_tuple[3]{};
    zhao_texture_packet_e_get_merge_class_state(
        response_class, &native_valid, &refusal_valid, &merged_valid, &merged_ready,
        &selected_refusal, &rr_refusal, native_tuple, refusal_tuple, merged_tuple);
    MergeState state;
    state.native_valid = native_valid != 0;
    state.refusal_valid = refusal_valid != 0;
    state.merged_valid = merged_valid != 0;
    state.merged_ready = merged_ready != 0;
    state.selected_refusal = selected_refusal != 0;
    state.rr_refusal = rr_refusal != 0;
    for (unsigned word = 0; word < 3; ++word) {
      state.native_tuple.words[word] = native_tuple[word];
      state.refusal_tuple.words[word] = refusal_tuple[word];
      state.merged_tuple.words[word] = merged_tuple[word];
    }
    return state;
  }

  CacheCounters cache_counters() {
    select_dpi_scope();
    CacheCounters counters;
    svBit protocol_fault = 0;
    svBitVecVal reservation_owners = 0;
    svBitVecVal work_state = 0;
    zhao_texture_packet_e_get_cache_counters(
        &counters.cache_accepted, &counters.cache_completed, &counters.fill_accepted,
        &counters.fill_completed, &counters.fill_refused, &counters.fill_beats, &protocol_fault,
        &counters.reservations, &reservation_owners, &work_state);
    counters.protocol_fault = protocol_fault != 0;
    counters.reservation_owners = static_cast<uint8_t>(reservation_owners);
    counters.work_state = static_cast<uint16_t>(work_state);
    return counters;
  }

  std::array<uint32_t, 3> packet_e_protocol_counters() {
    select_dpi_scope();
    std::array<uint32_t, 3> counters{};
    zhao_texture_packet_e_get_protocol_counters(&counters[0], &counters[1], &counters[2]);
    return counters;
  }

  QuietObservation quiet_observation() {
    select_dpi_scope();
    svBit q_dispatch_valid = 0;
    svBit data_is_quiet = 0;
    svBit public_is_quiet = 0;
    zhao_texture_packet_e_get_quiet_observation(&q_dispatch_valid, &data_is_quiet,
                                                &public_is_quiet);
    return QuietObservation{q_dispatch_valid != 0, data_is_quiet != 0, public_is_quiet != 0};
  }

  void observe_packet_e_edges() {
    if (!observe_packet_e) return;
    for (unsigned response_class = 0; response_class < 4; ++response_class) {
      const MergeState state = merge_state(response_class);
      if (state.merged_valid && state.merged_ready)
        merge_accepts.push_back(MergeAccept{response_class, state.selected_refusal,
                                            state.merged_tuple, cycle,
                                            state.native_valid && state.refusal_valid});
    }

    select_dpi_scope();
    svBit valid = 0;
    svBit ready = 0;
    svBitVecVal status = 0;
    svBitVecVal token = 0;
    svBitVecVal data[2]{};
    zhao_texture_packet_e_get_cache_observation(&valid, &ready, &status, &token, data);
    if (valid && ready && status != 0) {
      ++cache_refusal_accepts;
      last_cache_refusal_token = token & 0x3ffffu;
      last_cache_refusal_status = static_cast<uint8_t>(status);
      last_cache_refusal_data =
          static_cast<uint64_t>(data[0]) | (static_cast<uint64_t>(data[1]) << 32);
    }
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
    if (auto_fill) {
      dut.fill_data_valid_i = 0;
      dut.fill_data_i = 0;
      dut.fill_refused_i = 0;
      if (fill_active) {
        switch (active_fill_plan) {
          case FillPlan::Success:
            dut.fill_data_valid_i = 1;
            dut.fill_data_i = fill_word(fill_line, fill_beat);
            break;
          case FillPlan::Refuse:
            dut.fill_refused_i = 1;
            break;
          case FillPlan::PartialRefuse:
            if (fill_beat == 0) {
              dut.fill_data_valid_i = 1;
              dut.fill_data_i = fill_word(fill_line, fill_beat);
            } else {
              dut.fill_refused_i = 1;
            }
            break;
          case FillPlan::SimultaneousRefuse:
            dut.fill_data_valid_i = 1;
            dut.fill_data_i = fill_word(fill_line, fill_beat);
            dut.fill_refused_i = 1;
            break;
        }
      }
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
    observe_packet_e_edges();

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
      for (unsigned word = 0; word < 5; ++word) row.retire[word] = dut.out_retire_ctx_o[word];
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
        switch (active_fill_plan) {
          case FillPlan::Success:
            ++fill_beat;
            if (fill_beat == 8) {
              fill_active = false;
              fill_beat = 0;
            }
            break;
          case FillPlan::Refuse:
          case FillPlan::SimultaneousRefuse:
            fill_active = false;
            fill_beat = 0;
            break;
          case FillPlan::PartialRefuse:
            if (fill_beat == 0) {
              fill_beat = 1;
            } else {
              fill_active = false;
              fill_beat = 0;
            }
            break;
        }
      }
      if (events.fill_req_fire) {
        require(!fill_active, "cache offered overlapping blocking fills", cycle);
        fill_active = true;
        fill_line = accepted_fill_line;
        fill_beat = 0;
        active_fill_plan = fill_plans.empty() ? FillPlan::Success : fill_plans.front();
        if (!fill_plans.empty()) fill_plans.pop_front();
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

void expect_result(const Retired& got, uint32_t rgb, uint8_t alpha, uint8_t index, uint8_t status,
                   uint16_t tag, const std::array<uint32_t, 5>& retire, uint64_t cycle) {
  require(got.rgb == rgb, "retired RGB mismatch", cycle);
  require(got.alpha == alpha, "retired alpha mismatch", cycle);
  if (got.index != index)
    std::fprintf(stderr, "  tag=%04x raw-index expected=%02x got=%02x\n", tag, index, got.index);
  require(got.index == index, "retired raw index mismatch", cycle);
  require(got.status == status, "retired status mismatch", cycle);
  require(got.tag == tag, "legacy tag did not alias frag_ctx[15:0]", cycle);
  require(got.retire == retire, "160-bit retirement context mismatch", cycle);
}

std::array<uint32_t, 5> current_retire(const Harness& h) {
  std::array<uint32_t, 5> value{};
  for (unsigned word = 0; word < 5; ++word) value[word] = h.dut.frag_retire_ctx_i[word];
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

uint8_t binding_command(Harness& h, uint8_t op, uint8_t generation, uint8_t selector,
                        const BindingRow& row, uint32_t crc, bool hold_response = false) {
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
    pulse_palette(h, 1, static_cast<uint16_t>(index), index == 5 ? 0x07e0u : 0u);
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
  rows[8] = BindingRow{0x00009000u, binding_mode(1, false, 0, 0, 3, 3), 0, 0, true};
  rows[9] = BindingRow{0x00006000u, 0x00000002u, 0, 1, true};
  rows[10] = BindingRow{0x00007000u, 0x00000003u, 0, 0, true};
  rows[11] = BindingRow{0x00008000u, 0x00000004u, 0, 0, true};
  rows[12] = BindingRow{0x0000a000u, 0x00000000u, 0, 1, true};
  rows[13] = BindingRow{0x0000b000u, 0x00000000u, 0, 1, true};
  rows[14] = BindingRow{0x0000c000u, 0x00000001u, 0, 0, true};
  rows[15] = BindingRow{0x0000d000u, 0x00000001u, 0, 0, true};
  rows[16] = BindingRow{0x0000e000u, 0x00000009u, 0, 0, true};
  rows[17] = BindingRow{0x0000f000u, 0x00000009u, 0, 0, true};
  rows[18] = BindingRow{0x00010000u, 0x00000001u, 0, 0, true};
  rows[19] = BindingRow{0x00011000u, 0x00000001u, 0, 0, true};
  rows[20] = BindingRow{0x00012000u, 0x00000000u, 0, 1, true};
  rows[21] = BindingRow{0x00013000u, 0x00000000u, 0, 1, true};
  rows[22] = BindingRow{0x00014000u, 0x00000001u, 0, 0, true};
  rows[23] = BindingRow{0x00015000u, 0x00000001u, 0, 0, true};
  rows[24] = BindingRow{0x00016000u, 0x00000009u, 0, 0, true};
  rows[25] = BindingRow{0x00017000u, 0x00000009u, 0, 0, true};
  rows[26] = BindingRow{0x00018000u, 0x00000001u, 0, 0, true};
  rows[27] = BindingRow{0x00019000u, 0x00000001u, 0, 0, true};
  for (unsigned selector = 1; selector <= 27; ++selector) present[selector] = true;
  const uint32_t crc = binding_crc(1, rows, present);
  BindingRow zero{};

  require(binding_command(h, 0, 1, 0, zero, 0, true) == 0, "binding BEGIN failed", h.cycle);
  for (unsigned selector = 1; selector <= 27; ++selector)
    require(binding_command(h, 1, 1, static_cast<uint8_t>(selector), rows[selector], 0) == 0,
            "binding WRITE failed", h.cycle);
  require(binding_command(h, 2, 1, 0, zero, crc) == 0, "binding END/CRC/seal failed", h.cycle);
  require(h.dut.active_page_generation_o == 1,
          "binding seal did not atomically activate generation 1", h.cycle);
}

void check_public_quiet_inputs(Harness& h) {
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;

  h.dut.fill_data_valid_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
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
  h.step();  // accepted by the AUX protocol-fault sink
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
  require(!h.dut.frame_fault_o, "idle clear did not clear unsolicited Sheet frame fault", h.cycle);
}

void set_fragment(Harness& h, uint16_t tag, uint32_t retire_seed, uint8_t count, uint8_t binding,
                  uint8_t recipe, bool aux, uint8_t response_class, uint8_t pal_gen,
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
  struct Case {
    uint32_t mode;
    const char* name;
    bool combine_mismatch;
  };
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
    require(!h.dut.frame_fault_o, "hostile material case did not start recoverably clear", h.cycle);

    const auto before = h.hostile_counters();
    const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
    h.set_material_fault_mode(cases[index].mode);
    set_fragment(h, static_cast<uint16_t>(0x3000u + index), 0xc0000000u + index, 1, 1, 0, false, 1,
                 0);
    const auto retire = current_retire(h);
    h.offer_fragment();
    h.wait_outputs(h.retired.size() + 1);
    expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, static_cast<uint16_t>(0x3000u + index),
                  retire, h.cycle);
    const auto after = h.hostile_counters();
    require(after[0] == before[0] + 1 && after[1] == before[1] + 1 && after[2] == before[2] + 1,
            "hostile material case did not issue/refuse/commit authoritative sample", h.cycle);
    require(after[3] == before[3] + (cases[index].combine_mismatch ? 1u : 0u),
            "hostile material mismatch evidence delta was wrong", h.cycle);
    require(h.dut.cnt_plan_accepted_o == planner_before,
            "hostile material refusal reached planner/cache", h.cycle);
    require(h.dut.frame_fault_o, "hostile material case did not set recoverable frame fault",
            h.cycle);

    h.set_material_fault_mode(0);
    h.wait_quiet();
    h.dut.frame_fault_clear_valid_i = 1;
    h.step();
    h.dut.frame_fault_clear_valid_i = 0;
    h.step();
    require(!h.dut.frame_fault_o, "hostile material case incorrectly entered reset lifetime",
            h.cycle);
  }

  // Empty owner masks become combine-ready before any sample/AUX commit. A bad
  // descriptor must therefore be fenced until joined validation and converted
  // directly into a canonical loud/refused combine ticket; there is no source
  // plane on which the expander could carry this refusal.
  h.wait_quiet();
  const auto before = h.hostile_counters();
  const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
  h.set_material_fault_mode(1);
  set_fragment(h, 0x30f0, 0xc00000f0u, 0, 0, 0, false, 0, 0, 0x2468acu, 0x5du);
  const auto retire = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x30f0, retire, h.cycle);
  const auto after = h.hostile_counters();
  require(after[0] == before[0] && after[1] == before[1] && after[2] == before[2] &&
              after[3] == before[3] + 1,
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
  require(!h.dut.frame_fault_o, "count-zero descriptor refusal incorrectly entered reset lifetime",
          h.cycle);
}

uint32_t wait_for_held_fill_request(Harness& h, unsigned stall_cycles = 16) {
  for (unsigned n = 0; n < 20000 && !h.dut.fill_req_valid_o; ++n) h.step();
  require(h.dut.fill_req_valid_o && !h.dut.fill_req_ready_i,
          "refusal control did not establish held fill request", h.cycle);
  const uint32_t address = h.dut.fill_req_addr_o;
  for (unsigned n = 0; n < stall_cycles; ++n) {
    h.step();
    require(h.dut.fill_req_valid_o && h.dut.fill_req_addr_o == address,
            "Packet-E fill request changed during long stall", h.cycle);
  }
  return address;
}

void clear_recoverable_fault(Harness& h) {
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o, "Packet-E recoverable refusal/fill fault did not clear", h.cycle);
}

void expect_tuple(const Tuple66& tuple, unsigned response_class, uint32_t rgb, uint8_t alpha,
                  uint8_t index, uint8_t status, const char* what, uint64_t cycle) {
  if ((tuple.token() >> 16) != response_class || tuple.rgb() != rgb || tuple.alpha() != alpha ||
      tuple.index() != index || tuple.status() != status) {
    std::fprintf(stderr,
                 "  %s class=%u token=%05x status=%02x index=%02x "
                 "alpha=%02x rgb=%06x\n",
                 what, response_class, tuple.token(), tuple.status(), tuple.index(), tuple.alpha(),
                 tuple.rgb());
    fail("Packet-E 66-bit terminal tuple mismatch", cycle);
  }
}

bool run_packet_e_refusal_controls(Harness& h) {
  constexpr unsigned CLS_CLUT = 0;
  constexpr unsigned CLS_NEAR = 1;
  constexpr unsigned CLS_BIL = 2;
  constexpr unsigned CLS_ERR = 3;

#if defined(PACKET_E_EXPECT_PRE_E_FILL_LIFETIME)
  const auto before = h.cache_counters();
  h.observe_packet_e = true;
  h.fill_plans.push_back(FillPlan::Refuse);
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x6f01, 0xe0000f01u, 1, 13, 0, false, CLS_CLUT, 1);
  h.offer_fragment();
  wait_for_held_fill_request(h);
  h.dut.fill_req_ready_i = 1;
  h.step();  // FI
  h.step();  // presented denial is withheld from the cache by the mutant
  for (unsigned n = 0; n < 64; ++n) h.step();
  const auto after = h.cache_counters();
  require(h.dut.frame_fault_o && !h.dut.frag_ready_o,
          "historical pre-E fill refusal did not latch/reset-block", h.cycle);
  require(h.retired.empty() && after.fill_accepted == before.fill_accepted + 1 &&
              after.fill_completed == before.fill_completed &&
              after.fill_refused == before.fill_refused,
          "historical pre-E fill refusal produced a typed completion", h.cycle);
  h.dut.frame_fault_clear_valid_i = 1;
  for (unsigned n = 0; n < 4; ++n) h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  require(h.dut.frame_fault_o && !h.dut.frame_fault_clear_ready_o,
          "historical pre-E reset-lifetime fault yielded to frame clear", h.cycle);
  std::printf("packet-e historical-pre-E-fill-lifetime mutant FIRED\n");
  return true;
#elif defined(PACKET_E_EXPECT_RELABEL_REFUSAL_ERR)
  const auto protocol_before = h.packet_e_protocol_counters();
  const uint32_t dispatch_before = h.dut.cnt_dispatch_accepted_o;
  h.observe_packet_e = true;
  h.fill_plans.push_back(FillPlan::Refuse);
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x6f02, 0xe0000f02u, 1, 13, 0, false, CLS_CLUT, 1);
  const auto retire = current_retire(h);
  h.offer_fragment();
  wait_for_held_fill_request(h);
  h.dut.fill_req_ready_i = 1;
  h.step();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x6f02, retire, h.cycle);
  require(!h.merge_accepts.empty(), "relabel mutant produced no merge acceptance", h.cycle);
  const auto& accepted = h.merge_accepts.back();
  require(accepted.response_class == CLS_ERR && accepted.refusal &&
              (accepted.tuple.token() >> 16) == CLS_CLUT &&
              h.dut.cnt_dispatch_accepted_o == dispatch_before + 1,
          "relabel mutant did not route original CLUT token through physical ERR", h.cycle);
  const auto protocol_after = h.packet_e_protocol_counters();
  require(protocol_after[0] == protocol_before[0] + 1 && protocol_after[1] == protocol_before[1] &&
              protocol_after[2] == protocol_before[2] && !h.cache_counters().protocol_fault,
          "relabel mutant did not fire only the dispatcher mismatch detector", h.cycle);
  std::printf("packet-e refusal-relabel-to-ERR mutant FIRED\n");
  return true;
#elif defined(PACKET_E_EXPECT_REFUSAL_NATIVE_ARITH)
  const uint32_t metadata_before = h.dut.meta_shadow_reads_o;
  const uint32_t near_work_before = h.dut.cnt_near_refused_o;
  h.fill_plans.push_back(FillPlan::Refuse);
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x6f03, 0xe0000f03u, 1, 15, 0, false, CLS_NEAR, 0);
  const auto retire = current_retire(h);
  h.offer_fragment();
  wait_for_held_fill_request(h);
  h.dut.fill_req_ready_i = 1;
  h.step();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0x000000u, 0xff, 0, 0, 0x6f03, retire, h.cycle);
  require(h.dut.meta_shadow_reads_o == metadata_before + 1 &&
              h.dut.cnt_near_refused_o == near_work_before && !h.dut.frame_fault_o &&
              !h.cache_counters().protocol_fault,
          "native-arithmetic mutant did not consume refusal as clean metadata/NEAR", h.cycle);
  std::printf("packet-e refusal-enters-native-arithmetic mutant FIRED\n");
  return true;
#elif defined(PACKET_E_EXPECT_DROP_HELD_REFUSAL)
  h.observe_packet_e = true;
  h.fill_plans.push_back(FillPlan::Refuse);
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x6f04, 0xe0000f04u, 1, 13, 0, false, CLS_CLUT, 1);
  h.offer_fragment();
  wait_for_held_fill_request(h);
  h.dut.fill_req_ready_i = 1;
  h.step();
  for (unsigned n = 0; n < 20000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    const MergeState state = h.merge_state(CLS_CLUT);
    if (state.refusal_valid) {
      require(!state.merged_valid && h.retired.empty(),
              "dropped-refusal mutant unexpectedly offered/retired denial", h.cycle);
      for (unsigned stall = 0; stall < 64; ++stall) h.step();
      require(h.merge_state(CLS_CLUT).refusal_valid && !h.merge_state(CLS_CLUT).merged_valid &&
                  !h.dut.quiet_o,
              "dropped-refusal mutant did not leave exact held refusal stranded", h.cycle);
      std::printf("packet-e dropped-held-refusal mutant FIRED\n");
      return true;
    }
    h.step();
  }
  fail("dropped-refusal mutant never captured cache denial", h.cycle);
#else
  struct RefusalCase {
    unsigned response_class;
    uint8_t native_binding0;
    uint8_t refusal_binding0;
    uint8_t native_binding1;
    uint8_t refusal_binding1;
    uint8_t native_witness_class;
    uint8_t refusal_witness_class;
    uint8_t native_pal_gen;
    uint8_t refusal_pal_gen;
    uint32_t native_rgb;
    uint8_t native_index;
  };
  const std::array<RefusalCase, 4> cases{{
      {CLS_CLUT, 12, 13, 20, 21, CLS_CLUT, CLS_CLUT, 1, 1, 0x00ff00u, 5},
      {CLS_NEAR, 14, 15, 22, 23, CLS_NEAR, CLS_NEAR, 0, 0, 0xff0000u, 0},
      {CLS_BIL, 16, 17, 24, 25, CLS_BIL, CLS_BIL, 0, 0, 0x0000ffu, 0},
      // ERR natives are resolver-local terminals from absent bindings. Cache
      // refusals use legal NEAR bindings whose accepted cache tokens are
      // test-only class-forced to ERR; their original returned tokens are ERR.
      {CLS_ERR, 250, 18, 251, 26, CLS_ERR, CLS_NEAR, 0, 0, 0xff00ffu, 0},
  }};

  const CacheCounters cache_begin = h.cache_counters();
  const std::size_t retirement_begin = h.retired.size();
  h.observe_packet_e = true;

  for (unsigned case_index = 0; case_index < cases.size(); ++case_index) {
    const RefusalCase& item = cases[case_index];
    const uint16_t tag0 = static_cast<uint16_t>(0x6000u + case_index * 4u);
    const uint16_t tag1 = static_cast<uint16_t>(tag0 + 1u);
    const uint16_t tag2 = static_cast<uint16_t>(tag0 + 2u);
    const uint16_t tag3 = static_cast<uint16_t>(tag0 + 3u);
    std::array<std::array<uint32_t, 5>, 4> retire{};
    h.dut.out_ready_i = 0;
    h.set_merge_hold(1u << item.response_class);
    h.set_cache_class_override(false, 0);

    // N0: establish the held native terminal.
    set_fragment(h, tag0, 0xe0000000u + case_index * 4u, 1, item.native_binding0, 0, false,
                 item.native_witness_class, item.native_pal_gen);
    retire[0] = current_retire(h);
    h.offer_fragment();
    for (unsigned n = 0; n < 20000; ++n) {
      h.dut.clk = 0;
      h.dut.eval();
      if (h.merge_state(item.response_class).native_valid) break;
      h.step();
    }
    h.dut.clk = 0;
    h.dut.eval();
    MergeState held = h.merge_state(item.response_class);
    require(held.native_valid && !held.refusal_valid, "N0 did not reach held native merge input",
            h.cycle);
    expect_tuple(held.native_tuple, item.response_class, item.native_rgb, 0xff, item.native_index,
                 item.response_class == CLS_ERR ? 1 : 0, "native N0", h.cycle);
    const Tuple66 native_tuple0 = held.native_tuple;

    // R0: establish the held refusal terminal beside N0.
    h.fill_plans.push_back(FillPlan::Refuse);
    h.dut.fill_req_ready_i = 0;
    if (item.response_class == CLS_ERR) h.set_cache_class_override(true, CLS_ERR);
    set_fragment(h, tag1, 0xe0000001u + case_index * 4u, 1, item.refusal_binding0, 0, false,
                 item.refusal_witness_class, item.refusal_pal_gen);
    retire[1] = current_retire(h);
    h.offer_fragment();
    wait_for_held_fill_request(h);
    h.dut.fill_req_ready_i = 1;
    h.step();  // FI; refusal is driven on the next edge.
    const uint32_t metadata_r0 = h.dut.meta_shadow_reads_o;
    const uint32_t palette_r0 = h.dut.cnt_palette_lookups_o;
    const uint32_t bilerp_r0 = h.dut.cnt_bilerp_jobs_o;
    const uint32_t mosaic_r0 = h.dut.cnt_mosaic_samples_o;

    for (unsigned n = 0; n < 20000; ++n) {
      h.dut.clk = 0;
      h.dut.eval();
      held = h.merge_state(item.response_class);
      if (held.native_valid && held.refusal_valid) break;
      h.step();
    }
    h.dut.clk = 0;
    h.dut.eval();
    held = h.merge_state(item.response_class);
    require(held.native_valid && held.refusal_valid && !held.selected_refusal && !held.rr_refusal,
            "N0/R0 contention did not retain initial native priority", h.cycle);
    require(h.cache_refusal_accepts == case_index * 2u + 1u,
            "cache did not capture exact first refusal for class", h.cycle);
    expect_tuple(held.refusal_tuple, item.response_class, 0xff00ffu, 0xff, 0, 1, "refusal R0",
                 h.cycle);
    const Tuple66 refusal_tuple0 = held.refusal_tuple;
    require(refusal_tuple0.token() == h.last_cache_refusal_token &&
                h.last_cache_refusal_status == 1 && h.last_cache_refusal_data == 0 &&
                refusal_tuple0.token() != native_tuple0.token(),
            "R0 did not preserve distinct original cache token/status/data", h.cycle);
    require(h.dut.meta_shadow_reads_o == metadata_r0 && h.dut.cnt_palette_lookups_o == palette_r0 &&
                h.dut.cnt_bilerp_jobs_o == bilerp_r0 && h.dut.cnt_mosaic_samples_o == mosaic_r0,
            "R0 launched metadata/palette/bilerp/Mosaic work", h.cycle);

    // Exact quiet-source mapping positive control. Every public input offer is
    // parked, but structural merged occupancy must remain q_dispatch=1 and hold
    // both data_quiet and public quiet low even though the dispatcher valid is
    // test-gated off.
    const QuietObservation quiet_held = h.quiet_observation();
    require(quiet_held.q_dispatch_valid && !quiet_held.data_quiet && !quiet_held.public_quiet &&
                !h.dut.frag_valid_i && !h.dut.cfg_valid_i && !h.dut.pal_load_valid_i &&
                !h.dut.fill_data_valid_i && !h.dut.fill_refused_i && !h.dut.pg_valid_i &&
                !h.dut.frame_fault_clear_valid_i,
            "held merge occupancy disappeared from q_dispatch/data_quiet mapping", h.cycle);

    for (unsigned stall = 0; stall < 32; ++stall) {
      h.step();
      const MergeState stalled = h.merge_state(item.response_class);
      const QuietObservation quiet_stalled = h.quiet_observation();
      require(stalled.native_valid && stalled.refusal_valid && !stalled.selected_refusal &&
                  !stalled.rr_refusal && stalled.native_tuple == native_tuple0 &&
                  stalled.refusal_tuple == refusal_tuple0 && quiet_stalled.q_dispatch_valid &&
                  !quiet_stalled.data_quiet,
              "N0/R0 records or structural quiet visibility changed while held", h.cycle);
    }

    // Stage one native refill and one refusal reload. ERR must send R1 through
    // its planner before parking local N1 in the single resolver disposition;
    // every other class stages N1 first so it is ahead of R1 in cache order.
    auto stage_native_refill = [&]() {
      h.set_cache_class_override(false, 0);
      const CacheCounters before_n1 = h.cache_counters();
      set_fragment(h, tag2, 0xe0000002u + case_index * 4u, 1, item.native_binding1, 0, false,
                   item.native_witness_class, item.native_pal_gen);
      retire[2] = current_retire(h);
      h.offer_fragment();
      if (item.response_class != CLS_ERR) {
        for (unsigned n = 0;
             n < 20000 && h.cache_counters().fill_completed == before_n1.fill_completed; ++n)
          h.step();
        require(h.cache_counters().fill_completed == before_n1.fill_completed + 1,
                "N1 successful fill did not complete", h.cycle);
      }
      for (unsigned n = 0; n < 96; ++n) h.step();
      held = h.merge_state(item.response_class);
      require(held.native_valid && held.refusal_valid && held.native_tuple == native_tuple0 &&
                  held.refusal_tuple == refusal_tuple0,
              "preloaded N1 overwrote held N0/R0", h.cycle);
    };

    auto stage_refusal_reload = [&]() {
      const CacheCounters before_r1 = h.cache_counters();
      h.fill_plans.push_back(FillPlan::Refuse);
      h.dut.fill_req_ready_i = 0;
      if (item.response_class == CLS_ERR) h.set_cache_class_override(true, CLS_ERR);
      set_fragment(h, tag3, 0xe0000003u + case_index * 4u, 1, item.refusal_binding1, 0, false,
                   item.refusal_witness_class, item.refusal_pal_gen);
      retire[3] = current_retire(h);
      h.offer_fragment();
      wait_for_held_fill_request(h);
      h.dut.fill_req_ready_i = 1;
      h.step();  // FI
      const uint32_t metadata_r1 = h.dut.meta_shadow_reads_o;
      const uint32_t palette_r1 = h.dut.cnt_palette_lookups_o;
      const uint32_t bilerp_r1 = h.dut.cnt_bilerp_jobs_o;
      const uint32_t mosaic_r1 = h.dut.cnt_mosaic_samples_o;
      for (unsigned n = 0;
           n < 20000 && h.cache_counters().fill_completed == before_r1.fill_completed; ++n)
        h.step();
      require(h.cache_counters().fill_completed == before_r1.fill_completed + 1,
              "R1 refused fill did not terminate", h.cycle);
      h.step();  // park external offers while R1 remains backpressured.
      require(h.cache_refusal_accepts == case_index * 2u + 1u &&
                  h.dut.meta_shadow_reads_o == metadata_r1 &&
                  h.dut.cnt_palette_lookups_o == palette_r1 &&
                  h.dut.cnt_bilerp_jobs_o == bilerp_r1 && h.dut.cnt_mosaic_samples_o == mosaic_r1,
              "backpressured R1 escaped slot or launched native arithmetic", h.cycle);
    };

    if (item.response_class == CLS_ERR) {
      stage_refusal_reload();
      stage_native_refill();
    } else {
      stage_native_refill();
      stage_refusal_reload();
    }

    // Release sustained contention. N0 and R0 must be accepted on consecutive
    // contended clocks in opposite directions. N1 refills native on N0's edge;
    // R1 refills refusal on R0's edge; N1 is therefore a third consecutive
    // contended acceptance. R1 then drains without a bubble or overwrite.
    const std::size_t merge_begin = h.merge_accepts.size();
    const uint32_t refusal_accepts_before_reload = h.cache_refusal_accepts;
    h.set_merge_hold(0);

    h.step();  // accept N0, simultaneously reload native with N1
    require(h.merge_accepts.size() == merge_begin + 1,
            "N0 was not accepted on first released clock", h.cycle);
    h.dut.clk = 0;
    h.dut.eval();
    const MergeState after_n0 = h.merge_state(item.response_class);
    require(after_n0.native_valid && after_n0.refusal_valid && after_n0.selected_refusal &&
                after_n0.rr_refusal && after_n0.refusal_tuple == refusal_tuple0,
            "N1 did not same-edge refill native behind contended N0", h.cycle);
    expect_tuple(after_n0.native_tuple, item.response_class, item.native_rgb, 0xff,
                 item.native_index, item.response_class == CLS_ERR ? 1 : 0, "native N1", h.cycle);
    const Tuple66 native_tuple1 = after_n0.native_tuple;
    require(native_tuple1.token() != native_tuple0.token() &&
                native_tuple1.token() != refusal_tuple0.token(),
            "N1 token was not distinct", h.cycle);

    h.step();  // accept R0, simultaneously pop/reload refusal with R1
    require(h.merge_accepts.size() == merge_begin + 2,
            "R0 was not accepted on second consecutive released clock", h.cycle);
    h.dut.clk = 0;
    h.dut.eval();
    const MergeState after_r0 = h.merge_state(item.response_class);
    require(after_r0.native_valid && after_r0.refusal_valid && !after_r0.selected_refusal &&
                !after_r0.rr_refusal &&
                h.cache_refusal_accepts == refusal_accepts_before_reload + 1,
            "R1 did not same-edge pop/reload refusal behind contended R0", h.cycle);
    expect_tuple(after_r0.refusal_tuple, item.response_class, 0xff00ffu, 0xff, 0, 1, "refusal R1",
                 h.cycle);
    const Tuple66 refusal_tuple1 = after_r0.refusal_tuple;
    require(refusal_tuple1.token() == h.last_cache_refusal_token &&
                refusal_tuple1.token() != refusal_tuple0.token() &&
                refusal_tuple1.token() != native_tuple1.token(),
            "R1 did not preserve a distinct original cache token", h.cycle);

    h.step();  // accept N1 under the reloaded N1/R1 contention
    require(h.merge_accepts.size() == merge_begin + 3,
            "N1 was not the third consecutive contended acceptance", h.cycle);
    h.step();  // drain R1 without a bubble
    require(h.merge_accepts.size() == merge_begin + 4,
            "R1 did not drain on the fourth consecutive clock", h.cycle);

    const MergeAccept& accept0 = h.merge_accepts[merge_begin + 0];
    const MergeAccept& accept1 = h.merge_accepts[merge_begin + 1];
    const MergeAccept& accept2 = h.merge_accepts[merge_begin + 2];
    const MergeAccept& accept3 = h.merge_accepts[merge_begin + 3];
    require(accept0.response_class == item.response_class &&
                accept1.response_class == item.response_class &&
                accept2.response_class == item.response_class &&
                accept3.response_class == item.response_class && accept0.tuple == native_tuple0 &&
                !accept0.refusal && accept1.tuple == refusal_tuple0 && accept1.refusal &&
                accept2.tuple == native_tuple1 && !accept2.refusal &&
                accept3.tuple == refusal_tuple1 && accept3.refusal,
            "sustained RR changed/relabelled an exact 66-bit record", h.cycle);
    require(accept0.contended && accept1.contended && accept2.contended && !accept3.contended &&
                accept1.cycle == accept0.cycle + 1 && accept2.cycle == accept1.cycle + 1 &&
                accept3.cycle == accept2.cycle + 1,
            "sustained RR did not alternate both directions without bubbles", h.cycle);
    require(h.merge_state(item.response_class).rr_refusal,
            "uncontended R1 acceptance illegally changed RR", h.cycle);

    for (unsigned stall = 0; stall < 48; ++stall) h.step();
    require(h.retired.size() == retirement_begin + case_index * 4,
            "ordered owner output escaped long output stall", h.cycle);
    h.dut.out_ready_i = 1;
    h.wait_outputs(retirement_begin + (case_index + 1) * 4);
    expect_result(h.retired[retirement_begin + case_index * 4 + 0], item.native_rgb, 0xff,
                  item.native_index, item.response_class == CLS_ERR ? 1 : 0, tag0, retire[0],
                  h.cycle);
    expect_result(h.retired[retirement_begin + case_index * 4 + 1], 0xff00ffu, 0xff, 0, 1, tag1,
                  retire[1], h.cycle);
    if (item.response_class == CLS_ERR) {
      // R1 was admitted before local N1 to clear the single resolver disposition.
      // The owner must restore admission order even though merge order was N1,R1.
      expect_result(h.retired[retirement_begin + case_index * 4 + 2], 0xff00ffu, 0xff, 0, 1, tag3,
                    retire[3], h.cycle);
      expect_result(h.retired[retirement_begin + case_index * 4 + 3], item.native_rgb, 0xff,
                    item.native_index, 1, tag2, retire[2], h.cycle);
    } else {
      expect_result(h.retired[retirement_begin + case_index * 4 + 2], item.native_rgb, 0xff,
                    item.native_index, 0, tag2, retire[2], h.cycle);
      expect_result(h.retired[retirement_begin + case_index * 4 + 3], 0xff00ffu, 0xff, 0, 1, tag3,
                    retire[3], h.cycle);
    }
    h.set_cache_class_override(false, 0);
    require(h.dut.frame_fault_o, "typed cache refusals did not set recoverable frame fault",
            h.cycle);
    clear_recoverable_fault(h);
  }

  const CacheCounters cache_end = h.cache_counters();
  require(cache_end.cache_accepted - cache_begin.cache_accepted == 14 &&
              cache_end.cache_completed - cache_begin.cache_completed == 14,
          "Packet-E cache CA/CC deltas were not 14/14", h.cycle);
  require(cache_end.fill_accepted - cache_begin.fill_accepted == 14 &&
              cache_end.fill_completed - cache_begin.fill_completed == 14 &&
              cache_end.fill_refused - cache_begin.fill_refused == 8 &&
              cache_end.fill_beats - cache_begin.fill_beats == 48,
          "Packet-E FI/FTERM/FREF/FB deltas violated 14/14/8/48", h.cycle);
  const auto protocol_end = h.packet_e_protocol_counters();
  require(!cache_end.protocol_fault && cache_end.reservations == 0 &&
              cache_end.reservation_owners == 0 && cache_end.work_state == 0 &&
              protocol_end[0] == 0 && protocol_end[1] == 0 && protocol_end[2] == 0,
          "Packet-E sustained refusal drain left protocol/credit/work evidence", h.cycle);
  return false;
#endif
}

void run_cache_protocol_fault_controls(Harness& h) {
  // Unsolicited data is malformed. It must set the child cache fault, surface at
  // the top, and clear without reset once every accepted owner is quiet.
  clear_recoverable_fault(h);
  h.auto_fill = false;
  h.dut.fill_data_valid_i = 1;
  h.dut.fill_data_i = 0x55aau;
  h.step();
  h.dut.fill_data_valid_i = 0;
  h.step();
  require(h.cache_counters().protocol_fault && h.dut.frame_fault_o,
          "unsolicited cache fill beat did not set child/top protocol fault", h.cycle);
  h.auto_fill = true;
  clear_recoverable_fault(h);
  require(!h.cache_counters().protocol_fault,
          "quiet frame clear did not clear cache protocol fault", h.cycle);

  // Partial refusal is both terminal and malformed: one legal beat, then denial.
  const CacheCounters before = h.cache_counters();
  h.fill_plans.push_back(FillPlan::PartialRefuse);
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x6e01, 0xe1000001u, 1, 19, 0, false, 1, 0);
  const auto retire = current_retire(h);
  h.offer_fragment();
  wait_for_held_fill_request(h, 4);
  h.dut.fill_req_ready_i = 1;
  h.step();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x6e01, retire, h.cycle);
  const CacheCounters after = h.cache_counters();
  require(after.fill_accepted == before.fill_accepted + 1 &&
              after.fill_completed == before.fill_completed + 1 &&
              after.fill_refused == before.fill_refused + 1 &&
              after.fill_beats == before.fill_beats + 1 && after.protocol_fault &&
              h.dut.frame_fault_o,
          "partial refusal did not terminate once and set malformed fault", h.cycle);
  clear_recoverable_fault(h);
}

void run_directed() {
  Harness* const harness = new Harness;
  Harness& h = *harness;
  h.reset();
  require(h.dut.quiet_o, "island did not reset to public quiet", h.cycle);
  check_public_quiet_inputs(h);
  program_palette(h);
  program_bindings(h);
#if !defined(PACKET_B_EXPECT_INDEX_ROUTE_MUTANT) && !defined(PACKET_B_EXPECT_AUX_AS_SAMPLE2) && \
    !defined(PACKET_B_EXPECT_RETIRE_TRUNCATION) &&                                              \
    !defined(PACKET_B_EXPECT_OWNER_MASK_LIFETIME) &&                                            \
    !defined(PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME) &&                                           \
    !defined(PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL) &&                                        \
    !defined(PACKET_B_EXPECT_SHADOW_CORRUPTION) && !defined(PACKET_B_EXPECT_RSP_DROP_OBSERVATION)
  if (run_packet_e_refusal_controls(h)) return;
#if !defined(PACKET_E_EXPECT_PRE_E_FILL_LIFETIME) && \
    !defined(PACKET_E_EXPECT_RELABEL_REFUSAL_ERR) && \
    !defined(PACKET_E_EXPECT_REFUSAL_NATIVE_ARITH) && !defined(PACKET_E_EXPECT_DROP_HELD_REFUSAL)
  run_cache_protocol_fault_controls(h);
#endif
#endif
#if !defined(PACKET_B_EXPECT_INDEX_ROUTE_MUTANT) && !defined(PACKET_B_EXPECT_AUX_AS_SAMPLE2) && \
    !defined(PACKET_B_EXPECT_RETIRE_TRUNCATION) &&                                              \
    !defined(PACKET_B_EXPECT_OWNER_MASK_LIFETIME) &&                                            \
    !defined(PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME) &&                                           \
    !defined(PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL) &&                                        \
    !defined(PACKET_B_EXPECT_SHADOW_CORRUPTION)
  run_material_hostile_cases(h);
#endif

  // R9: TEXTURE.TMU's `texture_samples` is owned by zhao_texture_v3own and counts
  // filtered samples PUBLISHED into a fragment. These three fragments run alone
  // and in order, so the count is exact: +0 for count-zero PASSTHRU (no TMU
  // source), +1 for NEAR, +1 for CLUT. A counter that moved on the PASSTHRU
  // fragment would be counting requests or fragments, not delivered samples.
  const uint32_t r9_samples0 = h.dut.cnt_texture_samples_o;
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
              h.retired.back().retire[1] == count0_ctx[1] && h.retired.back().retire[2] == 0 &&
              h.retired.back().retire[3] == 0 && h.retired.back().retire[4] == 0,
          "retirement truncation mutant did not zero upper 96 bits", h.cycle);
  std::printf("packet-b retire-context-truncation mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x123456u, 0x7a, 0, 0, 0x1001, count0_ctx, h.cycle);
  require(h.dut.cnt_texture_samples_o == r9_samples0,
          "R9 texture_samples moved on a count-zero PASSTHRU fragment", h.cycle);

  // NEAR tuple: direct RGB565 decode, opaque alpha, raw index zero.
  set_fragment(h, 0x1002, 0xa0000002u, 1, 1, 0, false, 1, 0);
  const auto near_ctx = current_retire(h);
  h.offer_fragment();
#ifdef PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME
  const std::size_t before_illegal_cache = h.retired.size();
  for (unsigned n = 0; n < 5000 && !h.dut.frame_fault_o; ++n) h.step();
  require(h.dut.frame_fault_o && !h.dut.frag_ready_o && h.retired.size() == before_illegal_cache,
          "cache sample-3 mutant did not consume/drop into reset lifetime", h.cycle);
  require(h.dut.meta_shadow_reads_o == 0, "cache sample-3 mutant launched metadata read", h.cycle);
  std::printf("packet-b cache-sidx3 mutant FIRED\n");
  return;
#endif
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_SHADOW_CORRUPTION
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1002, near_ctx, h.cycle);
  require(h.dut.shadow_present_o && h.dut.meta_shadow_reads_o == 1 &&
              h.dut.meta_shadow_mismatch_o == 1 && h.dut.meta_near_chk_o == 1 &&
              h.dut.meta_near_err_o == 1 && h.dut.meta_align_chk_o == 0 &&
              h.dut.meta_bil_chk_o == 0,
          "one-sided shadow corruption did not fire exact NEAR counters", h.cycle);
  std::printf("packet-b one-sided-shadow mutant FIRED\n");
  return;
#endif
#ifdef PACKET_B_EXPECT_INDEX_ROUTE_MUTANT
  require(h.retired.back().rgb == 0xff0000u && h.retired.back().index != 0 &&
              h.retired.back().status == 0,
          "dispatcher/index-route mutant did not corrupt first TMU raw index", h.cycle);
  std::printf("packet-b dispatcher-index-route mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1002, near_ctx, h.cycle);
  require(h.dut.cnt_texture_samples_o == r9_samples0 + 1u,
          "R9 texture_samples did not count the NEAR fragment's one delivered sample", h.cycle);
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
  require(!h.dut.err_rsp_dropped_o, "healthy cache response path fired dropped-response detector",
          h.cycle);
#endif

  // CLUT tuple: addressed raw index survives palette latency unchanged.
  set_fragment(h, 0x1003, 0xa0000003u, 1, 2, 0, false, 0, 1);
  const auto clut_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_INDEX_ROUTE_MUTANT
  require(h.retired.back().rgb == 0x00ff00u && h.retired.back().index != 5 &&
              h.retired.back().status == 0,
          "dispatcher/index-route mutant did not corrupt only raw index", h.cycle);
  std::printf("packet-b dispatcher-index-route mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x00ff00u, 0xff, 5, 0, 0x1003, clut_ctx, h.cycle);
  require(h.dut.cnt_texture_samples_o == r9_samples0 + 2u,
          "R9 texture_samples did not count the CLUT fragment's one delivered sample", h.cycle);
  std::printf("packet-b R9 texture_samples: +0 passthru, +1 near, +1 clut (now %u)\n",
              static_cast<unsigned>(h.dut.cnt_texture_samples_o));

  // Selected-top format seams removed with the historical composed driver.
  // Exercise CLUT4 nibble selection and both direct alpha formats here instead
  // of relying on leaf decode tests that cannot see top-level wiring.
  set_fragment(h, 0x1013, 0xa0000013u, 1, 9, 0, false, 0, 1);
  const auto clut4_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0x00ff00u, 0xff, 5, 0, 0x1013, clut4_ctx, h.cycle);

  set_fragment(h, 0x1014, 0xa0000014u, 1, 10, 0, false, 1, 0);
  const auto argb1555_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff0000u, 0xff, 0, 0, 0x1014, argb1555_ctx, h.cycle);

  set_fragment(h, 0x1015, 0xa0000015u, 1, 11, 0, false, 1, 0);
  const auto argb4444_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff1100u, 0x88, 0, 0, 0x1015, argb4444_ctx, h.cycle);

  // Ingress class 3 is invalid rather than an ordinary raw ERR request. The
  // detector must fire, the owner must retire loudly, and legal traffic must
  // remain possible after the recoverable clear.
  const uint32_t invalid_class_before = h.dut.err_class_invalid_o;
  set_fragment(h, 0x1016, 0xa0000016u, 1, 1, 0, false, 3, 0);
  const auto invalid_class_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1016, invalid_class_ctx, h.cycle);
  require(h.dut.err_class_invalid_o == invalid_class_before + 1 && h.dut.frame_fault_o,
          "ingress class-3 detector did not fire exactly once", h.cycle);
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o, "recoverable ingress class-3 fault did not clear", h.cycle);

  // BIL tuple: four physical lanes produce RGBA; equal taps stay exact.
  const uint32_t bilerp_jobs_before = h.dut.cnt_bilerp_jobs_o;
  set_fragment(h, 0x1004, 0xa0000004u, 1, 3, 0, false, 2, 0);
  const auto bil_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1004, bil_ctx, h.cycle);
  require(h.dut.frame_fault_o, "bilerp identity mutant did not set recoverable frame fault",
          h.cycle);
  require(h.dut.cnt_bilerp_jobs_o == bilerp_jobs_before + 4,
          "bilerp identity mutant changed physical job count", h.cycle);
  std::printf("packet-b bilerp-identity mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x0000ffu, 0xff, 0, 0, 0x1004, bil_ctx, h.cycle);
  require(h.dut.cnt_bilerp_jobs_o == bilerp_jobs_before + 4,
          "BIL response did not execute exactly four RGBA lane jobs", h.cycle);

  // Warm three non-conflicting direct-cache lines, then require the metadata
  // join to sustain the TMU's one-response-per-two-clocks hot cadence. The old
  // capture-then-hold path produced gaps of three even with every line resident.
  set_fragment(h, 0x1040, 0xa0000040u, 3, 5, 6, false, 1, 0);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  require(h.retired.back().status == 0, "metadata cadence warm-up fragment refused", h.cycle);
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
  require(h.retired.back().status == 0, "hot metadata cadence fragment refused", h.cycle);

  // AUX HIT is independently owed and cannot become sample 2.
  //
  // "... OR ALTER RGBA" WAS THE REST OF THIS SENTENCE AND IT IS SUPERSEDED,
  // 2026-09-25 (TERRAINAUX), by `zhao_texture_sheetmod` -- the visible
  // terrain-effect composition `TEXTURE.AUX.V2.md` and `TEXTURE.COMBINE.md`
  // both said was "reserved for later" and "not claimed connected".
  // `design/contracts/TEXTURE.SHEETMOD.md` is the decision record.
  //
  // WHAT IS SUPERSEDED IS EXACTLY ONE CLAUSE. AUX still never becomes sample
  // 2 (the case below still proves it, and its mutant control still fires),
  // no RECIPE consumes tag or strength, and the tag byte is still consumed by
  // nothing. What changed is that a fragment whose material DECLARES AUX now
  // has charter section 12's sheet tint applied to the colour the recipe
  // produced -- `rgb * (255 - strength/2) / 256`, the oracle's own law.
  //
  // SO THIS EXPECTATION IS NOW THE PROOF THAT THE EFFECT IS CONNECTED, and it
  // is written as an arithmetic statement rather than a constant so it cannot
  // be satisfied by accident: strength 0x44 gives tint 255 - 0x22 = 221, and
  // 0xff becomes (255*221 + 128) >> 8 = 0xdc. Red 0xff0000 -> 0xdc0000; the
  // zero channels stay zero, which is the same law and a different witness.
  //
  // The two cases after this one are its controls and BOTH still expect the
  // untinted value, for two different reasons: the DETAIL_MASK fragment's RGB
  // is 0x000000 and a tint of zero is zero, and the AUX MISS fragment's status
  // is non-zero so the tint arm is refused outright -- `TEXTURE.COMBINE.md`
  // fixes a faulted result at EXACTLY 0xff00ff and a tinted magenta would be a
  // quieter colour.
  h.sheet_plans.push_back(SheetPlan{0, 0x99, 0x44, 6});
  h.dut.sheet_req_ready_i = 0;
  set_fragment(h, 0x1005, 0xa0000005u, 1, 1, 0, true, 1, 0);
  h.set_aux(false);
  const auto aux_hit_ctx = current_retire(h);
  const unsigned sheet_before_hit = h.sheet_requests;
  h.offer_fragment();
  for (unsigned n = 0; n < 20000 && !h.dut.sheet_req_valid_o; ++n) h.step();
  require(h.dut.sheet_req_valid_o && !h.dut.quiet_o, "held Sheet request omitted from quiet",
          h.cycle);
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
  require(!h.dut.quiet_o, "accepted Sheet READ owing a response escaped quiet", h.cycle);
  h.wait_outputs(h.retired.size() + 1);
  require(h.sheet_requests == sheet_before_hit + 1, "AUX HIT did not issue exactly one Sheet READ",
          h.cycle);
  {
    const uint32_t kSheetStrength = 0x44u;
    const uint32_t tint = 255u - (kSheetStrength >> 1);
    const uint32_t red = (0xffu * tint + 128u) >> 8;
    expect_result(h.retired.back(), red << 16, 0xff, 0, 0, 0x1005, aux_hit_ctx, h.cycle);
    require(red != 0xffu, "the sheet tint did not move the retired red channel", h.cycle);
  }

  // Sample 2 and AUX are simultaneously required. DETAIL_MASK must take alpha
  // from real sample 2 (blue ARGB/RGB565 => 255), never AUX strength 0x22.
  h.sheet_plans.push_back(SheetPlan{0, 0x7d, 0x22, 3});
  set_fragment(h, 0x1055, 0xa0000055u, 3, 1, 7, true, 1, 0);
  h.set_aux(false, 0x12345617u);
  const auto sample2_aux_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
#ifdef PACKET_B_EXPECT_AUX_AS_SAMPLE2
  expect_result(h.retired.back(), 0x000000u, 0x22, 0, 0, 0x1055, sample2_aux_ctx, h.cycle);
  std::printf("packet-b AUX-as-sample2 mutant FIRED\n");
  return;
#endif
  expect_result(h.retired.back(), 0x000000u, 0xfe, 0, 0, 0x1055, sample2_aux_ctx, h.cycle);

  // AUX MISS is a typed terminal refusal and still retires normally.
  h.sheet_plans.push_back(SheetPlan{3, 0xaa, 0xbb, 2});
  set_fragment(h, 0x1006, 0xa0000006u, 1, 1, 0, true, 1, 0);
  h.set_aux(false, 0x12345608u);
  const auto aux_miss_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1006, aux_miss_ctx, h.cycle);
  require(h.dut.frame_fault_o, "typed AUX MISS did not set frame fault", h.cycle);

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
  set_fragment(h, 0x1007, 0xa0000007u, 0, 0, 0, true, 0, 0, 0x654321u, 0x66);
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
  require(h.retired.size() == degenerate_target, "degenerate AUX did not terminally retire",
          h.cycle);
  require(h.retired.back().cycle + 1 > aux_issue_cycle,
          "local AUX refusal did not follow its logical issue", h.cycle);
  require(h.sheet_requests == sheet_before_degenerate,
          "degenerate AUX illegally issued a Sheet READ", h.cycle);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1007, degenerate_ctx, h.cycle);

  // Malformed material still issues its declared sample before local refusal;
  // no planner/cache request is allowed to escape.
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  const uint32_t planner_before = h.dut.cnt_plan_accepted_o;
  set_fragment(h, 0x1008, 0xa0000008u, 1, 1, 1, false, 1, 0);
  const auto malformed_ctx = current_retire(h);
  h.offer_fragment();
  h.wait_outputs(h.retired.size() + 1);
  expect_result(h.retired.back(), 0xff00ffu, 0xff, 0, 1, 0x1008, malformed_ctx, h.cycle);
  require(h.dut.cnt_plan_accepted_o == planner_before, "malformed material reached planner/cache",
          h.cycle);
  require(h.dut.frame_fault_o, "malformed material did not set frame fault", h.cycle);

  // Hold a real fill request and a real Sheet request to exercise their output
  // channel operands, then release them without changing payload.
  h.wait_quiet();
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  h.auto_fill = false;
  h.dut.fill_req_ready_i = 0;
  set_fragment(h, 0x1009, 0xa0000009u, 1, 8, 0, false, 1, 0);
  h.dut.frag_invw24_i = 0x00010000;
  h.dut.frag_u_over_w_i = 0x00005000;
  h.dut.frag_v_over_w_i = 0x00009000;
  h.offer_fragment();
  for (unsigned n = 0; n < 20000 && !h.dut.fill_req_valid_o; ++n) h.step();
  require(h.dut.fill_req_valid_o && !h.dut.quiet_o, "held fill request omitted from quiet",
          h.cycle);
  const uint32_t held_fill_address = h.dut.fill_req_addr_o;
  if (held_fill_address != 0x00009040u)
    std::fprintf(stderr, "  nonzero perspective fill line: %08x\n", held_fill_address);
  require(held_fill_address == 0x00009040u,
          "nonzero perspective coordinates produced the wrong 8x8 RGB565 cache line", h.cycle);
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
    set_fragment(h, static_cast<uint16_t>(0x2000u + item), 0xb0000000u + item, 0, 0, 0, false, 0, 0,
                 0x010203u + item, static_cast<uint8_t>(0x80u + item));
    h.offer_fragment();
  }
  for (unsigned n = 0; n < 20000 && h.combine_counters()[5] != 8; ++n) h.step();
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
  require(h.dut.out_valid_o && !h.dut.quiet_o, "held retirement omitted from quiet", h.cycle);
  // Let every remaining one-phase job finish while the ordered edge is held.
  for (unsigned n = 0; n < 1200; ++n) h.step();
  const auto combine_after_preload = h.combine_counters();
  require(combine_after_preload[1] == combine_jobs_before + kPrepared,
          "one-phase stream did not admit exactly one combine job per fragment", h.cycle);
  const uint32_t held_rgb = h.dut.out_rgb_o;
  const uint16_t held_tag = h.dut.out_tag_o;
  for (unsigned n = 0; n < 3; ++n) {
    h.step();
    require(h.dut.out_valid_o && h.dut.out_rgb_o == held_rgb && h.dut.out_tag_o == held_tag,
            "ordered result changed under output backpressure", h.cycle);
  }
  h.dut.out_ready_i = 1;
  uint64_t prior_cycle = 0;
  h.wait_outputs(prepared_begin + kPrepared);
  for (std::size_t index = prepared_begin; index < prepared_begin + kPrepared; ++index) {
    if (index != prepared_begin)
      require(h.retired[index].cycle == prior_cycle + 1,
              "prepared owner tuples did not retire one per clock", h.cycle);
    prior_cycle = h.retired[index].cycle;
  }
  const auto combine_drained = h.combine_counters();
  require(combine_drained[1] == combine_drained[2] && combine_drained[3] == combine_drained[4],
          "combine CJ/CD or PI/PC counters did not close", h.cycle);

  require(h.dut.cnt_fragrob_id_errors_o == 0,
          "healthy traffic moved a typed owner protocol counter", h.cycle);
  require(h.dut.err_rcp_q_o == 0, "RCP reset-lifetime queue fault moved", h.cycle);
  require(h.dut.meta_genmis_o == 0, "metadata generation mismatch moved", h.cycle);
  require(h.leaf_idle_seen_idle == 0xffffu && h.leaf_idle_seen_busy == 0xffffu,
          "not every composed leaf idle had both idle and busy evidence", h.cycle);
#ifdef PACKET_B_EXPECT_NO_SHADOWS
  require(!h.dut.shadow_present_o && h.dut.meta_shadow_reads_o == 0 &&
              h.dut.meta_shadow_mismatch_o == 0,
          "production profile retained migration-shadow capability", h.cycle);
#else
  require(h.dut.shadow_present_o && h.dut.meta_shadow_reads_o != 0,
          "laboratory profile did not execute metadata shadows", h.cycle);
  require(h.dut.meta_shadow_mismatch_o == 0 && h.dut.meta_align_err_o == 0 &&
              h.dut.meta_bil_err_o == 0 && h.dut.meta_near_err_o == 0,
          "laboratory metadata shadow disagreed with functional carriage", h.cycle);
  require(h.dut.meta_align_chk_o != 0 && h.dut.meta_bil_chk_o != 0 && h.dut.meta_near_chk_o != 0,
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
      set_fragment(reset_case, static_cast<uint16_t>(0x5000u + item), 0xd0000000u + item, 0, 0, 0,
                   false, 0, 0, 0x112200u + item, static_cast<uint8_t>(0x40u + item));
      reset_case.offer_fragment();
    }
    for (unsigned n = 0; n < 20000 && !reset_case.dut.out_valid_o; ++n) reset_case.step();
    require(reset_case.dut.out_valid_o && !reset_case.dut.quiet_o && reset_case.retired.empty(),
            "mid-flight reset control did not establish held composed work", reset_case.cycle);
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
            "pre-reset composed result escaped into the new owner namespace", reset_case.cycle);
    set_fragment(reset_case, 0x50ff, 0xd00000ffu, 0, 0, 0, false, 0, 0, 0x334455u, 0x66);
    const auto fresh_ctx = current_retire(reset_case);
    reset_case.offer_fragment();
    reset_case.wait_outputs(1);
    expect_result(reset_case.retired.back(), 0x334455u, 0x66, 0, 0, 0x50ff, fresh_ctx,
                  reset_case.cycle);
  }

  // A refusal with no accepted FI is malformed cache protocol, not the removed
  // Packet-B reset-lifetime unsupported-fill state. It is clearable without reset
  // and cannot invent an owner completion.
  {
    Harness* const malformed_storage = new Harness;
    Harness& malformed = *malformed_storage;
    malformed.reset();
    malformed.auto_fill = false;
    malformed.dut.fill_refused_i = 1;
    malformed.step();
    malformed.dut.fill_refused_i = 0;
    malformed.step();
    require(malformed.dut.frame_fault_o && malformed.cache_counters().protocol_fault &&
                malformed.retired.empty(),
            "refusal without FI did not set only recoverable cache protocol fault",
            malformed.cycle);
    malformed.wait_quiet();
    malformed.dut.frame_fault_clear_valid_i = 1;
    malformed.step();
    malformed.dut.frame_fault_clear_valid_i = 0;
    malformed.step();
    require(!malformed.dut.frame_fault_o && !malformed.cache_counters().protocol_fault,
            "frame clear failed to recover refusal-without-FI protocol fault", malformed.cycle);
  }

  std::printf(
      "packet-b directed PASS: outputs=%zu near/clut/bil exact, "
      "sheet_requests=%u, bilerp_jobs=%u, active_generation=%u\n",
      h.retired.size(), h.sheet_requests, static_cast<unsigned>(h.dut.cnt_bilerp_jobs_o),
      static_cast<unsigned>(h.dut.active_page_generation_o));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  run_directed();
  zhao::exit_hard(0);
}

#endif  // exactly zero production or one PACKET_E_EXPECT_* mutant mode
