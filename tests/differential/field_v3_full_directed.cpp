// field_v3_full_directed.cpp — the Field v3 engine and its long-op service
// path AS ONE MACHINE, against the shipped interpreter.
//
// WHAT THIS ADDS THAT THE PIECES CANNOT
// -------------------------------------
// The engine is closed at 42/42 and the service path at 25/25, and every
// defect this project has paid real time for lived between two such blocks and
// not inside either:
//
//     the executor's open-loop DOT     no mul_ready port to refuse it
//     the curve service's hang         no mul_ready port at all
//     the dispatcher's missing imm     no port to carry it
//     the register file's moving read  needed FOUR contexts to appear
//
// Writing this file found a fifth before it ran a single clock: the executor
// routes SPLINE and RING to the service path and the dispatcher refuses both,
// so a program using either parks that context forever. That is in
// zhao_probe_v3_full.sv's header and it is NOT fixed here, because the fix
// follows a decision that is Fabian's.
//
// WHAT THE COMPOSITION ACTUALLY SUPPORTS TODAY, stated rather than implied:
// the service path has ONE service, the noise unit, so of the ten opcodes the
// executor routes long, exactly TWO are served end to end -- NOISE2 and RIDGE.
// Six more are accepted by the dispatcher and would reach a service that does
// not implement them; `wrong_op_o` is the wire that says so, and section 5
// proves it fires rather than trusting it to.
//
// So: sections 1-4 drive the two that work. Section 5 drives one that does not
// and requires the alarm. Nothing here quietly avoids the gap.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_v3_full.h"

#include "zfield/zfield.hpp"
#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

constexpr int kCtx = 8;
constexpr int kRegs = 32;

using zhao::check;

struct Prng {
  uint32_t s;
  explicit Prng(uint32_t seed) : s(seed ? seed : 1u) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  uint32_t below(uint32_t n) { return next() % n; }
  int32_t interesting() {
    switch (below(6)) {
      case 0:
        return 0;
      case 1:
        return 1 << 16;
      case 2:
        return -(1 << 16);
      case 3:
        return (int32_t)0x7FFFFFFF;
      case 4:
        return (int32_t)0x80000000;
      default:
        return (int32_t)next();
    }
  }
};

/** The shipped interpreter, for one point of one long op. */
void oracle(uint8_t op, uint32_t imm, const int32_t* src, int32_t* dst) {
  const std::vector<zfield::Table> no_tables;
  zref::SatLedger L;
  zfield::steps::exec_op(op, imm, no_tables, src, dst, &L);
}

struct Dut {
  Vzhao_probe_v3_full& t;
  int32_t shadow[kCtx][kRegs] = {};
  int writes = 0;

  explicit Dut(Vzhao_probe_v3_full& top) : t(top) {}

  void reset(int policy) {
    t.rst_n = 0;
    t.up_we_i = 0;
    t.pre_we_i = 0;
    t.start_i = 0;
    t.rival_req_i = 0;
    t.wb_policy_i = (uint8_t)policy;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    std::memset(shadow, 0, sizeof shadow);
    writes = 0;
  }

  void load_uop(int ctx, int pc, uint8_t op, int dst, int a, int b, int c, uint32_t imm) {
    t.up_we_i = 1;
    t.up_ctx_i = (uint8_t)ctx;
    t.up_pc_i = (uint8_t)pc;
    t.up_op_i = op;
    t.up_dst_i = (uint8_t)dst;
    t.up_a_i = (uint8_t)a;
    t.up_b_i = (uint8_t)b;
    t.up_c_i = (uint8_t)c;
    t.up_imm_i = imm;
    t.eval();
    zhao::tick(t);
    t.up_we_i = 0;
    t.eval();
  }

  void preload(int ctx, int reg, int32_t v) {
    t.pre_we_i = 1;
    t.pre_ctx_i = (uint8_t)ctx;
    t.pre_reg_i = (uint8_t)reg;
    t.pre_data_i = (uint32_t)v;
    t.eval();
    zhao::tick(t);
    t.pre_we_i = 0;
    t.eval();
  }

  void start(int ctx) {
    t.start_i = 1;
    t.start_ctx_i = (uint8_t)ctx;
    step(nullptr);
    t.start_i = 0;
  }

  /** One clock. THE WRITE PORT IS WATCHED, NOT RECONSTRUCTED: `wr_*_o` is the
      arbiter's output, which is the register file's write as it actually
      happens -- the ALU's and the drain's alike. */
  bool step(int* done_ctx) {
    t.eval();
    const bool w = t.wr_en_o != 0;
    const int wc = (int)t.wr_ctx_o, wr = (int)t.wr_reg_o;
    const int32_t wd = (int32_t)t.wr_data_o;
    const bool dn = t.done_valid_o != 0;
    if (done_ctx && dn) *done_ctx = (int)t.done_ctx_o;
    zhao::tick(t);
    if (w) {
      shadow[wc][wr] = wd;
      ++writes;
    }
    return dn;
  }

  /** Every alarm either block owns, unmerged. */
  int alarms() const {
    return (t.exec_desync_o ? 1 : 0) + (t.bank_desync_o ? 2 : 0) + (t.svc_bank_desync_o ? 4 : 0) +
           (t.tag_mismatch_o ? 8 : 0) + (t.wrong_op_o ? 16 : 0) + (t.sk_overflow_o ? 32 : 0) +
           (t.unsupported_o ? 64 : 0);
  }
};

/** One long op per context: preload x,y then run `op` into r2 (and r3). */
int run_long(Vzhao_probe_v3_full& top, uint8_t op, int n_ctx, const int32_t* xs, const int32_t* ys,
             uint32_t seed, int policy, bool rival, int32_t out[][2], int* clocks,
             bool reverse_start = false) {
  Dut d(top);
  d.reset(policy);
  for (int c = 0; c < n_ctx; ++c) {
    d.preload(c, 0, xs[c]);
    d.preload(c, 1, ys[c]);
    d.load_uop(c, 0, op, 2, 0, 1, 0, seed);
    d.load_uop(c, 1, zfield::OP_END, 0, 0, 0, 0, 0);
  }
  if (reverse_start) {
    for (int c = n_ctx - 1; c >= 0; --c) d.start(c);
  } else {
    for (int c = 0; c < n_ctx; ++c) d.start(c);
  }

  Prng rv(0xC0FFEEu + seed);
  int fin = 0, guard = 0, done = -1;
  while (guard++ < 400000 && fin < n_ctx) {
    top.rival_req_i = (rival && (rv.below(2) != 0)) ? 1 : 0;
    if (d.step(&done)) ++fin;
  }
  top.rival_req_i = 0;
  for (int i = 0; i < 32; ++i) d.step(&done);

  for (int c = 0; c < n_ctx; ++c) {
    out[c][0] = d.shadow[c][2];
    out[c][1] = d.shadow[c][3];
  }
  if (clocks) *clocks = guard;
  return (fin == n_ctx) ? d.alarms() : -1;
}

void check_group(Vzhao_probe_v3_full& top, uint8_t op, int n_ctx, uint32_t seed, int policy,
                 bool rival, Prng& rng, const std::string& what) {
  int32_t xs[kCtx], ys[kCtx], got[kCtx][2];
  for (int c = 0; c < n_ctx; ++c) {
    xs[c] = rng.interesting();
    ys[c] = rng.interesting();
  }
  int clocks = 0;
  const int al = run_long(top, op, n_ctx, xs, ys, seed, policy, rival, got, &clocks);

  check(al >= 0, (what + ": every context finished").c_str(), 1, al >= 0 ? 1 : 0);
  if (al < 0) return;
  check(al == 0, (what + ": and no block raised an alarm").c_str(), 0, al);

  const int width = (op == zfield::OP_RIDGE) ? 1 : 2;
  if (n_ctx == kCtx && op == zfield::OP_NOISE2) {
    for (int c = 0; c < n_ctx; ++c) {
      int32_t s2[4] = {xs[c], ys[c], 0, 0}, w2[4] = {};
      oracle(op, seed, s2, w2);
      printf("      ctx%d x=%08X y=%08X want %08X/%08X got %08X/%08X%s\n", c, (uint32_t)xs[c],
             (uint32_t)ys[c], (uint32_t)w2[0], (uint32_t)w2[1], (uint32_t)got[c][0],
             (uint32_t)got[c][1], (got[c][0] == w2[0] && got[c][1] == w2[1]) ? "" : "   <-- WRONG");
    }
  }
  for (int c = 0; c < n_ctx; ++c) {
    int32_t src[4] = {xs[c], ys[c], 0, 0};
    int32_t want[4] = {};
    oracle(op, seed, src, want);
    for (int w = 0; w < width; ++w) {
      check(got[c][w] == want[w],
            (what + ": ctx " + std::to_string(c) + " r" + std::to_string(2 + w)).c_str(),
            (uint32_t)want[w], (uint32_t)got[c][w]);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_probe_v3_full top;
  Prng rng(0xF0117Eu);

  printf("== section 1: ONE long op, all the way round the machine ==\n");
  // The whole loop in one instruction: the executor parks the context, the
  // dispatcher gathers it, the noise unit computes it on the shared bank, the
  // arbiter wins the write port, the register file takes it, and the context
  // is released. Every one of those was proven alone; none was proven to hand
  // over until this ran.
  check_group(top, zfield::OP_NOISE2, 1, 0xBEEFu, 1, false, rng, "NOISE2 x1");
  check_group(top, zfield::OP_RIDGE, 1, 0xBEEFu, 1, false, rng, "RIDGE x1");

  printf("== section 2: a FULL GROUP -- four contexts gathered into one request ==\n");
  // Four points is what the dispatcher is for. One context exercises the
  // partial-group flush; four exercise the shape the fabric was built around.
  check_group(top, zfield::OP_NOISE2, 4, 0x1234u, 1, false, rng, "NOISE2 x4");
  check_group(top, zfield::OP_RIDGE, 4, 0x1234u, 1, false, rng, "RIDGE x4");

  printf("== section 3: EVERY GROUP SIZE, because 8 is the only one that fails ==\n");
  // Two FULL groups is the case that breaks. Four is one group; five to seven
  // are one full group plus a partial, and all are clean. Only eight -- two
  // complete groups back to back -- mis-assigns, and it mis-assigns exactly
  // one point. The rival makes no difference, which rules contention out.
  //
  // KNOWN FAILING. ctx6 receives ctx7’s answer while ctx7 is also correct, so
  // the group carried ctx6’s IDENTITY in a slot holding ctx7’s OPERANDS --
  // the lane computed the wrong point. ctx6 is still released, so it is not a
  // lost or duplicated context.
  //
  // Left failing on purpose. Deleting the case that found it would make the
  // suite green and the machine no better.
  {
    const int ns[5] = {4, 5, 6, 7, 8};
    for (int ri = 0; ri < 2; ++ri) {
      for (int i = 0; i < 5; ++i) {
        int32_t xs[kCtx], ys[kCtx], got[kCtx][2];
        for (int c = 0; c < kCtx; ++c) {
          xs[c] = (int32_t)((c + 1) << 16);
          ys[c] = (int32_t)((c + 2) << 16);
        }
        int clocks = 0;
        const int al =
            run_long(top, zfield::OP_NOISE2, ns[i], xs, ys, 0x11u, 1, ri != 0, got, &clocks);
        int bad = 0;
        for (int c = 0; c < ns[i]; ++c) {
          int32_t s2[4] = {xs[c], ys[c], 0, 0}, w2[4] = {};
          oracle(zfield::OP_NOISE2, 0x11u, s2, w2);
          if (got[c][0] != w2[0] || got[c][1] != w2[1]) {
            ++bad;
            int src_of = -1;
            for (int o = 0; o < ns[i]; ++o) {
              int32_t s3[4] = {xs[o], ys[o], 0, 0}, w3[4] = {};
              oracle(zfield::OP_NOISE2, 0x11u, s3, w3);
              if (got[c][0] == w3[0] && got[c][1] == w3[1]) src_of = o;
            }
            printf("      ctx%d holds ctx%d\'s answer\n", c, src_of);
          }
        }
        check(bad == 0,
              (std::string("n=") + std::to_string(ns[i]) + " rival=" + std::to_string(ri) +
               ": every context got its own answer")
                  .c_str(),
              0, bad);
        printf("   n=%d rival=%d -> %s in %5d clocks, %d wrong\n", ns[i], ri,
               al >= 0 ? "done" : "HUNG", clocks, bad);
      }
    }
  }

  printf("== section 3: the WHOLE BARREL, and the bank contended ==\n");
  // Eight contexts with the rival pressing the engine's bank. Three defects
  // this run needed more than one context to appear and one needed all eight,
  // so the composed test starts where the block tests ended.
  check_group(top, zfield::OP_NOISE2, kCtx, 0x5A5Au, 1, false, rng, "NOISE2 x8 quiet");
  check_group(top, zfield::OP_NOISE2, kCtx, 0x5A5Au, 1, true, rng, "NOISE2 x8 contended");
  check_group(top, zfield::OP_RIDGE, kCtx, 0x5A5Au, 1, true, rng, "RIDGE x8 contended");

  {
    // WHICH DOES THE FAULT FOLLOW -- the context number, or the slot it lands
    // in? Starting the contexts in reverse changes which context reaches the
    // dispatcher first without changing anything else.
    int32_t xs[kCtx], ys[kCtx], got[kCtx][2];
    for (int c = 0; c < kCtx; ++c) {
      xs[c] = (int32_t)((c + 1) << 16);
      ys[c] = (int32_t)((c + 2) << 16);
    }
    int clocks = 0;
    (void)run_long(top, zfield::OP_NOISE2, kCtx, xs, ys, 0x11u, 1, false, got, &clocks, true);
    for (int c = 0; c < kCtx; ++c) {
      int32_t s2[4] = {xs[c], ys[c], 0, 0}, w2[4] = {};
      oracle(zfield::OP_NOISE2, 0x11u, s2, w2);
      if (got[c][0] != w2[0] || got[c][1] != w2[1]) {
        int src_of = -1;
        for (int o = 0; o < kCtx; ++o) {
          int32_t s3[4] = {xs[o], ys[o], 0, 0}, w3[4] = {};
          oracle(zfield::OP_NOISE2, 0x11u, s3, w3);
          if (got[c][0] == w3[0] && got[c][1] == w3[1]) src_of = o;
        }
        printf("   REVERSE START: ctx%d holds ctx%d answer\n", c, src_of);
      }
    }
  }

  {
    int32_t xs[kCtx], ys[kCtx], got[kCtx][2];
    for (int c = 0; c < kCtx; ++c) {
      xs[c] = (int32_t)((c + 1) << 16);
      ys[c] = (int32_t)((c + 2) << 16);
    }
    int clocks = 0;
    (void)run_long(top, zfield::OP_NOISE2, kCtx, xs, ys, 0x11u, 1, false, got, &clocks);
    printf("   n=8 groups=%u partial=%u drain_writes=%u served[0]=%u served[1]=%u\n",
           (unsigned)top.groups_o, (unsigned)top.partial_o, (unsigned)top.drain_writes_o,
           (unsigned)top.wb_served_o[0], (unsigned)top.wb_served_o[1]);
  }

  printf("== section 4: THE POLICY, measured on the composed machine ==\n");
  {
    // wbarb's policy was measured on the service path alone: ALU-first starves
    // the drain outright. Here the ALU is a real executor rather than a
    // synthetic stream, so the question is whether that still holds when the
    // starved claimant is the thing producing the work.
    const char* names[3] = {"ALU first", "drain first", "round robin"};
    for (int pol = 0; pol < 3; ++pol) {
      int32_t xs[kCtx], ys[kCtx], got[kCtx][2];
      for (int c = 0; c < kCtx; ++c) {
        xs[c] = (int32_t)((c + 1) << 16);
        ys[c] = (int32_t)((c + 3) << 16);
      }
      int clocks = 0;
      const int al = run_long(top, zfield::OP_NOISE2, 4, xs, ys, 0x77u, pol, false, got, &clocks);
      printf("   %-12s finished %s in %5d clocks\n", names[pol], al >= 0 ? "yes" : "NO ", clocks);
      if (pol == 0) {
        // MEASURED, NOT ASSUMED. On the service path alone ALU-first starved
        // the drain forever. Whatever this does, it is recorded rather than
        // predicted -- and it is not the policy the engine uses.
        continue;
      }
      check(al >= 0, (std::string(names[pol]) + ": the group completed").c_str(), 1,
            al >= 0 ? 1 : 0);
      if (al >= 0) {
        check(al == 0, (std::string(names[pol]) + ": with no alarm").c_str(), 0, al);
      }
    }
  }

  printf("== section 5: an op the service does NOT implement must RAISE, not answer ==\n");
  {
    // ROT2 is dispatchable -- `dst_width_of` knows it -- and the service path
    // has only the noise unit, so it arrives somewhere that cannot compute it.
    //
    // THE POINT IS THAT THIS IS LOUD. A composition that answered it quietly
    // would be the worst outcome available: a plausible wrong number in a real
    // register. `wrong_op_o` exists for exactly this and is asserted here
    // rather than trusted.
    int32_t xs[1] = {3 << 16}, ys[1] = {5 << 16}, got[1][2];
    int clocks = 0;
    (void)run_long(top, zfield::OP_ROT2, 1, xs, ys, 0u, 1, false, got, &clocks);
    check(top.wrong_op_o == 1, "the service reported an op it does not implement", 1,
          (uint32_t)top.wrong_op_o);
    printf("   MEASURED: wrong_op_o = %u after a ROT2 (dispatchable, unserved)\n",
           (unsigned)top.wrong_op_o);
  }

  return zhao::report_and_exit("field_v3_full_directed");
}
