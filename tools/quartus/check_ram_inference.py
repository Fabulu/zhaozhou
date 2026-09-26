#!/usr/bin/env python3
"""Will these arrays infer as memory? Answered statically, in a second.

WHY THIS EXISTS
---------------
On 2026-09-03 a texture cache rebuild was reported as a PASS at 98.66 MHz while
9,728 bits of array sat in flip-flops. The cause was one construct: the arrays
were written from an `always_ff @(posedge clk or negedge rst_n)`, and an M10K
has no reset port. That diagnosis was RIGHT for that block and the rule
drawn from it was too strong -- see rule 1 below, downgraded 2026-09-04
against two fits that infer perfectly well from an async-reset process.

It cost an 85-minute fit to find, and the same defect was already documented in
a comment inside the block being replaced -- `zhao_texture_cache.sv:495-523`,
which records the A/B measurement and says the async reset was the blocker.

This check is that knowledge made mechanical. It is not a substitute for the
fit; only the fitter knows what the tool did. It is a way to stop spending an
hour discovering something a grep can see.

WHAT IT LOOKS FOR, and why each one
-----------------------------------
1. AN ARRAY WRITTEN FROM AN ASYNC-RESET PROCESS.
   **DOWNGRADED 2026-09-04 -- THIS RULE HAS MEASURED FALSE POSITIVES.**

   It used to say "an M10K has no reset port, so this array cannot be one",
   and two recorded fits say otherwise:

       zhao_texture_palette_res  d656521  M10K 2  16,384 bits  (clean=True)
       zhao_raster_tilestore     96c0394a M10K 4  32,768 bits

   16,384 is exactly `mem_r`; 32,768 is exactly `ram0 + ram1`. Both arrays
   were fully in block memory while being written from an async-reset
   process, because the ARRAY itself was never reset. Quartus 17 infers
   that fine.

   The rule was generalised from the texture cache, where inference DID
   fail -- but that block was ALSO multidimensional, and rule 4 is what
   actually blocked it: 128 bits of block memory and an explicit "cannot
   regroup multidimensional array". Two defects appeared together and the
   wrong one was blamed.

   It is kept because it is still worth LOOKING at -- a reset process is
   where the fatal cases have been found -- but it is a hint, not a
   verdict, and on its own it does not predict failure.
2. AN ARRAY READ COMBINATIONALLY through a dynamic index. Forces a per-bit mux
   the width of the array. This is TEXJOIN's second defect: `srgb_q[head_q][0]`
   in an `always_comb`.

   "WHATEVER THE WRITES DO" USED TO BE THE REST OF THIS SENTENCE, AND IT IS
   MEASURED FALSE. Removed 2026-09-26 by FLOPARRAY, which mapped the four arms
   of `tests/probes/zhao_floparray_probe.sv` against `zhao_forge_assemble`'s
   34,840-bit vertex store:

       arm  reset loop  read   registers  mem bits
       p0      yes      comb      34,917         0   <- production, the control
       p1      NO       comb         245    34,840   <- INFERRED M10K
       p2      yes      reg       34,984         0
       p3      NO       reg           77    34,840

   Arm p1 keeps the combinational read in full and still infers a Simple Dual
   Port M10K, because the read address was ALREADY REGISTERED a cycle ahead of
   its use and Quartus absorbed that register into the RAM's own read port.
   The asynchronous reset loop was the entire cause. So rule 2 is a NECESSARY-
   CONDITION HINT collected from real failures, not a mechanism: a
   combinational read is a reason to look, and on its own it is not a reason to
   believe the array cannot infer.

   Note the direction of the error, which is why it survived: the old sentence
   was ALARMING rather than flattering. It costs work that need not be done --
   FLOPARRAY kept the registered read anyway, for 168 registers, but it is an
   improvement rather than the repair the rule implied was mandatory.

   The counter-example does NOT generalise to a read whose address is itself
   combinational. `zhao_geom_lodstate`'s `st_q[slot_c]` read-modify-write is
   exactly that case, and there the two properties are a genuine conjunction:
   neither removing the reset loop nor registering the read moves a single bit
   on its own (arms l0-l3, same probe).
3. TWO DYNAMIC WRITE ADDRESSES into one array, which the island brief's S5.3
   forbids by name -- it asks for a two-write-port memory, which the device
   does not have at that shape.
4. A MULTIDIMENSIONAL unpacked array, `[LANES][N]`. Added 2026-09-03 after this
   checker called `zhao_texture_cache_pipe` clean and an 88-minute fit came
   back with 2 M10K and 128 memory bits anyway. Synthesis was explicit:

       EDA Netlist Writer cannot regroup multidimensional array "data_r"

   with no "Inferred RAM" line for it at all. Quartus cannot map a memory whose
   OUTER selection is dynamic; it builds a mux across every element of the
   outer dimension and the whole array falls into flip-flops, however correct
   the writes are.

   **THIS RULE'S REMEDY USED TO READ "one flat array per lane inside a
   `generate`, with the outer index a genvar", AND THAT IS MEASURED WRONG.**
   Corrected 2026-09-26 by ARENAINFER. `zhao_geom_arenabin` followed exactly
   that advice -- fourteen flat one-dimensional arrays inside a generate-for --
   and every one of them went to flip-flops, 145,152 bits of them. See rule 6:
   the generate FOR-LOOP is itself a killer. The remedy is one flat array per
   lane AT A MODULE'S SCOPE -- its own module, instantiated inside the loop --
   and `zhao_dc_sdp_ram` is that module.

   This rule is the reason the checker exists at all: it was blind to the exact
   construct that cost the fit it was written to prevent. Its remedy then sent
   the next block into a different one, which is the same lesson one level up:
   ADVICE IN A TOOL IS A CLAIM, and this one had never been measured.
6. AN ARRAY DECLARED INSIDE A `generate for` BLOCK.

   Added 2026-09-26 by ARENAINFER. An array whose declaration sits inside a
   genvar-indexed generate block is NOT A RAM CANDIDATE for Quartus 17.0.2 --
   AT ONE ITERATION, with every other property held fixed.

   Nine map_only rows on 5CSEBA6U23I7, one variable per arm, one bank of
   576 x 18 (tests/probes/zhao_arenabin_stage_probe.sv, table in
   reports/synthesis/arenabin/STAGE-PROBE-ROWS.txt):

       production, verbatim (generate-FOR)   10,386 reg        0 bits  CONTROL
       read address as its own net           10,386 reg        0 bits
       write enable without the genvar cmp   10,386 reg        0 bits
       read in its own always_ff             10,386 reg        0 bits
       ramstyle = "no_rw_check"              10,386 reg        0 bits
       declared at MODULE SCOPE                   0 reg   10,368 bits
       declared in a generate-IF, no loop         0 reg   10,368 bits
       a SUBMODULE instance inside the loop       0 reg   18,432 bits

   IT IS THE LOOP, NOT "A GENERATE". A generate-`if` scope infers perfectly.
   This matters because rule 4's remedy sends you straight into it, and
   because the shape carries NONE of QUARTUS_GOTCHAS section 10's other three
   killers -- the read is synchronous, nothing resets the array, the element
   is written whole. It is the killer a clean block can have.

   The remedy is a submodule: put the array at some module's scope and
   instantiate that module inside the loop. On the real block that took
   `zhao_geom_arenabin` from 146,414 registers / 33,408 memory bits to
   1,010 / 291,456, same device, same 6 DSP, same 1,305 virtual pins, with
   the map itself falling 1,025.7 s -> 37.1 s.

   SCOPE AND ITS LIMIT, STATED. The detector walks begin/end nesting and asks
   whether the declaration sits inside a `for`-opened block within a
   `generate` region and outside any procedural block. An `always_ff` written
   WITHOUT begin/end inside a generate would leave its procedural flag set
   until the next `generate`/`endgenerate` keyword, which can only make this
   rule MISS, never cry wolf -- stated because that is the flattering
   direction and this file's own header is about instruments that fail
   quietly.

It is deliberately CONSERVATIVE about what counts as an array: only unpacked
declarations with a depth, since those are what become memories. A packed
vector is a register file by construction and is not the subject.

RANKING, and why an unranked list is nearly useless
---------------------------------------------------
`--rank` added 2026-09-04, after the first full-tree run returned 898 findings.
That number is true and unusable: most of them are two-entry control arrays
that are CORRECTLY in flops, and a `state [0:1]` sits in the list beside a
9,728-bit cache line store looking exactly as important.

So each finding is sized. The estimate resolves `localparam`s declared in the
same file and simple arithmetic over them; anything it cannot resolve is
reported as UNKNOWN and listed separately rather than guessed at, because a
confident wrong size is worse than an admitted gap -- it would send the next
pass at the wrong file.

The size is on the COMPARISON side: it says which findings are worth looking
at. It does not decide that an array should be a memory. A 4,096-bit array that
is read on three ports every cycle belongs in flops and no ranking should
suggest otherwise.
"""
import io
import os
import re
import sys

# `logic [W-1:0] name [DEPTH];` or `logic name [A][B];` -- an UNPACKED array.
DECL = re.compile(
    r"^\s*(?:logic|reg|bit)\s*(?:signed\s+)?(?:\[[^\]]*\]\s*)*"
    r"([A-Za-z_]\w*)\s*((?:\[[^\]]*\]\s*)+);", re.M)

PROC = re.compile(r"always_(ff|comb|latch)\s*(?:@\(([^)]*)\))?")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def processes(text):
    """(start, kind, has_async_reset) for every always_* block, in order."""
    out = []
    for m in PROC.finditer(text):
        sens = m.group(2) or ""
        out.append((m.start(), m.group(1), "negedge" in sens or "posedge rst" in sens))
    return out


def owner(procs, pos):
    """Which process contains `pos`? None means module scope."""
    best = None
    for p in procs:
        if p[0] < pos:
            best = p
        else:
            break
    return best


NUM = re.compile(r"^\s*-?\d+$")


def local_params(text):
    """{name: int} for localparams/parameters this file can resolve itself."""
    vals = {}
    pat = re.compile(
        # Stop at a NEWLINE as well as at ; and , -- a module parameter has
        # no terminator of its own, so `[^;,]+` swallowed the entire port
        # list after it and every PARAMETERISED array came back UNKNOWN.
        r"\b(?:localparam|parameter)\s+(?:\w+\s+)*?([A-Za-z_]\w*)\s*=\s*([^;,\n]+)")
    # Two passes, so a localparam defined in terms of an earlier one resolves.
    for _ in range(3):
        for m in pat.finditer(text):
            name, expr = m.group(1), m.group(2)
            v = eval_expr(expr, vals)
            if v is not None:
                vals[name] = v
    return vals


def eval_expr(expr, vals):
    """Evaluate a width/depth expression, or None. Deliberately narrow: only
    integers, known identifiers, + - * / and parentheses. Anything else is
    UNKNOWN rather than a guess."""
    e = expr.strip()
    e = re.sub(r"\b\d+'[sS]?[dD]([0-9_]+)", r"\1", e)   # 8'd12 -> 12
    e = re.sub(r"\b(?:int|unsigned)'\s*\(", "(", e)      # int'(x) -> (x)
    e = re.sub(r"\$clog2\s*\(", "clog2(", e)
    if not re.fullmatch(r"[\w\s()+\-*/]*", e):
        return None
    def clog2(x):
        n, r = 1, 0
        while n < x:
            n, r = n * 2, r + 1
        return r
    env = dict(vals)
    env["clog2"] = clog2
    try:
        v = eval(e, {"__builtins__": {}}, env)
    except Exception:
        return None
    return int(v) if isinstance(v, (int, float)) and v == int(v) else None


def array_bits(decl_widths, decl_depths, vals):
    """Total declared bits, or None if any dimension will not resolve."""
    bits = 1
    for d in decl_widths + decl_depths:
        inner = d.strip()[1:-1]
        if ":" in inner:
            hi, lo = inner.split(":", 1)
            h, l = eval_expr(hi, vals), eval_expr(lo, vals)
            if h is None or l is None:
                return None
            n = abs(h - l) + 1
        else:
            n = eval_expr(inner, vals)
            if n is None:
                return None
        if n <= 0:
            return None
        bits *= n
    return bits


def bracket_groups(text, pos):
    """Consume balanced `[...]` groups starting at `pos`. Returns (end, groups).

    WHY THIS IS NOT A REGEX, measured 2026-09-09
    --------------------------------------------
    The write scan used `(?:\\[[^\\]]*\\]\\s*)+`, which cannot match a NESTED
    bracket -- and a nested bracket is the normal case here, because an index is
    usually a slice of something:

        sampmeta_m[plan_acc_src[SRC_SLOT_HI:SRC_SLOT_LO]]
                  [plan_acc_src[SRC_SIDX_LO+1:SRC_SIDX_LO]] <=

    `[^\\]]*` stops at the INNER `]`, so the group closes early, the leftover `]`
    matches nothing, and the write site is missed entirely. `writes` then comes
    back empty and `if not writes: continue` skips that array from EVERY rule in
    this file -- not merely the write rules.

    **63 arrays across fpga/rtl were invisible to this checker**, including
    `sampmeta_m` (4,032 bits, which FIT GATE 1 independently measured sitting in
    flip-flops), `mat_m`, `palslot_m`, `palgen_m`, every one of v3own's eight
    queues, cache_pipe's five, fragrob's five, and this session's own
    perspuv_pairpipe FIFO arrays.

    The tool printed "1009 structural findings" and read as thorough while
    silently skipping the largest arrays in the design. That is the
    broken-instrument law exactly: the defect made the answer look SMALLER, and
    nobody audits good news. It is also the third parser in this repository to be
    defeated by a bracket -- see worst_path_index.py, which gave up on regex
    entirely and splits columns instead.
    """
    groups = []
    i = pos
    n = len(text)
    while i < n and text[i] == "[":
        depth = 0
        j = i
        while j < n:
            if text[j] == "[":
                depth += 1
            elif text[j] == "]":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if j >= n:
            return (i, groups)          # unbalanced: refuse to guess
        groups.append(text[i:j + 1])
        i = j + 1
        while i < n and text[i] in " \t\r\n":
            i += 1
    return (i, groups)


def write_sites(text, name):
    """(start, joined-index, groups) for every `name[...] <=` site, nesting ok.

    The GROUPS are returned as a list as well as joined, because rule 5 needs to
    count them: on an array with one unpacked dimension, a SECOND bracket group
    is a bit- or part-select of the element rather than another dimension, and
    that is a killer of storage inference in its own right.

    Note this matches `<=` only. Blocking writes are deliberately out of scope:
    a blocking assignment to an array in an `always_comb`, or a continuous
    `assign`, is combinational and has no storage to infer -- rule 2 is what
    covers the damage those do. Measured 2026-09-26 while adding rule 5: of 28
    two-bracket sites in fpga/rtl, 8 were `=` or `assign` (ch_pack, dstep_sat,
    same_c, seen_n, rt_n_valid) and every one of them would have been noise.
    """
    out = []
    for m in re.finditer(r"\b" + re.escape(name) + r"\s*(?=\[)", text):
        end, groups = bracket_groups(text, m.end())
        if groups and text[end:end + 2] == "<=":
            out.append((m.start(), "".join(groups), groups))
    return out


# A KNOWN-BAD INPUT, checked on every run.
#
# The old regex passes the flat case and fails the nested one, so a self-check
# built only from flat writes would have gone on reporting a pass forever. Both
# shapes are here, and the nested one is the one that mattered.
_NEST_FIRE = (
    "  logic [20:0] sampmeta_m [64][3];\n"
    "  always_ff @(posedge clk) begin\n"
    "    sampmeta_m[src[HI:LO]]\n"
    "              [src[LO+1:LO]] <= v;\n"
    "    flat_m[i][j] <= w;\n"
    "  end\n"
)


def self_fire_test():
    """True if the write scan still sees BOTH a nested and a flat write."""
    nested = write_sites(_NEST_FIRE, "sampmeta_m")
    flat = write_sites(_NEST_FIRE, "flat_m")
    return len(nested) == 1 and len(flat) == 1 and "[src[HI:LO]]" in nested[0][1]


# RULE 5's POSITIVE CONTROL, and its negative one beside it.
#
# "A detector that has not been shown to FIRE has not been tested." Rule 5 is
# the rule most likely to be written once and never exercised, because the
# construct it catches is rare -- and it is also the rule most likely to be
# written TOO BROADLY, because the obvious version of it collides head-on with
# rule 4. So both directions are asserted on every run:
#
#   pal_q    one unpacked dimension, element written through a part-select.
#            Rule 5 MUST fire. This is zhao_geom_drawjob's shape before the
#            2026-09-26 repair, reduced to four lines.
#   twodim_m two unpacked dimensions, element written WHOLE at [i][j]. Rule 5
#            must NOT fire -- that second bracket is a dimension, and rule 4
#            already owns it. ~175 sites in fpga/rtl have this shape, so a
#            rule 5 that fires here is a rule nobody will read.
_RULE5_FIRE = (
    "  logic [383:0] pal_q [XFORMS];\n"
    "  logic [31:0] twodim_m [LANES][DEPTH];\n"
    "  always_ff @(posedge clk) begin\n"
    "    for (int unsigned k = 0; k < 12; k++)\n"
    "      pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];\n"
    "    twodim_m[lane_q][slot_q] <= payload_c;\n"
    "  end\n"
)


def rule5_fire_test():
    """True if rule 5 fires on the part-select shape and NOT on [i][j]."""
    fired = {n for n, why in check_file_text(_RULE5_FIRE)
             if "written through a bit/part-select" in why}
    return "pal_q" in fired and "twodim_m" not in fired


# RULE 6'S STRUCTURE SCAN. See rule 6 in the header for the nine rows.
GEN_TOK = re.compile(
    r"\b(generate|endgenerate|begin|end|for|always_ff|always_comb|always_latch"
    r"|always|initial|function|task)\b")


def genfor_spans(text):
    """Character ranges that lie inside a genvar-indexed `generate for` block.

    Comment-stripped text only. The scan is structural rather than regex-shaped
    because the question is about NESTING, and a pattern that matched
    `for (...) begin` would answer it for the first bank and not for an array
    declared three lines further down.

    `end` is matched with a word boundary, so `endcase`, `endgenerate`,
    `endmodule`, `endfunction` and `endtask` do not pop the stack -- which is
    what lets `case ... endcase` inside a loop body be ignored for free.
    """
    spans = []
    in_gen = 0
    stack = []          # one entry per open `begin`: True if it is a generate-for
    opens = []          # start offsets of the currently open generate-for blocks
    proc_depth = None   # stack depth at which the current procedural block began
    pending_for = False
    for m in GEN_TOK.finditer(text):
        t = m.group(1)
        if t == "generate":
            in_gen += 1
            proc_depth = None      # a generate keyword cannot be inside a process
        elif t == "endgenerate":
            in_gen = max(0, in_gen - 1)
            proc_depth = None
        elif t in ("always_ff", "always_comb", "always_latch", "always",
                   "initial", "function", "task"):
            proc_depth = len(stack)
        elif t == "for":
            pending_for = (in_gen > 0 and proc_depth is None)
        elif t == "begin":
            stack.append(pending_for)
            if pending_for:
                opens.append(m.end())
            pending_for = False
        elif t == "end":
            if stack:
                if stack.pop() and opens:
                    spans.append((opens.pop(), m.start()))
            if proc_depth is not None and len(stack) <= proc_depth:
                proc_depth = None
    # An unterminated block (a file this scan cannot balance) is reported as
    # reaching the end of the text rather than silently dropped: a span that is
    # too LONG produces a visible false alarm, a dropped one produces silence.
    while opens:
        spans.append((opens.pop(), len(text)))
    return spans


# RULE 6'S POSITIVE CONTROL AND ITS TWO NEGATIVE ONES.
#
#   loop_q   inside a generate FOR -- rule 6 MUST fire. This is
#            zhao_geom_arenabin's shape before the 2026-09-26 repair.
#   gif_q    inside a generate IF -- it must NOT fire; arm 8 measured this
#            shape inferring at 10,368 bits, so firing here would be a lie.
#   flat_q   module scope, with a PROCEDURAL for-loop in an always_ff between
#            it and the generate -- it must NOT fire. This is the case the
#            structure scan exists to get right; a `for` inside a process is
#            not a generate loop.
_RULE6_FIRE = (
    "  logic [17:0] flat_q [576];\n"
    "  always_ff @(posedge clk) begin\n"
    "    for (int unsigned k = 0; k < 4; k++) begin\n"
    "      flat_q[wa] <= wd;\n"
    "    end\n"
    "  end\n"
    "  genvar gs;\n"
    "  generate\n"
    "    if (EN) begin : g_if\n"
    "      logic [17:0] gif_q [576];\n"
    "      always_ff @(posedge clk) begin\n"
    "        gif_q[wa] <= wd;\n"
    "      end\n"
    "    end\n"
    "    for (gs = 0; gs < 14; gs = gs + 1) begin : g_stage\n"
    "      logic [17:0] loop_q [576];\n"
    "      always_ff @(posedge clk) begin\n"
    "        loop_q[wa] <= wd;\n"
    "      end\n"
    "    end\n"
    "  endgenerate\n"
)


def rule6_fire_test():
    """True if rule 6 fires ONLY on the generate-for declaration."""
    fired = {n for n, why in check_file_text(_RULE6_FIRE)
             if "generate FOR-LOOP" in why}
    return fired == {"loop_q"}


def check_file(path, sizes=None):
    raw = io.open(path, encoding="utf-8", errors="replace").read()
    return check_file_text(raw, sizes)


def check_file_text(raw, sizes=None):
    """The body of check_file, over TEXT rather than a path.

    Split out 2026-09-26 so rule 5's positive control can run the real rule
    against a known-bad snippet instead of a reimplementation of it. A control
    that exercises a copy of the logic proves nothing about the logic that ships.
    """
    text = strip_comments(raw)
    procs = processes(text)
    vals = local_params(text)
    gfspans = genfor_spans(text)
    findings = []

    arrays = {}
    for m in DECL.finditer(text):
        name = m.group(1)
        # Only unpacked dimensions with a real depth are memory candidates.
        if m.group(2).strip():
            # Remember the declaration's SPAN, not just its start: the read
            # scan below would otherwise match the declaration itself --
            # `logic arr_q [DEPTH];` looks exactly like a read indexed by
            # `DEPTH`. A tool with obvious false positives teaches people to
            # ignore its real findings.
            widths = re.findall(r"\[[^\]]*\]", m.group(0)[:m.group(0).index(name)])
            depths = re.findall(r"\[[^\]]*\]", m.group(2))
            arrays[name] = (m.start(), m.end(), widths, depths)
            if sizes is not None:
                sizes[name] = array_bits(widths, depths, vals)

    for name in sorted(arrays):
        # 6. DECLARED INSIDE A `generate for` BLOCK.
        #
        # Checked FIRST and BEFORE the write scan's `continue`, because this
        # one is a property of the DECLARATION and is fatal on its own. Putting
        # it after `if not writes: continue` would have hidden it for any array
        # whose write the scan cannot see -- which is exactly the compounding
        # blindness this file's header is about.
        d_lo = arrays[name][0]
        if any(lo <= d_lo < hi for lo, hi in gfspans):
            findings.append(
                (name, "DECLARED INSIDE A generate FOR-LOOP -- not a RAM "
                       "candidate for Quartus 17.0.2, at one iteration, with "
                       "every other property clean. A generate-IF scope is "
                       "fine; the LOOP is the killer. Put the array at a "
                       "module's scope -- its own module, instantiated inside "
                       "the loop -- as zhao_dc_sdp_ram is. Measured: "
                       "zhao_geom_arenabin 146,414 reg / 33,408 bits -> "
                       "1,010 / 291,456."))

        # Writes: `name [i] <=` or `name [i][j] <=`, whitespace tolerated,
        # because a spaced index is the same construct and missing it would
        # make this check quietly useless.
        # Nesting-aware: see bracket_groups' header for the 63 arrays the old
        # regex silently skipped.
        sites = write_sites(text, name)
        writes = [p for p, _, _ in sites]
        if not writes:
            continue

        async_writes = 0
        write_addrs = set()
        for w, addr, _groups in sites:
            o = owner(procs, w)
            if o and o[1] == "ff" and o[2]:
                async_writes += 1
            write_addrs.add(addr.strip())

        if async_writes:
            findings.append(
                (name, "written from an ASYNC-RESET process (%d site%s) -- WEAK "
                       "SIGNAL, measured false positives; see the header"
                 % (async_writes, "" if async_writes == 1 else "s")))

        # Combinational read through a dynamic index.
        rpat = re.compile(r"\b" + name + r"\s*\[\s*([A-Za-z_]\w*)")
        decl_lo, decl_hi = arrays[name][0], arrays[name][1]
        for mm in rpat.finditer(text):
            if decl_lo <= mm.start() < decl_hi:
                continue  # the declaration, not a read
            o = owner(procs, mm.start())
            after = text[mm.start():mm.start() + 300]
            # The read scan had the SAME nesting blindness, and it fails the
            # other way: a nested-index WRITE was not recognised as a write, so it
            # was reported as a combinational READ. Use the write sites already
            # computed above rather than re-deriving with a pattern.
            if mm.start() in set(writes):
                continue
            del after
            if o and o[1] == "comb":
                findings.append(
                    (name, "read COMBINATIONALLY through dynamic index `%s` -- "
                           "forces a per-bit mux the width of the array"
                     % mm.group(1)))
                break
            if o is None:
                findings.append(
                    (name, "read at MODULE SCOPE through dynamic index `%s` "
                           "(a continuous assignment is combinational)"
                     % mm.group(1)))
                break

        # 4. Multidimensional unpacked array.
        #
        # Read from the DECLARATION rather than from use, because that is where
        # the shape is: `logic [15:0] m [LANES][N];` has two unpacked
        # dimensions and cannot be a memory whatever the accesses look like.
        decl_text = text[arrays[name][0]:arrays[name][1]]
        unpacked_dims = re.findall(r"\[[^\]]*\]", decl_text[decl_text.index(name) + len(name):])
        if len(unpacked_dims) > 1:
            findings.append(
                (name, "MULTIDIMENSIONAL unpacked array %s -- Quartus cannot "
                       "regroup this into a memory; it muxes across the outer "
                       "dimension and the whole array becomes flip-flops. One "
                       "flat array per element inside a generate, outer index a "
                       "genvar." % "".join(unpacked_dims)))

        if len(write_addrs) > 1:
            findings.append(
                (name, "TWO OR MORE distinct write addresses %s -- island brief "
                       "S5.3 forbids this by name; it asks for a memory shape "
                       "the device does not have"
                 % sorted(write_addrs)))

        # 5. THE ELEMENT IS WRITTEN THROUGH A BIT- OR PART-SELECT.
        #
        # Added 2026-09-26 by the PALRAM packet, which found `pal_q` in
        # `zhao_geom_drawjob` sitting in 98,304 flip-flops while THIS CHECKER
        # REPORTED IT CLEAN. It passed all four rules above and was still the
        # single largest array in flops in the design -- 60% of the shipping
        # part's register capacity in one leaf.
        #
        # The construct, measured by tests/probes/zhao_palram_probe.sv at one
        # change per map:
        #
        #     for (int unsigned k = 0; k < 12; k++)
        #       pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];   0 mem bits
        #     pal_q[int'(px_index_i)] <= whole_word_c;            98,304 mem bits
        #
        # THIS IS NOT A NEW DISCOVERY, WHICH IS THE POINT. QUARTUS_GOTCHAS.md
        # section 10 established it on purpose in August with a 102-bench
        # calibration grid: "an ASYNCHRONOUS READ, a RESET on the array, or BYTE
        # ENABLES -- ZERO memory bits. The three conditions kill inference
        # INDEPENDENTLY." This file encoded the first two (rules 2 and 1) and
        # never encoded the third. zhao_surface_sheet.sv:164 then paid 131,258
        # REGISTERS and an estimated 229% of the device for exactly this
        # template, wrote the post-mortem in its own header, and nothing read it
        # back. A killer that is documented and unencoded is a killer that gets
        # rewritten -- which is this repository's own law about uncashed
        # cheques, arriving inside its own checker.
        #
        # HOW HARD TO BELIEVE IT, stated in rule 1's spirit rather than repeating
        # rule 1's mistake. Two sub-cases, both measured:
        #
        #   * a write with a PER-SLICE ENABLE (a true byte enable) infers
        #     NOTHING on this device, ever. Section 10's grid, 102 benches.
        #     The remedy is surface_sheet's: one array per plane, each written
        #     whole under its own enable. That is a DESIGN change, not a rewrite
        #     of one line.
        #   * an UNGATED set of part-selects covering the whole element can
        #     still infer -- probe arm 2 does, at 98,304 bits -- PROVIDED the
        #     element index is exactly the array's address width. Combined with
        #     an over-wide index expression (`int'()` of a wider port) it does
        #     not. drawjob was that conjunction.
        #
        # So this finding is A REASON TO LOOK, and the array's size decides
        # whether it is worth the look -- which is what --rank is for. A sweep
        # of fpga/rtl at the commit this was added found no array anywhere that
        # is written through an element part-select AND still infers, so the
        # measured false-positive rate on this tree is zero; but "no innocents
        # today" is not "no innocents possible", and arm 2 is the proof that an
        # innocent shape exists.
        #
        # Scoped deliberately:
        #   * ONE unpacked dimension only. On `arr [A][B]` the second bracket is
        #     a dimension, not a select, and that is rule 4's business -- firing
        #     here too would double-count ~175 sites in this tree.
        #   * NON-BLOCKING writes only (see write_sites). A blocking write or a
        #     continuous assign is combinational; rule 2 owns that damage.
        if len(unpacked_dims) == 1:
            selects = []
            for w, _addr, groups in sites:
                if len(groups) > 1:
                    o = owner(procs, w)
                    if o and o[1] == "ff" and groups[1] not in selects:
                        selects.append(groups[1])
            if selects:
                findings.append(
                    (name, "the ELEMENT is written through a bit/part-select %s "
                           "-- QUARTUS_GOTCHAS section 10's third killer. A "
                           "per-slice-enabled (byte-enable) write infers NOTHING "
                           "on this device; an ungated one infers only while the "
                           "element index is exactly the array's address width. "
                           "Assemble the word combinationally and write the "
                           "element WHOLE, or split it into one array per plane."
                     % ", ".join("`%s`" % s for s in selects[:3])))

    return findings


def effort(whys):
    """MECHANICAL or DESIGN. The distinction that decides who does the work.

    Added 2026-09-04 after the ranked report called `zhao_forge_cliff`
    "single-reason ... should be mechanical" and it was not: its array is read
    COMBINATIONALLY by a dozen same-cycle consumers, so registering that read
    moves the whole state machine a cycle. Three arrays were fixed mechanically
    in an hour that night and the fourth would have been a pipeline restructure
    at two in the morning.

    MECHANICAL means every reason is one of the two that a port move fixes on
    its own -- an async-reset process, or a multidimensional shape -- with no
    behaviour change and the block's own tests as the guard. Three such fixes
    took 216,704 bits out of flip-flops without a single output byte moving.

    DESIGN means at least one reason needs a decision:
      * a COMBINATIONAL READ has to become a registered one, which adds a clock
        that every consumer sees;
      * MULTIPLE WRITE ADDRESSES may or may not be genuine. On
        zhao_terrain_residency_v2 they turned out to be mutually exclusive per
        way once the multidimensional shape was gone -- one defect producing
        two findings -- so this tag says "look", not "rewrite".
    """
    # An async reset ALONE no longer implies anything is wrong: two blocks
    # flagged only for it were measured fully in block memory on 2026-09-04.
    if len(whys) == 1 and "ASYNC-RESET" in whys[0]:
        return "UNPROVEN"
    design = False
    for w in whys:
        if "read COMBINATIONALLY" in w or "read at MODULE SCOPE" in w:
            design = True
        if "TWO OR MORE distinct write addresses" in w:
            design = True
    return "DESIGN" if design else "MECHANICAL"


def composed_index(map_rpt):
    """module name -> (own registers, subtree memory bits) in a COMPOSED map.

    ADDED 2026-09-26, BECAUSE THIS FILE'S OWN `--rank` LIST IS A BAD WORK LIST
    AND NOBODY HAD CHECKED IT AGAINST A COMPOSITION. Its top six entries on
    2026-09-26 were:

        zhao_terrain_patch_acc      139,776 bits   NOT IN THE COMPOSED CONSOLE
        zhao_forge_cliff            119,808 bits   NOT IN THE COMPOSED CONSOLE
        zhao_forge_cliff_ram        119,808 bits   already 55,428 MEMORY BITS
        zhao_audio_fifo              65,536 bits   already 65,536 MEMORY BITS
        zhao_post_composite          64,512 bits   already 64,512 MEMORY BITS
        zhao_texture_binding_res_v2  38,912 bits   already 38,912 MEMORY BITS

    **Every one of them is either absent from the machine or ALREADY INFERRING.**
    This checker reads SOURCE with DECLARED parameters; it cannot know what the
    composition instantiated or what Quartus then did. Its silence is not a
    verdict (that is rule 5's lesson) and its NOISE is not a work list either.

    So a ranked row can now be annotated with what the composed console actually
    shows for that module, and a reader can tell "this is 100k flip-flops in the
    machine" from "this is a parameter in a file nothing instantiates".
    """
    idx = {}
    try:
        with open(map_rpt, "r", encoding="utf-8", errors="replace") as fh:
            started = False
            for line in fh:
                if not started:
                    if line.startswith("; Compilation Hierarchy Node"):
                        started = True
                    continue
                if not line.startswith(";"):
                    if idx:
                        break
                    continue
                cells = [c.strip() for c in line.split(";")]
                if len(cells) < 9 or not cells[1].startswith("|"):
                    continue
                mod = cells[1].strip("|").split(":")[0]
                own = re.search(r"\((\d+)\)", cells[3])
                mem = re.match(r"^(\d+)", cells[4])
                if mod and mod not in idx:
                    idx[mod] = (int(own.group(1)) if own else 0,
                                int(mem.group(1)) if mem else 0)
    except OSError:
        return {}
    return idx


def main():
    # The write scan is this file's foundation: an array whose write it cannot see
    # is skipped from every rule, silently. It was defeated by a nested bracket for
    # its whole life, so it now proves it can see both shapes before it is allowed
    # to report anything.
    if not self_fire_test():
        print("RAM-INFERENCE CHECKER BROKEN: the write scan no longer sees a "
              "nested-index write, or no longer sees a flat one. Refusing to "
              "report findings from a scan that cannot find writes -- an array "
              "whose write is missed is dropped from every rule below without a "
              "word.")
        return 2

    # Rule 5 owes the same proof as the write scan, in BOTH directions: it must
    # fire on the construct it was written for, and stay silent on the
    # multidimensional shape rule 4 owns. A rule that has never been watched to
    # fire is an assertion, not an instrument.
    if not rule5_fire_test():
        print("RAM-INFERENCE CHECKER BROKEN: rule 5 no longer fires on an "
              "element part-select write, or it now fires on a whole-element "
              "write into a multidimensional array. Refusing to report: a "
              "silent rule 5 reads exactly like a design with no part-select "
              "writes, which is how zhao_geom_drawjob kept 98,304 flip-flops "
              "while this file called it clean.")
        return 2

    # Rule 6 owes the same proof in THREE directions, because its two ways of
    # being wrong are opposite: a generate-IF is innocent (measured: it infers)
    # and a PROCEDURAL for-loop is innocent, so a rule that fired on either
    # would bury the real finding under the ~175 generate blocks in this tree.
    if not rule6_fire_test():
        print("RAM-INFERENCE CHECKER BROKEN: rule 6 no longer fires on an "
              "array declared inside a generate FOR-LOOP, or it now fires on a "
              "generate-IF or on a procedural for. Refusing to report: this is "
              "the rule that was ABSENT while zhao_geom_arenabin held 145,152 "
              "bits in flip-flops and this file reported five other arrays -- "
              "all five of which inferred -- and said nothing about that one.")
        return 2

    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    rank = "--rank" in sys.argv
    against = ""
    for a in sys.argv[1:]:
        if a.startswith("--against="):
            against = a.split("=", 1)[1]
    args = [a for a in args if not a.startswith("--against=")]
    composed = composed_index(against) if against else {}

    targets = args
    if not targets:
        targets = []
        for root, _d, names in os.walk("fpga/rtl"):
            for n in names:
                if n.endswith(".sv"):
                    targets.append(os.path.join(root, n).replace(os.sep, "/"))

    total = 0
    ranked = []      # (bits, path, name, [reasons])
    unknown = []     # (path, name, [reasons])
    for path in sorted(targets):
        sizes = {}
        found = check_file(path, sizes)
        if not found:
            continue
        total += len(found)
        if rank:
            byname = {}
            for name, why in found:
                byname.setdefault(name, []).append(why)
            for name, whys in byname.items():
                bits = sizes.get(name)
                if bits is None:
                    unknown.append((path, name, whys))
                else:
                    ranked.append((bits, path, name, whys))
            continue
        print(path.replace("fpga/rtl/", ""))
        for name, why in found:
            print("    %-16s %s" % (name, why))

    if rank:
        # An M10K is 10,240 bits (8,192 usable at common widths). An array
        # below a few hundred bits cannot pay for one however it is written,
        # so the cut is where the finding starts being worth an edit.
        ranked.sort(reverse=True)
        print("ARRAYS THAT WILL NOT INFER AS MEMORY, LARGEST FIRST")
        print("(size is DECLARED bits -- what an M10K would have to hold. It")
        print(" says which findings are worth looking at, and nothing more:")
        print(" a large array read on three ports every cycle belongs in flops")
        print(" and no ranking should suggest otherwise.)")
        print()
        shown = 0
        for bits, path, name, whys in ranked:
            if bits < 256:
                continue
            shown += 1
            note = ""
            if composed:
                mod = os.path.basename(path)[:-3]
                if mod not in composed:
                    note = "   <- NOT IN THE COMPOSED MAP"
                else:
                    own, mem = composed[mod]
                    note = ("   <- composed: %d own reg, %d mem bits%s"
                            % (own, mem,
                               "  ALREADY INFERRING" if mem >= bits else ""))
            print("%8d bits  %s  %s  [%s]%s"
                  % (bits, path.replace("fpga/rtl/", ""), name, effort(whys), note))
            for w in whys:
                print("               - %s" % w)
        small = sum(1 for b, _p, _n, _w in ranked if b < 256)
        print()
        print("%d array(s) at or above 256 bits; %d below it, not shown -- those "
              "are control state that is CORRECTLY in flops and would drown the "
              "list." % (shown, small))
        if unknown:
            print()
            print("%d array(s) whose size would not resolve from the file alone. "
                  "Reported rather than guessed: a confident wrong size sends the "
                  "next pass at the wrong file." % len(unknown))
            for path, name, _w in unknown[:20]:
                print("               ? %s  %s" % (path.replace("fpga/rtl/", ""), name))
        return 0

    print()
    if total:
        print("%d structural finding(s). Not all are defects -- a small array "
              "in flops can be deliberate -- but each one is a reason an array "
              "will NOT become an M10K. Re-run with --rank to see them by SIZE, "
              "which is the only way this list is actionable." % total)
    else:
        print("no structural obstacles to memory inference found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
