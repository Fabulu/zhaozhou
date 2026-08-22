// cull_check.cpp — validate the GEOM.MESHFETCH cull derivation BEFORE any RTL.
//
// Two claims are being checked, both of which would be expensive to discover
// later:
//
//   1. THE FIVE PLANES ARE RIGHT. A point is inside the visible volume exactly
//      when all five plane dot-products are >= 0. The volume is defined by
//      project_vertex's own conditions: w > 0, -w <= x <= w, -w <= y <= w.
//      Checked against the direct clip-space test over random points.
//
//   2. THE SPHERE TEST IS CONSERVATIVE. It may reject only spheres that contain
//      NO visible point. The dangerous direction is rejecting something visible,
//      which deletes geometry rather than costing performance — so this samples
//      each sphere densely and asserts that anything rejected really was empty.
//      It also reports how often the test is merely loose (kept, but empty),
//      which is the acceptable failure.
//
// Header-only on purpose: mat4fx / mat4_vec4 are constexpr in zref_fixp.hpp and
// isqrt_u64 is constexpr in zref_trig.hpp, so this needs no build system and
// cannot disturb the repo's cmake tree.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

using zref::fx16;
using zref::mat4fx;
using zref::vec4fx;

namespace {

constexpr int32_t ONE = 1 << 16;

struct Plane {
  int64_t a, b, c, d;  // fx16 components, kept as int64 for headroom
};

/** The five planes, as row combinations of vp (see the design note). */
void extract(const mat4fx& m, Plane out[5]) {
  auto R = [&](int i, int j) { return static_cast<int64_t>(m.m[i][j].raw); };
  // left = row3 + row0
  out[0] = {R(3, 0) + R(0, 0), R(3, 1) + R(0, 1), R(3, 2) + R(0, 2), R(3, 3) + R(0, 3)};
  // right = row3 - row0
  out[1] = {R(3, 0) - R(0, 0), R(3, 1) - R(0, 1), R(3, 2) - R(0, 2), R(3, 3) - R(0, 3)};
  // bottom = row3 + row1
  out[2] = {R(3, 0) + R(1, 0), R(3, 1) + R(1, 1), R(3, 2) + R(1, 2), R(3, 3) + R(1, 3)};
  // top = row3 - row1
  out[3] = {R(3, 0) - R(1, 0), R(3, 1) - R(1, 1), R(3, 2) - R(1, 2), R(3, 3) - R(1, 3)};
  // near = row3   (w > 0)
  out[4] = {R(3, 0), R(3, 1), R(3, 2), R(3, 3)};
}

/** dot(plane, point) in fx16 world coords, exact in s128, returned scaled by 2^16. */
__int128 plane_dot(const Plane& p, int32_t x, int32_t y, int32_t z) {
  return static_cast<__int128>(p.a) * x + static_cast<__int128>(p.b) * y +
         static_cast<__int128>(p.c) * z + (static_cast<__int128>(p.d) << 16);
}

/** The direct clip-space visibility test, straight from project_vertex. */
bool visible_direct(const mat4fx& m, int32_t x, int32_t y, int32_t z) {
  zref::SatLedger* L = nullptr;
  const vec4fx clip = zref::mat4_vec4(m, vec4fx{fx16{x}, fx16{y}, fx16{z}, fx16{ONE}}, L);
  if (clip.w.raw <= 0) return false;
  const int64_t w = clip.w.raw;
  return (clip.x.raw >= -w) && (clip.x.raw <= w) && (clip.y.raw >= -w) && (clip.y.raw <= w);
}

/** CEIL of the length of the plane normal — the safe bound (see the design note). */
uint64_t normal_len_ceil(const Plane& p) {
  const __int128 sq = static_cast<__int128>(p.a) * p.a + static_cast<__int128>(p.b) * p.b +
                      static_cast<__int128>(p.c) * p.c;
  if (sq <= 0) return 0;
  // sum of squares can exceed u64 for large fx16 rows; clamp the check to the
  // range isqrt_u64 accepts and report if we ever needed more.
  if (sq > static_cast<__int128>(UINT64_MAX)) return UINT64_MAX;
  const uint64_t lo = zref::isqrt_u64(static_cast<uint64_t>(sq));
  return (static_cast<__int128>(lo) * lo < sq) ? lo + 1 : lo;
}

/** Reject iff the sphere lies wholly outside at least one plane. */
bool reject_sphere(const Plane pl[5], int32_t cx, int32_t cy, int32_t cz, int32_t r) {
  for (int i = 0; i < 5; ++i) {
    const uint64_t len = normal_len_ceil(pl[i]);
    const __int128 slack = static_cast<__int128>(r) * static_cast<__int128>(len);
    if (plane_dot(pl[i], cx, cy, cz) < -slack) return true;
  }
  return false;
}

struct Prng {
  uint64_t s = 0x243F6A8885A308D3ull;
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  int32_t range(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(next() % static_cast<uint64_t>(hi - lo + 1));
  }
};

/** A plain perspective view-projection, built in fx16 the way the renderer would. */
mat4fx make_vp(double fov_deg, double aspect, double eye_z) {
  const double f = 1.0 / std::tan(fov_deg * 3.14159265358979 / 360.0);
  mat4fx m{};
  auto set = [&](int i, int j, double v) {
    double s = v * 65536.0;
    if (s > 2147483647.0) s = 2147483647.0;
    if (s < -2147483648.0) s = -2147483648.0;
    m.m[i][j] = fx16{static_cast<int32_t>(s)};
  };
  // rows: x, y, z(unused by the cull), w = -(z - eye_z) so that w > 0 in front
  set(0, 0, f / aspect);
  set(1, 1, f);
  set(2, 2, 1.0);
  set(3, 2, -1.0);
  set(3, 3, eye_z);
  return m;
}

}  // namespace

int main() {
  int fail_planes = 0, checked_planes = 0;
  int fail_conservative = 0, rejected = 0, kept_but_empty = 0, checked_spheres = 0;

  const mat4fx cams[3] = {make_vp(60.0, 4.0 / 3.0, 40.0), make_vp(90.0, 1.0, 12.0),
                          make_vp(35.0, 16.0 / 9.0, 200.0)};
  Prng rng;

  for (const mat4fx& m : cams) {
    Plane pl[5];
    extract(m, pl);

    // ---- claim 1: the planes describe exactly the clip volume --------------
    for (int i = 0; i < 200000; ++i) {
      const int32_t x = rng.range(-60 * ONE, 60 * ONE);
      const int32_t y = rng.range(-60 * ONE, 60 * ONE);
      const int32_t z = rng.range(-60 * ONE, 60 * ONE);
      bool by_planes = true;
      for (int k = 0; k < 5; ++k) {
        if (plane_dot(pl[k], x, y, z) < 0) {
          by_planes = false;
          break;
        }
      }
      const bool by_clip = visible_direct(m, x, y, z);
      ++checked_planes;
      if (by_planes != by_clip) {
        if (fail_planes < 5) {
          std::printf("PLANE MISMATCH at (%d,%d,%d): planes=%d clip=%d\n", x, y, z,
                      static_cast<int>(by_planes), static_cast<int>(by_clip));
        }
        ++fail_planes;
      }
    }

    // ---- claim 2: rejection is conservative --------------------------------
    for (int i = 0; i < 20000; ++i) {
      const int32_t cx = rng.range(-60 * ONE, 60 * ONE);
      const int32_t cy = rng.range(-60 * ONE, 60 * ONE);
      const int32_t cz = rng.range(-60 * ONE, 60 * ONE);
      const int32_t r = rng.range(0, 8 * ONE);
      const bool rej = reject_sphere(pl, cx, cy, cz, r);
      // sample the sphere: centre plus a lattice of surface/interior points
      bool any_visible = false;
      for (int s = 0; s < 64 && !any_visible; ++s) {
        const double u = (rng.next() % 10001) / 10000.0 * 2.0 - 1.0;
        const double t = (rng.next() % 10001) / 10000.0 * 6.28318530718;
        const double rr = std::cbrt((rng.next() % 10001) / 10000.0) * r;
        const double sx = rr * std::sqrt(1 - u * u) * std::cos(t);
        const double sy = rr * std::sqrt(1 - u * u) * std::sin(t);
        const double sz = rr * u;
        any_visible = visible_direct(m, cx + static_cast<int32_t>(sx),
                                     cy + static_cast<int32_t>(sy),
                                     cz + static_cast<int32_t>(sz));
      }
      if (!any_visible) any_visible = visible_direct(m, cx, cy, cz);
      ++checked_spheres;
      if (rej && any_visible) {
        if (fail_conservative < 5) {
          std::printf("NOT CONSERVATIVE: rejected sphere c=(%d,%d,%d) r=%d that has a visible point\n",
                      cx, cy, cz, r);
        }
        ++fail_conservative;
      }
      if (rej) ++rejected;
      if (!rej && !any_visible) ++kept_but_empty;
    }
  }

  std::printf("\nplane equivalence : %d checked, %d mismatches\n", checked_planes, fail_planes);
  std::printf("conservatism      : %d spheres, %d rejected, %d WRONGLY rejected\n", checked_spheres,
              rejected, fail_conservative);
  std::printf("looseness         : %d kept although empty (acceptable: costs work, not geometry)\n",
              kept_but_empty);
  return (fail_planes || fail_conservative) ? 1 : 0;
}
