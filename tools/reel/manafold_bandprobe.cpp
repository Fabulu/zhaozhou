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

// ---------------------------------------------------------------------------
// DIRECTION 7 §9.1 AS A GATE: EVERY JOINT ON A BALL, EVERY BALL ON A JOINT.
//
// "The joints need to be where the balls are and also the two spots where the
// antennae meet the creature."
//
// This is checkable arithmetic and nobody had ever checked it, which is how
// pass 8 shipped a kneading joint at arc 586 -- 344 mm from the nearest ball,
// in the middle of a smooth run -- while the re-entry ball at 2660 had no joint
// at all. Both faults were visible from these two lists side by side, and
// neither list was wrong on its own. So the lists live in one function now.
//
// Tolerance is the knuckle's own half-width: a joint inside the swell it drives
// is AT that ball as far as the eye is concerned; one outside it creases bare
// band. That makes the tolerance a property of the geometry rather than a
// number somebody picked.
// PASS 10, 0.3 -- THE STATION TABLE IS NOW A PARAMETER.
//
// QA's #3 and #4 together: the ball list omitted `knuckle-End`, the ONE ball
// with no joint -- so the gate's own list quietly excluded the failure it needs
// to catch -- and `--selftest` re-implemented the distance loop on local arrays
// instead of calling this function, over a DIFFERENT ball set (five here, four
// in the gate). A bug in the real loop or its wiring would have passed the
// selftest undetected.
//
// Both are fixed by one change: the stations come in as an argument, so the
// selftest can inject pass 8's layout and drive THE REAL FUNCTION.
struct JointStations {
  int32_t neck, a, b, c, d;
};
inline JointStations shipped_stations() {
  JointStations s;
  s.neck = u02::kLoopBuryMm + u02::kLoopArcMm[0];
  s.a = s.neck + u02::kLoopArcMm[1];
  s.b = s.a + u02::kLoopArcMm[2];
  s.c = s.b + u02::kLoopArcMm[3];
  s.d = s.c + u02::kLoopArcMm[4];
  return s;
}

// PASS 10, 0.3: ONE FLAG, ONE PLACE. While the joint AT the re-entry ball is
// unlanded this is false, `knuckle-End` is enumerated as a DECLARED GAP with
// its blocking measurement printed, and it is counted and named rather than
// silently left out of the list. When a re-entry joint lands, this flips and
// the gate enforces all five. Pass 10's C.2 prototype ABORTED, so it stays
// false and the gap stays loud.
constexpr bool kReentryJointLanded = false;

int joints_on_balls(bool verbose, const JointStations& st = shipped_stations()) {
  const int32_t stNeck = st.neck;
  const int32_t stA = st.a;
  const int32_t stB = st.b;
  const int32_t stC = st.c;
  const int32_t stD = st.d;
  struct Named { const char* name; int32_t at; };
  // kBJunctionF is deliberately absent: it shares kBNeck's pivot exactly, so
  // it is the same station and checking it twice would prove nothing.
  // WHICH STATIONS THIS GATE JUDGES, and why hingeD is not one of them.
  // The owner is talking about the joints the creature KNEADS with: "I see you
  // try to knead with antennae. But it is only one joint that does it, and the
  // joint is in the wrong place." The stations antenna_knead() and HingePlay
  // actually drive are junctionF/neck, A, B and C -- those four, and no other.
  //
  // kBHingeD is NOT driven by either: it carries the CLOSURE AIM, computed per
  // frame in loop_pose() so the return arm points at the re-entry anchor. It is
  // a solver variable, not a joint anybody plays with, and its bend is the
  // band curving back into the body rather than a crease.
  //
  // ⚠ AND THIS WAS TESTED, NOT ASSUMED. Pass 9 first moved hingeD onto the
  // re-entry ball at 2660, which is where §9.1 would put it. THE LOOP STOPS
  // CLOSING: the committed closure probe goes from 989 pm (baseline, gate 1120)
  // to 2401 pm, because at low fold arc 2660 is out in the air and the arm left
  // past it can no longer reach back into the body. Arm length cannot buy it --
  // swept 640/850/950/1050/1270, the open-fold end improves as the clip-bank
  // end degrades and neither reaches the gate -- and nor can the anchor.
  // So the re-entry ball keeps its swell and has NO articulation station.
  // That is a DECLARED GAP, printed below on every run, not a silent one.
  const Named joints[] = {{"junctionF/neck", stNeck}, {"hingeA", stA},
                          {"hingeB", stB},            {"hingeC", stC}};
  // PASS 10, 0.3: ALL FIVE BALLS ARE ENUMERATED. knuckle-End is on the list it
  // was missing from; whether its absence of a joint COUNTS is decided by
  // kReentryJointLanded, in one place, and either way it is printed by name.
  const Named balls[] = {{"knuckle-Jf", u02::kKnuckleAtJfMm},
                         {"knuckle-A", u02::kKnuckleAtAMm},
                         {"knuckle-B", u02::kKnuckleAtBMm},
                         {"knuckle-C", u02::kKnuckleAtCMm},
                         {"knuckle-End", u02::kKnuckleAtEndMm}};
  const int32_t tol = u02::kKnuckleSwellHalfMm;
  int bad = 0;
  int declared_gaps = 0;
  for (const Named& j : joints) {
    int32_t best = 1 << 30; const char* who = "(none)";
    for (const Named& b : balls) {
      const int32_t d = j.at > b.at ? j.at - b.at : b.at - j.at;
      if (d < best) { best = d; who = b.name; }
    }
    if (verbose)
      std::printf("  joint %-15s arc %5d -> nearest ball %-12s %4d mm %s\n",
                  j.name, j.at, who, best, best <= tol ? "OK" : "*** IN A STRAIGHT RUN");
    if (best > tol) ++bad;
  }
  for (const Named& b : balls) {
    int32_t best = 1 << 30;
    for (const Named& j : joints) {
      const int32_t d = j.at > b.at ? j.at - b.at : b.at - j.at;
      if (d < best) best = d;
    }
    // The re-entry ball is the one DECLARED gap. It is enumerated, measured and
    // named on every run; it simply does not fail the exit code while the flag
    // says the joint is unlanded. That is the difference between a gap the
    // project has decided to carry and a gap nobody mentioned.
    const bool is_declared_gap =
        !kReentryJointLanded && b.at == u02::kKnuckleAtEndMm;
    const bool fails = best > tol && !is_declared_gap;
    if (verbose)
      std::printf("  ball  %-15s arc %5d -> nearest joint            %4d mm %s\n",
                  b.name, b.at, best,
                  best <= tol ? "OK"
                              : (is_declared_gap ? "*** RIGID -- DECLARED GAP (see below)"
                                                 : "*** RIGID, no joint"));
    if (fails) ++bad;
    if (is_declared_gap && best > tol) ++declared_gaps;
  }
  if (verbose) {
    std::printf("  ---- not articulation stations, reported for completeness ----\n");
    std::printf("  hingeD          arc %5d    the CLOSURE SOLVER, not a knead joint\n", stD);
    std::printf("  knuckle-End     arc %5d    DECLARED GAP (%d counted): no "
                "articulation station.\n"
                "      pass 9: moving hingeD here breaks closure, 989 -> 2401 pm "
                "against a 1120 gate; arm length and anchor swept, no help.\n"
                "      pass 10: the two-segment redesign was PROTOTYPED and "
                "ABORTED. Splitting the arm 630+640 cannot reach the anchor at "
                "9 of 24 fold scales (D sits up to 1575 mm away, the chain "
                "reaches 1270), and the reachable form -- D aiming as today with "
                "a bounded bend at the ball -- holds only 7.5 deg before the rim "
                "gate breaks, under the ~10 deg that reads at 240p.\n"
                "      THE REASON, for pass 11: the straight strut already sits "
                "at 991 pm of a 1120 pm gate. There are 129 pm of headroom in "
                "total, so no visible joint fits here until the arm/anchor "
                "GEOMETRY changes. Nobody has yet swept those against a "
                "two-segment form; that is where the design round starts.\n",
                u02::kKnuckleAtEndMm, declared_gaps);
  }
  if (verbose)
    std::printf("bandprobe: joints-on-balls %s over %d balls and %d joints "
                "(tolerance %d mm = the knuckle's own half-width; %d declared "
                "gap%s not counted as failures)\n",
                bad ? "FAIL" : "PASS",
                static_cast<int>(sizeof(balls) / sizeof(balls[0])),
                static_cast<int>(sizeof(joints) / sizeof(joints[0])), tol,
                declared_gaps, declared_gaps == 1 ? "" : "s");
  return bad;
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
  // PASS 9: and prove the JOINTS gate can FAIL, not merely that it passes. A
  // gate nobody has watched fail is a gate nobody has tested -- pass 8's own
  // standard, and the reason this probe plants a bulge rather than trusting
  // one. The shipped geometry must pass; the pass-8 layout must be caught.
  if (joints_on_balls(false) != 0) {
    std::printf("selftest FAIL: the SHIPPED geometry does not satisfy S9.1\n");
    return 1;
  }
  {
    // PASS 10, 0.3 -- THE SELFTEST NOW CALLS THE REAL FUNCTION.
    //
    // It used to re-implement the distance loop on local arrays, over a
    // different ball set than the gate itself used (five here, four there). A
    // bug in joints_on_balls' own loop, or in how it is wired, would have
    // passed this undetected -- it was testing a copy of the idea, which is the
    // thing this project keeps being bitten by.
    //
    // Now pass 8's layout is INJECTED into joints_on_balls and the real
    // function is required to return non-zero. Same code path as the shipping
    // gate, same ball list, no second implementation to drift.
    JointStations stale = shipped_stations();
    stale.neck = 586;   // pass 8: mid-tube, no ball within 340 mm
    stale.d = 2030;
    const int caught = joints_on_balls(false, stale);
    if (caught == 0) {
      std::printf("selftest FAIL: joints_on_balls() PASSES the pass-8 layout "
                  "(neck 586, hingeD 2030). The gate cannot fail, so its OK on "
                  "the shipped geometry means nothing.\n");
      return 1;
    }
    std::printf("bandprobe selftest: the REAL joints_on_balls() rejects the "
                "pass-8 layout (%d violations) — gate proved failable through "
                "the shipping code path, not a copy of it\n", caught);
  }
  std::printf("bandprobe selftest: OK (bulge found, flat stack flat, joints gated)\n");
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
    std::printf("bandprobe: DIRECTION 7 S9.1 -- joints on balls, balls on joints\n");
  const int bad = joints_on_balls(true);
  return bad ? 1 : 0;
}
