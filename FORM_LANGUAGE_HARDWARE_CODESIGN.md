<!-- Provenance: copied 2026-08-14 from zencrifice/FORM_LANGUAGE_HARDWARE_CODESIGN.md (unsuffixed source); the Form language co-design document. -->
# Form: Zhaozhou Language and Hardware Co-Design Charter
## Agent-ready language architecture â€” v0.2

**Working name:** `Form`

**Purpose:** Form is the native game and content language for Zhaozhou. It is not a cosmetic scripting layer over a conventional graphics API. It is the semantic front end of the console: gameplay, terrain transformation, continuous screen-space LOD, procedural geometry, particles, materials, audio cues, split-screen presentation, tests, and performance contracts are expressed in one language and lowered into the ARM runtime, the semantic command stream, offline assets, and bounded FPGA microprograms.

The language exists because Zhaozhou is unusually specialised. A generic C++ API would expose thousands of low-level knobs while hiding the machineâ€™s real strengths. Form makes the strengths first-class and makes expensive or nondeterministic mistakes difficult to express.

---

## 1. The central language law

Form separates **truth** from **form**.

### Truth

Truth is deterministic gameplay state:

- terrain height and terrain velocity;
- persistent scars and material-state changes;
- collision;
- navigation costs;
- creature state;
- projectiles that can damage gameplay objects;
- spell timing and resource use;
- victory conditions;
- random outcomes;
- controller input snapshots.

Truth is never degraded because the renderer is busy. It is evaluated at declared fixed rates and is replayable exactly.

### Form

Form is presentation:

- terrain tessellation;
- creature representation level;
- polygon count;
- procedural spell subdivision;
- particle count and particle representation;
- texture and material detail;
- glow, distortion and other post effects;
- distant microforms and glints;
- which effects are omitted under pressure.

Form may change coherently to honour the 60 Hz contract. The language and type system prevent presentation code from mutating truth.

This is the language equivalent of Zhaozhouâ€™s machine law:

> The frame rate does not negotiate; form negotiates.

---

## 2. Form is one language with several explicit execution domains

Every function belongs to a domain. Domain boundaries are checked by the compiler and remain visible in the intermediate representation.

| Domain | Runs where | Purpose |
|---|---|---|
| `build` | Compiler/offline tools | Generate and process textures, meshes, palettes, LODs, microforms and terrain data |
| `sim` | ARM/HPS | Deterministic gameplay, terrain truth, physics, AI, navigation and spell state |
| `present` | ARM command builder plus FPGA | Pure declarative frame description and quality policy |
| `earth` | Exact C++ reference plus Mantle/Earth hardware profile | Bounded terrain-field evaluation |
| `warp` | Exact C++ reference plus vertex-deformation hardware profile | Bounded deformation of geometry |
| `flow` | Exact C++ reference plus Myriad hardware profile | Bounded particle/field updates |
| `formation` | Exact C++ reference plus Transform Loom profile | Transform generation by index, hierarchy and time |
| `audio` | ARM mixer | Sound graphs, emitters, envelopes and spatial parameters |
| `test` | Desktop, Verilator and hardware harnesses | Deterministic scenarios, properties, captures and budget assertions |

A function cannot silently migrate between domains. The source says where it is intended to run, the compiler verifies the allowed operations, and the build report shows the result.

### No per-game FPGA resynthesis

Form does **not** turn arbitrary game code into Verilog. Shipping games contain:

- native ARM game code;
- semantic frame commands;
- assets;
- small bounded microprograms for existing hardware engines;
- exact metadata and test captures.

The FPGA core remains a stable console. A game is loaded like software, not synthesized like a new machine.

---

## 3. Source-to-machine pipeline

```text
Form source
   |
   v
Typed Form HIR
   |
   v
Multi-domain ZIR
   |------------------------------|----------------------------|
   v                              v                            v
ARM C++17 backend             Semantic ZDL                Build-time asset graph
sim/audio/runtime             present commands             textures/meshlets/LOD
   |                              |                            |
   v                              v                            v
native ARM binary          command templates              .zpak resource pages
                                  |
                    |-------------|-------------|----------------|
                    v             v             v                v
                 Earth code    Warp code     Flow code      Formation code
                    |             |             |                |
                    v             v             v                v
             C++ exact evaluator + bounded loadable FPGA microprograms
```

The first compiler is implemented in TypeScript and emits C++17 for ARM and desktop. This avoids beginning with LLVM or a custom machine-code backend. The typed IR, not the generated C++, is the source of truth.

The desktop build links against ZEmu. The ARM build links against the HPS runtime. Both consume the same packed resources and use the same command ABI.

---

## 4. The language borrows one crucial compiler idea: algorithm and schedule are separate

A spellâ€™s gameplay meaning is not its graphics schedule.

The spell definition states what occurs:

- terrain rises;
- creatures are launched;
- units take damage;
- a scar remains;
- shards are emitted;
- a sound begins.

The presentation definition states how that meaning may appear:

- terrain error target in pixels;
- shard representation ladder;
- procedural surface subdivisions;
- glow priority;
- split-screen fairness;
- degradation order.

This separation allows one gameplay effect to remain exact while Z60, Storm and Duo modes choose different visual schedules.

Illustrative syntax:

```text
spell upheaval(origin: world2) {
    sim {
        terrain.apply rising_ridge(origin, radius: 18m, height: 8m)
        force.launch units within 14m by terrain.velocity * 0.75
        damage units within 8m amount 30
        scar.write crack_ring(origin, radius: 16m)
    }

    present {
        surface upheaval_shell at origin importance critical
        emit rock_shards at origin count 2048
        post shockwave at origin
        sound earth_break at origin
    }
}

presentation rock_shards {
    ladder {
        mesh shard_high while projected_radius >= 2.0px
        mesh shard_low  while projected_radius >= 0.8px
        triangle       while projected_radius >= 0.25px
        streak         while speed >= 3m/s
        glint          otherwise
    }

    degrade count before representation
}
```

The exact syntax can evolve. The semantic separation cannot.

---

## 5. Deterministic simulation model

### Fixed ticks

The main simulation tick is 60 Hz. Systems may declare slower rates:

```text
system locomotion every 1 tick
system targeting  every 4 ticks
system tactics    every 6 ticks
system strategy   every 20 ticks
```

Slow systems may be deterministically staggered by entity ID to avoid workload spikes.

### Explicit state access

Every system declares the state it reads and writes. The compiler constructs a deterministic schedule and rejects ambiguous conflicting writes.

```text
system move_creatures every 1 tick
reads  Input, TerrainQuery, Velocity
writes Position, Velocity, GroundState
```

The first implementation does not need a sophisticated borrow checker. It needs a simple, inspectable rule: one writer per state component in a phase, explicit later phases for dependent work, and stable iteration order.

### Deterministic randomness

Randomness is never a hidden global generator. A random stream is derived from explicit identities:

```text
let rng = random.stream(match_seed, spell_id, cast_sequence)
```

Entity creation, particle seeds and procedural variations remain replayable across desktop, ARM, Verilator and physical hardware.

### Numeric policy

Game truth uses explicit fixed-point and integer types. Floating point is permitted in `build` tools and may later be permitted in explicitly nondeterministic editor-only code, but not in the canonical replay path.

Core types include:

```text
fx16        signed 16.16 fixed point
fx24        larger-range fixed point
angle16     wrapping angle
unit8       normalized 0..1 value
world2      world-space two-vector
world3      world-space three-vector
velocity3   typed world velocity
colour8     packed colour
pixel       projected-pixel unit
pixel_error geometric screen-error unit
tick        integer simulation tick
```

Coordinate spaces should be reflected in types where practical:

```text
point<world>
point<view<0>>
point<clip<0>>
vector<world>
```

This prevents accidental mixing of model, world, view and clip coordinates in the compiler and generated code.

---

## 6. Memory and data model

There is no garbage collector in the shipping frame loop.

Form provides:

- fixed arrays;
- bounded pools;
- frame arenas;
- resource handles;
- immutable asset data;
- optional compile-time generics for capacity;
- explicit persistent state.

Example:

```text
pool creatures: Creature[512]
pool gameplay_projectiles: Projectile[2048]
pool spell_instances: SpellInstance[256]
arena frame_commands: 8MiB
```

The compiler emits data-oriented layouts. Large homogeneous populations default to structure-of-arrays storage so the ARM can update positions, velocities, states and LOD inputs efficiently.

Unbounded recursion and unbounded collection growth are rejected in `sim`, `present`, `earth`, `warp`, `flow` and `formation` domains.

---

## 7. First-class forms and continuous screen-space LOD

A `form` is not merely a mesh path. It is a compiled representation hierarchy.

```text
form carrion_bell {
    source "carrion_bell.glb"

    preserve silhouette
    preserve feature attack_mouth priority high
    preserve feature faction_lantern priority critical

    derive progressive_geometry error_space pixels
    derive rigid_combat_form
    derive microform views 32
    derive glint colour faction_lantern
}
```

The asset compiler produces:

- meshlets;
- progressive refinement clusters;
- per-cluster geometric error;
- silhouette importance;
- material cost;
- rigid and skinned representations;
- micro-meshes;
- splat/microform data;
- far-distance glints;
- morph mappings between adjacent representations.

The Measure receives compiler-generated data rather than trying to infer semantic importance from raw triangles.

### Features can be semantically protected

A creatureâ€™s weapon, banner, glowing eye or attack mouth may be tagged as gameplay-readable. The compiler preserves it longer than inconsequential interior detail.

### Pixels are a language unit

Form may state:

```text
error <= 0.65px
show faction glint below 1.5px height
keep attack silhouette for 8 ticks after targeting
```

This makes the consoleâ€™s defining visual law directly authorable.

---

## 8. Terrain is authored as fields, materials and history

### Terrain truth

A terrain operation produces deterministic channels:

- height delta;
- height velocity;
- material-state delta;
- scar request;
- hazard state;
- navigation-cost delta;
- persistence behaviour.

Illustrative Earth-domain code:

```text
@earth
field rising_ridge(sample: terrain_sample, p: RidgeParams) -> terrain_delta
footprint capsule(p.a, p.b, p.radius)
max_ops 16
{
    let d = distance_to_segment(sample.xz, p.a, p.b)
    let envelope = smoothstep(p.radius, 0m, d)
    let phase = curve(p.age, 0t..45t)

    return terrain_delta {
        height   = envelope * phase * p.height,
        velocity = envelope * derivative(phase) * p.height,
        material = blend_tag(cracked_earth, envelope),
        nav_cost = slope_cost(envelope * p.height)
    }
}
```

The compiler must know or be given:

- conservative footprint;
- maximum operation count;
- parameter layout;
- output bounds;
- whether the result can be baked permanently;
- whether it is valid for hardware execution.

### Surface appearance is also language-driven

Deformation without visible history is insufficient. A terrain material declaration compiles offline into texture pages, palettes, material tables, scar responses and decal templates.

```text
terrain_material grave_soil {
    base mosaic {
        soil   weight 0.50
        ash    weight 0.30
        bone   weight 0.20
        variation noise2(scale: 0.08)
    }

    slope {
        above 0.55 use exposed_rock
    }

    scar burn {
        sheet_tag char
        strength 220
        decay 1800t
        material charred_soil
        emissive ember when age < 120t
    }

    scar crack {
        sheet_tag fracture
        decal crack_ribbon when strength > 180
    }
}
```

The offline compiler is allowed to do expensive procedural texture generation. The runtime hardware still executes a bounded material recipe: typically one primary sample, a compact surface-sheet lookup, palette/material tables, and selected polygon decals.

This gives a programmer-author a route to distinctive terrain without hand-painting every battlefield texture.

---

## 9. One bounded field semantics, several hardware profiles

Earth8, Warp8, Myriad/Flow and formation formulas should not each invent unrelated arithmetic semantics.

The compiler defines one canonical, branchless **Field IR** with exact fixed-point behaviour. Profiles restrict inputs, outputs, instruction count and legal operations.

### Shared semantic operations

Provisional operations include:

```text
mov add sub mul mad min max abs clamp
select compare
dot2 dot3 normalize_approx
sin cos curve noise2
length_approx distance_approx
smoothstep ring ridge
sample_spline sample_curve
rotate2 rotate3
```

There are:

- no loops;
- no recursion;
- no arbitrary pointer reads;
- no texture fetches;
- no dynamic allocation;
- no unbounded control flow.

### Profiles

| Profile | Input record | Output record | Typical use |
|---|---|---|---|
| `earth` | terrain sample, time, parameters | height/material/velocity/nav delta | craters, waves, ridges, healing |
| `warp` | vertex, normal, attributes, time | displaced vertex/normal | breathing, twisting, morphing |
| `flow` | particle state, fields, time | new particle state and render attributes | vortices, sparks, shards |
| `formation` | index, parent transform, time, parameters | transform/material phase | rings, helices, armies, boss parts |
| `stamp` | sheet coordinate, age, parameters | tag/strength operation | scorch, cracks, corruption |

Language semantics are unified. Physical RTL is not forced to be one shared bottleneck. The FPGA may instantiate separate profile engines or share ALU libraries according to synthesis results.

### Exact dual output

For every accepted field program, the compiler emits:

1. serialized microcode;
2. a scalar C++ evaluator from the same typed IR;
3. random-vector generators;
4. source maps from microcode PCs to Form source;
5. a static cost report;
6. declared numerical bounds.

A program is not hardware-valid until the C++ evaluator, Verilated profile engine and physical FPGA agree on the generated vectors.

---

## 10. First-class populations and polygon particles

A population declaration separates particle truth, update law and representation ladder.

```text
population rock_shards capacity 8192 {
    state {
        position: world3
        velocity: velocity3
        age: tick
        spin: angle16
        size: fx16
        seed: u32
    }

    @flow update {
        velocity += gravity
        velocity += field.vortex(position)
        position += velocity
        spin += spin_rate(seed)
        size *= 0.996
        die when age >= lifetime
    }

    present {
        mesh shard_high while projected_radius >= 2.0px
        mesh shard_low  while projected_radius >= 0.8px
        triangle        while projected_radius >= 0.25px
        streak          while speed >= 3m/s
        glint           otherwise
    }
}
```

The simulation state may be shared while each camera chooses a different representation. In split-screen, one player may see a shard mesh and the other a glint.

Gameplay projectiles and cosmetic particles remain distinct types. Cosmetic particle degradation can never remove a damaging gameplay projectile.

---

## 11. Procedural spell geometry

Procedural surfaces are language primitives, not low-level vertex loops:

```text
surface tornado {
    axis spline spell_path
    tube radius tornado_radius(time, height)
    radial_segments by_pixels(min: 3, max: 16)
    longitudinal_segments by_pixels(min: 8, max: 64)
    twist time * 3.2
    material storm_skin
}
```

The compiler lowers this to a Primitive Forge descriptor and a representation schedule. The programmer states the form and quality law; the hardware generates the triangles.

Required procedural forms include:

- ribbon;
- tube;
- radial shell;
- ring;
- chain;
- terrain skirt/cliff;
- shard burst;
- billboard sheet;
- spline wall;
- low-poly lattice.

The language prevents procedural geometry from producing an unbounded number of segments. Every subdivision rule has a declared maximum and a screen-space error policy.

---

## 12. Split-screen is a source-level concept

Form exposes players, cameras and viewports explicitly:

```text
present duo {
    view left  from player[0].camera budget 45%
    view right from player[1].camera budget 45%
    shared emergency budget 10%
}
```

Shared simulation, terrain fields, creature animation and particle state are represented once. Camera-local LOD and projection remain separate.

A render declaration may indicate shared importance:

```text
draw boss importance critical views both
```

The compiler/runtime generates viewport masks, source IDs and Measure weights. It does not duplicate game entities to obtain split-screen.

---

## 13. Pure presentation and reorderable command graphs

`present` blocks are pure. They may read simulation state and emit semantic presentation operations, but they may not mutate simulation state.

This allows the compiler/runtime to:

- sort opaque work;
- merge instance draws;
- cache static command templates;
- assign view masks;
- allocate geometry and fragment tokens;
- collapse populations coherently;
- repeat the previous completed frame safely;
- capture an entire frame deterministically.

Order-dependent effects require explicit constructs such as `layer`, `after`, `transparent_group` or `barrier`. Ordinary source order is not secretly treated as a graphics synchronization primitive.

---

## 14. Performance is part of the type-and-build system

Every hardware-domain program has a known worst-case instruction cost.

Every bounded pool has a known maximum population.

Every procedural surface has a known maximum subdivision count.

Every material has a declared fragment class.

Every representation ladder has known geometry and fragment costs.

The compiler emits a `costs.zcost` report containing:

- maximum simulation pool sizes;
- maximum command memory;
- per-field instruction counts;
- program-cache residency;
- maximum terrain footprint;
- maximum generated terrain samples;
- form hierarchy sizes;
- triangle ceilings by representation;
- material sample classes;
- particle-state bandwidth;
- split-screen budget policy;
- source lines responsible for each cost.

Build modes:

- `prototype`: allows explicit software fallbacks and warnings;
- `hardware`: rejects high-frequency field programs that cannot lower to hardware;
- `release`: additionally requires all budget assertions and golden captures to pass.

Example assertion:

```text
assert_budget Duo {
    total_geometry <= 18000 triangles
    fast_fragments <= 950000
    player_min_share >= 45%
    audio_voices <= 32
}
```

Static estimates do not replace runtime counters. Source IDs connect runtime counter overruns back to Form declarations.

---

## 15. Input and audio

Controllers are deterministic frame snapshots:

```text
let pad0: Pad = input.player(0)
let pad1: Pad = input.player(1)
```

Input is read at the simulation boundary and stored in replay captures.

The `audio` domain defines emitters and mixer parameters while remaining ARM-executed:

```text
sound earth_break {
    sample "earth_break.zadpcm"
    gain envelope attack 0t decay 40t
    pitch random 0.92..1.08 from event_seed
    spatial world
    bus effects
}
```

The language may generate audio events from the same spell declaration, but audio failure or voice stealing never changes gameplay truth.

---

## 16. Tests are part of the language

Form provides scenario and property blocks:

```text
scenario opposing_waves {
    seed 0x5A17
    load map wound_lab
    spawn player 0 at west_altar
    spawn player 1 at east_altar

    at 120t cast upheaval by player 0 toward centre
    at 120t cast upheaval by player 1 toward centre

    assert terrain.height(centre) within 0.01m of expected_height
    assert no creature below terrain
    capture frame 150 as "opposing_waves"
    assert_budget Duo
}
```

The compiler emits:

- desktop test executable entries;
- deterministic input streams;
- expected simulation hashes;
- ZRef frame captures;
- Verilator vectors;
- physical-hardware replay packages.

Field programs automatically receive generated random differential tests over their declared input bounds.

---

## 17. Source-level hardware debugging

Every semantic command, form, field program, material and population receives a stable source ID.

Hardware traces report:

- frame ID;
- source ID;
- command sequence;
- field-program PC;
- tile;
- primitive;
- counter class;
- first divergent fixed-point values.

The capture inspector resolves these into source locations such as:

```text
spells/upheaval.form:48
population rock_shards / presentation ladder / mesh shard_high
```

This is required, not optional polish. Without source mapping, a custom language would make FPGA debugging worse instead of better.

---

## 18. Compiler implementation order

### L0 â€” Semantics before syntax

Write:

- domain/effect rules;
- fixed-point rules;
- deterministic scheduling rules;
- memory rules;
- Field IR semantics;
- command semantics;
- cartridge format;
- source-ID rules.

A small textual or JSON IR may drive initial tests before the full parser exists.

### L1 â€” Minimal Form frontend

Support:

- modules;
- constants;
- structs and enums;
- functions;
- fixed arrays and pools;
- fixed-point types;
- `sim`, `present` and `test` domains;
- controller input;
- camera and simple draw commands;
- C++17 output.

The first Wound Lab software demo is written here.

### L2 â€” Field profiles

Add:

- `earth`;
- `warp`;
- `flow`;
- `formation`;
- exact Field IR evaluator;
- program serializer;
- hardware-validity checker;
- generated random vectors.

### L3 â€” Forms, Measure and populations

Add:

- form declarations;
- representation ladders;
- semantic feature priorities;
- pixel-error types;
- population declarations;
- split-screen budget declarations;
- static cost reporting.

### L4 â€” Terrain materials and build domain

Add:

- terrain material grammar;
- procedural texture baking;
- scar response tables;
- meshlet/LOD/microform asset pipeline;
- cartridge packing.

### L5 â€” Production tooling

Add:

- language server;
- formatter;
- hot reload in ZEmu;
- source-level profiler;
- interactive terrain/spell inspector;
- capture navigation;
- compiler fuzzing.

The language must remain usable after L1. L2â€“L5 expand it without requiring a rewrite of Wound Lab.

---

## 19. Changes required in the console architecture

Adopting Form changes Zhaozhou in concrete ways.

### 19.1 The compiler/IR becomes a fifth co-equal product

The project now builds:

1. ZSpec;
2. Form compiler and typed IR;
3. ZRef/ZEmu;
4. ZRTL;
5. ZSDK/runtime/assets/demo.

The compiler is not postponed until the GPU is finished.

### 19.2 Phase 1 must freeze language-visible semantics

Before major RTL, specify:

- numeric and rounding behaviour;
- domain boundaries;
- deterministic system scheduling;
- Field IR;
- semantic command ABI;
- source IDs;
- program hashes;
- cost metadata.

### 19.3 The command processor remains semantic

Final game-facing commands include concepts such as:

```text
APPLY_TERRAIN_FIELD
STAMP_SURFACE
DRAW_FORM
DRAW_POPULATION
DRAW_PROCEDURAL
SET_PRESENTATION_CONTRACT
EMIT_AUDIO_EVENT
```

Low-level triangle commands remain bootstrap/debug routes.

### 19.4 FPGA program storage and trace support are base features

The RTL needs:

- small program/constant caches;
- profile-specific field sequencers;
- program hash/version checks;
- source ID propagation;
- field-program PC tracing;
- exact instruction counters;
- safe rejection of invalid microcode.

Exact capacities are chosen after Phase 0 synthesis and bandwidth results. The initial planning target is hundreds of small resident programs, not arbitrary shader code.

### 19.5 The Measure consumes compiler metadata

LOD hardware receives:

- progressive cluster errors;
- feature importance;
- representation costs;
- microform data;
- view masks;
- semantic weights;
- hysteresis rules.

This metadata is part of the native asset format.

### 19.6 Scar Scribe consumes language-compiled material tables

The runtime does not improvise surface appearance. The compiler packs terrain materials, scar responses, decals, palettes and texture pages into fixed hardware recipes.

### 19.7 Hardware counters map back to source

All major command packets and generated work carry source IDs so the profiler can say which Form declaration consumed geometry, fragments, texture bandwidth or particle updates.

---

## 20. Deliberate non-goals

Do not turn Form into:

- a replacement for every general-purpose language;
- a dynamic object-oriented language;
- a garbage-collected scripting VM on the console;
- arbitrary game-to-Verilog synthesis;
- a general fragment-shader language;
- a macro system before the core language works;
- an LLVM research project before the C++ backend ships;
- an excuse to delay the exact triangle pipeline;
- a language whose behaviour depends on source-order draw calls;
- a language that hides resource costs.

C and C++ FFI remain available for platform code and emergency escape hatches. Escape-hatch code cannot bypass capture, ownership and determinism rules silently.

---

## 21. Frozen language decisions

1. The native language is called **Form** provisionally.
2. Form is statically typed and AOT-compiled.
3. The first compiler is TypeScript; the first native backend emits C++17.
4. Simulation is deterministic and fixed-tick.
5. Shipping gameplay has no garbage-collected frame-loop heap.
6. Truth and presentation are separate domains.
7. Presentation blocks are pure and reorderable unless explicit barriers are used.
8. Fixed-point and projected-pixel types are first-class.
9. Bounded pools and capacities are visible in source.
10. Terrain, formations, vertex warps and particles share one exact Field IR semantics with restricted profiles.
11. Hardware microprograms are loadable data; games do not resynthesize the FPGA.
12. Every hardware-lowered program also has an exact generated C++ evaluator.
13. Continuous LOD and representation ladders are language concepts, not merely asset-tool settings.
14. Terrain appearance includes compiled materials, scar response and polygon decals, not deformation alone.
15. Polygon particles are a standard representation level.
16. Split-screen budgets are declared and enforced as part of presentation.
17. Tests, captures, budgets and source mapping are first-class outputs of the compiler.
18. Wound Lab is the permanent language, emulator and hardware integration test.

---

## 22. The first language acceptance scene

The first complete Form program does not need hardware terrain or particles yet. It must prove the semantic architecture:

- two controller snapshots;
- two player entities;
- one deterministic 60 Hz simulation;
- one software-rendered island;
- one declarative Duo presentation block;
- one terrain-field definition evaluated in C++;
- one scar material response;
- one particle population evaluated in C++;
- one scenario capture;
- one cost report;
- identical simulation hashes on desktop and ARM.

As FPGA blocks mature, the same Form source progressively moves terrain, LOD, particles and rendering into hardware without changing gameplay meaning.
