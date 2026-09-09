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
//   2. the CORE KEEPS ITS OWN PIGMENT                             (D11 §2.3)
//   3. density RISES INWARD from the gas's outer edge              (D11 §2.3)
//   4. the ink is painted LESS than it would be without its mask   (§14.1)
//   5. alpha 0 changes not one pixel                        (the revert path)
//   6. the annulus SCALES WITH THE BODY, not with a pixel count    (D11 §2.3)
//   7. the fog is an OUTER LAYER -- its peak is in the outer half   (D11 §2.3)
//
// ⚠ PASS 15 LANE-FX-2 INVERTED CHECK 2 AND ADDED 7, BECAUSE THE GATE WAS
// GREEN ON THE BUILD THE BY-EYE REVIEW REFUSED.
//
// This is the sharpest thing in this file and it belongs at the top. Pass 15's
// check 2 demanded that the fog reach the BODY'S CENTRE. The shipped values
// did, so it passed -- and reaching the centre WAS THE DEFECT: a near-white
// plateau over the whole interior bleached the pigment and erased the
// terminator that made the ball read as round (PASS-15-REVIEW.md §1, plate
// J01). **Six checks passed and six legs fired on a build that damaged the one
// quality the owner had named as praised.**
//
// It was not a sloppy check. It was a faithful encoding of a reading of D11
// §2.3 -- "make the eye/body intersection read as fog" -- that had been
// obsoleted by a SIBLING LANE, in the same pass, without either lane knowing:
// the eye lane cured the sinking lens by making the lenses ride the deform and
// stand proud, so the fog was still being asked to hide something that no
// longer happens. The gate went on enforcing a requirement whose reason had
// been deleted.
//
//   > **A gate encodes a hypothesis about what the owner wants** (10-GATE §0),
//   > and a hypothesis can be RETIRED by someone else's fix. When a sibling
//   > lane removes the symptom your check exists to serve, your check does not
//   > become harmless -- it becomes a rule enforcing a dead requirement, and it
//   > will hold the line for it.
//
// So check 2 now asserts the OPPOSITE (the core keeps its own pigment) and 7 is
// new (the peak sits in the outer half). Both are the FIRST clause of his
// sentence -- "the OUTER BODY PART is made of a thick fog" -- which no check
// had ever owned; 3 owns the second clause and always did. Both fail on the
// values pass 15 shipped, which is the only known-negative that matters here.
//
// PASS 15 ALSO RE-AIMED CHECKS 3 AND 4 AND ADDED 6, because the mechanism
// changed under them and two of them would have passed TAUTOLOGICALLY.
// The shell was a 3 px / 6 px band around the silhouette, densest AT the ink;
// it is now an annulus scaled to the body's own radius, densest INWARD. So:
//   * old 3 asserted density FALLS outward from the line. The owner's sentence
//     is the opposite ("gets less thick the nearer to the OUTSIDE"), so the
//     check is inverted to match him rather than to match the old code. It
//     also now reads RENDERED PIXELS instead of the profile function: a gate
//     that reads only the profile cannot see a compositing fault.
//   * old 4 compared the ink against its inner neighbour. Under a profile that
//     rises inward the neighbour is denser ANYWAY, so the check would have
//     passed with kShellOverInkPm set to 1000 -- theatre, 10-GATE item 10.
//     It now compares the ink pixel against the SAME pixel rendered with no
//     ink mask, which is a direct question about kShellOverInkPm and fails if
//     it is neutralised.
//   * 6 is new and is the mechanism itself: render the same scene at two body
//     radii and the fog's depth must grow with the body. A fixed-pixel band
//     passes every other check here and fails this one, which is the exact
//     regression pass 15 exists to prevent.
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
// Build. PASS 15 GAVE IT A build-direct.sh TARGET (`mshell`). The old comment
// here said there was none "on purpose... adding a target is a collision the
// pass does not need" -- true of pass 12's file split, and it had quietly
// become 10-GATE item 42: a tool the docs promise, with no way to build it, is
// a tool that does not exist. Nobody had run this one since it was written.
//   bash tools/reel/build-direct.sh --output <dir> mshell
// or by hand:
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
  // break 1: hand the production function a cover mask that is empty, so it
  // cannot reach anywhere. break 4: hand it no ink mask, so it cannot spare
  // the line.
  //
  // ⚠ break 2 USED TO SHARE THIS EMPTY-COVER LEG and no longer does. The old
  // check 2 asserted the centre WAS painted, so blanking the cover broke it;
  // the new one asserts the centre is NOT painted, and an empty cover would
  // make it pass TRIUMPHANTLY -- a leg that certifies the check instead of
  // breaking it. It now has its own leg (restore the shipped core plateau),
  // and this is exactly the shape of the check-5 tautology recorded above:
  // when a check is inverted, its old failing leg becomes a PASSING one.
  const uint8_t* cover = s.cover.data();
  std::vector<uint8_t> empty(static_cast<size_t>(kW) * kH, 0);
  if (break_check == 1) cover = empty.data();
  const uint8_t* ink = (break_check == 4) ? nullptr : s.ink.data();
  u02::shell_paint(s.rgb.data(), kW, kH, cover, ink, alpha);
  // Check 4's control: the SAME scene with no ink mask at all. The ink pixel
  // must take strictly less paint in the run that has one. This is the pair
  // that makes the check about kShellOverInkPm rather than about the profile.
  Scene noink = make_scene(radius, 40);
  u02::shell_paint(noink.rgb.data(), kW, kH, noink.cover.data(), nullptr, alpha);

  // ---- 1. the shell reaches OUTSIDE the silhouette ----------------------
  {
    const int x = cx + radius + 1;
    const bool ok = lum_at(s.rgb, x, cy) != lum_at(zero.rgb, x, cy);
    char b[160];
    std::snprintf(b, sizeof(b), "px (%d,%d) just outside the line: %d vs %d",
                  x, cy, lum_at(s.rgb, x, cy), lum_at(zero.rgb, x, cy));
    r.push_back({"1 reaches OUTSIDE the silhouette (D9 s14.1)", ok, b});
  }
  // ---- 2. the CORE KEEPS ITS OWN PIGMENT --------------------------------
  // ⚠ THIS CHECK IS THE EXACT REVERSE OF WHAT IT ASSERTED AN HOUR AGO, AND
  // THE OLD ONE PASSED ALL THE WAY TO A HELD PUBLISH.
  //
  // Pass 15 read D11 s2.3 as "the fog must reach the eye/body intersection,
  // which is tens of pixels interior", and wrote a check that the disc CENTRE
  // is painted. The shipped values satisfied it -- and that is precisely the
  // regression: `kShellCoreFloorPm` held a plateau across the entire interior,
  // so every pixel of the animal took ~10% of a near-white tint, the
  // terminator that made a flat-shaded ball read as ROUND was erased, and the
  // by-eye review held the bank over it (PASS-15-REVIEW.md s1).
  //
  // Two things had gone wrong and only one was the number:
  //   * the remedy that check was serving IS NO LONGER NEEDED. It existed so a
  //     lens sinking into the bouncing body would read as entering gas. The
  //     eye lane cured that a different way -- the lenses ride the deform and
  //     stand proud (N01, `hit` f24-f52) -- so the fog was still paying for a
  //     fault that no longer appears on screen.
  //   * and it was never what he asked for anyway. The sentence is "the OUTER
  //     BODY PART is made of a thick fog". The outer part. A gate that demands
  //     paint at the centre demands the one thing the sentence excludes.
  //
  // So the structural content of his sentence is that THE CORE IS NOT FOG, and
  // that is what is asserted here. This is a SHAPE claim, not a density one --
  // it says where the fog is, never how strong it may be, so 10-GATE s0 (no
  // number may bless a shell density) is respected.
  {
    Scene core = make_scene(radius, 40);
    Scene bare = make_scene(radius, 40);
    const int saved_floor = u02::g_u02_shell_floor_pm;
    // THE FAILING LEG IS THE SHIPPED REGRESSION ITSELF (10-GATE item 43: a
    // known-negative built by restoring the MECHANISM, not by nulling the
    // input). 180 pm is what pass 15 actually shipped; it repaints the core
    // and this check must go red for it.
    if (break_check == 2) u02::g_u02_shell_floor_pm = 180;
    u02::shell_paint(core.rgb.data(), kW, kH, core.cover.data(),
                     core.ink.data(), alpha);
    u02::g_u02_shell_floor_pm = saved_floor;
    const bool ok = lum_at(core.rgb, cx, cy) == lum_at(bare.rgb, cx, cy);
    char b[240];
    std::snprintf(b, sizeof(b),
                  "the disc CENTRE (%d,%d), %d px inside the line: %d vs an "
                  "unpainted %d (floor=%d pm)",
                  cx, cy, radius, lum_at(core.rgb, cx, cy),
                  lum_at(bare.rgb, cx, cy), u02::g_u02_shell_floor_pm);
    r.push_back({"2 the CORE KEEPS ITS OWN PIGMENT (D11 s2.3)", ok, b});
  }
  // ---- 3. density RISES INWARD across the annulus -----------------------
  // "a thick fog that gets less thick the nearer to the outside it goes."
  // Walk from the gas's outer edge inward to the annulus's inner boundary; the
  // paint taken must never fall. Read off the RENDERED PIXELS, not off the
  // profile function -- the profile is one input to the composite, and a gate
  // that reads only the profile cannot see a compositing fault.
  //
  // break 3 walks the same samples on the ALPHA-0 buffer, where every step is
  // zero and "never falls" is trivially true. That is what makes this a real
  // leg: it proves the check is reading paint and not reading a constant.
  //
  // ⚠ AND THE FIRST VERSION OF THIS LEG WAS THEATRE, CAUGHT BY RUNNING IT.
  // It read `int v = lum(zero) - lum(shell)` and guarded the comparison with
  // `if (prev >= 0 && v < prev)`. The shell LIGHTENS a dark test field, so
  // every v was NEGATIVE, `prev >= 0` was never true, and the comparison never
  // executed once. It printed a plausible row of numbers and passed
  // unconditionally -- 10-GATE item 10's exact fault, in a gate written in the
  // same hour as a comment about that fault. Paint is a MAGNITUDE now, and the
  // walk is stated in terms the owner's sentence can be checked against.
  {
    // Walk the +x axis from the gas's outer edge to the disc centre. Two
    // things are asserted, and both are his words rather than the code's:
    //   * the DENSEST fog is well INSIDE the silhouette, not at the edge
    //     ("gets less thick the nearer to the outside it goes");
    //   * the rise up to that peak never reverses.
    // The ink pixel is skipped: it is deliberately painted less
    // (kShellOverInkPm) and check 4 is the one that owns it.
    const std::vector<uint8_t>& buf = (break_check == 3) ? zero.rgb : s.rgb;
    std::vector<int> paint;      // magnitude, outermost sample first
    std::vector<int> inset;      // px inside the silhouette (negative = out)
    std::string d;
    for (int k = u02::kShellOutReachMinPx; k >= -radius; --k) {
      const int x = cx + radius + k;
      if (x < 0 || x >= static_cast<int>(kW)) continue;
      if (s.ink[static_cast<size_t>(cy) * kW + x]) continue;
      int v = lum_at(buf, x, cy) - lum_at(zero.rgb, x, cy);
      if (v < 0) v = -v;
      paint.push_back(v);
      inset.push_back(-k);
      d += std::to_string(v) + " ";
    }
    size_t peak = 0;
    for (size_t q = 1; q < paint.size(); ++q)
      if (paint[q] > paint[peak]) peak = q;
    bool rises = true;
    for (size_t q = 1; q <= peak; ++q)
      if (paint[q] < paint[q - 1]) rises = false;
    // "well inside" -- more than the outward skirt plus a couple of pixels, so
    // a fringe peaking at the ink cannot satisfy it.
    const bool deep = !paint.empty() && paint[peak] > 0 &&
                      inset[peak] >= u02::kShellOutReachMinPx + 2;
    const bool ok = rises && deep;
    char b[240];
    std::snprintf(b, sizeof(b),
                  "densest at %d px INSIDE the line (paint %d); rise to it is "
                  "%s; walk outermost-first: ",
                  inset.empty() ? 0 : inset[peak],
                  paint.empty() ? 0 : paint[peak],
                  rises ? "monotone" : "BROKEN");
    r.push_back({"3 the fog is THICKEST INWARD (D11 s2.3)", ok,
                 std::string(b) + d});
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
    const int d_free =
        lum_at(noink.rgb, ink_x, cy) - lum_at(zero.rgb, ink_x, cy);
    const bool ok = ink_x > 0 && d_ink < d_free;
    char b[240];
    std::snprintf(b, sizeof(b),
                  "ink px x=%d took %d of paint; the SAME px with no ink mask "
                  "took %d (over_ink=%d pm)",
                  ink_x, d_ink, d_free, u02::g_u02_shell_over_ink_pm);
    r.push_back({"4 the INK survives being crossed (D9 s14.1)", ok, b});
  }
  // ---- 5. alpha 0 is a true no-op --------------------------------------
  {
    Scene bare = make_scene(radius, 40);
    const bool ok = (zero.rgb == bare.rgb);
    r.push_back({"5 alpha 0 changes NO pixel (the revert path)", ok,
                 "compared the whole buffer"});
  }
  // ---- 6. THE MECHANISM: the fog scales with the body -------------------
  // The pass-12 shell was a fixed pixel band, and that is precisely why four
  // passes of alpha could not make it fog. A band satisfies checks 1-5 on a
  // big enough disc and fails here. Measure how deep the paint reaches on a
  // SMALL body and on a LARGE one: the large one's fog must go deeper.
  //
  // break 6 measures the large body twice, so the two depths are equal and the
  // strict inequality fails -- the leg proves the check can tell "scales" from
  // "does not".
  {
    const auto fog_depth = [&](int rad) {
      Scene t = make_scene(rad, 40);
      Scene z = make_scene(rad, 40);
      u02::shell_paint(t.rgb.data(), kW, kH, t.cover.data(), t.ink.data(),
                       alpha);
      int deepest = 0;
      for (int k = 0; k <= rad; ++k) {
        const int x = cx + rad - k;
        if (lum_at(t.rgb, x, cy) != lum_at(z.rgb, x, cy)) deepest = k;
      }
      return deepest;
    };
    const int small = fog_depth(break_check == 6 ? 34 : 10);
    const int large = fog_depth(34);
    const bool ok = large > small;
    char b[220];
    std::snprintf(b, sizeof(b),
                  "fog reaches %d px into a radius-10 body and %d px into a "
                  "radius-34 one (a fixed-pixel band would tie)",
                  small, large);
    r.push_back({"6 the annulus SCALES WITH THE BODY (D11 s2.3)", ok, b});
  }
  // ---- 7. THE FOG IS AN *OUTER* LAYER -----------------------------------
  // "the OUTER BODY PART is made of a thick fog that gets less thick the
  // nearer to the outside it goes." Check 3 owns the second clause -- density
  // rises inward. NOBODY OWNED THE FIRST, and that is the half pass 15 broke:
  // its annulus ran 520 pm of the radius, so the densest fog sat 0.52 R in,
  // in the MIDDLE of the ball. A fog whose peak is at the centre satisfies
  // "rises inward" perfectly and is not an outer layer at all.
  //
  // ⚠ THE FIRST VERSION OF THIS CHECK PASSED ON THE REGRESSION, AND RUNNING IT
  // IS THE ONLY REASON I KNOW THAT. It used the radius-24 fixture and asked for
  // the peak in the OUTER HALF. But an annulus of 520 pm PUTS the peak at
  // ~0.5 R by construction, so the threshold and the fault sat on the same
  // line and integer quantisation decided the verdict -- it reported "9 px,
  // must be under 12: PASS" on the exact values the by-eye review rejected.
  //
  // A threshold that coincides with the value under test is not a loose gate,
  // it is a COIN TOSS wearing a number, and it is the same family as the three
  // tautological checks this file already records. Two things fix it, and both
  // are needed:
  //   * a BIGGER fixture (radius 40, near `inspect`'s real on-screen body)
  //     so one pixel of Chebyshev quantisation cannot swing the answer;
  //   * the OUTER THIRD, not the outer half. "Outer body part" of a ball is
  //     generously read as a third of the radius; a peak at HALF the radius is
  //     the middle of the ball by any reading, which is precisely what shipped.
  //
  // The margin is printed either way, so the next person can see how close this
  // ran rather than trusting the verdict (10-GATE item 12).
  {
    const int rad7 = 40;
    Scene t = make_scene(rad7, 40);
    Scene z = make_scene(rad7, 40);
    const int saved_depth = u02::g_u02_shell_depth_pm;
    // The leg restores pass 15's own annulus, which is the regression this
    // check exists to catch (item 43: ablate the MECHANISM, do not null it).
    if (break_check == 7) u02::g_u02_shell_depth_pm = 520;
    u02::shell_paint(t.rgb.data(), kW, kH, t.cover.data(), t.ink.data(),
                     alpha);
    u02::g_u02_shell_depth_pm = saved_depth;
    int peak_inset = 0, peak_paint = -1;
    for (int k = 0; k <= rad7; ++k) {
      const int x = cx + rad7 - k;
      if (x < 0 || x >= static_cast<int>(kW)) continue;
      if (t.ink[static_cast<size_t>(cy) * kW + x]) continue;
      int v = lum_at(t.rgb, x, cy) - lum_at(z.rgb, x, cy);
      if (v < 0) v = -v;
      if (v > peak_paint) { peak_paint = v; peak_inset = k; }
    }
    const int limit = rad7 / 3;
    const bool ok = peak_paint > 0 && peak_inset < limit;
    char b[240];
    std::snprintf(b, sizeof(b),
                  "densest fog sits %d px inside the line on a radius-%d body; "
                  "must be under %d (the OUTER THIRD), margin %d; reach=%d pm",
                  peak_inset, rad7, limit, limit - peak_inset,
                  u02::g_u02_shell_depth_pm);
    r.push_back({"7 the fog is an OUTER LAYER (D11 s2.3)", ok, b});
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
  for (int k = 1; k <= 7; ++k) {
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
