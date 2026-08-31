This file describes sunder. A stretch goal, but one we really want. Shouldn't cost too much so please consider it.

Can our terrain machine cut terrain? Split an island and the two halves float apart. Or cut off the top of a mountain, the remaining nub is flat. Or maybe diagonal. We could spawn a piece that looks like what the top was and have it fly off with some rudimentary physics
Yes — with one important distinction

Mantle can already represent the cut. It cannot yet turn the severed material into an independently moving terrain body.

That second part needs a new runtime/world-object layer, but it does not require voxel terrain, arbitrary CSG, or a replacement terrain architecture.

The mountain-top example is actually an unusually clean fit for the current dual-heightfield representation.

Cutting the top off a mountain

Each terrain column currently represents exactly one solid interval:

bottom(x,z) … top(x,z)

or it is void. That excludes caves with several stacked solid layers, but it is perfect for shaving terrain with a horizontal or sloped plane.

For a cut plane

cut_y(x,z) = a·x + b·z + c

the remaining mountain becomes:

remaining_top = max(bottom, min(old_top, cut_y))

So:

a = 0, b = 0 gives a perfectly flat nub.
Nonzero a or b gives a diagonally sloped nub.
The result is still one top height for every (x,z), so Mantle likes it perfectly.

The severed cap is equally neat:

chunk_top    = old_top
chunk_bottom = cut_y
chunk_solid  = old_top > cut_y

That chunk is itself a valid dual-heightfield object: the original mountain surface on top, the cut plane underneath, void outside the intersection. It can have proper side walls around its boundary and becomes a closed chunk of rock.

This is far cleaner than generic mesh slicing.

The ideal implementation

At the instant of the cut:

Snapshot the affected terrain’s composed top.
Build the detached chunk from old_top down to the cut plane.
Bake the remaining terrain down to that same plane.
Commit both changes atomically.
Give the new chunk position, velocity, spin and a simple collision body.
Let it fly away, crash, shatter, or fall into the void.

The player should never see both the old mountain and the detached cap simultaneously. The terrain mutation and chunk spawn belong to one deterministic event/resource epoch.

Small or temporary cap

Compile it into transient ordinary meshlets:

old terrain top
+ planar underside
+ generated boundary walls
→ transient meshlets
→ ordinary rigid transform

Then the FPGA treats it like an object. Physics remains on the HPS, which already owns gameplay physics and high-level world state.

This is probably the best first implementation. It is bounded, visually spectacular, and does not require moving-terrain collision.

Large or walkable cap

Reuse and generalise the existing body_patch seam.

The creature specification already describes a terrain patch attached to a rigid Loom node:

world queries are transformed into node-local space;
the normal and velocity are transformed back out;
rendering tessellates the terrain under the node’s transform;
only rigid transformation plus uniform scale is allowed.

That was written for a terrain-bodied giant, but the abstraction is really:

A terrain patch belonging to a moving rigid body.

A severed mountain top is exactly that.

The seam is currently prototype-before-silicon, so it is not working hardware yet. But it means the required transform/query architecture has already been anticipated.

Cutting all the way through an island

That also fits the terrain representation.

To cut a trench completely through the island, lower a band of terrain vertices until they reach the bottom surface. The existing breach law then converts a cell to VOID_BREACHED when all four corners have reached the bottom.

You get:

solid half
──────────┐
          │ generated cut wall
          │
          └── open void

open void

          ┌── generated cut wall
          │
──────────┘
solid half

The rim system already defines walls on every solid-to-void boundary, including runtime breaches. The present FORGE.CLIFF RTL only plans which rim edges receive walls; the actual wall-vertex emission stage is still missing.

So a stationary island split with a visible gap is directly in line with the existing terrain law.

Making the two halves float apart

That is where it becomes a real feature rather than merely another bake.

After the cut, the HPS needs to run connected-component detection over the solid-cell graph:

breach committed
      ↓
find disconnected terrain components
      ↓
large component A → terrain body A
large component B → terrain body B
small components   → debris / chunks

Then each component receives its own rigid transform and movement state.

I would not literally rewrite every patch as a completely new island. A better extension is:

TerrainBody
    body_id
    rigid transform
    linear/angular velocity
    set of patch-page references
    attached entities

The ordinary world is TerrainBody 0 with an identity transform. An island split creates two bodies referencing the existing patch pages.

Only pages crossed by the cut need to be duplicated or reconstructed, because one boundary page may contain cells belonging to both resulting components. Whole untouched patches can simply move from one component’s page set to the other.

Translation first

If the halves merely move apart without rotating, this is much easier:

update two world origins;
transform neither normals nor local terrain geometry;
units inherit the platform translation;
local nav grids remain valid;
scars and materials remain unchanged.

That would already look extraordinary.

Rotation later

If halves may pitch, roll and tumble, use the body_patch/rigid-Loom transform seam:

inverse-transform collision queries;
transform terrain vertices before projection;
rotate surface normals;
add body velocity to terrain velocity;
parent standing units to the terrain body.

That is substantially more integration work, but still bounded. It is not general deformable rigid-body simulation.

What is genuinely difficult

The cut arithmetic is not the hard part.

The hard part is deciding what everything attached to the terrain does when the world breaks:

Which half owns each unit?
Do units standing on a moving half inherit its velocity?
Are active Earth fields world-space or island-body-local?
Are live fields baked before separation?
How are projectiles and navigation re-binned?
Can a moving half collide with another island?
What happens when a huge chunk leaves the resident streaming region?
Does it remain walkable, or become ordinary debris?

Those are mostly HPS gameplay/world-management semantics, not FPGA capacity problems.

A deliberately sane first law would be:

At separation, bake all affected live terrain fields, classify entities by their support column, attach them to the resulting terrain body, and allow terrain bodies to translate with limited rotation. Tiny disconnected components become debris instead.

Diagonal has two meanings
Sloped cut surface

A diagonal surface such as:

y = 0.3x − 0.1z + c

is completely compatible. It remains one height per (x,z).

Diagonal boundary seen from above

A diagonal line cutting across the island’s map will currently be represented through terrain cells. At the canonical two-metre pitch, the raw breach edge becomes a two-metre staircase.

At 240p that may look like a rough fractured cut, which could be desirable. For a magically precise sword-plane cut, the visible boundary should instead use a generated cut-face mesh that intersects the exact plane, while the underlying cell mask remains the conservative gameplay representation.

The current cliff law is axis-aligned per terrain-cell edge, so exact oblique cut faces would be a small new Forge family rather than something FORGE.CLIFF already produces.

LOD consequence

A cut is not entirely free in terrain rendering.

The tessellator currently rejects coarse subpatch representations when they contain void cells, because coarse terrain cannot safely represent an internal hole. That means subpatches along a severing cut remain at fine resolution.

That is sensible:

the two giant unbroken halves can remain coarse;
only the narrow fracture boundary stays detailed;
the dramatic part receives the geometry.

For a long cut across an entire island, the seam may consume a noticeable terrain budget, but nowhere near the cost of keeping the whole island at maximum detail.

Current implementation status
Feature	Representation	Current implementation
Flatten mountain top	Excellent fit	No plane-cut bake yet
Sloped/diagonal shave	Excellent fit	No plane-cut bake yet
Through-cut into void	Existing breach law	Bake exists, memory integration incomplete
New side walls	Existing rim law	Planner built; wall emitter missing
Detached mesh chunk	Straightforward hybrid	Not built
Moving walkable terrain chunk	Existing body_patch concept	Spec/prototype seam only
Split island, halves stationary	Good fit	Not end-to-end integrated
Split island, halves translate	Good bounded extension	Needs terrain-body runtime
Arbitrary rotating halves carrying units	Feasible but substantial	Not built
General arbitrary Boolean terrain	Poor fit	Correctly outside Mantle v1

The current bake RTL is specifically a radial paraboloid dig stamp, so a plane cut is not secretly already available as a different flag. It needs either:

a cheap exact clip-top-to-plane bake path; or
HPS-side dynamic page construction followed by an upload.

The second is probably the fastest route to the first spectacular demonstration.

What I would build first

The flying mountain cap.

It gives you almost everything aesthetically:

an impossibly clean magical cut;
a flat or angled exposed surface;
the original top preserved perfectly;
a huge chunk moving through the air;
dust, fragments and lighting;
a permanent changed mountain afterward.

And it avoids the most complicated initial questions about repartitioning an entire island, moving navigation regions, and carrying armies on two independently rotating continents.

Then generalise the same mechanism:

shave
→ detach one bounded chunk
→ detach several chunks
→ sever a stationary island
→ promote disconnected components to translating terrain bodies
→ allow limited rotation and rideable surfaces

So the answer is yes, strongly. Not “the current RTL already has a CUT_ISLAND opcode,” but “the chosen terrain representation supports these effects unusually well, and the missing abstraction is a movable terrain body rather than a new terrain engine.”

The mountain-top chunk may actually be easier and cleaner than arbitrary ordinary mesh destruction because the dual-heightfield gives you both halves of the cut almost for free.

Can we architect this into hardware as capable as we can? As a stretch goal if we still have silicon left. We'll think about the actual software implemntation later once we know what we can and can't do.
Yes. The right stretch architecture is not “a terrain-cut opcode.”

It is a bounded, transactional terrain fracture engine plus moving terrain bodies.

I would call the optional subsystem Sunder, with these formal blocks:

TERRAIN.FRACTURE
TERRAIN.CUTTXN
TERRAIN.BODY
FORGE.CUTFACE
TERRAIN.COMPONENT      optional
TERRAIN.BODYQUERY      optional

The important conclusion is:

Flat cuts, sloped cuts, arbitrary planes, detachable caps, and two moving island halves are all compatible with Mantle.

They do not require voxel terrain or a replacement terrain engine. The existing terrain representation is one solid vertical interval [bottom, top] or void per column, and there is already a conceptual seam for binding terrain patches to rigid moving transforms through body_patch.

What we must add is a way to:

split that solid exactly;
generate newly exposed faces;
publish new terrain pages atomically;
attach the result to an independently moving body;
continue feeding it through ordinary Mantle, geometry and raster hardware.
What the hardware could support

With the full stretch tier, I would target:

Operation	Support
Cut mountain at a constant height	Yes
Sloped/diagonal mountain cut	Yes
Arbitrarily oriented 3D plane	Yes
Cut a slab between two planes	Yes
Extract a wedge or bounded chunk	Yes, bounded plane set
Split island into two halves	Yes
Make both halves translate/rotate independently	Yes
Keep a detached piece walkable	Yes
Continue deforming detached terrain	Yes, at the full terrain-body tier
Generate matching exposed rock faces	Yes
Calculate approximate volume, centre of mass and inertia	Optional hardware accumulator
Automatically detect disconnected components	Optional highest tier
Curved/wavy cut surface	Yes through an uploaded cut lattice or later Field output
Arbitrary caves and unrestricted Boolean CSG	No; mesh/Wound fallback
Unlimited fragments and rigid-body collision	No

That is an extremely capable terrain-destruction machine without becoming a general-purpose volumetric processor.

Why planar cutting fits the terrain law so well

Suppose a mountain column currently contains:

bottom(x,z) ≤ y ≤ top(x,z)

A cut surface gives a height cut(x,z).

The lower part is:

bottom ≤ y ≤ clamp(cut, bottom, top)

The upper part is:

clamp(cut, bottom, top) ≤ y ≤ top

Each result is still one interval per column. That means both pieces remain legal dual-heightfield terrain.

A horizontal plane gives a flat nub. A sloped plane gives a diagonal nub. A second cut surface allows three bands:

lower piece
middle slab
upper piece

So the fundamental fracture operation should support either one or two cut surfaces, not just “dig down.”

The present bake engine is specifically a radial paraboloid scar/breach engine. It is not a general plane slicer and should not be twisted into one.

One complication: exact cut boundaries

Clamping the 33×33 height samples alone would produce a decent lattice-resolution cut, but not always an exact plane.

A cut may cross through the interior of a terrain triangle. Simply clamping its three vertices gives a slightly different triangle instead of splitting it on the true intersection line.

Top-down diagonal and vertical cuts are even more obvious: the current cell-state map says that an entire 2 m cell is solid or void. It cannot represent half of one cell.

The capable solution is a hybrid terrain/body representation:

ordinary cells
    remain ordinary Mantle terrain

cells intersected by the cut boundary
    become MESH_OWNED

FORGE.CUTFACE
    emits exact replacement geometry for those boundary cells

So the overwhelming majority of a mountain or island remains cheap heightfield terrain. Only the one-cell-wide fracture contour becomes generated mesh.

The current tessellator already excludes void cells and uses a fixed diagonal for top and underside geometry; current cliff hardware only plans axis-aligned solid-to-void walls, and its actual wall-emission stage has not yet been written. Exact oblique cut faces should therefore be their own generator rather than an increasingly tortured FORGE.CLIFF.

The exact boundary-cell generator

Each terrain cell is already divided along a fixed diagonal. Between its top and bottom surfaces, each half-cell is a triangular prism.

FORGE.CUTFACE can process those prisms with a small fixed-case clipper:

terrain cell
    → two triangular prisms
    → bounded tetrahedral decomposition
    → evaluate cut plane at vertices
    → fixed marching-tetrahedra case table
    → emit retained geometry for side A
    → emit retained geometry for side B
    → emit identical cut face with opposite winding

This has several advantages:

arbitrary plane orientation;
exact intersection against the actual terrain triangles;
matching geometry on both severed pieces;
no cracks between pieces;
completely bounded maximum output per cell;
no dynamic polygon library or general CSG machinery;
perfect agreement with the fixed terrain diagonal.

Shared terrain edges must calculate the intersection once under an exact fixed-point division law and use an ownership rule, so adjacent cells generate the same intersection vertex bit-for-bit.

The emitted triangles should become ordinary RAW meshlets in local SDRAM. They then use the normal MESHFETCH → VDECODE → transform → PROJECT → PARAMBUF → raster path. They should not receive a private renderer.

TERRAIN.BODY: the key reusable abstraction

The existing body_patch proposal is effectively already the beginning of this architecture: transform the query into patch-local space, perform ordinary terrain evaluation, and transform the result back out. It was written for a terrain-bodied giant, but a severed island chunk is the same abstraction.

I would generalise it into a body table:

TerrainBody {
    body_id
    generation
    page_directory_handle

    local_datum

    current_forward_transform
    previous_forward_transform
    inverse_transform

    bounds
    cut_mesh_handle
    cut_material
    representation_tier
    flags
}

Body 0 is the static world with an identity transform.

A provisional body capacity could be parameterised:

16 bodies baseline
32 bodies stretch

The count must be fitted rather than frozen, but the descriptor storage itself is tiny.

Render path
body-local terrain page
    → TERRAIN.LOD
    → TERRAIN.TESS
    → TERRAIN.NORMALS
    → TERRAIN.BODYXFORM
    → shared GEOM.WCACHE / shared PROJECT
    → GEOM.PARAMBUF
    → renderer

The transform stage applies rigid rotation, translation and optional uniform scale. Static world terrain takes the identity bypass.

Crucially, do not add another terrain projector. The current terrain and ordinary-geometry projectors already contain expensive equivalent projection machinery, and the architecture has identified shared projection as a major silicon lever. A moving terrain body should enter the shared geometry projector after its local-to-world transformation.

This also means:

a cap may rotate;
an island half may drift;
its scars and materials move with it;
both cameras project it independently;
distant moving chunks can use ordinary representation degradation.
Walkable and deformable pieces

A large output can remain a PATCH_BODY:

terrain pages
+ exact fracture-border mesh
+ rigid body transform

It remains:

walkable;
surface-stampable;
potentially deformable in body-local coordinates;
terrain-LOD controlled;
independently streamable.

A small output can instead become a RIGID_MESH:

generated meshlets
+ rigid transform

That is cheaper and appropriate for chunks the player will never walk on.

The fracture transaction can choose based on declared size and complexity:

large piece     → PATCH_BODY
medium piece    → RIGID_MESH
small piece     → debris population
tiny piece      → particle/glint

That fits The Measure’s existing philosophy much better than insisting every pebble remain terrain.

TERRAIN.FRACTURE: the page splitter

The fracture engine should be a background local-SDRAM stream processor.

For every source patch, it:

reads top, bottom, cell state and relevant material state;
evaluates the cut surface or plane;
classifies cells as:
unchanged side A;
unchanged side B;
clipped but still heightfield-representable;
exact boundary mesh required;
writes new copy-on-write pages only where necessary;
emits boundary-cell jobs to FORGE.CUTFACE;
accumulates bounds and optional physical statistics.

For a vertical split, most pages belong wholly to one side. Those pages need not be copied at all: the new body directories simply adopt their existing page handles.

Only pages through which the fracture passes require reconstruction.

For a mountain cap, both the mountain and cap occupy much of the same (x,z) footprint, so more pages need duplication. That is still local-SDRAM bandwidth, not FPGA fabric storage.

The terrain page is currently 21,376 bytes and already designed to stream as whole sequential pages. That makes this kind of background reconstruction substantially friendlier than random per-voxel editing.

A generic cut-surface seam

Do not bind Sunder only to planes.

Give TERRAIN.FRACTURE a generic provider interface:

cut value at lattice vertex
optional cell mask
cut source id

Three providers can feed it:

1. Internal plane stepper

For ordinary magical sword cuts:

A·x + B·y + C·z + D = 0

The page-local plane base and increments are prepared once. Most lattice traversal then uses additions rather than full multiplication at every sample.

2. Uploaded cut lattice

A 33×33 fixed-point cut-height lattice lets later software or tools describe:

curved cuts;
wavy cuts;
scooped caps;
deliberately jagged fracture surfaces;
authored cut patterns.

The hardware does not care how the lattice was generated.

3. Field-produced cut stream

Later, a bounded Field program may generate the same lattice. This is an optional producer, not a dependency.

That is important. We should exploit the Field machinery without making fracture silicon hostage to the Field scheduler again.

The limitation remains that a cut-surface output must describe a bounded interval-compatible shape. A general 3D SDF that produces several separated vertical intervals still falls outside Mantle and becomes generated mesh/Wound geometry.

Runtime-generated terrain page v2

I would leave authored Island Patch v1 untouched and create an additive runtime-generated page v2.

The current cell-state byte has five reserved bits, but v1 requires them to be zero. A v2 page can assign them deliberately:

bits 1:0   substance
bit  2     no_bake
bit  3     top_is_cut_surface
bit  4     bottom_is_cut_surface
bit  5     mesh_owned_cell
bits 7:6   surface class / reserved

This solves three problems:

the flat mountain nub can use exposed-rock material instead of grass;
the detached cap’s bottom can use a fresh-cut material instead of generic island underside;
boundary cells can suppress ordinary heightfield tessellation because exact meshlets replace them.

The body descriptor supplies its cut material or cut material set. The same primary TMU samples it; no second sampler is needed.

Newly exposed surfaces may also receive a deterministic surface-sheet tag such as:

fresh rock
glowing magical cut
crystal interior
burning fracture

The page format and semantic command are Class C decisions, so they need an explicit versioned ruling. We should reserve the seam now without casually assigning an opcode or changing v1 bytes in place.

TERRAIN.CUTTXN: atomic publication

A cut must never appear half-built.

The transaction controller should perform:

validate source generation
    ↓
claim destination scratch regions
    ↓
build output pages
    ↓
build cut meshlets
    ↓
calculate CRCs / bounds / quotas
    ↓
validate all outputs
    ↓
publish new body directories at frame boundary
    ↓
release old references when no reader owns them

Until commit, the game continues displaying the intact source terrain.

On any failure:

discard scratch
leave original terrain untouched
record job/source id and reason

This is especially useful because the hardware job may take multiple frames. The 60 Hz renderer never waits for the fracture engine.

An implementation may hide that time behind the cut spell’s anticipation, flash, dust and shockwave, but the hardware contract itself only promises atomic publication—not a particular cinematic delay.

Memory client and arbitration

The current client enumeration has two reserved IDs after DEBUG, while the owner ruling gives ENGINE1 to the render-critical external GEOM.PARAMBUF. I would reserve one of the currently unused clients—provisionally client 6—as TERRAIN_BUILD, rather than making fracture traffic masquerade as parameter-buffer traffic.

Its arbiter law should be:

scanout and active rendering always win
refresh remains guaranteed
fracture runs only from bounded leftover credits
fracture can be paused at burst boundaries

Memory ownership transitions would be explicit:

scratch page:
    TERRAIN.FRACTURE owns all writes

after commit:
    immutable geometry/material layers become read-only
    TERRAIN.BAKE owns scar/cell mutation
    SURFACE.STAMP owns surface sheet

On-chip storage should be limited to:

two or three terrain rows;
cut descriptors;
one boundary-cell polyhedron scratchpad;
mesh-output FIFO;
body table;
transaction state.

The pages, labels, cut meshes and scratch results all live in local SDRAM.

Optional hardware component detection

A single plane naturally produces side A and side B, but either side may contain several disconnected pieces.

The high-tier TERRAIN.COMPONENT block could identify them automatically with a streaming run-length connected-component algorithm:

scan solid-cell rows
    → form horizontal runs
    → compare with prior-row runs
    → union component labels
    → merge across patch borders
    → accumulate area/bounds/page ownership

Union tables and run records belong in SDRAM; only a small row window and union cache stay on chip.

Outputs could be bounded to something like:

up to 16 material components per fracture job

Anything smaller than a configured threshold becomes debris rather than a walkable terrain body.

This is attractive, but it is also the first hardware feature I would cut if silicon or validation time is tight. A single clean island split does not require it: the cut descriptor already knows side A and side B.

Optional mass and inertia accumulator

While the boundary clipper already decomposes affected volume into bounded tetrahedra, it can feed a single time-multiplexed accumulator for:

volume;
centre of mass;
AABB;
first moments;
approximate principal inertia;
exposed cut area.

This is useful later for rudimentary physics without requiring software to rediscover the exact generated shape.

Because fracture is event-rate work, the accumulator can reuse one small multiplier/MAC lane. It does not need a giant parallel physics unit.

I would expose the results but leave actual body simulation on the HPS. The charter puts gameplay physics there, and an FPGA rigid-body collision solver would be a poor use of precious fabric.

A tiny non-authoritative ballistic integrator could exist for hardware demos:

position += velocity
velocity += gravity
rotation += angular velocity

but it should be a lab option, not the gameplay truth.

Optional moving-body query hardware

The highest stretch tier could add TERRAIN.BODYQUERY for FPGA-side particles:

broadphase a particle against a bounded table of moving-body AABBs;
inverse-transform the particle into body-local space;
run the ordinary patch/column lookup;
transform normal and surface velocity back to world space.

That would let sparks, rocks and dust bounce off a moving island half.

Units, navigation and canonical gameplay collision should still remain software-owned.

Representation ladder for fractured terrain

A moving chunk should not remain full terrain at all distances.

I would give each terrain body:

full moving patch body
    ↓
coarse terrain shell
    ↓
generated rigid mesh
    ↓
micro-mesh
    ↓
debris/splat/glint
    ↓
culled

A walkable island half near the player remains full terrain.

A mountain cap flying into the distance quickly becomes a rigid proxy. The fracture transaction may generate the first coarse shell while it already has all relevant pages in flight.

This keeps spectacular destruction compatible with the console’s central projected-importance law.

Capability tiers

This should be built as independently removable tiers.

Tier	Hardware	Result
0 — reserved seam	body ID/generation, page-v2 flags, memory region, generated-mesh seam	HPS can construct cuts later; FPGA can render moving terrain
1 — Terrain Body	body table, local-to-world transform, identity bypass	Whole islands or pages can move rigidly
2 — Planar Fracture	page splitter, arbitrary plane, atomic transaction	Flat/sloped mountain cuts and clean island splits
3 — Exact Cut Faces	boundary-prism clipper, generated meshlets/material	Exact vertical and diagonal cuts, matching exposed faces
4 — Surface/Slab Cuts	uploaded cut lattice, two cut surfaces	Curved caps, slabs, wedges, authored fracture surfaces
5 — Automatic Fracture	component labelling, body creation, mass metrics	One cut may spawn several independently moving bodies
6 — Full spectacle	moving-body particle query, bounded re-cut/plane stack	Debris collides with pieces; selected pieces may be cut again

The clean cut order is:

component detection;
moving-body particle collision;
bounded repeated cutting;
curved/uploaded cut surfaces;
hardware mass/inertia;
exact cut accelerator;
never cut the Terrain Body seam.

Even with almost no spare silicon, Tier 0–1 would let later HPS software generate a flying mountain cap and have FPGA Mantle render it correctly.

Planning-level silicon expectation

These are not synthesis numbers, but the architectural shape is favourable:

Terrain Body seam

Mostly:

descriptor RAM;
body tags;
transform selection;
identity bypass;
reuse of existing/shared geometry transform and projection.

Expected class: small.

Planar page splitter

Mostly:

sequential reads/writes;
comparisons;
clamps;
counters;
line buffers;
one setup/plane arithmetic lane.

Expected class: small to moderate, few or no permanently dedicated DSPs.

Exact cut-cell clipper

Mostly:

fixed topology table;
one iterative intersection divider;
small vertex scratch;
sequential meshlet writer.

Expected class: moderate.

Component labeller

Mostly:

control;
row-run comparison;
union cache;
external-memory traffic.

Expected class: moderate control/RAM complexity, no important DSP demand.

This is not another renderer, TMU or Field chip. Its output rate is event-driven, so it can spend hundreds of thousands or millions of cycles completing a spectacular cut without threatening frame timing.

But we cannot call it affordable yet. The only current composed fit uses 30% ALMs on an incomplete, test-capacity cone and reaches just 53.48 MHz rather than 100 MHz. That result cannot certify one additional stretch feature.

What should be reserved before implementation

Even while actual Sunder RTL waits for timing closure, I would reserve six architectural seams now:

body_id and body generation in terrain patch jobs.
Runtime terrain page v2, including cut-surface and mesh-owned flags.
A generated RAW meshlet pool for cut faces and rigid chunks.
A reserved local-SDRAM client and guard region for transactional terrain construction.
An atomic terrain-directory publication mechanism.
An optional cut-surface stream independent of whether plane, HPS or Field produces it.

Those reservations are small. Retrofitting them after every terrain packet and cache format has frozen would be painful.

Verification obligations

The flagship tests need to be structural, not screenshot-based.

For a single plane split:

volume(A) + volume(B) = volume(source)
interiors(A) ∩ interiors(B) = empty
cut-face vertices are identical
cut-face windings are opposite
every output cell has top ≥ bottom or is void

Other mandatory cases:

plane exactly through a lattice vertex;
plane exactly along a terrain edge;
coplanar top or bottom;
horizontal, sloped, vertical and near-vertical planes;
cut crossing patch borders;
every supported pitch;
split with authored and breached voids;
cut through an existing scar;
transaction reset or overflow halfway through;
stale source generation;
both cameras viewing opposite sides;
identity terrain body producing the pre-body exact frame CRC;
moving body streamed out and back in;
body representation collapsing to a rigid proxy.

The final spectacle capture should be:

A mountain is cut diagonally. Its top becomes a separately lit, fully textured terrain body, rises and spins away. The nub has the exact matching cut plane. Debris pours from the seam. Elsewhere an island is severed, both halves drift apart with units still standing on them, and Duo views see different halves without a repeated or partial frame.

My recommendation

Architect Sunder now. Implement it only after the 53 MHz repair and the first complete base-machine fit.

The architecture should promise:

one exact arbitrary-plane split;
up to two terrain-body outputs as the core guarantee;
exact generated cut faces;
rigidly moving, still-walkable terrain pages;
optional curved/slab cuts;
optional automatic component separation;
all work transactional and background;
no private renderer or projector;
no dependence on spare M10K for world-sized geometry;
a complete fallback where HPS generates the same pages if the accelerator is cut.

That gives us a superb stretch feature with a graceful silicon failure mode:

With spare fabric, the FPGA itself cleaves mountains and manufactures moving terrain bodies. Without it, the exact same body/page/render architecture remains usable, and software performs only the fracture construction.

So yes: we can architect this very aggressively without betting the console on it. And unlike general cloth or voxel terrain, this is genuinely aligned with the machine Zhaozhou already is.
