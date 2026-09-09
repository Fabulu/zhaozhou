# FORGE.CLIFF rearchitecture — bitmap fabric out, RAM streams in

Date: 2026-09-09. Branch `zixxtrixx-v8-closeout`. Author: architecture subagent.
Scope: Packet D (Rearchitecture Roadmap §7) / ALM-Liberation Roadmap §6.
Verification artifact: `tests/forge/cliff_rearch_model.py` (this report's claims
that say "verified" were checked by that executable model against a faithful
mirror of `zref::forge::rim_plan`; run it — 601 differential pages + directed
cases, all passing, with coverage counters proving the awkward paths fired).

A note on provenance: ALM-Liberation §6.4 cites supplied files
`models/cliff_radix.hpp` and `tests/test_cliff_cpp.cpp`. **Those files do not
exist** anywhere in the tree or the research zips. The model above was built
fresh for this packet; nothing here leans on the phantom files.

---

## 1. Where the 7,664 ALM actually is

The estimate is current: `git hash-object fpga/rtl/forge/zhao_forge_cliff.sv`
returns `e220bd6d5b32e82c17e171a9e7e881bcc323852d`, the exact blob the map row
in `reports/synthesis/zhao_block_map.json` was taken from. Map row (Quartus
17.0.2, map-only, `rtlCleanAtHead: true`): **7,664 estimated ALM, 8,149 comb
ALUTs, 3,875 registers, 2 DSP, 119,808 block-memory bits.**

What the ALM is NOT: the payload tables. The map row's `inferredMemories`
proves all four already live in RAM —

| table | geometry | bits | inferred |
|---|---|---|---|
| `edge_key_r` | 2048 x 12 | 24,576 | yes (SDP) |
| `edge_span_r` | 2048 x 6 | 12,288 | yes (SDP) |
| `prio_mem_r` | 2048 x 32 | 65,536 | yes (SDP) |
| `run_mem_r` | 1024 x 17 | 17,408 | yes (SDP) |

Sum = 119,808 bits — exactly the row's `blockMemoryBits`. **Do not claim these
as new savings** (they are the already-applied key/span split, map variant
`zhao_forge_cliff@edge-split-wip` shows identical numbers).

What the ALM IS, by line in `fpga/rtl/forge/zhao_forge_cliff.sv`:

1. **`solid_r [1155:0]`** — decl line 291. 1,156 flip-flops, plus:
   - a 1,156-way write decoder: `solid_r[ld_cnt_r] <= ld_solid_i` (line 574);
   - **five dynamic bit-selects** off `win_base_c`, an 11-bit computed index
     (lines 375–384): self + four neighbour offsets, i.e. five ~1156:1 mux
     cones, plus the `(cj+1)*34 + (ci+1)` index product feeding them.
2. **`alive_r [2047:0]`** — decl line 322. 2,048 flip-flops, plus:
   - three dynamically indexed write sites (set at `idx_r` line 600; clear at
     `mhead_r + mstep_r` line 720; clear at `idx_r` line 807) — each a
     2,048-way decode into per-FF enables;
   - dynamic reads `alive_r[idx_r[10:0]]` (lines 753, 781, 798, 830) — a
     2048:1 mux cone;
   - a 2,048-bit bulk clear every page (line 566).
3. Control FSM, comparators, and address arithmetic around them (the two DSPs
   are the `vbase_c`/`vspan_c` 16x16 products, lines 425–426; they stay).

Register arithmetic (exact, from declarations): the two bitmaps are
**3,204 of the 3,875 registers (83%)**. Every other declared register in the
module sums to 442; the remaining ~229 are Quartus's RAM read-address/output
rescue registers and synthesis duplication. The comb-ALUT total cannot be split
per-structure without a cross-probe map experiment (flat module, no hierarchy
in the report), so no percentage is claimed — but the only wide combinational
structures in the source ARE the bitmap mux/decoder cones, the index products
and 32-bit comparators. The roadmap's sentence "over 3,200 bitmap bits before
other state" is confirmed at 3,204 exactly.

---

## 2. F1 — the solid window becomes one RAM plus four 34-bit rows

**Storage.** One M10K holding the window as **34 words x 34 bits** (row-major,
window row `wj` = page row `wj-1`; word 0 is the north halo). Written during
StLoad through a 34-bit assembly shift register: `ld_solid_i` shifts in, one
RAM write per 34 bits — 34 writes per page, same 1,156-cycle load handshake,
same external interface, bit order unchanged.

**Live registers.** Four 34-bit row registers: `north`, `current`, `south`,
`prefetch` (136 FFs total, replacing 1,156):

- while scanning page row `cj`: `north` = window row `cj`, `current` = row
  `cj+1`, `south` = row `cj+2`;
- `prefetch` fills from the RAM with window row `cj+3` — **one 34-bit read
  inside a 128-clock cell row** (4 sides x 32 cells), so refills cost zero
  bubbles; verified: the model asserts ≤1 refill per row and that no consulted
  bit ever leaves the three live rows;
- row end: rotate `north<-current`, `current<-south`, `south<-prefetch`.

**Addressing.** The five consulted bits are `current[ci+1]` (self),
`north[ci+1]`, `south[ci+1]`, `current[ci]`, `current[ci+2]`. Two options,
both knobs: (a) five 34:1 muxes (~10 ALUTs each — already 200x smaller cones
than today), or (b) **shift the three live rows left one bit per cell** (every
4 clocks), so all five bits sit at fixed positions and the muxes vanish.
Option (b) is the recommendation; the rotate then reloads from a 34-bit
holding copy per row.

**Order preservation.** The scan loop is untouched: `cj` outer, `ci` inner,
side 0..3, one side per clock (F3). Enumeration still appends to
`edge_key/edge_span` through the existing single write port — no four-write
edge memory is created (the roadmap's explicit trap). **Verified:** 400 pages,
edge stream byte-identical to direct-window enumeration, including partial
pages (cw/ch < 32) and halo/OUT cases.

---

## 3. F2/F3 — compaction deletes the alive state ENTIRELY (RAM included)

The roadmap's §6.2 proposes moving `alive_r` into a 2048x1 RAM. The compaction
route (Rearchitecture §7.3) is strictly better: **no alive storage exists in
any form**, and the model proves the accounting stays exact.

**The observation that makes it work** (verified over 601 pages): a merge
writes `take` into the head's span, and the entries it consumes are *exactly
the `take-1` entries following the head* — runs are disjoint, each run merges
at most once (the RTL's own one-pass theorem, header lines 78–105), so dead
interiors are contiguous and start immediately after a head whose span names
their length. Therefore:

> **The span field IS the compaction instruction.** A walker doing
> `rd += span[rd]` visits every live entry in scan order and never touches a
> dead one. No alive bitmap, no merge-plan list, no per-interior clears.

**Merge phase change.** StMsel (counting sort: length 32 down to 2, run table
in ascending start order — unchanged, ties law preserved) now completes each
merge in **one cycle**: write `span[head] <= take`. StMdead's `take-1`
interior-clear cycles are deleted along with the bits they cleared.
`take = min(len, need+1)` is untouched (R1).

**Compaction pass, fused with priority (C2 preserved).** One pass after the
merge phase, only when a merge happened or selection is needed:

```
rd = 0; wr = 0
while rd != cnt:
    (key, span) = edge[rd]                # sync read
    prio        = signed_max(vd[va], vd[vb])   # from FINAL span endpoints
    if wr < rd:  edge[wr] <= (key, span)  # in-place, wr strictly behind
    prio_mem[wr] <= prio
    wr += 1; rd += span
cnt' = wr        # dense, all-live, spans final
```

- **In place, no ping-pong store.** The existing SDP RAMs serve it: read `rd`,
  write `wr`, and the write is *suppressed while `wr == rd`* so no
  same-address read/write ever occurs (mandatory RAM rule). `wr < rd` holds
  strictly after the first skip. This beats the roadmap's "separate ping-pong
  compact edge stores" — zero extra M10K, zero extra ports.
- Priority is computed here from the **final merged endpoints** (the law:
  merged priority comes from final span, not interior endpoints) at the
  existing 3-clocks-per-edge vdist schedule, which also hides the span-walk's
  read-latency dependency. With `vdist_en` low: 2 clocks/entry, or the pass is
  skipped entirely when no merge occurred.
- After compaction every later phase runs on a **dense 0..cnt'-1 table** — no
  alive checks, no dead-entry iterations (the current FSM wastes full
  iterations and even vdist reads on dead entries in StPrio/StBsCount/
  StGtCount/StKeep; that waste goes with the bitmap).

**The awkward cases, verified (model section [2] + coverage counters):**

- two 20-edge runs under `need = 31` produce spans **20 and 13** (partial
  prefix on the last merge), never two 20s; the second run's unmerged 7-edge
  tail survives as unit edges in order;
- a dropped merged span of 13 adds **13 bodies** to `page_dropped_o`
  (2,890 dropped-merged-span events across the random pages);
- `emitted_bodies + dropped == enumerated` held on all 601 pages;
- checkerboard merges nothing (155 need>0-with-no-runs pages), R3 intact.

---

## 4. F4 — exact radix selection, and its honest price

**Algorithm** (replaces the 32 threshold passes; verified equal to the
reference's stable descending sort + prefix on 208 directed key sets and all
differential pages):

```
key = prio XOR 0x8000_0000            # unsigned key spans the signed range
K = Budget; prefix = 0; mask = 0; gt = 0
for shift in (24, 16, 8, 0):
    clear 256-counter histogram                    # 256 cycles
    for each of cnt' entries:                      # dense post-compaction
        if (key & mask) == prefix: hist[(key>>shift)&255]++
    walk b = 255 downto 0:
        if K > hist[b]: K -= hist[b]; gt += hist[b]
        else: select b; break
    prefix |= b << shift; mask |= 255 << shift
cutoff = prefix
tie_quota = Budget - gt               # gt == count(key > cutoff), by construction
```

`gt` accumulates the skipped-above buckets across all four passes, so the
separate StGtCount pass disappears too. Final filter, one pass in scan order:
keep `key > cutoff`; keep `key == cutoff` while `tie_quota > 0`; else drop and
add `span` bodies. `vdist_en == 0` skips the whole machine and keeps the first
`Budget` entries (F4's null path). Extremes verified: INT32_MIN/MAX, all-equal
keys, exactly-Budget ties, under-budget pages.

**The histogram RMW hazard is part of the design** (roadmap §6.5). The 256x12
counter RAM takes a read-modify-write with 1-cycle read latency; back-to-back
same-bucket increments collide. Two committed candidates, both knobs:

- **rmw=2 (first candidate, honest):** two cycles per key, no forwarding.
- **rmw=1:** a 2-deep forwarding window comparing in-flight bucket ids,
  selecting the newest matching count. Must be proven on: all 2,048 keys in
  one bucket, two alternating buckets, and read/write stall injection.

Clears are priced at 256 cycles per pass (a generation-tagged histogram that
makes clears free is a later knob, +4 bits width).

**Honest cycle budget** (schedule-derived — NOT simulator-measured; model
section [6] prints these from the same page statistics for both designs):

| page | current RTL | new, rmw=2 | new, rmw=1 |
|---|---|---|---|
| checkerboard (2,048 edges, no merges, full priority degrade) | 85,700 | 34,468 (**2.49x**) | 26,276 (**3.26x**) |
| merge-heavy walls (789 edges, 24 merges, 757 live) | 37,399 | 18,485 (**2.02x**) | 15,457 (**2.42x**) |

The "32 passes -> 4 passes" headline does NOT become 8x, exactly as the owner
warned: load (1,156), enumeration (4,096), the run pass, the merge sweep,
3-cycle-per-edge priority reads and clears are all unchanged or nearly so, and
they now dominate. The radix phase itself shrinks 65,568 -> 18,432/10,240 on
the worst page; everything around it is the floor.

**Frame math** (100 MHz / 60 Hz = 1,666,666 cycles): if FORGE owned the whole
frame, pathological pages per frame go **19 -> 48 (rmw=2) / 63 (rmw=1)**.
An all-pathological 256-page workload needs ~6.7M cycles — **four frames — so
radix does NOT close that case**; matching the roadmap's own arithmetic
(§6.6). The escalation ladder stands unchanged and is not part of this
packet's claims: (A) exact topology memoization keyed on solid/deformation
epoch, (B) shared topology + per-view priority, (C) 2–4 lane-partitioned
scan/histogram banks (price every read replica), (D) real stage pipelining
with separately owned page buffers. The vdist master's ONE read port is the
priority-build limiter (6,144 cycles on the worst page) before any lane talk;
a two-reader endpoint arena is priced there, not assumed. The 512 budget is
not touched.

---

## 5. The new block, phase by phase

States: `Idle -> Load -> Enum -> AfterEnum -> Runs -> Msel(+MergeWrite) ->
CompactPrio -> RadixClear/RadixScan/RadixWalk x4 -> KeepEmit -> Idle`.
Interface unchanged (same ports, same handshake hygiene, same C3/C4
counter/status semantics, `triangles_submitted += 2` per emitted edge).

Memory sheet (mandatory RAM rules, one owner each, no partial writes, no
same-address R/W, no reset loop over payload — count/phase authorize reads):

| memory | geometry | ports | phases touching it | M10K |
|---|---|---|---|---|
| solid window | 34 x 34 | 1W (Load) / 1R (Enum prefetch) | Load, Enum | 1 |
| edge key | 2048 x 12 | 1W / 1R | Enum(W), Runs(R), Compact(R+W), KeepEmit(R) | 3 |
| edge span | 2048 x 6 | 1W / 1R | Enum(W), Msel(W: head only), Compact(R+W), KeepEmit(R) | 2 |
| priority | 2048 x 32 | 1W / 1R | Compact(W), Radix(R), KeepEmit(R) | 7 |
| run table | 1024 x 17 | 1W / 1R | Runs(W), Msel(R) | 2 |
| radix histogram | 256 x 12 | 1R + 1W (RMW) | Radix | 1 |
| **total** | | | | **16** |

(The ALM-Liberation sheet says ~17 because it keeps a 2048x1 alive RAM; the
compaction route deletes it. Excluded, per the roadmap: scan replication,
endpoint arenas, double buffering, wall-emission storage.)

Registers expected: ~442 control (unchanged) + 136 row buffers + RAM
address/output registers — under ~800, versus 3,875. Flip-flop deletion is
arithmetic; the **ALM number is NOT claimed** — no delta exists until a
quartus_map of the new source (repo law). One page in flight (C5) is kept.

Named knobs (owner's control, all localparams): `Budget`, `MaxEdges`,
`WinDim`, `RadixBits` (8, with 4-bit/8-pass as the documented alternative),
`RmwForwardDepth` (0 or 2), histogram clear style, row-shift vs row-mux
enumeration.

Quartus 17.0 syntax constraints for the implementation: explicit
`generate/endgenerate`, separate `genvar`, elaboration checks inside
`initial begin`, run `python tools/quartus/check_quartus17_syntax.py`; and a
Verilator-clean file is not synthesizable until `quartus_map` says so.

---

## 6. What remains UNPRICED — the wall emitter does not vanish

This block is the **rim-PLANNING half only** (its own header, lines 22–38,
says so). The emission half — turning a selected rim edge into wall vertices —
needs the tessellator's stitched top lattice at the owning subpatch's LOD, the
bottom lattice, the accumulated rim length driving strata U (spec 6.6),
winding, material metadata, and output into the shared geometry/projection
path (no second private projector). None of that is built, none of it is in
the 7,664, and none of the savings above pay for it. Its queue, lattice read
ports, projection demand and frame-calendar slot are a separate obligation in
FORGE's domain (§6.7 / §7.5). "Planning is cheap now" does not make emission
zero-cost.

---

## 7. Verified vs unverified

**Verified (executable, `tests/forge/cliff_rearch_model.py`):**
- row-window enumeration == direct-window enumeration, 400 pages, exact order;
  ≤1 row refill per 128-clock row;
- span-walk compaction == live-in-scan-order, 601 differential pages; merges
  disjoint; identity `bodies + dropped == enumerated` on every page;
- partial-prefix merge (20/20 need 31 -> 20+13), dropped-body accounting
  (13-span -> 13), checkerboard (2,048/0 merged/512 kept/1,536 dropped);
- radix cutoff + gt + tie quota == stable descending sort + prefix, incl.
  INT32_MIN/MAX, all-equal, heavy ties, vdist-off, under-budget;
- coverage counters nonzero for every awkward path (a check that never fired
  is not a check).

**Verified (repo evidence):**
- blob SHA of current source == the estimate's blob; map row numbers; all four
  payload tables already RAM; 3,204/3,875 registers are the two bitmaps.

**Unverified — do not quote as results:**
- any post-change ALM/ALUT/Fmax number (needs quartus_map on real RTL);
- RAM inference of the new arrangement (in-place SDP compaction, histogram
  RMW) — the surface_sheet precedent says inference dies on patterns, not
  intentions;
- cycle counts are schedule-derived, not Verilator-measured; RTL will add
  pipeline-prime and handshake cycles (order ~tens per phase);
- M10K count of 16 is a layout candidate, not a fitter count;
- everything about the wall emitter.

---

## 8. First implementation step

**D1 (this packet's next commit): memory/control swap, algorithm frozen.**
Replace `solid_r` with the row-window (§2) and `alive_r` with span-walk
compaction (§3), keeping the existing 32-pass threshold selector untouched.
One change axis — if the differential moves, it's the storage, not the law.
Gate: the existing `tests/forge/forge_cliff_directed.cpp` +
`forge_cliff_random.cpp` suites pass unmodified against the new RTL (they pin
R1/R2/R3 and the identity), plus a new directed stall/backpressure case for
the compactor's `wr==rd` write suppression.

**D2:** swap the selector for the radix machine (§4), rmw=2 first; prove the
histogram on all-same-bucket, alternating-bucket and stall stimulus; keep the
32-pass selector in the tree until D2's differential is green.

**D3:** the wall emitter — separately scheduled, separately priced.

**The one fit gate** (batched, per fit discipline; question named in advance):
after D2, one `quartus_map` answering: "estimated ALM of the new
`zhao_forge_cliff`, with all six memories in `inferredMemories` and
`ramConversionWarnings: 0`?" No fit before that point; nothing here needs one.
