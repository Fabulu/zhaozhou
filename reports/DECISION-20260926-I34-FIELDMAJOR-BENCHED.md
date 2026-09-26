# DECISION 2026-09-26 — THE FIELD-MAJOR EARTH MACHINE IS BENCHED, IT MEETS THE CONTRACT, AND THE THING THAT DECIDES IT IS THE ENGINE'S WIDTH

Taken by packet FIELDMAJOR under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. It settles the
measurement `reports/DECISION-20260926-I34-PATCH-V2-CHANNELS.md` left as
arithmetic. It amends nothing about the preserved capability.

---

## THE QUESTION

PATCHV2 measured the composed **vertex-major** Earth path as a line —
`clocks(L) = 4,431 + 1,089·L`, **91,551 clocks** at the real engine's price,
**15.3× the active ≤ 6,000-clock contract** — and then priced the field-major
alternative with arithmetic, flagging it honestly: *"everything in this
subsection after the two measured constants is arithmetic"*, *"it has not been
built or benched"*, and *"whoever takes the performance packet should carry
this comparison rather than re-derive it — and should bench it."*

Its load-bearing claim was about **the intercept**: 4,431 clocks, *"4.07 per
vertex of work that is NOT engine latency"*, *"74 % of the 6,000-clock contract
on its own"*. The whole argument for §13.1 is that the field-major form
**attacks the intercept, not just the slope**.

**Does it?**

## THE DECISION

**Yes, and by more than the arithmetic predicted. The field-major machine is
commissioned, and a SECOND prerequisite that nothing had priced is now on the
bill: the executor's WIDTH.**

### What was built to answer it

`zhao_terrain_field_walk` and `zhao_terrain_patch_acc` have each carried a full
differential test since 2026-09-20 and **had never been elaborated together** —
each test does the other block's job in C++, so the composed machine's cost had
never been a measurement. `tests/terrain/tb_terrain_fieldmajor.sv` instantiates
both, port for port; `tests/terrain/fieldmajor_census.cpp` drives them.
**233 checks, 0 failures.** No production RTL changed.

The engine between them is modelled **at the same seam and with the same
declared latencies `composepub_acceptance` case 11 uses**, because the
field-major counterpart of `zhao_field_earth_adapter` does not exist — that is
what §13.1 commissions. So the one axis the two measurements differ on is the
stream order, which is the comparison that was wanted. It is stated in the
bench's own header rather than buried: **this is a floor for the field-major
form**, and the vertex-major number additionally contains a real adapter's
clocks while this one contains no adapter at all.

### The measurement

```
  clocks(L) = 851 + (297 / depth) * L      field-major   (THIS BENCH)
  clocks(L) = 4,431 + 1,089 * L            vertex-major  (case 11)

  engine lat   d=1      d=2      d=4      d=8      d=16     d=32
  0            851      851      851      851      851      851
  3            1742     1150     854      854      854      854
  20           6791     3683     2129     1352     961      871
  50           15701    8153     4379     2492     1531     1072
  80           24611    12623    6629     3632     2101     1372
```

**THE INTERCEPT FELL 5.21×.** 4,431 clocks (4.07/vertex, **74 %** of the
contract) becomes **851 clocks** (0.78/vertex, **14 %**). Measured, it is
273 INIT + 298 association + 276 DRAIN + 4 phase-gap clocks.

**PATCHV2's arithmetic was right to within eight clocks.** It predicted
"273 INIT + 297 UPDATE + 273 DRAIN ≈ 843". That is recorded deliberately: this
campaign expects a brief's claims to come apart under measurement, and **this
one held**. The eight are the DRAIN pipeline's three, the descriptor's
acceptance edge, and the phase gaps the accumulator's header requires.

**THE SLOPE FELL 3.67× FROM LANE WIDTH ALONE** — 297 against 1,089 at depth 1,
and that is *asserted*, not observed, exactly as case 11 asserts its 1,089. The
walker deletes the per-**vertex** round trip and pays one per **group of four**.

### AND THE FINDING NOBODY HAD PRICED: THE FIELD-MAJOR WALK ALONE DOES NOT CLOSE IT

Two of the six depths are real configurations, read out of the tree rather than
chosen:

| depth | what it is | at L=80 | vs. 6,000 |
|---|---|---|---|
| 1 | case 11's engine — `zhao_field_host`'s front holds ONE point | 24,611 | **4.10×** |
| **2** | **THE CONSOLE AS COMPOSED TODAY** | **12,623** | **2.10× — STILL MISSES** |
| 32 | **THE ENGINE'S OWN GATED CONFIGURATION** | **1,372** | **0.23× — FITS, 4.4× margin** |

* **Depth 2 is measured, not assumed.** `zhao_console_core.sv` gives
  `u_field_host` `.FAB_LANES(1)` and leaves `PROGS` at its default 8, so the
  executor is a **scalar** datapath with eight contexts — and a four-point
  group therefore occupies **four** of them. Two groups in flight.
* **Depth 32 is the configuration the engine is gated at.**
  `tests/CMakeLists.txt`'s `lint_field_v3_engine_shipped` runs
  `-GCTX=32 -GLANES=4 …`, and `zhao_field_host.sv:115` names the same seven
  values, with its own warning beside them: *"The engine's defaults are the
  SCALAR bench point … a fit that does not override them is measuring the
  bench."* At `LANES=4` one context **is** one four-point group.

**So §13.1's swap and the executor's width are ONE prerequisite, not two.**
Build the field-major walk and leave the console composing `FAB_LANES(1)` and
the association still costs 12,623 clocks — a 2.1× miss, reached after the
whole subsystem swap. That is the shape `CLAUDE.md` calls *a thing BUILT is not
a thing INSTALLED*, arriving in advance for once: it is cheaper to know now.

**The contract is met from depth 4 upward**, and the crossing is between the
console's present configuration and the engine's own.

### Why L = 80 is the honest column, and why it is conservative

`zhao_field_earth_adapter.sv:302-306` prices a run at *"REGS + IN_LANES + the
program's own length … order 80-100 clocks PER COVERED VERTEX PER FIELD"*. At
this console that is REGS(32) + IN_LANES(15) = **47 clocks of register and lane
transport**, and the walker's entire purpose is that it **generates** the points
— its header: *"v2 moved lattice points INTO the engine through a generic
12-in/4-out host stream … This block deletes that transport. It GENERATES the
points."* So the field-major machine should not be paying most of that 80.

**It is charged anyway**, in every number above, because the packet's job is to
report a cost and not to award itself a discount it has not measured. The
verdict does not need the discount.

## WHAT WAS REFUSED, AND WHY IT IS A REFUSAL THIS PACKET CHECKED RATHER THAN INHERITED

**Composing `zhao_terrain_patch_v2` in this packet.** `CLAUDE.md`'s *"A REFUSAL
IS AN INSTRUMENT AND IT GOES BLIND IN THE FLATTERING DIRECTION"* says to ask
**DECISION or BUILD** of every refusal, including one's own. This is a **BUILD**,
the decision is taken, and the refusal is about *sequence*, not about doubt.
Named, so it cannot be inherited as an open question:

1. **A field-major Earth adapter does not exist.** It is exactly what this
   bench's C++ engine model stands in for. §13.1 commissions it.
2. **`zhao_terrain_patch_acc` has no backpressure**, and its own header says so
   in capitals: *"no ready/valid and no backpressure on any phase"*, with
   INIT/ACCUM/DRAIN exclusivity and the two idle cycles as **caller
   obligations the RTL does not enforce**. §13.4 commissions that repair and it
   is a prerequisite, because a real cache write port can stall.
3. **A patch phase owner / lifecycle** — INIT, ACCUM, DRAIN, staging and drain
   before publication — is the directive's own requirement and does not exist.
4. **`zhao_terrain_veljoin` must be re-homed.** PATCHV2's finding stands and was
   re-checked: it rides the **vertex-major** per-vertex lane stream
   (`.vtx_fire_i(tpt_vtx_valid && tpt_vtx_ready)`) and takes
   `terr_pt_fld_covers_o`. A field-major v2 has neither. That chain reaches
   `zhao_part_collide` today and the fences forbid regressing it.
5. **The executor's width**, above — new on the bill as of this packet.

Doing (1)–(5) inside one packet, against a live composed velocity chain, is the
subsystem swap PATCHV2 refused for the same reasons and ruling R163 forbids
creating the `_v2` file before it composes. **What changed is that the refusal
now sits on a measured number saying the build is worth making**, rather than
on arithmetic.

## CONSTRAINTS AND COST

* **No RTL behaviour changed.** Two test files and a `tests/CMakeLists.txt`
  entry. Nothing composed, no port moved.
* **`I34` does not close**, and must not be reported as closing. Its remaining
  content is unchanged in kind: material's absent encoding, and the clock
  contract — the second of which now has a *route* as well as a verdict.
* **Nothing is deleted or narrowed.**

## CONSEQUENCES FOR CODE, TESTS AND COMPATIBILITY

* New ctest `fieldmajor_census`, 233 checks. Correctness is checked **before**
  cost: all 1,089 vertices × 6 lanes against the ratified vertex-major oracle
  `zref::terrain::compose_vertex`, which is the claim behind the whole
  stream-order swap and had never been checked with both blocks in one
  elaboration.
* No port, no ABI, no `spec/commands.zidl` change.
* The next packet on this entry has a **build list with a measured
  justification and a named failure mode**, not an open question.

## THE CONTROLS, AND ONE OF THEM FIRED ON ITS AUTHOR

* **The exact-recurrence assertion fired on me.** The census first asserted
  `ceil(297/depth)` as the slope, and the measured surface **refused it in five
  cells** — the line is **piecewise**, and while `L < depth` the walker's
  one-group-per-clock is the binding rate, so latency is free. It now asserts
  `t_k = max(t_(k-1)+1, t_(k-D)+L+1)` for all 30 cells, which has the knee in it.
* **The coverage control was fired deliberately.** Driving `up_mask_i` to `0xF`
  instead of the walker's mask makes case 2 report **920 wrong vertices** and
  case 1 report **96**. Both controls are load-bearing and were seen to fail;
  the tree was restored and `git status` came back clean, which is the check
  `CLAUDE.md` requires of a restore.
* **A negative control on the counter itself**: a footprint covering no lattice
  vertex gives `verts_covered_o == 0` **while the walk still runs all 297
  groups** — so the zero is about coverage, and not about an idle machine. That
  is the broken-instrument shape this entry has fallen into before.
* **No assertion asserts the budget, in either direction.** Asserting
  "field-major meets 6,000" would assert the conclusion the packet was sent to
  measure; asserting the vertex-major miss would assert a bug.
