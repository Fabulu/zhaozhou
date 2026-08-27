This report has nothing to do with Zhaozhou but with Tribute Upheaval. It's for the creature animator, texturer, and modelling agent. Some good advice from bro:

Okay, this new Zixxtrixx pass has been fucking monumental. Many things are still wrong. Head, pupil rotation, head and neck pose, fins, falling animation. But...

It looks fucking sick and animation is bendy and organic and oh my god this whole project is actually gonna work out!

Go and really thoroughly check up on the animation and what's been done, we need to analyze the shit out of this for the ages.

Walking is perfect. Idle has one bit that is a bit too static but is otherwise perfect. Salto looks good and the issue are mostly launch and attack vector, lack of a cool jump start that compresses the snake S then shoots up, the spear being a bit too rigidly straight, and the camera not following it. But the animation is god dang mint. And yeah, falling is kinda shot but we'll figure it out, I told it to wobble more. Go check it. It looks better than any indie game I've seen and most commercial games. I see a bit too many polygon edges. Maybe we don't use enough of them or we need some poly smoothing technique? What do you say?
This is the moment the creature stopped being a technical demo

Yes. This pass is monumental. Not “good for AI,” not “good considering the hardware,” not “promising once polished.” The walking animation is already a genuinely strong piece of character animation.

I could not pull the live Cloudflare Pages playback directly through this environment, so I am not going to pretend I watched the current site stream independently. I went through the latest committed animation source, camera code, renderer, measurements, and the evidence package. Your live visual diagnosis matches the implementation almost uncannily.

The current Zixxtrixx is built from one continuous two-weight body, 20 spine bones, 57 body stations and 28 vertices around each ring, with roughly 4,000 compiled vertices. Every clip is authored at 30 Hz but presentation-interpolated to 60 Hz with normalized quaternion interpolation. The full S, the waves, contact compensation, root motion and secondary movement are all being composed rather than switching abruptly between canned poses.

That combination is the breakthrough:

continuous skin + distributed shape animation + mathematically preserved contact + layered secondary movement

The creature is no longer a collection of parts being rotated. It is a deforming body.

The remaining problems are real, but they are now local artistic and presentation problems. They are not evidence that the creature system cannot work. The walk proves that the system can work spectacularly.

Walking: lock this fucking thing down

The walking animation is the strongest evidence for the entire Tribute Upheaval project.

Its primary movement is not a generic sine applied to every joint. The agent authors a non-negative height field over the grounded section, moves a compact cosine-squared hump through it, and converts the desired segment heights back into exact joint angles with integer asin. The underlying S remains intact while the grounded run bunches upward. A separate front-body wave moves through the raised section, a breathing term produces a visible surge, and the tiny lateral sway is restricted to the elevated body so it cannot ruin ground contact.

The root compensation is especially important. Any deformation of the elevated front changes the accumulated vertical position of the grounded belly. Instead of eyeballing an opposite root offset, apply_stance calculates the displacement from the difference in sine sums and compensates it exactly. That is why the body can wave violently while the belly still appears to bear weight.

This is why it reads as organic locomotion rather than an animation playing on a model:

The wave has a clear direction of travel.
Different parts respond at different phases.
The body changes shape rather than merely changing position.
Contact constrains the movement instead of being an accidental result.
Forward speed is matched approximately to the travelling hump.
Secondary sway is subordinate to the defining vertical movement.
The head participates without becoming the sole source of motion.
The 60 Hz interpolation removes the held-key stepping without changing simulation timing.

The final diagnostic pass measured the belly within roughly −10 to +12 mm while the head-top travel became about 10 screen pixels at the accepted camera distance. Those numbers are not what make the animation good, but they confirm what the eye sees: a planted lower body under a visibly moving upper body.

Do not “improve” walking while fixing the other clips

Treat the current walk as a golden master.

The danger now is that an agent changes the shared S, head hierarchy, smoothing or body proportions while working on the idle or fall and quietly damages the one thing that is already right. Keep:

the present walk clip;
its fixed camera;
the current contact sheet;
the current probe output;
a video/GIF of the accepted version;
the exact source commit.

The walk is no longer an experiment. It is the species locomotion reference. Everything else should be evaluated against its standard of distributed motion and clear weight transfer.

And more broadly, this is a reusable animation grammar. The same “author a deformation field, solve the bones, compensate contact” method can animate tentacles, roots, necks, tongues, worms, fabric-like creatures and spell-grown organic structures. That is why this matters beyond Zixxtrixx.

Idle: I think I can identify the static part

Your “one bit is too static” observation has a very plausible structural explanation in the source.

The idle’s front wave operates only before the grounded section. The tail sway begins only after the grounded section. The long middle run—roughly spine segments 11 through 16—is deliberately protected from both because earlier lateral movement there drove it into the terrain.

So the animation currently has:

animated front lobe | protected grounded dead zone | animated raised tail

The middle still receives the global girth change, but its centerline is essentially the anchor. That is almost certainly the piece reading static.

I would not fix it with another obvious vertical caterpillar hump. Then idle starts looking like low-speed walking.

The best addition is a very small secondary deformation that does not alter ground height:

A slow torsional breathing wave

The body runs approximately along its local X axis, so a tiny local-axis roll can travel through the grounded section without lifting its centerline. Something like one or two visible degrees, phase-lagged behind the upper-body breath, would move the dorsal stripe and crayon marks enough to show that the flesh is alive while retaining the grounded anchor.

Alternatively, a broad longitudinal compression could pass through the grounded run: neighboring joints shorten the horizontal footprint slightly and recover, with equal-and-opposite corrections so the belly does not translate. That is harder but may look even more animal-like.

The important principle is:

Keep the middle planted, but do not keep its surface and internal tension frozen.

The idle already has a strong layered structure: breath/deepen, exact root rise, front wave, slight head yaw, global girth and independently timed tail sway. A tiny middle-body aftershock is enough. It does not need another headline movement.

Salto: the animation is good; the launch, trajectory and camera logic are the real defects

The salto’s underlying animation architecture is strong:

The canonical S loses authority.
The body coils into a wheel.
Bone 0 rotates the entire coil through three somersaults.
Root displacement repivots that rotation around the coil centre instead of the nose.
The body unrolls into the attack pose.
The tip is authored to penetrate the ground.
It remains stuck for five seconds.
It extracts before the fourth turn restores the S.

That is far beyond the original tail-wag attack.

Your problems with it are not vague taste complaints. They map to concrete things in the curves.

1. It does not have a real anticipation/compression phase

The current first sixteen keys transition roughly like this:

canonical S authority: 1000 → 450 → 0
coil: 0 → 350 → 1000
lift: 0 → 40 → 560 mm
forward travel: effectively zero until key 12

So it changes from S into coil while beginning to rise, but there is no unmistakable “load the spring” pose.

The launch should be broken into three visually distinct beats.

Compression

For perhaps six to ten keys:

deepen the S beyond its idle extreme;
draw the grounded rear forward;
tuck the head slightly into the hook;
lower the apparent centre of mass;
close the fins a little;
shorten the creature’s screen-space length.

The pose should unmistakably say stored energy.

Hold

Two or three keys where the compressed pose is almost still. That pause is what makes the next movement feel explosive instead of merely continuous.

Release

A wave should run from the grounded tail/rear through the S and launch the front upward. The root should not begin with a smooth generic rise; it should accelerate sharply after the compression releases. The body can begin rolling up only after the propulsion is already legible.

This could be implemented with another authored preload curve feeding apply_stance, rather than inventing a second animation system.

2. The spear orientation and the actual velocity vector disagree dramatically

This is probably the largest technical reason the attack vector feels wrong.

The creature is held at 30° from vertical during the final attack. But immediately before impact, its root drops almost straight downward.

From the current values:

key 52 lift: 3800 mm
impact lift:
3317 tip drop − 542 body height − 420 burial = 2355 mm
vertical change: approximately 1445 mm downward

Meanwhile, forward travel only goes from roughly 1883 mm at key 52 to 1900 mm at key 53—about 17 mm forward.

A spear travelling along an axis 30° from vertical should move horizontally by roughly:

1445 × tan(30°) ≈ 834 mm

It currently moves around 17 mm.

So the creature points like a diagonal javelin but moves almost vertically. The eye reads that immediately even if nobody can articulate the equation.

There are two coherent solutions:

Keep the 30° attack pose and add roughly 800 mm of forward movement during the final plunge.
Keep the almost-vertical trajectory and make the spear almost vertical.

Your stated direction seems clearly to favor the first. The final eight to twelve frames should have the tail tip moving nearly along the creature’s own long axis. The exact root curve can be solved from the desired tip trajectory offline.

That would transform the hit from “rotating object moved downward” into a body throwing itself point-first along a line of force.

3. The camera exists, but it follows the wrong thing

The code does now contain a tracking camera. It tracks:

100% of the authored forward root path;
85% of the authored vertical root path.

It intentionally ignores the coil repivot displacement.

That is not the same as following Zixxtrixx visually.

During the sequence, the creature changes between:

a folded S;
a wheel;
a rotating coil;
a 3.8-metre diagonal spear;
an embedded line sticking out of the ground.

The visual centre can move by nearly half the creature’s length even when the root stays in the same place. Following the root flight curve cannot keep all those shapes framed consistently.

The reel should calculate a presentation focus point, not reuse the animation root:

S / launch:       centre of visible skinned bounds
coil:             coil centre
unroll:           blend coil centre toward spear midpoint
plunge:           midpoint between head and blade tip
impact:           bias toward the contact point

Because this is reel-only presentation, the cheapest robust solution is simply to skin the creature, calculate its world-space or screen-space bounds, select a target inside those bounds, and smooth that target with a damped camera. No hardware or game-simulation contract needs to change.

The camera should also have a little inertia. Perfectly pinning the centroid can feel robotic. A small follow lag during launch, then a faster catch-up during the plunge, would make the movement feel larger.

4. The spear should be straight in intent, not mathematically dead

The current code makes it completely straight and completely still through the long stick phase. That was faithful to the earlier instruction, but your new visual verdict is right: the body has become so organically animated elsewhere that absolute rigidity looks imported from a different creature.

Keep the strong spear silhouette, but add:

a two-to-four-degree elastic bow during the plunge;
a compression kink at impact;
a damped wave travelling from the embedded tail toward the head;
a brief fin shudder;
then a gradual settlement into the straight pinned pose.

The mean axis remains straight. The body still reads as a weapon. It merely behaves as a flexible biological weapon rather than a steel pole.

That small recoil could be one of the coolest parts of the whole move.

Falling: “more wobble” is right, but amplitude alone will not solve it

The present fall already has a lot of machinery:

one full whole-body tumble per 3.2-second loop;
additional root roll and yaw;
a neck motion made from one- and two-cycle waves;
a travelling lateral wave over almost the whole spine;
a middle-body roll/twist;
independently moving fins;
60 Hz pose interpolation;
no ground contact.

And yet it can still look wrong because of the hierarchy of those motions.

The dominant action is a coherent 360° rotation of the entire fully formed S. All the waves are layered underneath that. The result can still read as:

an S-shaped rigid sign rotating through space, with some wiggles attached

rather than:

a flexible creature whose different masses are being dragged through an uncontrolled fall.

There are four things I would change.

First, evaluate it with a fixed camera

The fall presentation currently orbits the camera while the creature itself tumbles on three axes. That creates two simultaneous rotations, and they can cancel, reinforce or obscure each other unpredictably.

Walking became legible once the camera stopped competing with it. Falling deserves the same test:

fixed side camera;
fixed three-quarter camera;
only after the animation reads, restore an extremely slow orbit.

This may reveal that part of the “bad animation” is actually bad presentation.

Second, stop preserving the S at full authority every frame

The source explicitly applies the complete canonical S first, every frame, before adding the flail.

That protects the character’s signature, but it also prevents the body from ever truly going slack.

Let S authority vary, perhaps between 45% and 100%. Sometimes the fall should momentarily pull the creature almost straight; sometimes inertia should compress it into a tighter S; sometimes the head and tail should form competing curves.

The signature S should recur, not be held like a logo throughout the fall.

Third, use inertia and phase lag rather than more synchronized sine

“Wobble more” should mean:

head lags the root turn;
neck overshoots the head;
middle follows later;
tail follows later still;
fins flutter with their own drag;
direction reversals propagate rather than happen everywhere simultaneously.

The ideal solution is a tiny deterministic offline spring-chain simulation:

drive the root with the desired tumble;
give each joint angular stiffness and damping;
give the head, trunk, tail and fins different inertia;
add gentle aerodynamic drag;
bake the resulting quaternions into the 30 Hz clip;
use the existing 60 Hz interpolation for presentation.

That costs nothing at runtime and will generate exactly the passive, delayed, organic movement that is difficult to hand-construct from sums of sines.

Walking benefits from authored intentional motion. Falling is the opposite: it should benefit from baked secondary physics.

Fourth, make the angular velocity nonuniform

A perfectly uniform full revolution reads like a display turntable. Let it:

accelerate when elongated;
slow as the body catches air broadside;
hesitate during a large fold;
then tip into the next rotation.

Still deterministic, still looping, but no longer metronomic.

Head, pupil and fins: these are now separable problems

That is good news. They do not require another creature-system rebuild.

Head and neck pose

The awkward structural fact remains that the head is effectively at bone 0, which is also the root. The code itself notes that no ordinary joint wave can move the head’s position; visible head bob comes from root compensation.

There are currently only 25 bones—20 spine bones and five tail/fork bones—under a 32-bone limit. So there is room for a cleaner hierarchy.

I would spend at least one spare bone on a dedicated skull/head attitude control. That lets the head:

remain level while the neck completes the S;
tuck during launch anticipation;
lag during falling;
make small idle gestures;
stop forcing every head correction into the first few spine slopes.

A more ambitious later cleanup would introduce a neutral root/centre-of-mass bone and make the spine a child of it. That would simplify root motion, camera tracking and ground placement. But do not destabilize the perfect walking clip casually; preserve it before changing the hierarchy.

Pupil rotation

The current pipeline transposes and maps the eye crop because U and V do not correspond intuitively to image horizontal and vertical. The intention is documented, but your visual verdict says the resulting orientation is still wrong.

Do not reason about it for another hour. Paint an asymmetric debug arrow onto each eye location and render the front, left and right views. Then test the eight cheap transformations:

0°, 90°, 180°, 270°
each with and without mirroring

The two eyes may need different transforms because the opposite side of a wrapped surface has opposite handedness. One patch duplicated at U=0 and U=32 does not necessarily preserve the same apparent pupil orientation on both sides.

This is a ten-minute visual search, not a geometry problem.

Fins

The fins are still produced as seven elliptical rings with eight sides each. In other words, they are extremely flattened tubes.

That was enough to establish “flat and pointy,” but it is not an ideal representation of a blade/leaf/fin.

The proper fin should be a small custom mesh:

one broad upper face;
one broad lower face;
a narrow bevel or rim;
a sharp tapered tip;
controlled root curvature;
hard shading on the broad planes;
smooth shading only on the bevel.

Do not solve the fins by simply adding more rings. Their problem is topology, not resolution.

The polygon edges: yes, I think I know exactly what you are seeing
The primary problem is flat shading, not an absurdly low polygon count

The creature currently has 28 sides around its body and 57 stations along its length. At 384×240, that is already a respectable amount of geometry.

But the renderer calculates a normal independently for every triangle, calculates the key and fill lighting from that triangle normal, and sends one constant lighting modulation across the entire triangle. The source explicitly calls it “flat per face.”

That means every polygon is being intentionally announced by the lighting.

Increasing from 28 to 40 sides would make the angle between adjacent side faces smaller, but every boundary would still be a discontinuity in illumination. It would improve the silhouette while leaving much of the visible interior faceting.

So my verdict is:

Gouraud/smooth normal shading first. More polygons second.

The right smoothing technique

For the body and head:

Generate a normal per vertex offline.
For an elliptical ring, derive the normal from the ellipse rather than merely pointing radially.
Transform each normal through the rotational part of its two bone matrices.
Blend with the same two skin weights.
Renormalize.
Calculate the key/fill illumination at each vertex.
Interpolate the lighting across the triangle.

You do not need per-pixel Phong shading. At 240p, Gouraud shading should remove most of the distracting faceting.

Because the light rig has colored ambient and fill, the nicest version interpolates an RGB lighting multiplier per vertex. A cheaper version could interpolate the two Lambert terms and compose the final color later.

Preserve the crayon character

Purely smooth diffuse shading could make Zixxtrixx look like glossy digital plastic. I would deliberately retain a little structure:

final light = 80% smooth vertex light + 20% face light

Or quantize the smooth result onto a moderate ladder.

That gives you:

smooth round body;
readable crayon grain;
subtle hand-cut planar character;
no distracting polygon chessboard.

Use smoothing groups:

smooth: body, neck, skull, eye bulges;
hard: fin faces, fin rims, mouth edge where appropriate, middle spike;
mixed: nose and tail transitions.
More polygons still have a role

At 28 sides, each radial face covers roughly 12.9°. In very close head shots, the silhouette can still show facets even under smooth lighting.

The current meshlet arrangement duplicates the texture-seam vertex, so 28 sides become 29 vertices per ring. Two rings fit under the 64-vertex meshlet limit. The source notes that 32 sides would become 33 vertices per ring and two rings would no longer fit.

A cheap interim improvement is 30 sides:

30 sides + seam duplicate = 31 vertices/ring
31 × 2 = 62 vertices

That still fits the present two-ring meshlet arrangement. It is only a modest improvement, but essentially free architecturally.

For a larger jump, alter meshlet splitting and use different segment counts by region:

head: 36–40;
front body: 30–32;
rear body: 24–28;
thin tail stem: 16–20;
fins: custom topology.

The ring zipper already supports differing ring counts, so this is conceptually compatible, though it will create more meshlet boundaries.

If the visible edges are along bends rather than around the circumference

Then more radial sides will not help.

The body has 57 rings but only 20 spine bones. That is about 160 mm per spine segment. The tight neck and 150° doubling-back S can expose the underlying bone spacing in the silhouette.

There are seven spare bones. I would allocate several nonuniformly:

one dedicated head bone;
one or two extra neck bones;
one or two extra bones around the sharp S reversal.

The long, nearly flat grounded run needs fewer bones than the dramatic front hook. A nonuniform spine is more valuable than simply increasing every polygon count.

Smooth shading will hide lighting discontinuities. Extra strategically placed bones will fix actual silhouette hinges and volume collapse. They solve different problems.

One diagnostic render settles it

Have the agent render the same close-up three ways:

current textured flat lighting;
completely unlit/fullbright texture;
wireframe or normal visualization.

Then:

Jagged outline in fullbright = more geometry/bones.
Smooth outline but visible tiles only under lighting = smooth normals.
Kinks only during extreme bends = bone distribution/weights.
Weird planes mostly on fins = replace fin topology.

My bet is that most of what bothers you is flat triangle lighting, with a smaller amount of genuine bone/silhouette faceting at the neck and head.

What this pass proves about Tribute Upheaval

The important thing is no longer “can an AI make one creature vaguely move?”

It has now demonstrated:

a reusable continuous flexible-body format;
texture extraction from the actual concept drawing;
a real texture page pipeline;
crack-free two-bone skinning;
layered deterministic animation;
exact terrain-contact preservation;
dramatic whole-body aerial animation;
presentation interpolation;
evidence-driven visual iteration;
a genuinely excellent locomotion cycle.

The evidence run now preserves before/after comparisons, walk/attack/fall contact sheets, idle breath sequences, impact frames, head views, texture tiles and final clip reports. That is exactly the historical record this deserves.

The first terrible remake was not proof that AI art direction would doom the project. The monumental recovery is stronger evidence in the other direction:

The machinery can support real character.
Visual criticism can steer the agent.
The agent can perform repeated render–look–adjust loops.
And the result can cross from “functional” into “fucking sick.”

Walking should be frozen. Then I would attack the remaining work in this order:

Smooth body/head normals so the existing excellent animation is presented properly.
Dedicated head attitude control, then fix the head/neck pose.
Pupil UV calibration with brute-force render variants.
Salto anticipation, axis-aligned final velocity and visual-centroid camera tracking.
Custom blade meshes for the fins.
Falling from a fixed camera, then replace coherent sine layering with baked inertial spring motion.

This is no longer “maybe the project can work.”

It works. Now you are art-directing it.


---

## Response from the animating agent (2026-08-27, RUN-20260827-0628, pass 3)

Read mid-run, after the pass-3 items had already landed. Where this report
and the pass-3 verbatim brief overlap, the brief was followed; several of
the report's headline defects were already fixed by the time it arrived:

- **Spear axis vs velocity vector**: fixed exactly as option 1 -- the dive
  keys now share one t^2 ramp across lift AND forward drive (9645 mm down,
  5570 mm forward), so the plunge travels along the spear's own 30-deg
  line. It is ~11 m of straight shot now (the apex went to 12 m).
- **Camera**: the tracking camera aims at a blended focus point -- root
  through the coil, spear midpoint from the unroll through the dive, the
  impact and the whole stick (kAtkAim). The burial is framed; verified on
  the frames around contact. Full skinned-bounds tracking with damped lag
  remains a good future step.
- **Pupil**: solved exactly as prescribed -- brute-force render variants
  through an EYE_ROT_DEG knob (12 shipped); derivation failed three times
  first, as predicted.
- **Fins/head/pink**: raked, both-coloured, bigger lifted head, softened
  pink (the pass-3 brief's items).

Taken from this report IN this pass (addendum commit):

- **Salto anticipation**: compress (kAtkPre deepens the S, belly planted by
  the exact-compensation mechanism) -> 3-key hold -> explosive release
  (lift 0 -> 3.2 m in six keys). The wind-up beat moved into the release --
  during the grounded compress it floated the rear 750 mm (probe caught it).
- **Idle's static middle**: the torsional breathing wave, as suggested --
  kIdleTorsion rolls the grounded run +-4 deg, travelling, phase-lagged
  behind the breath; centreline unmoved, probe band [-8..-3] mm.
- **Fall turntable read**: tumble phase warped by kFallTumbleWarp*sin(phase)
  -- hesitates and tips instead of metronoming; loop seam exact.
- **Polygon edges**: agreed with the diagnosis -- the dominant cause is the
  flat per-face lighting, and smooth vertex normals are a RENDERER/hardware
  -lane decision (the reference implementation defines what silicon must
  produce), so they are deliberately NOT smuggled in through authoring.
  Took the one free authoring step: kSides 28 -> 30 (31 verts/ring, two
  rings still fit the 64-vert meshlet). One correction for the record: the
  mirror note cites zhao_reel.cpp:1649 `kFaceShade[ti / 2]` as the creature
  shading -- that line is the DEBRIS-CUBE path. The creature's per-face
  light rig lives in the zref reference renderer, which is exactly why
  smoothing it is a hardware-lane call and not a reel patch.

Deferred, deliberately, with agreement they are the right next steps:
Gouraud/smooth normals (hardware lane decision first), custom fin blade
topology, a dedicated head-attitude bone (touches the golden-master walk's
hierarchy -- wants its own run with before/after), spring-chain baked fall
secondary motion, S-authority variation in the fall, fixed-camera fall
evaluation subject. The walk was treated as the golden master throughout:
its motion is untouched and its probe band re-verified every build.
