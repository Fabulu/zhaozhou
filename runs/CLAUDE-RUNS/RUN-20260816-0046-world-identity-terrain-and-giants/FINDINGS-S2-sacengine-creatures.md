# RECON S2 — sacengine creature/animation system

*Fable recon agent, 2026-08-16. Source: `C:\programmieren\sacengine\` (D). Note: repo is **engine code only** — the `.SXMD`/`.SXSK` art assets live in the absent game install, so per-model vertex counts are format bounds, not measurements. Persisted by orchestrator.*

## 1. Creature geometry

**SXMD (mesh) + SXSK (animation) + SXTX (TGA texture per body part).** A creature is a list of **body parts**, each a *generalized cylinder*: stacked rings of vertices auto-stitched into triangles by zig-zag merge of adjacent rings (`saxs.d:104-116`), plus optional `CLOSE_TOP`/`CLOSE_BOT` fan caps. **Topology is largely implicit** — the file stores rings + per-entry 8-bit angular "alignment" (→ U texcoord, `saxs.d:90`); triangulation is derived. Extremely compact encoding.

- Vertices store **1–3 indices into a shared Position pool**; each entry = `(bone, offset, weight)` with **weight quantized to 1/64 in a ubyte** (`sxmd.d:93-100`, `saxs.d:76`). Vertex = Σ boneTransform(offset) × weight.
- One texture per body part (`.SXTX` = TGA, optional 1-bit alpha keyed on pure black); per-model bitmask picks which parts get alpha (up to bit 10 → 10+ body parts).
- **Bones ≤ 32, hard limit** (`sxsk.d:10`, enforced `saxs.d:221`).
- **No creature LOD.** Archaeology: the *building* format MRMM has a per-face `uint lod` field (`mrmm.d:37`) — original Sacrifice shipped per-face LOD — but sacengine **discards all but the highest** (`sacobject.d:4060-4062`).
- **93 `CreatureData` entries**: 50 standard (10 per god × 5), 10 heroes, 15 wizards, plus neutrals and campaign specials.

**Verdict: directly portable, better than hoped.** Ring topology is meshlet-friendly (a body part of ≤64 verts *is* a meshlet), integer-native (8-bit alignment, 16-bit indices, 1/64 weights — our fixed-point requirement is already how the original data is stored). One texture per part maps to per-meshlet texture binding.

## 2. Skeletal animation

**True multi-weight skinning over a rigid hierarchy — but degenerately cheap.** Bones are parent-before-child (`saxs.d:72`); each pose is per-bone quaternion + one root displacement. Vertices blend **≤3 influences** (`sxmd.d:57`); many use 1, joints 2–3.

- **Storage:** per frame, a 12-byte header + **`short[4]` per bone = one quantized quaternion, 8 B/bone/frame** (`sxsk.d:19-24,61-64`). Max frame payload = 32 bones × 8 B = **256 B**.
- **Compilation:** every keyframe baked to world-space matrices at load (`sxsk.d:119-139`), so **runtime pose cost = one array lookup**.
- **Rates:** sim 60 Hz, animation keys at **30 Hz**, each key shown 2 sim ticks. **No interpolation between keyframes** (`renderer.d:1303` TODO). Ships and looks fine.
- **No blending between animations** — hard cuts (`object.frame=0`), masked by **64 authored animation slots per creature** (`animations.d:5-66`): getUp, knocked2Floor, takeoff/fly/land, 5 directional damage anims, 3 deaths, corpseRise…
- **Gameplay events ride keyframes**: per-frame `AnimEvent` tags (attack, shoot, load, cast, grab, foot, sound — `sxsk.d:26-36`) drive damage timing. `foot` parsed but unconsumed.
- GPU skinning (≤32 mat4 uniforms) with a complete CPU fallback that re-evaluates all vertices + normals per frame.
- Attachment points ("hands") = bone index + offset, for weapons/projectiles/spell sparkles.

**Verdict: portable almost verbatim.** Our Transform Loom's rigid hierarchy + optional two-weight skinning covers ~95%; the delta is 3-weight vertices (rare, at joints — clamp to the 2 largest and renormalise; weights are 6-bit so error is sub-quantum for most). Bake quaternion keys → fixed-point matrices at load exactly as they do. **Hard-cut transitions + many authored clips is *the* proven recipe for zero-blend hardware.** Keyframe event tags give audio/damage sync for free.

## 3. Creature deformation beyond skeletal — all cheap uniform tricks

1. **"Bulk" inflation** — a single uniform inflates the whole skinned creature. *Intestinal Vaporization*: victim swells 1.0 → `maxBulk=2.5` over the spell, then **pops** into gibs. Blight mites: target bulk `2.0−0.75^n`, exponential smoothing per tick.
2. **Gibbing** — mesh simply *removed*, replaced with 40 blood-cloud + 200 spray particles + a soul sprite. No damage states.
3. **Dissolving corpses** — alpha fade over 2.5 s after a 1 s delay.
4. **Ghost form** — per-creature alpha/energy uniforms ramped over an animation.
5. **Petrify / slime / freeze** — petrify = shader flag + frozen pose; slime = texture swap **plus animation slowdown by skipping sim updates** (`slowdownFactor = 4^numSlimes`) — **slow-motion for free**; freeze = translucent ice-shell mesh over the paused creature.
6. **Per-instance uniform scale** on every MovingObject, multiplying speed/acceleration too; there's even a `randomCreatureScale` mode giving each creature uniform(0.5, 1.5).

**Verdict: all directly portable — the "disproportionate life" jackpot.** Bulk = one multiplier in the Loom; swell-then-pop-into-particles is enormous character for near-zero cost. Slow-motion-by-tick-skipping costs literally nothing. Petrify/slime are palette/texture swaps.

## 4. Scale — how many on screen

- **No engine-imposed creature cap**; armies bounded by the souls economy (1–9 souls each), not code. Selection groups cap at 12. Modes up to 4v4 (8 wizards + armies).
- **The renderer brute-forces it**: SoA storage **grouped by creature type**, one material bind per body part per type, then one draw call per instance with pose + transform uniforms. **No frustum culling, no LOD, no instancing, no impostors** — two TODOs admit it. Survives because per-creature CPU cost is a pointer lookup + a 2 KB uniform upload.
- Ballpark: dozens visible, low hundreds alive across 8 sides.

**Verdict: take the type-grouped SoA + precomputed poses; do NOT take the no-culling/no-LOD part.** They get away with it on a 2018+ GPU. Good news: nothing in this content *requires* LOD machinery to look right — so our mesh→micro-mesh→splat→glint collapse has full artistic licence.

## 5. Giant creatures — the surprise

**Giants get NO special rendering path at all.** Same 32-bone budget, same pipeline, same animation system. The differences are pure data:
- `zfactorOverride` — per-model scalar on the animation root displacement's z.
- `hitboxType: largeZ / largeZbot / small` — bigger collision envelopes.
- Per-bone 8-corner hitboxes transformed by the live pose give giants accurate animated collision **without any extra system**.
- Flying giants use per-creature pitch limits and 5× slower turn smoothing while airborne.

**Verdict: directly portable — the cheapest possible giants implementation.** One skeleton budget for everything from peasant to Hellmouth; size is per-instance scale + hitbox class.

## 6. Animation ↔ terrain interaction

- **Creatures never deform terrain** — no footprints, no weight dents. The `foot` AnimEvent exists but is unconsumed.
- **Terrain deforms creatures instead**, beautifully cheap: `rotateOnGround = no | sideways | completely` — quadrupeds/tanks pitch/roll to the slope using **two finite-difference ground-height derivatives** along facing and side vectors, folded in with a rate-limited slerp (`state.d:13513-13539`). Bipeds stay upright.
- **Spells deform terrain** in two tiers; the **same function** is evaluated in sim height queries and in the terrain vertex shader — physics and pixels agree by construction (except the drift bug S1 found). Creatures standing on an Erupt get **catapulted**: strength `20·(1−d/42.5)`.

**Verdict: `rotateOnGround` is a must-take** — two fixed-point height taps + smoothed tilt gives quadrupeds enormous life for pennies; a natural Loom node. Skip creature footprints — the source material never had them.

## 7. Spell/effect geometry (brief)

**The star trick: the creature skinning path is reused as a spline renderer.** Lightning bolts, guardian tethers, grasping vines, rainbows, Hellmouth tongue projectiles are tube meshes whose "bones" are N+1 matrices sampled along a curve, fed through `setPose`. Lightning uses 10 segments. Everything else is procedural spheres/boxes/tubes built once, billboard sprites (souls: 16-frame animated, multi-souls orbit in a ring), and particles.

**Verdict: the spline-as-skeleton trick is tailor-made for our Loom's spline node + two-weight skinning** — one tube meshlet, control points from the spline node.

## What we should take — ranked

1. **Precompiled pose tables** (quaternion keys → fixed-point matrices at load; runtime = table lookup). Zero per-frame animation CPU — the whole reason 100+ creatures is thinkable. *Directly portable.*
2. **Hard-cut transitions + rich clip vocabulary instead of blending.** 64 authored slots cover every seam; shipped-game-proven at 30 Hz keys with no interpolation. **Buys us out of a blend unit entirely.** *Directly portable.*
3. **`rotateOnGround` slope tilt** — the single cheapest "alive" trick in the codebase. *Directly portable as a Loom node.*
4. **Bulk inflation + gib-to-particles + tick-skip slow-motion.** One scalar, a particle burst, a modulo counter = swelling, popping, petrify, slime slo-mo. Maximum character per gate. *Directly portable.*
5. **SXSK-style animation compression**: 8 B/bone/frame quantized quaternions, ≤32 bones, 6-bit weights — **an existence proof that our fixed-point budgets fit real AAA-era content.** *Directly portable.*
6. **Keyframe-embedded event tags** (attack/shoot/foot/sound) — gameplay and audio sync ride the animation stream for free. *Directly portable.*
7. **Spline-as-skeleton effect tubes** (lightning/vines/tethers). *Portable if simplified*: spline node + 2-weight skinning, ~10 segments.
8. **Analytic terrain displacement evaluated identically in sim and render** + tiny permanent dent grid. *Portable if simplified.*
9. **Ring-cylinder implicit meshes** — body part ≈ meshlet, topology derived, all-integer. *Portable if simplified* (pre-expand offline; keep the compact on-flash form).
10. **Data-driven giants** — one bone budget for all sizes; giants = scale + hitbox class + zfactor. *Directly portable.*
11. **3-weight skinning** → clamp to our 2-weight hardware (drop smallest, renormalise; sub-quantum error).

**Budget hazards (do NOT copy):** no frustum culling / no LOD / no instancing for creatures (only a desktop GPU forgives it) · per-creature uniform re-upload of ≤32 mat4 each draw (keep poses in shared pose RAM indexed per meshlet instead) · the CPU skinning fallback that regenerates normals per frame · **MRMM per-face LOD is a cautionary tale: LOD as *artist data* rotted — sacengine just ignores it. Automatic screen-space-error collapse is the better design.**
