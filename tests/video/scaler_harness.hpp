// scaler_harness.hpp — driver for the VIDEO.SCALER tests (own Verilator
// top: the scaler is tested in isolation with a synthetic stream).
//
// Compares the RTL against zref::ScalerFeed (the 2-cycle-delay identity
// with a consumer-ready freeze, spec/video_rules.md §6) every vid cycle and
// mixes every compared value into an FNV trace hash for run-twice
// determinism (plan R1).

#pragma once

#include "Vzhao_scaler_tb.h"
#include "zhao_sim.hpp"
#include "video_common.hpp"  // Fnv1a + Rng (shared with the subsystem tests)
#include "zref/zref_video.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace zhao_video {

class ScalerTb {
 public:
  Vzhao_scaler_tb top;
  zref::ScalerFeed oracle;
  uint64_t n = 0;
  Fnv1a trace;
  int failures = 0;
  int checks = 0;

  void reset(int cycles = 3) {
    top.rst_n = 0;
    drive_(zref::PxStream{}, true);
    top.vid_clk = 0;
    top.eval();
    for (int i = 0; i < cycles; ++i) tick_clk_();
    top.rst_n = 1;
    top.eval();
    n = 0;
    oracle.reset();
    trace = Fnv1a{};
  }

  // one vid cycle: px = stream level DURING the cycle; ready = sink level
  void step(const zref::PxStream& px, bool ready) {
    drive_(px, ready);
    tick_clk_();
    oracle.step(px, ready);
    compare_(px);
  }

 private:
  void drive_(const zref::PxStream& px, bool ready) {
    top.in_valid = px.valid ? 1 : 0;
    top.in_rgb = px.rgb565 & 0xFFFF;
    top.in_x = px.x & 0x3FF;
    top.in_y = px.y & 0xFF;
    top.in_hsync = px.hsync ? 1 : 0;
    top.in_vsync = px.vsync ? 1 : 0;
    top.in_hblank = px.hblank ? 1 : 0;
    top.in_vblank = px.vblank ? 1 : 0;
    top.out_ready = ready ? 1 : 0;
  }

  void tick_clk_() {
    n++;
    top.vid_clk = 0;
    top.eval();
    top.vid_clk = 1;
    top.eval();
    top.vid_clk = 0;
    top.eval();
  }

  void cmp_(const char* what, uint64_t exp, uint64_t act) {
    ++checks;
    trace.mix(exp);
    trace.mix(act);
    if (exp != act) {
      ++failures;
      if (failures <= 20) {
        std::printf("SCALER MISMATCH n=%llu %s: oracle=%llu rtl=%llu\n",
                    (unsigned long long)n, what, (unsigned long long)exp,
                    (unsigned long long)act);
        std::vector<uint8_t> vec;
        for (uint64_t v : {exp, act, n}) {
          for (int i = 0; i < 8; ++i)
            vec.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
        }
        zhao::save_failing_vector(std::string("scaler_") + what, vec,
                                  std::to_string((unsigned long long)exp),
                                  std::to_string((unsigned long long)act));
      }
    }
  }

  void compare_(const zref::PxStream& in) {
    const zref::PxStream o = oracle.out();
    cmp_("valid", o.valid, top.out_valid);
    cmp_("rgb", o.rgb565, top.out_rgb);
    cmp_("x", o.x, top.out_x);
    cmp_("y", o.y, top.out_y);
    cmp_("hsync", o.hsync, top.out_hsync);
    cmp_("vsync", o.vsync, top.out_vsync);
    cmp_("hblank", o.hblank, top.out_hblank);
    cmp_("vblank", o.vblank, top.out_vblank);
    cmp_("never_active", oracle.never_active(), top.never_active);
  }
};

}  // namespace zhao_video
