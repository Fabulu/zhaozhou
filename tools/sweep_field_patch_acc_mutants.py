#!/usr/bin/env python3
"""Mutation table for the FIELD v3 four-bank patch accumulator probe sweep
(tools/sweep_field_patch_acc.sh).

Target: fpga/rtl/synth/zhao_probe_patch_acc.sv ONLY — the probe has exactly
one consumer (guard 7).

Classes from the TERRAIN.PATCH field-major amendment where they exist in
this seam: output reducer swapped/pooled (the amendment's own named
mutation targets), masked vertex incorrectly written, wrong-lane routing
(rotation), the RMW bypass (inverted, lane-collapsed), both bottom clamps,
the dirty law, the legacy bottom lane, saturation-lane misattribution,
init leakage between lanes, and non-writer clobber of the material lane.
"""

RTL = "fpga/rtl/synth/zhao_probe_patch_acc.sv"

MUTANTS = [
    ("P01 material pools into an add (the generic-reducer defect)",
     RTL,
     "      new_m[b]  = m_m[b];  // writer-selection: this writer wins wholesale",
     "      new_m[b]  = q_m[b] + m_m[b];  // writer-selection: this writer wins wholesale"),

    ("P02 the height add wraps instead of saturating",
     RTL,
     "      new_h[b]  = fx_add_sat(old_h[b], m_h[b]);",
     "      new_h[b]  = old_h[b] + m_h[b];"),

    ("P03 the RMW bypass hits on the WRONG address compare (inverted)",
     RTL,
     "      by_hit[b] = by_valid[b] && (by_addr[b] == m_addr[b]);",
     "      by_hit[b] = by_valid[b] && (by_addr[b] != m_addr[b]);"),

    # Reshaped for lint: gating velocity by the height bit alone orphans
    # by_wv (UNUSEDSIGNAL); CROSSING the two gates keeps every signal read
    # and is the same per-lane-bypass-collapse defect.
    ("P04 the height and velocity bypass gates are crossed",
     RTL,
     "      old_h[b] = (by_hit[b] && by_wh[b]) ? by_h[b] : $signed(q_h[b]);\n"
     "      old_v[b] = (by_hit[b] && by_wv[b]) ? by_v[b] : $signed(q_v[b]);",
     "      old_h[b] = (by_hit[b] && by_wv[b]) ? by_h[b] : $signed(q_h[b]);\n"
     "      old_v[b] = (by_hit[b] && by_wh[b]) ? by_v[b] : $signed(q_v[b]);"),

    ("P05 the bank rotation runs the wrong way",
     RTL,
     "      up_rot[b]  = 2'(b) - up_iv_i[1:0];",
     "      up_rot[b]  = 2'(b) + up_iv_i[1:0];"),

    ("P06 the rotated vertex forgets its lane offset (wrong row address)",
     RTL,
     "      up_vtx[b]  = up_iv_i + {9'd0, up_rot[b]};",
     "      up_vtx[b]  = up_iv_i;"),

    ("P07 the lane mask is indexed by BANK instead of lane",
     RTL,
     "      up_act[b]  = up_valid_i && up_mask_i[up_rot[b]];",
     "      up_act[b]  = up_valid_i && up_mask_i[2'(b)];"),

    ("P08 the INIT bottom clamp is inverted",
     RTL,
     "      init_top[b] = (in_dual_i[b] && (init_t[b] < init_botfx[b])) ? init_botfx[b] : init_t[b];",
     "      init_top[b] = (in_dual_i[b] && (init_t[b] > init_botfx[b])) ? init_botfx[b] : init_t[b];"),

    ("P09 the DRAIN final clamp is inverted",
     RTL,
     "      dr_live[b] = (d1_dual[b] && ($signed(q_h[b]) < dr_botfx[b])) ? dr_botfx[b] : $signed(q_h[b]);",
     "      dr_live[b] = (d1_dual[b] && ($signed(q_h[b]) > dr_botfx[b])) ? dr_botfx[b] : $signed(q_h[b]);"),

    ("P10 the dirty law is inverted",
     RTL,
     "          out_dirty_o[b] <= dr_live[b] != (32'($signed(d1_base[b])) <<< 8);",
     "          out_dirty_o[b] <= dr_live[b] == (32'($signed(d1_base[b])) <<< 8);"),

    ("P11 a legacy page reports fx(bottom) instead of live_top",
     RTL,
     "          out_bot[b]     <= d1_dual[b] ? dr_botfx[b] : dr_live[b];",
     "          out_bot[b]     <= dr_botfx[b];"),

    ("P12 the height sat pulse is gated by the VELOCITY write bit",
     RTL,
     "        sat_h_o[l] <= m_valid && m_wmask[W_H] && m_act[2'(l)+m_rotl] && satb_h[2'(l)+m_rotl];",
     "        sat_h_o[l] <= m_valid && m_wmask[W_V] && m_act[2'(l)+m_rotl] && satb_h[2'(l)+m_rotl];"),

    ("P13 INIT leaks compose_top into the velocity lane",
     RTL,
     "        ram_v[b][in_g_i] <= 32'd0;",
     "        ram_v[b][in_g_i] <= $unsigned(init_top[b]);"),

    ("P14 the update values are routed by BANK instead of rotation",
     RTL,
     "          m_h[b]    <= up_h[up_rot[b]];",
     "          m_h[b]    <= up_h[2'(b)];"),

    ("P15 a non-writer clobbers the material lane (adds-zero defect)",
     RTL,
     "        if (m_wmask[W_M]) ram_m[b][m_addr[b]] <= new_m[b];",
     "        if (m_wmask[W_M] || m_wmask[W_H]) ram_m[b][m_addr[b]] <= new_m[b];"),
]

# Machine-readable, so a survivor is either PROVEN equivalent here or fails
# the sweep. Nothing is declared until the first run says what survives.
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
