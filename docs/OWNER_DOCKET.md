# Owner docket — Zhaozhou

## 2026-08-23 — STOP THE ONE-BLOCK-AT-A-TIME LOOP: build a budget compiler

**Ruling: the next engineering run is a repo-wide audit, not another isolated
rescue.** Every block so far has been chosen because its latest number looked
horrifying. That is reactive, and it has cost roughly a day per block. The
recurring disaster shapes are **mechanically detectable from source** — the exact
DSP/ALM/Fmax still needs measurement, but *which blocks to suspect* does not.

### Verified before accepting the premise

| claim | verdict |
| --- | --- |
| the census is an undercount | **CONFIRMED** — 94 RTL files, **41 measured rows** |
| `GEOM.PROJECT` is unfitted and duplicates `TERRAIN.PROJECT` (33 DSP) | **CONFIRMED unfitted** — not in the report at all; duplication asserted by its own header |
| `FORGE.CLIFF`'s tables cannot infer as RAM | **CONFIRMED** — three `assign x = mem_r[idx]` async reads over ~120 kbit (2048x18, 2048x32, 1024x17), written from an async-reset process; its fit **timed out** |
| a regex can inventory the arithmetic | **REFUTED — by me, twice.** Counting nonconstant multiplies in `geom_project` with grep gave 0, then gave line-counts. Both useless. **This is the argument for an elaborated-AST scanner rather than pattern matching.** |

`design/budgets/dsp.md`'s rule has also been corrected: operator count is a
**lower bound**, not the answer — width and signedness change the cost
discontinuously, as §5's 72-bit/64-bit 28-vs-18 result proves.

### The honest total

    canonical measured census        134
    probable hidden projector       +30..33
    probable hidden pose arithmetic +14..18
                                    -------
    source-level warning total      ~178..185 DSP

**Treat this as a ~180-DSP design that must become 85-90**, not a 134-DSP design
needing twenty more removed. That is worse than it looked and still credible,
because the same pass found enough architectural duplication to cover it.

### The seven recurring failure classes

Every block burned so far fits one or more. **All seven are gateable
mechanically.**

1. **A placeholder throughput becomes physical parallel hardware.** "One item
   per clock" builds six or twenty-eight multipliers before anyone asks how many
   items a frame contains.
2. **Software-shaped combinational functions treated as one cycle.** Loops,
   normalisation, decode→filter, add→compare→saturate, whole state-machine
   finishes.
3. **Things that are memories described as registers and muxes.** Async read,
   reset-touched arrays, dynamic bank selection, giant constant case trees — all
   prevent inference. Field spends 8,901 ALMs and **zero** M10Ks while 502 sit
   idle.
4. **The same arithmetic duplicated in adjacent blocks.** Both projectors;
   Setup/Binner edge starts; ten Field op units; duplicated bilinear weights.
5. **Leaf timing does not represent subsystem timing.** First clocks were
   absent, then I/O paths were absent; even a corrected zero-delay leaf boundary
   is not a real internal seam.
6. **Latency, initiation interval and Fmax confused.** A block can have low
   latency and terrible rate, or good II and miss the clock. Only
   `min(Fmax, target) / II` answers capacity.
7. **Correctness tests validate values, not physical realisation.** Simulation
   cannot tell you that RAM became flops, that a directive was ignored, or that
   narrow multiplies failed to pack.

### The deliverable

**`tools/budget/scan_rtl.py`** — an **elaborated-AST** scanner (Verilator XML,
not regex), emitting one record per module: every nonconstant multiply with
operand widths, signedness, result width and dependency chain; variable
shifts/division/modulo; serial combinational loops; wide add/compare/saturate
chains; repeated calls to identical expensive functions; arrays with total bits,
read style, reset style, bank selection, port count and expected RAM/ROM;
interface shape (direct input→arithmetic→output paths, ready-only-in-IDLE,
dead return-to-IDLE cycles, max in-flight records); and counters whose enable
depends on deep combinational logic. Severity RED/ORANGE/YELLOW/GREEN.

**Quartus calibration microbenches** — stop guessing the multiplier mapping.
Generated modules across widths 8/9/16/18/19/24/31/32/33/40/48/64, signed and
unsigned, combinational / input-registered / input+output-registered, at 1–4
operators. Same for RAM templates: sync vs async read, reset vs none, depths,
widths, byte enables, one and two ports. Then the audit can say *"3 x signed
33x32, input/output registered ~= 9 DSP on this exact tool and device"* instead
of hand-waving from a datasheet.

**Map-only pass over every module at HEAD.** Far cheaper than a full fit and it
already answers DSP inference, RAM inference, ALM explosions and whether
parameters actually elaborate differently. **The 41-of-94 gap becomes a CI
failure rather than a historical footnote.**

**Registered characterisation wrappers** — registered stimulus → DUT →
registered hash sink. Raw leaf blocks with hundreds of virtual pins are poor
physical models. And fit representative **pairs**, because the seam becomes
internal: TMU+CACHE, FRAGMENT+TILESTORE, SETUP+BINNER, TESS+NORMALS,
PROJECTOR+vertex cache, FIELD+terrain intake.

**`design/budgets/workloads.yml`** — executable demand for every expensive
block: invocations/frame, workload mix, measured II, latency, target clock,
reserve. Capacity is generated, not asserted:

    capacity = min(measured_fmax, target_clock) x frame_seconds / measured_II

Field needs distributions rather than a scalar — opcode mix per profile.

**`reports/budget_manifest.json` + `reports/BUDGET_HEATMAP.md`**, carrying per
block: resources at HEAD, expected vs inferred RAM, corrected-I/O Fmax,
WNS/TNS/hold, **critical-path family** (not just the headline number), II by work
type, items/frame, demand ratio, latency, provenance, composition status, and
**debt flags** — `NO_CURRENT_FIT`, `OLD_SDC`, `NO_WORKLOAD`, `NO_II_TEST`,
`EXPECTED_RAM_NOT_INFERRED`, `NO_SUBSYSTEM_FIT`, `NO_RESERVE`.

**Those flags would have exposed Field and the TMU before anyone read their
misleading headline numbers.**

### CI gates — hard failures

* a new nonconstant multiply must appear in the arithmetic inventory with proven
  widths and a resource owner;
* any array above ~512–1,024 bits must declare expected RAM/ROM behaviour;
  async-read or reset-touched storage is **rejected** unless explicitly waived;
* **expected memory but zero inferred RAM blocks fails the fit even if every
  differential passes**;
* a throughput claim with no executable II test fails the ledger;
* an Fmax without corrected clock+I/O constraints is **not recorded as Fmax**;
* an Fmax without a named critical path is not accepted;
* >5 DSPs or >5% ALMs requires two measured Pareto points, or a stated reason no
  cheaper point exists;
* `SYNTHESIZED` maturity requires a current-HEAD map, a corrected fit, zero hold
  failures and provenance;
* a boundary-heavy block requires a subsystem-wrapper fit before integration.

`reports/TIMING_HAZARD_SCAN.md` remains useful but searches only long
combinational **loops**, and says so itself. The scanner must also cover
dataflow depth, memory shape and state-machine rate.

### Predicted red list, to be confirmed not assumed

| area | likely now | plausible target | return |
| --- | ---: | ---: | ---: |
| two projectors (shared core + projected-vertex cache) | ~63–66 | 9–12 | **~51–57** |
| pose arithmetic (serialise quat and matrix lanes) | ~14–18 | 4–6 | 10–14 |
| `TERRAIN.NORMALS` (six 33x33 products, 2,000/frame demand) | 18 | **3** | 15 |
| `GEOM.CULL` (parallel plane dots, demand never derived) | 15 | 3–6 | 9–12 |
| `SETUP`+`BINNER` (share edge starts; `E2 = area2 − E0 − E1`) | 16 | 4 | 12 |
| `RASTER.FRAGMENT` (mux the left operand — two products per channel are mutually exclusive) | 10 | 4–7 | 3–6 |
| `TERRAIN.TESS` (replace the 64-cell dynamic scan with an AND pyramid) | 6 | 2–3 | 3–4 |

From ~180–190 that plausibly lands **~75–95 DSP**.

**Two non-DSP bombs also predicted:** `FORGE.CLIFF`'s three async-read tables
(confirmed above — fix the memories *before* another fit, the source already
gives the answer), and the **pose palette** at 128 tuples x 32 bones x 12
elements x 32 bits = **1,572,864 bits ~= 150 M10Ks, 28% of the device in one
bite.** The current low M10K census creates a false sense of abundance.

### Order — expose before optimising

`GEOM.PROJECT` map+fit → composed `GEOM.POSE` wrapper → `FORGE.CLIFF`
map-only inference check → `FRAGMENT`+`TILESTORE` subsystem fit →
`TERRAIN.NORMALS` → `GEOM.CULL` → `SETUP`+`BINNER` → `RASTER.EDGEWALK` →
`TERRAIN.TESS` → derive the Earth/Patch/Velocity/Field workload → decide the
pose-palette memory frontier.

**This run does NOT optimise blocks**, except where a map cannot complete because
storage is clearly uninferable. Its deliverable is **evidence and ranked work**.

Cost: perhaps a couple of agent-days plus substantial unattended Quartus time.
It replaces weeks of rediscovering, one block at a time, that each "one-cycle"
block was thirty pieces of hardware or one 25-nanosecond combinational sausage.


## 2026-08-23 — TEXTURE.TMU REARCHITECTURE RULING: it is a calculator wearing a
## one-request-at-a-time trench coat

**The TMU architecture is fine. The current RTL fails two independent ways.**
The 28 → 6 DSP result stands and is not implicated in either.

Queued behind the Field waves. **Not started.**

### The two failures

**1. Timing — 36.11 MHz.** Fitted worst path `q_fmt_r[0] → smp_a_o[4]`, **21.432
ns**: a registered format bit through `decode16`, the channel mux, the whole
factored bilinear filter, to the sample output.

**2. Throughput — CLUT initiation interval is 6.** Even at a full 100 MHz that
is **277,778 samples/frame against ~850,000 demanded — 0.33×.** At the measured
36.11 MHz it is **102,556 — 0.12×.** *No clock fix reaches this.*

### The DSP work is exonerated by measurement, not argument

| filter lanes | DSPs | Fmax |
| ---: | ---: | ---: |
| old arithmetic | 28 | 36.92 MHz |
| 4 | 12 | 36.38 MHz |
| **2 (shipping)** | **6** | **36.11 MHz** |
| 1 | 3 | 35.62 MHz |

**Fmax barely moves while DSPs fall 28 → 3.** Multiplier count was never what
made this block slow, so adding arithmetic back cannot rescue it. Registers in
the right places can.

And the 2-lane choice is better justified than "within budget": one lane needs
**2,211,840 filter cycles/frame against 1,666,667 available** — it fails. **Two
lanes is the cheapest filter width that clears the workload.**

### FIRST, fix a stale diagnosis in the repo

`STATUS.md` said the 37.004 ns address-generator path was the honest worst path.
**It is not**, and this is corrected — the figure came from applying I/O
constraints *post hoc* to a database placed with **no I/O objective at all**, so
the fitter had never once optimised those paths. **A post-hoc timing query on an
unoptimised placement is an upper bound, not a fitted critical path.** With the
objective present during the fit, the address generator improves and the
filter/output cone leads.

Left uncorrected, an implementer would pipeline the second-worst cone first.

### The shape

Today: accept one request → compute every address → wait for the texel cache →
maybe wait for the palette cache → decode all texels → run the whole filter →
expose a combinational output → wait for it to be consumed → **only then** accept
another request.

Wanted: request-plan pipeline → small in-flight queue → cache-access conveyor →
registered decode → pipelined two-lane filter → in-order result queue →
registered output.

**Latency may rise from ~5 clocks to ~10–12. That is harmless** if a request can
enter every clock or two.

### Waves

**W0 — baseline.** Re-fit `zhao_texture_cache` under the corrected clock+I/O
SDC (its row predates the repair). Fit a **TMU + real cache** characterisation
wrapper — the seam becomes internal, so that is more representative than giving
each side a zero-delay external environment. Save the top 100–200 setup paths
grouped by family.

**W1 — break the 21 ns filter.** A clocked `zhao_texture_bilerp_pipe`: F0 decode
and register taps/fractions/tags; F1 the two U lerps; F2 the V lerp; F3 round
and register the byte. **Still exactly three multiplies per lane, two lanes, six
DSPs.** Keep the existing combinational `zhao_texture_bilerp` as the formally
proved arithmetic law and add a **latency-equivalence proof** — pipelined output
at N+3 equals the combinational law on input at N — so the clocked
implementation never becomes the only readable statement of the arithmetic.

**W2 — pipeline the address generator.** It is not today's worst path but will
become it. Capture the raw request first, so no request pin feeds deep
arithmetic; then mode/mip, coordinate scale, wrap, address. **Compute `u0/u1/v0/v1`
once and `row0/row1` once** — the present loop computes four wrapped U values,
four wrapped V values and four row shifts where only two of each are unique.
Hold the cache bundle in an issue register, bit-stable until accepted.

**W3 — throughput, and do better than the existing II=2 proposal.** The
contract's two-entry queue gives `100 MHz / 60 / 2 = 833,333` against a true
**829,440** demand — **0.47% headroom and nothing for a miss.** That is a number
that looks like success and is not.

Better: **`TEXTURE.CACHE` has four independent lanes with four independent
addresses resolved in one access**, and the CLUT path uses only lane 0. Pack
them:

    beat   lane 0        lane 1
    0      texel A       —
    1      texel B       —
    2      texel C       palette A
    3      texel D       palette B

A deliberate two-beat lag means **no cache-response-to-cache-request
combinational path**. After warm-up: **one complete CLUT sample per clock →
1,666,667/frame, twice the demand.**

> **One cache amendment is required and must not be fudged:** the access packet
> carries **one scalar source ID for all four lanes**. Packing sample C's texel
> with sample A's palette needs **per-lane provenance** (`acc_lane_src_id_i[0..3]`).
> And **do not identify the TMU sample by the cache's response ID** — carry
> internal record IDs in a parallel access-tag register. Results may complete out
> of order internally; **only the head record may drive `smp_*`.**

**W4 — cache capacity.** The cache is 1 KiB while spending one M10K per lane —
capacity is parameterised and nearly free. Sweep LINES/LINE_BYTES on **real
raster traces**, and choose by **effective samples/frame**, not nominal hit rate.
**Quartus must confirm M10K counts; do not infer them from nominal capacity.**

**W5 — the integration trap.** `RASTER.FRAGMENT` currently receives *already
sampled* texel fields; the real request/rejoin composition does not exist. Without
a **fragment-context FIFO**, the rasteriser will issue one request, wait for it,
and **serialise the new pipeline from outside** — destroying the whole point.
Rejoin by strict queue order or an internal sequence number, **not** by assuming
source IDs are unique.

### Acceptance — none of it optional

| | requirement |
| --- | --- |
| DSPs | **6** |
| standalone Fmax, corrected I/O SDC | ≥110 MHz preferred, **≥100 required** |
| **TMU + real cache composed Fmax** | **≥100 MHz** |
| hold violations | **0** |
| CLUT initiation interval on a hit | **1 clock** |
| direct bilinear II at two lanes | 2 clocks |
| CLUT capacity at 100 MHz | **1.667 M/frame before misses** |
| accept-to-output latency on a hit | ≤16 clocks preferred |
| ordering | **exact acceptance order** |
| arithmetic | **byte-identical to `zref::Tmu`** |
| output under backpressure | stable |
| frontier | 1/2/4 lanes still build and test |

**Explicitly forbidden:** adding filter lanes; restoring the 28-DSP arithmetic;
optimising the 32-bit sample counter; false-path or multicycle exemptions on
functional logic; changing arithmetic rounding; and shipping the II=2 proposal
while calling 0.47% headroom done.

### Verification the sweep must carry

Distinct source IDs in packed lane accesses; lane-0 miss, lane-1 miss, and
simultaneous; issue bundle stable across an entire miss; output stall with the
queue full; queue wrap-around; palette response assigned to the wrong record;
texel/palette lane swap; dropped pipeline valid/tag; filter channel-tag
corruption; early or out-of-order retirement; mode-error attribution with
several requests in flight; hostile disabled cache lanes; and the existing
directed/random/formal suites at all three lane counts.

### Scale

**Larger than the skinning timing fix, substantially smaller than Field.** The
arithmetic is already correct and formally pinned, and the DSP problem is
already solved. **The remaining risk is bookkeeping — tags, queues, stalls and
ordering — which is exactly what the differential and mutation infrastructure is
good at catching.**


## 2026-08-23 — FIELD TIMING REARCHITECTURE RULING: it needs to become a real
## little processor

**The DSP emergency is over for Field. The 79 → 3 architecture stays.** What
33.86 MHz exposes is not an over-ambitious instruction set — it is that Field is
still built like a collection of software functions translated literally into
combinational RTL, rather than like a synchronous FPGA processor.

Queued as the next Field wave. **Not started** — `TEXTURE.TMU` holds the
implementation slot.

### The central finding, verified before anything else was accepted

**Field reports ZERO block memories while consuming 8,901 ALMs.**

    zhao_field_seq:  ramBlocks = 0,  blockMemoryBits = 0,  alms = 8,901

And across all 47 measured blocks the whole design uses **51 of 553 M10Ks —
9%.** There are **502 idle block memories** on the device.

Meanwhile the Field cone contains, in logic:

* a **64×32 register file** — 2,048 flip-flops behind several 64:1 asynchronous
  read muxes, which the contract itself already names as the dominant cost;
* a **256×16** reciprocal seed table, as an `always_comb` case tree;
* a **256×31** normalisation reciprocal table, likewise;
* a **257×17 sine table instantiated TWICE**, to obtain `base` and `next`.

Cyclone V M10Ks are synchronous true-dual-port memories with initialised
contents. This is precisely what they are for.

> **Field is starved for timing while refusing to spend the one resource it has
> in abundance.** That is the architectural mistake now, and it is a much better
> problem than the one we thought we had.

Proposed storage budget: **3 register-file copies + 1 dual-port sine + 1 RCP +
1 RCP24 = 6 M10Ks**, from 502 free.

### Waves, each independently measurable

Deliberately **not** one "fix everything" job. The 500k-token single-agent
pattern is explicitly rejected.

**Wave 0 — evidence.** Preserve the 33.86 MHz netlist. Save the **top 200**
setup paths grouped into `RF_READ`, `CONST_ROM`, `ISQRT`, `SIN`, `ALU_FINISH`,
`NORMALIZE_RESCALE`, `UNIT_REQUEST_MUX`, `RESULT_WRITEBACK`, with cell delay,
routing delay and logic levels for each.

**Wave 1 — NORMALIZE.** The root is proven `< 2^32` (`isqrt(3·2^62) =
3,719,550,786`), so no dynamic shift is needed at all:

    norm32 = h_rt[31:0] << lz32
    m      = norm32[31:8]
    e      = 8 - lz32

replacing `(h_rt >> rsh) << lsh`, which describes two dynamic shifts in series
and leaves the simplification to Quartus. Register `m`/`e` before the ROM
lookup; make RCP24 a synchronous ROM; replace the `resc_s` / `resc_s_fired`
duplication — which computes the same 66-bit rounded value **twice**, once only
for a ledger bit — with one tagged rescale pipeline at one lane per clock.
Narrow `shift_amt` from 8 bits to 6 (proven range 8–39).

**Wave 2 — ISQRT.** Replace the 64-bit add → compare → subtract recurrence with
the digit-by-digit remainder form: `rem_shift = (rem << 2) | pair`,
`trial = (root << 2) | 1`, `diff = rem_shift − trial`, `take = no borrow`. Still
exactly 32 iterations, still exact floor, still bit-identical for every u64 —
but one ~34-bit subtractor instead of a wide compare *and* a wide subtract, and
the new root bit is a concatenation rather than an add.

**Wave 3 — MEMORIES.** Three replicated synchronous simple-dual-port RF copies
(read `a`, `b`, `c`/host), every write broadcast to all three. **Keep one-cycle
clear via a 64-bit `rf_valid` bitmap** rather than walking 64 entries — which is
also exactly right after reset, when M10K contents are undefined. Convert the
sine table to **one dual-port ROM** (port A `base`, port B `next`) and the two
reciprocal tables to synchronous ROMs.

> **Two traps recorded here because they are silent:** "Allow Any RAM Size For
> Recognition" is **disabled** in the current Quartus settings, so a 64×32 array
> may quietly stay in logic — use an explicit wrapper, not hopeful inference,
> and **treat a fit that still reports `ramBlocks = 0` as a FAILED
> implementation even if every test passes.** And prove no same-address
> read/write occurs, then set read-during-write to don't-care, or Quartus
> inserts a bypass network that costs area and speed.

**Wave 4 — registered finish.** A registered result state before the RF write;
`WB0` before the existing `WB1`/`WB2` walk, which removes the selected-unit mux
from the write path at one clock per instruction rather than per lane. For fixed
rescales, replace two wide magnitude comparisons with a **sign-extension fit
test** — `fits = (&r[65:48]) || (~|r[65:48])` — the same identity that fixed the
skinner's 73-bit saturation tail.

**Wave 5 — SIN.** Synchronous dual-port table, balanced six-term `d·t` tree
(currently a procedural accumulator loop that may synthesise as a serial adder
chain), pipelined ROM → tree → round/sign, one request per clock so ROT can
issue cosine and sine on consecutive clocks.

### Acceptance

* 3 DSPs preferred, ≤4 only with measured justification; **≤8 M10Ks**;
* **standalone Fmax ≥110 MHz preferred, ≥100 MHz required** — a leaf block
  barely reaching 100 will not hold 100 after composition;
* zero hold violations; every result **bit-identical to `zfield::interpret`**;
* **report Fmax / measured opcode cycles, never Fmax alone**;
* **no false-path or multicycle exemption used to hide functional logic**, and
  "Quartus accepted the attribute" is not evidence.

### Why extra clocks are affordable — the arithmetic that makes this safe

The trade is real time, not cycles:

| | today | proposed |
| --- | ---: | ---: |
| simple op | 6 clk @ 33.86 MHz = **5.64 M/s** | 7 clk @ 100 MHz = **14.29 M/s** |
| NORMALIZE3 | 67 clk = **0.505 M/s** | ~80 clk = **1.25 M/s** |

**Even at 8 clocks per simple op the new engine is 2.2× today's real
throughput.** Estimated targets (not measurements): ALMs 7,750 → ~3,500–5,000,
M10Ks 0 → 5–8, DSPs unchanged at 3.

### THE UNRESOLVED QUESTION, and it is the important one

**No profile has a derived per-frame workload.** Several still claim "one Field
IR instruction per clock" — the same one-clock placeholder that produced 327
DSPs, and exactly what the three demand numbers replaced elsewhere.

The budget each profile owes is:

    invocations/frame  x  instructions/program  x  measured clocks/instruction

summed across Earth + Warp + Flow + Formation + Stamp against **1,666,667
clocks/frame**. A worked example shows why this matters:

    120,000 warped vertices x 8 instructions x 8 clocks = 7,680,000 clocks/frame

**One core cannot do that however well it closes timing** — it is 4.6× the
whole frame.

That would not mean the architecture failed; it would mean **one physical
instance was the wrong quantity.** At ~3 DSPs, ~4–5k ALMs and ~6 M10Ks per core,
a plausible arrangement is core A = Earth + Stamp, core B = Warp + Formation,
core C = Flow. **Three cores are nine DSPs — the original single engine used
79.** They remain instances of one verified engine, which preserves the "one
engine, five profiles" ruling (V21 `kind: profile`, `implemented_by`) rather
than becoming five specified implementations.

**Derive the demand before deciding the count.** Instantiating by rate class is
the answer if the numbers say so; widening the arithmetic farm is not.

### What Field is for, recorded because it justifies keeping it

Field is the console's small programmable math engine — not a scripting
language, not a renderer. It answers *"given a point in the world, what should
happen there?"* in deterministic fixed-point that matches the software reference
bit-for-bit, across five bindings: **EARTH** → `TERRAIN.PATCH` (crater height,
ridge, material, hazard), **WARP** → `GEOM.WARP` (vertex deformation),
**FLOW** → `PART.UPDATE` (wind, vortex, explosion force), **FORMATION** →
`GEOM.LOOM` (procedural placement), **STAMP** → `SURFACE.STAMP` (what a scar
should be, before SURFACE.STAMP walks the texels).

The point is that effects are **data rather than hardwired blocks**: a new spell
recombines the primitives without redesigning the FPGA. It is also why the
original was 79 DSPs — a single-threaded machine with ten FPUs bolted on, nine
idle on every instruction.

This is the piece most directly connected to the "Sacrifice but bigger, crazier,
more deformable" identity. Rasterisation makes pixels, geometry makes triangles,
terrain owns the land, particles own the particles — **Field supplies the maths
that tells all of them how to misbehave.**


## 2026-08-23 — A PATTERN: our blocks are cheap because they assume normalised
## content, and nobody has written down what "normalised" means

Three findings today share one shape, and the third was found by checking rather
than by being bitten. **Each is a hardware law that is correct and efficient,
which silently imposes a precondition on the asset pipeline that Sacrifice's own
art violates.** None is a defect. All three become defects the day real content
is loaded and nobody remembers the assumption.

### 1. Bone weights must sum to 64

`zhao_geom_skin` blends with `(pb << 6) + w0·(pa − pb)`, which is exact **only
when `w0 + w1 = 64`.** That identity is what halved the multiply count.

Sacrifice's weights are a raw `ubyte` scaled by `weight/64.0f` (`sxmd.d:97`,
`saxs.d:76`) and **all 256 raw values occur**, so its blend is an *affine* sum of
independently-weighted bone-space offsets, not a convex combination.

**Precondition: the importer must normalise each vertex's weights to sum to 64.**

### 2. At most two bone influences per vertex

Sacrifice stores **three** (`saxs.d:23-42`, `int[3] indices_`). Measured over
317,234 ring-vertices: 65.07% use one bone, 32.41% two, **2.51% three** — and
the three-influence vertices are the seams (shoulders, hips, neck) where error
is most visible.

**Precondition: the importer must reduce to two influences (dropping the
smallest and renormalising), or the block needs a second pass for the 1-in-40.**

### 3. Texture dimensions must be powers of two — NEW

`TEXTURE.TMU.md:62-63` — `LOG2W`/`LOG2H` are 4-bit fields and level-0 width is
`1 << LOG2W`. **Non-square is supported** (that is why the 16×64 beam ramp
works, and the contract says so explicitly), but **non-power-of-two is not
representable at all.**

That restriction is load-bearing, not incidental. The contract at `:74`: with
`size = 1 << log2s`, converting to texels is *"a **shift, not a multiply**"*.
Making dimensions arbitrary would put a multiply on the per-sample path — adding
DSPs to the very block we are trying to cut from 28 to 6–9 — and would break the
closed-form mip level offset, whose exactness depends on the same property
(`:116-121`).

**Sacrifice's creature art violates this.** Measured over 637 `.SXTX` assets:
width is essentially always 256, but **height is arbitrary — 9 to 799** (269,
287, 331, … are typical), and **only 81 of 637 (12.7%) are power-of-two in both
axes.** It is structural rather than sloppy: each is a **vertical atlas strip**
whose height is whatever that body part needed, with V computed as
`ring.texture / textureMax` (`saxs.d:90`).

Everything else in the game is already compliant — all 626 `.TXTR` assets are
strictly power-of-two and square, **nothing exceeds 256×256**, and land tiles are
64×64.

**Precondition, and there is a good and a bad way to meet it:**

* **Bad:** pad each strip up to the next power of two. 799 → 1024 wastes 28%,
  and a 256×1024 texture is 512 KB at 16bpp — for one creature.
* **Good:** the strips are already a **concatenation of per-body-part regions**.
  Split them at body-part boundaries into power-of-two tiles at import. That is
  a repack, not a resample, so **no texel is altered and nothing is lost** — and
  it matches how the geometry addresses them, since each ring already carries
  its own `texture` offset.

**Recommend the split.** It costs importer complexity and nothing at runtime.

---

### Why this is worth a docket entry rather than three scattered notes

Every one of these is the same trade: **the hardware is cheaper because it
assumes something about the content**, and each assumption was made for a good
reason that is documented *in the block* — and nowhere in a place the asset
pipeline would ever look.

The failure mode is specific and predictable: the first real creature loads,
something is subtly wrong at the shoulders or the texture is off by a row, and
the search starts in the RTL — which is correct — rather than in the importer,
which was never told.

**Proposed: `design/contracts/` grows an `ASSET_PRECONDITIONS.md`**, listing
every content invariant the hardware relies on, each pointing at the block and
the line that relies on it. Three entries today. There will be more — every
block sized against a demand figure is also a block making assumptions about the
shape of its input.

**None of this is urgent and none blocks current work.** But it is much cheaper
to write down now, while the reasons are fresh, than to rediscover from a
rendering artefact later.

## 2026-08-23 — THE THREE DEMAND NUMBERS, derived from Sacrifice itself

**These are DERIVED, not ruled.** Fabian asked for a best guess from evidence
rather than a decision from him: *"Orient yourself on sacrifice… Find what
resources we need. We look flashier, bigger, crazier, but our resolution is shit
so we probably actually need less of everything."* Every figure below carries
its source. Overturn any of them on sight.

Evidence is a read-only survey of `sacengine` (a D reimplementation that parses
the original formats) and the retail install, including symbols exported from
`Sacrifice.exe`.

### First, a corroboration worth more than the three answers

**The 120,000 vertices/frame ruling is independently plausible.** Measured across
**93 creature/wizard/hero models**: median **2,951 vertices** (min 1,498, max
8,996). The engine's own caps give 10 groups × 12 creatures addressable, but the
real limit is the soul economy — starting pools measured across all 32 retail
maps are **4–12 souls, mode 8**, and creatures cost 1–5 souls each. A defensible
scene is **40–60 skinned characters**.

    40 x 2,951 = 118,040        60 x 2,951 = 177,060
    the ruling of 120,000  ->  ~41 creatures at median detail

So the number was well chosen. It was the one input that made GEOM.SKIN's 13.9x
over-provisioning computable, and it now has a second, independent source.

### The headline ratio

Sacrifice shipped at **800×600×16bpp** (retail log `thaum.err`, build 0678,
May 2001), falling back to 640×480, max 1024×768. We render **92,160** pixels
against its **480,000** — **19%**. At equal cost per triangle that is roughly
**5× the per-pixel budget to spend on "more stuff"**, which is exactly the brief.

---

### 1. SURFACE.STAMP — 28 DSPs, and it needs approximately none

**The original's system is called SCAR and it is a texture stamp, exactly like
this block.** `Sacrifice.exe` exports `ApplyScarring`, `ApplyScarringToTile`,
`GetFreeScarTexture`, `ReleaseScarTexture`, `MakeScarLUT`, `ClearScars`,
`SaveScars`. 19 SCAR assets exist; the format was decoded and validated against
all 19 (`16 + 1024 + 2·W·H`): 8-bit palette index **plus 8-bit alpha**, sizes
8×8 to 256×256, median 64×64. The five elemental god scars (`air_`, `deth`,
`erth`, `fire`, `life`) are **128×128** — the bread-and-butter impact mark.

Land tiles are 64×64, so a 128×128 scar dirties **at most 3×3 = 9 tiles =
36,864 texels**, and that happens **once per spell impact, not per frame**:

| | texels/s | texels/frame | share of a frame at 1 texel/clock |
| --- | ---: | ---: | ---: |
| 10 impacts/s (heavy barrage) | 368,640 | 6,144 | **0.37%** |
| 30 impacts/s (absurd) | 1,105,920 | 18,432 | **1.11%** |

**The block is provisioned for 1,666,667 texels/frame. It needs about 6,000.
Over-provisioned by roughly 270×.**

The multiplier count should fall further than the rate alone implies, because
**the original composites through a lookup table — `MakeScarLUT` — not
per-texel arithmetic.** A LUT-composited stamp at one texel per ~90 clocks
plausibly needs **zero DSPs**.

> **DERIVED DEMAND: 20,000 stamp texels per frame** (3× the heavy-barrage
> figure, for headroom). **Proposed target: 0–2 DSPs.**

**OUTCOME, 2026-08-23 (RUN-20260823-1415): 0 DSPs, measured, constrained fit.**
Two notes back to this entry, because both correct it:

- **The LUT was not needed and would not have helped.** The guess above was that
  zero DSPs would come from copying `MakeScarLUT`. It did not: `zhao_surface_blend`
  — where all five stamp modes, the ABI mapping and age/decay live — **never
  contained a multiply at all** and fits on its own at 64 ALMs / 0 DSPs. Every
  one of the 28 DSPs was in the **coverage geometry** (`dx²+dz²`, `r²`,
  `r_inner²` and two texel-centre products), which a LUT does not touch. They
  came out by making the four squares share one sequential shift-add squarer and
  turning the two texel-centre products into accumulators.
- **The 270× figure was right about the ratio and understated the problem.** The
  block did not merely spend DSPs on a rate nobody asked for; the constrained fit
  it bought closed at **32.33 MHz against a 100 MHz `gpu_clk`**. It was holding
  the console's shared clock to a third of its constraint. After: **87.54 MHz**,
  at 37,784 texels/frame — 1.89× the derived demand.

**The real constraint is not bandwidth — it is the tile pool.**
`GetFreeScarTexture` / `ReleaseScarTexture` prove a **finite pool of writable
64×64 tiles**, copy-on-write: an unscarred tile shares the read-only static set
and only gets a private mutable copy when first scarred. Scars are **permanent**
(no fade path anywhere; serialised into savegames). **Size the pool, not the
rate.** The pool's capacity is compiled into the original's private data and is
*not* recoverable — a genuine open question, and an architecture one rather than
a throughput one.

---

### 2. TEXTURE.TMU — 28 DSPs, needs roughly half-rate

Terrain is layered **tile + detail + lightmap** (`sacmap.d:136-174`), so **at
least 3 samples per terrain pixel**. Tiles are 64×64 8-bit paletted; detail
textures 256×256, up to 7 per tileset; one 256×256 RGBA lightmap per map. **No
alpha splatting between tile types** — the tile index is per-cell and hard-edged,
which is a real simplification in our favour.

| | samples/frame | samples/clock |
| --- | ---: | ---: |
| Z60 384×240, overdraw 2.0 | 552,960 | **0.33** |
| Z60 384×240, overdraw 3.0 | 829,440 | **0.50** |
| WIDE 384×216, overdraw 3.0 | 746,496 | **0.45** |

> **DERIVED DEMAND: 850,000 samples per frame** — one sample every two clocks,
> not one per clock. A bilinear tap is `a + (b−a)·w`: three lerps per channel,
> three channels ≈ 9 products at full rate, so **about 5–6 DSPs at half rate.**
> **Proposed target: 6–9 DSPs**, from 28.

**OUTCOME, 2026-08-23 (RUN-20260823-1736): 6 DSPs, measured, constrained fit.**
Four notes back to this entry, because each corrects or sharpens it:

- **The arithmetic guess was right and the route to it was not what this entry
  imagined.** `a + (b−a)·w` is exactly the form that landed — but as an
  *algebraic factoring of the existing four-weight law*, not as a new law:
  `A = (t00<<8) + (t10−t00)·fu`, `B = (t01<<8) + (t11−t01)·fu`,
  `S = (A<<8) + (B−A)·fv`, one rescale at the end. Three products a channel,
  **twelve across the block against the shipped thirty-two**, and bit-identical
  to `zref::Tmu` for every input. Crucially it is **not** the staged-rounding
  form `spec/qformats.md` §3 refuses — nothing intermediate is rounded.
- **"Three channels ≈ 9 products" undercounts by one channel.** ARGB4444 and
  ARGB1555 carry a real alpha that has to survive the filter, so it is four
  channels and twelve products at full rate, six at half. The conclusion
  survives: half rate is 6 DSPs, which is what was measured.
- **Half rate had to be spent, and the fit is why.** Quartus 17.0.2 Lite packs
  **nothing**: twelve products fit at **12 DSPs**, one block each — it will not
  fuse two small multiplies into one block however narrow they are. So the
  full-rate option missed the 6–9 target and the multiplexed half-rate one hit
  its floor exactly. (The operator count is a **lower** bound, not the whole
  answer: cost also jumps with operand width — see `design/budgets/dsp.md`'s
  correction and `reports/QUARTUS_GOTCHAS.md` §5.)
- **The DSPs were never the block's worst number.** See the throughput and
  timing notes in `reports/REMAINING_BLOCKERS.md`: this block runs at **0.33× the
  demand above**, and its per-block Fmax had been measuring its sample counter.

**Two structural warnings for whoever builds it**, both measured:

* **Creature textures are 256 wide with *arbitrary* height, up to 799.** Only
  **81 of 637 (12.7%)** are power-of-two in both dimensions. This is structural
  rather than sloppy: each body-part texture is a **vertical atlas strip** whose
  height is whatever that part needed (`saxs.d:90`, V = `ring.texture /
  textureMax`). **A TMU that assumes square power-of-two breaks on the majority
  of character art.** The natural hardware shape is 256 wide × up to 1024 tall.
* **Everything else is strictly power-of-two and square**, and **nothing exceeds
  256×256** — 390 of 626 are 256×256, the rest 128×128 or smaller.

**ANSWERED by whoever built it (2026-08-23, RUN-20260823-1736), and the answer
is "keep the repack docketed, and here is what it is actually buying".** The
warning above is right that the block assumes power-of-two, and the cost of
relaxing that is now measurable rather than notional: `LOG2W`/`LOG2H` are what
make texel conversion (`u_raw << log2s`), the mip level offset (a base-4-repunit
table plus **one** variable shift) and the row-major index (`(v << log2w) + u`)
all **shifts**. Every one becomes a **multiply on the per-sample address path**
if dimensions are arbitrary — on the block whose entire rearchitecture was
removing multiplies from that cone, and on a kit where **one `*` operator = one
DSP block**, measured three ways. A 256×799 sampler would put the DSPs straight
back. The asset-pipeline repack is not the cheap option; it is the only one that
does not undo the cut.

Mip-mapping is **proven present**: every `.MAPT` is 5,476 bytes, and
`4096+1024+256+64+16+4+1 = 5,461` is a complete 64×64→1×1 chain. Verified
empirically rather than by arithmetic alone — a palette-parent test gives a
68.6% hit rate at offset 0 against 22.9% at offset 16, the signature of
palette-space box filtering. **The shipped mips exist and sacengine throws them
away** (`sacmap.d:361-368` reads only the first 4,096 bytes). We should not.

---

### 3. TERRAIN.NORMALS — 18 DSPs, needs almost nothing

Terrain is a **regular 256×256 heightfield** — every one of the 32 retail maps
is exactly 256×256, corroborated three ways in source. Grid spacing is 10 world
units, so a map is 2,550 × 2,550.

Runtime deformation is **permanent and tiny**. `PermanentDisplacement` holds
`float[256][256]` (`state.d:6787`), CRC-hashed for network sync, with exactly
two deformers:

| source | radius | depth | grid cells touched |
| --- | ---: | ---: | --- |
| Bombardment | 15.0 units | 1.2 | ~3×3 |
| Volcano | 25.0 units | 10.0 | ~5×5 |

**A crater touches 9–25 vertices.** Neither fades.

| | normals/frame |
| --- | ---: |
| 10 deform events/s × 25 vertices | **4.2** |
| 30 events/s | 12.5 |
| rebuilding the **entire** 65,536-vertex field once per second | **1,092** |

**The block is provisioned for 1,666,667 per frame.** Even the pathological
whole-field rebuild is 0.07% of that.

> **DERIVED DEMAND: 2,000 normals per frame** — generous enough for a full field
> rebuild every second plus heavy combat. **Proposed target: 1–2 DSPs**, from 18.

There is a real optimisation here too: the reference backend only re-runs terrain
displacement when the hash differs from `emptyHash` (`dagonBackend.d:2050-2058`)
— **it is free until something actually digs.**

---

### If all three land, the census closes

| block | now | derived target |
| --- | ---: | ---: |
| ~~`zhao_surface_stamp`~~ | ~~28~~ → **0** | ~~0–2~~ **LANDED** |
| ~~`zhao_texture_tmu`~~ | ~~28~~ → **6** | ~~6–9~~ **LANDED** |
| `zhao_terrain_normals` | 18 | **1–2** |
| | **74** → **24** | **7–13** |

**188 − 74 + ~10 = about 124**, before `terrain_project` (33), `geom_cull` (15)
and `geom_binner` (12) are touched at all. **The 85–90 ceiling is reachable.**

---

### TWO CORRECTNESS FINDINGS that outrank all of the above

**1. Skinning is 3-bone, not 2 — and our block does 2.**
`saxs.d:23-42` — `struct Vertex { int[3] indices_; }`, with `-1` terminating.
Measured over **317,234 ring-vertices across all 93 models**:

| influences | share |
| --- | ---: |
| 1 bone | 65.07% |
| 2 bones | 32.41% |
| **3 bones** | **2.51%** |

**Our 2-lane block is exact for 97.49% of vertices and clips 2.51%** — and the
clipped ones are the *seam* vertices (shoulders, hips, neck), where the error is
most visible. Options: accept and renormalise the two largest weights; widen to
3 lanes; or split 3-influence vertices into a rare second pass. **The second pass
is probably cheapest** given the tail is 1 vertex in 40. This is a decision, not
a defect — but it has to be taken deliberately rather than discovered.

**2. Sacrifice's weights do NOT sum to a constant, and our blend identity assumes
they do.** Weights are a raw `ubyte` scaled by `weight/64.0f` (`sxmd.d:97`,
`saxs.d:76`), and **all 256 raw values occur**. So the blend is an affine sum of
independently-weighted bone-space offsets, **not a convex combination**. Our
`(pb << 6) + w0·(pa − pb)` identity — the one that halved the multiply count — is
valid only when `w0 + w1 = 64`.

That is fine **if our content pipeline normalises on import**, and it means we
cannot ingest Sacrifice meshes unmodified. Worth stating explicitly in
`GEOM.SKIN.md` as a precondition on the asset pipeline, rather than discovering
it when the first real creature loads.

---

### Also worth knowing

* **Particles: no global cap exists** — all storage is dynamically grown, across
  69 particle types. The largest single burst is the fireball explosion at
  `state.d:15453-15490`: **1,375 particles in one frame** (200 + 800 additive,
  300 ash, 75 smoke), living 0.52–2.1 s at 60 Hz. **Budget 2,000–3,000
  simultaneous particles**; two overlapping fireballs alone reach ~2,750.
  Particle *behaviour* remains owner-reserved and nothing here designs any.
* **Terrain is sub-pixel dense at our resolution.** A full map is up to **130,050
  triangles** against 92,160 pixels. The original used **ROAM** continuous LOD —
  proven by `CalculateVariance`, `UpdateVariance` and `GetHeightMipMap` exported
  from `Sacrifice.exe`. So `zhao_terrain_lod` is not optional polish; aggressive
  terrain LOD is close to free visually.
* **Creatures had no LOD** in the original (one mesh each), and the `lod` field
  in the static-model format is **not** an LOD level — measured values are
  near-unique and monotonically increasing, i.e. a face ordering index. The
  reimplementer's field name is a guess; treat "static models have LOD" as
  unproven.
* Simulation ran at **60 Hz**, animation sampled at **30 Hz** (`state.d:13`,
  `sacobject.d:19`), rotations stored as quantised `short[4]` quaternions, up to
  64 named animation states per creature.
* **No documented budget of any kind exists** in either tree — no draw-call or
  polygon budgets, no design notes. The only quality knobs are `enableParticles`
  and a `reduceParticles` divisor.

### Not determinable, stated plainly

The scar-texture **pool size** and maximum concurrent scar count; the actual
stamp rate under combat; the global live-particle maximum; the texture filtering
mode as an explicit setting; texture samples per pixel as a stated budget; the
target frame rate (the string `Target framerate` exists in the binary, the value
does not); the ROAM error threshold and its triangle reduction; and whether
creatures were ever distance-culled or impostored.


## 2026-08-23 — THREE NUMBERS NEEDED to finish the multiplier campaign

Not urgent, and nothing is blocked on them today. But they are the same *kind*
of number as the 120,000 vertices/frame ruling, and that one ruling is what
turned GEOM.SKIN from an argument into a measurement.

### Why a number is needed at all

Every reduction so far came from one calculation: **sustained demand per frame,
against products per item.** GEOM.SKIN had 18 multipliers; 120,000 vertices at
13.88 clocks each made the honest requirement **1.30**, so it was
over-provisioned 13.9x. That is the whole method, and it cannot be run without
a demand figure.

The compute budget it runs against is now pinned down:
**1,666,667 clocks per 60 Hz frame** at the 100 MHz placeholder. (Note: *not*
`frame_gpu_cycles` = 251,520, which is the raster period and the scheduler's
deadline — the two differ by 6.6x and both are called "gpu cycles". Written up
in `design/budgets/latency.md`.)

### Where the remaining 188 multipliers sit

| block | DSPs | has a ruled demand? |
| --- | ---: | --- |
| `zhao_terrain_project` | 33 | **yes** — ~270 patches/frame, already costed against the 1.67 M budget |
| ~~`zhao_surface_stamp`~~ | ~~28~~ → **0** | **DONE 2026-08-23** — demand derived above, block rearchitected, measured 0 DSPs at 87.54 MHz (RUN-20260823-1415) |
| ~~`zhao_texture_tmu`~~ | ~~28~~ → **6** | **DONE 2026-08-23** — demand derived above, filter factored 32 products to 12 and multiplexed to 6, measured 6 DSPs (RUN-20260823-1736) |
| `zhao_terrain_normals` | 18 | **NO** |
| `zhao_geom_cull` (the cull third of GEOM.MESHFETCH) | 15 | **yes** — one evaluation per five clocks |
| `zhao_geom_binner` | 12 | **yes** — per-triangle costs plus hard caps (`TRI_CAP` 128, `CHUNKS` 256) |

**Three blocks, 74 of the 188 remaining multipliers, have no demand figure.**
*(SURFACE.STAMP's 28 are now gone — see the row above. 46 remain unruled, in
`zhao_texture_tmu` and `zhao_terrain_normals`.)*
What `design/blocks.yml` records for them instead is a **one-clock placeholder** —
"1 stamp texel per clock", "1 sample per clock", "1 normal per vertex per
clock". That is precisely the input the architecture ruling rejected, and it is
where the original 327 came from: a block told to do one item per clock is
built to do one item per clock, whether or not anything ever asks it to.

### The three questions

1. **SURFACE.STAMP — how many stamp texels per frame, sustained?** Burn marks,
   craters, blood, spell scarring. The figure that matters is the *sustained*
   one, not the worst single frame — a Bore collapse can be allowed to take two
   frames if that is cheaper than building for the peak.

2. **TEXTURE.TMU — how many texture samples per frame, sustained?** This is
   close to fragments-per-frame times samples-per-fragment. Z60 is 92,160
   pixels; with overdraw and bilinear the honest figure could be anywhere from
   ~100,000 to ~500,000, and the difference is roughly 5x of multiplier budget.

3. **TERRAIN.NORMALS — how many normals per frame, sustained?** Only deformed
   terrain needs recomputing. If it is "the patches that changed this frame"
   rather than "the whole field", the number could be very small — and this
   block is 18 multipliers on the strength of a placeholder.

### What a ruling unlocks, and what a refusal costs

With a number, each block gets the GEOM.SKIN treatment and the campaign can
finish. Without one, the honest options are to leave them at their current
counts, or to guess — and guessing is what produced a design wanting 2.9x the
chip's multipliers with every gate green.

**If any of these depends on game content that is not decided yet, say so and it
stays open.** A recorded "not decided" is worth more than an invented rate; that
is why the particle-simulation, compositor and 2D blocks are still empty in this
docket rather than filled with plausible behaviour.

One follow-up regardless of the answers: **`design/blocks.yml` should carry
sustained demand rather than one-clock rates**, so the next block sized from it
starts from the real number.

## 2026-08-23 — WIDESCREEN: accepted in principle, and the arithmetic has one
## problem the proposal did not catch

**Status: DOCKETED, not implemented. Deliberately not interrupting the DSP and
timing campaign for it.** Recording it now is the point — the game, HUD and
content pipeline are not yet entrenched, and adding a mode after fifty missions
and a finished interface is far more painful than adding one to the hardware
later.

### The ruling as proposed

* Widescreen presentation at minimum via an **anamorphic 384×240** mode: same
  framebuffer, same line buffers, same scanout bandwidth, same fragment count,
  no extra DSPs. A 16:9 view-projection matrix is sent while the FPGA draws the
  same 92,160 pixels.
* A native square-pixel **`VIDEO_WIDE` = 384×216 RGB565 @ 60 Hz**, presented
  over HDMI as **1920×1080p60 at exact 5× nearest-neighbour**. Scanlines and CRT
  treatment happen in the output scaler, never in the render framebuffer. On a
  4K panel, keep 1080p out and let the television do the final exact 2×.
* **No native mode may exceed the existing 512×240 storage/scanout envelope in
  v1.**
* `320×180` kept as a possible extra-chunky mode later (exact 6× to 1080p).

### The arithmetic, checked

| | pixels | framebuffer | row | bursts/row | 16:9? | to 1920×1080 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| Z60 384×240 (today) | 92,160 | 184,320 B | 768 B | 12 | no (8:5) | 5× / 4.5× — **not integer** |
| **WIDE 384×216** | **82,944** | **165,888 B** | 768 B | **12** | **exact** | **5× / 5× — integer** |
| alt 416×234 | 97,344 | 194,688 B | 832 B | 13 | exact | 4.615× — not integer |
| alt 320×180 | 57,600 | 115,200 B | 640 B | 10 | exact | 6× / 6× — integer |

384×216 is **10% fewer pixels than Z60**, keeps the row size and the 12-burst
fetch **completely unchanged**, and is the only candidate that is simultaneously
exact 16:9 and an exact integer scale to 1080p. The preference for it over
416×234 is correct, and for a stronger reason than was given: 416×234 is not
merely "uneven", it is **4.615×**, which alternates 4- and 5-pixel columns and
will shimmer on horizontal scroll.

### THE PROBLEM: 216 is not a multiple of the tile height, and neither is any alternative

The renderer works in **16×16 tiles**. 384/16 = 24 exactly, so width is fine.
But:

    240 / 16 = 15      exact   (Z60 works)
    216 / 16 = 13.5    NOT exact
    234 / 16 = 14.625  NOT exact
    180 / 16 = 11.25   NOT exact

The proposal states "416×234 → 26 × 15 = 390 tiles". **That is wrong** — 234/16
is 14.625, not 15 — and the same slip hides the problem in 384×216.

**And this is not a matter of picking a better number. It is impossible.** For
an integer scale `s` to 1920×1080, the source height is `1080/s`. For that to be
a multiple of 16, 1080 must be divisible by `16s`. But

    1080 = 2^3 · 3^3 · 5

has only **three** factors of two, and 16 needs four. **No integer scale to
1080p yields a tile-exact height — none, at any resolution.** Verified by
enumeration as well as by the factorisation.

So the choice is not between resolutions. It is between *integer 1080p scaling*
and *tile-exact frame height*, and one of them has to give.

### The resolution, which costs nothing

**Decouple the tile grid from the displayed area.** Round the grid *up* to a
multiple of 16 and display a centred window inside it. Converged on
independently from both directions, and refined in two ways worth taking:

    stored / tiled canvas   384 × 224   = 24 × 14 = 336 complete tiles
    ┌──────────────────────────────────┐  stored row   0
    │ 4-row guard, outside the camera  │
    ├──────────────────────────────────┤  stored row   4  = displayed row   0
    │                                  │
    │   visible  384 × 216   16:9      │
    │                                  │
    ├──────────────────────────────────┤  stored row 219  = displayed row 215
    │ 4-row guard, outside the camera  │
    └──────────────────────────────────┘  stored row 223

| | value |
| --- | ---: |
| tile grid / stored canvas | **384 × 224** — 24 × 14 = **336 exact tiles** |
| stored bytes | **172,032 B** (fits the 245,760 B slot unchanged) |
| camera viewport | **x0 = 0, y0 = 4, w = 384, h = 216** |
| active raster / displayed | **384 × 216**, 165,888 B |
| scanout mapping | displayed row `y` fetches stored row `y + 4` |
| stride, bursts per row | 768 B, **12** — both unchanged from Z60 |
| HDMI | 1920×1080 at exact 5× nearest-neighbour |

This needs **no partial-tile support anywhere**: the grid stays exactly
divisible and every tile is a full 16×16.

**Refinement 1 — centre the window rather than top-aligning it.** Four guard
rows above and four below, instead of eight dead rows at the bottom. Verified
to cost nothing: the visible span touches tile rows 0..13 either way, so the
tile count is identical. The gain is that **neither visible edge coincides with
a buffer edge**, so no clamp-at-row-0 or conservative-rasteriser spill can
reach a displayed pixel.

**Refinement 2 — and this is the important one — the camera is genuinely 16:9,
not a cropped 16:10 image.** The projection matrix and the viewport are for
384:216. The eight guard rows are simply *outside the camera viewport*; they
exist to satisfy the tile machinery and nothing renders into them. Rendering a
16:10 frame and chopping it afterwards would silently change what the player
sees; this does not.

**There is precedent for exactly this in the design already, which is the part
that makes it cheap.** Duo stores two 256×192 canvases — 196,608 B — while
emitting a 512×240 displayed stream of 245,760 B, with scanout manufacturing
the border rows rather than storing them; the spec already distinguishes
allocation, stored occupancy and displayed bytes. `zhao_scanout_fetch` already
carries a **mode-specific mapping from displayed row to stored row** (in Duo,
displayed rows 0–23 and 216–239 fetch nothing and displayed row `y` maps to
stored `y − 24`). Wide needs the same mechanism with `y + 4`. *(This precedent
is on the audit list below — it is the claim the cost estimate leans on hardest,
so it gets checked rather than assumed.)*

Tile count **falls** from Z60's 360 to 336. Pixels fall 10%. Scanout falls 10%
(216 × 12 = 2,592 bursts/frame against 2,880). **Widescreen makes the
renderer's job slightly easier, not harder** — the same conclusion the proposal
reached, and it survives the correction.

The identical trick sizes `320×180` later: stored 320×192 (20 × 12 = 240
tiles), visible 320×180 with 6-row guards, exact 6× to 1080p — 57,600 displayed
pixels against Z60's 92,160. A genuinely cheap, very chunky performance mode.

### WIDE DUO — split screen, and it is cheaper than every mode that exists

Docketed alongside `VIDEO_WIDE`. The arithmetic was checked and it holds
throughout; the structure below is one step better than proposed, for a reason
the audit turned up.

Two **192×144** views side by side inside the 384×216 window:

    384 x 216 displayed
    +------------------------------------------+
    |            36 rows, black                 |
    | +--------------------+------------------+ |
    | |   P1  192x144      |   P2  192x144    | |
    | |      exactly 4:3   |     exactly 4:3  | |
    | +--------------------+------------------+ |
    |            36 rows, black                 |
    +------------------------------------------+

Everything lands exactly:

* 192 × 2 = **384** — fills the width with no remainder;
* 192 / 144 = **4:3 exactly**;
* 192/16 = 12 and 144/16 = 9 — **tile-exact in both axes**, 108 tiles per view;
* (216 − 144) / 2 = **36 top and 36 bottom** — symmetric, no rounding;
* at 1080p each view is **960 × 720** with 180-pixel borders, every source
  pixel still a perfect 5 × 5 block.

**Store the two views only and manufacture the borders at scanout — do not
build a shared canvas.** This is the structural improvement, and it is exactly
what Duo already does (stores 2 × 256×192 = 196,608 B, displays 512×240 =
245,760 B, with border rows fetching nothing —
`zhao_pkg.sv:100`, `zhao_scanout_fetch.sv:125-140`). Structuring WIDE DUO the
same way means **each view is its own 12×9 grid at its own origin**, so the
question of whether the views sit on tile boundaries inside a larger canvas
never arises. Had they been placed in a shared 384×224 canvas they would
*not* have been tile-aligned vertically, and each view would have cost 10 tile
rows instead of 9.

| | Duo today | **WIDE DUO** | Z60 |
| --- | ---: | ---: | ---: |
| per view | 256×192 | **192×144** | — |
| tiles per view | 192 | **108** | — |
| **total tiles** | 384 | **216** | 360 |
| stored bytes | 196,608 | **110,592** | 184,320 |
| displayed bytes | 245,760 | **165,888** | 184,320 |
| bursts per active row | 16 | **12** | 12 |
| **bursts per frame** | 3,072 | **1,728** | 2,880 |

**44% fewer tiles and 44% fewer memory bursts than Duo, and fewer than Z60 on
both counts.** It is cheaper than every mode the console currently has.

Two details that matter given what the audit found:

* **192 px is 384 B per row = exactly 6 bursts.** The fetch client issues only
  whole 64-byte bursts with all byte enables set and **has no masked-tail
  path** (`zhao_scanout_fetch.sv:149-155`), so any width that is not a multiple
  of 32 pixels would be unbuildable. 192 is.
* **A 12-column grid is within `GRID_W = 24`.** The binner's compile-time
  parameter is a ceiling, and Duo already runs a 16-column grid against it, so
  12 needs no change — unlike the 26 columns that 416 px would have required,
  which would have aliased tile rows silently.

The Measure gains a great deal of room here: 216 tiles against Duo's 384, with
the same two-player workload.

**Not a replacement for the existing Duo.** Keep 2 × 256×192 as the
big-views option if the budget ever allows it. WIDE DUO is the high-performance
default, and two proper chunky 4:3 mini-screens side by side is arguably the
better look anyway. The 36-row bands are black by default and are the natural
home for restrained shared UI later — boss health, timer, spell state — without
shrinking either player's image.

### Anamorphic fallback — worth having, but be honest about it at 1080p

384×240 stretched to 16:9 is genuinely almost free and should exist. But at
1080p it is **5× horizontally and 4.5× vertically**, so the vertical scale is
not an integer and scanlines cannot be uniform — exactly the artefact the native
mode is chosen to avoid. It is the right fallback for adapters that cannot do
better; it is not the shipping presentation.

### Scanlines: after scaling, never in the framebuffer

At exact 5×, each source row occupies five HDMI rows and every scanline is
identical in thickness. Darkening rows in a 384×216 framebuffer would instead
throw away real vertical information. Three presets — Sharp / Scanline / CRT —
belong in the output stage. A strong RGB shadow mask should probably not be the
default: five output columns per source pixel do not divide into three phosphor
triads, so an aggressive mask risks moiré.

### AUDIT RESULT — the plan holds, with two corrections and one bug found

A read-only audit checked every claim against the code. **Eleven of thirteen
confirmed outright.** The two that did not are both important, and one is a
defect that exists today independently of widescreen.

#### 1. "Value 3 is available" is FALSE in practice

The *encoding slot* is free — `zhao_pkg.sv:40-44` declares a 2-bit enum with
three values — but three separate places treat **anything that is not 0 or 1 as
Duo**, and two would read out of bounds:

* `zhao_pkg.sv:191-194` `zhao_mode_from_abi`: `(v==0)?Z60:(v==1)?STORM:DUO` —
  **any byte ≥ 2 becomes Duo**;
* `zhao_pkg.sv:137-148` `zhao_canvas_bytes` / `zhao_displayed_bytes` — the same
  else-is-Duo ternary;
* `zhao_pkg.sv:67-80` `ZHAO_TIMING[0:2]` is a **three**-entry table;
  `ZHAO_TIMING[3]` is out of bounds. `zhao_cmd_scheduler.sv:527-535` already
  carries `a_mode_act_in_range : assert (mode_act <= 8'd2)` **because indexing
  it out of range was a real defect once** (the W2.6 "lawless-mode-byte"
  defect);
* `zhao_scanout_fetch.sv:141-145` has a `default:` arm commented "unreachable:
  2-bit enum, three declared values" that emits `line_real=0` — **a fourth mode
  would silently fetch nothing.**

So adding `VIDEO_WIDE` is not "use the free slot". It is a deliberate ABI
amendment plus the removal of four else-is-Duo assumptions.

#### 2. A LATENT BUG, present today, unrelated to widescreen

`reference/src/zref_video.cpp:18-27`:

    static const VidTiming kTable[3] = { ... };
    return kTable[mode & 3u];

**The mask admits 0–3. The table has three entries.** `mode == 3` is an
out-of-bounds read in the shipped reference oracle. Verified by reading the
file directly, not just from the audit.

It is currently unreachable — every entry point rejects mode 3 (`ZH_ABI_BAD_VALUE`
in all three generated validators, plus two RTL hard-rejects at
`zhao_video_mode.sv:133` and `zhao_cmd_scheduler.sv:370`). **And that
unreachability is exactly what makes the fix safe: no golden capture can contain
mode 3, so correcting the read is provably golden-neutral.** Worth doing on its
own merits, before anyone adds a fourth mode and turns a latent bug into a live
one.

#### 3. 416 px width would have SILENTLY CORRUPTED the binner

Vindicating the choice of 384 for a reason nobody had raised.
`zhao_geom_binner.sv:205-217` has `GRID_W = 24` as a **compile-time parameter
embedded in the tile-RAM address arithmetic** (`:486-488` `tidx = ty * GRID_W`,
`:786`, `:805` row strides — deliberately a constant multiply, not a
multiplier). 416 px needs 26 columns. Feeding `grid_w_i = 26` makes the row
stride wrong by 2 and **tile rows overlap in the tile RAM** — lists alias. The
clamp at `:441-453` only protects against `grid_w_i` being too *small*.
26×15 = 390 entries still fits the 576-entry RAM, so it would be **silent
corruption, not an overflow.**

**384 width needs no binner change at all.** This alone settles 384×216 over
416×234.

#### 4. The partial-tile question has no answer, because the consumer is not built

Better news than it sounds. There is **no divisibility assertion anywhere** —
`tile_of()` is floor-and-clamp (`>>> 4`), and there is no round-up expression in
the repository. The grid is a **runtime input** (`grid_w_i`, `grid_h_i`, 6 bits),
so `ceil(H/16)` is simply passed in. Every current caller hardcodes an exact
quotient because every current mode divides exactly.

But the block that would clip a 16×16 tile against a 216-line canvas edge
**does not exist yet**: `zhao_raster_resolve.sv:41-46` explicitly excludes
framebuffer writes and tile scheduling, and always emits a full 256-pixel tile.
So this is **an unwritten requirement the tile scheduler will inherit anyway**,
not a blocker the proposal creates. The 384×224-store/216-display design avoids
needing it at all.

#### 5. Everything else: confirmed

Slot span `ZHAO_FB_SLOT_SPAN = 0x3C000` = 245,760 B for every mode
(`zhao_pkg.sv:126`) — 172,032 B fits with room. Line buffers really are
2 × 512 RGB565 (`zhao_scanout_linebuf.sv:5`), sized for Duo. `x[9:0]`, `y[7:0]`
(`zhao_pkg.sv:203-212`) — **Y caps at 255**, so 216 is fine and anything ≥ 256
active lines would need a frozen-package amendment. Z60 is 384×240 with
12 × 64 B whole bursts, Storm 10, Duo 8 per segment — all exactly as stated.
Both projectors take `vp_x0/y0/w/h` as **12-bit configuration** with **no fixed
aspect baked in anywhere** — the aspect lives entirely in the software-supplied
matrix. `VIDEO.SCALER` is pass-through with "no arithmetic exists in this block
by law". **No internal scanline, integer-scale or filter logic exists** — the
only pixel post-processing is 4×4 Bayer dither at RGB888→565, before the
framebuffer.

**And the Duo precedent is real:** Duo *stores* 196,608 B while *displaying*
245,760 B (`zhao_pkg.sv:100`), with scanout manufacturing border rows that fetch
nothing (`zhao_scanout_fetch.sv:125-140`). Stored occupancy and displayed bytes
are already distinguished by the design. The mechanism widescreen needs exists.

#### 6. Blast radius: ~61 live files across 8 areas

The heavy ones: **`zhao_pkg.sv` is FROZEN and has 18 sites**, including the
`ZHAO_TIMING[0:2]` bound that must widen; the ABI is **generated**, so
`spec/commands.zidl` plus `tools/abi-gen` must be edited rather than the seven
generated outputs; the `unique case (fetch_mode)` in
`zhao_scanout_fetch.sv:112-146` needs a fourth arm; `seg_geometry` in
`zref_video.cpp:283-312` likewise; four separate frame-deadline tables need a
fourth row; and the golden captures (`z60_10frame.zcap`, `storm_10frame.zcap`,
`duo_10frame.zcap`, `duo_markers.zcap` — a 600-frame CRC chain) would need
regeneration.

Mostly mechanical surface propagation, as the proposal said — but it touches a
frozen package and a generated ABI, so it is a deliberate amendment, not an
edit.

### Original audit list (all now answered above)

The claims that were checked:

1. the mode enum really is 2 bits with value 3 free, in RTL **and** in the ABI;
2. an out-of-set enum byte really is `ZH_ABI_BAD_VALUE`, so this must be a
   deliberate ABI amendment rather than a silent widening;
3. the framebuffer slot really is 245,760 B for every mode;
4. the line buffers really are 2 × 512 pixels, and X/Y really are 10 and 8 bits;
5. **whether anything hardcodes or budgets 360 tiles**;
6. whether the projector's viewport is genuinely configurable;
7. `VIDEO.SCALER`'s contract really is native-stream-formatter-only;
8. the full blast radius of a fourth mode;
9. **the Duo precedent** — that `zhao_scanout_fetch` really does hold a
   mode-specific displayed-row-to-stored-row mapping, and that stored occupancy
   and displayed bytes really are already distinguished. The whole "this is not
   a new architectural concept" argument rests on it.

### RULED, 2026-08-23 — the native mode, not anamorphic alone

Fabian: *"I confirm the widescreen, pick the better one we figured out."*

So the ruling is the **native square-pixel path**:

* **`VIDEO_WIDE`** — stored/tiled canvas **384 × 224** (24 × 14 = 336 exact
  tiles, 172,032 B), camera viewport and displayed raster **384 × 216** centred
  with 4-row guards, scanout mapping `displayed y -> stored y + 4`, 12 × 64 B
  bursts per row unchanged, presented as **1920 × 1080 at exact 5×**.
* **`WIDE_DUO`** — two **192 × 144** views (exactly 4:3, exactly 12 × 9 tiles),
  stored as two canvases with the 36-row bands manufactured at scanout the way
  Duo already does. 216 tiles and 1,728 bursts/frame — **cheaper than every mode
  the console currently has.**
* The **anamorphic 384×240** presentation is kept as a fallback for adapters
  that cannot do better, and is explicitly **not** the shipping presentation:
  at 1080p it is 5× horizontally but 4.5× vertically, so its scanlines cannot
  be uniform.
* Scanlines and CRT treatment live in the output scaler, never in the render
  framebuffer. `320×180` (stored 320×192, exact 6×) stays available as a very
  chunky performance mode later.
* **No native mode exceeds the existing 512×240 storage/scanout envelope in
  v1.** 416×234 is rejected outright — it would silently corrupt the binner's
  tile RAM (`GRID_W = 24` is a compile-time constant in the address
  arithmetic).

**Still not scheduled.** The DSP and timing campaign continues; this is the
target when a video wave opens. Prerequisite recorded above: the four
`else-is-DUO` assumptions and the three-entry `ZHAO_TIMING` table must be
amended deliberately, because "enum value 3 is free" is false in practice.

## 2026-08-23 — GEOM.SKIN done, and ONE DECISION FOR YOU: the shipped
## skinning reference silently truncates to 64 bits

`zhao_geom_skin` is rearchitected against your 120,000-vertex budget. That part
is reported with the run. **This section is the one thing I could not decide.**

### What was found

`reference/include/zref/zref_fixp.hpp:106`

    constexpr int32_t rescale_s32(int64_t x, int k, SatLedger* L, ...)

`reference/src/zcreature/creature_core.cpp:255`, inside `skin_vertex`

    *o[i] = rescale_s32(v.w0 * pa + w1 * pb, 22, L, &SatLedger::mul);

`pa` and `pb` are `__int128`, and the whole blend is formed in `__int128` — that
is the single-rounding law working exactly as intended. **Then the argument is
narrowed to `int64_t` by an implicit conversion**, and anything that does not
fit wraps.

It is not subtle where it bites. A blend of 1.0627e20 rescales to a saturating
`+0x7FFFFFFF` exactly; narrowed first, it wraps and saturates to `0x80000000`.
A sign flip at the rail.

Three things say the narrowing is unintended rather than a design choice:

1. `rescale_s32`'s own comment is *"The rounding add runs in s128: x near
   INT64_MAX must not wrap before the shift"* — it was written for s64 inputs
   and is careful about them;
2. a `rescale_s64(__int128 x, int k, ...)` exists twenty lines below it;
3. nothing anywhere depends on the wrap.

### Why nobody had seen it

**Because it is unreachable with a real bone matrix.** Measured: across 24,000
random pose-range coordinates, **zero** left the domain where the exact and
narrowed results agree. It needs |blend| >= 2^63, which needs matrix elements
and vertex coordinates near the s32 rails simultaneously — a pose no creature
has.

It surfaced only because `GEOM.SKIN.md` had recorded, correctly, that the
differential never reached the operand extremes and that this had to be fixed
**before** the weight-identity rewrite. It was fixed first, and this was behind
the door.

### It is NOT a regression from the rearchitecture

The old RTL carried 67- and 75-bit lanes and did not truncate either, so the
shipped block diverged from the shipped oracle in exactly the same places, for
as long as both have existed. Nothing had ever compared them there.

### THE DECISION, and I have not taken it

Three options, and I deliberately took none of them because all three change
either the reference renderer's arithmetic or the console's:

**(a) Fix the reference** — change `skin_vertex` to call `rescale_s64`, or add a
`__int128` overload of `rescale_s32`. Correct, one line, and it changes the
arithmetic of the function **every shipped picture was skinned with**. In the
unreachable region only — but "unreachable" is a measurement over the poses that
exist today, not a proof.

**(b) Make the RTL imitate the narrowing** — truncate to signed 64 before the
rescale, and the hardware matches the oracle bit for bit everywhere including
the wraps. Also correct, in the sense that the oracle is the law. It bakes a C++
implicit conversion into silicon, and it makes the block WRAP where its contract
says "saturation, never wraparound".

**(c) Leave both, and pin the boundary** — which is what is committed now. The
differential checks the shipped oracle wherever the oracle is well defined
(3,976 coordinates in the directed lane, all 24,000 in the random lane) and
above the boundary it checks the exact arithmetic the RTL claims, saying so in
the check name. The divergence is documented in the contract and counted in the
test output.

My recommendation is **(a)**, because the narrowing is an accident in a helper's
signature rather than a rule anyone chose, and because (b) commits silicon to
reproducing it forever. But (a) touches the reference renderer, and that is
yours.

Whichever you pick, the RTL does not change under (a) or (c).

---

## 2026-08-23 — Field IR engine done: 79 DSPs -> 3. Two things
## for you, one of them a defect in how every block fit has ever been measured

The ruled rearchitecture is implemented and measured. `zhao_field_seq` plus its
dependencies went from **79 DSP blocks of 112** to **3**, with
ALMs 10,615 -> 8,901. Simple ops still retire in
**six clocks**; the worst op is **67** against the 96 you set. All 31 opcodes are
bit-exact against `zfield::interpret`, the anti-hang proof is green with a
derived bound, and the alone-versus-interleaved contamination test is green and
proven non-vacuous by a mutant.

Detail in
`runs/CLAUDE-RUNS/RUN-20260822-2136-.../FINDINGS-dsp-field-engine.md`.

### 1. I did NOT build the parameterized two-lane variant, and here is the number

The mid-flight brief asked for `MUL_LANES` as a parameter with 1 and 2 both
fitted, so the Pareto point is measured rather than guessed. I have not built
it, and this is the reason rather than an excuse.

**Neither axis binds.** DSPs are at 3 against a target of 8. Worst
op is 67 against a ceiling of 96. A Pareto frontier is information about a trade,
and there is no trade here: a second lane spends the entire remaining DSP budget
to buy latency on a constraint that is already met with 30% margin.

**And the latency it would buy is small, because the multiplier is not what the
long ops are waiting for.** Cycle accounting per op, derived from the state
machines and cross-checked against the measured totals that section 12 of the
differential prints:

| op | measured | ceiling with UNLIMITED multipliers | what the rest is |
| --- | ---: | ---: | --- |
| MUL / MAD / DOT2 / DOT3 | 6 | **6** | fetch, latch, three register reads, execute |
| RCP | 15 | 14 | two dependent product stages |
| NOISE2 | 29 | 29 | a strict hash chain: mix -> LCG -> xor-shift |
| SPLINE | 45 | 45 | 12 clocks of binary search, then a strict Horner chain |
| LEN3 | 48 | 46 | **36 of the 48 are the integer square root** |
| RING | 54 | 52 | nine products, every one dependent on the last |
| NORMALIZE3 | 67 | 63 | **36 are the root**; the correction is a 4-step chain |

The binding resource in the two most expensive ops is `zhao_field_isqrt` --
thirty-four clocks of restoring digit recurrence with **no multiplier in it at
all**. A second multiplier lane cannot touch it.

So the honest reading of the brief's own principle -- measure the frontier, pick
the cheapest point that clears the workload with reserve -- is that the frontier
in this block has one interesting point and I have fitted it. **If you want the
long ops shorter, the lever is a two-bit-per-cycle integer root (34 clocks -> 17,
zero DSPs, roughly double the compare-and-subtract logic), not a second
multiplier.** That is a separate change and I have not made it, because nothing
requires it.

**One free win is measured and unspent.** ROT issues its four products serially
although they are independent; issuing them back to back on the SAME lane takes
ROT2/ROT3 from 24/25 clocks to about 16, for zero DSPs. I did not take it because
no acceptance criterion moves and it would have invalidated a running mutation
sweep. It is recorded here so it is a decision rather than an oversight.

### 2. THE FIELD ENGINE CANNOT MEET THE CLOCK, AND THE PATH IS ONE WIRE

Your SDC fix gave this block its first timing numbers, and they change what the
next Field task should be.

| | Fmax | WNS setup | TNS setup |
| --- | ---: | ---: | ---: |
| before | **7.72 MHz** | -119.5 ns | -2,389,303 ns |
| after | **8.59 MHz** | -106.4 ns | -2,122,226 ns |

Twelve times too slow against 10 ns, on BOTH sides. The DSP work bought 11% and
was never going to buy more: it removed multipliers, and multipliers are not the
path.

I asked TimeQuest instead of guessing. All three worst paths are the same wire,
with **78 levels of logic in one cycle**:

```
from  zhao_field_normalize|h_rt[41]      (the integer root's held answer)
to    zhao_field_normalize|o1_o[1]       (a normalised output lane)
```

That is the exponent extraction, written as two SIXTY-FOUR-ITERATION
combinational loops that unroll into 128 sequential compare-and-shift stages on
a 64-bit value, feeding both the seed ROM index and the per-lane rescale shift
in the same cycle. **The same loops are in the pre-change design**, which is why
both sides sit at 8 MHz, and they were invisible for exactly the reason your
census note gives: no per-block fit had ever been given a clock.

**The fix is small and I did not make it.** `zhao_field_rcp` already does this
correctly a few lines away — a leading-zero count, log depth instead of linear.
Replacing the two loops with one, or simply registering `(m, e)` in a state of
their own, costs **one clock on an operation with thirteen clocks of margin**
under MAX_OP_CYCLES.

I left it alone because it is a different change with its own verification cost
— it would invalidate the mutation sweep and both fits — and because you should
decide whether Field timing is worth a wave now or after the other DSP blocks.
**My recommendation: do it with the next Field touch, not as a special trip.**
It is bounded, it is cheap in latency, and until it is done no Field number
about timing means anything except "not close".

### 2b. NO PER-BLOCK FIT HAS EVER CARRIED A TIMING NUMBER, AND NOW I KNOW WHY

This is the one that matters beyond this block.

`tools/quartus/run_block_fit.ps1` copies `zhao_shell_fit.sdc`, which says:

```
create_clock -name gpu_clk -period 10.000 [get_ports {gpu_clk}]
```

Every leaf block's clock port is called **`clk`**, and the block flow makes all
I/O virtual. So `get_ports {gpu_clk}` matches nothing, no clock is created, and
**every one of the 47 rows in `reports/synthesis/zhao_block_fit.json` was fitted
UNCONSTRAINED.** The fitter had no timing objective. That is why no row has ever
carried WNS, TNS or Fmax: there was never a clock to report against.

What that does and does not invalidate:

* **The ALM and DSP numbers stand.** Those are placement and resource counts and
  they are what the census was built for.
* **Anything anyone infers about block-level timing from that file is void**, and
  more subtly, an unconstrained fit optimises for area rather than speed -- so
  the ALM numbers are, if anything, the optimistic end for a design that will
  later be asked to close timing.

I measured Fmax for both sides of this block separately, with `derive_clocks`
against the real fitted netlist, and both numbers are in the FINDINGS. I have
**not** changed `run_block_fit.ps1`, because doing so would change what every
future row means and half the existing rows would no longer be comparable to the
new ones. That is your call:

* **(a)** leave it, and treat the census as an area census only, saying so in the
  file's `limitations` list; or
* **(b)** add a per-block SDC that clocks `clk` at the gpu period, re-measure the
  blocks that matter, and accept that old and new rows are not comparable.

I recommend **(b) for the blocks still to be rearchitected** and leaving the rest
alone -- the DSP wave is going to re-fit them anyway, so the re-measure is nearly
free, and a timing number is exactly what you will want when the composed fit's
setup slack is the open question.

---

## 2026-08-22 — CDC seam DONE; two follow-on calls before the A/B remeasure

The ruled move is implemented: **DEBUG.CRC now runs in `vid_clk`** and nothing
per-pixel crosses the clock boundary any more. The block takes one displayed
RGB565 pixel per video clock and folds its two bytes in one `zhao_crc32c_fold`
tree; only the finalized 32-bit CRC crosses to `gpu_clk`, once per frame, on a
toggle with the value held stable beside it — the same pattern
`zhao_video_framectl` already uses and that
`tests/formal/video_framectl_one_fence.sby` already proves. The document
contradiction is closed the way you ruled: `design/blocks.yml` now says
`clock_domain: video`, matching `DEBUG.CRC.md`, which had been right all along.

Proven in **simulation only** — no fit was run for this change, because the
composed fit is yours to run. What was measured: 57 directed checks and 2,100
random checks on a cross-granularity differential (the device driven with
PIXELS, the shipped `zref::Crc32c` driven with the same stream as BYTES), the
shell lane green (`shell_golden_replay`, `shell_duo_markers_fast`), and a
22-mutant sweep: 22 attempted, 22 accounted, **20 caught**, 2 survivors, both
of them mutants proven EQUIVALENT in `tools/sweep_debug_crc.sh` (no input can
reach either).

**Two things I did not decide, because deciding either would change what your
A/B measures.**

### 1. Should `gpu_clk` and `vid_clk` now be cut in the SDC?

`fpga/quartus/shell_fit/zhao_shell_fit.sdc` says, deliberately:

> GPU and video are deliberately NOT cut from one another: the known
> phase-dependent displayed-byte crossing must remain visible in TimeQuest.

That crossing no longer exists. Every remaining `vid_clk <-> gpu_clk` path
except one (see 2) is a toggle handoff with its data held stable for a whole
frame, which is exactly the shape a clock-group cut is FOR. Cutting them would
almost certainly take the remaining vid/gpu hold analysis to zero.

**That is the reason not to do it silently.** A cut makes numbers better by
telling the tool to stop looking, and your A/B is specifically about hold. I
left the SDC untouched so the run you make measures the RTL change alone. If
you want the cut afterwards it is four lines, and the honest form is an
asynchronous clock group plus `set_max_skew` on the toggle handoffs, not a
blanket `set_false_path`.

### 2. `starvation_o` is the last unstructured vid -> gpu crossing

`zhao_shell_top` samples the video domain's 64-bit `scanout_starvation_cycles`
counter straight into a GPU register for counter id 30. It is guarded by a
PROTOCOL argument, not by structure: the counter only moves during active
lines, the frame tick lands in vblank, and `shell_err_cdc_o` trips if the value
ever moves across the sample window. The argument is sound, and it is still 64
bits of asynchronous data with 64 timed paths behind it — the only remaining
`vid_clk -> gpu_clk` family that is not a toggle handoff.

If the two hold violations you saw were on THIS family rather than on the CRC,
the remeasure will still show them. **Your call:**

* **(a)** run the A/B now, on the ruled change alone, and see what is left; or
* **(b)** let me convert the starvation sample to the same toggle handoff
  first — latch the counter in vid at the frame tick, cross a toggle, drop the
  tripwire to a redundant check — and run the A/B once against both fixes.

I recommend **(a)**: it is one change per measurement, and if the holds are
gone the second fix becomes optional rather than urgent. But (b) is the smaller
number of 40-minute fits if the holds turn out to be there.

---

## RULED 2026-08-23 — measure a Pareto frontier, not one point. And 105 DSPs is
## not reserve, it is the ceiling.

A survey of comparable FPGA GPU projects — Raster I, RasterIX, Vortex, eGPU,
SIMTight, and the time-multiplexed FPGA overlay literature — relayed by Fabian
and adopted. It **confirms** the direction taken today and **corrects the
process** around it.

### What it confirms

Every one of those projects reaches the same conclusion Zhaozhou reached this
week: *do not instantiate every mathematically independent operation in
parallel; choose a sustained rate, build only enough arithmetic for it, and
time-share the rest.*

The overlay literature reports up to **85% resource reduction** from replacing
spatial one-unit-per-operation hardware with time-multiplexed DSP functional
units. So today's results are not flukes:

```
GEOM.LOD     18 DSP -> 6
TERRAIN.LOD  28 DSP -> 3
```

It also independently validates the exact boundary already adopted: **time-share
WITHIN a subsystem, never build a console-global multiplier.** Field ops are
mutually exclusive with each other; they are not mutually exclusive with
rasterisation, terrain projection or texture filtering.

And it names the 72-bit mistake in `zhao_geom_lod` for what it was: **"slack on
an operand is not free when it changes DSP decomposition"** — a textbook FPGA
anti-pattern, caught here only by fitting the block.

### THE CORRECTION THAT MATTERS MOST: one point is not an architecture

> Do not let the agent implement only one "optimized" architecture per block.
> Make it preserve two or three parameterized resource points and measure the
> Pareto frontier.
>
> That is the clearest difference between Zhaozhou's work so far and mature FPGA
> GPU research. **Zhaozhou has been discovering the right point one emergency at
> a time.** Vortex, RasterIX and eGPU make the choice of point part of the
> architecture.

Vortex's own numbers show why guessing fails: 1 → 2 cache virtual ports cost 9%
more LUTs; 4 ports cost 25%. They chose **2** — neither the fastest nor the
smallest — and could only choose it because all three were measured.

**Variants worth fitting, per subsystem:**

| subsystem | variants |
| --- | --- |
| Field | 1 and 2 shared multiplier lanes |
| Skin | 3 and 6 row lanes |
| Cull | 1 and 2 arithmetic lanes |
| TMU | 1, 2 and 4 channel accumulators |
| Normals | 2 and 6 multipliers |
| Stamp | 1 and 2 square lanes |
| Project | cached 3-lane; cached 6-lane |
| Raster | one and two fragment lanes |

Each row records: source commit, device and tool version, DSP / ALM / registers
/ M10K, WNS / TNS / hold, latency, sustained ops per clock, ops per 60 Hz frame,
frame-budget utilisation. **The canonical build then selects the cheapest point
that clears its workload with reserve** — not whichever version was coded last.

### THE TARGET WAS TOO LOOSE

I recorded 90–105 DSPs as the goal. On a 112-DSP device that is **80% to 94%** —
not reserve, effectively the ceiling, before the ~40 still-unfitted modules,
synthesis drift and physical placement.

| resource | development target | warning line |
| --- | --- | --- |
| DSP | **≤85–90 / 112** | **>95** |
| ALM | ≤70–75% | >80% |
| M10K | ≤70–80% | >85% |
| timing | positive slack with meaningful margin | "barely zero" |

Raster I is the cautionary case: a real, working tile-based FPGA renderer at
**69% LUT, 88% DSP, 97% BRAM** on an Arty A7-100T. It works — and it has
essentially no room for the next required feature. Optional texture support
"if enough BRAM remains" is acceptable in a research demo and is not a console
architecture.

### Three techniques to apply before sequencing anything

1. **Ask whether the calculation should exist multiple times AT ALL, first.**
   SIMTight cut register-file storage 68% by recognising genuinely shared values
   instead of copying them per lane. This is the insight that turned
   `TERRAIN.PROJECT` from "three times slower" into "almost twice as fast": the
   projections were duplicated, not merely parallel. Live analogues here:
   instances sharing a decoded pose; camera matrices and frustum-plane lengths
   are per camera not per instance; Field constants are per program not per
   sample; TMU weights are per sample not per RGBA channel; stamp radius squares
   are per stamp not per texel; repeated texture addresses should issue once.
2. **Share modes rather than duplicate machinery.** Vortex implements only
   bilinear filtering — point sampling runs through the same sampler with zero
   blend weights, because a separate one-cycle path was not worth the muxing,
   and trilinear is a pseudo-instruction invoking the primitive twice. So:
   nearest stays the bilinear identity; rare quality modes spend **cycles, not
   permanent silicon**.
3. **Spend M10K to avoid DSP and DDR traffic.** Universal across every design
   surveyed. M10Ks are not merely storage — they replace arithmetic, ALM muxes
   and bandwidth. The projected-vertex cache is the local example: ~82 kbit per
   patch, nine or ten M10Ks against 553 available.

### Two structural changes to consider (not yet ruled)

**Build configurations, as RasterIX does.** It exposes hardware capability as a
compile-time choice — 4.5k LUT minimal, 11k typical, 36k full — behind one
unchanged OpenGL-facing API, with the driver parameterised to agree. The
Zhaozhou analogue would be `ZH_COMPACT` / `ZH_BALANCED` / `ZH_THROUGHPUT`
sharing one command ABI and one Form program set, with the Measure enforcing the
capability the target declares. **Not three consoles — one console, three
budgets, so a Pareto point is never lost by being overwritten.**

**Placement zones, as eGPU does.** eGPU budgeted by physical FPGA sector (16,400
ALMs / 164 DSPs / 237 M20Ks, four units per sector) and matched each unit's
resource *proportions* to the device's, which is what let it replicate without
losing frequency. The Zhaozhou analogue is grouping subsystem farms so they do
not reach across the chip: terrain zone (projector, normals, terrain LOD,
projected-vertex M10Ks); creature zone (pose staging, skin, cull, creature LOD);
texture/raster zone; field/surface zone. Worth doing **before** an apparently
legal 108-DSP machine fails routing.

### Also worth noting: hardware/software partitioning is not cheating

RasterIX runs its vertex pipeline in software. Vortex accelerates selectively.
Neither treats CPU assistance as a compromise — they put regular, rate-sensitive
work in hardware and irregular work where software is cheaper. For Zhaozhou the
800 MHz ARM remains the right home for scene traversal, dynamic fracture,
unusual mesh processing, command generation, gameplay and cache preparation.

---

## RULED 2026-08-23 — the DSP rearchitecture: per-subsystem multiplier farms,
## and a vertex budget that finally exists

Relayed by Fabian from a collaborator, and adopted. This supersedes my
block-by-block improvisation with an architecture rule and a target table.

### The rule, which is the important part

> **Do not build one global multiplier for the entire console.** Terrain,
> texture, Field and creature geometry genuinely run concurrently.
>
> Instead: give each major subsystem the smallest local multiplier farm that its
> **sustained rate** actually needs. Share only operations that are **mutually
> exclusive inside that subsystem**.
>
> That should become the DSP equivalent of the RAM-inference rule.

And the framing that reorders everything: **DSP allocation is justified by
sustained frame demand, not by preserving one-clock placeholder throughputs.**
Several blocks in this design are one-clock because that is what the first
handshake happened to deliver, not because anything measured a requirement.

### The targets

| subsystem | now | first credible target |
| --- | ---: | ---: |
| Field IR engine | 79 | **8–12** |
| `GEOM.SKIN` | 72 | **12–18** |
| `TERRAIN.PROJECT` | 33 | 12–18, *after* removing repeated projections |
| ~~`TEXTURE.TMU`~~ | ~~28~~ → **6** | ~~8–12~~ — **landed at 6**, below this audit's estimate |
| `SURFACE.STAMP` | 28 | 4–6 |
| `TERRAIN.NORMALS` | 18 | ~6 |
| `GEOM.CULL` | 15 | 4–6 |
| `GEOM.BINNER` + `SETUP` | 16 | 4–6 |
| `RASTER.FRAGMENT` | 10 | ~7 |

Landing these puts the currently-measured subset near **90–105 DSPs** — reserve
rather than 112/112 desperation. Explicitly **not** a full-console prediction,
because half the modules remain unfitted.

### THE OWNER RULING THAT WAS MISSING: a vertex budget

> **Zhaozhou v1 guarantees approximately 120,000 skinned vertex instances per
> 60 Hz frame across all active views. The Measure must degrade before
> exceeding that.**

This is the number I said was needed and could not invent. `GEOM.SKIN` currently
spends 64% of the device's DSPs on a "one vertex per clock" target that its own
contract admits is unbacked. At 100 MHz an 11-clock weighted engine delivers
~151,000 weighted vertices per frame; at 95 MHz, ~144,000. So 120,000 has real
reserve. If it later proves too low, **duplicate the three-lane engine** — two
15-DSP engines are still far cheaper than one 72-DSP combinational block.

### The two designs that matter most

**Field IR — one arithmetic engine, not ten idle calculators.** One registered
signed 33×33 universal multiplier service, one shared isqrt, one shared sine
table, one ordinary-RCP ROM and one normalize-RCP ROM. **No production op unit
may keep a private nonconstant multiplier.** The three existing register-read
cycles `Q_RD0/1/2` are free scheduling slots, so MUL, MAD, DOT2, DOT3 and the
square-gathering for LEN/NORMALIZE stay at the **existing six-clock cadence**.
Only genuinely complex ops lengthen; worst instruction must stay **≤96 clocks**,
inside the present 120-cycle anti-hang window — and that bound must be re-proven
from a named `MAX_OP_CYCLES` rather than left a magic constant.

Staged, not one flattening commit: introduce the service; move NOISE, ROT, RING
(which already walk a local multiplier) onto it; share isqrt between LEN and
NORMALIZE; move ALU multiply/DOT into the read states; sequence NORMALIZE, RCP,
CURVE; delete the orphaned multipliers and duplicate ROMs; refit.

**GEOM.SKIN — three row lanes, not 24 simultaneous operators.** Three 32×32
lanes, one per output row: matrix A in three cycles, B in three more only for
weighted vertices, so a **rigid vertex stops paying for the two-bone circuit**
(today it pays in full, because the rigid branch is only a result mux). And the
blend uses the identity the contract already documents:

```
w0*pa + (64-w0)*pb  ==  (pb << 6) + w0*(pa-pb)
```

walked as shift-add or radix-4 digits through reused wide adders — **not another
six DSP multiplies**. Exact, single final rounding preserved. Rigid ~4 clocks,
weighted ~10–11.

### TERRAIN.PROJECT: do NOT serialize it first

The most important larger point, and it inverts the obvious move. The projector
transforms **triangle** vertices, so a 33×33 patch does **6,144 projections for
1,089 unique lattice vertices** — the source itself records the duplication as up
to sixfold.

```
today:                6,144 projections x 1 clock = 6,144 clocks/patch
cached + sequenced:   1,089 projections x 3 clocks = 3,267 clocks/patch
```

**A three-cycle projector with a projected-vertex cache is almost twice as fast
per patch** while cutting nine simultaneous matrix products to three. A cached
vertex is ~75 bits; a patch is ~82 kbit, roughly nine or ten M10K blocks per
in-flight view against 553 available. Serializing before caching would be
strictly worse. This is a separate architecture wave, not a local edit.

### Process rulings adopted with it

* **Mutation sweeps must run in separate git worktrees with separate build
  directories.** The terrain sweep contaminated other targets with
  mutant-generated Verilator sources and made clean RTL look broken — sharing one
  build tree between agents "is no longer defensible". This is the standing
  permission for worktrees that the project's own rule otherwise requires be
  asked for each time.
* Every block gets: clean baseline commit → baseline block fit → mutation
  preflight → rearchitecture → direct + random + **composition** tests →
  explicit **alone-vs-interleaved contamination** test → clean after commit →
  after block fit → ledger/report update.

### Order

1. Field shared arithmetic engine
2. Three-lane skin prototype, against the 120k budget
3. `GEOM.CULL` one-lane
4. `SURFACE.STAMP` one square lane + numerator recurrences
5. TMU weight hoist, refit, then channel-walk if still needed
6. `TERRAIN.NORMALS` over three clocks
7. Projected-vertex cache + row-sequenced projector
8. Binner/setup and fragment
9. **Only then** a graphics-composed fit, and reassess what is genuinely left

---

## MEASURED 2026-08-22 (night) — the re-measure you were owed, and it answers
## the (a)/(b) question the CDC agent left open

The agent offered two ways forward after moving the displayed CRC into `vid_clk`:
**(a)** measure now on the ruled change alone, or **(b)** let it convert
`starvation_o` to the same toggle handoff first. I measured (a). Here is what it
says.

### The headline, and it is worse

| | `6d23c84` (before) | `cbb6eab` (after the CDC fix) |
| --- | ---: | ---: |
| setup worst | **-0.475 ns** | **-1.991 ns** |
| failing setup endpoints | **56** | **836** |
| setup TNS | — | **-253.5 ns** |
| hold worst | +0.253 ns | **-0.952 ns** |
| hold failing endpoints | **0** | **1** |
| ALMs | 7,415 | 7,442 |

Reproduced exactly on a second run (`-1.991` / 836 again), so this is the design,
not noise. The composed fit remains deterministic.

### But the SHAPE of it is the finding

Per clock:

```
setup:  gpu_clk  -1.991   TNS -253.490
        vid_clk  +1.469   TNS    0.000     <- completely clean
hold:   gpu_clk  -0.952
        vid_clk  +0.366               <- clean
```

**`vid_clk` is spotless.** The CRC move did exactly what it was supposed to: the
video domain now closes with 1.5 ns to spare and the 122,880-per-frame pixel
crossing is gone. Every failure is in `gpu_clk`.

### And the worst path is the crossing the agent TOLD US would still be there

```
-1.991 ns   zhao_video_scanout|zhao_scanout_serializer|starve_q[57]  ->  cdc_err
            vid_clk -> gpu_clk
```

That is `starvation_o`: `shell_top` compares the whole 64-bit counter against a
sampled copy across the domains —

```systemverilog
if (tick_d1 && (starvation_o != starve_samp)) cdc_err <= 1'b1;
```

— a 64-bit cross-domain compare guarded by a quiescence argument rather than a
structure. It is **not new**: `cdc_err` dates to `3971d86`, the original Phase-2
shell. It was always unsound; it simply was not the worst path while the CRC
crossing existed.

The agent predicted this outcome in its own words before the measurement: *"If
the two hold violations were on that family rather than the CRC, the remeasure
will still show them."* They were, and it does.

### The rest of the 836

The next families are GPU-internal and pre-existing, but worse than they were:

| paths | family | worst |
| ---: | --- | ---: |
| 27 | `cmd_dma hdr_win -> crc_pay_r` | -0.875 |
| many | `f_pos -> recq[2][*]` | -0.765 |

**A caution on reading those.** Quartus optimises worst-path-first, and a
-1.991 ns path consumes optimisation effort that the -0.7 ns families would
otherwise have received. So the GPU-internal degradation may be a CONSEQUENCE of
the starvation path rather than an independent regression. That is testable and
it is exactly what option (b) tests.

### What this changes about the recommendation

**(b), and the evidence is now strong.** The remaining crossing is the worst path
by a factor of 2.3, it is the same seam and the same class of defect the ruling
already addressed, and converting it to the toggle handoff the CRC now uses is
the same fix applied to the same problem rather than a new decision.

**The SDC is still untouched and still yours.** `gpu_clk` and `vid_clk` remain in
one group, which is why this path is visible at all. Worth noting the honest
distinction: declaring the groups asynchronous once *every* crossing between them
is properly synchronized is the CORRECT constraint, not a way of hiding — but
that is only true once `starvation_o` is fixed too. Doing it now would hide a
genuinely unsound path.

**What the trade actually is.** The old arrangement measured better *because* it
was sampling 122,880 pixels a frame across a seam that only works when a
simulator holds two clocks in lockstep. Trading a fictitious 56 endpoints for a
real 836 is not obviously a bad trade — but it is a trade, and the way out is to
finish the seam rather than to re-hide it.

---

## 2026-08-22 — CLOCK TARGETS: 120 MHz fabric + 150 MHz DDR (owner ask)

**Fabian, first:** *"Stretch target for 120 MHz. Historical reasons — GeForce 256
had that and it went with Sacrifice."*

**Fabian, clarifying:** *"That'd be 120 MHz GPU fabric, 150 MHz DDR interface.
That's the GeForce 256."*

So the target is the **GeForce 256 DDR** part, spelled out properly:

| lane | target | period | today |
| --- | ---: | ---: | --- |
| GPU fabric (`gpu_clk`) | **120 MHz** | 8.333 ns | 10.475 ns measured = **95.5 MHz** |
| memory interface | **150 MHz DDR** (300 MT/s) | 6.667 ns | **not characterized at all** |

**On the fabric number.** 100 MHz is not closed yet: worst setup at `6d23c84` is
10.475 ns, so the machine is good for about 95.5 MHz. 120 MHz is not 4.75% more,
it is **20.4% more**, and the previous campaign already took the worst path from
65 ns to 10.475 ns and ended placement-bound rather than logic-bound. The seven
fixes that bought those 55 ns were all accidental combinational depth — real
defects, now gone. Another 20% on a placement-bound design is pipelining,
floorplanning or a faster device: architecture, not cleanup.

**On the memory number, which is the more interesting half.** 150 MHz DDR is not
an RTL target at all — it is the SDRAM controller, the I/O timing and the board.
None of it is characterized today: the composed fit has **zero package pins**,
all harness I/O is virtual, and `MEM.SDRAM` is SPECIFIED / blocked_on: hardware
with a depth-900 refresh proof that has never finished. So this number cannot be
costed, or even honestly estimated, until the board is frozen. Recorded as the
target it is.

**Sequence, unchanged by this.** 100 MHz first, and the CDC seam before that
(the ruled next move) — moving that logic changes placement, so any 120 MHz
measurement taken before it would be measuring the wrong design.

**One thing worth flagging about the medium.** The GeForce 256 ran 120 MHz in
1999 on 220 nm dedicated silicon. This is an FPGA, where the fabric is roughly
an order of magnitude slower per gate — and the stated destination is
**fabricated silicon**, where 120 MHz stops being ambitious at all. The honest
reading of the ask is "match a GeForce 256 on the real device": a hard target on
the FPGA lane, and a low bar on the silicon lane. Worth deciding which lane the
number is meant to bind, because it changes whether it is a stretch goal or a
floor.

---

## 2026-08-22 — THE CULL IS BUILT. Three things it cannot decide for itself.

`zhao_geom_cull` implements the ruling above: five planes out of the shipped
view-projection, a bounding-sphere test with a CEILING length bound, and
rejection only when no active camera can see the sphere. Reference
(`zref::cull`), differential and mutation sweep are in. Simulation only — the
block has never been through a Quartus fit.

Three questions came out of building it. None is a blocker today, because the
block takes its bound as ports; each becomes one the moment something consumes
it.

### 1. WHICH SPACE IS `bound_centre` IN? This is the load-bearing one.

The whole reason the cull is cheap is that the five planes and their five square
roots are extracted **once per camera per frame** — 185 cycles on a matrix
write, against potentially thousands of instances. That is only true if the
sphere centre arrives in the SAME space the configured matrix consumes.

If instances instead carry a model transform and MESHFETCH were expected to
compose model x view x projection per instance, the extraction would move onto
the per-instance path and the block would cost 185 cycles *per instance*. It
would still be correct and it would be useless.

The cheap arrangement is for the caller to hand over a **world-space** sphere:
transform the centre by the model matrix (three dot products) and scale the
radius. The ruling above already contemplates this — "transform the bound into
camera space" — so this is likely just confirmation. But **the radius scaling
law is not specified anywhere**, and it must be conservative: for a non-uniform
model transform, the radius has to grow by an upper bound on the transform's
largest scale factor, and a floor there deletes geometry exactly as a floor on
the plane length would. Options, cheapest first:

* forbid non-uniform scale on culled instances and multiply by the uniform one;
* multiply by an upper bound on the largest scale (e.g. the largest row norm,
  rounded UP), which is loose but never wrong;
* carry a pre-scaled world radius in the instance data and make it the asset
  pipeline's problem.

This matters more here than in most engines because the stated identity feature
is terrain that rotates and deforms in real time.

### 2. THE MESHLET DESCRIPTOR FORMAT is still the only thing missing.

`blocks.yml` still says "Meshlet limits are Phase-0 data (P2 risk 1) — schema
fields stay unfrozen", and `zref::MeshFetch` still resolves to nothing. Two of
GEOM.MESHFETCH's three jobs now have RTL (the LOD ladder and the cull) and
neither needed the format. The third IS the format. Nothing was invented to fill
the gap.

### 3. WHO DRIVES `active_i`, the two-bit active-camera mask?

The block takes it as a port because the ruling makes rejection depend on it,
and it deliberately has no default: with no camera active the verdict is
"reject", which is correct (nothing is drawn, so nothing is lost) but is also
exactly what a caller that forgot to drive the signal would get. `CMD.SCHEDULER`
is the obvious owner — it already knows whether the frame is Duo — but nothing
says so.

### 4. THE LOD LADDER NOW TAKES FIVE CLOCKS. Is that acceptable to whatever
### ends up calling it?

`zhao_geom_lod` was one combinational evaluation per clock and 18 DSPs. It is
now five clocks and **6 DSPs**, because its five 32x32 products walk through one
multiplier instead of standing side by side. Measured both ways on the same
machine: 1,303 ALMs / 18 DSPs before, 1,183 ALMs / 6 DSPs after -- area fell
too, so this is not the usual area-for-DSP trade.

The rate argument is that the ladder runs **once per instance per frame**: ten
thousand live creatures at 60 Hz is 600 k evaluations/s and the sequenced block
sustains 10 M/s at 50 MHz, a 16x margin. That argument is sound but it rests on
a number nobody has ruled: **how many creatures are live at once**. Charter §10
does not say, and `zref::MeshFetch` resolves to nothing, so the consumer that
would answer it is unbuilt.

**What is being asked:** nothing, unless the instance count is far larger than
assumed. If a future GEOM.MESHFETCH needs one LOD answer per clock, the parallel
form is one commit back (`d8278bd`) and costs 18 of the device's 112 DSPs. This
is recorded so the five is read as a *choice made against a stated rate*, not as
an inherited property of the block.


---

## RULED 2026-08-22 — "visibility sectors" is deleted. MESHFETCH culls a
## bounding sphere against each camera frustum.

I asked what a "camera visibility sector" was, because the phrase appeared
exactly twice in the repository and both were the block's own purpose line.
Fabian's ruling, recorded as given:

> "Somebody had a vague idea of spatial cells/portal sectors and wrote it into
> the ledger before any such system existed. Since the phrase has no
> corresponding data structure, algorithm, or format anywhere else, delete the
> word 'sectors' rather than invent a subsystem to justify it."

**THE LAW, as ruled:**

* `GEOM.MESHFETCH` performs **conservative per-camera frustum rejection of an
  instance/meshlet bound, before vertex decode**.
* The bound is a **bounding sphere**: `bound_center` + `bound_radius` in the
  descriptor. Not an AABB — a sphere is a few subtracts, multiplies and
  compares with no corner-walking, and a loose bound only costs performance.
* Per active camera: transform the bound into camera space, test the sphere
  against the frustum planes.
* **Reject only when the sphere is outside EVERY active camera.** In Duo, cull
  only if outside both.
* Optionally carry a **two-bit visibility result** (camera 0, camera 1)
  downstream, so work that genuinely is camera-specific is not duplicated.
* Static/rigid meshes take an **asset-generated** bound. Animated creatures take
  a **conservative animation-safe instance bound** — per-pose exact bounds are
  explicitly NOT required.
* `GEOM.CLIP` remains the exact per-triangle screen rejection stage. The two are
  complementary, not alternatives.
* **"Visibility sectors" is deleted. No sector system exists.**

**Explicitly forbidden for now:** meshlet occlusion sectors, BSP cells, portals,
island visibility grids, Hi-Z occlusion. Each needs new scene-format laws,
dynamic-update behaviour, memory structures and probably toolchain
participation — and for a world of floating, deforming, rotating terrain a rigid
baked visibility system could become actively annoying. Another rejection bit
can be added in front of MESHFETCH later without changing this law.

**Why here and not in GEOM.CLIP** (the option I had offered and Fabian rejected,
correctly): MESHFETCH feeds `GEOM.VDECODE` and `GEOM.POSE`, so rejecting an
invisible object here avoids compressed vertex fetch and decode, pose work,
skinning, projection, setup, binning and rasterisation. `GEOM.CLIP` receives
already-projected individual triangles — its cheap scissor test comes far too
late to save any of that.

**A correction to my own framing.** I had written that the existing projection
code "defines it completely". It does not: projection defines the camera and the
frustum, but the coarse BOUND REPRESENTATION was still an open choice, and the
bounding-sphere ruling above is what closes it.

---

## RULED 2026-08-22 — ONE ENGINE, FIVE PROFILES.

**Fabian's ruling: option 2 below.** One sequencer block; the five profiles
become configuration, and their ops are attributed to the blocks that consume
the output. `FIELD.SEQ.CORE` is already RTL_VERIFIED and is a complete engine,
so this is a ledger and contract change rather than new RTL.

What follows is the question as it stood, kept because it records WHY the
profiles were never distinguishable in hardware.

## (ruled) Are the five FIELD.SEQ profiles five blocks, or one used five ways?

Nothing in the RTL distinguishes them. `zhao_field_seq` has no profile input
and no profile-specific port. The thing that would distinguish them — which
registers the input and output lanes bind to — is carried by the DECODED
PROGRAM (`zfield::Decoded::in_lanes` / `out_lanes`, filled by the decoder from
the image), not by the block.

So a "profile" appears to be a program set plus shell wiring, not a hardware
variant. The ledger models five blocks (`EARTH`, `WARP`, `FLOW`, `FORMATION`,
`STAMP`), each wanting its own directed and random test.

I cannot write those tests without first deciding what each profile's I/O
contract is: `ops.yml` defines the profiles by name and description only, and
every one of the five contracts still has a generated TODO under "## Input and
output packet layouts". For FLOW that decision is particle behaviour, which is
reserved to you anyway.

**Two ways forward, and it is your call which:**

1. Keep five blocks and specify each profile's lane binding — then the tests
   are ordinary work.
2. Collapse them: one sequencer block, with the profiles becoming shell
   configuration and their ops attributed to the blocks that consume the
   output (`TERRAIN.PATCH`, `SURFACE.SHEET`, and so on).

Everything else about the sequencer is done: `FIELD.SEQ.CORE` is RTL_VERIFIED,
all 31 opcodes dispatch with a coverage gate, every Field IR piece carries a
mutation score, and the anti-hang law is formally proven.

## Three earth-field write ops need their law pinned

`FIELD.WRITE.MATERIAL`, `FIELD.WRITE.NAV` and `FIELD.WRITE.HAZARD` have no
tests, no reference functions, and no RTL — and unlike every other gap I closed
this session, these cannot be filled by working harder, because the law is not
written down anywhere.

Charter §11.2 names the layers and stops:

* layer 4 — "Base material map — two candidate material IDs plus a weight"
* layer 6 — "Gameplay state — heat, wetness, corruption, hazard and movement
  cost at a lower resolution"

`ops.yml`'s entries for NAV and HAZARD are one-liners pointing back at §11.2.
What a differential needs and nobody has stated: the encoding and width of each
layer, the resolution ("lower" — how much lower?), the blend or resolve rule
when two writes land on one cell, and the saturation behaviour.

MATERIAL is closest to reachable: layer 4 plus its ops.yml line ("2 candidate
material IDs + blend weight per cell; resolved deterministically") may be
enough to pin it. NAV and HAZARD are not.

These three will block `FIELD.SEQ.EARTH` — and therefore `TERRAIN.PATCH`
behind it — the moment either advances.

## RULED 2026-08-22 — BALANCED stays authoritative. Fix the CDC seam FIRST,
## then remeasure both fitter efforts.

**Fabian's ruling on fitter effort: defer, in a specific order.**

> "Right now the comparison is contaminated by a known structural clock-domain
> problem. A hold violation is qualitatively different from a setup miss. At
> -0.475 ns the balanced design is saying 'I can presently do about 95.5 MHz
> instead of 100 MHz'. A hold failure says 'this transfer is not physically safe
> even if you run the GPU at 20 MHz'."

The sequence, as ruled:

1. **BALANCED remains the authoritative fitter configuration.** HIGH PERFORMANCE
   is an experiment, not the shipping basis.
2. **Fix the video/GPU seam structurally** — specifically, move the displayed
   CRC into `vid_clk` rather than crossing per-pixel state.
3. Then run the SAME RTL twice, BALANCED and HIGH PERFORMANCE, and compare worst
   setup slack, TNS, failing endpoint count, worst hold slack and hold count,
   ALMs, and compile time.
4. Adopt HIGH PERFORMANCE only if it then has zero hold violations AND
   materially better setup. Otherwise stay BALANCED.

**And a direct instruction I am following:** do NOT spend time chasing the
remaining -0.475 ns of setup paths before the CDC decision. Moving that logic
changes placement enough that today's 56 endpoints may not be tomorrow's 56.

Measured at commit `6d23c84` (BALANCED): worst setup **-0.475 ns**, **56**
failing endpoints, **0** hold violations, **7,415** ALMs. The HIGH PERFORMANCE
experiment measured 17 failing setup endpoints and 2 hold violations, both on
the `vid_clk -> gpu_clk` seam.

---

## RULED 2026-08-22 — the creature-LOD boundary overflow: fix the law, never
## bake the wrap into silicon.

Found while building `zhao_geom_lod` against the shipped oracle: the random lane
caught the RTL and the reference disagreeing at `R = 59353, thresh = 40818,
e = 1, proj = 339695`. The cause was in the REFERENCE. `boundary_q8` computed
`thresh * bound_radius` in `__int128` and then narrowed the quotient to
`int32_t`; at 2,422,670,754 that wraps to **-1,872,296,542**, and a negative
boundary makes the eager-coarsen test false for every projected radius. **The
ladder refuses to coarsen and the creature stays pinned at a fine rung forever.**
Reachable with a small `micro_error` and a large threshold — both ordinary.

**Fabian's ruling, with an amendment I had missed:**

> "Fix the reference, but do not merely change `boundary_q8()` to `int64_t` and
> leave the following arithmetic unchanged. The quotient can approach 2^62, and
> multiplying that by 9 or 11 can overflow signed 64-bit."

That is correct, and it is why the fix is not a widening. **The boundary is now
never formed at all.** Both tests are cross-multiplied in `__int128` using the
same exact identity the RTL uses, so reference and hardware now evaluate ONE
mathematical predicate rather than the reference dividing and the RTL proving an
equivalent comparison by another route.

**Clamping to `INT32_MAX` was considered and rejected**, on Fabian's reasoning:
a clamp moves the 90% hysteresis threshold downward, so there are representable
radii for which `proj <= 0.9 * true_boundary` holds but `proj <= 0.9 * INT32_MAX`
does not. It fixes the catastrophic wrap while subtly changing the transition
law — and there is no reason to keep an int32 here at all.

**Regression cases added**, as directed: the exact reproducer; thresholds
astride the old 2^31 boundary; the smallest error term with the largest legal
radius and threshold; and the invariant that needs no oracle at all —

> for a fixed creature and threshold, decreasing the projected radius must NEVER
> produce a finer LOD decision.

That invariant is what the overflow actually broke, and it would have caught it
with no reference to compare against.

---

## (ruled) Timing closure: two decisions, both measured, both yours

### 1. Fitter effort -- 125 failing endpoints, or 17 with two hold violations?

The composed shell is 6.4% too fast-paced for its clock. Measured both ways:

|                    | BALANCED (current) | HIGH PERFORMANCE EFFORT |
| ------------------ | -----------------: | ----------------------: |
| worst path         |     10.64 ns       |            11.39 ns     |
| endpoints too slow |          125       |                 **17**  |
| hold violations    |        **0**       |                   **2** |
| logic cells        |        7,648       |                   8,147 |

Neither closes. High effort leaves seven times fewer things to fix, but two of
them are HOLD failures -- data arriving too EARLY, which no clock speed fixes
and which needs a real repair.

I reverted to BALANCED because every number this project has ever recorded was
taken on it, and switching makes them all incomparable. If you would rather
start closure from the 17-endpoint position, it is a one-line change.

### 2. The GPU/video crossing

Three hold violations appeared and vanished across this session's fits on the
seam where video-domain signals are sampled directly into GPU registers. They
come and go with placement, which means they are not fixed -- they are lucky.

A review you relayed proposed the real answer: the displayed-frame checksum
should live in the VIDEO clock domain, where the pixels already are, instead of
crossing every pixel into the GPU domain to be counted. `DEBUG.CRC.md` already
says the displayed lane is video-domain; `design/blocks.yml` says GPU; the
implementation followed blocks.yml and built the crossing. **The documents
disagree and the code picked one.**

That is an architecture decision, not an SDC constraint. A false path would
only stop the tool reporting a real crossing, and `set_max_delay` addresses
setup rather than the hold failures actually seen.

**RESOLVED 2026-08-22** — see the entry at the top of this file. The CRC moved
into `vid_clk`, `design/blocks.yml` was corrected to `clock_domain: video`, and
no per-pixel state crosses the boundary any more. What is still open is only
whether to cut the two clocks in the SDC, and the 64-bit `starvation_o`
sample.

Feature asks from Fabian, newest first. This file exists because the asks were
scattered across run `QUEUE-*.md` files and an agent's memory, which meant the
only complete list lived outside the repo. Everything here is an **ask**, not a
decision: an entry records what was asked, what already exists that it can lean
on, and the questions that must be answered before it becomes a spec change.

Nothing in this file is a schedule. The standing priority order is set in
`STATUS.md`; the standing content order is **terrain effects and 3D character
LOD/deformation first**, and entries below do not outrank that.

---

## 2026-08-21 — the Reality Tear (LUXURY, gated on proven hardware + measured slack)

> "If we have extra dsp and alm at the end, can we add some fancy silly thing?
> Or is the architecture too rigid"

**Not too rigid — and the effect is already specified in pieces.** Verified
against `design/blocks.yml` 2026-08-21, this is not a new feature needing a new
socket. Every part of it is already named:

| Block | What the ledger ALREADY says |
| --- | --- |
| `FORGE.PRIM` | "Ribbons, tubes, radial shells, rings, chains, shard bursts, billboard sheets, spline walls, cones" |
| `POST.GATHER` | "Accumulate low-resolution glow, **distortion-XY** and outline buffers" |
| `POST.COMPOSITE` | "Bloom, haze, **shockwave**, **refraction**, grading, palette and flash" |
| `TEXTURE.AUX` | "Restricted aux texel source (surface sheets, light/shadow compare, **distortion**)" |
| `POST.ECHO` | "Optional echo of the composited frame back to a capture buffer; **first on the §26 cut list**" — `deferred: true`, `cut_order: 1` |

Distortion is already a sanctioned aux use. Shockwave and refraction are
already composite modes. The distortion vector field is already a gather
output. The frame echo already exists as the designated first luxury to cut.
**The parts spell the effect; nobody had noticed.**

### The effect

Procedural spell geometry from the Forge writes glow/distortion strength into
the ordinary effect-tag path; the post gate turns that into a low-resolution
vector field that bends the finished image; `SURFACE.STAMP` leaves a persistent
scar underneath. Wormholes that twist the background, spells that fold the
screen inward, heat-haze creatures whose silhouettes distort the world behind
them, portals shedding fragments of previous frames, an unreality storm where
terrain scars, geometry and screen feedback all agree on one event.

> "Use the spare silicon to let enormous procedural spells physically generate
> geometry, scar the terrain and bend the completed image around themselves."

**Spend surplus geometry throughput on impossible spell geometry, not on every
unit gaining another 200 triangles.** One outrageous battlefield-scale twisting
object does more for the machine's identity than a uniform detail bump.

### What "spare" has to mean

Owner's own constraint, and it is the important half of this entry. A final fit
saying "20 DSPs free" does **not** mean 20 spendable DSPs. A luxury feature also
needs SDRAM bandwidth, M10K, routing near the right pipeline, timing margin,
frame-cycle slack and command capacity. A full-frame feedback effect might cost
almost no DSP and be impossible because it adds a framebuffer read; a
procedural geometry engine might cost eight DSPs and no bandwidth yet swamp
triangle setup.

So **spare means**: placed and routed, timing closed **on the actual board**,
worst-case workload at 60 Hz, and measured resource AND bandwidth reserve left
over.

Budget shape, if the machine lands near the estimated 92-95 DSP: keep **10-12
DSPs untouched** as engineering reserve, spend **5-8** on the luxury, and only
go to 10-16 if it lands at 80-85 with comfortable timing. Charter §25's 10%
reserve is a floor, not a target to consume.

### Easy, hard, and forbidden

**Very easy — after the renderer.** Post effects change no triangle packet, no
coverage, no depth or texture semantics, no tile layout, and not the bit-exact
geometry reference. Miss the budget and you drop resolution or taps or switch
it off; the base frame stays correct.

**Easy — before geometry setup.** A generator that emits ordinary triangles
feeds the same setup and raster path as everything else. `FORGE.PRIM` is
exactly this category.

**Medium — a new fixed material recipe**, if it uses texel, vertex colour,
depth and effect-tag data already present. Dangerous the moment it wants
another texture lookup or touches the fragment critical path.

**Hard, and mostly forbidden — the renderer's centre.** No second unrestricted
TMU, no general fragment shaders, no wider tile word, no extra arbitrary
interpolants, no second geometry pass, no shadow maps, no unrestricted
render-to-texture, no recursive portals. The ledger already defends this:
`TEXTURE.AUX` is "deliberately NOT a general second TMU (§26)".

**A fake portal from Forge geometry plus post distortion is easy. A genuinely
recursively rendered portal is nearly a new renderer.**

### What to reserve NOW, while the contracts are stubs

This is the actionable half and it costs no fabric. `FORGE.PRIM`,
`POST.GATHER` and `POST.COMPOSITE` are all still `SPECIFIED`, so the interfaces
can be shaped before anything is built:

- one optional post-effect dispatch;
- one bounded post recipe id (shockwave / heat haze / portal lens / radial
  streaks / feedback echo / chromatic smear — **one recipe per pixel, so one
  bounded DSP bank walks the selected one rather than six multiplier farms**);
- a low-resolution signed XY distortion buffer;
- a configurable quality level / tap count;
- a scheduler token budget;
- counters for processed texels, taps, dropped effects, deadline degradation;
- a deterministic fallback ladder: 4-tap filtered feedback -> 2-tap filtered
  warp -> nearest warp -> glow only -> off.

That ladder is what makes the luxury obey the machine's central rule:
**the effect negotiates; 60 Hz does not.**

### Status

**NOT SCHEDULED.** Gated on proven hardware and measured slack, per the owner.
Recorded now because the reservation is free today and expensive later.

---


## 2026-08-21 — latency is now a formal goal (RULING, not an ask)

> "Less latency is an improvement. It should be a goal, really, as long as it
> doesn't fuck up anything else. In fact I consider this a massive boon, a
> gargantuan success, and an architectural oversight that this hadn't been made
> a formal goal."

**Acted on, not queued.** `design/budgets/latency.md` is the budget document and
charter §25 now carries the rule. Latency joins ALM, DSP, M10K and bandwidth as
a budgeted resource: a change that moves it must say so and by how much, a
reduction is a win to be KEPT rather than a failing test to revert, and an
increase needs a reason better than "it was easier".

**Honest scope.** The blit path is measured — ~58k gpu cycles saved, one whole
frame earlier, roughly 16.7 ms at the 60 Hz field rate. **End-to-end
controller-sample to displayed-photon is NOT measured**, and nobody should quote
a number for it yet. Three gaps are named in the budget document rather than
guessed at, including whether `CMD.SCHEDULER`'s tick alignment costs a frame
that nobody has ever costed.

---


## 2026-08-21 — terrain rotated at arbitrary angles, and rotated in REAL TIME

> "our terrains can be rotated at different [angles]. Would be great if terrains
> could also be rotated in real time. So a skyscraper suddenly falls over."

Two asks, and they are not the same size.

### A. Arbitrary static rotation

Already resolved in principle, not yet in the spec. The resolution is **rotate
the ISLAND, not the patch**: a rotated *patch* breaks the one-solid-interval
column law that `spec/terrain_rules.md` is built on, the same wall that true
tunnels hit. An island is the right granularity because everything inside it
stays axis-aligned in island space.

`spec/terrain_rules.md` has no `orientation` field today — islands carry
translation only. **That single format change is the whole of this ask**, plus
an inverse transform on every world-space query that reaches terrain.

### B. Real-time rotation — the skyscraper that falls over

Much larger, and worth separating because the interesting failure modes are not
in the renderer.

**What it can lean on.** `GEOM.LOOM` already parents terrain patches under
transform nodes; that is the specced terrain-class-giant capability. A falling
skyscraper is a terrain-class giant whose transform is animated per tick. The
rendering path is therefore not the new part. Scars and deformation baked into
the sheet ride along for free, because they live in island space.

**The questions that decide whether this is cheap or expensive:**

1. **World-space column walking.** The one-interval law holds in island space.
   Once an island tilts, its columns are no longer vertical in world space, so
   anything that walks a world column — projectiles, particles settling,
   height-at-(x,z) queries — must inverse-transform first. Cheap per query,
   but it has to be *every* query, and a missed one is a desync rather than a
   visual glitch.
2. **The keel.** The deep textured keel is a downward curtain. When the tower
   is upright the keel points at the ground; when it has fallen the keel points
   sideways and is seen edge-on, which is exactly the "flimsy sheet" reading
   the keel was added to prevent. Does a tilting island grow a keel on the face
   that is now downward, or is the keel a full skirt from the start?
3. **The transition moment.** A structure that is part of the resident terrain
   set and then becomes dynamic has to move between two representations while
   the player is looking at it. Whether that hand-off can be made invisible is
   the real risk in this feature, and it is a determinism question before it is
   a visual one — both representations must agree exactly on the frame they
   swap.
4. **Rotation about what.** A skyscraper falling over is rotation about an edge
   at its base, not its centre. The pivot has to be authored, or derived from
   the contact edge; either way it is data the format does not carry yet.

**Not proposed here:** any answer to those four. They need Fabian's call on how
much of it is physics and how much is an authored performance.

**Placement.** This is a terrain effect, so it sits inside the standing top
priority — but behind the current hardware lane (`DEBUG.FRAMEBLIT` integration
and the composed Quartus fit), because it is a *format and sim* change and the
lane that would carry it is the same one those are holding.

---

## Standing asks, consolidated

Carried forward from `runs/CLAUDE-RUNS/RUN-20260816-0046-.../QUEUE-*.md` and
prior sessions, so the list is readable from the repo.

**Visual identity — non-negotiable**

- Noctis IV suns and lens flares; a whole gamut of suns, including from space.
- Moving suns must carry the Noctis **smear** (fade-not-clear ghost trails).
  Hard travelling discs are "not fully Noctis style".
- 360-degree skyboxes; god beams piercing cloud.
- Floating terrain islands, **more** deformable than Sacrifice: breaches,
  undercuts, rim bites.
- The rubbery liquid terrain feel: travelling waves, rebound dips, ground
  behaving like a membrane.

**Queued**

- Effect library: every sun variant and every terrain effect catalogued with
  screens and reels, renderable by id.
- Clouds in front of sky and sun.
- Atmospheric rain that darkens the sky as cloud rolls in; spells may cause
  weather. Weather is sim state and must replay exactly.
- Cheap, impressive global and local light changes. Explicitly **no ray
  tracing** — "we cheat like the cheap fucks we are."
- Rotated terrain sheets for vertical structures: skyscrapers and mountains
  built from several smaller rotated sheets, deformable. Four walls and a top.
- Deep textured keel on island rims so terrain reads solid rather than as a
  flimsy sheet. **Sequenced before rotated sheets.**
- Thick-atmosphere sun: the free win is the star ramp's P3 control point, which
  currently whitens the top and is backwards for thick air. Two real gaps
  remain — the falloff in §4 is linear and wants a soft shoulder, and the sky
  has an elevation ramp with no azimuthal term centred on the sun.
- Game modes: campaign, skirmish, 2P versus, 2P co-op. Split screen is decided
  and shipped.
- Digging and tunnels: a column is one solid interval, so true tunnels need
  Wounds. Trenches and keel burrows are free today; round overhangs already
  work via high-bottom slab columns.

**Process**

- Reel subjects must be legible at gallery scale, not merely correct.
- Site copy gate is hard: no em dashes, no AI-isms, including reel provenance
  strings.
