# R3's OWED SCHEDULE / BANDWIDTH PROOF — CLIENT A OF THE ONE PROJECTOR

> Lane SHADOWCLOSE, 2026-09-23, from `220c67fd`. Commissioned by owner ruling
> **R244 D-FORGESHADOW-C**, which names this a deliverable of the FORGE.SHADOW
> subsystem packet rather than an optional extra.
>
> Every figure below is cited to a file and a line in this tree. Where a number
> is DERIVED rather than ruled, it says so. Where a half cannot be measured
> today, it says that, rather than quoting the half that can as though it
> settled both — which is the shape `design/contracts/FORGE.SHADOW.md:337-345`
> caught this file's predecessor in.

## WHAT IS OWED

`reports/OWNER-RULINGS-20260919-EVENING.md:14`, ruling **R3 `(owner, explicit)`**:

> *"Third projector port for particles / FORGE.SHADOW (I24, docket §2). **Keep
> the time-multiplex (owner, explicit).** No third port in v1. Owed: **a written
> schedule proof** that geometry, particles and FORGE.SHADOW's instance-centre
> 1/w share client A's bandwidth within the frame at the guaranteed content
> tier."*

## FINDING 0 — R3 NAMES THREE SHARERS. THE TREE HAS FOUR, AND THE CODE R244 DIRECTS THIS PACKET TO CLAIM IS ALREADY SPENT.

**This is the first thing the proof has to say, because it changes what is being
proved.**

R244 directs this packet to build *"a third request arm on `zhao_part_project`
claiming `OWNER_LOD = 2'd2`"*, on the strength of
`design/contracts/FORGE.SHADOW.md:319` — *"`2'd2` and `2'd3` unallocated"*.

**`2'd2` is allocated.** `fpga/rtl/particles/zhao_part_project.sv:475`:

    localparam logic [OWNER_W-1:0] OWNER_FORGE = 2'd2; // FORGE.PRIM's vertices

It was claimed on 2026-09-21 by the FORGECOMP packet (`zhao_part_project.sv:304`),
and the arm is **composed and live**: `zhao_console_core.sv:19371-19378` wires
`f_*` to `zhao_forge_assemble` (instantiated at `:13299`), under the console's
own note at `:19366-19370`:

> *"the THIRD owner on this front mux — FORGE.PRIM's vertices, under
> `OWNER_FORGE = 2'd2` … Owner ruling R3 is untouched: it withholds a third PORT
> on `zhao_project_service`, and this is a third CLIENT on the multiplex R3
> names as the thing to keep."*

**The block's own comment two lines below that localparam still says otherwise**,
and it is the comment the contract and R244 were both reading —
`zhao_part_project.sv:476-479`:

> *"2'd2 and 2'd3 are UNCLAIMED. 2'd2 is reserved for GEOM.LOD's instance
> centre (`zhao_geom_lodstate.pr_*`)…"*

A localparam and the comment beneath it disagreeing, in the file whose whole
header is about not letting an owner encoding drift. Repaired in this packet.

### What that means, and what it does NOT mean

* The instance centre's owner code is **`2'd3`, the last one**. The 2-bit field
  is then **exhausted**; a fifth client-A owner is a `GEOM_OWNER_W_C` widening
  and the console's live elaboration guard at `zhao_console_core.sv:10607` will
  say so — loudly, which is what it is for.
* **R3 is still not disturbed, and the escalation clause is not reached.** R3
  withholds a third *port on `zhao_project_service`*; the service still has
  exactly two client arms (`zhao_project_service.sv:155-195`). A fourth *client*
  on the front multiplex is the same act the console already performed for
  FORGE.PRIM and justified in writing against R3 at `:19366`. The owner's clause
  — *"if closing the subsystem actually requires changing that ratified law
  rather than using the already-authorized multiplex"* — is not triggered: the
  multiplex is exactly what is being used, and the reserved encoding still has
  the room. **This finding is reported, not escalated.**
* **R3's proof must be computed at FOUR sharers.** R3's sentence lists three.
  FORGE.PRIM joined two days ago and nobody re-asked the question. That is the
  substance of this document.

## THE MACHINE UNDER MEASUREMENT

Read from the composed console, not from a contract:

| fact | value | evidence |
|---|---|---|
| frame compute budget | **1,666,666 clocks** (100 MHz / 60 Hz, conservative floor) | `design/budgets/workloads.yml:41,51` |
| physical projector cores | **one** | `zhao_project_service.sv` header; `zhao_proj_subsystem.sv:191` |
| core initiation interval | **1 vertex / clock** (`ROWS_PER_PASS = 3`, not overridden at `zhao_console_core.sv:14859`) | `zhao_project_service.sv:129-137` |
| service client arms | **two** (A, B); round-robin, toggling priority | `zhao_project_service.sv:112-119` |
| client A front multiplex | `zhao_part_project`, rotating priority, **bound N-1 turns** | `zhao_part_project.sv:600-641` |
| the turn advances only on an ACCEPTED grant | `if (any_c && a_ready_i)` | `zhao_part_project.sv:918-920` |
| a losing arm HOLDS its request | `*_ready_o = a_ready_i && <granted>` | `zhao_part_project.sv:684-687` |
| client A rider | `GEOM_PAY_A_W = 17` = 2 owner + 3 arena + 12 index | `zhao_console_core.sv:7061`, guard `:10607` |

**The one core is shared by BOTH service clients.** The ceiling is therefore not
"client A's share of the frame" but *A + B ≤ one grant per clock*, and a proof
that ignores terrain is not a proof. This is the correction
`design/budgets/workloads.yml:299-312` already forced once, from owner brief
2.6.E; it applies again here.

## THE RATE HALF — ANSWERED, WITH MARGIN

Demand is counted in **grant-clocks**. The arbiter is work-conserving and the
core takes one vertex per clock, so one projection is exactly one clock of the
shared resource, and grant-clocks are strictly additive across all four client-A
arms and client B.

### Client B — terrain

| | projections / frame |
|---|---|
| without the vertex arena | 1,572,864 (256 visible patches × 6,144 corners) |
| **with the arena (composed)** | **278,784** (256 × 1,089 unique lattice vertices) |

`design/budgets/workloads.yml:243-260`. The arena is composed —
`zhao_proj_subsystem`, `zhao_console_core.sv:14859`, `GEOM_DEPTH = 1089`
(`:7054`).

### Client A — the four arms

| owner | arm | block | projections / frame | source |
|---|---|---|---|---|
| `2'd0` `OWNER_GEOM` | `g_*` | GEOM.GROUP_SEQ | **120,000** | `workloads.yml:277,290` — *ruled* (120,000 weighted vertices/frame) |
| `2'd1` `OWNER_PART` | `p_*` | PART.PROJECT | **65,536** | `zhao_part_project.sv:79` — 32,768 required tier (`PART.STATE.md:124`) × 2 views |
| `2'd2` `OWNER_FORGE` | `f_*` | FORGE.PRIM / `zhao_forge_assemble` | **≤ 204,800** worst case | `FORGE.PRIM.md:151` — 100 maximal primitives × 2,048 vertices |
| `2'd3` *(this subsystem)* | *new* | GEOM.LODSTATE instance centre | **256** | below |

**The instance centre's demand is 256 projections per frame, and it is one
request per evaluation, not a stream.** `zhao_geom_lodstate.sv:277` —
`assign pr_valid_o = (st_w == S_PROJ)` — is a **single-in-flight FSM**: one
instance is evaluated at a time and it offers the client-A port exactly once
(state `S_PROJ`, `:243`) before moving to `S_RAD`. The tick rate is **one per
instance per frame** (`:53-70`) and the content tier is **256 instance
transforms** (`:121`, matching `zhao_geom_drawjob`'s XFORMS under owner ruling
R29).

*If the docked per-camera deviation (`zhao_geom_lodstate.sv:73-86`) is ever ruled
the other way this becomes 512 — `design/prod_manifest.yml:1531` already prices
the radius service at that figure. **Both are negligible and the proof holds
either way**, which is worth stating because it means R3's answer does not depend
on that undecided owner question.*

### The sum

    client B  terrain (with arena)                      278,784
    client A  geometry                                  120,000
    client A  particles (32,768 x 2 views)               65,536
    client A  FORGE.PRIM (100 maximal primitives)       204,800
    client A  GEOM.LODSTATE instance centre                 256
                                                      ---------
    TOTAL grant-clocks per frame                        669,376
    of 1,666,666                                          40.2 %

Against the **20 % reserve** `workloads.yml` applies to every projection row
(`reserve: 0.20`, e.g. `:261`) the usable budget is 1,333,333 clocks and the
total is **50.2 %**.

**The marginal cost of admitting the instance centre is 256 clocks — 0.0154 % of
the frame.** Without it the same sum is 669,120 (40.15 %). At the per-camera 512
it is 0.0307 %.

**Steady state, with FORGE.PRIM idle** — a spell-heavy frame is the case to
watch, not the resting one (`FORGE.PRIM.md:152`): 464,576 clocks, **27.9 %**.

### A CORRECTION TO `zhao_part_project.sv:76-84`'s OWN ARITHMETIC

That header computes *"the composed total is about 41.6%"* by adding **295,000
clocks** for particles — 65,536 projections divided by the `SLOTS = 8` in-flight
cap of 8/36 = 0.22 particles per clock.

**That 295,000 is the particle client's ELAPSED time, not its bandwidth.** A
client throttled by its own in-flight limit occupies the port for 65,536 grant
clocks and leaves the other ~230,000 free for the arms beside it; the arbiter is
work-conserving and grants whoever is asking. Adding elapsed time to other
clients' grant-clocks double-counts idle cycles.

The error is **in the pessimistic direction**, so nothing was built on a
flattering number and the header's conclusion (one core is affordable) is
unaffected. It is corrected here because a wrong number that happens to be safe
is still the number the next reader will quote. The composed total is **40.2 % at
four arms**, not 41.6 % at three.

**What the elapsed figure DOES bound, correctly:** particles finish their 65,536
projections in ~295,000 clocks (17.7 % of the frame), so the throttle does not
make them miss a frame. `SLOTS` remains the frontier knob its declaration says it
is (`zhao_part_project.sv:230-237`).

**VERDICT, RATE HALF: PASSES**, with roughly 60 % of the frame unused at the
worst case and the instance centre costing 0.0154 % of it.

## THE FAIRNESS HALF — A BOUND, PROVED STRUCTURALLY; THE MEASUREMENT IS STILL OWED

R3's arbiter law is a **bound, not an average**, and
`zhao_part_project.sv:613-616` says so in terms: *"a fixed priority would let two
busy clients starve the third for an unbounded time, and R3's schedule proof is
owed against a BOUND, not against an average."*

### The bound, at N arms

Two arbitration layers, each with a law read from its RTL:

1. **Front multiplex** (`zhao_part_project.sv:620-641`): rotating priority,
   scanning cyclically from `turn_q`. The turn advances **only on an accepted
   grant** (`:918-920`), so a holder is passed by **at most N-1 accepted grants**
   before it is served. A loser's `*_ready_o` stays low (`:684-687`), so its
   request is HELD, never dropped.
2. **Service** (`zhao_project_service.sv:112-119`): round-robin at two with
   toggling priority. With both saturated *"each gets every other clock"*, so a
   client-A grant costs **at most 2 core clocks**.

Worst-case wait from a client-A arm asserting `valid` to being accepted:

    (N-1) other accepted grants  x  <=2 clocks each   +  <=2 for its own
      N = 3 (today)       2 x 2 + 2  =  6 clocks
      N = 4 (this arm)    3 x 2 + 2  =  8 clocks

**The marginal fairness cost of the fourth arm is 2 clocks of worst-case wait,
for every arm including itself.**

### Does GEOM.LODSTATE's evaluation still close?

`zhao_geom_lodstate.sv:104-112` budgets **200 clocks** per evaluation and itemises
it: bank lookup 2 + centre projection (36-clock core + arbitration) + radius 121
+ ladder 5 = **164 + arbitration**. At the bound above that is **172 of 200**, and
256 evaluations × 200 = 51,200 clocks = **3.1 % of the frame** — the block's own
figure, unchanged by the fourth arm.

`dropped_o` (`zhao_geom_lodstate.sv:207`) is the counter that says whether that
reasoning held on real traffic, and `zhao_project_service`'s `contended_o` is the
one that says whether two clients' combined demand approached the one-per-clock
limit.

### AND IT IS NOW MEASURED AT N=3, WHICH IS WHAT MAKES THE N=4 PREDICTION EVIDENCE

`design/contracts/FORGE.SHADOW.md:509-516` records why this could not be done:

> *"The RATE half needs a bench holding `zhao_part_project` against the real
> `zhao_proj_subsystem` with every arm saturated. `tb_part_project` drives the
> block **standalone** … so the composed multi-client throughput has never been
> measured, and cannot be from any bench in this tree until the third arm
> exists."*

The first two sentences were true (`tests/CMakeLists.txt:15956-15962`, checked).
**The third was not: the third arm landed on 2026-09-21.** The bench is built in
this packet — `tests/common/tb_projshare.sv` and
`tests/common/projshare_contention.cpp`, registered as `projshare_contention` —
and holds the **real** `zhao_part_project` against the **real**
`zhao_project_service` and `zhao_project_core`. The only thing modelled is
PART.LADDER's one-deep skid, because an open ladder loop would wedge the
particle arm and the bench would measure a stall it invented.

*(It instantiates `zhao_project_service` rather than `zhao_proj_subsystem`: the
subsystem passes **both** client arms straight through to it, unregistered —
`zhao_proj_subsystem.sv:203-234`, the only transformation being
`b_payload_i({b_arena_i, b_index_i})`. The arbitration is therefore identical
and the shell would add fifty tie-offs that can only obscure it. The citation is
there so a future reader can check that the shell still does not register an
arm.)*

**120,000 clocks per case. Every number below is measured, not derived.**

| case | load | measured accepts | worst wait |
|---|---|---|---|
| **1** | all four saturated | geom **20,000** · part **20,000** · forge **20,000** · terrain **60,000** | client A arms **5**, terrain **1** |
| **2** | three client-A arms, terrain idle | geom **48,571** · part **22,858** · forge **48,571** | geom/forge **2**, part **20** |
| **3** | geometry + terrain only | geom **60,000** · terrain **60,000** | **1** each |
| **4** | **one INTERMITTENT client-A asker** (1 request / 200 clocks) against geometry, particles and terrain all saturated | **600 of 600 requests served** | **4** |

**The bound holds in every case, and it is tight.** Predicted 6 at N=3 with
terrain saturated; measured 5. The core was granted on **100 % of clocks in
every case** — work-conserving, so grant-clocks are additive and the sum in the
rate half is the right kind of sum. `a_grants_o + b_grants_o = 120,000` exactly,
in a case where both clients asked on every clock.

`geom_tag_collision_o`, `owner_unroutable_o`, `ladder_unexpected_o` and
`mat_refused_o` are asserted silent throughout, because a contention bench that
fires one is measuring a **routing** fault and reporting it as a **rate**.

### THREE THINGS THE MEASUREMENT SAYS THAT THE ARITHMETIC DID NOT

**1. CASE 4 is the answer to R3's actual question.** The other three cases
saturate everything, which is the worst load but the wrong SHAPE.
`zhao_geom_lodstate` is a single-in-flight FSM: it asks **once**, waits, spends
~164 clocks on the radius and the ladder, and asks again. Driven in that shape
against a fully saturated machine, **every request was served and the worst wait
was 4 clocks.** The evaluation closes at 4 + 36 + 121 + 5 + 2 = **168 of its 200
budgeted clocks**, measured rather than assumed. *That is the fairness half of
R3's proof, at the load the arm will actually present.*

**2. Client A's arms share HALF the core, not all of it.** In CASE 1 each
client-A arm took exactly **1/6** of the clocks while terrain took **1/2** — the
service's A/B round-robin halves client A *before* the front multiplex divides
it three ways. Nothing in the tree said this, and a reader reasoning from the
front multiplex alone would predict 1/3. **It does not threaten the rate half**
(total demand is 40.2 %, so no arm is rate-limited) but it is the number that
matters if client A's demand ever approaches half the frame.

**3. `SLOTS = 8` is the binding constraint only when terrain is idle**, and the
block's own model of it is validated. In CASE 2 the particle arm took
22,858/120,000 = **0.190 particles per clock** against
`zhao_part_project.sv:81`'s predicted `8/36 = 0.222` — the difference being the
ladder round trip the header's figure leaves out. Its worst wait of **20
clocks** is that round trip, **not** arbitration, which is why this document
does not assert the rotating-priority bound against it. In CASE 1 the throttle
does **not** bind at all: client A gets 0.5 clocks per clock split three ways =
0.167 each, below the 0.222 cap. *A frontier knob that binds in one load shape
and not the other is worth knowing before anyone spends ALMs raising it.*

### WHAT IS STILL OWED

| leg | status |
|---|---|
| grant-clock sum at four arms | **DONE** — above |
| structural starvation bound at N=4 | **DONE** — above |
| measured grant distribution and worst-case wait at N=3, composed | **DONE** — `projshare_contention`, four cases |
| the same at N=4, with `zhao_geom_lodstate`'s FSM closing 200 clocks under real contention | **owed at the commit that adds the arm.** The bench is built and its fourth case is already the right shape; adding the arm is a `run_intermittent` call away from covering it. |

## A FIFTH DEMAND NOBODY HAS COUNTED, AND THERE IS NO CODE LEFT FOR IT

Counted here so the next lane does not discover it.

The **shadow hull's own vertices** must also reach screen space. Route B's chosen
shape is the forge's (`design/contracts/FORGE.SHADOW.md:358-368`: *"a shadow
hull's route is the particle's, not terrain's"*), which means world vertices into
**client A** under an owner. At `VTX_HERO = 16` vertices per hero-rung hull
(`zhao_forge_shadow.sv` parameter block) and 256 casters, that is a further
**4,096 projections — 0.25 % of the frame** at the worst rung for every instance
simultaneously, which the ladder exists to prevent.

It is inside the margin above by two orders of magnitude. **The constraint is not
bandwidth, it is the ENCODING**: the instance centre takes `2'd3` and the field is
then full, so the hull vertices must either share an owner code with the centre
(distinguishable by the rider's arena/index bits, which the centre does not use)
or ride a widened `GEOM_OWNER_W_C`. That is a design question for the arm's
commit, and the elaboration guard at `zhao_console_core.sv:10607` is what will
refuse the lazy answer.

## WHAT THIS PROOF DOES NOT CLAIM

* It does not price the **arena/replay cost itself**, which
  `design/budgets/workloads.yml:304-306` is explicit about: *"Any cache or replay
  used to avoid that re-projection is itself a cost, in cycles and in memory
  ports."* Client B's 278,784 is the projection count only.
* It says nothing about **area or Fmax**. No fit was run (a fit and the BURSTTRUTH
  lane were live). Widening a 3-way combinational scan to 4-way is an unmeasured
  ALM claim.
* It is a **frame-scale** argument. It does not claim any particular instant is
  uncongested; it claims the per-frame demand fits and the per-request wait is
  bounded. `proj_contended_o` is the port that reports the instants.

---

*Sources read first-hand at `220c67fd`: `zhao_project_service.sv`,
`zhao_proj_subsystem.sv`, `zhao_part_project.sv`, `zhao_geom_lodstate.sv`,
`zhao_forge_shadow.sv`, `zhao_console_core.sv`, `design/budgets/workloads.yml`,
`design/contracts/PART.STATE.md`, `design/contracts/FORGE.PRIM.md`,
`design/contracts/FORGE.SHADOW.md`, `design/prod_manifest.yml`,
`reports/OWNER-RULINGS-20260919-EVENING.md`, `tests/CMakeLists.txt`.*
