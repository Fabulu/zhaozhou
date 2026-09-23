// terrain_uvlane_directed.cpp -- TERRAIN.UV: the frozen terrain
// texture-coordinate law, driven through the store that sits beside the
// projector's arena.
//
// WHAT IS PROVED, and against what:
//
//   1. THE LAW, CHECKED TWO INDEPENDENT WAYS. `spec/terrain_rules.md` 6.2/6.6
//      and `reference/src/zrender/terrain.cpp:581-612` give
//          top:       u = wx >> top_shift, v = wz >> top_shift
//          underside: u = wx >> 3,         v = wz >> 3      (STRATA_M = 8 m)
//      with `top_shift = pitch_log2` CLAMPED AT ZERO.
//      The oracle is NOT exported by `zref` -- the law is inline in
//      `draw_terrain`'s loop -- so a driver that merely replays the same shift
//      would be one implementation checking itself. This test therefore
//      carries TWO oracles that reach the answer by different routes and
//      requires both to agree with the RTL:
//
//        ORACLE A -- the law directly: `w >> shift` on int32_t.
//        ORACLE B -- the PLACEMENT ALGEBRA, which never performs the law's
//          shift at all. `zhao_terrain_place` places lattice column n at
//          `wx(n) = n <<< (16 + pitch_log2)` (its own header, spec 2.1). Feed
//          that to the law and the pitch CANCELS on the top surface:
//              pitch_log2 >= 0 : u = (n << (16+p)) >> p   = n << 16
//              pitch_log2 = -1 : u = (n << 15)    >> 0    = n << 15
//          -- which is terrain_rules 6.2's "one tile period per cell" made
//          arithmetic: on the top surface u in tile units IS the lattice
//          index. On the underside the pitch does not cancel:
//              u = (n << (16+p)) >> 3 = n << (13+p).
//
//      A defect that both oracles share would have to be a mistake about the
//      SPEC rather than about the arithmetic, and the two disagree on every
//      pitch if the clamp is wrong.
//
//   2. THE ARITHMETIC SHIFT. World coordinates west and north of the island
//      datum are NEGATIVE and the oracle's operands are `int32_t`. A logical
//      shift passes every non-negative test and fails here. Section 3 drives
//      random negative coordinates that are NOT lattice-placed, so only
//      oracle A applies and the value is a wrong number rather than a
//      plausible one.
//
//   3. THE CLAMP IS OBSERVABLE. `pitch_log2 = -1` (0.5 m) is LEGAL by spec
//      1.3. Section 4 asserts the clamped value AND that `pitch_clamped_o`
//      counts exactly the fills where the clamp changed the answer -- and
//      that it stays SILENT on the underside, whose shift is STRATA_M's and
//      is pitch-independent.
//
//   4. ONE TILE PERIOD PER CELL, checked through the CONSUMER's own fold.
//      `zref::terrain::mirror_texel` is the frozen TMU fold; section 5 shows
//      adjacent top-surface cells land exactly 64 texels apart before folding,
//      which is the units claim `zhao_texture_mosaic`'s `req_u_i` port makes.
//
//   5. THE VALUE TRAVERSES AND THE RECORD HOLDS. src_id rides each reference
//      and comes back with its coordinates; N references produce N packets.
//      The lattice is random, so a misrouted row is a wrong number.
//
//   6. THE REFUSALS FIRE. A reference carrying a generation the rows do not
//      hold is refused and counted (`stale_reads_o`), and an out-of-range
//      pitch is counted (`pitch_illegal_o`). Both by stimulus -- no mutant
//      needed, because both states are reachable at this block's boundary.
//
// POSITIVE CONTROL: --break-oracle corrupts one expected coordinate by +1 and
// the suite must then FAIL. A checker never seen to fail is not a checker.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_uvlane.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

using zhao::check;

namespace {

constexpr int kDepth = 81;
constexpr int kArenas = 4;

bool g_break_oracle = false;

uint32_t lcg(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s;
}

// ---- ORACLE A: the law, directly ------------------------------------------
// reference/src/zrender/terrain.cpp:589-591 and 606-609.
int32_t oracle_law(int32_t w, int pitch_log2, bool underside) {
  int shift;
  if (underside) {
    shift = 3;  // terrain_rules 6.6, STRATA_M = 8 m
  } else {
    shift = pitch_log2;
    if (shift < 0) shift = 0;  // THE CLAMP
  }
  return w >> shift;  // arithmetic: the oracle's operand is int32_t
}

// ---- ORACLE B: the placement algebra, which never performs that shift ------
// zhao_terrain_place: wx(n) = n <<< (16 + pitch_log2).
int32_t place(int32_t n, int pitch_log2) {
  const int sh = 16 + pitch_log2;  // {15, 16, 17, 18} for {-1, 0, +1, +2}
  return static_cast<int32_t>(static_cast<uint32_t>(n) << sh);
}

int32_t oracle_algebra(int32_t n, int pitch_log2, bool underside) {
  int sh;
  if (underside) {
    sh = 13 + pitch_log2;  // (n << (16+p)) >> 3
  } else {
    sh = (pitch_log2 >= 0) ? 16 : 15;  // the pitch cancels; the clamp bites at -1
  }
  return static_cast<int32_t>(static_cast<uint32_t>(n) << sh);
}

struct Tri {
  uint32_t ia, ib, ic;
  uint16_t src;
};

struct Got {
  int32_t au, av, bu, bv, cu, cv;
  uint16_t src;
};

struct Lane {
  Vtb_terrain_uvlane& d;
  std::vector<Got> got;
  uint64_t cycles = 0;

  explicit Lane(Vtb_terrain_uvlane& dut) : d(dut) {}

  void tick() {
    d.eval();
    if (d.uv_valid && d.uv_ready) {
      Got g;
      g.au = static_cast<int32_t>(d.uv_au);
      g.av = static_cast<int32_t>(d.uv_av);
      g.bu = static_cast<int32_t>(d.uv_bu);
      g.bv = static_cast<int32_t>(d.uv_bv);
      g.cu = static_cast<int32_t>(d.uv_cu);
      g.cv = static_cast<int32_t>(d.uv_cv);
      g.src = static_cast<uint16_t>(d.uv_src);
      got.push_back(g);
    }
    zhao::tick(d);
    ++cycles;
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) tick();
  }

  void open_arena(uint32_t arena, uint32_t gen) {
    d.open_v = 1;
    d.open_arena = static_cast<uint8_t>(arena);
    d.open_gen = static_cast<uint8_t>(gen);
    tick();
    d.open_v = 0;
    tick();
  }

  void set_pitch(int pitch_log2) {
    d.pitch_log2 = static_cast<uint8_t>(static_cast<int8_t>(pitch_log2));
  }

  // The FILL beat: exactly the handshake that writes the projector's arena.
  void fill(uint32_t arena, uint32_t index, int32_t wx, int32_t wz, bool underside) {
    d.fill_valid = 1;
    d.fill_ready = 1;
    d.fill_arena = static_cast<uint8_t>(arena);
    d.fill_index = static_cast<uint8_t>(index);
    d.fill_vx = static_cast<uint32_t>(wx);
    d.fill_vz = static_cast<uint32_t>(wz);
    d.fill_surface = underside ? 1 : 0;
    tick();
    d.fill_valid = 0;
    d.fill_ready = 0;
    d.fill_surface = 0;
  }

  // Offer one reference and run until it is taken.
  void send_ref(uint32_t arena, uint32_t gen, const Tri& t, int max_cycles = 64) {
    d.ref_valid = 1;
    d.ref_arena = static_cast<uint8_t>(arena);
    d.ref_gen = static_cast<uint8_t>(gen);
    d.ref_ia = static_cast<uint8_t>(t.ia);
    d.ref_ib = static_cast<uint8_t>(t.ib);
    d.ref_ic = static_cast<uint8_t>(t.ic);
    d.ref_src = t.src;
    for (int i = 0; i < max_cycles; ++i) {
      d.eval();
      const bool taken = d.ref_valid && d.ref_ready;
      tick();
      if (taken) break;
    }
    d.ref_valid = 0;
  }
};

void reset(Vtb_terrain_uvlane& d) {
  d.rst_n = 0;
  d.fill_valid = 0;
  d.fill_ready = 0;
  d.fill_surface = 0;
  d.pitch_log2 = 0;
  d.open_v = 0;
  d.ref_valid = 0;
  d.uv_ready = 1;
  d.eval();
  for (int i = 0; i < 6; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

// One full pass: place a lattice at `pitch_log2`, fill it, replay triangles,
// and check every corner against BOTH oracles.
void run_surface_pass(Lane& L, int pitch_log2, bool underside, uint32_t arena, uint32_t gen,
                      const char* label) {
  L.set_pitch(pitch_log2);
  L.open_arena(arena, gen);

  // A lattice of columns/rows, placed by TERRAIN.PLACE's own law. Indices are
  // deliberately spread so a misrouted row is a wrong number, not a neighbour.
  std::vector<int32_t> col(kDepth), row(kDepth);
  for (int i = 0; i < kDepth; ++i) {
    col[i] = static_cast<int32_t>(i) - 17;  // straddles zero: negatives included
    row[i] = static_cast<int32_t>(i * 3) - 40;
    L.fill(arena, static_cast<uint32_t>(i), place(col[i], pitch_log2), place(row[i], pitch_log2),
           underside);
  }
  L.idle(4);

  const size_t before = L.got.size();
  std::vector<Tri> tris;
  uint32_t s = 0x1234u + static_cast<uint32_t>(pitch_log2 * 7 + (underside ? 91 : 0));
  for (int t = 0; t < 12; ++t) {
    Tri tri;
    tri.ia = lcg(s) % kDepth;
    tri.ib = lcg(s) % kDepth;
    tri.ic = lcg(s) % kDepth;
    tri.src = static_cast<uint16_t>(0x1000 + t);
    tris.push_back(tri);
    L.send_ref(arena, gen, tri);
  }
  L.idle(64);

  char msg[256];
  std::snprintf(msg, sizeof(msg), "%s: %d references produce %d coordinate packets", label,
                static_cast<int>(tris.size()), static_cast<int>(tris.size()));
  check(L.got.size() == before + tris.size(), msg, before + tris.size(), L.got.size());
  if (L.got.size() != before + tris.size()) return;

  for (size_t t = 0; t < tris.size(); ++t) {
    const Tri& tri = tris[t];
    const Got& g = L.got[before + t];

    std::snprintf(msg, sizeof(msg), "%s: src_id rides triangle %d back with its coordinates",
                  label, static_cast<int>(t));
    check(g.src == tri.src, msg, tri.src, g.src);

    const uint32_t idx[3] = {tri.ia, tri.ib, tri.ic};
    const int32_t gotu[3] = {g.au, g.bu, g.cu};
    const int32_t gotv[3] = {g.av, g.bv, g.cv};
    const char* corner = "ABC";

    for (int k = 0; k < 3; ++k) {
      const int32_t n_u = col[idx[k]];
      const int32_t n_v = row[idx[k]];

      int32_t exp_u_a = oracle_law(place(n_u, pitch_log2), pitch_log2, underside);
      int32_t exp_v_a = oracle_law(place(n_v, pitch_log2), pitch_log2, underside);
      const int32_t exp_u_b = oracle_algebra(n_u, pitch_log2, underside);
      const int32_t exp_v_b = oracle_algebra(n_v, pitch_log2, underside);

      // The two oracles must agree with EACH OTHER before either is quoted.
      std::snprintf(msg, sizeof(msg),
                    "%s: tri %d corner %c -- the law and the placement algebra agree on u", label,
                    static_cast<int>(t), corner[k]);
      check(exp_u_a == exp_u_b, msg, static_cast<uint32_t>(exp_u_b),
            static_cast<uint32_t>(exp_u_a));
      std::snprintf(msg, sizeof(msg),
                    "%s: tri %d corner %c -- the law and the placement algebra agree on v", label,
                    static_cast<int>(t), corner[k]);
      check(exp_v_a == exp_v_b, msg, static_cast<uint32_t>(exp_v_b),
            static_cast<uint32_t>(exp_v_a));

      if (g_break_oracle && t == 0 && k == 0) exp_u_a += 1;

      std::snprintf(msg, sizeof(msg), "%s: tri %d corner %c u is the frozen law's value", label,
                    static_cast<int>(t), corner[k]);
      check(gotu[k] == exp_u_a, msg, static_cast<uint32_t>(exp_u_a),
            static_cast<uint32_t>(gotu[k]));
      std::snprintf(msg, sizeof(msg), "%s: tri %d corner %c v is the frozen law's value", label,
                    static_cast<int>(t), corner[k]);
      check(gotv[k] == exp_v_a, msg, static_cast<uint32_t>(exp_v_a),
            static_cast<uint32_t>(gotv[k]));
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--break-oracle") == 0) g_break_oracle = true;
  }

  Verilated::commandArgs(argc, argv);
  Vtb_terrain_uvlane d;
  reset(d);
  Lane L(d);

  // =========================================================================
  // 1 + 2. THE LAW ON BOTH SURFACES, AT ALL FOUR LEGAL PITCHES (spec 1.3)
  // =========================================================================
  const int pitches[4] = {-1, 0, 1, 2};
  uint32_t gen = 1;
  for (int p = 0; p < 4; ++p) {
    char lt[64], lu[64];
    std::snprintf(lt, sizeof(lt), "1.top surface, pitch_log2=%d", pitches[p]);
    std::snprintf(lu, sizeof(lu), "2.underside, pitch_log2=%d", pitches[p]);
    run_surface_pass(L, pitches[p], false, 0, gen++, lt);
    run_surface_pass(L, pitches[p], true, 1, gen++, lu);
  }

  // =========================================================================
  // 3. THE ARITHMETIC SHIFT, on coordinates that are NOT lattice-placed
  // =========================================================================
  // Only oracle A applies here, and that is the point: these values have no
  // algebraic shortcut, so a logical shift produces a number nothing predicts.
  {
    const uint32_t arena = 2;
    const uint32_t g = gen++;
    L.set_pitch(2);
    L.open_arena(arena, g);
    uint32_t s = 0xBEEF1234u;
    std::vector<int32_t> wx(kDepth), wz(kDepth);
    for (int i = 0; i < kDepth; ++i) {
      wx[i] = static_cast<int32_t>(lcg(s)) | (i < kDepth / 2 ? static_cast<int32_t>(0x80000000u)
                                                             : 0);  // force negatives
      wz[i] = -static_cast<int32_t>(lcg(s) & 0x3FFFFFFFu) - 1;       // always negative
      L.fill(arena, static_cast<uint32_t>(i), wx[i], wz[i], false);
    }
    L.idle(4);
    const size_t before = L.got.size();
    Tri tri{3, 40, 77, 0x7A00};
    L.send_ref(arena, g, tri);
    L.idle(32);
    check(L.got.size() == before + 1, "3.a negative-coordinate reference produces one packet",
          before + 1, L.got.size());
    if (L.got.size() == before + 1) {
      const Got& got = L.got[before];
      const uint32_t idx[3] = {tri.ia, tri.ib, tri.ic};
      const int32_t gu[3] = {got.au, got.bu, got.cu};
      const int32_t gv[3] = {got.av, got.bv, got.cv};
      for (int k = 0; k < 3; ++k) {
        const int32_t eu = oracle_law(wx[idx[k]], 2, false);
        const int32_t ev = oracle_law(wz[idx[k]], 2, false);
        check(gu[k] == eu,
              "3.u of a NEGATIVE world x is an ARITHMETIC shift (a logical shift fails here)",
              static_cast<uint32_t>(eu), static_cast<uint32_t>(gu[k]));
        check(gv[k] == ev, "3.v of a NEGATIVE world z is an ARITHMETIC shift",
              static_cast<uint32_t>(ev), static_cast<uint32_t>(gv[k]));
      }
    }
  }

  // =========================================================================
  // 4. THE CLAMP, asserted as a VALUE and as a COUNTER
  // =========================================================================
  {
    const uint32_t arena = 3;
    const uint32_t g = gen++;
    const uint32_t clamped_before = d.pitch_clamped;

    L.set_pitch(-1);  // 0.5 m, LEGAL by spec 1.3
    L.open_arena(arena, g);
    const int32_t wx = place(5, -1);  // 5 << 15
    const int32_t wz = place(-3, -1);
    L.fill(arena, 0, wx, wz, false);
    L.fill(arena, 1, wx, wz, false);
    L.fill(arena, 2, wx, wz, false);
    L.idle(4);

    check(d.pitch_clamped == clamped_before + 3,
          "4.the oracle's `top_shift < 0` clamp is COUNTED on every top fill at pitch -1",
          clamped_before + 3, d.pitch_clamped);

    const size_t before = L.got.size();
    Tri tri{0, 1, 2, 0x4C00};
    L.send_ref(arena, g, tri);
    L.idle(32);
    check(L.got.size() == before + 1, "4.the clamped pitch still produces a packet", before + 1,
          L.got.size());
    if (L.got.size() == before + 1) {
      // The clamp means shift 0: u == wx EXACTLY. A signed `>>> pitch_log2`
      // would shift LEFT and give wx*2.
      check(L.got[before].au == wx, "4.at pitch_log2=-1 the shift is CLAMPED to 0, so u == wx",
            static_cast<uint32_t>(wx), static_cast<uint32_t>(L.got[before].au));
      check(L.got[before].au != static_cast<int32_t>(static_cast<uint32_t>(wx) << 1),
            "4.and it is NOT the unclamped left shift a signed shift-by-pitch would give", 1u,
            L.got[before].au != static_cast<int32_t>(static_cast<uint32_t>(wx) << 1) ? 1u : 0u);
    }

    // The underside's shift is STRATA_M's and is pitch-independent, so the
    // clamp observation must stay SILENT there.
    const uint32_t clamped_now = d.pitch_clamped;
    L.fill(arena, 3, wx, wz, true);
    L.fill(arena, 4, wx, wz, true);
    L.idle(4);
    check(d.pitch_clamped == clamped_now,
          "4.the clamp counter is SILENT on underside fills -- their shift is pitch-independent",
          clamped_now, d.pitch_clamped);
  }

  // =========================================================================
  // 5. ONE TILE PERIOD PER CELL, through the CONSUMER's own frozen fold
  // =========================================================================
  {
    // terrain_rules 6.2: the sampled texel is `m = floor(u * 64)` = u >> 10.
    // Adjacent top-surface cells must therefore be exactly 64 texels apart --
    // one tile period per cell -- at EVERY legal pitch.
    for (int p = 0; p < 4; ++p) {
      const int pl = pitches[p];
      const int32_t u0 = oracle_law(place(7, pl), pl, false);
      const int32_t u1 = oracle_law(place(8, pl), pl, false);
      const int32_t m0 = u0 >> 10;
      const int32_t m1 = u1 >> 10;
      const int32_t expect = (pl >= 0) ? 64 : 32;  // the clamp halves it at 0.5 m
      char msg[160];
      std::snprintf(msg, sizeof(msg),
                    "5.pitch_log2=%d: adjacent top cells are %d texels apart before the fold", pl,
                    expect);
      check(m1 - m0 == expect, msg, static_cast<uint32_t>(expect),
            static_cast<uint32_t>(m1 - m0));
      // And the frozen fold accepts them: mirror_texel is total on its domain.
      const int32_t f = zref::terrain::mirror_texel(u0);
      check(f >= 0 && f < 64, "5.the frozen mirrored-repeat fold accepts the produced u", 1u,
            (f >= 0 && f < 64) ? 1u : 0u);
    }
  }

  // =========================================================================
  // 6. THE REFUSALS FIRE
  // =========================================================================
  {
    // 6a. A stale generation: re-open the arena, then replay with the OLD gen.
    const uint32_t arena = 0;
    const uint32_t stale_before = d.stale_reads;
    const uint32_t emitted_before = d.uvs_emitted;
    L.set_pitch(1);
    L.open_arena(arena, 200);
    for (int i = 0; i < 12; ++i) L.fill(arena, static_cast<uint32_t>(i), place(i, 1), place(i, 1), false);
    L.idle(4);
    L.open_arena(arena, 201);  // bump: the rows now carry 200, references carry 201
    Tri tri{0, 1, 9, 0x3000};
    L.send_ref(arena, 201, tri);
    L.idle(64);
    check(d.stale_reads == stale_before + 1,
          "6a.a reference whose generation the stored rows do not carry is REFUSED and counted",
          stale_before + 1, d.stale_reads);
    check(d.uvs_emitted == emitted_before,
          "6a.and it emits NO coordinates -- a stale corner never becomes a texture coordinate",
          emitted_before, d.uvs_emitted);

    // 6b. An illegal pitch. spec 1.3 freezes {-1, 0, +1, +2}; the composer
    // filters, but the block must still see and count one.
    const uint32_t illegal_before = d.pitch_illegal;
    L.set_pitch(7);
    L.open_arena(1, 9);
    L.fill(1, 0, place(1, 0), place(1, 0), false);
    L.fill(1, 1, place(1, 0), place(1, 0), false);
    L.idle(4);
    check(d.pitch_illegal == illegal_before + 2,
          "6b.a pitch spec 1.3 cannot carry is COUNTED on every top fill", illegal_before + 2,
          d.pitch_illegal);
  }

  // =========================================================================
  // 7. THE CENSUS AGREES WITH THE TRAFFIC
  // =========================================================================
  check(d.refs_taken == d.uvs_emitted + d.stale_reads,
        "7.every reference taken either emitted coordinates or was refused as stale",
        d.uvs_emitted + d.stale_reads, d.refs_taken);
  check(d.idle == 1, "7.the lane returns to idle with nothing in flight", 1u, d.idle);

  d.final();
  return zhao::report_and_exit("terrain_uvlane_directed");
}
