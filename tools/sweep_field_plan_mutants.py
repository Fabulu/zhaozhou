#!/usr/bin/env python3
"""Mutation table for the FIELD v3 planner sweep (tools/sweep_field_plan.sh).

Targets the Phase 2 seam of reports/Fieldv3.md: the exact software planner
(reference/src/zfield/zfield_plan.cpp) and the shared semantic step layer
(reference/include/zfield/zfield_steps.hpp). The mutation classes are the
brief's own list where it applies to software: uniform/varying
misclassification, wrong canonical-to-uop mapping, stale prepared values,
prepared-ring rounding removed or fused, dropped uniform Status, wrong
output tags, and prediction counters that undercount real demand.

The table lives in Python, not bash, because mutations contain characters
bash would mangle (house rule from sweep_geom_wcache_mutants.py, guard 6).

Each entry: (name, relative file, old, new). Applied to the PRISTINE file,
one at a time; the anchor must match exactly once.

STEP-LAYER MUTANTS AND THE ORACLE: a mutation in zfield_steps.hpp that hits
a CANONICAL op changes interpret() and the plan executor IDENTICALLY, so the
planner differential alone is blind to it — which is precisely why the sweep
also runs test_field_crater_ring, whose committed golden .zvec pins
interpret() itself. A steps mutant that survives the differential but fails
the golden replay is scored CAUGHT, as it should be.
"""

PLAN = "reference/src/zfield/zfield_plan.cpp"
STEPS = "reference/include/zfield/zfield_steps.hpp"

MUTANTS = [
    ("P01 taint reads only the FIRST member of each operand group",
     PLAN,
     """        for (int w = 0; w < sh->group_width[g]; ++w) {
          const RegLoc& l = loc[starts[g] + w];
          if (l.defined && l.varying) any_varying = true;
        }""",
     """        for (int w = 0; w < 1 && w < sh->group_width[g]; ++w) {
          const RegLoc& l = loc[starts[g] + w];
          if (l.defined && l.varying) any_varying = true;
        }"""),

    ("P02 a uniform redefinition no longer KILLS the varying flag",
     PLAN,
     "      loc[ins.dst + w] = RegLoc{true, false, (uint16_t)(u.dst + w)};",
     "      loc[ins.dst + w] = RegLoc{true, loc[ins.dst + w].varying, (uint16_t)(u.dst + w)};"),

    ("P03 a varying group member collapses to the group start",
     PLAN,
     "          v.src[k++] = UopSrc{SrcKind::kVec, r};  // compacted below",
     "          v.src[k++] = UopSrc{SrcKind::kVec, starts[g]};  // compacted below"),

    ("P04 the valid-but-different opcode mapping (ADD lowered as SUB)",
     PLAN,
     "    v.op = ins.op;",
     "    v.op = (ins.op == OP_ADD) ? (uint8_t)OP_SUB : ins.op;"),

    ("P05 preparation seeds only the first uniform input lane (stale zeros)",
     PLAN,
     "    if (fp.in_slot[i] != kNoSlot) out.scalar[fp.in_slot[i]] = in[i];",
     "    if (fp.in_slot[i] != kNoSlot && i == 0) out.scalar[fp.in_slot[i]] = in[i];"),

    ("P06 preparation writes only the first destination lane of a group",
     PLAN,
     "    for (int i = 0; i < w; ++i) out.scalar[u.dst + i] = dst[i];",
     "    for (int i = 0; i < 1 && i < w; ++i) out.scalar[u.dst + i] = dst[i];"),

    ("P07 ring midpoint rounding replaced by saturating-add-then-floor",
     STEPS,
     "  return zref::rescale_s32((int64_t)r0 + r1, 1, L);",
     "  return zref::fx_add(F(r0), F(r1), L).raw >> 1;"),

    ("P08 prepared ring FUSES the sub into the product (single-rounding law)",
     STEPS,
     "  fx16 t0 = zref::fx_mul(zref::fx_sub(F(d), F(r0), L), F(rA), L);",
     "  fx16 t0 = F(zref::rescale_s32(((int64_t)d - (int64_t)r0) * rA, 16, L));"),

    ("P09 prepared ring drops the smoothstep clamp",
     STEPS,
     "  t0 = zref::fx_clamp(t0, F(0), F(1 << 16));",
     "  t0 = t0;"),

    ("P10 combined Status drops the uniform block's saturation",
     PLAN,
     "  return Status{prep.uniform_status.sat || sat, prep.uniform_status.rcp0 || (L.rcp0 != 0)};",
     "  return Status{sat, prep.uniform_status.rcp0 || (L.rcp0 != 0)};"),

    ("P11 combined Status drops the uniform block's rcp0",
     PLAN,
     "  return Status{prep.uniform_status.sat || sat, prep.uniform_status.rcp0 || (L.rcp0 != 0)};",
     "  return Status{prep.uniform_status.sat || sat, (L.rcp0 != 0)};"),

    ("P12 uniform outputs all read scalar slot zero",
     PLAN,
     "        fp.out_map.push_back(OutTag{SrcKind::kSca, l.defined ? l.slot : slot_of(o.reg)});",
     "        fp.out_map.push_back(OutTag{SrcKind::kSca, (uint16_t)0});"),

    ("P13 prepared ring undercounts its multiplier demand (9 -> 8)",
     PLAN,
     "    fp.demand.vmul_slots += 9;",
     "    fp.demand.vmul_slots += 8;"),

    ("P14 distance-service demand vanishes from the vector",
     PLAN,
     """      case optable::SVC_DIST:
        fp.demand.dist_req += 1;""",
     """      case optable::SVC_DIST:"""),

    ("P15 the admission deadline grows an order of magnitude",
     PLAN,
     "    const bool hot = fp.demand.cold_ops == 0 && bind <= 6000 && fp.n_vreg <= 32;",
     "    const bool hot = fp.demand.cold_ops == 0 && bind <= 60000 && fp.n_vreg <= 32;"),

    ("P16 the executor seeds only the first varying input lane",
     PLAN,
     "    if (fp.in_vreg[i] != kNoVreg) vec[fp.in_vreg[i]] = in[i];",
     "    if (fp.in_vreg[i] != kNoVreg && i == 0) vec[fp.in_vreg[i]] = in[i];"),
]

# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {}


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
    nl = "\r\n" if "\r\n" in gold else "\n"
    o = old.replace("\n", nl)
    n = new.replace("\n", nl)
    count = gold.count(o)
    if count != 1:
        raise ValueError("anchor matches %d times" % count)
    if o == n:
        raise ValueError("mutant identical to base")
    return gold.replace(o, n, 1)


if __name__ == "__main__":
    import sys

    # LF-only stdout: bash command substitution must not capture CRs
    sys.stdout.reconfigure(newline="\n")

    if len(sys.argv) >= 2 and sys.argv[1] == "--count":
        print(len(MUTANTS))
    elif len(sys.argv) >= 3 and sys.argv[1] == "--name":
        print(MUTANTS[int(sys.argv[2])][0])
    elif len(sys.argv) >= 3 and sys.argv[1] == "--file":
        print(MUTANTS[int(sys.argv[2])][1])
    elif len(sys.argv) >= 3 and sys.argv[1] == "--apply":
        idx = int(sys.argv[2])
        name, path, old, new = MUTANTS[idx]
        with open(path, "r", encoding="utf-8", newline="") as f:
            gold = f.read()
        out = mutate(gold, old, new)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(out)
        print("applied %s to %s" % (name.split()[0], path))
    elif len(sys.argv) >= 3 and sys.argv[1] == "--equiv":
        tok = sys.argv[2]
        if tok in EQUIVALENT:
            print(EQUIVALENT[tok])
        else:
            sys.exit(1)
    else:
        print("usage: --count | --name N | --file N | --apply N | --equiv TOK")
        sys.exit(2)
