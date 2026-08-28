// field_v3_svcpath_directed.cpp — the whole long-op path end to end:
// dispatcher, service, shared multiplier bank and writeback arbiter, with a
// rival on the bank so refusal is real.
//
// WHAT THIS TESTS THAT NO BLOCK'S OWN SUITE CAN
// ----------------------------------------------
// Each piece is green alone: the dispatcher 28/28, the noise unit 23 mutants,
// the arbiter 17/17, the bank 14. Every defect that has cost real time in this
// engine was invisible to exactly those suites, because it lived BETWEEN two
// of them and a sweep cannot mutate a port that does not exist.
//
// So the questions here are the ones no block can answer about itself:
//
//   1. DOES A LONG OP SURVIVE THE ROUND TRIP? Four contexts gathered, one
//      four-point request, a bank that can refuse, results drained one
//      register per clock, each context released. Every value is checked
//      against zfield::steps::exec_op -- the shipped interpreter -- for the
//      CONTEXT THAT ASKED, which is what proves the routing rather than the
//      arithmetic.
//   2. WHAT DOES CONTENTION ACTUALLY COST, AND DOES IT CHANGE AN ANSWER? The
//      rival is driven and PROVEN to have been served, not merely to have
//      asked.
//   3. WHICH WRITEBACK POLICY SHOULD THE ENGINE USE? The register file has one
//      write port and the ALU wants it too. This is the measurement that
//      decides a question deliberately left open, and it is per claimant
//      because "the port was busy" is not the finding.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_svcpath.h"

#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kLanes = 4;
constexpr uint8_t OP_NOISE2 = 0x1C;
constexpr uint8_t OP_RIDGE = 0x22;

constexpr int POL_ALU_FIRST = 0;
constexpr int POL_DRAIN_FIRST = 1;
constexpr int POL_ROUND_ROBIN = 2;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
  int32_t coord() {
    switch (below(5)) {
      case 0: return 0;
      case 1: return (int32_t)0x00010000;
      case 2: return (int32_t)0x80000000;
      default: return (int32_t)next64();
    }
  }
};

struct Ctx {
  int ctx;
  int32_t x, y;
};

struct Wrote {
  int ctx, reg;
  uint32_t data;
};

/** One point through the shipped interpreter. */
void oracle_point(bool ridge, int32_t x, int32_t y, uint32_t seed, int32_t* r0, int32_t* r1) {
  const std::vector<zfield::Table> no_tables;
  zref::SatLedger L;
  const int32_t src[3] = {x, y, 0};
  int32_t dst[3] = {0, 0, 0};
  zfield::steps::exec_op(ridge ? zfield::OP_RIDGE : zfield::OP_NOISE2, seed, no_tables, src, dst,
                         &L);
  *r0 = dst[0];
  *r1 = ridge ? 0 : dst[1];
}

struct Dut {
  Vzhao_field_v3_svcpath& t;
  std::vector<Wrote> writes;      // only the DRAIN's writes
  std::vector<Wrote> alu_writes;  // only the ALU's
  std::vector<int> released;
  // The ALU is a background stream: it asks every clock with a distinctive
  // context so its writes can never be confused with the drain's.
  bool alu_busy = false;
  int alu_offered = 0;

  explicit Dut(Vzhao_field_v3_svcpath& top) : t(top) {}

  void reset(int policy) {
    t.rst_n = 0;
    t.long_valid_i = 0;
    t.flush_i = 0;
    t.alu_wb_valid_i = 0;
    t.rival_req_i = 0;
    t.wb_policy_i = (uint8_t)policy;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    writes.clear();
    alu_writes.clear();
    released.clear();
    alu_offered = 0;
  }

  /** One cycle. The ALU's writes use context 7 and register 31, so a write
      that belongs to neither stream is visible as itself. */
  void step() {
    if (alu_busy) {
      t.alu_wb_valid_i = 1;
      t.alu_wb_ctx_i = 7;
      t.alu_wb_reg_i = 31;
      t.alu_wb_data_i = 0xA10A10;
    } else {
      t.alu_wb_valid_i = 0;
    }
    t.eval();
    if (t.wr_en_o) {
      const Wrote w{(int)t.wr_ctx_o, (int)t.wr_reg_o, (uint32_t)t.wr_data_o};
      if (w.ctx == 7 && w.reg == 31) {
        alu_writes.push_back(w);
      } else {
        writes.push_back(w);
      }
    }
    if (alu_busy && t.alu_wb_ready_o) ++alu_offered;
    if (t.rel_valid_o) released.push_back((int)t.rel_ctx_o);
    zhao::tick(t);
  }

  bool offer(const Ctx& c, uint8_t op, int dst, uint32_t imm, int guard_max = 256) {
    t.long_valid_i = 1;
    t.long_ctx_i = (uint8_t)c.ctx;
    t.long_op_i = op;
    t.long_dst_i = (uint8_t)dst;
    t.long_s0_i = (uint32_t)c.x;
    t.long_s1_i = (uint32_t)c.y;
    t.long_s2_i = 0;
    t.long_s3_i = 0;
    t.long_imm_i = imm;
    int guard = 0;
    t.eval();
    while (!t.long_ready_o && guard++ < guard_max) step();
    if (!t.long_ready_o) {
      t.long_valid_i = 0;
      t.eval();
      return false;
    }
    step();
    t.long_valid_i = 0;
    t.eval();
    return true;
  }
};

/** Push one group through the whole path and check every write. */
void run_group(Vzhao_field_v3_svcpath& top, const std::vector<Ctx>& cs, bool ridge, int dst,
               uint32_t imm, int policy, bool rival, bool alu, const std::string& what) {
  Dut d(top);
  d.reset(policy);
  d.alu_busy = alu;
  const int n = (int)cs.size();
  const int w = ridge ? 1 : 2;

  for (int i = 0; i < n; ++i) {
    const bool ok = d.offer(cs[(size_t)i], ridge ? OP_RIDGE : OP_NOISE2, dst, imm);
    check(ok, (what + ": context " + std::to_string(i) + " accepted").c_str(), 1, ok ? 1 : 0);
    if (!ok) return;
  }
  if (n < kLanes) {
    d.t.flush_i = 1;
    d.t.eval();
  }

  Prng rv(0x51DEu + (uint32_t)n);
  int guard = 0;
  int rival_served = 0;
  while ((int)d.writes.size() < n * w && guard++ < 4000) {
    d.t.rival_req_i = (rival && (rv.below(2) != 0)) ? 1 : 0;
    d.step();
    if (d.t.rival_rsp_o) ++rival_served;
  }
  d.t.rival_req_i = 0;
  d.t.flush_i = 0;
  d.t.eval();
  for (int i = 0; i < 8; ++i) d.step();

  check(guard < 4000, (what + ": the group completed").c_str(), 1, guard < 4000 ? 1 : 0);
  check((int)d.writes.size() == n * w, (what + ": write count").c_str(), (uint32_t)(n * w),
        (uint32_t)d.writes.size());

  // LAW 1: every write lands in the CONTEXT THAT ASKED, at the right register,
  // with the value the interpreter gives for THAT context's own operands.
  if ((int)d.writes.size() == n * w) {
    size_t k = 0;
    for (int l = 0; l < n; ++l) {
      int32_t r0 = 0, r1 = 0;
      oracle_point(ridge, cs[(size_t)l].x, cs[(size_t)l].y, imm, &r0, &r1);
      const int32_t want[2] = {r0, r1};
      for (int m = 0; m < w; ++m, ++k) {
        check(d.writes[k].ctx == cs[(size_t)l].ctx,
              (what + ": write " + std::to_string(k) + " context").c_str(),
              (uint32_t)cs[(size_t)l].ctx, (uint32_t)d.writes[k].ctx);
        check(d.writes[k].reg == dst + m, (what + ": write " + std::to_string(k) + " reg").c_str(),
              (uint32_t)(dst + m), (uint32_t)d.writes[k].reg);
        check(d.writes[k].data == (uint32_t)want[m],
              (what + ": write " + std::to_string(k) + " VALUE").c_str(), (uint32_t)want[m],
              d.writes[k].data);
      }
    }
  }

  check(d.released.size() == (size_t)n, (what + ": one release per context").c_str(),
        (uint32_t)n, (uint32_t)d.released.size());
  check(d.t.bank_desync_o == 0, (what + ": the bank stayed in step").c_str(), 0,
        (int)d.t.bank_desync_o);
  check(d.t.tag_mismatch_o == 0, (what + ": no tag mismatch").c_str(), 0,
        (int)d.t.tag_mismatch_o);
  check(d.t.wrong_op_o == 0, (what + ": the service was never asked the wrong op").c_str(), 0,
        (int)d.t.wrong_op_o);
  check(d.t.groups_o == 1u, (what + ": one group issued").c_str(), 1, (int)d.t.groups_o);

  if (rival) {
    // LAW 2: the rival must have been SERVED, not merely allowed to ask.
    check(rival_served > 0, (what + ": the rival was actually served").c_str(), 1,
          rival_served > 0 ? 1 : 0);
    check(d.t.bank_stall_lanes_o > 0, (what + ": and the bank really refused somebody").c_str(),
          1, d.t.bank_stall_lanes_o > 0 ? 1 : 0);
  }
  if (alu) {
    check(d.alu_writes.size() > 0, (what + ": the ALU got the port too").c_str(), 1,
          d.alu_writes.size() > 0 ? 1 : 0);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vzhao_field_v3_svcpath top;
  const std::vector<Ctx> four = {
      {0, 0x00012345, 0x00023456},
      {1, -0x00034567, 0x00045678},
      {2, 0x7FFFFFFF, (int32_t)0x80000000},
      {3, 0, 0x00010000}};

  printf("== section 1: one long op, all the way through, undisturbed ==\n");
  run_group(top, four, false, 8, 0xBEEFu, POL_DRAIN_FIRST, false, false, "quiet NOISE2");
  run_group(top, four, true, 12, 0xF00Du, POL_DRAIN_FIRST, false, false, "quiet RIDGE");

  printf("== section 2: a partial group, all the way through ==\n");
  run_group(top, {four[0]}, false, 4, 0x1234u, POL_DRAIN_FIRST, false, false, "one-context NOISE2");
  run_group(top, {four[1], four[2]}, false, 6, 0x5678u, POL_DRAIN_FIRST, false, false,
            "two-context NOISE2");

  printf("== section 3: the BANK is contended, and answers do not move ==\n");
  run_group(top, four, false, 8, 0xBEEFu, POL_DRAIN_FIRST, true, false, "contended NOISE2");
  run_group(top, four, true, 12, 0xF00Du, POL_DRAIN_FIRST, true, false, "contended RIDGE");

  printf("== section 4: the WRITE PORT is contended too ==\n");
  run_group(top, four, false, 8, 0xBEEFu, POL_DRAIN_FIRST, true, true, "both contended");

  printf("== section 5: THE POLICY MEASUREMENT ==\n");
  {
    // The question deliberately left open in zhao_field_v3_wbarb: with the ALU
    // asking every clock and a drain wanting the same port, who waits and how
    // much? Same traffic, three policies, one model -- which is the reason the
    // policy is an input rather than a parameter.
    const char* names[3] = {"ALU first", "drain first", "round robin"};
    printf("   (ALU-first is EXPECTED to starve: it is measured here, not used)\n");
    for (int pol = 0; pol < 3; ++pol) {
      Dut d(top);
      d.reset(pol);
      d.alu_busy = true;
      for (const Ctx& c : four) d.offer(c, OP_NOISE2, 8, 0xBEEFu);
      int guard = 0;
      while ((int)d.writes.size() < 8 && guard++ < 4000) d.step();
      for (int i = 0; i < 8; ++i) d.step();

      const uint32_t served_alu = d.t.wb_served_o[0];
      const uint32_t served_drn = d.t.wb_served_o[1];
      const uint32_t stall_alu = d.t.wb_stalled_o[0];
      const uint32_t stall_drn = d.t.wb_stalled_o[1];
      printf("   %-12s drain finished in %4d clocks | served ALU %5u drain %2u | "
             "stalled ALU %5u drain %2u\n",
             names[pol], guard, served_alu, served_drn, stall_alu, stall_drn);

      const std::string w = std::string(names[pol]);
      check(served_alu > 0, (w + ": the ALU was served at least once").c_str(), 1,
            served_alu > 0 ? 1 : 0);

      if (pol == POL_ALU_FIRST) {
        // THE MEASUREMENT REJECTED THIS POLICY, and the starvation is PINNED
        // rather than treated as a failure -- it is the evidence for the
        // engine's choice, and if somebody later makes ALU-first live, that is
        // a change worth noticing rather than a silent improvement.
        //
        // I had argued this would be "self-limiting rather than a deadlock",
        // because a stalled drain holds contexts out of the ready set and so
        // reduces the ALU's own supply of work. That is a claim about a
        // FEEDBACK LOOP, and the arbiter's header said in advance that such
        // claims are the ones measurement overturns. It did.
        check(served_drn == 0u,
              (w + ": STARVES the drain outright -- measured, not argued").c_str(), 0,
              served_drn);
        check(guard >= 4000, (w + ": and never finishes").c_str(), 1, guard >= 4000 ? 1 : 0);
      } else {
        check(guard < 4000, (w + ": the drain finished").c_str(), 1, guard < 4000 ? 1 : 0);
        check((int)d.writes.size() == 8, (w + ": all eight registers landed").c_str(), 8,
              (uint32_t)d.writes.size());
        check(served_drn == 8u, (w + ": the drain was served exactly eight times").c_str(), 8,
              served_drn);
        // The ALU pays exactly the drain's length and not a clock more. That
        // is the number that makes services-first cheap rather than a
        // trade-off: eight writes for a four-point NOISE2, once per group.
        check(stall_alu == 8u, (w + ": and the ALU lost exactly the drain's eight clocks").c_str(),
              8, stall_alu);
      }
    }
  }

  return zhao::report_and_exit("field_v3_svcpath_directed");
}
