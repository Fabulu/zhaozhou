"""Refuse SystemVerilog forms that Verilator accepts and Quartus 17.0.2 rejects.

---------------------------------------------------------------------------
WHY THIS EXISTS
---------------------------------------------------------------------------
2026-09-09. `fpga/rtl/field/zhao_field_alu_vec.sv` sat in the tree for ten days
containing

    for (genvar l = 0; l < LANES; l++) begin : gen_lane

which Quartus 17.0.2 rejects outright:

    Error (10170): Verilog HDL syntax error ... near text: "for";
                   expecting "endmodule"
    Error (10112): Ignored design unit "zhao_field_alu_vec"

Verilator linted it clean the whole time and its differential test passed. This
is CLAUDE.md's rule stated as an incident: **a block that has never been through
quartus_map has not been shown to be synthesizable, however clean its lint.**
That file had ZERO rows in the fit ledger and ZERO in the map ledger.

TWO THINGS MADE IT WORSE THAN A ONE-BLOCK DEFECT.

**It broke every map in the repository.** `run_block_map.ps1` compiles every
`.sv` under `fpga/rtl`, because a per-module cone list "buys nothing and costs a
maintenance surface that has already drifted once" (its own header). So one
unparseable file fails Analysis & Synthesis for whatever unrelated module is
being mapped. It was found when a probe of a texture block died on it.

**Manifest exclusion does not exclude a file from that source list.**
`zhao_field_alu_vec` is `frozen  FIELD v1` in prod_manifest.yml -- kept as the
reference the differential tests run against, instantiated by no production
path, and correctly carrying no fit target. Every one of those facts says "this
cannot affect a measurement", and none of them is true for the map lane. Frozen
means unshipped, not uncompiled.

**And the lesson had already been learned seven times.** crc32c_fold,
field_v3_len, field_v3_mulbank, field_v3_normalize, field_v3_ring_svc,
raster_attrdiv_svc and raster_toon_div all carry a comment warning about exactly
this parser limitation. Every one of them was fixed. `zhao_field_alu_vec.sv` was
written afterwards without it -- the same half-fixed shape as the core.autocrlf
guard found on the same day: a lesson applied where it was learned and nowhere
else. A comment in seven files is not a check.

---------------------------------------------------------------------------
WHAT IT CHECKS
---------------------------------------------------------------------------
Three forms, all documented in CLAUDE.md's Build note as having passed
`verilator --lint-only` with 0 diagnostics and then failed `quartus_map`:

  1. `for (genvar N = ...)`   -- inline genvar in a loop-generate header.
     Quartus 17.0 needs `genvar N;` separately, inside generate/endgenerate.
  2. a bare module-scope elaboration check, e.g.
     `if (A + B != C) $fatal(...);` at module level.
     Quartus 17.0 needs it inside `initial begin ... end`.
  3. an implicit `if` generate at module scope -- `if (EN) begin : g ... end`.
     Quartus 17.0 needs explicit `generate` / `endgenerate`.

Only form 1 is detected precisely enough to gate on, and only form 1 is
reported as an ERROR. Forms 2 and 3 need real scope tracking to tell a
module-level `if` from one inside an always block, and a checker that cries wolf
gets suppressed -- so they are not guessed at here. Form 1 is the one that cost
the ten days.

Run:  python tools/quartus/check_quartus17_syntax.py
Exit: 0 clean, 1 rejected forms found, 2 self-test failure.
"""

from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RTL = os.path.join(REPO, "fpga", "rtl")

# `for (genvar x = ...` with any spacing. Quartus 17.0 rejects the inline
# declaration regardless of what surrounds it.
_INLINE_GENVAR = re.compile(r"\bfor\s*\(\s*genvar\b")

# FORM 4, added 2026-09-19: a UNARY minus applied directly to a size cast,
# `-WIDTH'(expr)`. Quartus 17.0 parses the minus, reaches the apostrophe and
# stops:
#
#   Error (10170): Verilog HDL syntax error at zhao_part_collide.sv(463)
#   near text: "'"; expecting ";"
#
# Six of these killed the first zhao_console_core synthesis after 6.3 seconds.
# Verilator accepts the form, so lint was 0/0 and 180 directed checks were green
# -- and THIS CHECKER PASSED THE FILE, which is why the form is added here
# rather than only fixed in the RTL.
#
# The repair is parenthesisation, `-(WIDTH'(expr))`. The cast already binds
# tighter than the unary minus, so it is semantically identical.
#
# Only UNARY minus is flagged. `a - B'(x)` is a binary subtraction with a left
# operand and Quartus accepts it, so the preceding non-space character decides:
# an operator or an opening bracket means unary, anything else means binary.
# Getting that wrong in the flagging direction is how a checker gets suppressed.
# NOTE the absence of a start-of-line alternative, and it is deliberate. The
# first draft had `^` in the unary context, and it immediately produced FOUR
# FALSE POSITIVES on continuation lines of ordinary sums:
#
#     pl_dist_c = DIST_W'(u_px) * DIST_W'(pl_nx_i)
#               + DIST_W'(u_py) * DIST_W'(pl_ny_i)
#               - DIST_W'(pl_c_i);          <- BINARY minus, Quartus is fine
#
# `zhao_texture_aux_pipe_v2.sv` has synthesised for weeks with three of those.
# Flagging them would have been the cry-wolf failure this module's own docstring
# warns about, in the tool written to prevent it.
#
# Nothing real is lost: `\s*` spans newlines, so a genuinely unary minus written
# after an `=` on the previous line is still caught. A `)` before the minus means
# binary, and `)` is not in the lookbehind set.
_UNARY_MINUS_CAST = re.compile(r"(?<=[=(\[{,?:;])\s*-\s*(\w+)\s*'\s*\(")


def strip_comments(text: str) -> str:
    """Blank out // and /* */ so a WARNING ABOUT the form is not read as the form.

    Seven files in this tree describe `for (genvar N = ...)` in a comment
    explaining why they do not use it. Flagging those would report eight
    findings for one defect and train the reader to ignore the tool -- the
    failure mode that made the first version of check_git_autocrlf_guard.py
    useless until it was narrowed.
    """
    out = []
    i, n = 0, len(text)
    in_block = False
    while i < n:
        if in_block:
            if text.startswith("*/", i):
                in_block = False
                out.append("  ")
                i += 2
            else:
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            continue
        if text.startswith("/*", i):
            in_block = True
            out.append("  ")
            i += 2
            continue
        if text.startswith("//", i):
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def scan_text(text: str) -> list[tuple[int, str]]:
    """Return [(line_number, form)] for rejected constructs."""
    clean = strip_comments(text)
    hits = []
    for m in _INLINE_GENVAR.finditer(clean):
        line = clean.count("\n", 0, m.start()) + 1
        hits.append((line, "for (genvar ...) -- inline genvar in a loop generate"))
    for m in _UNARY_MINUS_CAST.finditer(clean):
        line = clean.count("\n", 0, m.start()) + 1
        hits.append((line, "-%s'(...) -- unary minus on a size cast; "
                           "write -(%s'(...))" % (m.group(1), m.group(1))))
    return hits


def scan_repo() -> tuple[list[tuple[str, int, str]], int]:
    findings, scanned = [], 0
    for dirpath, dirnames, filenames in os.walk(RTL):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        for fn in sorted(filenames):
            if not fn.endswith((".sv", ".svh", ".v", ".vh")):
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, REPO).replace(os.sep, "/")
            try:
                text = io.open(full, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            scanned += 1
            for line, form in scan_text(text):
                findings.append((rel, line, form))
    return findings, scanned


# ---------------------------------------------------------------------------
# SELF-TEST AT IMPORT. A detector that has not been shown to FIRE has not been
# tested. The positive control is the exact line that cost ten days; the
# negative controls are the repaired form and the seven comments describing it.
# ---------------------------------------------------------------------------
_MUST_FLAG = [
    "  for (genvar l = 0; l < LANES; l++) begin : gen_lane",
    "for(genvar i=0;i<N;i++) begin",
    "    for ( genvar  N = 0 ; N <= 8 ; N++ ) begin : g_fold",
    # form 4 -- the exact six lines that killed the first console synthesis
    "SRC_NEGE: tan_c = -COEF_W'(d_restitution_i);",
    "      pen_c             = -PUSH_W'(dist_c);",
    "  localparam logic signed [ACC_W-1:0]  V_MIN = -ACC_W'(1 << (VEL_W-1));",
    "  assign y = (a) ? -W'(b) : c;",
    # a genuinely unary minus wrapped onto its own line after an `=` -- the
    # lookbehind spans the newline, so dropping `^` must not lose this
    "  assign v_min =\n      -ACC_W'(1 << (VEL_W-1));",
]
_MUST_NOT_FLAG = [
    # the repaired form, which is what crc32c_fold and now alu_vec use
    "  genvar gl;\n  generate\n  for (gl = 0; gl < LANES; gl++) begin : gen_lane",
    # an ordinary procedural loop
    "    for (int i = 0; i < 32; i++) begin",
    "      for (int unsigned ax = 0; ax < 2; ax++) begin",
    # the seven warning comments -- a description of the hazard is not the hazard
    "  // Quartus 17.0.2's Verilog parser rejects `for (genvar N = ...)` with",
    "  // `for (genvar N = ...)` and a loop generate at module level, while",
    "/* for (genvar q = 0; q < 4; q++) is what NOT to write */",
    # form 4 negatives -- the repaired shape, and BINARY minus, which Quartus
    # accepts. Flagging the binary case would fire on ordinary arithmetic all
    # over the tree and get the whole checker ignored.
    "SRC_NEGE: tan_c = -(COEF_W'(d_restitution_i));",
    "  assign d = span - PUSH_W'(dist_c);",
    "  assign d = a[3] - W'(b);",
    "  assign d = f(x) - W'(b);",
    # the four real continuation lines that the first draft of this rule
    # wrongly flagged. Kept verbatim so the regression cannot come back.
    "  pl_dist_c = DIST_W'(u_px) * DIST_W'(pl_nx_i)\n"
    "            + DIST_W'(u_py) * DIST_W'(pl_ny_i)\n"
    "            - DIST_W'(pl_c_i);",
    "  offer_count_q <= offer_count_q + CREDIT_W'(div_valid_w)\n"
    "                                - CREDIT_W'(offer_pop_c);",
    # and a genuinely unary minus split across a line must STILL fire, so this
    # one belongs in the fire list, not here -- see _MUST_FLAG.
    # and a comment describing the hazard is not the hazard
    "  // never write -W'(x); Quartus 17.0 stops at the apostrophe",
]


def _self_test() -> None:
    bad = []
    for s in _MUST_FLAG:
        if not scan_text(s):
            bad.append("FAILED TO FIRE on: %s" % s.strip())
    for s in _MUST_NOT_FLAG:
        if scan_text(s):
            bad.append("FALSE POSITIVE on: %s" % s.strip()[:70])
    if bad:
        sys.stderr.write("check_quartus17_syntax SELF-TEST FAILED:\n")
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        raise SystemExit(2)


_self_test()


def main() -> int:
    findings, scanned = scan_repo()
    print("check_quartus17_syntax: self-test %d fire / %d no-fire cases PASSED"
          % (len(_MUST_FLAG), len(_MUST_NOT_FLAG)))
    print("scanned %d file(s) under fpga/rtl" % scanned)
    if not findings:
        print("no Quartus-17.0-rejected forms found")
        return 0
    print("\n%d REJECTED FORM(S) -- these fail quartus_map and, because"
          % len(findings))
    print("run_block_map.ps1 compiles every .sv under fpga/rtl, they fail EVERY map:\n")
    for rel, line, form in findings:
        print("  %s:%d" % (rel, line))
        print("      %s" % form)
    print("\nRepair: declare the genvar separately and wrap the loop in explicit")
    print("generate / endgenerate. fpga/rtl/common/zhao_crc32c_fold.sv is the pattern.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
