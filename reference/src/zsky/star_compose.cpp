// star_compose.cpp — the ZRef celestial compositor preview: glints, halos,
// discs, the occlusion probe latch, fade counters, and the flare splat +
// tinted composite, drawn into the renderer's RGB888 working canvas + depth
// plane (spec/stars_and_flares.md §1/§3/§4/§5, §9 placement; [phase3-preview]
// scope note in zref_star.hpp).
//
// Material law (§1, charter §15):
//   star_disc_masked   — CLUT8 nearest, alpha-test index 0, Z-test on /
//                        Z-write off, glow-tag write (strength = the texel's
//                        CLUT intensity)
//   star_halo_additive — CLUT8 nearest, dst = sat(dst + src), Z-test on /
//                        Z-write off, glow-tag write
//   glints             — PART.SOFT additive 1–2 px, glow-tag write, Z-test
//                        against the sky far constant (§7)
// Depth law (§3): celestial depth = kStarDepth (sky-prefill far + 1);
// strict-greater 1/w compare (qformats §8) — beats the sky backdrop's 0,
// loses to every real surface. Depth is never written.
// Probe law (§5): POST.GATHER latches the effect-tag byte at the probe
// pixel; visible ⟺ (tag >> 6) == GLOW. The tag plane here is transient
// per call — the preview stand-in for charter §8's 8-bit tag lane; the
// probe reads the latch, never the framebuffer.

#include "zref/zref_star.hpp"

namespace zref {
namespace star {

namespace {

inline uint8_t sat_add_u8(uint8_t d, uint8_t s) {
  const uint32_t v = static_cast<uint32_t>(d) + s;
  return static_cast<uint8_t>(v > 255 ? 255 : v);
}

struct Plane {
  uint8_t* rgb;
  int32_t* depth;
  uint8_t* tag;
  uint32_t w;
  // viewport clip rect
  int32_t x0, y0, x1, y1;
};

// one masked/additive CLUT8 sprite quad at (cx,cy), half-size r px, sampled
// nearest from a 128-texel sprite through a 64-entry palette. tag_cap clamps
// the glow-tag strength below the texel's own intensity (the §15 ghosts draw
// a flat level; the tag carries the intensity AS DRAWN). skip2 < 0: no skip;
// else pixels with (x−scx)² + (y−scy)² < skip2 are left untouched (the §15
// halo-skip: a trail ghost never renders inside the star's additive halo,
// where halo ramp × ghost would multiply the palette).
void draw_clut_quad(Plane& p, const Sprite8& spr, const uint8_t pal[64][3], int32_t cx, int32_t cy,
                    int32_t r, bool additive, uint32_t* frags, uint8_t tag_cap = 63,
                    int64_t skip2 = -1, int32_t scx = 0, int32_t scy = 0) {
  if (r <= 0) return;
  const int32_t qx0 = cx - r, qy0 = cy - r;  // quad origin; extent 2r
  int32_t x0 = qx0, y0 = qy0, x1 = cx + r, y1 = cy + r;
  if (x0 < p.x0) x0 = p.x0;
  if (y0 < p.y0) y0 = p.y0;
  if (x1 > p.x1) x1 = p.x1;
  if (y1 > p.y1) y1 = p.y1;
  const int64_t wq = 2 * static_cast<int64_t>(r);
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * spr.h) / wq);
    for (int32_t x = x0; x < x1; ++x) {
      if (skip2 >= 0) {
        const int64_t dx = x - scx, dy = y - scy;
        if (dx * dx + dy * dy < skip2) continue;  // §15 halo-skip
      }
      const size_t idx = static_cast<size_t>(y) * p.w + x;
      if (!(kStarDepth > p.depth[idx])) continue;  // Z-test on (§8 strict)
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * spr.w) / wq);
      const uint8_t t = spr.pix[static_cast<size_t>(sy) * spr.w + sx];
      if (t == 0) continue;  // alpha-test index 0 / additive identity
      const size_t rgb_idx = idx * 3;
      if (additive) {
        p.rgb[rgb_idx] = sat_add_u8(p.rgb[rgb_idx], pal[t][0]);
        p.rgb[rgb_idx + 1] = sat_add_u8(p.rgb[rgb_idx + 1], pal[t][1]);
        p.rgb[rgb_idx + 2] = sat_add_u8(p.rgb[rgb_idx + 2], pal[t][2]);
      } else {
        p.rgb[rgb_idx] = pal[t][0];
        p.rgb[rgb_idx + 1] = pal[t][1];
        p.rgb[rgb_idx + 2] = pal[t][2];
      }
      p.tag[idx] = glow_tag(t < tag_cap ? t : tag_cap);  // strength as drawn
      if (frags) ++(*frags);
    }
  }
}

// Stamp one CLUT8 silhouette into a six-bit scratch plane. Corona texels add;
// face texels replace, matching the current halo-then-disc composition order.
// floor6 carries the distance washout of a disc while preserving its texture.
void stamp_intensity_quad(std::vector<uint8_t>& dst, const Plane& p, const Sprite8& spr, int32_t cx,
                          int32_t cy, int32_t r, bool additive, uint8_t floor6 = 0) {
  if (r <= 0) return;
  const int32_t qx0 = cx - r, qy0 = cy - r;
  int32_t x0 = qx0, y0 = qy0, x1 = cx + r, y1 = cy + r;
  if (x0 < p.x0) x0 = p.x0;
  if (y0 < p.y0) y0 = p.y0;
  if (x1 > p.x1) x1 = p.x1;
  if (y1 > p.y1) y1 = p.y1;
  const int64_t wq = 2 * static_cast<int64_t>(r);
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * spr.h) / wq);
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * spr.w) / wq);
      uint8_t v = spr.pix[static_cast<size_t>(sy) * spr.w + sx] & 0x3Fu;
      if (v == 0) continue;
      if (v < floor6) v = floor6;
      uint8_t& d = dst[static_cast<size_t>(y) * p.w + x];
      if (additive) {
        const uint16_t sum = static_cast<uint16_t>(d) + v;
        d = static_cast<uint8_t>(sum > 63 ? 63 : sum);
      } else {
        d = v;
      }
    }
  }
}

uint8_t disc_floor6(const ComposeLight& L) {
  if (L.r_milli <= 0 || L.d_milli <= 0) return 0;
  if (L.d_milli >= (63 * L.r_milli + 11) / 12) return 63;
  const int64_t v = (12 * L.d_milli) / L.r_milli;
  return static_cast<uint8_t>(v > 63 ? 63 : v);
}

int32_t trail_disc_radius(const ComposeLight& L, int32_t corona_r) {
  if (corona_r <= 0) return 0;
  int32_t dr = L.disc_r_px;
  if (dr <= 0) return 0;
  if (dr > kDiscRMaxPx) dr = kDiscRMaxPx;
  int32_t hr = L.halo_r_px;
  const int32_t hmax = L.halo_r_max_px < kHaloRMaxZ60Px ? L.halo_r_max_px : kHaloRMaxZ60Px;
  if (hr > hmax) hr = hmax;
  if (hr > 0) {
    dr = static_cast<int32_t>((static_cast<int64_t>(dr) * corona_r + hr / 2) / hr);
  } else if (dr > corona_r) {
    dr = corona_r;
  }
  return dr < 1 ? 1 : dr;
}

// The source-age step: decay, then diffuse.
//
// AMENDMENT v1.3 (2026-08-18) - THE KERNEL FOLLOWS THE MOTION.
//
// Noctis samples (x,y+1), (x+1,y+1), (x,y+2), (x+1,y+2), which moves energy up
// and left on every pass no matter which way the light is travelling. That is
// right in Noctis, where the star field slides past a ship flying one way. It
// is wrong here: our suns ping-pong horizontally, so a fixed up-left bias puts
// the smear ABOVE the sun instead of behind it, and does so however the ghosts
// are placed. The trail-at-the-top the owner reported is THIS KERNEL, not the
// stamps.
//
// The offsets are therefore rotated onto the direction of travel. The two
// on-axis taps sample AHEAD of each pixel along the motion, which propagates
// energy BACKWARD; two more taps sample either side of the first, spreading the
// smear across its own axis. On-axis extent makes the trail long, across-axis
// spread is what reads as haze. A sun moving in any direction trails behind
// itself, and purely horizontal motion gets no vertical smear at all.
//
// Unchanged: the mean is one shift, every sample is six-bit, out-of-view
// samples are zero, and an age-g sample receives exactly g decay and diffusion
// steps. Only the kernel's orientation and its pass count move.
struct TrailKernel {
  int32_t ax = 0, ay = 1;  // on-axis: sample ahead, so energy goes back
  int32_t px = 1, py = 0;  // across-axis: the haze spread
  int passes = 3;
};

// Eight-way choice rather than a blend: every offset stays a whole texel, so
// the mean stays exact and no interpolation enters a six-bit plane.
TrailKernel trail_kernel_for(int32_t vx, int32_t vy) {
  TrailKernel k;
  if (vx == 0 && vy == 0) return k;  // static; the section 15 skip means unused
  const int32_t sx = vx > 0 ? 1 : (vx < 0 ? -1 : 0);
  const int32_t sy = vy > 0 ? 1 : (vy < 0 ? -1 : 0);
  const int64_t mx = vx < 0 ? -static_cast<int64_t>(vx) : vx;
  const int64_t my = vy < 0 ? -static_cast<int64_t>(vy) : vy;
  // Within a factor of two on both axes the travel is diagonal enough to smear
  // diagonally; otherwise snap to the dominant axis.
  if (mx > 0 && my > 0 && mx <= 2 * my && my <= 2 * mx) {
    k.ax = sx;
    k.ay = sy;
    k.px = sx;
    k.py = -sy;
  } else if (mx >= my) {
    k.ax = sx;
    k.ay = 0;
    k.px = 0;
    k.py = 1;
  } else {
    k.ax = 0;
    k.ay = sy;
    k.px = 1;
    k.py = 0;
  }
  return k;
}

void trail_source_step(std::vector<uint8_t>& intensity, std::vector<uint8_t>& work,
                       const Plane& p, const TrailKernel& k) {
  for (int32_t y = p.y0; y < p.y1; ++y)
    for (int32_t x = p.x0; x < p.x1; ++x) {
      uint8_t& v = intensity[static_cast<size_t>(y) * p.w + x];
      v = trail_fade(v & 0x3Fu);
    }

  auto tap = [&](int32_t x, int32_t y) -> uint16_t {
    if (x < p.x0 || x >= p.x1 || y < p.y0 || y >= p.y1) return 0;
    return intensity[static_cast<size_t>(y) * p.w + x];
  };

  // EIGHT taps, mean by one shift of three. Four reach along the motion at
  // 1..4 texels, four sit either side of the first two. A narrower kernel is
  // not enough blur: the ghosts are discs about 9.7 px apart, so a two-tap
  // reach leaves them visible as discrete scallops instead of one haze. Reach
  // is what dissolves the stamps; the across-axis pairs stop the haze from
  // being a hard-edged bar.
  for (int pass = 0; pass < k.passes; ++pass) {
    std::fill(work.begin(), work.end(), 0);
    for (int32_t y = p.y0; y < p.y1; ++y) {
      for (int32_t x = p.x0; x < p.x1; ++x) {
        const uint16_t sum =
            tap(x + k.ax, y + k.ay) + tap(x + 2 * k.ax, y + 2 * k.ay) +
            tap(x + 3 * k.ax, y + 3 * k.ay) + tap(x + 4 * k.ax, y + 4 * k.ay) +
            tap(x + k.ax + k.px, y + k.ay + k.py) + tap(x + k.ax - k.px, y + k.ay - k.py) +
            tap(x + 2 * k.ax + k.px, y + 2 * k.ay + k.py) +
            tap(x + 2 * k.ax - k.px, y + 2 * k.ay - k.py);
        work[static_cast<size_t>(y) * p.w + x] = static_cast<uint8_t>(sum >> 3);
      }
    }
    intensity.swap(work);
  }
}

void composite_trail(Plane& p, const std::vector<uint8_t>& intensity, const uint8_t ramp[64][3],
                     int64_t skip2, int32_t scx, int32_t scy, uint32_t* frags) {
  for (int32_t y = p.y0; y < p.y1; ++y) {
    for (int32_t x = p.x0; x < p.x1; ++x) {
      if (skip2 >= 0) {
        const int64_t dx = x - scx, dy = y - scy;
        if (dx * dx + dy * dy < skip2) continue;
      }
      const size_t idx = static_cast<size_t>(y) * p.w + x;
      const uint8_t v = intensity[idx];
      if (v == 0 || !(kStarDepth > p.depth[idx])) continue;
      const size_t rgb_idx = idx * 3;
      p.rgb[rgb_idx] = sat_add_u8(p.rgb[rgb_idx], ramp[v][0]);
      p.rgb[rgb_idx + 1] = sat_add_u8(p.rgb[rgb_idx + 1], ramp[v][1]);
      p.rgb[rgb_idx + 2] = sat_add_u8(p.rgb[rgb_idx + 2], ramp[v][2]);
      p.tag[idx] = glow_tag(v);
      if (frags) ++(*frags);
    }
  }
}

}  // namespace

void compose_view(uint8_t* rgb888, int32_t* depth, uint32_t w, uint32_t h, uint32_t vx0,
                  uint32_t vy0, uint32_t vw, uint32_t vh, uint32_t tick, const ComposeLight* lights,
                  int n_lights, const GlintPoint* glints, int n_glints, FlareSlots& slots,
                  ComposeStats* stats) {
  (void)h;
  ComposeStats local;
  ComposeStats* st = stats ? stats : &local;

  // transient effect-tag plane (charter §8 tag lane, preview-local)
  std::vector<uint8_t> tag(static_cast<size_t>(w) * (vy0 + vh), 0);
  Plane p{rgb888,
          depth,
          tag.data(),
          w,
          static_cast<int32_t>(vx0),
          static_cast<int32_t>(vy0),
          static_cast<int32_t>(vx0 + vw),
          static_cast<int32_t>(vy0 + vh)};

  // ---- §7 glints (PART.SOFT rung): additive squares, Z-test vs far -------
  for (int i = 0; i < n_glints; ++i) {
    const GlintPoint& gp = glints[i];
    const int32_t s = gp.size_px < 1 ? 1 : gp.size_px;
    for (int32_t dy = 0; dy < s; ++dy) {
      for (int32_t dx = 0; dx < s; ++dx) {
        const int32_t x = gp.x_px - s / 2 + dx;
        const int32_t y = gp.y_px - s / 2 + dy;
        if (x < p.x0 || y < p.y0 || x >= p.x1 || y >= p.y1) continue;
        const size_t idx = static_cast<size_t>(y) * w + x;
        if (!(kStarDepth > depth[idx])) continue;
        uint8_t* dst = &rgb888[idx * 3];
        dst[0] = sat_add_u8(dst[0], gp.rgb[0]);
        dst[1] = sat_add_u8(dst[1], gp.rgb[1]);
        dst[2] = sat_add_u8(dst[2], gp.rgb[2]);
        tag[idx] = glow_tag(gp.intensity6);
        ++st->star_fragments;
      }
    }
  }

  // ---- pass 6: per light, reconstructed smear, then halo before disc ------
  uint8_t pal_d[64][3];
  uint8_t pal_h[64][3];
  for (int i = 0; i < n_lights && i < 4; ++i) {
    const ComposeLight& L = lights[i];
    if (L.ramp == nullptr) continue;

    // ---- §15 bounded reconstruction of Noctis's retained-frame smear ------
    // Replay the ring oldest to newest in one six-bit plane. Every historical
    // source silhouette receives exactly one subtract-8 and two-smoother step
    // per age. Graded corona and scaled disc texels retain radial and surface
    // structure. One class-ramp lookup happens only after reconstruction.
    if (L.trail != nullptr && L.ghost_r_px > 0 && L.corona != nullptr) {
      int32_t gr = L.ghost_r_px;
      const int32_t gmax = L.halo_r_max_px < kHaloRMaxZ60Px ? L.halo_r_max_px : kHaloRMaxZ60Px;
      if (gr > gmax) gr = gmax;

      int32_t hr0 = L.halo_r_px;
      if (hr0 > gmax) hr0 = gmax;
      const int64_t skip2 = hr0 > 0 ? static_cast<int64_t>(hr0) * hr0 : -1;
      const size_t plane_size = static_cast<size_t>(w) * (vy0 + vh);
      std::vector<uint8_t> intensity(plane_size, 0);
      std::vector<uint8_t> work(plane_size, 0);
      const int32_t gdr = L.face != nullptr ? trail_disc_radius(L, gr) : 0;
      const uint8_t floor6 = disc_floor6(L);

      // v1.3: the diffusion follows the sun's CURRENT velocity, taken from the
      // newest retained position to where it is now. One kernel for the whole
      // replay, not one per age: the smear is a single trail behind a single
      // moving light, so every age must diffuse the same way. Deriving it per
      // age would bend the trail wherever the drift reversed.
      TrailKernel kern;
      if (L.trail->length >= 1) {
        uint16_t rx, ry;
        trail_at(*L.trail, 1, rx, ry);
        kern = trail_kernel_for(L.x_px - static_cast<int32_t>(rx),
                                L.y_px - static_cast<int32_t>(ry));
      }

      for (uint32_t g = L.trail->length; g >= 1; --g) {
        uint16_t gx, gy;
        trail_at(*L.trail, g, gx, gy);
        uint16_t nx, ny;
        if (g == 1) {
          nx = static_cast<uint16_t>(L.x_px);
          ny = static_cast<uint16_t>(L.y_px);
        } else {
          trail_at(*L.trail, g - 1, nx, ny);
        }
        const bool moved =
            !(gx == static_cast<uint16_t>(L.x_px) && gy == static_cast<uint16_t>(L.y_px)) &&
            !(gx == nx && gy == ny);
        if (moved) {
          stamp_intensity_quad(intensity, p, *L.corona, gx, gy, gr, true);
          if (gdr > 0) stamp_intensity_quad(intensity, p, *L.face, gx, gy, gdr, false, floor6);
        }
        trail_source_step(intensity, work, p, kern);
      }
      composite_trail(p, intensity, L.ramp, skip2, L.x_px, L.y_px, &st->star_fragments);
    }
    // the push happens AFTER the ghost render and BEFORE the star itself:
    // the ring holds strictly past positions (§15)
    if (L.trail != nullptr)
      trail_push(*L.trail, static_cast<uint16_t>(L.x_px), static_cast<uint16_t>(L.y_px));

    if (L.halo_r_px > 0 && L.corona != nullptr) {
      int32_t hr = L.halo_r_px;
      const int32_t hmax = L.halo_r_max_px < kHaloRMaxZ60Px ? L.halo_r_max_px : kHaloRMaxZ60Px;
      if (hr > hmax) hr = hmax;  // §4 HALO_RMAX (the Measure degradation knob)
      palette_halo(L.ramp, pal_h);
      draw_clut_quad(p, *L.corona, pal_h, L.x_px, L.y_px, hr, true, &st->star_fragments);
    }
    if (L.disc_r_px > 0 && L.face != nullptr) {
      int32_t dr = L.disc_r_px;
      if (dr > kDiscRMaxPx) dr = kDiscRMaxPx;  // §3 DISC_RMAX
      palette_disc(L.ramp, tick, L.d_milli, L.r_milli, pal_d);
      draw_clut_quad(p, *L.face, pal_d, L.x_px, L.y_px, dr, false, &st->star_fragments);
    }
  }

  // ---- §5 probe latch + fade + flare splats + tinted composite -----------
  uint32_t budget = flare::kFlareTexelBudget;
  post::GlowBuffer glow;
  for (int i = 0; i < n_lights && i < 4; ++i) {
    const ComposeLight& L = lights[i];
    // POST.GATHER: latch the tag at the probe pixel (never the framebuffer)
    uint8_t latched = 0;
    if (L.probe_x >= p.x0 && L.probe_x < p.x1 && L.probe_y >= p.y0 && L.probe_y < p.y1)
      latched = tag[static_cast<size_t>(L.probe_y) * w + L.probe_x];
    slots.latched_tag[i] = latched;
    const bool visible = tag_is_glow(latched);
    const bool target = visible && L.in_window != 0 && L.in_front != 0 && L.flare_mode != 0;
    slots.fade_ctr[i] = flare::fade_step(slots.fade_ctr[i], target);
    if (L.flare_mode == 0 || slots.fade_ctr[i] == 0) continue;
    // §2 pulsar duty: the strobe gates the splats INSTANTLY (the fade
    // tracks visibility; a 10-tick flash must not be smeared by it)
    if (L.flare_mode == 2 && !pulsar_active(L.spin_phase)) continue;

    // border fade: signed distance of the light centre to the nearest edge
    const int32_t ex =
        (L.x_px - p.x0) < (p.x1 - 1 - L.x_px) ? (L.x_px - p.x0) : (p.x1 - 1 - L.x_px);
    const int32_t ey =
        (L.y_px - p.y0) < (p.y1 - 1 - L.y_px) ? (L.y_px - p.y0) : (p.y1 - 1 - L.y_px);
    const uint8_t border = flare::border_alpha(ex < ey ? ex : ey);
    if (border == 0) continue;
    const uint8_t fade = flare::fade_alpha(slots.fade_ctr[i]);

    const flare::LightLaw law = flare::light_law(L.d_milli, L.r_milli);
    flare::Splat splats[4];
    const int ns = flare::emit(L.x_px, L.y_px, static_cast<int32_t>(vx0 + vw / 2),
                               static_cast<int32_t>(vy0 + vh / 2), law, splats);
    const flare::Sprites& sp = flare::sprites();
    glow.reset(static_cast<uint16_t>(vw / 4), static_cast<uint16_t>(vh / 4));
    for (int s = 0; s < ns; ++s) {
      const flare::Splat& fs = splats[s];
      const star::Sprite8& spr =
          fs.sprite == 0 ? sp.burst12 : (fs.sprite == 1 ? sp.burst4 : sp.streak);
      // effective alpha: splat × fade × border through the ONE unit_mul
      const uint8_t a = unit_mul(unit8{unit_mul(unit8{fs.alpha}, unit8{fade})}, unit8{border});
      // glow-buffer coords: the plane is ¼ res of the view (§5)
      const int32_t gcx = (fs.cx_px - static_cast<int32_t>(vx0)) >> 2;
      const int32_t gcy = (fs.cy_px - static_cast<int32_t>(vy0)) >> 2;
      const int32_t r =
          post::flare_splat(glow, spr, gcx, gcy, fs.half_x_px >> 2, fs.half_y_px >> 2, a, budget);
      if (r < 0) {
        ++st->splats_dropped;
      } else {
        budget -= static_cast<uint32_t>(r);
        st->flare_texels += static_cast<uint32_t>(r);
      }
    }
    post::glow_composite(rgb888, w, h, vx0, vy0, vw, vh, glow, L.tint[0], L.tint[1], L.tint[2]);
  }
}

}  // namespace star
}  // namespace zref
