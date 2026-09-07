# Island rearchitecture — the census retaken, the path re-read, and the proposals ranked

**Date** 2026-09-07. **Author** FABLE architect pass. **Scope** architecture only —
no RTL, test, or build file was modified and no Quartus run was launched.

**Primary evidence** (all already on disk):

| artifact | provenance |
|---|---|
| `reports/synthesis/blockpaths/zhao_texture_island_top.map.rpt` | A&S successful Sat Sep 06 14:24:15 2026, fit `sourceCommit 064d5fad`, 18,611 lines |
| `reports/synthesis/blockpaths/zhao_texture_island_top.setup.summary.rpt` | same fit, 2,000 summarised paths |
| `reports/synthesis/blockpaths/zhao_texture_island_top.setup.rpt` | same fit, `report_timing -nworst 1 -npaths 200 -detail full_path` |
| `reports/synthesis/zhao_block_fit.json`, row `zhao_texture_island_top` | 16,192 ALM / 28,490 reg / 17 DSP / 32 M10K / 67.57 MHz, `rtlCleanAtHead: false` |
| `reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt` | 4,926 lines, pinned at `d884ce01` |
| `reports/V3-DIAGNOSIS-VERIFICATION-20260906.md` | 13 of 14 findings CONFIRMED against HEAD |
| `reports/V3-DEMONSTRATOR-FIT-20260907.md` | `zhao_texture_v3own@v3-full`, 5,678 ALM |
| `fpga/rtl/texture/zhao_texture_island_top.sv` | HEAD `09de69a8` — **358 insertions past the fit tree**, see §0.1 |

**Workload numbers used anywhere below** follow the corrected TMU recon
(`fpga/rtl/texture/RECON-TMU-WHAT-IT-IS-ACTUALLY-FOR.md`, correction box):
known Z60 subtotal **541,640 samples/frame**, up to **1,094,600** under the
ruling-8 three-sample recipe. The retired 850,000 figure is not used to justify
anything here. Proposals that depend on a sample-throughput target are marked
**[TMU-TARGET-DEPENDENT]** — a replacement TMU spec is expected imminently and
those rankings must be re-read against it. The measurements in §1–§2 do not
depend on any target and will survive it.

---

## 0. Two facts that reframe the brief

### 0.1 The fit row is stale against HEAD, in a known and bounded way

`zhao_block_fit.json` says so itself: `rtlCleanAtHead: false`. Between the fit
tree (`064d5fad`) and HEAD (`09de69a8`), `git log` shows four commits touching
the island closure (`a1846867` W9b — already in the fit's working tree per
§4.3c of the island report, `3a06a590`, `d80f29b4` tripwire ports to the
boundary, `b55959f0` CLUT4 nibble), a net **+358/−70 lines in
`zhao_texture_island_top.sv`** plus small edits in cache_pipe, fragrob,
tmu_plan. Everything in §1–§2 below describes **the fit tree**, and the next
fit will not be 1:1 comparable — in particular `d80f29b4` adds boundary ports,
which moves the virtual-pin count that already went 889 → 1,259 → 1,484.

### 0.2 A V3 architecture already exists, is verified, and has a fitted demonstrator

The brief asks for "a ranked rearchitecture" as if the field were open. It is
not: `TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt` is a complete architecture
(decisions A–L, a 56+8 M10K bank inventory, a 6,500-ALM allocation sheet, an
acceptance checklist), `V3-DIAGNOSIS-VERIFICATION-20260906.md` confirmed 13 of
its 14 repository findings against HEAD, and its mandated first task — the
owner/completion/retire experiment — is **built** (`zhao_texture_v3own.sv`,
1,355 lines) and **fitted** (`V3-DEMONSTRATOR-FIT-20260907.md`).

So this report's job is not to invent a rival. It is to (a) retake the census
independently and check it against both the island report §4.7 and V3's own
premises, (b) re-read the timing evidence, and (c) rank what to do next —
including the places where my measurements agree with, refine, or contradict
what those documents say. Where they contradict, I say so by name.

---

## 1. The independent census

### 1.1 Totals, and one unit trap

Map report (Resource Usage Summary, lines 316–345):

    ALMs needed (estimate)      19,307      <- synthesis estimate; fit measured 16,192
    Combinational ALUTs         15,992
    Dedicated logic registers   27,973      <- fit reports 28,490 (+517 fitter duplication;
                                               ~DUPLICATE nodes are visible in the paths)
    Virtual pins                 1,484
    MLAB memory bits                 0      <- see §1.7
    Block memory bits           36,024      (32 M10K in the fit row)
    DSP blocks                      17
    Max fan-out (clk)           28,707

The entity table reports **ALUTs**, the budgets are written in **ALMs**. On
this design they happen to land within 1.3% of each other (15,992 vs 16,192),
so per-entity ALUT shares are a fair proxy — but that is a coincidence of this
fit, not an identity.

### 1.2 Per-entity attribution — §4.7a verified, and extended with DSP

From the Resource Utilization by Entity table (map.rpt lines 346–420), self
figures in parentheses confirmed row by row. §4.7a's table is **correct**. The
extension it did not make:

| entity | ALUT (self) | regs (self) | mem bits | **DSP** | brief §3.3 budget (ALM/reg) |
|---|---:|---:|---:|---:|---|
| **top-level file itself** | **5,483** | **13,459** | 3,904 | 0 | **— no row exists —** |
| PERSPUV | 1,772 | 3,211 | 96 | **6** | 900 / 700 |
| CACHE_PIPE | 1,267 | 3,030 | 8,320 | 0 | 900 / 900 |
| FRAGROB | 2,554 | 2,955 | 5,184 | 0 | 900 / 1,200 |
| RSP_DISPATCH | 334 | 1,025 | 0 | 0 | 350 / 400 |
| RCP24 (incl. seed ROM 147 ALUT) | 1,050 | 953 | 0 | **6** | 650 / 600 |
| TMU_PLAN | 1,298 | 832 | 0 | 0 | 700 / 500 |
| COMBINE.V2 | 759 | 784 | 1,560 | **2** | 650 / 500 |
| AUX_PIPE (incl. div6 406/382) | 816 | 1,105 | 576 | 0 | 550 / 500 |
| PALETTE_RES | 499 | 402 | 16,384 | 0 | 250 / 200 |
| BILERP_LANE | 119 | 183 | 0 | **3** | 250 / 200 |
| MOSAIC | 41 | 34 | 0 | 0 | 500 / 350 |

Three census findings the earlier reports did not state:

1. **The top-level file was never given a budget row.** The brief's §3.3
   nominal table (`islandrearchitecture5.md:493–507`) sums eleven blocks to
   6,600 ALM and allocates **zero** to the composition layer. The layer with no
   budget is where 48% of the registers and 34% of the ALUTs went. That is not
   a coincidence; it is what an unbudgeted organ does. (V3 fixes this
   structurally: its §21.6 sheet gives "owner/control/completion/ready/retire
   machinery" an explicit 1,800-ALM row.)
2. **The blocks alone are already over nominal.** Self-ALUT of the eleven
   blocks sums to 10,509 against the 6,600 nominal — 1.6x before the top adds
   its 5,483. Every §3.4 tripwire that exists is breached: FRAGROB regs 2,955 >
   2,500, cache regs 3,030 > 2,000, PERSPUV at 4.6x its register budget. Only
   BILERP (and the synthesis-gutted MOSAIC) are under budget.
3. **DSP attribution: 6 + 6 + 3 + 2.** PERSPUV's 6 is exactly its brief budget.
   The overrun against the 14 redline is **RCP24 at 6 vs a 3–4 budget** and
   **BILERP at 3 vs 1**. Shapes (DSP Block Usage Summary, map.rpt 455–470):
   4 independent 9x9, 7 two-independent 18x18, 3 sum-of-two-18x18,
   3 independent 27x27. The three 27x27s are consistent with RCP24's measured
   32x64 product (`zhao_raster_rcp24_svc.sv:268`), whose reduction to an exact
   32x32-plus-correction V3 decision I specifies and whose width proof
   `V3-DIAGNOSIS-VERIFICATION` §2.10 reproduced independently.

### 1.3 Where the 36,024 memory bits live

From the RAM Summary (map.rpt 421–453) and entity table, summing exactly:

    PALETTE_RES  mem_r 1024x16            16,384   45.5%
    CACHE_PIPE   4 lanes 128x16 + rq/rs    8,320   23.1%
    FRAGROB      desc merge 16x240 (3,264 bits) + ctx 16x64 x2
                 + aux/order small          5,184   14.4%
    top          fsc_m 64x45 + rob_tag_m 64x16
                                            3,904   10.8%
    COMBINE.V2   payload 8x108 + comp/scratch 8x33 + tag 8x22
                                            1,560    4.3%
    AUX_PIPE     off_tok/u/v + sd_tok         576    1.6%
    PERSPUV      e_tag 16x6                    96    0.3%
                                           ------
                                           36,024

Nearly half the island's block memory is the palette. 32 M10K against a 553
device and a 64 redline: memory was never the problem — under-USE of memory is.

### 1.4 Registers Removed, and Registers Packed — a correction to §4.7a

* **Removed During Synthesis: 1,652 total** (map.rpt 496–1985): 994 merged
  duplicates, 419 stuck at GND, 19 stuck at VCC, 50 lost fanout, remainder
  cascade removals. Notable: AUX's `a0_nv_q[5]` stuck at GND cascades into
  div6's `rv_q` bank — more of the AUX constant-folding §4.3c warned about.
* **Packed Into Inferred Megafunctions** (map.rpt 2962–2984): the table §4.7
  never read. **`fbase_m[0..63][0..31]`, `fwt_m[0..63][0..7]` and
  `frec_m[0..63][0..2]` were packed into `fsc_m_rtl_0`.** That is why the
  "fsc_m" RAM is 64x45: fsc(2) + frec(3) + fwt(8) + fbase(32) = 45 bits,
  exactly. So §4.7a's sentence "only `fsc_m` (2,880 bits) and `rob_tag_m`
  (1,024) became memory" is right about the bits and **wrong about what they
  are**: four attribute-table members went to memory, not one — every member
  whose read is taken at the single retire point. This matters because it is an
  in-file existence proof: *at this exact depth, in this exact file, an
  attribute array whose read Quartus can register becomes a 1-block RAM with
  its neighbours packed in for free.* The same packing also absorbed COMBINE's
  read registers (`pay_rd`, `tag_rd`, `scr_rd`, `cmp_rd`) and FRAGROB's
  (`out_ctx_r`, `ax_ctx_r`, `out_aux_*`) — those blocks already obey the law.

### 1.5 The top's 13,459 registers, accounted bit for bit

Declared top-level state at the fit tree (declarations at
`zhao_texture_island_top.sv:514–1029,1981–1984`, widths from the header
parameters BINDW=8, LODW=8, GENW=8, PSW=3, FCTXW=6, FCTXN=64, DEPTH=16):

    uvw_m      64 x 64  = 4,096      flops (uninferred: async read)
    fctx_m     64 x 64  = 4,096      flops (uninferred: async read)
    rob_m      64 x 33  = 2,112      flops (async 64:1 emit mux, :1999-2002)
    sampmeta_m 16x3x21  = 1,008      flops (THREE async read sites — §1.6)
    fbind_m    64 x 8   =   512      flops
    flod_m     64 x 8   =   512      flops (async read)
    fpgn_m     64 x 8   =   512      flops (async read)
    fseq_m     64 x 6   =   384      flops
    fpsl_m     64 x 3   =   192      flops (async read)
    fcls_m     64 x 2   =   128      flops (async read)
    faux_m     64 x 1   =    64      flops (async read)
    rob_full_m 64 x 1   =    64      flops (legitimately: broadside clear)
    class_m    16 x 2   =    32      flops (async read)
                        -------
                         13,712  declared; 13,459 measured (98%; the gap is
                                 removed/stuck bits from §1.4)

The top's register count is not glue that grew. It is **two 64-entry tables
and a reorder buffer stored as flip-flops**, plus their retinue. uvw_m + fctx_m
+ rob_m alone are 10,304 of the 13,459.

### 1.6 The uninferred-RAM list — §4.7b confirmed, with one refinement §4.3b half-deserves

All 24 messages re-read (map.rpt 6276–6300). Confirmed: 11 x "asynchronous
read logic" (8 top-level arrays + `fragrob|tok_m`, `rcp24|c_m`,
`field_rcp24_rom|Ram0`), 13 x "inappropriate RAM size" (3- and 8-deep block
arrays, correctly flops). §4.7b's refutation of §4.3b stands: for uvw_m and
fctx_m the read-site count fits an M10K and only the asynchronous read blocks
inference.

The refinement: **`sampmeta_m` really is a port-count case.** It has three
independent async read sites with three different indices
(`island_top.sv:1273,1456,1581` — bilinear, CLUT, nearest tokens). No M10K
gives three read ports; registering those reads does not make it inferable
without either replication (3 copies x 1,008 bits in RAM = 3 M10K, cheap) or
V3's single-reader response-preparation organization (decision/implementation
line 14: "Read metadata once"). So §4.3b was wrong as a general prescription
and right about this one array — worth recording so the next pass does not
"fix" sampmeta_m by registering reads and then report a mystery failure.

### 1.7 Smaller census corrections and observations

* **§4.7d located the latch in the wrong file.** The warning (map.rpt 6193) is
  at **`zhao_texture_material_combine_v2.sv:653`**, variable
  **`refused_recipe_o`** — 32 latches (10041 messages counted), all bits of one
  exported diagnostic output. Not "in the island top, among the per-fragment
  attribute declarations". The fix belongs to COMBINE.V2; note
  `refused_recipe_o` is an exported counter, so any repair that changes its
  semantics rather than merely giving it a default assignment is an **ABI
  change**.
* **MLAB bits: 0.** The device's LUT-RAM is entirely unused. Sixteen-deep
  arrays like `sampmeta_m` and FRAGROB's res banks are MLAB-shaped; Quartus
  never got the chance because the reads feed logic combinationally
  (`QUARTUS_GOTCHAS` §14 — the read must reach a register first).
* The fitter added ~517 registers over synthesis (28,490 vs 27,973), visible
  as `~DUPLICATE` nodes on the worst paths — replication chasing exactly the
  fanout problems §2 describes (e.g. `head_q[1]` fanout 273).

---

## 2. The critical path, re-read

### 2.1 §4.6 is stale, and the current summary does not match §4.3c's split either

* **§4.6 describes the `afb7070f` fit** (worst −4.482 from `frag_depth_i[19]`
  into `rcp24|c_x[7][8]`, 2,000 paths splitting 405 pin / 1,595 internal).
  Neither that path nor that split exists in the current reports. The
  `frag_depth_i` launch is gone because ingress capture registered it. §4.6's
  headline — "the finding is RCP24" — is **partly survived**: RCP24's context
  array is still a worst-family endpoint (§2.2 F2), but it is no longer the
  worst path and its launch point moved.
* **§4.3c's split ("3,904 paths, 3,715 = 95.2% start at a PIN") does not
  describe the summary on disk.** The on-disk `setup.summary.rpt` for this same
  fit holds **2,000 rows**, of which exactly **5** launch from a pin and
  **8** launch from a non-hierarchical (top-level register) name. I cannot
  reconstruct where 3,904 came from — possibly a different capture of the same
  run, possibly a classifier that counted the top's own registers (`live_r`,
  `seq_head_r`, `rob_m…`, which carry no `|`) as pins. Its **conclusion** is
  nonetheless correct and I reproduce it exactly: worst overall −4.800
  (67.57 MHz), worst with core at both ends **−2.936 (77.30 MHz)**. My split:
  1,994 core→core, 5 port→core, 1 core→port, 0 reset-launched.
* **The reported 67.57 is set by ONE virtual pin.** All five boundary paths
  launch at `pal_ld_gen_i[1]` and land in `palette_res|res_r/loading_r`. Delete
  one pin and the reported number becomes the core number. When the island is
  composed for real, that seam is real again — registering the palette-upload
  control at the island boundary is a two-flop fix that makes every future
  reported number honest (§4, R6).

### 2.2 The four families, with anatomy

Grouping the 1,994 core→core paths by module (my split of the summary):
launches — RCP24 1,014, RSP_DISPATCH 967, top 8, PERSPUV 5; landings —
PERSPUV 985, FRAGROB 972, RCP24 37. Read with the detailed path reports
(`setup.rpt` paths #6–#15):

| family | worst | shape | levels | data delay | IC share |
|---|---:|---|---:|---:|---:|
| **F1** `perspuv\|head_q[1]` → `fragrob` RAM write-enables (`axg_m` porta_we, `axq_m` bypass) | **−2.936** | seam handshake: head pointer (fanout 273) through Mux9 → `alloc_ev_c` → RAM WE | **5** | 13.994 ns | **67%** |
| **F2** top `live_r[6]` → `rcp24\|c_m.raddr_a[*]` | −2.834 | island credit counter feeding the context array's **read address** | 8 | 12.654 | ~65% |
| **F3** `dispatch\|cq_t/cq_rp` → `fragrob\|res_rgb_m/res_a_m[bank][slot][bit]` | −2.777 | response payload through seam into the **flip-flop result banks'** write demux | 7–9 | 12.198 | ~63% |
| **F4** `rcp24\|m1_i_q[2]` → `perspuv\|e_num_u/v[entry][bit]` | −2.565 | RCP product through seam into PERSPUV's **uninferred numerator arrays'** write demux | 9 | 11.988 | ~64% |

And one path family nobody flagged: the single core→port row is
**`seq_head_r[2] → out_rgb_o[1]` at −1.647 (85.9 MHz implied)** — the R6
reorder buffer's 64-to-1 emit mux reaching the island output. Even standing
alone, before any composition routing is added, the asynchronous ordered-emit
read misses the product clock.

### 2.3 What this refines about §4.7c's "one defect" claim

§4.7c argued area and clock are ONE defect — asynchronous reads of deep
arrays — with the register-the-read fix. The brief told me to test that claim
rather than inherit it. The verdict is **half**:

* **For AREA the claim is measured and solid.** §1.5: 10,304 of the top's
  13,459 registers are exactly the async-read arrays plus the ROB, and the
  in-file packing proof (§1.4) shows what happens when a read can be
  registered.
* **For the CLOCK the claim is not what the paths say.** Of the four families,
  only F2 is a read-address path into an async-read array, and none of the
  four is a read-DATA mux path. F1, F3, F4 all end on the **write side** —
  write enables and write demux — launched across a block seam with 63–67%
  interconnect and only 5–9 logic levels. The clock is limited by
  **combinational seam crossings terminating in array-write cones**, which is
  V3 §0 point D's diagnosis (named at `fragrob.sv:626/:443`) far more than it
  is a wide-read-mux story. Registering the reads will collapse the register
  count and *may* relieve routing pressure; it does not directly touch −2.936.
  §4.7e was right to refuse the conclusion, and the specific reason is now on
  paper.

The two mechanisms share one root — fragment state organized as
register files with no pipeline boundary at either the seams or the array
ports — and that root is precisely V3 decisions C, D and H. My independent
path reading **corroborates the V3 diagnosis**.

### 2.4 The demonstrator's two Fmax numbers, reconciled

`FMAX-WHAT-ACTUALLY-LIMITS-IT-20260907.md` lists `zhao_texture_v3own@v3-full`
at **89.09 MHz core**; `V3-DEMONSTRATOR-FIT-20260907.md` says **75.79 and
internal**. Both are computed from the same fit; they disagree because they
classify differently. The demonstrator's three worst internal-origin paths end
at `adm_accept_o` / `ev_quiet_o` — **combinational output ports**. The
both-ends-core split (89.09) excludes them as boundary; the origin-only split
(75.79) keeps them. For *planning* the honest number is nearer 75.79: a
combinational accept computed from a ready-queue write pointer becomes REAL
seam logic in composition, plus routing the leaf fit never saw. The ready
queue's `wp_q → adm_accept_o` cone is a genuine V3 design debt, not a
measurement artefact.

---

## 3. Ranked proposals

Ranking is (measured benefit) / (blast radius). Each entry: what changes,
which measured number it targets, prediction **with derivation**, risk, and
falsifier. R1–R3 are measurements or map-only work; R4–R8 change the machine.

### R1. Attribute `zhao_texture_v3own`'s 5,678 ALM before anything else touches V3

* **Targets:** the credibility of the entire V3 lane. The demonstrator's
  structural claims all passed (payload in RAM to the bit, write-enable cone
  structurally gone, 4,864 regs at 64-owner capacity), but it costs **3.15x
  its own §21.6 allocation** (5,678 vs 1,800), and §21.6's whole sheet sums to
  6,500 — if the owner-machinery row really needs 5,678, V3 does not fit its
  own budget and the island question reopens at the architecture level.
* **What:** a map-only run's entity/hierarchy attribution of the demonstrator
  (minutes, not hours; the harvest infrastructure already exists), splitting
  owner scoreboard vs the three `v3rq` ready queues vs bank glue vs the
  adversarial-wrapper overhead that a leaf fit inflicts (952 virtual pins).
* **Prediction, derived:** §21.5's own arithmetic says the scoreboard is
  ~1,920 flops; 4,864 measured registers leave ~2,900 unattributed, and the
  fit report's suspicion is the 64-way scoreboard decode plus the ready
  queues. If a large share is the characterisation wrapper, the breach
  shrinks; if it is the queues, R3 fixes area and timing together.
* **Risk/blast radius:** zero. **Falsifier:** none needed — this IS the
  falsification step for everything V3-shaped below.

### R2. V2 palliative: synchronous reads for `uvw_m`, `fctx_m`, `rob_m` at the island top

* **Targets:** registers 28,490 (rule 9,000) and ALM 16,192 (rule 7,500) of
  the live V2 island; also the experiment that settles §2.3's open half.
* **What:** register the read address (data next cycle) for the three big
  top-level arrays; consume one cycle of latency at RCP ingress
  (`uvw_rd`, line 726), at the FRAGROB feed (`fctx_rd`, line 750), and at
  ordered emit (lines 1999–2002). All three sit behind elastic
  valid/ready handshakes, so the cycle is absorbed, not an interface change.
* **Prediction, derived:** registers **−10,304** (§1.5: 4,096 + 4,096 +
  2,112) → island ~18,200. ALUT: the read muxes these arrays imply are
  ~13–21 LUTs per 64:1 bit-slice; uvw 64 b + fctx 64 b (x2 read sites) +
  rob 33 b ≈ 225 mux-bit-slices ≈ **−2,000 to −4,000 ALUT** (estimate, marked
  as such — the map run is the measurement) → island ~12,200–14,200 ALM.
  M10K **+5** (64x64 needs two blocks at that width, x2 arrays, +1 for rob_m).
  Fmax: **modest** — perhaps the F2 family only; F1/F3/F4 are untouched. If
  fmax does not move, that is §2.3 confirmed, not a failed change.
* **Why bother if V3 replaces the organization:** it is a one-file, reversible
  change on the baseline that (a) keeps the V2 island usable as the oracle
  V3 §25 requires, (b) prices the async-read mechanism with a real number
  instead of my estimate, and (c) is the exact storage law V3 demands anyway,
  applied to the file that violates it hardest. `fsc_m`'s packing (§1.4) is
  the in-file proof of concept.
* **Risk:** the live-tree trap — this file is in the island fit closure
  (`design/fit_targets.yml:826`); do not touch while an island fit runs.
  **Falsifier:** map-only run after the edit. If the arrays still refuse to
  infer, the read restructuring was wrong, and the 276007 messages will say so
  by name. **What it cannot do:** reach budget. Even at full predicted effect
  the island is ~2x the register rule. Only V3's reorganization plausibly
  closes the remaining gap — which is why this is ranked below R1.

### R3. Fix the V3 ready queue (`zhao_texture_v3rq`) accept path

* **Targets:** the demonstrator's own clock — all three worst internal paths
  are `v3rq|wp_q[0] → adm_accept_o / ev_quiet_o` — and possibly part of its
  ALM breach (R1 will say).
* **What:** make acceptance a registered credit (count-based `can_accept_q`
  updated from enq/deq, standard credit counter) instead of a combinational
  function of the write pointer. This is the same acceptance-over-latency law
  the owner already ratified for the TMU.
* **Prediction:** removes the named −3.194 family; next-worst unknown.
* **Risk:** an off-by-one in credit accounting stalls or over-admits — the
  adversarial bench (467 checks) exists precisely to catch this.
  **Falsifier:** refit of the demonstrator; the split must show `wp_q` gone
  from the worst internal paths.

### R4. RCP24: exact 32x32 + correction, and a synchronous `c_m`

* **Targets:** DSP 17 → ~14–15 (of the 3 x 27x27 shapes, §1.2) and family F2.
* **Derivation:** the width proof is verified at HEAD
  (`V3-DIAGNOSIS-VERIFICATION` §2.10: every `c_w` write is ≤32 significant
  bits after `>>24`; the wrap case at `:173` is real and must be kept
  bit-exact — V3 decision I specifies the signed-wrap correction). `c_m` is on
  the 276007 async-read list and its read address is the F2 endpoint.
* **Blast radius:** RCP24 leaf + its oracle equivalence tests; the island
  merely refits. This is also V3 §26.1's sanctioned parallel lane ("the RCP
  arithmetic tile can establish its own exact-tool packing").
  **Falsifier:** the leaf map's DSP Block Usage Summary; the exact-tool
  microbenchmark is the authority (§21.7), not the source asterisk count.

### R5. Register the island-boundary palette-upload controls

* **Targets:** honesty of every future reported island Fmax (§2.1: 67.57 is
  one pin's number) and the real composed seam that pin stands for.
* **What:** one register stage on `pal_ld_*` at the island edge (palette
  upload is pre-frame configuration; a cycle is free).
* **Prediction:** reported number becomes the core number (+9.7 MHz of pure
  measurement honesty, 0 of real speed). Cost ~40 flops.
* **Risk:** nil. It is also cheap enough to ride along with R2 in one map run.

### R6. Seam registration in the V2 lane — only if V2 must ship before V3

* **Targets:** the 77.30 core number; families F1, F3, F4.
* **What:** skid/pipeline registers at perspuv→fragrob allocation,
  dispatch→fragrob result delivery, rcp→perspuv result delivery. This is a
  **pipeline change: every consumer's timing moves**, though all three seams
  are credit/valid-ready and absorb a cycle without protocol change.
* **Why ranked low despite targeting the headline:** it is V3 decisions C/D/H
  done piecemeal inside an organization V3 deletes. Worth doing only if the
  owner wants the V2 island to reach toward 100 MHz *as is*; otherwise it is
  double work. **[Owner call.]**

### R7. Bilerp DSP reduction **[TMU-TARGET-DEPENDENT]**

* BILERP holds 3 DSPs vs the brief's 1-DSP serial-channel budget. Against the
  corrected workload, bilinear serves beams, one cloud sheet and a sun quad —
  small-area content (recon §2.A) — so a serialized filter at II=3 likely
  clears demand. But the replacement TMU spec may re-mode-weight the contract
  (ruling 8's three-sample recipes reach 1,094,600 samples/frame), so **do not
  act on this row until that spec lands.** If it survives: −1 to −2 DSP,
  putting 17 → 14 within reach together with R4.

### R8. `sampmeta_m` single-reader restructure

* Covered by V3 implementation line 14 ("read metadata once; one response-pool
  writer, narrow class tickets"). In the V2 lane the cheap form is 3x
  replication into MLAB/M10K (3 M10K, −1,008 flops, removes three async read
  muxes). Ranked last: small numbers, and V3 deletes the structure.

---

## 4. The ordering question, in numbers

What **exact** final ordering costs in the fitted V2 island (all measured or
derived from §1.5/§2.2):

| component | cost |
|---|---|
| `rob_m` 64x33 payload in flops | 2,112 registers |
| `rob_full_m` + `seq_head_r` + `fseq_m` sequence plumbing | 64 + 6 + 384 registers |
| `rob_tag_m` | 1 M10K (1,024 bits) — already in memory |
| 64:1 x 33-bit asynchronous emit mux | ~450–700 ALUT (estimate) **and a measured −1.647 ns path to `out_rgb_o` = 85.9 MHz best case** (§2.2) |

So exact ordering as currently built costs ~2.6k registers, roughly half a
thousand ALUTs, and caps the island below the product clock at its very
output. **But the expensive part is the storage discipline, not the
exactness.** V3 §18 keeps order EXACT and prices it at: FINAL_RESULT 64x40 +
OWNER_CONTEXT read (already-budgeted banks), an F0–F4 registered read
pipeline (~150–300 regs), a 4-deep output FIFO (3 M10K in the buffered
profile), and **zero** wide mux — sustaining 1 fragment/clock on contiguous
completions.

A bounded-reorder window (e.g. 16-entry) would save, against the *flop* ROB,
~1,600 registers and shrink the mux 64:1 → 16:1 — but against the
*synchronous* ROB it saves approximately **nothing** (the RAM is one block
either way; the read pipeline is identical), while it **changes the island's
output contract**: downstream blend/raster would receive fragments up to a
window out of order and must either tolerate that (a depth/blend correctness
argument that does not currently exist) or rebuild the reorder there — an
**ABI change** that moves the cost, not removes it. V3 §18.3 additionally
gives the throughput argument for strict in-order emission (no hole-skipping).

**Recommendation with numbers:** keep exact ordering; buy it back by making
the ROB synchronous (R2's `rob_m` line, or V3 §18 wholesale). The adjective
"expensive" attached to exactness belongs to the asynchronous mux, which no
ordering policy requires.

---

## 5. The budget question

Device: 41,910 ALM, 553 M10K, 112 DSP; 10% fabric reserve ⇒ **37,719 ALM
budgetable**. Shares:

| island at | of device | of budgetable fabric |
|---|---:|---:|
| 7,500 (redline) | 17.9% | 19.9% |
| 16,192 (measured) | 38.6% | **42.9%** |

The island is one organ among terrain, field, raster, geometry, blend, and
the shell. At 43% of budgetable fabric it forecloses the rest of the console;
no accounting of the new features rescues that.

Do ingress capture and R6 ordering justify moving the redline? **Priced
honestly, no.** As specified (not as implemented), the two features cost:
64-entry attribute capture ≈ 2–4 M10K + write/read pipeline (~300–600 ALM),
exact ordering ≈ 2 M10K + read pipeline + FIFO (~200–400 ALM). V3 §21.2/21.3
budgets both inside a 56+8 M10K profile and the SAME 6,500-ALM sheet, and
§21.1 states the rule this report endorses verbatim: the 7,500 / 9,000 / 14
redlines "remain escalation gates. A design exceeding them is reported as a
failed allocation, not quietly accepted because it is smaller than 16,192."

**Defensible budget for the island AS SPECIFIED NOW:**

* **ALM: keep 7,500** (target 6,600). The overrun is storage discipline, not
  feature weight — §1.5 shows 10.3k registers of it is three arrays in flops.
* **Registers: keep 9,000, with V3's own caveat surfaced to the owner:**
  V3 §21.5's sizing envelope is 6,900–8,900 — the first correct V3 may land
  above the 8,000 objective while inside the redline. That is a knowingly
  tight gate, not a comfortable one.
* **M10K: adopt the explicit 64-block profile** (V3 §21.3) in
  `design/fit_targets.yml` in place of the vaguer 32–56-expected/64-redline
  prose. Spending memory is the correction, not the regression.
* **DSP: keep 14** (target 10–13); the concrete path is R4 (+R7 if the TMU
  respec permits).
* One budget line is genuinely MISSING rather than wrong: **the composition
  layer needs its own row** (V3 gives it 1,800 + 200). Whatever sheet the
  owner ratifies next, no organ should again be left off it.

---

## 6. What I could not determine, and the measurement that settles each

1. **Whether registering the async reads moves the clock at all.** §2.3
   predicts "barely". Settled by the R2 map-only run plus one island refit
   (map: minutes; fit: budget 4 h per §4.3a history).
2. **Where v3own's 5,678 ALM live.** Settled by R1's map-only attribution run.
   Until then, every V3 area claim is provisional.
3. **The island's real Fmax with AUX unfolded and MOSAIC's outputs consumed.**
   §4.3c documented both foldings; AUX's own worst path (−8.199, 54.95 MHz,
   `req_env_x1_i → ru_q`) is worse than anything in the composed summary.
   Settled only by a composed fit with a real envelope wired — which should
   wait for the V3/V2 lane decision rather than spend 4 h pricing a known
   confound.
4. **What the current HEAD island (post-`b55959f0`, +358 lines, new boundary
   tripwire ports) measures.** Every number here is `064d5fad`'s. Settled by
   the next scheduled island fit; do not launch one just for this
   (owner's v2 priority 5: no refits after speculative glue edits).
5. **Whether the replacement TMU spec changes the bilerp/filter sizing** (R7)
   or the sample-descriptor plan. Settled by the owner brief expected today —
   §1–§2 of this report are measurements and survive it either way.
6. **fmax honesty of the composed acceptance floor.** The 105 MHz composed
   floor (brief §16) has never been re-derived since the shell measured 99.34;
   whether 105 or 100 binds the island composition is an owner ruling, not a
   measurement.

## 7. Corrections to the record, consolidated

| where | claim | status |
|---|---|---|
| G1D report §4.7d | latch "in the island top" | **WRONG** — `material_combine_v2.sv:653`, `refused_recipe_o`, 32 latches (§1.7) |
| G1D report §4.7a | "only fsc_m and rob_tag_m became memory" | **INCOMPLETE** — `fbase_m`/`fwt_m`/`frec_m` packed into the fsc RAM; that is why it is 45 bits wide (§1.4) |
| G1D report §4.7c | area and clock "one defect… register the read" | **HALF** — area yes (measured); the clock on this fit is seam crossings into array-WRITE cones (§2.3) |
| G1D report §4.6 | "the finding is RCP24", worst −4.482 | **STALE** — `afb7070f` fit; current worst family is perspuv→fragrob at −2.936; RCP24's `c_m.raddr` survives as family #2 (§2.1–2.2) |
| G1D report §4.3c | "3,904 paths, 95.2% start at a PIN" | **UNREPRODUCIBLE from the on-disk summary** (2,000 rows, 5 pin-launched); its 77.30 conclusion independently confirmed (§2.1) |
| G1D report §4.3b | "it is a PORT COUNT question" | already refuted by §4.7b; **except sampmeta_m**, where it is true (§1.6) |
| FMAX report | v3own "89.09 core" | **OPTIMISTIC** — excludes real combinational-output seam paths; plan against 75.79 (§2.4) |
| islandrearchitecture5 §3.3 | nominal table | **MISSING A ROW** for the composition layer that now holds 48% of the registers (§1.2) |
