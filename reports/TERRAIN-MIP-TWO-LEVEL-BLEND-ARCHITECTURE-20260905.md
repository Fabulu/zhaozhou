# TERRAIN MIP TWO-LEVEL BLEND — "nearest within two mips, blended"

**Date** 2026-09-05
**Examined snapshot** `4b891518` (working tree; a Quartus fit was running on
`fpga/rtl/texture/zhao_texture_material_combine_v1.sv` while this was written,
so that file was neither read nor cited beyond its frozen contract in
`reports/islandrearchitecture5.md` §15)
**Status** ARCHITECTURE ONLY. No RTL, test or tool was changed. No simulation
was run and no fit exists for anything proposed here. Every number below is
either (a) read from a named source file today, (b) quoted from a named report,
or (c) arithmetic, and each is labelled. Nothing in this document is a
measurement of the proposed design.

This extends `reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt`
(the mipmapping architecture) and must not contradict
`reports/zhaozhou-texture-island-rearchitecture-v2-2026-09-05.txt` (the
recovery architecture, "v2" below). Where it supersedes a sentence of the
mipmapping doc it says so explicitly (§1.3).

---

## 0. EXECUTIVE DECISION

The owner has ruled in the cheap two-level option, verbatim:

> "Sample one texel from level L / Sample one texel from level L+1 / Decode
> both palette entries / Blend the two resulting colours using fractional LOD.
> Blend colours, never palette indices."

The design that serves this with the least new machinery:

1. **Fix the LOD nibble defect and widen the boundary LOD to the oracle's
   existing U4.4 ABI** (8 bits: integer level in [7:4], fraction in [3:0]).
   The fraction the defect currently misplaces IS the blend weight. One fix,
   two purposes. (§2)

2. **Issue the two texels as ONE planner request using two of the four
   existing address lanes** (`acc_en = 4'b0011`: lane 0 = level-L texel,
   lane 1 = level-L+1 texel). The cache already retires a request only when
   every enabled lane hits, already replays multi-line misses, and already
   returns all lanes in ONE response with ONE token. Pairing, miss-on-one/
   hit-on-other, and back-pressure are then answered by machinery that exists
   and is tested, instead of by a new pairing station between two independent
   in-flight requests. (§3.1–§3.3)

3. **Put a small holding-and-blend station between the dispatcher's CLUT queue
   and the completion merger** (proposed name `zhao_texture_palette_blend`).
   It issues the two palette lookups back-to-back, captures each one-clock
   palette answer into a register it reserved BEFORE issuing, blends with the
   frozen `lerp8` law, and presents one backpressurable SampleResult. This is
   the direct answer to "does the palette need a holding register": yes, and
   the station is that register, governed by a reserve-before-issue credit.
   (§3.5)

4. **Blend expanded 8-bit colours with the oracle's `lerp8`, weight
   `w8 = {lod_frac[3:0], 4'b0000}`.** No new rounding law is invented; the
   one discrepancy between the two textual formulations of `lerp8` in the
   repo is documented and resolved in favour of the oracle's bytes. (§4)

5. **Scope: terrain-opaque CLUT8 albedo only**, selected by a new mode bit
   MODE[21] plus a new response class `CLS_CLUT_MIP = 2'd3`, with a planner
   consistency check that turns any disagreement into a declared error. Stars,
   sky, and every raw-index user keep today's path untouched, and a mutation
   test proves the fence detects rather than intends. (§5)

Full trilinear (8 taps) and anisotropy are explicitly OUT of scope and are
recorded as deferred in §12. The Mosaic material-selection pattern is NOT
fixed by this feature and §6 says exactly what remains broken.

The island this lands in is over budget and not timing-closed (§8). This
feature is not free and this document says what it costs and where it sits in
the recovery sequence (WP6-adjacent; it must not preempt v2 §0 priorities 1–5).

---

## 1. OWNER RULING, SCOPE, AND WHAT THIS SUPERSEDES

### 1.1 In scope

* Two-level nearest sampling for CLUT8 terrain-opaque albedo: one texel at
  level L, one at level L+1, both palette-decoded, colours blended by
  fractional LOD.
* The end-to-end LOD contract fix (the nibble defect and the fraction's
  carriage).
* The scheduling, pairing, blending, capacity and test architecture for the
  above.

### 1.2 Out of scope, named as deferred (not designed here)

* Full trilinear (2×2 footprint on each of two levels, 8 taps).
* Anisotropic filtering.
* CLUT bilinear of any kind (the planner's `filter && is_clut` error stands).
* Per-fragment hardware LOD derivation (the footprint law stays the mipmapping
  doc's §5, computed upstream; this document only fixes the FORMAT contract).
* Mosaic prefiltered transition tails (asset-side; see §6).
* A second palette read port (a capacity option, priced in §7.4, not built).

### 1.3 What this supersedes in the mipmapping doc

The mipmapping doc's §3 default policy — "albedo: one selected mip, one
nearest texel" and "certainly does not require sampling two mip levels" — is
superseded FOR TERRAIN-OPAQUE ALBEDO by the owner's ruling quoted in §0. Its
§5 `ceil` level policy is superseded by the floor-plus-fraction law in §2.3,
because with a blend the fraction does the anti-pop work the conservative
`ceil` was doing. Everything else in that document stands, including its asset
pipeline (§3 a–f), its Mosaic analysis (§6), and its LOD-derivation law (§5).

The owner's rationale this design serves: at 240p, low resolution hides small
details but does not hide instability. The goal is "make the ground stop
crawling", not maximum detail. A blend whose weight is exact but whose look is
wrong fails; §11's WP-M4 therefore ends with looking at the moving picture,
per CLAUDE.md's art law.

---

## 2. THE LOD CONTRACT, END TO END

### 2.1 The confirmed defect (read from source today, snapshot `4b891518`)

`fpga/rtl/texture/zhao_texture_island_top.sv` (LODW = 4):

```systemverilog
.req_lod_i({{(8-LODW){1'b0}}, fr_tmu_lod}),
```

puts the island's 4-bit LOD in `req_lod_i[3:0]`.

`fpga/rtl/texture/zhao_texture_tmu_plan.sv` T1:

```systemverilog
lvl_req = m_mip_en ? t0_lod[7:4] : 4'd0;
```

selects the level from `req_lod_i[7:4]`. The planner's own T0 comment states
the law: "Only t0_lod[7:4] selects the level. The low nibble is the FRACTIONAL
LOD." So every level the island can express lands in the fraction field and
the selected level is always zero. **The mip level cannot currently be
non-zero through the composed island at all.** Both lines verified in the
current source today.

### 2.2 The frozen format: unsigned Q4.4, and it is already the oracle's ABI

`reference/include/zref/zref_texture.hpp:151` already declares the binding
LOD as `uint8_t lod; // U 4.4 (charter 9's Measure, computed upstream)`. The
island boundary, not the oracle, is out of ABI. The fix converges them:

**LOD is one unsigned 8-bit Q4.4 value everywhere.**

| bits  | meaning                              | consumer                       |
|-------|--------------------------------------|--------------------------------|
| [7:4] | integer level L, 0..15               | planner T1 level select (as-is)|
| [3:0] | fraction f, value f/16               | blend weight (new, §2.6)       |

The mipmapping doc §2 offered two contracts (integer-only at 4 bits, or typed
Q4.4 end to end) and warned "do not combine the two interpretations". This
design REQUIRES the fraction, so the Q4.4 contract is chosen and the
integer-only alternative is closed.

### 2.3 Who computes it, and the new level law (proposed)

The producer stays upstream of the island, per the mipmapping doc §5: terrain
setup (HPS or the reference path initially) computes the footprint

```
Gu = |du_raw/dx| + |du_raw/dy|;  Gv likewise;  G = max(Gu, Gv)
rho = G / 2^uv_shift            (texels per pixel, level-0 units)
```

New selection law (proposed; supersedes the ceil law for the blended path):

```
if rho <= 1:            lod_q44 = 0                (magnification: pure L0, w = 0, crisp)
else:                   lod_q44 = clamp(floor(16 * log2(rho)), 0, 255)
L = lod_q44[7:4],  f = lod_q44[3:0]
```

Cheap generation of `16*log2(rho)` for a positive fixed-point rho: the
leading-one position gives the integer part; the four bits below the leading
one give the fraction (the linear-in-mantissa log2 approximation). Its maximum
error is about 0.086 in log2 (arithmetic property of the approximation, not a
measurement); the error perturbs only the blend WEIGHT, never the address law,
and a weight off by 0.086 of a level is invisible next to the pop it removes.
The exact-vs-approximate choice belongs to the producer; the island contract
only fixes the format. The reference oracle must implement whichever law the
producer ships, bit-exactly (§4.4).

LOD is per view (mipmapping doc §5): split-screen cameras do not share one
value. Nothing in the island carriage below cares; it moves bytes.

### 2.4 Carriage: every hop, every width (the fix, stated concretely)

All per-fragment attributes follow the ingress-capture law from
`reports/G1D-INGRESS-CAPTURE-REPAIR-20260905.md`: captured at admission,
recovered by token; anything a TMU RESPONSE needs is keyed by FRAGROB slot.
The LOD already follows it; only its width and one wiring expression change.

| # | where | today | becomes |
|---|-------|-------|---------|
| 1 | `zhao_texture_island_top` param `LODW` | 4 | 8 |
| 2 | boundary port `frag_lod_i[LODW-1:0]` | 4-bit integer | 8-bit Q4.4 |
| 3 | capture array `flod_m[FCTXN]` | 4 bits/entry | 8 bits/entry (same array, wider) |
| 4 | read-back `f_lod_c` at read point 2 | 4 bits | 8 bits |
| 5 | FRAGROB `f_lod_i[3]`, `desc_met_m` (METW = BINDW+LODW) | 4-bit lanes | 8-bit lanes (parameter flows) |
| 6 | FRAGROB `tmu_lod_o` | 4 bits | 8 bits |
| 7 | island wiring to planner | `{{(8-LODW){1'b0}}, fr_tmu_lod}` | `req_lod_i(fr_tmu_lod)` — direct, the padding expression is DELETED |
| 8 | planner T1 `lvl_req = t0_lod[7:4]` | unchanged — it becomes correct once its input is honest | unchanged |
| 9 | NEW: per-slot weight table `blendw_m[DEPTH]`, 4 bits | — | written at `fr_alloc_valid` from `f_lod_c[3:0]`, beside `class_m`/`palslot_m`/`palgen_m` |

Parameter safety (the mipmapping doc's own warning): the adapter must not
assume LODW is any particular value again. With the port width equal to the
planner's `req_lod_i` width (8), the direct connection is the whole adapter;
add an elaboration-time check (`if (LODW != 8) $fatal`) so a future narrowing
is a loud error rather than a silent re-run of this defect.

Why the weight is keyed by slot and not carried in the response token: the
token is FULL. `plan_src_id = {class[15:14], slot[13:10], sidx[9:8],
gen[7:0]}` — with DEPTH=16 and GENW=8 the pad width is
`16 − 2 − 4 − 2 − 8 = 0` bits (arithmetic on the localparams in the island
top). And it is SAFE to key by slot: FRAGROB retires a fragment only when all
its required samples have arrived (`arr_q == req_q`), so a slot cannot be
reallocated while any of its sample responses — including a blend pair — is
still in flight. A stale response that outlives its fragment is refused by
generation at FRAGROB exactly as today; the blend it wasted is discarded work,
not wrong work. This invariant is stated here because the design leans on it;
WP-M3's test list includes a directed attempt to break it (§10.3).

### 2.5 Clamping interactions (planner, unchanged laws, one new arm)

* `MIP_EN = 0` with the blend mode bit set: declared error (§5), never a
  silent single-level fallback.
* `lvl_req > lvl_cap`: both levels clamp to `lvl_cap`; the pair degenerates to
  two reads of the SAME texel and the blend returns that colour exactly
  (lerp8(c,c,w) = c for all w — §4.2). Correct, slightly wasteful, and rare;
  the producer should clamp its lod to `16*max_level` to avoid it, but the
  hardware is correct without that courtesy.
* `L+1 > lvl_cap` with `L = lvl_cap`: same degenerate case, same answer.
* `L < lvl_cap`: the normal pair.

### 2.6 The weight

```
w8 = {lod_frac[3:0], 4'b0000}        // value f/16 exactly, in unit8 (raw/256)
```

`{f, f}` replication is deliberately NOT used: it maps f=15 to 255/256, which
is not 15/16, and unequal spacing of the weight steps buys nothing here. The
consequence of the 4-bit fraction is a bounded residual step at each level
rollover, quantified in §4.3.

---

## 3. THE SAMPLING SCHEDULE

### 3.1 One request, two lanes — the load-bearing decision

Two ways exist to get two texels for one logical sample:

**(A) Two independent planner requests** (an expander between FRAGROB and the
planner). Then: two cache responses at arbitrary separation; a pairing store
keyed by (slot, sidx) with a sub-identity per half (v2 §9.3's warning about
multiple outstanding lookups per sample); two independently droppable palette
answers; and a new class of failure — one half retired, the other lost —
which is precisely the shape of bug this island has spent a month excavating.
It also halves planner/cache request throughput for terrain.

**(B) One planner request with two enabled lanes.** Read from
`zhao_texture_cache_pipe.sv` today:

* each of the four lanes has its OWN tag/data bank; a request's lane-k address
  probes only bank k;
* C3 computes `c3_need_c = c2_en & ~c3_hit_c`; the request retires ONLY when
  every enabled lane hits (`c3_all_hit`), into ONE response FIFO entry
  carrying all `LANES*16` bits and ONE `src_id`;
* a miss picks the lowest needing lane's (tag, idx), fills that one line,
  multicasts it to every needing lane wanting the SAME (tag, idx), rewinds
  the issue pointer, and replays; distinct lines fill on successive rounds;
* nothing is emitted twice: "never emit two sample responses for one external
  request" is the existing law (v2 §8.2) and the rewind implements it.

Option B makes the pair ATOMIC in the existing machinery: the two texels
arrive together or not at all, misses on either or both lanes are the same
replay loop that four-tap bilinear already exercises, and back-pressure needs
no new analysis at this seam. **Option B is chosen.** Option A is rejected,
not deferred.

Two consequences of B, stated rather than discovered later:

* The two levels are ALWAYS in different cache lines (their level offsets
  differ by at least one full level's bytes), except in the clamp-degenerate
  case of §2.5 where both lanes carry the same address — and there the
  multicast mask serves both lanes from one fill. Both behaviours already
  exist.
* Lane 1's bank, today used only by bilinear direct formats, begins caching
  terrain L+1 lines. Lane banks are 16 lines each; the L+1 working set is 4×
  smaller than L's (arithmetic: quarter the texels per level). No structural
  cache change is proposed and none is needed.

### 3.2 The planner's pair arm (the one real datapath change)

New mode bit: **MODE[21] = MIP_BLEND_EN**. Today bits [31:21] are reserved
and `err_c` fires if any is set, so no existing legal client changes meaning:
previously-erroring words become defined, which is the safe direction.
Reserved shrinks to [31:22].

T1 additions:

```
blend_pair_c = m_mip_en && m_blend && (m_fmt == FMT_CLUT8);
level0_c     = clamp(lvl_req,          lvl_cap);
level1_c     = clamp(lvl_req + 4'd1,   lvl_cap);
err_c        |= m_blend && !m_mip_en;
err_c        |= m_blend && (m_fmt != FMT_CLUT8);
// class/mode consistency — the fence of §5:
err_c        |= (req_src_id_i[15:14] == CLS_CLUT_MIP) != blend_pair_c;
```

T2–T4: the four-lane address datapath is REUSED by re-binding lane semantics.
In bilinear mode the four lanes are the 2×2 footprint of one level; in pair
mode lanes 0 and 1 are the 1×1 footprint of two levels and lanes 2, 3 are
idle. Concretely:

* T2 computes the second level's coordinates by ONE MORE right shift of the
  already-shifted value, not a second barrel shifter:
  `tu_q(L+1) = tu_q(L) >>> 1` (sign-preserving), because
  `u <<< (log2w_l − 1)` is `(u <<< log2w_l) >>> 1`. Same for v. In pair mode
  the "iu1/iv1" registers carry the L+1 coordinates instead of the +1 taps.
* T2 carries PER-LANE masks and row shifts: in bilinear mode
  `masku0 = masku1`, in pair mode `masku1` is the L+1 level's mask (one bit
  shorter). The wrap folds in T3 are unchanged in shape — there are already
  four of them; only their mask operands gain a mux set in T2.
* T2 computes TWO level offsets (`REP4[level0] << shift0`,
  `REP4[level1] << shift1`) instead of one — a second copy of an existing
  small structure. REP4 stays the copied table with its two documented traps.
* T3 row bases: `row0 = vw0 << log2w_l(L)`, `row1 = vw1 << log2w_l(L+1)` —
  per-lane shift amounts carried from T2.
* T4 in pair mode: `addr[0] = base + lvl_off_L  + row0 + uw0`,
  `addr[1] = base + lvl_off_L1 + row1 + uw1`, `acc_en = 4'b0011`,
  `acc_filter = 0`, `acc_fu = acc_fv = 0`. `total_c[1]`'s operands are muxed
  by registered pair-mode state — the adders do not lengthen.

The planner's elastic five-stage structure, encodings, wrap laws, REP4 edge
cases and error handling are preserved per v2 §9.3 ("do not repeat them during
a 'cleanup'"). This is arithmetic re-binding plus registered muxes — i.e.
bookkeeping — and §8 treats it as such.

### 3.3 SAMPLE_META: per-request metadata the response needs

v2 §9.3 already prescribes the store this design needs:

```
SAMPLE_META[slot][sidx] := { bsel0, bsel1, is_pair }     (proposed contents, this feature)
```

written in the island top when the planner's access packet is accepted by the
cache (`plan_acc_valid && plan_acc_ready`), addressed by the slot/sidx fields
of `plan_acc_src`, with:

* `bsel0 = plan_acc_addr[0]`  (lane 0 byte address LSB)
* `bsel1 = plan_acc_addr[32]` (lane 1 byte address LSB)
* `is_pair = (plan_acc_src[15:14] == CLS_CLUT_MIP)`

Why it must exist: the cache addresses halfwords — `i_beat[k]` is taken from
address bits [3:1] and **bit 0 never enters the cache** — while a CLUT8 texel
is a byte. The old golden `zhao_texture_tmu_pipe.sv` stored `bytesel` per
in-flight record and selected `cac_data_i[15:8]` vs `[7:0]` on return; the
composed island's new path dropped that and reads `disp_clut_data[7:0]`
unconditionally, which is wrong for odd texel addresses (§9.1). The blend path
must not inherit that, and SAMPLE_META is the prescribed home.

Lifetime proof: exactly ONE cache request is outstanding per (slot, sidx) —
FRAGROB's work queue issues each sample once and the sample cannot be
re-issued or its slot reallocated before its response arrives (§2.4's
invariant) — so no lookup sub-identity is needed, which is the condition v2
§9.3 sets for this exact shape.

### 3.4 Dispatch: one new class, one closed drop

`CLS_CLUT_MIP = 2'd3` (the last free encoding of the 2-bit class field; the
field cannot grow — the token is full, §2.4). `zhao_texture_rsp_dispatch`
routes it to the SAME queue as `CLS_CLUT` (queue 0): one added case arm in
`head_room` and the push select. The class travels in the token, so the blend
station re-reads `tok[15:14]` at its head to distinguish pair from single —
no queue widening.

This change also closes a named defect: the dispatcher's `default: head_room
= 1'b1` currently DROPS an unknown class silently, which v2 §10.1 calls out
("it strands the parent sample"). With all four encodings defined after this
change, the default arm becomes unreachable; per v2 it must still become a
counted declared fault, not a drop. That is one always_comb arm plus one
counter.

### 3.5 The palette blend station — where this design either works or breaks

The hard constraints, verified in source today:

* `zhao_texture_palette_res` has **no `lu_ready_i`**; `lu_valid_o` is high for
  exactly one clock, one clock after `lu_valid_i`; the answer cannot be held.
* The island ties `disp_clut_ready = 1'b1` and gives palette answers strict
  priority into FRAGROB's response port, safe only because
  `zhao_texture_fragrob.tmu_rready_o = 1'b1`; `err_rsp_dropped_o` tripwires
  that assumption.
* A pair needs TWO lookups through a port that serves one per clock.

Proposed block: **`zhao_texture_palette_blend`**, between dispatch queue 0 and
the completion merger. It owns `disp_clut_ready` (no longer tied high), the
palette lookup port, and a small result skid toward the merger.

**Structure** (all depths are parameters; the numbers are the proposed
initial configuration, not fit results):

```
issue side    : reads {data64, tok16} at the CLUT queue head
                reads is_pair/bsel from SAMPLE_META[tok.slot][tok.sidx]
                reads weight from blendw_m[tok.slot]
                reads palslot_m/palgen_m[tok.slot]        (as today)
expectation Q : depth 2, entries {tok, which_half, is_pair, w8}
                (2 covers the palette's 1-clock latency at 1 issue/clock)
first-half reg: cL_r — the level-L colour, expanded 8:8:8, plus its verdict
result skid   : depth 2, entries {rgb24, status, tok}
credit        : result-skid free entries minus results already promised
                to in-flight lookups (a 2-bit counter)
```

**The credit law, which is the whole safety argument** (v2 §10.3: "Reserve
completion storage before issuing the read"):

* A SINGLE head may issue its one lookup only if `credit >= 1`; issuing
  decrements credit (one promised result).
* A PAIR head may issue its FIRST lookup only if `credit >= 1`; the pair
  promises ONE merged result, reserved at first issue. The second lookup
  needs no further credit. The two lookups issue on consecutive clocks
  (the palette is fully pipelined at one per clock); the CLUT queue head is
  held (`disp_clut_ready` low) until the second issues.
* Every palette answer is captured UNCONDITIONALLY into a register on the
  clock it appears: the first half into `cL_r`, the second half (or a
  single's only half) through the blend/expand into the reserved skid entry.
  A register write cannot fail, and the skid entry was reserved before the
  lookup left — so **no palette answer can ever be dropped, structurally**,
  which retires the tied-high assumption instead of extending it.
* Credit returns when the merger takes a result from the skid.

**Deadlock argument:** credit is replenished by the merger, whose consumer is
FRAGROB's response port with `tmu_rready_o` tied high; the bilinear lane
yields to the station under the existing priority. So the only wait chain is
station → merger → FRAGROB(always ready), which cannot cycle back to the
station. Upstream, exhausted credit holds the CLUT queue head; that
back-pressures dispatch (head-of-line, counted by `hol_stall_o`), the raw
FIFO, and the cache's `c1_go` — all existing, legal stall paths.

**Blend datapath:** on the second half's answer clock (or one registered clock
later if the fit wants it — the skid absorbs the latency either way):

```
out.r = lerp8(cL.r, cL1.r, w8);  same for g, b       // §4
status = verdict(cL) OR verdict(cL1)                  // stale/cold poison both
```

Three `lerp8` instances, one instantiation site. The COMBINE.V1 lesson
(fourteen multipliers from one helper in seven case arms; CLAUDE.md 2026-09-05)
is enforced by construction: the station has exactly ONE arm that multiplies,
and §10.4's mutation list includes the fitter-side tripwire `DSP == 0` for
this block (islandrearchitecture5.md §3.4 idiom).

**Verdicts:** a stale or non-resident answer on EITHER half poisons the whole
sample — the result carries a defined fault status, never a colour computed
from an unusable half. Blending one good colour with one garbage colour is
the exact "wrong that looks right on most pixels" the palette block's own
header warns about. Counted per half (`stale_o`/`cold_o` already count at the
palette; the station adds a pair-poisoned counter).

**Single-lookup behaviour** (CLS_CLUT, stars/sky): one lookup, one credit,
expansion, no blend — byte-identical to today's path EXCEPT the two adjacent
defects of §9, whose fixes are separately gated there. The station's single
arm is the compatibility surface; §10.2 pins it.

**Bookkeeping first:** this week's two timing lessons (perspuv's
`tail_q`/`free_cnt_q`, COMBINE.V1's §15.4 counters) both say the critical
path of a small scheduler is its pointers, not its arithmetic. Accordingly:
the credit counter is 2 bits with its net delta computed ONCE per clock (the
FRAGROB lost-update idiom, already written down in that block); the
expectation queue is depth 2 with 1-bit pointers; the head-class decode
(`tok[15:14]`, SAMPLE_META read) is REGISTERED into the issue stage rather
than rippling from the dispatch RAM read into the palette's address port. The
three lerp8 products sit behind registered operands and in front of the skid
— they are pipeline filler, not the path. This paragraph is a design
intention, not a timing result.

### 3.6 The completion merger

Unchanged in topology: the station (formerly "the palette") keeps strict
priority over the bilinear lane, `bil_out_ready = fr_tmu_rready &&
!station_valid`. The difference is that the station's output is now
backpressurable, so the priority is a POLICY rather than the only safe order.
It is kept because changing it buys nothing and v2 §2.10 says not to replace
it for cosmetic fairness. `err_rsp_dropped_o` is retargeted to the two now-
impossible events: a palette answer arriving with no expectation entry, and a
capture with no reserved skid entry. Sticky, asserted-zero in every test.

### 3.7 Failure modes, named

| # | scenario | behaviour | mechanism |
|---|----------|-----------|-----------|
| F1 | lane 0 hits, lane 1 misses (or vice versa) | request replays until both hit; ONE response | existing `c3_all_hit`/rewind |
| F2 | both lanes miss, different lines | two sequential fills, replay each round; bounded by the fill engine's one-line-at-a-time law | existing miss sequencer |
| F3 | clamp-degenerate: both lanes same address | one fill multicasts to both banks; blend(c,c,w)=c | existing `m_mask_c`; lerp8 identity |
| F4 | merger stalled by combiner refusal upstream burst | credit exhausts → CLUT queue holds → dispatch HOL → raw FIFO → cache `c1_go` holds | §3.5 credit law; all existing stall paths |
| F5 | palette reloaded between the two halves (BEGIN lands mid-pair) | second half reports stale → pair poisoned, defined fault | palette's accept-time verdict + station OR |
| F6 | response outlives its fragment (aborted/faulted elsewhere) | FRAGROB refuses on generation, as today; blend work discarded | existing `tmu_ok_c` |
| F7 | second lookup issued, first answer's capture clock coincides with new head issue | disjoint resources: capture writes `cL_r`/skid, issue reads queue head; no shared port | station structure |
| F8 | CLUT queue head is a pair but SAMPLE_META says single (or classes disagree anywhere) | planner err at T1 kills it before issue; if a corrupted token reaches the station anyway, mismatch is a counted declared fault, not a hang | §3.2 consistency err + station check |
| F9 | reset mid-pair | expectation queue and credit reset; palette's `l1_v_q` resets; no half survives to be mispaired | reset law |

The head-of-line cost is real and honest: a blended head occupies the CLUT
issue for 2 clocks, so NEAR/BIL responses behind it in the raw FIFO wait
longer. That is the dispatcher's admitted limitation (v2 §10.1) getting
heavier, it is counted by `hol_stall_o`, and §10.3's mixed test records it.

---

## 4. THE BLEND ARITHMETIC, EXACTLY

### 4.1 Operands

Blend AFTER palette decode and AFTER RGB565→RGB888 expansion, per channel, on
8-bit unsigned values. Expansion is the ORACLE's law
(`reference/include/zref/zref_sky.hpp:202`, `rgb565::to_rgb888`):

```
r8 = (r5 << 3) | (r5 >> 2);   g8 = (g6 << 2) | (g6 >> 4);   b8 = (b5 << 3) | (b5 >> 2)
```

(bit replication — NOT the zero-fill the island top currently performs on the
palette path; that divergence is §9.2's adjacent defect). Alpha: CLUT8 terrain
samples carry `a = 0xFF` today at this seam and the blend does not touch
alpha; lerp8(255,255,w) = 255 anyway, so blending it would be harmless and is
still not done — fewer multiplier sites.

Blending is per-channel on expanded colours. Blending the packed RGB565
fields, or the palette INDICES, is forbidden: "Palette entry 17 is not a
numerical colour halfway between palette entries 16 and 18" (mipmapping doc
§3), and §10.4's mutation list makes index-blending detectably red.

### 4.2 The product law: `lerp8`, frozen, with one discrepancy resolved

The law is the oracle's `lerp8`
(`reference/include/zref/zref_material.hpp:223`):

```c
d      = (int)b - (int)a;
mag    = |d|;
scaled = (mag * w + 128) >> 8;          // unit_mul's law on a magnitude
r      = a + (d < 0 ? -scaled : +scaled);
result = clamp(r, 0, 255);
```

with `unit8` convention `value = raw/256` and **255 is NOT 1.0**: w = 255
gives "almost b", by law.

**A discrepancy exists in the repo's own text and this document resolves it.**
`reports/islandrearchitecture5.md` §15.1 writes the recipe as
`lerp8(a,b,w) = sat_u8(a + rescale_s((b-a)*w, 8))` with
`rescale_s(x,8) = (x+128)>>8` (arithmetic shift). The oracle rounds the
MAGNITUDE and reapplies the sign. These differ when `(b−a)*w` is an exact
negative half-LSB multiple: e.g. x = −128 → `(x+128)>>8` = 0, magnitude form
= −1; x = −384 → −1 vs −2. The oracle's comment says why it chose magnitude
form ("symmetric about zero rather than biased toward +inf on darkening
lerps"). **Ruling proposed: the oracle's bytes govern.** The station's lerp8
implements the magnitude form; the exhaustive vector set in §10.4 pins it
against the oracle for every reachable input, so a transcription toward the
`rescale_s` form fails loudly instead of shipping a half-LSB bias. (The
combiner's own RTL was not read for this document — its fit is live — so no
claim is made about which form it implements; the same vector law already
governs it via §15.7's rail tests.)

### 4.3 The weight, and the bounded rollover step (arithmetic)

`w8 = {f, 4'b0000}` = 16f, value f/16, f ∈ 0..15.

* f = 0 → exactly the level-L colour (crisp within a level, the owner's
  stated goal).
* f = 15 → `cL + round(15/16 · (cL1 − cL))`, which is NOT pure L+1. At the
  rollover to (L+1, f=0) the output steps by at most `|cL1 − cL| / 16` per
  channel (arithmetic bound: the un-traversed 1/16 of the delta). Worst case
  16 codes if adjacent mips differ by full range at that texel; adjacent-mip
  differences at matching positions are typically far smaller because L+1 is
  the filtered version of L. This residual is 1/16 of the pop the feature
  removes, it is a property of the 4-bit Q4.4 fraction ABI, and widening the
  fraction is the only cure — recorded as a possible future ABI change, not
  designed.

### 4.4 What the reference oracle must implement (so hardware can be checked bit-exactly)

In `reference/include/zref/zref_texture.hpp` (or a sibling), a scalar
two-level sample law, built ONLY from already-frozen pieces:

```
sample_clut8_mip_blend(binding, u_raw, v_raw, lod_q44, palette):
  L0 = clamp(lod_q44 >> 4,     0, effective_cap)      // planner's clamp law
  L1 = clamp((lod_q44 >> 4)+1, 0, effective_cap)
  i0 = nearest CLUT8 index at level L0                 // existing plan/address law,
  i1 = nearest CLUT8 index at level L1                 //   incl. level_offset_texels and wrap
  c0 = to_rgb888(palette[i0]);  c1 = to_rgb888(palette[i1])   // zref_sky replication law
  w  = (lod_q44 & 0xF) << 4
  out.{r,g,b} = lerp8(c0.{r,g,b}, c1.{r,g,b}, w)       // zref_material::detail::lerp8
  out.a = 0xFF
  verdicts: stale/cold per half, OR'd, poisoning the sample
```

plus the producer's LOD law of §2.3 (exact or approximate, whichever ships),
and the existing `plan()` exposure extended so directed tests can pin BOTH
levels' addresses without memory (the mipmapping doc §2's offset table —
L0 0; L1 4096; L2 5120; L3 5376; L4 5440; L5 5456; L6 5460 for an unpadded
64×64 CLUT8 chain — is the independent check).

No new rounding, wrap, offset, expansion or product law is introduced
anywhere in this feature. That sentence is the point of this section.

---

## 5. WHICH MATERIALS TAKE THIS PATH, AND THE FENCE THAT ENFORCES IT

**In:** terrain-opaque CLUT8 albedo bindings, and (when they exist) Mosaic
coarse-tail bindings, which are ordinary CLUT8 bindings by construction
(mipmapping doc §6).

**Out, untouched:** stars, sky backdrop, every raw-palette-index user, every
direct-format binding, CLUT4, and anything whose contract depends on
nearest-single semantics. The owner's constraint is explicit: no silent
behaviour change for raw-index semantics.

The fence is three independent mechanisms, so intention is not the
enforcement:

1. **Binding opt-in:** MODE[21] must be set in the binding's mode word. Every
   existing legal mode word has it clear (it is inside today's must-be-zero
   reserved field), so no existing binding can drift onto the new path.
2. **Class opt-in:** the fragment's class must be `CLS_CLUT_MIP` (2'd3), a
   previously unused encoding. The class already "comes from the material
   binding, which is upstream of this island" (island top header) — stars
   present CLS_CLUT as today.
3. **Consistency error:** planner T1 raises `err_c` when class and mode
   disagree (§3.2), so a half-configured binding is a DECLARED error, not
   whichever path won the race. This is the charter's "no unsupported state
   silently falls back" gate applied to the new state.

Proof it detects rather than intends: §10.4's mutations include (a) setting
MODE[21] on a star binding — must go red via the consistency error and via
the star scene's byte-identity check; (b) deleting the consistency check —
the byte-identity check must still go red. Two fences, independently
observed.

The raw-index contract (`§15.1: "Palette index is sample0.index"`) is
untouched: a blended sample has no meaningful single index, and the blended
path never emits one. When the fuller machine carries `smp_idx`, a
CLS_CLUT_MIP sample's index field must be a declared non-value, not
whichever of the two indices happened to be second. Recorded here so the
WP6 integration does not improvise it.

---

## 6. MOSAIC: WHAT THIS FIXES AND WHAT IT DOES NOT

The mipmapping doc §6's warning stands in full. Mosaic picks matA or matB per
BASE-LEVEL world texel by a frozen hash. This feature blends each CANDIDATE
texture's mip levels; it does nothing to the SELECTION pattern's frequency
content. Therefore:

* **Fixed:** the abrupt level-transition pop within each candidate texture —
  the "texture becoming abruptly blurrier across a hillside" strip the owner
  named. That is this feature's whole claim.
* **NOT fixed:** distant ground shimmering between two material colours as
  the sub-pixel hash pick flickers. That requires the prefiltered
  transition-tail assets of the mipmapping doc §6 (or equivalent), which are
  asset-side work resolving to ONE binding per region before issue.
* **Composition:** when those tails exist, a tail is an ordinary CLUT8
  mip-chained binding, so transitions BETWEEN TAIL LEVELS take this blend
  path with zero additional hardware. The two features stack; neither
  substitutes for the other.
* **Unchanged fact:** in the composed island today Mosaic's pick feeds only a
  counter, not the address path (v2 §2.11). This feature neither worsens nor
  repairs that; the binding-resolution work that consumes the pick is WP6.

No overclaim: after this feature ships, minified Mosaic terrain will still
crawl where the hash dominates the signal. Anyone evaluating WP-M4's renders
should expect that and not file it against this feature.

---

## 7. CAPACITY AND THROUGHPUT — ARITHMETIC, LABELLED AS ARITHMETIC

Workload mix, QUOTED from `zhao_texture_tmu_pipe.sv`'s header (per frame):
terrain CLUT8 nearest 276,480; sky backdrop CLUT8 nearest 92,160; stars CLUT8
nearest 128,000. Frame budget at the 100 MHz product clock, quoted from
islandrearchitecture5.md §15.4: 1,333,333 clocks.

### 7.1 Requests and cache pressure (arithmetic)

* Planner/cache REQUESTS per blended sample: 1 (unchanged — the single-packet
  decision). Zero request-rate cost for reusing the existing sampler.
* Enabled lanes per blended request: 2 instead of 1. Lane-fetch work rises,
  line-FETCH traffic mostly does not: steady-state, the L and L+1 lines each
  serve many neighbouring fragments (that reuse is the entire premise of a
  texture cache), and the added L+1 working set is 1/4 of L's per level
  (4:1 texel ratio). Cold-start doubles the distinct lines touched per
  terrain region (both chains warm up). These are arithmetic statements about
  line counts, NOT a measured miss rate; §10.3's capacity test records
  actual `cache_misses_o`/`fills_o` under the terrain trace, per the
  mipmapping doc §8's "performance hypothesis, not a measured saving"
  discipline.
* Asset bytes: the 4/3 chain factor, unchanged from the mipmapping doc §8
  (1 MiB → 1.333 MiB for 256 tiles). No addition from this feature.

### 7.2 Palette port pressure (arithmetic on the quoted mix)

Lookups per frame today: 276,480 + 92,160 + 128,000 = 496,640.
With terrain blended (×2): 552,960 + 92,160 + 128,000 = 773,120.
Increase: +55.7%. Against one lookup/clock: 773,120 / 1,333,333 = **58.0%
occupancy of the single palette port** (was 37.2%). Headroom remains, but the
port is now the first thing a bigger CLUT workload saturates; §7.4 prices the
escape hatch.

### 7.3 Service rates (arithmetic on today's structures)

* Blended CLUT sample: 2 station clocks (two lookups, pipelined). Ceiling 0.5
  blended samples/clock.
* TODAY this is not the limiter: FRAGROB's issue FSM (I_IDLE→I_READ→I_HOLD)
  transfers one sample per 3 clocks — v2 §2.11's named defect — so the
  station idles behind it. **Reusing the sampler costs no throughput at the
  island's current rates.**
* AFTER v2 WP3 (one sample/clock issue), the blended class alone tops out at
  0.5/clock. Whether that matters depends on the class MIX at the response
  merger: blended terrain at 0.5/clk, single CLUT at 1/clk, bilinear at
  1/(3·channels-built)/clk share one FRAGROB response port at 1/clk. On the
  quoted frame mix, total palette-side service = 773,120 station clocks =
  58% of the frame (same number as §7.2 — the port and the station are the
  same serialisation), leaving 42% of response-port clocks for the direct
  classes. That is arithmetic on the quoted mix; the mixed-trace test of
  §10.3 is what turns it into a measurement.
* Head-of-line: a pair head holds the CLUT queue 2 clocks, lengthening
  NEAR/BIL waits behind it in the raw FIFO (§3.7). Counted, not hidden.

### 7.4 If the palette port saturates (priced, deferred)

A second read port by slot-bank duplication (the M10K's two physical ports
are already spent on load-write + lookup-read): duplicate the 4-slot store,
+4–8 M10K by the same width/depth packing caveats the mipmapping doc §4
applies to RAM claims. Then two lookups issue in one clock and a pair costs
one station clock. NOT proposed now: §7.2's 58% does not justify it, and v2
§17.3's rejected-shortcuts discipline applies — measure the mixed trace
first.

---

## 8. COST, AND WHAT IT DISPLACES — A STRUCTURAL ARGUMENT, NOT AN ESTIMATE

Per the owner's instruction, no ALM figure is stated for this feature, as an
estimate or otherwise. ("A small mathematical operation surrounded by poor
scheduling has already proved capable of becoming a large hardware problem.")
What can be said structurally:

* **New arithmetic:** three 9×8-class products (the lerp8 lanes) with one
  instantiation site, in ALM logic, DSP-tripwired to zero; one extra
  REP4-shift level-offset; one extra sign-preserving right shift per axis.
  Every other datapath element is a re-binding of existing structures.
* **New bookkeeping:** the station's 2-deep expectation queue, 2-deep skid,
  2-bit credit; SAMPLE_META (DEPTH×3 × 3 bits, MLAB-class); blendw_m
  (DEPTH × 4 bits); per-lane mask/shift muxes in the planner; one dispatch
  case arm. This week's evidence says bookkeeping is where such blocks lose
  their clock, so §3.5's register placement is specified up front — and
  §11's WP-M5 fits the station leaf BEFORE the island refit so the first
  timing fact about it is a bounded attribution experiment (v2 §0 item 5).
* **Widths:** LODW 4→8 widens flod_m by 4×64 bits, desc_met_m by 4×3×16
  bits, and one planner input — linear width growth, no new ports.

**What it displaces:** the island measured 7,720 ALM / 69.05 MHz before the
ingress repair (that figure is stale in the unhelpful direction — the repair
added storage) against a nominal architecture total of 6,600 ALM
(islandrearchitecture5.md §3.3) inside a 41,910-ALM device, and does not meet
its 100/105 MHz gates. This feature adds to a machine that is already over
budget. It therefore ships INSIDE the recovery sequence, not beside it:
nothing here lands before v2's WP1 correctness capture, and the station/
planner changes ride WP6's "complete real semantic paths" wave, where the
palette-residency and binding work they depend on already lives. If the
composed island cannot afford the station, the displacement candidates are
the island's own §3.3 contingency rows, and that trade is the owner's to
rule on with leaf-fit numbers in hand — not this document's to assume.

---

## 9. ADJACENT DEFECTS FOUND WHILE DESIGNING THIS (dispositions, not scope creep)

These were found by reading the code paths this feature reuses. They are
recorded here because the new path must not INHERIT them, and because fixing
them changes existing outputs, which requires its own gate — silently folding
them into the feature would violate the owner's no-silent-change constraint.

### 9.1 CLUT byte-select is dropped at the composed island

The cache never sees address bit 0 (`i_beat[k]` reads bits [3:1]); the old
golden `zhao_texture_tmu_pipe` stored `bytesel` per record and selected
`cac_data_i[15:8]` vs `[7:0]`; the composed island's dispatch path reads
`disp_clut_data[7:0]` unconditionally (island top, `lu_idx_i` wiring). Odd
byte-addressed CLUT8 texels therefore decode the WRONG index on the composed
path. Nothing enforces even addresses. **Disposition:** SAMPLE_META (§3.3)
carries `bsel` for both lanes; the single-CLUT arm gets the same fix in
WP-M1, gated by its own before/after evidence because it changes star/sky
bytes — from wrong to oracle-matching, but changed is changed and the gate
must show it.

### 9.2 Palette RGB565→888 expansion diverges from the oracle

Island top expands with zero fill (`{r5, 3'b000}` shape); the oracle law is
bit replication (`zref_sky.hpp:202`). Maximum divergence 7 codes per channel
on white (arithmetic). **Disposition:** the station implements the oracle
law; the single-CLUT arm converges in the same WP-M1 gate as §9.1. Until that
gate, the composed island cannot pass a byte-exact CLUT oracle comparison —
which is worth knowing before anyone writes one and blames the new feature.

### 9.3 Unknown response class is silently dropped

`rsp_dispatch`'s `default: head_room = 1'b1` discards a class-3 response
today — exactly where CLS_CLUT_MIP would have gone had it been wired without
reading this block. v2 §10.1 already names the defect. **Disposition:** fixed
by §3.4 (class 3 defined; residual default becomes a counted declared fault).

---

## 10. THE TEST PLAN THAT CATCHES IT BEING WRONG

Governing laws: per-input identity, not aggregate histograms (G1-D: "counters
cannot see identity, and identity is what a transport bug destroys"); a gate
that has not been shown to FIRE has not been tested (CLAUDE.md, broken-
instrument law); exact drain, not `retired > 0` (v2 §2.8).

### 10.1 The scoreboard (extends `island_composed_directed`'s identity gate)

For every submitted fragment the bench precomputes, via the §4.4 oracle:
expected per-sample blended colour, expected verdict, expected final combined
colour, and its identity tag. On retire it asserts per fragment: exact colour
and alpha equality, tag seen exactly once, verdict as expected. End of test:
exact full drain (64 of 64 and N of N), zero `err_rsp_dropped_o`, zero
station-internal sticky faults, palette lookup counter EXACTLY
`2·pairs + singles`, per-recipe combine jobs exactly as the drive pattern
predicts (the existing gate, still armed).

### 10.2 The compatibility pin (the fence's other half)

A star/sky scene (CLS_CLUT, MODE[21]=0) run before and after every WP below,
asserting byte-identical retired colours against the recorded baseline —
EXCEPT across the WP-M1 gate, where the baseline is re-recorded against the
oracle with the §9.1/§9.2 corrections and the delta is itself the reviewed
artifact. After WP-M1, the pin is byte-exact oracle equality and never moves
again.

### 10.3 Directed cases (each is one scenario, not a category)

1. Explicit-level sweep: uniquely-coloured mip levels 0..6 through the ACTUAL
   island boundary; level 3 must not sample level 0 (the mipmapping doc §9A
   case that today FAILS by §2.1 — write it first, watch it fail, then fix).
2. Address pin: both levels' addresses against the independent offset table
   (L1=4096, L2=5120, … §4.4) via the oracle's `plan()`.
3. Fraction sweep: fixed L, f = 0..15, colours chosen so every step changes
   the output; f=0 must equal pure L exactly.
4. Clamp edges: L = cap, L = cap−1, MAX_LEVEL = 0, rectangular-chain caps;
   the degenerate pair must return the single colour exactly.
5. Consistency errors: MODE[21] without MIP_EN; MODE[21] with CLUT4/RGB565;
   class 3 with MODE[21] clear; class 0 with MODE[21] set. All must complete
   as DECLARED errors, none may hang or sample.
6. Byte parity: odd and even texel addresses on both lanes, pinned against
   the oracle (catches §9.1 forever).
7. Mid-pair palette reload: BEGIN timed between the two halves' lookups →
   pair poisoned, defined fault, nothing retired as colour; then the same
   with the reload one clock later (both halves old generation → both stale).
8. Back-pressure storm: random `out_ready_i` duty 10–90%, random fill delays;
   scoreboard still exact; `err_rsp_dropped_o` still zero; `hol_stall_o`
   recorded (not asserted) for §7's arithmetic to be checked against.
9. Invalid-input poison per the G1-D pattern: all idle boundary inputs driven
   with alternating legal-to-present wrong-to-use values, poison landing
   mid-stream; every measurement unchanged.
10. Mixed-class stress: blended terrain + single-CLUT stars + bilinear direct
    in one run, interleaved, with the full-drain and per-identity checks —
    the case where the 2-clock pair head and the HOL interact.
11. Nonlinear palette: entries are a pseudo-random permutation of colours so
    that ANY index arithmetic diverges from colour arithmetic at almost every
    texel (arms mutation M3 below with maximum sensitivity).

### 10.4 The mutation list — break it on purpose, watch it go red, put it back

| # | mutation (the wrong implementation) | must go red via |
|---|--------------------------------------|-----------------|
| M1 | reintroduce the nibble pad `{{4'b0}, lod}` | directed 1 |
| M2 | swap cL/cL1 into the lerp | directed 3 (f≠8 cases) |
| M3 | blend indices, decode once | directed 11 + scoreboard |
| M4 | weight read from live `frag_lod_i` pin instead of `blendw_m` | poison test 9 |
| M5 | issue second lookup without reserved credit | directed 8: sticky fault must fire |
| M6 | drop the SAMPLE_META bsel (use data[7:0] always) | directed 6 |
| M7 | route CLS_CLUT_MIP to the silent default arm | directed 10: exact drain fails (fragment never retires) |
| M8 | use one colour when the other half is stale | directed 7: verdict check |
| M9 | `rescale_s` rounding instead of magnitude rounding in lerp8 | exhaustive vectors below |
| M10 | MODE[21] on a star binding with the consistency err deleted | compatibility pin 10.2 |
| M11 | zero-fill expansion in the station | oracle equality (post-WP-M1) |
| M12 | pair issued as two planner requests (the rejected schedule) | scoreboard identity: two responses for one (slot,sidx,gen) → FRAGROB id_errors or duplicate-arrival check |

Every mutation is run once, its red is recorded, and it is reverted — a
detector that has not fired is not a detector.

### 10.5 Arithmetic vectors

lerp8: exhaustive over a ∈ 0..255, b ∈ 0..255, w ∈ {16f : f = 0..15} —
1,048,576 vectors (exhaustive for this feature's reachable weight domain),
hardware station vs `zref_material::detail::lerp8`, bit-exact. This is the
vector set that makes §4.2's rounding ruling enforceable. Rails (0, 1, 127,
128, 254, 255) additionally cross the full w ∈ 0..255 range so the shared
lerp8 lane stays correct for any future caller.

### 10.6 Fitter-side tripwires (islandrearchitecture5.md §3.4 idiom)

Station leaf fit: require DSP == 0; require the skid/queues NOT to force the
palette RAM out of M10K (the palette block's own reset-domain law); reject a
station register count that implies SAMPLE_META fell into flops. Numbers for
these tripwires are set when the first leaf fit exists, not invented here.

---

## 11. STAGED IMPLEMENTATION SEQUENCE (in the WP style; each stage has a stop condition)

Sequencing constraint: nothing below preempts v2 §0 priorities 1–5. WP-M0/M1
are independent correctness repairs and may ride with v2 WP1/WP6 waves; the
live-tree trap applies at every step (check `design/fit_targets.yml` closure
before editing while any fit runs).

**WP-M0 — LOD ABI freeze and the failing test.**
Write directed test 1 against the CURRENT island; record its red (level 3
samples level 0). Land §2.4's carriage changes (LODW 8, direct wiring,
elaboration check, blendw_m table written but unread). Re-run: test 1 green,
compatibility pin 10.2 byte-identical (the widened LOD is zero-extended by
producers until WP-M4).
*Stop: explicit levels 0..6 correct through the composed boundary; star pin
unchanged; no other behaviour moved.*

**WP-M1 — SAMPLE_META and CLUT oracle convergence (behaviour-changing, own gate).**
Build SAMPLE_META; fix §9.1 byte-select and §9.2 expansion for the EXISTING
single-CLUT path. Produce the before/after byte deltas as the reviewed
artifact; re-baseline pin 10.2 to oracle equality.
*Stop: composed single-CLUT path is bit-exact against the scalar oracle at
all address parities; the delta report exists and is acknowledged.*

**WP-M2 — planner pair arm and dispatch class.**
MODE[21], CLS_CLUT_MIP, T1 consistency errors, T2–T4 lane re-binding,
dispatch routing + declared-fault default. Verified at the planner boundary
against the oracle's two-level `plan()` (directed 2, 4, 5); the pair is
issued and the cache returns both texels in one response (observed at the
dispatch queue in a bench, no blending yet).
*Stop: pair addresses bit-exact vs oracle for a directed matrix of
{u,v,L,f,wrap,cap}; every consistency violation is a declared error; star
pin unchanged; F1–F3 exercised at the cache boundary.*

**WP-M3 — the blend station.**
`zhao_texture_palette_blend` with the credit law, expectation queue, skid,
lerp8 lanes; merger integration; sticky faults. Unit bench first (palette +
station alone): §10.5 vectors, F4–F9, mutations M2/M5/M8/M9. Then composed:
scoreboard 10.1, directed 3, 6, 7, 8, 9, 11; mutations M3/M4/M6/M7/M12.
*Stop: composed scoreboard exact under back-pressure and poison; every listed
mutation red then reverted; `err_rsp_dropped_o` and station faults zero in
every green run.*

**WP-M4 — the picture.**
Producer supplies real Q4.4 LODs (reference-path footprint law §2.3). Render
the receding-plain and grazing-angle scenes of the mipmapping doc §9C, in
motion, close-to-far-to-close, two cameras. LOOK at it (CLAUDE.md art law):
the level pop must have become a wash; Mosaic hash shimmer is EXPECTED to
remain (§6) and is not this feature's failure. Record level/weight histograms
and cache counters for §7's arithmetic to be checked.
*Stop: the owner (or their standing direction) accepts the motion read; the
capacity counters are recorded against §7's predictions with divergences
explained or filed.*

**WP-M5 — physical evidence, bounded.**
Leaf-fit the station (bounded attribution, v2 §0 item 5) with §10.6
tripwires; then the batched island refit that is ALREADY owed for the ingress
repair picks up this feature in the same pass — no dedicated island refit per
glue edit.
*Stop: leaf numbers exist with their worst paths named; the island refit
scorecard reports this feature's marginal cost honestly against §8; budget
breaches go to the owner as breaches, not waivers.*

---

## 12. WHAT IS NOT CLAIMED

* **No simulated or fitted result exists for this design.** Every stage above
  produces its own evidence; none of it exists today.
* Not claimed: that the island is correct or timing-closed underneath this
  feature — v2's ordering defect and budget overruns stand, and this feature
  inherits their schedule, not their solutions.
* Not claimed: any ALM, register, MHz or FPS figure for the feature. §7's
  percentages are arithmetic on a quoted workload mix; §7.1's cache-reuse
  statement is a hypothesis the capacity counters must test.
* Not claimed: that the blend improves the LOOK. The rationale predicts it;
  WP-M4's motion review decides it. Measurement never trumps looking.
* Not claimed: any Mosaic selection-pattern improvement (§6), any trilinear
  or anisotropic capability, any CLUT4/direct-format blend, or any
  per-fragment hardware LOD derivation.
* Not claimed: that the 4-bit fraction is sufficient forever — §4.3's
  rollover step is bounded, not zero, and widening it is an ABI change left
  to the owner.
* The §4.2 rounding ruling (oracle magnitude form governs) is PROPOSED here
  and becomes law when the owner or the frozen vector gate ratifies it; the
  combiner's in-flight fit was deliberately not consulted.

---

## APPENDIX A. SIGNAL AND BIT-POSITION LEDGER (normative for implementation)

```
LOD (everywhere)          lod_q44[7:0]   unsigned Q4.4; L = [7:4], f = [3:0]
mode word                 fmt[2:0] filter[3] wrap_u[5:4] wrap_v[7:6]
                          log2w[11:8] log2h[15:12] maxlvl[19:16] mip_en[20]
                          MIP_BLEND_EN[21] (NEW)  rsvd[31:22] must be 0
classes                   CLS_CLUT=0  CLS_NEAR=1  CLS_BIL=2  CLS_CLUT_MIP=3 (NEW)
token (SRCW=16, full)     {class[15:14], slot[13:10], sidx[9:8], gen[7:0]}
pair access packet        acc_en=4'b0011; lane0 = level-L texel byte addr;
                          lane1 = level-(L+1) texel byte addr; acc_filter=0;
                          acc_fu=acc_fv=0; acc_fmt=FMT_CLUT8
SAMPLE_META[slot][sidx]   {bsel0, bsel1, is_pair}; written at acc accept from
                          acc_addr[0], acc_addr[32], class; read at station issue
per-slot tables (island)  class_m / palslot_m / palgen_m (existing)
                          + blendw_m[DEPTH][3:0] (NEW), all written at fr_alloc_valid
weight                    w8 = {f, 4'b0000}   (f/16 exactly; 255 is NOT 1.0)
expansion (oracle law)    r8=(r5<<3)|(r5>>2)  g8=(g6<<2)|(g6>>4)  b8=(b5<<3)|(b5>>2)
blend (oracle law)        lerp8 magnitude form, zref_material.hpp:223; per R,G,B;
                          alpha not blended (0xFF at this seam)
verdicts                  stale/cold per half, OR'd; either poisons the pair
station credits           reserve 1 result-skid entry BEFORE first lookup of a
                          single or a pair; capture is unconditional; credit
                          returns on merger take
```

## APPENDIX B. SOURCE ANCHORS (read today, snapshot `4b891518`)

```
fpga/rtl/texture/zhao_texture_island_top.sv
    req_lod_i zero-extension (the defect); plan_src_id layout; class_m/palslot_m/
    palgen_m slot tables; palette expansion zero-fill; strict-priority merger and
    err_rsp_dropped_o; disp_clut_ready tie; GLUE 2/3 notes
fpga/rtl/texture/zhao_texture_tmu_plan.sv
    t0_lod[7:4] selection and the fraction comment; T1 sanitise/clamp; T2 scale;
    T3 wrap folds; T4 addresses; REP4 traps; elastic readies
fpga/rtl/texture/zhao_texture_cache_pipe.sv
    per-lane banks; c3_all_hit retirement; lowest-lane miss pick, multicast mask,
    rewind/replay; i_beat from addr[3:1] (bit 0 dropped); one-line fill engine
fpga/rtl/texture/zhao_texture_rsp_dispatch.sv
    raw FIFO; per-class queues; head_room; silent default drop; hol_stall_o
fpga/rtl/texture/zhao_texture_palette_res.sv
    BEGIN/WRITE/END, generations, accept-time verdict, one-clock lu_valid_o,
    no lu_ready_i
fpga/rtl/texture/zhao_texture_fragrob.sv
    slot/gen return validation; arr_q/req_q completion; 3-clock issue FSM;
    tmu_rready_o = 1; free-list + order ring; lost-update idiom
fpga/rtl/texture/zhao_texture_tmu_pipe.sv
    per-record bytesel and cac_data_i byte select (the law §9.1 restores);
    workload table (terrain/sky/stars sample counts)
reference/include/zref/zref_material.hpp        lerp8 (§4.2), unit_mul, mul2x
reference/include/zref/zref_sky.hpp:202         rgb565::to_rgb888 replication
reference/include/zref/zref_texture.hpp         Binding.lod U4.4; plan(); level_offset_texels
reference/include/zref/zref_fixp.hpp            unit8 convention, unit_mul law
reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt   (extended here)
reports/zhaozhou-texture-island-rearchitecture-v2-2026-09-05.txt  §0,2,8,9,10,16,App B
reports/G1D-INGRESS-CAPTURE-REPAIR-20260905.md  capture law; slot-keyed response law
reports/islandrearchitecture5.md                §3.3/3.4 budgets+tripwires; §15 combine laws
```

END
