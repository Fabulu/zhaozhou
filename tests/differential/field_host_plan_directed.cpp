// field_host_plan_directed.cpp -- directed cases for the production lowerer
// (reference/include/zfield/zfield_host_plan.hpp), packet L1.
//
// FT005-FT010  every opcode shape the directive section 5.3 enumerates, EACH
//              WITH A DELIBERATE MISWIRE CONTROL. A shape that maps correctly
//              proves nothing on its own: the old test-local translator mapped
//              ROT2 "successfully" and wired the angle to the wrong port. So
//              every shape owes a case that must FAIL, and the failure string
//              is checked, not just the boolean.
// FT011        the ordinal/window translation on a real program -- plan
//              section 2's worked example, register for register.
// FT012        the register high-water mark (directive 5.2 / S07).
// FT013        the generated table covers every canonical opcode (the guard
//              that did not exist while three tables described opcode shape).
// FT015        byte-identical capsules over two clean runs, hashes quoted.
// FT030        R111's gate: required_mask == 0 with a nonempty declared output
//              list is REFUSED, and the refusal is SEEN TO FIRE.
// FT034        an INIT_PROOF whose read is absent from both the preload and
//              point sets is REJECTED, with the positive control separate.
//
// RULE OBSERVED THROUGHOUT (CLAUDE.md): no case asserts the bug. Every case
// asserts the CORRECT behaviour; the controls are separate and are required to
// fire.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "crater_ring.hpp"  // TS-generated (compiler/tests/generated)

#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield.hpp"
#include "zfield/zfield_host_plan.hpp"
#include "zfield/zfield_plan.hpp"

namespace hp = zfield::host_plan;

namespace {

int g_checks = 0;
int g_fail = 0;
const char* g_case = "";

void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("  FAIL [%s] %s\n", g_case, what);
  }
}

void check_eq(long got, long want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fail;
    std::printf("  FAIL [%s] %s: expected %ld, got %ld\n", g_case, what, want, got);
  }
}

void check_str(const std::string& got, const char* want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fail;
    std::printf("  FAIL [%s] %s:\n    expected \"%s\"\n    got      \"%s\"\n", g_case, what, want,
                got.c_str());
  }
}

zfield::UopSrc V(int i) {
  zfield::UopSrc s;
  s.kind = zfield::SrcKind::kVec;
  s.idx = (uint16_t)i;
  return s;
}
zfield::UopSrc S(int i) {
  zfield::UopSrc s;
  s.kind = zfield::SrcKind::kSca;
  s.idx = (uint16_t)i;
  return s;
}

/** A uop with `n` flattened sources taken from `src`. */
zfield::VecUop uop(uint8_t op, int dst, std::initializer_list<zfield::UopSrc> src,
                   uint32_t imm = 0) {
  zfield::VecUop u{};
  u.op = op;
  u.dst = (uint8_t)dst;
  u.n_src = (uint8_t)src.size();
  int i = 0;
  for (const zfield::UopSrc& s : src) u.src[i++] = s;
  u.imm = imm;
  return u;
}

/** Map one uop through a fresh Translator. */
bool map(const zfield::VecUop& u, hp::Mapped* m, std::string* refusal, int scalar_base = 32) {
  zfield::Fplan fp;  // the Translator reads only scalar_base from the options
  hp::Translator tr(fp, scalar_base);
  const bool ok = tr.map_one(u, m);
  if (refusal) *refusal = tr.refusal();
  return ok;
}

// ======================================================================
// FT005 -- the three-scalar-operand shapes, in their canonical roles
// ======================================================================
void ft005() {
  g_case = "FT005 MAD/SELECT/CLAMP";
  // Directive 5.3: "MAD/SELECT/CLAMP: three scalar operands in their canonical
  // roles." Three groups of width 1, so a/b/c are src[0..2] and NOTHING is
  // required to be adjacent -- a width-1 group cannot be non-adjacent, and a
  // translator that demanded adjacency here would refuse legal programs.
  const int kMadOps[3] = {zfield::OP_MAD, zfield::OP_SELECT, zfield::OP_CLAMP};
  for (int i = 0; i < 3; ++i) {
    hp::Mapped m;
    std::string r;
    // deliberately NON-adjacent registers: 3, 9, 20
    check(map(uop((uint8_t)kMadOps[i], 1, {V(3), V(9), V(20)}), &m, &r), "three-operand maps");
    check_eq(m.a, 3, "operand a is src[0]");
    check_eq(m.b, 9, "operand b is src[1]");
    check_eq(m.c, 20, "operand c is src[2]");
    check_eq(m.n_groups, 3, "three groups");
    check_eq(m.dst_width, 1, "one destination register");
  }

  // MOV and ADD, the width-1 one- and two-operand shapes.
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_MOV, 2, {V(7)}), &m, &r), "MOV maps");
    check_eq(m.a, 7, "MOV reads src[0]");
    check_eq(m.n_groups, 1, "MOV has one group");
    check_eq(m.b, 0, "MOV leaves b unset");
  }
  {
    // A SCALAR operand becomes scalar_base + slot: the uniform broadcast.
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_ADD, 2, {V(4), S(5)}), &m, &r), "ADD maps");
    check_eq(m.a, 4, "ADD operand a is the vector register");
    check_eq(m.b, 32 + 5, "ADD operand b is broadcast to scalar_base + slot");
  }

  // CONTROL -- a source count that disagrees with the generated group widths
  // must be refused by name rather than truncated to fit.
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_MAD, 1, {V(3), V(9)}), &m, &r), "MAD with two sources is refused");
    check_str(r, hp::kRefusalSrcCount, "refusal names the source-count disagreement");
  }
}

// ======================================================================
// FT006 -- DOT3: A[3], B[3], one destination
// ======================================================================
void ft006() {
  g_case = "FT006 DOT3";
  {
    hp::Mapped m;
    std::string r;
    // A = R4,R5,R6   B = R10,R11,R12
    check(map(uop(zfield::OP_DOT3, 1, {V(4), V(5), V(6), V(10), V(11), V(12)}), &m, &r),
          "DOT3 with two adjacent 3-groups maps");
    check_eq(m.a, 4, "group A starts at src[0]");
    check_eq(m.b, 10, "group B starts at src[3], NOT src[1]");
    check_eq(m.n_groups, 2, "two groups");
    check_eq(m.group_width[0], 3, "group A is three wide");
    check_eq(m.group_width[1], 3, "group B is three wide");
  }
  // MISWIRE CONTROL -- the extra member of B is not consecutive. This is the
  // aliasing directive 5.3 forbids: "It may not alias neighboring registers and
  // hope the resulting numbers agree."
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_DOT3, 1, {V(4), V(5), V(6), V(10), V(11), V(31)}), &m, &r),
          "DOT3 with a broken B group is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
  // MISWIRE CONTROL -- and in group A, so the check is not only watching B.
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_DOT3, 1, {V(4), V(9), V(6), V(10), V(11), V(12)}), &m, &r),
          "DOT3 with a broken A group is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
  // DOT2, the same law one width down.
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_DOT2, 1, {V(4), V(5), V(8), V(9)}), &m, &r), "DOT2 maps");
    check_eq(m.a, 4, "DOT2 group A");
    check_eq(m.b, 8, "DOT2 group B starts at src[2]");
  }
}

// ======================================================================
// FT007 -- LEN3 / NORMALIZE3: A[3], NO INVENTED B GROUP
// ======================================================================
void ft007() {
  g_case = "FT007 LEN3/NORMALIZE3";
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_LEN3, 1, {V(20), V(21), V(22)}), &m, &r), "LEN3 maps");
    check_eq(m.a, 20, "LEN3 reads one 3-group");
    check_eq(m.n_groups, 1, "LEN3 has ONE group -- no invented B");
    check_eq(m.b, 0, "LEN3 leaves operand b unset");
    check_eq(m.dst_width, 1, "LEN3 writes one register");
  }
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_NORMALIZE3, 8, {V(20), V(21), V(22)}), &m, &r), "NORMALIZE3 maps");
    check_eq(m.n_groups, 1, "NORMALIZE3 has one group");
    // The destination is a GROUP of three. The defined-set walk must add all
    // three, or a later read of dst+1 or dst+2 would look undefined.
    check_eq(m.dst_width, 3, "NORMALIZE3 writes THREE registers");
  }
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_LEN2, 1, {V(20), V(21)}), &m, &r), "LEN2 maps");
    check_eq(m.a, 20, "LEN2 reads one 2-group");
    check_eq(m.n_groups, 1, "LEN2 has one group");
  }
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_NORMALIZE2, 8, {V(20), V(21)}), &m, &r), "NORMALIZE2 maps");
    check_eq(m.dst_width, 2, "NORMALIZE2 writes TWO registers");
  }
  // MISWIRE CONTROL
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_LEN3, 1, {V(20), V(21), V(30)}), &m, &r),
          "LEN3 with a broken group is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_NORMALIZE3, 8, {V(20), V(25), V(22)}), &m, &r),
          "NORMALIZE3 with a broken group is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
}

// ======================================================================
// FT008 -- DIST2: A[2], B[2], NOT A[3]+B[1]
// ======================================================================
void ft008() {
  g_case = "FT008 DIST2";
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_DIST2, 1, {V(4), V(5), V(12), V(13)}), &m, &r), "DIST2 maps");
    check_eq(m.a, 4, "DIST2 group A starts at src[0]");
    // The directive names the wrong reading explicitly. A[3]+B[1] would put
    // operand b at src[3] = R13; the correct A[2]+B[2] puts it at src[2] = R12.
    check_eq(m.b, 12, "DIST2 group B starts at src[2], not src[3]");
    check_eq(m.group_width[0], 2, "A is two wide");
    check_eq(m.group_width[1], 2, "B is two wide");
  }
  // MISWIRE CONTROL -- exactly the A[3]+B[1] misreading, expressed as operands
  // that are adjacent under that reading and NOT under the correct one.
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_DIST2, 1, {V(4), V(5), V(6), V(20)}), &m, &r),
          "DIST2 whose members are only adjacent under the A[3]+B[1] misreading is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
}

// ======================================================================
// FT009 -- ROT2: A[2] and angle B, NOT the third flattened source as C
// ======================================================================
void ft009() {
  g_case = "FT009 ROT2";
  // THE CASE THAT DISCRIMINATES. ROT2's flattened sources are {a0, a1, angle}.
  // A translator that infers "width 1, three sources" -- which is what a
  // default-to-1 width helper does -- produces a=a0, b=a1, c=angle. The correct
  // shape {2,1} produces a=a0, b=angle, c unset.
  //
  // Both readings "succeed"; they differ only in WHICH PORT the angle reaches.
  // That is why this case asserts the port and not the boolean.
  hp::Mapped m;
  std::string r;
  check(map(uop(zfield::OP_ROT2, 8, {V(4), V(5), V(17)}), &m, &r), "ROT2 maps");
  check_eq(m.a, 4, "ROT2 group A starts at src[0]");
  check_eq(m.b, 17, "operand b is THE ANGLE (src[2])");
  check(m.b != 5, "operand b is NOT the second member of group A");
  check_eq(m.c, 0, "operand c is unset -- ROT2 has two groups, not three");
  check_eq(m.n_groups, 2, "ROT2 has two groups");
  check_eq(m.group_width[0], 2, "A is two wide");
  check_eq(m.group_width[1], 1, "the angle group is one wide");
  check_eq(m.dst_width, 2, "ROT2 writes TWO registers");

  // MISWIRE CONTROL -- group A not consecutive.
  {
    hp::Mapped m2;
    std::string r2;
    check(!map(uop(zfield::OP_ROT2, 8, {V(4), V(11), V(17)}), &m2, &r2),
          "ROT2 with a broken A group is REFUSED");
    check_str(r2, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
  // CONTROL -- a uniform angle still lands in b, broadcast above the vectors.
  {
    hp::Mapped m3;
    std::string r3;
    check(map(uop(zfield::OP_ROT2, 8, {V(4), V(5), S(2)}), &m3, &r3), "ROT2 with a uniform angle");
    check_eq(m3.b, 32 + 2, "the uniform angle is broadcast to scalar_base + slot");
  }
}

// ======================================================================
// FT010 -- ROT3: A[3] and angle B plus the axis IMMEDIATE
// ======================================================================
void ft010() {
  g_case = "FT010 ROT3";
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::OP_ROT3, 8, {V(4), V(5), V(6), V(19)}, 2u), &m, &r), "ROT3 maps");
    check_eq(m.a, 4, "ROT3 group A starts at src[0]");
    check_eq(m.b, 19, "operand b is the angle (src[3])");
    check_eq(m.c, 0, "the axis is an IMMEDIATE, not a third operand group");
    check_eq((long)m.imm, 2, "the axis immediate is carried through unchanged");
    check_eq(m.dst_width, 3, "ROT3 writes THREE registers");
    check_eq(m.n_groups, 2, "ROT3 has two groups");
  }
  // MISWIRE CONTROL
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::OP_ROT3, 8, {V(4), V(5), V(30), V(19)}, 2u), &m, &r),
          "ROT3 with a broken A group is REFUSED");
    check_str(r, hp::kRefusalNonConsecutive, "refusal names the non-consecutive group");
  }
  // CONTROL -- a byte that is not a canonical opcode is refused BY NAME rather
  // than guessed at.
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(0x77, 1, {V(1)}), &m, &r), "a non-canonical opcode is REFUSED");
    check_str(r, hp::kRefusalUnknownShape, "refusal names the unknown shape");
  }
  // RING_PREP, the synthetic contraction target, with its own controls.
  {
    hp::Mapped m;
    std::string r;
    check(map(uop(zfield::UOP_RING_PREP, 9, {V(3), S(1), S(1), S(2), S(3)},
                  hp::kRingPrepSmoothBit),
              &m, &r),
          "RING_PREP maps");
    check_eq(m.a, 3, "RING_PREP reads the distance from a vector register");
    // Four six-bit slot indices in bits 0..23, smooth mode in bit 24.
    check_eq((long)(m.imm & 0x3F), 1, "slot 0 in bits 0..5");
    check_eq((long)((m.imm >> 6) & 0x3F), 1, "slot 1 in bits 6..11");
    check_eq((long)((m.imm >> 12) & 0x3F), 2, "slot 2 in bits 12..17");
    check_eq((long)((m.imm >> 18) & 0x3F), 3, "slot 3 in bits 18..23");
    check(m.imm & hp::kRingPrepSmoothBit, "smooth mode survives in bit 24");
  }
  {
    hp::Mapped m;
    std::string r;
    check(!map(uop(zfield::UOP_RING_PREP, 9, {V(3), V(1), S(1), S(2), S(3)}), &m, &r),
          "RING_PREP with a VARYING radius is REFUSED");
    check_str(r, hp::kRefusalRingPrepVarying, "refusal names the non-uniform radius");
  }
}

// ======================================================================
// FT013 -- the generated table covers every canonical opcode
// ======================================================================
void ft013() {
  g_case = "FT013 one table";
  // While three descriptions of opcode shape existed, nothing asserted that
  // any of them was COMPLETE. zfield_decode.cpp's opMeta() now delegates here
  // (R127), so this is the gate for all of them at once.
  const uint8_t all[] = {zfield::OP_END,        zfield::OP_MOV,    zfield::OP_LDC,
                         zfield::OP_ADD,        zfield::OP_SUB,    zfield::OP_MUL,
                         zfield::OP_MAD,        zfield::OP_MIN,    zfield::OP_MAX,
                         zfield::OP_ABS,        zfield::OP_CLAMP,  zfield::OP_SELECT,
                         zfield::OP_CMP,        zfield::OP_DOT2,   zfield::OP_DOT3,
                         zfield::OP_LEN2,       zfield::OP_LEN3,   zfield::OP_DIST2,
                         zfield::OP_NORMALIZE2, zfield::OP_NORMALIZE3, zfield::OP_RCP,
                         zfield::OP_SIN,        zfield::OP_COS,    zfield::OP_CURVE,
                         zfield::OP_SPLINE,     zfield::OP_NOISE2, zfield::OP_DCURVE,
                         zfield::OP_RING,       zfield::OP_RIDGE,  zfield::OP_ROT2,
                         zfield::OP_ROT3};
  check_eq((long)(sizeof(all) / sizeof(all[0])), zfield::optable::OP_COUNT,
           "the enum and the generated table hold the same number of opcodes");
  for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    const zfield::optable::OpShape* sh = zfield::optable::shape_of(all[i]);
    ++g_checks;
    if (sh == nullptr) {
      ++g_fail;
      std::printf("  FAIL [%s] opcode 0x%02X has no generated shape\n", g_case, all[i]);
      continue;
    }
    int flat = 0;
    for (int g = 0; g < sh->n_groups; ++g) flat += sh->group_width[g];
    check_eq(flat, sh->n_src, "the flattened source count equals the sum of the group widths");
  }
  // CONTROL -- a non-canonical byte must NOT resolve.
  check(zfield::optable::shape_of(0x77) == nullptr, "a reserved byte has no shape");
  check(zfield::optable::shape_of(0x1E) == nullptr, "an unassigned code has no shape");
}

// ======================================================================
// The real program: crater_ring
// ======================================================================

struct Real {
  zfield::Decoded prog;
  zfield::Fplan fp;
  zfield::Prepared prep;
  bool ok = false;
};

Real load_crater_ring() {
  Real r;
  zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                           zfield_gen::crater_ring::kProgramBytesLen);
  if (dr.error != zfield::DecodeError::kOk) {
    std::printf("  FAIL crater_ring did not decode: %s (%s)\n", zfield::decodeErrorName(dr.error),
                dr.detail.c_str());
    ++g_fail;
    return r;
  }
  r.prog = dr.prog;
  r.fp = zfield::plan(r.prog, 0x3);  // Earth: x and z vary
  std::vector<int32_t> in(r.prog.in_lanes.size(), 0);
  r.prep = zfield::prepare(r.fp, r.prog, in.data(), in.size());
  r.ok = true;
  return r;
}

/** MEASURED, not assumed: 3. See ft011's chapter on the two numberings --
 *  R111's out_base of 13 is a CANONICAL register number and this is a LOWERED
 *  one, and quietly using the first here is the exact confusion this packet
 *  exists to make impossible. */
inline constexpr int kCraterLoweredOutBase = 3;

hp::LowerOptions crater_opts() {
  hp::LowerOptions o;
  o.scalar_base = hp::kDefaultScalarBase;
  o.out_base = kCraterLoweredOutBase;
  o.out_lanes = 7;  // the composed OUT_LANES
  o.register_ceiling = hp::kPhysicalRegisters;
  return o;
}

// ======================================================================
// FT011 -- the ordinal/window translation, on the program R111 measured
// ======================================================================
void ft011(const Real& R) {
  g_case = "FT011 ordinal<->window";
  if (!R.ok) return;

  // ==================================================================
  // THERE ARE TWO REGISTER NUMBERINGS AND R111'S EXAMPLE IS IN THE
  // FIRST ONE. Getting this wrong is the packet's own subject matter.
  // ==================================================================
  //
  // (a) CANONICAL numbering -- the registers the .zprog itself names. This is
  //     what tools/field/zprog_output_coverage.py reads and what R111's
  //     "0x17 / 0x1D / 0x17" describes. crater_ring: height R14, velocity R15,
  //     material R17, nav_cost R13, so out_base 13 and window mask 0x17.
  //
  // (b) LOWERED numbering -- the physical registers zfield::plan assigns after
  //     the uniform/varying split. Completely different, and SMALLER: the
  //     planner compacts to n_vreg=7 because most of the canonical file is
  //     uniform and moves to the scalar bank.
  //
  // Asserting (a)'s numbers against (b)'s image is precisely the category
  // error the two mask TYPES exist to prevent, one level up -- the types stop
  // an ordinal being read as a window position, and nothing stops a CANONICAL
  // register being read as a LOWERED one. So both are checked, separately,
  // and each is named where it is used.

  // ---- (a) R111's measurement, reproduced in its own numbering ----------
  check_eq((long)R.prog.out_lanes.size(), 4, "crater_ring declares four canonical outputs");
  int canon_min = 255;
  for (const zfield::IoLane& l : R.prog.out_lanes)
    if ((int)l.reg < canon_min) canon_min = (int)l.reg;
  check_eq(canon_min, 13, "the canonical out_base is R13, as R111 measured");
  uint32_t canon_window = 0;
  for (const zfield::IoLane& l : R.prog.out_lanes) {
    const int k = (int)l.reg - canon_min;
    check(k >= 0 && k < 7, "every canonical output sits inside a 7-lane window");
    if (k >= 0 && k < 7) canon_window |= 1u << k;
  }
  check_eq((long)canon_window, 0x17,
           "the CANONICAL window mask is 0x17 -- R111's number, independently reproduced");

  // ---- (b) the lowered image, which is the thing that ships -------------
  hp::HostPlan p;
  std::string refusal;
  check(hp::lower(R.fp, R.prog, &R.prep, crater_opts(), &p, &refusal), "crater_ring lowers");
  if (!refusal.empty()) {
    std::printf("  note: refusal was \"%s\"\n", refusal.c_str());
    return;
  }
  check_eq(p.output_count, 4, "the lowered image declares four outputs");
  check_eq((long)p.required_mask.bits(), 0x0F, "the ORDINAL mask is 0x0F -- four declared outputs");

  // ==================================================================
  // THE FINDING THIS CASE EXISTS TO PIN: TWO OF CRATER_RING'S FOUR
  // OUTPUTS ARE PREPARED SCALARS, AND NO WINDOW MASK CAN SEE THEM.
  // ==================================================================
  //
  // velocity (ordinal 1) and material (ordinal 2) are UNIFORM across the whole
  // association -- the planner resolves them in the prepare block and they
  // never touch a vector register. Directive 7.3 is explicit that such an
  // ordinal is seeded at point start and "Do not wait for a vector write which
  // an all-uniform program will never generate."
  //
  // So on a SHIPPED Earth program, a completion rule built on the window mask
  // alone can observe at most two of the four declared outputs. It is not a
  // question of holes in the window; those two ordinals have NO WINDOW
  // POSITION AT ALL. That is plan section 2 case 1 with a measured instance.
  int vec_outs = 0, sca_outs = 0;
  for (const hp::OutputSource& o : p.output_map) {
    if (o.source_kind == zfield::host_image::ZFH_SOURCE_KIND_VECTOR_REG) {
      ++vec_outs;
      // Every VECTOR_REG ordinal must be inside the capture window, or it
      // could never be observed and the point would hang.
      check(o.source_index >= p.out_base && o.source_index < p.out_base + p.out_lanes,
            "a vector output register lies inside the capture window");
    } else {
      ++sca_outs;
    }
  }
  check_eq(vec_outs, 2, "two ordinals are VECTOR_REG");
  check_eq(sca_outs, 2, "two ordinals are PREPARED_SCALAR -- they have no window position");

  // The window mask therefore carries TWO bits against FOUR declared outputs.
  check_eq((long)p.window_mask().bits(), 0x03, "the WINDOW mask is 0x03");
  check(p.required_mask.bits() != p.window_mask().bits(),
        "the two masks differ on a real program -- holes are the NORMAL case");
  std::printf(
      "  measured: crater_ring canonical window 0x%02X (out_base 13) | lowered ordinal 0x%02X, "
      "lowered window 0x%02X (out_base %d), %d VECTOR_REG + %d PREPARED_SCALAR\n",
      (unsigned)canon_window, (unsigned)p.required_mask.bits(), (unsigned)p.window_mask().bits(),
      p.out_base, vec_outs, sca_outs);

  // A prepared-scalar ordinal must NOT be refused for lying outside the
  // window. Refusing it would break the all-uniform form the directive
  // requires to work -- the guard has to know the difference, and this is the
  // case that proves it does.
  check(hp::verify_output_contract(p, &refusal),
        "prepared-scalar ordinals are NOT refused for having no window position");

  // CONTROL -- move the window off the VECTOR outputs and the image must be
  // REFUSED, not silently emitted with an ordinal nothing can ever set.
  {
    hp::LowerOptions bad = crater_opts();
    bad.out_base = 40;
    hp::HostPlan p2;
    std::string r2;
    check(!hp::lower(R.fp, R.prog, &R.prep, bad, &p2, &r2),
          "a vector output register outside the capture window is REFUSED");
    check_str(r2, hp::kRefusalOutsideWindow, "refusal names BAD_IMAGE");
  }
  // CONTROL, the other polarity -- a window that only just covers them passes,
  // so the refusal above is discriminating on the window and not on something
  // incidental.
  {
    hp::LowerOptions tight = crater_opts();
    tight.out_base = 3;
    tight.out_lanes = 2;
    hp::HostPlan p3;
    std::string r3;
    check(hp::lower(R.fp, R.prog, &R.prep, tight, &p3, &r3),
          "a 2-lane window that exactly covers the vector outputs passes");
    check_eq((long)p3.window_mask().bits(), 0x03, "and its window mask is still 0x03");
  }
}

// ======================================================================
// FT012 -- the register high-water mark (directive 5.2)
// ======================================================================
void ft012(const Real& R) {
  g_case = "FT012 high-water";
  if (!R.ok) return;
  hp::HostPlan p;
  std::string refusal;
  if (!hp::lower(R.fp, R.prog, &R.prep, crater_opts(), &p, &refusal)) return;

  // "It must check the final register high-water mark, not assume n_vreg alone
  // is the RF requirement." The broadcast uniform region sits ABOVE the vector
  // registers, so the true requirement is scalar_base + n_scalar, which n_vreg
  // cannot see.
  check(p.register_high_water > (int)R.fp.n_vreg,
        "the high-water mark exceeds n_vreg -- n_vreg alone understates the RF");
  std::printf("  measured: n_vreg=%u n_scalar=%u scalar_base=%d high_water=%d\n",
              (unsigned)R.fp.n_vreg, (unsigned)R.fp.n_scalar, p.scalar_base,
              p.register_high_water);

  // CONTROL, and it is directive S07's claim MEASURED rather than repeated:
  // the console composes REGS=32 (zhao_console_core.sv:15662) and this real
  // program does not fit it. FH22 asks for REGS=64; plan contradiction C1 says
  // the sequencing needs ratification. This check reports the fact; it does not
  // decide the contradiction.
  {
    hp::LowerOptions tight = crater_opts();
    tight.register_ceiling = 32;
    hp::HostPlan p2;
    std::string r2;
    check(!hp::lower(R.fp, R.prog, &R.prep, tight, &p2, &r2),
          "crater_ring does NOT fit a 32-register file at this scalar_base");
    check_str(r2, hp::kRefusalRegisterRange, "refusal names the register range");
  }
}

// ======================================================================
// FT030 -- R111's gate, SHOWN FAILING
// ======================================================================
void ft030(const Real& R) {
  g_case = "FT030 required_mask==0";
  if (!R.ok) return;
  hp::HostPlan p;
  std::string refusal;
  check(hp::lower(R.fp, R.prog, &R.prep, crater_opts(), &p, &refusal),
        "the POSITIVE control: a descriptor with a real mask passes");
  check(!p.required_mask.empty(), "the packer emitted a nonempty required_mask");

  // THE GATE. Nothing in the tree emits a header word today, so mask == 0 is
  // the only case that occurs -- which means a plan writer who simply omits it
  // restores the R101 defect and passes every other gate in the set. Assert the
  // REFUSAL, not the defect.
  hp::HostPlan bad = p;
  bad.required_mask = hp::RequiredMask(0);
  std::string r2;
  check(!hp::verify_output_contract(bad, &r2),
        "a descriptor with required_mask == 0 and declared outputs is REFUSED");
  check_str(r2, hp::kRefusalEmptyRequiredMask, "refusal names the empty required mask");

  // And the refusal must survive to serialisation -- a check that only runs on
  // the constructing path cannot see a descriptor edited afterwards.
  std::vector<uint8_t> img;
  std::string r3;
  check(!hp::serialize_program_image(bad, zfield_gen::crater_ring::kProgramBytes.data(),
                                     zfield_gen::crater_ring::kProgramBytesLen, &img, &r3),
        "serialisation REFUSES the same descriptor");
  check_str(r3, hp::kRefusalEmptyRequiredMask, "the serialiser's refusal names the same cause");
  check_eq((long)img.size(), 0, "no bytes were produced for a refused descriptor");

  // THE SEPARATE POSITIVE CONTROL, so the refusal is not merely "it always
  // says no": restore the mask and the identical descriptor passes.
  hp::HostPlan good = bad;
  good.required_mask = p.required_mask;
  std::string r4;
  check(hp::verify_output_contract(good, &r4), "restoring the mask makes the SAME descriptor pass");
  check(r4.empty(), "and it passes with no refusal text");
}

// ======================================================================
// FT034 -- the INIT_PROOF defined-set walk
// ======================================================================
void ft034() {
  g_case = "FT034 init proof";
  // A minimal image whose single uop reads R5. R5 is in NEITHER the
  // association preload set nor the point write set, so the symbolic walk of
  // directive 6.2 must reject it. This is the read-before-definition that
  // E_ZERO used to mask by clearing the whole file every point.
  hp::HostPlan p;
  p.out_base = 0;
  p.out_lanes = 7;
  p.output_count = 1;
  {
    hp::OutputSource o;
    o.ordinal = 0;
    o.source_kind = zfield::host_image::ZFH_SOURCE_KIND_VECTOR_REG;
    o.source_index = 1;
    o.required = true;
    p.output_map.push_back(o);
  }
  p.required_mask = hp::RequiredMask(0x1);

  hp::Mapped m;
  m.op = zfield::OP_MOV;
  m.dst = 1;
  m.dst_width = 1;
  m.a = 5;
  m.n_groups = 1;
  m.group_width[0] = 1;
  p.uops.push_back(m);

  // Nothing defines R5.
  p.association_register_mask = 0;
  p.point_register_mask = 0;
  p.initial_defined_mask = 0;
  p.vector_write_mask = (uint64_t)1 << 1;

  std::string r;
  check(!hp::verify_init_proof(p, &r), "a read with no definition is REJECTED");
  check_str(r, hp::kRefusalReadBeforeDef, "refusal names the read before definition");

  // THE POSITIVE CONTROL -- the SAME program with the preload added passes.
  // Separate from the refusal, so "it always says no" is excluded.
  hp::HostPlan q = p;
  q.association_register_mask = (uint64_t)1 << 5;
  q.initial_defined_mask = q.association_register_mask | q.point_register_mask;
  std::string r2;
  check(hp::verify_init_proof(q, &r2), "the same program WITH the preload passes");
  check(r2.empty(), "and it passes with no refusal text");

  // The other direction: supplying R5 as a per-point varying write also
  // satisfies the proof, because the point's accepted writes are part of the
  // starting set too.
  hp::HostPlan s = p;
  s.point_register_mask = (uint64_t)1 << 5;
  s.initial_defined_mask = s.association_register_mask | s.point_register_mask;
  std::string r3;
  check(hp::verify_init_proof(s, &r3), "a per-point write also defines the register");

  // A read of a register defined by an EARLIER uop is legal -- the walk adds
  // destinations as it goes, which is what makes it a walk and not a set test.
  hp::HostPlan t = q;
  hp::Mapped m2;
  m2.op = zfield::OP_MOV;
  m2.dst = 2;
  m2.dst_width = 1;
  m2.a = 1;  // written by the first uop
  m2.n_groups = 1;
  m2.group_width[0] = 1;
  t.uops.push_back(m2);
  t.vector_write_mask |= (uint64_t)1 << 2;
  std::string r4;
  check(hp::verify_init_proof(t, &r4), "a read of a register an earlier uop defined passes");

  // CONTROL -- reversing the two uops makes the same pair ILLEGAL, which is
  // what proves the walk is ordered rather than a union.
  hp::HostPlan u = q;
  u.uops.clear();
  u.uops.push_back(m2);  // reads R1 before anything writes it
  u.uops.push_back(m);
  u.vector_write_mask |= (uint64_t)1 << 2;
  std::string r5;
  check(!hp::verify_init_proof(u, &r5), "the SAME two uops in the wrong ORDER are rejected");
  check_str(r5, hp::kRefusalReadBeforeDef, "refusal names the read before definition");

  // A DOT3 read must check all three members, not just the group start.
  hp::HostPlan w;
  w.out_base = 0;
  w.out_lanes = 7;
  w.output_count = 1;
  {
    hp::OutputSource o;
    o.ordinal = 0;
    o.source_kind = zfield::host_image::ZFH_SOURCE_KIND_VECTOR_REG;
    o.source_index = 1;
    o.required = true;
    w.output_map.push_back(o);
  }
  w.required_mask = hp::RequiredMask(0x1);
  hp::Mapped d;
  d.op = zfield::OP_DOT3;
  d.dst = 1;
  d.dst_width = 1;
  d.a = 4;
  d.b = 10;
  d.n_groups = 2;
  d.group_width[0] = 3;
  d.group_width[1] = 3;
  w.uops.push_back(d);
  // R4,R5,R6 and R10,R11 defined -- R12 is NOT.
  w.association_register_mask = 0;
  w.point_register_mask = ((uint64_t)0x7 << 4) | ((uint64_t)0x3 << 10);
  w.initial_defined_mask = w.point_register_mask;
  w.vector_write_mask = (uint64_t)1 << 1;
  std::string r6;
  check(!hp::verify_init_proof(w, &r6),
        "a DOT3 whose THIRD B member is undefined is rejected -- the walk reads the whole group");
  check_str(r6, hp::kRefusalReadBeforeDef, "refusal names the read before definition");

  hp::HostPlan x = w;
  x.point_register_mask |= (uint64_t)1 << 12;
  x.initial_defined_mask = x.point_register_mask;
  std::string r7;
  check(hp::verify_init_proof(x, &r7), "defining the third member makes the same DOT3 pass");

  // And the immutable/vector-write disjointness of directive 11.3.
  hp::HostPlan y = q;
  y.immutable_register_mask = y.vector_write_mask;
  std::string r8;
  check(!hp::verify_init_proof(y, &r8),
        "an immutable association register that is also a vector write target is rejected");
}

// ======================================================================
// FT015 -- byte-identical capsules over two clean runs
// ======================================================================
void ft015(const Real& R) {
  g_case = "FT015 determinism";
  if (!R.ok) return;

  // Two INDEPENDENT lowerings from the original bytes -- not one lowering
  // serialised twice, which would only prove memcpy is deterministic.
  std::vector<uint8_t> a, b;
  for (int pass = 0; pass < 2; ++pass) {
    zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                             zfield_gen::crater_ring::kProgramBytesLen);
    check(dr.error == zfield::DecodeError::kOk, "the program decodes on both passes");
    if (dr.error != zfield::DecodeError::kOk) return;
    zfield::Fplan fp = zfield::plan(dr.prog, 0x3);
    std::vector<int32_t> in(dr.prog.in_lanes.size(), 0);
    zfield::Prepared prep = zfield::prepare(fp, dr.prog, in.data(), in.size());
    hp::HostPlan p;
    std::string refusal;
    if (!hp::lower(fp, dr.prog, &prep, crater_opts(), &p, &refusal)) {
      std::printf("  FAIL [%s] lowering refused: %s\n", g_case, refusal.c_str());
      ++g_fail;
      return;
    }
    std::vector<uint8_t>& dst = pass == 0 ? a : b;
    if (!hp::serialize_program_image(p, zfield_gen::crater_ring::kProgramBytes.data(),
                                     zfield_gen::crater_ring::kProgramBytesLen, &dst, &refusal)) {
      std::printf("  FAIL [%s] serialisation refused: %s\n", g_case, refusal.c_str());
      ++g_fail;
      return;
    }
  }
  check_eq((long)a.size(), (long)b.size(), "both capsules are the same length");
  check(a == b, "the two capsules are BYTE-IDENTICAL");
  const uint32_t ha = hp::crc32c(a.data(), a.size());
  const uint32_t hbv = hp::crc32c(b.data(), b.size());
  check_eq((long)ha, (long)hbv, "the two capsule digests agree");
  std::printf("  FT015 capsule: %zu bytes, crc32c=0x%08X (both passes)\n", a.size(), ha);

  // The envelope itself, checked rather than trusted.
  check(a.size() >= 64, "the capsule carries at least a header");
  check_eq(a[0], 'Z', "magic[0]");
  check_eq(a[1], 'F', "magic[1]");
  check_eq(a[2], 'H', "magic[2]");
  check_eq(a[3], '2', "magic[3]");
  check_eq((long)(a.size() % zfield::host_image::ZFH_ALIGNMENT), 0,
           "total_bytes is 64-byte aligned");
  uint32_t total = 0;
  std::memcpy(&total, &a[zfield::host_image::ZFH_HDR_OFF_TOTAL_BYTES], 4);
  check_eq((long)total, (long)a.size(), "total_bytes equals the real length");

  // The body CRC is over the whole image with bytes 12..15 zero. Recompute it
  // the way a reader would and require agreement -- otherwise the field is
  // decoration.
  if (a.size() >= zfield::host_image::ZFH_HDR_BYTES) {
    uint32_t stored = 0;
    std::memcpy(&stored, &a[zfield::host_image::ZFH_HDR_OFF_BODY_CRC32C], 4);
    std::vector<uint8_t> z = a;
    for (int i = 0; i < 4; ++i) z[zfield::host_image::ZFH_HDR_OFF_BODY_CRC32C + i] = 0;
    check_eq((long)stored, (long)hp::crc32c(z.data(), z.size()),
             "body_crc32c verifies with bytes 12..15 zeroed");
  }

  // CONTROL -- the digest must MOVE when the image does, or it is not watching.
  // A determinism claim backed by a digest that cannot change is worthless.
  {
    hp::LowerOptions o2 = crater_opts();
    o2.object_serial = 0xA5A5A5A5u;
    zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                             zfield_gen::crater_ring::kProgramBytesLen);
    zfield::Fplan fp = zfield::plan(dr.prog, 0x3);
    std::vector<int32_t> in(dr.prog.in_lanes.size(), 0);
    zfield::Prepared prep = zfield::prepare(fp, dr.prog, in.data(), in.size());
    hp::HostPlan p;
    std::string refusal;
    std::vector<uint8_t> c;
    if (hp::lower(fp, dr.prog, &prep, o2, &p, &refusal) &&
        hp::serialize_program_image(p, zfield_gen::crater_ring::kProgramBytes.data(),
                                    zfield_gen::crater_ring::kProgramBytesLen, &c, &refusal)) {
      check(c != a, "changing one header field CHANGES the capsule");
      check(hp::crc32c(c.data(), c.size()) != ha, "and changes its digest");
    } else {
      check(false, "the digest-moves control lowered and serialised");
    }
  }
}

// ======================================================================
// The contraction (directive 5.4) -- present and honest about its effect
// ======================================================================
void ft016(const Real& R) {
  g_case = "FT016 contraction";
  if (!R.ok) return;
  int saved = 0;
  const std::vector<zfield::VecUop> c = hp::contract_smoothstep(R.fp, &saved);
  check_eq((long)(R.fp.uops.size() - c.size()), (long)saved,
           "the reported saving equals the uops actually removed");
  check(c.size() <= R.fp.uops.size(), "the contraction never lengthens the stream");
  std::printf("  measured: crater_ring %zu uops -> %zu contracted (saved %d)\n", R.fp.uops.size(),
              c.size(), saved);
  // Every contracted uop must still map. A contraction that produces a shape
  // the translator refuses would be a rewrite that cannot be installed.
  for (const zfield::VecUop& u : c) {
    hp::Mapped m;
    std::string r;
    if (!map(u, &m, &r)) {
      ++g_checks;
      ++g_fail;
      std::printf("  FAIL [%s] contracted uop op=0x%02X refused: %s\n", g_case, u.op, r.c_str());
    }
  }
  // CONTROL -- with the contraction ON, the image still lowers and still
  // carries the same output contract.
  hp::LowerOptions o = crater_opts();
  o.contract = true;
  hp::HostPlan p;
  std::string refusal;
  check(hp::lower(R.fp, R.prog, &R.prep, o, &p, &refusal), "the contracted image lowers");
  if (refusal.empty()) {
    check_eq((long)p.required_mask.bits(), 0x0F, "the ordinal mask is unchanged by contraction");
    check_eq((long)p.window_mask().bits(), 0x03, "the window mask is unchanged by contraction");
    check_eq((long)p.contraction_saved, (long)saved, "the image records the saving");
  }
}

}  // namespace

int main() {
  std::printf("field_host_plan_directed -- the production lowerer (packet L1)\n");

  ft005();
  ft006();
  ft007();
  ft008();
  ft009();
  ft010();
  ft013();

  const Real R = load_crater_ring();
  ft011(R);
  ft012(R);
  ft030(R);
  ft034();
  ft015(R);
  ft016(R);

  std::printf("\n%d checks, %d failures\n", g_checks, g_fail);
  if (g_fail != 0) {
    std::printf("FAIL\n");
    return 1;
  }
  std::printf("PASS\n");
  return 0;
}
