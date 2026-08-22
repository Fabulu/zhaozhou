// geom_cull_directed.cpp — GEOM.MESHFETCH's conservative frustum cull, RTL
// against the oracle, and the ORACLE against the shipped renderer.
//
// ---------------------------------------------------------------------------
// THIS FILE CARRIES TWO DIFFERENT BURDENS, AND THEY ARE NOT THE SAME WEIGHT
// ---------------------------------------------------------------------------
// reports/PHANTOM_REFERENCES.md separates a reference that already exists
// (kind 1) from one that has to be WRITTEN and thereby becomes the law
// (kind 2). `zref::cull` is both at once, so this file checks both halves and
// keeps them apart:
//
//   THE CAMERA IS KIND 1, and sections 2 and 3 pin it to the SHIPPED renderer.
//   The five planes are supposed to describe exactly the volume
//   `zref::render::project_vertex` draws. Section 3 checks that against
//   `project_vertex` ITSELF and against `zref::mat4_vec4` and
//   `zref::fx_div_exact` — the functions the renderer calls — never against a
//   second copy of the condition written here. If the row combinations were
//   wrong, most plausibly by using row0 where row1 belongs, this is where it
//   shows.
//
//   THE SPHERE TEST IS KIND 2. Nothing shipped tests a sphere against a plane,
//   so `zref::cull` IS the law and sections 4 onward are the ordinary RTL
//   differential: same inputs into the module and into the header, compare.
//   The law itself is defended by ARGUMENT (the bound is a ceiling, so
//   rejection implies true rejection) plus section 5, which measures that the
//   test can actually see the difference between the ceiling and the floor.
//
// ---------------------------------------------------------------------------
// WHAT THE RTL DOES DIFFERENTLY FROM THE HEADER, WHICH IS WHY THIS IS WORTH
// MORE THAN A RETYPE CHECK
// ---------------------------------------------------------------------------
//   · The header stores five planes per view. The RTL stores NONE — it rebuilds
//     any plane from the registered matrix with four 33-bit adds behind a mux.
//     A mux that mis-selects a row is invisible on a symmetric camera.
//   · The header calls a 128-bit `cull_isqrt`. The RTL runs the restoring
//     recurrence over 33 clocks in 66-bit registers, starting unconditionally
//     at 4^32 where the software has a `while (bit > num)` prologue.
//   · The header takes ceil by squaring the root back. The RTL reads the
//     recurrence's own REMAINDER, which is a different computation that happens
//     to be equal — section 1 checks exactly that.
//   · The header is a pure function. The RTL is a sequencer with a dirty bit,
//     and a stale length bound would delete geometry silently. Section 7 drives
//     the protocol rather than trusting it.
//
// DOMAIN. `radius >= 0`, which the RTL asserts under FORMAL. A negative radius
// is arithmetically well defined and both sides agree on it, but it is
// meaningless as a bound and it makes rejection easier — the one direction that
// loses geometry — so this file stays inside the domain deliberately.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_cull.h"

#include "zhao_sim.hpp"
#include "zref/zref_cull.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"
#include "zrender/internal.hpp"  // white-box: zref::render::project_vertex IS the law

namespace {

using zhao::check;
using zref::fx16;
using zref::mat4fx;
using zref::vec3fx;
using zref::vec4fx;
namespace zc = zref::cull;

constexpr int32_t ONE = 1 << 16;

int g_cases = 0;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0u; }
  int32_t range(int32_t lo, int32_t hi) {
    return lo +
           static_cast<int32_t>(next() % static_cast<uint64_t>(static_cast<int64_t>(hi) - lo + 1));
  }
};

// ---------------------------------------------------------------------------
// cameras
// ---------------------------------------------------------------------------

/**
 * A perspective view-projection built in fx16 the way the renderer would.
 *
 * `shear_x`/`shear_y` push the principal point off centre. THEY EXIST FOR ONE
 * REASON: on a symmetric camera, left and right are mirror images and so are
 * top and bottom, which makes a swapped pair of planes produce an IDENTICAL
 * frustum. A symmetric camera can therefore never see that mistake. Off-centre
 * rows break the mirror.
 */
mat4fx make_vp(double fov_deg, double aspect, double eye_z, double shear_x = 0.0,
               double shear_y = 0.0) {
  const double f = 1.0 / std::tan(fov_deg * 3.14159265358979 / 360.0);
  mat4fx m{};
  auto set = [&](int i, int j, double v) {
    double s = v * 65536.0;
    if (s > 2147483647.0) s = 2147483647.0;
    if (s < -2147483648.0) s = -2147483648.0;
    m.m[i][j] = fx16{static_cast<int32_t>(s)};
  };
  set(0, 0, f / aspect);
  set(0, 2, shear_x);
  set(1, 1, f);
  set(1, 2, shear_y);
  set(2, 2, 1.0);   // row 2 is inert: this machine has no z clip
  set(3, 2, -1.0);  // w = eye_z - z, so w > 0 in front of the eye
  set(3, 3, eye_z);
  return m;
}

/** A raw matrix, for driving the extremes the plane widths were sized for. */
mat4fx raw_vp(const int32_t w[16]) {
  mat4fx m{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) m.m[i][j] = fx16{w[i * 4 + j]};
  return m;
}

// ---------------------------------------------------------------------------
// the DUT
// ---------------------------------------------------------------------------

void reset_dut(Vzhao_geom_cull& dut) {
  dut.rst_n = 0;
  dut.cfg_we_i = 0;
  dut.cfg_view_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.tick_i = 0;
  dut.active_i = 0;
  dut.centre_x_i = 0;
  dut.centre_y_i = 0;
  dut.centre_z_i = 0;
  dut.radius_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** Write one view's sixteen matrix words. Each write dirties the view. */
void write_view(Vzhao_geom_cull& dut, int view, const mat4fx& m) {
  for (int k = 0; k < 16; ++k) {
    dut.cfg_we_i = 1;
    dut.cfg_view_i = static_cast<uint8_t>(view);
    dut.cfg_addr_i = static_cast<uint8_t>(k);
    dut.cfg_data_i = static_cast<uint32_t>(m.m[k / 4][k % 4].raw);
    zhao::tick(dut);
  }
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.eval();
}

/** Spin until the block reports it can accept a bound. Returns the cycle count. */
int wait_ready(Vzhao_geom_cull& dut, const char* where) {
  int n = 0;
  while (!dut.ready_o && n < 4000) {
    zhao::tick(dut);
    ++n;
  }
  check(dut.ready_o != 0, where, 1, dut.ready_o);
  return n;
}

/** One instance through the RTL. Must be called with ready_o high. */
zc::Verdict dut_cull(Vzhao_geom_cull& dut, uint8_t active, vec3fx c, fx16 r) {
  dut.active_i = active;
  dut.centre_x_i = c.x.raw;
  dut.centre_y_i = c.y.raw;
  dut.centre_z_i = c.z.raw;
  dut.radius_i = r.raw;
  dut.tick_i = 1;
  zhao::tick(dut);
  dut.tick_i = 0;
  dut.eval();
  int n = 0;
  while (!dut.valid_o && n < 64) {
    zhao::tick(dut);
    ++n;
  }
  zc::Verdict v{};
  v.visible_mask = static_cast<uint8_t>(dut.vis_o);
  v.reject = dut.reject_o != 0;
  if (!dut.valid_o) check(false, "verdict never asserted valid_o", 1, 0);
  return v;
}

/** The whole state a comparison needs: both cameras, extracted once. */
struct Rig {
  zc::View view[2];
};

Rig load(Vzhao_geom_cull& dut, const mat4fx& m0, const mat4fx& m1) {
  write_view(dut, 0, m0);
  write_view(dut, 1, m1);
  wait_ready(dut, "load: block never became ready");
  Rig rig;
  rig.view[0] = zc::make_view(m0);
  rig.view[1] = zc::make_view(m1);
  return rig;
}

/** Drive both, compare the whole verdict. */
void one(Vzhao_geom_cull& dut, const Rig& rig, const char* tag, uint8_t active, vec3fx c, fx16 r) {
  const zc::Verdict want = zc::cull_instance(rig.view, active, c, r);
  const zc::Verdict got = dut_cull(dut, active, c, r);
  char nm[224];
  std::snprintf(nm, sizeof nm, "%s vis act=%u c=(%d,%d,%d) r=%d", tag, active, c.x.raw, c.y.raw,
                c.z.raw, r.raw);
  check(got.visible_mask == want.visible_mask, nm, want.visible_mask, got.visible_mask);
  std::snprintf(nm, sizeof nm, "%s reject act=%u c=(%d,%d,%d) r=%d", tag, active, c.x.raw, c.y.raw,
                c.z.raw, r.raw);
  check(got.reject == want.reject, nm, want.reject, got.reject);
  ++g_cases;
}

// ---------------------------------------------------------------------------
// the shipped renderer's own visibility verdict, for section 3
// ---------------------------------------------------------------------------

/**
 * Is this point inside the volume the renderer draws?
 *
 * `w > 0` comes from `zref::render::project_vertex` DIRECTLY — `o.in` is that
 * function's own near rejection, not a copy of it. The NDC range comes from the
 * same function's next two lines, evaluated with the same shipped
 * `zref::fx_div_exact`; project_vertex's own comment states the mapping it then
 * performs as "ndc -1..+1 -> [x0, x0+w)".
 */
bool shipped_visible(const mat4fx& m, vec3fx p) {
  zref::SatLedger* L = nullptr;
  const zref::render::Viewport vp{0, 0, 640, 480};
  const zref::render::ProjOut o = zref::render::project_vertex(m, vp, p.x, p.y, p.z, L);
  if (!o.in) return false;  // w <= 0, the renderer's own near cull
  const vec4fx clip = zref::mat4_vec4(m, vec4fx{p.x, p.y, p.z, fx16{ONE}}, L);
  const fx16 nx = zref::fx_div_exact(clip.x, clip.w, L);
  const fx16 ny = zref::fx_div_exact(clip.y, clip.w, L);
  return (nx.raw >= -ONE) && (nx.raw <= ONE) && (ny.raw >= -ONE) && (ny.raw <= ONE);
}

/** The exact clip-space form, from the shipped row product. No division, no rounding. */
bool clip_visible(const mat4fx& m, vec3fx p) {
  zref::SatLedger* L = nullptr;
  const vec4fx clip = zref::mat4_vec4(m, vec4fx{p.x, p.y, p.z, fx16{ONE}}, L);
  if (clip.w.raw <= 0) return false;
  const int64_t w = clip.w.raw;
  return (clip.x.raw >= -w) && (clip.x.raw <= w) && (clip.y.raw >= -w) && (clip.y.raw <= w);
}

/** Does the plane set admit this point? */
bool planes_admit(const zc::View& v, vec3fx p) {
  for (int k = 0; k < zc::kPlaneCount; ++k) {
    if (zc::plane_dot(v.plane[k], p) < 0) return false;
  }
  return true;
}

/** The same rejection with a FLOOR length bound — the mistake, for section 5. */
bool view_rejects_floor(const zc::View& v, vec3fx c, fx16 r) {
  for (int k = 0; k < zc::kPlaneCount; ++k) {
    const uint64_t lo = zc::cull_isqrt(zc::normal_sumsq(v.plane[k]));
    const __int128 slack = static_cast<__int128>(r.raw) * static_cast<__int128>(lo);
    if (zc::plane_dot(v.plane[k], c) < -slack) return true;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_geom_cull dut;
  reset_dut(dut);
  Prng rng(0xC0117EEDu);

  // ---- 1. THE WIDENED SQUARE ROOT IS THE RATIFIED ONE ----------------------
  // `zref::cull::cull_isqrt` is qformats §7.2's recurrence with the starting
  // bit moved from 4^31 to 4^32 so the argument may reach 3*2^64. A widening
  // that quietly disagreed with `zref::isqrt_u64` would move every length bound
  // by an LSB in an unanalysed direction, so it is checked rather than claimed:
  // exhaustively at the small end, and randomly across the whole u64 range.
  {
    for (uint64_t n = 0; n < 100000; ++n) {
      if (zc::cull_isqrt(n) != zref::isqrt_u64(n)) {
        check(false, "cull_isqrt disagrees with isqrt_u64 (small)", zref::isqrt_u64(n),
              zc::cull_isqrt(n));
        break;
      }
    }
    int bad = 0;
    for (int i = 0; i < 200000; ++i) {
      uint64_t n = rng.next() >> (rng.next() % 64);
      if (zc::cull_isqrt(n) != zref::isqrt_u64(n)) ++bad;
    }
    check(bad == 0, "cull_isqrt disagrees with isqrt_u64 (u64 range)", 0,
          static_cast<uint64_t>(bad));

    // ABOVE u64 there is nothing shipped to compare against, so the DEFINING
    // property is checked instead: res^2 <= n < (res+1)^2, over the range a
    // plane's sum of squares actually reaches (3*2^64).
    int bad_hi = 0;
    for (int i = 0; i < 200000; ++i) {
      unsigned __int128 n = static_cast<unsigned __int128>(rng.next()) +
                            (static_cast<unsigned __int128>(rng.below(3)) << 64);
      const unsigned __int128 r = zc::cull_isqrt(n);
      if (!(r * r <= n && (r + 1) * (r + 1) > n)) ++bad_hi;
    }
    // and the exact extreme: a plane whose every component is the 2^32 rail
    {
      const zc::Plane p{static_cast<int64_t>(1) << 32, static_cast<int64_t>(1) << 32,
                        static_cast<int64_t>(1) << 32, 0};
      const unsigned __int128 n = zc::normal_sumsq(p);
      const unsigned __int128 r = zc::cull_isqrt(n);
      if (!(r * r <= n && (r + 1) * (r + 1) > n)) ++bad_hi;
      if (zc::normal_len_ceil(p) != static_cast<uint64_t>(r) + 1) ++bad_hi;
    }
    check(bad_hi == 0, "cull_isqrt violates res^2 <= n < (res+1)^2 above u64", 0,
          static_cast<uint64_t>(bad_hi));
    std::printf("geom_cull 1: isqrt widening pinned to isqrt_u64 over u64, property above it\n");
  }

  // The camera set. Symmetric ones for the ordinary case; SHEARED ones because
  // a symmetric camera cannot distinguish a swapped left/right or top/bottom
  // pair — the frustum is literally the same set of planes.
  const mat4fx cam[6] = {
      make_vp(60.0, 4.0 / 3.0, 40.0),           make_vp(90.0, 1.0, 12.0),
      make_vp(35.0, 16.0 / 9.0, 200.0),         make_vp(60.0, 4.0 / 3.0, 40.0, 0.35, 0.0),
      make_vp(60.0, 4.0 / 3.0, 40.0, 0.0, 0.5), make_vp(75.0, 1.6, 90.0, -0.4, 0.25),
  };

  // ---- 2/3. THE PLANES DESCRIBE THE VOLUME THE SHIPPED RENDERER DRAWS ------
  // The kind-1 half. Nothing about spheres yet: a POINT is admitted by all five
  // planes exactly when the renderer would draw it. Both forms of the shipped
  // verdict are used — the exact clip-space one, and the end-to-end one that
  // goes through `project_vertex` and `fx_div_exact`.
  {
    int checked = 0, mismatch_exact = 0, mismatch_shipped = 0, lost = 0;
    for (const mat4fx& m : cam) {
      const zc::View v = zc::make_view(m);
      for (int i = 0; i < 60000; ++i) {
        const vec3fx p{fx16{rng.range(-60 * ONE, 60 * ONE)}, fx16{rng.range(-60 * ONE, 60 * ONE)},
                       fx16{rng.range(-60 * ONE, 60 * ONE)}};
        const bool by_planes = planes_admit(v, p);
        if (by_planes != clip_visible(m, p)) ++mismatch_exact;
        // The one-sided claim that actually protects geometry: nothing the
        // planes refuse may be something the renderer would have drawn.
        if (!by_planes && shipped_visible(m, p)) ++lost;
        if (by_planes != shipped_visible(m, p)) ++mismatch_shipped;
        ++checked;
      }
    }
    check(mismatch_exact == 0, "planes disagree with the exact clip volume", 0,
          static_cast<uint64_t>(mismatch_exact));
    check(lost == 0, "a point the planes REJECT is one project_vertex would draw", 0,
          static_cast<uint64_t>(lost));
    std::printf(
        "geom_cull 2/3: %d points, exact-clip mismatches %d, project_vertex mismatches %d "
        "(of which geometry-losing %d)\n",
        checked, mismatch_exact, mismatch_shipped, lost);
  }

  // ---- 4. THE ORDINARY DIFFERENTIAL, on all six cameras --------------------
  {
    for (int a = 0; a < 3; ++a) {
      const Rig rig = load(dut, cam[a], cam[(a + 3) % 6]);
      for (int i = 0; i < 400; ++i) {
        const vec3fx c{fx16{rng.range(-60 * ONE, 60 * ONE)}, fx16{rng.range(-60 * ONE, 60 * ONE)},
                       fx16{rng.range(-60 * ONE, 60 * ONE)}};
        const fx16 r{rng.range(0, 8 * ONE)};
        one(dut, rig, "4.pair", static_cast<uint8_t>(rng.below(4)), c, r);
      }
    }
  }

  // ---- 5. THE CEILING BOUNDARY, WHICH IS THE WHOLE ARGUMENT ----------------
  // The bound must be a CEILING: a floor makes rejection easier and deletes
  // visible geometry near the screen edges. That direction is only observable
  // in a narrow band of radii, so the band is SEARCHED for rather than sampled
  // — for a centre outside the frustum, rejection is monotone in r (a bigger
  // sphere is harder to reject), so a bisection lands exactly on the switch.
  //
  // The number printed is the count of probes at which a FLOOR bound would have
  // answered differently. If that were zero this section would prove nothing,
  // so it is asserted to be positive: the test can SEE the rounding direction.
  {
    int probes = 0, discriminating = 0;
    for (int a = 0; a < 6; ++a) {
      const Rig rig = load(dut, cam[a], cam[(a + 1) % 6]);
      for (int i = 0; i < 120; ++i) {
        // a centre well outside, so that r = 0 rejects
        const vec3fx c{fx16{rng.range(-60 * ONE, 60 * ONE)}, fx16{rng.range(-60 * ONE, 60 * ONE)},
                       fx16{rng.range(-60 * ONE, 60 * ONE)}};
        if (!zc::view_rejects(rig.view[0], c, fx16{0})) continue;
        int32_t lo = 0, hi = 2000 * ONE;  // rejects at lo, may not at hi
        if (zc::view_rejects(rig.view[0], c, fx16{hi})) continue;
        while (hi - lo > 1) {
          const int32_t mid = lo + (hi - lo) / 2;
          if (zc::view_rejects(rig.view[0], c, fx16{mid}))
            lo = mid;
          else
            hi = mid;
        }
        // hi is the smallest radius that is NOT rejected; straddle it
        for (int32_t d = -2; d <= 2; ++d) {
          const int32_t rr = hi + d;
          if (rr < 0) continue;
          one(dut, rig, "5.ceil-edge", 0x1, c, fx16{rr});
          ++probes;
          if (zc::view_rejects(rig.view[0], c, fx16{rr}) !=
              view_rejects_floor(rig.view[0], c, fx16{rr})) {
            ++discriminating;
          }
        }
      }
    }
    check(discriminating > 0, "no probe distinguishes the ceiling bound from a floor", 1,
          static_cast<uint64_t>(discriminating));
    std::printf("geom_cull 5: %d boundary probes, %d distinguish ceiling from floor\n", probes,
                discriminating);
  }

  // ---- 5b. dot == -r*len EXACTLY, which is the only place `<` differs from
  // `<=` -------------------------------------------------------------------
  // Section 5 finds the radius at which rejection switches, but it cannot make
  // the two sides EQUAL: that needs the plane's length to divide the dot
  // product, which never happens by accident. So the camera is BUILT to make it
  // happen. With row0 = (n,0,0,0) and row3 = (0,0,0,k) the left plane is
  // (n,0,0,k), its normal is (n,0,0) whose length is exactly n (a perfect
  // square, so the ceiling is the length itself), and for a centre on the x
  // axis the test reduces to
  //
  //     n*cx + k*2^16  <  -r*n      which with n = 2^16 is    cx + k < -r
  //
  // so cx = -r-k lands exactly on the boundary. Without this, a `<` widened to
  // `<=` differs on nothing a random sweep would ever draw.
  {
    for (int32_t k : {0, 1, 5, -3, 100}) {
      mat4fx m{};
      m.m[0][0] = fx16{ONE};  // row0 = (1,0,0,0) in fx16
      m.m[3][3] = fx16{k};    // row3 = (0,0,0,k)
      const Rig rig = load(dut, m, m);
      for (int32_t r : {0, 1, 2, 1000, ONE, 7 * ONE}) {
        for (int32_t d = -2; d <= 2; ++d) {
          const int32_t cx = -r - k + d;
          one(dut, rig, "5b.exact", 0x3, vec3fx{fx16{cx}, fx16{0}, fx16{0}}, fx16{r});
          // and the right plane's mirror, cx = +r-k
          const int32_t cx2 = r - k + d;
          one(dut, rig, "5b.exact", 0x3, vec3fx{fx16{cx2}, fx16{0}, fx16{0}}, fx16{r});
        }
      }
    }
  }

  // ---- 6. THE TWO-CAMERA LAW ----------------------------------------------
  // Reject only when the sphere is outside EVERY active camera. Two cameras
  // aimed opposite ways make "visible in exactly one" reachable, and each case
  // is driven under all four active masks.
  {
    const mat4fx front = make_vp(60.0, 4.0 / 3.0, 40.0);
    mat4fx back = make_vp(60.0, 4.0 / 3.0, 40.0);
    // flip the w row: this camera looks the other way, so w > 0 behind
    back.m[3][2] = fx16{ONE};
    back.m[3][3] = fx16{40 * ONE};
    const Rig rig = load(dut, front, back);

    const vec3fx pts[6] = {
        {fx16{0}, fx16{0}, fx16{0}},           // in front, both may see
        {fx16{0}, fx16{0}, fx16{-100 * ONE}},  // far behind
        {fx16{0}, fx16{0}, fx16{100 * ONE}},   // far in front of the flipped one
        {fx16{50 * ONE}, fx16{0}, fx16{0}},    // off to one side
        {fx16{0}, fx16{50 * ONE}, fx16{0}},    // above/below — the +Y question
        {fx16{0}, fx16{-50 * ONE}, fx16{0}},
    };
    for (const vec3fx& p : pts) {
      for (uint8_t act = 0; act < 4; ++act) {
        for (int32_t r : {0, ONE, 20 * ONE}) {
          one(dut, rig, "6.duo", act, p, fx16{r});
        }
      }
    }
    // The ruled law, stated as its own check rather than left implicit in the
    // verdict comparison: a sphere both cameras can see is never rejected, and
    // rejection requires BOTH to refuse it.
    for (int i = 0; i < 3000; ++i) {
      const vec3fx c{fx16{rng.range(-80 * ONE, 80 * ONE)}, fx16{rng.range(-80 * ONE, 80 * ONE)},
                     fx16{rng.range(-80 * ONE, 80 * ONE)}};
      const fx16 r{rng.range(0, 4 * ONE)};
      const bool out0 = zc::view_rejects(rig.view[0], c, r);
      const bool out1 = zc::view_rejects(rig.view[1], c, r);
      const zc::Verdict got = dut_cull(dut, 0x3, c, r);
      check(got.reject == (out0 && out1), "6.law reject iff outside BOTH active cameras",
            static_cast<uint64_t>(out0 && out1), static_cast<uint64_t>(got.reject));
      ++g_cases;
    }
  }

  // ---- 7. THE PROTOCOL, because a stale length bound deletes geometry ------
  // The dirty bit is the only thing standing between a matrix write and a cull
  // evaluated against the previous camera's length bounds. Drive it.
  {
    const mat4fx a = make_vp(60.0, 4.0 / 3.0, 40.0);
    const mat4fx b = make_vp(20.0, 1.0, 300.0, 0.6, -0.3);

    // a write must make the block NOT ready on the very next cycle
    load(dut, a, a);
    dut.cfg_we_i = 1;
    dut.cfg_view_i = 0;
    dut.cfg_addr_i = 0;
    dut.cfg_data_i = static_cast<uint32_t>(b.m[0][0].raw);
    dut.eval();
    check(dut.ready_o == 0, "7.ready is refused in the cycle a matrix word is written", 0,
          dut.ready_o);
    zhao::tick(dut);
    dut.cfg_we_i = 0;
    dut.eval();
    check(dut.ready_o == 0, "7.ready stays low while a view is dirty", 0, dut.ready_o);

    // a write to addr 16 (GEOM.PROJECT's viewport) is inert here
    wait_ready(dut, "7.block never re-readied after the dirtying write");
    dut.cfg_we_i = 1;
    dut.cfg_addr_i = 16;
    dut.cfg_data_i = 0x12345678u;
    dut.eval();
    check(dut.ready_o == 1, "7.a viewport write does not dirty the cull", 1, dut.ready_o);
    zhao::tick(dut);
    dut.cfg_we_i = 0;
    dut.cfg_addr_i = 0;
    dut.eval();
    check(dut.ready_o == 1, "7.a viewport write left no dirt behind", 1, dut.ready_o);

    // and the verdicts before and after a camera change must follow the camera
    const Rig ra = load(dut, a, a);
    const vec3fx probe{fx16{18 * ONE}, fx16{7 * ONE}, fx16{5 * ONE}};
    one(dut, ra, "7.before", 0x3, probe, fx16{ONE});
    const Rig rb = load(dut, b, b);
    one(dut, rb, "7.after", 0x3, probe, fx16{ONE});

    // extraction takes the same time for either view, and both views must be
    // extracted when both are written
    const Rig rab = load(dut, a, b);
    one(dut, rab, "7.split", 0x1, probe, fx16{ONE});
    one(dut, rab, "7.split", 0x2, probe, fx16{ONE});
    one(dut, rab, "7.split", 0x3, probe, fx16{ONE});
  }

  // ---- 8. THE PLANE COMPONENT DOES NOT FIT IN 32 BITS ----------------------
  // A component is row3[j] +/- rowk[j], so it reaches 2^32 and the sum of
  // squares reaches 3*2^64 — outside u64, which is the whole reason the
  // recurrence was widened. Rails and near-rails drive that path; a 32-bit
  // truncation anywhere wraps here and nowhere else.
  {
    const int32_t rails[16] = {INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN, INT32_MIN, INT32_MAX,
                               INT32_MIN, INT32_MAX, 0,         0,         ONE,       0,
                               INT32_MAX, INT32_MAX, INT32_MIN, INT32_MAX};
    const int32_t near_rails[16] = {
        INT32_MAX - 1, 1, INT32_MIN + 1, -1, 1,         INT32_MAX, -1, INT32_MIN + 3, 0, 0,
        ONE,           0, INT32_MIN,     -3, INT32_MAX, 7};
    const Rig rig = load(dut, raw_vp(rails), raw_vp(near_rails));
    for (int32_t v : {0, 1, -1, ONE, -ONE, 1000 * ONE, -1000 * ONE, INT32_MAX, INT32_MIN + 1}) {
      for (int32_t r : {0, 1, ONE, 1000 * ONE, INT32_MAX}) {
        one(dut, rig, "8.rails", 0x3, vec3fx{fx16{v}, fx16{v}, fx16{v}}, fx16{r});
        one(dut, rig, "8.rails", 0x3, vec3fx{fx16{v}, fx16{0}, fx16{-v}}, fx16{r});
      }
    }
    for (int i = 0; i < 600; ++i) {
      const vec3fx c{fx16{static_cast<int32_t>(rng.next())}, fx16{static_cast<int32_t>(rng.next())},
                     fx16{static_cast<int32_t>(rng.next())}};
      const fx16 r{static_cast<int32_t>(rng.next() >> 33)};  // non-negative
      one(dut, rig, "8.rails-random", static_cast<uint8_t>(rng.below(4)), c, r);
    }
  }

  // ---- 9. DEGENERATE CAMERAS ----------------------------------------------
  // An all-zero matrix makes every plane the zero vector: every dot product is
  // zero, every length ceiling is zero, and nothing is ever outside. The
  // reference and the RTL must agree that such a camera rejects NOTHING —
  // a zero-length normal is the one case where the ceiling and the floor and
  // the true length are all the same number.
  {
    mat4fx zero{};
    mat4fx near_only{};
    near_only.m[3][3] = fx16{ONE};  // w = 1 everywhere: everything is in front
    const Rig rig = load(dut, zero, near_only);
    for (int32_t v : {0, ONE, -ONE, 1000 * ONE}) {
      for (int32_t r : {0, ONE, 1000 * ONE}) {
        one(dut, rig, "9.degenerate", 0x3, vec3fx{fx16{v}, fx16{v}, fx16{v}}, fx16{r});
        one(dut, rig, "9.degenerate", 0x1, vec3fx{fx16{v}, fx16{v}, fx16{v}}, fx16{r});
        one(dut, rig, "9.degenerate", 0x2, vec3fx{fx16{v}, fx16{v}, fx16{v}}, fx16{r});
        one(dut, rig, "9.degenerate", 0x0, vec3fx{fx16{v}, fx16{v}, fx16{v}}, fx16{r});
      }
    }
  }

  // ---- 10. r = 0 IS A POINT, AND MUST MATCH THE POINT TEST -----------------
  // With r = 0 the slack vanishes and the sphere test degenerates to the plane
  // test on the centre. That ties sections 2/3 (points, against the shipped
  // renderer) to sections 4 onward (spheres, against the header) — the two
  // halves of this file meet exactly here.
  {
    const Rig rig = load(dut, cam[3], cam[5]);
    int agree = 0;
    for (int i = 0; i < 2000; ++i) {
      const vec3fx c{fx16{rng.range(-60 * ONE, 60 * ONE)}, fx16{rng.range(-60 * ONE, 60 * ONE)},
                     fx16{rng.range(-60 * ONE, 60 * ONE)}};
      one(dut, rig, "10.point", 0x3, c, fx16{0});
      const zc::Verdict got = dut_cull(dut, 0x1, c, fx16{0});
      const bool by_shipped = clip_visible(cam[3], c);
      check((got.visible_mask & 1u) != 0u ? by_shipped : !by_shipped,
            "10.r=0 disagrees with the shipped clip volume", static_cast<uint64_t>(by_shipped),
            static_cast<uint64_t>((got.visible_mask & 1u) != 0u));
      if (((got.visible_mask & 1u) != 0u) == by_shipped) ++agree;
      ++g_cases;
    }
    std::printf("geom_cull 10: %d zero-radius verdicts matched the shipped clip volume\n", agree);
  }

  // ---- 11. random over the whole domain ------------------------------------
  if (random_iters > 0) {
    Rig rig = load(dut, cam[0], cam[3]);
    for (int it = 0; it < random_iters; ++it) {
      if ((it % 500) == 0) {
        // change the cameras occasionally — extraction is 185 cycles a view, so
        // this is deliberately rare rather than per iteration
        const mat4fx& m0 = cam[rng.below(6)];
        const mat4fx& m1 = cam[rng.below(6)];
        rig = load(dut, m0, m1);
      }
      const vec3fx c{fx16{rng.range(-200 * ONE, 200 * ONE)}, fx16{rng.range(-200 * ONE, 200 * ONE)},
                     fx16{rng.range(-200 * ONE, 200 * ONE)}};
      const fx16 r{rng.range(0, 40 * ONE)};
      one(dut, rig, "11.random", static_cast<uint8_t>(rng.below(4)), c, r);
    }
    std::printf("geom_cull random: %d instances\n", random_iters);
  }

  std::printf("geom_cull: %d instance verdicts compared\n", g_cases);
  return zhao::report_and_exit("geom_cull_directed");
}
