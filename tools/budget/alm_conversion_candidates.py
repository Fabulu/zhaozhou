r"""Which zero-M10K blocks are worth converting to lookup, and which are not.

The owner's standing direction is to spend memory to buy ALMs. The roadmap
carries a candidate list to aim it: "51 fitted blocks report zero M10K, and
they carry 39,121 ALM between them." That list says of itself that it is "a
sorting aid, not a budget", and it is right to -- two of its named rows do not
survive being read:

  * `zhao_geom_skin` (2,225 ALM, 9 DSP) is three lanes of a full 32x32 signed
    multiply. A quarter-square decomposition needs SIXTEEN byte products per
    lane, and the measured exchange rate is TWO M10K per quarter-square site
    (commit dd5ab471, which found the one-M10K claim wrong by exactly a factor
    of two). 48 sites is 96 M10K against Geometry's 64 unspent -- over the
    domain envelope before the sixteen-term adder trees are counted.
  * `zhao_probe_dist_svc` (1,745 ALM, 0 DSP) is a PROBE. Its own header says
    "Field v3 decisive probe 3 ... a probe that misses kills or changes this
    topology BEFORE it contaminates the engine". Its ALMs are not in the
    console, so converting them saves nothing.

So this tool applies the two filters the list needs, and it exists because the
ALM ceiling is the binding constraint on closing the machine -- 40,591 against
a 30,000 target on a 41,910 device -- and work aimed by an unfiltered list is
work spent in the wrong place.

FILTER 1: IS IT IN THE MACHINE? A block nothing reachable from a production
root instantiates is not carrying ALMs in the console, however large its
standalone row. `module_graph.build` owns "who instantiates whom" and is reused
rather than reimplemented.

FILTER 2: WHICH WAY DOES THE TRADE GO? This is the one that matters and the one
the raw list inverts. Replacing a DSP-based multiply with a table does not
remove logic -- it removes a DSP and ADDS an adder tree, paying ALMs to save
DSPs. That is backwards here: DSP is at 63 of 85 on the composed shell and is
NOT the constraint; ALM is. The blocks worth converting are the ones whose
ALMs are COMBINATIONAL FUNCTION, not multiplier: high ALM with LOW DSP.

A block with high ALM and high DSP is therefore ranked DOWN, not up, which is
the opposite of what "highest DSP density with no memory at all" suggests.

This prints a ranking and a reason per row. It does not decide anything: an
M10K read is ~2 ns against a flip-flop's ~0.3, so a lookup on a block's
critical path can still cost more than it saves, and G8B T4 is the worked
example of exactly that. The ranking says where to LOOK.

HOW FILTER 1 WAS CHECKED, and what the check actually proved
------------------------------------------------------------
A filter that wrongly excludes a block shortens the work list, which is the
direction nobody audits, so it was run against a control: every module declared
in `zhao_shell_top_v2`'s 97-file fit target should come out LIVE.

Four did not -- `zhao_raster_rcp24_svc`, `zhao_raster_blend`,
`zhao_raster_quant` and `zhao_render_texture_layout_guard` -- and the walk was
right about all four. `zhao_raster_rcp24_svc` is instantiated only by
`zhao_texture_island_top.sv`, the V1 island; the V3 cone uses
`zhao_raster_rcp24_v4`. The other three have no instantiator anywhere.

So the CONTROL's premise was wrong, not the filter: a fit source list may
legitimately name files nothing elaborates. Quartus compiles only the top's
cone, so they cost nothing measured -- but they are dead entries inherited from
the list's derivation, and `zhao_raster_rcp24_svc` at 1,041 ALM is exactly the
kind of row that gets counted as a saving by somebody reading the source list
as if it were the design.

THE AUTHORITATIVE CONTROL is Quartus's own hierarchy report from the composed
fit, which names what actually elaborated. Compare against that when it exists;
a graph walk over text is evidence, not proof.
"""
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'tools', 'quartus'))

import module_graph  # noqa: E402

FIT = os.path.join(REPO, 'reports', 'synthesis', 'zhao_block_fit.json')

# Roots that mean "in the shipped machine". A block reachable from none of
# these is measured but not carried.
PRODUCTION_ROOTS = (
    'zhao_prod_top',
    'zhao_shell_top',
    'zhao_shell_top_v2',
)

# Directories whose blocks are instruments rather than console logic. A probe's
# ALMs are real and are not in the product.
NON_PRODUCT_DIRS = ('/synth/', '\\synth\\', '/generated/', '\\generated\\')


def reachable(decl, inst):
    """Every module reachable from a production root, by file."""
    by_module = {}
    for path, mods in inst.items():
        by_module.setdefault(path, set()).update(mods)

    seen = set()
    stack = [r for r in PRODUCTION_ROOTS if r in decl]
    while stack:
        mod = stack.pop()
        if mod in seen:
            continue
        seen.add(mod)
        path = decl.get(mod)
        if path is None:
            continue
        for child in by_module.get(path, ()):  # children declared in that file
            if child not in seen:
                stack.append(child)
    return seen


def main():
    rows = []
    doc = json.load(io.open(FIT, encoding='utf-8', errors='replace'))
    for b in doc['blocks']:
        for r in (b.get('rows') or b.get('runs') or [b]):
            mod = r.get('module')
            if not mod or '@' in mod:
                continue          # labelled rows are variants, not the block
            if r.get('ramBlocks') not in (0, None):
                continue
            if r.get('alms') is None:
                continue
            rows.append(r)

    decl, inst = module_graph.build(os.path.join(REPO, 'fpga', 'rtl'))
    live = reachable(decl, inst)

    scored = []
    for r in rows:
        mod = r['module']
        path = decl.get(mod, '')
        alm = r.get('alms') or 0
        dsp = r.get('dspBlocks') or 0

        if any(d in path for d in NON_PRODUCT_DIRS):
            why = 'INSTRUMENT -- not console logic'
            rank = -1
        elif mod not in live:
            why = 'not reachable from a production root'
            rank = -1
        elif dsp == 0:
            why = 'pure logic, no DSP to trade away'
            rank = alm
        else:
            # Each DSP displaced costs roughly a 16-term adder tree to rebuild
            # in fabric. Rank the block by the ALMs that are NOT multiplier.
            why = '%d DSP -- conversion pays ALM to save DSP' % dsp
            rank = alm - dsp * 150
        scored.append((rank, alm, dsp, mod, why))

    scored.sort(reverse=True)
    print('%-38s %7s %5s  %s' % ('block', 'ALM', 'DSP', 'verdict'))
    live_total = 0
    for rank, alm, dsp, mod, why in scored:
        if rank >= 0:
            live_total += alm
        print('%-38s %7d %5d  %s' % (mod[:38], alm, dsp, why))

    excluded = [s for s in scored if s[0] < 0]
    print()
    print('%d zero-M10K rows; %d are in the machine carrying %d ALM, '
          '%d excluded' % (len(scored), len(scored) - len(excluded),
                           live_total, len(excluded)))
    print('Excluded ALM does not count toward any saving:')
    for _rank, alm, _dsp, mod, why in excluded:
        print('   %-36s %7d  %s' % (mod[:36], alm, why))
    return 0


if __name__ == '__main__':
    sys.exit(main())
