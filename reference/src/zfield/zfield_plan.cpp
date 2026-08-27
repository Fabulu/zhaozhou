// zfield_plan.cpp — exact FPLAN planner + uniform preparation + vector
// reference executor (Field v3 Phase 2, reports/Fieldv3.md).
//
// Everything numeric here calls the SAME semantic step layer the canonical
// interpreter calls (zfield/zfield_steps.hpp) — preparation is partial
// interpretation, not a reimplementation. The lowering walk is driven by the
// GENERATED canonical operation table; canonical opcodes pass through into
// the uops unchanged. See zfield_plan.hpp for the contract.

#include "zfield/zfield_plan.hpp"

#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield_steps.hpp"

namespace zfield {

namespace {

using zref::SatLedger;

constexpr uint16_t kNoSlot = 0xFFFF;
constexpr uint8_t kNoVreg = 0xFF;
constexpr uint32_t kGroups = 273;  // ceil(1089 / 4): vector groups per full patch

// Where a canonical register's CURRENT value lives during the lowering walk.
struct RegLoc {
  bool defined = false;
  bool varying = false;
  uint16_t slot = kNoSlot;  // scalar-bank slot when uniform
};

struct Lowerer {
  const Decoded& prog;
  Fplan fp;
  RegLoc loc[REG_COUNT];
  uint16_t zero_slot = kNoSlot;  // lazily allocated; unreachable on validated programs

  explicit Lowerer(const Decoded& p) : prog(p) {}

  uint16_t alloc_slots(int n) {
    const uint16_t base = fp.n_scalar;
    fp.n_scalar = (uint16_t)(fp.n_scalar + n);
    return base;
  }

  uint16_t slot_of(uint8_t reg) {
    RegLoc& l = loc[reg];
    if (!l.defined) {
      // Validated programs never read before def (V11); an undefined register
      // reads as the zeroed file, so route it to one shared zero slot.
      if (zero_slot == kNoSlot) zero_slot = alloc_slots(1);
      return zero_slot;
    }
    return l.slot;
  }

  void run(uint32_t varying_mask) {
    fp.canonical_hash = prog.program_hash;
    fp.profile = prog.profile;
    fp.varying_mask = varying_mask;
    fp.in_slot.assign(prog.in_lanes.size(), kNoSlot);
    fp.in_vreg.assign(prog.in_lanes.size(), kNoVreg);

    // Seed input lanes: varying lanes live in vector registers (canonical ids
    // for now, compacted below); uniform lanes get scalar slots.
    for (size_t i = 0; i < prog.in_lanes.size(); ++i) {
      const uint8_t r = prog.in_lanes[i].reg;
      if (varying_mask & (1u << i)) {
        loc[r] = RegLoc{true, true, kNoSlot};
      } else {
        const uint16_t s = alloc_slots(1);
        fp.in_slot[i] = s;
        loc[r] = RegLoc{true, false, s};
      }
    }

    for (size_t pc = 0; pc < prog.instrs.size(); ++pc) {
      const Instr& ins = prog.instrs[pc];
      if (ins.op == OP_END) break;
      const optable::OpShape* sh = optable::shape_of(ins.op);
      if (sh == nullptr) __builtin_unreachable();

      const uint8_t starts[3] = {ins.a, ins.b, ins.c};
      bool any_varying = false;
      for (int g = 0; g < sh->n_groups; ++g) {
        for (int w = 0; w < sh->group_width[g]; ++w) {
          const RegLoc& l = loc[starts[g] + w];
          if (l.defined && l.varying) any_varying = true;
        }
      }

      if (!any_varying) {
        lower_uniform(ins, sh, (uint16_t)pc);
      } else if (ins.op == OP_RING && !loc[ins.b].varying && !loc[ins.c].varying) {
        lower_prepared_ring(ins, (uint16_t)pc);
      } else {
        lower_varying(ins, sh, (uint16_t)pc);
      }
    }

    // Output map from the final register locations.
    fp.out_map.reserve(prog.out_lanes.size());
    for (const IoLane& o : prog.out_lanes) {
      const RegLoc& l = loc[o.reg];
      if (l.defined && l.varying) {
        fp.out_map.push_back(OutTag{SrcKind::kVec, o.reg});  // compacted below
      } else {
        fp.out_map.push_back(OutTag{SrcKind::kSca, l.defined ? l.slot : slot_of(o.reg)});
      }
    }

    compact_vregs();
    finish_demand();
  }

  void lower_uniform(const Instr& ins, const optable::OpShape* sh, uint16_t pc) {
    PrepUop u{};
    u.op = ins.op;
    u.imm = ins.imm;
    u.src_pc = pc;
    const uint8_t starts[3] = {ins.a, ins.b, ins.c};
    int k = 0;
    for (int g = 0; g < sh->n_groups; ++g) {
      for (int w = 0; w < sh->group_width[g]; ++w) u.src[k++] = slot_of(starts[g] + w);
    }
    u.n_src = (uint8_t)k;
    u.dst = alloc_slots(sh->dst_width);
    for (int w = 0; w < sh->dst_width; ++w) {
      loc[ins.dst + w] = RegLoc{true, false, (uint16_t)(u.dst + w)};
    }
    fp.prep.push_back(u);
    fp.demand.uniform_ops += 1;
  }

  // RING(d varying, r0/r1 uniform): prepare m, rA, rB once; emit ONE
  // UOP_RING_PREP whose nine varying products stay separately rounded.
  void lower_prepared_ring(const Instr& ins, uint16_t pc) {
    const uint16_t s_r0 = slot_of(ins.b);
    const uint16_t s_r1 = slot_of(ins.c);

    const uint16_t s_m = alloc_slots(1);
    fp.prep.push_back(PrepUop{PREP_RING_MID, s_m, 2, {s_r0, s_r1}, 0, pc});
    const uint16_t s_d0 = alloc_slots(1);
    fp.prep.push_back(PrepUop{OP_SUB, s_d0, 2, {s_m, s_r0}, 0, pc});
    const uint16_t s_rA = alloc_slots(1);
    fp.prep.push_back(PrepUop{OP_RCP, s_rA, 1, {s_d0}, 0, pc});
    const uint16_t s_d1 = alloc_slots(1);
    fp.prep.push_back(PrepUop{OP_SUB, s_d1, 2, {s_r1, s_m}, 0, pc});
    const uint16_t s_rB = alloc_slots(1);
    fp.prep.push_back(PrepUop{OP_RCP, s_rB, 1, {s_d1}, 0, pc});
    fp.demand.uniform_ops += 5;

    VecUop v{};
    v.op = UOP_RING_PREP;
    v.dst = ins.dst;
    v.n_src = 5;
    v.src[0] = UopSrc{SrcKind::kVec, ins.a};  // d (compacted below)
    v.src[1] = UopSrc{SrcKind::kSca, s_r0};
    v.src[2] = UopSrc{SrcKind::kSca, s_m};
    v.src[3] = UopSrc{SrcKind::kSca, s_rA};
    v.src[4] = UopSrc{SrcKind::kSca, s_rB};
    v.imm = 0;
    v.src_pc = pc;
    fp.uops.push_back(v);
    loc[ins.dst] = RegLoc{true, true, kNoSlot};

    fp.demand.vec_issue += 1;
    fp.demand.vmul_slots += 9;
  }

  void lower_varying(const Instr& ins, const optable::OpShape* sh, uint16_t pc) {
    VecUop v{};
    v.op = ins.op;
    v.imm = ins.imm;
    v.src_pc = pc;
    const uint8_t starts[3] = {ins.a, ins.b, ins.c};
    int k = 0;
    for (int g = 0; g < sh->n_groups; ++g) {
      for (int w = 0; w < sh->group_width[g]; ++w) {
        const uint8_t r = starts[g] + w;
        const RegLoc& l = loc[r];
        if (l.defined && l.varying) {
          v.src[k++] = UopSrc{SrcKind::kVec, r};  // compacted below
        } else {
          v.src[k++] = UopSrc{SrcKind::kSca, l.defined ? l.slot : slot_of(r)};
        }
      }
    }
    v.n_src = (uint8_t)k;
    v.dst = ins.dst;
    fp.uops.push_back(v);
    for (int w = 0; w < sh->dst_width; ++w) {
      loc[ins.dst + w] = RegLoc{true, true, kNoSlot};
    }

    fp.demand.vec_issue += 1;
    switch (sh->svc) {
      case optable::SVC_VMUL:
        fp.demand.vmul_slots += 1;
        break;
      case optable::SVC_CURVE:
        fp.demand.curve_req += 1;
        fp.demand.vmul_slots += 1;  // the interpolation MAD schedules on the bank
        break;
      case optable::SVC_DIST:
        fp.demand.dist_req += 1;
        fp.demand.vmul_slots += 1;  // the squares schedule on the bank
        break;
      case optable::SVC_COLD:
        fp.demand.cold_ops += 1;
        break;
      default:
        break;  // VALU: lane-private
    }
  }

  // Rewrite canonical register ids in the uops into a compacted vector file.
  // Order-preserving compaction keeps adjacent canonical group members
  // adjacent: group members are consecutive integers and are all referenced,
  // so nothing can fall between them.
  void compact_vregs() {
    bool used[REG_COUNT] = {};
    for (const VecUop& v : fp.uops) {
      const int w = (v.op == UOP_RING_PREP) ? 1 : optable::shape_of(v.op)->dst_width;
      for (int i = 0; i < w; ++i) used[v.dst + i] = true;
      for (int i = 0; i < v.n_src; ++i) {
        if (v.src[i].kind == SrcKind::kVec) used[v.src[i].idx] = true;
      }
    }
    for (const OutTag& o : fp.out_map) {
      if (o.kind == SrcKind::kVec) used[o.idx] = true;
    }
    uint8_t map[REG_COUNT];
    uint8_t n = 0;
    for (int r = 0; r < (int)REG_COUNT; ++r) map[r] = used[r] ? n++ : kNoVreg;
    fp.n_vreg = n;

    for (VecUop& v : fp.uops) {
      v.dst = map[v.dst];
      for (int i = 0; i < v.n_src; ++i) {
        if (v.src[i].kind == SrcKind::kVec) v.src[i].idx = map[v.src[i].idx];
      }
    }
    for (OutTag& o : fp.out_map) {
      if (o.kind == SrcKind::kVec) o.idx = map[o.idx];
    }
    for (size_t i = 0; i < prog.in_lanes.size(); ++i) {
      const uint8_t r = prog.in_lanes[i].reg;
      if ((fp.varying_mask & (1u << i)) && used[r]) fp.in_vreg[i] = map[r];
    }
  }

  void finish_demand() {
    fp.demand.vec_groups = kGroups;
    for (const Table& t : prog.tables) {
      fp.demand.table_bytes += (uint32_t)t.x.size() * 12;  // x, y, dy words
    }
    fp.demand.vreg_hwm = fp.n_vreg;
    fp.demand.sreg_hwm = fp.n_scalar;

    // Admission (spec/form/cost-model.md §5): hot iff every service fits the
    // measured/targeted machine inside the 6,000-clock association deadline
    // and the reduced register file, with no cold-lane demand.
    const uint32_t occ_issue = fp.demand.vec_issue * kGroups;
    const uint32_t occ_vmul = fp.demand.vmul_slots * kGroups;
    const uint32_t occ_curve = fp.demand.curve_req * kGroups * 14;
    const uint32_t occ_dist = fp.demand.dist_req * kGroups * 20;
    uint32_t bind = occ_issue;
    if (occ_vmul > bind) bind = occ_vmul;
    if (occ_curve > bind) bind = occ_curve;
    if (occ_dist > bind) bind = occ_dist;
    const bool hot = fp.demand.cold_ops == 0 && bind <= 6000 && fp.n_vreg <= 32;
    fp.perf_class = hot ? PlanClass::kHot : PlanClass::kCold;
  }
};

}  // namespace

Fplan plan(const Decoded& prog, uint32_t varying_mask) {
  Lowerer lw(prog);
  lw.run(varying_mask);
  return lw.fp;
}

Prepared prepare(const Fplan& fp, const Decoded& prog, const int32_t* in, size_t n_in) {
  Prepared out;
  out.scalar.assign(fp.n_scalar, 0);
  for (size_t i = 0; i < fp.in_slot.size() && i < n_in; ++i) {
    if (fp.in_slot[i] != kNoSlot) out.scalar[fp.in_slot[i]] = in[i];
  }
  SatLedger L = {};
  int32_t src[9] = {0};  // LDC has no sources; exec_op reads only the shape's members
  int32_t dst[3] = {0};
  for (const PrepUop& u : fp.prep) {
    for (int i = 0; i < u.n_src; ++i) src[i] = out.scalar[u.src[i]];
    int w = 1;
    if (u.op == PREP_RING_MID) {
      dst[0] = steps::ring_mid(src[0], src[1], &L);
    } else {
      const optable::OpShape* sh = optable::shape_of(u.op);
      if (sh == nullptr) __builtin_unreachable();
      steps::exec_op(u.op, u.imm, prog.tables, src, dst, &L);
      w = sh->dst_width;
    }
    for (int i = 0; i < w; ++i) out.scalar[u.dst + i] = dst[i];
  }
  out.uniform_status = Status{L.add || L.mul || L.rescale || L.unit || L.rcp, L.rcp0 != 0};
  out.uniform_ledger = L;
  return out;
}

Status execute_point(const Fplan& fp, const Decoded& prog, const Prepared& prep, const int32_t* in,
                     size_t n_in, int32_t* out, size_t n_out) {
  return execute_point(fp, prog, prep, in, n_in, out, n_out, nullptr);
}

Status execute_point(const Fplan& fp, const Decoded& prog, const Prepared& prep, const int32_t* in,
                     size_t n_in, int32_t* out, size_t n_out, SatLedger* ledger_out) {
  int32_t vec[REG_COUNT] = {0};  // n_vreg <= 64 always
  for (size_t i = 0; i < fp.in_vreg.size() && i < n_in; ++i) {
    if (fp.in_vreg[i] != kNoVreg) vec[fp.in_vreg[i]] = in[i];
  }
  SatLedger L = {};
  int32_t src[9] = {0};
  int32_t dst[3] = {0};
  for (const VecUop& v : fp.uops) {
    for (int i = 0; i < v.n_src; ++i) {
      src[i] = (v.src[i].kind == SrcKind::kVec) ? vec[v.src[i].idx] : prep.scalar[v.src[i].idx];
    }
    int w = 1;
    if (v.op == UOP_RING_PREP) {
      dst[0] = steps::ring_prepared(src[0], src[1], src[2], src[3], src[4], &L);
    } else {
      const optable::OpShape* sh = optable::shape_of(v.op);
      if (sh == nullptr) __builtin_unreachable();
      steps::exec_op(v.op, v.imm, prog.tables, src, dst, &L);
      w = sh->dst_width;
    }
    for (int i = 0; i < w; ++i) vec[v.dst + i] = dst[i];
  }
  for (size_t i = 0; i < fp.out_map.size() && i < n_out; ++i) {
    const OutTag& o = fp.out_map[i];
    out[i] = (o.kind == SrcKind::kVec) ? vec[o.idx] : prep.scalar[o.idx];
  }
  if (ledger_out != nullptr) *ledger_out = L;
  const bool sat = L.add || L.mul || L.rescale || L.unit || L.rcp;
  return Status{prep.uniform_status.sat || sat, prep.uniform_status.rcp0 || (L.rcp0 != 0)};
}

}  // namespace zfield
