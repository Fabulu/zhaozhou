# DECISION 2026-09-26 — THE FIELD-MAJOR EARTH MACHINE'S DEPTH IS **ONE**, MEASURED ON THE COMPOSED HOST. THE ADAPTER IS NOT THE FIRST BUILD; THE GATHERING FRONT IS.

Taken by packet EARTHMAJOR under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. It corrects a load-bearing
input to `reports/DECISION-20260926-I34-FIELDMAJOR-BENCHED.md` and **reorders
the build that record commissions**. It does not withdraw the commission, does
not narrow any capability, and changes no production RTL.

---

## THE QUESTION

FIELDMAJOR benched the field-major Earth machine and refused to build it **on
sequence, not on doubt**: *"This is a BUILD, the decision is taken, and the
refusal is about sequence."* This packet is that sequence. Its first act was the
one `CLAUDE.md` requires of any inherited number — **check the deciding cell
before spending a packet on it.**

The deciding cell was:

> the engine's own GATED depth (32), the conservative L = 80, and BOTH ends at
> the rates this tree runs today: **4,102 clocks = 0.68× the 6,000 contract.
> IT FITS, with 1.5× margin.**

**Is depth 32 a configuration this console can reach?**

## THE DECISION

**No, and neither is depth 2. The composed host's engine overlap is ONE, at
every parameter setting, and it is measured.** The deciding cell describes a
machine no `-G` value produces.

**The composed cost is therefore 74,507 clocks — 12.42× the contract — not
4,102.** The record is off by 18×, in the flattering direction.

**So the build order inverts.** The field-major adapter is still commissioned
and still correct; it is no longer FIRST. **The gathering front is**, because
the walker's four-wide group buys nothing while the front answers one point per
run — and until it does, **the whole transpose is a WASH**, not the 22× the
deciding cell implies.

### What was built to answer it

`tests/field/field_host_depth_census.cpp` drives `zhao_field_host_v2` — the
module `zhao_console_core.sv` instantiates as `u_field_host` — **at the
console's own twenty parameters, not the module's defaults**, which is the trap
`zhao_field_host.sv:115` already names (*"a fit that does not override them is
measuring the bench"*). **25 checks, 0 failures. No production RTL changed.**

Four clients hold `req_valid_i` high for 4,000 clocks and collect every response
immediately. That is the most concurrency this seam can be offered.

### The measurement

```
  === THE CONSOLE'S COMPOSED HOST, SATURATED (per-point clear ON) ===
    ACCEPT-TO-ACCEPT INTERVAL     : 62.00 clocks   <- this is L/depth
    run latency (accept->response): 62.00 clocks
    ENGINE OVERLAP (derived)      : 1.00 evaluations
    last status                   : 0x00  (StOk, out[0] = 42)

  === THE SAME HOST WITH THE PER-POINT REGISTER CLEAR SKIPPED ===
    ACCEPT-TO-ACCEPT INTERVAL     : 30.00 clocks
    ENGINE OVERLAP (derived)      : 1.00 evaluations
```

**The overlap is a DERIVED number with both operands separately measured** —
latency by accept-to-response per client, interval by accept-to-accept — so it
is not the single-register argument it would have been if read off the RTL.

### Why no parameter reaches depth 2, let alone 32

`zhao_field_host_v2`'s front is one sequential state machine —
`E_IDLE → E_ZERO → E_WRITE → E_START → E_RUN → E_DRAIN → E_RETIRE` — with
`req_ready_o` gated on `state == E_IDLE` (that file `:1150`). There is one
`state` and one `cur_slot`, and **neither is dimensioned by any parameter**. The
executor's `CTX` is a context count the front never reaches; a context the front
cannot occupy is not a depth.

`FAB_LANES` does not help either, and the RTL says so at the preload port:
`fab_pre_data = {FAB_LANES{cur_in[lane_sel]}}` — **the point is REPLICATED**.
The host's own header: *"at FAB_LANES>1 the other lanes recompute the same point
and their results are discarded. This is a WASTE, not a fix ... A front that
gathers FAB_LANES points per grant is the thing that makes the width pay, AND IT
IS NOT BUILT."*

**THREE DOCUMENTS ALREADY SAID THIS AND THE CENSUS WAS WRITTEN ANYWAY**, which
is the part worth recording. `design/contracts/GEOM.WARP.md` P5 measured it on
**2026-09-21, five days earlier**: *"Parameter-independent across CLIENTS,
PROGS, FAB_LANES (**which replicates one point across lanes and discards the
surplus — it does not add points**), FAB_GROUP_PTS, FAB_OUTSTANDING and
CREDITS. It reopens only on an RTL change that adds a second front."* The
knowledge was in the tree, correct, in a contract, and nothing read it back.
That is `CLAUDE.md`'s own uncashed-cheque chapter arriving in a measurement
rather than in silicon.

### The composed cost, stated the way the budget binds

A group is **four points**, and this front answers **one point per run**, so a
four-point group costs four accept intervals until a gathering front exists.

| | clocks/group | 297 groups + the 851 floor | vs. 6,000 |
|---|---|---|---|
| the console as composed today | 248 | **74,507** | **12.42×** |
| ... with the per-point clear skipped | 120 | **36,491** | **6.08×** |
| — the record's deciding cell, for comparison | ~13 | 4,102 | 0.68× |
| — vertex-major, the thing being replaced | — | 91,551 | 15.3× |

### AND THE TWO FORMS COMPARED LIKE FOR LIKE — A SELF-CORRECTION

**The first version of this record said the transpose was worth 1.23×.** It
reached that by quoting 74,507 (measured here, at L = 62) against the published
vertex-major **91,551 (modelled, at L = 80)**. **That comparison measures the
two ENGINES, not the two STREAM ORDERS** — `CLAUDE.md`'s *"a measurement across
MISMATCHED POSES measures the pose"*, in clocks, by the packet that had just
quoted that law at someone else.

Evaluated like for like, **both of the census's own lines at the same measured
L = 62**, and printed by the instrument at the end of every run:

| | clocks | vs. vertex-major |
|---|---|---|
| vertex-major `4,431 + 1,089·L` | **71,949** | 1.00× |
| field-major `851 + 1,188·L` (every group slot) | **74,507** | **1.04× — worse** |
| field-major `851 + 1,089·L` (masked lanes skipped) | **68,369** | **0.95×** |

**SO THE TRANSPOSE ALONE IS A WASH — between 5 % better and 4 % worse — and
that, not 1.23× and not 22×, is the number that changes the build order.**

**The reason is exact and it is this packet's sharpest finding.** With a front
that answers **one point per run, both forms pay one engine round trip per
COVERED VERTEX.** The 297-against-1,089 slope — FIELDMAJOR's *"the walker
deletes the per-VERTEX round trip and pays one per GROUP OF FOUR"* — is a
property of a **GROUP-WIDE FRONT**, not of the stream order. **The transpose
relocates the work; on its own it removes no round trips at all.**

**Said the other way round, which is how it should be quoted:** with the 851
floor and 297 groups, **a group must cost ≤ 17.3 clocks** to meet the contract.
It costs 248.

### AND THE COST DECOMPOSES, WHICH IS WHAT MAKES THIS ACTIONABLE

The 62 clocks are not one lump. Measured here, and each is a separate lever:

* **E_ZERO — 32 clocks, 52 % of the run.** `REGS` clocks of per-point register
  clear. The census measures the saving as `REGS`, within 20 %, and asserts it.
  **This one needs no new block at all:** directive 6.1 / FH08 already let an
  image with an accepted `INIT_PROOF` skip it, and `hdr_ipok` is a live gate in
  shipped RTL. It is an AUTHORING act, not an RTL one.
* **E_WRITE — `IN_LANES` = 15 clocks.** Half of what remains. A group-wide front
  writes four points' worth into `fab_pre_data`'s four lanes in the same 15
  clocks, because that port is already `FAB_LANES*32` wide and only the
  replication wastes it.
* **the run and its drain — the rest.** The census's program is two uops on
  purpose, so **every figure above is a FLOOR**: a real Earth program adds its
  own clocks and `tools/field/measure_earth_budget.cpp` owns that half.

**A route that reaches the contract, arithmetic on these measurements and
declared as such:** gathering front (÷4) + `INIT_PROOF` (−32/point) + two runs
outstanding (÷2) puts a group at ≈ 15 clocks, i.e. ≈ 5,300 clocks per
association. **That is the first arrangement any measurement in this tree has
put under 6,000, and all three of its terms are named blocks or named acts.**

### AND IT DISCHARGES A STANDING OPEN ITEM

`design/contracts/GEOM.WARP.md` P9 records the FH08 fast path as existing and
**"Still unmeasured in clocks"**. It is measured now: **32 clocks, 52 % of a
run, on the console's own parameters.**

## WHAT THIS PACKET REFUSED, ASKED AS DECISION-OR-BUILD RATHER THAN INHERITED

**Building and composing the field-major Earth adapter in this packet.**

`CLAUDE.md`'s newest chapter — *"A REFUSAL IS AN INSTRUMENT, AND IT GOES BLIND
IN THE FLATTERING DIRECTION"* — says to ask **DECISION or BUILD** of every
refusal, including one's own, and notes that a wrong refusal is caught by
nothing in the tree. So, precisely:

**It is a BUILD, the decision is still taken, and this is not a refusal of the
build — it is a REORDERING of it, on a measurement.** The adapter is worth
exactly what the front makes it worth:

1. **Composing it today lands at 74,507 clocks against vertex-major's 71,949 at
   the same measured engine price** — a **wash, and very slightly worse** —
   **bought with a whole subsystem swap** — the stream
   order, the patch lifecycle, the accumulator's backpressure, and the
   re-homing of `zhao_terrain_veljoin`, which is reaching `zhao_part_collide`
   today and which §13.1 forbids regressing.
2. **A packet that did that would then have to be undone**, because the front
   changes the adapter's own shape: a group-wide adapter presents four points
   per request and a scalar one presents one. Building the scalar version first
   is building the thing the next change replaces.
3. **`CLAUDE.md`: "A fit that measures a circuit you already know is wrong is
   wasted."** The same applies to a composition. Repair the sequence, then
   compose.

**What is NOT refused, and must not be inherited as open:** the field-major
machine is still commissioned, `zhao_terrain_field_walk` and
`zhao_terrain_patch_acc` are still its front and back, FIELDMAJOR's 248-check
co-elaboration still stands, and none of the seven prerequisites is withdrawn.
**Only their ORDER changes, and only because prerequisite (5) turned out not to
be a parameter.**

## CONSTRAINTS AND COST

* **No RTL behaviour changed.** One new test file and one `tests/CMakeLists.txt`
  entry.
* **`I34` does not close**, and must not be reported as closing. Its remaining
  content is unchanged in kind: material's absent encoding, and the clock
  contract — the second of which now has a **measured** route rather than a
  modelled one.
* **Nothing is deleted or narrowed.** Velocity's chain is untouched: this packet
  composed nothing and moved no port.

## CONSEQUENCES FOR CODE, TESTS AND COMPATIBILITY

* New ctest `field_host_depth_census`, 25 checks, label `fast;nightly`.
* No port, no ABI, no `spec/commands.zidl` change.
* **`fieldmajor_census`'s depth sweep is not wrong and is not withdrawn** — it
  is a correct surface over a parameter. What is withdrawn is the MAPPING of two
  of its columns onto configurations of this console. The line
  `clocks(L) = 851 + (297/depth)*L` stands; **`depth` is 1 here and `L/depth` is
  248 clocks per group, both measured.**

## THE CONTROLS, AND ONE FIRED ON ITS AUTHOR

* **The census's headline is a counter reading ONE**, which `CLAUDE.md` says is
  the claim to check hardest. Case 0 drives the same accumulator the DUT
  measurement uses from a synthetic trace with genuine overlap and requires it
  to report 2 and then 3 — **and to report 1, not 8, for eight strictly serial
  runs**, which is the half that separates a concurrency from a total.
* **THE FIRST VERSION OF THIS FILE ASSERTED THE WRONG QUANTITY AND THE HOST
  REFUSED IT.** It asserted peak outstanding at the client seam == 1, from the
  single `state` register. The host answered **2** and was right: FH20 reserves
  a response entry before acceptance, so a **finished** result can wait in the
  delivery queue while the next run is granted. That overlaps no engine work and
  amortises no latency, so counting it as depth would divide `L` by a number
  that buys nothing. GEOM.WARP.md P5 had already said so in one clause and the
  first draft failed to read it. The peak is still reported, under its honest
  name; what is asserted is the derived overlap.
* **A SECOND CONTROL FIRED, AND IT CAUGHT A VACUOUS MEASUREMENT.** The first
  draft of case 2 loaded `INIT_PROOF` **after** the header and reported
  **1.00 clocks per run** — a 62× "speed-up". `LdInitProof` sets
  `hdr_loaded[slot] <= 1'b0` (`zhao_field_host_v2.sv:1472`), so it invalidates
  the header, and every run was answering `StNoProgram` instantly. **The
  run-count check passed; only the STATUS check caught it.** That is why the
  census asserts `StOk` and `out[0] == 42` beside every cadence figure: a
  throughput measured over refusals is the flattering direction and it looks
  exactly like success.
* **No assertion asserts the budget, in either direction.** Asserting
  "field-major misses 6,000" would assert a defect that a gathering front is
  meant to remove; the contract figures are printed, not checked.
