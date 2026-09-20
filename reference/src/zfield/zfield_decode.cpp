// zfield_decode.cpp — .zprog loader (field-ir.md §4/§5). Runs the FULL
// validator rule set V1..V12 on every load — never trusts the bytes (charter
// §19.4: invalid microcode rejected before any register write). Mirrors the
// TS reader compiler/src/field_ir/serialize.ts rule for rule; their agreement
// is asserted by the golden-vector differential.

#include "zfield/zfield.hpp"

#include <cstdio>

// The ONE canonical operation table. decode() was the last consumer in the
// tree still carrying its own hand-written copy of these shapes (R127).
#include "zfield/generated/zfield_optable.hpp"

namespace zfield {

namespace {

uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct Fail {
  DecodeError err;
  std::string detail;
};

// OP METADATA COMES FROM THE GENERATED TABLE. There is no third description of
// opcode shape any more (ruling R127, packet L1, 2026-09-20).
//
// WHAT WAS HERE, AND WHY IT WENT. This was a hand-transcribed switch over all
// 31 canonical opcodes whose own comment read: "duplicated from types.ts by
// hand ONCE, asserted by the fuzz corpus replay (any drift shows up as a
// decode/interpret divergence)." Both halves of that sentence were true and the
// conclusion drawn from it was not:
//
//   * The corpus reaches 19 of 31 opcodes. `compiler/src/field_ir/fuzz_gen.ts`
//     selects ops from a SIXTEEN-ARM SWITCH (lines 77-94) plus one appended
//     NOISE2; it can never emit DOT2, DOT3, LEN2, LEN3, DIST2, NORMALIZE2,
//     NORMALIZE3, COS, RING, RIDGE, ROT2 or ROT3. Counting the three committed
//     Earth programs as well (they add DIST2, RING and COS) the whole repo
//     reached 22 of 31, so EIGHT OF THIS SWITCH'S NINETEEN ROWS had never once
//     executed -- and they were the wide ones. `dstW == 3` was never validated
//     by anything, nor was imm kind 5 (the ROT3 axis), nor a source group of
//     width 3.
//   * The divergence signal needed BOTH sides to run. decode() reached this
//     table; interpret() reads the generated one. For any opcode the corpus
//     does not contain, there was no comparison to diverge.
//   * `field_fplan_diff.cpp` DOES draw uniformly from all 31 shapes, but it
//     builds a zfield::Decoded in memory and never serialises a .zprog, so it
//     never called decode() and never reached here at all.
//
// So the hand table could have disagreed with the generated one across nine
// opcodes indefinitely while every test stayed green -- CLAUDE.md's own law
// that a gate which cannot reach the state is not evidence about the state.
// (Measured 2026-09-20: the two tables AGREED on all 31 shapes at the moment of
// folding, so nothing was silently wrong. The point is that nothing was
// watching, and the flattering direction would have been to quote the silence.)
//
// Delegating removes the possibility rather than re-auditing it. The generated
// table is already the single source for zfield_plan.cpp and
// zfield_interpret.cpp; decode() was the lone hold-out.
struct OpMeta {
  int dstW;
  int srcGroups[3];
  int nGroups;
  char imm;
};  // imm: 0 none, 1 raw, 2 cmp, 3 table, 4 seed, 5 rot3 -- the IMM_* order
    // of zfield/generated/zfield_optable.hpp, asserted below.
static_assert(optable::IMM_NONE == 0 && optable::IMM_RAW == 1 && optable::IMM_CMP == 2 &&
                  optable::IMM_TABLE == 3 && optable::IMM_SEED == 4 &&
                  optable::IMM_ROT3_AXIS == 5,
              "OpMeta::imm and optable IMM_* have come apart");

bool opMeta(uint8_t op, OpMeta& m) {
  const optable::OpShape* sh = optable::shape_of(op);
  if (sh == nullptr) return false;  // a non-canonical byte, exactly as before
  m.dstW = sh->dst_width;
  m.nGroups = sh->n_groups;
  for (int g = 0; g < 3; ++g) m.srcGroups[g] = sh->group_width[g];
  m.imm = (char)sh->imm_kind;
  return true;
}

}  // namespace

const char* decodeErrorName(DecodeError e) {
  switch (e) {
    case DecodeError::kOk:
      return "ok";
    case DecodeError::kBadMagic:
      return "V1 bad magic";
    case DecodeError::kBadVersion:
      return "V1 bad version";
    case DecodeError::kBadLength:
      return "V1 bad length";
    case DecodeError::kBadCrc:
      return "V2 body CRC mismatch";
    case DecodeError::kBadHash:
      return "V2 program hash mismatch";
    case DecodeError::kBadProfile:
      return "V3 profile out of range";
    case DecodeError::kBadFlags:
      return "V3 flags nonzero";
    case DecodeError::kInstrCeiling:
      return "V4 instr ceiling";
    case DecodeError::kBadTable:
      return "V5 table";
    case DecodeError::kBadIoMap:
      return "V6 io map";
    case DecodeError::kRegOutOfRange:
      return "V7 register >= 64";
    case DecodeError::kDstOverlapsInputOrSource:
      return "V8 dst overlaps input/source";
    case DecodeError::kBadOpcodeOrImm:
      return "V9 opcode/imm discipline";
    case DecodeError::kBadEnd:
      return "V10 END placement";
    case DecodeError::kUseBeforeDef:
      return "V11 use before def";
    case DecodeError::kOutputNeverDefined:
      return "V12 output never defined";
  }
  return "?";
}

uint32_t programHashOfBytes(const uint8_t* bytes, size_t n) {
  if (n < ZPROG_HEADER_BYTES) return 0;
  const uint16_t instr_count = rd16(bytes + 12);
  const uint16_t table_bytes = rd16(bytes + 16);
  const uint8_t* code = bytes + ZPROG_HEADER_BYTES;
  const uint8_t* tables = code + (size_t)8 * instr_count;
  uint32_t h = zhao_abi::zhao_crc32c(0, code, (size_t)8 * instr_count);
  h = zhao_abi::zhao_crc32c(h, tables, table_bytes);
  return h + instr_count;
}

DecodeResult decode(const uint8_t* bytes, size_t n) {
  DecodeResult res;
  const auto fail = [&](DecodeError e, const std::string& d) {
    res.error = e;
    res.detail = d;
    return res;
  };

  // V1
  if (n < ZPROG_HEADER_BYTES) return fail(DecodeError::kBadLength, "shorter than header");
  if (rd32(bytes) != ZPROG_MAGIC) return fail(DecodeError::kBadMagic, "");
  if (rd16(bytes + 4) != FIELD_IR_VERSION) return fail(DecodeError::kBadVersion, "");
  const uint8_t profile = bytes[6];
  const uint8_t flags = bytes[7];
  const uint32_t source_id = rd32(bytes + 8);
  const uint16_t instr_count = rd16(bytes + 12);
  const uint8_t table_count = bytes[14];
  const uint8_t io_lane_count = bytes[15];
  const uint16_t table_bytes = rd16(bytes + 16);
  const uint16_t map_bytes = rd16(bytes + 18);
  const uint32_t hash_field = rd32(bytes + 20);
  const uint32_t body_crc = rd32(bytes + 24);

  const size_t body_start = ZPROG_HEADER_BYTES + (size_t)8 * instr_count;
  const size_t expected = ZPROG_HEADER_BYTES + (size_t)8 * instr_count + table_bytes + map_bytes;
  if (n != expected)
    return fail(DecodeError::kBadLength, std::to_string(n) + " != " + std::to_string(expected));

  // V2 (CRC over the image with the CRC field zeroed)
  {
    std::vector<uint8_t> zeroed(bytes, bytes + n);
    zeroed[24] = zeroed[25] = zeroed[26] = zeroed[27] = 0;
    if (zhao_abi::zhao_crc32c(0, zeroed.data(), zeroed.size()) != body_crc) {
      return fail(DecodeError::kBadCrc, "");
    }
  }
  if (programHashOfBytes(bytes, n) != hash_field) {
    return fail(DecodeError::kBadHash, "");
  }

  // V3
  if (profile > 4) return fail(DecodeError::kBadProfile, std::to_string(profile));
  if (flags != 0) return fail(DecodeError::kBadFlags, std::to_string(flags));

  // V4
  if (instr_count == 0 || instr_count > GLOBAL_CEILING) {
    return fail(DecodeError::kInstrCeiling, std::to_string(instr_count));
  }
  if (instr_count > PROFILE_CEILING[profile]) {
    return fail(DecodeError::kInstrCeiling, std::to_string(instr_count));
  }

  // V5
  if (table_count > 4) return fail(DecodeError::kBadTable, "count > 4");

  const uint8_t* p = bytes + ZPROG_HEADER_BYTES;

  // code
  res.prog.instrs.resize(instr_count);
  for (size_t i = 0; i < instr_count; ++i, p += 8) {
    const uint8_t b1 = p[1], b2 = p[2], b3 = p[3];
    Instr& ins = res.prog.instrs[i];
    ins.op = p[0];
    ins.dst = b1 & 0x3F;
    ins.a = (uint8_t)(((b1 >> 6) | ((b2 & 0x0F) << 2)) & 0x3F);
    ins.b = (uint8_t)(((b2 >> 4) | ((b3 & 0x03) << 4)) & 0x3F);
    ins.c = (uint8_t)((b3 >> 2) & 0x3F);
    ins.imm = rd32(p + 4);
  }

  // tables
  res.prog.tables.resize(table_count);
  for (uint8_t t = 0; t < table_count; ++t) {
    Table& tab = res.prog.tables[t];
    tab.kind = *p++;
    const uint8_t rsvd = *p++;
    const uint16_t cnt = rd16(p);
    p += 2;
    if (tab.kind > 1) return fail(DecodeError::kBadTable, "kind");
    if (rsvd != 0) return fail(DecodeError::kBadTable, "reserved byte");
    if (cnt < 2 || cnt > 64) return fail(DecodeError::kBadTable, "entry count");
    tab.x.resize(cnt);
    tab.y.resize(cnt);
    tab.dy.resize(cnt);
    for (uint16_t i = 0; i < cnt; ++i) {
      const int32_t x = (int32_t)rd32(p);
      p += 4;
      const int32_t y = (int32_t)rd32(p);
      p += 4;
      const int32_t dy = (int32_t)rd32(p);
      p += 4;
      tab.x[i] = x;
      tab.y[i] = y;
      tab.dy[i] = dy;
      if (i > 0 && x <= tab.x[i - 1]) return fail(DecodeError::kBadTable, "x order");
    }
    if (tab.kind == 1) {
      const int32_t step0 = tab.x[1] - tab.x[0];
      for (uint16_t i = 1; i < cnt; ++i) {
        if (tab.x[i] - tab.x[i - 1] != step0) {
          return fail(DecodeError::kBadTable, "spline spacing");
        }
      }
    }
  }
  if ((size_t)(p - bytes) - ZPROG_HEADER_BYTES != (size_t)8 * instr_count + table_bytes) {
    return fail(DecodeError::kBadTable, "section length");
  }

  // io map (12 bytes per lane)
  if (io_lane_count == 0 || io_lane_count > 32) {
    return fail(DecodeError::kBadIoMap, "lane count");
  }
  struct RawLane {
    uint8_t reg, kind, type, name_id;
    int32_t min, max;
  };
  std::vector<RawLane> lanes(io_lane_count);
  for (uint8_t i = 0; i < io_lane_count; ++i) {
    lanes[i].reg = *p++;
    lanes[i].kind = *p++;
    lanes[i].type = *p++;
    lanes[i].name_id = *p++;
    lanes[i].min = (int32_t)rd32(p);
    p += 4;
    lanes[i].max = (int32_t)rd32(p);
    p += 4;
  }
  // source map (8 bytes per instruction)
  res.prog.src_map.resize(instr_count);
  for (size_t i = 0; i < instr_count; ++i) {
    res.prog.src_map[i].source_id = rd32(p);
    res.prog.src_map[i].line = rd16(p + 4);
    res.prog.src_map[i].col = rd16(p + 6);
    p += 8;
  }
  // name pool (map section remainder; exactly consumed at the file end)
  const uint8_t* names_start = p;
  std::vector<std::string> names;
  for (uint8_t i = 0; i < io_lane_count; ++i) {
    std::string s;
    while (true) {
      if (p >= bytes + n) return fail(DecodeError::kBadIoMap, "name pool truncated");
      const uint8_t ch = *p++;
      if (ch == 0) break;
      s.push_back((char)ch);
      if (s.size() > 64) return fail(DecodeError::kBadIoMap, "name too long");
    }
    names.push_back(s);
  }
  if (p != bytes + n || (size_t)(p - names_start) !=
                            map_bytes - (size_t)12 * io_lane_count - (size_t)8 * instr_count) {
    return fail(DecodeError::kBadIoMap, "map section length");
  }

  // V6
  for (uint8_t i = 0; i < io_lane_count; ++i) {
    const RawLane& l = lanes[i];
    if (l.type > 3) return fail(DecodeError::kBadIoMap, "type");
    if (l.name_id != i) return fail(DecodeError::kBadIoMap, "name_id != ordinal");
    if (l.kind > 1) return fail(DecodeError::kBadIoMap, "kind");
    IoLane lane;
    lane.name = i < names.size() ? names[i] : "";
    lane.type = l.type;
    lane.reg = l.reg;
    lane.min = l.min;
    lane.max = l.max;
    if (l.kind == 0)
      res.prog.in_lanes.push_back(lane);
    else
      res.prog.out_lanes.push_back(lane);
  }
  for (size_t i = 0; i < res.prog.in_lanes.size(); ++i) {
    if (res.prog.in_lanes[i].reg != i) {
      return fail(DecodeError::kBadIoMap, "input reg != ordinal");
    }
    if (res.prog.in_lanes[i].min > res.prog.in_lanes[i].max) {
      return fail(DecodeError::kBadIoMap, "bounds inverted");
    }
  }
  for (const IoLane& o : res.prog.out_lanes) {
    if (o.reg < res.prog.in_lanes.size()) {
      return fail(DecodeError::kBadIoMap, "output reg is an input");
    }
    if (o.reg >= REG_COUNT) return fail(DecodeError::kRegOutOfRange, "output reg");
    for (const IoLane& other : res.prog.out_lanes) {
      if (&other != &o && other.reg == o.reg) {
        return fail(DecodeError::kBadIoMap, "duplicate output reg");
      }
    }
  }

  // V7..V12 over the instruction stream
  const size_t n_in = res.prog.in_lanes.size();
  bool defined[REG_COUNT] = {false};
  for (size_t i = 0; i < n_in; ++i) defined[i] = true;

  for (size_t pc = 0; pc < instr_count; ++pc) {
    const Instr& ins = res.prog.instrs[pc];
    OpMeta m;
    if (!opMeta(ins.op, m)) {
      return fail(DecodeError::kBadOpcodeOrImm, "pc " + std::to_string(pc) + " unknown opcode");
    }
    const uint16_t srcFields[3] = {ins.a, ins.b, ins.c};
    bool srcIsReg[3] = {false, false, false};
    int groupStart[3] = {0, 0, 0};
    for (int g = 0; g < m.nGroups; ++g) {
      srcIsReg[g] = true;
      groupStart[g] = srcFields[g];
      for (int k = 0; k < m.srcGroups[g]; ++k) {
        const int r = srcFields[g] + k;
        if (r >= (int)REG_COUNT) {
          return fail(DecodeError::kRegOutOfRange, "pc " + std::to_string(pc) + " src reg");
        }
        if (!defined[r]) {
          return fail(DecodeError::kUseBeforeDef,
                      "pc " + std::to_string(pc) + " reg " + std::to_string(r));
        }
      }
    }
    for (int f = m.nGroups; f < 3; ++f) {
      if (srcFields[f] != 0) {
        return fail(DecodeError::kBadOpcodeOrImm,
                    "pc " + std::to_string(pc) + " unused operand field nonzero");
      }
    }
    for (int k = 0; k < m.dstW; ++k) {
      const int d = ins.dst + k;
      if (d >= (int)REG_COUNT) {
        return fail(DecodeError::kRegOutOfRange, "pc " + std::to_string(pc) + " dst");
      }
      if (d < (int)n_in) {
        return fail(DecodeError::kDstOverlapsInputOrSource, "dst is an input");
      }
      for (int g = 0; g < m.nGroups; ++g) {
        if (d >= groupStart[g] && d < groupStart[g] + m.srcGroups[g]) {
          return fail(DecodeError::kDstOverlapsInputOrSource, "dst overlaps a source");
        }
      }
      defined[d] = true;
    }
    // V9 imm discipline
    switch (m.imm) {
      case 0:
        if (ins.imm != 0) return fail(DecodeError::kBadOpcodeOrImm, "imm nonzero");
        break;
      case 1:
        break;  // LDC raw
      case 2:
        if (ins.imm > 5) return fail(DecodeError::kBadOpcodeOrImm, "cmp mode");
        break;
      case 3:
        if (ins.imm >= res.prog.tables.size()) {
          return fail(DecodeError::kBadOpcodeOrImm, "table id");
        }
        break;
      case 4:
        break;  // seed
      case 5:
        if ((ins.imm & ~3u) != 0 || (ins.imm & 3) > 2) {
          return fail(DecodeError::kBadOpcodeOrImm, "rot3 axis");
        }
        break;
    }
    (void)srcIsReg;
  }

  // V10
  if (res.prog.instrs.back().op != OP_END) {
    return fail(DecodeError::kBadEnd, "last instruction is not END");
  }
  for (size_t pc = 0; pc + 1 < instr_count; ++pc) {
    if (res.prog.instrs[pc].op == OP_END) {
      return fail(DecodeError::kBadEnd, "END before last");
    }
  }
  // V12
  for (const IoLane& o : res.prog.out_lanes) {
    if (!defined[o.reg]) {
      return fail(DecodeError::kOutputNeverDefined, o.name);
    }
  }

  res.prog.profile = profile;
  res.prog.source_id = source_id;
  res.prog.program_hash = hash_field;
  res.error = DecodeError::kOk;
  return res;
}

}  // namespace zfield
