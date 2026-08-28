# OWNER FEEDBACK — READ BEFORE THE NEXT PASS

**From Fabian, 2026-08-28. Relayed here by the HARDWARE session, unchanged.**

He put this in `reports/zixxheadadvice` and, when it did not reach you, again in
`reports/zixxheadadvice2`. Both landed in commits your session had already moved
past, so pulling them does not deliver them to you. He asked any other agent
that picked them up to say so; I did that in
`reports/zixxheadadvice_NOT_THE_ZIXX_AGENT.md`, and I am putting a copy here
because this run folder is the one place your session reads and writes every
pass.

I have changed nothing about the creature and hold no opinion on the geometry.
His words follow verbatim.

---

Zixxtrixx has been having head issues. Agent is going in the wrong direction. If you are not zixx agent, throw up an md here saying so so I can re-place this file for him. When you pull it, he won't get it and you can't give it to him.

Here's bro advice: The head is beyond fucked. It gets worse every iteration. And it still droops and looks down and the S still refuses to bend to get it up there.

Yeah. This pass is still solving the wrong problem. The repo more or less confesses it: the latest model wave says “the S silhouette is kept” while trying to fix the head with attitude, taper, skull blending, body height, and pivot changes.

That is exactly backwards.

The head’s baseline position and apparent gaze are primarily determined by the final third of the S-curve. A local head joint can rotate the skull, but it cannot make a descending neck suddenly read as a proud, upward-held head. You can mathematically measure the snout axis as “horizontal” while the entire skull mass still hangs from the bottom of a downward hook.

So each iteration has done some version of:

preserve the descending S → enlarge/fatten/re-angle the head → rotate it locally to compensate

That makes the wrong mounting increasingly visible. The fatter and more detailed the head becomes, the more grotesque the droop reads.https://github.com/Fabulu/zhaozhou/tree/main/reports

The published closeout even admits that a notch remains at the head/neck junction, despite declaring all the mechanical gates green. That tells us those gates are not testing the thing you care about. Ground contact, overlap allowance, deterministic clips, and a measured snout vector can all pass while the animal still looks completely fucking wrong.

The next pass must not be another “head pass”

It needs to be a front-S reconstruction.

Freeze the following for now:

skull dimensions;
eyes and pupils;
texture and colours;
atlas resolution;
general body girth;
head pivot.

Then deliberately change the front 30–40% of the centreline:

Raise the entire upper arc. The neck must climb toward the head instead of descending into it.
Distribute that lift across several neck bones. Five to eight bones should share a smooth bend. No single hinge should carry the correction.
Let the final neck tangent point slightly upward into the skull. The head should continue that tangent rather than counter-rotate against it.
Return the head attitude bone close to neutral. It should provide small expressive look adjustments, not compensate for a fundamentally wrong body curve.
Permit the S to change. The current S is not sacred. Preserving it is preserving the failure.
Re-pin the animation goldens afterward. This is an intentional pose correction; bit-identical preservation of the wrong front arc is not a virtue.

The clean architecture is a spline constraint rather than more angle constants:

Fix one anchor in the mid-body, author the desired head centre and desired snout tangent from the concept, then generate a smooth C¹-continuous neck curve between them.

That gives the system explicit answers to:

where the head must be;
which way it must face;
how the neck arrives there;
how to avoid the junction notch.

Today the head position is an accidental consequence of a preserved S plus local rotation. That is why it keeps oscillating.

The acceptance gate also needs replacing

The only meaningful visual gate for this repair is a fixed, concept-matched side view:

overlay the model silhouette on Side.png;
measure the head centre’s screen/world height relative to the S crown;
measure the world-space snout direction;
measure the neck tangent immediately behind the skull;
limit the angular discontinuity between those two;
reject any visible inward notch at the junction.

No three-quarter beauty shot. No “the probe says the axis is horizontal.” No acceptance based on the head alone.

The instruction to the agent should be blunt:

Stop modifying the skull to compensate for the body. The head is the endpoint of the S, and the S is wrong. Break the existing S intentionally. Raise and re-curve the whole front third so the neck carries the head upward. Distribute the correction over the neck chain, keep the head joint nearly neutral, and accept only against the fixed Side.png silhouette. Do not touch eyes, texture, colours, or head size until the head position, neck tangent, and unbroken contour are correct.

The head is not getting worse because the model cannot make a head. It is getting worse because every pass treats the head as a detachable object when the concept treats it as the swollen conclusion of the whole snake.

---

*Relay note: the two copies differ only in that a stray
`https://github.com/Fabulu/zhaozhou/tree/main/reports` is glued to the end of
one paragraph in the second. Nothing else changed between them.*
