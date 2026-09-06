"""Independent off-by-one hunt. Fixes boundscan's known blind spots:
 - strips block comments AND string/char literals before analysis
 - counts initializer entries paren/bracket aware
 - accepts std::array<T,N>, non-const/static arrays, any identifier (not just kXxx)
 - flags explicit-size vs initializer-count mismatch
 - reports the MAX LITERAL INDEX ever used per table (unreachable tail entries)
"""
import io, os, re, sys, collections

root = sys.argv[1]
SKIP = ('.git', 'build', 'third_party', 'node_modules')
files = []
for dp, dn, fn in os.walk(root):
    dn[:] = [d for d in dn if d not in SKIP]
    for f in sorted(fn):
        if f.endswith(('.h', '.hpp', '.cpp', '.cc')):
            files.append(os.path.join(dp, f))

def scrub(t):
    """Blank out comments and string/char literals, preserving line structure."""
    out, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if c == '/' and i + 1 < n and t[i+1] == '/':
            j = t.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i)); i = j
        elif c == '/' and i + 1 < n and t[i+1] == '*':
            j = t.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in t[i:j])); i = j
        elif c in '"\'':
            q, j = c, i + 1
            while j < n and t[j] != q:
                j += 2 if t[j] == chr(92) else 1
            j = min(j + 1, n)
            out.append(''.join(ch if ch == '\n' else 'S' for ch in t[i:j])); i = j
        else:
            out.append(c); i += 1
    return ''.join(out)

def count_entries(t, brace):
    """Top-level comma count inside {...}, ignoring nested (), [], {}."""
    d_br = d_par = d_sq = 0
    items, j, started = 0, brace, False
    nonempty = False
    while j < len(t):
        c = t[j]
        if c == '{':
            d_br += 1; started = True
        elif c == '}':
            d_br -= 1
            if d_br == 0: break
        elif c == '(': d_par += 1
        elif c == ')': d_par -= 1
        elif c == '[': d_sq += 1
        elif c == ']': d_sq -= 1
        elif c == ',' and d_br == 1 and d_par == 0 and d_sq == 0:
            items += 1
        elif d_br >= 1 and not c.isspace():
            nonempty = True
        j += 1
    if not nonempty: return 0, j
    body = t[brace+1:j].rstrip()
    return (items if body.endswith(',') else items + 1), j

# --- collect tables -------------------------------------------------------
CARR = re.compile(r'\b(?:constexpr|const|static)\b[\w:<>,\s*&]*?\b(\w{2,})\s*\[\s*([\w+\-* ]*?)\s*\]\s*=\s*\{')
CSTD = re.compile(r'\bstd::array\s*<[^;>]*?,\s*(\w+)\s*>\s*(\w+)\s*=\s*\{')
texts, tables = {}, collections.defaultdict(list)
for p in files:
    raw = io.open(p, encoding='utf-8', errors='replace').read()
    t = scrub(raw)
    texts[p] = (raw, t)
    for m in CARR.finditer(t):
        name, size = m.group(1), m.group(2).strip()
        n, _ = count_entries(t, m.end() - 1)
        if n:
            tables[name].append((n, size, p, t[:m.start()].count('\n') + 1))
    for m in CSTD.finditer(t):
        size, name = m.group(1).strip(), m.group(2)
        n, _ = count_entries(t, m.end() - 1)
        if n:
            tables[name].append((n, size, p, t[:m.start()].count('\n') + 1))

print('=== A. EXPLICIT SIZE vs INITIALIZER COUNT MISMATCH ===')
mm = 0
for name, defs in sorted(tables.items()):
    for n, size, p, ln in defs:
        if size.isdigit() and int(size) != n:
            print('  %s:%d  %s[%s] but initializer has %d entries' % (p, ln, name, size, n)); mm += 1
print('  (%d)' % mm)

print('\n=== B. UNREACHABLE TAIL: max literal index used < entries-1, and no sizeof/Count constant ===')
allsrc = '\n'.join(t for _, t in texts.values())
ur = 0
for name, defs in sorted(tables.items()):
    if len(defs) != 1: continue          # skip name collisions
    n, size, p, ln = defs[0]
    if n < 3: continue
    idxs = [int(x) for x in re.findall(re.escape(name) + r'\s*\[\s*(\d+)\s*\]', allsrc)]
    has_count = bool(re.search(r'sizeof\s*\(\s*' + re.escape(name) + r'\s*\)', allsrc))
    # any non-literal index at all?
    nonlit = re.search(re.escape(name) + r'\s*\[\s*(?!\d+\s*\])', allsrc)
    if has_count or nonlit or not idxs: continue
    if max(idxs) < n - 1:
        print('  %s:%d  %s[] has %d entries; highest literal index used is %d '
              '-> entries %d..%d NEVER READ' % (p, ln, name, n, max(idxs), max(idxs)+1, n-1)); ur += 1
print('  (%d)' % ur)

print('\n=== C. GUARDS: every <,<=,>,>=,% literal near an index of a table (no delta filter) ===')
GUARD = re.compile(r'(?<![<>])(<=|>=|<|>|%)\s*(\d+)\b')
hits, seen = [], set()
for p in files:
    raw, t = texts[p]
    lines = t.split('\n'); rawl = raw.split('\n')
    for i, ln in enumerate(lines):
        for name, defs in tables.items():
            if len(defs) != 1: continue
            n, size, dp, dl = defs[0]
            if not re.search(re.escape(name) + r'\s*\[', ln): continue
            if i + 1 == dl and dp == p: continue      # skip the decl line itself
            for w in range(max(0, i-10), min(len(lines), i+11)):
                for gm in GUARD.finditer(lines[w]):
                    op, v = gm.group(1), int(gm.group(2))
                    bad = ((op == '<' and v != n) or (op == '<=' and v != n-1)
                           or (op == '%' and v != n))
                    if not bad or abs(v - n) > 2: continue
                    k = (p, i+1, name, op, v)
                    if k in seen: continue
                    seen.add(k)
                    hits.append((p, i+1, name, n, op, v, rawl[i].strip()[:95],
                                 rawl[w].strip()[:95], dp, dl))
for h in sorted(hits):
    print('  %s:%d  %s[] has %d entries; guard `%s %d`\n      idx  : %s\n      guard: %s\n      decl : %s:%d'
          % (h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9]))
print('  (%d)' % len(hits))
print('\ntables: %d  files: %d' % (sum(len(v) for v in tables.values()), len(files)))
