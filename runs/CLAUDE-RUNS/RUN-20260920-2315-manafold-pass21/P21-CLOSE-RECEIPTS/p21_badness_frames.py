import csv, math, sys

# ⚠ PASS-21 CLOSE: THE CSV IS AN ARGUMENT NOW, AND THIS MATTERED IMMEDIATELY.
# The review's copy hard-coded a path inside one session's scratchpad. Re-running
# it in the closing packet SILENTLY READ THAT STALE FILE -- it happened to be the
# same session id, so it existed, so the probe printed 9,700 confident samples
# measured from someone else's tree with no hint that it had not read the CSV it
# was handed. That is the "commit the probe" law half-applied: the code was kept
# and the input it describes was not, which is worse than throwing it away,
# because the numbers still come out.
#
#   usage: p21_badness_frames.py <mrod --csv output>
if len(sys.argv) < 2:
    raise SystemExit('usage: p21_badness_frames.py <mrod --csv output>')
CSV = sys.argv[1]

# rods layout: base 0-3, rod0 4-12, ballA 13-19, rod1 20-24, ballB 25-31,
# rod2 32-36, ballC 37-43, rod3 44-52, ballEnd 53-59, tail 60-63
ROD = {0: (4, 12), 1: (20, 24), 2: (32, 36), 3: (44, 52)}
# slot -> subject, the WHOLE live bank rather than the eight the review had
# rendered. A worst frame nobody can attribute to a subject cannot be looked at.
# Read off zhao_reel.cpp's subject_u02_clip(...) calls, not remembered. Slots 7
# (still) and 16 (nodule-solo) are diagnostic subjects the live site does not
# carry, and slot 15 is unused -- named so a worst frame landing there is not
# mistaken for a live clip nobody looked at.
RENDERED = {
    0: 'hover + inspect', 1: 'drift', 2: 'channel', 3: 'curious',
    4: 'startle', 5: 'rest', 6: 'pirouette', 7: '(still, not live)',
    8: 'hasty', 9: 'fall', 10: 'hit', 11: 'taunt', 12: 'taunt2',
    13: 'trick', 14: 'damage', 16: '(nodule-solo, not live)',
    17: 'death-drop', 18: 'death-gutter', 19: 'lasso', 20: 'blown',
    21: 'taunt3', 22: 'flight', 23: 'crackle',
}


def cen(row, i):
    return (float(row[f'cx{i}']), float(row[f'cy{i}']), float(row[f'cz{i}']))


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def ang(u, v):
    du = math.sqrt(sum(x * x for x in u))
    dv = math.sqrt(sum(x * x for x in v))
    if du == 0 or dv == 0:
        return 0.0
    c = sum(a * b for a, b in zip(u, v)) / (du * dv)
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


rows = list(csv.DictReader(open(CSV)))
print(f'{len(rows)} samples')
best = {}
for r in rows:
    slot = int(r['slot'])
    d = {k: sub(cen(r, hi), cen(r, lo)) for k, (lo, hi) in ROD.items()}
    joints = {'A': ang(d[0], d[1]), 'B': ang(d[1], d[2]), 'C': ang(d[2], d[3])}
    for j, a in joints.items():
        k = (j, slot)
        if a > best.get(k, (-1,))[0]:
            best[k] = (a, int(r['frame']), int(r['sub']))

for j in 'ABC':
    print(f'\n--- joint {j}: worst angle per slot (rendered slots marked) ---')
    rank = sorted(((v[0], s, v[1], v[2]) for (jj, s), v in best.items()
                   if jj == j), reverse=True)
    for a, s, f, sb in rank[:8]:
        tag = RENDERED.get(s, '')
        pres = f * 2 + sb
        print(f'  {a:6.1f} deg  slot {s:2d} key {f:3d}.{sb}  presframe {pres:4d}'
              f'  {"<< " + tag if tag else ""}')
