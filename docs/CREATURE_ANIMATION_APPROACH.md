# Creature animation — the approach, and why it is a later wave

**Status: DESIGN NOTE, deliberately not started.** Recorded 2026-08-18 from the
owner. Animation architecture is a **later wave**, after `DEBUG.FRAMEBLIT`
integration and the FPGA fit, and the first prototyping happens in the
PC/reference path rather than in hardware. The reason is in §5.

---

## 1. The thesis

The 3D models are the hard part of this game, and the mistake to avoid is
treating them as fifty separate productions. **Build one creature-production
machine**, then run fifty creatures through it. Whether the machine works is a
question that gets answered by creature number two, not creature number one —
the first creature always works, because it was made by hand while the machine
was being built around it.

Comparable creature counts, for scale: Impossible Creatures 51, Flock 60,
Zanzarah 77, Bugsnax 100, Temtem 165, Cassette Beasts 120, Monster Sanctuary
101. Handcrafting fifty disciplined 3D creatures costs roughly **$150k-$300k**.
The machine exists to not spend that.

## 2. The Spore correction

Spore is the obvious reference and the obvious trap. Fully procedural animation
over arbitrary morphology produces motion that is *plausible for anything* and
*characterful for nothing* — every creature moves like the same rubber toy.

The correction is a **hybrid**: authored performances stored
**morphology-independently**, then solved onto a specific skeleton with IK. The
performance carries the character; the solve carries the anatomy. Neither
alone gets there.

## 3. Eight to twelve morphology systems, not fifty

The unit of reuse is a **morphology system**, not a creature. Eight to twelve of
them — biped, planted quadruped, serpentine, flyer, and so on — cover a fifty
creature roster, and each system is a place where authored work amortises.
Fifty bespoke rigs is the failure mode this exists to prevent.

## 4. The four-layer stack

1. **Procedural locomotion** — gait, footfall, ground adaptation.
2. **Morphology-independent authored poses** — the character layer, the part a
   human actually makes.
3. **Procedural transitions and motion warping** — blending, retargeting,
   reacting to terrain and impact.
4. **Secondary motion** — cloth, flesh, tails, ears; the layer that sells
   weight and costs the least to add.

### What 240p forgives, and what it does not

It forgives **surface detail**: a face, a hand, subtle skin deformation. None of
that survives the resolution, so none of it should be paid for.

It does not forgive **silhouette and timing**. Those are exactly what reads at
240p, and they are exactly what bad animation gets wrong. The budget should move
from detail into silhouette clarity and timing.

## 5. Why this is a later wave, and why it prototypes on PC first

**The hardware creature plan is clip-oriented.** That is fine as a shipping
format and dangerous as a starting point: committing to static clips now would
freeze the animation architecture before anything has been learned about
whether the four-layer stack survives contact with a real creature.

So: prototype in the **PC/reference path**, where iteration is free and the
oracle already exists. Decide the architecture there. Then make the hardware
animation format a deliberate wave, informed by what the prototype showed,
after FRAMEBLIT integration and the FPGA fit have settled how much fabric and
bandwidth there is to spend.

Prematurely forcing static clips is the specific mistake this note exists to
prevent.

## 6. Money

Revised external spend: **$10k-$40k**, against the $150k-$300k that fifty
handcrafted creatures would cost. That gap is the entire argument for the
machine, and it is also the measure of how badly it goes if the machine does
not work — which is why creature number two is the checkpoint that matters.
