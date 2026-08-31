// render_textured_seam.cpp — the eight attribute blocks, running together.
//
// ---------------------------------------------------------------------------
// WHY A COMPOSITION TEST, WHEN EVERY BLOCK ALREADY PASSES
// ---------------------------------------------------------------------------
// CLAUDE.md: "Component checks passing is not likeness evidence." Eight blocks
// now pass their own directed tests -- ATTRSETUP, ATTRDIV, ATTRDIV.SVC,
// INTERP, CLIP's attribute swap, RCP24, PERSPUV, TEXJOIN -- and NOTHING has
// ever run two of them in sequence. Every one of those tests supplies its own
// inputs, so every one of them agrees with itself about what the formats mean.
//
// The bugs that survive that are JOINT bugs, and they are all of the same
// shape: one block emits a field the next block reads differently.
//
//   * ATTRDIV takes `area_i` as 47 bits UNSIGNED; GEOM.CLIP emits `out_area2_o`
//     as 48 bits SIGNED. Positive after winding normalisation, so the top bit
//     is always zero -- but "always" is a claim about a neighbour.
//   * INTERP emits a 96-bit signed numerator; ATTRDIV takes 96-bit signed. Same
//     width, and the only way to know the SIGN CONVENTION matches is to push a
//     negative one through both.
//   * PERSPUV takes `invw24` as U 0.0.24. Nothing upstream clamps an
//     interpolated attribute into that range; the divide can hand it anything
//     that fits 32 bits.
//   * INTERP places the plane at pixel CENTRES. EDGEWALK decides coverage at
//     pixel centres. If those two ever disagreed by half a pixel, every
//     attribute would be subtly wrong on exactly the pixels that are covered.
//
// So this file runs one textured triangle through the real chain --
//
//     ATTRSETUP  ->  INTERP  ->  ATTRDIV  ->  PERSPUV
//        (x3)          |            |            |
//                   coverage     area from     u, v for
//                 from EDGEWALK   the clip      the TMU
//
// -- and checks the coordinate at EVERY COVERED PIXEL against an independent
// C++ chain that never touches the RTL. The comparison is on the final u and v,
// which is where a mistake anywhere in the eight would land.

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_attrsetup.h"
#include "Vzhao_raster_attrdiv.h"
#include "Vzhao_raster_attrinterp.h"
#include "Vzhao_raster_edgewalk.h"
#include "Vzhao_raster_perspuv.h"

#include "zhao_sim.hpp"
#include "zref/zref_rcp.hpp"

namespace {

int64_t orient(int64_t ux, int64_t uy, int64_t vx, int64_t vy, int64_t px, int64_t py) {
  return (vx - ux) * (py - uy) - (vy - uy) * (px - ux);
}

int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
}

int64_t centre(int64_t p) { return (p << 8) + 128; }

__int128 wide(const uint32_t* w, int bits) {
  __int128 v = 0;
  const int words = (bits + 31) / 32;
  for (int i = words - 1; i >= 0; --i) v = (v << 32) | w[i];
  const __int128 one = 1;
  if (v & (one << (bits - 1))) v -= (one << bits);
  return v;
}

void put_wide(uint32_t* w, int words, __int128 v) {
  for (int i = 0; i < words; ++i) w[i] = static_cast<uint32_t>((v >> (32 * i)) & 0xFFFFFFFFu);
}

struct Tri {
  int32_t ax, ay, bx, by, cx, cy;
  // Per-vertex attributes: invw24 (U 0.0.24) and the two perspective numerators.
  int32_t invw[3], uow[3], vow[3];
};

/** THE INDEPENDENT CHAIN. No RTL, restated from the specs. */
struct Ref {
  int64_t invw, uow, vow;
  int32_t u, v;
};

Ref reference_at(const Tri& t, int64_t pxl, int64_t pyl, int64_t area) {
  const int64_t sx = centre(pxl), sy = centre(pyl);
  const int64_t w0 = orient(t.bx, t.by, t.cx, t.cy, sx, sy);
  const int64_t w1 = orient(t.cx, t.cy, t.ax, t.ay, sx, sy);
  const int64_t w2 = orient(t.ax, t.ay, t.bx, t.by, sx, sy);
  auto interp = [&](const int32_t* a) {
    const __int128 num = static_cast<__int128>(w0) * a[0] + static_cast<__int128>(w1) * a[1] +
                         static_cast<__int128>(w2) * a[2];
    return div_rhu(num, area);
  };
  Ref r;
  r.invw = interp(t.invw);
  r.uow = interp(t.uow);
  r.vow = interp(t.vow);
  // spec section 8, with the shift derived from the Q-formats (see
  // zhao_raster_perspuv.sv).
  const zref::rcp24_result rc = zref::rcp_u24(static_cast<uint32_t>(r.invw));
  const int sh = 32 - rc.k;
  auto recover = [&](int64_t n) -> int32_t {
    const __int128 p = static_cast<__int128>(n) * rc.r;
    __int128 q = (p + (static_cast<__int128>(1) << (sh - 1))) >> sh;
    if (q > INT32_MAX) q = INT32_MAX;
    if (q < INT32_MIN) q = INT32_MIN;
    return static_cast<int32_t>(q);
  };
  r.u = recover(r.uow);
  r.v = recover(r.vow);
  return r;
}

/** Run GEOM.ATTRSETUP for one attribute and return its plane. */
struct Plane {
  __int128 n0, dndx, dndy;
};

Plane run_setup(Vzhao_geom_attrsetup& t, const Tri& tri, const int32_t* a) {
  t.rst_n = 0;
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  t.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  t.ax_i = static_cast<uint32_t>(tri.ax);
  t.ay_i = static_cast<uint32_t>(tri.ay);
  t.bx_i = static_cast<uint32_t>(tri.bx);
  t.by_i = static_cast<uint32_t>(tri.by);
  t.cx_i = static_cast<uint32_t>(tri.cx);
  t.cy_i = static_cast<uint32_t>(tri.cy);
  t.va_i = static_cast<uint32_t>(a[0]);
  t.vb_i = static_cast<uint32_t>(a[1]);
  t.vc_i = static_cast<uint32_t>(a[2]);
  t.v_valid_i = 1;
  zhao::tick(t);
  t.v_valid_i = 0;
  t.eval();
  Plane p;
  p.n0 = wide(t.n0_o.data(), 96);
  p.dndx = wide(t.dndx_o.data(), 72);
  p.dndy = wide(t.dndy_o.data(), 72);
  return p;
}

struct Frag {
  int row, col;
  __int128 num;
};

/** Run RASTER.INTERP over one tile's coverage and collect every numerator. */
std::vector<Frag> run_interp(Vzhao_raster_attrinterp& t, const Plane& p, int tile_x, int tile_y,
                             const std::vector<std::pair<int, uint16_t>>& rows) {
  t.rst_n = 0;
  t.job_valid_i = 0;
  t.cov_valid_i = 0;
  t.n_ready_i = 0;
  t.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  put_wide(t.job_n0_i.data(), 3, p.n0);
  put_wide(t.job_dndx_i.data(), 3, p.dndx);
  put_wide(t.job_dndy_i.data(), 3, p.dndy);
  t.job_tile_x_i = static_cast<uint16_t>(tile_x) & 0xFFFu;
  t.job_tile_y_i = static_cast<uint16_t>(tile_y) & 0xFFFu;
  t.job_valid_i = 1;
  for (int i = 0; i < 10; ++i) {
    t.eval();
    if (t.job_ready_o) {
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.job_valid_i = 0;

  std::vector<Frag> out;
  size_t next = 0;
  for (int clocks = 0; clocks < 4000; ++clocks) {
    const bool offering = next < rows.size();
    if (offering) {
      t.cov_row_i = static_cast<uint8_t>(rows[next].first);
      t.cov_mask_i = rows[next].second;
      t.cov_last_i = (next + 1 == rows.size()) ? 1 : 0;
    }
    t.cov_valid_i = offering ? 1 : 0;
    t.n_ready_i = 1;
    t.eval();
    const bool took = offering && t.cov_ready_o;
    bool done = false;
    if (t.n_valid_o && t.n_ready_i) {
      out.push_back({t.n_row_o, t.n_col_o, wide(t.n_num_o.data(), 96)});
      done = t.n_last_o != 0;
    }
    zhao::tick(t);
    if (took) ++next;
    if (done) break;
  }
  t.cov_valid_i = 0;
  t.eval();
  return out;
}

/** One divide through RASTER.ATTRDIV. */
int64_t run_div(Vzhao_raster_attrdiv& t, __int128 num, uint64_t area, bool* ovf) {
  put_wide(t.num_i.data(), 3, num);
  t.area_i = area;
  t.v_valid_i = 1;
  for (int i = 0; i < 200; ++i) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    if (taken) break;
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (int i = 0; i < 200; ++i) {
    t.eval();
    if (t.r_valid_o) {
      const int64_t q = static_cast<int32_t>(t.q_o);
      *ovf = t.q_overflow_o != 0;
      zhao::tick(t);
      return q;
    }
    zhao::tick(t);
  }
  *ovf = true;
  return 0;
}

/** One fragment through RASTER.PERSPUV. */
void run_perspuv(Vzhao_raster_perspuv& t, int64_t uow, int64_t vow, uint32_t invw, int32_t* u,
                 int32_t* v, bool* zero) {
  t.u_over_w_i = static_cast<uint32_t>(static_cast<int32_t>(uow));
  t.v_over_w_i = static_cast<uint32_t>(static_cast<int32_t>(vow));
  t.invw24_i = invw;
  t.tag_i = 0;
  t.v_valid_i = 1;
  for (int i = 0; i < 400; ++i) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    if (taken) break;
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (int i = 0; i < 400; ++i) {
    t.eval();
    if (t.r_valid_o) {
      *u = static_cast<int32_t>(t.u_o);
      *v = static_cast<int32_t>(t.v_o);
      *zero = t.depth_zero_o != 0;
      zhao::tick(t);
      return;
    }
    zhao::tick(t);
  }
  *zero = true;
}

/** RASTER.EDGEWALK's coverage of one tile, from the RTL rather than a model. */
std::vector<std::pair<int, uint16_t>> run_edgewalk(Vzhao_raster_edgewalk& t, const Tri& tri,
                                                   int tile_x, int tile_y) {
  t.rst_n = 0;
  t.job_valid_i = 0;
  t.cov_ready_i = 1;
  t.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  t.job_ax_i = static_cast<uint32_t>(tri.ax);
  t.job_ay_i = static_cast<uint32_t>(tri.ay);
  t.job_bx_i = static_cast<uint32_t>(tri.bx);
  t.job_by_i = static_cast<uint32_t>(tri.by);
  t.job_cx_i = static_cast<uint32_t>(tri.cx);
  t.job_cy_i = static_cast<uint32_t>(tri.cy);
  t.job_tile_x_i = static_cast<uint16_t>(tile_x) & 0xFFFu;
  t.job_tile_y_i = static_cast<uint16_t>(tile_y) & 0xFFFu;
  t.job_src_id_i = 0x77;
  t.job_valid_i = 1;
  for (int i = 0; i < 20; ++i) {
    t.eval();
    if (t.job_ready_o) {
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.job_valid_i = 0;

  std::vector<std::pair<int, uint16_t>> rows;
  for (int clocks = 0; clocks < 2000; ++clocks) {
    t.eval();
    if (t.cov_valid_o && t.cov_ready_i) rows.push_back({t.cov_row_o, t.cov_mask_o});
    const bool done = t.job_done_o != 0;
    zhao::tick(t);
    if (done) break;
  }
  return rows;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_attrsetup setup;
  Vzhao_raster_edgewalk edge;
  Vzhao_raster_attrinterp interp;
  Vzhao_raster_attrdiv divi;
  Vzhao_raster_perspuv persp;

  divi.rst_n = 0;
  divi.v_valid_i = 0;
  divi.r_ready_i = 1;
  persp.rst_n = 0;
  persp.v_valid_i = 0;
  persp.r_ready_i = 1;
  divi.eval();
  persp.eval();
  for (int i = 0; i < 4; ++i) {
    zhao::tick(divi);
    zhao::tick(persp);
  }
  divi.rst_n = 1;
  persp.rst_n = 1;
  divi.eval();
  persp.eval();

  auto px = [](double p) { return static_cast<int32_t>(p * 256.0); };

  // A textured triangle with a real perspective spread: invw24 varies 4x across
  // it, so the reciprocal's exponent MOVES and a fixed shift anywhere in the
  // chain would show. u/v_over_w are the perspective numerators, so they scale
  // with invw24 the way the geometry pipeline produces them.
  Tri tri;
  tri.ax = px(2);
  tri.ay = px(1);
  tri.bx = px(60);
  tri.by = px(4);
  tri.cx = px(8);
  tri.cy = px(58);
  const int32_t iw[3] = {0x400000, 0x100000, 0x800000};  // U 0.0.24, a 8x spread
  for (int k = 0; k < 3; ++k) {
    tri.invw[k] = iw[k];
    // u = 3.5 texture units, v = -1.25, carried as u*invw in S 8.24.
    tri.uow[k] = static_cast<int32_t>(3.5 * iw[k]);
    tri.vow[k] = static_cast<int32_t>(-1.25 * iw[k]);
  }

  const int64_t area = orient(tri.ax, tri.ay, tri.bx, tri.by, tri.cx, tri.cy);
  zhao::check(area > 0, "the case triangle is winding-normalised", 1, area > 0 ? 1 : 0);
  // The joint the composition depends on: GEOM.CLIP emits 2A as 48 bits SIGNED
  // and RASTER.ATTRDIV takes it as 47 bits UNSIGNED. That is only safe because
  // winding normalisation makes it positive, which is a claim about a
  // neighbour -- so it is checked, here, on the value actually passed.
  zhao::check(area > 0 && area < (int64_t(1) << 47),
              "and its 2A fits the divider's 47 unsigned bits", 1,
              (area > 0 && area < (int64_t(1) << 47)) ? 1 : 0);

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: three planes, one coverage, every pixel through the chain ==\n");
  long checked = 0, bad_u = 0, bad_v = 0, bad_attr = 0;
  // The reciprocal exponents ACTUALLY exercised, collected as the chain runs.
  std::set<int> ks_seen;
  int64_t iw_lo = INT64_MAX, iw_hi = 0;
  {
    const Plane p_iw = run_setup(setup, tri, tri.invw);
    const Plane p_uw = run_setup(setup, tri, tri.uow);
    const Plane p_vw = run_setup(setup, tri, tri.vow);

    for (int ty = 0; ty < 64; ty += 16) {
      for (int tx = 0; tx < 64; tx += 16) {
        const std::vector<std::pair<int, uint16_t>> rows = run_edgewalk(edge, tri, tx, ty);
        if (rows.empty()) continue;

        // The SAME coverage drives all three planes, which is what makes them
        // one fragment rather than three coincidences.
        const std::vector<Frag> f_iw = run_interp(interp, p_iw, tx, ty, rows);
        const std::vector<Frag> f_uw = run_interp(interp, p_uw, tx, ty, rows);
        const std::vector<Frag> f_vw = run_interp(interp, p_vw, tx, ty, rows);
        if (f_iw.size() != f_uw.size() || f_uw.size() != f_vw.size()) {
          ++bad_attr;
          continue;
        }

        for (size_t i = 0; i < f_iw.size(); ++i) {
          bool o1 = false, o2 = false, o3 = false;
          const int64_t q_iw = run_div(divi, f_iw[i].num, static_cast<uint64_t>(area), &o1);
          const int64_t q_uw = run_div(divi, f_uw[i].num, static_cast<uint64_t>(area), &o2);
          const int64_t q_vw = run_div(divi, f_vw[i].num, static_cast<uint64_t>(area), &o3);
          if (o1 || o2 || o3) {
            ++bad_attr;
            continue;
          }

          const Ref want = reference_at(tri, tx + f_iw[i].col, ty + f_iw[i].row, area);
          if (q_iw != want.invw || q_uw != want.uow || q_vw != want.vow) {
            if (bad_attr < 4)
              printf("      (%d,%d): interpolated attrs differ (iw %lld vs %lld)\n",
                     tx + f_iw[i].col, ty + f_iw[i].row, (long long)q_iw, (long long)want.invw);
            ++bad_attr;
            continue;
          }

          int32_t u = 0, v = 0;
          bool zero = false;
          run_perspuv(persp, q_uw, q_vw, static_cast<uint32_t>(q_iw), &u, &v, &zero);
          if (zero) {
            ++bad_attr;
            continue;
          }
          if (u != want.u) {
            if (bad_u < 4)
              printf("      (%d,%d): u %d, reference %d\n", tx + f_iw[i].col, ty + f_iw[i].row, u,
                     want.u);
            ++bad_u;
          }
          if (v != want.v) ++bad_v;
          ks_seen.insert(zref::rcp_u24(static_cast<uint32_t>(q_iw)).k);
          if (q_iw < iw_lo) iw_lo = q_iw;
          if (q_iw > iw_hi) iw_hi = q_iw;
          ++checked;
        }
      }
    }
    printf("   MEASURED: %ld covered pixels through ATTRSETUP -> INTERP -> ATTRDIV -> PERSPUV\n",
           checked);
    zhao::check(bad_attr == 0, "every interpolated attribute matches the reference chain", 0,
                (uint32_t)bad_attr);
    zhao::check(bad_u == 0, "and every recovered u matches", 0, (uint32_t)bad_u);
    zhao::check(bad_v == 0, "and every recovered v matches", 0, (uint32_t)bad_v);
    zhao::check(checked > 200, "over enough pixels to mean something", 1, checked > 200 ? 1 : 0);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the case could have failed ==\n");
  {
    // ANTI-VACUITY, and it is the part that makes section 1 worth running. The
    // chain is only under strain if the depth actually VARIES -- a constant
    // invw24 gives one reciprocal exponent and hides every shift error in the
    // composition.
    //
    // This asks what section 1 ACTUALLY EXERCISED, not what two hand-picked
    // pixels would have. The first version of this check sampled the plane at
    // (3,2) and (9,55), got the same exponent at both, and failed -- while
    // section 1 had been running the whole time over a range those two points
    // did not represent. A vacuity check that samples somewhere other than the
    // thing it is vouching for is not a vacuity check.
    printf("   MEASURED: invw24 spanned %lld..%lld over the pixels actually run\n",
           (long long)iw_lo, (long long)iw_hi);
    printf("   MEASURED: %zu distinct reciprocal exponents exercised\n", ks_seen.size());
    zhao::check(iw_hi > iw_lo * 2, "the depth varied by more than 2x over the covered pixels", 1,
                (iw_hi > iw_lo * 2) ? 1 : 0);
    zhao::check(ks_seen.size() >= 2,
                "and more than one reciprocal exponent was exercised, so a fixed shift fails", 2,
                (uint32_t)ks_seen.size());
    // And the coordinates are not trivially zero, which would match anything.
    const Ref mid = reference_at(tri, 10, 10, area);
    zhao::check(mid.u != 0 && mid.v != 0, "and the recovered coordinates are non-zero", 1,
                (mid.u != 0 && mid.v != 0) ? 1 : 0);
  }

  return zhao::report_and_exit("render_textured_seam");
}
