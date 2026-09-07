#!/usr/bin/env python3
"""check_seam_widths.py -- do the two ends of a declared seam actually fit?

WHY THIS EXISTS
---------------
D22's remaining work is wiring eighteen geometry blocks together, and the first
seam checked by hand -- GEOM.SETUP -> zhao_geom_bin_pipe -- turned out to match
on every name AND every width, needing no glue at all. That was worth knowing
before writing any RTL, and there are seventeen more seams nobody has looked at.

`compose_order.py` already checks that a declared edge is JUSTIFIED: that the
two rows name each other and share a signal name in the ledger's prose
vocabulary. It says nothing about the RTL. Two blocks can agree in
`design/blocks.yml` and disagree by a bit.

WHAT IT MATCHES ON, AND WHY IT IS A HEURISTIC
---------------------------------------------
Port names carry a role prefix and a direction suffix that differ per block:

    zhao_geom_setup     out_kx0_o
    zhao_geom_bin_pipe  tri_kx0_i

Both mean "kx0". So a port is reduced to its STEM by dropping the trailing
`_o`/`_i` and, if what remains still has an underscore, an optional leading
`<word>_`. Stems are then matched across the seam.

That is a guess about naming, not a fact about the design, so:

  * a matched stem with EQUAL widths is reported as a fit -- useful, and the
    thing that made the SETUP seam a one-line job;
  * a matched stem with DIFFERENT widths is the finding worth having, because
    it is a real cast or a real bug and both want deciding before wiring;
  * an unmatched stem is reported quietly, because the heuristic missing a
    rename is far more likely than a genuinely absent signal.

It REPORTS. It does not gate, and it never edits. A seam whose two ends use
unrelated vocabularies is not thereby broken -- it just cannot be checked this
way, which is itself worth printing.
"""
from __future__ import annotations

import io
import os
import re
import sys

PORT_RE = re.compile(
    r"^\s*(input|output)\s+(?:var\s+)?(?:[\w:]+\s+)*?(?:signed\s*)?"
    r"((?:\[[^\]]*\]\s*)*)(\w+)\s*(?:\[[^\]]*\]\s*)*\s*(?:[,);])?\s*(?://.*)?$"
)

# Any line that declares a port, however it is written. If PORT_RE reads fewer
# of these than this finds, the parser is DROPPING ports -- and a dropped port
# is a seam this tool silently calls "fits exactly". Three sibling tools each
# carried a version of that bug (scoped types `pkg::t`, unpacked arrays
# `x_o [7]`, and a final port with no trailing comma), and every one of them
# made the answer look BETTER than it was.
PORT_LINE_RE = re.compile(r"^\s*(?:input|output)\s")
assert PORT_LINE_RE.match("    output var logic [31:0] x_o,"), "dead self-check"

UNPARSED = []


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def find_modules():
    mods = {}
    for root, _d, files in os.walk("fpga/rtl"):
        for f in files:
            if f.endswith(".sv"):
                mods[f[:-3]] = os.path.join(root, f)
    return mods


def ports_of(path, module):
    s = read(path)
    i = s.find("module " + module)
    if i < 0:
        return [], []
    seg = s[i:]
    j = seg.find("\n);")
    if j > 0:
        seg = seg[:j]
    ins, outs = [], []
    for line in seg.splitlines():
        m = PORT_RE.match(line)
        if not m:
            if PORT_LINE_RE.match(line):
                UNPARSED.append((path, line.strip()))
            continue
        direction, width, name = m.group(1), (m.group(2) or "").strip(), m.group(3)
        (ins if direction == "input" else outs).append((name, width or "logic"))
    return ins, outs


def norm(width: str) -> str:
    """Whitespace inside a range is not a difference. `[ 7:0]` and `[7:0]` are
    the same bus, and reporting them as a mismatch buries the real ones."""
    return re.sub(r"\s+", "", width)


def parameterised(width: str) -> bool:
    """A width mentioning an identifier cannot be compared textually: SRCW-1:0
    IS 15:0 when SRCW is 16, and calling that a mismatch is a false alarm of
    exactly the kind that gets a tool ignored. Reported separately instead."""
    return bool(re.search(r"[A-Za-z_]", width.replace("logic", "")))


def bits(width: str):
    """Bit count of a simple `[hi:lo]`, or 1 for a bare `logic`. None if it
    cannot be read literally."""
    w = norm(width)
    if w in ("", "logic"):
        return 1
    m = re.match(r"^\[(\d+):(\d+)\]$", w)
    if not m:
        return None
    return int(m.group(1)) - int(m.group(2)) + 1


def stems(name: str) -> set:
    """Every name this port could reasonably be matched by.

    A SET, NOT ONE STRING, AND THAT IS THE 2026-09-07 REPAIR. The previous
    version returned a single stem: strip `_o`/`_i`, then drop the leading
    `head_` whenever one existed and was short. It could not tell a ROLE prefix
    (`v_`, `req_`, `cfg_`) from the first word of a NAME, so it dropped both --
    and a producer and consumer that disagree about whether they carry a prefix
    stopped matching:

        v_src_id_o  ->  strip _o  ->  v_src_id  ->  drop "v"    ->  "src_id"
        src_id_i    ->  strip _i  ->  src_id    ->  drop "src"   ->  "id"

    Two different stems for the same signal. That pair is a REAL NARROWING --
    TERRAIN.PAGESTREAM carries T5's 32-bit `source_id` and TERRAIN.PATCH takes
    16 -- and this tool reported the seam as fitting exactly, because the two
    ports never met. It was found by hand instead, which is precisely the miss
    this file's own footer warns is "far likelier than an absent signal".

    So both readings are offered and a match on EITHER counts. That trades more
    stem collisions for fewer misses, which is the correct direction for a tool
    whose failure mode is silence: a collision is a row a reader can dismiss by
    looking at the port names, and the header already tells them to. A miss is
    a row nobody ever sees.
    """
    n = name
    if n.endswith("_o") or n.endswith("_i"):
        n = n[:-2]
    out = {n}
    if "_" in n:
        head, rest = n.split("_", 1)
        if rest and len(head) <= 6:
            out.add(rest)
    return out


def edges_from_ledger():
    s = read("design/blocks.yml")
    out = []
    for chunk in re.split(r"\n  - id: ", s)[1:]:
        bid = chunk.split("\n", 1)[0].strip()
        m = re.search(r"^    downstream: \[(.*?)\]", chunk, re.M)
        if not m:
            continue
        for d in [x.strip() for x in m.group(1).split(",") if x.strip()]:
            out.append((bid, d))
    return out


def module_for(bid, mods):
    cand = "zhao_" + bid.lower().replace(".", "_")
    return cand if cand in mods else None


def main() -> int:
    only = [a for a in sys.argv[1:] if not a.startswith("-")]
    mods = find_modules()
    fits, mismatches, unpaired, unresolved, widenings, skipped = [], [], [], [], [], 0

    for up, dn in edges_from_ledger():
        if only and up not in only and dn not in only:
            continue
        mu, md = module_for(up, mods), module_for(dn, mods)
        if not mu or not md:
            skipped += 1
            continue
        _ui, uo = ports_of(mods[mu], mu)
        di, _do = ports_of(mods[md], md)
        # One port now contributes SEVERAL keys, so the same signal can be
        # reached from either naming. `setdefault` keeps the first port that
        # claimed a key, which is the same tie-break the single-stem version
        # had.
        prod = {}
        for n, w in uo:
            for k in stems(n):
                prod.setdefault(k, (n, w))
        cons = {}
        for n, w in di:
            for k in stems(n):
                cons.setdefault(k, (n, w))
        # A PORT PAIR, NOT A KEY, IS THE UNIT. With several keys per port the
        # same producer/consumer pair can now be reached twice (once by the
        # full name, once by the prefix-dropped one) and would otherwise be
        # reported and counted twice. Deduplicate on the pair of port NAMES,
        # which is what a reader is actually looking at.
        seen_pairs = set()
        shared = []
        for k in sorted(set(prod) & set(cons)):
            pair = (prod[k][0], cons[k][0])
            if pair in seen_pairs:
                continue
            seen_pairs.add(pair)
            shared.append(k)
        if not shared:
            unpaired.append((up, dn, len(uo), len(di)))
            continue
        bad, para = [], []
        for s in shared:
            pw, cw = norm(prod[s][1]), norm(cons[s][1])
            if pw == cw:
                continue
            if parameterised(pw) or parameterised(cw):
                para.append((s, prod[s], cons[s]))
            else:
                pb, cb = bits(pw), bits(cw)
                # A WIDENING cannot lose data. Only a narrowing can, and for a
                # SIGNED port it does not even clip -- it wraps. Reporting the
                # two together is what made the first run of this tool look
                # like nine findings when it had one.
                if pb is not None and cb is not None and pb < cb:
                    widenings.append((up, dn, s, prod[s], cons[s]))
                else:
                    bad.append((s, prod[s], cons[s]))
        if para:
            unresolved.append((up, dn, para))
        if bad:
            mismatches.append((up, dn, bad, len(shared)))
        else:
            fits.append((up, dn, len(shared)))

    print("seam widths: %d seam(s) fit exactly, %d with a REAL width "
          "NARROWING, %d harmless widening(s), %d with a parameterised width "
          "this tool cannot compare, %d share no recognisable stem, %d skipped "
          "(no module file)"
          % (len(fits), len(mismatches), len(widenings), len(unresolved),
             len(unpaired), skipped))

    if mismatches:
        print("\nNARROWING -- the producer is WIDER than the consumer, so bits "
              "are lost. For a SIGNED port that does not clip, it WRAPS.\n"
              "BEWARE STEM COLLISION: two unrelated signals can share a stem "
              "(`status`, `slot`, `w`), and most rows below are exactly that. "
              "Read the port NAMES before believing a row:")
        for up, dn, bad, n in mismatches:
            print("  %s -> %s  (%d stems shared)" % (up, dn, n))
            for s, (pn, pw), (cn, cw) in bad:
                print("      %-12s %-22s %-10s  ->  %-22s %s"
                      % (s, pn, pw, cn, cw))

    if fits:
        print("\nFIT EXACTLY -- every shared stem agrees on width. These seams "
              "need no glue:")
        for up, dn, n in fits:
            print("  %-24s -> %-24s %d stem(s)" % (up, dn, n))

    if unresolved:
        print("\nPARAMETERISED, NOT COMPARABLE -- one side states its width "
              "with an identifier. `[SRCW-1:0]` IS `[15:0]` when SRCW is 16, so "
              "these are NOT reported as differences:")
        for up, dn, para in unresolved:
            for s, (pn, pw), (cn, cw) in para:
                print("  %-22s -> %-22s %-10s %-20s -> %-20s %s"
                      % (up, dn, s, pw, cn, cw))

    if unpaired:
        print("\nNO SHARED STEM (%d) -- the two ends use unrelated port "
              "vocabularies, so this tool cannot check them. Not a defect."
              % len(unpaired))

    if UNPARSED:
        print("\nPARSER DROPPED %d PORT DECLARATION(S) it could not read. A "
              "dropped port is a signal this tool cannot compare, and a seam "
              "missing half its signals is reported as FITTING. Every number "
              "above is therefore optimistic until this is zero:" % len(UNPARSED))
        for path, line in UNPARSED[:10]:
            print("  %-30s %s" % (os.path.basename(path), line[:64]))
        if len(UNPARSED) > 10:
            print("  ... and %d more" % (len(UNPARSED) - 10))
    else:
        print("\nparser read every port line it met -- no silent drops.")

    print("\nHEURISTIC: stems are guessed by stripping a direction suffix and a "
          "short role prefix. A missed match is far likelier than an absent "
          "signal. Reports; does not gate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
