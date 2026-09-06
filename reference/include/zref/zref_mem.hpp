// zref_mem.hpp — the MEM subsystem golden-reference oracles (plan W2.5).
//
// Spec: spec/memory_rules.md (§1 SDRAM sim profile, §2 arbiter policy D3,
// §3-§4 bridge bursts/rings, §5 guard region map) and the law tables in
// fpga/rtl/memory/zhao_sdram_ctrl.sv / zhao_vram_arbiter.sv.
//
// Four oracles, all deterministic and cycle-exact mirrors of the RTL law
// (independently coded in C++; the differential tests prove them equal):
//
//   zref::SdramController  transaction-level/cycle-stepped SDRAM timing
//                          oracle under the frozen sim profile (CAS 3, burst
//                          8, tRCD/tRP 3, tRC 9, refresh every 780 with the
//                          deferral law) — accepted requests -> exact grant,
//                          beat and completion cycles, incl. refresh steals.
//   zref::VramArbiter      the D3 grant-sequence oracle: scanout strict
//                          priority, RR among guaranteed, aging override,
//                          credit pools, byte accounting.
//   zref::MemoryGuard      the Phase-2 region-map verdict oracle.
//   zref::HpsBridge        burst bookkeeping under the sim latency profile
//                          (16 cycles to first beat, 1 beat/cycle after).
//
// No globals, no I/O; every piece of state is a plain struct field.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace zref {

// ---------------------------------------------------------------------------
// deterministic stream helper (PCG32 — same family as the other wave-2 tests)
// ---------------------------------------------------------------------------
struct Pcg32 {
  uint64_t state;
  uint64_t inc;
  explicit Pcg32(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t seq = 0xda3e39cb94b95bdbULL)
      : state(seed), inc(seq | 1ULL) {}
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    const uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
    const uint32_t rot = uint32_t(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
  }
  uint32_t range(uint32_t n) { return n ? next() % n : 0; }  // tests only
};

// ---------------------------------------------------------------------------
// frozen sim profile (spec/memory_rules.md §1 / zhao_sdram_params_pkg.sv)
// ---------------------------------------------------------------------------
struct SdramProfile {
  unsigned cas = 3;
  unsigned burst = 8;
  unsigned trcd = 3;
  unsigned trp = 3;
  unsigned trc = 9;
  unsigned refresh_interval = 780;
  unsigned refresh_urgent = 40;               // 780+40 = 820 preempts requests
  unsigned max_burst_span = 19;               // conflict-read span bound (deferral)
  unsigned refresh_hard = 780 + 40 + 19 + 1;  // 840: preempts everything
};

// ===========================================================================
// zref::SdramController — cycle-stepped timing oracle
// ===========================================================================
// step() once per sdram cycle with the offered request; the returned Rsp is
// what the RTL reads during that cycle (grant high during cycle G etc.). The
// state machine mirrors the ctrl law table exactly:
//   G        : grant reads high; first command cycle of the burst
//   hit      : R/W at G          miss : ACT at G, R/W at G+3
//   conflict : PRE at G, ACT at G+3, R/W at G+6
//   read     : model beats [R+3, R+10]; rdata_valid [R+4, R+3+words]
//   write    : beats [R+1, R+8] (tail DQM-masked >= words)
//   refresh  : PRE-ALL at P, REF at P+3, released at P+12
struct SdramReq {
  bool valid = false;
  bool write = false;
  uint32_t addr = 0;   // byte address (27 bits used)
  unsigned words = 8;  // 1..8 (burst split is the caller's/arbiter's job)
};

struct SdramRsp {
  bool grant = false;    // reads high during cycle G
  unsigned credits = 0;  // retired word count, one cycle after the
                         // burst's last DRAM beat
  bool rdata_valid = false;
  uint16_t rdata = 0;
  bool wr_beat = false;   // beat request: wdata for word `beat_idx`
  unsigned beat_idx = 0;  // is sampled at the END of this cycle
  bool refresh_pulse = false;
};

class SdramController {
 public:
  struct Counters {
    uint32_t refresh_stalls = 0;
    uint32_t bank_conflicts = 0;
    uint32_t refreshes = 0;
  };

  explicit SdramController(SdramProfile p = SdramProfile()) : prof_(p) { reset(); }

  void reset() {
    st_ = S_INIT_PRE;
    t_ = 0;
    beat_ = 0;
    cnt_ = 0;
    init_done_ = false;
    std::memset(open_valid_, 0, sizeof(open_valid_));
    for (auto& r : open_row_) r = 0;
    cur_ = {};
    grant_q_ = false;
    credits_v_ = false;
    credits_n_ = 0;
    rdata_v_ = false;
    rdata_ = 0;
    wr_beat_v_ = false;
    beat_out_ = 0;
    refresh_pulse_ = false;
    ctr_ = {};
  }

  const Counters& counters() const { return ctr_; }
  unsigned refresh_count() const { return cnt_; }
  bool init_done() const { return init_done_; }

  // registered outputs DURING the current cycle (call before edge())
  SdramRsp now() const {
    SdramRsp rsp;
    rsp.grant = grant_q_;
    rsp.credits = credits_v_ ? credits_n_ : 0;
    rsp.rdata_valid = rdata_v_;
    rsp.rdata = rdata_;
    rsp.wr_beat = wr_beat_v_;
    rsp.beat_idx = beat_out_;
    // refresh_pulse is COMBINATIONAL in the RTL (decoded from the state
    // during the cycle); compute it the same way, not from the register
    rsp.refresh_pulse = init_done_ && (st_ == S_REF_PRE) && (t_ == prof_.trp);
    return rsp;
  }

  // advance one clock edge with the request offered DURING this cycle
  void edge(const SdramReq& req, bool hold_refresh = false) {
    decode(req, hold_refresh);
    seq(req);
  }

  // convenience: one full cycle (now + edge)
  SdramRsp step(const SdramReq& req, bool hold_refresh = false) {
    SdramRsp rsp = now();
    edge(req, hold_refresh);
    return rsp;
  }

 private:
  enum St {
    S_INIT_PRE,
    S_INIT_RP,
    S_INIT_REF1,
    S_INIT_RC1,
    S_INIT_REF2,
    S_INIT_RC2,
    S_INIT_MRS,
    S_READY,
    S_PRE,
    S_ACT,
    S_TRCD,
    S_RW,
    S_CAS,
    S_RDATA,
    S_WDATA,
    S_REF_PRE
  };
  struct Cur {
    bool write = false;
    unsigned bank = 0;
    unsigned row = 0;  // 13 bits
    unsigned col = 0;  // 11 bits
    unsigned words = 8;
  };

  SdramProfile prof_;  // by value: a default-argument reference would dangle
  St st_;
  unsigned t_, beat_;
  unsigned cnt_;
  bool init_done_;
  bool open_valid_[4];
  unsigned open_row_[4];
  Cur cur_;
  bool grant_q_, credits_v_, rdata_v_, wr_beat_v_, refresh_pulse_;
  unsigned credits_n_, rdata_, beat_out_;
  Counters ctr_;

  // combinational decode for the END of the current cycle
  bool cmd_ref_ = false;
  bool accept_ = false;
  bool refresh_now_ = false;
  bool retire_ = false;

  void decode(const SdramReq& req, bool hold_refresh) {
    cmd_ref_ = (st_ == S_INIT_REF1 || st_ == S_INIT_REF2) && t_ == 0;
    cmd_ref_ = cmd_ref_ || (st_ == S_REF_PRE && t_ == prof_.trp);
    const bool pend = cnt_ >= prof_.refresh_interval && cnt_ < prof_.refresh_hard;
    const bool urg =
        cnt_ >= (prof_.refresh_interval + prof_.refresh_urgent) && cnt_ < prof_.refresh_hard;
    const bool hard = cnt_ >= prof_.refresh_hard;
    refresh_now_ = (st_ == S_READY) && (hard || (urg && !hold_refresh) || (pend && !req.valid));
    accept_ = (st_ == S_READY) && req.valid && !refresh_now_;
    retire_ = (st_ == S_RDATA && beat_ == cur_.words - 1) || (st_ == S_WDATA && beat_ == 7);
    // reads occupy the DQ bus for the full 8-beat SDR burst even when
    // the client consumes fewer words (bus shaping, ctrl law table)
  }

  void seq(const SdramReq& req) {
    // registered output defaults for NEXT cycle
    const bool grant_next = accept_;
    const bool credits_next = retire_;
    const unsigned credits_n_next = cur_.words;
    grant_q_ = grant_next;
    credits_v_ = credits_next;
    credits_n_ = credits_n_next;
    refresh_pulse_ = cmd_ref_ && init_done_;
    wr_beat_v_ = false;
    rdata_v_ = false;

    // wr_beat next-cycle value (RTL: wr_beat is combinational on state;
    // the RTL's S_RW/S_WDATA beat logic)
    // computed below after the state advance

    // refresh counter
    if (st_ == S_INIT_MRS)
      cnt_ = 0;
    else if (init_done_ && cmd_ref_)
      cnt_ = 0;
    else if (init_done_)
      cnt_ = cnt_ + 1;

    if (st_ == S_REF_PRE && req.valid) ctr_.refresh_stalls++;
    if (cmd_ref_ && init_done_) ctr_.refreshes++;

    // state advance (edge)
    St next = st_;
    unsigned t_next = t_;
    unsigned beat_next = beat_;
    Cur cur_next = cur_;
    switch (st_) {
      case S_INIT_PRE:
        next = S_INIT_RP;
        t_next = 0;
        break;
      case S_INIT_RP:
        if (t_ == prof_.trp - 1) {
          next = S_INIT_REF1;
          t_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_INIT_REF1:
        if (t_ == prof_.trc - 1) {
          next = S_INIT_RC1;
          t_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_INIT_RC1:
        next = S_INIT_REF2;
        t_next = 0;
        break;
      case S_INIT_REF2:
        if (t_ == prof_.trc - 1) {
          next = S_INIT_RC2;
          t_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_INIT_RC2:
        next = S_INIT_MRS;
        t_next = 0;
        break;
      case S_INIT_MRS:
        next = S_READY;
        t_next = 0;
        init_done_ = true;
        break;
      case S_READY:
        t_next = 0;
        beat_next = 0;
        if (refresh_now_) {
          next = S_REF_PRE;
        } else if (accept_) {
          grant_q_ = true;  // (grant_next already true)
          const uint32_t wa = req.addr >> 1;
          cur_next.write = req.write;
          cur_next.bank = (wa >> 24) & 3;
          cur_next.row = (wa >> 11) & 0x1FFF;
          cur_next.col = wa & 0x7FF;
          cur_next.words = req.words == 0 ? 8 : req.words;
          if (open_valid_[cur_next.bank] && open_row_[cur_next.bank] != cur_next.row) {
            ctr_.bank_conflicts++;
            open_valid_[cur_next.bank] = false;
            next = S_PRE;
          } else if (!open_valid_[cur_next.bank]) {
            next = S_ACT;
          } else {
            next = S_RW;
          }
        }
        break;
      case S_PRE:
        if (t_ == prof_.trp - 1) {
          next = S_ACT;
          t_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_ACT:
        open_valid_[cur_.bank] = true;
        open_row_[cur_.bank] = cur_.row;
        t_next = 0;
        beat_next = 0;
        next = S_TRCD;
        break;
      case S_TRCD:
        if (t_ == prof_.trcd - 2) {
          next = S_RW;
          t_next = 0;
          beat_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_RW:
        t_next = 0;
        beat_next = 0;
        next = cur_.write ? S_WDATA : S_CAS;
        break;
      case S_CAS:
        if (t_ == prof_.cas - 2) {
          next = S_RDATA;
          t_next = 0;
        } else
          t_next = t_ + 1;
        break;
      case S_RDATA:
        // full 8-beat bus burst; the client samples `words` beats
        if (beat_ == 7) {
          next = S_READY;
          t_next = 0;
          beat_next = 0;
        } else
          beat_next = beat_ + 1;
        break;
      case S_WDATA:
        if (beat_ == 7) {
          next = S_READY;
          t_next = 0;
          beat_next = 0;
        } else
          beat_next = beat_ + 1;
        break;
      case S_REF_PRE:
        if (t_ == prof_.trc + prof_.trp - 1) {
          next = S_READY;
          t_next = 0;
          open_valid_[0] = open_valid_[1] = false;
          open_valid_[2] = open_valid_[3] = false;
        } else
          t_next = t_ + 1;
        break;
    }

    // registered data outputs for NEXT cycle (rdata_valid spans the
    // `words` sampled beats only; the data VALUE is owned by the memory
    // model — the differential tests compare model/shadow, not rdata)
    if (st_ == S_RDATA && beat_ < cur_.words) {
      rdata_v_ = true;
      rdata_ = 0;
    }

    // wr_beat during NEXT cycle (RTL combinational on next state):
    // S_RW(write) requests word 0; S_WDATA beat i requests word i+1
    const unsigned words_cur = cur_.words;  // burst in flight next cycle
    const bool cur_write = cur_.write;
    if (next == S_RW && cur_write && cur_next_valid(next, cur_next)) {
      wr_beat_v_ = true;
      beat_out_ = 0;
    } else if (next == S_WDATA && cur_write && beat_next + 1 < words_cur) {
      wr_beat_v_ = true;
      beat_out_ = beat_next + 1;
    }

    st_ = next;
    t_ = t_next;
    beat_ = beat_next;
    cur_ = cur_next;
  }

  bool cur_next_valid(St, const Cur&) const { return true; }
};

// ===========================================================================
// zref::VramArbiter — the D3 grant-sequence oracle
// ===========================================================================
struct ArbClientReq {
  bool valid = false;
  bool write = false;
  uint32_t addr = 0;  // byte address
  unsigned len = 0;   // BYTES 1..64 (zhao_pkg port law)
};

struct ArbClientRsp {
  bool grant = false;
  unsigned credits = 0;
};

class VramArbiter {
 public:
  static constexpr unsigned NCLIENTS = 5;  // scanout,blit,eng0,eng1,debug
  static constexpr unsigned AGING_OVERRIDE = 20;
  static constexpr unsigned CREDIT_INIT = 32;

  struct Trace {  // one entry per ctrl-accepted burst
    uint64_t cycle;
    unsigned client;
    uint32_t addr;
    unsigned words;
  };

  struct SdramPort {  // what the arbiter offers the controller
    bool valid = false;
    bool write = false;
    uint32_t addr = 0;
    unsigned words = 0;  // 1..8
  };

  void reset() {
    for (unsigned k = 0; k < NCLIENTS; k++) {
      pend_[k] = {};
      credits_[k] = CREDIT_INIT;
      age_[k] = 0;
      bytes_[k] = 0;
    }
    rr_ = 1;
    last_issuer_ = 0;
    scanout_preempted_ = 0;
    cycle_ = 0;
    sel_ = -1;
    sel_bw_ = 0;
    sel_override_ = false;
    offer_ = {};
    offer_client_ = 0;
    offer_words_ = 0;
    offer_hold_ = false;
    offer_sup_ = false;
    for (unsigned k = 0; k < NCLIENTS; k++) grant_q_[k] = false;
    trace_.clear();
  }

  explicit VramArbiter() { reset(); }

  // ---- phase 1 (current cycle): the REGISTERED, stable offer ------------
  // The RTL latches the offered burst and holds it until the controller
  // accepts (grant reads high one cycle after acceptance); a purely
  // recomputed offer could change underneath an acceptance. offer()
  // returns the CURRENT registered offer (no state change); the internal
  // selection below feeds the same-edge latch in edge().
  const SdramPort& offer() const { return offer_; }
  bool hold_now() const { return offer_.valid && offer_hold_; }

  unsigned offer_client() const { return offer_client_; }
  unsigned offer_words() const { return offer_words_; }

  // compute the raw selection from the pre-edge state (called by edge())
  void select_raw() {
    bool eligible[NCLIENTS];
    for (unsigned k = 0; k < NCLIENTS; k++) eligible[k] = pend_[k].active && pend_[k].words != 0;

    int ov = -1;
    for (unsigned k = 0; k < 4; k++)
      if (eligible[k] && age_[k] >= AGING_OVERRIDE) {
        ov = int(k);
        break;
      }

    sel_ = -1;
    sel_override_ = false;
    if (ov >= 0) {
      sel_ = ov;
      sel_override_ = true;
    } else if (eligible[0])
      sel_ = 0;
    else {
      unsigned c = rr_;
      for (unsigned kk = 0; kk < 3 && sel_ < 0; kk++) {
        if (eligible[c]) sel_ = int(c);
        c = (c == 3) ? 1 : c + 1;
      }
      if (sel_ < 0 && eligible[4]) sel_ = 4;
    }
    sel_bw_ = (sel_ >= 0) ? burst_words(pend_[sel_].words, pend_[sel_].addr) : 0;
  }

  // client responses DURING the current cycle: grant pulses registered at
  // the previous edge; credits routed to the issuing client
  bool grant_now(unsigned k) const { return grant_q_[k]; }
  unsigned dbg_age(unsigned k) const { return age_[k]; }  // test tap
  unsigned dbg_credits(unsigned k) const { return credits_[k]; }
  bool dbg_pend_active(unsigned k) const { return pend_[k].active; }
  unsigned dbg_pend_words(unsigned k) const { return pend_[k].words; }
  unsigned dbg_offer_client() const { return offer_client_; }
  bool dbg_offer_valid() const { return offer_.valid; }
  unsigned credits_now(unsigned k, unsigned ctrl_credits) const {
    return (ctrl_credits && last_issuer_ == k) ? ctrl_credits : 0;
  }

  // ---- phase 2: advance one clock edge -----------------------------------
  // ctrl_grant/ctrl_credits are the controller's outputs DURING the cycle
  // that offer() was computed for.
  void edge(const ArbClientReq* req, bool ctrl_grant, unsigned ctrl_credits) {
    // PRE-EDGE eligibility snapshot: the RTL's aging/selection evaluate
    // the OLD pend registers (nonblocking); pend_ mutates below
    bool elig_pre[NCLIENTS];
    for (unsigned k = 0; k < NCLIENTS; k++) elig_pre[k] = pend_[k].active && pend_[k].words != 0;
    select_raw();
    // snapshot the raw selection BEFORE pend_ mutates below (the RTL
    // latches the offer from the pre-edge state, nonblocking-style)
    const int s = sel_;
    const unsigned s_bw = sel_bw_;
    const bool s_ov = sel_override_;
    const bool s_write = (s >= 0) ? pend_[s].write : false;
    const uint32_t s_addr = (s >= 0) ? pend_[s].addr : 0;
    const bool grant_now = ctrl_grant && offer_.valid;
    const unsigned served = grant_now ? offer_client_ : 99u;

    unsigned ret_words[NCLIENTS];
    for (unsigned k = 0; k < NCLIENTS; k++)
      ret_words[k] = (ctrl_credits && last_issuer_ == k) ? ctrl_credits : 0;

    for (unsigned k = 0; k < NCLIENTS; k++) {
      const unsigned wk = words_of(req[k].len);
      grant_q_[k] = req[k].valid && !pend_[k].active && wk != 0 && credits_[k] + ret_words[k] >= wk;
      credits_[k] += ret_words[k];
      if (grant_q_[k]) {
        credits_[k] -= wk;
        pend_[k].active = true;
        pend_[k].write = req[k].write;
        pend_[k].addr = req[k].addr;
        pend_[k].words = wk;
        bytes_[k] += req[k].len;
        if (bytes_[k] > 0xFFFFFFFFULL) bytes_[k] = 0xFFFFFFFFULL;
      } else if (grant_now && offer_client_ == k) {
        pend_[k].words -= offer_words_;
        pend_[k].addr += offer_words_ * 2;
        if (pend_[k].words == 0) pend_[k].active = false;
      }
    }

    if (grant_now) {
      if (offer_client_ >= 1 && offer_client_ <= 3) {
        unsigned n = offer_client_;
        rr_ = (n == 3) ? 1 : n + 1;
      }
      if (offer_client_ != 0 && !offer_hold_ && offer_sup_) scanout_preempted_++;
      last_issuer_ = offer_client_;
      trace_.push_back(Trace{cycle_, offer_client_, offer_.addr, offer_words_});
      offer_.valid = false;  // consumed; the NEXT edge re-latches from
                             // the updated pend (no same-edge re-offer)
    }
    if (s >= 0 && !offer_.valid && !grant_now) {
      offer_.valid = true;
      offer_.write = s_write;
      offer_.addr = s_addr;
      offer_.words = s_bw;
      offer_client_ = unsigned(s);
      offer_words_ = s_bw;
      offer_hold_ = s_ov;
      offer_sup_ = !s_ov && (s != 0) && pend_[0].active && pend_[0].words != 0;
    }

    // aging: resets when idle or when the edge served the client
    for (unsigned k = 0; k < NCLIENTS; k++) {
      if (elig_pre[k] && k != served) {
        if (age_[k] < 63) age_[k]++;
      } else
        age_[k] = 0;
    }
    cycle_++;
  }

  // convenience: one full cycle (offer + edge)
  void step(const ArbClientReq* req, bool ctrl_grant, unsigned ctrl_credits, ArbClientRsp* rsp,
            SdramPort& out) {
    out = offer();
    if (rsp) {
      for (unsigned k = 0; k < NCLIENTS; k++) {
        rsp[k].grant = grant_now(k);
        rsp[k].credits = credits_now(k, ctrl_credits);
      }
    }
    edge(req, ctrl_grant, ctrl_credits);
  }

  bool hold_refresh() const { return hold_now(); }  // (legacy name)
  uint64_t scanout_preempted() const { return scanout_preempted_; }
  uint64_t bytes_by_client(unsigned c) const { return bytes_[c]; }
  const std::vector<Trace>& trace() const { return trace_; }
  uint64_t cycle() const { return cycle_; }

  static unsigned words_of(unsigned len_bytes) {
    if (len_bytes == 0 || len_bytes > 64) return 0;
    return (len_bytes + 1) / 2;
  }

  static unsigned burst_words(unsigned rem, uint32_t addr) {
    const unsigned col = (addr >> 1) & 0x7FF;
    const unsigned row_tail = 2048 - col;
    if (row_tail >= rem) return rem >= 8 ? 8 : rem;
    return row_tail >= 8 ? 8 : row_tail;
  }

 private:
  struct Pend {
    bool active = false;
    bool write = false;
    uint32_t addr = 0;
    unsigned words = 0;
  };
  Pend pend_[NCLIENTS];
  unsigned credits_[NCLIENTS];
  unsigned age_[NCLIENTS];
  uint64_t bytes_[NCLIENTS];
  unsigned rr_;
  unsigned last_issuer_;
  uint64_t scanout_preempted_;
  uint64_t cycle_;
  int sel_ = -1;
  unsigned sel_bw_ = 0;
  bool sel_override_ = false;
  unsigned offer_client_ = 0;
  unsigned offer_words_ = 0;
  bool offer_hold_ = false;
  bool offer_sup_ = false;
  bool grant_q_[NCLIENTS] = {};
  SdramPort offer_{};
  std::vector<Trace> trace_;
};

// ===========================================================================
// zref::MemoryGuard — Phase-2 region-map verdict oracle (memory_rules §5)
// ===========================================================================
// THE FRAMEBUFFER-WRITER LEASE. This was a blit-specific grant, and it becomes
// a lease that NAMES ITS WRITER, because there are now two blocks that write an
// inactive framebuffer slot: DEBUG.FRAMEBLIT and RASTER.FBWRITE.
//
// They share the SPATIAL window and not the TEMPORAL permission. A second
// overlapping region entry would have copied the same address law, cost more
// policy plumbing, and still not stopped the two writers corrupting each other.
// VIDEO.SLOTMGR already owns one lease at a time with a generation, so the
// lease is the natural place to say WHO may write.
//
// A v1 frame uses the renderer or DebugFrameBlit, never both.
struct GuardMap {
  // Writer identity. Kept as an enum rather than a bool so a third writer is a
  // compile error at every switch rather than a silently wrong comparison.
  enum Writer { WRITER_BLIT = 0, WRITER_ENGINE0 = 1 };

  bool valid = false;             // a framebuffer-write lease exists this frame
  unsigned blit_slot = 0;         // 0/1 -- the leased slot
  uint32_t blit_span = 0;         // granted bytes (canvas_bytes(mode))
  unsigned writer = WRITER_BLIT;  // which client the lease is held BY
};

constexpr uint32_t kFbSlot0Base = 0x00000000u;
// bank split (W2.7): slot 1 lives in DRAM bank 1 — zhao_pkg ZHAO_FB_SLOT1_BASE
constexpr uint32_t kFbSlot1Base = 0x02000000u;
constexpr uint32_t kFbSlotSpan = 0x0003C000u;
// Phase-3 windows (zhao_pkg ZHAO_GEOM_ASSET_* / ZHAO_TERRAIN_PAGE_POOL_*).
// GEOM.ASSET_POOL is ENGINE1's, READ-only (spec/memory_rules.md 5f).
constexpr uint32_t kGeomAssetBase = 0x06A00000u;
constexpr uint32_t kGeomAssetSpan = 0x01600000u;  // 22 MiB, ends at 0x0800_0000
// TERRAIN.PAGE_POOL is TERRAIN.BUILD's, WRITE-only (ruling T2 / 5b).
// 1,024 x 21,376 B = 0x014E_0000, so the pool ends at 0x054E_0000 -- the
// ruling's inclusive 0x054D_FFFF, to the byte.
constexpr uint32_t kTerrainPagePoolBase = 0x04000000u;
constexpr uint32_t kTerrainPagePoolSpan = 0x014E0000u;

struct MemoryGuard {
  // client ids (zhao_client_e)
  // Client ids (zhao_client_e). 5 is the unspent reservation of ruling T3 and
  // is deliberately absent -- naming it here would be spending it.
  enum Client {
    SCANOUT = 0,
    BLIT_DMA = 1,
    ENGINE0 = 2,
    ENGINE1 = 3,
    DEBUG = 4,
    TERRAIN_BUILD = 6
  };

  struct Req {
    bool valid = false;
    bool write = false;
    unsigned client = SCANOUT;
    uint32_t addr = 0;
    unsigned len = 0;  // bytes 1..64
    uint64_t be = 0;   // full mask required
  };

  static bool verdict(const GuardMap& m, const Req& r) {
    if (!r.valid) return false;
    const bool len_ok = r.len >= 1 && r.len <= 64;
    uint64_t full = r.len == 64 ? ~0ULL : ((1ULL << r.len) - 1);
    if (!len_ok || r.be != full) return false;
    const uint32_t end = r.addr + r.len;  // 32-bit: cannot wrap the map
    switch (r.client) {
      case SCANOUT:
        // disjoint slots since the bank split (a <=64-B request
        // cannot bridge from slot 0 into slot 1)
        return !r.write && (end <= kFbSlot0Base + kFbSlotSpan ||
                            (r.addr >= kFbSlot1Base && end <= kFbSlot1Base + kFbSlotSpan));
      case BLIT_DMA:
      case ENGINE0: {
        // ONE WINDOW, ONE OWNER AT A TIME. Both writers are checked against the
        // same clamped slot span; what separates them is which one the lease
        // names. A writer without the lease is refused exactly as a request
        // outside the window is.
        if (!r.write || !m.valid) return false;
        const unsigned want =
            (r.client == BLIT_DMA) ? GuardMap::WRITER_BLIT : GuardMap::WRITER_ENGINE0;
        if (m.writer != want) return false;
        const uint32_t base = m.blit_slot ? kFbSlot1Base : kFbSlot0Base;
        return r.addr >= base && end <= base + m.blit_span;
      }
      case ENGINE1:
        // GEOM.ASSET_POOL, READ-only (spec/memory_rules.md 5f). This arm was
        // MISSING from the oracle while the RTL had it, so the model and the
        // block disagreed about every meshlet descriptor read. Nothing caught
        // it because mem_guard_directed's fuzz anchors sit in the framebuffer
        // range and its client draw is range(5) -- the divergence existed only
        // at addresses the test never generates. Added with the terrain arm
        // rather than left, because a reference that is right about the region
        // being added and wrong about the one beside it is not a reference.
        return !r.write && r.addr >= kGeomAssetBase &&
               end <= kGeomAssetBase + kGeomAssetSpan;
      case TERRAIN_BUILD:
        // TERRAIN.PAGE_POOL, WRITE-only (ruling T2 / 5b). Constant bounds: no
        // map input reaches this window, so unlike the framebuffer one it is
        // not frame-scoped. Spatially the whole pool -- T2's "a loader may
        // write only a LOADING slot" needs residency state the guard does not
        // have, and the RTL says so at `terrain_ok`.
        return r.write && r.addr >= kTerrainPagePoolBase &&
               end <= kTerrainPagePoolBase + kTerrainPagePoolSpan;
      default:
        return false;  // DEBUG owns nothing, and neither does the unspent 5
    }
  }
};

// ===========================================================================
// zref::HpsBridge — burst bookkeeping under the sim latency profile (D10)
// ===========================================================================
// Bridge register law: grant reads high 1 cycle after req.valid; the HPS
// request is presented the cycle after acceptance; write beats pass with one
// register stage; read beats arrive from the HPS `lat_to_first` cycles after
// the request grant, 1 beat/cycle, and pass to the client one cycle later.
struct HpsBridge {
  static constexpr unsigned LAT_TO_FIRST = 16;  // sim profile (frozen)
  static constexpr unsigned LAT_PER_BEAT = 1;

  struct Burst {
    unsigned client = 0;
    bool write = false;
    uint32_t addr = 0;
    unsigned len = 0;    // bytes 1..64, 64-B aligned addr
    uint64_t t_req = 0;  // cycle the request is first presented
  };

  struct Beat {
    uint64_t cycle;
    uint64_t data;
    bool last;
  };

  struct Result {
    bool malformed = false;
    uint64_t grant_cycle = 0;  // req_grant reads high
    std::vector<Beat> beats;   // read beats at the client port
    uint64_t bytes_by_client[5] = {0, 0, 0, 0, 0};
  };

  // transactional oracle: a strictly ordered list of bursts (one in flight
  // per client; the caller supplies legal spacing). Malformed bursts are
  // answered with a single err pulse the cycle after the request and issue
  // NOTHING.
  static Result run(const std::vector<Burst>& bursts) {
    Result res;
    HpsBridge b;
    for (const auto& bu : bursts) {
      if (bu.len == 0 || bu.len > 64 || (bu.addr & 0x3F) != 0) {
        res.malformed = true;
        continue;
      }
      const uint64_t t0 = b.free_at_ > bu.t_req ? b.free_at_ : bu.t_req;
      const uint64_t grant = t0 + 1;  // registered grant
      const unsigned nbeats = (bu.len + 7) / 8;
      if (bu.write) {
        // write beats stream after grant; completion at the last beat
        res.grant_cycle = grant;
        const uint64_t done = grant + 1 + nbeats - 1;  // hps sees them
        b.free_at_ = done + 1;
      } else {
        // read beats: first at grant+1+LAT_TO_FIRST (hps side) and
        // one register stage more to the client port
        for (unsigned i = 0; i < nbeats; i++) {
          res.beats.push_back(Beat{grant + 1 + LAT_TO_FIRST + i + 1, 0, i + 1 == nbeats});
        }
        b.free_at_ = grant + 1 + LAT_TO_FIRST + nbeats;
      }
      res.bytes_by_client[bu.client] += bu.len;
      b.free_at_ = b.free_at_ > res.grant_cycle ? b.free_at_ : res.grant_cycle;
    }
    return res;
  }

 private:
  uint64_t free_at_ = 0;
};

}  // namespace zref
