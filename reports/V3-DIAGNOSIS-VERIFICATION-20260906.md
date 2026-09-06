# V3 diagnosis verification — section 2 of TEXTURE-ISLAND-V3-ARCHITECTURE, checked against HEAD

**Date:** 2026-09-06
**Document under check:** `reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt`, section 2
(lines 262–478) plus Appendix A (line 3850).
**Document's pin:** `d884ce01c2d02453e935c6bf33bfe85a5c4c7a1e`, branch `zixxtrixx-v8-closeout`.
**HEAD at check:** `b32a6524` — *"File the V3 texture-island architecture beside the thing it governs"*.
**Scope:** read-only. No RTL, spec, design YAML or test was modified. This file is the only write.

---

## 0. The premise this verification was launched on is wrong, and that matters most

The tasking said: *"THE TREE HAS CHANGED SINCE d884ce01 … several of the things it
diagnoses have been repaired."* It has not.

```
$ git log --oneline d884ce01..HEAD
b32a6524 File the V3 texture-island architecture beside the thing it governs, ...
```

**One commit separates the pin from HEAD, and it is the commit that added the document
itself.** Every file Appendix A cites is byte-identical to the pinned tree
(`git diff --stat d884ce01 HEAD -- <file>` is empty for all eleven):

| Appendix A source | file | lines @HEAD | diff vs d884ce01 |
|---|---|---|---|
| R04 | `fpga/rtl/texture/zhao_texture_island_top.sv` | 1971 | none |
| R05 | `fpga/rtl/texture/zhao_texture_fragrob.sv` | 866 | none |
| R06 | `fpga/rtl/texture/zhao_texture_material_combine_v2.sv` | 755 | none |
| R07 | `fpga/rtl/texture/zhao_texture_bilerp_lane.sv` | 179 | none |
| R09 | `fpga/rtl/raster/zhao_raster_perspuv_svc.sv` | 534 | none |
| R10 | `fpga/rtl/raster/zhao_raster_rcp24_svc.sv` | 310 | none |
| R11 | `fpga/rtl/texture/zhao_texture_cache_pipe.sv` | 684 | none |
| R12 | `fpga/rtl/texture/zhao_texture_rsp_dispatch.sv` | 296 | none |
| R13 | `fpga/rtl/texture/zhao_texture_tmu_plan.sv` | 479 | none |
| R14 | `fpga/rtl/texture/zhao_texture_palette_res.sv` | 292 | none |
| R16 | `fpga/rtl/texture/zhao_texture_aux_pipe.sv` | 617 | none |

### Every "recent fix" named in the tasking is *inside* the document's baseline

| Claimed post-pin fix | Where it actually landed | Relation to the pin | Document's own text |
|---|---|---|---|
| `near_ok_c = 1'b0` repaired; shared `decode16(h, fmt)` / `fmt_is_direct()` | `a1846867` "W9b: the nearest station decodes, alpha is real" | **9 commits BEFORE** `d884ce01` | — |
| `chan8()` no longer hardwired 5:6:5 | `a1846867` | before pin | — |
| `sampmeta_m` widened 17 → 20 bits | `a1846867` (earlier work at `bc40b70d`) | before pin | §2.4: *"The current **20-bit** sample metadata word"* |
| Bilerp sequencer 3 → 4 phases | `a1846867` | before pin | §2.14: *"live code uses **four**"* |
| COMBINE V1 → V2 in the island top | before pin | before pin | §2.1: *"eight **COMBINE V2** execution contexts"* |
| Cache reservation repair | before pin | before pin | §2.12, verbatim |

Confirmed present at HEAD: `zhao_texture_island_top.sv:392` `decode16(h, fmt)`,
`:425` `fmt_is_direct(fmt)`, `:1301` `chan8(h, fmt, c)` — three arguments,
format-controlled; `:1549` `wire near_ok_c = fmt_is_direct(near_fmt);`;
`:1005` `logic [19:0] sampmeta_m [DEPTH][3]`; `:1316`/`:1325` phase sequence 0,1,2,3;
`:1892` `zhao_texture_material_combine_v2 #(.NCTX(8), ...)`.

**Consequence for the V3 lanes.** There is no "stale assumption about the old nearest
station" anywhere in the document to correct, because the document was written against the
repaired tree and describes the repaired behaviour. The reverse error is the live one: an
implementing lane told "the tree has moved on since the doc" may go looking for repairs
that are not there and may treat §2's findings as historical. **They are current.**

Appendix A anticipated exactly this and its provenance discipline held: it records the pin,
the tree hash `e8f77a75…`, and states *"Later branch changes are not silently incorporated."*
That claim is verified.

---

## 1. Finding-by-finding verdicts

Verdict counts: **13 CONFIRMED**, **1 PARTLY-CONFIRMED (2.14 — three sub-claims confirmed,
one confirmed harder than stated, one nuanced)**, **0 ALREADY-FIXED**, **0 NOT-REPRODUCED**.

| § | Document's claim (abbreviated) | Verdict | Evidence at HEAD | Changes what V3 must preserve? |
|---|---|---|---|---|
| **2.1** | *"64-owner end-to-end credit, 64-entry ingress attribute tables … 16-slot FRAGROB, eight COMBINE V2 execution contexts, and a separate 64-entry final reorder buffer."* | **CONFIRMED** — counted, not read | `island_top.sv:563-564` `FCTXN=64`, `FCTXW=6`; `:494` `OWNER_DEPTH = FCTXN`; `:90` `DEPTH=16`; `:1892` `.NCTX(8)`; `:1917-1919` `rob_m`/`rob_full_m`/`rob_tag_m [FCTXN]`. Thirteen `[FCTXN]` ingress arrays at `:571-573`, `:629-637`, `:651`. | No change. The owner-credit repair (decision A) is real and must survive. |
| **2.2** | *"several consumers still use asynchronous indexed reads from those tables"* | **CONFIRMED** | `:707` `wire [63:0] uvw_rd = uvw_m[rcp_tok[FCTXW-1:0]]`; `:731-733` `fctx_m`/`fbase_m`/`fbind_m[fc_rp]`; `:1895-1906` six material fields read `[fr_o_tok]` combinationally into COMBINE; `:1935-1938` the output head reads `rob_m`/`rob_tag_m[seq_head_r]` in continuous assigns. | No change. |
| **2.3** | FRAGROB payload partly banked; six-part combinational validation *"immediately drives the result-bank write"*; FSM is `IDLE/READ/HOLD`; work storage takes 3 entries then serializes | **CONFIRMED, all four parts** | Banks `fragrob.sv:228-236` (`desc_u_m`/`desc_v_m`/`desc_met_m`/`res_rgb_m`/`res_a_m [3][DEPTH]`). Predicate `:444-449`: slot-valid ∧ generation ∧ `tmu_sidx_ok_c` ∧ `req_q[..]` ∧ `iss_q[..]` ∧ `!arr_q[..]` — six terms. It gates the bank write directly at `:627-628`. FSM `:375` `{I_IDLE, I_READ, I_HOLD}`; a second copy `:546` `{R_IDLE, R_READ, R_HOLD}`. Allocation pushes up to 3 at `:746`; `wq_m[WQN]`, `WQN=64` at `:299-301`. | **Yes — hard preserve.** The six terms plus the issued-bit clear (`:821`) are the anti-stale / anti-duplicate / anti-unsolicited law. See §3.1 on the *invisible* tripwires. |
| **2.4** | *"20-bit sample metadata … Bilinear, nearest, and palette consumers independently index it after dispatch. Palette slot and generation … from additional FRAGROB-slot tables."* | **CONFIRMED — exactly three independent readers** | `:1005` declaration; one write port `:1016`; three async read sites, each with a *different* index: `:1241` `disp_bil_tok`, `:1407` `disp_clut_tok`, `:1533` `disp_near_tok`. Palette side-tables `:917-918`, read async at `:1404-1405`. | No change. |
| **2.5** | *"The planner nevertheless receives `bind_base_i` and `bind_mode_i` directly from the top-level live pins … The composed test's drained-mode phases are an explicit workaround"* | **CONFIRMED — mechanism confirmed, with one framing correction** | `island_top.sv:1031` `.req_base_i(bind_base_i), .req_mode_i(bind_mode_i)`. `tmu_plan.sv` has **no** per-fragment binding port. FRAGROB *does* emit a binding (`fragrob.sv:390` `tmu_binding_o`) and the top *does* build per-sample variants (`island_top.sv:863` `fr_f_binding[s] = f_binding_c + BINDW'(s)`), and that value reaches **nothing on the request path**. | **Yes.** See §3.3. |
| **2.6** | *"the top … still routes by the class carried in the source token. That is redundant authority"* | **CONFIRMED** | Routing: `:947` `plan_src_id = {class_m[fr_tmu_slot], …}`. Derived: `:1088-1091` `plan_class_derived_c = class_of(plan_acc_fmt, plan_acc_filter)`, compared and counted into `err_class_mismatch_o`. The RTL's own comment `:1077` names the full repair as *"section 5A.7's resolved sample descriptor"* — i.e. decision F. | No change; V3's decision F is the file's own stated plan. But see §4 item 7: the RTL comment about this being *live* is now stale. |
| **2.7** | *"palette lookup produces a pulse without response-ready … its safety depends on FRAGROB being unconditionally ready … one incomplete owner can stop strict final retirement forever."* | **CONFIRMED, verbatim** | `palette_res.sv` has no `lu_ready_i` port (`:80-85`); `island_top.sv:1383` `assign disp_clut_ready = 1'b1;  // the palette lookup is unconditional`; `fragrob.sv:456` `assign tmu_rready_o = 1'b1;`; strict priority chain `island_top.sv:1626-1629`. The RTL states the identical hazard at `:1597-1615`. | **Yes — and there is more to preserve than the document says.** See §3.4 note on `err_rsp_dropped_o`. |
| **2.8** | *"the sample failure is not carried into FRAGROB as an independent typed status … Material math can alter the magenta"* | **CONFIRMED** | `:444-445` `SMP_ERR_RGB = 24'hFF00FF`, `SMP_ERR_A = 8'hFF`; injected as colour only, at `:1696` and `:1776`. FRAGROB has no per-sample status input — its only error ports are `id_errors_o`/`id_error_o` (`fragrob.sv:163-165`), which are *fragment*-level, not sample-typed. | No change. |
| **2.9** | *"allocates a token table, maintains one queue per axis, duplicates reciprocal mantissas … writes separate U and V result tables, then joins them at a head"* | **CONFIRMED, all five** | `perspuv_svc.sv:228` `logic [TW-1:0] wq [2][NTOK]` — literally two queues; `:180-181` `e_mant_u`/`e_mant_v`; `:178-179` `e_num_u`/`e_num_v`; `:190-191` `e_q_u`/`e_q_v`; context table `:151-194`. **Two** product sites only (`:476`, inside a 2-iteration `ax` loop), so decision H's *"does not reduce the lane count"* is correct — the lanes are already exactly 2. | No change. |
| **2.10** | *"scans for a free context, scans for a ready micro-job, and scans for a completed context … a 32-by-64-bit multiply expression. `c_w` is stored as 64 bits, although every MW write is a 24-by-32-bit product shifted right by 24."* | **CONFIRMED, counted** | Three scans: `rcp24_svc.sv:141` (free), `:159` (ready, round-robin), `:212` (done). Exactly **one** multiplier site, `:268` `m1_p_q <= 64'(mul_a_c) * mul_b_c`, with `mul_a_c` 32-bit (`:168`) and `mul_b_c` 64-bit (`:169`) — a 32×64. `c_w [NCTX]` is `logic [63:0]` at `:105`. All three writes to `c_w`: `:256` (`64'd0`), `:278`, `:288` (both `m1_p_q >> 24` in MW phases, where `mul_a_c = {8'd0, c_m}` (24 significant) × `mul_b_c = {32'd0, c_x}` (32) ⇒ ≤56-bit product ⇒ ≤32 bits after `>>24`). **The width proof reproduces independently.** | No change. The doc's caveat also holds: `:173` `t_c = 64'h8000_0000 - c_w[pick_i]` genuinely wraps, so the negative case is real and must be retained exactly. |
| **2.11** | *"a raw FIFO plus four per-class FIFOs, with separate 64-bit payload arrays and token arrays"* | **CONFIRMED, counted** | `rsp_dispatch.sv:174-176` `raw_d [RAWN]` (`DATAW`=64), `raw_t`, `raw_c`; `:188-191` `cq_d [NCLS][CHN]` and `cq_t [NCLS][CHN]` — i.e. `1 + NCLS` separate payload stores. | No change. |
| **2.12** | *"accounts for queued responses plus probe results in flight. It reserves a response slot at issue and refunds squashed probes on a miss. It has static generated tag/data banks and a fabric capture stage after RAM."* | **CONFIRMED, all four** | Detailed in §3.2. | **Yes — the single highest-risk preserve.** See §3.2. |
| **2.13** | *"AUX still receives world X/Z by slicing `fr_aux_ctx` … receives the hardcoded envelope 0..65536 … packed as a colour-like third sample. Mosaic is exercised and counted"* | **CONFIRMED and materially understated** | Detailed in §3.4. | **Yes.** See §3.4. |
| **2.14** | Four sub-claims about stale comments | **PARTLY-CONFIRMED** | Broken out below. | Sub-claim (d) upgrades from "audit item" to "defect". |

### 2.14 broken out

**(a) *"Several inspected files still say that nothing instantiates them. They are instantiated."*
— CONFIRMED. Five files, counted:**

| File | Stale line | Actually instantiated at |
|---|---|---|
| `raster/zhao_raster_perspuv_svc.sv:4` | `// Nothing instantiates this yet.` | `island_top.sv:716` |
| `raster/zhao_raster_rcp24_svc.sv:4` | `// ORACLE. Nothing instantiates this yet.` | `island_top.sv:518` |
| `texture/zhao_texture_rsp_dispatch.sv:3` | `// … Nothing instantiates it yet.` | `island_top.sv:1195` |
| `texture/zhao_texture_tmu_plan.sv:4` | `// … Nothing instantiates this yet.` | `island_top.sv:1027` |
| `texture/zhao_texture_palette_res.sv:3` | `// … Nothing instantiates it yet.` | `island_top.sv:1385` |

Only `cache_pipe.sv:6` has been corrected (*"IT IS LIVE. This header said 'Nothing instantiates
this yet' until…"*). A sixth file, `texture/zhao_texture_tmu_pipe.sv:4` — *"INCOMPLETE. NOT
INSTANTIATED ANYWHERE. DO NOT WIRE THIS IN."* — is **not** stale: a repo-wide grep finds no RTL
instantiation, only a standalone Verilator harness under `build/`. Do not "fix" that one.

**(b) *"comments describe one or three bilinear phases while live code uses four."* — CONFIRMED,
two sites in the top:** `island_top.sv:262-263` (the `err_bil_chan_o` port comment: *"The
three-channel accumulator pairs R, G and B by ARRIVAL ORDER"*) and `:1281-1284` (*"one response
becomes THREE jobs — R, then G, then B"*; *"R, G and B return in the order they were issued"*).
Live code: `:1316` `disp_bil_ready = bil_job_ready && (bil_phase_r == 2'd3)`, `:1325` wrap at
`2'd3` — four phases, alpha is the fourth.

**(c) *"A FRAGROB comment still discusses an unfrozen sample zero combiner."* — CONFIRMED but
nuanced.** `fragrob.sv:593-597` and its port `:166` `combiner_unfrozen_o` are accurate *about
FRAGROB*: `o_rgb_o`/`o_a_o` genuinely are sample 0, and are retained deliberately (`:145-149`).
What misleads is the *island-level* implication — the top no longer uses that port for material,
reading `o_s_rgb_o[3]`/`o_s_a_o[3]` (`island_top.sv:889-897`) into COMBINE V2 instead. The
comment is not false; it is a compatibility remnant whose signal is dead at the island boundary
(see §3.1). Do not delete `o_rgb_o` in V3 without confirming no out-of-island consumer.

**(d) *"`bil_expect_r` is not visibly initialized alongside the other collector state. Treat this
as an unverified source-level reset concern, not a reproduced failure."* — CONFIRMED, and it is
stronger than an audit item. It is a reset asymmetry with a determinable consequence.**

```
island_top.sv:1233   logic [1:0]  bil_expect_r;
island_top.sv:1323   if (!rst_n) bil_phase_r <= 2'd0;        // issuer IS reset
island_top.sv:1357-1362
    always_ff @(posedge clk or negedge rst_n) begin
      if (!rst_n) begin
        bil_r_r <= 8'd0; bil_g_r <= 8'd0; bil_b_r <= 8'd0;
        err_bil_chan_o <= 1'b0;                              // bil_expect_r ABSENT
      end else if (bil_lane_valid && bil_lane_ready) begin
        ...
        if (bil_out_chan != bil_expect_r) err_bil_chan_o <= 1'b1;
```

`bil_phase_r` (the issuer) resets to 0; `bil_expect_r` (the checker) does not. Two failure
directions, both real:

* **Cold start / 4-state sim:** `bil_expect_r` is `X`; `X != bil_out_chan` evaluates `X`;
  `if (X)` does not fire — the order check is a **false negative** on the first sample.
  Verilator's 2-state zero-init happens to make it correct, which is precisely why the composed
  fixture cannot see this.
* **Warm reset mid-sample:** `bil_phase_r` returns to 0 while `bil_expect_r` retains a
  mid-sequence value — a permanent, sticky **false positive** on `err_bil_chan_o` for the rest
  of the run.

Either way `err_bil_chan_o` is not trustworthy across a reset. The document's requested
"four-state/reset-reuse test" is the right test, but the verdict is already determinable from
source. **V3 must not carry this shape forward, and must not treat `err_bil_chan_o == 0` from
the existing fixture as evidence that the collector orders correctly.**

---

## 2. Correction to a load-bearing count: the composed fixture runs FIVE drained phases, not three

The tasking (and a casual reading of §2.5) says *"three fully-drained phases."* The fixture's own
header also says three. **Both are stale.** In `tests/texture/island_composed_directed.cpp`:

* header `:77-79` lists PHASE 1 / 2 / 3;
* `run_phase()` is called at `:1057` (CLUT8/nearest, `0x6600`), `:1208` (RGB565/bilinear,
  `0x6609`), and `:1648` — which sits inside a loop over `kDirect[3]` (`:1629-1634`):
  `RGB565/nearest`, `ARGB1555/nearest`, `ARGB4444/bilinear`.

**Five fully-drained mode phases.** Further, the header's *"PHASE 3 … CLUT8 again"* is wrong: the
owner-credit / reorder-wrap stress uses `kModeBil` (`:1312`, `:1369`, `:1398`).

This matters because §2.5's argument is *"the drained-mode phases are the workaround"*, and the
cost of the missing binding resolver is proportional to how many phases the workaround forces.
It is five, and it grows by one for every additional format V3 wants covered.

The fixture also carries two stale line citations into the RTL — `island_top:933` for the planner
T0 (actually `:1027-1041`) and `":1017-1030"` for the class-mismatch comment (actually
`:1084-1125`). Section 2.14's law applies to the tests as well as to the RTL.

---

## 3. The three flagged items, in detail

### 3.1 Preamble — an unstated hazard that touches all three

Twelve signals in `zhao_texture_island_top.sv` are declared, connected to a submodule output
port, and **never read again**. Counted by occurrence (2 occurrences = declaration + port
connection, i.e. zero consumers):

| Signal | Source port | island_top lines |
|---|---|---|
| `fr_wq_overflow` | `fragrob.wq_overflow_o` | 814, 900 |
| `fr_id_error` | `fragrob.id_error_o` | 814, 900 |
| `fr_combiner_unfrozen` | `fragrob.combiner_unfrozen_o` | 814, 901 |
| `aux_sheet_reads` | `aux_pipe.sheet_reads_o` | 1794, 1810 |
| `aux_degenerate` | `aux_pipe.degenerate_o` | 1794, 1811 |
| `mos_req_ready`, `mos_pick_valid`, `mos_tile`, `mos_tx`, `mos_ty`, `mos_src`, `mos_idle` | every output of `zhao_texture_mosaic` | 757-761, 765-772 |

`cnt_fragrob_id_errors_o` **is** surfaced (`:900` `.id_errors_o(cnt_fragrob_id_errors_o)`), so
identity rejections are countable. But **`wq_overflow_o` is not** — and `fragrob.sv:336`
explicitly flags that the "queue cannot overflow" sizing argument is an *assumption* with a
tripwire. That tripwire is invisible at the island boundary, and therefore invisible to every
composed test. The same applies to AUX's `degenerate_o`. **V3 must surface these, not merely
reproduce them.**

### 3.2 Item 1 — the cache's response reservations and static RAM banks (decision G, §2.12)

**What they are, precisely.**

*Reservations* (`zhao_texture_cache_pipe.sv:378-426`, `:587-605`, `:649-666`):

* `rs_resv` (`:395`, width `RQW+1`) counts **queued responses + probe results in flight**, as one
  number. `resv_room = (rs_resv != REQN)` (`:398`).
* A probe may only issue when there is room: `:426`
  `assign c1_go = rq_issuable && !fb_busy_r && !c3_miss && resv_room;`
* **Retirement TRANSFERS a reservation; it does not free one.** The RTL states this twice
  (`:381-382`, `:594`): `rs_wp++` deliberately does **not** decrement `rs_resv`.
* A C3 miss rewinds `rq_ip` and squashes the two probes in C1 and C2; both reservations are
  **refunded**. `:417` `squash_c = c2_v + c1_v`; `:604`
  `if (c3_miss) rs_resv <= rs_resv - squash_c - rs_pop;` else `:605`
  `rs_resv <= rs_resv + c1_go - rs_pop;` — one combined next-state, no second driver.
* Two named assertions guard it: `a_resv_never_exceeds_capacity` (`:653`) and
  `a_queued_entries_stay_reserved` (`:663`, `rs_n <= rs_resv`).

*Static banks* (`:182-191`, `:456-476`):

* `genvar gl` generate `g_lane`, each holding `logic [15:0] data_r [LINES*HW_PL]` and
  `logic [TAG_W-1:0] tag_r [LINES]`. **Deliberately un-reset.**
* A second generate `g_lane_port` puts the RAM ports *inside* the generate, so the lane index is
  a genvar and cannot be a register. `:442-447` states why: a lane chosen by a register is the
  dynamic selection that forces a mux and kills M10K inference.
* Registered read address **and** registered read output (`ram_tag[gl]`, `ram_dat[gl]`);
  non-blocking write ⇒ old-data read-during-write, matching the M10K mode.
* `valid_r [LANES][LINES]` stays flat flops **on purpose** (`:193-196`): 64 bits, and it needs
  the reset a memory cannot give.
* Enforced by `tools/quartus/check_ram_inference.py`, which gained this exact rule after calling
  a half-fixed version clean and losing an 88-minute fit to 2 M10K.

**What "accidentally discarding them during the rewrite" looks like in code.** Six concrete
shapes, any one of which silently reverts the repair:

1. Decrementing `rs_resv` on `rs_wp++` "because the probe is done." This is exactly what
   `a_queued_entries_stay_reserved` exists to catch: the FIFO then holds responses nobody
   reserved room for, and a fresh probe issues against a slot already spoken for.
2. Splitting `rs_resv`'s next-state across two `if` branches or two `always_ff` blocks. It is
   deliberately **one** combined assignment; splitting it re-opens the over/under-refund window
   at a miss that coincides with an issue or a pop.
3. Dropping `resv_room` from `c1_go` while adding a new stall term. The assertion catches it —
   but only if the assertion is carried over.
4. Rewriting the banks as `logic [15:0] data_r [LANES][LINES*HW_PL]` for tidiness. This is
   precisely the `[LANES][N]` shape Quartus reports as *"cannot regroup multidimensional array"* /
   *"uninferred RAM logic"* (`:160-171`). Cost of record for the predecessor block:
   **5,402 ALMs, 9,993 registers, zero M10K** (`zhao_texture_cache.sv:237`).
5. Adding a reset loop over `data_r`/`tag_r`, or an async reset on the RAM output register,
   during a "make everything resettable" pass. Both kill inference (`:186-188`, `:450-452`).
6. Indexing `g_lane[k]` with a loop variable or register outside a generate. It reads as a syntax
   convenience; it is the whole mechanism.

Preserve the two assertions verbatim — they are the only automated check on (1)–(3), and
`check_ram_inference.py` is the only automated check on (4)–(6).

### 3.3 Item 3 — late global binding reads (decision F, §2.5, §12)

**Mechanism, confirmed, with one correction to how it is usually described.**

`island_top.sv:1031` `.req_base_i(bind_base_i), .req_mode_i(bind_mode_i)` — the island's own
top-level input pins, wired straight into the planner. The planner **does** register them, at
`tmu_plan.sv:370-377`:

```
if (t0_rdy) begin
  t0_v <= req_valid_i;
  if (req_valid_i) begin
    ... t0_base <= req_base_i; t0_mode <= req_mode_i; ...
```

So this is *not* a combinational path from a pin into deep logic. **The defect is scope, not
depth.** The word is captured at the *planner's* T0 — the request-issue handshake, many clocks
after the fragment was admitted at `island_top.sv:689-700`, and with no correspondence to *which*
fragment is being planned. `tmu_plan.sv` has no per-fragment binding input at all; its entire
descriptor is decoded from `t0_mode` at `:231-239` (fmt, filter, wrap_u/v, log2w/h, maxlvl,
mip_en, rsvd).

Meanwhile the per-fragment binding **exists and is fully plumbed, then goes nowhere useful**:
`frag_binding_i` (`:134`) → `fbind_m[fc_wp]` (`:694`) → `f_binding_c` (`:733`) →
`fr_f_binding[s] = f_binding_c + BINDW'(s)` (`:863`) → `fragrob.f_binding_i[3]` → `desc_met_m`
(`fragrob.sv:612`) → `tmu_binding_o` (`fragrob.sv:390`) → `island_top.sv:882`
`.tmu_binding_o(fr_tmu_binding)` — and `fr_tmu_binding` reaches **no request-path consumer**.
§2.5's *"A token field that is carried, counted, and never influences the requested address is
not a completed feature"* is literally true, and the carry is eight stages long.

**Consequence, confirmed.** A mode word cannot travel per fragment on this port. The fixture says
so in its own words (`island_composed_directed.cpp:64-76`): *"Writing them inside the submit loop
does not attach a mode to a fragment — it is a late ingress read, the exact class
`tools/rtl/check_ingress_capture.py` exists to prevent, **and the gate misses it because its
contract watches the `frag_` prefix only**."* That last clause is what an implementing lane most
needs: **the ingress-capture gate structurally cannot see this defect**, so a V3 that keeps a
global mode pin will pass `check_ingress_capture.py` and still be wrong.

Cost today: five fully drained phases (§2 above), which is why the composed fixture cannot mix
formats in one stream and therefore cannot exercise the dispatcher's class decoupling under real
mixed load at all.

### 3.4 Item 2 — the AUX connection that uses context bits as geometry (§2.13, §16)

Confirmed at `island_top.sv:1800-1803`:

```
.req_wx_i(fr_aux_ctx[31:0]), .req_wz_i(fr_aux_ctx[63:32]),
.req_env_x0_i(32'sd0), .req_env_x1_i(32'sd65536),
.req_env_z0_i(32'sd0), .req_env_z1_i(32'sd65536),
```

and the colour-like packing at `:1817-1818`:
`fr_aux_rgb = {aux_out_tag, aux_out_str, 8'd0}`,
`fr_aux_a = aux_out_degenerate ? 8'd0 : 8'hFF`.

**Three consequences the document does not state, all bearing on what V3 must preserve.**

**(i) The island contradicts its own opaque-context contract, in the same file.** Lines 655-675
declare the caller's context word inviolate and add an *"HONEST LIMIT"*:

> *"this island consumes only the low 16 bits of the context, as the fragment tag behind
> `out_tag_o`. The remaining bits are stored and carried through FRAGROB intact, but nothing
> downstream reads them and no port exposes them, so NO TEST CAN OBSERVE that they survive."*

`u_aux` reads **all 64 bits** and interprets `[63:32]` as signed world Z. The "honest limit"
comment is false as written, and so is the "no test can observe" claim — an AUX-enabled fragment
observes bits 63:32 *as geometry*. V3's *"keeps opaque context opaque"* is therefore a
**behaviour change with an observable consequence for any caller currently exploiting the
overlap**, not a tidy-up. Establish first whether any caller relies on it.

**(ii) The hardcoded envelope constant-folds away the AUX block's worst measured path, so the
composed fit under-reports AUX.** `aux_pipe.sv:203-206` records the measurement:

```
//   req_env_x1_i[20] -> zhao_texture_aux_div6:u_div|ru_q[0][11]   -8.199 ns
```

fitted at **54.95 MHz — the slowest thing on the texture island by 7 MHz**. The repair was to
split that cone with an A0 register. But in the composed island `req_env_x0/x1/z0/z1` are tied to
literals, so `degen_c` (`:228`), `du_c`/`dv_c` (`:230-231`) and the `x0`/`z0` subtractions inside
`nu_c`/`nv_c` (`:233-234`) all constant-fold. **The 16,192 ALM / 67.57 MHz composed fit does not
measure AUX with a real envelope.** The moment V3 supplies the *"immutable envelope descriptor"*
§2.13 asks for, that arithmetic returns — as area and as a path. Price it into the V3 budget; the
current fit is not a baseline for it.

**(iii) The AUX result has no typed channel, only a colour channel and a hijacked alpha.**
`out_degenerate_o` survives only as `fr_aux_a = 8'd0`. This is the same defect as §2.8 wearing
AUX's clothes: a failure inferred from a colour. §2.8 and §2.13 need one shared fix, not two.

**Mosaic, same section, materially understated.** *"Mosaic is exercised and counted"* — it is
counted and **nothing else**. All seven of `u_mosaic`'s outputs are dangling (§3.1 table), and
`island_top.sv:770` ties `.pick_ready_i(1'b1)` so the block never stalls. Only
`cnt_mosaic_samples_o` escapes. In synthesis, everything outside that counter's cone is prunable
— so, as with the AUX envelope, **the fit does not contain a working Mosaic either.** §2.13's
*"a moving counter is not proof of use"* is exactly right, and the evidence is stronger than the
document shows.

**Also for §2.7's preserve list, unstated by the document:** the palette hazard already has a
tripwire — `island_top.sv:1619-1622`
`else if (pal_lu_valid_o && !fr_tmu_rready) err_rsp_dropped_o <= 1'b1;`. It is unreachable today
*precisely because* `fragrob.sv:456` ties `tmu_rready_o` high. V3's decision J (completion credits
before non-backpressurable launches) makes it reachable. **Carry `err_rsp_dropped_o` forward; it
is the alarm on the exact invariant decision J is changing.**

---

## 4. What the V3 lanes must know that the document does not say

1. **The document is current, not historical.** One commit separates it from HEAD, and that
   commit *is* the document. Do not hunt for repairs that landed "since"; there are none. All
   fixes named as recent (`near_ok_c`, `decode16`/`fmt_is_direct`, `chan8`'s format argument,
   `sampmeta_m` at 20 bits, the 4-phase bilerp, COMBINE V2) are **inside** the baseline and are
   already reflected in §2's own wording.
2. **Twelve island-level signals are dangling** (§3.1). Three are FRAGROB's and AUX's own
   tripwires (`wq_overflow_o`, `id_error_o`, `degenerate_o`). Preserving the *logic* that sets
   them is not enough — they must reach a port, or V3 inherits the same blindness.
3. **The composed fixture runs five drained phases, not three** (§2). Its own header is stale
   about both the count and phase 3's mode.
4. **`check_ingress_capture.py` cannot see the global-binding defect** — it watches the `frag_`
   prefix only, per the fixture's own note. A V3 that keeps a global mode/base pin will pass that
   gate. Add a rule, or the gate teaches false confidence.
5. **The 16,192 ALM / 67.57 MHz fit contains a constant-folded AUX and a prunable Mosaic**
   (§3.4 ii and the Mosaic paragraph). The §2.13 work that "completes" these two connections will
   *add* area and path the baseline never carried. A V3 that lands at the same ALM count while
   completing AUX and Mosaic has improved more than the number shows; one landing slightly higher
   has not necessarily regressed.
6. **`bil_expect_r` is a determinable reset defect, not an open question** (§2.14 d), with a
   false negative at cold start and a sticky false positive after warm reset. Treat
   `err_bil_chan_o == 0` from the current fixture as **no evidence** about collector ordering.
7. **`island_top.sv:1110-1120` is stale in the *other* direction.** It says the composed fixture
   *"drives bind_mode 0x6600 … while tagging half its fragments as the bilinear class"* — i.e.
   that `err_class_mismatch_o` is nonzero today. The W10 fixture rebuild made every phase drive an
   agreeing mode and class (`island_composed_directed.cpp:81`, phase-2 assertion at `:1219`), so
   the counter is now asserted **zero**. The class-disagreement defect remains structurally
   present (§2.6 stands), but **it is no longer being exercised**. A lane reading that comment
   will go looking for a live failure the fixture no longer produces.
8. **`zhao_texture_tmu_pipe.sv:4`'s "NOT INSTANTIATED ANYWHERE" is true** and must not be swept up
   in the §2.14 comment cleanup.
9. **`tmu_binding_o` is plumbed eight stages and consumed nowhere.** When the binding resolver
   lands, that carry chain is where it attaches — the wiring already exists; only the resolver and
   its registered configuration interface are missing.
10. **`err_rsp_dropped_o` must survive decision J** (§3.4, final paragraph).

---

## 5. What I could not verify, and why

* **Section 1's timing claims (67.57 MHz, timing families, critical-path endpoint).** The fit
  summary at `reports/synthesis/blockpaths/zhao_texture_island_top.fit.summary` confirms the
  **resource** half of section 0's headline exactly — 16,192 ALMs (39%), 28,490 registers, 17 DSP,
  32 RAM blocks, 36,024 memory bits, 1,484 virtual pins, Cyclone V `5CSEBA6U23I7`, Quartus
  17.0.2 — but the file carries **no Fmax line**, and the 2.2 MB raw setup report was untracked by
  `d884ce01` itself. I did not run a fit. Appendix A already labels this PUBLISHED ANALYSIS
  (R02/R03), and that label is correct.
* **§2.3's *"plausible mechanism behind a path ending at a RAM write-enable."*** I confirmed the
  structure (a six-term combinational predicate feeding a bank write in the same clock) but did
  **not** establish startpoint, fanout or delay. The document explicitly declines to claim this
  too, which is the right call.
* **COMBINE V2's execution body and its exact eight-recipe arithmetic** (R06, decision G's first
  clause). Appendix A marks it PARTIAL and I did not audit it either; I verified only the
  instantiation and its `NCTX(8)`. Decision G's *"preserve COMBINE V2's paired phases and exact
  eight-recipe arithmetic"* rests on an unaudited body in both the document and this check.
* **The four-state / reset-reuse test §2.14 asks for.** Not run — writing it would mean editing
  tests, which this pass forbids. The source-level verdict in §2.14(d) stands on its own, but the
  simulation confirmation is still owed.
* **Whether any out-of-island caller depends on the AUX / opaque-context bit overlap (§3.4 i).**
  That needs tracing `frag_ctx_i`'s producers outside the island, which was outside scope. It
  gates whether §2.13's *"keeps opaque context opaque"* is a repair or a break.
* **`zhao_texture_bilerp_lane.sv` (R07) internals.** I confirmed the four-phase sequencing from
  the island side (`bil_phase_r`, `chan_i`/`out_chan_o`) but did not re-derive the lane's three
  product sites or its rounding. The document's §2.14 and §10/§11 arithmetic claims about it are
  untested here.
