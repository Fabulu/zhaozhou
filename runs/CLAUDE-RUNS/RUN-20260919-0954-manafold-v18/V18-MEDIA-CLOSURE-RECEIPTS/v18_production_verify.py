"""Cache-bypassed production verification for Manafold version 18.

usage: v18_production_verify.py <out.json> <host> [host ...]
Compares index.html (against the local deployed public/index.html) and the
44 live media (against V18-LIVE-MEDIA-SHA256.txt) plus archive spot checks
(against V17-ARCHIVE-SHA256.txt). Exit 1 on any mismatch."""
import hashlib, json, re, sys, time, urllib.request, uuid
from pathlib import Path
UP = Path(r'C:\programmieren\zencrifice\manafold-p16\Upheaval')
site = UP / 'website'
live = [l.split() for l in (UP/'creature/Manafold/V18-LIVE-MEDIA-SHA256.txt').read_text().splitlines() if l.strip() and not l.startswith('#')]
arch = [l.split('\t') for l in (UP/'creature/Manafold/V17-ARCHIVE-SHA256.txt').read_text(encoding='utf-8').splitlines() if l.strip() and not l.startswith('#')]
spot = [(d, int(n), a) for d, n, _, a in arch if a.endswith(('archive-v17-manafold-trick.webm','archive-v17-manafold-hover.png','archive-v17-manafold-mana-boil.webm'))]
assert len(live) == 44 and len(spot) == 3
idx = (site/'public/index.html').read_bytes()
H = {'Cache-Control': 'no-cache, no-store, max-age=0', 'Pragma': 'no-cache', 'User-Agent': 'v18-verify'}
def get(url):
    for attempt in range(1, 6):
        try:
            with urllib.request.urlopen(urllib.request.Request(f'{url}?v18verify={uuid.uuid4().hex}', headers=H), timeout=120) as r:
                return r.status, r.read(), attempt
        except Exception as e:
            err = e; time.sleep(5 * attempt)
    return 0, repr(err).encode(), 5
def index_checks(b):
    t = b.decode('utf-8'); nc = re.sub(r'(?s)<!--.*?-->', '', t)
    robots = re.findall(r'(?is)<meta\s+[^>]*name\s*=\s*"robots"[^>]*>', nc)
    vids = {m.group(1): m.group(0) for m in re.finditer(r'(?is)<video\b[^>]*?src="(renders/[^"]+)"[^>]*>', t)}
    if not vids:
        vids = {}
        for m in re.finditer(r'(?is)<video\b.*?</video>', t):
            s = re.search(r'src="(renders/[^"]+\.webm)"', m.group(0))
            if s: vids[s.group(1)] = m.group(0)
    tag = lambda k: next((v for s, v in vids.items() if s == k), '')
    fall, hover = tag('renders/manafold-fall.webm'), tag('renders/manafold-hover.webm')
    opening = lambda v: v.split('>')[0]
    c = {
      'robots_exactly_one_noindex_nofollow': len(robots) == 1 and 'content="noindex, nofollow"' in robots[0],
      'renderer_md5': '0f082622d4ca0c58d012d1f0de555723' in t,
      'manifest_sha256': 'bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd' in t,
      'source_db2bcf0e': 'db2bcf0e' in t,
      '22_subjects_7992_frames': ('22 live subjects — 7,992 frames' in t) or ('22 live subjects &mdash; 7,992 frames' in t),
      'all_44_live_declared': all(p in t for _, _, p in live),
      'fall_no_autoplay_no_loop': bool(fall) and not re.search(r'\b(autoplay|loop)\b', opening(fall)),
      'hover_autoplay_loop': bool(hover) and re.search(r'\bautoplay\b', opening(hover)) is not None and re.search(r'\bloop\b', opening(hover)) is not None,
      'no_pass_18': re.search(r'(?i)pass[ -]18', t) is None,
    }
    return c
out = {'hosts': {}}; bad = 0
for host in sys.argv[2:]:
    rows = []; tot = 0; ok = 0
    st, b, att = get(f'{host}/')
    good = st == 200 and b == idx
    checks = index_checks(b) if st == 200 else {}
    good = good and all(checks.values())
    rows.append({'path': 'index.html', 'status': st, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest(), 'ok': good, 'attempt': att, 'checks': checks})
    ok += good; tot += len(b)
    for d, n, p in [(d, int(n), p) for d, n, p in live] + spot:
        st, b, att = get(f'{host}/{p}')
        g = st == 200 and len(b) == n and hashlib.sha256(b).hexdigest() == d
        rows.append({'path': p, 'status': st, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest(), 'ok': g, 'attempt': att}); ok += g; tot += len(b)
    n = len(rows); bad += n - ok
    out['hosts'][host] = {'verified': ok, 'of': n, 'bytes': tot, 'rows': rows}
    print(f'{host}: {ok}/{n} verified, {tot} bytes, mismatches {n-ok}; index checks {checks}')
    for r in rows:
        if not r['ok']: print('  MISMATCH', r['path'], r['status'], r['bytes'])
Path(sys.argv[1]).write_text(json.dumps(out, indent=1))
sys.exit(1 if bad else 0)
