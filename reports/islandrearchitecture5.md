ZHAOZHOU TEXTURE-SURVIVOR ISLAND REARCHITECTURE
AND ALM / DSP / REGISTER RECOVERY SPECIFICATION
================================================

Date: 2026-09-03
Repository: Fabulu/zhaozhou
Hardware working branch examined: zixxtrixx-v8-closeout
Hardware branch head examined: 1c0a7f44ebb82cb3fd5dd74bd182a5385dff3f1e
Latest default-branch head observed while writing: 49672fbabbf9bd65adfd2f0ae18bb8fb913089a7
Provisional fit device: 5CSEBA6U23I7
Provisional fit tool: Quartus Prime Lite 17.0.2

Status: IMPLEMENTATION ARCHITECTURE / PROPOSED OWNER RULING

This document is intended to be copied into reports/ beside the earlier durable
architecture rulings. It becomes binding when the owner adopts or commits it.
It does not change the reference arithmetic, material capability, texture
formats, visible pixels, throughput promise, or the 100 MHz product clock.

It supersedes the production recommendations for the first-generation texture
leaf prototypes wherever they conflict. Those prototypes remain valuable as
oracles, regression targets and demonstrations of the right semantic ideas.
They are not the production physical organization.


0. THE DECISION IN ONE PAGE
===========================

The first complete texture-leaf fit answered the question it was built to
answer:

    Does the 99.50 MHz reduced renderer survive the texture path as written?

    NO.

All ten texture leaves were physically fitted. Eight of ten were below the
99.50 MHz reduced renderer. The slowest was 54.95 MHz. Their isolated resource
sum was 15,749 ALMs, 25,123 registers, 11 M10Ks and 16 DSPs. The important
resource symptom is not merely the ALM total. It is 25,123 registers against
only 11 M10Ks: state that belongs in memories and narrow token queues was
implemented as wide flip-flop arrays and replicated payload FIFOs.

The failure is not evidence that the console, the material model, or the target
clock is impossible. It is evidence that the first texture implementation was
built as ten independently convenient leaves rather than one physically
coherent subsystem.

The production ruling is therefore:

  1. KEEP the 96.87 MHz mean / 99.50 MHz best reduced renderer provisionally.
     Do not restart local raster whack-a-mole because the texture prototypes
     were slow.

  2. DO NOT integrate the current leaf collection into the shell.

  3. Build one production TEXTURE.SURVIVOR island around a single transaction
     token allocated immediately after Early-Z and before reciprocal,
     perspective, TMU or AUX work.

  4. Store wide, persistent transaction payloads in M10K-backed record banks.
     Queues carry token IDs and tiny work descriptors, not copied fragments,
     footprints or texture packets.

  5. Eliminate every table-wide combinational scan. A scheduler is told about
     work once through a queue; it does not rediscover work by searching every
     live entry every clock.

  6. Replace the current cache completely with the synchronous C0-C4 cache
     described here. Four static data banks and four static tag banks must
     infer as M10K. A cache made from 10,812 registers is rejected even if its
     pixels are correct.

  7. Replace the current PERSPUV table/scheduler with a fixed streaming pair
     pipeline: U and V products launch together, one complete UV pair per
     clock, followed by separately registered rescale and saturation stages.

  8. Keep one reciprocal engine, but narrow its multiplier operands from the
     accidental 32x64 expression to the proved 32x32 domain and replace free,
     ready and done scans with queues.

  9. Move texture metadata out of the per-sample hot path. A resident binding
     table and precomputed mip descriptors replace repeated mode sanitation,
     REP4 arithmetic and wide dynamic address setup.

 10. Replace the response dispatcher’s copied wide FIFOs with token queues and
     class-owned RAM-backed payload stores.

 11. Make palette loading transactional and atomic with inactive-bank loading,
     CRC validation and one-clock bank/generation commit.

 12. Spend throughput where the measured workload needs it:
       - perspective pairs: II = 1, because every primary sample needs one;
       - cache hit path: II = 1;
       - TMU planning: II = 1;
       - bilinear channel arithmetic: initially II = 3 with one multiplier,
         because the known workload is roughly 180,000 channel jobs/frame;
       - material combiner: two byte-product lanes plus bypass paths, not six
         always-live multipliers.

 13. Replace Mosaic’s two dynamic 32-bit constant multipliers with explicit,
     exact modulo-2^32 shift/add pipelines. The frozen hash remains bit exact;
     its four DSPs do not.

 14. Trade registers and ALMs for M10Ks deliberately. The production island
     target is approximately:

         5,500-6,600 ALMs       target
         7,500 ALMs             hard pre-composition redline
         6,000-8,000 registers  target
         9,000 registers        hard redline
         32-56 M10Ks            expected and acceptable
         10-13 DSPs             target
         14 DSPs                hard redline

     Against the measured prototypes, the target saves roughly 9,000 ALMs and
     17,000-19,000 registers while also increasing UV throughput. M10K use is
     expected to rise because that is where caches, records and queues belong.

 15. Close the island in this order:

       physical fit harness
       -> repaired-leaf before/after measurements
       -> cache v2
       -> FRAGROB / token fabric
       -> perspective pair pipe
       -> reciprocal scheduler v2
       -> binding table + planner v2
       -> class decode + palette + bilerp
       -> Mosaic shift/add
       -> material combiner
       -> AUX v2
       -> composed texture-survivor island
       -> reduced renderer + texture-survivor composition

 16. Frequency gates remain:

       105 MHz  full-composition acceptance floor
       110 MHz  architecture objective
       115 MHz  stretch/headroom
       100 MHz  product clock

     The production texture-survivor island should close at 115-120 MHz before
     it is allowed to spend margin in full composition.

 17. Freeze further speculative terrain or Field RTL growth until this island
     has a credible physical result. Architecture, contracts and tests may
     progress in parallel; another mountain of unfitted hardware may not.


1. EVIDENCE THAT GOVERNS THIS SPECIFICATION
===========================================

1.1 The complete first-pass leaf fit
------------------------------------

One seed each, virtual I/O, provisional device. These are reconnaissance
numbers, not board proof and not a composed-console clock. They are nevertheless
large enough to expose structural failures.

  block                               Fmax       ALM     regs   M10K  DSP
  ----------------------------------------------------------------------
  zhao_texture_aux_pipe              54.95      1,118    1,244    1    0
  zhao_raster_texjoin_v2             61.66      3,465    6,143    3    0
  zhao_raster_perspuv_svc            62.67      1,792    2,827    2    3
  zhao_raster_rcp24_svc              68.46      1,041    1,101    0    6
  zhao_texture_cache_pipe            81.06      5,634   10,812    3    0
  zhao_texture_mosaic                86.63        197      192    0    4
  zhao_texture_tmu_plan              93.55      1,419    1,054    0    0
  zhao_texture_bilerp_lane           99.69        125      177    0    3
  zhao_texture_palette_res          104.42        152      141    2    0
  zhao_texture_rsp_dispatch         110.90        806    1,432    0    0
  ----------------------------------------------------------------------
  isolated unique-leaf warning sum             15,749   25,123   11   16

The provisional device reports 41,910 ALMs, 553 M10Ks and 112 DSP blocks.
15,749 ALMs is 37.6% of the device. The sum is not a final composed resource
count: virtual I/O disappears, shared logic may merge, and wrappers differ.
It is still a valid warning. The cache alone consuming 5,634 ALMs and 10,812
registers while using only three M10Ks is not composition noise.

1.2 Measured path diagnoses
---------------------------

AUX, 54.95 MHz:

    req_env_x1_i[20]
      -> clamp / normalize cone
      -> zhao_texture_aux_div6:u_div|ru_q[0][11]

The path starts at an input port. The block had internal stages but no registered
boundary before the first wide arithmetic. The newly added A0/A0b split is the
correct immediate repair, but the production AUX also needs bounded credits and
real request/return queues.

TEXJOIN v2, 61.66 MHz:

    free_cnt_q[3]
      -> 16 entries x 3 samples priority selection
      -> selected slot/sample output

The report shows thirteen logic levels and approximately 15.7 ns of data delay,
mostly routing. This proves that “scan the table for work” is not a scheduler.
The recently landed work FIFO removes this exact path and is a good interim
repair. The final architecture goes further: reciprocal completion already
bounds sample expansion to at most three jobs in four clocks, so the production
island does not need a 64-entry sample-work FIFO at all.

PERSPUV, 62.67 MHz:

    registered 64-bit product
      -> variable rounding bias
      -> variable arithmetic rescale
      -> saturation compare
      -> token-table entry write

One approximately 15 ns cone performs three different pipeline stages. The
current one-product-per-clock scheduler also supplies only one UV pair per two
clocks. It is replaced, not patched.

CACHE, 81.06 MHz:

    response/request pointer
      -> combinational tag/valid array selection
      -> hit/miss and storage update

The resource report confirms the code review: cache state became flip-flops.
The cache must be rebuilt around synchronous memories and a fabric capture
stage. No amount of queue tuning fixes this organization.

RCP24, 68.46 MHz:

    multiplier/context state
      -> result selection
      -> virtual output register/pin

The data path was about 8.5 ns but the report assigned almost 6 ns of skew to
the leaf boundary. The same-seed rerun repeated the same placement and therefore
proved nothing. A seed sweep and a registered physical wrapper are required
before deciding how much of that number belongs to RTL. Independently of the
clock artifact, the current source still exposes three scans and a 32x64
multiply expression whose true value domain is only 32x32.

TMU planner, 93.55 MHz:

    scaled integer V
      -> 32-bit wrap fold
      -> 32-bit row shift

The recent narrowing to CW=12 and TW=24, plus moving the +1 tap one stage earlier,
is directionally right and remains. Its refit is a before/after measurement,
not the final planner architecture, because static binding work should move out
of the sample path entirely.

Mosaic, 86.63 MHz:

Two exact 32-bit constant products are inferred as four DSP blocks. The law is
frozen; the multiplier implementation is not. Explicit signed-digit shift/add
pipelines preserve modulo-2^32 arithmetic while returning the DSPs.

1.3 What the numbers do not prove
---------------------------------

They do not prove that the completed console runs at 54.95 MHz.

They do not prove that the repaired AUX or narrowed planner retain their old
numbers; those are before measurements until refitted.

They do not prove that virtual-pin placement is harmless. TEXJOIN has hundreds
of virtual pins; RCP’s output skew demonstrates why leaf boundaries must be
registered inside a physical harness.

They do prove that several internal data paths are far too long and that the
storage topology is wrong. Virtual pins do not turn a 15.7 ns table scan or a
10,812-register cache into acceptable production hardware.

1.4 What the recent work got right
----------------------------------

Keep these lessons and tests:

  - exact shipped RTL or zref arithmetic as oracle;
  - local-ready discipline;
  - same-cycle accept/retire counter mutation tests;
  - eight-bit generations;
  - output packets held under stall;
  - zero-sample handling;
  - same-line fill multicast;
  - one blocking cache miss until traces justify MSHRs;
  - one bilinear channel engine rather than four speculative engines;
  - explicit head-of-line counters;
  - fit source-closure manifests and preflight;
  - source contamination detection;
  - seed control;
  - honest distinction between timeout, apparatus failure and timing failure.

1.5 What the recent work got wrong
----------------------------------

The common mistake was not bad arithmetic. It was treating independently clean
leaf source files as a subsystem architecture.

  - Wide payloads were copied into several FIFOs.
  - Tiny tables were searched combinationally every clock.
  - Cache arrays were declared as arrays but not organized as synchronous RAM.
  - Fixed-latency arithmetic was wrapped in token tables and selection scans
    when a streaming pipe was sufficient.
  - Static texture metadata was recomputed per sample.
  - Block interfaces were hundreds of virtual pins wide.
  - Input and output seams were sometimes unregistered.
  - A same-seed rerun was mistaken for a second placement experiment.
  - New RTL accumulated faster than physical evidence.

The production island must make those mistakes structurally difficult to write.


2. NON-NEGOTIABLE LAWS
======================

2.1 Functional law
------------------

Preserve all existing texture semantics:

  CLUT8
  CLUT4
  RGB565
  ARGB1555
  ARGB4444
  nearest
  direct-colour bilinear
  mip selection
  repeat / clamp / mirror
  raw sample-0 palette index
  exact fixed-point rounding
  exact Mosaic hash and mirrored fold
  exact RCP24 mantissa/exponent
  exact AUX six-bit quotient
  exact material recipes R9

No approximation, “visually equivalent” rewrite or new donor interpretation is
a timing optimization.

2.2 Throughput law
------------------

Latency may grow. Initiation capacity and exact output may not silently regress.
Every deliberate initiation-rate reduction must be justified against a named
workload and made visible as a versioned contract.

Required production rates:

  fragment allocation after Early-Z        one survivor / clock
  TMU sample planning                       one sample / clock
  perspective pair input                    one sample / clock
  cache hit path                            one sample / clock
  nearest decode                            one sample / clock
  CLUT index + palette lookup               one sample / clock
  AUX acceptance                            one request / clock when credited
  material retirement                       one fragment / clock on bypass path

Bilinear channel arithmetic is intentionally workload-sized rather than diagram-
sized. Initial contract: one channel job every three clocks from one exact
18x9 multiplier structure. The known approximately 180,000 jobs/frame consume
540,000 of the 1,333,333 design clocks, leaving more than 2x capacity. A second
lane is admitted only by a trace that exceeds the one-lane gate.

2.3 Transaction law
-------------------

The transaction identity is minted immediately after Early-Z.

Never use external source_id as transaction identity.

No slot is reused until its final output is accepted or the whole island is
explicitly aborted and generation-invalidated.

Every variable-latency return carries internal identity and writes a direct-
indexed destination.

Completion order is not retirement order.

2.4 Storage law
---------------

Persistent wide state lives in M10K-backed records.

Queues carry tokens and small work descriptors. A queue may carry a wide payload
only when a measured port or RAM-shape argument shows that token indirection is
more expensive.

A memory is not accepted because the source calls it RAM. The fitter must show
the expected M10K count and the timing report must show a fabric capture stage
between M10K output and broad logic.

2.5 Scheduling law
------------------

No live-entry scan larger than four candidates is permitted in a hot path.

A free-slot scan, work scan, done scan or oldest scan across 8/16/48 entries is
a rejected scheduler. Use free FIFOs, ready FIFOs, continuation FIFOs and direct
queues.

Three-way fixed arbitration is acceptable. Forty-eight-way priority discovery
is not.

2.6 Backpressure law
--------------------

No consumer-ready signal may run combinationally through more than one subsystem
boundary.

Fixed-latency pipelines reserve output credit before accepting input and then
run without internal stalls.

Variable-latency units allocate destination storage before issuing work.

Any counter affected by two events in one clock receives one net assignment.

2.7 Error law
-------------

Unknown recipe, sample-count mismatch, stale token, duplicate return, invalid
sample index, palette CRC failure, binding-generation mismatch, queue overflow
or impossible memory state raises a sticky frame fault.

The frame drains and the prior complete frame repeats. No plausible placeholder
texel is published.

2.8 Physical law
----------------

All hot-block ingress and egress boundaries are registered in the physical
composition.

No M10K output directly feeds a wide compare, priority tree, barrel shift,
multiplier, cross-module ready chain or output pin.

No datapath stage contains more than one wide multiplier layer.


3. FREQUENCY AND RESOURCE GATES
==============================

3.1 Frequency hierarchy
-----------------------

  product clock                           100 MHz
  full-composition acceptance floor       105 MHz
  full-composition architecture objective 110 MHz
  stretch / headroom                      115 MHz

  texture-survivor composition target     115-120 MHz
  major texture islands                   120-125 MHz, three seeds
  individual arithmetic/storage leaves    125 MHz minimum after stable design
                                          130-150 MHz preferred

The former blanket “every leaf must be 150 MHz” is retained as a design
aspiration, not used as a reason to reject a compact 128 MHz block whose composed
island closes at 118 MHz. The binding gates are composition gates.

3.2 Resource target
-------------------

Measured first-pass warning sum:

  15,749 ALMs
  25,123 registers
  11 M10Ks
  16 DSPs

Production texture-survivor targets:

  ALMs       5,500-6,600 target; 7,500 hard redline before composition
  registers  6,000-8,000 target; 9,000 hard redline
  M10Ks      32-56 expected; 64 hard redline before evidence
  DSPs       10-13 target; 14 hard redline

At 6,600 ALMs, the island recovers about 9,149 ALMs, or 58% of the prototype
sum. At the 7,500 redline it still recovers 8,249 ALMs, or 52%.

At 8,000 registers it recovers 17,123 registers. The additional M10Ks are an
intentional correction, not a regression: 56 M10Ks are about 10% of the
provisional device, and the full console fit still decides whether that trade
is affordable.

3.3 Per-component design budgets
--------------------------------

These are architecture budgets, not predicted fit results. A block exceeding one
must explain why the total island still remains below the hard redline.

  component                         ALM target  reg target  M10K  DSP
  -----------------------------------------------------------------
  FRAGROB + token fabric                  900       1,200   14-20   0
  RCP24 scheduler v2                      650         600    0-1  3-4
  perspective pair pipeline               900         700    0-1    6
  binding tables + TMU planner v2         700         500   6-10    0
  synchronous texture cache v2            900         900   8-10    0
  class router + decode stores            350         400    3-6    0
  transactional resident palette         250         200    4-8    0
  serial bilinear channel engine          250         200    0-1    1
  Mosaic CSD pipeline                     500         350      0    0
  material combiner                       650         500    1-4  0-2
  AUX v2                                  550         500    1-3    0
  -----------------------------------------------------------------
  nominal architecture total            6,600       6,050  37-64 11-13

The M10K range is deliberately broad because exact width/depth packing is tool
truth. The hard island redline is 64, not the sum of every pessimistic row.

3.4 Automatic resource tripwires
--------------------------------

Every fit target records both maxima and required minima.

Examples:

  cache v2:
    require M10K >= 8
    reject registers > 2,000
    reject ALMs > 1,500

  FRAGROB:
    require its declared payload banks to infer as RAM
    reject registers > 2,500

  Mosaic:
    require DSP == 0 for the CSD variant

  RCP24:
    reject DSP > 4 after the 32x32 width proof is implemented

  material combiner:
    reject DSP > 2

A fit that meets Fmax while violating its memory/DSP structure is not a pass.


4. THE PRODUCTION ISLAND
========================

4.1 Top-level flow
------------------

  RASTER.EARLYZ survivor
       |
       v
  TEX.FRAGROB allocate {slot, generation8}
       |-------------------------------> AUX request token (immediate)
       |
       +--> RCP request, once per fragment with sample_count > 0
                    |
                    v
              RCP24.SCHED.V2
                    |
                    v
              RCP completion FIFO
                    |
                    v
              SAMPLE.EXPANDER
              (0..3 descriptor reads;
               at most one sample job/clock)
                    |
                    v
              PERSPUV.PAIR.PIPE
              (U and V together, II=1)
                    |
                    v
              BINDING + TMU.PLAN.V2
                    |
                    v
              CACHE.V2 C0-C4
                    |
                    v
              CLASS ROUTER
                 /      |       \
              CLUT    NEAREST   BILINEAR
                |        |         |
              PALETTE    |       BILERP.SCHED
                 \       |        /
                  \      |       /
                   SAMPLE COMPLETION MERGE
                            |
                            v
                     token-indexed result banks
                            |
                 +----------+----------+
                 |                     |
               AUX result          join-check events
                 |                     |
                 +----------+----------+
                            |
                            v
                    MATERIAL.COMBINE
                            |
                            v
                  final-result-by-slot RAM
                            |
                            v
                 allocation-order retire pipe
                            |
                            v
                    RASTER.FRAGMENT

4.2 Why this topology is smaller
--------------------------------

One fragment context is written once.

One sample descriptor is written once per material sample.

One cache footprint is stored in exactly one class-owned place.

One result is written directly to its sample bank.

Every queue between them carries a 12-16 bit token plus at most a few control
bits. The same 64- to 200-bit payload is not copied through raw FIFO, class FIFO,
ROB entry and output register simultaneously.

4.3 Why this topology is faster
-------------------------------

The hot path contains no 16-entry scan, no 48-entry scan and no combinational
RAM read.

Every wide memory read is followed by a fabric capture register.

The variable shift and saturation in perspective recovery are separate stages.

Static binding arithmetic is performed on load, not per sample.

Cache hit classification occurs from captured RAM outputs.

Output pins are never timing endpoints of the internal arithmetic; the composed
island has registered packet seams.

4.4 Why it still supports three-sample materials
------------------------------------------------

FRAGROB stores three independent sample descriptors.

RCP runs once because every sample of a fragment shares invw24.

The sample expander emits up to three perspective jobs using that reciprocal.

The perspective pair pipe accepts one job per clock.

The one TMU accepts one sample per clock.

Samples and fragments interleave freely. Retirement remains in allocation order.

No second general TMU is added.


5. TOKEN, RECORD AND QUEUE CONTRACT
==================================

5.1 Internal identity
---------------------

The native fragment identity is:

  frag_tag = { generation[7:0], slot[3:0] }          12 bits

The native sample identity is:

  sample_tag = { generation[7:0], slot[3:0], sidx[1:0] }  14 bits

When a generic 16-bit transport field is useful:

  bits  3:0   slot
  bits  5:4   sample index
  bits 13:6   generation
  bits 15:14  kind

  kind 00  primary sample
  kind 01  AUX
  kind 10  fragment / reciprocal
  kind 11  reserved and rejected

Sample index 3 is invalid and raises a sticky frame fault.

source_id remains a draw/asset provenance value stored in the fragment context.
It never routes a return.

5.2 Generation and ABA bound
----------------------------

Generation increments on every allocation of a slot. It is never reset on a
normal frame boundary.

Eight bits are the binding ruling, but the protocol must also bound return
lifetime. A finite generation field does not make an infinitely delayed duplicate
safe by magic.

Required assertions:

  - a return source cannot retain a response after its transaction has been
    retired and the island has acknowledged a flush;
  - no source contains more than the declared bounded queue depth;
  - an abort invalidates every live slot before any slot is repopulated;
  - a slot cannot be reused more than 255 times while any pre-abort source may
    still return an old token;
  - generation is compared before any payload write.

On explicit island abort:

  1. stop allocation;
  2. stop issuing new memory work;
  3. drain or cancel bounded internal queues;
  4. walk sixteen slots, clear valid and increment generation;
  5. repopulate the free-slot FIFO;
  6. acknowledge abort.

An asynchronous reset is not the transaction protocol.

5.3 Queue law
-------------

All hot queues are powers of two unless they use explicit wrap compare.

Every queue count or pointer difference is width-derived from the declared depth.
No default-only two-bit pointer is accepted behind a parameter named REQN.

Every push/pop count receives one net update. For two possible producers, form:

  pushes = producer0_fire + producer1_fire
  count_next = count + pushes - pop

and prove the maximum simultaneous pushes fit the physical write ports.

If two producers can write distinct entries in one clock, the storage must be
banked or multiported. Writing two dynamic addresses into an unpacked array and
hoping for M10K is not a design.

5.4 Credit law for fixed pipelines
----------------------------------

A fixed-latency pipeline that cannot stall internally reserves one output credit
at input acceptance.

  input_ready = (credits != 0)
  credits -= accept
  credits += final_output_pop

The counter is updated once for same-cycle accept and pop.

The output FIFO depth must be at least pipeline latency plus the maximum number
of responses that can be committed while downstream ready is absent for the
contracted stall window. If downstream may stall without a bound, the pipeline
must be able to stop at a registered boundary rather than overwrite data.

5.5 Direct-index return law
---------------------------

A valid return performs:

  decode kind / slot / generation / sample index
  check slot valid
  check generation
  check sample index legal
  check required bit set
  check arrived bit clear
  write result bank
  set arrived bit

A duplicate return never overwrites the first result. It increments a duplicate
counter and faults the frame.

A stale or invalid return never backpressures the returning unit if its error
record has reserved local space. The error path is bounded and must not turn a
corrupt response into a system deadlock.


6. TEX.FRAGROB: THE TRANSACTION CENTRE
=====================================

6.1 Replace the current TEXJOIN monolith
----------------------------------------

Create a new production block beside v1 and v2:

  zhao_texture_fragrob

Do not mutate zhao_raster_texjoin_v2 until it becomes impossible to tell which
implementation its tests describe. Keep v2 as the oracle for token allocation,
multi-sample completion, generation rejection and allocation-order retirement.

The production FRAGROB owns:

  - sixteen fragment slots;
  - free-slot allocation;
  - generation;
  - raw interpolated sample descriptors;
  - fragment context storage;
  - required/arrived masks;
  - reciprocal request identity;
  - AUX request identity;
  - sample and AUX return validation;
  - combine-ready state;
  - final-result state;
  - ordered retirement;
  - all texture-island frame faults.

It does not own:

  - reciprocal arithmetic;
  - perspective arithmetic;
  - texture address arithmetic;
  - cache policy;
  - palette arithmetic;
  - bilinear arithmetic;
  - material-combiner arithmetic;
  - RASTER.FRAGMENT blending or depth/stencil behavior.

6.2 Allocation boundary
-----------------------

Input is the post-Early-Z survivor before perspective recovery.

Minimum logical packet:

  fragment_context              opaque, width from the real fragment seam
  invw24                        u24
  sample_count                  u2, legal 0..3
  recipe                        u3
  recipe_weight                 unit8
  for sample 0..2:
      u_over_w                  s32, S8.24
      v_over_w                  s32, S8.24
      binding_slot             generated width
      binding_generation       u8
      lod                       Q4.4 / exact inherited byte
  aux_required                  1 bit
  aux_binding_slot              generated width
  aux_binding_generation        u8
  world_x, world_z              fx16, or an opaque context pointer if already
                                resident in fragment_context
  status/error bits             inherited

The production interface should use packed structs or generated field slices so
one definition feeds RTL, test harness and zref. Do not manually duplicate this
layout in three modules.

6.3 Control state: flops only where direct indexed mutation is useful
---------------------------------------------------------------------

Per slot, keep the following in small direct-indexed registers:

  valid
  generation[7:0]
  sample_required_mask[2:0]
  sample_arrived_mask[2:0]
  aux_required
  aux_arrived
  combine_enqueued
  combine_done
  final_ready
  sticky sample/status OR
  malformed/fault bit

This is a few hundred registers total and permits TMU and AUX return events to
update independent fields without a RAM read-modify-write loop.

Do not put wide U/V, context, bindings or colours in these flops.

6.4 Payload RAMs
----------------

Bank by sample index so one accepted three-sample fragment can write sample 0,
1 and 2 in the same clock without a three-write-port memory.

Recommended physical banks:

  SAMPLE_DESC_U[3]       each 16 x 32
  SAMPLE_DESC_V[3]       each 16 x 32
  SAMPLE_DESC_META[3]    each 16 x 32 or 40

META contains binding slot, binding generation, LOD and per-sample flags. If the
width exceeds one convenient RAM shape, split it deliberately. Do not allow the
tool to implement a wide dynamic array as flops.

Sample results must also be banked by sample index so the material combiner can
read all three in parallel:

  SAMPLE_RESULT[3]       each 16 x result_word

A result word contains:

  RGB888 or the exact internal colour width
  alpha8
  palette index8
  status/error bits
  has_texture / format status as required

Fragment payload banks:

  FRAGMENT_CONTEXT       16 x real downstream context width
  FRAGMENT_META          recipe, weight, source/provenance, AUX binding, etc.
  FINAL_RESULT           16 x final sample packet + AUX sideband

Use M10K even when depth utilization looks poor if the alternative is thousands
of always-live flops. The physical report, not abstract bit efficiency, decides.

6.5 Free-slot allocation
------------------------

Use a sixteen-entry free-slot FIFO initialized by a sixteen-cycle sweep after
reset/abort.

  allocation pops one slot
  final output acceptance pushes one slot

No free-slot scan.

The generation increments when allocation commits, before any request carrying
the token leaves the block.

6.6 Reciprocal issue
--------------------

If sample_count == 0, issue no reciprocal.

Otherwise push one narrow record into RCP_REQ_FIFO:

  {frag_tag, invw24}

The FIFO is at least sixteen deep, because every live fragment may be waiting
for reciprocal service. It is narrow enough for MLAB or a small M10K.

The current architecture’s sample work queue is not needed in production.
Reciprocal completion itself becomes the bounded source of sample work.

6.7 Sample expansion after reciprocal
--------------------------------------

RCP_RESULT_FIFO carries:

  {frag_tag, reciprocal_mantissa24, exponent6, zero/error}

A two- to four-entry FIFO is sufficient initially. The reciprocal engine cannot
complete faster than one fragment per four clocks; the expander emits at most
three sample jobs in three clocks.

SAMPLE.EXPANDER holds one RCP result and walks sidx 0..sample_count-1:

  E0  choose current sidx; issue synchronous descriptor-bank read
  E1  capture u_over_w, v_over_w and metadata
  E2  present one PERSPECTIVE_PAIR request

If PERSPECTIVE_PAIR is credited and II=1, the expander finishes every fragment
before the next worst-case RCP completion. There is no 64-entry 48-way-discovery
queue, no three-entry multiwrite, and no need to rediscover outstanding samples.

If a future RCP variant completes faster, resize the result FIFO or permit two
active expansion contexts. Do not prebuild that complexity.

6.8 AUX issue
-------------

On allocation with aux_required, push frag_tag into AUX_REQ_FIFO.

The fragment’s world coordinate and AUX binding are read by token from FRAGROB
or a dedicated AUX descriptor RAM. The FIFO carries no repeated envelope.

AUX starts immediately and runs in parallel with reciprocal/perspective/TMU.

6.9 Completion events
---------------------

Primary decode completion and AUX completion may happen in the same clock.
Avoid a multiwriter completion table and avoid a sixteen-entry completion scan.

Use two narrow event skids:

  SAMPLE_DONE_EVENT = frag_tag
  AUX_DONE_EVENT    = frag_tag

A one-token-per-clock JOIN_CHECK stage reads current control state after return
writes and asks whether the fragment is complete. If both events name the same
slot, they may be checked on consecutive clocks; combine_enqueued prevents a
duplicate. One cycle of completion latency is free.

JOIN_CHECK pushes frag_tag to COMBINE_FIFO exactly once when:

  sample_arrived_mask == sample_required_mask
  and (aux_arrived or !aux_required)
  and !combine_enqueued

For sample_count=0 and no AUX, allocation emits a join-check event after its
control write.

6.10 Material combination
-------------------------

COMBINE_FIFO carries only frag_tag.

The combiner reads the three result banks and fragment recipe/meta, computes the
frozen recipe, and writes FINAL_RESULT[slot]. It then sets final_ready[slot].

Combination order is independent of fragment retirement order. This prevents a
slow head fragment from idling the combiner while later complete fragments wait.

6.11 Ordered retirement
-----------------------

Maintain allocation head_q. There is no search.

When valid[head] && final_ready[head] and no retirement read is active:

  R0  issue synchronous FRAGMENT_CONTEXT and FINAL_RESULT reads
  R1  capture both into fabric registers
  R2  hold the final output packet until RASTER.FRAGMENT accepts it

Only output acceptance:

  clears valid/final state;
  advances head;
  returns the slot to the free FIFO.

The output packet must remain bit-stable under arbitrary consumer stall.

6.12 Return ports and write conflicts
-------------------------------------

The primary TMU may produce at most one raw sample response per clock, but CLUT,
nearest and bilinear decode paths may converge and finish in the same clock.
Do not require three write ports on SAMPLE_RESULT.

Each class output owns a two-entry completion skid. A three-to-one round-robin
SAMPLE_COMMIT merger writes at most one sample result per clock.

This is throughput-safe because the cache admits at most one sample response per
clock on average. The skids absorb latency convergence; the merger sustains the
same one result/clock average.

If a stress trace overflows a two-entry class skid, first raise it to four and
measure. Do not add a multiport result RAM without evidence.

6.13 FRAGROB resource and timing gate
-------------------------------------

Target:

  <= 900 ALMs
  <= 1,200 registers
  14-20 M10Ks expected
  0 DSP
  >= 125 MHz in registered physical wrapper

Hard rejection:

  any live-entry scan larger than four;
  any sample/context payload array in flops above the explicit control bits;
  any combinational output from RAM/table storage;
  any external-ready path to allocation;
  any source_id-based matching;
  any placeholder material arithmetic.


7. RCP24.SCHED.V2
=================

7.1 Keep the arithmetic; replace the control topology
------------------------------------------------------

Create:

  zhao_raster_rcp24_sched_v2

Keep zhao_raster_rcp24 and zhao_raster_rcp24_svc as arithmetic and throughput
oracles.

The current scheduler correctly interleaves four dependent microjobs among eight
contexts. The production rewrite removes:

  - free-context scan;
  - ready-context scan;
  - done-context scan;
  - combinational completion selection into the output;
  - accidental 32x64 multiplier expression.

7.2 Exact operand-width proof
-----------------------------

The four products are:

  MW0: m24 * x32
  MX0: x32 * (2^31 - w)
  MW1: m24 * x32
  MX1: x32 * (2^31 - w)

The exhaustive reciprocal audit measured maximum w = 0x401FEF88, below 2^31.
Therefore t = 2^31 - w is non-negative and fits 31 bits. Both phase families fit
unsigned 32 x 32 operands.

Implement one explicit phase-selected 32x32 product:

  mul_a[31:0]
  mul_b[31:0]
  mul_p[63:0]

Do not express mul_b as 64 bits. Width is physical architecture on this device.

Retain uint64 modulo behavior where the reference requires it, but do not widen
an operand whose proved high bits are zero.

7.3 Queued control
------------------

Use:

  FREE_CTX_FIFO        eight context IDs
  NEW_JOB_FIFO         IDs newly allocated at MW0
  CONT_JOB_FIFO        IDs made ready by multiplier writeback
  RESULT_FIFO          completed {frag_tag, mant24, exponent6, zero/error}

The multiplier issue arbiter chooses between CONT_JOB_FIFO and NEW_JOB_FIFO.
Use bounded fair arbitration; continuation may receive priority to minimize live
context pressure, but new work must not starve.

One accepted reciprocal:

  pops a free context;
  writes normalized m, seed x, k, token;
  pushes context ID to NEW_JOB_FIFO.

One multiplier writeback:

  updates context;
  either pushes ID to CONT_JOB_FIFO for the next phase;
  or pushes the final result and returns the context ID after result acceptance.

No scan is necessary.

7.4 Front-end stages
--------------------

R0  capture raw d_i and frag_tag.

R1  24-bit leading-zero count / normalization shift / zero classify.

R2  seed-ROM lookup capture and context allocation.

The leading-zero count must be a balanced tree or the already proven generated
shape, not a 24-iteration priority chain inferred from a loop.

The seed output is registered before entering the multiplier context.

7.5 Multiplier stages
---------------------

M0  pop a context ID and read phase operands.

M1  one 32x32 product, registered.

M2  phase-specific rescale and context writeback.

M2 does not drive the external output. Final `(x + 64) >> 7` and clamp occur in
a dedicated result stage and enter RESULT_FIFO.

7.6 Throughput decision
-----------------------

Four product launches per reciprocal on one product lane give a hard II of four:

  1,333,333 / 4 = 333,333 reciprocals per design frame.

The canonical pre-Early-Z upper target is 320,000 covered fragments and only
survivors request reciprocal. Therefore one lane is retained initially.

This is not luxurious. Required counters:

  accepted fragments
  reciprocal requests
  RCP issue clocks
  multiplier busy clocks
  result FIFO full clocks
  post-Z survivor count

A second lane is considered only if a measured legal post-Z trace exceeds
300,000 reciprocal requests or the RCP queue contributes more than 5% of frame
stall clocks. Do not double the multiplier from the conservative pre-Z envelope
alone.

7.7 Resource and timing gate
----------------------------

Target:

  <= 650 ALMs
  <= 600 registers
  <= 1 M10K
  3 DSP preferred, 4 DSP hard ceiling
  >= 125 MHz across three seeds in the physical wrapper

The seed sweep on the current block remains useful to separate virtual-output
skew from internal logic. It does not remove the need for the scan-free, 32x32
production form.


8. PERSPECTIVE_PAIR.PIPE
========================

8.1 Replace the token-table scheduler
-------------------------------------

Create:

  zhao_raster_perspuv_pair_pipe

The current zhao_raster_perspuv_svc is an excellent arithmetic oracle and a bad
production topology. Once reciprocal has returned, each sample job is fixed
latency. It needs no 16-entry table, no oldest-work scan and no completion scan.

8.2 Input packet
----------------

  sample_tag
  u_over_w s32
  v_over_w s32
  reciprocal mantissa u24
  exponent k u6
  binding slot + generation
  LOD byte
  inherited status

The sample expander already supplies one job in the order it wants accepted.

8.3 Exact width
---------------

A signed 32-bit numerator times an unsigned 24-bit mantissa needs a signed
56-bit product domain. Derive and static-assert the exact width in the source.
Do not keep 64 bits merely because the oracle used uint64 storage.

The implementation may retain one or two guard bits if the proof names the
maximum bias and saturation compare, but every retained bit must have a stated
consumer.

8.4 Fixed streaming stages
--------------------------

Recommended stages:

  P0  capture packet; derive shift = 32 - k; reserve output credit

  P1  launch TWO products concurrently
        prod_u = u_over_w * mantissa
        prod_v = v_over_w * mantissa
      register both products and shift

  P2  add exact round-half-up bias to both products
      register biased products

  P3  variable arithmetic right shift on both lanes
      register rescaled wide values

  P4  compare against s32 rails and select saturated/exact result
      OR the two saturation flags
      register final pair

  P5  push held output packet / result FIFO

One stage contains products. One stage contains wide add. One contains barrel
shift. One contains saturation. The measured 15 ns product-to-table cone cannot
exist.

8.5 Flow control
----------------

The internal arithmetic pipeline does not stall. An output FIFO reserves credit
at P0. Depth of eight is sufficient for a five/six-stage pipe plus two clocks of
merge pressure; prove the exact bound in the implementation.

The output packet remains held until TMU planning accepts it.

No consumer ready propagates through P4/P3/P2 to input.

8.6 Why two product lanes are required
--------------------------------------

The full conservative sample envelope is 1,094,600 sample invocations. Each
requires U and V recovery.

One product lane provides 666,666 products in the 1,333,333-clock design budget,
or 333,333 complete pairs. It cannot sustain the material capability.

Two concurrent product paths provide one pair per clock and 1,333,333 pair
capacity. That covers the 1,094,600 upper envelope with 238,733 pair clocks
remaining before the 20% frame reserve boundary.

This is a deliberate DSP increase relative to the current one-product lane, paid
for by DSP reductions in RCP, Mosaic and bilerp.

8.7 Optional UV alias optimization
----------------------------------

The material compiler MAY mark sample 1 or 2 as sharing a recovered UV pair with
an earlier sample. If so, SAMPLE.EXPANDER may issue one perspective job and clone
the final pair to multiple TMU requests.

This is an optimization only. The baseline capacity and acceptance tests assume
three independent UV sets.

8.8 Resource and timing gate
----------------------------

Target:

  <= 900 ALMs
  <= 700 registers
  <= 1 M10K
  6 DSP hard initial budget
  II = 1 pair/clock
  >= 125 MHz across three seeds

A 4-DSP implementation may be explored only after an exact operand-decomposition
proof and paired differential. Do not delay the 6-DSP production baseline for a
packing theory.


9. RESIDENT BINDINGS AND TMU.PLAN.V2
====================================

9.1 Static work must stop occurring per sample
----------------------------------------------

The current planner receives a 32-bit mode word and base address with every
sample, then repeatedly:

  decodes format;
  sanitizes illegal combinations;
  derives selected level dimensions;
  computes a mip-chain offset;
  derives masks;
  identifies palette behavior;
  classifies nearest/bilinear/CLUT.

Most of that is material state, not sample state.

Create:

  zhao_texture_binding_table
  zhao_texture_tmu_plan_v2

A material binding is validated and resolved before the frame is sealed. The
hot sample packet carries only:

  sample_tag
  final perspective U/V
  binding slot
  binding generation
  LOD

9.2 Binding root record
-----------------------

Recommended root fields:

  generation8
  resident/valid
  format3
  filter1
  wrap_u2
  wrap_v2
  log2_width4
  log2_height4
  max_level4
  mip_enable1
  palette_slot
  palette_generation8
  base address32
  material/status flags

Binding slot count starts at 32. Fit 32 and 64 only after a real working-set
trace. Do not instantiate a 16-way or 64-way search: the material packet already
carries the resolved slot.

9.3 Per-level descriptor table
------------------------------

Precompute on load, using the exact inherited law, one descriptor for every
legal binding/level pair:

  level_base_byte_address32
  width_mask, up to MAXLOG2 bits
  height_mask
  log2_level_width4
  log2_level_height4
  byte/halfword addressing class
  class = CLUT / NEAREST / BILINEAR
  any exact byte/nibble selectors that are static

This removes REP4 multiplication/shift and static format offset arithmetic from
the per-sample path.

The loader and zref generator produce these values from one function. The RTL
checks generation and reads them; it does not independently reinvent mip layout.

9.4 Atomic binding load
-----------------------

Use a BEGIN / WRITE / END transaction or load an inactive binding page and flip
one active-page pointer after CRC verification.

A binding generation changes only at successful END.

A request carrying a stale generation is rejected and faults the frame. It does
not silently use the newer material.

9.5 Planner stages
------------------

Suggested pipeline:

  T0  capture sample packet; issue binding-root read

  T1  capture binding; validate generation/residency; clamp/validate requested
      LOD; issue per-level descriptor read

  T2  capture level descriptor; scale U/V by level dimensions; apply exact
      half-texel bias; register integer coordinates and fu/fv

  T3  form +1 taps in parallel; carry narrow low bits plus sign/overflow flags

  T4  wrap two unique U and two unique V coordinates at CW = MAXLOG2+1

  T5  form two row indices and four texel/byte positions

  T6  add precomputed level base; form registered cache-access packet

Every stage is elastic or credit-backed. No single valid bit owns the whole
pipe.

9.6 Width rules
---------------

Retain the current proved narrowing:

  MAXLOG2 = 11 initial configurable maximum
  coordinate fold width CW = MAXLOG2 + 1 = 12
  level texel-index width TW = 2*MAXLOG2 + 2 = 24
  final byte address width = 32

Dimensions above MAXLOG2 fault; they do not wrap.

REPEAT and MIRROR retain exact two’s-complement low-bit behavior. CLAMP carries
negative and overflow flags from the discarded high bits.

9.7 Packet-width reduction
--------------------------

The current planner’s hundreds of virtual pins are partly an artifact of passing
mode/base metadata on every request. The production boundary is approximately:

  U32 + V32 + token14 + binding slot5/6 + generation8 + LOD8 + status

not:

  U32 + V32 + base32 + mode32 + multiple wide result buses.

This helps routing, but it is not used to excuse a slow internal path.

9.8 Resource and timing gate
----------------------------

Binding table plus planner target:

  <= 700 ALMs
  <= 500 registers
  6-10 M10Ks
  0 DSP
  II = 1 sample/clock
  >= 125 MHz across three seeds

Differential gate:

  every inherited 79-case TMU planning condition;
  CLUT4 and CLUT8 included;
  all mip levels and rails;
  all wrap modes, including negative coordinates;
  malformed binding/generation cases;
  random stalls;
  100% address/enables/class equality against the shipped oracle.


10. TEXTURE.CACHE.V2: THE ALM RECOVERY CENTRE
=============================================

10.1 Replace, do not patch
--------------------------

Create:

  zhao_texture_cache_v2

Keep zhao_texture_cache and zhao_texture_cache_pipe as behavioral oracles for
line identity, fill order, counters, invalidation and same-line multicast.

Do not attempt to rescue the current cache_pipe by sprinkling registers around
its combinational arrays. The RAM interface, hit pipeline, fill replay and
invalidate law must be coherent from the beginning.

10.2 Retained cache policy
--------------------------

Retain:

  four independent lanes, one per possible bilinear tap;
  direct-mapped, read-only lines;
  16-byte line size;
  one blocking miss engine initially;
  one physical line fetch multicast into every lane requesting the same line;
  nearest enabling lane 0 only;
  exact little-endian halfword behavior;
  per-line and global invalidation;
  first-look hit/miss accounting.

Do not add:

  associativity;
  PLRU;
  dirty state;
  writeback;
  coherence;
  MSHRs;
  hit-under-miss;
  speculative fill bypass.

Those are trace-driven future options, not repairs for the measured problem.

10.3 Capacity ruling
--------------------

Start with:

  LANES      = 4
  LINES      = 64 per lane
  LINE_BYTES = 16

Per lane data:

  64 lines x 16 bytes = 1,024 bytes = 8,192 bits

At a 16-bit halfword port this is 512 entries, fitting inside one 10-Kbit memory
block by capacity. The physical fitter must confirm one data M10K per lane.

This raises data capacity from 1 KiB total to 4 KiB total without necessarily
raising the four-data-bank M10K count. It is the natural capacity point before
128 lines would require another data block per lane.

Fit LINES=32 and LINES=64 on identical RTL. Keep 64 if M10K count and Fmax are
unchanged or better. Capacity is a parameter and may be reduced only by a full-
console M10K conflict, not by discomfort at utilization.

10.4 Storage organization
-------------------------

Per lane:

  DATA_RAM     512 x 16   synchronous read, one write port for fill
  TAG_RAM       64 x approximately 30

TAG_RAM stores:

  physical tag
  cache epoch8
  optional one-bit state if needed

Four static generate banks are mandatory. Lane selection is a compile-time
index; each bank has its own write enable. A dynamic lane index selecting which
memory to write is rejected because it is the known route to flip-flop inference.

Expected initial RAM count:

  4 data M10Ks
  4 tag M10Ks
  optional 1-2 response/fill queue M10Ks

10.5 Global and line invalidation
---------------------------------

Global invalidate increments current_cache_epoch. Hit requires stored_epoch ==
current_cache_epoch and tag equality.

This avoids a 256-entry clear loop and leaves data RAM untouched.

On epoch wrap, stop acceptance and sweep all 4 x 64 tag entries before returning
to epoch 1. Epoch 0 may be reserved invalid.

Per-line invalidate writes a non-current epoch/tag-invalid state into the named
index in every lane. If it races a completing fill, invalidate wins.

10.6 Hit pipeline
-----------------

The production stages are:

  C0  registered ingress / two-entry skid
      capture {token, enables, four addresses, class}

  C1  derive and register tag/index/beat for each lane
      issue synchronous DATA_RAM and TAG_RAM reads

  C2  capture every RAM output into fabric registers
      no compare, mux or output selection before this capture

  C3  compare captured tags/epochs; classify hit/miss
      select one missing line identity
      compute same-line fill_lane_mask
      capture hit data and miss record

  C4  on all-hit, push one registered response
      on miss, transfer one bounded miss record to fill engine

No M10K output launches a cross-module signal or wide combinational cone.

10.7 Multiple hit requests in flight
------------------------------------

The hit pipe accepts one request per clock until a miss reaches C3/C4.

On one blocking miss:

  - freeze new ingress at a registered boundary;
  - retain younger requests already in C0-C2;
  - issue/fill one physical line;
  - write every matching lane through fill_lane_mask;
  - re-read the missed request through C1-C4;
  - if another distinct line is still missing, repeat;
  - otherwise release the pipe.

This preserves the deliberately blocking policy while retaining II=1 on a hit
stream.

Do not recompute the request from external pins during replay. The miss record
owns the exact token, addresses, enables and class.

10.8 Response buffering
-----------------------

A local response FIFO of at least eight samples terminates consumer backpressure.
It may be a RAM-backed packet FIFO or a two-entry register skid in front of a
RAM FIFO.

CACHE input-ready never reads class-engine ready or final fragment ready.

10.9 Fill engine
----------------

Fill request packet:

  line_base32
  fill_lane_mask4
  expected tag/index
  source/sample token for accounting

Every returned halfword beat writes all lanes in fill_lane_mask at the same
beat address.

Tags become current only after the last beat. A partial/torn line is never valid.

If fill is denied, malformed or aborted, invalidate every destination lane and
fault the frame. Never expose partially written data.

10.10 Counters
--------------

Count once per first look:

  enabled lane hits
  enabled lane misses
  physical line fills
  multicast lanes saved
  hit requests
  miss requests
  fill busy clocks
  pipeline stall clocks
  response FIFO full clocks

The relation:

  lane_hits + lane_misses = enabled lane first-look count

is asserted.

10.11 Resource and timing gate
------------------------------

Target:

  <= 900 ALMs
  <= 900 registers
  8-10 M10Ks
  0 DSP
  II = 1 hit request/clock
  >= 125 MHz across three seeds

Hard rejection:

  M10K < 8 without an explicit better packing report;
  registers > 2,000;
  any combinational data/tag memory read;
  any reset over data RAM;
  any output-ready path reaching C0 acceptance;
  four same-line taps causing more than one physical fill.

This block is expected to recover the largest single share of the current ALM
and register overrun.


11. CACHE RESPONSE, DECODE AND RESULT COMMIT
============================================

11.1 Delete copied wide queues
------------------------------

The current response dispatcher stores a wide payload in a raw FIFO and then
copies it into one of three more wide class FIFOs. It is functionally useful and
physically wasteful.

Create:

  zhao_texture_class_router
  zhao_texture_near_decode
  zhao_texture_clut_decode
  zhao_texture_filter_front
  zhao_texture_sample_commit

11.2 Cache response packet
--------------------------

The cache response carries:

  sample_tag14
  raw lane halfwords 4 x 16
  class2
  status/error bits

The detailed decode plan — format, byte/nibble selection, fu/fv, palette slot
and generation — lives in SAMPLE_PLAN_RAM indexed by sample_tag’s slot/sidx.
It is written once by TMU.PLAN.V2. Do not carry it through every queue.

11.3 Class routing
------------------

Use three class-owned queues:

  CLUT_QUEUE
  NEAREST_QUEUE
  BILINEAR_QUEUE

Each queue is depth 8 initially. Each entry stores sample_tag and the one raw
footprint. Implement as M10K/MLAB if the fitter chooses; do not duplicate an
additional raw FIFO unless a measured burst requires it.

CACHE response-ready depends only on room in the selected local class queue.
It never depends on palette, bilerp or sample-commit consumer ready directly.

Unknown class faults and consumes the packet into an error record; it does not
stall forever.

11.4 Nearest path
-----------------

One registered decode stage:

  read SAMPLE_PLAN_RAM by token;
  choose byte/nibble/halfword;
  decode direct format or raw index as required;
  form sample result;
  push its two-entry completion skid.

II=1, no DSP.

11.5 CLUT path
--------------

Stages:

  K0  read plan; extract CLUT4/CLUT8 index exactly
  K1  issue resident palette lookup {slot, generation, index}
  K2  capture RGB565 and expand through the inherited exact helper
  K3  form sample result with raw index and alpha255
  K4  push completion skid

A stale/nonresident palette does not fetch through the hot texture cache. It
raises a bounded palette-miss event for the frame manager and faults or applies
a predeclared material degradation next frame.

11.6 Bilinear front
-------------------

Read plan by token, decode four direct-colour texels into exact channel values,
and expand the footprint into channel jobs:

  RGB565       R,G,B only; alpha is constant 255
  ARGB1555     A,R,G,B as the inherited law requires
  ARGB4444     A,R,G,B

A small FILTER_ROB, depth 8 initially, stores token, required-channel mask and
returned channel bytes. It is not the fragment ROB and does not copy fragment
context.

The channel expander feeds BILERP.SCHED one job at a time. Completion forms one
sample result and pushes the bilinear completion skid.

11.7 Sample completion merge
-----------------------------

The three class paths may finish on the same clock even though average input is
one sample/clock. Give each a two-entry held skid.

A fair three-to-one merge commits one result per clock to the sample result bank
and emits SAMPLE_DONE_EVENT.

Since the average arrival rate is bounded by cache acceptance, one commit port is
sufficient. Add counters for per-class skid full and merge wait. A class skid
overflow is a frame fault and a failed architecture gate.

11.8 Resource and timing gate
-----------------------------

Class router + decode stores target:

  <= 350 ALMs
  <= 400 registers
  3-6 M10Ks
  0 DSP
  >= 125 MHz

The old 806 ALM / 1,432 register response dispatcher should not survive merely
because its Fmax was the best of the first-pass leaves.


12. TRANSACTIONAL RESIDENT PALETTES
==================================

12.1 Create palette v2
----------------------

Create:

  zhao_texture_palette_v2

Keep palette_res as a behavior oracle for generation rejection and stale lookup.

Initial slot count:

  PAL_SLOTS = 8 preferred
  PAL_SLOTS = 4 minimum comparison point
  256 entries per palette
  RGB565 entries
  generation width 8

12.2 Ping-pong page per slot
----------------------------

Each logical slot has two physical 256-entry pages: active and inactive.

A 256 x 16 page is 4,096 bits. Two pages are 8,192 bits, so one logical slot is
a natural one-M10K target by capacity. Eight slots therefore target eight
M10Ks.

The active-bank bit and generation are small control registers.

12.3 Load transaction
---------------------

  BEGIN(slot, new_generation, expected_crc, entry_count=256)
    require slot not already loading
    require new_generation != active_generation
    select inactive bank
    reset rolling CRC and count

  WRITE(slot, index, RGB565)
    legal only for the active load transaction
    write inactive bank
    update CRC and seen/count law

  END(slot, generation)
    require exactly 256 legal entries
    require expected CRC match
    atomically flip active bank
    atomically publish new generation/resident

  ABORT(slot)
    discard inactive load state
    leave active bank/generation untouched

A failed or partial upload can never tear the active palette.

12.4 Lookup
-----------

One lookup per clock:

  L0 capture {sample_tag, slot, expected_generation, index}
  L1 synchronous active-bank RAM read
  L2 fabric capture + generation/resident verdict
  L3 registered result

The generation verdict is tied to the active-bank snapshot accepted with the
request. A simultaneous successful END is ordered explicitly; choose and test
one law. Recommended law: END wins for requests accepted after the edge, old
requests retain the old captured bank/generation.

12.5 Cold behavior
------------------

The hot path never performs a search and never launches a palette fetch through
the texture cache.

A nonresident or stale palette emits a narrow miss event. Frame sealing should
prevent it. If it reaches hardware, the frame faults; an optional next-frame
loader/degradation decision is software policy.

Do not let one cold palette monopolize every resident request.

12.6 Resource and timing gate
-----------------------------

PAL_SLOTS=8 target:

  <= 250 ALMs
  <= 200 registers
  8 M10Ks expected
  0 DSP
  II = 1 lookup/clock
  >= 125 MHz

Required tests:

  lookup during inactive-bank load returns old active value;
  successful END switches atomically;
  CRC failure preserves old palette;
  missing/duplicate index fails;
  stale generation never returns usable color;
  same-slot load and lookup ordering;
  generation wrap handling;
  randomized stalls and back-to-back commits.


13. BILERP.SCHED: ONE MULTIPLIER UNTIL TRACES SAY TWO
====================================================

13.1 Why the current II=1 lane is overprovisioned
-------------------------------------------------

The current channel lane is elegant and exact, but it spends three DSP
structures to accept one channel job per clock.

The named workload contains approximately 180,000 filtered channel jobs/frame.
One channel job every three clocks provides:

  1,333,333 / 3 = 444,444 jobs per design frame

or 2.47x the named workload. The three-DSP II=1 implementation solves a problem
the current workload does not have.

Create:

  zhao_texture_bilerp_sched

Keep zhao_texture_bilerp and zhao_texture_bilerp_lane as exact oracles.

13.2 One exact multiplier structure
-----------------------------------

A channel job requires three products:

  p0 = (t10 - t00) * fu
  p1 = (t11 - t01) * fu
  p2 = (b - a) * fv

Use one signed 18 x 9 multiplier structure and a three-step microsequence:

  B0  capture job from FIFO
  B1  issue p0
  B2  capture p0, issue p1
  B3  capture p1, form exact a/b, issue p2
  B4  capture p2, perform the one final round, emit result

The next job begins every three multiplier launches. Latency may be five clocks;
II is three channel jobs.

No intermediate rounding is added. a and b remain exact signed 18-bit values.
The only rounding remains `(s_w + 32768) >>> 16`.

13.3 Input and output queues
----------------------------

An eight-entry input FIFO carries:

  sample_tag
  channel id
  t00/t10/t01/t11
  fu/fv

This is one of the few queues where carrying the small arithmetic payload may be
cheaper than an extra RAM read. Fit token-indirected and direct-payload forms if
area is close; do not assume.

Output is a two-entry held skid into FILTER_ROB.

13.4 Replication trigger
------------------------

Keep one multiplier unless a committed legal trace shows either:

  filtered_channel_jobs > 300,000 per design frame;
  input FIFO full > 2% of texture-island clocks;
  bilerp queue causes cache/class backpressure > 2%;
  a shipping material requires a larger non-droppable filtered workload.

Then instantiate a second identical lane and hash/round-robin jobs by token.
Do not redesign the arithmetic.

13.5 Resource and timing gate
-----------------------------

One-lane target:

  <= 250 ALMs
  <= 200 registers
  <= 1 M10K
  exactly 1 DSP structure / fitter DSP count within the proved width budget
  II = 3 channel jobs
  >= 125 MHz

The inherited 408-job exact comparison, corner vectors and randomized output
stalls remain mandatory. Add a frame-rate test proving 180,000 jobs complete
inside 540,000 issue clocks.


14. MOSAIC: RETURN FOUR DSPs WITHOUT CHANGING ONE HASH BIT
=========================================================

14.1 Frozen law, replaceable implementation
-------------------------------------------

The exact law remains:

  tx = arithmetic_floor(u * 64), represented by u_raw >>> 10
  tz = arithmetic_floor(v * 64), represented by v_raw >>> 10

  hx = uint32(tx) * 73856093 mod 2^32
  hz = uint32(tz) * 19349663 mod 2^32
  h  = hx XOR hz
  p  = h mod 255
  pick = p < weight ? matA : matB

The two current constant multipliers consume four DSP blocks and close at
86.63 MHz. Constant multiplication is not entitled to DSP merely because `*`
expresses it compactly.

Create:

  zhao_texture_mosaic_csd

Keep zhao_texture_mosaic as the exact oracle.

14.2 Exact signed-digit decompositions
--------------------------------------

Modulo 2^32, with every intermediate kept at 32 bits:

  73856093 * x =
      +(x <<  0)
      -(x <<  2)
      -(x <<  5)
      +(x <<  7)
      +(x << 10)
      -(x << 12)
      -(x << 16)
      +(x << 19)
      -(x << 21)
      +(x << 23)
      +(x << 26)

  19349663 * x =
      -(x <<  0)
      +(x <<  5)
      +(x <<  7)
      +(x << 14)
      -(x << 16)
      +(x << 19)
      +(x << 21)
      +(x << 24)

Addition/subtraction modulo 2^32 is associative. Truncating every registered
stage to 32 bits therefore preserves the frozen uint32 result exactly.

14.3 Balanced pipeline
----------------------

Do not create an eleven-adder serial chain.

Suggested form:

  H0  capture tx/tz and folded texel outputs

  H1  form groups of two/three shifted signed terms in parallel

  H2  combine group sums in a balanced tree

  H3  form final hx/hz modulo 2^32; register XOR h

  H4  exact mod-255 byte-fold stage 1

  H5  mod-255 final fold + compare/select

  H6  held output register

II remains one terrain candidate per clock.

14.4 Alternative packed-DSP experiment
--------------------------------------

A vendor-specific constant-multiply implementation may be compared, but the
architecture does not depend on it. Quartus Lite has already demonstrated that
plausible multiplier packing assumptions are unsafe.

The CSD variant is the required 0-DSP baseline. A DSP variant survives only if:

  - it is explicitly instantiated rather than inferred by wish;
  - the fitter reports fewer resources or materially better composition timing;
  - the exact whole-domain/random differential remains green;
  - the saved ALMs are worth the DSPs in the full console budget.

14.5 Resource and timing gate
-----------------------------

Target:

  <= 500 ALMs
  <= 350 registers
  0 M10K
  0 DSP
  II = 1
  >= 125 MHz

Hard gate: exhaustive mirrored-fold test and broad full-domain randomized hash
comparison against the frozen oracle, including signed-negative coordinates and
uint32 wrap rails.


15. MATERIAL.COMBINE.V1
=======================

15.1 The arithmetic is now frozen
---------------------------------

Create:

  zhao_texture_material_combine_v1

The old TEXJOIN placeholder is no longer acceptable. The native recipes are:

  unit_mul8(a,b)   = (a*b + 128) >> 8
  modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)
  lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w, 8))

  0 PASSTHRU                 0 or 1 sample
  1 MODULATE                 2 samples
  2 MODULATE2X               2 samples
  3 LERP                     2 samples
  4 ADD_SAT                  2 samples
  5 MASK                     2 samples
  6 TERRAIN_DETAIL_LIGHT     3 samples
  7 TERRAIN_DETAIL_MASK      3 samples

Sample 0 owns base alpha except where the recipe names a mask. Palette index is
sample0.index. Status is ORed over required samples.

15.2 Do not build six always-live multipliers
---------------------------------------------

The TMU supplies at most one sample per clock. Therefore:

  two-sample recipes can complete no faster than one fragment per two sample
  clocks;

  three-sample recipes can complete no faster than one fragment per three
  sample clocks.

A fully parallel three-RGB-product stage followed by another three-product stage
spends six byte multipliers to serve a stream whose own sample supply does not
need them.

Use two 9-bit signed/unsigned product lanes and tokenized microjobs.

15.3 Combiner record and scheduler
----------------------------------

COMBINE_FIFO supplies frag_tag. The combiner reads sample0/1/2 and recipe data
into a small local record. Depth 8 is sufficient initially.

Each record contains:

  recipe
  weight
  sample RGB/A/index/status
  intermediate RGB/A
  required microjob count
  completed microjob mask

Fast bypasses:

  PASSTHRU      no multiplier
  ADD_SAT       no multiplier

Product jobs:

  MODULATE      3 RGB products
  MODULATE2X    3 RGB products
  LERP          3 signed difference x weight products
  MASK          1 alpha product
  DETAIL_LIGHT  3 first-layer RGB products + 3 second-layer RGB products
  DETAIL_MASK   3 first-layer RGB products + 1 alpha product

Two product lanes accept two jobs per clock. Continuation jobs are queued after
first-layer results; no table-wide scan.

15.4 Capacity
-------------

Worst named three-sample terrain recipe DETAIL_LIGHT requires six byte products
per fragment:

  276,480 fragments x 6 = 1,658,880 byte products

Two lanes provide:

  1,333,333 clocks x 2 = 2,666,666 byte-product slots

The named terrain load therefore uses about 62% of combiner product capacity,
before counting other multiplying recipes. Passthrough sky/stars and additive
paths consume no product jobs.

Counters must record actual product jobs by recipe. If the complete stress trace
exceeds 80% capacity, first prove whether optional material degradation removes
it. A third lane is a last resort, not the default.

15.5 Product implementation variants
------------------------------------

Build and fit two exact variants behind one interface:

  A. LOGIC2
     two exact 9x9/9x8 multipliers in ALM logic;
     zero DSP;
     preferred if <= 800 ALMs and >= 125 MHz.

  B. DSP2_PACKED_OR_EXPLICIT
     explicit vendor primitive/IP only;
     maximum two DSP blocks for the whole combiner;
     accepted only if the fitter proves the count and composition improves.

Do not write six independent `*` operators and assume they pack.

15.6 Pipeline and retirement
----------------------------

C0  read/capture fragment samples and recipe.

C1  emit up to two initial product microjobs or complete bypass.

C2+ capture product results, round exactly, enqueue continuations.

C3  second-layer jobs for DETAIL_LIGHT / DETAIL_MASK.

C4  assemble final RGB/A/index/status.

C5  write FINAL_RESULT[slot] and emit final-ready event.

The combiner may finish fragments out of allocation order. FRAGROB performs final
ordered retirement.

15.7 Resource and timing gate
-----------------------------

Target:

  <= 650 ALMs
  <= 500 registers
  1-4 M10Ks
  0 DSP preferred, 2 hard ceiling
  product issue capacity = 2 jobs/clock
  bypass acceptance = 1 fragment/clock
  >= 125 MHz

Every recipe receives exhaustive 8-bit rail vectors plus randomized differential
against one generated scalar reference. Mutate rounding constants, shift count,
saturation, alpha ownership, recipe count and sample order; every mutation must
be caught.


16. AUX.V2
==========

16.1 Keep the exact divide, replace the unsafe shell
----------------------------------------------------

Create:

  zhao_texture_aux_pipe_v2

Keep zhao_texture_aux_div6 unchanged unless a differential proves a defect. Keep
the recently added A0/A0b input boundary.

The production rewrite removes:

  - unconditional req_ready=1 without downstream credit;
  - a single A7 register that can be overwritten while sheet_ready is low;
  - side-channel identity derived from a wrapping write pointer;
  - copied wide return storage;
  - generic RGB/A AUX fields in TEXJOIN.

Actual AUX result remains the restricted `{tag8, strength8, status}` sideband.
It is not a general RGB texture.

16.2 AUX binding table
----------------------

Patch/material-constant envelope data must not be repeated in every fragment.

An AUX binding record contains:

  generation8
  resident
  env_x0, env_x1
  env_z0, env_z1
  surface-sheet handle/base
  any exact addressing flags

FRAGROB stores only world_x/world_z and AUX binding slot+generation.

AUX A0 reads the binding synchronously and captures it before subtraction.

16.3 Fixed divide pipeline
--------------------------

Stages:

  A0  capture token, world x/z, issue binding read

  A1  capture binding; generation/residency check; subtract x0/z0 and ranges

  A2  shift numerators; classify negative, saturated, degenerate

  A3-A8  exact six restoring quotient bits, U and V in parallel

  A9  form sheet coordinate and push SHEET_REQ_FIFO, or push degenerate result

The exact arithmetic remains the old block’s oracle.

16.4 Bounded flow control
-------------------------

Accepted requests reserve one final AUX-result credit.

SHEET_REQ_FIFO depth starts at 8. It holds token, sheet handle and u/v. A stalled
surface sheet cannot overwrite the next divider result.

Sheet responses are already tokenized and push AUX_RESULT_FIFO depth 8/16.
Degenerate results use a separate two-entry skid and merge by token; they need not
wait behind a memory request merely to preserve order, because FRAGROB reorders by
identity.

req_ready is based on:

  binding/read pipeline credit
  divider pipeline credit
  sheet-request/result credits

It is never constant unless formal capacity proves every downstream stall bound.

16.5 Width work
---------------

Do not narrow NUM_W/DEN_W from intuition. First derive envelope and world-position
bounds from the surface-sheet contract and assert them in zref and RTL. If the
range proof reduces 40/32-bit arithmetic, implement and differential-test it as a
separate commit.

16.6 Resource and timing gate
-----------------------------

Target:

  <= 550 ALMs
  <= 500 registers
  1-3 M10Ks
  0 DSP
  II = 1 credited request/clock
  >= 125 MHz

Required adversarial cases:

  sheet_ready low longer than divider latency;
  output ready low with continuous requests;
  simultaneous sheet and degenerate completion;
  stale AUX binding;
  token wrap;
  saturated U only, V only and both;
  degenerate envelope;
  randomized response delay and order;
  no overwrite, drop or duplicate.


17. SAMPLE AND MATERIAL CAPACITY MODEL
======================================

17.1 Distinct units
-------------------

Never combine these counters:

  pre_z_covered_fragments
  post_z_survivors
  reciprocal_requests
  perspective_pair_jobs
  primary_samples_issued
  cache_lane_lookups
  cache_line_fills
  filtered_channel_jobs
  material_product_jobs
  aux_requests

276,480 is pre-Early-Z covered fragments in Z60 at 3x overdraw. It is not any of
the other units.

17.2 Conservative sample envelope
---------------------------------

Three-sample terrain profile:

  terrain primary invocations     829,440
  sky                              92,160
  stars                           128,000
  clouds                           45,000
  ---------------------------------------
  pre-Z no-rejection envelope   1,094,600 samples

At 1,333,333 design clocks, an II=1 perspective/TMU/cache path has 238,733 sample
slots remaining. This is a stress envelope, not a measured real frame.

17.3 Reciprocal capacity
------------------------

RCP is per fragment, not per sample.

One lane at II=4:

  333,333 reciprocals/design frame

against a 320,000 pre-Z upper target and a lower post-Z stream. Retain one lane,
measure, and admit a second only from survivor traces.

17.4 Bilerp capacity
--------------------

One channel job every three clocks:

  444,444 channel jobs/design frame

against roughly 180,000 named jobs. This is the explicit reason one DSP is the
baseline.

17.5 Material-combiner capacity
-------------------------------

Two byte products per clock:

  2,666,666 products/design frame

The 276,480-fragment DETAIL_LIGHT terrain case consumes 1,658,880 RGB products.
Actual recipe mix decides the remainder.

17.6 Cache bandwidth
--------------------

Hit-path arithmetic capacity is one sample/clock. Miss bandwidth is independent
and must be measured as:

  physical line fills x 16 bytes
  fill busy clocks
  same-line multicast saving
  class queue stalls

The first production cache keeps one blocking miss. If legal traces show fill
busy >15% of texture-island clocks while SDRAM has service capacity, evaluate one
MSHR. Do not add one before that result.

17.7 Material degradation
-------------------------

Three-sample capability is shipping functionality. It is not a promise to apply
three samples to every visible terrain fragment.

The pre-seal ladder remains:

  3 samples  base + detail + light/mask
  2 samples  base + higher-priority optional layer
  1 sample   base
  0 samples  Gouraud/microform/glint fallback where declared

No in-flight hardware silently drops a sample from a sealed recipe.


18. DSP RECOVERY STRATEGY
=========================

18.1 Current prototype texture DSPs
-----------------------------------

  RCP24 service        6
  PERSPUV service      3
  bilerp lane          3
  Mosaic               4
  ----------------------
  total               16

18.2 Production target
----------------------

  RCP24 32x32 lane          3-4
  perspective U/V lanes       6
  bilerp serial channel       1
  Mosaic CSD                  0
  material combiner         0-2
  --------------------------------
  target                  10-13
  hard ceiling              14

This simultaneously increases perspective throughput from one pair per two
clocks to one pair per clock.

18.3 Order of DSP experiments
-----------------------------

  1. Prove RCP operand widths and fit one 32x32 expression.
  2. Build the straightforward 6-DSP perspective pair pipe.
  3. Replace Mosaic with 0-DSP CSD.
  4. Replace bilerp with one 18x9 product structure.
  5. Fit material combiner in 0-DSP logic and explicit <=2-DSP variants.
  6. Only then consider packing or decomposition inside perspective.

Do not optimize the only unit whose extra DSPs buy required throughput before
removing DSPs from constant and overprovisioned work.

18.4 No global multiplier sharing
---------------------------------

Do not share the RCP, perspective, bilerp, Mosaic or material multipliers across
subsystems merely to reduce a count. They have different rates and may run
concurrently. A global arithmetic arbiter would add refusal, starvation and
routing paths in the hottest island.

Share only within one bounded service where the schedule and capacity are proved.


19. ALM, REGISTER AND M10K RECOVERY STRATEGY
============================================

19.1 Largest recovery: cache
----------------------------

Current cache_pipe:

  5,634 ALMs
  10,812 registers
  3 M10Ks

Target cache_v2:

  approximately 900 ALMs
  approximately 900 registers
  8-10 M10Ks

The expected recovery is several thousand ALMs and roughly ten thousand
registers from one block. This is why cache_v2 is the first substantial rewrite.

19.2 Second recovery: TEXJOIN/FRAGROB
-------------------------------------

Current TEXJOIN v2:

  3,465 ALMs
  6,143 registers
  3 M10Ks

The production split removes:

  64-entry multiwrite work FIFO;
  three-wide U/V/binding storage in flops;
  sample colours in flops;
  wide context in flops;
  table-wide issue scans;
  combinational retirement readout.

Target FRAGROB:

  approximately 900 ALMs
  approximately 1,200 registers
  14-20 M10Ks

Do not judge this by bit utilization. Judge it by recovered routing and full-
island fit.

19.3 Fixed pipelines instead of scheduler tables
------------------------------------------------

PERSPUV’s 16-entry scheduler table is unnecessary after RCP. Replacing it with a
fixed pipe trades table/control ALMs for six intentional DSPs.

RCP keeps contexts because its operations are dependent and variable by token,
but queues replace scans.

AUX keeps a fixed pipeline and bounded queues; its repeated envelope data moves
to a binding table.

19.4 Token indirection instead of payload copying
-------------------------------------------------

A 14-bit sample token copied through four queues costs 56 queue bits per depth
position.

A 64-bit footprint plus 30-80 bits of plan copied through those queues costs
hundreds. The payload already has a natural direct-index home: slot + sample
index.

The production rule is:

  put the payload in one bank;
  move the token.

19.5 M10K is not the enemy
--------------------------

The provisional device exposes 553 M10Ks. The texture island’s expected 32-56 is
roughly 6-10% of that device.

This does not mean M10K is infinite. Terrain, framebuffer support, Field tables,
post buffers and other blocks also consume it. It means that converting a cache
or transaction store from 10,000 flops into ten memories is exactly the trade the
fabric was built to make.

Every full-console fit must still report total M10K and shape pressure.

19.6 Do not hide state in MLAB accidentally
-------------------------------------------

Quartus may implement small arrays in MLAB/logic. That can be correct but may
consume ALMs the budget expects to recover.

For each payload bank, record an expected implementation:

  MUST_M10K
  MAY_MLAB
  MUST_FLOPS

The fitter report must verify it. If a MUST_M10K bank lands in registers, the fit
fails structurally even if the MHz number passes.


20. PHYSICAL FIT HARNESS: STOP TIMING VIRTUAL PINS
==================================================

20.1 Why the existing leaf top is insufficient
----------------------------------------------

The current leaves expose 132-829 virtual pins. RCP’s approximately six
nanoseconds of clock skew into an output boundary is the clearest warning.

A leaf fit still has value when it exposes a 15 ns internal cone. It is a poor
final measurement of a registered subsystem boundary.

20.2 Create registered fit wrappers
-----------------------------------

For every major production leaf and island, create a physical wrapper:

  zhao_fit_<name>_top

External ports should be small:

  clock
  reset
  seed/control
  final signature32/64
  status/fault

Inside the wrapper:

  deterministic LFSR or ROM stimulus
    -> registered ingress packet
    -> DUT
    -> registered sink / rolling CRC

Every functional output must influence the sink signature so the fitter cannot
remove the datapath. Every input field must vary over a nontrivial sequence.

The wrapper’s ingress and egress registers are the architectural boundary that
the composed machine will also have.

20.3 Path reporting
-------------------

Report separately:

  wrapper source -> DUT ingress
  DUT internal paths
  DUT egress -> wrapper sink
  wrapper-only paths

The acceptance number is the worst path involving the DUT, with no unconstrained
clock or port.

20.4 Target-period fitting
--------------------------

Run explicit periods:

  8.000 ns   125 MHz
  8.333 ns   120 MHz
  8.696 ns   115 MHz
  9.091 ns   110 MHz
  9.524 ns   105 MHz
 10.000 ns   100 MHz

A derived Fmax number remains useful, but the gate is zero negative slack and
zero TNS at the named target period.

20.5 Seed policy
----------------

During an architectural edit:

  one fixed seed for before/after path comparison.

When the block first passes its target:

  seeds 1, 2 and 3;
  report minimum, median, maximum;
  do not select only the lucky seed.

For the composed texture island and full composition, add at least two more
seeds if the minimum is within 2 MHz of the acceptance floor.

20.6 Provenance
---------------

Every fit bundle records:

  exact commit
  dirty/clean status
  source closure and hashes
  module parameters
  Quartus version
  device
  seed
  target period
  fitter settings read back from Quartus
  ALM/register/M10K/DSP counts
  inferred-memory table
  worst 100 setup paths
  hold report
  virtual-pin count
  wrapper version

A timeout, contaminated run or source-closure failure receives no Fmax.

20.7 Resource-structure assertions
----------------------------------

The fit script should support per-target rules from design/fit_targets.yml:

  min_m10k
  max_m10k
  max_registers
  max_alms
  max_dsp
  required_instance_pattern
  forbidden_path_pattern

Examples:

  cache_v2 min_m10k 8
  mosaic_csd max_dsp 0
  rcp24_sched_v2 max_dsp 4
  fragrob forbidden_path live-entry priority scan

The exact schema is implementation detail; the enforcement is not.


21. COMPOSITION BOUNDARIES
==========================

21.1 Island A: transaction and perspective front
-------------------------------------------------

Composition:

  FRAGROB allocation
  RCP24.SCHED.V2
  SAMPLE.EXPANDER
  PERSPECTIVE_PAIR.PIPE

Registered ingress is a realistic post-Early-Z survivor packet. Registered egress
is a sample packet for TMU planning.

Gate:

  >= 120 MHz, target 125
  one fragment allocation/clock burst
  one perspective pair/clock after fill
  no token loss under random stalls
  RCP II=4 or better
  sample expansion of 0/1/2/3 correctly
  <= 2,700 ALMs excluding reusable fit wrapper
  <= 10 DSP

21.2 Island B: texture plan and cache
------------------------------------

Composition:

  binding table
  TMU.PLAN.V2
  CACHE.V2
  local response FIFO

Gate:

  >= 120 MHz, target 125
  one hit sample/clock
  expected M10K inference
  no ready chain across the island
  exact address stream against oracle
  one fill for same-line four-tap footprint
  <= 2,000 ALMs
  0 DSP

21.3 Island C: decode and sample completion
-------------------------------------------

Composition:

  class router
  nearest
  CLUT/palette
  bilerp scheduler/FILTER_ROB
  sample completion merge

Gate:

  >= 120 MHz
  cache-response acceptance one/clock on sustainable class mixes
  exact inherited sample output
  no class starvation
  one sample result commit/clock average
  <= 1,600 ALMs
  <= 3 DSP, target 1-2

21.4 Island D: AUX
------------------

Composition:

  AUX binding read
  AUX divide pipe
  surface-sheet request/response model
  AUX return to FRAGROB

Gate:

  >= 120 MHz
  II=1 credited requests
  no overwrite under sheet/output stalls
  <= 800 ALMs
  0 DSP

21.5 Island E: material completion
----------------------------------

Composition:

  FRAGROB result banks
  material combiner
  final result RAM
  ordered retirement

Gate:

  >= 120 MHz
  all eight recipes
  bypass one fragment/clock
  two byte products/clock
  output held under stall
  <= 1,700 ALMs for FRAGROB retirement + combiner slice

21.6 Full texture-survivor composition
--------------------------------------

Composition:

  complete topology in section 4
  realistic local cache/fill memory model
  realistic random backpressure
  0-, 1-, 2- and 3-sample materials
  AUX concurrently

Gate:

  115 MHz minimum architecture target
  120 MHz preferred
  three seeds
  target resources <= 6,600 ALMs, <= 8,000 regs, <= 56 M10Ks, <= 13 DSP
  hard resources <= 7,500 ALMs, <= 9,000 regs, <= 64 M10Ks, <= 14 DSP

Only after this gate passes is the island allowed into reduced-renderer
composition.

21.7 Reduced renderer + texture-survivor
----------------------------------------

Integrate:

  existing 96.87/99.50 reduced renderer
  one registered survivor boundary
  full texture-survivor island
  RASTER.FRAGMENT final packet

Gate hierarchy:

  >= 105 MHz hard acceptance
  >= 110 MHz objective
  >= 115 MHz stretch
  no pixel/capture drift except versioned material additions
  no throughput degradation on the sealed stress traces

If the first result is below 105, read actual paths before modifying either
side. Do not assume the slower isolated leaf remains the composed limiter.


22. VERIFICATION PLAN
=====================

22.1 Preserve old implementations as oracles
--------------------------------------------

  RCP24 old block             exact reciprocal oracle
  PERSPUV old block           exact UV arithmetic oracle
  TMU pipe / zref::Tmu        exact address/decode oracle
  texture cache old block     line/fill/invalidate oracle
  bilerp old block            exact filter oracle
  Mosaic old block            exact hash/fold oracle
  TEXJOIN v2                  token/retirement behavior oracle
  material scalar reference   exact recipe oracle
  AUX old block               exact coordinate/quotient oracle

New blocks live beside them until composition passes.

22.2 Unit differentials
-----------------------

RCP:

  exhaustive/hash-equivalent mantissa result;
  every exponent boundary;
  zero;
  randomized token order and output stalls;
  exactly four product launches/request;
  stale/abort handling.

Perspective:

  exact against old block;
  positive/negative rails;
  every exponent;
  depth-zero;
  simultaneous U/V;
  output backpressure credit;
  II=1 pair stream.

Planner:

  inherited formats/wrap/mips;
  CLUT included;
  binding generation;
  dimension overflow;
  exact four addresses/enables/class/fractions.

Cache:

  cold miss then hit;
  four taps one line = one fill;
  four taps four lines = four fills;
  invalidation beats fill;
  epoch wrap sweep;
  randomized hit/miss/replay;
  output stalls;
  first-look counters;
  M10K inference gate.

Palette:

  inactive load while old active reads;
  atomic commit;
  failed CRC;
  aborted load;
  stale generation;
  duplicate/missing writes;
  same-edge lookup/commit law.

Bilerp:

  exact old arithmetic;
  corners fu/fv 0/255;
  one multiplier launch schedule;
  no intermediate rounding;
  sustained 180k workload.

Mosaic:

  exact modulo-2^32 products;
  negative coordinate rails;
  full mirrored fold;
  randomized hash;
  0 DSP structural gate.

Combiner:

  all recipes;
  all count mismatch errors;
  alpha/index/status ownership;
  two product lanes;
  pipeline stalls;
  exhaustive channel rails.

AUX:

  exact old arithmetic;
  saturation/negative/degenerate;
  sheet stalls longer than divider latency;
  out-of-order token returns;
  no result loss.

22.3 Transaction torture
------------------------

Run at least 100,000 accepted fragments with randomized:

  sample_count 0..3;
  recipe legal for count;
  repeated external source IDs;
  TMU response delay;
  class mix;
  cache misses;
  palette generations;
  AUX delay;
  output stalls;
  same-cycle accept and retire;
  same-cycle sample and AUX completion;
  frame abort/restart.

Assert:

  outstanding slots never exceed 16;
  every accepted fragment retires once or belongs to an explicitly aborted
  frame;
  allocation order equals retirement order;
  no sample writes the wrong slot/index;
  no slot frees early;
  no result changes while valid&&!ready;
  no queue overflows;
  all counters reconcile.

22.4 Mutation requirements
---------------------------

At minimum, prove tests catch:

  separate free-count increment/decrement race;
  generation narrowed to two/four bits;
  source_id substituted for token;
  sample index bit swapped;
  duplicate return accepted;
  output packet not held;
  cache combinational read / missing capture;
  same-line multicast disabled;
  palette bank flipped before CRC;
  perspective shift off by one;
  one U/V product omitted;
  bilerp extra rounding;
  Mosaic constant or sign changed;
  combiner +127/+64/+128 bias mutation;
  AUX side channel shifted by one token.

A new test is not trusted until at least one relevant mutation makes it red.

22.5 Stress traces
------------------

Required named traces:

  one_sample_terrain
  two_sample_detail_terrain
  sacrifice_terrain_3sample
  creature_atlas_thrash
  mixed_clut_direct_bilinear
  palette_reload_inflight
  aux_surface_storm
  duo_view_union
  output_backpressure_burst
  cache_same_line_multicast
  cache_four_distinct_lines

For each record:

  pre-Z covered fragments
  post-Z survivors
  reciprocal requests
  sample requests
  class counts
  filtered channel jobs
  material product jobs
  cache lane hits/misses
  physical fills
  AUX requests
  queue high-water marks
  stall clocks
  total clocks


23. BUILD AND MIGRATION ORDER
=============================

Phase 0 — freeze evidence
-------------------------

  - Preserve the complete ten-row first-pass fit report.
  - Mark each repaired leaf result as BEFORE until refitted.
  - Preserve current v1/v2 blocks and tests.
  - Merge or otherwise protect the hardware branch before more parallel edits.

Phase 1 — physical apparatus
----------------------------

  - Add registered fit wrappers.
  - Add target-period support and resource minima/maxima.
  - Complete the currently launched AUX/TMU-plan/TEXJOIN/RCP refits.
  - Do not let those interim improvements become permission to skip v3.

Phase 2 — cache_v2
-----------------

  - Implement static four-lane data/tag M10K banks.
  - Implement C0-C4.
  - Keep one blocking miss and multicast.
  - Fit LINES=32/64.
  - Stop if fewer than eight M10Ks infer or registers exceed 2,000.

Phase 3 — FRAGROB skeleton
--------------------------

  - Token types and free FIFO.
  - Control flops.
  - Banked sample descriptors/results.
  - Context/final RAM.
  - Allocation, abort and ordered retirement.
  - No arithmetic yet; loopback completion model.
  - Fit before adding services.

Phase 4 — RCP and sample expander
--------------------------------

  - Build scan-free RCP scheduler with 32x32 product.
  - Pair differential and seed fit.
  - Add RCP request/result FIFOs.
  - Add sample expander and descriptor reads.
  - Compose Island A without perspective arithmetic first.

Phase 5 — perspective pair
--------------------------

  - Build two-product fixed pipe.
  - Preserve exact arithmetic.
  - Compose with FRAGROB/RCP.
  - Require one pair/clock and >=120 MHz.

Phase 6 — binding/planner
-------------------------

  - Define generated binding and mip descriptor records.
  - Build atomic loader and table.
  - Replace dynamic per-sample static arithmetic.
  - Differential against full inherited plan suite.
  - Compose with cache_v2.

Phase 7 — decode path
---------------------

  - Token/class queues.
  - sample-plan RAM.
  - nearest path.
  - palette_v2.
  - bilerp_sched + FILTER_ROB.
  - one-port sample completion merge.

Phase 8 — Mosaic and material combiner
--------------------------------------

  - CSD Mosaic, 0 DSP.
  - exact combiner scalar reference.
  - logic and explicit packed-DSP variants.
  - all eight recipes.
  - combine-ready/final-result integration.

Phase 9 — AUX v2
----------------

  - AUX binding table.
  - credit-safe pipe and queues.
  - actual tag/strength sideband.
  - concurrent integration with primary texture path.

Phase 10 — complete texture-survivor island
-------------------------------------------

  - Run named traces and torture.
  - Three-seed 115-120 MHz fit.
  - Enforce total ALM/reg/M10K/DSP gates.
  - No terrain/Field expansion until this phase has a verdict.

Phase 11 — renderer composition
-------------------------------

  - Integrate at registered survivor/final-fragment boundaries.
  - Fit 105/110/115 hierarchy.
  - Read paths.
  - Rearchitect only measured cross-island offenders.


24. KEEP / REWRITE / DEFER TABLE
================================

KEEP AS-IS OR AS ORACLE
-----------------------

  reciprocal mathematical law and ROM
  old RCP block and exhaustive evidence
  old PERSPUV arithmetic
  TMU format/address semantics
  old texture cache semantics
  same-line fill multicast concept
  exact bilerp arithmetic and single rounding
  Mosaic hash/fold law
  AUX six-step quotient law
  material recipes R9
  8-bit generation ruling
  current fit source/provenance tooling
  current counter-race and stall tests

KEEP THE IDEA, REIMPLEMENT PHYSICALLY
------------------------------------

  TEXJOIN token allocation and ordered retirement
  RCP multi-context scheduling
  elastic TMU planning
  resident palettes
  response-class decoupling
  one bilerp channel engine
  blocking cache miss policy
  AUX parallel issue

REWRITE BEFORE INTEGRATION
--------------------------

  current cache_pipe storage and hit path
  current TEXJOIN wide storage organization
  current TEXJOIN 64-entry work queue as final sample scheduler
  current PERSPUV token table and one-product lane
  current RCP scans and 32x64 product expression
  current wide response dispatcher queues
  current palette load protocol
  current AUX side-channel/flow shell
  current Mosaic inferred multipliers
  current TEXJOIN placeholder combiner

DEFER UNTIL TRACE
-----------------

  cache MSHR / hit-under-miss
  second bilerp lane
  second RCP multiplier lane
  more than 8 resident palettes
  more than 32/64 texture bindings
  UV alias optimization
  per-class cache-response pre-sorting beyond local queues
  perspective multiplier decomposition below the straightforward 6-DSP pipe

REJECT
------

  integrating the ten first-pass leaves and accepting a 55-80 MHz console;
  relaxing product clock or material capability before rearchitecture;
  dropping three-sample capability;
  building a second general TMU;
  keeping cache lines in registers;
  hiding paths with multicycle/false constraints;
  using a lucky seed as architecture;
  assuming DSP packing from source syntax;
  adding terrain/Field RTL faster than the texture fit can be closed.


25. STOP / GO DECISION TREE
===========================

After repaired interim leaf refits:

  If AUX and narrowed planner rise above 110:
    keep their local fixes as evidence; still migrate to binding/credit v2.

  If TEXJOIN work-FIFO version remains below 100:
    do not iterate that monolith; proceed directly to FRAGROB split.

  If RCP varies widely by seed and internal data paths pass 10 ns:
    blame leaf boundary first; validate in registered Island A.

  If RCP remains below 110 on internal paths:
    implement scan-free queues and 32x32 product before any algorithm change.

After cache_v2:

  If M10K >=8, registers <=2,000 and Fmax >=120:
    proceed.

  If M10K does not infer:
    stop. Fix memory coding style. Do not optimize logic around flops.

  If hit path passes but fill path fails:
    pipeline fill bookkeeping; do not add MSHR.

After Island A:

  If perspective pair fails:
    inspect whether multiplier output, bias add, barrel shift or saturation owns
    the path; each should be a separate stage. No third product lane.

  If RCP owns the path only at egress:
    validate wrapper/composition clock placement before arithmetic changes.

After Island B:

  If planner owns:
    move more level metadata into precomputed descriptor RAM.

  If cache owns:
    verify M10K capture and replay; no associativity.

After full texture-survivor fit:

  >=120 MHz:
    excellent; integrate with renderer.

  115-119.99 MHz:
    passes architecture target; integrate and preserve report.

  110-114.99 MHz:
    inspect one composition pass. Do not declare failure, but do not spend all
    margin without identifying the seam.

  105-109.99 MHz:
    below texture-island target; rearchitect measured seam before full shell.

  <105 MHz:
    hard fail. Do not lower the product clock or delete capability.

After full renderer composition:

  >=115 MHz:
    stretch success.

  110-114.99 MHz:
    architecture objective met.

  105-109.99 MHz:
    accepted provisional full composition with 5-10% product margin; continue
    board closure and seed robustness.

  100-104.99 MHz:
    below acceptance; one measured closure pass required.

  <100 MHz:
    product failure; read cross-island paths and rearchitect.


26. EXACT FIRST IMPLEMENTATION COMMITS
======================================

The following commit sequence is recommended so every step has one falsifiable
claim.

C1  FIT WRAPPERS: registered ingress/egress and signature sink
    Claim: virtual output pins no longer define DUT timing.

C2  FIT TARGET RULES: min/max M10K/ALM/reg/DSP
    Claim: a cache implemented as flops cannot be reported as passing.

C3  CACHE V2 STORAGE: four static data banks + four static tag banks
    Claim: expected M10K inference, no behavior yet beyond read/write harness.

C4  CACHE V2 HIT PIPE C0-C4
    Claim: one hit/clock, exact data, captured M10K output.

C5  CACHE V2 BLOCKING FILL + MULTICAST
    Claim: same line fetched once, distinct lines separately, exact counters.

C6  FRAGROB TOKEN/FREE FIFO/ABORT
    Claim: bounded allocation, gen8, no scan, no wide payload yet.

C7  FRAGROB BANKED DESCRIPTORS/RESULTS
    Claim: three descriptors accepted in one clock and three results read for
           combine without flop arrays.

C8  FRAGROB ORDERED RETIRE
    Claim: out-of-order mock completion, in-order held output.

C9  RCP SCHED V2 CONTROL QUEUES
    Claim: same four microjobs, no context scans.

C10 RCP 32x32 WIDTH
    Claim: exact whole-domain/hash behavior, DSP <=4.

C11 SAMPLE EXPANDER
    Claim: one RCP result produces exactly count 0..3 jobs; no 64-entry work
           queue.

C12 PERSPECTIVE PAIR PIPE
    Claim: one pair/clock, exact old arithmetic, six DSP maximum.

C13 ISLAND A COMPOSITION
    Claim: token-exact fragment-to-sample flow at >=120 MHz.

C14 BINDING TABLE + GENERATED LEVEL DESCRIPTORS
    Claim: static material work removed from sample path.

C15 TMU PLAN V2
    Claim: exact inherited addresses at II=1.

C16 ISLAND B COMPOSITION
    Claim: planner+cache >=120 MHz, expected RAM structure.

C17 PALETTE V2 PING-PONG TRANSACTION
    Claim: failed upload cannot tear active colors.

C18 NEAREST/CLUT CLASS PATHS
    Claim: one result/clock, token exact.

C19 BILERP SERIAL ONE-DSP
    Claim: exact result, II=3, 180k workload gate.

C20 FILTER ROB + SAMPLE COMMIT
    Claim: class completions converge without multiport result RAM.

C21 MOSAIC CSD
    Claim: exact hash, zero DSP.

C22 MATERIAL COMBINE SCALAR + PIPE
    Claim: all eight recipes exact, <=2 DSP.

C23 AUX BINDING + CREDITED PIPE V2
    Claim: sheet stalls cannot overwrite a request/result.

C24 FULL TEXTURE-SURVIVOR FUNCTIONAL TORTURE
    Claim: 100k fragments, every token once, all counters reconcile.

C25 FULL TEXTURE-SURVIVOR THREE-SEED FIT
    Claim: physical and resource gates.

C26 REDUCED RENDERER COMPOSITION
    Claim: full conventional renderer with texture path >=105 MHz.

No commit should combine a new arithmetic law, a scheduler rewrite, a fit-tool
change and a golden re-pin. Separate claims are how a negative result remains
usable.


27. KNOWN OPEN ITEMS THAT THIS SPEC DOES NOT INVENT
===================================================

The following still require measurement or an existing contract; this document
does not silently choose them:

  - final texture binding count (32 initial, 64 comparison only);
  - final resident palette slot count (8 preferred, 4 comparison);
  - exact legal range that may narrow AUX numerator/denominator;
  - actual post-Z survivor traces;
  - actual filtered channel-job traces for full creature battles;
  - cache fill bandwidth and whether one MSHR is ever justified;
  - exact full-fragment context width at the Early-Z/FRAGROB seam;
  - whether optional UV alias metadata earns its complexity;
  - whether explicit packed-DSP byte multipliers beat logic in the final fit;
  - final board device and I/O/PLL truth.

None blocks the baseline production island described here.


28. BROADER PROJECT STRATEGY
============================

28.1 The reduced renderer result stands
----------------------------------------

The 53.48 -> 96.87 mean / 99.50 best recovery remains valid. The texture result
does not reopen Fragment, Early-Z or Edgewalk by default.

The next architectural risk is the texture-survivor island. Fix it where it is.

28.2 Do not spend texture margin twice
--------------------------------------

A 115-120 MHz standalone texture island is not luxury. Composition with the
existing renderer, Field/Earth, memory arbitration and board clocking will spend
it.

Do not accept a 101 MHz island because the product clock is 100. That leaves no
composition or seed margin.

28.3 Terrain work remains important, but sequenced
--------------------------------------------------

The 8 km terrain residency, SW.STREAM and GEOM.PARAMBUF decisions remain binding
and useful. Their architecture should continue as contracts/reference/tests.

Substantial new terrain RTL waits until the texture island’s storage and packet
boundaries are stable, because terrain will feed those exact bindings, cache and
AUX interfaces.

28.4 Field is not redesigned in this pass
-----------------------------------------

Field has its own accepted configuration and measured workload. Do not use the
texture crisis as permission to rebuild it. The full composition will say whether
Field/Earth becomes the next physical limiter.

28.5 One console, not a museum of prototypes
--------------------------------------------

The project should retain old implementations as executable specifications, but
only one production instance of each function belongs in the composed machine.

A v1/v2/v3 source collection is healthy. Instantiating all of their queues and
records because each solved one local test is not.


29. ACCEPTANCE CHECKLIST
========================

Architecture
------------

[ ] Token allocated after Early-Z, before RCP/perspective/AUX.
[ ] One RCP per textured fragment, not per sample.
[ ] Zero to three sample descriptors supported.
[ ] U/V perspective pair starts every clock.
[ ] Static texture metadata resolved through binding tables.
[ ] Cache is four static synchronous data/tag banks.
[ ] Same-line fill multicast retained.
[ ] One blocking miss retained until traces.
[ ] Class queues carry tokens / one owned payload, not duplicated payloads.
[ ] Palette load is atomic and CRC-gated.
[ ] Bilerp starts with one exact multiplier and II=3 channel jobs.
[ ] Mosaic is zero-DSP exact shift/add.
[ ] Material combiner implements all eight recipes.
[ ] AUX returns tag/strength, not general RGB.
[ ] Fragment retirement remains allocation ordered.

Correctness
-----------

[ ] All old arithmetic differentials green.
[ ] All eight material recipes green.
[ ] 100k randomized transaction torture green.
[ ] Every new test red-validates at least one relevant mutation.
[ ] Same-clock accept/retire covered.
[ ] Same-clock primary/AUX completion covered.
[ ] Duplicate/stale/invalid returns covered.
[ ] Output hold under stall covered.
[ ] Abort and generation invalidation covered.
[ ] No plausible placeholder on malformed state.

Physical
--------

[ ] Registered fit wrapper used.
[ ] No unconstrained DUT clock.
[ ] Three seeds on stable major islands.
[ ] Cache M10K >=8.
[ ] Cache registers <=2,000.
[ ] Mosaic DSP ==0.
[ ] RCP DSP <=4.
[ ] Combiner DSP <=2.
[ ] Total texture DSP <=14.
[ ] Total texture ALM <=7,500.
[ ] Total texture registers <=9,000.
[ ] Total texture M10K <=64 unless full fit explicitly approves more.
[ ] Texture-survivor >=115 MHz.
[ ] Full renderer composition >=105 MHz.

Evidence
--------

[ ] Fit commit/source hashes recorded.
[ ] Dirty tree rejected or explicitly marked contaminated.
[ ] Target period and seed recorded.
[ ] Fitter settings read back.
[ ] Worst paths archived.
[ ] Memory inference archived.
[ ] Resource budgets machine-checked.
[ ] Workload counters saved with units.


30. FINAL RULING
================

The 54.95-110.90 MHz first-pass texture results are not a reason to reduce the
console. They are the evidence that prevents the console from being reduced by
accident later.

The first prototypes got the hard semantic ideas mostly right:

  token identity;
  exact old arithmetic;
  multi-sample fragments;
  local acceptance;
  same-line cache fills;
  resident palettes;
  independent decode classes;
  pipelined AUX and bilerp.

They got the physical ownership wrong:

  too many wide flop records;
  too many copied payload queues;
  too many table scans;
  too little real RAM;
  missing seam registers;
  static work repeated per sample;
  multipliers spent where workload did not require them.

The production solution is not to throw away three-sample terrain, lower the
clock, clone every unit, or accept a 55 MHz console.

It is to build the texture path as one island:

  one early token;
  one fragment record;
  one reciprocal per fragment;
  one UV pair per sample per clock;
  one binding read;
  one synchronous cache access;
  one class-owned decode path;
  one direct-index sample result;
  one exact material combination;
  one ordered final retirement.

The design target intentionally converts the prototype’s approximately 15.7k
ALMs / 25.1k registers / 16 DSPs / 11 M10Ks into approximately 5.5-6.6k ALMs /
6-8k registers / 10-13 DSPs / 32-56 M10Ks.

That is not merely optimization. It is the architecture that makes the promised
machine physically credible.


APPENDIX A — PACKET SUMMARY
===========================

A1. frag_tag
------------

  generation8
  slot4

A2. sample_tag
--------------

  generation8
  slot4
  sample_index2

A3. post-Early-Z allocation packet
----------------------------------

  fragment context
  invw24
  sample_count2
  recipe3
  recipe_weight8
  3 x {u_over_w32, v_over_w32, binding slot, binding gen8, lod8}
  AUX required / binding / world coordinates
  status

A4. RCP request/result
----------------------

  request  {frag_tag, invw24}
  result   {frag_tag, mantissa24, exponent6, zero/error}

A5. perspective job/result
--------------------------

  job      {sample_tag, uow32, vow32, mant24, k6, binding, lod, status}
  result   {sample_tag, u32, v32, binding, lod, saturation/status}

A6. cache request/result
------------------------

  request  {sample_tag, enable4, address[4]x32, class2}
  result   {sample_tag, raw_halfword[4], class2, status}

A7. decoded sample result
-------------------------

  sample_tag
  rgb888
  alpha8
  index8
  status

A8. AUX result
--------------

  frag_tag
  tag8
  strength8
  status

A9. final fragment result
-------------------------

  fragment context
  has_texture
  final rgb/alpha/index/status
  AUX tag/strength/status
  source/provenance retained in context


APPENDIX B — RESOURCE RECOVERY LEDGER
====================================

The numbers below are targets, not fit claims.

  block                    measured first pass      production target
  --------------------------------------------------------------------------
  cache                    5634 A /10812 R /3 M     900 A / 900 R /8-10 M
  TEXJOIN/FRAGROB          3465 A / 6143 R /3 M     900 A /1200 R /14-20 M
  PERSPUV                  1792 A / 2827 R /3 D     900 A / 700 R /6 D
  RCP24                    1041 A / 1101 R /6 D     650 A / 600 R /3-4 D
  planner                  1419 A / 1054 R           700 A / 500 R /6-10 M
  response dispatch         806 A / 1432 R           350 A / 400 R /3-6 M
  palette                   152 A /  141 R /2 M      250 A / 200 R /4-8 M
  bilerp                    125 A /  177 R /3 D      250 A / 200 R /1 D
  Mosaic                    197 A /  192 R /4 D      500 A / 350 R /0 D
  AUX                      1118 A / 1244 R /1 M      550 A / 500 R /1-3 M
  material combiner        absent                   650 A / 500 R /0-2 D

The target intentionally spends more M10K and a few more ALMs in Mosaic/palette
where doing so returns scarce DSPs and removes much larger ALM/register waste
elsewhere.


APPENDIX C — SOURCE EVIDENCE SNAPSHOT
=====================================

Primary repository evidence used:

  reports/TEXTURE-ISLAND-FIT.md
  reports/MATERIAL_ARCHITECTURE.md
  reports/OWNER-RULINGS-BUILDABILITY-20260902.md
  reports/REARCHITECTUREADVICE.md
  reports/REARCHITECTURE-110-115-SPEC.txt
  design/budgets/dsp.md
  design/budgets/workloads.yml
  design/fit_targets.yml

  fpga/rtl/raster/zhao_raster_texjoin_v2.sv
  fpga/rtl/raster/zhao_raster_rcp24_svc.sv
  fpga/rtl/raster/zhao_raster_perspuv_svc.sv
  fpga/rtl/texture/zhao_texture_tmu_plan.sv
  fpga/rtl/texture/zhao_texture_cache_pipe.sv
  fpga/rtl/texture/zhao_texture_cache.sv
  fpga/rtl/texture/zhao_texture_rsp_dispatch.sv
  fpga/rtl/texture/zhao_texture_palette_res.sv
  fpga/rtl/texture/zhao_texture_bilerp_lane.sv
  fpga/rtl/texture/zhao_texture_mosaic.sv
  fpga/rtl/texture/zhao_texture_aux_pipe.sv

Key hardware commits examined:

  9f0d72218f5cb30f60321308dc6b2e5aefb9aa4e
    complete ten-row texture fit and resource diagnosis

  1831e10f6359264c51840b9c55c3c363d1f98b8d
    TMU planner width narrowing after measured path

  50b0f3cc9faefee8496ee81f51979ef969f0025a
    TEXJOIN free-count race and generation widening

  8de11b1b0b92a2524404a15a33dc7d30cc644ad2
    TEXJOIN table scan replaced by work FIFO, registered outputs

  e85da61011d1a2b31be7affa34952ff8469fb195
    AUX input boundary register

  1c0a7f44ebb82cb3fd5dd74bd182a5385dff3f1e
    block-fit seed support and current repair/refit launch

This specification was written against those facts. Any later fit result should
amend the measured evidence, not silently alter the architectural laws.


END OF SPECIFICATION
====================
