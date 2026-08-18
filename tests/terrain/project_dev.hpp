// project_dev.hpp — the shared driver for TERRAIN.PROJECT.
//
// One place that knows the block's handshake, so the directed lane, the two
// random lanes and the composition all drive it identically. If the port list
// changes, exactly one file needs editing and every lane fails to compile
// until it is.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_terrain_project.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zrender/internal.hpp"  // white-box: project_vertex IS the law

namespace project_test {

/** One terrain_primitives packet: TERRAIN.NORMALS' triangle, plus the riders. */
struct TriIn {
  int32_t ax = 0, ay = 0, az = 0;
  int32_t bx = 0, by = 0, bz = 0;
  int32_t cx = 0, cy = 0, cz = 0;
  uint16_t src_id = 0;
  uint8_t view = 0;
  uint8_t mat_a = 0, mat_b = 0, weight = 0;
};

/** One projected triangle: GEOM.CLIP's input packet, plus the riders. */
struct TriOut {
  int32_t x[3] = {0, 0, 0};
  int32_t y[3] = {0, 0, 0};
  int32_t d[3] = {0, 0, 0};
  uint8_t behind = 0;
  uint16_t src_id = 0;
  uint8_t view = 0;
  uint8_t mat_a = 0, mat_b = 0, weight = 0;
};

/** What `project_vertex` says the block must produce for one packet. */
inline TriOut oracle(const TriIn& t, const zref::mat4fx& m, const zref::render::Viewport& vp,
                     zref::SatLedger* L = nullptr) {
  const int32_t wx[3] = {t.ax, t.bx, t.cx};
  const int32_t wy[3] = {t.ay, t.by, t.cy};
  const int32_t wz[3] = {t.az, t.bz, t.cz};
  TriOut o;
  for (int k = 0; k < 3; ++k) {
    const zref::render::ProjOut p = zref::render::project_vertex(
        m, vp, zref::fx16{wx[k]}, zref::fx16{wy[k]}, zref::fx16{wz[k]}, L);
    // `project_vertex` returns a default ProjOut on the near-plane branch, so
    // a behind-the-eye vertex carries {0,0,0} and not stale coordinates.
    o.x[k] = p.s.x;
    o.y[k] = p.s.y;
    o.d[k] = p.s.d;
    if (!p.in) o.behind = static_cast<uint8_t>(o.behind | (1u << k));
  }
  o.src_id = t.src_id;
  o.view = t.view;
  o.mat_a = t.mat_a;
  o.mat_b = t.mat_b;
  o.weight = t.weight;
  return o;
}

/**
 * Sign-extend a 21-bit screen coordinate.
 *
 * Verilator hands a `logic signed [20:0]` port back as a raw 21-bit word in a
 * uint32; taking it as an int32 makes every negative coordinate a large
 * positive one. GEOM.CLIP's port is the same width, so the two blocks agree in
 * hardware and it is only the C++ view that needs this.
 */
inline int32_t sx21(uint32_t v) {
  const uint32_t m = v & 0x1FFFFFu;
  return static_cast<int32_t>((m ^ 0x100000u) - 0x100000u);
}

/** The DUT driver: reset, configure a view, push packets, collect packets. */
class Dev {
 public:
  explicit Dev(Vzhao_terrain_project& dut) : dut_(dut) {}

  void reset() {
    dut_.rst_n = 0;
    dut_.cfg_we_i = 0;
    dut_.tri_valid_i = 0;
    dut_.out_ready_i = 0;
    dut_.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
    zhao::tick(dut_);
  }

  /** Write one view's matrix and viewport through the config port. */
  void configure(int view, const zref::mat4fx& m, const zref::render::Viewport& vp) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        write_cfg(view, static_cast<uint8_t>(r * 4 + c), static_cast<uint32_t>(m.m[r][c].raw));
      }
    }
    write_cfg(view, 16, (vp.y0 << 16) | (vp.x0 & 0xFFFFu));
    write_cfg(view, 17, (vp.h << 16) | (vp.w & 0xFFFFu));
  }

  void write_cfg(int view, uint8_t addr, uint32_t data) {
    dut_.cfg_we_i = 1;
    dut_.cfg_view_i = static_cast<uint8_t>(view);
    dut_.cfg_addr_i = addr;
    dut_.cfg_data_i = data;
    dut_.eval();
    zhao::tick(dut_);
    dut_.cfg_we_i = 0;
    dut_.eval();
  }

  /**
   * Push every packet and collect every result.
   *
   * `stall_mask` is rotated one bit per cycle onto `out_ready_i`; 0 means the
   * consumer is always ready. `cycles_out`, when given, receives the number of
   * clocks from the first accepted packet to the last collected one, which is
   * how the throughput claim in the contract is measured rather than asserted.
   */
  std::vector<TriOut> run(const std::vector<TriIn>& in, uint32_t stall_mask = 0,
                          int* cycles_out = nullptr) {
    std::vector<TriOut> out;
    out.reserve(in.size());
    size_t pushed = 0;
    int first_cycle = -1;
    int last_cycle = 0;
    const int limit = static_cast<int>(in.size()) * 64 + 4096;

    for (int cycle = 0; cycle < limit; ++cycle) {
      const bool ready = (stall_mask == 0) || (((stall_mask >> (cycle & 31)) & 1u) == 0);
      dut_.out_ready_i = ready ? 1 : 0;

      if (pushed < in.size()) {
        const TriIn& t = in[pushed];
        dut_.tri_valid_i = 1;
        dut_.ax_i = t.ax;
        dut_.ay_i = t.ay;
        dut_.az_i = t.az;
        dut_.bx_i = t.bx;
        dut_.by_i = t.by;
        dut_.bz_i = t.bz;
        dut_.cx_i = t.cx;
        dut_.cy_i = t.cy;
        dut_.cz_i = t.cz;
        dut_.src_id_i = t.src_id;
        dut_.view_i = t.view;
        dut_.mat_a_i = t.mat_a;
        dut_.mat_b_i = t.mat_b;
        dut_.weight_i = t.weight;
      } else {
        dut_.tri_valid_i = 0;
        // Poison the port once the stream is done: a block that latched a
        // stale packet would show it as an extra output.
        dut_.ax_i = 0x5BADF00D;
        dut_.by_i = 0x5BADF00D;
        dut_.cz_i = 0x5BADF00D;
      }

      dut_.eval();
      const bool take = (pushed < in.size()) && dut_.tri_ready_o;
      if (dut_.out_valid_o && ready) {
        TriOut o;
        o.x[0] = sx21(dut_.out_ax_o);
        o.y[0] = sx21(dut_.out_ay_o);
        o.x[1] = sx21(dut_.out_bx_o);
        o.y[1] = sx21(dut_.out_by_o);
        o.x[2] = sx21(dut_.out_cx_o);
        o.y[2] = sx21(dut_.out_cy_o);
        o.d[0] = static_cast<int32_t>(dut_.out_ad_o);
        o.d[1] = static_cast<int32_t>(dut_.out_bd_o);
        o.d[2] = static_cast<int32_t>(dut_.out_cd_o);
        o.behind = static_cast<uint8_t>(dut_.out_behind_o);
        o.src_id = static_cast<uint16_t>(dut_.out_src_id_o);
        o.view = static_cast<uint8_t>(dut_.out_view_o);
        o.mat_a = static_cast<uint8_t>(dut_.out_mat_a_o);
        o.mat_b = static_cast<uint8_t>(dut_.out_mat_b_o);
        o.weight = static_cast<uint8_t>(dut_.out_weight_o);
        out.push_back(o);
        last_cycle = cycle;
      }
      zhao::tick(dut_);
      if (take) {
        if (first_cycle < 0) first_cycle = cycle;
        ++pushed;
      }
      if (out.size() == in.size()) break;
    }
    dut_.tri_valid_i = 0;
    dut_.out_ready_i = 0;
    dut_.eval();
    if (cycles_out != nullptr) *cycles_out = (first_cycle < 0) ? 0 : (last_cycle - first_cycle + 1);
    return out;
  }

 private:
  Vzhao_terrain_project& dut_;
};

/** Row-major fx16 matrix from sixteen raws. */
inline zref::mat4fx mat_of(const int32_t (&m)[16]) {
  zref::mat4fx r{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) r.m[i][j] = zref::fx16{m[i * 4 + j]};
  }
  return r;
}

/**
 * The fixture perspective matrix: x' = kx·x, y' = ky·y, w = z.
 *
 * This is `render_helpers.hpp::persp2x`'s shape, generalised. World Y is up
 * (qformats §9) and world Z runs away from the eye, so a vertex at z = 0 sits
 * exactly on the near plane and `clip.w <= 0` rejects it — which is what makes
 * this fixture able to reach the behind-the-eye branch by moving one number.
 */
inline zref::mat4fx persp(int32_t kx, int32_t ky) {
  const int32_t m[16] = {kx, 0, 0, 0, 0, ky, 0, 0, 0, 0, 1 << 16, 0, 0, 0, 1 << 16, 0};
  return mat_of(m);
}

}  // namespace project_test
