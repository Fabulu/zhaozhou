// proj_rowmux_directed.cpp — ROWS_PER_PASS=1 against ROWS_PER_PASS=3,
// byte-for-byte, under every stall pattern.
//
// The chain of evidence this test closes: the existing geom/terrain
// differential suites pin the DEFAULT core (ROWS_PER_PASS=3) to the shipped
// oracle `zref::render::project_vertex`; this test pins ROWS_PER_PASS=1 to
// ROWS_PER_PASS=3 as an output STREAM — same records, same order, every
// field — so the sequenced shape inherits the oracle by transitivity without
// this file re-implementing one line of projection arithmetic (which the
// repository forbids).
//
// What it asserts, by section:
//   1. stream equality under four stall patterns (free-running, periodic,
//      LFSR-random, bursty), each instance under its OWN pattern;
//   2. the latencies are FIXED, measured in en-cycles, and differ by exactly
//      the declared +3; the DUT's initiation interval is exactly 3 under a
//      saturated offer;
//   3. a configuration write landing MID-SEQUENCE (between a DUT vertex's row
//      cycles) does not tear the transform — the capture-at-accept law;
//   4. busy_o covers the sequencer's holding state (the queue-occupancy law);
//   5. POSITIVE CONTROL: a one-raw-bit skew in one DUT matrix word makes the
//      comparator fire. A checker that has not been seen to fail has not been
//      tested.
//
// The corpus deliberately includes full-s32 coordinates, both views,
// behind-the-eye vertices (w <= 0), rail-saturating rows and sub-bit-16
// matrix fractions — the terrain suite's "matrices too regular to exercise
// rounding" failure is the documented trap here.

#include "Vtb_proj_rowmux.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// This Verilator runtime's verilated.o references the legacy SystemC hook.
double sc_time_stamp() { return 0; }

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                   \
  do {                                                     \
    ++g_checks;                                            \
    if (!(cond)) {                                         \
      ++g_fails;                                           \
      std::printf("FAIL %s:%d: ", __FILE__, __LINE__);     \
      std::printf(__VA_ARGS__);                            \
      std::printf("\n");                                   \
    }                                                      \
  } while (0)

// ---------------------------------------------------------------------------
// stimulus
// ---------------------------------------------------------------------------
struct Vtx {
  int32_t x, y, z;
  bool view;
  uint16_t pay;
};

struct Rec {
  int32_t x, y, d;
  uint32_t w;
  bool behind, view;
  uint16_t pay;
  bool operator==(const Rec& o) const {
    return x == o.x && y == o.y && d == o.d && w == o.w && behind == o.behind &&
           view == o.view && pay == o.pay;
  }
};

struct Stream {
  std::vector<Rec> recs;
  std::vector<uint64_t> acc_stamp;  // en-cycle of each acceptance
  std::vector<uint64_t> out_stamp;  // en-cycle of each emission
};

static uint32_t xs32(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

// Stall patterns. 0: free-running. 1: periodic 2-on-1-off. 2: LFSR ~50%.
// 3: bursty 8-on-5-off.
static bool pat_en(int pat, uint64_t cyc, uint32_t& lfsr) {
  switch (pat) {
    case 0: return true;
    case 1: return (cyc % 3) != 2;
    case 2: return (xs32(lfsr) & 1u) != 0u;
    default: return (cyc % 13) < 8;
  }
}

static std::vector<Vtx> make_corpus(int n, uint32_t seed) {
  std::vector<Vtx> v;
  uint32_t s = seed;
  for (int k = 0; k < n; ++k) {
    Vtx t{};
    switch (k % 9) {
      case 0:  // full-width extremes: the contract's "ANY input" claim
        t.x = (k & 16) ? INT32_MIN : INT32_MAX;
        t.y = static_cast<int32_t>(xs32(s));
        t.z = (k & 32) ? INT32_MAX : INT32_MIN;
        break;
      case 1:  // small world coords, fractions live
        t.x = static_cast<int32_t>(xs32(s)) >> 12;
        t.y = static_cast<int32_t>(xs32(s)) >> 12;
        t.z = static_cast<int32_t>(xs32(s)) >> 12;
        break;
      case 2:  // near-plane pressure: large negative z drives w <= 0
        t.x = static_cast<int32_t>(xs32(s)) >> 8;
        t.y = static_cast<int32_t>(xs32(s)) >> 8;
        t.z = -(static_cast<int32_t>(xs32(s) >> 1));
        break;
      default:  // full random spread
        t.x = static_cast<int32_t>(xs32(s)) >> (xs32(s) % 14u);
        t.y = static_cast<int32_t>(xs32(s)) >> (xs32(s) % 14u);
        t.z = static_cast<int32_t>(xs32(s)) >> (xs32(s) % 14u);
        break;
    }
    t.view = ((k / 3) & 1) != 0;
    t.pay = static_cast<uint16_t>(k);
    v.push_back(t);
  }
  return v;
}

// ---------------------------------------------------------------------------
// DUT plumbing
// ---------------------------------------------------------------------------
static void settle0(Vtb_proj_rowmux* tb) {
  tb->clk = 0;
  tb->eval();
}
static void edge(Vtb_proj_rowmux* tb) {
  tb->clk = 1;
  tb->eval();
}

static void reset(Vtb_proj_rowmux* tb) {
  tb->rst_n = 0;
  tb->r_en_i = 1;
  tb->d_en_i = 1;
  tb->r_in_valid_i = 0;
  tb->d_in_valid_i = 0;
  tb->r_cfg_we_i = 0;
  tb->d_cfg_we_i = 0;
  for (int i = 0; i < 4; ++i) {
    settle0(tb);
    edge(tb);
  }
  tb->rst_n = 1;
  settle0(tb);
  edge(tb);
}

// One matrix+viewport set, written to one instance's cfg bus. `skew_addr`,
// if 0..17, adds +1 raw to that view-0 word — the positive control's knife.
struct Cfg {
  int32_t m0[16], m1[16];
  uint32_t vp0[2], vp1[2];
};

static Cfg make_cfg(uint32_t seed) {
  Cfg c{};
  uint32_t s = seed;
  for (int k = 0; k < 16; ++k) {
    // pose-plausible 3x3 with LIVE fraction bits (the rounding wall), a
    // translation that carries range, and a w row that varies with z.
    c.m0[k] = static_cast<int32_t>(xs32(s)) >> 14;
    c.m1[k] = static_cast<int32_t>(xs32(s)) >> 14;
  }
  c.m0[3] = static_cast<int32_t>(xs32(s)) >> 6;   // translations
  c.m0[7] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m1[3] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m1[7] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m0[12] = static_cast<int32_t>(xs32(s)) >> 18;  // w row: small, nonzero
  c.m0[13] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m0[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;  // varies with z
  c.m0[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  c.m1[12] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m1[13] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m1[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;
  c.m1[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  // row-2 words are inert by construction; write junk there to prove it rides
  // along in BOTH shapes identically (it must affect neither).
  for (int k = 8; k < 12; ++k) {
    c.m0[k] = static_cast<int32_t>(xs32(s));
    c.m1[k] = static_cast<int32_t>(xs32(s));
  }
  // viewports differing on BOTH axes between views (the dual-view lesson).
  c.vp0[0] = (17u << 16) | 32u;    // {y0, x0}
  c.vp0[1] = (240u << 16) | 320u;  // {h, w}
  c.vp1[0] = (90u << 16) | 5u;
  c.vp1[1] = (192u << 16) | 256u;
  return c;
}

static void write_cfg_word(Vtb_proj_rowmux* tb, bool dut, bool view,
                           uint32_t addr, uint32_t data) {
  if (dut) {
    tb->d_cfg_we_i = 1;
    tb->d_cfg_view_i = view;
    tb->d_cfg_addr_i = addr;
    tb->d_cfg_data_i = data;
  } else {
    tb->r_cfg_we_i = 1;
    tb->r_cfg_view_i = view;
    tb->r_cfg_addr_i = addr;
    tb->r_cfg_data_i = data;
  }
  settle0(tb);
  edge(tb);
  tb->r_cfg_we_i = 0;
  tb->d_cfg_we_i = 0;
}

static void load_cfg(Vtb_proj_rowmux* tb, const Cfg& c, int skew_dut_addr) {
  for (int inst = 0; inst < 2; ++inst) {
    const bool dut = (inst == 1);
    for (uint32_t k = 0; k < 16; ++k) {
      uint32_t w0 = static_cast<uint32_t>(c.m0[k]);
      if (dut && static_cast<int>(k) == skew_dut_addr) w0 += 1u;
      write_cfg_word(tb, dut, false, k, w0);
      write_cfg_word(tb, dut, true, k, static_cast<uint32_t>(c.m1[k]));
    }
    write_cfg_word(tb, dut, false, 16, c.vp0[0]);
    write_cfg_word(tb, dut, false, 17, c.vp0[1]);
    write_cfg_word(tb, dut, true, 16, c.vp1[0]);
    write_cfg_word(tb, dut, true, 17, c.vp1[1]);
  }
}

// Drive both instances through one corpus, each under its own stall pattern.
// If `mid_write_at >= 0`, then on the cycle after acceptance number
// `mid_write_at` (0-based) each instance receives a cfg write of
// view0/addr0 <- `mid_write_val` while its offer is held back one cycle —
// for the DUT that lands with the row sequencer mid-vertex, which is exactly
// the tear window the capture law must close.
struct SideState {
  size_t next = 0;                 // next corpus index to offer
  uint64_t en_count = 0;           // en-cycles elapsed
  bool mid_pending = false;        // cfg write scheduled for this cycle
  bool mid_done = false;
  Stream out;
};

static void run_streams(Vtb_proj_rowmux* tb, const std::vector<Vtx>& corpus,
                        int pat_ref, int pat_dut, int mid_write_at,
                        uint32_t mid_write_val, Stream& ref_out,
                        Stream& dut_out) {
  SideState R, D;
  uint32_t lfsr_r = 0xACE1u, lfsr_d = 0xBEEFu;
  uint64_t cyc = 0;
  const uint64_t kTimeout = 400000;

  while ((R.out.recs.size() < corpus.size() ||
          D.out.recs.size() < corpus.size()) &&
         cyc < kTimeout) {
    const bool en_r = pat_en(pat_ref, cyc, lfsr_r);
    const bool en_d = pat_en(pat_dut, cyc, lfsr_d);
    tb->r_en_i = en_r;
    tb->d_en_i = en_d;

    // cfg mid-write, scheduled the cycle after the trigger acceptance
    tb->r_cfg_we_i = 0;
    tb->d_cfg_we_i = 0;
    if (R.mid_pending) {
      tb->r_cfg_we_i = 1;
      tb->r_cfg_view_i = 0;
      tb->r_cfg_addr_i = 0;
      tb->r_cfg_data_i = mid_write_val;
    }
    if (D.mid_pending) {
      tb->d_cfg_we_i = 1;
      tb->d_cfg_view_i = 0;
      tb->d_cfg_addr_i = 0;
      tb->d_cfg_data_i = mid_write_val;
    }

    // offer: hold the same vertex until accepted; hold OFF while a mid-write
    // is landing so the write orders identically against the stream on both.
    const bool offer_r = (R.next < corpus.size()) && !R.mid_pending;
    const bool offer_d = (D.next < corpus.size()) && !D.mid_pending;
    tb->r_in_valid_i = offer_r;
    tb->d_in_valid_i = offer_d;
    if (offer_r) {
      const Vtx& t = corpus[R.next];
      tb->r_vx_i = static_cast<uint32_t>(t.x);
      tb->r_vy_i = static_cast<uint32_t>(t.y);
      tb->r_vz_i = static_cast<uint32_t>(t.z);
      tb->r_view_i = t.view;
      tb->r_payload_i = t.pay;
    }
    if (offer_d) {
      const Vtx& t = corpus[D.next];
      tb->d_vx_i = static_cast<uint32_t>(t.x);
      tb->d_vy_i = static_cast<uint32_t>(t.y);
      tb->d_vz_i = static_cast<uint32_t>(t.z);
      tb->d_view_i = t.view;
      tb->d_payload_i = t.pay;
    }

    settle0(tb);

    // sample DURING the cycle, before the edge — what the flops will see
    const bool acc_r = en_r && offer_r && tb->r_in_ready_o;
    const bool acc_d = en_d && offer_d && tb->d_in_ready_o;
    const bool emit_r = en_r && tb->r_out_valid_o;
    const bool emit_d = en_d && tb->d_out_valid_o;
    if (emit_r) {
      Rec rec{};
      rec.x = static_cast<int32_t>(tb->r_out_x_o << 11) >> 11;
      rec.y = static_cast<int32_t>(tb->r_out_y_o << 11) >> 11;
      rec.d = static_cast<int32_t>(tb->r_out_d_o);
      rec.w = tb->r_out_w_o;
      rec.behind = tb->r_out_behind_o;
      rec.view = tb->r_out_view_o;
      rec.pay = tb->r_out_payload_o;
      R.out.recs.push_back(rec);
      R.out.out_stamp.push_back(R.en_count);
    }
    if (emit_d) {
      Rec rec{};
      rec.x = static_cast<int32_t>(tb->d_out_x_o << 11) >> 11;
      rec.y = static_cast<int32_t>(tb->d_out_y_o << 11) >> 11;
      rec.d = static_cast<int32_t>(tb->d_out_d_o);
      rec.w = tb->d_out_w_o;
      rec.behind = tb->d_out_behind_o;
      rec.view = tb->d_out_view_o;
      rec.pay = tb->d_out_payload_o;
      D.out.recs.push_back(rec);
      D.out.out_stamp.push_back(D.en_count);
    }

    edge(tb);

    if (acc_r) {
      R.out.acc_stamp.push_back(R.en_count);
      if (mid_write_at >= 0 && !R.mid_done &&
          R.next == static_cast<size_t>(mid_write_at)) {
        R.mid_pending = true;
      }
      ++R.next;
    } else if (R.mid_pending) {
      R.mid_pending = false;  // the write landed on this edge
      R.mid_done = true;
    }
    if (acc_d) {
      D.out.acc_stamp.push_back(D.en_count);
      if (mid_write_at >= 0 && !D.mid_done &&
          D.next == static_cast<size_t>(mid_write_at)) {
        D.mid_pending = true;
      }
      ++D.next;
    } else if (D.mid_pending) {
      D.mid_pending = false;
      D.mid_done = true;
    }
    if (en_r) ++R.en_count;
    if (en_d) ++D.en_count;
    ++cyc;
  }

  CHECK(cyc < kTimeout, "stream run timed out at %llu cycles (ref %zu/%zu, dut %zu/%zu)",
        static_cast<unsigned long long>(cyc), R.out.recs.size(), corpus.size(),
        D.out.recs.size(), corpus.size());
  ref_out = R.out;
  dut_out = D.out;
}

// Elementwise stream compare. Returns the mismatch count; when `report` is
// true every mismatch is a CHECK failure that prints both records.
static int compare_streams(const Stream& a, const Stream& b, bool report,
                           const char* tag) {
  int mm = 0;
  if (report) {
    CHECK(a.recs.size() == b.recs.size(), "%s: stream lengths %zu vs %zu", tag,
          a.recs.size(), b.recs.size());
  }
  const size_t n = a.recs.size() < b.recs.size() ? a.recs.size() : b.recs.size();
  for (size_t i = 0; i < n; ++i) {
    if (!(a.recs[i] == b.recs[i])) {
      ++mm;
      if (report && mm <= 4) {
        CHECK(false,
              "%s: record %zu differs: ref{x=%d y=%d d=%d w=%u b=%d v=%d p=%u} "
              "dut{x=%d y=%d d=%d w=%u b=%d v=%d p=%u}",
              tag, i, a.recs[i].x, a.recs[i].y, a.recs[i].d, a.recs[i].w,
              a.recs[i].behind, a.recs[i].view, a.recs[i].pay, b.recs[i].x,
              b.recs[i].y, b.recs[i].d, b.recs[i].w, b.recs[i].behind,
              b.recs[i].view, b.recs[i].pay);
      }
    }
  }
  if (report) CHECK(mm == 0, "%s: %d mismatching records", tag, mm);
  return mm;
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* tb = new Vtb_proj_rowmux;

  const int kN = 400;
  const std::vector<Vtx> corpus = make_corpus(kN, 0xC0FFEEu);
  const Cfg cfg = make_cfg(0x5EED5EEDu);

  // ---- section 1: stream equality under four stall-pattern pairs ----------
  // The golden stream is the ROWS_PER_PASS=3 instance free-running; every
  // other (instance, pattern) stream must equal it byte for byte.
  Stream golden, s_ref, s_dut;
  {
    reset(tb);
    load_cfg(tb, cfg, -1);
    run_streams(tb, corpus, /*ref*/ 0, /*dut*/ 0, -1, 0, golden, s_dut);
    CHECK(golden.recs.size() == static_cast<size_t>(kN),
          "golden stream short: %zu of %d", golden.recs.size(), kN);
    compare_streams(golden, s_dut, true, "P0/P0");
  }
  for (int pat = 1; pat <= 3; ++pat) {
    reset(tb);
    load_cfg(tb, cfg, -1);
    // ref under its own different pattern too: stall-invariance both sides
    run_streams(tb, corpus, pat == 2 ? 3 : 2, pat, -1, 0, s_ref, s_dut);
    char tag[32];
    std::snprintf(tag, sizeof tag, "ref-pat/P%d", pat);
    compare_streams(golden, s_ref, true, tag);
    std::snprintf(tag, sizeof tag, "dut-P%d", pat);
    compare_streams(golden, s_dut, true, tag);
  }

  // corpus honesty: the near plane and the live path must BOTH be present,
  // or the equality above proved less than it claims.
  {
    int behind = 0, live = 0;
    for (const Rec& r : golden.recs) (r.behind ? behind : live)++;
    CHECK(behind > 0, "corpus never crossed the near plane");
    CHECK(live > 0, "corpus never produced a live vertex");
  }

  // ---- section 2: fixed latency, +3, and the initiation interval ----------
  {
    reset(tb);
    load_cfg(tb, cfg, -1);
    run_streams(tb, corpus, 0, 0, -1, 0, s_ref, s_dut);
    CHECK(s_ref.acc_stamp.size() == s_ref.out_stamp.size(),
          "ref accept/emit count skew");
    CHECK(s_dut.acc_stamp.size() == s_dut.out_stamp.size(),
          "dut accept/emit count skew");
    uint64_t l3 = s_ref.out_stamp[0] - s_ref.acc_stamp[0];
    uint64_t l1 = s_dut.out_stamp[0] - s_dut.acc_stamp[0];
    bool l3_fixed = true, l1_fixed = true, ii3 = true;
    for (size_t i = 0; i < s_ref.out_stamp.size(); ++i) {
      if (s_ref.out_stamp[i] - s_ref.acc_stamp[i] != l3) l3_fixed = false;
    }
    for (size_t i = 0; i < s_dut.out_stamp.size(); ++i) {
      if (s_dut.out_stamp[i] - s_dut.acc_stamp[i] != l1) l1_fixed = false;
    }
    for (size_t i = 1; i < s_dut.acc_stamp.size(); ++i) {
      if (s_dut.acc_stamp[i] - s_dut.acc_stamp[i - 1] != 3) ii3 = false;
    }
    CHECK(l3_fixed, "ROWS_PER_PASS=3 latency is not fixed");
    CHECK(l1_fixed, "ROWS_PER_PASS=1 latency is not fixed");
    CHECK(l1 == l3 + 3,
          "latency delta: measured L1=%llu L3=%llu, declared L1=L3+3",
          static_cast<unsigned long long>(l1),
          static_cast<unsigned long long>(l3));
    CHECK(ii3, "ROWS_PER_PASS=1 initiation interval is not exactly 3 under saturation");
    std::printf("  measured: L(RPP=3)=%llu en-cycles, L(RPP=1)=%llu, II(RPP=1)=3\n",
                static_cast<unsigned long long>(l3),
                static_cast<unsigned long long>(l1));
  }

  // ---- section 3: a cfg write mid-sequence cannot tear a vertex -----------
  // The write lands one en-cycle after acceptance K on each instance — for
  // the DUT that is the row sequencer's SeqMx cycle, mid-vertex. Vertices
  // 0..K must project through the OLD word and K+1.. through the NEW one, on
  // both instances identically.
  {
    Stream m_ref, m_dut;
    reset(tb);
    load_cfg(tb, cfg, -1);
    const uint32_t new_m0 = static_cast<uint32_t>(cfg.m0[0]) ^ 0x00010001u;
    run_streams(tb, corpus, 0, 0, /*mid at accept #*/ 7, new_m0, m_ref, m_dut);
    compare_streams(m_ref, m_dut, true, "cfg-mid-sequence");
    // and the write itself must have MATTERED, or this section tested nothing
    CHECK(compare_streams(golden, m_ref, false, "") > 0,
          "mid-sequence cfg write changed no record; the section is inert");
  }

  // ---- section 4: busy_o covers the sequencer's holding state -------------
  {
    reset(tb);
    load_cfg(tb, cfg, -1);
    // one vertex, free-running; busy must be high from the accept edge to the
    // emission edge inclusive, and low two cycles after the record is taken.
    tb->d_en_i = 1;
    tb->r_en_i = 1;
    tb->d_in_valid_i = 1;
    tb->d_vx_i = 0x1234u;
    tb->d_vy_i = 0x2345u;
    tb->d_vz_i = 0x3456u;
    tb->d_view_i = 0;
    tb->d_payload_i = 77;
    settle0(tb);
    bool accepted = tb->d_in_ready_o != 0;
    CHECK(accepted, "single-vertex: DUT not ready at idle");
    edge(tb);
    tb->d_in_valid_i = 0;
    bool busy_all = true;
    bool emitted = false;
    for (int c = 0; c < 80 && !emitted; ++c) {
      settle0(tb);
      if (!tb->d_busy_o) busy_all = false;
      if (tb->d_out_valid_o) emitted = true;
      edge(tb);
    }
    CHECK(emitted, "single-vertex: no emission within 80 cycles");
    CHECK(busy_all, "busy_o dropped while the vertex was in flight "
                    "(sequencer holding state not covered)");
    settle0(tb);
    edge(tb);
    settle0(tb);
    CHECK(!tb->d_busy_o, "busy_o still high after the pipe drained");
  }

  // ---- section 5: POSITIVE CONTROL — the checker seen to fail -------------
  // One raw LSB on the DUT's view-0 m[5] (a row-Y term). The streams must
  // now differ; a comparator that stays quiet here is a broken instrument.
  {
    Stream p_ref, p_dut;
    reset(tb);
    load_cfg(tb, cfg, /*skew_dut_addr*/ 5);
    std::vector<Vtx> shortc(corpus.begin(), corpus.begin() + 64);
    run_streams(tb, shortc, 0, 0, -1, 0, p_ref, p_dut);
    const int mm = compare_streams(p_ref, p_dut, false, "");
    CHECK(mm > 0, "positive control: +1 raw on m[5] produced ZERO mismatches "
                  "— the comparator cannot be trusted");
    std::printf("  positive control: skewed m[5] -> %d of %zu records differ\n",
                mm, p_ref.recs.size());
  }

  delete tb;
  if (g_fails == 0) {
    std::printf("proj_rowmux_directed: %d checks passed\n", g_checks);
    return 0;
  }
  std::printf("proj_rowmux_directed: %d of %d checks FAILED\n", g_fails,
              g_checks);
  return 1;
}
