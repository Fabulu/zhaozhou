// field_alu_vec_directed.cpp — the four-wide ALU against the shipped oracle.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK HERE
// ---------------------------------------------------------------------------
// `zhao_field_alu_vec` adds no arithmetic. It instantiates LANES copies of the
// swept `zhao_field_alu` and combines their flags, so the values cannot be
// wrong unless the WIRING is wrong -- and wiring is exactly what a packed
// vector interface gets wrong. Every lane reads its operands out of a single
// packed word by bit slice, and a slice off by one lane produces perfectly
// plausible numbers that belong to the neighbour.
//
// So this file is not really testing arithmetic. It is testing that lane l
// computed lane l's answer from lane l's operands, which is why every check
// gives each lane DIFFERENT data. A test that fed all four lanes the same
// numbers would pass with the lanes crossed, and this project has already
// shipped three streaming tests with exactly that hole in them.
//
// ---------------------------------------------------------------------------
// THE TWO KINDS OF FLAG
// ---------------------------------------------------------------------------
// `is_end`, `writes` and `op_unsupported` are properties of the OPCODE, which
// every lane shares, so they must agree and lane 0 speaks for all. `sat_*` are
// properties of the DATA, so one lane may saturate alone and the group flag is
// the OR. Both are checked, including the case where exactly one lane
// saturates -- which is the case that tells the two kinds apart.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_alu_vec.h"

#include "zfield/zfield.hpp"
#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

constexpr int kLanes = 4;

/** Deterministic, so a failure is reproducible from the seed alone. */
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return (uint32_t)(s >> 32);
  }
};

/** The shipped interpreter for ONE lane. Scalar ops take their sources
 *  flattened in a, b, c order -- the same order the executor presents them. */
int32_t oracle(uint8_t op, uint32_t imm, int32_t a, int32_t b, int32_t c, zref::SatLedger* L) {
  const std::vector<zfield::Table> no_tables;
  int32_t src[9] = {a, b, c};
  int32_t dst[3] = {};
  zfield::steps::exec_op(op, imm, no_tables, src, dst, L);
  return dst[0];
}

void put(uint32_t* w, int lane, int32_t v) { w[lane] = (uint32_t)v; }

/** A 66-bit product, packed lane by lane, exactly as the bank delivers it. */
void put_prod(Vzhao_field_alu_vec& t, int lane, int64_t p) {
  // Verilator packs 66*LANES bits into a word array; write through the
  // generated accessor by lane offset in bits.
  const int base = 66 * lane;
  for (int b = 0; b < 66; ++b) {
    const int bit = base + b;
    const uint32_t v = (uint32_t)((p >> (b < 64 ? b : 63)) & 1);
    const uint32_t mask = 1u << (bit & 31);
    if (v)
      t.prod_ab_i[bit >> 5] |= mask;
    else
      t.prod_ab_i[bit >> 5] &= ~mask;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_alu_vec top;

  const uint8_t kOps[] = {zfield::OP_ADD, zfield::OP_SUB, zfield::OP_MIN, zfield::OP_ABS,
                          zfield::OP_CLAMP};

  printf("== section 1: every lane answers for ITS OWN operands ==\n");
  {
    Rng rng(0xA11CE);
    int cases = 0;
    for (uint8_t op : kOps) {
      for (int trial = 0; trial < 24; ++trial) {
        int32_t a[kLanes], b[kLanes], c[kLanes];
        for (int l = 0; l < kLanes; ++l) {
          // DIFFERENT PER LANE, ALWAYS. A crossed slice must change an answer.
          a[l] = (int32_t)rng.next();
          b[l] = (int32_t)rng.next();
          c[l] = (int32_t)rng.next();
          put(top.a0_i.data(), l, a[l]);
          put(top.b0_i.data(), l, b[l]);
          put(top.c_i.data(), l, c[l]);
          put(top.a1_i.data(), l, 0);
          put(top.a2_i.data(), l, 0);
          put(top.b1_i.data(), l, 0);
          put(top.b2_i.data(), l, 0);
        }
        // CLAMP wants a sane ordering or it is only ever testing the clamp.
        if (op == zfield::OP_CLAMP)
          for (int l = 0; l < kLanes; ++l) {
            b[l] = -(1 << 16) * (l + 1);
            c[l] = (1 << 16) * (l + 1);
            put(top.b0_i.data(), l, b[l]);
            put(top.c_i.data(), l, c[l]);
          }
        top.op_i = op;
        top.imm_i = 0;
        top.eval();

        for (int l = 0; l < kLanes; ++l) {
          zref::SatLedger L;
          const int32_t want = oracle(op, 0, a[l], b[l], c[l], &L);
          const int32_t got = (int32_t)top.result_o[l];
          char what[96];
          snprintf(what, sizeof what, "op 0x%02X lane %d matches the oracle", op, l);
          zhao::check(got == want, what, (uint32_t)want, (uint32_t)got);
          ++cases;
        }
        zhao::check(top.lane_desync_o == 0, "no lane disagrees about the opcode", 0,
                    (uint32_t)top.lane_desync_o);
      }
    }
    printf("   MEASURED: %d lane-results checked across %zu ops\n", cases, sizeof kOps);
  }

  printf("== section 2: MUL takes its product from the bank, per lane ==\n");
  {
    Rng rng(0xB0B);
    for (int trial = 0; trial < 16; ++trial) {
      int32_t a[kLanes], b[kLanes];
      for (int l = 0; l < kLanes; ++l) {
        a[l] = (int32_t)(rng.next() & 0x000FFFFF) - 0x00080000;
        b[l] = (int32_t)(rng.next() & 0x000FFFFF) - 0x00080000;
        put(top.a0_i.data(), l, a[l]);
        put(top.b0_i.data(), l, b[l]);
        put(top.c_i.data(), l, 0);
        put_prod(top, l, (int64_t)a[l] * (int64_t)b[l]);
      }
      top.op_i = zfield::OP_MUL;
      top.imm_i = 0;
      top.eval();
      for (int l = 0; l < kLanes; ++l) {
        zref::SatLedger L;
        const int32_t want = oracle(zfield::OP_MUL, 0, a[l], b[l], 0, &L);
        const int32_t got = (int32_t)top.result_o[l];
        char what[96];
        snprintf(what, sizeof what, "MUL lane %d takes ITS OWN product", l);
        zhao::check(got == want, what, (uint32_t)want, (uint32_t)got);
      }
    }
  }

  printf("== section 3: ONE lane saturating raises the group flag, and only it ==\n");
  {
    // The case that separates a DATA flag from an OPCODE flag. Three lanes add
    // harmlessly; lane 2 overflows. `sat_add` must rise for the group, and the
    // opcode-shaped flags must not move.
    for (int l = 0; l < kLanes; ++l) {
      put(top.a0_i.data(), l, l == 2 ? INT32_MAX : 1);
      put(top.b0_i.data(), l, l == 2 ? INT32_MAX : 1);
      put(top.c_i.data(), l, 0);
    }
    top.op_i = zfield::OP_ADD;
    top.imm_i = 0;
    top.eval();
    zhao::check(top.sat_add_o == 1, "one lane saturating raises the group's add flag", 1,
                (uint32_t)top.sat_add_o);
    zhao::check(top.lane_desync_o == 0, "and no lane disagrees about the opcode", 0,
                (uint32_t)top.lane_desync_o);
    zhao::check(top.writes_o == 1, "ADD still writes", 1, (uint32_t)top.writes_o);
    zhao::check(top.is_end_o == 0, "and is not END", 0, (uint32_t)top.is_end_o);

    // The quiet lanes still carry their own correct answer.
    for (int l = 0; l < kLanes; ++l) {
      if (l == 2) continue;
      char what[80];
      snprintf(what, sizeof what, "lane %d is unharmed by lane 2 saturating", l);
      zhao::check((int32_t)top.result_o[l] == 2, what, 2, top.result_o[l]);
    }
  }

  printf("== section 4: the opcode-shaped flags follow the opcode ==\n");
  {
    for (int l = 0; l < kLanes; ++l) {
      put(top.a0_i.data(), l, 1 << 16);
      put(top.b0_i.data(), l, 1 << 16);
      put(top.c_i.data(), l, 0);
    }
    top.op_i = zfield::OP_END;
    top.imm_i = 0;
    top.eval();
    zhao::check(top.is_end_o == 1, "OP_END is END in every lane at once", 1,
                (uint32_t)top.is_end_o);
    zhao::check(top.lane_desync_o == 0, "and the lanes agree about it", 0,
                (uint32_t)top.lane_desync_o);

    top.op_i = 0xFE;  // not an op this machine serves
    top.eval();
    zhao::check(top.op_unsupported_o == 1, "an unknown opcode is refused, not guessed", 1,
                (uint32_t)top.op_unsupported_o);
    zhao::check(top.lane_desync_o == 0, "and refused in every lane together", 0,
                (uint32_t)top.lane_desync_o);
  }

  return zhao::report_and_exit("field_alu_vec_directed");
}
