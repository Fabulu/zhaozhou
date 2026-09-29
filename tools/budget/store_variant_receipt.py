#!/usr/bin/env python3
"""Per-variant receipt for a storage-conversion experiment, with ATTRIBUTION.

WHY THIS EXISTS. The external R5 feedback asked for a machine-readable receipt
per variant and, more importantly, for a predicate stronger than "total memory
increased". That predicate is unsound in three distinct ways, and this repository
has already shipped two of them:

  * AN UNRELATED RAM CAN BE COUNTED AS THE REPLACEMENT. This is exactly the
    defect in `check_ram_inference.py`'s old `ALREADY INFERRING` label, which
    compared a MODULE's subtree memory against ONE array's declared size. A
    module holding a larger unrelated RAM labelled the array as inferred.
  * THE ARRAY CAN BE DELETED RATHER THAN CONVERTED. Reducing a module to find a
    failure removes consumers, and synthesis then removes the object. Memory
    bits go to zero, registers go to zero, and nothing infers -- which reads
    like progress and is the absence of the thing under test.
  * THE ROW CAN BE STALE. A map that did not rerun reports the previous
    configuration, and a measurement that did not move after a change that must
    have moved it is this tree's most repeated trap.

So the question is never "did memory change". It is **"what did Quartus say
about THIS array, by name"** -- and Quartus says it explicitly:

    Info (276007): RAM logic "<path>|<array>" is uninferred due to <reason>
    ... or an inferred-memory entry whose name contains the array

WHAT IT REPORTS, per variant:
  verdict        INFERRED | UNINFERRED | ABSENT | AMBIGUOUS
  reason         Quartus's own words when it declined
  shape          depth x width and mode of the memory attributed to this array
  resources      registers, combinational ALUTs, estimated ALMs, memory bits
  provenance     source digest, parameter overrides, device, tool version, clean

`ABSENT` is the verdict this tool exists to produce. It is not a pass.

Usage:
  python tools/budget/store_variant_receipt.py --report <map.rpt> --array store \\
      [--row <module@label>] [--json OUT.json]
"""
import argparse
import hashlib
import io
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEDGER = ROOT / 'reports' / 'synthesis' / 'zhao_block_map.json'

# Quartus's own attribution lines.
UNINFERRED = re.compile(
    r'Info \(276007\): RAM logic "([^"]+)" is uninferred due to ([^\r\n]*?)(?: File:|$)',
    re.M)
INFERRED_HDR = re.compile(r'Inferred RAM|Inferred Memory', re.I)
# `altsyncram:NAME_rtl_0|...`, and the RAM-summary table's own name column.
ALTSYNC = re.compile(r'altsyncram:([A-Za-z0-9_\[\]$]+)')


def read(path):
    return io.open(path, encoding='utf-8', errors='replace').read()


def digest_sources(paths):
    h = hashlib.sha256()
    for p in sorted(paths):
        fp = ROOT / p
        if fp.exists():
            h.update(fp.read_bytes())
    return h.hexdigest()[:16]


def classify(report_text, array):
    """What did Quartus say about THIS array, by name?"""
    hits_un = [(m.group(1), m.group(2).strip())
               for m in UNINFERRED.finditer(report_text)
               if m.group(1).split('|')[-1] == array]
    # an inferred memory is attributed only if its own name carries the array
    names = set(ALTSYNC.findall(report_text))
    hits_in = sorted(n for n in names
                     if n == array or n.startswith(array + '_')
                     or n.startswith(array + '['))
    if hits_un and hits_in:
        return 'AMBIGUOUS', hits_un[0][1], hits_in
    if hits_un:
        return 'UNINFERRED', hits_un[0][1], []
    if hits_in:
        return 'INFERRED', '', hits_in
    # Named nowhere: either the declaration is not in this closure, or synthesis
    # removed the object. Both are ABSENT and neither is a pass.
    return 'ABSENT', ('the array is named in no 276007 line and no altsyncram: '
                      'either it is outside this closure or synthesis deleted '
                      'it. Deletion is NOT successful inference.'), []


def shape_for(report_text, names):
    """depth x width and mode of the memories attributed to this array."""
    out = []
    for n in names:
        for m in re.finditer(re.escape(n) + r'[^\r\n]{0,400}', report_text):
            seg = m.group(0)
            d = re.search(r'\b(\d+)\s*words?\b|depth[^0-9]{0,8}(\d+)', seg, re.I)
            w = re.search(r'\b(\d+)\s*bits?\s*wide\b|width[^0-9]{0,8}(\d+)', seg, re.I)
            mode = re.search(r'(Simple Dual Port|True Dual Port|Single Port|ROM)', seg)
            if d or w or mode:
                out.append({
                    'name': n,
                    'depth': next((g for g in (d.groups() if d else ()) if g), None),
                    'width': next((g for g in (w.groups() if w else ()) if g), None),
                    'mode': mode.group(1) if mode else None,
                })
                break
    return out


def ledger_row(label):
    if not LEDGER.exists():
        return None
    d = json.load(io.open(LEDGER, encoding='utf-8'))
    for r in d.get('blocks', []):
        if r.get('module') == label:
            return r
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report', required=True, help='the variant .map.rpt')
    ap.add_argument('--array', required=True, help='the array under test, e.g. store')
    ap.add_argument('--row', help='module@label in zhao_block_map.json')
    ap.add_argument('--sources', nargs='*', default=[],
                    help='repo-relative sources to digest for provenance')
    ap.add_argument('--json', dest='out')
    a = ap.parse_args()

    rp = pathlib.Path(a.report)
    if not rp.exists():
        sys.exit('report not found: %s' % rp)
    text = read(rp)

    # ANTI-VACUITY. If the report carries no 276007 line and no altsyncram at
    # all, this parser is looking at the wrong file or the format moved, and
    # every verdict below would be a false ABSENT -- good news, in the direction
    # nobody audits.
    if not UNINFERRED.search(text) and not ALTSYNC.search(text):
        sys.exit('ANTI-VACUITY: the report contains neither an Info (276007) '
                 'line nor any altsyncram reference. Refusing to classify: a '
                 'verdict from a report this parser cannot read would be a '
                 'false ABSENT.')

    verdict, reason, names = classify(text, a.array)
    row = ledger_row(a.row) if a.row else None

    rec = {
        'array': a.array,
        'verdict': verdict,
        'quartus_reason': reason,
        'attributed_memories': names,
        'shape': shape_for(text, names),
        'report': str(rp),
        'report_sha256_16': hashlib.sha256(text.encode('utf-8', 'replace')).hexdigest()[:16],
        'sources_digest': digest_sources(a.sources) if a.sources else None,
        'row': None if row is None else {
            'module': row.get('module'),
            'topParameters': row.get('topParameters'),
            'device': row.get('device'),
            'tool': row.get('tool'),
            'registers': row.get('registers'),
            'combAluts': row.get('combAluts'),
            'estimatedAlms': row.get('estimatedAlms'),
            'blockMemoryBits': row.get('blockMemoryBits'),
            'mlabMemoryBits': row.get('mlabMemoryBits'),
            'inferredMemoryCount': row.get('inferredMemoryCount'),
            'rtlCleanAtHead': row.get('rtlCleanAtHead'),
            'seconds': row.get('seconds'),
            'critical': row.get('critical'),
        },
        'not_claimed': [
            'estimatedAlms is a pre-placement ESTIMATE and is not a placed area',
            'blockMemoryBits is logical; physical RAM blocks are a separate count',
            'map-only establishes no setup/hold and no integrated timing',
            'a resource delta against a row with different parameters is not a delta',
        ],
    }

    print('array            : %s' % rec['array'])
    print('VERDICT          : %s' % rec['verdict'])
    if reason:
        print('quartus says     : %s' % reason)
    if names:
        print('attributed to    : %s' % ', '.join(names))
    for s in rec['shape']:
        print('  shape          : %s depth=%s width=%s mode=%s'
              % (s['name'], s['depth'], s['width'], s['mode']))
    if row:
        print('params           : %s' % row.get('topParameters'))
        print('device / tool    : %s / %s' % (row.get('device'), row.get('tool')))
        print('reg / ALUT / ALM : %s / %s / %s'
              % (row.get('registers'), row.get('combAluts'), row.get('estimatedAlms')))
        print('memBits / MLAB   : %s / %s'
              % (row.get('blockMemoryBits'), row.get('mlabMemoryBits')))
        print('clean / seconds  : %s / %s'
              % (row.get('rtlCleanAtHead'), row.get('seconds')))
    if verdict == 'ABSENT':
        print()
        print('ABSENT IS NOT A PASS. The array is named nowhere, so either it is '
              'outside this closure or synthesis removed it.')

    if a.out:
        json.dump(rec, io.open(a.out, 'w', encoding='utf-8'), indent=2)
        print('\nwrote %s' % a.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
