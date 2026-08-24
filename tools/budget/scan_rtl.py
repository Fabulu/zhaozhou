#!/usr/bin/env python3
"""scan_rtl.py -- an ELABORATED-AST inventory of the Zhaozhou RTL.

WHY THIS IS NOT A REGEX
=======================
`docs/OWNER_DOCKET.md` records the owner trying twice to count nonconstant
multiplies in `zhao_geom_project` with grep: the first attempt returned 0, the
second returned line counts.  Both were useless, and the reason is structural
rather than a matter of a better pattern.

`zhao_geom_project` contains exactly **three** `*` operators in its text.  One
of them lives inside

    function automatic logic signed [63:0] mul32(a, b);
      mul32 = $signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b});

which the module calls **nine times**.  So the honest answer is 1x9 + 2 = **11
nonconstant multiplies**, and no pattern matcher can reach it, because the
number nine is not written anywhere near the `*`.  Quartus then reports 33 DSP
blocks for that module -- exactly three per product.  The chain
`3 operators -> 11 products -> 33 DSPs` is the whole argument for this tool.

The scanner therefore works from Verilator's elaborated AST.

  NOTE ON THE FLAG NAME.  The docket asks for `verilator --xml-only`.  This
  machine's Verilator is 5.051, which REMOVED `--xml-only` in favour of
  `--json-only` (`Vtop.tree.json` + `Vtop.tree.meta.json`).  That is the
  "or equivalent" the ruling allows; it is the same elaborated tree in a
  different serialisation, and the flag was verified against the tool rather
  than assumed.

WHAT IT EMITS
=============
One record per module, covering what the ruling asked for:

  multiplies      every NONCONSTANT multiply, with operand widths BEFORE and
                  AFTER peeling sign/zero extension, signedness, result width,
                  dependency-chain depth, whether an operand is provably
                  constant or provably narrow, and whether it is a
                  mux-before-multiply candidate (two products under mutually
                  exclusive arms of one condition, sharing an operand)
  shifts          variable shifts, with the shift-amount width
  divides         division and modulo, constant divisor called out separately
                  because QUARTUS_GOTCHAS 2 records `lpm_divide` refusing
                  numerators wider than 64 bits
  loops           serial loops in COMBINATIONAL context, with trip count
  satchains       wide add -> compare -> saturate chains
  duplicates      identical expensive subtrees computed more than once
                  (SURFACE.STAMP computed one 66-bit rescale twice, the second
                  time only for a ledger bit)
  arrays          total bits, read style (SYNC/ASYNC), reset-touched, dynamic
                  bank selection, read/write port counts, ACCESS SITES PER
                  ELEMENT (which separates storage from a pipeline), and the
                  expected RAM/ROM behaviour
  const_roms      combinational selects over many constants -- a ROM built from
                  LUTs, which no array check can see because it is not an array
  interface       direct input->arithmetic->output paths, FSM state count,
                  ready-only-in-IDLE, inferred minimum initiation interval
  counters        counters whose enable depends on deep combinational logic

Every finding carries a severity and the REASON for it.

WHAT IT DOES NOT DO, stated so nobody reads more into a GREEN
============================================================
* It does not synthesise.  A multiply's DSP cost is decided by Quartus, and
  `design/budgets/dsp.md` was corrected today precisely because width and
  signedness change that cost discontinuously.  Use
  `tools/budget/calibration.json` to turn these counts into resources.
* It does not time anything.  Depth here is AST depth, not nanoseconds.
* Its detectors are validated by positive and negative controls in
  `runs/CLAUDE-RUNS/RUN-20260823-2226-budget-audit-wave1/validate_detectors.py`.
  Run them after changing a rule.  ONE OF THEM HAS ALREADY CAUGHT A DETECTOR
  THAT COULD NEVER FIRE, and a detector that never fires is indistinguishable
  from a clean repository.
* The inferred initiation interval is derived from the state-transition graph.
  It is an upper bound on throughput, not a measurement, and a block whose II
  matters still needs an executable II test -- which is why `NO_II_TEST`
  exists as a debt flag rather than being papered over here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RTL = os.path.join(REPO, "fpga", "rtl")

# docs/BUILD.md: invoke verilator_bin.exe, never the perl wrapper.
VERILATOR = os.environ.get(
    "ZHAO_VERILATOR",
    r"C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe",
)
VERILATOR_ROOT = os.environ.get(
    "VERILATOR_ROOT",
    r"C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator",
)

SEV_ORDER = {"GREEN": 0, "YELLOW": 1, "ORANGE": 2, "RED": 3}


def sev_max(a, b):
    return a if SEV_ORDER[a] >= SEV_ORDER[b] else b


# ---------------------------------------------------------------------------
# source discovery
# ---------------------------------------------------------------------------

def rtl_files():
    out = []
    for dirpath, _dirs, files in os.walk(RTL):
        for f in sorted(files):
            if f.endswith(".sv"):
                out.append(os.path.join(dirpath, f).replace(os.sep, "/"))
    return sorted(out)


PKG_FIRST = [
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    "fpga/rtl/memory/zhao_sdram_params_pkg.sv",
]


def ordered_sources():
    allf = rtl_files()
    head = [os.path.join(REPO, p).replace(os.sep, "/") for p in PKG_FIRST]
    tail = [f for f in allf if f not in head]
    return head + tail


def discover_modules():
    """Enumerate module names.

    This one step IS textual, and it is the only one.  It answers "what should
    I ask Verilator to elaborate", not "what is in this module" -- a wrong
    answer here produces a missing record, which is visible, rather than a
    wrong number, which is not.
    """
    mods = {}
    for f in rtl_files():
        src = open(f, encoding="utf-8", errors="replace").read()
        stripped = re.sub(r"//[^\n]*", "", src)
        stripped = re.sub(r"/[*].*?[*]/", "", stripped, flags=re.S)
        for m in re.finditer(r"^\s*module\s+(\w+)", stripped, flags=re.M):
            mods[m.group(1)] = f
    return mods


# ---------------------------------------------------------------------------
# verilator
# ---------------------------------------------------------------------------

def elaborate(top, outdir, sources, quiet=True):
    os.makedirs(outdir, exist_ok=True)
    env = dict(os.environ)
    env["VERILATOR_ROOT"] = VERILATOR_ROOT
    cmd = [
        VERILATOR, "--json-only", "--bbox-unsup", "-Wno-fatal",
        "--top-module", top, "--Mdir", outdir,
    ] + sources
    p = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=REPO)
    tree = os.path.join(outdir, "V%s.tree.json" % top)
    meta = os.path.join(outdir, "V%s.tree.meta.json" % top)
    if not os.path.exists(tree):
        return None, None, (p.stdout + p.stderr)[-4000:]
    return tree, meta, (p.stderr[-2000:] if p.returncode else "")


def read_meta_files(meta_path):
    """file-letter -> repo-relative path.

    Parsed with a regex rather than json.load because Verilator 5.051 emits
    Windows paths into the `filename` field WITHOUT escaping the backslash --
    `"fpga/rtl/common\\zhao_pkg.sv"` is not valid JSON, and json.load raises
    `Invalid \\escape`.  Found the hard way; the `realpath` field is properly
    escaped, so that is the one read here.
    """
    if not meta_path or not os.path.exists(meta_path):
        return {}
    txt = open(meta_path, encoding="utf-8", errors="replace").read()
    out = {}
    for m in re.finditer(r'"(\w+)":\s*\{[^{}]*?"realpath"\s*:\s*"((?:[^"\\]|\\.)*)"', txt):
        raw = m.group(2).replace("\\\\", "/").replace("\\", "/")
        low = raw.replace("//", "/")
        idx = low.lower().find("/fpga/rtl/")
        out[m.group(1)] = low[idx + 1:] if idx >= 0 else low
    return out


# ---------------------------------------------------------------------------
# AST helpers
# ---------------------------------------------------------------------------

KIDS_SKIP = {"type", "name", "addr", "loc", "dtypep", "varp", "taskp",
             "refDTypep", "typedefp", "classOrPackagep", "varScopep",
             "sensIfacep"}


def children(n):
    for k, v in n.items():
        if k in KIDS_SKIP:
            continue
        if isinstance(v, list):
            for c in v:
                if isinstance(c, dict):
                    yield k, c


def walk(n):
    yield n
    for _k, c in children(n):
        yield from walk(c)


def loc_line(n):
    lo = n.get("loc") or ""
    m = re.match(r"(\w+),(\d+):", lo)
    return (m.group(1), int(m.group(2))) if m else (None, None)


class Types:
    """dtype address -> width / signedness / kind, memoised."""

    def __init__(self, root):
        self.by_addr = {}
        for n in walk(root):
            a = n.get("addr")
            if a and n.get("type", "").endswith("DTYPE"):
                self.by_addr[a] = n
        self.cache = {}

    def info(self, addr, depth=0):
        if addr is None or depth > 12:
            return {"width": None, "signed": False, "kind": "unknown"}
        if addr in self.cache:
            return self.cache[addr]
        n = self.by_addr.get(addr)
        res = {"width": None, "signed": False, "kind": "unknown"}
        if n is not None:
            t = n.get("type")
            res["kind"] = t
            if t == "BASICDTYPE":
                rng = n.get("range")
                if rng and ":" in rng:
                    hi, lo = rng.split(":")
                    try:
                        res["width"] = abs(int(hi) - int(lo)) + 1
                    except ValueError:
                        pass
                else:
                    res["width"] = 1
                res["signed"] = bool(n.get("signed"))
            elif t in ("REFDTYPE", "MEMBERDTYPE", "ENUMDTYPE"):
                sub = self.info(n.get("refDTypep"), depth + 1)
                res["width"] = sub["width"]
                res["signed"] = sub["signed"]
                if t == "ENUMDTYPE":
                    res["kind"] = "ENUMDTYPE"
                    res["items"] = len(n.get("itemsp") or [])
            elif t == "STRUCTDTYPE":
                tot, ok = 0, True
                for mem in (n.get("membersp") or []):
                    w = self.info(mem.get("refDTypep") or mem.get("dtypep"), depth + 1)["width"]
                    if w is None:
                        ok = False
                        break
                    tot += w
                res["width"] = tot if ok else None
            elif t == "UNPACKARRAYDTYPE":
                dr = n.get("declRange") or ""
                m = re.match(r"\[(-?\d+):(-?\d+)\]", dr)
                depth_n = abs(int(m.group(2)) - int(m.group(1))) + 1 if m else None
                sub = self.info(n.get("refDTypep"), depth + 1)
                res["kind"] = "UNPACKARRAYDTYPE"
                res["elems"] = depth_n
                res["elemWidth"] = sub["width"]
                res["signed"] = sub["signed"]
                if depth_n and sub["width"]:
                    res["width"] = depth_n * sub["width"]
        self.cache[addr] = res
        return res

    def width(self, node):
        return self.info(node.get("dtypep"))["width"]

    def signed(self, node):
        return self.info(node.get("dtypep"))["signed"]


# ---------------------------------------------------------------------------
# expression classification
# ---------------------------------------------------------------------------

MUL_TYPES = {"MUL", "MULS"}
DIV_TYPES = {"DIV", "DIVS", "MODDIV", "MODDIVS"}
SHIFT_TYPES = {"SHIFTL", "SHIFTR", "SHIFTRS"}
CMP_TYPES = {"GT", "GTS", "GTE", "GTES", "LT", "LTS", "LTE", "LTES"}
ADD_TYPES = {"ADD", "SUB"}
ARITH = MUL_TYPES | DIV_TYPES | ADD_TYPES | SHIFT_TYPES | CMP_TYPES


def only_child(n, key):
    v = n.get(key) or []
    return v[0] if v else None


def is_constant_cone(n):
    """True when the subtree contains no VARREF/ARRAYSEL at all."""
    for x in walk(n):
        if x.get("type") in ("VARREF", "ARRAYSEL", "FUNCREF"):
            return False
    return True


def pure_extension_funcs(mnode, types):
    """Functions whose entire body is a sign or zero extension of one argument.

    WHY THIS IS NEEDED, and how the gap showed itself.

    `zhao_geom_project` and `zhao_terrain_project` both write

        mad_x = ext32m(s5_ndc_x) * $signed({... vp_w ..., 15'b0}) + ...

    where `ext32m` is `$signed({{(MAD_W-32){v[31]}}, v})` -- a 32-bit value
    widened to 64 by a function call. `peel_extension` could not see through
    the FUNCREF, so it reported those two products as **64x64** and rated them
    RED for carrying no peelable slack.

    Measurement says otherwise, and says it exactly. Both modules map at 33 DSP
    blocks for 11 products -- THREE each, uniformly, which is the 32x32
    decomposition. A genuine 64x64 signed product is far more than three
    blocks, so if two of the eleven were really 64-bit the total could not be
    33. The extension is folded away by Quartus whether it is written inline or
    behind a function.

    This is the SAME false positive as the inline-extension rule that had to be
    withdrawn earlier in this run, wearing a function call. Both were caught
    the same way: by a measured number refusing to agree.
    """
    out = {}
    for f in walk(mnode):
        if f.get("type") != "FUNC":
            continue
        fvar = (f.get("fvarp") or [None])[0]
        args = [v for v in walk(f) if v.get("type") == "VAR"
                and v.get("direction") == "INPUT"]
        if fvar is None or len(args) != 1:
            continue
        # Verilator prepends an `ext32m = CRESET` initialiser to every
        # function body, so "the body is one assignment" is never literally
        # true and the first version of this check rejected every candidate.
        body = [s for s in walk(f) if s.get("type") in ("ASSIGN", "ASSIGNW")
                and (only_child(s, "rhsp") or {}).get("type") != "CRESET"]
        if len(body) != 1:
            continue
        lhs, rhs = only_child(body[0], "lhsp"), only_child(body[0], "rhsp")
        if lhs is None or rhs is None or lhs.get("type") != "VARREF":
            continue
        if lhs.get("varp") != fvar.get("addr"):
            continue
        # NOTE: peel_extension returns the ORIGINAL node, not a peeled one --
        # it computes a width, it does not rewrite the tree. An earlier version
        # of this check compared the returned node against the argument and
        # therefore matched nothing once the peel became a width recursion.
        # The structural test is what it always should have been: the body is a
        # widening, and the only signal it reads is the one argument.
        _n, _d, _h, how = peel_extension(rhs, types)
        if how == "none":
            continue
        refs = {v.get("varp") for v in walk(rhs) if v.get("type") == "VARREF"}
        if refs != {args[0].get("addr")}:
            continue
        out[f["addr"]] = True
    return out


def peel_extension(n, types, ext_funcs=None):
    """The HONEST width of a multiply operand, and how it differs from declared.

    QUARTUS_GOTCHAS 5 is the reason this exists: the SAME `zhao_geom_lod`
    source cost **28 DSPs at 72-bit operands and 18 at 64-bit**.  A 32-bit
    value widened to 64 asks Quartus for a 64x64 multiplier; the honest need is
    32x32.  "Prove the width, then synthesise" needs something that can compute
    the proven width, and the declared `dtypep` is not it.

    Three widening forms appear in this repository and all three are folded:

        {{32{a[31]}}, a}          sign extension, written inline
        ext32m(x)                 sign extension, written as a function
        {x, 15'b0}                a left SHIFT wearing a concatenation

    The third is why this became a width RECURSION rather than a loop that
    peels wrappers off one node.  `mad_x`'s right operand is
    `{$unsigned(vp_w[view]), 15'b0}` inside a 64-bit lane: peeling wrappers
    finds nothing to remove and reports 64, when the value is 12 + 15 = **27**
    bits.  Measurement settles which is right -- both projectors map at 33 DSP
    blocks for 11 products, three each, uniformly, and a genuine 64x64 signed
    product is not three blocks.

    Returns (representative_node, declared_width, honest_width, how).
    """
    declared = types.width(n)

    def honest(node, depth=0):
        if node is None or depth > 12:
            return types.width(node) if node is not None else None, "none"
        t = node.get("type")
        if t == "CONST":
            return types.width(node), "none"
        if t in ("EXTEND", "EXTENDS"):
            sub = only_child(node, "lhsp") or only_child(node, "srcp")
            w, _ = honest(sub, depth + 1)
            return w, "extend"
        if t == "FUNCREF" and ext_funcs and ext_funcs.get(node.get("taskp")):
            args = node.get("argsp") or []
            if args:
                a0 = args[0]
                inner = a0 if a0.get("type") != "ARG" else only_child(a0, "exprp")
                w, _ = honest(inner, depth + 1)
                return w, "extension-function"
        if t == "CONCAT":
            lhs, rhs = only_child(node, "lhsp"), only_child(node, "rhsp")
            if lhs is not None and rhs is not None:
                # {{k{v[msb]}}, v} -- sign extension
                if lhs.get("type") == "REPLICATE":
                    src = only_child(lhs, "srcp")
                    if src is not None and types.width(src) == 1:
                        w, _ = honest(rhs, depth + 1)
                        return w, "concat-replicate"
                # {k'b0, v} -- zero extension
                if lhs.get("type") == "CONST" and re.match(r"^\d+'[hbdo]?0+$", lhs.get("name", "")):
                    w, _ = honest(rhs, depth + 1)
                    return w, "concat-zero"
                # {v, k'b0} and every other pack -- a SHIFT, so the value needs
                # the payload's honest width plus the padding, not the lane's.
                lw, _ = honest(lhs, depth + 1)
                rw = types.width(rhs)
                if lw is not None and rw is not None:
                    return lw + rw, "concat-shift"
        return types.width(node), "none"

    w, how = honest(n)
    return n, declared, w, how


def expr_depth(n, limit=64):
    """Longest operator chain under this node.  AST depth, NOT nanoseconds."""
    best = 0
    stack = [(n, 0)]
    while stack:
        node, d = stack.pop()
        if d > limit:
            return limit
        t = node.get("type")
        nd = d + 1 if t in ARITH or t in ("COND", "AND", "OR", "XOR", "NOT",
                                          "EQ", "NEQ", "REDAND", "REDOR") else d
        if nd > best:
            best = nd
        for _k, c in children(node):
            stack.append((c, nd))
    return best


def canon(n, types, depth=0):
    """Structural signature of a subtree, for duplicate detection."""
    if depth > 14:
        return "..."
    t = n.get("type")
    if t == "CONST":
        return "K:" + str(n.get("name"))
    if t == "VARREF":
        return "V:" + str(n.get("name"))
    w = types.width(n)
    parts = [t, str(w)]
    for k, c in children(n):
        parts.append(k + "=" + canon(c, types, depth + 1))
    return "(" + "|".join(parts) + ")"


def is_expensive(n):
    for x in walk(n):
        t = x.get("type")
        if t in MUL_TYPES or t in DIV_TYPES:
            return True
        if t in SHIFT_TYPES:
            amt = only_child(x, "rhsp")
            if amt is not None and not is_constant_cone(amt):
                return True
    return False


# ---------------------------------------------------------------------------
# per-module analysis
# ---------------------------------------------------------------------------

RESET_RE = re.compile(r"(^|_)(rst|reset|rst_n|nrst|aresetn)(_|$)", re.I)
READY_RE = re.compile(r"(ready|rdy|accept|can_accept)", re.I)
IDLE_RE = re.compile(r"IDLE|S_IDLE|ST_IDLE", re.I)
STATE_RE = re.compile(r"(^|_)(state|st|fsm|phase)(_|$)", re.I)


class ModuleScan:
    def __init__(self, mod, types, filemap, funcs_by_addr):
        self.mod = mod
        self.types = types
        self.filemap = filemap
        self.funcs = funcs_by_addr
        self.findings = []

    def loc(self, n):
        f, ln = loc_line(n)
        return {"file": self.filemap.get(f, f), "line": ln}

    def add(self, kind, sev, reason, node, **extra):
        rec = {"kind": kind, "severity": sev, "reason": reason}
        rec.update(self.loc(node))
        rec.update(extra)
        self.findings.append(rec)
        return rec


def build_func_call_counts(module_node):
    """How many times each function body is INSTANTIATED as hardware.

    Combinational function calls are not calls at synthesis time -- each one
    elaborates its own copy of the body.  `zhao_geom_project` calls `mul32`
    nine times and gets nine multipliers from one written `*`.

    Counts are propagated transitively: a function called three times, which
    itself calls another twice, instantiates the inner body six times.  Solved
    by iteration rather than recursion so a cyclic reference (which is illegal
    in SystemVerilog but not impossible in a malformed tree) terminates.
    """
    direct = defaultdict(Counter)      # container addr -> Counter(callee addr)
    funcs = {}
    for n in walk(module_node):
        if n.get("type") == "FUNC":
            funcs[n["addr"]] = n

    def container_of(node_addr_path):
        return node_addr_path

    # count FUNCREFs, attributing each to the FUNC that lexically contains it
    def scan(node, container):
        for k, c in children(node):
            if c.get("type") == "FUNC":
                scan(c, c["addr"])
            else:
                if c.get("type") == "FUNCREF":
                    direct[container][c.get("taskp")] += 1
                scan(c, container)

    scan(module_node, "@top")

    counts = {a: 0 for a in funcs}
    counts["@top"] = 1
    for _ in range(len(funcs) + 2):
        nxt = {a: 0 for a in funcs}
        nxt["@top"] = 1
        for container, callees in direct.items():
            cc = counts.get(container, 0)
            for callee, k in callees.items():
                if callee in nxt:
                    nxt[callee] += cc * k
        if nxt == counts:
            break
        counts = nxt
    return funcs, counts, direct


def collect_module_nodes(root):
    """MODULE nodes keyed by name."""
    out = {}
    for n in walk(root):
        if n.get("type") == "MODULE" and n.get("name") and not n["name"].startswith("@"):
            out[n["name"]] = n
    return out


def instance_counts(root, top):
    """How many times each module is instantiated inside `top`'s cone.

    WITHOUT THIS THE AUDIT UNDERCOUNTS THE WAY THE OLD CENSUS DID.
    `zhao_raster_fragment` contains no multiply of its own -- all ten of its
    measured DSPs live in three instances of `zhao_raster_blend`, one per
    colour channel. A per-MODULE scanner reports it GREEN, which is precisely
    the shape of error that let `TEXTURE.BILERP inside TEXTURE.TMU` be counted
    twice in one direction and zero times in the other.

    CELL nodes carry `modp`, the address of the MODULE they instantiate, so
    the cone and its multipliers are exact rather than inferred from names.
    """
    by_addr = {}
    for n in walk(root):
        if n.get("type") == "MODULE" and n.get("name"):
            by_addr[n["addr"]] = n["name"]
    direct = defaultdict(Counter)
    for n in walk(root):
        if n.get("type") != "MODULE" or not n.get("name"):
            continue
        for c in walk(n):
            if c.get("type") == "CELL":
                callee = by_addr.get(c.get("modp"))
                if callee and callee != n["name"]:
                    direct[n["name"]][callee] += 1
    counts = Counter({top: 1})
    frontier = [(top, 1)]
    guard = 0
    while frontier and guard < 4096:
        guard += 1
        name, mult = frontier.pop()
        for callee, k in direct.get(name, {}).items():
            counts[callee] += mult * k
            frontier.append((callee, mult * k))
    return counts, direct


def analyse_module(name, mnode, types, filemap):
    S = ModuleScan(name, types, filemap, {})
    funcs, call_counts, _direct = build_func_call_counts(mnode)
    ext_funcs = pure_extension_funcs(mnode, types)

    # which FUNC (if any) lexically contains each node addr
    owner = {}

    def mark(node, cur):
        for _k, c in children(node):
            nxt = c["addr"] if c.get("type") == "FUNC" else cur
            owner[c.get("addr")] = nxt
            mark(c, nxt)
    owner[mnode.get("addr")] = "@top"
    mark(mnode, "@top")

    def instances(node):
        return call_counts.get(owner.get(node.get("addr"), "@top"), 1)

    # ---- clocked vs combinational context -------------------------------
    clocked_nodes, comb_nodes = set(), set()
    for n in walk(mnode):
        if n.get("type") == "ALWAYS":
            st = (n.get("sentreep") or [None])[0]
            edged = False
            if st:
                for si in walk(st):
                    if si.get("type") == "SENITEM" and si.get("edgeType") in ("POS", "NEG", "BOTH"):
                        edged = True
            target = clocked_nodes if edged else comb_nodes
            for x in walk(n):
                target.add(x.get("addr"))
        elif n.get("type") in ("ASSIGNW",):
            for x in walk(n):
                comb_nodes.add(x.get("addr"))

    def in_clocked(n):
        return n.get("addr") in clocked_nodes

    # =====================================================================
    # 1. MULTIPLIES
    # =====================================================================
    mul_records = []
    for n in walk(mnode):
        if n.get("type") not in MUL_TYPES:
            continue
        lhs, rhs = only_child(n, "lhsp"), only_child(n, "rhsp")
        if lhs is None or rhs is None:
            continue
        lc, rc = is_constant_cone(lhs), is_constant_cone(rhs)
        inst = instances(n)
        lnode, ldecl, lhon, lhow = peel_extension(lhs, types, ext_funcs)
        rnode, rdecl, rhon, rhow = peel_extension(rhs, types, ext_funcs)
        rec = {
            "kind": "multiply",
            "signed": n.get("type") == "MULS",
            "instances": inst,
            "constantOperand": ("lhs" if lc else ("rhs" if rc else None)),
            "lhsDeclaredWidth": ldecl, "lhsHonestWidth": lhon, "lhsExtension": lhow,
            "rhsDeclaredWidth": rdecl, "rhsHonestWidth": rhon, "rhsExtension": rhow,
            "resultWidth": types.width(n),
            "chainDepth": expr_depth(n),
            "operandChainDepth": max(expr_depth(lhs), expr_depth(rhs)),
            "inClockedProcess": in_clocked(n),
            "insideFunction": owner.get(n.get("addr"), "@top") != "@top",
        }
        rec.update(S.loc(n))
        # serial multiply: another product inside an operand cone
        rec["operandContainsMultiply"] = any(
            x.get("type") in MUL_TYPES for side in (lhs, rhs) for x in walk(side))

        if lc or rc:
            # a constant multiplicand is shift-add, not a DSP -- unless the tool
            # infers one anyway. GOTCHAS 3: multstyle="logic" is silently
            # ignored, so this is reported, not assumed away.
            rec["severity"] = "GREEN"
            rec["reason"] = "one operand is a constant cone: shift-add, not a multiplier (verify against the map row -- GOTCHAS 3 records multstyle being ignored)"
        else:
            # ---- WIDTH, AND A RULE THAT WAS WRONG ON ITS FIRST DRAFT ------
            # The first version of this scanner flagged RED whenever an operand
            # carried extension above its honest width. That fires on the
            # CORRECT SystemVerilog idiom for a widening signed product --
            #
            #   mul32 = $signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b});
            #
            # -- which is how `zhao_geom_project` writes all nine of its matrix
            # products. It was refuted by measurement in the same session: that
            # module maps to 33 DSP blocks for 11 products, i.e. THREE each,
            # which is the 32x32 decomposition (`Two Independent 18x18` 22 +
            # `Sum of two 18x18` 11). A genuine 64x64 would be far more.
            # So Quartus 17.0.2 folds sign extension away before inferring the
            # multiplier, and a rule that called the idiom a defect would have
            # sent an implementer to rewrite nine correct lines.
            #
            # QUARTUS_GOTCHAS 5's real case is the OTHER shape: `zhao_geom_lod`
            # declared its operands 72 bits wide with nothing to peel, so the
            # honest width WAS 72 and the tool built for 72. That is what the
            # thresholds below key on -- honest width after peeling, never the
            # peel itself. `extensionPeeled` is reported as information.
            wid = max(x for x in (lhon or 0, rhon or 0, 1))
            rec["extensionPeeled"] = (lhow != "none") or (rhow != "none")
            rec["declaredWidthSlack"] = max((ldecl or 0) - (lhon or 0), (rdecl or 0) - (rhon or 0))
            if wid >= 40:
                rec["severity"] = "RED"
                rec["reason"] = ("nonconstant %dx%d product with no extension left to peel: the tool is being "
                                 "asked for a multiplier that wide. QUARTUS_GOTCHAS 5 measured the same source "
                                 "at 28 DSPs on 72-bit operands and 18 on 64-bit -- prove the width, then "
                                 "synthesise" % (lhon or 0, rhon or 0))
            elif wid >= 25:
                rec["severity"] = "ORANGE"
                rec["reason"] = ("nonconstant %dx%d product: beyond one 18x18, so it decomposes. Measured on "
                                 "this tool, a 32x32 signed product costs THREE DSP blocks, and "
                                 "design/budgets/dsp.md's operator count is only a lower bound here"
                                 % (lhon or 0, rhon or 0))
            elif wid >= 19:
                rec["severity"] = "YELLOW"
                rec["reason"] = "nonconstant %dx%d product, just past one DSP's native 18x18" % (lhon or 0, rhon or 0)
            else:
                rec["severity"] = "GREEN"
                rec["reason"] = "nonconstant %dx%d product, inside one 18x18" % (lhon or 0, rhon or 0)
        mul_records.append(rec)
        S.findings.append(rec)

    # ---- mux-before-multiply candidates ---------------------------------
    # RASTER.FRAGMENT's blend computes two products per channel for MUTUALLY
    # EXCLUSIVE modes.  Muxing the left operand before ONE multiplier halves
    # them.  Detected structurally: a COND whose two arms each contain a
    # multiply, and whose two multiplies share an operand signature.
    #
    # AND IT MUST FOLLOW NAMED SIGNALS, which the first draft did not.
    # A positive control caught this: the detector reported ZERO candidates
    # across all 91 modules, which is indistinguishable from "the repository
    # has none". A synthetic module in exactly the shape RASTER.BLEND's own
    # header describes its former self --
    #
    #   always_comb p_alpha  = (src_i - dst_i) * a_i;
    #   always_comb p_addmod = src_i * a_i;
    #   assign y_o = mode_i ? p_alpha : p_addmod;
    #
    # -- was also reported clean, because the multiplies are not INSIDE the
    # COND's arms. Each arm is a bare VARREF and the product is one level
    # behind it. Nobody writes two products inline in a ternary; they write
    # them into named signals, which is why a detector that only looks inside
    # the arms can never fire on real RTL.
    comb_driver = {}
    for n in walk(mnode):
        if n.get("type") not in ("ASSIGN", "ASSIGNW"):
            continue
        if n.get("addr") in clocked_nodes:
            continue
        lhs, rhs = only_child(n, "lhsp"), only_child(n, "rhsp")
        if lhs is None or rhs is None or lhs.get("type") != "VARREF":
            continue
        v = lhs.get("varp")
        if v in comb_driver:
            comb_driver[v] = None      # driven from several places; ambiguous
        else:
            comb_driver[v] = rhs

    def resolve(node, depth=0):
        """Follow a bare VARREF back to its combinational driver."""
        while depth < 4 and node is not None and node.get("type") == "VARREF":
            nxt = comb_driver.get(node.get("varp"))
            if nxt is None:
                break
            node, depth = nxt, depth + 1
        return node

    mux_candidates = []
    for n in walk(mnode):
        if n.get("type") != "COND":
            continue
        thenp, elsep = resolve(only_child(n, "thenp")), resolve(only_child(n, "elsep"))
        if thenp is None or elsep is None:
            continue
        tm = [x for x in walk(thenp) if x.get("type") in MUL_TYPES]
        em = [x for x in walk(elsep) if x.get("type") in MUL_TYPES]
        if not tm or not em:
            continue
        for a in tm:
            for b in em:
                sa = {canon(only_child(a, "lhsp"), types), canon(only_child(a, "rhsp"), types)}
                sb = {canon(only_child(b, "lhsp"), types), canon(only_child(b, "rhsp"), types)}
                shared = sa & sb
                if shared:
                    rec = {
                        "kind": "mux_before_multiply",
                        "severity": "ORANGE",
                        "reason": "two products under mutually exclusive arms of one condition share an operand; "
                                  "muxing the differing operand before a single multiplier removes one product",
                        "sharedOperands": len(shared),
                        "instances": instances(n),
                    }
                    rec.update(S.loc(n))
                    mux_candidates.append(rec)
                    S.findings.append(rec)
                    break
            else:
                continue
            break

    # =====================================================================
    # 2. VARIABLE SHIFTS, DIVISION, MODULO
    # =====================================================================
    shifts, divides = [], []
    for n in walk(mnode):
        t = n.get("type")
        if t in SHIFT_TYPES:
            amt = only_child(n, "rhsp")
            if amt is None or is_constant_cone(amt):
                continue
            w = types.width(n) or 0
            rec = {
                "kind": "variable_shift", "instances": instances(n),
                "dataWidth": w, "amountWidth": types.width(amt),
                "amountDepth": expr_depth(amt),
                "severity": "ORANGE" if w >= 32 else "YELLOW",
                "reason": "variable shift of a %d-bit value: a barrel shifter, log2(width) levels of muxing" % w,
            }
            rec.update(S.loc(n))
            shifts.append(rec)
            S.findings.append(rec)
        elif t in DIV_TYPES:
            den = only_child(n, "rhsp")
            num = only_child(n, "lhsp")
            constden = den is not None and is_constant_cone(den)
            nw = types.width(num) or 0
            sev = "RED" if (constden and nw > 64) else ("ORANGE" if constden else "RED")
            reason = ("division by a constant with a %d-bit numerator: QUARTUS_GOTCHAS 2 records lpm_divide "
                      "REFUSING numerators above 64 bits -- the block does not synthesise at all" % nw) \
                if (constden and nw > 64) else \
                ("division by a constant, %d-bit numerator: a multiply-and-shift, but Quartus infers lpm_divide" % nw
                 if constden else
                 "nonconstant %s: a full divider, one of the most expensive structures available" % t)
            rec = {"kind": "divide", "op": t, "instances": instances(n),
                   "numeratorWidth": nw, "constantDenominator": constden,
                   "severity": sev, "reason": reason}
            rec.update(S.loc(n))
            divides.append(rec)
            S.findings.append(rec)

    # =====================================================================
    # 3. SERIAL COMBINATIONAL LOOPS
    # =====================================================================
    loops = []
    for n in walk(mnode):
        if n.get("type") != "LOOP":
            continue
        comb = n.get("addr") in comb_nodes or owner.get(n.get("addr"), "@top") != "@top"
        body_ops = sum(1 for x in walk(n) if x.get("type") in ARITH)
        rec = {
            "kind": "serial_loop", "combinational": bool(comb),
            "instances": instances(n), "arithOpsInBody": body_ops,
            "unroll": n.get("unroll"),
            "severity": ("RED" if comb and body_ops >= 8 else
                         "ORANGE" if comb and body_ops >= 2 else
                         "YELLOW" if comb else "GREEN"),
            "reason": ("a loop in COMBINATIONAL context with %d arithmetic operators in its body -- "
                       "this is a serial chain, not one cycle" % body_ops) if comb else
                      "loop inside a clocked process; the trip is spent in registers, not in one path",
        }
        rec.update(S.loc(n))
        loops.append(rec)
        S.findings.append(rec)

    # =====================================================================
    # 4. WIDE ADD -> COMPARE -> SATURATE CHAINS
    # =====================================================================
    satchains = []
    for n in walk(mnode):
        if n.get("type") != "COND":
            continue
        cond = only_child(n, "condp")
        if cond is None:
            continue
        cmps = [x for x in walk(cond) if x.get("type") in CMP_TYPES]
        if not cmps:
            continue
        widest = max((types.width(only_child(c, "lhsp")) or 0) for c in cmps)
        adds = [x for x in walk(cond) if x.get("type") in ADD_TYPES
                and (types.width(x) or 0) >= 32]
        if widest < 32 or not adds:
            continue
        rec = {
            "kind": "sat_chain", "compareWidth": widest,
            "wideAddsInCondition": len(adds), "instances": instances(n),
            "depth": expr_depth(n),
            "severity": "RED" if widest >= 64 else "ORANGE",
            "reason": "a %d-bit add feeds a %d-bit magnitude compare feeds a select, all combinational: "
                      "the add's carry must settle before the compare starts" % (
                          max((types.width(a) or 0) for a in adds), widest),
        }
        rec.update(S.loc(n))
        satchains.append(rec)
        S.findings.append(rec)

    # =====================================================================
    # 5. DUPLICATED EXPENSIVE EXPRESSIONS
    # =====================================================================
    # SURFACE.STAMP computed ONE 66-bit rescale twice, the second time only to
    # obtain a ledger bit.  Field does the same with resc_s / resc_s_fired.
    sig_count, sig_node = Counter(), {}
    for n in walk(mnode):
        t = n.get("type")
        if t in ("CONST", "VARREF", "ARRAYSEL", "SEL"):
            continue
        w = types.width(n) or 0
        if w < 24 and not is_expensive(n):
            continue
        if expr_depth(n) < 3:
            continue
        if not is_expensive(n):
            continue
        s = canon(n, types)
        if len(s) < 40:
            continue
        sig_count[s] += 1
        sig_node.setdefault(s, n)
    duplicates = []
    seen_dup = set()
    for s, c in sig_count.items():
        if c < 2:
            continue
        n = sig_node[s]
        key = (loc_line(n), types.width(n), c)
        if key in seen_dup:
            continue
        seen_dup.add(key)
        w = types.width(n) or 0
        rec = {
            "kind": "duplicate_expression", "occurrences": c, "width": w,
            "depth": expr_depth(n), "instances": instances(n),
            "severity": "RED" if w >= 48 else "ORANGE",
            "reason": "an identical %d-bit expensive expression is computed %d times; "
                      "SURFACE.STAMP's second 66-bit rescale existed only to set a ledger bit" % (w, c),
        }
        rec.update(S.loc(n))
        duplicates.append(rec)
        S.findings.append(rec)

    # =====================================================================
    # 6. ARRAYS
    # =====================================================================
    arrays = []
    vars_by_addr = {}
    for n in walk(mnode):
        if n.get("type") == "VAR":
            vars_by_addr[n["addr"]] = n

    # index every ARRAYSEL by the VAR it reads/writes
    sel_by_var = defaultdict(list)
    for n in walk(mnode):
        if n.get("type") != "ARRAYSEL":
            continue
        base = only_child(n, "fromp")
        while base is not None and base.get("type") in ("ARRAYSEL", "SEL"):
            base = only_child(base, "fromp")
        if base is not None and base.get("type") == "VARREF":
            sel_by_var[base.get("varp")].append((n, base))

    # ---- PARTIAL WRITES: GOTCHAS section 10's THIRD killer -----------------
    # This scanner had NO byte-enable detector at all, in 1,577 lines, while
    # section 10 names it as one of the three independent killers of storage
    # inference and calls it "the one most likely to be written by accident".
    #
    # The cost of the gap, measured: zhao_surface_sheet asked for 131,072 bits,
    # inferred NONE, and spent an estimated 95,947 ALMs -- 229 % of the device,
    # the largest single resource item in the repository. Its array was read
    # synchronously and was not reset, so this scanner reported it YELLOW and
    # `expectedStorage: RAM`, i.e. HEALTHY. The one thing wrong with it was
    #
    #     if (be[1]) mem[a][15:8] <= d[15:8];
    #
    # a nonblocking assignment whose LHS is a SEL over an ARRAYSEL -- which is
    # exactly what is matched below. Same shape found in zhao_forge_cliff:
    # `edge_mem_r[mhead_r][5:0] <= mtake_r` is the only structural difference
    # between the one of its three tables that did not infer and the two that
    # did.
    #
    # A partial write is only a killer for something that WANTS to be memory; a
    # bitfield in a small register bank is fine. The severity below is gated on
    # `expects_ram` for that reason.
    partial_writes = defaultdict(int)
    for n in walk(mnode):
        if n.get("type") not in ("ASSIGN", "ASSIGNDLY"):
            continue
        lhs = only_child(n, "lhsp")
        if lhs is None or lhs.get("type") != "SEL":
            continue
        inner = only_child(lhs, "fromp")
        if inner is None or inner.get("type") != "ARRAYSEL":
            continue
        base = only_child(inner, "fromp")
        while base is not None and base.get("type") in ("ARRAYSEL", "SEL"):
            base = only_child(base, "fromp")
        if base is not None and base.get("type") == "VARREF":
            partial_writes[base.get("varp")] += 1

    # which VARs are written inside a reset branch
    reset_written = set()
    for n in walk(mnode):
        if n.get("type") != "IF":
            continue
        cond = only_child(n, "condp")
        if cond is None:
            continue
        names = [x.get("name", "") for x in walk(cond) if x.get("type") == "VARREF"]
        if not any(RESET_RE.search(nm or "") for nm in names):
            continue
        # ONE BRANCH OF THE IF, not both.
        #
        # This walked `n` -- the entire IF node, ELSE included, i.e. the block's
        # normal operating logic. So every array written in any
        # `always_ff @(posedge clk or negedge rst_n)` reported
        # `resetTouched: true` whether the reset touched it or not.
        #
        # Caught RUN-20260824-0317 on zhao_forge_cliff: all three of its tables
        # were reported reset-touched, and the 43 lines after `if (!rst_n)`
        # assign thirty-one scalars and not one array element. This field is how
        # an agent checks GOTCHAS section 10's second killer, so a false
        # positive sends someone to fix a reset that is not there -- the same
        # shape as the widening-multiply rule that would have sent an
        # implementer to rewrite nine correct lines.
        #
        # WHICH BRANCH IS THE RESET BRANCH CANNOT BE READ OFF THE POLARITY.
        # Every block in this tree writes `if (!rst_n) <reset> else <work>`, and
        # Verilator's elaborated AST folds the `!` away by SWAPPING the arms:
        # the condition arrives as a bare `VARREF rst_n` with the WORK in
        # `thensp` and the RESET in `elsesp`. Taking `thensp` is therefore
        # exactly as wrong as taking both, and wrong in the more flattering
        # direction. Nor does the name settle it -- `rst_n` reads active-low and
        # `rst` active-high, and a scanner that guesses from a name is the
        # detector-picked-by-name failure this repository has already disclosed.
        #
        # So the branch is identified by what a reset branch IS: it drives
        # things to KNOWN VALUES, so every right-hand side in it is constant.
        # The working branch is full of VARREFs. That is polarity-independent
        # and frontend-independent. When the test does not separate the two
        # arms, BOTH are taken -- the old, over-broad behaviour, which is the
        # safe direction for a guard.
        def _assigns(branch_key):
            out = []
            for b in (n.get(branch_key) or []):
                for x in walk(b):
                    if x.get("type") in ("ASSIGN", "ASSIGNDLY"):
                        out.append(x)
            return out

        def _all_const_rhs(assigns):
            if not assigns:
                return False
            for x in assigns:
                rhs = only_child(x, "rhsp")
                if rhs is None or not is_constant_cone(rhs):
                    return False
            return True

        then_a, else_a = _assigns("thensp"), _assigns("elsesp")
        then_c, else_c = _all_const_rhs(then_a), _all_const_rhs(else_a)
        if then_c and not else_c:
            branch_assigns = then_a
        elif else_c and not then_c:
            branch_assigns = else_a
        else:
            branch_assigns = then_a + else_a

        for x in branch_assigns:
            lhs = only_child(x, "lhsp")
            for y in walk(lhs) if lhs is not None else []:
                if y.get("type") == "VARREF":
                    reset_written.add(y.get("varp"))

    for addr, v in vars_by_addr.items():
        info = types.info(v.get("dtypep"))
        elems = info.get("elems")
        ew = info.get("elemWidth")
        total = info.get("width")
        if info.get("kind") != "UNPACKARRAYDTYPE" or not elems or not ew:
            continue
        if total is None:
            total = elems * ew
        sels = sel_by_var.get(addr, [])
        async_reads, sync_reads, writes, dynamic = 0, 0, 0, 0
        for sel, ref in sels:
            idx = only_child(sel, "bitp")
            if idx is not None and not is_constant_cone(idx):
                dynamic += 1
            if ref.get("access") == "WR" or ref.get("access") == "RW":
                writes += 1
            elif sel.get("addr") in clocked_nodes:
                sync_reads += 1
            else:
                async_reads += 1
        # ---- IS THIS STORAGE, OR IS IT A PIPELINE? ------------------------
        # An array whose every index is a CONSTANT is not a memory that failed
        # to infer -- it is a bank of pipeline registers or an unrolled vector,
        # and it was always going to be flops. `zhao_geom_project`'s
        # `dstep_dv[0:31][0:2]` is 6,048 bits of divider pipeline, one stage
        # per index, and calling it an uninferred RAM would be a false alarm
        # loud enough to bury the real ones.
        #
        # The first draft of this rule keyed on size alone and flagged three
        # such pipelines RED in that one module. Storage is DYNAMICALLY
        # ADDRESSED and deep; a two-entry config bank selected by a view bit is
        # a mux however many bits it holds.
        #
        # AND ONE MORE DISCRIMINATOR, ADDED AFTER THE FIRST RUN FLAGGED BOTH
        # PROJECTORS. `dstep_dv[0:31][0:2]` is a 32-stage divider pipeline
        # written from a genvar loop, and two of its reads use a loop variable
        # Verilator had not yet folded -- so `dynamic > 0` was true and 6,048
        # bits of pipeline registered as an uninferred memory in the two
        # largest rows on the board. Wrong twice over: the alarm is false, and
        # it sits exactly where a real one would be missed.
        #
        # ACCESS SITES PER ELEMENT separates them cleanly. A memory has O(1)
        # access sites however deep it is; a pipeline has one write and one
        # read PER STAGE. Measured on this repo:
        #
        #   zhao_field_seq   rf          64 elems,   11 sites   -> memory
        #   zhao_forge_cliff prio_mem_r  2048 elems,  2 sites   -> memory
        #   pose_cache       tags        128 elems,   2 sites   -> memory
        #   geom_project     dstep_dv    32 elems,  754 sites   -> pipeline
        sites = async_reads + sync_reads + writes
        addressable = dynamic > 0 and elems >= 32 and sites <= elems
        expects_ram = addressable and total >= 512
        rec_sites = sites
        rec = {
            "kind": "array", "name": v.get("origName") or v.get("name"),
            "elements": elems, "elementWidth": ew, "totalBits": total,
            "asyncReadSites": async_reads, "syncReadSites": sync_reads,
            "writeSites": writes, "dynamicIndexSites": dynamic,
            "accessSites": rec_sites, "sitesPerElement": round(rec_sites / elems, 2),
            "dynamicallyAddressed": bool(dynamic),
            "resetTouched": addr in reset_written,
            "partialWriteSites": partial_writes.get(addr, 0),
            "expectedStorage": ("RAM" if expects_ram else "registers"),
        }
        rec.update(S.loc(v))
        if expects_ram and async_reads:
            rec["severity"] = "RED"
            rec["reason"] = ("%d addressable bits (%d x %d) read ASYNCHRONOUSLY. A Cyclone V M10K has a "
                             "SYNCHRONOUS read port, so this cannot infer as memory and becomes flops behind a "
                             "%d:1 mux. zhao_field_seq spends 8,901 ALMs and ZERO M10Ks exactly this way while "
                             "502 block memories sit idle" % (total, elems, ew, elems))
        elif expects_ram and rec["partialWriteSites"]:
            rec["severity"] = "RED"
            rec["reason"] = ("%d addressable bits written with %d PARTIAL (bit-select) write(s) -- byte "
                             "enables. Quartus 17.0.2 Lite does not infer M10K byte-enable support from "
                             "that template and the memory disappears entirely: measured 0 memory bits and "
                             "45,134 ALMs for a 65,536-bit buffer. Split the word into one array per "
                             "independently-written field, or instantiate altsyncram" %
                             (total, rec["partialWriteSites"]))
        elif expects_ram and rec["resetTouched"]:
            rec["severity"] = "RED"
            rec["reason"] = ("%d addressable bits written from a reset branch. A reset that touches every "
                             "element forbids RAM inference; M10K contents are undefined after reset, so a "
                             "valid bitmap is the answer and walking the array is not" % total)
        elif expects_ram and dynamic > 2 and (async_reads + sync_reads) > 2:
            rec["severity"] = "ORANGE"
            rec["reason"] = ("%d addressable bits with %d dynamically indexed access sites: more ports than a "
                             "two-port M10K offers, so some accesses are rebuilt in logic or the array is "
                             "replicated" % (total, dynamic))
        elif expects_ram:
            rec["severity"] = "YELLOW"
            rec["reason"] = ("%d addressable bits; expects RAM inference. Confirm blockMemoryBits > 0 in the "
                             "map row -- a passing test is not evidence that an array became a memory" % total)
        elif total >= 2048 and sites > elems:
            rec["severity"] = "YELLOW"
            rec["reason"] = ("%d bits across %d access sites for %d elements -- %0.1f sites per element, so "
                             "this is a PIPELINE or unrolled vector rather than a memory that failed to "
                             "infer. It is still %d flops and belongs in the block's own budget"
                             % (total, sites, elems, sites / elems, total))
        elif total >= 2048 and not dynamic:
            rec["severity"] = "YELLOW"
            rec["reason"] = ("%d bits of CONSTANT-indexed array: a pipeline or unrolled vector, so flops by "
                             "construction rather than an uninferred memory -- but %d flops is a real cost and "
                             "belongs in the block's own budget" % (total, total))
        else:
            rec["severity"] = "GREEN"
            rec["reason"] = ("%d bits, %s" % (total, "constant-indexed: registers by construction"
                                              if not dynamic else "too shallow to be worth an M10K"))
        arrays.append(rec)
        S.findings.append(rec)

    # =====================================================================
    # 6b. CONSTANT CASE TREES -- MEMORIES DESCRIBED AS LOGIC
    # =====================================================================
    # The docket's failure class 3 names these explicitly: Field holds a
    # 256x16 reciprocal seed table, a 256x31 normalisation table and a 257x17
    # sine table INSTANTIATED TWICE, all as `always_comb` case trees. None of
    # them is an array, so the array pass above cannot see any of them -- and
    # the block reports ramBlocks = 0 while spending 8,901 ALMs with 502 M10Ks
    # idle.
    #
    # A case tree over constants is a ROM whichever way it is written. It is
    # found here by counting the distinct wide constants that a single
    # combinational assignment can produce; a mux tree that selects among 256
    # of them is 256 words of storage built out of LUTs.
    # NOTE ON HOW THIS IS KEYED, because the obvious way does not work.
    # A first attempt looked for ONE assignment whose right-hand side selects
    # among many constants. It found nothing at all in `zhao_field_rcp_rom`,
    # which is 256 lines of `8'dN : seed_o = 16'hXXXX;` -- because Verilator
    # lowers a `case` into a chain of separate IF/ASSIGN statements, so no
    # single assignment ever holds more than one constant. The tables are
    # therefore counted PER TARGET VARIABLE, across every combinational
    # assignment in the module, which is what a ROM is regardless of the
    # syntax it was written in.
    comb_consts = defaultdict(set)
    comb_site = {}
    for n in walk(mnode):
        if n.get("type") not in ("ASSIGN", "ASSIGNW"):
            continue
        if n.get("addr") in clocked_nodes:
            continue
        lhs, rhs = only_child(n, "lhsp"), only_child(n, "rhsp")
        if lhs is None or rhs is None:
            continue
        tgts = {y.get("varp") for y in walk(lhs) if y.get("type") == "VARREF"}
        if len(tgts) != 1:
            continue
        tgt = next(iter(tgts))
        w = types.width(lhs) or 0
        if w < 4:
            continue
        for x in walk(rhs):
            if x.get("type") == "CONST" and (types.width(x) or 0) >= 4:
                comb_consts[tgt].add(x.get("name"))
        comb_site.setdefault(tgt, n)

    const_roms = []
    for tgt, consts in comb_consts.items():
        if len(consts) < 32:
            continue
        v = vars_by_addr.get(tgt)
        w = (types.width(v) or 0) if v is not None else 0
        if w < 4:
            continue
        n = comb_site[tgt]
        bits = len(consts) * w
        rec = {
            "kind": "const_rom",
            "target": (v.get("origName") or v.get("name")) if v is not None else None,
            "distinctConstants": len(consts), "outputWidth": w,
            "impliedBits": bits, "instances": instances(n),
            "severity": "RED" if bits >= 2048 else "ORANGE",
            "reason": ("a combinational select over %d distinct %d-bit constants -- %d bits of ROM built from "
                       "LUTs. This is not an array, so no array check can see it; Field holds three such "
                       "tables and reports ramBlocks = 0 while 502 M10Ks sit idle" % (len(consts), w, bits)),
        }
        rec.update(S.loc(n))
        const_roms.append(rec)
        S.findings.append(rec)

    # =====================================================================
    # 7. INTERFACE SHAPE AND INFERRED INITIATION INTERVAL
    # =====================================================================
    ports = [v for v in vars_by_addr.values() if v.get("isPrimaryIO")]
    inputs = {v["addr"] for v in ports if v.get("direction") == "INPUT"}
    outputs = [v for v in ports if v.get("direction") == "OUTPUT"]

    # direct input -> arithmetic -> output, with no register in between
    direct_paths = []
    for n in walk(mnode):
        if n.get("type") != "ASSIGNW":
            continue
        lhs, rhs = only_child(n, "lhsp"), only_child(n, "rhsp")
        if lhs is None or rhs is None:
            continue
        tgt = [y.get("varp") for y in walk(lhs) if y.get("type") == "VARREF"]
        if not any(t in {o["addr"] for o in outputs} for t in tgt):
            continue
        srcs = {y.get("varp") for y in walk(rhs) if y.get("type") == "VARREF"}
        if not (srcs & inputs):
            continue
        ops = sum(1 for x in walk(rhs) if x.get("type") in ARITH)
        if ops == 0:
            continue
        rec = {"kind": "direct_io_path", "arithOps": ops, "depth": expr_depth(rhs),
               "severity": "RED" if ops >= 8 else ("ORANGE" if ops >= 3 else "YELLOW"),
               "reason": "an output is driven combinationally from an input through %d arithmetic operators. "
                         "QUARTUS_GOTCHAS 9: without set_input_delay/set_output_delay this path is not timed "
                         "at all, which is how a 21 ns cone reported 199.72 MHz" % ops}
        rec.update(S.loc(n))
        direct_paths.append(rec)
        S.findings.append(rec)

    # ---- FSM: state variable, state count, transition graph -------------
    # ---- FINDING THE STATE VARIABLE BY BEHAVIOUR, NOT BY NAME ------------
    # The first draft picked the widest-enum or best-named candidate and, on
    # `zhao_texture_tmu`, chose `ST_IDLE` -- a LOCALPARAM whose name matches
    # any `st_*` pattern -- instead of `st_r`, the register that actually
    # holds the state. It then reported "0 distinct state constants" and
    # II >= 1 for a block whose MEASURED II is 6.
    #
    # So the candidate must be something a clocked process ASSIGNS SEVERAL
    # DISTINCT CONSTANTS TO. That is what a state register is, whether it was
    # declared as an enum, a localparam-coded vector, or anything else; and
    # LPARAM/GPARAM/GENVAR are excluded outright because a parameter is not a
    # register however it is named.
    assigned_consts = defaultdict(set)
    for n in walk(mnode):
        if n.get("type") not in ("ASSIGN", "ASSIGNDLY"):
            continue
        lhs, rhs = only_child(n, "lhsp"), only_child(n, "rhsp")
        if lhs is None or rhs is None:
            continue
        tgts = {y.get("varp") for y in walk(lhs) if y.get("type") == "VARREF"}
        if len(tgts) != 1:
            continue
        tgt = next(iter(tgts))
        if rhs.get("type") == "CONST":
            assigned_consts[tgt].add(rhs.get("name"))

    fsm = None
    state_vars = []
    for addr, v in vars_by_addr.items():
        if v.get("varType") not in ("VAR",) or v.get("isPrimaryIO"):
            continue
        info = types.info(v.get("dtypep"))
        w = info.get("width") or 0
        if info.get("kind") == "UNPACKARRAYDTYPE" or w > 8:
            continue
        consts = assigned_consts.get(addr, set())
        if len(consts) < 2:
            continue
        nm = v.get("origName") or v.get("name") or ""
        score = (len(consts), 1 if STATE_RE.search(nm) else 0,
                 info.get("items", 0) if info.get("kind") == "ENUMDTYPE" else 0)
        state_vars.append((v, info.get("items"), score))
    if state_vars:
        v, nstates, _score = max(state_vars, key=lambda x: x[2])
        # ---- HOW MANY STATES, AND HOW THE FIRST DRAFT GOT THIS WRONG -----
        # State count is taken from the DISTINCT CONSTANTS assigned to the
        # state variable, not from the names in the source. `zhao_texture_tmu`
        # writes `localparam logic [2:0] ST_IDLE = 3'd0;` -- a localparam, not
        # an enum -- so after elaboration the name ST_IDLE does not exist and a
        # detector looking for the string "IDLE" finds nothing. That is exactly
        # what the first draft did, and it reported II >= 1 for the block whose
        # measured II is 6.
        state_consts = set()
        for n in walk(mnode):
            if n.get("type") not in ("ASSIGN", "ASSIGNDLY"):
                continue
            lhs = only_child(n, "lhsp")
            if lhs is None:
                continue
            if not any(y.get("varp") == v["addr"] for y in walk(lhs) if y.get("type") == "VARREF"):
                continue
            rhs = only_child(n, "rhsp")
            if rhs is None:
                continue
            if rhs.get("type") == "CONST":
                state_consts.add(rhs.get("name"))
            else:
                for y in walk(rhs):
                    if y.get("type") == "CONST":
                        state_consts.add(y.get("name"))

        # ---- IS ACCEPTANCE GATED ON ONE STATE? --------------------------
        # The signal is structural and name-independent: an output whose name
        # says "ready" is driven by an equality against the state variable.
        # `assign req_ready_o = (st_r == ST_IDLE);` is the canonical shape and
        # it means at most one record is ever in flight.
        state_gated_ready = False
        gated_signal = None
        for n in walk(mnode):
            if n.get("type") not in ("ASSIGNW", "ASSIGN"):
                continue
            lhs = only_child(n, "lhsp")
            rhs = only_child(n, "rhsp")
            if lhs is None or rhs is None:
                continue
            lnames = [y.get("name", "") for y in walk(lhs) if y.get("type") == "VARREF"]
            if not any(READY_RE.search(x or "") for x in lnames):
                continue
            for eq in walk(rhs):
                if eq.get("type") not in ("EQ", "NEQ"):
                    continue
                sides = [only_child(eq, "lhsp"), only_child(eq, "rhsp")]
                refs_state = any(
                    s is not None and any(y.get("varp") == v["addr"]
                                          for y in walk(s) if y.get("type") == "VARREF")
                    for s in sides)
                has_const = any(s is not None and is_constant_cone(s) for s in sides)
                if refs_state and has_const:
                    state_gated_ready = True
                    gated_signal = next((x for x in lnames if READY_RE.search(x or "")), None)
                    break
            if state_gated_ready:
                break

        # II: the state walk cannot be shorter than the number of states it
        # visits, and a ready gated on one state means the next record waits
        # for the whole walk. So this is a LOWER bound on II -- TEXTURE.TMU
        # infers 5 here against a MEASURED 6, which is the right direction.
        inferred_ii = max(1, len(state_consts)) if state_gated_ready else 1
        fsm = {
            "kind": "fsm", "stateVar": v.get("origName") or v.get("name"),
            "declaredStates": nstates, "distinctStateConstants": len(state_consts),
            "readyGatedOnSingleState": state_gated_ready,
            "gatedSignal": gated_signal,
            "inferredMinII": inferred_ii,
        }
        fsm.update(S.loc(v))
        # SEVERITY STOPS AT ORANGE HERE, DELIBERATELY, AND THE FIRST DRAFT
        # GOT THIS WRONG. Rating a gated multi-state walk RED from source alone
        # turned 13 modules RED on this rule and nothing else -- including
        # `zhao_sdram_ctrl`, where a sixteen-state walk IS the design, and
        # `zhao_surface_stamp`, which was rearchitected to 0 DSPs and 87.54 MHz
        # this afternoon. 42 of 91 modules RED is not a heatmap, it is a
        # uniform colour.
        #
        # A long II is a FACT. Whether it is a DEFECT depends on items/frame,
        # which lives in design/budgets/workloads.yml and which this scanner
        # deliberately does not read. So the fact is reported here at ORANGE
        # with the number attached, and tools/budget/build_manifest.py
        # escalates to RED where a demand figure proves it. TEXTURE.TMU still
        # comes out RED, via NO_RESERVE at 3.06x, which is a measured rate
        # problem rather than a shape that resembles one.
        if state_gated_ready and inferred_ii >= 4:
            fsm["severity"] = "ORANGE"
            fsm["reason"] = ("`%s` is asserted only while the state variable equals one constant, and the walk "
                             "visits %d distinct states -- so no new record can enter for at least that many "
                             "clocks whatever the clock rate. TEXTURE.TMU is exactly this shape: II=6 against a "
                             "demand needing II=1, and NOTHING in its suite had ever measured the rate"
                             % (gated_signal or "ready", inferred_ii))
        elif state_gated_ready:
            fsm["severity"] = "ORANGE"
            fsm["reason"] = ("`%s` is gated on a single state: one record in flight at a time, whatever the "
                             "latency" % (gated_signal or "ready"))
        else:
            fsm["severity"] = "GREEN"
            fsm["reason"] = "acceptance is not gated on a single state constant"
        S.findings.append(fsm)

    # =====================================================================
    # 8. COUNTERS GATED BY DEEP COMBINATIONAL LOGIC
    # =====================================================================
    counters = []
    for n in walk(mnode):
        if n.get("type") != "IF":
            continue
        cond = only_child(n, "condp")
        if cond is None:
            continue
        d = expr_depth(cond)
        if d < 5:
            continue
        for x in walk(n):
            if x.get("type") not in ("ASSIGN", "ASSIGNDLY"):
                continue
            rhs = only_child(x, "rhsp")
            lhs = only_child(x, "lhsp")
            if rhs is None or lhs is None or rhs.get("type") not in ADD_TYPES:
                continue
            lv = {y.get("varp") for y in walk(lhs) if y.get("type") == "VARREF"}
            rv = {y.get("varp") for y in walk(rhs) if y.get("type") == "VARREF"}
            if not (lv & rv):
                continue
            rec = {"kind": "deep_gated_counter", "conditionDepth": d,
                   "width": types.width(lhs),
                   "severity": "ORANGE" if d >= 8 else "YELLOW",
                   "reason": "a counter's enable is %d operators deep; the enable, not the carry chain, "
                             "sets this register's arrival time" % d}
            rec.update(S.loc(x))
            counters.append(rec)
            S.findings.append(rec)
            break

    # =====================================================================
    # summary
    # =====================================================================
    eff_mul = sum(r["instances"] for r in mul_records if r["constantOperand"] is None)
    const_mul = sum(r["instances"] for r in mul_records if r["constantOperand"] is not None)
    sev = "GREEN"
    for f in S.findings:
        sev = sev_max(sev, f.get("severity", "GREEN"))

    return {
        "module": name,
        "severity": sev,
        "arithmetic": {
            "multiplyOperatorsWritten": len(mul_records),
            "nonconstantMultiplyInstances": eff_mul,
            "constantMultiplyInstances": const_mul,
            "widestNonconstantOperand": max(
                [max(r["lhsHonestWidth"] or 0, r["rhsHonestWidth"] or 0)
                 for r in mul_records if r["constantOperand"] is None] or [0]),
            "signedNonconstant": sum(r["instances"] for r in mul_records
                                     if r["constantOperand"] is None and r["signed"]),
            "muxBeforeMultiplyCandidates": len(mux_candidates),
            "variableShifts": sum(r["instances"] for r in shifts),
            "divides": sum(r["instances"] for r in divides),
            "combinationalLoops": sum(1 for r in loops if r["combinational"]),
            "satChains": len(satchains),
            "duplicateExpressions": len(duplicates),
        },
        "storage": {
            "arrays": len(arrays),
            "totalArrayBits": sum(a["totalBits"] for a in arrays),
            "arraysExpectingRam": sum(1 for a in arrays if a["expectedStorage"] == "RAM"),
            "asyncReadArrays": sum(1 for a in arrays if a["asyncReadSites"] > 0),
            "addressableArrays": sum(1 for a in arrays if a["expectedStorage"] == "RAM"),
            "addressableBits": sum(a["totalBits"] for a in arrays if a["expectedStorage"] == "RAM"),
            "resetTouchedArrays": sum(1 for a in arrays if a["resetTouched"]),
            "constRomTables": len(const_roms),
            "constRomBits": sum(r["impliedBits"] * r["instances"] for r in const_roms),
        },
        "interface": {
            "ports": len(ports),
            "directIoArithmeticPaths": len(direct_paths),
            "fsm": fsm,
            "inferredMinII": (fsm or {}).get("inferredMinII", 1),
        },
        "functions": {
            "count": len(funcs),
            "instancesByName": {funcs[a].get("name"): call_counts.get(a, 0) for a in funcs},
            "repeatedExpensiveCalls": sorted(
                [{"function": funcs[a].get("name"), "instances": call_counts.get(a, 0),
                  "multipliesInBody": sum(1 for x in walk(funcs[a]) if x.get("type") in MUL_TYPES)}
                 for a in funcs
                 if call_counts.get(a, 0) >= 2
                 and any(x.get("type") in MUL_TYPES | DIV_TYPES for x in walk(funcs[a]))],
                key=lambda d: -d["instances"]),
        },
        "findings": S.findings,
    }


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--modules", nargs="*", help="module names; default all")
    ap.add_argument("--out", default=os.path.join(REPO, "reports", "rtl_inventory.json"))
    ap.add_argument("--workdir", default=os.path.join(REPO, "build-budget", "ast"))
    ap.add_argument("--keep-trees", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(VERILATOR):
        sys.exit("verilator_bin not found at %s -- dot-source tools/env/zhao-env.ps1 "
                 "or set ZHAO_VERILATOR" % VERILATOR)

    mods = discover_modules()
    names = args.modules or sorted(mods)
    sources = ordered_sources()
    src_hash = hashlib.sha256("\n".join(
        os.path.relpath(s, REPO).replace(os.sep, "/") for s in sources).encode()).hexdigest()[:16]

    head = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                          capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", REPO, "-c", "core.autocrlf=true",
                            "status", "--porcelain", "--", "fpga/rtl"],
                           capture_output=True, text=True).stdout.strip()

    records, failures = [], []
    for i, name in enumerate(names, 1):
        if name not in mods:
            failures.append({"module": name, "error": "not found in fpga/rtl"})
            continue
        outdir = os.path.join(args.workdir, name)
        tree, meta, err = elaborate(name, outdir, sources)
        if tree is None:
            failures.append({"module": name, "error": "verilator did not elaborate", "detail": err})
            if not args.quiet:
                print("[%d/%d] %-32s ELABORATION FAILED" % (i, len(names), name))
            continue
        root = json.load(open(tree, encoding="utf-8", errors="replace"))
        filemap = read_meta_files(meta)
        types = Types(root)
        mnodes = collect_module_nodes(root)
        mnode = mnodes.get(name)
        if mnode is None:
            failures.append({"module": name, "error": "top module absent from elaborated tree"})
            continue
        rec = analyse_module(name, mnode, types, filemap)
        rec["sourceFile"] = os.path.relpath(mods[name], REPO).replace(os.sep, "/")

        # ---- the cone, not just the file ---------------------------------
        counts, direct = instance_counts(root, name)
        sub = {}
        for sname, snode in mnodes.items():
            if sname == name or counts.get(sname, 0) == 0:
                continue
            sub[sname] = analyse_module(sname, snode, types, filemap)
        rec["instances"] = {k: v for k, v in counts.items() if k != name}
        rec["submodules"] = sorted(sub)
        agg_a = dict(rec["arithmetic"])
        agg_s = dict(rec["storage"])
        hier_sev = rec["severity"]
        for sname, srec in sub.items():
            k = counts[sname]
            for key, val in srec["arithmetic"].items():
                if key == "widestNonconstantOperand":
                    agg_a[key] = max(agg_a[key], val)
                else:
                    agg_a[key] = agg_a.get(key, 0) + val * k
            for key, val in srec["storage"].items():
                agg_s[key] = agg_s.get(key, 0) + val * k
            hier_sev = sev_max(hier_sev, srec["severity"])
        rec["hierarchical"] = {
            "arithmetic": agg_a, "storage": agg_s, "severity": hier_sev,
            "note": "own module plus every instance of every submodule in its cone, "
                    "weighted by instance count from the elaborated CELL graph",
        }
        rec["severity"] = hier_sev
        rec["submoduleFindings"] = [
            dict(f, module=sname, instancesOfModule=counts[sname])
            for sname, srec in sub.items() for f in srec["findings"]
            if f.get("severity") in ("RED", "ORANGE")
        ]
        records.append(rec)
        if not args.quiet:
            a = rec["hierarchical"]["arithmetic"]
            print("[%d/%d] %-32s %-7s mul=%-4d shift=%-3d div=%-2d arrays=%-3d II>=%d %s"
                  % (i, len(names), name, rec["severity"],
                     a["nonconstantMultiplyInstances"], a["variableShifts"], a["divides"],
                     rec["hierarchical"]["storage"]["arrays"], rec["interface"]["inferredMinII"],
                     ("+%d sub" % len(sub)) if sub else ""))
        if not args.keep_trees:
            for fn in (tree, meta):
                try:
                    os.remove(fn)
                except OSError:
                    pass

    out = {
        "schemaVersion": 1,
        "tool": "tools/budget/scan_rtl.py",
        "frontend": "Verilator --json-only (elaborated AST)",
        "sourceCommit": head,
        "rtlCleanAtHead": (dirty == ""),
        "sourceListHash": src_hash,
        "moduleCount": len(records),
        "modules": records,
        "failures": failures,
        "limitations": [
            "This is SOURCE ANALYSIS. It infers no resources. A multiply's DSP cost is decided by Quartus and depends discontinuously on operand width and signedness (design/budgets/dsp.md, corrected 2026-08-23).",
            "chainDepth and depth are AST operator depth, not delay. Nothing here is a timing measurement.",
            "inferredMinII is derived from the state-transition graph and ready gating. It is an upper bound on throughput, never a measurement; a block whose rate matters still needs an executable II test.",
            "Module discovery is textual; module ANALYSIS is not. A missed module appears as a missing record, not as a wrong number.",
        ],
    }
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(out, fh, indent=1)
        fh.write("\n")
    print("WROTE %s (%d module(s), %d failure(s))" % (args.out, len(records), len(failures)))


if __name__ == "__main__":
    main()
