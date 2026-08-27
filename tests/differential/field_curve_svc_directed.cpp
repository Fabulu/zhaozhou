// field_curve_svc_directed.cpp — the Field v3 barrel curve service against
// the ONE semantic layer (zfield::exec_op, which zfield::interpret itself
// calls), plus the probe's ACCEPTANCE GATE: measured four-point CURVE
// initiation interval <= 14 clocks (reports/Fieldv3.md Phase 3, probe 4).
//
// LAWS:
//   1. THE SEARCH IS SIX STEPS, ALWAYS — section 1 sweeps table sizes
//      including 2, 3 and 33 (non-powers of two), probing every knot,
//      knot +/- 1 and segment midpoints, so a step count derived from n is
//      wrong at some size.
//   2. THE SEARCH RUNS ON THE CLAMPED VALUE, and the clamp bounds come from
//      the TABLE CACHE META registers latched at load — section 1 probes far
//      outside both ends; section 3 RELOADS a slot and re-probes, so stale
//      meta is caught.
//   3. THE ENTRY IS CAPTURED ON THE WAY DOWN (six reads per lane, no entry
//      fetch): every value check exercises it, and the landed segment index
//      is checked against zfield::steps::segment_search directly.
//   4. FLAG ATTRIBUTION IS THE REFERENCE'S: fx_sub -> add lane, fx_mad ->
//      mul lane, DCURVE records NOTHING. Section 2 drives saturating and
//      clean lanes mixed in ONE group so the flags cannot be a single OR.
//   5. THE GATE: streaming four-lane requests must sustain II <= 14. The
//      structural minimum is 12 (24 table reads through 2 ports); the
//      designed schedule is 13.
//   6. NOTHING IS LOST, DUPLICATED OR REORDERED under reply backpressure;
//      the service holds exactly FOUR groups with replies blocked (reply
//      skid + finish + completed search + staging) and refuses the fifth.
//
// The multiplier bank is the ENGINE's, not the probe's: the test models the
// vector lane (4 products, registered, two-cycle) exactly as the engine
// documentation prices it, and the probe's measured cost excludes it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_curve_svc.h"

#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

using zhao::check;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
};

// ---- the vector multiplier bank model (engine property) -------------------
// One four-lane issue, products back with the lane's registered two-cycle
// latency. 33x33 products fit s63, so int64 holds them exactly.
struct MulBank {
  bool busy = false;
  int cnt = 0;
  int64_t p[4] = {0, 0, 0, 0};
};

int64_t sx33(uint64_t v) { return ((int64_t)(v << 31)) >> 31; }

template <typename W>
void set66(W& w, int64_t p) {
  w[0] = (uint32_t)((uint64_t)p & 0xFFFFFFFFull);
  w[1] = (uint32_t)(((uint64_t)p >> 32) & 0xFFFFFFFFull);
  w[2] = (p < 0) ? 0x3u : 0x0u;
}

/** One cycle: serve the mul bank, then clock the DUT. */
void step(Vzhao_probe_curve_svc& dut, MulBank& mb) {
  if (mb.busy && mb.cnt == 0) {
    set66(dut.mul_p_0_i, mb.p[0]);
    set66(dut.mul_p_1_i, mb.p[1]);
    set66(dut.mul_p_2_i, mb.p[2]);
    set66(dut.mul_p_3_i, mb.p[3]);
    dut.mul_valid_i = 1;
    mb.busy = false;
  } else {
    dut.mul_valid_i = 0;
  }
  dut.eval();
  if (dut.mul_issue_o) {
    mb.p[0] = sx33(dut.mul_a_0_o) * sx33(dut.mul_b_0_o);
    mb.p[1] = sx33(dut.mul_a_1_o) * sx33(dut.mul_b_1_o);
    mb.p[2] = sx33(dut.mul_a_2_o) * sx33(dut.mul_b_2_o);
    mb.p[3] = sx33(dut.mul_a_3_o) * sx33(dut.mul_b_3_o);
    mb.busy = true;
    mb.cnt = 1;
  } else if (mb.busy && mb.cnt > 0) {
    --mb.cnt;
  }
  zhao::tick(dut);
}

// ---- table load ------------------------------------------------------------
void load_table(Vzhao_probe_curve_svc& dut, MulBank& mb, int slot, const zfield::Table& t) {
  const int n = (int)t.x.size();
  for (int i = 0; i < n; ++i) {
    dut.tl_we_i = 1;
    dut.tl_tbl_i = (uint8_t)slot;
    dut.tl_idx_i = (uint8_t)i;
    dut.tl_x_i = (uint32_t)t.x[i];
    dut.tl_y_i = (uint32_t)t.y[i];
    dut.tl_dy_i = (uint32_t)t.dy[i];
    step(dut, mb);
  }
  dut.tl_we_i = 0;
  dut.tl_commit_i = 1;
  dut.tl_n_i = (uint8_t)n;
  step(dut, mb);
  dut.tl_commit_i = 0;
  dut.eval();
}

// ---- the oracle: the shipped semantic layer -------------------------------
struct Want {
  int32_t r[4];
  bool sat_add[4];
  bool sat_mul[4];
  int seg[4];
};

Want oracle(const std::vector<zfield::Table>& tabs, int tbl, bool dcurve, const int32_t a[4]) {
  Want w;
  for (int l = 0; l < 4; ++l) {
    zref::SatLedger L{};
    int32_t src = a[l];
    int32_t dst = 0;
    zfield::steps::exec_op(dcurve ? zfield::OP_DCURVE : zfield::OP_CURVE, (uint32_t)tbl, tabs, &src,
                           &dst, &L);
    w.r[l] = dst;
    w.sat_add[l] = !dcurve && L.add > 0;
    w.sat_mul[l] = !dcurve && L.mul > 0;
    w.seg[l] = zfield::steps::segment_search(tabs[(size_t)tbl], a[l]);
  }
  return w;
}

void drive_req(Vzhao_probe_curve_svc& dut, int tbl, bool dcurve, const int32_t a[4], uint8_t tag) {
  dut.req_mode_i = dcurve ? 1 : 0;
  dut.req_tbl_i = (uint8_t)tbl;
  dut.req_a_0_i = (uint32_t)a[0];
  dut.req_a_1_i = (uint32_t)a[1];
  dut.req_a_2_i = (uint32_t)a[2];
  dut.req_a_3_i = (uint32_t)a[3];
  dut.req_tag_i = tag;
  dut.req_valid_i = 1;
}

void check_rsp(Vzhao_probe_curve_svc& dut, const Want& w, uint8_t tag, const char* what) {
  const std::string t(what);
  const int32_t got[4] = {(int32_t)dut.rsp_r_0_o, (int32_t)dut.rsp_r_1_o, (int32_t)dut.rsp_r_2_o,
                          (int32_t)dut.rsp_r_3_o};
  for (int l = 0; l < 4; ++l) {
    check(got[l] == w.r[l], (t + ": lane value").c_str(), (uint32_t)w.r[l], (uint32_t)got[l]);
    check(((dut.rsp_sat_add_o >> l) & 1) == (w.sat_add[l] ? 1 : 0), (t + ": add flag").c_str(),
          w.sat_add[l] ? 1 : 0, (dut.rsp_sat_add_o >> l) & 1);
    check(((dut.rsp_sat_mul_o >> l) & 1) == (w.sat_mul[l] ? 1 : 0), (t + ": mul flag").c_str(),
          w.sat_mul[l] ? 1 : 0, (dut.rsp_sat_mul_o >> l) & 1);
    check(((dut.rsp_seg_o >> (6 * l)) & 0x3F) == (uint32_t)w.seg[l], (t + ": segment").c_str(),
          (uint32_t)w.seg[l], (dut.rsp_seg_o >> (6 * l)) & 0x3F);
  }
  check(dut.rsp_tag_o == tag, (t + ": tag").c_str(), tag, dut.rsp_tag_o);
}

/** One lone request through an idle service; returns reply latency. */
int run_one(Vzhao_probe_curve_svc& dut, MulBank& mb, const std::vector<zfield::Table>& tabs,
            int tbl, bool dcurve, const int32_t a[4], uint8_t tag, const char* what) {
  drive_req(dut, tbl, dcurve, a, tag);
  dut.rsp_ready_i = 1;
  dut.eval();
  int guard = 0;
  while (!dut.req_ready_o && guard++ < 128) step(dut, mb);
  step(dut, mb);  // accepted
  dut.req_valid_i = 0;
  dut.eval();
  int cycles = 0;
  while (!dut.rsp_valid_o && cycles < 256) {
    step(dut, mb);
    ++cycles;
  }
  check(cycles < 256, (std::string(what) + ": reply arrived").c_str(), 1, cycles < 256 ? 1 : 0);
  check_rsp(dut, oracle(tabs, tbl, dcurve, a), tag, what);
  step(dut, mb);  // reply taken
  dut.eval();
  return cycles;
}

zfield::Table make_ramp(int n, int32_t x0, int32_t gap, int32_t y0, int32_t dy) {
  zfield::Table t;
  t.kind = 0;
  for (int i = 0; i < n; ++i) {
    t.x.push_back(x0 + gap * i);
    t.y.push_back(y0 + 1000 * i);
    t.dy.push_back(dy + 7 * i);
  }
  return t;
}

zfield::Table random_table(Prng& r) {
  static const int sizes[] = {2, 3, 4, 5, 8, 16, 33, 64};
  const int n = sizes[r.below(8)];
  zfield::Table t;
  t.kind = 0;
  const bool wide = r.below(4) == 0;  // a quarter of tables span most of s32
  int64_t x = wide ? INT32_MIN : (int32_t)(r.next64() & 0x3FFFFFFF) - 0x20000000;
  for (int i = 0; i < n; ++i) {
    t.x.push_back((int32_t)x);
    const int64_t gap =
        wide ? (int64_t)(UINT64_C(0xFFFFFFFF) / (uint32_t)n) : (int64_t)(1 + r.below(1 << 26));
    x += gap;
    if (x > INT32_MAX) x = INT32_MAX;
    const bool big = r.below(8) == 0;
    t.y.push_back(big ? (int32_t)r.next64() : (int32_t)(r.next64() & 0xFFFFF) - 0x80000);
    t.dy.push_back(big ? (int32_t)r.next64() : (int32_t)(r.next64() & 0xFFFFF) - 0x80000);
  }
  // strictly increasing except possible clamp pileup at INT32_MAX; dedup tail
  for (int i = 1; i < n; ++i) {
    if (t.x[(size_t)i] <= t.x[(size_t)i - 1]) t.x[(size_t)i] = t.x[(size_t)i - 1];
  }
  return t;
}

int32_t interesting_a(Prng& r, const zfield::Table& t) {
  const int n = (int)t.x.size();
  switch (r.below(8)) {
    case 0:
      return t.x[(size_t)r.below((uint32_t)n)];  // a knot
    case 1:
      return t.x[(size_t)r.below((uint32_t)n)] + 1;
    case 2:
      return t.x[(size_t)r.below((uint32_t)n)] - 1;
    case 3: {  // inside a random segment
      const int i = (int)r.below((uint32_t)(n - 1));
      const int64_t lo = t.x[(size_t)i], hi = t.x[(size_t)i + 1];
      return (int32_t)(lo + (int64_t)(r.next64() % (uint64_t)(hi - lo + 1)));
    }
    case 4:
      return (t.x[0] == INT32_MIN) ? INT32_MIN : t.x[0] - (int32_t)(1 + r.below(1000));
    case 5: {
      const int32_t xe = t.x[(size_t)(n - 1)];
      return (xe == INT32_MAX) ? INT32_MAX : xe + (int32_t)(1 + r.below(1000));
    }
    default:
      return (int32_t)r.next64();
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_probe_curve_svc dut;
  MulBank mb;
  dut.rst_n = 0;
  dut.req_valid_i = 0;
  dut.rsp_ready_i = 0;
  dut.tl_we_i = 0;
  dut.tl_commit_i = 0;
  dut.mul_valid_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  Prng rng(random_n ? 0xC0BBE + random_n : 0xC0BBE);

  // The four resident tables (the active-program table cache's contents).
  std::vector<zfield::Table> tabs(4);

  if (random_n == 0) {
    printf("== section 1: directed exactness over four resident tables ==\n");
    {
      tabs[0] = make_ramp(2, -1000, 2000, -50, 3);             // minimal table
      tabs[1] = make_ramp(3, 0, 65536, 100, -9);               // tiny, non-pow2
      tabs[2] = make_ramp(33, -400000, 30000, -12345, 250);    // 33 knots
      tabs[3] = make_ramp(64, INT32_MIN / 2, 1 << 24, 7, 11);  // full size
      for (int s = 0; s < 4; ++s) load_table(dut, mb, s, tabs[(size_t)s]);

      uint8_t tag = 0x10;
      for (int s = 0; s < 4; ++s) {
        const zfield::Table& t = tabs[(size_t)s];
        const int n = (int)t.x.size();
        std::vector<int32_t> probes;
        for (int i = 0; i < n; ++i) {
          probes.push_back(t.x[(size_t)i]);
          probes.push_back(t.x[(size_t)i] - 1);
          probes.push_back(t.x[(size_t)i] + 1);
          if (i + 1 < n)
            probes.push_back((int32_t)(((int64_t)t.x[(size_t)i] + t.x[(size_t)i + 1]) / 2));
        }
        probes.push_back(t.x[0] - 100000);                // below: clamp
        probes.push_back(t.x[(size_t)(n - 1)] + 100000);  // above: clamp
        // pad to a multiple of 4
        while (probes.size() % 4) probes.push_back(t.x[0]);
        for (size_t p = 0; p < probes.size(); p += 4) {
          int32_t a[4] = {probes[p], probes[p + 1], probes[p + 2], probes[p + 3]};
          char msg[64];
          snprintf(msg, sizeof msg, "tbl %d CURVE probe group %zu", s, p / 4);
          run_one(dut, mb, tabs, s, false, a, tag++, msg);
          snprintf(msg, sizeof msg, "tbl %d DCURVE probe group %zu", s, p / 4);
          run_one(dut, mb, tabs, s, true, a, tag++, msg);
        }
      }
    }

    printf("== section 2: saturation attribution, mixed within one group ==\n");
    {
      // Segment 0 spans INT32_MIN..INT32_MAX-1 (wider than s32 holds), with
      // dy[0] = INT32_MAX. Four lanes, four distinct flag patterns in ONE
      // group:
      //   lane 0  a = INT32_MIN          d_off 0, product 0         clean
      //   lane 1  a = INT32_MIN + 2^20   d_off 2^20 (fits), product
      //                                  2^51 -> resc16 2^35        MUL only
      //   lane 2  a = 5                  d_off 2^31+5 SATURATES,
      //                                  then INT32_MAX * INT32_MAX ADD+MUL
      //   lane 3  a = INT32_MAX          lands segment 2, d_off 0   clean
      zfield::Table t;
      t.kind = 0;
      t.x = {INT32_MIN, INT32_MAX - 1, INT32_MAX};
      t.y = {7, 100, 200};
      t.dy = {INT32_MAX, 5, 9};
      tabs[1] = t;
      load_table(dut, mb, 1, t);
      const int32_t a[4] = {INT32_MIN, INT32_MIN + (1 << 20), 5, INT32_MAX};
      run_one(dut, mb, tabs, 1, false, a, 0xE1, "saturating group (CURVE)");
      run_one(dut, mb, tabs, 1, true, a, 0xE2, "same group as DCURVE records NOTHING");
    }

    printf("== section 3: slot reload refreshes the cached meta ==\n");
    {
      // Replace table 2 with different bounds AND a different entry 0; probes
      // that fell inside the old range now clamp, and vice versa.
      tabs[2] = make_ramp(5, 1000000, 500000, 42, -17);
      load_table(dut, mb, 2, tabs[2]);
      const int32_t a[4] = {-400000, 1000000, 2500000, 99999999};
      run_one(dut, mb, tabs, 2, false, a, 0xE3, "reloaded slot, fresh bounds");
      run_one(dut, mb, tabs, 2, true, a, 0xE4, "reloaded slot DCURVE");
    }

    printf("== section 4: lone-request latency ==\n");
    {
      const int32_t a[4] = {12345, -6789, 100000, -100000};
      const int lat = run_one(dut, mb, tabs, 3, false, a, 0xE5, "latency probe");
      printf("   measured lone-reply latency: %d cycles\n", lat);
      check(lat <= 24, "lone reply within 24 cycles (13-cycle search + finish)", 24, lat);
    }

    printf("== section 5: II gate — streaming groups ==\n");
    {
      const int N = 32;
      int32_t pend[N][4];
      int tbl[N];
      bool dc[N];
      Want want[N];
      for (int i = 0; i < N; ++i) {
        tbl[i] = (int)rng.below(4);
        dc[i] = rng.below(4) == 0;
        for (int l = 0; l < 4; ++l) pend[i][l] = interesting_a(rng, tabs[(size_t)tbl[i]]);
        want[i] = oracle(tabs, tbl[i], dc[i], pend[i]);
      }
      dut.rsp_ready_i = 1;
      int accepted = 0, drained = 0, cycle = 0;
      int first_accept = -1, last_accept = -1;
      drive_req(dut, tbl[0], dc[0], pend[0], (uint8_t)(0x40 + 0));
      dut.eval();
      int guard = 0;
      while (drained < N && guard++ < 20000) {
        const bool fire_req = accepted < N && dut.req_valid_i && dut.req_ready_o;
        const bool fire_rsp = dut.rsp_valid_o != 0;
        if (fire_rsp) {
          char msg[48];
          snprintf(msg, sizeof msg, "stream reply %d", drained);
          check_rsp(dut, want[drained], (uint8_t)(0x40 + drained), msg);
          ++drained;
        }
        step(dut, mb);
        ++cycle;
        if (fire_req) {
          if (first_accept < 0) first_accept = cycle;
          last_accept = cycle;
          ++accepted;
          if (accepted < N) {
            drive_req(dut, tbl[accepted], dc[accepted], pend[accepted], (uint8_t)(0x40 + accepted));
          } else {
            dut.req_valid_i = 0;
          }
        }
        dut.eval();
      }
      check(drained == N, "all streamed replies arrived", N, drained);
      const int ii_num = last_accept - first_accept;
      const int ii = (N > 1) ? (ii_num + (N - 2)) / (N - 1) : 0;  // ceil
      printf("   MEASURED four-point CURVE II over %d groups: %d clocks (span %d)\n", N, ii,
             ii_num);
      check(ii <= 14, "THE GATE: four-point CURVE II <= 14", 14, ii);
    }

    printf("== section 6: reply backpressure — capacity 4, order intact ==\n");
    {
      dut.rsp_ready_i = 0;
      dut.eval();
      int32_t grp[4][4];
      Want want[4];
      const uint8_t tags[4] = {0xA1, 0xB2, 0xC3, 0xD4};
      for (int g = 0; g < 4; ++g) {
        for (int l = 0; l < 4; ++l) grp[g][l] = interesting_a(rng, tabs[0]);
        want[g] = oracle(tabs, 0, false, grp[g]);
        drive_req(dut, 0, false, grp[g], tags[g]);
        dut.eval();
        int guard = 0;
        while (!(dut.req_valid_i && dut.req_ready_o) && guard++ < 200) step(dut, mb);
        check(guard < 200, "backpressure: request accepted", 1, guard < 200 ? 1 : 0);
        step(dut, mb);
        dut.req_valid_i = 0;
        dut.eval();
      }
      // The FIFTH offer must be refused for as long as replies stay blocked.
      int32_t g5[4] = {1, 2, 3, 4};
      drive_req(dut, 0, false, g5, 0xEE);
      dut.eval();
      for (int c = 0; c < 64; ++c) {
        check(dut.req_ready_o == 0, "fifth group refused while replies blocked", 0,
              dut.req_ready_o);
        step(dut, mb);
      }
      dut.req_valid_i = 0;
      dut.eval();
      // Unblock: the four replies drain in ACCEPT order with their own tags.
      dut.rsp_ready_i = 1;
      dut.eval();
      for (int g = 0; g < 4; ++g) {
        int guard = 0;
        while (!dut.rsp_valid_o && guard++ < 200) step(dut, mb);
        char msg[48];
        snprintf(msg, sizeof msg, "blocked drain %d", g);
        check_rsp(dut, want[g], tags[g], msg);
        step(dut, mb);
      }
    }
  } else {
    printf("== random differential: %d groups against zfield::exec_op ==\n", random_n);
    for (int s = 0; s < 4; ++s) {
      tabs[(size_t)s] = random_table(rng);
      load_table(dut, mb, s, tabs[(size_t)s]);
    }
    for (int i = 0; i < random_n; ++i) {
      if (rng.below(64) == 0) {  // reload a random slot mid-run
        const int s = (int)rng.below(4);
        tabs[(size_t)s] = random_table(rng);
        load_table(dut, mb, s, tabs[(size_t)s]);
      }
      const int tbl = (int)rng.below(4);
      const bool dc = rng.below(4) == 0;
      int32_t a[4];
      for (int l = 0; l < 4; ++l) a[l] = interesting_a(rng, tabs[(size_t)tbl]);
      char msg[48];
      snprintf(msg, sizeof msg, "random group %d", i);
      run_one(dut, mb, tabs, tbl, dc, a, (uint8_t)(i & 0xFF), msg);
    }
  }

  return zhao::report_and_exit("field_curve_svc_directed");
}
