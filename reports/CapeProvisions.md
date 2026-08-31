Please implement:

I want a main character with a cape. You'll see him a lot. Cape will flutter in the wind. Do we need special stuff to support that?
Yes—but not a cloth processor

For the cape you describe, Zhaozhou needs a small secondary-motion pose system, not dedicated cloth-simulation hardware.

The existing geometry architecture is already a good fit for a bone-driven cape:

Creatures have a hard limit of 32 bones.
Vertices may have at most two bone influences.
GEOM.SKIN is explicitly designed for rigid and two-weight vertices.
Its workload target is roughly 120,000 skinned vertex instances per frame, so another few hundred vertices on the main character are basically noise.

The important action is therefore: reserve cape bones now.

What I would give the cape

Use the existing shoulder/spine bones as the attachment line, then give the cape:

Six cape-specific bones for an ordinary waist/thigh-length cape: two columns × three rows.
Eight cape-specific bones for a long, dramatic cape: two columns × four rows.

A single chain down the centre can flap forward and backward, but it cannot convincingly billow asymmetrically, curl around one side, or twist during a turn. Two columns let the left and right sides react independently while remaining extremely cheap.

Every vertex can blend between two neighbouring cape bones, which maps exactly onto Zhaozhou’s two-weight skinning restriction. No general per-vertex deformation is required.

I would aim at roughly 150–300 visible cape triangles, perhaps somewhat more if you make it a thin closed shell with an independently coloured inside. Even doubling that for a fancy hero cape is trivial compared with the army-scale skinning budget.

The actual missing feature: per-instance pose overrides

This is the one genuinely special piece.

GEOM.POSE currently caches a decoded pose by approximately:

{creature type, clip, frame}

Instances playing the same clip frame share that pose. That is excellent for armies, but a cape reacting to wind is instance-specific. You cannot modify the shared pose palette’s cape bones without modifying every instance that shares it.

So the eventual path should be:

shared authored clip pose
        ↓
base bone palette
        +
small per-instance secondary-motion patch
        ↓
final palette selection/composition
        ↓
GEOM.SKIN

The patch should contain only the procedural bones:

instance id
cape-bone override mask
6–8 local rotations
optional root spread/lift value

Do not upload a fresh set of 32 full matrices every frame. The creature architecture explicitly rejects per-creature uploading of all 32 matrices; it destroys the pose-cache economy. A bounded six-to-eight-bone patch preserves the shared body pose and changes only the cape.

GEOM.LOOM is conceptually the right place to compose those parent-before-child transforms. The current ruling says that ARM/compiler supplies a topologically sorted graph while Loom merely composes transforms; it does not perform recursion, gameplay logic or arbitrary simulation. Loom is still only specified, though, so this capability does not exist as a finished path yet.

Where the flutter simulation should run

On the game side—HPS/shared runtime—not as FPGA cloth hardware.

The solver only has six or eight moving nodes. That is utterly trivial software work. Each frame it would use:

relative wind = world wind − character velocity

plus:

Character acceleration, so the cape lags when starting and stopping.
Turning acceleration, so it swings outward during sharp turns.
Gravity.
An authored target pose from the current animation.
Spring stiffness and damping.
Bend, twist and spread limits.
A gust/noise value.

The authored animation should provide the intention—cape tucked during a roll, lifted during a jump, spread during a dramatic spell—while the procedural solver provides wind and inertia around that target.

That distinction matters because the normal creature clip system uses 30 Hz authored keys, hard-cut transitions and no animation-blending unit. Purely baking cape flutter into every clip would work technically, but it would look canned on a character you stare at constantly, and it would not respond correctly when the same animation plays in a different wind direction.

Make the spring solver deterministic and fixed-timestep, using the shared PC/ZEMU/HPS implementation. Then authentic PC mode and the console produce the same cape motion. The PC’s optional high-resolution mode may subdivide the surface visually, but it should remain driven by the same six-to-eight simulated bones.

Wind does not require a separate hardware system

Do not sample a flow field for every cape vertex.

The cape needs one wind sample near the shoulders, perhaps a second near the lower edge if you want tall gusts to bend it differently. That sample can come from:

The ordinary game-world wind function.
A Field program.
Scripted spell or weather forces.
A combination of those.

The existing FIELD.SEQ.FLOW plan is not a separate hardware unit anyway; it is a program/profile executed by the shared Field engine, and even its profile-specific lane binding remains open. The cape therefore should not depend on completing some new Flow accelerator.

Likewise, this is not a reason to revive GEOM.WARP. That block is intended for general per-vertex deformation and is currently deferred. A skeletal cape is cheaper, more controllable and better matched to a hero asset.

Collision is the other quality requirement

Because this is the main character, the thing that will make the cape look cheap is not imperfect fluid dynamics. It is constantly passing through his torso and legs.

You need only simple local collision:

One upper-back/chest plane or capsule.
Two hip/thigh capsules.
Possibly one pelvis capsule.
Angular constraints preventing the upper cape from folding through the shoulders.

The solver pushes cape nodes outside those proxies and then re-enforces segment lengths. There is no need for cape-triangle-versus-character-triangle collision.

World collision can initially be extremely crude. A cape occasionally brushing through a wall is tolerable; a cape permanently living inside the wizard’s arse is not. For cinematics or tightly framed scenes, clip-specific authored targets can keep it under control.

The cape’s inside surface needs a decision

The rasterizer’s coverage path already handles both triangle windings by flipping a negative-area triangle rather than simply dropping it, so the cape will not disappear merely because the camera sees its reverse side.

But lighting the reverse side is a separate issue. A paper-thin single surface carries only one authored normal direction. When seen from behind, its cel bands may be wrong.

The least troublesome solution is to model the hero cape as a very thin closed shell:

Outside surface with outward normals and its primary colour.
Inside surface slightly offset, with inward normals and potentially a darker or contrasting colour.
A narrow stitched edge around the silhouette.

That avoids requiring a special two-sided lighting shader and makes flips, curls and dramatic overhead shots read properly. The added geometry is tiny for one main character.

What should be added to the plan

The cape does not warrant:

An FPGA cloth simulator.
Per-vertex wind evaluation.
General cloth-versus-world collision.
A new rendering engine.
A dependency on GEOM.WARP.
Uploading a complete unique skeleton palette every frame.

It does warrant four small provisions:

Reserve six to eight bones under the 32-bone creature limit.
Add a bounded per-instance secondary-pose patch between shared pose decoding and skinning.
Implement a deterministic spring-chain/lattice component in the shared game runtime.
Support a few authored body collision proxies and clip-specific cape targets.

That should produce a cape that lifts in wind, trails during movement, swings during turns, settles naturally, behaves during attacks and jumps, and remains cheap enough that it is essentially irrelevant to the FPGA budget. The special support is real, but it is a compact animation feature—not another giant hardware adventure.
