// texture_aux_dev.hpp — the shared driver for TEXTURE.AUX.
//
// One place that knows the block's handshake and its SURFACE.SHEET master
// port, so the directed lane and both random lanes drive it identically. If
// the port list changes, exactly one file needs editing and every lane fails
// to compile until it is. (Same shape as tests/terrain/project_dev.hpp.)
//
// `SheetModel` is a C++ stand-in for SURFACE.SHEET's READ side only — one
// halfword of {tag, strength} per texel, a residency flag, and a programmable
// stall — so the random lanes can sweep millions of world positions without
// dragging a 4,096-texel RTL store through every one. It is NOT the law: the
// directed lane composes the REAL `zhao_surface_sheet` with the real block and
// proves the wiring, which is the only place the port semantics are actually
// pinned.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_texture_aux.h"

#include "zhao_sim.hpp"
#include "zref/zref_aux.hpp"

namespace aux_test {

/** One `aux_requests` packet. */
struct Req {
  int32_t wx = 0, wz = 0;
  int32_t ex0 = 0, ez0 = 0, ex1 = 0, ez1 = 0;
  uint32_t handle = 0;
  uint16_t src_id = 0;
};

/** One `aux_samples` packet. */
struct Smp {
  uint8_t tag = 0, strength = 0, u = 0, v = 0;
  bool degenerate = false, miss = false;
  uint16_t src_id = 0;
  bool operator==(const Smp& o) const {
    return tag == o.tag && strength == o.strength && u == o.u && v == o.v &&
           degenerate == o.degenerate && miss == o.miss && src_id == o.src_id;
  }
};

/** What `zref::AuxSource` says the block must produce for one packet. */
inline Smp oracle(const Req& r, const uint8_t* tags, const uint8_t* strengths, bool resident) {
  zref::aux::Envelope e;
  e.x0 = r.ex0;
  e.z0 = r.ez0;
  e.x1 = r.ex1;
  e.z1 = r.ez1;
  const zref::aux::Sample s = zref::AuxSource::sample(e, r.wx, r.wz, tags, strengths, resident);
  Smp o;
  o.tag = s.tag;
  o.strength = s.strength;
  o.u = s.u;
  o.v = s.v;
  o.degenerate = s.degenerate;
  o.miss = s.miss;
  o.src_id = r.src_id;
  return o;
}

/** SURFACE.SHEET's read side, in C++. See the header note. */
struct SheetModel {
  uint8_t tag[4096] = {};
  uint8_t strength[4096] = {};
  bool resident = true;
  uint32_t stall_mask = 0;  // rotated onto the request port's ready

  // one in-flight read, matching SURFACE.SHEET's own depth
  bool busy = false;
  uint8_t r_tag = 0, r_str = 0;
  bool r_miss = false;
};

/** The DUT driver: reset, push packets, collect packets. */
class Dev {
 public:
  explicit Dev(Vzhao_texture_aux& dut) : dut_(dut) {}

  void reset() {
    dut_.rst_n = 0;
    dut_.req_valid_i = 0;
    dut_.smp_ready_i = 0;
    dut_.shr_ready_i = 0;
    dut_.shp_valid_i = 0;
    dut_.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
    zhao::tick(dut_);
  }

  uint32_t samples() const { return dut_.texture_samples_o; }
  bool idle() const { return dut_.idle_o != 0; }
  /** Sheet reads the block actually issued — a degenerate envelope issues none. */
  long sheet_reads() const { return sheet_reads_; }
  /** The worst accept-to-retire distance seen, in clocks (the ledger's bound). */
  int worst_latency() const { return worst_lat_; }
  /** Clocks from the first accept to the last retire — the SUSTAINED rate. */
  long span_clocks() const { return span_clocks_; }

  std::vector<Smp> run(const std::vector<Req>& in, SheetModel& sheet, uint32_t stall_mask = 0) {
    std::vector<Smp> out;
    out.reserve(in.size());
    size_t pushed = 0;
    int accept_cycle = -1;
    int first_accept = -1;
    int last_retire = 0;
    const int limit = static_cast<int>(in.size()) * 256 + 4096;
    sheet_reads_ = 0;
    worst_lat_ = -1;
    span_clocks_ = 0;

    for (int cycle = 0; cycle < limit; ++cycle) {
      const bool ready = (stall_mask == 0) || (((stall_mask >> (cycle & 31)) & 1u) == 0);
      dut_.smp_ready_i = ready ? 1 : 0;
      const bool sh_ready =
          (sheet.stall_mask == 0) || (((sheet.stall_mask >> (cycle & 31)) & 1u) == 0);
      dut_.shr_ready_i = (!sheet.busy && sh_ready) ? 1 : 0;

      // the sheet's response channel: one cycle after acceptance
      dut_.shp_valid_i = sheet.busy ? 1 : 0;
      dut_.shp_status_i = sheet.r_miss ? 3 : 0;  // StMiss / StHit
      dut_.shp_tag_i = sheet.r_tag;
      dut_.shp_strength_i = sheet.r_str;

      if (pushed < in.size()) {
        const Req& r = in[pushed];
        dut_.req_valid_i = 1;
        dut_.req_wx_i = r.wx;
        dut_.req_wz_i = r.wz;
        dut_.req_env_x0_i = r.ex0;
        dut_.req_env_z0_i = r.ez0;
        dut_.req_env_x1_i = r.ex1;
        dut_.req_env_z1_i = r.ez1;
        dut_.req_handle_i = r.handle;
        dut_.req_src_id_i = r.src_id;
      } else {
        dut_.req_valid_i = 0;
        // Poison the port once the stream is done: a block that latched a
        // stale packet would show it as an extra output.
        dut_.req_wx_i = 0x5BADF00D;
        dut_.req_wz_i = 0x5BADF00D;
        dut_.req_env_x0_i = 0x5BADF00D;
        dut_.req_handle_i = 0xDEADBEEF;
        dut_.req_src_id_i = 0xDEAD;
      }

      dut_.eval();

      const bool take = (pushed < in.size()) && dut_.req_ready_o;
      const bool sh_issue = dut_.shr_valid_o && dut_.shr_ready_i;
      const bool sh_take = dut_.shp_valid_i && dut_.shp_ready_o;

      if (dut_.smp_valid_o && ready) {
        Smp s;
        s.tag = static_cast<uint8_t>(dut_.smp_tag_o);
        s.strength = static_cast<uint8_t>(dut_.smp_strength_o);
        s.u = static_cast<uint8_t>(dut_.smp_u_o);
        s.v = static_cast<uint8_t>(dut_.smp_v_o);
        s.degenerate = dut_.smp_degenerate_o != 0;
        s.miss = dut_.smp_miss_o != 0;
        s.src_id = static_cast<uint16_t>(dut_.smp_src_id_o);
        out.push_back(s);
        last_retire = cycle;
        if (accept_cycle >= 0) {
          const int lat = cycle - accept_cycle;
          if (lat > worst_lat_) worst_lat_ = lat;
          accept_cycle = -1;
        }
      }

      // the model's own state advances on the same edge the DUT's does
      if (sh_issue) {
        ++sheet_reads_;
        const uint32_t idx = static_cast<uint32_t>(dut_.shr_texel_o) & 0xFFFu;
        sheet.r_miss = !sheet.resident;
        sheet.r_tag = sheet.resident ? sheet.tag[idx] : 0;
        sheet.r_str = sheet.resident ? sheet.strength[idx] : 0;
        sheet.busy = true;
      } else if (sh_take) {
        sheet.busy = false;
      }

      zhao::tick(dut_);
      if (take) {
        accept_cycle = cycle;
        if (first_accept < 0) first_accept = cycle;
        ++pushed;
      }
      if (out.size() == in.size()) break;
    }
    dut_.req_valid_i = 0;
    dut_.smp_ready_i = 0;
    dut_.eval();
    span_clocks_ = (first_accept < 0) ? 0 : (last_retire - first_accept + 1);
    return out;
  }

 private:
  Vzhao_texture_aux& dut_;
  long sheet_reads_ = 0;
  int worst_lat_ = -1;
  long span_clocks_ = 0;
};

}  // namespace aux_test
