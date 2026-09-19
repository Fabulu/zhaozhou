// geom_loom_directed.cpp -- GEOM.LOOM against `zref::TransformLoom`.
//
// Contract: design/contracts/GEOM.LOOM.md, "Directed tests". RTL:
// fpga/rtl/geometry/zhao_geom_loom.sv.
//
// THE SECTIONS ARE THE CONTRACT'S OWN LIST, plus three the contract implies and
// this repo's laws demand:
//
//   1  every node kind in isolation against the oracle and, for the kinds with
//      a closed form, against a hand-written matrix
//   2  a two-level chain, then a ten-level chain
//   3  parent_index == node_index and parent_index > node_index: both refused
//   4  a parent that was never emitted: refused
//   5  a stream without `last`: refused; and a `first` inside a stream: refused
//   6  a non-uniform SCALE on a body-patch node: refused
//   7  THE DEEP-CHAIN DRIFT MEASUREMENT -- the number nobody has asked for
//   8  reset mid-stream, then resume: refused, not silently composed
//   9  the per-node clock cost, MEASURED off the RTL rather than asserted in a
//      header
//  10  every counter fired, by name
//
// WHAT SECTION 3 IS REALLY FOR. The precondition `parent_index < node_index` is
// the ruling's, and the failure it prevents is SILENT: a child composed against
// a stale parent is wrong geometry, not an error. So the assertion is not "the
// block does not crash" but "the block emits NOTHING and names the node".
//
// WHAT SECTION 8 IS REALLY FOR, and it is the same shape one level up. The
// contract says a stream "must be restarted from its root, never resumed
// mid-stream". A block that merely happened not to hold stale rows would pass a
// test that checked the output; this checks that resuming is REFUSED, which is
// the only version of the law that survives someone later adding a store that
// does persist.

#include "Vzhao_geom_loom.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_loom.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The RTL's parameterisation for this build. `test_geom_loom_small` overrides
// both to reach the OVERFLOW guard, which no legal stimulus reaches at the
// shipping numbers -- see the RTL header's reason table.
#ifndef ZHAO_LOOM_MAX_NODES
#define ZHAO_LOOM_MAX_NODES 1024
#endif
// Clocks from the accept edge to the output beat for a ONE-NODE stream.
// MEASURED off the RTL, not derived from the state list -- the first cut of
// this file predicted 47 from counting states and the block delivers 48. The
// header's frame-occupancy arithmetic is written from THIS number.
// MUL_LANES=1 is the default (37-cycle walk); the -GMUL_LANES=3 build passes
// its own value.
#ifndef ZHAO_LOOM_NODE_CLOCKS
#define ZHAO_LOOM_NODE_CLOCKS 48
#endif

namespace {

using zhao::check;
namespace zc = zref::creature;

const int32_t ONE = 1 << 16;

// The camera basis this suite uses for BILLBOARD. Deliberately NOT the identity
// -- an identity basis would let a block that ignored `cam_basis_i` entirely
// pass every billboard case.
const int32_t kCam[9] = {0, -ONE, 0, ONE, 0, 0, 0, 0, ONE};

struct Rtl {
  Vzhao_geom_loom dut;

  void boot() {
    dut.rst_n = 0;
    dut.in_valid_i = 0;
    dut.out_ready_i = 1;
    dut.in_first_i = 0;
    dut.in_last_i = 0;
    for (int i = 0; i < 9; ++i) dut.cam_basis_i[i] = static_cast<uint32_t>(kCam[i]);
    dut.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();
    // The scrub walks the whole store before anything is accepted; that is the
    // reset law, so waiting it out is the protocol, not a workaround.
    settle();
  }

  // Spin until the block is accepting again (in_ready_o after the scrub).
  void settle(int guard = 8 * ZHAO_LOOM_MAX_NODES + 512) {
    int n = 0;
    while (!dut.in_ready_o && n++ < guard) {
      zhao::tick(dut);
      dut.eval();
    }
  }
};

// ---------------------------------------------------------------------------
// COUNTERS SURVIVE THE TEST'S OWN RESETS BY BEING READ BEFORE THEM.
//
// Reset clears every counter in this block -- it has to, they live in the same
// reset tree as the FSM -- and this suite resets deliberately three times, to
// test the reset law and to start the cost measurement from a known state. So
// reading the counters at the end reads only the traffic since the last reset,
// which on the first cut of this file made nine of them read ZERO while the
// stimulus that should have moved them had already run.
//
// That is CLAUDE.md's broken-instrument law pointed at the test rather than at
// the design: the reading was precisely zero, it was believed, and it was the
// measurement that was wrong. The fix is to absorb the counters at every reset
// boundary, so the totals below are over the WHOLE suite.
// ---------------------------------------------------------------------------
struct CounterAcc {
  uint64_t nodes = 0;
  uint64_t streams = 0;
  uint64_t refused[6] = {0, 0, 0, 0, 0, 0};
  uint64_t stalls = 0;
  uint32_t nodes_max = 0;
  uint32_t depth_max = 0;
  uint64_t kind[10] = {0};

  void absorb(Rtl& r) {
    nodes += r.dut.nodes_transformed_o;
    streams += r.dut.streams_composed_o;
    stalls += r.dut.consumer_stall_cycles_o;
    if (r.dut.nodes_per_stream_max_o > nodes_max) nodes_max = r.dut.nodes_per_stream_max_o;
    if (r.dut.chain_depth_max_o > depth_max) depth_max = r.dut.chain_depth_max_o;
    for (int i = 0; i < 6; ++i) refused[i] += r.dut.streams_refused_o[i];
    for (int i = 0; i < 10; ++i) kind[i] += r.dut.node_kind_hist_o[i];
  }
};

// ---------------------------------------------------------------------------
// Drive one whole stream and collect everything that comes back.
// ---------------------------------------------------------------------------
struct Run {
  std::vector<zref::LoomTransform> out;
  std::vector<uint8_t> last_flags;
  bool refused = false;
  uint8_t reason = 0;
  uint16_t refused_node = 0;
  uint16_t refused_src = 0;
  int accept_to_first_out = -1;   // section 9's measurement
  bool timed_out = false;
};

/**
 * Present a stream beat by beat, collecting outputs and any refusal.
 *
 * `stall_every` inserts consumer backpressure: out_ready_i is dropped for one
 * clock every N output beats. Zero means never. It exists because a stalled
 * consumer is the case the emission walk's address hold was written for, and a
 * bug there reads the wrong store row -- which is silently wrong geometry, not
 * a hang.
 */
Run drive(Rtl& r, const std::vector<zref::LoomNode>& stream, int stall_every = 0) {
  Run run;
  auto& dut = r.dut;
  size_t beat = 0;
  int since_out = 0;
  int stall_hold = 0;   // clocks of backpressure still to apply
  int idle_clocks = 0;
  int accept_clock = -1;
  int clock = 0;
  const int guard = 220 * static_cast<int>(stream.size()) + 16 * ZHAO_LOOM_MAX_NODES + 4096;

  dut.out_ready_i = 1;
  while (clock < guard) {
    // ---- offer the next beat ------------------------------------------------
    if (beat < stream.size()) {
      const zref::LoomNode& n = stream[beat];
      dut.in_valid_i = 1;
      dut.in_node_index_i = n.node_index;
      dut.in_parent_index_i = n.parent_index;
      dut.in_kind_i = static_cast<uint8_t>(n.kind);
      for (int i = 0; i < 12; ++i) dut.in_param_i[i] = static_cast<uint32_t>(n.param[i]);
      dut.in_angle_i = n.angle;
      dut.in_axis_i = n.axis;
      dut.in_bodypatch_i = n.body_patch ? 1 : 0;
      dut.in_src_id_i = n.src_id;
      dut.in_first_i = n.first ? 1 : 0;
      dut.in_last_i = n.last ? 1 : 0;
    } else {
      dut.in_valid_i = 0;
    }

    // Consumer backpressure, applied BEFORE eval so the block sees it on the
    // same edge the output would otherwise be taken.
    // TRANSIENT, NOT LATCHED. The first cut of this held out_ready_i low from
    // the moment `since_out % stall_every == 0` and never released it, because
    // `since_out` cannot advance while the consumer is not taking anything --
    // a deadlock in the STIMULUS that reads exactly like the block hanging.
    dut.out_ready_i = (stall_hold > 0) ? 0 : 1;
    dut.eval();

    const bool accepted = dut.in_valid_i && dut.in_ready_o;
    const bool emitted = dut.out_valid_o && dut.out_ready_i;

    if (emitted) {
      zref::LoomTransform t;
      t.node_index = static_cast<uint16_t>(dut.out_node_index_o);
      t.src_id = static_cast<uint16_t>(dut.out_src_id_o);
      for (int i = 0; i < 12; ++i) t.m.m[i] = static_cast<int32_t>(dut.out_m_o[i]);
      run.out.push_back(t);
      run.last_flags.push_back(static_cast<uint8_t>(dut.out_last_o));
      if (run.accept_to_first_out < 0 && accept_clock >= 0) {
        run.accept_to_first_out = clock - accept_clock;
      }
      ++since_out;
      // SIX clocks, not three, and the number is load-bearing. The emission
      // walk is order-RAM -> store-RAM -> output register, so a hold shorter
      // than that walk is released before the block ever reaches the state
      // where it could stall -- consumer_stall_cycles then reads zero while
      // the stimulus looks like backpressure. It did, on the first cut.
      if (stall_every && (since_out % stall_every == 0)) stall_hold = 6;
      idle_clocks = 0;
    }

    if (dut.refuse_valid_o) {
      run.refused = true;
      run.reason = static_cast<uint8_t>(dut.refuse_reason_o);
      run.refused_node = static_cast<uint16_t>(dut.refuse_node_index_o);
      run.refused_src = static_cast<uint16_t>(dut.refuse_src_id_o);
      idle_clocks = 0;
    }

    if (accepted) {
      if (accept_clock < 0) accept_clock = clock;
      ++beat;
      idle_clocks = 0;
    } else if (beat >= stream.size()) {
      ++idle_clocks;
    }

    zhao::tick(dut);
    dut.eval();
    ++clock;
    if (stall_hold > 0) --stall_hold;

    // Done when every beat has gone in and the block has been quiet long enough
    // for the emission walk plus the scrub to have finished.
    if (beat >= stream.size() && idle_clocks > (4 * ZHAO_LOOM_MAX_NODES + 4096)) break;
    if (run.refused && beat >= stream.size() && idle_clocks > 64) break;
    if (!run.out.empty() && !run.last_flags.empty() && run.last_flags.back() &&
        beat >= stream.size()) {
      // The `last` beat has been taken; give the scrub a moment and stop.
      if (idle_clocks > 4) break;
    }
  }
  dut.in_valid_i = 0;
  dut.eval();
  if (clock >= guard) run.timed_out = true;
  r.settle();
  return run;
}

// ---------------------------------------------------------------------------
// Differential: the RTL run against `zref::TransformLoom` on the same stream.
// ---------------------------------------------------------------------------
void diff(Rtl& r, const std::vector<zref::LoomNode>& stream, const char* what,
          int stall_every = 0) {
  zref::TransformLoom ref(ZHAO_LOOM_MAX_NODES, 10);
  const zref::LoomResult want = ref.compose(stream, kCam);
  const Run got = drive(r, stream, stall_every);

  const std::string t(what);
  char lbl[200];

  std::snprintf(lbl, sizeof lbl, "%s: no timeout", t.c_str());
  check(!got.timed_out, lbl, 0, got.timed_out ? 1 : 0);

  std::snprintf(lbl, sizeof lbl, "%s: refused?", t.c_str());
  check(got.refused == want.refused, lbl, want.refused ? 1 : 0, got.refused ? 1 : 0);

  if (want.refused) {
    std::snprintf(lbl, sizeof lbl, "%s: refusal reason", t.c_str());
    check(got.reason == static_cast<uint8_t>(want.reason), lbl,
          static_cast<uint8_t>(want.reason), got.reason);
    std::snprintf(lbl, sizeof lbl, "%s: refusal names the node", t.c_str());
    check(got.refused_node == want.refused_node, lbl, want.refused_node, got.refused_node);
    std::snprintf(lbl, sizeof lbl, "%s: refusal names the src_id", t.c_str());
    check(got.refused_src == want.refused_src, lbl, want.refused_src, got.refused_src);
    // THE POINT OF THE WHOLE SECTION: a refusal drops the WHOLE stream.
    std::snprintf(lbl, sizeof lbl, "%s: nothing emitted (no partial stream)", t.c_str());
    check(got.out.empty(), lbl, 0, static_cast<uint64_t>(got.out.size()));
    return;
  }

  std::snprintf(lbl, sizeof lbl, "%s: node count", t.c_str());
  check(got.out.size() == want.out.size(), lbl, static_cast<uint64_t>(want.out.size()),
        static_cast<uint64_t>(got.out.size()));
  if (got.out.size() != want.out.size()) return;

  for (size_t k = 0; k < want.out.size(); ++k) {
    std::snprintf(lbl, sizeof lbl, "%s: [%zu] node_index", t.c_str(), k);
    check(got.out[k].node_index == want.out[k].node_index, lbl, want.out[k].node_index,
          got.out[k].node_index);
    std::snprintf(lbl, sizeof lbl, "%s: [%zu] src_id", t.c_str(), k);
    check(got.out[k].src_id == want.out[k].src_id, lbl, want.out[k].src_id, got.out[k].src_id);
    for (int e = 0; e < 12; ++e) {
      std::snprintf(lbl, sizeof lbl, "%s: [%zu] m[%d]", t.c_str(), k, e);
      check(got.out[k].m.m[e] == want.out[k].m.m[e], lbl,
            static_cast<uint32_t>(want.out[k].m.m[e]), static_cast<uint32_t>(got.out[k].m.m[e]));
    }
  }
  // `last` marks exactly the final beat, and nothing else.
  for (size_t k = 0; k < got.last_flags.size(); ++k) {
    const bool expect = (k + 1 == got.last_flags.size());
    std::snprintf(lbl, sizeof lbl, "%s: [%zu] last flag", t.c_str(), k);
    check((got.last_flags[k] != 0) == expect, lbl, expect ? 1 : 0, got.last_flags[k]);
  }
}

zref::LoomNode node(uint16_t idx, uint16_t parent, zref::LoomKind k) {
  zref::LoomNode n;
  n.node_index = idx;
  n.parent_index = parent;
  n.kind = k;
  n.src_id = static_cast<uint16_t>(0x5A00u + idx);
  return n;
}

void set12(zref::LoomNode& n, const int32_t v[12]) {
  for (int i = 0; i < 12; ++i) n.param[i] = v[i];
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats 7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Rtl r;
  CounterAcc acc;
  r.boot();

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  // =========================================================================
  // RANDOMIZED DIFFERENTIAL -- contract "Randomized differential tests"
  // =========================================================================
  // The generator REPORTS ITS REFUSAL MIX, as the contract requires of this
  // block and of GEOM.MESHFETCH and GEOM.VDECODE: "so coverage loss shows as a
  // shift rather than as silence".
  //
  // Generation is biased toward DEEP NARROW CHAINS and WIDE SHALLOW FANS
  // SEPARATELY, also the contract's instruction -- "they stress the parent
  // store's residency in opposite ways, and a uniform random graph produces
  // neither".
  if (random_mode) {
    Prng rng(0x10E3u);
    uint32_t mix[7] = {0, 0, 0, 0, 0, 0, 0};
    uint32_t deep = 0, fan = 0;
    uint32_t malformed = 0;

    for (uint32_t k = 0; k < iters && zhao::check_failures() == 0; ++k) {
      const bool deep_chain = (k & 1) == 0;
      const int n_nodes = 2 + static_cast<int>(rng.next() % 14u);
      std::vector<zref::LoomNode> s;
      for (int i = 0; i < n_nodes; ++i) {
        const uint16_t idx = static_cast<uint16_t>(i);
        uint16_t par = 0;
        if (i > 0) par = deep_chain ? static_cast<uint16_t>(i - 1)
                                    : static_cast<uint16_t>(rng.next() % static_cast<uint32_t>(i));
        const zref::LoomKind kind =
            (i == 0) ? zref::LoomKind::kRoot
                     : static_cast<zref::LoomKind>(1u + (rng.next() % 9u));
        zref::LoomNode n = node(idx, par, kind);
        for (int e = 0; e < 12; ++e) {
          // Pose-plausible magnitudes: near-unit 3x3, translation carrying the
          // range. Sampling the whole s32 word would put nearly every iteration
          // on the multiplier's saturation rail instead of on the arithmetic.
          n.param[e] = static_cast<int32_t>(rng.next()) >> ((e % 4 == 3) ? 10 : 15);
        }
        n.angle = static_cast<uint16_t>(rng.next());
        n.axis = static_cast<uint8_t>(rng.next() % 3u);
        n.first = (i == 0);
        n.last = (i + 1 == n_nodes);
        s.push_back(n);
      }
      if (deep_chain) ++deep; else ++fan;

      // A DELIBERATE MALFORMED FRACTION. The fault kind is ROUND ROBIN rather
      // than drawn from the same roll that decides whether to inject one: the
      // first cut derived both from one draw, and over sixty streams that gave
      // four BAD_KIND, one NOT_SORTED and nothing else -- three reasons silent,
      // which is coverage loss that looks exactly like a clean run. The
      // contract asks this generator to REPORT ITS REFUSAL MIX "so coverage
      // loss shows as a shift rather than as silence"; a mix that cannot reach
      // a reason reports silence in the flattering direction.
      const uint32_t roll = rng.next() % 100u;
      if (roll < 40u && n_nodes >= 3) {
        switch ((malformed++) % 6u) {
          case 0: s[1].parent_index = s[1].node_index; break;            // NOT_SORTED
          case 1: s[2].node_index = s[1].node_index; break;              // NOT_SORTED
          case 2: s[1].kind = static_cast<zref::LoomKind>(12); break;    // BAD_KIND
          case 3: s[1].kind = zref::LoomKind::kOrbit; s[1].axis = 3; break;  // BAD_KIND
          case 4: s[0].first = false; break;                             // FRAMING
          default:                                                       // SCALE_SHEAR
            s[1].kind = zref::LoomKind::kScale;
            s[1].body_patch = true;
            s[1].param[0] = ONE; s[1].param[1] = 2 * ONE; s[1].param[2] = ONE;
            break;
        }
      } else if (roll < 48u && n_nodes >= 3) {
        // PARENT_UNSET on its own, because it is the only reason that is not a
        // property of the beat and the only one the store has to answer.
        s[n_nodes - 1].node_index = 500;
        s[n_nodes - 1].parent_index = 400;   // legal (400 < 500) and never emitted
      }

      zref::TransformLoom probe(ZHAO_LOOM_MAX_NODES, 10);
      const zref::LoomResult w = probe.compose(s, kCam);
      mix[static_cast<int>(w.reason)]++;

      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u]%s", k, deep_chain ? " deep" : " fan");
      diff(r, s, tag, (k % 3 == 0) ? 2 : 0);
    }

    std::printf("[loom] refusal mix over %u streams: sorted=%u parent=%u overflow=%u "
                "kind=%u shear=%u framing=%u clean=%u  (deep=%u fan=%u)\n",
                iters, mix[0], mix[1], mix[2], mix[3], mix[4], mix[5], mix[6], deep, fan);
    // AND THE MIX IS GATED, not merely printed. A generator that quietly stops
    // reaching a refusal reason is the failure the contract's own sentence is
    // about, and a printed line nobody reads does not catch it. OVERFLOW (2) is
    // excluded because it is structurally unreachable at these parameters --
    // `test_geom_loom_small` owns it.
    if (iters >= 200u) {
      const char* rn[6] = {"NOT_SORTED", "PARENT_UNSET", "OVERFLOW", "BAD_KIND",
                           "SCALE_SHEAR", "FRAMING"};
      for (int i = 0; i < 6; ++i) {
        if (i == 2) continue;
        char lbl[112];
        std::snprintf(lbl, sizeof lbl, "generator still reaches refusal %s", rn[i]);
        check(mix[i] > 0, lbl, 1, mix[i]);
      }
      check(mix[6] > 0, "generator still produces clean streams", 1, mix[6]);
    }
    r.dut.final();
    return zhao::report_and_exit("geom_loom_random");
  }

  // =========================================================================
  // 1. EVERY NODE KIND IN ISOLATION
  // =========================================================================
  // Each is a two-node stream: a ROOT carrying a NON-IDENTITY parent, then the
  // kind under test. A ROOT-only test would let a block that dropped the parent
  // entirely pass every one of them.
  {
    const int32_t rootm[12] = {0, -ONE, 0, 3 * ONE,
                               ONE, 0, 0, -5 * ONE,
                               0, 0, ONE, 7 * ONE};
    const int32_t childm[12] = {2 * ONE, 0, 0, ONE / 2,
                                0, ONE, ONE / 4, -ONE,
                                0, 0, ONE, 2 * ONE};

    for (int k = 0; k <= 9; ++k) {
      zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
      set12(root, rootm);
      root.first = true;

      zref::LoomNode child = node(1, 0, static_cast<zref::LoomKind>(k));
      set12(child, childm);
      child.angle = 0x2000;   // an eighth turn, so sin and cos are both nonzero
      child.axis = static_cast<uint8_t>(k % 3);
      child.last = true;
      if (child.kind == zref::LoomKind::kScale) {
        child.param[0] = 2 * ONE;
        child.param[1] = 3 * ONE;
        child.param[2] = ONE / 2;
      }

      char tag[64];
      std::snprintf(tag, sizeof tag, "kind %d under a rotated parent", k);
      diff(r, {root, child}, tag);
    }
  }

  // ---- 1b. ROOT IS EXACTLY ITS PARAMETERS ---------------------------------
  // The identity compose is not an approximation of a bypass; it is the same
  // twelve numbers. If this ever fails, the rounding law moved.
  {
    const int32_t m[12] = {ONE, -12345, 77, 4 * ONE,
                           99, ONE, -3, -9 * ONE,
                           5, 6, ONE, 123456};
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    set12(root, m);
    root.first = true;
    root.last = true;
    const Run got = drive(r, {root});
    check(got.out.size() == 1, "ROOT: one node out", 1, static_cast<uint64_t>(got.out.size()));
    if (got.out.size() == 1) {
      for (int e = 0; e < 12; ++e) {
        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "ROOT is exactly its params: m[%d]", e);
        check(got.out[0].m.m[e] == m[e], lbl, static_cast<uint32_t>(m[e]),
              static_cast<uint32_t>(got.out[0].m.m[e]));
      }
    }
  }

  // ---- 1c. BILLBOARD ACTUALLY READS cam_basis_i ---------------------------
  // A billboard whose 3x3 came from `param` instead of the port would pass
  // section 1 (the oracle would be wrong in the same way). This one changes the
  // port between two otherwise identical streams and requires the answer to
  // move.
  {
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;
    zref::LoomNode bb = node(1, 0, zref::LoomKind::kBillboard);
    bb.last = true;

    const Run a = drive(r, {root, bb});
    for (int i = 0; i < 9; ++i) r.dut.cam_basis_i[i] = static_cast<uint32_t>(i == 0 || i == 4 || i == 8 ? ONE : 0);
    r.dut.eval();
    const Run b = drive(r, {root, bb});
    for (int i = 0; i < 9; ++i) r.dut.cam_basis_i[i] = static_cast<uint32_t>(kCam[i]);
    r.dut.eval();

    bool moved = false;
    if (a.out.size() == 2 && b.out.size() == 2) {
      for (int e = 0; e < 12; ++e) if (a.out[1].m.m[e] != b.out[1].m.m[e]) moved = true;
    }
    check(moved, "BILLBOARD reads cam_basis_i (answer moves when the port does)", 1,
          moved ? 1 : 0);
  }

  // =========================================================================
  // 2. CHAINS
  // =========================================================================
  {
    std::vector<zref::LoomNode> s;
    for (int i = 0; i < 2; ++i) {
      zref::LoomNode n = node(static_cast<uint16_t>(i), static_cast<uint16_t>(i ? i - 1 : 0),
                              i ? zref::LoomKind::kRigid : zref::LoomKind::kRoot);
      n.param[0] = ONE; n.param[5] = ONE; n.param[10] = ONE;
      n.param[3] = ONE * (i + 1);
      n.param[7] = -ONE * (i + 2);
      n.param[11] = ONE / (i + 3);
      n.first = (i == 0);
      n.last = (i == 1);
      s.push_back(n);
    }
    diff(r, s, "two-level chain");
  }
  {
    std::vector<zref::LoomNode> s;
    for (int i = 0; i < 10; ++i) {
      zref::LoomNode n = node(static_cast<uint16_t>(i), static_cast<uint16_t>(i ? i - 1 : 0),
                              i ? zref::LoomKind::kOrbit : zref::LoomKind::kRoot);
      n.param[0] = ONE; n.param[5] = ONE; n.param[10] = ONE;
      n.angle = static_cast<uint16_t>(0x0800 * (i + 1));
      n.axis = static_cast<uint8_t>(i % 3);
      if (i) { n.param[0] = ONE / 2; n.param[1] = 0; n.param[2] = ONE / 4; }
      n.first = (i == 0);
      n.last = (i == 9);
      s.push_back(n);
    }
    diff(r, s, "ten-level ORBIT chain");
    diff(r, s, "ten-level ORBIT chain, consumer stalling", 2);
  }

  // =========================================================================
  // 3. THE TOPOLOGICAL PRECONDITION, AT ITS BOUNDARY
  // =========================================================================
  {
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;

    zref::LoomNode eq = node(4, 4, zref::LoomKind::kRigid);   // parent == node
    eq.param[0] = ONE; eq.param[5] = ONE; eq.param[10] = ONE;
    eq.last = true;
    diff(r, {root, eq}, "parent_index == node_index");

    zref::LoomNode gt = node(4, 7, zref::LoomKind::kRigid);   // parent > node
    gt.param[0] = ONE; gt.param[5] = ONE; gt.param[10] = ONE;
    gt.last = true;
    diff(r, {root, gt}, "parent_index > node_index");

    // And the other half of NOT_SORTED: an index that did not rise. This is
    // what makes a duplicate row impossible, so the emission order IS the
    // arrival order.
    zref::LoomNode a = node(5, 0, zref::LoomKind::kRigid);
    a.param[0] = ONE; a.param[5] = ONE; a.param[10] = ONE;
    zref::LoomNode b = node(5, 0, zref::LoomKind::kRigid);
    b.param[0] = ONE; b.param[5] = ONE; b.param[10] = ONE;
    b.last = true;
    diff(r, {root, a, b}, "node_index repeated");
  }

  // =========================================================================
  // 4. A PARENT THAT WAS NEVER EMITTED
  // =========================================================================
  {
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;
    zref::LoomNode orphan = node(9, 4, zref::LoomKind::kRigid);   // 4 never appeared
    orphan.param[0] = ONE; orphan.param[5] = ONE; orphan.param[10] = ONE;
    orphan.last = true;
    diff(r, {root, orphan}, "parent never emitted");
  }

  // =========================================================================
  // 5. FRAMING
  // =========================================================================
  {
    // A `first` beat inside a stream: the previous stream ended without `last`.
    zref::LoomNode a = node(0, 0, zref::LoomKind::kRoot);
    a.param[0] = ONE; a.param[5] = ONE; a.param[10] = ONE;
    a.first = true;
    zref::LoomNode b = node(1, 0, zref::LoomKind::kRoot);
    b.param[0] = ONE; b.param[5] = ONE; b.param[10] = ONE;
    b.first = true;
    b.last = true;
    diff(r, {a, b}, "a second `first` inside a stream");

    // A beat without `first` while idle: a stream resumed mid-way.
    zref::LoomNode c = node(3, 0, zref::LoomKind::kRoot);
    c.param[0] = ONE; c.param[5] = ONE; c.param[10] = ONE;
    c.last = true;
    diff(r, {c}, "a beat without `first` while idle");

    // A stream that simply stops: nothing is emitted, ever. The RTL cannot
    // report this until the next `first`, so the assertion here is the one that
    // matters -- NO OUTPUT -- rather than a refusal code.
    zref::LoomNode d = node(0, 0, zref::LoomKind::kRoot);
    d.param[0] = ONE; d.param[5] = ONE; d.param[10] = ONE;
    d.first = true;
    const Run got = drive(r, {d});
    check(got.out.empty(), "stream without `last` emits nothing", 0,
          static_cast<uint64_t>(got.out.size()));
    // And the block must not be wedged: the next well-formed stream still works.
    acc.absorb(r);
    r.boot();
  }

  // =========================================================================
  // 6. NON-UNIFORM SCALE ON A BODY-PATCH NODE
  // =========================================================================
  {
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;

    zref::LoomNode shear = node(1, 0, zref::LoomKind::kScale);
    shear.body_patch = true;
    shear.param[0] = ONE; shear.param[1] = 2 * ONE; shear.param[2] = ONE;
    shear.last = true;
    diff(r, {root, shear}, "non-uniform SCALE on a body-patch node");

    // The SAME shear WITHOUT the body-patch flag is legal -- the rule is the
    // binding's, not scale's, and a block that refused both would be wrong in
    // the direction nobody notices.
    zref::LoomNode ok = shear;
    ok.body_patch = false;
    diff(r, {root, ok}, "non-uniform SCALE without the body-patch binding");

    // And a UNIFORM scale on a body-patch node is legal.
    zref::LoomNode uni = shear;
    uni.param[1] = ONE; uni.param[2] = ONE;
    diff(r, {root, uni}, "uniform SCALE on a body-patch node");
  }

  // =========================================================================
  // 6b. AN UNKNOWN NODE KIND -- "a newer graph read by an older block"
  // =========================================================================
  {
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;

    zref::LoomNode alien = node(1, 0, static_cast<zref::LoomKind>(12));
    alien.param[0] = ONE; alien.param[5] = ONE; alien.param[10] = ONE;
    alien.last = true;
    diff(r, {root, alien}, "node_kind 12 (outside the v1 set)");

    // The boundary: 9 is the last legal kind, 10 is the first illegal one, and
    // an off-by-one in the comparison is invisible without both.
    zref::LoomNode edge = node(1, 0, zref::LoomKind::kFormationOffset);
    edge.param[0] = ONE; edge.param[5] = ONE; edge.param[10] = ONE;
    edge.last = true;
    diff(r, {root, edge}, "node_kind 9 is the last legal kind");
    zref::LoomNode over = node(1, 0, static_cast<zref::LoomKind>(10));
    over.param[0] = ONE; over.param[5] = ONE; over.param[10] = ONE;
    over.last = true;
    diff(r, {root, over}, "node_kind 10 is the first illegal kind");

    // ORBIT's AXIS IS PART OF ITS OPCODE. Axis 3 is not a rotation this block
    // has, so it is BAD_KIND -- not quietly folded onto Z, which is what an
    // unguarded 2-bit selector with three arms does.
    zref::LoomNode ax = node(1, 0, zref::LoomKind::kOrbit);
    ax.angle = 0x1234;
    ax.axis = 3;
    ax.last = true;
    diff(r, {root, ax}, "ORBIT with axis 3");
    for (uint8_t a3 = 0; a3 < 3; ++a3) {
      zref::LoomNode ok = ax;
      ok.axis = a3;
      char tag[64];
      std::snprintf(tag, sizeof tag, "ORBIT with axis %u is legal", a3);
      diff(r, {root, ok}, tag);
    }
  }

  // =========================================================================
  // 7. THE DEEP-CHAIN DRIFT MEASUREMENT
  // =========================================================================
  // The contract: "a 1,024-deep chain is legal and nobody has yet asked what the
  // drift looks like ... This test's job is to produce a NUMBER for a question
  // nobody has asked yet, not to assert a bound invented here."
  //
  // THE REFERENCE IS ANALYTIC, NOT A SECOND FIXED-POINT CHAIN. Composing N
  // rotations of angle theta about Z is a rotation of N*theta, exactly. So the
  // truth is known in closed form and the measured error is the REAL error --
  // quantisation of fx_sin plus accumulation through the composes -- rather
  // than the difference between two approximations that might drift together.
  //
  // What it therefore measures, and the distinction is the whole value of the
  // number: the fx16 quantum is 1/65536, so an error of E LSBs is E*1.53e-5 of
  // unit scale. A rotation matrix element off by 300 LSBs is off by 0.46 % --
  // which is visible on a limb. Nobody has ruled on that; this prints it.
  {
    const int depth = (ZHAO_LOOM_MAX_NODES < 1024) ? ZHAO_LOOM_MAX_NODES : 1024;
    const uint16_t step = 41;   // a small angle, so the chain sweeps many turns
    std::vector<zref::LoomNode> s;
    for (int i = 0; i < depth; ++i) {
      zref::LoomNode n = node(static_cast<uint16_t>(i), static_cast<uint16_t>(i ? i - 1 : 0),
                              i ? zref::LoomKind::kOrbit : zref::LoomKind::kRoot);
      if (i == 0) { n.param[0] = ONE; n.param[5] = ONE; n.param[10] = ONE; }
      n.angle = step;
      n.axis = 2;   // Z
      n.first = (i == 0);
      n.last = (i + 1 == depth);
      s.push_back(n);
    }

    diff(r, s, "maximal chain, RTL == oracle");

    const Run got = drive(r, s);
    double worst_rot = 0.0;
    int worst_at = -1;
    if (static_cast<int>(got.out.size()) == depth) {
      for (int i = 1; i < depth; ++i) {
        const double ang = 2.0 * 3.14159265358979323846 *
                           (static_cast<double>(step) * static_cast<double>(i)) / 65536.0;
        const double c = std::cos(ang), si = std::sin(ang);
        const double want[4] = {c, -si, si, c};
        const int at[4] = {0, 1, 4, 5};
        for (int e = 0; e < 4; ++e) {
          const double err =
              std::fabs(static_cast<double>(got.out[i].m.m[at[e]]) / 65536.0 - want[e]) * 65536.0;
          if (err > worst_rot) { worst_rot = err; worst_at = i; }
        }
      }
      std::printf("[loom] DRIFT: %d-deep Z-rotation chain, worst rotation element error "
                  "%.2f LSB (%.5f %% of unit) at depth %d\n",
                  depth, worst_rot, worst_rot * 100.0 / 65536.0, worst_at);

      // AND THE SHAPE OF IT, which is the part that is actually informative.
      // A random-walk of independent roundings would grow as sqrt(depth); a
      // BIASED rounding grows LINEARLY. `rescale_sat16` is round-half-UP, which
      // is biased, so the prediction is linear -- and this table is how a reader
      // checks that rather than taking the sentence's word for it. If a later
      // change makes the growth sub-linear, somebody changed the rounding law.
      std::printf("[loom] DRIFT by depth (LSB):");
      for (int d = 16; d < depth; d *= 4) {
        double e = 0.0;
        const double ang = 2.0 * 3.14159265358979323846 *
                           (static_cast<double>(step) * static_cast<double>(d)) / 65536.0;
        const double c = std::cos(ang), si = std::sin(ang);
        const double want[4] = {c, -si, si, c};
        const int at[4] = {0, 1, 4, 5};
        for (int e2 = 0; e2 < 4; ++e2) {
          const double err =
              std::fabs(static_cast<double>(got.out[d].m.m[at[e2]]) / 65536.0 - want[e2]) * 65536.0;
          if (err > e) e = err;
        }
        std::printf("  d=%d:%.1f", d, e);
      }
      std::printf("  d=%d:%.1f\n", depth - 1, worst_rot);
    } else {
      std::printf("[loom] DRIFT: chain did not compose (%zu of %d nodes out)\n",
                  got.out.size(), depth);
    }
    check(static_cast<int>(got.out.size()) == depth, "maximal chain composes every node",
          static_cast<uint64_t>(depth), static_cast<uint64_t>(got.out.size()));
    // chain_depth_max must have seen the whole chain. A counter that reads the
    // number the test just put in is the only way this one is evidence.
    check(r.dut.chain_depth_max_o >= static_cast<uint32_t>(depth),
          "chain_depth_max saw the maximal chain", static_cast<uint64_t>(depth),
          r.dut.chain_depth_max_o);
  }

  // =========================================================================
  // 8. RESET MID-STREAM, THEN RESUME
  // =========================================================================
  // The contract's worst failure mode. The assertion is NOT "no output" -- a
  // block that quietly composed against a surviving row would also emit
  // something -- it is that resuming is REFUSED and named.
  {
    acc.absorb(r);
    r.boot();
    zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
    root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
    root.first = true;

    // Compose the root of a stream, then pull reset while the stream is live.
    std::vector<zref::LoomNode> head = {root};
    drive(r, head);      // no `last`, so nothing is emitted and the store holds row 0

    acc.absorb(r);
    r.dut.rst_n = 0;
    r.dut.in_valid_i = 0;
    r.dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(r.dut);
    r.dut.rst_n = 1;
    r.dut.eval();
    r.settle();

    // Resume: a child of row 0, WITHOUT `first`. Two laws must both hold -- the
    // framing law sees a non-first beat while idle, which is exactly "resumed
    // mid-stream".
    zref::LoomNode child = node(1, 0, zref::LoomKind::kRigid);
    child.param[0] = ONE; child.param[5] = ONE; child.param[10] = ONE;
    child.last = true;
    const Run res = drive(r, {child});
    check(res.refused, "resume after reset is REFUSED", 1, res.refused ? 1 : 0);
    check(res.out.empty(), "resume after reset emits nothing", 0,
          static_cast<uint64_t>(res.out.size()));
    check(res.reason == static_cast<uint8_t>(zref::LoomRefusal::kFraming),
          "resume after reset: reason is FRAMING",
          static_cast<uint8_t>(zref::LoomRefusal::kFraming), res.reason);

    // And the store itself is empty, which is the STRONGER claim: a properly
    // framed stream naming row 0 as a parent is refused PARENT_UNSET, because
    // the scrub cleared it. A block that kept the row would compose here and
    // deliver silently wrong geometry.
    zref::LoomNode reuse = node(1, 0, zref::LoomKind::kRigid);
    reuse.param[0] = ONE; reuse.param[5] = ONE; reuse.param[10] = ONE;
    reuse.first = true;
    reuse.last = true;
    const Run res2 = drive(r, {reuse});
    check(res2.refused, "reset cleared the store: stale parent is REFUSED", 1,
          res2.refused ? 1 : 0);
    check(res2.reason == static_cast<uint8_t>(zref::LoomRefusal::kParentUnset),
          "reset cleared the store: reason is PARENT_UNSET",
          static_cast<uint8_t>(zref::LoomRefusal::kParentUnset), res2.reason);
  }

  // =========================================================================
  // 9. THE PER-NODE CLOCK COST, MEASURED
  // =========================================================================
  // The RTL header states 47 clocks per node at MUL_LANES=1 and 22 at
  // MUL_LANES=3, and derives a frame-occupancy figure from it. That figure is
  // only worth anything if the number is measured and held. CLAUDE.md: a
  // throughput budget is written against HOW MANY TIMES the machine did the
  // work, and a result-checking test cannot see it.
  {
    acc.absorb(r);
    r.boot();
    zref::LoomNode only = node(0, 0, zref::LoomKind::kRoot);
    only.param[0] = ONE; only.param[5] = ONE; only.param[10] = ONE;
    only.first = true;
    only.last = true;
    const Run got = drive(r, {only});
    std::printf("[loom] COST: %d clocks from accept to the output beat (header says %d)\n",
                got.accept_to_first_out, ZHAO_LOOM_NODE_CLOCKS);
    check(got.accept_to_first_out == ZHAO_LOOM_NODE_CLOCKS,
          "clocks per node match the header's throughput arithmetic",
          ZHAO_LOOM_NODE_CLOCKS, static_cast<uint64_t>(got.accept_to_first_out));
  }

  // =========================================================================
  // 11. THE BOUND -- accepted at MAX_NODES, refused past it
  // =========================================================================
  // At MAX_NODES = 1024 only the first half of this runs, because the second is
  // unreachable; `test_geom_loom_small` runs both. The contract asks for
  // "1,024 nodes accepted, 1,025 refused" and that is exactly what the pair of
  // builds delivers, with the shipping build proving the acceptance and the
  // small build proving the refusal.
  {
    acc.absorb(r);
    r.boot();
    const int n = ZHAO_LOOM_MAX_NODES;
    std::vector<zref::LoomNode> s;
    for (int i = 0; i < n; ++i) {
      zref::LoomNode nd = node(static_cast<uint16_t>(i), 0,
                               i ? zref::LoomKind::kRigid : zref::LoomKind::kRoot);
      nd.param[0] = ONE; nd.param[5] = ONE; nd.param[10] = ONE;
      nd.param[3] = ONE / (i + 1);
      nd.first = (i == 0);
      nd.last = (i + 1 == n);
      s.push_back(nd);
    }
    diff(r, s, "exactly MAX_NODES nodes: accepted");

    if (ZHAO_LOOM_MAX_NODES < 1024) {
      // One past the bound. Only reachable when MAX_NODES < 2**IDXW, which is
      // why IDXW is a separate parameter.
      std::vector<zref::LoomNode> t = s;
      t[t.size() - 1].last = false;
      zref::LoomNode extra = node(static_cast<uint16_t>(n), 0, zref::LoomKind::kRigid);
      extra.param[0] = ONE; extra.param[5] = ONE; extra.param[10] = ONE;
      extra.last = true;
      t.push_back(extra);
      diff(r, t, "MAX_NODES + 1 nodes: refused");

      // And an index past the bound, which is the other half of OVERFLOW.
      zref::LoomNode root = node(0, 0, zref::LoomKind::kRoot);
      root.param[0] = ONE; root.param[5] = ONE; root.param[10] = ONE;
      root.first = true;
      zref::LoomNode beyond = node(static_cast<uint16_t>(ZHAO_LOOM_MAX_NODES + 3), 0,
                                zref::LoomKind::kRigid);
      beyond.param[0] = ONE; beyond.param[5] = ONE; beyond.param[10] = ONE;
      beyond.last = true;
      diff(r, {root, beyond}, "node_index past MAX_NODES: refused");

      check(r.dut.streams_refused_o[2] > 0, "counter streams_refused[2] OVERFLOW fired", 1,
            r.dut.streams_refused_o[2]);
    }
  }

  // =========================================================================
  // 12. EVERY COUNTER FIRED, BY NAME
  // =========================================================================
  // CLAUDE.md: "a detector reading zero is a claim, and it is the claim to
  // check hardest". Everything below has been driven by the sections above on
  // this same instance -- these are readings of the suite's own traffic, not a
  // fresh setup arranged to make them move. `acc` carries the totals across the
  // three deliberate resets; see the CounterAcc comment for why that is
  // necessary and what it caught.
  {
    acc.absorb(r);

    check(acc.nodes > 0, "counter nodes_transformed fired", 1, acc.nodes);
    check(acc.streams > 0, "counter streams_composed fired", 1, acc.streams);
    check(acc.nodes_max > 1, "counter nodes_per_stream_max fired", 2, acc.nodes_max);
    check(acc.depth_max > 1, "counter chain_depth_max fired", 2, acc.depth_max);
    check(acc.stalls > 0, "counter consumer_stall_cycles fired", 1, acc.stalls);
    for (int k = 0; k <= 9; ++k) {
      char lbl[80];
      std::snprintf(lbl, sizeof lbl, "counter node_kind_hist[%d] fired", k);
      check(acc.kind[k] > 0, lbl, 1, acc.kind[k]);
    }
    // Reason 2 (OVERFLOW) is UNREACHABLE at the shipping parameters and the RTL
    // header says why. `test_geom_loom_small` is the build that fires it; this
    // build asserts the other five and asserts that this one is SILENT, so the
    // two together are a positive control rather than a hole.
    const char* rn[6] = {"NOT_SORTED", "PARENT_UNSET", "OVERFLOW", "BAD_KIND",
                         "SCALE_SHEAR", "FRAMING"};
    for (int i = 0; i < 6; ++i) {
      char lbl[112];
      if (i == 2 && ZHAO_LOOM_MAX_NODES >= 1024) {
        std::snprintf(lbl, sizeof lbl,
                      "counter streams_refused[%d] %s is structurally silent here", i, rn[i]);
        check(acc.refused[i] == 0, lbl, 0, acc.refused[i]);
      } else {
        std::snprintf(lbl, sizeof lbl, "counter streams_refused[%d] %s fired", i, rn[i]);
        check(acc.refused[i] > 0, lbl, 1, acc.refused[i]);
      }
    }
    std::printf("[loom] counters: nodes=%llu streams=%llu stalls=%llu nodes_max=%u "
                "depth_max=%u refused={%llu,%llu,%llu,%llu,%llu,%llu}\n",
                (unsigned long long)acc.nodes, (unsigned long long)acc.streams,
                (unsigned long long)acc.stalls, acc.nodes_max, acc.depth_max,
                (unsigned long long)acc.refused[0], (unsigned long long)acc.refused[1],
                (unsigned long long)acc.refused[2], (unsigned long long)acc.refused[3],
                (unsigned long long)acc.refused[4], (unsigned long long)acc.refused[5]);
  }
  r.dut.final();
  return zhao::report_and_exit("geom_loom_directed");
}
