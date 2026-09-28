# DECISION: phase 3 proceeds, and the escalation stops being a block

Coordinator, 2026-09-28. **This retires the blocked posture of
`OWNER-ESCALATION-20260928-THE-CONSOLE-DOES-NOT-FIT-ANY-CYCLONE-V.md`.** The
finding in it stands and is not withdrawn. What is withdrawn is my treating it as
a gate on further work.

## Why I am deciding this instead of waiting

The escalation ends with a section headed *"What is NOT blocked and continues
regardless"*, which says the ~4,000 ALUTs of genuine duplication *"remain
available as an ordinary packet **whenever it is wanted**"*.

That is the failure this repository names in CLAUDE.md, and I wrote it:

> *"**If you write a default, execute it.** 'This is what I will do if nobody
> objects' in a document nobody is reading is not a plan."*

> *"…a default nobody executes is not a default; it is a second escalation
> wearing a decision's clothes."*

Under a standing vacation directive, **"whenever it is wanted" was always going
to resolve to never.** The same section of CLAUDE.md warns that a wrong refusal
is *"invisible to every instrument in the tree by construction"* — it produces no
red, no counter, no output to be wrong. I spent this session finding seven such
sentences in `zhao_console_core.sv` and correcting them. This is the same shape,
one level up, in my own report.

**The directive is also explicit that this is my call:** *"You are the
implementation architect, not a relay that sends every unresolved engineering
choice back to me. … Do not stop at another list headed OWNER DECISION."*

## The question, answered

**Is whole-console FPGA placement still an acceptance criterion?**
**YES, and it was already answered.** The directive says *"The shipping target
stays `5CSEBA6U23I7`; a bigger diagnostic target does not change it."* I do not
need a ruling to know the target; I have one. I was asking the owner to relieve
me of a target he had already fixed, which is the over-asking pattern the
refusal law describes.

What the directive ALSO says is what to do with my measurement: *"A measured
engineering impossibility is a finding, not permission to invent a pass."* It
does not say *stop*. The finding is filed. Phase 3 proceeds against the fixed
target.

## Getting the arithmetic right, because I had been sloppy about it

There are **two different gaps** and they had been blurring together:

| to reach | ALUT ceiling | current 293,886 | must remove |
|---|---|---|---|
| placement on the largest **installed** die (`5CEBA9F31C7`) | 227,120 | 129% | **66,766 (23%)** |
| the **shipping** part (`5CSEBA6U23I7`, 41,910 ALM) | 83,820 | 351% | **210,066 (71%)** |

"Does not fit any Cyclone V" means the **first** row — we cannot place it even to
measure it. The shipping target is the second, and it is a different scale of
problem.

**This is why the ~4,000 ALUT consolidation cannot be the plan.** It is 6% of the
first gap and 2% of the second. Cashing it is right — an uncashed cheque is this
repository's most expensive recurring habit — but it must not be presented as
progress toward placement, and my phase-3 report already had to correct itself
once for counting instances without their sizes.

## The chosen lever: LOOKUP FOR COMPUTATION

Not micro-consolidation. The measured target is the class the phase-3 report
already identified and did not pursue:

* **Twelve blocks with ZERO memory and ALUTs far above registers own 32,784
  ALUTs — 11.2% of the design.** That is pure combinational computation holding
  no state.
* **The device has 246 FREE M10K**, and memory is at 60% while ALMs are the
  binding constraint. The slack is in exactly the resource the computation
  could move into.
* A **ROM instantiated thirteen times for 1,727 ALUTs** is the shape stated
  plainly: one table, thirteen copies, 246 spare M10K, blocked only on adding a
  pipeline stage at its call sites.

This is the direction already recorded in memory as the standing principle —
*ALMs are the binding constraint; memory is the slack, but the lever is
**lookup-for-computation**, not relocating state.* Phase 3 adopts it.

**Why this and not the alternatives:**

* *Cut features / reduce fields / shrink the giant* — **forbidden by the
  directive**, explicitly, and correctly. Not considered.
* *Reduce parallelism (time-multiplex lanes)* — permitted in principle, since
  it trades throughput rather than function. **Held as the second lever**, not
  the first, because it changes measured throughput criteria and every such
  criterion would need re-deriving. Lookup-for-computation changes latency at a
  call site and nothing else.
* *Micro-consolidation of duplicated instances* — **do it, but as hygiene**, not
  as the plan. 4,000 ALUTs, verified standalone.

## What this commits me to

1. **The ~4,000 ALUT consolidation gets executed as an ordinary packet**, not
   held pending a want nobody will voice. `zhao_field_v3_normalize` (FIELD +
   TERRAIN), `zhao_field_isqrt` (4 instances), `zhao_geom_mat3x4_mul` (LOOM +
   POSE_DECODE). Each needs a shared service with arbitration between two
   subsystems that run concurrently — that is real design, not a rename, and it
   is sized accordingly.
2. **The thirteen-copy ROM is the first lookup-for-computation target**, because
   its blocker is named and small: a pipeline stage at 13 call sites.
3. **No fit is spent until a subsystem's worth of change is ready.** Fits are
   the scarce resource; the map at HEAD already answers "where do we stand".
4. **Nothing here touches a capability.** If a reduction ever requires removing
   function, it stops and becomes an owner question — that is a real one, unlike
   the one I was sitting on.

## What remains genuinely owner-owned

One question survives, and it is narrow enough to be worth asking properly rather
than as a gate: **what is whole-console placement FOR** — bring-up on the
SuperStation One, a demo, or timing evidence? Each implies a different reduced
diagnostic target. **It does not block phase 3**, because every lever above is
worth pulling regardless of the answer, and I will not wait on it again.
