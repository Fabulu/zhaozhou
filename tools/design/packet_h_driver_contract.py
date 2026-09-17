r"""Where does every NEW input of the swapped V2 blocks get its value?

Not a gate. A work list for Packet H, computed rather than remembered, and
committed rather than thrown away -- an unreproducible number is an opinion.
`reports/PACKET-H-DRIVER-CONTRACT-20260917.md` is this script's output with
judgement applied; run this to see whether that report has gone stale.

TWO PARSER BUGS THIS SCRIPT HAD, BOTH SILENT AND BOTH FLATTERING, which is why
there are self-checks at import:

  * `\bmodule\s+\w+` matched the phrase "module inputs," inside
    zhao_geom_bin_pipe.sv's header COMMENT, took the next '(' out of a
    sentence, and parsed prose as a port list -- so the module reported ZERO
    ports and every comparison against it found nothing to report.
  * anchored to the real declaration, it then took the PARAMETER list `#(...)`
    as the port list. A parameter list contains no `input`/`output`, so: zero
    ports again, and a contract that came back short and looked finished.

Both read LOW, and a work list that reads low looks like progress. The known
counts below are asserted at import so neither can come back quietly.
"""
import io
import re
import sys
from pathlib import Path

REPO = Path(r'C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912')

# The lookahead accepts END OF LINE as well as ',' or ')'. Without that it
# silently drops the last port before a LEADING-COMMA continuation, which is the
# style zhao_geom_bin_pipe_v2 uses for its final three:
#
#       output logic [23:0] z_floor_o
#     , input  logic  [4:0] test_start_enable_i
#
# `z_floor_o` has no trailing comma, so it vanished, and the count came back 166
# against a true 168. A per-LINE scan gets it wrong the other way -- it misses
# the three comma-led declarations and reports 165, which is the number the
# roadmap recorded by hand. Three methods, three answers, ALL OF THEM LOW.
PORT_RE = re.compile(
    r'\b(input|output|inout)\s+(?:var\s+)?(?:logic|wire|reg)?\s*'
    r'(?:signed\s+)?(?:\[[^\]]*\]\s*)*([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?(?=[,)]|$)',
    re.M)


def find(name):
    hits = list(REPO.glob('fpga/rtl/**/%s.sv' % name))
    assert len(hits) == 1, (name, hits)
    return hits[0]


def balanced(t, o):
    """Index of the ')' closing the '(' at o."""
    depth, i = 0, o
    while i < len(t):
        if t[i] == '(':
            depth += 1
        elif t[i] == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise AssertionError('unbalanced')


def header(path, name):
    t = io.open(path, encoding='utf-8', errors='replace').read()
    # ANCHORED TO THE DECLARATION, not to the word. `\bmodule\s+\w+` matched the
    # phrase "module inputs," inside zhao_geom_bin_pipe.sv's header comment,
    # took the next '(' from a sentence, and returned prose as a port list.
    m = re.search(r'^\s*module\s+' + re.escape(name) + r'\b', t, re.M)
    assert m, 'no module declaration for %s' % name
    # SKIP THE PARAMETER LIST. Without this, `module foo #(...) (...)` hands back
    # the PARAMETERS as the port list -- and a parameter list contains no
    # `input`/`output`, so the module reports ZERO ports and every comparison
    # against it silently finds nothing to report. That is the same bug this
    # repo already fixed once in packet_h_wiring_survey.py, written again here,
    # and it read 23 new inputs where the true answer is larger. Precision at
    # zero is a tell, not a result.
    o = t.index('(', m.end())
    if '#' in t[m.end():o]:
        o = t.index('(', balanced(t, o) + 1)
    return t[o:balanced(t, o) + 1]


def ports(name):
    pl = [(d, p) for d, p in PORT_RE.findall(header(find(name), name))]
    # A module with no ports is a broken parse here, not a real module.
    assert pl, 'parsed ZERO ports for %s -- the parser is wrong, not the RTL' % name
    return pl


def stem(p):
    return re.sub(r'_[io]$', '', p)


# SELF-CHECKS, and these are the CORRECTED numbers. The roadmap records
# 63 -> 165 and 22 -> 73 from a hand count; this script's first version asserted
# 166 and PASSED, enshrining its own blind spot as a fact -- which is precisely
# the failure a self-check exists to prevent, arriving inside the self-check.
# See the note on PORT_RE. A parser that cannot reproduce these is broken, and
# the way it breaks is by reporting less work than exists.
for _m, _n in (('zhao_geom_bin_pipe', 63), ('zhao_geom_bin_pipe_v2', 168),
               ('zhao_video_slotmgr', 22), ('zhao_video_slotmgr_v2', 74)):
    _got = len(ports(_m))
    assert _got == _n, 'port count for %s: expected %d, parsed %d' % (_m, _n, _got)

ORGANS = ['zhao_renderer_lease_v2', 'zhao_video_terminal_adapter_v2',
          'zhao_video_ready_bridge_v2', 'zhao_engine1_raw_last_v2',
          'zhao_video_blit_lease_v2', 'zhao_fb_ready_cdc_v2']

# every stem an organ can DRIVE
offered = {}
for o in ORGANS:
    for d, p in ports(o):
        if d == 'output':
            offered.setdefault(stem(p), []).append(o)

shell_text = io.open(find('zhao_shell_top'), encoding='utf-8', errors='replace').read()
shell_ids = set(re.findall(r'\b([A-Za-z_]\w*)\b', shell_text))

SWAPS = {'zhao_geom_bin_pipe_v2': 'zhao_geom_bin_pipe',
         'zhao_video_slotmgr_v2': 'zhao_video_slotmgr'}

rows = []
for v2, v1 in SWAPS.items():
    old = {p for _, p in ports(v1)}
    for d, p in ports(v2):
        if p in old or d != 'input':
            continue
        s = stem(p)
        if s in offered:
            where = 'ORGAN:' + ','.join(sorted(set(offered[s])))
        elif p in shell_ids or s in shell_ids:
            where = 'SHELL'
        else:
            where = 'NEW'
        rows.append((v2, p, where))

by = {}
for v2, p, where in rows:
    by.setdefault(where.split(':')[0], []).append((v2, p, where))

print('NEW INPUTS ON THE SWAPPED BLOCKS THAT NEED A DRIVER: %d' % len(rows))
for k in ('ORGAN', 'SHELL', 'NEW'):
    v = by.get(k, [])
    print()
    print('== %s (%d) ==' % (k, len(v)))
    for v2, p, where in sorted(v):
        print('  %-24s %-32s %s' % (v2.replace('zhao_', ''), p,
                                    where if k == 'ORGAN' else ''))
