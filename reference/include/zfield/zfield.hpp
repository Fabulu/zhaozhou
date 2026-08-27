// zfield.hpp — the ONE generic Field IR interpreter + full-validating loader
// (spec/form/field-ir.md, FIELD_IR_VERSION 1; charter 29-6: op semantics live
// here and in the TS interpreter compiler/src/field_ir/interpret.ts ONLY —
// grep-audit: no third implementation shall exist).
//
// Spec map:
//   field-ir.md 1     machine model (64-bit word, 64x32 regfile, adjacency)
//   field-ir.md 2     frozen opcode table v1 (incl. DCURVE 0x1D)
//   field-ir.md 3     pinned op semantics (single-rounding law A3b; every op
//                     total; sticky Status{sat,rcp0})
//   field-ir.md 4     validator V1..V12 (Dalvik model: reject before any
//                     register write — decode() runs the FULL rule set,
//                     never trusting the bytes)
//   field-ir.md 5     .zprog layout; program hash = CRC-32C(code|tables) +
//                     instr_count; body CRC = CRC over file w/ field zeroed
//   field-ir.md 7     profiles + provisional ceilings
// Numeric law: spec/qformats.md (QFMT_VERSION 1) via zref:: — fx_mul/fx_mad
// single rounding (3/A3b), field_rcp (6.2, rcp(0)=0x7FFFFFFF+RCP0),
// fx_sin/fx_cos (7.1), noise2_hash (7.5), smoothstep (7.3),
// normalize3_approx (7.4), isqrt_u64 (7.2), rcp_u24_norm (6.1).
// CRC-32C: the generated zhao_abi.h table (capture_format.md 2).

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "zhao_abi.h"  // generated: constexpr ZHAO_CRC32C_TABLE (capture_format.md 2)

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

namespace zfield {

// ---------------------------------------------------------------- version ---

constexpr uint16_t FIELD_IR_VERSION = 1;
constexpr uint32_t ZPROG_MAGIC = 0x5049465Au;  // 'Z','F','I','P' LE
constexpr uint32_t ZPROG_HEADER_BYTES = 28;
constexpr size_t REG_COUNT = 64;
constexpr size_t GLOBAL_CEILING = 64;

// -------------------------------------------------------------- profiles ---

enum Profile : uint8_t { EARTH = 0, WARP = 1, FLOW = 2, FORMATION = 3, STAMP = 4 };
constexpr uint16_t PROFILE_CEILING[5] = {32, 48, 48, 64, 32};  // provisional (7.3)

// --------------------------------------------------------------- opcodes ---

enum Op : uint8_t {
  OP_END = 0x00,
  OP_MOV = 0x01,
  OP_LDC = 0x02,
  OP_ADD = 0x03,
  OP_SUB = 0x04,
  OP_MUL = 0x05,
  OP_MAD = 0x06,
  OP_MIN = 0x07,
  OP_MAX = 0x08,
  OP_ABS = 0x09,
  OP_CLAMP = 0x0A,
  OP_SELECT = 0x0B,
  OP_CMP = 0x0C,
  OP_DOT2 = 0x10,
  OP_DOT3 = 0x11,
  OP_LEN2 = 0x12,
  OP_LEN3 = 0x13,
  OP_DIST2 = 0x14,
  OP_NORMALIZE2 = 0x15,
  OP_NORMALIZE3 = 0x16,
  OP_RCP = 0x17,
  OP_SIN = 0x18,
  OP_COS = 0x19,
  OP_CURVE = 0x1A,
  OP_SPLINE = 0x1B,
  OP_NOISE2 = 0x1C,
  OP_DCURVE = 0x1D,
  OP_RING = 0x21,
  OP_RIDGE = 0x22,
  OP_ROT2 = 0x28,
  OP_ROT3 = 0x29,
};

// ---------------------------------------------------------- decode errors ---

enum class DecodeError {
  kOk = 0,
  kBadMagic,
  kBadVersion,
  kBadLength,  // V1
  kBadCrc,
  kBadHash,  // V2
  kBadProfile,
  kBadFlags,                  // V3
  kInstrCeiling,              // V4
  kBadTable,                  // V5
  kBadIoMap,                  // V6
  kRegOutOfRange,             // V7
  kDstOverlapsInputOrSource,  // V8
  kBadOpcodeOrImm,            // V9
  kBadEnd,                    // V10
  kUseBeforeDef,              // V11
  kOutputNeverDefined,        // V12
};

const char* decodeErrorName(DecodeError e);

// -------------------------------------------------------------- structures --

struct Instr {
  uint8_t op;
  uint8_t dst, a, b, c;  // 6-bit register fields (group starts, 1.3)
  uint32_t imm;          // raw u32
};

struct Table {
  uint8_t kind;  // 0 curve, 1 spline
  std::vector<int32_t> x, y, dy;
};

struct IoLane {
  std::string name;
  uint8_t type;  // 0 fx, 1 unit, 2 angle, 3 u32
  uint8_t reg;
  int32_t min = 0, max = 0;  // declared bounds; inputs only
};

struct SourceRef {
  uint32_t source_id;
  uint16_t line;
  uint16_t col;
};

struct Status {
  bool sat = false;
  bool rcp0 = false;
};

/** Decoded + fully validated program view (never trusts the bytes, 4). */
struct Decoded {
  uint8_t profile = 0;
  uint32_t source_id = 0;
  std::vector<Instr> instrs;
  std::vector<Table> tables;
  std::vector<IoLane> in_lanes, out_lanes;
  std::vector<SourceRef> src_map;
  uint32_t program_hash = 0;
};

struct DecodeResult {
  DecodeError error = DecodeError::kOk;
  std::string detail;  // human context (pc / lane / index)
  Decoded prog;        // valid when error == kOk
};

/** Load + re-validate a .zprog byte image (field-ir.md 4/5). */
DecodeResult decode(const uint8_t* bytes, size_t n);

/** Program hash of a serialized image (5.4): CRC32C(code|tables) + count. */
uint32_t programHashOfBytes(const uint8_t* bytes, size_t n);

/**
 * constexpr hash for static_asserts in generated wrappers — same law as
 * programHashOfBytes (5.4), using the generated constexpr CRC table.
 */
constexpr uint32_t crc32cConst(const uint8_t* p, size_t n, uint32_t crc = 0) {
  crc = ~crc;
  for (size_t i = 0; i < n; ++i) {
    crc = zhao_abi::ZHAO_CRC32C_TABLE[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

/** constexpr program hash of a byte image (for static_asserts). */
constexpr uint32_t programHashConst(const uint8_t* p, size_t n) {
  // header: instr_count @12 (u16), table_section_bytes @16 (u16)
  const uint16_t instr_count = (uint16_t)(p[12] | (p[13] << 8));
  const uint16_t table_bytes = (uint16_t)(p[16] | (p[17] << 8));
  const uint8_t* code = p + ZPROG_HEADER_BYTES;
  const uint8_t* tables = code + (size_t)8 * instr_count;
  uint32_t h = crc32cConst(code, (size_t)8 * instr_count);
  h = crc32cConst(tables, table_bytes, h);
  return h + instr_count;
}

// ------------------------------------------------------------ interpreter --

/**
 * Interpret one input record (field-ir.md 3). `in` lanes map to R0.. in the
 * program's input order; `out` lanes are read from the output map at END.
 * Every op is total; Status is sticky per call. Only reachable on a decoded
 * (i.e. validated) program.
 */
Status interpret(const Decoded& prog, const int32_t* in, size_t n_in, int32_t* out, size_t n_out);

/**
 * As above, additionally exposing the final SatLedger (Field v3 Phase 2:
 * the planner differential compares EVERY saturation lane, not only the
 * collapsed Status). `ledger_out` may be nullptr.
 */
Status interpret(const Decoded& prog, const int32_t* in, size_t n_in, int32_t* out, size_t n_out,
                 zref::SatLedger* ledger_out);

}  // namespace zfield
