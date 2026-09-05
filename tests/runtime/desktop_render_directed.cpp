// desktop_render_directed.cpp — the host's frame actually draws, in the right
// place, through the reference renderer.
// Authored 2026-09-05 (roadmap, parallel software lane).
//
// ---------------------------------------------------------------------------
// WHAT THIS IS FOR
// ---------------------------------------------------------------------------
// The roadmap's instruction for the software lane was to
//
//   > turn the existing exact reference/rendering machinery into a reusable
//   > console backend, rather than adding another renderer beside it
//
// and its acceptance test is that the game "uses the same simulation and
// resource/command interfaces intended for the console". So the host builds a
// FramePlan through `zcon::build_frame` and hands it to
// `zref::render::SoftwareRenderer` -- the same renderer the capture tools and
// the RTL differentials already agree with.
//
// Two defects were found wiring that up, and both are pinned here because both
// were INVISIBLE TO EVERY COUNTER:
//
//   1. The resource table is keyed by the WIRE handle, not by a bare index.
//      handle32 is {index:24, generation:8}, so a form published at index 1,
//      generation 1 is 0x101 -- not 1. Keyed wrongly, the host reported 400
//      frames rendered, status 0, and a blank canvas, because a resource miss
//      is not an error. Only counting painted bytes caught it.
//
//   2. The demo input script was a fast sawtooth with a near-zero mean, so the
//      wizards ended a tenth of a metre from where they started. Frames
//      rendered; the markers never left their pixel; the canvas CRC alternated
//      between exactly two values for every run length.
//
// So this file does not ask "did it render". It asks WHERE THE MARKER LANDED,
// against a projection computed by hand, and it asks whether the picture moves
// when the simulation does.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zcon/frame_build.hpp"
#include "zcon/zcon.hpp"
#include "zref/zref_render.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

// The host's own numbers, restated here rather than shared, so a silent change
// on either side shows up as a failure instead of tracking along.
constexpr int32_t kScale = 2048;     // ortho_topdown: world +-32 m across NDC
constexpr int32_t kMarkerHalf = 8;   // screen-space px half-extent
constexpr uint8_t kScreenSpaceSize = 0x2;  // DrawForm flags b1

zcon::Handle h_form{0, 1, zcon::ResourceKind::kMeshStream};
zcon::Handle h_xform{1, 1, zcon::ResourceKind::kMeshStream};

zref::render::FormPattern red_marker() {
  zref::render::FormPattern p;
  for (int i = 0; i < 64; ++i) {
    p.rgb[i * 3 + 0] = 230;
    p.rgb[i * 3 + 1] = 60;
    p.rgb[i * 3 + 2] = 60;
    p.mask[i] = 1;
  }
  return p;
}

zcon::ViewSpec top_down_view() {
  zcon::ViewSpec v;
  v.m[0] = kScale;    // ndc.x <- world.x
  v.m[6] = kScale;    // ndc.y <- world.z
  v.m[10] = 1 << 16;
  v.m[15] = 1 << 16;  // w = 1, orthographic
  return v;
}

// Render one frame with the marker at a given WORLD position (fx16) and return
// the centroid of the non-black pixels, plus how many there were.
struct Spot {
  int n = 0;
  int cx = 0, cy = 0;
};

Spot render_marker_at(int32_t wx, int32_t wz, bool key_correctly = true,
                      uint32_t* misses_out = nullptr) {
  zref::render::RenderResources res;
  const uint32_t fk = key_correctly ? zcon::detail::handle32(h_form) : h_form.index;
  const uint32_t xk = key_correctly ? zcon::detail::handle32(h_xform) : h_xform.index;
  res.forms.emplace_back(fk, red_marker());
  res.transforms.emplace_back(
      xk, zref::render::FormTransform{wx, 0, wz, kMarkerHalf << 16});

  zcon::FramePlan plan;
  plan.frame_id = 1;
  plan.sequence = 1;
  plan.resource_epoch = 1;
  plan.views.push_back(top_down_view());
  zcon::DrawItem d;
  d.form = h_form;
  d.transform = h_xform;
  d.material_set = h_form;
  d.viewport_mask = 0x1;
  d.flags = kScreenSpaceSize;
  plan.draws.push_back(d);

  zref::render::SoftwareRenderer r;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult rr =
      r.render_frame(zcon::build_frame(plan), 0, canvas, res);
  if (misses_out) *misses_out = rr.resource_misses;

  const zhao_abi::video_mode m = r.latched_mode();
  const uint32_t w = zref::render::canvas_width(m);
  const uint32_t h = zref::render::canvas_height(m);
  const uint8_t* px = canvas.slot[0].data();
  Spot s;
  long long sx = 0, sy = 0;
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      const uint32_t i = y * w + x;
      if (px[i * 2] != 0 || px[i * 2 + 1] != 0) {
        ++s.n;
        sx += x;
        sy += y;
      }
    }
  if (s.n) {
    s.cx = static_cast<int>(sx / s.n);
    s.cy = static_cast<int>(sy / s.n);
  }
  return s;
}

// ---------------------------------------------------------------------------

void test_a_marker_lands_where_the_projection_says() {
  // World origin: dead centre of a 384x240 view.
  const Spot c = render_marker_at(0, 0);
  check(c.n == kMarkerHalf * 2 * kMarkerHalf * 2,
        "a screen-space marker of half-extent 8 covers exactly 16x16 px",
        kMarkerHalf * 2 * kMarkerHalf * 2, c.n);
  check(c.cx == 191 || c.cx == 192, "and is centred horizontally", 192, c.cx);
  check(c.cy == 119 || c.cy == 120, "and vertically", 120, c.cy);

  // -16 m in x: ndc -0.5 -> 192 - 96 = 96. This is the hand-computed value the
  // host's rendered frame was checked against, kept as the anchor.
  const Spot l = render_marker_at(-16 << 16, 0);
  check(l.cx >= 95 && l.cx <= 96, "-16 m maps to x = 96 (ndc -0.5)", 96, l.cx);
  check(l.cy >= 119 && l.cy <= 120, "and does not move in y", 120, l.cy);

  // +12.06 m in Z drives SCREEN Y, not world y: ortho_topdown reads screen y
  // from world z. A version that read world y would put this back at centre.
  const Spot d = render_marker_at(0, (12 << 16) + (4 << 12));
  check(d.cy > 160 && d.cy < 170, "world Z drives SCREEN Y (top-down view)",
        165, d.cy);
  check(d.cx >= 191 && d.cx <= 192, "and x is unchanged", 192, d.cx);
}

void test_the_picture_moves_when_the_position_does() {
  // The demo-script defect in one check: two different world positions must
  // not produce the same picture. A near-zero-mean input made this false for
  // 400 ticks while every counter reported healthy work.
  const Spot a = render_marker_at(-8 << 16, 0);
  const Spot b = render_marker_at(8 << 16, 0);
  check(a.cx != b.cx, "distinct world positions give distinct pixels",
        1, a.cx != b.cx ? 1 : 0);
  check(b.cx - a.cx == 96,
        "and the displacement matches the scale: 16 m at 6 px/m", 96,
        b.cx - a.cx);
}

void test_a_bare_index_key_misses_and_draws_nothing() {
  // The keying defect, pinned. A miss is NOT an error -- status stays 0 -- so
  // the only evidence is the miss counter and the empty canvas.
  uint32_t misses = 0;
  const Spot s = render_marker_at(0, 0, /*key_correctly=*/false, &misses);
  check(misses == 2,
        "keying the table by the bare index misses BOTH form and transform", 2,
        misses);
  check(s.n == 0, "and nothing is drawn -- silently", 0, s.n);

  // ...and the correct key is the same call with one thing changed, so this
  // test cannot pass by the renderer being broken outright.
  uint32_t ok_misses = 0;
  const Spot g = render_marker_at(0, 0, /*key_correctly=*/true, &ok_misses);
  check(ok_misses == 0, "the wire key hits", 0, ok_misses);
  check(g.n > 0, "and draws", 1, g.n > 0 ? 1 : 0);
}

void test_the_marker_keeps_its_colour_through_rgb565() {
  zref::render::RenderResources res;
  res.forms.emplace_back(zcon::detail::handle32(h_form), red_marker());
  res.transforms.emplace_back(
      zcon::detail::handle32(h_xform),
      zref::render::FormTransform{0, 0, 0, kMarkerHalf << 16});
  zcon::FramePlan plan;
  plan.frame_id = 1;
  plan.sequence = 1;
  plan.resource_epoch = 1;
  plan.views.push_back(top_down_view());
  zcon::DrawItem d;
  d.form = h_form;
  d.transform = h_xform;
  d.material_set = h_form;
  d.viewport_mask = 0x1;
  d.flags = kScreenSpaceSize;
  plan.draws.push_back(d);

  zref::render::SoftwareRenderer r;
  zref::render::RenderCanvas canvas;
  r.render_frame(zcon::build_frame(plan), 0, canvas, res);

  const uint32_t w = zref::render::canvas_width(r.latched_mode());
  const uint8_t* px = canvas.slot[0].data();
  const uint32_t i = 120u * w + 192u;
  const uint16_t v = static_cast<uint16_t>(px[i * 2] | (px[i * 2 + 1] << 8));
  const int r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
  // 230/60/60 through RGB565 with the resolve's ordered dither: red dominant,
  // green and blue low. Bands rather than exact values, because the dither is
  // position-dependent by design and pinning one value would make this test a
  // hostage to the dither table.
  check(r5 >= 26, "the red marker is red after the RGB565 resolve", 26, r5);
  check(g6 <= 18, "green stays low", 18, g6);
  check(b5 <= 9, "blue stays low", 9, b5);
}

}  // namespace

int main() {
  test_a_marker_lands_where_the_projection_says();
  test_the_picture_moves_when_the_position_does();
  test_a_bare_index_key_misses_and_draws_nothing();
  test_the_marker_keeps_its_colour_through_rgb565();

  if (g_failed) {
    std::printf("[desktop_render_directed] %d/%d checks FAILED\n", g_failed,
                g_checks);
    return 1;
  }
  std::printf("[desktop_render_directed] %d checks passed\n", g_checks);
  return 0;
}
