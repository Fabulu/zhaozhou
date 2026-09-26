# ESCALATION — I34's material and nav channels, and why the register cannot reach zero without you

**Written 2026-09-26. One decision is needed. Nothing is blocked waiting for it —
every other entry is being worked — but the register cannot reach 0 until it is
taken, and I am not taking it because every option on the table breaches
something you protected.**

`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` gives me standing authority
over technical decisions including amending rulings. I have used it several times
this week and recorded each. **This one I am handing back**, for a specific
reason: the directive's authority is to *choose among workable options*, and
here all three named options are unworkable against constraints you set. That is
an escalation by the directive's own terms, not a failure of nerve.

---

## Where the register actually stands

**6 remaining.** Five of them are engineering, being worked, and need nothing
from you:

| entry | what remains | status |
|---|---|---|
| `I13` + `zhao_terrain_normalmap` | carriage for terrain's per-cell layer-E triple | engineering, briefed |
| `I54` | a serialiser: binner reference order -> R7 chunks of arena ids | **in flight** |
| `I55` | the raster-path swap; needs I54 landed first | engineering, ordered |
| `I56` | the guaranteed giant's 32,768-reference quota | **in flight** |
| `I34` | **material and nav** | **THIS DOCUMENT** |

I34's other three channels are done or answered. **Height** was measured live end
to end. **Velocity** closed on 2026-09-26 — it now reaches
`zhao_part_collide`'s relative-velocity term over fabric, 1,089 lattice words per
patch agreeing with the oracle at every vertex. **The intake-versus-live-patch
hole** is answered: nothing structurally bounds CMD.EXEC's record stream against
the patch schedule, and the one-line fix is written into the entry and refused
pending two named measurements, because it carries a reachable deadlock.

**So I34 is one question wide, and it is yours.**

---

## The question

`design/ops.yml` names TERRAIN.PATCH as an implementation block of **both**
`FIELD.WRITE.MATERIAL` and `FIELD.WRITE.NAV`, and MATERIAL's semantics names the
resolver outright: *"2 candidate material IDs + blend weight per cell; resolved
deterministically by TERRAIN.PATCH"*. NAV's note is *"Consumed on the FPGA side
AND MIRRORED by SW.CPUCOLL"* — so the CPU's role is a mirror, not ownership.
**The fabric is supposed to carry both.**

**What does not exist is anywhere to put them.**

* `spec/memory_rules.md` 5b ratifies `COMPOSED_HEIGHT` and `COMPOSED_VELOCITY`
  and has **no composed-material region and no nav region at all**.
* **Two incompatible encodings are ratified in one tree with nothing mapping
  between them**: `ops.yml` FIELD.WRITE.MATERIAL, `zref::fieldir::compose_material`
  and `zhao_field_sinks` all speak layer-E `{u8 a, u8 b, u8 weight}`;
  `spec/form/field-ir.md` 7.1, the earth adapter and `zhao_terrain_patch_acc` all
  speak an **opaque u32**.
* The oracle cannot settle it: `reference/src/zrender/terrain.cpp`'s
  `compose_lattice` writes height and pushes velocity and **builds no material
  and no nav lattice at all.**

---

## The three shapes, and what each one breaks

These are the entry's own three options. I re-measured the numbers rather than
quoting them.

### Option 1 — the accumulator owns them, held as patch-local state

Cheapest in silicon terms and it needs no new region. **But it produces two lanes
that are computed and read by nothing**, and that is the exact shape this
campaign exists to refuse — `completion_register.py` counts a disconnected
implementation as a gap on purpose. **It would not close I34 honestly**; it would
move the arithmetic into existence without giving it a consumer, and the next
reader would inherit "material is implemented" as a false presence.

Its honest cost, which the entry states against its own recommendation: roughly
**two 32-bit lanes across the patch working set**. Its merit is real but narrow —
the arithmetic is computed once, exactly, at the moment the evaluation is
available, rather than re-derived later from a record that no longer exists.

### Option 2 — ratify composed-material and nav regions in 5b

**This is the one that makes directive 20.8 literally true, and it is
arithmetically impossible on this bus at this frame rate.** Measured today with
`tools/budget/sdram_bandwidth.py`:

| | SDRAM cycles | frame |
|---|---:|---|
| today | 330,474 **free** | 19.83% headroom |
| + composed VELOCITY publish | 406,806 **over** | **24.41% oversubscribed** |
| + the fill-side read that makes it a consumer | 1,291,542 **over** | **77.49% oversubscribed** |

So **one 2 B/vertex plane costs 737,280 cycles against 330,474 free — 2.2× the
entire headroom on its own.** Material as a u32 and nav as an fx are each about
**twice** velocity's width, so option 2 needs on the order of **3.7 million
cycles against 330 thousand free.** That is not a tuning problem.

### Option 3 — rule material and nav OUT of the Earth record

Smallest silicon, and **I will not take it.** It contradicts `field-ir.md` 7.1,
needs the ISA changed, and **deletes a requirement `ops.yml` states twice** by
naming TERRAIN.PATCH an implementation block of both sinks. Your directive lists
*"NOT authority to delete a feature"* first among the things it does not cover,
and this is that. It is also, as the entry says, "the one an overstated blocker
would smuggle in" — so it is named here explicitly rather than left as an
implication.

---

## What I would do, offered and not taken

**A fourth shape the entry does not list: give them a FABRIC consumer, the way
velocity just got one.**

Velocity was in exactly this position a week ago — owner named, destination
"ratified" in 5b, and the SDRAM publish refused on the same bandwidth proof. It
closed **without any SDRAM at all**, because the composed height already reaches
its consumers through fabric and velocity could ride the same route to
`zhao_terrain_heighttap`'s §4.3 cell and out to the particle tap. **No region, no
bandwidth, a real consumer, and a number that moves.**

Material has a plausible fabric consumer in the mosaic path and nav has one in
SW.CPUCOLL's mirror. **I have not measured either**, and I will not assert a
route I have not walked — the "X does not exist" claims in this tree run false at
a rate near one in two, and I have been caught by that twice this week myself.

**So the decision I am asking for is narrower than the entry's three options
suggest:**

1. **Do you want option 2's regions badly enough to move something else off the
   bus, or to change the frame budget?** If yes, that is a bandwidth
   re-architecture and it should be commissioned as one.
2. **Or may I commission a packet to look for a FABRIC consumer for material and
   nav, the way velocity found one** — with the standing instruction that if it
   finds none, it reports that and I bring you option 1-as-temporary with a
   recorded expiry rather than shipping it silently?

**(2) is what I recommend**, and it is what I will start if you say nothing,
because it spends one packet to answer a question rather than ratifying a region
we cannot pay for. **But I will not adopt option 1 or 3 without you.**

---

## What this does NOT block

Nothing. `I54` and `I56` are in flight, `I13` and `I55` are briefed and ordered,
and phase 3 — damage control on the failed console fit — is running in parallel
and producing measured reductions (`zhao_geom_drawjob` 100,561 -> 1,865
registers; `zhao_geom_attrsetup` 45 -> 36 DSP). **The register can reach 1 without
you. It cannot reach 0.**
