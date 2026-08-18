// surface_stamp_chain.cpp — SURFACE.STAMP -> SURFACE.SHEET, both blocks REAL.
//
// Composition has paid for itself twice in this tree already (a silent 16x
// tile-index-versus-pixel error, and a transposed neighbour index that left
// both blocks self-consistent while only the composition called it a tear), so
// the two surface blocks are run against each other rather than each against
// its own model. What this catches that the standalone suites cannot: a slot
// or texel address that both blocks agree on and the reference does not, and a
// handshake pair that only deadlocks when the real clear sweep stalls the real
// stamp.
//
// The chain then goes one step FURTHER than the pair: the sheet is read back
// through SURFACE.SHEET's own read port and handed to
// `zref::render::sample_sheet`, the DRAW-TIME consumer. That sampler maps world
// coordinates to texels with a DIFFERENT rule (floor across the envelope) than
// the stamp's texel-centre rule, so a u/v transposition or a dropped 64x in
// either block survives both standalone suites and dies here.
//
// ---------------------------------------------------------------------------
// THE SEAM THAT COULD NOT BE CLOSED, AND WHY — recorded, not quietly skipped
// ---------------------------------------------------------------------------
// The obvious third link would be SURFACE.SHEET -> TERRAIN.PATCH, because
// TERRAIN.PATCH takes `baked_scars`. It does not exist, and building it would
// have meant inventing law:
//
//   * TERRAIN.PATCH's `scar_i` is LAYER B — a height16 per-vertex bake delta on
//     the 33x33 lattice (design/contracts/TERRAIN.PATCH.md line 55,
//     spec/terrain_rules.md 2). SURFACE.SHEET owns LAYER F — a 64x64
//     {tag u8, strength u8} appearance sheet. They are different layers with
//     different extents, different elements and different owners:
//     spec/terrain_rules.md 7 says "B (scar) written only by TERRAIN.BAKE ...
//     F written only by SURFACE.STAMP".
//   * The block that converts one into the other is TERRAIN.BAKE
//     (design/blocks.yml: `inputs: [stamp_results]`, `outputs: [baked_scars]`,
//     upstream SURFACE.STAMP, downstream TERRAIN.PATCH) — and it is PHASE 7,
//     maturity SPECIFIED, with no RTL in fpga/rtl/terrain/ and no reference
//     model. `zref::terrain::bake_dig` bakes from a DigStamp paraboloid, not
//     from a surface sheet; there is no ratified strength -> height16 mapping
//     anywhere in the tree.
//
// So `stamp_results` is wired out to the edge of this test and checked for
// exactly what TERRAIN.BAKE will need (the texel, the tag, and the strength
// BEFORE and AFTER the blend, i.e. the delta), and the last hop waits for its
// own block. Fabricating the conversion here would have produced a green test
// asserting an invention.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"
#include "Vzhao_surface_stamp.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_surface.hpp"
#include "zrender/internal.hpp"  // white-box: stamp_surface, sample_sheet

using zhao::check;
using sdev::Rng;
using sdev::SheetResponse;
using sdev::StampCmd;
namespace zs = zref::surface;

namespace {

constexpr int32_t kM = 1 << 16;
constexpr uint32_t kPatch = 0x0000'2C01;

// ---------------------------------------------------------------------------
// the wiring
// ---------------------------------------------------------------------------

struct Chain {
  Vzhao_surface_stamp stamp;
  Vzhao_surface_sheet sheet;
  bool detached = false;  // true = the test drives the sheet's request port

  std::vector<zs::StampWrite> results;
  bool res_stall_on = false;
  Rng rng{0xC4A1'2026ULL};

  void settle() {
    for (int k = 0; k < 4; ++k) {
      if (!detached) {
        sheet.req_valid_i = stamp.req_valid_o;
        sheet.req_op_i = stamp.req_op_o;
        sheet.req_handle_i = stamp.req_handle_o;
        sheet.req_texel_i = stamp.req_texel_o;
        sheet.req_src_id_i = stamp.req_src_id_o;
        sheet.pg_ready_i = stamp.pg_ready_o;
        sheet.wr_valid_i = stamp.wr_valid_o;
        sheet.wr_handle_i = stamp.wr_handle_o;
        sheet.wr_texel_i = stamp.wr_texel_o;
        sheet.wr_tag_i = stamp.wr_tag_o;
        sheet.wr_strength_i = stamp.wr_strength_o;
        sheet.wr_we_tag_i = stamp.wr_we_tag_o;
        sheet.wr_we_strength_i = stamp.wr_we_strength_o;
        sheet.wr_src_id_i = stamp.wr_src_id_o;
      }
      sheet.eval();
      stamp.req_ready_i = detached ? 0 : sheet.req_ready_o;
      stamp.pg_valid_i = detached ? 0 : sheet.pg_valid_o;
      stamp.pg_status_i = sheet.pg_status_o;
      stamp.pg_strength_i = sheet.pg_strength_o;
      stamp.wr_ready_i = detached ? 0 : sheet.wr_ready_o;
      stamp.eval();
    }
  }

  void tick() {
    settle();
    stamp.clk = 0;
    sheet.clk = 0;
    settle();
    stamp.clk = 1;
    sheet.clk = 1;
    stamp.eval();
    sheet.eval();
    stamp.clk = 0;
    sheet.clk = 0;
    settle();
  }

  void reset() {
    stamp.rst_n = 0;
    sheet.rst_n = 0;
    stamp.cmd_valid_i = 0;
    stamp.fld_valid_i = 0;
    stamp.fld_tag_op_i = 0;
    stamp.fld_strength_i = 0;
    stamp.res_ready_i = 1;
    sheet.req_valid_i = 0;
    sheet.wr_valid_i = 0;
    sheet.pg_ready_i = 0;
    detached = false;
    settle();
    for (int i = 0; i < 3; ++i) tick();
    stamp.rst_n = 1;
    sheet.rst_n = 1;
    settle();
    tick();
  }

  /** Drive one stamp end to end; returns cycles, or -1 on a hang. */
  int run(const StampCmd& c, int max_cycles = 40000) {
    sdev::drive_cmd(stamp, c);
    stamp.cmd_valid_i = 1;
    bool accepted = false;
    for (int cyc = 0; cyc < max_cycles; ++cyc) {
      settle();
      if (!accepted && stamp.cmd_ready_o) accepted = true;
      // Stall stamp_results on a varying schedule so the third channel's
      // backpressure is exercised in the composition too.
      stamp.res_ready_i = (res_stall_on && rng.chance(4)) ? 0 : 1;
      settle();
      zs::StampWrite rec;
      const bool res_fire = stamp.res_valid_o && stamp.res_ready_i;
      if (res_fire) {
        rec.texel = static_cast<uint16_t>(stamp.res_texel_o);
        rec.tag = static_cast<uint8_t>(stamp.res_tag_o);
        rec.strength = static_cast<uint8_t>(stamp.res_strength_o);
        rec.before = static_cast<uint8_t>(stamp.res_before_o);
      }
      tick();
      if (res_fire) results.push_back(rec);
      if (accepted) stamp.cmd_valid_i = 0;
      settle();
      if (accepted && (stamp.stamp_done_o || stamp.stamp_rejected_o)) return cyc + 1;
    }
    return -1;
  }

  /** Read the whole sheet back through SURFACE.SHEET's own read port. */
  zs::Sheet readback(uint32_t handle) {
    zs::Sheet s;
    detached = true;
    stamp.cmd_valid_i = 0;
    for (int t = 0; t < zs::kSheetTexels; ++t) {
      sheet.req_valid_i = 1;
      sheet.req_op_i = sdev::kOpRead;
      sheet.req_handle_i = handle;
      sheet.req_texel_i = static_cast<uint16_t>(t);
      sheet.req_src_id_i = 0;
      sheet.pg_ready_i = 1;
      bool sent = false, got = false;
      for (int c = 0; c < 64 && !got; ++c) {
        settle();
        if (!sent && sheet.req_ready_o) sent = true;
        if (sent && sheet.pg_valid_o) {
          s.tag[t] = static_cast<uint8_t>(sheet.pg_tag_o);
          s.strength[t] = static_cast<uint8_t>(sheet.pg_strength_o);
          got = true;
        }
        tick();
        if (sent) sheet.req_valid_i = 0;
      }
    }
    sheet.req_valid_i = 0;
    detached = false;
    settle();
    return s;
  }

  SheetResponse request(uint8_t op, uint32_t handle) {
    SheetResponse r;
    detached = true;
    sheet.req_valid_i = 1;
    sheet.req_op_i = op;
    sheet.req_handle_i = handle;
    sheet.req_texel_i = 0;
    sheet.req_src_id_i = 0;
    sheet.pg_ready_i = 1;
    bool sent = false;
    for (int c = 0; c < 8192; ++c) {
      settle();
      if (!sent && sheet.req_ready_o) sent = true;
      if (sent && sheet.pg_valid_o) {
        r.status = static_cast<uint8_t>(sheet.pg_status_o);
        r.got = true;
        tick();
        break;
      }
      tick();
      if (sent) sheet.req_valid_i = 0;
    }
    sheet.req_valid_i = 0;
    detached = false;
    settle();
    return r;
  }
};

// The oracle sheet for a command sequence, through the RATIFIED entry point.
void apply_reference(zref::render::SurfaceSheet& ref, const zs::Envelope& e, const StampCmd& c) {
  zref::render::TerrainPatch patch;
  patch.env_x0 = e.x0;
  patch.env_z0 = e.z0;
  patch.env_x1 = e.x1;
  patch.env_z1 = e.z1;
  zhao_abi::ZhCmdSurfaceStamp st;
  std::memset(&st, 0, sizeof(st));
  st.tag = c.tag;
  st.strength = c.strength;
  st.operation = c.operation;
  st.transform.tx = c.tx;
  st.transform.ty = c.ty;
  st.radius = c.radius;
  st.ring_width = c.ring_width;
  zref::render::stamp_surface(ref, patch, st);
}

uint32_t diff(const zref::render::SurfaceSheet& a, const zs::Sheet& b) {
  uint32_t d = 0;
  for (int t = 0; t < zs::kSheetTexels; ++t)
    if (a.tag[t] != b.tag[t] || a.strength[t] != b.strength[t]) ++d;
  return d;
}

const zs::Envelope kEnv{-32 * kM, -32 * kM, 32 * kM, 32 * kM};

// ---------------------------------------------------------------------------

void test_one_stamp_through_both_blocks(Chain& ch) {
  ch.reset();
  const SheetResponse a = ch.request(sdev::kOpAcquire, kPatch);
  check(a.status == sdev::kStAllocated, "the sheet allocates for the patch", sdev::kStAllocated,
        a.status);

  StampCmd c;
  c.handle = kPatch;
  c.env = kEnv;
  c.tag = 1;
  c.strength = 0xD000;
  c.radius = 10 * kM;
  c.ring_width = 3 * kM;  // a crack ring, as render_golden.cpp emits
  ch.results.clear();
  const int cyc = ch.run(c);
  check(cyc > 0, "the composed pair completes a stamp", 1, cyc > 0 ? 1 : 0);

  zref::render::SurfaceSheet ref;
  apply_reference(ref, kEnv, c);
  const zs::Sheet got = ch.readback(kPatch);
  check(diff(ref, got) == 0,
        "STAMP -> SHEET, read back through SHEET, equals zref::render::stamp_surface", 0,
        diff(ref, got));
  ch.sheet.eval();
  check(ch.sheet.surface_texels_touched_o == ch.results.size(),
        "SHEET's texel counter equals the stamp_results record count",
        static_cast<uint64_t>(ch.results.size()), ch.sheet.surface_texels_touched_o);
  ch.stamp.eval();
  check(ch.stamp.surface_stamps_o == 1, "one stamp counted", 1, ch.stamp.surface_stamps_o);
}

void test_draw_time_sampler_agrees(Chain& ch) {
  // The sampler that actually paints the ground. It maps world -> texel by
  // FLOOR across the envelope, not by texel centre, so it is an independent
  // witness to the placement.
  zref::render::TerrainPatch patch;
  patch.env_x0 = kEnv.x0;
  patch.env_z0 = kEnv.z0;
  patch.env_x1 = kEnv.x1;
  patch.env_z1 = kEnv.z1;

  const zs::Sheet hw = ch.readback(kPatch);
  zref::render::SurfaceSheet hw_ref;
  for (int t = 0; t < zs::kSheetTexels; ++t) {
    hw_ref.tag[t] = hw.tag[t];
    hw_ref.strength[t] = hw.strength[t];
  }
  zref::render::SurfaceSheet ref;
  StampCmd c;
  c.handle = kPatch;
  c.env = kEnv;
  c.tag = 1;
  c.strength = 0xD000;
  c.radius = 10 * kM;
  c.ring_width = 3 * kM;
  apply_reference(ref, kEnv, c);

  uint32_t bad = 0, nonzero = 0;
  for (int j = 0; j < 64; ++j) {
    for (int i = 0; i < 64; ++i) {
      // sample at each texel's own centre AND at a quarter-texel offset, so an
      // off-by-one in either direction is visible
      for (int k = 0; k < 2; ++k) {
        const int32_t wx = static_cast<int32_t>(zs::texel_wx(kEnv, i)) + (k ? kM / 8 : 0);
        const int32_t wz = static_cast<int32_t>(zs::texel_wz(kEnv, j)) + (k ? kM / 8 : 0);
        const uint8_t sa = zref::render::sample_sheet(hw_ref, patch, zref::fx16{wx}, zref::fx16{wz});
        const uint8_t sb = zref::render::sample_sheet(ref, patch, zref::fx16{wx}, zref::fx16{wz});
        if (sa != sb) ++bad;
        if (sb) ++nonzero;
      }
    }
  }
  check(bad == 0, "the draw-time sampler reads the hardware sheet exactly as the reference's", 0,
        bad);
  // The fixture is a 3 m-wide ring of radius 10 m on a 64 m envelope: about
  // pi*(10^2 - 7^2) = 160 texels, sampled twice each.
  check(nonzero > 200, "the sampler actually found stamped texels (not a vacuous sweep)", 1,
        nonzero);
}

void test_persistence_across_frames(Chain& ch) {
  ch.reset();
  ch.request(sdev::kOpAcquire, kPatch);
  zref::render::SurfaceSheet ref;
  StampCmd c;
  c.handle = kPatch;
  c.env = kEnv;
  c.tag = 5;
  c.strength = 0x3000;
  c.radius = 6 * kM;
  c.operation = 1;  // decay-accumulate, so persistence is visible as growth
  uint8_t prev = 0;
  int rising = 0;
  for (int frame = 0; frame < 4; ++frame) {
    // The command stream re-acquires the sheet every frame, exactly as a real
    // frame would; a re-acquire must NOT clear.
    const SheetResponse a = ch.request(sdev::kOpAcquire, kPatch);
    check(a.status == sdev::kStHit, "the sheet is still resident next frame", sdev::kStHit,
          a.status);
    ch.run(c);
    apply_reference(ref, kEnv, c);
    const zs::Sheet got = ch.readback(kPatch);
    check(diff(ref, got) == 0, "frame N of the persistent sheet matches the reference", 0,
          diff(ref, got));
    const uint8_t v = got.strength[32 * 64 + 32];
    if (frame > 0 && v > prev) ++rising;
    prev = v;
  }
  check(rising == 3, "the scar accrued every frame — persistence survives the whole chain", 3,
        static_cast<uint64_t>(rising));
}

void test_overflow_writes_nothing(Chain& ch) {
  ch.reset();
  // Fill both slots with OTHER handles, so the stamp's patch cannot get in.
  const SheetResponse s0 = ch.request(sdev::kOpAcquire, 0x1111'0001);
  const SheetResponse s1 = ch.request(sdev::kOpAcquire, 0x2222'0001);
  check(s0.status == sdev::kStAllocated, "slot 0 taken", sdev::kStAllocated, s0.status);
  check(s1.status == sdev::kStAllocated, "slot 1 taken", sdev::kStAllocated, s1.status);

  StampCmd c;
  c.handle = kPatch;
  c.env = kEnv;
  c.tag = 9;
  c.strength = 0xFFFF;
  c.radius = 64 * kM;  // would cover every texel if it ran
  ch.results.clear();
  ch.sheet.eval();
  const uint32_t touched_before = ch.sheet.surface_texels_touched_o;
  const int cyc = ch.run(c);
  check(cyc > 0, "the rejected stamp terminates instead of hanging", 1, cyc > 0 ? 1 : 0);
  ch.stamp.eval();
  ch.sheet.eval();
  check(ch.results.empty(), "a rejected stamp emits no stamp_results", 0,
        static_cast<uint64_t>(ch.results.size()));
  check(ch.sheet.surface_texels_touched_o == touched_before,
        "SHEET saw not one write from the rejected stamp", touched_before,
        ch.sheet.surface_texels_touched_o);
  check(ch.sheet.res_occupancy_o == 3, "and nothing was evicted to make room", 3,
        ch.sheet.res_occupancy_o);
  check(cyc < 200, "the rejection is decided before the texel loop, not after it", 1,
        cyc < 200 ? 1 : 0);
}

void test_clear_sweep_stalls_the_stamp(Chain& ch) {
  // The real 4,096-cycle sweep against the real stamp: the pair must not
  // deadlock and the stamp must land on a ZEROED sheet, not on the previous
  // tenant's scars.
  ch.reset();
  ch.request(sdev::kOpAcquire, 0x3333'0001);
  StampCmd hot;
  hot.handle = 0x3333'0001;
  hot.env = kEnv;
  hot.tag = 200;
  hot.strength = 0xFFFF;
  hot.radius = 64 * kM;
  ch.run(hot);
  ch.request(sdev::kOpRelease, 0x3333'0001);

  // A different handle now takes the same physical slot; its sheet must be
  // clean even though the RAM still holds the old tenant's 255s.
  StampCmd c;
  c.handle = kPatch;
  c.env = kEnv;
  c.tag = 7;
  c.strength = 0x2000;  // 32
  c.radius = 4 * kM;
  const SheetResponse a = ch.request(sdev::kOpAcquire, kPatch);
  check(a.status == sdev::kStAllocated, "the recycled slot ALLOCATES", sdev::kStAllocated,
        a.status);
  const int cyc = ch.run(c);
  check(cyc > 0, "the stamp survives the real clear sweep", 1, cyc > 0 ? 1 : 0);
  zref::render::SurfaceSheet ref;
  apply_reference(ref, kEnv, c);
  const zs::Sheet got = ch.readback(kPatch);
  check(diff(ref, got) == 0, "the recycled slot was truly cleared before the stamp landed", 0,
        diff(ref, got));
  check(got.strength[0] == 0, "the previous tenant's 255 is gone from texel 0", 0,
        got.strength[0]);
}

void test_randomized_composition(Chain& ch) {
  ch.reset();
  ch.request(sdev::kOpAcquire, kPatch);
  ch.res_stall_on = true;
  zref::render::SurfaceSheet ref;
  Rng rng(0x9911'2026ULL);
  uint32_t rings = 0, full = 0, empty = 0;
  for (int i = 0; i < 24; ++i) {
    StampCmd c;
    c.handle = kPatch;
    c.env = kEnv;
    c.tag = static_cast<uint8_t>(rng.range(1, 255));
    c.strength = static_cast<uint16_t>(rng.range(0, 65535));
    c.operation = static_cast<uint8_t>(rng.range(0, 1));
    c.tx = rng.range(-48, 48) * kM;
    c.ty = rng.range(-48, 48) * kM;
    c.radius = (i % 8 == 0) ? 64 * kM : rng.range(1, 14) * kM;
    if (rng.chance(3)) {
      c.ring_width = rng.range(1, 8) * kM;
      ++rings;
    }
    ch.run(c);
    apply_reference(ref, kEnv, c);
    if (c.radius == 64 * kM) ++full;
    if (c.tx > 44 * kM || c.tx < -44 * kM) ++empty;
  }
  const zs::Sheet got = ch.readback(kPatch);
  check(diff(ref, got) == 0, "24 randomized stamps through both real blocks match the reference",
        0, diff(ref, got));
  check(rings > 0 && full > 0, "the randomized composition covered rings and full sheets", 1,
        (rings > 0 && full > 0) ? 1 : 0);
  ch.res_stall_on = false;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Chain ch;
  test_one_stamp_through_both_blocks(ch);
  test_draw_time_sampler_agrees(ch);
  test_persistence_across_frames(ch);
  test_overflow_writes_nothing(ch);
  test_clear_sweep_stalls_the_stamp(ch);
  test_randomized_composition(ch);

  std::printf("surface_stamp_chain: STAMP -> SHEET -> readback -> sample_sheet, all real RTL\n");
  return zhao::report_and_exit("surface_stamp_chain");
}
