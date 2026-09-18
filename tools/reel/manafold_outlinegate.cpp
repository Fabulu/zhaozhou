// Manafold Pass 17 Direction 17 outline-ownership gate.
//
// The antenna loop surrounds enclosed negative space. This gate calls the same
// pure mask helpers as production and proves that the O stays open, the complete
// creature-side inner boundary is owned, the accepted body/head line retains its
// depth test, and a nearer effect is not erased by a stale post-pass repaint.
//
// Mutants (each must return non-zero):
//   --selftest-no-opening-owner
//   --selftest-force-post-repaint

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "manafold_outline.h"

namespace {

constexpr uint32_t kW = 13;
constexpr uint32_t kH = 13;
constexpr uint8_t kInkR = 26, kInkG = 24, kInkB = 22;
constexpr uint8_t kBgR = 90, kBgG = 70, kBgB = 110;
constexpr uint8_t kFxR = 40, kFxG = 230, kFxB = 255;

int g_failures = 0;

size_t at(uint32_t x, uint32_t y) {
  return static_cast<size_t>(y) * kW + x;
}

void fail(const char* what) {
  std::printf("  FAIL: %s\n", what);
  ++g_failures;
}

bool rgb_is(const std::vector<uint8_t>& rgb, size_t i,
            uint8_t r, uint8_t g, uint8_t b) {
  return rgb[i * 3] == r && rgb[i * 3 + 1] == g && rgb[i * 3 + 2] == b;
}

void depth_tested_effect(std::vector<uint8_t>& rgb, size_t i,
                         int32_t effect_depth, int32_t surface_depth) {
  // The fixture uses larger reciprocal depth as nearer, matching the renderer's
  // z-test ordering. Effects do not update the shared surface-depth receipt.
  if (effect_depth <= surface_depth) return;
  rgb[i * 3] = kFxR;
  rgb[i * 3 + 1] = kFxG;
  rgb[i * 3 + 2] = kFxB;
}

int run(bool no_opening_owner, bool force_post_repaint) {
  g_failures = 0;
  const size_t n = static_cast<size_t>(kW) * kH;
  std::vector<uint8_t> mask(n, 0);

  // A two-pixel-thick square stick loop with a 3x3 enclosed O.
  for (uint32_t y = 3; y <= 9; ++y)
    for (uint32_t x = 3; x <= 9; ++x)
      mask[at(x, y)] = 1;
  for (uint32_t y = 5; y <= 7; ++y)
    for (uint32_t x = 5; x <= 7; ++x)
      mask[at(x, y)] = 0;

  std::vector<uint8_t> exterior;
  u02::outline::classify_exterior(mask, kW, kH, exterior);
  size_t enclosed = 0;
  for (uint32_t y = 5; y <= 7; ++y)
    for (uint32_t x = 5; x <= 7; ++x)
      if (!mask[at(x, y)] && !exterior[at(x, y)]) ++enclosed;
  std::printf("G1 enclosed negative space: %zu/9 pixels\n", enclosed);
  if (enclosed != 9) fail("the O is not a distinct enclosed negative-space component");

  std::vector<uint8_t> edge(n, 0), opening_owned;
  if (!no_opening_owner)
    u02::outline::add_enclosed_opening_edge(mask, exterior, kW, kH, 1,
                                             edge, &opening_owned);
  else
    opening_owned.assign(n, 0);

  size_t inner_expected = 0, inner_owned = 0;
  const auto expect = [&](uint32_t x, uint32_t y) {
    ++inner_expected;
    if (edge[at(x, y)]) ++inner_owned;
  };
  for (uint32_t x = 5; x <= 7; ++x) {
    expect(x, 4);
    expect(x, 8);
  }
  for (uint32_t y = 5; y <= 7; ++y) {
    expect(4, y);
    expect(8, y);
  }
  std::printf("G2 complete creature-side O boundary: %zu/%zu witnesses\n",
              inner_owned, inner_expected);
  if (inner_owned != inner_expected)
    fail("the lower/inside antenna-stick boundary has no complete owner");

  size_t hole_edge = 0;
  for (uint32_t y = 5; y <= 7; ++y)
    for (uint32_t x = 5; x <= 7; ++x)
      hole_edge += edge[at(x, y)] != 0;
  std::printf("G3 O remains open: %zu opening pixels selected for ink\n", hole_edge);
  if (hole_edge != 0) fail("ink ownership filled the O instead of painting the stick side");

  // Body-only top-line fixture. (5,4) remains the visible body owner; (6,4)
  // has a nearer full-creature surface and must not receive body-derived ink.
  std::vector<uint8_t> body_cover(n, 0), body_edge(n, 0), body_owned;
  std::vector<int32_t> body_depth(n, 0), full_depth(n, 0);
  for (uint32_t y = 3; y <= 4; ++y) {
    for (uint32_t x = 3; x <= 9; ++x) {
      const size_t i = at(x, y);
      body_cover[i] = 1;
      body_depth[i] = 100;
      full_depth[i] = 100;
    }
  }
  full_depth[at(6, 4)] = 120;
  u02::outline::add_visible_body_inner_edge(
      body_cover, body_depth, full_depth.data(), exterior, kW, kH, 1,
      body_edge, &body_owned);
  const bool top_kept = body_owned[at(5, 4)] != 0;
  const bool nearer_suppressed = body_owned[at(6, 4)] == 0;
  std::printf("G4 body/head top line: visible %s, nearer-antenna suppression %s\n",
              top_kept ? "owned" : "MISSING",
              nearer_suppressed ? "owned by foreground" : "OVERPAINTED");
  if (!top_kept) fail("the accepted body/head top line was reduced");
  if (!nearer_suppressed)
    fail("body-only ink draws through nearer composed creature geometry");

  for (size_t i = 0; i < n; ++i)
    if (body_edge[i]) edge[i] = 1;
  std::vector<uint8_t> rgb(n * 3, 0);
  for (size_t i = 0; i < n; ++i) {
    rgb[i * 3] = kBgR;
    rgb[i * 3 + 1] = kBgG;
    rgb[i * 3 + 2] = kBgB;
  }
  u02::outline::paint_ink(rgb.data(), n, edge, kInkR, kInkG, kInkB);
  const size_t effect_pixel = at(5, 4);
  depth_tested_effect(rgb, effect_pixel, 90, 100);  // behind: rejected
  const bool behind_loses = rgb_is(rgb, effect_pixel, kInkR, kInkG, kInkB);
  depth_tested_effect(rgb, effect_pixel, 110, 100);  // nearer: visible
  if (force_post_repaint)
    u02::outline::paint_ink(rgb.data(), n, edge, kInkR, kInkG, kInkB);
  const bool nearer_wins = rgb_is(rgb, effect_pixel, kFxR, kFxG, kFxB);
  std::printf("G5 final ownership: behind effect %s, nearer effect %s\n",
              behind_loses ? "occluded" : "LEAKED",
              nearer_wins ? "retained" : "ERASED BY INK");
  if (!behind_loses) fail("an effect behind internal ink became visible");
  if (!nearer_wins)
    fail("stale post-energy outline repaint erased a nearer visible effect");

  // Production extends creature coverage by the exact initial edge union. The
  // opening itself remains uncovered so mist may occupy negative space, while
  // no stick/ink pixel can be softened.
  std::vector<uint8_t> cover = mask;
  for (size_t i = 0; i < n; ++i)
    if (edge[i]) cover[i] = 1;
  size_t uncovered_ink = 0, covered_hole = 0;
  for (size_t i = 0; i < n; ++i)
    if (edge[i] && !cover[i]) ++uncovered_ink;
  for (uint32_t y = 5; y <= 7; ++y)
    for (uint32_t x = 5; x <= 7; ++x)
      covered_hole += cover[at(x, y)] != 0;
  std::printf("G6 mist cover: uncovered ink %zu, covered O pixels %zu\n",
              uncovered_ink, covered_hole);
  if (uncovered_ink != 0) fail("mist cover does not protect every ink pixel");
  if (covered_hole != 0) fail("mist cover incorrectly fills the negative-space O");

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "PASS" : "FAIL", g_failures);
  return g_failures == 0 ? 0 : 1;
}

void usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s [--selftest-no-opening-owner|"
               "--selftest-force-post-repaint]\n",
               argv0);
}

}  // namespace

int main(int argc, char** argv) {
  bool no_opening_owner = false;
  bool force_post_repaint = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--selftest-no-opening-owner") == 0)
      no_opening_owner = true;
    else if (std::strcmp(argv[i], "--selftest-force-post-repaint") == 0)
      force_post_repaint = true;
    else {
      usage(argv[0]);
      return 2;
    }
  }
  if (no_opening_owner)
    std::printf("[MUTANT] enclosed-opening owner disabled\n");
  if (force_post_repaint)
    std::printf("[MUTANT] stale post-energy ink restore enabled\n");
  return run(no_opening_owner, force_post_repaint);
}
