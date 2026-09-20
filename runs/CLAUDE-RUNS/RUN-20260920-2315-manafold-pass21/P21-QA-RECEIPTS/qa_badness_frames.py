import csv, math, sys

CSV = r'C:\Users\Fabs\AppData\Local\Temp\claude\C--programmieren-zencrifice\7f6527d5-4471-4224-ac24-c3cd3eb9b5cf\scratchpad\p21qa\receipts\mrod.csv'

# rods layout: base 0-3, rod0 4-12, ballA 13-19, rod1 20-24, ballB 25-31,
# rod2 32-36, ballC 37-43, rod3 44-52, ballEnd 53-59, tail 60-63
ROD = {0: (4, 12), 1: (20, 24), 2: (32, 36), 3: (44, 52)}
RENDERED = {0: 'inspect/hover', 1: 'drift', 2: 'channel', 5: 'rest',
            13: 'trick', 17: 'death-drop', 20: 'blown', 21: 'taunt3'}


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
