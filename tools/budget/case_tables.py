#!/usr/bin/env python3
"""Find COMBINATIONAL CASE TABLES -- logic that is a lookup wearing a mux.

WHY THIS EXISTS. On 2026-09-28 a five-row map sweep measured that
`zhao_field_rcp24_rom` -- a 256-arm `unique case` in an `always_comb` -- costs
**145 comb ALUTs and zero memory bits**, and that REGISTERING ITS READ turns it
into one `altsyncram depth 256 width 31, mode ROM` at **zero ALUTs**. No
restructuring into an indexed array was required.
See reports/MEASURED-20260928-A-REGISTERED-READ-TURNS-THE-CASE-INTO-A-ROM.md.

That makes "which other blocks are tables?" a question worth answering
MECHANICALLY rather than by reading twelve modules and forming an impression.
ALMs are the binding constraint at the ceiling; the device has 246 free M10K.

WHAT THIS IS NOT. It reports CANDIDATES, not savings. A candidate becomes a
saving only when someone checks that its call sites can absorb a cycle of
latency -- and for a declared `latency: fixed:N` block that is a contract change
with consumers, not an edit. This tool ranks; it does not decide. That
distinction is the whole of CLAUDE.md's art law applied to area work:
measurement belongs on the comparison side.

ANTI-VACUITY. A scanner that silently matches nothing reports "no opportunities"
-- good news, in the flattering direction, which is the one nobody audits. So
this asserts at import that it still finds the known-good example with the arm
count and width the map measured. If that assertion fails the tool exits; it
does not quietly report fewer tables.

Usage:
  python tools/budget/case_tables.py                # ranked candidates
  python tools/budget/case_tables.py --min-arms 32
  python tools/budget/case_tables.py --self-test    # just the guard
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RTL = ROOT / 'fpga' / 'rtl'

MODULE = re.compile(r'^\s*module\s+([A-Za-z_][\w$]*)', re.M)
ENDMODULE = re.compile(r'^\s*endmodule', re.M)
# `8'd12:` / `4'b0010:` / `16'h00FF:` / `3'o7:` -- a SIZED CONSTANT arm label.
ARM = re.compile(r"^\s*(\d+)'([bodhBODH])[0-9a-fA-FxzXZ_]+\s*:", re.M)
CASE_OPEN = re.compile(r'^\s*(unique\s+|priority\s+)?case[xz]?\s*\(', re.M)
CASE_CLOSE = re.compile(r'^\s*endcase', re.M)
ALWAYS_COMB = re.compile(r'^\s*(always_comb|always\s*@\s*\*|always\s*@\s*\(\s*\*\s*\))', re.M)
ALWAYS_FF = re.compile(r'^\s*always_ff|^\s*always\s*@\s*\(\s*posedge', re.M)
# `x = 31'h7FC01FF0;` -- an arm body that assigns a CONSTANT, which is what
# makes it a table rather than a decode.
CONST_ASSIGN = re.compile(r"=\s*(\d+)'[bodhBODH][0-9a-fA-FxzXZ_]+\s*;")


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def scan_file(path):
    """Yield one record per case block that looks like a constant table."""
    raw = path.read_text(encoding='utf-8', errors='replace')
    text = strip_comments(raw)

    # Which module does an offset belong to?
    bounds = []
    for m in MODULE.finditer(text):
        end = ENDMODULE.search(text, m.end())
        bounds.append((m.start(), end.end() if end else len(text), m.group(1)))

    def owner(pos):
        for s, e, name in bounds:
            if s <= pos < e:
                return name
        return '<file scope>'

    out = []
    for om in CASE_OPEN.finditer(text):
        cm = CASE_CLOSE.search(text, om.end())
        if not cm:
            continue
        body = text[om.end():cm.start()]
        arms = ARM.findall(body)
        if not arms:
            continue
        consts = CONST_ASSIGN.findall(body)
        # A table's arms overwhelmingly assign constants. A decode/FSM assigns
        # expressions and next-state names, so this ratio separates them.
        const_ratio = len(consts) / len(arms) if arms else 0.0
        widths = sorted({int(w) for w in consts}, reverse=True)
        # Clocked or combinational? Look back a little for the enclosing block.
        head = text[max(0, om.start() - 400):om.start()]
        ff = bool(ALWAYS_FF.search(head))
        comb = bool(ALWAYS_COMB.search(head))
        out.append({
            'file': str(path.relative_to(ROOT)).replace('\\', '/'),
            'module': owner(om.start()),
            'arms': len(arms),
            'const_assigns': len(consts),
            'const_ratio': const_ratio,
            'width': widths[0] if widths else 0,
            'clocked': ff,
            'comb': comb and not ff,
            'line': text[:om.start()].count('\n') + 1,
        })
    return out


def scan_tree():
    recs = []
    for p in sorted(RTL.rglob('*.sv')):
        recs.extend(scan_file(p))
    return recs


def self_test():
    """The known-good example, with the numbers the MAP measured."""
    p = RTL / 'field' / 'zhao_field_rcp24_rom.sv'
    if not p.exists():
        sys.exit('self-test: zhao_field_rcp24_rom.sv is gone; update this guard')
    hits = [r for r in scan_file(p) if r['module'] == 'zhao_field_rcp24_rom']
    if not hits:
        sys.exit('SELF-TEST FAILED: the known-good case table was not matched. '
                 'The pattern has drifted; every "no candidates" result from '
                 'this tool would be a false negative.')
    r = max(hits, key=lambda x: x['arms'])
    if r['arms'] != 256:
        sys.exit(f"SELF-TEST FAILED: expected 256 arms, matched {r['arms']}")
    if r['width'] != 31:
        sys.exit(f"SELF-TEST FAILED: expected width 31, matched {r['width']}")
    if not r['comb']:
        sys.exit('SELF-TEST FAILED: the known-good table is combinational and '
                 'was not classified as such')
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--min-arms', type=int, default=16)
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--all', action='store_true',
                    help='include clocked and low-constant-ratio blocks')
    a = ap.parse_args()

    ref = self_test()
    print(f"self-test PASSED: {ref['module']} still matches "
          f"({ref['arms']} arms x {ref['width']} bits, combinational)\n")
    if a.self_test:
        return 0

    recs = scan_tree()
    cand = [r for r in recs if r['arms'] >= a.min_arms]
    if not a.all:
        # A table assigns constants in most arms and is not already clocked.
        cand = [r for r in cand if r['const_ratio'] >= 0.8 and r['comb']]
    cand.sort(key=lambda r: r['arms'] * max(r['width'], 1), reverse=True)

    print(f"scanned {len(list(RTL.rglob('*.sv')))} files, {len(recs)} case block(s)")
    print(f"{len(cand)} COMBINATIONAL CONSTANT-TABLE candidate(s) "
          f"with >= {a.min_arms} arms\n")
    if not cand:
        print('none -- and note the self-test above passed, so this is a real '
              'absence rather than a dead pattern.')
        return 0

    print(f"{'module':<38}{'arms':>6}{'bits':>6}{'tblbits':>9}  file:line")
    total = 0
    for r in cand:
        bits = r['arms'] * r['width']
        total += bits
        print(f"{r['module']:<38}{r['arms']:>6}{r['width']:>6}{bits:>9}  "
              f"{r['file']}:{r['line']}")
    print(f"\n{'':<38}{'':>6}{'':>6}{total:>9}  total table bits")
    print(f"\nM10K is 10,240 bits. These would occupy roughly "
          f"{-(-total // 10240)} block(s) of the 246 free.")
    print('\nCANDIDATES, NOT SAVINGS. Each becomes real only if its call sites '
          'can absorb one cycle;\nfor a declared `latency: fixed:N` block that '
          'is a contract change with consumers.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
