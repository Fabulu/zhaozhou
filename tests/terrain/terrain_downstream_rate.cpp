// terrain_downstream_rate.cpp — the downstream triangle rate, MEASURED.
//
// reports/PROJECTION-ADOPTION-20260910.md §7 item 4: "One triangle per clock
// is 3x the legacy rate. If GEOM.CLIP cannot take it, three copies buy
// nothing and one copy at 6 M10K is the right shell. Unmeasured."
//
// This drives tb_terrain_downstream (GEOM.CLIP -> GEOM.SETUP, the two blocks
// the terrain shell's packet enters, wired with no adapter) at ONE TRIANGLE
// PER CLOCK from the front and counts what comes out the back, three ways:
//
//   1. consumer always ready, terrain-shaped triangles (every one accepted):
//      the whole stream must pass in N + fixed latency cycles, i.e. the pair
//      sustains 1 triangle/clock. The PRINTED number is the claim; the check
//      is a bound (N + 16) so a regression that halves the rate fails loudly.
//   2. the same stream with a mix of rejections (behind-eye, off-screen,
//      degenerate): a rejected triangle must cost NOTHING extra -- GEOM.CLIP's
//      contract says a reject retires regardless of out_ready_i.
//   3. a 30 % random stall on GEOM.SETUP's consumer: every accepted triangle
//      still arrives, in order, with its src_id -- backpressure loses nothing.
//
// The rate the FRAME needs is derived beside the number in the report: the
// two-view all-level-0 stress is 2 x 256 x 16 x 128 = 1,048,576 triangles,
// which needs >= 0.63 triangles/clock of the 1,666,666-clock frame. One arena
// copy replays 0.33/clock. That arithmetic, not this measurement alone,
// decides the replica count -- this measurement says whether the rate the
// arithmetic asks for can be CONSUMED.

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_downstream.h"

#include "zhao_sim.hpp"

using zhao::check;

namespace {

struct Rng {
  uint32_t s = 0x2545F491u;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  bool chance(uint32_t pct) { return (next() % 100u) < pct; }
};

struct InTri {
  int32_t x[3], y[3];
  uint8_t behind;
  uint16_t src;
  bool expect_accept;
};

uint32_t m21(int32_t v) { return static_cast<uint32_t>(v) & 0x1FFFFFu; }

// A level-0 terrain cell seen from a middling distance: 4 px cells over a
// 256x192 viewport, the two §4.3 triangles per cell. Everything inside.
std::vector<InTri> terrain_stream(int n, uint16_t src0) {
  std::vector<InTri> v;
  for (int i = 0; i < n; ++i) {
    const int cell = i / 2, t = i % 2;
    const int cx = (cell % 60) * 4 + 8, cy = ((cell / 60) % 44) * 4 + 8;  // pixels
    const int x0 = cx * 256, x1 = (cx + 4) * 256, y0 = cy * 256, y1 = (cy + 4) * 256;
    InTri tr{};
    if (t == 0) {  // (i00, i11, i10)
      tr.x[0] = x0; tr.y[0] = y0; tr.x[1] = x1; tr.y[1] = y1; tr.x[2] = x1; tr.y[2] = y0;
    } else {       // (i00, i01, i11)
      tr.x[0] = x0; tr.y[0] = y0; tr.x[1] = x0; tr.y[1] = y1; tr.x[2] = x1; tr.y[2] = y1;
    }
    tr.behind = 0;
    tr.src = static_cast<uint16_t>(src0 + i);
    tr.expect_accept = true;
    v.push_back(tr);
  }
  return v;
}

class Bench {
 public:
  Vtb_terrain_downstream* tb;
  Rng rng;
  uint64_t cycle = 0;
  std::deque<uint16_t> got;  // src ids leaving GEOM.SETUP, in order
  uint64_t ret_accept = 0, ret_reject = 0;

  explicit Bench(Vtb_terrain_downstream* t) : tb(t) {}

  void reset() {
    tb->tri_valid_i = 0;
    tb->out_ready_i = 1;
    tb->vp_x0_i = 0;
    tb->vp_y0_i = 0;
    tb->vp_w_i = 256;
    tb->vp_h_i = 192;
    tb->cull_mode_i = 0;
    tb->tri_attr_a_i = 0xA;
    tb->tri_attr_b_i = 0xB;
    tb->tri_attr_c_i = 0xC;
    tb->rst_n = 0;
    for (int i = 0; i < 3; ++i) step_edge();
    tb->rst_n = 1;
    step_edge();
  }

  void present(const InTri& t) {
    tb->tri_valid_i = 1;
    tb->tri_ax_i = m21(t.x[0]);
    tb->tri_ay_i = m21(t.y[0]);
    tb->tri_bx_i = m21(t.x[1]);
    tb->tri_by_i = m21(t.y[1]);
    tb->tri_cx_i = m21(t.x[2]);
    tb->tri_cy_i = m21(t.y[2]);
    tb->tri_behind_i = t.behind;
    tb->tri_src_id_i = t.src;
  }

  // One cycle: settle, sample handshakes/outputs, edge. Returns whether the
  // presented triangle was taken.
  bool step(uint32_t stall_pct) {
    tb->out_ready_i = (stall_pct != 0 && rng.chance(stall_pct)) ? 0 : 1;
    tb->clk = 0;
    tb->eval();
    const bool take = tb->tri_valid_i && tb->tri_ready_o;
    if (tb->out_valid_o && tb->out_ready_i) got.push_back(tb->out_src_id_o);
    if (tb->ret_valid_o) {
      if (tb->ret_verdict_o == 0) ++ret_accept; else ++ret_reject;
    }
    step_edge();
    return take;
  }

  // Drive a whole stream at the front, one per clock when accepted; returns
  // the number of cycles from the first presentation to the last OUTPUT.
  uint64_t run(const std::vector<InTri>& in, uint32_t stall_pct, size_t expect_out) {
    const uint64_t c0 = cycle;
    size_t i = 0;
    uint64_t last_out_cycle = c0;
    const size_t out0 = got.size();
    int guard = 0;
    while (got.size() < out0 + expect_out && guard++ < 400000) {
      if (i < in.size()) present(in[i]); else tb->tri_valid_i = 0;
      const size_t before = got.size();
      const bool take = step(stall_pct);
      if (take) ++i;
      if (got.size() != before) last_out_cycle = cycle;
    }
    tb->tri_valid_i = 0;
    check(guard < 400000, "downstream: the stream drained", 1, guard < 400000 ? 1 : 0);
    return last_out_cycle - c0;
  }

 private:
  void step_edge() {
    tb->clk = 1;
    tb->eval();
    tb->clk = 0;
    tb->eval();
    ++cycle;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_terrain_downstream;
  Bench b(top);
  b.reset();

  // ---- 1. one triangle per clock, consumer always ready ------------------
  constexpr int kN = 4096;
  {
    const std::vector<InTri> in = terrain_stream(kN, 0x1000);
    const uint64_t cycles = b.run(in, 0, kN);
    std::printf("MEASURED downstream (GEOM.CLIP -> GEOM.SETUP): %d accepted triangles in %llu clocks "
                "with the consumer always ready = %.4f clocks/triangle (fixed fill %lld)\n",
                kN, static_cast<unsigned long long>(cycles),
                static_cast<double>(cycles) / kN, static_cast<long long>(cycles) - kN);
    // 3 + 3 stages of fill; anything under N + 16 is one per clock.
    check(cycles >= static_cast<uint64_t>(kN), "downstream: not faster than one per clock (sanity)",
          kN, cycles);
    check(cycles <= static_cast<uint64_t>(kN) + 16, "downstream: one triangle per clock sustained",
          kN + 16, cycles);
    check(b.got.size() == static_cast<size_t>(kN), "downstream: every triangle came out", kN,
          b.got.size());
    bool in_order = true;
    for (int i = 0; i < kN; ++i) in_order = in_order && b.got[static_cast<size_t>(i)] == 0x1000 + i;
    check(in_order, "downstream: in order with src_id", 1, in_order ? 1 : 0);
    check(top->clip_submitted_o == static_cast<uint32_t>(kN), "clip counter: submitted", kN,
          top->clip_submitted_o);
    check(top->setup_submitted_o == static_cast<uint32_t>(kN), "setup counter: submitted", kN,
          top->setup_submitted_o);
    check(b.ret_accept == static_cast<uint64_t>(kN), "clip verdicts: all accepted", kN, b.ret_accept);
    b.got.clear();
  }

  // ---- 2. rejections cost nothing extra ---------------------------------
  {
    std::vector<InTri> in = terrain_stream(kN, 0x3000);
    int rejects = 0;
    for (size_t i = 0; i < in.size(); ++i) {
      switch (i % 8) {
        case 1: in[i].behind = 1; in[i].expect_accept = false; ++rejects; break;         // near plane
        case 3: for (int k = 0; k < 3; ++k) in[i].x[k] += 300 * 256; in[i].expect_accept = false; ++rejects; break;  // off-screen
        case 5: in[i].x[1] = in[i].x[0]; in[i].y[1] = in[i].y[0]; in[i].expect_accept = false; ++rejects; break;   // degenerate
        default: break;
      }
    }
    const int accepted = kN - rejects;
    const uint64_t r0 = b.ret_reject;
    const uint64_t cycles = b.run(in, 0, static_cast<size_t>(accepted));
    std::printf("MEASURED downstream with %d of %d rejected: %llu clocks = %.4f clocks per OFFERED triangle\n",
                rejects, kN, static_cast<unsigned long long>(cycles),
                static_cast<double>(cycles) / kN);
    check(cycles <= static_cast<uint64_t>(kN) + 16, "downstream: rejects cost no extra clocks",
          kN + 16, cycles);
    check(b.ret_reject - r0 == static_cast<uint64_t>(rejects), "clip verdicts: every reject retired",
          rejects, b.ret_reject - r0);
    check(b.got.size() == static_cast<size_t>(accepted), "downstream: only accepted triangles out",
          accepted, b.got.size());
    size_t gi = 0;
    bool order_ok = true;
    for (const InTri& t : in)
      if (t.expect_accept) order_ok = order_ok && gi < b.got.size() && b.got[gi++] == t.src;
    check(order_ok, "downstream: accepted stream in order", 1, order_ok ? 1 : 0);
    b.got.clear();
  }

  // ---- 3. backpressure: nothing lost, nothing reordered -------------------
  {
    const std::vector<InTri> in = terrain_stream(kN, 0x5000);
    const uint64_t cycles = b.run(in, 30, kN);
    std::printf("downstream under a 30%% consumer stall: %d triangles in %llu clocks (%.3f/triangle)\n",
                kN, static_cast<unsigned long long>(cycles), static_cast<double>(cycles) / kN);
    check(b.got.size() == static_cast<size_t>(kN), "backpressure: every triangle came out", kN,
          b.got.size());
    bool in_order = true;
    for (int i = 0; i < kN; ++i) in_order = in_order && b.got[static_cast<size_t>(i)] == 0x5000 + i;
    check(in_order, "backpressure: in order", 1, in_order ? 1 : 0);
    // A stalled consumer at 30 % must cost roughly 30 % more clocks, never less
    // than the unstalled run: the check is that the stall REACHED the pipe.
    check(cycles > static_cast<uint64_t>(kN) + 16, "backpressure: the stall was felt", kN + 16, cycles);
  }

  top->final();
  return zhao::report_and_exit("terrain_downstream_rate");
}
