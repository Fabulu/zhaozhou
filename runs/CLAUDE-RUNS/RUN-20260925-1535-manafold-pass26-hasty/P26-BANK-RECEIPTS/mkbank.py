import re, io, hashlib
LOG = '.tmp/p26-review/logs/ship-bank.out'
P25 = 'runs/CLAUDE-RUNS/RUN-20260922-2146-manafold-pass25-final/P25-BANK-RECEIPTS/bank-manifest.txt'
O = 'runs/CLAUDE-RUNS/RUN-20260925-1535-manafold-pass26-hasty/P26-BANK-RECEIPTS'

rx = re.compile(r'(manafold-[a-z0-9-]+): (\d+) frames, \d+ unique colours, sequence_crc32c=(0x[0-9A-F]+)')
rows = sorted(set(rx.findall(io.open(LOG, encoding='utf-8', errors='replace').read())))
assert len(rows) == 22, len(rows)
tot = sum(int(f) for _, f, _ in rows)

man = ['# Manafold pass 26 exact bank -- per-subject frame count and sequence CRC32',
       '# subjects=22 frames=%d' % tot] + ['%s\t%s\t%s' % r for r in rows]
io.open(O + '/bank-manifest.txt', 'w', encoding='utf-8', newline='\n').write('\n'.join(man) + '\n')
d = hashlib.sha256(io.open(O + '/bank-manifest.txt', 'rb').read()).hexdigest()
io.open(O + '/bank-manifest-sha256.txt', 'w', encoding='utf-8', newline='\n').write(
    '%s *%s/bank-manifest.txt\n' % (d, O))
print('manifest sha256:', d, ' subjects 22 frames', tot)

p25 = {}
for L in io.open(P25, encoding='utf-8'):
    if L.startswith('#') or not L.strip():
        continue
    n, f, c = L.split()
    p25[n] = (int(f), c)

out = ["# Manafold pass 26 SCOPE PROOF: the shipping bank against pass 25's.",
       '# Direction 27 reopened HASTY ALONE. Crackle\'s rear and Hover\'s front spin were',
       '# CLOSED by the owner, so exactly one subject may differ and 21 must not.',
       '# The frame COUNT of every subject must also be unchanged -- a changed count is a',
       '# retimed clip, which this pass is not allowed to do.',
       '# subjects p26=22 p25=%d' % len(p25), '',
       'subject\tframes_p25\tframes_p26\tcrc_p25\tcrc_p26\tframes\tpixels']
nd = 0
retimed = 0
unexpected = []
for n, f, c in rows:
    f25, c25 = p25[n]
    same = 'SAME' if int(f) == f25 else '*** CHANGED ***'
    if int(f) != f25:
        retimed += 1
    px = 'identical' if c == c25 else 'differ'
    if c != c25:
        nd += 1
        if n != 'manafold-hasty':
            unexpected.append(n)
    out.append('%s\t%d\t%s\t%s\t%s\t%s\t%s' % (n, f25, f, c25, c, same, px))

ok = nd == 1 and not unexpected and retimed == 0
out += ['',
        'SUBJECTS DIFFERING: %d  (expected 1: manafold-hasty)' % nd,
        'UNEXPECTED SUBJECTS MOVED: %s' % (', '.join(unexpected) if unexpected else 'none'),
        'FRAME COUNTS CHANGED: %d  (expected 0)' % retimed,
        "CRACKLE: %s  <- owner CLOSED" % ('identical' if dict((n, c) for n, _, c in rows)['manafold-crackle'] == p25['manafold-crackle'][1] else '*** MOVED ***'),
        "HOVER:   %s  <- owner CLOSED" % ('identical' if dict((n, c) for n, _, c in rows)['manafold-hover'] == p25['manafold-hover'][1] else '*** MOVED ***'),
        '',
        'VERDICT: %s' % ('IN SCOPE -- only manafold-hasty moved' if ok else '*** OUT OF SCOPE ***')]
io.open(O + '/scope-vs-pass25.txt', 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('\n'.join(out[-7:]))
