# RECON S4 — sacengine spell/effect system vs Zhaozhou hardware

*Fable recon+feasibility agent, 2026-08-16. Primary source: `C:\programmieren\sacengine\` (D language reimplementation of Sacrifice; engine code only — `.SXMD/.SXSK/.SXTX/.TXTR` art lives in the absent game install, so texture contents are inferred from format and usage, not inspected). Hardware envelope: charter v0.2 §8/§13/§14/§15/§26, `spec/qformats.md`, `spec/form/field-ir.md`, `spec/terrain_rules.md`, `spec/sky_and_beams.md`, `design/ops.yml`, `design/contracts/*`. Builds on S1 (terrain), S2 (creatures), S3 (giants). Analysis only; no repo modified. No builds, tests, or provers were run — greps and file reads only. Persisted by orchestrator.*

---

## 1. The spell vocabulary — data, dispatch, and the honest counts

**Data model.** A "spell" is one of three fixed-layout records from the original game data: `Spel` (wizard spell: cooldown, manaCost, range, target flags, castingTime, damageRange, amount, speed, acceleration, duration, effectRange, sounds, 8 argument slots, three 36-char proc names — `spells.d:212-260`), `Cre8` (creature summon, 528 bytes: stats, abilities, animation table — `spells.d:16-78`), or `Strc` (structure spell with one building tag per god — `spells.d:273-288`). The engine wraps them in `SacSpell` (`sacspell.d:74-233`) and patches ~15 values by tag (e.g. heal duration=4.5 s, firewalk range×0.75 — `sacspell.d:197-211`). Targeting legality is a pure flag intersection: `SpelFlags` × `TargetFlags` (`sacspell.d:8-66`, `spells.d:172-209`) — 14 spell-side bits against ~20 target-side bits, plus a handful of per-tag exceptions (`sacspell.d:154-164`). **This is a fully data-driven cost/targeting layer over hand-written per-spell mechanics — there is no scripting language.** The three proc-name fields prove the original engine dispatched to native code too.

**The enum.** `SpellTag` has **188 members** (`nttData.d:1006-1210`): ~93 creature/hero summons, **53 castable wizard spells** (2 neutral + 9 per god × 5 + 6 structure spells), 19 active creature abilities, 7 passive abilities, ~30 ranged attacks.

**Dispatch.** One switch turns a cast into effect records: `state.d:9309-9467` (wizard spells), `state.d:11655-11786` (ranged attacks/abilities), `state.d:7738-7740` (passives). Effects live in `Effects(B)` (`state.d:4934`), a struct-of-arrays with **203 typed effect lists** over **212 effect struct types** (`state.d:2363-4933`) — most spells contribute a `FooCasting` record (channelling phase) plus a `Foo` record (live phase) plus 0–2 satellite records (projectile drops, per-target sub-effects).

**Not implemented in sacengine:** `meanstalks`, `bore`, `tornado` exist only as tags and AI-cooldown table entries (`state.d:8993,9065,9106,9141` for meanstalks/bore; `state.d:8999,9071,9185` for tornado). No cast case, no effect struct, no renderer. Design intent for these is web-sourced (see §3 and Sources).

## 2. The interesting number: ~26 distinct mechanisms under 188 tags

The 212 effect structs collapse onto a small set of shared machinery. I count **13 presentation primitives** and **13 sim-behaviour families** — **about 26 distinct mechanisms** carry the entire spell system of a AAA 2000 release. Cost-per-god after the first god is almost pure data.

### 2.1 Presentation primitives (what the renderer can draw)

| # | Primitive | Evidence | Used by |
|---|---|---|---|
| P1 | **Atlas billboard particles** — 72 species, all 3×3/4×4/5×5 animated sprite atlases with per-species {colour, energy, gravity, relative-to-parent, bounce-off-ground} flags | `sacobject.d:1211-1282` (enum), `1294-1375` (flags) | every spell's dressing; per-god casting sparkle species (`castPersephone…castCharnel2`) |
| P2 | **Animated sprite-frame billboards** for discrete effects — 16-frame rings, hearts, frogs, souls | `SacBlueRing sacobject.d:2113-2126`, `SacRainFrog 2746-2760`, `SacCharmHeart 3185-3198` | level-up/teleport rings, charm hearts, rain of frogs |
| P3 | **Bone-chain tubes** — a 3-sided prism strip, one bone per segment, posed through the creature skinning path (S2's star trick), with **four distinct spine drivers**: (a) jittered polyline (`LightningBolt.changeShape` `state.d:2597-2611`, 10 segments, re-jittered every 6 frames), (b) rope physics — 4–5 control points with velocities, spline-interpolated (`SacDocTether state.d:2445-2450`, `Guardian:2553-2566`, `Vine:2863-2872`), (c) **trail-history ribbons** — a ring buffer of the last N head positions sampled as the spine (`DemonicRiftSpirit` 120 frames `state.d:3270-3272`, `CharmSpirit` 60 `:3940-3942`, `WailingWallSpirit` 90 `:3495-3497`, `HellmouthProjectile` tongue 60 `:4602-4604`), (d) straight beams (`renderLaser renderer.d:2913`, `renderCord :3250`) | mesh: `makeLineMesh sacobject.d:2157-2195` | lightning, chain lightning, soul wind bolts, styx bolts, death bolts, cage-pull bolts, guardian/sac-doctor tethers, grasping vines, vinewall, charm/rift/wailing spirits, hellmouth tongue, rainbow arc |
| P4 | **Bone-chain wall sheets** — double-sided vertical strips, bone per post, height modulated by a travelling sinusoid `top·(1+0.1·sin(2π(0.125t−f/60)))` | `makeWallMesh sacobject.d:2921-2963`, `Firewall.height state.d:3438-3440` | firewall, wailing wall (+ vinewall/wall-of-spikes use per-element props) |
| P5 | **Swept/lathed one-off meshes with baked UV-scroll frame arrays** — the engine pre-builds N copies of a surface whose texcoords advance per frame | Wrath ring band, 512 segments (`sacobject.d:2222-2277`); soul-wind funnel, 32 frames × 33×33 verts (`:2636-2675`); demonic-rift border cylinder (`:2778-2812`) | wrath, soul wind, demonic rift, air shield cylinder |
| P6 | **Morph-target noisy-sphere blobs** — sets of pre-distorted spheres pairwise vertex-morphed on the GPU (`shadelessMorphMaterialBackend.setMorphProgress`, `mesh1.morph(mesh2)`) | `makeNoisySphereMeshes` uses: `SacCloud sacobject.d:2703-2719` (20 distortions; **rendered as 20 overlapping alpha blobs per cloud** `renderer.d:2279-2288`), `SacHealingAura :2829-2853`, `SacDeathAura :3234-3262`, `SacExplosionEffect :2677-2701` | rain-of-X storm clouds, healing aura, death aura, explosion shells |
| P7 | **MRMM mesh-flipbook actors** — vertex-animated models played as morphing mesh-frame sequences | cow: 65 mesh frames incl. a 3-salto loop (`SacCow sacobject.d:3157-3183`); Death avatar: 27 frames with spawn/walk/attack index choreography (`SacDeath :3200-3232`); dragonfire serpent (`SacDragonfire :2615-2634`) | bovine intervention, death, dragonfire |
| P8 | **Solid props with rigid ballistic motion** — position+velocity+quaternion spin, ground bounce | `EruptDebris state.d:3071-3077,17348-17378`, `HaloRock :3176-3192`, `Spike :3618-3628`, fence posts `:3706-3717`, snowball, rock, boulders | erupt debris, halo of earth, wall of spikes, fence, rock/boulderdash/earthfling projectiles |
| P9 | **Terrain-conforming decal patch** — a 32×32 GroundPatch mesh draped over live terrain height, annular grow/shrink radii; the **only** terrain-draped spell visual | `SacFrozenGround sacobject.d:2855-2869`, `renderer.d:3165-3176`; radius stepping `floor(minScale·6)/6` `state.d:3365-3398` | frozen ground (ice sheet) |
| P10 | **Per-instance creature material overrides** — alpha/energy uniforms (ghost, ethereal 0.36, stealth 0.06), bulk inflation uniform, petrify shader flag, slime diffuse rebind, freeze ice-shell overdraw mesh, vined double-render at pre-teleport position, speed-up afterimages (stale pose + position re-render, `SpeedUpShadow state.d:2572-2580`) | `renderer.d:1271-1301,1309-1321` | all shield/form/status spells |
| P11 | **Height-threshold dissolve** — per-object world-Z threshold discards fragments; buildings materialize bottom-up with a glow gradient band, altars sink in reverse | `setThresholdZ state.d:7160-7166`, altar `24185-24187`, backends `renderer.d:1247-1252,1327-1341` | structure spells, convert/desecrate, altar destruction |
| P12 | **Terrain displacement visuals** — S1's two tiers (analytic transient + persistent delta texture) | `state.d:3010-3069` (Erupt), `:4802-4838` (Quake), `:4054-4113` (Volcano), `:3811-3827` (Bombardment dent) | erupt, quake, volcano, bombardment |
| P13 | **Screen shake** — camera displacement with squared distance falloff, 30 Hz shake stepping | `ScreenShake state.d:4909-4924` | erupt, quake, explosions, big impacts |

Material census across the effect asset table (`renderer.d:150-950`): **42 additive** and **25 alpha-blend** material declarations; **every one** is shadeless (unlit), `depthWrite=false`, most with an `energy` multiplier of 5–20 driving bloom. Nothing else — no distortion, no render-to-texture, no post-processing exists anywhere in the spell system.

### 2.2 Sim-behaviour families (what the state machine does)

| # | Family | Evidence |
|---|---|---|
| B1 | **Channelled casting**: `ManaDrain` per-frame cost + `CastingStatus {underway, interrupted, finished}`; interrupt cleanly cancels (Volcano even un-applies terrain, §3) | `state.d:2384-2388`, every `*Casting` struct |
| B2 | **Projectile grammar**: homing with `PositionPredictor` target-position prediction (`state.d:493`), ballistic with per-spell `fallingAcceleration` (`sacspell.d:120-127`), instant beams with `remainingDistance`, 3-way splits (`BoulderdashProjectile.which state.d:4489-4502`), multi-phase state machines (BlindRage fly→form-fist→explode `:3859-3884`) |
| B3 | **Chain retargeting** with fixed-size target lists: chain lightning 7 (`state.d:2966`), rainbow 6 (`:2917`), dragonfire 5 sequential (`:3102`), soul wind 24 (`:3132`) |
| B4 | **Jumping-agent infectors**: fall → sit → jump at victims → attach and tick damage (RainFrog `state.d:3226-3250`, BlightMite `:4694-4707`, StickyBomb `:4756-4768`, swarm Bugs `:2692-2715`) |
| B5 | **Status flags + timers on creatures**: shields, poison/oil DoT stacking, slime slowdown `4^n`, freeze pause, petrify, blind rage berserk, charm side-change — all bounded per-creature flag+timer records (S2 covered the render side) |
| B6 | **Area zones with per-target bookkeeping**: frozen ground target list (`state.d:3391-3397`), firewall per-target frame counting (`:3442-3462`), cloudkill strike rate, healing/death auras |
| B7 | **Wall-line placement grammar** — one mechanism, five spells: centre + direction, grow `left/right` along the line, conform each segment to ground height (`get(t)` → `getHeight`, `state.d:3420-3424`), stop sides on obstacles (`leftStopped/rightStopped`), grow/stationary/shrink lifecycle. Firewall (`:3407-3469`), wailing wall + 12 patrol spirits (`:3503-3547`), vinewall 180 vines (`:3556-3616`), wall of spikes 180 spikes (`:3641-3680`), fence 12 posts with charge/discharge logic (`:3689-3735`) |
| B8 | **Cloud rain spawners**: cloud at height 90, drops at `dropRate` per frame, per-spell fall behaviour and impact conversion — frogs (agents), fire (ground patches), plague (droplets), bombardment (boulders + permanent dents). `state.d:3742-3827` |
| B9 | **Impulse physics on creatures**: catapult `20·(1−d/42.5)` (`state.d:17275-17277`), squall pushback (`:4379-4387`), web/cage pull toward attacker (`Pull :4726-4754`), flurry implosion attraction (`:4479-4487`), bovine burial 4 m under (`:3905-3917`) |
| B10 | **Soul/corpse/allegiance manipulation**: soul mole/soul wind steal souls on a 9 s round trip (`:2890-2902,3121-3139`), animate dead, charm (side change + hearts), death (instant kills via bolts, max 8 targets `:4005-4041`), guardian (creature→building tether), convert/desecrate rituals — 4 sac doctors, 4 tethers, altar bolts, red vortex (`Ritual :2480-2498`, `RedVortex :2411-2424`) |
| B11 | **Translocation**: teleport (with the grasping-vines interaction — the victim's ghost renders at `preTeleportPosition`, `renderer.d:1309-1321`), firewalk (`state.d:4843-4851`) |
| B12 | **Summon/materialize lifecycle**: summoned creatures fade in at alpha 0.36 (`state.d:9300-9307`); buildings rise via P11; altar destruction reverses it |
| B13 | **Vision**: divine sight flying eye (`:4669-4678`), stealth alpha + shadow-pass skip (`renderer.d:1259`) |

All 13 families are ARM/sim-side logic in our architecture — they touch no hardware seam except through draw submissions and the terrain/stamp/particle lanes. The hardware question is entirely §2.1.

## 3. Terrain-deforming spells under the dual-heightfield format — the priority assessment

The donor's terrain-affecting spells, complete list: **Erupt** (transient analytic, `state.d:3010-3069`), **Quake** (transient, creature ability, `:4802-4838`), **Volcano** (persistent 33×33 stencil, incremental scaling, residual 0.25, `:4054-4113`), **Bombardment** (persistent ellipsoid dents r=15, h=1.2, `:3811-3827`), plus the unimplemented **Bore** and the non-deforming **Frozen Ground** (a floating decal — the terrain itself never changes material; retexturing is the donor's admitted TODO, `sacmap.d:176`, S1 §4).

**Mapping onto our format (all fit):**

- **Erupt/Quake → FIELD.SEQ.EARTH transient programs.** The math is cos envelopes + polynomial ramps + one expanding annulus. Our ISA has `FIELD.SIN/COS` (table class), `FIELD.RING` ("annular distance falloff… the crater_ring demo's core shape", `design/ops.yml:361-375`), `FIELD.SMOOTHSTEP`, `FIELD.DIST.APPROX`, `MAD` — the Erupt displacement transcribes op-for-op; I estimate it fits the 64-instruction ceiling (`spec/form/field-ir.md:51`) with clear headroom, but the exact count is **not derived here**. The lattice law (`terrain_rules.md §4.3`) makes the donor's own CPU/GPU drift bug (S1: rebound 2.0 vs 3.0) structurally impossible.
- **Volcano → TERRAIN.BAKE.** The contract already encodes the donor's exact trick: "incremental-scaling law: apply `(to−from) × stencil`, so interrupted casts un-apply and permanence decays to a residual fraction — terrain_rules §9" (`design/contracts/TERRAIN.BAKE.md`). S1's recommendation landed; nothing further needed. One process note: the donor's plaza-flattening term reads *current composed heights* when computing the stencil (`computeDisplacement` reads `state.getHeight`, `state.d:4083-4097`). In our split this is stencil *generation*, which is ARM/sim-side against the composed cache — no hardware read path is implied. Worth one sentence in TERRAIN.BAKE's notes when it advances.
- **Bombardment dents → small bake stamps** + `SURFACE.STAMP` scorch (stamp primitives include circle/ring; `design/contracts/SURFACE.STAMP.md`).
- **Erupt debris/catapult/stun-ring** → PART polygon particles (64 debris + 200 rock sprites, `state.d:17291-17311`), sim catapult, sim stun ring chasing `waveLoc` (`:17328-17341`), ScreenShake (`:17287`). All existing lanes.

**Where we become better than the donor — concrete, not aspirational:**

1. **Bore is the crown finding.** Design intent (web-sourced; the donor never implemented it): a mole spirals outward from the target "leaving a black line in its wake", then the ground **opens into a hole** that destroys units near it. Sacrifice's single heightfield could only fake a pit; sacengine has *nothing*. Our format was frozen with exactly the needed law: deepening bake → composed top meets bottom at 4 corners → `VOID_BREACHED` (`terrain_rules.md:210-212`), entities go ballistic through the breach, particles pour into open sky with zero new code (addendum §1). The spiral scorch line is `SURFACE.STAMP` spline mode verbatim. **Bore should be the Wound-Lab-era proof piece for "more deformable than Sacrifice": the donor's own top-tier spell, done honestly for the first time.**
2. **Erupt/bombardment near rims**: thin lips punched through by a crater become breaches; rim edge-bites and true-thickness cliff walls (FORGE.CLIFF) make the aftermath legible. The donor just bent its skirt geometry.
3. **Frozen Ground done properly**: our stamp lane *converts the material* (`SURFACE.STAMP` "material conversion") and writes gameplay state (`FIELD.WRITE.HAZARD`, `FIELD.WRITE.NAV` — `design/ops.yml`), where the donor drapes a translucent decal above unchanged terrain and keeps a manual target list. We get the ice *into* the world: Mosaic picks ice tiles, nav slows, and the decal patch (P9) becomes optional garnish.
4. **Scars the donor never shipped**: Erupt's scar is literally `// TODO: scar` twice (`state.d:17257,17312`). Our residual-decay bake gives every big spell a permanent mark by default.
5. **Normals/nav after deformation**: donor never updates either (S1); our finite-difference normals from the composed lattice (`terrain_rules.md:306`) and nav-dirty marking out-Sacrifice Sacrifice visibly.
6. **Tornado closes a loop**: unimplemented in the donor, but charter §14 already uses it as the worked example — "a spline field plus several generated ribbons, polygon debris, soft dust, distortion and a terrain footprint". Every listed ingredient is a specified lane (FIELD.SEQ.FLOW vortex recipe, FORGE ribbons, PART, SURFACE.STAMP).

**Would anything be harder for us?** Two watch-items, no format gap:

- **Concurrent-effect sizing (the one real check).** Donor worst case: 8 wizards, each able to hold an Erupt (footprint radius 90 m ⇒ diameter 180 m — at 2 m pitch and 64 m patches that is a 3×3-to-4×4 patch footprint, 9–16 patches) plus Quakes and a Volcano, overlapping. `terrain_rules.md:410` promises "bounded lists per patch with bake/compose/reject on overflow", and `:202` sums live field lanes in command order, but the **bound's numeric value is not frozen anywhere I can find**. Recommendation to the terrain owner: freeze the per-patch live-field bound with the 8-wizard donor scenario as the sizing floor, and add a cost-model line for "patches × lattice × transient programs per frame". Not costed here.
- **Bottom-surface live deformation** is v1-refused (bake only, `terrain_rules.md:435-436`). No donor spell needs it (the donor has no real underside), so this costs nothing against the "more deformable than Sacrifice" claim; it stays the logged v2 hook.

**Format amendment needed: none found.** I went looking for a missing lane and did not find one — breach law, incremental bake, stamp spline, hazard write, velocity plane, and fall-through together cover every deformation behaviour the donor has *and* the two it only dreamed of (Bore, Tornado). The freshly frozen format survives its first hostile review.

## 4. Transparency and blending under our silicon — the verdict

**First, a framing correction (loudly): CLUT8 is our texture format, not our framebuffer.** The working tile is **24-bit RGB** with an 8-bit effect tag (charter §8), and the ratified material recipes include **additive, multiply, fogged alpha, masked** (charter §15). The sky/beams spec has already ratified the additive fast path with saturating adds and declared sorting moot for it (`sky_and_beams.md:38`). So the question is not "how do we fake blending through a palette" — the tile pipeline blends in RGB. The Noctis ramp discipline (2-bit ramp + 6-bit intensity, colour at scanout — recon `FINDINGS-N3`, not yet ratified spec) is a *complementary* tool for palette-space effects, not the load-bearing substitute the briefing implied.

**Second, the donor's actual requirement is humbler than its reputation.** Measured from the source:

1. **Additive dominates: 42 of 67 effect materials** (`renderer.d:150-950`). Additive commutes — order-independence is free. Our additive recipe + saturating adds on the 24-bit tile reproduces it exactly; the `energy` multipliers (5–20×) become either pre-scaled texture ramps or the effect-tag/glow path (POST.GATHER/COMPOSITE), which the sun already uses.
2. **The donor ships with NO transparency sorting.** The transparent pass iterates effect types in fixed declaration order, all with depth-test on and depth-write off (`renderer.d:1241-1246`; every material `depthWrite=false`). Alpha effects interpenetrating other alpha effects pop and layer wrongly *in the donor*, and nobody ever noticed enough to fix it. **§26's "no exact OIT" refusal therefore costs zero fidelity against this target.** Our pass-6 coarse depth binning (charter §8) is strictly more ordering than the donor has.
3. **The alpha minority (25 materials) is low-stakes**: bugs, frogs, freeze shell (transparency 0.2), soul wind (0.75), air shield (0.075!), hellmouth tongue, ethereal/stealth creatures. Single-layer alpha over opaque geometry — fogged-alpha recipe, correct under any order. The only *stacked*-alpha offender is the storm cloud: 20 overlapping morphing alpha blobs (`renderer.d:2282-2287`) — see §6, don't copy that construction.
4. **Effect textures are palette-native anyway.** Every effect texture is an original-game `.TXTR` (256-colour paletted format) — the donor's glow ramps were authored *in* 256 colours. Where bilinear-through-palette would band (smooth additive ramps), the beam spec already made the ruling: tiny direct-colour RGB565/ARGB4444 ramp textures (`sky_and_beams.md:38`), a legal charter §15 format.

**What will look right at 240p/CLUT8:** everything additive (bolts, glows, rings, sparkles, fires, tethers — the bulk of Sacrifice's spell identity); single-layer alpha shells and ghosts; dissolves via fogged alpha + resolve dither; petrify/slime *better* than the donor (palette swap or CLUT remap instead of a fragment-shader flag — an actual CLUT8 superpower).

**What will not, and the substitutes:** (a) 20-blob alpha clouds — restage as an additive core + a few fogged-alpha soft sprites (PART.SOFT) + P2 underside flicker; the silhouette survives, the smooth self-occlusion does not, and at 240p the difference is marginal against a bright sky; (b) large translucent walls seen through each other will sort per coarse bin, popping exactly like the donor when two walls interpenetrate — acceptable by donor precedent; (c) the building-summon glow gradient becomes a dithered masked band + an additive FORGE ring riding the threshold height — crisper, slightly less creamy.

**Verdict: the donor's effect stack is additive-first, unsorted, unlit, and single-sample — it was practically designed for our §26 refusals.** The 24-bit tile + additive/fogged-alpha recipes + effect-tag glow already specified are sufficient; the ramp discipline adds palette-space statuses and scanout tinting on top.

## 5. Feasibility classification — every mechanism, sorted

**Directly portable — 19 of 26** (blocks named):

| Mechanism | Blocks |
|---|---|
| P1 atlas particles (72 species = a species table) | PART.STATE/UPDATE/SPAWN/COLLIDE/LADDER/SOFT (gravity/bounce/relative map onto PART.UPDATE recipes; short-lived "relative" casting sparkles respawn per tick from sim, as the donor does) |
| P2 sprite-frame billboards | PART.SOFT / FORGE.PRIM billboard sheet |
| P3 bone-chain tubes, all four spine drivers | FORGE.PRIM tube/chain + GEOM.LOOM spline node + GEOM.SKIN 2-weight; trail ribbons = ring buffer of head positions + FIELD.SAMPLE.SPLINE |
| P4 wall sheets | FORGE.PRIM **spline wall** (named in the contract) + oscillator node for the travelling sinusoid |
| P5 swept meshes + UV scroll | FORGE.PRIM ring/radial shell/cone; UV scroll is a draw parameter for us — the donor's 32-mesh frame arrays existed only because it baked scroll into vertices |
| P8 ballistic props | PART.EXPAND polygon particles (charter §13's headline use case: "rock debris… chunks of terrain") |
| P9 ground decal | SURFACE.STAMP/SHEET (and see §3 — we upgrade it to real material conversion) |
| P12 terrain displacement | FIELD.SEQ.EARTH + TERRAIN.BAKE + composed-lattice law (§3) |
| P13 screen shake | ARM camera transform; trivial |
| P10 (majority): alpha/energy per instance, bulk, freeze shell, vined double-render, afterimages | fogged-alpha recipe params; GEOM.LOOM SCALE node (bulk); extra draw with stale (clip,frame) — cheap via the GEOM.POSE shared cache; ice shell = one more meshlet |
| B1–B13 all sim families | ARM/Form sim code + existing lanes; REPARENT covers guardian/charm/carry verbs; catapult/pull/pushback are velocity writes; FIELD.WRITE.HAZARD/NAV carry zone gameplay |

**Portable if simplified — 4** (what is lost, and is it visible at 240p):

1. **P6 morph blobs** → mesh flipbook with hard cuts + the Measure's stable crossfade dither, or (later) a FIELD.SEQ.WARP noise program — Warp8 is specified but sits at cut-order 5. Lost: continuous shape interpolation → 30 Hz stepped shape. The donor's own creatures already hard-cut at 30 Hz keys (S2); at 240p under additive restaging, effectively invisible.
2. **P7 MRMM flipbook actors (cow, Death, dragonfire)** → same flipbook treatment, or re-rig to skeletal clips at asset compile (Death and the cow are periodic gaits — skeleton-friendly). Lost: nothing at 240p if re-rigged; flipbook costs mesh memory (65 cow frames — measure before choosing). Not costed.
3. **P11 height-threshold dissolve** → per-vertex fade band + masked (alpha-test) dither instead of a fragment-shader discard, plus an additive ring at the threshold to sell the seam (the beam cone's `fade_band` vertex-colour dissolve is the ratified precedent, `sky_and_beams.md:36`). Lost: smooth per-pixel edge → ordered-dither edge. At 240p a dither band during a 2-second materialize reads as sparkle, arguably in-genre.
4. **P10 (remainder): petrify/slime as shader flags** → petrify = CLUT ramp remap to stone greys, slime = texture-page swap at asset level. Lost: nothing — gained: cheaper and more stylish than the donor's uniform hacks.

**Needs a new block or contract — 0.** I looked hard for one and did not find one. The nearest candidates dissolve on inspection: trail ribbons (ring buffer + spline sample = existing ops), morph (Warp8 already reserved), building dissolve (masked recipe). The spell system as a whole is a *content* program for FORGE.PRIM + PART + FIELD + STAMP, which is exactly what those blocks were chartered for.

**Refused by §26 — 2 techniques (not whole mechanisms), substitutes in hand:**

1. **Exact OIT for stacked translucency** (clouds; interpenetrating walls) — refusal stands at zero fidelity cost against the donor, which never sorted (`renderer.d:1241-1246`). Substitute: additive-first restaging + pass-6 coarse binning; documented in §4.
2. **General fragment shaders** (the donor's petrify/bulk/threshold/morph uniforms all live in bespoke GLSL backends) — substitutes per item above: CLUT remap, LOOM SCALE, vertex fade + masked, flipbook/Warp8. No refusal needs relaxing; nothing proposed here weakens §26.

## 6. Ranked cheap wins — character delivered ÷ hardware cost

1. **The wall-line grammar (B7/P4)** — one placement mechanism + one spline-wall primitive yields **five god-defining spells** (fire/spirit/vine/spike/fence) by swapping dressing: texture, particle species, per-element prop, target-list rule. The single highest spells-per-mechanism ratio in the donor. All existing blocks.
2. **Bore via the breach law** — the donor's own unshipped fantasy, delivered by machinery we already froze (§3). One spline scorch stamp + one deepening bake + fall-through. The definitive "more deformable than Sacrifice" demo.
3. **Trail-history ribbons (P3c)** — a 20–40-entry position ring buffer turns the projectile you already have into wailing spirits, charm wisps, rift ghosts, and the Hellmouth tongue. Enormous Charnel/Persephone identity; near-zero cost over the spline tube.
4. **Erupt/Quake full theatre** — the wave program is specified (S1); add the sim garnish measured here: 64 polygon-debris chunks + 200 rock sprites + catapult impulse + expanding stun ring + ScreenShake every 6 frames (`state.d:17287-17341`). Each garnish is an existing lane; together they are the donor's most famous screenshot.
5. **Chain retargeting (B3)** — a 7-slot target array + re-aimed bolts = chain lightning, rainbow (heal chain), dragonfire. Pure sim logic over P3.
6. **Per-god casting particle species** — god identity during every single cast, delivered as PART species table rows (9 `cast*` species, `sacobject.d:1242-1250`).
7. **Speed-up afterimages** — re-render the same meshlet at 2–3 stale poses with fogged alpha; the GEOM.POSE shared cache makes stale-pose fetch a table hit. Slow-motion's sibling, and nobody else on 2000s hardware had it.
8. **Jumping infectors (B4)** — frogs/mites/sticky bombs: one tiny agent state machine + P2 billboards; comedy and menace, no hardware.
9. **Screen shake** — four fields and a squared falloff (`state.d:4909-4924`). The cheapest "the world has mass" signal in the whole study.
10. **Materialize dissolves (P11 simplified)** — buildings rising out of the ground with a dither-sparkle band; sells the entire structure-spell economy with one vertex-fade trick.

## 7. Not worth taking

1. **The 20-blob morphing alpha cloud** (`renderer.d:2282-2287`) — 20 overlapping full-alpha morph meshes per storm cloud is desktop-GPU overdraw arrogance; restage (§4). The *spell* is worth taking; the construction is not.
2. **Baked-scroll frame arrays** — 32 pre-built meshes per effect to fake a UV offset (`SacSoulWind`, `SacBlueRing` et al.). We have draw-parameter UV scroll; copying this would waste meshlet RAM for nothing.
3. **Tessellation profligacy** — Wrath's 512-segment ring (`sacobject.d:2238`), 129-vertex rift border circles, 33×33 funnels, drawn at any screen size with no LOD. FORGE.PRIM's screen-error subdivision replaces all of it; keep the shapes, not the counts.
4. **Per-effect unbounded `Array!` growth** (bugs, spikes, vines allocated per instance, `state.d:2714` TODO admits it) — our PART.STATE stream + budgets is the correct home.
5. **Bespoke GLSL backends per status effect** — nine shader backends where a palette line-item would do. Refused by §26 anyway; the substitutes are better.
6. **The unsorted-alpha shrug as a *plan*** — we inherit the donor's tolerance, but adopt additive-first restaging as discipline rather than luck.
7. **Anything about tornado/meanstalks/bore *from the source*** — there is nothing in the source to take; they are design intent only (and our lanes already cover the intent, §3).

## 8. Corrections and contradictions found

1. **Briefing framing corrected (§4): "CLUT8 ramp discipline is our strongest tool for spell blending" — no.** The working tile is 24-bit RGB with ratified additive/fogged-alpha recipes (charter §8/§15; `sky_and_beams.md:38`); the ramp discipline is a recon-stage complement (N3 is not yet spec), best for palette statuses and scanout tinting. Spell blending needs no palette heroics.
2. **"No exact OIT" costs nothing against this donor** — sacengine itself draws all translucency unsorted in type order with depth-write off (`renderer.d:1241-1246`). The shipped look never depended on ordering. This strengthens, not strains, §26.
3. **S2 §7 refinement (not a contradiction)**: "everything else is procedural spheres/boxes/tubes + billboards + particles" undercounts. The spell renderer contains three families S2 missed: **morph-target noisy-sphere blobs** (clouds/auras, P6 — the only vertex-morph GPU path in the codebase), **MRMM mesh-flipbook actors** (cow 65 frames, Death 27, dragonfire — P7), and **trail-history ribbon spines** distinct from rope-sim spines (P3c vs P3b). The first two are the only spell visuals that don't map 1:1 onto already-specified blocks (both land as "portable if simplified").
4. **S2's "lightning uses 10 segments" confirmed** (`numLightningSegments=10, state.d:2596`) — and each Lightning holds **two** simultaneous jittered bolts (`bolts[2], state.d:2631`), re-jittered every 6 frames; chain lightning uses one per hop.
5. **S1's Erupt/Volcano numbers all re-verified** against `state.d:3010-3069/4054-4113` — no drift from S1's report. The Volcano stencil-generation composed-height read (§3) is a nuance S1's "directly portable" verdict glossed; it resolves ARM-side without hardware impact.
6. **Three spells the donor never implemented** (tornado, meanstalks, bore) — worth stating loudly because every "sacengine is far along" summary implies completeness. The AI knows their cooldowns; the world has never seen them cast in this engine (`state.d:8993-9186` is their only trace).
7. **No prior-recon claim was found false this run.** The S2 weight-clamp affair was already corrected by the architect; nothing of that severity surfaced in the spell lane.

## Headline summary

- **188 spell tags, 212 effect records, 203 live-effect lists — carried by ~26 mechanisms** (13 render primitives + 13 sim families).
- **Feasibility: 19 directly portable, 4 portable-if-simplified, 0 new blocks needed, 2 §26-refused techniques with substitutes in hand.** The spell system is a content program for FORGE.PRIM/PART/FIELD/STAMP, not a hardware program.
- **Terrain: every donor deformation spell fits the frozen dual-heightfield format; no amendment required.** Bore — which the donor never implemented and its format could not express — falls out of our breach law and should be the flagship "more deformable than Sacrifice" demo. Two watch-items for the terrain owner: freeze the per-patch live-field bound against the 8-wizard worst case, and add a transient-footprint cost-model line.
- **Transparency: the donor is additive-first (42/67 materials), unlit, and never sorts.** Our 24-bit tile + additive/fogged-alpha recipes + coarse binning already exceed its contract; CLUT8 constrains textures, not blending, and pays us back with palette-swap statuses.
- **Best cheap wins**: the wall-line grammar (five spells from one mechanism), Bore-by-breach, trail-history ribbons, the full Erupt theatre, chain retargeting.

*Design-intent sources (web, for the three unimplemented spells only). All implementation claims cite `C:\programmieren\sacengine\source\` file:line.*

Sources:
- [Sacrifice Wiki — Tornado](https://sacrifice-shiny.fandom.com/wiki/Tornado)
- [GameFAQs — Sacrifice Unit/Spell Guide](https://gamefaqs.gamespot.com/pc/914208-sacrifice-2000/faqs/25532)
- [All The Tropes — Sacrifice](https://allthetropes.org/wiki/Sacrifice)
