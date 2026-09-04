// geom_pose_cache_directed.cpp — the pose cache against
// `zref::creature::PoseBank`.
//
// The RTL holds tags, LRU and counters; the reference holds those AND the
// palettes. So the differential is on what both agree to expose and what the
// contract actually promises: hits, misses, bad_ids, clamped_inserts and
// residency, after identical request sequences.
//
// That is not a weaker test than comparing palettes would be. The decode is
// already pinned element-for-element by geom_pose_decode_directed, and a
// palette is a pure function of (type, clip, frame) — the reference says so
// itself. What the cache can get wrong is WHICH tuple it decides is already
// resident and WHICH one it throws away, and those decisions are visible in
// exactly these five numbers.
//
// Three laws carry the weight:
//
//   * A BAD ID TOUCHES NOTHING. It must not evict, must not insert, must not
//     restamp an LRU. A cache that let a bad id evict would lose a live palette
//     to a malformed request.
//   * A SLOT REFERENCED THIS FRAME IS NEVER EVICTED. This is what lets many
//     instances of one type share one decode within a frame.
//   * WHEN EVERY SLOT IS REFERENCED, THE INSERT CLAMPS rather than stalling or
//     evicting anyway: counted, deterministic, and the caller still gets a
//     correct palette.

#include "Vzhao_geom_pose_cache.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using zhao::check;
namespace zc = zref::creature;

constexpr int kTuples = 128;  // must match the RTL default and PoseBank

struct Req {
  uint16_t type;
  uint16_t clip;
  uint16_t frame;
  uint8_t sub = 0;   // the half-key phase; 1 is the baked 60 Hz midpoint
  uint16_t gen = 0;  // the clip-bank residency generation (D-3)
};

/** Creature types built so their clip tables are easy to reason about. */
struct World {
  std::vector<zc::CreatureType> types;

  // Every request names a real type. `PoseBank::acquire` takes a
  // `CreatureType&`, so "no such type id" is not a case the reference models at
  // all -- resolving the type is the caller's job, upstream of the cache. An
  // earlier draft of this test invented bad type ids and this lookup quietly
  // fell back to types.front(), so the reference saw a VALID request while the
  // RTL was told it was bad. The two then disagreed, and the RTL was right.
  const zc::CreatureType& by_id(uint16_t id) const {
    for (const auto& t : types) {
      if (t.type_id == id) return t;
    }
    std::fprintf(stderr, "FATAL: test asked for unknown type id %u\n", id);
    std::abort();
  }

  /**
   * The same predicate the reference applies before it touches the cache:
   * a clip with that slot id must exist, and the frame must be inside it.
   */
  bool resolvable(uint16_t type, uint16_t clip, uint16_t frame) const {
    const zc::CreatureType& t = by_id(type);
    for (const auto& c : t.bank.clips) {
      if (c.slot_id == clip) return frame < c.frame_count;
    }
    return false;
  }
};

World make_world() {
  World w;
  // Three types; clip slot ids are deliberately sparse (the format allows it)
  // and frame counts differ so the frame bound is a real edge.
  const uint16_t slots[3][2] = {{0, 7}, {3, 9}, {1, 2}};
  const uint16_t frames[3][2] = {{4, 100}, {1, 250}, {64, 8}};
  for (uint16_t ti = 0; ti < 3; ++ti) {
    zc::CreatureType t;
    t.type_id = static_cast<uint16_t>(10 + ti);
    t.skeleton.bone_count = 1;
    t.skeleton.bones[0].parent = 0;
    t.skeleton.bones[0].tx = 1 << 16;
    t.skeleton.bones[0].ty = 0;
    t.skeleton.bones[0].tz = 0;
    zc::bake_skeleton(t.skeleton, t.baked);
    t.bank.bone_count = 1;
    for (int ci = 0; ci < 2; ++ci) {
      zc::Clip c;
      c.slot_id = slots[ti][ci];
      c.frame_count = frames[ti][ci];
      c.root.assign(static_cast<size_t>(c.frame_count) * 3, 0);
      c.quats.assign(c.frame_count, zc::quat16_identity());
      t.bank.clips.push_back(c);
    }
    w.types.push_back(t);
  }
  return w;
}

// ---- RTL driving ---------------------------------------------------------

void rtl_begin_frame(Vzhao_geom_pose_cache& dut) {
  dut.begin_frame_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.begin_frame_i = 0;
  dut.eval();
}

/** One acquire. Returns resp_kind, or -1 if the block never answered. */
int rtl_acquire(Vzhao_geom_pose_cache& dut, const Req& r, bool resolvable) {
  dut.acq_valid_i = 1;
  dut.acq_type_i = r.type;
  dut.acq_clip_i = r.clip;
  dut.acq_frame_i = r.frame;
  dut.acq_sub_i = r.sub;
  dut.acq_gen_i = r.gen;
  dut.acq_resolvable_i = resolvable ? 1 : 0;
  dut.resp_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.acq_ready_o && guard++ < 4096) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.acq_valid_i = 0;
  dut.eval();

  guard = 0;
  while (!dut.resp_valid_o && guard++ < 4096) {
    zhao::tick(dut);
    dut.eval();
  }
  if (!dut.resp_valid_o) return -1;
  const int kind = static_cast<int>(dut.resp_kind_o);
  zhao::tick(dut);  // resp_ready_i is high: the verdict is taken
  dut.eval();
  return kind;
}

struct Counters {
  uint32_t hits, misses, bad_ids, clamped, resident;
};

Counters rtl_counters(Vzhao_geom_pose_cache& dut) {
  return Counters{dut.hits_o, dut.misses_o, dut.bad_ids_o, dut.clamped_inserts_o, dut.resident_o};
}

Counters ref_counters(const zc::PoseBank& bank) {
  const auto& c = bank.counters();
  return Counters{c.hits, c.misses, c.bad_ids, c.clamped_inserts,
                  static_cast<uint32_t>(bank.resident())};
}

void compare(const Counters& want, const Counters& got, const char* what) {
  const std::string t(what);
  check(got.hits == want.hits, (t + ": hits").c_str(), want.hits, got.hits);
  check(got.misses == want.misses, (t + ": misses").c_str(), want.misses, got.misses);
  check(got.bad_ids == want.bad_ids, (t + ": bad_ids").c_str(), want.bad_ids, got.bad_ids);
  check(got.clamped == want.clamped, (t + ": clamped_inserts").c_str(), want.clamped, got.clamped);
  check(got.resident == want.resident, (t + ": resident").c_str(), want.resident, got.resident);
}

/** Replay a request stream against both, comparing counters at the end. */
void replay(Vzhao_geom_pose_cache& dut, zc::PoseBank& bank, const World& w,
            const std::vector<Req>& reqs, const std::vector<size_t>& frame_breaks,
            const char* what) {
  size_t next_break = 0;
  for (size_t n = 0; n < reqs.size(); ++n) {
    while (next_break < frame_breaks.size() && frame_breaks[next_break] == n) {
      bank.begin_frame();
      rtl_begin_frame(dut);
      ++next_break;
    }
    const Req& r = reqs[n];
    const bool ok = w.resolvable(r.type, r.clip, r.frame);
    bank.acquire(w.by_id(r.type), r.clip, r.frame);
    const int kind = rtl_acquire(dut, r, ok);
    if (kind < 0) {
      check(false, (std::string(what) + ": the block answered every acquire").c_str(), 1, 0);
      return;
    }
    // The verdict kind must agree with what the reference did, not just the
    // totals: BAD_ID is 3 in the RTL encoding.
    if (!ok) {
      check(kind == 3, (std::string(what) + ": an unresolvable request answers BAD_ID").c_str(), 3,
            static_cast<uint64_t>(kind));
    }
  }
  compare(ref_counters(bank), rtl_counters(dut), what);
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
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
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

void reset(Vzhao_geom_pose_cache& dut) {
  dut.rst_n = 0;
  dut.acq_valid_i = 0;
  dut.begin_frame_i = 0;
  dut.resp_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Vzhao_geom_pose_cache dut;
  reset(dut);

  const World w = make_world();

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0xCAC4u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      zc::PoseBank bank;
      reset(dut);
      const size_t n = 20 + rng.below(180);
      std::vector<Req> reqs;
      std::vector<size_t> breaks;
      for (size_t k = 0; k < n; ++k) {
        // A mix that reaches every branch: mostly resolvable, with a steady
        // trickle of bad clip slots and out-of-range frames.
        Req r;
        r.type = static_cast<uint16_t>(10 + rng.below(3));
        r.clip = (rng.below(6) == 0) ? static_cast<uint16_t>(40 + rng.below(4))
                                     : static_cast<uint16_t>(rng.below(10));
        r.frame = static_cast<uint16_t>(rng.below(70));
        reqs.push_back(r);
        // Frame boundaries at irregular intervals, including back-to-back ones.
        if (rng.below(12) == 0) breaks.push_back(k);
      }
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u] n=%zu", it, n);
      replay(dut, bank, w, reqs, breaks, tag);
    }
    dut.final();
    // ---- the half-key phase is part of the KEY -----------------------------
    // Before 2026-09-03 the tag was {lru, frame, clip, type} with no `sub`,
    // while the reference `PoseBank::acquire(type, slot, frame, sub)` always
    // carried it. With baked 60 Hz presentation data a key and its midpoint
    // differ in NOTHING ELSE, so the second one hit the first one's line and the
    // cache handed back the wrong palette -- silently, with no counter moving.
    //
    // The animation ruling of 2026-09-03 permits baked 60 Hz for any creature,
    // which is what turned a latent mismatch into a live defect.
    {
      Vzhao_geom_pose_cache dut2;
      dut2.rst_n = 0;
      for (int i = 0; i < 4; ++i) zhao::tick(dut2);
      dut2.rst_n = 1;
      zhao::tick(dut2);
      rtl_begin_frame(dut2);

      const Req key{7, 3, 11, 0};
      const Req mid{7, 3, 11, 1};  // same key, the 60 Hz midpoint

      const int a = rtl_acquire(dut2, key, true);
      const int b = rtl_acquire(dut2, mid, true);

      // The first is a miss that inserts. The second MUST also miss and insert:
      // if it reports a hit it has been given the wrong pose.
      zhao::check(a == 1 && b == 1,
                  "a key and its 60 Hz midpoint are DIFFERENT cache entries -- "
                  "`sub` is part of the key, and a hit here would hand back a "
                  "palette the animator never authored",
                  1, (a == 1 && b == 1) ? 1 : 0);

      // And re-asking for the midpoint now hits, so the insert really happened
      // rather than the entry being dropped.
      const int c = rtl_acquire(dut2, mid, true);
      zhao::check(c == 0, "and the midpoint's own entry hits on the second ask", 0, c);
    }

    return zhao::report_and_exit("geom_pose_cache_random");
  }

  // ---- 1. cold miss, then a hit on the same tuple -------------------------
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs = {{10, 0, 0}, {10, 0, 0}, {10, 0, 0}};
    replay(dut, bank, w, reqs, {}, "one tuple: one miss then two hits");
  }

  // ---- 2. distinct tuples all miss ----------------------------------------
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    for (uint16_t f = 0; f < 4; ++f) reqs.push_back({10, 0, f});
    for (uint16_t f = 0; f < 4; ++f) reqs.push_back({10, 0, f});  // now all hits
    replay(dut, bank, w, reqs, {}, "four frames of one clip: four misses, four hits");
  }

  // ---- 3. BAD IDS TOUCH NOTHING -------------------------------------------
  // A bad type, a bad clip slot and a frame one past the end, interleaved with
  // real traffic. If any of them evicted or inserted, the hit that follows
  // would turn into a miss and residency would move.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs = {
        {10, 0, 0},   // miss, inserts
        {10, 55, 0},  // no such clip slot
        {10, 0, 4},   // frame == frame_count, one past the end
        {10, 0, 3},   // last legal frame of that clip
        {10, 0, 0},   // must still be a HIT
    };
    replay(dut, bank, w, reqs, {}, "bad ids never disturb the cache");
  }

  // ---- 4. the frame bound, exactly ----------------------------------------
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs = {
        {11, 3, 0},  // frame_count is 1 here: 0 is the only legal frame
        {11, 3, 1},  // one past
        {12, 2, 7},  // frame_count 8: last legal
        {12, 2, 8},  // one past
    };
    replay(dut, bank, w, reqs, {}, "the frame bound is frame < frame_count");
  }

  // ---- 5. EVICTION, and that the victim choice matches --------------------
  // The first version of this section could not tell LRU from MRU. It filled
  // the cache, evicted eight, then re-requested BOTH the oldest eight and the
  // newest eight -- so either policy produced eight hits and eight misses, just
  // with the roles swapped, and the totals were identical. A mutation that
  // evicted most-recently-used passed it cleanly.
  //
  // The probes below are asymmetric on purpose: each one asks only about a
  // group whose fate the two policies disagree on.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    std::vector<size_t> breaks;

    breaks.push_back(0);
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});  // fill

    breaks.push_back(reqs.size());
    for (uint16_t f = 200; f < 208; ++f) reqs.push_back({11, 9, f});  // evict eight

    // Probe the OLDEST group alone. Under LRU these are the eight that went;
    // under MRU they are untouched and every one of these is a hit.
    breaks.push_back(reqs.size());
    for (uint16_t f = 0; f < 8; ++f) reqs.push_back({11, 9, f});

    replay(dut, bank, w, reqs, breaks, "eviction takes the least-recently-used, not the most");
  }

  // ---- 5b. A HIT RESTAMPS THE LRU -----------------------------------------
  // Touching a tuple has to make it young again, or the cache throws away the
  // pose it is most likely to be asked for next. The restamp is invisible until
  // an eviction happens in a LATER frame -- in the same frame the tuple is
  // protected by its mark anyway, which is why this needs three frames.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    std::vector<size_t> breaks;

    breaks.push_back(0);
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});  // fill; 0 is oldest

    breaks.push_back(reqs.size());
    reqs.push_back({11, 9, 0});  // hit: 0 becomes newest

    breaks.push_back(reqs.size());
    for (uint16_t f = 130; f < 138; ++f) reqs.push_back({11, 9, f});  // evict eight

    // With the restamp, frame 0 is the YOUNGEST tuple and survives. Without it,
    // frame 0 is still the oldest and is the first thing thrown out.
    breaks.push_back(reqs.size());
    reqs.push_back({11, 9, 0});

    replay(dut, bank, w, reqs, breaks, "a hit makes its tuple young again");
  }

  // ---- 5c. A HIT MARKS THE TUPLE REFERENCED-THIS-FRAME --------------------
  // A frame in which every cached pose is used, and then one more is asked for.
  // Every slot is referenced, so there is no legal victim and the insert must
  // clamp. If a hit did not set the mark, the cache would happily evict a pose
  // that is in use this very frame -- and the clamp counter would stay at zero.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    std::vector<size_t> breaks;

    breaks.push_back(0);
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});  // fill

    breaks.push_back(reqs.size());
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});  // all HITS, all marked
    reqs.push_back({11, 9, 240});                                       // must clamp

    replay(dut, bank, w, reqs, breaks, "hits mark their tuples, so a fully-used frame clamps");
    check(rtl_counters(dut).clamped == 1,
          "the insert clamped rather than evicting a pose in use this frame", 1,
          rtl_counters(dut).clamped);
  }

  // ---- 6. THE CLAMP -------------------------------------------------------
  // Fill every slot WITHIN ONE FRAME, so every slot is referenced-this-frame
  // and none may be evicted, then ask for one more. The reference decodes into
  // scratch and counts a clamped insert; the RTL must answer CLAMPED and count
  // the same. A cache that evicted anyway would lose a palette another instance
  // is still going to ask for this frame.
  {
    zc::PoseBank bank;
    reset(dut);
    bank.begin_frame();
    rtl_begin_frame(dut);
    std::vector<Req> reqs;
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});
    for (uint16_t f = kTuples; f < kTuples + 5; ++f) reqs.push_back({11, 9, f});
    replay(dut, bank, w, reqs, {}, "a full frame clamps rather than evicting");

    check(rtl_counters(dut).clamped == 5, "exactly the five over-budget inserts clamped", 5,
          rtl_counters(dut).clamped);
    check(rtl_counters(dut).resident == kTuples, "and residency stays at the cache size", kTuples,
          rtl_counters(dut).resident);
  }

  // ---- 7. the clamp lifts at the frame boundary ---------------------------
  // The same over-budget request, after begin_frame, must insert normally. If
  // the marks did not clear, the cache would clamp forever after one busy frame.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    std::vector<size_t> breaks;
    breaks.push_back(0);
    for (uint16_t f = 0; f < kTuples; ++f) reqs.push_back({11, 9, f});
    reqs.push_back({11, 9, 200});   // clamps: frame is full
    breaks.push_back(reqs.size());  // marks clear
    reqs.push_back({11, 9, 201});   // must insert now
    replay(dut, bank, w, reqs, breaks, "begin_frame lifts the clamp");
  }

  // ---- 8. sharing within a frame ------------------------------------------
  // The economy the cache exists for: many instances of one type asking for the
  // same (clip, frame) in one frame must produce ONE miss and many hits.
  {
    zc::PoseBank bank;
    reset(dut);
    std::vector<Req> reqs;
    std::vector<size_t> breaks{0};
    for (int k = 0; k < 60; ++k) reqs.push_back({12, 1, 5});
    replay(dut, bank, w, reqs, breaks, "sixty instances of one pose: one miss, fifty-nine hits");
    check(rtl_counters(dut).misses == 1, "exactly one decode was demanded", 1,
          rtl_counters(dut).misses);
    check(rtl_counters(dut).hits == 59, "and the rest were shared", 59, rtl_counters(dut).hits);
  }

  dut.final();
  // ---- the clip-bank generation is part of the KEY (D-3) -----------------
  // Owner ruling D-3: cache tag = physical line tag + residency generation,
  // and the ruling names this cache. Animation banks are uploaded into
  // REUSABLE local-SDRAM slots, so a pose cached from generation N and read
  // after N+1 was published is a pose from bytes that are no longer there. It
  // decodes, it looks like animation, and nothing reports an error.
  //
  // This is the same silent-wrong-answer shape as the `sub` aliasing fixed
  // earlier today, one level up: an identity that is not unique returns the
  // wrong entry confidently.
  {
    Vzhao_geom_pose_cache dut3;
    dut3.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(dut3);
    dut3.rst_n = 1;
    zhao::tick(dut3);
    rtl_begin_frame(dut3);

    const Req old_gen{5, 2, 9, 0, 1};  // generation 1
    const Req new_gen{5, 2, 9, 0, 2};  // republished: same tuple, generation 2

    const int a = rtl_acquire(dut3, old_gen, true);
    const int b = rtl_acquire(dut3, new_gen, true);

    zhao::check(a == 1 && b == 1,
                "a pose from a REPUBLISHED clip bank is a different cache "
                "entry -- the generation is part of the key, so a hit here "
                "would hand back a pose decoded from bytes that have been "
                "overwritten",
                1, (a == 1 && b == 1) ? 1 : 0);

    // and the old generation must NOT come back to life on a re-ask
    const int c = rtl_acquire(dut3, new_gen, true);
    zhao::check(c == 0,
                "while the new generation's own entry hits, so the insert "
                "really happened",
                0, c);
  }

  return zhao::report_and_exit("geom_pose_cache_directed");
}
