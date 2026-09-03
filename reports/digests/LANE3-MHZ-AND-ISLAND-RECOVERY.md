# LANE 3 — Timing closure and the texture-island resource recovery

Reconnaissance digest, 2026-09-03. **No RTL, tests, manifests or design files were
modified; no fit, map or synthesis was run** (`zhao_prod_top` was fitting throughout —
`quartus_map.exe` confirmed live at the time of writing).

Sources read in full: `islandrearchitecture5.md` (3,829 lines), `MHZArchitected`,
`ShellFixes.md`, `MHZ-PASS-SUMMARY.md`, `NOISE_FLOOR.md`, `TIMING_HAZARD_SCAN.md`,
`QUARTUS_GOTCHAS.md` §10, `TEXTURE-ISLAND-FIT.md`, `FRAGMENT-RMW-SPLIT-DESIGN.md`,
`RENDERER_ARCHITECTURE.md`, `PER_PIXEL_BUDGET.md`, `RASTER_Polygon_Budget_Proposal.md`,
`DOCKET.md`. RTL read: `zhao_texture_cache_pipe.sv`, `zhao_texture_cache.sv`,
`zhao_raster_texjoin_v2.sv`, `zhao_raster_perspuv_svc.sv`, `zhao_raster_rcp24_svc.sv`,
`zhao_texture_tmu*.sv`. Measurements from `reports/synthesis/zhao_block_fit.json`
(2026-09-03 17:31) and `INTERNAL-PATHS.txt`.

`Islandrearchitect3.md` was checked for anything dropped: it is a chat transcript whose
specification section begins at line 1003 and runs 22 lines. **v5 is a strict superset.
Nothing was lost between versions.** (`Islandrearchitect.md`, `2.md` and
`islandrearchitecture4.md` are no longer in the tree.)

---

# THE RECOVERY PLAN

Ordered by dependency and by recovered resource per unit of risk. Every row names the
brief's C-numbered commit gate (§26) and its §3.4 tripwire where one exists.

**The first two rows carry 79% of the ALM recovery and 75% of the register recovery.**
Everything after R4 is small, and several rows deliberately *add* ALMs to buy back DSPs.

| # | block | the change | ΔALM | Δreg | ΔM10K | ΔDSP | gate |
|---|---|---|---:|---:|---:|---:|---|
| **R0** | — | `design/fit_targets.yml` resource minima/maxima | 0 | 0 | 0 | 0 | **C2** |
| **R1** | `cache_pipe` → `cache_v2` | **storage coding style + static per-lane banks** | **−5,003** | **−10,428** | **+6** | 0 | **C3/C4/C5** |
| **R2** | `texjoin_v2` → `fragrob` | bank descriptors/results by sample index | **−2,924** | **−5,951** | **+12** | 0 | **C6/C7/C8** |
| R3 | `perspuv_svc` → pair pipe | delete the 16-entry token table; 64→56 bit | −1,304 | −2,593 | 0 | 0 | **C12** |
| R4 | `rcp24_svc` → sched v2 | queues replace 3 scans; 32×64 → 32×32 | −391 | −501 | +1 | **−3** | **C9/C10** |
| R5 | `mosaic` → `mosaic_csd` | exact signed-digit shift/add | +303 | +158 | 0 | **−4** | **C21** |
| R6 | `bilerp_lane` → `bilerp_sched` | one 18×9 structure, II=3 | +125 | +23 | +1 | **−2** | **C19/C20** |
| R7 | `tmu_plan` → `plan_v2` + bindings | static work off the sample path | −442 | −880 | +8 | 0 | **C14/C15/C16** |
| R8 | `fragrob` | sample expander (deletes the 64-entry work FIFO) | in R2 | in R2 | in R2 | 0 | **C11** |
| R9 | `rsp_dispatch` → class router | token queues replace copied wide FIFOs | −456 | −1,032 | +4 | 0 | **C13/C18** |
| R9b | `palette_res` → `palette_v2` | ping-pong + CRC (protocol, not recovery) | +98 | +59 | +4 | 0 | **C17** |
| R10 | `aux_pipe` → `aux_pipe_v2` | binding table + credits | −632 | −1,098 | +1 | 0 | **C23** |
| R11 | *new* `material_combine_v1` | the eight recipes (adds resource) | **+650** | **+500** | +2 | 0–2 | **C22** |
| R12 | composed island | torture + three-seed fit | — | — | — | — | **C24/C25** |
| R13 | + reduced renderer | ≥105 MHz | — | — | — | — | **C26** |

Running total, starting from the measured like-for-like island (see *Correction 1*):

```
start        16,576 ALM   27,793 reg   10 M10K   19 DSP
after R1     11,573       17,365       16        19
after R2      8,649       11,414       28        19
after R4      6,954        8,320       29        16
after R6      7,382        8,501       30        10
after R11     6,600        6,050       49      10-12   <-- brief §3.3 nominal, exactly
```

The plan closes on the brief's own §3.3 nominal total from the measured start. That is a
confidence signal about the budget, not a prediction about the fitter.

## WHAT TO DO FIRST

**R1a–c and R0, in one pass, before any other island work.**

R1's fix is not a design — **it is already written, measured, and sitting in the block
`cache_pipe` was built to replace.** `zhao_texture_cache.sv` lines 495–523 record a clean
A/B: making the lane index static changed nothing (5,402 → 5,373 ALM, zero M10K both
times); moving the array into a **clock-only process** is what inferred the memory. That
block reports **8,192 memory bits and 4 M10K at 1,087 ALM / 1,737 reg.** The rebuild
reports **128 memory bits and 2 M10K at 5,903 ALM / 11,328 reg.**

R0 costs no fit and no RTL, and it is what makes R1 falsifiable: `design/fit_targets.yml`
today carries only `top:` and `sources:` for `zhao_texture_cache_pipe` (lines 41–43) —
**no `min_m10k`, no `max_registers`, no `max_alms`.** That absence is why 98.66 MHz was
reportable as a pass this morning.

The brief's §25 stop-condition makes the ordering mandatory rather than merely sensible:

> After cache_v2 — **If M10K does not infer: stop. Fix memory coding style. Do not
> optimize logic around flops.**

Do not run any fit until `zhao_prod_top` finishes.

---

# THE STORAGE-INFERENCE DIAGNOSIS

## Why the rebuild went backwards: it is one cause, and it has a name

`zhao_texture_cache_pipe` reports **`blockMemoryBits: 128`.** Not "poorly packed" —
absent. The register arithmetic closes to the bit:

```
data_r   4 lanes x 16 lines x 8 halfwords x 16 bits  =  8,192 bits
tag_r    4 lanes x 16 lines x 24 bits                =  1,536 bits
                                                        -------
                                                         9,728 bits

measured registers                                      11,328
minus the arrays                                       -  9,728
                                                        -------
the rest of the block                                    1,600   <-- plausible
```

**Every bit of both arrays is a flip-flop behind a mux tree.** On §10's measured penalty
curve (2,048 bits → 35×, 4,096 → 108×, 32,768 → 502×) that is also the whole ALM overrun.

### The killer, named, with lines

The file's own header claims (`zhao_texture_cache_pipe.sv:128-130`):

> `data_r` is READ ONLY THROUGH A REGISTERED ADDRESS, below, and never in a continuous
> assignment. That is the difference between an M10K and 8,192 flip-flops.

The **read** side of that claim is true — `:302-306` is a clean synchronous read in a
clock-only process. The **write** side defeats it:

| line | construct | why it kills inference |
|---|---|---|
| **`:309`** | `always_ff @(posedge clk or negedge rst_n)` — the process that contains the fill engine | **THE KILLER.** An M10K has no reset port. An array written inside a process with an asynchronous reset cannot be one, *whether or not the array appears in the reset branch*. |
| **`:412`** | `data_r[k][{fb_idx_r, fb_beat_r}] <= fill_data_i;` — inside that process | the data array's only write |
| **`:416`** | `tag_r[k][fb_idx_r] <= fb_tag_r;` — inside that process | the tag array's only write |
| `:136-137` | `logic [15:0] data_r [LANES][LINES*HW_PL];` — a **2-D unpacked array** | secondary. The predecessor's note is explicit that this alone was *not* the blocker, but §10.4 mandates static generate banks regardless, and 2-D unpacked arrays are the shape Quartus Lite maps worst. |
| `:371` | `rs_data[rs_wp[RQW-1:0]][16*k +: 16] <= c2_rdat[k];` | **§10 killer 3** — a part-select on the LHS of an array-element write is a byte enable wearing different syntax. Harmless at `REQN=4` (256 bits, correctly flops), but this pattern must not be carried into the depth-8+ response and class queues §10.8/§11.3 ask for. |

Compare the predecessor, `zhao_texture_cache.sv:495-523`, which gets 4 M10K:

```systemverilog
generate
  for (gl = 0; gl < int'(LANES); gl++) begin : g_lane_port
    // NO RESET IN THIS PROCESS, and that is the point of it.
    always_ff @(posedge clk) begin                       // <-- clock only
      if (... && fill_lane_r == LANE_W'(gl))
        g_lane[gl].mem_r[{fill_idx_r, fill_beat_r}] <= fill_data_i;
      if (acc_go) s1_hw_r[gl] <= g_lane[gl].mem_r[{a_idx[gl], a_beat[gl]}];
    end
  end
endgenerate
```

One flat 1-D array per lane, created in a `generate`, written and read in a **clock-only**
process. That is the whole difference.

**The C0–C4 pipeline, the two-pointer replay, the multicast, the counters and the
same-edge tag/valid fix are all correct and all stay.** R1 is a storage-coding change
inside a working block, not a rewrite of it.

### Capacity, once it infers

At `LINES=64` (brief §10.3): per lane, data = 64 × 8 × 16 = **8,192 bits → one M10K**;
tag = 64 × 24 = **1,536 bits → one M10K**. Four lanes = **8 M10K exactly**, which is what
§10.4 predicts and what the §3.4 tripwire requires as a *minimum*. Capacity rises 1 KiB →
4 KiB at no extra M10K over `LINES=16` would have cost. Fit 32 and 64 on identical RTL and
keep 64 unless the count or Fmax worsens.

## TEXJOIN: the same disease, plus two more

`zhao_raster_texjoin_v2` — 3,824 ALM / **7,151 reg** / 4 M10K / 1,648 memory bits.

The per-entry table at `:157-173` sums to **7,056 bits** (`DEPTH=16, CTXW=64, BINDW=8,
LODW=4, GENW=8`), against 7,151 measured registers. **The entry table is 99% of the
register count.** Three independent faults:

1. **Written in the async-reset process** — `:350`, same killer as the cache. Writes at
   `:385-393`, `:416-417`, `:468-469`, `:496`.
2. **Read combinationally** — `:332-341`, `always_comb` reading `srgb_q[head_q][0]` and
   `sa_q[head_q][0]` through the seven-way recipe case. §10 killer 1. This alone forbids
   `srgb_q`/`sa_q` from ever being memory. (The case is also a placeholder: all seven arms
   are identical and return sample 0 — the combiner arithmetic is not frozen, so the
   island's real material functionality is **not yet inside the measured number**.)
3. **Two dynamic write addresses into one array in one clock** — allocation writes
   `su_q[tail_q][j]` at `:390-393`; the TMU return writes `srgb_q[tmu_rslot_i][tmu_rsidx_i]`
   at `:468-469`. Brief §5.3 names this exactly: *"Writing two dynamic addresses into an
   unpacked array and hoping for M10K is not a design."*

**The fix is §6.4's banking by sample index.** `SAMPLE_DESC_U[3]`, `_V[3]`, `_META[3]`,
`SAMPLE_RESULT[3]`, each 16 deep, in a `generate` so `j` becomes a *compile-time* index and
each bank carries one dynamic address. Descriptor banks are written only by allocation;
result banks only by returns — which removes fault 3 structurally rather than by
arbitration. Keep only §6.3's control flops (valid / generation / required / arrived /
aux / combine / final ≈ a few hundred bits).

## PERSPUV and RCP24 — confirmed on the brief's rewrite list

**`zhao_raster_perspuv_svc`** (2,204 / 3,293 / 1 / 6 DSP, internal 99.14 MHz):
- `:125-132` — the 16-entry token table `e_have/e_num/e_mant/e_k/e_q/e_tag [NTOK]`, all in
  the async-reset process `:234`. §8.1 deletes it outright: after RCP the job is fixed
  latency and needs no table, no oldest-work scan and no completion scan.
- Its **current worst internal path is that table** — `pk_i~5 → p1_prod_q[0][36]`, the
  16-entry priority scan feeding the multiplier. `TEXTURE-ISLAND-FIT.md` names it and says
  it was "deliberately left alone here. It is the next thing to take."
- `:183, :188, :193, :205, :209-211` — `logic signed [63:0]` products, sums and rescales.
  §8.3 requires the proved **signed-56** domain with a static assert: *"Do not keep 64 bits
  merely because the oracle used uint64 storage."* This shrinks a 64-bit add, a 64-bit
  variable arithmetic shift and two 64-bit magnitude compares.
- The 6 DSPs stay. They are the bought throughput (1.00 → 1.99 products/clock), and §18.3
  forbids optimising them before the free DSPs are recovered elsewhere.

**`zhao_raster_rcp24_svc`** (1,041 / 1,101 / 0 / 6 DSP, internal 73–80 MHz):
- `:138-147` free-context scan; `:156-168` ready/pick scan — §7.3 replaces both with
  `FREE_CTX_FIFO` / `NEW_JOB_FIFO` / `CONT_JOB_FIFO` / `RESULT_FIFO`.
- `:148` `assign v_ready_o = free_v;` — a **ready output driven combinationally from a
  table scan**, which is §2.6's backpressure law directly.
- `:170` `logic [63:0] mul_b_c` — the accidental 32×64. §7.2's proof: the exhaustive audit
  measured max `w = 0x401FEF88 < 2^31`, so `t = 2^31 − w` fits 31 bits and both phase
  families are unsigned 32×32. Tripwire: **reject DSP > 4.**
- `:115-117` — a 24-iteration leading-zero loop. §7.4 requires a balanced tree.

---

# CORRECTIONS TO THE MEASURED BASELINE

## Correction 1 — the island figure double-counts a superseded block

The 18,497 / 28,143 / 10 / 25 sum includes **`zhao_texture_tmu`**, which
`design/prod_manifest.yml:162` marks explicitly:

```
- zhao_texture_tmu: superseded  by zhao_texture_tmu_pipe
```

It is not in the production island. The prototype baseline of 15,749 does not contain it
either — §1.1's ten rows are `aux_pipe, texjoin_v2, perspuv_svc, rcp24_svc, cache_pipe,
mosaic, tmu_plan, bilerp_lane, palette_res, rsp_dispatch`. **Comparing an eleven-block sum
against a ten-block sum measures the membership**, which is the project's own
compare-like-with-like law in a new costume.

Like for like, the same ten modules:

| | ALM | reg | M10K | DSP |
|---|---:|---:|---:|---:|
| prototype (§1.1) | 15,749 | 25,123 | 11 | 16 |
| **as built today** | **16,576** | **27,793** | **10** | **19** |
| delta | **+827 (+5.3%)** | **+2,670 (+10.6%)** | −1 | +3 |
| spec target | 6,600 | 6,050 | 37–64 | 10–13 |
| hard redline | 7,500 | 9,000 | 64 | 14 |

**The verdict does not change — the rebuild is still worse than the prototype on ALM,
registers and M10K — but the magnitudes do.** ALM is 2.21× the redline, not 2.47×;
registers 3.09×, not 3.13×; DSP **1.36×, not 1.79×**.

And the three extra DSPs are **on plan, not a defect**: they are perspuv's second product
lane, which §8.6 authorises explicitly — *"a deliberate DSP increase relative to the
current one-product lane, paid for by DSP reductions in RCP, Mosaic and bilerp."* Those
reductions (R4, R5, R6 = −9 DSP) are simply unimplemented. The DSP column is on schedule;
the ALM and register columns are not.

## Correction 2 — the island's real Fmax floor is a block nobody has measured

`zhao_texture_tmu` fits at **36.11 MHz**, 1,921 ALM, **350 registers**, 6 DSP. It appears
in no report — `TEXTURE-ISLAND-FIT.md`'s table lists `tmu_plan`, not `tmu` — and it is
absent from `INTERNAL-PATHS.txt`, so there is no internal/boundary split for it and 36.11
may be largely its 496 virtual pins. It is superseded, so this is not a crisis. But:

**`zhao_texture_tmu_pipe`, the block that replaces it and the one named in
`prod_manifest.yml:50`, says this in its own header:**

> `INCOMPLETE. NOT INSTANTIATED ANYWHERE. DO NOT WIRE THIS IN.`
> This is the front half of the v2 sampler…

It **has never been fitted** — it is not in `zhao_block_fit.json` at all. So the production
island's primary sampler is half-built and entirely unmeasured.

## Correction 3 — the brief's §3.3 budget has no line for the sampler

§3.3's eleven components total 6,600 ALM and include "binding tables + TMU planner v2"
(700 ALM) but **no row for the TMU sample datapath itself.** The block it replaces costs
1,921 ALM and 6 DSP. Much of that is likely its internal `FILT_LANES=2` bilinear hardware,
which §13 moves out into `bilerp_sched` at one DSP — so the residual sampler may be small.
But it is not zero and it is not budgeted, and with perspuv 6 + rcp 3–4 + bilerp 1 = 10–11
of a 10–13 DSP target already committed, there is little room. **This belongs on §27's
open-items list and is not there.** Flag it to the owner; do not invent a number.

## Correction 4 — a rule the repo states two ways, and the tool that was taught the wrong one

`QUARTUS_GOTCHAS.md` §10 rule 2 reads: *"Do **not** touch the array from a reset branch."*

`zhao_texture_cache.sv:495-523` records a measured A/B that is **stronger**: the array was
never in a reset branch. It was merely written inside `always_ff @(posedge clk or negedge
rst_n)`, and that alone cost 5,402 ALM and zero M10K until the write moved to a clock-only
process.

Worse, §10's own tooling note records that `scan_rtl.py`'s `resetTouched` detector was
"fixed" on 2026-08-24 precisely to **stop** flagging arrays *"written in the operating
logic of any `always_ff` with an async reset"* — i.e. the exact pattern that broke the
cache. **The static scanner was taught not to detect this, and eleven days later the
rebuild reintroduced it and nothing caught it.**

Recommended, and cheap:
1. Restore the detector as a **separate, lower-severity** `ASYNC_RESET_PROCESS` finding,
   distinct from `RESET_BRANCH`, so the two are not conflated again.
2. Take the one calibration point the 34-template grid never took: sync read, array **not**
   in the reset branch, write process with `or negedge rst_n`. One microbench via
   `tools/budget/gen_calib.py` settles a rule the repository currently states two ways.
3. Amend §10's rule 2 to read *"do not write the array from a process with an asynchronous
   reset"* — which subsumes the reset-branch case.

R0's fit-target rules catch this at the fitter regardless of the scanner. Both are worth
having; the scanner is the cheap one and catches it before an hour of Quartus.

---

# THE TWO TIMING PLANS, RECONCILED

## Where the clock actually is

| | value | source |
|---|---|---|
| composed reduced renderer, mean of 3 seeds | **96.87 MHz** (95.45 / 95.66 / **99.50**) | `MHZ-PASS-SUMMARY.md` |
| **measured placement noise floor** | **±4.61 MHz** / 0.527 ns | `NOISE_FLOOR.md` |
| honest statement | **96.9 ± 4.6 MHz** | — |
| product clock | 100 | brief §3.1 |
| full-composition acceptance floor | **105** | §3.1, and ShellFixes' bar independently |
| full-composition objective | **110** | §3.1 |
| stretch / headroom | 115 | §3.1 |
| texture-survivor island, standalone | **115–120**, three seeds | §21.6 |
| major texture islands | 120–125 | §3.1 |
| individual leaves after stable design | ≥125 (130–150 preferred) | §3.1 |

**The current 96.87 MHz does not contain the texture path.** The docket is explicit: 45
sources in the QSF, zero for TMU v2, cache, TEXJOIN, AUX or Field/Earth. That is why the
composed target is 110–115 and not 100 — the margin is pre-spent.

## `MHZArchitected` — the RENDERER plan. Substantially DONE.

53.48 → 96.87 mean / 99.50 best, **+81%**, +797 ALM, zero DSP, every pixel CRC
bit-identical. Sixteen rounds of RTL bought +41.3 MHz; **one QSF line
(`OPTIMIZATION_MODE BALANCED → HIGH PERFORMANCE EFFORT`) bought +2.08 MHz more**, measured
paired, same seed, all three positive.

Its five named offenders, finally accounted:

```
FRAGMENT   fixed twice, largest single contributor
EDGEWALK   fixed FOUR times, a different tail each time
EARLY-Z    fixed twice
BINNER     absent from every worst-100 for eleven fits; appeared at ~95 MHz
FBWRITE    never appeared at all, in sixteen fits
```

**Two of five were named from reading RTL and never confirmed by a measurement.** Step 6
(FBWRITE fixed rows) should not be executed on the note's authority alone.

Still open from it:
- **Step 4, the Fragment RMW split + address CAM** — designed and costed in
  `FRAGMENT-RMW-SPLIT-DESIGN.md`, **blocked on an owner tradeoff**: (a) accept a bubble on
  ~31% same-address traffic, knowingly amending *"initiation rate may not regress"*;
  (b) pay for true forwarding with a deeper pipe and a multi-entry scoreboard; (c) measure
  the clock problem first. The note's own "small in-flight address CAM" **does not work** —
  forwarding `wr_data_o` into stage A's tests recreates the long path in a new shape.
- **D3, the fit-top split** (3,214 virtual pin bits, 1,608 ALMs holding them).
- **A sixth offender not on its list at all:** `gpu_clk~CLKENA0` at **13,682 fanout** with
  1.995 ns of launch/latch skew. No datapath pipelining recovers skew. Unmeasured.

**And the finding that supersedes most of the remainder.** At 96.87 MHz, three seeds
produce three different limiter blocks; `zhao_geom_binner` is −0.365 with 81 paths at one
seed and *positive* at another. **No single block limits this design.** The last ~3 MHz is
spread across cmd_dma, binner, Early-Z and tilestore with no dominant structure, each
candidate costs 2–3 fits (3–5 h) to distinguish from noise, and the honest answer will
usually be "inconclusive". The two remaining structural targets — Early-Z's 256:1 presence
lookup and tilestore's presence mux, ~2.6 ns of encode-plus-mux — are RMW over 256 entries
with a late address, and **Early-Z's fix was already rejected by the reference model in
round 12** (8 decisions diverged, because `zref::EarlyZ` promotes the floor in the same
cycle and the next fragment sees it).

## `ShellFixes.md` — the SHELL plan. All three items already answered.

Written against the shell-only fit at 83.4 MHz, whose real synchronous paths were CMD.DMA
header validation (−0.875 ns, ~92 MHz) and the record framer (−0.765 ns, ~93 MHz), with
the 83.4 figure itself an artefact of a monitored `vid_clk → gpu_clk` crossing.

| item | status |
|---|---|
| 1 starvation-counter CDC → snapshot mailbox | **done** |
| 2 CMD.DMA header → `crc_pay_r` dependency cut | **done — in `M_SEED_PREP`, NOT the document's `M_HCRC`** |
| 3 record-framer streaming rewrite | **partly** — `pkt_len − 4` hoisted; the parser rewrite is not done |

**Two of them improved on the document rather than following it, and recorded why:**
- Item 2's suggested home *measured worse* — seeding at the end of `M_HCRC` took
  −0.423 → −0.621 ns and 16 → 60 failing endpoints, because that state runs
  `crc_hdr_r <= fold_o` and the write landed in the CRC fold's shadow.
- Item 1's real justification is stronger than the document's: that crossing's hold slack
  read −0.952 / +0.254 / +0.259 / −0.728 **across four fits that touched nothing in that
  path** — 1.2 ns of swing on placement alone, making the shell's verdict *nondeterministic*,
  which is worse than permanently red.

**None of the three appears in the current composed worst-100.** The renderer owns every
failing path. Whether item 3 is needed at all is not measurable until the renderer stops
dominating.

## Overlap, conflict, and the combined critical-path story

**They do not conflict on substance.** Both adopt *"latency may grow; initiation rate and
exact arithmetic may not regress"*; both forbid false paths and multicycle constraints on
logic genuinely sampled every clock; both forbid seed fishing; both demand one measured
change per commit; both set an acceptance bar well above WNS = +0.001. `islandrearchitecture5`
§2.2 / §2.6 / §2.8 restates the same three laws for the texture island, and its 105 MHz
floor is the same number ShellFixes reached independently from +0.5 ns at 10 ns.

**The one apparent conflict resolves cleanly.** ShellFixes says *"do not start by turning
up fitter effort"*, citing a measured shell experiment where HIGH PERFORMANCE made the
worst path *worse* (−0.639 → −1.389 ns) and created two hold failures. The MHz pass then
measured HIGH PERFORMANCE buying +2.08 MHz on the composed renderer. **Both are true; they
measured different designs at different congestion.** ShellFixes' actual rule is ordering —
close the RTL under BALANCED first, then treat effort as an experiment — and that is
exactly the order events took: the effort change landed *after* sixteen rounds of RTL work.
Keep the rule.

**The combined story is three cones, and only one of them is measured.**

1. **The composed reduced renderer** — 96.9 ± 4.6 MHz, no dominant owner, ~3 MHz spread
   thin, plus an unmeasured 1.995 ns clock-enable skew. *Lowest-value work available.*
2. **The shell** — real WNS unknown since the renderer began dominating; its three named
   paths absent from the current worst-100. *Not actionable until the renderer clears.*
3. **The texture island — not in the composed fit at all**, internal floor `texjoin_v2`
   93.12 MHz against a 115–120 MHz island gate, with its primary sampler committed
   incomplete and never fitted, and 2.2× its ALM redline.

**Cone 3 is the whole job.** The recovery plan above is the prerequisite for the only
number that matters, and it improves timing as a side effect: R1 removes a 9,728-bit flop
array and its mux trees, R2 removes a 7,056-bit one, R3 deletes the 16-entry priority scan
that is perspuv's own current worst path, and R4 removes three table scans and a ready
signal driven combinationally out of one of them.

---

# WHAT IS ALREADY DONE — do not re-plan it

From the docket's DONE table and the git log:

- **D1 rounds 0–16, 53.48 → 96.87/99.50 MHz** (`c23a5ef`, `6e549ef`, `ce84b10`, `43bf8a0`,
  `adeaa52` …). Nine faults found, all by a fit naming a path — **not one by reading RTL.**
- **D2 ENGINE0 route tripwire** — `c23a5ef`.
- EDGEWALK's serial 16-bit popcount → balanced tree, fed from a registered row mask
  (`TIMING_HAZARD_SCAN.md` RESOLVED 2026-09-01, at `b3bd69b`).
- All three `ShellFixes.md` items (2 fully, 1 partly) — see above.
- `perspuv_svc` two product lanes, 62.67 → **99.14 MHz internal**, 1.00 → 1.99
  products/clock (`056e37c7`, `8faaa240`, `9391399e`).
- `texjoin_v2` 13-level priority scan → work FIFO, 61.66 → **93.12 MHz** (`8de11b1b`).
- `tmu_plan` narrowed to `MAXLOG2` — **−0.690 → +0.956 ns** on the targeted cone, ALM
  1,419 → 1,142 (`1831e10f`).
- `aux_pipe` input boundary registered; internal **120.37 MHz** (`e85da610`).
- `cache_pipe` C0–C4 rebuild (`bb46109b`) — **the pipeline is right; only the storage
  coding style is wrong.** It also caught two real bugs: the memory output register being
  overwritten every clock and read two stages later, and `valid` sampled one clock after
  the tag.
- All ten island leaves fitted; `internal_paths.py` separates internal from virtual-pin
  boundary; `run_block_fit.ps1 -Seed`; `fit_targets.yml` source closure + preflight;
  2000-path reports; production manifest + `zhao_prod_top` (`0e8b1c9d`).
- The **noise floor measured at 4.61 MHz** (`NOISE_FLOOR.md`) — which retired the fictional
  "~1.5 MHz" used for fifteen rounds and re-read five of them.

**Deliberately not started, correctly:** `RASTER_Polygon_Budget_Proposal.md` (a proposal,
sequenced after a composed fit); FBWRITE (never appeared in sixteen fits); the Fragment RMW
split (blocked on an owner decision).

---

# WHAT THE BRIEF REJECTS

§24 `REJECT` — **a plan violating any of these is worthless:**

- integrating the ten first-pass leaves and accepting a 55–80 MHz console;
- relaxing the product clock or material capability before rearchitecture;
- dropping three-sample capability;
- building a second general TMU;
- **keeping cache lines in registers** ← *the current state of the tree*;
- hiding paths with multicycle or false constraints;
- using a lucky seed as architecture;
- assuming DSP packing from source syntax;
- **adding terrain/Field RTL faster than the texture fit can be closed.**

§10.2 `Do not add` to the cache: associativity, PLRU, dirty state, writeback, coherence,
**MSHRs**, hit-under-miss, speculative fill bypass. *"Those are trace-driven future options,
not repairs for the measured problem."*

§25 stop-conditions: if M10K does not infer, **stop** — do not optimise logic around flops.
If the hit path passes but the fill path fails, pipeline the fill bookkeeping — do not add
an MSHR. If TEXJOIN's work-FIFO version stays below 100, do not iterate the monolith.

Deferred until a committed legal trace exists (§13.4, §17.3, §17.6, §24): a second bilerp
lane, a second RCP lane, one MSHR, more than 8 palettes, more than 32 bindings, the UV alias
optimisation, perspective multiplier decomposition below the straightforward 6-DSP pipe.

§3.4, restated because it is the rule that was broken this morning:

> **A fit that meets Fmax while violating its memory/DSP structure is not a pass.**

---

# OPEN ITEMS FOR THE OWNER

1. **`FRAGMENT-RMW-SPLIT-DESIGN.md`'s (a)/(b)/(c)** — outstanding since 2026-08-31. Note
   that the noise-floor result argues for **(c)**: at 96.87 MHz with no dominant owner, the
   split's expected value is unmeasurable, and the 1.995 ns clock-enable skew is both
   unmeasured and cheaper to investigate.
2. **The TMU sample datapath has no line in §3.3's budget** (Correction 3), and
   `zhao_texture_tmu_pipe` is committed incomplete and never fitted.
3. **`QUARTUS_GOTCHAS.md` §10 rule 2 understates the measured law** and `scan_rtl.py` was
   taught not to detect the pattern that broke the cache (Correction 4).
4. **`design/fit_targets.yml` carries no resource rules at all** — C2 is unimplemented, and
   it is the cheapest item in this entire plan.
