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
// nearest from a 128-texel sprite through a 64-entry palette
void draw_clut_quad(Plane& p, const Sprite8& spr, const uint8_t pal[64][3], int32_t cx, int32_t cy,
                    int32_t r, bool additive, uint32_t* frags) {
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
      const size_t idx = static_cast<size_t>(y) * p.w + x;
      if (!(kStarDepth > p.depth[idx])) continue;  // Z-test on (§8 strict)
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * spr.w) / wq);
      const uint8_t t = spr.pix[static_cast<size_t>(sy) * spr.w + sx];
      if (t == 0) continue;  // alpha-test index 0 / additive identity
      uint8_t* dst = &p.rgb[idx * 3];
      if (additive) {
        dst[0] = sat_add_u8(dst[0], pal[t][0]);
        dst[1] = sat_add_u8(dst[1], pal[t][1]);
        dst[2] = sat_add_u8(dst[2], pal[t][2]);
      } else {
        dst[0] = pal[t][0];
        dst[1] = pal[t][1];
        dst[2] = pal[t][2];
      }
      p.tag[idx] = glow_tag(t);  // strength = the texel's CLUT intensity (§1)
      if (frags) ++(*frags);
    }
  }
}

}  // namespace

void compose_view(uint8_t* rgb888, int32_t* depth, uint32_t w, uint32_t h, uint32_t vx0,
                  uint32_t vy0, uint32_t vw, uint32_t vh, uint32_t tick,
                  const ComposeLight* lights, int n_lights, const GlintPoint* glints, int n_glints,
                  FlareSlots& slots, ComposeStats* stats) {
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

  // ---- pass 6: per light, halo BEFORE disc (§4 "halo submitted before") --
  uint8_t pal_d[64][3];
  uint8_t pal_h[64][3];
  for (int i = 0; i < n_lights && i < 4; ++i) {
    const ComposeLight& L = lights[i];
    if (L.ramp == nullptr) continue;
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
    const int32_t ex = (L.x_px - p.x0) < (p.x1 - 1 - L.x_px) ? (L.x_px - p.x0)
                                                             : (p.x1 - 1 - L.x_px);
    const int32_t ey = (L.y_px - p.y0) < (p.y1 - 1 - L.y_px) ? (L.y_px - p.y0)
                                                             : (p.y1 - 1 - L.y_px);
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
      const uint8_t a =
          unit_mul(unit8{unit_mul(unit8{fs.alpha}, unit8{fade})}, unit8{border});
      // glow-buffer coords: the plane is ¼ res of the view (§5)
      const int32_t gcx = (fs.cx_px - static_cast<int32_t>(vx0)) >> 2;
      const int32_t gcy = (fs.cy_px - static_cast<int32_t>(vy0)) >> 2;
      const int32_t r = post::flare_splat(glow, spr, gcx, gcy, fs.half_x_px >> 2,
                                          fs.half_y_px >> 2, a, budget);
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
