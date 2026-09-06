"""Does a star pixel ever land on SKY/GROUND rather than inside the creature?

Method: the cel renderer draws a near-black ink outline around the creature's
whole silhouette (the union of ALL its parts, eyes included). So flood-fill the
background from the frame border over everything that is NOT ink; the fill
reaches all sky and ground and stops at the ink ring. Anything unreached is
inside the creature. A star pixel inside the fill IS drawn against sky.
Imports the committed reader (rgbframe); no fifth frame reader.
"""
import sys, numpy as np
from collections import deque
sys.path.insert(0, 'zhaozhou/tools/reel')
from rgbframe import load

def background(a, ink_max=60):
    h, w, _ = a.shape
    notink = a.max(axis=2) >= ink_max
    seen = np.zeros((h, w), bool); q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if notink[y, x] and not seen[y, x]: seen[y, x] = True; q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if notink[y, x] and not seen[y, x]: seen[y, x] = True; q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny, nx = y+dy, x+dx
            if 0 <= ny < h and 0 <= nx < w and notink[ny,nx] and not seen[ny,nx]:
                seen[ny,nx] = True; q.append((ny,nx))
    return seen

def stars(a):
    r,g,b = a[:,:,0], a[:,:,1], a[:,:,2]
    return ((r>190)&(g>200)&(b>200)) | ((b>170)&(g>150)&(r<150))

def check(path):
    a = load(path).astype(int)
    bg = background(a); st = stars(a)
    return a, bg, st, int(st.sum()), int((st & bg).sum()), 100.0*bg.sum()/bg.size
