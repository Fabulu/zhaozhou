#!/usr/bin/env python3
r"""check_array_storage.py -- which declared arrays did NOT become memory?

WHY THIS EXISTS (D19m)
----------------------
`zhao_texture_tmu_pipe` declares

    logic [255:0] pal_val_r [PAL_SLOTS];        //  16 x 256      =  4,096 bits
    logic [15:0]  pal_dat_r [PAL_SLOTS][256];   //  16 x 256 x 16 = 65,536 bits

and synthesised to **72,824 registers against 256 block-memory bits**. Nearly
70 Kbit of palette cache went into flip-flops -- about 87% of every register on
the device -- where as memory it is 7 M10K out of 553.

That was found by reading one fit log. The same question can be asked of every
block from the SOURCE, against the fit results already committed, without
running Quartus at all:

    declared array bits  >>  measured blockMemoryBits   ->  it is in flops

WHAT IT IS AND IS NOT
---------------------
It is a COMPARISON-SIDE tool. It reads what was authored and what was measured
and reports the gap. It never edits RTL, never edits `fit_targets.yml` (the fit
queue polls that file and a non-atomic rewrite of it has broken a run before),
and never launches a fit.

It is a HEURISTIC in one specific way: **not every array should be a memory.**
A four-entry pipeline register file, a small lookup indexed combinationally on
several ports, a shift register -- all are correctly flops, and all appear here
as a gap. So the output is sorted by size and the threshold is stated, because
the interesting cases are large and the small ones are noise.

It also cannot see through parameters it cannot resolve. `logic [W-1:0] x [N]`
with N set by a `parameter` is read when the parameter has a literal default in
the same file, and skipped otherwise -- skipped, not guessed. A guessed width
would be a measurement that decides a value, which this repository does not do.

HOW WELL IT ACTUALLY PREDICTS, MEASURED
---------------------------------------
Checked against blocks whose answer is already known, in both directions --
because a detector that only ever fires is as useless as one that never does.

    block                     declared    measured        verdict
    zhao_texture_tmu_pipe       72,544    72,824 REGISTERS  flagged  (0.4% off)
    zhao_texture_fragrob         6,608     6,464 MEM BITS   silent   (2% off)
    zhao_texture_cache           2,304     8,192 MEM BITS   silent
    zhao_field_v2_front          2,688   266,513 MEM BITS   silent

The two that matter are the first two. `tmu_pipe` put its arrays in flops and
the declared total predicts the REGISTER count to 0.4%. `fragrob` put its arrays
in memory and the same declared total predicts the MEMORY BIT count to 2%. The
arithmetic is reading real storage in both cases; only its destination differs,
which is exactly the question this tool asks.

**AND IT MUST NOT COMPARE ACROSS TIME.** The first version reported
`zhao_texture_tmu` as 9,762 declared bits against 0 memory and only 350
registers, with a note guessing "deleted by a leaf fit whose outputs are
unconnected". That was wrong twice over: the block's current source reads its
array straight into a flop -- the shape that DOES infer -- and the row it was
compared against was measured 2026-08-23 against source last changed 2026-08-30.

**Declared bits come from today's file; measured bits come from whenever the fit
ran.** When those differ, the subtraction is not a weaker check, it is a
different question. Rows older than their source are now reported separately and
never as gaps.

I walked into that trap while writing this tool, and the trap is filed as D19o
-- which was itself found from this tool's output. A finding and its own
counterexample can come from the same afternoon.

**Its weakness is UNDER-counting**, visible in `zhao_field_v2_front`: 2,688
declared against 266,513 measured, because most of that block's arrays are sized
by expressions this cannot resolve and are skipped rather than guessed. That is
the safe direction -- it misses defects, it does not invent them -- but it means
a small declared total is not evidence of a small block. Read the `skipped`
count in the header line.

READ IT WITH THE FIT ROW, NOT INSTEAD OF IT
--------------------------------------------
A gap here is a QUESTION. The answer is in the fit: a block with 13 M10K and
6 Kbit measured (`zhao_texture_fragrob`) inferred its arrays fine even though
they are multidimensional, which is why the multidimensional-array folklore is
not a sufficient explanation on its own. Being multidimensional AND large AND
indexed on both axes is what actually broke inference in tmu_pipe.
"""
from __future__ import annotations

import ast
import io
import json
import os
import re
import subprocess
import sys

# ANCHORED TO THE FILE, NOT THE WORKING DIRECTORY. Both of these were relative
# paths, so running the tool from anywhere but the repo root found no fit rows
# and no RTL and printed a clean, confident, empty report -- the same CWD defect
# CLAUDE.md records being repaired in tools/design/check_counters.py.
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RTL_ROOT = os.path.join(REPO, "fpga", "rtl")
RESULTS = os.path.join(REPO, "reports", "synthesis", "zhao_block_fit.json")

# `logic [15:0] name [A][B];` / `logic [255:0] name [N];` / `logic name [0:3];`
ARRAY_RE = re.compile(
    r"^\s*(?:var\s+)?(?:logic|reg|bit)\s*"
    r"(?:signed\s*)?(\[[^\];]*\]\s*)?"            # packed width, optional
    r"(\w+)\s*"                                   # name
    r"((?:\[[^\];]*\]\s*)+);",                    # one or more unpacked dims
    re.M,   # `^` must mean start-of-LINE. Without this the pattern matches only
            # at offset 0 of the file and the tool reports a confident ZERO --
            # which is how the first run of this file found nothing at all, on
            # a tree containing the 65,536-bit array it was written to catch.
)

# THE OLD FORM, KEPT SO THE WIDENING IS VISIBLE:
#
#   PARAM_RE = ... r"(\w+)\s*=\s*(\d+)"
#
# It read only a DECIMAL LITERAL, so `localparam int W = 2*ELEM_W;` and every
# parameter that lives in a package were invisible. That is why this tool
# reported "0 blocks" beside "206 declaration(s) skipped as unresolvable" -- and
# a zero standing next to a skip count that large is the broken-instrument law
# in its exact documented form: precision at zero is a tell, not a result.
# Widened below to an EXPRESSION, resolved to a fixpoint, with package
# parameters imported. The skip-never-guess law is unchanged: anything the
# evaluator cannot reduce to an int is still SKIPPED.
PARAM_RE = re.compile(
    r"^\s*(?:parameter|localparam)\s+"
    r"(?:type\s+)?"
    r"(?:(?:logic|bit|int|integer|byte|shortint|longint)\s+)?"
    r"(?:unsigned\s+|signed\s+)?"
    r"(?:\[[^\]]*\]\s*)?"                         # an optional packed width
    r"(\w+)\s*=\s*([^;,]+?)\s*(?:[;,]|$)", re.M)

PACKAGE_RE = re.compile(r"^\s*package\s+(\w+)\s*;(.*?)^\s*endpackage", re.M | re.S)
IMPORT_RE = re.compile(r"^\s*import\s+(\w+)\s*::\s*\*\s*;", re.M)

_PKG_CACHE: dict | None = None


def _clog2(value: int) -> int:
    if value <= 1:
        return 0
    return (value - 1).bit_length()


class _Unresolvable(Exception):
    """Raised the moment the evaluator meets something it cannot reduce.

    A separate exception rather than a None return, because this evaluator
    recurses: a None sentinel has to be checked at every node and one missed
    check silently becomes a 0, which is a GUESS, and a guessed size reports a
    defect that may not exist. Raising makes the skip path the default.
    """


def _sv_div(a: int, b: int):
    """SystemVerilog integer division: TRUNCATES TOWARD ZERO, unlike Python's.

    Found by this file's own self-check, before the tool had run once on the
    tree. The first version normalised `/` to Python's `//` and separately
    carried an "exact division only, else skip" rule -- and the normalisation
    meant that rule NEVER RAN: `DEPTH/2` with DEPTH 81 quietly returned 40. The
    self-check was written expecting a skip and reported "evaluator GUESSED".

    Both halves of that were wrong, which is the useful part. 40 is the RIGHT
    answer -- `logic [DEPTH/2-1:0]` really is 40 bits wide, because SV truncates
    -- so refusing it would have SKIPPED a legitimate declaration and
    under-reported, the flattering direction. And Python's floor is the right
    answer only while both operands are non-negative; `-5/2` is -2 in SV and -3
    in Python, so the normalisation was importing a different language's
    arithmetic and agreeing with it by luck on the cases that happen to occur.
    """
    if b == 0:
        return None
    quotient = abs(a) // abs(b)
    return -quotient if (a < 0) != (b < 0) else quotient


def _sv_mod(a: int, b: int):
    """SV `%` takes the sign of the LEFT operand; Python's takes the right."""
    if b == 0:
        return None
    remainder = abs(a) % abs(b)
    return -remainder if a < 0 else remainder


_BIN_OPS = {
    ast.Add: lambda a, b: a + b,
    ast.Sub: lambda a, b: a - b,
    ast.Mult: lambda a, b: a * b,
    ast.FloorDiv: _sv_div,
    ast.Div: _sv_div,
    ast.Mod: _sv_mod,
    ast.LShift: lambda a, b: a << b,
    ast.RShift: lambda a, b: a >> b,
    ast.BitOr: lambda a, b: a | b,
    ast.BitAnd: lambda a, b: a & b,
    ast.BitXor: lambda a, b: a ^ b,
}

SIZED_LITERAL_RE = re.compile(r"\b(?:\d+)?'([sS])?([dDhHbBoO])([0-9a-fA-F_]+)")
_RADIX = {"d": 10, "h": 16, "b": 2, "o": 8}


def _normalise_expr(expr: str) -> str:
    """SystemVerilog expression text -> something Python's parser accepts."""
    text = expr.strip()
    text = re.sub(r"\$clog2", "_clog2", text)
    text = re.sub(r"\bint'\s*", "", text)
    text = re.sub(r"\b\w+'\s*\(", "(", text)          # any cast: WIDTH'(x)
    text = SIZED_LITERAL_RE.sub(
        lambda m: str(int(m.group(3).replace("_", ""), _RADIX[m.group(2).lower()])),
        text)
    # `/` stays `/`: _BIN_OPS maps BOTH ast.Div and ast.FloorDiv to SV's
    # truncating division, so there is nothing to normalise and the old rewrite
    # to `//` only served to hide which operator was written.
    return text


def eval_expr(expr: str, params: dict) -> int:
    """Reduce a parameter/dimension expression to an int, or raise.

    Deliberately NOT a general evaluator: integers, the named parameters already
    resolved, $clog2, and the arithmetic/shift/bitwise operators above. A
    function call, a division that is not exact, an unknown name, a `$bits`, a
    string -- all raise, and the caller skips the declaration.
    """
    try:
        tree = ast.parse(_normalise_expr(expr), mode="eval")
    except SyntaxError as exc:
        raise _Unresolvable(expr) from exc

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant):
            if isinstance(node.value, bool) or not isinstance(node.value, int):
                raise _Unresolvable(ast.dump(node))
            return node.value
        if isinstance(node, ast.Name):
            if node.id in params:
                return params[node.id]
            raise _Unresolvable(node.id)
        if isinstance(node, ast.Attribute):          # pkg::NAME after normalising
            raise _Unresolvable(ast.dump(node))
        if isinstance(node, ast.UnaryOp):
            if isinstance(node.op, ast.USub):
                return -walk(node.operand)
            if isinstance(node.op, ast.UAdd):
                return walk(node.operand)
            if isinstance(node.op, ast.Invert):
                raise _Unresolvable("~")
            raise _Unresolvable(ast.dump(node.op))
        if isinstance(node, ast.BinOp):
            handler = _BIN_OPS.get(type(node.op))
            if handler is None:
                raise _Unresolvable(ast.dump(node.op))
            value = handler(walk(node.left), walk(node.right))
            if value is None:
                raise _Unresolvable("inexact division")
            return value
        if isinstance(node, ast.Call):
            if isinstance(node.func, ast.Name) and node.func.id == "_clog2" \
                    and len(node.args) == 1 and not node.keywords:
                return _clog2(walk(node.args[0]))
            raise _Unresolvable("call")
        raise _Unresolvable(ast.dump(node))

    value = walk(tree)
    if not isinstance(value, int):
        raise _Unresolvable(expr)
    return value


def collect_params(text: str, seed: dict | None = None) -> dict:
    """Every parameter/localparam this text defines, resolved to a fixpoint.

    Iterated because parameters are defined in terms of each other -- `DEPTH`,
    then `INDEX_W = $clog2(DEPTH) + 1` -- and a single pass in file order would
    resolve only the ones whose dependencies happen to appear above them.
    """
    params = dict(seed or {})
    pending = [(m.group(1), m.group(2)) for m in PARAM_RE.finditer(text)]
    for _ in range(len(pending) + 1):
        progress = False
        remaining = []
        for name, expr in pending:
            if name in params:
                continue
            try:
                params[name] = eval_expr(expr, params)
                progress = True
            except _Unresolvable:
                remaining.append((name, expr))
        pending = remaining
        if not pending or not progress:
            break
    return params


def package_params(repo_root: str) -> dict:
    """{package name: {parameter: value}} for every package in the RTL tree.

    A package parameter is the single largest reason a declaration was skipped:
    the widths this machine actually uses live in `zhao_pkg`, not beside the
    array. Resolved once and cached.
    """
    global _PKG_CACHE
    if _PKG_CACHE is not None:
        return _PKG_CACHE
    found: dict = {}
    for root, _dirs, files in os.walk(repo_root):
        for name in files:
            if not name.endswith(".sv") and not name.endswith(".svh"):
                continue
            try:
                text = read(os.path.join(root, name))
            except OSError:
                continue
            for match in PACKAGE_RE.finditer(text):
                found[match.group(1)] = collect_params(match.group(2))
    _PKG_CACHE = found
    return found


def file_params(path: str, text: str) -> dict:
    """The parameters visible inside one file: its imports, then its own."""
    seed: dict = {}
    for pkg in IMPORT_RE.findall(text):
        seed.update(package_params(RTL_ROOT).get(pkg, {}))
    return collect_params(text, seed)


# Never ship a detector that has not been shown to fire. This is the exact shape
# from D19m; if the pattern stops matching it, this tool is broken, not the tree.
assert ARRAY_RE.search("  logic [15:0]  pal_dat_r [PAL_SLOTS][256];"), (
    "ARRAY_RE no longer matches the declaration this tool exists to find")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


# ---------------------------------------------------------------------------
# THE EVALUATOR'S OWN POSITIVE AND NEGATIVE CONTROLS, run at import.
#
# A detector that has not been shown to fire has not been tested, and this file
# already carries one such assert for ARRAY_RE. The widened resolver needs BOTH
# directions, because its two failure modes point opposite ways: a form it
# cannot read makes an array invisible (under-reports, the flattering
# direction), and a form it reads WRONG invents a size (reports a defect that
# may not exist). The negative half is the one that would otherwise never be
# checked -- nobody audits a tool for refusing things.
def _self_check() -> None:
    params = {"DEPTH": 81, "ELEM_W": 32, "ARENAS": 4}
    must_resolve = {
        "16": 16,
        "DEPTH": 81,
        "DEPTH - 1": 80,
        "2 * ELEM_W": 64,
        "ELEM_W * 3 + 7": 103,
        "$clog2(DEPTH) + 1": 8,
        "$clog2(ARENAS)": 2,
        "1 << 4": 16,
        "8'd64": 64,
        "32'h20": 32,
        "int'(DEPTH)": 81,
        "(DEPTH + 1) / 2": 41,
        # SV truncates toward zero. 81/2 is 40 and the array really is 40 bits
        # wide, so this must RESOLVE, not skip -- and the negative case is the
        # one where Python's floor would silently disagree.
        "DEPTH / 2": 40,
        "(0 - 5) / 2": -2,
        "(0 - 5) % 2": -1,
    }
    for expr, expected in must_resolve.items():
        got = eval_expr(expr, params)
        if got != expected:
            raise AssertionError(
                "check_array_storage evaluator is broken: %r -> %r, expected %r"
                % (expr, got, expected))
    must_refuse = (
        "WIDTH_THAT_DOES_NOT_EXIST",
        "$bits(some_struct_t)",
        "func(DEPTH)",
        "\"a string\"",
    )
    for expr in must_refuse:
        try:
            value = eval_expr(expr, params)
        except _Unresolvable:
            continue
        raise AssertionError(
            "check_array_storage evaluator GUESSED %r -> %r; it must skip"
            % (expr, value))
    # $clog2 is the one that silently produces an off-by-one if written from
    # memory, so it is pinned at the two boundaries that distinguish the forms.
    if (_clog2(1), _clog2(2), _clog2(3), _clog2(4), _clog2(81)) != (0, 1, 2, 2, 7):
        raise AssertionError("check_array_storage $clog2 is not Verilog's $clog2")


def _git_date(*args):
    """The commit date, asked INSIDE the repo.

    `cwd=REPO` is not tidiness. Without it this ran git in whatever directory
    the caller happened to be in; from outside the repository every call
    failed, `row_predates_source` answered False for everything, and ten rows
    that ARE older than their source moved silently out of the
    "cannot be compared" bucket and into "DECLARED BUT NOT IN MEMORY" -- a
    manufactured finding. Measured: run from the repo root this file reports 0
    flagged and 10 predating; run from C:\\ it reported 1 flagged and 0
    predating, off the same tree.

    That is this file's own documented trap -- comparing a current file against
    a stale measurement is a different question wearing the same shape -- with
    the working directory as the mechanism, and it fails toward a FINDING
    rather than toward silence, so it would have been chased rather than
    noticed.
    """
    r = subprocess.run(["git", "log", "-1", "--format=%cI"] + list(args),
                       capture_output=True, text=True, cwd=REPO)
    return r.stdout.strip() or None


def row_predates_source(row, path):
    """Is the measurement OLDER than the file it is being compared against?

    This tool compares bits declared in TODAY'S source against memory measured
    in some past fit. When the source has changed since, that comparison is
    meaningless -- and it produces confident nonsense: `zhao_texture_tmu` was
    reported as having 9,762 declared bits against 0 memory and only 350
    registers, and the note attached to it guessed "deleted by a leaf fit".
    Wrong. The row was measured 2026-08-23 against source last changed
    2026-08-30, and the current code reads the array straight into a flop --
    the shape that DOES infer (QUARTUS_GOTCHAS 14).

    I walked into exactly the trap that is filed as D19o, while writing the
    tool whose output produced D19o. Comparing a current file to a stale
    measurement is not a weaker version of the check; it is a different
    question with the same shape.
    """
    measured = _git_date(row.get("sourceCommit") or "HEAD")
    changed = _git_date("--", path)
    return bool(measured and changed and changed > measured)


def dim_size(expr, params):
    """Elements in one `[...]` dimension. `[N]` is N, `[a:b]` is |a-b|+1.

    Returns None when anything in the expression cannot be resolved. None means
    SKIP, never a guess: a tool that invents a size reports a defect that may
    not exist.
    """
    e = expr.strip()[1:-1].strip()
    if ":" in e:
        a, b = e.split(":", 1)
        va, vb = resolve(a, params), resolve(b, params)
        if va is None or vb is None:
            return None
        return abs(va - vb) + 1
    return resolve(e, params)


def resolve(tok, params):
    try:
        return eval_expr(tok, params)
    except _Unresolvable:
        return None


def bits_of(path):
    """Total declared bits across every resolvable unpacked array in the file."""
    s = read(path)
    params = file_params(path, s)

    total, biggest, skipped = 0, [], 0
    for m in ARRAY_RE.finditer(s):
        packed, name, unpacked = m.group(1), m.group(2), m.group(3)
        w = 1
        if packed:
            w = dim_size(packed, params)
            if w is None:
                skipped += 1
                continue
        n = 1
        ok = True
        for d in re.findall(r"\[[^\]]*\]", unpacked):
            v = dim_size(d, params)
            if v is None:
                ok = False
                break
            n *= v
        if not ok:
            skipped += 1
            continue
        b = w * n
        total += b
        biggest.append((b, name, "%s%s" % (packed or "", unpacked.strip())))
    biggest.sort(reverse=True)
    return total, biggest[:3], skipped


_self_check()


def main() -> int:
    thresh = 8192
    for a in sys.argv[1:]:
        if a.startswith("--min="):
            thresh = int(a.split("=", 1)[1])

    rows = {}
    if os.path.exists(RESULTS):
        d = json.loads(read(RESULTS))
        rs = d if isinstance(d, list) else d.get("blocks", d)
        if isinstance(rs, dict):
            rs = list(rs.values())
        rows = {r.get("module"): r for r in rs if r.get("module")}

    found, unmeasured, outdated, skipped_total = [], [], [], 0
    for root, _dirs, files in os.walk(RTL_ROOT):
        for f in files:
            if not f.endswith(".sv"):
                continue
            mod = f[:-3]
            total, biggest, skipped = bits_of(os.path.join(root, f))
            skipped_total += skipped
            if total < thresh:
                continue
            row = rows.get(mod)
            if row is None:
                unmeasured.append((total, mod, biggest))
                continue
            mem, reg = row.get("blockMemoryBits"), row.get("registers")
            # A row that TIMED OUT or errored carries no numbers. Reading those
            # as zero turns "not measured" into "zero memory" and invents a
            # gap -- `zhao_forge_cliff` (status: timeout) was reported that way,
            # with 119,808 declared bits against 0 memory AND 0 registers, a
            # combination no real block can have.
            if mem is None or reg is None:
                unmeasured.append((total, mod, biggest))
                continue
            # A measurement older than the file it is compared against answers
            # a different question. See row_predates_source().
            if row_predates_source(row, os.path.join(root, f)):
                outdated.append((total, mod, biggest))
                continue
            if mem < total // 2:
                # A leaf fit with unconnected outputs lets synthesis DELETE
                # storage nothing observes, so few registers and no memory can
                # mean "optimised away" rather than "in flops".
                # `zhao_texture_tmu`: 9,762 declared bits, 350 registers, 0
                # memory -- 350 flops cannot hold 9,762 bits, so the array is
                # not there at all. Flagged, with the reason named.
                gone = reg * 2 < total // 2
                found.append((total - mem, total, mem, reg, mod, biggest, gone))

    found.sort(reverse=True)
    unmeasured.sort(reverse=True)

    outdated.sort(reverse=True)
    print("array storage: %d measured block(s) declare >= %d array bits with "
          "less than half of it in block memory; %d further block(s) declare "
          "that much but have no fit row yet; %d whose row PREDATES the source "
          "and cannot be compared; %d declaration(s) skipped as unresolvable"
          % (len(found), thresh, len(unmeasured), len(outdated), skipped_total))

    if found:
        print("\nDECLARED BUT NOT IN MEMORY -- largest gap first. A gap is a "
              "QUESTION, not a verdict: a small array indexed on many ports is "
              "correctly flops. Read each against its fit row:")
        for gap, total, mem, reg, mod, biggest, gone in found:
            note = ("   <- too few registers to HOLD it either: likely deleted "
                    "by a leaf fit whose outputs are unconnected" if gone else "")
            print("  %-34s declared %8d bits, memory %7d, registers %6d%s"
                  % (mod, total, mem, reg, note))
            for b, name, shape in biggest:
                print("        %8d bits  %-22s %s" % (b, name, shape))

    if unmeasured:
        print("\nNO FIT ROW (%d) -- declares the bits, nothing has measured "
              "where they went:" % len(unmeasured))
        for total, mod, biggest in unmeasured[:10]:
            print("  %-34s declared %8d bits   largest: %s"
                  % (mod, total, biggest[0][1] if biggest else "-"))

    print("\nNOTE: heuristic. Not every array should be a memory, and a "
          "declaration whose size cannot be resolved from literals and "
          "same-file parameter defaults is SKIPPED rather than guessed. "
          "REPORTS; never gates, never edits RTL or fit_targets.yml.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
