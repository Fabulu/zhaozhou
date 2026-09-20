r"""Every literal in a composition must say why it is there.

The Packet-H gate says *every new port is connected*. The trouble with checking
that by eye is that a port map entry reading `(1'b0)` and one reading
`(some_net)` look equally deliberate, and the first is how an input nobody
decided about ships as a decision. CLAUDE.md's phrasing is exact: a tie-off that
looks deliberate is worse than a missing connection, because a missing one is a
`PINMISSING` and a tie-off is silence.

So this audit reads a composition, finds every instantiation port map, and
sorts the literal connections into four kinds:

  * DECLARED  -- an explicit `// TIE:` or `// REAL:` marker;
  * REASONED  -- a trailing comment of substance, or a comment above that
                 NAMES the port;
  * GROUP     -- a substantive comment introducing the run of connections this
                 one belongs to (this file's own convention);
  * SILENT    -- a literal with nothing said about it anywhere. The only class
                 that is an error, and the only thing this tool exists to find.

It is deliberately dumb about SystemVerilog: it does not elaborate, it does not
resolve parameters, and it does not care what the literal's value is. A tool
that understood the design could be wrong about it. This one can only be wrong
about text, and the text is the thing being audited.

WHY THE FOUR CLASSES AND NOT TWO (owner ruling R166). This audit was written
for two Packet-H shells and, on 2026-09-20, pointed at `zhao_console_core.sv`
for the first time -- to answer R159, which found that the completion register
cannot see an undeclared tie-off. It reported `0 declared, 19 silent`, and
every one of the nineteen dissolved on reading:

  * it could see only 18 of the core's 88 instantiations, because 70 are
    parameterised and its pattern matched only the plain form;
  * the core declares with `REAL:` -- 163 times, against 0 uses of `TIE:`;
  * and the core writes one reason above a RUN of connections, so a walk-back
    that stopped at the first non-comment line credited the first and called
    its siblings silent.

MAXIMUM ALARM AT A FILE THAT WAS DOING THE RIGHT THING. A false alarm is the
same disease as a blind gate -- the instrument is not measuring its subject --
but it presents as diligence, so it is harder to stop. Each class above exists
because a real, well-kept convention was being called a defect.

Run it; it takes about a second. `packet_h_tieoff_audit` is the ctest, and with
no arguments it audits exactly the two Packet-H compositions it always did.
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

# THE DECLARATION MARKERS, and there is more than one because this repository
# has more than one convention and this tool knew only half of it.
#
# `TIE:` is what the two Packet-H shells use. `zhao_console_core.sv` uses
# `REAL:` -- 163 times, against 0 uses of `TIE:`. Accepting both weakens
# neither gate, and that is MEASURED rather than assumed: the shells contain no
# `REAL:` and the core contains no `TIE:`, so no connection changes class in
# the file it already lived in.
MARKERS = ('TIE:', 'REAL:')

# How far a group comment's authority reaches: the number of intervening
# connection lines the walk-back will cross to find it. Bounded, because an
# unbounded walk reaches the instance header and credits everything below it to
# whatever was written there.
GROUP_SPAN = 12

# The shortest thing that counts as prose rather than furniture. A banner, a
# divider or a one-word label is not a reason.
MIN_REASON = 24

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

# TWO instantiation forms, and the second one was missing until 2026-09-20.
#
#   zhao_foo u_foo (                 <- plain
#   zhao_foo #(  .W(32)  ) u_foo (   <- parameterised, closing paren on its own
#                                       line: `) u_foo (`
#
# Only the first was matched. That was invisible while the targets were the two
# Packet-H shells, which instantiate plainly, and it became load-bearing the
# moment the audit was pointed at `zhao_console_core.sv`, which holds 88
# instantiations of which 70 are parameterised. The tool read 18 of 88 and
# printed a total that looked like a whole-file audit.
#
# The parameter list itself is NOT audited, and that is deliberate: between a
# module name and `) u_foo (` the connections are PARAMETERS, and a literal
# parameter is a value, not a tie-off. They are skipped for free, because
# `inst` is still None while they are being read.
INSTANCE = re.compile(r'^\s*(?:(\w+)\s+|\)\s*)(u_\w+)\s*\($')

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
            # group(1) is None for the parameterised form, where the module
            # name sits lines above on the `zhao_foo #(` line. The instance
            # name alone is enough to locate it and is never ambiguous.
            inst = ('%s %s' % (m.group(1), m.group(2)) if m.group(1)
                    else m.group(2))
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


def governing_comment(lines, i):
    """The contiguous comment block that introduces this connection.

    Walks back over the connection's own group. `zhao_console_core.sv` writes
    one reason and then lists the connections it covers:

        // Ruled zero for a decoder-stage event -- zref_trace.hpp choice 3.
        .ev_tile_i      (32'd0),
        .ev_primitive_i (32'd0),
        .ev_pixel_i     (32'd0),

    A walk-back that stopped at the first non-comment line credited only
    `ev_tile_i` and called the other two silent -- which is how this audit
    reported 16 silent connections in a file where all sixteen were reasoned.
    The same shape split `vp_x0_i` from the `vp_y0_i` on the very next line.
    """
    j = i - 1
    skipped = 0
    while j >= 0 and skipped <= GROUP_SPAN:
        s = lines[j].strip()
        if s.startswith('//'):
            break
        if not s or not CONN.search(lines[j]):
            return []
        skipped += 1
        j -= 1
    if j < 0 or skipped > GROUP_SPAN:
        return []
    block = []
    while j >= 0 and lines[j].strip().startswith('//'):
        block.append(lines[j])
        j -= 1
    block.reverse()
    return block


def classify(lines, i, inst, port, conn, trailing):
    # An EMPTY connection is audited exactly like a literal. This line used to
    # read `continue  # an unread output, named on purpose` -- an assumption
    # wearing a check's clothes, which excluded a whole class of silent
    # decision from a count that then printed "0 silent". Dropping a fault on
    # the floor is a decision, and afterwards it is indistinguishable from an
    # oversight.
    if conn.strip() == '':
        if not is_fault_port(port):
            return None       # an unread telemetry output; not this tool's job
    elif not LITERAL.match(conn):
        return None

    lit = conn.strip() or '<empty>'
    block = governing_comment(lines, i)
    joined = '\n'.join(block)
    prose = ' '.join(b.strip().lstrip('/ ') for b in block).strip()
    if re.match(r'^[-=*\s]*$', prose):
        prose = ''

    # 1. AN EXPLICIT MARKER, on the line or in the governing comment.
    for mk in MARKERS:
        if mk in trailing:
            return (inst, port, lit,
                    trailing.split(mk, 1)[1].strip()[:110], i + 1, 'declared')
    for mk in MARKERS:
        if mk in joined:
            return (inst, port, lit,
                    joined.split(mk, 1)[1].strip()[:110], i + 1, 'declared')

    # 2. A TRAILING COMMENT of substance on the connection's own line.
    #    `.n_mag_i (32'd0),   // not read: n_mag_valid_i is low` is a complete
    #    reason, and demanding it be re-spelt with a marker would be pedantry.
    if len(trailing.strip()) >= MIN_REASON:
        return (inst, port, lit, trailing.strip()[:110], i + 1, 'reasoned')

    # 3. A GOVERNING COMMENT that NAMES THE PORT. The strongest non-marker
    #    form: naming the port is what makes the prose evidence ABOUT this
    #    connection rather than merely near it.
    if joined and re.search(r'\b%s\b' % re.escape(port), joined):
        return (inst, port, lit, prose[:110], i + 1, 'reasoned')

    # 4. A SUBSTANTIVE GOVERNING COMMENT -- the group convention.
    #
    #    THE LENGTH FLOOR IS THE DEFENCE against rebuilding this repository's
    #    `uncashed_cheques` defect (R105), where a tool graded comments and a
    #    paragraph that merely sat near code was read as evidence about it. A
    #    banner or a one-word divider is furniture, not a reason. This class is
    #    reported separately and is NOT an error, because a human still has to
    #    decide whether the group's reason covers this member of it.
    if len(prose) >= MIN_REASON:
        return (inst, port, lit, prose[:110], i + 1, 'group')

    return (inst, port, lit, '', i + 1, 'silent')


def main(argv=None):
    # Targets may be named on the command line so this audit can be pointed at
    # a composition it was not written for -- which is the whole reason it was
    # worth anything on 2026-09-20. With no arguments it audits the two
    # Packet-H compositions exactly as before, so `packet_h_tieoff_audit`
    # remains the same ctest it always was.
    argv = sys.argv[1:] if argv is None else argv
    targets = [Path(a).resolve() for a in argv] if argv else TARGETS
    rc = 0
    for target in targets:
        if not target.exists():
            print('tie-off audit: %s -- MISSING' % target)
            return 1
        rows = audit(io.open(target, encoding='utf-8',
                             errors='replace').read())
        by = lambda k: [r for r in rows if r[5] == k]      # noqa: E731
        declared, reasoned = by('declared'), by('reasoned')
        group, silent = by('group'), by('silent')

        try:
            name = target.relative_to(REPO).as_posix()
        except ValueError:
            name = str(target)
        print('tie-off audit: %s' % name)
        print('  literal connections: %d declared, %d reasoned, '
              '%d by group comment, %d SILENT'
              % (len(declared), len(reasoned), len(group), len(silent)))

        if group:
            print()
            print('  == COVERED BY A GROUP COMMENT (not an error; a human '
                  'confirms the reason covers each member) ==')
            for inst, port, lit, r, ln, _k in group:
                print('    %s:%d  %s . %s (%s)' % (target.name, ln, inst,
                                                   port, lit))
                print('        %s' % r)

        if silent:
            print()
            print('  == SILENT (a literal with no reason) ==')
            for inst, port, lit, _r, ln, _k in silent:
                print('    %s:%d  %s . %s (%s)' % (target.name, ln, inst,
                                                   port, lit))
            print()
            print('    A literal in a port map and a decision look identical')
            print('    afterwards. Add `// TIE: <why>` on the line, or '
                  'connect it.')
            rc = 1
    return rc


# SELF-CHECK. A pattern that matches nothing reports a clean sheet, which is
# this repository's most-repeated failure. Prove every regex still bites.
assert LITERAL.match("16'd0") and LITERAL.match("'0") and LITERAL.match("2'b10")
assert not LITERAL.match('some_net') and not LITERAL.match('zhao_fb_tuple_slot(x')
_probe = audit("\n".join([
    'zhao_thing u_probe (',
    "    .a (1'b0),",
    "    .b (1'b1),  // TIE: because the bench forces it and nothing else can",
    '    .c (some_net),',
    '    .d (),',
    '    .some_fault_o (),',
    '    .some_fault_count_o (),',
    '    .hits_o (),',
    ');']))
assert len(_probe) == 3, _probe
assert _probe[0][1] == 'a' and _probe[0][5] == 'silent', _probe
assert _probe[1][1] == 'b' and _probe[1][5] == 'declared', _probe
# An empty FAULT output is audited; a plain unread output is not, and a fault
# COUNT is telemetry. These assertions are the whole reason the distinction
# cannot drift back to "skip every empty connection".
assert _probe[2][1] == 'some_fault_o' and _probe[2][2] == '<empty>'
assert is_fault_port('local_fault_pulse_o')
assert is_fault_port('binner_overflow_o')
assert not is_fault_port('local_fault_count_o')
assert not is_fault_port('texture_cache_hits_o')

# THE PARAMETERISED FORM, which this tool could not see until 2026-09-20.
# A silent literal inside `) u_foo (` must be found, and the parameter literals
# above it must NOT be reported -- a parameter value is not a tie-off.
_param = audit("\n".join([
    'zhao_thing #(',
    '    .WIDTH (32),',          # a PARAMETER literal: must not be audited
    "    .MODE  (2'b01)",        # likewise
    ') u_param (',
    "    .a (1'b0),",            # a real silent tie-off: must be found
    '    .b (some_net),',
    ');']))
assert [r[1] for r in _param] == ['a'], _param
assert _param[0][0] == 'u_param' and _param[0][5] == 'silent', _param
# NEGATIVE CONTROL for the fix itself. The OLD pattern is kept here verbatim so
# the improvement cannot silently regress: it must still fail to match the line
# the new one matches. If this assertion ever fails, the two patterns have
# converged and the test above has stopped proving anything.
_OLD_INSTANCE = re.compile(r'^\s*(\w+)\s+(u_\w+)\s*\($')
assert _OLD_INSTANCE.match('zhao_thing u_plain (')
assert not _OLD_INSTANCE.match(') u_param (')
assert INSTANCE.match(') u_param (')

# THE GROUP CONVENTION, and the discriminator that keeps it honest.
#   .x_i -- named by the prose above it            -> reasoned
#   .y_i -- covered by that same group comment     -> group
#   .z_i -- preceded only by a divider             -> SILENT
# The divider case is the defence: a banner is furniture, and if it ever counts
# as a reason this tool has become a comment-grader (R105).
_prose = audit("\n".join([
    'zhao_thing u_prose (',
    '    // x_i is the identity element here: the real producer is one level',
    '    // down and ORs into it.',
    "    .x_i (1'b0),",
    "    .y_i (1'b0),",
    '    // ----------------------------------------------------------------',
    "    .z_i (1'b0),",
    ');']))
assert [(r[1], r[5]) for r in _prose] == [('x_i', 'reasoned'),
                                          ('y_i', 'group'),
                                          ('z_i', 'silent')], _prose
# And the group's reach is BOUNDED: past GROUP_SPAN connections the comment no
# longer speaks for what follows.
_far = audit("\n".join(
    ['zhao_thing u_far (',
     '    // A reason that introduces the group immediately beneath it.']
    + ["    .p%d (1'b0)," % n for n in range(GROUP_SPAN + 3)]
    + [');']))
assert _far[0][5] == 'group', _far[0]
assert _far[-1][5] == 'silent', _far[-1]

if __name__ == '__main__':
    sys.exit(main())
