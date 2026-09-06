// manafold-bandprobe -- the COMMITTED antenna-band cross-section probe.
//
// WHY THIS EXISTS (pass 8, Direction 5 SS2b / pass-7 review fault 1).
// Pass 7 shipped an antenna the by-eye reviewer called "a uniform strap with
// mitred corners", while manafold_art.h carried five named knuckle swells that
// arithmetic said should nearly double the band. Two stories, no instrument.
// This probe settles it by reading the COMPILED MESH -- not the constants, not
// a rendered frame -- and printing the band's actual half-width per ring.
//
// It measures the thing that IS the thing: for every ring of the loop chain it
// reports max|x - cx| and max|z| over that ring's own bind vertices. That is
// the silhouette half-width the renderer will draw in the loop plane (x) and
// across it (z). A knuckle is a local maximum in that series; a uniform strap
// is a flat line. CLAUDE.md: measurement belongs on the COMPARISON side.
//
// HOW ITS EARLIER FORM WOULD HAVE LIED: reading kLoopBladeR*Mm and the
// kKnuckleSwell* constants and doing the arithmetic in a comment. That is
// exactly what pass 7 did, and the render disagreed. Rings are grouped by
// their EXACT bind y, so meshlet splits (which duplicate a seam ring) and cap
// fans (whose apex sits on the axis) cannot smear two rings together.
//
//   manafold-bandprobe            -- print the profile, exit 0
//   manafold-bandprobe --selftest -- prove the probe can FAIL (see below)
//
// SELFTEST: it rebuilds the same grouping over a SYNTHETIC ring stack with a
// planted bulge and asserts the bulge is found at the right station and that a
// flat stack reports flat. A probe that has never returned "no knuckles" on a
// strap and "knuckles" on a knuckled band has not been shown to work.

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace zc = zref::creature;
#include "manafold.h"

namespace {

constexpr int32_t kFx = 65536;

struct Row {
  int32_t y_mm;
  int32_t half_x_mm;
  int32_t half_z_mm;
  int n;
};

// Group a meshlet set's vertices by exact bind y and report the ring profile.
// `cx_mm` is subtracted from x so an offset chain is measured about its own
// centreline rather than about the creature origin.
std::vector<Row> profile(const std::vector<zc::Meshlet>& mesh, size_t first,
                         size_t last, int32_t cx_fx) {
  std::map<int32_t, Row> by_y;
  for (size_t m = first; m <= last && m < mesh.size(); ++m) {
    for (const auto& v : mesh[m].verts) {
      Row& r = by_y[v.y];
      const int32_t dx = v.x - cx_fx;
      const int32_t ax = dx < 0 ? -dx : dx;
      const int32_t az = v.z < 0 ? -v.z : v.z;
      if (r.n == 0) r.y_mm = static_cast<int32_t>((static_cast<int64_t>(v.y) * 1000) / kFx);
      r.half_x_mm = std::max(r.half_x_mm,
                             static_cast<int32_t>((static_cast<int64_t>(ax) * 1000) / kFx));
      r.half_z_mm = std::max(r.half_z_mm,
                             static_cast<int32_t>((static_cast<int64_t>(az) * 1000) / kFx));
      ++r.n;
    }
  }
  std::vector<Row> out;
  for (const auto& kv : by_y) out.push_back(kv.second);
  return out;
}

int selftest() {
  // A synthetic stack: 20 rings, flat half-width 60, with a planted bulge of
  // +50 at ring 10. Fed through the SAME grouping code path.
  std::vector<zc::Meshlet> fake(1);
  for (int i = 0; i < 20; ++i) {
    const int32_t hw = 60 + (i == 10 ? 50 : 0);
    for (int k = 0; k < 4; ++k) {
      zc::SkinVertex v{};
      v.y = static_cast<int32_t>((static_cast<int64_t>(i) * 100 * kFx) / 1000);
      v.x = (k == 0) ? static_cast<int32_t>((static_cast<int64_t>(hw) * kFx) / 1000)
                     : ((k == 1) ? -static_cast<int32_t>((static_cast<int64_t>(hw) * kFx) / 1000) : 0);
      v.z = (k == 2) ? static_cast<int32_t>((static_cast<int64_t>(hw) * kFx) / 1000) : 0;
      fake[0].verts.push_back(v);
    }
  }
  std::vector<Row> p = profile(fake, 0, 0, 0);
  if (p.size() != 20) { std::printf("selftest FAIL: %zu rings, want 20\n", p.size()); return 1; }
  if (p[10].half_x_mm < 105 || p[10].half_x_mm > 115) {
    std::printf("selftest FAIL: planted bulge read %d, want ~110\n", p[10].half_x_mm);
    return 1;
  }
  if (p[5].half_x_mm < 55 || p[5].half_x_mm > 65) {
    std::printf("selftest FAIL: flat band read %d, want ~60\n", p[5].half_x_mm);
    return 1;
  }
  // and the FAILURE direction: a flat stack must NOT report a knuckle.
  for (auto& v : fake[0].verts) {
    const int32_t hw = 60;
    if (v.x > 0) v.x = static_cast<int32_t>((static_cast<int64_t>(hw) * kFx) / 1000);
    if (v.x < 0) v.x = -static_cast<int32_t>((static_cast<int64_t>(hw) * kFx) / 1000);
  }
  p = profile(fake, 0, 0, 0);
  int32_t lo = p[0].half_x_mm, hi = p[0].half_x_mm;
  for (const auto& r : p) { lo = std::min(lo, r.half_x_mm); hi = std::max(hi, r.half_x_mm); }
  if (hi - lo > 2) { std::printf("selftest FAIL: flat stack reported %d..%d\n", lo, hi); return 1; }
  std::printf("bandprobe selftest: OK (bulge found, flat stack flat)\n");
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) return selftest();

  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) { std::printf("bandprobe: FAIL no meshlets\n"); return 1; }

  // Which meshlets belong to the loop? Parts are pushed in manafold.h order:
  // body, LOOP, lensL, lensR, star x4. The loop is the only CHAIN part and the
  // only one whose vertices carry two DIFFERENT bones, so identify it that way
  // rather than by index -- an index would rot the day a part is inserted.
  size_t first = static_cast<size_t>(-1), last = 0;
  for (size_t m = 0; m < T.mesh.size(); ++m) {
    bool blended = false;
    for (const auto& v : T.mesh[m].verts)
      if (v.b0 != v.b1 && v.w0 != 64) { blended = true; break; }
    if (blended) { if (first == static_cast<size_t>(-1)) first = m; last = m; }
  }
  if (first == static_cast<size_t>(-1)) {
    std::printf("bandprobe: FAIL no chain meshlet found (b0!=b1 and w0!=64)\n");
    return 1;
  }
  const int32_t cx_fx = static_cast<int32_t>(
      (static_cast<int64_t>(u02::kLoopTubeXMm) * kFx) / 1000);
  std::vector<Row> p = profile(T.mesh, first, last, cx_fx);

  std::printf("bandprobe: loop meshlets %zu..%zu, %zu distinct bind rings\n",
              first, last, p.size());
  std::printf("  ring   y_mm    arc_mm   halfX_mm   halfZ_mm   verts\n");
  const int32_t y0 = p.empty() ? 0 : p.front().y_mm;
  int32_t lox = 1 << 30, hix = 0, loz = 1 << 30, hiz = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    std::printf("  %4zu %6d %9d %10d %10d %7d\n", i, p[i].y_mm, p[i].y_mm - y0,
                p[i].half_x_mm, p[i].half_z_mm, p[i].n);
    // skip the buried base flare (the first two stations) in the range stat:
    // it is authored huge and would swamp the band's own variation
    if (p[i].y_mm - y0 > 500) {
      lox = std::min(lox, p[i].half_x_mm); hix = std::max(hix, p[i].half_x_mm);
      loz = std::min(loz, p[i].half_z_mm); hiz = std::max(hiz, p[i].half_z_mm);
    }
  }
  std::printf("bandprobe: above arc 500mm  halfX %d..%d (ratio %d%%)  "
              "halfZ %d..%d (ratio %d%%)\n",
              lox, hix, lox ? (hix * 100) / lox : 0,
              loz, hiz, loz ? (hiz * 100) / loz : 0);
  std::printf("bandprobe: a UNIFORM STRAP is ratio ~100%%; the sheet's four "
              "swellings want a clear local maximum at each joint station.\n");
  return 0;
}
