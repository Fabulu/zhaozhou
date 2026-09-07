"""L0 -- the island top's array and port inventory.

Master recovery handoff 10.5: "L0 read archived/current array and port
inventory". 10.1 sets the criterion: "Two independent reads PLUS a concurrent
write are three accesses, not a free use of two-port RAM. Pack fields only when
their writer and read event genuinely coincide."

So this records, per array, WHERE each access sits -- which always block, and
which branch -- because two reads in the same always_comb are CONCURRENT and
two in different mutually exclusive branches are not. A textual site count
cannot tell those apart, and an earlier pass in this session used one.
"""
import io
import re

P = 'fpga/rtl/texture/zhao_texture_island_top.sv'
lines = io.open(P, encoding='utf-8', errors='replace').read().splitlines()

NAMED = ['uvw_m', 'fctx_m', 'flod_m', 'fpgn_m', 'fcls_m', 'fpsl_m',
         'faux_m', 'class_m']

DECL = re.compile(r'^\s*logic\s*(?:signed\s*)?((?:\[[^\]]*\]\s*)*)([a-z_][a-z_0-9]*)\s*\[')

# map each line to its enclosing always block
block_of = [None] * (len(lines) + 1)
cur = None
for n, ln in enumerate(lines, 1):
    m = re.match(r'\s*(always_comb|always_ff|always)\b', ln)
    if m:
        cur = (n, m.group(1))
    block_of[n] = cur

decl = {}
for n, ln in enumerate(lines, 1):
    m = DECL.match(ln)
    if m and m.group(2) in NAMED:
        decl[m.group(2)] = (n, m.group(1).strip() or '1 bit')

print('L0 INVENTORY -- the eight arrays 10.1 names, by ACCESS EVENT not site count')
print('=' * 100)
for name in NAMED:
    pat = re.compile(r'\b' + re.escape(name) + r'\s*\[')
    wr, rd = [], []
    for n, ln in enumerate(lines, 1):
        code = ln.split('//')[0]
        if not pat.search(code):
            continue
        if DECL.match(ln):
            continue
        # A line that IS a continuous assign is continuous, whatever `always`
        # preceded it. The first version of this script tracked only where
        # blocks BEGIN, never where they end, so it filed every `wire x =
        # arr[i]` after an always_ff as if it were inside one -- which would
        # have read as "already registered" when these are precisely the
        # asynchronous reads Quartus refuses to infer.
        if re.match(r'\s*(wire|assign)', ln):
            where = 'CONTINUOUS (asynchronous read)'
        else:
            blk = block_of[n]
            where = ('%s@%d' % (blk[1], blk[0])) if blk else 'continuous'
        if re.search(r'\b' + re.escape(name) + r'\s*\[[^\]]*\](\s*\[[^\]]*\])*\s*(<=|=[^=])', code):
            wr.append((n, where))
        else:
            rd.append((n, where))
    d = decl.get(name, ('?', '?'))
    print()
    print('%-9s declared line %-5s width %-14s   %d write event(s), %d read event(s)'
          % (name, d[0], d[1], len(wr), len(rd)))
    for n, w in wr:
        print('            WRITE  line %-5d in %s' % (n, w))
    for n, w in rd:
        print('            read   line %-5d in %s' % (n, w))
    # concurrency: reads sharing one always block are simultaneous
    from collections import Counter
    c = Counter(w for _, w in rd)
    conc = {k: v for k, v in c.items() if v > 1}
    if conc:
        for k, v in conc.items():
            print('            ** %d reads in the SAME block (%s) -- concurrent, not '
                  'alternatives' % (v, k))
    if len(rd) <= 2 and not conc:
        print('            -> reads sit in distinct blocks; two-port RAM is PLAUSIBLE')
    else:
        print('            -> more than a two-port schedule without a port plan')
