"""Cache-bypassed production verification for Manafold pass 26 -- Hasty's hurry.

usage: p26_production_verify.py --selftest
       p26_production_verify.py <out.json> <host> [host ...]

Compares index.html (byte-for-byte against the local deployed public/index.html,
plus content checks), the 44 live media (against P26-LIVE-MEDIA-SHA256.txt), and
TWENTY-SEVEN archive spot checks -- three each from version-17, version-18,
pass-19, pass-20, pass-21, pass-22, pass-23, pass-24 and now pass-25, ONE SET
PER LOCKED GENERATION, so a generation cannot be locked in the repo and silently
absent from the host.

Exit 1 on any mismatch. Derived from pass 25's; `--selftest` proves each check
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
live = [l.split() for l in (CM / 'P26-LIVE-MEDIA-SHA256.txt').read_text().splitlines()
        if l.strip() and not l.startswith('#')]

RENDERER_MD5 = 'b822bb9bd7ceb8f0b921623566b26169'
MANIFEST_SHA = 'eb6d4e9a8191d8e958711eb09d955842bec5782ef53c2b236d206b81126c5e13'
SOURCE = 'bfb22a65'


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
        + archive_spots('P24-ARCHIVE-SHA256.txt', trio('p24'))
        + archive_spots('P25-ARCHIVE-SHA256.txt', trio('p25')))
assert len(live) == 44, f'live receipt has {len(live)} rows, expected 44'
assert len(spot) == 27, f'{len(spot)} archive spots, expected 27 (3 x 9 generations)'
idx = (site / 'public/index.html').read_bytes()
H = {'Cache-Control': 'no-cache, no-store, max-age=0', 'Pragma': 'no-cache', 'User-Agent': 'p26-verify'}


def get(url):
    err = None
    for attempt in range(1, 6):
        try:
            with urllib.request.urlopen(
                    urllib.request.Request(f'{url}?p26verify={uuid.uuid4().hex}', headers=H),
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
        'card_pass_26': 'MANAFOLD, pass 26' in t and 'MANAFOLD, pass 25 ' not in t,
        # THE DIAGNOSIS IS THE WHOLE PASS. A card that keeps the fix and loses
        # the reason invites the next person to re-derive it from nothing.
        'traverse_deleted_stated': 'travel had been DELETED' in t,
        'camera_never_told': 'the camera was never told' in t,
        'pan_is_not_speed': 'AND THE GROUND UNDER IT' in t,
        'speed_cue_numbers':
            'SIX HUNDREDTHS OF A PIXEL A FRAME' in t and '3.05 pixels a frame' in t,
        # The 45-degree finding, which is also Drift's open bug.
        'lens_axis_stated': 'straight INTO THE LENS' in t,
        # Speed, by the numbers the owner would act on.
        'cadence_5_to_13': '5 beats in the clip to 13' in t,
        'surge_not_posture': 'the lean is no longer a POSE' in t,
        'margins_stated': '45 pixels of margin on the left and 93 on the right' in t,
        # The face, and specifically the thing that distinguishes it from alarm.
        'brow_tops_together': 'brow tops drawn TOGETHER' in t,
        'not_startle': any(
            ("Startle%ss payoff is the tops flying APART" % a) in t
            for a in ("'", "&#x27;", "&#39;", "’")),
        'three_checks': 'Three times in the clip it CHECKS' in t,
        'camera_enabled_face': 'ONE OR TWO PIXELS a frame' in t,
        # THE SEAM, AND THE CORRECTION. The review threw out a claim that was
        # about to ship. If the page quietly loses the correction it becomes the
        # reassuring version again, which is the fault, not the fix.
        'seam_moved_and_shrank': '163 pixels down to 111' in t,
        'seam_relative_figure': '238 TIMES anything else on screen' in t,
        'seam_correction_owned': 'The review threw that out' in t,
        'seam_mask_fault_stated': '82 per cent of what it scored with the creature in it' in t,
        # THE PAGE MUST NOT CLAIM THE SEAM CLOSED. The only place the phrase
        # "effectively vanished" may appear is inside the sentence that DISOWNS
        # it. Strip that quoted occurrence and any remaining one is the page
        # making the claim for itself -- which is the exact regression this
        # whole check exists to prevent.
        'no_standalone_vanished_claim':
            'effectively vanished' not in t.replace('&quot;effectively vanished&quot;', ''),
        # Containment, in the owner's terms: the two things he closed.
        'closed_items_identical': any(
            ("Crackle%ss rear is fine, Hover%ss front ball is fine" % (a, a)) in t
            for a in ("'", "&#x27;", "&#39;", "’")),
        'scope_240_all_hasty': 'exactly 240 of them differ' in t and 'All 240 are Hasty' in t,
        # The red gate that shipped. The most valuable thing found this pass.
        'stale_gate_disclosed': 'had been RED since before pass 25 shipped' in t,
        'pass25_not_in_doubt': any(
            ("PASS 25%sS ACTUAL PUBLISHED CLIPS WERE NEVER IN DOUBT" % a) in t
            for a in ("'", "&#x27;", "&#39;", "’")),
        # Provenance.
        'renderer_md5': RENDERER_MD5 in t,
        'manifest_sha256': MANIFEST_SHA in t,
        'source_bfb22a65': f'zhaozhou {SOURCE}' in t,
        '22_subjects_7992_frames':
            ('22 live subjects — 7,992 frames' in t) or ('22 live subjects &mdash; 7,992 frames' in t),
        'reviewer_rebuilt': 'BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER' in t,
        'no_bound_relaxed': 'No bound anywhere was relaxed' in t,
        'frame_review_not_overclaimed':
            'this card does not claim they were' in t,
        'all_44_live_declared': all(p in t for _, _, p in live),
        'v18_archive_22_declared': len(set(re.findall(r'renders/archive-v18-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p19_archive_22_declared': len(set(re.findall(r'renders/archive-p19-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p20_archive_22_declared': len(set(re.findall(r'renders/archive-p20-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p21_archive_22_declared': len(set(re.findall(r'renders/archive-p21-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p22_archive_22_declared': len(set(re.findall(r'renders/archive-p22-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p23_archive_22_declared': len(set(re.findall(r'renders/archive-p23-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p24_archive_22_declared': len(set(re.findall(r'renders/archive-p24-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'p25_archive_22_declared': len(set(re.findall(r'renders/archive-p25-manafold-[a-z0-9-]+\.webm', t))) == 22,
        'archive_21_generations': 'Archive (21 generations)' in t,
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
        'old p25 card': (t.replace('MANAFOLD, pass 26', 'MANAFOLD, pass 25 '), 'card_pass_26'),
        # THE DIAGNOSIS NEGATIVES. The fix is a handful of constants; the reason
        # is the whole pass, and it is what stops someone "fixing" Hasty's speed
        # again by turning the camera pan up.
        'the deletion dropped':
            (t.replace('travel had been DELETED', 'travel was adjusted'), 'traverse_deleted_stated'),
        'the un-removed compensation dropped':
            (t.replace('the camera was never told', 'the camera was updated'), 'camera_never_told'),
        'pan-is-not-speed dropped':
            (t.replace('AND THE GROUND UNDER IT', 'across the frame'), 'pan_is_not_speed'),
        'speed-cue numbers dropped':
            (t.replace('SIX HUNDREDTHS OF A PIXEL A FRAME', 'very small'), 'speed_cue_numbers'),
        'the 45-degree finding dropped':
            (t.replace('straight INTO THE LENS', 'sideways'), 'lens_axis_stated'),
        'cadence rung dropped': (t.replace('5 beats in the clip to 13', 'faster'), 'cadence_5_to_13'),
        'surge finding dropped':
            (t.replace('the lean is no longer a POSE', 'the lean changed'), 'surge_not_posture'),
        'margins dropped':
            (t.replace('45 pixels of margin on the left and 93 on the right', 'room to spare'),
             'margins_stated'),
        # THE FACE NEGATIVES. Tops-together against tops-apart is the whole
        # difference between hurry and alarm, and it is one word.
        'brow direction dropped':
            (t.replace('brow tops drawn TOGETHER', 'brow drawn'), 'brow_tops_together'),
        'the Startle contrast dropped':
            (t.replace('Startle&#x27;s payoff is the tops flying APART', 'Startle is different')
               .replace("Startle's payoff is the tops flying APART", 'Startle is different'),
             'not_startle'),
        'the three checks dropped':
            (t.replace('Three times in the clip it CHECKS', 'It looks around'), 'three_checks'),
        'why the camera moved dropped':
            (t.replace('ONE OR TWO PIXELS a frame', 'quite small'), 'camera_enabled_face'),
        # THE SEAM CORRECTION -- the most important negatives in this file. The
        # implementation was about to publish "the seam effectively vanished";
        # the review found the measurement was 82% background and that the jump
        # had merely MOVED. A page that quietly reverts to the reassuring
        # version is worse than one that never had the correction, because it
        # looks reviewed.
        'the corrected seam figures dropped':
            (t.replace('163 pixels down to 111', 'smaller'), 'seam_moved_and_shrank'),
        'the relative seam figure dropped':
            (t.replace('238 TIMES anything else on screen', 'a lot'), 'seam_relative_figure'),
        'the admission dropped':
            (t.replace('The review threw that out', 'The review agreed'), 'seam_correction_owned'),
        'the broken-mask finding dropped':
            (t.replace('82 per cent of what it scored with the creature in it', 'some background'),
             'seam_mask_fault_stated'),
        'the vanished claim put BACK':
            (t.replace('The honest numbers are the ones above',
                       'The seam effectively vanished'),
             'no_standalone_vanished_claim'),
        # CONTAINMENT, in the owner's own closed items.
        'closed-items sentence dropped':
            (t.replace('Crackle&#x27;s rear is fine, Hover&#x27;s front ball is fine',
                       'you closed some things')
               .replace("Crackle's rear is fine, Hover's front ball is fine",
                        'you closed some things'), 'closed_items_identical'),
        'scope numbers dropped':
            (t.replace('exactly 240 of them differ', 'a few differ'), 'scope_240_all_hasty'),
        # THE RED GATE. A pass that finds one and does not say so has wasted it.
        'the stale-gate disclosure dropped':
            (t.replace('had been RED since before pass 25 shipped', 'were checked'),
             'stale_gate_disclosed'),
        'the pass-25 reassurance dropped':
            (t.replace('PASS 25&#x27;S ACTUAL PUBLISHED CLIPS WERE NEVER IN DOUBT',
                       'pass 25 was probably fine')
               .replace("PASS 25'S ACTUAL PUBLISHED CLIPS WERE NEVER IN DOUBT",
                        'pass 25 was probably fine'), 'pass25_not_in_doubt'),
        'no-bound-relaxed dropped':
            (t.replace('No bound anywhere was relaxed', 'The bounds were adjusted'),
             'no_bound_relaxed'),
        'frame-review honesty dropped':
            (t.replace('this card does not claim they were',
                       'every frame of every clip was reviewed'),
             'frame_review_not_overclaimed'),
        'reviewer provenance dropped':
            (t.replace('BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER', 'built'),
             'reviewer_rebuilt'),
        # The pass-25 generation is the NEW lock this pass adds, so its negative
        # has never been run before. DROP the declaration, never rename it -- a
        # rename still matches the pattern and still counts 22, which is the
        # fixture fault the pass-20 selftest caught in itself.
        'a pass-25 archive clip dropped':
            (t.replace('renders/archive-p25-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p25_archive_22_declared'),
        'a pass-24 archive clip dropped':
            (t.replace('renders/archive-p24-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p24_archive_22_declared'),
        'a pass-19 archive clip dropped':
            (t.replace('renders/archive-p19-manafold-trick.webm', 'renders/manafold-trick.webm'),
             'p19_archive_22_declared'),
        'generation count stale':
            (t.replace('Archive (21 generations)', 'Archive (20 generations)'),
             'archive_21_generations'),
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
