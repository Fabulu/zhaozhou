# Contract — MEASURE.TOKENS (Geometry/fragment token guard)

> Ledger: `design/blocks.yml` · owner ZH-048 · phase 8 · maturity SPECIFIED
>
> RTL: `fpga/rtl/measure/zhao_measure_tokens.sv`
> Oracle: `reference/include/zref/zref_measure.hpp` (`zref::measure::TokenGuard`)
> Tests: `tests/measure/measure_tokens_directed.cpp`,
> `tests/measure/measure_tokens_random.cpp`,
> `tests/formal/measure_tokens_fairness.sby`

## Purpose and exclusions

The global token guard: it admits or refuses each unit of geometry and fragment
work against the frame's five declared budgets, and it enforces the charter §9
Duo fairness split so that neither player's guaranteed allowance can be spent by
the other.

**In:** the five per-frame budgets from `SetPresentationContract`; a
combinational request/grant pair; a token return path; a registered denial
report; the five live pool levels; the counters.

**Out of scope, deliberately:** no priority heap (charter §9: *"Do not begin
with a global FPGA priority heap"* — Version 1 is a guard, not a scheduler); no
error histogram and no cutoff bucket (MEASURE.HISTOGRAM, charter Version 2); no
re-submission of denied work to the next frame (GEOM.BINNER's law D names that
as unbuilt and this block does not build it either — it *reports* that it
happened); no per-request cost model (the cost arrives on the wire; deriving it
from a meshlet descriptor is GEOM.MESHFETCH's job); no memory of any kind
beyond five pools and five budgets.

## THE SEAM — found in a LANDED block, not invented here

GEOM.BINNER shipped in phase 6 with the guard's other half already written.
Its law E says:

> *"one combinational request/grant pair. `tok_req_o` pulses on the cycle a
> triangle is accepted and `tok_grant_i` is sampled on that same edge; a denied
> triangle is dropped and counted into `triangles_culled`. Tie `tok_grant_i`
> high and the guard is absent. Deliberately NOT invented here: the 45/45/10
> Duo fairness split, any token WIDTH or cost model, and the return path — all
> of those are MEASURE.TOKENS' law to write."*

So `tok_grant_o` **is combinational from `req_valid_i`**, and the ledger's
`latency: fixed:1` is *not* the grant's latency.

**LEDGER DEVIATION, recorded rather than resolved by fiat.** The alternative —
a registered grant one cycle after the request — would require re-opening a
landed, tested, lint-clean block to add a stall state, contradicting a law that
block wrote down deliberately. `latency: fixed:1` is honoured by the *other*
output: `token_denial` is registered and presents exactly one cycle after the
refused request. Both halves are MEASURED, not asserted
(`measure_tokens_directed.cpp:test_latency`).

**The second declared consumer has no port.** The ledger lists
`downstream: [GEOM.BINNER, RASTER.FRAGMENT]`, but `zhao_raster_fragment.sv`'s
only signal matching `token` or `error` is `fragment_error_o`, a one-bit
tilestore-read protocol flag. The fragment class is therefore built and served
here exactly as the geometry class is, and its consumer side is named as
MISSING rather than faked.

**`reference_model` DOES NOT RESOLVE.** `zref::MeasureTokens` names nothing in
this tree and never has — the eighth phantom after `zref::CmdDma`,
`zref::SurfaceStamp`, `zref::SurfaceSheet`, `zref::AuxSource`,
`zref::TerrainBake`, `zref::TerrainVelocity` and `zref::ProgCache`. Amended in
`design/blocks.yml` to `zref::measure::TokenGuard`, the oracle written for this
block. Unlike `zref::terrain::velocity_vertex`, that oracle is **not** a view
onto an executed reference: nothing in `reference/src` allocates tokens, the
software console renders whatever it is given. The oracle IS the semantics, so
every law below is carried here rather than inherited.

## Laws found

1. **The pool set's SHAPE is the ABI's.** `SetPresentationContract 0x0020`
   carries `u32 geometry_tokens[2]`, `u32 fragment_tokens[2]`, `u32
   shared_tokens` — four private pools and **one** shared pool spanning both
   views *and* both classes. The shared pool is a scalar because the ABI field
   is a scalar; that is not a choice made here.
2. **The fairness split is structural.** Charter §9: *"45% guaranteed to player
   1; 45% guaranteed to player 2; 10% shared emergency pool. One player looking
   directly into a volcano cannot make the other player's army disappear."*
3. **The admission rule's shape.** Charter §9 Version 1: *"a global token guard
   rejects only low-priority refinement when the budget is nearly exhausted."*
4. **Counters saturate, never wrap** (`spec/counters.md` §4), and adding a
   counter is a ledger edit, never an RTL whim (§2). This block therefore adds
   none: it drives the two the ledger already gives it.

## Laws CHOSEN, not found

Each is numbered identically in the RTL header and in the oracle.

**T1. The private pool is tried FIRST, always; the shared pool is a fallback,
and only essential work may reach it.**
Charter §9 gives three pools, calls the third an *emergency* pool, and gives the
admission rule *"rejects only low-priority refinement when the budget is nearly
exhausted"*. Together those two sentences have exactly one consistent reading:
low-priority refinement is confined to its view's own pool, so when that pool
nears exhaustion refinement is what stops; essential work spills into the
emergency pool and keeps going. The charter never spells this out, so it is
CHOSEN.
*Rejected:* (a) **shared-pool-first** — it burns the emergency reserve on the
frame's first requests, leaving no emergency for the work the reserve exists
for; (b) **proportional draw**, taking part of a request from each pool — it
splits one request's accounting across two pools, so the return path must carry
two numbers and a partial return can inflate one pool while starving the other;
(c) **letting low-priority work reach the shared pool too** — then the guard
rejects HIGH-priority work when the budget is nearly exhausted, which is the
charter's sentence backwards.

**T2. A view's private pools are unreachable by the other view.**
This is the mechanical content of the volcano sentence and it is what makes the
word *guaranteed* mean something. It is not a policy knob: there is no path in
the RTL from `req_view_i == 1` to `avail_geom0_o`.
*Rejected:* a **work-conserving guard** that lends an idle view's unspent tokens
to the busy one. It renders more, and it destroys the guarantee — a player who
looks at the sky for two frames and then turns around finds their budget already
spent. The charter chose the guarantee over the throughput and so does this
block.
*Enforced by:* `tests/formal/measure_tokens_fairness.sby` — `a_view0_sealed` /
`a_view1_sealed`, proved by **temporal induction on the shipping 32-bit
instance**, so it holds for all time rather than for a bounded prefix.

**T3. The 45/45/10 numbers are NOT in the RTL.** They arrive on `budget_*_i`
from `SetPresentationContract`. The charter's split is a POLICY the producer
sets; the guard ENFORCES whatever five numbers it is handed.
*Rejected:* hardcoding 45/45/10 and taking one total — it would make a ratified
ABI field dead, and it would put a percentage-to-token division inside the
guard's combinational grant path.
*Observed and NOT ratified here:* the Nanquan compiler currently writes
**percentages** into these u32 fields (`record.payload.geometry_tokens[0u] =
80u`, `compiler/src/backends/cpp/emitter.ts`), while this block reads them as
absolute token counts. The block is unit-agnostic — the guard is correct under
either reading, since it only ever compares a cost against a budget in the same
unit — but **the two producers do not agree with each other**, and that is
written down here rather than papered over. Whoever ratifies the unit should
amend this section; nothing in this block needs to change either way.

**T4. A return names the pool its grant drew from.** `tok_shared_o` is presented
with the grant; `ret_shared_i` echoes it back.
*Rejected:* returning to the private pool first and spilling to shared. That
MOVES tokens permanently from the shared pool into a private one — draw from
shared, return to private — and after enough round trips a private pool would
sit at its budget while the emergency pool emptied, with no event anywhere
marking it. The echo costs one wire.

**T5. A return cannot inflate a pool past its budget.** Excess is dropped and
the pool clamps.
*Rejected:* trusting the return. A single malformed or duplicated return would
raise a view's spendable budget above its guarantee — the exact thing T2 exists
to prevent — so the clamp is a safety property, not tidiness.
*Enforced by:* `a_within_budget` (formal, inductive) together with
`a_budget_latched`; and directly at the boundary in the directed lane
(a credit landing exactly on the budget, one token past it, and `0xFFFFFFFF`).

**T6. A return arriving in the same cycle as a request does not help it.** The
grant is decided against the pools as they stand; the return lands for the next
cycle.
*Rejected:* forwarding the return combinationally into the decision. It would
put an adder in front of the comparator on a path GEOM.BINNER already samples
combinationally in its accept cycle, and it would make the grant depend on a
signal that consumer does not produce. The two updates still compose exactly —
one subtract and one add on the same pool in the same cycle, `5 − 3 + 4 == 6`,
checked directly — so nothing is lost but the forwarding.

**T7. `req_rep_i` is a port**, because `lod_representation_counts` has to come
from somewhere. Charter §9's ladder has seven rungs (full hierarchical form,
reduced mesh hierarchy, rigid/simplified combat form, tiny micro-mesh,
depth-aware splat cluster, animated faction glint, culled); three bits carry
them with one spare, and the counter is GRANTS per rung. That is the Measure's
own vocabulary, and §9 puts this block in charge of it.
*Rejected:* mapping the four (view, class) pairs onto the four lanes
TERRAIN.LOD uses. Those are not representations, and a counter whose name lies
is worse than a counter that is missing.

**T8. A denied GEOMETRY request adds its COST to `triangles_culled`.** Charter
§9 calls a geometry token an *"approximate triangle/vertex/fragment cost"*, so a
geometry token IS a triangle in the charter's own approximation, and refusing N
of them culls N triangles. A denied FRAGMENT request adds nothing — fragments
are not triangles.
*Rejected:* counting one per denial regardless of cost. It would report a
refused 500-triangle meshlet and a refused single triangle identically, which is
exactly the distinction a budget post-mortem needs.

**T9. Nothing is silent.** A request refused because a budget load is landing in
the same cycle is refused with `den_reason_o = REASON_RELOAD` (2), not dropped.
The frame-boundary protocol makes that unreachable in real traffic; it is
defined anyway, because "unreachable" is a claim about the producer and this
block does not get to make claims about producers.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset zeroes the five budgets, the five pools, the denial register and every
counter. **A reset therefore refuses every non-zero request until a budget load
arrives**, which is the conservative direction: an unconfigured guard grants
nothing rather than everything. A zero-cost request is still granted (0 ≤ 0).
No clock-domain crossing lives here.

## Input and output packet layouts

`dispatch` in (CMD.SCHEDULER — the `SetPresentationContract` numbers). A
one-cycle `budget_valid_i` pulse loads the budgets **and refills every pool to
them**: a frame starts with its whole allowance, which is what "per-frame
budget" means.

| field | width | meaning |
|---|---|---|
| `budget_geom0_i` `budget_geom1_i` | 32 | `geometry_tokens[0..1]` |
| `budget_frag0_i` `budget_frag1_i` | 32 | `fragment_tokens[0..1]` |
| `budget_shared_i` | 32 | `shared_tokens` — one pool, both views, both classes |

The request — **combinational in, combinational out**:

| field | width | meaning |
|---|---|---|
| `req_valid_i` | 1 | GEOM.BINNER's `tok_req_o` |
| `req_view_i` | 1 | which player |
| `req_class_i` | 1 | 0 = geometry, 1 = fragment |
| `req_essential_i` | 1 | 0 = low-priority refinement, which never reaches the reserve |
| `req_rep_i` | 3 | charter §9 ladder rung |
| `req_cost_i` | 32 | tokens |
| `req_src_id_i` | 16 | `source_ids: true` |
| `tok_grant_o` | 1 | GEOM.BINNER's `tok_grant_i`, SAME cycle |
| `tok_shared_o` | 1 | the grant drew the emergency pool |

`token_return` in: `ret_valid_i`, `ret_view_i`, `ret_class_i`, `ret_shared_i`
(the echo T4 asks for), `ret_cost_i` (32).

`token_denial` out, **registered, one cycle later**: `den_valid_o`,
`den_view_o`, `den_class_o`, `den_rep_o` (3), `den_reason_o` (2 — 0
LOW_PRIORITY, 1 EXHAUSTED, 2 RELOAD; 3 is unused and never presented),
`den_src_id_o` (16), `den_cost_o` (32).

Plus the five live pools (`avail_geom0_o`…`avail_shared_o`), the eight
`lod_representation_counts` lanes and `triangles_culled_o`.

## Backpressure rules

`credit`, per the ledger — and the token accounting **is** the flow control.
There is no ready/valid on the request port: `tok_grant_o` low is the
backpressure, and a refused request is reported on `token_denial` rather than
held. That is GEOM.BINNER's protocol as it already ships (a denied triangle is
dropped and counted, not retried), and the block adds nothing to it. The
denial port has no `ready`: it is a one-cycle report, not a queue.

## Memory ownership

**None.** No VRAM port, no cache, no M10K. The block's entire state is five
32-bit budgets, five 32-bit pools, one denial register and nine 32-bit
counters — 458 flops.

## Q formats and rounding

**There is no rounding anywhere in this block, and that is the point.** Pools
and costs are unsigned 32-bit integers (the ABI's u32), every comparison is
unsigned between two same-width operands, and every update is an exact integer
add or subtract. A token guard whose accounting rounded would leak or invent
tokens over a frame, and the fairness guarantee would be approximate.

## Latency (fixed or variable)

**The grant: 0 (combinational)** — the landed GEOM.BINNER seam. **The denial:
`fixed:1`** — the ledger's number, measured directly. See *THE SEAM* above for
why the deviation exists and why it was not resolved by changing the landed
block.

## Target throughput

`1 grant decision per clock`, **MEASURED**: 4,096 back-to-back requests in
4,096 clocks with no bubble anywhere = **1.000 clocks/decision**. There is no
internal stall state that could produce anything else — the decision is a
comparator and a two-way mux.

## Overflow and malformed-input behaviour

- **The debit cannot underflow.** A grant guarantees `req_cost_i ≤` the pool it
  draws, so the subtract is safe. Asserted in simulation under `ZHAO_ASSERT`
  and proved for all time by `a_no_underflow`.
- **The credit cannot wrap.** It is formed at 33 bits and clamped to the
  budget (T5).
- **A malformed return** (more than was granted, a duplicated return, a wrong
  `ret_shared_i` echo) can only ever move tokens *within* the declared budgets:
  the clamp bounds every pool by its own budget, so the worst a bad return can
  do is refill a pool early. It can never raise a view's allowance.
- **A request on a reload cycle** is refused with `REASON_RELOAD` (T9).
- **`req_cost_i == 0`** is granted against any pool including an empty one, and
  spends nothing.
- **Counters saturate** at `0xFFFF_FFFF` rather than wrapping
  (`spec/counters.md` §4). `triangles_culled_o` saturates on a *wide* add, which
  is a different check from a wrapping increment and is tested at the rail.

## Counters and traces

- `lod_representation_counts` — eight lanes, `tok_rep_count0_o`…`7_o`: GRANTS
  per charter §9 ladder rung (law T7).
- `triangles_culled` — `triangles_culled_o`: the summed COST of denied
  **geometry** requests (law T8).

The `lod_representation_counts` rail (2³² grants) is not reachable in
simulation; it is covered by the shared saturating adder, which
`triangles_culled_o` drives onto its rail directly in the directed lane.

## Scalar reference function

`zref::measure::TokenGuard` in `reference/include/zref/zref_measure.hpp`.
`step()` is one clock: it returns the combinational answer computed against the
pools *as they stand*, then applies the debit and the credit. Called once per
cycle with the same stimulus the RTL sees, it reproduces the RTL bit-for-bit
including every counter — which is exactly how both random lanes drive it.

## Directed tests

`tests/measure/measure_tokens_directed.cpp` — 101,939 checks, all green. Every
case CONSTRUCTS the value it is about.

1. **Exact fit / one over** — `cost == avail` grants and empties the pool;
   `cost == avail + 1` denies and spends nothing. *Red on:* `<` where the law
   says `≤`.
2. **Zero cost against an empty pool** — granted (0 ≤ 0); one token more is
   refused.
3. **The emergency pool at its boundary** — `cost == shared` grants and is
   tagged shared; `cost == shared + 1` denies with reason EXHAUSTED; and a cost
   that fits privately does NOT touch the reserve.
4. **Low priority is confined** — refinement is refused with a 1,000,000-token
   reserve sitting untouched beside it; the identical cost marked essential is
   admitted from that reserve.
5. **THE VOLCANO** — view 1 drains its own 450-token pool *and* the whole
   reserve, is then refused everything, and view 0 spends all 450 of its
   guaranteed tokens one at a time with every request granted.
6. **The return path** — exact return, one past the budget, and `0xFFFFFFFF`,
   all clamping at the budget; and a shared draw returning to the shared pool
   with the private pool left where it was.
7. **Same-cycle return** — a credit that would make the request fit does not
   make it fit; the identical request succeeds one cycle later; and
   `5 − 3 + 4 == 6` on one pool in one cycle.
8. **Latency** — nothing in the request's own cycle, the denial with its whole
   payload exactly one cycle later, and gone the cycle after.
9. **The reload collision** — no grant, the new budgets landed, reason RELOAD.
10. **Counters** — the eight rungs counted `rep+1` times each so a swapped lane
    shows; a denied geometry request culling its COST; a denied fragment
    culling nothing; and the rail reached and held.
11. **Sustained rate** — 4,096 decisions in 4,096 clocks.

## Randomized differential tests

`tests/measure/measure_tokens_random.cpp` — 2,818,046 checks, all green, two
lanes of 60,000 cycles each (400,000 under `--nightly`).

- **Lane A, the workload:** the charter's 45/45/10 as tokens, meshlet-sized
  costs, both views busy, a minority of essential work, returns of amounts
  really granted, a reload at each frame boundary. 42,314 grants (2,444 from the
  reserve), 10,116 denials, **0 mismatching cycles**.
- **Lane B, the domain limit:** budgets and costs within a whisker of 2³², so
  the wide add, the clamp and the counter rail are live on most cycles. 32,957
  grants (4,358 from the reserve), 19,555 denials, **0 mismatching cycles**.

**The exact equalities are CONSTRUCTED.** Uniform random `cost` against a
32-bit pool hits `cost == avail` with probability 2⁻³², so on a fraction of
cycles the lane reads the pool and builds the value. Reached, both lanes:

| construction | lane A | lane B |
|---|---|---|
| E1 `cost == avail` | 3,330 | 3,361 |
| E2 `cost == avail + 1` | 3,309 | 3,096 |
| E3 `cost == shared`, private short | 781 | 1,760 |
| E4 one over the reserve | 860 | 2,047 |
| E5 credit landing exactly on the budget | 3,757 | 3,754 |
| E6 credit one token into the clamp | 3,702 | 3,731 |
| E7 debit and credit, same pool, same cycle | 5,726 | 5,208 |
| E8 zero cost against an empty pool | 1,134 | 9,888 |
| credits actually clamped | 3,702 | 3,731 |
| reload collisions | 22 | 121 |
| cycles with `triangles_culled` at its rail | 0 | 59,992 |

Each lane asserts at the end that it reached every construction it claims.

## Formal properties

`tests/formal/measure_tokens_fairness.sby` — **GREEN, both tasks**, 13 s wall.
The ledger already named this file; this is it populated rather than promised.

- `prove` (**temporal induction**, not BMC — every property is one-step
  inductive, and the interesting state is billions of tokens from reset):
  `a_view0_sealed` / `a_view1_sealed` (THE VOLCANO — with no budget load, no
  granted request of its own and no return of its own, a view's guaranteed
  pools do not move, and nothing the other view does appears in the
  antecedent); `a_budget_latched`; `a_within_budget`; `a_no_underflow`;
  `a_private_first`; `a_reserve_needs_essential`. Basecase pass, induction pass.
- `cover`, on a deliberately tiny `TOK_W = 4` second instance so a bounded
  trace can get there: all six reached — `c_grant_private` (step 2),
  `c_grant_reserve` (3), `c_denied` (3), `c_reserve_emptied` (4),
  `c_pool_emptied` (4), `c_credit_clamped` (5). Without them every assertion
  would also hold for a guard that never granted anything.

**`a_budget_latched` was added because the first run's induction failed.** The
harness mirrors the block's budget registers; under induction the mirror and
the block's own registers are independent free state, so the solver started
from a pre-state where they disagreed. The fix is an *assertion* that they
agree, not an assumption — an assumption would have hidden a block that latched
something else.

**Kept arithmetic-flat on purpose.** Five 32-bit compare/add/subtract lanes, no
divider, no multiplier, nothing unrolled. This repo has one banked property
(`terrain_bake_delta.sby`) whose BMC sat 10.7 hours on a 17-step restoring
divide unrolled into SMT; a proof that cannot finish is worse than none.

**Not proved:** the counters (pure saturating adders — covered at the rail
directed and over 120,000 cycles random), the denial's `fixed:1` timing
(measured directly), and the 45/45/10 numbers (they arrive from the command
stream — law T3).

## Synthesis / resource ceiling

458 flops, five 32-bit comparators, five 33-bit add/subtract lanes and nine
saturating counter adders. No RAM, no DSP, no divider. Conservative
SystemVerilog subset (charter §2); **no function-call result is indexed
anywhere in this file** — Verilator accepts `f(x)[7:0]`, Quartus 17.0 rejects
it outright, and it cost GEOM.BINNER a synthesis failure that every simulation
lane passed.

**The one timing note worth writing down:** `tok_grant_o` is combinational from
`req_valid_i` through a 32-bit unsigned comparator and a two-way mux, and
GEOM.BINNER's accept path is combinational into it. That composed path is the
block's critical path and it belongs to the *seam*, not to either block alone.
Simulated and linted here; **not synthesized and not timed on hardware.**

## Integration capture cases

None yet — the guard has no landed effect on any capture, because GEOM.BINNER
ties `tok_grant_i` high in every test that is not about tokens (its own law E)
and RASTER.FRAGMENT has no token port at all. The first capture case worth
having is a Duo frame under load where one view is denied and the other is not;
that needs the fragment consumer's port, which does not exist.

## Notes

**MUTATION-CHECKED.** Four defects were injected ONE AT A TIME, each proved to
have relinked by hashing both test binaries before running them, and each
reverted afterwards. Baseline hashes (SHA-256, first 16): directed
`DE83445C327508F1`, random `88EB1B4AFD79AE65`. After the revert and one comment
correction: directed `98AC9A1CB6461805`, random `BBEF12F5AC9C84E5`, both green.

| mutation | directed hash | random hash | directed | random | formal |
|---|---|---|---|---|---|
| `fits_priv` compares `<` instead of `<=` | `F29D7F6A62C97332` | `F1B8BEDDCF448CE9` | 1,861 red | 1,324,454 red | — |
| the reserve is reachable without `req_essential_i` | `736B5E82B2F6CAE8` | `D203B3A093DDF8C0` | **17 red** | 1,166,719 red | `a_reserve_needs_essential` **FAILS** |
| both views debit view 0's geometry pool | `E7B4181FEC06F23A` | `061D82582D8AD121` | 6,348 red | 1,136,848 red | `a_view0_sealed` **FAILS** |
| the T5 credit clamp is dropped | `B31009D65CF6785E` | `31DDF6B1937F318F` | **3 red** | 1,746,663 red | `a_within_budget` **FAILS** |

**The third row is the whole argument for the formal lane.** A guard where both
views debit one pool is *self-consistent*: every grant is accounted, no pool
wraps, the totals balance. What it destroys is the guarantee, and
`a_view0_sealed` names it in one line — *view 0's pool moved on a cycle where
view 0 did nothing.* That is the volcano, caught by induction rather than by a
scenario someone remembered to write.

**The fourth row is the argument for CONSTRUCTING boundaries.** Dropping the
clamp is only visible on a cycle where a credit would actually have exceeded the
budget. The directed lane sees exactly **3** such cycles — because it builds
them by hand. Uniform random returns would have found it too, but only because
lane B deliberately parks the pools within eight tokens of a near-2³² ceiling;
at meshlet scale against a 45,000-token budget a random credit clears the
ceiling essentially never.

**The second row is the argument for the reason codes.** Only 17 directed checks
go red, because the mutation changes the *admission* of low-priority work and
almost every directed case is about something else. The 17 are the four
low-priority cases and their reason codes — and the formal lane turns the same
defect into a one-line invariant violation.


**NOT BUILT, and named so the next wave knows:** the composition test wiring
this block's `tok_grant_o` to a REAL `zhao_geom_binner`'s `tok_grant_i`. The
seam is honoured port-for-port and the binner's law E is quoted verbatim above,
but the two blocks have not been run against each other. That is the single
most valuable next increment for this block, and it is named as missing rather
than implied — TERRAIN.LOD → TERRAIN.TESS found a real tear that way, and this
seam has the same shape.

**Maturity stays SPECIFIED.** The RTL exists, lints clean, agrees with its
oracle over 2.9 M checks and carries a green inductive proof of its central
guarantee — but it is simulated, not synthesized and not on hardware, and its
one landed consumer has never been driven by it.
