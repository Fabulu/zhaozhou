"""THE STENCIL CLAMP PROBE.

`fold_mvc` (manafold_fx.h:576) clamps a stencil point toward the pocket
centre by 10% at a time until it lies inside the six-anchor hexagon, giving
up after 12 tries and returning the plain CENTROID -- i.e. collapsing that
station onto a single dot. Nothing measures how often that fires.

This reimplements the clamp exactly (integer, same order of operations) and
reports, per shape and stencil scale, how much of the AUTHORED shape actually
survives to be drawn. Measurement on the comparison side: it says how far the
drawn shape is from the authored one, it does not choose a scale.

  stencilprobe.py [scale ...]
"""
import sys, math

ANCH = [(89, 664), (45, 996), (0, 1337), (-231, 1564), (-566, 1434), (-230, 179)]
CU, CV = -240, 1120

def isqrt(v):
    return math.isqrt(max(v, 0))

def clamp(pu, pv):
    """Returns (final_pu, final_pv, shrinks, collapsed)."""
    for shrink in range(12):
        dx = [a[0] - pu for a in ANCH]
        dy = [a[1] - pv for a in ANCH]
        d = [isqrt(dx[i] * dx[i] + dy[i] * dy[i]) for i in range(6)]
        if any(x < 2 for x in d):
            return pu, pv, shrink, False
        outside = False
        for i in range(6):
            j = (i + 1) % 6
            if dx[i] * dy[j] - dy[i] * dx[j] <= 0:
                outside = True
                break
        if outside:
            pu = CU + (pu - CU) * 9 // 10
            pv = CV + (pv - CV) * 9 // 10
            continue
        return pu, pv, shrink, False
    return CU, CV, 12, True   # the all-682 centroid fallback

def stencils():
    """The six authored stencils, per-mille, from fold_stencils()."""
    N = 18
    st = [[None] * N for _ in range(6)]
    def scp(i, n, r, ph):
        a = (i * 65536 // n + ph) & 0xFFFF
        return (int(r * math.cos(a * 2 * math.pi / 65536)),
                int(r * math.sin(a * 2 * math.pi / 65536)))
    for i in range(N):
        st[0][i] = scp(i, N, 1000, 0)                        # RING
        if i < 2:                                            # STAR
            st[1][i] = (0, 0) if i == 0 else (90, -90)
        else:
            arm, stn = (i - 2) // 4, (i - 2) % 4
            r = [320, 620, 900, 1150][stn]
            a = (0x2000 + arm * 0x4000) & 0xFFFF
            st[1][i] = (int(r * math.cos(a * 2 * math.pi / 65536)),
                        int(r * math.sin(a * 2 * math.pi / 65536)))
        half = N // 2                                        # BAR
        j = i % half
        t = -1000 + 2000 * j // (half - 1)
        off = 190 if i < half else -190
        st[2][i] = (t * 707 // 1000 - off * 707 // 1000, t * 707 // 1000 + off * 707 // 1000)
        a = (0x5000 + i * 47000 // (N - 1)) & 0xFFFF         # CRESCENT
        st[3][i] = (int(950 * math.cos(a * 2 * math.pi / 65536)),
                    int(950 * math.sin(a * 2 * math.pi / 65536)))
        per = N // 3                                         # TRIANGLE
        e, j2 = i // per, i % per
        vx = [(0, 1000), (-870, -500), (870, -500), (0, 1000)]
        st[4][i] = (vx[e][0] + (vx[e + 1][0] - vx[e][0]) * j2 // per,
                    vx[e][1] + (vx[e + 1][1] - vx[e][1]) * j2 // per)
        a = (i * 98304 // (N - 1)) & 0xFFFF                  # S-CURL
        r = 1000 - 780 * i // (N - 1)
        st[5][i] = (int(r * math.cos(a * 2 * math.pi / 65536)),
                    int(r * math.sin(a * 2 * math.pi / 65536)))
    return st

NAMES = ["RING", "STAR", "BAR", "CRESCENT", "TRIANGLE", "S-CURL"]

if __name__ == "__main__":
    scales = [int(x) for x in sys.argv[1:]] or [300]
    st = stencils()
    for scale in scales:
        print("\n=== stencil scale %d mm ===" % scale)
        print("shape      clamped/18  collapsed  authored_extent  drawn_extent  kept%")
        for sh in range(6):
            nc = ncoll = 0
            au = []; dr = []
            for i in range(18):
                pu = CU + st[sh][i][0] * scale // 1000
                pv = CV + st[sh][i][1] * scale // 1000
                fu, fv, k, coll = clamp(pu, pv)
                if k: nc += 1
                if coll: ncoll += 1
                au.append((pu, pv)); dr.append((fu, fv))
            def extent(ps):
                return max(math.hypot(p[0] - CU, p[1] - CV) for p in ps)
            ae, de = extent(au), extent(dr)
            print("%-10s %5d/18   %6d      %8.0f mm   %8.0f mm  %4.0f%%" % (
                NAMES[sh], nc, ncoll, ae, de, 100.0 * de / ae if ae else 0))
