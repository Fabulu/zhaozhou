# Texture owner residency: register attribution, the read-late COMBINE boundary, and a verdict on deleting the ready-claimed table

2026-09-09, RUN-20260909-1842-texture-owner-residency-architecture.
Stage-1 campaign architecture for the texture island rearchitecture.
Sources: `@g2-prod` fit report (per-entity + Fitter RAM Summary), the current
RTL on `zixxtrixx-v8-closeout` (clean tree for `fpga/rtl/texture` and
`fpga/rtl/raster`), the two owner roadmaps of 2026-09-09, and the three
diagnosis reports this follows. No fit was run; no RTL was changed.

---

## 1. The island's 16,285 registers, attributed BY LINE

The fit's own per-entity register column is the anchor; every named structure
below reconciles against it. Declared-bit sums come from reading the
declarations, not from a RAM summary that only lists what escaped to memory.
Two instrument notes, both hard-won this session: a name-followed-by-bracket
grep MISSES `name [i]` writes (the fourth-correction law — my first LHS scan
lost every nested-index write such as `cq_d[cls][cq_wp[cls]] <= ...`), and a
declared-bit sum is an UPPER bound: Quartus sweeps unread lanes and merges
duplicates, so fitted counts can sit well under declared counts (rsp_dispatch
is the worked example below).

### 1.1 `zhao_raster_perspuv_svc` — 3,240 registers (19.9%, the single largest)

The registers are the **per-token operand/result planes**, held in flops
because every completion writes its own row while the drain scans:

| structure | file:line | shape | flop bits |
|---|---|---|---:|
| `e_num_u`, `e_num_v` | `zhao_raster_perspuv_svc.sv:178-179` | 2 x 16x32 | 1,024 |
| `e_mant_u`, `e_mant_v` | `:180-181` | 2 x 16x24 | 768 |
| `e_k` | `:189` | 16x6 | 96 |
| `e_have`/`e_val`/`e_sat`/`e_dz` | `:151-152,192-193` | 16x2 + 3x16x1 | 80 |
| work queues `wq` + pointers | `:228-229` | 2x16x4 + 4x5 | 148 |
| pair pipeline `p0_*..p3_*` | `:248-264` | mixed | ~544 |
| outputs `u_q`,`v_q`,`tag_q` | `:373-374` | 32+32+16 | 80 |

`e_tag` (16x16) is the ONE plane that inferred RAM (`@g2-prod` RAM summary).
The token planes alone are ~1,968 flops — **61% of this block**. This block is
the pairpipe swap's territory; the roadmap states the paired pipe is already
integrated in the main lane and its delta must not be credited twice, so this
report names the structure and claims nothing for it.

### 1.2 `zhao_texture_v3own` — 3,018 registers (18.5%; 2,841 own + 173 in `v3rq` pointers)

| group | file:line | flop bits |
|---|---|---:|
| per-owner scoreboard: `live/req/iss/clm/cmt/rdy/cbi/crs/fcl/fdn/ftc_q` | `zhao_texture_v3own.sv:235-263` | **1,472** |
| TMU return pipeline `c0t/c1t/c3t/c4t` | `:562-595` | ~222 (40 of it became a 3-deep shift-tap RAM: `c0t_res_q`, per RAM summary) |
| AUX return pipeline `c0a..c4a` | `:601-622` | ~205 |
| FINAL return pipeline `c0f..c4f` | `:625-645` | ~192 |
| ticket write regs `q0t/q0a/q0i_*` | `:780-781` | 45 |
| arbiter + combine/output read stages `rr_q, k0..k2, g0..g2, cmb_res_q, out_res_q, addr regs` | `:864-912, 1007-1011` | ~160 |
| plane-read captures `sres_cap/ares_cap/fres_cap/ctx_cap` | `:976-979` | 264 |
| queue pointers `cq_*/oq_*` | `:990,1017` | 12 |
| window/fence/credit control `tail/emit/fetch, live/unf/peak, sh_*_gen, fn_*, ctxw_*` | `:265-266,322,415,483-485,515-517` | ~207 |
| evidence counters (11 x 32, on ports) | `:214-224` | 352 |

Sum ≈ 3,090 declared, ~2,850 after the shift-tap and sim-only exclusions —
reconciles with the fitted 2,841 to within 3%. The payload/context/ready
STORAGE is already M10K (`v3bank` x6, `v3rq` x3); the registers are the
**status scoreboard, three validation pipelines, and the counters**.

### 1.3 `zhao_texture_cache_pipe` — 2,945 registers (18.1%)

| structure | file:line | flop bits |
|---|---|---:|
| **per-lane tag arrays `tag_r`** | `zhao_texture_cache_pipe.sv:209` (inside `g_lane`, `:204-211`) | **1,536** (4 x 16x24) |
| request FIFO `rq_addr` | `:225` | 512 (4 x 128) |
| C1/C2 stage records (tags, captured tags/data, idx, src) | `:250-275` | ~430 |
| response-hold `rs_data` | `:324` | 256 |
| `valid_r` + fill engine `fb_*` + pointers | `:216,346-350` | ~150 |

Only the per-lane `data_r` became M10K; `rq_src`/`rs_src` became small RAMs.
The single biggest register structure in the cache is the **tag store held in
fabric** — a T3 (cache) lever, named here for the attribution and left to that
packet.

### 1.4 `zhao_texture_rsp_dispatch` — 1,321 registers (8.1%)

Declared: class queues `cq_d` 16x64=1,024 + `cq_t` 16x18=288 + `cq_m`
16x40=640 (`META_EN=1` in the island) at `zhao_texture_rsp_dispatch.sv:217-218,255`,
raw FIFO `raw_d/raw_t/raw_c` 336 at `:189-191`, counters 96, pointers ~50 —
about 2,430 declared against 1,321 fitted. `raw_m` alone inferred RAM. The
fitted number sitting ~1,100 under declared is Quartus sweeping/merging unread
lanes (the error-class queue's outputs and part of the metadata lanes); the
structures to change are still the four-copy class payload queues.

### 1.5 The remainder

`aux_pipe` 1,226 (result-record queue `rq_tok/tag/str/deg` ~400 flops at
`zhao_texture_aux_pipe.sv:398-401`, divider pipeline + credits; its offer-queue
payloads are already RAM), `rcp24_svc` 990, `tmu_plan` 972, combiner 749 (its
context payload is already RAM — see section 5), `frag_expand` 578,
`palette_res` 427, `bilerp_lane` 173, island glue 435 (`px_uvw_q`, `r1_d_q`,
`mat_addr_q` — `mat_m` and `uvw_m` are already RAM per the fit's own summary).

**Verified: the near-equal-thirds claim holds on the current tree.** perspuv /
v3own / cache_pipe = 3,240 / 3,018 / 2,945 = 56.5% of the island, and the
structures above are pointable by line. The register breach is systemic; the
owner-residency campaign addresses the v3own third and the combine transport,
and CANNOT by itself close a 16,285-vs-9,000 breach. perspuv's third belongs
to the pairpipe swap, cache/dispatch's to T3.

---

## 2. The read-late COMBINE boundary

### 2.1 What exists today (measured, not assumed)

The island ALREADY has most of the residency: sample/AUX/final/context payloads
live in `v3bank` M10Ks with single writers; the material descriptor lives in
`mat_m` (64x46, RAM, written at admission, registered read —
`zhao_texture_island_v3_top.sv:2757-2797`); the combiner's per-context payload
(`payload_m` 8x110), tag, scratch and completion stores are ALL altsyncram
already. What still CIRCULATES is the assembled copy chain:

    v3bank read -> sres_cap/ares_cap regs (v3own:976-977, 160 flops)
      -> cq_s0..cq_ax MLAB queues (v3own:986-989, 640 bits)
      -> 160-bit cmb_s0/s1/s2/aux bus + island aux/s2 mux (island:3304-3312)
      -> combiner f_* port (~171 bits) -> payload_m immutable copy (RAM)

### 2.2 The replacement boundary — exact signals

Adopting the roadmap 4.2 names at measured widths (OWNERW=14, SLOTW=6,
descriptor=46, RESW=40, NCTX=8 -> ctx=3):

    cmb_job      { valid, ready, owner_handle[13:0] }                  16 wires
    material_rd  { valid, ready, slot[5:0], ctx[2:0] }                 11
    material_rsp { valid, ctx[2:0], desc[45:0] }                       50
      desc = {base_rgb[23:0], base_a[7:0], weight[7:0],
              recipe[2:0], sample_count[1:0], has_aux}   (mat_m layout, island:2812-2817)
    source_rd    { valid, ready, slot[5:0], source_index[1:0],
                   ctx[2:0], phase }                                   14
    source_rsp   { valid, ctx[2:0], phase, rgba[31:0], status[7:0] }   45
    cmb_final    { valid, ready, owner[13:0], rgba[31:0], refused }    unchanged
    ordered out  — unchanged (out_* : 14+40+64)

Job acceptance (`cmb_job` fire) reserves a real combiner context and is the
event that sets combine-issued (`cbi`); the ready-queue pop remains only a
reservation (`crs`) — the two stay distinct exactly as `v3own:1179-1193`
distinguishes them today. The aux-as-third-sample mux
(`mat_has_aux_c ? aux : s2`, island:3311-3312) moves BEHIND the boundary: the
combiner's phase scheduler selects the aux plane or the s2 plane as the
source_rd target, using the descriptor's `has_aux` bit it already read.

### 2.3 What stops circulating / what stays in flops

STOPS: the 160-bit sample bus and its hold; `sres_cap_q`/`ares_cap_q`
(160 flops); the four 40-bit MLAB queues `cq_s0..cq_ax` (640 bits — `cq_own_q`
survives as the token-only job queue); the combiner's 96-bit sample section +
base/weight of `payload_m` (RAM bits, NOT registers — see section 5); the
island's 32-bit aux mux.

STAYS IN FLOPS: ready-queue write registers (`q0*` — the single-writer law),
arbiter `rr_q`, reservation counters `cmb_res_q`/`out_res_q`, queue pointers,
the combiner's Q/R/D/O/M/F stage records and its phase accumulator path
(scratch storage is already RAM; the in-flight partial is pipeline registers
and must remain so — "RAM is not an excuse to move a one-cycle accumulator
behind a multi-cycle read", roadmap 8.2).

### 2.4 Ports, latency, and the invariants to assert

Port pressure does NOT grow: each sample plane keeps exactly one reader — the
reader moves from v3own's k0..k2 prefetch pipeline into the combiner's phase
engine. The combiner already tolerates one-cycle-late RAM data (its `d_*`
stage reads `payload_m` per phase today, `material_combine_v2.sv:326`), so
pointing phase reads at the owner planes is latency-shaped like the present
design; phase 1 reads s0+s1 (different planes, parallel), phase 2 reads
s2-or-aux plus the descriptor's base. The job-acceptance path keeps the
island's existing one-cycle material alignment (`mat_aligned_c`).

Invariants, per the roadmap's gate and the metadata-swap law:

* **publication-before-ready** — a source is readable only after its payload
  commit edge; structurally C3-write -> C4-publish today; assert it at the
  boundary.
* **release-after-last-reader** — already the island's law (`v3own:1200`:
  output release is THE only ordinary owner-free event; final write,
  prefetch and combine completion do not free). The read-late combiner reads
  strictly before `fin` and `fin` before `out`, so storage cannot be reused
  under it; assert, don't assume.
* **one enable captures data + identity** — every `source_rsp` capture takes
  {rgba, status, ctx, phase} on one accepted enable, never data on one enable
  and identity on another (the metadata-bank defect class).

---

## 3. Small stores, each with one clear writer

The scoreboard replacement (roadmap 8.2, checked against the RTL's actual
event structure). "Writer" is a registered single enable, `v3rq`-style
(`V3-WREN-REG` discipline, `v3own:806-814`).

| # | store | shape | single writer | readers / replicas | why the replicas |
|---|---|---|---|---|---|
| 1 | required mask + valid/epoch | 64x13, **2 copies** | admission | TMU C4 eligibility; AUX C4 eligibility | the two publication lanes present DIFFERENT owners on the same cycle; zero-work needs no read (writer decodes `adm_req_i==0` in fabric) |
| 2 | TMU issued epoch | 256x9 ({slot,sample}, 192 legal rows; never concatenate to 192 — the vertex-arena stride law) | TMU issue event | TMU return validator (C0/C1) | one reader; TDP gives write+read same cycle |
| 3 | TMU claimed epoch | 256x9 + recent-claim forward regs | validated TMU claim (C2) | TMU dup validator | one reader; the forwarding CAM covers snapshot-to-visibility (section 4.3) |
| 4/5 | AUX issued / claimed epoch | 64x9 each | AUX issue / AUX claim | AUX return validator | AUX events coincide with TMU events — own planes, never shared ports |
| 6 | sample payloads s0/s1/s2 | 64x40 x3 — **exists** (`v3bank g_sres`) | C3 TMU commit, per-bank one-hot registered we | combiner phase engine (moves from k-pipeline) | single reader preserved |
| 7 | AUX payload | 64x40 — **exists** (`u_ares`) | C3 AUX commit | combiner phase engine | single reader |
| 8 | committed-epoch mirrors | four 64x9 planes (s0,s1,s2,aux) x **2 read copies** = 8 small RAMs | each plane: its own source's C4 publication (TMU lane writes at most one sample plane/cycle; AUX lane writes the aux plane) | TMU C4 union query reads ALL FOUR at its owner; AUX C4 union query likewise, different owner, same cycle | this is `cmt_q`'s replacement — the union across sources needs all four planes at one address, twice concurrently |
| 9 | final claimed / done epochs | two 64x9 | C2F claim / C4F publish | final return validator | replaces `fcl_q`/`fdn_q` |
| 10 | combine-accepted epoch | 64x9 | actual combine acceptance (`cmb_fire`) | final validator (replaces `cbi_q` read at C1F) | one reader |
| 11 | final result + status/epoch | 64x49 — extends existing `u_fres` | validated final commit | ordered output prefetch | one reader |
| 12 | owner context | 64x64 — **exists** (`u_ctx`) | admission | ordered output | one reader |
| 13 | material descriptor | 64x46 — **exists** (`mat_m`, island) | admission | combiner context reserve (+ per-phase if base read late) | one reader today; 2nd copy ONLY if job-accept and phase reads ever overlap — measure before replicating |
| 14 | ready tickets | three narrow queues — **exist** (`v3rq` x3) | q0t / q0a / q0i registered enables | fair arbiter | the roadmap's own first-replacement rule: keep all three; three same-edge producers cannot share one write port |

Epoch planes carry "not issued/claimed THIS generation" without admission
scrubbing every plane; boot and generation-wrap get the deterministic validity
scrub the roadmap requires. Retained as fabric: window cursors and counters,
credit, fence machine, admission window, all pipeline stage records, and — for
the first candidate — `live_q` (64b, pending packet T1's window predicate) and
the interlocks of section 4.4.

M10K arithmetic: ~15 new small blocks (2+1+1+1+1+8+2+1) at often-poor
occupancy, on top of the island's 49 — lands ~64, at the section-21.6 M10K
line and inside the roadmap's proposed 80-block texture allocation. M10K is
the abundant resource (553, ~147 used); this is the intended trade. The exact
list must reconcile in the packet's map, not here.

---

## 4. The ready-claimed elimination: a verdict

The proposal: for a frozen nonzero required mask, exactly one publication
transitions incomplete->complete; process same-owner simultaneous TMU+AUX in a
documented order; only the completing event makes a ticket; therefore the
`rdy_q` table can go.

### 4.1 The theorem is sound — and the current RTL already PROVES it in the flop domain

Read from `v3own`:

* C2 acceptance of a publication requires that source-bit CLEAR in both `clm`
  and `cmt`, with the C3 forwarding record covering the one-cycle snapshot
  window (`:656-712`). So every accepted publication adds at least one NEW
  required bit — duplicates are structurally rejected (`ev_err_dup`).
* `req_q` is written at admission ONLY (`:1204-1218`): the mask is frozen.
* `cmt` is monotone within a generation, so the union covering `req` happens
  on exactly one accepted event.
* Same-edge TMU+AUX for one owner is the ONLY true simultaneity, and it is
  already coalesced with a documented winner: `same_owner_c` folds each
  event's mask into the other's union and `tkt_a_c = a_elig_c &&
  !same_owner_c` suppresses the AUX ticket (`:751-781`) — Appendix D.2's
  READY_TMU rule, in production today.
* Adjacent-cycle events are safe because C4 reads `cmt_q` as a SAME-CYCLE
  flop: an event at cycle n+1 sees cycle n's commit.

Consequence, stated plainly: **the `!rdy_q[...]` term in `t_elig_c`/`a_elig_c`
is already unreachable by legal stimulus.** For it to decide, a second
completing publication would have to pass C2 for an owner whose required bits
are all committed — every such return is rejected as dup or unsol first. The
same holds for `!crs_q` at eligibility (a reserved owner's ticket has been
consumed; no further completing event exists). `rdy_q` and `crs_q` are
defensive interlocks, not load-bearing state. By the committed-mutant law,
that classification itself needs a demonstration: the guard cannot be fired
with legal stimulus, so its removal evidence is a `tests/mutants/` specimen
that breaks C2 dup-rejection and shows the interlock (and its counter)
catching the double ticket.

### 4.2 But the argument does NOT transfer on the existing forwarding — and this is the owner's actual question

The EXISTING forwarding is one record per pipe (C3, `FWD_WINDOW=1`,
`:650-668`) and it covers exactly one thing: CLAIM visibility for C2 dup
rejection. Commit visibility needs no forwarding today because C4 reads flops.

Move `cmt` into committed-epoch mirrors and the publication decision reads a
row that is stale by the mirror's write-to-read-visibility latency L (address
reg + RAM output reg + any staging: L = 2-3). Then:

* Two publications for the same owner **within L cycles** — not merely on the
  same edge — each read a pre-peer "before" state. The later one computes a
  union missing the earlier commit, decides "incomplete", and emits NO
  ticket. The earlier one was genuinely incomplete. **No ticket is ever
  created: the owner never combines, and the island deadlocks in
  allocation-order retirement** — the same deadlock class as the dispatch
  head_room defect. This is the elimination's failure mode, and it is worse
  than the duplicate it was guarding against.
* The dual error — forwarding a LATER cycle's commit backwards into an
  earlier event's before-state — manufactures two "last" events and a double
  ticket (roadmap 8.4 premise 5). With `rdy_q`/`crs_q` deleted, the
  interlock that would have absorbed exactly this bug is gone.

So `same_owner_c` (same-edge coincidence) must generalize to a
**same-owner-within-L coincidence window**: a small ordered batch/forwarding
structure over the last L publication events per lane, cross-pipe (TMU sees
AUX's recent commits and vice versa), full-handle matched, never slot-only,
whose union feeds the eligibility computation — the commit-side sibling of the
C3 claim record. The RAM-collision rule (roadmap 8.4 premise 4) also binds: a
same-cycle same-address read/write on a currently-publishing bit must ignore
the undefined read and reconstruct the update explicitly — legal precisely
because publication is once-only, so the before-value is known.

The supplied 167-case model proves the finite transition theorem only — its
own documentation (rearchitecture roadmap :901) concedes it simulates no
memory latency, no forwarding, no duplicates, no reset, no zero-work
admission, no backpressure. The premises are NOT yet demonstrated for RTL.

### 4.3 The claim-side window also moves

Dup rejection itself gets slower eyes: the claimed-epoch plane's
snapshot-to-claim-visibility distance grows from 1 cycle to 1+L. `FWD_WINDOW`
is derived, not chosen (`:650-656`) — the roadmap's instruction to write the
cycle table (snapshot -> validation -> claim -> physical write -> publication
-> ready) and re-derive the window is exactly right, and the existing
"consecutive duplicates that both captured an old snapshot" test must be
re-run at the new depth.

### 4.4 Verdict

**Endorse the once-only ticket ARGUMENT; refuse the table deletion as a
day-one act.** Concretely:

1. The theorem is correct, and the flop RTL already embodies it — the
   documented-order coalescing exists and works (`same_owner_c`).
2. The elimination is sound ONLY together with a new commit-visibility
   forwarding/batch structure spanning the mirror latency, cross-pipe, plus
   the re-derived claim window. "The existing forwarding" does not carry it;
   anyone implementing on that assumption ships a lost-ticket deadlock.
3. Savings honesty: `rdy_q`+`crs_q` are 128 flops and four narrow 64:1 muxes.
   The elimination is architecturally pleasing and materially small. The big
   state (`req/iss/clm/cmt`, 1,024 flops and the wide muxes) moves to planes
   REGARDLESS of whether readiness stays a flag or becomes an event
   computation.
4. Therefore adopt the roadmap's own fallback as the plan of record: first
   candidate keeps a small ready-flag array beside the epoch planes; the
   deletion lands only after (a) the cycle table exists, (b) an adversarial
   same-owner dual-publication kernel sweeps event offsets 0..L (both
   orders), (c) duplicate-terminal exclusion and zero-work admission are
   re-proven composed, and (d) a committed mutant demonstrates the
   double-ticket detector actually fires (a detector reading zero is a claim,
   and it is the claim to check hardest).

---

## 5. Honest accounting: genuinely new vs already banked

**Already memory-backed TODAY — claiming these again is double-counting:**

* v3own payload/context/ready storage: six `v3bank` M10Ks + three `v3rq`
  queues (the fit lists all nine), the `cq_*`/`oq_*` MLAB queues (1,056
  bits), and the `c0t_res_q` shift-tap.
* island: `mat_m` (64x46), `uvw_m` (Stage A's -4,092 was banked when its read
  was registered), `early_desc` slices.
* combiner: `payload_m`/`tag_m`/`scratch_m`/`comp_m` and its four context
  queues — the "880-register saving" from deleting the f_* capture does not
  exist; the capture is RAM. Narrowing `payload_m` 110 -> ~62 bits saves RAM
  bits and ZERO registers.
* cache `data_r` (4 M10K), `rq_src`/`rs_src`; dispatch `raw_m`; aux_pipe
  offer-queue payloads; perspuv `e_tag`.

**Genuinely NEW, this campaign (registers, declared-bit basis — fitted results
must be earned by the packet's own map/fit, per roadmap 8.6):**

* scoreboard -> epoch planes: 1,472 flops down to ~130-200 retained hot bits
  (`live_q` until T1, interlocks per section 4.4) => **~1,250-1,340 flops
  removed**, MINUS new plane-read stage registers (address/valid/forward
  records, ~150-250 back). Net **~1,000-1,150 registers** in v3own, and — the
  actual ALM prize — the eleven 64:1 muxes and the 64-row update recurrences
  leave the fabric. No ALM number is claimed: predict what moves, not how far.
* read-late COMBINE: capture regs 160 + queue MLABs 640 bits + the 160-bit
  bus/mux fabric => **~200-300 registers** and modest ALM. This is a topology
  correctness/cleanliness move, not a register lever.
* validation pipelines (~620 flops of c0..c4 records) largely REMAIN — they
  are the "active pipeline records stay in registers" class. T1's window
  predicate and phase encoding trims some (its own packet, its own gate).

**Scale check, stated coldly:** a fully successful owner-residency campaign
touches ~1,200-1,450 of the island's 16,285 registers (7-9%) and one to two
thousand of its ALMs. It is the right stage-1: it removes the 81%-of-ALM-
overage block's mechanism and fixes the boundary the roadmap calls the main
topology change. It does NOT close the register criterion (16,285 vs 9,000):
that breach is systemic — perspuv's 3,240 falls to the already-credited
pairpipe swap, cache's 1,536-flop tag store and dispatch's class-queue
payloads are T3, and after ALL of those land the budget question the systemic
report raised (model vs implementation) is still an owner decision.

**DSP/M10K:** unchanged by this campaign except +~15 small M10Ks (to ~64 of
553; the roadmap's own texture allocation is 80). The DSP redline stays owned
by the rcp24_v3 swap, already priced elsewhere.

---

## 6. What this report does not do

No RTL changed, no fit run, no test touched; the composed suites and
`gate3_paired.py` are untouched by construction. The register groups are
declared-bit attributions reconciled to the fit's entity totals (±3% on
v3own/cache_pipe; rsp_dispatch documented as sweep-dominated), not per-flop
fitter attributions. The ALM consequences of sections 3 and 5 are mechanisms
with direction, not numbers — the packet that implements them buys its numbers
with its own controlled map, exactly as roadmap 8.6 demands.
