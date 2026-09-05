import sys, numpy as np
sys.path.insert(0, r'C:\programmieren\zencrifice\manafold-p6-architect\zhaozhou\tools\reel')
import rgbframe as rf
R = r'C:\programmieren\zencrifice\manafold-p6-architect\renders'
E = r'C:\programmieren\zencrifice\manafold-p6-architect\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-2018-manafold-p6-architect\evidence'

def fr(sub, f):
    return rf.load(fr'{R}\{sub}\{f:04d}.rgb')

# --- fog attribution: rest vs mana-only vs off, same frame ---
for f in (0, 200, 399):
    row = np.concatenate([fr('manafold-rest', f), fr('manafold-fogprobe-mana', f), fr('manafold-fogprobe-off', f)], axis=1)
    rf.save_png(row, fr'{E}\fog-triptych-f{f:03d}.png', scale=2)

# difference: rest minus fogprobe-mana isolates the SMEAR contribution;
# fogprobe-mana minus fogprobe-off isolates the MANA (motes) contribution
a, b, c = fr('manafold-rest', 200).astype(int), fr('manafold-fogprobe-mana', 200).astype(int), fr('manafold-fogprobe-off', 200).astype(int)
smear_px = int((np.abs(a - b).max(axis=2) > 12).sum())
mana_px  = int((np.abs(b - c).max(axis=2) > 12).sum())
print(f'f200 changed px: smear-contribution={smear_px}  mana-contribution={mana_px}')
# pale translucent zone: greyish-white near-body px (low sat, high val) count per variant
def pale(img):
    i = img.astype(int); mx = i.max(axis=2); mn = i.min(axis=2)
    return int(((mx > 130) & (mx - mn < 45)).sum())
print('pale/grey-white px f200: rest=%d mana-only=%d off=%d' % (pale(a), pale(b), pale(c)))

# --- camera 360 vs 240 ---
for f in (0, 100, 210):
    row = np.concatenate([fr('manafold-channel', f), fr('manafold-channel-360', f)], axis=1)
    rf.save_png(row, fr'{E}\channel-360-pair-f{f:03d}.png', scale=2)

# --- moving rig vs shipping sun ---
for f in (0, 100, 210):
    row = np.concatenate([fr('manafold-channel', f), fr('manafold-channel-ml', f)], axis=1)
    rf.save_png(row, fr'{E}\channel-ml-pair-f{f:03d}.png', scale=2)

# aqua mana pixels + saturation, channel vs 360 vs ml at f100/f210
def aqua(img):
    i = img.astype(int); r, g, b = i[...,0], i[...,1], i[...,2]
    m = (g > r + 25) & (b > r + 15) & (i.max(axis=2) >= 110)
    if m.sum() == 0: return 0, 0.0
    mx = i.max(axis=2)[m]; mn = i.min(axis=2)[m]
    return int(m.sum()), float(np.mean(mx - mn))
for f in (100, 210):
    for s in ('manafold-channel', 'manafold-channel-360', 'manafold-channel-ml'):
        n, sat = aqua(fr(s, f))
        print(f'{s} f{f}: aqua_px={n} sat_spread={sat:.1f}')
