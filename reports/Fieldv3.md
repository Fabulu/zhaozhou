I guess our recent measurements showed we're still way below par when it comes to this difficult chip.

Here's a rearchitecture from bro: 

Check everything thoroughly and architect a real lasting solution
The verdict

I checked current main through 8e1c402, the real Earth programs, the compiler, the v2 RTL, the fitted timing reports, the front end, the terrain composer, the program cache, and the workload models.

Field v2 is a successful prototype, not a viable production architecture.

It has proved several crucial things:

the complete exact Field instruction set fits;
four-lane SIMD is worthwhile;
the arithmetic units agree with the reference;
the register file can infer as real RAM;
the chip has enough resources for a serious Field machine.

But v2 cannot be repaired into Earth60 by another local timing pass. It has four separate architectural failures, and fixing only one leaves the others intact.

The lasting answer is:

Freeze v2 as the exact fallback and differential oracle. Build Field v3 as a prepared, four-wide vector fabric with direct profile walkers, queued vector math services, and field-major terrain accumulation.

That preserves programmable fields. It is not hardwiring the three current spells. It changes how a validated Field program is lowered onto the hardware, not what the program means.

What is actually broken
1. The current engine is neither 100 MHz nor timing-clean

The clean leaf fit is:

Field v2 front + core	Measured
ALMs	7,870
registers	10,482
M10Ks	33
DSPs	15
restricted Fmax	59.22 MHz

At the 100 MHz constraint, setup slack is −6.886 ns with −5,134 ns TNS. It also has −1.938 ns hold slack. That hold failure matters: lowering the clock frequency does not inherently repair hold timing. “Just run it at 59 MHz” is not even a clean sign-off result for this isolated characterization, never mind the composed console.

The last three timing cuts exposed three unrelated paths clustered around 16–18 ns:

register file → multiply → rescale → add → write;
ring → reciprocal normalization → ROM → multiplier;
ready scoreboard → round-robin select → indexed state → scoreboard.

That is a pipeline-depth plateau, not one defective arithmetic block.

2. Even a magical 100 MHz v2 would still be far too slow

The exact committed Earth programs are 27–30 instructions long. Their costly operations are not theoretical:

every one has DIST2;
every one has a reciprocal from smoothstep;
impact has curves;
crater has RING;
wave pool has sine and cosine.

With the present ready-only-when-idle long units, the modeled 128-association frame costs at 100 MHz are:

Program	Share of reserved Field budget
impact_wave	930%
wave_pool	439%
crater_ring	502%

So pipelining the scheduler from 59 to 100 MHz while leaving scalar long units intact would still miss by roughly 4.4×–9.3×.

3. The current front end alone is impossible

This is a major hole in the earlier performance model.

The existing front fills 32 point slots, runs them, then drains them. For Earth:

each point has 12 input lanes, written one lane per clock;
each point has 4 output lanes;
each output lane takes three read phases;
one further cycle completes the result handshake.

Derived directly from the current RTL:

input transport   = 12 × 1,089 = 13,068 clocks/association
output transport  = 13 × 1,089 = 14,157 clocks/association
total transport   =             27,225 clocks/association

The intended 100 MHz/60 Hz/20%-reserve allowance is only 10,416 clocks per association.

For 128 associations, transport alone is:

27,225 × 128 = 3,484,800 clocks/frame

That is 261% of the entire reserved 100 MHz Field budget before executing one instruction.

The existing Scenario C model explicitly assumed no transport overhead, so its otherwise encouraging numbers never described the implemented front end.

4. The current execution and terrain seams point in opposite directions

The Field engine naturally evaluates:

one program over all points of one patch.

That is field-major execution.

TERRAIN.PATCH currently expects:

for each vertex, receive all field results in command order.

That is vertex-major composition.

Bridging those two requires either:

storing every complete field lattice and transposing it; or
destroying program locality by switching programs at every vertex.

Neither bridge currently exists. The generic front therefore does not connect naturally to the terrain composer that is supposed to consume it.

There is also an opcode integration problem: the v2 core uses a private compact opcode encoding, while canonical .zprog files use the real Field IR encoding. The differential test has a hand-written translator because the numeric values collide as valid but different operations. A production front cannot safely load canonical programs into v2 as presently encoded.

The approaches to reject
Proposal	Verdict	Why
Run current v2 at 59 MHz	Reject	Long units and transport remain fatal; current leaf result also has hold failures
Pipeline current scheduler to 100 MHz	Reject as the complete answer	Still 4.4×–9.3× short and transport alone still exceeds the frame
Replicate complete v2 engines	Reject	Multiplies 7,870 ALMs, 33 M10Ks and the bad front; does not solve composition
Add one more multiplier	Reject as the complete answer	Long operations, transport, scheduling and composition still bind
Hardwire three current Earth spells	Reject	Throws away the programmable-machine premise
Approximate distance, reciprocal or curves	Reject	Breaks bit-exact software/FPGA semantics
Give v2 a slower private clock and move on	Reject	A clock crossing does not cure insufficient work per second
Field v3: the lasting architecture
Canonical validated .zprog
            │
            ▼
ARM planner / exact uniform preparation
            │
            ├── canonical program hash remains the identity
            ├── prepared scalar values
            └── derived FPLAN execution plan
                         │
                         ▼
                  FPLAN / table cache
                         │
              ┌──────────┴──────────┐
              ▼                     ▼
       profile walker         uniform bank
       Earth / Warp / ...            │
              │                     │
              └──────┬──────────────┘
                     ▼
           4-wide vector executor
           ready-context FIFO
           vector register file
                     │
       ┌─────────────┼───────────────────┐
       ▼             ▼                   ▼
 vector MUL     curve service       distance service
    bank        barrelized          exact root banks
       │             │                   │
       └─────────────┴───────────────────┘
                     │
                     ▼
          field-major patch reducer
          command-ordered accumulators
                     │
                     ▼
             composed terrain cache
1. Keep one canonical ISA

.zprog, its validation rules, its program hash, captures and the exact C++ semantics remain canonical.

Add a derived hardware execution artifact—call it FPLAN—keyed by:

canonical_program_hash
+ field-plan ABI version
+ target execution-fabric version

FPLAN is not a new game-visible language. It is analogous to a CPU’s decoded micro-op cache.

The translation must be generated from the canonical operation table, not maintained as another hand-written opcode switch. That eliminates the current “two valid but different opcode numberings” failure class.

A raw canonical program without an FPLAN remains executable through frozen v2 or a software fallback. It simply does not receive an Earth60 performance certificate.

2. Partially evaluate uniform work on the ARM

The compiler already has an SSA-like virtual program before physical register allocation, and the existing analysis has proved the exact uniform/varying split for the real Earth programs. Only x and z vary at each lattice point; age, phase and parameters are uniform for the whole field instance.

Measured split:

Program	Varying instructions	Uniform instructions
impact_wave	16	13
wave_pool	17	9
crater_ring	13	14

The ARM should execute the uniform subgraph once per field instance per frame, not once per vertex and not once per patch.

That distinction is enormous. Eight Erupts crossing 16 patches produce 128 patch-field associations, but only eight uniform preparations.

The preparation uses the exact same fixed-point primitives as the canonical interpreter. No floats and no approximate “CPU version.” The C++ interpreter should be refactored so full interpretation and partial preparation call the same semantic step functions.

Prepared values include ordinary scalar intermediate results and mixed-operation preparation. For example:

smoothstep’s reciprocal disappears from the varying loop;
uniform curves disappear completely;
crater material and velocity may become direct uniform outputs;
a mixed RING(d, r0, r1) becomes an internal prepared-ring operation with exact midpoint and reciprocals calculated once, while the varying d path retains all separately rounded multiplications.

The uniform SatLedger result is carried in the field descriptor and ORed with the varying execution result exactly as if every point had independently executed those same uniform instructions.

3. Replace point transport with profile walkers

The generic 12-in / 4-out point stream must not be used in the production Earth path.

An Earth walker already knows:

patch origin;
pitch;
lattice index;
field footprint;
prepared uniform block.

It should generate four x,z points directly every vector group. Uniform operands come from the scalar bank. No point spends twelve clocks being copied into registers.

The same vector fabric remains shared across all five profiles, but each profile gets a small adapter:

Profile	Varying source
Earth	lattice x,z
Warp	vertex position/normal
Flow	particle state
Formation	instance/index state
Stamp	stencil u,v

These are not five Field engines. They are five stream generators feeding one engine.

Outputs also leave directly. The plan marks its four output registers. Writeback snoops those destinations into per-context export registers, so END can enqueue a complete four-point result without rereading four registers through a three-phase host port.

4. Replace the scoreboard scan with a ready-context FIFO

Keep the excellent v2 invariant:

one instruction in flight per context.

That avoids forwarding, register renaming and reorder machinery.

But replace the combinational round-robin scan with explicit ownership:

a context becomes ready → its ID is enqueued;
issue pops one context ID;
short-op completion re-enqueues it;
long-service completion re-enqueues it;
END releases it.

A context ID must always be in exactly one state:

free
ready queue
issue pipeline
waiting on one service
writeback queue
completed

The execution pipeline becomes intentionally staged:

dequeue and register context ID;
registered plan fetch/decode;
synchronous register-file read;
execute or enqueue service request;
result/writeback and context requeue.

That removes the measured select-then-index-then-feedback scheduler loop by construction.

5. Use separate queues for long services

Current v2 has one global long-op slot and then serializes the four SIMD lanes through one scalar unit. That is why a 23-clock scalar curve becomes roughly a 92-clock vector curve.

v3 needs:

one request FIFO per long service;
a tag containing context ID, destination and result count;
one result FIFO per service;
a central single-writeback arbiter.

The register file retains one write destination per clock, but service execution can overlap. A context waits until all its owed writes land.

6. Build only the hot services wide

Do not attempt to make all 31 operations equally fast.

Vector multiply bank

Retain one four-wide vector multiply/MAD bank. Four 33-bit lanes map to about 12 DSPs; the current complete v2 already consumes 15 DSPs, so this is not introducing an alien multiplier budget.

Ordinary MUL, distance squares, curve interpolation and prepared-ring steps all schedule onto this one measured resource. FPLAN records its demand.

Curve service

The current curve unit performs six dependent binary-search steps and leaves the unit idle during registered table waits. Rather than copying the whole 950-ALM unit four times, make it a barrel service:

multiple scalar lane contexts in flight;
active-program table cache with true dual-port reads;
two segment-search reads per clock;
vector multiplier bank for the final interpolation.

A four-lane curve request requires 24 table reads. Two ports give a structural minimum of 12 lookup clocks. The first probe target should therefore be:

four-point CURVE initiation interval ≤ 12–14 clocks.

That is a real architecture target derived from memory-port demand, not “II=1 because that sounds nice.”

Exact distance service

The existing exact 64-bit integer square-root block is only about 251 mapped ALMs. Eight copies are therefore on the order of 2,000 leaf ALMs before queues and routing—plausible rather than catastrophic.

Use two banks of four exact roots:

bank A processes one four-point DIST2;
bank B accepts the next while A runs;
vector multiplier bank supplies the squares;
exact unsigned sum and exact floor-root semantics remain unchanged.

Initial target:

four-point DIST2 initiation interval ≤ 20 clocks.

A radix-4 exact root may eventually beat the duplicated current root, but the decision should be made by two fitted probes. No new root algorithm is adopted merely because it sounds clever.

Prepared ring

Current RING uses two reciprocals and nine separately rounded multiplications. Its radii are uniform in the committed crater program. Prepare the midpoint and reciprocals once on ARM; retain an exact vector microsequence for the nine varying products.

That makes one four-point ring approximately nine vector-multiplier issue slots, not four scalar runs through a 50-clock FSM. Its rounding remains bit-identical because every individual product and rescale still exists.

Cold service lane

Keep the complete exact scalar implementations for uncommon operations:

varying normalize;
noise;
rotation;
spline;
unprepared ring;
any future odd program.

But give FPLAN a performance classification:

realtime/hot: certified against the profile deadline;
cold: exact but not certified for the maximum live-field workload;
software: evaluate patch on ARM and upload the lattice.

The full opcode whitelist survives. What disappears is the lie that every legal program is automatically affordable at the maximum Earth workload.

7. Reduce the vector register file

Canonical Field IR still has 64 registers. FPLAN does not need to expose all 64 physical vector registers.

The current real programs reach only about 17–18 registers before uniform elimination. A hot plan can be allocated into at most 32 vector registers, while uniform values live in a separate scalar bank.

That changes each register-file replica from:

8 contexts × 64 regs × 32 bits = 16,384 bits

to:

8 contexts × 32 regs × 32 bits = 8,192 bits

It also removes the host read replica.

Current v2 has four read replicas per lane—A, B, C and host—and those memories account for 32 M10Ks. FPLAN needs A/B/C only, and halving the depth makes each replica one M10K rather than two. The resulting hot register file should be around:

4 lanes × 3 readers × 1 M10K = 12 M10Ks

rather than 32.

The slow fallback retains the canonical 64-register file.

The terrain side must change too
Field-major patch accumulation

Amend TERRAIN.PATCH’s internal seam.

For one patch:

load compose_top = max(base + scar, bottom) into an on-chip accumulator;
process fields in command order;
for each field, run its program over the patch;
read-modify-write each affected accumulator vertex;
after the last field, apply the final bottom clamp and publish the composed lattice.

This is bit-exact.

The composition law is:

live_top[v] =
  max(compose_top[v] + field0[v] + field1[v] + ... in command order,
      bottom[v])

Changing the outer loop from:

for vertex:
    for field:

to:

for field:
    for vertex:

does not change the order of additions at any individual vertex. Vertices are independent, and command order remains intact.

The patch scratch is naturally four-bank M10K storage, indexed by vertex mod 4, allowing one four-vertex vector update per clock. Four Earth output reducers require roughly 16–20 M10Ks for one patch in flight, depending on packed widths.

The reducer for each output follows its own profile law:

height: command-ordered saturating add;
velocity/nav: their exact declared accumulation;
material: exact writer-selection law;
hazard or other categorical output: its declared reduction.

It is not one generic “add all four outputs” block.

This simultaneously:

matches Field’s program-major execution;
eliminates storage of sixteen complete temporary field lattices;
preserves exact order;
makes both cameras consume one composed result;
creates the natural point for dirty masks and counters.
A conservative closure target

These numbers are a v3 architectural acceptance envelope, not a claim that unbuilt RTL already achieves them.

One full patch has:

ceil(1,089 / 4) = 273 vector groups

Initial service targets:

Resource	Target
vector instruction issue	1/group instruction/clock
four-point CURVE II	≤14
four-point DIST2 II	≤20
vector multiply/MAD	II 1
prepared RING	9 multiplier slots/group

For the three committed programs, after exact uniform preparation, the expected binding service is around 5,460 clocks per association. Add scheduling, masks, plan setup and conservative queue overhead, and set the hard target at:

≤6,000 clocks per full 1,089-vertex association.

Then:

128 associations × 6,000 = 768,000 clocks

Worst-case patch initialization and finalization, assuming 128 different patches rather than beneficial overlap:

128 × 2 × 273 = 69,888 vector clocks

Add further plan/table/setup margin and make the acceptance ceiling:

≤850,000 Field/Earth-slice clocks for the complete 128-association stress frame.

Comparison against a 20%-reserved frame:

Field clock	Usable clocks/frame	850k usage
100 MHz	1,333,333	64%
90 MHz	1,200,000	71%
80 MHz	1,066,667	80%
current 59.22 MHz	789,600	108% — fails

This is the correct clock decision rule:

Not “must be 100 MHz because 100 is round,” and not “59 MHz sounds fast.”
The complete measured Earth slice must finish the frozen worst frame inside 80% of its actual available cycles.

Design v3 for the shared 100 MHz GPU domain. Accept a lower private clock only after the complete slice—not a leaf core—proves the deadline with reserve. Based on this conservative envelope, 80 MHz is approximately the lowest credible result; 59 MHz is not.

Provisional resource gates

These are stop-work thresholds for architecture experiments, not predictions:

Scope	ALMs	DSPs	M10Ks
v3 execution fabric	≤10,000	≤18	≤40
complete Earth slice including patch scratch	≤11,500	≤18	≤64

Why these are plausible:

current v2 is already 7,870 ALMs / 15 DSP / 33 M10Ks;
removing the host RF replica and reducing hot plans to 32 vector registers should save roughly 20 M10Ks;
seven additional current-style root units are roughly 1,750 leaf ALMs;
the scheduler scan and generic fill/drain front disappear;
the patch accumulator spends M10Ks instead of ALMs or SDRAM bandwidth.

But every one of those is only a hypothesis until the isolated probe is fitted. No full-engine overnight fit should be used to answer a question a 30-line microprobe can answer in minutes.

Implementation order
Phase 1 — freeze and correct the contracts

Freeze v2 except correctness bugs. Mark it:

exact fallback;
differential reference RTL;
unsupported as the Earth60 production path.

Amend:

FIELD.SEQ.CORE: one semantic engine, profile adapters permitted;
FIELD.SEQ.EARTH: real lane bindings and real deadline;
TERRAIN.PATCH: field-major internal reducer;
Field cost model: resource-demand vectors rather than one provisional cycle count;
program cache: store/lookup rules for FPLAN and table data.

The current model must be regenerated with:

current 59.22 MHz;
input/output transport;
patch reduction;
table loads;
real service IIs;
actual active vertex counts.
Phase 2 — build the exact software planner first

The planner produces:

canonical hash
plan version
uniform instruction block
prepared uniform register values
vector uops
source-kind tags
output map
per-resource demand vector
original PC/source mappings

Required differential:

full canonical zfield::interpret
==
uniform preparation + FPLAN vector reference executor

Compare:

every output;
every saturation lane;
rcp0;
boundary inputs;
random legal programs;
all three committed Earth programs.

Until this is green, no v3 RTL.

Phase 3 — fit the five decisive probes
Ready-context FIFO scheduler with registered plan fetch.
Reduced 4×8×32 vector register file.
Two-bank exact distance service, target II ≤20.
Barrel curve service with real registered table RAM, target II ≤14.
Four-bank patch accumulator with exact command-order reducers.

Each probe gets:

map;
place and route;
Fmax;
setup and hold;
measured II;
randomized differential;
mutation sweep.

A probe missing its target kills or changes that topology before it contaminates the whole engine.

Phase 4 — compose one actual Earth machine

The first meaningful integration is:

prepared field descriptor
→ Earth lattice walker
→ v3 vector executor
→ patch accumulator
→ composed-height cache

Run actual serialized versions of:

impact_wave;
wave_pool;
crater_ring.

Then run a synthetic frame with:

128 full-patch associations;
worst command-order overlap;
maximum output backpressure;
table-cache hits and misses;
mixed programs;
saturation edges.

The gate is ≤850,000 cycles at the measured clock, not “the microbenchmark looked fast.”

Phase 5 — only then integrate with the console

The composed Earth slice must be tested with:

actual local SDRAM contention;
command DMA;
terrain tessellation and normals;
both views;
real scanout;
board clocking;
HPS uploads.

And it needs non-negative setup and hold at every required corner.

Verification law

The latest repository audit found that a block recorded as closed—with directed tests and a formal proof—still had tests that failed to notice 8 of 20 deliberate defects. That is not an indictment of the RTL; it demonstrates that green tests do not prove test sensitivity.

Every new Field v3 seam therefore needs mutation coverage, especially:

uniform/varying misclassification;
wrong canonical-to-uop mapping;
stale prepared values;
lost or duplicated context IDs;
reply delivered to the wrong context;
early context reuse;
wrong curve-table port/tag;
root-bank selection;
prepared-ring rounding removed or fused;
field-order transposition;
output reducer swapped;
masked vertex incorrectly written;
prediction counters that undercount real demand.

Formal properties should include:

no context is lost or duplicated
one context has at most one instruction in flight
every service reply returns to its issuing context
no context is reused before all writes complete
field command order is preserved per vertex
every accepted association produces exactly one result per active vertex
Final ruling

Do not spend another serious pass optimizing Field v2’s individual critical paths.

The current work is not wasted. v2 supplied:

exact arithmetic units;
the four-lane proof;
the register-file inference solution;
exhaustive differential fixtures;
mutation infrastructure;
real resource measurements;
the evidence that local timing fixes plateau.

But the production architecture must be v3:

Canonical program → exact ARM preparation → derived vector plan → direct profile walker → queued four-wide execution → field-major in-place patch reduction.

That solves all four real problems together:

frequency is addressed by intentional stage boundaries;
long-unit throughput is addressed by vector/barrel services;
point transport disappears;
the Field/Terrain ordering seam becomes natural.

Most importantly, future Field programs acquire an honest admission test. A program is not “fast” because its opcode count is below 32. It is fast only when its complete resource-demand vector fits the measured machine.

That is the lasting solution.
