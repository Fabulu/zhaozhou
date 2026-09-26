# ADDENDUM 2 to the I34 escalation — ONE OF ITS TWO QUESTIONS WAS ALREADY DECIDED, AND THE OTHER HAD A DEFAULT I FAILED TO EXECUTE

**Written 2026-09-26, by the author of the escalation.** Read
`OWNER-ESCALATION-20260926-I34.md` first. **Nothing here asks the owner for
anything.** It records that the escalation over-asked, and what I am doing
instead.

The owner's correction on sequencing — *"We still need to close all the things
first, too, otherwise fit isn't complete"* — sent me back through the campaign's
refusals with the question *is this a DECISION or a BUILD?*. It found that I had
been over-respecting them. **I13's two "unsettled laws" turned out to be settled
and merely unbuilt.** Applying the same lens to my own escalation finds two
faults, and one of them is mine twice over.

---

## FAULT 1 — the encoding question is settled, and `ops.yml` names the map in its own sentence

The escalation says:

> *"Two incompatible encodings are ratified in one tree with nothing mapping
> between them"* — `ops.yml` / `compose_material` / `zhao_field_sinks` speaking
> layer-E `{u8 a, u8 b, u8 weight}`, against `field-ir.md` 7.1 / the earth
> adapter / `zhao_terrain_patch_acc` speaking an opaque u32.

**Both halves of that are wrong.**

**There is a map, and `ops.yml` states it in the same line I quoted from.**
`FIELD.WRITE.MATERIAL`'s semantics reads *"2 candidate material IDs + blend
weight per cell; **resolved deterministically by TERRAIN.PATCH**"*. That is not a
competing encoding — it is the **sink's input** plus **the name of the block that
resolves it**. `field-ir.md` 7.1's `material:u32` is the **resolved output** of
that same block. They are two ends of one pipeline, and I read them as two
irreconcilable claims because I quoted the first eleven words of the sentence
that contains the answer.

**And the owner already ruled it anyway**, in the directive's own numerical
policy for this exact entry:

> *"Material is the last covering field that actually writes that lane, in
> original command order; its value remains an **opaque, full-width u32**."*
> … *"Keep the full material token; **do not narrow it to fit an older
> consumer**."*

So the encoding is decided, the direction of the decision is u32, and narrowing
to the layer-E triple is **explicitly forbidden**. I escalated it three days
after adopting the document that decides it.

This is the campaign's most-repeated shape arriving in a document of my own:
**an absence claim that a single grep of an already-open file refutes**, and
CLAUDE.md's warning that *"the confident one-line summary is where the error
lives"* — here, *"nothing mapping between them"*.

---

## FAULT 2 — the destination question had a stated default, and I did not execute it

This half of the escalation **stands on its merits**. The directive names
destinations and exact addresses:

```
  COMPOSED_MATERIAL  [0x058B0000, 0x05AB0000)   2 MiB
  COMPOSED_NAV       [0x05AB0000, 0x05CB0000)   2 MiB   <- SUPERSEDED, see below
```


and the bus measurably cannot pay for them. `tools/budget/sdram_bandwidth.py`:
**one 2 B/vertex plane costs 737,280 cycles against 330,474 free — 2.2× the
entire headroom on its own**, and material-as-u32 plus nav-as-fx are each about
twice velocity's width, so the pair needs on the order of **3.7 million cycles
against 330 thousand free.** The directive anticipates exactly this case:
*"A measured engineering impossibility is a finding, not permission to invent a
pass."* **Reporting it is correct and it stays reported.**

**THE NAV RANGE ABOVE IS DEAD AND I QUOTED IT ANYWAY.** Corrected 2026-09-26
after FABRICSINK measured it. `[0x05AB_0000, 0x05CB_0000)` **collides with
POST.ECHO**, which runs `ZHAO_POST_ECHO_BASE = 0x05C0_0000` for
`SPAN = 0x0003_C000` and therefore ends at `0x05C3_C000`
(`fpga/rtl/common/zhao_pkg.sv:244-245`).

**And the correction already existed when I wrote this.** DECISION RECORD 1 in
`reports/OWNER-RULINGS-20260919-EVENING.md:7935` had already moved it to
**`TERRAIN.COMPOSED_NAV [0x05C4_0000, 0x05E4_0000)`**, 2 MiB / 256 x 8 KiB,
clearing POST.ECHO's half-open end by 16 KiB. I quoted the DIRECTIVE without
checking the RULING that supersedes it -- in a document whose whole subject is
that I failed to check the directive before escalating. **The directive's own
sentence anticipates exactly this**: *"Before enacting these ranges, check the
LIVE map, guard, allocator, and branches being integrated."*

**What is not correct is that I stopped there.** My own escalation says, in its
own words:

> *"**(2) is what I recommend**, and it is what I will start **if you say
> nothing**"*

The owner is on vacation under a standing directive whose first instruction is
*"Use your own brain. You are the implementation architect, not a relay."*
**"If you say nothing" was always going to be the state**, and I wrote a default
and then waited on the thing the default exists to make unnecessary. A default
nobody executes is not a default; it is a second escalation wearing the clothes
of a decision.

---

## SO, DECIDED HERE, UNDER THE STANDING DIRECTIVE

**Question 1 (encoding): CLOSED.** Material is an opaque, full-width u32, per the
directive's numerical policy. The layer-E `{a, b, weight}` triple is the **sink
input**; TERRAIN.PATCH resolves it; the u32 is the result. Narrowing the token to
fit the layer-E form is prohibited by the directive and is not on the table. The
escalation's "two incompatible encodings" paragraph is **struck**.

**Question 2 (destination): THE HUNT RAN AND CAME BACK EMPTY. MEASURED.**
FABRICSINK walked both candidate fabric routes on 2026-09-26 and both end.
**So this half is now genuinely the owner's, and it is the ONLY thing in this
entry that is.**

* **MATERIAL's route is REAL and ends at I13's boundary, not at a missing
  consumer.** The authored layer-E triple already walks **eight composed hops**
  and dies at `proj_out_*`, a dangling top-level output of core *and* board --
  and it is already **per-triangle** there, which is the granularity "never
  interpolate identifiers" requires. Past that boundary the mosaic's material
  bytes are a **compile-time constant**, `MAT_BASE_RGB_C = 24'hFF_FF_FF`, on
  every fragment drawn. **I34's material channel and I13 share one blocker.**
* **AND THE DRAIN OBJECTION IS MEASURABLY WRONG, in the helpful direction.**
  `zhao_material_window.sv:415-420`'s `match_c` has five terms -- mode,
  vertex_alpha, frag_state, material_set, material_id -- and **`base_rgb` and
  `recipe_weight`, the exact bits the mosaic slices, are not among them.** A
  per-triangle triple on the rider costs **zero drains**. The drain price is
  real for per-cell `{material_set, material_id}`, which nobody proposes; it had
  been charged to the triple by conflation.
* **NAV's route does not exist in either language.** No navigation query exists
  at all -- `nav_grid`/`navmesh`/`pathfind`/`zref::nav` are zero hits across
  eight trees against a live positive control. There is exactly **one** nav port
  on a composed block and it is the **producer**. SW.CPUCOLL is `maturity:
  SPECIFIED` with an empty log, and **even built it would not read that wire**:
  the mirror is specified as RE-DERIVATION, and `zhao_terrain_writeback.sv:27-32`
  refuses mirrored state under T4 as a second-writer violation.

**AND MY VELOCITY ANALOGY -- THE WHOLE REASON I EXPECTED THIS TO WORK -- IS
FALSE.** I wrote that *"velocity was in precisely this position a week ago"*. It
was not. **Velocity's consumer is a point query at PARTICLE rate off a single
staged patch; material's is at FRAGMENT rate.** That difference is exactly why
velocity was free, and it does not transfer. The analogy was doing the
persuasive work in this document and it should not have been.

---

## THE OWNER DECIDED IT, 2026-09-26 -- AND CHOSE NONE OF THE OPTIONS OFFERED

**See `reports/OWNER-DECISION-20260926-I34-NAV.md`. It supersedes part of
the vacation directive, which is amended in place.**

Not option 1 (a dated stopgap), not option 2/3 (a bandwidth re-architecture
commissioned to preserve the SDRAM topology). **A fourth option: navigation
truth and its query service belong to SW.CPUCOLL / the CPU simulation
runtime**, as the terrain ownership contract already said.

* **The FPGA is NOT required to publish a nav lattice into SDRAM when no
  hardware consumer needs it.** `TERRAIN.COMPOSED_NAV` is struck; neither
  its original range nor DECISION RECORD 1's relocation is live.
* **`FIELD.WRITE.NAV` and its behaviour are PRESERVED.** Ownership decision,
  not deletion.
* **The replacement obligation is a REAL production query** -- hard
  passability plus composed movement cost, exposed through the runtime
  interface Form simulation and game AI actually call. A reference-only
  helper, a debug counter or a testbench read is **not** completion.
* **I34 STAYS OPEN until that service is implemented, integrated and
  tested.** The owner is explicit that the register must not reach zero
  *"through a dated stopgap, a renamed gap, or a computed-but-unread lane."*
* **Material is a separate half** and does not close merely because authored
  terrain materials render -- the bar is that **a Field material write
  changes the intended consumer.**

**AND THE BANDWIDTH FINDING WAS AN ARGUMENT AGAINST A TRANSPORT, NOT AGAINST
THE FEATURE.** Everything below this line measured a publication scheme
nobody needed. The measurement was right; the question was wrong. Kept
unedited as the record of how that happened.

---

## SO THE CHOICE IS THE OWNER'S NOW, AND IT IS NARROW

I said I would bring this back with a measurement rather than adopt option 1
silently. **Here it is.** Two live options:

1. **Option 1 as a TEMPORARY with a recorded expiry** -- the accumulator owns
   material and nav as patch-local state. It ships two lanes computed and read
   by nothing, which is the false presence this campaign exists to refuse, so it
   is only defensible as a declared, dated stopgap.
2. **A bandwidth re-architecture** to afford the directive's regions. The
   impossibility is robust: even handing option 2 BOTH provisional terrain rows
   entire -- 54% of the frame, figures their own authors refused to freeze --
   leaves a **2,468,566-cycle shortfall at 3.0 : 1**.

**A third path now exists and is cheaper than either, but it is not
independent:** material's route is complete up to `proj_out_*`, so **whatever
closes I13's boundary carries material with it at zero drain cost.** That makes
I34's material channel a consequence of I13 rather than a separate build. **Nav
has no such path and needs a consumer that does not exist in any language.**

**Option 3 -- ruling material and nav out of the Earth record -- remains
refused** and is not mine to take. It deletes a feature, which is first on the
directive's list of what the delegation does not cover.

---

## The general lesson, which is the one worth keeping

**An escalation is an instrument, and instruments go blind in the flattering
direction.** Handing a question upward *feels* like the careful, conservative
act, so it does not get audited the way a decision does — and a question that has
already been answered can sit in a document indefinitely looking like diligence.
Two of the three things I had been treating as owner-blocked this week (I13's two
laws, and this one's encoding half) were **already settled in documents I had
read**.

**Before escalating, grep the standing directive for the entry's own name.** It
costs one command. For I34 it returns the decision, the destinations, the exact
addresses and the numerical policy.
