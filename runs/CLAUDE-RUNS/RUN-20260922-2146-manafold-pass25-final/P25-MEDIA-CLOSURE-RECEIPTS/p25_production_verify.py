"""Cache-bypassed production verification for Manafold pass 25 -- the creature's FINAL pass.

usage: p25_production_verify.py --selftest
       p25_production_verify.py <out.json> <host> [host ...]

Compares index.html (byte-for-byte against the local deployed public/index.html,
plus content checks), the 44 live media (against P25-LIVE-MEDIA-SHA256.txt), and
TWENTY-FOUR archive spot checks -- three each from version-17, version-18,
pass-19, pass-20, pass-21, pass-22, pass-23 and now pass-24, ONE SET PER LOCKED
GENERATION, so a generation cannot be locked in the repo and silently absent
from the host.

Exit 1 on any mismatch. Derived from pass 24's; `--selftest` proves each check
can fire on a deliberately broken copy BEFORE its silence is quoted.

! THE CHECK STRINGS AVOID APOSTROPHES AND EM DASHES wherever a plain phrase will
do. Both survive the assembler today, but a negative that stops firing because
an entity encoding changed is a negative that goes quiet without anybody
noticing -- and this file's whole job is to be the thing that does not go quiet.
The two places an em dash is unavoidable accept either form.
"""
import hashlib, json, re, sys, time, urllib.request, uuid
from pathlib import Path

UP = Path(r'C:\programmieren\zencrifice\manafold-p16\Upheaval')
site = UP / 'website'
CM = UP / 'creature/Manafold'
live = [l.split() for l in (CM / 'P25-LIVE-MEDIA-SHA256.txt').read_text().splitlines()
        if l.strip() and not l.startswith('#')]

RENDERER_MD5 = 'a0c0c80a2e9b02dc852c25c1ff1aa530'
MANIFEST_SHA = '23cd615d25ceb27f29231c769f3e1a507e2d9cc2dcc74612da247b99fc3cef16'
SOURCE = 'b3c5760e'


def archive_spots(receipt, names):
    rows = [l.split('\t') for l in (CM / receipt).read_text(encoding='utf-8').splitlines()
            if l.strip() and not l.startswith('#')]
    return [(d, int(n), a) for d, n, _, a in rows if a.endswith(names)]


def trio(tag):
    return (f'archive-{tag}-manafold-trick.webm',
            f'archive-{tag}-manafold-hover.png',
            f'archive-{tag}-manafold-inspect.webm')


spot = (archive_spots('V17-ARCHIVE-SHA256.txt',
                      ('archive-v17-manafold-trick.webm', 'archive-v17-manafold-hover.png',
                       'archive-v17-manafold-mana-boil.webm'))
        + archive_spots('V18-ARCHIVE-SHA256.txt', trio('v18'))
        + archive_spots('P19-ARCHIVE-SHA256.txt', trio('p19'))
        + archive_spots('P20-ARCHIVE-SHA256.txt', trio('p20'))
        + archive_spots('P21-ARCHIVE-SHA256.txt', trio('p21'))
        + archive_spots('P22-ARCHIVE-SHA256.txt', trio('p22'))
        + archive_spots('P23-ARCHIVE-SHA256.txt', trio('p23'))
        + archive_spots('P24-ARCHIVE-SHA256.txt', trio('p24')))
assert len(live) == 44, f'live receipt has {len(live)} rows, expected 44'
assert len(spot) == 24, f'{len(spot)} archive spots, expected 24 (3 x 8 generations)'
idx = (site / 'public/index.html').read_bytes()
H = {'Cache-Control': 'no-cache, no-store, max-age=0', 'Pragma': 'no-cache', 'User-Agent': 'p25-verify'}


def get(url):
    err = None
    for attempt in range(1, 6):
        try:
            with urllib.request.urlopen(
                    urllib.request.Request(f'{url}?p25verify={uuid.uuid4().hex}', headers=H),
                    timeout=120) as r:
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
    def opening(v):
        return v.split('>')[0]
    fall, hover = vids.get('renders/manafold-fall.webm', ''), vids.get('renders/manafold-hover.webm', '')
    return {
        'robots_exactly_one_noindex_nofollow':
            len(robots) == 1 and 'content="noindex, nofollow"' in robots[0],
        'card_pass_25': 'MANAFOLD, pass 25' in t and 'MANAFOLD, pass 24 ' not in t,
        # THE OWNER CLOSED THE CREATURE WITH THIS PASS. If the page loses that
        # sentence it reads like any other pass and the one fact that changes
        # what he does next is gone.
        'final_pass_stated': 'THIS IS THE FINAL PASS ON THIS CREATURE' in t,
        # Item 1: the rollout, its size, and the number that located the complaint.
        'rollout_all_22': 'ALL 22 CLIPS' in t,
        'nearest_miss_stated': 'FOUR AND A HALF MILLIMETRES' in t and 'THIRTY-TWO millimetres' in t,
        # The non-monotone knob is the trap the next person would walk into.
        'non_monotone_stated': '56, 66, 70, 76 and 106 millimetres all PUT CROSSINGS BACK' in t,
        'fold_figure_edge': 'FOLDED FIGURE' in t and 'one segment in nine' in t,
        # Item 2.
        'eyes_600_to_800': '600 to 800' in t,
        # Item 3 -- the diagnosis, which is the whole content of the pass and the
        # reason four earlier knobs failed. Three separate negatives, because a
        # card that keeps the fix and loses the reason is a card that invites the
        # same four passes again.
        'backball_calmest': 'THE BALL AT THE VERY BACK BARELY MOVES AT ALL' in t,
        'carrier_c_named': 'one station further forward and the rod running back from it' in t,
        'cancellation_stated':
            'NO SINGLE PART OF THE ANTENNA OWNS THAT MOVEMENT' in t and '146 PER CENT MORE' in t,
        'filter_not_knob': 'IT IS A FILTER' in t,
        # The two things the owner must not be surprised by.
        'front_spin_declared': 'OWN SPIN IS SLOWER' in t,
        'crackle_offered': 'CRACKLE STILL HAS THE REAR FAULT' in t,
        # Provenance.
        'renderer_md5': RENDERER_MD5 in t,
        'manifest_sha256': MANIFEST_SHA in t,
        'source_b3c5760e': f'zhaozhou {SOURCE}' in t,
        '22_subjects_7992_frames':
            ('22 live subjects — 7,992 frames' in t) or ('22 live subjects &mdash; 7,992 frames' in t),
        'reviewer_rebuilt': 'BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER' in t,
        'scope_two_subjects': 'Exactly TWO subjects changed from the smoothing' in t,
        'all_44_live_declared': all(p in t for _, _, p in live),
        'v18_archive_22_declared': len(set(re.findall(r'renders/archive-v18-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p19_archive_22_declared': len(set(re.findall(r'renders/archive-p19-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p20_archive_22_declared': len(set(re.findall(r'renders/archive-p20-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p21_archive_22_declared': len(set(re.findall(r'renders/archive-p21-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p22_archive_22_declared': len(set(re.findall(r'renders/archive-p22-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p23_archive_22_declared': len(set(re.findall(r'renders/archive-p23-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p24_archive_22_declared': len(set(re.findall(r'renders/archive-p24-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'archive_20_generations': 'Archive (20 generations)' in t,
        'fall_no_autoplay_no_loop': bool(fall) and not re.search(r'\b(autoplay|loop)\b', opening(fall)),
        'hover_autoplay_loop': bool(hover) and re.search(r'\bautoplay\b', opening(hover)) is not None
            and re.search(r'\bloop\b', opening(hover)) is not None,
    }


def media_ok(st, b, n, d):
    return st == 200 and len(b) == n and hashlib.sha256(b).hexdigest() == d


def selftest():
    fails = []
    good = index_checks(idx)
    if not all(good.values()):
        fails.append('local index fails its own checks: '
                     + str({k: v for k, v in good.items() if not v}))
    t = idx.decode('utf-8')
    negatives = {
        'hover loop stripped':
            (re.sub(r'(<video src="renders/manafold-hover\.webm"[^>]*?)\sloop\b', r'\1', t),
             'hover_autoplay_loop'),
        'robots index,follow':
            (t.replace('content="noindex, nofollow"', 'content="index, follow"'),
             'robots_exactly_one_noindex_nofollow'),
        'renderer md5 altered':
            (t.replace(RENDERER_MD5, RENDERER_MD5[:-1] + ('0' if RENDERER_MD5[-1] != '0' else '1')),
             'renderer_md5'),
        'manifest sha altered':
            (t.replace(MANIFEST_SHA, MANIFEST_SHA[:-1] + ('0' if MANIFEST_SHA[-1] != '0' else '1')),
             'manifest_sha256'),
        'old p24 card': (t.replace('MANAFOLD, pass 25', 'MANAFOLD, pass 24 '), 'card_pass_25'),
        # THE FINAL-PASS SENTENCE. It is the one fact on this page that changes
        # what the owner does next, and a page that loses it still looks finished.
        'final-pass sentence dropped':
            (t.replace('THIS IS THE FINAL PASS ON THIS CREATURE', 'This is a pass on this creature'),
             'final_pass_stated'),
        # The rollout's size is the pass's headline claim.
        'rollout size dropped': (t.replace('ALL 22 CLIPS', 'several clips'), 'rollout_all_22'),
        # The nearest-miss pair is the number that LOCATED the owner's complaint
        # after a gate reading zero had failed to. Losing it loses the argument
        # for why the clearance moved at all.
        'nearest-miss numbers dropped':
            (t.replace('FOUR AND A HALF MILLIMETRES', 'a few millimetres'), 'nearest_miss_stated'),
        # The non-monotone warning: without it the next person turns the knob up.
        'non-monotone warning dropped':
            (t.replace('56, 66, 70, 76 and 106 millimetres all PUT CROSSINGS BACK',
                       'some values are worse'), 'non_monotone_stated'),
        'eye rung dropped': (t.replace('600 to 800', 'a little higher'), 'eyes_600_to_800'),
        # THE THREE DIAGNOSIS NEGATIVES. The fix is one line of code; the reason
        # is four passes of work, and it is the part that stops the next person
        # reaching for a gain again.
        'back-ball finding dropped':
            (t.replace('THE BALL AT THE VERY BACK BARELY MOVES AT ALL',
                       'the back ball moves a bit'), 'backball_calmest'),
        'carrier C identification dropped':
            (t.replace('one station further forward and the rod running back from it',
                       'the back of the antenna'), 'carrier_c_named'),
        'cancellation finding dropped':
            (t.replace('NO SINGLE PART OF THE ANTENNA OWNS THAT MOVEMENT',
                       'the antenna moves'), 'cancellation_stated'),
        'filter/knob distinction dropped':
            (t.replace('IT IS A FILTER', 'it is a setting'), 'filter_not_knob'),
        # THE TWO THINGS THE OWNER MUST NOT BE SURPRISED BY. If either is quietly
        # dropped the page is reassuring and incomplete, which is worse than
        # either fault.
        'front-spin disclosure dropped':
            (t.replace('OWN SPIN IS SLOWER', 'own spin is the same'), 'front_spin_declared'),
        'crackle offer dropped':
            (t.replace('CRACKLE STILL HAS THE REAR FAULT', 'Crackle is fine'), 'crackle_offered'),
        'scope sentence dropped':
            (t.replace('Exactly TWO subjects changed from the smoothing',
                       'some subjects changed'), 'scope_two_subjects'),
        'reviewer provenance dropped':
            (t.replace('BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER', 'built'),
             'reviewer_rebuilt'),
        # The pass-24 generation is the NEW lock this pass adds, so its negative
        # has never been run before. DROP the declaration, never rename it -- a
        # rename still matches the pattern and still counts 22, which is the
        # fixture fault the pass-20 selftest caught in itself.
        'a pass-24 archive clip dropped':
            (t.replace('renders/archive-p24-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p24_archive_22_declared'),
        'a pass-23 archive clip dropped':
            (t.replace('renders/archive-p23-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p23_archive_22_declared'),
        'a pass-19 archive clip dropped':
            (t.replace('renders/archive-p19-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p19_archive_22_declared'),
        'generation count stale':
            (t.replace('Archive (20 generations)', 'Archive (19 generations)'),
             'archive_20_generations'),
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
        rows.append({'path': 'index.html', 'status': st, 'bytes': len(b),
                     'sha256': hashlib.sha256(b).hexdigest(), 'ok': good,
                     'attempt': att, 'checks': checks})
        ok += good
        tot += len(b)
        for d, n, p in [(d, int(n), p) for d, n, p in live] + spot:
            st, b, att = get(f'{host}/{p}')
            g = media_ok(st, b, n, d)
            rows.append({'path': p, 'status': st, 'bytes': len(b),
                         'sha256': hashlib.sha256(b).hexdigest(), 'ok': g, 'attempt': att})
            ok += g
            tot += len(b)
        n = len(rows)
        bad += n - ok
        retries = sum(r['attempt'] - 1 for r in rows)
        out['hosts'][host] = {'verified': ok, 'of': n, 'bytes': tot, 'retries': retries, 'rows': rows}
        print(f'{host}: {ok}/{n} verified, {tot} bytes, mismatches {n - ok}, retries {retries}')
        failed = {k: v for k, v in checks.items() if not v}
        print(f'  index checks: {len(checks)} run, {len(failed)} failed'
              + (f' -> {failed}' if failed else ''))
        for r in rows:
            if not r['ok']:
                print('  MISMATCH', r['path'], r['status'], r['bytes'])
    Path(sys.argv[1]).write_text(json.dumps(out, indent=1))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
