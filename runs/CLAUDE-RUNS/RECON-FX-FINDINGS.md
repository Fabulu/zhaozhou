# RECON — visual effects machinery for the mana conduit (Unnamed02)

**Date:** 2026-09-04 · **Status:** recon only. Read-only pass; nothing built,
nothing rendered, nothing published. Written against
`Upheaval/creature/Unnamed02/OWNER-DIRECTION-1-2026-09-04.md`.

Paths are relative to the lane root
`C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\`.

Provenance is marked throughout: **[V]** verified by reading the code/spec named,
**[A]** reported by a sub-agent from a named file, **[I]** my inference.

---

## 0. Bottom line

* **A particle system exists and renders today.** [V] Not a stub. World-space,
  per-particle RGB, depth-tested, never depth-writing, unbounded count in
  software. Two of its seven hardware blocks are built and parity-tested.
* **It is OPAQUE.** [V] `draw_population` writes flat RGB with no blend. Ten
  kinds of *glowing* mana is not a knob away; it is an additive blend the
  particle path does not have yet, even though the fragment law defines one.
* **There is no lightning primitive, and live line drawing was refused.** [V]
  The sanctioned route is a ribbon/chain from `FORGE.PRIM` — specified, no RTL.
* **The skybox bloom is the `planet-sun-*` family.** [V] High confidence.
* **The single biggest structural blocker is one line of the reel:** a subject
  can install exactly ONE pre-resolve hook, and creature / celestial /
  planet-sky are three mutually exclusive claimants on it. [V] Today you cannot
  render a creature and a sun effect in the same clip.
* **Cost:** geometry is not the risk. **Fill is**, and fill is the one axis with
  no measured headroom. [A]

---

## 1. What effect systems exist today

### 1.1 The catalogue

`zhaozhou/effects-library.yaml` (484 lines) is the declared single source of
truth — 40-odd entries with `implemented:` flags, reel subject names, render
paths and CRC pins. [V] It is accurate about what has been rendered; it says
nothing about hardware maturity.

### 1.2 Stars and suns — `spec/stars_and_flares.md`

Twelve classes S00–S11, nine implemented as reel subjects. [V]

| § | What it owns | [A] |
|---|---|---|
| §1 | 64-entry CLUT ramp page, ARM-rebuilt per frame; glow effect-tag |
| §2 | Class gamut table; pulsar duty strobe (`spin_phase < 0x4000`, gain 160) |
| §3 | **Disc** — one masked billboard quad, 128² CLUT8 bake. **CLUT boil** = `rot = (tick/3) mod 63`, a palette rotation that touches **zero pixels**. `DISC_RMAX = 112 px` |
| §4 | **Corona/halo** — one baked radial CLUT8 sprite per variant: `halo_atmo` / `halo_space` / `halo_airless`. `HALO_RMAX = 225 px` |
| §5 | **Lens flare** — `burst12`/`burst4`/`streak` sprites, ghost chain at −26/−77/−230 of 256, occlusion probe + 15-frame fade |
| §6 | LOD ladder incl. far glints |
| §7 | Procedural starfield by sector hash |
| §15 | **Motion trails** — bounded retained-frame smear, decay 4 |

Reel subjects: `star-boil` (S03 + CLUT boil), `noctis-flare` (S00 + full ghost
chain), `pulsar` (S11 strobe), `blue-giant`, `white-dwarf`, `orange-giant`,
`blue-dwarf`, `multiple` (binary), `infant`, `flare-occlusion` (the probe fade).
All are cases in the switch at `zhaozhou/tools/reel/zhao_reel.cpp:1150-1400`. [V]

**The key architectural fact about all of them: nothing is evaluated per pixel.**
The corona is a baked LUT sampled as an axis-aligned additive sprite quad
(`reference/src/zsky/star_bake.cpp:116-145` bake,
`reference/src/zsky/star_compose.cpp:52-89` draw). [A] The animation is a palette
rotation. §4's own thesis: *"no per-pixel analytic evaluation anywhere — the LUT
**is** the texture"* (`spec/stars_and_flares.md:67`). [A]

### 1.3 Atmospheric suns — `atmo-sun-donor` / `atmo-sun-thick`

Celestial cases 11 and 12, `zhao_reel.cpp:1361-1393`; both fall through to the
same code and differ only in one ramp control point. [A] A 26 px disc with a
**104 px halo** (4× the disc, per §4's `halo_atmo` note), flare off, trail off,
over a deliberately flattened sky (`atmo_common()` at `:4136-4158` sets
`sky_variant = 2` and `island_flat = true`). [A] The code's own gloss: *"a dark
land under a vast glowing sky"*. [A]

### 1.4 Planetside suns — `planet-sun-*` (ten worlds)

`zhao_reel.cpp:375-620`. This is the most interesting thing in the tree for this
creature, and it is a fundamentally different mechanism from §1.2/§1.3. [V]

The sky is **a six-bit intensity plane, not RGB**. The sun is an additive radial
splat added *into that plane* and clamped at 63, which is the sky palette's own
peak entry — **before anything is coloured**. Consequences, all stated in the
source comment at `:375-415`:

* The sun has **no colour of its own**; it saturates to the sky's peak.
* With atmosphere (`sun_core = 0`) there is **no disc at all** — just bloom.
* *"The formless look is EMERGENT from additive-plus-clamp… the bloom has no
  edge anywhere. Alpha-blend a sprite instead and the effect cannot happen at
  any radius."*
* **Sky and sun together select at most 64 colours however large the bloom
  gets.** A second sun adds **zero** palette entries (`:425-429`).

The splat itself: `planet_sky_hook` at `:480-556`. Per sky pixel — one
`depth != 0` skip, a vertical ramp, two octaves of hashed value noise, one
`isqrt`, one linear falloff, a clamp, one ramp lookup. [V]

Ten worlds in `kPlanets` at `:577+`, each three control colours plus
base/noise/mag/core: violet-thick, terran-blue, dust-ochre, methane-teal,
airless-grey, ember-red, and four large ones up to a **420 px splat** that
saturates most of the 384×240 dome. [V]

### 1.5 Terrain effects

`terrain-wave` (`zhao_reel.cpp:3667`), `terrain-impact` (`:3702`),
`terrain-scars` (`:3762`), `terrain-orbit` (`:3815`), `terrain-breach` (`:3846`).
Field programs (`wave_pool`, `impact_wave`) deforming a dual heightfield, plus
persistent surface-sheet scars. [V] Not directly useful to a floating conduit,
but `terrain-impact` is where the debris/particle authoring lives (§3.2).

### 1.6 Sky and god beams — `spec/sky_and_beams.md`

Layers: lower/upper drum bands, zenith cap, under-plane, cloud sheet, and a
built-in **sun quad** drawn additively with `dst = sat(dst + src·tex.a)`,
Z-test on / Z-write off. [A] Plus `radial_decay` (§3, `:158`): a POST.COMPOSITE
glow-path mode, 12 taps over a 96×60 buffer, upscaled additive — a genuine
screen-space god-ray bloom, **spec-only, no `.cpp` found**. [A]

**God beams are additive world-anchored cones**, depth-test on, depth-write off,
LOD ladder 16-24 / 12 / 4 / 2 / 0 triangles. [A] This is the nearest thing in the
machine to a directed energy shaft, and it is the right shape for a conduit's
antenna beam — but it is `FORGE.PRIM`, phase 11, **SPECIFIED with no RTL**. [A]

### 1.7 Creature effects

`creature-wave-walk` (`:4298`) and `creature-bulk-pop` (`:5570`). The latter is
the only existing "creature explodes into stuff" effect: 18 chunks, integer
ballistics, gravity, damped ground bounce. [V] Drawn as **rotating cubes through
the ordinary triangle raster** in `creature_hook` at `:2833-2900`, and the source
is explicit that this is *"deliberately a fixed fragment list, not a new particle
or rigging framework"* (`:2836-2838`). [V]

---

## 2. Which one "blooms over the skybox"?

**Answer: the `planet-sun-*` family (§1.4).** High confidence.

The owner's phrase has two halves and this effect satisfies both literally:

* **"blooms"** — the code's own word, three times: *"a formless bloom near the
  horizon"* (`:382`), *"the bloom has no edge anywhere"* (`:403`), and the
  comment on the clamp itself, `// the clamp that makes the bloom formless`
  (`zhao_reel.cpp:552`). [V]
* **"basically just"** — with an atmosphere there is **no disc, no corona sprite,
  no lens flare, no trail**. The effect is nothing *but* a bloom. [V]
* **"over the skybox"** — the bloom is composited *into* the sky's own intensity
  plane and comes out in the sky's own colour. It is not over the skybox in the
  z-order sense; it is painted through it. [V]

**Runner-up: `halo_atmo` on `atmo-sun-donor` / `atmo-sun-thick`** (§1.3). A 208 px
additive glow ball — roughly 47% of the frame — over a flattened sky, with flare
and trail deliberately off. [A] "A vast glowing sky" is its own description. It
is a strong second and the owner may well mean it; the deciding evidence for
`planet-sun-*` is *"basically **just** blooms"*, and `atmo-sun` still draws a
disc.

**Also noted: `corona_sprite_bloom`**, `reference/src/zsky/star_bake.cpp:147-190`.
An **unratified** §4 amendment whose comment is the owner's phrase almost
verbatim — *"a formless bloom, mostly sitting below the horizon, whose light
bleeds a long way up into the sky with no boundary anywhere"*. It is referenced
only by its own header and is **never called by the reel**. [A] So it cannot be
what he saw rendered — but it is plainly the proposal written in answer to the
same complaint, and it is sitting there ready to be wired up.

---

## 3. Is there a particle system? **YES.**

### 3.1 What is real

**Software renderer — shipped and rendering.** [A]

* `reference/src/zrender/sprites.cpp:139-165` — `draw_population()`. Flags bit 0
  = point sprites (`blit_pattern_block`, `:70`), bit 1 = triangle sprites
  (`raster_tri`, `:162`). Both **test depth, never write it** (charter §8 pass
  7). [V]
* `reference/src/zrender/render_frame.cpp:309-311` decodes
  `ZHAO_OP_DRAW_POPULATION`; `:509` dispatches per viewport. [A]
* The reel authors and emits it for real: `zhao_reel.cpp:959-965` (`struct
  Debris`), `:2926-2927` (population handle 3 registered), `:3088-3099`
  (per-frame ballistic integer physics), `:3533-3540` (the `DRAW_POPULATION`
  ABI record, `flags = 0x0003` — points **and** triangles). [V]

**The particle record, software side** —
`reference/include/zref/zref_render.hpp:194-202`: [V]

```
struct Particle { int32_t x, y, z;   // world3 fx16 raw
                  uint8_t size;      // U 0.4.4 px  -> max 15.9 px
                  uint8_t r, g, b; };
struct Population { std::vector<Particle> parts; };   // unbounded
```

**Hardware maturity** — 7 PART blocks, ledger `design/blocks.yml`: [A]

| Block | line | maturity |
|---|---|---|
| PART.EXPAND (particle → triangle) | `:2774` | **UNIT_VERIFIED** |
| PART.SOFT (particle → scissored span) | `:2813` | **UNIT_VERIFIED** |
| PART.STATE / UPDATE / COLLIDE / SPAWN / LADDER | `:2624`–`:2768` | SPECIFIED only |

Maturity ladder: `SPECIFIED < REFERENCE_COMPLETE < UNIT_VERIFIED < RTL_VERIFIED
< SYNTHESIZED < INTEGRATED < HARDWARE_PROVEN`; nothing in the repo is above
RTL_VERIFIED. [A] RTL exists only for the two draw endpoints
(`fpga/rtl/particles/zhao_part_expand.sv`, `zhao_part_soft.sv`), both with
Verilator parity benches wired into CI (`tests/CMakeLists.txt:2585-2635`,
`.github/workflows/ci.yml:159,294`). [A]

**So: the DRAW half of the particle system is built. The SIMULATE half
(lifetime, spawn, collision, representation ladder) is prose.** The reel supplies
its own CPU-side physics to fill that gap, and that is exactly what a creature
pass would do too.

### 3.2 Spawn, sim, draw, budget — as they actually stand

* **Spawn:** hand-authored table in the subject. `subject_impact()` at
  `zhao_reel.cpp:3723-3745` declares **8** debris particles by hand — position,
  velocity, size 80–128 (i.e. 5–8 px), and one of two flat colours, with the
  comment *"opaque point/tri sprites (palette-cheap)"*. [V] That is the current
  state of the art.
* **Simulate:** integer ballistics in the frame loop, `:3088-3099`, gravity
  `t(t-1)/2`, despawn below `debris_floor`. [V] No lifetime curves, no forces,
  no noise.
* **Draw:** project, then either a solid subpixel rect or a 3-vertex fan. [V]
* **Sort/blend/depth:** no sort needed — depth test on, depth write off, so
  particles never occlude each other or anything else. **No blend at all.**
  `blit_pattern_block` at `sprites.cpp:70-88` writes `r,g,b` straight into the
  surface after a depth compare. [V]
* **Budget:** software, unbounded. Hardware required tier **32,768 active
  particles**, 128-bit record, ping-pong in HPS DDR, **1 MiB/tick = 63 MB/s**
  (`design/contracts/PART.STATE.md`). [V] 7 bits of `species` = **128 distinct
  behaviours at zero per-particle cost** — ten kinds of mana is nowhere near the
  ceiling. [V] Max 16 children per spawn event (`PART.SPAWN.md`). [V]
* **Representation ladder** (`PART.LADDER.md`, frozen owner ruling 2026-08-31
  §2.5): `meshlet → triangle/shard → ribbon/streak → soft sprite → glint →
  culled`, selected **per camera per frame**, simulation never affected. [V]

---

## 4. Is there a lightning / arc primitive? **NO.**

Nothing named `lightning`, `bolt`, or `arc` exists anywhere in the tree in a
rendering sense. [V] Worse than absent — the shape of it was **explicitly
refused**:

> `spec/stars_and_flares.md:87` — *"The entire chain is frozen-table additive
> sprite splats… **Live line drawing refused** (§26: unbounded per-pixel RMW ≈ a
> fragment program)."* [V]

Charter §26 (`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:1882-1900`) refuses general
fragment shaders and arbitrary compute kernels; the stars spec reads live line
drawing as falling under that. [V] The lens flare's spokes are therefore **baked
sprites**, not drawn lines: `streak` 96×16, spokes at screen-fixed angles, 1-px
additive lines rendered **at asset-compile time** into a 768×128 canvas and 8×
box-downsampled. [V]

**What could serve instead, in order of how real it is:**

1. **A chain of small triangles through the ordinary raster** — exactly what the
   bulk-pop gibs already do (`zhao_reel.cpp:2848-2900`). Works today, in the
   reel, with no new machinery. Authoring, not engineering. [I]
2. **The particle triangle path** (`PART.EXPAND` / `draw_population` tri branch)
   with particles placed along a jittered polyline — a bolt as a bead string.
   Works today. Opaque. [I]
3. **`FORGE.PRIM` ribbons/chains/tubes** — the sanctioned answer. Its declared
   vocabulary is *"ribbons, tubes, radial shells, rings, chains, shard bursts,
   billboard sheets, spline walls, cones"* (`design/contracts/FORGE.PRIM.md:7`).
   [V] Phase 11, **SPECIFIED, no RTL**. This is where a real bolt belongs.
4. **The god-beam cone** (`sky_and_beams.md`) — additive, depth-tested, ladder to
   2 triangles. Right blend, right occlusion law, wrong shape for a jagged arc;
   right shape for a conduit's steady channelling beam. Also unbuilt. [A]
5. **A baked flare `streak` sprite** — already exists, already additive, already
   in the glow buffer. Cheapest possible "arc flash". Not a bolt with a path. [I]

---

## 5. Cost

### 5.1 Frame top-line [A]

| Quantity | Value | Status |
|---|---|---|
| Compute budget | 1,666,667 clocks/frame (100 MHz ÷ 60) | SPECIFIED; clock is a **placeholder** |
| Resolution Z60 | 384×240 = **92,160 px** | FROZEN |
| Full-screen pass | 92,160 fragments = **1.00× overdraw** | **MEASURED** (`reports/PER_PIXEL_BUDGET.md:186-190`) |
| DDR bandwidth ceiling | **no ratified total exists** | NOT COSTED |

### 5.2 Per-instance cost of each system

| System | One instance costs | Dominated by | Several on screen |
|---|---|---|---|
| **Star disc + corona** | ≤8 triangles/view; `star_fragments ≤ 128K` Z60 / ≤64K per Duo view; VRAM ≤256 KB; **zero fabric for all animation** (CLUT rotation) | **fill** (a 225 px halo is ~a third of the frame) | **HARD CAP ≤2 near stars per view** — ARM rebuilds one 64-entry CLUT per star per frame [A] |
| **Lens flare** | `flare_texels ≤ 16,384`/view (¼ of `radial_decay`); 4 splats; occlusion = 1 latched tag byte | glow-buffer texels | **HARD CAP ≤2 flare lights per view**, 4 probe slots [A] |
| **Planetside bloom** (`planet-sun-*`) | one pass over sky pixels only; per pixel ≈ 1 skip + ramp + 2 noise octaves + 1 `isqrt` + clamp + LUT | **per-pixel work on the sky area** | **Two suns cost ~2× the splat and ZERO extra palette** [V]. Cheapest bloom in the machine per unit of screen impact. |
| **God beam** | ladder 16-24 / 12 / 4 / 2 / 0 tris; additive, Z-write off, bilinear mandatory; 1 ARM DDA ray (64 steps) per beam per view | fill, then the CPU ray | scales linearly; occlusion is CPU-side [A] |
| **Particles (software)** | per particle: 1 projection + a ≤16 px² fill or a 3-vertex fan | **fill × count**; triangles are trivial | linear; no sort, no blend, no depth write [V] |
| **Particles (hardware tier)** | 1 particle/clock; 32,768 = 2.5% of a frame's clocks | **bandwidth, not clocks** — 1 MiB/tick = 63 MB/s | the contract says optimising clocks/particle is the wrong lever [V] |
| **Terrain field program** | worst legal single patch = 557,568 instrs ≈ **33% of a frame** | field instruction intake | ≤16 programs/patch, >16 **rejected** not dropped [A] |
| **Creature mesh** | Zixxtrixx LOD0 3,680 → **2,076 triangles**, no visible cost at 240p | LOD ladder is **mandatory** | **pose cost scales with VARIETY, not COUNT** — identical creatures in step are nearly free [A] |

### 5.3 The three numbers that decide this creature

1. **Fill is the constraint, not geometry.** `reports/PER_PIXEL_BUDGET.md:135-140`
   — three independent per-pixel units all landed within 2× of budget: *"there is
   no slack anywhere on the per-pixel path."* [A] Every effect the owner asked
   for is a fill effect.
2. **Suns and flares are hard-capped at two per view.** [A] If each conduit wants
   its own sun-style corona, **two conduits exhausts the budget** and a third gets
   nothing. This is the single most important collision between the owner's ask
   and the machine.
3. **Pose cost scales with variety.** [A] Several conduits playing the *same*
   clip in step are nearly free on the animation side. Stagger their phases and
   the decoded-pose cache (~192 KiB, ~128 tuples) starts to matter.

### 5.4 Health warnings [A]

* 0 of 91 modules have an RTL fit at HEAD; **do not quote a timing slack.**
* Every "% of frame" above is arithmetic against a stated capacity, **not a frame
  measurement.** No trace of the real content tier exists.
* `spec/counters.md:79-89` has **no polygon, particle or fragment counters** in
  the ratified set. There is no instrumentation to measure an effect budget with.
* The budget-audit run's own conclusion: five of seven wrong calls came from
  reasoning about the tool instead of measuring it. Same shape as the art law.

---

## 6. Creature-side hooks — how an effect would attach

### 6.1 The blocker: one hook slot

`zref::render::SoftwareRenderer::set_pre_resolve(fn, ctx)`
(`reference/include/zref/zref_render.hpp:326-329`) stores **one** function
pointer. [V] The reel calls it three times, unconditionally overwriting:

| order | caller | line | condition |
|---|---|---|---|
| 1 | `cel_hook` (stars, coronae, flares) | `zhao_reel.cpp:2951` | `sub.celestial != 0` |
| 2 | `planet_sky_hook` (the bloom) | `:2972` | `sub.planet > 0` |
| 3 | `creature_hook` | `:3047` | `sub.creature != 0` |

The source states the exclusivity outright at `:2954-2956`: the planet sky *"is
its own hook and is exclusive with the celestial compositor."* [V] **A creature
subject silently wins over both.** So today: no creature clip can carry a sun
effect or a bloom.

**This looks cheap to fix.** [I] The hooks are ordered, depth-aware passes over
the same `(rgb, depth)` buffers: `planet_sky_hook` only touches `depth == 0`
pixels, `compose_creatures` writes depth, `cel_hook`'s star compose depth-tests
against `kStarDepth` (`star_compose.cpp:71`). Chaining them in the order
sky → celestial → creature should compose correctly with no law change. I have
not tried it; this is inference from reading, and it is the first thing to
prototype.

### 6.2 What attachment mechanisms exist

* **No emitter concept and no named socket.** [V] Nothing in the tree maps a
  bone/joint to an effect origin.
* **The closest existing pattern is `ZixxSunSpec`** (`zhao_reel.cpp:2131-2136`):
  a world offset *from the tracked staged centre*, plus radii and per-channel
  gains, evaluated per frame and tracking the creature. [V] It is a light, not a
  visual, but it is exactly the shape a socket wants: an authored offset that
  follows the creature.
* **Gibs are pose-derived** — `spawn_reel_gibs` at `:2058-2085` reads the posed
  bone matrices to place chunks. [V] That is the proof that "spawn something at a
  point on the animated creature" already works; it just isn't generalised.
* **Debris is NOT creature-attached.** It spawns at a fixed world `x0,z0`
  (`struct Debris`, `:960-964`). [V]
* **Point lights ARE creature-attached and capped at four.** `kCreatureMaxPointLights = 4`
  (`reference/include/zref/zref_creature.hpp:1084`), the console's simultaneous-source
  budget; strongest four win (`Upheaval/creature/08-LIGHTING.md`). [V]

**So an effect would be authored in the reel subject alongside the creature**,
reading the posed bones the way the gib spawner does. That is the house pattern
and it is the honest answer.

---

## 7. Colour

* **Star/sun colour** comes from a 64-entry CLUT ramp built from a class RGB, and
  the corona rides *the same ramp* un-rotated, so a corona is automatically its
  star's colour fading to black through additive identity. [A] The twelve classes
  are a fixed table — a bespoke mana hue means a bespoke ramp.
* **Planetside colour** is fully authorable: `planet_ramp()` (`zhao_reel.cpp:458-473`)
  builds 64 entries from **three** control colours (lo / mid / hi), and the sun IS
  entry 63. [V] Ten worlds already do this. Ten mana colours would be the same
  mechanism.
* **Particle colour** is per-particle `r,g,b` in the record. [V] Ten kinds in ten
  colours is trivially authorable today — as ten flat opaque colours.
* **The 256-colour law does not bind here.** `SceneSubject::full_colour`
  (`zhao_reel.cpp:1067-1075`) exempts a subject from the palette gate; the gate
  is a GIF-export constraint and the bestiary's primary format is full-colour
  webm. [V] The gate check is at `:3586-3597`.
* **The additive point-light term interacts, and helpfully.** [V] Approved
  2026-09-03 (`reports/CREATURE-LIGHT-ADDITIVE-TERM.md`, reference on `main` at
  `3d1ace62`, gated off by default). Each source carries `add_r/g/b` alongside its
  multiplicative gain, riding the **same lambert × attenuation response**, applied
  after the texel multiply and **before** the final saturate — so it bypasses the
  1.0 channel ceiling. Cost: **+3 MACs and one saturating 3-channel add per
  source**, ruled per-fragment. For a mana conduit this is the difference between
  a cyan glow that reads cyan on pink pigment and one that reads white. Two
  documented traps in `08-LIGHTING.md`: a source strong enough to clamp all three
  channels becomes a hue-neutral floodlight that erases every other source; and a
  path that never enters its own inner radius contributes counts nobody can see.

---

## 8. What the owner asked for that DOES NOT EXIST YET

Ordered by how much has to be built.

| # | Ask | Status | What it would take |
|---|---|---|---|
| 1 | **Ten kinds of particle in different colours** | **Mostly exists.** Ten colours and ten behaviours are authorable today. | Generalise the 8-entry hand-authored debris table into named kinds with lifetime/velocity laws. Reel-side authoring, no engine. |
| 2 | **Particles that read as MANA (glowing)** | **Does not exist.** `draw_population` is opaque; no blend. | Add an additive/ALPHA path to `blit_pattern_block` + the tri branch. The fragment law already defines `ADD`, `ADD_MOD` and `ALPHA` (`RASTER.FRAGMENT.md:42,123-125`) — the particle path just never reaches for them. **This is the single highest-value change in this document.** |
| 3 | **Particles attached to the creature's centre** | **Does not exist** as a concept; the ingredients do. | A socket: an authored offset from a posed bone, the way `spawn_reel_gibs` reads the pose and `ZixxSunSpec` tracks the centre. |
| 4 | **Creature + sun effects in one clip** | **Blocked.** One hook slot, three claimants. | Chain the pre-resolve hooks (§6.1). Looks cheap; unproven. |
| 5 | **Sun effects, various, incl. the skybox bloom** | **Exists** (`planet-sun-*`, `atmo-sun-*`, nine star subjects) — but see #4, and see the **≤2 suns/flares per view** cap. | Wiring, then a hard decision about the per-view cap with several conduits on screen. |
| 6 | **Lightning** | **Does not exist**, and live line drawing is refused. | Author a bolt as a triangle chain or a bead of particles (works today), or build the `FORGE.PRIM` ribbon (phase 11, no RTL). |
| 7 | **Big effect in its CENTRE** | **Partly blocked by mechanism.** The `planet-sun` bloom paints only where `depth == 0` — it is a *sky* effect and the creature would occlude it. | A bloom at a creature's centre is a different draw: an additive sprite at a projected point, i.e. the `halo_atmo` corona path aimed at a world position instead of a sun. That path exists (`star_compose.cpp:52-89`) and is cheap. |
| 8 | **Cheap with several on screen** | **Unmeasurable today.** | `spec/counters.md` has no fragment/particle/polygon counters. Cost claims for this creature will be arithmetic, not measurement, unless something is instrumented. Say so when publishing. |

---

## 9. Recommended reading order for whoever builds this

1. `reports/DoubleHelixTornado.md` (427 lines) — an owner-facing feasibility
   answer for a large hybrid VFX, built from exactly these subsystems. It is the
   closest thing to a design precedent and its cost verdict names **alpha/overdraw
   at 5/10 as the one real risk**, everything else 1–3/10. Its recipe: ~50%
   procedural ribbons, ~35% soft particles, ~15% polygon debris — *"not a giant
   opaque cone mesh, not 2,000 individual particles either."* And its governing
   rule: **visual density is governed by screen coverage, not by a fixed particle
   count.** [V]
2. `Upheaval/creature/08-LIGHTING.md` — the four-source budget and the two ways a
   light is present and invisible.
3. `spec/stars_and_flares.md` §4 and §5 — the bake-don't-compute discipline that
   every cheap effect here obeys.
4. `zhao_reel.cpp:375-620` — the planetside bloom, and the best-written argument
   in the tree for why additive-into-a-clamped-plane beats compositing a sprite.

---

## 10. Provenance summary

**Verified by me, in code:** the particle record and draw path; the opacity of
`draw_population`; the single pre-resolve hook slot and its three claimants; the
planetside bloom mechanism and its palette property; the debris authoring in
`subject_impact`; the gib draw as ordinary triangles; `ZixxSunSpec`; the
`full_colour` palette exemption; the refusal of live line drawing; `FORGE.PRIM`'s
vocabulary and SPECIFIED status; `kCreatureMaxPointLights = 4`.

**Reported by sub-agents from named files:** the ledger maturity table; the CI
wiring of the particle benches; the `atmo-sun` halo geometry; the corona bake/draw
primitives; the whole of §5's cost table; the ≤2-per-view sun and flare caps.

**Inferred, not verified:** that chaining the three pre-resolve hooks composes
correctly; that a triangle chain is a workable bolt; that `planet-sun-*` rather
than `atmo-sun-*` is the effect the owner meant — the reasoning is in §2 and the
runner-up is close enough that it is worth one question or one render.

**Not attempted:** nothing was built, run, rendered or measured. Every cost figure
here is specified or arithmetic; the only measured numbers are the per-pixel unit
costs in `reports/PER_PIXEL_BUDGET.md` and the 1.00× full-screen overdraw.
