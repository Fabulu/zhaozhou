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
    t.tl_we_i = 0;
    t.tl_commit_i = 0;
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

  // The knot table the curve service reads. It belongs to the PROGRAM, so
  // the harness supplies it exactly as the command stream will -- there is no
  // table inside the silicon to fall back on.
  void load_table(int slot, const zfield::Table& tb) {
    const int n = (int)tb.x.size();
    for (int i = 0; i < n; ++i) {
      t.tl_we_i = 1;
      t.tl_tbl_i = (uint8_t)slot;
      t.tl_idx_i = (uint8_t)i;
      t.tl_x_i = (uint32_t)tb.x[(size_t)i];
      t.tl_y_i = (uint32_t)tb.y[(size_t)i];
      t.tl_dy_i = (uint32_t)tb.dy[(size_t)i];
      t.eval();
      zhao::tick(t);
    }
    t.tl_we_i = 0;
    t.tl_commit_i = 1;
    t.tl_tbl_i = (uint8_t)slot;
    t.tl_n_i = (uint8_t)n;
    t.eval();
    zhao::tick(t);
    t.tl_commit_i = 0;
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
             bool reverse_start = false, const zfield::Table* tab = nullptr) {
  Dut d(top);
  d.reset(policy);
  if (tab) d.load_table(0, *tab);
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

// BOTH SERVICES AT ONCE. Every other runner in this file gives all contexts
// the SAME op, so exactly one service is ever busy and the path between them
// is never contended. That is not a small omission: it is the arrangement the
// second service was added to create, and five deliberate defects survived the
// 37-mutant sweep purely because nothing here produced it.
//
// Each context gets its own op and its own immediate, and they are all started
// before any finishes.
int run_mixed(Vzhao_probe_v3_full& top, const uint8_t* ops, const uint32_t* imms, int n_ctx,
              const int32_t* xs, const int32_t* ys, int32_t out[][2], int* clocks,
              const zfield::Table* t0, const zfield::Table* t2) {
  Dut d(top);
  d.reset(0);
  if (t0) d.load_table(0, *t0);
  if (t2) d.load_table(2, *t2);
  for (int c = 0; c < n_ctx; ++c) {
    d.preload(c, 0, xs[c]);
    d.preload(c, 1, ys[c]);
    d.load_uop(c, 0, ops[c], 2, 0, 1, 0, imms[c]);
    d.load_uop(c, 1, zfield::OP_END, 0, 0, 0, 0, 0);
  }
  for (int c = 0; c < n_ctx; ++c) d.start(c);

  int fin = 0, guard = 0, done = -1;
  while (guard++ < 400000 && fin < n_ctx) {
    if (d.step(&done)) ++fin;
  }
  for (int i = 0; i < 32; ++i) d.step(&done);
  for (int c = 0; c < n_ctx; ++c) {
    out[c][0] = d.shadow[c][2];
    out[c][1] = d.shadow[c][3];
  }
  if (clocks) *clocks = guard;
  return (fin == n_ctx) ? d.alarms() : -1;
}

// One context, one long op, an explicit register layout, and up to three
// result registers. The other runners hardwire a = reg0 and b = reg1 with the
// destination at reg2, which collides with the SOURCES of a three-member
// operand -- so NORMALIZE3 and ROT3 cannot be expressed in them at all. That
// is not a small gap: it is why nothing in this file had ever run the two
// widest ops through the whole machine.
int run_one(Vzhao_probe_v3_full& top, uint8_t op, uint32_t imm, const int32_t src[4],
            int32_t out[3], int* clocks, const zfield::Table* tab) {
  Dut d(top);
  d.reset(0);
  if (tab) d.load_table(0, *tab);
  for (int r = 0; r < 4; ++r) d.preload(0, r, src[r]);
  // a = reg0 (the group start, 1..3 members) and b = reg3 (the single-member
  // operand: ROT's angle). The destination is well clear of both.
  d.load_uop(0, 0, op, 8, 0, 3, 0, imm);
  d.load_uop(0, 1, zfield::OP_END, 0, 0, 0, 0, 0);
  d.start(0);
  int fin = 0, guard = 0, done = -1;
  while (guard++ < 100000 && fin < 1) {
    if (d.step(&done)) ++fin;
  }
  for (int i = 0; i < 32; ++i) d.step(&done);
  for (int m = 0; m < 3; ++m) out[m] = d.shadow[0][8 + m];
  if (clocks) *clocks = guard;
  return (fin == 1) ? d.alarms() : -1;
}

void check_group(Vzhao_probe_v3_full& top, uint8_t op, int n_ctx, uint32_t seed, int policy,
                 bool rival, Prng& rng, const std::string& what) {
  // ZERO-INITIALISED, NOT MERELY DECLARED. Only n_ctx of kCtx entries are
  // filled, and all kCtx are passed down. A differential whose inputs are
  // stack garbage has a verdict that depends on the stack -- it would pass
  // or fail on what the previous call happened to leave behind.
  int32_t xs[kCtx] = {}, ys[kCtx] = {}, got[kCtx][2] = {};
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
        int32_t xs[kCtx] = {}, ys[kCtx] = {}, got[kCtx][2] = {};
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
    int32_t xs[kCtx] = {}, ys[kCtx] = {}, got[kCtx][2] = {};
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
    int32_t xs[kCtx] = {}, ys[kCtx] = {}, got[kCtx][2] = {};
    for (int c = 0; c < kCtx; ++c) {
      xs[c] = (int32_t)((c + 1) << 16);
      ys[c] = (int32_t)((c + 2) << 16);
    }
    int clocks = 0;
    (void)run_long(top, zfield::OP_NOISE2, kCtx, xs, ys, 0x11u, 1, false, got, &clocks);
    // BATCHING IS ASSERTED, NOT MERELY PRINTED. These five numbers were
    // measured and shown for eight releases and never checked, so mutant D30
    // -- which closes a group on a mismatch nobody is offering, collapsing
    // every batch to a single point -- survived a 30-mutant sweep with the
    // count sitting in the output the whole time.
    //
    // Eight points of ONE op is exactly two full groups. If the dispatcher
    // ever closes a group early, this is 8 and partial is non-zero, which is
    // the entire reason the block batches at all.
    check(top.groups_o == 2u, "eight same-op points batch into TWO full groups", 2,
          (int)top.groups_o);
    check(top.partial_o == 0u, "and neither of them is partial", 0, (int)top.partial_o);
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
      int32_t xs[kCtx] = {}, ys[kCtx] = {}, got[kCtx][2] = {};
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

  printf("== section 5: EVERY op the table offers is actually served ==\n");
  {
    // THIS SECTION USED TO ASSERT THE OPPOSITE and it was right to. Until
    // 2026-08-29 the table gave NORMALIZE2/3 and ROT2/3 a destination width --
    // which is what makes the executor OFFER them -- while the service path
    // had nothing that could compute them. They fell into the noise unit's
    // else-branch, were answered with NOISE semantics, and the wrong number
    // was written to a real register with only `wrong_op_o` to say so.
    //
    // A raised flag beside a wrong value is not a safe failure. It is the
    // worst outcome available: individually plausible, completely wrong, and
    // discoverable only by someone who thought to read the flag.
    //
    // So the check inverts. The table and the service path are two lists that
    // must agree -- the fifth instance of that shape in this engine -- and the
    // way to hold them together is to run EVERY op the table offers and demand
    // the machine answer it correctly. If either list gains an entry the other
    // lacks, this fails.
    struct OpCase {
      uint8_t op;
      const char* name;
      int a_members;  // members of operand a, from zfield_decode's shape
      bool has_b;     // ROT's single-member angle operand
      int width;      // destination registers, from field_long_width
      uint32_t imm;
    };
    // Read from zfield_decode.cpp, not inferred: the shapes are
    // {dst, {a, b, c}, groups, imm_class}.
    const OpCase cases[] = {
        {zfield::OP_CURVE, "CURVE", 1, false, 1, 0u},
        {zfield::OP_DCURVE, "DCURVE", 1, false, 1, 0u},
        {zfield::OP_SPLINE, "SPLINE", 1, false, 1, 0u},
        {zfield::OP_RIDGE, "RIDGE", 2, false, 1, 0x31u},
        {zfield::OP_NOISE2, "NOISE2", 2, false, 2, 0x5Bu},
        {zfield::OP_NORMALIZE2, "NORMALIZE2", 2, false, 2, 0u},
        {zfield::OP_ROT2, "ROT2", 2, true, 2, 0u},
        {zfield::OP_NORMALIZE3, "NORMALIZE3", 3, false, 3, 0u},
        {zfield::OP_ROT3, "ROT3", 3, true, 3, 1u},
    };

    zfield::Table tb;
    tb.kind = 0;
    tb.x = {-(1 << 20), 0, 1 << 20, 3 << 20};
    tb.y = {7 << 16, 11 << 16, -(5 << 16), 2 << 16};
    tb.dy = {1 << 12, 1 << 12, 1 << 12, 1 << 12};
    std::vector<zfield::Table> tabs{tb};

    // reg3 is ROT's angle, so it is a real one rather than zero -- an angle of
    // zero makes cos 1 and sin 0, and a rotation that does nothing would pass
    // with the sine term wired to anything at all.
    const int32_t src[4] = {(3 << 16) + 1234, -(2 << 16) + 77, (1 << 16) - 991,
                            (int32_t)0x00003A2Bu};

    for (const OpCase& c : cases) {
      int32_t out[3] = {};
      int clocks = 0;
      const int al = run_one(top, c.op, c.imm, src, out, &clocks, &tb);

      // The oracle's src[] is the FLATTENED operand list: a's members first,
      // then b's. That is why ROT2 finds its angle at src[2] and ROT3 at
      // src[3] -- the same register, a different index.
      int32_t osrc[4] = {};
      int n = 0;
      for (int m = 0; m < c.a_members; ++m) osrc[n++] = src[m];
      if (c.has_b) osrc[n++] = src[3];

      zref::SatLedger L{};
      int32_t odst[3] = {};
      zfield::steps::exec_op(c.op, c.imm, tabs, osrc, odst, &L);

      char what[96];
      snprintf(what, sizeof what, "%s is served with no alarm", c.name);
      check(al == 0, what, 0, al);
      snprintf(what, sizeof what, "%s does not raise wrong_op_o", c.name);
      check(top.wrong_op_o == 0, what, 0, (uint32_t)top.wrong_op_o);
      for (int m = 0; m < c.width; ++m) {
        snprintf(what, sizeof what, "%s result register %d matches the oracle", c.name, m);
        check(out[m] == odst[m], what, (uint32_t)odst[m], (uint32_t)out[m]);
      }
      printf("   MEASURED: %-10s width %d in %5d clocks\n", c.name, c.width, clocks);
    }
  }

  printf("== section 6: an op the TABLE does not know is refused, not handed over ==\n");
  {
    // zhao_field_ops_pkg is the single answer to "does this op leave the pipe".
    //
    // SPLINE HELD THIS ROLE UNTIL 2026-08-29 and no longer can: it is in the
    // table now, it is served by the curve service, and section 6b checks that
    // it is. OP_RING (0x21) takes over, and it is not a stand-in picked for
    // convenience -- the brief leaves the varying-radius ring on the COLD
    // lane, so it is genuinely an op this table does not know.
    //
    // If the executor ever regains a private opinion and offers an op the
    // dispatcher will not take, that context parks forever -- which is exactly
    // the deadlock this table was created to make impossible. So the check that
    // matters is not the flag, it is that THE PROGRAM FINISHES AT ALL.
    //
    // Mutant X49 is that defect, and it survived until this section existed:
    // no test program contained a refused op, so nothing could reach it. Same shape
    // as X46 before the barrel was full -- a mutant stating a real fragility
    // that the traffic could not exercise.
    int32_t xs[1] = {3 << 16}, ys[1] = {5 << 16}, got[1][2];
    int clocks = 0;
    (void)run_long(top, zfield::OP_RING, 1, xs, ys, 0u, 1, false, got, &clocks);
    check(clocks < 20000, "a RING program finishes rather than parking forever", 1,
          clocks < 20000 ? 1 : 0);
    check(top.unsupported_o == 1, "and the ALU reports it as unsupported", 1,
          (uint32_t)top.unsupported_o);
    printf("   MEASURED: RING retired in %d clocks, unsupported_o = %u\n", clocks,
           (unsigned)top.unsupported_o);
  }

  printf("== section 6b: SPLINE is SERVED, and its answer reaches the register ==\n");
  {
    // THE SEAM, NOT THE ARITHMETIC. The cubic is closed at 21/21 in the spline
    // unit's own sweep and the lookup at 6930 checks in the curve service's,
    // so what is unproven HERE is the path: the executor offering 0x1B, the
    // dispatcher accepting it, the service path routing it to the curve
    // service rather than the noise unit, and the answer arriving in the right
    // register of the right context under the right tag.
    //
    // A VALUE CHECK IS WHAT PROVES THAT PATH. "It finished and no flag fired"
    // would pass just as happily if the answer were somebody else's.
    zfield::Table tb;
    // SET, NOT DEFAULTED. `kind` is read only by the DECODER, which validates
    // a serialised table; exec_op never looks at it, so it cannot move the
    // answer. It is initialised anyway because reading an uninitialised member
    // is a defect whether or not this particular reader happens to skip it --
    // and 0 is what the curve service's own bench uses for its SPLINE probes,
    // so the two differentials agree about the table they describe.
    tb.kind = 0;
    tb.x = {-(1 << 20), 0, 1 << 20, 3 << 20};
    tb.y = {7 << 16, 11 << 16, -(5 << 16), 2 << 16};
    tb.dy = {1 << 12, 1 << 12, 1 << 12, 1 << 12};
    std::vector<zfield::Table> tabs{tb};

    // Deliberately spread: below the first knot, exactly on a knot, between
    // two, and past the last -- so the end replication is exercised through
    // the whole machine and not only in the service's own bench.
    const int32_t probes[4] = {-(3 << 20), 0, (1 << 19), (9 << 20)};
    for (int p = 0; p < 4; ++p) {
      int32_t xs[1] = {probes[p]}, ys[1] = {0}, got[1][2] = {};
      int clocks = 0;
      const int al =
          run_long(top, zfield::OP_SPLINE, 1, xs, ys, 0u, 1, false, got, &clocks, false, &tb);

      zref::SatLedger L{};
      int32_t src = probes[p], dst = 0;
      zfield::steps::exec_op(zfield::OP_SPLINE, 0u, tabs, &src, &dst, &L);

      char what[96];
      snprintf(what, sizeof what, "SPLINE probe %d lands the oracle's value", p);
      check(al == 0, "a served SPLINE raises no alarm", 0, al);
      check(got[0][0] == dst, what, (uint32_t)dst, (uint32_t)got[0][0]);
      check(top.unsupported_o == 0, "and it is NOT reported unsupported", 0,
            (uint32_t)top.unsupported_o);
      check(top.wrong_op_o == 0, "and no service was asked for an op it lacks", 0,
            (uint32_t)top.wrong_op_o);
      printf("   MEASURED: probe %d -> %d in %d clocks\n", p, got[0][0], clocks);
    }
  }

  printf("== section 7: BOTH services at once, every answer against the oracle ==\n");
  {
    // FIVE DEFECTS SURVIVED THE SWEEP AND FOUR OF THEM ARE HERE. Every other
    // runner gives all contexts the same op, so one service is busy and the
    // other is idle, and:
    //
    //   W02  taking the handshake from the service that was NOT asked is
    //        invisible while both are idle and therefore both ready;
    //   W04  dropping the losing service's held response needs BOTH to be
    //        holding one in the same cycle;
    //   W07  serving DCURVE as CURVE needs a DCURVE to exist at all, and no
    //        program in this file contained one;
    //   W09  reading the table index from the wrong bits of the immediate is
    //        invisible while every table index is ZERO.
    //
    // So: two different tables, one of them in a NON-ZERO slot, and eight
    // contexts split across both services, all started before any finishes.
    zfield::Table ta;
    ta.kind = 0;
    ta.x = {-(1 << 20), 0, 1 << 20, 3 << 20};
    ta.y = {7 << 16, 11 << 16, -(5 << 16), 2 << 16};
    ta.dy = {1 << 12, 1 << 12, 1 << 12, 1 << 12};

    // DELIBERATELY UNLIKE ta. If the two tables agreed, reading the wrong one
    // would give the right answer and W09 would survive this section too.
    zfield::Table tb;
    tb.kind = 0;
    tb.x = {-(2 << 20), -(1 << 19), 2 << 20, 5 << 20};
    tb.y = {-(3 << 16), 23 << 16, 1 << 16, -(9 << 16)};
    tb.dy = {1 << 11, 3 << 11, 1 << 13, 1 << 12};

    // The oracle indexes tables[imm], so slot 1 must exist even though the
    // hardware is never asked for it. It is ta, not an empty table: an empty
    // one would crash the oracle rather than disagree with the hardware.
    std::vector<zfield::Table> tabs{ta, ta, tb};

    const uint8_t ops[8] = {zfield::OP_SPLINE, zfield::OP_NOISE2, zfield::OP_DCURVE,
                            zfield::OP_RIDGE,  zfield::OP_CURVE,  zfield::OP_SPLINE,
                            zfield::OP_NOISE2, zfield::OP_DCURVE};
    // Curve ops carry a TABLE INDEX; noise ops carry a SEED. Same field.
    const uint32_t imms[8] = {0u, 0xA5A5u, 2u, 0x1234u, 2u, 2u, 0x77u, 0u};
    const int32_t xs[8] = {(1 << 19), 3 << 16, -(3 << 20), 5 << 16,
                           0,         9 << 20, -(1 << 16), (1 << 21)};
    const int32_t ys[8] = {0, 5 << 16, 0, -(7 << 16), 0, 0, 2 << 16, 0};

    int32_t got[kCtx][2] = {};
    int clocks = 0;
    const int al = run_mixed(top, ops, imms, 8, xs, ys, got, &clocks, &ta, &tb);
    check(al == 0, "eight mixed contexts finish with no alarm", 0, al);
    check(top.wrong_op_o == 0, "and no service was asked for an op it lacks", 0,
          (uint32_t)top.wrong_op_o);

    for (int c = 0; c < 8; ++c) {
      zref::SatLedger L{};
      int32_t src[2] = {xs[c], ys[c]};
      int32_t dst[2] = {0, 0};
      zfield::steps::exec_op(ops[c], imms[c], tabs, src, dst, &L);

      const bool wide = (ops[c] == zfield::OP_NOISE2) || (ops[c] == zfield::OP_RIDGE);
      char what[96];
      snprintf(what, sizeof what, "ctx %d (op %02X) lands the oracle's value", c, (unsigned)ops[c]);
      check(got[c][0] == dst[0], what, (uint32_t)dst[0], (uint32_t)got[c][0]);
      if (wide) {
        snprintf(what, sizeof what, "ctx %d (op %02X) lands the second register", c,
                 (unsigned)ops[c]);
        check(got[c][1] == dst[1], what, (uint32_t)dst[1], (uint32_t)got[c][1]);
      }
    }
    printf("   MEASURED: 8 mixed contexts across two services in %d clocks\n", clocks);
  }

  return zhao::report_and_exit("field_v3_full_directed");
}
