#!/usr/bin/env python3
r"""Fail if the TEXTURE ISLAND V3 transaction file is not physically what
section 6 of the V3 architecture declares it to be.

WHY THIS IS A GATE AND NOT A GREP
---------------------------------
`reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt`, section 0, last
paragraph, states the exact failure this file exists to prevent:

    "The repository already contains earlier advice about RAMs and tokens.
     This specification makes that advice concrete enough that the next agent
     cannot implement another large asynchronous table, call it a transaction
     file, and declare the architecture complete."

Prose cannot enforce that. The previous island already proved it: D19m found
`zhao_texture_tmu_pipe` synthesising 72,824 REGISTERS against 256 block-memory
bits, because two arrays that everybody called a cache were read
combinationally on several ports. Every comment in that file said "cache".
The fitter said flip-flops. The comment is not the machine.

So the declared bank table is transcribed here AS DATA and compared against
what the RTL actually declares and actually does. A bank that is one bit narrow,
read from an `assign`, written from two addresses, or consumed through an adder
before its first register is reported by name, with its line, and the gate
goes red.

WHAT IT CHECKS  (each maps to a numbered spec obligation)
---------------------------------------------------------
 1. EXISTENCE / GEOMETRY  -- every bank in the section 6 table exists with the
    declared DEPTH x WIDTH.                          [sec 6, table]
 2. SYNCHRONOUS           -- every bank read happens inside a clocked process,
    never in `assign` / `always_comb` / a function.  [LAW 08, sec 6.3]
    Its payload process is clock-only; a payload array in a reset branch is a
    separate, named offence.                         [LAW 08, sec 6.5]
 3. ONE WRITER, ONE READER-- exactly one distinct write address expression and
    one distinct read address expression, in exactly one clocked process each.
    A second write port is the multiwrite payload state the experiment must
    not introduce.                                   [sec 6 table, LAW 07]
 4. NO LOGIC BEFORE THE FIRST REGISTER -- the right-hand side of the capturing
    assignment is the array read and NOTHING ELSE. An adder, a ternary, a
    concat or a part-select between the array output and the flop is what turns
    a declared M10K into fabric.                     [QUARTUS_GOTCHAS sec 14,
                                                      spec sec 22.1, sec 21.8]
 5. FABRIC BUDGET         -- a wide, deep array in V3 source that is neither a
    section 6 bank nor one of the section 21.2/21.3 separately accounted stores
    is payload hiding in flops.                      [sec 21.4, sec 21.8]

TWO WAYS A BANK CAN EXIST, AND WHY
-----------------------------------
This tool was first written assuming a bank is an array declared inline and
named after its section 6 entry. Running it against the real tree said
otherwise: the V3 lane built ONE parameterised primitive
(`zhao_texture_v3bank`) and gets each bank by instantiating it. That is the
better answer -- section 21.2 counts "independently ported stores", and a list
of instances IS that count -- so the gate follows the RTL rather than the other
way round. Both forms are supported:

  * an inline array, identified by `// V3-BANK: <NAME>` or by its name;
  * an instance of a registered primitive, identified the same two ways, whose
    geometry comes from its .WIDTH/.DEPTH overrides.

For an instance, checks 2-4 are done once on the primitive's body and checks
1/5 per instance. An instance of the primitive that names no bank and matches
no accounted store is V3-UNBOUND: it is a real M10K in the fit report that
appears in no inventory.

WHAT IT DOES NOT CHECK
----------------------
Whether Quartus actually inferred an M10K. Only the RAM report can say that,
and the spec says so twice (sec 21.7: "the Quartus RAM report, not the
attribute, establishes the physical result"; Appendix B.6 says the same).
This gate catches the SOURCE shapes that make inference impossible. It is the
cheap half of the two-part evidence, and it is the half that can run on every
commit.

It also does not check read-during-write law publication (sec 6.4), collision
semantics, the credit/claim protocol (sec 8), or the ABI bit layouts of
Appendix B. Those are behavioural and belong to the adversarial testbench of
section 23. This is the STRUCTURAL half only.

STATE MACHINE OF THIS GATE'S OWN VERDICT
-----------------------------------------
The V3 lane may not have landed yet, and a gate that is red for months is a
gate people learn to ignore -- `check_ingress_capture.py` records that lesson
in its own header. So:

  * no V3 source at all      -> NOT STARTED. Printed loudly, exit 0.
  * some V3 source present   -> every bank that EXISTS is fully checked and a
                                defect is a hard failure. A bank that does not
                                exist yet is PENDING, printed by name, exit 0.
  * --require-all            -> PENDING becomes MISSING and fails. This is the
                                section 26.2 acceptance form of the gate: run
                                it when the lane claims architecture-complete.

V3-UNBOUND -- a bank primitive instantiated under a name that declares no bank
and no accounted store -- gets the same two-tier treatment, and for the same
reason. It is a missing DECLARATION rather than a wrong structure: the
primitive's body is checked, so the store is synchronous and single-ported
whatever it is called; what is missing is its line in the inventory. It prints
in full on every run, marked [WARN], and is fatal under --require-all.

USAGE
    python tools/rtl/check_v3_banks.py [--list] [--require-all] [--fixtures]
                                       [FILE ...]

    --fixtures  run the deliberately-wrong fixtures and print each one's
                verbatim output. This is the proof the instrument can fire.
    FILE ...    check these files instead of discovering the V3 tree.

Exit 0 clean, 1 on a violation, 2 when the tool could not parse what it was
given -- which is an ERROR, never a pass. A result of exactly zero from a
parser that silently dropped its input is the single most repeated failure in
this tree's tooling history, and every self-check below exists because of one.
"""

from __future__ import annotations

import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SPEC = "reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt"

# ---------------------------------------------------------------------------
# THE DECLARED BANK TABLE.
#
# Transcribed verbatim from section 6.1 of the spec named above, so that the
# tool and the specification can be diffed BY EYE. Do not "improve" a number
# here; change the spec, then transcribe it again.
#
#  BANK                  DEPTH x WIDTH       SINGLE WRITER       SINGLE READER
#  --------------------  ------------------  ------------------  -----------------
#  OWNER_CONTEXT             64 x 64         admission           ordered retirement
#  MATERIAL                  64 x 48         admission           combine admission
#  AUX_GEOMETRY              64 x 80         admission           AUX issue
#  SAMPLE_DESC_0             64 x 80         admission           sample expansion
#  SAMPLE_DESC_1             64 x 80         admission           sample expansion
#  SAMPLE_DESC_2             64 x 80         admission           sample expansion
#  RCP_RESULT                64 x 32         RCP final commit    sample expansion
#  SAMPLE_RESULT_0           64 x 40         TMU commit bank 0   combine admission
#  SAMPLE_RESULT_1           64 x 40         TMU commit bank 1   combine admission
#  SAMPLE_RESULT_2           64 x 40         TMU commit bank 2   combine admission
#  AUX_RESULT                64 x 40         AUX commit          combine admission
#  FINAL_RESULT              64 x 40         combine completion  ordered retirement
#  SAMPLE_METADATA          256 x 40         planner accept      response preparation
#
# SAMPLE_METADATA is 256 deep on purpose: {owner_slot, sample_index} is an
# eight-bit address and the 64 rows with sample_index 3 are intentionally
# unused. The spec forbids compressing it to 192 through a multiply-by-three.
# ---------------------------------------------------------------------------
BANKS = [
    # (name,            depth, width, writer,               reader)
    ("OWNER_CONTEXT",      64,    64, "admission",          "ordered retirement"),
    ("MATERIAL",           64,    48, "admission",          "combine admission"),
    ("AUX_GEOMETRY",       64,    80, "admission",          "AUX issue"),
    ("SAMPLE_DESC_0",      64,    80, "admission",          "sample expansion"),
    ("SAMPLE_DESC_1",      64,    80, "admission",          "sample expansion"),
    ("SAMPLE_DESC_2",      64,    80, "admission",          "sample expansion"),
    ("RCP_RESULT",         64,    32, "RCP final commit",   "sample expansion"),
    ("SAMPLE_RESULT_0",    64,    40, "TMU commit bank 0",  "combine admission"),
    ("SAMPLE_RESULT_1",    64,    40, "TMU commit bank 1",  "combine admission"),
    ("SAMPLE_RESULT_2",    64,    40, "TMU commit bank 2",  "combine admission"),
    ("AUX_RESULT",         64,    40, "AUX commit",         "combine admission"),
    ("FINAL_RESULT",       64,    40, "combine completion", "ordered retirement"),
    ("SAMPLE_METADATA",   256,    40, "planner accept",     "response preparation"),
]
BANK_BY_NAME = {b[0]: b for b in BANKS}

# ---------------------------------------------------------------------------
# SEPARATELY ACCOUNTED STORES.
#
# Section 21.2's core inventory and section 21.3's buffered profile name stores
# that are NOT in the section 6 transaction-file table but are budgeted M10Ks
# all the same. Without this list the fabric check (check 5) would flag every
# one of them, and a gate that flags the spec's own inventory is a noise
# generator -- `check_ingress_capture.py` had to learn that distinction too.
#
# A name is matched as a lowercase substring of the array identifier. Anything
# matched here is REPORTED as an accounted store rather than silently ignored,
# because "allowed" and "invisible" are different, and the spec's own words are
# "honest counting, not refusing one useful block on principle" (sec 21.4).
# ---------------------------------------------------------------------------
# Markers that resolved to no declaration. Reported, not fatal; see the
# note at the append site.
_unbound_markers = []

ACCOUNTED_STORES = [
    # section 21.2, core bank inventory
    ("cache_dat",     "cache static data bank            [sec 21.2]"),
    ("cache_tag",     "cache static tag bank             [sec 21.2]"),
    ("rsp_pool",      "prepared response pool 16x120     [sec 21.2]"),
    ("palette",       "palette 1024x16                   [sec 21.2]"),
    ("binding",       "binding descriptors 256x128       [sec 21.2]"),
    ("envelope",      "envelopes 16x160                  [sec 21.2]"),
    ("rcp_payload",   "RCP local payload77 x16           [sec 21.2]"),
    ("rcp_scratch",   "RCP local scratch64 x16           [sec 21.2]"),
    ("comb_payload",  "COMBINE local payload allowance   [sec 21.2]"),
    ("comb_scratch",  "COMBINE local scratch allowance   [sec 21.2]"),
    ("comb_tag",      "COMBINE tag allowance             [sec 21.2]"),
    ("rcp_pending",   "RCP_PENDING 64x40                 [sec 21.2]"),
    ("sample_owner",  "SAMPLE_OWNER 64x17                [sec 21.2]"),
    ("aux_pending",   "AUX_PENDING 64x14                 [sec 21.2]"),
    ("ready_tmu",     "READY_TMU 64x14                   [sec 21.2]"),
    ("ready_aux",     "READY_AUX 64x14                   [sec 21.2]"),
    ("ready_initial", "READY_INITIAL 64x14               [sec 21.2]"),
    ("seed_rom",      "RCP generated seed ROM            [sec 21.4]"),
    # section 21.3, explicit buffered profile
    ("aux_offer",     "AUX offer FIFO 16x32              [sec 21.3]"),
    ("aux_return",    "AUX terminal-return FIFO 16x40    [sec 21.3]"),
    ("raw_capture",   "raw cache-return capture 8x80     [sec 21.3]"),
    ("out_fifo",      "final output FIFO 4x120           [sec 21.3]"),
    ("persp_side",    "PERSPUV side-metadata FIFO 16x40  [sec 21.3]"),
]

# An array this wide AND this deep is payload, not control.
#
# The width floor is 32 rather than something smaller ON PURPOSE. Section 21.5
# says "a representative 29-bit per-owner control allocation already costs
# 1,856 flip-flops for 64 owners" and budgets exactly that in fabric -- so a
# 29-bit scoreboard is legal and must not be flagged. 32 is the first width at
# which the spec has no fabric allowance left.
FABRIC_MIN_WIDTH = 32
FABRIC_MIN_DEPTH = 16

# ... OR simply large, whatever its shape.
#
# THE WIDTH FLOOR ALONE READ LOW, and the case that proved it is the one this
# file's own header cites. D19m's worst array is
#
#     logic [15:0] pal_dat_r [PAL_SLOTS][256];   // 16 x 256 x 16 = 65,536 bits
#
# -- SIXTEEN bits wide. It is the single largest payload plane in the old
# island, it is most of the 72,824 registers, and a width>=32 rule sails
# straight past it. Checking the heuristic against a case already known by hand
# is CLAUDE.md's rule 3, and this is what it found.
#
# 4,096 bits is above section 21.5's explicitly fabric-legal 29-bit-per-owner
# scoreboard (29 x 64 = 1,856) and below anything the spec calls a bank.
FABRIC_MIN_BITS = 4096

# Where the V3 lane's source lives. Deliberately a glob and not a hand list:
# unlike check_guard_verdict's CLIENTS, there is nothing here yet to enumerate,
# and a hand list of files that do not exist would report a permanent zero.
V3_GLOBS = [
    "fpga/rtl/texture/*v3*.sv",
    "fpga/rtl/texture/*V3*.sv",
]

# ---------------------------------------------------------------------------
# BANK PRIMITIVES AND THEIR WRAPPERS.
#
# THE MODEL THIS TOOL WAS FIRST WRITTEN WITH WAS WRONG, and running it against
# the real tree is what said so. It assumed a bank is an array declared inline
# and named after the section 6 entry. The V3 lane instead built ONE
# parameterised primitive, `zhao_texture_v3bank`, and gets each bank by
# instantiating it -- which is a better answer to section 6.6 ("register local
# write enables and bank selects near each bank") and to section 21.2's
# "independently ported store" counting, because the port inventory becomes a
# list of instances.
#
# So identity lives at the INSTANTIATION, and the gate has to look there:
#
#   BANK_TEMPLATES     a module that IS one bank. Its internal array is checked
#                      for synchronicity, ports and logic-before-register, but
#                      NOT for geometry (it is parameterised) and not against
#                      the fabric budget (it is the bank).
#   WRAPPER_TEMPLATES  a generic module that instantiates a primitive without
#                      itself being a named bank -- `zhao_texture_v3rq` is the
#                      ready-queue body, and which of READY_TMU/AUX/INITIAL it
#                      is, is decided where the WRAPPER is instantiated.
#
# Both are hand-registered, deliberately, in the check_guard_verdict CLIENTS
# tradition -- and both are AUDITED against the tree below, so a rename cannot
# turn this list into a gate that quietly checks nothing.
# ---------------------------------------------------------------------------
BANK_TEMPLATES = ["zhao_texture_v3bank"]
WRAPPER_TEMPLATES = ["zhao_texture_v3rq"]

# ---------------------------------------------------------------------------
# PATTERNS
# ---------------------------------------------------------------------------

# `(* ramstyle = "M10K" *) logic [39:0] mem [0:63];`
# `logic [DW-1:0] sample_desc_0_m [0:DEPTH-1];`
# `reg [39:0] mem [64];`
ARRAY_RE = re.compile(
    r"^[ \t]*(?:\(\*[^)]*\*\)[ \t]*)?"           # optional (* ramstyle = ... *)
    r"(?:var[ \t]+)?(?:logic|reg|bit)[ \t]*"
    r"(?:signed[ \t]*)?"
    r"(\[[^\];]*\][ \t]*)?"                       # packed width, optional
    r"([A-Za-z_]\w*)[ \t]*"                       # name
    r"((?:\[[^\];]*\][ \t]*)+)[ \t]*;",           # one or more unpacked dims
    re.M,  # `^` must mean start-of-LINE. check_array_storage.py reported a
           # confident ZERO on the very tree it was written for because this
           # flag was missing. Same regex family, same trap.
)

# The value must not cross a NEWLINE. `[^;,]+` looked equivalent and was not:
# the LAST parameter in a `#( ... )` header has no trailing comma, so the value
# ran on through `) (` and into the port list until it hit the first port's
# comma. Every such parameter silently failed to resolve, and every array using
# it then failed to parse -- which, before the `--require-all` audit was run
# against real files, looked exactly like "the legacy tree is fine".
PARAM_RE = re.compile(
    r"^\s*(?:parameter|localparam)\s+(?:type\s+)?(?:int\s+|integer\s+|bit\s+|logic\s+)?"
    r"(?:unsigned\s+|signed\s+)?(?:\[[^\]]*\]\s*)?([A-Za-z_]\w*)\s*=\s*([^;,\n]+)",
    re.M,
)

# `// V3-BANK: SAMPLE_RESULT_0` -- the explicit, unambiguous way for RTL to say
# which declared bank an array or a bank INSTANCE is. Preferred over the name
# convention because a rename cannot silently detach it from its contract.
# A marker may name SEVERAL banks, comma separated. That is not a
# convenience: `zhao_texture_v3own` creates SAMPLE_RESULT_0/1/2 from ONE
# generate loop over three genvar values, which is the right way to write
# it (section 6: three statically banked stores, one writer each) and
# which one source line cannot otherwise declare. Without this the lane
# could not satisfy the gate for those three banks at all.
MARKER_RE = re.compile(r"//\s*V3-BANK\s*:\s*([A-Za-z0-9_ ,]+)")
CLOG2_RE = re.compile(r"\$clog2\s*\(\s*(\d+)\s*\)")

# A NAMED PATTERN, ASSERTED BELOW, because the inline version of this one was
# written through a shell heredoc on 2026-09-06 and its `\b` arrived in the file
# as a literal backspace character (0x08). The pattern then matched nothing and
# the template audit reported that `zhao_texture_v3rq` had been deleted -- a
# module sitting right there on disk. CLAUDE.md records the same accident
# happening to a self-check, where it read LOW and printed reassurance for the
# tool's whole life; this one happened to read HIGH and was caught in one run.
# Both are the same bug. Named patterns get asserted; inline ones do not.
MODULE_DECL_RE = re.compile(r"\bmodule\s+([A-Za-z_]\w*)")

PROC_START = re.compile(
    r"^\s*(always_ff|always_comb|always_latch|always|initial|final|function|task)\b")
ASSIGN_START = re.compile(r"^\s*assign\b")

OPEN_RE = re.compile(r"\bbegin\b|\bcase\b|\bcasez\b|\bcasex\b|\bfork\b")
CLOSE_RE = re.compile(r"\bend\b|\bendcase\b|\bjoin\b")

# ---------------------------------------------------------------------------
# IMPORT-TIME SELF-CHECK OF THE PATTERNS THEMSELVES.
#
# CLAUDE.md: a self-check written with an escape a shell heredoc mangled matched
# nothing and printed reassurance for its entire life. Every pattern that can
# read low is asserted against a known-good example here, at import, so the
# module cannot even load in a state where it would report a comfortable zero.
# ---------------------------------------------------------------------------
assert ARRAY_RE.search('  (* ramstyle = "M10K" *) logic [39:0] mem [0:63];'), \
    "ARRAY_RE no longer matches the Appendix B.6 coding template"
assert ARRAY_RE.search("  logic [DW-1:0] sample_desc_0_m [0:DEPTH-1];"), \
    "ARRAY_RE no longer matches a parameterised bank declaration"
assert ARRAY_RE.search("  reg [39:0] final_result_m [64];"), \
    "ARRAY_RE no longer matches the [N] unpacked form"
assert ARRAY_RE.search("  logic [15:0] pal_dat_r [16][256];"), \
    "ARRAY_RE no longer matches a multidimensional array -- the D19m shape"
assert PARAM_RE.search("  localparam int DEPTH = 64;"), "PARAM_RE is dead"
assert PARAM_RE.search("  parameter DW = 40;"), "PARAM_RE is dead"
assert MARKER_RE.search("  // V3-BANK: SAMPLE_RESULT_0"), "MARKER_RE is dead"
assert MARKER_RE.search(
    "  // V3-BANK: SAMPLE_RESULT_0, SAMPLE_RESULT_1, SAMPLE_RESULT_2"
).group(1).count(",") == 2, "MARKER_RE lost the multi-bank list form"
assert MODULE_DECL_RE.search("module zhao_texture_v3bank #("), "MODULE_DECL_RE is dead"
assert MODULE_DECL_RE.search("`default_nettype none\nmodule fx_good_bank ("), \
    "MODULE_DECL_RE is dead"
assert CLOG2_RE.search("parameter int unsigned AW = $clog2(DEPTH)") is None, \
    "CLOG2_RE must only fire on a literal argument"
assert CLOG2_RE.search("$clog2(64)"), "CLOG2_RE is dead"
assert PROC_START.match("  always_ff @(posedge clk) begin"), "PROC_START is dead"
assert ASSIGN_START.match("  assign rd_data = mem[a];"), "ASSIGN_START is dead"


# `chr(10)`, not a backslash-n literal. check_guard_verdict.py spells its
# newlines the same way, for the same reason: an escape sequence in this
# repository has been turned into a real newline by a shell heredoc three times
# in one day, and each time the file was still syntactically plausible.
NL = chr(10)


class Unparsed(Exception):
    """The tool could not read something it must read. Never a pass."""


# ---------------------------------------------------------------------------
# SOURCE HANDLING
# ---------------------------------------------------------------------------

def strip_comments_keep_lines(src):
    """Remove comments but preserve every newline, so line numbers stay true."""
    def repl(m):
        return "\n" * m.group(0).count("\n")
    src = re.sub(r"/\*.*?\*/", repl, src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    return src


def resolve(expr, params):
    """Evaluate a width/depth expression against the file's own parameters.

    Returns an int, or raises Unparsed. It NEVER guesses -- check_array_storage
    records why: "A guessed width would be a measurement that decides a value,
    which this repository does not do."
    """
    e = expr.strip()
    for _ in range(8):
        sub = re.sub(r"\b([A-Za-z_]\w*)\b",
                     lambda m: str(params[m.group(1)])
                     if m.group(1) in params else m.group(1), e)
        # `$clog2(64)` is resolved only once its argument is a literal, so this
        # sits inside the substitution loop rather than before it.
        sub = CLOG2_RE.sub(
            lambda m: str(max(0, (int(m.group(1)) - 1).bit_length())), sub)
        if sub == e:
            break
        e = sub
    e = e.replace("'d", "").strip()
    if not re.match(r"^[\d\s+\-*/()%]+$", e):
        raise Unparsed("cannot resolve %r (unresolved symbol)" % expr)
    try:
        return int(eval(e, {"__builtins__": {}}, {}))  # noqa: S307 - guarded above
    except Exception as exc:  # pragma: no cover - defensive
        raise Unparsed("cannot evaluate %r: %s" % (expr, exc))


def packed_width(bracket, params):
    """`[39:0]` -> 40, `[DW-1:0]` -> DW, None -> 1."""
    if bracket is None:
        return 1
    inner = bracket.strip()[1:-1]
    if ":" not in inner:
        raise Unparsed("packed dimension %r has no msb:lsb" % bracket)
    hi, lo = inner.split(":", 1)
    return abs(resolve(hi, params) - resolve(lo, params)) + 1


def unpacked_dims(text, params):
    """`[0:63]` -> [64], `[64]` -> [64], `[16][256]` -> [16, 256]."""
    dims = []
    for m in re.finditer(r"\[([^\]]*)\]", text):
        inner = m.group(1)
        if ":" in inner:
            hi, lo = inner.split(":", 1)
            dims.append(abs(resolve(hi, params) - resolve(lo, params)) + 1)
        else:
            dims.append(resolve(inner, params))
    if not dims:
        raise Unparsed("no unpacked dimension in %r" % text)
    return dims


class Region(object):
    __slots__ = ("kind", "start", "end", "header", "sens", "idx")

    def __init__(self, kind, start, end, header, sens, idx):
        self.kind = kind      # ff | comb | assign | initial | func
        self.start = start    # 0-based line
        self.end = end        # 0-based line, inclusive
        self.header = header
        self.sens = sens      # sensitivity text for clocked processes
        self.idx = idx


def _body_end(lines, i, kw):
    """Last line of the process opening on line i. Raises Unparsed if unbalanced."""
    n = len(lines)
    if kw in ("function", "task"):
        term = "endfunction" if kw == "function" else "endtask"
        for k in range(i, n):
            if re.search(r"\b%s\b" % term, lines[k]):
                return k
        raise Unparsed("no %s for %s opened on line %d" % (term, kw, i + 1))
    # find `begin` before the statement terminates
    depth = 0
    started = False
    for k in range(i, n):
        t = lines[k]
        opens = len(OPEN_RE.findall(t))
        closes = len(CLOSE_RE.findall(t))
        if opens:
            started = True
        depth += opens - closes
        if started and depth <= 0:
            return k
        if not started and ";" in t and k >= i:
            return k          # single-statement process, no begin
    raise Unparsed("unbalanced begin/end for process opened on line %d" % (i + 1))


def _sens(lines, i):
    """Text of the `@(...)` sensitivity list of the process opening on line i."""
    blob = " ".join(lines[i:i + 4])
    m = re.search(r"@\s*\(", blob)
    if not m:
        return ""
    depth = 0
    for j in range(m.end() - 1, len(blob)):
        if blob[j] == "(":
            depth += 1
        elif blob[j] == ")":
            depth -= 1
            if depth == 0:
                return blob[m.end():j]
    return ""


def regions(lines):
    regs = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if ASSIGN_START.match(line):
            j = i
            while j < n and ";" not in lines[j]:
                j += 1
            j = min(j, n - 1)
            regs.append(Region("assign", i, j, line.strip(), "", len(regs)))
            i = j + 1
            continue
        m = PROC_START.match(line)
        if m:
            kw = m.group(1)
            sens = _sens(lines, i) if kw in ("always_ff", "always") else ""
            kind = {
                "always_ff": "ff", "always_comb": "comb", "always_latch": "comb",
                "initial": "initial", "final": "initial",
                "function": "func", "task": "func",
            }.get(kw)
            if kind is None:            # plain `always`
                kind = "ff" if ("posedge" in sens or "negedge" in sens) else "comb"
            end = _body_end(lines, i, kw)
            regs.append(Region(kind, i, end, line.strip(), sens, len(regs)))
            i = end + 1
            continue
        i += 1
    return regs


def statements(lines):
    """(first_line_index, joined_text) for each statement-ish unit."""
    out = []
    buf = []
    start = None
    for idx, raw in enumerate(lines):
        if start is None:
            if not raw.strip():
                continue
            start = idx
            buf = [raw]
        else:
            buf.append(raw)
        if ";" in raw or OPEN_RE.search(raw) or CLOSE_RE.search(raw):
            out.append((start, " ".join(x.strip() for x in buf)))
            start, buf = None, []
    if start is not None:
        out.append((start, " ".join(x.strip() for x in buf)))
    return out


def refs(stmt, name):
    """Every `name[...]([...])*` in stmt.

    Returns (start, address_text, last_close_index, full_reference_text).

    The trailing groups matter. `mem[slot][idx] <= d` is a WRITE, and the first
    version of this function stopped at the first `]`, found `[idx]` where it
    wanted `<=`, and filed a multidimensional write as a READ -- which then
    produced a phantom second read address and a phantom logic-before-register
    finding on `multidim_bank.sv`. The fixture showed both, which is why the
    fixture exists.
    """
    out = []
    for m in re.finditer(r"\b" + re.escape(name) + r"\s*\[", stmt):
        j = m.end() - 1
        idx = []
        while j < len(stmt) and stmt[j] == "[":
            depth = 0
            k = j
            while k < len(stmt):
                if stmt[k] == "[":
                    depth += 1
                elif stmt[k] == "]":
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            if k >= len(stmt):
                raise Unparsed("unbalanced index bracket on %s in %r"
                               % (name, stmt[:80]))
            idx.append(stmt[j + 1:k].strip())
            j = k + 1
            while j < len(stmt) and stmt[j] == " ":
                j += 1
        last = j - 1
        out.append((m.start(), "][".join(idx), last, stmt[m.start():j].rstrip()))
    return out


def is_write(stmt, start, close):
    after = stmt[close + 1:].lstrip()
    if after.startswith("<="):
        pass
    elif after.startswith("=") and not after.startswith("=="):
        pass
    else:
        return False
    before = stmt[:start].rstrip()
    return (before == ""
            or before.endswith("begin") or before.endswith("else")
            or before.endswith(")") or before.endswith(":"))


def rhs_of(stmt):
    """Text right of the top-level assignment operator, or None."""
    depth = 0
    i = 0
    while i < len(stmt):
        c = stmt[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif depth == 0:
            if stmt.startswith("<=", i):
                return stmt[i + 2:]
            if c == "=" and not stmt.startswith("==", i) \
                    and (i == 0 or stmt[i - 1] not in "=<>!+-*/&|^%~"):
                return stmt[i + 1:]
        i += 1
    return None


def norm(s):
    return re.sub(r"\s+", "", s or "")


# ---------------------------------------------------------------------------
# THE CHECKS
# ---------------------------------------------------------------------------

class Array(object):
    def __init__(self, path, line, name, width, dims, bank, marker):
        self.path = path
        self.line = line          # 1-based
        self.name = name
        self.width = width
        self.dims = dims
        self.bank = bank          # declared bank name or None
        self.marker = marker      # True when identified by an explicit marker
        self.module = None        # enclosing module, filled in by analyse()
        self.writes = []          # (region, addr, line, stmt, full_ref)
        self.reads = []           # (region, addr, line, stmt, full_ref)

    @property
    def depth(self):
        d = 1
        for x in self.dims:
            d *= x
        return d


class Instance(object):
    """An instantiation of a bank primitive: one physical, independently
    ported store, in the section 21.2 sense."""

    def __init__(self, path, line, template, inst, width, depth, banks, marker):
        self.path = path
        self.line = line
        self.template = template
        self.inst = inst
        self.width = width
        self.depth = depth
        # A LIST, because one generate loop is legitimately several banks.
        self.banks = banks
        self.marker = marker


SUFFIXES = ("_mem", "_ram", "_bank", "_arr", "_body", "_m", "_q", "_r")
INST_PREFIXES = ("u_", "i_", "the_")


def _strip_names(name):
    base = name.lower()
    for pre in INST_PREFIXES:
        if base.startswith(pre):
            base = base[len(pre):]
            break
    if base.startswith("v3_"):
        base = base[3:]
    for suf in SUFFIXES:
        if base.endswith(suf):
            base = base[:-len(suf)]
            break
    return base


def bank_for(name, markers, decl_line):
    """Which declared bank is this array, if any?

    A marker binds to the declaration it sits ON or immediately ABOVE, and is
    CONSUMED, so it cannot reach past its own array and adopt the next one.
    The first version searched three lines back and quietly annexed the
    unbanked shadow copy in `fabric_payload.sv` -- which made the fabric fixture
    stop firing and the file look like a duplicate-declaration problem instead.
    Found by the fixture suite on its first run, which is the entire point of it.
    """
    for probe in (decl_line, decl_line - 1):
        if probe in markers:
            return markers.pop(probe), True
    base = _strip_names(name)
    for bname in BANK_BY_NAME:
        if base == bname.lower():
            return [bname], False
    return [], False


def _balanced(text, i, opener="(", closer=")"):
    """Index just past the group opening at text[i]. Raises Unparsed."""
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
    raise Unparsed("unbalanced %s..%s starting at offset %d" % (opener, closer, i))


def find_instances(path, src, markers, params, defaults):
    """Every instantiation of a registered bank primitive in this file."""
    out = []
    for tmpl in BANK_TEMPLATES:
        for m in re.finditer(r"\b" + re.escape(tmpl) + r"\b\s*", src):
            k = m.end()
            # never treat the module's own declaration as an instance
            head = src[max(0, m.start() - 12):m.start()]
            if re.search(r"\bmodule\s*$", head):
                continue
            over = {}
            if k < len(src) and src[k] == "#":
                k += 1
                while k < len(src) and src[k] in " \t\n":
                    k += 1
                if k >= len(src) or src[k] != "(":
                    raise Unparsed("%s: `#` without a parameter list on %s"
                                   % (path, tmpl))
                end = _balanced(src, k)
                body = src[k + 1:end - 1]
                for pm in re.finditer(r"\.\s*(\w+)\s*\(([^()]*)\)", body):
                    try:
                        over[pm.group(1)] = resolve(pm.group(2), params)
                    except Unparsed:
                        pass
                k = end
            while k < len(src) and src[k] in " \t\n":
                k += 1
            im = re.match(r"([A-Za-z_]\w*)", src[k:])
            if not im:
                raise Unparsed("%s: cannot read the instance name after %s"
                               % (path, tmpl))
            inst = im.group(1)
            line0 = src.count("\n", 0, m.start())

            merged = dict(defaults.get(tmpl, {}))
            merged.update(over)
            for need in ("WIDTH", "DEPTH"):
                if need not in merged:
                    raise Unparsed(
                        "%s:%d instance %s of %s has no %s and the primitive's "
                        "default could not be read. The gate will not guess a "
                        "bank geometry." % (path, line0 + 1, inst, tmpl, need))
            banks, marked = bank_for(inst, markers, line0)
            out.append(Instance(path, line0 + 1, tmpl, inst,
                                merged["WIDTH"], merged["DEPTH"], banks, marked))
    return out


def module_param_defaults(src):
    """{module: {PARAM: value}} for every `module X #( ... )` in this text."""
    out = {}
    for m in re.finditer(r"\bmodule\s+([A-Za-z_]\w*)\s*", src):
        k = m.end()
        if k >= len(src) or src[k] != "#":
            out.setdefault(m.group(1), {})
            continue
        k += 1
        while k < len(src) and src[k] in " \t\n":
            k += 1
        if k >= len(src) or src[k] != "(":
            out.setdefault(m.group(1), {})
            continue
        end = _balanced(src, k)
        vals = {}
        for pm in PARAM_RE.finditer(src[k:end]):
            try:
                vals[pm.group(1)] = resolve(pm.group(2), vals)
            except Unparsed:
                pass
        out[m.group(1)] = vals
    return out


def module_spans(lines):
    """[(name, first_line, last_line)] for every module in the file."""
    out = []
    open_name = None
    start = 0
    for i, ln in enumerate(lines):
        m = re.match(r"\s*module\s+([A-Za-z_]\w*)", ln)
        if m and open_name is None:
            open_name, start = m.group(1), i
        elif re.match(r"\s*endmodule\b", ln) and open_name is not None:
            out.append((open_name, start, i))
            open_name = None
    if open_name is not None:
        out.append((open_name, start, len(lines) - 1))
    return out


def read_source(path):
    p = path if os.path.isabs(path) else os.path.join(REPO, path)
    try:
        return io.open(p, encoding="utf-8").read()
    except OSError as exc:
        raise Unparsed("cannot read %s: %s" % (path, exc))


def collect_defaults(paths):
    """Parameter defaults of every module in the scanned set, for instances."""
    out = {}
    for p in paths:
        out.update(module_param_defaults(strip_comments_keep_lines(read_source(p))))
    return out


def analyse(path, defaults=None):
    """Parse one file. Raises Unparsed on anything it cannot read."""
    defaults = defaults or {}
    try:
        raw = io.open(os.path.join(REPO, path) if not os.path.isabs(path) else path,
                      encoding="utf-8").read()
    except OSError as exc:
        raise Unparsed("cannot read %s: %s" % (path, exc))

    markers = {}
    for i, line in enumerate(raw.split("\n")):
        m = MARKER_RE.search(line)
        if m:
            names = [n.strip().upper() for n in m.group(1).split(",") if n.strip()]
            for name in names:
                if name in BANK_BY_NAME:
                    continue
                # A marker may also name a section 21.2/21.3 ACCOUNTED STORE.
                # READY_TMU / READY_AUX / READY_INITIAL, RCP_PENDING,
                # SAMPLE_OWNER, AUX_PENDING and the rest are real independently
                # ported stores in the inventory -- they are simply not in
                # section 6's transaction-file table, which lists only the
                # per-owner record banks.
                #
                # Rejecting them made this gate red the moment a lane marked one
                # correctly, which is the worst possible failure for a gate: it
                # punishes the exact behaviour it is asking for. The gate owns
                # both tables and had no business understanding only one.
                #
                # Geometry is NOT checked for these: section 21.2 gives their
                # block counts, not their depth x width, so there is nothing to
                # check them against and pretending otherwise would invent a
                # law. They are resolved for IDENTITY only, which is what
                # stops them being reported as unbound.
                if any(k in name.lower() for k, _d in ACCOUNTED_STORES):
                    continue
                raise Unparsed(
                    "%s:%d names %r, which is in neither the section 6 bank "
                    "table nor the section 21.2/21.3 accounted-store "
                    "inventory. Either the spec moved or the marker is a typo; "
                    "a marker the gate cannot resolve is not allowed to pass."
                    % (path, i + 1, name))
            if len(set(names)) != len(names):
                raise Unparsed("%s:%d repeats a bank name in one marker"
                               % (path, i + 1))
            markers[i] = names

    src = strip_comments_keep_lines(raw)
    lines = src.split("\n")

    params = {}
    for m in PARAM_RE.finditer(src):
        try:
            params[m.group(1)] = resolve(m.group(2), params)
        except Unparsed:
            pass          # a non-numeric parameter is fine until an array needs it

    regs = regions(lines)
    line_region = {}
    for r in regs:
        for n in range(r.start, r.end + 1):
            line_region.setdefault(n, r)

    arrays = []
    decl_lines = set()
    for m in ARRAY_RE.finditer(src):
        line0 = src.count("\n", 0, m.start())
        decl_lines.add(line0)
        name = m.group(2)
        # a declaration inside a process is a local variable, not a bank plane
        try:
            width = packed_width(m.group(1), params)
            dims = unpacked_dims(m.group(3), params)
        except Unparsed as exc:
            raise Unparsed("%s:%d %s -- %s" % (path, line0 + 1, name, exc))
        banks, marked = bank_for(name, markers, line0)
        if len(banks) > 1:
            raise Unparsed(
                "%s:%d array %s is marked as %d banks. One array declaration is "
                "one physical plane; a multi-bank marker belongs on a GENERATE "
                "loop that really does create several."
                % (path, line0 + 1, name, len(banks)))
        arrays.append(Array(path, line0 + 1, name, width, dims,
                            banks[0] if banks else None, marked))

    spans = module_spans(lines)
    for arr in arrays:
        for (mname, s, e) in spans:
            if s <= arr.line - 1 <= e:
                arr.module = mname
                break

    instances = find_instances(path, src, markers, params, defaults)

    # A marker that bound to nothing is a marker the gate did not act on. It
    # must be loud: a contract annotation pointing at no array or instance is
    # precisely the kind of thing that would let a bank drop out of the watch
    # set unnoticed.
    if markers:
        # WARN, not Unparsed -- and the distinction is worth stating because
        # the instinct to make it fatal is the right instinct applied to the
        # wrong case.
        #
        # "A file this gate cannot read is an ERROR, not a pass" is about
        # READING. This file reads perfectly; what is unresolved is an
        # ANNOTATION. Making it fatal reddens the shared gate the moment a lane
        # writes a marker slightly off, which is exactly what happened: a lane
        # was asked to add markers, did so correctly in spirit, and the gate
        # went red on placement. A gate that punishes the behaviour it is
        # asking for gets switched off.
        #
        # V3-UNBOUND is already a WARN tier for precisely this reason, so a
        # misplaced marker being fatal while a MISSING one is not was
        # inconsistent as well as unhelpful. Fatal under --require-all, where
        # the whole point is that nothing is provisional.
        _unbound_markers.append(
            "V3-MARKER  %s: marker(s) on line(s) %s bind to no array "
            "declaration or bank instantiation. A marker must sit ON the "
            "declaration or instantiation line, or the line DIRECTLY ABOVE it."
            % (path, ", ".join(str(k + 1) for k in sorted(markers))))

    by_name = {a.name: a for a in arrays}
    for start, stmt in statements(lines):
        if start in decl_lines:
            continue
        for name, arr in by_name.items():
            if name not in stmt:
                continue
            for (s, addr, close, full) in refs(stmt, name):
                reg = line_region.get(start)
                if is_write(stmt, s, close):
                    arr.writes.append((reg, addr, start + 1, stmt, full))
                else:
                    arr.reads.append((reg, addr, start + 1, stmt, full))
    return arrays, instances


def check_array(arr, offences, template=False):
    """Checks 1-4 for one bank array. Returns its finding count.

    `template=True` is the body of a registered bank PRIMITIVE: same
    synchronicity, port and logic-before-register laws, no geometry check,
    because a parameterised primitive has no single declared shape and its
    instances are checked instead.
    """
    out = []
    if template:
        name = "a %s bank" % arr.module
        depth = width = None
        writer, reader = "the primitive's one writer", "the primitive's one reader"
    else:
        name, depth, width, writer, reader = BANK_BY_NAME[arr.bank]
    where = "%s:%d %s (%s)" % (arr.path, arr.line, arr.name,
                               arr.module if template else arr.bank)

    # --- 1. geometry -------------------------------------------------------
    if len(arr.dims) > 1:
        out.append(("V3-MULTIDIM", where,
                    "declared with %d unpacked dimensions %s. A bank plane is one "
                    "dimensional; the D19m palette array was multidimensional and "
                    "indexed on both axes, and became 72,824 registers."
                    % (len(arr.dims), arr.dims)))
    if not template:
        if arr.width != width:
            out.append(("V3-WIDTH", where,
                        "is %d bits wide; section 6 declares %s at %d. A width "
                        "change requires updating the physical inventory, not a "
                        "quiet edit." % (arr.width, name, width)))
        if arr.depth != depth:
            out.append(("V3-DEPTH", where,
                        "is %d deep; section 6 declares %s at %d."
                        % (arr.depth, name, depth)))

    # --- 2. synchronous ----------------------------------------------------
    for (reg, addr, ln, stmt, full) in arr.reads:
        kind = reg.kind if reg else "continuous"
        if kind != "ff":
            out.append(("V3-ASYNC", "%s:%d" % (arr.path, ln),
                        "%s is read from a %s context. Section 6/LAW 08 declare "
                        "%s a synchronous M10K-backed bank; a combinationally "
                        "read array IS the 'large asynchronous table' section 0 "
                        "forbids.\n        %s"
                        % (arr.name, {"assign": "continuous assign",
                                      "comb": "combinational always",
                                      "func": "function/task",
                                      "initial": "initial",
                                      "continuous": "module-scope"}.get(kind, kind),
                           name, stmt.strip()[:110])))

    payload_regs = {}
    for (reg, addr, ln, stmt, full) in arr.writes + arr.reads:
        if reg is not None and reg.kind == "ff":
            payload_regs[reg.idx] = reg
    for reg in payload_regs.values():
        if "negedge" in reg.sens or "posedge rst" in reg.sens:
            out.append(("V3-RESETPAYLOAD", "%s:%d" % (arr.path, reg.start + 1),
                        "the payload process for %s has an asynchronous reset "
                        "(@(%s)). LAW 08 and section 6.5: payload memory "
                        "processes have only posedge clk; reset clears valid, "
                        "ownership and queue state, not the payload arrays."
                        % (name, reg.sens.strip())))

    # --- 3. one writer, one reader ----------------------------------------
    waddrs = sorted({norm(a) for (_r, a, _l, _s, _f) in arr.writes})
    raddrs = sorted({norm(a) for (_r, a, _l, _s, _f) in arr.reads})
    wregs = {r.idx for (r, _a, _l, _s, _f) in arr.writes if r is not None}

    if not arr.writes:
        out.append(("V3-NOWRITER", where,
                    "has no write. Its single writer is declared as "
                    "'%s'. A bank nothing writes is dead storage." % writer))
    if not arr.reads:
        out.append(("V3-NOREADER", where,
                    "has no read. Its single reader is declared as "
                    "'%s'. A bank nothing reads is dead storage." % reader))
    if len(waddrs) > 1:
        out.append(("V3-MULTIWRITE", where,
                    "has %d distinct write addresses %s. Exactly ONE "
                    "writer is declared ('%s'). A second write port blocks M10K inference and "
                    "is exactly the multiwrite payload state section 26.1 says the "
                    "experiment must not introduce."
                    % (len(waddrs), waddrs, writer)))
    if len(wregs) > 1:
        out.append(("V3-MULTIWRITE", where,
                    "is written from %d separate clocked processes. One writer is "
                    "declared ('%s')." % (len(wregs), writer)))
    if len(raddrs) > 1:
        out.append(("V3-MULTIREAD", where,
                    "has %d distinct read addresses %s. Exactly ONE "
                    "reader is declared ('%s'). A simple-dual-port M10K has one read port; "
                    "section 6.2: 'ramstyle cannot give an M10K an extra port.'"
                    % (len(raddrs), raddrs, reader)))

    # --- 4. no logic between the read and the first register --------------
    for (reg, addr, ln, stmt, full) in arr.reads:
        if reg is None or reg.kind != "ff":
            continue          # already reported as ASYNC
        rhs = rhs_of(stmt)
        want = full
        if rhs is None or norm(rhs).rstrip(";").split(";")[0] != norm(want):
            out.append(("V3-COMBLOGIC", "%s:%d" % (arr.path, ln),
                        "logic sits between the %s read and its first register. "
                        "QUARTUS_GOTCHAS section 14 / spec section 22.1: the "
                        "capturing assignment's right-hand side must be the array "
                        "read and nothing else, or the declared M10K becomes "
                        "flip-flops.\n        %s"
                        % (name, stmt.strip()[:110])))

    offences.extend(out)
    return len(out)


def check_instance(inst, bank, offences):
    """Check 1 (geometry) for an instantiated bank. Returns its finding count.

    Checks 2-4 do not repeat here: the primitive's body is checked once, where
    it is written, and every instance shares it. That is the whole reason the
    lane made it a module -- section 21.2 counts "independently ported stores",
    and a list of instances is exactly that count.
    """
    out = []
    name, depth, width, writer, reader = BANK_BY_NAME[bank]
    where = "%s:%d %s (%s of %s)" % (inst.path, inst.line, inst.inst,
                                     bank, inst.template)
    if inst.width != width:
        out.append(("V3-WIDTH", where,
                    "is instantiated .WIDTH(%d); section 6 declares %s at %d. "
                    "A width change requires updating the physical inventory."
                    % (inst.width, name, width)))
    if inst.depth != depth:
        out.append(("V3-DEPTH", where,
                    "is instantiated .DEPTH(%d); section 6 declares %s at %d."
                    % (inst.depth, name, depth)))
    offences.extend(out)
    return len(out)


def check_fabric(arrays, out):
    """Check 5. Payload-shaped arrays that are not declared banks."""
    notes = []
    for arr in arrays:
        if arr.bank:
            continue
        # The body of a registered bank primitive IS the bank. Its geometry is
        # parameterised and checked at each instantiation instead; flagging it
        # here would make the gate red on the one file that gets this right,
        # which is how a gate teaches people to ignore it.
        if arr.module in BANK_TEMPLATES:
            notes.append("  bank primitive: %s:%d %s in module %s "
                         "-- geometry checked per instance"
                         % (arr.path, arr.line, arr.name, arr.module))
            continue
        bits = arr.width * arr.depth
        wide = arr.width >= FABRIC_MIN_WIDTH and arr.depth >= FABRIC_MIN_DEPTH
        big = bits >= FABRIC_MIN_BITS
        if not (wide or big):
            continue
        low = arr.name.lower()
        hit = None
        for key, desc in ACCOUNTED_STORES:
            if key in low:
                hit = desc
                break
        if hit:
            notes.append("  accounted store: %s:%d %s  %dx%d  -- %s"
                         % (arr.path, arr.line, arr.name, arr.depth, arr.width, hit))
            continue
        out.append(("V3-FABRIC", "%s:%d %s" % (arr.path, arr.line, arr.name),
                    "is %d x %d = %d bits of payload-shaped state that is neither a "
                    "section 6 bank nor a section 21.2/21.3 accounted store. "
                    "Section 21.4 budgets masks, pointers, valid bits and bounded "
                    "pipeline registers in fabric -- not this. Section 21.8 says "
                    "stop before another long fit when a supposedly banked payload "
                    "shows up as thousands of registers.\n        (If it is "
                    "legitimate, add it to ACCOUNTED_STORES with its spec section, "
                    "or mark it with // V3-BANK: <NAME>.)"
                    % (arr.depth, arr.width, arr.depth * arr.width)))
    return notes


# ---------------------------------------------------------------------------
# DRIVER
# ---------------------------------------------------------------------------

def discover():
    found = []
    for pat in V3_GLOBS:
        for p in glob.glob(os.path.join(REPO, pat)):
            found.append(os.path.relpath(p, REPO).replace(os.sep, "/"))
    return sorted(set(found))


def run(paths, require_all=False, verbose=False, quiet=False,
        audit_templates=False):
    """Returns (exit_code, printed_lines)."""
    lines_out = []

    def say(s=""):
        lines_out.append(s)

    all_arrays = []
    all_insts = []
    try:
        defaults = collect_defaults(paths)
        for p in paths:
            arrs, insts = analyse(p, defaults)
            all_arrays.extend(arrs)
            all_insts.extend(insts)
            if verbose:
                say("  %s: %d arrays, %d bank instances, %d identified as banks"
                    % (p, len(arrs), len(insts),
                       len([a for a in arrs if a.bank]) +
                       sum(len(i.banks) for i in insts)))
    except Unparsed as exc:
        say("check_v3_banks: CANNOT PARSE -- %s" % exc)
        say("")
        say("A file this gate cannot read is an ERROR, not a pass. A parser that")
        say("silently drops what it cannot match reports fewer problems, and this")
        say("tree has been bitten by exactly that at least nine times.")
        return 2, lines_out

    offences = []
    warnings = []
    seen = {}
    found = {}

    # THE REGISTERED-TEMPLATE AUDIT. A hand list rots the moment a module is
    # renamed, and a gate that silently skips the one primitive everything is
    # built from is the instrument-reads-low failure at full scale. So a listed
    # template that no scanned file defines is an ERROR, exactly as
    # check_guard_verdict audits its CLIENTS against the tree.
    defined = set()
    for arr in all_arrays:
        if arr.module:
            defined.add(arr.module)
    for p in paths:
        try:
            for m in re.finditer(MODULE_DECL_RE,
                                 strip_comments_keep_lines(read_source(p))):
                defined.add(m.group(1))
        except Unparsed:
            pass
    if audit_templates:
        for t in BANK_TEMPLATES + WRAPPER_TEMPLATES:
            if t not in defined:
                offences.append((
                    "V3-TEMPLATE-GONE", t,
                    "is registered in BANK_TEMPLATES/WRAPPER_TEMPLATES but no "
                    "scanned V3 file defines it. Either it was renamed and the "
                    "list must follow, or the gate is now checking nothing."))

    # The body of each registered bank primitive: checks 2-4, once.
    for arr in all_arrays:
        if arr.module in BANK_TEMPLATES and not arr.bank:
            n = check_array(arr, offences, template=True)
            if verbose:
                say("  primitive %s.%s: %d finding(s)" % (arr.module, arr.name, n))

    # Bank identity carried by an INSTANCE of a primitive.
    for inst in all_insts:
        if inst.template in WRAPPER_TEMPLATES:
            continue
        if not inst.banks:
            low = inst.inst.lower()
            if any(k in low for k, _d in ACCOUNTED_STORES):
                continue
            if inst.path.rsplit("/", 1)[-1].split(".")[0] in WRAPPER_TEMPLATES:
                # inside a generic wrapper: identity is fixed where the WRAPPER
                # is instantiated, not here. Section 21.2 counts it there.
                continue
            # WARN, NOT FAIL, UNTIL THE ACCEPTANCE RUN.
            #
            # An unbound instance is missing a DECLARATION, not missing
            # correctness: the primitive it instantiates is checked, so the
            # store is synchronous and single-ported whatever it is called.
            # What is missing is its line in the inventory, and section 26.2
            # ("payload bank ownership matches actual inferred memory ports")
            # is where that has to be true. Failing the shared rtl:gates lane
            # for it while the V3 lane is still landing files would make a
            # gate that is red for weeks, which check_ingress_capture's header
            # explains is worse than the hole. So: printed in full on every
            # run, and fatal under --require-all, exactly like PENDING.
            warnings.append((
                "V3-UNBOUND", "%s:%d %s" % (inst.path, inst.line, inst.inst),
                "instantiates the bank primitive %s but names no section 6 bank "
                "and matches no section 21.2/21.3 accounted store. Section 21.4: "
                "'honest counting'. Add a // V3-BANK: <NAME> line above it (a "
                "comma-separated list for a generate loop), or give it an "
                "accounted-store name." % inst.template))
            continue
        for bank in inst.banks:
            if bank in seen:
                offences.append((
                    "V3-DUPLICATE",
                    "%s:%d %s" % (inst.path, inst.line, inst.inst),
                    "is a second instance of bank %s (first at %s). Section 6.6: "
                    "a replicated payload table because several modules want "
                    "their own copy must not happen accidentally."
                    % (bank, seen[bank])))
                continue
            seen[bank] = "%s:%d" % (inst.path, inst.line)
            found[bank] = check_instance(inst, bank, offences)

    for arr in all_arrays:
        if not arr.bank:
            continue
        if arr.bank in seen:
            offences.append((
                "V3-DUPLICATE", "%s:%d %s" % (arr.path, arr.line, arr.name),
                "is a second declaration of bank %s (first at %s). Section 6.6: a "
                "replicated 64-entry payload table because several modules want "
                "asynchronous access must not happen accidentally."
                % (arr.bank, seen[arr.bank])))
            continue
        seen[arr.bank] = "%s:%d" % (arr.path, arr.line)
        # The COUNT comes back from the checker rather than being recovered by
        # substring-matching the messages afterwards. The first version did the
        # latter, and the line-scoped codes (V3-ASYNC, V3-COMBLOGIC) carry only
        # a path:line, so a bank with a real logic-before-register defect printed
        # "OK" in this very summary. A status column that reads clean over a red
        # gate is the same instrument-lies-low failure in miniature.
        found[arr.bank] = check_array(arr, offences)

    notes = check_fabric(all_arrays, offences)

    present = sorted(seen)
    absent = [b[0] for b in BANKS if b[0] not in seen]

    if not quiet:
        say("check_v3_banks: section 6 declares %d banks; %d found, %d not yet in RTL"
            % (len(BANKS), len(present), len(absent)))
        for name, depth, width, writer, reader in BANKS:
            if name in seen:
                nbad = found.get(name, 0)
                say("  %-17s %4d x %-3d  %-12s  %s"
                    % (name, depth, width, seen[name],
                       "OK" if not nbad else "%d FINDING(S)" % nbad))
            else:
                say("  %-17s %4d x %-3d  %-12s  %s"
                    % (name, depth, width, "-",
                       "MISSING" if require_all else "pending (lane has not built it)"))
        for n in notes:
            say(n)

    # Misplaced markers ride the SAME warning channel as V3-UNBOUND, so they
    # are printed in full on every run and are fatal under --require-all. A
    # warning that is collected and never printed would be strictly worse than
    # the fatal error it replaced: the gate would silently ignore an annotation
    # the author believed was doing something.
    for _line in _unbound_markers:
        _code, _rest = _line.split("  ", 1)
        warnings.append((_code, _rest.split(":")[0],
                         _rest.split(":", 1)[1].strip()))
    del _unbound_markers[:]

    if warnings:
        say("")
        for code, where, msg in warnings:
            say("check_v3_banks: %s  %s%s"
                % (code, where, "" if require_all else "   [WARN]"))
            say("    %s" % msg)
        if not require_all:
            say("")
            say("These are DECLARATION gaps, not structural defects, and they do")
            say("not fail this run. `--require-all` -- the section 26.2 acceptance")
            say("form of this gate -- treats every one of them as a failure.")
    if require_all:
        offences.extend(warnings)
        for name in absent:
            _n, depth, width, writer, reader = BANK_BY_NAME[name]
            offences.append((
                "V3-MISSING", name,
                "declared %d x %d in section 6, writer '%s', reader '%s', is not "
                "present in any V3 source. Section 26.2 requires that payload bank "
                "ownership match actual inferred memory ports before the "
                "architecture is complete." % (depth, width, writer, reader)))

    if offences:
        say("")
        for code, where, msg in offences:
            say("check_v3_banks: %s  %s" % (code, where))
            say("    %s" % msg)
        say("")
        say("Section 0, final paragraph: this specification exists so that the next")
        say("agent cannot implement another large asynchronous table, call it a")
        say("transaction file, and declare the architecture complete.")
        return 1, lines_out

    return 0, lines_out


# ---------------------------------------------------------------------------
# FIRE TESTS.
#
# CLAUDE.md and this repository's tooling history: "A detector that has not been
# shown to FIRE has not been tested." Nine measuring tools were found wrong in
# one session and every one of them failed in the direction that made the answer
# look better. So each check has a deliberately-wrong fixture, and the gate
# refuses to run until every fixture has fired with the expected code.
#
# These run on EVERY invocation, in main(), not only under a test runner. A
# suite that has to be remembered is a suite that stops being run.
# ---------------------------------------------------------------------------
FIXDIR = "tools/rtl/fixtures/v3_banks"

FIXTURES = [
    ("good_bank.sv",             0, []),
    ("async_bank.sv",            1, ["V3-ASYNC"]),
    ("two_writer_bank.sv",       1, ["V3-MULTIWRITE"]),
    ("two_reader_bank.sv",       1, ["V3-MULTIREAD"]),
    ("logic_before_register.sv", 1, ["V3-COMBLOGIC"]),
    ("undersized_bank.sv",       1, ["V3-WIDTH", "V3-DEPTH"]),
    ("reset_payload.sv",         1, ["V3-RESETPAYLOAD"]),
    ("multidim_bank.sv",         1, ["V3-MULTIDIM"]),
    ("fabric_payload.sv",        1, ["V3-FABRIC"]),
    ("narrow_huge_payload.sv",   1, ["V3-FABRIC"]),
    ("unresolvable.sv",          2, []),
    ("no_banks.sv",              0, []),
    # The INSTANCE path, added after running against the real tree showed the
    # lane builds banks by instantiating one primitive rather than declaring
    # an array per bank. Every check above has a twin here or is shared.
    ("inst_good.sv",             0, []),
    ("inst_wrong_geometry.sv",   1, ["V3-WIDTH", "V3-DEPTH"]),
    # Checked in the ACCEPTANCE form, because that is where an unbound
    # bank instance is fatal. The default form must still PRINT it, which
    # the next entry asserts.
    ("inst_unbound.sv",          1, ["V3-UNBOUND"], True),
    ("inst_unbound.sv",          0, ["V3-UNBOUND", "[WARN]"], False),
    ("inst_no_geometry.sv",      2, []),
    ("inst_generate.sv",         0, []),
    ("array_multi_marker.sv",    2, []),
]


def _fixture_path(name):
    return os.path.join(FIXDIR, name).replace(os.sep, "/")


def _selftest(show=False):
    failures = []
    for entry in FIXTURES:
        name, want_rc, want_codes = entry[0], entry[1], entry[2]
        want_all = entry[3] if len(entry) > 3 else False
        path = _fixture_path(name)
        if not os.path.exists(os.path.join(REPO, path)):
            failures.append("%s: FIXTURE FILE MISSING -- the suite that proves this "
                            "gate can fire is not on disk" % name)
            continue
        rc, out = run([path], quiet=not show, require_all=want_all)
        text = "\n".join(out)
        if rc != want_rc:
            failures.append("%s: expected exit %d, got %d" % (name, want_rc, rc))
        for code in want_codes:
            if code not in text:
                failures.append("%s: expected %s in the output and it is not there"
                                % (name, code))
        if show:
            print("=" * 72)
            print("FIXTURE %s%s   (expect exit %d%s)"
                  % (name, " --require-all" if want_all else "", want_rc,
                     ", codes " + ",".join(want_codes) if want_codes else ""))
            print("=" * 72)
            print(text)
            print("-> exit %d" % rc)
            print("")
    if failures:
        raise SystemExit(
            "check_v3_banks SELF-TEST FAILED:\n  " + "\n  ".join(failures) +
            "\n\nThe detector cannot be trusted to report zero, because it has not "
            "been shown to report one. Fix the tool, not the fixtures.")


# `no_banks.sv` must be checked in --require-all form too, or the MISSING path
# is never exercised and could rot into silence.
def _selftest_require_all():
    rc, out = run([_fixture_path("no_banks.sv")], require_all=True, quiet=True)
    text = "\n".join(out)
    if rc != 1 or "V3-MISSING" not in text or text.count("V3-MISSING") != len(BANKS):
        raise SystemExit(
            "check_v3_banks SELF-TEST FAILED: --require-all on a file with no "
            "banks returned rc=%d with %d V3-MISSING lines; expected rc=1 and %d."
            % (rc, text.count("V3-MISSING"), len(BANKS)))


def _selftest_template_audit():
    r"""V3-TEMPLATE-GONE must be a detector that has been SEEN to fire.

    It is the guard on the hand-registered primitive list, and a hand list that
    silently stops matching is this repository's single most repeated tooling
    failure. It has already fired once for real, on a `\b` that a shell heredoc
    turned into a literal backspace -- see MODULE_DECL_RE.
    """
    paths = discover() or [_fixture_path("inst_good.sv")]
    BANK_TEMPLATES.append("zhao_texture_v3bank_that_does_not_exist")
    try:
        rc, out = run(paths, quiet=True, audit_templates=True)
    finally:
        BANK_TEMPLATES.pop()
    if rc != 1 or "V3-TEMPLATE-GONE" not in NL.join(out):
        raise SystemExit(
            "check_v3_banks SELF-TEST FAILED: the registered-template audit did "
            "not fire for a primitive that is not in the tree (rc=%d). The hand "
            "list is then unguarded, which is how a gate quietly checks nothing."
            % rc)
    # ... and it must NOT fire for the real list.
    rc, out = run(paths, quiet=True, audit_templates=True)
    if "V3-TEMPLATE-GONE" in NL.join(out):
        raise SystemExit(
            "check_v3_banks SELF-TEST FAILED: the template audit fires on the "
            "real registered list. Either a primitive was renamed and "
            "BANK_TEMPLATES/WRAPPER_TEMPLATES must follow, or the audit is "
            "broken:" + NL + NL.join(out))


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    verbose = "--list" in argv
    require_all = "--require-all" in argv
    show = "--fixtures" in argv

    _selftest(show=show)
    _selftest_require_all()
    _selftest_template_audit()
    if show:
        print("check_v3_banks: all %d fixtures fired as expected." % len(FIXTURES))
        return 0

    paths = args or discover()
    if not paths:
        print("check_v3_banks: NO V3 SOURCE FOUND (%s)" % ", ".join(V3_GLOBS))
        print("")
        print("The V3 texture-island lane has not landed any RTL. This gate is")
        print("ARMED AND WAITING, not passing: the section 6 bank table below is")
        print("what it will require the moment the first file appears.")
        print("")
        for name, depth, width, writer, reader in BANKS:
            print("  %-17s %4d x %-3d  writer=%-19s reader=%s"
                  % (name, depth, width, writer, reader))
        print("")
        print("Its %d fire-test fixtures all passed on this run, so a zero here is"
              % len(FIXTURES))
        print("an empty tree, not a broken instrument. Run with --fixtures to see")
        print("each deliberately-wrong file being caught.")
        return 0

    rc, out = run(paths, require_all=require_all, verbose=verbose,
                  # The template audit is meaningful only over the whole
                  # discovered V3 set; over one hand-named file, every
                  # primitive defined elsewhere would look deleted.
                  audit_templates=not args)
    for line in out:
        print(line)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
