# G1-D — the composed texture island

**Date** 2026-09-05
**Top** `fpga/rtl/texture/zhao_texture_island_top.sv`
**Function** `tests/texture/island_composed_directed.cpp` — 11/11 checks
**Capacity** 7,720 ALM / 69.05 MHz / 17 DSP / 18 M10K — §4

G1-D has two halves and they answer different questions. **Capacity** is the
fit: how much silicon the island costs once its blocks are wired to each other
instead of to pads. **Function** is whether the wiring is right at all. This
report is written function-first, because the function half found four defects
that the capacity half would have priced without complaint.

---

## 1. Why the existing island number was never the island's size

Every figure quoted for the island so far — including the 7,913 ALM in
`G1-ISLAND-SURVIVORS-20260905.md` — is a **sum of standalone per-block fits**.
Two things make that sum wrong, and they pull in opposite directions, so the
error does not even have a known sign:

* each standalone fit wraps its block in virtual pins and the registers that
  feed them, so every row carries I/O cost that disappears once the block is
  wired to a neighbour;
* nothing is shared across a block boundary — no common control, no merged
  constants, no retiming across the seam.

**The census contains its own disproof.** Totalled by row it comes to
**342 DSP blocks against the device's 112**, which is impossible. The rows
include mutually exclusive variants (`@lanes2`, `@lanes1-io`, `@pre-rearch`,
`svcseed2/3`) and no cross-block packing. A number that cannot be true is a good
place to stop adding.

`zhao_prod_top` does not answer this either, and its own header says so: it
drives every block from a **separate LFSR**. The roadmap's phrasing is exact —
"a resource-counting harness, not the console".

## 2. What the composed top is

```
  depth ---> RCP24 ---> PERSPUV ---> FRAGROB ---> COMBINE.V1 ---> out
                                       |  ^
                             tmu req   |  | sample responses
                                       v  |
                       TMU_PLAN -> CACHE_PIPE -> RSP_DISPATCH
                                                   |      |
                                             bilinear   palette
                                             BILERP     PALETTE_RES
                                       |  ^
                             aux req   |  | aux responses
                                       v  |
                                    AUX_PIPE

  MOSAIC sits on the pre-TMU u/v path.
```

Every internal signal is a real connection between two island components.
Tie-offs exist only at the island's true external boundary: the fragment stream
in from the rasteriser, the memory fill interface, the palette upload port, the
aux sheet port, and the fragment stream out.

## 3. What the composed test found

The functional test drives fragments in at the boundary and requires **every
block's counter to have moved** on the way out, each named separately so a break
is *located* rather than merely detected. That structure is what made the
debugging tractable: each failure pointed at exactly one hop.

Four defects, found in order, each hidden behind the previous one.

### 3.1 The fill model served one halfword where a line is eight beats

`zhao_texture_cache_pipe` counts beats and only marks a line valid on the last:

```systemverilog
  if (fb_beat_r == BEAT_W'(HW_PL - 1)) ... fb_busy_r <= 1'b0;
```

with `HW_PL = LINE_BYTES / 2 = 8`. The harness answered each fill with one
halfword, so the cache waited forever, back-pressured the planner, and nothing
reached the dispatcher. Reported as `cache miss 1, dispatch 0`. **A harness
defect, not RTL** — but it is recorded because a composed test's memory model is
part of the evidence, and a wrong one condemns working hardware.

### 3.2 AUX_PIPE's token was too narrow to carry the identity

FRAGROB validates every response against the slot **and generation** it issued:

```systemverilog
  assign aux_ok_c = aux_rvalid_i && val_q[aux_rslot_i] &&
                    (gen_q[aux_rslot_i] == aux_rgen_i) && auxreq_q[aux_rslot_i];
```

That is `$clog2(DEPTH) + GENW` = **12 bits**. `AUX_PIPE`'s `TOKW` defaults to
**8**, so the identity could not round-trip. `TOKW` is a parameter; it simply
had to be told how wide the identity is.

### 3.3 The sheet token was left unconnected — and it stalled the whole island

The first draft wrote `.sheet_tok_o()`. AUX_PIPE matches a sheet response to the
request that asked for it by that token, so with it dangling the responder had
nothing to echo and **every** aux result carried a wrong identity. FRAGROB
rejected all of them and counted it: 7 ID errors against exactly 7 aux requests.

The consequence is the part worth remembering. **FRAGROB retires in allocation
order**, so a single aux fragment stuck at the head blocked everything behind
it. The observable state was:

```
  plan 48 | cache hit 48 | dispatch 48 | bilerp 48 | fragrob 16 | retired 0
```

— a completely healthy sample path, 48 texels fetched and filtered, and **zero
fragments out**. Nothing in that picture points at the aux port. The per-block
counters did.

### 3.4 The recipe was not travelling with the fragment

The combiner's `recipe` / `weight` / `sample_count` were wired from the island's
**input ports**. With a reorder buffer in between, "the fragment arriving now"
and "the fragment retiring now" are different fragments *by construction*, so
every fragment was combined with somebody else's recipe. They now ride FRAGROB's
context word:

```
  [15:0] tag   [21:16] rsv   [23:22] sample_count   [26:24] recipe   [34:27] weight
```

**This one would have passed every handshake test ever written.** A wrong recipe
is a wrong picture, not a stall: counters move, fragments retire, throughput is
nominal. Only a composed test that cared about the *result* could see it, and it
is the clearest argument in this whole gate for why the roadmap demands a test
that actually draws through the added hardware.

### 3.5 And one found while writing the top: FRAGROB was returning sample 0

Not a wiring defect — a block defect, upstream of where anyone was looking.
FRAGROB banks all three sample results internally (`res_rgb_m [3][DEPTH]`) and
its retire read was:

```systemverilog
  out_rgb_r <= res_rgb_m[0][head_slot_c];
```

Banks 1 and 2 had no reader. That is precisely the *"returns sample 0 for every
recipe"* fault `MATERIAL.RESOLVE.md` attributes to the surviving TEXJOIN, living
one block earlier in the chain. FRAGROB now exposes `o_s_rgb_o[3]` /
`o_s_a_o[3]`; `o_rgb_o` / `o_a_o` are unchanged, so every existing consumer
still works.

**The near miss is worth recording.** The first draft of the top held two past
retirements in a shift register and called it a sample bank, because FRAGROB
appeared to expose only one colour. That version would have synthesised, fitted,
reported an ALM count, and blended every fragment with its two predecessors
while calling it three-sample material.

### 3.6 Result

```
  submitted 64, retired 64, fills served 8
  rcp 64 | persp 64 | plan 192 | cache hit 192 miss 1 | dispatch 192
  bilerp 192 | palette 0 | mosaic 64 | aux 41 | fragrob 64
  fragrob ID ERRORS 0
  [island_composed_directed] 11 checks passed
```

`palette 0` is expected and is not a gap in the test: the composed top tags every
request with the bilinear class, so the CLUT path is wired and idle. Exercising
it needs a class-varying workload, which belongs with the seam in §5.3.

## 4. Capacity

### 4.1 Analysis & Synthesis — complete

```
Analysis & Synthesis Status : Successful - Sat Sep 05 06:31:07 2026
Top-level Entity Name       : zhao_texture_island_top
Logic utilization (in ALMs) : N/A          <- synthesis never reports ALMs
Total registers             : 11,613
Total virtual pins          : 889
Total block memory bits     : 25,872
Total DSP Blocks            : 22
```

**The virtual pin count is the direct evidence for §1.** 889 pins for the whole
island, because only its external boundary is exposed. Eleven standalone fits
each pinned out their own full interface — `zhao_texture_fragrob` alone declares
54 ports — and every one of those pins carries registers that exist solely to
feed a pad that will not be there. That cost is in the standalone sum and is not
in the island.

**22 DSP of 112.** Note this is NOT comparable to the "14 DSP" quoted from the
standalone texture rows: the composed island also contains RCP24 and PERSPUV
(6 each), which that total omitted. Like-for-like comparison waits on the
per-block breakdown in the fitter report.

### 4.2 What inferred as memory, and one thing that did not

Thirteen memories inferred, all Simple Dual Port:

| RAM | depth x width |
|---|---|
| `palette_res mem_r` | 1024 x 16 |
| `cache_pipe data_r` x4 lanes | 128 x 16 |
| **`fragrob desc_u_m[2][0][31]__1`** | **16 x 228** |
| `fragrob ctx_m` x2 | 16 x 64 |
| `fragrob auxrgb_m` / `auxa_m` / `axg_m` / `axq_m` / `order_m` | 16 x 16/8/8/4/4 |
| `aux_pipe sd_tok` | 16 x 12 |

The 228-bit-wide one is worth reading carefully: **Quartus merged FRAGROB's
`desc_u_m`, `desc_v_m` and `desc_met_m` into a single memory** — 3 samples x
(32 + 32 + 12) = 228 bits exactly. The survivors report listed those three
separately as "wide payload"; they are one array now, and they are in memory.

**`res_rgb_m` and `res_a_m` are NOT in that list.** 3 x (24 + 8) = 96 bits per
slot, 1,536 bits total, still in flip-flops — and that is plausibly a cost of
the §3.5 fix. Exposing the three sample banks added a second, COMBINATIONAL read
of those arrays at `head_slot_c`, and combinational logic between an array read
and the first register is exactly what blocks absorption into an M10K's output
register (QUARTUS_GOTCHAS §14).

So the sample-bank fix may have bought correctness at the price of 1,536 bits
staying in flops. **Recorded as a suspicion with a mechanism, not a
measurement** — the pre-fix composed synthesis was never run, so there is no
before-figure to difference against, and inventing one would be the
"declared-today versus measured-a-week-ago" error. The way to settle it is a
registered output stage on those reads, which is the same remedy §5 of the
perspuv report proposes for `e_q`.

### 4.3 Fitter — COMPLETE, AND NOW STALE

> **STALE A SECOND TIME, AND A REFIT IS RUNNING** (started 2026-09-06). Since
> the note below was written the island also gained the R6 ORDERING BOUNDARY:
> a submission sequence stamped at admission, an FCTXN-entry reorder buffer at
> the output with a 64-to-1 mux of 33 bits on the emit path, and the combiner
> tag widened from 16 to 22 bits to carry the sequence. That last one touches
> the record file in every one of MATERIAL.COMBINE.V1's slots, so it moves area
> INSIDE the closure as well as at the island level. Again no prediction is
> offered, for the same reason as below.
>
> **These numbers no longer describe the tree.** They were measured at
> `afb7070f`, before the ingress-capture repair. Since then the island has
> gained a 64-entry per-fragment attribute table with combinational LUT-RAM
> reads on PERSPUV's input path, a typed token field through FRAGROB, per-slot
> class and palette-binding tables, four new output ports, and an LODW change
> from 4 to 8. Every one of those moves area, and the last two move the pin
> count that 405 of the 2,000 summarised paths already start at.
>
> **No prediction of the new figures is offered.** This island's last obvious
> explanation was worth 4 MHz of 36. The re-fit is deliberately batched behind
> the corrected combiner leaf fit, per the owner's v2 priority 5: *"Do not
> launch an expensive complete-island refit after each speculative glue edit."*
>
> The comparison table below is kept because the RELATIONSHIP it records —
> composed against nominal, redline, and the standalone sum — is what the next
> measurement must be read against. The absolute numbers in it are history.
>
> One thing here transfers directly: this fit needed 9,238 s against a 14,400 s
> budget. The combiner leaf fit was failing at `[int]$TimeoutSeconds = 3000`
> and reporting it as a fitter failure, twice, before that was diagnosed.



```
status        ok
seconds       9238            (2h34m, budget 14400)
sourceCommit  afb7070f
alms          7720            of 41910
registers     11790
blockMemoryBits 25872
ramBlocks     18              of 553
dspBlocks     17              of 112
virtualPins   889
fmaxMhz       69.05           clk
```

**The third attempt produced it.** The first was killed at a 7,200 s watchdog;
the second was killed on purpose because it had snapshotted a top that never
retired a fragment. This one ran 9,238 s — 2h34m — so the two-hour budget was
never going to be enough and the four-hour one had about 86 minutes to spare.

| | ALMs | vs composed |
|---|---|---|
| architecture nominal (§3.3) | 6,600 | **+1,120 (+17.0%)** |
| **redline** | **7,500** | **+220 (+2.9%)** |
| sum of standalone per-block rows | 7,913 | −193 (−2.4%) |
| **composed** | **7,720** | |

### 4.4 THE CENTRAL ARGUMENT OF THIS REPORT IS LARGELY REFUTED

§1 argued that the standalone sum is not the island's size, and gave two
reasons: virtual-pin cost that disappears when blocks are wired to each other,
and no cross-block sharing. §4.1 then read the 889-pin count as *evidence* for
the first.

**The measurement says that saving is 193 ALMs — 2.4%.** The composed island is
very nearly the sum of its standalone parts.

The reasoning was not wrong in kind; it was wrong in MAGNITUDE, and the
difference matters because the sum was being treated as unusable rather than as
approximate. A 2.4% correction does not change any decision the 7,913 figure
would have driven. **Had this gate been skipped on the grounds that the sum was
"meaningless", nothing would have been learned that the sum did not already
say — about area.**

What the sum genuinely could not say is the other two columns:

* **fmax, which only exists composed.** 69.05 MHz is not a number any per-block
  row contains, and it is the finding that matters most below.
* **DSP, where the sum is genuinely broken.** The census totals 342 against a
  device with 112 — impossible, as §1 said. The composed island uses **17**.
  Synthesis had reported 22, so the fitter packed five away; a standalone-row
  sum can represent neither the packing nor the mutually exclusive variants.

So §1's conclusion survives for DSP and for fmax and does not survive for ALMs.
That is worth stating plainly rather than quietly re-scoping the claim, because
the next person will otherwise inherit a rule of thumb — "standalone sums
overstate" — that is 2.4% true.

### 4.5 The two findings that DO change what happens next

**1. The island is over its redline, and only just.** 7,720 against 7,500 is
+2.9%. That is not a rounding error and it is not a crisis; it is a number a
survivors decision can act on, and it is the first time that decision has had a
real one. The 10% fabric reserve on a 41,910-ALM device is 37,719 ALMs, so the
island at 7,720 is 18.4% of the device and 20.5% of the reserved budget.

**2. fmax is 69.05 MHz against a 105 MHz composed acceptance floor.** This is
the serious result. It is not marginal — it is **34% short**, and it is well
below the shell's own 99.34 MHz, which is itself under owner decision D19j for
being 5.66 MHz short.

### 4.6 The critical path, read rather than guessed — and it is RCP24

The fit's own reports were archived to `reports/synthesis/blockpaths/`, so the
path was read instead of speculated about.

**Worst path overall, slack −4.482 ns:**

```
From  frag_depth_i[19]                          (a top-level VIRTUAL PIN)
To    zhao_raster_rcp24_svc:u_rcp|c_x[7][8]
      14 logic levels · data delay 17.763 ns
      interconnect 11.153 ns (63%) · cell 6.610 ns (37%) · worst single hop 3.871 ns
```

All twelve worst paths start at that same pin bit and land in RCP24's context
array. **The obvious reading is that the boundary is the problem** — a virtual
pin is placed as arbitrary logic, so 63% interconnect from one looks like an
artefact of the measurement rather than a property of the design.

**That reading is wrong, and the same report refutes it.** Splitting the 2,000
summarised paths by where they start:

| paths starting | count | worst slack | implied fmax |
|---|---|---|---|
| at a virtual pin | 405 | −4.482 ns | 69.05 MHz |
| **inside the design** | **1,595** | **−3.63 ns** | **~73.4 MHz** |

If every virtual-pin path were pure artefact and vanished, fmax would move from
69.05 to about **73.4 MHz**. Still **31.6 MHz short** of the 105 MHz floor. The
boundary is worth roughly 4 MHz; it is not the finding.

**The finding is `zhao_raster_rcp24_svc`.** Every one of the worst internal
paths is the same shape:

```
zhao_raster_rcp24_svc:u_rcp|c_val[6]~DUPLICATE
  -> zhao_raster_rcp24_svc:u_rcp|c_m.raddr_a[1]~6_O
```

— the context-valid bit driving the context memory's READ ADDRESS. A valid bit
computing an address is a long combinational hop into a memory's address port,
and `~DUPLICATE` says the fitter already replicated the register trying to
shorten it.

**This is exactly what a composed fit is for.** RCP24 measures fine as a leaf;
it becomes the island's limit only once it is placed among ten other blocks
competing for the same fabric. No per-block row contains this.

**Third time today the flattering reading was the wrong one.** The DSP overrun
looked like the `multstyle` attribute being ignored and was fourteen
multipliers I had written; the standalone sum looked meaningless and is 2.4%
off; the critical path looked like a virtual-pin artefact and is RCP24. The
pattern is consistent enough to be worth naming: **the first explanation that
absolves the design is the one to check hardest.**

### 4.7 What this does NOT establish

* **Not that RCP24 is badly designed.** It says the `c_val` → `c_m.raddr_a`
  path does not close at 100 MHz *in this composition*. Whether that is
  RCP24's structure, its placement among ten neighbours, or the absence of a
  pipeline stage the composition needs is not answered here.
* **Not that 105 MHz is unreachable.** One path is named; the work of closing
  it has not been attempted.
* **Not a comparison with the shell's 99.34 MHz.** Different design, different
  constraint set. Putting the two numbers side by side would invite exactly the
  mismatched-pose comparison `CLAUDE.md` warns about.

## 5. Seams that remain, named rather than hidden

### 5.1 Texel-to-channel

`RSP_DISPATCH` hands the bilinear class 64 bits — four RGB565 texels.
`BILERP_LANE` consumes four **eight-bit channel values**. The top extracts one
channel; the three-channel sequencing the lane is designed for ("serial bilinear
**channel** engine") is not built, so its job counter under-reports by a factor
of three until it is.

### 5.2 Per-sample coordinates

FRAGROB takes u/v/binding/lod **per sample**. The island boundary supplies one
pair; the three samples are given the same coordinates and distinguished by
binding index (offset per sample, so the cache does not serve one line three
times and understate miss traffic). Per-sample coordinate variation is a
binding-table concern this composition does not own.

### 5.3 The response class rides the source id

`RSP_DISPATCH` needs `rsp_class_i`; `CACHE_PIPE` carries no class lane, only
`smp_src_id_o`. The class therefore travels in the **top two bits of the source
id**, which works only because `SRCW` is 16 and live source ids are far below
2^14. That is a real constraint on the id space and it was nowhere written down
before this file.

## 6. Next

1. Read the composed fit and fill §4.
2. If the composed number lands above the 7,500 redline, the survivors question
   reopens — and it reopens with a *real* number for the first time.
3. `zhao_texture_material_combine_v1` still has no measurement of its own; the
   refuted `zhao_texture_combine` stays in the tree until it does.
4. perspuv's context store (see `PERSPUV-REGISTER-DIAGNOSIS-20260905.md`) is the
   largest single register consumer in the island and is inside this fit's
   closure, so it is untouched until the fit clears.

---

## 7. Will this fit finish? — the evidence, and the fallback

Asked directly on 2026-09-05 while the four-hour run was in flight. The honest
answer is **probably, but it is not certain, and the first timeout is a warning
rather than bad luck.**

**Why it is a warning.** Analysis & Synthesis on this design takes about four
minutes. The two-hour run therefore gave the FITTER roughly 115 minutes, and it
still did not finish. For 11,613 registers and 22 DSPs on a 41,910-ALM device
that is slow, so something about *this* design is hard to place rather than
merely large.

**The most likely reason is the 889 virtual pins.** A virtual pin is not free:
the fitter places it as logic. The island's boundary is genuinely wide — a
fragment stream in, a memory fill interface, a palette upload port, an aux sheet
port and a fragment stream out — and every one of those bits becomes something
to place. That is the same cost §4.1 counts as *evidence* that the standalone
sum overstates the island, showing up here as fitter time.

**Settings are already at maximum quality**, inherited from the shell fit:
`FITTER_EFFORT "STANDARD FIT"`, `OPTIMIZATION_MODE "HIGH PERFORMANCE EFFORT"`,
Advanced Physical Optimization on, `NUM_PARALLEL_PROCESSORS 4` on an 8-core
machine. Those were tuned for the shell and are not obviously right for a block
this shape.

### The fallback, and what it would and would not cost

If the four-hour run also times out, the next attempt drops Advanced Physical
Optimization and `OPTIMIZATION_MODE`. **The ALM number survives that** — those
knobs steer timing-driven restructuring, and their measured effect on the shell
was +2.08 MHz, not an area change. **The fmax does not**: it would become a
FLOOR rather than the island's speed, and it must be reported with that word
attached and never quoted as the composed acceptance figure against the 105 MHz
gate.

That is the honest degradation. An area number with a labelled fmax floor is
worth having; a fourth timeout with nothing is not. And raising
`NUM_PARALLEL_PROCESSORS` to 8 costs nothing in quality and is free speed,
which is the first thing to change either way.

### The speed lever I did NOT pull, and why

`NUM_PARALLEL_PROCESSORS` is **4** on an **8-core** machine, and the block fit
inherits it by copying the shell project's QSF. Doubling it is the cheapest
speedup available and it changes no design decision.

**It was left alone deliberately.** That setting lives in the SHARED shell QSF,
so changing it changes the measurement basis for every block that has ever been
fit through this flow — including the shell's own 99.34 MHz, which is currently
under an owner decision (D19j). Quartus is documented to be deterministic across
processor counts, but "documented to be" is not the same as "verified here", and
a settings change that silently makes today's numbers incomparable with
yesterday's is exactly the kind of quiet drift this report exists to argue
against.

If the four-hour run times out and the fallback is needed, raising it is the
first thing to change — **as an explicit, recorded change to the fit flow, with
one block re-fit at both settings to show the numbers did not move.** That is
one extra fit to keep a whole census comparable, which is a good trade.

**What will NOT be done is quoting a number this gate did not produce.** The
census already contains one impossible figure — 342 DSP against a device with
112 — because sums were taken of things that were never measured together. G1-D
exists to replace that with a measurement, and a measurement it did not make is
not an improvement on it.
