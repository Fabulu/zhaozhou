// proj_matw_directed.cpp — MATW=18 against MATW=32, byte-for-byte, over the
// whole legal coefficient domain; and the refusal law seen to fire.
//
// Owner ruling R1 (2026-09-09, reports/OWNER-RULINGS-20260909-2300.md):
// the view-projection matrix's perspective coefficients may be capped at
// +-2.0, so the nine PRODUCT words of zhao_project_core may be carried as
// signed 18-bit Q16.16. The acceptance criterion is that the narrowing is
// INVISIBLE for legal content: for every product word inside the ruled range
// -- +131071 (1.99998) down to -131072 (-2.0) -- and for every value of the
// full-width words the ruling does not touch (the translation column 3/7/15
// at the whole s32, the inert row 2 at anything, the vertices at the whole
// s32), the MATW=18 core must emit exactly the record the MATW=32 core emits.
//
// The chain of evidence: geom_project_directed and the two terrain suites pin
// the DEFAULT core (MATW=32) to the shipped oracle `zref::render::
// project_vertex`, the terrain random suite sweeping matrix words over the
// FULL s32; this test pins MATW=18 to MATW=32 as an output STREAM, so the
// narrowed core inherits the oracle by transitivity without this file
// re-implementing one line of projection arithmetic (which the repository
// forbids). Two narrowed instances are compared, at ROWS_PER_PASS=3 and 1,
// because that lever landed the day before and the two must keep composing.
//
// What it asserts, by section:
//   1. stream equality over SIX configurations -- a pose-plausible matrix,
//      an edge matrix with every product word on a boundary of the 18-bit
//      range and the translation column on the s32 rails, and four random
//      matrices drawn over the full legal domain -- each narrowed instance
//      under its own stall pattern, and the refusal counter reading ZERO on
//      every instance throughout (legal content is never refused);
//   2. THE REFUSAL LAW, at MATW=18: an out-of-range write to a product word
//      moves `mat_refused_o` by exactly one and leaves the word UNCHANGED (the
//      never-written reference still agrees afterwards); the boundary values
//      +131071 / -131072 are accepted and +131072 / -131073 refused; the
//      translation column and row 2 accept anything without counting; both
//      views count; the MATW=32 reference never counts;
//   3. fixed latency: the spatial MATW=18 instance is cycle-identical to the
//      reference, the sequenced one is +3 with initiation interval 3;
//   4. POSITIVE CONTROL: a one-raw-LSB skew in one narrowed instance's word
//      makes the comparator fire. A checker never seen to fail is untested.
//
// The mutant control (tests/mutants/zhao_project_core_mutant.sv +
// proj_matw_mutant_control.cpp) is the other half of section 2's evidence:
// with the fits-check removed, +2.0 wraps to -2.0, the counter stays silent
// and ONLY the differential sees it.

#include "Vtb_proj_matw.h"
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

using Vtb = Vtb_proj_matw;


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

// ---------------------------------------------------------------------------
// the three instances, addressed uniformly through accessor functions
// ---------------------------------------------------------------------------
// Verilator 5 exposes top-level ports as REFERENCE members of the model, so
// pointers-to-member are not available; capture-less lambdas as function
// pointers are, and cost nothing at the call site.
struct Side {
  const char* name;
  void (*set_cfg)(Vtb*, bool we, bool view, uint32_t addr, uint32_t data);
  void (*set_en)(Vtb*, bool en);
  void (*set_in)(Vtb*, bool valid, uint32_t x, uint32_t y, uint32_t z, bool view, uint16_t pay);
  bool (*in_ready)(Vtb*);
  bool (*out_valid)(Vtb*);
  Rec (*read_rec)(Vtb*);
  uint32_t (*refused)(Vtb*);
};

#define SIDE(P, NAME)                                                                   \
  Side {                                                                                \
    NAME,                                                                               \
        [](Vtb* t, bool we, bool view, uint32_t a, uint32_t d) {                        \
          t->P##_cfg_we_i = we;                                                         \
          t->P##_cfg_view_i = view;                                                     \
          t->P##_cfg_addr_i = a;                                                        \
          t->P##_cfg_data_i = d;                                                        \
        },                                                                              \
        [](Vtb* t, bool e) { t->P##_en_i = e; },                                        \
        [](Vtb* t, bool v, uint32_t x, uint32_t y, uint32_t z, bool vw, uint16_t p) {   \
          t->P##_in_valid_i = v;                                                        \
          t->P##_vx_i = x;                                                              \
          t->P##_vy_i = y;                                                              \
          t->P##_vz_i = z;                                                              \
          t->P##_view_i = vw;                                                           \
          t->P##_payload_i = p;                                                         \
        },                                                                              \
        [](Vtb* t) -> bool { return t->P##_in_ready_o != 0; },                          \
        [](Vtb* t) -> bool { return t->P##_out_valid_o != 0; },                         \
        [](Vtb* t) -> Rec {                                                             \
          Rec r{};                                                                      \
          r.x = static_cast<int32_t>((t->P##_out_x_o) << 11) >> 11;                     \
          r.y = static_cast<int32_t>((t->P##_out_y_o) << 11) >> 11;                     \
          r.d = static_cast<int32_t>(t->P##_out_d_o);                                   \
          r.w = t->P##_out_w_o;                                                         \
          r.behind = t->P##_out_behind_o != 0;                                          \
          r.view = t->P##_out_view_o != 0;                                              \
          r.pay = t->P##_out_payload_o;                                                 \
          return r;                                                                     \
        },                                                                              \
        [](Vtb* t) -> uint32_t { return t->P##_mat_refused_o; }                         \
  }

static const Side R = SIDE(r, "ref MATW=32");
static const Side D = SIDE(d, "MATW=18 RPP=3");
static const Side S = SIDE(s, "MATW=18 RPP=1");
static const Side* const ALL[3] = {&R, &D, &S};
enum { kRef = 1, kDut = 2, kSeq = 4, kNarrow = kDut | kSeq, kEvery = 7 };

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

// Full-s32 extremes, small world coordinates with live fractions, near-plane
// pressure, and a full random spread -- the vertex operand is UNTOUCHED by
// the ruling, so it is swept to the whole word.
static std::vector<Vtx> make_corpus(int n, uint32_t seed) {
  std::vector<Vtx> v;
  uint32_t s = seed;
  for (int k = 0; k < n; ++k) {
    Vtx t{};
    switch (k % 9) {
      case 0:
        t.x = (k & 16) ? INT32_MIN : INT32_MAX;
        t.y = static_cast<int32_t>(xs32(s));
        t.z = (k & 32) ? INT32_MAX : INT32_MIN;
        break;
      case 1:
        t.x = static_cast<int32_t>(xs32(s)) >> 12;
        t.y = static_cast<int32_t>(xs32(s)) >> 12;
        t.z = static_cast<int32_t>(xs32(s)) >> 12;
        break;
      case 2:
        t.x = static_cast<int32_t>(xs32(s)) >> 8;
        t.y = static_cast<int32_t>(xs32(s)) >> 8;
        t.z = -(static_cast<int32_t>(xs32(s) >> 1));
        break;
      case 3:  // tiny coordinates: fractions dominate, rounding is live
        t.x = static_cast<int32_t>(xs32(s)) >> 20;
        t.y = static_cast<int32_t>(xs32(s)) >> 20;
        t.z = static_cast<int32_t>(xs32(s)) >> 20;
        break;
      default:
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
static void settle0(Vtb* tb) {
  tb->clk = 0;
  tb->eval();
}
static void edge(Vtb* tb) {
  tb->clk = 1;
  tb->eval();
}

static void reset(Vtb* tb) {
  tb->rst_n = 0;
  for (const Side* s : ALL) {
    s->set_en(tb, true);
    s->set_in(tb, false, 0, 0, 0, false, 0);
    s->set_cfg(tb, false, false, 0, 0);
  }
  for (int i = 0; i < 4; ++i) {
    settle0(tb);
    edge(tb);
  }
  tb->rst_n = 1;
  settle0(tb);
  edge(tb);
}

static uint32_t refused(Vtb* tb, const Side& s) {
  settle0(tb);
  return s.refused(tb);
}

// One configuration word to every instance in `mask`, on one clock.
static void write_word(Vtb* tb, int mask, bool view, uint32_t addr, uint32_t data) {
  for (int i = 0; i < 3; ++i) {
    if (mask & (1 << i)) ALL[i]->set_cfg(tb, true, view, addr, data);
  }
  settle0(tb);
  edge(tb);
  for (const Side* s : ALL) s->set_cfg(tb, false, false, 0, 0);
}

// Is `addr` (0..15) one of the nine PRODUCT words: columns 0..2 of rows 0, 1, 3?
static bool is_prod(uint32_t addr) {
  return addr < 16 && (addr & 3) != 3 && (addr >> 2) != 2;
}

struct Cfg {
  int32_t m0[16], m1[16];
  uint32_t vp0[2], vp1[2];
};

// A pose-plausible matrix: 3x3 near unit scale with live fraction bits, a
// translation that carries range, a w row that varies with z. Every product
// word already inside the 18-bit range (>> 14 is exactly [-131072, 131071]).
static Cfg make_cfg_pose(uint32_t seed) {
  Cfg c{};
  uint32_t s = seed;
  for (int k = 0; k < 16; ++k) {
    c.m0[k] = static_cast<int32_t>(xs32(s)) >> 14;
    c.m1[k] = static_cast<int32_t>(xs32(s)) >> 14;
  }
  c.m0[3] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m0[7] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m1[3] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m1[7] = static_cast<int32_t>(xs32(s)) >> 6;
  c.m0[12] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m0[13] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m0[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;
  c.m0[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  c.m1[12] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m1[13] = static_cast<int32_t>(xs32(s)) >> 18;
  c.m1[14] = (static_cast<int32_t>(xs32(s)) >> 15) | 1;
  c.m1[15] = 65536 + (static_cast<int32_t>(xs32(s)) >> 16);
  for (int k = 8; k < 12; ++k) {
    c.m0[k] = static_cast<int32_t>(xs32(s));
    c.m1[k] = static_cast<int32_t>(xs32(s));
  }
  c.vp0[0] = (17u << 16) | 32u;
  c.vp0[1] = (240u << 16) | 320u;
  c.vp1[0] = (90u << 16) | 5u;
  c.vp1[1] = (192u << 16) | 256u;
  return c;
}

// Every product word on a BOUNDARY of the ruled range, the translation column
// on the s32 rails, row 2 at junk. The two views take the boundaries in
// different orders so no coincidence between them hides a swapped word.
static Cfg make_cfg_edge() {
  static const int32_t kEdge[9] = {131071, -131072, 0, 1, -1, 65536, -65536, 131071, -131072};
  Cfg c{};
  static const int kProd[9] = {0, 1, 2, 4, 5, 6, 12, 13, 14};
  for (int i = 0; i < 9; ++i) {
    c.m0[kProd[i]] = kEdge[i];
    c.m1[kProd[i]] = kEdge[(i * 4 + 3) % 9];
  }
  c.m0[3] = INT32_MAX;
  c.m0[7] = INT32_MIN;
  c.m0[15] = INT32_MAX;      // w translation on the rail: w saturates positive
  c.m1[3] = INT32_MIN;
  c.m1[7] = INT32_MAX;
  c.m1[15] = 3 << 16;        // small positive: w is live for small vertices
  for (int k = 8; k < 12; ++k) {
    c.m0[k] = static_cast<int32_t>(0xDEADBEEFu + k);
    c.m1[k] = static_cast<int32_t>(0xCAFEF00Du - k);
  }
  c.vp0[0] = (0u << 16) | 0u;
  c.vp0[1] = (240u << 16) | 384u;
  c.vp1[0] = (100u << 16) | 4000u;   // origin near the 12-bit rail
  c.vp1[1] = (192u << 16) | 256u;
  return c;
}

// Random over the WHOLE legal domain: product words at full 18-bit, the
// translation column at full 32-bit, row 2 at anything, viewports random.
static Cfg make_cfg_random18(uint32_t seed) {
  Cfg c{};
  uint32_t s = seed;
  for (int k = 0; k < 16; ++k) {
    if (is_prod(static_cast<uint32_t>(k))) {
      c.m0[k] = static_cast<int32_t>(xs32(s)) >> 14;
      c.m1[k] = static_cast<int32_t>(xs32(s)) >> 14;
    } else {
      c.m0[k] = static_cast<int32_t>(xs32(s));
      c.m1[k] = static_cast<int32_t>(xs32(s));
    }
  }
  // Keep w's translation positive on view 0 so the near plane is not the
  // whole story; view 1 is left wherever it landed (behind-eye pressure).
  c.m0[15] = static_cast<int32_t>(xs32(s) >> 1) | 1;
  c.vp0[0] = xs32(s) & 0x0FFF0FFFu;
  c.vp0[1] = xs32(s) & 0x0FFF0FFFu;
  c.vp1[0] = xs32(s) & 0x0FFF0FFFu;
  c.vp1[1] = xs32(s) & 0x0FFF0FFFu;
  return c;
}

// Load one configuration into every instance in `mask`. `skew_mask`/`skew_addr`
// add +1 raw to that view-0 word on the skewed instances only -- the
// positive control's knife.
static void load_cfg(Vtb* tb, const Cfg& c, int mask, int skew_mask = 0,
                     int skew_addr = -1) {
  for (uint32_t k = 0; k < 16; ++k) {
    const uint32_t w0 = static_cast<uint32_t>(c.m0[k]);
    if (static_cast<int>(k) == skew_addr && skew_mask) {
      write_word(tb, mask & ~skew_mask, false, k, w0);
      write_word(tb, mask & skew_mask, false, k, w0 + 1u);
    } else {
      write_word(tb, mask, false, k, w0);
    }
    write_word(tb, mask, true, k, static_cast<uint32_t>(c.m1[k]));
  }
  write_word(tb, mask, false, 16, c.vp0[0]);
  write_word(tb, mask, false, 17, c.vp0[1]);
  write_word(tb, mask, true, 16, c.vp1[0]);
  write_word(tb, mask, true, 17, c.vp1[1]);
}

// Drive all three instances through one corpus, each under its own stall
// pattern, collecting each output stream with acceptance/emission stamps.
static void run_streams(Vtb* tb, const std::vector<Vtx>& corpus, const int pats[3],
                        Stream out[3]) {
  size_t next[3] = {0, 0, 0};
  uint64_t en_count[3] = {0, 0, 0};
  uint32_t lfsr[3] = {0xACE1u, 0xBEEFu, 0x1357u};
  for (int i = 0; i < 3; ++i) out[i] = Stream{};
  uint64_t cyc = 0;
  const uint64_t kTimeout = 600000;

  auto all_done = [&]() {
    for (int i = 0; i < 3; ++i)
      if (out[i].recs.size() < corpus.size()) return false;
    return true;
  };

  while (!all_done() && cyc < kTimeout) {
    bool en[3], offer[3];
    for (int i = 0; i < 3; ++i) {
      const Side& s = *ALL[i];
      en[i] = pat_en(pats[i], cyc, lfsr[i]);
      s.set_en(tb, en[i]);
      offer[i] = next[i] < corpus.size();
      if (offer[i]) {
        const Vtx& t = corpus[next[i]];
        s.set_in(tb, true, static_cast<uint32_t>(t.x), static_cast<uint32_t>(t.y),
                 static_cast<uint32_t>(t.z), t.view, t.pay);
      } else {
        s.set_in(tb, false, 0, 0, 0, false, 0);
      }
    }

    settle0(tb);

    bool acc[3];
    for (int i = 0; i < 3; ++i) {
      const Side& s = *ALL[i];
      acc[i] = en[i] && offer[i] && s.in_ready(tb);
      if (en[i] && s.out_valid(tb)) {
        out[i].recs.push_back(s.read_rec(tb));
        out[i].out_stamp.push_back(en_count[i]);
      }
    }

    edge(tb);

    for (int i = 0; i < 3; ++i) {
      if (acc[i]) {
        out[i].acc_stamp.push_back(en_count[i]);
        ++next[i];
      }
      if (en[i]) ++en_count[i];
    }
    ++cyc;
  }
  for (const Side* s : ALL) s->set_in(tb, false, 0, 0, 0, false, 0);

  CHECK(cyc < kTimeout, "stream run timed out at %llu cycles (%zu/%zu/%zu of %zu)",
        static_cast<unsigned long long>(cyc), out[0].recs.size(), out[1].recs.size(),
        out[2].recs.size(), corpus.size());
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

static void check_corpus_honest(const Stream& g, const char* tag) {
  int behind = 0, live = 0, railx = 0, sat_d = 0;
  for (const Rec& r : g.recs) {
    (r.behind ? behind : live)++;
    if (!r.behind && (r.x == 524288 || r.x == -524288 || r.y == 524288 || r.y == -524288)) ++railx;
    if (!r.behind && (r.d == INT32_MAX)) ++sat_d;
  }
  CHECK(behind > 0, "%s: corpus never crossed the near plane", tag);
  CHECK(live > 0, "%s: corpus never produced a live vertex", tag);
  std::printf("  %-10s live=%d behind=%d guard-rail=%d 1/w-rail=%d\n", tag, live, behind,
              railx, sat_d);
}

static void check_no_refusals(Vtb* tb, const char* tag) {
  for (const Side* s : ALL) {
    const uint32_t n = refused(tb, *s);
    CHECK(n == 0, "%s: %s refused %u legal writes", tag, s->name, n);
  }
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* tb = new Vtb;

  const int kN = 360;
  const std::vector<Vtx> corpus = make_corpus(kN, 0xC0FFEEu);

  // ---- section 1: stream equality over the legal domain, six configs ------
  // The reference free-runs; the two narrowed instances take a rotating pair
  // of stall patterns so stall-invariance is proven alongside width-invariance.
  {
    Cfg cfgs[6] = {make_cfg_pose(0x5EED5EEDu), make_cfg_edge(),
                   make_cfg_random18(0x1111u),  make_cfg_random18(0x2222u),
                   make_cfg_random18(0x3333u),  make_cfg_random18(0x4444u)};
    const char* names[6] = {"pose", "edge", "rnd18-a", "rnd18-b", "rnd18-c", "rnd18-d"};
    for (int ci = 0; ci < 6; ++ci) {
      reset(tb);
      load_cfg(tb, cfgs[ci], kEvery);
      check_no_refusals(tb, names[ci]);
      const int pats[3] = {0, 1 + (ci % 3), 1 + ((ci + 1) % 3)};
      Stream out[3];
      run_streams(tb, corpus, pats, out);
      CHECK(out[0].recs.size() == static_cast<size_t>(kN), "%s: reference stream short: %zu",
            names[ci], out[0].recs.size());
      char tag[48];
      std::snprintf(tag, sizeof tag, "%s/%s", names[ci], D.name);
      compare_streams(out[0], out[1], true, tag);
      std::snprintf(tag, sizeof tag, "%s/%s", names[ci], S.name);
      compare_streams(out[0], out[2], true, tag);
      check_corpus_honest(out[0], names[ci]);
      check_no_refusals(tb, names[ci]);
    }
  }

  // ---- section 2: THE REFUSAL LAW at MATW=18 --------------------------------
  {
    const Cfg base = make_cfg_pose(0x0BADF00Du);
    std::vector<Vtx> shortc(corpus.begin(), corpus.begin() + 72);
    const int free3[3] = {0, 0, 0};
    static const int kProd[9] = {0, 1, 2, 4, 5, 6, 12, 13, 14};
    // Out of range for signed 18: +2.0 exactly, -2.0 minus one LSB, the
    // s32 rails, and two wide values with clean low halves (a silent wrap
    // would leave a plausible coefficient behind).
    static const uint32_t kIllegal[6] = {0x00020000u, 0xFFFDFFFFu, 0x7FFFFFFFu,
                                         0x80000000u, 0x00040000u, 0xFFFC0000u};

    reset(tb);
    load_cfg(tb, base, kEvery);
    check_no_refusals(tb, "refusal/base");

    // 2a: each illegal value into each product word, view 0, narrowed only.
    uint32_t expect = 0;
    for (int p = 0; p < 9; ++p) {
      for (uint32_t v : kIllegal) {
        const uint32_t before_d = refused(tb, D), before_s = refused(tb, S);
        write_word(tb, kNarrow, false, static_cast<uint32_t>(kProd[p]), v);
        ++expect;
        const uint32_t after_d = refused(tb, D), after_s = refused(tb, S);
        CHECK(after_d == before_d + 1, "m[%d] <- 0x%08X: RPP=3 counter %u -> %u, expected +1",
              kProd[p], v, before_d, after_d);
        CHECK(after_s == before_s + 1, "m[%d] <- 0x%08X: RPP=1 counter %u -> %u, expected +1",
              kProd[p], v, before_s, after_s);
      }
    }
    CHECK(refused(tb, D) == expect && refused(tb, S) == expect,
          "refusal total: expected %u, got RPP=3 %u RPP=1 %u", expect, refused(tb, D),
          refused(tb, S));
    CHECK(refused(tb, R) == 0, "MATW=32 reference counted %u refusals; it is structurally zero",
          refused(tb, R));
    // The refused words must have KEPT their values: the never-written
    // reference still agrees with both narrowed instances, record for record.
    {
      Stream out[3];
      run_streams(tb, shortc, free3, out);
      compare_streams(out[0], out[1], true, "after-refusal/RPP=3 (register kept its value)");
      compare_streams(out[0], out[2], true, "after-refusal/RPP=1 (register kept its value)");
    }

    // 2b: the boundary. +131071 and -131072 FIT and are written on all three;
    // +131072 and -131073 do NOT and are refused. The accepted writes must
    // also have MATTERED (the streams move), or the section is inert.
    {
      Stream before[3], after[3];
      run_streams(tb, shortc, free3, before);
      const uint32_t r0d = refused(tb, D), r0s = refused(tb, S);
      write_word(tb, kEvery, false, 5, 131071u);
      write_word(tb, kEvery, false, 1, static_cast<uint32_t>(-131072));
      write_word(tb, kEvery, true, 4, 131071u);
      write_word(tb, kEvery, true, 13, static_cast<uint32_t>(-131072));
      CHECK(refused(tb, D) == r0d && refused(tb, S) == r0s,
            "boundary values +131071/-131072 were refused (RPP=3 %u->%u, RPP=1 %u->%u)", r0d,
            refused(tb, D), r0s, refused(tb, S));
      write_word(tb, kNarrow, false, 5, 131072u);
      CHECK(refused(tb, D) == r0d + 1 && refused(tb, S) == r0s + 1,
            "+131072 (2.0) was not refused");
      write_word(tb, kNarrow, true, 13, static_cast<uint32_t>(-131073));
      CHECK(refused(tb, D) == r0d + 2 && refused(tb, S) == r0s + 2,
            "-131073 was not refused (view 1 does not count)");
      run_streams(tb, shortc, free3, after);
      compare_streams(after[0], after[1], true, "boundary/RPP=3");
      compare_streams(after[0], after[2], true, "boundary/RPP=1");
      CHECK(compare_streams(before[0], after[0], false, "") > 0,
            "boundary writes changed no record; the section is inert");
    }

    // 2c: the translation column (3, 7, 15) and row 2 (8..11) accept ANY
    // word without counting -- they never enter a multiplier.
    {
      const uint32_t r0d = refused(tb, D), r0s = refused(tb, S);
      static const uint32_t kWide[4] = {0x12345678u, 0x80000000u, 0x7FFFFFFFu, 0xFFFE0000u};
      static const int kFree[7] = {3, 7, 15, 8, 9, 10, 11};
      Stream before[3];
      run_streams(tb, shortc, free3, before);
      for (int a : kFree)
        for (uint32_t v : kWide) write_word(tb, kEvery, (a & 1) != 0, static_cast<uint32_t>(a), v);
      CHECK(refused(tb, D) == r0d && refused(tb, S) == r0s,
            "translation/row-2 words were refused (RPP=3 %u->%u, RPP=1 %u->%u)", r0d,
            refused(tb, D), r0s, refused(tb, S));
      Stream after[3];
      run_streams(tb, shortc, free3, after);
      compare_streams(after[0], after[1], true, "wide-translation/RPP=3");
      compare_streams(after[0], after[2], true, "wide-translation/RPP=1");
      CHECK(compare_streams(before[0], after[0], false, "") > 0,
            "full-width translation writes changed no record; the section is inert");
      // and row 2 alone must change NOTHING (inert by construction, both shapes)
      Stream row2[3];
      for (uint32_t a = 8; a < 12; ++a) write_word(tb, kEvery, false, a, 0x5A5A5A5Au ^ a);
      run_streams(tb, shortc, free3, row2);
      CHECK(compare_streams(after[0], row2[0], false, "") == 0, "row-2 write moved the reference");
      compare_streams(row2[0], row2[1], true, "row2/RPP=3");
      compare_streams(row2[0], row2[2], true, "row2/RPP=1");
    }
    std::printf("  refusal law: %u refused writes counted on each narrowed instance, 0 on the reference\n",
                refused(tb, D));
  }

  // ---- section 3: latency -- spatial identical, sequenced +3 at II=3 ---------
  {
    reset(tb);
    load_cfg(tb, make_cfg_pose(0x5EED5EEDu), kEvery);
    const int free3[3] = {0, 0, 0};
    Stream out[3];
    run_streams(tb, corpus, free3, out);
    uint64_t lat[3] = {0, 0, 0};
    bool fixed[3] = {true, true, true};
    for (int i = 0; i < 3; ++i) {
      CHECK(out[i].acc_stamp.size() == out[i].out_stamp.size(), "%s: accept/emit count skew",
            ALL[i]->name);
      lat[i] = out[i].out_stamp[0] - out[i].acc_stamp[0];
      for (size_t k = 0; k < out[i].out_stamp.size(); ++k)
        if (out[i].out_stamp[k] - out[i].acc_stamp[k] != lat[i]) fixed[i] = false;
      CHECK(fixed[i], "%s: latency is not fixed", ALL[i]->name);
    }
    bool ii3 = true;
    for (size_t k = 1; k < out[2].acc_stamp.size(); ++k)
      if (out[2].acc_stamp[k] - out[2].acc_stamp[k - 1] != 3) ii3 = false;
    CHECK(lat[1] == lat[0], "MATW=18 RPP=3 latency %llu differs from reference %llu",
          static_cast<unsigned long long>(lat[1]), static_cast<unsigned long long>(lat[0]));
    CHECK(lat[2] == lat[0] + 3, "MATW=18 RPP=1 latency %llu, declared reference+3 = %llu",
          static_cast<unsigned long long>(lat[2]), static_cast<unsigned long long>(lat[0] + 3));
    CHECK(ii3, "MATW=18 RPP=1 initiation interval is not exactly 3 under saturation");
    std::printf("  measured: L(ref)=%llu  L(MATW=18,RPP=3)=%llu  L(MATW=18,RPP=1)=%llu  II=3\n",
                static_cast<unsigned long long>(lat[0]), static_cast<unsigned long long>(lat[1]),
                static_cast<unsigned long long>(lat[2]));
  }

  // ---- section 4: POSITIVE CONTROL -- the comparator seen to fail ------------
  // One raw LSB on the spatial narrowed instance's view-0 m[5] and on the
  // sequenced one's m[1] (both IN range, so nothing is refused). The streams
  // must differ from the reference; a comparator quiet here is broken.
  {
    reset(tb);
    const Cfg c = make_cfg_pose(0x5EED5EEDu);
    load_cfg(tb, c, kRef | kSeq);
    load_cfg(tb, c, kDut, kDut, 5);
    // re-skew the sequenced instance on a different word
    write_word(tb, kSeq, false, 1, static_cast<uint32_t>(c.m0[1]) + 1u);
    check_no_refusals(tb, "positive-control");
    std::vector<Vtx> shortc(corpus.begin(), corpus.begin() + 72);
    const int free3[3] = {0, 0, 0};
    Stream out[3];
    run_streams(tb, shortc, free3, out);
    const int mm_d = compare_streams(out[0], out[1], false, "");
    const int mm_s = compare_streams(out[0], out[2], false, "");
    CHECK(mm_d > 0, "positive control: +1 raw on RPP=3 m[5] produced ZERO mismatches");
    CHECK(mm_s > 0, "positive control: +1 raw on RPP=1 m[1] produced ZERO mismatches");
    std::printf("  positive control: skewed words -> %d / %d of %zu records differ\n", mm_d, mm_s,
                out[0].recs.size());
  }

  delete tb;
  if (g_fails == 0) {
    std::printf("proj_matw_directed: %d checks passed\n", g_checks);
    return 0;
  }
  std::printf("proj_matw_directed: %d of %d checks FAILED\n", g_fails, g_checks);
  return 1;
}
