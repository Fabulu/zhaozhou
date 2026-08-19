// geom_dev.hpp — Verilator drivers for the GEOM.CLIP / GEOM.SETUP /
// GEOM.BINNER differential tests (design/contracts/GEOM.*.md).
//
// The oracles are zref::Clip, zref::Setup and zref::Binner
// (reference/include/zref/zref_geom.hpp) — the ledger's declared
// reference_models. This file contains no clip law, no edge decomposition and
// no binning rule at all: it drives the RTL and compares packets.
//
// Each driver is compiled only where its Verilated model exists, selected by
// the ZHAO_GEOM_DEV_* macro the translation unit defines before including.

#pragma once

#include <cstdint>
#include <functional>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_geom.hpp"

namespace zhao_geom {

// PCG RXS-M-XS — the committed test PRNG shape (qformats.md §7.5 constants),
// the same one every other random lane in this tree uses.
inline uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = static_cast<uint32_t>(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct Prng {
  uint32_t state;
  explicit Prng(uint32_t seed) : state(seed) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
  int32_t span(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(draw() % static_cast<uint32_t>(hi - lo + 1));
  }
};

// ---- raw <-> signed helpers for the Verilated ports ------------------------
inline uint32_t m21(int32_t v) { return static_cast<uint32_t>(v) & 0x1FFFFFu; }
inline uint32_t m23(int32_t v) { return static_cast<uint32_t>(v) & 0x7FFFFFu; }
inline uint32_t m12(int32_t v) { return static_cast<uint32_t>(v) & 0xFFFu; }
inline uint64_t m48(int64_t v) { return static_cast<uint64_t>(v) & 0xFFFFFFFFFFFFull; }

inline int32_t s12(uint32_t raw) {
  const uint32_t v = raw & 0xFFFu;
  return (v & 0x800u) ? static_cast<int32_t>(v | 0xFFFFF000u) : static_cast<int32_t>(v);
}
inline int32_t s21(uint32_t raw) {
  const uint32_t v = raw & 0x1FFFFFu;
  return (v & 0x100000u) ? static_cast<int32_t>(v | 0xFFE00000u) : static_cast<int32_t>(v);
}
inline int32_t s23(uint32_t raw) {
  const uint32_t v = raw & 0x7FFFFFu;
  return (v & 0x400000u) ? static_cast<int32_t>(v | 0xFF800000u) : static_cast<int32_t>(v);
}
inline int64_t s48(uint64_t raw) {
  const uint64_t v = raw & 0xFFFFFFFFFFFFull;
  return (v & 0x800000000000ull) ? static_cast<int64_t>(v | 0xFFFF000000000000ull)
                                 : static_cast<int64_t>(v);
}

inline void add_err(std::string* err, const char* what) {
  if (err->empty()) *err = what;
}

// One full clock cycle: settle low, rising edge, settle low.
template <typename Top>
inline void edge(Top& t) {
  t.clk = 0;
  t.eval();
  t.clk = 1;
  t.eval();
  t.clk = 0;
  t.eval();
}

}  // namespace zhao_geom

// ===========================================================================
#ifdef ZHAO_GEOM_DEV_CLIP
#include "Vzhao_geom_clip.h"

namespace zhao_geom {

/** Drives zhao_geom_clip through one triangle and returns its verdict/packet. */
class ClipDev {
 public:
  ClipDev() { reset(); }
  ~ClipDev() { top_.final(); }
  ClipDev(const ClipDev&) = delete;
  ClipDev& operator=(const ClipDev&) = delete;

  void reset() {
    top_.rst_n = 0;
    park();
    top_.eval();
    for (int i = 0; i < 2; ++i) edge(top_);
    top_.rst_n = 1;
    top_.eval();
  }

  /**
   * One triangle. `stall_seed` != 0 gates out_ready_i with a PCG bit stream
   * (the backpressure lane); 0 means "always ready".
   */
  zref::Clip::Out run(const zref::Clip::In& t, const zref::Clip::Viewport& vp,
                      zref::Clip::CullMode cull, uint16_t src_id, uint32_t stall_seed,
                      std::string* err) {
    zref::Clip::Out out;
    park();
    top_.vp_x0_i = static_cast<uint32_t>(vp.x0);
    top_.vp_y0_i = static_cast<uint32_t>(vp.y0);
    top_.vp_w_i = static_cast<uint32_t>(vp.w);
    top_.vp_h_i = static_cast<uint32_t>(vp.h);
    top_.cull_mode_i = static_cast<uint32_t>(cull);
    top_.tri_ax_i = m21(t.ax);
    top_.tri_ay_i = m21(t.ay);
    top_.tri_bx_i = m21(t.bx);
    top_.tri_by_i = m21(t.by);
    top_.tri_cx_i = m21(t.cx);
    top_.tri_cy_i = m21(t.cy);
    top_.tri_behind_i = t.behind & 7u;
    top_.tri_src_id_i = src_id;
    top_.tri_valid_i = 1;
    top_.eval();

    // offer until accepted
    for (int guard = 0; !top_.tri_ready_o; ++guard) {
      if (guard > 64) {
        add_err(err, "tri_ready_o never asserted");
        return out;
      }
      edge(top_);
      top_.eval();
    }
    edge(top_);
    top_.tri_valid_i = 0;
    top_.eval();

    // collect: the retire pulse carries the verdict, the output port the packet
    uint32_t rng = stall_seed;
    bool held = false;
    bool got_ret = false;
    for (int guard = 0; !got_ret; ++guard) {
      if (guard > 64) {
        add_err(err, "no retire pulse");
        return out;
      }
      const bool rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      top_.out_ready_i = rdy ? 1 : 0;
      top_.eval();

      if (top_.out_valid_o) {
        if (held && !same_packet()) add_err(err, "stalled packet changed while !out_ready_i");
        snap_packet();
        held = !rdy;
      } else {
        if (held) add_err(err, "out_valid_o dropped while a packet was stalled");
        held = false;
      }

      if (top_.ret_valid_o) {
        out.verdict = static_cast<zref::Clip::Verdict>(top_.ret_verdict_o);
        if ((out.verdict == zref::Clip::kAccept) != (top_.out_valid_o != 0))
          add_err(err, "out_valid_o disagrees with the verdict");
        if (out.verdict == zref::Clip::kAccept) {
          if (top_.out_src_id_o != src_id) add_err(err, "out_src_id_o mismatch");
          out.ax = s21(top_.out_ax_o);
          out.ay = s21(top_.out_ay_o);
          out.bx = s21(top_.out_bx_o);
          out.by = s21(top_.out_by_o);
          out.cx = s21(top_.out_cx_o);
          out.cy = s21(top_.out_cy_o);
          out.area2 = s48(top_.out_area2_o);
          out.min_x = s12(top_.out_min_x_o);
          out.max_x = s12(top_.out_max_x_o);
          out.min_y = s12(top_.out_min_y_o);
          out.max_y = s12(top_.out_max_y_o);
        }
        got_ret = true;
      }
      edge(top_);
    }
    park();
    top_.eval();
    return out;
  }

  uint32_t submitted() const { return top_.triangles_submitted_o; }
  uint32_t clipped() const { return top_.triangles_clipped_o; }
  uint32_t culled() const { return top_.triangles_culled_o; }

 private:
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    return pcg_perm(*s);
  }
  void park() {
    top_.tri_valid_i = 0;
    top_.out_ready_i = 1;
  }
  void snap_packet() {
    snap_[0] = top_.out_ax_o;
    snap_[1] = top_.out_ay_o;
    snap_[2] = top_.out_bx_o;
    snap_[3] = top_.out_by_o;
    snap_[4] = top_.out_cx_o;
    snap_[5] = top_.out_cy_o;
    snap_[6] = top_.out_min_x_o;
    snap_[7] = top_.out_max_x_o;
    snap_[8] = top_.out_min_y_o;
    snap_[9] = top_.out_max_y_o;
  }
  bool same_packet() const {
    return snap_[0] == top_.out_ax_o && snap_[1] == top_.out_ay_o && snap_[2] == top_.out_bx_o &&
           snap_[3] == top_.out_by_o && snap_[4] == top_.out_cx_o && snap_[5] == top_.out_cy_o &&
           snap_[6] == top_.out_min_x_o && snap_[7] == top_.out_max_x_o &&
           snap_[8] == top_.out_min_y_o && snap_[9] == top_.out_max_y_o;
  }

  uint32_t snap_[10] = {};
  Vzhao_geom_clip top_;
};

}  // namespace zhao_geom
#endif  // ZHAO_GEOM_DEV_CLIP

// ===========================================================================
#ifdef ZHAO_GEOM_DEV_SETUP
#include "Vzhao_geom_setup.h"

namespace zhao_geom {

/** Drives zhao_geom_setup through one winding-normalised triangle. */
class SetupDev {
 public:
  SetupDev() { reset(); }
  ~SetupDev() { top_.final(); }
  SetupDev(const SetupDev&) = delete;
  SetupDev& operator=(const SetupDev&) = delete;

  void reset() {
    top_.rst_n = 0;
    park();
    top_.eval();
    for (int i = 0; i < 2; ++i) edge(top_);
    top_.rst_n = 1;
    top_.eval();
  }

  zref::Setup::Out run(const zref::Clip::Out& c, uint16_t src_id, uint32_t stall_seed,
                       std::string* err) {
    zref::Setup::Out out;
    park();
    top_.tri_ax_i = m21(c.ax);
    top_.tri_ay_i = m21(c.ay);
    top_.tri_bx_i = m21(c.bx);
    top_.tri_by_i = m21(c.by);
    top_.tri_cx_i = m21(c.cx);
    top_.tri_cy_i = m21(c.cy);
    top_.tri_area2_i = m48(c.area2);
    top_.tri_min_x_i = m12(c.min_x);
    top_.tri_max_x_i = m12(c.max_x);
    top_.tri_min_y_i = m12(c.min_y);
    top_.tri_max_y_i = m12(c.max_y);
    top_.tri_src_id_i = src_id;
    top_.tri_valid_i = 1;
    top_.eval();
    for (int guard = 0; !top_.tri_ready_o; ++guard) {
      if (guard > 64) {
        add_err(err, "tri_ready_o never asserted");
        return out;
      }
      edge(top_);
      top_.eval();
    }
    edge(top_);
    top_.tri_valid_i = 0;
    top_.eval();

    uint32_t rng = stall_seed;
    bool held = false;
    for (int guard = 0;; ++guard) {
      if (guard > 64) {
        add_err(err, "no output packet");
        return out;
      }
      const bool rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      top_.out_ready_i = rdy ? 1 : 0;
      top_.eval();
      if (top_.out_valid_o) {
        if (held && !same_packet()) add_err(err, "stalled packet changed while !out_ready_i");
        snap_packet();
        if (rdy) {
          out.area2 = s48(top_.out_area2_o);
          out.e[0].kx = s23(top_.out_kx0_o);
          out.e[0].ky = s23(top_.out_ky0_o);
          out.e[0].kc = s48(top_.out_kc0_o);
          out.e[1].kx = s23(top_.out_kx1_o);
          out.e[1].ky = s23(top_.out_ky1_o);
          out.e[1].kc = s48(top_.out_kc1_o);
          out.e[2].kx = s23(top_.out_kx2_o);
          out.e[2].ky = s23(top_.out_ky2_o);
          out.e[2].kc = s48(top_.out_kc2_o);
          for (int i = 0; i < 3; ++i) out.e[i].tl = ((top_.out_tl_o >> i) & 1u) != 0u;
          if (top_.out_src_id_o != src_id) add_err(err, "out_src_id_o mismatch");
          if (s21(top_.out_ax_o) != c.ax || s21(top_.out_ay_o) != c.ay ||
              s21(top_.out_bx_o) != c.bx || s21(top_.out_by_o) != c.by ||
              s21(top_.out_cx_o) != c.cx || s21(top_.out_cy_o) != c.cy)
            add_err(err, "vertex passthrough corrupted");
          if (s12(top_.out_min_x_o) != c.min_x || s12(top_.out_max_x_o) != c.max_x ||
              s12(top_.out_min_y_o) != c.min_y || s12(top_.out_max_y_o) != c.max_y)
            add_err(err, "scan box passthrough corrupted");
          edge(top_);
          break;
        }
        held = true;
      } else {
        if (held) add_err(err, "out_valid_o dropped while a packet was stalled");
        held = false;
      }
      edge(top_);
    }
    park();
    top_.eval();
    return out;
  }

  uint32_t submitted() const { return top_.triangles_submitted_o; }

 private:
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    return pcg_perm(*s);
  }
  void park() {
    top_.tri_valid_i = 0;
    top_.out_ready_i = 1;
  }
  void snap_packet() {
    snap_[0] = top_.out_kx0_o;
    snap_[1] = top_.out_ky0_o;
    snap_[2] = top_.out_kx1_o;
    snap_[3] = top_.out_ky1_o;
    snap_[4] = top_.out_kx2_o;
    snap_[5] = top_.out_ky2_o;
    snap_[6] = top_.out_tl_o;
    snapq_[0] = top_.out_kc0_o;
    snapq_[1] = top_.out_kc1_o;
    snapq_[2] = top_.out_kc2_o;
  }
  bool same_packet() const {
    return snap_[0] == top_.out_kx0_o && snap_[1] == top_.out_ky0_o && snap_[2] == top_.out_kx1_o &&
           snap_[3] == top_.out_ky1_o && snap_[4] == top_.out_kx2_o && snap_[5] == top_.out_ky2_o &&
           snap_[6] == top_.out_tl_o && snapq_[0] == top_.out_kc0_o &&
           snapq_[1] == top_.out_kc1_o && snapq_[2] == top_.out_kc2_o;
  }

  uint32_t snap_[7] = {};
  uint64_t snapq_[3] = {};
  Vzhao_geom_setup top_;
};

}  // namespace zhao_geom
#endif  // ZHAO_GEOM_DEV_SETUP

// ===========================================================================
#ifdef ZHAO_GEOM_DEV_BINNER
#include "Vzhao_geom_binner.h"

namespace zhao_geom {

/**
 * One drained (triangle × tile) job — RASTER.EDGEWALK's job port. The port
 * carries the tile's top-left PIXEL (that block's unit); `tx`/`ty` here are
 * the tile INDEX the driver derives from it, and the driver asserts the two
 * agree so an index-for-pixel confusion cannot hide behind the conversion.
 */
struct BinJob {
  int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
  int32_t tx = 0, ty = 0;  // tile index
  int32_t px = 0, py = 0;  // the raw port value: the tile's top-left pixel
  uint16_t src_id = 0;
};

/** One triangle offered to the binner. */
struct BinTri {
  zref::Setup::Out s;
  int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
  int32_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  uint16_t src_id = 0;
  bool token = true;  // MEASURE.TOKENS grant for this triangle
};

/**
 * Build one BinTri by pushing a triangle through the GEOM.CLIP and GEOM.SETUP
 * ORACLES — the same packet the real chain would hand the binner. Returns
 * false if GEOM.CLIP rejects the triangle (there is then nothing to bin).
 */
inline bool make_bin_tri(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy,
                         const zref::Clip::Viewport& vp, uint16_t src, BinTri* out) {
  zref::Clip::In in;
  in.ax = ax;
  in.ay = ay;
  in.bx = bx;
  in.by = by;
  in.cx = cx;
  in.cy = cy;
  const zref::Clip::Out c = zref::Clip::clip(in, vp, zref::Clip::kCullNone);
  if (c.verdict != zref::Clip::kAccept) return false;
  out->s = zref::Setup::setup(c.ax, c.ay, c.bx, c.by, c.cx, c.cy, c.area2);
  out->ax = c.ax;
  out->ay = c.ay;
  out->bx = c.bx;
  out->by = c.by;
  out->cx = c.cx;
  out->cy = c.cy;
  out->min_x = c.min_x;
  out->max_x = c.max_x;
  out->min_y = c.min_y;
  out->max_y = c.max_y;
  out->src_id = src;
  out->token = true;
  return true;
}

/** Counters and status read at the end of a frame. */
struct BinStatus {
  // The block's counters are CUMULATIVE (spec/counters.md §4: they saturate,
  // never wrap, and are sampled by the frame_tick shadow protocol, not reset
  // by the block). These two are therefore reported as the DELTA over this
  // frame; `max_depth` is a high-water mark and cannot be delta'd, so both its
  // before and after values are reported.
  uint32_t tile_references = 0;
  uint32_t triangles_culled = 0;
  uint32_t max_depth = 0;
  uint32_t max_depth_before = 0;
  uint32_t arena_used = 0;
  bool overflow = false;
  uint64_t bin_cycles = 0;    // cycles spent binning (frame_begin -> last accept)
  uint64_t drain_cycles = 0;  // cycles spent draining (frame_end -> drain_done)
};

/** Drives zhao_geom_binner through one whole frame. */
class BinnerDev {
 public:
  BinnerDev() { reset(); }
  ~BinnerDev() { top_.final(); }
  BinnerDev(const BinnerDev&) = delete;
  BinnerDev& operator=(const BinnerDev&) = delete;

  void reset() {
    top_.rst_n = 0;
    park();
    top_.eval();
    for (int i = 0; i < 2; ++i) edge(top_);
    top_.rst_n = 1;
    top_.eval();
    settle();
  }

  /**
   * OPTIONAL LIVE TOKEN AUTHORITY (phase 8 composition).
   *
   * Left empty — the default — every triangle's scripted `BinTri::token`
   * decides the grant, which is what every lane that is not about tokens
   * wants and what this driver has always done.
   *
   * Set, it is consulted at the one instant the protocol allows: after
   * `tri_ready_o` has risen (so `tok_req_o` is genuinely asserted) and BEFORE
   * the accepting edge, because law E of `zhao_geom_binner.sv` says
   * `tok_grant_i` "is sampled on that same edge". The callback is where a real
   * MEASURE.TOKENS instance gets driven and its combinational answer read
   * back, so the two blocks meet cycle-accurately rather than through a
   * recorded script.
   *
   * `after_edge` is called once immediately after the binner's accepting edge,
   * so a co-driven DUT can take the same edge and commit its debit.
   */
  std::function<bool(const BinTri&)> token_authority;
  std::function<void()> after_edge;

  /**
   * A whole frame: begin, offer every triangle, end, drain. `stall_seed` != 0
   * gates job_ready_i with a PCG bit stream.
   */
  std::vector<BinJob> frame(const std::vector<BinTri>& tris, int grid_w, int grid_h,
                            uint32_t stall_seed, BinStatus* st, std::string* err) {
    std::vector<BinJob> jobs;
    top_.grid_w_i = static_cast<uint32_t>(grid_w);
    top_.grid_h_i = static_cast<uint32_t>(grid_h);

    // ---- frame_begin: release the arena, clear the tile heads ------------
    park();
    top_.eval();
    const uint32_t refs0 = top_.tile_references_o;
    const uint32_t cull0 = top_.triangles_culled_o;
    const uint32_t depth0 = top_.max_tile_list_depth_o;
    top_.frame_begin_i = 1;
    top_.eval();
    edge(top_);
    top_.frame_begin_i = 0;
    top_.eval();
    settle();

    // ---- bin ------------------------------------------------------------
    uint64_t bin_cycles = 0;
    for (const BinTri& t : tris) {
      top_.tri_valid_i = 1;
      top_.tok_grant_i = t.token ? 1 : 0;
      top_.tri_kx0_i = m23(t.s.e[0].kx);
      top_.tri_ky0_i = m23(t.s.e[0].ky);
      top_.tri_kc0_i = m48(t.s.e[0].kc);
      top_.tri_kx1_i = m23(t.s.e[1].kx);
      top_.tri_ky1_i = m23(t.s.e[1].ky);
      top_.tri_kc1_i = m48(t.s.e[1].kc);
      top_.tri_kx2_i = m23(t.s.e[2].kx);
      top_.tri_ky2_i = m23(t.s.e[2].ky);
      top_.tri_kc2_i = m48(t.s.e[2].kc);
      top_.tri_tl_i = static_cast<uint32_t>((t.s.e[0].tl ? 1 : 0) | (t.s.e[1].tl ? 2 : 0) |
                                            (t.s.e[2].tl ? 4 : 0));
      top_.tri_ax_i = m21(t.ax);
      top_.tri_ay_i = m21(t.ay);
      top_.tri_bx_i = m21(t.bx);
      top_.tri_by_i = m21(t.by);
      top_.tri_cx_i = m21(t.cx);
      top_.tri_cy_i = m21(t.cy);
      top_.tri_min_x_i = m12(t.min_x);
      top_.tri_max_x_i = m12(t.max_x);
      top_.tri_min_y_i = m12(t.min_y);
      top_.tri_max_y_i = m12(t.max_y);
      top_.tri_src_id_i = t.src_id;
      top_.eval();
      int guard = 0;
      while (!top_.tri_ready_o) {
        if (++guard > 4096) {
          add_err(err, "tri_ready_o never asserted while binning");
          top_.tri_valid_i = 0;
          return jobs;
        }
        edge(top_);
        ++bin_cycles;
        top_.eval();
      }
      if (!top_.tok_req_o) add_err(err, "tok_req_o low on an accepted triangle");
      // The live-authority window: the request is up, the edge has not
      // happened. Asking here and re-evaluating is what makes the grant land
      // on the same edge the protocol names.
      if (token_authority) {
        top_.tok_grant_i = token_authority(t) ? 1 : 0;
        top_.eval();
      }
      edge(top_);
      if (after_edge) after_edge();
      ++bin_cycles;
      top_.tri_valid_i = 0;
      top_.eval();
    }
    // let the last triangle finish enumerating
    for (int i = 0; i < 4096 && !top_.tri_ready_o; ++i) {
      edge(top_);
      ++bin_cycles;
      top_.eval();
    }

    if (st) {
      st->tile_references = top_.tile_references_o - refs0;
      st->triangles_culled = top_.triangles_culled_o - cull0;
      st->max_depth = top_.max_tile_list_depth_o;
      st->max_depth_before = depth0;
      st->arena_used = top_.arena_used_o;
      st->overflow = top_.overflow_o != 0;
      st->bin_cycles = bin_cycles;
    }

    // ---- frame_end + drain ----------------------------------------------
    top_.frame_end_i = 1;
    top_.eval();
    edge(top_);
    top_.frame_end_i = 0;
    top_.eval();

    uint32_t rng = stall_seed;
    uint64_t drain_cycles = 0;
    bool held = false;
    uint32_t held_snap[9] = {};
    for (int guard = 0; guard < 400000; ++guard) {
      const bool rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      top_.job_ready_i = rdy ? 1 : 0;
      top_.eval();
      if (top_.job_valid_o) {
        const uint32_t snap[9] = {top_.job_ax_o,     top_.job_ay_o,     top_.job_bx_o,
                                  top_.job_by_o,     top_.job_cx_o,     top_.job_cy_o,
                                  top_.job_tile_x_o, top_.job_tile_y_o, top_.job_src_id_o};
        if (held) {
          for (int i = 0; i < 9; ++i)
            if (snap[i] != held_snap[i]) add_err(err, "stalled job changed while !job_ready_i");
        }
        if (rdy) {
          BinJob j;
          j.ax = s21(top_.job_ax_o);
          j.ay = s21(top_.job_ay_o);
          j.bx = s21(top_.job_bx_o);
          j.by = s21(top_.job_by_o);
          j.cx = s21(top_.job_cx_o);
          j.cy = s21(top_.job_cy_o);
          j.px = s12(top_.job_tile_x_o);
          j.py = s12(top_.job_tile_y_o);
          if ((j.px & 15) != 0 || (j.py & 15) != 0)
            add_err(err, "job_tile_x/y_o is not a 16-pixel tile origin");
          j.tx = j.px >> 4;
          j.ty = j.py >> 4;
          j.src_id = static_cast<uint16_t>(top_.job_src_id_o);
          jobs.push_back(j);
          held = false;
        } else {
          held = true;
          for (int i = 0; i < 9; ++i) held_snap[i] = snap[i];
        }
      } else {
        if (held) add_err(err, "job_valid_o dropped while a job was stalled");
        held = false;
      }
      edge(top_);
      ++drain_cycles;
      if (top_.drain_done_o) break;
    }
    top_.job_ready_i = 1;
    top_.eval();
    if (st) st->drain_cycles = drain_cycles;
    if (!jobs.empty() && drain_cycles == 0) add_err(err, "drain never completed");
    return jobs;
  }

 private:
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    return pcg_perm(*s);
  }
  void park() {
    top_.tri_valid_i = 0;
    top_.frame_begin_i = 0;
    top_.frame_end_i = 0;
    top_.tok_grant_i = 1;
    top_.job_ready_i = 1;
    top_.grid_w_i = 24;
    top_.grid_h_i = 24;
  }
  // the tile-head clear runs TILES cycles after reset / frame_begin
  void settle() {
    for (int i = 0; i < 1200 && !top_.tri_ready_o; ++i) {
      edge(top_);
      top_.eval();
    }
  }

  Vzhao_geom_binner top_;
};

}  // namespace zhao_geom
#endif  // ZHAO_GEOM_DEV_BINNER
