// forge_fanindex_directed.cpp -- a ring becomes a fan, and the two faults it
// refuses rather than wedges on both FIRE with legal stimulus.
//
// THE ACCEPTANCE QUESTIONS, and nothing wider (CLAUDE.md: bound validation to
// the question):
//
//   1. A ring of N produces EXACTLY N-2 triples, in the order (0,1,2), (0,2,3)
//      ... (0,N-2,N-1), with `t_last` on the final one and on no other. The
//      fan order is the ring's own emission order -- the same ordering
//      convention `zhao_forge_prim` obeys, and the reason it matters is
//      identical: two orderings give the same picture and different capture
//      CRCs.
//   2. The vertices are FORWARDED UNCHANGED and not stored. This block must not
//      become a second copy of the assembler's vertex store.
//   3. The per-primitive ALPHA is LATCHED at the hull's first vertex and held
//      through the triangle phase. A combinational passthrough would read the
//      NEXT caster's strength onto THIS hull's triangles, which is the
//      metadata-swap shape and is what this check exists for.
//   4. `short_ring_o` FIRES on a two-vertex ring, and the block RETIRES rather
//      than deadlocking the assembler that is already holding those vertices.
//   5. `ring_overflow_o` FIRES on a ring longer than MAXV, and the hull is
//      TRUNCATED rather than overrunning the store.
//   6. Backpressure on either stream loses nothing and invents nothing.
//
// A DETECTOR READING ZERO IS A CLAIM. Checks 4 and 5 are the positive controls
// for this block's two fault counters and both are reachable from the port, so
// neither needs a committed mutant.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_forge_fanindex.h"
#include "verilated.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

struct Triple {
  int i0, i1, i2, last, src, material;
};

struct Vert {
  int32_t x, y, z;
  int last;
};

void reset(Vzhao_forge_fanindex& d) {
  d.rst_n = 0;
  d.vtx_valid_i = 0;
  d.vtx_x_i = 0;
  d.vtx_y_i = 0;
  d.vtx_z_i = 0;
  d.vtx_alpha_i = 0;
  d.vtx_last_i = 0;
  d.vtx_src_id_i = 0;
  d.vtx_rung_i = 0;
  d.v_ready_i = 1;
  d.t_ready_i = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

struct Run {
  std::vector<Vert> verts;
  std::vector<Triple> tris;
  std::vector<int> alpha_during_tris;
  int alpha_at_first_offer = -1;
};

// Drive one ring of `n` vertices with the given alpha/src/rung, then drain the
// triangle phase. `v_gap` / `t_gap` insert backpressure so the same sequence is
// exercised with and without stalls.
Run ring(Vzhao_forge_fanindex& d, int n, int alpha, int src, int rung, int v_gap, int t_gap,
         bool omit_last = false) {
  Run r;
  int sent = 0;
  int vcount = 0;
  int tcount = 0;
  for (int cycle = 0; cycle < 8000; ++cycle) {
    const bool offering = sent < n;
    d.vtx_valid_i = offering ? 1 : 0;
    if (offering) {
      d.vtx_x_i = static_cast<int32_t>(0x1000 + sent);
      d.vtx_y_i = static_cast<int32_t>(0x2000 + sent);
      d.vtx_z_i = static_cast<int32_t>(0x3000 + sent);
      d.vtx_alpha_i = alpha;
      d.vtx_src_id_i = src;
      d.vtx_rung_i = rung;
      d.vtx_last_i = (!omit_last && sent + 1 == n) ? 1 : 0;
    } else {
      d.vtx_last_i = 0;
    }
    d.v_ready_i = (v_gap == 0 || (cycle % (v_gap + 1)) == 0) ? 1 : 0;
    d.t_ready_i = (t_gap == 0 || (cycle % (t_gap + 1)) == 0) ? 1 : 0;
    d.eval();

    if (r.alpha_at_first_offer < 0 && d.v_valid_o && vcount == 0) {
      r.alpha_at_first_offer = d.j_vertex_alpha_o;
    }
    if (d.v_valid_o && d.v_ready_i) {
      r.verts.push_back({static_cast<int32_t>(d.v_x_o), static_cast<int32_t>(d.v_y_o),
                         static_cast<int32_t>(d.v_z_o), d.v_last_o});
      ++vcount;
      ++sent;
    }
    if (d.t_valid_o) {
      r.alpha_during_tris.push_back(d.j_vertex_alpha_o);
      if (d.t_ready_i) {
        r.tris.push_back({d.t_i0_o, d.t_i1_o, d.t_i2_o, d.t_last_o, d.t_src_id_o, d.t_material_o});
        ++tcount;
        const bool done = d.t_last_o != 0;
        zhao::tick(d);
        if (done) {
          d.vtx_valid_i = 0;
          d.vtx_last_i = 0;
          return r;
        }
        continue;
      }
    }
    zhao::tick(d);
  }
  d.vtx_valid_i = 0;
  d.vtx_last_i = 0;
  return r;
}

void check_fan(const Run& r, int n, const char* label) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s: vertices forwarded", label);
  check(static_cast<int>(r.verts.size()) == n, buf, n, static_cast<long long>(r.verts.size()));

  const int want_tris = (n >= 3) ? (n - 2) : 1;  // a short ring makes ONE degenerate triple
  std::snprintf(buf, sizeof(buf), "%s: triple count", label);
  check(static_cast<int>(r.tris.size()) == want_tris, buf, want_tris,
        static_cast<long long>(r.tris.size()));
  if (static_cast<int>(r.tris.size()) != want_tris) return;

  for (int k = 0; k < want_tris; ++k) {
    const Triple& t = r.tris[k];
    const int want_i1 = (n >= 3) ? (k + 1) : 0;
    const int want_i2 = (n >= 3) ? (k + 2) : 0;
    std::snprintf(buf, sizeof(buf), "%s: triple %d i0", label, k);
    check(t.i0 == 0, buf, 0, t.i0);
    std::snprintf(buf, sizeof(buf), "%s: triple %d i1", label, k);
    check(t.i1 == want_i1, buf, want_i1, t.i1);
    std::snprintf(buf, sizeof(buf), "%s: triple %d i2", label, k);
    check(t.i2 == want_i2, buf, want_i2, t.i2);
    std::snprintf(buf, sizeof(buf), "%s: triple %d last", label, k);
    const int want_last = (k == want_tris - 1) ? 1 : 0;
    check(t.last == want_last, buf, want_last, t.last);
    // The assembler's `mat_skew_o` differences this against the job's id, and a
    // shadow job's id is ZERO by the material window's MATMODE_NONE law.
    std::snprintf(buf, sizeof(buf), "%s: triple %d material is the job's zero", label, k);
    check(t.material == 0, buf, 0, t.material);
  }

  // The vertices arrive unchanged and `v_last` marks exactly one.
  int lasts = 0;
  for (int k = 0; k < static_cast<int>(r.verts.size()); ++k) {
    std::snprintf(buf, sizeof(buf), "%s: vertex %d x unchanged", label, k);
    check(r.verts[k].x == 0x1000 + k, buf, 0x1000 + k, r.verts[k].x);
    std::snprintf(buf, sizeof(buf), "%s: vertex %d y unchanged", label, k);
    check(r.verts[k].y == 0x2000 + k, buf, 0x2000 + k, r.verts[k].y);
    std::snprintf(buf, sizeof(buf), "%s: vertex %d z unchanged", label, k);
    check(r.verts[k].z == 0x3000 + k, buf, 0x3000 + k, r.verts[k].z);
    lasts += r.verts[k].last;
  }
  std::snprintf(buf, sizeof(buf), "%s: exactly one v_last", label);
  check(lasts == 1, buf, 1, lasts);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_fanindex d;
  reset(d);

  // ---- 1. the three ladder rungs, no backpressure -------------------------
  {
    const Run r16 = ring(d, 16, 0x5A, 0x2A2A, 0, 0, 0);
    check_fan(r16, 16, "hero ring of 16");
    check(d.hulls_o == 1, "hero: hulls_o", 1, d.hulls_o);
    check(d.triangles_o == 14, "hero: triangles_o", 14, d.triangles_o);
    check(d.hulls_rung_o[0] == 1, "hero: rung 0 census", 1, d.hulls_rung_o[0]);

    const Run r8 = ring(d, 8, 0x40, 0x2A2A, 1, 0, 0);
    check_fan(r8, 8, "army ring of 8");
    check(d.hulls_o == 2, "army: hulls_o", 2, d.hulls_o);
    check(d.triangles_o == 14 + 6, "army: triangles_o", 20, d.triangles_o);
    check(d.hulls_rung_o[1] == 1, "army: rung 1 census", 1, d.hulls_rung_o[1]);

    const Run r4 = ring(d, 4, 0x20, 0x2A2A, 2, 0, 0);
    check_fan(r4, 4, "mid ring of 4");
    check(d.hulls_o == 3, "mid: hulls_o", 3, d.hulls_o);
    check(d.triangles_o == 22, "mid: triangles_o", 22, d.triangles_o);
    check(d.hulls_rung_o[2] == 1, "mid: rung 2 census", 1, d.hulls_rung_o[2]);

    check(d.short_ring_o == 0, "no short ring yet", 0, d.short_ring_o);
    check(d.ring_overflow_o == 0, "no overflow yet", 0, d.ring_overflow_o);
  }

  // ---- 2. the alpha is LATCHED, not passed through -------------------------
  // The strength is on the wire only while the caster is emitting. During the
  // triangle phase `zhao_forge_shadow` has retired and its port describes the
  // NEXT caster, so a passthrough would put the wrong alpha on these triangles.
  {
    reset(d);
    const Run r = ring(d, 8, 0x77, 0x1234, 1, 0, 2);
    check_fan(r, 8, "latched-alpha ring of 8");
    check(r.alpha_at_first_offer == 0x77, "alpha valid at the FIRST OFFER", 0x77,
          r.alpha_at_first_offer);
    bool all_held = !r.alpha_during_tris.empty();
    for (int a : r.alpha_during_tris) all_held = all_held && (a == 0x77);
    check(all_held, "alpha held for every triple", 1, all_held ? 1 : 0);
  }

  // ---- 3. two hulls back to back keep their OWN alphas ---------------------
  // The metadata-swap check: hull B's strength must not reach hull A's
  // triangles, and A's must not survive into B's.
  {
    reset(d);
    const Run a = ring(d, 4, 0x11, 0x0A0A, 2, 0, 0);
    bool a_held = !a.alpha_during_tris.empty();
    for (int v : a.alpha_during_tris) a_held = a_held && (v == 0x11);
    check(a_held, "hull A keeps 0x11 through its triples", 1, a_held ? 1 : 0);
    check(a.tris.size() == 2 && a.tris[0].src == 0x0A0A, "hull A src id", 0x0A0A,
          a.tris.empty() ? -1 : a.tris[0].src);

    const Run b = ring(d, 4, 0x22, 0x0B0B, 2, 0, 0);
    bool b_held = !b.alpha_during_tris.empty();
    for (int v : b.alpha_during_tris) b_held = b_held && (v == 0x22);
    check(b_held, "hull B keeps 0x22 through its triples", 1, b_held ? 1 : 0);
    check(b.tris.size() == 2 && b.tris[0].src == 0x0B0B, "hull B src id", 0x0B0B,
          b.tris.empty() ? -1 : b.tris[0].src);
  }

  // ---- 4. POSITIVE CONTROL: short_ring_o FIRES -----------------------------
  // Two vertices cannot make a triangle. The assembler is already holding them
  // and waits in A_TRIS for a `t_last`, so returning nothing would deadlock it.
  {
    reset(d);
    const Run r = ring(d, 2, 0x33, 0x0C0C, 3, 0, 0);
    check_fan(r, 2, "short ring of 2");
    check(d.short_ring_o == 1, "short_ring_o FIRED", 1, d.short_ring_o);
    check(d.hulls_o == 0, "a short ring is NOT counted as a hull", 0, d.hulls_o);
    check(r.tris.size() == 1 && r.tris[0].last == 1, "the degenerate triple carries t_last", 1,
          r.tris.empty() ? -1 : r.tris[0].last);
    check(r.tris.size() == 1 && r.tris[0].i0 == 0 && r.tris[0].i1 == 0 && r.tris[0].i2 == 0,
          "the degenerate triple is (0,0,0) -- zero area, refused downstream", 1,
          r.tris.empty() ? -1 : 1);
    // And the block is live again afterwards, which is the point of retiring.
    const Run after = ring(d, 4, 0x44, 0x0D0D, 2, 0, 0);
    check_fan(after, 4, "ring after a short ring");
    check(d.hulls_o == 1, "the block retired and fanned the next ring", 1, d.hulls_o);
  }

  // ---- 5. POSITIVE CONTROL: ring_overflow_o FIRES --------------------------
  // MAXV is 16. A producer that offers 20 vertices without a `last` is
  // TRUNCATED at 16 and counted, rather than running past the store.
  {
    reset(d);
    const Run r = ring(d, 20, 0x55, 0x0E0E, 0, 0, 0, /*omit_last=*/true);
    check(d.ring_overflow_o == 1, "ring_overflow_o FIRED", 1, d.ring_overflow_o);
    check(static_cast<int>(r.verts.size()) == 16, "the ring was TRUNCATED at MAXV", 16,
          static_cast<long long>(r.verts.size()));
    check(static_cast<int>(r.tris.size()) == 14, "a truncated ring still fans", 14,
          static_cast<long long>(r.tris.size()));
    check(r.verts.size() == 16 && r.verts[15].last == 1, "v_last forced on vertex MAXV-1", 1,
          r.verts.size() == 16 ? r.verts[15].last : -1);
  }

  // ---- 6. backpressure on both streams -------------------------------------
  {
    reset(d);
    const Run r = ring(d, 16, 0x66, 0x0F0F, 0, 3, 5);
    check_fan(r, 16, "stalled ring of 16");
    check(d.hulls_o == 1, "stalled: hulls_o", 1, d.hulls_o);
    check(d.triangles_o == 14, "stalled: triangles_o", 14, d.triangles_o);
    check(d.short_ring_o == 0, "stalling is not a short ring", 0, d.short_ring_o);
    check(d.ring_overflow_o == 0, "stalling is not an overflow", 0, d.ring_overflow_o);
  }

  std::printf("forge_fanindex_directed: %d check(s), %d failure(s)\n", g_checks, g_failed);
  std::fflush(stdout);
  // TEARDOWN-DEADLOCK WORKAROUND, documented in tests/harness/zhao_sim.hpp:
  // a plain C++ return is exactly the shape that hangs on this toolchain.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
