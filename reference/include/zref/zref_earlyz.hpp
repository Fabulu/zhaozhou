// zref_earlyz.hpp — RASTER.EARLYZ reference model (phase 5, ZH-059).
//
// The scalar oracle named by design/blocks.yml (`reference_model:
// zref::EarlyZ`) and design/contracts/RASTER.EARLYZ.md.
//
// Like zref::TileStore this is a second implementation of a CONTRACT — no
// earlier code here maintains a hierarchical-Z floor — so it is written in
// the plainest way that can be checked by eye: one bound, one 256-entry
// bitset, one running minimum. The RTL's registered output stage, its
// backpressure and its saturating counters have no counterpart in it, which
// is exactly what makes "RTL == oracle" test those mechanics.
//
// It shares exactly one thing with the RTL: the fragment state word's bit
// positions, which are zref::FragmentPipeline::State's (and, upstream of
// that, fpga/rtl/raster/zhao_raster_fragment.sv's). This file decodes that
// word through State::unpack rather than re-deriving any bit position.
//
// THE INVARIANT, restated so a reader can check the code against it:
// `floor()` is a LOWER BOUND on the stored depth of every one of the tile's
// 256 pixels. Under spec/qformats.md 8's strict test (`pass <=> d_new >
// d_old`), a fragment with `depth <= floor()` therefore loses at every pixel,
// so rejecting it is exactly what the late test would have done. The model
// may be pessimistic; it may never be optimistic. See the RTL header for the
// three-step argument that the accumulator preserves the invariant.
//
// CYCLE MODEL. `fragment()` is one accepted fragment and returns the decision
// the RTL presents one cycle later (its `latency: fixed:1`). `tile_begin()`
// is applied AFTER the fragment of the same cycle, mirroring the RTL's
// ordering — the tests drive them in that order.

#pragma once

#include <cstdint>

namespace zref {

/** Conservative early-Z rejection plus the coarse transparent-depth bins. */
struct EarlyZ {
  /** One 16x16 tile's worth of pixels (charter 8). */
  static constexpr int kWords = 256;
  /** spec/qformats.md 8: depth is invw24, clear value 0, larger is closer. */
  static constexpr uint32_t kDepthMax = 0xFFFFFFu;

  /** What the block decides about one fragment. */
  struct Out {
    bool keep = false;  // it survived: a `shaded_candidates` beat follows
    uint8_t bin = 0;    // the coarse depth bin, depth[23:21]; 7 is nearest
  };

  EarlyZ() { reset(); }

  /** Power-on / reset state: floor 0, nothing accumulated, counters zero. */
  void reset();

  /**
   * Begin a tile whose clear depth is `clear_depth`. Every pixel holds that
   * value, so the floor is exactly it; the accumulator and the bin mask start
   * empty. Applied after the same cycle's fragment, as the RTL does.
   */
  void tile_begin(uint32_t clear_depth);

  /** One accepted fragment. Updates the counters, the bins and the floor. */
  Out fragment(uint8_t addr, uint32_t depth, uint32_t state);

  uint32_t floor() const { return floor_; }
  uint8_t bin_mask() const { return bin_mask_; }
  uint32_t early_z_rejects() const { return rejects_; }
  uint32_t covered_fragments() const { return covered_; }

 private:
  uint32_t floor_ = 0;
  bool acc_mask_[kWords] = {};
  uint32_t acc_count_ = 0;  // popcount of acc_mask_, so "all covered" is O(1)
  uint32_t acc_min_ = kDepthMax;
  uint8_t bin_mask_ = 0;
  uint32_t rejects_ = 0;
  uint32_t covered_ = 0;
};

}  // namespace zref
