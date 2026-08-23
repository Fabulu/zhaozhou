// field_alu_ops.cpp — the Field IR arithmetic ops, RTL against the oracle.
//
// This one file covers FIELD.MOV, FIELD.ADD and FIELD.SUB — the three op
// differentials ledger rule V10 names as blockers for ALL FOUR `FIELD.SEQ.*`
// blocks — and the other twelve arithmetic ops alongside them, because they
// share one ALU and testing them separately would mean four files that each
// instantiate the same module.
//
// THE ORACLE IS `zfield::interpret`'S OWN ARITHMETIC. Every law here is a
// `zref::fx_*` primitive that the interpreter calls directly: `fx_add`,
// `fx_sub`, `fx_mul`, `fx_mad`, `fx_min`, `fx_max`, `fx_clamp`, plus the
// interpreter's own `abs_sat`, CMP predicate table and `dot_finish`. Nothing is
// re-derived from spec prose, which is what keeps the block on the right side of
// field-ir.md §1's grep-audit law.
//
// FOUR LAWS CARRY THE FILE, and each is a place an implementation drifts:
//
//   1. MAD IS ONE ROUNDING, AND THE DIFFERENCE IS AT THE RAILS. `a*b + (c<<16)`
//      exact in s64, rounded once (qformats A3b). NOT off by an LSB in general:
//      `(p + c*2^16) >> 16` equals `(p >> 16) + c` exactly for integer c, so the
//      two orders agree wherever nothing overflows and differ only when the
//      intermediate saturates and the exact form does not. Section 4 constructs
//      those cases; the residue sweep beside them is background.
//   2. ABS OF INT32_MIN IS INT32_MIN. Not INT32_MAX. Negating does not fit and
//      the reference does NOT saturate. A tidier saturating abs disagrees on
//      exactly one input in four billion.
//   3. CMP'S TRUE IS 0x10000. fx16 one-point-zero, because the result feeds
//      arithmetic. Returning 1 makes every comparison 1/65536th of itself.
//   4. THE SATURATION LEDGERS ARE SEPARATE. ADD/SUB record in `add`, the
//      rescaling ops in `mul`. A block that saturated in the wrong lane can
//      still produce the right number and be wrong everywhere else.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_alu_tb.h"

#include "zhao_sim.hpp"
#include "zfield/zfield.hpp"
#include "zref/zref_fixp.hpp"

namespace {

using zhao::check;

constexpr uint8_t OP_END = 0x00, OP_MOV = 0x01, OP_LDC = 0x02, OP_ADD = 0x03, OP_SUB = 0x04,
                  OP_MUL = 0x05, OP_MAD = 0x06, OP_MIN = 0x07, OP_MAX = 0x08, OP_ABS = 0x09,
                  OP_CLAMP = 0x0A, OP_SELECT = 0x0B, OP_CMP = 0x0C, OP_DOT2 = 0x10, OP_DOT3 = 0x11;

inline zref::fx16 F(int32_t v) { return zref::fx16{v}; }

/** The interpreter's own `dot_finish`, restated for the two DOT ops. */
int32_t dot_finish(__int128 p, bool& sat_mul) {
  const __int128 r = (p + (static_cast<__int128>(1) << 15)) >> 16;
  if (r > INT32_MAX) {
    sat_mul = true;
    return INT32_MAX;
  }
  if (r < INT32_MIN) {
    sat_mul = true;
    return INT32_MIN;
  }
  return static_cast<int32_t>(r);
}

struct Res {
  int32_t value = 0;
  bool is_end = false, writes = true, unsupported = false;
  bool sat_add = false, sat_mul = false;
  // ABS saturates into the `rescale` lane. The block had no such output until
  // the sequencer's differential showed ABS disagreeing with the reference at
  // INT32_MIN -- the lane was missing because the behaviour was missing.
  bool sat_rescale = false;
};

/** The oracle: exactly what `zfield::interpret` does for this op. */
Res oracle(uint8_t op, uint32_t imm, const int32_t a[3], const int32_t b[3], int32_t c) {
  Res o;
  zref::SatLedger L{};
  switch (op) {
    case OP_END:
      o.is_end = true;
      o.writes = false;
      break;
    case OP_MOV:
      o.value = a[0];
      break;
    case OP_LDC:
      o.value = static_cast<int32_t>(imm);
      break;
    case OP_ADD:
      o.value = zref::fx_add(F(a[0]), F(b[0]), &L).raw;
      break;
    case OP_SUB:
      o.value = zref::fx_sub(F(a[0]), F(b[0]), &L).raw;
      break;
    case OP_MUL:
      o.value = zref::fx_mul(F(a[0]), F(b[0]), &L).raw;
      break;
    case OP_MAD:
      o.value = zref::fx_mad(F(a[0]), F(b[0]), F(c), &L).raw;
      break;
    case OP_MIN:
      o.value = zref::fx_min(F(a[0]), F(b[0])).raw;
      break;
    case OP_MAX:
      o.value = zref::fx_max(F(a[0]), F(b[0])).raw;
      break;
    case OP_ABS:
      // The interpreter's abs_sat (zfield_interpret.cpp §3.7): INT32_MIN
      // SATURATES to INT32_MAX and bumps the `rescale` lane.
      //
      // This restatement previously said "INT32_MIN stays INT32_MIN", which is
      // the opposite, and the RTL said the same -- so the two agreed and the
      // reference was the odd one out for as long as nobody compared against
      // it. The sequencer's differential runs whole programs through
      // `zfield::interpret` and found it in its first random sweep.
      if (a[0] == INT32_MIN) {
        o.value = INT32_MAX;
        o.sat_rescale = true;
      } else {
        o.value = (a[0] < 0) ? -a[0] : a[0];
      }
      break;
    case OP_CLAMP:
      o.value = zref::fx_clamp(F(a[0]), F(b[0]), F(c)).raw;
      break;
    case OP_SELECT:
      o.value = (c != 0) ? a[0] : b[0];
      break;
    case OP_CMP: {
      bool t = false;
      switch (imm) {
        case 0:
          t = a[0] == b[0];
          break;
        case 1:
          t = a[0] != b[0];
          break;
        case 2:
          t = a[0] < b[0];
          break;
        case 3:
          t = a[0] <= b[0];
          break;
        case 4:
          t = a[0] > b[0];
          break;
        case 5:
          t = a[0] >= b[0];
          break;
        default:
          t = false;
          break;
      }
      o.value = t ? 0x10000 : 0;
      break;
    }
    case OP_DOT2: {
      const __int128 p = static_cast<__int128>(static_cast<int64_t>(a[0])) * b[0] +
                         static_cast<__int128>(static_cast<int64_t>(a[1])) * b[1];
      o.value = dot_finish(p, o.sat_mul);
      break;
    }
    case OP_DOT3: {
      const __int128 p = static_cast<__int128>(static_cast<int64_t>(a[0])) * b[0] +
                         static_cast<__int128>(static_cast<int64_t>(a[1])) * b[1] +
                         static_cast<__int128>(static_cast<int64_t>(a[2])) * b[2];
      o.value = dot_finish(p, o.sat_mul);
      break;
    }
    default:
      o.unsupported = true;
      o.writes = false;
      break;
  }
  // The reference's SatLedger lanes, as the interpreter fills them.
  if (op == OP_ADD || op == OP_SUB) o.sat_add = L.add != 0;
  if (op == OP_MUL || op == OP_MAD) o.sat_mul = L.mul != 0;
  return o;
}

Res run(Vzhao_field_alu_tb& dut, uint8_t op, uint32_t imm, const int32_t a[3], const int32_t b[3],
        int32_t c) {
  dut.op_i = op;
  dut.imm_i = imm;
  dut.a0_i = static_cast<uint32_t>(a[0]);
  dut.a1_i = static_cast<uint32_t>(a[1]);
  dut.a2_i = static_cast<uint32_t>(a[2]);
  dut.b0_i = static_cast<uint32_t>(b[0]);
  dut.b1_i = static_cast<uint32_t>(b[1]);
  dut.b2_i = static_cast<uint32_t>(b[2]);
  dut.c_i = static_cast<uint32_t>(c);
  dut.eval();
  Res r;
  r.value = static_cast<int32_t>(dut.result_o);
  r.is_end = dut.is_end_o != 0;
  r.writes = dut.writes_o != 0;
  r.unsupported = dut.op_unsupported_o != 0;
  r.sat_add = dut.sat_add_o != 0;
  r.sat_mul = dut.sat_mul_o != 0;
  r.sat_rescale = dut.sat_rescale_o != 0;
  return r;
}

void diff(Vzhao_field_alu_tb& dut, uint8_t op, uint32_t imm, const int32_t a[3], const int32_t b[3],
          int32_t c, const char* what) {
  const Res want = oracle(op, imm, a, b, c);
  const Res got = run(dut, op, imm, a, b, c);
  const std::string t(what);
  check(got.unsupported == want.unsupported, (t + ": supported").c_str(), want.unsupported ? 1 : 0,
        got.unsupported ? 1 : 0);
  if (want.unsupported) return;
  check(got.is_end == want.is_end, (t + ": is_end").c_str(), want.is_end ? 1 : 0,
        got.is_end ? 1 : 0);
  check(got.writes == want.writes, (t + ": writes").c_str(), want.writes ? 1 : 0,
        got.writes ? 1 : 0);
  if (!want.writes) return;
  check(got.value == want.value, (t + ": value").c_str(), static_cast<uint32_t>(want.value),
        static_cast<uint32_t>(got.value));
  check(got.sat_add == want.sat_add, (t + ": SatLedger::add lane").c_str(), want.sat_add ? 1 : 0,
        got.sat_add ? 1 : 0);
  check(got.sat_rescale == want.sat_rescale, (t + ": SatLedger::rescale lane").c_str(),
        want.sat_rescale ? 1 : 0, got.sat_rescale ? 1 : 0);
  check(got.sat_mul == want.sat_mul, (t + ": SatLedger::mul lane").c_str(), want.sat_mul ? 1 : 0,
        got.sat_mul ? 1 : 0);
}

void diff1(Vzhao_field_alu_tb& dut, uint8_t op, int32_t a0, int32_t b0, int32_t c, const char* what,
           uint32_t imm = 0) {
  const int32_t a[3] = {a0, 0, 0};
  const int32_t b[3] = {b0, 0, 0};
  diff(dut, op, imm, a, b, c, what);
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
  /** A value biased toward the rails and toward small magnitudes alike. */
  int32_t val() {
    switch (below(6)) {
      case 0:
        return 0;
      case 1:
        return INT32_MAX;
      case 2:
        return INT32_MIN;
      case 3:
        return static_cast<int32_t>(next()) >> static_cast<int>(below(24));
      case 4:
        return static_cast<int32_t>(next()) >> 16;
      default:
        return static_cast<int32_t>(next());
    }
  }
};

const uint8_t kArith[] = {OP_MOV, OP_LDC, OP_ADD,   OP_SUB,    OP_MUL, OP_MAD,  OP_MIN,
                          OP_MAX, OP_ABS, OP_CLAMP, OP_SELECT, OP_CMP, OP_DOT2, OP_DOT3};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_field_alu_tb dut;

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0xA1E0u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      const uint8_t op = kArith[rng.below(sizeof kArith)];
      int32_t a[3] = {rng.val(), rng.val(), rng.val()};
      int32_t b[3] = {rng.val(), rng.val(), rng.val()};
      const int32_t c = rng.val();
      const uint32_t imm = (op == OP_CMP) ? rng.below(8) : rng.next();
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] op 0x%02X", it, op);
      diff(dut, op, imm, a, b, c, tag);
    }
    dut.final();
    return zhao::report_and_exit("field_alu_random");
  }

  // ---- 1. FIELD.MOV, and END ----------------------------------------------
  {
    diff1(dut, OP_MOV, 0x1234'5678, 0, 0, "FIELD.MOV copies a");
    diff1(dut, OP_MOV, INT32_MIN, 0, 0, "FIELD.MOV of INT32_MIN");
    diff1(dut, OP_MOV, 0, 0x7FFF'FFFF, 0, "FIELD.MOV ignores b");
    const int32_t z[3] = {1, 2, 3};
    diff(dut, OP_END, 0, z, z, 4, "OP_END stops and writes nothing");
  }

  // ---- 2. FIELD.ADD and FIELD.SUB -----------------------------------------
  // The two the ledger names, and the rails where saturation decides.
  {
    diff1(dut, OP_ADD, 1, 2, 0, "FIELD.ADD: small");
    diff1(dut, OP_ADD, -1, -2, 0, "FIELD.ADD: negative");
    diff1(dut, OP_ADD, INT32_MAX, 1, 0, "FIELD.ADD: saturates at the top rail");
    diff1(dut, OP_ADD, INT32_MIN, -1, 0, "FIELD.ADD: saturates at the bottom rail");
    diff1(dut, OP_ADD, INT32_MAX, INT32_MIN, 0, "FIELD.ADD: rails cancel exactly");
    diff1(dut, OP_ADD, INT32_MAX, 0, 0, "FIELD.ADD: at the rail without crossing it");

    diff1(dut, OP_SUB, 5, 3, 0, "FIELD.SUB: small");
    diff1(dut, OP_SUB, INT32_MIN, 1, 0, "FIELD.SUB: saturates at the bottom rail");
    diff1(dut, OP_SUB, INT32_MAX, -1, 0, "FIELD.SUB: saturates at the top rail");
    diff1(dut, OP_SUB, INT32_MIN, INT32_MIN, 0, "FIELD.SUB: INT32_MIN from itself is zero");
    // Not the same as ADD with a negated operand -- negating INT32_MIN does not
    // fit, so a block that implemented SUB that way is wrong here.
    diff1(dut, OP_SUB, 0, INT32_MIN, 0, "FIELD.SUB: zero minus INT32_MIN saturates");
  }

  // ---- 3. MUL: ONE rescale by 16 ------------------------------------------
  {
    diff1(dut, OP_MUL, 0x10000, 0x10000, 0, "MUL: 1.0 * 1.0");
    diff1(dut, OP_MUL, 0x8000, 0x10000, 0, "MUL: 0.5 * 1.0");
    diff1(dut, OP_MUL, 1, 1, 0, "MUL: one ulp squared rounds to zero");
    diff1(dut, OP_MUL, 0x8000, 0x8000, 0, "MUL: rounding at exactly a half");
    diff1(dut, OP_MUL, -0x8000, 0x8000, 0, "MUL: negative half rounds toward +inf");
    diff1(dut, OP_MUL, INT32_MAX, INT32_MAX, 0, "MUL: saturates");
    diff1(dut, OP_MUL, INT32_MIN, INT32_MIN, 0, "MUL: negative rails saturate");
    diff1(dut, OP_MUL, INT32_MIN, 0x10000, 0, "MUL: INT32_MIN by one");
  }

  // ---- 4. MAD IS ONE ROUNDING, AND IT MATTERS AT THE RAILS ----------------
  // a*b + (c<<16) formed exactly in s64, rounded once.
  //
  // WHERE THE DIFFERENCE ACTUALLY IS, corrected after a mutation passed this
  // whole section: multiply-round-then-add is NOT wrong by an LSB in general.
  // `(p + c*2^16) >> 16` equals `(p >> 16) + c` exactly for integer c, so the
  // two orders agree on every input where nothing overflows. They differ ONLY
  // when the intermediate saturates and the exact form does not -- the rounding
  // is identical, the RANGE is not.
  //
  // So the residue sweep below is not the test; it is background. The rail cases
  // after it are the test, and they are constructed so the intermediate
  // saturates while the exact result comfortably fits.
  {
    const int32_t res[] = {1, 3, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 12345, -12345, -0x8000, -0x8001};
    for (int32_t r : res) {
      for (int32_t cc : {0, 1, -1, 0x10000, -0x10000}) {
        char tag[112];
        std::snprintf(tag, sizeof tag, "MAD single rounding: a=%d c=%d", r, cc);
        diff1(dut, OP_MAD, r, 3, cc, tag);
      }
    }
    diff1(dut, OP_MAD, INT32_MAX, INT32_MAX, INT32_MAX, "MAD: everything at the rail");
    diff1(dut, OP_MAD, INT32_MIN, INT32_MAX, INT32_MIN, "MAD: mixed rails");

    // The cases that actually separate one rounding from two. a*b >> 16 lands
    // just PAST the top rail, so an implementation that narrowed there is stuck
    // at INT32_MAX; c then pulls the exact result comfortably back inside, and
    // the two answers differ by exactly how far past the rail the product went.
    diff1(dut, OP_MAD, INT32_MAX, 65537, -40000,
          "MAD: intermediate saturates, exact result does NOT");
    diff1(dut, OP_MAD, INT32_MAX, 70000, -300000,
          "MAD: intermediate saturates further past the rail");
    diff1(dut, OP_MAD, INT32_MIN, 65537, 40000,
          "MAD: intermediate saturates at the BOTTOM rail, exact result does not");
    diff1(dut, OP_MAD, INT32_MIN, 70000, 300000, "MAD: bottom rail, further past");

    // And the fixture is only meaningful if the two orders really do disagree
    // here, so that is asserted rather than assumed.
    {
      const int64_t p = static_cast<int64_t>(INT32_MAX) * 65537;
      const int32_t exact = zref::fx_mad(F(INT32_MAX), F(65537), F(-40000), nullptr).raw;
      const int32_t narrowed = zref::rescale_s32(p, 16, nullptr);
      const int64_t two_step = static_cast<int64_t>(narrowed) + (-40000);
      const int32_t two_step_sat =
          two_step > INT32_MAX
              ? INT32_MAX
              : (two_step < INT32_MIN ? INT32_MIN : static_cast<int32_t>(two_step));
      check(exact != two_step_sat,
            "the fixture separates one rounding from two -- they really do differ here", 1,
            exact != two_step_sat ? 1 : 0);
    }
  }

  // ---- 5. MIN, MAX, and CLAMP's ORDER -------------------------------------
  // fx_clamp is max(lo, min(hi, x)). With lo > hi the two orders differ, and
  // that is not a hypothetical: a clamp with crossed bounds returns `lo`.
  {
    diff1(dut, OP_MIN, 5, 7, 0, "MIN");
    diff1(dut, OP_MIN, INT32_MIN, INT32_MAX, 0, "MIN of the rails");
    diff1(dut, OP_MAX, 5, 7, 0, "MAX");
    diff1(dut, OP_MAX, INT32_MIN, INT32_MAX, 0, "MAX of the rails");

    diff1(dut, OP_CLAMP, 5, 0, 10, "CLAMP: inside");
    diff1(dut, OP_CLAMP, -5, 0, 10, "CLAMP: below lo");
    diff1(dut, OP_CLAMP, 50, 0, 10, "CLAMP: above hi");
    diff1(dut, OP_CLAMP, 5, 10, 0, "CLAMP: CROSSED bounds -- max(lo, min(hi, x)) gives lo");
    diff1(dut, OP_CLAMP, INT32_MIN, INT32_MIN, INT32_MAX, "CLAMP: full range");
  }

  // ---- 6. ABS OF INT32_MIN IS INT32_MIN -----------------------------------
  {
    diff1(dut, OP_ABS, 5, 0, 0, "ABS: positive");
    diff1(dut, OP_ABS, -5, 0, 0, "ABS: negative");
    // The rail. abs_sat(INT32_MIN) is INT32_MAX and bumps `rescale` -- the case
    // this test previously got wrong in the same direction as the RTL.
    diff1(dut, OP_ABS, INT32_MIN, 0, 0, "ABS: the INT32_MIN rail");
    diff1(dut, OP_ABS, INT32_MIN + 1, 0, 0, "ABS: one above the rail");
    diff1(dut, OP_ABS, INT32_MAX, 0, 0, "ABS: the positive rail");
    diff1(dut, OP_ABS, 0, 0, 0, "ABS: zero");
    diff1(dut, OP_ABS, INT32_MAX, 0, 0, "ABS: the top rail");
    diff1(dut, OP_ABS, INT32_MIN, 0, 0, "ABS: INT32_MIN SATURATES to INT32_MAX");

    // ---- the check that was wrong, and how it is written now -------------
    // This block used to read:
    //
    //     const Res r = oracle(OP_ABS, ...);
    //     check(r.value == INT32_MIN, "the reference really does return ...");
    //
    // with a comment saying "the oracle and the RTL could agree and both be
    // wrong about what the reference does with this one input". It named the
    // failure mode exactly and then walked into it: the oracle here is a
    // RESTATEMENT, so asking it what the reference does only asks the
    // restatement what it thinks. It said INT32_MIN, the RTL said INT32_MIN,
    // and `zfield_interpret.cpp` §3.7 says INT32_MAX + SAT.
    //
    // The fix is not a better comment. It is to ask the SHIPPED INTERPRETER,
    // by running a real one-instruction program through it -- the same thing
    // the sequencer's differential does, which is what finally caught this.
    {
      zfield::Decoded d;
      d.profile = 0;
      zfield::Instr ins{};
      ins.op = zfield::OP_ABS;
      ins.dst = 1;
      ins.a = 0;
      d.instrs.push_back(ins);
      zfield::Instr e{};
      e.op = zfield::OP_END;
      d.instrs.push_back(e);
      zfield::IoLane li{};
      li.name = "i";
      li.type = 0;
      li.reg = 0;
      d.in_lanes.push_back(li);
      zfield::IoLane lo{};
      lo.name = "o";
      lo.type = 0;
      lo.reg = 1;
      d.out_lanes.push_back(lo);

      const int32_t in[1] = {INT32_MIN};
      int32_t out[1] = {0};
      const zfield::Status st = zfield::interpret(d, in, 1, out, 1);
      check(out[0] == INT32_MAX, "the SHIPPED interpreter returns INT32_MAX for abs(INT32_MIN)",
            static_cast<uint32_t>(INT32_MAX), static_cast<uint32_t>(out[0]));
      check(st.sat, "and reports it as a saturation", 1, st.sat ? 1 : 0);
    }
  }

  // ---- 7. SELECT and CMP --------------------------------------------------
  // CMP's true is 0x10000, fx16 one-point-zero, because the result feeds
  // arithmetic ops. Every predicate, and both outcomes of each.
  {
    diff1(dut, OP_SELECT, 111, 222, 0, "SELECT: c == 0 picks b");
    diff1(dut, OP_SELECT, 111, 222, 1, "SELECT: c != 0 picks a");
    diff1(dut, OP_SELECT, 111, 222, -1, "SELECT: any non-zero c picks a");

    for (uint32_t pred = 0; pred < 6; ++pred) {
      for (auto pr : {std::pair<int32_t, int32_t>{3, 3},
                      {3, 5},
                      {5, 3},
                      {INT32_MIN, INT32_MAX},
                      {INT32_MAX, INT32_MIN}}) {
        char tag[96];
        std::snprintf(tag, sizeof tag, "CMP pred %u: %d vs %d", pred, pr.first, pr.second);
        diff1(dut, OP_CMP, pr.first, pr.second, 0, tag, pred);
      }
    }
    {
      const int32_t a[3] = {1, 0, 0}, b[3] = {1, 0, 0};
      const Res r = run(dut, OP_CMP, 0, a, b, 0);
      check(r.value == 0x10000, "CMP true is fx16 1.0, not 1", 0x10000,
            static_cast<uint32_t>(r.value));
    }
    // An out-of-range predicate: the reference's inner switch has no default, so
    // `t` stays false. Decode rejects these (V9), which makes this the
    // unreachable-but-defined case rather than a live path.
    diff1(dut, OP_CMP, 3, 3, 0, "CMP: an out-of-range predicate is false", 9);
  }

  // ---- 8. DOT2 and DOT3: exact sum, ONE rescale ---------------------------
  {
    const int32_t a[3] = {0x10000, 0x10000, 0x10000};
    const int32_t b[3] = {0x10000, 0x10000, 0x10000};
    diff(dut, OP_DOT2, 0, a, b, 0, "DOT2: unit vectors");
    diff(dut, OP_DOT3, 0, a, b, 0, "DOT3: unit vectors");
    const int32_t big[3] = {INT32_MAX, INT32_MAX, INT32_MAX};
    diff(dut, OP_DOT2, 0, big, big, 0, "DOT2: saturates");
    diff(dut, OP_DOT3, 0, big, big, 0, "DOT3: saturates");
    const int32_t mix[3] = {INT32_MAX, INT32_MIN, INT32_MAX};
    diff(dut, OP_DOT3, 0, mix, big, 0, "DOT3: mixed rails, the exact sum matters");
    // The residues that separate one rounding from three.
    const int32_t r1[3] = {1, 3, 5}, r2[3] = {0x8000, 0x8000, 0x8000};
    diff(dut, OP_DOT2, 0, r1, r2, 0, "DOT2: residues below bit 16");
    diff(dut, OP_DOT3, 0, r1, r2, 0, "DOT3: residues below bit 16");
    // DOT3 reads three registers; DOT2 must read only two. If it read the third
    // it would give DOT3's answer here.
    const int32_t third[3] = {0, 0, INT32_MAX};
    diff(dut, OP_DOT2, 0, third, big, 0, "DOT2 does NOT read the third lane");

    // AND THE SECOND LANE MUST BE b1, NOT b0. Every DOT2 case above happens to
    // use a b vector whose first two elements are EQUAL -- {1.0,1.0,1.0},
    // {MAX,MAX,MAX}, {0.5,0.5,0.5} -- so a block computing a0*b0 + a1*b0
    // instead of a0*b0 + a1*b1 gives the identical answer to all of them. The
    // hole is visible by inspection and does not depend on any sweep: b0 and
    // b1 have to DIFFER for the term to be pinned.
    const int32_t asym_a[3] = {0x10000, 0x20000, 0};
    const int32_t asym_b[3] = {0x30000, 0x50000, 0};
    diff(dut, OP_DOT2, 0, asym_a, asym_b, 0, "DOT2 reads b1 for the second term");
    const int32_t asym_b2[3] = {0x10000, INT32_MIN, 0};
    diff(dut, OP_DOT2, 0, asym_a, asym_b2, 0, "DOT2's second term at a rail");
    // the same for DOT3's third term, which no case above separates either
    const int32_t asym3_a[3] = {0x10000, 0x20000, 0x30000};
    const int32_t asym3_b[3] = {0x40000, 0x50000, 0x60000};
    diff(dut, OP_DOT3, 0, asym3_a, asym3_b, 0, "DOT3 reads b2 for the third term");
  }

  // ---- 9. the unimplemented ops REFUSE, they do not answer -----------------
  // An ALU that returned zero for RCP or SIN would let a sequencer run a program
  // it cannot evaluate, and the capture would look healthy.
  {
    const int32_t a[3] = {1, 2, 3}, b[3] = {4, 5, 6};
    for (uint8_t op : {0x0D, 0x0E, 0x0F, 0x12, 0x13, 0x14, 0x20, 0x40, 0xFF}) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "op 0x%02X is refused, not answered", op);
      const Res r = run(dut, op, 0, a, b, 7);
      check(r.unsupported, tag, 1, r.unsupported ? 1 : 0);
      check(!r.writes, "and it writes no register", 0, r.writes ? 1 : 0);
    }
  }

  dut.final();
  return zhao::report_and_exit("field_alu_ops");
}
