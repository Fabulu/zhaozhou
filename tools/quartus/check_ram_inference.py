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
   the width of the array and pins it in flops whatever the writes do. This is
   TEXJOIN's second defect: `srgb_q[head_q][0]` in an `always_comb`.
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
   the writes are. The fix is one flat array per lane inside a `generate`, with
   the outer index a genvar.

   This rule is the reason the checker exists at all: it was blind to the exact
   construct that cost the fit it was written to prevent.

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
    """(start, joined-index) for every `name[...] <=` site, nesting allowed."""
    out = []
    for m in re.finditer(r"\b" + re.escape(name) + r"\s*(?=\[)", text):
        end, groups = bracket_groups(text, m.end())
        if groups and text[end:end + 2] == "<=":
            out.append((m.start(), "".join(groups)))
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


def check_file(path, sizes=None):
    raw = io.open(path, encoding="utf-8", errors="replace").read()
    text = strip_comments(raw)
    procs = processes(text)
    vals = local_params(text)
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
        # Writes: `name [i] <=` or `name [i][j] <=`, whitespace tolerated,
        # because a spaced index is the same construct and missing it would
        # make this check quietly useless.
        # Nesting-aware: see bracket_groups' header for the 63 arrays the old
        # regex silently skipped.
        sites = write_sites(text, name)
        writes = [p for p, _ in sites]
        if not writes:
            continue

        async_writes = 0
        write_addrs = set()
        for w, addr in sites:
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

    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    rank = "--rank" in sys.argv

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
            print("%8d bits  %s  %s  [%s]"
                  % (bits, path.replace("fpga/rtl/", ""), name, effort(whys)))
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
