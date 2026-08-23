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
//
// ---------------------------------------------------------------------------
// WHAT CHANGED WHEN THE BLOCK WAS SEQUENCED (2026-08-23)
// ---------------------------------------------------------------------------
// `zhao_geom_skin` was one clock and 72 DSP blocks. It is now a MUL_LANES-wide
// multiplier farm sized to the owner's ~120,000 skinned vertices per 60 Hz
// frame, so the result arrives ten clocks after accept rather than one. Three
// things follow, and all three are new tests rather than new comments:
//
//   * the driver must complete a real ready/valid handshake instead of
//     assuming one tick (`run`);
//   * **the RATE is now the justification for the DSP count, so the rate is
//     pinned as a law** (section 8). A block whose multiplier budget is argued
//     from a frame budget must have that frame budget in the test, or the
//     argument decays into a comment the moment somebody edits the schedule;
//   * the OPERAND EXTREMES had to be reached before the narrowed 65/66/73-bit
//     lanes could be trusted (section 7). `design/contracts/GEOM.SKIN.md`
//     recorded this as the gap that gates the rewrite. It is closed here.
//
// ENFORCED-BY, referenced from the RTL header: `require_legal_w0` below is the
// check that makes `w0 <= 64` a fact about this differential rather than an
// assumption inside it. The `(pb << 6) + w0*(pa - pb)` identity the RTL now
// uses is exact on the legal domain and false outside it, so every comparison
// this file makes is knowingly a statement about the legal domain.

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

// THE FRONTIER IS BUILT THREE TIMES, NOT ARGUED ONCE.
//
// `ZHAO_SKIN_LANES` matches the `MUL_LANES` this executable's model was
// elaborated with; tests/CMakeLists.txt builds this same source against 1, 3
// and 6. That is the difference between a parameter whose other settings are
// *claimed* to work and one whose other settings are DIFFERENTIALLY VERIFIED —
// and it is not cosmetic here, because MUL_LANES = 3 collapses the term-walk
// (TSTEPS == 1) and leaves the entire multi-cycle accumulate path unreached.
// Only the MUL_LANES = 1 build exercises it.
#ifndef ZHAO_SKIN_LANES
#define ZHAO_SKIN_LANES 3
#endif

// The frontier table from the RTL header, as numbers this file will fail on.
#if ZHAO_SKIN_LANES == 1
constexpr int kLatencyBlend = 22;
constexpr int kLatencyRigid = 13;
constexpr bool kMeetsDemand = false;  // 75,757 vertices/frame: BELOW the demand
#elif ZHAO_SKIN_LANES == 3
constexpr int kLatencyBlend = 10;
constexpr int kLatencyRigid = 7;
constexpr bool kMeetsDemand = true;  // 166,666 vertices/frame, 1.39x
#elif ZHAO_SKIN_LANES == 6
constexpr int kLatencyBlend = 8;
constexpr int kLatencyRigid = 7;
constexpr bool kMeetsDemand = true;  // 208,333 vertices/frame, 1.74x
#else
#error "ZHAO_SKIN_LANES must be 1, 3 or 6 -- see zhao_geom_skin.sv's frontier table"
#endif

// 100 MHz gpu_clk / 60 Hz, and the owner's sustained demand.
constexpr long kClocksPerFrame = 1666666;
constexpr long kVertexDemand = 120000;

struct Vtx {
  int32_t x, y, z;
  uint8_t w0;
  bool rigid;
};

/** ENFORCED-BY for the RTL's weight identity: nothing may present w0 > 64.
 *
 * The identity `w0*pa + (64-w0)*pb == (pb << 6) + w0*(pa - pb)` is exact for
 * w0 <= 64 and FALSE above it, where the old form's 7-bit `64 - w0` wrapped to
 * `192 - w0`. `w0 > 64` is out of contract, so there is no right answer to
 * agree on — but "out of contract" has to be a checked property of the
 * stimulus, not a belief about it. */
void require_legal_w0(uint32_t w0) {
  if (w0 > 64) {
    check(false, "ENFORCED-BY: driver presented w0 > 64, outside GEOM.SKIN's contract", 64, w0);
  }
}

void setMat(VlUnpacked<IData, 12>& dst, const zc::mat3x4fx& m) {
  for (int i = 0; i < 12; ++i) dst[i] = static_cast<uint32_t>(m.m[i]);
}

void present(Vzhao_geom_skin& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v,
             uint16_t src_id) {
  dut.v_x_i = static_cast<uint32_t>(v.x);
  dut.v_y_i = static_cast<uint32_t>(v.y);
  dut.v_z_i = static_cast<uint32_t>(v.z);
  dut.v_w0_i = v.w0;
  dut.v_rigid_i = v.rigid ? 1 : 0;
  dut.v_src_id_i = src_id;
  setMat(dut.a_m_i, A);
  setMat(dut.b_m_i, B);
}

/** Drive one vertex through the full handshake and read the skinned result.
 *
 * Returns the accept-to-o_valid latency in clocks, which section 8 checks and
 * every other caller ignores. The guards are hang detectors, not backpressure
 * tests: a sequenced engine that never raises `o_valid_o` would otherwise spin
 * this loop forever and look like a build that hung rather than a block that
 * is wrong. */
int run(Vzhao_geom_skin& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v,
        int32_t& ox, int32_t& oy, int32_t& oz) {
  require_legal_w0(v.w0);

  dut.o_ready_i = 1;
  dut.v_valid_i = 1;
  present(dut, A, B, v, 0x1234);
  dut.eval();

  int waited = 0;
  while (!dut.v_ready_o) {
    zhao::tick(dut);
    dut.eval();
    if (++waited > 128) {
      check(false, "engine never accepted the vertex", 1, 0);
      return -1;
    }
  }
  zhao::tick(dut);  // the rising edge that accepts it
  dut.v_valid_i = 0;
  dut.eval();

  int latency = 1;
  while (!dut.o_valid_o) {
    zhao::tick(dut);
    dut.eval();
    if (++latency > 128) {
      check(false, "engine never produced a result", 1, 0);
      return -1;
    }
  }

  ox = static_cast<int32_t>(dut.o_x_o);
  oy = static_cast<int32_t>(dut.o_y_o);
  oz = static_cast<int32_t>(dut.o_z_o);

  zhao::tick(dut);  // o_ready_i is high: the result is taken, the block idles
  dut.eval();
  return latency;
}

// ---------------------------------------------------------------------------
// THE ORACLE'S OWN INT64 NARROWING, found 2026-08-23 by section 7
// ---------------------------------------------------------------------------
// `zref::rescale_s32` takes an **int64_t**:
//
//     constexpr int32_t rescale_s32(int64_t x, int k, SatLedger*, ...)
//
// and `skin_vertex` hands it a `__int128`:
//
//     rescale_s32(v.w0 * pa + w1 * pb, 22, L, &SatLedger::mul);
//
// so the argument is silently truncated modulo 2^64 whenever it does not fit
// int64. `rescale_s32`'s own comment ("the rounding add runs in s128: x near
// INT64_MAX must not wrap before the shift") shows the narrowing is not
// intended -- it was written for s64 inputs, and a `rescale_s64(__int128, ...)`
// exists two functions below it.
//
// The effect is not subtle. At `fullrange[599]` the blend is 64*pb =
// 1.0627e20, whose exact rescale saturates to +0x7FFFFFFF; narrowed to int64 it
// wraps negative and saturates to 0x80000000 instead. A sign flip at the rail.
//
// **THIS PREDATES THE REARCHITECTURE.** The old RTL carried 67- and 75-bit
// lanes and did not truncate either, so it diverged from the oracle in exactly
// the same places. Nothing caught it because the differential had never driven
// an operand that large -- which is precisely the gap `GEOM.SKIN.md` recorded
// as blocking this rewrite.
//
// What this file does about it, and what it deliberately does NOT do:
//
//   * it does not change the reference. `skin_vertex` is the function every
//     shipped picture was skinned with, and changing its arithmetic is an
//     owner decision, not a test's;
//   * it does not teach the RTL to imitate a C++ narrowing. Baking an accident
//     into silicon needs a ruling, not a commit;
//   * it CHECKS THE SHIPPED ORACLE wherever the oracle is well defined -- which
//     is everywhere the old differential reached and everywhere real bone
//     matrices live -- and above the narrowing boundary it checks the exact
//     arithmetic the RTL claims instead, and says so in the check name.
//
// `skin_exact` below is that arithmetic. It is NOT a second oracle written
// beside the RTL: it is `skin_vertex` with the implicit narrowing removed, and
// every in-domain case in this file asserts that it agrees with the shipped
// oracle bit for bit. It earns the right to carry the out-of-domain cases by
// tracking the real one across several thousand in-domain checks first.
namespace {

int32_t rescale_exact(__int128 v, int k) {
  const __int128 r = (v + (static_cast<__int128>(1) << (k - 1))) >> k;
  if (r > INT32_MAX) return INT32_MAX;
  if (r < INT32_MIN) return INT32_MIN;
  return static_cast<int32_t>(r);
}

bool fits_s64(__int128 v) {
  return v >= -(static_cast<__int128>(1) << 63) && v < (static_cast<__int128>(1) << 63);
}

/** `skin_vertex`'s arithmetic without the int64 narrowing, plus a per-row flag
 *  saying whether the shipped oracle's argument fitted int64 (and so whether
 *  the two are required to agree on that row). */
void skin_exact(const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v, int32_t out[3],
                bool in_domain[3]) {
  const bool rigid = v.rigid || v.w0 == 64;
  const int32_t w1 = 64 - static_cast<int32_t>(v.w0);
  for (int i = 0; i < 3; ++i) {
    const int r = i * 4;
    const __int128 pa =
        static_cast<__int128>(A.m[r]) * v.x + static_cast<__int128>(A.m[r + 1]) * v.y +
        static_cast<__int128>(A.m[r + 2]) * v.z + (static_cast<__int128>(A.m[r + 3]) << 16);
    if (rigid) {
      out[i] = rescale_exact(pa, 16);
      in_domain[i] = fits_s64(pa);
      continue;
    }
    const __int128 pb =
        static_cast<__int128>(B.m[r]) * v.x + static_cast<__int128>(B.m[r + 1]) * v.y +
        static_cast<__int128>(B.m[r + 2]) * v.z + (static_cast<__int128>(B.m[r + 3]) << 16);
    const __int128 blend = static_cast<__int128>(v.w0) * pa + static_cast<__int128>(w1) * pb;
    out[i] = rescale_exact(blend, 22);
    in_domain[i] = fits_s64(blend);
  }
}

// Visible tally, printed at the end: a silent domain split is a way to lose a
// differential without noticing.
long g_oracle_checked = 0;
long g_beyond_oracle = 0;

}  // namespace

void diff(Vzhao_geom_skin& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, const Vtx& v,
          const char* what) {
  zc::SkinVertex sv;
  sv.x = v.x;
  sv.y = v.y;
  sv.z = v.z;
  sv.b0 = 0;
  sv.b1 = v.rigid ? 0 : 1;
  sv.w0 = v.w0;

  zc::mat3x4fx palette[2] = {A, B};
  int32_t wx = 0, wy = 0, wz = 0;
  zc::skin_vertex(palette, sv, wx, wy, wz, nullptr);
  const int32_t oracle[3] = {wx, wy, wz};

  int32_t exact[3] = {0, 0, 0};
  bool in_domain[3] = {false, false, false};
  skin_exact(A, B, v, exact, in_domain);

  int32_t g[3] = {0, 0, 0};
  run(dut, A, B, v, g[0], g[1], g[2]);

  static const char kAxis[3] = {'x', 'y', 'z'};
  const std::string t(what);
  for (int i = 0; i < 3; ++i) {
    const std::string axis = t + ": " + kAxis[i];
    if (in_domain[i]) {
      // THE SHIPPED ORACLE IS THE LAW HERE.
      check(g[i] == oracle[i], axis.c_str(),
            static_cast<uint64_t>(static_cast<uint32_t>(oracle[i])), static_cast<uint32_t>(g[i]));
      // And this is what earns `skin_exact` the right to carry the rest.
      check(exact[i] == oracle[i], (axis + " [exact model tracks the shipped oracle]").c_str(),
            static_cast<uint64_t>(static_cast<uint32_t>(oracle[i])),
            static_cast<uint32_t>(exact[i]));
      ++g_oracle_checked;
    } else {
      check(g[i] == exact[i], (axis + " [beyond the oracle's int64 narrowing]").c_str(),
            static_cast<uint64_t>(static_cast<uint32_t>(exact[i])), static_cast<uint32_t>(g[i]));
      ++g_beyond_oracle;
    }
  }
}

zc::mat3x4fx mat(int32_t a, int32_t b, int32_t c, int32_t tx, int32_t d, int32_t e, int32_t f,
                 int32_t ty, int32_t g, int32_t h, int32_t i, int32_t tz) {
  return zc::mat3x4fx{{a, b, c, tx, d, e, f, ty, g, h, i, tz}};
}

/** Every element the same value — the shape that drives `pa` to its bound. */
zc::mat3x4fx uniform_mat(int32_t v) {
  zc::mat3x4fx m{};
  for (int i = 0; i < 12; ++i) m.m[i] = v;
  return m;
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
        // saturation rails instead of on the arithmetic. The rails have their
        // own directed lane (section 5) and their own full-range lane
        // (section 7b) precisely because this one is aimed away from them.
        A.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 8 : 15);
        B.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 8 : 15);
      }
      Vtx v;
      v.x = static_cast<int32_t>(rng.next()) >> 8;
      v.y = static_cast<int32_t>(rng.next()) >> 8;
      v.z = static_cast<int32_t>(rng.next()) >> 8;
      v.w0 = static_cast<uint8_t>(rng.below(65));  // 0..64 inclusive: both rails
      v.rigid = (rng.below(4) == 0);
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u] w0=%u%s", k, v.w0, v.rigid ? " rigid" : "");
      diff(dut, A, B, v, tag);
    }
    dut.final();
    std::printf(
        "[geom_skin] oracle-checked coordinates: %ld, beyond the oracle's "
        "int64 narrowing: %ld\n",
        g_oracle_checked, g_beyond_oracle);
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
    const int32_t kResidue[] = {1,
                                3,
                                (1 << 15) - 1,
                                1 << 15,
                                (1 << 15) + 1,
                                (1 << 16) - 1,
                                (1 << 16) + (1 << 15),
                                12345,
                                -12345,
                                -(1 << 15),
                                -(1 << 15) - 1};
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
    const zc::mat3x4fx N =
        mat(-BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG);
    diff(dut, A, A, Vtx{BIG, BIG, BIG, 64, true}, "saturate: positive rail");
    diff(dut, N, N, Vtx{BIG, BIG, BIG, 64, true}, "saturate: negative rail");
    diff(dut, A, N, Vtx{BIG, BIG, BIG, 32, false}, "saturate: blend of both rails");
  }

  // ---- 6. the interface laws, which arithmetic cannot reach ---------------
  // A mutation that zeroes the `src_id` passthrough survived all 2118 checks
  // of the arithmetic sections, because every one of them compares coordinates
  // and nothing else. The tag is what tells the stage downstream WHICH vertex
  // it just received; dropping it silently reassigns every vertex to source 0,
  // which is a far worse failure than an LSB and was completely uncovered.
  {
    const zc::mat3x4fx A = zc::mat3x4_identity();
    const uint32_t before = dut.vertices_transformed_o;

    // o_ready is held LOW so the finished result stays in the register and the
    // "full output register" laws below have something to be full of.
    dut.o_ready_i = 0;
    dut.v_valid_i = 1;
    present(dut, A, A, Vtx{ONE, ONE, ONE, 64, true}, 0xBEEF);
    dut.eval();
    check(dut.v_ready_o == 1, "an idle engine offers ready", 1, dut.v_ready_o);
    zhao::tick(dut);
    dut.v_valid_i = 0;
    dut.eval();

    check(dut.vertices_transformed_o == before + 1,
          "an accepted vertex advances the counter by one", before + 1, dut.vertices_transformed_o);

    // ---- the law the sequenced engine introduced ------------------------
    // A busy engine must refuse a second vertex. With one accumulator bank
    // there is nowhere to put it, and a `v_ready_o` that ignored `busy` would
    // silently overwrite the vertex in flight with the next one -- producing a
    // plausible skinned vertex belonging to neither.
    int busy_cycles = 0;
    while (!dut.o_valid_o) {
      check(dut.v_ready_o == 0, "a busy engine refuses a new vertex", 0, dut.v_ready_o);
      zhao::tick(dut);
      dut.eval();
      if (++busy_cycles > 128) {
        check(false, "engine never produced a result", 1, 0);
        break;
      }
    }

    check(dut.o_src_id_o == 0xBEEF, "src_id rides through with its vertex", 0xBEEF, dut.o_src_id_o);
    check(dut.o_valid_o == 1, "a finished vertex raises o_valid", 1, dut.o_valid_o);

    // A vertex OFFERED but not accepted must not be counted. This is the whole
    // reason the counter sits inside the `take` branch rather than beside
    // v_valid_i, and it is the difference between a telemetry number that means
    // "work done" and one that means "work attempted".
    const uint32_t held = dut.vertices_transformed_o;
    dut.v_valid_i = 1;
    dut.o_ready_i = 0;  // downstream stalls: nothing may be accepted
    present(dut, A, A, Vtx{ONE, ONE, ONE, 64, true}, 0x0BAD);
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

  // ---- 7. THE ROW-PRODUCT RAILS ------------------------------------------
  // `design/contracts/GEOM.SKIN.md` recorded this as the gap that GATES the
  // weight-identity rewrite, and it was right to: the random lane shifts
  // matrices down 15 bits and vertices 8, so `pa` reaches about 2^43 against a
  // lane that now holds only what the proof requires.
  //
  // The sequenced RTL narrowed the accumulator from 67 bits to 65, the
  // difference to 66 and the blend lane from 75 to 73, each proven from the s32
  // input widths (|pa| < 2^64, |pa-pb| < 2^65, |blend| < 2^72). QUARTUS_GOTCHAS
  // §5 is the reason to narrow and this section is the reason it is safe to:
  // a proof in a comment is not a check, and the difference between 65 and 64
  // bits is invisible until an operand actually reaches the bound.
  //
  // A truncated lane does not produce a slightly wrong answer here. It WRAPS,
  // so a value that should saturate to +2^31-1 saturates to -2^31 instead --
  // which is why these cases are worth running even though most of them land
  // on a rail.
  {
    const int32_t MINI = INT32_MIN;
    const int32_t MAXI = INT32_MAX;
    const int32_t kRail[] = {MINI, MINI + 1, -0x4000'0000, -1, 0, 1, 0x4000'0000, MAXI - 1, MAXI};
    const uint8_t kW[] = {0, 1, 2, 32, 62, 63};

    for (int32_t am : kRail) {
      // B is the NEGATIVE of A wherever that is representable, so `pa - pb`
      // reaches twice |pa| -- the bound the 66-bit difference lane was sized
      // for, and the one nothing previously drove.
      const int32_t bm = (am == MINI) ? MAXI : -am;
      const zc::mat3x4fx A = uniform_mat(am);
      const zc::mat3x4fx B = uniform_mat(bm);
      for (int32_t xv : kRail) {
        char tag[128];
        std::snprintf(tag, sizeof tag, "rail rigid m=%d x=%d", am, xv);
        diff(dut, A, B, Vtx{xv, xv, xv, 64, true}, tag);
        for (uint8_t w : kW) {
          std::snprintf(tag, sizeof tag, "rail blend m=%d x=%d w0=%u", am, xv, w);
          diff(dut, A, B, Vtx{xv, xv, xv, w, false}, tag);
        }
      }
    }

    // The NEAR-CANCELLATION family, which is the sensitive one. With B = -A the
    // blend at w0 == 32 is `(pb << 6) + 32*(pa - pb) == 32*(pa + pb)`, so the
    // final answer is SMALL while every intermediate is at its bound. A lane
    // one bit too narrow gives a large wrong answer here instead of the same
    // saturated rail it would give anywhere else -- this is the case that can
    // tell a correct width from a lucky one.
    for (int32_t am : kRail) {
      const int32_t bm = (am == MINI) ? MAXI : -am;
      const zc::mat3x4fx A = uniform_mat(am);
      const zc::mat3x4fx B = uniform_mat(bm);
      for (int32_t d : {0, 1, -1, 65535, -65535}) {
        char tag[128];
        std::snprintf(tag, sizeof tag, "cancellation m=%d d=%d", am, d);
        const int32_t xv = (am == 0) ? 0x4000'0000 : d;
        diff(dut, A, B, Vtx{xv, xv + 1, xv - 1, 32, false}, tag);
      }
    }
  }

  // ---- 7b. FULL-RANGE RANDOM ---------------------------------------------
  // A separate lane rather than a change to `--random`: the pose-range lane in
  // `--random` is deliberately aimed away from the rails, and the mutation
  // record depends on that (the no-saturation mutation fails 6 directed checks
  // and passes 900 random ones). Widening it would have destroyed a known
  // property of the existing lane to gain one the directed sections can carry.
  //
  // Deterministic seed, so a failure here is reproducible from the name alone.
  {
    Prng rng(0xB16Eu);
    for (uint32_t k = 0; k < 600; ++k) {
      zc::mat3x4fx A{}, B{};
      for (int i = 0; i < 12; ++i) {
        A.m[i] = static_cast<int32_t>(rng.next());  // the WHOLE word
        B.m[i] = static_cast<int32_t>(rng.next());
      }
      Vtx v;
      v.x = static_cast<int32_t>(rng.next());
      v.y = static_cast<int32_t>(rng.next());
      v.z = static_cast<int32_t>(rng.next());
      v.w0 = static_cast<uint8_t>(rng.below(65));
      v.rigid = (rng.below(4) == 0);
      char tag[64];
      std::snprintf(tag, sizeof tag, "fullrange[%u] w0=%u%s", k, v.w0, v.rigid ? " rigid" : "");
      diff(dut, A, B, v, tag);
    }
  }

  // ---- 8. THE RATE, which is the justification for the DSP count ----------
  // This block spends 12-18 DSP blocks instead of 72 because the owner's
  // sustained demand is ~120,000 skinned vertex instances per 60 Hz frame and
  // a 100 MHz gpu_clk gives 1,666,666 clocks to serve them -- 13.88 clocks per
  // vertex against the 10 this configuration takes.
  //
  // That entire argument lives or dies on the issue interval, so the issue
  // interval is a LAW here and not a comment. A scheduling change that quietly
  // cost three more clocks would leave every arithmetic check green and every
  // frame short, and nothing else in this repository would notice.
  {
    const zc::mat3x4fx A = mat(ONE, 0, 0, 3 * ONE, 0, ONE, 0, -ONE, 0, 0, ONE, 2 * ONE);
    const zc::mat3x4fx B = mat(0, ONE, 0, 0, ONE, 0, 0, 5 * ONE, 0, 0, ONE, 0);

    // Latency, measured through the ordinary driver.
    int32_t bx = 0, by = 0, bz = 0;
    const int lat_blend = run(dut, A, B, Vtx{ONE, 2 * ONE, 3 * ONE, 21, false}, bx, by, bz);
    const int lat_rigid = run(dut, A, B, Vtx{ONE, 2 * ONE, 3 * ONE, 64, true}, bx, by, bz);
    check(lat_blend == kLatencyBlend, "two-weight latency is the frontier table's value",
          static_cast<uint64_t>(kLatencyBlend), static_cast<uint64_t>(lat_blend));
    check(lat_rigid == kLatencyRigid, "rigid latency is the frontier table's value",
          static_cast<uint64_t>(kLatencyRigid), static_cast<uint64_t>(lat_rigid));

    // Sustained interval: valid held high, ready held high, nothing stalling.
    // The interval between successive results is what a frame budget is spent
    // in, and it is NOT necessarily the latency -- a block that pipelined would
    // have a smaller one, and one that dropped a cycle re-arming would have a
    // larger one.
    dut.v_valid_i = 1;
    dut.o_ready_i = 1;
    present(dut, A, B, Vtx{ONE, 2 * ONE, 3 * ONE, 21, false}, 0x77);
    int seen[6] = {0, 0, 0, 0, 0, 0};
    int nseen = 0;
    for (int c = 0; c < 200 && nseen < 6; ++c) {
      dut.eval();
      if (dut.o_valid_o) seen[nseen++] = c;
      zhao::tick(dut);
    }
    dut.v_valid_i = 0;
    dut.eval();

    check(nseen == 6, "a never-stalling stream retires six vertices", 6,
          static_cast<uint64_t>(nseen));
    if (nseen == 6) {
      bool uniform = true;
      for (int i = 2; i < 6; ++i) uniform = uniform && (seen[i] - seen[i - 1] == seen[1] - seen[0]);
      const int interval = seen[1] - seen[0];
      check(uniform, "the sustained issue interval does not drift", 1, uniform ? 1 : 0);
      check(interval == kLatencyBlend, "sustained two-weight issue interval",
            static_cast<uint64_t>(kLatencyBlend), static_cast<uint64_t>(interval));

      // And the number that actually matters: does that interval serve the
      // frame? Stated as a check so the DSP budget cannot drift away from the
      // vertex budget without a red test.
      //
      // MUL_LANES = 1 is EXPECTED TO FAIL the demand and is checked to fail it.
      // A frontier point that under-delivers is evidence, and a test that let
      // it quietly pass would erase the evidence -- the wall is where the
      // measurement stops being a shrug.
      const long served = interval > 0 ? kClocksPerFrame / interval : 0;
      check((served >= kVertexDemand) == kMeetsDemand,
            kMeetsDemand ? "sustained rate meets the 120,000 vertices/frame demand"
                         : "this lane count is BELOW the demand, as the frontier table says",
            static_cast<uint64_t>(kVertexDemand), static_cast<uint64_t>(served));
    }

    // Drain, so the block is idle for whatever runs next.
    for (int c = 0; c < 32 && dut.o_valid_o; ++c) zhao::tick(dut);
    dut.eval();
  }

  dut.final();
  std::printf(
      "[geom_skin] oracle-checked coordinates: %ld, beyond the oracle's "
      "int64 narrowing: %ld\n",
      g_oracle_checked, g_beyond_oracle);
  return zhao::report_and_exit("geom_skin_directed");
}
