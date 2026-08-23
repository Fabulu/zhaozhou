// field_curve_directed.cpp — OP_CURVE, OP_DCURVE and OP_SPLINE, RTL against the
// interpreter itself.
//
// These three ops are the first in the engine that read a TABLE, so this file
// carries two oracles rather than one:
//
//   * `zfield::interpret` on a two-instruction program (the op, then OP_END).
//     That is the shipped path, not a restatement of it, and it decides every
//     VALUE.
//   * a restatement of the per-lane saturation attribution, because
//     `Status.sat` collapses all five SatLedger lanes into one bit and the RTL
//     reports three of them separately. The restatement builds on `zref::fx_*`,
//     so only the ATTRIBUTION is restated here; the rounding law still has one
//     implementation.
//
// Every check runs both, so the two oracles also police each other: a
// restatement that drifts from the interpreter fails on the value.
//
// SIX LAWS, each one a place an implementation drifts:
//
//   1. THE SEARCH IS SIX STEPS, ALWAYS -- not ceil(log2(n)). Section 3 sweeps
//      every table size from 2 to 64 and probes every knot, every knot +/- 1
//      and every segment midpoint, so a step count derived from n is wrong at
//      some size.
//   2. THE SEARCH RUNS ON THE CLAMPED VALUE. Section 2 probes far outside both
//      ends, where searching raw `a` gives a different index.
//   3. `dy` MEANS SLOPE IN A CURVE TABLE AND 1/step IN A SPLINE TABLE. This is
//      a fact about the ENCODING, not about the RTL -- the datapath multiplies
//      by dy either way and no mutation of it can be caught here. Section 5
//      therefore checks the consequence that IS visible: on a well-formed
//      spline table the segment parameter runs 0 -> 1 across each segment.
//   4. SPLINE's FINAL TERM IS rescale_s32(v, 1), NOT v << 16 and NOT v >> 1.
//      Section 7 counts the cases where v is odd, and asserts the count is
//      non-zero, because round-half-up and truncation agree on every even v.
//   5. FOUR LEDGER LANES IN ONE OP. Section 8 drives each lane on its own.
//   6. THE ENDS ARE REPLICATED, NOT EXTRAPOLATED. Section 6 uses a table whose
//      end slopes are steep, where replication and extrapolation diverge hard.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_curve_tb.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"

namespace {

using zhao::check;

constexpr uint8_t M_CURVE = 0, M_DCURVE = 1, M_SPLINE = 2;
constexpr int32_t kOne = 1 << 16;

inline zref::fx16 F(int32_t raw) { return zref::fx16{raw}; }

struct Tab {
  uint8_t kind = 0;
  std::vector<int32_t> x, y, dy;
  int n() const { return static_cast<int>(x.size()); }
};

/** The table as the DUT sees it: out-of-range reads answer HOSTILE. */
struct TabPort {
  const Tab* t = nullptr;
  int32_t x_of(uint32_t i) const {
    // INT32_MIN compares <= every clamped value, so a missing `mid <= n-1`
    // guard walks straight off the end instead of quietly agreeing.
    return i < static_cast<uint32_t>(t->n()) ? t->x[i] : INT32_MIN;
  }
  int32_t y_of(uint32_t i) const {
    return i < static_cast<uint32_t>(t->n()) ? t->y[i] : static_cast<int32_t>(0x5A5A5A5A);
  }
  int32_t dy_of(uint32_t i) const {
    return i < static_cast<uint32_t>(t->n()) ? t->dy[i] : static_cast<int32_t>(0x5A5A5A5A);
  }
};

// ---------------------------------------------------------------- oracle 1 --

/** One op as a real program, run through the shipped interpreter. */
struct InterpRes {
  int32_t value = 0;
  bool sat = false;
};

InterpRes interp(uint8_t mode, int32_t a, const Tab& t) {
  zfield::Decoded prog;
  prog.profile = 0;
  prog.tables.push_back(zfield::Table{t.kind, t.x, t.y, t.dy});

  uint8_t op = zfield::OP_CURVE;
  if (mode == M_DCURVE) op = zfield::OP_DCURVE;
  if (mode == M_SPLINE) op = zfield::OP_SPLINE;

  zfield::Instr ins{};
  ins.op = op;
  ins.dst = 1;
  ins.a = 0;
  ins.b = 0;
  ins.c = 0;
  ins.imm = 0;  // table 0
  prog.instrs.push_back(ins);

  zfield::Instr end{};
  end.op = zfield::OP_END;
  prog.instrs.push_back(end);

  zfield::IoLane in_lane{};
  in_lane.name = "a";
  in_lane.type = 0;
  in_lane.reg = 0;
  prog.in_lanes.push_back(in_lane);

  zfield::IoLane out_lane{};
  out_lane.name = "r";
  out_lane.type = 0;
  out_lane.reg = 1;
  prog.out_lanes.push_back(out_lane);

  int32_t out = 0;
  const zfield::Status st = zfield::interpret(prog, &a, 1, &out, 1);
  return InterpRes{out, st.sat};
}

// ---------------------------------------------------------------- oracle 2 --

struct Lanes {
  int32_t value = 0;
  int seg = 0;
  int32_t tt = 0;  // SPLINE only
  int32_t v = 0;   // SPLINE only, the pre-halving Horner result
  bool add = false, mul = false, rescale = false;
};

int32_t clamp_raw(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

/** Law 1: six steps, k = 5..0, guarded by `mid <= n-1`. */
int seg_search(const Tab& t, int32_t a) {
  const int n = t.n();
  const int32_t c = clamp_raw(a, t.x[0], t.x[n - 1]);
  int lo = 0;
  for (int k = 5; k >= 0; --k) {
    const int mid = lo + (1 << k);
    if (mid <= n - 1 && t.x[mid] <= c) lo = mid;
  }
  return lo;
}

int32_t sat_s32_i64(int64_t v, bool* rescale) {
  if (v > INT32_MAX) {
    *rescale = true;
    return INT32_MAX;
  }
  if (v < INT32_MIN) {
    *rescale = true;
    return INT32_MIN;
  }
  return static_cast<int32_t>(v);
}

Lanes lanes_of(uint8_t mode, int32_t a, const Tab& t) {
  Lanes o;
  const int n = t.n();
  const int i = seg_search(t, a);
  o.seg = i;
  const int32_t c = clamp_raw(a, t.x[0], t.x[n - 1]);

  if (mode == M_DCURVE) {
    o.value = t.dy[i];
    return o;
  }

  zref::SatLedger L{};
  if (mode == M_CURVE) {
    const zref::fx16 d = zref::fx_sub(F(c), F(t.x[i]), &L);
    o.value = zref::fx_mad(d, F(t.dy[i]), F(t.y[i]), &L).raw;
    o.add = L.add != 0;
    o.mul = L.mul != 0;
    o.rescale = L.rescale != 0;
    return o;
  }

  // SPLINE. Four lanes: `add` for the fx_sub and the closing fx_add, `mul` for
  // the three Horner steps, `rescale` for the t product, the three coefficient
  // saturates and the halving.
  const int32_t d = zref::fx_sub(F(c), F(t.x[i]), &L).raw;
  const int32_t tt_raw =
      zref::rescale_s32(static_cast<int64_t>(d) * static_cast<int64_t>(t.dy[i]), 16, &L);
  const int32_t tt = zref::fx_clamp(F(tt_raw), F(0), F(kOne)).raw;
  o.tt = tt;

  const int32_t p0 = t.y[i > 0 ? i - 1 : 0];
  const int32_t p1 = t.y[i];
  const int32_t p2 = t.y[i + 1 < n ? i + 1 : n - 1];
  const int32_t p3 = t.y[i + 2 < n ? i + 2 : n - 1];

  bool coef_sat = false;
  const int32_t C1 = sat_s32_i64(static_cast<int64_t>(p2) - p0, &coef_sat);
  const int32_t C2 = sat_s32_i64(2 * static_cast<int64_t>(p0) - 5 * static_cast<int64_t>(p1) +
                                     4 * static_cast<int64_t>(p2) - p3,
                                 &coef_sat);
  const int32_t C3 = sat_s32_i64(
      -static_cast<int64_t>(p0) + 3 * static_cast<int64_t>(p1) - 3 * static_cast<int64_t>(p2) + p3,
      &coef_sat);

  int32_t u = zref::fx_mad(F(tt), F(C3), F(C2), &L).raw;
  u = zref::fx_mad(F(tt), F(u), F(C1), &L).raw;
  const int32_t v = zref::fx_mul(F(tt), F(u), &L).raw;
  o.v = v;

  const int32_t half = zref::rescale_s32(static_cast<int64_t>(v), 1, &L);
  o.value = zref::fx_add(F(p1), F(half), &L).raw;

  o.add = L.add != 0;
  o.mul = L.mul != 0;
  o.rescale = (L.rescale != 0) || coef_sat;
  return o;
}

// -------------------------------------------------------------------- DUT --

struct Res {
  int32_t value = 0;
  int seg = 0;
  bool add = false, mul = false, rescale = false;
  int cycles = 0;
};

/** One cycle with the table serviced: the index presented this cycle answers on
 *  the next one, which is what a registered M10K read does. */
void tick_tbl(Vzhao_field_curve_tb& dut, const TabPort& p) {
  const uint32_t idx = dut.tbl_idx_o;
  zhao::tick(dut);
  dut.tbl_x_i = static_cast<uint32_t>(p.x_of(idx));
  dut.tbl_y_i = static_cast<uint32_t>(p.y_of(idx));
  dut.tbl_dy_i = static_cast<uint32_t>(p.dy_of(idx));
  dut.eval();
}

Res run(Vzhao_field_curve_tb& dut, uint8_t mode, int32_t a, const Tab& t, int stall_after = -1) {
  const TabPort p{&t};
  dut.v_valid_i = 1;
  dut.mode_i = mode;
  dut.a_i = static_cast<uint32_t>(a);
  dut.tbl_n_i = static_cast<uint8_t>(t.n());
  dut.r_ready_i = stall_after < 0 ? 1 : 0;
  dut.eval();

  int guard = 0;
  while (!dut.v_ready_o && guard++ < 256) tick_tbl(dut, p);
  tick_tbl(dut, p);
  dut.v_valid_i = 0;
  dut.eval();

  Res r;
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 256) {
    tick_tbl(dut, p);
    ++cycles;
  }
  if (stall_after >= 0) {
    for (int s = 0; s < stall_after; ++s) tick_tbl(dut, p);
    dut.r_ready_i = 1;
    dut.eval();
  }
  r.value = static_cast<int32_t>(dut.result_o);
  r.seg = static_cast<int>(dut.seg_idx_o);
  r.add = dut.sat_add_o != 0;
  r.mul = dut.sat_mul_o != 0;
  r.rescale = dut.sat_rescale_o != 0;
  r.cycles = cycles;
  tick_tbl(dut, p);  // the result is taken
  return r;
}

int g_odd_v = 0;
int g_neg_v = 0;

void diff(Vzhao_field_curve_tb& dut, uint8_t mode, int32_t a, const Tab& t, const char* what) {
  const InterpRes want = interp(mode, a, t);
  const Lanes lane = lanes_of(mode, a, t);
  const Res got = run(dut, mode, a, t);
  const std::string s(what);

  // The two oracles police each other before either judges the RTL.
  check(lane.value == want.value, (s + ": oracles agree").c_str(),
        static_cast<uint32_t>(want.value), static_cast<uint32_t>(lane.value));
  check(got.value == want.value, (s + ": value").c_str(), static_cast<uint32_t>(want.value),
        static_cast<uint32_t>(got.value));
  check(got.seg == lane.seg, (s + ": segment").c_str(), static_cast<uint32_t>(lane.seg),
        static_cast<uint32_t>(got.seg));
  check(got.add == lane.add, (s + ": SatLedger::add").c_str(), lane.add ? 1 : 0, got.add ? 1 : 0);
  check(got.mul == lane.mul, (s + ": SatLedger::mul").c_str(), lane.mul ? 1 : 0, got.mul ? 1 : 0);
  check(got.rescale == lane.rescale, (s + ": SatLedger::rescale").c_str(), lane.rescale ? 1 : 0,
        got.rescale ? 1 : 0);
  check((got.add || got.mul || got.rescale) == want.sat, (s + ": Status.sat").c_str(),
        want.sat ? 1 : 0, (got.add || got.mul || got.rescale) ? 1 : 0);

  if (mode == M_SPLINE) {
    if ((lane.v & 1) != 0) ++g_odd_v;
    if (lane.v < 0) ++g_neg_v;
  }
}

// ------------------------------------------------------------------ tables --

/** A curve table: n knots at `step` apart from `x0`, y from `f`, dy the exact
 *  segment slope so the piecewise-linear reading is continuous. */
Tab make_curve(int n, int32_t x0, int32_t step, const int32_t* yv) {
  Tab t;
  t.kind = 0;
  for (int i = 0; i < n; ++i) {
    t.x.push_back(x0 + static_cast<int32_t>(step) * i);
    t.y.push_back(yv[i]);
  }
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1 < n) ? i + 1 : i;
    const int64_t dyv = (j == i) ? 0 : ((static_cast<int64_t>(t.y[j]) - t.y[i]) << 16) / step;
    t.dy.push_back(static_cast<int32_t>(dyv));
  }
  return t;
}

/** A spline table: uniform spacing, dy = 1/step as the encoding requires. */
Tab make_spline(int n, int32_t x0, int32_t step, const int32_t* yv) {
  Tab t;
  t.kind = 1;
  const int32_t inv = static_cast<int32_t>((static_cast<int64_t>(kOne) << 16) / step);
  for (int i = 0; i < n; ++i) {
    t.x.push_back(x0 + static_cast<int32_t>(step) * i);
    t.y.push_back(yv[i]);
    t.dy.push_back(inv);
  }
  return t;
}

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

/** A random but LAWFUL table: strictly increasing x, 2..64 entries. */
Tab random_table(Prng& rng, bool spline) {
  Tab t;
  t.kind = spline ? 1 : 0;
  const int n = 2 + static_cast<int>(rng.below(63));
  // Keep x inside a range where n strictly-increasing knots always fit.
  int32_t x = static_cast<int32_t>(rng.next()) >> 8;
  const int32_t step = 1 + static_cast<int32_t>(rng.below(1u << 18));
  for (int i = 0; i < n; ++i) {
    t.x.push_back(x);
    x += step;  // uniform, so the same generator serves both kinds
    switch (rng.below(6)) {
      case 0:
        t.y.push_back(0);
        break;
      case 1:
        t.y.push_back(INT32_MAX);
        break;
      case 2:
        t.y.push_back(INT32_MIN);
        break;
      case 3:
        t.y.push_back(static_cast<int32_t>(rng.next()) >> 12);
        break;
      default:
        t.y.push_back(static_cast<int32_t>(rng.next()));
        break;
    }
  }
  const int32_t inv = static_cast<int32_t>((static_cast<int64_t>(kOne) << 16) / step);
  for (int i = 0; i < n; ++i) {
    if (spline) {
      t.dy.push_back(inv);
    } else {
      switch (rng.below(5)) {
        case 0:
          t.dy.push_back(0);
          break;
        case 1:
          t.dy.push_back(INT32_MAX);
          break;
        case 2:
          t.dy.push_back(static_cast<int32_t>(rng.next()) >> 14);
          break;
        default:
          t.dy.push_back(static_cast<int32_t>(rng.next()));
          break;
      }
    }
  }
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_field_curve_tb dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 1;
  dut.tbl_x_i = 0;
  dut.tbl_y_i = 0;
  dut.tbl_dy_i = 0;
  dut.tbl_n_i = 2;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 1. CURVE on a plain ramp -----------------------------------------
  {
    const int32_t yv[5] = {0, kOne, 2 * kOne, 4 * kOne, 4 * kOne};
    const Tab t = make_curve(5, 0, kOne, yv);
    for (int i = 0; i < 5; ++i) {
      char nm[64];
      std::snprintf(nm, sizeof(nm), "1.knot[%d]", i);
      diff(dut, M_CURVE, t.x[i], t, nm);
    }
    for (int i = 0; i + 1 < 5; ++i) {
      char nm[64];
      std::snprintf(nm, sizeof(nm), "1.mid[%d]", i);
      diff(dut, M_CURVE, t.x[i] + kOne / 2, t, nm);
      std::snprintf(nm, sizeof(nm), "1.knot-1[%d]", i);
      diff(dut, M_CURVE, t.x[i + 1] - 1, t, nm);
    }
  }

  // ---- 2. The clamp: law 2 ------------------------------------------------
  {
    const int32_t yv[4] = {-3 * kOne, kOne, 7 * kOne, -2 * kOne};
    const Tab t = make_curve(4, 1000, kOne, yv);
    diff(dut, M_CURVE, INT32_MIN, t, "2.curve far below");
    diff(dut, M_CURVE, INT32_MAX, t, "2.curve far above");
    diff(dut, M_CURVE, t.x[0] - 1, t, "2.curve just below");
    diff(dut, M_CURVE, t.x[3] + 1, t, "2.curve just above");
    diff(dut, M_DCURVE, INT32_MIN, t, "2.dcurve far below");
    diff(dut, M_DCURVE, INT32_MAX, t, "2.dcurve far above");
    diff(dut, M_SPLINE, INT32_MIN, t, "2.spline far below");
    diff(dut, M_SPLINE, INT32_MAX, t, "2.spline far above");

    // Clamping to an end pins the answer to that end's y, because the offset
    // is exactly zero there. Stated separately so the reason is checked and
    // not just the number.
    const Res below = run(dut, M_CURVE, INT32_MIN, t);
    check(below.value == t.y[0], "2.below == y[0]", static_cast<uint32_t>(t.y[0]),
          static_cast<uint32_t>(below.value));
    check(below.seg == 0, "2.below segment 0", 0, static_cast<uint32_t>(below.seg));
    const Res above = run(dut, M_CURVE, INT32_MAX, t);
    check(above.value == t.y[3], "2.above == y[n-1]", static_cast<uint32_t>(t.y[3]),
          static_cast<uint32_t>(above.value));
    check(above.seg == 3, "2.above segment n-1", 3, static_cast<uint32_t>(above.seg));
  }

  // ---- 2b. The HIGH end of the clamp, with a non-zero final slope --------
  {
    // The tables `make_curve` builds give the last knot slope zero, because
    // there is no segment after it. That hides the high clamp completely: with
    // dy[n-1] == 0 the extrapolation term vanishes whether or not the value was
    // clamped, and a block that only clamps the low end passes every case in
    // section 2. This table sets the final slope by hand.
    Tab t;
    t.kind = 0;
    t.x = {0, kOne, 2 * kOne};
    t.y = {0, kOne, 2 * kOne};
    t.dy = {kOne, kOne, 3 * kOne};  // a real slope at the top knot
    diff(dut, M_CURVE, 2 * kOne, t, "2b.at the top knot");
    diff(dut, M_CURVE, 2 * kOne + 1, t, "2b.one past the top");
    diff(dut, M_CURVE, 4 * kOne, t, "2b.well past the top");
    diff(dut, M_CURVE, INT32_MAX, t, "2b.far past the top");
    const Res past = run(dut, M_CURVE, 4 * kOne, t);
    check(past.value == t.y[2], "2b.clamped to y[n-1] despite the slope",
          static_cast<uint32_t>(t.y[2]), static_cast<uint32_t>(past.value));
  }

  // ---- 3. The six-step search across every size: law 1 -------------------
  {
    const int sizes[] = {2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 47, 63, 64};
    for (int si = 0; si < static_cast<int>(sizeof(sizes) / sizeof(sizes[0])); ++si) {
      const int n = sizes[si];
      std::vector<int32_t> yv(static_cast<size_t>(n));
      for (int i = 0; i < n; ++i) yv[static_cast<size_t>(i)] = (i * 37 - 400) << 12;
      const Tab t = make_curve(n, -50000, 4096, yv.data());
      for (int i = 0; i < n; ++i) {
        char nm[80];
        std::snprintf(nm, sizeof(nm), "3.n=%d knot[%d]", n, i);
        diff(dut, M_DCURVE, t.x[i], t, nm);
        std::snprintf(nm, sizeof(nm), "3.n=%d knot[%d]-1", n, i);
        diff(dut, M_DCURVE, t.x[i] - 1, t, nm);
        std::snprintf(nm, sizeof(nm), "3.n=%d knot[%d]+1", n, i);
        diff(dut, M_DCURVE, t.x[i] + 1, t, nm);
        if (i + 1 < n) {
          std::snprintf(nm, sizeof(nm), "3.n=%d mid[%d]", n, i);
          diff(dut, M_DCURVE, t.x[i] + 2048, t, nm);
        }
      }
    }
  }

  // ---- 4. DCURVE ignores y entirely --------------------------------------
  {
    const int32_t yv[6] = {0, 0, 0, 0, 0, 0};
    Tab a = make_curve(6, 0, 8192, yv);
    Tab b = a;
    for (int i = 0; i < 6; ++i) b.y[static_cast<size_t>(i)] = static_cast<int32_t>(0xDEADBEEF);
    for (int i = 0; i < 6; ++i) {
      a.dy[static_cast<size_t>(i)] = (i + 1) * 12345;
      b.dy[static_cast<size_t>(i)] = (i + 1) * 12345;
    }
    for (int i = 0; i < 6; ++i) {
      const Res ra = run(dut, M_DCURVE, a.x[i] + 100, a);
      const Res rb = run(dut, M_DCURVE, b.x[i] + 100, b);
      char nm[64];
      std::snprintf(nm, sizeof(nm), "4.dcurve[%d] ignores y", i);
      check(ra.value == rb.value && ra.value == a.dy[static_cast<size_t>(i)], nm,
            static_cast<uint32_t>(a.dy[static_cast<size_t>(i)]), static_cast<uint32_t>(ra.value));
    }
  }

  // ---- 5. SPLINE, and the segment parameter: law 3's visible consequence --
  {
    const int32_t yv[8] = {0, 2 * kOne, -kOne, 5 * kOne, 3 * kOne, 3 * kOne, -4 * kOne, kOne};
    const Tab t = make_spline(8, -4 * kOne, kOne, yv);
    for (int i = 0; i < 8; ++i) {
      char nm[64];
      std::snprintf(nm, sizeof(nm), "5.spline knot[%d]", i);
      diff(dut, M_SPLINE, t.x[i], t, nm);
    }
    for (int i = 0; i + 1 < 8; ++i) {
      for (int f = 1; f < 8; ++f) {
        char nm[64];
        std::snprintf(nm, sizeof(nm), "5.spline seg%d f%d", i, f);
        diff(dut, M_SPLINE, t.x[i] + (kOne * f) / 8, t, nm);
      }
    }
    // On a well-formed spline table t runs 0 at the knot and approaches 1 just
    // before the next one. That is what dy == 1/step buys.
    const Lanes at_knot = lanes_of(M_SPLINE, t.x[3], t);
    check(at_knot.tt == 0, "5.t == 0 at the knot", 0, static_cast<uint32_t>(at_knot.tt));
    const Lanes near_next = lanes_of(M_SPLINE, t.x[4] - 1, t);
    check(near_next.tt > kOne - 4 && near_next.tt <= kOne, "5.t -> 1 before the next knot",
          static_cast<uint32_t>(kOne), static_cast<uint32_t>(near_next.tt));
    // At the knot the spline passes exactly through p1.
    const InterpRes knot_v = interp(M_SPLINE, t.x[3], t);
    check(knot_v.value == t.y[3], "5.spline interpolates its knot", static_cast<uint32_t>(t.y[3]),
          static_cast<uint32_t>(knot_v.value));
  }

  // ---- 5b. A spline table whose dy does NOT match the spacing ------------
  {
    // The decoder validates a spline table's KIND, its entry count, its x order
    // and its uniform spacing -- and never its dy. So a program can be entirely
    // decodable and still carry a dy that is not 1/step, which drives the
    // segment parameter outside [0, 1]. The clamp is what makes the hardware
    // agree with the interpreter on such a program; without it these are the
    // only cases that diverge, and every well-formed table hides the fact.
    const int32_t yv[5] = {0, 3 * kOne, -2 * kOne, 5 * kOne, kOne};
    const int32_t bad_dy[] = {0, -kOne, -1, 16 * kOne, INT32_MAX, INT32_MIN, 1};
    int out_of_range = 0;
    for (int b = 0; b < static_cast<int>(sizeof(bad_dy) / sizeof(bad_dy[0])); ++b) {
      Tab t = make_spline(5, 0, kOne, yv);
      for (int i = 0; i < 5; ++i) t.dy[static_cast<size_t>(i)] = bad_dy[b];
      for (int f = 0; f < 4; ++f) {
        char nm[80];
        std::snprintf(nm, sizeof(nm), "5b.dy[%d] f%d", b, f);
        diff(dut, M_SPLINE, t.x[1] + (kOne * f) / 4, t, nm);
      }
      // Count the probes where the UNCLAMPED parameter really is out of range.
      // Without this the section could run entirely on values the clamp never
      // touches and still pass, which is how the hole got here in the first
      // place.
      for (int f = 0; f < 4; ++f) {
        const int64_t off = (kOne * f) / 4;
        const int64_t unclamped = (off * bad_dy[b] + (1 << 15)) >> 16;
        if (unclamped < 0 || unclamped > kOne) ++out_of_range;
      }
    }
    check(out_of_range > 0, "5b.the clamp is actually exercised", 1,
          static_cast<uint32_t>(out_of_range));
  }

  // ---- 6. End replication, not extrapolation: law 6 ----------------------
  {
    // Steep, alternating ends: replication and extrapolation diverge by a wide
    // margin in the first and last segments and agree nowhere.
    const int32_t yv[6] = {-8 * kOne, 6 * kOne, -5 * kOne, 4 * kOne, -7 * kOne, 9 * kOne};
    const Tab t = make_spline(6, 0, kOne, yv);
    for (int f = 0; f < 8; ++f) {
      char nm[64];
      std::snprintf(nm, sizeof(nm), "6.first seg f%d", f);
      diff(dut, M_SPLINE, t.x[0] + (kOne * f) / 8, t, nm);
      std::snprintf(nm, sizeof(nm), "6.last seg f%d", f);
      diff(dut, M_SPLINE, t.x[4] + (kOne * f) / 8, t, nm);
    }
    // The replication is what makes the first segment's p0 equal y[0]. Prove
    // the extrapolated alternative would have been visibly different.
    const Lanes mid = lanes_of(M_SPLINE, t.x[0] + kOne / 2, t);
    const int64_t extrap_p0 = 2LL * t.y[0] - t.y[1];
    check(extrap_p0 != t.y[0], "6.extrapolation would differ", 1, extrap_p0 != t.y[0] ? 1 : 0);
    check(mid.seg == 0, "6.first segment index", 0, static_cast<uint32_t>(mid.seg));
  }

  // ---- 7. The halving: law 4 ---------------------------------------------
  {
    // A dense sweep whose only purpose is to produce ODD v, where round-half-up
    // and truncation part company, and NEGATIVE odd v, where they part company
    // in the other direction.
    const int32_t yv[4] = {1, -3, 7, -11};  // tiny, so v stays small and odd often
    const Tab t = make_spline(4, 0, kOne, yv);
    const int before_odd = g_odd_v, before_neg = g_neg_v;
    for (int f = 0; f < 64; ++f) {
      char nm[64];
      std::snprintf(nm, sizeof(nm), "7.halve f%d", f);
      diff(dut, M_SPLINE, t.x[0] + (kOne * f) / 64, t, nm);
      std::snprintf(nm, sizeof(nm), "7.halve b f%d", f);
      diff(dut, M_SPLINE, t.x[1] + (kOne * f) / 64, t, nm);
    }
    check(g_odd_v - before_odd > 0, "7.odd v actually covered", 1,
          static_cast<uint32_t>(g_odd_v - before_odd));
    check(g_neg_v - before_neg > 0, "7.negative v actually covered", 1,
          static_cast<uint32_t>(g_neg_v - before_neg));
  }

  // ---- 8. The four lanes, one at a time: law 5 ---------------------------
  {
    // `add`: the fx_sub overflows. This needs a WIDE INTERIOR segment, not a
    // wide table -- outside the ends the clamp pins the value to x[i] itself
    // and the difference is exactly zero, so an out-of-range probe saturates
    // nothing. Two knots at the rails leave one segment spanning the whole
    // range, and a value in the middle of it is 2^31 away from x[0].
    Tab t;
    t.kind = 0;
    t.x = {INT32_MIN, INT32_MAX};
    t.y = {0, 0};
    t.dy = {1, 1};  // tiny slope, so `mul` stays quiet and `add` is isolated
    diff(dut, M_CURVE, 0, t, "8.add lane");
    const Res add_only = run(dut, M_CURVE, 0, t);
    check(add_only.add, "8.add lane fired", 1, add_only.add ? 1 : 0);
    check(!add_only.mul, "8.add lane alone", 0, add_only.mul ? 1 : 0);

    // `mul`: a big offset against a big slope saturates the mad.
    Tab m;
    m.kind = 0;
    m.x = {0, 1 << 20};
    m.y = {INT32_MAX, INT32_MAX};
    m.dy = {INT32_MAX, INT32_MAX};
    diff(dut, M_CURVE, 1 << 19, m, "8.mul lane");
    const Res mul_only = run(dut, M_CURVE, 1 << 19, m);
    check(mul_only.mul, "8.mul lane fired", 1, mul_only.mul ? 1 : 0);

    // `rescale`: the SPLINE coefficient combination overflows s32 on its own.
    Tab s;
    s.kind = 1;
    s.x = {0, kOne, 2 * kOne, 3 * kOne};
    s.y = {INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN};
    s.dy = {kOne, kOne, kOne, kOne};
    diff(dut, M_SPLINE, kOne + kOne / 3, s, "8.rescale lane");
    const Res resc_only = run(dut, M_SPLINE, kOne + kOne / 3, s);
    check(resc_only.rescale, "8.rescale lane fired", 1, resc_only.rescale ? 1 : 0);

    // And a quiet case, so "always report saturation" is not a passing answer.
    const int32_t yv[4] = {0, kOne, 2 * kOne, 3 * kOne};
    const Tab q = make_spline(4, 0, kOne, yv);
    const Res quiet = run(dut, M_SPLINE, kOne / 2, q);
    check(!quiet.add && !quiet.mul && !quiet.rescale, "8.quiet case reports nothing", 0,
          (quiet.add ? 4u : 0u) | (quiet.mul ? 2u : 0u) | (quiet.rescale ? 1u : 0u));
  }

  // ---- 8b. A3b: the mad rounds ONCE, and it shows only at the rails ------
  {
    // rescale(d*dy + (y << 16), 16) and rescale(d*dy, 16) + y are equal for
    // every value that fits -- ((p + c*2^16 + 2^15) >> 16) is exactly
    // ((p + 2^15) >> 16) + c. They part company ONLY where the intermediate
    // saturates and the sum would have pulled it back, so a directed set that
    // never reaches the rail cannot tell the two apart at all.
    Tab t;
    t.kind = 0;
    t.x = {0, 1 << 20};
    t.y = {INT32_MIN, INT32_MIN};
    t.dy = {INT32_MAX, INT32_MAX};
    diff(dut, M_CURVE, 1 << 19, t, "8b.positive rail");
    const Res hi = run(dut, M_CURVE, 1 << 19, t);
    check(hi.value == INT32_MAX, "8b.saturates high, not wrapped back",
          static_cast<uint32_t>(INT32_MAX), static_cast<uint32_t>(hi.value));

    Tab u;
    u.kind = 0;
    u.x = {0, 1 << 20};
    u.y = {INT32_MAX, INT32_MAX};
    u.dy = {INT32_MIN, INT32_MIN};
    diff(dut, M_CURVE, 1 << 19, u, "8b.negative rail");
    const Res low = run(dut, M_CURVE, 1 << 19, u);
    check(low.value == INT32_MIN, "8b.saturates low, not wrapped back",
          static_cast<uint32_t>(INT32_MIN), static_cast<uint32_t>(low.value));
  }

  // ---- 9. Interface laws --------------------------------------------------
  {
    const int32_t yv[4] = {0, kOne, -kOne, 2 * kOne};
    const Tab t = make_spline(4, 0, kOne, yv);
    const TabPort p{&t};

    // v_ready_o is low for the whole transaction, and the result is HELD until
    // r_ready_i takes it.
    dut.v_valid_i = 1;
    dut.mode_i = M_SPLINE;
    dut.a_i = static_cast<uint32_t>(kOne / 2);
    dut.tbl_n_i = 4;
    dut.r_ready_i = 0;
    dut.eval();
    int guard = 0;
    while (!dut.v_ready_o && guard++ < 256) tick_tbl(dut, p);
    tick_tbl(dut, p);
    dut.v_valid_i = 0;
    dut.eval();

    bool ready_stayed_low = true;
    int c = 0;
    while (!dut.r_valid_o && c < 256) {
      if (dut.v_ready_o) ready_stayed_low = false;
      tick_tbl(dut, p);
      ++c;
    }
    check(ready_stayed_low, "9.v_ready low during transaction", 1, ready_stayed_low ? 1 : 0);

    const int32_t held = static_cast<int32_t>(dut.result_o);
    bool stable = true;
    for (int i = 0; i < 12; ++i) {
      tick_tbl(dut, p);
      if (!dut.r_valid_o || static_cast<int32_t>(dut.result_o) != held) stable = false;
    }
    check(stable, "9.result held under backpressure", 1, stable ? 1 : 0);
    dut.r_ready_i = 1;
    dut.eval();
    tick_tbl(dut, p);
    check(!dut.r_valid_o, "9.result retires on ready", 0, dut.r_valid_o ? 1 : 0);

    // Back to back, and the second answer is not the first one leaking through.
    const Res r1 = run(dut, M_SPLINE, kOne / 2, t);
    const Res r2 = run(dut, M_SPLINE, 2 * kOne + kOne / 2, t);
    const InterpRes w1 = interp(M_SPLINE, kOne / 2, t);
    const InterpRes w2 = interp(M_SPLINE, 2 * kOne + kOne / 2, t);
    check(r1.value == w1.value, "9.back-to-back first", static_cast<uint32_t>(w1.value),
          static_cast<uint32_t>(r1.value));
    check(r2.value == w2.value, "9.back-to-back second", static_cast<uint32_t>(w2.value),
          static_cast<uint32_t>(r2.value));

    // Reset mid-transaction publishes nothing.
    dut.v_valid_i = 1;
    dut.mode_i = M_SPLINE;
    dut.a_i = static_cast<uint32_t>(kOne / 2);
    dut.eval();
    tick_tbl(dut, p);
    dut.v_valid_i = 0;
    dut.eval();
    tick_tbl(dut, p);
    tick_tbl(dut, p);
    dut.rst_n = 0;
    dut.eval();
    tick_tbl(dut, p);
    check(!dut.r_valid_o, "9.reset publishes nothing", 0, dut.r_valid_o ? 1 : 0);
    dut.rst_n = 1;
    dut.eval();
    tick_tbl(dut, p);
    const Res after = run(dut, M_SPLINE, kOne / 2, t);
    check(after.value == w1.value, "9.usable after reset", static_cast<uint32_t>(w1.value),
          static_cast<uint32_t>(after.value));
  }

  // ---- 10. Random differential -------------------------------------------
  if (random_iters > 0) {
    Prng rng(0xC0FFEEu);
    for (int i = 0; i < random_iters; ++i) {
      const uint8_t mode = static_cast<uint8_t>(rng.below(3));
      const Tab t = random_table(rng, mode == M_SPLINE);
      // Sample relative to the table, so most draws land INSIDE it rather than
      // clamping to an end -- a uniform s32 draw would hit the interior almost
      // never.
      int32_t a;
      switch (rng.below(8)) {
        case 0:
          a = INT32_MIN;
          break;
        case 1:
          a = INT32_MAX;
          break;
        case 2:
          a = t.x[0] - static_cast<int32_t>(rng.below(1u << 16));
          break;
        case 3:
          a = t.x[static_cast<size_t>(t.n() - 1)] + static_cast<int32_t>(rng.below(1u << 16));
          break;
        default: {
          const int j = static_cast<int>(rng.below(static_cast<uint32_t>(t.n())));
          const int32_t span = (t.n() > 1) ? (t.x[1] - t.x[0]) : 1;
          a = t.x[static_cast<size_t>(j)] +
              static_cast<int32_t>(rng.below(static_cast<uint32_t>(span))) -
              static_cast<int32_t>(rng.below(4));
          break;
        }
      }
      char nm[64];
      std::snprintf(nm, sizeof(nm), "10.random[%d]", i);
      diff(dut, mode, a, t, nm);
    }
    std::printf("random: %d iterations, %d odd v, %d negative v\n", random_iters, g_odd_v, g_neg_v);
  }

  return zhao::report_and_exit("field_curve_directed");
}
