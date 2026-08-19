// texture_mosaic_dev.hpp — the shared driver for TEXTURE.MOSAIC.
//
// One place that knows the block's handshake, so the directed lane and both
// random lanes drive it identically. If the port list changes, exactly one
// file needs editing and every lane fails to compile until it is. (Same shape
// as tests/terrain/project_dev.hpp and tests/surface/surface_dev.hpp.)

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_texture_mosaic.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace mosaic_test {

/** One `mosaic_candidates` packet: the layer-E triple plus the fragment UV. */
struct Req {
  int32_t u = 0;  // Q16.16 tile units
  int32_t v = 0;
  uint8_t mat_a = 0;
  uint8_t mat_b = 0;
  uint8_t weight = 0;
  bool mosaic = true;  // TextureSpan::mosaic — false on walls/underside
  uint16_t src_id = 0;
};

/** One `mosaic_pick` packet. */
struct Pick {
  uint8_t tile = 0;
  uint8_t tx = 0;
  uint8_t ty = 0;
  uint16_t src_id = 0;
  bool operator==(const Pick& o) const {
    return tile == o.tile && tx == o.tx && ty == o.ty && src_id == o.src_id;
  }
};

/**
 * What the FROZEN §6.2 laws say the block must produce, composed exactly the
 * way `raster_tri`'s TextureSpan branch composes them: `mirror_texel` on each
 * axis for the texel pair, and `mosaic_pick` on the UNFOLDED world indices
 * `u >> 10` / `v >> 10` for the id. Both calls are the ratified header, never
 * a re-derivation.
 */
inline Pick oracle(const Req& r) {
  Pick p;
  p.tx = static_cast<uint8_t>(zref::terrain::mirror_texel(r.u));
  p.ty = static_cast<uint8_t>(zref::terrain::mirror_texel(r.v));
  p.tile = r.mosaic ? zref::terrain::mosaic_pick(r.mat_a, r.mat_b, r.weight, r.u >> 10, r.v >> 10)
                    : r.mat_a;
  p.src_id = r.src_id;
  return p;
}

/** The p that §6.2's hash produces at one world texel — the pick's decider. */
inline uint32_t oracle_p(int32_t mx, int32_t my) {
  const uint32_t hx = static_cast<uint32_t>(mx) * 73856093u;
  const uint32_t hy = static_cast<uint32_t>(my) * 19349663u;
  return (hx ^ hy) % 255u;
}

/**
 * Find a world texel whose hash residue is EXACTLY `want_p`, searching a
 * spiral of small indices. The random lanes cannot be trusted to stumble onto
 * `p == weight`: with p uniform over 255 values a uniform weight hits the
 * boundary about once in 255 draws in ONE direction only, and the two
 * directions (p == weight, p == weight-1) are what separate `<` from `<=`.
 * They are CONSTRUCTED here instead of hoped for.
 *
 * Returns false if the window holds no such texel (it never does for
 * want_p <= 254 at this size, and the caller asserts on the return value so a
 * silent no-op cannot pretend to be coverage).
 */
inline bool find_texel_with_p(uint32_t want_p, int32_t* mx, int32_t* my) {
  for (int32_t y = -32; y < 32; ++y) {
    for (int32_t x = -32; x < 32; ++x) {
      if (oracle_p(x, y) == want_p) {
        *mx = x;
        *my = y;
        return true;
      }
    }
  }
  return false;
}

/** Q16.16 tile-unit u whose world texel index (u >> 10) is exactly `m`. */
inline int32_t u_for_texel(int32_t m) { return m << 10; }

/** The DUT driver: reset, push packets, collect packets. */
class Dev {
 public:
  explicit Dev(Vzhao_texture_mosaic& dut) : dut_(dut) {}

  void reset() {
    dut_.rst_n = 0;
    dut_.req_valid_i = 0;
    dut_.pick_ready_i = 0;
    dut_.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
    zhao::tick(dut_);
  }

  uint32_t samples() const { return dut_.texture_samples_o; }
  bool idle() const { return dut_.idle_o != 0; }

  /**
   * Push every packet and collect every result.
   *
   * `stall_mask` is rotated one bit per cycle onto `pick_ready_i`; 0 means the
   * consumer is always ready. `cycles_out`, when given, receives the number of
   * clocks from the first accepted packet to the last collected one — how the
   * throughput claim is MEASURED rather than asserted. `first_latency_out`
   * receives the accept-to-retire distance of the first packet, which is the
   * ledger's `fixed:2`.
   */
  std::vector<Pick> run(const std::vector<Req>& in, uint32_t stall_mask = 0,
                        int* cycles_out = nullptr, int* first_latency_out = nullptr) {
    std::vector<Pick> out;
    out.reserve(in.size());
    size_t pushed = 0;
    int first_accept = -1;
    int first_retire = -1;
    int last_cycle = 0;
    const int limit = static_cast<int>(in.size()) * 64 + 4096;

    for (int cycle = 0; cycle < limit; ++cycle) {
      const bool ready = (stall_mask == 0) || (((stall_mask >> (cycle & 31)) & 1u) == 0);
      dut_.pick_ready_i = ready ? 1 : 0;

      if (pushed < in.size()) {
        const Req& r = in[pushed];
        dut_.req_valid_i = 1;
        dut_.req_u_i = r.u;
        dut_.req_v_i = r.v;
        dut_.req_mat_a_i = r.mat_a;
        dut_.req_mat_b_i = r.mat_b;
        dut_.req_weight_i = r.weight;
        dut_.req_mosaic_i = r.mosaic ? 1 : 0;
        dut_.req_src_id_i = r.src_id;
      } else {
        dut_.req_valid_i = 0;
        // Poison the port once the stream is done: a block that latched a
        // stale packet would show it as an extra output.
        dut_.req_u_i = 0x5BADF00D;
        dut_.req_v_i = 0x5BADF00D;
        dut_.req_mat_a_i = 0xA5;
        dut_.req_mat_b_i = 0x5A;
        dut_.req_weight_i = 0xC3;
        dut_.req_src_id_i = 0xDEAD;
      }

      dut_.eval();
      const bool take = (pushed < in.size()) && dut_.req_ready_o;
      if (dut_.pick_valid_o && ready) {
        Pick p;
        p.tile = static_cast<uint8_t>(dut_.pick_tile_o);
        p.tx = static_cast<uint8_t>(dut_.pick_tx_o);
        p.ty = static_cast<uint8_t>(dut_.pick_ty_o);
        p.src_id = static_cast<uint16_t>(dut_.pick_src_id_o);
        out.push_back(p);
        if (first_retire < 0) first_retire = cycle;
        last_cycle = cycle;
      }
      zhao::tick(dut_);
      if (take) {
        if (first_accept < 0) first_accept = cycle;
        ++pushed;
      }
      if (out.size() == in.size()) break;
    }
    dut_.req_valid_i = 0;
    dut_.pick_ready_i = 0;
    dut_.eval();
    if (cycles_out != nullptr)
      *cycles_out = (first_accept < 0) ? 0 : (last_cycle - first_accept + 1);
    if (first_latency_out != nullptr) {
      *first_latency_out =
          (first_accept < 0 || first_retire < 0) ? -1 : (first_retire - first_accept);
    }
    return out;
  }

 private:
  Vzhao_texture_mosaic& dut_;
};

}  // namespace mosaic_test
