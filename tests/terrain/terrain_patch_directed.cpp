// terrain_patch_directed.cpp — TERRAIN.PATCH against its oracle, and the
// oracle against the ratified law it claims to be a view of.
//
// THREE LAYERS, IN THIS ORDER, BECAUSE THE MIDDLE ONE IS THE ONE THAT USUALLY
// GETS SKIPPED:
//
//   1. `zref::terrain::compose_vertex` == `zref::render::compose_lattice` over
//      a real 33x33 dual patch with two real earth programs, every one of the
//      1,089 vertices, bit-for-bit. Without this the oracle would be a SECOND
//      implementation of terrain_rules §3.4 and charter §29-6 would be broken
//      before the RTL was even written. With it, "RTL == oracle" means "RTL ==
//      the composition the renderer and the sim column query already share".
//   2. The RTL against the oracle on the composition chain: both §3.4 clamps,
//      the closed-interval footprint test, saturation, the legacy page.
//   3. The RTL against terrain_rules §9.1 on the intake: capacity, one past
//      capacity, the reject trace event, no eviction, and the counter.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_patch.h"

#include "render_helpers.hpp"  // rtest::make_earth_prog, xform_identity
#include "zfield/zfield.hpp"   // THE one interpreter (terrain_rules §4.1)
#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"
#include "zrender/internal.hpp"  // white-box: compose_lattice, FieldApp

using zhao::check;
namespace zt = zref::terrain;
namespace zr = zref::render;

namespace {

// zhao::reset() assumes the byte-stream ports (in_valid/in_data) the harness's
// first consumers had; this block has neither. Local reset, same async-negedge
// semantics.
void reset_dut(Vzhao_terrain_patch& top) {
  top.rst_n = 0;
  top.list_clear_i = 0;
  top.patch_id_i = 0;
  top.fld_add_valid_i = 0;
  top.vtx_valid_i = 0;
  top.fld_valid_i = 0;
  top.st_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

/** Offer one field record to the intake. Returns true iff accepted. */
bool offer(Vzhao_terrain_patch& dut, const zt::FieldRecord& f, uint16_t patch_id, bool clear,
           uint32_t* trace_hash = nullptr, uint16_t* trace_cmd = nullptr,
           uint16_t* trace_patch = nullptr) {
  dut.list_clear_i = clear ? 1 : 0;
  dut.patch_id_i = patch_id;
  dut.fld_add_valid_i = 1;
  dut.fld_add_x0_i = f.x0;
  dut.fld_add_z0_i = f.z0;
  dut.fld_add_x1_i = f.x1;
  dut.fld_add_z1_i = f.z1;
  dut.fld_add_hash_i = f.program_hash;
  dut.fld_add_cmd_i = f.cmd_index;
  zhao::tick(dut);
  dut.fld_add_valid_i = 0;
  dut.list_clear_i = 0;
  const bool accepted = dut.fld_add_accept_o != 0;
  if (!accepted) {
    if (trace_hash != nullptr) *trace_hash = dut.trace_hash_o;
    if (trace_cmd != nullptr) *trace_cmd = dut.trace_cmd_o;
    if (trace_patch != nullptr) *trace_patch = dut.trace_patch_id_o;
  }
  return accepted;
}

/** Load a whole field list into a fresh intake, asserting accept/reject. */
void load_list(Vzhao_terrain_patch& dut, const zt::FieldList& list, uint16_t patch_id) {
  bool first = true;
  for (int i = 0; i < list.size(); ++i) {
    const bool ok = offer(dut, list[i], patch_id, first);
    check(ok, "a record inside capacity is accepted", 1, ok ? 1 : 0);
    first = false;
  }
  if (first) {  // an empty list still needs the clear
    dut.list_clear_i = 1;
    zhao::tick(dut);
    dut.list_clear_i = 0;
  }
  check(dut.fields_active_o == list.size(), "fields_active matches the loaded list", list.size(),
        dut.fields_active_o);
}

/**
 * The empty field-lane array, VALUE-INITIALISED and passed instead of a null
 * pointer. `compose_vertex` never dereferences it when the list is empty, but
 * cppcheck cannot see that across the call and reports a possible null
 * dereference — the same family as the `uninitvar` trap that bites arrays
 * filled through helper calls. Handing it a real array costs nothing and
 * removes the question.
 */
constexpr int32_t kNoFields[zt::kMaxPatchFields] = {};

struct RtlOut {
  int32_t top = 0;
  int32_t bottom = 0;
  int32_t compose_top = 0;
  bool dirty = false;
  uint16_t src_id = 0;
  bool seen = false;
};

/**
 * Drive one vertex plus its field-result burst through and collect the result.
 * `field_h[i]` is the height out-lane of accepted list entry i at this vertex —
 * one result per ACCEPTED lane, in list order, whether or not the lane's
 * footprint covers the vertex (the block gates it; chosen law 2).
 */
RtlOut compose_one(Vzhao_terrain_patch& dut, const zt::ComposeIn& in, int n_fields,
                   const int32_t* field_h, int vi, int vj, uint16_t src) {
  dut.st_ready_i = 1;
  dut.base_i = in.base;
  dut.scar_i = in.scar;
  dut.bottom_i = in.bottom;
  dut.dual_i = in.dual ? 1 : 0;
  dut.wx_i = in.wx;
  dut.wz_i = in.wz;
  dut.vi_i = static_cast<uint8_t>(vi);
  dut.vj_i = static_cast<uint8_t>(vj);
  dut.src_id_i = src;
  dut.vtx_valid_i = 1;
  dut.fld_valid_i = 0;

  RtlOut out;
  int lane = 0;
  // Generous drain: a latency change must show up as a MISSING result, never as
  // a wrong one silently read from the previous vertex.
  for (int cycle = 0; cycle < 64 && !out.seen; ++cycle) {
    if (lane < n_fields) {
      dut.fld_valid_i = 1;
      dut.fld_height_i = field_h[lane];
    } else {
      dut.fld_valid_i = 0;
    }
    dut.eval();
    const bool took_vtx = dut.vtx_valid_i && dut.vtx_ready_o;
    const bool took_fld = dut.fld_valid_i && dut.fld_ready_o;
    zhao::tick(dut);
    if (took_vtx) dut.vtx_valid_i = 0;
    if (took_fld) ++lane;
    if (dut.st_valid_o) {
      out.top = static_cast<int32_t>(dut.top_o);
      out.bottom = static_cast<int32_t>(dut.bottom_o);
      out.compose_top = static_cast<int32_t>(dut.compose_top_o);
      out.dirty = dut.st_dirty_o != 0;
      out.src_id = dut.st_src_id_o;
      out.seen = true;
    }
  }
  check(out.seen, "a vertex produces a patch_state record", 1, out.seen ? 1 : 0);
  dut.fld_valid_i = 0;
  zhao::tick(dut);  // let it retire
  return out;
}

/** Compare RTL against the oracle for one vertex. */
void expect(Vzhao_terrain_patch& dut, const zt::ComposeIn& in, const zt::FieldList& list,
            const int32_t* field_h, const char* what, uint16_t src = 7) {
  const zt::ComposeOut want = zt::compose_vertex(in, list, field_h);
  const RtlOut got = compose_one(dut, in, list.size(), field_h, 4, 4, src);
  check(got.top == want.live_top, what, static_cast<uint32_t>(want.live_top),
        static_cast<uint32_t>(got.top));
  check(got.compose_top == want.compose_top, what, static_cast<uint32_t>(want.compose_top),
        static_cast<uint32_t>(got.compose_top));
  check(got.bottom == want.bottom, what, static_cast<uint32_t>(want.bottom),
        static_cast<uint32_t>(got.bottom));
  check(got.dirty == want.dirty, what, want.dirty ? 1 : 0, got.dirty ? 1 : 0);
  check(got.src_id == src, "src_id rides its own record", src, got.src_id);
}

// ---------------------------------------------------------------------------
// LAYER 1: the oracle IS compose_lattice
// ---------------------------------------------------------------------------
// Build a real 33x33 dual Island-Patch-shaped page with scars, a thin authored
// lip (bottom above base -> §3.4 clamp 1 fires), void cells (whose corner
// vertices are still composed, §3.2), and TWO live earth programs with
// different footprints and different p0 signs (so the fx_add chain has an
// order, and so `covers` is false for a real subset of vertices). Then compose
// it both ways and require every vertex to agree.
void test_oracle_is_compose_lattice() {
  constexpr int W = 33;
  zr::TerrainPatch patch;
  patch.width = patch.height = W;
  patch.env_x0 = patch.env_z0 = -(32 << 16);
  patch.env_x1 = patch.env_z1 = (32 << 16);
  patch.heights.resize(static_cast<size_t>(W) * W);
  patch.scar.resize(static_cast<size_t>(W) * W);
  patch.bottom.resize(static_cast<size_t>(W) * W);
  patch.cell_state.assign(32 * 32, zt::kSolid);
  for (int j = 0; j < W; ++j) {
    for (int i = 0; i < W; ++i) {
      const size_t k = static_cast<size_t>(j) * W + i;
      patch.heights[k] = static_cast<int16_t>(2560 + ((i * 7) % 11) * 32 + ((j * 5) % 13) * 16);
      patch.scar[k] = static_cast<int16_t>(-(((i + 2 * j) % 17) * 24));
      patch.bottom[k] = static_cast<int16_t>(-2048 + ((i + j) % 5) * 64);
    }
  }
  // the thin authored lip: bottom ABOVE base+scar at two vertices
  patch.bottom[5 * W + 5] = static_cast<int16_t>(patch.heights[5 * W + 5] + 100);
  patch.bottom[5 * W + 6] = static_cast<int16_t>(patch.heights[5 * W + 6] + 80);
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci * 3 + cj * 7) % 23 == 0) patch.cell_state[cj * 32 + ci] = zt::kVoidAuthored;

  const std::vector<uint8_t> prog_bytes = rtest::make_earth_prog();
  const zfield::DecodeResult dec = zfield::decode(prog_bytes.data(), prog_bytes.size());
  check(dec.error == zfield::DecodeError::kOk, "earth program decodes", 0,
        static_cast<uint32_t>(dec.error));

  // two apps: a big negative one over the whole envelope (drives the LIVE clamp
  // at bottom), and a smaller positive one over one quadrant only (so `covers`
  // is false for three quarters of the lattice).
  std::vector<zr::FieldApp> apps(2);
  const int32_t p0s[2] = {-(20 << 16), (6 << 16)};
  const int32_t fp[2][4] = {{-(32 << 16), -(32 << 16), (32 << 16), (32 << 16)},
                            {0, 0, (32 << 16), (32 << 16)}};
  for (int a = 0; a < 2; ++a) {
    apps[static_cast<size_t>(a)].prog = &dec.prog;
    std::memset(&apps[static_cast<size_t>(a)].cmd, 0, sizeof(apps[static_cast<size_t>(a)].cmd));
    apps[static_cast<size_t>(a)].cmd.program = static_cast<uint32_t>(a + 1);
    apps[static_cast<size_t>(a)].cmd.footprint.x0 = fp[a][0];
    apps[static_cast<size_t>(a)].cmd.footprint.y0 = fp[a][1];
    apps[static_cast<size_t>(a)].cmd.footprint.x1 = fp[a][2];
    apps[static_cast<size_t>(a)].cmd.footprint.y1 = fp[a][3];
    apps[static_cast<size_t>(a)].cmd.start_tick = 0;
    apps[static_cast<size_t>(a)].cmd.duration_ticks = 100;
    for (int b = 0; b < 4; ++b)
      apps[static_cast<size_t>(a)].cmd.parameters[b] = static_cast<uint8_t>(p0s[a] >> (8 * b));
  }

  zref::SatLedger L;
  const zt::ComposedLattice lat =
      zr::compose_lattice(patch, rtest::xform_identity(), apps, 50, nullptr, &L);
  check(lat.dual && lat.w == W && lat.h == W, "composed lattice is dual 33x33", 1,
        (lat.dual && lat.w == W) ? 1 : 0);

  // the same two records through the intake oracle
  zt::FieldList list;
  for (int a = 0; a < 2; ++a) {
    zt::FieldRecord r;
    r.x0 = fp[a][0];
    r.z0 = fp[a][1];
    r.x1 = fp[a][2];
    r.z1 = fp[a][3];
    r.program_hash = static_cast<uint32_t>(0xA5000000u + a);
    r.cmd_index = static_cast<uint16_t>(a);
    check(list.offer(r, 1), "the intake accepts record inside capacity", 1, 1);
  }

  // The field height lane at a vertex: this program is `height <- p0`, so the
  // lane is p0 — but it is obtained by CALLING the one interpreter with the
  // §7.1 input record compose_lattice builds, not by assuming it.
  const auto height_lane = [&](int a, int32_t cx, int32_t cz) -> int32_t {
    const zfield::Decoded& prog = dec.prog;
    const uint32_t age = 50;  // frame_tick 50, start_tick 0, duration 100
    int32_t in[12] = {cx, cz, static_cast<int32_t>(age), 0};
    in[3] = static_cast<int32_t>((static_cast<uint64_t>(age) * (1u << 16) + 100 / 2) / 100);
    const size_t n_in = prog.in_lanes.size() < 12 ? prog.in_lanes.size() : 12;
    for (size_t k = 4; k < n_in; ++k) {
      uint32_t lane = 0;
      std::memcpy(&lane, apps[static_cast<size_t>(a)].cmd.parameters + 4 * (k - 4), 4);
      in[k] = static_cast<int32_t>(lane);
    }
    int32_t out[4] = {0, 0, 0, 0};
    const size_t n_out = prog.out_lanes.size() < 4 ? prog.out_lanes.size() : 4;
    zfield::interpret(prog, in, n_in, out, n_out);
    return out[0];
  };

  int mismatches = 0;
  int clamped_live = 0;
  int clamped_compose = 0;
  int uncovered = 0;
  for (int j = 0; j < W; ++j) {
    for (int i = 0; i < W; ++i) {
      const size_t k = static_cast<size_t>(j) * W + i;
      zt::ComposeIn in;
      in.base = patch.heights[k];
      in.scar = patch.scar[k];
      in.bottom = patch.bottom[k];
      in.dual = true;
      in.wx = lat.wx[static_cast<size_t>(i)];
      in.wz = lat.wz[static_cast<size_t>(j)];
      int32_t fh[2] = {0, 0};
      for (int a = 0; a < 2; ++a) fh[a] = height_lane(a, in.wx, in.wz);
      const zt::ComposeOut got = zt::compose_vertex(in, list, fh);
      if (got.live_top != lat.top[k] || got.bottom != lat.bottom[k]) ++mismatches;
      if (got.live_top == got.bottom) ++clamped_live;
      if (got.compose_top == static_cast<int32_t>(in.bottom) << 8) ++clamped_compose;
      if (!zt::covers(list[1], in.wx, in.wz)) ++uncovered;
    }
  }
  check(mismatches == 0, "the oracle equals compose_lattice at every one of 1,089 vertices", 0,
        static_cast<uint32_t>(mismatches));
  // The cross-check must have EXERCISED the interesting states, or it proved
  // nothing but that two zero functions agree.
  check(clamped_live > 200, "the live clamp actually engaged in the cross-check", 1,
        static_cast<uint32_t>(clamped_live));
  check(clamped_compose > 0, "the compose clamp (the thin lip) actually engaged", 1,
        static_cast<uint32_t>(clamped_compose));
  check(uncovered > 500, "a real subset of vertices was outside the second footprint", 1,
        static_cast<uint32_t>(uncovered));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_oracle_is_compose_lattice();

  Vzhao_terrain_patch dut;
  reset_dut(dut);
  dut.st_ready_i = 1;

  constexpr int32_t kOne = 1 << 16;

  // =========================================================================
  // 1. the composition chain, no live fields
  // =========================================================================
  {
    zt::FieldList empty;
    load_list(dut, empty, 1);

    // base + scar, no clamp needed: the identity case.
    zt::ComposeIn in;
    in.base = 1000;  // ~3.9 m
    in.scar = -200;
    in.bottom = -500;
    in.dual = true;
    expect(dut, in, empty, kNoFields, "base + scar, above bottom");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, empty, kNoFields);
      check(w.live_top == ((1000 - 200) << 8), "the height16 up-conversion is an exact <<8",
            static_cast<uint32_t>(800 << 8), static_cast<uint32_t>(w.live_top));
      check(w.dirty, "a scarred vertex reports dirty", 1, w.dirty ? 1 : 0);
    }

    // zero scar: NOT dirty, because the ground has not moved.
    in.scar = 0;
    expect(dut, in, empty, kNoFields, "zero scar composes to base exactly");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, empty, kNoFields);
      check(!w.dirty, "an unscarred, unfielded vertex is not dirty", 0, w.dirty ? 1 : 0);
    }

    // §3.4 CLAMP 1: base + scar below bottom -> compose_top == fx(bottom).
    in.base = 100;
    in.scar = -900;
    in.bottom = -300;
    expect(dut, in, empty, kNoFields, "base + scar clamps UP to bottom");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, empty, kNoFields);
      check(w.compose_top == (-300 << 8), "clamp 1 lands exactly on fx(bottom)",
            static_cast<uint32_t>(-300 << 8), static_cast<uint32_t>(w.compose_top));
      check(w.live_top == w.compose_top, "with no fields live_top == compose_top",
            static_cast<uint32_t>(w.compose_top), static_cast<uint32_t>(w.live_top));
    }

    // The THIN AUTHORED LIP (§3.6): bottom strictly ABOVE base. Legal content,
    // and clamp 1 is what makes it a slab rather than an inverted column.
    in.base = 400;
    in.scar = 0;
    in.bottom = 600;
    expect(dut, in, empty, kNoFields, "a thin lip clamps the top up to its own bottom");

    // THE LEGACY SINGLE-SURFACE PAGE (terrain_rules §3.1 option (a), kept as
    // the degenerate case): NO underside, so NO clamp, and `bottom` is meant to
    // be ignored entirely. A block that clamped anyway would change every
    // legacy page's pixels.
    in.dual = false;
    in.base = 100;
    in.scar = -900;
    in.bottom = 30000;  // deliberately absurd: it must not be consulted
    expect(dut, in, empty, kNoFields, "a legacy page ignores bottom entirely");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, empty, kNoFields);
      check(w.live_top == ((100 - 900) << 8), "the legacy page composes base + scar unclamped",
            static_cast<uint32_t>((-800) << 8), static_cast<uint32_t>(w.live_top));
      check(w.bottom == w.live_top, "a legacy lattice reports bottom == top",
            static_cast<uint32_t>(w.live_top), static_cast<uint32_t>(w.bottom));
    }

    // the height16 rails, both signs
    in.dual = true;
    in.base = 32767;
    in.scar = 32767;
    in.bottom = -32768;
    expect(dut, in, empty, kNoFields, "both height16 rails compose without saturating fx16");
    in.base = -32768;
    in.scar = -32768;
    in.bottom = -32768;
    expect(dut, in, empty, kNoFields, "the negative height16 rails clamp at bottom");
  }

  // =========================================================================
  // 2. the field chain and the closed-interval footprint test
  // =========================================================================
  {
    // one lane covering [0,0]..[16,16] world units
    zt::FieldList list;
    zt::FieldRecord r;
    r.x0 = 0;
    r.z0 = 0;
    r.x1 = 16 * kOne;
    r.z1 = 16 * kOne;
    r.program_hash = 0xDEADBEEFu;
    r.cmd_index = 3;
    list.offer(r, 1);
    load_list(dut, list, 1);

    zt::ComposeIn in;
    in.base = 1000;
    in.scar = 0;
    in.bottom = -10000;
    in.dual = true;
    const int32_t fh[1] = {-(4 * kOne)};

    // THE CLOSED INTERVAL (terrain_rules §9.1): a vertex exactly on a footprint
    // edge is INSIDE. All four edges, and one raw unit outside each.
    struct Probe {
      int32_t wx, wz;
      bool inside;
      const char* what;
    };
    const Probe probes[] = {
        {0, 0, true, "footprint corner (x0,z0) is inside"},
        {16 * kOne, 16 * kOne, true, "footprint corner (x1,z1) is inside"},
        {0, 16 * kOne, true, "footprint corner (x0,z1) is inside"},
        {16 * kOne, 0, true, "footprint corner (x1,z0) is inside"},
        {-1, 0, false, "one raw unit left of x0 is outside"},
        {16 * kOne + 1, 0, false, "one raw unit right of x1 is outside"},
        {0, -1, false, "one raw unit before z0 is outside"},
        {0, 16 * kOne + 1, false, "one raw unit past z1 is outside"},
        {8 * kOne, 8 * kOne, true, "the footprint interior is inside"},
    };
    for (const Probe& p : probes) {
      in.wx = p.wx;
      in.wz = p.wz;
      check(zt::covers(list[0], p.wx, p.wz) == p.inside, p.what, p.inside ? 1 : 0,
            zt::covers(list[0], p.wx, p.wz) ? 1 : 0);
      expect(dut, in, list, fh, p.what);
    }

    // An INVERTED rectangle covers nothing — the reference's behaviour, not a
    // special case bolted on here.
    zt::FieldList inv;
    zt::FieldRecord bad = r;
    bad.x1 = -kOne;
    inv.offer(bad, 1);
    load_list(dut, inv, 1);
    in.wx = 0;
    in.wz = 0;
    check(!zt::covers(inv[0], 0, 0), "an inverted footprint covers nothing", 0,
          zt::covers(inv[0], 0, 0) ? 1 : 0);
    expect(dut, in, inv, fh, "an inverted footprint adds nothing");

    // §3.4 CLAMP 2: the live field drives the top below bottom -> clamped, and
    // compose_top is UNTOUCHED (a transient can never fake a breach).
    zt::FieldList one;
    one.offer(r, 1);
    load_list(dut, one, 1);
    in.wx = 8 * kOne;
    in.wz = 8 * kOne;
    in.base = 100;
    in.scar = 0;
    in.bottom = -50;
    const int32_t deep[1] = {-(500 * kOne)};
    expect(dut, in, one, deep, "the live clamp catches a wave punching below bottom");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, one, deep);
      check(w.live_top == (-50 << 8), "live_top lands exactly on fx(bottom)",
            static_cast<uint32_t>(-50 << 8), static_cast<uint32_t>(w.live_top));
      check(w.compose_top == (100 << 8), "compose_top is untouched by the live lane",
            static_cast<uint32_t>(100 << 8), static_cast<uint32_t>(w.compose_top));
    }
  }

  // =========================================================================
  // 3. saturation and order: the fx_add chain is order-dependent ON PURPOSE
  // =========================================================================
  {
    // Two lanes, +INT32_MAX then -INT32_MAX. The first add saturates, so the
    // chain does NOT come back to where it started: (x + MAX)|sat - MAX. A block
    // that accumulated wide and narrowed once at the end would disagree.
    zt::FieldList list;
    zt::FieldRecord r;
    r.x0 = -(1 << 30);
    r.z0 = -(1 << 30);
    r.x1 = (1 << 30);
    r.z1 = (1 << 30);
    for (int i = 0; i < 2; ++i) {
      r.cmd_index = static_cast<uint16_t>(i);
      list.offer(r, 1);
    }
    load_list(dut, list, 1);

    zt::ComposeIn in;
    in.base = 1000;
    in.scar = 0;
    in.bottom = -32768;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;
    const int32_t up_then_down[2] = {INT32_MAX, INT32_MIN};
    const int32_t down_then_up[2] = {INT32_MIN, INT32_MAX};
    expect(dut, in, list, up_then_down, "saturate-then-subtract does not undo itself");
    expect(dut, in, list, down_then_up, "subtract-then-saturate is the other answer");
    {
      const zt::ComposeOut a = zt::compose_vertex(in, list, up_then_down);
      const zt::ComposeOut b = zt::compose_vertex(in, list, down_then_up);
      check(a.live_top != b.live_top,
            "the two lane orders genuinely differ (saturating adds do not commute)", 1,
            a.live_top != b.live_top ? 1 : 0);
    }
  }

  // =========================================================================
  // 4. the §9.1 intake: capacity, one past it, no eviction, the trace event
  // =========================================================================
  {
    reset_dut(dut);
    dut.st_ready_i = 1;

    // AT capacity: 16 records accepted, fields_active == 16.
    zt::FieldRecord recs[20];
    for (int i = 0; i < 20; ++i) {
      recs[i].x0 = -(1 << 30);
      recs[i].z0 = -(1 << 30);
      recs[i].x1 = (1 << 30);
      recs[i].z1 = (1 << 30);
      recs[i].program_hash = 0x1000u + static_cast<uint32_t>(i);
      recs[i].cmd_index = static_cast<uint16_t>(100 + i);
    }
    zt::FieldList oracle;
    oracle.reset();
    for (int i = 0; i < zt::kMaxPatchFields; ++i) {
      const bool got = offer(dut, recs[i], 0x2BAD, i == 0);
      const bool want = oracle.offer(recs[i], 0x2BAD);
      check(got == want, "accept verdict matches the oracle inside capacity", want ? 1 : 0,
            got ? 1 : 0);
    }
    check(dut.fields_active_o == 16, "the list fills to exactly MAX_PATCH_FIELDS = 16", 16,
          dut.fields_active_o);
    check(dut.programs_rejected_o == 0, "nothing is rejected at capacity", 0,
          dut.programs_rejected_o);

    // ONE PAST capacity: rejected, counted, traced — and NOTHING EVICTED.
    for (int i = zt::kMaxPatchFields; i < 20; ++i) {
      uint32_t th = 0;
      uint16_t tc = 0;
      uint16_t tp = 0;
      const bool got = offer(dut, recs[i], 0x2BAD, false, &th, &tc, &tp);
      const bool want = oracle.offer(recs[i], 0x2BAD);
      check(got == want, "the record past capacity is REJECTED", want ? 1 : 0, got ? 1 : 0);
      check(!got, "reject, never silently drop and never overrun", 0, got ? 1 : 0);
      check(th == recs[i].program_hash, "the trace event carries the program hash",
            recs[i].program_hash, th);
      check(tc == recs[i].cmd_index, "the trace event carries the command index", recs[i].cmd_index,
            tc);
      check(tp == 0x2BAD, "the trace event carries the patch id", 0x2BAD, tp);
      check(dut.fields_active_o == 16, "the list never grows past 16", 16, dut.fields_active_o);
    }
    check(dut.programs_rejected_o == 4, "programs_rejected counts every rejected record", 4,
          dut.programs_rejected_o);
    check(dut.programs_rejected_o == oracle.programs_rejected(),
          "programs_rejected matches the oracle", oracle.programs_rejected(),
          dut.programs_rejected_o);
    check(oracle.trace().size() == 4, "the oracle traced every reject", 4,
          static_cast<uint32_t>(oracle.trace().size()));

    // NO EVICTION: the surviving list must still be the FIRST 16 in command
    // order. Prove it by composing with 16 distinct lane heights and checking
    // the sum, which is only right if the accepted set and its order are right.
    // (Every footprint here is the whole world, so all 16 lanes apply.)
    zt::ComposeIn in;
    in.base = 0;
    in.scar = 0;
    in.bottom = -32768;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;
    int32_t fh[16];
    for (int i = 0; i < 16; ++i) fh[i] = (i + 1) * (1 << 16);
    expect(dut, in, oracle, fh, "the first 16 in command order win, and in that order");
    {
      const zt::ComposeOut w = zt::compose_vertex(in, oracle, fh);
      check(w.live_top == 136 * (1 << 16), "the 16 accepted lanes sum to 1+..+16 = 136",
            static_cast<uint32_t>(136 << 16), static_cast<uint32_t>(w.live_top));
    }

    // list_clear_i empties the LIST but not the COUNTER (chosen law 4).
    const uint32_t rejected_before = dut.programs_rejected_o;
    dut.list_clear_i = 1;
    zhao::tick(dut);
    dut.list_clear_i = 0;
    check(dut.fields_active_o == 0, "list_clear empties the per-patch list", 0,
          dut.fields_active_o);
    check(dut.programs_rejected_o == rejected_before, "list_clear does NOT clear programs_rejected",
          rejected_before, dut.programs_rejected_o);

    // clear AND offer in the same cycle: the record lands in the fresh list.
    const bool ok = offer(dut, recs[0], 0x2BAD, true);
    check(ok, "a clear and an offer in one cycle admit the record", 1, ok ? 1 : 0);
    check(dut.fields_active_o == 1, "the record lands in slot 0 of the fresh list", 1,
          dut.fields_active_o);
  }

  // =========================================================================
  // 5. the subpatch dirty mask (charter §11.1)
  // =========================================================================
  {
    reset_dut(dut);
    dut.st_ready_i = 1;
    zt::FieldList empty;
    load_list(dut, empty, 5);

    struct MaskCase {
      int vi, vj;
      int bits;
      const char* what;
    };
    // A 33x33 lattice over a 4x4 grid of 8-cell subpatches. Interior vertices
    // touch one subpatch; a vertex on one internal border touches two; a vertex
    // on two internal borders touches four; the lattice CORNERS touch one.
    const MaskCase cases[] = {
        {4, 4, 1, "an interior vertex marks one subpatch"},
        {0, 0, 1, "the lattice corner marks one subpatch"},
        {32, 32, 1, "the far lattice corner marks one subpatch"},
        {32, 4, 1, "a vertex on the lattice edge marks one subpatch"},
        {8, 4, 2, "a vertex on an internal x border marks two subpatches"},
        {4, 16, 2, "a vertex on an internal z border marks two subpatches"},
        {8, 8, 4, "a vertex on two internal borders marks four subpatches"},
        {24, 16, 4, "the far internal cross marks four subpatches"},
    };
    for (const MaskCase& c : cases) {
      reset_dut(dut);
      dut.st_ready_i = 1;
      load_list(dut, empty, 5);
      zt::ComposeIn in;
      in.base = 100;
      in.scar = -7;  // nonzero: the vertex MOVED, so it may mark
      in.bottom = -32768;
      in.dual = true;
      compose_one(dut, in, 0, kNoFields, c.vi, c.vj, 1);
      const uint16_t want = zt::subpatch_mask(c.vi, c.vj);
      int n = 0;
      for (int b = 0; b < 16; ++b)
        if ((want >> b) & 1) ++n;
      check(n == c.bits, c.what, c.bits, n);
      check(dut.subpatch_dirty_o == want, c.what, want, dut.subpatch_dirty_o);
    }

    // THE FULL SWEEP: all 33x33 lattice vertices, not eight spot cases. A
    // randomized lane found the RTL and the oracle disagreeing on ONE bit for
    // some vertices, which eight hand-picked cases missed entirely.
    {
      reset_dut(dut);
      dut.st_ready_i = 1;
      load_list(dut, empty, 5);
      int mismatches = 0;
      int first_vi = -1, first_vj = -1;
      uint16_t first_want = 0, first_got = 0;
      for (int vj = 0; vj <= 32; ++vj) {
        for (int vi = 0; vi <= 32; ++vi) {
          dut.list_clear_i = 1;
          zhao::tick(dut);
          dut.list_clear_i = 0;
          zt::ComposeIn in;
          in.base = 100;
          in.scar = -7;  // the vertex MOVED, so it marks
          in.bottom = -32768;
          in.dual = true;
          compose_one(dut, in, 0, kNoFields, vi, vj, 1);
          const uint16_t want = zt::subpatch_mask(vi, vj);
          if (dut.subpatch_dirty_o != want) {
            if (mismatches == 0) {
              first_vi = vi;
              first_vj = vj;
              first_want = want;
              first_got = static_cast<uint16_t>(dut.subpatch_dirty_o);
            }
            ++mismatches;
          }
        }
      }
      if (mismatches != 0)
        std::fprintf(stderr, "  first mask mismatch at (%d,%d): want %04X got %04X\n", first_vi,
                     first_vj, first_want, first_got);
      check(mismatches == 0, "the subpatch mask agrees at every one of 1,089 lattice vertices", 0,
            static_cast<uint32_t>(mismatches));
    }

    // A vertex that did NOT move marks nothing — the whole point of the mask.
    reset_dut(dut);
    dut.st_ready_i = 1;
    load_list(dut, empty, 5);
    zt::ComposeIn still;
    still.base = 100;
    still.scar = 0;
    still.bottom = -32768;
    still.dual = true;
    compose_one(dut, still, 0, kNoFields, 4, 4, 1);
    check(dut.subpatch_dirty_o == 0, "an unmoved vertex marks no subpatch dirty", 0,
          dut.subpatch_dirty_o);
    // and a moved one, in the same patch, accumulates
    still.scar = -7;
    compose_one(dut, still, 0, kNoFields, 20, 20, 1);
    check(dut.subpatch_dirty_o == zt::subpatch_mask(20, 20), "the mask accumulates over a patch",
          zt::subpatch_mask(20, 20), dut.subpatch_dirty_o);
    dut.list_clear_i = 1;
    zhao::tick(dut);
    dut.list_clear_i = 0;
    check(dut.subpatch_dirty_o == 0, "list_clear clears the dirty mask for the next frame", 0,
          dut.subpatch_dirty_o);
  }

  // =========================================================================
  // 6. backpressure — on both the result port and the field-result stream
  // =========================================================================
  {
    reset_dut(dut);
    zt::FieldList empty;
    dut.st_ready_i = 1;
    load_list(dut, empty, 9);

    // (a) a stalled consumer must not lose or corrupt a result
    dut.st_ready_i = 0;
    dut.base_i = 1234;
    dut.scar_i = -34;
    dut.bottom_i = -32768;
    dut.dual_i = 1;
    dut.wx_i = 0;
    dut.wz_i = 0;
    dut.vi_i = 3;
    dut.vj_i = 3;
    dut.src_id_i = 0xC0DE;
    dut.vtx_valid_i = 1;
    zhao::tick(dut);
    dut.vtx_valid_i = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(dut);
    check(dut.st_valid_o == 1, "a stalled result waits at the output", 1, dut.st_valid_o);
    check(static_cast<int32_t>(dut.top_o) == ((1234 - 34) << 8),
          "a stalled result is not corrupted while it waits",
          static_cast<uint32_t>((1234 - 34) << 8), static_cast<uint32_t>(dut.top_o));
    check(dut.st_src_id_o == 0xC0DE, "src_id survives the stall", 0xC0DE, dut.st_src_id_o);
    check(dut.vtx_ready_o == 0, "a blocked output deasserts vtx_ready", 0, dut.vtx_ready_o);
    dut.st_ready_i = 1;
    zhao::tick(dut);
    check(dut.st_valid_o == 0, "the result retires exactly once", 0, dut.st_valid_o);

    // (b) THE INVARIANT the field port's ready line rests on. A vertex is
    // accepted only when the result register can receive its answer, so the
    // register is provably free by the time the last field lane lands and the
    // field stream never needs to be stalled by a full output. The first
    // version of this block carried a stall term for that case; it was
    // UNREACHABLE, and this case is what found it. Asserted directly: while a
    // chain is in flight, `st_valid_o` must stay 0 and `vtx_ready_o` must stay
    // 0, and a producer that goes idle mid-chain must not corrupt the partial
    // sum.
    zt::FieldList three;
    zt::FieldRecord r;
    r.x0 = -(1 << 30);
    r.z0 = -(1 << 30);
    r.x1 = (1 << 30);
    r.z1 = (1 << 30);
    for (int i = 0; i < 3; ++i) three.offer(r, 9);
    load_list(dut, three, 9);
    dut.st_ready_i = 0;  // the consumer is stalled for the WHOLE chain
    dut.base_i = 0;
    dut.scar_i = 0;
    dut.bottom_i = -32768;
    dut.dual_i = 1;
    dut.wx_i = 0;
    dut.wz_i = 0;
    dut.vi_i = 3;
    dut.vj_i = 3;
    dut.src_id_i = 0x55;
    dut.vtx_valid_i = 1;
    zhao::tick(dut);
    dut.vtx_valid_i = 0;
    const int32_t lanes[3] = {3 << 16, 5 << 16, -(1 << 16)};
    for (int i = 0; i < 3; ++i) {
      dut.eval();
      check(dut.st_valid_o == 0, "the result register stays free for the whole chain", 0,
            dut.st_valid_o);
      check(dut.vtx_ready_o == 0, "no second vertex is admitted while a chain is in flight", 0,
            dut.vtx_ready_o);
      check(dut.fld_ready_o == 1, "the field port is ready for every lane of a live chain", 1,
            dut.fld_ready_o);
      // The producer goes idle for a few cycles before each lane: the partial
      // sum must survive untouched.
      dut.fld_valid_i = 0;
      for (int g = 0; g < 3; ++g) zhao::tick(dut);
      dut.fld_valid_i = 1;
      dut.fld_height_i = lanes[i];
      zhao::tick(dut);
    }
    dut.fld_valid_i = 0;
    dut.eval();
    check(dut.st_valid_o == 1,
          "the chain publishes into the free register even with the consumer "
          "stalled",
          1, dut.st_valid_o);
    check(static_cast<int32_t>(dut.top_o) == (7 << 16),
          "an idle producer between lanes does not corrupt the partial sum",
          static_cast<uint32_t>(7 << 16), static_cast<uint32_t>(dut.top_o));
    dut.st_ready_i = 1;
    zhao::tick(dut);
    check(dut.st_valid_o == 0, "the chained result retires exactly once", 0, dut.st_valid_o);
  }

  // =========================================================================
  // 7. the counter counts composed vertices, not cycles and not offers
  // =========================================================================
  {
    reset_dut(dut);
    dut.st_ready_i = 1;
    zt::FieldList one;
    zt::FieldRecord r;
    r.x0 = -(1 << 30);
    r.z0 = -(1 << 30);
    r.x1 = (1 << 30);
    r.z1 = (1 << 30);
    one.offer(r, 3);
    load_list(dut, one, 3);
    const uint32_t before = dut.terrain_samples_evaluated_o;
    check(before == 0, "the counter starts at zero after reset", 0, before);
    zt::ComposeIn in;
    in.base = 500;
    in.scar = 0;
    in.bottom = -32768;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;
    const int32_t fh[1] = {kOne};
    for (int i = 0; i < 5; ++i) compose_one(dut, in, 1, fh, 4, 4, static_cast<uint16_t>(i));
    check(dut.terrain_samples_evaluated_o == 5,
          "terrain_samples_evaluated counts composed lattice vertices", 5,
          dut.terrain_samples_evaluated_o);
    dut.fld_valid_i = 0;
    dut.vtx_valid_i = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(dut);
    check(dut.idle_o == 1, "the block reports idle when drained", 1, dut.idle_o);
  }

  // =========================================================================
  // 8. throughput: the ledger's "1 patch-layer update per clock"
  // =========================================================================
  {
    reset_dut(dut);
    dut.st_ready_i = 1;
    zt::FieldList empty;
    load_list(dut, empty, 11);
    // With no live field on the patch the compose lane must sustain one vertex
    // per clock. This is the ledger's stated rate, and it is MEASURED here
    // rather than asserted in prose.
    dut.base_i = 700;
    dut.scar_i = 3;
    dut.bottom_i = -32768;
    dut.dual_i = 1;
    dut.wx_i = 0;
    dut.wz_i = 0;
    dut.vi_i = 4;
    dut.vj_i = 4;
    dut.src_id_i = 1;
    dut.vtx_valid_i = 1;
    int accepted = 0;
    int published = 0;
    for (int cycle = 0; cycle < 64; ++cycle) {
      dut.eval();
      if (dut.vtx_ready_o) ++accepted;
      if (dut.st_valid_o && dut.st_ready_i) ++published;
      zhao::tick(dut);
    }
    dut.vtx_valid_i = 0;
    check(accepted >= 63, "the compose lane accepts a vertex EVERY clock with no live fields", 64,
          static_cast<uint32_t>(accepted));
    check(published >= 62, "and publishes one result per clock", 64,
          static_cast<uint32_t>(published));
    std::printf("terrain_patch_directed: throughput %d accepts / %d publishes in 64 clocks\n",
                accepted, published);
  }

  return zhao::report_and_exit("terrain_patch_directed");
}
