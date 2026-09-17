r"""The harness's fault OR and the shell's fault OR must name the same faults.

`zhao_shell_v2_lease_path.sv` composes the lease path so the directed test can
drive it. Its `bin_fault_w` is a COPY of `zhao_shell_top_v2.sv`'s
`v2_fault_level_c` -- the same list of structural faults, written twice, in two
files, by hand.

This repository has a chapter about exactly that and it is not optimistic.
Thirteen committed mutant copies went stale while staying GREEN, because their
mutations were intact and the body around them was two weeks old. A copy does
not report that it is stale.

The failure here has a specific and nasty shape. If the shell gains a fault
term and the harness does not, the harness composes a machine whose fault
attribution is WEAKER than the one that ships -- and every directed check goes
on passing, because nothing in them looks at the list. The gate clause reads
"five structural faults each through the reset barrier"; a harness measuring
four of five answers it in the flattering direction.

WHAT IT CHECKS. The operands of each OR, by name, with the `v2_bin_` / `bin_`
and `_o` / `_w` decorations stripped so the two spellings compare. It does not
check that the faults MEAN the same thing -- they are different instances of
different modules -- only that neither list has a term the other lacks.

This is a text comparison of two expressions, deliberately. A tool that
elaborated the design could be wrong about it; this one can only be wrong about
text, and the drift being guarded against is textual.
"""
import io
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

SHELL = REPO / 'fpga/rtl/common/zhao_shell_top_v2.sv'
HARNESS = REPO / 'tests/shell/zhao_shell_v2_lease_path.sv'

# `assign <name> = a || b || c;` across as many lines as it takes.
ASSIGN = r'assign\s+%s\s*=(.*?);'

# The two files spell the same fault differently, which is why this is a
# normalisation rather than a set comparison of raw identifiers.
#   shell:    v2_bin_raster_abort_w   v2_cdc_gpu_fault_w
#   harness:  bin_raster_abort_o      cdc_gpu_protocol_fault_o
ALIASES = {
    'cdc_gpu_fault': 'cdc_gpu_protocol_fault',
    'bin_attr_abort': 'attr_abort',
    'bin_frame_fault': 'frame_fault',
    'bin_lifetime_fault': 'lifetime_fault',
    'bin_raster_abort': 'raster_abort',
    'bin_sequence_mismatch': 'sequence_mismatch',
}

# Terms that legitimately appear on ONE side, each with its reason. A term
# listed here is exempt from parity; anything else is drift.
EXEMPT = {
    'fault_inject': 'the harness drives faults the shell has no port for -- '
                    'it is the stimulus, not a fault of the composed machine',
}


def normalise(name):
    name = re.sub(r'^v2_', '', name)
    name = re.sub(r'_[owc]$', '', name)
    name = re.sub(r'_valid_i$', '', name)
    for k, v in ALIASES.items():
        if name == k:
            return v
    return name


def operands(path, signal):
    text = io.open(path, encoding='utf-8', errors='replace').read()
    m = re.search(ASSIGN % re.escape(signal), text, re.S)
    if not m:
        return None
    body = re.sub(r'//[^\n]*', '', m.group(1))
    return {normalise(t) for t in re.findall(r'[A-Za-z_]\w*', body)}


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    # Overridable so the controls can fire this at synthetic files rather than
    # by editing production RTL, which is a live-tree hazard and leaves no
    # evidence behind.
    ap.add_argument('--shell', default=str(SHELL))
    ap.add_argument('--harness', default=str(HARNESS))
    args = ap.parse_args(argv)
    SHELL_P, HARNESS_P = Path(args.shell), Path(args.harness)

    shell = operands(SHELL_P, 'v2_fault_level_c')
    harness = operands(HARNESS_P, 'bin_fault_w')

    for label, got, sig in (('shell', shell, 'v2_fault_level_c'),
                            ('harness', harness, 'bin_fault_w')):
        if got is None:
            print('FAIL: no `assign %s` found in the %s.' % (sig, label))
            print('  The expression was renamed or restructured. This tool')
            print('  cannot silently pass when it cannot find what it checks.')
            return 1

    exempt = set(EXEMPT)
    only_shell = sorted(shell - harness - exempt)
    only_harness = sorted(harness - shell - exempt)

    print('shell   fault OR: %d terms  %s' % (len(shell), ', '.join(sorted(shell))))
    print('harness fault OR: %d terms  %s' % (len(harness), ', '.join(sorted(harness))))
    for name, why in sorted(EXEMPT.items()):
        if name in shell or name in harness:
            print('exempt: %s -- %s' % (name, why))

    if not only_shell and not only_harness:
        print()
        print('packet-h fault OR parity: PASS -- both name the same %d faults'
              % len(shell - exempt))
        return 0

    print()
    print('== THE TWO FAULT LISTS DISAGREE ==')
    for n in only_shell:
        print('  only the SHELL has  %s' % n)
    for n in only_harness:
        print('  only the HARNESS has %s' % n)
    print()
    print('  The harness is a copy of the shell\'s attribution. A term in one')
    print('  and not the other means the directed test measures a machine')
    print('  that does not ship -- and it measures it GREEN, because no check')
    print('  reads the list. Add the term to both, or exempt it with a reason.')
    return 1


# SELF-CHECK. A parser that finds nothing reports two empty sets, which compare
# equal, which PASSES. That is this repository's most-repeated failure mode and
# it would be load-bearing here, so prove the extractor bites.
assert normalise('v2_bin_raster_abort_w') == 'raster_abort'
assert normalise('cdc_gpu_protocol_fault_o') == 'cdc_gpu_protocol_fault'
assert normalise('v2_cdc_gpu_fault_w') == 'cdc_gpu_protocol_fault'
assert operands(SHELL, 'v2_fault_level_c'), 'shell OR not found'
assert operands(HARNESS, 'bin_fault_w'), 'harness OR not found'
assert operands(SHELL, 'no_such_signal_xyz') is None

if __name__ == '__main__':
    sys.exit(main())
