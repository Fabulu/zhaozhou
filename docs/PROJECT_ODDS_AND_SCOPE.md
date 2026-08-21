# Odds and scope — an outside assessment

Recorded 2026-08-18. This is an **outside assessment** the owner brought back
from a conversation elsewhere, kept in the repo because its central claim is a
scoping decision and scoping decisions belong where the work is.

It is not a plan and nothing in it is scheduled.

---

## The claim

> "Yes, you have a chance in hell. But the thing currently in your head is not
> one project. It is about six enormous projects welded together."

Everything below follows from that sentence. The response to it is not to
abandon anything — it is to **name which project is being worked on right now**,
and to let the others be later.

## The nested victories

The assessment splits the ambition into three, each a real stopping point:

- **Victory 1 — the proof.** The console does something no one thinks a
  hobbyist FPGA console does. Terrain that deforms like Sacrifice's, suns that
  smear like Noctis'. This is a *demonstration*, and it is complete on its own.
- **Victory 2 — the first commercial game.** One finished game that runs on the
  hardware and that someone pays for.
- **Victory 3 — the great expansion.** The full roster, the campaign, the
  platform.

Each victory contains the next. Reaching Victory 1 and stopping is not failure;
it is the most common good outcome for a project shaped like this one.

## Why the hardware terminates and the content does not

The load-bearing asymmetry:

**Hardware questions terminate.** "Does the raster path fit in 112 DSPs" has an
answer. Once answered it stays answered. The set of open hardware questions
shrinks monotonically as work is done.

**Content expands forever.** Every creature suggests three more. Every effect
suggests a variant. The set of open content questions *grows* as work is done,
because doing the work is what reveals the possibilities.

This is the argument for finishing the hardware first, and it is a stronger
argument than "hardware is the foundation" — it is that hardware is the only
part of this project that can actually be *finished*.

## Graceful failure

The project should be arranged so that stopping at any point leaves something
real:

- Stop after the proof: a working, novel console, publicly demonstrated.
- Stop after the first game: a shipped product.
- Stop mid-content: a smaller game, not a broken one.

A plan with no graceful failure mode is a plan that converts years into nothing
if it stalls. This one has three.

## What would make the assessor bet for, or against

**For:** hardware questions closing one after another, on the record, with
evidence. Scope held. A second creature that comes out of the machine cheaply.

**Against:** content work starting before the hardware questions close. Scope
growing in response to progress. A first creature that looks great and a second
that costs the same as the first.

## Creature counts and budget

The research the assessment carried, kept because it is the same data the
animation note depends on:

| Game | Creatures |
| --- | --- |
| Impossible Creatures | 51 |
| Flock | 60 |
| Zanzarah | 77 |
| Bugsnax | 100 |
| Monster Sanctuary | 101 |
| Cassette Beasts | 120 |
| Temtem | 165 |

Fifty disciplined 3D creatures, handcrafted: **$150k-$300k**. See
[CREATURE_ANIMATION_APPROACH.md](CREATURE_ANIMATION_APPROACH.md) for the
approach that exists to avoid that number, and the revised $10k-$40k picture.

---

## How this reads against the current state of the repo

One honest note, since the assessment is about scope and this file lives in the
repo the scope applies to.

The project is currently doing the thing the assessment recommends: hardware
questions, closed one at a time, each with an evidence path recorded in the
ledger. The standing rule that **hardware is first and the compiler is
incidental** is the same rule this assessment arrives at from the outside.

The risk it names that is live right now is the second one under "against":
scope growing in response to progress. The owner docket is the place that gets
watched for it.
