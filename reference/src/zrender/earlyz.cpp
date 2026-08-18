// earlyz.cpp — zref::EarlyZ, the RASTER.EARLYZ oracle.
// Law and rationale: reference/include/zref/zref_earlyz.hpp.

#include "zref/zref_earlyz.hpp"

#include <cstdint>

#include "zref/zref_fragment.hpp"

namespace zref {

void EarlyZ::reset() {
  floor_ = 0;
  for (int i = 0; i < kWords; ++i) acc_mask_[i] = false;
  acc_count_ = 0;
  acc_min_ = kDepthMax;
  bin_mask_ = 0;
  rejects_ = 0;
  covered_ = 0;
}

void EarlyZ::tile_begin(uint32_t clear_depth) {
  floor_ = clear_depth & 0xFFFFFFu;
  for (int i = 0; i < kWords; ++i) acc_mask_[i] = false;
  acc_count_ = 0;
  acc_min_ = kDepthMax;
  bin_mask_ = 0;
}

EarlyZ::Out EarlyZ::fragment(uint8_t addr, uint32_t depth, uint32_t state) {
  const FragmentPipeline::State st = FragmentPipeline::State::unpack(state);
  const uint32_t d = depth & 0xFFFFFFu;

  ++covered_;

  // THE DECISION. `<=` and not `<`, because spec/qformats.md 8's late test is
  // strict (`d_new > d_old`, ties fail): a fragment exactly at the floor loses
  // at every pixel too. With the depth test off nothing is ever rejected.
  const bool reject = st.z_test_en && (d <= floor_);

  Out out;
  if (reject) {
    ++rejects_;
  } else {
    out.keep = true;
    out.bin = static_cast<uint8_t>((d >> 21) & 7u);
    bin_mask_ = static_cast<uint8_t>(bin_mask_ | (1u << out.bin));
  }

  // THE ACCUMULATOR. Deliberately narrow: only a fragment that is certain to
  // write depth counts as evidence. Anything that could still be killed
  // downstream (a masked star disc, a stencilled decal) or that writes no
  // depth at all (every additive recipe: "Z-write OFF") contributes nothing.
  const bool qualify = !reject && (st.blend == FragmentPipeline::kReplace) && !st.z_write_dis &&
                       !st.atest_en && (st.sten_func == FragmentPipeline::kAlways);
  if (qualify) {
    // z_force_far writes the far constant, so the evidence is that value and
    // not the fragment's interpolated depth.
    const uint32_t written = st.z_force_far ? FragmentPipeline::kDepthFar : d;
    if (!acc_mask_[addr]) {
      acc_mask_[addr] = true;
      ++acc_count_;
    }
    if (written < acc_min_) acc_min_ = written;

    if (acc_count_ == static_cast<uint32_t>(kWords)) {
      // Every pixel has taken a depth write of at least acc_min_, so the
      // floor may rise to it. It never moves backwards.
      if (acc_min_ > floor_) floor_ = acc_min_;
      for (int i = 0; i < kWords; ++i) acc_mask_[i] = false;
      acc_count_ = 0;
      acc_min_ = kDepthMax;
    }
  }

  return out;
}

}  // namespace zref
