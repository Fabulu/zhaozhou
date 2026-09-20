// zfield_host_plan.cpp -- the production lowerer. See zfield_host_plan.hpp for
// the two-mask chapter and the declared non-goals; this file is the mechanism.

#include "zfield/zfield_host_plan.hpp"

#include <cstdio>
#include <cstring>

namespace zfield {
namespace host_plan {

namespace {

inline uint64_t bit64(int i) { return (uint64_t)1 << i; }

inline int popcount64(uint64_t v) {
  int n = 0;
  while (v) {
    v &= v - 1;
    ++n;
  }
  return n;
}

/** Append `n` bytes of `v` little-endian. Every ZFH2 field is little-endian
 *  and written through here, so an endianness assumption cannot be made in one
 *  place and forgotten in another. */
void put(std::vector<uint8_t>* b, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) b->push_back((uint8_t)((v >> (8 * i)) & 0xFF));
}

void pad_to(std::vector<uint8_t>* b, size_t align) {
  while (b->size() % align) b->push_back(0);
}

}  // namespace

// ---------------------------------------------------------------- Mapped ---

uint64_t Mapped::word() const {
  // zhao_field_host.sv:923-928, field for field. Not a new layout.
  const uint64_t m = (uint64_t)((1u << kUopRegBits) - 1);
  uint64_t w = 0;
  w |= (uint64_t)op << kUopOpLsb;
  w |= ((uint64_t)dst & m) << kUopDstLsb;
  w |= ((uint64_t)a & m) << kUopALsb;
  w |= ((uint64_t)b & m) << kUopBLsb;
  w |= ((uint64_t)c & m) << kUopCLsb;
  w |= (uint64_t)imm << kUopImmLsb;
  return w;
}

// ------------------------------------------------------------ Translator ---

bool Translator::map_one(const VecUop& u, Mapped* m) {
  *m = Mapped{};
  m->op = u.op;
  m->dst = (int)u.dst;
  m->imm = u.imm;
  m->src_pc = u.src_pc;
  m->dst_width = 1;

  // ---- the synthetic prepared ring -------------------------------------
  //
  // UOP_RING_PREP is plan-internal (>= 0xF0) and deliberately outside the
  // canonical opcode space, so it is NOT in the generated table and must be
  // placed by hand. It reads its four uniforms from the SCALAR BANK by index,
  // packed into the immediate -- it does not go through the register file at
  // all, which is why only src[0] counts as a register read downstream.
  if (u.op == UOP_RING_PREP) {
    if (u.n_src != 1 + kRingPrepSlotCount) return fail(kRefusalRingPrepCount);
    m->a = reg_of(u.src[0]);
    m->n_groups = 1;
    m->group_width[0] = 1;
    uint32_t packed = 0;
    for (int k = 1; k <= kRingPrepSlotCount; ++k) {
      if (u.src[k].kind != SrcKind::kSca) return fail(kRefusalRingPrepVarying);
      if ((int)u.src[k].idx >= kScalarBankSlots) return fail(kRefusalRingPrepSlot);
      packed |= (uint32_t)u.src[k].idx << (kRingPrepSlotBits * (k - 1));
    }
    // Directive 5.4: bit 24 carries smooth mode through from the contraction,
    // and the live consumer outranks the older "31..24 wholly reserved" note.
    m->imm = packed | (u.imm & kRingPrepSmoothBit);
    return true;
  }

  // ---- every canonical opcode, from the GENERATED table -----------------
  //
  // Directive 5.3: read the generated source group widths and operand roles.
  // The examples it insists be explicit all fall out of this one rule:
  //
  //   DOT3        groups {3,3}  -> a = src[0], b = src[3]
  //   LEN3        groups {3}    -> a = src[0], NO invented B group
  //   NORMALIZE3  groups {3}    -> a = src[0], dst_width 3
  //   DIST2       groups {2,2}  -> a = src[0], b = src[2]  (not A[3]+B[1])
  //   ROT2        groups {2,1}  -> a = src[0], b = src[2]  (the ANGLE, not a1)
  //   ROT3        groups {3,1}  -> a = src[0], b = src[3], axis in the imm
  //   MAD/SEL/CLA groups {1,1,1}-> a,b,c = src[0..2] in their canonical roles
  //
  // None of those is a special case here. That is the point: a hand-written
  // width switch is what let ROT2 take the width-1 catch-all and mis-wire its
  // angle while the comment above it described the opposite.
  const optable::OpShape* sh = optable::shape_of(u.op);
  if (sh == nullptr) return fail(kRefusalUnknownShape);

  int flat = 0;
  for (int g = 0; g < sh->n_groups; ++g) flat += sh->group_width[g];
  if (flat != (int)u.n_src) return fail(kRefusalSrcCount);

  m->dst_width = sh->dst_width;
  m->n_groups = sh->n_groups;

  int off = 0;
  int* dstp[3] = {&m->a, &m->b, &m->c};
  for (int g = 0; g < sh->n_groups && g < 3; ++g) {
    const int w = sh->group_width[g];
    const int start = reg_of(u.src[off]);
    // EVERY multi-member group is checked for physical adjacency, because a
    // group read is what the register file actually performs. A logical group
    // with some scalar and some vector members may not remain adjacent after
    // the uniform/varying split -- directive 5.3 says to check exactly this,
    // and NOT to alias neighbouring registers and hope the numbers agree.
    for (int k = 1; k < w; ++k) {
      if (reg_of(u.src[off + k]) != start + k) return fail(kRefusalNonConsecutive);
    }
    *dstp[g] = start;
    m->group_width[g] = w;
    off += w;
  }
  return true;
}

// ------------------------------------------------------- the contraction ---
//
// Lifted from tests/differential/field_v3_earth_directed.cpp with its
// behaviour preserved exactly. The prose that justified it lives in that file
// and in the directive; what matters here is that there is now ONE copy.

bool match_smoothstep(const Fplan& fp, size_t i, VecUop* out) {
  if (i + 7 > fp.uops.size()) return false;
  const VecUop* u = &fp.uops[i];
  auto vec = [](const UopSrc& s) { return s.kind == SrcKind::kVec; };
  auto sca = [](const UopSrc& s) { return s.kind == SrcKind::kSca; };

  // SUB d, e0 -> t
  if (u[0].op != OP_SUB || u[0].n_src != 2 || !vec(u[0].src[0]) || !sca(u[0].src[1])) return false;
  // MUL t, rcp
  if (u[1].op != OP_MUL || u[1].n_src != 2 || u[1].src[0].idx != u[0].dst || !vec(u[1].src[0]) ||
      !sca(u[1].src[1]))
    return false;
  // CLAMP t, lo, hi
  if (u[2].op != OP_CLAMP || u[2].n_src != 3 || u[2].src[0].idx != u[1].dst || !vec(u[2].src[0]) ||
      !sca(u[2].src[1]) || !sca(u[2].src[2]))
    return false;
  // MUL t, t
  if (u[3].op != OP_MUL || u[3].n_src != 2 || !vec(u[3].src[0]) || !vec(u[3].src[1]) ||
      u[3].src[0].idx != u[2].dst || u[3].src[1].idx != u[2].dst)
    return false;
  // MUL 2.0, t   -- constant FIRST, as the builder emits it
  if (u[4].op != OP_MUL || u[4].n_src != 2 || !sca(u[4].src[0]) || !vec(u[4].src[1]) ||
      u[4].src[1].idx != u[2].dst)
    return false;
  // SUB 3.0, 2t
  if (u[5].op != OP_SUB || u[5].n_src != 2 || !sca(u[5].src[0]) || !vec(u[5].src[1]) ||
      u[5].src[1].idx != u[4].dst)
    return false;
  // MUL t2, (3-2t)
  if (u[6].op != OP_MUL || u[6].n_src != 2 || !vec(u[6].src[0]) || !vec(u[6].src[1]) ||
      u[6].src[0].idx != u[3].dst || u[6].src[1].idx != u[5].dst)
    return false;

  VecUop r{};
  r.op = UOP_RING_PREP;
  r.dst = u[6].dst;
  r.n_src = 5;
  r.src[0] = u[0].src[0];  // d
  r.src[1] = u[0].src[1];  // r0 = e0
  r.src[2] = u[0].src[1];  // m  = e0, so the dead subtraction is the live one
  r.src[3] = u[1].src[1];  // rA = reciprocal
  r.src[4] = u[2].src[1];  // rB = the clamp low bound, which is 0
  r.imm = kRingPrepSmoothBit;
  r.src_pc = u[0].src_pc;
  *out = r;
  return true;
}

std::vector<VecUop> contract_smoothstep(const Fplan& fp, int* saved) {
  std::vector<VecUop> out;
  if (saved) *saved = 0;
  for (size_t i = 0; i < fp.uops.size();) {
    VecUop r{};
    if (match_smoothstep(fp, i, &r)) {
      out.push_back(r);
      i += 7;
      if (saved) *saved += 6;
    } else {
      out.push_back(fp.uops[i]);
      ++i;
    }
  }
  return out;
}

// ------------------------------------------------------------ window mask ---

WindowMask HostPlan::window_mask() const {
  // THE ONLY ORDINAL -> WINDOW TRANSLATION IN THE LIBRARY.
  WindowMask::rep bits = 0;
  for (const OutputSource& o : output_map) {
    if (o.source_kind != host_image::ZFH_SOURCE_KIND_VECTOR_REG) continue;
    const int k = o.source_index - out_base;
    if (k < 0 || k >= out_lanes) continue;  // refused by verify_output_contract
    bits |= (WindowMask::rep)(1u << k);
  }
  return WindowMask(bits);
}

// ------------------------------------------------------- the output contract ---

bool verify_output_contract(const HostPlan& hp, std::string* refusal) {
  auto no = [&](const char* s) {
    if (refusal) *refusal = s;
    return false;
  };

  if ((int)hp.output_map.size() != hp.output_count) return no(kRefusalOutputCount);

  // R111's GATE. Nothing in the tree emits a header word today, so mask == 0 is
  // not merely backward-compatible -- it is the ONLY case that occurs, and a
  // plan writer who omits the mask silently restores the R101 defect and passes
  // every other gate. Directive 7.2: for a new strict import, a zero
  // required_mask is a load-time refusal, and zero does not mean "accept
  // whatever happened".
  if (!hp.output_map.empty() && hp.required_mask.empty()) return no(kRefusalEmptyRequiredMask);

  // Every REQUIRED ordinal must agree with the mask, and every VECTOR_REG
  // ordinal must be OBSERVABLE -- i.e. inside the capture window. An ordinal
  // whose register lies outside it can never set its seen bit, so the point
  // hangs or refuses for the wrong reason. Plan section 2, case 2.
  for (const OutputSource& o : hp.output_map) {
    if (o.required != hp.required_mask.test(o.ordinal)) return no(kRefusalOutputCount);
    if (o.source_kind == host_image::ZFH_SOURCE_KIND_VECTOR_REG) {
      const int k = o.source_index - hp.out_base;
      if (k < 0 || k >= hp.out_lanes) return no(kRefusalOutsideWindow);
    }
    // A PREPARED_SCALAR ordinal has NO window position by construction and is
    // seeded at point start (directive 7.3). It is not checked against the
    // window, and it must not be: doing so would refuse the all-uniform form
    // the directive explicitly requires to work.
  }
  return true;
}

// --------------------------------------------------- the initialisation proof ---

bool verify_init_proof(const HostPlan& hp, std::string* refusal) {
  auto no = [&](const char* s) {
    if (refusal) *refusal = s;
    return false;
  };

  // Directive 6.2: start the symbolic defined set with THIS ASSOCIATION'S
  // committed preload set plus THIS POINT'S accepted varying writes. Not with
  // "whatever a previous context happened to leave behind" -- that is the
  // "repair" the directive forbids by name, and it is what makes removing the
  // per-point E_ZERO walk safe rather than optimistic.
  uint64_t defined = hp.association_register_mask | hp.point_register_mask;

  for (const Mapped& m : hp.uops) {
    if (m.op == OP_END) continue;
    // READS ARE CHECKED BEFORE THE DESTINATION IS ADDED. An op that reads its
    // own destination must have had it defined earlier; adding dst first would
    // make every such read trivially legal and the proof worthless.
    for (int g = 0; g < m.n_groups && g < 3; ++g) {
      const int start = (g == 0) ? m.a : (g == 1) ? m.b : m.c;
      for (int k = 0; k < m.group_width[g]; ++k) {
        const int r = start + k;
        if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
        if ((defined & bit64(r)) == 0) return no(kRefusalReadBeforeDef);
      }
    }
    for (int k = 0; k < m.dst_width; ++k) {
      const int r = m.dst + k;
      if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
      defined |= bit64(r);
    }
  }

  // Directive 11.3's cross-record invariants, checked rather than assumed.
  if (hp.initial_defined_mask != (hp.association_register_mask | hp.point_register_mask))
    return no("initial_defined_mask is not association_register_mask | point_register_mask");
  // Immutable registers are association-owned; a vector write into one would
  // clobber a uniform every other context is sharing.
  if (hp.immutable_register_mask & hp.vector_write_mask)
    return no("an immutable association register is also a vector write target");
  if (hp.immutable_register_mask & hp.point_register_mask)
    return no("an immutable association register is also a per-point write target");
  return true;
}

// ------------------------------------------------------------------ lower ---

bool lower(const Fplan& fp, const Decoded& prog, const Prepared* prep, const LowerOptions& opt,
           HostPlan* out, std::string* refusal) {
  auto no = [&](const char* s) {
    if (refusal) *refusal = s;
    return false;
  };
  if (refusal) refusal->clear();

  HostPlan hp;
  hp.profile = fp.profile;
  hp.scalar_base = opt.scalar_base;
  hp.out_base = opt.out_base;
  hp.out_lanes = opt.out_lanes;
  hp.canonical_hash = fp.canonical_hash;
  hp.canonical_program_handle32 = opt.canonical_program_handle32;
  hp.resource_epoch = opt.resource_epoch;
  hp.source_id32 = opt.source_id32;
  hp.object_serial = opt.object_serial;
  hp.binding_signature = opt.binding_signature;
  hp.varying_input_mask = fp.varying_mask;
  hp.canonical_instruction_count = (uint16_t)prog.instrs.size();

  // ---- 1. the uops ------------------------------------------------------
  const std::vector<VecUop> src =
      opt.contract ? contract_smoothstep(fp, &hp.contraction_saved) : fp.uops;

  Translator tr(fp, opt.scalar_base);
  int hwm = 0;
  auto touch = [&](int r) {
    if (r + 1 > hwm) hwm = r + 1;
  };
  for (const VecUop& u : src) {
    Mapped m;
    if (!tr.map_one(u, &m)) return no(tr.refusal().c_str());
    for (int g = 0; g < m.n_groups && g < 3; ++g) {
      const int start = (g == 0) ? m.a : (g == 1) ? m.b : m.c;
      touch(start + m.group_width[g] - 1);
    }
    for (int k = 0; k < m.dst_width; ++k) {
      const int r = m.dst + k;
      if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
      hp.vector_write_mask |= bit64(r);
      touch(r);
    }
    hp.uops.push_back(m);
  }

  // ---- 2. the input map, and the two register sets it induces -----------
  hp.input_count = (int)prog.in_lanes.size();
  for (int i = 0; i < hp.input_count; ++i) {
    host_image::ZfhInputMapRow row{};
    row.ordinal = (uint8_t)i;
    row.lane_type = prog.in_lanes[(size_t)i].type;
    row.flags = 0;
    // The unused address in a map row is ZFH_ADDR_UNUSED, NEVER an implicitly
    // valid register or slot zero (directive 11.3). Register 0 is a real
    // register and "unused" must not be spelled the same way.
    row.physical_register = (uint16_t)host_image::ZFH_ADDR_UNUSED;
    row.prepared_slot = (uint16_t)host_image::ZFH_ADDR_UNUSED;

    const bool varying = i < (int)fp.in_vreg.size() && fp.in_vreg[(size_t)i] != 0xFF;
    const bool uniform = i < (int)fp.in_slot.size() && fp.in_slot[(size_t)i] != 0xFFFF;
    if (varying) {
      row.source_class = host_image::ZFH_SOURCE_CLASS_VARYING_INPUT;
      const int r = (int)fp.in_vreg[(size_t)i];
      if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
      row.physical_register = (uint16_t)r;
      hp.point_register_mask |= bit64(r);
      hp.point_preload_mask |= (uint32_t)1 << i;
      touch(r);
    } else if (uniform) {
      row.source_class = host_image::ZFH_SOURCE_CLASS_UNIFORM_INPUT;
      const int slot = (int)fp.in_slot[(size_t)i];
      const int r = opt.scalar_base + slot;
      if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
      row.physical_register = (uint16_t)r;
      row.prepared_slot = (uint16_t)slot;
      touch(r);
    } else {
      // Only an ACTUALLY unused input may be UNUSED_PROVEN. The declared value
      // still exists in the association record for validation and capture.
      row.source_class = host_image::ZFH_SOURCE_CLASS_UNUSED_PROVEN;
    }
    hp.input_map.push_back(row);
  }

  // The whole broadcast region is association-owned and immutable: it is
  // written once per association and shared by every context, so a per-point
  // write into it is a cross-context corruption, not a local bug.
  for (int s = 0; s < (int)fp.n_scalar; ++s) {
    const int r = opt.scalar_base + s;
    if (r < 0 || r >= kPhysicalRegisters) return no(kRefusalRegisterRange);
    hp.association_register_mask |= bit64(r);
    touch(r);
  }
  hp.immutable_register_mask = hp.association_register_mask;
  hp.initial_defined_mask = hp.association_register_mask | hp.point_register_mask;

  // ---- 3. the output map -- plan section 2's translation ----------------
  hp.output_count = (int)fp.out_map.size();
  if (hp.output_count > (int)host_image::ZFH_MAX_CANONICAL_OUTPUTS)
    return no(kRefusalOutputCount);
  RequiredMask::rep req = 0;
  for (int j = 0; j < hp.output_count; ++j) {
    const OutTag& t = fp.out_map[(size_t)j];
    OutputSource o;
    o.ordinal = j;
    o.lane_type = j < (int)prog.out_lanes.size() ? prog.out_lanes[(size_t)j].type : 0;
    o.required = true;
    if (t.kind == SrcKind::kVec) {
      o.source_kind = host_image::ZFH_SOURCE_KIND_VECTOR_REG;
      o.source_index = (int)t.idx;
      if (o.source_index < 0 || o.source_index >= kPhysicalRegisters)
        return no(kRefusalRegisterRange);
      touch(o.source_index);
    } else {
      // Directive 7.3: a prepared scalar is validated at association seal and
      // SEEDED at point start. It never waits for a vector write, because an
      // all-uniform program will never generate one.
      o.source_kind = host_image::ZFH_SOURCE_KIND_PREPARED_SCALAR;
      o.source_index = (int)t.idx;
    }
    // Bit j of the ORDINAL mask. This is the line the whole two-type exercise
    // exists to protect: `req` is a RequiredMask::rep and is never mixed with
    // a window position.
    req |= (RequiredMask::rep)(1u << j);
    hp.output_map.push_back(o);
  }
  hp.required_mask = RequiredMask(req);

  // ---- 4. the preload rows ----------------------------------------------
  if (prep != nullptr) {
    for (int s = 0; s < (int)fp.n_scalar && s < (int)prep->scalar.size(); ++s) {
      host_image::ZfhPreloadRow row{};
      row.physical_register = (uint16_t)(opt.scalar_base + s);
      row.flags = 0;
      row.value = prep->scalar[(size_t)s];
      hp.preload.push_back(row);
    }
  }

  // ---- 5. execution form -------------------------------------------------
  //
  // UNIFORM_ONLY requires a checked plan with NO varying instructions, all
  // output sources valid prepared scalars, and ZERO physical uops. "An empty
  // arbitrary physical program is NOT the uniform-only optimisation", so all
  // three conditions are tested, not just the uop count.
  bool all_prepared = !hp.output_map.empty();
  for (const OutputSource& o : hp.output_map)
    if (o.source_kind != host_image::ZFH_SOURCE_KIND_PREPARED_SCALAR) all_prepared = false;
  hp.execution_form = (hp.uops.empty() && all_prepared && fp.varying_mask == 0)
                          ? host_image::ZFH_EXECUTION_FORM_UNIFORM_ONLY
                          : host_image::ZFH_EXECUTION_FORM_PREPARED_REGISTER;

  hp.register_high_water = hwm;
  // Directive 5.2: check the FINAL high-water mark, not n_vreg. The corpus
  // example needing 7+29=36 is the concrete reason REGS=32 is insufficient --
  // reporting n_vreg there would say 7 and look comfortable.
  if (hwm > opt.register_ceiling) return no(kRefusalRegisterRange);

  if (!verify_output_contract(hp, refusal)) return false;
  if (!verify_init_proof(hp, refusal)) return false;

  *out = hp;
  return true;
}

// ------------------------------------------------------------------- crc ---

uint32_t crc32c(const uint8_t* p, size_t n, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < n; ++i)
    crc = zhao_abi::ZHAO_CRC32C_TABLE[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

// ----------------------------------------------------------- serialisation ---

bool serialize_program_image(const HostPlan& hp, const uint8_t* canonical_bytes,
                             size_t canonical_len, std::vector<uint8_t>* out,
                             std::string* refusal) {
  if (!verify_output_contract(hp, refusal)) return false;
  if (!verify_init_proof(hp, refusal)) return false;

  using namespace host_image;

  // ---- section bodies, built first so the directory can carry real sizes --
  struct Body {
    uint16_t kind;
    uint16_t element_bytes;
    uint32_t element_count;
    std::vector<uint8_t> bytes;
  };
  std::vector<Body> bodies;

  // 0x0001 CANONICAL_IMAGE -- the validated .zprog, EXACT bytes. Source-map
  // and name metadata are not silently stripped before identity comparison.
  if (canonical_bytes != nullptr && canonical_len > 0) {
    Body b{ZFH_SECTION_CANONICAL_IMAGE, 1, (uint32_t)canonical_len, {}};
    b.bytes.assign(canonical_bytes, canonical_bytes + canonical_len);
    bodies.push_back(b);
  }

  // 0x0002 PROGRAM_META
  {
    Body b{ZFH_SECTION_PROGRAM_META, (uint16_t)ZFH_PM_BYTES, 1, {}};
    std::vector<uint8_t>& v = b.bytes;
    put(&v, hp.profile, 1);
    put(&v, hp.binding_signature, 1);
    put(&v, (uint8_t)hp.execution_form, 1);
    put(&v, 0, 1);  // flags
    put(&v, (uint8_t)hp.input_count, 1);
    put(&v, (uint8_t)hp.output_count, 1);
    put(&v, hp.required_mask.bits(), 1);  // ORDINAL-indexed, by type
    put(&v, 0, 1);                        // table_count
    put(&v, hp.canonical_instruction_count, 2);
    put(&v, (uint16_t)hp.uops.size(), 2);
    put(&v, (uint16_t)hp.register_high_water, 2);
    put(&v, (uint16_t)hp.preload.size(), 2);
    put(&v, hp.varying_input_mask, 4);
    put(&v, hp.point_preload_mask, 4);
    put(&v, hp.canonical_hash, 4);  // code_image_crc
    put(&v, 0, 4);                  // maps_crc, filled below
    put(&v, hp.canonical_hash, 4);  // logical_plan_identity
    for (int i = 0; i < 7; ++i) put(&v, 0, 4);
    if (v.size() != ZFH_PM_BYTES) {
      if (refusal) *refusal = "PROGRAM_META did not serialise to 64 bytes";
      return false;
    }
    bodies.push_back(b);
  }

  // 0x0003 PHYSICAL_UOPS -- 8-byte native words, the existing register format
  {
    Body b{ZFH_SECTION_PHYSICAL_UOPS, 8, (uint32_t)hp.uops.size(), {}};
    for (const Mapped& m : hp.uops) put(&b.bytes, m.word(), 8);
    bodies.push_back(b);
  }

  // 0x0004 INPUT_MAP
  {
    Body b{ZFH_SECTION_INPUT_MAP, (uint16_t)ZFH_IN_BYTES, (uint32_t)hp.input_map.size(), {}};
    for (const ZfhInputMapRow& r : hp.input_map) {
      put(&b.bytes, r.ordinal, 1);
      put(&b.bytes, r.lane_type, 1);
      put(&b.bytes, r.source_class, 1);
      put(&b.bytes, r.flags, 1);
      put(&b.bytes, r.physical_register, 2);
      put(&b.bytes, r.prepared_slot, 2);
    }
    bodies.push_back(b);
  }

  // 0x0005 OUTPUT_MAP -- one row per ORDINAL; the translation record itself
  {
    Body b{ZFH_SECTION_OUTPUT_MAP, (uint16_t)ZFH_OUT_BYTES, (uint32_t)hp.output_map.size(), {}};
    for (const OutputSource& o : hp.output_map) {
      put(&b.bytes, (uint8_t)o.ordinal, 1);
      put(&b.bytes, o.lane_type, 1);
      put(&b.bytes, (uint8_t)o.source_kind, 1);
      put(&b.bytes, o.required ? ZFH_OUTPUT_FLAG_REQUIRED : 0u, 1);
      put(&b.bytes, (uint16_t)o.source_index, 2);
      put(&b.bytes, 0, 2);  // reserved, must be zero
    }
    bodies.push_back(b);
  }

  // 0x0006 INIT_PROOF
  {
    Body b{ZFH_SECTION_INIT_PROOF, (uint16_t)ZFH_IP_BYTES, 1, {}};
    std::vector<uint8_t>& v = b.bytes;
    put(&v, 1, 2);             // proof_version
    put(&v, ZFH_IP_BYTES, 2);  // record_bytes
    // Exactly the one flag matching execution_form is set; the others are zero.
    const uint32_t f = (hp.execution_form == ZFH_EXECUTION_FORM_CANONICAL)
                           ? ZFH_INITPROOF_FLAG_CANONICAL
                           : ZFH_INITPROOF_FLAG_PREPARED;
    put(&v, f, 4);
    put(&v, hp.association_register_mask, 8);
    put(&v, hp.point_register_mask, 8);
    put(&v, hp.immutable_register_mask, 8);
    put(&v, hp.initial_defined_mask, 8);
    put(&v, hp.vector_write_mask, 8);
    put(&v, hp.canonical_hash, 4);
    put(&v, 0, 4);  // maps_crc, filled below
    // THE POPULATION COUNT, not the highest register number plus one. The
    // directive says so explicitly and the two differ on every sparse map.
    put(&v, (uint16_t)popcount64(hp.association_register_mask), 2);
    put(&v, 0, 2);
    put(&v, 0, 4);
    if (v.size() != ZFH_IP_BYTES) {
      if (refusal) *refusal = "INIT_PROOF did not serialise to 64 bytes";
      return false;
    }
    bodies.push_back(b);
  }

  // ---- maps_crc, over the two map sections, before layout ---------------
  uint32_t maps_crc = 0;
  for (const Body& b : bodies)
    if (b.kind == ZFH_SECTION_INPUT_MAP || b.kind == ZFH_SECTION_OUTPUT_MAP)
      maps_crc = crc32c(b.bytes.data(), b.bytes.size(), maps_crc);
  for (Body& b : bodies) {
    if (b.kind == ZFH_SECTION_PROGRAM_META) {
      std::memcpy(&b.bytes[ZFH_PM_OFF_MAPS_CRC], &maps_crc, 4);
    } else if (b.kind == ZFH_SECTION_INIT_PROOF) {
      std::memcpy(&b.bytes[ZFH_IP_OFF_MAPS_CRC], &maps_crc, 4);
    }
  }

  // ---- layout: header, directory (sorted by kind), 64-byte-aligned bodies --
  // bodies were appended in ascending kind order; assert rather than assume.
  for (size_t i = 1; i < bodies.size(); ++i) {
    if (bodies[i].kind <= bodies[i - 1].kind) {
      if (refusal) *refusal = "section directory entries are not sorted by kind";
      return false;
    }
  }

  const size_t dir_bytes = bodies.size() * ZFH_SEC_BYTES;
  size_t off = ZFH_HDR_BYTES + dir_bytes;
  off = (off + ZFH_ALIGNMENT - 1) / ZFH_ALIGNMENT * ZFH_ALIGNMENT;
  std::vector<uint32_t> offsets;
  for (const Body& b : bodies) {
    offsets.push_back((uint32_t)off);
    off += b.bytes.size();
    off = (off + ZFH_ALIGNMENT - 1) / ZFH_ALIGNMENT * ZFH_ALIGNMENT;
  }
  const uint32_t total = (uint32_t)off;

  std::vector<uint8_t> img;
  img.reserve(total);
  put(&img, ZFH_MAGIC0, 1);
  put(&img, ZFH_MAGIC1, 1);
  put(&img, ZFH_MAGIC2, 1);
  put(&img, ZFH_MAGIC3, 1);
  put(&img, ZFH_IMAGE_VERSION, 2);
  put(&img, ZFH_OBJECT_KIND_PROGRAM, 1);
  put(&img, 0, 1);  // flags
  put(&img, total, 4);
  put(&img, 0, 4);  // body_crc32c -- computed with these four bytes ZERO
  put(&img, ZFH_HOST_PROTOCOL_VERSION, 2);
  put(&img, 0, 2);
  put(&img, hp.canonical_hash, 4);
  put(&img, hp.canonical_program_handle32, 4);
  put(&img, hp.resource_epoch, 4);
  put(&img, FPLAN_ABI_VERSION, 4);
  put(&img, FPLAN_FABRIC_VERSION, 4);
  put(&img, hp.source_id32, 4);
  put(&img, (uint16_t)bodies.size(), 2);
  put(&img, ZFH_HEADER_BYTES, 2);
  put(&img, 0, 4);  // parent_binding_handle: zero for an unbound PROGRAM
  put(&img, canonical_len ? crc32c(canonical_bytes, canonical_len) : 0u, 4);
  put(&img, hp.object_serial, 4);
  put(&img, 0, 4);

  for (size_t i = 0; i < bodies.size(); ++i) {
    put(&img, bodies[i].kind, 2);
    put(&img, bodies[i].element_bytes, 2);
    put(&img, bodies[i].element_count, 4);
    put(&img, offsets[i], 4);
    put(&img, (uint32_t)bodies[i].bytes.size(), 4);
  }
  pad_to(&img, ZFH_ALIGNMENT);
  for (size_t i = 0; i < bodies.size(); ++i) {
    if (img.size() != offsets[i]) {
      if (refusal) *refusal = "a section body did not land at its directory offset";
      return false;
    }
    img.insert(img.end(), bodies[i].bytes.begin(), bodies[i].bytes.end());
    pad_to(&img, ZFH_ALIGNMENT);
  }
  if (img.size() != total) {
    if (refusal) *refusal = "the serialised image did not reach total_bytes";
    return false;
  }

  const uint32_t body_crc = crc32c(img.data(), img.size());
  std::memcpy(&img[ZFH_HDR_OFF_BODY_CRC32C], &body_crc, 4);

  *out = img;
  return true;
}

std::string manifest(const HostPlan& hp, const std::vector<uint8_t>& image) {
  char buf[1400];
  // The two masks are printed with their NUMBERING NAMED. A bare pair of hex
  // values in a log is exactly how they got confused in the first place.
  std::snprintf(
      buf, sizeof(buf),
      "{\n"
      "  \"profile\": %u,\n"
      "  \"execution_form\": %u,\n"
      "  \"canonical_hash\": \"0x%08X\",\n"
      "  \"physical_uops\": %zu,\n"
      "  \"canonical_instructions\": %u,\n"
      "  \"contraction_saved_uops\": %d,\n"
      "  \"input_count\": %d,\n"
      "  \"output_count\": %d,\n"
      "  \"required_mask_ORDINAL_indexed\": \"0x%02X\",\n"
      "  \"window_mask_WINDOW_POSITION_indexed\": \"0x%02X\",\n"
      "  \"out_base\": %d,\n"
      "  \"out_lanes\": %d,\n"
      "  \"scalar_base\": %d,\n"
      "  \"register_high_water\": %d,\n"
      "  \"association_register_mask\": \"0x%016llX\",\n"
      "  \"point_register_mask\": \"0x%016llX\",\n"
      "  \"vector_write_mask\": \"0x%016llX\",\n"
      "  \"preload_rows\": %zu,\n"
      "  \"image_bytes\": %zu,\n"
      "  \"image_crc32c\": \"0x%08X\"\n"
      "}\n",
      (unsigned)hp.profile, (unsigned)hp.execution_form, hp.canonical_hash, hp.uops.size(),
      (unsigned)hp.canonical_instruction_count, hp.contraction_saved, hp.input_count,
      hp.output_count, (unsigned)hp.required_mask.bits(), (unsigned)hp.window_mask().bits(),
      hp.out_base, hp.out_lanes, hp.scalar_base, hp.register_high_water,
      (unsigned long long)hp.association_register_mask,
      (unsigned long long)hp.point_register_mask, (unsigned long long)hp.vector_write_mask,
      hp.preload.size(), image.size(), crc32c(image.data(), image.size()));
  return std::string(buf);
}

}  // namespace host_plan
}  // namespace zfield
