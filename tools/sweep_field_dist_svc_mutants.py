#!/usr/bin/env python3
"""Mutation table for the FIELD v3 distance-service probe sweep
(tools/sweep_field_dist_svc.sh).

Target: fpga/rtl/synth/zhao_probe_dist_svc.sv ONLY. zhao_field_isqrt is
deliberately NOT mutated here — it is a frozen verified unit with its own
suite and many consumers, and mutating it would require running every one of
them (guard 7); the probe file has exactly one consumer.

Classes from reports/Fieldv3.md where they exist in this seam: reply
delivered to the wrong requester (order/tag), lost or duplicated requests,
early result reuse under backpressure, root-bank selection, saturation-lane
collapse — plus "the second bank does not exist", which proves the II gate
in the directed test actually bites.
"""

RTL = "fpga/rtl/synth/zhao_probe_dist_svc.sv"

MUTANTS = [
    # D01 first ran named "flag fires AT INT32_MAX" and SURVIVED — correctly:
    # the mutation touches only sat_len's VALUE path, whose two branches
    # coincide at the single differing input (see EQUIVALENT). The flag has
    # its own compare, which D13 now mutates and the directed suite catches.
    ("D01 the value-path saturation compare is off by one (proven equivalent)",
     RTL,
     "    return (len > 64'h7FFF_FFFF) ? 32'h7FFF_FFFF : len[31:0];",
     "    return (len >= 64'h7FFF_FFFF) ? 32'h7FFF_FFFF : len[31:0];"),

    ("D02 the per-lane sat flags collapse onto lane 0",
     RTL,
     "          rsp_sat_o[l] <= (bank_r[head_bank][l] > 64'h7FFF_FFFF);",
     "          rsp_sat_o[l] <= (bank_r[head_bank][0] > 64'h7FFF_FFFF);"),

    ("D03 the order FIFO is read at the WRITE pointer",
     RTL,
     "  assign head_bank  = ord_bank[ord_rd[1:0]];",
     "  assign head_bank  = ord_bank[ord_wr[1:0]];"),

    ("D04 the reply carries the OTHER bank's tag",
     RTL,
     "        rsp_tag_o   <= bank_tag[head_bank];",
     "        rsp_tag_o   <= bank_tag[~head_bank];"),

    ("D05 the request is steered to the busy bank",
     RTL,
     "  assign accept_b   = bank_free[0] ? 1'b0 : 1'b1;",
     "  assign accept_b   = bank_free[0] ? 1'b1 : 1'b0;"),

    # D06 reshaped for lint: the naive "delete bank 1" orphans bank_free[1]
    # (UNUSEDSIGNAL under -Wall). Requiring BOTH banks free is the same
    # architectural defect - one request in flight, II ~ 35 - without the
    # orphan. The II gate must bite on it.
    ("D06 the service holds one request in flight (II gate must bite)",
     RTL,
     "  assign req_ready_o = (bank_free[0] | bank_free[1]) && (ord_count != 3'd4);",
     "  assign req_ready_o = (bank_free[0] & bank_free[1]) && (ord_count != 3'd4);"),

    ("D07 the tag is stored into the OTHER bank on accept",
     RTL,
     "        bank_tag[accept_b] <= req_tag_i;",
     "        bank_tag[~accept_b] <= req_tag_i;"),

    ("D08 the drain no longer waits for the roots to finish",
     RTL,
     "  assign drain_fire = head_valid && bank_done[head_bank] && (!rsp_valid_o || rsp_ready_i);",
     "  assign drain_fire = head_valid && (bank_done[head_bank] || head_valid) && (!rsp_valid_o || rsp_ready_i);"),

    ("D09 the order count never comes back down (service wedges shut)",
     RTL,
     "        2'b01:   ord_count <= ord_count - 3'd1;",
     "        2'b01:   ord_count <= ord_count;"),

    ("D10 a held reply is clobbered by the next drain (early reuse)",
     RTL,
     "  assign drain_fire = head_valid && bank_done[head_bank] && (!rsp_valid_o || rsp_ready_i);",
     "  assign drain_fire = head_valid && bank_done[head_bank];"),

    ("D11 the read pointer advances by two per drain",
     RTL,
     "        ord_rd <= ord_rd + 3'd1;",
     "        ord_rd <= ord_rd + 3'd2;"),

    # D12 reshaped for lint: dropping the compare entirely orphans the top
    # bits (UNUSEDSIGNAL). Raising the threshold to u32-max means every
    # length in (2^31, 2^32) returns the WRAPPED NEGATIVE low word - the
    # exact failure the law forbids - while still reading all 64 bits.
    ("D12 saturation returns the wrapped low word below u32-max",
     RTL,
     "    return (len > 64'h7FFF_FFFF) ? 32'h7FFF_FFFF : len[31:0];",
     "    return (len > 64'hFFFF_FFFF) ? 32'h7FFF_FFFF : len[31:0];"),

    ("D13 the per-lane sat FLAG compare is off by one (fires AT INT32_MAX)",
     RTL,
     "          rsp_sat_o[l] <= (bank_r[head_bank][l] > 64'h7FFF_FFFF);",
     "          rsp_sat_o[l] <= (bank_r[head_bank][l] >= 64'h7FFF_FFFF);"),
]

# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {
    "D01": (
        "PROVEN EQUIVALENT. sat_len's two branches coincide at the only input "
        "the compare change affects: at len == 0x7FFF_FFFF the true branch "
        "returns the constant 0x7FFF_FFFF and the false branch returns "
        "len[31:0] == 0x7FFF_FFFF, so > and >= produce identical values for "
        "all 2^64 inputs. The per-lane sat FLAG is computed by a separate "
        "compare (mutated by D13, which is CAUGHT by the directed "
        "INT32_MAX-root lane-sat case). "
        "RE-SCORE THIS THE MOMENT sat_len's result feeds the flag, or the "
        "flag compare is folded into sat_len."
    ),
}


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
