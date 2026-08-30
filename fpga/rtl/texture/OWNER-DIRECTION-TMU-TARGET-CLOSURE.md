# TEXTURE.TMU — target-closure direction

**Status: QUEUED, not started. Do not begin until the Field/Earth lane closes.**

Received 2026-08-30 from the reviewer, relayed by Fabian, with the instruction to
write it down because "it might be a while until we get to it". It lives here,
beside the TMU it governs, and NOT in a run folder — every pass creates a new run
folder and a file left in the current one is orphaned by the next (CLAUDE.md,
"Instructions are not delivered until they are read").

**Read this whole file before touching `zhao_texture_tmu.sv`.**

---

## The number

    850,000 texture samples per frame
    100 MHz / 60 Hz = 1,666,667 clocks per frame
    => 1.961 clocks per sample, averaged over everything

The unrounded demand behind it is 829,440 samples: 92,160 Z60 pixels x 3 texture
layers x 3x overdraw.

**An II=2 machine delivers 833,333 samples/frame.** That is 0.47% above the
unrounded demand and BELOW the rounded 850,000 contract. So:

* CLUT at II 6 (today) is dead.
* CLUT at II 2 is not a stopping point.
* **CLUT must be II 1 on hits. Direct nearest must be II 1.**
* Direct bilinear may be II 2 *only* if the contract is explicitly mode-weighted
  — an all-bilinear stream at II 2 reaches only 833,333.

Terrain is CLUT8 and is the dominant stress mode, so two filter lanes are
probably enough for the real content mix. They do **not** constitute a
mode-independent 850k sampler, and the docket currently blurs that.

## The diagnosis

The TMU is not bandwidth-limited. It is a competent cache and filter wrapped in a
one-request-at-a-time FSM: accept, calculate every address, wait for texel, maybe
wait for palette, decode, filter, expose, wait for consumer, only then accept the
next request. That is CLUT II 6 and direct II 5 at the shipping two-filter-lane
setting, and it is why the block closes at 36.11 MHz — the output sits
combinationally behind format decode, channel selection and the whole factored
bilinear expression.

The texture cache already has four physically independent lanes and returns four
halfwords per accepted access. On hits it can accept an access every cycle.
**The cache port is not the hit-path bottleneck. The output port can retire one
complete sample per clock, nearly 2x the target.**

    sample type                              cache work   filter work   ceiling
    CLUT nearest + resident palette RAM      1 lane        none          1/clock
    direct nearest with filter bypass        1 lane        none          1/clock
    RGB565 bilinear                          4 lanes       3 channels    2 per 3 clocks
    ARGB bilinear                            4 lanes       4 channels    1 per 2 clocks
    any bilinear, four logical filter lanes  4 lanes       4 channels    1/clock

## Amendment 1 — palettes get their own RAM. Do NOT chase them through the texture cache.

This is the single largest change and the reason CLUT costs six clocks today.

The repo's existing docket proposes a lane conveyor: pack a newer texture-index
access into lane 0 while an older palette access uses lane 1. It can work, and it
buys two logical records per cache transaction, per-lane source IDs, per-lane
response attribution, texture/palette conflict pollution, palette misses blocking
texture accesses, direct bilinear stealing the palette lane, and an all-or-nothing
access where one lane's miss stops both records. That is a great deal of
bookkeeping to avoid a few M10Ks.

**Give the TMU a resident palette RAM.**

    texture cache response: CLUT index
        -> resident palette RAM, 1-cycle synchronous read
        -> RGB565 decode

The request carries a resident **palette slot**, not a VRAM byte address.

    256 entries x 16 bits = 4,096 bits = 512 bytes per page
    8 pages  = 32 Kbit
    16 pages = 64 Kbit

First configuration: `PAL_SLOTS=8`, `PAL_ENTRIES=256`, `PAL_WORD=RGB565`. Bind or
upload before frame execution under the resource epoch — near-star palette
replacement already has an upload/invalidate discipline, so residency fits the
machine's existing resource model. The charter expects palette/material caches
and permits vendor RAM wrappers. This is storage inside the primary sampler, not
a second TMU.

Why not the conveyor: a cache lane is 16 lines x 16 bytes = **256 bytes**, so a
512-byte palette cannot even reside in one lane; direct mapping guarantees
aliasing; and the cache has one fill engine and no hit-under-miss, so any miss
blocks acceptance of the whole access. Palettes are tiny, predictable and
explicitly identified — exactly the wrong thing to demand-page.

Resulting pipeline, accepting one per clock:

    T0 accept CLUT request
    T1 texture-cache index access accepted
    T2 index arrives; palette RAM address registered
    T3 palette colour arrives
    T4 result lands in reorder record

Keep the lane conveyor as a measured fallback if the composed Quartus fit makes
palette residency unexpectedly expensive — as a fallback, not as the default by
assumption.

## Amendment 2 — nearest must BYPASS the bilinear filter

Nearest sets fu=fv=0 and runs the full bilinear arithmetic because the result is
mathematically an identity. Elegant, and terrible for throughput: with two filter
lanes even a direct nearest sample consumes filter passes. The charter itself
calls nearest a fast path, and a bypass inside the one TMU is not a second
sampler.

    direct nearest:   cache response -> registered decode16 -> result record
    direct bilinear:  cache response -> footprint queue -> channel scheduler -> record

Both share address planning, cache, format laws, ordering, output and mode
checking. The bypass skips only arithmetic whose proven output is its input.

## Amendment 3 — the filter is a work-conserving CHANNEL pipeline, not fixed passes

Today every direct texel is four channels in two fixed passes (R,G then B,A).
**RGB565 alpha is always exactly 255** — filtering four identical 255 taps
produces 255, and there is no reason to spend a filter job proving it per sample.

Schedule tagged channel jobs instead:

    RGB565 bilinear:  R, G, B      = 3 jobs -> 1.5 filter clocks/sample
    ARGB1555/4444:    R, G, B, A   = 4 jobs -> 2.0 filter clocks/sample

An all-RGB565 bilinear stream then reaches 1,666,667 / 1.5 = 1,111,111
samples/frame, clearing 850k with margin on the six-DSP design.

Keep `zhao_texture_bilerp` as the readable arithmetic law and build a registered
equivalent of its exact factored expression:

    A = (t00 << 8) + (t10 - t00) * fu
    B = (t01 << 8) + (t11 - t01) * fu
    S = (A   << 8) + (B   - A  ) * fv
    out = (S + 32768) >> 16

    F0  register taps, fractions, record ID, channel ID
    F1  du0/du1 products, exact A and B
    F2  dv product, exact S
    F3  round once, register byte and {record, channel}

Two lanes: four 9x9 products in F1, two 18x9 in F2, six DSP structures, one pair
of channel results accepted per clock. **No intermediate rounding, no width
changes.** The proof obligation is

    pipe_out[N+3] == zhao_texture_bilerp(inputs[N])

under an uninterrupted valid stream, plus separate stall and tag properties.

The present 21.432 ns path is one decode-to-output continent; DSPs already fell
28 -> 3 with Fmax barely moving, which says multiplier count was never the timing
problem. **Pipeline boundaries are.**

## Bounded probe — force Cyclone V packed DSP modes explicitly

A variable-precision DSP block is roughly three 9x9 multipliers, or two 18x19.
Quartus refused to infer that packing from separate `*` operators, so twelve small
products became twelve DSP blocks. The charter permits a vendor primitive behind
a wrapper.

    F1: 8 independent 9x9 products, four channels   -> theoretically 3 DSP blocks
    F2: 4 independent 18x9 products                 -> theoretically 2 DSP blocks

If Quartus 17.0.2 honours it: direct bilinear becomes II 1, the strict 850k target
works even for an all-ARGB-bilinear stream, and DSPs may fall 6 -> 5.

    PASS  exact arithmetic, >=110 MHz isolated, <=6 DSPs, no ALM explosion
    FAIL  archive the evidence, retain the 2-lane / 6-DSP pipeline

**Do not restore four normally inferred filter lanes.** That is already measured
at twelve DSPs.

## Address generation becomes a real pipeline

Today the request pins feed mode decode, mip selection, several variable shifts,
half-texel bias, wrap folds, four repeated tap calculations, row-major indexing,
level offset and final byte addresses — all before the access bundle is
registered. The loop computes four wrapped U, four wrapped V and four row shifts
where only two of each are unique. **Remove that duplication before placing
registers.**

    A0  accept, allocate a record, latch raw request + internal sequence + source ID
    A1  mode sanitise: format, effective filter, mode_error, level clamp,
        log2 dimensions at the selected level
    A2  scale u/v, half-texel bias, floor, fu/fv, compute u0/u1 and v0/v1 ONCE
    A3  wrap u0/u1 and v0/v1 once each, row0/row1 once each, form four addresses,
        register the access bundle

The cache issue bundle must be a registered object that stays **bit-stable until
accepted** — the present cache's first-look accounting assumes the offered access
does not mutate before ready.

Later, not first: a texture-binding table. Base, palette, format, wrap, dimensions
and mip shape are constant for thousands of samples, so the per-sample request
could become `{binding_id, u, v, lod, source_id}` with per-level byte base,
shifts, masks, palette slot and sanitised format precomputed. Attractive second
pass if address ALMs or timing stay unpleasant. Do not block the first pipeline on
it.

## The cache is the real danger once the hit path is fixed

Today: four independent lanes, only 16 lines x 16 bytes per lane, **one fill
engine, no MSHR, no hit-under-miss, all-or-nothing access acceptance.** A default
line fill returns eight halfword beats, so one miss costs roughly nine or more
blocked clocks.

    1,666,667 - 850,000 = 816,667 spare clocks

At ~9 clocks per line miss with no overlap, only about a **10% line-miss rate**
fits — and direct-bilinear work, material changes and other bubbles eat into that.
**Tune with real traces, not a synthetic all-hit test.**

Cheap capacity sweep first (Quartus gets the vote on M10Ks):

    LINES  LINE_BYTES  data/lane   likely data M10K/lane
    16     16          256 B       1
    32     16          512 B       probably 1
    64     16          1 KiB       probably 1
    32     32          1 KiB       probably 1
    16     64          1 KiB       probably 1

Measure completed samples/frame, fill cycles, unique line fills, lane misses,
material/palette changes, ALMs, M10Ks and Fmax. **Do not optimise nominal hit rate
alone** — a larger line can improve hit rate and still lose because each miss
takes longer.

**Same-line fill multicast.** A bilinear footprint commonly has t00/t10 in one
line and t01/t11 in another. Today two lanes missing the same physical line are
filled separately. The data is read-only, so one VRAM line response can be written
into every missing lane requesting that `{tag,index}`. Replace `fill_lane` with
`fill_lane_mask`: on fill start gather all missing lanes whose requested line
matches, broadcast every returned beat into those M10Ks, publish all matching tags
together. A cold bilinear footprint goes from four fills toward two with no change
to visible semantics.

Only if traces still fail, escalate in this order: bigger/more palette slots, a
small victim cache, hit-under-miss for independent lanes, one or two MSHRs,
texture asset banking/swizzling by 2x2 parity. **Do not jump to a complex
non-blocking cache** — capacity, palette separation and multicast are far cheaper.

## Raster integration must not serialise the TMU from outside

`zhao_raster_fragment` accepts an already-sampled texel: RGB, alpha and index
fields, no UV request path, no outstanding texture context. Wire it as "take
fragment -> request texture -> wait -> feed core -> take next" and the II 1 TMU is
externally reduced to one fragment at a time.

Build **RASTER.TEXJOIN** in front of it rather than contaminating the verified
fragment arithmetic:

    EARLYZ survivor
         -> fragment-context FIFO ----+
         -> TMU request               |
    TMU result + sequence ID ---------+
         -> existing zhao_raster_fragment

The context record holds tile address, depth, fragment state, vertex colour/alpha,
tag/stencil, source ID, texture binding/UV/LOD and bounded material sample state.
Depth 16 is the sensible first probe (pipelined hit latency ~8-12 clocks);
**sweep 8/16/32 rather than declaring 16 holy.**

**Keep EARLY-Z before the TMU** or the console pays texture bandwidth for
fragments it already knows are hidden.

**Multiple samples per material.** The 850k target assumes layered demand while
the fragment core consumes one final texel. Do not fake the benchmark by sampling
repeatedly and discarding two results. Either v1 material/Mosaic genuinely
collapses to one primary sample and 850k stays a conservative stress contract, or
the front end supports a bounded 1-3-sample recipe and accumulates the combined
texel before the fragment core. **That decision belongs in the composed workload,
not in the standalone TMU.**

## Record architecture

Internal reorder ring. **Never use `src_id` as the unique identity.**

    record: valid, generation/internal sequence, external src_id,
            kind (CLUT / direct-nearest / direct-bilinear), format, mode_error,
            palette slot, CLUT index, RGBA result, channel completion mask, done

    job:    {record_id, generation, operation, channel}

The oldest completed record drives the registered output. While `smp_ready_i` is
low the output is stable and younger work continues until the ring fills.

A source ID can repeat. **Tests must deliberately use identical source IDs on
adjacent requests**, because otherwise source-ID-based reordering bugs look
correct. For RGB565 bilinear the completion mask expects only RGB and installs
A=255 at allocation; ARGB waits for all four channels.

## Expected result

    path                                    hit II   capacity at 100 MHz / 60
    CLUT nearest                            1        1.667 M/frame
    direct nearest                          1        1.667 M/frame
    RGB565 bilinear, 2 filter lanes         1.5 avg  1.111 M/frame
    ARGB bilinear, 2 filter lanes           2        833 k/frame
    any bilinear, packed-DSP probe passes   1        1.667 M/frame

Reviewer's confidence: ~90% on the hit path (the hardware width is plainly
sufficient); ~70-80% on a realistic trace including misses and full raster rejoin.
The uncertainty is almost entirely cache behaviour and integration, not texture
arithmetic.

---

# THE RUN, as queued

## Baseline

Start from main after the Field/Earth lane closes. **Do not modify Field/Earth.**
Freeze the existing TMU/cache/reference tests and record: TMU `FILT_LANES=2` ALMs,
regs, DSPs and corrected-I/O Fmax; CLUT and direct hit II; cache
`LINES`/`LINE_BYTES` and fit; current exact directed/random/formal status.

## Architecture ruling

Keep **one** primary TMU. Prefer resident palette RAM over routing palette lookups
through TEXTURE.CACHE. Prototype `PAL_SLOTS=8`, 256 RGB565 entries per slot,
synchronous one read per clock, bound before frame execution under the resource
epoch. Retain the lane conveyor as a **measured** fallback only.

## Pipeline stages

    A0  request acceptance + record allocation
    A1  mode sanitise / mip clamp / selected-level dimensions
    A2  scaled coordinates, bias, floor, fu/fv, unique u0/u1/v0/v1
    A3  wraps, unique row0/row1, final cache bundle
    C   registered cache issue, held stable until accepted
    D   registered decode / CLUT-index routing / nearest completion
    F   two-lane tagged bilinear pipeline
    R   in-order result ring and registered output

A request may enter every clock while record/queue credits exist. **Latency may
rise; sustained rate is the contract.**

## Correctness gates, against an untouched `zref::Tmu`

Every format, wrap, mip and error mode. Mixed CLUT / nearest / bilinear streams.
Distinct **and repeated** source IDs. Queue and sequence wrap. Output stall while
full. Cache miss on texture, palette fallback, and both at once. Channel-result
swap. Record-result swap. Wrong palette slot. Dropped and duplicated valid. Early
and out-of-order retirement. RGB565 alpha remains exactly 255. Same-line multicast
writes all and only matching lanes. Output stable under backpressure; no result
duplicated, dropped or attached to another fragment.

## Performance gates

Hit-only: CLUT nearest II 1; direct nearest II 1; RGB565 bilinear <= 1.5
clocks/sample average on two filter lanes; ARGB bilinear II 2 on two lanes; any
bilinear II 1 **if and only if** the packed-DSP probe succeeds.

Production: an 850,000-sample representative Z60 trace inside 1,666,667 clocks,
with useful margin — **target <= 1,450,000 clocks, not a one-clock pass**. Run
terrain-heavy, mixed sky/beam/star, Duo-interleaved and adversarial-thrash traces.
All outputs exact.

## Synthesis gates

Under corrected clock and I/O constraints: standalone TMU >= 110 MHz preferred and
>= 100 required; TMU + real cache + palette RAM >= 100 MHz; TMU/cache/TEXJOIN/
fragment composition >= 100 MHz; hold violations 0; filter DSPs <= 6; save the top
200 setup paths by family; record ALM/reg/DSP/M10K for every accepted frontier
point.

## Instrumentation

**Every non-progress clock must land in exactly one bucket**, and every counter
must be differenced over the measured window — the Field lane shipped a
123%-occupancy report by dividing a from-reset counter by a windowed clock count,
and a `--points 256` gate that measured the ramp and reported a machine 1.23x over
budget when it was inside with margin.

Count: accepted and retired samples by CLUT/direct and nearest/bilinear; planner
full; record ring full; cache issue wait; cache response wait; cache hits/misses by
lane; unique line fills; multicast fills saved; palette waits; filter jobs; empty
filter slots; output backpressure; texture join full; completed samples/frame.

## Explicitly forbidden

* a second unrestricted TMU
* restoring the 28-DSP filter
* calling II 2 sufficient for the rounded 850k target
* relying on source IDs for ordering
* benchmarking only cache hits
* integrating one fragment at a time, serialising the TMU externally
* false-path or multicycle exemptions on functional logic
* changing rounding or palette semantics to gain speed

---

## What the Field lane learned that applies directly here

Written by the agent that did the Field/Earth optimisation on 2026-08-30, because
every one of these cost hours there and the TMU run will meet all of them.

1. **A short benchmark measures the ramp.** The Earth gate ran 256 points on a
   32-context machine and reported it 1.23x OVER budget; the same binary at 1024
   points reported it INSIDE with 22% margin. Sustained rate is a steady-state
   quantity. Pick the trace length so the pipeline is full for most of it.
2. **Do not derive a frame figure from a rounded per-sample figure.** Rounding
   clocks-per-group to an integer and multiplying by the group count turned 1%
   changes into 5% headline swings and produced two false "regressions".
3. **Latency reductions do not buy throughput; acceptance-rate increases do.** Four
   latency cuts in the Field engine each made the worst program slower or did
   nothing. The one change that raised the ACCEPTANCE rate — caching a descriptor
   so the front end took two clocks instead of eight — was worth 31%. Bro's whole
   TMU direction is an acceptance-rate argument, which is why it is likely right.
4. **A parameter optimum does not survive an architectural change, and neither does
   a rejection.** `LONGQ=4` was the best sweep point before the descriptor cache
   and among the worst after. DIST2 front-end pipelining cost 1.3% before and
   bought 2.7% after. **Re-sweep and re-test after every structural change**, and
   put a date on every "measured and rejected" row.
5. **Measure which unit refuses, not which unit looks busy.** Per-service
   accept/refuse counters named CURVE as the Field wall in one run, after six sweep
   rounds had failed to find it. Build the per-lane, per-claimant counters bro asks
   for BEFORE building any of the datapath.
6. **A parameterised block whose loops are not parameterised is dishonest.** Two
   `for (int u = 0; u < 2; u++)` loops survived `UNITS` becoming a parameter, so
   six of eight ring units were never reset and never marked offered — X in
   silicon, and only Verilator's two-state zero-fill made it look right.
7. **Verify a new test by mutation.** The descriptor cache's invalidation test was
   confirmed real by deleting the invalidation line and watching exactly those two
   checks go red.
