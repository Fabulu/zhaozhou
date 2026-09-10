// proj_matw_mutant_control.cpp — THE POSITIVE CONTROL for the MATW refusal
// law's two instruments, driving a committed MUTANT.
//
// proj_matw_directed asserts that MATW=18 is byte-identical to MATW=32 on
// legal content and that an out-of-range write is refused and counted. Both
// claims lean on ONE line -- the fits-check in g_fits_chk. tests/mutants/
// zhao_project_core_mutant.sv removes that line; THIS TEST PASSES WHEN THE
// DIFFERENTIAL FAILS -- inverse polarity, evidence about the instruments, not
// about the design. Three things must hold, in order:
//
//   1. on LEGAL content (every product word inside +-1.99998) the mutant is
//      INDISTINGUISHABLE from the real core -- the weak vector. A checker that
//      only drove in-range matrices would wave the broken guard through, and
//      this is the half that shows why proj_matw_directed's section 2 exists;
//   2. a write of +2.0 (0x0002_0000) to a product word leaves the mutant's
//      `mat_refused_o` at ZERO -- the instrument that ships is silent, because
//      the check it reports on is gone;
//   3. the differential against the real core now FAILS -- the coefficient
//      wrapped to -2.0 and only the comparison can see it.
//
// (2) and (3) together are what the refusal law buys: with the check present,
// the counter moves and the register holds; with it absent, the counter is
// quiet and the picture is wrong. Neither instrument alone would do.

#include "Vtb_proj_matw_mutant.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <vector>

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

using Vtb = Vtb_proj_matw_mutant;

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

static uint32_t xs32(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

static void settle0(Vtb* tb) { tb->clk = 0; tb->eval(); }
static void edge(Vtb* tb) { tb->clk = 1; tb->eval(); }

static void reset(Vtb* tb) {
  tb->rst_n = 0;
  tb->r_en_i = 1; tb->m_en_i = 1;
  tb->r_in_valid_i = 0; tb->m_in_valid_i = 0;
  tb->r_cfg_we_i = 0; tb->m_cfg_we_i = 0;
  for (int i = 0; i < 4; ++i) { settle0(tb); edge(tb); }
  tb->rst_n = 1;
  settle0(tb);
  edge(tb);
}

// Write one word to both instances (mask bit 0 = ref, bit 1 = mutant).
static void write_word(Vtb* tb, int mask, bool view, uint32_t addr, uint32_t data) {
  if (mask & 1) { tb->r_cfg_we_i = 1; tb->r_cfg_view_i = view; tb->r_cfg_addr_i = addr; tb->r_cfg_data_i = data; }
  if (mask & 2) { tb->m_cfg_we_i = 1; tb->m_cfg_view_i = view; tb->m_cfg_addr_i = addr; tb->m_cfg_data_i = data; }
  settle0(tb);
  edge(tb);
  tb->r_cfg_we_i = 0;
  tb->m_cfg_we_i = 0;
}

// A pose-plausible matrix, every product word inside the 18-bit range.
static void load_pose(Vtb* tb, uint32_t seed) {
  uint32_t s = seed;
  int32_t m0[16], m1[16];
  for (int k = 0; k < 16; ++k) {
    m0[k] = static_cast<int32_t>(xs32(s)) >> 14;
    m1[k] = static_cast<int32_t>(xs32(s)) >> 14;
  }
  m0[3] = static_cast<int32_t>(xs32(s)) >> 6;  m0[7] = static_cast<int32_t>(xs32(s)) >> 6;
  m1[3] = static_cast<int32_t>(xs32(s)) >> 6;  m1[7] = static_cast<int32_t>(xs32(s)) >> 6;
  m0[12] = static_cast<int32_t>(xs32(s)) >> 18; m0[13] = static_cast<int32_t>(xs32(s)) >> 18;
  m0[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;
  m0[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  m1[12] = static_cast<int32_t>(xs32(s)) >> 18; m1[13] = static_cast<int32_t>(xs32(s)) >> 18;
  m1[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;
  m1[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  for (uint32_t k = 0; k < 16; ++k) {
    write_word(tb, 3, false, k, static_cast<uint32_t>(m0[k]));
    write_word(tb, 3, true, k, static_cast<uint32_t>(m1[k]));
  }
  write_word(tb, 3, false, 16, (17u << 16) | 32u);
  write_word(tb, 3, false, 17, (240u << 16) | 320u);
  write_word(tb, 3, true, 16, (90u << 16) | 5u);
  write_word(tb, 3, true, 17, (192u << 16) | 256u);
}

// Run N vertices through both, free-running, and return the mismatch count.
static int run_and_compare(Vtb* tb, int n, uint32_t seed, int* live_out) {
  std::vector<Rec> r, m;
  uint32_t s = seed;
  size_t next = 0;
  uint64_t cyc = 0;
  while ((r.size() < static_cast<size_t>(n) || m.size() < static_cast<size_t>(n)) && cyc < 100000) {
    const bool offer = next < static_cast<size_t>(n);
    int32_t x = 0, y = 0, z = 0;
    if (offer) {
      // small-to-medium world coordinates: w stays positive for most, so the
      // wrapped coefficient lands in LIVE records rather than behind-eye zeros
      x = static_cast<int32_t>(xs32(s)) >> (8 + (xs32(s) % 8u));
      y = static_cast<int32_t>(xs32(s)) >> (8 + (xs32(s) % 8u));
      z = static_cast<int32_t>(xs32(s)) >> (8 + (xs32(s) % 8u));
    }
    tb->r_in_valid_i = offer; tb->m_in_valid_i = offer;
    tb->r_vx_i = static_cast<uint32_t>(x); tb->m_vx_i = static_cast<uint32_t>(x);
    tb->r_vy_i = static_cast<uint32_t>(y); tb->m_vy_i = static_cast<uint32_t>(y);
    tb->r_vz_i = static_cast<uint32_t>(z); tb->m_vz_i = static_cast<uint32_t>(z);
    tb->r_view_i = (next / 3) & 1; tb->m_view_i = (next / 3) & 1;
    tb->r_payload_i = static_cast<uint16_t>(next); tb->m_payload_i = static_cast<uint16_t>(next);
    settle0(tb);
    if (tb->r_out_valid_o) {
      Rec q{};
      q.x = static_cast<int32_t>(tb->r_out_x_o << 11) >> 11;
      q.y = static_cast<int32_t>(tb->r_out_y_o << 11) >> 11;
      q.d = static_cast<int32_t>(tb->r_out_d_o); q.w = tb->r_out_w_o;
      q.behind = tb->r_out_behind_o; q.view = tb->r_out_view_o; q.pay = tb->r_out_payload_o;
      r.push_back(q);
    }
    if (tb->m_out_valid_o) {
      Rec q{};
      q.x = static_cast<int32_t>(tb->m_out_x_o << 11) >> 11;
      q.y = static_cast<int32_t>(tb->m_out_y_o << 11) >> 11;
      q.d = static_cast<int32_t>(tb->m_out_d_o); q.w = tb->m_out_w_o;
      q.behind = tb->m_out_behind_o; q.view = tb->m_out_view_o; q.pay = tb->m_out_payload_o;
      m.push_back(q);
    }
    edge(tb);
    if (offer) ++next;
    ++cyc;
  }
  tb->r_in_valid_i = 0; tb->m_in_valid_i = 0;
  CHECK(cyc < 100000, "run timed out");
  CHECK(r.size() == m.size(), "stream lengths %zu vs %zu", r.size(), m.size());
  int mm = 0, live = 0;
  for (size_t i = 0; i < r.size() && i < m.size(); ++i) {
    if (!(r[i] == m[i])) ++mm;
    if (!r[i].behind) ++live;
  }
  if (live_out) *live_out = live;
  return mm;
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* tb = new Vtb;

  // ---- 1. the weak vector: legal content, mutant indistinguishable ----------
  reset(tb);
  load_pose(tb, 0x5EED5EEDu);
  int live = 0;
  const int mm_legal = run_and_compare(tb, 120, 0xC0FFEEu, &live);
  CHECK(mm_legal == 0, "mutant differs from the real core on LEGAL content (%d records) -- "
                       "then this is not the mutant described", mm_legal);
  CHECK(live > 0, "weak-vector run produced no live vertices");
  CHECK(tb->m_mat_refused_o == 0 && tb->r_mat_refused_o == 0,
        "counters moved on legal content (mutant %u, ref %u)", tb->m_mat_refused_o,
        tb->r_mat_refused_o);
  std::printf("  legal content: %d mismatches of 120 (mutant hides on the weak vector)\n",
              mm_legal);

  // ---- 2 + 3. +2.0 into view-0 m[5]: silent counter, failing differential ----
  // The reference (MATW=32) takes 0x0002_0000 as +2.0. The mutant stores it
  // and reads its low 18 bits, which is -2.0. Nothing says so but the output.
  write_word(tb, 3, false, 5, 0x00020000u);
  settle0(tb);
  CHECK(tb->m_mat_refused_o == 0,
        "mutant counter read %u -- the fits-check is supposed to be GONE here",
        tb->m_mat_refused_o);
  const int mm_wrap = run_and_compare(tb, 120, 0xC0FFEEu, &live);
  CHECK(mm_wrap > 0, "differential did NOT fire on a wrapped +2.0 coefficient -- the "
                     "comparator cannot see the fault the refusal law exists for");
  CHECK(live > 0, "wrap run produced no live vertices");
  std::printf("  +2.0 on m[5]: mutant counter=%u, differential mismatches=%d of 120 (%d live)\n",
              tb->m_mat_refused_o, mm_wrap, live);

  // and a wide value with a CLEAN low half: 0x8000_0000 into m[0] reads as 0
  // in the mutant -- a coefficient that quietly vanishes.
  write_word(tb, 3, false, 0, 0x80000000u);
  settle0(tb);
  CHECK(tb->m_mat_refused_o == 0, "mutant counter moved (%u); the check should be absent",
        tb->m_mat_refused_o);
  const int mm_zero = run_and_compare(tb, 120, 0xC0FFEEu, &live);
  CHECK(mm_zero > 0, "differential did NOT fire on a coefficient that wrapped to zero");
  std::printf("  INT32_MIN on m[0]: mutant counter=%u, differential mismatches=%d of 120\n",
              tb->m_mat_refused_o, mm_zero);

  delete tb;
  if (g_fails == 0) {
    std::printf("proj_matw_mutant_control: %d checks passed -- the differential fires and the "
                "mutant's counter is silent, as a broken guard would be\n", g_checks);
    return 0;
  }
  std::printf("proj_matw_mutant_control: %d of %d checks FAILED\n", g_fails, g_checks);
  return 1;
}
