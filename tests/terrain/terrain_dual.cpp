// terrain_dual.cpp — dual-heightfield island format tests (world-identity
// wave; spec/terrain_rules.md test plan §10 items 1, 2 and the §9.2 deferral
// identity, at Phase-3 reference scope).
//
// What each lane would catch (the "could have been red" statement):
//   1. column_query directed — corner identities, the fixed i00–i11 diagonal
//      (single-valued seam), triangle pick (ties to A), void/OUT classes.
//      Red on: any mis-indexed corner, wrong diagonal, wrong tie rule.
//   2. physics_equals_pixels — 20k pseudo-random columns over a composed
//      33x33 dual patch (live field + clamp active): column_query must equal
//      an independently written exact-rational plane oracle, with the cell
//      located by the SPEC's pitch-shift form (not the renderer's lerp).
//      Red on: any interpolation/rounding/locate divergence between the
//      §4.3 query and the plane of the tessellated triangle.
//   3. breach/heal/no_bake — the §3.4 law end to end with hand-computed
//      expected transitions; corner-coupling asserted (a neighbour sags but
//      stays SOLID); the vertex-level no_bake clamp protects the whole
//      corner neighbourhood; heal telescopes scar back to EXACT zero.
//   4. bake deferral identity — a stepped dig ramp produces bit-identical
//      B and D layers to the one-shot bake (terrain_rules §9.2 law 3).
//   5. renderer semantics — a breached cell renders sky through the island
//      (top-down), rim walls render true local thickness (front elevation);
//      the legacy single-surface patch draws neither. Red on: the migration
//      changing pixels anywhere cell state does not say so.

#include "render_helpers.hpp"  // tests/render packet/canvas helpers
#include "zfield/zfield.hpp"
#include "zref/zref_terrain.hpp"
#include "zrender/internal.hpp"  // white-box: compose_lattice, FieldApp

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

using zref::fx16;
using zref::SatLedger;
namespace zt = zref::terrain;
namespace zr = zref::render;

// ---- 1. column_query directed ----------------------------------------------

void test_column_query_directed() {
  zt::ComposedLattice lat;
  lat.w = 3;
  lat.h = 3;
  lat.dual = true;
  lat.wx = {0, 2 << 16, 4 << 16};
  lat.wz = {0, 2 << 16, 4 << 16};
  // top heights (fx16 raw), row-major z-then-x — deliberately asymmetric
  lat.top = {10 << 16, 20 << 16, 30 << 16,   //
             12 << 16, 26 << 16, 31 << 16,   //
             14 << 16, 27 << 16, 40 << 16};  //
  lat.bottom = {0, 1 << 16, 2 << 16,         //
                0, 1 << 16, 2 << 16,         //
                0, 1 << 16, 2 << 16};        //
  lat.cell_state = {zt::kSolid, zt::kSolid, zt::kSolid, zt::kVoidBreached};

  // corner identity: query exactly at vertex (1,1) — lands in cell (1,1)
  // (floor rule) which is VOID_BREACHED
  zt::ColumnResult r = zt::column_query(lat, fx16{2 << 16}, fx16{2 << 16});
  check(r.cls == zt::ColumnClass::kVoid, "vertex (1,1) floor-lands in the breached cell");
  // corner identity at (0,0): u = v = 0, triangle A, h = h00 exactly
  r = zt::column_query(lat, fx16{0}, fx16{0});
  check(r.cls == zt::ColumnClass::kSolid && r.top.raw == (10 << 16) && r.bottom.raw == 0,
        "corner identity at i00");
  // corner (2,2) is the far corner of the last cell (x == max lands inside)
  r = zt::column_query(lat, fx16{4 << 16}, fx16{4 << 16});
  check(r.cls == zt::ColumnClass::kVoid, "far corner lands in the (breached) last cell");
  // corner (2,0): cell (1,0), u = 1, v = 0 -> triangle A, h = h10 of that cell
  r = zt::column_query(lat, fx16{4 << 16}, fx16{0});
  check(r.cls == zt::ColumnClass::kSolid && r.top.raw == (30 << 16), "corner identity at i10");

  // interior of cell (0,0): u = 0.75, v = 0.25 -> triangle A:
  // h = h00 + u*(h10-h00) + v*(h11-h10) = 10 + 0.75*10 + 0.25*6 = 19
  r = zt::column_query(lat, fx16{(3 << 16) / 2}, fx16{(1 << 16) / 2});
  check(r.cls == zt::ColumnClass::kSolid && r.top.raw == (19 << 16), "triangle A two-MAD value");
  // u = 0.25, v = 0.75 -> triangle B: h = h00 + u*(h11-h01) + v*(h01-h00)
  //   = 10 + 0.25*14 + 0.75*2 = 15
  r = zt::column_query(lat, fx16{(1 << 16) / 2}, fx16{(3 << 16) / 2});
  check(r.cls == zt::ColumnClass::kSolid && r.top.raw == (15 << 16), "triangle B two-MAD value");
  // the diagonal u == v == 0.5 must be single-valued: both formulas give
  // h00 + 0.5*(h11-h00) = 18 (tie goes to A by law; the VALUE is the seam law)
  r = zt::column_query(lat, fx16{1 << 16}, fx16{1 << 16});
  check(r.cls == zt::ColumnClass::kSolid && r.top.raw == (18 << 16),
        "diagonal seam single-valued (ties to A)");
  // bottom interpolates on the SAME triangulation: at u=v=0.5 in cell (0,0):
  // 0 + 0.5*(1-0) = 0.5 m
  check(r.bottom.raw == (1 << 15), "bottom lane rides the same triangulation");

  // outside the envelope: OUT on all four sides
  check(zt::column_query(lat, fx16{-1}, fx16{0}).cls == zt::ColumnClass::kOut, "OUT left");
  check(zt::column_query(lat, fx16{(4 << 16) + 1}, fx16{0}).cls == zt::ColumnClass::kOut,
        "OUT right");
  check(zt::column_query(lat, fx16{0}, fx16{-1}).cls == zt::ColumnClass::kOut, "OUT near");
  check(zt::column_query(lat, fx16{0}, fx16{(4 << 16) + 1}).cls == zt::ColumnClass::kOut,
        "OUT far");
}

// ---- 2. physics equals pixels ----------------------------------------------

// exact-rational plane oracle: the plane through the three §4.3 triangle
// corners, derived via the cross-product normal (an independent derivation
// of the interpolant), with its own round-half-up division.
int64_t oracle_div_rhu(__int128 n, __int128 d) {
  const __int128 num = 2 * n + d;
  const __int128 den = 2 * d;
  __int128 q = num / den;
  if (num % den != 0 && num < 0) --q;
  return static_cast<int64_t>(q);
}

void test_physics_equals_pixels() {
  // one Island-Patch-sized page: 33x33 lattice, 32x32 cells, envelope
  // ±32 m -> the canonical 2.0 m pitch (terrain_rules §1.3)
  const int W = 33;
  zr::TerrainPatch patch;
  patch.width = patch.height = W;
  patch.env_x0 = patch.env_z0 = -(32 << 16);
  patch.env_x1 = patch.env_z1 = (32 << 16);
  patch.heights.resize(static_cast<size_t>(W) * W);
  patch.bottom.resize(static_cast<size_t>(W) * W);
  patch.cell_state.assign(32 * 32, zt::kSolid);
  for (int j = 0; j < W; ++j) {
    for (int i = 0; i < W; ++i) {
      const size_t k = static_cast<size_t>(j) * W + i;
      // deterministic integer relief ~10..13 m, bottom ~ -8 m with ripple
      patch.heights[k] = static_cast<int16_t>(2560 + ((i * 7) % 11) * 32 + ((j * 5) % 13) * 16);
      patch.bottom[k] = static_cast<int16_t>(-2048 + ((i + j) % 5) * 64);
    }
  }
  // a thin authored lip: bottom above base at two vertices — compose_top
  // must clamp UP to bottom there (§3.4 first clamp)
  patch.bottom[5 * W + 5] = static_cast<int16_t>(patch.heights[5 * W + 5] + 100);
  patch.bottom[5 * W + 6] = static_cast<int16_t>(patch.heights[5 * W + 6] + 80);
  // scattered authored void cells (coverage of the kVoid class)
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci * 3 + cj * 7) % 23 == 0) patch.cell_state[cj * 32 + ci] = zt::kVoidAuthored;

  // one live field: height <- p0 (the tests' hand-built earth program),
  // p0 = -20 m over the north-east quadrant -> the live clamp at bottom is
  // ACTIVE there (base + delta < bottom)
  const std::vector<uint8_t> prog_bytes = rtest::make_earth_prog();
  const zfield::DecodeResult dec = zfield::decode(prog_bytes.data(), prog_bytes.size());
  check(dec.error == zfield::DecodeError::kOk, "earth program decodes");
  zr::FieldApp app;
  app.prog = &dec.prog;
  std::memset(&app.cmd, 0, sizeof(app.cmd));
  app.cmd.program = 1;
  app.cmd.footprint.x0 = 0;
  app.cmd.footprint.y0 = 0;
  app.cmd.footprint.x1 = 32 << 16;
  app.cmd.footprint.y1 = 32 << 16;
  app.cmd.start_tick = 0;
  app.cmd.duration_ticks = 100;
  const int32_t p0 = -(20 << 16);
  for (int b = 0; b < 4; ++b) app.cmd.parameters[b] = static_cast<uint8_t>(p0 >> (8 * b));

  SatLedger L;
  const zt::ComposedLattice lat =
      zr::compose_lattice(patch, rtest::xform_identity(), {app}, 50, nullptr, &L);
  check(lat.dual && lat.w == W && lat.h == W, "composed lattice is dual 33x33");

  // clamp evidence: inside the footprint the -20 m delta drives base under
  // bottom -> live_top == bottom exactly; outside it live_top == base(+lip)
  int clamped = 0;
  for (size_t k = 0; k < lat.top.size(); ++k) {
    check(lat.top[k] >= lat.bottom[k], "live_top never below bottom");
    if (lat.top[k] == lat.bottom[k]) ++clamped;
  }
  check(clamped > 200, "the live clamp actually engaged over the footprint");
  // the authored lip vertex composes AT bottom (clamped up)
  check(lat.top[5 * W + 5] >= lat.bottom[5 * W + 5], "thin-lip vertex clamped up to bottom");

  // 20k pseudo-random columns (plus the envelope frame): query vs oracle
  uint32_t rng = 0x2A6E1D4Bu;
  const auto next = [&rng]() {
    rng = rng * 1103515245u + 12345u;
    return rng;
  };
  int n_a = 0, n_b = 0, n_void = 0, n_out = 0, n_mismatch = 0;
  for (int t = 0; t < 20000; ++t) {
    // span ±34 m so ~6% of points fall OUTSIDE the envelope (OUT coverage)
    const int32_t qx = -(34 << 16) + static_cast<int32_t>(next() % (68u << 16));
    const int32_t qz = -(34 << 16) + static_cast<int32_t>(next() % (68u << 16));
    const zt::ColumnResult r = zt::column_query(lat, fx16{qx}, fx16{qz});

    // oracle locate: the SPEC's pitch-shift form (exact at 2 m pitch)
    if (qx < patch.env_x0 || qx > patch.env_x1 || qz < patch.env_z0 || qz > patch.env_z1) {
      ++n_out;
      if (r.cls != zt::ColumnClass::kOut) ++n_mismatch;
      continue;
    }
    int ci = static_cast<int>((static_cast<int64_t>(qx) - patch.env_x0) >> 17);
    int cj = static_cast<int>((static_cast<int64_t>(qz) - patch.env_z0) >> 17);
    if (ci > 31) ci = 31;
    if (cj > 31) cj = 31;
    if ((patch.cell_state[cj * 32 + ci] & zt::kSubstanceMask) != zt::kSolid) {
      ++n_void;
      if (r.cls != zt::ColumnClass::kVoid) ++n_mismatch;
      continue;
    }
    const int64_t un = static_cast<int64_t>(qx) - (patch.env_x0 + (static_cast<int64_t>(ci) << 17));
    const int64_t vn = static_cast<int64_t>(qz) - (patch.env_z0 + (static_cast<int64_t>(cj) << 17));
    const int64_t ud = 1 << 17, vd = 1 << 17;
    const size_t i00 = static_cast<size_t>(cj) * W + ci;
    const bool tri_a = un * vd >= vn * ud;
    tri_a ? ++n_a : ++n_b;
    // plane through the triangle corners via the cross-product normal:
    //   A = (i00, i10, i11): h = h00 + [(h10-h00)*un*vd + (h11-h10)*vn*ud]/(ud*vd)
    //   B = (i00, i01, i11): h = h00 + [(h11-h01)*un*vd + (h01-h00)*vn*ud]/(ud*vd)
    // assembled here independently from the plane equation (see the normal
    // derivation in the test header) with our own rounding
    const auto plane = [&](const std::vector<int32_t>& hgt) {
      const int64_t h00 = hgt[i00], h10 = hgt[i00 + 1], h01 = hgt[i00 + W], h11 = hgt[i00 + W + 1];
      const __int128 num = tri_a ? static_cast<__int128>(h10 - h00) * un * vd +
                                       static_cast<__int128>(h11 - h10) * vn * ud
                                 : static_cast<__int128>(h11 - h01) * un * vd +
                                       static_cast<__int128>(h01 - h00) * vn * ud;
      return static_cast<int32_t>(h00 + oracle_div_rhu(num, static_cast<__int128>(ud) * vd));
    };
    if (r.cls != zt::ColumnClass::kSolid || r.top.raw != plane(lat.top) ||
        r.bottom.raw != plane(lat.bottom)) {
      if (n_mismatch < 4)
        std::fprintf(stderr, "  mismatch at (%d,%d): cls=%d top=%d want=%d\n", qx, qz,
                     static_cast<int>(r.cls), r.top.raw, plane(lat.top));
      ++n_mismatch;
    }
  }
  std::printf("  physics==pixels: A=%d B=%d void=%d out=%d mismatch=%d\n", n_a, n_b, n_void, n_out,
              n_mismatch);
  check(n_mismatch == 0, "column_query == plane oracle on every sampled column");
  check(n_a > 1000 && n_b > 1000 && n_void > 100 && n_out > 500,
        "the sample actually covered both triangles, void and OUT");
}

// ---- 3. breach / heal / no_bake --------------------------------------------

zr::TerrainPatch flat_slab_5x5() {
  zr::TerrainPatch p;
  p.width = p.height = 5;
  p.env_x0 = p.env_z0 = -(4 << 16);
  p.env_x1 = p.env_z1 = (4 << 16);
  p.heights.assign(25, 2560);  // 10 m top
  p.bottom.assign(25, 0);      // 0 m underside
  p.scar.assign(25, 0);
  p.cell_state.assign(16, zt::kSolid);
  return p;
}

void test_breach_heal_nobake() {
  const zt::DigStamp dig{0, 0, 5 << 16};  // centre column, 5 m radius
  SatLedger L;

  // shallow dig: 4 m into a 10 m slab -> nothing can breach
  zr::TerrainPatch p = flat_slab_5x5();
  zt::bake_dig(p, dig, fx16{0}, fx16{4 << 16}, &L);
  std::vector<zt::BreachEvent> ev = zt::apply_breach_law(p);
  check(ev.empty(), "4 m dig into a 10 m slab breaches nothing");
  check(p.scar[2 * 5 + 2] == -1024, "centre vertex scar = exactly -4 m (stencil s=1)");

  // deepen to 16 m: the inner 3x3 vertex block meets bottom -> exactly the
  // middle 2x2 cells breach, in z-then-x scan order (hand-computed: corner
  // vertices at d=2 m carry scar -3441, at d=2.83 m carry -2785, both under
  // the 2560 base; the d=4 m ring stays at 1085 above bottom)
  zt::bake_dig(p, dig, fx16{4 << 16}, fx16{16 << 16}, &L);
  ev = zt::apply_breach_law(p);
  check(ev.size() == 4, "16 m dig breaches exactly the middle 2x2 cells");
  if (ev.size() == 4) {
    const uint16_t want[4][2] = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
    bool order_ok = true;
    for (int k = 0; k < 4; ++k)
      if (ev[k].ci != want[k][0] || ev[k].cj != want[k][1] || ev[k].state != zt::kVoidBreached)
        order_ok = false;
    check(order_ok, "breach events in deterministic z-then-x scan order");
  }
  // corner-coupling (S5 caveat 1, recorded in terrain_rules): the SOLID
  // border cell (0,0) sags — its shared corner vertex (1,1) carries a
  // negative scar — but does NOT breach (its outer corners stay up)
  check((p.cell_state[0] & zt::kSubstanceMask) == zt::kSolid, "border cell stays SOLID");
  check(p.scar[1 * 5 + 1] < 0, "border cell sags toward the breach (shared corner)");

  // heal: bake the dig back out — scar telescopes to EXACT zero (the g(depth)
  // absolute-contribution law) and the four cells heal in scan order
  zt::bake_dig(p, dig, fx16{16 << 16}, fx16{0}, &L);
  ev = zt::apply_breach_law(p);
  check(ev.size() == 4, "raising the ground heals all four breached cells");
  bool heal_ok = ev.size() == 4;
  for (const zt::BreachEvent& e : ev)
    if (e.state != zt::kSolid) heal_ok = false;
  check(heal_ok, "heal events return cells to SOLID");
  bool scar_zero = true;
  for (int16_t s : p.scar)
    if (s != 0) scar_zero = false;
  check(scar_zero, "scar telescopes back to exact zero after un-bake");

  // no_bake: protect the middle cell (1,1); the SAME 16 m dig then cannot
  // breach ANY cell — the vertex-level clamp holds every corner vertex of
  // the protected cell at bottom + 1 LSB, and those vertices are shared
  // corners of all four middle cells (corner coupling protects neighbours)
  zr::TerrainPatch q = flat_slab_5x5();
  q.cell_state[1 * 4 + 1] |= zt::kNoBakeBit;
  zt::bake_dig(q, dig, fx16{0}, fx16{16 << 16}, &L);
  ev = zt::apply_breach_law(q);
  check(ev.empty(), "no_bake plinth: nothing breaches (vertex clamp shields the corners)");
  for (int j = 1; j <= 2; ++j)
    for (int i = 1; i <= 2; ++i)
      check(static_cast<int32_t>(q.heights[j * 5 + i]) + q.scar[j * 5 + i] >=
                static_cast<int32_t>(q.bottom[j * 5 + i]) + 1,
            "protected corner vertex holds >= bottom + 1 LSB");

  // determinism: replay the whole sequence — identical layers and events
  zr::TerrainPatch r1 = flat_slab_5x5(), r2 = flat_slab_5x5();
  SatLedger L1, L2;
  zt::bake_dig(r1, dig, fx16{0}, fx16{4 << 16}, &L1);
  zt::bake_dig(r1, dig, fx16{4 << 16}, fx16{16 << 16}, &L1);
  const std::vector<zt::BreachEvent> e1 = zt::apply_breach_law(r1);
  zt::bake_dig(r2, dig, fx16{0}, fx16{4 << 16}, &L2);
  zt::bake_dig(r2, dig, fx16{4 << 16}, fx16{16 << 16}, &L2);
  const std::vector<zt::BreachEvent> e2 = zt::apply_breach_law(r2);
  check(r1.scar == r2.scar && r1.cell_state == r2.cell_state && e1.size() == e2.size(),
        "bake + breach replay is state-exact");
}

// ---- 4. bake deferral identity (terrain_rules §9.2 law 3) ------------------

void test_bake_deferral_identity() {
  const zt::DigStamp dig{1 << 16, -(1 << 16), 5 << 16};  // off-centre
  SatLedger L;
  zr::TerrainPatch one = flat_slab_5x5();
  zt::bake_dig(one, dig, fx16{0}, fx16{16 << 16}, &L);
  zt::apply_breach_law(one);

  zr::TerrainPatch stepped = flat_slab_5x5();
  // an uneven 5-step ramp (the deferred-drain shape: catch-up steps differ)
  const int32_t steps[6] = {0, 2 << 16, 3 << 16, 9 << 16, 11 << 16, 16 << 16};
  for (int s = 0; s + 1 < 6; ++s) {
    zt::bake_dig(stepped, dig, fx16{steps[s]}, fx16{steps[s + 1]}, &L);
    zt::apply_breach_law(stepped);  // breach timing may differ; final state may not
  }
  check(one.scar == stepped.scar, "stepped ramp scar == one-shot scar (bit-exact)");
  check(one.cell_state == stepped.cell_state, "stepped ramp cell state == one-shot");
}

// ---- 5. renderer semantics: sky through a breach, walls with thickness -----

// 9x9 slab island, 8x8 cells over ±8 m (2 m pitch), 4 m top, 0 m bottom;
// the middle 2x2 cells breached when `holed`
zr::TerrainPatch slab_island(bool dual, bool holed) {
  zr::TerrainPatch p;
  p.width = p.height = 9;
  p.env_x0 = p.env_z0 = -(8 << 16);
  p.env_x1 = p.env_z1 = (8 << 16);
  p.heights.assign(81, 1024);  // 4 m
  if (dual) {
    p.bottom.assign(81, 0);
    p.cell_state.assign(64, zt::kSolid);
    if (holed)
      for (int cj = 3; cj <= 4; ++cj)
        for (int ci = 3; ci <= 4; ++ci) p.cell_state[cj * 8 + ci] = zt::kVoidBreached;
  }
  return p;
}

zref::render::RenderResult render_with(const zr::TerrainPatch& patch,
                                       const zhao_abi::ZhMat4fx& view,
                                       zref::render::RenderCanvas& canvas) {
  zr::Material mat{200, 180, 160};
  zr::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});
  zr::SoftwareRenderer rend;
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = view;
    std::vector<uint8_t> v1;
    zhao_abi::zhao_pack_set_view(sv, v1);
    b.append_record(v1);
    auto dp = zhao_abi::zhao_sample_draw_procedural();
    dp.payload.program = 44;
    dp.payload.material = 45;
    dp.payload.transform = rtest::xform_identity();
    dp.payload.screen_error = 1 << 16;
    dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_draw_procedural(dp, v2);
    b.append_record(v2);
  };
  return rend.render_frame(rtest::seal_frame(1, body), 0, canvas, res);
}

constexpr uint8_t kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
uint16_t bg_px(uint32_t x, uint32_t y) {  // the dithered black background
  const uint8_t B = kBayer4[y & 3][x & 3];
  const uint32_t r5 = (B * 16 + 8) / 255, g6 = (B * 32 + 16) / 255, b5 = (B * 16 + 8) / 255;
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

void test_render_breach_and_walls() {
  // top-down ortho (rtest::ortho_topdown 1/32): env ±8 m -> px [144,240),
  // py [90,150); the breached middle cells (world [-2,2]²) -> px [180,204),
  // py [112.5,127.5) — (192,120) is inside the hole, (150,95) is solid rim
  zref::render::RenderCanvas c1, c2, c3;
  check(render_with(slab_island(true, true), rtest::ortho_topdown(2048), c1).status == 0,
        "dual holed patch renders");
  check(render_with(slab_island(false, false), rtest::ortho_topdown(2048), c2).status == 0,
        "legacy patch renders");
  check(rtest::px(c1, 0, 192, 120, 384) == bg_px(192, 120),
        "SKY THROUGH THE BREACH: hole pixel is background");
  check(rtest::px(c1, 0, 150, 95, 384) != bg_px(150, 95), "solid ground still draws");
  check(rtest::px(c2, 0, 192, 120, 384) != bg_px(192, 120),
        "legacy page draws the same pixel (migration changed nothing it shouldn't)");

  // front elevation (x' = x/32, y' = -y/32, w = 1): the 4 m slab spans
  // rows [105,120] at the island's px columns. A wall pixel at mid-thickness
  // (192,112) must be geometry for the dual page and background for legacy
  // (a flat top edge-on is a zero-area line; there are no walls to draw).
  const int32_t elev[16] = {2048, 0, 0, 0, 0, -2048, 0, 0, 0, 0, 1 << 16, 0, 0, 0, 0, 1 << 16};
  check(render_with(slab_island(true, false), rtest::mat(elev), c3).status == 0,
        "elevation view renders");
  check(rtest::px(c3, 0, 192, 112, 384) != bg_px(192, 112),
        "RIM WALL WITH TRUE THICKNESS: mid-thickness pixel is wall geometry");
  zref::render::RenderCanvas c4;
  check(render_with(slab_island(false, false), rtest::mat(elev), c4).status == 0,
        "legacy elevation renders");
  check(rtest::px(c4, 0, 192, 112, 384) == bg_px(192, 112),
        "legacy page has no wall there (single surface, honest control)");
}

}  // namespace

int main() {
  test_column_query_directed();
  test_physics_equals_pixels();
  test_breach_heal_nobake();
  test_bake_deferral_identity();
  test_render_breach_and_walls();
  if (failures == 0) std::printf("terrain_dual: all green\n");
  return failures == 0 ? 0 : 1;
}
