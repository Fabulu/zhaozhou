// surface_dev.hpp — shared drivers for the SURFACE.SHEET / SURFACE.STAMP
// suites.
//
// Two things live here and nothing else:
//   1. `SheetSim` — a C++ model of SURFACE.SHEET's WIRE PROTOCOL (not of its
//      contents; the contents come from `zref::surface::SheetStore`, the
//      oracle). It exists so SURFACE.STAMP can be exercised standalone: a
//      defect in one block must not be able to hide inside the other, which is
//      exactly why surface_stamp_chain.cpp then runs the two REAL blocks
//      against each other.
//   2. the reset sequences, which cannot use `zhao::reset()` — that helper
//      assumes the byte-stream ports (`in_valid`/`in_data`) of the harness's
//      first consumers, and neither of these blocks has them.

#pragma once

#include <cstdint>
#include <vector>

#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

namespace sdev {

// SURFACE.SHEET protocol constants, kept in step with the RTL localparams.
constexpr uint8_t kOpAcquire = 0;
constexpr uint8_t kOpRead = 1;
constexpr uint8_t kOpRelease = 2;

constexpr uint8_t kStHit = 0;
constexpr uint8_t kStAllocated = 1;
constexpr uint8_t kStOverflow = 2;
constexpr uint8_t kStMiss = 3;

// zref::surface::Blend, mirrored.
constexpr uint8_t kBlStamp = 0;
constexpr uint8_t kBlDecayAcc = 1;
constexpr uint8_t kBlMax = 2;
constexpr uint8_t kBlAdd = 3;
constexpr uint8_t kBlSub = 4;
constexpr uint8_t kBlReplace = 5;
constexpr uint8_t kBlAge = 6;

// Deterministic: same sequence every run, on every host. splitmix64.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
  bool chance(int one_in) { return (next() % static_cast<uint64_t>(one_in)) == 0; }
};

// ---------------------------------------------------------- SURFACE.SHEET --

template <typename Top>
void reset_sheet(Top& top) {
  top.rst_n = 0;
  top.req_valid_i = 0;
  top.req_op_i = 0;
  top.req_handle_i = 0;
  top.req_texel_i = 0;
  top.req_src_id_i = 0;
  top.pg_ready_i = 0;
  top.wr_valid_i = 0;
  top.wr_handle_i = 0;
  top.wr_texel_i = 0;
  top.wr_tag_i = 0;
  top.wr_strength_i = 0;
  top.wr_we_tag_i = 0;
  top.wr_we_strength_i = 0;
  top.wr_src_id_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

struct SheetResponse {
  uint8_t op = 0;
  uint8_t status = kStMiss;
  uint8_t tag = 0;
  uint8_t strength = 0;
  uint16_t src_id = 0;
  bool got = false;
  // `res_overflow_o` is a ONE-CYCLE pulse, so a caller that reads it after the
  // response has already missed it. Counted across the whole request window
  // instead. Counting rather than latching is deliberate: it distinguishes "did
  // not fire" from "fired twice", and a fault line stuck HIGH is exactly as
  // broken as one stuck low.
  uint32_t overflow_pulses = 0;
};

/**
 * Offer one request and drain exactly one response. `max_cycles` bounds the
 * ACQUIRE clear sweep (4,096 cycles by law) with room to spare.
 */
template <typename Top>
SheetResponse sheet_request(Top& top, uint8_t op, uint32_t handle, uint16_t texel, uint16_t src_id,
                            int max_cycles = 8192) {
  SheetResponse r;
  top.req_valid_i = 1;
  top.req_op_i = op;
  top.req_handle_i = handle;
  top.req_texel_i = texel;
  top.req_src_id_i = src_id;
  top.pg_ready_i = 1;
  bool sent = false;
  for (int c = 0; c < max_cycles; ++c) {
    top.eval();
    if (top.res_overflow_o) r.overflow_pulses++;
    if (!sent && top.req_ready_o) sent = true;
    if (top.pg_valid_o && sent) {
      r.op = static_cast<uint8_t>(top.pg_op_o);
      r.status = static_cast<uint8_t>(top.pg_status_o);
      r.tag = static_cast<uint8_t>(top.pg_tag_o);
      r.strength = static_cast<uint8_t>(top.pg_strength_o);
      r.src_id = static_cast<uint16_t>(top.pg_src_id_o);
      r.got = true;
      zhao::tick(top);
      top.req_valid_i = 0;
      top.eval();
      if (top.res_overflow_o) r.overflow_pulses++;
      return r;
    }
    zhao::tick(top);
    if (sent) top.req_valid_i = 0;
  }
  top.req_valid_i = 0;
  top.eval();
  return r;
}

/** Offer one write; returns true once accepted. */
template <typename Top>
bool sheet_write(Top& top, uint32_t handle, uint16_t texel, uint8_t tag, uint8_t strength,
                 bool we_tag, bool we_str, uint16_t src_id, int max_cycles = 8192) {
  top.wr_valid_i = 1;
  top.wr_handle_i = handle;
  top.wr_texel_i = texel;
  top.wr_tag_i = tag;
  top.wr_strength_i = strength;
  top.wr_we_tag_i = we_tag ? 1 : 0;
  top.wr_we_strength_i = we_str ? 1 : 0;
  top.wr_src_id_i = src_id;
  for (int c = 0; c < max_cycles; ++c) {
    top.eval();
    if (top.wr_ready_o) {
      zhao::tick(top);
      top.wr_valid_i = 0;
      top.eval();
      return true;
    }
    zhao::tick(top);
  }
  top.wr_valid_i = 0;
  top.eval();
  return false;
}

// ---------------------------------------------------------- SURFACE.STAMP --

/** The dispatch half of a SurfaceStamp, as SURFACE.STAMP's ports take it. */
struct StampCmd {
  uint32_t handle = 44;
  uint8_t operation = 0;
  uint8_t tag = 0;
  uint16_t strength = 0;
  int32_t tx = 0, ty = 0;
  int32_t radius = 0, ring_width = 0;
  zref::surface::Envelope env{};
  bool blend_en = false;
  uint8_t blend = kBlStamp;
  uint8_t age_shift = 1;
  bool field_en = false;
  uint16_t src_id = 0;
};

template <typename Top>
void reset_stamp(Top& top) {
  top.rst_n = 0;
  top.cmd_valid_i = 0;
  top.fld_valid_i = 0;
  top.fld_tag_op_i = 0;
  top.fld_strength_i = 0;
  top.req_ready_i = 0;
  top.pg_valid_i = 0;
  top.pg_status_i = 0;
  top.pg_strength_i = 0;
  top.wr_ready_i = 0;
  top.res_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

template <typename Top>
void drive_cmd(Top& top, const StampCmd& c) {
  top.cmd_handle_i = c.handle;
  top.cmd_operation_i = c.operation;
  top.cmd_tag_i = c.tag;
  top.cmd_strength_i = c.strength;
  top.cmd_tx_i = c.tx;
  top.cmd_ty_i = c.ty;
  top.cmd_radius_i = c.radius;
  top.cmd_ring_width_i = c.ring_width;
  top.cmd_env_x0_i = c.env.x0;
  top.cmd_env_z0_i = c.env.z0;
  top.cmd_env_x1_i = c.env.x1;
  top.cmd_env_z1_i = c.env.z1;
  top.cmd_blend_en_i = c.blend_en ? 1 : 0;
  top.cmd_blend_i = c.blend;
  top.cmd_age_shift_i = c.age_shift;
  top.cmd_field_en_i = c.field_en ? 1 : 0;
  top.cmd_src_id_i = c.src_id;
}

/**
 * A model of SURFACE.SHEET's wire protocol, holding a `zref::surface::Sheet`
 * per resident handle. Timing matches the RTL: a request is accepted when the
 * response register is free, the answer lands one cycle later and HOLDS until
 * it is taken. Fresh allocation is modelled as instantaneous here — the clear
 * sweep is SURFACE.SHEET's own property and is checked in its own suite; what
 * SURFACE.STAMP must survive is the STALL, which `stall_acquire` injects.
 */
struct SheetSim {
  zref::surface::SheetStore store;
  explicit SheetSim(int slots = 2) : store(slots) {}

  // response register
  bool pg_valid = false;
  uint8_t pg_status = kStMiss;
  uint8_t pg_strength = 0;

  // backpressure knobs (0 = never stall)
  int stall_req = 0;
  int stall_wr = 0;
  int stall_res = 0;
  int stall_acquire = 0;

  // observed traffic
  std::vector<zref::surface::StampWrite> results;  // stamp_results stream
  std::vector<uint16_t> writes;                    // sheet write port, in order
  uint32_t write_count = 0;
  bool saw_req_stall = false;
  bool saw_wr_stall = false;
  bool saw_res_stall = false;

  // pending answer countdown (models the clear sweep as a stall)
  int pending = 0;
  uint8_t pending_status = kStHit;
  uint8_t pending_strength = 0;

  bool force_overflow = false;

  /**
   * One cycle of the STAMP <-> SHEET conversation. Inputs are driven, the
   * combinational outputs are read, the handshakes are resolved, the clock
   * ticks, and only then does the model state move — the same order real
   * hardware sees.
   */
  template <typename Top>
  void step(Top& top, Rng& rng, uint32_t handle, const std::vector<zref::surface::FieldResult>* fld,
            size_t* fld_cursor) {
    const bool req_stall = stall_req && rng.chance(stall_req);
    const bool wr_stall = stall_wr && rng.chance(stall_wr);
    const bool res_stall = stall_res && rng.chance(stall_res);

    top.pg_valid_i = pg_valid ? 1 : 0;
    top.pg_status_i = pg_status;
    top.pg_strength_i = pg_strength;
    top.wr_ready_i = wr_stall ? 0 : 1;
    top.res_ready_i = res_stall ? 0 : 1;
    if (fld && *fld_cursor < fld->size()) {
      top.fld_valid_i = 1;
      top.fld_tag_op_i = (*fld)[*fld_cursor].tag_op;
      top.fld_strength_i = (*fld)[*fld_cursor].strength;
    } else {
      top.fld_valid_i = 0;
      top.fld_tag_op_i = 0;
      top.fld_strength_i = 0;
    }
    top.req_ready_i = 0;
    top.eval();

    const bool pg_fire = pg_valid && top.pg_ready_o;
    // SURFACE.SHEET accepts when its response slot is free — mirrored exactly.
    const bool slot_free = !pg_valid || pg_fire;
    top.req_ready_i = (slot_free && pending == 0 && !req_stall) ? 1 : 0;
    top.eval();

    const bool req_fire = top.req_valid_o && top.req_ready_i;
    const uint8_t req_op = static_cast<uint8_t>(top.req_op_o);
    const uint16_t req_texel = static_cast<uint16_t>(top.req_texel_o);
    const bool wr_fire = top.wr_valid_o && !wr_stall;
    const bool res_fire = top.res_valid_o && !res_stall;
    const bool fld_fire = top.fld_ready_o && top.fld_valid_i;

    if (top.req_valid_o && !top.req_ready_i) saw_req_stall = true;
    if (top.wr_valid_o && wr_stall) saw_wr_stall = true;
    if (top.res_valid_o && res_stall) saw_res_stall = true;

    uint16_t wr_texel = 0;
    uint8_t wr_tag = 0, wr_str = 0;
    if (wr_fire) {
      wr_texel = static_cast<uint16_t>(top.wr_texel_o);
      wr_tag = static_cast<uint8_t>(top.wr_tag_o);
      wr_str = static_cast<uint8_t>(top.wr_strength_o);
    }
    zref::surface::StampWrite rec;
    if (res_fire) {
      rec.texel = static_cast<uint16_t>(top.res_texel_o);
      rec.tag = static_cast<uint8_t>(top.res_tag_o);
      rec.strength = static_cast<uint8_t>(top.res_strength_o);
      rec.before = static_cast<uint8_t>(top.res_before_o);
    }

    zhao::tick(top);

    if (pg_fire) pg_valid = false;
    if (fld_fire && fld) ++(*fld_cursor);
    if (wr_fire) {
      const int slot = store.find(handle);
      if (slot >= 0) {
        store.at(slot).tag[wr_texel] = wr_tag;
        store.at(slot).strength[wr_texel] = wr_str;
      }
      writes.push_back(wr_texel);
      ++write_count;
    }
    if (res_fire) results.push_back(rec);

    // The answer is REGISTERED at this edge, exactly as the RTL registers it:
    // a read issued in cycle t is readable in t+1. Only a fresh ACQUIRE can
    // take longer, and `stall_acquire` is how the clear sweep is injected.
    if (pending > 0) {
      if (--pending == 0) {
        pg_valid = true;
        pg_status = pending_status;
        pg_strength = pending_strength;
      }
    } else if (req_fire) {
      if (req_op == kOpAcquire) {
        if (force_overflow) {
          pending_status = kStOverflow;
        } else {
          const zref::surface::AcquireResult a = store.acquire(handle);
          pending_status = (a.status == zref::surface::Residency::kHit)         ? kStHit
                           : (a.status == zref::surface::Residency::kAllocated) ? kStAllocated
                                                                                : kStOverflow;
        }
        pending_strength = 0;
        if (stall_acquire > 0) {
          pending = stall_acquire;
        } else {
          pg_valid = true;
          pg_status = pending_status;
          pg_strength = 0;
        }
      } else {
        const int slot = store.find(handle);
        pending_status = slot >= 0 ? kStHit : kStMiss;
        pending_strength = slot >= 0 ? store.at(slot).strength[req_texel] : 0;
        pg_valid = true;
        pg_status = pending_status;
        pg_strength = pending_strength;
      }
    }
  }
};

// THE HANG GUARD, derived rather than guessed.
//
// SURFACE.STAMP's coverage geometry is sequential since the DSP farm came out
// of it (28 -> 0 DSPs, 2026-08-23). At the default SQ_RADIX = 1 one square
// costs 38 cycles, dz is squared once per row and dx once per texel, so a
// 4,096-texel scan is 64 * 65 * 38 = 158,080 cycles plus setup and drain --
// measured 158,162. The guard is 2.5x that, because the stalled runs stretch it
// and a guard that trips is indistinguishable from a design that hangs.
//
// It was 200,000, which was 1.26x and survived only by luck; the chain's own
// guard was 40,000 and DID trip, producing fifteen "wrong sheet" failures that
// were nothing of the kind. Recorded because a too-tight guard reads exactly
// like a correctness bug.
constexpr int kStampHangGuard = 400000;

/** Drive one stamp to completion against `SheetSim`. Returns cycles spent. */
template <typename Top>
int run_stamp(Top& top, SheetSim& sim, const StampCmd& c, Rng& rng,
              const std::vector<zref::surface::FieldResult>* fld = nullptr,
              int max_cycles = kStampHangGuard) {
  drive_cmd(top, c);
  top.cmd_valid_i = 1;
  size_t fld_cursor = 0;
  int cycles = 0;
  bool accepted = false;
  for (; cycles < max_cycles; ++cycles) {
    top.eval();
    if (!accepted && top.cmd_ready_o) accepted = true;
    sim.step(top, rng, c.handle, fld, &fld_cursor);  // includes the tick
    if (accepted) top.cmd_valid_i = 0;
    top.eval();
    if (accepted && (top.stamp_done_o || top.stamp_rejected_o)) return cycles + 1;
  }
  return -1;
}

}  // namespace sdev
