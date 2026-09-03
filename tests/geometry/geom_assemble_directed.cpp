// geom_assemble_directed.cpp — the index walk, against the oracle.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS AT ALL
// ---------------------------------------------------------------------------
// `BORING_3D_FUNDAMENTALS_AUDIT.md` R1: MESHFETCH emits `index_offset` and
// `triangle_count`, VDECODE accepts neither, SETUP expects a complete
// triangle, and `tri_ax_i` was driven only from a harness. Nothing turned a
// meshlet's index stream into triangles.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY WORTH TESTING
// ---------------------------------------------------------------------------
// The arithmetic is one addition, so a test that only checked sums would be
// theatre. The cases below are the ones where this block can be silently
// wrong:
//
//   * a local index at `vertex_count` — the count is a count, not a last
//     index, and clamping it draws a triangle from a real vertex belonging to
//     a different part of the mesh;
//   * a meshlet over its frozen limits — refused WHOLE, not as a truncated
//     prefix, because a mesh missing its tail looks like a modelling error;
//   * DUO — the same local index resolves to a DIFFERENT projected vertex per
//     view, and getting that wrong gives one eye a correct image and the other
//     a subtly wrong one;
//   * a stalling consumer — the walk must resume without losing position,
//     because losing it drops triangles from the MIDDLE of a mesh.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_assemble.h"

#include "zhao_sim.hpp"
#include "zref/zref_geom.hpp"

namespace {

struct Tri {
  unsigned v[3];
  int last;
};

// Drive one meshlet through, answering the index port from `idx`.
std::vector<Tri> run(Vzhao_geom_assemble& t, const std::vector<uint8_t>& idx,
                     unsigned voff, unsigned vcount, unsigned tcount,
                     bool stall) {
  std::vector<Tri> out;
  t.m_valid_i = 1;
  t.m_vertex_offset_i = voff;
  t.m_vertex_count_i = vcount;
  t.m_triangle_count_i = tcount;
  t.m_material_id_i = 0x0777;
  t.m_raster_state_i = 0x12345678;
  t.m_src_id_i = 0xBEEF;
  t.ix_valid_i = 0;
  t.t_ready_i = 1;
  t.eval();
  zhao::tick(t);
  t.m_valid_i = 0;

  uint32_t g = 0x51u;
  for (int c = 0; c < 8000; ++c) {
    g = g * 1664525u + 1013904223u;
    t.t_ready_i = stall ? (((g >> 25) & 3u) != 0u) : 1u;

    // answer the index request
    t.ix_valid_i = 0;
    if (t.ix_req_o) {
      const unsigned n = t.ix_index_o;
      if (n * 3 + 2 < idx.size()) {
        t.ix_valid_i = 1;
        t.ix_a_i = idx[n * 3 + 0];
        t.ix_b_i = idx[n * 3 + 1];
        t.ix_c_i = idx[n * 3 + 2];
      }
    }
    t.eval();

    if (t.t_valid_o && t.t_ready_i) {
      out.push_back({{static_cast<unsigned>(t.t_v0_o),
                      static_cast<unsigned>(t.t_v1_o),
                      static_cast<unsigned>(t.t_v2_o)},
                     static_cast<int>(t.t_last_o)});
    }
    zhao::tick(t);
    if (!t.ix_req_o && !t.t_valid_o && !out.empty()) break;
  }
  t.ix_valid_i = 0;
  t.t_ready_i = 1;
  return out;
}

std::vector<uint8_t> make_indices(unsigned tcount, unsigned vcount) {
  std::vector<uint8_t> v;
  for (unsigned n = 0; n < tcount; ++n) {
    v.push_back(static_cast<uint8_t>((n * 3 + 0) % vcount));
    v.push_back(static_cast<uint8_t>((n * 3 + 1) % vcount));
    v.push_back(static_cast<uint8_t>((n * 3 + 2) % vcount));
  }
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_assemble top;

  auto reset = [&]() {
    top.m_valid_i = 0;
    top.ix_valid_i = 0;
    top.t_ready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };
  reset();

  // ---- 1: every triplet, in index-stream order, against the oracle -------
  {
    const unsigned vcount = 40, tcount = 30, voff = 1000;
    const auto idx = make_indices(tcount, vcount);
    const auto out = run(top, idx, voff, vcount, tcount, false);

    int bad = 0;
    if (out.size() != tcount) ++bad;
    for (size_t n = 0; n < out.size(); ++n) {
      const auto want =
          zref::geom::assemble_triangle(idx.data(), static_cast<unsigned>(n),
                                        voff, vcount);
      if (!want.legal || out[n].v[0] != want.v[0] || out[n].v[1] != want.v[1] ||
          out[n].v[2] != want.v[2]) {
        if (bad < 3)
          std::printf("    tri %zu: rtl (%u,%u,%u) oracle (%u,%u,%u)\n", n,
                      out[n].v[0], out[n].v[1], out[n].v[2], want.v[0],
                      want.v[1], want.v[2]);
        ++bad;
      }
    }
    zhao::check(bad == 0,
                "every triplet becomes a triangle in index-stream ORDER, with "
                "global ids matching zref::geom::assemble_triangle -- the "
                "order is the contract, because two orderings give the same "
                "picture and different capture CRCs",
                0, bad);

    int lasts = 0, last_at = -1;
    for (size_t n = 0; n < out.size(); ++n)
      if (out[n].last) { ++lasts; last_at = static_cast<int>(n); }
    zhao::check(lasts == 1 && last_at == static_cast<int>(out.size()) - 1,
                "`last` marks exactly the final triangle, once", 1,
                (lasts == 1 && last_at == static_cast<int>(out.size()) - 1) ? 1
                                                                           : 0);
  }

  // ---- 2: DUO — the same local index is a DIFFERENT vertex per view ------
  // The subtle one. `vertex_offset` is per view, so a walk that emitted into
  // both views from one offset would give view 1 the vertices of view 0: a
  // correct image in one eye, a subtly wrong one in the other.
  {
    const unsigned vcount = 16, tcount = 4;
    const auto idx = make_indices(tcount, vcount);
    const auto v0 = run(top, idx, 100, vcount, tcount, false);
    const auto v1 = run(top, idx, 900, vcount, tcount, false);

    int bad = 0;
    if (v0.size() != tcount || v1.size() != tcount) ++bad;
    for (size_t n = 0; n < v0.size() && n < v1.size(); ++n)
      for (int k = 0; k < 3; ++k)
        if (v1[n].v[k] != v0[n].v[k] + 800) ++bad;

    zhao::check(bad == 0,
                "the same meshlet in a second view resolves to that view's OWN "
                "projected vertices -- one walk per visible view, and a shared "
                "one would give the second eye the first eye's geometry",
                0, bad);
  }

  // ---- 3: a local index AT the vertex count is refused -------------------
  // Not clamped: a clamped index draws a triangle from a real vertex belonging
  // to a different part of the mesh, which nothing downstream can detect.
  {
    const unsigned vcount = 8, tcount = 3;
    std::vector<uint8_t> idx = make_indices(tcount, vcount);
    idx[3] = static_cast<uint8_t>(vcount);  // exactly AT the count

    const uint32_t before = top.refused_index_o;
    const auto out = run(top, idx, 0, vcount, tcount, false);

    zhao::check(out.size() == tcount - 1,
                "a triplet with an index AT the vertex count emits NO triangle "
                "-- the count is a count, not a last index",
                static_cast<int>(tcount - 1), static_cast<int>(out.size()));
    zhao::check(top.refused_index_o == before + 1,
                "and it is counted, so a corrupt mesh is visible rather than "
                "merely smaller",
                1, static_cast<int>(top.refused_index_o - before));
    zhao::check(zref::geom::assemble_triangle(idx.data(), 1, 0, vcount).legal ==
                    false,
                "the oracle agrees that triplet is illegal", 1, 1);
  }

  // ---- 4: the frozen limits, refused WHOLE -------------------------------
  {
    const uint32_t before = top.refused_limits_o;
    int emitted = 0;
    const auto idx = make_indices(4, 8);
    emitted += static_cast<int>(run(top, idx, 0, 65, 4, false).size());   // >64
    emitted += static_cast<int>(run(top, idx, 0, 8, 127, false).size());  // >126
    emitted += static_cast<int>(run(top, idx, 0, 0, 4, false).size());    // zero
    zhao::check(emitted == 0 && top.refused_limits_o == before + 3,
                "65 vertices, 127 triangles and zero vertices are each refused "
                "WHOLE -- not as a truncated prefix, because a mesh missing "
                "its tail looks like a modelling error rather than a fault",
                3, static_cast<int>(top.refused_limits_o - before));
  }

  // ---- 5: a stalling consumer does not lose position ---------------------
  {
    const unsigned vcount = 24, tcount = 20, voff = 500;
    const auto idx = make_indices(tcount, vcount);
    const auto clean = run(top, idx, voff, vcount, tcount, false);
    const auto stalled = run(top, idx, voff, vcount, tcount, true);

    int diff = 0;
    if (clean.size() != stalled.size()) ++diff;
    for (size_t n = 0; n < clean.size() && n < stalled.size(); ++n)
      for (int k = 0; k < 3; ++k)
        if (clean[n].v[k] != stalled[n].v[k]) ++diff;

    zhao::check(diff == 0,
                "a stalling consumer changes neither the count nor the order "
                "-- the walk resumes without losing position, which is what "
                "stops triangles vanishing from the MIDDLE of a mesh",
                0, diff);
  }

  // ---- 6: a zero-triangle meshlet is legal and not a refusal -------------
  {
    const uint32_t before = top.refused_limits_o;
    const auto idx = make_indices(1, 8);
    const auto out = run(top, idx, 0, 8, 0, false);
    zhao::check(out.empty() && top.refused_limits_o == before,
                "a meshlet with no triangles emits nothing and is NOT counted "
                "as refused -- it is legal, and conflating the two would make "
                "an empty mesh look like a corrupt one",
                0, static_cast<int>(top.refused_limits_o - before));
  }

  std::printf("  %u meshlets, %u triangles, %u limit-refused, %u index-refused\n",
              top.meshlets_o, top.triangles_o, top.refused_limits_o,
              top.refused_index_o);

  return zhao::report_and_exit("geom_assemble_directed");
}
