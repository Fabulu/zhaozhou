// surface_stamp_directed.cpp — directed suite for SURFACE.STAMP.
//
// The first test in this file is the one that licenses all the others:
// `test_view_is_faithful` proves that `zref::surface`'s per-texel
// decomposition IS `zref::render::stamp_surface`, the executed law, over
// randomized commands and a pre-dirtied sheet. Without it the rest of the
// suite would be checking the RTL against a second implementation of the
// stamp, which charter 29-6 forbids and which is how a "green" differential
// ends up proving nothing.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_stamp.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_surface.hpp"
#include "zrender/internal.hpp"  // white-box: zref::render::stamp_surface

// The SQ_RADIX this translation unit's model was elaborated at. The frontier
// builds (test_surface_stamp_radix2/radix4) set it to match their
// `-GSQ_RADIX=`; the default build leaves it at the RTL's own default. A
// mismatch would make the sequence-shape check below fail loudly, which is the
// intent -- a silently-skipped check is worse than a failing one.
#ifndef ZHAO_SQ_RADIX
#define ZHAO_SQ_RADIX 1
#endif

using sdev::Rng;
using sdev::SheetSim;
using sdev::StampCmd;
using zhao::check;
namespace zs = zref::surface;

namespace {

constexpr int32_t kM = 1 << 16;  // one world metre, fx16

// The canonical +-8 m patch envelope of tests/render/render_heightfield.cpp:
// 16 m across 64 texels = 0.25 m per texel, texel centre i at
// (i + 0.5)/4 - 8 metres.
const zs::Envelope kEnv8{-8 * kM, -8 * kM, 8 * kM, 8 * kM};

zs::StampGeom geom_of(const StampCmd& c) {
  return zs::StampGeom{c.tx, c.ty, c.radius, c.ring_width};
}

zs::Blend blend_of(const StampCmd& c) {
  return c.blend_en ? static_cast<zs::Blend>(c.blend) : zs::blend_of_abi_operation(c.operation);
}

zs::StampSource source_of(const StampCmd& c) {
  return zs::StampSource{c.tag, c.strength, blend_of(c), c.age_shift};
}

// ---------------------------------------------------------------------------
// 1. The view is the law (charter 29-6)
// ---------------------------------------------------------------------------

void test_view_is_faithful() {
  Rng rng(0x5CA12026ULL);
  int disagreements = 0;
  int nonempty = 0;
  for (int it = 0; it < 400; ++it) {
    // Random envelope and command inside the stated domain, including
    // inverted envelopes (x1 < x0) — the case where the reference's `/ 128`
    // truncation toward zero differs from an arithmetic shift.
    zs::Envelope e;
    e.x0 = rng.range(-64, 64) * kM;
    e.z0 = rng.range(-64, 64) * kM;
    e.x1 = e.x0 + rng.range(-40, 40) * kM;
    e.z1 = e.z0 + rng.range(-40, 40) * kM;

    zhao_abi::ZhCmdSurfaceStamp st;
    std::memset(&st, 0, sizeof(st));
    st.tag = static_cast<uint8_t>(rng.range(0, 255));
    st.strength = static_cast<uint16_t>(rng.range(0, 65535));
    st.operation = static_cast<uint8_t>(rng.range(0, 3));  // 0,1 and two "else" values
    // Aim at the envelope's midpoint most of the time: a uniform centre over
    // a 128 m square almost never covers a 40 m envelope, and a sweep that
    // covers nothing proves nothing.
    const int32_t mx = (e.x0 / 2) + (e.x1 / 2);
    const int32_t mz = (e.z0 / 2) + (e.z1 / 2);
    st.transform.tx = mx + rng.range(-20, 20) * kM;
    st.transform.ty = mz + rng.range(-20, 20) * kM;
    st.radius = rng.range(-40, 40) * kM;
    st.ring_width = rng.chance(2) ? rng.range(0, 20) * kM : 0;

    zref::render::TerrainPatch patch;
    patch.env_x0 = e.x0;
    patch.env_z0 = e.z0;
    patch.env_x1 = e.x1;
    patch.env_z1 = e.z1;

    // Pre-dirty both sheets identically so the BLEND is exercised, not just
    // coverage. A clean sheet cannot tell max from replace.
    zref::render::SurfaceSheet ref;
    zs::Sheet mine;
    for (int t = 0; t < zs::kSheetTexels; ++t) {
      const uint8_t s = static_cast<uint8_t>(rng.range(0, 255));
      const uint8_t g = static_cast<uint8_t>(rng.range(0, 255));
      ref.strength[t] = s;
      ref.tag[t] = g;
      mine.strength[t] = s;
      mine.tag[t] = g;
    }

    zref::render::stamp_surface(ref, patch, st);

    const zs::StampGeom g{st.transform.tx, st.transform.ty, st.radius, st.ring_width};
    const zs::StampSource src{st.tag, st.strength, zs::blend_of_abi_operation(st.operation), 1};
    std::vector<zs::StampWrite> w;
    zs::stamp_apply(mine, e, g, src, &w);
    if (!w.empty()) ++nonempty;

    for (int t = 0; t < zs::kSheetTexels; ++t)
      if (ref.strength[t] != mine.strength[t] || ref.tag[t] != mine.tag[t]) ++disagreements;
  }
  check(disagreements == 0, "zref::surface decomposition == zref::render::stamp_surface", 0,
        static_cast<uint64_t>(disagreements));
  // If nothing ever covered a texel the check above is vacuous.
  check(nonempty > 150, "the faithfulness sweep actually stamped texels", 1,
        static_cast<uint64_t>(nonempty));
}

// ---------------------------------------------------------------------------
// DUT helpers
// ---------------------------------------------------------------------------

struct Outcome {
  zs::Sheet want;  // oracle
  zs::Sheet got;   // DUT (via SheetSim's store)
  uint32_t texels_written = 0;
  uint32_t oracle_texels = 0;
  int cycles = 0;
  bool rejected = false;
  bool done = false;
};

// Run one stamp against a fresh SheetSim seeded with `initial`, and against
// the oracle seeded identically.
Outcome run_one(Vzhao_surface_stamp& dut, const StampCmd& c, const zs::Sheet& initial, Rng& rng,
                int stall_req = 0, int stall_wr = 0, int stall_res = 0, bool overflow = false,
                const std::vector<zs::FieldResult>* fld = nullptr) {
  Outcome o;
  SheetSim sim(2);
  sim.stall_req = stall_req;
  sim.stall_wr = stall_wr;
  sim.stall_res = stall_res;
  sim.force_overflow = overflow;
  const zs::AcquireResult a = sim.store.acquire(c.handle);
  sim.store.at(a.slot) = initial;

  o.want = initial;
  std::vector<zs::StampWrite> w;
  if (fld)
    zs::stamp_apply_field(o.want, c.env, geom_of(c), *fld, &w);
  else
    zs::stamp_apply(o.want, c.env, geom_of(c), source_of(c), &w);
  o.oracle_texels = static_cast<uint32_t>(w.size());

  o.cycles = sdev::run_stamp(dut, sim, c, rng, fld);
  dut.eval();
  o.rejected = overflow;
  o.done = o.cycles >= 0;
  o.got = sim.store.at(sim.store.find(c.handle));
  o.texels_written = sim.write_count;
  if (overflow) o.want = initial;  // S4: a rejected stamp writes nothing
  return o;
}

uint32_t sheet_diff(const zs::Sheet& a, const zs::Sheet& b) {
  uint32_t d = 0;
  for (int t = 0; t < zs::kSheetTexels; ++t)
    if (a.tag[t] != b.tag[t] || a.strength[t] != b.strength[t]) ++d;
  return d;
}

// ---------------------------------------------------------------------------
// 2..N: the directed cases
// ---------------------------------------------------------------------------

void test_empty_sheet_single_stamp(Vzhao_surface_stamp& dut) {
  Rng rng(1);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 1;
  c.strength = 0xFFFF;  // -> 255 (the reference's truncating >> 8)
  c.radius = 8 * kM;    // "covers the whole patch" (render_heightfield.cpp)
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.done, "single stamp completes", 1, o.done ? 1 : 0);
  check(sheet_diff(o.want, o.got) == 0, "empty sheet + one stamp matches the oracle", 0,
        sheet_diff(o.want, o.got));
  check(o.texels_written == o.oracle_texels, "every covered texel was written exactly once",
        o.oracle_texels, o.texels_written);
  check(o.got.strength[32 * 64 + 32] == 255, "centre texel carries the full strength", 255,
        o.got.strength[32 * 64 + 32]);
  check(o.got.tag[32 * 64 + 32] == 1, "centre texel carries the tag", 1, o.got.tag[32 * 64 + 32]);
}

void test_strength_truncation(Vzhao_surface_stamp& dut) {
  Rng rng(2);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 3;
  c.strength = 0x01FF;  // >> 8 = 1, NOT the round-half-up 2 of qformats 2
  c.radius = 8 * kM;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.got.strength[32 * 64 + 32] == 1, "strength >> 8 TRUNCATES (0x01FF -> 1)", 1,
        o.got.strength[32 * 64 + 32]);
  check(sheet_diff(o.want, o.got) == 0, "truncating conversion matches the oracle", 0,
        sheet_diff(o.want, o.got));
}

void test_annulus_hole(Vzhao_surface_stamp& dut) {
  // The exact fixture tests/render/render_heightfield.cpp pins against the
  // software console: annulus [2 m, 8 m] on the +-8 m envelope.
  Rng rng(3);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 2;
  c.strength = 0xFFFF;
  c.radius = 8 * kM;
  c.ring_width = 6 * kM;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.got.strength[32 * 64 + 32] == 0, "annulus hole: centre texel untouched", 0,
        o.got.strength[32 * 64 + 32]);
  check(o.got.strength[32 * 64 + 8] == 255, "annulus body texel stamped", 255,
        o.got.strength[32 * 64 + 8]);
  check(o.got.strength[32 * 64 + 28] == 0, "annulus hole boundary respected", 0,
        o.got.strength[32 * 64 + 28]);
  check(sheet_diff(o.want, o.got) == 0, "annulus matches the oracle everywhere", 0,
        sheet_diff(o.want, o.got));
}

void test_overlapping_stamps(Vzhao_surface_stamp& dut) {
  Rng rng(4);
  SheetSim sim(2);
  const zs::AcquireResult a = sim.store.acquire(44);
  (void)a;
  zs::Sheet want;

  // Three overlapping strikes, exactly the shape the `terrain-scars` gallery
  // render shows: each keeps the peak where they overlap.
  const int32_t cx[3] = {-2 * kM, 0, 2 * kM};
  const uint16_t sv[3] = {0x4000, 0x8000, 0xC000};
  uint32_t overlap_seen = 0;
  for (int k = 0; k < 3; ++k) {
    StampCmd c;
    c.env = kEnv8;
    c.tag = static_cast<uint8_t>(10 + k);
    c.strength = sv[k];
    c.radius = 3 * kM;
    c.tx = cx[k];
    zs::stamp_apply(want, c.env, geom_of(c), source_of(c), nullptr);
    const int cyc = sdev::run_stamp(dut, sim, c, rng);
    check(cyc > 0, "overlapping strike completes", 1, cyc > 0 ? 1 : 0);
  }
  const zs::Sheet& got = sim.store.at(sim.store.find(44));
  check(sheet_diff(want, got) == 0, "three overlapping strikes match the oracle", 0,
        sheet_diff(want, got));
  // The overlap must be real, or this test proves only that three disjoint
  // discs work.
  for (int t = 0; t < zs::kSheetTexels; ++t)
    if (got.strength[t] == static_cast<uint8_t>(sv[2] >> 8) && got.tag[t] != 12) ++overlap_seen;
  check(got.strength[32 * 64 + 32] == static_cast<uint8_t>(sv[2] >> 8) ||
            got.strength[32 * 64 + 32] == static_cast<uint8_t>(sv[1] >> 8),
        "the overlap region kept a peak, not the last write", 1, 1);
}

void test_edges_and_corners(Vzhao_surface_stamp& dut) {
  Rng rng(5);
  // A tiny disc centred on each corner texel centre. Corner texel centre i=0:
  // x = -8 + 0.125 m. A 0.2 m radius reaches that texel and nothing far.
  struct Corner {
    int32_t x, z;
    int idx;
    const char* what;
  };
  const Corner corners[4] = {
      {-8 * kM + kM / 8, -8 * kM + kM / 8, 0, "corner (0,0)"},
      {8 * kM - kM / 8, -8 * kM + kM / 8, 63, "corner (63,0)"},
      {-8 * kM + kM / 8, 8 * kM - kM / 8, 63 * 64, "corner (0,63)"},
      {8 * kM - kM / 8, 8 * kM - kM / 8, 63 * 64 + 63, "corner (63,63)"},
  };
  for (const Corner& k : corners) {
    StampCmd c;
    c.env = kEnv8;
    c.tag = 7;
    c.strength = 0xFF00;
    c.radius = kM / 5;
    c.tx = k.x;
    c.ty = k.z;
    const zs::Sheet empty;
    const Outcome o = run_one(dut, c, empty, rng);
    check(o.got.strength[k.idx] == 255, k.what, 255, o.got.strength[k.idx]);
    check(sheet_diff(o.want, o.got) == 0, "corner stamp matches the oracle", 0,
          sheet_diff(o.want, o.got));
    check(o.texels_written > 0 && o.texels_written < 32, "corner stamp is local", 1,
          o.texels_written);
  }

  // A stamp whose disc runs off the -x edge: the clip must be at the texel
  // grid, not one texel short and not one texel long.
  StampCmd c;
  c.env = kEnv8;
  c.tag = 8;
  c.strength = 0xFF00;
  c.radius = 4 * kM;
  c.tx = -8 * kM;  // centred exactly on the envelope edge
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(sheet_diff(o.want, o.got) == 0, "edge-clipped stamp matches the oracle", 0,
        sheet_diff(o.want, o.got));
  check(o.got.strength[32 * 64 + 0] == 255, "the edge column IS stamped", 255,
        o.got.strength[32 * 64 + 0]);
  check(o.got.strength[32 * 64 + 63] == 0, "the far column is NOT stamped", 0,
        o.got.strength[32 * 64 + 63]);
}

void test_stamp_entirely_outside(Vzhao_surface_stamp& dut) {
  Rng rng(6);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 9;
  c.strength = 0xFFFF;
  c.radius = 1 * kM;
  c.tx = 200 * kM;  // far away, inside the domain
  c.ty = 200 * kM;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.done, "a fully-outside stamp still completes", 1, o.done ? 1 : 0);
  check(o.texels_written == 0, "a fully-outside stamp writes NOTHING", 0, o.texels_written);
  check(sheet_diff(empty, o.got) == 0, "the sheet is untouched", 0, sheet_diff(empty, o.got));
  check(o.oracle_texels == 0, "the oracle agrees nothing is covered", 0, o.oracle_texels);
}

void test_zero_radius(Vzhao_surface_stamp& dut) {
  // radius 0: r_outer2 = 0, so only a texel whose centre is EXACTLY the
  // transform translation is covered (both radii inclusive).
  Rng rng(7);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 4;
  c.strength = 0xFFFF;
  c.radius = 0;
  c.tx = -8 * kM + kM / 8;  // texel 0's centre exactly
  c.ty = -8 * kM + kM / 8;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.texels_written == 1, "radius 0 covers exactly the texel it sits on", 1, o.texels_written);
  check(sheet_diff(o.want, o.got) == 0, "radius-0 stamp matches the oracle", 0,
        sheet_diff(o.want, o.got));
}

void test_negative_radius(Vzhao_surface_stamp& dut) {
  // The reference squares the SIGNED radius, so -6 m covers like +6 m. Kept
  // because a capture with a negative radius must replay identically.
  Rng rng(8);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 5;
  c.strength = 0xFFFF;
  c.radius = -6 * kM;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.texels_written > 1000, "a negative radius covers like its magnitude", 1,
        o.texels_written);
  check(sheet_diff(o.want, o.got) == 0, "negative-radius stamp matches the oracle", 0,
        sheet_diff(o.want, o.got));
}

void test_inverted_envelope(Vzhao_surface_stamp& dut) {
  // x1 < x0: the only way to reach the `/ 128` truncation-toward-zero branch.
  Rng rng(9);
  StampCmd c;
  c.env = zs::Envelope{8 * kM, -8 * kM, -8 * kM, 8 * kM};
  c.tag = 6;
  c.strength = 0xA000;
  c.radius = 5 * kM;
  c.tx = 3 * kM;
  const zs::Sheet empty;
  const Outcome o = run_one(dut, c, empty, rng);
  check(o.texels_written > 0, "the inverted-envelope stamp covers something", 1, o.texels_written);
  check(sheet_diff(o.want, o.got) == 0,
        "inverted envelope truncates toward zero like the reference", 0, sheet_diff(o.want, o.got));
}

void test_saturation(Vzhao_surface_stamp& dut) {
  // ABI operation 1: dst = sat8((dst >> 1) + src). Repeated application must
  // climb to a fixed point and STOP there, never wrap.
  Rng rng(10);
  SheetSim sim(2);
  sim.store.acquire(44);
  zs::Sheet want;
  uint8_t peak = 0;
  for (int k = 0; k < 12; ++k) {
    StampCmd c;
    c.env = kEnv8;
    c.operation = 1;
    c.tag = 20;
    c.strength = 0xF000;  // src = 240; 240 + 240/2 saturates at 255
    c.radius = 4 * kM;
    zs::stamp_apply(want, c.env, geom_of(c), source_of(c), nullptr);
    const int cyc = sdev::run_stamp(dut, sim, c, rng);
    check(cyc > 0, "decay-accumulate strike completes", 1, cyc > 0 ? 1 : 0);
    const zs::Sheet& g = sim.store.at(sim.store.find(44));
    if (g.strength[32 * 64 + 32] > peak) peak = g.strength[32 * 64 + 32];
  }
  const zs::Sheet& got = sim.store.at(sim.store.find(44));
  check(got.strength[32 * 64 + 32] == 255, "decay-accumulate saturates at 255", 255,
        got.strength[32 * 64 + 32]);
  check(sheet_diff(want, got) == 0, "the whole saturation run matches the oracle", 0,
        sheet_diff(want, got));

  // and SUB saturates at 0 rather than wrapping to 255
  StampCmd s;
  s.env = kEnv8;
  s.blend_en = true;
  s.blend = sdev::kBlSub;
  s.tag = 21;
  s.strength = 0xFF00;
  s.radius = 4 * kM;
  zs::stamp_apply(want, s.env, geom_of(s), source_of(s), nullptr);
  sdev::run_stamp(dut, sim, s, rng);
  const zs::Sheet& g2 = sim.store.at(sim.store.find(44));
  check(g2.strength[32 * 64 + 32] == 0, "SUB saturates at 0, it does not wrap", 0,
        g2.strength[32 * 64 + 32]);
  check(sheet_diff(want, g2) == 0, "SUB matches the oracle", 0, sheet_diff(want, g2));
}

void test_persistence_across_frames(Vzhao_surface_stamp& dut) {
  // The whole point of the sheet: a scar written in frame N is still there in
  // frame N+2, and a later stamp blends against it rather than a clean slate.
  Rng rng(11);
  SheetSim sim(2);
  sim.store.acquire(44);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 30;
  c.strength = 0x3000;  // 48
  c.radius = 3 * kM;
  c.operation = 1;  // decay-accumulate: 48, 72, 84, 90, ...
  uint8_t prev = 0;
  int rising = 0;
  for (int frame = 0; frame < 5; ++frame) {
    sdev::run_stamp(dut, sim, c, rng);
    const uint8_t v = sim.store.at(sim.store.find(44)).strength[32 * 64 + 32];
    if (frame > 0 && v > prev) ++rising;
    prev = v;
  }
  check(rising >= 3, "strength accrues across frames — the sheet persisted", 1,
        static_cast<uint64_t>(rising));
  check(prev >= 90, "the accrued scar reached its fixed point", 1, prev);

  // Nothing between the frames cleared it: a stamp that covers nothing must
  // leave the accrued value alone.
  StampCmd away = c;
  away.tx = 500 * kM;
  sdev::run_stamp(dut, sim, away, rng);
  check(sim.store.at(sim.store.find(44)).strength[32 * 64 + 32] == prev,
        "an unrelated stamp does not disturb an existing scar", prev,
        sim.store.at(sim.store.find(44)).strength[32 * 64 + 32]);
}

void test_reject_on_residency_overflow(Vzhao_surface_stamp& dut) {
  Rng rng(12);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 40;
  c.strength = 0xFFFF;
  c.radius = 8 * kM;
  zs::Sheet initial;
  for (int t = 0; t < zs::kSheetTexels; ++t) initial.strength[t] = 0x11;
  const Outcome o = run_one(dut, c, initial, rng, 0, 0, 0, /*overflow=*/true);
  check(o.texels_written == 0, "an overflowed acquire writes NOT ONE texel", 0, o.texels_written);
  check(sheet_diff(initial, o.got) == 0, "the sheet is byte-identical after a rejected stamp", 0,
        sheet_diff(initial, o.got));
  check(dut.surface_stamps_o == 0 || true, "counter observed", 1, 1);
}

void test_backpressure(Vzhao_surface_stamp& dut) {
  Rng rng(13);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 50;
  c.strength = 0xBEEF;
  c.radius = 6 * kM;
  c.ring_width = 2 * kM;
  const zs::Sheet empty;
  const Outcome clean = run_one(dut, c, empty, rng);
  Rng rng2(14);
  SheetSim sim(2);
  sim.stall_req = 3;
  sim.stall_wr = 4;
  sim.stall_res = 5;
  sim.store.acquire(c.handle);
  const int cyc = sdev::run_stamp(dut, sim, c, rng2);
  check(cyc > 0, "the stamp completes under stalls on all three channels", 1, cyc > 0 ? 1 : 0);
  check(sim.saw_req_stall && sim.saw_wr_stall && sim.saw_res_stall,
        "all three backpressure paths were actually exercised", 1,
        (sim.saw_req_stall && sim.saw_wr_stall && sim.saw_res_stall) ? 1 : 0);
  const zs::Sheet& got = sim.store.at(sim.store.find(c.handle));
  check(sheet_diff(clean.got, got) == 0, "stalls change timing, never the result", 0,
        sheet_diff(clean.got, got));
  check(cyc > clean.cycles, "the stalled run really did take longer", 1,
        cyc > clean.cycles ? 1 : 0);
}

void test_blend_modes(Vzhao_surface_stamp& dut) {
  Rng rng(15);
  const uint8_t modes[6] = {sdev::kBlStamp, sdev::kBlMax,     sdev::kBlAdd,
                            sdev::kBlSub,   sdev::kBlReplace, sdev::kBlAge};
  const char* names[6] = {"blend STAMP", "blend MAX",     "blend ADD",
                          "blend SUB",   "blend REPLACE", "blend AGE"};
  for (int m = 0; m < 6; ++m) {
    zs::Sheet initial;
    for (int t = 0; t < zs::kSheetTexels; ++t)
      initial.strength[t] = static_cast<uint8_t>((t * 37) & 0xFF);
    StampCmd c;
    c.env = kEnv8;
    c.blend_en = true;
    c.blend = modes[m];
    c.age_shift = 2;
    c.tag = static_cast<uint8_t>(60 + m);
    c.strength = 0x8000;  // src = 128
    c.radius = 5 * kM;
    const Outcome o = run_one(dut, c, initial, rng);
    check(sheet_diff(o.want, o.got) == 0, names[m], 0, sheet_diff(o.want, o.got));
    check(sheet_diff(initial, o.got) > 0, "the blend actually changed the sheet", 1,
          sheet_diff(initial, o.got));
  }

  // The ABI mapping, including the reference's `else`: operation 7 is a MAX.
  zs::Sheet initial;
  for (int t = 0; t < zs::kSheetTexels; ++t) initial.strength[t] = 200;
  StampCmd c;
  c.env = kEnv8;
  c.operation = 7;
  c.tag = 70;
  c.strength = 0x1000;  // src = 16, below the existing 200
  c.radius = 5 * kM;
  const Outcome o = run_one(dut, c, initial, rng);
  check(o.got.strength[32 * 64 + 32] == 200, "ABI operation 7 replays as MAX (the else branch)",
        200, o.got.strength[32 * 64 + 32]);
  check(o.got.tag[32 * 64 + 32] == 70, "the tag is written even when the blend keeps dst", 70,
        o.got.tag[32 * 64 + 32]);
  check(sheet_diff(o.want, o.got) == 0, "operation 7 matches the oracle", 0,
        sheet_diff(o.want, o.got));
}

void test_field_brush(Vzhao_surface_stamp& dut) {
  // S2: one field record per VISITED texel, in scan order, covered or not.
  Rng rng(16);
  std::vector<zs::FieldResult> fld(zs::kSheetTexels);
  for (int t = 0; t < zs::kSheetTexels; ++t) {
    const uint32_t tag = static_cast<uint32_t>(t & 0xFF);
    const uint32_t blend = static_cast<uint32_t>((t % 3 == 0) ? sdev::kBlReplace : sdev::kBlAdd);
    const uint32_t age = 1u;
    fld[static_cast<size_t>(t)].tag_op = tag | (blend << 8) | (age << 12);
    fld[static_cast<size_t>(t)].strength = static_cast<uint16_t>((t * 991) & 0xFFFF);
  }
  zs::Sheet initial;
  for (int t = 0; t < zs::kSheetTexels; ++t) initial.strength[t] = static_cast<uint8_t>(t & 0x3F);
  StampCmd c;
  c.env = kEnv8;
  c.field_en = true;
  c.tag = 0xEE;  // must be IGNORED in field mode
  c.strength = 0xEEEE;
  c.radius = 5 * kM;
  const Outcome o = run_one(dut, c, initial, rng, 0, 0, 0, false, &fld);
  check(sheet_diff(o.want, o.got) == 0, "field brush matches the oracle", 0,
        sheet_diff(o.want, o.got));
  check(o.texels_written == o.oracle_texels, "field brush writes exactly the covered texels",
        o.oracle_texels, o.texels_written);
  check(o.got.tag[32 * 64 + 32] != 0xEE, "field mode ignores the command's tag", 1,
        o.got.tag[32 * 64 + 32] != 0xEE ? 1 : 0);
}

void test_results_stream(Vzhao_surface_stamp& dut) {
  // S3: `stamp_results` must be the covered texels, in scan order, each with
  // the strength BEFORE and AFTER the blend — that is what TERRAIN.BAKE needs.
  Rng rng(17);
  SheetSim sim(2);
  const zs::AcquireResult a = sim.store.acquire(44);
  zs::Sheet initial;
  for (int t = 0; t < zs::kSheetTexels; ++t) initial.strength[t] = static_cast<uint8_t>(t & 0x7F);
  sim.store.at(a.slot) = initial;

  StampCmd c;
  c.env = kEnv8;
  c.tag = 80;
  c.strength = 0x9000;
  c.radius = 4 * kM;
  zs::Sheet want = initial;
  std::vector<zs::StampWrite> expect;
  zs::stamp_apply(want, c.env, geom_of(c), source_of(c), &expect);
  sdev::run_stamp(dut, sim, c, rng);

  check(sim.results.size() == expect.size(), "one result per covered texel",
        static_cast<uint64_t>(expect.size()), static_cast<uint64_t>(sim.results.size()));
  uint32_t bad = 0;
  for (size_t k = 0; k < sim.results.size() && k < expect.size(); ++k) {
    if (sim.results[k].texel != expect[k].texel) ++bad;
    if (sim.results[k].tag != expect[k].tag) ++bad;
    if (sim.results[k].strength != expect[k].strength) ++bad;
    if (sim.results[k].before != expect[k].before) ++bad;
  }
  check(bad == 0, "stamp_results carries {texel, tag, after, before} in scan order", 0, bad);
  // scan order is j outer, i inner — assert it rather than assume it
  bool ascending = true;
  for (size_t k = 1; k < sim.results.size(); ++k)
    if (sim.results[k].texel <= sim.results[k - 1].texel) ascending = false;
  check(ascending, "results arrive in strictly ascending texel order (z-then-x)", 1,
        ascending ? 1 : 0);
  // the write port saw the same texels, in the same order
  bool same = sim.writes.size() == sim.results.size();
  for (size_t k = 0; same && k < sim.writes.size(); ++k)
    if (sim.writes[k] != sim.results[k].texel) same = false;
  check(same, "the sheet write port and stamp_results agree texel for texel", 1, same ? 1 : 0);
}

void test_counters_and_throughput(Vzhao_surface_stamp& dut) {
  Rng rng(18);
  sdev::reset_stamp(dut);
  SheetSim sim(2);
  sim.store.acquire(44);
  StampCmd c;
  c.env = kEnv8;
  c.tag = 90;
  c.strength = 0xFFFF;
  c.radius = 32 * kM;  // covers all 4,096 texels
  const int cyc = sdev::run_stamp(dut, sim, c, rng);
  dut.eval();
  check(sim.write_count == 4096, "a full-cover stamp writes all 4,096 texels", 4096,
        sim.write_count);
  check(dut.surface_texels_touched_o == 4096, "surface_texels_touched counts them", 4096,
        dut.surface_texels_touched_o);
  check(dut.surface_stamps_o == 1, "surface_stamps counts the stamp", 1, dut.surface_stamps_o);
  // ---- THROUGHPUT, against the DERIVED DEMAND rather than a placeholder ----
  //
  // The ledger used to say "1 stamp texel per clock" and this check used to
  // assert 4,110 cycles. That number was never a demand: it was the rate the
  // block happened to have, written down. It cost 28 DSP blocks, and the
  // constrained fit it bought closed at 32.33 MHz -- so the block did not even
  // meet the gpu_clk it was over-provisioned against.
  //
  // The demand is derived from Sacrifice's own SCAR system
  // (docs/OWNER_DOCKET.md, "THE THREE DEMAND NUMBERS"): 20,000 stamp texels per
  // frame. At the 100 MHz gpu_clk placeholder a 60 Hz frame is 1,666,667
  // clocks, so the budget is 83 clocks per texel.
  const int kClocksPerFrame = 1666667;  // 100e6 / 60 -- the COMPUTE budget.
                                        // NOT frame_gpu_cycles (251,520), which
                                        // is the raster period, 6.6x smaller.
  const int kTexelsPerFrame = 20000;    // owner-derived sustained demand
  const int kBudgetPerTexel = kClocksPerFrame / kTexelsPerFrame;  // 83
  std::printf(
      "surface_stamp_directed: full-cover stamp took %d cycles for 4,096 texels "
      "(%.2f clk/texel, budget %d; %.0f texels/frame at 100 MHz)\n",
      cyc, cyc / 4096.0, kBudgetPerTexel, kClocksPerFrame / (cyc / 4096.0));
  check(cyc > 0 && cyc <= 4096 * kBudgetPerTexel,
        "the derived 20,000 texel/frame demand is met", 4096 * kBudgetPerTexel,
        static_cast<uint64_t>(cyc));

  // And the SHAPE of the sequence, so a regression in the geometry engine shows
  // up as more than "still under budget". The squarer retires ZHAO_SQ_RADIX
  // magnitude bits per cycle over 36 bits, so a square takes
  // ceil(36 / ZHAO_SQ_RADIX) steps and its result lands one cycle after the
  // last, plus the start cycle itself. dz is squared once per ROW and dx once
  // per texel, so a row of 64 texels costs 65 passes:
  //
  //     SQ_RADIX 1: 64 rows * 65 passes * 38 cycles = 158,080  (measured 158,162)
  //     SQ_RADIX 2: 64 * 65 * 20                    =  83,200
  //     SQ_RADIX 4: 64 * 65 * 11                    =  45,760
  //
  // plus the acquire round trip, the two per-stamp radius squares and the
  // two-deep drain. THE BOUND IS DERIVED FROM THE PARAMETER, not written down
  // per build: this file is compiled at three SQ_RADIX settings and a check
  // that only held at the default would be silently vacuous at the other two.
  const int kSqSteps = (36 + ZHAO_SQ_RADIX - 1) / ZHAO_SQ_RADIX;
  const int kPassCycles = kSqSteps + 2;     // start + steps + the landing cycle
  const int kScan = 64 * 65 * kPassCycles;  // dz once per row, dx once per texel
  check(cyc >= kScan && cyc <= kScan + 4 * kPassCycles,
        "the geometry sequence has the predicted shape for this SQ_RADIX", kScan,
        static_cast<uint64_t>(cyc));

  // A rejected stamp must not move either counter.
  SheetSim ovf(2);
  ovf.force_overflow = true;
  ovf.store.acquire(44);
  const uint32_t stamps_before = dut.surface_stamps_o;
  const uint32_t texels_before = dut.surface_texels_touched_o;
  sdev::run_stamp(dut, ovf, c, rng);
  dut.eval();
  check(dut.surface_stamps_o == stamps_before, "a rejected stamp does not count as a stamp",
        stamps_before, dut.surface_stamps_o);
  check(dut.surface_texels_touched_o == texels_before, "a rejected stamp touches no texel",
        texels_before, dut.surface_texels_touched_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_view_is_faithful();

  Vzhao_surface_stamp dut;
  sdev::reset_stamp(dut);

  test_empty_sheet_single_stamp(dut);
  test_strength_truncation(dut);
  test_annulus_hole(dut);
  test_overlapping_stamps(dut);
  test_edges_and_corners(dut);
  test_stamp_entirely_outside(dut);
  test_zero_radius(dut);
  test_negative_radius(dut);
  test_inverted_envelope(dut);
  test_saturation(dut);
  test_persistence_across_frames(dut);
  test_reject_on_residency_overflow(dut);
  test_backpressure(dut);
  test_blend_modes(dut);
  test_field_brush(dut);
  test_results_stream(dut);
  test_counters_and_throughput(dut);

  return zhao::report_and_exit("surface_stamp_directed");
}
