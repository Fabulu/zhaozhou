// raster_texture_v3_fit_top_directed.cpp -- Packet-F G8A activity gate.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define DPI_DLLISPEC
#define PLI_DLLISPEC
#include "Vzhao_raster_texture_v3_fit_top.h"
#include "Vzhao_raster_texture_v3_fit_top__Dpi.h"
#include "verilated.h"
#include "svdpi.h"

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

namespace {

struct Activity {
  uint32_t jobs = 0;
  uint32_t fills = 0;
  uint32_t fill_beats = 0;
  uint32_t fb_words = 0;
  uint32_t tiles = 0;
  uint32_t cfg = 0;
  uint32_t palette = 0;
  bool setup_fault = false;
  uint8_t setup_fault_cause = 0;
  bool frame_fault = false;
  bool fragment_error = false;
  bool saw_green = false;
  bool saw_index5 = false;
  uint16_t first_nonzero_fb = 0;
  bool saw_candidate = false;
  bool saw_fragment = false;
  bool saw_fill = false;
  bool saw_fb_stall = false;
  uint8_t active_generation = 0;
};

int checks = 0;
int failures = 0;

void check(bool condition, const char* message, uint64_t expected = 1,
           uint64_t actual = 0) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s (expected %llu, got %llu)\n", message,
                static_cast<unsigned long long>(expected),
                static_cast<unsigned long long>(actual));
  }
}

void tick(Vzhao_raster_texture_v3_fit_top& dut) {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
  dut.clk = 0;
  dut.eval();
}

Activity activity() {
  constexpr const char* kScope = "TOP.zhao_raster_texture_v3_fit_top";
  svScope scope = svGetScopeFromName(kScope);
  if (scope == nullptr) {
    std::printf("FAIL: G8A DPI scope absent: %s\n", kScope);
    std::fflush(nullptr);
    std::_Exit(1);
  }
  svSetScope(scope);
  Activity a;
  svBit setup_fault = 0;
  svBitVecVal setup_fault_cause = 0;
  svBit frame_fault = 0;
  svBit fragment_error = 0;
  svBit saw_green = 0;
  svBit saw_index5 = 0;
  svBitVecVal first_nonzero_fb = 0;
  svBit saw_candidate = 0;
  svBit saw_fragment = 0;
  svBit saw_fill = 0;
  svBit saw_fb_stall = 0;
  svBitVecVal generation = 0;
  zhao_g8a_get_activity(
      &a.jobs, &a.fills, &a.fill_beats, &a.fb_words, &a.tiles,
      &a.cfg, &a.palette, &setup_fault, &setup_fault_cause, &frame_fault,
      &fragment_error, &saw_green, &saw_index5, &first_nonzero_fb,
      &saw_candidate, &saw_fragment, &saw_fill, &saw_fb_stall,
      &generation);
  a.setup_fault = setup_fault != 0;
  a.setup_fault_cause = static_cast<uint8_t>(setup_fault_cause);
  a.frame_fault = frame_fault != 0;
  a.fragment_error = fragment_error != 0;
  a.saw_green = saw_green != 0;
  a.saw_index5 = saw_index5 != 0;
  a.first_nonzero_fb = static_cast<uint16_t>(first_nonzero_fb);
  a.saw_candidate = saw_candidate != 0;
  a.saw_fragment = saw_fragment != 0;
  a.saw_fill = saw_fill != 0;
  a.saw_fb_stall = saw_fb_stall != 0;
  a.active_generation = static_cast<uint8_t>(generation);
  return a;
}

}  // namespace

double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_texture_v3_fit_top dut;
  dut.clk = 0;
  dut.rst_n = 0;
  dut.eval();
  for (int cycle = 0; cycle < 8; ++cycle) tick(dut);
  dut.rst_n = 1;

  bool signatures[256] = {};
  unsigned signature_count = 0;
  uint8_t previous_epoch = dut.fit_epoch_o;
  unsigned epoch_changes = 0;
  Activity a;
  uint32_t clocks = 0;
  constexpr uint32_t kLimit = 3000000;
  for (; clocks < kLimit; ++clocks) {
    tick(dut);
    const uint8_t signature = dut.fit_signature_o;
    if (!signatures[signature]) {
      signatures[signature] = true;
      ++signature_count;
    }
    if (dut.fit_epoch_o != previous_epoch) {
      previous_epoch = dut.fit_epoch_o;
      ++epoch_changes;
    }
    if ((clocks & 127u) == 0u) {
      a = activity();
      if (a.tiles >= 2) break;
    }
  }
  a = activity();

  check(clocks < kLimit, "two G8A tiles complete within the bounded run", 1,
        clocks < kLimit);
  check(a.palette == 258, "palette BEGIN/256-WRITE/END accepted exactly", 258,
        a.palette);
  check(a.cfg == 3, "binding BEGIN/ROW/END accepted exactly", 3, a.cfg);
  check(a.active_generation == 1, "binding generation one activated", 1,
        a.active_generation);
  check(a.jobs >= 2 && a.tiles >= 2,
        "legal repeating tile jobs and completions are active", 2, a.tiles);
  check(a.fills >= 1 && a.saw_fill, "real cache miss/fill path is active", 1,
        a.fills);
  check(a.fill_beats == 8 * a.fills,
        "every accepted cache fill receives exactly eight halfwords",
        8 * a.fills, a.fill_beats);
  check(a.fb_words == 256 * a.tiles,
        "every completed tile emits exactly 256 resolved framebuffer words",
        256 * a.tiles, a.fb_words);
  check(a.saw_index5,
        "CLUT path returns raw index five, opaque green, and zero status");
  check(a.first_nonzero_fb != 0,
        "textured fragments produce a nonzero resolved framebuffer value", 1,
        a.first_nonzero_fb);
  check(a.saw_candidate && a.saw_fragment,
        "Packet-C candidate and real fragment stages are active");
  check(a.saw_fb_stall, "deterministic responder exercises framebuffer backpressure");
  check(!a.setup_fault && !a.frame_fault && !a.fragment_error,
        "legal G8A traffic raises no setup/frame/fragment fault", 0,
        static_cast<unsigned>(a.setup_fault || a.frame_fault || a.fragment_error));
  check(signature_count >= 16, "MISR exposes nonconstant connected activity", 16,
        signature_count);
  check(epoch_changes >= 64, "MISR source epoch traverses the complete schedule", 64,
        epoch_changes);

  if (failures != 0) {
    std::printf("[raster_texture_v3_fit_top] %d of %d checks FAILED ", failures,
                checks);
    std::printf("after %u clocks jobs=%u fills=%u beats=%u fb=%u tiles=%u "
                "cfg=%u pal=%u gen=%u setup_fault=%u cause=%x frame_fault=%u frag_error=%u\n",
                clocks, a.jobs, a.fills, a.fill_beats, a.fb_words, a.tiles,
                a.cfg, a.palette, a.active_generation,
                static_cast<unsigned>(a.setup_fault),
                static_cast<unsigned>(a.setup_fault_cause),
                static_cast<unsigned>(a.frame_fault),
                static_cast<unsigned>(a.fragment_error));
    std::fflush(nullptr);
    std::_Exit(1);
  }
  std::printf(
      "[raster_texture_v3_fit_top] %d checks passed after %u clocks; "
      "jobs=%u fills=%u beats=%u fb=%u tiles=%u signatures=%u\n",
      checks, clocks, a.jobs, a.fills, a.fill_beats, a.fb_words, a.tiles,
      signature_count);
  std::fflush(nullptr);
  std::_Exit(0);

  // Implicit `return 0` deadlocks the same way -- see zhao_sim.hpp.
  zhao::exit_hard(0);
}
