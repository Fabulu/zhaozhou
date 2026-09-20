"""Cache-bypassed production verification for Manafold pass 20.

usage: p20_production_verify.py --selftest
       p20_production_verify.py <out.json> <host> [host ...]
Compares index.html (byte-for-byte against the local deployed public/index.html,
plus content checks), the 44 live media (against P20-LIVE-MEDIA-SHA256.txt),
and NINE archive spot checks: three version-17 (V17-ARCHIVE-SHA256.txt), three
version-18 (V18-ARCHIVE-SHA256.txt) and three pass-19 (P19-ARCHIVE-SHA256.txt).
Exit 1 on any mismatch. Derived from p19_production_verify.py; --selftest proves each check can fire
on a deliberately broken copy before its silence is quoted."""
import hashlib, json, re, sys, time, urllib.request, uuid
from pathlib import Path

UP = Path(r'C:\programmieren\zencrifice\manafold-p16\Upheaval')
site = UP / 'website'
CM = UP / 'creature/Manafold'
live = [l.split() for l in (CM / 'P20-LIVE-MEDIA-SHA256.txt').read_text().splitlines() if l.strip() and not l.startswith('#')]


def archive_spots(receipt, names):
    rows = [l.split('\t') for l in (CM / receipt).read_text(encoding='utf-8').splitlines() if l.strip() and not l.startswith('#')]
    return [(d, int(n), a) for d, n, _, a in rows if a.endswith(names)]


spot = (archive_spots('V17-ARCHIVE-SHA256.txt', ('archive-v17-manafold-trick.webm', 'archive-v17-manafold-hover.png', 'archive-v17-manafold-mana-boil.webm'))
        + archive_spots('V18-ARCHIVE-SHA256.txt', ('archive-v18-manafold-trick.webm', 'archive-v18-manafold-hover.png', 'archive-v18-manafold-inspect.webm'))
        + archive_spots('P19-ARCHIVE-SHA256.txt', ('archive-p19-manafold-trick.webm', 'archive-p19-manafold-hover.png', 'archive-p19-manafold-inspect.webm')))
assert len(live) == 44 and len(spot) == 9
idx = (site / 'public/index.html').read_bytes()
H = {'Cache-Control': 'no-cache, no-store, max-age=0', 'Pragma': 'no-cache', 'User-Agent': 'p20-verify'}


def get(url):
    err = None
    for attempt in range(1, 6):
        try:
            with urllib.request.urlopen(urllib.request.Request(f'{url}?p20verify={uuid.uuid4().hex}', headers=H), timeout=120) as r:
                return r.status, r.read(), attempt
        except Exception as e:  # noqa: BLE001
            err = e
            time.sleep(5 * attempt)
    return 0, repr(err).encode(), 5


def index_checks(b):
    t = b.decode('utf-8')
    nc = re.sub(r'(?s)<!--.*?-->', '', t)
    robots = re.findall(r'(?is)<meta\s+[^>]*name\s*=\s*"robots"[^>]*>', nc)
    vids = {}
    for m in re.finditer(r'(?is)<video\b.*?</video>', t):
        s = re.search(r'src="(renders/[^"]+\.webm)"', m.group(0))
        if s:
            vids[s.group(1)] = m.group(0)
    opening = lambda v: v.split('>')[0]
    fall, hover = vids.get('renders/manafold-fall.webm', ''), vids.get('renders/manafold-hover.webm', '')
    return {
        'robots_exactly_one_noindex_nofollow': len(robots) == 1 and 'content="noindex, nofollow"' in robots[0],
        'card_pass_20': 'MANAFOLD, pass 20' in t and 'MANAFOLD, pass 19 ' not in t,
        'renderer_md5': 'e95faca916627d1bddb02892c5eb67e1' in t,
        'manifest_sha256': 'a40b41549383246d7c9580c768c936f8919eb810dce7c0ecae24e3cdb1313b15' in t,
        'source_55767880': 'zhaozhou 55767880' in t,
        '22_subjects_7992_frames': ('22 live subjects — 7,992 frames' in t) or ('22 live subjects &mdash; 7,992 frames' in t),
        'all_44_live_declared': all(p in t for _, _, p in live),
        'v18_archive_22_declared': len(set(re.findall(r'renders/archive-v18-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p19_archive_22_declared': len(set(re.findall(r'renders/archive-p19-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'archive_15_generations': 'Archive (15 generations)' in t,
        'fall_no_autoplay_no_loop': bool(fall) and not re.search(r'\b(autoplay|loop)\b', opening(fall)),
        'hover_autoplay_loop': bool(hover) and re.search(r'\bautoplay\b', opening(hover)) is not None and re.search(r'\bloop\b', opening(hover)) is not None,
    }


def media_ok(st, b, n, d):
    return st == 200 and len(b) == n and hashlib.sha256(b).hexdigest() == d


def selftest():
    fails = []
    good = index_checks(idx)
    if not all(good.values()):
        fails.append(f'local index fails its own checks: {good}')
    t = idx.decode('utf-8')
    negatives = {
        'hover loop stripped': (re.sub(r'(<video src="renders/manafold-hover\.webm"[^>]*?)\sloop\b', r'\1', t), 'hover_autoplay_loop'),
        'robots index,follow': (t.replace('content="noindex, nofollow"', 'content="index, follow"'), 'robots_exactly_one_noindex_nofollow'),
        'renderer md5 altered': (t.replace('e95faca916627d1bddb02892c5eb67e1', 'e95faca916627d1bddb02892c5eb67e2'), 'renderer_md5'),
        'old p19 card': (t.replace('MANAFOLD, pass 20', 'MANAFOLD, pass 19 '), 'card_pass_20'),
        # The first version of this negative RENAMED the clip to
        # archive-p19-manafold-zzz.webm, which still matches the pattern and
        # still counts 22 -- the selftest caught its own broken fixture, which
        # is the whole reason it runs before any host is queried. It now DROPS
        # the declaration, which is the fault it is meant to describe.
        'a pass-19 archive clip dropped': (t.replace('renders/archive-p19-manafold-trick.webm', 'renders/manafold-trick.webm'), 'p19_archive_22_declared'),
        'generation count stale': (t.replace('Archive (15 generations)', 'Archive (14 generations)'), 'archive_15_generations'),
    }
    for name, (text, key) in negatives.items():
        if text == t:
            fails.append(f'negative "{name}" did not change the page (fixture broken)')
        elif index_checks(text.encode('utf-8'))[key]:
            fails.append(f'negative "{name}" did not fire {key}')
    d, n, p = live[0]
    raw = (site / 'public' / p).read_bytes()
    broken = bytearray(raw)
    broken[len(broken) // 2] ^= 0xFF
    if not media_ok(200, raw, int(n), d):
        fails.append(f'local {p} does not match its receipt')
    if media_ok(200, bytes(broken), int(n), d):
        fails.append('a one-byte-corrupted media copy passed')
    if media_ok(200, raw[:-1], int(n), d):
        fails.append('a truncated media copy passed')
    if media_ok(404, raw, int(n), d):
        fails.append('a non-200 response passed')
    for f in fails:
        print('SELFTEST FAIL', f)
    print('SELFTEST', 'FAIL' if fails else 'OK: local page passes; hover-loop, robots, md5 and old-card negatives fire; corrupted, truncated and non-200 media fire')
    return 1 if fails else 0


def main():
    if sys.argv[1] == '--selftest':
        return selftest()
    out = {'hosts': {}}
    bad = 0
    for host in sys.argv[2:]:
        rows, tot, ok = [], 0, 0
        st, b, att = get(f'{host}/')
        checks = index_checks(b) if st == 200 else {}
        good = st == 200 and b == idx and all(checks.values())
        rows.append({'path': 'index.html', 'status': st, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest(), 'ok': good, 'attempt': att, 'checks': checks})
        ok += good
        tot += len(b)
        for d, n, p in [(d, int(n), p) for d, n, p in live] + spot:
            st, b, att = get(f'{host}/{p}')
            g = media_ok(st, b, n, d)
            rows.append({'path': p, 'status': st, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest(), 'ok': g, 'attempt': att})
            ok += g
            tot += len(b)
        n = len(rows)
        bad += n - ok
        retries = sum(r['attempt'] - 1 for r in rows)
        out['hosts'][host] = {'verified': ok, 'of': n, 'bytes': tot, 'retries': retries, 'rows': rows}
        print(f'{host}: {ok}/{n} verified, {tot} bytes, mismatches {n - ok}, retries {retries}; index checks {checks}')
        for r in rows:
            if not r['ok']:
                print('  MISMATCH', r['path'], r['status'], r['bytes'])
    Path(sys.argv[1]).write_text(json.dumps(out, indent=1))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
