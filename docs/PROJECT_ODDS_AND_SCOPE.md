# Odds and scope — an outside assessment

Recorded 2026-08-18, from a conversation the owner had elsewhere and asked to
have filed. It is kept in the repo because its central claim is a **scoping
decision**, and scoping decisions belong where the work is.

Nothing here is a plan and nothing here is scheduled. The odds are explicitly
described by their author as intuition, not statistics.

---

## The claim

> "Yes, you have a chance in hell. But the thing currently in your head is not
> one project. It is about six enormous projects welded together."

The six:

1. a custom FPGA console;
2. a custom programming language and toolchain;
3. a bespoke 3D engine;
4. a Sacrifice-scale strategy/action game;
5. a large professional content production;
6. potentially a physical console afterward.

> "Trying to complete the maximal version of all six before calling anything
> 'finished' would be a terrible bet. Getting to a real, distinctive,
> commercially releasable game by treating them as **nested victories** is much
> more plausible."

## The odds

Intuition-based, per their author:

| Outcome | The bet |
| --- | --- |
| Impressive playable PC/emulator vertical slice | **High: 70–90%** |
| Real FPGA demo rendering its own terrain, creatures and spells | **Plausible: 50–70%** |
| Polished, sellable 2–5 hour game using this technology | **Plausible but hard: 40–60%** |
| Large 15–30 hour campaign, 50 creatures, human-quality VO | **Long shot: 10–25%** |
| All of that plus a finished bespoke physical console as one launch | **Single digits** |

The last two become far more plausible **as expansions over several years**
rather than as the minimum acceptable first release. That reframing is the
whole point of the table — it is not a forecast of failure, it is an argument
about what the first release should contain.

> "The full maximal dream is a moonshot. The core dream — an extraordinary game
> that genuinely runs on your strange machine — is not fantasy anymore."

## Why the assessment is not flattery

The argument offered, in its own terms: Zhaozhou is no longer a folder of
architecture prose and enthusiasm. It has real RTL, real reference behaviour,
differential tests, mutation testing, formal properties, synthesis evidence and
increasingly real subsystem composition.

The `DEBUG.FRAMEBLIT` work is cited as the example: the design proposal exposed
genuine faults, those were verified against the implementation and corrected,
missing ownership and arbitration blocks were built, and further integration
bugs surfaced in the process.

> "That is what an actual hardware project looks like. It does not look clean.
> It keeps revealing that the thing you thought was finished was resting on a
> fake assumption."

## The structural advantage

**The hardware is specialised for one kind of game.** This is not a
general-purpose PlayStation. Silicon can be spent on deformable terrain,
extreme LOD, creature geometry and absurd magic, rather than being spread
evenly across everything a general console must do.

That specialisation is the reason the odds above are as high as they are.

## The asymmetry worth remembering

**Hardware questions terminate.** "Does the raster path fit in 112 DSPs" has an
answer; once answered it stays answered; the set of open hardware questions
shrinks as work is done.

**Content questions expand.** Every creature suggests three more, every effect a
variant. The set *grows* as work is done, because doing the work is what reveals
the possibilities.

This is a stronger argument for finishing the hardware first than "hardware is
the foundation" — it is that hardware is the only part of this project that can
actually be **finished**.

## Graceful failure

Each nested victory is a real stopping point, so stopping leaves something
real: a working novel console publicly demonstrated; or a shipped product; or a
smaller game rather than a broken one. A plan with no graceful failure mode
converts years into nothing if it stalls. This one has three.

## Creature counts and budget

The comparison data, kept because the animation note depends on it:

| Game | Creatures |
| --- | --- |
| Impossible Creatures | 51 |
| Flock | 60 |
| Zanzarah | 77 |
| Bugsnax | 100 |
| Monster Sanctuary | 101 |
| Cassette Beasts | 120 |
| Temtem | 165 |

Fifty disciplined 3D creatures, handcrafted through the standard production
chain: **$150,000–$300,000**. See
[CREATURE_ANIMATION_APPROACH.md](CREATURE_ANIMATION_APPROACH.md) for the plan
that exists to avoid that number and the revised $10,000–$40,000 picture.

---

## How this reads against the repo it lives in

One honest note, since the assessment is about scope.

The project is currently doing what the assessment recommends: hardware
questions closed one at a time, each with an evidence path in the ledger. The
standing rule that hardware comes first and the compiler is incidental is the
same conclusion this assessment reaches from outside.

The risk it names that is live right now is **scope growing in response to
progress**. [OWNER_DOCKET.md](OWNER_DOCKET.md) is the file to watch for it —
which is also, honestly, the file most likely to cause it.
