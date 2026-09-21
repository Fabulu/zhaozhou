"""Cache-bypassed production verification for Manafold pass 23.

usage: p23_production_verify.py --selftest
       p23_production_verify.py <out.json> <host> [host ...]
Compares index.html (byte-for-byte against the local deployed public/index.html,
plus content checks), the 44 live media (against P23-LIVE-MEDIA-SHA256.txt),
and EIGHTEEN archive spot checks -- three each from version-17, version-18,
pass-19, pass-20, pass-21 and now pass-22, one set per locked generation, so a generation cannot
be locked in the repo and silently absent from the host.
Exit 1 on any mismatch. Derived from p23_production_verify.py; --selftest proves
each check can fire on a deliberately broken copy BEFORE its silence is quoted."""
import hashlib, json, re, sys, time, urllib.request, uuid
from pathlib import Path

UP = Path(r'C:\programmieren\zencrifice\manafold-p16\Upheaval')
site = UP / 'website'
CM = UP / 'creature/Manafold'
live = [l.split() for l in (CM / 'P23-LIVE-MEDIA-SHA256.txt').read_text().splitlines() if l.strip() and not l.startswith('#')]


def archive_spots(receipt, names):
    rows = [l.split('\t') for l in (CM / receipt).read_text(encoding='utf-8').splitlines() if l.strip() and not l.startswith('#')]
    return [(d, int(n), a) for d, n, _, a in rows if a.endswith(names)]


spot = (archive_spots('V17-ARCHIVE-SHA256.txt', ('archive-v17-manafold-trick.webm', 'archive-v17-manafold-hover.png', 'archive-v17-manafold-mana-boil.webm'))
        + archive_spots('V18-ARCHIVE-SHA256.txt', ('archive-v18-manafold-trick.webm', 'archive-v18-manafold-hover.png', 'archive-v18-manafold-inspect.webm'))
        + archive_spots('P19-ARCHIVE-SHA256.txt', ('archive-p19-manafold-trick.webm', 'archive-p19-manafold-hover.png', 'archive-p19-manafold-inspect.webm'))
        + archive_spots('P20-ARCHIVE-SHA256.txt', ('archive-p20-manafold-trick.webm', 'archive-p20-manafold-hover.png', 'archive-p20-manafold-inspect.webm'))
        + archive_spots('P21-ARCHIVE-SHA256.txt', ('archive-p21-manafold-trick.webm', 'archive-p21-manafold-hover.png', 'archive-p21-manafold-inspect.webm'))
        + archive_spots('P22-ARCHIVE-SHA256.txt', ('archive-p22-manafold-trick.webm', 'archive-p22-manafold-hover.png', 'archive-p22-manafold-inspect.webm')))
assert len(live) == 44 and len(spot) == 18
idx = (site / 'public/index.html').read_bytes()
H = {'Cache-Control': 'no-cache, no-store, max-age=0', 'Pragma': 'no-cache', 'User-Agent': 'p23-verify'}


def get(url):
    err = None
    for attempt in range(1, 6):
        try:
            with urllib.request.urlopen(urllib.request.Request(f'{url}?p23verify={uuid.uuid4().hex}', headers=H), timeout=120) as r:
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
        'card_pass_23': 'MANAFOLD, pass 23' in t and 'MANAFOLD, pass 22 ' not in t,
        'scope_four_moved': 'EXACTLY FOUR subjects differ' in t and 'EIGHTEEN are byte-identical' in t,
        'renderer_md5': '3e9104bae2777a3160da9bbe37bd0b9c' in t,
        'manifest_sha256': '2f3cfb8175f4e4cad80ced5351591ca1fdaa6fc5ee3fa768f4449059ecdffb18' in t,
        'source_d6532cc1': 'zhaozhou d6532cc1' in t,
        '22_subjects_7992_frames': ('22 live subjects — 7,992 frames' in t) or ('22 live subjects &mdash; 7,992 frames' in t),
        'all_44_live_declared': all(p in t for _, _, p in live),
        'v18_archive_22_declared': len(set(re.findall(r'renders/archive-v18-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p19_archive_22_declared': len(set(re.findall(r'renders/archive-p19-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p20_archive_22_declared': len(set(re.findall(r'renders/archive-p20-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p21_archive_22_declared': len(set(re.findall(r'renders/archive-p21-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p22_archive_22_declared': len(set(re.findall(r'renders/archive-p22-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'archive_18_generations': 'Archive (18 generations)' in t,
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
        'renderer md5 altered': (t.replace('3e9104bae2777a3160da9bbe37bd0b9c', '3e9104bae2777a3160da9bbe37bd0b9d'), 'renderer_md5'),
        'old p22 card': (t.replace('MANAFOLD, pass 23', 'MANAFOLD, pass 22 '), 'card_pass_23'),
        # The scope sentence IS the claim of this pass -- four moved, eighteen
        # byte-identical. If the card ever loses it the page is still pretty and
        # the evidence is gone, so it gets a negative of its own.
        'scope sentence dropped': (t.replace('EXACTLY FOUR subjects differ', 'some subjects differ'), 'scope_four_moved'),
        # The first version of this negative RENAMED the clip to
        # archive-p19-manafold-zzz.webm, which still matches the pattern and
        # still counts 22 -- the selftest caught its own broken fixture, which
        # is the whole reason it runs before any host is queried. It now DROPS
        # the declaration, which is the fault it is meant to describe.
        'a pass-19 archive clip dropped': (t.replace('renders/archive-p19-manafold-trick.webm', 'renders/manafold-trick.webm'), 'p19_archive_22_declared'),
        # The pass-20 generation is the NEW lock this pass adds, so its negative
        # is the one that has never been run before. Same shape as pass 19's:
        # DROP the declaration rather than rename it, because a rename still
        # matches the pattern and still counts 22 -- the fixture fault the
        # pass-20 selftest caught in itself.
        'a pass-20 archive clip dropped': (t.replace('renders/archive-p20-manafold-trick.webm', 'renders/manafold-trick.webm'), 'p20_archive_22_declared'),
        # The pass-21 generation is the NEW lock this pass adds, so its negative
        # is the one that has never been run before. DROP the declaration, never
        # rename it -- a rename still matches the pattern and still counts 22.
        'a pass-21 archive clip dropped': (t.replace('renders/archive-p21-manafold-trick.webm', 'renders/manafold-trick.webm'), 'p21_archive_22_declared'),
        # The pass-22 generation is the NEW lock this pass adds, so its negative has
        # never been run before. DROP the declaration, never rename it -- a rename
        # still matches the pattern and still counts 22.
        'a pass-22 archive clip dropped': (t.replace('renders/archive-p22-manafold-trick.webm', 'renders/manafold-trick.webm'), 'p22_archive_22_declared'),
        'generation count stale': (t.replace('Archive (18 generations)', 'Archive (17 generations)'), 'archive_18_generations'),
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
    # PASS 23: this line used to NAME four negatives while the dict ran nine,
    # so adding one left the message describing a smaller test than the one
    # that ran. It now reports the count it actually executed and lists them,
    # because a summary nobody recomputes is the same stale number in prose.
    print('SELFTEST', 'FAIL' if fails else
          f'OK: local page passes; {len(negatives)} index negatives fire '
          f'({", ".join(sorted(negatives))}); corrupted, truncated and non-200 media fire')
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
