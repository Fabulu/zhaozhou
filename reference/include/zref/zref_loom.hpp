// zref_loom.hpp -- `zref::TransformLoom`, the GEOM.LOOM oracle.
//
// Contract: design/contracts/GEOM.LOOM.md. RTL: fpga/rtl/geometry/zhao_geom_loom.sv.
//
// WHAT THIS MODEL OWNS, and the list is short because the owner's 2026-08-31
// ruling section 6.4 made it short: the compose law, the rounding, the refusal
// taxonomy and the bounded-store semantics.
//
// WHAT IT EXPLICITLY DOES NOT OWN, quoting the contract: "sorting, cycle
// detection, inversion, gait or formation evaluation -- per the ruling those
// belong to the compiler and to Form/Field, and an oracle that implemented them
// would be asserting a scope this block does not have."
//
// IT OWNS THE COMPOSE LAW BY CALLING IT, NOT BY RESTATING IT.
// `zref::creature::mat3x4_mul` is the one implementation of the 3x4 affine
// product and of its one-rescale-per-element rounding, and it is what
// `zhao_geom_mat3x4_mul` is already the RTL of. A second copy here would be the
// duplication CLAUDE.md's "read the SIBLING contract" chapter exists about --
// and worse than usual, because a differential test whose oracle is a second
// implementation of the law tests nothing when both copies drift together.
// Same for `zref::fx_sin` / `zref::fx_cos`, which are what `zhao_field_sin`
// implements.
//
// HEADER-ONLY ON PURPOSE. Everything here is a thin sequencer over those two
// laws; there is no state to compile into a translation unit, and keeping it
// out of `reference/CMakeLists.txt` keeps this packet off a file two other
// agents are editing.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

namespace zref {

// The v1 closed node-kind set, in the contract's own order. A kind that decodes
// identically to another is still a DISTINCT kind: it has its own histogram
// bucket, and that bucket is what the integration capture cases read.
enum class LoomKind : uint8_t {
  kRoot = 0,
  kRigid = 1,
  kScale = 2,
  kOrbit = 3,
  kAim = 4,
  kBillboard = 5,
  kOscillator = 6,
  kSpline = 7,
  kGaitOffset = 8,
  kFormationOffset = 9,
};

// The contract's overflow/malformed table, one entry each, in the RTL's
// encoding so a differential can compare the number and not a mapping.
enum class LoomRefusal : uint8_t {
  kNotSorted = 0,     // parent_index >= node_index, or node_index did not rise
  kParentUnset = 1,   // the parent was never emitted
  kOverflow = 2,      // more than MAX_NODES nodes, or an index past the bound
  kBadKind = 3,       // a kind outside the v1 set, or ORBIT with axis 3
  kScaleShear = 4,    // non-uniform SCALE on a body-patch-bound node
  kFraming = 5,       // a stream ended without `last`, or resumed mid-stream
  kNone = 6,
};

struct LoomNode {
  uint16_t node_index = 0;
  uint16_t parent_index = 0;
  LoomKind kind = LoomKind::kRoot;
  int32_t param[12] = {0};
  uint16_t angle = 0;      // angle16 turns; ORBIT only
  uint8_t axis = 0;        // 0=X 1=Y 2=Z; ORBIT only. 3 is BAD_KIND.
  bool body_patch = false;
  uint16_t src_id = 0;
  bool first = false;
  bool last = false;
};

struct LoomTransform {
  uint16_t node_index = 0;
  uint16_t src_id = 0;
  creature::mat3x4fx m{};
};

struct LoomCounters {
  uint32_t nodes_transformed = 0;
  uint32_t streams_composed = 0;
  uint32_t streams_refused[6] = {0, 0, 0, 0, 0, 0};
  uint16_t nodes_per_stream_max = 0;
  uint16_t chain_depth_max = 0;
  uint32_t node_kind_hist[10] = {0};
};

struct LoomResult {
  bool refused = false;
  LoomRefusal reason = LoomRefusal::kNone;
  uint16_t refused_node = 0;   // the contract: a refusal NAMES node and src
  uint16_t refused_src = 0;
  std::vector<LoomTransform> out;   // empty on refusal -- never a prefix
};

/**
 * The Transform Loom.
 *
 * ONE INSTANCE PER STREAM SEQUENCE. `compose()` runs one stream; the store is
 * cleared before each, which is the RTL's S_SCRUB, and `reset()` is the RTL's
 * reset -- both leave nothing a later stream can compose against. That is the
 * contract's central law ("a child composed against a stale parent is silently
 * wrong geometry rather than an error, which is the worst failure mode this
 * block can have") and it is enforced here by construction, not by a check.
 */
class TransformLoom {
 public:
  explicit TransformLoom(int max_nodes = 1024, int index_bits = 10)
      : max_nodes_(max_nodes), index_limit_(1 << index_bits) {
    reset();
  }

  void reset() {
    world_.assign(static_cast<size_t>(max_nodes_), creature::mat3x4_identity());
    depth_.assign(static_cast<size_t>(max_nodes_), 0);
    src_.assign(static_cast<size_t>(max_nodes_), 0);
    valid_.assign(static_cast<size_t>(max_nodes_), 0);
    order_.clear();
    in_stream_ = false;
    prev_index_ = 0;
  }

  const LoomCounters& counters() const { return c_; }

  /**
   * Compose one stream. The vector is the whole stream, first beat to last.
   *
   * A REFUSAL RETURNS AN EMPTY `out`, ALWAYS. The contract: "A refusal drops
   * the whole stream, not one node. A partially composed graph is meaningless."
   * The RTL gets this structurally by composing into its store and emitting on
   * a second walk; this model gets it by building the vector and discarding it,
   * which is the same guarantee expressed in the language that has vectors.
   *
   * `cam_basis` is the per-frame camera 3x3, row-major, for BILLBOARD. It is a
   * port on the RTL, not a lookup.
   */
  LoomResult compose(const std::vector<LoomNode>& stream, const int32_t cam_basis[9]) {
    LoomResult r;
    reset();

    std::vector<LoomTransform> pending;
    for (size_t i = 0; i < stream.size(); ++i) {
      LoomNode n = stream[i];
      // The wire carries IDXW bits, so an index wider than the port simply is
      // not representable on it. Masking here models the WIRE, not a policy --
      // the bound that REFUSES is max_nodes_, checked below.
      n.node_index = static_cast<uint16_t>(n.node_index & (index_limit_ - 1));
      n.parent_index = static_cast<uint16_t>(n.parent_index & (index_limit_ - 1));

      // ---- faults that are a property of the beat, in the RTL's priority ---
      const bool f_framing = (n.first && in_stream_) || (!n.first && !in_stream_);
      const bool f_overflow = (static_cast<int>(n.node_index) >= max_nodes_) ||
                              (!n.first && static_cast<int>(order_.size()) >= max_nodes_);
      const bool f_sorted = (!n.first && n.node_index <= prev_index_) ||
                            (n.kind != LoomKind::kRoot && n.parent_index >= n.node_index);
      const bool f_kind = (static_cast<uint8_t>(n.kind) > 9) ||
                          (n.kind == LoomKind::kOrbit && n.axis == 3);
      const bool f_scale = (n.kind == LoomKind::kScale) && n.body_patch &&
                           !(n.param[0] == n.param[1] && n.param[1] == n.param[2]);

      LoomRefusal why = LoomRefusal::kNone;
      if (f_framing) why = LoomRefusal::kFraming;
      else if (f_overflow) why = LoomRefusal::kOverflow;
      else if (f_sorted) why = LoomRefusal::kNotSorted;
      else if (f_kind) why = LoomRefusal::kBadKind;
      else if (f_scale) why = LoomRefusal::kScaleShear;

      // ---- the one fault that is not a property of the beat ---------------
      if (why == LoomRefusal::kNone && n.kind != LoomKind::kRoot &&
          !valid_[n.parent_index]) {
        why = LoomRefusal::kParentUnset;
      }

      if (why != LoomRefusal::kNone) {
        r.refused = true;
        r.reason = why;
        r.refused_node = n.node_index;
        r.refused_src = n.src_id;
        c_.streams_refused[static_cast<int>(why)]++;
        in_stream_ = false;
        return r;   // `out` stays empty: the WHOLE stream drops.
      }

      if (n.first) {
        in_stream_ = true;
        order_.clear();
      }
      prev_index_ = n.node_index;

      // ---- params -> local matrix -----------------------------------------
      creature::mat3x4fx local = local_matrix(n, cam_basis);

      // ---- the compose, through the one implementation of the law ----------
      const creature::mat3x4fx parent =
          (n.kind == LoomKind::kRoot) ? creature::mat3x4_identity() : world_[n.parent_index];
      creature::mat3x4fx w{};
      creature::mat3x4_mul(parent, local, w, nullptr);

      const uint16_t d =
          static_cast<uint16_t>((n.kind == LoomKind::kRoot) ? 1 : depth_[n.parent_index] + 1);

      world_[n.node_index] = w;
      depth_[n.node_index] = d;
      src_[n.node_index] = n.src_id;
      valid_[n.node_index] = 1;
      order_.push_back(n.node_index);

      c_.nodes_transformed++;
      c_.node_kind_hist[static_cast<int>(n.kind)]++;
      if (d > c_.chain_depth_max) c_.chain_depth_max = d;

      LoomTransform t;
      t.node_index = n.node_index;
      t.src_id = n.src_id;
      t.m = w;
      pending.push_back(t);

      if (n.last) {
        in_stream_ = false;
        if (order_.size() > c_.nodes_per_stream_max) {
          c_.nodes_per_stream_max = static_cast<uint16_t>(order_.size());
        }
        c_.streams_composed++;
        r.out = pending;
        return r;
      }
    }

    // Ran off the end without a `last`. The RTL cannot observe this until the
    // NEXT stream's `first` arrives, and refuses it as FRAMING then; here the
    // vector simply ends, so the model reports the same reason on the same
    // grounds -- the stream ended without `last`.
    r.refused = true;
    r.reason = LoomRefusal::kFraming;
    if (!stream.empty()) {
      r.refused_node = stream.back().node_index;
      r.refused_src = stream.back().src_id;
    }
    c_.streams_refused[static_cast<int>(LoomRefusal::kFraming)]++;
    in_stream_ = false;
    return r;
  }

  /** The kind decode on its own, for the directed test's per-kind section. */
  static creature::mat3x4fx local_matrix(const LoomNode& n, const int32_t cam_basis[9]) {
    creature::mat3x4fx L{};
    for (int i = 0; i < 12; ++i) L.m[i] = 0;
    const int32_t one = 1 << 16;

    switch (n.kind) {
      case LoomKind::kScale:
        L.m[0] = n.param[0];
        L.m[5] = n.param[1];
        L.m[10] = n.param[2];
        break;

      case LoomKind::kOrbit: {
        // An axis-aligned rotation's entries ARE sin and cos. No multiply, which
        // is why ORBIT costs a table read and not a DSP in the RTL.
        const int32_t s = fx_sin(angle16{n.angle}).raw;
        const int32_t c = fx_cos(angle16{n.angle}).raw;
        if (n.axis == 0) {
          L.m[0] = one;
          L.m[5] = c;   L.m[6] = -s;
          L.m[9] = s;   L.m[10] = c;
        } else if (n.axis == 1) {
          L.m[0] = c;   L.m[2] = s;
          L.m[5] = one;
          L.m[8] = -s;  L.m[10] = c;
        } else {
          L.m[0] = c;   L.m[1] = -s;
          L.m[4] = s;   L.m[5] = c;
          L.m[10] = one;
        }
        L.m[3] = n.param[0];
        L.m[7] = n.param[1];
        L.m[11] = n.param[2];
        break;
      }

      case LoomKind::kBillboard:
        L.m[0] = cam_basis[0];  L.m[1] = cam_basis[1];  L.m[2] = cam_basis[2];
        L.m[4] = cam_basis[3];  L.m[5] = cam_basis[4];  L.m[6] = cam_basis[5];
        L.m[8] = cam_basis[6];  L.m[9] = cam_basis[7];  L.m[10] = cam_basis[8];
        L.m[3] = n.param[0];
        L.m[7] = n.param[1];
        L.m[11] = n.param[2];
        break;

      case LoomKind::kOscillator:
      case LoomKind::kSpline:
        L.m[0] = one;
        L.m[5] = one;
        L.m[10] = one;
        L.m[3] = n.param[0];
        L.m[7] = n.param[1];
        L.m[11] = n.param[2];
        break;

      default:
        // ROOT, RIGID, AIM, GAIT_OFFSET, FORMATION_OFFSET.
        for (int i = 0; i < 12; ++i) L.m[i] = n.param[i];
        break;
    }
    return L;
  }

 private:
  int max_nodes_;
  int index_limit_;
  std::vector<creature::mat3x4fx> world_;
  std::vector<uint16_t> depth_;
  std::vector<uint16_t> src_;
  std::vector<uint8_t> valid_;
  std::vector<uint16_t> order_;
  bool in_stream_ = false;
  uint16_t prev_index_ = 0;
  LoomCounters c_{};
};

}  // namespace zref
