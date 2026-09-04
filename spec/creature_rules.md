# Zhaozhou Creature & Giant Rules — v1

**Status:** new spec, world-identity wave (run RUN-20260816-0046,
consolidating recons S2 — sacengine creatures — and S3 — Giants: Citizen
Kabuto). Law for the creature asset format, the animation/pose pipeline,
the Transform-Loom vocabulary additions (reparent, scale, heavy gait), the
sim-side "alive" laws (slope tilt, bulk, gibs, tick-skip), and the giant
model including the terrain-class giant seam. Numeric formats defer to
`spec/qformats.md`; terrain interaction defers to `spec/terrain_rules.md`;
container bytes to `spec/cartridge.md`.

Charter anchors: §10 (meshlets, two-weight skinning), §9 (The Measure),
§26 refusals, §29-6 (one semantics), Phase 9 gate ("no CPU per-limb draw
submission").

Donor evidence: S2 (SXMD/SXSK: ring-cylinder parts, ≤32 bones, 8 B/bone/
frame quantized quats baked at load, hard-cut clips over a 64-slot
vocabulary, keyframe event tags, giants as pure data); S3 (Kabuto: scale as
gameplay variable, weight sold by choreography + camera + response, and the
cautionary finding — the playable giant was "sluggish, a big target" and
played worst).

---

## 1. Creature geometry

### 1.1 Authoring format (tool-side, ring-cylinders)

A creature is a list of **body parts**; each part is a generalized cylinder
— stacked vertex rings zig-zag-stitched into triangles, with optional
top/bottom fan caps and an 8-bit angular alignment per entry (→ U texcoord).
This is the donor's SXMD shape and it is all-integer by construction. The
ring format is a **tool input** (SW.TOOLS.ASSET): the asset compiler
expands rings offline into ordinary meshlets. **Hardware never sees rings**
— it sees meshlets, clip banks and poses. The ring source travels in the
cartridge only as provenance if at all; the compiled page (§5) is what
loads.

### 1.2 Compiled constraints (frozen)

- Body part ↦ one or more meshlets (charter §10: ≤64 unique vertices,
  ≤96–126 triangles, one primary material each); parts larger than a
  meshlet are split by rings.
- One texture page per part; optional 1-bit alpha key. **SUPERSEDED 2026-08-27**: the page carries a FORMAT tag and may be CLUT8 or direct colour (RGB565) -- see the amendment at the end of this file. Filtering REQUIRES direct colour, so a CLUT8-only page made bilinear permanently unreachable.
- **≤ 32 bones per creature** (hard, donor-proven sufficient from peasant
  to Hellmouth-class).
- Vertices carry **≤ 2 bone influences** (hardware law, charter §10);
  3-influence source vertices are clamped at compile (§3).
- Attachment points = bone index + local offset (weapons, projectiles,
  spell sparkles, rider/carry sockets).

## 2. Animation

### 2.1 Storage (frozen; the Q formats are frozen — qformats §7.6, C1)

Clips ship compressed, SXSK-style:

- per frame: root displacement (3 × fx16, 12 B) + `bone_count` × quantized
  quaternion (`quat16`, s16[4], **8 B/bone/frame** — qformats §7.6).
  32 bones ⇒ ≤ 268 B/frame.
- keys at **30 Hz**, each key shown 2 sim ticks, **no interpolation** —
  donor-shipped and it reads perfectly at 240p; a later geomorph between
  keys is a presentation nicety, not v1.
- **hard-cut transitions only** — there is no blend unit, deliberately.
  What buys the seams is a **rich clip vocabulary**: 64 authored slots per
  creature (getUp, knocked2Floor, 5 directional damage anims, 3 deaths,
  corpseRise, takeoff/fly/land, …). The donor proves 64 slots cover every
  transition a battle produces (S2 §2).
- **keyframe event tags** ride the clip stream: {frame u16, event u8,
  param u8}, ≤4/frame — attack, shoot, load, cast, grab, foot, sound.
  Damage timing, audio sync and the giant impact chain (§6.2) consume
  these in the sim. Free synchronization, donor-proven.

### 2.2 Pose pipeline (DECISION — bake-at-load rejected, decode-on-fetch chosen)

The donor baked every keyframe to matrices at load. Costed for us:
3×4 fx16 matrix = 48 B/bone ⇒ 1,536 B/pose at 32 bones; a 64-slot bank at
~20 frames/clip = 1,280 poses ⇒ **1.97 MB per creature type baked** vs
**343 KB compressed** (5.7× ≈ ×6). Ten resident types: 19.7 MB baked =
82% of the whole 24 MB meshlet+LOD+animation pool — unacceptable; 3.4 MB
compressed = 14%.

**Decision:** VRAM stores clips compressed; a dedicated block —
**GEOM.POSE** (new, SPECIFIED) — decodes (type, clip, frame) → 32 bone
matrices into a shared **decoded-pose cache** on demand. Armies are
type-grouped: instances of one type playing one clip at one tick share one
decoded pose, so the per-frame working set ≈ distinct (type, clip, frame)
tuples ≈ ≤128 ⇒ 192 KiB cache (VRAM hot region, M10K staging for the pose
in use). Decode cost: quat→matrix ≈ 12 multiplies/bone; 128 misses × 32
bones × 12 ≈ 49k multiplies/frame — noise against a DSP budget. This also
satisfies the Phase-9 gate: poses live in shared pose RAM indexed by
instances, the ARM never uploads per-limb matrices (S2's "budget hazard"
list, honoured).

Quantized-quat decode numerics (lane format, rounding of the 9-product
matrix formula) ~~are **proposed for a qformats amendment** and must be
frozen before GEOM.POSE reaches REFERENCE_COMPLETE — flagged, not decided
here (QFMT_VERSION change control, qformats §13).~~ **Frozen 2026-08-17:
qformats §7.6 (amendment C1, QFMT_VERSION 2)** — S 1.0.14 hemisphere-
canonical lanes, one rescale(·,11) per matrix element, no renormalisation,
declared bounds measurement-cited to the creature lane commit `bd1c733`.
GEOM.POSE is REFERENCE_COMPLETE on that evidence.

## 3. The 3→2 weight clamp — claim CORRECTED, gate specified

Recon S2/S3 said dropping the smallest of 3 weights is "sub-quantum because
weights are 6-bit". **That claim is wrong as stated and is corrected here.**
Weights are 1/64 quanta, but the error of dropping w₃ is not a quantization
error:

```
v  = w₁T₁x + w₂T₂x + w₃T₃x
v' = (w₁/(w₁+w₂))T₁x + (w₂/(w₂+w₁))T₂x
|v − v'| ≤ w₃ · |T₃x − blend₁₂x|  =  w₃ · D
```

Worst legal w₃ = 21/64 ≈ 0.33 (equal thirds). With D ≈ 4 cm at an elbow-like
joint, worst error ≈ **13 mm — three thousand quanta, not sub-quantum.**
Projected: at Z60 (384 px, 60° FOV, ≈333/d px per metre), 13 mm ≈ 2.2 px at
2 m, 0.4 px at 10 m, invisible beyond. Real distributions could not be
measured (the donor's .SXMD assets are not in the engine repo — S2 header);
so the spec gates instead of guessing:

**Compile gate (SW.TOOLS.ASSET):** renormalize into 1/64 quanta
(round-half-up, sum forced to exactly 64 by adjusting the largest weight);
compute the exact per-vertex drop error `w₃·|p₃(f) − p₁₂(f)|` over every
frame of every clip; **warn** above 1% of the creature bound radius,
**reject** above 3% (author must re-rig the vertex to 2 influences).

## 4. Loom vocabulary additions (GEOM.LOOM) and sim-side "alive" laws

### 4.1 Loom nodes (hardware, specified now, built Phase 9)

- **SCALE** — uniform per-node fx16 scale multiplying the subtree.
  Uses: bulk inflation, growth-as-scale (§6.4), per-instance size variety
  (the donor's uniform(0.5, 1.5) trick).
- **REPARENT** — a node's parent id is retargeted (epoch/generation
  checked), with a keep-world-transform flag. The graph evaluation cost is
  unchanged — reparenting is a pointer swap, which is why grab/eat/carry/
  ride/horn-larder are near-free (§6.1). The ABI command shape
  (`ReparentTransform`) is **proposed**, to land with an abi-gen
  regeneration by the ABI owner — not edited into commands.zidl here.
- **GAIT** (existing node, enriched) — phase clock with an
  anticipation/impact/settle envelope for heavy walkers. Presentation only:
  the envelope shapes bone offsets; it emits NO events.

### 4.2 Sim-side laws (Form `sim` domain — no new hardware)

- **Impact event chain (heavy gait):** the sim owns gameplay truth (charter
  §29-8), so impacts are sim-authored: the clip's `foot`/`attack` event tag
  fires in the sim, which issues SurfaceStamp (crater ring), TerrainField
  (shockwave), particle spawns (debris/dust) and a camera-shake
  presentation cue. Hardware never emits gameplay events. "Snappy controls,
  heavy consequences" (S3): input latency is never traded for weight —
  weight lives in the world's response.
- **`rotateOnGround` slope tilt:** two extra `column_query` taps (along
  facing and side), finite-difference slope, rate-limited tilt slerp;
  modes no | sideways | completely (bipeds stay upright). The cheapest
  "alive" trick in the donor (S2 §6): 2 taps + a clamp per creature tick.
- **Bulk + pop:** bulk rides the SCALE node (exponential smoothing toward a
  target); crossing a species' pop threshold gibs the creature — mesh
  removed, PART.SPAWN burst (blood cloud + spray + soul cue). No damage
  states, donor-proven.
- **Tick-skip slow-motion:** slime/petrify/freeze scale an entity's sim
  update interval (update every 4ⁿ ticks); petrify additionally freezes the
  pose (clip clock stops) and swaps palette; freeze overlays a translucent
  shell form. Costs a modulo counter.

## 5. Cartridge pages (additive; layouts freeze with SW.TOOLS.ASSET at Phase 12 entry)

- **kind 8 — creature form:** compiled part table (part → meshlet ids,
  texture page, alpha flag), bone hierarchy (parent-before-child, ≤32),
  attachment points, hitbox class + per-bone 8-corner hitboxes, LOD ladder
  refs (microforms are compiler-generated per charter §9 — per-face
  authored LOD is REFUSED, see §7).
- **kind 9 — clip bank:** §2.1 records: clip directory {slot_id u16,
  frame_count u16, event_count u16} + frames + event tags. Consumed by
  GEOM.POSE (hardware) and by the sim clip clock (same bytes — one truth).

## 6. Giants

### 6.1 Giants are data (donor law, adopted whole)

No special rendering path. A giant = the same ≤32-bone creature with:
per-instance root SCALE, a hitbox class (largeZ/largeZbot/small +
per-bone live-pose hitboxes — accurate animated collision with no new
system), slower turn smoothing while airborne, heavier gait parameters.
Reparent verbs give the character wins: grab/eat/throw a unit (reparent
under hand/mouth node), the horn-larder (reparent under a horn socket —
a visible trophy rack for near-zero cost), riding (reparent under a back
socket).

### 6.2 Weight is choreography + response, not latency

Movie-monster staging (wind-up, impact, aftermath) via clip vocabulary +
event tags; craters/debris/shake via the §4.2 impact chain; giant-scale
camera packages (mouth-cam, under-foot-cam) are transform nodes — free.

### 6.3 The giant is NOT a symmetric playable faction — REJECTED

Giants: Citizen Kabuto's own evidence: the giant "read magnificently and
played worst — sluggish, a big target" (S3 §A1). The giant enters the
wizard loop as a summonable/awakened entity, or as the asymmetric second
player in Duo (1 giant vs 1 commander, balanced by headcount not stat
mirrors, the donor's 5-3-1 lesson). Revisit only with playtest evidence.

### 6.4 Growth-as-scale, honestly costed

Mechanically near-free (root SCALE + speed/acceleration multipliers).
The honest cost is **fill in split-screen**: a max-size giant near the
other player's camera can cover most of a 256×192 view (49,152 px), and
its owner's own view sees its limbs constantly. The Measure already owns
this: growth raises the giant's projected error mass, and the presentation
contract must declare a giant tier whose fragment tokens reserve that
fill. Numbers are scenario bounds, not promises — declared per content
tier at Phase 11, NOT frozen here.

### 6.5 Terrain-class giant — the seam, specified; prototype before silicon

The novel flagship (S3 §B4): a giant whose torso is walkable terrain.

- **Format seam:** an island patch page with header flag `body_patch = 1`
  (terrain_rules §2.1) is bound to a Loom node instead of a world placement.
- **Query seam:** `column_query` on a body patch transforms the query point
  into node-local space first (inverse rigid transform + uniform inverse
  scale — the node driving a body patch MUST be rigid + uniform scale, no
  shear, packer- and validator-enforced), then runs the standard lattice
  law unchanged; the result's normal/velocity are rotated back out.
- **Render seam:** the patch tessellates as ordinary terrain whose
  transform comes from the Loom node; the governor treats it as terrain
  (screen-error LOD), which is precisely why giant-fills-screen and
  giant-on-horizon need no special case.
- **Status: PROTOTYPE-BEFORE-SILICON.** Sim + ZRef/ZEmu first (Phase 9+
  software), no RTL assumptions may cite it before a prototype capture
  exists. The mud-shepherd resurrection (terrain stands up as creature)
  layers on this same seam later — specified as an intent, not scheduled.

## 7. Refused / deferred (with reasons)

- **Symmetric playable giant faction** — refused (§6.3, donor evidence).
- **No-culling / no-LOD creature rendering** — refused; the donor survived
  it on a 2018 desktop GPU only (S2 §4). Our ladder (mesh → micro-mesh →
  splat → glint) is mandatory.
- **Per-face artist-authored LOD** — refused. The donor's MRMM per-face LOD
  rotted and sacengine discards it (S2 §1). LOD is compiler-generated
  screen-space-error collapse (charter §9); artists author forms, never LOD
  faces.
- **Creature footprints deforming terrain** — deferred indefinitely; the
  donor never consumed its `foot` events for terrain, and our stamp lane
  makes it *possible* later without new hardware. Not spec'd, not budgeted.
- **Animation blending unit** — refused; hard cuts + vocabulary (§2.1).
- **Per-creature uniform re-upload of 32 matrices per draw** — refused
  (donor budget hazard); poses are cache-resident and indexed (§2.2).
- **Second unrestricted TMU** for creature detail — refused, charter §26.

## 8. Test plan (obligations)

1. GEOM.POSE decode vs zref bit-exactness once quat numerics freeze;
   pose-cache determinism (same tuple set → same content, any order).
   [Delivered in the reference lane: `tests/geometry/creature_core.cpp`
   §1–§2 (decode anchors + the 3,600-rotation sweep vs a double oracle) and
   §7 (pose-cache determinism, eviction, bad-id no-op), commit `bd1c733`.
   The RTL-vs-zref leg runs at Phase 9 with the directed suite.]
2. Clamp-gate goldens: synthetic 3-weight rigs at known D and w₃ — the
   compile error metric must equal the analytic bound.
3. Reparent epoch safety: stale-generation reparent is a safe no-op.
4. Body-patch query transform round-trip: node-local query == world query
   under rigid + uniform-scale binding, exact.
5. Event-tag replay: sim impact chain fires identically in capture replay.

## 9. Not decided here (and what it blocks)

- ~~Quantized-quat lane format + decode rounding — blocks GEOM.POSE
  REFERENCE_COMPLETE (qformats amendment required first).~~ Decided
  2026-08-17: qformats §7.6 (C1); GEOM.POSE promoted REFERENCE_COMPLETE
  (ledger maturity_log pins `bd1c733`).
- `ReparentTransform` ABI wire shape — blocks nothing until Phase 9
  integration; lands via abi-gen with the ABI owner.
- Giant content-tier fragment reservations — Phase 11 presentation
  contracts.
- Kind 8/9 byte-exact layouts — freeze at SW.TOOLS.ASSET (Phase 12 entry);
  the semantic fields above are the contract.
- Mud-shepherd scheduling — intent recorded (§6.5), unscheduled.

---

# AMENDED 2026-08-26 (RUN-20260826-1615, Zixxtrixx production pass)

These are AUTHORING-FORMAT amendments. **No hardware contract changed** — the
compiled product is still ordinary meshlets with at most two bone influences,
and GEOM.SKIN already specified the blend.

**1.1 ring format, three additions**

* `RingSpec` gains `{b0, b1, w0}` — per-ring bones and the first one's weight in
  1/64 quanta. `w1 = 64 - w0` is structural.
* `RingSpec` gains `cx, cz` (ring centre offset) and `rx, rz` (elliptical
  radii; both zero means circular and uses `radius`).
* `RingPart` gains `chain`. A chain part's rings carry their own bones and are
  authored in **creature-global bind space**; one part spans a whole bone chain
  as a single continuous surface with no internal caps.

**1.2 correction of a false citation.** `zref_creature.hpp` described one bone
per part as "(donor law)". It is not: the donor is **34.92% multi-bone**
(measured). Independently, `build_ring_part` had never emitted a two-bone blend
— every vertex was `{bone, bone, 64}` — while the runtime, the hardware contract
and the 3->2 clamp gate all implemented blending correctly. The blend existed
everywhere except where vertices are made.

**1.2 texture page.** `RingPart` and `Meshlet` gain `page`; `CreatureType` gains
`page_set`. `page == 255` means untextured and falls back to the flat material
colour.

**2.1 clip bank.** `Clip` gains `interpolate`. When set, the decoder blends
between consecutive keys at the half-tick with a normalized integer quaternion
lerp. **Presentation only** — the sim clock, event frames and every gameplay
consequence still run on the authored 30 Hz keys. Default off.

**Compile-time validators added:** chain bone indices in range, `w0 <= 64`, caps
restricted to chain ends.

**Not a creature law, but it governs creature work:** the 256-colour rule is a
GIF-export constraint and must never shape a creature. `SceneSubject::full_colour`
exempts a subject from it.

---

# AMENDED 2026-08-27 — the texture page carries a FORMAT

## 1.2 supersedes "One texture (CLUT8 page) per part"

**A creature page may be CLUT8 or a direct-colour format. The page carries a
format tag; CLUT8 remains available and remains the default.**

### Why the old line had to go

§1.2 froze the creature page as CLUT8. That was never a hardware limit — it was
a narrower option than the silicon has, and it had become load-bearing in the
wrong direction:

* **TEXTURE.TMU decodes five formats today**, CLUT8 and **RGB565** among them,
  with mip selection and wrap/clamp/mirror, and by charter §26 it is the ONLY
  sampler in the machine. Nothing had to be built to make this legal.
* **`spec/stars_and_flares.md` §1 is a hard law: "nearest sampling mandatory —
  bilinear must never touch a palette."** Therefore **filtering requires direct
  colour.** A CLUT8-only creature page does not merely prefer nearest sampling;
  it makes bilinear *unreachable*, permanently, for every creature in the game.
  The first creature's surface reads as pixelated for exactly this reason.
* The freeze was cheap to lift and cheap to lift NOW: kind-8 byte layouts do not
  freeze until Phase 12 entry (§9), so this amendment costs a spec edit and
  **zero silicon**.

### What changes

* The page gains a **per-page format tag**. `RingPart`/`Meshlet::page` and
  `CreatureType::page_set` (added 2026-08-26) are unchanged; the format rides
  the page, not the part.
* **CLUT8 stays.** It remains correct where a creature wants hard indexed
  colour, palette animation, or the smallest possible page. It is a choice now
  rather than the only option.
* **Direct colour unlocks bilinear and mips together**, which is the actual cure
  for a hand-drawn surface reading as pixels.
* 1-bit alpha keying survives in both.

### The rule that comes with it

**Choose the format from what the surface needs, not from habit.** A palette
buys indexed tricks and costs filtering. Direct colour buys filtering and costs
the tricks. Neither is the default answer for every creature.

### What this amendment does NOT do

It does **not** touch lighting. Per-vertex (Gouraud) creature lighting is a
SEPARATE and genuinely unbuilt thing: the consuming end exists
(`RASTER.FRAGMENT`'s `frag_vert_rgb_i` and SHADE_MOD), the frozen fog law in
`spec/qformats.md` §8 already assumes "the ordinary Gouraud path", but the
PRODUCING end does not — `GEOM.PROJECT` emits no colour lane and
`GEOM.SETUP.md` states plainly that "the gradients are NOT built", with two
reasons that are arguments rather than omissions (a plane-form gradient would
not be bit-exact with the oracle's per-scanline barycentric re-division, and
there is no consumer). That gap is **already on the ledger and is not opened by
this amendment.** Smooth normals in the software reference renderer are an
authoring-side change; smooth normals in silicon are a costed proposal that
belongs to the hardware lane.

*Evidence and the full change census:
`Upheaval/creature/Zixxtrixx/PRESENTATION-V2-PLAN.md`.*

---

# AMENDED 2026-08-27 (RUN-20260827-2140) — the vertex carries a packed NORMAL, and the reference renderer is Gouraud

## 1.1/1.2 gain a vertex normal lane

**Each `SkinVertex` (and its compiled meshlet twin) carries a packed
bind-space smooth normal, s8×3 (S1.7, 127 = 1.0). (0,0,0) means "no
normal" and selects flat face shading, so every pre-amendment asset
renders bit-identically.** Charter §10's meshlet field list has carried
"compact normal and UV encoding" since the start; kind-8 layouts do not
freeze until Phase 12 entry (§9), so — like the format tag above — this
costs a spec edit and an asset-tool change, zero silicon TODAY.

* **Generated, not authored**: `compile_creature` computes area-weighted
  smooth normals over the finished meshlet set, keyed on the exact bind
  position so seam duplicates (the u=255 wrap vertex) and meshlet-boundary
  duplicates receive the SAME packed normal — a lighting seam cannot open
  where the surface is closed. The micro rung's normals are recomputed
  from micro topology, never copied (LOD is hardware's job; the compiler
  derives).
* **s8×3 over octahedral**, by the V2 brief's own argument: no decode
  hardware, simple fixed-point dots; vertex grows 12 → 15 bytes against a
  ~45% authored-vertex-count cut in the same run.

## 2.x — the per-vertex lighting LAW (the missing reference model, now built)

`spec/qformats.md` §8 (frozen) has the fogged colour "riding the ordinary
Gouraud path"; `design/blocks.yml` gives GEOM.PROJECT "projection +
LIGHTING" and GEOM.SETUP "edge coefficients, GRADIENTS"; GEOM.SETUP.md
records the gap: *"until that exists there is nothing to be bit-exact
against."* The reference model now exists, in
`reference/src/zcreature/creature_sim.cpp` + `zrender/rast.cpp`:

1. ~~**Light pullback, per bone**~~ and
2. ~~**Lambert = the blend of the two bones' CLAMPED responses**~~ --
   **BOTH SUPERSEDED 2026-09-04.** See *2.x.1* immediately below. The text
   is struck rather than deleted because RTL and contracts were written
   against it and a reader needs to recognise the old shape to know they
   are looking at it.

   The superseded law was: `L_b = R_b^T · L / bulk_scale` per bone, then
   `lam = (w0·clamp(N·L_b0) + w1·clamp(N·L_b1) + 32) >> 6` -- the N5
   probe's option (b), adopted with the note *"no renormalisation
   anywhere, which is the cheap form the silicon increment would build."*
### 2.x.1 -- the normal law, corrected against the live reference

**Owner, `reports/CREATURESANDLIGHTS`:** *"The live reference has since been
corrected: it transforms both normals, blends the normal vector, renormalises
that result, and only then takes Lambert. That repair removed bright patches
around mixed-weight joints. The live reference should become the law."* And at
the close: *"Make the live blended-and-renormalised normal law authoritative."*

**Verified against the code, not taken from the summary.**
`reference/src/zcreature/creature_core.cpp::skin_normal_lambert` does exactly
this and the superseded text above describes something it does not do:

    n[row] = w0 * (A_row . N) + w1 * (B_row . N)     // blend the VECTOR
    range-reduce until max|n| < 2^30                 // direction preserved
    mag    = isqrt_u64(n.n)                          // ONE renormalisation
    dot    = n . L                                   // Lambert AFTER the blend
    lam    = dot <= 0 ? 0 : min(65536, (dot + mag/2) / mag)

So the spec had a ratified law its own oracle did not implement. Anything built
to the struck text would have been bit-wrong against every golden capture, and
would have shown the bright patches at mixed-weight joints that the reference
change removed.

**THE LAW IS NOW:**

1. **Transform the normal by BOTH bones and blend the VECTORS**, keeping the
   full weighted direction. The common `1/64` and uniform-bulk factors cancel
   in the normalisation, so no pre-normalise rounding is introduced.
2. **Range-reduce, then renormalise ONCE** -- `isqrt_u64` of the sum of
   squares. The shift is applied to every lane equally, so it changes the
   magnitude guard and not the direction.
3. **Lambert last**, `dot <= 0` clamping to zero and the quotient saturating at
   `0x10000`.

**What this costs, stated plainly, because the struck law was chosen for being
cheap.** The old form needed two dots and a clamp per light and left
`GEOM.SKIN`'s datapath untouched. This one needs the normal transformed by two
bones -- 18 multiplies -- plus a square root, per vertex. That is a real
increase and it was the owner's call with the reason given: the cheap form
produced visible bright patches.

**The per-light repetition in the reference is NOT law.** Owner, same document:
*"The current reference repeatedly calls `skin_normal_lambert` for key, fill and
point light. That means it effectively repeats the transformed-normal work for
each light. The hardware should not reproduce that structure."* The transform,
blend and renormalisation happen **once per vertex**; each light then costs only

    lambert = max(0, dot(world_normal, light_direction))

This distinction is the difference between one square root per vertex and three,
and it is the one place where being bit-exact with the reference's *structure*
would be wrong. Bit-exactness is owed to the reference's **result** for a given
light, not to the number of times it recomputes the normal.

3. **Smooth/face blend knob**: `kSmoothMixNum`/1024 (currently 819) parts
   smooth, remainder the face Lambert — the owner's control over how much
   hand-cut read survives.
4. **The per-channel rig composes per corner** (unchanged rig), and the
   raster interpolates the three colour lanes with the SAME per-row model
   as depth/alpha/UV: **row starts re-evaluate the full barycentric form
   (one §4 division per lane per row); pixels step by the affine
   x-gradient.** A setup-emitted `(c0, dc/dx, dc/dy)` plane is REJECTED —
   it would not be bit-exact with this oracle; the row walker owns the
   division, in RTL as in the reference.

### What the silicon increment now costs (the hardware lane's docket, stated once)

* **GEOM.VDECODE**: unpack s8×3 → fx (a shift; the block is a stub and
  gains the lane when designed at all).
* **GEOM.PROJECT lighting stage — RECOSTED 2026-09-04**, because the bullet
  that stood here priced the law struck in 2.x.1 and is no longer the
  design. It read: *"per vertex, 2x2 fixed-point dots (key+fill x two
  bones' pulled-back dirs are per-INSTANCE constants computed once, 12
  multiplies per bone per frame) + the w0/w1 blend + the rig's 3 saturating
  mul-adds — small beside its 31-stage divider."* That is the pullback
  form: the normal never moved, so nothing per-vertex was transformed.

  The blended-and-renormalised law moves the normal, and the cost moves with
  it. Counted off `skin_normal_lambert` rather than estimated:

  | | per vertex | per light |
  |---|---|---|
  | transform N by both bones (two 3x3 mat-vecs) | 18 mul | — |
  | blend the vectors (`w0·na + w1·nb`, 3 lanes) | 6 mul | — |
  | sum of squares | 3 mul | — |
  | renormalise | **1 isqrt** | — |
  | `dot(n, L)` | — | 3 mul |
  | `dot / mag`, round-half-up, saturate | — | **1 divide** |

  So roughly **27 multiplies and one square root per vertex**, plus three
  multiplies and one division per light, against the struck law's six
  multiplies per light and no per-vertex work at all. It is a real
  increase and it was the owner's call, with the reason given: the cheap
  form produced bright patches at mixed-weight joints.

  **The square root is per VERTEX, not per light** — see 2.x.1. Copying the
  reference's structure, which calls `skin_normal_lambert` once per light,
  would triple it for no change in the answer.
* **GEOM.SETUP + RASTER.FRAGMENT**: three more interpolated attributes on
  the row-walker division the contract already names as "what lands it";
  `frag_vert_rgb_i` and SHADE_MOD are already built and verified.
* **Payoff measured in this run**: with Gouraud + filtered direct colour,
  the authored model dropped 3,680 → 2,076 triangles with no visible cost
  at 240p — the interpolator increment BUYS BACK more geometry than it
  spends.

WHEN this lands in RTL is the owner's scheduling decision. The reel is the
oracle it will be verified against; until then the console shows flat
shading and the site's clips honestly come from the reference oracle, as
they always have.

---

## AMENDMENT (2026-08-28) — the continuous body atlas: page V ranges and per-page mode words

The 2026-08-27 format-tag amendment made a direct-colour page lawful. Building
the first one (Zixxtrixx's 128×256 body atlas, T4 of the presentation-v2 plan)
needed two more small format facts, both authoring-side, both zero silicon:

* **A part may claim a V RANGE of its page.** `RingPart` gains `v0`/`v1`
  (default 0..255 — every existing part is bit-identical): the part's rings
  span page rows v0..v1 instead of the whole axis, so several parts can share
  ONE continuous atlas (U = circumference, V = nose-to-tail) with the shared
  seam ring's V agreeing on both sides. This is what deletes the V restart at
  the head/body junction and buys a longitudinally continuous painted surface.
* **Pages in one set may differ in shape, so the mode word is per page.** The
  TMU mode word is per-BIND by contract; a creature's page set records one
  mode per page (`DirectPageSet::tile_mode`, empty = the shared word as
  before). Zixxtrixx: the 128×256 atlas (LOG2W=7, LOG2H=8 — rectangular
  power-of-two, legal since the TMU was built) beside two 64×64 fin pages.
  Fins are separate pages BY RULE: bilinear + mips bleed across atlas
  neighbours, so unrelated surface regions must not share one atlas.
* The upstream req_lod texel-density Measure generalises to the page's own
  log2 dims (the 64×64 constant was a special case; floored-division identity
  keeps the old pages bit-identical).
