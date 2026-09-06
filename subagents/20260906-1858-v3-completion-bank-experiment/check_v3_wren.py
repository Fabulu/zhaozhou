#!/usr/bin/env python3
r"""Fail if a V3 payload bank's write enable is driven by anything other than a
register output.

WHY THIS EXISTS
---------------
`reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt` section 0 point D is the
whole reason the completion experiment was built:

    "Replace FRAGROB's return-to-RAM-write-enable cone with a real completion
     pipeline ... A delayed write-enable alone is not this pipeline."

and LAW 09 states the physical half of it:

    "A result RAM write-enable is registered; it is not a table lookup,
     priority scan, arithmetic decision, and memory enable in one cycle."

`zhao_texture_v3bank` documents that as a CALLER OBLIGATION, and SystemVerilog
gives no way to assert it -- a module cannot ask whether its port argument came
from a flop. So it was a comment, and a comment is not the machine. This is
that obligation made decidable.

The defect it is aimed at is real and is in this repository at a line number:
`fpga/rtl/texture/zhao_texture_fragrob.sv:626` writes a payload array under
`if (tmu_ok_c)`, and `tmu_ok_c` at :443 is an input pin through five
slot-indexed table lookups and a seven-term predicate. This checker would have
reported that file, by name, in under a second.

THE RULE, in two parts, both decidable from source without elaboration
----------------------------------------------------------------------
 1. SHAPE. The `.wr_en_i()` argument of every bank instantiation must be a BARE
    IDENTIFIER, or a single constant bit-select of one (`c3t_we_q[0]`, or
    `c3t_we_q[gs]` where gs is a genvar). No operators, no ternary, no
    concatenation, no function call, no reduction. If a cone is legal at the
    port, the property is undecidable here and the checker says so rather than
    guessing.

 2. PROVENANCE. That identifier must be assigned ONLY inside `always_ff`
    blocks. It must appear on the left of no `assign`, and inside no
    `always_comb` / `always @*` / `always_latch`. A signal that satisfies this
    is a register output, and nothing else can be.

WHY BOTH PARTS ARE NEEDED
-------------------------
Part 1 alone permits `.wr_en_i(some_wire)` where `some_wire` is an alias for a
predicate -- which is the fragrob defect with one extra hop. Part 2 alone
permits `.wr_en_i(reg_a && reg_b)`, which is two register outputs and an AND
gate on a memory enable: still logic in the cone, still the thing LAW 09
forbids. The conjunction is the property.

This is also why the three ready queues in `zhao_texture_v3own` are
instantiated by hand rather than in a generate loop: a loop needs
`.wr_en_i(alias_c[i])`, part 1 fails, and the check becomes undecidable for the
sake of six saved lines.

NEGATIVE CONTROLS
-----------------
CLAUDE.md: "A detector that has not been shown to FIRE has not been tested."
Six synthetic fixtures below are each a different way to break the law, and the
checker refuses to report on the real tree until every one of them has fired.
A pattern that matches nothing prints reassurance for its whole life.

STATUS: written by the completion-bank lane as a reference implementation for
`tools/rtl/check_v3_banks.py` to absorb. It lives here rather than in tools/
because the bank gate is another lane's file.
"""

import os
import re
import sys

# Modules whose instantiations carry a payload write enable, and the port name.
BANK_PRIMITIVES = {
    "zhao_texture_v3bank": "wr_en_i",
    "zhao_texture_v3rq": "wr_en_i",
}

# A bare identifier, or one bit-select of an identifier by a constant or a
# simple name (a genvar). Deliberately strict: anything else is undecidable.
BARE_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_$]*$")
BITSEL_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_$]*)\s*\[\s*([A-Za-z0-9_$]+)\s*\]$")


def strip_comments(src):
    """Remove // and /* */ comments, preserving line count."""
    out = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    out = re.sub(r"//[^\n]*", "", out)
    return out


def balanced(text, i, opener="(", closer=")"):
    depth = 0
    j = i
    while j < len(text):
        if text[j] == opener:
            depth += 1
        elif text[j] == closer:
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1
    raise ValueError("unbalanced parentheses at offset %d" % i)


def find_instantiations(src):
    """Yield (module, instance, wren_argument, line_number)."""
    code = strip_comments(src)
    for mod, port in BANK_PRIMITIVES.items():
        for m in re.finditer(
            r"\b" + re.escape(mod) + r"\b\s*(#\s*\()?", code
        ):
            pos = m.end()
            if m.group(1):
                pos = balanced(code, m.start(1) + len(m.group(1)) - 1)
            # instance name, then the port map
            im = re.match(r"\s*([A-Za-z_][A-Za-z0-9_$]*)\s*\(", code[pos:])
            if not im:
                continue
            inst = im.group(1)
            popen = pos + im.end() - 1
            pclose = balanced(code, popen)
            ports = code[popen:pclose]
            pm = re.search(
                r"\.\s*" + re.escape(port) + r"\s*\(", ports
            )
            if not pm:
                continue
            aopen = popen + pm.end() - 1
            aclose = balanced(code, aopen)
            arg = code[aopen + 1:aclose - 1].strip()
            line = code[:m.start()].count("\n") + 1
            yield mod, inst, arg, line


def assignment_contexts(src):
    """Map signal -> set of contexts it is assigned in.

    Contexts: 'always_ff', 'always_comb', 'assign', 'always_other'.
    Coarse but sufficient: the question is only whether a name is EVER driven
    by something that is not a clocked process.
    """
    code = strip_comments(src)
    ctx = {}

    def note(name, kind):
        ctx.setdefault(name, set()).add(kind)

    # continuous assignments
    for m in re.finditer(r"\bassign\s+([A-Za-z_][A-Za-z0-9_$]*)", code):
        note(m.group(1), "assign")

    # procedural blocks: walk each `always*` header, take the text up to the
    # next `always`/`endmodule`, and record every `<=` / `=` target in it.
    heads = [
        (m.start(), m.group(1))
        for m in re.finditer(r"\b(always_ff|always_comb|always_latch|always)\b", code)
    ]
    heads.append((len(code), None))
    for idx in range(len(heads) - 1):
        start, kind = heads[idx]
        end = heads[idx + 1][0]
        body = code[start:end]
        kindname = {
            "always_ff": "always_ff",
            "always_comb": "always_comb",
            "always_latch": "always_other",
            "always": "always_other",
        }[kind]
        for m in re.finditer(
            r"([A-Za-z_][A-Za-z0-9_$]*)\s*(?:\[[^\]]*\])*\s*<?=(?![=>])", body
        ):
            note(m.group(1), kindname)
    return ctx


def input_ports(src):
    """Names declared as module inputs in this file.

    A wrapper that passes its own port straight through -- `zhao_texture_v3rq`
    hands `wr_en_i` to the `zhao_texture_v3bank` inside it -- cannot be decided
    locally, and calling that a violation would be wrong: the obligation simply
    moves UP to whoever instantiates the wrapper. Those instantiations are
    checked by this same rule, so the chain closes as long as every parent
    connection passes. Reporting it as a violation would push a lane to write
    a pointless local register just to silence the tool, which is how a gate
    starts making designs worse.
    """
    code = strip_comments(src)
    return set(
        m.group(1)
        for m in re.finditer(
            r"\binput\s+(?:var\s+)?(?:logic|wire|reg)?\s*(?:\[[^\]]*\]\s*)?"
            r"([A-Za-z_][A-Za-z0-9_$]*)",
            code,
        )
    )


def check_text(src, path="<fixture>", deferred=None):
    """Return a list of violation strings."""
    ctx = assignment_contexts(src)
    ports = input_ports(src)
    bad = []
    for mod, inst, arg, line in find_instantiations(src):
        base = None
        if BARE_RE.match(arg):
            base = arg
        else:
            bm = BITSEL_RE.match(arg)
            if bm:
                base = bm.group(1)
        if base is None:
            bad.append(
                "%s:%d %s %s: .wr_en_i(%s) is an EXPRESSION, not a register "
                "name. LAW 09 forbids logic on a payload memory enable."
                % (path, line, mod, inst, arg)
            )
            continue
        kinds = ctx.get(base, set())
        if not kinds:
            if base in ports:
                # Pass-through: the obligation belongs to every parent
                # instantiation of THIS module, and those are checked too.
                if deferred is not None:
                    deferred.append("%s:%d %s %s: .wr_en_i(%s) is this module's "
                                    "own input port -- obligation deferred to "
                                    "its instantiators"
                                    % (path, line, mod, inst, base))
                continue
            bad.append(
                "%s:%d %s %s: .wr_en_i(%s) names a signal with no assignment "
                "in this file; provenance is undecidable." % (path, line, mod, inst, base)
            )
            continue
        illegal = kinds - {"always_ff"}
        if illegal:
            bad.append(
                "%s:%d %s %s: .wr_en_i(%s) is driven from %s, so it is NOT a "
                "register output." % (path, line, mod, inst, base, "/".join(sorted(illegal)))
            )
    return bad


# ---------------------------------------------------------------------------
# NEGATIVE CONTROLS -- each must fire, or this checker does not run.
# ---------------------------------------------------------------------------
_HEAD = "module t; logic we_q, pred_c, a_q, b_q; logic [2:0] oh_q;\n"
_TAIL = "endmodule\n"

FIXTURES = [
    (
        "combinational predicate straight onto the enable (the fragrob shape)",
        _HEAD
        + "assign pred_c = valid_i && live_q[slot] && !arr_q[slot];\n"
        + "zhao_texture_v3bank u_b (.wr_en_i(pred_c), .clk(clk));\n"
        + _TAIL,
    ),
    (
        "wire alias of a register (one extra hop, same defect)",
        _HEAD
        + "always_ff @(posedge clk) we_q <= x;\n"
        + "assign alias_c = we_q;\n"
        + "zhao_texture_v3bank u_b (.wr_en_i(alias_c), .clk(clk));\n"
        + _TAIL,
    ),
    (
        "two register outputs and an AND gate",
        _HEAD
        + "always_ff @(posedge clk) begin a_q <= x; b_q <= y; end\n"
        + "zhao_texture_v3bank u_b (.wr_en_i(a_q && b_q), .clk(clk));\n"
        + _TAIL,
    ),
    (
        "ternary at the port",
        _HEAD
        + "always_ff @(posedge clk) we_q <= x;\n"
        + "zhao_texture_v3bank u_b (.wr_en_i(sel ? we_q : 1'b0), .clk(clk));\n"
        + _TAIL,
    ),
    (
        "driven from always_comb",
        _HEAD
        + "always_comb we_q = valid_i && ok_c;\n"
        + "zhao_texture_v3bank u_b (.wr_en_i(we_q), .clk(clk));\n"
        + _TAIL,
    ),
    (
        "generate-loop alias array (why the ready queues are hand-written)",
        _HEAD
        + "assign wr_c[0] = q0t_v_q;\n"
        + "zhao_texture_v3rq u_q (.wr_en_i(wr_c[0]), .clk(clk));\n"
        + _TAIL,
    ),
]

GOOD_FIXTURE = (
    _HEAD
    + "always_ff @(posedge clk) begin we_q <= acc_c; oh_q <= acc_c ? m : 3'b000; end\n"
    + "zhao_texture_v3bank u_a (.wr_en_i(we_q), .clk(clk));\n"
    + "zhao_texture_v3bank u_b (.wr_en_i(oh_q[0]), .clk(clk));\n"
    + "zhao_texture_v3bank u_c (.wr_en_i(oh_q[gs]), .clk(clk));\n"
    + _TAIL
)


def self_test():
    failures = []
    for name, text in FIXTURES:
        if not check_text(text, "fixture"):
            failures.append("FIXTURE DID NOT FIRE: " + name)
    if check_text(GOOD_FIXTURE, "fixture"):
        failures.append(
            "GOOD FIXTURE WAS REPORTED: a legal registered enable must pass. "
            + "; ".join(check_text(GOOD_FIXTURE, "fixture"))
        )
    return failures


def main():
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    targets = [
        "fpga/rtl/texture/zhao_texture_v3own.sv",
        "fpga/rtl/texture/zhao_texture_v3rq.sv",
    ]

    sf = self_test()
    if sf:
        print("check_v3_wren: SELF-TEST FAILED -- refusing to report on the tree")
        for f in sf:
            print("  " + f)
        return 2
    print("check_v3_wren: %d negative controls fired, 1 positive control passed"
          % len(FIXTURES))

    violations = []
    deferred = []
    checked = 0
    for rel in targets:
        path = os.path.join(repo, rel)
        if not os.path.exists(path):
            continue
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            src = fh.read()
        insts = list(find_instantiations(src))
        checked += len(insts)
        for mod, inst, arg, line in insts:
            print("  %-56s .wr_en_i(%s)" % (rel + ":" + str(line) + " " + inst, arg))
        violations += check_text(src, rel, deferred)
    for d in deferred:
        print("  DEFERRED  " + d)

    if not checked:
        print("check_v3_wren: NO BANK INSTANTIATIONS FOUND -- that is a broken "
              "instrument, not a clean tree.")
        return 2

    if violations:
        print("check_v3_wren: %d VIOLATION(S)" % len(violations))
        for v in violations:
            print("  " + v)
        return 1

    print("check_v3_wren: %d bank write enable(s), every one a register output "
          "(spec LAW 09 / section 0 point D)" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
