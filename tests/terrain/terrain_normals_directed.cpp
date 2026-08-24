// terrain_normals_directed.cpp — TERRAIN.NORMALS against its oracle.
//
// THE ORACLE IS THE RATIFIED LAW, not a second opinion.
// `zref::terrain::face_normal` is a thin view onto `shade_flat_tri`'s cross
// product in reference/src/zrender/terrain.cpp, which the golden captures
// already pin. So a mismatch here means the RTL disagrees with the shading the
// project already ships, not with a model written to match it.
//
// The cases below are chosen around the two things this arithmetic gets wrong
// when it is wrong: the Q-format shift, and the rounding at the half.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_normals.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_normals.hpp"

using zhao::check;

namespace {

// zhao::reset() assumes the byte-stream ports (in_valid/in_data) that the
// harness's first consumers had; this block has neither. Local reset, same
// async-negedge semantics.
template <typename Top>
void reset_dut(Top& top) {
  top.rst_n = 0;
  top.tri_valid_i = 0;
  top.nrm_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

constexpr int32_t kOne = 1 << 16;  // 1.0 in fx16

struct Tri {
  zref::terrain::NormalVertex a, b, c;
};

/** Drive one triangle through and return what the RTL said. */
zref::terrain::FaceNormal drive(Vzhao_terrain_normals& dut, const Tri& t, uint16_t src) {
  dut.tri_valid_i = 1;
  dut.ax_i = t.a.x;
  dut.ay_i = t.a.y;
  dut.az_i = t.a.z;
  dut.bx_i = t.b.x;
  dut.by_i = t.b.y;
  dut.bz_i = t.b.z;
  dut.cx_i = t.c.x;
  dut.cy_i = t.c.y;
  dut.cz_i = t.c.z;
  dut.src_id_i = src;
  dut.nrm_ready_i = 1;

  zref::terrain::FaceNormal got;
  bool seen = false;
  // Fixed latency 2, but drain generously so a latency change shows up as a
  // missing result rather than a wrong one.
  for (int cycle = 0; cycle < 16 && !seen; ++cycle) {
    zhao::tick(dut);
    if (dut.nrm_valid_o) {
      got.x = dut.nx_o;
      got.y = dut.ny_o;
      got.z = dut.nz_o;
      got.degenerate = dut.degenerate_o != 0;
      seen = true;
      // src_id must ride with its own result, not with whatever is current.
      check(dut.src_id_o == src, "src_id rides its own normal", src, dut.src_id_o);
    }
    dut.tri_valid_i = 0;
    // POISON THE INPUTS. In ready/valid the producer is free to change its
    // data the cycle after `valid && ready`, and this block now spends six more
    // cycles walking one shared multiplier over the LATCHED edges. Held
    // stimulus made a live read of `e1y` indistinguishable from the latched
    // `l1y`, and mutant M09 survived the sweep on exactly that.
    dut.ax_i = 0x0BAD0000;
    dut.ay_i = 0x0BAD0001;
    dut.az_i = 0x0BAD0002;
    dut.bx_i = 0x0BAD0003;
    dut.by_i = 0x0BAD0004;
    dut.bz_i = 0x0BAD0005;
    dut.cx_i = 0x0BAD0006;
    dut.cy_i = 0x0BAD0007;
    dut.cz_i = 0x0BAD0008;
    dut.src_id_i = 0xBAD5;
  }
  check(seen, "a triangle produces a normal", 1, seen ? 1 : 0);
  return got;
}

void expect(Vzhao_terrain_normals& dut, const Tri& t, const char* what, uint16_t src) {
  const zref::terrain::FaceNormal want = zref::terrain::face_normal(t.a, t.b, t.c);
  const zref::terrain::FaceNormal got = drive(dut, t, src);
  check(got.x == want.x && got.y == want.y && got.z == want.z, what, want.x, got.x);
  check(got.degenerate == want.degenerate, what, want.degenerate ? 1 : 0, got.degenerate ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_normals dut;
  reset_dut(dut);
  dut.nrm_ready_i = 1;

  uint16_t src = 1;

  // ---- 1. flat ground, both windings ---------------------------------------
  // A flat cell in the XZ plane must give a normal along Y and nothing else.
  // y-up winding: e1 x e2 = +Y for a flat cell (terrain.cpp's own note).
  const Tri flat_up{{0, 0, 0}, {kOne, 0, 0}, {0, 0, kOne}};
  expect(dut, flat_up, "flat cell, y-up winding", src++);
  {
    const zref::terrain::FaceNormal n = zref::terrain::face_normal(flat_up.a, flat_up.b, flat_up.c);
    check(n.x == 0 && n.z == 0 && n.y != 0, "a flat cell's normal is pure Y", 0, n.x);
    check(!n.degenerate, "a flat cell is not degenerate", 0, n.degenerate ? 1 : 0);
  }
  // Reversed winding must flip the sign, not merely change magnitude.
  const Tri flat_dn{{0, 0, 0}, {0, 0, kOne}, {kOne, 0, 0}};
  expect(dut, flat_dn, "flat cell, reversed winding", src++);
  {
    const zref::terrain::FaceNormal u = zref::terrain::face_normal(flat_up.a, flat_up.b, flat_up.c);
    const zref::terrain::FaceNormal d = zref::terrain::face_normal(flat_dn.a, flat_dn.b, flat_dn.c);
    check(d.y == -u.y, "reversing the winding negates the normal", -u.y, d.y);
  }

  // ---- 2. a crest: the cell is tilted, so X or Z must appear ---------------
  const Tri crest{{0, 0, 0}, {kOne, kOne / 2, 0}, {0, 0, kOne}};
  expect(dut, crest, "tilted cell across X", src++);
  {
    const zref::terrain::FaceNormal n = zref::terrain::face_normal(crest.a, crest.b, crest.c);
    check(n.x != 0, "a cell rising along X tilts its normal off Y", 1, n.x != 0 ? 1 : 0);
  }

  // ---- 3. degeneracy, exact and by rounding -------------------------------
  // Collinear: zero area exactly.
  expect(dut, Tri{{0, 0, 0}, {kOne, 0, 0}, {2 * kOne, 0, 0}}, "collinear is degenerate", src++);
  // All three vertices identical.
  expect(dut, Tri{{5, 6, 7}, {5, 6, 7}, {5, 6, 7}}, "a collapsed cell is degenerate", src++);

  // A cross product that is NONZERO yet rescales to zero -- the case the
  // contract deliberately calls degenerate ("a cell whose exact cross product
  // is nonzero but rounds to zero counts as degenerate there too"). Raw fx16
  // unit edges give cross = (1, 0, 0): rescale16(1) is (1 + 32768) >> 16 = 0,
  // so every lane rounds to zero and the cell IS degenerate -- while the
  // pre-rescale lane is 1. Mutant M10 judged degeneracy on that pre-rescale
  // lane and survived, because nothing here had ever been small enough to tell
  // the two apart.
  expect(dut, Tri{{0, 0, 0}, {0, 1, 0}, {0, 0, 1}},
         "a cross product that rounds to zero is degenerate", src++);
  // THE SUB-METRE CASE. This is the regime the rescale-by-32 defect destroyed:
  // a tiny near-flat cell whose exact cross product is nonzero. With the shift
  // at 16 it survives; at 32 every lane rounds to zero and the cell reads as
  // degenerate. The assertion below is what makes that defect visible here.
  const Tri tiny{{0, 0, 0}, {kOne / 32, 0, 0}, {0, 0, kOne / 32}};
  expect(dut, tiny, "a sub-metre cell survives the rescale", src++);
  {
    const zref::terrain::FaceNormal n = zref::terrain::face_normal(tiny.a, tiny.b, tiny.c);
    check(!n.degenerate && n.y != 0,
          "a 1/32-unit cell is NOT degenerate (the rescale-32 defect made it so)", 1,
          (!n.degenerate && n.y != 0) ? 1 : 0);
  }

  // ---- 4. the rounding boundary -------------------------------------------
  // Round-half-up is only distinguishable from truncation when the discarded
  // bits are exactly one half, and only distinguishable in SIGN handling when
  // the value is negative. Both are exercised: a lane of exactly +2^15 must
  // round to 1, and exactly -2^15 must round to 0 (not -1), because
  // (-2^15 + 2^15) >> 16 == 0.
  {
    zref::terrain::NormalVertex a{0, 0, 0};
    // Choose edges so one lane is exactly 2^15: e1x*e2y - e1y*e2x with
    // e1x = 2^15, e2y = 1, and the other term zero.
    zref::terrain::NormalVertex b{1 << 15, 0, 0};
    zref::terrain::NormalVertex c{0, 1, 0};
    const zref::terrain::FaceNormal n = zref::terrain::face_normal(a, b, c);
    check(n.z == 1, "a lane of exactly +2^15 rounds half UP to 1", 1, n.z);
    expect(dut, Tri{a, b, c}, "positive half-way lane", src++);

    zref::terrain::NormalVertex bn{-(1 << 15), 0, 0};
    const zref::terrain::FaceNormal m = zref::terrain::face_normal(a, bn, c);
    check(m.z == 0, "a lane of exactly -2^15 rounds half up to 0, not -1", 0, m.z);
    expect(dut, Tri{a, bn, c}, "negative half-way lane", src++);
  }

  // ---- 5. the far edge of the legal domain --------------------------------
  // THE DOMAIN, and why it is not the whole word. A lane is a difference of two
  // products of two edge components. Inside qformats §8's +-2048 world-unit
  // guard band a coordinate is 2^27, an edge 2^28, and a lane needs 58 bits, so
  // the reference's int64 carries it exactly. At the FULL int32 word a lane
  // needs 66 bits and the REFERENCE ITSELF overflows, while this block's 67-bit
  // lanes do not. That is a divergence in the block's favour, on inputs no
  // legal lattice can produce, and it is recorded in the contract rather than
  // papered over by narrowing the RTL to wrap the same way.
  //
  // So the domain is +-4096 world units (2^28 raw), double the guard band,
  // where a lane needs 60 bits and both agree exactly. The OUTPUT still
  // saturates in here (a 2^59 lane rescales to 2^43, far past INT32_MAX), so
  // the fx16 rails are still exercised.
  {
    const int32_t lim = 4096 << 16;
    expect(dut, Tri{{0, 0, 0}, {lim, lim, 0}, {0, lim, lim}}, "domain-limit positive", src++);
    expect(dut, Tri{{0, 0, 0}, {-lim, -lim, 0}, {0, -lim, -lim}}, "domain-limit negative", src++);
    expect(dut, Tri{{-lim, 0, lim}, {lim, -lim, 0}, {0, lim, -lim}}, "domain-limit mixed", src++);
    const zref::terrain::FaceNormal n =
        zref::terrain::face_normal({0, 0, 0}, {lim, lim, 0}, {0, lim, lim});
    check(n.x == INT32_MAX || n.y == INT32_MAX || n.z == INT32_MAX || n.x == INT32_MIN ||
              n.y == INT32_MIN || n.z == INT32_MIN,
          "a domain-limit cell still reaches an fx16 rail", 1, 1);
  }

  // ---- 6. backpressure ----------------------------------------------------
  // A stalled consumer must not lose or corrupt a result.
  {
    const Tri t{{0, 0, 0}, {kOne, kOne / 4, 0}, {0, 0, kOne}};
    const zref::terrain::FaceNormal want = zref::terrain::face_normal(t.a, t.b, t.c);
    reset_dut(dut);
    dut.nrm_ready_i = 0;  // consumer stalled from the outset
    dut.tri_valid_i = 1;
    dut.ax_i = t.a.x;
    dut.ay_i = t.a.y;
    dut.az_i = t.a.z;
    dut.bx_i = t.b.x;
    dut.by_i = t.b.y;
    dut.bz_i = t.b.z;
    dut.cx_i = t.c.x;
    dut.cy_i = t.c.y;
    dut.cz_i = t.c.z;
    dut.src_id_i = 0xBEEF;
    zhao::tick(dut);
    dut.tri_valid_i = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(dut);
    check(dut.nrm_valid_o == 1, "a stalled result waits at the output", 1, dut.nrm_valid_o);
    check(dut.nx_o == want.x && dut.ny_o == want.y && dut.nz_o == want.z,
          "a stalled result is not corrupted while it waits", want.y, dut.ny_o);
    dut.nrm_ready_i = 1;
    zhao::tick(dut);
    check(dut.nrm_valid_o == 0, "the result retires exactly once", 0, dut.nrm_valid_o);
  }

  // ---- 7. the counter counts results, not cycles --------------------------
  {
    reset_dut(dut);
    dut.nrm_ready_i = 1;
    const uint32_t before = dut.terrain_samples_evaluated_o;
    const Tri t{{0, 0, 0}, {kOne, 0, 0}, {0, 0, kOne}};
    for (int i = 0; i < 5; ++i) drive(dut, t, static_cast<uint16_t>(0x100 + i));
    check(dut.terrain_samples_evaluated_o == before + 5,
          "terrain_samples_evaluated counts evaluated triangles", before + 5,
          dut.terrain_samples_evaluated_o);
    // drive() returns as soon as it SEES a result, which is the cycle before
    // that result retires. Give the pipe its retire cycle before asking.
    dut.tri_valid_i = 0;
    dut.nrm_ready_i = 1;
    for (int i = 0; i < 4; ++i) zhao::tick(dut);
    check(dut.idle_o == 1, "the block reports idle when drained", 1, dut.idle_o);
  }

  return zhao::report_and_exit("terrain_normals_directed");
}
