"""Find hand-written bounds that disagree with the constant table they guard.

Two shipped defects share one shape -- a LITERAL comparison bound written
beside an indexed constexpr table:
  pass 5: `< 14` on a 15-entry table
  pass 6: `< 5`  on a  6-entry table (kSmearPresets; 12 of 15 clips lost the smear)

Scope: project-convention constant tables (kXxx). For every line that INDEXES
one, the +/-6 line window is searched for a literal `< N` / `<= N` / `% N`
guard, and N is compared with the table's real extent.
"""
import io, os, re, sys

root = sys.argv[1] if len(sys.argv) > 1 else '.'
SKIP = ('.git', 'build', 'third_party', 'node_modules')
files = []
for dp, dn, fn in os.walk(root):
    dn[:] = [d for d in dn if d not in SKIP]
    for f in fn:
        if f.endswith(('.h', '.hpp', '.cpp', '.cc')):
            files.append(os.path.join(dp, f))


def entries(text, brace):
    depth, items, groups, j = 0, 0, 0, brace
    while j < len(text):
        ch = text[j]
        if ch == '{':
            depth += 1
            if depth == 2:
                groups += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                break
        elif ch == ',' and depth == 1:
            items += 1
        j += 1
    if groups:
        return groups
    body = re.sub(r'//[^\n]*', '', text[brace + 1:j]).strip().rstrip(',')
    return len([x for x in body.split(',') if x.strip()]) if body else 0


decl = re.compile(
    r'\b(?:constexpr|const)\s+[\w:<>,\s*&]+?\b(k\w{3,})\s*\[\s*(\d*)\s*\]\s*=\s*(\{)')
tables = {}
for path in files:
    txt = io.open(path, encoding='utf-8', errors='replace').read()
    for m in decl.finditer(txt):
        name, size = m.group(1), m.group(2)
        n = int(size) if size else entries(txt, m.start(3))
        if n:
            tables[name] = (n, path, txt[:m.start()].count('\n') + 1)

guard = re.compile(r'(?:<=?|%)\s*(\d+)\b')
hits, seen = [], set()
for path in files:
    lines = io.open(path, encoding='utf-8', errors='replace').read().split('\n')
    for i, ln in enumerate(lines):
        for name, (n, dpath, dline) in tables.items():
            if (name + '[') not in ln:
                continue
            for wl in lines[max(0, i - 6):i + 7]:
                if '//' in wl:
                    wl = wl[:wl.index('//')]
                for gm in guard.finditer(wl):
                    v = int(gm.group(1))
                    if v == n or v < 2 or abs(v - n) > 4:
                        continue
                    k = (path, i + 1, name, v)
                    if k in seen:
                        continue
                    seen.add(k)
                    hits.append((path, i + 1, name, n, v, ln.strip()[:100],
                                 wl.strip()[:100], dpath, dline))

for h in sorted(hits):
    print('%s:%d\n    indexes %s[] which has %d entries, but a guard says %d\n'
          '    index : %s\n    guard : %s\n    table : %s:%d\n'
          % (h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8]))
print('tables: %d   files: %d   SUSPECTS: %d' % (len(tables), len(files), len(hits)))
