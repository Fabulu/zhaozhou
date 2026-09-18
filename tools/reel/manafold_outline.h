// Manafold cel-outline ownership helpers.
//
// The projected antenna loop encloses negative space. Viewport flood-fill must
// distinguish that opening from exterior background, then ink the CREATURE side
// of the enclosed boundary without filling the opening itself. These helpers are
// pure mask operations shared by production and the committed outline gate.

#ifndef ZHAO_REEL_MANAFOLD_OUTLINE_H
#define ZHAO_REEL_MANAFOLD_OUTLINE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace u02 {
namespace outline {

inline void classify_exterior(const std::vector<uint8_t>& mask,
                              uint32_t w, uint32_t h,
                              std::vector<uint8_t>& exterior) {
  const size_t n = static_cast<size_t>(w) * h;
  exterior.assign(n, 0);
  if (mask.size() != n || w == 0 || h == 0) return;

  std::vector<size_t> queue;
  queue.reserve(n);
  const auto admit = [&](uint32_t x, uint32_t y) {
    const size_t i = static_cast<size_t>(y) * w + x;
    if (mask[i] || exterior[i]) return;
    exterior[i] = 1;
    queue.push_back(i);
  };
  for (uint32_t x = 0; x < w; ++x) {
    admit(x, 0);
    if (h > 1) admit(x, h - 1);
  }
  for (uint32_t y = 1; y + 1 < h; ++y) {
    admit(0, y);
    if (w > 1) admit(w - 1, y);
  }
  for (size_t q = 0; q < queue.size(); ++q) {
    const size_t i = queue[q];
    const uint32_t x = static_cast<uint32_t>(i % w);
    const uint32_t y = static_cast<uint32_t>(i / w);
    if (x > 0) admit(x - 1, y);
    if (x + 1 < w) admit(x + 1, y);
    if (y > 0) admit(x, y - 1);
    if (y + 1 < h) admit(x, y + 1);
  }
}

inline bool has_cardinal_witness(const std::vector<uint8_t>& candidate_mask,
                                 const std::vector<uint8_t>& excluded,
                                 uint32_t w, uint32_t h,
                                 uint32_t x, uint32_t y, int width) {
  for (int r = 1; r <= width; ++r) {
    const int nx[4] = {static_cast<int>(x) - r,
                       static_cast<int>(x) + r,
                       static_cast<int>(x), static_cast<int>(x)};
    const int ny[4] = {static_cast<int>(y), static_cast<int>(y),
                       static_cast<int>(y) - r,
                       static_cast<int>(y) + r};
    for (int d = 0; d < 4; ++d) {
      if (nx[d] < 0 || ny[d] < 0 || nx[d] >= static_cast<int>(w) ||
          ny[d] >= static_cast<int>(h))
        continue;
      const size_t j = static_cast<size_t>(ny[d]) * w +
                       static_cast<uint32_t>(nx[d]);
      if (!candidate_mask[j] && !excluded[j]) return true;
    }
  }
  return false;
}

// Add ink on visible full-creature pixels bordering enclosed negative space.
// `mask == 0` pixels are never destinations, so the O remains open.
inline void add_enclosed_opening_edge(const std::vector<uint8_t>& mask,
                                      const std::vector<uint8_t>& exterior,
                                      uint32_t w, uint32_t h, int width,
                                      std::vector<uint8_t>& edge,
                                      std::vector<uint8_t>* owned = nullptr) {
  const size_t n = static_cast<size_t>(w) * h;
  if (mask.size() != n || exterior.size() != n || edge.size() != n ||
      width <= 0)
    return;
  if (owned != nullptr) owned->assign(n, 0);
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      if (!mask[i]) continue;
      if (!has_cardinal_witness(mask, exterior, w, h, x, y, width)) continue;
      edge[i] = 1;
      if (owned != nullptr) (*owned)[i] = 1;
    }
  }
}

// Preserve the accepted body/head line on the body side only where body depth
// still owns the fully composed creature pixel. A nearer antenna suppresses it.
inline void add_visible_body_inner_edge(
    const std::vector<uint8_t>& body_cover,
    const std::vector<int32_t>& body_depth,
    const int32_t* full_depth,
    const std::vector<uint8_t>& exterior,
    uint32_t w, uint32_t h, int width,
    std::vector<uint8_t>& edge,
    std::vector<uint8_t>* owned = nullptr) {
  const size_t n = static_cast<size_t>(w) * h;
  if (body_cover.size() != n || body_depth.size() != n ||
      full_depth == nullptr || exterior.size() != n || edge.size() != n ||
      width <= 0)
    return;
  if (owned != nullptr) owned->assign(n, 0);
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      if (!body_cover[i] || body_depth[i] != full_depth[i]) continue;
      if (!has_cardinal_witness(body_cover, exterior, w, h, x, y, width))
        continue;
      edge[i] = 1;
      if (owned != nullptr) (*owned)[i] = 1;
    }
  }
}

inline void paint_ink(uint8_t* rgb, size_t pixel_count,
                      const std::vector<uint8_t>& edge,
                      uint8_t r, uint8_t g, uint8_t b) {
  if (rgb == nullptr || edge.size() != pixel_count) return;
  for (size_t i = 0; i < pixel_count; ++i) {
    if (!edge[i]) continue;
    rgb[i * 3] = r;
    rgb[i * 3 + 1] = g;
    rgb[i * 3 + 2] = b;
  }
}

}  // namespace outline
}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_OUTLINE_H
