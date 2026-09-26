# OWNER DECISION 2026-09-26 — I34 NAV: CPU-OWNED CAPABILITY WITH A REAL RUNTIME QUERY

**This is an OWNER decision, taken 2026-09-26, and it SUPERSEDES part of
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`.** It is recorded once, here,
and the conflicting live requirements are amended in place and linked back to
this file. Further implementation choices are delegated.

**The options that were offered are all refused.** Not option 1 (a dated
stopgap), not option 3 (a bandwidth re-architecture commissioned merely to
preserve the SDRAM topology). The owner chose a fourth.

---

## THE DECISION, IN THE OWNER'S WORDS

> Choose a fourth option. Do not implement option 1, and do not commission
> option 3 merely to preserve the SDRAM topology in my earlier directive.
>
> The requirement I preserve is that active Earth fields affect the navigation
> cost used by gameplay, with the specified deterministic semantics. I do NOT
> require the FPGA to publish a nav lattice into SDRAM when no hardware
> consumer needs it.
>
> **1. OWNERSHIP AND SUPERSESSION**
>
> Navigation truth and its query service belong to SW.CPUCOLL / the CPU
> simulation runtime, as the existing terrain ownership contract already says.
> Use the same implementation on desktop and ARM where possible.
>
> This explicitly supersedes the vacation directive's requirement to create
> and publish TERRAIN.COMPOSED_NAV in FPGA-local SDRAM. Do not open that memory
> window or add a writer merely to give an otherwise unread output a home.
>
> Keep FIELD.WRITE.NAV and its behavior. This is an implementation-ownership
> decision, not permission to delete navigation, ignore nav writes, change
> their meaning, or cut supported field capacity.
>
> **2. BUILD THE MISSING SERVICE, NOT A DUMMY CONSUMER**
>
> Implement the smallest useful production navigation query against actual
> runtime terrain and active-field state: hard passability plus composed
> movement cost for a location/cell.
>
> Reuse the existing canonical field evaluation and compose_nav semantics.
> Reuse any appropriate implementation already present in the game/language
> repositories; absence from this console repository is not proof of absence
> from the entire project.
>
> Expose the service through the runtime interface that Form simulation and
> game AI can actually call. A new reference-only helper, debug counter, or
> testbench-only read is not completion.
>
> This packet does NOT need a complete pathfinding framework, tactical AI,
> army formations, or the full game engine. It needs the real cost/passability
> service those systems will consume.
>
> **3. SEMANTICS**
>
> Preserve command order, signed Q16.16 contributions, saturating accumulation,
> the final nonnegative cost floor, field coverage and optional-output
> presence. An absent output is not a write of zero.
>
> Navigation cost must never make hard-blocked terrain passable. Preserve the
> canonical terrain/lattice rules; do not substitute render LOD or invent a
> different off-lattice field evaluation.
>
> The query must use a coherent terrain/field/tick generation. Field creation,
> evolution, expiry and removal must affect the answer correctly. Do not cache
> animated fields forever under a dirty-bit rule that only notices stamps.
>
> Use shared semantics, not a separately hand-transcribed approximation.
> Resolve missing sampling, cache and interface details under the existing
> delegation and document them.
>
> **4. NO SECOND WRITER**
>
> The CPU derives its navigation state from its canonical terrain and the
> same accepted field commands, ordering and tick state. Do not feed FPGA
> results back as another writer of canonical simulation state.
>
> An unused FPGA-side nav result is not a capability we must keep pretending
> has a consumer. Explicitly retire or classify unnecessary nav-specific
> hardware staging/output paths under this new ownership decision, preserving
> shared-engine behavior and existing program/ABI compatibility.
>
> **5. ACCEPTANCE**
>
> Exercise the PRODUCTION runtime query through real field activation and
> terrain state, not by injecting a final cost into a fake consumer.
>
> Show:
> - No-field results match the authored baseline.
> - A nonzero field changes the queried cost in its covered region.
> - A small route-selection test using that same API responds to the changed
>   costs, then responds correctly when the field expires or is removed.
> - Hard-blocked terrain remains blocked, including with negative cost deltas.
> - Overlapping fields, saturation, command order and generation changes
>   follow the existing rules.
>
> The route-selection test can be a small harness. The query implementation
> and its state must be production code, not a disposable test substitute.
>
> Measure CPU work and memory use for representative workloads. This is not
> permission to claim ARM performance from an OMEN benchmark or to hide
> unbounded full-world evaluation behind an apparently cheap API.
>
> **6. HONEST COMPLETION**
>
> Record this as an explicit architecture revision. Do not mark the old
> FPGA-publication requirement "implemented"; mark it superseded and link its
> replacement obligation to the CPU navigation service.
>
> Keep navigation genuinely open until that service is implemented,
> integrated and tested. Do not make the register reach zero through a dated
> stopgap, a renamed gap, or a computed-but-unread lane.
>
> Material continues separately through its real Field-to-material path.
> Do not close that half merely because authored terrain materials render;
> verify that a Field material write changes the intended consumer.
>
> Record this decision once, amend conflicting live requirements in place,
> and proceed. Further implementation choices are delegated. Do not return
> another questionnaire about the internal shape of this service.

---

## WHAT THIS CHANGES, CONCRETELY

**SUPERSEDED — and marked as such, never as "implemented":**

* The vacation directive §1 DESTINATIONS requirement to *"create
  TERRAIN.COMPOSED_NAV ... as derived caches in local SDRAM bank 2"*, and with
  it the address allocation `[0x05AB_0000, 0x05CB_0000)` **and** DECISION
  RECORD 1's relocation of it to `[0x05C4_0000, 0x05E4_0000)`. **That window is
  not to be opened.** Neither range is live.
* Every argument in this campaign that treated *"nav has nowhere to go"* as a
  blocker requiring an FPGA destination. The destination was the wrong question.

**PRESERVED, explicitly:**

* `FIELD.WRITE.NAV` and its behaviour. **This is not permission to delete
  navigation, ignore nav writes, change their meaning, or cut field capacity.**
* The full deterministic semantics: command order, signed Q16.16 contributions,
  saturating accumulation, the nonnegative floor, field coverage, and
  optional-output presence (**an absent output is not a write of zero**).
* **Hard-blocked terrain stays blocked**, including under negative cost deltas.

**NEW OBLIGATION, which replaces the superseded one:** a production CPU-side
navigation query — hard passability plus composed movement cost — exposed
through the runtime interface Form simulation and game AI actually call,
reusing canonical field evaluation and `compose_nav` semantics.

**AND THE REGISTER STAYS OPEN.** The owner is explicit: *"Keep navigation
genuinely open until that service is implemented, integrated and tested. Do not
make the register reach zero through a dated stopgap, a renamed gap, or a
computed-but-unread lane."* **I34 does not close because its destination
question was answered.** It closes when the service exists and is tested.

**MATERIAL IS A SEPARATE HALF** and continues through its real Field-to-material
path. **It does not close merely because authored terrain materials render** —
the acceptance is that *a Field material write changes the intended consumer.*
That is a sharper bar than "terrain draws", and it is the bar.

---

## WHY THE CAMPAIGN GOT THIS WRONG, recorded because it generalises

The bandwidth measurement was correct and was **an argument against a
publication scheme, not against the feature.** Three packets and two documents
of mine treated *"the directive names an SDRAM destination"* as the thing to
satisfy, and then measured that it was unaffordable — never asking whether the
destination was load-bearing. **It was not. The capability was.**

The owner's own words for it: *"Preserve the capability: fields affect
navigation. Do not preserve an unnecessary implementation requirement."*

And the tell was in the evidence the whole time. FABRICSINK measured that
SW.CPUCOLL *"would not read that wire even if built"* because the mirror is
specified as **re-derivation** — and read that as proof the route was dead. **It
was proof the route was the wrong shape:** the CPU was always supposed to own
this, and the contract said so.

**This is `A REFUSAL IS AN INSTRUMENT` one level up.** A measured impossibility
is a finding — but a finding about *the thing measured*. Ours measured a
transport nobody needed, and the confident 3.0 : 1 number made the conclusion
feel settled enough that nobody re-asked what it was a conclusion about.
