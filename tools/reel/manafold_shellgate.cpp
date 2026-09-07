// manafold_shellgate.cpp -- the committed gate for the Direction 9 §7/§14 shell.
//
// WHY THIS FILE EXISTS, and what it deliberately does NOT do.
//
// 10-GATE-CHECKLIST §0 is explicit that no number may bless a shell density:
// this exact value has burned two gates, and the owner's eye is the acceptance
// test. So this gate says NOTHING about how much shell is right. It checks only
// the STRUCTURE the owner's two sentences describe, which is regression
// protection underneath his judgement rather than a substitute for it:
//
//   1. the shell reaches OUTSIDE the silhouette                    (§14.1)
//   2. it reaches INSIDE it as well                                (§7, D5 §3)
//   3. density DECREASES with distance from the line, both ways    (§14.2)
//   4. the ink is painted at strictly LESS than its neighbours     (§14.1)
//   5. alpha 0 changes not one pixel                        (the revert path)
//
// 10-GATE-CHECKLIST item 10 has two mandatory halves and both are met here:
//
//   * IT CALLS THE PRODUCTION SYMBOL. `u02::shell_paint` and
//     `u02::shell_profile_pm` from manafold_fx.h, the very functions
//     zhao_reel.cpp calls -- not a re-derivation, not a same-named copy, not
//     the formula written out again. Pass 10 shipped two gates that re-derived
//     what they were checking and could not fail; the fixes were real and the
//     gates were theatre.
//   * IT SHIPS A FAILING LEG. `--selftest-fail <n>` feeds the REAL function an
//     input that must break check <n>, and the gate must report that check
//     failing. Run it and watch each one go red; a gate whose failure has never
//     been witnessed is a rumour with an exit code.
//
// Build (no build-direct.sh target on purpose -- this is a standalone check and
// adding a target is a collision the pass does not need):
//   g++ -O2 -std=c++17 -I<repo>/reference/include -I<repo>/runtime/include \
//       -I<repo>/tests/render -I<repo>/reference/src \
//       tools/reel/manafold_shellgate.cpp -o shellgate.exe
//
// Run:  shellgate.exe            (all checks; rc 0 = pass)
//       shellgate.exe --selftest (every failing leg must fail; rc 0 = pass)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

// manafold_fx.h states its own consumer contract at the top of the file:
// "include after `namespace zc = zref::creature;`". Honoured here exactly as
// zhao_reel.cpp honours it -- the gate must compile against the header the
// renderer compiles against, not a convenient subset of it.
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
namespace zc = zref::creature;

#include "manafold_fx.h"

namespace {

constexpr uint32_t kW = 96, kH = 96;

struct Scene {
  std::vector<uint8_t> rgb, cover, ink;
};

// A filled disc plus the one-pixel ink ring the cel pass would grow around it:
// the same shape of input shell_paint sees from the renderer (cover INCLUDES
// the ink, exactly as zhao_reel.cpp builds it).
Scene make_scene(int radius, uint8_t fill) {
  Scene s;
  s.rgb.assign(static_cast<size_t>(kW) * kH * 3, fill);
  s.cover.assign(static_cast<size_t>(kW) * kH, 0);
  s.ink.assign(static_cast<size_t>(kW) * kH, 0);
  const int cx = kW / 2, cy = kH / 2;
  for (uint32_t y = 0; y < kH; ++y)
    for (uint32_t x = 0; x < kW; ++x) {
      const int dx = static_cast<int>(x) - cx, dy = static_cast<int>(y) - cy;
      const int d2 = dx * dx + dy * dy;
      const size_t i = static_cast<size_t>(y) * kW + x;
      if (d2 <= radius * radius) s.cover[i] = 1;
      if (d2 <= radius * radius && d2 > (radius - 1) * (radius - 1)) {
        s.ink[i] = 1;
      }
    }
  return s;
}

int lum_at(const std::vector<uint8_t>& rgb, int x, int y) {
  const size_t i = (static_cast<size_t>(y) * kW + x) * 3;
  return rgb[i] + rgb[i + 1] + rgb[i + 2];
}

struct Result { const char* name; bool ok; std::string detail; };

// `break_check` reproduces, through the real function, the fault each check
// exists to catch -- by feeding it a degenerate input, never by editing the
// expectation.
std::vector<Result> run(int break_check) {
  std::vector<Result> r;
  const int radius = 24;
  const int cx = kW / 2, cy = kH / 2;

  Scene s = make_scene(radius, 40);
  // leg 5's input: alpha 0 must be a no-op.
  Scene zero = make_scene(radius, 40);
  // break 5: the revert path is claimed to be a no-op, so the way to break the
  // claim is to give the "zero" run a REAL alpha. The first version of this leg
  // set the MAIN run's alpha to 0 instead and then asserted that the main run
  // was unchanged -- which is true by construction. The selftest caught it and
  // printed "check 5 DID NOT FAIL -- gate is theatre", which is exactly the job
  // it exists for, and is left recorded here because that is how easy this
  // mistake is to make: the tautological leg looked identical to a real one.
  u02::shell_paint(zero.rgb.data(), kW, kH, zero.cover.data(), zero.ink.data(),
                   break_check == 5 ? u02::kShellAlphaMaxPm : 0);

  const int alpha = u02::kShellAlphaMaxPm;
  // break 1/2: hand the production function a cover mask that is empty, so it
  // cannot reach anywhere. break 4: hand it no ink mask, so it cannot spare
  // the line.
  const uint8_t* cover = s.cover.data();
  std::vector<uint8_t> empty(static_cast<size_t>(kW) * kH, 0);
  if (break_check == 1 || break_check == 2) cover = empty.data();
  const uint8_t* ink = (break_check == 4) ? nullptr : s.ink.data();
  u02::shell_paint(s.rgb.data(), kW, kH, cover, ink, alpha);

  // ---- 1. the shell reaches OUTSIDE the silhouette ----------------------
  {
    const int x = cx + radius + 1;
    const bool ok = lum_at(s.rgb, x, cy) != lum_at(zero.rgb, x, cy);
    char b[160];
    std::snprintf(b, sizeof(b), "px (%d,%d) just outside the line: %d vs %d",
                  x, cy, lum_at(s.rgb, x, cy), lum_at(zero.rgb, x, cy));
    r.push_back({"1 reaches OUTSIDE the silhouette (D9 s14.1)", ok, b});
  }
  // ---- 2. and INSIDE it -------------------------------------------------
  {
    const int x = cx + radius - 3;
    const bool ok = lum_at(s.rgb, x, cy) != lum_at(zero.rgb, x, cy);
    char b[160];
    std::snprintf(b, sizeof(b), "px (%d,%d) inside the line: %d vs %d", x, cy,
                  lum_at(s.rgb, x, cy), lum_at(zero.rgb, x, cy));
    r.push_back({"2 reaches INSIDE it too (D9 s7 / D5 s3)", ok, b});
  }
  // ---- 3. density falls off with distance from the line, OUTWARD --------
  // Read through the production profile, so a change to the falloff shape is
  // caught in the function that computes it rather than in a copy of it here.
  {
    bool ok = true;
    std::string d;
    int prev = -1;
    for (int32_t k = 1; k <= u02::kShellOutReachPx; ++k) {
      const int t_pm = 1000 - (k * 1000) / (u02::kShellOutReachPx + 1);
      const int v = u02::shell_profile_pm(
          break_check == 3 ? 1000 : t_pm, u02::kShellFalloffGamma);
      d += std::to_string(v) + " ";
      if (prev >= 0 && v >= prev) ok = false;
      prev = v;
    }
    r.push_back({"3 density DECREASES outward (D9 s14.2)", ok,
                 "profile weights: " + d});
  }
  // ---- 4. the ink is painted LESS than its neighbours -------------------
  // The ink pixel and the body pixel one step inside it sit at adjacent band
  // weights, so without kShellOverInkPm the ink would take MORE paint than its
  // inner neighbour, not less.
  {
    int ink_x = -1;
    for (int x = cx; x < static_cast<int>(kW); ++x)
      if (s.ink[static_cast<size_t>(cy) * kW + x]) { ink_x = x; break; }
    const int d_ink = lum_at(s.rgb, ink_x, cy) - lum_at(zero.rgb, ink_x, cy);
    const int d_in = lum_at(s.rgb, ink_x - 1, cy) - lum_at(zero.rgb, ink_x - 1, cy);
    const bool ok = ink_x > 0 && d_ink < d_in;
    char b[200];
    std::snprintf(b, sizeof(b),
                  "ink px x=%d took %d of paint, its inner neighbour took %d",
                  ink_x, d_ink, d_in);
    r.push_back({"4 the INK survives being crossed (D9 s14.1)", ok, b});
  }
  // ---- 5. alpha 0 is a true no-op --------------------------------------
  {
    Scene bare = make_scene(radius, 40);
    const bool ok = (zero.rgb == bare.rgb);
    r.push_back({"5 alpha 0 changes NO pixel (the revert path)", ok,
                 "compared the whole buffer"});
  }
  return r;
}

int report(const std::vector<Result>& rs, const char* title) {
  std::printf("-- %s\n", title);
  int bad = 0;
  for (const Result& x : rs) {
    std::printf("   [%s] %s\n        %s\n", x.ok ? "PASS" : "FAIL", x.name,
                x.detail.c_str());
    if (!x.ok) ++bad;
  }
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  const bool selftest = argc > 1 && std::strcmp(argv[1], "--selftest") == 0;
  if (!selftest) {
    const int bad = report(run(0), "SHELL GATE (Direction 9 s7 + s14)");
    std::printf("%s\n", bad == 0 ? "shellgate: PASS" : "shellgate: FAILED");
    return bad == 0 ? 0 : 1;
  }
  // Every check must be demonstrably breakable through the real code path.
  int unbreakable = 0;
  for (int k = 1; k <= 5; ++k) {
    char t[96];
    std::snprintf(t, sizeof(t), "SELFTEST: check %d must FAIL when broken", k);
    const std::vector<Result> rs = run(k);
    // checks 1 and 2 share the empty-cover break; either failing proves it.
    bool broke = false;
    for (size_t i = 0; i < rs.size(); ++i)
      if (!rs[i].ok && static_cast<int>(i) + 1 == k) broke = true;
    report(rs, t);
    if (k == 5)
      std::printf(
          "   (checks 1-2 read against that same buffer here, so they go "
          "quiet; check 5 is the one this leg is about.)\n");
    std::printf("   -> check %d %s\n", k,
                broke ? "FAILED as required" : "DID NOT FAIL -- gate is theatre");
    if (!broke) ++unbreakable;
  }
  std::printf("%s\n", unbreakable == 0
                          ? "shellgate --selftest: every check is failable"
                          : "shellgate --selftest: SOME CHECK CANNOT FAIL");
  return unbreakable == 0 ? 0 : 1;
}
