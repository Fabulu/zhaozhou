# RECON — standing up creature 2 (Unnamed02, the mana conduit)

**Date:** 2026-09-04 · **Mode:** read-only recon, no source edited, nothing built,
nothing rendered, nothing committed.
**Lane:** `zixxtrixx-wholebody-s-spring-20260901/{zhaozhou,Upheaval}`, branch
`zixxtrixx-wholebody-s-spring` in both.
**Claims are marked VERIFIED (read in this lane) or INFERRED (reasoned).**

---

## 0. The answer in one page

1. **Author creature 2 in `zhaozhou/tools/reel/`, as NEW files beside Zixxtrixx**
   — but split into four small headers from day one, named so a later `git mv`
   into the Upheaval package is mechanical. The ownership migration has **not
   landed** and standing it up is a separate project, not this creature.
2. **The durable record — SPEC, CREATURE.json, owner direction, texture recipe,
   probe policy, site-entry — goes in `Upheaval/creature/Unnamed02/`**, scaffolded
   by `new_creature.py`. That is the half of the blueprint that works today.
3. **The generic API is real and complete.** `zref::creature` takes exactly
   `Skeleton` + `ClipBank` + `vector<RingPart>` into `compile_creature()` and
   returns the whole runtime object. Zixxtrixx itself consumes it. Nothing about
   snake anatomy is baked into the reference lane.
4. **Spheres are easy and nobody has written one.** The ring builder is a
   swept-surface lofter along local +Y — which is exactly a UV sphere's
   parameterisation. `radius = R·sinθ`, `y = -R·cosθ`, both caps. ~10 lines. No
   engine change. Four balls ≈ 900 triangles.
5. **The site needs no code change**: prepend one object to `creatures[0]` in
   `website/creatures.json`. Ordering is pure array order.
6. **Per-instance budget: ~1,300 triangles / ~670 verts / ~11–14 meshlets / 8–10
   bones / one RGB565 body page + one eye page.** Comfortable for several on screen.
7. **Two reel walls are on the critical path** (§6): the species selector is a
   binary `sub.creature >= 3`, and there is exactly **one pre-resolve hook slot**,
   so a creature subject cannot currently also carry a sun corona / skybox bloom
   — which the owner explicitly asked for. Spike the hook composition first.

---

## 1. Read the direction again — it changed under us today

`Upheaval/creature/Unnamed02/OWNER-DIRECTION-1-2026-09-04.md` was **101 lines at
the start of this recon and is 119 now** (HEAD commit `1cb1ec5`, "Creature 02
Direction 1: it FLOATS"). VERIFIED.

**The creature FLOATS.** *"Oh, and I forgot, the creature floats. No legs to
animate. Woo!"* Not cosmetic: **Zixxtrixx's ground-contact law does not apply** —
no planted support, no declared bite, no peel. The committed posed-vertex probe
must assert **clearance above terrain at all times**, not a penetration window;
the blueprint's probe template already expresses that (author `min_allowed_mm > 0`
with no upper bound), and declaring "this creature never contacts" explicitly is
required by the direction, not optional. **The hover IS the idle** — drift, bob
and settle are the whole baseline read. Locomotion is drift/follow: **no walk
clip, no footfall events.**

Everything else: four balls (one big body + three hinge spheres in the antenna
loop), bulging *partly polygonal* eyes more expressive than Zixxtrixx's, visible
up/down compression, ten particle kinds in varied colours, lightning, the
existing sun effects including the one that blooms over the skybox, cheap enough
for several instances, textured, published **at the top** of the bestiary.

**Re-read that file before starting.** It has been amended twice in one day and
CLAUDE.md's "instructions are not delivered until they are read" law is live here.

---

## 2. The scaffolder — what it emits, and how complete it is

`Upheaval/creature/CREATURE-AUTHORING-BLUEPRINT/tools/new_creature.py` (206 lines).
Manifest-driven: 15 templates → 15 files + a generated `SCAFFOLD.json` = the
16-file package. Deterministic (LF-canonical hashing, no clock, no `hash()`).
All 15 templates together are **1,108 lines**. VERIFIED.

| emitted file | what it is | how complete |
| --- | --- | --- |
| `source/creature_authoring.hpp` | 16 lines. Declares `const zref::creature::CreatureType& type();` in `namespace upheaval::<slug>`. | **Complete** — this is the whole seam and it is correct. |
| `source/creature_authoring.cpp` | 100 lines. Builds a 2-bone, 3-ring grey chain capsule and calls `zc::compile_creature`. | **A working smoke fixture, not a fill-in skeleton.** It compiles and would render; it teaches the API shape (`ring()` helper, chain part, `ClipBank`). Its own header: *"intentionally a tiny grey capsule… not a form proposal"*. |
| `source/art_controls.hpp` | 78 lines of neutral named constants: form, rig weights, pigments, crayon knobs, light rig, cel thresholds, LOD ladder + hysteresis, 14 clip key counts. | **A good checklist, zero art.** Its value is the *taxonomy* — which knobs a creature is expected to name. |
| `source/motion_plans.hpp` | 117 lines. Integer `JumpPlan`/`sample_jump` and `TargetAttackPlan`/`sample_attack` + `choose_outcome`. | **Real working fixed-point code — but a jump-and-attack vocabulary, and Unnamed02 has neither.** Keep `lerp_i32` and the phase-plan pattern; write hover/drift/channel plans instead. |
| `texture/texture_recipe.py` | 137 lines. P6 PPM → `page.rgb565` + `recipe-manifest.json` with SHA256s. Refuses output outside `build/generated/creatures/<id>/`. | **Runs, but emits a raw blob, not a C++ page header** — see §8. |
| `probes/posed_vertex_probe.cpp` | 102 lines. Decodes every key **and** every 60 Hz midpoint, skins every full-detail vertex via `zc::skin_vertex`, checks a declared clearance band, fails on any `SatLedger` saturation. | **Complete and correct** — real API calls, exactly what CLAUDE.md's committed-3D-probe law demands. Only the policy table is placeholder. |
| `reel/subjects.cpp` | 67 lines. A `SubjectSpec` table (name, slot, View, Shade, camera, every-frame) plus integration requirements as comments. | **A PLAN, not running code.** Its own header: *"adapt these records to the generic zreel/provider seam available at integration time… do not copy that monolith here."* **No such seam exists.** Nothing consumes this file. |
| `CREATURE.json` | 109 lines. Identity, ownership, pinned zhaozhou commit, contract limits, clip/subject id registry, LOD ladder, ground-contact declarations, generated-output policy. | **Complete as a record.** Nothing reads it programmatically yet. |
| `media/site-entry.json` | 44 lines. Staging record for one clip: native 384×240/60, VP9 yuv444p + 3× nearest poster, captions, archive/publication checkboxes. | **A staging form.** `website/creatures.json` remains the source of truth; transcription is manual. |
| `README.md`, `SPEC.md`, `OWNER-DIRECTION-TEMPLATE.md`, `validation/*` ×2, `texture/source/README.txt` | prose | Templates. Fill in. |

**Verdict.** The package is **a working skeleton for the model half and a plan for
the pipeline half.** `creature_authoring.{hpp,cpp}` + `probes/` + `texture_recipe.py`
are real and use the real API. `reel/subjects.cpp` and `media/site-entry.json` are
aspirational — they describe a seam and a publisher that do not exist.

### Gotcha that will bite on the first command (VERIFIED)

`new_creature.py` **refuses a non-empty destination**:
```python
if output.exists() and (not output.is_dir() or any(output.iterdir())):
    raise SystemExit(f"destination exists and is not empty: {output}")
```
`Upheaval/creature/Unnamed02/` already holds `Concept/`, `README.md` and
`OWNER-DIRECTION-1-2026-09-04.md`, and `--output` must be a **direct child** of
`creature/`. So the obvious invocation fails. Also `README.md.tmpl` and
`OWNER-DIRECTION-TEMPLATE.md.tmpl` would collide with the hand-written ones.

**Do this:** scaffold to a scratch sibling, then move in only the files that do
not already exist, keeping the hand-written `README.md` and the owner direction.
```
python creature/CREATURE-AUTHORING-BLUEPRINT/tools/new_creature.py \
  --upheaval-root . --id unnamed02 --name "Unnamed02" \
  --output creature/Unnamed02Scaffold
```
(`--id` must match `[a-z][a-z0-9]*(-[a-z0-9]+)*`; `unnamed02` is legal. Rename
the id the moment the owner names the creature — it is embedded in every file.)

---

## 3. Where creature 2's code actually goes — RECOMMENDATION

### The migration has NOT landed. VERIFIED.

`CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md` (611 lines, 2026-08-29) is marked
**"Status: proposed migration contract."** None of its target layout exists:
`Upheaval/tools/` holds only `githooks/` (no `tools/creature/`, no CMakeLists);
`Upheaval/creature/Zixxtrixx/` has `AGENT-PACK/ Concept/ golden/ tools/` and **no
`source/`, `reel/` or `probes/`**; `zhaozhou/tools/reel/include/zreel/` does not
exist — **no reel library, no provider seam, no `CreatureSourceProvider`**; and
`zhao_reel.cpp` (6,223 lines) still `#include`s the creature directly and
dispatches by hard-coded name.

### The two honest options

| | **(a) `zhaozhou/tools/reel/`, beside Zixxtrixx** | **(b) the Upheaval package the blueprint scaffolds** |
| --- | --- | --- |
| renders today | **yes** — one `#include` and ~15 dispatch lines | no — requires building `zreel` + an Upheaval build target first |
| build | `tools/reel/build-direct.sh --output <dir> cel` already works | must be written from nothing |
| texture page | `mkcreaturepage.py` → C++ header pattern proven | recipe emits a raw blob; header emitter missing |
| probes | `zixx_*.cpp` pattern proven, build-direct has targets | template is correct but has no build |
| ownership | **wrong** — game content in the console repo, again | right |
| cost to move later | one `git mv` + include paths (code only depends on `zref/*`) | none |
| risk | repeats the debt the architecture doc was written to stop | the creature stalls behind an infrastructure project |

### Recommendation: **(a), structured for (b).**

Concretely — new files, none of them touching Zixxtrixx:

```
zhaozhou/tools/reel/
  unnamed02_art.h        # ALL named knobs: form, palette, timing, light, LOD
  unnamed02_model.h      # make_ball(), the loop, the eyes -> vector<RingPart>
  unnamed02_rig.h        # Skeleton + bone ids
  unnamed02_clips.h      # deterministic clip builders, deform sidecar
  unnamed02.h            # composition + `unnamed02::type()`  (the only header
                         # zhao_reel.cpp includes)
  unnamed02_page.h       # GENERATED, gitignored — do not track it
  u02_probe.cpp          # hover-clearance probe (blueprint template, adapted)
```
plus a `subject_u02_*()` block in `zhao_reel.cpp` and its dispatch lines.

**Why this split matters.** Zixxtrixx is one 8,514-line header because byte
identity froze it that way; creature 2 has no such constraint, so splitting now
costs nothing and makes the eventual migration a `git mv` of five files into
`Upheaval/creature/Unnamed02/source/` under the names the architecture doc already
specifies. The doc's own note supports it: *"Splitting the current monolithic
header is useful, but byte identity takes priority"* — which does not bind a new
file.

**The Upheaval package is still created**, holding `CREATURE.json`, `SPEC.md`, the
owner direction, `texture/`, `validation/`, `media/site-entry.json` and the probe
*policy*. Record `"copied_or_adapted"` honestly and note in `migration_assumptions`
that the C++ currently lives in `tools/reel/`. **Do not** put creature 2 inside
`zixxtrixx.h`, and **do not** start from it — it is anatomy end to end.

---

## 4. Generic vs Zixxtrixx-specific

### GENERIC — link and consume. `zhaozhou/reference/`. VERIFIED.

`reference/include/zref/zref_creature.hpp` (1,177 lines, `namespace zref::creature`),
`reference/src/zcreature/creature_core.cpp` (1,421) and `creature_sim.cpp` (1,191).
The two `.cpp`s mention Zixxtrixx **three times, all in explanatory comments,
never in code.**

**The contract is four inputs and one output:**
```cpp
bool compile_creature(const Skeleton&, const ClipBank&,
                      const std::vector<RingPart>&, CreatureType& out,
                      const char** reason);           // + midpoint-authorship overload
```
* `Skeleton{bone_count; array<Bone,32>}`, `Bone{parent, tx,ty,tz}`.
  **Parent-before-child required**; rest rotations identity by convention, so
  bind is a pure translation chain and `inv_rest` is exact.
* `ClipBank{bone_count, vector<Clip>, vector<SeamPair>, bake60}` — `bone_count`
  must equal the skeleton's. `SeamPair`s are **byte-compared at compile and
  FAIL the build** on mismatch.
* `Clip{slot_id, frame_count, root, quats, events, deform, interpolate,
  hold_last, mid_*}`. Frames are **baked samples, not keyframes** — 30 Hz keys
  held 2 sim ticks, **hard-cut transitions only**, 64 slots.
* `RingPart` is the **only** vertex source. No triangle-soup path exists.

**Ring construction is a LIBRARY**, not a pattern to reimplement:
`std::vector<Meshlet> build_ring_part(const RingPart&)` (`creature_core.cpp:705`)
plus `constexpr int ring_side_tris(rings, segments)`. `compile_creature` runs it
twice per part (full + decimated micro rung), computes `bound_radius`, generates
**area-weighted, position-keyed** smooth normals on both rungs, and splits
oversized parts at ring boundaries with the seam ring **duplicated** — watertight
by construction. Normals are **generated, never authored**.

**Skinning / pose:** `skin_vertex`, `skin_normal_lambert`, `decode_pose`,
`PoseBank::acquire` (128-tuple cache; instances of one type on one clip at one
tick **share** a decoded pose — cost scales with variety, not count).

**The deformation sidecar is generic and is exactly the "compresses up and down"
mechanism.** `DeformSample{flatten, spread}` (Q0.16 deltas from identity; `{0,0}`
is exact identity; a positive-volume gate rejects `flatten == 1.0`) rides on
`Clip::deform`/`mid_deform`. Per-vertex `DeformVertex` is authored on `RingSpec`
via `deform_role`/`deform_axis`/`deform_strength`/`deform_center_*`, quarter-turned
with the ring. `kRadial` scales about a centre with exact fixed-point
inverse-transpose normal correction; **`kFollower` translates an attachment by its
carrier's contraction only, keeping its own dimensions rigid — that is how the
eyes ride a squashing body without being crushed.** And: *"It never enters the
PoseBank, so decoded bone matrices remain rigid and shareable"* — squash-and-stretch
costs nothing in the army-sharing economy.

**Also generic:** `anim_advance`, `LodRung{kMesh,kMicro,kSplat,kGlint}` +
`lod_update` (hysteresis, `kLodHoldTicks = 15`), `bulk_update`, `spawn_gibs`,
`compose_creatures`, ~28 named `CreatureLightRig`s, `g_creature_point_lights` /
`g_creature_additive_light` (default OFF), `g_debug_shade`, `g_cel_bands`.
Particles: `Population{vector<Particle{x,y,z,size,r,g,b}}` drawn by
`draw_population` — points (flag b0) or tris (b1), **depth-tested, never
depth-written** (particles never occlude). Sky/star: `zref_sky.hpp` (`SkySet`,
sun quad, `kLayerSunGlowTag`), `zref_star.hpp` (12-class gamut, coronas, halos,
lens-flare chains, procedural starfield).

### ZIXXTRIXX-SPECIFIC — do not copy. VERIFIED.

`zhaozhou/tools/reel/zixxtrixx.h` (**8,514 lines**), `zixxtrixx_page.h`,
`zixxtrixx_page_cel.h`, `zixxtrixx_page_debug.h`, `zixxtrixx_profile.h`, and
`zixx_probe/golden/meshcheck/striketip/planner/choreo/headaim/sideprofile/springpose.cpp`.

**Section map of `zixxtrixx.h` — roughly 79% anatomy, 21% scaffolding.** The
parts a second creature should MIRROR (not copy verbatim, except where noted):

| lines | what | verdict |
| --- | --- | --- |
| 47–72 | namespace open + the three-way page include switch | **mirror** |
| 74–1220 | the KNOBS block (taper, head, eyes/pupils, blades, colours, clip lengths, spring bank) | anatomy — mirror only the *habit* of one findable block |
| 1222–1337 | `quat_axis/x/y/z`, `quat_mul`, `quat_conj`, `quat_rot_vec`, `asin16`, `struct Key`, `curve()` | **copy verbatim — pure maths** |
| 1486–1639 | `station_x`, `station_r`, `station_sides`, `struct Bind`, `station_bind` | **mirror the pattern** (a station→bind law) |
| 1640–1728 | tile enum, `page()`, `page_direct()`, `set_rgb()` | **mirror** — bodies differ, shape is generic |
| 1729–1762 | `struct Rig` (the per-bone quat accumulator written into a `zc::Clip`) | **mirror** |
| 1763–7601 | gaze, stance, ~1,400 lines of spring-chain machinery, the attack planner, every clip, the salto/jump exported API | anatomy — none of it transfers |
| 7602–8065 | `// ---- the build ----`: `type()`, one static lambda building `Skeleton` + `RingPart`s + `ClipBank` → `compile_creature` | **mirror exactly** |

There is **no exported-API block** — public symbols sit wherever they were
authored. Creature 2's header should declare its interface up front instead.

Anatomy that must not travel: the nose dome table `kNoseDome[4]`, the body taper
and girth progression, `kEyeStation0/1` + `ZIXX_EYEBULGE` + the whole moving-pupil
stripe block, `kTailRoll` and the fin blade, the canonical-S slope table, the
spring/salto/attack choreography, `AttackPlan{compress,coil,unroll,plunge,
spear_dx_mm}` in the *generic header* (a Zixxtrixx-shaped record — ignore it,
this creature has no attacks), `ZixxSunSpec` / `kZixxSun<Clip>` / the
`g_zixx_additive_normal` gate, and `tools/pack/mkcreaturepage.py` (a Zixxtrixx
pigment/grain recipe wearing a generic name).

**One divergence worth naming early.** `zixxtrixx.h:370`: *"THE EYE DISC IS PAINT;
ONLY THE MOVING PUPIL IS TINY GEOMETRY. A whole yellow eyeball mesh looked
exactly like a sphere glued to a tube."* Unnamed02's direction says the opposite —
the eyes **bulge** and must read **partly polygonal**. So the eyes here are real
faceted geometry. That is not a contradiction of the lesson: Zixxtrixx's eye sat
on a smooth tube, this creature's face *is* a ball and a faceted lens belongs on
it. Author it, render it, look. What **does** transfer is the mechanism:
mirrored per-eye bones driven by deterministic per-clip gaze curves
(`kPupilGlanceA16`, `kPupilHeadLagKeys`, the idle move-in/settle/hold/move-out/rest
key schedule) — that is the "more expressive" precedent to beat.

**Doc warning (VERIFIED).** `01-RING-CONSTRUCTION.md` lines 42–63 print a
`RingSpec`/`RingPart` listing that **predates the 2026-08-26 amendment**: no
`b0/b1/w0`, `cx/cz`, `rx/rz`, `chain`, `page`, `v0/v1`, `micro_keep_*`, or the
`deform_*` block, and it repeats the retracted *"ONE bone per part (donor law)"*
claim. **Read `zref_creature.hpp`, not the guide's struct block.**

---

## 5. Spheres — the four balls

### Can the ring machinery build a sphere? Yes, well.

`build_ring_part` is a swept surface along local **+Y**: each `RingSpec` gives
`y`, `radius` (or elliptical `rx`/`rz`), an off-axis centre `cx`/`cz`, and its own
`segments`. `kCapTop`/`kCapBot` close the ends with triangle fans whose apex sits
at the ring centre. **That is a sphere of revolution's parameterisation exactly.**

Working in favour: `generate_smooth_normals` is area-weighted and **keyed on exact
bind (x,y,z)**, so the wrap vertex, the duplicated split seam ring and the pole
apex all get **identical** packed normals — a sphere comes out smooth, no seam;
meshlet splits duplicate the seam ring (watertight); unequal `segments` between
adjacent rings are handled by the integer zipper walk, so segment count can
**fall toward the poles**; and `rx`/`rz` give an ellipsoid free, which is exactly
what the flat blade-like antenna loop needs.

Constraints: `segments ∈ [3,32]` (enforced, `creature_core.cpp:1036`); `idx` is
`uint8_t` with `kMeshletMaxVerts = 64`, so ~3 rings of 16+1 verts per meshlet;
pole triangles are the usual UV-sphere slivers (invisible at 240p — and the owner
*wants* facets on the eyes); and `v_lane_of` maps V **linearly in ring index, not
arc length**, so space rings by equal height or compensate with `v0`/`v1` or the
texture stretches at the poles.

### Nothing in the codebase generates a sphere. VERIFIED.

No `icosphere`/`uv_sphere`/`build_sphere`/`make_sphere`/`geodesic` anywhere in
reference, tools or templates. The only precedent is Zixxtrixx's hand-tabulated
nose dome `kNoseDome[4] = {270, 590, 810, 950}` (radius factors per mille,
authored by eye). `SACRIFICE-NOTES.md` records the donor's 24×25 UV sphere and
**rejects** it at 384×240 as not earning its place — so keep the counts low.

### The cheapest honest sphere

A local helper in `unnamed02_model.h` — **not** a promotion into the engine until
a second creature wants it:

```cpp
// R fx16, rings latitude bands, seg equator segments, both caps.
RingPart make_ball(int32_t R, int rings, int seg, uint8_t bone);
//   theta_i = (i * 32768) / (rings - 1)          // half turn, angle16
//   y_i     = -mul_fx(R, fx_cos(theta_i));
//   rad_i   =  mul_fx(R, fx_sin(theta_i));
//   segments_i = taper seg -> 6 near the poles   // zipper walk handles it
```
Use `zref::fx_sin` / `fx_cos` — the same tables `build_ring` uses internally, so
it stays all-integer. Every input is a named constant in `unnamed02_art.h`
(`kBodyRadiusMm`, `kBodyRings`, `kBodySegments`, `kHinge*`…) per CLAUDE.md rule 6.

### How the hinges become joints

* **One bone per hinge** (`kBHinge0/1/2`), parented up the loop from the body
  root, each bone's bind translation **at that hinge ball's own centre**.
  `01-RING-CONSTRUCTION.md`'s hard-won lesson applies directly: *"a pivot offset
  from the head's mass makes the head orbit a distant point instead of turning
  about itself."*
* **The hinge balls are rigid parts** (`RingPart::bone = kBHingeN`) — a ball at a
  joint does not need to deform.
* **The antenna loop between them is ONE `chain` part**, rings in creature-global
  bind space with per-ring `{b0,b1,w0}` blending across each hinge. That primitive
  exists precisely because separate rigid parts opened Zixxtrixx's 61 mm hole.
  Elliptical `rx`/`rz` per ring make it blade-like.
* **Secondary motion** = hinge rotations lag the body by a few keys and settle,
  authored in the clip builders and baked. No runtime physics (project law).
* **The compression** is the `DeformSample` sidecar on the body ball
  (`kRadial`, vertical axis, centre at the ball centre); eyes and markings use
  `kFollower` so they ride the squash uncrushed. One `{flatten,spread}` per key.
* **Bind straight, pose the shape.** All rings in a stack stay parallel, so the
  loop is built neutral and articulated by the hinge bones.

---

## 6. The reel and the site

### Adding a subject to `zhao_reel.cpp`. VERIFIED in this lane.

There is **no registry table**. `main` holds a long chain of guarded calls
(around lines 6120–6210):
```cpp
if (wanted("zixxtrixx-idle")) rc |= render_scene(subject_zixx_idle());
```
and each `subject_*()` is a free function returning a `SceneSubject` by value
(`struct SceneSubject` at line 970, ~124 defaulted fields) carrying name, frames,
step, camera (`cam_k/cam_eye/cam_dist/cam_bias`), sky variant, island flags,
material RGB, `full_colour`, `expect_seq_crc`, `note`, and the light/sun rig.
Names are typed into **three unsynchronised lists** — the `wanted()` dispatch
(authoritative), the `kLibrary[]` array used only by `--list`, and the `--check`
CRC set. Nothing checks that they agree.

`SceneSubject::creature` is **the clip slot plus 2** (`zhao_reel.cpp:3016`:
`dog_inst.anim.cut(static_cast<uint16_t>(sub.creature - 2))`).

**To add creature 2:**
1. `#include "unnamed02.h"` at `zhao_reel.cpp:1430`, beside the Zixxtrixx
   include — it must sit **after** `namespace zc = zref::creature;` (line 1426).
2. Add `subject_u02_*()` functions — copy the *shape* of `subject_zixx_idle()`
   and `zixx_common()`, not their values.
3. Add one `if (wanted("unnamed02-<clip>")) …` line per subject.
4. Set `full_colour` on every creature subject (the 256-colour rule is the GIF
   lane only; without it a >256-colour sequence writes **no frames** and returns 3).
5. Pin `expect_seq_crc` after the first accepted render so `--check` guards it.

The mandatory interface is exactly one symbol: `unnamed02::type()` returning
`const zc::CreatureType&`. Everything else the reel calls into `zixxtrixx.h` for
(~65 symbols: `station_x`, `station_bind`, the attack/jump planner API, the
`kSlot*` ids, the `k*Keys` counts) is authoring convenience, not contract.

### TWO WALLS a second creature hits. VERIFIED, `zhao_reel.cpp`.

**Wall 1 — the creature selector is BINARY.** Line 3000–3001:
```cpp
const bool zixx_subject = sub.creature >= 3;
dog = zixx_subject ? &zixx::type() : &watchdog_type();
```
`zixx_subject` then also drives `tilt_mode` (3007), `facing` (3013), the slot
mapping (3016), and `dog_inst.x = fxm(zixx::kStageCentreMm)` (3022), with
hard-coded special cases at `sub.creature == 4` (3023) and `== 29` (3025). **A
third creature is not a pure add** — this expression must be widened to a species
selector (add a `SceneSubject::species` field and switch on it, leaving
`creature` as the slot). That is the single real code change, and it is small.

**Wall 2 — there is ONE pre-resolve hook slot, so a creature cannot currently
also carry a sun/flare/bloom.** `rend.set_pre_resolve(...)` is called in exactly
three mutually exclusive places: `cel_hook` (2951, celestial/star/flare/corona),
`planet_sky_hook` (2972), and `creature_hook` (3047). **The owner explicitly
wants the sun effects and the skybox bloom ON this creature.** So either the
hooks must be composed (a small chained-hook wrapper) or the creature hook must
call the celestial path itself. **Decide this before authoring the effects** —
it is the one architectural item on the critical path, and the current
`ZixxSunSpec` point-light suns are *not* the same thing as the corona/bloom.

### Particles, lightning and suns — what already exists. VERIFIED.

* **A working per-frame particle emitter is already in the reel**: `zref::render::Population debris_pop` registered as resource handle 3, cleared and refilled every frame from integer ballistics (`y = vy·t − g·t(t−1)/2`, despawn below a floor line), from a `Debris{x0,y0,z0,vx,vy,vz,size,r,g,b}` table on the subject. **That is the template for the ten mana kinds** — ten emitter tables with different colour, size, spawn geometry and integer motion law, all feeding one `Population`.
* **Suns**: each Zixxtrixx clip declares one `ZixxSunSpec` (`kZixxSun<Clip>`), 50 m up, 22 m aside, inner radius 65 m so the whole body sits at attenuation 1. Direction 30's calm values (adds ~24%, mults ~30% of Direction 29) are the tuned baseline; the ceiling lesson — *"bringing a source under the ceiling RESTORES its hue"* — applies to any new creature's suns.
* **The one that blooms over the skybox** is `atmo-sun-donor` / `atmo-sun-thick`: *"halo_atmo corona at 4× the disc radius, no lens chain"*, composited **additively** (`dst = sat(dst+src)`) over a fully flat sky with a silhouette-material island. `subject_atmosunthick` changes exactly one number — a per-channel transmission `(1.0, 0.60, 0.25)` on the ramp control points.
* **Lightning**: VERIFIED absent — no hit for `lightning|bolt|spark` anywhere in the reel. Note also that `zref_particle.hpp` / `zref_particle_soft.hpp` exist but are **not included** by `zhao_reel.cpp`; the reel rolls its own integer ballistics (`Debris`, `ReelGibPiece`, `spawn_reel_gibs`, `advance_reel_gibs`). INFERRED cheapest route for lightning — a `Population` of tri-sprites along an integer-stepped polyline between the antenna hinges, recomputed per frame from a deterministic hash of the frame index. No new engine code; one sprite per segment.
* **Full effect shelf already built**: `star-s00..s11` (12 classes with corona + lens-flare chains), `noctis-flare`, `flare-occlusion`, `sky-sweep`, `star-boil`, `planet-sun-*` ×10, `atmo-sun-*` ×2.

### Building and rendering. VERIFIED.

**Never `cmake --build`** (the stale-binary trap). Use:
```
zhaozhou/tools/reel/build-direct.sh --output <lane-local dir> [reel|cel|meshcheck|probe|all]
```
g++ `-O2 -std=c++17`, compiles ~30 `reference/src` objects in parallel, links
`zhao_reel.cpp` → `<dir>/bin/zhao-reel.exe`; the `cel` target additionally defines
`ZIXX_PAGE_VARIANT` to `zixxtrixx_page_cel.h`. Note: **`build_cel` hard-requires
that Zixxtrixx header** — creature 2 needs its own target or its page compiled in
directly. Frames land as `<subject>/%04d.rgb`; `website/tools/tovideo.py` encodes
to VP9 crf 16 yuv444p 60 fps plus a 3× nearest-neighbour PNG poster.

The end-to-end recipe is captured in
`Upheaval/creature/Zixxtrixx/AGENT-PACK/V14-WORKBENCH.json` (build script/target/
binary/output, render subject + env, 384×240×600 @60, then encoder → creatures.json
→ assemble → deploy), driven by `CREATURE-AUTHORING-BLUEPRINT/tools/workbench.py`.
**Copy its path contract for creature 2; learn its shape, not its values.**

The "22-subject bank" is a **convention, not a data structure**: the 22 published
Zixxtrixx clips (`zixxtrixx-{idle,walk,attack,fall,hit,look,run,damage,death,
death2,balance,knockdown,hitfloor,taunt,slow-taunt,jump-one,jump-multi,
salto-{dummy,fly,six,nine},moving-light}`), rendered as a list per generation.
VERIFIED against `qa-bank/`.

### Putting the new creature at the TOP of the site. VERIFIED.

`website/creatures.json` is an **object** with `_comment`, `site`, and
`creatures` — an array currently holding **exactly one** element (`zixxtrixx`,
lines 32–2638). Zixxtrixx's 19 archive generations are **not** separate entries;
they are `renders` entries inside that one object flagged `"archive": true` with
an `"archive_generation"` string, bucketed at assemble time and ordered by the
creature-level `archive_generation_order` array.

Ordering is **pure array order** — `assemble.py:295-303`:
```python
creatures = data.get("creatures", [])
body += [card(c, public) for c in creatures]
```
No sort, no `order` field, no date parsing anywhere. `.grid` is
`flex-direction: column`, so array order is top-to-bottom.

**The exact change: insert one object as `creatures[0]`, before the `{` at line 32.
Zixxtrixx becomes `creatures[1]`, byte-untouched, archives travelling with it.
`assemble.py` needs NO code change.** Then `python tools/assemble.py .`.

```json
{ "id": "unnamed02", "name": "…", "kind": "…", "status": "production",
  "blurb": "…",
  "renders": [ { "group": "…", "src": "renders/unnamed02-idle.webm",
                 "label": "Idle", "caption": "…", "note": "…", "alt": "…" } ] }
```
Only `id` is strictly required (`card()` reads `c["id"]` unguarded); an empty
`renders` renders a "No renders yet" stub. `id` must be unique — it becomes the
radio-group name `tab-{id}`, and a collision silently cross-links two cards.

Media rules: paths are relative to `public/`; **`renders/x.webm` requires
`renders/x.png` beside it** (the poster is derived by string surgery,
`assemble.py:164`). `assemble.py` hard-fails on a declared `src` missing from disk
— **but never validates the derived poster**, so a webm without its PNG deploys a
404 poster silently. `MAX_TABS = 40` and `MAX_ARCHIVE_GENERATIONS = 19` are **per
card**, so a new creature starts at zero — but Zixxtrixx sits at exactly 19/19,
and a 20th generation for it would hard-fail and require moving `assemble.py` and
all three `nth-of-type`/`nth-child` families in `style.css` together.

Deploy: `website/deploy.ps1 -Project upheaval -Branch main` — both args mandatory;
it re-assembles, refuses unless `index.html` carries `name="robots" content="noindex`,
then `wrangler pages deploy public/`. **Conflict to resolve with the coordinator:**
root `CLAUDE.md` gives the bestiary **standing authorisation** to publish on every
finished creature pass, while `creature/00-START-HERE.md` §5 says *"Do not run
`deploy.ps1`. The coordinator verifies and publishes."* Ask rather than assume —
it is an outward-facing action.

---

## 7. Budgets — honest, per instance

Hard limits (VERIFIED, `zref_creature.hpp` + `spec/creature_rules.md` §1.2, §2.1):

| | |
| --- | --- |
| bones per creature | **≤ 32** (hard) |
| influences per vertex | **≤ 2**, structural (`SkinVertex{b0,b1,w0}`, `w1 = 64 − w0`); quanta 1/64, sum forced to exactly 64 |
| meshlet | **≤ 64 unique verts, ≤ 96–126 tris, one material** |
| ring `segments` | **3 … 32** (compile-enforced) |
| clip slots / frame | **64** slots; `12 + 8·bone_count` B/frame (≤ 268 B at 32 bones); ≤ 4 events/frame |
| authored rate | 30 Hz keys held 2 sim ticks; 60 Hz presentation midpoints optional |
| creature extent | every posed vertex within **128 m** of root (`kCreatureLocalRadius`) |
| pools | decoded-pose cache ~128 tuples ≈ 192 KiB, **shared across instances**; whole meshlet+LOD+anim pool 24 MB; clip bank ≈ 343 KB/type |
| donor reference | 1,600–10,500 verts per creature, 11–32 bones |

**Proposed per-instance budget for Unnamed02** (INFERRED from the four-ball form;
sized with `side = (rings−1)·2·segments`, `cap = segments`):

| part | build | tris | verts |
| --- | --- | --- | --- |
| body ball | 11 rings × 16 seg, both caps, segments tapering to 6 at poles | ~350 | ~180 |
| 3 hinge balls | 7 rings × 10 seg each, both caps | ~420 | ~210 |
| antenna loop | chain part, 24 rings × 8 seg, elliptical `rx/rz` | ~370 | ~190 |
| 2 eye lenses | 5 rings × 8 seg, faceted on purpose | ~160 | ~90 |
| **total** | | **~1,300** | **~670** |

That is **~11–14 meshlets** after ring-boundary splits (bounded by both the 126-tri
and 64-vert ceilings) — **five to eight times under the donor's own low end**, and
it should be spent up rather than down if the creature does not read at 384×240.

* **Bones: 8–10** — root + 3 hinges + 2 eye/pupil + 1–2 gaze/attitude. Against 32,
  ample. `kMaxBones` is not the constraint; legibility is.
* **Texture: two pages.** One body atlas in **RGB565 direct colour** — because
  *"nearest sampling mandatory — bilinear must never touch a palette"*, filtering
  **requires** direct colour and CLUT8 would lock this surface to pixelated
  forever — plus a **separate eye page**, since bilinear and mips bleed across
  atlas neighbours and unrelated regions must not share one atlas.
* **Deformation sidecar:** one `DeformVertex` per body-ball vertex (~180) and one
  `DeformSample` per key. Never enters the PoseBank.

**The "several on screen" answer, honestly.** Geometry is the cheap part:
identical instances on the same clip at the same tick **share one decoded pose**,
so N instances cost N× rasterisation and ~1× animation. The expense is **variety
and particles** — ten particle kinds × N instances is N× sprite fill, and sprites
are depth-tested but never depth-written, so they always pay it. **Budget
particles per SCENE, not per creature**: cap the live population, share emitter
tables across instances, phase-offset rather than duplicate. One sun serving all
instances is the cheap and correct read.

---

## 8. Verified vs inferred

**VERIFIED** (read in this lane): every path, line number and quotation above —
the scaffolder's 15 templates and its non-empty-destination refusal; the absence
of `Upheaval/tools/creature/` and `Zixxtrixx/source/`; `compile_creature`'s
signature; `zixxtrixx.h` = 8,514 lines using `zc::` throughout; `build_ring_part`
as a library and no sphere generator anywhere; the deform sidecar; the
`if (wanted(...))` dispatch, the binary `sub.creature >= 3` selector and the
three exclusive `set_pre_resolve` sites; `build-direct.sh`; `debris_pop`; the
sun/star/flare inventory; `creatures.json` shape and `assemble.py`'s array-order
emission; `deploy.ps1`; the 22 `qa-bank` names; every limit in §7.

**INFERRED**: the triangle/vertex/meshlet estimates in §7 (arithmetic from the
stated law — no sphere has been built here); the lightning approach; that a
`git mv` is most of the later migration cost (true for the model; the reel
subjects and page generation would still need the `zreel` seam); that splitting
into five headers now is free.

**RESOLVED during recon — the texture page path.** Zixxtrixx's pages are
**generated C++ headers** (`zixxtrixx_page.h` and `zixxtrixx_page_cel.h`, ~18k
lines each, byte-identical in structure and differing only in pigment; plus a
3.6k-line debug page). All three export the **same symbol set** and are swapped
by `-D`, not by code (`zixxtrixx.h:49-62`: `ZIXX_DEBUG_PAGE` / `ZIXX_PAGE_VARIANT`
/ default). Each carries **both** formats: CLUT8 (`kPagePalette[256]` +
`kPageTexels[6][4096]`, assembled by `zixx::page()`) and direct RGB565
(`kPageAtlas`, one 256×512 body atlas, + `kPageDirect[6]` mip chains, assembled by
`zixx::page_direct()` into a `zref::DirectPageSet` with per-tile `Tmu::Mode`,
`wrap_u = kRepeat`, `wrap_v = kClamp`). Tiles are a plain enum selected by
`RingPart::page`, with `v0`/`v1` atlas row ranges per part.
**So creature 2 needs its own page-header generator** — the blueprint's
`texture_recipe.py` emits a raw blob and stops short. Either extend it to emit a
header in the same shape, or fork `tools/pack/mkcreaturepage.py` (which is a
Zixxtrixx pigment recipe wearing a generic name). Note `build-direct.sh`'s `cel`
target **hard-requires `zixxtrixx_page_cel.h`** and will need its own target.

**STILL NOT ESTABLISHED:** whether the two pre-resolve hooks can be composed
cheaply (Wall 2, §6) — worth a 30-minute spike before promising sun effects.

## 9. Risks, in order

1. **The direction file is moving.** It grew 18 lines mid-recon. Re-read
   `OWNER-DIRECTION-1-2026-09-04.md` and check `Unnamed02/reports/` before the
   first edit, every pass.
2. **The scaffolder will fail on `creature/Unnamed02`.** Scaffold to a sibling and
   merge; keep the hand-written README and owner direction.
3. **`01-RING-CONSTRUCTION.md`'s struct listing is stale.** Read the header.
4. **The single pre-resolve hook slot (Wall 2) blocks "creature + sun bloom".**
   The highest-value spike available. Do it before promising the effects.
5. **The binary `sub.creature >= 3` species selector (Wall 1) must be widened.**
   Small, but it is the one edit that touches Zixxtrixx's own code path — so
   re-render the 22-subject bank and check CRCs after it.
6. **`build_cel` hard-requires `zixxtrixx_page_cel.h`**, and the blueprint's
   texture recipe stops at a raw blob. Creature 2 needs a page-header generator
   and its own build target.
7. **Do not copy `zixxtrixx.h`.** Copy the maths helpers and the `type()` shape;
   nothing else.
8. **Publishing is outward-facing and the two rule sources disagree.** Confirm
   with the coordinator before running `deploy.ps1`.
9. **Ten particle kinds is the cost risk, not the mesh.** State each effect's cost
   as the direction demands, and cap the scene population.
10. **Line numbers drift between checkouts.** Everything cited here was
    re-verified in *this* lane; if a number is off by a few hundred, grep the
    symbol rather than trusting the offset.

---

*Recon only. No file in either repository was modified, and this document has not
been staged, committed or pushed.*
