# comparison-side: sweep scratch-reel meta.txt sequence CRCs against the baseline table
import os, re, sys
scratch = sys.argv[1]
baseline = {}
for line in open('evidence/baseline-crc.txt'):
    m = re.match(r'(zixxtrixx-\S+)\s+(0x[0-9A-Fa-f]+)\s+frames=(\d+)', line)
    if m: baseline[m.group(1)] = (m.group(2), int(m.group(3)))
rows, changed = [], []
for sub in sorted(os.listdir(scratch)):
    meta = os.path.join(scratch, sub, 'meta.txt')
    if not os.path.exists(meta): continue
    txt = open(meta).read()
    crc = re.search(r'sequence_crc32c=(0x[0-9A-Fa-f]+)', txt).group(1)
    n = len([f for f in os.listdir(os.path.join(scratch, sub)) if re.fullmatch(r'\d{4}\.rgb', f)])
    b = baseline.get(sub, ('?', -1))
    same = (b[0].lower() == crc.lower()) and (b[1] == n)
    verdict = 'IDENTICAL' if same else 'CHANGED'
    if not same: changed.append(sub)
    print(f'{sub:26s} base {b[0]} f={b[1]:4d} | now {crc} f={n:4d}  {verdict}')
print('changed:', ', '.join(changed) if changed else 'none')
