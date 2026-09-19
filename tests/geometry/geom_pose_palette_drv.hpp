// geom_pose_palette_drv.hpp — the stimulus for `zhao_geom_pose_palette`,
// shared by the directed suite and by the mutant's inverted-polarity control.
//
// It is a TEMPLATE on the DUT type for one reason only: the mutant is a renamed
// copy, so its Verilated class is a different type with the identical port set.
// Sharing the driver is what makes the control a statement about the DESIGN
// rather than about a second, differently-written harness.

#pragma once

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace geom_pose_palette {

constexpr int kElems = 12;
constexpr int kBones = 32;

// The derived walk, from the RTL header: six row reads plus one cycle for the
// last registered read to land. Asserted, not observed — if the measurement
// disagrees, one of the two is wrong and both get looked at.
constexpr int kWalk = 7;

// GEOM.SKIN's issue interval at MUL_LANES=3 (its own frontier table). This
// block must stay strictly under it or it silently becomes the bottleneck and
// the owner-ruled 120,000 vertices/frame fails.
constexpr int kSkinII = 12;

// The identity bind pose, fx16 — the substitution the RTL makes for an
// out-of-range or non-resident bone.
constexpr uint32_t kOneFx16 = 65536u;

inline uint32_t ident_elem(int i) { return (i == 0 || i == 5 || i == 10) ? kOneFx16 : 0u; }

// A matrix whose every element names its own (bone, element). Distinct from the
// identity in every position, so a substitution can never be mistaken for a
// correct read.
inline uint32_t pal_elem(int bone, int i) {
  return (static_cast<uint32_t>(bone + 1) << 16) | static_cast<uint32_t>(i * 7 + 3);
}

struct Offer {
  uint32_t x = 0, y = 0, z = 0;
  uint8_t w0 = 64;
  bool rigid = true;
  uint16_t b0 = 0, b1 = 0;
  uint16_t src = 0;
};

// ------------------------------------------------------------------ write --
// One palette beat. Returns the number of clocks the beat took; the RTL's write
// walk is three rows at one row per clock, so three is the law.
template <typename Dut>
int write_bone(Dut& d, int bone, int content_bone) {
  for (int i = 0; i < kElems; ++i) {
    d.wr_m_i[i] = pal_elem(content_bone, i);
  }
  d.wr_bone_i = static_cast<uint8_t>(bone);
  d.wr_valid_i = 1;
  d.eval();
  int clocks = 0;
  while (!d.wr_ready_o) {
    zhao::tick(d);
    if (++clocks > 32) break;  // hang guard
  }
  zhao::tick(d);  // the edge that completes the beat
  ++clocks;
  d.wr_valid_i = 0;
  d.eval();
  return clocks;
}

template <typename Dut>
void write_full_palette(Dut& d) {
  for (int b = 0; b < kBones; ++b) write_bone(d, b, b);
}

template <typename Dut>
void pal_begin(Dut& d) {
  d.pal_begin_i = 1;
  d.eval();
  zhao::tick(d);
  d.pal_begin_i = 0;
  d.eval();
}

// ------------------------------------------------------------------- read --
struct Result {
  uint32_t a[kElems] = {};
  uint32_t b[kElems] = {};
  uint32_t x = 0, y = 0, z = 0;
  uint8_t w0 = 0;
  bool rigid = false;
  uint16_t src = 0;
  int accept_to_valid = -1;  // clocks from the accepting edge to o_valid_o high
  bool ok = false;
};

/** Offer one vertex, wait for the offer, capture it, consume it. */
template <typename Dut>
Result run_vertex(Dut& d, const Offer& o) {
  Result r;
  d.o_ready_i = 0;
  d.v_valid_i = 1;
  d.v_x_i = o.x;
  d.v_y_i = o.y;
  d.v_z_i = o.z;
  d.v_w0_i = o.w0;
  d.v_rigid_i = o.rigid ? 1 : 0;
  d.v_bone0_i = o.b0;
  d.v_bone1_i = o.b1;
  d.v_src_id_i = o.src;
  d.eval();

  int guard = 0;
  while (!d.v_ready_o) {
    zhao::tick(d);
    if (++guard > 256) return r;
  }
  zhao::tick(d);  // accepting edge
  d.v_valid_i = 0;
  d.eval();

  int cyc = 0;
  while (!d.o_valid_o) {
    zhao::tick(d);
    if (++cyc > 256) return r;
  }
  r.accept_to_valid = cyc;
  for (int i = 0; i < kElems; ++i) {
    r.a[i] = d.a_m_o[i];
    r.b[i] = d.b_m_o[i];
  }
  r.x = d.o_x_o;
  r.y = d.o_y_o;
  r.z = d.o_z_o;
  r.w0 = d.o_w0_o;
  r.rigid = d.o_rigid_o != 0;
  r.src = d.o_src_id_o;

  d.o_ready_i = 1;
  d.eval();
  zhao::tick(d);
  d.o_ready_i = 0;
  d.eval();
  r.ok = true;
  return r;
}

/** True when `got` is bone `bone`'s stored matrix, element for element. */
inline bool matches_bone(const uint32_t* got, int bone) {
  for (int i = 0; i < kElems; ++i) {
    if (got[i] != pal_elem(bone, i)) return false;
  }
  return true;
}

inline bool matches_identity(const uint32_t* got) {
  for (int i = 0; i < kElems; ++i) {
    if (got[i] != ident_elem(i)) return false;
  }
  return true;
}

// The same two comparisons against a live DUT port rather than a captured copy
// — needed while a stalled offer is being watched cycle by cycle.
template <typename Arr>
inline bool port_matches_bone(const Arr& got, int bone) {
  for (int i = 0; i < kElems; ++i) {
    if (static_cast<uint32_t>(got[i]) != pal_elem(bone, i)) return false;
  }
  return true;
}

// ------------------------------------------------------------------ reset --
template <typename Dut>
void reset(Dut& d) {
  d.rst_n = 0;
  d.pal_begin_i = 0;
  d.wr_valid_i = 0;
  d.wr_bone_i = 0;
  d.v_valid_i = 0;
  d.o_ready_i = 0;
  d.v_x_i = 0;
  d.v_y_i = 0;
  d.v_z_i = 0;
  d.v_w0_i = 0;
  d.v_rigid_i = 0;
  d.v_bone0_i = 0;
  d.v_bone1_i = 0;
  d.v_src_id_i = 0;
  for (int i = 0; i < kElems; ++i) d.wr_m_i[i] = 0;
  d.eval();
  zhao::tick(d);
  zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

}  // namespace geom_pose_palette
