# P0-C — Architecture for replacing FRAGROB's ownership machinery with `zhao_texture_v3own`

2026-09-07. ARCHITECTURE ONLY — no RTL, test or build file is modified by this
document. Deliverable of the P0-C architect pass.

Sources this stands on, all of today unless noted:
`ZHAOZHOU_HARDWARE_REARCHITECTURE_PRIORITIES_2026-09-07.txt` §6 (P0-C) and §9
(P0-E); `reports/ZHAOZHOU_TEXTURE_V3_1_REARCHITECTURE_2026-09-07.txt` (the
event schedule is its §8.1; the deletion-by-ownership inventory its §17.14);
`fpga/rtl/texture/OWNER-DIRECTION-T2-LIFETIME-2026-09-07.md`;
`reports/P0C-V3OWN-NOT-IN-THE-ISLAND-20260907.md`;
`reports/P0C-NOT-A-SWAP-20260907.md`;
`reports/P0E-THE-GLUE-POOL-MEASURED-20260907.md`;
`reports/G1D-COMPOSED-ISLAND-20260905.md` §4.3f;
`reports/V31-T2-OWNER-FIT-20260907.md`; `reports/DOCKET.md` M1, M3, M4, M5,
M6; `reports/synthesis/worst_path_index.json`; and the RTL itself —
`zhao_texture_island_top.sv` (2,035 lines), `zhao_texture_fragrob.sv` (866),
`zhao_texture_v3own.sv` (2,082), `zhao_texture_rsp_dispatch.sv`,
`zhao_texture_v3bank.sv`, `zhao_texture_v3rq.sv` — read for this document, at
the working tree of 2026-09-07 evening.

Every number below is marked MEASURED (a fit/map/count on disk, with its
source) or ESTIMATED (arithmetic on declared widths, or judgement). Where two
same-day sources disagree, the disagreement is stated, not resolved by
preference.

---

## 0. The one-paragraph decision

`zhao_texture_v3own` is integrated by ADMITTING AT THE ISLAND BOUNDARY and
making its 14-bit owner handle the island's single identity namespace, end to
end. The island's current end-to-end credit (`live_r`, `OWNER_DEPTH = FCTXN =
64`, island_top:494-521), its admission token (`tok_r`), its sequence stamp
(`fseq_m`/`seq_alloc_r`) and its output reorder buffer
(`rob_m`/`rob_tag_m`/`rob_full_m`/`seq_head_r`, island_top:1981-2034) are all
partial implementations of exactly the lifetime v3own owns properly — the
credit's own comment block cites the owner recovery brief as its origin. They
are deleted, not wrapped. FRAGROB is removed whole; its one function v3own
does NOT provide — expanding a fragment into per-sample TMU requests — moves
into a small new EXPANDER block that holds no results, no ordering and no
lifetime. The datapath blocks (tmu_plan, cache_pipe, rsp_dispatch, bilerp,
palette_res, aux_pipe, combine_v2, rcp, perspuv, mosaic) are retained; the
V3.1 report's §17.5 explicitly retains the response/dispatch services and
places the owner control plane downstream of them.

This is the brief's §6.2 instruction ("use the existing V3 event structure,
not a third design") applied with `P0C-NOT-A-SWAP-20260907.md`'s finding taken
seriously: fragrob and v3own share 4 port names of 60 and 48, so there is no
instantiation-shaped change available. What follows is the seam, the deletion
ledger, the staged plan, the invariants, and the risks — in that order.

---

> **PARENT VERIFICATION NOTE (added by the reviewing session, 2026-09-07).**
> The architecture below was checked, not accepted. Two results:
>
> * **Its central risk is CONFIRMED.** `SRCW = 16` and the pad term
>   `SRCW-2-$clog2(DEPTH)-2-GENW` evaluates to **exactly 0**
>   (`island_top:966-968`), so widening the slot 4 → 6 really does force
>   `SRCW` 16 → 18 and really does ripple into `cache_pipe`. That is a
>   consequence the document found on its own and it is real.
> * **Its line citations were off by 3-9 lines and are corrected here.**
>   `rob_m` is at 1981 not 1990, `rob_tag_m` 1983 not 1992, `fseq_m` 670 not
>   663, `fctx_m` 591 not 588. `island_top.sv` has not been edited since
>   2026-09-06, so the file did not move under it. Small, but this repository's
>   reports are meant to be checkable by following the citation, and a citation
>   that lands on a comment three lines away is a citation a reader stops
>   trusting.
>
> The evidence classing (every ledger row marked MEASURED or ESTIMATED, and rows
> split where the absence is measured but the saving is not) and the acceptance
> framing (§2.3: "SIGN UNKNOWN ... anyone who requires P0-C to cut composed ALM
> on its own should not approve this plan") are stronger than what was asked for
> and are endorsed as written.

## 1. The seam, with real ports

### 1.1 Identity: three namespaces become one

Today the island runs THREE identity namespaces, translated by lookup tables:

| namespace | width | where it lives |
|---|---|---|
| ingress token `fc` | 6 b (`FCTXW`) | `tok_r` stamped at admission; RCP `TOKW=8` carries it; PERSPUV `tag_i({8'd0, rcp_tok})` carries it; indexes the twenty top-level tables |
| FRAGROB slot+gen | 4+8 b (`DEPTH=16`, `GENW=8`) | fragrob's `alloc_slot_o`; keys `class_m`, `palslot_m`, `palgen_m`, `sampmeta_m` |
| routing token `SRCW` | 16 b = {class 2, slot 4, sidx 2, gen 8} | `plan_src_id` (island_top:965-968); echoed by cache (`smp_src_id_o`) and routed on by rsp_dispatch |

Under the integration there is ONE: v3own's handles (v3own.sv:22-28,
V3.1 §6.1 lines 601-612):

    owner  handle = {slot[5:0], generation[7:0]}             = 14 b (OWNERW)
    sample handle = {slot[5:0], sample_index[1:0], gen[7:0]} = 16 b (SMPW)

The routing token becomes `{class[1:0], sample_handle[15:0]}` = **18 bits**.
Today's `SRCW=16` is exactly full at slot width 4 — the pad term in
`plan_src_id` is `SRCW-2-$clog2(DEPTH)-2-GENW = 0` — so slot 6 forces the
widening; there is no slack to absorb it. This ripples through THREE files:

* `zhao_texture_tmu_plan` — `SRCW` is already a parameter (island_top:1058);
* `zhao_texture_rsp_dispatch` — `TOKW` is already a parameter (default 16);
* `zhao_texture_cache_pipe` — the HARD ONE: the island connects
  `.acc_src_id_i(plan_acc_src[15:0])` (island_top:1197) and declares
  `logic [15:0] cache_smp_src` (island_top:1179); whether the leaf's port is
  parameterised was not verified for this document. NAMED MEASUREMENT: grep
  `zhao_texture_cache_pipe.sv` for its src-id port declaration before Stage B
  is planned in detail. If it is a literal 16, the leaf needs a width
  parameter — mechanical, but it touches a retained service and therefore
  belongs in its own commit with its own leaf test run.

> **MEASUREMENT TAKEN, 2026-09-08 — it is a LITERAL 16, in FOUR places.**
> The conditional above fires. `zhao_texture_cache_pipe.sv` has a parameter list
> (`LANES`, `LINES`, `LINE_BYTES`, `REQN`) but **no width parameter for the
> source id**, and the width is hard-coded at:
>
> | site | line | declaration |
> |---|---|---|
> | input port | 102 | `input  var logic [15:0] acc_src_id_i` |
> | output port | 108 | `output var logic [15:0] smp_src_id_o` |
> | request queue | 206 | `logic [15:0] rq_src [REQN]` |
> | response queue | 305 | `logic [15:0] rs_src [REQN]` |
>
> So Stage B's prerequisite is confirmed and sized: add an `SRCW` parameter
> (default 16, so every existing instantiation is bit-identical) and thread it
> through those four sites. It touches a RETAINED service, so per this document's
> own rule it is its own commit with its own leaf test run — and it must land
> BEFORE anything depends on an 18-bit token, not alongside it.

REJECTED ALTERNATIVE, recorded so nobody re-proposes it: keep the transport
token at 16 bits by giving the expander a private slot namespace and a
translation table back to owner handles at the return seam. Rejected because
it recreates a second identity domain with its own generation discipline —
the exact duplicated-ownership structure P0-C exists to remove — and because
truncating or omitting the generation in transport is forbidden twice over
(brief §6.5 "do not truncate generations"; V3.1 lines 610-612 "External
clients continue carrying all 14 identity bits").

### 1.2 Admission (island boundary → v3own)

Today (island_top:494-551): `frag_ready_o = rcp_v_ready && credit_available`;
`tok_r` increments per accepted fragment; twelve attribute tables are written
at `fc_wp = tok_r[5:0]`; `fseq_m[fc_wp] <= seq_alloc_r`.

After: the same handshake edge drives v3own's admission port:

    adm_valid_i  = frag_valid_i && rcp_v_ready        (both-sides gating kept)
    frag_ready_o = rcp_v_ready && adm_ready_o
    adm_ctx_i    = frag_ctx_i                         (verbatim, untouched)
    adm_req_i    = {frag_aux_i, req2, req1, req0}     (from frag_sample_count_i)
    owner        = adm_owner_o                        (slot = owner[13:8])

The required mask is FROZEN at admission (V3.1 §9.1), which is exactly the
island's existing capture discipline — `frag_sample_count_i` and `frag_aux_i`
are already captured at this edge into `fsc_m`/`faux_m`. v3own allocates in
sequence order (V3.1 §6.1: cursor A, ring), so `adm_owner_o[13:8]` walks
0..63 exactly as `tok_r[5:0]` does today. The RCP token (`TOKW=8`) carries
the owner slot with two bits to spare; PERSPUV's 16-bit tag carries the full
14-bit owner handle with two to spare — today it carries `{8'd0, rcp_tok}`,
so this is a payload change on existing ports, not a port change.

DELETED BY THIS SEAM: `tok_r`, `live_r`, `live_peak_r`, `seq_alloc_r`,
`fseq_m[64]`, `fctx_m[64]` (context now enters v3own's OWNER_CONTEXT bank and
returns at `out_ctx_o`; v3own already stores the full 64 bits — its CTXW
parameter comment: "the FULL 64-bit opaque context, returned at the output").
`cnt_live_peak_o` is re-driven from `ev_live_peak_o` (CNTW=7, zero-extended);
the composed test asserts `cnt_live_peak_o == 64` and v3own's `ev_live_peak_o`
counts the same quantity by the same law (admission to ordered-output
acceptance), so the assertion carries over unchanged.

### 1.2b THE AUX SHEET TOKEN — A SECOND WIDENING, FOUND DURING STAGE C (2026-09-08)

**§1.1 above is incomplete and this is the correction.** Its widening analysis
follows the SRCW routing token through `tmu_plan`, `cache_pipe` and
`rsp_dispatch`, and correctly identifies `cache_pipe` as the hard one. It does
not mention the AUX path, which carries a SEPARATE token and needs the same
widening for the same reason.

    AUX_TOKW = $clog2(DEPTH) + GENW = 4 + 8 = 12      (island_top:92)
    v3own    OWNERW = SLOTW + GENW  = 6 + 8 = 14

**And it is HARDER than the SRCW case, not easier.** `cache_pipe`'s `SRCW` could
be given a parameter defaulting to 16, so every existing instantiation stayed
bit-identical and the prerequisite landed alone without touching anything's
behaviour. `AUX_TOKW` leaves the island on `sheet_tok_o` and returns on
`sheet_rtok_i` (island_top:149,161) — **it is a top-level port**, so its width is
the composed top's contract with whatever drives it. There is nothing to default
it away with.

Consequences for the plan:

* the AUX widening is its own prerequisite, like `cache_pipe`'s, and belongs
  BEFORE (c3) rather than inside it;
* it changes an island BOUNDARY, so the oracle's own port list and the composed
  test's stimulus both see it — unlike every change so far, this one cannot be
  invisible to the paired run;
* `zhao_texture_aux_pipe`'s `TOKW` parameter already exists
  (island_top:1898 instantiates it with `.TOKW(AUX_TOKW)`), so the leaf may be
  parameterised already.

**MEASUREMENT TAKEN, same session — and it resolves the EASY way**, which is the
opposite of `cache_pipe`. `zhao_texture_aux_pipe.sv` is parameterised
throughout: `TOKW` at :127 and every token path declared `[TOKW-1:0]` —
`req_tok_i` (:141), `sheet_tok_o` (:148), `sheet_rtok_i` (:152), `out_tok_o`
(:157), and the internal `a0_tok_q` (:240), `sd_tok` (:280), `off_tok` (:358).
No literal widths anywhere in the token path.

So the AUX widening needs **no change to the aux_pipe leaf at all** — only
`AUX_TOKW`'s definition in the island and the boundary port it feeds. That is a
one-line parameter change plus its consequences at the island's edge, against
`cache_pipe`'s four hard-coded sites.

**The contrast is worth keeping.** Two token widenings, both discovered by
following the same question; one needed a new parameter threaded through four
literal declarations, the other needed nothing but a different number. Guessing
which was which from the outside would have been a coin flip — `cache_pipe` HAS
a parameter list and still hard-coded the width; `aux_pipe` parameterised even
its internal queues.

**How it was found is the point.** Stage C's (c2) wiring referenced a signal that
did not exist (`fr_aux_rslot_c`); correcting it to the real `fr_aux_rslot`
exposed that the real signal is 4 bits where v3own wants 6. A plausible-looking
tie would have compiled the intent away and left this to surface as an
elaboration failure in Stage C's fit.


**ARCHITECT REVIEW, 2026-09-08 — §1.2b CONFIRMED, with two additions.** The
placement as its own prerequisite BEFORE (c3) is correct, and the measurement
resolving the aux_pipe leaf to no-change stands on its quoted line numbers.
Two consequences to carry into Stage C's bookkeeping:

1. The boundary width change adds four virtual pins (+2 `sheet_tok_o`, +2
   `sheet_rtok_i`). Interface growth is exactly the comparability caveat the
   brief's §1.1 raised for a THREE-bit growth on the fresh island; the Stage C
   fit report must name the pin delta beside the resource numbers rather than
   let it pass silently.
2. "Cannot be invisible to the paired run" is right and BENIGN, and should be
   said so the harness author does not build a translator. The sheet responder
   is an ECHO — `sheet_rtok_i` returns whatever `sheet_tok_o` presented,
   opaquely — so the paired harness echoes 12 bits to the oracle and 14 to the
   new top with no semantic mapping. A width translator would wire the harness
   into the identity namespace, which is the pattern this document deletes
   from the island; do not rebuild it in the testbench.

### 1.3 Attribute and descriptor tables: re-keyed, not yet re-homed

The remaining per-fragment tables (`uvw_m`, `fbase_m`, `fbind_m`, `flod_m`,
`fcls_m`, `fpsl_m`, `fpgn_m`, `frec_m`, `fwt_m`, `fsc_m`, `faux_m`) stay in
the island top at FIRST integration, indexed by owner slot instead of `fc` —
a 1:1 re-key, since both spaces are 64 deep and both walk in admission order.
This is deliberate, per brief §6.5: "At first integration, preserve the old
descriptor/format choices to isolate the ownership change." Their migration
into declared MATERIAL / SAMPLE_DESC planes (V3.1 §15.4, §17.14) is the P0-E
lane and a LATER stage; doing it in the same fit as the ownership change
would produce one unattributable number.

One table CHANGES SIZE: `sampmeta_m` is `[DEPTH][3]` = 16x3 rows of 21 bits
(1,008 bits, fabric — MEASURED absent from the map report's altsyncram
inventory) because it is keyed by FRAGROB slot. Keyed by owner slot it
becomes 64x3 = 4,032 fabric bits. That is a real, honest cost of the first
integration (+ ~3,024 flops, ESTIMATED from widths) and it is TEMPORARY by
declared plan: V3.1 §15.4 assigns this data to SAMPLE_METADATA 256x40 in
M10K. It cannot be converted in the same stage, because its read is
same-cycle in the response path (`clut_meta_c`, `bil_meta`, `near_meta` —
island_top:1274, 1455, 1601) and a synchronous read needs the one-cycle
R0-capture stage of §8.1 to hide in. Stage D below does exactly that.
`class_m`, `palslot_m`, `palgen_m` re-key the same way (16 → 64 entries;
+ ~576 flops ESTIMATED).

### 1.4 The expander — the one NEW block, and why it is not fragrob reborn

v3own has NO request side: its only issue-facing ports are notifications
(`iss_tmu_valid_i`/`iss_tmu_handle_i`, `iss_aux_valid_i`/`iss_aux_owner_i` —
"ISSUED is its own moment", v3own.sv:35-39). Fragrob currently owns the
fragment-to-sample expansion: per-slot `iss_q`/`auxiss_q` walk, `desc_u_m`/
`desc_v_m`/`desc_met_m [3][DEPTH]` descriptor copies, and the `tmu_valid_o`
request port. That function must live somewhere.

THE EXPANDER accepts one joined fragment (PERSPUV's `pu_valid`/`pu_u`/`pu_v`
plus the owner handle from its tag, plus the attribute-table reads that
island_top:735-780 already does today), and:

* issues up to three TMU sample requests into `u_plan.req_*` — same ports
  the island drives today (`req_u_i`, `req_v_i`, `req_lod_i`,
  `req_src_id_i`), with `req_src_id_i = {class, owner_slot, sidx, owner_gen}`;
* issues the AUX request into `u_aux.req_*` when required;
* pulses `iss_tmu_valid_i`/`iss_aux_valid_i` toward v3own as each request is
  ACCEPTED by the plan/aux pipe (the handshake edge, not the intent);
* writes the response-side tables (`sampmeta_m` etc.) at the same accepted
  edge, keyed by the identity the response will return under — preserving
  `check_ingress_capture.py`'s law exactly as today.

What it does NOT hold is the argument that it is not fragrob rebuilt: no
result banks (v3bank's), no arrival masks (v3own's issued/claimed/committed),
no ordering (v3own's cursors), no context (OWNER_CONTEXT), no generation
authority (T2's). Its state is the CURRENT fragment's expansion progress —
sample index counter, held coordinates, a skid for the plan handshake —
order tens of registers, plus whatever small per-fragment queue is needed to
decouple PERSPUV's output rate from the planner's acceptance rate. Today that
decoupling is fragrob's DEPTH=16 admission buffer; the expander needs a
bounded fragment queue of its own, and its depth is a measured decision for
the implementer (start at 4; the composed throughput requirement is one
fragment per clock sustained, §18.3's law, and the composed test will show
starvation). ESTIMATED cost: 200-600 ALM. Guessing is marked as guessing;
the leaf fit in Stage B prices it.

The expander does not re-derive per-sample bindings; the island's current law
(`f_binding_c + s` per sample, common u/v — island_top:869-881) is kept
verbatim at first integration, limitation and all.

### 1.5 The return seam (completion merger → v3own)

The completion merger (island_top:1630-1727) survives WITH ITS PRIORITY LAW
INTACT: palette first because PALETTE_RES has no `lu_ready_i` and its answer
lives exactly one clock; then nearest, unknown-class, bilinear. Today it
feeds `fr_tmu_rvalid/rgb/a/rslot/rsidx/rgen` and is safe only because
fragrob ties `tmu_rready_o` high. v3own gives the SAME guarantee by
construction — `assign tmu_rready_o = 1'b1;` and `aux_rready_o = 1'b1;`
(v3own.sv:558-559, with the capacity argument in its comment: banks are
written at most once per source per owner; READY queue depth 64 equals owner
capacity). So the merger re-targets with NO holding register and the
`err_rsp_dropped_o` tripwire moves to the new seam unchanged
(`pal_lu_valid_o && !tmu_rready_o` — now statically unreachable, which is
what a tripwire on an assumption is for).

The payload changes shape: today `{rgb24, a8}` + `{slot4, sidx2, gen8}`;
after, `tmu_rhandle_i = {slot6, sidx2, gen8}` (from the routing token's low
16) and `tmu_rresult_i = result40 = {STATUS8, alpha8, RGB888}` (v3own RESW
comment, Appendix B.1). STATUS8 at first integration encodes exactly the
merger's existing verdicts — OK, and the three counted error classes
(`pal_bad_c`, `!near_ok_c`, unknown-class) — with the error colour
`SMP_ERR_RGB` still placed in the RGB field so the retired pixel is
bit-identical to today's and the zref oracle comparison carries over without
a reference change. An error completion still satisfies its required bit
(v3own adversarial case 21 pins this; T2: it "must not strand its owner's
required bit").

### 1.6 AUX, and the one new capture it forces

Today fragrob stores the caller's context and replays `aux_ctx_o` so the top
can slice `wx = ctx[31:0]`, `wz = ctx[63:32]` into `u_aux` (island_top:
1845-1849). After integration the context lives inside v3own's OWNER_CONTEXT
bank, which has no mid-life read port — by design; only ordered output reads
it. So the island captures AUX GEOMETRY AT ADMISSION into its own table
keyed by owner slot: at first integration a 64x64 copy of the two context
words ({wx, wz}), later the typed AUX_GEOMETRY 64x80 plane V3.1 §15.4
declares. This is a DECLARED duplication with a plan, not an accident: the
brief (§10, P0-F) requires AUX geometry to become explicitly typed rather
than opaque context anyway, and this capture is where that type lands.
ESTIMATED cost at first integration: 4,096 bits, and it should be born as a
`zhao_texture_v3bank` instance (synchronous read at AUX issue — the expander
tolerates a one-cycle geometry read) so it costs an M10K, not flops.
The AUX return maps `{aux_out_tok → aux_rowner_i}` (owner handle, 14 b —
AUX_TOKW today is slot+gen and simply re-widens) with `aux_rresult_i` packing
`{status, degenerate?0:FF, tag, str, 8'd0}` exactly as the top builds
`fr_aux_rgb`/`fr_aux_a` today.

### 1.7 COMBINE and the final return

Today: fragrob retires → `u_combine.f_*` with material fields read from the
tables at `fr_o_tok`, `f_tag_i = {fseq_m[tok], ctx[15:0]}` (ROBTAGW = 22),
combiner output lands in the top's ROB by sequence, head walks in order.

After: v3own's COMBINE admission drives the same combiner —

    f_valid_i      = cmb_valid_o          f_ready_o → cmb_ready_i
    f_s0/s1        = cmb_s0_o/cmb_s1_o    (result40 → rgb24+a8 unpack)
    f_s2           = has_aux ? cmb_aux_o : cmb_s2_o     (today's mux, kept)
    f_recipe/weight/base/sample_count = MATERIAL tables at cmb_owner_o[13:8]
    f_tag_i        = cmb_owner_o          (TAGW: 22 → 14)

and the final return closes the loop: `fin_valid_i/fin_owner_i/fin_result_i`
from the combiner's `o_valid_o/o_tag_o/o_rgb_o+o_a_o+o_refused_o`, with
`fin_ready_o = 1'b1` (v3own.sv:560) preserving today's `comb_o_ready = 1'b1`
law — a completing fragment always has a reserved place, backpressure parks
in the owner, not in the pipe. `o_refused_o` rides in the result40 STATUS
byte and re-emerges at `out_refused_o`. COMBINE V2 itself is untouched except
the TAGW parameter; the brief's "keep COMBINE V2" stands.

`cmb_valid_o` presents owners in READY order, not admission order. Ordering
is restored at v3own's ordered output (cursor E, admission sequence), which
is the same boundary law as today's ROB — "reorder INSIDE, order at the
EDGE". The composed test's `out_of_order == 0` assertion is the proof either
way. NOTE A REAL BEHAVIOURAL DIFFERENCE, visible only in counters: today a
fragment BEHIND a stalled head cannot even reach the combiner (fragrob
retires in allocation order); after, it can combine early and wait as a
published FINAL. `cnt_reorder_held_o` must be re-derived (from an
emission-held event at v3own's output cursor) and its `> 0` assertion in
phase 3 still holds — more easily, in fact.


> **PARENT VERIFICATION, 2026-09-08.** All four load-bearing claims of the
> ruling below were checked against the sources, not accepted:
>
> * **COMBINE packet carries no context** — CONFIRMED. `zhao_texture_v3own.sv`
>   :191-198 is `cmb_valid_o`, `cmb_ready_i`, `cmb_owner_o`, `cmb_s0/s1/s2/aux_o`.
>   There is no context port on that interface.
> * **`out_ctx_o` is at ORDERED OUTPUT only** — CONFIRMED, :211, inside the
>   `out_*` group. So it arrives after the combiner needed the fields, which is
>   what makes option (i) structurally impossible rather than merely forbidden.
> * **`cmb_gen_ok_c` re-verifies the generation at combine admission** —
>   CONFIRMED, :1128: `win_gen_of_slot(cmb_owner_o[slot]) == cmb_owner_o[gen]`.
>   This is the load-bearing one for the ruling: slot-only keying of the material
>   plane is safe BECAUSE v3own itself checks the generation at the moment the
>   plane is read.
> * **The owner ruling forbidding context repacking** — CONFIRMED verbatim at
>   `zhao_texture_island_top.sv`:677-686, quoting recovery architecture v2 §2.3:
>   *"Packing recipe bits into that word is not a valid way to retain an
>   independently opaque context ... Do not silently overwrite caller-owned
>   bits."* An earlier version of the island did exactly this and it was ruled
>   out.
>
> The ruling stands as written. Its strongest argument is the second point:
> option (i) fails on structure before it fails on policy, and a rejection that
> holds for two independent reasons is worth more than one that holds for either.

### 1.7b THE MATERIAL PLANE — ruling on how recipe/weight/base/sample_count reach COMBINE (2026-09-08)

Stage C found what §1.7's one-line table entry glossed: the oracle presents a
fragment to `u_combine` by indexing top-level tables with the retiring token
(`fsc_m[fr_o_tok]`, `frec_m[fr_o_tok]`, `fwt_m[fr_o_tok]`,
`fbase_m[fr_o_tok]`), and v3own hands over a packet with NO token to index
anything with — `cmb_owner_o` plus four result40 lanes (v3own.sv:191-198),
nothing else. The material attributes must arrive another way. Two candidates
were raised; this is the ruling.

**REJECTED: (i) carry the material fields in OWNER_CONTEXT via `adm_ctx_i`.**
Twice over, and either ground alone suffices:

* **It is forbidden by a standing owner ruling.** The island's own capture
  comment (island_top:677-686) records that an earlier version wrote the
  recipe, weight, sample count and token into bits [34:16] of the caller's
  context word, and quotes the owner recovery architecture v2 §2.3 verdict:
  "Packing recipe bits into that word is not a valid way to retain an
  independently opaque context ... Do not silently overwrite caller-owned
  bits." All 64 context bits are the caller's — the AUX path consumes
  ctx[63:32]/[31:0] as wx/wz and the tag is ctx[15:0]; there is no free
  space, and island_top:593 ("ONE NAMED ARRAY PER FIELD, and deliberately not
  one packed word") is the same law from the other side.
* **It is structurally impossible without editing v3own, which is off
  limits.** The COMBINE admission packet carries no context (MEASURED port
  list, v3own.sv:191-198), and OWNER_CONTEXT has no mid-life read port —
  `out_ctx_o` is read only at ordered output (v3own.sv:211, 1024), which is
  AFTER combine. Even the variant of (i) that widens the instantiation's
  CTXW parameter to ride material bits beside the caller's word fails on
  this: the widened word still emerges only after the combiner needed it.

**SELECTED: (ii) — but named for what it is: the MATERIAL PLANE, not a new
side table.** The new top keeps ONE table, keyed by the owner SLOT
(`cmb_owner_o[13:8]`), holding
`{base_rgb24, base_a8, weight8, recipe3, sample_count2}` = 45 bits x 64 =
2,880 bits (ESTIMATED from widths; the brief's own §9.2 arithmetic — "base
RGBA32 + recipe3 + weight8 + sample_count2 is 45 bits before flags" — lands
on the same number). Written once at admission on the `adm_accept_o` edge;
read once at COMBINE admission. Folding `faux` in as a 46th bit is the
implementer's choice and saves the separate `faux_m` read at the `f_s2` mux.

Why this does not violate §0's delete-not-wrap decision: §0 deletes the
LIFETIME and ORDERING machinery — the structures that partially implement
v3own's job. A single-writer, single-reader attribute plane is DATAPATH
storage, and it is precisely what the brief prescribes for P0-E ("A bank with
one writer and one synchronous reader is promising", §9.5) and what V3.1
§15.4 declares as the MATERIAL plane. This is that plane's first form, not
old machinery wrapped. At Stage C it reads combinationally, matching the
oracle's own access pattern so behaviour is identical; Stage D moves it into
a v3bank/M10K with the R0-capture cycle absorbing the synchronous read.

**Why slot-only keying (no generation on the table read) is safe:** the table
is read only for an owner v3own itself presents as live, and v3own re-verifies
the presented handle's generation against the slot's own window AT combine
admission (`cmb_gen_ok_c`, v3own.sv:1128-1129 — MEASURED). Under T2, a slot
cannot be re-admitted until its owner's ordered output retires, so the entry
written at that owner's admission is still that owner's when combine reads
it. The generation check lives where it belongs — inside the owner — and the
plane stays 6-bit-indexed.

**Ledger effect: NONE — and stating that is the point.** The `fctx_m` row in
§2.1 stands unchanged (deleted; the caller's context moves into
OWNER_CONTEXT, already inside v3own's measured 17 M10K). The material fields
never lived in `fctx_m` — the island stores them in separate named arrays
BECAUSE of the ruling quoted above — so the MATERIAL plane is the §1.3
re-key of `fbase_m`/`frec_m`/`fwt_m`/`fsc_m` under one name, 64 entries
before and after. No §2.2 addition row is created.

**`fseq_m`: the coordinator's reading is CONFIRMED.** v3own's ordered output
(cursor E, admission sequence) IS the sequence; a separate sequence number
carried through the combiner tag is exactly the "partial implementation of
v3own's lifetime" §0 deletes. `f_tag_i` becomes `cmb_owner_o` (TAGW 22→14),
`fin_owner_i` echoes it back, and `out_tag_o = out_ctx_o[15:0]`. The §2.1
`fseq_m` deletion row stands.

**FALSIFIERS for this ruling:** (a) if any consumer needs a material field
BETWEEN admission and combine admission that the plane's one read cannot
serve, the one-writer/one-reader claim collapses and the plane needs a second
port — no such consumer exists in the seam as drawn (§1.4's expander reads
the DESCRIPTOR tables, not MATERIAL); (b) if a successor's material were ever
read for a stale owner, the per-recipe `cnt_combine_jobs_o` exact counts and
the paired-run colour identity both diverge — a wrong recipe is a wrong
job-count distribution, the same detection §1.7's ENFORCED-BY notes rely on
today; (c) if Stage C's elaboration finds a combine-side field this section
did not enumerate, the 45-bit width was wrong and the miss is published, not
absorbed.

> **IMPLEMENTATION CORRECTION, 2026-09-08: the plane is 46 bits, not 45.**
>
> This section enumerates {base_rgb24, base_a8, weight8, recipe3, sc2} = 45.
> Building it showed one field missing. `u_combine`'s S2 lane is a MUX, not a
> plain sample — `.f_s2_rgb_i(fr_o_has_aux ? fr_o_aux_rgb : fr_o_s_rgb[2])`,
> oracle island_top:2341-2342 — so the combiner must know whether this fragment
> used AUX. v3own's COMBINE packet does not carry it (`cmb_aux_o` is a RESULT
> lane, not a validity), and the fact is known at admission as `frag_aux_i`.
>
> One bit added. It is this ruling's OWN logic — per-fragment attributes written
> once at admission, read once at combine — applied to a field the enumeration
> missed, not a departure from it.
>
> **MATW = 46, 64 entries, 2,944 bits.** Recorded here rather than absorbed
> silently: a plane that quietly grew a bit is a plane whose width nobody can
> check against its specification, and the deletion ledger prices it by width.
>
> The six fields are extracted through named wires in the implementation
> (`mat_has_aux_c`, `mat_scount_c`, `mat_recipe_c`, `mat_weight_c`,
> `mat_base_a_c`, `mat_base_rgb_c`) so that no consumer re-derives a bit
> position — the failure mode that made the original AUX_TOKW defect possible.


### 1.8 Ordered output

`out_valid_o/out_owner_o/out_result_o/out_ctx_o` → island `out_*`:
`out_tag_o = out_ctx_o[15:0]`, `out_rgb_o/out_a_o/out_refused_o` unpacked
from `out_result_o`. DELETED: `rob_m` (33x64), `rob_tag_m` (16x64),
`rob_full_m` (64), `seq_head_r`, `rob_held_r` (re-derived), and the
head-of-line-stall law is PRESERVED — v3own's emission cursor stops on a
missing final exactly as `rob_full_m[seq_head_r]` does; a lost fragment is
still a loud stall, not a silent skip.

### 1.9 rsp_dispatch — the explicit answer

It STAYS, functionally untouched. The V3.1 report retains the
response/dispatch services by name (§17.5: "The prepared response pool, class
queues and palette-return credits retain their separate writers and
ownership. The new owner control plane is downstream of those services") and
its census row (334 self ALUTs / 1,025 self registers) is on no deletion
list. P0-C changes two things about it: `TOKW` 16 → 18, and the ENDPOINT of
its ~-2.0 ns path family. Today "dispatch queue token → FRAGROB result
storage" ends in fragrob's `tmu_ok_c`-gated bank write
(zhao_texture_fragrob.sv:626 — the very cone v3bank's write-enable contract
names as the defect being replaced). After, the token lands in the merger and
then v3own's R0 CAPTURE REGISTER (V3.1 §8.1: "T R0 capture"), and the bank
write enable is a one-hot flop formed in R2 — two registers upstream of any
RAM. The family, as a named (from → to) identity, must VANISH from the
composed path report. That is Stage C's falsifiable physical claim.

### 1.10 Evidence-port continuity

The composed test (119 checks) touches fragrob only through three island
port NAMES: `err_fragrob_wq_overflow_o`, `err_fragrob_id_error_o`,
`cnt_fragrob_id_errors_o`. The integration keeps all three names at the
island boundary with defined semantics:

* `cnt_fragrob_id_errors_o` := `ev_err_range_o + ev_err_stale_o +
  ev_err_unsol_o + ev_err_dup_o` (v3own's typed refusal counters — a
  refinement of fragrob's single count; the test asserts zero on clean runs,
  which holds iff all four are zero, so the assertion is strictly stronger);
* `err_fragrob_id_error_o` := sticky OR of the same four deltas;
* `err_fragrob_wq_overflow_o` := the EXPANDER's fragment-queue overflow
  tripwire (the nearest live equivalent of fragrob's `wq_overflow_o`) — not
  a constant 0, because a tied-off tripwire is decoration, which is the exact
  defect the island's own header history records.

`cnt_fragments_o` := `ev_admitted_o`. A rename pass (`fragrob` → `owner` in
port names) is deliberately NOT proposed now; renaming and restructuring in
one stage is two changes in one diff.

---

## 2. The deletion ledger

The warning this section answers (brief §6, quoted in
P0C-V3OWN-NOT-IN-THE-ISLAND): "we risk building a better owner alongside the
old expensive machinery and wondering why the composed area barely changes."
And the arithmetic that makes it sharp: v3own standalone is 3,348 ALM / 3,953
registers (MEASURED, V31-T2-OWNER-FIT); fragrob standalone is 1,676 ALM /
2,631 registers (MEASURED, block ledger). The swap alone ADDS ~1,672 ALM by
leaf numbers — with docket M4's caveat that leaf numbers mis-price composed
worth (a +159 leaf register cost landed as +14 composed), cutting BOTH ways.

Baseline for all rows: composed island `@p0b-island`, 13,615 ALM / 23,295
registers / 37 M10K / 41,528 bits / 17 DSP (MEASURED, G1D §4.3f). The fresh
fit's entity table attributes 8,554 dedicated-logic registers and ~4,790 self
ALMs to the top entity itself (MEASURED,
`reports/synthesis/blockpaths/zhao_texture_island_top@p0b-island.fit.rpt`,
entity row).

### 2.1 Deleted outright

| item | file:line | size | evidence class |
|---|---|---|---|
| `zhao_texture_fragrob` instance + file from island closure | island_top:888; fit_targets.yml:989 | 1,676 ALM / 2,631 reg standalone; 2,554 ALUT / 2,955 reg self in the OLD composed census | MEASURED (leaf fit; V3.1 §17.10 census — an older fit generation, noted) |
| `rob_m [64] x 33 b` | island_top:1981 | 2,112 bits; absent from the map's altsyncram inventory AND from its Info 276007 refusal list, so fabric flops | MEASURED absence; 2,112-register saving ESTIMATED (1:1 bit-to-flop mapping unconfirmed) |
| `rob_tag_m [64] x 16 b` | island_top:1983 | 1,024 bits — ALREADY an inferred altsyncram (map.rpt:471, Simple Dual Port 64x16) | MEASURED; deleting it saves RAM bits, nearly zero registers |
| `rob_full_m [64]`, `seq_head_r`, `rob_held_r` | island_top:1982,1984 | ~102 bits | ESTIMATED |
| `fseq_m [64] x 6 b`, `seq_alloc_r` | island_top:670-672 | 390 bits, fabric (absent from altsyncram inventory) | MEASURED absence; count ESTIMATED |
| `fctx_m [64] x 64 b` | island_top:591 | 4,096 bits — an inferred altsyncram in the fresh map | MEASURED; deletion saves RAM bits / an M10K share, not registers |
| `live_r`, `live_peak_r`, `tok_r` | island_top:495-521, 548 | ~22 flops | ESTIMATED |
| fragrob-only glue: `fr_*` nets, `plan_src_id` pad, aux slot/gen slicing | throughout | not separately sized | ESTIMATED small |

A note on a same-day source disagreement: the DOCKET sweep of 2026-09-07
("registering the reads is worth about −10,304 registers: uvw_m 4,096 +
fctx_m 4,096 + rob_m 2,112") counted `fctx_m` and `rob_tag_m` as flops. The
FRESH map report shows both already inferred as altsyncram — the six L0
conversions landed between the two measurements. This ledger follows the
fresh map report; the sweep's figure is stale for those two arrays and still
right about `uvw_m` and `rob_m`.

### 2.2 Added

| item | size | evidence class |
|---|---|---|
| `zhao_texture_v3own` (+ its v3bank/v3rq children) | 3,348 ALM / 3,953 reg / 17 M10K / 20,640 bits / 0 DSP standalone, 952 virtual pins | MEASURED (V31-T2-OWNER-FIT); composed cost will differ per M4, direction unknown |
| expander block | 200-600 ALM | ESTIMATED — priced by Stage B's leaf fit |
| `sampmeta_m` 16x3 → 64x3 | +3,024 fabric bits | ESTIMATED from widths |
| `class_m`/`palslot_m`/`palgen_m` 16 → 64 entries | +~576 fabric bits | ESTIMATED |
| AUX_GEOMETRY capture 64x64 (as a v3bank) | 4,096 bits, ~1 M10K | ESTIMATED |
| SRCW/TOKW 16 → 18 plumbing | +2 bits across plan/cache/dispatch queues | ESTIMATED small |

### 2.3 The honest sum

ALM: −1,676 (fragrob, leaf-priced) + 3,348 (v3own, leaf-priced) + expander +
adapters − whatever share of the top's ~4,790 self ALMs served the deleted
ordering/credit logic. SIGN UNKNOWN. This document does NOT predict the
composed ALM falls at Stage C, and anyone who requires P0-C to cut composed
ALM on its own should not approve this plan. What P0-C buys, on the evidence:

1. the ~-2 ns dispatch→result-storage path family's removal (named,
   checkable — brief §1.2 lists it as one of the two genuine internal
   problems);
2. the T2/§8.1 ownership correctness the brief argues for on its own ground —
   fragrob's acceptance predicate combines range, generation, liveness,
   required, issued and duplicate checks WITH the payload write
   (fragrob.sv:442-455 into :626), which is the defect brief §6.1 names and
   v3bank's write-enable contract exists to end;
3. the DELETIONS that only become legal after it: the ROB pool, the sequence
   machinery, `fctx_m`, and (via Stage D) the descriptor tables' move into
   declared planes. Registers: the top's 8,554-register self pool loses the
   ROB (~2,100 est.) and sequence state at Stage C, and the island loses
   fragrob's 2,955; against v3own's 3,953. Net register movement ESTIMATED
   between −1,500 and +500 depending on how the leaf/composed mis-pricing
   lands. The prediction is REGISTERED HERE so Stage C can falsify it.

The area RELIEF in this file's neighbourhood comes from the adjacent,
independent P0-E finding: `uvw_m`'s asynchronous read costs 4,096 flops
(MEASURED — map.rpt:7269 refusal, 64x64 declared), and registering that read
is available WITHOUT the ownership rework. It is Stage A precisely so its
number is never entangled with P0-C's.

---

## 3. The staged plan

Discipline binding every stage (DOCKET M1/M4/M5/M6): a leaf Fmax delta under
~5 MHz is seed noise; a leaf fit mis-prices area (+159 leaf vs +14 composed,
same RTL); a composed Fmax delta is attributable ONLY if the gating path
family — the (from → to) identity in
`reports/synthesis/worst_path_index.json` — is the same on both sides; 0 of 4
pairs currently on disk share a family. Therefore NO stage below accepts on
"Fmax improved". Structural claims carry the weight: named families present
or absent, ALM/register counts against registered predictions, netlist
contents. Run `tools/quartus/worst_path_index.py` after every fit so the
family identity is recorded, not reconstructed.

Also binding: the live-tree trap. Every stage that edits island sources must
not run while an island fit is executing (fit_targets.yml:986-1002 names the
15-file closure), and each stage writes its in-progress state to the run's
TASK_LOG before reading any fit result.

### Stage A — register `uvw_m`'s read (P0-E item, sequenced first)

* CHANGE: one file, `zhao_texture_island_top.sv` — register the
  `uvw_m[rcp_tok]` read (island_top:669) with a skid on the RCP→PERSPUV
  handshake (the read is combinational today because PERSPUV's input
  handshake is combinational off RCP's output; the skid is the cost of the
  cycle). Nothing else.
* TEST: `island_composed_directed` — all 119 checks, bit-exact colours,
  identical counter values.
* FIT: island refit. ACCEPT: map report no longer lists `uvw_m` under Info
  276007; fitted registers drop materially (prediction registered: −3,000 to
  −4,200 of the 4,096).
* FALSIFIER: `uvw_m` still refused, or register drop < 3,000, or any check
  changes. Fmax: record the family; the current gate is
  `rcp24_svc|c_pend[7] → perspuv|e_num_v` (M5, −2.690) and this change sits
  INSIDE that seam, so if the family changes identity, report movement with
  cause unproven — do not claim it.
* INTERACTION, declared: P0-B2's registered RCP completion boundary (M5's
  fix) touches the same seam. WHOEVER GOES FIRST, the other rebaselines.
  They must not share a fit.

### Stage B — the expander as a leaf

* CHANGE: new file (suggested `zhao_texture_frag_expand.sv`) + leaf test.
  No island change; no v3own change.
* TEST: replay a RECORDED workload's request stream (captured from
  `island_composed_directed`'s phases): the expander must reproduce
  fragrob's accepted request sequence bit-identically modulo the declared
  identity re-map (slot4+pad → slot6) and the SRCW-18 layout, and its
  `iss_*` pulses must count exactly the accepted requests.
* FIT: leaf fit for STRUCTURE (what the netlist contains) and a first price;
  per M4 the price is provisional.
* FALSIFIER: any request divergence; any iss pulse without an accepted
  request — assert exact per-fragment issue counts, not just output
  equality (the counters-see-what-pictures-cannot law: byte-identical output
  has twice hidden a machine doing double work).

### Stage C — the composed V3 island variant, as a NEW top

* CHANGE: new file `zhao_texture_island_v3_top.sv` composing: rcp, perspuv,
  mosaic, expander, tmu_plan, cache_pipe, rsp_dispatch, bilerp, palette_res,
  aux_pipe, combine_v2, v3own — per §1 above. New fit_targets entry with its
  own rules block (same 7,500 / 9,000 / 64 / 14 gates). THE OLD
  `zhao_texture_island_top` AND ITS TARGET ARE UNTOUCHED — it is the oracle,
  exactly as brief §6.5 instructs ("First keep the current island behavior as
  an end-to-end oracle"). `zhao_texture_v3own.sv` IS NOT EDITED — the
  541-check adversarial suite reads internal probes (`c3t_we_q`, `cmt_q`, …)
  and must keep passing on the unmodified file; every adapter lives in the
  new top or the expander.
* TESTS, three independent gates:
  1. `texture_v3own_adversarial` — 541 checks, untouched, green (proves the
     integration changed nothing about the owner);
  2. `island_v3_composed_directed` — the 119-check suite retargeted to the
     new top: same stimulus bytes, same zref oracle, same assertions
     including `out_of_order == 0`, `cnt_live_peak_o == 64`, per-recipe
     `cnt_combine_jobs_o`, all error latches, and the three continuity ports
     of §1.10;
  3. PAIRED RUN: both tops driven with identical stimulus; the retired
     streams (rgb, a, tag, refused, ORDER) byte-identical. Counter identity
     is NOT required where semantics legitimately refine (documented list:
     `cnt_reorder_held_o`, `cnt_fragrob_id_errors_o` decomposition); colour
     and order identity IS required, everywhere.
* FIT: one composed fit of the new top. ACCEPT on, in order:
  1. the dispatch-token → result-storage family ABSENT from the new setup
     report (grep the path list for rsp_dispatch launch nodes capturing in
     any result bank; today's island identities are recorded in
     `worst_path_index.json`);
  2. M10K within the 64 gate (prediction registered: 37 − deleted-table RAM
     + 17 + 1 AUX bank ≈ 50-55; the device has 553, the GATE is 64; V3.1's
     69-M10K feature-live profile is a later, explicit budget-revision
     request and NOT this stage's to borrow);
  3. ALM/register against §2.3's registered prediction band — a miss is a
     FINDING to publish, not a silent re-scope;
  4. gating family recorded on both sides; Fmax reported as movement unless
     the family matches.
* FALSIFIER: the dispatch family still present; or paired-run divergence; or
  the 541 suite red; or M10K over 64. ANY of these stops the cutover.
* HONESTY CLAUSE: this stage is irreducibly multi-variable — a first
  instantiation plus its licensed deletions cannot be split into
  one-variable fits, because the intermediate "v3own beside fragrob"
  composition is exactly the both-machineries state the brief forbids
  building. The mitigation is not decomposition but PREDICTION: every §2 row
  is checked against the new map/fit reports by name (the V3.1 §17.14
  pattern), so the one number decomposes on paper even though it was
  measured once. And §17.14's own acceptance check applies verbatim: "A new
  owner ring sitting underneath fseq_m plus an old independent reorder
  buffer has not completed the rearchitecture" — the new top must contain
  NO fseq, NO rob_m, NO second lifetime.
* REQUIRED DETECTORS (brief §6.6), carried into the retargeted suite or the
  paired harness where not already present: duplicate replies at every
  forwarding distance, stale tokens before and after slot reuse,
  simultaneous TMU/AUX, all owners ready with COMBINE stalled, FINAL before
  actual COMBINE acceptance, long output stalls at full owner capacity —
  most exist in the 541 suite at the leaf; the composed variant needs at
  least the stall/backpressure and error-completion subset end to end.

### Stage D — descriptor planes and metadata into RAM (P0-E convergence)

After C stands: move `sampmeta_m` (as SAMPLE_METADATA 256x40), the MATERIAL
fields, and SAMPLE_DESC into v3bank instances, ONE family per stage-D fit,
using the R0 capture cycle to absorb the synchronous read. Each conversion:
paired-run bit-exactness + map-report inference check + its own fit. This is
where the remaining fabric tables (fbase_m 2,048 b, fbind_m/fwt_m 512 b
each, sampmeta 4,032 b post-C — all MEASURED absent from the altsyncram
inventory) stop being flops. Detailed sequencing belongs to the P0-E lane
owner; it is named here only so Stage C is not blamed for not doing it.

### Stage E — cutover

Retire the old island target (archive the row, keep the report files), point
CI at the new top, run `tools/quartus/check_prod_manifest.py` (ledger,
manifest and source list are three different acts — CLAUDE.md), regenerate
`zhao_prod_top.sv` if any production-registered port changed, and only then
delete `zhao_texture_fragrob.sv` from the tree. Deleting the file before the
oracle is retired would kill the paired-run gate.

---

## 4. What must not change

1. **The T2 identity law**, verbatim
   (OWNER-DIRECTION-T2-LIFETIME-2026-09-07.md:10-18): "An owner instance has
   authority from its accepted admission until its ordered external output
   transfer. Once that transfer retires the owner, later events for that
   instance are stale. Matching a slot's residual generation bits does not
   extend the owner's authority after retirement. A stale event must not
   change that owner's scoreboard, create ready work, authorize COMBINE,
   publish a result, release a credit, or resurrect the owner. The receiver
   may consume and discard a bad transport packet so that transport can
   drain. Consuming a packet is not accepting its claimed ownership." —
   applied separately to TMU C4, AUX C4, FINAL C4, and the COMBINE input
   handshake. The integration touches NONE of the logic this rules; it stays
   in the untouched v3own file.
2. **The event order** (brief §6.2 lanes; V3.1 §8.1 stage schedule
   R0→R1→R2→W→P→Q): bank write enables are register outputs formed before
   the write edge; a source is not readable until publication; lanes are
   typed (a TMU index cannot select AUX storage); a reserved FINAL requires
   actual COMBINE acceptance. The adapters must not add or remove a stage
   without recomputing the forwarding distance (brief §6.4: "Do not keep
   FWD_WINDOW=1 by habit") — at first integration no stage is added between
   merger and R0, so the distance stands; any later pipelining re-opens it.
3. **The 541 checks** of `texture_v3own_adversarial.cpp` on the UNMODIFIED
   v3own — including the internal write-enable and publish-ordering probes
   (cases 4i and 24, reading `c3t_we_q`/`cmt_q` via verilator-public paths).
4. **The 119 checks** of `island_composed_directed.cpp` in retargeted form:
   bit-exact colour against zref (`zref::Tmu::sample` +
   `zref::material::combine`), strict output order, exact per-recipe job
   counts, the error latches, and the three fragrob-named boundary ports per
   §1.10.
5. **All material arithmetic**: `decode16` and the frozen 5→8 / 6→8
   replication expansions, the CLUT4 nibble law, SMP_ERR magenta at full
   alpha, COMBINE V2's recipes and saturation laws, RCP's exact
   Newton/truncation contract (brief §5.4), PERSPUV's registered output
   seam. None of these is on the P0-C path; a colour diff in the paired run
   is a stop, not a tolerance.
6. **The palette can't-wait law** and its tripwire (§1.5): strict merger
   priority, unconditional acceptance at the return port,
   `err_rsp_dropped_o` retargeted, never deleted.
7. **The capacity-domain separation** (V3.1 §17.2): "A free local RCP
   context is not a free global owner." RCP's NCTX=8 credit and v3own's
   64-owner credit remain separately named counters; no adapter may conflate
   them.

---

## 5. Risks, stated against the evidence

**NCTX=8.** The island's `NCTX=8` is RCP's micro-job context count
(island_top:537), not an owner count; v3own's 64 owners pair with the
island's existing FCTXN=64 exactly. Two real interactions, neither blocking:
(a) admission-at-boundary means an owner slot is held during the ~12-clock
RCP/PERSPUV transit — but today's `live_r` credit ALREADY reserves at the
boundary, so effective capacity is unchanged; worth one sentence in the
Stage C report. (b) V3.1 §16.5 records sixteen contexts as RCP's throughput
knee for the OLD pipeline — if P0-B changes RCP's topology, that knee is
re-measured on its own lane, not P0-C's. FALSIFIER for (a): the sustained
one-per-clock law (adversarial case 20) exercised at the composed level with
`ev_live_peak_o` observed.

**M10K.** 37 (island) + 17 (v3own) + ~1 (AUX geometry) − the deleted
inferred tables' blocks (`fctx_m`, `rob_tag_m` and share) ≈ 50-55 against
the 64 gate, 553 physical (ESTIMATED; the fit decides). Two honest caveats:
v3own's 17 M10K include seven blocks holding 1,056 bits in shallow queues —
P1-A's port-geometry question, deliberately NOT solved here (brief §8: "Do
not spend owner credits twice") — and V3.1's feature-live 69-M10K profile
means the 64 gate is expected to be REVISED for P0-F scope; Stage C must
pass 64 without borrowing that future revision.

**rsp_dispatch** stays (§1.9). Its 1,025 self registers are NOT in the
deletion ledger, and any P0-C accounting that quietly counts them is wrong.
The path-family claim (its token cone now ends in a capture register, not a
RAM-enable cone) is the falsifiable part.

**The SRCW widening** touches a retained service (cache_pipe) whose port
width discipline is unverified (§1.1's named measurement). If the leaf
hardcodes 16, that is a small leaf change plus its leaf test — schedule it
before Stage B freezes the expander's token layout.

**The composed number may go UP at Stage C.** §2.3's arithmetic allows it;
the plan's acceptance is deliberately not "ALM falls". The risk is political
rather than technical — a rising composed number the day after a 41%
owner-leaf win invites exactly the "wondering why the composed area barely
changes" the brief predicts. The answer is written in advance: the §2 ledger
names what was bought (correctness structure, a named path family, licensed
deletions) and what remains (P0-E's planes, P1-A's queues, P0-B2's
completion boundary — the CURRENT gating family, which P0-C does not claim
to move).

**Behavioural refinements visible in counters** (§1.7): early combining of
out-of-order-ready fragments changes internal timing observably. The
composed suite's assertion inventory was checked against this plan (all 85
static check sites reviewed): colour/order/latch assertions are unaffected;
the two counter families needing re-derivation are named in §1.7 and §1.10.
Any OTHER counter divergence in the paired run is a defect, not a
refinement.

**Fmax expectations.** After Stage C the expected gating family is still
`rcp24_svc|c_pend → perspuv|e_num_*` (M5's completion scan, −2.690), which
is P0-B2's to remove, not P0-C's. If Stage C's fit reports a different
family, that is recorded per M6 and NOT claimed by this work. Any stage
whose acceptance were "Fmax improves" would be wrong on today's evidence —
0 of 4 fit pairs on disk share a gating family — so no stage here has one.

---

## 6. Summary for the implementer

Build order: A (uvw_m read, one fit) → B (expander leaf) → C (new composed
top, one fit, paired oracle, deletion checklist) → D (planes to RAM, one
family per fit) → E (cutover). v3own's file is never edited. fragrob's file
is deleted only at E. Every fit's worst-path family goes through
`worst_path_index.py`. Every prediction in §2 and §3 is written down before
its fit is read. The composed area is allowed to disappoint; the parity
gates and the named path family are not.
