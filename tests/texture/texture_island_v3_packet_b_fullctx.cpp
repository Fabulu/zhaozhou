// Packet-B private full-context/backpressure test.
// Built only in a private Verilator --Mdir; intentionally not in CMake/CTest.
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

// Required by this Verilator/MinGW runtime configuration.
double sc_time_stamp() { return 0.0; }

namespace {

[[noreturn]] void fail(const char* text, uint64_t cycle) {
  std::fprintf(stderr, "packet-b fullctx FAIL at cycle %llu: %s\n",
               static_cast<unsigned long long>(cycle), text);
  std::abort();
}

void require(bool condition, const char* text, uint64_t cycle) {
  if (!condition) fail(text, cycle);
}

uint32_t rng_state = 0x91e10da5u;
uint32_t random32() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

struct Expected {
  std::array<uint32_t, 2> legacy{};
  std::array<uint32_t, 5> retire{};
  uint32_t rgb = 0;
  uint8_t alpha = 0;
};

struct Snapshot {
  uint32_t rgb = 0;
  uint8_t alpha = 0;
  uint8_t index = 0;
  uint8_t status = 0;
  uint16_t tag = 0;
  std::array<uint32_t, 5> retire{};
  std::array<uint32_t, 7> owner_context{};
};

class Harness {
 public:
  VerilatedContext context;
  Vzhao_texture_island_v3_top dut{&context};
  uint64_t cycle = 0;
  bool saw_material_busy_leaf_idle = false;
  bool saw_material_idle_leaf_busy = false;

  Harness() {
    dut.clk = 0;
    dut.rst_n = 0;
    dut.frag_valid_i = 0;
    dut.frag_invw24_i = 0x800000u;
    dut.frag_u_over_w_i = 0;
    dut.frag_v_over_w_i = 0;
    dut.frag_sample_count_i = 0;
    dut.frag_binding_i = 0;
    dut.frag_lod_i = 0;
    dut.frag_recipe_i = 0;
    dut.frag_weight_i = 0;
    dut.frag_ctx_i = 0;
    for (unsigned i = 0; i < 5; ++i) dut.frag_retire_ctx_i[i] = 0;
    for (unsigned i = 0; i < 7; ++i) dut.frag_aux_ctx_i[i] = 0;
    dut.frag_aux_i = 0;
    dut.frag_base_rgb_i = 0;
    dut.frag_base_a_i = 0;
    dut.frag_class_i = 0;
    dut.frag_pal_slot_i = 0;
    dut.frag_pal_gen_i = 0;
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
    dut.out_ready_i = 0;
  }

  void observe_combine_idle_components() {
    svBit material_idle = 0;
    svBit leaf_idle = 0;
    select_dpi_scope();
    zhao_texture_packet_b_get_combine_idle_components(&material_idle, &leaf_idle);
    saw_material_busy_leaf_idle |= !material_idle && leaf_idle;
    saw_material_idle_leaf_busy |= material_idle && !leaf_idle;
  }

  void step() {
    dut.clk = 0;
    dut.eval();
    observe_combine_idle_components();
    dut.clk = 1;
    dut.eval();
    ++cycle;
    context.timeInc(1);
  }

  void reset() {
    for (unsigned i = 0; i < 4; ++i) step();
    dut.rst_n = 1;
    for (unsigned i = 0; i < 4; ++i) step();
  }

  void select_dpi_scope() {
    svScope scope = svGetScopeFromName(PACKET_B_DPI_SCOPE);
    require(scope != nullptr, "DPI scope was not registered", cycle);
    svSetScope(scope);
  }

  uint32_t owner_admitted() {
    unsigned int admitted = 0;
    select_dpi_scope();
    zhao_texture_packet_b_get_owner_admitted(&admitted);
    return admitted;
  }

  Snapshot snapshot() {
    Snapshot value;
    value.rgb = dut.out_rgb_o;
    value.alpha = dut.out_a_o;
    value.index = dut.out_texel_idx_o;
    value.status = dut.out_status_o;
    value.tag = dut.out_tag_o;
    for (unsigned i = 0; i < 5; ++i) value.retire[i] = dut.out_retire_ctx_o[i];
    svBitVecVal context_words[7]{};
    select_dpi_scope();
    zhao_texture_packet_b_get_owner_context(context_words);
    for (unsigned i = 0; i < 7; ++i)
      value.owner_context[i] = context_words[i];
    return value;
  }
};

bool snapshots_equal(const Snapshot& lhs, const Snapshot& rhs) {
  return lhs.rgb == rhs.rgb && lhs.alpha == rhs.alpha &&
      lhs.index == rhs.index && lhs.status == rhs.status &&
      lhs.tag == rhs.tag && lhs.retire == rhs.retire &&
      lhs.owner_context == rhs.owner_context;
}

void load_offer(Harness& h, const Expected& expected) {
  h.dut.frag_ctx_i = (static_cast<uint64_t>(expected.legacy[1]) << 32) |
                     expected.legacy[0];
  for (unsigned i = 0; i < 5; ++i)
    h.dut.frag_retire_ctx_i[i] = expected.retire[i];
  h.dut.frag_base_rgb_i = expected.rgb;
  h.dut.frag_base_a_i = expected.alpha;
  h.dut.frag_valid_i = 1;
}

void check_retirement(const Snapshot& got, const Expected& expected,
                      uint64_t cycle) {
  require(got.rgb == expected.rgb, "count-zero base RGB changed", cycle);
  require(got.alpha == expected.alpha, "count-zero base alpha changed", cycle);
  require(got.index == 0 && got.status == 0,
          "count-zero result status/index changed", cycle);
  require(got.tag == static_cast<uint16_t>(expected.legacy[0]),
          "legacy tag alias changed", cycle);
  require(got.retire == expected.retire,
          "public 160-bit retirement context changed", cycle);
  require(got.owner_context[0] == expected.legacy[0] &&
          got.owner_context[1] == expected.legacy[1],
          "full legacy64 owner context changed", cycle);
  for (unsigned i = 0; i < 5; ++i)
    require(got.owner_context[i + 2] == expected.retire[i],
            "owner context is not exact {retire160,legacy64}", cycle);
}

void run_full_context() {
  Harness* const harness = new Harness;
  Harness& h = *harness;
  h.reset();
  h.dut.clk = 0;
  h.dut.eval();
  require(h.dut.quiet_o, "reset island was not publicly quiet", h.cycle);

  constexpr unsigned kTransactions = 96;
  std::array<Expected, kTransactions> source{};
  for (unsigned index = 0; index < kTransactions; ++index) {
    source[index].legacy[0] = random32();
    source[index].legacy[1] = random32();
    for (unsigned word = 0; word < 5; ++word)
      source[index].retire[word] = random32();
    source[index].rgb = random32() & 0x00ffffffu;
    source[index].alpha = static_cast<uint8_t>(random32());
  }

  std::deque<Expected> scoreboard;
  unsigned offered = 0;
  unsigned retired = 0;
  bool holding_offer = false;
  bool stalled_snapshot_valid = false;
  Snapshot stalled_snapshot{};
  unsigned ingress_stall_cycles = 0;
  unsigned output_stall_cycles = 0;

  for (unsigned watchdog = 0; watchdog < 200000 && retired < kTransactions;
       ++watchdog) {
    if (!holding_offer && offered < kTransactions && (random32() & 3u) != 0) {
      load_offer(h, source[offered]);
      holding_offer = true;
    }
    h.dut.out_ready_i = ((random32() & 7u) < 5u) ? 1 : 0;

    h.dut.clk = 0;
    h.dut.eval();
    h.observe_combine_idle_components();
    const bool frag_fire = h.dut.frag_valid_i && h.dut.frag_ready_o;
    const bool out_fire = h.dut.out_valid_o && h.dut.out_ready_i;

    if (holding_offer && !h.dut.frag_ready_o) ++ingress_stall_cycles;
    if (h.dut.out_valid_o && !h.dut.out_ready_i) {
      ++output_stall_cycles;
      const Snapshot now = h.snapshot();
      if (stalled_snapshot_valid)
        require(snapshots_equal(now, stalled_snapshot),
                "full output/context changed while stalled", h.cycle);
      stalled_snapshot = now;
      stalled_snapshot_valid = true;
    } else {
      stalled_snapshot_valid = false;
    }

    if (out_fire) {
      require(!scoreboard.empty(), "output had no accepted input", h.cycle);
      check_retirement(h.snapshot(), scoreboard.front(), h.cycle);
      scoreboard.pop_front();
      ++retired;
    }

    h.dut.clk = 1;
    h.dut.eval();
    ++h.cycle;
    h.context.timeInc(1);

    if (frag_fire) {
      scoreboard.push_back(source[offered]);
      ++offered;
      holding_offer = false;
      h.dut.frag_valid_i = 0;
    }
  }

  require(offered == kTransactions && retired == kTransactions,
          "randomized stream did not completely retire", h.cycle);
  require(scoreboard.empty(), "scoreboard did not drain", h.cycle);
  require(ingress_stall_cycles != 0,
          "randomized run never exercised ingress backpressure", h.cycle);
  require(output_stall_cycles != 0,
          "randomized run never exercised output backpressure", h.cycle);
  require(h.saw_material_busy_leaf_idle,
          "material-read pipeline never independently held work", h.cycle);
  require(h.saw_material_idle_leaf_busy,
          "combine leaf never independently held work", h.cycle);

  h.dut.out_ready_i = 1;
  for (unsigned n = 0; n < 20000; ++n) {
    h.dut.clk = 0; h.dut.eval();
    if (h.dut.quiet_o) break;
    h.step();
  }
  h.dut.clk = 0; h.dut.eval();
  require(h.dut.quiet_o, "full-context stream did not reach quiet", h.cycle);

  // Owner counter fire behavior is exercised by the legal standalone owner suite;
  // this top test checks only the stable public sum at the healthy zero point.
  require(h.dut.cnt_fragrob_id_errors_o == 0,
          "healthy full-context run moved owner error sum", h.cycle);

  // Inject an independent recoverable event on the exact accepted clear edge
  // through the stable synthesis-excluded DPI seam.
  h.select_dpi_scope();
  zhao_texture_packet_b_set_frame_fault_inject(1);
  h.dut.frame_fault_clear_valid_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
  require(h.dut.frame_fault_clear_ready_o,
          "same-edge control did not start from accepted clear", h.cycle);
  h.dut.clk = 1;
  h.dut.eval();
  ++h.cycle;
  h.context.timeInc(1);
  zhao_texture_packet_b_set_frame_fault_inject(0);
  h.dut.frame_fault_clear_valid_i = 0;
  h.dut.eval();
  require(!h.dut.lifetime_structural_fault_o,
          "recoverable fault was misclassified as reset-lifetime", h.cycle);
#ifdef PACKET_B_EXPECT_CLEAR_OVER_FAULT
  require(!h.dut.frame_fault_o,
          "clear-over-fault mutant did not erase same-edge fault", h.cycle);
  std::printf("packet-b clear-over-fault mutant FIRED\n");
  return;
#else
  require(h.dut.frame_fault_o,
          "same-edge recoverable fault lost to frame clear", h.cycle);
#endif

  // Clear the injected recoverable event, then exercise each remaining
  // reset-lifetime aggregation lane independently. Packet-E removed the old
  // fill-refusal lane entirely; the six retained sources still receive a
  // one-cycle pulse and must block fragment/config/palette until reset.
  h.dut.frame_fault_clear_valid_i = 1;
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.frame_fault_o && !h.dut.lifetime_structural_fault_o,
          "recoverable injection did not clear", h.cycle);
  for (unsigned fault_source = 0; fault_source < 6; ++fault_source) {
    // Hold a complete legal offer on the first cycle with the registered
    // lifetime level asserted. The outer combinational barrier must refuse it
    // before either registered admission event can exist.
    h.select_dpi_scope();
    zhao_texture_packet_b_set_lifetime_fault_inject(1u << fault_source);
    h.step();
    load_offer(h, source[fault_source]);
    const uint32_t admitted_before = h.owner_admitted();
    h.dut.clk = 0;
    h.dut.eval();
    require(!h.dut.frag_ready_o,
            "fault-coincident fragment offer was not blocked immediately", h.cycle);
    h.step();
    require(h.owner_admitted() == admitted_before,
            "fault-coincident fragment entered the owner event boundary", h.cycle);
    h.dut.frag_valid_i = 0;
    h.select_dpi_scope();
    zhao_texture_packet_b_set_lifetime_fault_inject(0);
    h.dut.eval();
    require(h.dut.frame_fault_o && h.dut.lifetime_structural_fault_o &&
                !h.dut.frag_ready_o,
            "pulsed lifetime source did not latch/classify/block admission", h.cycle);
    h.dut.cfg_valid_i = 1;
    h.dut.pal_load_valid_i = 1;
    h.dut.eval();
    require(!h.dut.cfg_ready_o && !h.dut.pal_load_ready_o,
            "pulsed lifetime source did not block binding/palette writes", h.cycle);
    h.dut.cfg_valid_i = 0;
    h.dut.pal_load_valid_i = 0;
    h.dut.frame_fault_clear_valid_i = 1;
    h.step();
    h.dut.frame_fault_clear_valid_i = 0;
    h.step();
    require(h.dut.frame_fault_o && h.dut.lifetime_structural_fault_o,
            "frame clear erased withdrawn lifetime source", h.cycle);
    h.dut.rst_n = 0;
    h.step();
    h.dut.rst_n = 1;
    h.step();
    require(!h.dut.frame_fault_o && !h.dut.lifetime_structural_fault_o,
            "reset did not recover lifetime source", h.cycle);
  }

  std::printf("packet-b fullctx PASS: accepted=%u retired=%u "
              "ingress_stalls=%u output_stalls=%u lifetime_sources=6\n",
              offered, retired, ingress_stall_cycles, output_stall_cycles);
}

} // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  run_full_context();
  zhao::exit_hard(0);
}
