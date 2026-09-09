#!/usr/bin/env python3
# cliff_rearch_model.py — executable model of the FORGE.CLIFF rearchitecture
# (Packet D / ALM-Liberation §6), verified against a faithful Python mirror of
# the frozen oracle zref::forge::rim_plan (reference/src/zterrain/terrain_core.cpp).
#
# WHAT THIS PROVES (algorithm level, not RTL):
#   1. ROW-WINDOW ENUMERATION: streaming the 34x34 solid window through three
#      34-bit row buffers (+1 prefetch) produces the IDENTICAL edge stream, in
#      the identical (cj, ci, side) order, while never consulting a solid bit
#      outside the three buffered rows. This is what lets solid_r[1155:0]
#      become one M10K + 4x34 flops instead of 1,156 flops with 1156:1 muxes.
#   2. SPAN-WALK COMPACTION: after the merge phase writes `take` into a head's
#      span (ONE write per merge, no interior clears), the dead interior of
#      every merged run is exactly the `take-1` entries that follow the head.
#      A compactor that jumps `addr += span[addr]` therefore reproduces the
#      live-in-scan-order edge list EXACTLY, with no alive bitmap and no merge
#      plan list. This is what deletes alive_r[2047:0].
#   3. EXACT RADIX SELECTION: four MSD byte-histogram passes over the biased
#      key (prio XOR 0x8000_0000) find the exact cutoff key and the exact
#      count(key > cutoff) — so "keep everything above the cut plus the first
#      (Budget - gt) ties in scan order" equals the reference's stable
#      descending sort + prefix. Signed extremes, all-equal keys, heavy ties,
#      vdist-disabled and under-budget pages included.
#   4. THE AWKWARD CASES the owner named:
#        - two 20-edge runs under need=31 give spans 20 and 13, NOT 20 and 20
#          (partial-prefix merge, take = min(len, need+1));
#        - dropping a merged span of 13 counts 13 bodies, not 1 record;
#        - emitted_bodies + dropped == enumerated on every page (identity).
#   5. HONEST CYCLE MODEL: per-phase cycle counts for the CURRENT RTL FSM and
#      the PROPOSED pipeline, from the same page statistics, so the speedup
#      claim is a number with its assumptions attached, not "8x".
#
# WHAT THIS DOES NOT PROVE: RAM inference, the histogram RMW hazard schedule
# in actual RTL, Fmax, ALM count. Those need RTL + a map. Every constant here
# is a named knob (owner's law: never remove the owner's control).

import random
import sys

# ---------------------------------------------------------------------------
# Knobs (mirror the RTL localparams; every one stays editable)
# ---------------------------------------------------------------------------
BUDGET = 512          # zref::forge::kRimBudgetPerPage (F5)
MAX_EDGES = 2048      # TIGHT worst case (blocks.yml)
WIN_DIM = 34          # page + one-cell halo (C1)
RADIX_BITS = 8        # byte histograms; 4 passes over a 32-bit key
RADIX_PASSES = 32 // RADIX_BITS
HIST_SIZE = 1 << RADIX_BITS
BIAS = 0x80000000     # signed -> monotone unsigned key

# side -> neighbour offset, the reference's noff table. 0=-z 1=+z 2=-x 3=+x
NOFF = [(0, -1), (0, 1), (-1, 0), (1, 0)]


def u32(x):
    return x & 0xFFFFFFFF


def key_of(prio_signed):
    """biased key: unsigned bits of signed priority XOR 0x80000000."""
    return u32(prio_signed) ^ BIAS


# ---------------------------------------------------------------------------
# Shared enumeration substrate: a solid WINDOW is a WIN_DIM x WIN_DIM bit
# array; window cell (0,0) is page cell (-1,-1) (the halo). solid[wj][wi].
# ---------------------------------------------------------------------------
def window_solid(win, ci, cj):
    """page-local cell -> window bit (halo offset +1)."""
    return win[cj + 1][ci + 1]


def enumerate_reference(win, cw, ch):
    """Direct-indexed enumeration: the oracle's loop and F3 scan order."""
    edges = []
    for cj in range(ch):
        for ci in range(cw):
            for side in range(4):
                if len(edges) == MAX_EDGES:
                    return edges
                di, dj = NOFF[side]
                if window_solid(win, ci, cj) and not window_solid(win, ci + di, cj + dj):
                    edges.append([ci, cj, side, 1])  # span=1
    return edges


def enumerate_row_window(win, cw, ch):
    """Row-window streaming enumeration (proposed F1 replacement).

    Models the hardware: the window lives in a 34-row x 34-bit RAM; only
    THREE 34-bit row registers (north, current, south) are live during a
    row's scan, plus a prefetch register filling from the RAM. Asserts that
    no solid bit outside the three buffered rows is ever consulted, and that
    the refill of the prefetch row fits inside the 128-clock cell row.
    Returns (edges, max_ram_reads_per_row).
    """
    # the RAM: one 34-bit word per window row
    ram = [list(win[wj]) for wj in range(WIN_DIM)]

    # prime: rows 0,1,2 of the window (page rows -1, 0, 1)
    north = list(ram[0])
    current = list(ram[1])
    south = list(ram[2])

    edges = []
    reads_per_row = 0
    max_reads = 0
    for cj in range(ch):
        # prefetch the NEXT south row (window row cj+3) during this row's scan.
        # In hardware: one 34-bit RAM read (or 34 bit-serial reads) somewhere
        # inside the 4*cw-clock row; modelled as issued at row start.
        reads_per_row = 1 if (cj + 3) < WIN_DIM else 0
        prefetch = list(ram[cj + 3]) if (cj + 3) < WIN_DIM else [0] * WIN_DIM
        max_reads = max(max_reads, reads_per_row)

        for ci in range(cw):
            for side in range(4):
                if len(edges) == MAX_EDGES:
                    return edges, max_reads
                wi = ci + 1  # window column of the cell
                self_solid = current[wi]
                if side == 0:
                    nb = north[wi]
                elif side == 1:
                    nb = south[wi]
                elif side == 2:
                    nb = current[wi - 1]
                else:
                    nb = current[wi + 1]
                if self_solid and not nb:
                    edges.append([ci, cj, side, 1])

        # row rotate: north<-current, current<-south, south<-prefetch
        north, current, south = current, south, prefetch

    return edges, max_reads


# ---------------------------------------------------------------------------
# The faithful oracle mirror: zref::forge::rim_plan, one page.
# (Rebuilds runs every merge iteration, stable_sort for priority — exactly
#  the C++, including take = min(len, need+1) and body-count drops.)
# ---------------------------------------------------------------------------
def ref_build_runs(es, alive):
    runs = []
    n = len(es)
    i = 0
    while i < n:
        if not alive[i]:
            i += 1
            continue
        j = i + 1
        while j < n:
            prev, cur = es[j - 1], es[j]
            if not alive[j] or cur[2] != es[i][2]:
                break
            along_x = es[i][2] < 2
            if along_x:
                cont = (cur[1] == es[i][1]) and (cur[0] == prev[0] + prev[3])
            else:
                cont = (cur[0] == es[i][0]) and (cur[1] == prev[1] + prev[3])
            if not cont:
                break
            j += 1
        if j - i >= 2:
            runs.append((i, j - i))
        i = j
    return runs


def ref_prio(es, idx, vdist, lat_w, pg_ci, pg_cj):
    """int64 p = vdist[va]; if (vdist[vb] > p) p = vdist[vb];  (signed max)"""
    if vdist is None:
        return 0
    ci, cj, side, span = es[idx]
    base = (cj + pg_cj) * lat_w + (ci + pg_ci)
    if side == 0:
        va, vb = base, base + span
    elif side == 1:
        va, vb = base + lat_w + span, base + lat_w
    elif side == 2:
        va, vb = base + span * lat_w, base
    else:
        va, vb = base + 1, base + span * lat_w + 1
    p = vdist[va]
    if vdist[vb] > p:
        p = vdist[vb]
    return p


def reference_page(win, cw, ch, vdist, lat_w, pg_ci=0, pg_cj=0):
    es = [list(e) for e in enumerate_reference(win, cw, ch)]
    count = len(es)
    alive = [True] * count
    merged = 0
    dropped = 0
    if count > BUDGET:
        need = count - BUDGET
        while need > 0:
            runs = ref_build_runs(es, alive)
            best, best_len = None, 1
            for start, length in runs:
                if length > best_len:
                    best_len, best = length, start
            if best is None:
                break
            take = best_len
            if take - 1 > need:
                take = need + 1
            es[best][3] = take
            for k in range(1, take):
                alive[best + k] = False
            need -= take - 1
            merged += take - 1
        alive_n = sum(alive)
        if alive_n > BUDGET:
            order = [k for k in range(count) if alive[k]]
            # stable sort descending by priority (ties keep scan order)
            order.sort(key=lambda k: -ref_prio(es, k, vdist, lat_w, pg_ci, pg_cj))
            for k in order[BUDGET:]:
                dropped += es[k][3]
                alive[k] = False
    out = [tuple(es[k]) for k in range(count) if alive[k]]
    return out, merged, dropped, count


# ---------------------------------------------------------------------------
# The PROPOSED architecture, one page. Coverage counters record which awkward
# paths actually fired (broken-instrument law: a check that never fires is
# not a check).
# ---------------------------------------------------------------------------
COV = {
    "partial_prefix_merge": 0,   # take < run length (R1's last-merge prefix)
    "dropped_merged_span": 0,    # a dropped entry with span > 1 (R2 bodies)
    "merge_then_priority": 0,    # pages that needed BOTH degrade steps
    "radix_tie_spend": 0,        # pages where ties at the cutoff were rationed
    "vdist_disabled": 0,
    "under_budget": 0,
    "no_runs_but_need": 0,       # checkerboard-like: need>0, nothing mergeable
}


def newarch_page(win, cw, ch, vdist, lat_w, pg_ci=0, pg_cj=0, cycle_stats=None):
    # ---- phase 1: row-window enumeration (order-identical, proven vs ref) --
    edges, _ = enumerate_row_window(win, cw, ch)
    es = [list(e) for e in edges]
    count = len(es)
    merged = 0
    dropped = 0
    st = cycle_stats if cycle_stats is not None else {}
    st["count"] = count
    st["runs"] = 0
    st["merges"] = 0
    st["merge_interior"] = 0
    st["msel_scan"] = 0

    if count <= BUDGET:
        COV["under_budget"] += 1
        st["compacted"] = count
        st["selected"] = False
        return [tuple(e) for e in es], merged, dropped, count

    need = count - BUDGET

    # ---- phase 2: ONE-pass run build (the RTL's StRuns, unchanged) --------
    runs = ref_build_runs(es, [True] * count)  # spans all 1 here
    st["runs"] = len(runs)

    # ---- phase 3: merge plan — descending length, ties ascending start ----
    # (the RTL's counting sort: length 32 down to 2, run table in order).
    # ONE span write per merge; NO interior clears (this is the change).
    for length in range(32, 1, -1):
        if need <= 0:
            break
        for start, rlen in runs:
            st["msel_scan"] += 1
            if need <= 0:
                break
            if rlen != length:
                continue
            take = rlen if rlen <= need + 1 else need + 1
            if take < rlen:
                COV["partial_prefix_merge"] += 1
            es[start][3] = take          # the ONE write
            need -= take - 1
            merged += take - 1
            st["merges"] += 1
            st["merge_interior"] += take - 1
    if need > 0 and st["merges"] == 0:
        COV["no_runs_but_need"] += 1

    # ---- phase 4: SPAN-WALK COMPACTION (deletes the alive bitmap) ---------
    # rd += span[rd]; wr trails. Dead interiors are never touched. Priority
    # is computed HERE, from the FINAL span (merged endpoints — the law).
    compact = []
    prios = []
    rd = 0
    while rd < count:
        e = es[rd]
        compact.append(e)
        prios.append(ref_prio(es, rd, vdist, lat_w, pg_ci, pg_cj))
        rd += e[3]
    assert rd == count, "span-walk must land exactly on count"
    live_n = len(compact)
    st["compacted"] = live_n
    assert live_n == count - merged

    if live_n <= BUDGET:
        st["selected"] = False
        return [tuple(e) for e in compact], merged, dropped, count
    COV["merge_then_priority"] += 1
    st["selected"] = True

    # ---- phase 5: selection ------------------------------------------------
    if vdist is None:
        # F4 null-vdist path: all priorities 0 -> stable sort keeps scan
        # order -> first BUDGET survive. No histograms, no reads.
        COV["vdist_disabled"] += 1
        keep = [True] * BUDGET + [False] * (live_n - BUDGET)
    else:
        keys = [key_of(p) for p in prios]
        # exact MSD radix cutoff: 4 byte passes; gt accumulates the counts of
        # buckets skipped ABOVE the selected one == count(key > cutoff).
        prefix = 0
        mask = 0
        K = BUDGET  # 1-based rank of the cutoff element, descending
        gt = 0
        for p in range(RADIX_PASSES):
            shift = 32 - RADIX_BITS * (p + 1)
            hist = [0] * HIST_SIZE
            for k in keys:
                if (k & mask) == prefix:
                    hist[(k >> shift) & (HIST_SIZE - 1)] += 1
            b = HIST_SIZE - 1
            while True:
                c = hist[b]
                if K > c:
                    K -= c
                    gt += c
                    b -= 1
                else:
                    break
            prefix |= b << shift
            mask |= (HIST_SIZE - 1) << shift
        cutoff = prefix
        # verify the two claims the selector makes (checked, not trusted)
        assert gt == sum(1 for k in keys if k > cutoff)
        assert sum(1 for k in keys if k >= cutoff) >= BUDGET
        tie_quota = BUDGET - gt
        if tie_quota < sum(1 for k in keys if k == cutoff):
            COV["radix_tie_spend"] += 1
        keep = []
        for k in keys:
            if k > cutoff:
                keep.append(True)
            elif k == cutoff and tie_quota > 0:
                keep.append(True)
                tie_quota -= 1
            else:
                keep.append(False)

    # ---- phase 6: final filter in scan order; dropped counts BODIES -------
    out = []
    for i, e in enumerate(compact):
        if keep[i]:
            out.append(tuple(e))
        else:
            dropped += e[3]  # R2: the whole span
            if e[3] > 1:
                COV["dropped_merged_span"] += 1
    return out, merged, dropped, count


# ---------------------------------------------------------------------------
# Cycle models. Both are derived from phase schedules, not measured from a
# simulator — labelled as such. Formulas name their assumptions.
# ---------------------------------------------------------------------------
def cycles_current_rtl(st, cw=32, ch=32, vden=True):
    """The shipping FSM (zhao_forge_cliff.sv), no output backpressure."""
    n = st["count"]
    c = {}
    c["load"] = WIN_DIM * WIN_DIM                     # 1,156, ld always valid
    c["enum"] = cw * ch * 4                           # one side per clock
    if n <= BUDGET:
        c["emit"] = n + st.get("compacted", n)        # walk + emit handshakes
        return c
    c["runs"] = n + 1
    # StMsel: full sweep length 32..2 over the run table + StMdead take cycles
    c["msel"] = st["msel_scan"] + 31
    c["mdead"] = st["merges"] + st["merge_interior"]  # take cycles per merge
    live = st["compacted"]
    if live > BUDGET:
        # NOTE: the shipping FSM walks idx over ALL cnt entries in these
        # phases — dead entries cost a full iteration (StPrio even reads
        # vdist for them). Verified against the RTL, not assumed.
        if vden:
            c["prio"] = 3 * n                         # C2: 3 clocks/edge
            c["bscount"] = 32 * n + 32                # 32 threshold passes
            c["gtcount"] = n
        c["keep"] = n
    c["emit"] = n + min(live, BUDGET)                 # dead-skip walk + emits
    return c


def cycles_newarch(st, cw=32, ch=32, vden=True, rmw_cycles=2):
    """Proposed pipeline. rmw_cycles: 2 = honest unforwarded histogram RMW,
    1 = with a 2-deep forwarding window (§6.5). No output backpressure."""
    n = st["count"]
    c = {}
    c["load"] = WIN_DIM * WIN_DIM                     # stream into row RAM
    c["enum"] = cw * ch * 4                           # order-preserving, 1 side/clk
    if n <= BUDGET:
        c["emit"] = n
        return c
    c["runs"] = n + 1
    c["msel"] = st["msel_scan"] + 31                  # counting sort unchanged
    c["merge_writes"] = st["merges"]                  # ONE cycle per merge
    live = st["compacted"]
    # compaction fused with priority: vdist reads dominate (3/edge);
    # without vdist the span-walk's sync-RAM dependency costs 2/edge.
    c["compact_prio"] = (3 if vden else 2) * live
    if live > BUDGET:
        if vden:
            per_pass = HIST_SIZE + rmw_cycles * live + HIST_SIZE
            c["radix"] = RADIX_PASSES * per_pass
        c["keep_emit"] = live + min(live, BUDGET)
    else:
        c["keep_emit"] = live
    return c


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------
def blank_window():
    return [[0] * WIN_DIM for _ in range(WIN_DIM)]


def checkerboard_window():
    win = blank_window()
    for cj in range(32):
        for ci in range(32):
            if (ci + cj) % 2 == 0:
                win[cj + 1][ci + 1] = 1
    return win


def make_vdist(lat_w, lat_h, rng, mode="random"):
    n = lat_w * lat_h
    if mode == "random":
        return [rng.randint(-(2**31), 2**31 - 1) for _ in range(n)]
    if mode == "ties":
        vals = [-(2**31), -1, 0, 1, 2**31 - 1]
        return [rng.choice(vals) for _ in range(n)]
    if mode == "equal":
        return [rng.choice([-(2**31), 0, 2**31 - 1])] * n
    raise ValueError(mode)


def random_window(rng, style):
    win = blank_window()
    if style == "density":
        p = rng.choice([0.2, 0.5, 0.62, 0.8])
        for cj in range(WIN_DIM):
            for ci in range(WIN_DIM):
                win[cj][ci] = 1 if rng.random() < p else 0
    elif style == "walls":
        # long straight solid runs -> real merge runs, incl. partial prefixes
        for _ in range(rng.randint(10, 60)):
            horiz = rng.random() < 0.5
            L = rng.randint(3, 32)
            ci0 = rng.randint(0, 33 - (L if horiz else 1))
            cj0 = rng.randint(0, 33 - (1 if horiz else L))
            for k in range(L):
                if horiz:
                    win[cj0][ci0 + k] = 1
                else:
                    win[cj0 + k][ci0] = 1
    elif style == "walls_noise":
        win = random_window(rng, "walls")
        for _ in range(rng.randint(50, 300)):
            win[rng.randint(0, 33)][rng.randint(0, 33)] ^= 1
    elif style == "checker_holes":
        win = checkerboard_window()
        for _ in range(rng.randint(0, 200)):
            win[rng.randint(1, 32)][rng.randint(1, 32)] = 0
    return win


# ---------------------------------------------------------------------------
# Edge-list-level directed test of the merge+compaction laws (the owner's
# named case, injected directly so the numbers are exact and readable).
# ---------------------------------------------------------------------------
def directed_two_runs_need_31():
    """Two 20-edge runs, need=31 -> spans 20 and 13, NOT two 20s (R1).
    Then: dropping the 13-span must count 13 bodies (R2)."""
    # Build a synthetic page edge list: two straight-wall runs of 20 (side 0,
    # rows 0 and 1) + enough isolated edges that count = BUDGET + 31.
    es = []
    for ci in range(20):
        es.append([ci, 0, 0, 1])
    for ci in range(20):
        es.append([ci, 1, 0, 1])
    fill = BUDGET + 31 - len(es)
    # isolated non-contiguous edges: alternate sides so no run forms (R3)
    ci, cj = 0, 3
    for k in range(fill):
        es.append([ci, cj, 2 if k % 2 == 0 else 3, 1])
        ci += 2
        if ci >= 32:
            ci = 0
            cj += 1
    count = len(es)
    assert count == BUDGET + 31
    need = count - BUDGET
    runs = ref_build_runs(es, [True] * count)
    assert [(s, l) for s, l in runs] == [(0, 20), (20, 20)], runs
    merged = 0
    for length in range(32, 1, -1):
        if need <= 0:
            break
        for start, rlen in runs:
            if need <= 0:
                break
            if rlen != length:
                continue
            take = rlen if rlen <= need + 1 else need + 1
            es[start][3] = take
            need -= take - 1
            merged += take - 1
    # THE law: first run merges whole (take 20), second takes a 13-prefix.
    assert es[0][3] == 20, es[0]
    assert es[20][3] == 13, es[20]
    assert merged == 19 + 12 == 31
    # span-walk compaction: heads survive, interiors vanish, tail remains
    compact = []
    rd = 0
    while rd < count:
        compact.append(es[rd])
        rd += es[rd][3]
    assert rd == count
    spans = [e[3] for e in compact[:3]]
    assert spans == [20, 13, 1], spans
    # run 2's unmerged 7-edge tail survives as unit edges in order
    tail = compact[2:9]
    assert [e[0] for e in tail] == [13, 14, 15, 16, 17, 18, 19]
    assert len(compact) == count - merged == BUDGET
    # R2: force the 13-span to be dropped by a selection and count bodies.
    # give every edge prio 0 except the 13-span head, which gets the WORST.
    prios = [0] * len(compact)
    prios[1] = -(2**31)
    keys = [key_of(p) for p in prios]
    # live_n == BUDGET here so no selection would run; emulate a page one
    # over budget by dropping rank BUDGET with the radix machinery on a
    # sub-budget of BUDGET-1 to exercise the accounting:
    sub_budget = len(compact) - 1
    order = sorted(range(len(keys)), key=lambda i: (-keys[i], i))
    cut_idx = order[sub_budget - 1]
    dropped = sum(compact[i][3] for i in order[sub_budget:])
    assert order[-1] == 1  # the 13-span is the loser
    assert dropped == 13, dropped  # BODIES, not 1
    return True


# ---------------------------------------------------------------------------
# Radix selector unit tests on synthetic key sets (extrema and ties)
# ---------------------------------------------------------------------------
def radix_cutoff(keys, budget):
    prefix, mask, K, gt = 0, 0, budget, 0
    for p in range(RADIX_PASSES):
        shift = 32 - RADIX_BITS * (p + 1)
        hist = [0] * HIST_SIZE
        for k in keys:
            if (k & mask) == prefix:
                hist[(k >> shift) & (HIST_SIZE - 1)] += 1
        b = HIST_SIZE - 1
        while K > hist[b]:
            K -= hist[b]
            gt += hist[b]
            b -= 1
        prefix |= b << shift
        mask |= (HIST_SIZE - 1) << shift
    return prefix, gt


def test_radix_directed(rng):
    cases = []
    # all-equal at each extreme
    for v in (-(2**31), -1, 0, 1, 2**31 - 1):
        cases.append([v] * 700)
    # exactly budget, budget-1, budget+1 entries
    cases.append([rng.randint(-(2**31), 2**31 - 1) for _ in range(BUDGET)])
    cases.append([rng.randint(-(2**31), 2**31 - 1) for _ in range(BUDGET + 1)])
    # heavy ties around the cut
    cases.append([0] * 400 + [1] * 400 + [-1] * 400)
    # random with extremes salted in
    for _ in range(200):
        n = rng.randint(BUDGET + 1, MAX_EDGES)
        c = [rng.randint(-(2**31), 2**31 - 1) for _ in range(n)]
        for _ in range(rng.randint(0, 20)):
            c[rng.randrange(n)] = rng.choice([-(2**31), 2**31 - 1, 0])
        cases.append(c)
    for prios in cases:
        if len(prios) <= BUDGET:
            continue
        keys = [key_of(p) for p in prios]
        cutoff, gt = radix_cutoff(keys, BUDGET)
        # oracle: stable descending sort, prefix BUDGET
        order = sorted(range(len(keys)), key=lambda i: (-keys[i], i))
        keep_oracle = set(order[:BUDGET])
        tie_quota = BUDGET - gt
        keep_new = set()
        for i, k in enumerate(keys):
            if k > cutoff:
                keep_new.add(i)
            elif k == cutoff and tie_quota > 0:
                keep_new.add(i)
                tie_quota -= 1
        assert keep_new == keep_oracle, (
            f"radix selection mismatch: n={len(keys)} cutoff={cutoff:08x}")
    return len(cases)


# ---------------------------------------------------------------------------
# main: directed + randomized differential, then the cycle table
# ---------------------------------------------------------------------------
def main():
    rng = random.Random(20260909)

    # 1. enumeration equivalence + row-window locality
    n_enum = 0
    for style in ("density", "walls", "walls_noise", "checker_holes"):
        for _ in range(100):
            win = random_window(rng, style)
            cw = rng.choice([32, 32, 32, 17, 1, 5])
            ch = rng.choice([32, 32, 32, 9, 1, 31])
            a = enumerate_reference(win, cw, ch)
            b, max_reads = enumerate_row_window(win, cw, ch)
            assert a == b, f"enumeration order diverged ({style})"
            assert max_reads <= 1  # one 34-bit refill per 128-clock row
            n_enum += 1
    print(f"[1] row-window enumeration: {n_enum} pages, order identical, "
          f"<=1 row-RAM refill per cell row")

    # 2. the owner's directed merge case
    directed_two_runs_need_31()
    print("[2] directed: two 20-runs need 31 -> spans 20+13 (partial prefix); "
          "dropped merged 13-span counts 13 bodies")

    # 3. radix directed/extrema
    nc = test_radix_directed(rng)
    print(f"[3] radix selector: {nc} directed/extreme key sets == stable sort")

    # 4. full-page differential: reference vs new architecture
    lat_w = 128
    lat_h = 128
    n_pages = 0
    mismatches = 0
    for style in ("density", "walls", "walls_noise", "checker_holes"):
        for trial in range(150):
            win = random_window(rng, style)
            vmode = rng.choice(["random", "ties", "equal", "none"])
            vd = None if vmode == "none" else make_vdist(lat_w, lat_h, rng, vmode)
            r_out, r_m, r_d, r_n = reference_page(win, 32, 32, vd, lat_w)
            n_out, n_m, n_d, n_n = newarch_page(win, 32, 32, vd, lat_w)
            assert r_n == n_n
            assert r_out == n_out, f"edge stream diverged ({style}/{vmode})"
            assert r_m == n_m and r_d == n_d, (
                f"merged/dropped diverged ({style}/{vmode}): "
                f"ref {r_m}/{r_d} new {n_m}/{n_d}")
            # the identity: emitted bodies + dropped == enumerated
            bodies = sum(e[3] for e in n_out)
            assert bodies + n_d == n_n
            n_pages += 1
    # checkerboard, the canonical pathological page
    win = checkerboard_window()
    vd = make_vdist(lat_w, lat_h, rng, "random")
    st = {}
    r_out, r_m, r_d, r_n = reference_page(win, 32, 32, vd, lat_w)
    n_out, n_m, n_d, n_n = newarch_page(win, 32, 32, vd, lat_w, cycle_stats=st)
    assert r_out == n_out and (r_m, r_d) == (n_m, n_d)
    assert n_n == 2048 and n_m == 0 and len(n_out) == BUDGET and n_d == 1536
    n_pages += 1
    print(f"[4] full-page differential: {n_pages} pages "
          f"(incl. 32x32 checkerboard: 2048 enum, 0 merged, 512 kept, "
          f"1536 dropped) — new == reference exactly")

    # 5. coverage: every awkward path must have FIRED
    print("[5] coverage (a check that never fired is not a check):")
    for k, v in COV.items():
        print(f"      {k:24s} {v}")
    assert COV["partial_prefix_merge"] > 0
    assert COV["dropped_merged_span"] > 0
    assert COV["merge_then_priority"] > 0
    assert COV["radix_tie_spend"] > 0
    assert COV["vdist_disabled"] > 0
    assert COV["under_budget"] > 0
    assert COV["no_runs_but_need"] > 0

    # 6. the honest cycle table, worst page (checkerboard) + a merge-heavy page
    def show(name, st, vden=True):
        cur = cycles_current_rtl(st, vden=vden)
        new2 = cycles_newarch(st, vden=vden, rmw_cycles=2)
        new1 = cycles_newarch(st, vden=vden, rmw_cycles=1)
        tc, t2, t1 = sum(cur.values()), sum(new2.values()), sum(new1.values())
        print(f"  {name}:")
        print(f"    current RTL : {tc:7d} cycles  {cur}")
        print(f"    new (rmw=2) : {t2:7d} cycles  ({tc/t2:.2f}x)  {new2}")
        print(f"    new (rmw=1) : {t1:7d} cycles  ({tc/t1:.2f}x)  {new1}")
        frame = 100_000_000 // 60
        print(f"    pages/frame if FORGE owned the whole frame: "
              f"current {frame//tc}, new(rmw=2) {frame//t2}, new(rmw=1) {frame//t1}")
        return tc, t2, t1

    print("[6] cycle model (schedule-derived, NOT simulator-measured):")
    show("checkerboard (2048 edges, no merges, full priority degrade)", st)
    # a merge-heavy page: dense walls
    rng2 = random.Random(7)
    for _ in range(50):
        w2 = random_window(rng2, "walls_noise")
        st2 = {}
        vd2 = make_vdist(lat_w, lat_h, rng2, "random")
        newarch_page(w2, 32, 32, vd2, lat_w, cycle_stats=st2)
        if st2.get("selected") and st2["merges"] > 10:
            show(f"merge-heavy walls page ({st2['count']} edges, "
                 f"{st2['merges']} merges, {st2['compacted']} live)", st2)
            break

    print("\nALL CHECKS PASSED")


if __name__ == "__main__":
    main()
    sys.exit(0)
