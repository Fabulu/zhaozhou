#!/usr/bin/env python3
"""Refresh the PORT BLOCK of every `zhao_console_core` wrapper mutant from
production.

WHY THIS EXISTS
---------------
`tests/mutants/zhao_console_core_*_mutant.sv` are WRAPPERS, not copies: each
declares the core's parameter and port blocks, mutates ONE parameter, and binds
production with `u_dut (.*)`. That shape cannot drift in its body -- but it goes
SHORT the moment the core's edge moves, because `.*` cannot bind a port the
wrapper does not declare, and it breaks in the shape that reads as "the core is
broken" (owner ruling R220).

`tools/design/wrapper_port_parity.py` is the gate that catches it. This is the
REPAIR, and it exists because the repair had been done by hand every time:
each wrapper's port block had accumulated its own maintenance commentary --
"Fourteen inputs left this list with the core's", "wrapper parity only", "FIRED
on all 33 of these before they were added" -- so the two files' port blocks were
no longer the verbatim copies their own headers claim they are, and the next
refresh was a manual diff against a 3,000-line block. Packet TWODCMD moved 84
ports at once (50 out, 34 in) and that was the second time in four days.

Losing that commentary is the intended trade. A note explaining why a port is
declared here belongs on the PORT IN PRODUCTION, where every reader sees it; a
note explaining that wrappers must track the core belongs in the file header and
in the gate's own message, which both say it. What was left in the middle was a
changelog of past refreshes, in the one region that must be byte-identical to
something else.

WHAT IT TOUCHES
---------------
`) (` through the closing `);`, and nothing else. The PARAMETER block carries
each wrapper's own mutation -- `GEOM_REPLAY_UNTEX_DECL = 1`, the slot-overflow
capacity -- and is never written. The header, the instantiation and the
`endmodule` are never written.

It is deliberately NOT registered as a ctest. A tool that silently rewrites a
positive control is not something a suite should run; `wrapper_port_parity`
reports the problem and a person runs this to fix it. Exit 0 if nothing moved,
so it is safe to run twice.

  python tools/design/refresh_core_wrapper_ports.py [--check]

--check rewrites nothing and exits 1 if any wrapper's port block differs, which
is the same verdict `wrapper_port_parity.py` reaches by comparing port NAMES.
This one compares the TEXT, so it also catches a width or a comment that moved.
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

CORE = os.path.join(ROOT, 'fpga', 'rtl', 'prod', 'zhao_console_core.sv')
WRAPPERS = [
    os.path.join(ROOT, 'tests', 'mutants', 'zhao_console_core_untex_decl_mutant.sv'),
    os.path.join(ROOT, 'tests', 'mutants', 'zhao_console_core_slot_overflow_mutant.sv'),
]

OPEN = ') ('
CLOSE = ');'


def port_block(path):
    """Return (raw, newline, lines, start, end) for a file's port block.

    `start` indexes the `) (` that closes the parameter block; `end` indexes
    the `);` that closes the port list. Both are matched at column zero, which
    is how every file here writes them, and an unmatched one is a hard error
    rather than a guess -- a tool that silently edits the wrong region of a
    positive control is worse than one that stops.
    """
    raw = io.open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in raw else '\n'
    lines = raw.replace('\r\n', '\n').split('\n')

    opens = [i for i, ln in enumerate(lines) if ln == OPEN]
    if len(opens) != 1:
        raise SystemExit('%s: expected exactly one %r line, found %d'
                         % (path, OPEN, len(opens)))
    start = opens[0]

    closes = [i for i in range(start, len(lines)) if lines[i] == CLOSE]
    if not closes:
        raise SystemExit('%s: no %r closing the port list' % (path, CLOSE))
    end = closes[0]

    return raw, nl, lines, start, end


def main(argv):
    check_only = '--check' in argv[1:]

    _, _, core_lines, cs, ce = port_block(CORE)
    core_ports = core_lines[cs:ce + 1]
    print('zhao_console_core: port block is %d lines (%d..%d)'
          % (len(core_ports), cs + 1, ce + 1))

    differ = 0
    for path in WRAPPERS:
        name = os.path.basename(path)
        _, nl, lines, ms, me = port_block(path)
        have = lines[ms:me + 1]
        if have == core_ports:
            print('  %-46s already verbatim (%d lines)' % (name, len(have)))
            continue
        differ += 1
        if check_only:
            print('  %-46s DIFFERS: %d lines here, %d in production'
                  % (name, len(have), len(core_ports)))
            continue
        out = lines[:ms] + list(core_ports) + lines[me + 1:]
        io.open(path, 'w', encoding='utf-8', newline='').write(nl.join(out))
        print('  %-46s refreshed: %d -> %d lines'
              % (name, len(have), len(core_ports)))

    if check_only and differ:
        print('\n%d wrapper(s) out of step with production. Run without '
              '--check to refresh, then rebuild the positive controls -- a '
              'control that does not elaborate is a control that cannot fail.'
              % differ)
        return 1
    if differ:
        print('\nRebuild the positive controls and re-run '
              'tools/design/wrapper_port_parity.py.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
