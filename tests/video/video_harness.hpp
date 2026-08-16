// video_harness.hpp — shared driver for the W2.2 VIDEO differential tests.
//
// Drives the Verilated zhao_video_tb (whole subsystem) and the zref::VideoSys
// mirror on ONE unified gpu-cycle timeline with a FIXED clock phase (plan
// risk R1): gpu_clk posedges every step, vid_clk = gpu_clk/2 with coincident
// posedges on ODD steps (both clocks rise in the same eval, so cross-domain
// synchronizers sample pre-edge values exactly like the mirror assumes).
//
// The zref::VramResponder is the harness-side guard/memory (D10 spirit: the
// harness answers bursts deterministically): its outputs are presented to
// BOTH sides on the same cycle, and the request streams are compared — a
// divergence fails at the first differing cycle with the saved minimal
// vector (charter §29-17).
//
// Run-twice determinism (plan R1): every step mixes the full observable set
// into an FNV-1a hash; a scenario is run twice and the hashes must match.

#pragma once

#include "Vzhao_video_tb.h"
#include "zhao_sim.hpp"      // save_failing_vector / check registry
#include "video_common.hpp"  // Fnv1a + Rng (shared with the scaler tests)
#include "zref/zref_video.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace zhao_video {

class VideoTb {
 public:
  Vzhao_video_tb top;
  zref::VramResponder resp{4};   // fixed sim profile latency (gpu cycles)
  zref::VideoSys oracle;

  // per-cycle stimulus (set by the scenario before step())
  bool mode_we = false;
  uint32_t mode_in = 0;
  uint32_t slot_ready = 0;
  uint32_t deadline_cycles = 0;
  bool px_out_ready = true;

  uint64_t n = 0;          // gpu steps since reset
  Fnv1a trace;             // run-twice determinism hash
  int failures = 0;
  int checks = 0;

  void reset(int cycles = 4) {
    top.rst_n = 0;
    top.mode_we = 0;
    top.mode_in = 0;
    top.slot_ready = 0;
    top.deadline_cycles = 0;
    top.px_out_ready = 1;
    top.guard_ready = 0;
    top.guard_ok = 0;
    top.guard_violation = 0;
    top.beat_valid = 0;
    top.beat_data = 0;
    top.beat_last = 0;
    top.gpu_clk = 0;
    top.vid_clk = 0;
    top.eval();
    for (int i = 0; i < cycles; ++i) clock_only_();
    top.rst_n = 1;
    top.eval();
    n = 0;
    resp.reset();
    oracle.reset();
    trace = Fnv1a{};
  }

  // One gpu cycle: compare every observable, mix into the trace hash.
  void step() {
    // harness stimulus for this cycle (levels during the cycle)
    top.mode_we = mode_we ? 1 : 0;
    top.mode_in = mode_in & 3u;
    top.slot_ready = slot_ready & 3u;
    top.deadline_cycles = deadline_cycles;
    top.px_out_ready = px_out_ready ? 1 : 0;

    // the responder answers the request CURRENTLY on the RTL wires (its
    // outputs feed BOTH sides; request equality is checked below)
    const bool rv = top.o_req_valid != 0;
    const uint32_t ra = top.o_req_addr;
    const uint32_t rl = top.o_req_len;
    if (rv && ra + 64u > 0x78000u) {
      // invariant: the fetch only ever addresses the two FB slots
      // (region map, spec/memory_rules.md §5)
      ++failures;
      std::fprintf(stderr,
                   "ROGUE REQ n=%llu addr=0x%x len=%u raster=(%u,%u) m=%u/%u\n",
                   (unsigned long long)(n + 1), ra, rl, (unsigned)top.o_x,
                   (unsigned)top.o_y, (unsigned)top.o_mode,
                   (unsigned)top.o_mode_next);
    }
    const zref::VramResponder::Out ro = resp.step(rv, ra, rl);
    top.guard_ready = ro.ready ? 1 : 0;
    top.guard_ok = ro.ok ? 1 : 0;
    top.guard_violation = ro.violation ? 1 : 0;
    top.beat_valid = ro.beat_valid ? 1 : 0;
    top.beat_data = ro.beat_data;
    top.beat_last = 0;   // the responder delivers beats back-to-back

    // edge
    clock_only_();

    // oracle on the same stimulus
    zref::VideoSysIn oi;
    oi.mode_we = mode_we;
    oi.mode_in = mode_in & 3u;
    oi.slot_ready = slot_ready & 3u;
    oi.deadline_cycles = deadline_cycles;
    oi.guard_ready = ro.ready;
    oi.guard_ok = ro.ok;
    oi.guard_violation = ro.violation;
    oi.beat_valid = ro.beat_valid;
    oi.beat_data = ro.beat_data;
    oi.px_out_ready = px_out_ready;
    oracle.step(oi);

    compare_();
  }

  uint64_t gpu_steps() const { return n; }

  // true when the last step ended on a vid edge: the vid-domain pin
  // levels are STABLE across the 2 gpu steps of a vid cycle — scenario
  // code that samples pins per vid cycle must gate on this
  bool vid_edge() const { return (n & 1ull) == 1ull; }

 private:
  void clock_only_() {
    n++;
    top.gpu_clk = 0;
    top.eval();
    if (n & 1) {
      top.gpu_clk = 1;
      top.vid_clk = 1;   // coincident posedges (fixed 2:1 phase)
      top.eval();
    } else {
      top.gpu_clk = 1;
      top.vid_clk = 0;   // gpu posedge only (vid low half)
      top.eval();
    }
    top.gpu_clk = 0;
    top.eval();
  }

  void cmp_(const char* what, uint64_t exp, uint64_t act) {
    ++checks;
    trace.mix(exp);
    trace.mix(act);
    if (exp != act) {
      ++failures;
      if (failures <= 20) {
        std::printf("MISMATCH gpu_step=%llu vid_step=%llu %s: oracle=%llu rtl=%llu\n",
                    (unsigned long long)n,
                    (unsigned long long)oracle.vid_steps(), what,
                    (unsigned long long)exp, (unsigned long long)act);
        std::vector<uint8_t> vec;
        for (uint64_t v : {exp, act, n}) {
          for (int i = 0; i < 8; ++i)
            vec.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
        }
        zhao::save_failing_vector(std::string("video_") + what,
                                  vec,
                                  std::to_string((unsigned long long)exp),
                                  std::to_string((unsigned long long)act));
      }
    }
  }

  void compare_() {
    const zref::VideoSysOut o = oracle.out();
    const zref::RasterView& r = o.raster;

    cmp_("x", r.x, top.o_x);
    cmp_("y", r.y, top.o_y);
    cmp_("hsync", r.hsync, top.o_hsync);
    cmp_("vsync", r.vsync, top.o_vsync);
    cmp_("hblank", r.hblank, top.o_hblank);
    cmp_("vblank", r.vblank, top.o_vblank);
    cmp_("frame_start", r.frame_start, top.o_frame_start);
    cmp_("frame_end", r.frame_end, top.o_frame_end);
    cmp_("vswap_dec", r.vswap_dec, top.o_vswap_dec);
    cmp_("mode", r.mode, top.o_mode);
    cmp_("mode_next", r.mode_next, top.o_mode_next);

    cmp_("req_valid", o.req_valid, top.o_req_valid);
    cmp_("req_write", o.req_write, top.o_req_write);
    if (o.req_valid) {
      cmp_("req_addr", o.req_addr, top.o_req_addr);
      cmp_("req_len", o.req_len, top.o_req_len);
    }

    cmp_("px_valid", o.px.valid, top.o_px_valid);
    cmp_("px_rgb", o.px.rgb565, top.o_px_rgb);
    cmp_("px_x", o.px.x, top.o_px_x);
    cmp_("px_y", o.px.y, top.o_px_y);
    cmp_("px_hsync", o.px.hsync, top.o_px_hsync);
    cmp_("px_vsync", o.px.vsync, top.o_px_vsync);
    cmp_("px_hblank", o.px.hblank, top.o_px_hblank);
    cmp_("px_vblank", o.px.vblank, top.o_px_vblank);
    cmp_("scaler_violation", o.scaler_violation, top.o_scaler_violation);

    cmp_("frame_tick", o.frame_tick, top.o_frame_tick);
    cmp_("frame_id", o.frame_id, top.o_frame_id);
    cmp_("repeated", o.frame_repeated, top.o_repeated);
    cmp_("swap_req", o.swap_req, top.o_swap_req);
    cmp_("swap_slot", o.swap_slot, top.o_swap_slot);
    cmp_("swap_ack", o.swap_ack, top.o_swap_ack);
    cmp_("deadline_faults", o.deadline_faults, top.o_deadline_faults);
    cmp_("frame_cycles", o.frame_cycles, top.o_frame_cycles);
    cmp_("deadline_margin", o.deadline_margin, top.o_deadline_margin);

    cmp_("gpu_tick", o.gpu_tick, top.o_gpu_tick);
    if (o.gpu_tick) {
      cmp_("gpu_tick_frame_id", o.gpu_tick_frame_id, top.o_gpu_tick_frame_id);
      cmp_("gpu_tick_repeated", o.gpu_tick_repeated, top.o_gpu_tick_repeated);
      cmp_("gpu_complete_slot", o.gpu_complete_slot, top.o_gpu_complete_slot);
    }

    cmp_("starvation", o.starvation, top.o_starvation);
  }
};
}  // namespace zhao_video
