// geom_project_directed.cpp — GEOM.PROJECT against `zref::render::project_vertex`.
//
// Every case runs the Verilated `zhao_geom_project` and the reference over the
// same vertex, matrix and viewport, and requires the whole output packet —
// canvas x and y, the 1/w depth word, the behind-the-eye flag and the source id
// — to be IDENTICAL.
//
// The ledger declared this block's oracle as `zref::GeomProject`, which does not
// exist. `project_vertex` is the real law: it is what the software raster
// projects every vertex with, and it is what TERRAIN.PROJECT is already verified
// against. Fixing the citation was the first step of building this block, and it
// is why this file names `zref::render::project_vertex` rather than a model
// written beside the RTL.
//
// FIVE LAWS, each one a place an implementation drifts:
//
//   1. ONE ROUNDING PER ROW. qformats §2: four products summed EXACTLY, then a
//      single rescale(.,16). Rounding each product is a second rounding and A3b
//      forbids it.
//   2. THE NEAR PLANE IS `clip.w <= 0`, AND A REJECTED VERTEX CARRIES ZERO.
//      Not a drop -- dropping is GEOM.CLIP's verdict. `w == 0` exactly is the
//      boundary and belongs on the reject side.
//   3. THE DIVISION IS EXACT AND ROUNDS HALF UP, including for NEGATIVE
//      numerators, where round-half-up is not the same as round-away-from-zero.
//      A reciprocal multiply cannot reproduce it, which is why the block
//      contains a real divider.
//   4. THE GUARD BAND IS A CLAMP, NOT A CLIP. ±2048 px, both rails, both axes.
//      GEOM.CLIP assumes every arriving vertex is already inside it.
//   5. THE DUAL VIEW IS TWO REGISTER SETS. The same vertex under two different
//      matrices must give two different answers, each matching its own view.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_project.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zrender/internal.hpp"

namespace {

using zhao::check;
namespace zr = zref::render;

constexpr int32_t kOne = 1 << 16;

/**
 * Sign-extend a 21-bit canvas coordinate. Verilator hands a 21-bit port back in
 * a uint32_t with the top bits zero, so a plain cast reads -524288 as 1572864
 * and every guard-band rail looks like a failure. The RTL is signed; the
 * testbench has to say so.
 */
int32_t sx21(uint32_t v) { return static_cast<int32_t>(v << 11) >> 11; }

struct VtxIn {
  int32_t x, y, z;
  bool view;
  uint16_t src;
};

struct VtxOut {
  int32_t x, y, d;
  bool behind;
  uint16_t src;
};

class Dut {
 public:
  explicit Dut(Vzhao_geom_project& d) : dut_(d) {
    dut_.rst_n = 0;
    dut_.cfg_we_i = 0;
    dut_.v_valid_i = 0;
    dut_.out_ready_i = 0;
    dut_.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
  }

  void write_cfg(int view, uint8_t addr, uint32_t data) {
    dut_.cfg_we_i = 1;
    dut_.cfg_view_i = static_cast<uint8_t>(view);
    dut_.cfg_addr_i = addr;
    dut_.cfg_data_i = data;
    dut_.eval();
    zhao::tick(dut_);
    dut_.cfg_we_i = 0;
    dut_.eval();
  }

  void configure(int view, const zref::mat4fx& m, const zr::Viewport& vp) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        write_cfg(view, static_cast<uint8_t>(r * 4 + c), static_cast<uint32_t>(m.m[r][c].raw));
      }
    }
    write_cfg(view, 16, (vp.y0 << 16) | (vp.x0 & 0xFFFFu));
    write_cfg(view, 17, (vp.h << 16) | (vp.w & 0xFFFFu));
  }

  /**
   * Push every vertex and collect every result. `stall_mask` is rotated one bit
   * per cycle onto `out_ready_i`; 0 means the consumer is always ready.
   */
  std::vector<VtxOut> run(const std::vector<VtxIn>& in, uint32_t stall_mask = 0) {
    std::vector<VtxOut> out;
    out.reserve(in.size());
    size_t pushed = 0;
    uint32_t mask = stall_mask;
    const int limit = static_cast<int>(in.size()) * 64 + 8192;

    for (int cycle = 0; cycle < limit && out.size() < in.size(); ++cycle) {
      const bool ready = (stall_mask == 0) || ((mask & 1u) == 0);
      mask = (mask >> 1) | (mask << 31);

      dut_.out_ready_i = ready ? 1 : 0;
      if (pushed < in.size()) {
        dut_.v_valid_i = 1;
        dut_.vx_i = static_cast<uint32_t>(in[pushed].x);
        dut_.vy_i = static_cast<uint32_t>(in[pushed].y);
        dut_.vz_i = static_cast<uint32_t>(in[pushed].z);
        dut_.view_i = in[pushed].view ? 1 : 0;
        dut_.src_id_i = in[pushed].src;
      } else {
        dut_.v_valid_i = 0;
      }
      dut_.eval();

      const bool took_in = dut_.v_valid_i && dut_.v_ready_o;
      const bool took_out = dut_.out_valid_o && dut_.out_ready_i;
      if (took_out) {
        VtxOut o;
        o.x = sx21(dut_.out_x_o);
        o.y = sx21(dut_.out_y_o);
        o.d = static_cast<int32_t>(dut_.out_d_o);
        o.behind = dut_.out_behind_o != 0;
        o.src = static_cast<uint16_t>(dut_.out_src_id_o);
        out.push_back(o);
      }
      zhao::tick(dut_);
      if (took_in) ++pushed;
    }
    dut_.v_valid_i = 0;
    dut_.eval();
    return out;
  }

  uint32_t transformed() const { return dut_.vertices_transformed_o; }

 private:
  Vzhao_geom_project& dut_;
};

/** The oracle, in the RTL's output shape. */
VtxOut oracle(const zref::mat4fx& m, const zr::Viewport& vp, const VtxIn& v) {
  const zr::ProjOut p = zr::project_vertex(m, vp, zref::fx16{v.x}, zref::fx16{v.y},
                                           zref::fx16{v.z}, nullptr);
  VtxOut o;
  o.behind = !p.in;
  o.x = p.s.x;
  o.y = p.s.y;
  o.d = p.s.d;
  o.src = v.src;
  return o;
}

void compare(const VtxOut& want, const VtxOut& got, const char* what) {
  const std::string t(what);
  check(got.behind == want.behind, (t + ": behind-the-eye flag").c_str(), want.behind ? 1 : 0,
        got.behind ? 1 : 0);
  check(got.x == want.x, (t + ": canvas x").c_str(), static_cast<uint32_t>(want.x),
        static_cast<uint32_t>(got.x));
  check(got.y == want.y, (t + ": canvas y").c_str(), static_cast<uint32_t>(want.y),
        static_cast<uint32_t>(got.y));
  check(got.d == want.d, (t + ": the 1/w depth word").c_str(), static_cast<uint32_t>(want.d),
        static_cast<uint32_t>(got.d));
  check(got.src == want.src, (t + ": src_id rides its own vertex").c_str(), want.src, got.src);
}

zref::mat4fx mat_of(const int32_t r[16]) {
  zref::mat4fx m{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) m.m[i][j] = zref::fx16{r[i * 4 + j]};
  }
  return m;
}

/** Identity view-projection: clip = the vertex itself, w = 1.0. */
zref::mat4fx identity_vp() {
  const int32_t r[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne};
  return mat_of(r);
}

/** A perspective-ish matrix: w comes from z, so the near plane is reachable. */
zref::mat4fx persp_vp(int32_t zscale, int32_t wz, int32_t wc) {
  const int32_t r[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, zscale, 0, 0, 0, wz, wc};
  return mat_of(r);
}

zr::Viewport view_of(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h) {
  zr::Viewport v;
  v.x0 = x0; v.y0 = y0; v.w = w; v.h = h;
  return v;
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_geom_project raw;
  Dut dut(raw);

  // Duo: two 256x192 view blocks stacked in the storage raster (video_rules §3.1).
  const zr::Viewport vp0 = view_of(0, 0, 256, 192);
  const zr::Viewport vp1 = view_of(0, 192, 256, 192);
  // A second pair that differs on BOTH axes. The Duo pair above differs only in
  // y0, so a block that selected the viewport with a hardwired view 0 on the X
  // lane produced identical x for both views and passed every case -- the
  // mutation that does exactly that survived the whole suite until this pair
  // existed.
  const zr::Viewport vpA = view_of(7, 3, 320, 200);
  const zr::Viewport vpB = view_of(101, 57, 128, 96);

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x9A0Fu);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      // A pose-plausible matrix: the 3x3 near unit scale, the translation
      // carrying the range, and a w row that actually varies with z so the near
      // plane is reachable rather than theoretical.
      int32_t r[16];
      for (int k = 0; k < 16; ++k) r[k] = static_cast<int32_t>(rng.next()) >> 14;
      r[12] = 0; r[13] = 0;
      r[14] = static_cast<int32_t>(rng.next()) >> 15;
      r[15] = static_cast<int32_t>(rng.next()) >> 14;
      const zref::mat4fx m = mat_of(r);
      const zr::Viewport vp = view_of(rng.below(64), rng.below(64), 1 + rng.below(512),
                                      1 + rng.below(512));
      dut.configure(0, m, vp);

      std::vector<VtxIn> in;
      const int n = 1 + static_cast<int>(rng.below(24));
      for (int k = 0; k < n; ++k) {
        VtxIn v;
        v.x = static_cast<int32_t>(rng.next()) >> static_cast<int>(rng.below(14));
        v.y = static_cast<int32_t>(rng.next()) >> static_cast<int>(rng.below(14));
        v.z = static_cast<int32_t>(rng.next()) >> static_cast<int>(rng.below(14));
        v.view = false;
        v.src = static_cast<uint16_t>(rng.next());
        in.push_back(v);
      }
      const std::vector<VtxOut> got = dut.run(in, rng.below(2) ? 0x55555555u : 0u);
      check(got.size() == in.size(), "every vertex produced a result", in.size(),
            static_cast<uint64_t>(got.size()));
      for (size_t k = 0; k < got.size() && k < in.size(); ++k) {
        char tag[96];
        std::snprintf(tag, sizeof tag, "random[%u] vertex %zu", it, k);
        compare(oracle(m, vp, in[k]), got[k], tag);
      }
    }
    raw.final();
    return zhao::report_and_exit("geom_project_random");
  }

  // ---- 1. the identity: a vertex lands at the viewport centre -------------
  {
    const zref::mat4fx m = identity_vp();
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in = {
        {0, 0, 0, false, 0x11},                 // NDC origin -> viewport centre
        {kOne / 2, 0, 0, false, 0x12},          // +x half
        {0, kOne / 2, 0, false, 0x13},          // +y half
        {-kOne / 2, -kOne / 2, 0, false, 0x14},
    };
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "identity: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size(); ++k) {
      char tag[64];
      std::snprintf(tag, sizeof tag, "identity vertex %zu", k);
      compare(oracle(m, vp0, in[k]), got[k], tag);
    }
    // Asserted directly as well: the reference and the RTL could agree and both
    // be wrong about where the centre of a viewport is.
    check(got[0].x == static_cast<int32_t>((vp0.x0 + vp0.w / 2) * 256),
          "identity: the NDC origin lands on the viewport centre in S 12.8",
          static_cast<uint32_t>((vp0.x0 + vp0.w / 2) * 256), static_cast<uint32_t>(got[0].x));
  }

  // ---- 2. THE NEAR PLANE, exactly at the boundary -------------------------
  // w = m[3][2]*z + m[3][3]. With m[3][2] = 1.0 and m[3][3] = 0, w == z, so the
  // boundary is z == 0 and it belongs on the REJECT side (`clip.w <= 0`). A
  // block using `< 0` passes every other case in this file and lets a vertex
  // exactly on the eye plane through, where the divider would divide by zero.
  {
    const zref::mat4fx m = persp_vp(kOne, kOne, 0);
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in = {
        {kOne, kOne, 1, false, 0x21},        // w = 1 raw unit: the smallest legal w
        {kOne, kOne, 0, false, 0x22},        // w == 0 EXACTLY: rejected
        {kOne, kOne, -1, false, 0x23},       // w < 0: rejected
        {kOne, kOne, 4 * kOne, false, 0x24}, // comfortably in front
    };
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "near plane: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size(); ++k) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "near plane vertex %zu", k);
      compare(oracle(m, vp0, in[k]), got[k], tag);
    }
    check(got[1].behind, "w == 0 exactly is BEHIND the eye, not in front", 1,
          got[1].behind ? 1 : 0);
    check(got[1].x == 0 && got[1].y == 0 && got[1].d == 0,
          "and a rejected vertex carries zeros, it is not dropped", 1,
          (got[1].x == 0 && got[1].y == 0 && got[1].d == 0) ? 1 : 0);
    check(!got[0].behind, "w == 1 raw unit is in front", 0, got[0].behind ? 1 : 0);
  }

  // ---- 3. THE DIVISION: negative numerators and round-half-up -------------
  // Round-half-up is NOT round-away-from-zero. A negative quotient whose
  // fractional part is exactly one half rounds toward +infinity, i.e. toward
  // zero -- and that is the case a naive |a|/|b| with a sign fixup gets wrong.
  {
    const zref::mat4fx m = persp_vp(kOne, 0, kOne);  // w == 1.0 constant
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in;
    // With w == 1.0 the quotient is the clip coordinate itself, so these sweep
    // the divider's sign and rounding behaviour directly.
    const int32_t vals[] = {0, 1, -1, 2, -2, 3, -3, kOne, -kOne, kOne / 2, -kOne / 2,
                            kOne + 1, -kOne - 1, 0x7FFFFFFF, static_cast<int32_t>(0x80000000)};
    for (int32_t v : vals) in.push_back({v, -v, 0, false, static_cast<uint16_t>(v & 0xFFFF)});
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "division: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size(); ++k) {
      char tag[96];
      std::snprintf(tag, sizeof tag, "division sign/rounding vertex %zu", k);
      compare(oracle(m, vp0, in[k]), got[k], tag);
    }
  }

  // ---- 4. THE GUARD BAND IS A CLAMP ---------------------------------------
  // Vertices far outside the viewport on every side. They must come back
  // CLAMPED to ±2048 px (±524288 in S 12.8), not wrapped and not dropped --
  // GEOM.CLIP's header assumes this block already did it.
  {
    const zref::mat4fx m = persp_vp(kOne, 0, kOne);
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in = {
        {1000 * kOne, 0, 0, false, 0x41},
        {-1000 * kOne, 0, 0, false, 0x42},
        {0, 1000 * kOne, 0, false, 0x43},
        {0, -1000 * kOne, 0, false, 0x44},
        {0x7FFFFFFF, 0x7FFFFFFF, 0, false, 0x45},
        {static_cast<int32_t>(0x80000000), static_cast<int32_t>(0x80000000), 0, false, 0x46},
    };
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "guard band: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size(); ++k) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "guard band vertex %zu", k);
      compare(oracle(m, vp0, in[k]), got[k], tag);
    }
    bool all_in_band = true;
    for (const VtxOut& o : got) {
      all_in_band = all_in_band && o.x <= 524288 && o.x >= -524288 && o.y <= 524288 &&
                    o.y >= -524288;
    }
    check(all_in_band, "every vertex leaves inside the ±2048 px guard band", 1,
          all_in_band ? 1 : 0);
    check(got[0].x == 524288, "and a far-right vertex sits exactly on the rail", 524288,
          static_cast<uint32_t>(got[0].x));
  }

  // ---- 5. TWO VIEWS, TWO REGISTER SETS ------------------------------------
  // The same vertex under two different matrices and viewports. If the view bit
  // selected nothing, both would come back identical.
  {
    const zref::mat4fx m0 = identity_vp();
    const int32_t r1[16] = {2 * kOne, 0, 0, 0, 0, 2 * kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne};
    const zref::mat4fx m1 = mat_of(r1);
    dut.configure(0, m0, vp0);
    dut.configure(1, m1, vp1);

    std::vector<VtxIn> in = {
        {kOne / 4, kOne / 4, 0, false, 0x51},
        {kOne / 4, kOne / 4, 0, true, 0x52},
        {kOne / 4, kOne / 4, 0, false, 0x53},
        {kOne / 4, kOne / 4, 0, true, 0x54},
    };
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "dual view: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size(); ++k) {
      char tag[64];
      std::snprintf(tag, sizeof tag, "dual view vertex %zu", k);
      compare(oracle(in[k].view ? m1 : m0, in[k].view ? vp1 : vp0, in[k]), got[k], tag);
    }
    check(got[0].x != got[1].x || got[0].y != got[1].y,
          "the two views genuinely differ -- the view bit selects a register set", 1,
          (got[0].x != got[1].x || got[0].y != got[1].y) ? 1 : 0);
    // Interleaved: view 0, 1, 0, 1 must not smear one view's matrix onto the
    // next vertex, which a single shared register set would do.
    check(got[0].x == got[2].x && got[0].y == got[2].y,
          "interleaving views does not smear one view's matrix onto the next vertex", 1,
          (got[0].x == got[2].x && got[0].y == got[2].y) ? 1 : 0);
    check(got[1].x == got[3].x && got[1].y == got[3].y, "and the same the other way", 1,
          (got[1].x == got[3].x && got[1].y == got[3].y) ? 1 : 0);

    // The same vertex, the SAME matrix, and viewports differing on both axes.
    // Now only the viewport register set can make the answers differ, so a
    // hardwired view index on either lane fails here.
    dut.configure(0, m0, vpA);
    dut.configure(1, m0, vpB);
    std::vector<VtxIn> in2 = {
        {kOne / 3, -kOne / 5, 0, false, 0x61},
        {kOne / 3, -kOne / 5, 0, true, 0x62},
    };
    const std::vector<VtxOut> g2 = dut.run(in2);
    check(g2.size() == 2, "both-axes pair produced both results", 2,
          static_cast<uint64_t>(g2.size()));
    compare(oracle(m0, vpA, in2[0]), g2[0], "viewport A under view 0");
    compare(oracle(m0, vpB, in2[1]), g2[1], "viewport B under view 1");
    check(g2[0].x != g2[1].x, "the two viewports give different x -- the X lane selects too", 1,
          (g2[0].x != g2[1].x) ? 1 : 0);
    check(g2[0].y != g2[1].y, "and different y", 1, (g2[0].y != g2[1].y) ? 1 : 0);
  }

  // ---- 5b. THE ROW ROUNDING WALL ------------------------------------------
  // qformats §2 rounds each row ONCE, half up. Every matrix above is built from
  // whole multiples of 1.0, so the row sums have nothing below bit 16 and a
  // truncating rescale gives the same answer -- the mutation that truncates
  // passed all 382 checks and showed up once in three hundred random vertices.
  //
  // These matrices carry single-ULP elements, so the row sum's low 16 bits are
  // whatever the vertex puts there, and the residues below sit on and around the
  // exact half where round-half-up and truncation disagree.
  {
    const int32_t r[16] = {1, 3, 5, 7, 2, 6, 10, 14, 0, 0, kOne, 0, 0, 0, 0, kOne};
    const zref::mat4fx m = mat_of(r);
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in;
    const int32_t res[] = {0, 1, -1, (1 << 15), -(1 << 15), (1 << 15) + 1, -(1 << 15) - 1,
                           (1 << 16) - 1, -(1 << 16) + 1, 32768 * 3, -32768 * 3, 12345, -12345,
                           21845, -21845};
    for (int32_t a : res) {
      for (int32_t b : {a, -a, a + 1}) {
        in.push_back({a, b, a - b, false, static_cast<uint16_t>(a & 0xFFFF)});
      }
    }
    const std::vector<VtxOut> got = dut.run(in);
    check(got.size() == in.size(), "rounding wall: every vertex produced a result", in.size(),
          static_cast<uint64_t>(got.size()));
    for (size_t k = 0; k < got.size() && k < in.size(); ++k) {
      char tag[96];
      std::snprintf(tag, sizeof tag, "row rounding residue vertex %zu", k);
      compare(oracle(m, vp0, in[k]), got[k], tag);
    }
  }

  // ---- 6. backpressure changes nothing but the timing ---------------------
  // A rigid pipeline: a stalled consumer freezes the chain, so the same vertices
  // must come back in the same order with the same values.
  {
    const zref::mat4fx m = persp_vp(kOne, kOne / 2, 2 * kOne);
    dut.configure(0, m, vp0);
    std::vector<VtxIn> in;
    for (int k = 0; k < 40; ++k) {
      in.push_back({(k - 20) * (kOne / 8), (20 - k) * (kOne / 16), (k - 25) * (kOne / 4), false,
                    static_cast<uint16_t>(0x600 + k)});
    }
    const std::vector<VtxOut> free_run = dut.run(in);
    const std::vector<VtxOut> stalled = dut.run(in, 0xAAAAAAAAu);
    const std::vector<VtxOut> choppy = dut.run(in, 0xF0F0F0F0u);
    check(free_run.size() == in.size() && stalled.size() == in.size() &&
              choppy.size() == in.size(),
          "every run returned every vertex", in.size(),
          static_cast<uint64_t>(free_run.size()));
    bool same = true;
    for (size_t k = 0; k < in.size(); ++k) {
      same = same && free_run[k].x == stalled[k].x && free_run[k].y == stalled[k].y &&
             free_run[k].d == stalled[k].d && free_run[k].src == stalled[k].src &&
             free_run[k].x == choppy[k].x && free_run[k].src == choppy[k].src;
    }
    check(same, "backpressure changes the timing and nothing else", 1, same ? 1 : 0);
    for (size_t k = 0; k < in.size(); ++k) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "stalled run vertex %zu", k);
      compare(oracle(m, vp0, in[k]), stalled[k], tag);
    }
  }

  // ---- 7. the counter counts accepted vertices ----------------------------
  {
    const uint32_t before = dut.transformed();
    std::vector<VtxIn> in = {{0, 0, 0, false, 1}, {kOne, 0, 0, false, 2}, {0, kOne, 0, false, 3}};
    dut.run(in);
    check(dut.transformed() == before + 3, "vertices_transformed counts what was accepted",
          before + 3, dut.transformed());
  }

  raw.final();
  return zhao::report_and_exit("geom_project_directed");
}
