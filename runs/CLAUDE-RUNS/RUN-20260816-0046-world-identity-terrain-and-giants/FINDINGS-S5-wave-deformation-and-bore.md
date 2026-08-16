# FINDINGS-S5 — Wave Deformation, Volcano, and Bore

**Recon S5, RUN-20260816 wave. Read-only recon of `C:\programmieren\sacengine` (D source, submodules absent), `tg-2/dagon` fork (web-fetched, labelled), zhaozhou frozen specs, and the shipped-game record (web, labelled). Corrects and extends S1/S4. Persisted by orchestrator.**

**Headline: the frozen Field IR CAN express the signature rubbery travelling wave** — every primitive it needs (DIST2, CURVE time envelopes, SIN/COS raised cosines, RCP, MAD) exists in the frozen opcode table, and Q16.16 resolution is orders of magnitude beyond what the effect needs at 240p. The two real pressure points are **sizing, not expressiveness**: a faithful single-program Erupt sketch-counts at ~36–38 instructions, over the **provisional** earth ceiling of 32 (fits the global 64; the ceiling is an explicitly tunable spec constant, or the effect phase-splits into two ≤32 programs), and the **4-tables-per-program limit (V5)** is the binding constraint that forces the trig to be recomputed inline. **No new opcode is required.**

**Secondary headline: Bore shipped, the hole was real, and the shipped map format proves it — bit 15 of every HMAP height sample is a per-vertex void flag** (`maps.d:24`). S4's claim that "Sacrifice's single heightfield could only fake a pit" and that the format "could not express" a hole is **wrong** and is corrected loudly in §4.

---

## 1. The wave mathematics (primary question)

All from `C:\programmieren\sacengine\source\state.d` unless noted. Time unit: seconds, `t = frame/updateFPS`, `updateFPS = 60` (`state.d:13`). Distance unit: world units ≈ metres; the terrain lattice is 10 u/cell. `d` = horizontal distance from effect centre.

### 1.1 Erupt (`state.d:3010-3069`) — the full signature effect

Constants (`state.d:3019-3023`): `range=50, height=15, growDur=4.2 s, fallDur=0.15 s, waveRange=90, waveDur=1.0 s, reboundHeight=2.0`.

**Phase A — rise, 0 ≤ t < 4.2 s** (`state.d:3034-3046`):

```
scale(t) = t/4.2                                  (linear 0→1)
shape(d) = 0.6·(1 − d/50)                          for d < 50
         + 0.2·(1 + cos(π·d/40))                   for d < 40   (raised-cosine cap)
disp = shape(d) · 15 · scale(t)
```

A 15 m dome (cone + cosine cap, peak `shape=1.0` at centre) grows linearly over 4.2 s. Live during casting too (`eruptCastings` are summed, `state.d:26802-26804`).

**Phase B — collapse, 4.2 ≤ t < 4.35 s** (`state.d:3038-3039`): `scale = 1 − (t−4.2)/0.15` — the 15 m dome crashes to zero in 150 ms (~100 m/s vertical). At the same instant `eruptExplosion` fires: catapult throw `20·(1−d/42.5)` on creatures within 42.5 u, 64 ballistic debris chunks at ~30 m/s, 200 rock particles, ScreenShake (`state.d:17262-17313`).

**Phase C — rebound dip + travelling ring, 4.2 < t < 5.2 s** (`state.d:3047-3057`):

```
// global elastic rebound (the ground overshoots BELOW rest):
rp(t) = 0.5·(t−4.2)/0.15              for t < 4.35        (fast ramp to ½)
      = 0.5 + 0.5·(t−4.35)/0.85       for t ≥ 4.35        (slow ramp to 1)
disp −= (1 + cos(π·d/90)) · sin(π·rp(t)) · 2.0             for d < 90
// peak dip at centre: −4 m at t = 4.35 s, back to 0 at t = 5.2 s (half-sine in time)

// the travelling wave crest:
p       = (t − 4.2)/1.0                                    (wave progress 0→1)
R(t)    = 90·p                                             (front radius: 90 m/s)
w(p)    = (0.15 + 0.65·(1−p)²) · 50                        (half-width: 40 m → 7.5 m)
A(p)    = 0.5·0.4·2 · 15·(1−p²)  =  6·(1−p²) m             (crest amplitude → 0)
wavePos = |d − R(t)| / w(p)
disp   += 0.2·(1 + cos(π·wavePos)) · 15·(1−p²)             for wavePos < 1
```

**The structure, named:** one **travelling raised-cosine crest** (front advancing at 90 m/s, narrowing 40→7.5 m, amplitude decaying quadratically 6→0 m) **on top of one global half-sine rebound dip** (down to −4 m, one half-cycle). A fixed point on the ground therefore experiences: slow rise, fast fall, one dip below rest, one crest passing — **about 1.5–2 excursions total. It is not a multi-ripple damped oscillator; the "liquid" reading is engineered from a single travelling crest plus a single elastic overshoot.** Far cheaper than a physical wave sim, and entirely closed-form.

**Return to rest:** exactly zero at t = 5.2 s by construction — `A(1) = 0` and `sin(π·1) = 0` — then the effect struct is removed (`totalFrames = 312`, `state.d:3026`; `updateErupt` returns false, `state.d:17343`). Continuous, no snap. **Erupt leaves no permanent mark** — the scar is a literal `// TODO: scar` twice (`state.d:17257,17312`).

**Gameplay coupling to the wave:** a stun ring chases the crest — active while `50 < R(t) < 75`, band half-width `1.5·90/(1.0·60) = 2.25 m` per tick (`state.d:17328-17341`).

### 1.2 Quake (`state.d:4802-4838`) — same grammar, small and fast

`waveRange=50, waveDur=0.5, waveHeight=1.5, growDur=0.05, fallDur=0.15, reboundHeight=2.0`. No mound phase; from t=0: the same rebound term (dip to −4 m at centre) and the same ring with `R(t)=100t` (100 m/s), `w` shrinking 40→7.5 m, crest amplitude `2·1.5·(1−p²)·min(1, t/0.05)` → 3 m peak, all over in 0.5 s. Known defect note at `state.d:4825` ("displacement at time 0 should be constant zero"). Stun ring analogous at `state.d:23897-23911`.

### 1.3 Everything else that moves terrain vertically (complete list)

The renderer's own enumeration is the proof of completeness (`dagonBackend.d:2049-2056`): permanent layer + testDisplacements + eruptCastings + erupts + quakes. Vertically-acting contributors:

- **Bombardment** (`state.d:3811-3827`): permanent spherical-cap dent `−1.2·sqrt(1 − d²/15²)` m, written once into the permanent grid (`state.d:6795-6805`). No transient.
- **Volcano** (`state.d:4054-4113`): persistent stencil — §2 below.
- **TestDisplacement** (`state.d:4926-4932`): `2.5·(sin(0.1x+t) + sin(0.1y+t))` — a debug **standing spatial sinusoid** (λ ≈ 62.8 m, ±5 m, phase speed 10 u/s). Not gameplay, but it is the pure "membrane" rig, and the donor thought it worth a dedicated GPU pass (`terrain2.d:346`).
- Nothing else. ScreenShake (`state.d:4909-4924`) moves the camera, not terrain.

### 1.4 Summation of simultaneous deformations

- **Sim:** `Displacement.opCall(i,j)` sums permanent grid + all testDisplacements + all eruptCastings + all erupts + all quakes, plain float adds (`state.d:26789-26812`).
- **GPU:** additive blending `glBlendFunc(GL_ONE, GL_ONE)` into the displacement texture (`terrain2.d:564-565, 585-586`, dagon fork — web-fetched, see §1.5).
- **Ours:** `terrain_rules.md` §3.4 sums live field lanes in command order with saturating fx_add — same law, deterministic. **Match.**

### 1.5 Rendered surface vs sim height — established properly (S1's drift claim confirmed)

The render-side code is **not in the sacengine repo** — the `dagon`/`dlib` submodules are unfetched (commit `6c41ec1`). Fetched from the fork (**web research**: `tg-2/dagon`, branch `tg-fork`, `src/dagon/graphics/materials/terrain2.d`).

- The GPU **re-implements each displacement formula in GLSL** as a fullscreen fragment pass writing a **256×256 R32F texture — exactly one texel per 10 m lattice vertex** (`pos = (0.5·(position+1)·256 − 0.5)·10`, `terrain2.d:389,463`), NEAREST-filtered (`terrain2.d:300-301, 528-529`). The terrain vertex shader adds `texture(displacementTexture, coord).r` to each vertex's z (`terrain2.d:85-86`).
- **Therefore the rendered surface is NOT a higher-resolution evaluation.** Sim and render both evaluate f(x,y,t) at the same 10 m lattice and both interpolate linearly across the same triangles (sim: `getPlane`/`getHeightImpl`, `sacmap.d:217-263`). **The rubbery smoothness comes from the formulas' large footprints (a 90 m wave spans 9 cells), not from render-side supersampling.**
- **The drift is real and now pinned:** Erupt `reboundHeight = 2.0f` in the sim (`state.d:3020`) vs `3.0f` in the shader (`terrain2.d:382`) — **the visual dip is 1.5× the physical one.** Every other constant matches. Quake's shader agrees fully (`terrain2.d:451-456`). S1 §4 was right; this is the strongest possible endorsement of our lattice law (`terrain_rules.md` §4: one evaluator, no shader copy).

### 1.6 Secondary cues that sell the rubberiness (and one that is absent)

- **Normals do NOT respond.** The vertex shader uses the static prebaked mesh normal (`eyeNormal = normalMatrix·va_Normal`, `terrain2.d:84`; normals generated once at load, `sacmap.d:448`). **A 15 m mound keeps flat-ground lighting.** The donor's rubberiness survives with **zero lighting response** — silhouette motion carries it. Our `TERRAIN.NORMALS` recompute (terrain_rules §4.4) is strictly beyond donor.
- **Texture rides the vertices** — texcoords are static while vertices move vertically, so the tile texture visibly stretches on the wave's flanks. Free with vertex displacement; ours behaves identically.
- **Shadow pass displaces too** (S1 §4's claim; consistent with the shared displacement texture — not independently re-verified in the fork's shadow shader).
- **Simultaneous ground-coupled theatre**: debris, rock particles, catapulted creatures, the chasing stun ring, and ScreenShake every 6 frames during the rise (`state.d:17320`). The membrane reading is heavily reinforced by **things standing on it moving with it** — creatures re-snap z to `getGroundHeight`, which includes displacement (`state.d:2153, 8050`).

### 1.7 Assessment against the frozen Field IR

**Expressible? Yes.** Mapping, term by term (ops per `spec/form/field-ir.md` §2-3):

| Donor construct | Field IR realisation |
|---|---|
| `d = \|pos − centre\|` | `DIST2` (centre in adjacent inputs p0,p1) — 1 op |
| PWL time envelopes: `scale(t)`, `rp(t)`, wave gate | `CURVE` tables in the `phase` input — **exact**, these envelopes are PWL in t |
| `sin(π·rp)` | `SIN` of `rp/2` turns (angle16; CURVE emits turns directly) |
| raised cosine `(1+cos(π·d/L))`, auto-zero beyond L | `MUL(d, 1/2L)` → `CLAMP(·,0,½)` → `COS` → `ADD(·,1)` — the clamp pins cos at −1 beyond the footprint, so the term **self-gates**. 4 ops |
| front radius `R = 90·p` | `MAD`/`MUL` |
| width `w(p)` quadratic, then `1/w` | `SUB, MUL, MAD, RCP` — 4 ops |
| amplitude `6(1−p²)` | one `CURVE` (PWL-approx of a quadratic: 33 knots → ≤ 1 mm error) |
| summation across effects | command-order fx_add chain, terrain_rules §3.4 — already law |

**Program sketch (Erupt, single program):** DIST2(1) + mound via shape-CURVE·scale-CURVE (4) + rebound (8) + ring (17) + outputs (2) + LDC constants not fitting in p2..p7 (≈4) + END = **~36–38 instructions, 4 CURVE tables** (shape(d), scale15(φ), reboundAngle(φ), amplitude(φ)) — *sketch-counted by hand, not compiled; treat ±3.* **Fits the global 64 ceiling with room; exceeds the provisional earth ceiling of 32 by ~6.**

Two clean resolutions, both inside existing law:
(a) **phase-split** — a grow-phase program (~10 ops) and a wave-phase program (~32 ops, fits with constants in params), swapped by the command stream at t = growDur, which is exactly how the donor's own code branches; or
(b) **retune the earth ceiling** — field-ir §7.3 says ceilings are "explicitly provisional… a spec-constant edit, never an encoding change."

Quake single-program sketch: ~33, same story.

**Missing ops: none.** Nothing in the donor's wave grammar needs an opcode that does not exist. (RING exists but is smoothstep-shaped; the donor's crest is raised-cosine — COS covers it directly, RING is not needed for fidelity.) A *generic* multi-crest water ripple — which the donor never actually does — is also trivially expressible: `SIN(k·d − ω·t)` in turns (angle16 wrapping gives periodicity for free) × CURVE(d) × CURVE(t) envelopes ≈ 10–12 ops.

**The one real ISA friction: the 4-table limit (V5).** A donor-faithful composite (mound + rebound + ring) wants 4–5 PWL envelopes; at 4 tables you buy the 4th back by computing `w(p)` arithmetically (done in the sketch above). It fits, but with **zero headroom** — the first composite effect richer than Erupt (e.g. Erupt + material lane curves) will hit it. Not an amendment demand; a heads-up while things are warm.

**Quantisation at 240p — ample, with margin measured in orders of magnitude:**
- **Vertical:** live math Q16.16 → 15.3 µm steps; composed-cache height16 → 3.9 mm steps. Crest amplitudes 1.5–15 m = 384–3,840 height16 steps. At 240p a terrain pixel covers ≥ several cm of world; 3.9 mm is sub-pixel by ≥ 10×. **No visible stepping.**
- **Temporal:** at 60 Hz the Erupt front moves 1.5 m/frame = 0.75 of our 2 m cells (the donor tolerated 0.15 of its 10 m cells — we are 5× finer in space, so our front visibly *crosses* cells frame-to-frame, which is smoother, not rougher). The rebound half-sine spans ~550 angle16 steps per frame. The sin table's proven ≤ 1.3 LSB error (qformats §7.1) is ~2·10⁻⁵ relative — sub-millimetre on a 15 m dome.
- **The donor's own look survived 10 m lattice + float32; we evaluate the same class of formula at 2 m pitch in fixed point with strictly finer vertical steps.** Verdict: **the motion will read smoother than the donor's, not stepped.**

---

## 2. Volcano — the rise-and-open, at scale

From `state.d:4054-4113` (struct + stencil application), `state.d:21007-21047` (update), `sacobject.d:3264-3274` (stencil source).

1. **The stencil is a shipped-game asset.** `SacVolcano.volc` is a **33×33 ubyte grid loaded from the original game's own WAD**: `extracted/xmenu/XMNU.WAD!/volc.DATA` (`sacobject.d:3269`). The original engine carried a mountain-profile stencil as data — the direct ancestor of our "stamp stencils are small integer assets" law (terrain_rules §9, which already cites it). Byte values **not derivable locally** (assets absent); peak height in sacengine's scaling is `0.2 × max_byte` — ≤ 51 m if the peak byte is 255. **Not derived further.**
2. **The displacement per cell** (`state.d:4086-4098`): `dmap = flatten + 0.2·volc[j][i]` where `flatten = (target_height − current_height)·(1−f)` and `f` is a **Chebyshev-distance falloff**, fully flattening terrain to the cast-point height within |dx|,|dz| ≤ 80 m, blending out to 164 m (`flatFalloff`, `state.d:4072-4076`). **The volcano does not sit on the terrain; it first pulls a 160 m-radius square of terrain toward a flat datum, then adds the mountain.** Earlier recon missed the flatten term entirely.
3. **The rise** (`state.d:21032-21034`): `progress = frame/volcanoFrame`, **linear in cast time** (`volcanoFrame = castingTime` from the spell asset, `state.d:10748` — numeric value asset-side, not derived), applied incrementally each frame via `applyDMapDelta(old, new)` (`state.d:4101-4112`) — so the whole 330 m-square feature rises as one continuous motion, with ScreenShake every 6 frames. An **interrupted cast un-applies completely**: `applyDMapDelta(progress, 0)` (`state.d:21016`).
4. **The "opening"** (`state.d:21035-21037`): in sacengine, at the exact frame the rise completes, a **discrete permanent crater dent** is punched: `volcanoDent` adds `(d²/25² − 1)·10` — a parabolic bowl, −10 m at centre, radius 25 m (`state.d:6813-6825`). So here the mouth is a **one-frame event at peak**, not a continuous opening. **Whether the shipped game's mouth was instead part of the volc.DATA stencil (an annular profile with a low centre) is not determinable locally** — the stencil bytes are absent. If the stencil is annular, "opens fluidly" is simply the stencil's own crater revealed continuously by the linear rise — the cheapest possible mechanism, and the one to bet on; sacengine's separate dent may be a reimplementation shortcut. **Not established — needs the asset or footage.**
5. **Permanence** (`state.d:21040-21045`): after peak, `progress` decays **linearly** to `residual = 0.25` over `spell.duration` seconds and stays there **forever** — a quarter-height mountain (and quarter-strength flattening) is permanent, plus the full −10 m crater dent. The eruption itself — fire and rocks — is `// TODO: deal damage, spawn debris` twice (`state.d:21038,21042`): **sacengine has no eruption**; the shipped game's fire/rock spray exists only in the owner's account and the record (web: GameSpot guide — "volcano is stationary, it causes incredible damage (which lasts for a while)"). Particle species/rates for it: **not in source, not derivable.**
6. **Units under the rising volcano:** carried up — ground-standing creatures re-snap `z = getGroundHeight` (which includes the permanent grid) during updates (`state.d:2153, 8050` et al.); sacengine deals no rise damage (TODO above).

**Against our format:** the rise is a many-patch **bake sequence**, not a live field, and the machinery already fits: incremental stamp scaling is literally specced as the `applyDMapDelta(from,to)` heir with residual decay (`terrain_rules.md` §9). Scale check at 2 m pitch: a donor-scale volcano (330 m square footprint) spans **6×6 = 36 patches** (5×5 if aligned); height ≤ ~51 m fits height16 ±128 m with real margin even atop relief.

Two sizing flags, both latent in S4 and now sharpened by the gargantuan case:
(a) **the per-patch live/stamp-list bound is still promised but numerically unfrozen** (terrain_rules §9 / charter §11.4) — a volcano plus an 8-wizard Erupt worst case is the sizing floor and should freeze it;
(b) a 36-patch incremental bake touching scar layer B every frame for `castingTime` seconds is a **bake-bandwidth cadence** the spec never bounds — TERRAIN.BAKE's contract owner should state a patches-per-frame bake budget.

Neither needs a format change; **both need a number.**

---

## 3. Bore

### 3.1 sacengine: genuinely unimplemented — re-verified

Complete inventory of "bore" in the source: spell tag `bore="erob"` (`nttData.d:1086`); listed in James's spell set (`nttData.d:1249`); AI-cooldown switch cases only (`state.d:8993, 9065, 9106, 9141`); sound-set tags `bore="kuqX"` **and `boreRepair="rkqX"`** (`sset.d:101-102`); the bore *sound* reused by SoulMole and Erupt (`state.d:16783, 17319`). **No cast case, no effect struct, no update function, no renderer hook.** Confirmed unimplemented in sacengine. Note the scaffolding that *does* reveal intent: a dedicated **repair** sound tag means the shipped game's hole audibly *refills*.

### 3.2 The shipped game (owner's account + web research, labelled)

Owner's account (authoritative): a dirt sprite spirals outward leaving a scar line; terrain then drops away from the centre outward along the spiral path, cell after cell, sequentially; everything standing on it goes down with it.

Web corroboration: the [Sacrifice Wiki (Fandom)](https://sacrifice-shiny.fandom.com/wiki/Bore) — "a rock begins drawing a spiral shape onto the ground… the ground starts to fall away from the center of the spiral. All creatures that are unable to fly will fall down the abyss and 'gib'… **Eventually, the ground begins to refill itself from the outside of the affected area. The ground does not fall away near structures**, such as manaliths or houses." The [GameFAQs Unit/Spell Guide](https://gamefaqs.gamespot.com/pc/914208-sacrifice-2000/faqs/25532) — "A boulder just like the 'mole' from Soul Mole spirals outward… leaving a black line in its wake." The [GameSpot guide (archive.org)](https://archive.org/stream/Sacrifice_Gamespot_Guide/Sacrifice_Gamespot_Guide_djvu.txt) — "Bore creates a giant hole in the earth into which all land-based creatures will fall and permanently disappear from the map (along with their souls)." (Source disagreement noted: Fandom says gibbed creatures leave souls; GameSpot says souls are lost. Likely rim-kill vs fell-through; not resolvable from here.)

### 3.3 How a single-heightfield engine presents a real hole — answered from the format itself

**The shipped map format natively stores per-vertex void.** `parseHMap` (`maps.d:13-31`): each height sample is a u16; **bit 15 = void** (`isVoid = elevation & (1<<15)`), remaining bits = height × 0.1. The renderer then simply **emits no triangles** where the void mask says so (`createMeshes`/`getFaces`, `sacmap.d:196-206`), and the sim's `isOnGround` returns false there (`sacmap.d:251-254`), which flips creatures to `CreatureMovement.tumbling` with ballistic `fallingVelocity` (`state.d:278-314, 2157, 8030`).

**A hole inland is topologically identical to the island rim — same mask, same machinery.** Sacrifice's islands already float with void all around them, so Bore needed **no new representation at all**: flip cells to void in spiral order, and the existing rim rendering clothes the new edge.

The rim clothing is: **vertical curtain quads** from each boundary vertex down to `top − mapDepth` with `mapDepth = 50` m **fixed** (`sacmap.d:106, 504-522`), textured with the environment's `edge` texture; the island "underside" is **the top surface mirrored at −50 m with negated normals** (`sacmap.d:476-481`, `enableMapBottom`). Diagonal boundary configurations get their own curtains (`sacmap.d:519-522`).

**So, plainly: the shipped game did not fake the hole — the hole was real void with real fall-through and real (refilling) state.** What it faked was the *solid* around the hole: constant 50 m curtain walls and a mirrored underside — no true thickness, no shaped keel, no undercut.

**Our dual-heightfield breach law solves the same problem the same basic way (per-cell void + rim clothing) and is an honest advance exactly where the donor faked**: true local thickness (top and bottom surfaces), shaped undersides, thin lips, and walls that end at a modelled bottom instead of an arbitrary −50. The earlier framing — "they couldn't, we can" — is wrong; the correct claim is **"they could and did; we do it with real topology."**

(Also: sacengine builds these meshes **once at load** — "TODO: allow dynamic retexturing", `sacmap.d:176` — so *sacengine* cannot yet do a runtime Bore even though the shipped engine demonstrably remeshed void at runtime, since the hole both formed and refilled.)

**The dirt sprite:** the machinery exists and is SoulMole's — a ground-following mound emitting `ParticleType.dust` (scale 2, lifetime 31) along its path plus `ParticleType.rock` bursts (`animateSoulMole`, `state.d:16718-16745`), with `SoundType.bore` (`state.d:16783`). The particle enum (`sacobject.d:1211-1282`) has `dirt`, `dust`, `slowDust`, `rock` — everything a Bore mole needs; no unused hole/chunk mesh species found in sacengine (the falling-chunk visuals of the shipped game have no counterpart in this source; **not derivable here**).

### 3.4 Sanity check of the orchestrator's claim against `terrain_rules.md`

Claim: per-cell sequential removal driven by spiral arc length is natural in our format. **Verdict: supportable, not overclaimed — with two honest caveats.**

- **Sequencing: yes.** Breaches happen at TERRAIN.BAKE events (§3.4); nothing in the spec constrains bake cadence, and the incremental stamp law (§9) exists precisely for progressive application. Drive the scar depth per cell by `max(0, elapsed − arcLength(cell)/moleSpeed)`-style envelopes (a stamp/earth program) and bake every frame or few frames: cells cross `compose_top == bottom` in arc-length order. Deterministic, capture-exact (§3.4 consequences). The donor's own "does not fall away near structures" is literally our `no_bake` bit (§3.3) — **a parity we already froze without knowing it.**
- **Heal: yes, and the donor demands it.** The wiki's "ground refills from the outside" plus the shipped `boreRepair` sound tag (`sset.d:102`) map exactly to our heal law (a bake raising `compose_top` above `bottom` returns the cell to SOLID, §3.4).
- **Caveat 1 — corner coupling.** A cell breaches only when compose_top meets bottom at **all four corners**, and corners are shared: you cannot take one cell to void without dragging the shared corner vertices down, which slopes the eight neighbours toward the hole. For a spiral crumble this is actually the **right look** (a sagging lip before each drop), but "cells transition independently in visit order" is not literally true — the transition order is arc-length order *convolved with corner sharing*. Fine in practice; say it precisely.
- **Caveat 2 — the falling chunks are not the format's business.** The spectacle of terrain pieces visibly dropping needs transient debris actors (our particle/debris lanes) spawned at breach moments; the format only makes the ground cease. Same division the donor used (Erupt's 64 debris chunks are separate ballistic actors, `state.d:17291-17300`).
- The "falls into open sky" point: **donor parity, not our addition** — their islands float and their Bore already dropped creatures into void. Our genuine deltas remain: modelled underside/thickness at the new rim, kill-margin below `min(bottom)` (§3.5) instead of an engine-magic gib, and nav that actually updates (§3.5 vs S1 §4).

---

## 4. Corrections to earlier recons (loudly)

1. **S4's "Bore is the crown finding" is wrong twice.** "The donor never implemented it" — the *shipped game* implemented, shipped, and balanced it (level-8 James spell); only **sacengine** lacks it. "Sacrifice's single heightfield could only fake a pit" / "its format could not express" — false: **HMAP bit 15 is a per-vertex void mask** (`maps.d:24`), void is a first-class state the whole engine honours, and the shipped Bore made real holes with real fall-through and refill. S4's product conclusion survives in weakened form only: our advance is *honest thickness at the rim*, not *the existence of holes*.
2. **S4's "no format amendment needed" survives this hostile pass too** — but its two watch-items are now sharpened into numbers to freeze: the per-patch stamp/live-list bound, and a bake patches-per-frame budget (volcano case, §2).
3. **S1 under-reported the wave.** It had the constants and one-line grammar but missed: the rebound's exact half-sine time profile and −4 m depth; that Erupt ends *exactly* at zero (no snap, no scar); the crest amplitude law 6·(1−p²) and width-shrink 40→7.5 m; the ~1.5–2-cycle truth (it is **not** damped ringing); the stun ring chasing the crest at 2.25 m tolerance; and that the GPU evaluates at the *same* 10 m lattice (so "smooth over a coarse grid" is footprint size, not resolution). Its drift claim (2.0 vs 3.0) and NEAREST-filter claim are both **confirmed** with citations (`state.d:3020` vs `terrain2.d:382`; `terrain2.d:300-301,528-529`).
4. **S4's Volcano skeleton missed** the Chebyshev flatten term (the volcano levels a 160 m radius before rising), the shipped-asset stencil (`volc.DATA` from XMNU.WAD), the discrete crater dent at peak (`volcanoDent`, −10 m, r=25), and the interrupted-cast full un-apply.
5. **The orchestrator's relayed claims** that shipped Sacrifice never had Bore, and that its maps were not floating, were both wrong — corrected above. **The source and the record outrank all of us.**

## 5. Action items surfaced for owners (no repo modified)

- **Form/Field-IR owner:** earth ceiling 32 vs single-program Erupt ≈ 36–38 (sketch-counted) — either bless **phase-split** as the composite-effect idiom or **retune the ceiling** (both inside existing change-control, no version bump); note the **4-table V5 limit** as the first constraint donor-grade composites hit.
- **Terrain owner:** freeze the per-patch stamp/live-field bound (8-wizard Erupt + volcano as floor); add a TERRAIN.BAKE patches-per-frame cadence budget (36-patch volcano rise is the sizing case); record the corner-coupling phrasing for the breach-sequencing story.
- **Asset lane (later):** pull `volc.DATA` (33×33 bytes, XMNU.WAD) when assets are available — it settles whether the crater mouth is stencil-native, and it is a ready-made stamp stencil for us.

**Sources (web, labelled):** [tg-2/dagon fork, terrain2.d](https://raw.githubusercontent.com/tg-2/dagon/tg-fork/src/dagon/graphics/materials/terrain2.d) · [Bore — Sacrifice Wiki (Fandom)](https://sacrifice-shiny.fandom.com/wiki/Bore) · [Unit/Spell Guide, GameFAQs](https://gamefaqs.gamespot.com/pc/914208-sacrifice-2000/faqs/25532) · [Sacrifice GameSpot Guide (archive.org)](https://archive.org/stream/Sacrifice_Gamespot_Guide/Sacrifice_Gamespot_Guide_djvu.txt) · [PC Gamer retrospective](https://www.pcgamer.com/sacrifice-was-a-visionary-strategy-game-too-great-to-be-forgotten/) (background only).
