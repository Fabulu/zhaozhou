# Owner rulings — the complete answer set, 2026-08-31

**Fabian's answers to every question in `reports/OWNER-QUESTIONS.html`.**
Recorded verbatim below the line. This file is the authority; anything that
disagrees with it is wrong, including earlier notes in this directory.

Baseline the owner consulted: `zhaozhou` main at `600fccd`.

## What this closes, at a glance

Twenty-eight open questions, including every one that the
`reports/CONSOLE_REMAINING.md` audit found blocking. **The ten behaviour blocks
are no longer blocked**, the seven stub contracts have a written authority to
proceed, and the depth ABI — the renderer's last specified gap — is decided.

Two decisions are new and were not in any earlier ruling:

* **Depth profile selection uses `SetView.flags[1:0]`.**
* **The serious-match guarantee is 256 active creatures total.**

## The four terms, and they are not decoration

| term | what it binds |
|---|---|
| **FROZEN CONSOLE LAW** | permanent console/ABI/semantic direction; implementation follows it |
| **FROZEN GAME SEMANTIC** | the rule is decided; numerical tuning may still move |
| **PROVISIONAL TUNING** | build and test with these; they are DATA, not ABI, and may be balanced without reopening architecture |
| **DEFERRED** | not required for base v1; keep the seam only where cheap |

**The distinction that matters most to the console:** every number in section 7
is PROVISIONAL TUNING and belongs in game data. None of it may be baked into the
command ABI or into RTL constants. A tuning value that reaches silicon stops
being tuning.

## The build order is explicitly unchanged

The ruling says so itself, and it is the last line of the document:

> close timing and the conventional renderer; build the external parameter
> buffer and real geometry path; prove the combined fit; then build the larger
> terrain/world and spectacle systems.

So this answer set does **not** redirect the current work. The 100 MHz surgery
continues, measurement-led, one fitted change at a time — and section 8 says
that in as many words: *"Do not alter this ruling set because a predicted timing
offender has a persuasive name; TimeQuest paths decide the next timing
intervention."*

---

# THE RULING, VERBATIM

```
ZHAOZHOU / UPHEAVAL
COMPLETE ANSWERS TO reports/OWNER-QUESTIONS.html
Owner-approved ruling set — 2026-08-31

Repository baseline consulted:
  Fabulu/zhaozhou main at 600fccd1867b4e0076fdc74f45d579963588a361

This document answers every question on reports/OWNER-QUESTIONS.html.
It incorporates reports/OWNER-RULINGS-20260831.md and adds the two decisions
Fabian explicitly approved:

  1. Depth profile selection uses two bits in SetView.flags.
  2. The serious-match content guarantee is 256 active creatures total.

Terminology used below:

  FROZEN CONSOLE LAW
      Permanent console/ABI/semantic direction. Implementation must follow it.

  FROZEN GAME SEMANTIC
      The rule of the game is decided, although numerical tuning may still move.

  PROVISIONAL TUNING
      Use these values to build and test the game. They are data, not hardware
      ABI, and may be balanced later without reopening the console architecture.

  DEFERRED
      Not required for the base v1 console. Keep the seam only where cheap.


===============================================================================
1. DEPTH PROFILE SELECTION
===============================================================================

STATUS: FROZEN CONSOLE LAW

Use bits [1:0] of SetView.flags as depth_profile.

  00  WORLD_LONG       1.0 m  .. 16,384 m
  01  WORLD_STANDARD   0.5 m  ..  8,192 m
  10  CLOSE            0.25 m ..  2,048 m
  11  RESERVED — reject/refuse until a fourth profile is separately proved

Zero flags therefore retain WORLD_LONG, preserving the meaning of existing
zero-filled captures.

There is no separate view-depth command in v1. The selected profile travels with
SetView, is recorded in captures, and is reproduced in replay.

The profile's scale and shift remain generated from the frozen reciprocal law.
Games do not upload arbitrary near/far values. A future fourth profile requires
new evidence, a new proof, and an explicit ABI ruling; it must not be smuggled in
as custom numbers.

Practical meaning:

  Console:
      Smallest command/decoder change. No new opcode and no duplicated view
      state. The camera's depth mode stays with the camera command.

  Game:
      Every camera may independently select long-world, standard, or close
      depth. Large islands retain the long profile; inspection/cinematic cameras
      may use CLOSE. wmax remains a depth clamp, not a far clipping wall.


===============================================================================
2. PARTICLE BEHAVIOUR BLOCKS
===============================================================================

-------------------------------------------------------------------------------
2.1 PART.STATE — the 128-bit record
-------------------------------------------------------------------------------

STATUS: FROZEN CONSOLE LAW

The particle state record is:

  position      54 bits  (3 x 18)
  velocity      33 bits  (3 x 11)
  age           10 bits
  species        7 bits
  size           6 bits
  spin           6 bits
  flags          4 bits
  variation      8 bits
               --------
                128 bits

Lifetime, update recipe, collision response, curves, material and child-spawn
rules live in the species descriptor rather than being repeated per particle.

Randomness is stateless deterministic hashing from stable identifiers and
variation. Do not use a tiny evolving random seed whose sequence changes when a
particle is culled or processed in a different batch.

State uses dense sequential ping-pong streams in HPS DDR:

  survivors are compacted first;
  children are appended after survivors;
  survivors always outrank new children when capacity is exhausted.

Capacity tiers:

  32,768 active particles required;
  65,536 active particles is a stretch tier only after physical board bandwidth
  proves it safe.

-------------------------------------------------------------------------------
2.2 PART.UPDATE — what ships and in what order
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC, with mechanical details delegated to the builder

The v1 recipe vocabulary is closed to these operations:

  integrate
  gravity
  linear drag
  attraction
  repulsion
  orbit
  vortex
  wind
  shockwave
  spline flow
  colour curve
  size curve

A species receives ordinary integration plus optional gravity/drag and one
bounded primary motion recipe. A Field/FLOW result may supply an additional
bounded acceleration input; this does not turn PART.UPDATE into a programmable
particle processor.

The deterministic update order is:

  1. advance age and evaluate lifetime;
  2. read species constants and external Field/wind input;
  3. evaluate the selected force/motion recipe;
  4. apply drag;
  5. integrate velocity and position;
  6. resolve at most one collision response for the tick;
  7. evaluate size and colour curves;
  8. emit deterministic spawn/death events.

New recipes are additive, versioned extensions. Do not silently reinterpret an
existing recipe id.

-------------------------------------------------------------------------------
2.3 PART.COLLIDE — bounce, slide, stick and die
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC

Collision response is selected explicitly by the species descriptor. Hardware
must not guess from speed, colour, particle size or material.

The v1 response enum is:

  IGNORE
      No collision response.

  DIE
      Remove the particle on first accepted contact.

  STICK
      Snap to the contact surface, zero relative velocity, and remain until the
      normal lifetime or a descriptor-defined stuck lifetime ends.

  SLIDE
      Remove the inward normal component, retain the tangential component, then
      apply descriptor-defined friction.

  BOUNCE
      Reflect the inward normal component using descriptor-defined restitution,
      then apply descriptor-defined tangential damping.

Collision sources in v1:

  simple planes;
  the live deformed terrain heightfield and normal.

One response is resolved per particle per tick. The particle is moved to a safe
contact point; any further contact is handled on a later tick. Do not build an
unbounded contact loop.

When moving terrain bodies arrive, body surface velocity is included in the
relative-velocity calculation. Canonical gameplay physics remains on the HPS.

-------------------------------------------------------------------------------
2.4 PART.SPAWN — who may spawn what
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC

A species descriptor may name child species for these events:

  birth;
  a bounded age marker;
  collision;
  death.

Maximum child count per event in v1: 16.

Large spell casts may seed large populations directly from the HPS; the
16-child limit governs local particle-to-particle bursts and therefore does not
limit a tornado, explosion or god spell to 16 visible particles.

Ordering is exact:

  parent stream order;
  then event order;
  then child index 0..N-1.

Only one spawn generation is processed in a tick. A child may itself spawn on a
later tick, but never recursively in the same tick.

On capacity exhaustion:

  existing survivors are retained;
  accepted earlier parents retain priority;
  later child spawns are dropped deterministically;
  counters record requested, emitted and refused children.

-------------------------------------------------------------------------------
2.5 PART.LADDER — representation may change during life
-------------------------------------------------------------------------------

STATUS: FROZEN CONSOLE LAW

Yes: a particle may change representation while it is alive.

The simulation state never changes because of the representation. Selection is
per camera and per frame, so the same particle may be a triangle in one Duo view
and a glint in the other.

The ladder is:

  meshlet
  triangle/shard
  ribbon/streak
  soft sprite
  glint
  culled

Provisional visual thresholds:

  meshlet             projected diameter >= 18 px
  triangle/shard      roughly 6..18 px
  ribbon/streak       projected trail >= 4 px and narrow enough to read as a line
  soft sprite         roughly 2..8 px
  glint               roughly 0.5..2 px
  culled              below 0.5 px unless semantically protected

These are Class-B, evidence-driven defaults, not permanent ABI constants.
Species may provide small threshold biases.

Every transition requires:

  approximately 20% hysteresis;
  a four-frame minimum hold;
  stable screen-space dither/crossfade where two representations overlap;
  no respawn, reseed or simulation discontinuity during a representation change.


===============================================================================
3. TWO-DIMENSIONAL PLANES AND HUD SPRITES
===============================================================================

-------------------------------------------------------------------------------
3.1 TWOD.PLANE
-------------------------------------------------------------------------------

STATUS: FROZEN CONSOLE LAW

Two plane descriptors are a real v1 limit, not a placeholder.

Use one time-multiplexed restricted plane engine. Do not instantiate two full
engines and do not let it become a second unrestricted TMU.

The two slots may be used for combinations such as:

  sky;
  distant landscape;
  water/lava;
  fog or cloud sheet;
  a bounded world-space depth plane.

Required features:

  CLUT8 and RGB565;
  nearest sampling;
  affine transform;
  line scroll;
  repeat and clamp;
  view masks/parameters where required for Duo.

Anything needing ordinary textured geometry should use the main renderer rather
than expanding this plane engine.

-------------------------------------------------------------------------------
3.2 TWOD.SPRITE
-------------------------------------------------------------------------------

STATUS: FROZEN CONSOLE LAW

HUD and overlay elements are ordinary sprite descriptors using the primary TMU.
There is no private HUD sampler and no special text rasterizer.

  text       = glyph sprites;
  windows    = tiled/stretched sprites or simple filled descriptors;
  cursors    = sprites;
  debug heat maps = debug sprites or one of the restricted plane slots.

The hardware should support the existing goal of two player HUD regions and
large descriptor counts, but the game authors layout and text in software.

HUD sprites are drawn after world distortion, bloom, grading, ink and flash so
the interface remains legible.


===============================================================================
4. POST-PROCESSING AND COMPOSITOR ORDER
===============================================================================

STATUS: FROZEN CONSOLE LAW

POST.GATHER collects glow, displacement and mask information during tile
resolve. It must not reread the completed framebuffer merely to rediscover tags.

Effect buffers remain quarter-linear-resolution:

  Z60     96 x 60
  Storm   80 x 60
  Duo    128 x 60

The world compositor order is:

  1. Resolve the 3D world and composite the two restricted world-plane slots.

  2. Build one bounded displacement field from refraction, shockwave and heat
     haze. Contributions combine before sampling and are clamped; do not
     repeatedly resample the framebuffer once per effect.

  3. Sample world colour, glow source and the exterior-ink mask through the same
     displaced coordinates. Refraction therefore bends the world, its glow and
     its outline coherently.

  4. Apply atmospheric haze/fog-sheet composition.

  5. Blur and add bloom/glow.

  6. Apply palette phase/remap, then the generated colour-grading transform.
     These may be fused into one generated lookup implementation as long as the
     result is exact.

  7. Apply the full-screen flash/tint.

  8. Overlay exterior ink last on the world image. The default flash does not
     wash out the line; changing that later is an explicit artistic mode, not an
     accidental consequence of stage order.

  9. Draw HUD sprites, text and debug overlays.

POST.ECHO:

  DEFERRED.
  It is not part of the v1 silicon requirement and remains first on the cut list.
  Keep no expensive storage or datapath for it now. A later PC implementation or
  evidence-backed hardware revival is allowed; the base console does not wait
  for it.


===============================================================================
5. THE THREE CAPACITY / VISUAL QUESTIONS
===============================================================================

-------------------------------------------------------------------------------
5.1 Serious-match battle guarantee
-------------------------------------------------------------------------------

STATUS: FROZEN CONTENT-TIER GUARANTEE

The required serious-match tier is:

  256 active creature instances total in the match;
  nominally 128 per player in Duo;
  nominally 64 per participant in a four-participant match;
  up to four participant wizards;
  ordinary structures, terrain and sky;
  the required 32,768-particle state tier;
  one simultaneous Level-9 spectacle at its declared flagship tier.

This does NOT promise 256 creatures in full hero mesh representation.

The Measure may choose, per view:

  full form;
  reduced form;
  micro-mesh;
  splat;
  glint.

What is guaranteed is that geometry admitted by The Measure is not silently lost
because a fixed arena filled up. The external GEOM.PARAMBUF and tile-reference
storage must be sized from real traces of this content tier.

Four-participant network play does not imply four local cameras. The hardware
still renders the active local view contract, normally one view or Duo's two.

More than 256 creatures remains legal. Multiple concurrent Level-9 spells also
remain legal. They may force more aggressive representation reduction or the
explicit overload response, but never memory corruption or arbitrary omission
of the tail of an army.

Level 10 / Apotheosis is deliberately outside the competitive guarantee.

Unexpected hard overflow law:

  fault the frame;
  drain safely;
  repeat the previous complete frame;
  record source ids and counters;
  never publish a partially missing army.

-------------------------------------------------------------------------------
5.2 Meaning of 276,480
-------------------------------------------------------------------------------

STATUS: FROZEN CONSOLE LAW

276,480 means:

  92,160 Z60 pixels x 3.0 conservative PRE-EARLY-Z overdraw.

It counts covered fragments before Early-Z.

It is not:

  unique pixels;
  texture layers;
  texture samples;
  divide operations;
  a measured complete game frame.

The canonical cross-mode design target is:

  320,000 covered fragments per frame.

Canonical workload profiles should complete in:

  1,333,333 clocks.

The 1,666,667 clocks available at 100 MHz / 60 Hz are the fault boundary, not
the normal design target.

Post-Z survivors, primary TMU requests, AUX requests and material sample counts
are separate trace vectors.

-------------------------------------------------------------------------------
5.3 Cel-material fog ordering
-------------------------------------------------------------------------------

STATUS: FROZEN VISUAL LAW

Cel materials are an explicit exception to the general per-vertex-fog rule.

Cel order:

  interpolate unfogged lighting
  -> apply toon bands
  -> modulate the texture
  -> apply fog
  -> enter the ordinary post chain

This prevents distant fog from being quantised into hard toon bands.

Non-cel materials retain the existing general per-vertex fog path unless a
separate material recipe explicitly says otherwise.

Emissive/additive spell surfaces, sky and HUD remain exempt according to their
own recipes.


===============================================================================
6. AUTHORITY TO WRITE THE SEVEN STUB CONTRACTS
===============================================================================

STATUS: FROZEN PROCESS LAW

Yes: the builder may write the contracts.

The authority split is:

  CLASS A — decide and proceed
      clocks, reset, ready/valid staging, FIFO depths, internal tags, counters,
      traces, formal properties, arbitration and failure handling already
      implied by the charter.

  CLASS B — compare evidence and adopt the winner
      internal compression, normal encoding, cache sizes, unit counts,
      parameter-buffer caches and other semantically equivalent implementation
      choices.

  CLASS C — return to Fabian
      game-visible behaviour, permanent command/cartridge ABI, capture-changing
      numeric law, representation-ladder meaning, content-tier guarantees,
      feature cuts and deterministic ordering.

Specific answers:

-------------------------------------------------------------------------------
6.1 GEOM.MESHFETCH
-------------------------------------------------------------------------------

Write the contract and build it.

Freeze a versioned 64-byte-aligned meshlet descriptor containing, at minimum:

  <= 64 unique vertices;
  <= 126 triangles;
  u8 local indices;
  local bounding sphere;
  material id;
  LOD/error data;
  format id;
  CRC/generation protection.

Cull only when the meshlet lies outside every active camera. Bound centre is
meshlet-local; radius uses maximum absolute instance scale.

-------------------------------------------------------------------------------
6.2 GEOM.VDECODE
-------------------------------------------------------------------------------

Write the contract and land RAW/CANONICAL format 0 first.

Packed rigid format 1 and two-weight skinned format 2 are additive formats chosen
after an asset bake-off on Zixxtrixx plus ten structurally different creatures.

The bake-off must compare:

  silhouette error;
  cel-band flips;
  normal angular error;
  bytes per vertex;
  decoder ALM/DSP/Fmax.

Do not block the geometry path on perfect compression.

-------------------------------------------------------------------------------
6.3 GEOM.WARP
-------------------------------------------------------------------------------

DEFER dedicated v1 hardware.

Current real needs are covered by:

  the bounded fixed creature-deform path;
  Loom transforms;
  HPS/PC preprocessing where necessary.

Keep GEOM.WARP as an optional later accelerator, cut-order 5. Do not allow it to
block conventional geometry or creature completion.

-------------------------------------------------------------------------------
6.4 GEOM.LOOM
-------------------------------------------------------------------------------

Write the contract.

The ARM/compiler supplies a parent-before-child topologically sorted stream.
Loom only composes transforms.

It does not perform:

  recursion;
  cycle detection;
  matrix inversion;
  gameplay event generation;
  autonomous gait logic;
  autonomous formation logic.

Gait and formation values come from Form/Field programs. Keep-world reparenting
is computed on the ARM between frames.

-------------------------------------------------------------------------------
6.5 FORGE.PRIM
-------------------------------------------------------------------------------

Write the contract around one bounded topology generator.

V1 primitive families:

  ribbon;
  radial fan/ring;
  tube;
  radial shell;
  billboard sheet;
  terrain cliff/skirt.

Limits:

  MAX_SEGMENTS = 64
  MAX_SIDES    = 8

Subdivision is selected before acceptance. Never emit a partial primitive.

Not separate v1 primitives:

  shard burst  = particle population;
  chain        = tube/ribbon or repeated meshlet instances;
  spline wall  = ribbon/tube use;
  low cone     = radial fan/shell use.

-------------------------------------------------------------------------------
6.6 INPUT.SNAC
-------------------------------------------------------------------------------

DEFERRED and optional.

The MiSTer input path satisfies the base console contract. Direct PS1/SNAC is
only built after a physical-board use case proves it worthwhile. It must emit
the same canonical PadFrame and may not create a second input semantics.

-------------------------------------------------------------------------------
6.7 MEASURE.HISTOGRAM
-------------------------------------------------------------------------------

The refusal stands.

Close it as Measure-v2/deferred work. Do not invent an error metric, bucket
boundaries, cutoff rule and new governor interface merely to make the ledger
look more complete.

Measure v1 remains:

  ARM predicts thresholds from prior counters;
  FPGA performs local traversal;
  token guard rejects low-priority refinement near the limit.

Revisit the histogram only after real game traces prove v1 inadequate.


===============================================================================
7. MANA TERRITORY — COMPLETE INITIAL RULES
===============================================================================

The following answers are game rules and initial balance data. They do not block
hardware. Numerical values are PROVISIONAL TUNING and must live in game data,
not in the console ABI.

Core principle:

  Mana wells are the source.
  Claimed terrain is the conductor.
  Claimed square metres do not independently print global mana.

This keeps the economy local and manageable.

-------------------------------------------------------------------------------
7.1 Gameplay grid and relation to Mantle
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC

Claim cell size:

  2 m x 2 m — exactly one Mantle terrain cell.

Therefore one 64 m terrain patch contains:

  32 x 32 = 1,024 claim cells.

Connectivity is four-neighbour, not diagonal. A one-cell-wide real cut can
therefore sever a route; influence may not sneak through a diagonal corner.

The terrain surface sheet is 64 x 64 texels, so one claim cell maps naturally to
a 2 x 2 presentation block. The visual can dither/interpolate without changing
the gameplay grid.

Canonical claim state lives on the HPS gameplay side:

  owner id;
  strength 0..255.

The FPGA sees claim as one more terrain material/presentation input, not as a
new world simulation engine.

Existing claimed surfaces remain attached to a moving terrain body. Freshly
exposed cut faces, reconstructed crater surfaces and newly created terrain begin
neutral.

-------------------------------------------------------------------------------
7.2 Mana trickle and local attenuation
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC; numbers are PROVISIONAL TUNING

Wizard fallback trickle anywhere:

  0.25 mana / second.

Each connected friendly well contributes at most:

  2.00 mana / second.

Only the two strongest connected well contributions are summed.

Maximum well contribution:

  4.00 mana / second.

Maximum total with fallback trickle:

  4.25 mana / second.

Distance is shortest conductive path distance through friendly claim, not
straight-line distance through void or enemy territory.

A cell conducts when:

  owner matches;
  strength >= 128;
  terrain cell is not void/breached.

Attenuation factor:

  d <= 64 m:
      factor = 1.0

  64 m < d < 512 m:
      t = (d - 64) / 448
      factor = 1 - smoothstep(t)
      smoothstep(t) = 3t^2 - 2t^3

  d >= 512 m:
      factor = 0.0

The contribution is additionally multiplied by the weakest claim-strength
fraction on the selected path. A damaged narrow corridor can therefore throttle
mana before it breaks completely.

Consequences:

  a southern well does not power a battle kilometres north;
  a captured forward well makes the local offensive come alive;
  severing a claim corridor cuts mana immediately;
  overlapping wells provide local throughput and redundancy, not unlimited
  global income;
  a well on a detached chunk remains useful to units on that chunk if its local
  claim remains connected.

-------------------------------------------------------------------------------
7.3 Claim spread speed and contention
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC; numbers are PROVISIONAL TUNING

Claim propagation updates at 4 Hz.

An uncontested neutral frontier advances at approximately:

  one full 2 m cell per second
  = 2 m / second.

A simple integer implementation may add 64 strength per 4 Hz tick, reaching full
strength in four ticks.

Enemy territory is not recoloured in one step:

  hostile strength is reduced toward zero;
  only then does the new owner's strength grow.

Equal opposing pressure may stall a border. A stronger connected well network
pushes it. This is pressure propagation, not freehand player painting.

-------------------------------------------------------------------------------
7.4 Destructive-spell claim-reset quotas
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC; table is PROVISIONAL TUNING

Quotas are measured in full-strength claim-cell equivalents.

One 2 m cell equals:

  4 square metres.

Each spell CAST receives one total claim-shock budget. A persistent spell such
as Twin Tornado shares one budget across all of its terrain stamps; it does not
receive the Level-9 minimum again every metre or every frame.

Initial table:

  Spell level     minimum cells     maximum cells     nominal area
  -----------     -------------     -------------     -------------------
       1                 4                16             16..64 m^2
       2                 8                32             32..128 m^2
       3                16                64             64..256 m^2
       4                32               128            128..512 m^2
       5                64               256            256..1,024 m^2
       6               128               512            512..2,048 m^2
       7               256               768          1,024..3,072 m^2
       8               384             1,280          1,536..5,120 m^2
       9               512             2,048          2,048..8,192 m^2
      10             1,024             8,192 default  4,096..32,768 m^2

Level 10 is not competitively bounded. Its table row is a default authoring
envelope, not a promise that SUNDER or another Apotheosis effect stops there.

Algorithm:

  1. Persistent terrain changes reset claim on the cells they actually rebuild,
     expose or turn to void.

  2. Count the removed claim as full-strength equivalents, including partial
     strength.

  3. Physical affected area decides the value between the level minimum and
     maximum.

  4. If a hostile network was physically hit but the result is below the
     minimum, strip additional hostile claim strength in deterministic
     breadth-first order outward from the real wound until the minimum is met.

  5. Never apply this minimum top-up to friendly claim. Friendly self-destruction
     loses what was actually destroyed, but the balancing cheat does not punish
     the caster with extra invisible self-damage.

  6. If several hostile owners are hit, split one cast budget among them in
     proportion to physically affected claim. Do not grant the full minimum once
     per victim.

  7. The maximum limits secondary network shock. Fresh rock, new void and
     exposed cut faces cannot remain magically painted merely to obey a number.
     Level 1..9 content should be authored so direct physical resets normally
     stay inside the nominal maximum; an overrun records telemetry and physical
     truth wins.

This preserves both rules:

  terrain decides where the wound is;
  spell level decides how economically serious the wound is allowed to become.

-------------------------------------------------------------------------------
7.5 Transient deformation versus a baked change
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC

Transient live deformation does not reset claim merely because the surface moved.

Examples that do NOT reset claim:

  a travelling ridge that rises and falls;
  a temporary wave;
  terrain breathing;
  a nonpersistent displacement that returns to its prior state.

"Baked" means a deterministic persistent terrain transaction has committed:

  persistent scar/top/bottom/cell state or surface state changes;
  the result survives after the live field ends;
  save/replay observes it;
  the patch generation/state advances at the commit boundary.

Examples that DO reset affected claim:

  a persistent crater;
  a permanent trench;
  a breach to void;
  healed/reconstructed ground;
  newly exposed Sunder faces;
  permanently raised or lowered ground where a new surface is created.

A live field that later bakes resets claim once, at the persistent commit, not
once per frame while it is moving.

-------------------------------------------------------------------------------
7.6 Disconnected-region decay
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC; numbers are PROVISIONAL TUNING

Use pressure, not a region timer.

The instant a region has no conductive path to a friendly well:

  it supplies no mana.

Its visible ownership remains temporarily and each cell loses claim strength at:

  6 strength / second with no hostile pressure
  (a full-strength cell takes about 42.5 seconds to fade).

Hostile connected pressure may accelerate total decay up to:

  18 strength / second.

Reconnection reverses the process through ordinary friendly pressure. No hidden
one-shot countdown is stored on the region.

A detached component containing a friendly well is still a locally connected
network and does not decay merely because it left the original island. A
detached component without a well loses throughput immediately and then fades.

-------------------------------------------------------------------------------
7.7 The one secondary benefit of claimed ground
-------------------------------------------------------------------------------

STATUS: FROZEN GAME SEMANTIC; magnitude is PROVISIONAL TUNING

Claimed ground gives:

  up to 15% shorter summon-emergence time.

The bonus scales with friendly claim strength and reaches 15% at full strength.
Mana cost is unchanged.

Claimed ground does NOT additionally grant:

  damage;
  armour;
  movement speed;
  healing;
  vision;
  longer summon range.

The mana network is already the primary strategic value. The one small summoning
benefit makes friendly ground feel cooperative without turning territorial
advantage into an automatic combat snowball.


===============================================================================
8. ITEMS THAT REQUIRE NO OWNER ANSWER
===============================================================================

Physical-board-blocked blocks:

  No owner action. They wait for the board and measured electrical/bandwidth
  truth.

FIELD.SEQ.EARTH / WARP / FLOW / FORMATION / STAMP:

  These are program profiles on the shared Field engine, not five new datapaths.
  Author them as content/use cases require. They do not need separate engines.

Software entries held behind hardware:

  Keep the existing order. Do not start software merely to make ledger maturity
  look better when its required hardware seam is absent.

Timing work:

  Continue measurement-led surgery one fitted change at a time. Do not alter
  this ruling set because a predicted timing offender has a persuasive name;
  TimeQuest paths decide the next timing intervention.


===============================================================================
9. WHAT IS NOW CLOSED
===============================================================================

The following owner questions are closed by this document:

  depth-profile ABI route;
  particle-state content;
  particle recipe set and deterministic order;
  particle collision semantics;
  deterministic child-spawn rules and overflow priority;
  dynamic mid-life/per-view representation changes;
  particle ladder defaults;
  two-plane v1 limit;
  HUD/text implementation route;
  compositor order;
  POST.ECHO disposition;
  serious-match creature guarantee;
  meaning of 276,480;
  cel fog order;
  authority to write the seven contracts;
  MESHFETCH/VDECODE route;
  WARP disposition;
  Loom scope;
  Forge v1 primitive set;
  SNAC disposition;
  Measure histogram refusal;
  mana trickle;
  attenuation;
  claim spread speed;
  spell-level claim-reset envelopes;
  transient-versus-baked rule;
  disconnected claim decay;
  claimed-ground secondary benefit;
  claim-grid size.

Nothing in this document changes the immediate build order:

  close timing and the conventional renderer;
  build the external parameter buffer and real geometry path;
  prove the combined fit;
  then build the larger terrain/world and spectacle systems.

END
```

---

## Notes for whoever implements this

**Read the status tag before touching a number.** The document deliberately
separates four kinds of statement, and the failure mode it is guarding against
is a PROVISIONAL TUNING value getting baked into RTL or the command ABI, where
it stops being tunable. Section 7's numbers in particular — trickle rates,
attenuation curve, decay rates, the spell quota table — belong in game data.

**Three things are now buildable that were not:**

* `GEOM.MESHFETCH` — "write the contract and build it", with the descriptor
  fields frozen. Two of its three thirds already exist.
* `GEOM.VDECODE` — land RAW/CANONICAL format 0 first, do not block on
  compression.
* `GEOM.LOOM` — write the contract; the ARM supplies a topologically sorted
  parent-before-child stream, so Loom only composes.

**Two are formally closed rather than built:** `GEOM.WARP` deferred to cut-order
5, `INPUT.SNAC` deferred, `MEASURE.HISTOGRAM` refusal upheld. Those three come
off the blocked list by being *decided*, which is the honest way to shrink it.

**The Class A/B/C split is now written down** and supersedes any local
interpretation: Class A is decide-and-proceed, Class B is compare-evidence-and-
adopt, Class C returns to Fabian. Game-visible behaviour, permanent ABI,
capture-changing numeric law, ladder meaning, content guarantees, feature cuts
and deterministic ordering are all Class C.
