// island_scene_traversal.cpp -- an 8 km island RENDERED, and flown across.
//
// ---------------------------------------------------------------------------
// THE QUESTION THIS TEST EXISTS TO ANSWER
// ---------------------------------------------------------------------------
// reports/Missingterrain: "The existing example island is consequently a
// manually registered patch resource, not the visible portion of a world
// selected by a world manager." And: "The crucial validation should be an 8 km
// traversal capture, not a static image."
//
// Before this test, EVERY frame in the tree -- tests/render/*.cpp,
// tools/reel/zhao_reel.cpp -- emitted exactly ONE `zhao_pack_draw_procedural`
// call against exactly one hand-registered `TerrainPatch`. The renderer's
// terrain path was already a loop over a vector (render_frame.cpp:308 pushes,
// :505 loops), so the CAPABILITY to draw many patches in one frame existed and
// was simply never exercised at island scale, and nothing derived the patch
// list from an `island::Directory`.
//
// So this test does three things, and the third is the one the owner asked for:
//
//   A. ONE FRAME of the whole shipped example island -- 793 patches of ground
//      inside a 125 x 125 grid -- issued from the directory, drawn by
//      `zref::render::SoftwareRenderer`.
//   B. AN 8 KM TRAVERSAL across a 125-patch-long island, out and back, with
//      `island::Streamer` driving residency at every step, asserting at each
//      step that the number of patches drawn equals what the directory
//      predicts -- computed three independent ways.
//   C. The arithmetic in reports/Missingterrain and design/contracts/
//      TERRAIN.ISLAND.md, recomputed rather than quoted.
//
// ---------------------------------------------------------------------------
// WHY THE PREDICTION IS COMPUTED THREE WAYS
// ---------------------------------------------------------------------------
// `Scene::build_frame` obtains its patch list from `island::visible_set`. A
// test that then asked `visible_set` what the answer should be would be
// comparing a function with itself and would pass for any window rule at all,
// including one that returned nothing. So each step compares:
//
//   (1) what the frame ISSUED                        -- the harness
//   (2) a loop in THIS FILE over `Directory::find`   -- the directory itself
//   (3) a CLOSED FORM from the island's shape        -- arithmetic, no code
//                                                       under test involved
//
// Two of those could agree while being wrong together. Three, derived from
// three different places, could not agree by accident.
//
// ---------------------------------------------------------------------------
// PROVING IT CAN FAIL
// ---------------------------------------------------------------------------
// `--fire=N` perturbs one INPUT (never an expectation) so a claim is violated;
// the failure text is the evidence that the claim is load-bearing. A green run
// of `--fire=0` plus a red run of each N is the acceptance evidence for this
// file. TWO OF THE EIGHT DID NOT FIRE ON THE FIRST TRY, and both failures are
// worth more than the modes that worked:
//
//   * mode 1 wrote its expectation as `g_fire == 1 ? 1 : 0`. The goalpost moved
//     with the input, so the perturbed run passed. An expectation that knows
//     about the perturbation is not a detector.
//   * mode 3 bumped ONE vertex of ONE patch by 16 m and the frame CRC did not
//     move. At the whole-island camera the island is 93 px across, so a patch
//     is ~3 px and nearly all of its 2,048 triangles are sub-pixel. The
//     perturbation was real, invisible, and would have certified determinism
//     that was never tested.
//
// Both are the "broken instrument" law: the defect made the answer look better.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "zref/zref_island.hpp"
#include "zref/zref_island_scene.hpp"
#include "zref/zref_island_stream.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_residency.hpp"
#include "zref/zref_terrain.hpp"

namespace {

// ---------------------------------------------------------------- harness ---

int g_checks = 0;
int g_failures = 0;
int g_fire = 0;

const char* kFireHelp =
    "  --fire=1  one page body is never registered   -> page/miss accounting\n"
    "  --fire=2  the frame is built from a NARROWER window than the prediction uses\n"
    "  --fire=3  the whole world changes between the two renders  -> determinism\n"
    "  --fire=4  the camera does not move            -> the picture must change, and churn\n"
    "  --fire=5  the return leg is skipped           -> eviction AND return\n"
    "  --fire=6  a shared edge height is bumped      -> patch seam continuity\n"
    "  --fire=7  the disc radius is wrong            -> the 793-patch figure\n"
    "  --fire=8  every patch placed at one envelope  -> the island's on-screen size\n";

void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq(long got, long want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_failures;
    std::printf("FAIL: %s: expected %ld, got %ld\n", what, want, got);
  }
}

// ------------------------------------------------------- the height field ---
//
// THE TERRAIN IS A FUNCTION OF WORLD POSITION, NOT OF PATCH INDEX, and that is
// the whole reason the seam check below can exist. Patch ix spans
// [ix*64, (ix+1)*64] and patch ix+1 starts at exactly (ix+1)*64, so their
// shared vertex column is the SAME world X and takes the SAME height -- the
// lattice is continuous across the island by construction rather than by
// stitching. Author it per patch index instead and every patch boundary in an
// 8 km island is a cliff.
//
// Three triangle waves with mutually non-commensurate periods (373 / 257 / 611
// metres). Non-commensurate ON PURPOSE: with a periodic field, a camera that
// steps by a whole period produces a BYTE-IDENTICAL frame, and a traversal
// test whose frames are all identical cannot tell "the camera moved" from "the
// camera did nothing". Integer throughout -- no float touches a height.
int32_t tri_wave(int32_t v, int32_t period) {
  int32_t m = v % period;
  if (m < 0) m += period;
  return m < period / 2 ? m : period - m;
}

// height16 raw (S 1.7.8 -- metres * 256, qformats §9), from world metres.
int16_t island_height(int32_t world_x_m, int32_t world_z_m) {
  const int32_t h = 2560                                       // ~10 m of base
                    + 12 * tri_wave(world_x_m, 373)            // long ridges
                    + 9 * tri_wave(world_z_m, 257)             // cross swell
                    + 5 * tri_wave(world_x_m + world_z_m, 611);  // diagonal relief
  return static_cast<int16_t>(h);
}

// Fill a page's lattice from the field, using the envelope the Scene placed it
// at. `lattice_lerp` is the renderer's own envelope walk; re-deriving the world
// X here with a different rounding would author the heights at coordinates the
// renderer does not use.
void author_page(zref::render::TerrainPatch& p) {
  const int w = p.width, h = p.height;
  for (int j = 0; j < h; ++j) {
    const int32_t wz = zref::terrain::lattice_lerp(p.env_z0, p.env_z1, j, h - 1);
    for (int i = 0; i < w; ++i) {
      const int32_t wx = zref::terrain::lattice_lerp(p.env_x0, p.env_x1, i, w - 1);
      p.heights[static_cast<size_t>(j) * w + i] = island_height(wx >> 16, wz >> 16);
    }
  }
}

// ------------------------------------------------------------ canvas reads ---

struct Footprint {
  uint32_t painted = 0;                      // pixels that are not the background
  uint32_t x0 = 0xFFFFFFFFu, x1 = 0, y0 = 0xFFFFFFFFu, y1 = 0;
  uint32_t width() const { return painted ? x1 - x0 + 1 : 0; }
  uint32_t height() const { return painted ? y1 - y0 + 1 : 0; }
};

// The background is the no-DrawSky fallback clear, RGB 0,0,0 -> 565 zero
// (render_frame.cpp: `sky::SkyColor bg{0,0,0}` when no sky set is bound). So a
// non-zero halfword is a pixel terrain wrote.
Footprint footprint_of(const zref::render::RenderCanvas& c, uint32_t slot,
                       zhao_abi::video_mode mode) {
  const uint32_t w = zref::render::canvas_width(mode);
  const uint32_t h = zref::render::canvas_height(mode);
  const uint8_t* base = c.slot[slot].data();
  Footprint f;
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const size_t off = (static_cast<size_t>(y) * w + x) * 2;
      const uint16_t v = static_cast<uint16_t>(base[off] | (base[off + 1] << 8));
      if (v == 0) continue;
      ++f.painted;
      if (x < f.x0) f.x0 = x;
      if (x > f.x1) f.x1 = x;
      if (y < f.y0) f.y0 = y;
      if (y > f.y1) f.y1 = y;
    }
  }
  return f;
}

// --------------------------------------------------------------- the world ---

constexpr int32_t kSide = 125;         // patches per side: 8000 m / 64 m
constexpr int32_t kResidency = 1024;   // pages, TERRAIN.RESIDENCY's plan
constexpr uint32_t kPageBytes = 21376; // terrain_rules §2 page stride
constexpr uint32_t kMaterial = 0x4000001u;
constexpr int32_t kDiscRadiusPatches100 = 1590;  // 15.9 patches, x100 to stay integer

// The shipped example island: a DISC of ground, because a floating island is
// not a square and its corners are exactly the sky that must cost nothing.
// island_8km_directed.cpp derives the same shape from terrain_rules §1.4's
// 3.25 km²; this rebuilds it so the two files can disagree if one drifts.
std::size_t build_disc(zref::island::Directory& dir) {
  const int32_t cx = kSide / 2, cz = kSide / 2;
  int32_t r100 = kDiscRadiusPatches100;
  if (g_fire == 7) r100 = 1400;  // FIRE: a smaller island, same claim of 793
  const int64_t r2 = static_cast<int64_t>(r100) * r100;
  uint32_t handle = 1;
  for (int32_t iz = 0; iz < kSide; ++iz) {
    for (int32_t ix = 0; ix < kSide; ++ix) {
      const int64_t dx = static_cast<int64_t>(ix - cx) * 100;
      const int64_t dz = static_cast<int64_t>(iz - cz) * 100;
      if (dx * dx + dz * dz <= r2) dir.set(ix, iz, handle++);
    }
  }
  return dir.resident_count();
}

// The 8 km RIBBON: ground in every one of the 125 patch columns, six rows deep.
// reports/Missingterrain names this case explicitly -- "An 8 km-long but
// relatively narrow island can fit inside the 1,024-patch resident set" -- and
// it is the shape a traversal test needs, because the disc above is 8 km of
// EXTENT with only ~2 km of ground in the middle, so most steps of a flight
// across it would be over open sky and would test nothing.
constexpr int32_t kRibbonZ0 = 60;
constexpr int32_t kRibbonZ1 = 65;  // inclusive: six rows

std::size_t build_ribbon(zref::island::Directory& dir) {
  uint32_t handle = 1;
  for (int32_t iz = kRibbonZ0; iz <= kRibbonZ1; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix) dir.set(ix, iz, handle++);
  return dir.resident_count();
}

// (3) THE CLOSED FORM. No code under test participates: this is the ribbon's
// own arithmetic, written out. A square window of radius R centred on
// (cix, ciz) overlaps the ribbon in
//     (columns of [cix-R, cix+R] inside [0, kSide))  x
//     (rows of    [ciz-R, ciz+R] inside [kRibbonZ0, kRibbonZ1])
int32_t span(int32_t lo, int32_t hi, int32_t clamp_lo, int32_t clamp_hi) {
  const int32_t a = lo > clamp_lo ? lo : clamp_lo;
  const int32_t b = hi < clamp_hi ? hi : clamp_hi;
  return b >= a ? b - a + 1 : 0;
}

int32_t ribbon_closed_form(const zref::island::View& v) {
  const int32_t cols = span(v.centre_ix - v.radius, v.centre_ix + v.radius, 0, kSide - 1);
  const int32_t rows = span(v.centre_iz - v.radius, v.centre_iz + v.radius, kRibbonZ0, kRibbonZ1);
  return cols * rows;
}

// (2) THE DIRECTORY ITSELF. A loop in this file, not the one under test.
int32_t directory_count_in_window(const zref::island::Directory& dir,
                                  const zref::island::View& v) {
  int32_t n = 0;
  for (int32_t iz = v.centre_iz - v.radius; iz <= v.centre_iz + v.radius; ++iz)
    for (int32_t ix = v.centre_ix - v.radius; ix <= v.centre_ix + v.radius; ++ix)
      if (dir.find(ix, iz).outcome == zref::island::Outcome::kResident) ++n;
  return n;
}

zref::render::Material ground_material() {
  zref::render::Material m;
  m.r = 168;
  m.g = 152;
  m.b = 112;
  return m;
}

}  // namespace

// ===========================================================================
int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string a(argv[i]);
    if (a.rfind("--fire=", 0) == 0) g_fire = std::atoi(a.c_str() + 7);
    if (a == "--help") {
      std::printf("island_scene_traversal [--fire=N]\n%s", kFireHelp);
      return 0;
    }
  }
  if (g_fire) std::printf("[FIRE MODE %d] one input is deliberately wrong\n", g_fire);

  namespace isl = zref::island;
  namespace rnd = zref::render;
  const auto t_start = std::chrono::steady_clock::now();

  // =========================================================================
  // C. THE ARITHMETIC, RECOMPUTED
  // =========================================================================
  check_eq(isl::patch_metres(1), 64, "a patch at the canonical 2.0 m pitch is 64 m per side");
  check_eq(8000 / isl::patch_metres(1), kSide, "8 km is 125 patches across");
  check_eq(static_cast<long>(kSide) * kSide, 15625, "a DENSE 8x8 km grid is 15,625 patches");
  {
    // 15,625 pages x 21,376 B = 318.5 MiB, against 128 MB of local memory --
    // the number that makes residency (1,024) the plan instead of "keep it all".
    const int64_t dense_bytes = static_cast<int64_t>(kSide) * kSide * kPageBytes;
    const int64_t tenths = (dense_bytes * 10 + (1 << 19)) / (1 << 20);
    check_eq(static_cast<long>(tenths), 3185,
             "a dense 8x8 km plate is 318.5 MiB of patch pages alone");
    const int64_t res_tenths = (static_cast<int64_t>(kResidency) * kPageBytes * 10 + (1 << 19)) /
                               (1 << 20);
    check_eq(static_cast<long>(res_tenths), 209, "the 1,024-page residency is 20.9 MiB");
  }

  // =========================================================================
  // A. ONE FRAME OF THE WHOLE SHIPPED ISLAND
  // =========================================================================
  isl::Desc dd;
  dd.island_id = 0x51u;
  dd.pitch_log2 = 1;
  dd.extent_ix = static_cast<uint16_t>(kSide);
  dd.extent_iz = static_cast<uint16_t>(kSide);
  dd.origin_y = 40 << 16;  // the datum: this island floats
  dd.tileset_id = 0;       // untextured: the flat path, and the goldens' path

  isl::Directory disc(dd);
  const std::size_t solid = build_disc(disc);
  std::printf("  shipped island: %zu patches of ground in %lld (%.1f%% ground)\n", solid,
              static_cast<long long>(disc.dense_count()),
              100.0 * static_cast<double>(solid) / static_cast<double>(disc.dense_count()));
  check_eq(static_cast<long>(solid), 793,
           "the shipped example island has 793 patches of ground "
           "(reports/Missingterrain and TERRAIN.ISLAND.md both say 793)");
  check(solid <= static_cast<std::size_t>(kResidency),
        "and it fits the 1,024-page residency, at 8 km across");

  isl::Scene disc_scene(disc, kMaterial, ground_material());
  {
    // Author every page the directory owns. 33x33 is the canonical lattice
    // (charter §11.1) and is used unchanged here: an island-scale frame drawn
    // at a reduced lattice would be a claim about a different island.
    int32_t skipped = -1;
    for (int32_t iz = 0; iz < kSide; ++iz) {
      for (int32_t ix = 0; ix < kSide; ++ix) {
        if (disc.find(ix, iz).outcome != isl::Outcome::kResident) continue;
        if (g_fire == 1 && skipped < 0) {  // FIRE: one page never registered
          skipped = 1;
          continue;
        }
        rnd::TerrainPatch* p = disc_scene.page(ix, iz, 33, 33);
        if (p == nullptr) continue;
        if (g_fire == 8) {  // FIRE: 793 patches all stacked on one spot
          isl::Envelope e;
          isl::patch_envelope(dd, kSide / 2, kSide / 2, &e);
          p->env_x0 = e.x0; p->env_z0 = e.z0; p->env_x1 = e.x1; p->env_z1 = e.z1;
        }
        author_page(*p);
      }
    }
  }

  // The window that holds the ENTIRE island: radius 63 about the centre covers
  // all 125 columns and rows. 127^2 = 16,129 questions asked of the directory
  // to find 793 answers -- 95.1% sky, which is the ordinary case and is why
  // OPEN SKY is a first-class outcome rather than a miss.
  isl::View whole;
  whole.centre_ix = kSide / 2;
  whole.centre_iz = kSide / 2;
  whole.radius = 63;

  const int32_t patch_m = isl::patch_metres(dd.pitch_log2);
  const int32_t centre_raw = (kSide * patch_m / 2) << 16;
  // scale 16: ndc = 16 * x / 65536, so +-4000 m -> +-0.977 ndc. The whole 8 km
  // island lands inside the 384 x 240 canvas.
  const zhao_abi::ZhMat4fx map_cam = isl::ortho_map_at(16, centre_raw, centre_raw);

  // FIRE 2 narrows the window the FRAME is built from while every prediction
  // below still uses `whole`. Erasing a directory entry (the first attempt)
  // does not work: the harness and the oracle both read the same directory, so
  // they move together and agree on a wrong answer.
  isl::View whole_frame_view = whole;
  if (g_fire == 2) whole_frame_view.radius = 10;
  const isl::FramePlan whole_plan = disc_scene.build_frame(1, map_cam, whole_frame_view);
  std::printf("  whole-island frame: %u DrawProcedural in %u records, %zu bytes; "
              "window asked %u, ground %u, sky %u, beyond %u\n",
              whole_plan.ledger.issued, whole_plan.ledger.records, whole_plan.packet.size(),
              whole_plan.ledger.tally.examined, whole_plan.ledger.tally.emitted,
              whole_plan.ledger.tally.sky, whole_plan.ledger.tally.out_of_extent);

  check_eq(static_cast<long>(whole_plan.ledger.tally.examined), 127L * 127,
           "a radius-63 window asks (2R+1)^2 = 16,129 questions");
  check_eq(static_cast<long>(whole_plan.ledger.tally.emitted +
                             whole_plan.ledger.tally.sky +
                             whole_plan.ledger.tally.out_of_extent),
           static_cast<long>(whole_plan.ledger.tally.examined),
           "every question got exactly one answer, and the answers are counted apart");
  check_eq(static_cast<long>(whole_plan.ledger.issued),
           directory_count_in_window(disc, whole),
           "the frame issues one DrawProcedural per patch the DIRECTORY calls ground");
  check_eq(static_cast<long>(whole_plan.ledger.issued), static_cast<long>(disc.resident_count()),
           "and a window containing the whole island issues the whole island");
  check_eq(static_cast<long>(whole_plan.ledger.unplaceable), 0,
           "every patch of an 8 km island is placeable inside fx16's +-32 km");

  rnd::SoftwareRenderer rend_a;
  rnd::RenderCanvas canvas_a;
  const rnd::RenderResult ra =
      rend_a.render_frame(whole_plan.packet, 0, canvas_a, disc_scene.resources());
  check_eq(ra.status, zhao_abi::ZH_ABI_OK, "the island-scale packet validates and renders");
  check_eq(static_cast<long>(ra.commands_executed), static_cast<long>(whole_plan.ledger.records),
           "the renderer executed every record the scene emitted");
  check_eq(static_cast<long>(ra.resource_misses), static_cast<long>(whole_plan.ledger.no_page_body),
           "a resource miss happens exactly when the directory has ground with no page body");
  // THE EXPECTATION IS A CONSTANT, NOT A FUNCTION OF THE FIRE MODE. The first
  // version of this line read `g_fire == 1 ? 1 : 0`, which moved the goalpost
  // with the input and made fire mode 1 pass -- a detector that could not fire,
  // which is worse than no detector because it looks like enforcement.
  check_eq(static_cast<long>(whole_plan.ledger.no_page_body), 0,
           "every patch the directory calls ground has a page body behind it");
  check_eq(static_cast<long>(ra.resource_misses), 0,
           "so the renderer resolved every record it was handed");

  const Footprint fp = footprint_of(canvas_a, 0, zhao_abi::VIDEO_Z60);

  // THE ISLAND MUST BE THE SIZE THE DIRECTORY SAYS IT IS, and that is a
  // PREDICTION, not a threshold. A pixel count alone cannot see the failure
  // this catches: 793 patches all stacked on one spot paint about as many
  // pixels as 793 patches spread across an island, and both look like "the
  // terrain rendered". The disc's own patch-index range gives its metre span,
  // the camera gives metres per pixel, and the two give the bbox in advance.
  //
  //   d_ndc = scale_raw * span_metres / 65536      (ortho_map_at, w = 1)
  //   d_px  = d_ndc * half_extent                  (project_vertex, qformats 8)
  //
  // The first version of this check was a hand-picked "> 100 px", it failed at
  // 93 px, and 93 px is EXACTLY RIGHT: the disc spans 31 patch columns = 1984 m,
  // and 16 * 1984 / 65536 * 192 = 93.0. The threshold was wrong, not the render.
  // Predicting the number is the only version of this check worth having.
  int32_t min_ix = kSide, max_ix = -1, min_iz = kSide, max_iz = -1;
  for (int32_t iz = 0; iz < kSide; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix)
      if (disc.find(ix, iz).outcome == isl::Outcome::kResident) {
        if (ix < min_ix) min_ix = ix;
        if (ix > max_ix) max_ix = ix;
        if (iz < min_iz) min_iz = iz;
        if (iz > max_iz) max_iz = iz;
      }
  const int32_t span_x_m = (max_ix - min_ix + 1) * patch_m;
  const int32_t span_z_m = (max_iz - min_iz + 1) * patch_m;
  const int32_t want_w = static_cast<int32_t>(16LL * span_x_m * 192 / 65536);
  const int32_t want_h = static_cast<int32_t>(16LL * span_z_m * 120 / 65536);
  const int32_t one_patch_w = static_cast<int32_t>(16LL * patch_m * 192 / 65536);
  std::printf("  whole-island frame painted %u px, bbox %ux%u at (%u,%u); directory says "
              "%d x %d m -> %d x %d px (one patch = %d px)\n",
              fp.painted, fp.width(), fp.height(), fp.x0, fp.y0, span_x_m, span_z_m, want_w,
              want_h, one_patch_w);
  check(fp.painted > 2000, "the island actually rasterises -- thousands of pixels, not a dot");
  check(fp.width() >= want_w - 2 && fp.width() <= want_w + 2,
        "the drawn island is exactly as WIDE as the directory's patch range predicts");
  check(fp.height() >= want_h - 2 && fp.height() <= want_h + 2,
        "and exactly as TALL -- 793 patches spread ACROSS an island, not stacked on one spot");
  check(want_w > one_patch_w * 20,
        "and that span is more than twenty times a single 64 m patch, so this is an "
        "island-scale frame and not one patch drawn 793 times");

  {  // determinism: a second, independent renderer must produce the same bytes
    if (g_fire == 3) {
      // FIRE: the world changes between the two renders. It has to change the
      // SLOPES, and it has to change many of them. The first attempt bumped ONE
      // vertex of ONE patch by 16 m and the CRC did not move -- at this camera
      // the whole island is 93 px wide, so a patch is ~3 px and its 2,048
      // triangles are almost all sub-pixel. Adding a constant would be just as
      // invisible: a top-down ortho takes screen position from x/z only, so a
      // uniform lift changes no normal and no shade. Doubling the relief across
      // the island changes every normal, which is what a shading path can see.
      for (int32_t iz = 0; iz < kSide; ++iz)
        for (int32_t ix = 0; ix < kSide; ++ix) {
          rnd::TerrainPatch* p = disc_scene.page(ix, iz, 33, 33);
          if (p == nullptr) continue;
          for (size_t k = 0; k < p->heights.size(); ++k)
            p->heights[k] = static_cast<int16_t>(p->heights[k] * 2);
        }
    }
    rnd::SoftwareRenderer rend_b;
    rnd::RenderCanvas canvas_b;
    const rnd::RenderResult rb =
        rend_b.render_frame(whole_plan.packet, 0, canvas_b, disc_scene.resources());
    check_eq(static_cast<long>(rb.canvas_crc32c), static_cast<long>(ra.canvas_crc32c),
             "the island-scale frame is deterministic: the same packet renders the same bytes");
    check_eq(static_cast<long>(rb.displayed_crc32c), static_cast<long>(ra.displayed_crc32c),
             "including the displayed stream");
  }

  // ---- the seam between two patches --------------------------------------
  // An island is only one surface if its patches AGREE on their shared edge.
  // 793 individually-correct patches with mismatched borders is 793 cliffs.
  {
    rnd::TerrainPatch* left = disc_scene.page(kSide / 2, kSide / 2, 33, 33);
    rnd::TerrainPatch* right = disc_scene.page(kSide / 2 + 1, kSide / 2, 33, 33);
    check(left != nullptr && right != nullptr, "two adjacent patches exist at the island centre");
    if (left != nullptr && right != nullptr) {
      check_eq(right->env_x0, left->env_x1,
               "adjacent patches share an edge exactly -- no gap, no overlap");
      if (g_fire == 6) right->heights[0] = static_cast<int16_t>(right->heights[0] + 2048);
      int mismatches = 0;
      for (int j = 0; j < 33; ++j) {
        const int16_t a = left->heights[static_cast<size_t>(j) * 33 + 32];
        const int16_t b = right->heights[static_cast<size_t>(j) * 33 + 0];
        if (a != b) ++mismatches;
      }
      check_eq(mismatches, 0,
               "and their shared vertex column carries identical heights -- the island is one "
               "continuous surface, not 793 separate tables");
    }
  }

  // =========================================================================
  // B. THE 8 KM TRAVERSAL
  // =========================================================================
  isl::Desc rd = dd;
  rd.island_id = 0x52u;
  isl::Directory ribbon(rd);
  const std::size_t ribbon_patches = build_ribbon(ribbon);
  check_eq(static_cast<long>(ribbon_patches), static_cast<long>(kSide) * 6,
           "the 8 km ribbon island is 125 columns x 6 rows of ground");
  check(ribbon_patches <= static_cast<std::size_t>(kResidency),
        "an 8 km-LONG island fits the residency, which is the case Missingterrain names");

  isl::Scene ribbon_scene(ribbon, kMaterial, ground_material());
  for (int32_t iz = kRibbonZ0; iz <= kRibbonZ1; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix) {
      rnd::TerrainPatch* p = ribbon_scene.page(ix, iz, 33, 33);
      if (p != nullptr) author_page(*p);
    }
  check_eq(static_cast<long>(ribbon_scene.page_count()), static_cast<long>(ribbon_patches),
           "every ribbon patch has a page body");

  // Residency, driven from the same camera the frame is drawn from.
  zref::residency::Arena arena(0x1000000u, kPageBytes, kResidency);
  isl::Streamer streamer(ribbon, arena);
  streamer.configure(kPageBytes, zref::mem::GuardRegion{0, 64u << 20});
  zref::residency::Ledger rl{};

  constexpr int32_t kViewRadius = 3;   // 7x7 patches = 448 m of hysteresis window
  constexpr int32_t kStepPatches = 8;  // 512 m per step: a fast flight, real churn
  const int32_t cam_z_raw = ((kRibbonZ0 + 3) * patch_m) << 16;  // down the ribbon's spine
  // 7 patches = 448 m across the 384 px canvas: scale = 65536 / 224 = 292.
  constexpr int32_t kTravScale = 292;

  struct Step {
    int32_t cam_x_m;
    int32_t centre_ix;
    int32_t predicted;
    uint32_t issued;
    uint32_t crc;
    uint32_t painted;
    uint32_t published;
    uint32_t evicted;
    uint32_t returned;
    std::size_t live;
  };
  std::vector<Step> steps;

  const auto fly = [&](bool forward, uint32_t& frame_id) {
    const int32_t n = kSide / kStepPatches;  // 15 steps of 8 patches
    for (int32_t k = 0; k <= n; ++k) {
      const int32_t idx = forward ? k : n - k;
      // The last step is pinned to the FAR EDGE rather than to 15*8 = 120, so
      // the flight covers all 125 columns -- 8 km end to end and not 7.7.
      int32_t cam_ix = idx == n ? kSide - 1 : idx * kStepPatches;
      if (g_fire == 4) cam_ix = 60;  // FIRE: the camera never moves
      const int32_t cam_x_raw = ((cam_ix * patch_m) + patch_m / 2) << 16;

      const isl::View v = isl::view_for_camera(rd, cam_x_raw, cam_z_raw, kViewRadius);
      const isl::Stats st = streamer.update(v, &rl);
      const zhao_abi::ZhMat4fx cam = isl::ortho_map_at(kTravScale, cam_x_raw, cam_z_raw);
      isl::View frame_view = v;
      if (g_fire == 2) frame_view.radius = kViewRadius - 1;  // FIRE: draw less than is in view
      const isl::FramePlan plan = ribbon_scene.build_frame(frame_id, cam, frame_view);

      rnd::SoftwareRenderer rend;
      rnd::RenderCanvas canvas;
      const rnd::RenderResult r =
          rend.render_frame(plan.packet, 0, canvas, ribbon_scene.resources());
      const Footprint f = footprint_of(canvas, 0, zhao_abi::VIDEO_Z60);

      Step s;
      s.cam_x_m = cam_x_raw >> 16;
      s.centre_ix = v.centre_ix;
      s.predicted = ribbon_closed_form(v);
      s.issued = plan.ledger.issued;
      s.crc = r.canvas_crc32c;
      s.painted = f.painted;
      s.published = st.published;
      s.evicted = st.evicted;
      s.returned = st.returned;
      s.live = streamer.live_count();
      steps.push_back(s);

      check_eq(r.status, zhao_abi::ZH_ABI_OK, "traversal step renders");
      check_eq(static_cast<long>(r.resource_misses), 0, "traversal step has every page it drew");
      check_eq(static_cast<long>(plan.ledger.issued), directory_count_in_window(ribbon, v),
               "step: patches drawn == the DIRECTORY's own count of ground in view");
      check_eq(static_cast<long>(plan.ledger.issued), s.predicted,
               "step: patches drawn == the island's closed-form prediction");
      check_eq(static_cast<long>(streamer.live_count()), static_cast<long>(plan.ledger.issued),
               "step: what is RESIDENT is exactly what is DRAWN");
      check(f.painted > 500, "step: the ribbon is actually on screen");
      ++frame_id;
    }
  };

  uint32_t frame_id = 100;
  fly(true, frame_id);
  const std::size_t forward_steps = steps.size();
  if (g_fire != 5) fly(false, frame_id);  // FIRE 5: never come back

  std::printf("\n  8 km traversal: %zu steps (%zu out, %zu back), view radius %d patches\n",
              steps.size(), forward_steps, steps.size() - forward_steps, kViewRadius);
  std::printf("  %4s %7s %5s %5s %5s %8s %6s %5s %5s %10s\n", "step", "cam_x", "ix", "pred",
              "drawn", "painted", "pub", "evic", "ret", "crc32c");
  for (std::size_t i = 0; i < steps.size(); ++i) {
    const Step& s = steps[i];
    std::printf("  %4zu %6d m %5d %5d %5u %8u %6u %5u %5u 0x%08X\n", i, s.cam_x_m, s.centre_ix,
                s.predicted, s.issued, s.painted, s.published, s.evicted, s.returned, s.crc);
  }

  // ---- what the traversal must have DONE ---------------------------------
  uint32_t total_pub = 0, total_evict = 0, total_return = 0;
  uint32_t max_issued = 0;
  for (const Step& s : steps) {
    total_pub += s.published;
    total_evict += s.evicted;
    total_return += s.returned;
    if (s.issued > max_issued) max_issued = s.issued;
  }
  std::printf("  residency: %u published, %u evicted, %u returned; peak %u of %d pages\n",
              total_pub, total_evict, total_return, streamer.peak(), kResidency);

  // ABSOLUTE, not `g_fire == 5 ? 16 : 32`. See the header note: an expectation
  // that moves with the perturbation is a detector that cannot fire.
  check_eq(static_cast<long>(steps.size()), 32, "the flight is 8 km out and 8 km back");
  check(total_evict > 0, "the flight forced eviction -- patches left the window");
  check(total_return > 0,
        "and patches CAME BACK -- the return leg is the half that tests reclamation");
  check_eq(static_cast<long>(streamer.peak()), static_cast<long>(max_issued),
           "the residency high-water mark equals the largest frame the flight drew");
  check(streamer.peak() <= static_cast<uint32_t>(kResidency),
        "and it never exceeded the 1,024-page residency");

  {  // the picture must CHANGE as the camera moves, or the counts prove nothing
    std::size_t distinct = 0;
    for (std::size_t i = 0; i < forward_steps; ++i) {
      bool seen = false;
      for (std::size_t j = 0; j < i; ++j)
        if (steps[j].crc == steps[i].crc) seen = true;
      if (!seen) ++distinct;
    }
    std::printf("  distinct frames on the outbound leg: %zu of %zu\n", distinct, forward_steps);
    check_eq(static_cast<long>(distinct), static_cast<long>(forward_steps),
             "every outbound step drew a DIFFERENT picture -- the camera really traversed 8 km");
  }

  {  // the flight is reproducible end to end, not merely each frame in isolation
    isl::Directory ribbon2(rd);
    build_ribbon(ribbon2);
    isl::Scene scene2(ribbon2, kMaterial, ground_material());
    for (int32_t iz = kRibbonZ0; iz <= kRibbonZ1; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix) {
        rnd::TerrainPatch* p = scene2.page(ix, iz, 33, 33);
        if (p != nullptr) author_page(*p);
      }
    std::size_t mismatches = 0, i = 0;
    uint32_t fid = 100;
    const int32_t n = kSide / kStepPatches;
    for (int leg = 0; leg < (g_fire == 5 ? 1 : 2); ++leg) {
      for (int32_t k = 0; k <= n; ++k) {
        const int32_t idx = leg == 0 ? k : n - k;
        int32_t cam_ix = idx == n ? kSide - 1 : idx * kStepPatches;
        if (g_fire == 4) cam_ix = 60;
        const int32_t cam_x_raw = ((cam_ix * patch_m) + patch_m / 2) << 16;
        const isl::View v = isl::view_for_camera(rd, cam_x_raw, cam_z_raw, kViewRadius);
        const zhao_abi::ZhMat4fx cam = isl::ortho_map_at(kTravScale, cam_x_raw, cam_z_raw);
        isl::View fv = v;
        if (g_fire == 2) fv.radius = kViewRadius - 1;
        const isl::FramePlan plan = scene2.build_frame(fid, cam, fv);
        rnd::SoftwareRenderer rend;
        rnd::RenderCanvas canvas;
        const rnd::RenderResult r = rend.render_frame(plan.packet, 0, canvas, scene2.resources());
        if (i < steps.size() && r.canvas_crc32c != steps[i].crc) ++mismatches;
        ++i;
        ++fid;
      }
    }
    check_eq(static_cast<long>(mismatches), 0,
             "the whole 8 km flight replays byte-identically from a rebuilt world");
  }

  // ---- Stats::peak_resident, observed and REPORTED (not fixed here) -------
  //
  // `island::Stats` is constructed fresh inside every `Streamer::update`, so
  // `st.peak_resident` starts at 0 on each call and the guarded assignment
  // `if (live_.size() > st.peak_resident)` is true every time. It is therefore
  // the CURRENT live count, not a high-water mark, whatever the comment beside
  // it says. `Streamer::peak()` (the member `peak_`) is the real high-water
  // mark and is what the assertions above use. Reported, not touched:
  // zref_island_stream.hpp belongs to another lane.
  if (!steps.empty()) {
    // Deliberately a NARROWER window than anything the flight used. A real
    // high-water mark cannot go down; the number printed below does.
    const uint32_t high_water_before = streamer.peak();
    const isl::Stats narrow =
        streamer.update(isl::view_for_camera(rd, ((60 * patch_m) << 16), cam_z_raw, 1), &rl);
    std::printf("  narrow window: Stats::peak_resident = %u, live = %zu, Streamer::peak() = %u"
                "   <-- peak_resident FOLLOWED the live count DOWN from %u\n",
                narrow.peak_resident, streamer.live_count(), streamer.peak(), high_water_before);
    check_eq(static_cast<long>(streamer.peak()), static_cast<long>(high_water_before),
             "Streamer::peak() is the real high-water mark and does NOT fall when the window "
             "shrinks (Stats::peak_resident, printed above, does -- reported, not fixed here: "
             "zref_island_stream.hpp belongs to another lane)");
  }

  const auto t_end = std::chrono::steady_clock::now();
  std::printf("\n[island_scene_traversal] %d checks, %d failures, %lld ms\n", g_checks, g_failures,
              static_cast<long long>(
                  std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count()));
  if (g_fire != 0 && g_failures == 0)
    std::printf("FAIL: fire mode %d perturbed an input and NOTHING failed\n", g_fire);
  return g_failures != 0 ? 1 : 0;
}
