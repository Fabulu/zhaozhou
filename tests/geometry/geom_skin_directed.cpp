// geom_skin_directed.cpp — GEOM.SKIN against `zref::creature::skin_vertex`.
//
// THE ORACLE IS THE SHIPPED FUNCTION. `skin_vertex` in
// reference/src/zcreature/creature_core.cpp is what every creature the
// reference renderer has ever drawn was skinned with. A mismatch here means the
// hardware puts vertices somewhere other than the pictures this project ships.
//
// The single-rounding law is what this file exists to defend. `pa` and `pb` are
// never rounded before the blend; the whole expression is rounded ONCE by 22.
// Rounding each skin separately and then blending is the natural way to write
// it and is wrong by an LSB on a large share of vertices — a difference that
// shows up as a shimmering silhouette rather than as an obvious break, which is
// precisely why it needs a differential rather than an eyeball.

#include "Vzhao_geom_skin.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

using zhao::check;
namespace zc = zref::creature;

struct Vtx {
  int32_t x, y, z;
  uint8_t w0;
  bool rigid;
};

void setMat(VlUnpacked<IData, 12>& dst, const zc::mat3x4fx& m) {
  for (int i = 0; i < 12; ++i) dst[i] = static_cast<uint32_t>(m.m[i]);
}

/** Drive one vertex and read the skinned result. */
void run(Vzhao_geom_skin& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v,
         int32_t& ox, int32_t& oy, int32_t& oz) {
  dut.v_valid_i = 1;
  dut.o_ready_i = 1;
  dut.v_x_i = static_cast<uint32_t>(v.x);
  dut.v_y_i = static_cast<uint32_t>(v.y);
  dut.v_z_i = static_cast<uint32_t>(v.z);
  dut.v_w0_i = v.w0;
  dut.v_rigid_i = v.rigid ? 1 : 0;
  dut.v_src_id_i = 0x1234;
  setMat(dut.a_m_i, A);
  setMat(dut.b_m_i, B);
  dut.eval();
  zhao::tick(dut);
  dut.v_valid_i = 0;
  dut.eval();
  ox = static_cast<int32_t>(dut.o_x_o);
  oy = static_cast<int32_t>(dut.o_y_o);
  oz = static_cast<int32_t>(dut.o_z_o);
}

void diff(Vzhao_geom_skin& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v,
          const char* what) {
  zc::SkinVertex sv;
  sv.x = v.x; sv.y = v.y; sv.z = v.z;
  sv.b0 = 0; sv.b1 = v.rigid ? 0 : 1;
  sv.w0 = v.w0;

  zc::mat3x4fx palette[2] = {A, B};
  int32_t wx = 0, wy = 0, wz = 0;
  zc::skin_vertex(palette, sv, wx, wy, wz, nullptr);

  int32_t gx = 0, gy = 0, gz = 0;
  run(dut, A, B, v, gx, gy, gz);

  const std::string t(what);
  check(gx == wx, (t + ": x").c_str(), static_cast<uint64_t>(static_cast<uint32_t>(wx)),
        static_cast<uint32_t>(gx));
  check(gy == wy, (t + ": y").c_str(), static_cast<uint64_t>(static_cast<uint32_t>(wy)),
        static_cast<uint32_t>(gy));
  check(gz == wz, (t + ": z").c_str(), static_cast<uint64_t>(static_cast<uint32_t>(wz)),
        static_cast<uint32_t>(gz));
}

zc::mat3x4fx mat(int32_t a, int32_t b, int32_t c, int32_t tx, int32_t d, int32_t e, int32_t f,
                   int32_t ty, int32_t g, int32_t h, int32_t i, int32_t tz) {
  return zc::mat3x4fx{{a, b, c, tx, d, e, f, ty, g, h, i, tz}};
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t x = s;
    s = x * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((x >> 22) ^ x) >> 29);
    const uint32_t v = (static_cast<uint32_t>(x >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_geom_skin dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.o_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  const int32_t ONE = 1 << 16;
  const zc::mat3x4fx I = zc::mat3x4_identity();

  // ---- random lane -------------------------------------------------------
  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x5C1Du);
    for (uint32_t k = 0; k < iters && zhao::check_failures() == 0; ++k) {
      zc::mat3x4fx A{}, B{};
      for (int i = 0; i < 12; ++i) {
        // Matrix elements in a plausible pose range rather than the full word:
        // a bone matrix is a rotation and a translation, not an arbitrary s32,
        // and sampling the whole word would spend every iteration on the
        // saturation rails instead of on the arithmetic.
        A.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 8 : 15);
        B.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 8 : 15);
      }
      Vtx v;
      v.x = static_cast<int32_t>(rng.next()) >> 8;
      v.y = static_cast<int32_t>(rng.next()) >> 8;
      v.z = static_cast<int32_t>(rng.next()) >> 8;
      v.w0 = static_cast<uint8_t>(rng.below(65));   // 0..64 inclusive: both rails
      v.rigid = (rng.below(4) == 0);
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u] w0=%u%s", k, v.w0, v.rigid ? " rigid" : "");
      diff(dut, A, B, v, tag);
    }
    dut.final();
    return zhao::report_and_exit("geom_skin_random");
  }

  // ---- 1. rigid, the identity: a vertex must not move --------------------
  diff(dut, I, I, Vtx{5 * ONE, -3 * ONE, 2 * ONE, 64, true}, "rigid identity");

  // ---- 2. rigid with a translation ---------------------------------------
  diff(dut, mat(ONE, 0, 0, 7 * ONE, 0, ONE, 0, -2 * ONE, 0, 0, ONE, ONE), I,
       Vtx{ONE, ONE, ONE, 64, true}, "rigid translate");

  // ---- 3. THE BRANCH BOUNDARIES ------------------------------------------
  // w0 == 64 takes the rigid path even when b1 != b0, and w0 == 0 is the far
  // rail where the vertex belongs entirely to B. Both are exact-equality
  // boundaries, which this project has learned six times over that uniform
  // random never lands on.
  {
    const zc::mat3x4fx A = mat(2 * ONE, 0, 0, 0, 0, 2 * ONE, 0, 0, 0, 0, 2 * ONE, 0);
    const zc::mat3x4fx B = mat(ONE, 0, 0, 100 * ONE, 0, ONE, 0, 0, 0, 0, ONE, 0);
    diff(dut, A, B, Vtx{ONE, ONE, ONE, 64, false}, "w0 == 64 (rigid path, b1 != b0)");
    diff(dut, A, B, Vtx{ONE, ONE, ONE, 0, false}, "w0 == 0 (entirely bone B)");
    diff(dut, A, B, Vtx{ONE, ONE, ONE, 32, false}, "w0 == 32 (the even split)");
    diff(dut, A, B, Vtx{ONE, ONE, ONE, 1, false}, "w0 == 1 (one quantum)");
    diff(dut, A, B, Vtx{ONE, ONE, ONE, 63, false}, "w0 == 63 (one quantum from rigid)");
  }

  // ---- 3b. THE SINGLE-ROUNDING WALL --------------------------------------
  // This is the section the file exists for, and the first draft did not have
  // it. Sections 1-4 all passed against an RTL mutated to round `pa` and `pb`
  // separately by 16 and then blend by 6 -- the exact double rounding qformats
  // A3b forbids. They passed because every matrix above is built from whole
  // multiples of ONE, so `pa` has no fraction below bit 16 and rescaling it
  // early throws nothing away. The mutation only showed up in the random lane,
  // at iteration 7, as a single LSB.
  //
  // A residue below bit 16 is therefore not a nice-to-have in this test, it is
  // the entire subject. These matrices carry 1 and 3 ULPs, so `pa = x` and
  // `pb = 3x` exactly, and every low bit of the vertex survives into the blend
  // where the two roundings can disagree.
  {
    const zc::mat3x4fx A = mat(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
    const zc::mat3x4fx B = mat(3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0);
    // Residues chosen around the half so the two rounding orders are most
    // likely to land on opposite sides of it.
    const int32_t kResidue[] = {1, 3, (1 << 15) - 1, 1 << 15, (1 << 15) + 1, (1 << 16) - 1,
                                (1 << 16) + (1 << 15), 12345, -12345, -(1 << 15), -(1 << 15) - 1};
    for (int32_t xv : kResidue) {
      // Every weight, not a sample of them: the disagreement between one
      // rounding and two depends on w0 through the 22-vs-16 shift, so a
      // sampled w0 can miss it exactly the way the first draft did.
      for (uint8_t w = 1; w < 64; ++w) {
        char tag[96];
        std::snprintf(tag, sizeof tag, "single-rounding wall x=%d w0=%u", xv, w);
        diff(dut, A, B, Vtx{xv, xv + 1, xv - 1, w, false}, tag);
      }
    }
  }

  // ---- 4. the rounding boundary ------------------------------------------
  // A product landing exactly on a half is where round-half-up and a plain
  // arithmetic shift disagree, and where a negative value separates
  // round-toward-+inf from round-toward-zero.
  {
    const zc::mat3x4fx A = mat(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
    diff(dut, A, A, Vtx{1 << 15, 1 << 15, 1 << 15, 64, true}, "rounding: exact +half");
    diff(dut, A, A, Vtx{-(1 << 15), -(1 << 15), -(1 << 15), 64, true}, "rounding: exact -half");
    diff(dut, A, A, Vtx{(1 << 15) + 1, -(1 << 15) - 1, 0, 64, true}, "rounding: either side");
  }

  // ---- 5. the saturation rails -------------------------------------------
  {
    const int32_t BIG = 0x4000'0000;
    const zc::mat3x4fx A = mat(BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG);
    const zc::mat3x4fx N = mat(-BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG,
                                 -BIG, -BIG);
    diff(dut, A, A, Vtx{BIG, BIG, BIG, 64, true}, "saturate: positive rail");
    diff(dut, N, N, Vtx{BIG, BIG, BIG, 64, true}, "saturate: negative rail");
    diff(dut, A, N, Vtx{BIG, BIG, BIG, 32, false}, "saturate: blend of both rails");
  }

  // ---- 6. the interface laws, which arithmetic cannot reach ---------------
  // A mutation that zeroes the `src_id` passthrough survived all 2118 checks
  // above, because every one of them compares coordinates and nothing else.
  // The tag is what tells the stage downstream WHICH vertex it just received;
  // dropping it silently reassigns every vertex to source 0, which is a far
  // worse failure than an LSB and was completely uncovered.
  {
    const zc::mat3x4fx A = zc::mat3x4_identity();
    const uint32_t before = dut.vertices_transformed_o;

    dut.v_valid_i = 1;
    dut.o_ready_i = 1;
    dut.v_x_i = ONE; dut.v_y_i = ONE; dut.v_z_i = ONE;
    dut.v_w0_i = 64; dut.v_rigid_i = 1; dut.v_src_id_i = 0xBEEF;
    setMat(dut.a_m_i, A);
    setMat(dut.b_m_i, A);
    dut.eval();
    zhao::tick(dut);
    dut.v_valid_i = 0;
    dut.eval();

    check(dut.o_src_id_o == 0xBEEF, "src_id rides through with its vertex", 0xBEEF,
          dut.o_src_id_o);
    check(dut.o_valid_o == 1, "an accepted vertex raises o_valid", 1, dut.o_valid_o);
    check(dut.vertices_transformed_o == before + 1, "an accepted vertex advances the counter by one",
          before + 1, dut.vertices_transformed_o);

    // A vertex OFFERED but not accepted must not be counted. This is the whole
    // reason the counter sits inside the `take` branch rather than beside
    // v_valid_i, and it is the difference between a telemetry number that means
    // "work done" and one that means "work attempted".
    const uint32_t held = dut.vertices_transformed_o;
    dut.v_valid_i = 1;
    dut.o_ready_i = 0;          // downstream stalls: nothing may be accepted
    dut.v_src_id_i = 0x0BAD;
    dut.eval();
    check(dut.v_ready_o == 0, "a full output register refuses a new vertex", 0, dut.v_ready_o);
    zhao::tick(dut);
    dut.eval();
    check(dut.vertices_transformed_o == held, "a stalled vertex is not counted as skinned", held,
          dut.vertices_transformed_o);
    check(dut.o_src_id_o == 0xBEEF, "a stalled vertex does not overwrite the held result", 0xBEEF,
          dut.o_src_id_o);

    dut.v_valid_i = 0;
    dut.o_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
    check(dut.o_valid_o == 0, "o_valid drops once the result is taken", 0, dut.o_valid_o);
  }

  dut.final();
  return zhao::report_and_exit("geom_skin_directed");
}
