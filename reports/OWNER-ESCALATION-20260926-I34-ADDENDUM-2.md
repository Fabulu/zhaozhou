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
  COMPOSED_NAV       [0x05AB0000, 0x05CB0000)   2 MiB
```

and the bus measurably cannot pay for them. `tools/budget/sdram_bandwidth.py`:
**one 2 B/vertex plane costs 737,280 cycles against 330,474 free — 2.2× the
entire headroom on its own**, and material-as-u32 plus nav-as-fx are each about
twice velocity's width, so the pair needs on the order of **3.7 million cycles
against 330 thousand free.** The directive anticipates exactly this case:
*"A measured engineering impossibility is a finding, not permission to invent a
pass."* **Reporting it is correct and it stays reported.**

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

**Question 2 (destination): I am executing my own recommended option**, which is
to commission a packet to look for a **FABRIC** consumer for material and nav,
the way velocity found one. Velocity was in precisely this position a week ago —
owner-named, "ratified" destination, SDRAM publish refused on the same bandwidth
proof — and it closed **with no SDRAM at all**, riding composed height's existing
route to `zhao_terrain_heighttap`'s §4.3 cell and out to the particle tap. **No
region, no bandwidth, a real consumer, a number that moves.**

**I have not walked either route and I am not asserting one exists.** Material
has a plausible fabric consumer in the mosaic path and nav has one in
SW.CPUCOLL's mirror; in this tree "X does not exist" runs false at a rate near
one in two, and it runs false in **both** directions — this addendum is itself a
false-absence I wrote. The packet's job is to **measure**, and to report a clean
negative if that is what it finds.

**What remains genuinely the owner's**, and is NOT being decided here:

* If the fabric hunt comes back empty, the choice is between **option 1 as a
  temporary with a recorded expiry** and a **bandwidth re-architecture** to
  afford the directive's regions. I will bring that back with the measurement
  rather than adopt option 1 silently, because a computed-and-unread lane is the
  false presence this campaign exists to refuse.
* **Option 3 — ruling material and nav out of the Earth record — remains
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
