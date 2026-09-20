// zfield_host_plan.hpp -- THE PRODUCTION LOWERER (owner directive
// reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt sections
// 5.2/5.3/5.4, 6.2, 7.1/7.3, 11.3; plan reports/FIELD-REPAIR-PLAN-20260920.md
// section 2; packet L1).
//
// WHAT THIS IS. The logical FPLAN (zfield_plan.hpp) says WHAT to compute. This
// says WHERE it lives on the current v3 fabric: which physical register each
// operand group starts at, which registers the association preloads, which
// registers each point writes, and which register or prepared scalar each
// canonical OUTPUT ORDINAL is exported from. It invents no arithmetic. Every
// numeric answer still comes from zfield::execute_point and zfield::interpret,
// which remain the only semantic authority (directive 5.1: "Do not invent a new
// arithmetic interpreter for C").
//
// WHY IT EXISTS AS A LIBRARY. The Translator, the smoothstep contraction and
// the uniform-broadcast discipline were all written inside ONE TEST
// (tests/differential/field_v3_earth_directed.cpp). The packer that will feed
// real silicon and the benchmark that measures it must lower a program the SAME
// way or they are measuring two different machines. Directive 5.2: one lowerer,
// for the packer and the benchmark both.
//
// ======================================================================
// THE TWO MASKS. READ THIS BEFORE WIRING ANYTHING.
// ======================================================================
//
// This file is where the ordinal/window confusion would do its damage, so it is
// worth restating what R101 and R111 cost:
//
//   RequiredMask is indexed by CANONICAL OUTPUT ORDINAL. Bit j means "canonical
//   output j of this profile is declared". Width = the profile's output count
//   (Earth 4, Warp 6, Flow 7, Stamp 3). It lives in PROGRAM_META.required_mask.
//
//   WindowMask is indexed by CONTIGUOUS CAPTURE-WINDOW POSITION. Bit k means
//   "physical register out_base + k was written". Width = the composed
//   OUT_LANES (the console composes 7). It lives in the loader header word at
//   [32 +: OUT_LANES] (zhao_field_host.sv:299).
//
// They coincide only when the output registers happen to run contiguously from
// out_base, and R111 measured that THEY NEVER DO: the three shipped Earth
// programs carry window masks 0x17 / 0x1D / 0x17, every one of them leaving
// three of seven window lanes unwritten. crater_ring writes R13,R14,R15,R17
// with out_base=13 -- window mask 0x17, ordinal mask 0x0F. Holes are the
// NORMAL case.
//
// S1 made assigning one to the other a COMPILE ERROR rather than a comment
// (zfield/generated/zfield_host_image.hpp). This header keeps that property:
// the two never appear as a bare integer in the same expression, and
// window_mask_of() is the ONLY place the translation happens.
//
// ======================================================================
// WHAT THIS FILE DELIBERATELY DOES NOT DO
// ======================================================================
//
// * It does not insert MOV packing to repair a non-adjacent operand group.
//   Directive 5.3 permits that ("MAY insert exact MOV packing into reserved
//   scratch if it proves space and counts the added instructions") and equally
//   permits falling back to the exact canonical form of 5.5. Neither is built
//   here. A non-adjacent group is REFUSED BY NAME -- it is not aliased onto a
//   neighbouring register and hoped over, which is the failure 5.3 names.
//   kRefusalNonConsecutive carries the same wording the test-local Translator
//   used, so the behaviour is preserved rather than re-litigated.
// * It does not choose the canonical execution form (5.5). A refusal here is
//   the input to that decision, not a substitute for it.
//
// Both are declared non-goals, not silent gaps: refuse() names them.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "zfield/generated/zfield_host_image.hpp"
#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"

namespace zfield {
namespace host_plan {

using host_image::RequiredMask;
using host_image::WindowMask;

// ======================================================================
// NAMED, EDITABLE CONSTANTS (CLAUDE.md rule 6).
//
// Every one of these is a knob, not a derived truth. "It came from the
// reference so it is not a knob" is how a wrong number becomes an
// unadjustable wrong number.
// ======================================================================

/** Physical register file depth. The INIT_PROOF masks are u64 because of it
 *  (directive 11.3: "No u32 mask may silently discard physical registers
 *  32..63"). */
inline constexpr int kPhysicalRegisters = 64;

/** Where the association's BROADCAST uniform region starts. Vector (varying)
 *  registers sit below it; every context holds the same copy of the uniforms
 *  above it, written once per association. This is the "first native mapping"
 *  of directive 5.2 -- varying low, prepared scalars high. */
inline constexpr int kDefaultScalarBase = 32;

/** The composed capture window: zhao_console_core composes OUT_LANES=7. The
 *  WindowMask is this wide. */
inline constexpr int kDefaultOutLanes = host_image::ZFH_WINDOW_MASK_BITS;

/** The 64-bit native uop word, EXACTLY as zhao_field_host.sv:923-928 already
 *  unpacks it. This is NOT a new layout -- inventing a second one is the
 *  failure this repository has shipped more than once. The elaboration guard at
 *  zhao_field_host.sv:506 states the same constraint from the RTL side: four
 *  6-bit register fields, so REGS>64 would overlap them. */
inline constexpr int kUopOpLsb = 0;    // ld_data_i[7:0]
inline constexpr int kUopOpBits = 8;
inline constexpr int kUopDstLsb = 8;   // ld_data_i[8  +: REGW]
inline constexpr int kUopALsb = 14;    // ld_data_i[14 +: REGW]
inline constexpr int kUopBLsb = 20;    // ld_data_i[20 +: REGW]
inline constexpr int kUopCLsb = 26;    // ld_data_i[26 +: REGW]
inline constexpr int kUopRegBits = 6;  // REGW at the REGS=64 ceiling
inline constexpr int kUopImmLsb = 32;  // ld_data_i[63:32]

/** UOP_RING_PREP packs four six-bit scalar-bank indices into bits 0..23 and
 *  carries SMOOTH MODE in bit 24. Directive 5.4 is explicit that some older
 *  comments still call bits 31..24 wholly reserved, and that the live
 *  smooth-mode consumer wins over the stale comment. */
inline constexpr int kRingPrepSlotBits = 6;
inline constexpr int kRingPrepSlotCount = 4;
inline constexpr uint32_t kRingPrepSmoothBit = 1u << 24;
inline constexpr int kScalarBankSlots = 64;

// ======================================================================
// REFUSAL STRINGS
//
// Named so a test can assert the EXACT string rather than "it returned
// false". A refusal that cannot be distinguished from another refusal is
// not evidence about which rule fired.
// ======================================================================

/** Preserved VERBATIM from the test-local Translator
 *  (field_v3_earth_directed.cpp:324). Plan section 6 F4 records that the
 *  existing refusal is CORRECT and inherited, not a hazard to be repaired:
 *  directive 5.3's worry that a mapper "may alias neighboring registers and
 *  hope the numbers agree" was already guarded. Do not weaken this. */
inline constexpr const char* kRefusalNonConsecutive =
    "an operand group whose members are not consecutive registers";
inline constexpr const char* kRefusalUnknownShape =
    "an op shape this translator has not been taught";
inline constexpr const char* kRefusalSrcCount =
    "an op whose flattened source count disagrees with its generated group widths";
inline constexpr const char* kRefusalRingPrepCount = "RING_PREP with an unexpected source count";
inline constexpr const char* kRefusalRingPrepVarying = "RING_PREP with a non-uniform radius/centre";
inline constexpr const char* kRefusalRingPrepSlot = "RING_PREP slot past the 64-slot bank";
inline constexpr const char* kRefusalRegisterRange =
    "a physical register past the register file";
/** Directive 6.2: "Reject the physical image if a read is unsupported by that
 *  set. Never 'repair' a bad proof by setting all registers defined because a
 *  previous context happened to use them." */
inline constexpr const char* kRefusalReadBeforeDef =
    "a physical register read before it was defined by preload or by this point";
/** Plan section 2, case 2: a VECTOR_REG ordinal whose source_index falls
 *  outside the capture window can never be OBSERVED, so the point would hang
 *  or refuse for the wrong reason. This is the case that silently produces a
 *  wrong value today. */
inline constexpr const char* kRefusalOutsideWindow =
    "BAD_IMAGE: an output ordinal whose source register lies outside the capture window";
/** R111's standing hazard, and FT030. "A plan writer who omits the mask
 *  silently restores the defect and passes every gate." */
inline constexpr const char* kRefusalEmptyRequiredMask =
    "a descriptor declaring outputs while its required_mask is zero";
inline constexpr const char* kRefusalOutputCount =
    "an output count disagreeing with the declared output map";

// ======================================================================
// THE TRANSLATOR
// ======================================================================

/** One uop as the silicon takes it: an opcode and three REGISTER GROUP STARTS.
 *
 *  THIS TRANSLATION IS THE EXACT PLACE A BUG ALREADY LIVED. The oracle's
 *  VecUop carries a FLATTENED src[9]; the hardware has per-operand-group ports.
 *  Reasoning from the flat array wires ROT2's angle to the wrong port.
 *
 *  The test-local version carried that warning and then did not implement it:
 *  its src_group_width() knew only DIST2, LEN2 and LEN3 and DEFAULTED TO 1, so
 *  ROT2 (flattened count 3, true shape A[2] + angle) fell through the
 *  width-1 catch-all and took src[1] -- the SECOND MEMBER OF A -- as operand b,
 *  with the angle landing in c. The comment describing the fix and the code
 *  implementing it had come apart, and no Earth program emits ROT2 so nothing
 *  ever asked.
 *
 *  So the widths are no longer a hand-written switch at all. They are read from
 *  the GENERATED table (zfield/generated/zfield_optable.hpp), which is the same
 *  table zfield_plan.cpp and zfield_interpret.cpp already lower and execute
 *  from. Directive 5.3: "read the generated source group widths and operand
 *  roles", not "expand the helper's guesses". A shape that is not in the
 *  generated table is REFUSED BY NAME rather than guessed at. */
struct Mapped {
  uint8_t op = 0;
  int dst = 0;
  int a = 0, b = 0, c = 0;
  uint32_t imm = 0;
  /** Destination GROUP width from the generated table: NORMALIZE3 writes three
   *  registers, ROT3 three, NORMALIZE2/ROT2/NOISE2 two, everything else one.
   *  The defined-set walk needs the whole group, not just dst. */
  int dst_width = 1;
  /** How many operand groups actually carry a register (0..3). Anything above
   *  this is zero and must not be read as register 0. */
  int n_groups = 0;
  /** Width of each operand group, from the generated table. The defined-set
   *  walk reads [start, start + width) for each group; recording only the
   *  starts would make a DOT3 look like it read one register instead of
   *  three, and the proof would pass while the image was unsound. */
  int group_width[3] = {0, 0, 0};
  uint16_t src_pc = 0;

  /** The 64-bit native word, in zhao_field_host.sv's own field order. */
  uint64_t word() const;
};

/** Lowers ONE logical uop onto physical register group starts.
 *
 *  `scalar_base` is where the broadcast uniform region begins; a scalar-bank
 *  operand at slot s becomes physical register scalar_base + s. */
class Translator {
 public:
  Translator(const Fplan& fp, int scalar_base)
      : fp_(fp), scalar_base_(scalar_base) {}

  /** A source becomes a register. Vector registers sit at the bottom of the
   *  file; the association's uniform values are BROADCAST above them. The
   *  broadcast preserves slot order, so a pair of adjacent scalar slots stays
   *  adjacent -- which is what makes the group-adjacency check below meaningful
   *  for uniform operands as well as varying ones. */
  int reg_of(const UopSrc& s) const {
    return s.kind == SrcKind::kVec ? (int)s.idx : scalar_base_ + (int)s.idx;
  }

  /** Map one uop. Returns false and sets refusal() on any shape it cannot
   *  place EXACTLY. */
  bool map_one(const VecUop& u, Mapped* m);

  const std::string& refusal() const { return refusal_; }
  int scalar_base() const { return scalar_base_; }

 private:
  const Fplan& fp_;
  int scalar_base_;
  std::string refusal_;

  bool fail(const char* why) {
    refusal_ = why;
    return false;
  }
};

// ======================================================================
// THE SMOOTHSTEP CONTRACTION (directive 5.4)
// ======================================================================
//
// Seven varying uops of smoothstep become one prepared RING. Promoted here
// "once, with its differential control" exactly as 5.4 asks, so it is not
// hand-copied into each profile and packer.
//
// The optimisation must preserve outputs AND numerical status, including
// intermediate saturation -- "a shorter instruction stream with different
// sticky flags is not equivalent". The untouched logical FPLAN and the
// canonical interpreter remain the comparison oracles; nothing here is its own
// oracle.

/** Does a seven-uop smoothstep run start at fp.uops[i]? If so, `out` receives
 *  the single UOP_RING_PREP that replaces it. */
bool match_smoothstep(const Fplan& fp, size_t i, VecUop* out);

/** The plan with every smoothstep run contracted. `saved` receives the number
 *  of uops removed (6 per match), which is the "counts the added instructions"
 *  half of directive 5.3 with the sign reversed. */
std::vector<VecUop> contract_smoothstep(const Fplan& fp, int* saved);

// ======================================================================
// THE PHYSICAL IMAGE
// ======================================================================

struct LowerOptions {
  /** Start of the broadcast uniform region. */
  int scalar_base = kDefaultScalarBase;
  /** The capture window this image will be installed behind. */
  int out_base = 0;
  int out_lanes = kDefaultOutLanes;
  /** Apply the directive 5.4 contraction. OFF by default: a lowering is easier
   *  to trust when it is the plain one, and the contraction owns its own
   *  differential. */
  bool contract = false;
  /** Physical register ceiling for this composition. The console composes
   *  REGS=32 today (zhao_console_core.sv:15662) and FH22 asks for 64; the
   *  high-water check reports against whatever is set here rather than
   *  assuming. Directive 5.2: "It must check the final register high-water
   *  mark, not assume n_vreg alone is the RF requirement." */
  int register_ceiling = kPhysicalRegisters;
  /** Identity fields the caller owns; copied into the image header. */
  uint32_t canonical_program_handle32 = 0;
  uint32_t resource_epoch = 0;
  uint32_t source_id32 = 0;
  uint32_t object_serial = 0;
  uint8_t binding_signature = 0;
};

/** One lowered canonical output ordinal, before serialisation. */
struct OutputSource {
  int ordinal = 0;
  uint8_t lane_type = 0;
  host_image::ZfhSourceKind source_kind = host_image::ZFH_SOURCE_KIND_VECTOR_REG;
  int source_index = 0;  // physical register, or prepared-scalar index
  bool required = true;
};

/** The physical executable: the C representation of directive 11.3's PROGRAM
 *  sections, before they become bytes. */
struct HostPlan {
  uint8_t profile = 0;
  host_image::ZfhExecutionForm execution_form = host_image::ZFH_EXECUTION_FORM_PREPARED_REGISTER;

  std::vector<Mapped> uops;
  std::vector<host_image::ZfhInputMapRow> input_map;
  std::vector<OutputSource> output_map;
  std::vector<host_image::ZfhPreloadRow> preload;

  /** ORDINAL-indexed. Never assign a WindowMask to this. */
  RequiredMask required_mask;
  int output_count = 0;
  int input_count = 0;

  // ---- INIT_PROOF (directive 6.2 / 11.3), all PHYSICAL-REGISTER indexed ----
  uint64_t association_register_mask = 0;
  uint64_t point_register_mask = 0;
  uint64_t immutable_register_mask = 0;
  uint64_t initial_defined_mask = 0;
  uint64_t vector_write_mask = 0;

  int scalar_base = kDefaultScalarBase;
  int out_base = 0;
  int out_lanes = kDefaultOutLanes;
  /** Highest physical register touched, plus one. Directive 5.2's high-water
   *  mark -- the actual RF requirement, which n_vreg alone understates. */
  int register_high_water = 0;
  int contraction_saved = 0;

  uint32_t canonical_hash = 0;
  uint32_t canonical_program_handle32 = 0;
  uint32_t resource_epoch = 0;
  uint32_t source_id32 = 0;
  uint32_t object_serial = 0;
  uint8_t binding_signature = 0;
  uint32_t varying_input_mask = 0;  // CANONICAL INPUT ORDINAL mask
  uint32_t point_preload_mask = 0;  // CANONICAL INPUT ORDINAL mask
  uint16_t canonical_instruction_count = 0;

  /** THE ORDINAL -> WINDOW TRANSLATION, and the only place it happens.
   *
   *  window_mask[k] is set iff some ordinal j has source_kind VECTOR_REG and
   *  source_index == out_base + k. A PREPARED_SCALAR ordinal contributes
   *  NOTHING: it has no window position by construction and is seeded at point
   *  start (directive 7.3), so waiting for a vector write it will never
   *  generate is exactly the hang plan section 2 case 1 describes. */
  WindowMask window_mask() const;
};

/** Lower a validated program onto the fabric.
 *
 *  `prep` supplies the prepared scalar values for the PRELOAD rows; pass
 *  nullptr to lower without them (the maps and the proof do not depend on the
 *  VALUES, only on which registers are written).
 *
 *  Returns false and fills `refusal` on any condition the image may not carry.
 *  Every refusal string is one of the k* constants above. */
bool lower(const Fplan& fp, const Decoded& prog, const Prepared* prep, const LowerOptions& opt,
           HostPlan* out, std::string* refusal);

/** The directive 6.2 symbolic defined-set walk, run on an already-lowered
 *  image. lower() calls it; a test calls it directly to fire the control.
 *
 *  Starts from the association's committed preload set PLUS this point's
 *  accepted varying writes, walks every physical instruction checking reads
 *  BEFORE adding its destination group, and refuses on an unsupported read. */
bool verify_init_proof(const HostPlan& hp, std::string* refusal);

/** The strict output contract (directive 7.1) and R111's gate.
 *
 *  Refuses required_mask == 0 while the declared output list is nonempty --
 *  which is the ONLY case that occurs today, because nothing in the tree emits
 *  a header word at all. Directive 7.2: "For new strict imports, absent
 *  metadata or zero required_mask is a load-time refusal. Do not use zero to
 *  mean 'accept whatever happened' in the selected production host." */
bool verify_output_contract(const HostPlan& hp, std::string* refusal);

// ======================================================================
// SERIALISATION -- the ZFH2 capsule (directive 11)
// ======================================================================

/** CRC-32C over a byte range, using the generated ABI table. Same law as
 *  zfield::crc32cConst; runtime form. */
uint32_t crc32c(const uint8_t* p, size_t n, uint32_t crc = 0);

/** Serialise a lowered image into ZFH2 PROGRAM capsule bytes (directive 11.1
 *  header, 11.2 directory, 11.3 program sections).
 *
 *  Byte-stability is a REQUIREMENT, not a nicety: the loader test must receive
 *  THESE bytes over the modelled bridge rather than a parallel handwritten
 *  sequence of raw loader writes assumed to be equivalent (directive 11.5). */
bool serialize_program_image(const HostPlan& hp, const uint8_t* canonical_bytes,
                             size_t canonical_len, std::vector<uint8_t>* out,
                             std::string* refusal);

/** A short human/machine manifest of what was emitted (directive 11.5: "The
 *  packer emits a machine-readable manifest and golden capsule bytes"). */
std::string manifest(const HostPlan& hp, const std::vector<uint8_t>& image);

}  // namespace host_plan
}  // namespace zfield
