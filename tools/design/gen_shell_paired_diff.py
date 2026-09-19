r"""Generate the paired-traffic differential harness for the Packet-H gate.

THE CLAUSE is "unaffected behaviour matches under paired traffic". The
structural half is `packet_h_sibling_diff`, which proves the sibling's carried-
over instances have not drifted TEXTUALLY. This is the other half: drive the
historical shell and the sibling from ONE set of stimulus and compare what
comes out.

That comparison is possible at all only because of a decision made when the
sibling was seeded. `zhao_shell_top_v2` is a strict SUPERSET of
`zhao_shell_top`: 146 ports became 209 and NOT ONE was removed, including
seven `job_*` inputs the V2 bin pipe does not consume, which are kept and sunk
rather than deleted. The sink's own comment says why -- "a sibling whose port
list has drifted cannot be driven by the same harness". This file is the cheque
that comment wrote.

WHY IT IS GENERATED. Wiring 146 ports twice by hand, then keeping both copies
correct as the shells move, is the exact shape this repository has a chapter
about: `zhao_prod_top.sv` was found stale for two separate port changes made
days apart, and a generated file nobody regenerates is a stale file with a
reassuring provenance line at the top. So this regenerates, and
`packet_h_paired_diff_fresh` fails if the checked-in output does not match what
the current shells produce.

It reuses `gen_prod_top.parse_ports` rather than parsing ports a third time.
That parser carries a hard-won fix -- a comma continuation such as
`input logic [31:0] a_i, b_i,` yields BARE IDENTIFIERS for everything after the
first, which once made 157 connections one bit wide against 32-bit pins and
understated the area of the one top that exists to measure area.

WHAT IT DOES NOT DECIDE. Which outputs are expected to MATCH is not something a
generator can know, and guessing would produce a differential that passes by
comparing nothing. The classification lives in DIVERGENT below, each entry with
a reason, and everything not named there must match cycle for cycle.
"""
import io
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'quartus'))

from gen_prod_top import parse_ports, port_header, unpacked_count  # noqa: E402

V1 = REPO / 'fpga/rtl/common/zhao_shell_top.sv'
V2 = REPO / 'fpga/rtl/common/zhao_shell_top_v2.sv'
OUT = REPO / 'tests/shell/zhao_shell_paired_diff.sv'
MUT = REPO / 'tests/mutants/zhao_shell_paired_diff_mutant.sv'

# Outputs that are EXPECTED to differ, each with the reason Packet H changed
# them. Everything else must match cycle for cycle, so this list is the whole
# substance of the check: every name added here is a claim withdrawn.
#
# Keep it short and keep it argued. A differential whose exception list grows
# to cover whatever failed is a differential that proves nothing, and it fails
# in the flattering direction -- silently, looking green.
#
# THE LIST IS THE FOUR OUTPUTS THE SWAPPED BLOCK DRIVES DIRECTLY, and it was
# nineteen names long in its first draft. The other fifteen were reasoned from
# the outside -- "the render path changed, so the render counters must differ"
# -- and reading the historical shell refuted most of them: `ring_wr_*` comes
# from `zhao_cmd_scheduler`, not from the slot manager that was replaced;
# `render_busy_o`, `render_pixels_o` and `render_fatal_o` come from the memory
# writer; `geom_guard_rsp_o` from the guard. The replaced slot manager drives
# no top-level output at all.
#
# Fifteen speculative exemptions would have been fifteen outputs never
# compared, in a tool whose whole value is the comparison -- and it would have
# passed, which is how that kind of mistake survives. Start minimal; add a name
# only when the differential DEMONSTRATES the divergence and the reason is
# written down beside it.
DIVERGENT = {
    'render_tri_ready_o':
        'driven by the swapped pipe: V2 backpressures triangles on its own '
        'schedule',
    'render_drain_done_o':
        'driven by the swapped pipe: V2 drains through a different path',
    'render_overflow_o':
        'driven by the swapped pipe: the V2 binner has its own arena',
    'render_fragment_error_o':
        'driven by the swapped pipe: a V2 tile-pipe signal the V1 binner had '
        'no counterpart for',
}


def read(path):
    return io.open(path, encoding='utf-8', errors='replace').read()


def ports_of(path, module):
    """The module's ports, with its INTEGER PARAMETERS resolved in the ranges.

    Since the TERRAIN.BUILD socket (2026-09-19) the sibling has a port sized by
    its own parameter -- `zhao_hps_burst_req_t [BUILD_HPS_N-1:0]
    build_hps_req_i`. Copied verbatim into this harness, which has no such
    parameter, that range names nothing and the harness does not elaborate.
    The harness instantiates the sibling at its DEFAULTS, so the default is the
    value that range has here; resolving it is not a guess.
    """
    text = read(path)
    ports = parse_ports(port_header(text, module))
    params = dict(re.findall(r'parameter\s+int\s+unsigned\s+(\w+)\s*=\s*(\d+)', text))
    if not params:
        return ports
    pat = re.compile(r'\b(%s)\b' % '|'.join(map(re.escape, params)))

    def sub(dims):
        return [pat.sub(lambda m: params[m.group(1)], x) for x in dims]

    out = []
    for p in ports:
        d, n, packed, unpacked, s, ut = p
        out.append((d, n, sub(packed), sub(unpacked), s, ut))
    return out


def decl(direction, name, packed, unpacked=(), utype=''):
    """One port declaration.

    Three things this has to carry, and each was got wrong once:

    * `parse_ports` returns ranges WITH their brackets already, so joining them
      is the whole job. Wrapping them again produced `[[1:0]]`.
    * Unpacked dimensions go AFTER the name. The shell has ports such as
      `input logic [1:0] hps_state_i [0:2]`, and dropping the `[0:2]` silently
      declares a scalar where an array was wanted.
    * A TYPEDEF port keeps its type. `geom_guard_req_i` is
      `input var zhao_guard_req_t`, 103 bits wide; declared as bare `logic` it
      becomes ONE bit, and Verilator reports that as a WIDTHEXPAND on the pin
      rather than as anything resembling "your generator dropped the type".
      This is the same class of miss as the comma-continuation bug in
      `gen_prod_top`, whose own comment warns that a silently narrowed port is
      not cosmetic.
    """
    rng = ''.join(packed)
    tail = ''.join(unpacked)
    kind = utype if utype else 'logic'
    return '  %s %s %s%s%s' % (direction, kind, (rng + ' ') if rng else '',
                               name, (' ' + tail) if tail else '')


# The port the committed mutant corrupts. A differential that has only ever
# returned "they agree" is a claim, and this one is worth doubting harder than
# most: its PASS is the absence of a signal, produced by 91 generated compare
# lines nobody reads. If one of them were mistyped -- comparing v1 against v1,
# say -- it would agree forever and look exactly like this.
#
# The mutant negates the V2 side of ONE comparison, which fires whether or not
# that output ever toggles under the stimulus. It is evidence about the
# INSTRUMENT, not about the design, and its driver's polarity is inverted: it
# passes when the differential FAILS.
MUTANT_PORT = 'px_valid_o'


def main(check=False, mutant=False):
    v1 = ports_of(V1, 'zhao_shell_top')
    v2 = ports_of(V2, 'zhao_shell_top_v2')

    v1_by_name = {n: (d, p, u, ut) for d, n, p, u, _s, ut in v1}
    v2_by_name = {n: (d, p, u, ut) for d, n, p, u, _s, ut in v2}

    missing = sorted(set(v1_by_name) - set(v2_by_name))
    if missing:
        print('FAIL: the sibling no longer carries every V1 port.')
        print('  %d missing: %s' % (len(missing), ', '.join(missing[:12])))
        print('  The paired differential depends on the superset property --')
        print('  one stimulus cannot drive two shells with different inputs.')
        return 1

    # A DIVERGENT name that is not an output at all is a dead rule: it excuses
    # nothing while looking like it excuses something.
    outs = {n for d, n, *_ in v1 if d == 'output'}
    dead = sorted(set(DIVERGENT) - outs)
    if dead:
        print('FAIL: DIVERGENT names something that is not a V1 output:')
        for n in dead:
            print('  %s' % n)
        return 1

    shared_in = [(n, v1_by_name[n][1], v1_by_name[n][2], v1_by_name[n][3])
                 for d, n, *_ in v1 if d == 'input']
    compared = [(n, v1_by_name[n][1], v1_by_name[n][2], v1_by_name[n][3]) for d, n, *_ in v1
                if d == 'output' and n not in DIVERGENT]
    all_out = [(n, v1_by_name[n][1], v1_by_name[n][2], v1_by_name[n][3]) for d, n, *_ in v1
               if d == 'output']
    new_in = [(n, p, u, ut) for d, n, p, u, _s, ut in v2
              if d == 'input' and n not in v1_by_name]

    L = []
    L.append('// zhao_shell_paired_diff.sv -- GENERATED, do not edit.')
    L.append('//')
    L.append('// Regenerate: python tools/design/gen_shell_paired_diff.py')
    L.append('// Checked by: packet_h_paired_diff_fresh')
    L.append('//')
    L.append('// The Packet-H gate clause "unaffected behaviour matches under')
    L.append('// paired traffic", as a machine. One stimulus, two shells, and a')
    L.append('// mismatch bit per compared output.')
    L.append('//')
    L.append('// %d shared inputs drive both. %d outputs must match. %d are'
             % (len(shared_in), len(compared), len(DIVERGENT)))
    L.append('// declared divergent, each with its reason in the generator --')
    L.append('// every name on that list is a claim WITHDRAWN, which is why it')
    L.append('// is short and why it is argued rather than discovered.')
    L.append('//')
    L.append('// %d inputs exist only on the sibling. They get harness ports of'
             % len(new_in))
    L.append('// their own so a test can exercise the new lifecycle without')
    L.append('// disturbing the paired comparison.')
    L.append('')
    L.append('module %s' % ('zhao_shell_paired_diff_mut' if mutant else 'zhao_shell_paired_diff'))
    L.append('  // The same import the two shells carry. Some ports are typedefs')
    L.append('  // -- `geom_guard_req_i` is a 103-bit `zhao_guard_req_t` -- and')
    L.append('  // without the package they are either unresolvable or, worse,')
    L.append('  // silently one bit wide. ONE comma-separated clause:')
    L.append('  // QUARTUS_GOTCHAS 20 says two consecutive `import` statements')
    L.append('  // lint clean here and are rejected by Quartus 17.0.')
    L.append('  import zhao_pkg::*, zhao_abi_pkg::*;')
    L.append('(')

    # Inputs of the pair: every V1 input, plus every sibling-only input.
    decls = []
    for n, p, u, ut in shared_in:
        decls.append(decl('input ', n, p, u, ut))
    for n, p, u, ut in new_in:
        decls.append(decl('input ', n, p, u, ut))
    # Outputs: V1's and V2's, both exposed, plus the verdict.
    for n, p, u, ut in all_out:
        decls.append(decl('output', 'v1_' + n, p, u, ut))
        decls.append(decl('output', 'v2_' + n, p, u, ut))
    decls.append(decl('output', 'mismatch_any_o', []))
    decls.append(decl('output', 'mismatch_sticky_o', []))
    # WITH the brackets: `parse_ports` yields ranges already bracketed, and
    # `decl` joins them verbatim. Passing '31:0' here produced
    # `output logic 31:0 mismatch_count_o`, which broke the port list and threw
    # six cascading syntax errors a thousand lines further down.
    decls.append(decl('output', 'mismatch_count_o', ['[31:0]']))
    decls.append(decl('output', 'mismatch_first_o', ['[31:0]']))
    decls.append(decl('output', 'toggled_count_o', ['[31:0]']))
    L.append(',\n'.join(decls))
    L.append(');')
    L.append('')

    L.append('  // ---- the two shells, one stimulus ------------------------')
    L.append('  zhao_shell_top u_v1 (')
    conns = []
    for d, n, *_ in v1:
        conns.append('    .%s(%s)' % (n, n if d == 'input' else 'v1_' + n))
    L.append(',\n'.join(conns))
    L.append('  );')
    L.append('')
    L.append('  // The sibling has %d outputs the historical shell never had'
             % (len([1 for d, n, *_ in v2
                     if d == 'output' and n not in v1_by_name])))
    L.append('  // -- the v2_* lifecycle counters and the new lease surface.')
    L.append('  // They are left unconnected ON PURPOSE: this harness exists to')
    L.append('  // compare the SHARED surface, and a V2-only output has nothing')
    L.append('  // to be compared against.')
    L.append('  /* verilator lint_off PINCONNECTEMPTY */')
    L.append('  zhao_shell_top_v2 u_v2 (')
    conns = []
    for d, n, *_ in v2:
        if d == 'input':
            conns.append('    .%s(%s)' % (n, n))
        else:
            target = 'v2_' + n if n in v1_by_name else ''
            conns.append('    .%s(%s)' % (n, target))
    L.append(',\n'.join(conns))
    L.append('  );')
    L.append('  /* verilator lint_on PINCONNECTEMPTY */')
    L.append('')

    L.append('  // ---- the verdict ----------------------------------------')
    L.append('  // One bit per compared output, OR-ed. A sticky copy because a')
    L.append('  // one-cycle divergence that the next cycle repairs is still a')
    L.append('  // divergence, and a test sampling at its own convenience would')
    L.append('  // miss it -- this repository has a chapter about sampling')
    L.append('  // uniformly and finding the typical frame.')
    L.append('  wire [%d:0] mm_c;' % (len(compared) - 1))
    for i, (n, _p, u, _ut) in enumerate(compared):
        if u:
            # `!==` is not defined for UNPACKED arrays, so an unpacked port is
            # compared element by element. Writing `v1_x !== v2_x` here lints
            # clean in some tools and is a type error in others, which is the
            # worst of both.
            count = unpacked_count(u)
            terms = ' ||\n                     '.join(
                '(v1_%s[%d] !== v2_%s[%d])' % (n, k, n, k)
                for k in range(count))
            L.append('  assign mm_c[%d] = %s;' % (i, terms))
        elif mutant and n == MUTANT_PORT:
            L.append('  // MUTATED: the V2 side is negated, so this comparison')
            L.append('  // must report a divergence that is not there. If the')
            L.append('  // differential still passes, it is not comparing.')
            L.append('  assign mm_c[%d] = (v1_%s !== ~v2_%s);' % (i, n, n))
        else:
            L.append('  assign mm_c[%d] = (v1_%s !== v2_%s);' % (i, n, n))
    L.append('  assign mismatch_any_o = |mm_c;')
    L.append('')
    L.append('  // THE INDEX OF THE FIRST OUTPUT TO DIVERGE, latched. A harness')
    L.append('  // that reports only "they differ" sends the reader hunting')
    L.append('  // through %d signals; the index names one. The generator')
    L.append('  // prints the table below so the number resolves to a port.')
    L.append('  logic sticky_q;')
    L.append('  logic [31:0] count_q, first_q;')
    L.append('  logic [31:0] first_index_c;')
    L.append('  always_comb begin')
    L.append('    first_index_c = 32\'hFFFF_FFFF;')
    L.append('    for (int unsigned mmi = 0; mmi < %d; mmi++)' % len(compared))
    L.append('      if (mm_c[mmi] && (first_index_c == 32\'hFFFF_FFFF))')
    L.append('        first_index_c = mmi;')
    L.append('  end')
    L.append('  always_ff @(posedge gpu_clk or negedge rst_n) begin')
    L.append('    if (!rst_n) begin')
    L.append('      sticky_q <= 1\'b0;')
    L.append('      count_q  <= 32\'d0;')
    L.append('      first_q  <= 32\'hFFFF_FFFF;')
    L.append('    end else if (mismatch_any_o) begin')
    L.append('      sticky_q <= 1\'b1;')
    L.append('      count_q  <= count_q + 32\'d1;')
    L.append('      if (!sticky_q) first_q <= first_index_c;')
    L.append('    end')
    L.append('  end')
    L.append('  assign mismatch_sticky_o = sticky_q;')
    L.append('  assign mismatch_count_o = count_q;')
    L.append('  assign mismatch_first_o = first_q;')
    L.append('')
    L.append('  // ---- THE ACTIVITY WITNESS -------------------------------')
    L.append('  // Two shells that produce NOTHING agree perfectly. Without')
    L.append('  // this, a stimulus that never reaches the compared outputs --')
    L.append('  // a clock that does not tick, a reset never released, a domain')
    L.append('  // nothing drives -- passes the differential completely and')
    L.append('  // means nothing by it. "A gate that cannot reach the state is')
    L.append('  // not evidence about the state."')
    L.append('  //')
    L.append('  // So: one bit per compared output, set the first time V1\'s')
    L.append('  // copy of it CHANGES. The test asserts a floor on the count,')
    L.append('  // and the count is the honest measure of how much of this')
    L.append('  // comparison was actually exercised.')
    L.append('  //')
    L.append('  // ARMED ONE CYCLE LATE, and that detail is the difference')
    L.append('  // between a witness and a decoration. The delayed copies hold')
    L.append('  // x until they are first loaded, so an unguarded `!==`')
    L.append('  // compares every output against x on the first edge and marks')
    L.append('  // ALL of them toggled -- a witness that reports full coverage')
    L.append('  // of a run that has not started.')
    for i, (n, _p, u, _ut) in enumerate(compared):
        L.append('  %s v1_%s_q%s;' % (_ut if _ut else 'logic ' + ''.join(_p), n, ''.join(u)))
    L.append('  logic [%d:0] toggled_q;' % (len(compared) - 1))
    L.append('  logic armed_q;')
    L.append('  always_ff @(posedge gpu_clk or negedge rst_n) begin')
    L.append('    if (!rst_n) begin')
    L.append('      toggled_q <= \'0;')
    L.append('      armed_q   <= 1\'b0;')
    L.append('    end else begin')
    L.append('      armed_q <= 1\'b1;')
    L.append('      if (armed_q) begin')
    for i, (n, _p, u, _ut) in enumerate(compared):
        if u:
            count = unpacked_count(u)
            cond = ' ||\n            '.join(
                '(v1_%s[%d] !== v1_%s_q[%d])' % (n, k, n, k)
                for k in range(count))
            L.append('        if (%s) toggled_q[%d] <= 1\'b1;' % (cond, i))
        else:
            L.append('        if (v1_%s !== v1_%s_q) toggled_q[%d] <= 1\'b1;'
                     % (n, n, i))
    L.append('      end')
    L.append('    end')
    L.append('  end')
    L.append('  always_ff @(posedge gpu_clk) begin')
    for i, (n, _p, u, _ut) in enumerate(compared):
        L.append('    v1_%s_q <= v1_%s;' % (n, n))
    L.append('  end')
    L.append('')
    L.append('  always_comb begin')
    L.append('    toggled_count_o = 32\'d0;')
    L.append('    for (int unsigned mmi = 0; mmi < %d; mmi++)' % len(compared))
    L.append('      if (toggled_q[mmi]) toggled_count_o = toggled_count_o + 1;')
    L.append('  end')
    L.append('')
    L.append('  // The compared outputs, in bit order, so `mismatch_first_o`')
    L.append('  // resolves to a name without re-running the generator:')
    for i, (n, _p, _u, _ut) in enumerate(compared):
        L.append('  //   %3d  %s' % (i, n))
    L.append('')
    L.append('endmodule : %s' % ('zhao_shell_paired_diff_mut' if mutant else 'zhao_shell_paired_diff'))
    text = '\n'.join(L) + '\n'

    # A mutant that mutates nothing is a control that always passes -- the
    # broken-instrument law applied to the instrument's own instrument.
    if MUTANT_PORT not in {n for n, _p, _u, _ut in compared}:
        print('FAIL: MUTANT_PORT %r is not a compared output, so the mutant'
              % MUTANT_PORT)
        print('  would be byte-identical to the real harness and its control')
        print('  would pass while testing nothing.')
        return 1

    if check:
        cur = read(MUT if mutant else OUT)
        cur = cur if cur else ''
        if cur.replace('\r\n', '\n') != text:
            print('FAIL: %s is STALE.' % (MUT if mutant else OUT).relative_to(REPO).as_posix())
            print('  The shells moved and the generated harness did not.')
            print('  Regenerate: python tools/design/gen_shell_paired_diff.py')
            return 1
        print('paired diff harness: fresh (%d shared inputs, %d compared '
              'outputs, %d declared divergent)'
              % (len(shared_in), len(compared), len(DIVERGENT)))
        return 0

    dest = MUT if mutant else OUT
    dest.parent.mkdir(parents=True, exist_ok=True)
    io.open(dest, 'w', encoding='utf-8', newline='\n').write(text)
    print('wrote %s' % dest.relative_to(REPO).as_posix())
    print('  %d shared inputs, %d sibling-only inputs'
          % (len(shared_in), len(new_in)))
    print('  %d outputs compared, %d declared divergent'
          % (len(compared), len(DIVERGENT)))
    return 0


if __name__ == '__main__':
    sys.exit(main(check='--check' in sys.argv, mutant='--mutant' in sys.argv))
