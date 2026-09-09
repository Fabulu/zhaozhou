What? We need lightning in the game. Do we not have a block provisioned to help? One of the effects blocks?

Yes. You’re right. I conflated “not available for Creature 02’s immediate reel render” with “not provisioned for the game.”

Zhaozhou absolutely has machinery intended to make lightning. It is not one dedicated LIGHTNING block; it is a composition of existing effects blocks:

deterministic bolt path
        ↓
FORGE.PRIM ribbon geometry
        ↓
RASTER.FRAGMENT additive blend
        ↓
POST.GATHER glow tag
        ↓
POST.COMPOSITE bloom

plus:
PART.* sparks / streaks / glints
The primary lightning block is FORGE.PRIM

FORGE.PRIM explicitly includes a ribbon primitive with up to 64 segments. The architecture deliberately removed “chain” as a separate hardware family because a chain is already a ribbon, tube, or repeated meshlet use. That is almost a direct description of a lightning bolt: a bounded jagged path expanded into a thin strip of triangles.

For a ribbon:

one segment is two triangles;
16 segments is 32 triangles;
32 segments is 64 triangles;
the maximum 64-segment bolt is 128 triangles;
generation is intended to stream at one vertex per clock;
subdivision is checked before acceptance, so an effect cannot emit half a bolt and run out of budget.

That is not inherently expensive. A strong lightning bolt made from perhaps 12–24 ribbon segments is ordinary bounded geometry.

The architectural mistake would be inventing a free-form command saying:

Draw any number of branching antialiased electrical lines with arbitrary widths.

Zhaozhou’s version should say:

Draw one deterministic ribbon with at most 24 segments and at most two bounded branches.

That is completely compatible with the console.

The catch: FORGE.PRIM is not fully usable yet

The present RTL is real, but it currently owns topology only. It walks the ribbon grid and emits triangle indices in deterministic order. It does not yet take the promised parameter block or calculate the actual vertex positions—the module literally has no position or params inputs. Its own header says that positions are supposed to come from a separate evaluator.

So right now it can prove:

segment 0 uses these indices
segment 1 uses these indices
...

but it cannot yet independently produce:

point 0 = bolt start
point 1 = start + deterministic sideways kink
point 2 = another kink
...
point N = bolt end

That missing evaluator is why the Creature 02 plan says “no FORGE.PRIM” for its first render. The first render is happening in the existing C++ reel, and the bead-chain can be built immediately through the already-working Population triangle path. The plan was choosing the currently executable route, not abolishing lightning from the console.

The particle system is the secondary lightning machinery

The particle representation ladder explicitly contains:

meshlet
→ triangle/shard
→ ribbon/streak
→ soft sprite
→ glint
→ culled

and it makes that choice independently per camera, which is exactly what lightning needs in Duo: a bolt can be a proper ribbon in one player’s view and a streak or glint in the other.

PART.SOFT is specifically described as the soft sprite/streak endpoint. It is also already earmarked for narrow beams and additive star glints. It does not generate a whole jagged bolt; it handles the cheap distant/thin representation and the little sparks around one.

So a full lightning effect naturally uses both systems:

main bolt: FORGE.PRIM ribbon;
tiny branches and corona sparks: PART.*;
very distant bolt: PART.SOFT streak/glint;
impact debris: particle triangle/shard;
afterglow: post pipeline.
Additive blending and bloom are already part of the machine law

RASTER.FRAGMENT already owns additive sun and beam recipes. The beam material law is essentially:

source = texture × vertex colour
destination = saturate(destination + source)
depth test on
depth write off

That is also the correct basic law for an electrical bolt.

Then POST.GATHER collects explicitly tagged emissive fragments during resolve, and POST.COMPOSITE blurs and adds the glow in its bounded post pass. That means the bolt does not need several increasingly wide geometry halos to appear luminous. Draw a narrow bright core, tag it as emissive, and let the shared bloom machinery provide the radiance.

That distinction dramatically changes the cost:

Stupid lightning
wide halo ribbon
+ medium halo ribbon
+ narrow core ribbon
+ branches
+ sparks

Lots of overlapping additive fragments.

Zhaozhou lightning
one narrow additive ribbon
+ perhaps one brighter core
+ glow tag
+ bounded sparks

The post system blooms it with work shared across the frame.

What the proper game-lightning contract should look like

I would add an effects-level contract, not a seventh Forge hardware family:

FX.LIGHTNING
start anchor
end anchor
tick
seed
segment count
jitter amplitude
width
colour
intensity
branch count
lifetime
semantic weight
viewport mask

The position evaluator generates a deterministic polyline using a recurrence:

P0 = start
PN = end

Pi = lerp(start, end, i/N)
   + perpendicular_1 × jitter(seed, tick_phase, i)
   + perpendicular_2 × jitter(seed², tick_phase, i)

Then the existing FORGE.PRIM ribbon topology joins adjacent point pairs.

A sane v1 budget could be:

Rung	Representation
Near	16–24 segment additive ribbon, optional brighter core
Mid	8–12 segment single ribbon
Far	one PART.SOFT streak
Tiny	glint
Branches	maximum two, each ≤8 segments
Sparks	bounded particle population
Lighting world	1–3 broad local-light samples, not one light per segment

Even the extravagant near case can stay around roughly 64–128 triangles, depending on core layering and branches. That is not remotely the same as an unbounded line renderer.

And the bolt’s illumination of terrain and creatures should go through the bounded local spell-light system we already ruled in—not by trying to derive lighting from every glowing pixel. A couple of broad coloured light samples moving along the bolt will sell the connection.

What was actually refused

The repo refused live per-pixel line drawing as an unbounded special mechanism. It did not refuse visible lightning.

It also removed “chain” as its own Forge opcode because the existing ribbon/tube machinery already covers that geometry. That is architectural consolidation, not a feature cut.

So the corrected conclusion is:

Creature 02 uses a bead-chain for its first render because that path works today and may actually look cleaner inside its busy silhouette.

The game absolutely gets proper lightning, built as bounded Forge ribbons with particle sparks, additive blending, post bloom, and a per-view LOD ladder.

We should not accept “the mana creature used beads” slowly mutating into “Zhaozhou cannot do lightning.” It can. The missing work is chiefly the procedural position evaluator and end-to-end Forge integration, not some huge new lightning ASIC organ.
