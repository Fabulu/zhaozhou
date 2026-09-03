#!/usr/bin/env python3
"""Emit fpga/rtl/prod/zhao_prod_top.sv -- one instance of every production block.

WHAT THIS IS AND, MORE IMPORTANTLY, WHAT IT IS NOT
--------------------------------------------------
It is a RESOURCE top, not the console. It answers exactly one question, the
owner's: what does the planned machine cost when every block is counted ONCE?
It does not wire blocks to each other, so it is not a functional design and no
timing number from it means anything about the console.

The two ways the number is wrong, both stated in the report:

  * it is an UPPER bound on the sum of parts, because composition shares
    queues, control and arithmetic that this top duplicates;
  * it is a LOWER bound on the machine, because integration glue is not here
    and neither are the blocks nobody has built yet.

HOW INPUTS ARE DRIVEN, AND WHY IT MATTERS
-----------------------------------------
Every instance gets its OWN LFSR with its own seed. Tying inputs to constants
would let the fitter constant-fold whole blocks away and report a beautiful,
meaningless total; feeding every block the SAME source would let it merge
common logic across blocks. Different seeds per instance defeat both.

Every output is XOR-reduced into a per-instance register and folded to one
pin, so nothing is dangling and nothing is optimised away for having no load.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_prod_manifest import read_manifest  # noqa: E402
from module_graph import build, strip_comments  # noqa: E402

OUT = "fpga/rtl/prod/zhao_prod_top.sv"
SRCW = 1024


def port_header(text, mod):
    """The port list of `mod`, with comments removed first.

    Anchored at the module keyword: cutting at the first `);` in the FILE
    returns nonsense for any module whose header comment quotes SystemVerilog,
    which is how a port list came back empty once before.
    """
    body = strip_comments(text)
    m = re.search(r"\bmodule\s+" + mod + r"\b", body)
    if not m:
        return None
    s = body[m.end():]
    # A module may import packages BEFORE its parameter block --
    # `module zhao_shell_top import zhao_pkg::*; #( ... ) ( ... )`. Missing this
    # makes the PARAMETER list parse as the port list, which comes back empty
    # and silently drops the console's whole integrated shell from the count.
    s = re.sub(r"^\s*import\b[^;]*;", "", s)
    if re.match(r"\s*#\s*\(", s):
        i = s.index("(")
        depth = 0
        for j in range(i, len(s)):
            if s[j] == "(":
                depth += 1
            elif s[j] == ")":
                depth -= 1
                if depth == 0:
                    s = s[j + 1:]
                    break
    if "(" not in s:
        return None
    i = s.index("(")
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return s[i + 1:j]
    return None


def parse_ports(header):
    """(direction, name, packed_ranges) for every port."""
    ports = []
    direction = None
    for decl in header.split(","):
        d = decl.strip()
        if not d:
            continue
        m = re.match(r"^(input|output|inout)\b", d)
        if m:
            direction = m.group(1)
            d = d[m.end():].strip()
        if direction is None:
            continue
        d = re.sub(r"^(var|wire|reg)\b", "", d).strip()
        d = re.sub(r"^(logic|bit|byte|integer)\b", "", d).strip()
        # Signedness has to survive: an unpacked array port declared signed and
        # connected to an unsigned wire is a TYPE mismatch, not a warning, and
        # it stops the whole top from elaborating.
        sgn = ""
        m3 = re.match(r"^(signed|unsigned)\b", d)
        if m3:
            sgn = " signed" if m3.group(1) == "signed" else ""
            d = d[m3.end():].strip()
        # Ranges BEFORE the name are packed (part of the width); ranges AFTER
        # it are an UNPACKED array of that width. Treating an unpacked
        # dimension as packed produces a port connection of the wrong shape
        # and an elaboration error that looks like a width bug.
        m2 = re.search(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", " ", d))
        if not m2:
            continue
        name = m2.group(0)
        at = d.index(name)
        packed = re.findall(r"\[[^\]]*\]", d[:at])
        unpacked = re.findall(r"\[[^\]]*\]", d[at + len(name):])
        ports.append((direction, name, packed, unpacked, sgn))
    return ports


def unpacked_count(dims):
    """Element count of numeric unpacked dimensions, or None if not numeric."""
    n = 1
    for dim in dims:
        inner = dim[1:-1]
        if ":" not in inner:
            return None
        hi, lo = inner.split(":", 1)
        try:
            n *= abs(int(hi.strip()) - int(lo.strip())) + 1
        except ValueError:
            return None
    return n


def param_block(text, mod):
    """The `#( ... )` text of `mod`, or None."""
    body = strip_comments(text)
    m = re.search(r"\bmodule\s+" + mod + r"\b", body)
    if not m:
        return None
    s = re.sub(r"^\s*import\b[^;]*;", "", body[m.end():])
    if not re.match(r"\s*#\s*\(", s):
        return None
    i = s.index("(")
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return s[i + 1:j]
    return None


def sv_number(tok):
    """`12'd7`, `8'hFF`, `4'b1010` or a plain integer, as a Python int."""
    m = re.match(r"^\s*(?:\d+)?'([sS]?)([dDhHbBoO])([0-9a-fA-FxXzZ_]+)\s*$", tok)
    if m:
        base = {"d": 10, "h": 16, "b": 2, "o": 8}[m.group(2).lower()]
        return int(m.group(3).replace("_", ""), base)
    return int(tok.strip())


def resolve_params(text, mod):
    """Every parameter of `mod` as an integer, using its DEFAULT value.

    Port widths are written in the module's own parameters (`[ENTRIES-1:0]`),
    which do not exist in the resource top's scope. Substituting the defaults
    is what makes the generated wires declarable at all -- and the defaults are
    the right values, because a resource top instantiates every block as the
    block ships.
    """
    block = param_block(text, mod)
    out = {}
    if not block:
        return out
    raw = {}
    for decl in re.split(r",(?![^\[]*\])", block):
        m = re.search(r"([A-Za-z_]\w*)\s*=\s*(.+)$", decl.strip(), re.S)
        if m:
            raw[m.group(1)] = m.group(2).strip()
    for _pass in range(6):
        for name, expr in raw.items():
            if name in out:
                continue
            try:
                out[name] = eval_sv(expr, out)
            except Exception:
                pass
    return out


def eval_sv(expr, params):
    """A SystemVerilog constant expression as an int, or raise."""
    e = expr.strip()
    e = re.sub(r"\$clog2", "clog2", e)
    e = re.sub(r"(\d+)?'[sS]?[dDhHbBoO][0-9a-fA-F_]+",
               lambda m: str(sv_number(m.group(0))), e)
    for name in sorted(params, key=len, reverse=True):
        e = re.sub(r"(?<![\w$])" + name + r"(?![\w])", str(params[name]), e)
    if re.search(r"[A-Za-z_]", e.replace("clog2", "")):
        raise ValueError("unresolved: " + e)
    val = eval(e, {"clog2": lambda x: max(1, (int(x) - 1).bit_length()),
                   "__builtins__": {}}, {})
    return int(val)


def width_expr(widths, params=None):
    """SV expression for the bit count of a packed range list."""
    if not widths:
        return "1"
    params = params or {}
    total = 1
    for w in widths:
        inner = w[1:-1]
        if ":" in inner:
            hi, lo = inner.split(":", 1)
            total *= abs(eval_sv(hi, params) - eval_sv(lo, params)) + 1
        else:
            total *= eval_sv(inner, params)
    return str(total)


def is_clock(name):
    return re.search(r"(^|_)clk", name) is not None


def is_reset(name):
    return re.search(r"(^|_)(rst|reset)", name) is not None


def main():
    tops, _excluded = read_manifest()
    decl, _inst = build()
    files = {}
    lines = []
    skipped = []
    used = []

    for idx, mod in enumerate(sorted(tops)):
        path = decl[mod]
        if path not in files:
            files[path] = io.open(path, encoding="utf-8", errors="replace").read()
        header = port_header(files[path], mod)
        if header is None:
            skipped.append((mod, "no port list found"))
            continue
        params = resolve_params(files[path], mod)
        ports = parse_ports(header)
        if not ports:
            skipped.append((mod, "port list parsed empty"))
            continue

        seed = (idx * 0x9E3779B1 + 0x12345) & 0xFFFFFFFFFFFFFFFF
        pre = "u%02d" % idx
        lines.append("  // ---- %s ----" % mod)
        lines.append("  logic [63:0] %s_lfsr_q;" % pre)
        lines.append("  logic [%d:0] %s_src;" % (SRCW - 1, pre))
        lines.append("  assign %s_src = {%d{%s_lfsr_q}};" % (pre, SRCW // 64, pre))
        lines.append("  always_ff @(posedge clk or negedge rst_n)")
        lines.append("    if (!rst_n) %s_lfsr_q <= 64'h%016X;" % (pre, seed or 1))
        lines.append(
            "    else %s_lfsr_q <= {%s_lfsr_q[62:0], "
            "(^(%s_lfsr_q & 64'hD800000000000000)) ^ seed_i};" % (pre, pre, pre)
        )

        conns = []
        folds = []
        off = 0
        for (d, name, widths, unpacked, sgn) in ports:
            try:
                w = width_expr(widths, params)
            except Exception as exc:
                skipped.append((mod, "port %s width unresolved (%s)" % (name, exc)))
                ports = None
                break
            if unpacked:
                # Unpacked dimensions are often PARAMETERISED (`[0:NSVC-1]`),
                # so the element count is not knowable by text. `foreach` walks
                # whatever the elaborated bounds turn out to be, which keeps
                # six real blocks -- texjoin_v2 and geom_skin among them -- in
                # the count instead of silently dropping them for being hard
                # to parse.
                # Unpacked dimensions are parameterised too (`[CLAIMANTS][4]`),
                # and the top has no such parameter in scope. Resolve them to
                # counts the same way the packed widths are resolved.
                try:
                    dims = "".join(
                        "[%d]" % (
                            abs(eval_sv(d[1:-1].split(":")[0], params)
                                - eval_sv(d[1:-1].split(":")[1], params)) + 1
                            if ":" in d else eval_sv(d[1:-1], params))
                        for d in unpacked)
                except Exception as exc:
                    skipped.append((mod, "unpacked %s unresolved (%s)" % (name, exc)))
                    ports = None
                    break
                idx = ["k%d" % k for k in range(len(unpacked))]
                sel = "".join("[%s]" % k for k in idx)
                wire = "%s_%s" % (pre, name)
                lines.append("  logic%s [%s-1:0] %s %s;" % (sgn, w, wire, dims))
                conns.append(".%s(%s)" % (name, wire))
                if d == "input":
                    lines.append("  always_comb")
                    lines.append("    foreach (%s[%s])" % (wire, ",".join(idx)))
                    # XOR by the index so the elements differ: identical drives
                    # would let the fitter collapse the block's own comparisons.
                    lines.append(
                        "      %s%s = %s_src[%d +: %s] ^ (%s)'(%s);"
                        % (wire, sel, pre, off % (SRCW // 2), w, w,
                           "+".join(idx))
                    )
                    off += 7
                else:
                    fold = "%s_fold" % wire
                    lines.append("  logic %s;" % fold)
                    lines.append("  always_comb begin")
                    lines.append("    %s = 1'b0;" % fold)
                    lines.append("    foreach (%s[%s])" % (wire, ",".join(idx)))
                    lines.append("      %s = %s ^ (^%s%s);" % (fold, fold, wire, sel))
                    lines.append("  end")
                    folds.append("(%s)" % fold)
                continue
            if d == "input":
                if is_clock(name):
                    conns.append(".%s(clk)" % name)
                elif is_reset(name):
                    conns.append(
                        ".%s(%s)" % (name, "rst_n" if name.endswith("_n") else "!rst_n")
                    )
                else:
                    conns.append(
                        ".%s(%s_src[%d +: %s])" % (name, pre, off % (SRCW // 2), w)
                    )
                    off += 7
            else:
                wire = "%s_%s" % (pre, name)
                lines.append("  logic%s [%s-1:0] %s;" % (sgn, w, wire))
                conns.append(".%s(%s)" % (name, wire))
                folds.append("(^%s)" % wire)

        if ports is None:
            continue
        lines.append("  %s %s_i (" % (mod, pre))
        lines.append("      " + ",\n      ".join(conns))
        lines.append("  );")
        used.append(pre + "_fold_q")
        lines.append("  logic %s_fold_q;" % pre)
        if folds:
            lines.append("  always_ff @(posedge clk or negedge rst_n)")
            lines.append("    if (!rst_n) %s_fold_q <= 1'b0;" % pre)
            lines.append(
                "    else %s_fold_q <= %s_fold_q ^ %s;" % (pre, pre, " ^ ".join(folds))
            )
        else:
            lines.append("  assign %s_fold_q = 1'b0;" % pre)
        lines.append("")

    head = [
        "// zhao_prod_top.sv -- GENERATED by tools/quartus/gen_prod_top.py.",
        "// Do not edit: edit design/prod_manifest.yml and regenerate.",
        "//",
        "// ONE instance of every intended production block, so the fitter can",
        "// answer the owner's question from reports/WeNeedSomeMeasurements.md:",
        "// what does the planned console cost when counted ONCE?",
        "//",
        "// This is a RESOURCE top, not the console. Blocks are not wired to each",
        "// other, so no timing number here means anything. Each instance is fed by",
        "// its own seeded LFSR -- constants would let the fitter fold blocks away,",
        "// and a shared source would let it merge logic across them.",
        "`default_nettype none",
        "",
        "module zhao_prod_top (",
        "    input  var logic clk,",
        "    input  var logic rst_n,",
        "    input  var logic seed_i,",
        "    output var logic fold_o",
        ");",
        "",
    ]
    tail = [
        "  // One pin, so nothing above is dangling and nothing is removed for",
        "  // having no load.",
        "  always_ff @(posedge clk or negedge rst_n)",
        "    if (!rst_n) fold_o <= 1'b0;",
        "    else fold_o <= " + (" ^ ".join(used) if used else "1'b0") + ";",
        "",
        "endmodule : zhao_prod_top",
        "",
        "`default_nettype wire",
    ]

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    io.open(OUT, "w", encoding="utf-8", newline="\n").write(
        "\n".join(head + lines + tail) + "\n"
    )
    print("wrote %s: %d instances" % (OUT, len(used)))
    for m, why in skipped:
        print("  SKIPPED %-34s %s" % (m, why))
    return 1 if skipped else 0


if __name__ == "__main__":
    sys.exit(main())
