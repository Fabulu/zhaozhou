#!/usr/bin/env python3
"""Will these arrays infer as memory? Answered statically, in a second.

WHY THIS EXISTS
---------------
On 2026-09-03 a texture cache rebuild was reported as a PASS at 98.66 MHz while
9,728 bits of array sat in flip-flops. The cause was one construct: the arrays
were written from an `always_ff @(posedge clk or negedge rst_n)`, and an M10K
has no reset port, so an array touched by an asynchronously-reset process
cannot be one.

It cost an 85-minute fit to find, and the same defect was already documented in
a comment inside the block being replaced -- `zhao_texture_cache.sv:495-523`,
which records the A/B measurement and says the async reset was the blocker.

This check is that knowledge made mechanical. It is not a substitute for the
fit; only the fitter knows what the tool did. It is a way to stop spending an
hour discovering something a grep can see.

WHAT IT LOOKS FOR, and why each one
-----------------------------------
1. AN ARRAY WRITTEN FROM AN ASYNC-RESET PROCESS. The killer above.
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
        wpat = re.compile(r"\b" + name + r"\s*(?:\[[^\]]*\]\s*)+<=")
        writes = [mm.start() for mm in wpat.finditer(text)]
        if not writes:
            continue

        async_writes = 0
        write_addrs = set()
        for w in writes:
            o = owner(procs, w)
            if o and o[1] == "ff" and o[2]:
                async_writes += 1
            frag = text[w:w + 200]
            idx = re.match(r"\b" + name + r"\s*((?:\[[^\]]*\]\s*)+)<=", frag)
            if idx:
                write_addrs.add(idx.group(1).strip())

        if async_writes:
            findings.append(
                (name, "written from an ASYNC-RESET process (%d site%s) -- an "
                       "M10K has no reset port, so this array cannot be one"
                 % (async_writes, "" if async_writes == 1 else "s")))

        # Combinational read through a dynamic index.
        rpat = re.compile(r"\b" + name + r"\s*\[\s*([A-Za-z_]\w*)")
        decl_lo, decl_hi = arrays[name][0], arrays[name][1]
        for mm in rpat.finditer(text):
            if decl_lo <= mm.start() < decl_hi:
                continue  # the declaration, not a read
            o = owner(procs, mm.start())
            after = text[mm.start():mm.start() + 300]
            is_write = re.match(r"\b" + name + r"\s*(?:\[[^\]]*\]\s*)+<=", after)
            if is_write:
                continue
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
    design = False
    for w in whys:
        if "read COMBINATIONALLY" in w or "read at MODULE SCOPE" in w:
            design = True
        if "TWO OR MORE distinct write addresses" in w:
            design = True
    return "DESIGN" if design else "MECHANICAL"


def main():
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
