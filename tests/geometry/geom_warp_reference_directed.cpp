// geom_warp_reference_directed.cpp -- the directed reference test for
// zref::GeomWarp (reference/include/zref/zref_geom_warp.hpp).
//
// AUTHORITY. Owner directive reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt
// (owner commit 4c256137), ratified 2026-09-20 in
// reports/OWNER-RATIFICATION-20260920-WARP.md. This file covers the directive's
// section 22.1 semantic cases T01-T08 and the canonical fixtures of 21.3, at the
// REFERENCE level only.
//
// WHAT THIS TEST DOES NOT CLAIM, stated here because the directive is emphatic
// about it (W18, 26.2, and the "use a successful identity/bypass test as
// evidence of active deformation" prohibition in section A):
//
//   * It is NOT evidence that GEOM.WARP exists in hardware. There is no
//     zhao_geom_warp.sv at this commit and this test does not instantiate one.
//   * It is NOT evidence about throughput, admission, or resources.
//   * Passing here is the directive's COMMIT A gate -- "reference tests/decoder/
//     program hash/map tests pass; NO RTL CLAIM YET" -- and nothing beyond it.
//
// Every fixture below is built through the REAL `zfield::decode` validator, so
// a program this file accepts is one the canonical loader accepted. Nothing is
// hand-asserted past the decoder.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zhao_abi.h"
#include "zref/zref_geom_warp.hpp"

namespace {

int g_checks = 0;
int g_fail = 0;

void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq_i64(long long got, long long want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fail;
    std::printf("FAIL: %s (got %lld want %lld)\n", what, got, want);
  }
}

// ---------------------------------------------------------------------------
// A minimal .zprog builder for W-profile programs.
//
// The layout is spec/form/field-ir.md section 5, and the encoding below is read
// off the REAL decoder (reference/src/zfield/zfield_decode.cpp:225-230) rather
// than from prose:
//
//   p[0] = op
//   dst = p[1] & 0x3F                                  -> word bits 8..13
//   a   = (p[1]>>6) | ((p[2] & 0x0F) << 2)             -> word bits 14..19
//   b   = (p[2]>>4) | ((p[3] & 0x03) << 4)             -> word bits 20..25
//   c   = p[3] >> 2                                    -> word bits 26..31
//
// so one instruction word is op | dst<<8 | a<<14 | b<<20 | c<<26, little-endian,
// followed by its u32 imm.
// ---------------------------------------------------------------------------

constexpr std::uint32_t insWord(std::uint8_t op, std::uint8_t dst, std::uint8_t a = 0,
                                std::uint8_t b = 0, std::uint8_t c = 0) {
  return static_cast<std::uint32_t>(op) | (static_cast<std::uint32_t>(dst & 0x3F) << 8) |
         (static_cast<std::uint32_t>(a & 0x3F) << 14) |
         (static_cast<std::uint32_t>(b & 0x3F) << 20) |
         (static_cast<std::uint32_t>(c & 0x3F) << 26);
}

struct Ins {
  std::uint32_t word;
  std::uint32_t imm;
};

struct Lane {
  std::uint8_t reg;
  std::uint8_t kind;  // 0 input, 1 output
  std::uint8_t type;  // 0 fx, 3 u32
  const char* name;
};

/** Assemble a .zprog image. `profile` lets us build a deliberately-wrong one. */
std::vector<std::uint8_t> buildProgram(std::uint8_t profile, const std::vector<Ins>& code,
                                       const std::vector<Lane>& lanes) {
  std::vector<std::uint8_t> map;
  std::vector<std::uint8_t> namepool;
  for (std::size_t i = 0; i < lanes.size(); ++i) {
    map.push_back(lanes[i].reg);
    map.push_back(lanes[i].kind);
    map.push_back(lanes[i].type);
    map.push_back(static_cast<std::uint8_t>(i));  // V6: name_id == ordinal
    map.insert(map.end(), 8, 0);                  // min/max bounds, unused
    for (const char* p = lanes[i].name; *p; ++p) namepool.push_back(std::uint8_t(*p));
    namepool.push_back(0);
  }
  std::vector<std::uint8_t> srcmap(8 * code.size(), 0);
  const std::size_t map_bytes = map.size() + srcmap.size() + namepool.size();

  std::vector<std::uint8_t> prog;
  auto u16 = [&](std::uint16_t v) {
    prog.push_back(std::uint8_t(v & 0xFF));
    prog.push_back(std::uint8_t(v >> 8));
  };
  auto u32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i) prog.push_back(std::uint8_t(v >> (8 * i)));
  };
  u32(0x5049465Au);                                     // magic 'ZFIP'
  u16(1);                                               // version
  prog.push_back(profile);                              // profile
  prog.push_back(0);                                    // flags
  u32(0);                                               // source_id
  u16(static_cast<std::uint16_t>(code.size()));         // instr_count
  prog.push_back(0);                                    // table_count
  prog.push_back(static_cast<std::uint8_t>(lanes.size()));  // io_lane_count
  u16(0);                                               // table_section_bytes
  u16(static_cast<std::uint16_t>(map_bytes));
  u32(0);  // program_hash, patched below
  u32(0);  // body_crc32c, patched below
  for (const Ins& in : code) {
    u32(in.word);
    u32(in.imm);
  }
  prog.insert(prog.end(), map.begin(), map.end());
  prog.insert(prog.end(), srcmap.begin(), srcmap.end());
  prog.insert(prog.end(), namepool.begin(), namepool.end());

  // section 5.4: hash = CRC32C(code || tables) + instr_count
  const std::uint8_t* codep = prog.data() + zfield::ZPROG_HEADER_BYTES;
  std::uint32_t h = zhao_abi::zhao_crc32c(0, codep, 8 * code.size());
  h = zhao_abi::zhao_crc32c(h, codep + 8 * code.size(), 0);
  h += static_cast<std::uint32_t>(code.size());
  for (int i = 0; i < 4; ++i) prog[20 + i] = std::uint8_t(h >> (8 * i));
  // section 5.5: body CRC over the image with the CRC field still zero
  const std::uint32_t bc = zhao_abi::zhao_crc32c(0, prog.data(), prog.size());
  for (int i = 0; i < 4; ++i) prog[24 + i] = std::uint8_t(bc >> (8 * i));
  return prog;
}

/** The canonical fifteen Warp input lanes, R0..R14 (directive 5.1). */
std::vector<Lane> warpLanes(std::size_t n_in = 15) {
  static const char* kInNames[15] = {"px", "py", "pz", "nx", "ny", "nz", "a0", "a1",
                                     "a2", "a3", "time", "p0", "p1", "p2", "p3"};
  std::vector<Lane> v;
  for (std::size_t i = 0; i < n_in; ++i) {
    // lane 10 is `time`, a u32 tick lane (type 3); every other lane is fx.
    v.push_back(Lane{static_cast<std::uint8_t>(i), 0, static_cast<std::uint8_t>(i == 10 ? 3 : 0),
                     kInNames[i]});
  }
  static const char* kOutNames[6] = {"dx", "dy", "dz", "nxo", "nyo", "nzo"};
  for (std::size_t i = 0; i < 6; ++i) {
    // Outputs live at R15..R20: V8 refuses a dst that overlaps an input.
    v.push_back(Lane{static_cast<std::uint8_t>(15 + i), 1, 0, kOutNames[i]});
  }
  return v;
}

zfield::Decoded mustDecode(const std::vector<std::uint8_t>& img, const char* what) {
  zfield::DecodeResult r = zfield::decode(img.data(), img.size());
  if (r.error != zfield::DecodeError::kOk) {
    ++g_fail;
    std::printf("FAIL: %s did not decode: %s (%s)\n", what, zfield::decodeErrorName(r.error),
                r.detail.c_str());
  }
  return r.prog;
}

// -------------------------------------------------------------- the fixtures --

constexpr std::uint8_t R_NX = 3, R_NY = 4, R_NZ = 5;
constexpr std::uint8_t R_A0 = 6, R_A1 = 7, R_A2 = 8, R_A3 = 9;
constexpr std::uint8_t R_P0 = 11, R_P1 = 12, R_P2 = 13, R_P3 = 14;
constexpr std::uint8_t R_DX = 15, R_DY = 16, R_DZ = 17;
constexpr std::uint8_t R_NXO = 18, R_NYO = 19, R_NZO = 20;

/** Carry the input normal through unchanged: the last three of every fixture. */
void appendNormalPassthrough(std::vector<Ins>& code) {
  code.push_back({insWord(zfield::OP_MOV, R_NXO, R_NX), 0});
  code.push_back({insWord(zfield::OP_MOV, R_NYO, R_NY), 0});
  code.push_back({insWord(zfield::OP_MOV, R_NZO, R_NZ), 0});
}

/** IDENTITY (21.3): zero displacement, normal unchanged. */
std::vector<std::uint8_t> fixtureIdentity() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_LDC, R_DX), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DY), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
  appendNormalPassthrough(code);
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/** TRANSLATE (21.3): displacement = p0,p1,p2; normal unchanged. */
std::vector<std::uint8_t> fixtureTranslate() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_MOV, R_DX, R_P0), 0});
  code.push_back({insWord(zfield::OP_MOV, R_DY, R_P1), 0});
  code.push_back({insWord(zfield::OP_MOV, R_DZ, R_P2), 0});
  appendNormalPassthrough(code);
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/**
 * P3_SENTINEL (21.3): "at least one output explicitly depends on input lane 14.
 * This catches the spec's fourteen-versus-fifteen mistake."
 */
std::vector<std::uint8_t> fixtureP3Sentinel() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_MOV, R_DX, R_P3), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DY), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
  appendNormalPassthrough(code);
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/** ATTRIBUTES (21.3): distinct dependence on a0..a3, including negatives. */
std::vector<std::uint8_t> fixtureAttributes() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_MOV, R_DX, R_A0), 0});
  code.push_back({insWord(zfield::OP_MOV, R_DY, R_A1), 0});
  code.push_back({insWord(zfield::OP_MOV, R_DZ, R_A2), 0});
  // a3 drives a NORMAL output, so all four attributes reach a distinct lane.
  code.push_back({insWord(zfield::OP_MOV, R_NXO, R_A3), 0});
  code.push_back({insWord(zfield::OP_MOV, R_NYO, R_NY), 0});
  code.push_back({insWord(zfield::OP_MOV, R_NZO, R_NZ), 0});
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/** A zero REPLACEMENT normal, for T07's declared degenerate light input. */
std::vector<std::uint8_t> fixtureZeroNormal() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_LDC, R_DX), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DY), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
  code.push_back({insWord(zfield::OP_LDC, R_NXO), 0});
  code.push_back({insWord(zfield::OP_LDC, R_NYO), 0});
  code.push_back({insWord(zfield::OP_LDC, R_NZO), 0});
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/**
 * STATUS (21.3): "exact saturation and RCP(0) cases with valid DEFINED
 * results."
 *
 * dx = ADD(p0, p0) saturates when p0 is the positive rail; dy = RCP(a0) with
 * a0 = 0 sets rcp0 and returns the canonical answer. Both are DEFINED numeric
 * results, not malformed programs — directive 5.5 exists precisely so that
 * "a check of `resp_status != 0` MUST NOT turn a defined saturated Field
 * answer into a dropped vertex".
 *
 * Driving the saturation from p0 rather than from px is deliberate: it keeps
 * the POSITION far from its own rail, so the FIELD flag fires while the
 * APPLICATION flag does not. That is the separation §5.3 demands, and a
 * fixture that saturated both at once could not demonstrate it. case6 already
 * covers the mirror image (application saturates, field does not).
 */
std::vector<std::uint8_t> fixtureStatus() {
  std::vector<Ins> code;
  code.push_back({insWord(zfield::OP_ADD, R_DX, R_P0, R_P0), 0});
  code.push_back({insWord(zfield::OP_RCP, R_DY, R_A0), 0});
  code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
  appendNormalPassthrough(code);
  code.push_back({insWord(zfield::OP_END, 0), 0});
  return buildProgram(zfield::WARP, code, warpLanes());
}

/**
 * SPARSE_OUTPUTS (21.3): six valid, NONCONTIGUOUS output registers, including
 * high addresses, written in a different order from the canonical lane order.
 *
 * This fixture matters far beyond the reference: the shared Field host captures
 * outputs through a single CONTIGUOUS window based at `hdr_outbase`
 * (fpga/rtl/field/zhao_field_host.sv), so a program shaped like this one cannot
 * be captured by the hardware as it stands today. The reference accepts it,
 * which is exactly how the gap becomes visible rather than assumed away. See
 * FINDINGS-warp.md, prerequisite F5.
 */
std::vector<std::uint8_t> fixtureSparseOutputs() {
  const std::uint8_t dx = 21, dy = 34, dz = 47, nxo = 55, nyo = 60, nzo = 63;
  std::vector<Ins> code;
  // deliberately NOT in canonical order
  code.push_back({insWord(zfield::OP_MOV, nzo, R_NZ), 0});
  code.push_back({insWord(zfield::OP_MOV, dy, R_P1), 0});
  code.push_back({insWord(zfield::OP_MOV, nxo, R_NX), 0});
  code.push_back({insWord(zfield::OP_MOV, dx, R_P0), 0});
  code.push_back({insWord(zfield::OP_MOV, nyo, R_NY), 0});
  code.push_back({insWord(zfield::OP_MOV, dz, R_P2), 0});
  code.push_back({insWord(zfield::OP_END, 0), 0});

  std::vector<Lane> lanes = warpLanes();
  const std::uint8_t regs[6] = {dx, dy, dz, nxo, nyo, nzo};
  for (int i = 0; i < 6; ++i) lanes[15 + i].reg = regs[i];
  return buildProgram(zfield::WARP, code, lanes);
}

// ------------------------------------------------------------------- helpers --

zref::geom_warp::Inputs baseInputs() {
  zref::geom_warp::Inputs in;
  in.position[0] = 100 << 16;
  in.position[1] = -(50 << 16);
  in.position[2] = 7 << 16;
  // A deliberately NON-UNIT direction (W02): SKIN.NORM emits a raw blended
  // direction and LIGHT owns the magnitude. A fixture using a unit vector here
  // would silently pass an implementation that normalized.
  in.direction[0] = 3 << 16;
  in.direction[1] = -(4 << 16);
  in.direction[2] = 12 << 16;
  in.time = 0x1234ABCD;
  return in;
}

zref::geom_warp::Declaration boundOf(std::int32_t b) {
  zref::geom_warp::Declaration d;
  d.displacement_bound[0] = b;
  d.displacement_bound[1] = b;
  d.displacement_bound[2] = b;
  return d;
}

}  // namespace

int main() {
  using namespace zref::geom_warp;

  // ================================================================ T01 =====
  // "Exactly 15 inputs and 6 outputs accepted; 14-input legacy typo refused
  // with an explicit signature diagnostic rather than reading past input
  // storage."  -- directive 22.1 T01, decision W01.
  {
    const zfield::Decoded id = mustDecode(fixtureIdentity(), "IDENTITY");
    check_eq_i64(static_cast<long long>(id.in_lanes.size()), 15, "case1: identity declares 15 in");
    check_eq_i64(static_cast<long long>(id.out_lanes.size()), 6, "case1: identity declares 6 out");
    check(check_signature(id) == Refusal::kNone, "case1: 15/6 signature accepted");
    check_eq_i64(GeomWarp::in_lanes, 15, "case1: zref::GeomWarp::in_lanes is FIFTEEN (W01)");
    check_eq_i64(GeomWarp::out_lanes, 6, "case1: zref::GeomWarp::out_lanes is six");

    // The legacy typo, built as a real program: fourteen input lanes.
    std::vector<Ins> code;
    code.push_back({insWord(zfield::OP_LDC, R_DX), 0});
    code.push_back({insWord(zfield::OP_LDC, R_DY), 0});
    code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
    appendNormalPassthrough(code);
    code.push_back({insWord(zfield::OP_END, 0), 0});
    const zfield::Decoded p14 = mustDecode(buildProgram(zfield::WARP, code, warpLanes(14)),
                                           "14-lane legacy typo");
    check_eq_i64(static_cast<long long>(p14.in_lanes.size()), 14, "case1: typo really has 14");
    check(check_signature(p14) == Refusal::kInputLaneCount,
          "case1: 14 inputs REFUSED with an explicit signature diagnostic");
    const Result r14 = GeomWarp::evaluate(p14, baseInputs(), boundOf(1 << 20));
    check(r14.poison(), "case1: a 14-lane program poisons rather than evaluating");
    check(r14.refusal == Refusal::kInputLaneCount, "case1: and names WHICH signature fault");
  }

  // ================================================================ T02 =====
  // "Profile mismatch rejected; ordinary Earth/Flow programs are not treated as
  // Warp merely because some output words happen to fit the bus."
  {
    std::vector<Ins> code;
    code.push_back({insWord(zfield::OP_LDC, R_DX), 0});
    code.push_back({insWord(zfield::OP_LDC, R_DY), 0});
    code.push_back({insWord(zfield::OP_LDC, R_DZ), 0});
    appendNormalPassthrough(code);
    code.push_back({insWord(zfield::OP_END, 0), 0});
    // Byte-for-byte the same shape, EARTH profile. The output words would fit.
    const zfield::Decoded earth =
        mustDecode(buildProgram(zfield::EARTH, code, warpLanes()), "EARTH-profile lookalike");
    check(check_signature(earth) == Refusal::kProfileMismatch,
          "case2: an EARTH program with a Warp-shaped signature is REFUSED");
    const Result re = GeomWarp::evaluate(earth, baseInputs(), boundOf(1 << 20));
    check(re.poison(), "case2: and it poisons rather than being evaluated as Warp");
  }

  // ================================================================ T03 =====
  // "Identity preserves position, raw reduced normal, and lighting exactly."
  {
    const zfield::Decoded id = mustDecode(fixtureIdentity(), "IDENTITY");
    const Inputs in = baseInputs();
    const Result r = GeomWarp::evaluate(id, in, boundOf(0));  // bound 0 admits d=0
    check(!r.poison(), "case3: identity is not poisoned even at bound 0");
    for (int k = 0; k < 3; ++k) {
      check_eq_i64(r.displacement[k], 0, "case3: identity displacement is exactly zero");
      check_eq_i64(r.position[k], in.position[k], "case3: identity preserves position EXACTLY");
      check_eq_i64(r.direction[k], in.direction[k], "case3: identity preserves the normal EXACTLY");
    }
    check_eq_i64(r.normal_shifts, 0,
                 "case3: a lawful upstream normal needs NO additional shift (5.4)");
    check(!r.degenerate, "case3: identity of a nonzero normal is not degenerate");
    check(!r.app_saturated, "case3: adding zero does not saturate");
  }

  // ================================================================ T04 =====
  // "p3 changes a returned displacement; all four parameter words independently
  // influence a fixture output."  -- the W01 sentinel.
  {
    const zfield::Decoded p3 = mustDecode(fixtureP3Sentinel(), "P3_SENTINEL");
    Inputs in = baseInputs();
    in.params[3] = 9 << 16;
    const Result a = GeomWarp::evaluate(p3, in, boundOf(1 << 20));
    check_eq_i64(a.displacement[0], 9 << 16, "case4: lane 14 (p3) REACHES a returned displacement");
    in.params[3] = -(9 << 16);
    const Result b = GeomWarp::evaluate(p3, in, boundOf(1 << 20));
    check_eq_i64(b.displacement[0], -(9 << 16), "case4: and it follows p3's sign");
    check(a.displacement[0] != b.displacement[0],
          "case4: THE FIFTEENTH LANE IS LIVE -- a 14-lane record would fail here");

    // p0/p1/p2 independently, through TRANSLATE.
    const zfield::Decoded tr = mustDecode(fixtureTranslate(), "TRANSLATE");
    Inputs t = baseInputs();
    t.params[0] = 1 << 16;
    t.params[1] = 2 << 16;
    t.params[2] = -(3 << 16);
    const Result rt = GeomWarp::evaluate(tr, t, boundOf(1 << 20));
    check_eq_i64(rt.displacement[0], 1 << 16, "case4: p0 drives dx");
    check_eq_i64(rt.displacement[1], 2 << 16, "case4: p1 drives dy");
    check_eq_i64(rt.displacement[2], -(3 << 16), "case4: p2 drives dz");
    check_eq_i64(rt.position[0], t.position[0] + (1 << 16), "case4: dx is ADDED to px");
    check_eq_i64(rt.position[1], t.position[1] + (2 << 16), "case4: dy is ADDED to py");
    check_eq_i64(rt.position[2], t.position[2] - (3 << 16), "case4: dz is ADDED to pz");
  }

  // ================================================================ T05 =====
  // "All four attributes independently influence a fixture, including
  // negative values."
  {
    const zfield::Decoded at = mustDecode(fixtureAttributes(), "ATTRIBUTES");
    Inputs in = baseInputs();
    in.attributes[0] = 11 << 16;
    in.attributes[1] = -(22 << 16);
    in.attributes[2] = 33 << 16;
    in.attributes[3] = -(44 << 16);
    const Result r = GeomWarp::evaluate(at, in, boundOf(1 << 22));
    check_eq_i64(r.displacement[0], 11 << 16, "case5: a0 reaches dx");
    check_eq_i64(r.displacement[1], -(22 << 16), "case5: a1 reaches dy, NEGATIVE");
    check_eq_i64(r.displacement[2], 33 << 16, "case5: a2 reaches dz");
    check_eq_i64(r.direction[0], -(44 << 16), "case5: a3 reaches a NORMAL lane, negative");
    // Each attribute is independent: move one, only one output moves.
    Inputs only = baseInputs();
    only.attributes[1] = 5 << 16;
    const Result r2 = GeomWarp::evaluate(at, only, boundOf(1 << 22));
    check_eq_i64(r2.displacement[0], 0, "case5: a0 alone controls dx");
    check_eq_i64(r2.displacement[1], 5 << 16, "case5: a1 alone controls dy");
    check_eq_i64(r2.displacement[2], 0, "case5: a2 alone controls dz");
  }

  // ================================================================ T06 =====
  // "Negative displacement, signed rails, zero, +/- one LSB, and position ADD
  // saturation use the exact canonical result and SEPARATE application status."
  //
  // This is decision W03 and directive 5.3's instrumentation rule: the
  // application's saturation must stay distinguishable from the Field
  // program's. A single collapsed flag would pass every check but this one.
  {
    const zfield::Decoded tr = mustDecode(fixtureTranslate(), "TRANSLATE");
    const std::int32_t kMax = 0x7FFFFFFF;
    const std::int32_t kMin = static_cast<std::int32_t>(0x80000000u);

    // +1 LSB
    Inputs a = baseInputs();
    a.position[0] = 0;
    a.params[0] = 1;
    const Result ra = GeomWarp::evaluate(tr, a, boundOf(kMax));
    check_eq_i64(ra.position[0], 1, "case6: +1 LSB is exact");
    check(!ra.app_saturated, "case6: +1 LSB does not saturate");

    // -1 LSB
    Inputs b = baseInputs();
    b.position[0] = 0;
    b.params[0] = -1;
    const Result rb = GeomWarp::evaluate(tr, b, boundOf(kMax));
    check_eq_i64(rb.position[0], -1, "case6: -1 LSB is exact");

    // Positive rail: INT32_MAX + 1 saturates, and SAYS SO on the application flag.
    Inputs c = baseInputs();
    c.position[0] = kMax;
    c.params[0] = 1;
    const Result rc = GeomWarp::evaluate(tr, c, boundOf(kMax));
    check_eq_i64(rc.position[0], kMax, "case6: the positive rail CLAMPS, never wraps");
    check(rc.app_saturated, "case6: and the APPLICATION saturation flag fires");
    check(!rc.field_status.sat,
          "case6: while the FIELD status stays clear -- the two causes are SEPARATE (5.3)");
    check(!rc.poison(),
          "case6: a saturated position is a defined numeric result, NOT a transport failure (5.5)");

    // Negative rail.
    Inputs d = baseInputs();
    d.position[0] = kMin;
    d.params[0] = -1;
    const Result rd = GeomWarp::evaluate(tr, d, boundOf(kMax));
    check_eq_i64(rd.position[0], kMin, "case6: the negative rail CLAMPS, never wraps");
    check(rd.app_saturated, "case6: and it too is reported on the application flag");

    // The bound is checked against the RETURNED displacement, and a violation
    // POISONS rather than clamping (5.6). The displacement is still reported.
    Inputs e = baseInputs();
    e.params[0] = 100 << 16;
    const Result re = GeomWarp::evaluate(tr, e, boundOf(10 << 16));
    check(re.refusal == Refusal::kBoundViolation, "case6: |d| > bound POISONS the meshlet (5.6)");
    check_eq_i64(re.displacement[0], 100 << 16,
                 "case6: and the OFFENDING value survives the refusal as evidence");
    check_eq_i64(re.position[0], 0, "case6: no position is published for a poisoned vertex");

    // Exactly AT the bound is legal: the law is `>`, not `>=`.
    Inputs f = baseInputs();
    f.params[0] = 10 << 16;
    const Result rf = GeomWarp::evaluate(tr, f, boundOf(10 << 16));
    check(!rf.poison(), "case6: |d| == bound is ADMITTED (the test is >, not >=)");

    // A negative bound is a malformed command (6.3), diagnosed distinctly.
    Declaration neg = boundOf(1 << 16);
    neg.displacement_bound[1] = -1;
    const Result rg = GeomWarp::evaluate(tr, baseInputs(), neg);
    check(rg.refusal == Refusal::kNegativeBound,
          "case6: a NEGATIVE bound is its own diagnosis, not a bound violation");
  }

  // ================================================================ T07 =====
  // "Zero output normal yields the declared degenerate light input."
  {
    const zfield::Decoded zn = mustDecode(fixtureZeroNormal(), "ZERO_NORMAL");
    const Result r = GeomWarp::evaluate(zn, baseInputs(), boundOf(0));
    check(r.degenerate, "case7: an all-zero replacement normal is DEGENERATE");
    check(!r.poison(), "case7: degenerate is a declared light input, NOT a poison");
    for (int k = 0; k < 3; ++k) {
      check_eq_i64(r.direction[k], 0, "case7: and the direction words are zero");
    }
    // ... and the test is "all three are zero", not "any is zero".
    const zfield::Decoded at = mustDecode(fixtureAttributes(), "ATTRIBUTES");
    Inputs one = baseInputs();
    one.attributes[3] = 0;  // nx' becomes 0, ny'/nz' do not
    const Result r2 = GeomWarp::evaluate(at, one, boundOf(1 << 22));
    check(!r2.degenerate,
          "case7: ONE zero component is NOT degenerate -- the law is all three (5.4)");
  }

  // ================================================================ T08 =====
  // "Output INT32_MIN and large normal components use safe absolute/range
  // reduction; the common shift applies to ALL components."
  {
    const zfield::Decoded at = mustDecode(fixtureAttributes(), "ATTRIBUTES");
    const std::int32_t kMin = static_cast<std::int32_t>(0x80000000u);

    // INT32_MIN in a normal lane. abs(INT32_MIN) overflows a signed 32-bit
    // negation, so this is the case that catches an implementation reaching for
    // `-x` instead of a widened unsigned magnitude.
    Inputs in = baseInputs();
    in.attributes[3] = kMin;      // -> nx'
    in.direction[1] = 1 << 16;    // -> ny' (small)
    in.direction[2] = -(1 << 16); // -> nz' (small, negative)
    const Result r = GeomWarp::evaluate(at, in, boundOf(1 << 30));
    check(!r.poison(), "case8: INT32_MIN in a normal lane is handled, not refused");
    check_eq_i64(r.normal_shifts, 2, "case8: INT32_MIN needs exactly TWO common shifts");
    check(r.normal_shifts <= kMaxNormalShifts,
          "case8: and never more than the directive's stated two (5.4)");
    // THE SHIFT IS COMMON. Every component moved by the same amount, so the
    // small ones shifted too -- that is what preserves the DIRECTION.
    check_eq_i64(r.direction[0], kMin >> 2, "case8: the large component is reduced");
    check_eq_i64(r.direction[1], (1 << 16) >> 2, "case8: the SMALL component shifted TOO");
    check_eq_i64(r.direction[2], (-(1 << 16)) >> 2, "case8: including the negative one");
    // And the result is genuinely inside the ceiling.
    for (int k = 0; k < 3; ++k) {
      const std::int64_t a = r.direction[k] < 0 ? -std::int64_t(r.direction[k]) : r.direction[k];
      check(a < kNormalReduceCeiling, "case8: every reduced component is under 2^30");
    }

    // A normal already in range is left EXACTLY alone -- no gratuitous shift.
    Inputs small = baseInputs();
    small.attributes[3] = 7 << 16;
    const Result rs = GeomWarp::evaluate(at, small, boundOf(1 << 30));
    check_eq_i64(rs.normal_shifts, 0, "case8: an in-range normal is not shifted at all");
    check_eq_i64(rs.direction[0], 7 << 16, "case8: and passes through bit-exact");
  }

  // ================================================================ T09 =====
  // "Sparse/noncontiguous/high output registers export the correct six values."
  //
  // The REFERENCE handles this because the canonical decoder owns the output
  // map. The composed hardware host does NOT -- it captures a single contiguous
  // window from `hdr_outbase`. This check therefore passes here and stands as
  // the reference half of a prerequisite the shared Field host still owes.
  {
    const zfield::Decoded sp = mustDecode(fixtureSparseOutputs(), "SPARSE_OUTPUTS");
    check(check_signature(sp) == Refusal::kNone, "case9: a sparse output map is a legal signature");
    Inputs in = baseInputs();
    in.params[0] = 1 << 16;
    in.params[1] = 2 << 16;
    in.params[2] = 3 << 16;
    const Result r = GeomWarp::evaluate(sp, in, boundOf(1 << 20));
    check(!r.poison(), "case9: sparse outputs evaluate cleanly");
    check_eq_i64(r.displacement[0], 1 << 16, "case9: dx read from R21, not R15");
    check_eq_i64(r.displacement[1], 2 << 16, "case9: dy read from R34");
    check_eq_i64(r.displacement[2], 3 << 16, "case9: dz read from R47");
    check_eq_i64(r.direction[0], baseInputs().direction[0], "case9: nx' read from R55");
    check_eq_i64(r.direction[1], baseInputs().direction[1], "case9: ny' read from R60");
    check_eq_i64(r.direction[2], baseInputs().direction[2], "case9: nz' read from R63");
  }

  // ================================================================ T11 =====
  // "Canonical Status.sat and Status.rcp0 match the prepared and RTL paths."
  //
  // The half this test owns is that they are TRANSPORTED and kept SEPARATE
  // from the application's own status, and that neither poisons. Directive
  // 5.5: canonical saturation and reciprocal-of-zero "are not automatically
  // malformed programs... A check of `resp_status != 0` MUST NOT turn a
  // defined saturated Field answer into a dropped vertex."
  {
    const zfield::Decoded stf = mustDecode(fixtureStatus(), "STATUS");
    const std::int32_t kMax = 0x7FFFFFFF;
    Inputs in = baseInputs();
    in.position[0] = 0;       // keep the POSITION far from its own rail
    in.position[1] = 0;
    in.params[0] = kMax;      // p0 + p0 saturates inside the FIELD program
    in.attributes[0] = 0;     // RCP(0) -> the canonical rcp0 answer
    zref::SatLedger ledger{};
    const Result r = GeomWarp::evaluate(stf, in, boundOf(kMax), &ledger);

    check(!r.poison(),
          "case12: a saturated / rcp0 program is a DEFINED result, never a poison (5.5)");
    check(r.field_status.sat, "case12: the FIELD program's saturation is transported");
    check(r.field_status.rcp0, "case12: and so is rcp0, on its own flag");
    check(!r.app_saturated,
          "case12: while the APPLICATION flag stays CLEAR -- the two are independent (5.3)");
    check(ledger.total() != 0,
          "case12: the per-cause SatLedger is populated, not just the collapsed Status (12.4)");
    // The position still advanced by the saturated displacement, exactly.
    check_eq_i64(r.position[0], kMax, "case12: the defined saturated value IS applied");
    check(!r.degenerate, "case12: and the normal is untouched by any of it");
  }

  // =============================================== the application split =====
  // W13 keeps the application and the evaluation as SEPARATE claims, so
  // `apply_outputs` must produce the identical result when handed the same six
  // words by any other means -- which is how an RTL differential will use it.
  {
    const zfield::Decoded tr = mustDecode(fixtureTranslate(), "TRANSLATE");
    Inputs in = baseInputs();
    in.params[0] = 3 << 16;
    in.params[1] = -(5 << 16);
    in.params[2] = 8 << 16;
    const Result whole = GeomWarp::evaluate(tr, in, boundOf(1 << 20));
    const std::int32_t out6[6] = {3 << 16,          -(5 << 16),         8 << 16,
                                  in.direction[0],  in.direction[1],  in.direction[2]};
    const Result split = GeomWarp::applyOutputs(out6, in, boundOf(1 << 20), whole.field_status);
    for (int k = 0; k < 3; ++k) {
      check_eq_i64(split.position[k], whole.position[k], "case10: split application agrees");
      check_eq_i64(split.direction[k], whole.direction[k], "case10: split normal agrees");
    }
    check(split.refusal == whole.refusal, "case10: and so does the classification");
  }

  // ========================================== lane packing is the LAW's order =
  // Directive 21.2: "Array order in the semantic record is the section-5 lane
  // table, not the C++ struct's incidental memory layout."
  {
    Inputs in;
    for (int k = 0; k < 3; ++k) in.position[k] = 1000 + k;
    for (int k = 0; k < 3; ++k) in.direction[k] = 2000 + k;
    for (int k = 0; k < 4; ++k) in.attributes[k] = 3000 + k;
    for (int k = 0; k < 4; ++k) in.params[k] = 4000 + k;
    in.time = 0xFFFFFFFFu;
    std::int32_t rec[15];
    pack_record(in, rec);
    check_eq_i64(rec[0], 1000, "case11: lane 0 is px");
    check_eq_i64(rec[2], 1002, "case11: lane 2 is pz");
    check_eq_i64(rec[3], 2000, "case11: lane 3 is nx");
    check_eq_i64(rec[5], 2002, "case11: lane 5 is nz");
    check_eq_i64(rec[6], 3000, "case11: lane 6 is a0");
    check_eq_i64(rec[9], 3003, "case11: lane 9 is a3");
    check_eq_i64(static_cast<std::uint32_t>(rec[10]), 0xFFFFFFFFu,
                 "case11: lane 10 is time, bit-cast not converted");
    check_eq_i64(rec[11], 4000, "case11: lane 11 is p0");
    check_eq_i64(rec[14], 4003, "case11: lane 14 is p3 -- THE FIFTEENTH LANE");
  }

  std::printf("geom_warp_reference_directed: %d checks, %d failed\n", g_checks, g_fail);
  if (g_fail != 0) {
    std::printf("REFERENCE DIRECTED TEST FAILED\n");
    return 1;
  }
  std::printf("PASS\n");
  return 0;
}
