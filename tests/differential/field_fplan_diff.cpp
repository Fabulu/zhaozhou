// field_fplan_diff.cpp — THE Field v3 Phase 2 gate (reports/Fieldv3.md):
//
//     full canonical zfield::interpret
//       ==  uniform preparation + FPLAN vector reference executor
//
// compared on EVERY output lane, EVERY saturation-ledger lane (as sticky
// booleans — the only collapse the Status law itself performs), rcp0,
// boundary inputs, random legal programs, and all three committed Earth
// programs. Until this is green there is no v3 RTL.
//
// The per-lane law being asserted: for each of the six ledger lanes,
//   interpret's lane fired  <=>  (uniform-block lane + varying lane) fired.
// Counts may legitimately differ (a uniform instruction's event is recorded
// once per field instance rather than once per point); the STICKY lane is
// the semantic content, exactly as zfield::Status defines it.
//
// Lanes: bare = directed (committed programs, boundary grids, prepared/cold
// ring, rcp0, fixed random). --random N = N random legal programs with
// random varying masks. The generator maintains the validator's invariants
// (def-before-use, dst not overlapping inputs or own sources, groups in
// range, imm discipline) so every generated program is one decode() would
// admit.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "crater_ring.hpp"  // TS-generated (compiler/tests/generated)
#include "impact_wave.hpp"
#include "wave_pool.hpp"

#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"
#include "zfield/zfield_steps.hpp"

namespace {

int failures = 0;
#define CHECK(cond, msg)         \
  do {                           \
    if (!(cond)) {               \
      printf("FAIL: %s\n", msg); \
      ++failures;                \
    } else {                     \
      printf("ok: %s\n", msg);   \
    }                            \
  } while (0)

// Quiet check for the inner comparison loops: only failures print.
int quiet_checks = 0;
#define QCHECK(cond, msg)          \
  do {                             \
    ++quiet_checks;                \
    if (!(cond)) {                 \
      printf("FAIL: %s\n", (msg)); \
      ++failures;                  \
    }                              \
  } while (0)

// ---- deterministic RNG (PCG32) ---------------------------------------------

struct Rng {
  uint64_t state;
  explicit Rng(uint64_t seed) : state(seed * 6364136223846793005ull + 1442695040888963407ull) {}
  uint32_t next() {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    uint32_t xorshifted = (uint32_t)(((state >> 18u) ^ state) >> 27u);
    uint32_t rot = (uint32_t)(state >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

// Boundary-biased value pool: the lanes that saturate live at the rails.
int32_t interesting(Rng& r) {
  switch (r.below(12)) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return -1;
    case 3:
      return 0x10000;
    case 4:
      return -0x10000;
    case 5:
      return INT32_MAX;
    case 6:
      return INT32_MIN;
    case 7:
      return INT32_MAX - 1;
    case 8:
      return INT32_MIN + 1;
    case 9:
      return (int32_t)(r.next() & 0xFFFF);  // small
    default:
      return (int32_t)r.next();  // anything
  }
}

// ---- the differential core --------------------------------------------------

struct DiffStats {
  int points = 0;
  int sat_seen = 0;
  int nosat_seen = 0;
  int rcp0_seen = 0;
};

// Compare interpret vs prepare+execute for one program, one uniform input
// record, over `n_pts` points whose varying lanes are drawn from the pool.
void diff_instance(const zfield::Decoded& prog, const zfield::Fplan& fp, uint32_t varying_mask,
                   const std::vector<int32_t>& base_in, int n_pts, Rng& rng, DiffStats& st,
                   const char* tag) {
  const size_t n_in = prog.in_lanes.size();
  const size_t n_out = prog.out_lanes.size();
  const zfield::Prepared prep = zfield::prepare(fp, prog, base_in.data(), n_in);

  std::vector<int32_t> in = base_in;
  std::vector<int32_t> out_ref(n_out, 0), out_pl(n_out, 0);
  char msg[192];
  for (int p = 0; p < n_pts; ++p) {
    for (size_t i = 0; i < n_in; ++i) {
      if (varying_mask & (1u << i)) in[i] = interesting(rng);
    }
    zref::SatLedger lref = {}, lvar = {};
    const zfield::Status sref =
        zfield::interpret(prog, in.data(), n_in, out_ref.data(), n_out, &lref);
    const zfield::Status spl =
        zfield::execute_point(fp, prog, prep, in.data(), n_in, out_pl.data(), n_out, &lvar);

    bool out_ok = true;
    for (size_t o = 0; o < n_out; ++o) out_ok = out_ok && out_ref[o] == out_pl[o];
    if (!out_ok) {
      snprintf(msg, sizeof msg, "%s: output lanes diverge at point %d", tag, p);
      QCHECK(false, msg);
    } else {
      ++quiet_checks;
    }

    const bool status_ok = sref.sat == spl.sat && sref.rcp0 == spl.rcp0;
    if (!status_ok) {
      snprintf(msg, sizeof msg, "%s: Status diverges at point %d (sat %d/%d rcp0 %d/%d)", tag, p,
               sref.sat, spl.sat, sref.rcp0, spl.rcp0);
      QCHECK(false, msg);
    } else {
      ++quiet_checks;
    }

    // EVERY saturation lane, as sticky booleans (uniform OR varying).
    const zref::SatLedger& lu = prep.uniform_ledger;
    const bool lanes_ok = ((lref.add != 0) == (lu.add + lvar.add != 0)) &&
                          ((lref.mul != 0) == (lu.mul + lvar.mul != 0)) &&
                          ((lref.rescale != 0) == (lu.rescale + lvar.rescale != 0)) &&
                          ((lref.unit != 0) == (lu.unit + lvar.unit != 0)) &&
                          ((lref.rcp != 0) == (lu.rcp + lvar.rcp != 0)) &&
                          ((lref.rcp0 != 0) == (lu.rcp0 + lvar.rcp0 != 0));
    if (!lanes_ok) {
      snprintf(msg, sizeof msg, "%s: a saturation LANE diverges at point %d", tag, p);
      QCHECK(false, msg);
    } else {
      ++quiet_checks;
    }

    ++st.points;
    if (sref.sat)
      ++st.sat_seen;
    else
      ++st.nosat_seen;
    if (sref.rcp0) ++st.rcp0_seen;
  }
}

// ---- random legal program generation ---------------------------------------
// Maintains the validator's invariants so every program is decode()-shaped:
// def-before-use (V11), dst groups inside R0..R63 (V7), dst overlapping
// neither an input lane nor its own sources (V8), imm discipline (V9),
// exactly one END, last (V10).

zfield::Table random_table(Rng& rng) {
  zfield::Table t;
  t.kind = (uint8_t)rng.below(2);
  const int n = 2 + (int)rng.below(6);  // 2..7 knots
  int32_t x = (int32_t)(rng.below(0x20000)) - 0x10000;
  for (int i = 0; i < n; ++i) {
    t.x.push_back(x);
    x += 1 + (int32_t)rng.below(0x18000);  // strictly increasing
    t.y.push_back(interesting(rng));
    t.dy.push_back((int32_t)(rng.below(0x40000)) - 0x20000);
  }
  return t;
}

zfield::Decoded random_program(Rng& rng, uint32_t* varying_mask_out) {
  zfield::Decoded d;
  d.profile = 0;
  d.tables.push_back(random_table(rng));
  d.tables.push_back(random_table(rng));

  const int n_in = 3 + (int)rng.below(4);  // 3..6 input lanes at R0..
  for (int i = 0; i < n_in; ++i) {
    zfield::IoLane l;
    l.name = "in";
    l.type = 0;
    l.reg = (uint8_t)i;
    d.in_lanes.push_back(l);
  }
  bool defined[zfield::REG_COUNT] = {};
  bool is_input[zfield::REG_COUNT] = {};
  for (int i = 0; i < n_in; ++i) defined[i] = is_input[i] = true;

  // executable ops = every canonical op except END
  std::vector<const zfield::optable::OpShape*> pool;
  for (const auto& s : zfield::optable::OPS) {
    if (s.code != zfield::OP_END) pool.push_back(&s);
  }

  const int n_instr = 4 + (int)rng.below(24);  // 4..27 + END
  for (int k = 0; k < n_instr; ++k) {
    const auto* sh = pool[rng.below((uint32_t)pool.size())];
    zfield::Instr ins = {};
    ins.op = sh->code;

    // choose source group starts among DEFINED registers (whole group defined)
    uint8_t starts[3] = {0, 0, 0};
    bool ok = true;
    for (int g = 0; g < sh->n_groups && ok; ++g) {
      const int w = sh->group_width[g];
      // collect candidates
      uint8_t cand[zfield::REG_COUNT];
      int nc = 0;
      for (int r = 0; r + w <= (int)zfield::REG_COUNT; ++r) {
        bool all = true;
        for (int j = 0; j < w; ++j) all = all && defined[r + j];
        if (all) cand[nc++] = (uint8_t)r;
      }
      if (nc == 0) {
        ok = false;
      } else {
        starts[g] = cand[rng.below((uint32_t)nc)];
      }
    }
    if (!ok) {
      --k;
      continue;
    }
    ins.a = starts[0];
    ins.b = starts[1];
    ins.c = starts[2];

    // choose a dst group: inside the file, not an input lane, not overlapping
    // this instruction's own sources
    const int dw = sh->dst_width;
    if (dw > 0) {
      uint8_t cand[zfield::REG_COUNT];
      int nc = 0;
      for (int r = 0; r + dw <= (int)zfield::REG_COUNT; ++r) {
        bool legal = true;
        for (int j = 0; j < dw && legal; ++j) {
          if (is_input[r + j]) legal = false;
          for (int g = 0; g < sh->n_groups && legal; ++g) {
            for (int w = 0; w < sh->group_width[g]; ++w) {
              if (r + j == starts[g] + w) legal = false;
            }
          }
        }
        if (legal) cand[nc++] = (uint8_t)r;
      }
      ins.dst = cand[rng.below((uint32_t)nc)];
      for (int j = 0; j < dw; ++j) defined[ins.dst + j] = true;
    }

    // imm discipline (V9)
    switch (sh->imm_kind) {
      case zfield::optable::IMM_RAW:
        ins.imm = rng.next();
        break;
      case zfield::optable::IMM_CMP:
        ins.imm = rng.below(6);
        break;
      case zfield::optable::IMM_TABLE:
        ins.imm = rng.below((uint32_t)d.tables.size());
        break;
      case zfield::optable::IMM_SEED:
        ins.imm = rng.next();
        break;
      case zfield::optable::IMM_ROT3_AXIS:
        ins.imm = rng.below(3);
        break;
      default:
        ins.imm = 0;
        break;
    }
    d.instrs.push_back(ins);
  }
  zfield::Instr end = {};
  end.op = zfield::OP_END;
  d.instrs.push_back(end);

  // outputs: 1..4 defined registers
  const int n_out = 1 + (int)rng.below(4);
  for (int i = 0; i < n_out; ++i) {
    uint8_t cand[zfield::REG_COUNT];
    int nc = 0;
    for (int r = 0; r < (int)zfield::REG_COUNT; ++r) {
      if (defined[r]) cand[nc++] = (uint8_t)r;
    }
    zfield::IoLane l;
    l.name = "out";
    l.type = 0;
    l.reg = cand[rng.below((uint32_t)nc)];
    d.out_lanes.push_back(l);
  }

  d.program_hash = rng.next();  // identity only; not semantic here

  // random varying mask, biased away from empty but covering it
  uint32_t mask = rng.below(1u << n_in);
  *varying_mask_out = mask;
  return d;
}

// ---- directed helpers -------------------------------------------------------

// A minimal Earth-shaped RING program built directly as a Decoded view.
// ring_from_x: r0 comes from the varying lane (cold monolithic RING);
// otherwise radii are the two parameter lanes (prepared ring).
zfield::Decoded ring_program(bool ring_from_x, bool equal_radii) {
  zfield::Decoded d;
  d.profile = 0;
  const char* names[4] = {"x", "z", "r0", "r1"};
  for (int i = 0; i < 4; ++i) {
    zfield::IoLane l;
    l.name = names[i];
    l.type = 0;
    l.reg = (uint8_t)i;
    d.in_lanes.push_back(l);
  }
  // R4 = DIST2((R0,R1),(R2,R3)) — varying distance-ish value
  zfield::Instr i0 = {};
  i0.op = zfield::OP_DIST2;
  i0.dst = 4;
  i0.a = 0;
  i0.b = 2;
  d.instrs.push_back(i0);
  // R5 = RING(d = R4, r0, r1)
  zfield::Instr i1 = {};
  i1.op = zfield::OP_RING;
  i1.dst = 5;
  i1.a = 4;
  i1.b = ring_from_x ? 0 : 2;
  i1.c = equal_radii ? i1.b : 3;
  d.instrs.push_back(i1);
  zfield::Instr end = {};
  end.op = zfield::OP_END;
  d.instrs.push_back(end);
  zfield::IoLane o;
  o.name = "h";
  o.type = 0;
  o.reg = 5;
  d.out_lanes.push_back(o);
  d.program_hash = 0xF1E57A00u | (ring_from_x ? 1 : 0) | (equal_radii ? 2 : 0);
  return d;
}

void run_committed(const char* name, const uint8_t* bytes, size_t len, uint32_t golden_hash,
                   const zfield::Demand& want, Rng& rng) {
  char msg[160];
  const zfield::DecodeResult dec = zfield::decode(bytes, len);
  snprintf(msg, sizeof msg, "%s: decodes + re-validates", name);
  CHECK(dec.error == zfield::DecodeError::kOk, msg);
  if (dec.error != zfield::DecodeError::kOk) return;

  const zfield::Fplan fp = zfield::plan(dec.prog, 0b11);  // Earth: x,z vary
  snprintf(msg, sizeof msg, "%s: plan carries the canonical hash", name);
  CHECK(fp.canonical_hash == golden_hash, msg);
  snprintf(msg, sizeof msg, "%s: plan is realtime/hot", name);
  CHECK(fp.perf_class == zfield::PlanClass::kHot, msg);

  // Pin the demand vector to the regenerated cost model
  // (reports/FIELD_V3_COST_MODEL.md §3): a classifier drift moves these.
  const zfield::Demand& d = fp.demand;
  const bool demand_ok = d.vec_issue == want.vec_issue && d.vmul_slots == want.vmul_slots &&
                         d.curve_req == want.curve_req && d.dist_req == want.dist_req &&
                         d.cold_ops == want.cold_ops && d.uniform_ops == want.uniform_ops;
  snprintf(msg, sizeof msg,
           "%s: demand vector matches the cost model (issue %u mul %u curve %u dist %u cold %u "
           "uni %u)",
           name, d.vec_issue, d.vmul_slots, d.curve_req, d.dist_req, d.cold_ops, d.uniform_ops);
  CHECK(demand_ok, msg);

  // Differential across several uniform instances x many points, including a
  // dense boundary grid on x,z for the first instance.
  DiffStats st;
  const size_t n_in = dec.prog.in_lanes.size();
  for (int inst = 0; inst < 6; ++inst) {
    std::vector<int32_t> base(n_in, 0);
    for (size_t i = 2; i < n_in; ++i) {
      // instance 0: in-bounds-ish parameters; later instances: hostile
      base[i] = (inst == 0) ? (int32_t)(rng.below(0x200000)) : interesting(rng);
    }
    const zfield::Prepared prep = zfield::prepare(fp, dec.prog, base.data(), n_in);
    (void)prep;
    if (inst == 0) {
      // boundary grid: 9x9 rails cross on x,z
      static const int32_t rail[9] = {INT32_MIN, INT32_MIN + 1, -0x10000,      -1,       0,
                                      1,         0x10000,       INT32_MAX - 1, INT32_MAX};
      std::vector<int32_t> in = base;
      std::vector<int32_t> o_ref(dec.prog.out_lanes.size()), o_pl(dec.prog.out_lanes.size());
      const zfield::Prepared pr = zfield::prepare(fp, dec.prog, base.data(), n_in);
      for (int xi = 0; xi < 9; ++xi) {
        for (int zi = 0; zi < 9; ++zi) {
          in[0] = rail[xi];
          in[1] = rail[zi];
          zref::SatLedger lr = {}, lv = {};
          const zfield::Status sr =
              zfield::interpret(dec.prog, in.data(), n_in, o_ref.data(), o_ref.size(), &lr);
          const zfield::Status sp = zfield::execute_point(fp, dec.prog, pr, in.data(), n_in,
                                                          o_pl.data(), o_pl.size(), &lv);
          bool same = sr.sat == sp.sat && sr.rcp0 == sp.rcp0;
          for (size_t o = 0; o < o_ref.size(); ++o) same = same && o_ref[o] == o_pl[o];
          const zref::SatLedger& lu = pr.uniform_ledger;
          same = same && ((lr.add != 0) == (lu.add + lv.add != 0)) &&
                 ((lr.mul != 0) == (lu.mul + lv.mul != 0)) &&
                 ((lr.rescale != 0) == (lu.rescale + lv.rescale != 0)) &&
                 ((lr.unit != 0) == (lu.unit + lv.unit != 0)) &&
                 ((lr.rcp != 0) == (lu.rcp + lv.rcp != 0)) &&
                 ((lr.rcp0 != 0) == (lu.rcp0 + lv.rcp0 != 0));
          snprintf(msg, sizeof msg, "%s: boundary grid (%d,%d)", name, xi, zi);
          QCHECK(same, msg);
          ++st.points;
          if (sr.sat)
            ++st.sat_seen;
          else
            ++st.nosat_seen;
        }
      }
    }
    snprintf(msg, sizeof msg, "%s inst %d", name, inst);
    diff_instance(dec.prog, fp, 0b11, base, 200, rng, st, msg);
  }
  snprintf(msg, sizeof msg, "%s: sensitivity — both saturating and clean points sampled", name);
  CHECK(st.sat_seen > 0 && st.nosat_seen > 0, msg);
}

}  // namespace

int main(int argc, char** argv) {
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }
  Rng rng(random_n ? 0xC0FFEEu + (unsigned)random_n : 0x5EEDu);

  if (random_n == 0) {
    printf("== the three committed Earth programs ==\n");
    {
      zfield::Demand w = {};
      w.vec_issue = 16;
      w.vmul_slots = 11;
      w.curve_req = 1;
      w.dist_req = 1;
      w.cold_ops = 0;
      w.uniform_ops = 13;
      run_committed("impact_wave", zfield_gen::impact_wave::kProgramBytes.data(),
                    zfield_gen::impact_wave::kProgramBytesLen, 0x82f5f4e4u, w, rng);
    }
    {
      zfield::Demand w = {};
      w.vec_issue = 17;
      w.vmul_slots = 11;
      w.curve_req = 0;
      w.dist_req = 1;
      w.cold_ops = 0;
      w.uniform_ops = 9;
      run_committed("wave_pool", zfield_gen::wave_pool::kProgramBytes.data(),
                    zfield_gen::wave_pool::kProgramBytesLen, 0x8bdceb63u, w, rng);
    }
    {
      zfield::Demand w = {};
      // 12 plain varying + 1 prepared ring; ring adds 9 mul slots; the 5
      // ring-prep steps join the 14 canonical uniform instructions.
      w.vec_issue = 13;
      w.vmul_slots = 18;
      w.curve_req = 0;
      w.dist_req = 1;
      w.cold_ops = 0;
      w.uniform_ops = 14 + 5;
      run_committed("crater_ring", zfield_gen::crater_ring::kProgramBytes.data(),
                    zfield_gen::crater_ring::kProgramBytesLen, 0x484add8du, w, rng);
    }

    printf("== prepared ring vs cold ring vs rcp0 ==\n");
    {
      // prepared ring (radii uniform): the plan must contain UOP_RING_PREP
      const zfield::Decoded d = ring_program(false, false);
      const zfield::Fplan fp = zfield::plan(d, 0b11);
      bool has_prep_ring = false;
      for (const auto& u : fp.uops) has_prep_ring = has_prep_ring || u.op == zfield::UOP_RING_PREP;
      CHECK(has_prep_ring, "uniform-radii RING lowers to UOP_RING_PREP");
      CHECK(fp.demand.cold_ops == 0, "prepared ring is not cold");
      DiffStats st;
      std::vector<int32_t> base = {0, 0, 0x40000, 0xC0000};
      diff_instance(d, fp, 0b11, base, 400, rng, st, "ring prepared");
      CHECK(st.sat_seen > 0 && st.nosat_seen > 0, "ring prepared: sensitivity");
    }
    {
      // cold ring (r0 varying): monolithic RING uop on the cold lane
      const zfield::Decoded d = ring_program(true, false);
      const zfield::Fplan fp = zfield::plan(d, 0b11);
      bool has_prep_ring = false;
      for (const auto& u : fp.uops) has_prep_ring = has_prep_ring || u.op == zfield::UOP_RING_PREP;
      CHECK(!has_prep_ring, "varying-radius RING stays monolithic");
      CHECK(fp.demand.cold_ops == 1, "varying-radius RING is cold-lane demand");
      CHECK(fp.perf_class == zfield::PlanClass::kCold, "cold demand declassifies the plan");
      DiffStats st;
      std::vector<int32_t> base = {0, 0, 0x40000, 0xC0000};
      diff_instance(d, fp, 0b11, base, 400, rng, st, "ring cold");
    }
    {
      // equal uniform radii: m == r0 == r1, both smoothstep spans are zero ->
      // rcp0 fires in PREPARATION and must surface identically.
      const zfield::Decoded d = ring_program(false, true);
      const zfield::Fplan fp = zfield::plan(d, 0b11);
      std::vector<int32_t> base = {0, 0, 0x40000, 0x40000};
      const zfield::Prepared prep = zfield::prepare(fp, d, base.data(), base.size());
      CHECK(prep.uniform_status.rcp0, "equal radii: rcp0 fires in the uniform block");
      DiffStats st;
      diff_instance(d, fp, 0b11, base, 100, rng, st, "ring rcp0");
      CHECK(st.rcp0_seen == st.points, "rcp0 surfaces at every point, as canonical does");
    }

    printf("== ring_mid law pins ==\n");
    {
      // §3.17: m = rescale_s32((s64)r0 + r1, 1) — round-half-up over the
      // EXACT 33-bit sum, never a saturating add. The mutation sweep found
      // that the whole differential corpus (and the crater golden vectors)
      // never sampled an ODD or OVERFLOWING radii sum, so a floor-rounding
      // or fused-saturating midpoint survived every lane. These pins are the
      // closure: values are the law's own arithmetic, cited per case.
      zref::SatLedger L = {};
      CHECK(zfield::steps::ring_mid(3, 4, &L) == 4, "ring_mid(3,4) rounds half up: (7+1)>>1 = 4");
      CHECK(zfield::steps::ring_mid(-5, 2, &L) == -1,
            "ring_mid(-5,2) rounds half up: (-3+1)>>1 = -1");
      CHECK(zfield::steps::ring_mid(INT32_MAX, INT32_MAX, &L) == INT32_MAX,
            "ring_mid(MAX,MAX): the exact 33-bit sum does NOT saturate before the shift");
      CHECK(zfield::steps::ring_mid(INT32_MAX, 2, &L) == 0x40000001,
            "ring_mid(MAX,2): (2147483649+1)>>1 = 0x40000001 — a saturating add would clamp first");
    }

    printf("== directed classifier pins ==\n");
    {
      // A varying value overwritten by a UNIFORM redefinition: the kill in the
      // taint walk is load-bearing. R10 = MUL(x,x) varying, then R10 = LDC
      // (uniform); the output must come from the SCALAR bank.
      zfield::Decoded d;
      d.profile = 0;
      zfield::IoLane lx;
      lx.name = "x";
      lx.type = 0;
      lx.reg = 0;
      d.in_lanes.push_back(lx);
      zfield::Instr m0 = {};
      m0.op = zfield::OP_MUL;
      m0.dst = 10;
      m0.a = 0;
      m0.b = 0;
      d.instrs.push_back(m0);
      zfield::Instr l0 = {};
      l0.op = zfield::OP_LDC;
      l0.dst = 10;
      l0.imm = 0x50000;
      d.instrs.push_back(l0);
      zfield::Instr end = {};
      end.op = zfield::OP_END;
      d.instrs.push_back(end);
      zfield::IoLane o;
      o.name = "h";
      o.type = 0;
      o.reg = 10;
      d.out_lanes.push_back(o);
      d.program_hash = 0xF1A70001u;
      const zfield::Fplan fp = zfield::plan(d, 0b1);
      CHECK(fp.out_map.size() == 1 && fp.out_map[0].kind == zfield::SrcKind::kSca,
            "uniform redefinition KILLS varying taint (output reads the scalar bank)");
      DiffStats st;
      std::vector<int32_t> base = {0};
      diff_instance(d, fp, 0b1, base, 60, rng, st, "uniform redef");
    }
    {
      // Saturation that happens ONLY in the uniform block: the combined
      // Status must carry it to every point even when the varying half is
      // clean. R2 = ADD(p, p) with p = INT32_MAX saturates in preparation;
      // R4 = ADD(x, x) with tiny x never does.
      zfield::Decoded d;
      d.profile = 0;
      zfield::IoLane lx;
      lx.name = "x";
      lx.type = 0;
      lx.reg = 0;
      d.in_lanes.push_back(lx);
      zfield::IoLane lp;
      lp.name = "p";
      lp.type = 0;
      lp.reg = 1;
      d.in_lanes.push_back(lp);
      zfield::Instr a0 = {};
      a0.op = zfield::OP_ADD;
      a0.dst = 2;
      a0.a = 1;
      a0.b = 1;
      d.instrs.push_back(a0);
      zfield::Instr a1 = {};
      a1.op = zfield::OP_ADD;
      a1.dst = 4;
      a1.a = 0;
      a1.b = 0;
      d.instrs.push_back(a1);
      zfield::Instr end = {};
      end.op = zfield::OP_END;
      d.instrs.push_back(end);
      zfield::IoLane o0;
      o0.name = "u";
      o0.type = 0;
      o0.reg = 2;
      d.out_lanes.push_back(o0);
      zfield::IoLane o1;
      o1.name = "v";
      o1.type = 0;
      o1.reg = 4;
      d.out_lanes.push_back(o1);
      d.program_hash = 0xF1A70002u;
      const zfield::Fplan fp = zfield::plan(d, 0b1);
      std::vector<int32_t> base = {0, INT32_MAX};
      const zfield::Prepared prep = zfield::prepare(fp, d, base.data(), 2);
      CHECK(prep.uniform_status.sat, "uniform-only saturation fires in preparation");
      std::vector<int32_t> in = {7, INT32_MAX};
      int32_t o_ref[2], o_pl[2];
      zref::SatLedger lv = {};
      const zfield::Status sr = zfield::interpret(d, in.data(), 2, o_ref, 2);
      const zfield::Status sp = zfield::execute_point(fp, d, prep, in.data(), 2, o_pl, 2, &lv);
      CHECK(lv.add == 0, "the varying half is clean at this point");
      CHECK(sr.sat && sp.sat && o_ref[0] == o_pl[0] && o_ref[1] == o_pl[1],
            "uniform-only saturation reaches the combined Status at a clean point");
    }
    {
      // Occupancy-driven declassification: two varying DIST2s put the DIST
      // service at 2 x 273 x 20 = 10,920 clocks/association — over the
      // 6,000 deadline, so the plan must be COLD even with zero cold ops.
      zfield::Decoded d;
      d.profile = 0;
      const char* nm[4] = {"x", "z", "cx", "cz"};
      for (int i = 0; i < 4; ++i) {
        zfield::IoLane l;
        l.name = nm[i];
        l.type = 0;
        l.reg = (uint8_t)i;
        d.in_lanes.push_back(l);
      }
      zfield::Instr d0 = {};
      d0.op = zfield::OP_DIST2;
      d0.dst = 4;
      d0.a = 0;
      d0.b = 2;
      d.instrs.push_back(d0);
      zfield::Instr d1 = {};
      d1.op = zfield::OP_DIST2;
      d1.dst = 5;
      d1.a = 0;
      d1.b = 2;
      d.instrs.push_back(d1);
      zfield::Instr end = {};
      end.op = zfield::OP_END;
      d.instrs.push_back(end);
      zfield::IoLane o;
      o.name = "h";
      o.type = 0;
      o.reg = 5;
      d.out_lanes.push_back(o);
      d.program_hash = 0xF1A70003u;
      const zfield::Fplan fp = zfield::plan(d, 0b11);
      CHECK(fp.demand.cold_ops == 0 && fp.demand.dist_req == 2,
            "two varying DIST2s, no cold demand");
      CHECK(fp.perf_class == zfield::PlanClass::kCold,
            "DIST occupancy over the deadline declassifies the plan to cold");
    }

    printf("== fixed random sweep (seed pinned) ==\n");
    {
      DiffStats st;
      int hot = 0, cold = 0;
      for (int i = 0; i < 25; ++i) {
        uint32_t mask = 0;
        const zfield::Decoded d = random_program(rng, &mask);
        const zfield::Fplan fp = zfield::plan(d, mask);
        (fp.perf_class == zfield::PlanClass::kHot ? hot : cold)++;
        std::vector<int32_t> base(d.in_lanes.size());
        for (auto& v : base) v = interesting(rng);
        char tag[48];
        snprintf(tag, sizeof tag, "fixed random %d", i);
        diff_instance(d, fp, mask, base, 120, rng, st, tag);
      }
      CHECK(st.sat_seen > 0 && st.nosat_seen > 0, "fixed random: saturation both ways sampled");
      CHECK(st.rcp0_seen > 0, "fixed random: rcp0 sampled");
      CHECK(hot > 0 && cold > 0, "fixed random: both plan classes produced");
      printf("   (%d programs: %d hot, %d cold; %d points, %d sat, %d rcp0)\n", 25, hot, cold,
             st.points, st.sat_seen, st.rcp0_seen);
    }
  } else {
    printf("== random legal programs: %d ==\n", random_n);
    DiffStats st;
    for (int i = 0; i < random_n; ++i) {
      uint32_t mask = 0;
      const zfield::Decoded d = random_program(rng, &mask);
      const zfield::Fplan fp = zfield::plan(d, mask);
      for (int inst = 0; inst < 3; ++inst) {
        std::vector<int32_t> base(d.in_lanes.size());
        for (auto& v : base) v = interesting(rng);
        char tag[48];
        snprintf(tag, sizeof tag, "random %d inst %d", i, inst);
        diff_instance(d, fp, mask, base, 60, rng, st, tag);
      }
    }
    CHECK(st.sat_seen > 0 && st.nosat_seen > 0, "random: saturation both ways sampled");
    CHECK(st.rcp0_seen > 0, "random: rcp0 sampled");
    printf("   (%d points, %d sat, %d rcp0)\n", st.points, st.sat_seen, st.rcp0_seen);
  }

  printf("[field_fplan_diff] %d checks passed (%d quiet), %d failures\n", quiet_checks,
         quiet_checks, failures);
  return failures == 0 ? 0 : 1;
}
