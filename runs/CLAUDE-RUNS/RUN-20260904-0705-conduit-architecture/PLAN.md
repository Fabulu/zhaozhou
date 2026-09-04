# PLAN — Unnamed02, the mana conduit (architecture pass)

**Run:** RUN-20260904-0705-conduit-architecture · **Date:** 2026-09-04
**Status:** ARCHITECTURE ONLY. Nothing here is built, rendered or published.
**Binding direction:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-1-2026-09-04.md`
(re-read it before every pass — it grew twice in one day; also check
`Upheaval/creature/Unnamed02/reports/`, which does not exist yet but may by the
time you start).
**Inputs inherited, not re-derived:** `RECON-FX-FINDINGS.md` and
`RECON-NEWCREATURE-FINDINGS.md` (both in `zhaozhou/runs/CLAUDE-RUNS/`),
`reports/DoubleHelixTornado.md`, `Upheaval/creature/{00,05,07,08}-*.md`,
`CREATURE-AUTHORING-BLUEPRINT/`.

Paths are relative to the lane root
`C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\`.

---

## 0. Headline rulings

Where the recons left a choice, this plan rules. Every ruling is one edit to
reverse.

1. **Additive blending IS wired into the particle draw path.** A new flag bit on
   `draw_population` selects saturating ADD per the existing `RASTER.FRAGMENT`
   law; flag off is byte-identical to today. Opaque flecks cannot read as
   *glowing mana* over pink pigment at 384×240 — the FX recon calls this the
   single highest-value change in its document, and it is the difference between
   mana and confetti. (§6.1, Spike S3.)
2. **The pre-resolve hook becomes a chained sequence** — sky → celestial →
   creature — behind a tiny wrapper; identity when only one hook is installed.
   This is Spike S1 and it happens **before anything else is promised**. On
   fail, the fallback is composition *inside* `creature_hook` for u02 subjects
   only. (§2.)
3. **Sun effects are SCENE-level, never per-instance.** One `planet-sun-*`
   skybox bloom serves every conduit in view (two suns cost ~2× the splat and
   ZERO extra palette — the cheapest bloom in the machine). The per-conduit
   centre glow is **not a sun and not a flare**: it is a baked radial additive
   sprite on the corona draw primitive with ONE shared CLUT ramp, so it never
   touches the ≤2-suns / ≤2-flares caps. This dissolves the "caps vs several on
   screen" collision. (§6.3.)
4. **Lightning is a bead-chain**: triangle sprites from the existing
   `Population` tri path laid along a deterministic jittered polyline between
   the hinge balls and the body centre, recomputed per frame from a hash of the
   frame index. No `FORGE.PRIM`, no god beams, no live line drawing — all three
   are unbuilt or refused. (§6.2.)
5. **Ten kinds of particle = ten named EMITTER TABLES** — each a behaviour
   (spawn geometry + integer motion law + representation + colour + size law),
   not ten colour swatches and not ten engine features. All feed one
   `Population` per scene with a scene-level cap. (§6.1.)
6. **Code goes beside Zixxtrixx in `zhaozhou/tools/reel/`, split into small
   headers named for the migration's target layout** (recon ruling, inherited).
   One addition to the recon's file list: `unnamed02_fx.h`, so the effects
   authoring migrates with the creature. (§3.)
7. **The eyes are real faceted geometry** (the direction says so; Zixxtrixx's
   paint-the-disc lesson is scoped to a smooth tube and does not bind here),
   with per-eye lens bones, per-eye pupil-star bones, and a deform-sidecar
   squint. Expressiveness = gaze + aim + squint + dilation-by-spread, all
   deterministic per-clip curves. (§4.3.)
8. **The compression is the deform sidecar** (`kRadial` flatten/spread on the
   body ball; `kFollower` on everything that rides it), free in the
   army-sharing economy because it never enters the PoseBank. One knob,
   `kCompressAmpPm`, drives both the sidecar curve and the sympathetic
   hinge-root bob so the antenna never detaches from a squashing body. (§5.2.)
9. **The probe asserts CLEARANCE, not a penetration window.** This creature
   never touches the ground; the contract says so explicitly
   (`min_clearance_mm > 0` for every posed vertex of every frame of every
   clip). Zixxtrixx's ground-contact law is declared not applicable in
   CREATURE.json. (§8.4.)
10. **Publish at the end of the finished pass without asking.** Root
    `CLAUDE.md`'s standing bestiary authorisation plus the direction's own
    words ("Once done, upload it") outrank `00-START-HERE.md` §5's older
    "coordinator publishes" line. `deploy.ps1 -Project upheaval -Branch main`,
    noindex intact, new creature at `creatures[0]`. (§8.6.)

---

## 1. What is being built, in one paragraph

A floating four-ball companion: one big pink teardrop body carrying two
bulging, faceted, purple-lens/cyan-star eyes, and a flat blade-like antenna
loop rising from it, articulated around three small hinge balls. It hovers
(the hover IS the idle), drifts, channels mana, reacts, and rests — no
attacks, no gait. Ten named kinds of mana particle in varied colours stream
around and through it, a bead-chain bolt crackles between its hinges and its
centre during channelling, a baked additive glow blazes at its core, and the
scene's `planet-sun-*` bloom paints the sky behind it. Budgeted so three or
four can share a frame. Published at the top of the bestiary with a video per
clip.

---

## 2. The spike list, ordered

Spikes run FIRST, in this order, each timeboxed to roughly half a session.
Log each verdict in the run's TASK_LOG before moving on.

### S1 — Chain the pre-resolve hooks  *(the gate for the whole effects promise)*

* **Do:** wrap `SoftwareRenderer::set_pre_resolve` usage in the reel with a
  small chained dispatcher (a fixed array of up to 3 `(fn, ctx)` pairs invoked
  in order sky → celestial → creature; the reel registers into slots instead of
  overwriting). Build a throwaway test subject: watchdog-or-zixx creature +
  `planet-sun-*` bloom + one corona, one render each of (bloom only),
  (creature only), (both chained).
* **Pass question:** does the chained render composite correctly — bloom only
  where `depth == 0`, creature over it, corona depth-tested — AND does
  `zhao-reel --check` still print "all sequence CRCs match" for the existing
  bank (each existing subject still installs exactly one hook, so chaining must
  be identity for them)? Redirect `--check` output to a FILE.
* **On fail:** do not fight the renderer seam. Fallback: `creature_hook` for
  u02 subjects calls the planet-sky splat and the star compose functions
  itself, in that order, before the creature compose — composition by explicit
  call rather than by generic chaining. Uglier, contained, certain to work
  (the hooks are ordinary functions over the same `(rgb, depth)` buffers).
* **If even the fallback misbehaves** (it should not): the skybox bloom ships
  only on dedicated showcase subjects and the plan's §6.3 scene-sun claim is
  cut back to "corona sprite centre glow only". Say so to the owner in the
  publish note.

### S2 — Widen the binary species selector  *(the gate for rendering u02 at all)*

* **Do:** replace `const bool zixx_subject = sub.creature >= 3;`
  (`zhao_reel.cpp:3000`, five downstream decisions, special cases at
  `creature == 4` and `== 29`) with an explicit `SceneSubject::species` enum
  (`kWatchdog`, `kZixxtrixx`, `kUnnamed02`), leaving `creature` as slot+2.
* **Pass question:** after the refactor, all 22 Zixxtrixx bank subjects render
  with unchanged sequence CRCs (`--check` to a file), and the watchdog subject
  still selects correctly.
* **On fail:** revert, re-land as a pure-additive guard
  (`species == kUnnamed02` checked before the old binary expression) so
  Zixxtrixx's path is untouched byte-for-byte; file the debt in TASK_LOG.

### S3 — Additive blend on the particle path  *(the gate for glowing mana)*

* **Do:** add a flag bit (b2 = additive) to `draw_population` /
  `blit_pattern_block` and the tri branch: `dst = sat(dst + src)` per channel
  after the same depth compare, never writing depth. Semantics follow
  `RASTER.FRAGMENT`'s ADD so the software change is a faithful preview of the
  hardware law, not an invention.
* **Pass question:** (a) a cyan 6 px additive mote over the pink body reads as
  GLOW at native 384×240 (look, don't measure); (b) with b2 clear, all
  existing debris/population renders are CRC-identical.
* **On fail** (e.g. the change is ruled out of bounds for the reference lane):
  ten kinds ship as bright saturated opaque cores, each sitting on a small
  baked additive halo sprite drawn through the corona primitive (S5's
  mechanism). More draws, same read at 240p. State the substitution in the
  SPEC.

### S4 — `make_ball()` through the ring builder  *(the gate for the whole form)*

* **Do:** write the ~10-line UV-sphere `RingSpec` generator in
  `unnamed02_model.h` (recon §5 sketch: `theta` sweep, `fx_sin/fx_cos`,
  segments tapering to 6 at the poles), compile one grey ball through
  `compile_creature`, render it, run meshcheck.
* **Pass question:** watertight (meshcheck clean), smooth-shaded with no seam
  (position-keyed normals should guarantee it), ~350 tris at 11×16, and it
  reads as a BALL at 240p from the three diagnostic cameras.
* **On fail:** unlikely; fall back to hand-tabulated radius rows in the
  `kNoseDome` style. The budget survives either way.

### S5 — Shared-ramp centre glow  *(the gate for "big effect at its centre" × N instances)*

* **Do:** bake ONE radial mana-glow CLUT8 sprite (the `halo_atmo` bake path
  aimed at a world position instead of a sun, per FX recon §8 row 7), draw it
  additively at the projected body-centre of 3 test instances with one shared
  64-entry ramp built once per frame.
* **Pass question:** three glows on screen with exactly one ARM ramp rebuild
  per frame (not per instance), no interaction with the ≤2 sun/flare caps, and
  the glow reads as emanating from *inside* the body (drawn before the
  creature compose in the chained order, so the body occludes its centre —
  that is the correct read for a thing whose light is in its belly; if the
  owner wants the glow OVER the body, flip it to after and look again).
* **On fail:** shrink radius until fill arithmetic fits, or replace with a
  dense additive particle cluster (kind #1 at high rate, zero new machinery).

Everything after S5 is authoring, not uncertainty — the render-look-adjust
loop, which is the job, not a spike.

---

## 3. The build path

### 3.1 Files

New, in `zhaozhou/tools/reel/` — none touch Zixxtrixx; names match the
ownership migration's target layout so the later move is a `git mv`:

| file | holds |
| --- | --- |
| `unnamed02_art.h` | EVERY named knob: form, palette, timing, deform, hover, effects, light, LOD. One findable block, Zixxtrixx-style. |
| `unnamed02_model.h` | `make_ball()`, the teardrop body builder, the loop chain builder, the eye lens + pupil-star builders → `vector<RingPart>` |
| `unnamed02_rig.h` | `Skeleton` + bone id constants (§4.4) |
| `unnamed02_clips.h` | deterministic clip builders (§5), the deform sidecar authoring, the shared `Rig`-style quat accumulator (mirror Zixx's `struct Rig`), the maths helpers copied verbatim from `zixxtrixx.h:1222-1337` (pure quat/curve maths — the one sanctioned copy) |
| `unnamed02_fx.h` | the ten emitter tables, the bolt polyline builder, the centre-glow spec, the per-clip sun spec (`U02SunSpec`, mirroring `ZixxSunSpec`) |
| `unnamed02.h` | composition + `unnamed02::type()` — the only header `zhao_reel.cpp` includes |
| `unnamed02_page.h` | GENERATED texture page header — gitignored, never tracked |
| `u02_probe.cpp` | the hover-clearance probe (blueprint template adapted; §8.4) |

New, in `zhaozhou/tools/pack/`:

| file | holds |
| --- | --- |
| `mku02page.py` | page generator forked from `mkcreaturepage.py`'s STRUCTURE (not its Zixxtrixx pigment recipe): emits `unnamed02_page.h` with the same symbol shape — `kPageAtlas` RGB565 + mip chains + per-tile `Tmu::Mode`, body tiles and a SEPARATE eye tile (bilinear bleeds across atlas neighbours; unrelated regions must not share). `zlib.crc32`, never `hash()`; verify two regens `cmp`-identical. |

Edited, in `zhaozhou/tools/reel/`:

* `zhao_reel.cpp`: the S1 chained-hook wrapper; the S2 species enum;
  `#include "unnamed02.h"` after `namespace zc = zref::creature;`; one
  `subject_u02_*()` per clip plus the diagnostic subjects (§8), each with
  `full_colour = true` (without it a >256-colour render writes NO frames and
  returns 3); dispatch lines in all three lists (`wanted()`, `kLibrary[]`,
  `--check` — they are unsynchronised, update all three); the S3 additive flag
  plumbing if it lands reel-side.
* `build-direct.sh`: a `u02` target (the `cel` target hard-requires
  `zixxtrixx_page_cel.h`; creature 2 gets its own).

Edited, in `zhaozhou/reference/` (S3 only):

* `src/zrender/sprites.cpp` — the additive branch in `blit_pattern_block` and
  the tri path. Smallest possible diff; flag-off CRC-identical.

New, in `Upheaval/creature/Unnamed02/` — the durable record. Scaffold with
`new_creature.py --id unnamed02 --output creature/Unnamed02Scaffold` (it
REFUSES the non-empty real folder), then move in only what does not collide;
keep the hand-written README and owner direction:

* `SPEC.md` (this plan distilled + the cost statement), `CREATURE.json`
  (identity, pinned zhaozhou commit, **ground-contact section explicitly
  declaring "never contacts — clearance contract"**), `source/` (a pointer
  note until the migration lands; record `"copied_or_adapted"` honestly),
  `texture/` (source art + recipe), `probes/` (the clearance policy table),
  `validation/`, `media/site-entry.json`.
* Delete `Unnamed02Scaffold/` after the merge.

### 3.2 Order of work

1. **Spikes S1–S5** (§2). Commit each verdict as it lands.
2. Scaffold + merge the Upheaval package; write SPEC.md and CREATURE.json.
3. **Form:** `unnamed02_model.h` + `_rig.h` + a minimal `type()` with a 1-frame
   rest clip; grey renders from side/front/three-quarter fixed cameras; LOOK
   against both sheets (overlay silhouette vs `Concept/Side.png` — comparison
   side only). Iterate by eye.
4. **Page:** `mku02page.py` + source art; textured renders; the crayon grain
   must exceed the light rig's own range or it is invisible (the ±16% failure).
5. **Motion:** clips in §5 order, idle first. Instruments after each: contact
   sheet of every frame, band checks (§8.2).
6. **Effects:** emitter tables one kind at a time, then the bolt, then the
   centre glow, then the scene sun + bloom wiring. Look at each solo against a
   dark plate first (the lighting doc's rule), then in scene.
7. **Probe + gates:** `u02_probe` clearance policy authored from the ACCEPTED
   motion (bands derived from authored motion, not vice versa); pin
   `expect_seq_crc` per subject; re-verify the Zixxtrixx bank CRCs one last
   time.
8. **Site + publish** (§8.6).

Commit and push at every numbered step, by explicit path, never `git add -A`.
`unnamed02_page.h` and raw `.rgb` frames never enter history (the pre-commit
hook enforces; do not fight it).

---

## 4. The form

Authored BY EYE against the sheets; every number below is a starting
orientation and a named knob, not a measurement. The sheet is a gate, not a
reference. Never sample pigment from the scans.

### 4.1 The four balls

* **Body ball** — `make_ball()` output reshaped into the teardrop: full and
  round in its lower ~60%, upper rings pulled inward and upward into the neck
  the loop grows from. One `RingPart`, ~11 rings × 16 segments tapering to 6
  at the poles, both caps, ~350 tris. Rings spaced by equal height (the V-lane
  law: V maps linearly in ring index — equal spacing or `v0/v1` compensation,
  else the texture stretches at the poles).
* **Three hinge balls** — rigid `RingPart`s, 7 rings × 10 segments each
  (~140 tris each), one per hinge bone, each bone's bind translation AT that
  ball's own centre (the pivot-offset lesson: a pivot away from the mass makes
  the part orbit a distant point). Placement from the side sheet, by eye: one
  at the loop's lower-front corner, one at the top peak, one at the upper-rear
  corner.
* **Eye lenses** — §4.3.

### 4.2 The flat antenna loop

* **ONE `chain` part**, rings in creature-global bind space, walking the loop
  path: out of the body at the front neck → hinge A (lower-front) → hinge B
  (peak) → hinge C (upper-rear) → back into the body at the rear neck. Both
  ends plunge INSIDE the body ball (hidden ends are cheaper and safer than
  welded seams).
* **Flat, not a hoop:** per-ring elliptical `rx`/`rz` — broad in the loop's
  plane (side view), narrow across it (front view reads blade-like, per the
  front sheet where the whole antenna is nearly a spike with bumps).
* ~24 rings × 8 segments ≈ 370 tris.
* **Skinning:** per-ring `{b0, b1, w0}` blending across each hinge bone —
  root→A across the first arc, A→B, B→C, C→root. This is exactly what the
  chain primitive exists for (it closed Zixxtrixx's 61 mm hole).
* **Bind straight, pose the shape:** the loop is built in its neutral drawn
  shape; articulation is hinge-bone rotation in clips, never baked bend.

### 4.3 The eyes — the whole face

* **Two lens parts**, close together on the front, angled outward in a V (front
  sheet), each on its own eye bone. Each lens: 5 rings × 8 segments, flattened
  along its outward normal (`rz` small) — a purple almond ~160 tris for the
  pair. Set PROUD of the body surface: the oblique detail sketch on the
  description sheet shows the lens standing off the silhouette from behind —
  that clearance is the bulge read and it is a knob (`kEyeBulgeMm`).
* **Partly polygonal:** the facet read comes from THREE sources, because
  generated normals are always smooth (position-keyed, area-weighted — flat
  shading is not available by vertex duplication): (1) the 8-segment silhouette
  visibly facets at 240p; (2) the cel quantiser bands the shading into facets;
  (3) the eye page paints hard-edged lens panels (purple field, white inner
  rim). Author, render, look — if it reads as a smooth dome, DROP segments to
  6 before adding any machinery.
* **The pupil is tiny geometry, Zixxtrixx-style:** a four-pointed cyan star
  (two crossed quads or an 8-tri fan, ~10 tris) riding a per-eye pupil bone
  that translates within the lens. The star shape echoes in particle kind #8.
* **Expressiveness — the vocabulary, all deterministic per-clip curves:**
  * **Gaze** — pupil-bone translation curves (mirror Zixxtrixx's
    glance/settle/hold schedule; this is the precedent to BEAT, so give every
    clip an authored gaze story, not a static stare).
  * **Aim** — eye-bone rotation: the lenses themselves swivel a few degrees
    (Zixxtrixx could not do this; its eye was paint).
  * **Squint/blink** — deform sidecar role on the lens vertices: flatten along
    the lens normal sinks the bulge toward the body = squint; near-full
    flatten = blink. Costs zero bones.
  * **Dilate** — spread on the pupil-star's deform role widens the star =
    alarm/delight.
* **Neither nose nor mouth exists.** Do not invent one.

### 4.4 The rig (9 bones of the 32 allowed)

| id | bone | parent | carries |
| --- | --- | --- | --- |
| 0 | `kBRoot` | — | body ball; hover translation + body attitude |
| 1 | `kBHingeA` | 0 | lower-front hinge ball + loop arc root→A→B |
| 2 | `kBHingeB` | 1 | peak hinge ball + loop arc A→B→C |
| 3 | `kBHingeC` | 2 | upper-rear hinge ball + loop arc B→C→root |
| 4 | `kBEyeL` | 0 | left lens |
| 5 | `kBEyeR` | 0 | right lens |
| 6 | `kBPupilL` | 4 | left star |
| 7 | `kBPupilR` | 5 | right star |
| 8 | `kBAttitude` (optional) | 0 | separates body tilt from hover root if clips want independent channels; cut it if the root alone reads fine |

Parent-before-child holds; rest rotations identity; bind is a pure translation
chain.

### 4.5 Deformation

* Body-ball vertices: `deform_role = kRadial`, vertical axis, centre at the
  ball centre, strength graded to peak at the equator.
* Lenses, pupils, hinge balls, loop: `kFollower` — they translate with the
  carrier's contraction, keeping their own dimensions rigid (the eyes ride the
  squash uncrushed; that is the mechanism's stated purpose).
* **Caveat to watch on the first squash render:** deform never enters the
  PoseBank, so hinge-bone PIVOTS do not move when follower geometry translates.
  At the slight amplitude directed this should be invisible; if the loop reads
  detached during compression, author a sympathetic hinge-root bob in the clip
  builders from the SAME `kCompressAmpPm` knob so amplitude stays one number.
* One `DeformSample{flatten, spread}` per key per clip; `{0,0}` is exact
  identity; the positive-volume gate forbids full flatten.

### 4.6 Budget (inherited from the recon, arithmetic not measurement)

~1,300 tris / ~670 verts / 11–14 meshlets / 9 bones / one RGB565 body page +
one separate eye tile. Five-to-eight times under the donor floor — if the
creature does not read at 240p, spend UP, not down.

---

## 5. The motion

### 5.1 The clip list — six core, one stretch, no attacks, no gait

All clips: 30 Hz keys held 2 ticks, hard-cut transitions, authored half-keys
from the schedule (never chord-interpolated), monotone tangents at reversing
knots, life layer never off.

| clip | slot | for | length target | what happens, mechanically |
| --- | --- | --- | --- | --- |
| `hover-idle` | 0 | the baseline; the hover IS the idle | 600 frames loop | Root Y = `kHoverHeightMm` + two incommensurate bob sines (periods ≈46 and ≈102 frames, amplitude `kBobAmpMm`). Compression wave runs continuously (§5.2). Antenna sways: hinge rotations lag the body bob by `kAntennaLagKeys` and settle. Gaze wanders on the idle glance schedule. |
| `drift` | 1 | locomotion-analogue: it follows the player | 300 frames loop | Body leans into travel (root pitch a few degrees), antenna trails behind by lag, bob continues shallower and faster. Root XZ translation loops seamlessly. No footfalls — declare zero events. |
| `channel` | 2 | the conduit at work — the showcase | 420 frames | Three beats: draw-in (drain streamers converge, compression deepens and slows — the body "inhales", ≥112 frames), blaze (bolt beads arc hinge↔centre, centre glow peaks, star pupils dilate, ≥150 frames), release (a ring kind pulses outward, body relaxes, ≥100 frames). One thing at a time, each long enough to read. |
| `react-curious` | 3 | reaction: something caught its eye | 180 frames | Gaze snaps to an off-screen point FIRST (eyes lead), then the body yaws after with lag, antenna perks (hinges rotate the loop upright), pupils dilate. Settle back. |
| `react-startle` | 4 | reaction: alarm | 160 frames | Quick recoil: root jumps back and up within the fast band, deep squash on landing-into-hover, antenna whips forward and settles over ≥48 frames, lenses squint hard then reopen. Speed spent on the payoff, wind-up still ≥16 frames. |
| `rest` | 5 | the settle; almost-sleep | 400 frames loop | Root descends to `kRestHeightMm` (lower hover), bob slows and shallows, compression period lengthens, lenses half-squint, gaze drifts down. NEVER byte-identical adjacent frames — the life clock stays on. |
| `pirouette` (stretch) | 6 | play; "make it move in different ways" | 240 frames | One slow full yaw about its own axis with the antenna flaring outward slightly under the turn, gaze holding camera as long as anatomy allows then whipping round. Cut first if time is short. |

### 5.2 The constant compression and the hover

* **Compression** is a continuous authored wave on the deform sidecar:
  `flatten = kCompressAmpPm · wave(t)`, spread the matching positive-volume
  partner, period `kCompressPeriodKeys` (start ≈64 keys — chosen by eye,
  slight but UNMISTAKABLE per the direction). It runs in every clip; clips
  modulate its amplitude/period (channel deepens it, rest slows it). It is the
  primary "organic, bouncy, jiggly" carrier together with the antenna lag.
* **Hover** is root-channel authoring: two slow counter-travelling sines with
  an amplitude floor (the life-layer law — seasoning at 2–4% of the primary,
  never off). Height above ground is an authored value with its own character
  per clip: steady (idle), breathing (rest), reacting (startle).
* **Root trap:** any tracking camera follows a SMOOTHED channel excluding bob
  and life wave.

### 5.3 The bands every clip must land inside (from `07-MOTION-STYLE.md`)

| property | band |
| --- | --- |
| silhouette half-life | 32–54 frames, never under 11 |
| shape change p90 | ≤ 10.7 %/frame |
| max centroid jerk | ≈ 1.5 px |
| direction reversals per station per primary move | 0 |
| path length ÷ net displacement | ≈ 1.01 |
| largest net joint change in one move | ≈ 84° |
| frames for a beat to register | ≥ 16 |
| multi-beat anticipation floor | ≈ 112 frames (and a top: "too careful" exists) |

The bolt and particles are exempt (they are effects, not silhouette); the
antenna, body, and eyes are not.

---

## 6. The effects

### 6.1 The ten kinds of mana

**A kind is a named emitter table** in `unnamed02_fx.h`:
`{name, spawn geometry, rate, lifetime law, integer motion law, representation
(point|tri, opaque|additive), colour, size law}`. All kinds feed ONE
`Population` per scene (the reel's proven `debris_pop` pattern — handle
registered once, cleared and refilled each frame from integer state). Spawn
positions read the POSED bones the way `spawn_reel_gibs` does, so every kind
tracks the creature.

| # | kind | colour family | representation | motion law | where it runs |
| --- | --- | --- | --- | --- | --- |
| 1 | `motes` | cyan | point, additive | slow orbit about body centre, ±noise | all clips (the baseline shimmer) |
| 2 | `sparks` | white-cyan | point, additive | fast radial burst from a hinge, short life | startle, channel |
| 3 | `wisps` | magenta | point, additive | rise from the body crown with lateral noise, fade | idle, rest |
| 4 | `ring-pulse` | violet | point, additive | beads on an expanding equatorial torus | channel release |
| 5 | `helix-stream` | teal | point, additive | two counter-rotating bead helices climbing the body axis (the tornado brief's recipe at creature scale) | channel, pirouette |
| 6 | `droplets` | deep blue | point, opaque | fall from the loop's underside, gravity `t(t-1)/2`, despawn at a floor line | idle, drift |
| 7 | `drain-streamers` | green | point, additive | spawn on a shell 2–3 body radii out, integer pull inward to centre, accelerate | channel draw-in (the mana-draining read from the artist's sheet) |
| 8 | `star-glints` | cyan | TRI, additive | pop at random loop stations as tiny four-point stars (echoing the pupil), hold ≥16 frames, vanish | all clips, sparse |
| 9 | `bolt-beads` | white-violet | TRI, additive | §6.2 | channel blaze |
| 10 | `shield-orbit` | gold-amber | point, additive, larger | slow equatorial band of fat beads, near-constant | channel, react-curious |

Ten kinds ≠ ten simultaneous streams: idle runs ~4 kinds at low rate; channel
is the only clip that runs all ten. Per-particle RGB is free in the record, so
"varied colours" costs nothing.

### 6.2 The lightning substitute

`bolt-beads`: a deterministic polyline from hinge B (the peak) down through the
loop plane to the body centre, `kBoltSegments` (≈12) segments, each vertex
jittered `±kBoltJitterMm` by a hash of `(frame_index, segment)` — a new jagged
path every frame, which IS the crackle read. Each segment renders as one tri
sprite (bead) via the Population tri path, additive. Optional garnish: re-hash
only every 3–4 frames with a bright flash frame on re-hash, so the bolt
"strikes" rather than boils. Second bolt hinge A↔hinge C during the blaze peak.
Cost: ~24 beads. No new machinery — this is authoring on the existing path,
matching the recon's route 2 and the gib precedent.

### 6.3 Sun effects — scene-level, and which ones

* **The skybox bloom = `planet-sun-*`** (the FX recon's high-confidence read of
  "basically just blooms over the skybox"). One per scene, wired through the
  S1 chained hook. Choose the world ramp whose bloom flatters pink-on-dark by
  eye; a bespoke mana-violet ramp is three control colours in `kPlanets`-style
  if none does.
* **One authored sun per clip** (`U02SunSpec`, the Direction 29/30 house rule):
  far away (50 m class), additive emission the hue carrier, mults suppressed on
  complements, tuned CALM — Direction 30's lesson that under the ceiling
  restores hue. Pink pigment is an amplifier; check against the sunless
  baseline before judging "too loud".
* **Centre glow**: the S5 shared-ramp baked radial additive sprite at the
  projected body centre, radius `kCentreGlowRadiusPx` (≈32 px start), pulsing
  with the compression wave (same phase knob — the body visibly breathes light).
* **Lens flare**: at most ONE, on the `channel` showcase subject only, at the
  blaze peak (the ≤2-flare cap makes it a garnish, never a per-instance
  feature).
* **Point lights**: each conduit carries ONE additive point source at its
  centre (`kCreatureMaxPointLights = 4` scene-wide — four conduits saturate
  the budget exactly; the strongest four win by law, so a fifth degrades
  gracefully). Marker (the glow) and light are ONE position by construction.

### 6.4 The cost budget per instance — arithmetic shown

There are NO fragment/particle counters in the ratified set; every number
below is arithmetic against stated capacity, and the SPEC and publish note say
so. Frame = 384×240 = 92,160 px.

Per conduit, worst case (`channel`), with the named caps:

| item | arithmetic | px fill | % frame |
| --- | --- | --- | --- |
| mesh | ~1,300 tris; conduit ≈ 60×90 px on screen | ≈ 5,400 | 5.9% (it is the creature; not an effect cost) |
| particles | cap `kConduitParticleCap = 96` live; mean sprite 5 px → 25 px² | 2,400 | 2.6% |
| bolt beads | 24 beads × ~30 px² | 720 | 0.8% |
| centre glow | r = 32 px → π·32² | 3,217 | 3.5% |
| point light | +3 MACs + one saturating add per fragment it touches (ruled cost) | — | negligible |
| **effects total** | | **≈ 6,340** | **≈ 6.9%** |

Idle drops to ≈ 40 live particles + glow ≈ 4.6%. Scene-level, three conduits
channelling simultaneously ≈ 21% of frame in effect fill, PLUS one shared
`planet-sun-*` bloom (sky pixels only, ~1 skip + ramp + 2 noise octaves +
`isqrt` per sky pixel — the cheapest bloom in the machine, zero marginal cost
per conduit, zero palette growth). A scene cap `kScenePopulationCap = 384`
bounds the pathological case; instances share emitter tables and are
phase-offset, not duplicated. Pose cost: 3–4 instances × distinct
(clip, tick) tuples sit comfortably inside the ~128-tuple decoded-pose cache;
identical-phase instances share one decode.

**The governing rule, inherited from the tornado brief: visual density is
governed by screen coverage, not by a fixed count.** The per-kind rates in
`unnamed02_fx.h` are the knobs that enforce it; the LOD ladder (below) is the
mechanism at distance: `kMesh → kMicro → kSplat → kGlint` for the body, and a
distance-gated rate multiplier (named constant) that thins every emitter table
as the conduit shrinks on screen.

---

## 7. The named constants

All in `unnamed02_art.h`, one findable block. **Every one is an owner knob**
(rule 6: never remove the owner's control in the name of fidelity). The load-
bearing ones:

* **Form:** `kBodyRadiusMm`, `kBodyTeardropTaperPm[]`, `kBodyRings`,
  `kBodySegments`, `kNeckHeightMm`, `kLoopHeightMm`, `kLoopWidthMm`,
  `kLoopBladeRxPm`/`kLoopBladeRzPm`, `kLoopRings`, `kHingeRadiusMm`,
  `kHingePosMm[3]`, `kEyeStationDeg`, `kEyeVAngleDeg`, `kEyeBulgeMm`,
  `kEyeLensRxMm`/`kEyeLensRzMm`, `kEyeFacetSegments`, `kPupilStarSizeMm`.
* **Palette** (chosen by eye in scene at 240p, NEVER sampled from the scans):
  `kBodyPink`, `kBodyPinkShadow`, `kEyePurple`, `kEyeRimWhite`, `kPupilCyan`,
  `kCrayonGrainAmpPm` (must exceed the light rig's own range to be visible).
* **Motion:** `kHoverHeightMm`, `kRestHeightMm`, `kBobAmpMm`,
  `kBobPeriodAKeys`/`kBobPeriodBKeys`, `kCompressAmpPm`, `kCompressPeriodKeys`,
  `kAntennaLagKeys`, `kAntennaSettleKeys`, per-clip key counts
  (`kIdleKeys` … `kPirouetteKeys`), the gaze schedule tables.
* **Effects:** per-kind `{rate, life, speed, size, colour}` in the ten tables;
  `kConduitParticleCap`, `kScenePopulationCap`, `kCentreGlowRadiusPx`,
  `kCentreGlowGain`, `kBoltSegments`, `kBoltJitterMm`, `kBoltRehashFrames`,
  `kFxLodThinPm[]`.
* **Light:** `kU02Sun<Clip>` specs (position, mult gains, add emissions),
  `kCentreLightAdd{R,G,B}`, radii.
* **LOD:** the rung distances + hysteresis (mirror the blueprint taxonomy).

---

## 8. The verification plan

Bounded to the acceptance questions — no exhaustive validation.

### 8.1 Instruments (existing; import, never rewrite)

`rgbframe.py` (the ONLY frame reader; run its selftest once), `sunmeter.py`,
`inkmask.py`, the Zixxtrixx look tools (`contact_sheet.py`, `silhouette*.py`,
`sidecmp.py`, `clip_report.py`) — parameterised, not forked, where they take a
subject name. Calibrate every instrument against a known build before trusting
a delta.

### 8.2 Per acceptance point, in pixels

1. **Four balls, real hinges** — `react-curious` and `startle` exercise the
   hinges through visible angles; a fixed side-ortho diagnostic subject
   (`u02-side`) + a hinge-angle-over-time plot from the clip data (a flat line
   IS "the hinges never articulate"). Reviewer looks at the contact sheet.
2. **Eyes bulge, partly polygonal, more expressive** — a profile crop at
   native 240p set beside the artist's oblique eye sketch (the lens must stand
   proud of the silhouette); the facet read judged by eye on the front
   diagnostic; expressiveness judged on `react-curious` (gaze leads, body
   follows) — the reviewer asks "does it out-act Zixxtrixx's idle glance?".
3. **Compression visible, organic, bouncy** — trajectory plot of the top and
   bottom silhouette extents across `hover-idle`: the vertical extent must
   oscillate (the compression IS a band on that plot, and its amplitude in
   PIXELS is the "slight but visible" judgement); zero byte-identical adjacent
   frames anywhere (never-off floor); band table §5.3 checked per clip via
   `clip_report`-style measures.
4. **Several distinct animations, no attacks, no gait** — the clip list is the
   evidence; QA confirms no clip contains a strike beat and no footfall events
   exist in any clip.
5. **Ten kinds + lightning + suns + bloom** — one diagnostic subject
   (`u02-fx-tour`) cycles the ten kinds solo, 60 frames each, captioned; each
   must read DISTINCT at 240p (kind, colour, motion). The bolt judged on
   `channel` contact sheets — a new jagged path per re-hash, beads bright over
   the body. The bloom visible behind the creature in `hover-idle` and
   `channel` (the S1 deliverable). The reviewer looks at the tour end to end.
6. **Cheap, several on screen** — a `u02-trio` diagnostic subject: three
   instances, phase-offset, one scene sun + bloom, all effects live. It must
   render correctly and the SPEC states the §6.4 arithmetic verbatim with the
   honest label "arithmetic, not measurement — no fragment counters exist".
7. **Textured** — textured renders vs grey renders on the same frames; grain
   visible; form judged UNLIT once (a colour edge reads as a shape edge).
8. **Site** — §8.6 checklist.

### 8.3 Regression gates (green at close)

* `u02_probe` exit 0 — clearance contract (§8.4).
* meshcheck clean on the u02 mesh.
* `zhao-reel --check` "all sequence CRCs match" **to a file** — including the
  untouched Zixxtrixx 22 after S1/S2.
* Two `mku02page.py` regens `cmp`-identical.
* Deform gate: positive-volume law never tripped (compile fails loudly if so).

### 8.4 The probe — clearance, not penetration

`u02_probe.cpp` from the blueprint template: decode every key AND every 60 Hz
midpoint of every clip, skin every full-detail vertex, assert
`world_y(v) - terrain_height(x,z) ≥ kMinClearanceMm` (a positive constant,
derived from the ACCEPTED hover after the motion is approved — a window
derived from key constants, never absolute ticks), fail on any `SatLedger`
saturation. `CREATURE.json`'s ground-contact section declares: **"This
creature never contacts terrain. The contract is clearance. Zixxtrixx's
authored-penetration law does not apply."** Committed, per the CLAUDE.md law
that a thrown-away probe is unreproducible.

### 8.5 What QA checks that gates cannot

Gates catch regressions; only looking catches wrongness. QA watches every clip
end to end at native resolution, against both sheets, and answers: is it the
creature on the sheet; is it alive at rest; does the mana read as mana; would
three of them be delightful or noisy. Contact sheets of EVERY frame, never
sampled stills.

### 8.6 Publish

* `media/site-entry.json` transcribed into `website/creatures.json` as
  **`creatures[0]`** — one object prepended, Zixxtrixx byte-untouched at
  `[1]`, archives travelling with it. No `assemble.py` change.
* Every `renders/unnamed02-*.webm` (via `tovideo.py`: VP9 crf 16 yuv444p
  60 fps) has its poster PNG BESIDE it — assemble derives the poster path and
  never validates it; a missing PNG deploys a silent 404.
* `id: "unnamed02"` until the owner names it (unique — a collision silently
  cross-links two cards); rename everything the day a name arrives.
* One video per animation, plus the fx-tour and trio as additional tabs
  (comfortably under `MAX_TABS = 40`).
* `python tools/assemble.py .`, verify noindex present, then
  `deploy.ps1 -Project upheaval -Branch main` — both args, always; a missing
  branch silently demotes to a preview.
* Trigger: the FINISHED pass, worth looking at — not a file save. This is a
  finished-creature publish under the standing authorisation AND the
  direction's explicit order; do not stop to ask.

---

## 9. Deliberately not doing, and the cut order

### Not doing (scope walls)

* **No `FORGE.PRIM` ribbons, no god beams, no `radial_decay` post pass** — all
  specified-unbuilt; this creature does not fund engine phases.
* **No PART.STATE hardware sim, no representation-ladder engineering** — CPU
  integer emitters in the reel, the proven pattern.
* **No attacks, no shield/paralysis gameplay, no walk** — the direction is
  explicit; the artist's bolt/shield lore is future vocabulary, not this pass.
* **No generic effect-socket framework** — effects read posed bones the
  gib-spawner way, per-creature, in `unnamed02_fx.h`. Generalise when a THIRD
  creature wants it.
* **No fragment/particle counter instrumentation** — cost stays arithmetic,
  honestly labelled. Building counters is a hardware-lane decision.
* **No ownership-migration infrastructure** (`zreel`, provider seam, Upheaval
  build target) — the header split keeps the exit cheap; standing it up is its
  own project.
* **No cel pigment page variant** unless the in-scene look demands it — one
  RGB565 page + eye tile first; the house cel presentation comes from the
  shade path, not a second page.
* **No creature-name work** — `unnamed02` everywhere until the owner names it.

### Cut order, if the budget or the clock refuses

1. `pirouette` (the stretch clip).
2. The lens flare garnish on `channel`.
3. `kCentreGlowRadiusPx` down (32 → 24 px halves nothing else).
4. Demote `star-glints` (#8) from tri to point sprites.
5. Run fewer kinds concurrently per clip (rotate kinds through slots rather
   than shrinking any kind's read).
6. `kConduitParticleCap` down 96 → 64.

**Never cut:** the compression, the hinges articulating, the bolt in
`channel`, the skybox bloom (scene-level and nearly free), the eye
expressiveness, publishing at the top. Those ARE the direction.
