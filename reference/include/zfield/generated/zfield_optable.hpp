// GENERATED FILE - compiler/src/field_ir/gen_optable.ts - DO NOT EDIT.
// The canonical Field IR operation table (spec/form/field-ir.md section 2)
// as consumed by the C++ v3 planner. Regenerate:
//   cd compiler && npm run build && node dist/src/field_ir/gen_optable.js --write
// and commit. The static_asserts below pin every code to the zfield::Op enum:
// a second numbering CANNOT exist on the C++ side without failing to compile.
#pragma once

#include <cstdint>

#include "zfield/zfield.hpp"

namespace zfield {
namespace optable {

inline constexpr uint8_t IMM_NONE = 0, IMM_RAW = 1, IMM_CMP = 2, IMM_TABLE = 3,
                         IMM_SEED = 4, IMM_ROT3_AXIS = 5;
inline constexpr uint8_t SVC_VALU = 0, SVC_VMUL = 1, SVC_CURVE = 2,
                         SVC_DIST = 3, SVC_COLD = 4, SVC_END = 5;

struct OpShape {
  uint8_t code;
  const char* name;
  uint8_t dst_width;
  uint8_t n_groups;
  uint8_t group_width[3];
  uint8_t n_src;  // sum of group widths (flattened operand count)
  uint8_t imm_kind;
  uint8_t svc;
};

inline constexpr OpShape OPS[] = {
    {0x00, "END", 0, 0, {0, 0, 0}, 0, 0, 5},  // END
    {0x01, "MOV", 1, 1, {1, 0, 0}, 1, 0, 0},  // VALU
    {0x02, "LDC", 1, 0, {0, 0, 0}, 0, 1, 0},  // VALU
    {0x03, "ADD", 1, 2, {1, 1, 0}, 2, 0, 0},  // VALU
    {0x04, "SUB", 1, 2, {1, 1, 0}, 2, 0, 0},  // VALU
    {0x05, "MUL", 1, 2, {1, 1, 0}, 2, 0, 1},  // VMUL
    {0x06, "MAD", 1, 3, {1, 1, 1}, 3, 0, 1},  // VMUL
    {0x07, "MIN", 1, 2, {1, 1, 0}, 2, 0, 0},  // VALU
    {0x08, "MAX", 1, 2, {1, 1, 0}, 2, 0, 0},  // VALU
    {0x09, "ABS", 1, 1, {1, 0, 0}, 1, 0, 0},  // VALU
    {0x0a, "CLAMP", 1, 3, {1, 1, 1}, 3, 0, 0},  // VALU
    {0x0b, "SELECT", 1, 3, {1, 1, 1}, 3, 0, 0},  // VALU
    {0x0c, "CMP", 1, 2, {1, 1, 0}, 2, 2, 0},  // VALU
    {0x10, "DOT2", 1, 2, {2, 2, 0}, 4, 0, 1},  // VMUL
    {0x11, "DOT3", 1, 2, {3, 3, 0}, 6, 0, 1},  // VMUL
    {0x12, "LEN2", 1, 1, {2, 0, 0}, 2, 0, 3},  // DIST
    {0x13, "LEN3", 1, 1, {3, 0, 0}, 3, 0, 3},  // DIST
    {0x14, "DIST2", 1, 2, {2, 2, 0}, 4, 0, 3},  // DIST
    {0x15, "NORMALIZE2", 2, 1, {2, 0, 0}, 2, 0, 4},  // COLD
    {0x16, "NORMALIZE3", 3, 1, {3, 0, 0}, 3, 0, 4},  // COLD
    {0x17, "RCP", 1, 1, {1, 0, 0}, 1, 0, 4},  // COLD
    {0x18, "SIN", 1, 1, {1, 0, 0}, 1, 0, 0},  // VALU
    {0x19, "COS", 1, 1, {1, 0, 0}, 1, 0, 0},  // VALU
    {0x1a, "CURVE", 1, 1, {1, 0, 0}, 1, 3, 2},  // CURVE
    {0x1b, "SPLINE", 1, 1, {1, 0, 0}, 1, 3, 4},  // COLD
    {0x1c, "NOISE2", 2, 1, {2, 0, 0}, 2, 4, 4},  // COLD
    {0x1d, "DCURVE", 1, 1, {1, 0, 0}, 1, 3, 2},  // CURVE
    {0x21, "RING", 1, 3, {1, 1, 1}, 3, 0, 4},  // COLD
    {0x22, "RIDGE", 1, 2, {1, 1, 0}, 2, 4, 4},  // COLD
    {0x28, "ROT2", 2, 2, {2, 1, 0}, 3, 0, 4},  // COLD
    {0x29, "ROT3", 3, 2, {3, 1, 0}, 4, 5, 4},  // COLD
};
inline constexpr int OP_COUNT = 31;

/** Shape by canonical opcode; nullptr for a non-canonical byte. */
inline constexpr const OpShape* shape_of(uint8_t code) {
  for (const OpShape& s : OPS) {
    if (s.code == code) return &s;
  }
  return nullptr;
}

static_assert(zfield::OP_END == 0x00, "canonical code drift: END");
static_assert(zfield::OP_MOV == 0x01, "canonical code drift: MOV");
static_assert(zfield::OP_LDC == 0x02, "canonical code drift: LDC");
static_assert(zfield::OP_ADD == 0x03, "canonical code drift: ADD");
static_assert(zfield::OP_SUB == 0x04, "canonical code drift: SUB");
static_assert(zfield::OP_MUL == 0x05, "canonical code drift: MUL");
static_assert(zfield::OP_MAD == 0x06, "canonical code drift: MAD");
static_assert(zfield::OP_MIN == 0x07, "canonical code drift: MIN");
static_assert(zfield::OP_MAX == 0x08, "canonical code drift: MAX");
static_assert(zfield::OP_ABS == 0x09, "canonical code drift: ABS");
static_assert(zfield::OP_CLAMP == 0x0a, "canonical code drift: CLAMP");
static_assert(zfield::OP_SELECT == 0x0b, "canonical code drift: SELECT");
static_assert(zfield::OP_CMP == 0x0c, "canonical code drift: CMP");
static_assert(zfield::OP_DOT2 == 0x10, "canonical code drift: DOT2");
static_assert(zfield::OP_DOT3 == 0x11, "canonical code drift: DOT3");
static_assert(zfield::OP_LEN2 == 0x12, "canonical code drift: LEN2");
static_assert(zfield::OP_LEN3 == 0x13, "canonical code drift: LEN3");
static_assert(zfield::OP_DIST2 == 0x14, "canonical code drift: DIST2");
static_assert(zfield::OP_NORMALIZE2 == 0x15, "canonical code drift: NORMALIZE2");
static_assert(zfield::OP_NORMALIZE3 == 0x16, "canonical code drift: NORMALIZE3");
static_assert(zfield::OP_RCP == 0x17, "canonical code drift: RCP");
static_assert(zfield::OP_SIN == 0x18, "canonical code drift: SIN");
static_assert(zfield::OP_COS == 0x19, "canonical code drift: COS");
static_assert(zfield::OP_CURVE == 0x1a, "canonical code drift: CURVE");
static_assert(zfield::OP_SPLINE == 0x1b, "canonical code drift: SPLINE");
static_assert(zfield::OP_NOISE2 == 0x1c, "canonical code drift: NOISE2");
static_assert(zfield::OP_DCURVE == 0x1d, "canonical code drift: DCURVE");
static_assert(zfield::OP_RING == 0x21, "canonical code drift: RING");
static_assert(zfield::OP_RIDGE == 0x22, "canonical code drift: RIDGE");
static_assert(zfield::OP_ROT2 == 0x28, "canonical code drift: ROT2");
static_assert(zfield::OP_ROT3 == 0x29, "canonical code drift: ROT3");

}  // namespace optable
}  // namespace zfield
