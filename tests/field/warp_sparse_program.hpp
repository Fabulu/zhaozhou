// warp_sparse_program.hpp -- THE SPARSE STIMULUS, IN ONE PLACE, FOR BOTH
// POLARITIES.
//
// Owner ruling R168 requires a NAMED test that tells a correctly-wired Warp
// adapter from one reading window positions as ordinals, and it requires that
// test to be SHOWN to discriminate -- which means running the same stimulus
// against a deliberately wrong arrangement and watching it fail.
//
// "The same stimulus" is the load-bearing phrase. If the real driver and the
// mutant driver each built their own program, a difference between them could
// come from the programs rather than from the wiring, and the control would be
// evidence about nothing. So the program, the vertex, the loader words and the
// software oracle all live HERE, and the two drivers differ only in which DUT
// they instantiate and what they conclude from the result.
//
// ---------------------------------------------------------------------------
// WHY SPARSE, AND WHY NOTHING ELSE WILL DO
// ---------------------------------------------------------------------------
// `required_mask` is indexed by canonical output ORDINAL. `hdr_outreq` and
// `window_mask` are indexed by contiguous capture-WINDOW position. `OUTPUT_MAP`
// is the translation between them. Two indexings, the same width, similar
// names.
//
// FOR A CONTIGUOUS PROGRAM THEY ARE THE SAME INTEGER. So a contiguous program
// is structurally incapable of telling a correct wiring from a wrong one -- and
// every other Warp test in this tree is contiguous. That is CLAUDE.md's "a gate
// that cannot reach the state is not evidence about the state", and it is the
// measured reason R168's defect survived review, elaboration, lint, Quartus
// syntax, and a green directed test per module.
//
// SPARSE puts Warp's six canonical outputs at R15, R16, R17, R18, R19 and R21.
// Ordinal 5 therefore lives at R21, and window lane 5 -- R20 -- is NEVER
// WRITTEN. An ordinal-indexed response returns the program's nz'; a
// window-indexed one returns the zero that was cleared at grant. One vertex
// separates them.
//
// The structure here is W1's, carried over from
// tests/field/warp_field_chain_directed.cpp case 3 so that the named test is
// the same experiment W1 ran and not a re-derivation of it.

#ifndef ZHAO_TESTS_FIELD_WARP_SPARSE_PROGRAM_HPP
#define ZHAO_TESTS_FIELD_WARP_SPARSE_PROGRAM_HPP

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "zhao_sim.hpp"
#include "zfield/zfield.hpp"
#include "zhao_abi.h"
#include "zref/zref_geom_warp.hpp"

namespace warp_sparse {

namespace gw = zref::geom_warp;

// ---------------------------------------------------------------- assembly --
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

inline std::vector<std::uint8_t> buildProgram(std::uint8_t profile, const std::vector<Ins>& code,
                                              const std::vector<Lane>& lanes) {
  std::vector<std::uint8_t> map, namepool;
  for (std::size_t i = 0; i < lanes.size(); ++i) {
    map.push_back(lanes[i].reg);
    map.push_back(lanes[i].kind);
    map.push_back(lanes[i].type);
    map.push_back(static_cast<std::uint8_t>(i));
    map.insert(map.end(), 8, 0);
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
  u32(0x5049465Au);
  u16(1);
  prog.push_back(profile);
  prog.push_back(0);
  u32(0);
  u16(static_cast<std::uint16_t>(code.size()));
  prog.push_back(0);
  prog.push_back(static_cast<std::uint8_t>(lanes.size()));
  u16(0);
  u16(static_cast<std::uint16_t>(map_bytes));
  u32(0);
  u32(0);
  for (const Ins& in : code) {
    u32(in.word);
    u32(in.imm);
  }
  prog.insert(prog.end(), map.begin(), map.end());
  prog.insert(prog.end(), srcmap.begin(), srcmap.end());
  prog.insert(prog.end(), namepool.begin(), namepool.end());

  const std::uint8_t* codep = prog.data() + zfield::ZPROG_HEADER_BYTES;
  std::uint32_t h = zhao_abi::zhao_crc32c(0, codep, 8 * code.size());
  h = zhao_abi::zhao_crc32c(h, codep + 8 * code.size(), 0);
  h += static_cast<std::uint32_t>(code.size());
  for (int i = 0; i < 4; ++i) prog[20 + i] = std::uint8_t(h >> (8 * i));
  const std::uint32_t bc = zhao_abi::zhao_crc32c(0, prog.data(), prog.size());
  for (int i = 0; i < 4; ++i) prog[24 + i] = std::uint8_t(bc >> (8 * i));
  return prog;
}

constexpr std::uint8_t R_NX = 3, R_NY = 4, R_NZ = 5;
constexpr std::uint8_t R_P0 = 11, R_P1 = 12, R_P2 = 13;

inline std::vector<Lane> warpLanes(const std::uint8_t out_regs[6]) {
  static const char* kInNames[15] = {"px", "py", "pz", "nx",   "ny", "nz", "a0", "a1",
                                     "a2", "a3", "time", "p0", "p1", "p2", "p3"};
  static const char* kOutNames[6] = {"dx", "dy", "dz", "nxo", "nyo", "nzo"};
  std::vector<Lane> v;
  for (std::size_t i = 0; i < 15; ++i) {
    v.push_back(Lane{static_cast<std::uint8_t>(i), 0,
                     static_cast<std::uint8_t>(i == 10 ? 3 : 0), kInNames[i]});
  }
  for (std::size_t i = 0; i < 6; ++i) v.push_back(Lane{out_regs[i], 1, 0, kOutNames[i]});
  return v;
}

struct Prog {
  const char* name = "";
  std::vector<Ins> code;
  std::uint8_t out_regs[6] = {15, 16, 17, 18, 19, 20};
  zfield::Decoded decoded{};
  bool decoded_ok = false;

  std::uint8_t out_base() const {
    std::uint8_t lo = out_regs[0];
    for (int i = 1; i < 6; ++i)
      if (out_regs[i] < lo) lo = out_regs[i];
    return lo;
  }
  /** Window mask: bit (reg - out_base) for every register an ordinal names. */
  std::uint8_t win_mask() const {
    std::uint8_t m = 0;
    for (int i = 0; i < 6; ++i) m |= static_cast<std::uint8_t>(1u << (out_regs[i] - out_base()));
    return m;
  }
  /** Ordinal mask: six declared ordinals. A DIFFERENT quantity that happens to
   *  be the same width, which is exactly why it gets a different name. */
  std::uint8_t ord_mask() const { return 0x3F; }
};

inline Prog makeProg(const char* name, const std::vector<Ins>& code,
                     const std::uint8_t out_regs[6]) {
  Prog p;
  p.name = name;
  p.code = code;
  std::memcpy(p.out_regs, out_regs, 6);
  const std::vector<std::uint8_t> img = buildProgram(zfield::WARP, code, warpLanes(out_regs));
  zfield::DecodeResult r = zfield::decode(img.data(), img.size());
  p.decoded_ok = (r.error == zfield::DecodeError::kOk);
  if (!p.decoded_ok) {
    std::printf("FAIL: %s did not decode: %s (%s)\n", name, zfield::decodeErrorName(r.error),
                r.detail.c_str());
  }
  p.decoded = r.prog;
  return p;
}

/**
 * THE DISCRIMINATING PROGRAM. Ordinal 5 at R21; window lane 5 (R20) unwritten.
 *
 * It is built here rather than in either driver so that "the same stimulus"
 * is a property of the code and not a claim in a comment.
 */
inline Prog sparseProgram() {
  static const std::uint8_t kSparse[6] = {15, 16, 17, 18, 19, 21};
  std::vector<Ins> sparse;
  sparse.push_back({insWord(zfield::OP_MOV, 15, R_P0), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 16, R_P1), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 17, R_P2), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 18, R_NX), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 19, R_NY), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 21, R_NZ), 0});  // ordinal 5 at R21
  sparse.push_back({insWord(zfield::OP_END, 0), 0});
  return makeProg("SPARSE", sparse, kSparse);
}

/** A CONTIGUOUS program, ordinal k at R(15+k). The negative control for the
 *  discrimination itself: this one CANNOT tell the two wirings apart, and the
 *  named test asserts that it cannot. */
inline Prog contiguousProgram() {
  static const std::uint8_t kContig[6] = {15, 16, 17, 18, 19, 20};
  std::vector<Ins> trans;
  trans.push_back({insWord(zfield::OP_MOV, 15, R_P0), 0});
  trans.push_back({insWord(zfield::OP_MOV, 16, R_P1), 0});
  trans.push_back({insWord(zfield::OP_MOV, 17, R_P2), 0});
  trans.push_back({insWord(zfield::OP_MOV, 18, R_NX), 0});
  trans.push_back({insWord(zfield::OP_MOV, 19, R_NY), 0});
  trans.push_back({insWord(zfield::OP_MOV, 20, R_NZ), 0});
  trans.push_back({insWord(zfield::OP_END, 0), 0});
  return makeProg("CONTIGUOUS", trans, kContig);
}

// ------------------------------------------------------------ host loading --
// Kinds: 0 UOP, 1 TABLE, 2 HEADER, 3 UNIFORM, 4 OUTMAP, 5 ASSOC, 6 INITPROOF,
// 7 PREPARED. HEADER is written LAST: it is the write that marks the slot
// runnable, and every other kind clears `hdr_loaded` again.
constexpr std::uint8_t kLdUop = 0, kLdHeader = 2, kLdOutMap = 4, kLdAssoc = 5;
constexpr std::uint8_t kSrcVectorReg = 0;

template <typename Dut>
void setLdData(Dut& d, std::uint64_t lo64, std::uint32_t hi32) {
  d.ld_data_i[0] = static_cast<std::uint32_t>(lo64 & 0xFFFFFFFFu);
  d.ld_data_i[1] = static_cast<std::uint32_t>(lo64 >> 32);
  d.ld_data_i[2] = hi32;
}

template <typename Dut>
bool loadWord(Dut& d, std::uint8_t kind, std::uint8_t slot, std::uint32_t addr,
              std::uint64_t lo64) {
  d.ld_valid_i = 1;
  d.ld_kind_i = kind;
  d.ld_slot_i = slot;
  d.ld_addr_i = addr;
  setLdData(d, lo64, 0);
  for (int guard = 0; guard < 40000; ++guard) {
    d.eval();
    if (d.ld_ready_o) {
      zhao::tick(d);
      d.ld_valid_i = 0;
      return true;
    }
    zhao::tick(d);
  }
  d.ld_valid_i = 0;
  return false;
}

inline std::uint64_t headerWord(std::uint8_t instr_count, std::uint8_t out_base,
                                std::uint8_t win_mask, std::uint8_t ord_mask,
                                std::uint8_t out_count, std::uint8_t form) {
  std::uint64_t w = static_cast<std::uint64_t>(instr_count) |
                    (static_cast<std::uint64_t>(out_base & 0x3F) << 8);
  w |= (static_cast<std::uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<std::uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<std::uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<std::uint64_t>(form) & 0x03ull) << 60;
  return w;
}

/** Lower one Prog into the host's loader words. HEADER strictly last. The
 *  OUTPUT_MAP is written ORDINAL-first on purpose: `addr` is the ordinal and
 *  the data's low bits are the physical source register, and that row IS the
 *  translation this whole file exists to exercise. */
template <typename Dut>
bool loadProg(Dut& d, std::uint8_t slot, const Prog& p) {
  for (std::size_t pc = 0; pc < p.code.size(); ++pc) {
    const std::uint64_t uop =
        (static_cast<std::uint64_t>(p.code[pc].imm) << 32) | p.code[pc].word;
    if (!loadWord(d, kLdUop, slot, static_cast<std::uint32_t>(pc), uop)) return false;
  }
  for (std::uint8_t ord = 0; ord < 6; ++ord) {
    const std::uint64_t row = static_cast<std::uint64_t>(p.out_regs[ord]) |
                              (static_cast<std::uint64_t>(kSrcVectorReg & 1) << 16);
    if (!loadWord(d, kLdOutMap, slot, ord, row)) return false;
  }
  if (!loadWord(d, kLdAssoc, slot, 0, 1ull)) return false;
  return loadWord(d, kLdHeader, slot, 0,
                  headerWord(static_cast<std::uint8_t>(p.code.size()), p.out_base(),
                             p.win_mask(), p.ord_mask(), 6, 0));
}

// ----------------------------------------------------------------- driving --
struct Stim {
  std::int32_t p[3] = {0, 0, 0};
  std::int64_t n[3] = {0, 0, 0};
  std::int32_t params[4] = {0, 0, 0, 0};
  std::int32_t bound[3] = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
  bool warp_en = true;
  std::uint16_t src_id = 0;
};

struct Outcome {
  bool published = false;
  std::int32_t p[3] = {0, 0, 0};
  std::int32_t n[3] = {0, 0, 0};
  bool degenerate = false;
  bool poisoned = false;
  int cause = 0;
  int clocks = 0;
};

/** THE VERTEX. n[2] = 99 is the value that must survive the sparse map: it is
 *  produced into R21 and it is what an ordinal-indexed response returns. A
 *  window-indexed response returns R20, which nothing wrote -- zero. */
inline Stim sparseVertex() {
  Stim s{};
  s.p[0] = 10;
  s.p[1] = 20;
  s.p[2] = 30;
  s.n[0] = 77;
  s.n[1] = 88;
  s.n[2] = 99;
  s.params[0] = 1;
  s.params[1] = 2;
  s.params[2] = 3;
  s.src_id = 13;
  return s;
}

template <typename W>
void setW128(W& w, const std::int32_t v[4]) {
  for (int i = 0; i < 4; ++i) w[i] = static_cast<std::uint32_t>(v[i]);
}

template <typename Dut>
Outcome runVertex(Dut& d, std::uint8_t slot, const Stim& s) {
  Outcome o{};
  d.v_px_i = s.p[0];
  d.v_py_i = s.p[1];
  d.v_pz_i = s.p[2];
  d.v_nx_i = s.n[0];
  d.v_ny_i = s.n[1];
  d.v_nz_i = s.n[2];
  d.v_n_degenerate_i = 0;
  d.v_src_id_i = s.src_id;
  const std::int32_t zero4[4] = {0, 0, 0, 0};
  setW128(d.v_attr_i, zero4);
  setW128(d.d_par_i, s.params);
  d.d_warp_en_i = s.warp_en ? 1 : 0;
  d.d_slot_i = slot;
  d.d_slot_valid_i = 1;
  d.d_profile_i = 1;
  d.d_time_i = 0;
  d.d_bx_i = s.bound[0];
  d.d_by_i = s.bound[1];
  d.d_bz_i = s.bound[2];
  d.v_valid_i = 1;

  bool p_done = false, n_done = false, accepted = false;
  for (int guard = 0; guard < 20000; ++guard) {
    d.o_p_ready_i = p_done ? 0 : 1;
    d.o_n_ready_i = n_done ? 0 : 1;
    d.eval();
    if (d.o_p_valid_o && d.o_p_ready_i) {
      o.p[0] = d.o_px_o;
      o.p[1] = d.o_py_o;
      o.p[2] = d.o_pz_o;
      p_done = true;
    }
    if (d.o_n_valid_o && d.o_n_ready_i) {
      o.n[0] = d.o_nx_o;
      o.n[1] = d.o_ny_o;
      o.n[2] = d.o_nz_o;
      o.degenerate = d.o_n_degenerate_o != 0;
      n_done = true;
    }
    if (d.poison_valid_o) {
      o.poisoned = true;
      o.cause = d.poison_cause_o;
    }
    if (d.v_ready_o && d.v_valid_i) accepted = true;
    zhao::tick(d);
    ++o.clocks;
    if (accepted) d.v_valid_i = 0;
    if (o.poisoned) break;
    if (p_done && n_done) break;
  }
  o.published = p_done && n_done;
  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  zhao::tick(d);
  return o;
}

/** The SOFTWARE engine's answer for the same program and vertex. It runs the
 *  real `zfield::interpret` over the real decoded image, so it is an
 *  independent implementation and not a restatement of the RTL. */
inline gw::Result softwareAnswer(const Prog& p, const Stim& s) {
  gw::Inputs in{};
  for (int k = 0; k < 3; ++k) {
    in.position[k] = s.p[k];
    in.direction[k] = static_cast<std::int32_t>(s.n[k]);
  }
  for (int k = 0; k < 4; ++k) in.params[k] = s.params[k];
  gw::Declaration decl{};
  for (int k = 0; k < 3; ++k) decl.displacement_bound[k] = s.bound[k];
  return gw::apply(p.decoded, in, decl);
}

template <typename Dut>
void resetAll(Dut& d) {
  d.rst_n = 0;
  d.cfg_slow_clear_i = 0;
  d.ld_valid_i = 0;
  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  d.d_warp_en_i = 0;
  d.d_slot_valid_i = 0;
  d.d_profile_i = 1;
  for (int i = 0; i < 4; ++i) {
    d.v_attr_i[i] = 0;
    d.d_par_i[i] = 0;
  }
  for (int i = 0; i < 6; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

}  // namespace warp_sparse

#endif  // ZHAO_TESTS_FIELD_WARP_SPARSE_PROGRAM_HPP
