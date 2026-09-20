// gather_law_render.cpp -- owner ruling R37's before/after: the SAME frame,
// resolved, with and without the PROPOSED tag-to-gather law, at three settings
// of the one constant the picture is most sensitive to.
//
// R37: "This is an ART law. The packet proposes it from stars_and_flares.md §1
// with every coefficient in a named, editable constant (CLAUDE.md rule 6) and a
// zref model, and renders a before/after for the OWNER TO JUDGE BY EYE."
//
// EVERYTHING THAT DECIDES A PIXEL'S GLOW IS THE REFERENCE'S:
// `zref::post::gather::tag_to_fragment` (the proposed law),
// `zref::post::glow_accumulate` / `glow_pack565` (POST.GATHER's ratified
// accumulate-wide-pack-once law) and the compositor's own stage-4 add. This
// file only draws a scene, carries its tags, and walks those three.
//
// THE SCENE IS ART, authored by eye in the named constants below. It is built
// to show the three things the law can get wrong:
//   * a bright emitter blooms (the suns and the big stars);
//   * a LIT but not EMITTING surface does not (the creature silhouette and the
//     ground -- they carry no glow tag at all);
//   * a dim emitter below the knee does not (the nebula band, tagged GLOW at
//     strength 18, under the proposed knee of 24). Drop the knee and it
//     starts to haze, which is exactly the judgement being asked for.
//
// Output: raw PPM frames into the directory given as argv[1]. The PNG contact
// sheet is assembled by tools/post/gather_law_sheet.py, and the PNG is the
// evidence and the only artefact kept (CLAUDE.md: "the evidence is the PNG
// contact sheet, not the frames it was made from").
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "zref/zref_post.hpp"

namespace zg = zref::post::gather;

namespace {

// ---- the frame ------------------------------------------------------------
constexpr int kW = 384;          // Z60
constexpr int kH = 240;
constexpr int kCell = 4;         // POST.GATHER: 4 x 4 pixels per effect cell
constexpr int kCW = kW / kCell;  // 96 x 60, the Z60 glow plane
constexpr int kCH = kH / kCell;

// ---- the scene, authored by eye (ART, every value a knob) -----------------
struct Emitter {
  int x, y, radius;      // pixels
  uint16_t rgb565;       // its colour AT RESOLVE -- the glow borrows it
  uint8_t strength;      // the tag's 0..63 intensity, stars_and_flares.md §1
};
// Two suns, a scatter of stars across the strength range, and one dim band.
constexpr Emitter kEmitters[] = {
    {296, 54, 13, 0xFFE8, 63},   // the near sun: white-warm, full strength
    {104, 40, 7, 0x9DFF, 52},    // a blue giant
    {54, 96, 3, 0xFFFF, 44},     // white dwarf
    {180, 30, 2, 0xFD20, 38},    // orange, mid strength
    {236, 118, 2, 0xF800, 31},   // red, just above the knee
    {330, 150, 2, 0x07FF, 27},   // cyan, barely above the knee
    {148, 150, 2, 0xFFE0, 20},   // yellow, BELOW the proposed knee
    {70, 186, 2, 0x001F, 14},    // blue, well below: must stay a dot
};
constexpr uint16_t kSkyTop = 0x0002;        // near-black, a hint of blue
constexpr uint16_t kSkyBottom = 0x1907;     // a dusty horizon
constexpr uint16_t kNebulaRgb = 0x30A6;     // the dim band's colour
constexpr uint8_t kNebulaStrength = 18;     // tagged GLOW, below the knee
constexpr int kNebulaTop = 128;
constexpr int kNebulaBottom = 168;
constexpr uint16_t kGroundRgb = 0x2124;     // lit, NOT emitting: no tag
constexpr int kGroundTop = 196;
constexpr uint16_t kCreatureRgb = 0x6B4D;   // a lit body, no tag
constexpr uint16_t kCreatureEyeRgb = 0xFFF0;
constexpr uint8_t kCreatureEyeStrength = 58;  // the eye IS a light

// The separable quarter-res blur POST.GATHER's contract calls optional
// (Part A). 1-2-1, twice, over the CELL plane -- named so it is a knob.
constexpr int kBlurTaps = 3;
constexpr int kBlurW[kBlurTaps] = {1, 2, 1};
int g_blur_passes = 2;          // -b N on the command line, for the look pass

// ---- the resolved frame ---------------------------------------------------
struct Resolved {
  std::vector<uint16_t> rgb;   // RGB565, what the raster wrote
  std::vector<uint8_t> tag;    // the effect tag, never dithered (§1)
};

uint16_t lerp565(uint16_t a, uint16_t b, int num, int den) {
  const int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  const int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  const int r = ar + (br - ar) * num / den;
  const int g = ag + (bg - ag) * num / den;
  const int bl = ab + (bb - ab) * num / den;
  return uint16_t((r << 11) | (g << 5) | bl);
}

Resolved build_scene() {
  Resolved f;
  f.rgb.assign(size_t(kW) * kH, 0);
  f.tag.assign(size_t(kW) * kH, 0);
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) f.rgb[size_t(y) * kW + x] = lerp565(kSkyTop, kSkyBottom, y, kH - 1);

  // the dim band: a real emitter, deliberately under the knee
  for (int y = kNebulaTop; y < kNebulaBottom; ++y) {
    const int fade = (y - kNebulaTop) * 255 / (kNebulaBottom - kNebulaTop);
    for (int x = 0; x < kW; ++x) {
      const int wob = 12 * ((x * 7 + y * 13) % 5) / 4;
      if (((x + wob) % 97) < 70) {
        f.rgb[size_t(y) * kW + x] = lerp565(kNebulaRgb, kSkyBottom, fade, 255);
        f.tag[size_t(y) * kW + x] = uint8_t((zg::kChannelGlow << 6) | kNebulaStrength);
      }
    }
  }

  // the ground: lit, not emitting
  for (int y = kGroundTop; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      f.rgb[size_t(y) * kW + x] = lerp565(kGroundRgb, kSkyBottom, kH - 1 - y, kH - kGroundTop);
      f.tag[size_t(y) * kW + x] = 0;
    }

  // a creature silhouette against the horizon: a body with one lit eye
  const int cx = 196, cy = 196;
  for (int y = -34; y <= 0; ++y)
    for (int x = -26; x <= 26; ++x) {
      const int ry = y + 18;
      if (x * x * 4 + ry * ry * 9 > 36 * 36) continue;
      const int px = cx + x, py = cy + y;
      if (px < 0 || px >= kW || py < 0 || py >= kH) continue;
      f.rgb[size_t(py) * kW + px] = kCreatureRgb;
      f.tag[size_t(py) * kW + px] = 0;
    }
  for (int y = -2; y <= 1; ++y)
    for (int x = -2; x <= 1; ++x) {
      const int px = cx + 9 + x, py = cy - 22 + y;
      f.rgb[size_t(py) * kW + px] = kCreatureEyeRgb;
      f.tag[size_t(py) * kW + px] = uint8_t((zg::kChannelGlow << 6) | kCreatureEyeStrength);
    }

  // the emitters
  for (const Emitter& e : kEmitters) {
    for (int y = -e.radius; y <= e.radius; ++y)
      for (int x = -e.radius; x <= e.radius; ++x) {
        if (x * x + y * y > e.radius * e.radius) continue;
        const int px = e.x + x, py = e.y + y;
        if (px < 0 || px >= kW || py < 0 || py >= kH) continue;
        f.rgb[size_t(py) * kW + px] = e.rgb565;
        f.tag[size_t(py) * kW + px] = uint8_t((zg::kChannelGlow << 6) | e.strength);
      }
  }
  return f;
}

// ---- the gather: the proposed law, then POST.GATHER's own accumulate -------
struct Plane {
  std::vector<uint16_t> cell;   // RGB565 per cell, packed ONCE at flush
  unsigned saturations = 0, reserved = 0, contributing = 0;
};

Plane gather_plane(const Resolved& f, uint8_t knee, uint8_t slope) {
  std::vector<uint16_t> acc_r(size_t(kCW) * kCH, 0), acc_g(acc_r.size(), 0), acc_b(acc_r.size(), 0);
  Plane p;
  p.cell.assign(size_t(kCW) * kCH, 0);
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      const uint8_t tag = f.tag[size_t(y) * kW + x];
      if (tag == 0) continue;
      // The law, with the two constants this render is varying. Everything
      // else -- the colour borrow, the tint, the master -- is the reference's.
      zref::post::gather::Fragment g;
      {
        const unsigned ch = zg::tag_channel(tag);
        if (ch == zg::kChannelGlow) {
          const unsigned s = zg::tag_strength(tag);
          uint8_t gain = 0;
          if (s > knee) {
            const uint32_t v = ((s - knee) * uint32_t(slope)) >> 4;
            gain = uint8_t(v > 255u ? 255u : v);
          }
          gain = zg::unit_mul(gain, zg::kGlowMaster);
          const uint16_t c = f.rgb[size_t(y) * kW + x];
          const uint8_t r = zg::exp5(uint8_t((c >> 11) & 0x1Fu));
          const uint8_t gg = zg::exp6(uint8_t((c >> 5) & 0x3Fu));
          const uint8_t b = zg::exp5(uint8_t(c & 0x1Fu));
          g.glow_r = zg::unit_mul(zg::unit_mul(r, gain), zg::kGlowTint[0]);
          g.glow_g = zg::unit_mul(zg::unit_mul(gg, gain), zg::kGlowTint[1]);
          g.glow_b = zg::unit_mul(zg::unit_mul(b, gain), zg::kGlowTint[2]);
        } else if (ch != zg::kChannelNone) {
          g.reserved_channel = true;
        }
      }
      if (g.reserved_channel) ++p.reserved;
      if ((g.glow_r | g.glow_g | g.glow_b) == 0) continue;
      const size_t c = size_t(y / kCell) * kCW + (x / kCell);
      acc_r[c] = zref::post::glow_accumulate(acc_r[c], g.glow_r);
      acc_g[c] = zref::post::glow_accumulate(acc_g[c], g.glow_g);
      acc_b[c] = zref::post::glow_accumulate(acc_b[c], g.glow_b);
    }
  for (size_t c = 0; c < p.cell.size(); ++c) {
    if (acc_r[c] > 255 || acc_g[c] > 255 || acc_b[c] > 255) ++p.saturations;
    if (acc_r[c] | acc_g[c] | acc_b[c]) ++p.contributing;
    p.cell[c] = zref::post::glow_pack565(acc_r[c], acc_g[c], acc_b[c]);
  }
  return p;
}

Plane blur_plane(const Plane& in) {
  Plane out = in;
  std::vector<int> r(size_t(kCW) * kCH), g(r.size()), b(r.size());
  for (size_t i = 0; i < r.size(); ++i) {
    r[i] = zg::exp5(uint8_t((in.cell[i] >> 11) & 31));
    g[i] = zg::exp6(uint8_t((in.cell[i] >> 5) & 63));
    b[i] = zg::exp5(uint8_t(in.cell[i] & 31));
  }
  for (int pass = 0; pass < g_blur_passes; ++pass) {
    for (int axis = 0; axis < 2; ++axis) {
      std::vector<int> nr = r, ng = g, nb = b;
      for (int y = 0; y < kCH; ++y)
        for (int x = 0; x < kCW; ++x) {
          int sr = 0, sg = 0, sb = 0, sw = 0;
          for (int t = 0; t < kBlurTaps; ++t) {
            const int d = t - kBlurTaps / 2;
            const int sx = axis == 0 ? x + d : x;
            const int sy = axis == 0 ? y : y + d;
            if (sx < 0 || sx >= kCW || sy < 0 || sy >= kCH) continue;
            const size_t s = size_t(sy) * kCW + sx;
            sr += r[s] * kBlurW[t];
            sg += g[s] * kBlurW[t];
            sb += b[s] * kBlurW[t];
            sw += kBlurW[t];
          }
          const size_t o = size_t(y) * kCW + x;
          nr[o] = sw ? sr / sw : 0;
          ng[o] = sw ? sg / sw : 0;
          nb[o] = sw ? sb / sw : 0;
        }
      r = nr;
      g = ng;
      b = nb;
    }
  }
  for (size_t i = 0; i < r.size(); ++i)
    out.cell[i] = zref::post::glow_pack565(uint16_t(r[i]), uint16_t(g[i]), uint16_t(b[i]));
  return out;
}

// ---- the compositor's stage 4, and only that ------------------------------
uint8_t sat_add8(uint8_t a, uint8_t b) {
  const unsigned s = unsigned(a) + b;
  return uint8_t(s > 255u ? 255u : s);
}

void compose(const Resolved& f, const Plane* p, uint8_t bloom_gain, std::vector<uint8_t>& out) {
  out.assign(size_t(kW) * kH * 3, 0);
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      const uint16_t c = f.rgb[size_t(y) * kW + x];
      uint8_t r = zg::exp5(uint8_t((c >> 11) & 31));
      uint8_t g = zg::exp6(uint8_t((c >> 5) & 63));
      uint8_t b = zg::exp5(uint8_t(c & 31));
      if (p != nullptr) {
        const uint16_t gc = p->cell[size_t(y / kCell) * kCW + (x / kCell)];
        r = sat_add8(r, zg::unit_mul(zg::exp5(uint8_t((gc >> 11) & 31)), bloom_gain));
        g = sat_add8(g, zg::unit_mul(zg::exp6(uint8_t((gc >> 5) & 63)), bloom_gain));
        b = sat_add8(b, zg::unit_mul(zg::exp5(uint8_t(gc & 31)), bloom_gain));
      }
      const size_t o = (size_t(y) * kW + x) * 3;
      out[o + 0] = r;
      out[o + 1] = g;
      out[o + 2] = b;
    }
}

void write_ppm(const std::string& path, const std::vector<uint8_t>& rgb) {
  std::FILE* fp = std::fopen(path.c_str(), "wb");
  if (fp == nullptr) {
    std::printf("gather_law_render: cannot write %s\n", path.c_str());
    std::exit(2);
  }
  std::fprintf(fp, "P6\n%d %d\n255\n", kW, kH);
  std::fwrite(rgb.data(), 1, rgb.size(), fp);
  std::fclose(fp);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: gather_law_render <out-dir> [knee ...]\n");
    return 2;
  }
  const std::string dir = argv[1];
  std::vector<uint8_t> knees;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      g_blur_passes = std::atoi(argv[++i]);
      continue;
    }
    knees.push_back(uint8_t(std::atoi(argv[i])));
  }
  if (knees.empty()) knees = {16, zg::kGlowKnee, 32};

  const Resolved f = build_scene();
  std::vector<uint8_t> img;

  compose(f, nullptr, 0, img);
  write_ppm(dir + "/before.ppm", img);
  std::printf("before: the resolved frame, no gather\n");

  for (uint8_t knee : knees) {
    const Plane raw = gather_plane(f, knee, zg::kGlowSlope);
    const Plane blurred = blur_plane(raw);
    for (uint8_t gain : {uint8_t(128), uint8_t(255)}) {
      compose(f, &blurred, gain, img);
      // NOT a fixed buffer: an output directory longer than it silently
      // TRUNCATES the name, and every frame then overwrites one file called
      // "after_". Measured, on a 110-character scratch path.
      char suffix[64];
      std::snprintf(suffix, sizeof(suffix), "/after_k%02u_g%03u.ppm", unsigned(knee), unsigned(gain));
      write_ppm(dir + suffix, img);
    }
    std::printf("knee %2u slope 0x%02X: %u of %d cells contributing, %u saturated, %u reserved-channel fragments\n",
                unsigned(knee), unsigned(zg::kGlowSlope), raw.contributing, kCW * kCH,
                raw.saturations, raw.reserved);
  }
  std::printf("proposed default: knee %u, slope 0x%02X, tint %u/%u/%u, master %u\n",
              unsigned(zg::kGlowKnee), unsigned(zg::kGlowSlope), unsigned(zg::kGlowTint[0]),
              unsigned(zg::kGlowTint[1]), unsigned(zg::kGlowTint[2]), unsigned(zg::kGlowMaster));
  return 0;
}
