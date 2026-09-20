// warp_field_chain_directed.cpp -- GEOM.WARP AS A REAL CLIENT OF THE REAL
// SHARED FIELD FABRIC. The value is followed all the way through.
//
// `zhao_geom_warp` -> `zhao_field_warp_adapter` -> `zhao_field_host_v2`, all
// three real, wired in `tests/field/tb_warp_field_chain.sv`. A real Warp
// program is loaded into a real resident slot, a real post-skin vertex is
// offered, the real v3 engine executes it, the six canonical output words come
// back, and the application law publishes one coherent warped vertex.
//
// WHY THIS FILE EXISTS. Before it, three green suites each tested one module
// against a hand-built stand-in for its neighbour: the adapter's test stubbed
// the host's response, the warp block's test played the adapter, the host's
// test drove its client ports from C++. None of them was evidence about the
// SEAM, and the seam is where the ordinal-versus-window confusion lives.
//
// THE DIFFERENTIAL IS DOUBLE, AND THAT IS THE POINT.
// Each program is built TWICE from ONE source of truth:
//   * as a real `.zprog` image, pushed through the real `zfield::decode`
//     validator and evaluated by `zref::geom_warp::apply`, which calls
//     `zfield::interpret`; and
//   * as host loader words -- uops, OUTPUT_MAP rows, an association, a header.
// So the comparison is not "does the hardware match a C++ restatement of the
// application law" (that is geom_warp_rtl_directed's job). It is "do the
// SOFTWARE ENGINE and the SILICON ENGINE, given the same program and the same
// vertex, produce the same warped vertex". Either one being wrong shows up.
//
// THE CASE THAT DISCRIMINATES, AND WHY IT IS HERE.
// `SPARSE` puts Warp's six canonical outputs at R15,R16,R17,R18,R19 and **R21**,
// skipping R20. Window lane 5 is therefore never written and window lane 6 is.
// If `resp_out_o` were window-indexed -- which is what the OLD `zhao_field_host`
// returns, and which the adapter's port list cannot distinguish from the new
// host's ordinal-indexed one -- then ordinal 5 would read window lane 5, the
// cleared zero, and the published normal's z would silently be 0 instead of the
// value the program computed.
//
// That is the whole reason this case exists. It is not a corner: it is the ONE
// stimulus that tells a correct wiring from a wiring that elaborates, runs,
// passes every existing gate and returns a plausible wrong number. A contiguous
// program cannot tell them apart, because for a contiguous program the window
// index and the ordinal index are the same integer.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_warp_field_chain.h"
#include "zhao_sim.hpp"
#include "zfield/zfield.hpp"
#include "zhao_abi.h"
#include "zref/zref_geom_warp.hpp"

namespace gw = zref::geom_warp;

using Dut = Vtb_warp_field_chain;

namespace {

// ---------------------------------------------------------------- assembly --
// Bit-identical to geom_warp_reference_directed.cpp's `insWord` and to
// field_host_v2_directed.cpp's `instr()` low word. That the two agree is what
// makes one program describable to both engines.
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

std::vector<std::uint8_t> buildProgram(std::uint8_t profile, const std::vector<Ins>& code,
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

/**
 * Warp's lane table. `out_regs` is where the six canonical outputs live --
 * R15..R20 for a contiguous program, and something else for SPARSE. Making it a
 * parameter rather than a constant is what lets one builder produce both, so
 * the sparse case differs from the contiguous one in exactly one respect.
 */
std::vector<Lane> warpLanes(const std::uint8_t out_regs[6]) {
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

/** One program, in both of the forms the two engines need. */
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
  /** Ordinal mask: six declared ordinals. A DIFFERENT quantity, same width by
   *  coincidence here -- which is exactly why they get different names. */
  std::uint8_t ord_mask() const { return 0x3F; }
};

Prog makeProg(const char* name, const std::vector<Ins>& code, const std::uint8_t out_regs[6]) {
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

// ------------------------------------------------------------ host loading --
// Kinds: 0 UOP, 1 TABLE, 2 HEADER, 3 UNIFORM, 4 OUTMAP, 5 ASSOC, 6 INITPROOF,
// 7 PREPARED. HEADER is written LAST and is the write that marks the slot
// runnable -- every other kind clears `hdr_loaded` again.
constexpr std::uint8_t kLdUop = 0, kLdHeader = 2, kLdOutMap = 4, kLdAssoc = 5;
constexpr std::uint8_t kLdInitProof = 6;
constexpr std::uint8_t kSrcVectorReg = 0;

void setLdData(Dut& d, std::uint64_t lo64, std::uint32_t hi32) {
  d.ld_data_i[0] = static_cast<std::uint32_t>(lo64 & 0xFFFFFFFFu);
  d.ld_data_i[1] = static_cast<std::uint32_t>(lo64 >> 32);
  d.ld_data_i[2] = hi32;
}

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

std::uint64_t headerWord(std::uint8_t instr_count, std::uint8_t out_base, std::uint8_t win_mask,
                         std::uint8_t ord_mask, std::uint8_t out_count, std::uint8_t form) {
  std::uint64_t w = static_cast<std::uint64_t>(instr_count) |
                    (static_cast<std::uint64_t>(out_base & 0x3F) << 8);
  w |= (static_cast<std::uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<std::uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<std::uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<std::uint64_t>(form) & 0x03ull) << 60;
  return w;
}

/**
 * Lower one Prog into the host's loader words. HEADER strictly last -- it is the
 * write that marks the slot runnable, and every other kind clears `hdr_loaded`
 * again, so an INIT_PROOF written after the header would silently un-arm the
 * slot rather than enable anything.
 *
 * `init_proof` is P9's lever. With it accepted the host skips E_ZERO's blanket
 * per-register walk (FH08); without it the host walks. The bench runs both and
 * measures the difference in clocks, because "the fast path exists in the
 * source" and "the fast path fires" are different claims.
 */
bool loadProg(Dut& d, std::uint8_t slot, const Prog& p, bool init_proof = false) {
  for (std::size_t pc = 0; pc < p.code.size(); ++pc) {
    const std::uint64_t uop =
        (static_cast<std::uint64_t>(p.code[pc].imm) << 32) | p.code[pc].word;
    if (!loadWord(d, kLdUop, slot, static_cast<std::uint32_t>(pc), uop)) return false;
  }
  // One OUTPUT_MAP row per canonical ORDINAL. `addr` is the ordinal; the data's
  // low 16 bits are the physical source register. THIS is the translation the
  // whole repair is about, and it is written ordinal-first on purpose.
  for (std::uint8_t ord = 0; ord < 6; ++ord) {
    const std::uint64_t row = static_cast<std::uint64_t>(p.out_regs[ord]) |
                              (static_cast<std::uint64_t>(kSrcVectorReg & 1) << 16);
    if (!loadWord(d, kLdOutMap, slot, ord, row)) return false;
  }
  if (!loadWord(d, kLdAssoc, slot, 0, 1ull)) return false;
  if (init_proof && !loadWord(d, kLdInitProof, slot, 0, 1ull)) return false;
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

void setW128(VlWide<4>& w, const std::int32_t v[4]) {
  for (int i = 0; i < 4; ++i) w[i] = static_cast<std::uint32_t>(v[i]);
}

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

/** The SOFTWARE engine's answer for the same program and vertex. */
gw::Result softwareAnswer(const Prog& p, const Stim& s) {
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

void compareEngines(const char* what, const Prog& p, const Stim& s, const Outcome& o) {
  const gw::Result r = softwareAnswer(p, s);
  zhao::check(p.decoded_ok, "the program decoded through the real validator", 1,
              p.decoded_ok ? 1 : 0);
  zhao::check(!r.poison(), "the software engine accepted this vertex", 0, r.poison() ? 1 : 0);
  zhao::check(o.published, what, 1, o.published ? 1 : 0);
  if (!o.published) return;
  for (int k = 0; k < 3; ++k) {
    zhao::check(o.p[k] == r.position[k], what, static_cast<std::uint32_t>(r.position[k]),
                static_cast<std::uint32_t>(o.p[k]));
    zhao::check(o.n[k] == r.direction[k], what, static_cast<std::uint32_t>(r.direction[k]),
                static_cast<std::uint32_t>(o.n[k]));
  }
}

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

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  resetAll(top);

  const std::uint8_t kContig[6] = {15, 16, 17, 18, 19, 20};
  // SPARSE: ordinal 5 lives at R21, so window lane 5 (R20) is NEVER written.
  const std::uint8_t kSparse[6] = {15, 16, 17, 18, 19, 21};

  std::vector<Ins> ident;
  ident.push_back({insWord(zfield::OP_LDC, 15), 0});
  ident.push_back({insWord(zfield::OP_LDC, 16), 0});
  ident.push_back({insWord(zfield::OP_LDC, 17), 0});
  ident.push_back({insWord(zfield::OP_MOV, 18, R_NX), 0});
  ident.push_back({insWord(zfield::OP_MOV, 19, R_NY), 0});
  ident.push_back({insWord(zfield::OP_MOV, 20, R_NZ), 0});
  ident.push_back({insWord(zfield::OP_END, 0), 0});
  const Prog IDENTITY = makeProg("IDENTITY", ident, kContig);

  std::vector<Ins> trans;
  trans.push_back({insWord(zfield::OP_MOV, 15, R_P0), 0});
  trans.push_back({insWord(zfield::OP_MOV, 16, R_P1), 0});
  trans.push_back({insWord(zfield::OP_MOV, 17, R_P2), 0});
  trans.push_back({insWord(zfield::OP_MOV, 18, R_NX), 0});
  trans.push_back({insWord(zfield::OP_MOV, 19, R_NY), 0});
  trans.push_back({insWord(zfield::OP_MOV, 20, R_NZ), 0});
  trans.push_back({insWord(zfield::OP_END, 0), 0});
  const Prog TRANSLATE = makeProg("TRANSLATE", trans, kContig);

  std::vector<Ins> sparse;
  sparse.push_back({insWord(zfield::OP_MOV, 15, R_P0), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 16, R_P1), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 17, R_P2), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 18, R_NX), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 19, R_NY), 0});
  sparse.push_back({insWord(zfield::OP_MOV, 21, R_NZ), 0});  // ordinal 5 at R21
  sparse.push_back({insWord(zfield::OP_END, 0), 0});
  const Prog SPARSE = makeProg("SPARSE", sparse, kSparse);

  // ---- 1. IDENTITY, end to end through the real engine ---------------------
  {
    zhao::check(loadProg(top, 0, IDENTITY), "IDENTITY loaded into resident slot 0", 1, 1);
    Stim s{};
    s.p[0] = 1234567;
    s.p[1] = -98765;
    s.p[2] = 42;
    s.n[0] = 1000;
    s.n[1] = -2000;
    s.n[2] = 3000;
    s.src_id = 11;
    const std::uint32_t g0 = top.h_grants_o;
    const Outcome o = runVertex(top, 0, s);
    compareEngines("IDENTITY: silicon agrees with the software engine", IDENTITY, s, o);
    zhao::check(o.p[0] == 1234567 && o.p[1] == -98765 && o.p[2] == 42,
                "IDENTITY leaves the world position bit-for-bit unchanged", 1234567,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(o.n[0] == 1000 && o.n[1] == -2000 && o.n[2] == 3000,
                "IDENTITY carries the input normal through unchanged", 1000,
                static_cast<std::uint32_t>(o.n[0]));
    zhao::check(top.h_grants_o > g0, "the HOST actually granted this point -- it ran", 1,
                top.h_grants_o > g0 ? 1 : 0);
    zhao::check(top.h_runs_o >= 1, "the host retired a COMPLETE point", 1,
                top.h_runs_o >= 1 ? 1 : 0);
    zhao::check(top.h_out_incomplete_o == 0,
                "and not a partial one -- every declared ordinal was produced", 0,
                top.h_out_incomplete_o);
    std::printf("  IDENTITY: %d clocks, host window=0x%02X present=0x%02X count=%u\n", o.clocks,
                top.h_resp_window_o, top.h_resp_present_o, top.h_resp_count_o);
  }

  // ---- 2. TRANSLATE: a real non-identity displacement ----------------------
  {
    zhao::check(loadProg(top, 1, TRANSLATE), "TRANSLATE loaded into resident slot 1", 1, 1);
    Stim s{};
    s.p[0] = 1000;
    s.p[1] = 2000;
    s.p[2] = 3000;
    s.n[0] = 5;
    s.n[1] = 6;
    s.n[2] = 7;
    s.params[0] = 111;
    s.params[1] = -222;
    s.params[2] = 333;
    s.src_id = 12;
    const Outcome o = runVertex(top, 1, s);
    compareEngines("TRANSLATE: silicon agrees with the software engine", TRANSLATE, s, o);
    zhao::check(o.p[0] == 1111 && o.p[1] == 1778 && o.p[2] == 3333,
                "TRANSLATE moved the vertex by the program's p0,p1,p2 -- a REAL warp, "
                "computed by the REAL v3 engine",
                1111, static_cast<std::uint32_t>(o.p[0]));
    std::printf("  TRANSLATE: %d clocks, position (%d,%d,%d)\n", o.clocks, o.p[0], o.p[1],
                o.p[2]);
  }

  // ---- 3. SPARSE OUTPUTS -- THE CASE THAT DISCRIMINATES --------------------
  // Ordinal 5 lives at R21, so window lane 5 is never written. A window-indexed
  // read would return the cleared zero for the normal's z.
  {
    zhao::check(loadProg(top, 2, SPARSE), "SPARSE loaded into resident slot 2", 1, 1);
    Stim s{};
    s.p[0] = 10;
    s.p[1] = 20;
    s.p[2] = 30;
    s.n[0] = 77;
    s.n[1] = 88;
    s.n[2] = 99;  // the value that must survive the sparse map
    s.params[0] = 1;
    s.params[1] = 2;
    s.params[2] = 3;
    s.src_id = 13;
    const std::uint32_t inc0 = top.h_out_incomplete_o;
    const std::uint32_t bad0 = top.h_bad_image_o;
    const Outcome o = runVertex(top, 2, s);
    compareEngines("SPARSE: silicon agrees with the software engine", SPARSE, s, o);
    zhao::check(o.n[2] == 99,
                "SPARSE: ordinal 5 came from R21 and NOT from the unwritten window lane 5. "
                "This is the one case that tells an ordinal-indexed response from a "
                "window-indexed one; a contiguous program cannot.",
                99, static_cast<std::uint32_t>(o.n[2]));
    zhao::check(o.p[0] == 11 && o.p[1] == 22 && o.p[2] == 33,
                "SPARSE: the displacement ordinals are unaffected", 11,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(top.h_out_incomplete_o == inc0,
                "SPARSE produced ALL SIX declared ordinals -- no partial result", inc0,
                top.h_out_incomplete_o);
    zhao::check(top.h_bad_image_o == bad0,
                "and the sparse OUTPUT_MAP was NOT a load-time refusal: every source "
                "register lies inside the capture window",
                bad0, top.h_bad_image_o);
    // The window mask reports lane 5 as claimed-and-unwritten only if some
    // ordinal named it; here none does, so the host reports it as PADDING.
    std::printf("  SPARSE: window=0x%02X present=0x%02X count=%u, normal z=%d\n",
                top.h_resp_window_o, top.h_resp_present_o, top.h_resp_count_o, o.n[2]);
    zhao::check(top.h_resp_present_o == 0x3F,
                "all six ORDINALS present (an ordinal-indexed mask, not a window one)", 0x3F,
                top.h_resp_present_o);
  }

  // ---- 4. W09: the bypass performs ZERO Field work -------------------------
  // Measured on the HOST's own grant counter, which is as far from the bypass
  // decision as this bench can get. "It never asked" is a property of the whole
  // chain, not of the block that decided.
  {
    const std::uint32_t g0 = top.h_grants_o;
    const std::uint32_t x0 = top.w_transformed_o;
    Stim s{};
    s.warp_en = false;
    s.p[0] = 77;
    s.p[1] = -88;
    s.p[2] = 99;
    s.n[0] = 11;
    s.n[1] = 22;
    s.n[2] = 33;
    s.src_id = 14;
    const Outcome o = runVertex(top, 0, s);
    zhao::check(o.published, "the bypassed vertex is published", 1, o.published ? 1 : 0);
    zhao::check(o.p[0] == 77 && o.n[0] == 11, "and is carried through exactly", 77,
                static_cast<std::uint32_t>(o.p[0]));
    zhao::check(top.h_grants_o == g0,
                "W09 MEASURED AT THE HOST: a disabled draw caused ZERO grants on the "
                "shared Field fabric",
                g0, top.h_grants_o);
    zhao::check(top.w_transformed_o == x0,
                "and `vertices_transformed` did not move -- bypasses are not applications",
                x0, top.w_transformed_o);
  }

  // ---- 4b. P9 / R91's LEVER, FIRED AND MEASURED ----------------------------
  // R103 records that "the uniforms once per association fast path of ruling
  // R91 does NOT exist anywhere in the tree." It exists now, in
  // zhao_field_host_v2, gated on an accepted INIT_PROOF -- but a lever present
  // in the source and a lever that FIRES are different claims, and a counter
  // reading zero is the claim to check hardest.
  //
  // So: the same IDENTITY program, loaded twice into two slots, differing in
  // exactly one loader word. The slow slot walks every register (E_ZERO); the
  // fast slot skips it. Both must produce the IDENTICAL vertex -- a fast path
  // that changed the answer would be a defect, not an optimisation -- and the
  // clock counts must differ.
  {
    const std::uint32_t fast0 = top.h_fast_path_o;
    const std::uint32_t slow0 = top.h_slow_path_o;

    Stim s{};
    s.p[0] = 555;
    s.p[1] = -666;
    s.p[2] = 777;
    s.n[0] = 4242;
    s.n[1] = -4242;
    s.n[2] = 1;
    s.src_id = 21;

    zhao::check(loadProg(top, 4, IDENTITY, /*init_proof=*/false),
                "IDENTITY loaded into slot 4 WITHOUT an INIT_PROOF", 1, 1);
    const Outcome slow = runVertex(top, 4, s);
    const std::uint32_t after_slow_fast = top.h_fast_path_o;
    const std::uint32_t after_slow_slow = top.h_slow_path_o;

    zhao::check(loadProg(top, 5, IDENTITY, /*init_proof=*/true),
                "IDENTITY loaded into slot 5 WITH an accepted INIT_PROOF", 1, 1);
    const Outcome fast = runVertex(top, 5, s);

    zhao::check(after_slow_slow == slow0 + 1,
                "the un-proved slot took the SLOW path (E_ZERO walked)", slow0 + 1,
                after_slow_slow);
    zhao::check(after_slow_fast == fast0,
                "and did NOT take the fast path", fast0, after_slow_fast);
    zhao::check(top.h_fast_path_o == fast0 + 1,
                "R91'S LEVER FIRES: the INIT_PROOF slot took the NO-CLEAR FAST PATH",
                fast0 + 1, top.h_fast_path_o);
    zhao::check(top.h_slow_path_o == after_slow_slow,
                "and the slow-path counter did not move for it", after_slow_slow,
                top.h_slow_path_o);

    // The answer must be identical. This is the half that makes the lever a
    // saving rather than a shortcut.
    zhao::check(slow.published && fast.published, "both slots published a vertex", 2,
                (slow.published ? 1 : 0) + (fast.published ? 1 : 0));
    bool same = true;
    for (int k = 0; k < 3; ++k) {
      if (slow.p[k] != fast.p[k] || slow.n[k] != fast.n[k]) same = false;
    }
    zhao::check(same,
                "and the fast path produced the BIT-IDENTICAL vertex -- it is a saving, "
                "not a different machine",
                1, same ? 1 : 0);
    compareEngines("the fast path still agrees with the software engine", IDENTITY, s, fast);

    std::printf("  P9 / R91 LEVER MEASURED: slow slot %d clocks, fast slot %d clocks "
                "(delta %d); fast_path=%u slow_path=%u\n",
                slow.clocks, fast.clocks, slow.clocks - fast.clocks, top.h_fast_path_o,
                top.h_slow_path_o);
    zhao::check(fast.clocks < slow.clocks,
                "the fast path is actually FASTER, in clocks, on the same program and "
                "the same vertex -- R91's lever is a number now, not a direction",
                1, fast.clocks < slow.clocks ? 1 : 0);
  }

  // ---- 5. conservation across all three modules ----------------------------
  // Contract section 10. Three independently-maintained counters in three
  // separately-written modules; if the chain were dropping or duplicating work,
  // these would disagree. One counter could not see it.
  {
    zhao::check(top.w_p_accepts_o == top.w_n_accepts_o,
                "position accepts == normal accepts (the fork took each exactly once)",
                top.w_n_accepts_o, top.w_p_accepts_o);
    // THE FIRST VERSION OF THIS ASSERTION WAS WRONG, AND THE BENCH TAUGHT ME
    // THE LAW. I wrote `a_vertices == w_transformed + w_bypassed` and measured
    // 3 against 4. The adapter NEVER SEES a bypassed vertex, because
    // `zhao_geom_warp` gates `f_vtx_valid_o` on `warp_en_q` -- which is W09
    // working exactly as specified. So the correct conservation is:
    //
    //   adapter vertices          == Warp APPLICATIONS          (enabled only)
    //   applications + bypasses   == published vertex outcomes  (the whole stream)
    //
    // AND A REAL HAZARD FOUND BY THE SAME MISCOMPARE: the adapter ALSO has a
    // counter called `bypassed_o`, and it counts a DIFFERENT EVENT from
    // `zhao_geom_warp`'s `bypassed_o` -- it reads 0 here while the application's
    // reads 1. Two counters, the same name, two quantities, in modules wired
    // directly together. Reading the wrong one would have been very easy and
    // would have looked fine.
    zhao::check(top.a_vertices_o == top.w_transformed_o,
                "adapter vertices == Warp APPLICATIONS (a bypass is never offered)",
                top.w_transformed_o, top.a_vertices_o);
    zhao::check(top.w_transformed_o + top.w_bypassed_o == top.w_p_accepts_o,
                "applications + bypasses == published vertex outcomes",
                top.w_p_accepts_o, top.w_transformed_o + top.w_bypassed_o);
    zhao::check(top.h_runs_o == top.w_transformed_o,
                "host COMPLETE retirements == successful Warp applications", top.w_transformed_o,
                top.h_runs_o);
    zhao::check(top.h_late_write_o == 0,
                "the host's retirement fence saw no late write (its own positive control "
                "lives in the host's directed test, not here)",
                0, top.h_late_write_o);
    zhao::check(top.h_zero_mask_o == 0, "no descriptor was loaded with a zero required mask", 0,
                top.h_zero_mask_o);
    zhao::check(top.h_noprog_o == 0 && top.a_noprog_o == 0,
                "every request found its resident program", 0, top.h_noprog_o + top.a_noprog_o);
    zhao::check(top.w_field_faults_o == 0, "no transport fault poisoned a vertex", 0,
                top.w_field_faults_o);
  }

  std::printf(
      "  CHAIN: host runs=%u grants=%u incomplete=%u no_result=%u bad_image=%u\n"
      "         fast_path=%u slow_path=%u credit_stall=%u prep_bad=%u num_status=0x%X\n"
      "         adapter vertices=%u identities=%u bypassed=%u faults=%u stalls=%u\n"
      "         warp transformed=%u bypassed=%u p_acc=%u n_acc=%u\n",
      top.h_runs_o, top.h_grants_o, top.h_out_incomplete_o, top.h_no_result_o, top.h_bad_image_o,
      top.h_fast_path_o, top.h_slow_path_o, top.h_credit_stall_o, top.h_prep_bad_o,
      top.h_num_status_o, top.a_vertices_o, top.a_identities_o, top.a_bypassed_o,
      top.a_faults_o, top.a_stall_cycles_o, top.w_transformed_o, top.w_bypassed_o,
      top.w_p_accepts_o, top.w_n_accepts_o);

  return zhao::report_and_exit("warp_field_chain_directed");
}
