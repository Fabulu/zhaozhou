// terrain_matjoin_directed.cpp -- TERRAIN.MATJOIN, entry I34's material channel.
//
// WHAT THIS TEST IS EVIDENCE FOR, stated before the code so it can be checked
// against what it actually asserts:
//
//   1. With no field, the authored layer-E triple passes through BIT FOR BIT.
//      That is the property that makes composing this block safe, and it is the
//      first case rather than an afterthought.
//   2. A FIELD material write CHANGES the composed triple the compose cache
//      receives -- the owner's own acceptance sentence, at this block's ports.
//   3. The arbitration rule between authored layer E and the field result is
//      `zref::fieldir::compose_material`'s: LAST ENABLED WRITER WINS, in
//      accepted command order. Reached by presenting several lane words in both
//      orders and showing the ANSWER FOLLOWS THE ORDER.
//   4. The three independent ways a lane word can fail to be enabled -- the
//      section 9.1 footprint miss, ordinal-2 absence, and a token that is not
//      v1 -- each leave the authored triple untouched. "An absent output is not
//      a write of zero" is a ratified rule and this is where it is checked.
//   5. Every counter this block exports is SEEN TO FIRE, each against a
//      negative control on otherwise identical stimulus.
//
// THE ASYMMETRIC VALUE IS THE POINT. `{1,1,1}` would pass under a swapped,
// rotated or truncated byte layout. Authored triples come from the shared
// `layere_fixture.hpp` -- three different odd functions of the cell, so a
// swapped matA/weight is visible -- and the field triples are distinct bytes
// chosen the same way. A deliberately ROTATED token is checked to decode
// DIFFERENTLY, which is this test proving it could tell.
//
// THE ORACLE IS THE RATIFIED LAW ITSELF, not a transcription: the randomised
// sweep at the end builds `zref::fieldir::MaterialWrite` records and differences
// the block against `zref::fieldir::compose_material` over pseudo-random lane
// streams. A second hand-written implementation of "last enabled wins" here
// would be exactly the rival-law failure this project keeps paying for.

#include "Vzhao_terrain_matjoin.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include "layere_fixture.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fieldir.hpp"

namespace fi = zref::fieldir;

namespace {

/** One composed write, as TERRAIN.COMPCACHE's layer-E face receives it. */
struct Cap {
  int ci = 0;
  int cj = 0;
  uint8_t a = 0;
  uint8_t b = 0;
  uint8_t w = 0;
};

/** One field answer beat on the per-vertex lane stream. */
struct Lane {
  bool covers = true;
  bool present = true;
  uint32_t token = 0;
};

void reset(Vzhao_terrain_matjoin& m) {
  m.clk = 0;
  m.rst_n = 0;
  m.a_we_i = 0;
  m.a_ci_i = 0;
  m.a_cj_i = 0;
  m.a_mat_a_i = 0;
  m.a_mat_b_i = 0;
  m.a_weight_i = 0;
  m.f_fire_i = 0;
  m.f_covers_i = 0;
  m.f_present_i = 0;
  m.f_material_i = 0;
  m.st_fire_i = 0;
  for (int i = 0; i < 4; ++i) {
    m.eval();
    m.clk = 1;
    m.eval();
    m.clk = 0;
    m.eval();
  }
  m.rst_n = 1;
  m.eval();
}

/**
 * One clock. The composed write face is combinational off the held registers,
 * so it is sampled at the LOW phase -- before the edge that would retire it --
 * which is where the compose cache's fire-and-forget write port reads it.
 */
void cycle(Vzhao_terrain_matjoin& m, std::vector<Cap>& caps) {
  m.clk = 0;
  m.eval();
  if (m.o_we_o) {
    caps.push_back(Cap{static_cast<int>(m.o_ci_o), static_cast<int>(m.o_cj_o),
                       static_cast<uint8_t>(m.o_mat_a_o), static_cast<uint8_t>(m.o_mat_b_o),
                       static_cast<uint8_t>(m.o_weight_o)});
  }
  m.clk = 1;
  m.eval();
  m.clk = 0;
  m.eval();
}

/**
 * One vertex as `zhao_terrain_patch` sequences it:
 *   accept (the page stream's cell beat) -> lane words -> state publish.
 * `has_cell` false is one of the 65 lattice vertices that owns no cell.
 */
void vertex(Vzhao_terrain_matjoin& m, std::vector<Cap>& caps, bool has_cell, int ci, int cj,
            const std::vector<Lane>& lanes) {
  if (has_cell) {
    m.a_we_i = 1;
    m.a_ci_i = static_cast<uint8_t>(ci);
    m.a_cj_i = static_cast<uint8_t>(cj);
    m.a_mat_a_i = tess_test::mat_a_at(ci, cj);
    m.a_mat_b_i = tess_test::mat_b_at(ci, cj);
    m.a_weight_i = tess_test::weight_at(ci, cj);
    cycle(m, caps);
    m.a_we_i = 0;
  } else {
    cycle(m, caps);
  }

  for (const Lane& l : lanes) {
    m.f_fire_i = 1;
    m.f_covers_i = l.covers ? 1 : 0;
    m.f_present_i = l.present ? 1 : 0;
    m.f_material_i = l.token;
    cycle(m, caps);
    m.f_fire_i = 0;
    m.f_covers_i = 0;
    m.f_present_i = 0;
    m.f_material_i = 0;
  }

  m.st_fire_i = 1;
  cycle(m, caps);
  m.st_fire_i = 0;
}

/** The ratified law, applied to the same lane stream the block just saw. */
Cap oracle(int ci, int cj, const std::vector<Lane>& lanes) {
  fi::MaterialState authored{tess_test::mat_a_at(ci, cj), tess_test::mat_b_at(ci, cj),
                             tess_test::weight_at(ci, cj)};
  std::vector<fi::MaterialWrite> writes;
  writes.reserve(lanes.size());
  for (const Lane& l : lanes) {
    fi::MaterialWrite w;
    fi::MaterialState decoded{};
    const bool tag_ok = fi::material_token_decode(l.token, &decoded);
    w.enabled = l.covers && l.present && tag_ok;
    w.value = decoded;
    writes.push_back(w);
  }
  const fi::MaterialState out = fi::compose_material(authored, writes.data(), writes.size());
  return Cap{ci, cj, out.mat_a, out.mat_b, out.weight};
}

void ck(bool cond, const char* what, uint64_t expected = 0, uint64_t actual = 0) {
  zhao::check(cond, what, expected, actual);
}

uint32_t tok(uint8_t a, uint8_t b, uint8_t w) {
  return fi::material_token_encode(fi::MaterialState{a, b, w});
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // Three distinct bytes, none equal to any other and none equal to an
  // authored byte at the cells used below. A rotated layout cannot pass.
  constexpr uint8_t kFa = 0x2A, kFb = 0x7C, kFw = 0xB3;

  // =====================================================================
  // 0. THE ENCODING, BOTH DIRECTIONS, AND A PROOF THE TEST COULD TELL.
  // =====================================================================
  {
    const uint32_t t = tok(kFa, kFb, kFw);
    ck(t == 0xE12A7CB3u, "matjoin s0: the v1 token is not {tag,matA,matB,weight}", 0xE12A7CB3u, t);

    fi::MaterialState back{};
    ck(fi::material_token_decode(t, &back), "matjoin s0: a v1 token failed to decode");
    ck(back.mat_a == kFa && back.mat_b == kFb && back.weight == kFw,
       "matjoin s0: the round trip did not preserve the triple");

    // The negative control on the ASYMMETRY claim: a byte-rotated token must
    // decode to something DIFFERENT, or the asymmetric value is proving nothing.
    fi::MaterialState rot{};
    ck(fi::material_token_decode(tok(kFb, kFw, kFa), &rot),
       "matjoin s0: the rotated token should still be a v1 token");
    ck(!(rot.mat_a == back.mat_a && rot.mat_b == back.mat_b && rot.weight == back.weight),
       "matjoin s0: a rotated layout decodes identically -- the value is not asymmetric");

    // The additive zero the adapter parks on an absent lane is REFUSED, not
    // decoded as the legal triple {0,0,0}.
    fi::MaterialState zero{9, 9, 9};
    ck(!fi::material_token_decode(0u, &zero), "matjoin s0: token 0 was accepted as a v1 token");
    ck(zero.mat_a == 9 && zero.mat_b == 9 && zero.weight == 9,
       "matjoin s0: a refused decode modified the caller's state");
    ck(!fi::material_token_tag_ok(0x002A7CB3u),
       "matjoin s0: a legal-looking triple under a WRONG TAG was accepted");
  }

  // =====================================================================
  // 1. NO FIELD: the authored triple passes through BIT FOR BIT.
  //    This is the property that makes composition safe.
  // =====================================================================
  {
    Vzhao_terrain_matjoin m;
    reset(m);
    std::vector<Cap> caps;
    int bad = 0;
    for (int cj = 0; cj < 4; ++cj) {
      for (int ci = 0; ci < 4; ++ci) vertex(m, caps, true, ci, cj, {});
    }
    ck(caps.size() == 16, "matjoin s1: expected one composed write per cell", 16,
       static_cast<uint64_t>(caps.size()));
    for (const Cap& c : caps) {
      if (c.a != tess_test::mat_a_at(c.ci, c.cj) || c.b != tess_test::mat_b_at(c.ci, c.cj) ||
          c.w != tess_test::weight_at(c.ci, c.cj)) {
        ++bad;
      }
    }
    ck(bad == 0, "matjoin s1: an authored triple was altered with no field present", 0,
       static_cast<uint64_t>(bad));
    ck(m.field_composed_o == 0u, "matjoin s1: field_composed fired with no field",
       0, m.field_composed_o);
    ck(m.token_refused_o == 0u, "matjoin s1: token_refused fired with no token",
       0, m.token_refused_o);
    ck(m.held_overrun_o == 0u, "matjoin s1: the sequencing guard fired on a correct walk",
       0, m.held_overrun_o);
    ck(m.cells_written_o == 16u, "matjoin s1: cells_written disagrees with the writes seen", 16,
       m.cells_written_o);
    std::printf("matjoin s1: 16 cells, authored preserved bit-exact, all guards silent\n");
  }

  // =====================================================================
  // 2. THE OWNER'S TEST AT THIS BLOCK'S PORTS: a FIELD material write
  //    CHANGES the triple the compose cache receives.
  // =====================================================================
  {
    Vzhao_terrain_matjoin m;
    reset(m);
    std::vector<Cap> caps;
    const int ci = 3, cj = 5;
    vertex(m, caps, true, ci, cj, {Lane{true, true, tok(kFa, kFb, kFw)}});

    ck(caps.size() == 1, "matjoin s2: expected exactly one composed write", 1,
       static_cast<uint64_t>(caps.size()));
    ck(caps[0].a == kFa && caps[0].b == kFb && caps[0].w == kFw,
       "matjoin s2: the FIELD triple did not reach the compose cache face");

    // ... and it is genuinely a CHANGE, not a coincidence with the authored
    // value. Stated as its own assertion because "the output equals X" is only
    // evidence if X differs from what was already there.
    ck(!(tess_test::mat_a_at(ci, cj) == kFa && tess_test::mat_b_at(ci, cj) == kFb &&
         tess_test::weight_at(ci, cj) == kFw),
       "matjoin s2: the fixture's authored triple equals the field triple -- case is vacuous");
    ck(caps[0].a != tess_test::mat_a_at(ci, cj) || caps[0].b != tess_test::mat_b_at(ci, cj) ||
           caps[0].w != tess_test::weight_at(ci, cj),
       "matjoin s2: the composed triple is still the authored one");
    ck(m.field_composed_o == 1u, "matjoin s2: field_composed did not fire", 1,
       m.field_composed_o);
    std::printf("matjoin s2: FIELD write {%02X,%02X,%02X} replaced authored {%02X,%02X,%02X}\n",
                kFa, kFb, kFw, tess_test::mat_a_at(ci, cj), tess_test::mat_b_at(ci, cj),
                tess_test::weight_at(ci, cj));
  }

  // =====================================================================
  // 3. THE THREE WAYS A LANE WORD IS NOT ENABLED, each leaving the
  //    authored triple untouched. Identical stimulus but for one term.
  // =====================================================================
  {
    struct Neg {
      const char* name;
      Lane lane;
      bool expect_refused;
    };
    const Neg negs[] = {
        {"footprint miss (section 9.1)", Lane{false, true, tok(kFa, kFb, kFw)}, false},
        {"ordinal-2 ABSENT (absent is not a write of zero)",
         Lane{true, false, tok(kFa, kFb, kFw)}, false},
        {"a legal-looking triple under a WRONG TAG", Lane{true, true, 0x002A7CB3u}, true},
        {"the adapter's additive zero on an absent lane", Lane{true, true, 0u}, true},
    };
    for (const Neg& n : negs) {
      Vzhao_terrain_matjoin m;
      reset(m);
      std::vector<Cap> caps;
      const int ci = 6, cj = 2;
      vertex(m, caps, true, ci, cj, {n.lane});
      char what[192];
      std::snprintf(what, sizeof(what), "matjoin s3: authored triple not preserved under %s",
                    n.name);
      ck(caps.size() == 1 && caps[0].a == tess_test::mat_a_at(ci, cj) &&
             caps[0].b == tess_test::mat_b_at(ci, cj) && caps[0].w == tess_test::weight_at(ci, cj),
         what);
      char what2[192];
      std::snprintf(what2, sizeof(what2), "matjoin s3: field_composed fired under %s", n.name);
      ck(m.field_composed_o == 0u, what2, 0, m.field_composed_o);
      char what3[192];
      std::snprintf(what3, sizeof(what3), "matjoin s3: token_refused wrong under %s", n.name);
      ck(m.token_refused_o == (n.expect_refused ? 1u : 0u), what3,
         n.expect_refused ? 1u : 0u, m.token_refused_o);
    }
    std::printf("matjoin s3: four non-enabled forms, authored preserved, refusals counted\n");
  }

  // =====================================================================
  // 4. THE ARBITRATION RULE, REACHED: last ENABLED writer wins, and the
  //    ANSWER FOLLOWS THE ORDER. Two orders over the same three words.
  // =====================================================================
  {
    const uint32_t t1 = tok(0x11, 0x22, 0x33);
    const uint32_t t2 = tok(0x44, 0x55, 0x66);

    struct Ord {
      const char* name;
      std::vector<Lane> lanes;
      uint8_t wa, wb, ww;
    };
    const Ord orders[] = {
        // t1 then t2: t2 is the last ENABLED word.
        {"t1,t2", {Lane{true, true, t1}, Lane{true, true, t2}}, 0x44, 0x55, 0x66},
        // the same two words, swapped: now t1 wins. The order IS the priority.
        {"t2,t1", {Lane{true, true, t2}, Lane{true, true, t1}}, 0x11, 0x22, 0x33},
        // a DISABLED later word must not displace an enabled earlier one --
        // this is where "last writer" and "last ENABLED writer" differ.
        {"t1,t2-absent", {Lane{true, true, t1}, Lane{true, false, t2}}, 0x11, 0x22, 0x33},
        // nor may a refused token displace it.
        {"t1,badtag", {Lane{true, true, t1}, Lane{true, true, 0x00445566u}}, 0x11, 0x22, 0x33},
    };
    for (const Ord& o : orders) {
      Vzhao_terrain_matjoin m;
      reset(m);
      std::vector<Cap> caps;
      const int ci = 9, cj = 7;
      vertex(m, caps, true, ci, cj, o.lanes);
      char what[192];
      std::snprintf(what, sizeof(what), "matjoin s4: wrong winner for order %s", o.name);
      ck(caps.size() == 1 && caps[0].a == o.wa && caps[0].b == o.wb && caps[0].w == o.ww, what);

      const Cap want = oracle(ci, cj, o.lanes);
      char what2[192];
      std::snprintf(what2, sizeof(what2), "matjoin s4: order %s disagrees with compose_material",
                    o.name);
      ck(caps.size() == 1 && caps[0].a == want.a && caps[0].b == want.b && caps[0].w == want.w,
         what2);
    }
    std::printf("matjoin s4: last-enabled-writer-wins, and the answer follows the order\n");
  }

  // =====================================================================
  // 5. THE COUNTERS FIRE, each beside a negative control.
  // =====================================================================
  {
    // lane_no_cell: the 65 lattice vertices that own no cell still receive
    // lane words. A CENSUS, not a fault -- asserted so that a later reader
    // finds it named rather than guesses.
    {
      Vzhao_terrain_matjoin m;
      reset(m);
      std::vector<Cap> caps;
      vertex(m, caps, true, 1, 1, {Lane{true, true, tok(kFa, kFb, kFw)}});
      ck(m.lane_no_cell_o == 0u, "matjoin s5: lane_no_cell fired for a vertex that owns a cell",
         0, m.lane_no_cell_o);
      vertex(m, caps, false, 0, 0, {Lane{true, true, tok(kFa, kFb, kFw)}});
      ck(m.lane_no_cell_o == 1u, "matjoin s5: lane_no_cell did not fire for a cell-less vertex",
         1, m.lane_no_cell_o);
      ck(caps.size() == 1, "matjoin s5: a cell-less vertex produced a composed write", 1,
         static_cast<uint64_t>(caps.size()));
    }

    // held_overrun: the sequencing guard. UNREACHABLE in the composed console
    // -- `zhao_terrain_patch` holds one vertex at a time -- so it is driven
    // deliberately here, which is the whole reason this block has its own
    // bench. The negative control is the identical walk WITH the state publish.
    {
      Vzhao_terrain_matjoin m;
      reset(m);
      std::vector<Cap> caps;
      vertex(m, caps, true, 2, 3, {});
      vertex(m, caps, true, 4, 5, {});
      ck(m.held_overrun_o == 0u, "matjoin s5: overrun fired on correctly sequenced vertices", 0,
         m.held_overrun_o);

      Vzhao_terrain_matjoin n;
      reset(n);
      std::vector<Cap> ncaps;
      // Two accepts, no state publish between them: the fault the guard exists
      // for. The held cell must still be emitted -- a dropped cell would be the
      // silent failure, so the guard is LOSSLESS as well as loud.
      n.a_we_i = 1;
      n.a_ci_i = 2;
      n.a_cj_i = 3;
      n.a_mat_a_i = tess_test::mat_a_at(2, 3);
      n.a_mat_b_i = tess_test::mat_b_at(2, 3);
      n.a_weight_i = tess_test::weight_at(2, 3);
      cycle(n, ncaps);
      n.a_ci_i = 4;
      n.a_cj_i = 5;
      n.a_mat_a_i = tess_test::mat_a_at(4, 5);
      n.a_mat_b_i = tess_test::mat_b_at(4, 5);
      n.a_weight_i = tess_test::weight_at(4, 5);
      cycle(n, ncaps);
      n.a_we_i = 0;
      n.st_fire_i = 1;
      cycle(n, ncaps);
      n.st_fire_i = 0;

      ck(n.held_overrun_o == 1u, "matjoin s5: the sequencing guard did NOT fire on a real overrun",
         1, n.held_overrun_o);
      ck(ncaps.size() == 2, "matjoin s5: the overrun dropped a cell instead of emitting it", 2,
         static_cast<uint64_t>(ncaps.size()));
      ck(ncaps.size() == 2 && ncaps[0].ci == 2 && ncaps[0].cj == 3,
         "matjoin s5: the overrun emitted the wrong cell");
    }
    std::printf("matjoin s5: lane_no_cell and held_overrun both fired, each against a control\n");
  }

  // =====================================================================
  // 6. THE DIFFERENTIAL: pseudo-random lane streams against the ratified
  //    `zref::fieldir::compose_material`, over cells whose authored triples
  //    come from the shared fixture.
  // =====================================================================
  {
    Vzhao_terrain_matjoin m;
    reset(m);
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    auto next = [&rng]() {
      rng ^= rng << 13;
      rng ^= rng >> 7;
      rng ^= rng << 17;
      return rng;
    };

    int mismatches = 0;
    int enabled_seen = 0;
    int refused_seen = 0;
    for (int trial = 0; trial < 600; ++trial) {
      const int ci = static_cast<int>(next() % 32);
      const int cj = static_cast<int>(next() % 32);
      const int nlanes = static_cast<int>(next() % 5);  // 0..4, 16 is the cap
      std::vector<Lane> lanes;
      for (int i = 0; i < nlanes; ++i) {
        const uint64_t r = next();
        Lane l;
        l.covers = (r & 1u) != 0;
        l.present = (r & 2u) != 0;
        const uint8_t a = static_cast<uint8_t>(r >> 8);
        const uint8_t b = static_cast<uint8_t>(r >> 16);
        const uint8_t w = static_cast<uint8_t>(r >> 24);
        // One word in four carries a deliberately corrupt tag, so the refusal
        // path is exercised by the sweep and not only by the directed case.
        l.token = ((r >> 4) & 3u) == 0 ? ((static_cast<uint32_t>(a) << 16) |
                                          (static_cast<uint32_t>(b) << 8) | w)
                                       : tok(a, b, w);
        if (l.covers && l.present) {
          if (fi::material_token_tag_ok(l.token)) {
            ++enabled_seen;
          } else {
            ++refused_seen;
          }
        }
        lanes.push_back(l);
      }
      std::vector<Cap> caps;
      vertex(m, caps, true, ci, cj, lanes);
      const Cap want = oracle(ci, cj, lanes);
      if (caps.size() != 1 || caps[0].a != want.a || caps[0].b != want.b || caps[0].w != want.w ||
          caps[0].ci != ci || caps[0].cj != cj) {
        ++mismatches;
      }
    }
    ck(mismatches == 0, "matjoin s6: the block disagrees with zref::fieldir::compose_material", 0,
       static_cast<uint64_t>(mismatches));
    // The sweep is only evidence if it REACHED both paths. A differential over
    // stimulus that never enables a writer passes trivially.
    ck(enabled_seen > 50, "matjoin s6: the sweep barely enabled a writer -- it proves little", 50,
       static_cast<uint64_t>(enabled_seen));
    ck(refused_seen > 10, "matjoin s6: the sweep barely refused a token -- it proves little", 10,
       static_cast<uint64_t>(refused_seen));
    ck(m.token_refused_o == static_cast<uint32_t>(refused_seen),
       "matjoin s6: token_refused disagrees with the refusals the stimulus contained",
       static_cast<uint64_t>(refused_seen), m.token_refused_o);
    std::printf("matjoin s6: 600 random vertices vs compose_material, %d enabled, %d refused\n",
                enabled_seen, refused_seen);
  }

  return zhao::report_and_exit("terrain_matjoin_directed");
}
