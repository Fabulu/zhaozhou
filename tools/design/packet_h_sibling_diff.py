r"""Packet-H gate clause: the sibling shell's UNAFFECTED domains are unaffected.

The clause is "unaffected behavior matches under paired traffic". The full
version of that is a paired-traffic differential against the historical shell,
which is a large harness and is not what this is. This is the structural half,
and it is the half that is cheap, durable, and catches the failure that paired
traffic would catch LATE: the sibling was SEEDED from `zhao_shell_top.sv` so
that the eighteen instances Packet H does not touch would be carried across
byte-for-byte, and nothing was checking that they still are.

A hand-maintained copy drifts. That is this repository's most-repeated lesson
and it has a whole chapter about committed mutant copies going stale in the
flattering direction. The sibling is a copy of a PROTECTED file, so the drift
would be silent on both sides.

WHAT IT CHECKS. Every differing region between the two files is classified:

  * COMMENT-ONLY -- the code lines are identical once comments and blank lines
    are stripped. Packet H added `// TIE:` reasons to inherited tie-offs and a
    long header; those are not behaviour.
  * SUBSTANTIVE -- real code differs. Every such region must fall inside a
    DECLARED region below, each of which names what Packet H changed and why.

An undeclared substantive region is an error. That is the whole tool: it does
not care how large the declared changes are, only that nothing else moved.

WHAT IT IS NOT. It compares TEXT, not behaviour. Two instances can be wired
differently while their text matches -- they cannot here, because the text IS
the wiring, but a signal they both read could have changed meaning elsewhere.
It is a guard against drift in the carried-over regions, not a proof of
equivalence, and the gate's paired-traffic differential is still owed.
"""
import io
import difflib
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RTL = REPO / 'fpga/rtl/common'
V1_NAME = 'zhao_shell_top.sv'
V2_NAME = 'zhao_shell_top_v2.sv'

# The protected hash of the seed. If the V1 shell ever moves, this comparison
# is against a different baseline and the result means something else.
# RE-PINNED under owner ruling R39 (provisional, 2026-09-19): the ONLY change
# to the protected V1 shell is R32's tie-off of MEM.GUARD's new region inputs,
# 3 lines x 4 zhao_mem_guard instances = 12 lines, each
#   .res_valid (1'b0) / .res_base (32'd0) / .res_span (32'd0)   // TIE: ...
# (the R32 write arm names TERRAIN_BUILD alone; V1 has no such client).
# No behaviour moves. Previous pin: 00fdd2387ffea985...
V1_SHA = '9ab87fd9ceeb5efb1333c2023cc4cf9a565b6f4d75633facfa33c9f6640b91cc'

# The controls cannot fire this by editing production RTL -- that is the
# live-tree hazard, and it would leave no evidence behind either. So the
# directory is overridable and `tests/tools/test_packet_h_sibling_diff.py`
# points it at synthetic trees in a temp directory. `--expect-seed-hash` lets
# a control keep the seed check honest while using a stand-in seed: the
# control that must fire on DRIFT would otherwise fail on the hash first and
# pass for the wrong reason.

# Every region Packet H is ALLOWED to change substantively, keyed by a marker
# that appears in V2 NEAR the change. Markers rather than line numbers,
# because line numbers move whenever anything above them does.
#
# ATTRIBUTION IS BY PROXIMITY, and the window is deliberately tight.
# difflib splits one logical edit -- a 165-port instantiation, say -- into a
# dozen opcodes, and only the first contains the marker at its head. So a
# substantive region counts as declared if a marker appears within
# ATTRIB_WINDOW lines above it or just after it. Wide enough to cover one
# instantiation; far too tight to reach a change in a different instance.
DECLARED = [
    ('zhao_shell_top_v2.sv -- the Packet-H sibling shell',
     'the new header'),
    ('module zhao_shell_top_v2',
     'the module rename, both ends'),
    ('PACKET-H: THE V3 PROGRAMMING CHANNEL',
     'the new top-level ports'),
    ('zhao_geom_bin_pipe_v2 u_render_bin',
     'the bin pipe swap'),
    # The lease seam is six organs across ~200 lines, which is wider than one
    # attribution window. Each is named, so an undeclared change lands on the
    # organ it is in rather than on "the seam" -- and so that ADDING a seventh
    # organ is an undeclared change until somebody writes down what it is.
    ('THE WRITER-AWARE LEASE SEAM',
     'the seam header and its nets'),
    ('zhao_renderer_lease_v2 u_render_lease',
     'the seam: the writer-1 renderer lease'),
    ('zhao_video_blit_lease_v2 u_blit_lease',
     'the seam: the writer-0 blit lease, the organ V1 never had'),
    ('zhao_video_terminal_adapter_v2 u_term_adapter',
     'the seam: the terminal adapter'),
    ('zhao_video_slotmgr_v2 u_slotmgr',
     'the seam: the V2 slot manager, replacing the V1 one'),
    ('zhao_fb_ready_cdc_v2 u_fb_cdc',
     'the seam: the 84-bit tuple CDC'),
    ('zhao_video_ready_bridge_v2 u_ready_bridge',
     'the seam: the READY bridge'),
    ('THE PAYLOAD LATCH, WHICH THE DELETED `lseq` FSM ALSO PERFORMED',
     'the blit payload capture the sequencer also did'),
    ('the blit path, writer-aware',
     'the V1 lease nets, deleted'),
    ('THE V1 SYNTHETIC-FRAGMENT INPUTS',
     'the seven job_* shell ports the V2 pipe does not consume, sunk'),
    ('THE V1 SWAP ECHO IS GONE',
     'the toggle-based swap echo, replaced by the 84-bit tuple CDC'),
    ('THE VIDEO-DOMAIN HALF OF THE OLD SWAP TOGGLE IS GONE TOO',
     'its video-domain half'),
    ('endmodule : zhao_shell_top_v2',
     'the module rename, closing'),
    # ---- DECLARED 2026-09-22, coordinator ------------------------------------
    # Both of these were flagged UNDECLARED by packet GEOMCLOSE, which correctly
    # said they were not its work and did not touch them. They are not drift:
    # both are TERRAIN.BUILD socket work that V2 has and V1 never had, and each
    # was traced to its commit rather than assumed -- which is the whole point
    # of declaring rather than silencing.
    ("slot 6's read beats, with",
     "the TERRAIN.BUILD socket read beats, c2647778 -- MEM.UPLOAD composed as "
     "the socket's first client (R4, R17). V1 has no socket"),
    ('assign build_hps_wr_ready_o = hb_wr_ready;',
     'the TERRAIN.BUILD socket write-ready, ae7e8d37. The commit is labelled '
     'WIP/BLOCKED and this one line is what survived of it; V1 has no socket'),
    # ---- DECLARED 2026-09-19 with owner ruling R39 ----------------------------
    # This table stopped being checked when the V1 seed hash went stale: the
    # tool fails on the hash BEFORE it looks at the diff, so every V2 change
    # landed after that went undeclared without anything going red that had not
    # already been red. Re-pinning the seed (R39) surfaced them; each is named
    # here by the packet that made it, from that packet's own comment in V2.
    ("CMD.DMA's PACKET STREAM, RE-EXPORTED.",
     'the CMD.DMA packet stream re-exported for CMD.DECODER (ports)'),
    ("honour the re-exported stream's second consumer",
     'the CMD.DMA packet stream re-exported: the accept fork'),
    ('THE TERRAIN.BUILD SOCKET (VRAM slot 6 + HPS clients 2..)',
     'cmdmem: the TERRAIN.BUILD socket ports (rulings R4/R17/R32)'),
    ('+ geom_gv_cnt + build_gv_cnt',
     'cmdmem: slot 6 guard violations totalled with the others'),
    ('Slot 6 is TERRAIN.BUILD, and it is now a SOCKET',
     'cmdmem: slot 6 guard (R32 region) and its write gate'),
    ('TWO WRITE QUEUES, and the burst says which one it pops',
     'cmdmem: the slot-6 write queue beside the framebuffer one'),
    ('THE THIRD READ OWNER, slot 6',
     'cmdmem: slot-6 read ownership and its beats'),
    ('TERRAIN_BUILD is the second legal writer (the slot-6 socket',
     'cmdmem: the routing tripwire admits TERRAIN_BUILD'),
    ('^ ^client_rsp[4] ^ ^client_rsp[5]',
     'cmdmem: client_rsp[6] leaves the unused sink (the socket reads it)'),
    ("POST.COMPOSITE's FRAMEBUFFER LEASE (core entries I15/I16",
     'post: the framebuffer lease ports (I15/I16)'),
    ('post lease, because three requesters share ENGINE0',
     'post: ENGINE0 retirement attributed per requester'),
    ("`render_pixels_o` / `render_bursts_o` are the RASTER's",
     'post: the post lease composed on ENGINE0'),
    ("ENGINE0's WRITES WAIT FOR THEIR DATA",
     'post: ENGINE0 writes wait for their data'),
    # Inside each carried-over zhao_mem_guard instance: R32's three new inputs,
    # tied off on every guard but the socket's. The marker is the TIE comment
    # itself, so it un-seals exactly the instances that carry the tie.
    ('the R32 resource-write arm names TERRAIN_BUILD alone',
     'cmdmem: R32 region inputs tied off on the non-socket guards'),
]

COMMENT = re.compile(r'^\s*(//.*)?$')

# One instantiation in this file runs to about 130 lines. Nothing else
# Packet H touches is longer.
ATTRIB_WINDOW = 140


INSTANCE = re.compile(r'^\s*([A-Za-z_]\w*)\s+(u_\w+)\s*\(')


def instance_spans(lines):
    """(instance name, first line, last line) for every instantiation.

    Paren-balanced from the header to its `);`, with `//` comments stripped
    first so a paren inside a comment cannot move the end.
    """
    out = []
    n = 0
    while n < len(lines):
        m = INSTANCE.match(lines[n])
        if not m:
            n += 1
            continue
        depth = 0
        end = n
        for k in range(n, len(lines)):
            code = re.sub(r'//.*$', '', lines[k])
            depth += code.count('(') - code.count(')')
            if depth <= 0:
                end = k
                break
        out.append((m.group(2), n, end))
        n = end + 1
    return out


def protected_spans(a, b, where):
    """V2 line spans of instances CARRIED OVER from the seed untouched.

    An attribution window must not be able to reach into one of these. The
    window exists because difflib splits one logical edit into many opcodes,
    and 140 lines is generous enough to cover a 165-port instantiation -- which
    is also generous enough to swallow a neighbouring instance nobody declared.
    A control caught exactly that: drift in an untouched instance seven lines
    below a declared marker was attributed to the marker and passed.

    Carried over means: the instance name exists in the seed too, and no
    declared marker sits inside its V2 span. The second half is what keeps the
    six swapped lease organs out of this set -- they share no name with V1
    anyway, but the bin pipe's `u_render_bin` DOES.
    """
    seed_names = {name for name, _, _ in instance_spans(a)}
    marked = sorted(n for ns in where.values() for n in ns)
    out = []
    for name, lo, hi in instance_spans(b):
        if name not in seed_names:
            continue
        if any(lo <= n <= hi for n in marked):
            continue
        out.append((lo, hi))
    return out


def marker_lines(b):
    """Every V2 line index at which each declared marker appears.

    A marker matching NOTHING is a dead rule, and a dead rule in a table of
    permissions is the flattering kind of broken: it silently permits nothing
    while looking like it permits something. Checked, not assumed.
    """
    where = {}
    for marker, why in DECLARED:
        where[marker] = [n for n, line in enumerate(b) if marker in line]
    return where


def attribute(where, protected, j1, j2):
    """The declared reason whose marker sits NEAREST this region, or None.

    Nearest rather than first-in-table. With first-in-table the seam header
    shadowed all six organ markers below it -- the tool passed, and eight of
    its seventeen rules were inert. `next()` over an ordered table is how a
    rule stops being load-bearing without anything going red.
    """
    if any(lo <= j2 and j1 <= hi for lo, hi in protected):
        return None      # inside an instance the seed carried over untouched
    best = None
    for marker, why in DECLARED:
        for n in where[marker]:
            if not (j1 - ATTRIB_WINDOW <= n <= j2 + 5):
                continue
            d = 0 if j1 <= n <= j2 else min(abs(n - j1), abs(n - j2))
            if best is None or d < best[0]:
                best = (d, why)
    return best[1] if best else None


def code_only(lines):
    """Strip comments, trailing comments and blank lines."""
    out = []
    for line in lines:
        if COMMENT.match(line):
            continue
        out.append(re.sub(r'\s*//.*$', '', line).rstrip())
    return [x for x in out if x]


def main(argv=None):
    import argparse
    import hashlib
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rtl', default=str(RTL),
                    help='directory holding both shells (controls only)')
    ap.add_argument('--expect-seed-hash', default=V1_SHA,
                    help='SHA-256 the seed must have (controls only)')
    args = ap.parse_args(argv)

    root = Path(args.rtl)
    V1, V2 = root / V1_NAME, root / V2_NAME
    expect = args.expect_seed_hash

    raw = V1.read_bytes()
    got = hashlib.sha256(raw).hexdigest()
    print('V1 protected hash   %s' % ('match' if got == expect else 'MISMATCH'))
    if got != expect:
        print('FAIL: the seed has moved; this comparison is against a different')
        print('  baseline and its result does not mean what it says.')
        return 1

    with io.open(V1, encoding='utf-8', errors='replace') as fh:
        a = fh.read().split('\n')
    with io.open(V2, encoding='utf-8', errors='replace') as fh:
        b = fh.read().split('\n')

    where = marker_lines(b)
    dead = [m for m, ns in where.items() if not ns]
    print('declared markers    %d, all present in V2: %s'
          % (len(DECLARED), 'yes' if not dead else 'NO'))
    if dead:
        for m in dead:
            print('  DEAD RULE, matches nothing: %s' % m)
        print('FAIL: a permission that grants nothing is not a permission.')
        print('  Either the marker text moved, or the region it named is gone.')
        return 1

    protected = protected_spans(a, b, where)
    print('carried-over instances %d, sealed against window attribution'
          % len(protected))

    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    identical = 0
    comment_only = 0
    declared = 0
    undeclared = []
    attributed = {}

    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            identical += i2 - i1
            continue
        old, new = a[i1:i2], b[j1:j2]
        if code_only(old) == code_only(new):
            comment_only += 1
            continue
        # substantive -- a declared marker must sit within the window
        hit = attribute(where, protected, j1, j2)
        if hit:
            declared += 1
            attributed.setdefault(hit, 0)
            attributed[hit] += 1
        else:
            undeclared.append((i1 + 1, i2, j1 + 1, j2, new[:3]))

    print('V1 lines            %d' % len(a))
    print('V2 lines            %d' % len(b))
    print('identical lines     %d  (%.1f%% of V1)'
          % (identical, 100.0 * identical / max(1, len(a))))
    print('comment-only regions %d' % comment_only)
    print('declared regions     %d, attributed to %d of %d reasons'
          % (declared, len(attributed), len(DECLARED)))
    for why in sorted(attributed):
        print('    %2d  %s' % (attributed[why], why))

    if undeclared:
        print()
        print('== UNDECLARED SUBSTANTIVE CHANGE (%d) ==' % len(undeclared))
        for i1, i2, j1, j2, sample in undeclared:
            print('  V1 %d-%d  ->  V2 %d-%d' % (i1, i2, j1, j2))
            for s in sample:
                print('      %s' % s[:96])
        print()
        print('  The sibling was seeded so the untouched instances stay')
        print('  byte-identical. Either this change belongs in DECLARED with a')
        print('  reason, or it is drift.')
        return 1

    print()
    print('packet-h sibling diff: PASS -- no undeclared substantive change')
    return 0


# SELF-CHECK: the classifier must separate a comment change from a code change.
assert code_only(['  // a', '', '  x = 1;  // b']) == ['  x = 1;']
assert code_only(['// only a comment']) == []
assert code_only(['  x = 1;']) != code_only(['  x = 2;'])

# SELF-CHECK: the span finder must find a span, and must end it at the `);`
# rather than at a paren inside a comment. A finder that matches nothing
# reports zero carried-over instances and seals nothing -- silently, and in
# the flattering direction.
_SPANS = instance_spans([
    'module x ();',
    '  zhao_thing u_thing (',
    '    .clk(clk),          // note ) in a comment',
    '    .d(d)',
    '  );',
    '  wire y;',
])
assert _SPANS == [('u_thing', 1, 4)], _SPANS

if __name__ == '__main__':
    sys.exit(main())
