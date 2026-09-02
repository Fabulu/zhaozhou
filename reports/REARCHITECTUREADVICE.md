Hi agent, here's some bro advice for some substantial rearchitecture that should help us hit the MHz target.

Find the best places for your substantial targeted subsystem rearchitecture, plan and architect it for the agent.
Verdict

The best place for substantial targeted rearchitecture is the textured-survivor path, not the current flat raster/shell:

EARLY-Z survivor
    → perspective recovery
    → primary TMU/cache
    → optional AUX/surface-sheet lookup
    → texture rejoin
    → existing fragment/TOON path

That path currently contains four separate one-at-a-time or nearly saturated machines:

zhao_raster_texjoin has a 16-entry context FIFO, but only one request may actually be live and returns are stored only for the head fragment.
zhao_raster_perspuv and zhao_raster_rcp24 process one fragment at a time.
zhao_texture_tmu_pipe is functionally complete and tested, but sustained resident throughput is still II=3 for CLUT and II=4 for direct colour, not II=1.
zhao_texture_aux is a one-request-at-a-time six-cycle machine with almost no capacity reserve.

This is exactly where a substantial redesign buys both clock frequency and useful throughput, while leaving the already-near-100-MHz raster core alone.

The current partial composition should now be considered provisionally frozen. It averages 96.87 MHz, reaches 99.50 MHz, and rotates among CMD.DMA, TILESTORE, EARLY-Z and BINNER depending on placement. There is no longer one dominant defect worth another blind local machining campaign.

Priority order
Priority	Subsystem	What to do
1	Textured-survivor island	Real multi-outstanding token architecture; rework perspective, TEXJOIN, TMU issue/return decoupling, cache hit pipeline and AUX
2	Field/Earth island	Production wrapper, tokenized F0/F1/F2 join, staged projection and real composed fit; preserve the v3 arithmetic
3	GEOM.SKIN	Re-fit current HEAD correctly; only then add registered multiplier-return and 2–4 interleaved vertex contexts if needed
4	Terrain tess → normals → project	Re-fit the current, already-reworked version; add stages only where the new report points
5	Current raster/shell	Freeze until the full composition repeatedly names the same remaining path
Do not redesign now	TOON, Surface Sheet	TOON already delivers 405,515 fragments/frame against 320,000; Surface Sheet already has the correct synchronous one-texel-per-cycle shape

The historical 31.1 MHz tessellation-plus-normals result is not a current measurement: normals have since been changed to one shared multiplier and retain roughly 119× their required capacity. It deserves a new seam fit, not a pre-emptive rewrite.

Three repository inconsistencies the agent must correct first
1. TMU v2’s header is stale

zhao_texture_tmu_pipe.sv still announces itself as incomplete and says nothing retires. The current body does contain response routing, CLUT, direct nearest, bilinear filtering, palette fallback and ordered retirement, and the CMake gate says the v2 implementation passes the serial sampler’s 79 checks. The engineering problem is no longer “finish the incomplete block”; it is turn the functionally complete block into a genuinely streaming one.

2. The old full-composition document describes the wrong cache

The existing rearchitecture document discusses a conventional four-way, write-back cache with victim selection and PLRU. The actual zhao_texture_cache is a read-only texture cache made of four independent direct-mapped lanes, with one blocking fill engine. Replacing it with a generic four-way write-back machine would add complexity the real design does not need. Preserve the actual lane architecture and pipeline it.

3. TEXJOIN’s “AUX concurrency” test does not match the production AUX interface

The current TEXJOIN test models AUX as another sampler receiving perspective-correct U/V. The real AUX block consumes:

world x/z
patch envelope x0/z0/x1/z1
surface-sheet handle

That means AUX should issue directly from the accepted fragment context. It must not wait for perspective recovery, and its descriptor must be stored in the join entry. The current test proves concurrent issuance in a simplified model, but not the production seam.

The architecture to build
1. Make a real texture-survivor island
                           ┌───────────────────────────────┐
EARLY-Z survivor ─────────>│ TEXJOIN v2: allocate token   │
                           │ store complete fragment ctx   │
                           └──────┬─────────────────┬──────┘
                                  │                 │
                           primary required     AUX required
                                  │                 │
                                  ▼                 ▼
                        PERSPECTIVE.SVC       AUX coordinate pipe
                                  │                 │
                                  ▼                 ▼
                            TMU stream        SURFACE.SHEET
                                  │                 │
                                  ▼                 ▼
                            texture return      AUX return
                                  └────────┬────────┘
                                           ▼
                              direct-index join entry
                                           ▼
                              allocation-order retirement
                                           ▼
                                existing material/fragment

The island should initially remain on gpu_clk. Field/Earth may later get its own clock; the per-pixel texture path should not begin life with a CDC boundary.

2. Internal transaction identity

Introduce a dedicated internal token type. Do not reuse external src_id.

A reasonable first point is:

typedef struct packed {
    logic [3:0] slot;        // 16 join entries
    logic [7:0] generation;  // stale-return protection
} zhao_render_token_t;

The width is cheap. Twelve bits fit comfortably inside the existing 16-bit tagging lanes where an adapter is temporarily required.

Every variable-latency request and return carries this token:

perspective request/return
TMU request/return
cache access/return
AUX request/return
Surface Sheet adapter
Field/Earth request/return later

src_id remains part of the stored visible context. It never selects a transaction.

3. zhao_raster_texjoin_v2

Build this beside the current TEXJOIN. Keep the existing one as executable specification until the new block passes all old tests plus the new multi-outstanding tests.

State

Start with 16 entries:

valid
generation
required_mask       PRIMARY | AUX
arrived_mask
base fragment context
material snapshot
world position and AUX envelope
primary RGB/A/index/status
AUX tag/strength/miss/degenerate/status
fault bits

Also have:

a free-token FIFO;
an allocation-order retirement FIFO;
a primary-request FIFO;
an AUX-request FIFO;
independent primary-return and AUX-return FIFOs;
a registered output hold.
Acceptance

f_ready_o may depend only on:

a free token;
local space for the primary request, when required;
local space for the AUX request, when required;
the one-entry input hold.

It must not depend combinationally on TMU ready, cache ready, Surface Sheet ready or final fragment output ready.

On acceptance:

Allocate a token.
Store the complete context.
Put the token into retirement order.
Enqueue a perspective job for a primary texture.
Enqueue an AUX job immediately when AUX is required.

Primary and AUX therefore begin independently.

Return handling

Returns directly index the table by token slot and verify generation.

A valid return:

stores its payload
sets its arrived bit
does not inspect the retirement head

A stale, free, wrong-generation or duplicate return:

does not mutate another entry
raises a sticky frame-fatal fault
allows already-issued traffic to drain
prevents framebuffer publication

It must not simply refuse the response forever and deadlock.

Retirement

Only the token at the retirement FIFO head may appear at the output. It retires when:

(arrived_mask & required_mask) == required_mask

The token is freed only when the registered output is accepted.

This gives out-of-order internal service while preserving exact fragment order.

Perspective recovery: turn six serial arithmetic jobs into two scheduled services

The current perspective path has two nested serialization problems:

zhao_raster_rcp24 performs four dependent multiply jobs for one reciprocal before accepting another request.
zhao_raster_perspuv then walks U and V through one shared multiplier while refusing another fragment.

The resulting current perspective capacity is about 151,515 fragments/frame, below the conservative 276,480-fragment terrain estimate.

Build zhao_raster_rcp24_svc

Keep the exact existing arithmetic and ROM. Change its execution model.

Context table

Use approximately eight reciprocal contexts:

valid
token
m
x
w
k
Newton iteration
phase = MW | MX
Pipeline
R0  accept denominator, detect zero, normalize
R1  capture normalized m, exponent and ROM seed
RQ  enqueue first MW job

M0  pop one micro-job and register multiplier operands
M1  registered multiplier result
M2  phase-specific update and requeue or complete

For each reciprocal:

MW0: m * x
MX0: x * (2^31 - w)
MW1: m * x
MX1: x * (2^31 - w)

One multiplier launches one micro-job per clock. Dependencies exist within a token, but other tokens occupy the intervening clocks.

The theoretical steady-state rate becomes:

4 multiplier jobs / reciprocal
= one reciprocal every 4 clocks
= 416,666 reciprocals/frame at 100 MHz

That is roughly 1.5× the conservative 276,480 demand without duplicating the whole reciprocal unit.

Build zhao_raster_perspuv_svc

After the reciprocal returns:

U0  enqueue U product
U1  multiply numerator × reciprocal mantissa
U2  exact variable rescale, saturation, store U

V0  enqueue V product
V1  multiply numerator × reciprocal mantissa
V2  exact variable rescale, saturation, store V

One product launches per cycle. Two are needed per fragment, so this lane can support one completed perspective pair every two cycles—well above the reciprocal engine’s one-per-four rate.

A direct-index table stores U and V by token and emits the perspective result once both have arrived.

Required gates
Bit-exact against the existing zhao_raster_rcp24 and zhao_raster_perspuv.
Zero, saturation and every exponent boundary.
Repeated external src_id.
Returns intentionally reordered by token.
Randomized output stalls.
At least 350,000 complete perspective pairs/frame at 100 MHz equivalent, giving 25% headroom over 276,480.
Standalone three-seed Fmax target: 120 MHz minimum, 140–150 MHz desirable.
TMU: retain the implementation, replace the single-file continent

The current TMU pipe has the correct broad ideas—capture, ROB, internal record identity, resident palette storage and ordered output—but too much work remains grouped into a small number of elastic stages. Its measured resident intervals are still three clocks for CLUT and four for direct colour. The currently documented workload already totals 541,640 samples/frame before creatures and beams, so this has very little usable reserve.

Build zhao_texture_tmu_stream beside the current implementation and point the same differential suite at both.

Planner stages
T0  accept request, allocate ROB entry, capture raw packet

T1  sanitize mode
    select format/filter/mip
    clamp level
    capture material/palette binding

T2  calculate selected-level dimensions
    scale U/V
    half-texel bias
    integer coordinates and fractions

T3  wrap the two unique U and two unique V coordinates
    calculate the two row bases

T4  form the four final addresses
    enqueue one registered cache-access packet

Every stage is a genuine elastic register. There must be no single a0_v that prevents accepting request N+1 while request N advances.

Cache-response handling

Place a raw-response FIFO of at least four entries after the cache.

A decode dispatcher routes each response to:

CLUT-index/palette path;
direct-nearest path;
bilinear-footprint FIFO.

The current global relationship resembling:

cache response ready = filter not busy

must disappear. A busy bilinear channel lane must not stop an unrelated nearest or CLUT response from entering local response storage.

Bilinear filtering

Retain one channel lane initially.

The known workload contains roughly 180,000 filtered channel jobs versus more than half a million cache accesses, so four lanes would spend DSPs on a unit that is not presently the maximum term. The filter accepts one channel job per clock from a footprint FIFO, carries token and channel, and writes the ROB directly.

Palette handling

Do not perform a 16-way palette-page search on the hot response path forever.

The preferred production form is:

material binding → resident palette slot + generation

The compatibility wrapper may continue accepting req_pal_base_i, but translation to a resident slot happens once in T1 or at material binding—not after every CLUT index returns.

Add an explicit palette load interface:

slot
generation
entry index
RGB565 value
valid/last

Keep the current lazy fallback only as a cold/error path behind a small fallback FIFO. It must not be one global pf_v that monopolizes issue and blocks resident work.

TMU gates
Existing 79-check differential remains green.
Resident CLUT8 nearest: II=1.
Resident direct nearest: II=1.
Bilinear requests may be accepted at II=1 until the bounded footprint FIFO fills.
No output or sampled byte changes.
ROB wrap, full, drain and output-stall tests.
Palette generation/invalidation under requests in flight.
Same src_id on hundreds of adjacent requests.
Cache and filter returns delayed independently.
Standalone three-seed Fmax target: 120–125 MHz minimum.
Cache: pipeline the real cache, not the imaginary one

Preserve the actual four independent direct-mapped read-only lanes.

Build zhao_texture_cache_pipe beside the current cache:

C0  accept and hold complete access packet
    token, lane enables, addresses

C1  calculate per-lane tag/index/word
    issue tag/data reads

C2  capture M10K outputs into local fabric registers

C3  compare tags and classify hit/miss
    register selected words and miss mask

C4  enqueue all-hit response

Important rules:

Input ready comes from the local request FIFO.
Output ready terminates at a local response FIFO.
No cache output-ready signal reaches TMU request acceptance in one combinational path.
No M10K output directly feeds broad selection, state transition or another RAM write.
Keep one blocking miss initially.
Do not add MSHRs or hit-under-miss until real traces prove the blocking miss engine, rather than hit-path timing, is the remaining limiter.
Same-line fill multicast

For a four-tap footprint, multiple lanes frequently request the same physical cache line. A miss should capture:

fill_lane_mask
line identity

Every returned fill beat is written into every matching lane in the mask. Do not fetch the same line separately for each lane.

Cache gates
One all-hit access accepted per clock.
One all-hit response produced per clock after fill.
Four lanes requesting one line cause one fill.
Random memory stalls.
Identical data, lane ordering and miss behavior to the present cache.
Three-seed island Fmax target: 125 MHz.
AUX: pipeline the exact six-bit divide

The current AUX FSM performs three restoring quotient bits in one state, three in another, issues one Surface Sheet request, waits for the answer, presents it, and only then accepts another fragment. Its nominal one-per-six rate is about 277,778/frame, almost exactly the 276,480 estimate.

Build zhao_texture_aux_pipe:

A0  accept token, world point, envelope, handle
    calculate numerator/divisor
    classify negative, saturation and degenerate cases

A1  restoring quotient bit 5, both axes
A2  bit 4
A3  bit 3
A4  bit 2
A5  bit 1
A6  bit 0

A7  form texel index and enqueue Surface Sheet read
A8  capture registered sheet response
A9  enqueue tokenized AUX return

Each stage carries its own remainder, divisor, quotient, token, handle and status. Both axes remain parallel.

A degenerate envelope travels through the ordering machinery but emits no sheet read.

SURFACE.SHEET itself should remain unchanged initially. Use a thin token adapter so its old src_id-shaped echo carries the internal token rather than an external draw ID. Surface Sheet already has the appropriate synchronous memory shape and one-read-per-cycle steady-state behavior; redesign it only if the composed fit actually names it.

Required result:

AUX acceptance II = 1
Surface Sheet request II = 1
AUX response II = 1 when the sheet is unstalled
Second campaign: Field/Earth

Do not rewrite Field v3. It already represents the architectural correction away from giant duplicated evaluators: uniform preparation, quad execution, shared services and bounded outstanding work. The unmeasured problem is its production composition and physical realization. The previous giant Field+Earth form reached only the low-40-MHz range and used enormous area; the shared v3 family has shown much healthier timing in historical probes.

Build one zhao_field_earth_island around the current quad engine.

Pipeline
E0  accept surface point, allocate point token

E1  schedule required F0/F1/F2 field requests
    record required mask

E2  direct-index field-return join
    store each returned value by point token

E3  once complete, register numerator U/V and denominator

E4  submit denominator to tokenized reciprocal service

E5  registered numerator × reciprocal products

E6  clip, depth and visibility decisions

E7  material/shading result

E8  registered output hold

Coefficient memories get an explicit structure:

request register
→ ROM/M10K read
→ fabric capture
→ local registered distribution

No coefficient-memory output may directly fan across all lanes.

First physical frontier

Fit at least two exact configurations:

A: current heavy gate
   CTX=32 OUTSTANDING=16 LANES=4 LONGQ=16
   DIST_BANKS=8 RING_UNITS=8 REGS=64

B: reduced physical point
   CTX=16 OUTSTANDING=8 LANES=4 LONGQ=8
   DIST_BANKS=4 RING_UNITS=4 REGS=64

Run the same 1,024-point crater-ring, impact-wave and wave-pool workloads through both. The current simulation gates establish throughput, but the area and composed Fmax of the selected configuration remain unmeasured.

If Field/Earth cannot close at the common renderer clock but still meets workload with at least 25% capacity reserve at a lower frequency, place it on a dedicated PLL clock with real asynchronous FIFOs. That is a legitimate subsystem boundary. Do not cut synchronous paths with timing exceptions and call it a second clock.

Conditional rearchitecture only
GEOM.SKIN

The current block’s own model says MUL_LANES=3, II=12, needs 86.4 MHz to serve 120,000 vertices/frame. Its quoted 89.65-MHz fit predates the timing-measurement cleanup and must be repeated under the current harness before architecture changes.

When—and only when—the current fit repeatedly names DSP-output-to-accumulator logic:

Register each multiplier lane’s output.
Register the balanced lane sum before accumulator update.
Permit the product issue walk for vertex N+1 while vertex N is in blend.
Start with two vertex contexts; sweep two and four.
Tag every row product {vertex_slot, bone, row}.
Retire vertices in acceptance order.

The goal is to let extra latency increase without forcing II from 12 to 13 or 14. TOON’s existing multi-context pipeline is the local pattern to copy: it covered a 32-stage latency with 16 slots and moved from measuring latency to measuring actual throughput.

Terrain

First build a current:

TESS → two-entry registered seam → NORMALS
     → two-entry registered seam → PROJECT

fit island.

If normals is named, register the shared 33×33 multiplier result before accumulator update. Its demand is so low that another cycle is essentially free.

If the projector is named, act on that path rather than restructuring tessellation. Separately, the production terrain path should project unique patch points once and reuse them rather than projecting every triangle corner, but that is primarily a frame-capacity architecture, not a speculative Fmax fix.

EARLY-Z and TILESTORE

Do not reopen these because one seed dislikes one of them.

Rearchitect only when the same logical cone appears in at least two of three full-composition seeds. Then use:

Z0  accept; predecode word and lane; reserve merge entry
Z1  RAM read
Z2  fabric capture
Z3  select/compare/forward
Z4  commit and response

with a two-to-four-entry direct-index forwarding/merge queue. Predecode the 256-entry address upstream into row and lane; do not place priority encoding plus a 256:1 mux after a RAM launch.

Execution staircase
Phase 0 — freeze and make the measurement trustworthy
Do not branch from an unresolved fitter experiment. Current HEAD 7fe018d is testing physical-synthesis register duplication and has no result commit yet.
Use the first commit that records its measured verdict. Until then, the known stable settings are:
OPTIMIZATION_MODE = HIGH PERFORMANCE EFFORT
synthesis technique Balanced
OPTIMIZATION_TECHNIQUE=SPEED remains reverted because it lost 3.01 MHz and added area.
Create a separate worktree and separate build/fit directories.
Add reports/TARGETED-SUBSYSTEM-REARCHITECTURE.md.
State that it supersedes contradictory implementation details in REARCHITECTURE-110-115-SPEC.txt, while retaining that document’s general rules about registered islands, tokenization and RAM capture.
Phase 1 — interfaces and stubs
Add the token package.
Add texjoin_v2 with deterministic registered primary/AUX stubs.
Randomize response order and backpressure.
Fit the join island before real arithmetic enters it.
Phase 2 — perspective service
Add tokenized reciprocal micro-scheduler.
Add U/V product service.
Differential, throughput and standalone fit.
Phase 3 — real AUX
Add the six-stage exact quotient pipeline.
Connect the real Surface Sheet through the token adapter.
Fit join + perspective + AUX while primary TMU remains a stub.
Phase 4 — streaming TMU with cache stub
Stage the TMU planner.
Separate nearest, CLUT and bilinear return paths.
Reach resident nearest/CLUT II=1.
Fit.
Phase 5 — real pipelined cache
Stage the actual direct-mapped four-lane cache.
Add same-line fill multicast.
Keep one blocking miss.
Fit.
Phase 6 — complete texture-survivor island
TEXJOIN v2 + perspective service + TMU + cache + AUX + Surface Sheet.
Three seeds.
Exact image/capture gate.
Long randomized-stall and token-wrap run.
Phase 7 — Field/Earth island
Real v3 quad engine.
Synthetic coefficient source first.
Real coefficient services second.
Real F0/F1/F2 join and projection last.
Fit after each boundary.
Phase 8 — conditional geometry and terrain
Re-fit GEOM.SKIN.
Re-fit current terrain chain.
Apply only measured fixes.
Phase 9 — full composition

Add one island at a time to the frozen raster/shell:

raster/shell
+ texture interfaces
+ texture-survivor island
+ Field
+ Earth/surface
+ terrain
+ creature geometry

Archive a fit after each addition. Do not jump from the present partial renderer to the entire intended machine in one fit.

Timing and throughput gates
Scope	Architecture target	Acceptance
New leaf datapath	150 MHz design budget	Missing 150 is acceptable when the island still clears its gate
Perspective/TMU/cache/AUX leaf island	120–125 MHz across three seeds	No structural path repeatedly below target
Texture-survivor composition	115–120 MHz	Exact output and required sustained rates
Field/Earth common-clock island	110–120 MHz	Or legitimate separate clock with ≥25% workload reserve
Intended full composition	105 MHz hard target	110 MHz preferred margin
Product clock	100–105 MHz	No lucky-seed dependency

Every fit must archive:

exact commit and source hashes
Quartus version and settings readback
seed
WNS/TNS and endpoint count
worst 100 paths
path owner and startpoint type
ALM/FF/DSP/M10K by hierarchy
compiled-source manifest
elaborated-instance manifest
queue high-water marks
transactions accepted/issued/returned/retired
all stall-reason counters
functional signature or rendered CRC

A major change is accepted only when:

the path it targeted disappears structurally;
the same-seed comparison is favorable;
functional outputs remain exact;
throughput does not silently fall;
at least three seeds support the architecture-level conclusion.
Things the agent must not do
Do not try to push the current flat core from 99.5 to 110 MHz before beginning integration.
Do not redesign a block because it owns many paths in one seed.
Do not resurrect synthesis SPEED.
Do not use external src_id as transaction identity.
Do not build a generic four-way/write-back texture cache.
Do not add nonblocking cache misses before real traces demand them.
Do not place a FIFO at every module boundary indiscriminately.
Do not let ready propagate through TEXJOIN → TMU → cache → memory.
Do not let M10K output drive broad combinational control before fabric capture.
Do not false-path or multicycle a real sampled path.
Do not change arithmetic order, widths or rounding to gain Fmax.
Do not replace the old implementations until the new ones pass their exact tests.
Do not redesign TOON, Surface Sheet or current normals without a current fit naming them.
Do not instantiate three old dense Field networks.
Paste-ready agent brief
ZHAOZHOU TARGETED SUBSYSTEM REARCHITECTURE

MISSION

Do not rewrite the console. Do not continue local timing whack-a-mole on the
current flat renderer. Build the missing production subsystems as registered,
tokenized islands, beginning with the complete textured-survivor path.

The first campaign is:

    EARLY-Z survivor
      -> TEXJOIN v2 allocation
      -> perspective service
      -> streaming TMU / actual four-lane direct-mapped cache
      -> optional pipelined AUX / SURFACE.SHEET
      -> direct-index return join
      -> allocation-order retirement
      -> existing fragment/material path

The second campaign is a production Field/Earth island around the existing v3
quad architecture. Do not rewrite Field arithmetic before a real composed fit.

BASELINE AND TOOL SETTINGS

1. The present main HEAD may be an unresolved physical-synthesis experiment.
   Read the latest commits first. Base work only on a commit that records the
   experiment's verdict.
2. HIGH PERFORMANCE EFFORT remains authoritative.
3. OPTIMIZATION_TECHNIQUE=SPEED was measured harmful and reverted. Do not
   restore it.
4. Use a private worktree and private build/fit directories. Never share a
   mutation-sweep or Quartus workspace with another lane.
5. One structural hypothesis per commit and per fit.
6. Keep the old implementations beside the new ones as executable
   specifications until exact equivalence is proved.

READ FIRST

    reports/MHZ-PASS-SUMMARY.md
    reports/REARCHITECTURE-110-115-SPEC.txt
    reports/PER_PIXEL_BUDGET.md
    reports/RENDERER_ARCHITECTURE.md
    fpga/rtl/raster/zhao_raster_texjoin.sv
    fpga/rtl/raster/zhao_raster_perspuv.sv
    fpga/rtl/raster/zhao_raster_rcp24.sv
    fpga/rtl/texture/zhao_texture_tmu_pipe.sv
    fpga/rtl/texture/zhao_texture_cache.sv
    fpga/rtl/texture/zhao_texture_aux.sv
    fpga/rtl/surface/zhao_surface_sheet.sv
    reports/Fieldv3.md
    reports/FIELD_V3_EARTH_OPTIMISATION_NOTES.md

Before RTL, create:

    reports/TARGETED-SUBSYSTEM-REARCHITECTURE.md

Record that this document supersedes stale implementation details in the older
110-115 MHz document:

    - the real texture cache is four independent direct-mapped read-only lanes,
      not a generic four-way write-back cache;
    - TMU v2 is now functionally complete and tested despite its stale header;
    - current TEXJOIN is functionally valid but not a multi-outstanding,
      direct-index production join;
    - production AUX takes world position, envelope and sheet handle, not TMU
      U/V coordinates.

NON-NEGOTIABLE ARCHITECTURE RULES

- Latency may grow. Exact arithmetic, output ordering and required sustained
  throughput may not regress.
- Every variable-latency operation carries an internal token containing an
  entry index and generation.
- External src_id is visible context, never transaction identity.
- RAM read -> fabric capture -> calculation. Never RAM -> broad logic -> commit
  in one stage.
- DSP operands and DSP results are registered where practical.
- Ready is local. A ready signal may not traverse more than one subsystem.
- Store wide context once in a direct-index table. Services carry narrow jobs
  and token IDs.
- Returns may complete out of order internally; visible fragments retire in
  acceptance order.
- Reset valid bits, state and pointers. Do not reset wide payload arrays merely
  to make simulation look clean.
- No false paths or multicycle paths for actual synchronous behavior.
- No floorplanning before registered island architecture is measured.

TOKEN CONTRACT

Create a parameterized internal render token, initially 16 slots with an
8-bit generation. Every perspective, TMU, AUX, cache and join transaction
carries it.

A stale/free/wrong-generation/duplicate response:

    - may not mutate a live unrelated entry;
    - raises a sticky frame-fatal fault;
    - does not deadlock response acceptance;
    - prevents publication after outstanding traffic drains.

PHASE 1: TEXJOIN V2 WITH STUB SERVICES

Create zhao_raster_texjoin_v2 beside the existing block.

Use:

    16 direct-index entries
    free-token FIFO
    allocation-order retirement FIFO
    primary-request FIFO
    AUX-request FIFO
    independent return FIFOs
    registered output hold

An entry stores:

    valid/generation
    required and arrived masks
    complete fragment/material context
    primary result
    AUX result
    error bits

Input ready depends only on free local capacity. AUX issues immediately from
the accepted world/envelope descriptor; it does not wait for perspective U/V.
Returns write entries by token. Output retires only the complete retirement
head.

Test:

    repeated external src_id
    randomized service ready
    primary and AUX returning in opposite orders
    output stalls
    token wrap
    stale and duplicate responses
    reset with work in flight
    fair-service eventual retirement
    no response consumed by the wrong pixel

Fit with registered deterministic stubs. Do not add real arithmetic until this
topology is green and at least 125 MHz standalone.

PHASE 2: TOKENIZED PERSPECTIVE SERVICE

Create zhao_raster_rcp24_svc and zhao_raster_perspuv_svc beside the scalar
blocks.

RCP service:

    R0 normalize and zero-classify
    R1 capture ROM seed and allocate context
    one registered multiplier micro-job launched per clock
    four jobs per reciprocal: MW0, MX0, MW1, MX1
    phase completion requeues the token or returns it

Use enough contexts to keep the multiplier busy. One multiplier therefore
sustains one reciprocal every four clocks.

U/V service:

    one registered product lane
    two jobs per fragment
    exact existing variable rescale and saturation
    direct-index U/V completion table

Requirements:

    bit-exact to existing scalar blocks
    at least 350,000 completed perspective pairs per 100 MHz frame equivalent
    >=120 MHz across three standalone seeds
    no external src_id identity
    randomized return/output stalls

PHASE 3: PIPELINED AUX

Create zhao_texture_aux_pipe.

A0 captures world point, envelope, handle and token and determines clamp and
degenerate cases.

A1..A6 perform one restoring quotient bit per clock for U and V in parallel.

A7 enqueues the exact Surface Sheet texel read.
A8 captures the registered sheet response.
A9 enqueues a tokenized AUX return.

Target II=1. Degenerate requests retire without a memory read. Add a thin token
adapter around Surface Sheet; do not redesign Surface Sheet unless a real fit
names it.

PHASE 4: STREAMING TMU WITH CACHE STUB

Create zhao_texture_tmu_stream beside zhao_texture_tmu_pipe.

Stages:

    T0 accept and ROB allocation
    T1 mode/mip/material sanitization
    T2 coordinate scale, floor and fractions
    T3 unique U/V wrapping and row bases
    T4 four addresses and registered cache request

Use a cache-request FIFO and raw cache-response FIFO.

Dispatch responses independently to:

    CLUT/palette
    direct nearest
    bilinear footprint FIFO

A busy bilinear lane may not globally deassert cache response ready. Keep one
channel filter lane initially. Add an explicit resident palette load/bind path;
move base-to-slot translation outside the hot response path. Lazy fallback is
cold and queued.

Requirements:

    all old TMU exact tests green
    resident CLUT nearest II=1
    direct nearest II=1
    bounded bilinear footprint queue
    output ordered by ROB
    >=120 MHz across three seeds

PHASE 5: ACTUAL CACHE PIPELINE

Create zhao_texture_cache_pipe and preserve the real architecture:

    four independent direct-mapped read-only lanes
    one blocking fill engine initially
    no PLRU
    no dirty victims
    no writeback architecture
    no MSHR yet

Stages:

    C0 request capture
    C1 lane tag/index/word and memory launch
    C2 fabric capture
    C3 hit/miss decision
    C4 response FIFO

Add same-line fill multicast with a fill_lane_mask. An all-hit access must be
accepted and returned every clock after pipeline fill.

PHASE 6: COMPLETE TEXTURE ISLAND

Compose:

    texjoin_v2
    perspective service
    tmu_stream
    cache_pipe
    aux_pipe
    surface-sheet adapter

Run:

    old exact block tests
    composed exact-image/reference tests
    long randomized stalls
    repeated src_ids
    independent primary/AUX latency
    token-generation stress
    cold palette operations
    cache misses
    final drain and frame-fatal behavior

Fit three fixed seeds. Archive all reports. Target 115-120 MHz for the island.

PHASE 7: FIELD/EARTH ISLAND

Build zhao_field_earth_island around the existing v3 quad machine.

Do not instantiate old dense Field evaluators.

Pipeline:

    point accept/token
    F0/F1/F2 scheduling
    direct-index field join
    registered numerator/denominator
    reciprocal request
    registered projection products
    clip/depth/visibility
    material output
    registered output hold

Every coefficient ROM/RAM output enters a fabric register before distribution.

Fit the current heavy configuration and one reduced-resource configuration
against identical 1024-point real programs. Choose by throughput, area and
three-seed Fmax. A separate Field/Earth PLL domain is allowed only with real
async FIFOs and at least 25% workload capacity reserve.

CONDITIONAL WORK

GEOM.SKIN:
    Re-fit current HEAD first. If a DSP-output/accumulator path repeats across
    seeds, register the product and balanced lane sum. Add two or four vertex
    contexts so added latency does not increase initiation interval.

TERRAIN:
    Re-fit current TESS->NORMALS->PROJECT after the normals redesign. Add stages
    only on the named current path. The historical 31.1 MHz number is not a
    current result.

EARLY-Z/TILESTORE:
    Frozen unless a full-composition path repeats in at least two of three
    seeds. Then use address predecode, RAM read, fabric capture, compare/forward
    and commit stages with a small merge queue.

DO NOT REARCHITECT:
    RASTER.TOON
    SURFACE.SHEET
    current TERRAIN.NORMALS
unless a current composed fit names them.

MEASUREMENT GATES

New leaf datapaths are designed against a 6.667 ns / 150 MHz stage budget.
Islands must close at 120-125 MHz across three seeds where stated.
Full composition hard target is 105 MHz; 110 MHz is preferred margin.

Every fit archives:

    exact source commit/hashes
    settings readback
    seed
    WNS/TNS/endpoints
    worst 100 paths
    startpoint and owner tables
    resource use by hierarchy
    source and elaborated-instance manifests
    queue high-water marks
    accepted/issued/returned/retired counters
    stall reasons
    exact image or output signature

A structural edit is accepted only if the path it targeted disappears, exact
outputs remain unchanged, sustained throughput passes, and same-seed evidence
earns the change.

Commit every independently green result immediately. Do not batch unrelated
architecture work into one commit or one fit.

This is a large redesign of one badly composed hot island, followed by a production wrapper around the already-better Field architecture. It deliberately avoids the two bad extremes: rewriting the entire console, or spending another month removing 0.1 ns from whichever flat-renderer path a particular seed happened to dislike.
