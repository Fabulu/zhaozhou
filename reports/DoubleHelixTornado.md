This is a diversion, but we haven't updated the Zhaozhou site in a while. We could use a good proof of concept for a strong spell. Here's the definition, try to get a render out of it after finishing the 53 MHz and whatever else important follows right after.

New question: I want a dual tornado spell. Level 9. Like Sacrifice's tornado spell, only with 2 tornados wildly rotating around each other, and carving in to the  terrain below them on their way making a helix shape. Is that feasible for our tech? I don't know what the tornadoes would be made out of. Particles? 2d Sprites? Some 3d mesh textured opaquely?

Very feasible. And I would not make the tornado out of just one thing. The best Zhaozhou version is a hybrid effect where each subsystem does the bit it is unusually good at.

The spell itself is actually an excellent Level 9 fit:

Two tornadoes orbit a moving common centre, sometimes swinging wide apart, sometimes nearly colliding, while their contact points gouge two persistent intertwining scars into the island.

That gives you the visual double helix carved into the world after the spell has gone.

The terrain part is almost tailor-made for us

Let the spell have a travelling centre C(t). The two tornado feet are roughly:

T1 = C(t) + R * ( cos θ,  sin θ )
T2 = C(t) - R * ( cos θ,  sin θ )

with noise/variation so it isn't mathematically sterile.

As C(t) travels forward and θ spins, the two feet trace:

     /\/\      tornado A
----/----\---- travel direction
   /      \

   \      /
----\----/----
     \/\/      tornado B

Together that's your ground-level double helix.

Each foot periodically issues a persistent terrain bake/stamp. Mantle already has persistent scar + breach machinery specifically for this kind of cumulative destruction.

You don't even need to stamp every frame. Stamp based on distance travelled:

every ~1–2 metres:
    gouge crater/bowl at tornado foot

Then successive overlapping stamps produce a continuous trench.

With perhaps an 8–12 m footprint, each tornado normally touches only a handful of 64 m patches. Even if both are actively chewing terrain, that's vastly smaller than the existing BAKE_PATCH_BUDGET = 64, which was sized to survive enormous Volcano-scale effects.

So:

double-helical permanent terrain carving: yes. Strong fit.

What should the tornado itself be?

My choice would be approximately:

50% procedural geometry/ribbons
35% soft particles/sprites
15% polygon debris

Not a giant opaque cone mesh.

Not 2,000 individual particles either.

1. The main tornado silhouette: procedural ribbons

This is what FORGE.PRIM ought eventually to be fantastic at. Its intended vocabulary explicitly includes:

ribbons;
tubes;
radial shells;
rings;
billboard sheets;
spline walls;
cones.

That's basically a tornado construction kit.

Imagine maybe 3–5 helical ribbons per tornado:

          /\
        //  \\
      ///   \\
     //      \\
    / /      \ \
   / /        \ \
  ////////\\\\\\\

Each ribbon winds upward around a narrowing/widening cone.

They don't need huge polygon counts. Something like:

24–40 vertical samples
×
3–5 ribbons

could create a convincing moving volume at 240p with a few hundred triangles.

Then animate:

angular offset;
varying radius;
vertical phase;
random lateral wobble;
width;
taper.

Two tornadoes would still be only perhaps 500–1,000 procedural triangles for their principal shape.

That's nothing scary for the geometry path compared with armies and terrain.

And importantly: because they're actual 3D geometry, they look correct when:

viewed from above;
viewed from the side;
one tornado passes behind the other;
the camera is close;
Duo cameras see them from completely different angles.

That alone makes them much better than a single billboard.

But don't make those ribbons opaque

We have proper fragment alpha blending in the architecture. RASTER.FRAGMENT supports ALPHA, ADD and ADD_MOD modes, and the soft-particle pipeline is explicitly intended to feed alpha/depth-faded effects into it.

So I'd make the tornado ribbons dithered/alpha-blended dusty strips, perhaps with a very simple scrolling cloud/dust texture once the TMU exists.

But here's the danger:

Transparency overdraw

If you make:

12 translucent shells
×
two tornadoes
×
each covering half the screen

then suddenly this cheap geometry becomes expensive fragment work.

That's the trap.

So rather than one beautiful translucent volumetric funnel, use sparse ribbons that imply volume.

At 240p, your brain does a lot of the work.

Something like:

3 dark dust ribbons;
2 pale highlight ribbons;
gaps between them;
lots of secondary particles.

The tornado remains visually porous.

That both looks turbulent and reduces fill.

2. Soft particles for the fuzzy body

Zhaozhou already has a very cheap planned soft-sprite endpoint: one soft sprite accepted per clock, no multiplier, no memory, basically a few adders/comparators.

So around the ribbons I'd spawn perhaps hundreds of:

dust puffs;
cloud wisps;
dirt sprites;
little translucent streaks.

They follow roughly:

velocity =
    tangential swirl
  + inward pull
  + upward lift
  + noise

And this is exactly what the Flow profile of Field exists for: particle/force-field programs driving Myriad updates. It reuses the one Field sequencer rather than requiring a special tornado engine.

That is a very Zhaozhou tornado:

The tornado isn't an animated model. It's a bounded procedural vortex field generating a visible population.

And because Field already exists, two vortex centres don't imply two hardware engines.

3. Polygon particles for rocks and shit

This is where I'd make the spell feel violent.

PART.EXPAND turns projected particles into ordinary little geometry triangles at one particle per clock, using essentially three adders and shifts.

So the tornado can carry:

dirt clods;
rocks;
fragments of structures;
leaves;
bones;
little shattered terrain chunks.

Close to the viewer, some particles become polygon shards.

Farther away they degrade to sprites/glints through the particle ladder.

Visually:

      dust
   . ' *   .
  /  o  ▲   \
 /  .  /|\ * \
| *      o    |
|   ▲  .    * |
 \     *     /
  \__rocks__/
      \ /
       V
     crater

A few pieces orbit low and extremely fast.

Other pieces shoot upward, orbit more slowly at altitude and eventually get thrown out.

That's what will sell the mass.

I would NOT use a traditional solid textured tornado mesh

A tapered cylinder/cone with a scrolling opaque texture is certainly the cheapest solution.

And I think it would look like 2001-era video-game tornado technology.

Which isn't necessarily terrible given the aesthetic, but you're specifically trying to exceed Sacrifice in systems spectacle.

A single opaque cone also causes visual problems:

looks solid;
intersection with ground looks fake;
close-up view reveals the geometry immediately;
the other tornado disappears behind it too cleanly;
creatures inside it become awkwardly occluded.

You want to see through the tornado intermittently.

An opaque shell could perhaps exist only as a faint inner dark core:

very cheap dark central spindle
+
transparent/ribbon exterior
+
particles

But I wouldn't let that be the primary effect.

Sprites alone also aren't enough

A pile of billboard sprites could absolutely make a tornado.

And our particle path is specifically built for that.

But two giant tornadoes circling each other means the player is going to see them from weird angles, including from above.

Pure billboards start betraying themselves.

So I would use sprites to make the tornado dirty and fuzzy, while 3D helical ribbons provide its coherent shape.

I'd build each tornado in layers
Core

A thin, dark, low-poly twisting funnel.

Maybe 50–100 triangles.

Not fully opaque—just enough that the centre always has visual weight.

Structural ribbons

3–5 procedural helices.

Perhaps 150–300 tris/tornado depending LOD.

These produce the recognisable rotating funnel.

Dust population

100–300 soft particles around each tornado.

Screen-space LOD aggressively reduces them with distance.

Debris

Perhaps 30–100 meaningful fragments in the two tornadoes combined.

Only nearby ones need polygon representation.

Ground contact

This should be an absolute mess:

dense dirt spray;
rock shards;
dust ring;
ground-darkening surface sheet;
terrain deformation;
strong tangential particle motion.

The foot is more important visually than the top.

The pair movement can get really nasty

Don't make them politely orbit at a constant radius.

Something more like:

shared centre advances

orbital radius:
    10m → 22m → 6m → 18m

orbit speed:
    accelerating/slowing

vertical lean:
    each tornado bends outward
    then snaps inward

phase:
    always approximately opposite
    but imperfect

Sometimes:

 T1                        T2
  \                        /
   \                      /

Then they race inward:

        T1    T2
         \    /

Almost touch:

          XX

Then fling apart again.

The terrain tracks underneath therefore aren't pretty perfect sine waves either.

They're a violent braided scar.

And the spell can interact with units without simulating CFD

Don't attempt real tornado fluid dynamics.

Gameplay can use two simple moving conical/cylindrical force volumes.

For a unit near tornado centre:

radial inward force
+
tangential force
+
upward force

HPS owns the unit gameplay response.

So you get:

small creatures lifted;
medium creatures dragged/orbited;
giants shoved but not lifted;
projectiles bent;
debris sucked in.

The Field Flow profile can supply essentially the same law for visual particles.

Thus the visible particles and gameplay units look as though they're responding to the same vortex without requiring one gigantic hardware fluid simulation.

And then the best bit

After perhaps ten seconds the tornadoes disappear.

But this stays:

       ___       ___
 _____/   \_____/   \_____
      \   /     \   /
       \_/       \_/

Two intertwined trenches.

Buildings hit along the way are gone.

Some holes may have punched all the way through thin terrain.

Creatures are scattered everywhere.

Debris has fallen into the void.

You can look at the map afterward and reconstruct where the spell went.

That's exactly what I think your Level 9 spells should do.

Level 10 changes the topology of the world.

Level 9 should leave the world looking like something terrible happened here.

Cost verdict

I'd roughly score it:

Part	Concern
Two-orbit path maths	1/10
Unit vortex gameplay	2/10 hardware; software job
Terrain helix carving	2–3/10
Procedural ribbons	3/10 once FORGE.PRIM exists
Soft dust particles	2/10 architecture
Debris	2/10
Alpha/overdraw	5/10 — main thing to watch
Whole spell	very plausible Level 9 effect

The one real performance constraint I'd establish is:

Tornado visual density is governed by screen coverage, not by a fixed particle count.

If both tornadoes are tiny on-screen: go fucking nuts.

If one tornado fills 70% of a Duo viewport: reduce ribbon count, particles and transparent layers aggressively.

The Measure is built for exactly this kind of problem.

So yes: I would absolutely put Twin Tornado / Double Helix / whatever we call it into the Level 9 roster. It uses almost every distinctive part of Zhaozhou—Field, Myriad, Forge and permanent Mantle deformation—without requiring some new insane subsystem.
