// proj_arena3_directed.cpp — directed differential for zhao_proj_arena3.
//
// The refusal semantics are the point, exactly as in geom_wcache_directed: a
// hit is trivial, a WRONG refusal (or a wrong acceptance) is how one group's
// vertex silently becomes another's. Every reply is compared in FULL — valid,
// refuse, all five record fields, and the group key.
//
// COUNTER LAW (CLAUDE.md, "a detector reading zero is a claim"): every one of
// the module's nine counters is FIRED at least once by this suite, on a fault
// it should catch, before its silence is ever quoted as evidence. The
// elaboration guards, which lint cannot run, are exercised by the separate
// bad-parameter build this file's runner script performs (inverted polarity:
// that build PASSES when $fatal fires).
//
// Self-contained on purpose so it can be built without the cmake graph while
// a fit owns the tree; promoting it into tests/CMakeLists.txt is a ten-line
// block copied from geom_wcache_directed's.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Vzhao_proj_arena3.h"

namespace {

int g_failures = 0;

void check(bool cond, const char* what, uint64_t expected, uint64_t actual) {
  if (!cond) {
    std::printf("FAIL %s: expected %llx actual %llx\n", what,
                (unsigned long long)expected, (unsigned long long)actual);
    ++g_failures;
  }
}

constexpr int kGroups = 4;
constexpr int kDepth = 81;

struct Rec {
  uint32_t x, y, d, w;
  uint8_t behind;
};

Rec pattern(int g, int i) {
  // Distinct per group and index; masked to port widths (raw bit compare).
  Rec r;
  r.x = (uint32_t)((i * 1000 - 40000 + g * 7) & 0x1FFFFF);       // 21b
  r.y = (uint32_t)((i * -777 + 90000 + g * 13) & 0x1FFFFF);      // 21b
  r.d = (uint32_t)(0x10000u + i * 0x123u + g * 0x9999u);         // 32b
  r.w = (uint32_t)((0x2000000u + i * 0x777u + g) & 0x7FFFFFFF);  // 31b
  r.behind = (i % 17 == 3) ? 1 : 0;
  if (r.behind) { r.x = r.y = r.d = r.w = 0; }  // the core's behind law
  return r;
}

struct Dut {
  Vzhao_proj_arena3* v;
  explicit Dut(Vzhao_proj_arena3* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->open_i = 0;
    v->fill_valid_i = 0;
    v->seal_i = 0;
    v->rel_i = 0;
    v->ref_acq_i = 0;
    v->ref_rel_i = 0;
    v->rd0_valid_i = 0;
    v->rd1_valid_i = 0;
    v->rd2_valid_i = 0;
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  uint32_t open(int group, uint32_t key) {
    idle();
    v->open_i = 1;
    v->open_group_i = group;
    v->open_key_i = key;
    uint32_t gen = 0;
    v->eval();
    gen = v->open_gen_o;
    tick();
    idle();
    return gen;
  }

  bool fill(int group, const Rec& r) {
    idle();
    v->fill_valid_i = 1;
    v->fill_group_i = group;
    v->fill_x_i = r.x;
    v->fill_y_i = r.y;
    v->fill_d_i = r.d;
    v->fill_w_i = r.w;
    v->fill_behind_i = r.behind;
    v->eval();
    bool took = v->fill_ready_o;
    tick();
    idle();
    return took;
  }

  void seal(int group) {
    idle();
    v->seal_i = 1;
    v->seal_group_i = group;
    tick();
    idle();
  }

  void release(int group) {
    idle();
    v->rel_i = 1;
    v->rel_group_i = group;
    tick();
    idle();
  }

  void ref_acq(int group) {
    idle();
    v->ref_acq_i = 1;
    v->ref_acq_group_i = group;
    tick();
    idle();
  }

  void ref_rel(int group) {
    idle();
    v->ref_rel_i = 1;
    v->ref_rel_group_i = group;
    tick();
    idle();
  }

  // Issue on all three replica ports in one cycle, reply next cycle.
  void read3(int g0, uint32_t gen0, int i0, int g1, uint32_t gen1, int i1,
             int g2, uint32_t gen2, int i2) {
    idle();
    v->rd0_valid_i = 1; v->rd0_group_i = g0; v->rd0_gen_i = gen0; v->rd0_index_i = i0;
    v->rd1_valid_i = 1; v->rd1_group_i = g1; v->rd1_gen_i = gen1; v->rd1_index_i = i1;
    v->rd2_valid_i = 1; v->rd2_group_i = g2; v->rd2_gen_i = gen2; v->rd2_index_i = i2;
    tick();
    idle();
    v->eval();
  }
};

void expect_hit(Dut& d, int port, const Rec& r, uint32_t key) {
  uint32_t rv, rf, x, y, dd, w, b, k;
  switch (port) {
    case 0: rv = d.v->rd0_rep_valid_o; rf = d.v->rd0_refuse_o;
            x = d.v->rd0_x_o & 0x1FFFFF; y = d.v->rd0_y_o & 0x1FFFFF;
            dd = d.v->rd0_d_o; w = d.v->rd0_w_o; b = d.v->rd0_behind_o;
            k = d.v->rd0_key_o; break;
    case 1: rv = d.v->rd1_rep_valid_o; rf = d.v->rd1_refuse_o;
            x = d.v->rd1_x_o & 0x1FFFFF; y = d.v->rd1_y_o & 0x1FFFFF;
            dd = d.v->rd1_d_o; w = d.v->rd1_w_o; b = d.v->rd1_behind_o;
            k = d.v->rd1_key_o; break;
    default: rv = d.v->rd2_rep_valid_o; rf = d.v->rd2_refuse_o;
            x = d.v->rd2_x_o & 0x1FFFFF; y = d.v->rd2_y_o & 0x1FFFFF;
            dd = d.v->rd2_d_o; w = d.v->rd2_w_o; b = d.v->rd2_behind_o;
            k = d.v->rd2_key_o; break;
  }
  check(rv == 1, "hit.rep_valid", 1, rv);
  check(rf == 0, "hit.refuse", 0, rf);
  check(x == r.x, "hit.x", r.x, x);
  check(y == r.y, "hit.y", r.y, y);
  check(dd == r.d, "hit.d", r.d, dd);
  check(w == r.w, "hit.w", r.w, w);
  check(b == r.behind, "hit.behind", r.behind, b);
  check(k == key, "hit.key", key, k);
}

void expect_refuse(Dut& d, int port) {
  uint32_t rv, rf, x, y, dd, w, b, k;
  switch (port) {
    case 0: rv = d.v->rd0_rep_valid_o; rf = d.v->rd0_refuse_o;
            x = d.v->rd0_x_o; y = d.v->rd0_y_o; dd = d.v->rd0_d_o;
            w = d.v->rd0_w_o; b = d.v->rd0_behind_o; k = d.v->rd0_key_o; break;
    case 1: rv = d.v->rd1_rep_valid_o; rf = d.v->rd1_refuse_o;
            x = d.v->rd1_x_o; y = d.v->rd1_y_o; dd = d.v->rd1_d_o;
            w = d.v->rd1_w_o; b = d.v->rd1_behind_o; k = d.v->rd1_key_o; break;
    default: rv = d.v->rd2_rep_valid_o; rf = d.v->rd2_refuse_o;
            x = d.v->rd2_x_o; y = d.v->rd2_y_o; dd = d.v->rd2_d_o;
            w = d.v->rd2_w_o; b = d.v->rd2_behind_o; k = d.v->rd2_key_o; break;
  }
  check(rv == 1, "refuse.rep_valid", 1, rv);
  check(rf == 1, "refuse.refuse", 1, rf);
  // A refused reply is DETERMINISTICALLY zero — never another group's vertex.
  check(x == 0 && y == 0 && dd == 0 && w == 0 && b == 0 && k == 0,
        "refuse.zeroed", 0, (uint64_t)x | y | dd | w | b | k);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_proj_arena3;
  Dut d(top);
  d.reset();

  const uint32_t kKey0 = 0xA11CE5u;
  const uint32_t kKey0b = 0xB0B0E5u;
  const uint32_t kKey1 = 0x7E44A1u;  // terrain-ish tag

  // ---- lifetime: open, read-before-seal refused, dense fill ---------------
  uint32_t gen0 = d.open(0, kKey0);
  check(gen0 == 1, "open.gen0", 1, gen0);

  // Counter: open_illegal (open of a FILLING group).
  d.open(0, kKey0);
  check(top->open_illegal_o == 1, "ctr.open_illegal", 1, top->open_illegal_o);

  // Read while FILLING: refused on all three ports.
  d.read3(0, gen0, 5, 0, gen0, 6, 0, gen0, 7);
  expect_refuse(d, 0);
  expect_refuse(d, 1);
  expect_refuse(d, 2);
  check(top->read_refusals_o == 3, "ctr.read_refusals.filling", 3,
        top->read_refusals_o);

  // Fill 40 rows, then a SHORT seal: refused, still FILLING.
  for (int i = 0; i < 40; ++i)
    check(d.fill(0, pattern(0, i)), "fill.took", 1, 0);
  d.seal(0);
  check(top->seal_short_o == 1, "ctr.seal_short", 1, top->seal_short_o);
  check(top->seals_o == 0, "ctr.seals.after_short", 0, top->seals_o);

  // Finish the dense fill; then one EXTRA fill must be refused (ready low).
  for (int i = 40; i < kDepth; ++i)
    check(d.fill(0, pattern(0, i)), "fill.took2", 1, 0);
  check(!d.fill(0, pattern(0, 99)), "fill.overfull_refused", 0, 1);
  check(top->fill_illegal_o == 1, "ctr.fill_illegal", 1, top->fill_illegal_o);
  check(top->fills_o == kDepth, "ctr.fills", kDepth, top->fills_o);

  d.seal(0);
  check(top->seals_o == 1, "ctr.seals", 1, top->seals_o);

  // ---- replay: all 81 rows through all three replicas simultaneously ------
  for (int i = 0; i < kDepth; ++i) {
    int i1 = (i + 27) % kDepth;
    int i2 = (i + 54) % kDepth;
    d.read3(0, gen0, i, 0, gen0, i1, 0, gen0, i2);
    expect_hit(d, 0, pattern(0, i), kKey0);
    expect_hit(d, 1, pattern(0, i1), kKey0);
    expect_hit(d, 2, pattern(0, i2), kKey0);
  }

  // ---- refusal taxonomy, one per cause, counted exactly -------------------
  uint32_t refusals_before = top->read_refusals_o;
  d.read3(0, gen0 + 1, 0,   // stale generation
          0, gen0, kDepth,  // index == DEPTH (first illegal row)
          5, gen0, 0);      // group out of range
  expect_refuse(d, 0);
  expect_refuse(d, 1);
  expect_refuse(d, 2);
  d.read3(1, 0, 0, 0, gen0, 0, 0, gen0, 1);  // port0: IDLE group refused
  expect_refuse(d, 0);
  expect_hit(d, 1, pattern(0, 0), kKey0);
  expect_hit(d, 2, pattern(0, 1), kKey0);
  check(top->read_refusals_o == refusals_before + 4, "ctr.read_refusals.mix",
        refusals_before + 4, top->read_refusals_o);

  // ---- references gate release --------------------------------------------
  d.ref_acq(0);
  d.ref_acq(0);
  d.release(0);
  check(top->release_blocked_o == 1, "ctr.release_blocked", 1,
        top->release_blocked_o);
  d.ref_rel(0);
  d.ref_rel(0);
  d.ref_rel(0);  // underflow attempt
  check(top->ref_err_o == 1, "ctr.ref_err.underflow", 1, top->ref_err_o);
  d.ref_acq(1);  // acquire on an IDLE group
  check(top->ref_err_o == 2, "ctr.ref_err.idle_acq", 2, top->ref_err_o);
  d.release(0);
  check(top->releases_o == 1, "ctr.releases", 1, top->releases_o);

  // ---- reopen: stale handles from the previous lifetime are refused -------
  uint32_t gen0b = d.open(0, kKey0b);
  check(gen0b == 2, "reopen.gen", 2, gen0b);
  for (int i = 0; i < kDepth; ++i) d.fill(0, pattern(2, i));
  d.seal(0);
  d.read3(0, gen0, 3, 0, gen0b, 3, 0, gen0b, 4);  // port0 replays OLD handle
  expect_refuse(d, 0);
  expect_hit(d, 1, pattern(2, 3), kKey0b);
  expect_hit(d, 2, pattern(2, 4), kKey0b);

  // ---- overlap: fill group 1 while replaying group 0 ----------------------
  // This is the shipping schedule (fill next subpatch during replay of the
  // sealed one) and the ports must not interfere.
  uint32_t gen1 = d.open(1, kKey1);
  for (int i = 0; i < kDepth; ++i) {
    // one fill and three reads in the SAME cycle
    d.v->fill_valid_i = 1;
    d.v->fill_group_i = 1;
    Rec r = pattern(1, i);
    d.v->fill_x_i = r.x; d.v->fill_y_i = r.y; d.v->fill_d_i = r.d;
    d.v->fill_w_i = r.w; d.v->fill_behind_i = r.behind;
    d.v->rd0_valid_i = 1; d.v->rd0_group_i = 0; d.v->rd0_gen_i = gen0b;
    d.v->rd0_index_i = i;
    d.v->rd1_valid_i = 1; d.v->rd1_group_i = 0; d.v->rd1_gen_i = gen0b;
    d.v->rd1_index_i = (i + 1) % kDepth;
    d.v->rd2_valid_i = 1; d.v->rd2_group_i = 0; d.v->rd2_gen_i = gen0b;
    d.v->rd2_index_i = (i + 2) % kDepth;
    d.v->eval();
    check(d.v->fill_ready_o == 1, "overlap.fill_ready", 1, d.v->fill_ready_o);
    d.tick();
    d.idle();
    d.v->eval();
    expect_hit(d, 0, pattern(2, i), kKey0b);
    expect_hit(d, 1, pattern(2, (i + 1) % kDepth), kKey0b);
    expect_hit(d, 2, pattern(2, (i + 2) % kDepth), kKey0b);
  }
  d.seal(1);
  d.read3(1, gen1, 80, 1, gen1, 0, 1, gen1, 33);
  expect_hit(d, 0, pattern(1, 80), kKey1);
  expect_hit(d, 1, pattern(1, 0), kKey1);
  expect_hit(d, 2, pattern(1, 33), kKey1);

  // ---- final counter cross-check ------------------------------------------
  check(top->fills_o == 3 * kDepth, "final.fills", 3 * kDepth, top->fills_o);
  check(top->seals_o == 3, "final.seals", 3, top->seals_o);

  std::printf(g_failures ? "proj_arena3_directed: %d FAILURES\n"
                         : "proj_arena3_directed: ALL CHECKS PASSED\n",
              g_failures);
  top->final();
  delete top;
  std::fflush(nullptr);
  std::_Exit(g_failures ? 1 : 0);  // the harness's exit_hard law: winpthread
                                   // teardown deadlocks; exit deterministically
}
