# Creature animation — the approach, and why it is a later wave

**Status: DESIGN NOTE, deliberately not started.** Recorded 2026-08-18 from the
owner. Animation architecture is a **later wave**, after `DEBUG.FRAMEBLIT`
integration and the FPGA fit work, and the first prototyping happens in the
PC/reference path rather than in hardware. §6 is the reason.

---

## 1. The problem, in the owner's words

> "Alright, so you basically told me I have to undercut the market somewhere
> between five and infinity orders of magnitude. I didn't expect it to be this
> bad. But I intend to attack this problem my way."

Two modes of attack, **only one of which has to work**, and they may complement
each other:

1. **Procedural animation.** Spore did it; others did too. With AI the iteration
   is fast enough to build bespoke procedural animation per creature. Bonus: it
   can animate through wonky scenes that static animation cannot handle.
2. **AI-assisted animation.** Not handing it to AI — working out, by trial and
   error, a process where the owner and AI together do the animating and
   modelling. Starting from no 3D experience.

> "I don't know how fast I'll be, but I have a suspicion it'll be like all my AI
> projects before this: I'll be faster than I have any right to be."

## 2. Why this changes the economics

The previous estimate assumed the standard chain:

    concept artist -> modeler -> texture artist -> rigger -> animator -> technical artist
    repeated fifty times

The proposal is a different equation:

    build ONE creature-production machine -> feed it fifty designs
    -> manually direct and repair the exceptions

That is not "undercutting contractors by working harder". It changes the unit
economics from **fifty handcrafted productions into one large tooling project
plus fifty parameter sets** — which is the kind of bet where being able to build
large amounts of software with agents actually matters.

## 3. The important correction about Spore

Spore is encouraging, but **not** because it proved code can replace animators.

Its final system was a **hybrid**. Animators created motion by familiar posing
and keyframing, but the result was stored in a **morphology-independent** form.
At runtime the system preserved the structural and stylistic relationships of
the performance, converted them into pose goals for a previously unseen
creature, and solved those goals with **inverse kinematics**. One authored
performance survived radically different skeletons and limb arrangements.

That distinction is the good news. The problem to solve is not "how can
mathematics invent a compelling attack performance" — it is "how does one
authored performance transfer onto fifty different bodies".

## 4. What 240p forgives, and what it does not

It forgives **surface detail** — faces, hands, subtle skin deformation. None of
it survives the resolution, so none of it should be paid for.

It does not forgive **silhouette and timing**. Those are what read at 240p, and
they are what bad animation gets wrong. Budget moves out of detail and into
silhouette clarity and timing.

## 5. The architectural issue for Zhaozhou

**The current hardware creature plan is still largely clip-oriented**:
compressed 30 Hz key poses, decoded pose caching, hard cuts, limited influences,
and no general animation-blending unit.

> **Do not let that prematurely force you into static clips.**

Prototype the creature-animation system in the **PC/reference path first**. Its
eventual hardware lowering could take several forms, and the choice should be
made with evidence rather than assumed now:

- run the procedural solver **offline** and bake reusable motion assets;
- run it on the **ARM** and cache poses shared by creatures in the same state;
- **quantize** slope, target direction and gait phase into reusable pose tuples;
- apply small runtime **IK or gait corrections through Transform Loom nodes**;
- reserve hardware **procedural modifiers** for feet, spine, head and secondary
  chains;
- use full CPU-derived poses only for **one or two close hero creatures**.

**Armies sharing states helps enormously.** Fifty instances of one species
walking at the same gait phase do not require fifty unrelated pose
calculations — and the hardware plan should be built to exploit that rather
than around it.

This becomes a **deliberate animation-architecture wave later**, after the
present FRAMEBLIT integration and FPGA fit work. It must not become a silent
assumption that all creature animation is baked.

## 6. The revised belief

The conclusion is no longer:

> "Fifty creatures require $150,000–$300,000, therefore they are impossible."

That was the conventional outsourcing answer. The revised one:

> **Fifty creatures are possible only if creature production itself becomes one
> of the project's major technologies.**

Which is entirely on-brand for Zhaozhou.

## 7. Money

Rough and **highly uncertain**: a successful automated pipeline could reduce
external creature-production spend to perhaps **$10,000–$40,000**, on top of the
**$5,000** concept work already done, with selective paid specialist rescue
rather than full outsourcing.

The real cost is engineering time, and the real risk is that the pipeline takes
longer to mature than expected.

> "You may be dramatically faster than a traditional production estimate assumes
> because you are not going to execute the traditional production process. You
> are going to attack the process itself until it becomes cheap enough. That
> does not guarantee success. But it is not delusion."

It is probably the only credible way a solo developer gets fifty distinctive 3D
creatures into this game without either becoming rich or accepting garbage.

## 8. The checkpoint that matters

**Creature number two.** The first creature always works, because it is made by
hand while the machine is being built around it. The second is the one that
tells you whether there is a machine.
