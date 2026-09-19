// manafold_motiongate.cpp -- Direction 18 production FX continuity gate.
//
// It evaluates the same pose decoder, anchor extractor and FX evaluators used by
// the reel at every 60 Hz key/midpoint. Stable entity IDs are explicit: strand
// point, fold station and mote ID. Mutants restore the shipped snap/blackout/
// reseed mechanisms and must turn this gate red for their named reason.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

int g_failures = 0;
uint64_t g_failure_categories = 0;

enum FailureCategory : uint64_t {
  kFailWeight = 1ull << 0,
  kFailPhase = 1ull << 1,
  kFailMoteMotion = 1ull << 2,
  kFailIdentity = 1ull << 3,
  kFailBlackout = 1ull << 4,
  kFailLabMotion = 1ull << 5,
  kFailLabEnergy = 1ull << 6,
  kFailManaBody = 1ull << 7,
  kFailShapeChain = 1ull << 8,
  kFailCoverage = 1ull << 9,
  kFailSeam = 1ull << 10,
  kFailFreeMotion = 1ull << 11,
  kFailFoldMotion = 1ull << 12,
  kFailFoldMoteMotion = 1ull << 13,
  kFailSurgeMotion = 1ull << 14,
  kFailEdgePresence = 1ull << 15,
  kFailEdgeBrightness = 1ull << 16,
  kFailStampCount = 1ull << 17,
  kFailAccumEnergy = 1ull << 18,
  kFailMoteVisibility = 1ull << 19,
  kFailAttribution = 1ull << 20,
  kFailPalette = 1ull << 21,
};

// Broad structural bands, authored after native every-frame and exact 4x review
// of the actual maxima (Lasso f86..91 and Trick f331..336). Those sequences read
// as continuous motion; these ceilings retain substantial headroom while still
// rejecting replacement/snap mutants. They compare the art; they did not choose
// its values.
constexpr double kFreeStepMaxMm = 250.0;
constexpr double kFreeAccelMaxMm = 250.0;
constexpr double kFreeJerkMaxMm = 300.0;
constexpr double kFoldStepMaxMm = 500.0;
constexpr double kFoldAccelMaxMm = 600.0;
constexpr double kFoldJerkMaxMm = 1200.0;
constexpr double kFoldMoteStepMaxMm = 420.0;
constexpr double kFoldMoteAccelMaxMm = 300.0;
constexpr double kFoldMoteJerkMaxMm = 350.0;
constexpr double kSurgeStepMaxMm = 420.0;
constexpr double kSurgeAccelMaxMm = 300.0;
constexpr double kSurgeJerkMaxMm = 350.0;
constexpr double kSeamStepMaxMm = 500.0;
// Native seam/badness review accepts the current C2 energy handoffs (max
// 166/82/85 pm) and rejects the old hundreds-of-pm one-frame branch reset.
constexpr double kEdgePresenceStepMaxPm = 300.0;
constexpr double kEdgePresenceAccelMaxPm = 300.0;
constexpr double kEdgePresenceJerkMaxPm = 500.0;
constexpr double kEdgeGainStepMaxPm = 300.0;
constexpr double kEdgeGainAccelMaxPm = 300.0;
constexpr double kEdgeGainJerkMaxPm = 500.0;
// Fold and surge mote visibility is a rendered operand, independent of stable
// IDs and positions. These C2-envelope bands retain the same reviewed headroom
// as edge presence while rejecting visibility-only off/reappear faults.
constexpr double kMoteVisibilityStepMaxPm = 300.0;
constexpr double kMoteVisibilityAccelMaxPm = 300.0;
constexpr double kMoteVisibilityJerkMaxPm = 500.0;
// Discrete production population and radius-weighted accumulated energy. One
// complete edge may enter across a C2 topology handoff; larger impulses are a
// replacement/pop fault, not smooth morphing.
constexpr double kStampCountStepMax = 32.0;  // one 24-stamp edge + 1/3 headroom
constexpr double kStampCountAccelMax = 48.0;
constexpr double kStampCountJerkMax = 72.0;
constexpr double kAccumEnergyStepMaxPm = 300.0;
constexpr double kAccumEnergyAccelMaxPm = 300.0;
constexpr double kAccumEnergyJerkMaxPm = 500.0;
// Lab edge energy is a smaller two-layer domain; its reviewed normal maxima are
// 4/4/8 pm, so these structural bands keep wide headroom while making the
// population-only control observable there too.
constexpr double kLabAccumEnergyStepMaxPm = 100.0;
constexpr double kLabAccumEnergyAccelMaxPm = 200.0;
constexpr double kLabAccumEnergyJerkMaxPm = 300.0;
// The caged pulsar's authored 4 Hz sine changes radius by at most 7 px/frame;
// these bands preserve that smooth pulse while rejecting a phase reset.
constexpr double kManaBodyStepMaxMm = 80.0;
constexpr double kManaBodyAccelMaxMm = 80.0;
constexpr double kManaBodyJerkMaxMm = 120.0;
constexpr double kManaBodyRadiusStepMaxPx = 12.0;
constexpr double kManaBodyRadiusAccelMaxPx = 8.0;
constexpr double kManaBodyRadiusJerkMaxPx = 10.0;
// Native full-loop and badness review selected the one-cycle C2 churn (entry
// 72/23/15 RGB units; emitted bloom energy 46/16/13 pm). These ceilings retain
// real headroom, reject the faster repeated-flicker rungs, and sit far below the
// raw-clock/hard-switch controls (entry 369/369/739; energy up to 1073 pm).
// Measurement compares the selected art; it did not generate its colours.
constexpr double kPaletteEntryStepMax = 100.0;
constexpr double kPaletteEntryAccelMax = 60.0;
constexpr double kPaletteEntryJerkMax = 60.0;
constexpr double kPaletteEnergyStepMax = 80.0;
constexpr double kPaletteEnergyAccelMax = 60.0;
constexpr double kPaletteEnergyJerkMax = 60.0;

void fail(uint64_t category, const char* what) {
  std::printf("  FAIL: %s\n", what);
  ++g_failures;
  g_failure_categories |= category;
}

struct V3 {
  double x = 0.0, y = 0.0, z = 0.0;
};

V3 mm_rel(const int32_t p[3], const int32_t body[3]) {
  constexpr double k = 1000.0 / 65536.0;
  return V3{(p[0] - body[0]) * k, (p[1] - body[1]) * k,
            (p[2] - body[2]) * k};
}

V3 sub(const V3& a, const V3& b) {
  return V3{a.x - b.x, a.y - b.y, a.z - b.z};
}

double mag(const V3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

struct EntityHistory {
  bool have_p = false, have_v = false, have_a = false;
  V3 p{}, v{}, a{};
  double max_step = 0.0, max_accel = 0.0, max_jerk = 0.0;
  int max_step_frame = -1, max_accel_frame = -1, max_jerk_frame = -1;
  void push(const V3& n, int frame) {
    if (have_p) {
      const V3 nv = sub(n, p);
      const double step = mag(nv);
      if (step > max_step) {
        max_step = step;
        max_step_frame = frame;
      }
      if (have_v) {
        const V3 na = sub(nv, v);
        const double accel = mag(na);
        if (accel > max_accel) {
          max_accel = accel;
          max_accel_frame = frame;
        }
        if (have_a) {
          const double jerk = mag(sub(na, a));
          if (jerk > max_jerk) {
            max_jerk = jerk;
            max_jerk_frame = frame;
          }
        }
        a = na;
        have_a = true;
      }
      v = nv;
      have_v = true;
    }
    p = n;
    have_p = true;
  }
};

struct ScalarHistory {
  bool have_p = false, have_v = false, have_a = false;
  double p = 0.0, v = 0.0, a = 0.0;
  double max_step = 0.0, max_accel = 0.0, max_jerk = 0.0;
  int max_step_frame = -1, max_accel_frame = -1, max_jerk_frame = -1;
  void push(double n, int frame) {
    if (have_p) {
      const double nv = n - p;
      const double step = std::abs(nv);
      if (step > max_step) { max_step = step; max_step_frame = frame; }
      if (have_v) {
        const double na = nv - v;
        const double accel = std::abs(na);
        if (accel > max_accel) { max_accel = accel; max_accel_frame = frame; }
        if (have_a) {
          const double jerk = std::abs(na - a);
          if (jerk > max_jerk) { max_jerk = jerk; max_jerk_frame = frame; }
        }
        a = na;
        have_a = true;
      }
      v = nv;
      have_v = true;
    }
    p = n;
    have_p = true;
  }
};

struct ClipMetrics {
  double free_step = 0.0, free_accel = 0.0, free_jerk = 0.0;
  double fold_step = 0.0, fold_accel = 0.0, fold_jerk = 0.0;
  double mote_step = 0.0, mote_accel = 0.0, mote_jerk = 0.0;
  double surge_step = 0.0, surge_accel = 0.0, surge_jerk = 0.0;
  double mote_visibility_step = 0.0, mote_visibility_accel = 0.0,
         mote_visibility_jerk = 0.0;
  double surge_visibility_step = 0.0, surge_visibility_accel = 0.0,
         surge_visibility_jerk = 0.0;
  double edge_presence_step = 0.0, edge_presence_accel = 0.0,
         edge_presence_jerk = 0.0;
  double edge_energy_step = 0.0, edge_energy_accel = 0.0,
         edge_energy_jerk = 0.0;
  double edge_stamp_step = 0.0, edge_stamp_accel = 0.0,
         edge_stamp_jerk = 0.0;
  double edge_accum_step = 0.0, edge_accum_accel = 0.0,
         edge_accum_jerk = 0.0;
  double free_stamp_step = 0.0, free_stamp_accel = 0.0,
         free_stamp_jerk = 0.0;
  double free_energy_step = 0.0, free_energy_accel = 0.0,
         free_energy_jerk = 0.0;
  int edge_presence_step_frame = -1, edge_presence_step_id = -1;
  int edge_presence_accel_frame = -1, edge_presence_accel_id = -1;
  int edge_presence_jerk_frame = -1, edge_presence_jerk_id = -1;
  int edge_energy_step_frame = -1, edge_energy_step_id = -1;
  int edge_energy_accel_frame = -1, edge_energy_accel_id = -1;
  int edge_energy_jerk_frame = -1, edge_energy_jerk_id = -1;
  int free_step_frame = -1, free_step_id = -1;
  int free_accel_frame = -1, free_accel_id = -1;
  int free_jerk_frame = -1, free_jerk_id = -1;
  int fold_step_frame = -1, fold_step_id = -1;
  int fold_accel_frame = -1, fold_accel_id = -1;
  int fold_jerk_frame = -1, fold_jerk_id = -1;
  int mote_step_frame = -1, mote_step_id = -1;
  int mote_accel_frame = -1, mote_accel_id = -1;
  int mote_jerk_frame = -1, mote_jerk_id = -1;
  int surge_step_frame = -1, surge_step_id = -1;
  int surge_accel_frame = -1, surge_accel_id = -1;
  int surge_jerk_frame = -1, surge_jerk_id = -1;
  u02::FoldPhase fold_step_phase{};
  u02::FoldPhase fold_step_prior_phase{};
  int32_t fold_step_lasso_pm = 0;
  int32_t fold_step_life_pm = 0;
  int32_t fold_step_agitation_pm = 0;
  int32_t fold_step_knead_pm = 0;
  int edge_blackouts = 0;
  int role_changes = 0;
  int mote_count_changes = 0;
  int surge_role_changes = 0;
  int surge_count_changes = 0;
  int seam_samples = 0;
  int held_tail_samples = 0;
  double seam_step = 0.0;
  int shape_chain_breaks = 0;
  int morph_reversals = 0;
};

ClipMetrics trace_clip(const zc::CreatureType& type, const zc::Clip& clip) {
  ClipMetrics out;
  std::array<EntityHistory, u02::kFxTraceMaxStrands * (u02::kBoltSegs + 1)> free_h{};
  std::array<EntityHistory, u02::kStencilPts> fold_h{};
  std::array<EntityHistory, u02::kMoteCount> mote_h{};
  std::array<EntityHistory, u02::kSurgeMotes> surge_h{};
  std::array<ScalarHistory, u02::kMoteCount> mote_visibility_h{};
  std::array<ScalarHistory, u02::kSurgeMotes> surge_visibility_h{};
  std::array<ScalarHistory, u02::kStencilPts> edge_presence_h{};
  std::array<std::array<ScalarHistory, 3>, u02::kStencilPts> edge_energy_h{};
  std::array<std::array<ScalarHistory, 3>, u02::kStencilPts> edge_stamp_h{};
  std::array<std::array<ScalarHistory, 3>, u02::kStencilPts> edge_accum_h{};
  std::array<std::array<ScalarHistory, 2>, u02::kFxTraceMaxStrands> free_stamp_h{};
  std::array<std::array<ScalarHistory, 2>, u02::kFxTraceMaxStrands> free_energy_h{};
  std::array<uint8_t, u02::kMoteCount> prior_role{};
  std::array<uint8_t, u02::kSurgeMotes> prior_surge_role{};
  prior_role.fill(0xFFu);
  prior_surge_role.fill(0xFFu);
  int prior_mote_count = -1;
  int prior_surge_count = -1;
  bool have_fold = false;
  u02::FoldPhase prior_fold{};
  int prior_morph = -1;
  int diagnostics = 0;
  u02::FoldState state{};
  std::vector<u02::ManaSplat> splats;

  const int frames = static_cast<int>(clip.frame_count) * 2;
  const bool loops = !clip.hold_last;
  // Loops replay frames 0..2 for cyclic derivatives. Held clips replay the
  // exact final presentation sample three times so stateful settling after the
  // animation parks is part of the same velocity/acceleration/jerk history.
  const int samples = frames + 3;
  for (int sample = 0; sample < samples; ++sample) {
    const int pf = sample >= frames
                       ? (loops ? sample - frames : frames - 1)
                       : sample;
    if (!loops && sample >= frames) ++out.held_tail_samples;
    const uint16_t key = static_cast<uint16_t>(pf / 2);
    const uint8_t subframe = static_cast<uint8_t>(pf & 1);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
    zc::decode_pose(type, clip, key, pose, nullptr, subframe);
    const u02::FxAnchors anchors = u02::fx_anchors_from_pose(type, pose);
    u02::FxContinuityTrace tr{};
    splats.clear();
    int32_t agit = 0;
    // Candidate 3 is the shipping folded figure. Evaluate the free strand too;
    // candidate stacks use this exact same production helper.
    u02::mana_fill(3, static_cast<uint32_t>(pf), clip.slot_id,
                   clip.frame_count, anchors, state, 1000, splats, &agit, &tr);
    u02::mana_lightning(static_cast<uint32_t>(pf), clip.slot_id,
                        clip.frame_count, anchors, splats, 1000, &tr);

    for (int strand = 0; strand < u02::kFxTraceMaxStrands; ++strand)
      for (int layer = 0; layer < 2; ++layer) {
        free_stamp_h[static_cast<size_t>(strand)][static_cast<size_t>(layer)].push(
            tr.free_strand_stamp_count[strand][layer], sample);
        free_energy_h[static_cast<size_t>(strand)][static_cast<size_t>(layer)].push(
            tr.free_strand_energy_pm[strand][layer], sample);
      }

    for (int i = 0; i < tr.free_point_count; ++i) {
      const V3 p = mm_rel(tr.free_point[i], anchors.body);
      EntityHistory& h = free_h[static_cast<size_t>(i)];
      if (loops && sample == frames && h.have_p) {
        out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
        ++out.seam_samples;
      }
      h.push(p, sample);
    }
    if (prior_surge_count >= 0 && tr.surge_mote_count != prior_surge_count)
      ++out.surge_count_changes;
    prior_surge_count = tr.surge_mote_count;
    for (int i = 0; i < u02::kSurgeMotes; ++i)
      surge_visibility_h[static_cast<size_t>(i)].push(
          i < tr.surge_mote_count ? tr.surge_mote_visibility_pm[i] : 0,
          sample);
    for (int i = 0; i < tr.surge_mote_count; ++i) {
      if (prior_surge_role[static_cast<size_t>(i)] != 0xFFu &&
          prior_surge_role[static_cast<size_t>(i)] != tr.surge_mote_role[i])
        ++out.surge_role_changes;
      prior_surge_role[static_cast<size_t>(i)] = tr.surge_mote_role[i];
      if (tr.surge_mote_visibility_pm[i] <= 0) continue;
      const V3 p = mm_rel(tr.surge_mote_position[i], anchors.body);
      EntityHistory& h = surge_h[static_cast<size_t>(i)];
      if (loops && sample == frames && h.have_p) {
        out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
        ++out.seam_samples;
      }
      h.push(p, sample);
    }
    if (tr.fold_active) {
      for (int i = 0; i < u02::kStencilPts; ++i) {
        const V3 p = mm_rel(tr.fold_station[i], anchors.body);
        EntityHistory& h = fold_h[static_cast<size_t>(i)];
        if (loops && sample == frames && h.have_p) {
          out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
          ++out.seam_samples;
        }
        if (h.have_p) {
          const double step = mag(sub(p, h.p));
          if (step > out.fold_step) {
            out.fold_step = step;
            out.fold_step_frame = pf;
            out.fold_step_id = i;
            out.fold_step_phase = tr.fold;
            out.fold_step_prior_phase = prior_fold;
            out.fold_step_lasso_pm = tr.lasso_mix_pm;
            out.fold_step_life_pm = tr.life_pm;
            out.fold_step_agitation_pm = tr.agitation_pm;
            out.fold_step_knead_pm = tr.knead_pm;
          }
          if (step > 500.0 && diagnostics < 6) {
            std::printf("     discontinuity slot %u f%d station%d %.2f mm "
                        "seg%d %u->%u morph%d turn%d lasso%d life%d agit%d knead%d\n",
                        clip.slot_id, sample, i, step,
                        static_cast<int>(tr.fold.seg), tr.fold.shape_from,
                        tr.fold.shape_to, tr.fold.morph_pm,
                        tr.fold.turn_a16, tr.lasso_mix_pm, tr.life_pm,
                        tr.agitation_pm, tr.knead_pm);
            ++diagnostics;
          }
        }
        h.push(p, sample);
      }
      for (int i = 0; i < u02::kStencilPts; ++i) {
        const ScalarHistory& presence_h = edge_presence_h[static_cast<size_t>(i)];
        if (presence_h.have_p &&
            std::abs(static_cast<double>(tr.edge_presence_pm[i]) - presence_h.p) > 300.0 &&
            diagnostics < 10) {
          std::printf("     energy jump slot %u f%d edge%d %.0f->%d "
                      "seg%d %u->%u morph%d amp%d coh%d lit%d life%d\n",
                      clip.slot_id, pf, i, presence_h.p,
                      tr.edge_presence_pm[i], static_cast<int>(tr.fold.seg),
                      tr.fold.shape_from, tr.fold.shape_to, tr.fold.morph_pm,
                      tr.fold.amp_pm, tr.coherence_pm, tr.edge_lit_pm,
                      tr.life_pm);
          ++diagnostics;
        }
      }
      if (tr.edge_energy_pm <= 0) ++out.edge_blackouts;
      if (have_fold) {
        if (tr.fold.shape_from != prior_fold.shape_from ||
            tr.fold.shape_to != prior_fold.shape_to) {
          if (prior_fold.shape_to != tr.fold.shape_from)
            ++out.shape_chain_breaks;
          prior_morph = -1;
        } else if (prior_morph >= 0 && tr.fold.morph_pm < prior_morph &&
                   tr.fold.shape_from != tr.fold.shape_to) {
          ++out.morph_reversals;
        }
      }
      prior_fold = tr.fold;
      prior_morph = tr.fold.morph_pm;
      have_fold = true;
    }
    // Stable edge/layer identities remain in the history even while their gain
    // is zero. This records fade-to-off, held-tail settling and discrete stamp
    // population exactly rather than dropping the operand with the geometry.
    for (int i = 0; i < u02::kStencilPts; ++i) {
      edge_presence_h[static_cast<size_t>(i)].push(
          tr.edge_presence_pm[i], sample);
      for (int layer = 0; layer < 3; ++layer) {
        edge_energy_h[static_cast<size_t>(i)][static_cast<size_t>(layer)].push(
            tr.edge_layer_gain_pm[i][layer], sample);
        edge_stamp_h[static_cast<size_t>(i)][static_cast<size_t>(layer)].push(
            tr.edge_layer_stamp_count[i][layer], sample);
        edge_accum_h[static_cast<size_t>(i)][static_cast<size_t>(layer)].push(
            tr.edge_layer_energy_pm[i][layer], sample);
      }
    }
    if (prior_mote_count >= 0 && tr.mote_count != prior_mote_count)
      ++out.mote_count_changes;
    prior_mote_count = tr.mote_count;
    for (int i = 0; i < u02::kMoteCount; ++i)
      mote_visibility_h[static_cast<size_t>(i)].push(
          i < tr.mote_count ? tr.mote_visibility_pm[i] : 0, sample);
    for (int i = 0; i < tr.mote_count; ++i) {
      if (prior_role[static_cast<size_t>(i)] != 0xFFu &&
          prior_role[static_cast<size_t>(i)] != tr.mote_role[i])
        ++out.role_changes;
      prior_role[static_cast<size_t>(i)] = tr.mote_role[i];
      if (tr.mote_visibility_pm[i] > 0) {
        const V3 p = mm_rel(tr.mote_position[i], anchors.body);
        EntityHistory& h = mote_h[static_cast<size_t>(i)];
        if (loops && sample == frames && h.have_p) {
          out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
          ++out.seam_samples;
        }
        if (h.have_p) {
          const double step = mag(sub(p, h.p));
          if (step > 420.0 && diagnostics < 6) {
            std::printf("     mote jump slot %u f%d id%d role%u %.2f mm "
                        "seg%d morph%d lasso%d vis%d\n",
                        clip.slot_id, sample, i, tr.mote_role[i], step,
                        static_cast<int>(tr.fold.seg), tr.fold.morph_pm,
                        tr.lasso_mix_pm, tr.mote_visibility_pm[i]);
            ++diagnostics;
          }
        }
        h.push(p, sample);
      }
    }
  }

  for (size_t i = 0; i < free_h.size(); ++i) {
    const EntityHistory& h = free_h[i];
    if (h.max_step > out.free_step) {
      out.free_step = h.max_step;
      out.free_step_frame = h.max_step_frame;
      out.free_step_id = static_cast<int>(i);
    }
    if (h.max_accel > out.free_accel) {
      out.free_accel = h.max_accel;
      out.free_accel_frame = h.max_accel_frame;
      out.free_accel_id = static_cast<int>(i);
    }
    if (h.max_jerk > out.free_jerk) {
      out.free_jerk = h.max_jerk;
      out.free_jerk_frame = h.max_jerk_frame;
      out.free_jerk_id = static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < fold_h.size(); ++i) {
    const EntityHistory& h = fold_h[i];
    if (h.max_step > out.fold_step) {
      out.fold_step = h.max_step;
      out.fold_step_frame = h.max_step_frame;
      out.fold_step_id = static_cast<int>(i);
    }
    if (h.max_accel > out.fold_accel) {
      out.fold_accel = h.max_accel;
      out.fold_accel_frame = h.max_accel_frame;
      out.fold_accel_id = static_cast<int>(i);
    }
    if (h.max_jerk > out.fold_jerk) {
      out.fold_jerk = h.max_jerk;
      out.fold_jerk_frame = h.max_jerk_frame;
      out.fold_jerk_id = static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < mote_h.size(); ++i) {
    const EntityHistory& h = mote_h[i];
    if (h.max_step > out.mote_step) {
      out.mote_step = h.max_step;
      out.mote_step_frame = h.max_step_frame;
      out.mote_step_id = static_cast<int>(i);
    }
    if (h.max_accel > out.mote_accel) {
      out.mote_accel = h.max_accel;
      out.mote_accel_frame = h.max_accel_frame;
      out.mote_accel_id = static_cast<int>(i);
    }
    if (h.max_jerk > out.mote_jerk) {
      out.mote_jerk = h.max_jerk;
      out.mote_jerk_frame = h.max_jerk_frame;
      out.mote_jerk_id = static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < surge_h.size(); ++i) {
    const EntityHistory& h = surge_h[i];
    if (h.max_step > out.surge_step) {
      out.surge_step = h.max_step;
      out.surge_step_frame = h.max_step_frame;
      out.surge_step_id = static_cast<int>(i);
    }
    if (h.max_accel > out.surge_accel) {
      out.surge_accel = h.max_accel;
      out.surge_accel_frame = h.max_accel_frame;
      out.surge_accel_id = static_cast<int>(i);
    }
    if (h.max_jerk > out.surge_jerk) {
      out.surge_jerk = h.max_jerk;
      out.surge_jerk_frame = h.max_jerk_frame;
      out.surge_jerk_id = static_cast<int>(i);
    }
  }
  for (const ScalarHistory& h : mote_visibility_h) {
    out.mote_visibility_step = std::max(out.mote_visibility_step, h.max_step);
    out.mote_visibility_accel = std::max(out.mote_visibility_accel, h.max_accel);
    out.mote_visibility_jerk = std::max(out.mote_visibility_jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : surge_visibility_h) {
    out.surge_visibility_step = std::max(out.surge_visibility_step, h.max_step);
    out.surge_visibility_accel = std::max(out.surge_visibility_accel, h.max_accel);
    out.surge_visibility_jerk = std::max(out.surge_visibility_jerk, h.max_jerk);
  }
  for (size_t i = 0; i < edge_presence_h.size(); ++i) {
    const ScalarHistory& h = edge_presence_h[i];
    if (h.max_step > out.edge_presence_step) {
      out.edge_presence_step = h.max_step;
      out.edge_presence_step_frame = h.max_step_frame;
      out.edge_presence_step_id = static_cast<int>(i);
    }
    if (h.max_accel > out.edge_presence_accel) {
      out.edge_presence_accel = h.max_accel;
      out.edge_presence_accel_frame = h.max_accel_frame;
      out.edge_presence_accel_id = static_cast<int>(i);
    }
    if (h.max_jerk > out.edge_presence_jerk) {
      out.edge_presence_jerk = h.max_jerk;
      out.edge_presence_jerk_frame = h.max_jerk_frame;
      out.edge_presence_jerk_id = static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < edge_energy_h.size(); ++i)
    for (size_t layer = 0; layer < edge_energy_h[i].size(); ++layer) {
      const ScalarHistory& h = edge_energy_h[i][layer];
      const ScalarHistory& sc = edge_stamp_h[i][layer];
      const ScalarHistory& en = edge_accum_h[i][layer];
      const int id = static_cast<int>(i * 3 + layer);
      if (h.max_step > out.edge_energy_step) {
        out.edge_energy_step = h.max_step;
        out.edge_energy_step_frame = h.max_step_frame;
        out.edge_energy_step_id = id;
      }
      if (h.max_accel > out.edge_energy_accel) {
        out.edge_energy_accel = h.max_accel;
        out.edge_energy_accel_frame = h.max_accel_frame;
        out.edge_energy_accel_id = id;
      }
      if (h.max_jerk > out.edge_energy_jerk) {
        out.edge_energy_jerk = h.max_jerk;
        out.edge_energy_jerk_frame = h.max_jerk_frame;
        out.edge_energy_jerk_id = id;
      }
      out.edge_stamp_step = std::max(out.edge_stamp_step, sc.max_step);
      out.edge_stamp_accel = std::max(out.edge_stamp_accel, sc.max_accel);
      out.edge_stamp_jerk = std::max(out.edge_stamp_jerk, sc.max_jerk);
      out.edge_accum_step = std::max(out.edge_accum_step, en.max_step);
      out.edge_accum_accel = std::max(out.edge_accum_accel, en.max_accel);
      out.edge_accum_jerk = std::max(out.edge_accum_jerk, en.max_jerk);
    }
  for (const auto& strand : free_stamp_h)
    for (const ScalarHistory& h : strand) {
      out.free_stamp_step = std::max(out.free_stamp_step, h.max_step);
      out.free_stamp_accel = std::max(out.free_stamp_accel, h.max_accel);
      out.free_stamp_jerk = std::max(out.free_stamp_jerk, h.max_jerk);
    }
  for (const auto& strand : free_energy_h)
    for (const ScalarHistory& h : strand) {
      out.free_energy_step = std::max(out.free_energy_step, h.max_step);
      out.free_energy_accel = std::max(out.free_energy_accel, h.max_accel);
      out.free_energy_jerk = std::max(out.free_energy_jerk, h.max_jerk);
    }
  return out;
}

int check_fold_weight_invariants() {
  const u02::FoldWeights& fw = u02::fold_weights();
  int bad_range = 0, bad_sum = 0;
  for (int sh = 0; sh < u02::kFoldStencilCount; ++sh)
    for (int st = 0; st < u02::kStencilPts; ++st) {
      int64_t sum = 0;
      for (int a = 0; a < 6; ++a) {
        const int32_t w = fw.w[sh][st][a];
        if (w < 0 || w > 4096) ++bad_range;
        sum += w;
      }
      if (sum != 4096) ++bad_sum;
    }
  std::printf("G0 fold MVC invariants: range failures %d, sum failures %d\n",
              bad_range, bad_sum);
  if (bad_range != 0)
    fail(kFailWeight, "a folded-lightning MVC weight is outside signed Q12 range");
  if (bad_sum != 0)
    fail(kFailWeight, "folded-lightning MVC weights do not sum to exact affine unity");
  return bad_range + bad_sum;
}

void check_phase_clocks(const zc::CreatureType& type) {
  int too_short = 0;
  int bad_chain = 0;
  int bad_endpoints = 0;
  for (const zc::Clip& clip : type.bank.clips) {
    const int total = std::max(2, static_cast<int>(clip.frame_count) * 2);
    // Slot 7 is a two-key static form diagnostic and never runs a live effect.
    if (total < u02::kFxMorphMinFrames) continue;
    const uint32_t cycles = std::max<uint32_t>(
        1u, static_cast<uint32_t>(total / u02::kBoltMorphFrames));
    const int duration = total / static_cast<int>(cycles);
    if (duration < u02::kFxMorphMinFrames) ++too_short;
    const u02::LightningMorphPhase first =
        u02::lightning_morph_phase(0, clip.frame_count,
                                   u02::kBoltMorphFrames);
    u02::LightningMorphPhase prev = first;
    if (first.morph_pm != 0) ++bad_endpoints;
    for (int f = 1; f < total; ++f) {
      const u02::LightningMorphPhase ph =
          u02::lightning_morph_phase(static_cast<uint32_t>(f), clip.frame_count,
                                     u02::kBoltMorphFrames);
      if (ph.from != prev.from && prev.to != ph.from) ++bad_chain;
      prev = ph;
    }
    if (prev.to != first.from) ++bad_chain;
  }
  std::printf("G1 persistent lightning clocks: short %d, chain breaks %d, "
              "bad starts %d\n", too_short, bad_chain, bad_endpoints);
  if (too_short != 0)
    fail(kFailPhase, "a lightning shape morph is shorter than the named floor");
  if (bad_chain != 0)
    fail(kFailPhase, "lightning phase identity does not hand old.to to new.from");
  if (bad_endpoints != 0)
    fail(kFailPhase, "a lightning loop does not begin at its authored morph endpoint");
  if (u02::kLassoShapeMixKeys * 2 < u02::kFxMorphMinFrames)
    fail(kFailPhase, "lasso figure handoff is shorter than the named motion floor");
}

void check_fold_morph_durations(const zc::CreatureType& type) {
  int short_morphs = 0;
  int completed = 0;
  int min_frames = std::numeric_limits<int>::max();
  for (const zc::Clip& clip : type.bank.clips) {
    if (clip.slot_id == 7) continue;
    int run = 0;
    u02::FoldSeg prior_seg = u02::kSegDrift;
    uint8_t prior_from = 0, prior_to = 0;
    const int frames = static_cast<int>(clip.frame_count) * 2;
    for (int pf = 0; pf <= frames; ++pf) {
      bool active = false;
      u02::FoldPhase ph{};
      if (pf < frames) {
        ph = u02::fold_phase(clip.slot_id, clip.frame_count, pf * 8);
        const int32_t life = u02::fold_life_pm(
            clip.slot_id, clip.frame_count, pf * 8);
        active = life > 0 && ph.shape_from != ph.shape_to &&
                 (ph.seg == u02::kSegKnead || ph.seg == u02::kSegRelease);
      }
      if (active && run > 0 && ph.seg == prior_seg &&
          ph.shape_from == prior_from && ph.shape_to == prior_to) {
        ++run;
      } else {
        if (run > 0) {
          ++completed;
          min_frames = std::min(min_frames, run);
          if (run < u02::kFxMorphMinFrames) ++short_morphs;
        }
        run = active ? 1 : 0;
        prior_seg = ph.seg;
        prior_from = ph.shape_from;
        prior_to = ph.shape_to;
      }
    }
  }
  std::printf("G2 folded-figure morph duration: %d completed, min %d frames, "
              "short %d\n", completed,
              completed > 0 ? min_frames : 0, short_morphs);
  if (short_morphs != 0)
    fail(kFailPhase, "a folded-lightning shape morph is shorter than the named motion floor");
}

struct NoiseMetrics {
  double step = 0.0, accel = 0.0, jerk = 0.0;
};

NoiseMetrics check_mote_jitter() {
  std::array<EntityHistory, u02::kMoteCount> hist{};
  constexpr int kKeys = 184;
  for (int f = 0; f < kKeys * 2; ++f)
    for (int m = 0; m < u02::kMoteCount; ++m) {
      int32_t p[3]{};
      u02::mote_knead_offset(static_cast<uint32_t>(f), kKeys, m,
                             u02::kKneadJitterMm, p);
      const int32_t z[3]{};
      hist[static_cast<size_t>(m)].push(mm_rel(p, z), f);
    }
  NoiseMetrics out;
  for (const EntityHistory& h : hist) {
    out.step = std::max(out.step, h.max_step);
    out.accel = std::max(out.accel, h.max_accel);
    out.jerk = std::max(out.jerk, h.max_jerk);
  }
  std::printf("G3 persistent mote noise step/accel/jerk: %.3f / %.3f / %.3f mm\n",
              out.step, out.accel, out.jerk);
  // The 70 mm authored radius remains; these reject replacement impulses.
  if (out.step > 30.0 || out.accel > 45.0 || out.jerk > 60.0)
    fail(kFailMoteMotion, "mote jitter reseeds instead of following a persistent path");
  for (int m = 0; m < u02::kMoteCount; ++m) {
    const uint8_t expected = m < u02::kMoteCount - u02::kWanderCount ? 0u : 1u;
    if (u02::persistent_mote_role(m) != expected)
      fail(kFailIdentity, "a persistent mote ID changed role");
  }
  return out;
}

void check_edge_presence() {
  int zeros = 0;
  for (int life = 1; life <= 1000; life += 37)
    for (int coh = 0; coh <= 1000; coh += 29)
      if (u02::fold_edge_lit_pm(coh, life) <= 0) ++zeros;
  std::printf("G4 living figure edge: %d non-death blackouts\n", zeros);
  if (zeros != 0)
    fail(kFailBlackout, "a living folded-lightning figure reaches an off/reappear blackout");
  if (u02::fold_edge_lit_pm(1000, 0) != 0)
    fail(kFailBlackout, "an authored death fade cannot reach exact zero");
}

struct LabMetrics {
  double station_step = 0.0, station_accel = 0.0, station_jerk = 0.0;
  double mote_step = 0.0, mote_accel = 0.0, mote_jerk = 0.0;
  double free_step = 0.0, free_accel = 0.0, free_jerk = 0.0;
  double surge_step = 0.0, surge_accel = 0.0, surge_jerk = 0.0;
  double mote_visibility_step = 0.0, mote_visibility_accel = 0.0,
         mote_visibility_jerk = 0.0;
  double surge_visibility_step = 0.0, surge_visibility_accel = 0.0,
         surge_visibility_jerk = 0.0;
  double seam_step = 0.0;
  int station_step_frame = -1, station_step_id = -1;
  int station_jerk_frame = -1, station_jerk_id = -1;
  int mote_step_frame = -1, mote_step_id = -1;
  int mote_jerk_frame = -1, mote_jerk_id = -1;
  u02::FoldPhase station_phase{}, station_prior_phase{};
  int count_changes = 0, role_changes = 0;
  int surge_count_changes = 0, surge_role_changes = 0;
  int edge_blackouts = 0;
  double edge_presence_step = 0.0, edge_presence_accel = 0.0,
         edge_presence_jerk = 0.0;
  double edge_stamp_step = 0.0, edge_stamp_accel = 0.0,
         edge_stamp_jerk = 0.0;
  double edge_energy_step = 0.0, edge_energy_accel = 0.0,
         edge_energy_jerk = 0.0;
};

LabMetrics trace_lab_variant(const zc::CreatureType& type, const zc::Clip& clip,
                             int vi) {
  LabMetrics out;
  std::array<EntityHistory, u02::kStencilPts> station_h{};
  std::array<EntityHistory, u02::lab::kLabTraceMaxMotes> mote_h{};
  std::array<EntityHistory,
             u02::kFxTraceMaxStrands * (u02::kBoltSegs + 1)> free_h{};
  std::array<EntityHistory, u02::kSurgeMotes> surge_h{};
  std::array<ScalarHistory, u02::lab::kLabTraceMaxMotes> mote_visibility_h{};
  std::array<ScalarHistory, u02::kSurgeMotes> surge_visibility_h{};
  std::array<uint8_t, u02::lab::kLabTraceMaxMotes> prior_role{};
  std::array<uint8_t, u02::kSurgeMotes> prior_surge_role{};
  std::array<ScalarHistory, u02::kStencilPts> edge_presence_h{};
  std::array<std::array<ScalarHistory, 2>, u02::kStencilPts> edge_stamp_h{};
  std::array<std::array<ScalarHistory, 2>, u02::kStencilPts> edge_energy_h{};
  prior_role.fill(0xFFu);
  prior_surge_role.fill(0xFFu);
  int prior_count = -1, prior_surge_count = -1;
  u02::FoldPhase prior_fold{};
  u02::FoldState state{};
  std::vector<u02::ManaSplat> splats;
  const int frames = static_cast<int>(clip.frame_count) * 2;
  for (int sample = 0; sample < frames + 3; ++sample) {
    const int pf = sample >= frames ? sample - frames : sample;
    const uint16_t key = static_cast<uint16_t>(pf / 2);
    const uint8_t subframe = static_cast<uint8_t>(pf & 1);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
    zc::decode_pose(type, clip, key, pose, nullptr, subframe);
    const u02::FxAnchors anchors = u02::fx_anchors_from_pose(type, pose);
    u02::lab::LabContinuityTrace lt{};
    u02::FxContinuityTrace ft{};
    splats.clear();
    int32_t agit = 0;
    u02::lab::lab_fill(u02::lab::kLabCandBase + vi,
                       static_cast<uint32_t>(pf), 0, clip.frame_count, anchors,
                       state, splats, &agit, &lt, &ft);

    for (int i = 0; i < lt.station_count; ++i) {
      const V3 p = mm_rel(lt.station[i], anchors.body);
      EntityHistory& h = station_h[static_cast<size_t>(i)];
      if (h.have_p) {
        const double step = mag(sub(p, h.p));
        if (step > out.station_step) {
          out.station_step = step;
          out.station_step_frame = sample;
          out.station_step_id = i;
          out.station_phase = lt.fold;
          out.station_prior_phase = prior_fold;
        }
      }
      if (sample == frames && h.have_p) {
        const double seam = mag(sub(p, h.p));
        out.seam_step = std::max(out.seam_step, seam);
        if (seam > 500.0 && i < 2)
          std::printf("     lab seam vi%d station%d %.2f mm last(%.0f,%.0f,%.0f) "
                      "first(%.0f,%.0f,%.0f)\n",
                      vi, i, seam, h.p.x, h.p.y, h.p.z, p.x, p.y, p.z);
      }
      h.push(p, sample);
    }
    if (prior_count >= 0 && lt.mote_count != prior_count) ++out.count_changes;
    prior_count = lt.mote_count;
    for (int i = 0; i < u02::lab::kLabTraceMaxMotes; ++i)
      mote_visibility_h[static_cast<size_t>(i)].push(
          i < lt.mote_count ? lt.mote_visibility_pm[i] : 0, sample);
    for (int i = 0; i < lt.mote_count; ++i) {
      if (prior_role[static_cast<size_t>(i)] != 0xFFu &&
          prior_role[static_cast<size_t>(i)] != lt.mote_role[i])
        ++out.role_changes;
      prior_role[static_cast<size_t>(i)] = lt.mote_role[i];
      if (lt.mote_visibility_pm[i] <= 0) continue;
      const V3 p = mm_rel(lt.mote_position[i], anchors.body);
      EntityHistory& h = mote_h[static_cast<size_t>(i)];
      if (sample == frames && h.have_p)
        out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
      h.push(p, sample);
    }
    if (prior_surge_count >= 0 && ft.surge_mote_count != prior_surge_count)
      ++out.surge_count_changes;
    prior_surge_count = ft.surge_mote_count;
    for (int i = 0; i < u02::kSurgeMotes; ++i)
      surge_visibility_h[static_cast<size_t>(i)].push(
          i < ft.surge_mote_count ? ft.surge_mote_visibility_pm[i] : 0,
          sample);
    for (int i = 0; i < ft.surge_mote_count; ++i) {
      if (prior_surge_role[static_cast<size_t>(i)] != 0xFFu &&
          prior_surge_role[static_cast<size_t>(i)] != ft.surge_mote_role[i])
        ++out.surge_role_changes;
      prior_surge_role[static_cast<size_t>(i)] = ft.surge_mote_role[i];
      if (ft.surge_mote_visibility_pm[i] <= 0) continue;
      const V3 p = mm_rel(ft.surge_mote_position[i], anchors.body);
      EntityHistory& h = surge_h[static_cast<size_t>(i)];
      if (sample == frames && h.have_p)
        out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
      h.push(p, sample);
    }
    for (int i = 0; i < ft.free_point_count; ++i) {
      const V3 p = mm_rel(ft.free_point[i], anchors.body);
      EntityHistory& h = free_h[static_cast<size_t>(i)];
      if (sample == frames && h.have_p)
        out.seam_step = std::max(out.seam_step, mag(sub(p, h.p)));
      h.push(p, sample);
    }
    if (u02::lab::kLabVariants[vi].edge_strands && lt.edge_energy_pm <= 0)
      ++out.edge_blackouts;
    for (int i = 0; i < u02::kStencilPts; ++i) {
      edge_presence_h[static_cast<size_t>(i)].push(
          lt.edge_presence_pm[i], sample);
      for (int layer = 0; layer < 2; ++layer) {
        edge_stamp_h[static_cast<size_t>(i)][static_cast<size_t>(layer)].push(
            lt.edge_layer_stamp_count[i][layer], sample);
        edge_energy_h[static_cast<size_t>(i)][static_cast<size_t>(layer)].push(
            lt.edge_layer_energy_pm[i][layer], sample);
      }
    }
    prior_fold = lt.fold;
  }
  for (size_t i = 0; i < station_h.size(); ++i) {
    const EntityHistory& h = station_h[i];
    if (h.max_step > out.station_step) {
      out.station_step = h.max_step;
      out.station_step_frame = h.max_step_frame;
      out.station_step_id = static_cast<int>(i);
    }
    out.station_accel = std::max(out.station_accel, h.max_accel);
    if (h.max_jerk > out.station_jerk) {
      out.station_jerk = h.max_jerk;
      out.station_jerk_frame = h.max_jerk_frame;
      out.station_jerk_id = static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < mote_h.size(); ++i) {
    const EntityHistory& h = mote_h[i];
    if (h.max_step > out.mote_step) {
      out.mote_step = h.max_step;
      out.mote_step_frame = h.max_step_frame;
      out.mote_step_id = static_cast<int>(i);
    }
    out.mote_accel = std::max(out.mote_accel, h.max_accel);
    if (h.max_jerk > out.mote_jerk) {
      out.mote_jerk = h.max_jerk;
      out.mote_jerk_frame = h.max_jerk_frame;
      out.mote_jerk_id = static_cast<int>(i);
    }
  }
  for (const EntityHistory& h : free_h) {
    out.free_step = std::max(out.free_step, h.max_step);
    out.free_accel = std::max(out.free_accel, h.max_accel);
    out.free_jerk = std::max(out.free_jerk, h.max_jerk);
  }
  for (const EntityHistory& h : surge_h) {
    out.surge_step = std::max(out.surge_step, h.max_step);
    out.surge_accel = std::max(out.surge_accel, h.max_accel);
    out.surge_jerk = std::max(out.surge_jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : mote_visibility_h) {
    out.mote_visibility_step = std::max(out.mote_visibility_step, h.max_step);
    out.mote_visibility_accel = std::max(out.mote_visibility_accel, h.max_accel);
    out.mote_visibility_jerk = std::max(out.mote_visibility_jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : surge_visibility_h) {
    out.surge_visibility_step = std::max(out.surge_visibility_step, h.max_step);
    out.surge_visibility_accel = std::max(out.surge_visibility_accel, h.max_accel);
    out.surge_visibility_jerk = std::max(out.surge_visibility_jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : edge_presence_h) {
    out.edge_presence_step = std::max(out.edge_presence_step, h.max_step);
    out.edge_presence_accel = std::max(out.edge_presence_accel, h.max_accel);
    out.edge_presence_jerk = std::max(out.edge_presence_jerk, h.max_jerk);
  }
  for (size_t i = 0; i < edge_stamp_h.size(); ++i)
    for (size_t layer = 0; layer < edge_stamp_h[i].size(); ++layer) {
      const ScalarHistory& sc = edge_stamp_h[i][layer];
      const ScalarHistory& en = edge_energy_h[i][layer];
      out.edge_stamp_step = std::max(out.edge_stamp_step, sc.max_step);
      out.edge_stamp_accel = std::max(out.edge_stamp_accel, sc.max_accel);
      out.edge_stamp_jerk = std::max(out.edge_stamp_jerk, sc.max_jerk);
      out.edge_energy_step = std::max(out.edge_energy_step, en.max_step);
      out.edge_energy_accel = std::max(out.edge_energy_accel, en.max_accel);
      out.edge_energy_jerk = std::max(out.edge_energy_jerk, en.max_jerk);
    }
  return out;
}

LabMetrics check_lab_continuity(const zc::CreatureType& type) {
  const zc::Clip* clip = nullptr;
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == 15) clip = &c;
  if (clip == nullptr) {
    fail(kFailCoverage, "the production bank has no Manafold lab clip");
    return LabMetrics{};
  }
  int count_changes = 0, role_changes = 0, blackouts = 0;
  int surge_counts = 0, surge_roles = 0;
  double station_step = 0.0, station_accel = 0.0, station_jerk = 0.0;
  double mote_step = 0.0, mote_accel = 0.0, mote_jerk = 0.0;
  double free_step = 0.0, free_accel = 0.0, free_jerk = 0.0;
  double surge_step = 0.0, surge_accel = 0.0, surge_jerk = 0.0;
  double mote_visibility_step = 0.0, mote_visibility_accel = 0.0,
         mote_visibility_jerk = 0.0;
  double surge_visibility_step = 0.0, surge_visibility_accel = 0.0,
         surge_visibility_jerk = 0.0;
  double seam_step = 0.0;
  double edge_presence_step = 0.0, edge_presence_accel = 0.0,
         edge_presence_jerk = 0.0;
  double edge_stamp_step = 0.0, edge_stamp_accel = 0.0,
         edge_stamp_jerk = 0.0;
  double edge_energy_step = 0.0, edge_energy_accel = 0.0,
         edge_energy_jerk = 0.0;
  for (int vi = 0; vi < u02::lab::kLabVariantCount; ++vi) {
    const LabMetrics m = trace_lab_variant(type, *clip, vi);
    if (m.station_step > kFoldStepMaxMm ||
        m.station_accel > kFoldAccelMaxMm ||
        m.station_jerk > kFoldJerkMaxMm ||
        m.mote_step > kFoldMoteStepMaxMm ||
        m.mote_accel > kFoldMoteAccelMaxMm ||
        m.mote_jerk > kFoldMoteJerkMaxMm ||
        m.free_step > kFreeStepMaxMm || m.free_accel > kFreeAccelMaxMm ||
        m.free_jerk > kFreeJerkMaxMm ||
        m.surge_step > kSurgeStepMaxMm || m.surge_accel > kSurgeAccelMaxMm ||
        m.surge_jerk > kSurgeJerkMaxMm ||
        m.seam_step > kSeamStepMaxMm ||
        m.edge_presence_step > kEdgePresenceStepMaxPm ||
        m.edge_presence_accel > kEdgePresenceAccelMaxPm ||
        m.edge_presence_jerk > kEdgePresenceJerkMaxPm ||
        m.edge_stamp_step > kStampCountStepMax ||
        m.edge_stamp_accel > kStampCountAccelMax ||
        m.edge_stamp_jerk > kStampCountJerkMax ||
        m.edge_energy_step > kAccumEnergyStepMaxPm ||
        m.edge_energy_accel > kAccumEnergyAccelMaxPm ||
        m.edge_energy_jerk > kAccumEnergyJerkMaxPm ||
        m.count_changes != 0 || m.role_changes != 0)
      std::printf("   lab %d %-18s station %.2f@f%d/id%d jerk %.2f@f%d/id%d "
                  "(%u->%u m%d; prev %u->%u m%d) "
                  "mote %.2f@f%d/id%d jerk %.2f@f%d/id%d "
                  "free %.2f surge %.2f seam %.2f "
                  "edge %.0f/%.0f/%.0f stamps %.0f/%.0f/%.0f "
                  "energy %.0f/%.0f/%.0f count/role %d/%d\n",
                  vi, u02::lab::kLabVariants[vi].name, m.station_step,
                  m.station_step_frame, m.station_step_id, m.station_jerk,
                  m.station_jerk_frame, m.station_jerk_id,
                  m.station_phase.shape_from, m.station_phase.shape_to,
                  m.station_phase.morph_pm, m.station_prior_phase.shape_from,
                  m.station_prior_phase.shape_to, m.station_prior_phase.morph_pm,
                  m.mote_step, m.mote_step_frame, m.mote_step_id, m.mote_jerk,
                  m.mote_jerk_frame, m.mote_jerk_id, m.free_step,
                  m.surge_step, m.seam_step,
                  m.edge_presence_step, m.edge_presence_accel,
                  m.edge_presence_jerk, m.edge_stamp_step,
                  m.edge_stamp_accel, m.edge_stamp_jerk,
                  m.edge_energy_step, m.edge_energy_accel,
                  m.edge_energy_jerk, m.count_changes, m.role_changes);
    station_step = std::max(station_step, m.station_step);
    station_accel = std::max(station_accel, m.station_accel);
    station_jerk = std::max(station_jerk, m.station_jerk);
    mote_step = std::max(mote_step, m.mote_step);
    mote_accel = std::max(mote_accel, m.mote_accel);
    mote_jerk = std::max(mote_jerk, m.mote_jerk);
    free_step = std::max(free_step, m.free_step);
    free_accel = std::max(free_accel, m.free_accel);
    free_jerk = std::max(free_jerk, m.free_jerk);
    surge_step = std::max(surge_step, m.surge_step);
    surge_accel = std::max(surge_accel, m.surge_accel);
    surge_jerk = std::max(surge_jerk, m.surge_jerk);
    mote_visibility_step = std::max(mote_visibility_step, m.mote_visibility_step);
    mote_visibility_accel = std::max(mote_visibility_accel, m.mote_visibility_accel);
    mote_visibility_jerk = std::max(mote_visibility_jerk, m.mote_visibility_jerk);
    surge_visibility_step = std::max(surge_visibility_step, m.surge_visibility_step);
    surge_visibility_accel = std::max(surge_visibility_accel, m.surge_visibility_accel);
    surge_visibility_jerk = std::max(surge_visibility_jerk, m.surge_visibility_jerk);
    seam_step = std::max(seam_step, m.seam_step);
    edge_presence_step = std::max(edge_presence_step, m.edge_presence_step);
    edge_presence_accel = std::max(edge_presence_accel, m.edge_presence_accel);
    edge_presence_jerk = std::max(edge_presence_jerk, m.edge_presence_jerk);
    edge_stamp_step = std::max(edge_stamp_step, m.edge_stamp_step);
    edge_stamp_accel = std::max(edge_stamp_accel, m.edge_stamp_accel);
    edge_stamp_jerk = std::max(edge_stamp_jerk, m.edge_stamp_jerk);
    edge_energy_step = std::max(edge_energy_step, m.edge_energy_step);
    edge_energy_accel = std::max(edge_energy_accel, m.edge_energy_accel);
    edge_energy_jerk = std::max(edge_energy_jerk, m.edge_energy_jerk);
    count_changes += m.count_changes;
    role_changes += m.role_changes;
    surge_counts += m.surge_count_changes;
    surge_roles += m.surge_role_changes;
    blackouts += m.edge_blackouts;
  }
  std::printf("G5 lab production continuity:\n"
              "   stations step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   motes    step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   strands  step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   surge    step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   visibility fold %.0f/%.0f/%.0f pm; surge %.0f/%.0f/%.0f pm\n"
              "   seam %.2f mm; edge presence %.0f/%.0f/%.0f pm; "
              "stamps %.0f/%.0f/%.0f; energy %.0f/%.0f/%.0f pm; "
              "count/role %d/%d surge %d/%d blackouts %d\n",
              station_step, station_accel, station_jerk,
              mote_step, mote_accel, mote_jerk,
              free_step, free_accel, free_jerk,
              surge_step, surge_accel, surge_jerk,
              mote_visibility_step, mote_visibility_accel, mote_visibility_jerk,
              surge_visibility_step, surge_visibility_accel, surge_visibility_jerk,
              seam_step,
              edge_presence_step, edge_presence_accel, edge_presence_jerk,
              edge_stamp_step, edge_stamp_accel, edge_stamp_jerk,
              edge_energy_step, edge_energy_accel, edge_energy_jerk,
              count_changes, role_changes, surge_counts,
              surge_roles, blackouts);
  if (count_changes != 0 || surge_counts != 0)
    fail(kFailIdentity, "a lab particle population changes count");
  if (role_changes != 0 || surge_roles != 0)
    fail(kFailIdentity, "a lab particle ID changes role");
  if (blackouts != 0)
    fail(kFailBlackout, "a living lab edge reaches a full blackout");
  if (station_step > kFoldStepMaxMm || mote_step > kFoldMoteStepMaxMm ||
      free_step > kFreeStepMaxMm || surge_step > kSurgeStepMaxMm ||
      seam_step > kSeamStepMaxMm)
    fail(kFailLabMotion, "a lab effect entity teleports between presentation frames");
  if (station_accel > kFoldAccelMaxMm || station_jerk > kFoldJerkMaxMm ||
      mote_accel > kFoldMoteAccelMaxMm || mote_jerk > kFoldMoteJerkMaxMm ||
      free_accel > kFreeAccelMaxMm || free_jerk > kFreeJerkMaxMm ||
      surge_accel > kSurgeAccelMaxMm || surge_jerk > kSurgeJerkMaxMm)
    fail(kFailLabMotion,
         "a lab effect entity exceeds the visually accepted acceleration/jerk band");
  if (mote_visibility_step > kMoteVisibilityStepMaxPm ||
      mote_visibility_accel > kMoteVisibilityAccelMaxPm ||
      mote_visibility_jerk > kMoteVisibilityJerkMaxPm ||
      surge_visibility_step > kMoteVisibilityStepMaxPm ||
      surge_visibility_accel > kMoteVisibilityAccelMaxPm ||
      surge_visibility_jerk > kMoteVisibilityJerkMaxPm)
    fail(kFailMoteVisibility,
         "a lab particle visibility turns off or reappears discontinuously");
  if (edge_presence_step > kEdgePresenceStepMaxPm ||
      edge_presence_accel > kEdgePresenceAccelMaxPm ||
      edge_presence_jerk > kEdgePresenceJerkMaxPm)
    fail(kFailEdgePresence,
         "a lab edge identity appears or disappears without a smooth crossfade");
  if (edge_stamp_step > kStampCountStepMax ||
      edge_stamp_accel > kStampCountAccelMax ||
      edge_stamp_jerk > kStampCountJerkMax)
    fail(kFailStampCount, "a lab edge stamp population changes discontinuously");
  if (edge_energy_step > kLabAccumEnergyStepMaxPm ||
      edge_energy_accel > kLabAccumEnergyAccelMaxPm ||
      edge_energy_jerk > kLabAccumEnergyJerkMaxPm)
    fail(kFailLabEnergy, "a lab edge accumulated layer energy changes discontinuously");
  LabMetrics summary;
  summary.station_step = station_step;
  summary.station_accel = station_accel;
  summary.station_jerk = station_jerk;
  summary.mote_step = mote_step;
  summary.mote_accel = mote_accel;
  summary.mote_jerk = mote_jerk;
  summary.free_step = free_step;
  summary.free_accel = free_accel;
  summary.free_jerk = free_jerk;
  summary.surge_step = surge_step;
  summary.surge_accel = surge_accel;
  summary.surge_jerk = surge_jerk;
  summary.mote_visibility_step = mote_visibility_step;
  summary.mote_visibility_accel = mote_visibility_accel;
  summary.mote_visibility_jerk = mote_visibility_jerk;
  summary.surge_visibility_step = surge_visibility_step;
  summary.surge_visibility_accel = surge_visibility_accel;
  summary.surge_visibility_jerk = surge_visibility_jerk;
  summary.seam_step = seam_step;
  summary.edge_presence_step = edge_presence_step;
  summary.edge_presence_accel = edge_presence_accel;
  summary.edge_presence_jerk = edge_presence_jerk;
  summary.edge_stamp_step = edge_stamp_step;
  summary.edge_stamp_accel = edge_stamp_accel;
  summary.edge_stamp_jerk = edge_stamp_jerk;
  summary.edge_energy_step = edge_energy_step;
  summary.edge_energy_accel = edge_energy_accel;
  summary.edge_energy_jerk = edge_energy_jerk;
  summary.count_changes = count_changes;
  summary.role_changes = role_changes;
  summary.surge_count_changes = surge_counts;
  summary.surge_role_changes = surge_roles;
  summary.edge_blackouts = blackouts;
  return summary;
}

struct BodyMetrics {
  double step = 0.0, accel = 0.0, jerk = 0.0, seam = 0.0;
  double radius_step = 0.0, radius_accel = 0.0, radius_jerk = 0.0;
  double gain_step = 0.0, gain_accel = 0.0, gain_jerk = 0.0;
  int count_changes = 0, role_changes = 0;
};

BodyMetrics trace_mana_bodies(const zc::CreatureType& type,
                              const zc::Clip& clip, int candidate) {
  BodyMetrics out;
  std::array<EntityHistory, u02::kFxTraceManaBodies> position{};
  std::array<ScalarHistory, u02::kFxTraceManaBodies> radius{}, gain{};
  std::array<uint8_t, u02::kFxTraceManaBodies> prior_role{};
  prior_role.fill(0xFFu);
  int prior_count = -1;
  u02::FoldState state{};
  std::vector<u02::ManaSplat> splats;
  const int frames = static_cast<int>(clip.frame_count) * 2;
  for (int sample = 0; sample < frames + 3; ++sample) {
    const int pf = sample >= frames ? sample - frames : sample;
    std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
    zc::decode_pose(type, clip, static_cast<uint16_t>(pf / 2), pose, nullptr,
                    static_cast<uint8_t>(pf & 1));
    const u02::FxAnchors anchors = u02::fx_anchors_from_pose(type, pose);
    u02::FxContinuityTrace tr{};
    splats.clear();
    int32_t agit = 0;
    u02::mana_fill(candidate, static_cast<uint32_t>(pf), clip.slot_id,
                   clip.frame_count, anchors, state, 1000, splats, &agit, &tr);
    if (prior_count >= 0 && tr.mana_body_count != prior_count)
      ++out.count_changes;
    prior_count = tr.mana_body_count;
    for (int i = 0; i < tr.mana_body_count; ++i) {
      if (prior_role[static_cast<size_t>(i)] != 0xFFu &&
          prior_role[static_cast<size_t>(i)] != tr.mana_body_role[i])
        ++out.role_changes;
      prior_role[static_cast<size_t>(i)] = tr.mana_body_role[i];
      if (tr.mana_body_visibility_pm[i] <= 0) continue;
      const V3 p = mm_rel(tr.mana_body_position[i], anchors.body);
      if (sample == frames && position[static_cast<size_t>(i)].have_p)
        out.seam = std::max(
            out.seam,
            mag(sub(p, position[static_cast<size_t>(i)].p)));
      position[static_cast<size_t>(i)].push(p, sample);
      radius[static_cast<size_t>(i)].push(tr.mana_body_radius_px[i], sample);
      gain[static_cast<size_t>(i)].push(tr.mana_body_gain_pm[i], sample);
    }
  }
  for (const EntityHistory& h : position) {
    out.step = std::max(out.step, h.max_step);
    out.accel = std::max(out.accel, h.max_accel);
    out.jerk = std::max(out.jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : radius) {
    out.radius_step = std::max(out.radius_step, h.max_step);
    out.radius_accel = std::max(out.radius_accel, h.max_accel);
    out.radius_jerk = std::max(out.radius_jerk, h.max_jerk);
  }
  for (const ScalarHistory& h : gain) {
    out.gain_step = std::max(out.gain_step, h.max_step);
    out.gain_accel = std::max(out.gain_accel, h.max_accel);
    out.gain_jerk = std::max(out.gain_jerk, h.max_jerk);
  }
  return out;
}

void check_shipping_mana_bodies(const zc::CreatureType& type) {
  const zc::Clip* idle = nullptr;
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == u02::kIdleFixedSlot) idle = &c;
  if (idle == nullptr) {
    fail(kFailCoverage, "the production bank has no fixed-camera idle for mana-menu effects");
    return;
  }
  static constexpr int kCandidates[] = {1, 2, 5, 7, 8};
  double step = 0.0, accel = 0.0, jerk = 0.0, seam = 0.0;
  double radius_step = 0.0, radius_accel = 0.0, radius_jerk = 0.0;
  double gain_step = 0.0, gain_accel = 0.0, gain_jerk = 0.0;
  int count_changes = 0, role_changes = 0;
  for (int cand : kCandidates) {
    const BodyMetrics m = trace_mana_bodies(type, *idle, cand);
    std::printf("   candidate %d body %.2f/%.2f/%.2f seam %.2f "
                "radius %.2f/%.2f/%.2f gain %.2f/%.2f/%.2f count/role %d/%d\n",
                cand, m.step, m.accel, m.jerk, m.seam,
                m.radius_step, m.radius_accel, m.radius_jerk,
                m.gain_step, m.gain_accel, m.gain_jerk,
                m.count_changes, m.role_changes);
    step = std::max(step, m.step);
    accel = std::max(accel, m.accel);
    jerk = std::max(jerk, m.jerk);
    seam = std::max(seam, m.seam);
    radius_step = std::max(radius_step, m.radius_step);
    radius_accel = std::max(radius_accel, m.radius_accel);
    radius_jerk = std::max(radius_jerk, m.radius_jerk);
    gain_step = std::max(gain_step, m.gain_step);
    gain_accel = std::max(gain_accel, m.gain_accel);
    gain_jerk = std::max(gain_jerk, m.gain_jerk);
    count_changes += m.count_changes;
    role_changes += m.role_changes;
  }
  std::printf("G6 shipping mana-body continuity: position %.2f/%.2f/%.2f "
              "seam %.2f; radius %.2f/%.2f/%.2f; gain %.2f/%.2f/%.2f; "
              "count/role %d/%d\n",
              step, accel, jerk, seam, radius_step, radius_accel, radius_jerk,
              gain_step, gain_accel, gain_jerk, count_changes, role_changes);
  // Initial structural bands; final values are tightened only after native seam
  // and badness review. They reject fixed-period reset impulses immediately.
  if (step > kManaBodyStepMaxMm || accel > kManaBodyAccelMaxMm ||
      jerk > kManaBodyJerkMaxMm || seam > kManaBodyStepMaxMm)
    fail(kFailManaBody, "a shipping mana body jumps or resets across its loop");
  if (radius_step > kManaBodyRadiusStepMaxPx ||
      radius_accel > kManaBodyRadiusAccelMaxPx ||
      radius_jerk > kManaBodyRadiusJerkMaxPx)
    fail(kFailManaBody, "a shipping mana-body radius snaps instead of breathing continuously");
  if (gain_step > 80.0 || gain_accel > 120.0 || gain_jerk > 160.0)
    fail(kFailManaBody, "a shipping mana-body gain snaps instead of changing continuously");
  if (count_changes != 0 || role_changes != 0)
    fail(kFailManaBody, "a shipping mana-body ID domain changes count or role");
}

struct PaletteMetrics {
  double entry_step = 0.0, entry_accel = 0.0, entry_jerk = 0.0;
  double energy_step = 0.0, energy_accel = 0.0, energy_jerk = 0.0;
  double seam_step = 0.0;
  int entry_step_frame = -1, entry_step_palette = -1, entry_step_index = -1;
};

PaletteMetrics trace_boil_palette(int frames) {
  PaletteMetrics out;
  if (frames <= 0) return out;
  std::array<std::array<EntityHistory, 64>, 2> entries{};
  std::array<ScalarHistory, 2> energy{};
  std::array<uint32_t, 64> bloom_hist{};
  u02::GlowAssets assets{};
  u02::glow_bake(assets);
  for (uint8_t index : assets.bloom.pix)
    if (index < bloom_hist.size()) ++bloom_hist[index];
  uint64_t bloom_live = 0;
  for (int i = 1; i < 64; ++i) bloom_live += bloom_hist[static_cast<size_t>(i)];
  constexpr int kRamp[2] = {u02::kRampBlue, u02::kRampViolet};
  u02::GlowFrame ramps[u02::kRampCount]{};
  for (int sample = 0; sample < frames + 3; ++sample) {
    const int pf = sample % frames;
    u02::mana_build_ramps(ramps, static_cast<uint32_t>(pf),
                          static_cast<uint32_t>(frames));
    for (int p = 0; p < 2; ++p) {
      uint64_t emitted = 0;
      for (int i = 1; i < 64; ++i) {
        const uint8_t* rgb = ramps[kRamp[p]].pal[i];
        const V3 c{static_cast<double>(rgb[0]), static_cast<double>(rgb[1]),
                   static_cast<double>(rgb[2])};
        EntityHistory& h = entries[static_cast<size_t>(p)][static_cast<size_t>(i)];
        if (sample == frames && h.have_p)
          out.seam_step = std::max(out.seam_step, mag(sub(c, h.p)));
        h.push(c, sample);
        emitted += static_cast<uint64_t>(bloom_hist[static_cast<size_t>(i)]) *
                   (static_cast<uint32_t>(rgb[0]) + rgb[1] + rgb[2]);
      }
      // The renderer indexes this exact bloom sprite by CLUT value. Normalize
      // its emitted unsaturated RGB sum to per-mille only to keep the gate's
      // units independent of sprite resolution; the histogram weighting is the
      // actual production population rather than a rotation-invariant CLUT sum.
      const double emitted_pm = bloom_live > 0
          ? static_cast<double>(emitted) * 1000.0 /
                (static_cast<double>(bloom_live) * 3.0 * 255.0)
          : 0.0;
      energy[static_cast<size_t>(p)].push(emitted_pm, sample);
    }
  }
  for (int p = 0; p < 2; ++p) {
    for (int i = 1; i < 64; ++i) {
      const EntityHistory& h =
          entries[static_cast<size_t>(p)][static_cast<size_t>(i)];
      if (h.max_step > out.entry_step) {
        out.entry_step = h.max_step;
        out.entry_step_frame = h.max_step_frame;
        out.entry_step_palette = p;
        out.entry_step_index = i;
      }
      out.entry_accel = std::max(out.entry_accel, h.max_accel);
      out.entry_jerk = std::max(out.entry_jerk, h.max_jerk);
    }
    const ScalarHistory& e = energy[static_cast<size_t>(p)];
    out.energy_step = std::max(out.energy_step, e.max_step);
    out.energy_accel = std::max(out.energy_accel, e.max_accel);
    out.energy_jerk = std::max(out.energy_jerk, e.max_jerk);
  }
  return out;
}

PaletteMetrics check_boil_palette(const zc::CreatureType& type) {
  const zc::Clip* idle = nullptr;
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == u02::kIdleFixedSlot) idle = &c;
  if (idle == nullptr) {
    fail(kFailCoverage,
         "the production bank has no fixed-camera idle for Boil palette checks");
    return PaletteMetrics{};
  }
  const int frames = static_cast<int>(idle->frame_count) * 2;
  const PaletteMetrics m = trace_boil_palette(frames);
  int exact_phase_mismatches = 0;
  constexpr uint8_t kBlack[3] = {0, 0, 0};
  for (int rot = 0; rot < 63; ++rot) {
    u02::GlowFrame exact_blue{}, phase_blue{}, exact_violet{}, phase_violet{};
    u02::glow_build_ramp(exact_blue, kBlack, u02::kManaBlueMid,
                         u02::kManaBlueHi, 1000, rot);
    u02::glow_build_ramp_phase(phase_blue, kBlack, u02::kManaBlueMid,
                               u02::kManaBlueHi, 1000, rot << 16);
    u02::glow_build_ramp(exact_violet, kBlack, u02::kManaVioletMid,
                         u02::kManaVioletHi, 1000, rot);
    u02::glow_build_ramp_phase(phase_violet, kBlack, u02::kManaVioletMid,
                               u02::kManaVioletHi, 1000, rot << 16);
    if (std::memcmp(&exact_blue, &phase_blue, sizeof(exact_blue)) != 0)
      ++exact_phase_mismatches;
    if (std::memcmp(&exact_violet, &phase_violet, sizeof(exact_violet)) != 0)
      ++exact_phase_mismatches;
  }
  std::printf("G7 Boil palette continuity: %d frames; entry %.2f/%.2f/%.2f "
              "seam %.2f @f%d/p%d/i%d; energy %.2f/%.2f/%.2f; "
              "integer-phase mismatches %d/126\n",
              frames, m.entry_step, m.entry_accel, m.entry_jerk,
              m.seam_step, m.entry_step_frame, m.entry_step_palette,
              m.entry_step_index, m.energy_step, m.energy_accel,
              m.energy_jerk, exact_phase_mismatches);
  if (exact_phase_mismatches != 0)
    fail(kFailPalette,
         "fractional Boil interpolation does not reproduce the authored CLUT at integer phases");
  if (m.entry_step > kPaletteEntryStepMax ||
      m.entry_accel > kPaletteEntryAccelMax ||
      m.entry_jerk > kPaletteEntryJerkMax ||
      m.seam_step > kPaletteEntryStepMax)
    fail(kFailPalette,
         "the production Boil CLUT changes colour discontinuously");
  if (m.energy_step > kPaletteEnergyStepMax ||
      m.energy_accel > kPaletteEnergyAccelMax ||
      m.energy_jerk > kPaletteEnergyJerkMax)
    fail(kFailPalette,
         "the production Boil CLUT accumulated colour energy snaps");
  return m;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fail-lightning-switch") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kLightningSwitch;
    else if (std::strcmp(argv[i], "--fail-shape-blackout") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kShapeBlackout;
    else if (std::strcmp(argv[i], "--fail-particle-reseed") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kParticleReseed;
    else if (std::strcmp(argv[i], "--fail-surge-reseed") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kSurgeReseed;
    else if (std::strcmp(argv[i], "--fail-loop-seam") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kLoopSeamSnap;
    else if (std::strcmp(argv[i], "--fail-mote-count") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kMoteCountPop;
    else if (std::strcmp(argv[i], "--fail-mote-role") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kMoteRolePop;
    else if (std::strcmp(argv[i], "--fail-weight-wrap") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kWeightWrap;
    else if (std::strcmp(argv[i], "--fail-morph-reverse") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kMorphReverse;
    else if (std::strcmp(argv[i], "--fail-brightness-seam") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kBrightnessSeam;
    else if (std::strcmp(argv[i], "--fail-stamp-count") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kStampCountPop;
    else if (std::strcmp(argv[i], "--fail-final-dwell") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kFinalDwell;
    else if (std::strcmp(argv[i], "--fail-mote-visibility") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kMoteVisibilityPop;
    else if (std::strcmp(argv[i], "--fail-palette-raw-clock") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kPaletteRawClock;
    else if (std::strcmp(argv[i], "--fail-palette-hard-switch") == 0)
      u02::g_u02_fx_continuity_fault = u02::FxContinuityFault::kPaletteHardSwitch;
    else if (std::strcmp(argv[i], "--boil-cycles") == 0 && i + 1 < argc) {
      const char* text = argv[++i];
      if (text[0] < '1' || text[0] > '3' || text[1] != '\0') return 2;
      u02::g_u02_boil_cycles = text[0] - '0';
    }
    else if (std::strcmp(argv[i], "--star-heart-shift") == 0 && i + 1 < argc) {
      char* end = nullptr;
      const long v = std::strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0' || v < 0 || v >= u02::kStencilPts)
        return 2;
      u02::g_u02_star_heart_station_shift = static_cast<int>(v);
    }
    else {
      std::fprintf(stderr,
                   "usage: %s [--fail-lightning-switch|--fail-shape-blackout|"
                   "--fail-particle-reseed|--fail-surge-reseed|--fail-loop-seam|"
                   "--fail-mote-count|--fail-mote-role|--fail-weight-wrap|"
                   "--fail-morph-reverse|--fail-brightness-seam|"
                   "--fail-stamp-count|--fail-final-dwell|"
                   "--fail-mote-visibility|--fail-palette-raw-clock|"
                   "--fail-palette-hard-switch|--boil-cycles 1|2|3|"
                   "--star-heart-shift N]\n",
                   argv[0]);
      return 2;
    }
  }

  const zc::CreatureType& type = u02::type();
  std::printf("MANAFOLD DIRECTION-18 EFFECT CONTINUITY GATE\n");
  std::printf("  STAR<->HEART station shift %d (reversed)\n",
              u02::g_u02_star_heart_station_shift >= 0
                  ? u02::g_u02_star_heart_station_shift
                  : u02::kStarHeartStationShift);
  const int weight_faults = check_fold_weight_invariants();
  check_phase_clocks(type);
  check_fold_morph_durations(type);
  const NoiseMetrics mote_noise = check_mote_jitter();
  check_edge_presence();
  const LabMetrics lab_metrics = check_lab_continuity(type);
  check_shipping_mana_bodies(type);
  const PaletteMetrics palette_metrics = check_boil_palette(type);

  ClipMetrics worst{};
  int blackouts = 0, roles = 0, counts = 0, chains = 0, reversals = 0;
  int surge_roles = 0, surge_counts = 0, seam_samples = 0;
  int held_tail_samples = 0;
  double seam_step = 0.0;
  int fa_slot=-1, fa_frame=-1, fa_id=-1, fj_slot=-1, fj_frame=-1, fj_id=-1;
  int xa_slot=-1, xa_frame=-1, xa_id=-1, xj_slot=-1, xj_frame=-1, xj_id=-1;
  int ma_slot=-1, ma_frame=-1, ma_id=-1, mj_slot=-1, mj_frame=-1, mj_id=-1;
  int sa_slot=-1, sa_frame=-1, sa_id=-1, sj_slot=-1, sj_frame=-1, sj_id=-1;
  const auto track = [](double value, int slot, int frame, int id,
                        double& best, int& best_slot, int& best_frame,
                        int& best_id) {
    if (value > best) {
      best = value;
      best_slot = slot;
      best_frame = frame;
      best_id = id;
    }
  };
  for (const zc::Clip& clip : type.bank.clips) {
    if (clip.slot_id == 7) continue;  // static form diagnostic: no live FX subject
    const ClipMetrics m = trace_clip(type, clip);
    if (m.shape_chain_breaks != 0 || m.free_step > 250.0 ||
        m.fold_step > 500.0 || m.mote_step > 420.0)
      std::printf("   slot %u: free %.2f@f%d/id%d fold %.2f@f%d/id%d "
                  "(prev seg%d %u->%u morph%d turn%d; "
                  "now seg%d %u->%u morph%d turn%d lasso%d life%d agit%d knead%d) "
                  "mote %.2f@f%d/id%d chain %d black %d\n",
                  clip.slot_id, m.free_step, m.free_step_frame, m.free_step_id,
                  m.fold_step, m.fold_step_frame, m.fold_step_id,
                  static_cast<int>(m.fold_step_prior_phase.seg),
                  m.fold_step_prior_phase.shape_from,
                  m.fold_step_prior_phase.shape_to,
                  m.fold_step_prior_phase.morph_pm,
                  m.fold_step_prior_phase.turn_a16,
                  static_cast<int>(m.fold_step_phase.seg),
                  m.fold_step_phase.shape_from, m.fold_step_phase.shape_to,
                  m.fold_step_phase.morph_pm, m.fold_step_phase.turn_a16,
                  m.fold_step_lasso_pm, m.fold_step_life_pm,
                  m.fold_step_agitation_pm, m.fold_step_knead_pm,
                  m.mote_step, m.mote_step_frame, m.mote_step_id,
                  m.shape_chain_breaks, m.edge_blackouts);
    if (m.edge_presence_step > 200.0 || m.edge_energy_step > 200.0)
      std::printf("     energy slot %u presence %.0f/%.0f/%.0f @f%d/id%d "
                  "layers %.0f/%.0f/%.0f @f%d/id%d\n",
                  clip.slot_id, m.edge_presence_step, m.edge_presence_accel,
                  m.edge_presence_jerk, m.edge_presence_step_frame,
                  m.edge_presence_step_id, m.edge_energy_step,
                  m.edge_energy_accel, m.edge_energy_jerk,
                  m.edge_energy_step_frame, m.edge_energy_step_id);
    worst.free_step = std::max(worst.free_step, m.free_step);
    track(m.free_accel, clip.slot_id, m.free_accel_frame, m.free_accel_id,
          worst.free_accel, fa_slot, fa_frame, fa_id);
    track(m.free_jerk, clip.slot_id, m.free_jerk_frame, m.free_jerk_id,
          worst.free_jerk, fj_slot, fj_frame, fj_id);
    worst.fold_step = std::max(worst.fold_step, m.fold_step);
    track(m.fold_accel, clip.slot_id, m.fold_accel_frame, m.fold_accel_id,
          worst.fold_accel, xa_slot, xa_frame, xa_id);
    track(m.fold_jerk, clip.slot_id, m.fold_jerk_frame, m.fold_jerk_id,
          worst.fold_jerk, xj_slot, xj_frame, xj_id);
    worst.mote_step = std::max(worst.mote_step, m.mote_step);
    track(m.mote_accel, clip.slot_id, m.mote_accel_frame, m.mote_accel_id,
          worst.mote_accel, ma_slot, ma_frame, ma_id);
    track(m.mote_jerk, clip.slot_id, m.mote_jerk_frame, m.mote_jerk_id,
          worst.mote_jerk, mj_slot, mj_frame, mj_id);
    worst.surge_step = std::max(worst.surge_step, m.surge_step);
    track(m.surge_accel, clip.slot_id, m.surge_accel_frame, m.surge_accel_id,
          worst.surge_accel, sa_slot, sa_frame, sa_id);
    track(m.surge_jerk, clip.slot_id, m.surge_jerk_frame, m.surge_jerk_id,
          worst.surge_jerk, sj_slot, sj_frame, sj_id);
    worst.mote_visibility_step =
        std::max(worst.mote_visibility_step, m.mote_visibility_step);
    worst.mote_visibility_accel =
        std::max(worst.mote_visibility_accel, m.mote_visibility_accel);
    worst.mote_visibility_jerk =
        std::max(worst.mote_visibility_jerk, m.mote_visibility_jerk);
    worst.surge_visibility_step =
        std::max(worst.surge_visibility_step, m.surge_visibility_step);
    worst.surge_visibility_accel =
        std::max(worst.surge_visibility_accel, m.surge_visibility_accel);
    worst.surge_visibility_jerk =
        std::max(worst.surge_visibility_jerk, m.surge_visibility_jerk);
    worst.edge_presence_step =
        std::max(worst.edge_presence_step, m.edge_presence_step);
    worst.edge_presence_accel =
        std::max(worst.edge_presence_accel, m.edge_presence_accel);
    worst.edge_presence_jerk =
        std::max(worst.edge_presence_jerk, m.edge_presence_jerk);
    worst.edge_energy_step =
        std::max(worst.edge_energy_step, m.edge_energy_step);
    worst.edge_energy_accel =
        std::max(worst.edge_energy_accel, m.edge_energy_accel);
    worst.edge_energy_jerk =
        std::max(worst.edge_energy_jerk, m.edge_energy_jerk);
    worst.edge_stamp_step = std::max(worst.edge_stamp_step, m.edge_stamp_step);
    worst.edge_stamp_accel = std::max(worst.edge_stamp_accel, m.edge_stamp_accel);
    worst.edge_stamp_jerk = std::max(worst.edge_stamp_jerk, m.edge_stamp_jerk);
    worst.edge_accum_step = std::max(worst.edge_accum_step, m.edge_accum_step);
    worst.edge_accum_accel = std::max(worst.edge_accum_accel, m.edge_accum_accel);
    worst.edge_accum_jerk = std::max(worst.edge_accum_jerk, m.edge_accum_jerk);
    worst.free_stamp_step = std::max(worst.free_stamp_step, m.free_stamp_step);
    worst.free_stamp_accel = std::max(worst.free_stamp_accel, m.free_stamp_accel);
    worst.free_stamp_jerk = std::max(worst.free_stamp_jerk, m.free_stamp_jerk);
    worst.free_energy_step = std::max(worst.free_energy_step, m.free_energy_step);
    worst.free_energy_accel = std::max(worst.free_energy_accel, m.free_energy_accel);
    worst.free_energy_jerk = std::max(worst.free_energy_jerk, m.free_energy_jerk);
    seam_step = std::max(seam_step, m.seam_step);
    held_tail_samples += m.held_tail_samples;
    seam_samples += m.seam_samples;
    blackouts += m.edge_blackouts;
    roles += m.role_changes;
    counts += m.mote_count_changes;
    surge_roles += m.surge_role_changes;
    surge_counts += m.surge_count_changes;
    chains += m.shape_chain_breaks;
    reversals += m.morph_reversals;
  }
  std::printf("G6 full-bank 60 Hz FX traces (including loop seams):\n"
              "   free lightning step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   fold stations  step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   fold particles step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   surge motes    step/accel/jerk %.2f / %.2f / %.2f mm\n"
              "   mote visibility fold %.0f/%.0f/%.0f pm; surge %.0f/%.0f/%.0f pm\n"
              "   edge presence  step/accel/jerk %.2f / %.2f / %.2f pm\n"
              "   per-stamp gain step/accel/jerk %.2f / %.2f / %.2f pm\n"
              "   fold stamps    step/accel/jerk %.2f / %.2f / %.2f\n"
              "   fold energy    step/accel/jerk %.2f / %.2f / %.2f pm\n"
              "   free stamps    step/accel/jerk %.2f / %.2f / %.2f\n"
              "   free energy    step/accel/jerk %.2f / %.2f / %.2f pm\n"
              "   seam max %.2f mm across %d entity samples; held-tail samples %d\n"
              "   blackouts %d fold role/count %d/%d surge role/count %d/%d "
              "shape-chain breaks %d morph reversals %d\n",
              worst.free_step, worst.free_accel, worst.free_jerk,
              worst.fold_step, worst.fold_accel, worst.fold_jerk,
              worst.mote_step, worst.mote_accel, worst.mote_jerk,
              worst.surge_step, worst.surge_accel, worst.surge_jerk,
              worst.mote_visibility_step, worst.mote_visibility_accel,
              worst.mote_visibility_jerk, worst.surge_visibility_step,
              worst.surge_visibility_accel, worst.surge_visibility_jerk,
              worst.edge_presence_step, worst.edge_presence_accel,
              worst.edge_presence_jerk, worst.edge_energy_step,
              worst.edge_energy_accel, worst.edge_energy_jerk,
              worst.edge_stamp_step, worst.edge_stamp_accel,
              worst.edge_stamp_jerk, worst.edge_accum_step,
              worst.edge_accum_accel, worst.edge_accum_jerk,
              worst.free_stamp_step, worst.free_stamp_accel,
              worst.free_stamp_jerk, worst.free_energy_step,
              worst.free_energy_accel, worst.free_energy_jerk,
              seam_step, seam_samples, held_tail_samples,
              blackouts, roles, counts,
              surge_roles, surge_counts, chains, reversals);
  std::printf("   maxima: free accel s%d/f%d/id%d jerk s%d/f%d/id%d; "
              "fold accel s%d/f%d/id%d jerk s%d/f%d/id%d\n"
              "           fold-mote accel s%d/f%d/id%d jerk s%d/f%d/id%d; "
              "surge accel s%d/f%d/id%d jerk s%d/f%d/id%d\n",
              fa_slot, fa_frame, fa_id, fj_slot, fj_frame, fj_id,
              xa_slot, xa_frame, xa_id, xj_slot, xj_frame, xj_id,
              ma_slot, ma_frame, ma_id, mj_slot, mj_frame, mj_id,
              sa_slot, sa_frame, sa_id, sj_slot, sj_frame, sj_id);
  if (blackouts != 0)
    fail(kFailBlackout, "a production figure has an alive edge-energy blackout");
  if (roles != 0)
    fail(kFailIdentity, "a production fold-mote ID changes role");
  if (counts != 0)
    fail(kFailIdentity, "the persistent production fold-mote domain changes count");
  if (surge_roles != 0)
    fail(kFailIdentity, "a production surge-mote ID changes role");
  if (surge_counts != 0)
    fail(kFailIdentity, "the persistent surge-mote domain changes count");
  if (chains != 0)
    fail(kFailShapeChain, "a production figure changes shape without old.to == new.from");
  if (reversals != 0)
    fail(kFailShapeChain, "a production shape morph reverses inside one transition");
  if (seam_samples == 0)
    fail(kFailCoverage, "no looping production entity was checked across last->first");
  if (held_tail_samples == 0)
    fail(kFailCoverage, "no hold-last production effect state was checked after the final sample");
  if (seam_step > kSeamStepMaxMm)
    fail(kFailSeam, "a production effect entity snaps across the loop seam");

  // Broad structural bands. Native every-frame review authors the final tighter
  // acceptance values; these distinguish interpolation from replacement now.
  if (worst.free_step > kFreeStepMaxMm)
    fail(kFailFreeMotion, "a free-lightning point teleports between presentation frames");
  if (worst.fold_step > kFoldStepMaxMm)
    fail(kFailFoldMotion, "a folded-lightning station teleports between presentation frames");
  if (worst.mote_step > kFoldMoteStepMaxMm)
    fail(kFailFoldMoteMotion, "a persistent fold mote teleports between presentation frames");
  if (worst.surge_step > kSurgeStepMaxMm)
    fail(kFailSurgeMotion, "a persistent surge mote teleports between presentation frames");
  if (worst.free_accel > kFreeAccelMaxMm ||
      worst.free_jerk > kFreeJerkMaxMm)
    fail(kFailFreeMotion,
         "free-lightning acceleration/jerk exceeds the reviewed structural band");
  if (worst.fold_accel > kFoldAccelMaxMm ||
      worst.fold_jerk > kFoldJerkMaxMm)
    fail(kFailFoldMotion,
         "fold-station acceleration/jerk exceeds the reviewed structural band");
  if (worst.mote_accel > kFoldMoteAccelMaxMm ||
      worst.mote_jerk > kFoldMoteJerkMaxMm)
    fail(kFailFoldMoteMotion,
         "fold-mote acceleration/jerk exceeds the reviewed structural band");
  if (worst.surge_accel > kSurgeAccelMaxMm ||
      worst.surge_jerk > kSurgeJerkMaxMm)
    fail(kFailSurgeMotion,
         "surge-mote acceleration/jerk exceeds the reviewed structural band");
  if (worst.mote_visibility_step > kMoteVisibilityStepMaxPm ||
      worst.mote_visibility_accel > kMoteVisibilityAccelMaxPm ||
      worst.mote_visibility_jerk > kMoteVisibilityJerkMaxPm ||
      worst.surge_visibility_step > kMoteVisibilityStepMaxPm ||
      worst.surge_visibility_accel > kMoteVisibilityAccelMaxPm ||
      worst.surge_visibility_jerk > kMoteVisibilityJerkMaxPm)
    fail(kFailMoteVisibility,
         "a production particle visibility turns off or reappears discontinuously");
  if (worst.edge_presence_step > kEdgePresenceStepMaxPm ||
      worst.edge_presence_accel > kEdgePresenceAccelMaxPm ||
      worst.edge_presence_jerk > kEdgePresenceJerkMaxPm)
    fail(kFailEdgePresence,
         "fold-edge presence snaps instead of following a continuous envelope");
  if (worst.edge_energy_step > kEdgeGainStepMaxPm ||
      worst.edge_energy_accel > kEdgeGainAccelMaxPm ||
      worst.edge_energy_jerk > kEdgeGainJerkMaxPm)
    fail(kFailEdgeBrightness,
         "fold-edge per-stamp brightness snaps between adjacent frames");
  if (worst.edge_stamp_step > kStampCountStepMax ||
      worst.edge_stamp_accel > kStampCountAccelMax ||
      worst.edge_stamp_jerk > kStampCountJerkMax ||
      worst.free_stamp_step > kStampCountStepMax ||
      worst.free_stamp_accel > kStampCountAccelMax ||
      worst.free_stamp_jerk > kStampCountJerkMax)
    fail(kFailStampCount,
         "a production lightning stamp population changes discontinuously");
  if (worst.edge_accum_step > kAccumEnergyStepMaxPm ||
      worst.edge_accum_accel > kAccumEnergyAccelMaxPm ||
      worst.edge_accum_jerk > kAccumEnergyJerkMaxPm ||
      worst.free_energy_step > kAccumEnergyStepMaxPm ||
      worst.free_energy_accel > kAccumEnergyAccelMaxPm ||
      worst.free_energy_jerk > kAccumEnergyJerkMaxPm)
    fail(kFailAccumEnergy,
         "production accumulated lightning energy changes discontinuously");

  bool fault_caught = true;
  const char* fault_name = "none";
  uint64_t expected_categories = 0;
  uint64_t allowed_categories = 0;
  switch (u02::g_u02_fx_continuity_fault) {
    case u02::FxContinuityFault::kNone: break;
    case u02::FxContinuityFault::kLightningSwitch:
      fault_name = "lightning-switch";
      expected_categories = kFailFreeMotion;
      // Replacing a free path also moves its attached surge motes, reaches the
      // lab's production strand, and breaks the loop seam. Those are causal;
      // identity/topology/brightness categories remain forbidden.
      allowed_categories = kFailFreeMotion | kFailSurgeMotion |
                           kFailLabMotion | kFailSeam;
      fault_caught = worst.free_step > kFreeStepMaxMm &&
                       worst.free_accel > kFreeAccelMaxMm &&
                       worst.free_jerk > kFreeJerkMaxMm;
      break;
    case u02::FxContinuityFault::kShapeBlackout:
      fault_name = "shape-blackout";
      expected_categories = kFailBlackout;
      allowed_categories = kFailBlackout | kFailEdgePresence |
                           kFailEdgeBrightness | kFailAccumEnergy;
      fault_caught = blackouts > 0;
      break;
    case u02::FxContinuityFault::kParticleReseed:
      fault_name = "fold-particle-reseed";
      expected_categories = kFailMoteMotion;
      allowed_categories = kFailMoteMotion | kFailFoldMoteMotion |
                           kFailLabMotion;
      fault_caught = mote_noise.step > 30.0 && mote_noise.accel > 45.0 &&
                       mote_noise.jerk > 60.0;
      break;
    case u02::FxContinuityFault::kSurgeReseed:
      fault_name = "surge-reseed";
      expected_categories = kFailSurgeMotion;
      allowed_categories = kFailSurgeMotion | kFailLabMotion | kFailSeam;
      fault_caught = worst.surge_step > kSurgeStepMaxMm &&
                       worst.surge_accel > kSurgeAccelMaxMm &&
                       worst.surge_jerk > kSurgeJerkMaxMm;
      break;
    case u02::FxContinuityFault::kLoopSeamSnap:
      fault_name = "loop-seam";
      expected_categories = kFailSeam;
      allowed_categories = kFailSeam | kFailFoldMotion | kFailManaBody;
      fault_caught = seam_step > kSeamStepMaxMm;
      break;
    case u02::FxContinuityFault::kMoteCountPop:
      fault_name = "mote-count";
      expected_categories = kFailIdentity;
      allowed_categories = kFailIdentity | kFailLabMotion |
                           kFailFoldMoteMotion | kFailMoteVisibility;
      fault_caught = counts > 0 && lab_metrics.count_changes > 0;
      break;
    case u02::FxContinuityFault::kMoteRolePop:
      fault_name = "mote-role";
      expected_categories = kFailIdentity;
      allowed_categories = kFailIdentity | kFailLabMotion | kFailSeam |
                           kFailFoldMoteMotion;
      fault_caught = roles > 0 && lab_metrics.role_changes > 0;
      break;
    case u02::FxContinuityFault::kWeightWrap:
      fault_name = "mvc-weight-wrap";
      expected_categories = kFailWeight;
      allowed_categories = kFailWeight | kFailFoldMotion;
      fault_caught = weight_faults > 0;
      break;
    case u02::FxContinuityFault::kMorphReverse:
      fault_name = "morph-reverse";
      expected_categories = kFailShapeChain | kFailFoldMotion;
      allowed_categories = expected_categories | kFailSeam |
                           kFailFoldMoteMotion | kFailEdgePresence |
                           kFailEdgeBrightness;
      fault_caught = reversals > 0 && worst.fold_accel > kFoldAccelMaxMm &&
                       worst.fold_jerk > kFoldJerkMaxMm;
      break;
    case u02::FxContinuityFault::kBrightnessSeam:
      fault_name = "brightness-seam";
      expected_categories = kFailEdgePresence | kFailEdgeBrightness;
      allowed_categories = expected_categories | kFailAccumEnergy;
      fault_caught =
          worst.edge_presence_step > kEdgePresenceStepMaxPm &&
          worst.edge_presence_accel > kEdgePresenceAccelMaxPm &&
          worst.edge_presence_jerk > kEdgePresenceJerkMaxPm &&
          worst.edge_energy_step > kEdgeGainStepMaxPm &&
          worst.edge_energy_accel > kEdgeGainAccelMaxPm &&
          worst.edge_energy_jerk > kEdgeGainJerkMaxPm;
      break;
    case u02::FxContinuityFault::kStampCountPop: {
      fault_name = "stamp-count-pop";
      expected_categories = kFailStampCount | kFailAccumEnergy | kFailLabEnergy;
      allowed_categories = expected_categories;
      const bool shipping_energy =
          worst.edge_accum_step > kAccumEnergyStepMaxPm ||
          worst.edge_accum_accel > kAccumEnergyAccelMaxPm ||
          worst.edge_accum_jerk > kAccumEnergyJerkMaxPm;
      const bool lab_count =
          lab_metrics.edge_stamp_step > kStampCountStepMax ||
          lab_metrics.edge_stamp_accel > kStampCountAccelMax ||
          lab_metrics.edge_stamp_jerk > kStampCountJerkMax;
      const bool lab_energy =
          lab_metrics.edge_energy_step > kLabAccumEnergyStepMaxPm ||
          lab_metrics.edge_energy_accel > kLabAccumEnergyAccelMaxPm ||
          lab_metrics.edge_energy_jerk > kLabAccumEnergyJerkMaxPm;
      fault_caught =
          worst.edge_stamp_step > kStampCountStepMax &&
          worst.edge_stamp_accel > kStampCountAccelMax &&
          worst.edge_stamp_jerk > kStampCountJerkMax &&
          shipping_energy && lab_count && lab_energy;
      break;
    }
    case u02::FxContinuityFault::kFinalDwell:
      fault_name = "final-dwell";
      expected_categories = kFailEdgeBrightness;
      allowed_categories = kFailEdgeBrightness | kFailEdgePresence |
                           kFailAccumEnergy;
      fault_caught = held_tail_samples > 0 &&
          worst.edge_energy_accel > kEdgeGainAccelMaxPm &&
          worst.edge_energy_jerk > kEdgeGainJerkMaxPm;
      break;
    case u02::FxContinuityFault::kMoteVisibilityPop:
      fault_name = "mote-visibility-pop";
      expected_categories = allowed_categories = kFailMoteVisibility;
      fault_caught =
          worst.mote_visibility_step > kMoteVisibilityStepMaxPm &&
          worst.mote_visibility_accel > kMoteVisibilityAccelMaxPm &&
          worst.mote_visibility_jerk > kMoteVisibilityJerkMaxPm &&
          worst.surge_visibility_step > kMoteVisibilityStepMaxPm &&
          worst.surge_visibility_accel > kMoteVisibilityAccelMaxPm &&
          worst.surge_visibility_jerk > kMoteVisibilityJerkMaxPm &&
          lab_metrics.mote_visibility_step > kMoteVisibilityStepMaxPm &&
          lab_metrics.mote_visibility_accel > kMoteVisibilityAccelMaxPm &&
          lab_metrics.mote_visibility_jerk > kMoteVisibilityJerkMaxPm;
      break;
    case u02::FxContinuityFault::kPaletteRawClock:
      fault_name = "palette-raw-clock";
      expected_categories = allowed_categories = kFailPalette;
      fault_caught =
          palette_metrics.seam_step > kPaletteEntryStepMax &&
          palette_metrics.energy_step > kPaletteEnergyStepMax &&
          palette_metrics.energy_accel > kPaletteEnergyAccelMax &&
          palette_metrics.energy_jerk > kPaletteEnergyJerkMax;
      break;
    case u02::FxContinuityFault::kPaletteHardSwitch:
      fault_name = "palette-hard-switch";
      expected_categories = allowed_categories = kFailPalette;
      fault_caught =
          palette_metrics.entry_accel > kPaletteEntryAccelMax &&
          palette_metrics.entry_jerk > kPaletteEntryJerkMax &&
          palette_metrics.energy_step > kPaletteEnergyStepMax &&
          palette_metrics.energy_accel > kPaletteEnergyAccelMax &&
          palette_metrics.energy_jerk > kPaletteEnergyJerkMax;
      break;
  }
  if (u02::g_u02_fx_continuity_fault != u02::FxContinuityFault::kNone) {
    const uint64_t missing = expected_categories & ~g_failure_categories;
    const uint64_t unrelated = g_failure_categories & ~allowed_categories;
    if (!fault_caught)
      fail(kFailAttribution,
           "the selected continuity mutant did not fire its named detector");
    if (missing != 0)
      fail(kFailAttribution,
           "the selected continuity mutant missed an expected failure category");
    if (unrelated != 0)
      fail(kFailAttribution,
           "the selected continuity mutant fired an unrelated failure category");
    std::printf("MUTANT %s: %s categories=0x%llx expected=0x%llx allowed=0x%llx\n",
                fault_name,
                fault_caught && missing == 0 && unrelated == 0
                    ? "attributed detector fired"
                    : "ATTRIBUTION FAILURE",
                static_cast<unsigned long long>(g_failure_categories),
                static_cast<unsigned long long>(expected_categories),
                static_cast<unsigned long long>(allowed_categories));
  }

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "PASS" : "FAIL", g_failures);
  return g_failures == 0 ? 0 : 1;
}
