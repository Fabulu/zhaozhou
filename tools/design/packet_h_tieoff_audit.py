r"""Every literal in the Packet-H composition must say why it is there.

The Packet-H gate says *every new port is connected*. The trouble with checking
that by eye is that a port map entry reading `(1'b0)` and one reading
`(some_net)` look equally deliberate, and the first is how an input nobody
decided about ships as a decision. CLAUDE.md's phrasing is exact: a tie-off that
looks deliberate is worse than a missing connection, because a missing one is a
`PINMISSING` and a tie-off is silence.

So this audit reads the composed harness, finds every instantiation port map,
and splits the connections into three kinds:

  * connected to a NET or PORT  -- fine, nothing to say;
  * connected to a LITERAL with a `// TIE:` reason on the same line or the line
    above -- a declared decision;
  * connected to a LITERAL with no reason -- an ERROR, and the only thing this
    tool exists to find.

It is deliberately dumb about SystemVerilog: it does not elaborate, it does not
resolve parameters, and it does not care what the literal's value is. A tool
that understood the design could be wrong about it. This one can only be wrong
about text, and the text is the thing being audited.

Run it; it takes about a second. `packet_h_tieoff_audit` is the ctest.
"""
import io
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Both Packet-H compositions. The harness is where the protocol is DRIVEN; the
# sibling shell is where it SHIPS, and the shell is the one the gate clause is
# actually about -- the harness is audited too because a tie-off learned there
# is the one most likely to be transplanted.
TARGETS = [
    REPO / 'tests/shell/zhao_shell_v2_lease_path.sv',
    REPO / 'fpga/rtl/common/zhao_shell_top_v2.sv',
]

# `.port (connection)` -- EVERY one on the line, not just a line that holds
# exactly one. The original pattern was anchored `^...$`, so a port map packing
# several connections onto one line was invisible to it. That is not a
# hypothetical: `zhao_shell_top_v2.sv` writes
# `.local_attribute_abort_o(), .local_fault_pulse_o(),` on a single line, and
# the shell -- the composition the gate clause is actually about -- reported
# "0 silent" while two discarded faults sat on that line.
#
# The leading `(?:^|[\s,])` is what keeps a nested call out: in `.a(f(x))` the
# inner `f(` is preceded by `(`, so it is not mistaken for a port named `f`.
CONN = re.compile(r'(?:^|[\s,])\.(\w+)\s*\(([^()]*)\)')
TRAILING = re.compile(r'//\s*(.*)$')

# A literal: sized (16'd0, 2'b10, 84'h0), unsized ('0, '1), or a bare number.
LITERAL = re.compile(r"^\s*(?:\d+\s*'\s*[bdho]?\s*[0-9a-fA-FxXzZ_]+|'[01]|\d+)\s*$")

INSTANCE = re.compile(r'^\s*(\w+)\s+(u_\w+)\s*\($')

# An EMPTY connection is audited only when the port is a FAULT, and this
# narrowness is the point. Demanding a reason for all 89 unread outputs of the
# bin pipe would mean 89 reasons saying "telemetry", and a field everybody
# fills in with the same word is one nobody reads -- the tool would have
# manufactured its own silence.
#
# Discarding `texture_cache_hits_o` loses a number. Discarding
# `local_fault_pulse_o` loses an EVENT, and the shell's fault attribution is
# the thing the gate clause asks about. `_count_o` is excluded because a count
# is the telemetry form of a fault, not the fault: the pulse beside it is what
# the OR needs.
FAULT_PORT = re.compile(
    r'(fault|abort|error|overflow|underflow|mismatch|violation)', re.I)


def is_fault_port(port):
    return bool(FAULT_PORT.search(port)) and not port.endswith('_count_o')


def audit(text):
    lines = text.split('\n')
    rows = []
    inst = None
    for i, line in enumerate(lines):
        m = INSTANCE.match(line)
        if m:
            inst = '%s %s' % (m.group(1), m.group(2))
            continue
        if inst is None:
            continue
        if re.match(r'^\s*\)\s*;', line):
            inst = None
            continue
        t = TRAILING.search(line)
        trailing = t.group(1) if t else ''
        code = line[:t.start()] if t else line
        for c in CONN.finditer(code):
            port, conn = c.group(1), c.group(2)
            row = classify(lines, i, inst, port, conn, trailing)
            if row:
                rows.append(row)
    return rows


def classify(lines, i, inst, port, conn, trailing):
        # An EMPTY connection is audited exactly like a literal. This line used
        # to read `continue  # an unread output, named on purpose` -- which is
        # an assumption wearing a check's clothes, and it excluded a whole
        # class of silent decision from a count that then printed "0 silent".
        #
        # It matters here specifically. `zhao_geom_bin_pipe_v2` brought SIX
        # structural fault outputs that the V1 binner did not have; the shell
        # puts four into its fault OR, routes one to the inherited top-level
        # output, and leaves `.local_attribute_abort_o()` and
        # `.local_fault_pulse_o()` empty. Dropping a fault on the floor is a
        # decision, and afterwards it is indistinguishable from an oversight.
    if conn.strip() == '':
        if not is_fault_port(port):
            return None       # an unread telemetry output; not this tool's job
    elif not LITERAL.match(conn):
        return None
    reason = ''
    if 'TIE:' in trailing:
        reason = trailing.split('TIE:', 1)[1].strip()
    else:
        # Walk back through the CONTIGUOUS comment block above the line. A
        # reason worth giving usually needs more than one line, and an
        # audit that only looked at the line directly above would push
        # people towards short reasons or none.
        k = i - 1
        while k >= 0 and lines[k].strip().startswith('//'):
            if 'TIE:' in lines[k]:
                reason = lines[k].split('TIE:', 1)[1].strip()
                break
            k -= 1
    return (inst, port, conn.strip() or '<empty>', reason, i + 1)


def main():
    rc = 0
    for target in TARGETS:
        if not target.exists():
            print('packet-h tie-off audit: %s -- MISSING'
                  % target.relative_to(REPO).as_posix())
            return 1
        rows = audit(io.open(target, encoding='utf-8', errors='replace').read())
        declared = [r for r in rows if r[3]]
        silent = [r for r in rows if not r[3]]

        print('packet-h tie-off audit: %s' % target.relative_to(REPO).as_posix())
        print('  literal connections: %d declared, %d silent'
              % (len(declared), len(silent)))

        if silent:
            print()
            print('  == SILENT (a literal with no reason) ==')
            for inst, port, conn, _r, ln in silent:
                print('    %s:%d  %s . %s (%s)' % (target.name, ln, inst, port, conn))
            print()
            print('    A literal in a port map and a decision look identical')
            print('    afterwards. Add `// TIE: <why>` on the line, or connect it.')
            rc = 1
    return rc


# SELF-CHECK. A pattern that matches nothing reports a clean sheet, which is
# this repository's most-repeated failure. Prove both regexes still bite.
assert LITERAL.match("16'd0") and LITERAL.match("'0") and LITERAL.match("2'b10")
assert not LITERAL.match('some_net') and not LITERAL.match('zhao_fb_tuple_slot(x')
_probe = audit("\n".join([
    'zhao_thing u_probe (',
    "    .a (1'b0),",
    "    .b (1'b1),  // TIE: because",
    '    .c (some_net),',
    '    .d (),',
    '    .some_fault_o (),',
    '    .some_fault_count_o (),',
    '    .hits_o (),',
    ');']))
assert len(_probe) == 3, _probe
assert _probe[0][1] == 'a' and _probe[0][3] == ''
assert _probe[1][1] == 'b' and _probe[1][3] == 'because'
# An empty FAULT output is audited; a plain unread output is not, and a fault
# COUNT is telemetry. These assertions are the whole reason the distinction
# cannot drift back to "skip every empty connection".
assert _probe[2][1] == 'some_fault_o' and _probe[2][2] == '<empty>'
assert is_fault_port('local_fault_pulse_o')
assert is_fault_port('binner_overflow_o')
assert not is_fault_port('local_fault_count_o')
assert not is_fault_port('texture_cache_hits_o')

if __name__ == '__main__':
    sys.exit(main())
