# DECISION 2026-09-26 — `zhao_terrain_patch_v2` DOES NOT OWN FOUR CHANNELS, AND ITS COMMISSION IS A CLOCK COUNT

Taken by packet PATCHV2 under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. Recorded once, here.
It amends how directive §13.2 and §20.8 apply to entry `I34`; it does not
amend the preserved capability behind them.

---

## THE QUESTION

Directive §13.2 commissions `zhao_terrain_patch_v2` owning *"ordered
application of **all four** Earth result channels"*, and entry `I34` has
carried the sentence *"what closes this entry is directive 13.2's
`zhao_terrain_patch_v2` owning four channels"* since 2026-09-23.

**Does it still?** The brief for this packet was explicit that the sentence
must be settled by measurement rather than inherited, because two things have
changed underneath it since it was written.

## THE DECISION

**No. The four-channel prescription is retired, and with it the idea that
`I34` is blocked on a channel-ownership question at all.**

If `zhao_terrain_patch_v2` is built, it owns **height and material** — two
channels. And **that is not why it would be built.** Its remaining commission
is the per-association clock contract and nothing else.

### The channel accounting, measured today, not inherited

| lane | ordinal | owner | status |
|---|---|---|---|
| `height_o` | 0 | `zhao_terrain_patch` (composed, **vertex-major v1**) | **real consumer**, measured live end to end by `composepub_acceptance` case 2 |
| `velocity_o` | 1 | `zhao_terrain_veljoin` → `zhao_terrain_velocity` → compcache §4.2 plane → `zhao_terrain_spdesc` → `zhao_terrain_heighttap` → `zhao_part_terrain_tap` → `zhao_part_collide` | **real consumer**, composed 2026-09-26 by TERRVEL |
| `material_o` | 2 | **nobody** | **the only open channel** |
| `nav_cost_o` | 3 | SW.CPUCOLL / `zref::nav::Service` | **owner-ruled elsewhere**, classified not deleted |

**1. NAV is out by owner decision.**
`reports/OWNER-DECISION-20260926-I34-NAV.md`: *"Navigation truth and its query
service belong to SW.CPUCOLL / the CPU simulation runtime … I do NOT require
the FPGA to publish a nav lattice into SDRAM when no hardware consumer needs
it."* `FIELD.WRITE.NAV` is **preserved**; `efa_nav_cost` stays produced and
classified, with its three-part reason beside `nav_cost_o` in
`zhao_field_earth_adapter.sv` (verified present, not merely claimed). The
replacement obligation `zref::nav::Service` is built. **Commissioning a
patch_v2 nav accumulator would spend silicon on a lane the owner has just
ruled has no FPGA consumer by design.**

**2. VELOCITY is already owned — and NOT by any patch_v2.**
TERRVEL composed the whole chain on 2026-09-26, three days after §13.2 was
written. §13.2's own closing responsibility, *"publication/drain to the
existing composed-height/**velocity** consumers"*, is satisfied by a different
block.

**And this is a NEW COST on the field-major rearchitecture that did not exist
when §13.2 was written, which is the part worth carrying forward.**
`zhao_terrain_veljoin` rides the **vertex-major per-vertex lane stream** and
takes `zhao_terrain_patch`'s `fld_covers_o` — measured at the instantiation:
`.vtx_fire_i(tpt_vtx_valid && tpt_vtx_ready)`, `.a_covers_i(terr_pt_fld_covers_o)`.
A **field-major** patch_v2 has neither of those. Composing it therefore does
not merely replace a reducer; it obliges whoever does it to **re-home a chain
that is composed, tested and reaching a particle contact today.** §13.1's *"do
not leave two opposite stream-order laws alive"* now has a second bill
attached to it.

**3. MATERIAL is the only channel with no owner, and its blocker is not a
missing port.** Measured 2026-09-26, opening the port lists rather than
grepping:

* `zhao_field_sinks.sv:31-49` — material is `{u8 mat_a, u8 mat_b, u8 weight}`
  on **both** its input and its output face. No 32-bit material port exists on
  that module in either direction.
* `zref::fieldir::compose_material` (`zref_fieldir.hpp:103-109`) — the very
  reference function `design/ops.yml:525` names for `FIELD.WRITE.MATERIAL` —
  **takes a `MaterialState` triple and returns a `MaterialState` triple.**
  `u32` appears nowhere in it.
* `zhao_field_earth_adapter.sv:522` and `zhao_terrain_patch_acc.sv:156-159` —
  `[31:0]` at every hop.
* **Nothing in this tree packs three u8s into that u32, and nothing unpacks it.**
  No encode, no decode, no LUT, no reserved bit layout.
* `zhao_terrain_compcache_front.sv:169-174` has **one** material write face,
  u8 triple, with a single driver — the page stream, i.e. authored layer E.
  No override input, no second writer, no arbitration port. **A field result
  has no port to arrive on.**

So material's gap is an **absent encoding**, and no widening of any block's
port list closes it.

### THEREFORE: what patch_v2 is actually for

Of §13.2's eight responsibilities, the channel clause is spent: two lanes have
owners elsewhere, one is owner-ruled out, and the fourth cannot be routed by
any block until an encoding exists. **What survives is §13.1 — the stream
order — and that is a performance commission.** It should be stated as one.

## THE MEASUREMENT THAT COMMISSIONS IT

`fld_earth_stall_cycles_o` was exported 2026-09-23 *"because it is the number
that decides whether the field-major machine has to be built before terrain
fields can run at frame rate"*, and nothing had ever read it. It is now read,
by `composepub_acceptance` case 11, through the four composed production
blocks that bench already drives.

```
  ASSOCIATION CYCLE CENSUS -- one field, whole 33x33 patch, 1,089 vertices
  engine lat     assoc clocks   stall_cycles  clocks/vertex
  (no field)             2252              0           2.07     <- NEGATIVE CONTROL
  3                      7698           5445           7.07
  20                    26211          23958          24.07
  50                    58881          56628          54.07
  80                    91551          89298          84.07
```

`clocks(L) = 4,431 + 1,089·L`, and **the slope is asserted, not merely
observed**: the association serialises exactly one un-overlapped engine round
trip per covered vertex. That assertion is what makes the `lat=80` row a
reading at the real engine's price rather than an extrapolation.

**The allowance these numbers are against is NOT 10,416.** Entry `I34`, this
packet's brief and `zhao_field_earth_adapter.sv:308` all quote that figure.
The owner directive retires it twice — §2.10, *"The familiar 10,416 clocks is
an older comparison allowance. The amended Earth contract states ≤6,000 clocks
per full association"*, and §18.2, *"An explanatory older 10,416-clock
allowance cannot replace the stricter active contract without a recorded
amendment."* `design/contracts/FIELD.SEQ.EARTH.md:167` is the active contract.

* at the real engine price (lat 80): **91,551 clocks = 15.3× the 6,000 contract.**
* at an idealised 3-clock engine, 27× faster than the real one: **7,698 = 1.3×.
  Over it already.**
* Put the way the budget actually binds: **6,000 clocks buys an engine latency
  of 1.44 clocks.** The retired 10,416 buys 5.5.

**Neither is reachable, so the verdict does not turn on which allowance is
used.** The 10,416 correction is about the honesty of the record; it changed
nothing about the answer, and it is recorded that way rather than as a finding
that mattered more than it did.

**VERDICT: the composed vertex-major Earth path cannot meet the active
per-association contract, by an order of magnitude, and the field-major machine
is commissioned by a measured number rather than by arithmetic in a header.**

### And what the measured line says about the OTHER repair, so the next packet does not have to guess

The census does not only condemn the current arrangement; because it is a
*line* rather than a number, it prices the obvious alternative too. **This is
offered as scoping, not as a decision — it is arithmetic on a measured
intercept and slope, and it has not been built or benched.**

The 1,089·L term exists because `zhao_field_host`'s front holds **one point in
flight**. Deepening the front to `P` outstanding points amortises that term and
leaves the intercept alone:

```
    clocks(L, P)  ~=  4,431  +  1,089 * L / P
```

* The intercept is **4,431 clocks**, i.e. **4.07 clocks per vertex** of work
  that is *not* engine latency — the per-vertex walk, the adapter's own two
  clocks, and the consumer's accept. **That floor is 74 % of the 6,000-clock
  contract on its own**, leaving 1,569 clocks for everything else.
* So at the real `L ≈ 80`, meeting the contract by pipelining alone needs
  `1,089 × 80 / P ≤ 1,569`, i.e. **P ≥ ~56 points in flight** — and it would
  still be sitting at 74 % of budget before the first field is evaluated, with
  no margin for a second covering field.
* The field-major machine's own walk is **273 INIT + 297 UPDATE + 273 DRAIN
  ≈ 843 clocks** (`design/contracts/FIELD.SEQ.EARTH.md:137-154`, and note it is
  297 update groups, not 273) plus the executor's vector work — **it attacks
  the intercept, not just the slope**, because it stops paying a per-vertex
  round trip at all.

**That is the real argument for §13.1, and it is stronger than "the stall is
large".** A deeper front would be chasing a 15.3× miss with a ~56-deep pipeline
into a budget three-quarters consumed by a floor it cannot touch. The
field-major form removes the floor. Whoever takes the performance packet should
carry this comparison rather than re-derive it — and should bench it, because
everything in this subsection after the two measured constants is arithmetic.

## ALTERNATIVES CONSIDERED AND REFUSED

1. **Inherit "four channels."** Refused: it commissions nav silicon the owner
   ruled out on 2026-09-26 and re-implements a velocity reduction whose
   consumer chain would then have to be rebuilt. The sentence was correct when
   written and two of its four terms have since moved.
2. **Declare `I34` closed on three of four.** Refused. §20.8's fence — *"DO NOT
   CLOSE I34 BY WIRING ONLY HEIGHT while declaring the other three channels
   present because they have spare bus bits"* — and material is genuinely
   unowned. Three real owners and one absent encoding is not four owners.
3. **Build `zhao_terrain_patch_v2` in this packet.** Refused: it is a subsystem
   swap (walker + four-bank accumulator + phase owner + lifecycle + drain +
   the paired v1 oracle, ~20 M10K at §13.5's packing estimate), its channel
   justification is dead, and — per §2 above — it now also owes the re-homing
   of TERRVEL's velocity chain. Ruling R163 additionally forbids creating the
   `_v2` file before it composes.
4. **Give `zhao_terrain_patch` a field-material port now.** Refused: an input
   with no encoding behind it and no write face to reach is an authored
   uncashed cheque, and `zhao_terrain_compcache_front` has no second writer to
   accept it.

## CONSTRAINTS AND COST

* **No RTL behaviour changed by this decision.** It is a scope amendment plus
  one test case; the census is additive and the console's logic is untouched.
* **Nothing is deleted.** `efa_nav_cost`, `efa_material`, `efa_present` and
  `zhao_terrain_patch_acc`'s four nav lanes all stay. Nav is *classified*, per
  the owner's §4, not retired.
* **`I34` does not close**, and it must not be reported as closing. Its
  remaining content is now exactly two things, both nameable: **material's
  absent encoding**, and **the ≤6,000 contract the census just measured a
  15.3× miss against.**

## CONSEQUENCES FOR CODE, TESTS AND COMPATIBILITY

* `tests/terrain/composepub_acceptance.cpp` gains case 11 — 42 checks, taking
  the file to **122 checks, 0 failures**. No existing check changed.
* No port, no ABI, no `spec/commands.zidl` change. `field-ir.md` §7.1's earth
  out record stays four ordinals; **ordinal 3 is not removed**, which the owner
  decision's "preserving existing program/ABI compatibility" clause requires.
* The next packet on this entry is a **performance** packet with a named
  question and a measured baseline, not an architecture packet with an open
  decision.

## AND MATERIAL'S DOWNSTREAM HALF IS ONE STEP WORSE THAN THE ENTRY RECORDS

Found while checking rather than inheriting, and it sharpens §3 above: material
is not "one missing link at a boundary". Entry `I34`'s FABRICSINK block says the
authored triple *"DIES AT `proj_out_*` — WHICH IS ENTRY I13"*. **That port no
longer exists** (two occurrences in `zhao_console_core.sv`, both comments). The
`I13` note at `:11995-12002` (CARRIAGE, 2026-09-26) says the triangle, its raw
w *and its layer-E triple* are now *"CONSUMED IN THIS MODULE"* by
`u_terrain_clipfeed`. **True of the triangle and the w; false of the triple.**

Counted by hand, because `tests/shell/v3_closure_inherited.vlt` waives
`UNUSEDSIGNAL` across whole directories and the linter therefore cannot see it:

* `tcf_tri_mat_a_w`, `_mat_b_w`, `_weight_w` — **exactly two occurrences each**:
  the declaration at `:19568` and the projector's write at `:19705-19707`.
  **No reader.**
* `zhao_terrain_clipfeed` **has no material input port at all**; its material
  outputs are the GEOM `{set, id, mode}` trio driven from constants at that
  file's `:695-697`.

The triple now has the same status as `tcf_tri_ad/bd/cd_w`, which the *same*
comment block openly declares *"DELIBERATELY NOT USED"* — the difference is that
those say so and the triple does not.

**Retiring a port is not connecting a lane.** The triple went from *visibly*
dangling at two module boundaries, where the closure gate and a reader could
both see it, to *invisibly* dangling on an internal wire — and the register's
`I13` row shrank in the same change. That is `CLAUDE.md`'s `.gitignore` chapter
wearing RTL. **Nothing is changed here**: the seam is `I13`'s and TERRAINTEX is
live on it. It is recorded so the next packet does not inherit *"material ends
at a boundary"* when it ends before one.

## A DANGLING SPEC REFERENCE FOUND IN PASSING, NOT REPAIRED HERE

`design/ops.yml:522-523` cites `spec/qformats.md §material-ids` and
`§material-state`. **Neither section exists**, and a search for those two names
across `spec/ design/ reference/ fpga/ tools/` returns only the two `ops.yml`
lines that cite them. This is the identical defect already recorded one op
below at `design/ops.yml:557-560` for `FIELD.WRITE.NAV`'s non-existent
`§nav-layer` — the same audit was made and was not carried up two entries. It
belongs to whoever authors material's encoding, because those are the sections
that would hold it.
