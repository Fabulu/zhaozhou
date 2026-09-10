// field_progdir_diff.hpp -- the transaction-level differential ENGINE for the
// FIELD program directory: the retained oracle (zhao_field_progcache) and the
// scanned candidate (zhao_field_progdir) elaborated side by side in
// tests/field/tb_field_progdir_diff.sv, driven with the SAME transaction script,
// each at its own pace, and compared TRANSACTION BY TRANSACTION.
//
// Shared by field_progdir_differential.cpp (the gate) and
// field_progdir_mutant_control.cpp (the inverted-polarity positive control), so
// the control exercises the very checker the gate relies on.
//
// WHY "AT ITS OWN PACE" AND STILL A FAIR COMPARISON. The two blocks accept the
// same requests in the same ORDER but not on the same clocks: the oracle answers
// in one cycle, the candidate in ENTRIES+2. A harness that drove both from one
// cycle-indexed stimulus would either stall the oracle artificially or present
// the candidate with requests it cannot yet take. Instead every OP in the script
// is a small handshake-legal programme (offer, wait for acceptance, wait for the
// response, hold ready low for a stall, consume) run to completion on BOTH sides
// before the next op starts. Both sides are quiescent at every op boundary, so
// the accepted order is the script order on both, and the comparison is of
// responses and counters at that boundary. Response stalls ("held responses"),
// simultaneous offers ("interleaved channels") and a commit accepted while a
// lookup's answer is still held are ops of their own.
//
// Verilator port types: 1-bit and <=8-bit ports are CData, 32-bit ports IData.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_field_progdir_diff.h"
#include "zhao_sim.hpp"

namespace progdir {

using Top = Vtb_field_progdir_diff;

#ifndef TB_ENTRIES
#error "compile with -DTB_ENTRIES=<n> matching -GENTRIES"
#endif
#ifndef TB_LRUW
#error "compile with -DTB_LRUW=<n> matching -GLRUW"
#endif
constexpr unsigned kEntries = TB_ENTRIES;
constexpr unsigned kLruW = TB_LRUW;

/** One side's ports, by reference into the shared top. */
struct Side {
  const char* name;
  CData& lu_valid;
  CData& lu_ready;
  IData& lu_hash;
  CData& lu_resp_valid;
  CData& lu_resp_ready;
  CData& lu_hit;
  CData& lu_slot;
  CData& cm_valid;
  CData& cm_ready;
  IData& cm_hash;
  CData& cm_ok;
  CData& cm_resp_valid;
  CData& cm_resp_ready;
  CData& cm_inserted;
  CData& cm_evicted;
  CData& cm_slot;
  IData& hits;
  IData& misses;
  IData& rejected;
  IData& evictions;
  CData& occupancy;
};

inline Side oracle_side(Top& t) {
  return Side{"oracle",
              t.o_lu_valid_i, t.o_lu_ready_o, t.o_lu_hash_i, t.o_lu_resp_valid_o,
              t.o_lu_resp_ready_i, t.o_lu_hit_o, t.o_lu_slot_o, t.o_cm_valid_i,
              t.o_cm_ready_o, t.o_cm_hash_i, t.o_cm_ok_i, t.o_cm_resp_valid_o,
              t.o_cm_resp_ready_i, t.o_cm_inserted_o, t.o_cm_evicted_o, t.o_cm_slot_o,
              t.o_hits_o, t.o_misses_o, t.o_programs_rejected_o, t.o_evictions_o,
              t.o_occupancy_o};
}

inline Side candidate_side(Top& t) {
  return Side{"candidate",
              t.n_lu_valid_i, t.n_lu_ready_o, t.n_lu_hash_i, t.n_lu_resp_valid_o,
              t.n_lu_resp_ready_i, t.n_lu_hit_o, t.n_lu_slot_o, t.n_cm_valid_i,
              t.n_cm_ready_o, t.n_cm_hash_i, t.n_cm_ok_i, t.n_cm_resp_valid_o,
              t.n_cm_resp_ready_i, t.n_cm_inserted_o, t.n_cm_evicted_o, t.n_cm_slot_o,
              t.n_hits_o, t.n_misses_o, t.n_programs_rejected_o, t.n_evictions_o,
              t.n_occupancy_o};
}

enum class Kind : uint8_t {
  Lookup,  // one lookup, consume its response after lu_stall cycles
  Commit,  // one commit (ok or rejected), consume after cm_stall cycles
  Pair,    // lookup AND commit offered on the same clock; both consumed
  Held,    // lookup; its response is left UNTAKEN while a commit is accepted and
           // answered; then the lookup response is consumed, then the commit's
  Reset,   // a lookup is accepted with ready LOW on both sides, then rst_n falls
           // mid-flight: "reset with work outstanding"
};

struct Op {
  Kind kind = Kind::Lookup;
  uint32_t lu_hash = 0;
  uint32_t cm_hash = 0;
  bool ok = true;
  unsigned lu_stall = 0;
  unsigned cm_stall = 0;
  const char* what = "";
};

/** What one side did for one op, recorded at the op boundary. */
struct Rec {
  bool lu_used = false, cm_used = false;
  bool hit = false;
  unsigned lu_slot = 0;
  bool inserted = false, evicted = false;
  unsigned cm_slot = 0;
  int order = -1;  // Pair: 0 = lookup accepted first, 1 = commit first
  bool both_fired = false;  // Pair: a lookup and a commit fired on ONE edge (illegal)
  uint32_t hits = 0, misses = 0, rejected = 0, evictions = 0;
  unsigned occupancy = 0;
  // timing, in harness cycles
  long lu_accept = -1, lu_resp = -1, cm_accept = -1, cm_resp = -1;
};

/**
 * Runs one op on one side, one clock at a time. `drive()` sets inputs for the
 * coming edge from the state; `sample()` looks at the settled combinational
 * outputs and decides what THIS edge will do; `edge()` is called after the tick.
 */
class Runner {
 public:
  explicit Runner(Side s) : s_(s) {}

  void begin(const Op& op) {
    op_ = op;
    rec_ = Rec{};
    st_ = St::Start;
    lu_ph_ = Ph::None;
    cm_ph_ = Ph::None;
    stall_left_lu_ = 0;
    stall_left_cm_ = 0;
    idle_inputs();
  }

  bool done() const { return st_ == St::Done; }
  const Rec& rec() const { return rec_; }
  const Side& side() const { return s_; }
  Side& side_mut() { return s_; }

  void snapshot_counters() {
    rec_.hits = s_.hits;
    rec_.misses = s_.misses;
    rec_.rejected = s_.rejected;
    rec_.evictions = s_.evictions;
    rec_.occupancy = s_.occupancy;
  }

  void idle_inputs() {
    s_.lu_valid = 0;
    s_.cm_valid = 0;
    s_.lu_resp_ready = 0;
    s_.cm_resp_ready = 0;
  }

  // ---- per cycle -----------------------------------------------------------
  void drive() {
    if (st_ == St::Start) {
      switch (op_.kind) {
        case Kind::Lookup: lu_ph_ = Ph::Offer; break;
        case Kind::Commit: cm_ph_ = Ph::Offer; break;
        case Kind::Pair: lu_ph_ = Ph::Offer; cm_ph_ = Ph::Offer; break;
        case Kind::Held: lu_ph_ = Ph::Offer; break;
        case Kind::Reset: lu_ph_ = Ph::Offer; break;
      }
      st_ = St::Run;
    }
    s_.lu_valid = (lu_ph_ == Ph::Offer) ? 1 : 0;
    s_.lu_hash = op_.lu_hash;
    s_.cm_valid = (cm_ph_ == Ph::Offer) ? 1 : 0;
    s_.cm_hash = op_.cm_hash;
    s_.cm_ok = op_.ok ? 1 : 0;
    // Ready is raised only in the Consume phase (after the stall has elapsed).
    s_.lu_resp_ready = (lu_ph_ == Ph::Consume) ? 1 : 0;
    s_.cm_resp_ready = (cm_ph_ == Ph::Consume) ? 1 : 0;
  }

  // Called after eval(), before the edge. Decides what the edge does.
  void sample(long cycle) {
    if (st_ != St::Run) return;
    const bool lu_fire = s_.lu_valid && s_.lu_ready;
    const bool cm_fire = s_.cm_valid && s_.cm_ready;
    if (lu_fire && cm_fire) rec_.both_fired = true;
    if (lu_fire) {
      rec_.lu_accept = cycle;
      lu_next_ = Ph::WaitResp;
      if (op_.kind == Kind::Pair && rec_.order < 0) rec_.order = 0;
      if (op_.kind == Kind::Reset) lu_next_ = Ph::Parked;  // never consumed
    }
    if (cm_fire) {
      rec_.cm_accept = cycle;
      cm_next_ = Ph::WaitResp;
      if (op_.kind == Kind::Pair && rec_.order < 0) rec_.order = 1;
    }
    // Responses become visible after an edge; a Consume-phase ready high with
    // valid high means THIS edge takes it: capture the fields now.
    if (lu_ph_ == Ph::Consume && s_.lu_resp_valid) {
      rec_.lu_used = true;
      rec_.hit = s_.lu_hit != 0;
      rec_.lu_slot = s_.lu_slot;
      lu_next_ = Ph::None;
    }
    if (cm_ph_ == Ph::Consume && s_.cm_resp_valid) {
      rec_.cm_used = true;
      rec_.inserted = s_.cm_inserted != 0;
      rec_.evicted = s_.cm_evicted != 0;
      rec_.cm_slot = s_.cm_slot;
      cm_next_ = Ph::None;
    }
  }

  // Called after the edge.
  void edge(long cycle) {
    if (st_ != St::Run) return;
    if (lu_next_ != Ph::Keep) { lu_ph_ = lu_next_; lu_next_ = Ph::Keep; }
    if (cm_next_ != Ph::Keep) { cm_ph_ = cm_next_; cm_next_ = Ph::Keep; }

    // Response arrival: WaitResp -> Stall (or Parked for Held's lookup).
    if (lu_ph_ == Ph::WaitResp && s_.lu_resp_valid) {
      rec_.lu_resp = cycle;
      if (op_.kind == Kind::Held) {
        lu_ph_ = Ph::Parked;   // keep it untaken; the commit goes first
        cm_ph_ = Ph::Offer;
      } else {
        stall_left_lu_ = op_.lu_stall;
        lu_ph_ = Ph::Stall;
      }
    }
    if (cm_ph_ == Ph::WaitResp && s_.cm_resp_valid) {
      rec_.cm_resp = cycle;
      stall_left_cm_ = op_.cm_stall;
      cm_ph_ = Ph::Stall;
      if (op_.kind == Kind::Held) {
        // Now release the lookup's answer first; the commit's stall counts
        // from here and it is consumed after.
        lu_ph_ = Ph::Consume;
        cm_ph_ = Ph::HeldWait;
      }
    }
    if (lu_ph_ == Ph::Stall) {
      if (stall_left_lu_ == 0) lu_ph_ = Ph::Consume; else --stall_left_lu_;
    }
    if (cm_ph_ == Ph::Stall) {
      if (stall_left_cm_ == 0) cm_ph_ = Ph::Consume; else --stall_left_cm_;
    }
    if (cm_ph_ == Ph::HeldWait && lu_ph_ == Ph::None) {
      cm_ph_ = Ph::Stall;  // lookup consumed; now the commit's stall then consume
    }

    const bool lu_idle = (lu_ph_ == Ph::None) || (lu_ph_ == Ph::Parked && op_.kind == Kind::Reset);
    const bool cm_idle = (cm_ph_ == Ph::None);
    if (op_.kind == Kind::Reset) {
      if (lu_ph_ == Ph::Parked) st_ = St::Done;  // accepted; the harness resets
    } else if (lu_idle && cm_idle) {
      st_ = St::Done;
      idle_inputs();
      snapshot_counters();
    }
  }

 private:
  enum class St { Start, Run, Done };
  enum class Ph { None, Offer, WaitResp, Stall, Consume, Parked, HeldWait, Keep };

  Side s_;
  Op op_{};
  Rec rec_{};
  St st_ = St::Done;
  Ph lu_ph_ = Ph::None, cm_ph_ = Ph::None;
  Ph lu_next_ = Ph::Keep, cm_next_ = Ph::Keep;
  unsigned stall_left_lu_ = 0, stall_left_cm_ = 0;
};

/** Mismatch categories, so a control can say WHICH law the checker saw break. */
enum : unsigned {
  kMisLuResult = 1u << 0,   // hit or lookup slot
  kMisCmSlot   = 1u << 1,   // commit slot only
  kMisCmFlags  = 1u << 2,   // inserted / evicted
  kMisCounters = 1u << 3,   // any of the five counters
  kMisOrder    = 1u << 4,   // Pair accepted order, or both fired on one edge
  kMisProtocol = 1u << 5,   // a side did not complete the op (hang)
};

struct Stats {
  unsigned ops = 0;
  unsigned mismatch_mask = 0;
  unsigned first_mismatch_op = 0;
  unsigned first_mismatch_mask = 0;
  std::string first_mismatch_what;
  // timing, candidate side
  long max_lu_latency = -1, max_cm_latency = -1;
  // the largest value each CANDIDATE counter reached at any op boundary, so
  // "every counter fired" can be asserted across a script that resets midway
  uint32_t seen_hits = 0, seen_misses = 0, seen_rejected = 0, seen_evictions = 0;
  unsigned seen_occupancy = 0;
};

class Harness {
 public:
  explicit Harness(Top& t) : top_(t), o_(oracle_side(t)), n_(candidate_side(t)) {}

  long cycle() const { return cycle_; }
  Stats& stats() { return stats_; }

  void reset() {
    top_.rst_n = 0;
    o_.idle_inputs();
    n_.idle_inputs();
    top_.eval();
    for (int k = 0; k < 3; ++k) tick();
    top_.rst_n = 1;
    top_.eval();
  }

  /** Run one op on both sides to completion and compare. Returns mismatch mask. */
  unsigned run(const Op& op, const char* tag) {
    ++stats_.ops;
    if (op.kind == Kind::Reset) return run_reset(op, tag);

    o_.begin(op);
    n_.begin(op);
    const long guard = 96 + 6 * static_cast<long>(kEntries) + 8 * (op.lu_stall + op.cm_stall);
    long spent = 0;
    while (!(o_.done() && n_.done()) && spent < guard) {
      step();
      ++spent;
    }
    unsigned mask = 0;
    const std::string t = std::string(tag) + " [" + op.what + "]";
    if (!(o_.done() && n_.done())) {
      mask |= kMisProtocol;
      zhao::check(false, (t + ": both sides complete the op").c_str(), 1, 0);
      std::printf("    oracle done=%d candidate done=%d after %ld cycles\n", o_.done(), n_.done(), spent);
    } else {
      mask |= compare(o_.rec(), n_.rec(), op, t);
    }
    if (n_.rec().hits > stats_.seen_hits) stats_.seen_hits = n_.rec().hits;
    if (n_.rec().misses > stats_.seen_misses) stats_.seen_misses = n_.rec().misses;
    if (n_.rec().rejected > stats_.seen_rejected) stats_.seen_rejected = n_.rec().rejected;
    if (n_.rec().evictions > stats_.seen_evictions) stats_.seen_evictions = n_.rec().evictions;
    if (n_.rec().occupancy > stats_.seen_occupancy) stats_.seen_occupancy = n_.rec().occupancy;
    if (n_.rec().lu_used && n_.rec().lu_accept >= 0 && n_.rec().lu_resp >= 0) {
      const long lat = n_.rec().lu_resp - n_.rec().lu_accept;
      if (lat > stats_.max_lu_latency) stats_.max_lu_latency = lat;
    }
    if (n_.rec().cm_used && n_.rec().cm_accept >= 0 && n_.rec().cm_resp >= 0) {
      const long lat = n_.rec().cm_resp - n_.rec().cm_accept;
      if (lat > stats_.max_cm_latency) stats_.max_cm_latency = lat;
    }
    note(mask, t);
    return mask;
  }

  /**
   * Throughput probe on ONE side: hold `lu_valid` (or `cm_valid`) high with a
   * fresh hash per acceptance, ready always high, and return the accept-to-
   * accept intervals observed. `commit` selects the channel; `ok` the verdict.
   */
  std::vector<long> probe_intervals(bool candidate, bool commit, bool ok, unsigned count,
                                    uint32_t hash_base) {
    Side& s = candidate ? n_.side_mut() : o_.side_mut();
    o_.idle_inputs();
    n_.idle_inputs();
    o_.side_mut().lu_resp_ready = 1;
    o_.side_mut().cm_resp_ready = 1;
    n_.side_mut().lu_resp_ready = 1;
    n_.side_mut().cm_resp_ready = 1;
    std::vector<long> accepts;
    uint32_t h = hash_base;
    long guard = 64 + static_cast<long>(count) * (8 + 2 * static_cast<long>(kEntries));
    while (accepts.size() < count && guard-- > 0) {
      if (commit) { s.cm_valid = 1; s.cm_hash = h; s.cm_ok = ok ? 1 : 0; }
      else        { s.lu_valid = 1; s.lu_hash = h; }
      top_.eval();
      const bool fire = commit ? (s.cm_valid && s.cm_ready) : (s.lu_valid && s.lu_ready);
      if (fire) { accepts.push_back(cycle_); ++h; }
      tick();
    }
    s.lu_valid = 0;
    s.cm_valid = 0;
    // drain
    for (int k = 0; k < 8 + 2 * static_cast<int>(kEntries); ++k) tick();
    o_.idle_inputs();
    n_.idle_inputs();
    std::vector<long> iv;
    for (size_t k = 1; k < accepts.size(); ++k) iv.push_back(accepts[k] - accepts[k - 1]);
    return iv;
  }

  /** Accept-to-response-visible latency on one side for one lookup / commit. */
  long probe_latency(bool candidate, bool commit, bool ok, uint32_t hash) {
    Side& s = candidate ? n_.side_mut() : o_.side_mut();
    o_.idle_inputs();
    n_.idle_inputs();
    s.lu_resp_ready = 1;
    s.cm_resp_ready = 1;
    if (commit) { s.cm_valid = 1; s.cm_hash = hash; s.cm_ok = ok ? 1 : 0; }
    else        { s.lu_valid = 1; s.lu_hash = hash; }
    long accept = -1, resp = -1;
    for (int k = 0; k < 64 + 4 * static_cast<int>(kEntries) && resp < 0; ++k) {
      top_.eval();
      const bool fire = commit ? (s.cm_valid && s.cm_ready) : (s.lu_valid && s.lu_ready);
      if (fire && accept < 0) accept = cycle_;
      tick();
      if (accept >= 0) { s.lu_valid = 0; s.cm_valid = 0; }
      const bool rv = commit ? (s.cm_resp_valid != 0) : (s.lu_resp_valid != 0);
      if (accept >= 0 && rv && resp < 0) resp = cycle_;
    }
    for (int k = 0; k < 4; ++k) tick();
    o_.idle_inputs();
    n_.idle_inputs();
    return (accept >= 0 && resp >= 0) ? (resp - accept) : -1;
  }

 private:
  unsigned run_reset(const Op& op, const char* tag) {
    // Accept a lookup on both sides with ready LOW, so the oracle's answer is
    // held and the candidate is mid-scan, then drop rst_n for two edges.
    o_.begin(op);
    n_.begin(op);
    long spent = 0;
    while (!(o_.done() && n_.done()) && spent < 32) { step(); ++spent; }
    const std::string t = std::string(tag) + " [" + op.what + "]";
    unsigned mask = 0;
    if (!(o_.done() && n_.done())) {
      mask |= kMisProtocol;
      zhao::check(false, (t + ": both sides accepted the lookup before reset").c_str(), 1, 0);
    }
    // A few more clocks so the candidate is genuinely inside its scan.
    for (int k = 0; k < 3; ++k) step();
    reset();
    // Both must be empty afterwards, with no response outstanding.
    const bool clean = top_.o_lu_resp_valid_o == 0 && top_.n_lu_resp_valid_o == 0 &&
                       top_.o_cm_resp_valid_o == 0 && top_.n_cm_resp_valid_o == 0 &&
                       top_.o_hits_o == 0 && top_.n_hits_o == 0 && top_.o_misses_o == 0 &&
                       top_.n_misses_o == 0 && top_.o_occupancy_o == 0 && top_.n_occupancy_o == 0 &&
                       top_.o_programs_rejected_o == 0 && top_.n_programs_rejected_o == 0 &&
                       top_.o_evictions_o == 0 && top_.n_evictions_o == 0;
    zhao::check(clean, (t + ": reset with work outstanding leaves both empty").c_str(), 1,
                clean ? 1 : 0);
    if (!clean) mask |= kMisCounters;
    note(mask, t);
    return mask;
  }

  void note(unsigned mask, const std::string& t) {
    if (mask && stats_.mismatch_mask == 0) {
      stats_.first_mismatch_op = stats_.ops;
      stats_.first_mismatch_mask = mask;
      stats_.first_mismatch_what = t;
    }
    stats_.mismatch_mask |= mask;
  }

  void step() {
    o_.drive();
    n_.drive();
    top_.eval();
    o_.sample(cycle_);
    n_.sample(cycle_);
    tick();
    o_.edge(cycle_);
    n_.edge(cycle_);
  }

  void tick() {
    zhao::tick(top_);
    ++cycle_;
  }

  unsigned compare(const Rec& a, const Rec& b, const Op& op, const std::string& t) {
    unsigned m = 0;
    auto chk = [&](bool cond, const char* what, uint64_t exp, uint64_t got, unsigned cat) {
      zhao::check(cond, (t + ": " + what).c_str(), exp, got);
      if (!cond) m |= cat;
    };
    chk(a.lu_used == b.lu_used, "lookup channel used on both", a.lu_used, b.lu_used, kMisProtocol);
    chk(a.cm_used == b.cm_used, "commit channel used on both", a.cm_used, b.cm_used, kMisProtocol);
    if (a.lu_used && b.lu_used) {
      chk(a.hit == b.hit, "lookup hit agrees", a.hit, b.hit, kMisLuResult);
      chk(a.lu_slot == b.lu_slot, "lookup slot agrees", a.lu_slot, b.lu_slot, kMisLuResult);
    }
    if (a.cm_used && b.cm_used) {
      chk(a.inserted == b.inserted, "commit inserted agrees", a.inserted, b.inserted, kMisCmFlags);
      chk(a.evicted == b.evicted, "commit evicted agrees", a.evicted, b.evicted, kMisCmFlags);
      chk(a.cm_slot == b.cm_slot, "commit slot agrees", a.cm_slot, b.cm_slot, kMisCmSlot);
    }
    if (op.kind == Kind::Pair) {
      chk(a.order == 0, "oracle accepted the LOOKUP first", 0, static_cast<uint64_t>(a.order), kMisOrder);
      chk(b.order == 0, "candidate accepted the LOOKUP first", 0, static_cast<uint64_t>(b.order), kMisOrder);
      chk(!a.both_fired, "oracle never fired both on one edge", 0, a.both_fired, kMisOrder);
      chk(!b.both_fired, "candidate never fired both on one edge", 0, b.both_fired, kMisOrder);
    }
    chk(a.hits == b.hits, "hits agree", a.hits, b.hits, kMisCounters);
    chk(a.misses == b.misses, "misses agree", a.misses, b.misses, kMisCounters);
    chk(a.rejected == b.rejected, "programs_rejected agree", a.rejected, b.rejected, kMisCounters);
    chk(a.evictions == b.evictions, "evictions agree", a.evictions, b.evictions, kMisCounters);
    chk(a.occupancy == b.occupancy, "occupancy agrees", a.occupancy, b.occupancy, kMisCounters);
    return m;
  }

  Top& top_;
  Runner o_, n_;
  long cycle_ = 0;
  Stats stats_;
};

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
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

inline uint32_t hash_of(unsigned k) { return 0x9E37'79B9u * (k + 1) ^ 0xA5A5'0000u; }

// ---------------------------------------------------------------------------
// THE DIRECTED SCRIPTS
// ---------------------------------------------------------------------------
inline Op lookup(uint32_t h, const char* what, unsigned stall = 0) {
  Op o; o.kind = Kind::Lookup; o.lu_hash = h; o.lu_stall = stall; o.what = what; return o;
}
inline Op commit(uint32_t h, bool ok, const char* what, unsigned stall = 0) {
  Op o; o.kind = Kind::Commit; o.cm_hash = h; o.ok = ok; o.cm_stall = stall; o.what = what; return o;
}
inline Op pair(uint32_t lh, uint32_t ch, bool ok, const char* what, unsigned ls = 0, unsigned cs = 0) {
  Op o; o.kind = Kind::Pair; o.lu_hash = lh; o.cm_hash = ch; o.ok = ok; o.lu_stall = ls; o.cm_stall = cs; o.what = what; return o;
}
inline Op held(uint32_t lh, uint32_t ch, bool ok, const char* what, unsigned cs = 0) {
  Op o; o.kind = Kind::Held; o.lu_hash = lh; o.cm_hash = ch; o.ok = ok; o.cm_stall = cs; o.what = what; return o;
}
inline Op reset_op(uint32_t lh, const char* what) {
  Op o; o.kind = Kind::Reset; o.lu_hash = lh; o.what = what; return o;
}

/** Fill the directory with hashes 0..E-1 the way a caller does: miss, then commit. */
inline void script_fill(std::vector<Op>& s, unsigned e) {
  for (unsigned k = 0; k < e; ++k) {
    s.push_back(lookup(hash_of(k), "fill: lookup misses"));
    s.push_back(commit(hash_of(k), true, "fill: commit inserts"));
  }
}

/** Every non-wrapping law. Valid at any ENTRIES >= 2. */
inline std::vector<Op> script_directed() {
  std::vector<Op> s;
  const unsigned E = kEntries;
  // 1. cold miss, insert, hits; response stalls on both channels
  s.push_back(lookup(hash_of(0), "cold lookup misses", 3));
  s.push_back(commit(hash_of(0), true, "first commit inserts into slot 0", 5));
  s.push_back(lookup(hash_of(0), "then it hits", 0));
  s.push_back(lookup(hash_of(0), "and hits again, held 7", 7));
  // 2. fill, then every one hits
  s.push_back(reset_op(hash_of(0), "reset before fill"));
  script_fill(s, E);
  for (unsigned k = 0; k < E; ++k) s.push_back(lookup(hash_of(k), "full: every resident hits", k % 3));
  // 3. LRU: touch 0, insert new -> evicts slot 1 (the oldest); asymmetric probes
  s.push_back(lookup(hash_of(0), "lru: touch hash 0"));
  s.push_back(lookup(hash_of(E), "lru: new hash misses"));
  s.push_back(commit(hash_of(E), true, "lru: insert evicts the oldest (slot 1)"));
  s.push_back(lookup(hash_of(0), "lru: hash 0 survived"));
  s.push_back(lookup(hash_of(1), "lru: hash 1 is gone"));
  s.push_back(lookup(hash_of(2), "lru: hash 2 still there"));
  // 4. rejected commits against a FULL directory: nothing moves
  s.push_back(commit(hash_of(E + 1), false, "reject against full: nothing moves", 2));
  s.push_back(commit(hash_of(E + 1), false, "reject again: not remembered"));
  for (unsigned k = 2; k < E; ++k) s.push_back(lookup(hash_of(k), "after rejects: residents still hit"));
  s.push_back(lookup(hash_of(E + 1), "the rejected hash never hits"));
  // 5. duplicate commit of a RESIDENT hash -- caller misuse, old semantics retained
  s.push_back(commit(hash_of(0), true, "duplicate commit of a resident hash inserts anyway"));
  s.push_back(lookup(hash_of(0), "duplicate: lookup returns the LOWEST index"));
  // 6. interleaved channels: lookup and commit offered on one clock, all four combos
  s.push_back(pair(hash_of(2), hash_of(E + 2), true, "pair: hit + valid commit", 1, 2));
  s.push_back(pair(hash_of(E + 9), hash_of(E + 3), true, "pair: miss + valid commit", 0, 0));
  s.push_back(pair(hash_of(3), hash_of(E + 4), false, "pair: hit + rejected commit", 4, 0));
  s.push_back(pair(hash_of(E + 8), hash_of(E + 5), false, "pair: miss + rejected commit", 0, 3));
  // 7. held lookup response while a commit is accepted and answered
  s.push_back(held(hash_of(E + 8), hash_of(E + 6), true, "held: lookup MISS held, valid commit lands"));
  s.push_back(held(hash_of(E + 6), hash_of(E + 7), false, "held: lookup HIT held, rejected commit -- the two are not confused", 2));
  s.push_back(held(hash_of(E + 30), hash_of(E + 31), false, "held: lookup MISS held, rejected commit -- hit=0 vs inserted=0"));
  s.push_back(lookup(hash_of(E + 31), "held: the rejected hash is absent"));
  s.push_back(lookup(hash_of(E + 6), "held: the inserted hash is present"));
  // 8. reset with work outstanding, then a short refill proves both are empty
  s.push_back(reset_op(hash_of(0), "reset mid-flight"));
  s.push_back(lookup(hash_of(0), "after reset: misses"));
  s.push_back(commit(hash_of(0), true, "after reset: inserts into slot 0"));
  return s;
}

/**
 * THE TIE. Stamps are unique until the LRUW-bit counter wraps, so a tie is
 * reachable only at a small LRUW: fill E rows (stamps 1..E), then hit hash 0
 * until its stamp equals hash 1's stamp (2). A commit then finds rows 0 and 1
 * tied at 2; the law says LOWEST index -> slot 0. Only built when the hit count
 * needed is small enough to be a test rather than a soak.
 */
inline bool tie_reachable() { return kLruW <= 8 && kEntries <= (1u << kLruW); }

inline std::vector<Op> script_tie() {
  std::vector<Op> s;
  if (!tie_reachable()) return s;
  const unsigned E = kEntries;
  const uint64_t mod = uint64_t{1} << (kLruW & 63u);   // masked: compiled at LRUW=48 too
  s.push_back(reset_op(hash_of(0), "tie: reset"));
  script_fill(s, E);  // stamps 1..E; lru_ctr == E
  // After k >= 1 hits on hash 0 its stamp is (E + k) mod 2^L; hash 1 sits at 2.
  // Want E + k == 2 (mod 2^L) with k >= 1 -- k == 0 would leave hash 0 at stamp
  // 1 and build NO tie, which is precisely what the first version of this
  // script did: the positive control then reported "no mismatch" and looked
  // like a checker that could not see a tie. A control that finds nothing is a
  // claim to check hardest.
  unsigned k = static_cast<unsigned>((mod + 2 - (E % mod)) % mod);
  if (k == 0) k = static_cast<unsigned>(mod);
  for (unsigned n = 0; n < k; ++n) s.push_back(lookup(hash_of(0), "tie: hit hash 0 to walk its stamp round to 2"));
  s.push_back(lookup(hash_of(E + 40), "tie: a fresh hash misses"));
  s.push_back(commit(hash_of(E + 40), true, "TIE: rows 0 and 1 both stamped 2 -> LOWEST index (slot 0) is the victim"));
  s.push_back(lookup(hash_of(1), "tie: hash 1 survived"));
  s.push_back(lookup(hash_of(0), "tie: hash 0 is gone"));
  return s;
}

/** Random op streams over a hash pool larger than the directory. */
inline std::vector<Op> script_random(Prng& rng, unsigned n_ops) {
  std::vector<Op> s;
  const unsigned pool = kEntries + kEntries / 2 + 2;
  s.push_back(reset_op(hash_of(0), "random: reset"));
  for (unsigned k = 0; k < n_ops; ++k) {
    const uint32_t r = rng.below(100);
    const uint32_t a = hash_of(rng.below(pool));
    const uint32_t b = hash_of(rng.below(pool));
    const bool ok = rng.below(100) < 78;
    const unsigned s1 = rng.below(100) < 30 ? rng.below(7) : 0;
    const unsigned s2 = rng.below(100) < 30 ? rng.below(7) : 0;
    if (r < 45)      s.push_back(lookup(a, "random lookup", s1));
    else if (r < 80) s.push_back(commit(a, ok, "random commit", s2));
    else if (r < 90) s.push_back(pair(a, b, ok, "random pair", s1, s2));
    else if (r < 98) s.push_back(held(a, b, ok, "random held", s2));
    else             s.push_back(reset_op(a, "random reset mid-flight"));
  }
  return s;
}

}  // namespace progdir
