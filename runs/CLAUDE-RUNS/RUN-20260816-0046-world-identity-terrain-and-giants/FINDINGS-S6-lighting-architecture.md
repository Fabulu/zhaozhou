# FINDINGS-S6 — Lighting Architecture Recon (read-only)

*Recon S6, 2026-08-16 evening. Subject: global + local light effects, cheap and impressive, explicitly not ray tracing. Sources: the charter v0.2, `spec/` (sky_and_beams, stars_and_flares, terrain_rules, qformats, video_rules, commands.zidl, capture_format), `design/blocks.yml` + `design/contracts/`, sacengine source (`envi.d`, `renderer.d`, `dagonBackend.d`), prior recons S1–S5, N1–N3, and the live Phase-3 code in `reference/src/zrender/terrain.cpp`. Persisted by orchestrator.*

---

## 0. The three things the orchestrator asked for first

### 0.1 The consolidated inventory — we own more lighting than anyone noticed

**A. Vertex tint — layer H, "the LMAP heir"** (`spec/terrain_rules.md` §2 layer table line 106, §6.4). 33×33 RGB565 per patch (2,178 B), modulating the lit vertex colour, per-vertex exactly like the donor's LMAP (1 texel/vertex). Rides the Gouraud path, costs no sampler. Bakes AO, colour variation, faction stain washes. Read-only to fabric, written through the ordinary upload path (`terrain_rules.md` §7). This is the machine's one per-vertex colour channel.

**B. The fragment recipes** (charter §15 "Material recipes"): `texture × vertex light`, terrain Mosaic, emissive, environment accent, cel bands, fogged alpha, additive, multiply, masked, terrain decal, glow/distortion writer — each with a declared cost class visible to compiler and profiler. Ratified recipe instances with frozen laws: `sky_backdrop`, `sky_cloud_fade`, `sun_additive`, `beam_additive_fade` (`spec/sky_and_beams.md` §1.1/§2; `design/blocks.yml` RASTER.FRAGMENT notes), `star_disc_masked`, `star_halo_additive` (`spec/stars_and_flares.md` §1).

**C. The restricted auxiliary source** (charter §15 "Samplers"): terrain surface sheet, light/mask map, shadow compare, distortion map — "must not become a second unrestricted full TMU" (§26). Ledger: 1 aux sample/clk, latency ≤8, cut-order 2 (`blocks.yml` TEXTURE.AUX).

**D. Glow / post lane** (charter §16 Mirror Gate; `spec/sky_and_beams.md` §3): 8-bit effect tag/strength per pixel in tile storage (charter §8), `GLOW = 0b01` channel convention with strength = source texel intensity (`stars_and_flares.md` §1); POST.GATHER sweeps tags → ¼-res glow buffer; POST.COMPOSITE does bloom/`radial_decay`/`flare_splat`, and its charter purpose line explicitly owns **"grading, palette and flash"** (`blocks.yml` POST.COMPOSITE).

**E. Beam machinery, generalisable** (`sky_and_beams.md` §2): `beam_cone` forge kind via DRAW_PROCEDURAL (no opcode), additive tex×vertex, depth-test on / write off, and the **ARM DDA occlusion law**: 64 fixed steps against the canonical heightfield → u8 intensity, with short-circuit and hysteresis. This is a generic world-light-occlusion oracle, not a sky-only trick.

**F. Flare machinery, light-agnostic in shape** (`stars_and_flares.md` §5): frozen-table sprite splats, ghost chain at −26/−77/−230/256, 15-frame fade counter, 1-byte occlusion probe latch in POST.GATHER. Everything is a "light slot" with screen pos + probe pixel; only the identity feeding it is celestial.

**G. CLUT ramp heritage** — the Noctis discipline, already ratified twice: intensity drawn, palette colourised per frame by ARM (≤512 B upload, star pages — `stars_and_flares.md` §1, `blocks.yml` TEXTURE.TMU notes); sky CLUTs swapped per sky-set (donor `ENVI` five texture tags — R1 findings §2). Terrain tilesets are CLUT8 with mips (`terrain_rules.md` §6.1).

**H. Emissive in the field ISA** — the **stamp profile's output record already contains `emissive:unit`** (`spec/form/field-ir.md` §7.1), and charter §12 says damage areas may mark "emissive; glow; distortion; hazard colour; palette phase". The machine has a lawful way to *write* local light state onto terrain; the fragment-side consumer is the sheet/aux path.

**I. "Lighting once"** — charter §10 dual-view sharing: fetch/decode/skin/deform/light once, project per camera. GEOM.PROJECT's ledger purpose is "Camera 0/1 projection + **lighting** for cached or warped vertices", 1 projected vertex/clock (`blocks.yml`). The lighting *stage* is designed in; its *state* is not (see §4).

**J. Shadow placement** — pass 5 of the tile pass order is "terrain decals and **projected shadows**" (charter §8); §12 "precise marks remain polygons: … projected shadows"; depth bias for terrain decals is a non-negotiable 3D basic (charter §8).

**K. Fog** — "deterministic fog" is a non-negotiable basic (charter §8), ZRef-exact (§20.1), with a fog sheet listed under Twin Horizons (§16). The sky under-plane is fog-exempt (`sky_and_beams.md` §1.1).

**L. Sky as the global light source** — elevation-ramp continuity law, additive sun, cloud alpha, and per-set `dfs` surface-sun grading fed by star class (`sky_and_beams.md` §1.2, `stars_and_flares.md` §2 "dfs feeds the ratified surface-sun grading — free per-class mood").

### 0.2 Top five cheap-impressive cheats, ranked by effect-per-cost against real blocks

1. **Time-of-day / weather mood = sky-set + global vertex-light tint.** The rain queue already needs sky-set crossfade; the same crossfade state can drive a per-frame global tint consumed by the already-chartered `texture × vertex light` recipe. Rides: DrawSky reserved 0x0311–0x031F + the recipe + GEOM.PROJECT. Needs: one small law (a light/tint state record — see §4). Nothing here touches fabric.
2. **Dynamic light pools on terrain = the stamp lane's `emissive` out-lane + glow tag + POST bloom.** The ISA output exists (field-ir §7.1), the sheet exists, the tag/gather/composite chain exists. A moving lava glow is a stamp program with age/decay — that's a *content* problem on chartered lanes. Needs: the sheet-tag → glow-tag fragment convention made explicit (RASTER.FRAGMENT recipe note) and a stamp-rate budget (not costed).
3. **Local-light halos and flares = generalise the celestial light slots.** `star_halo_additive`, PART.SOFT glints, `flare_splat`, probe-latch occlusion — all view-anchored and identity-agnostic. A wizard's staff light is a "star" with a world position. Rides SetCelestials' shape (reserved 0x0320 already carries "flare light array ≤2/view").
4. **Light shafts for spells = `beam_cone` with the DDA occlusion law.** No new opcode (DRAW_PROCEDURAL parameter blob); the DDA oracle `zref::beam::occlusion_intensity` reads the canonical heightfield — identical machinery for a god-beam and a spell glow column.
5. **Depth-cue haze = fog toward the sky ramp (Giants' trick).** Fog must be built anyway (charter §8 non-negotiable); choosing its colour from the sky-set's horizon ramp and its banding per-vertex makes "unobstructed to the horizon" (S3 A4.2: "a distance-banded CLUT shift toward sky palette… for free"). Home: deterministic per-vertex fog in GEOM.PROJECT. **Not** scanout CLUT — see contradiction #1.

### 0.3 Contradictions with the orchestrator's framing (mine — kept verbatim)

1. **"Colour grading via CLUT/ramp swap at scanout — does our scanout pipeline permit this?"** **No.** VIDEO.SCANOUT's contract exclusions say "no colour conversion or scaling (VIDEO.SCALER)"; VIDEO.SCALER is "a pass-through… No colour conversion, no scaling" (`spec/video_rules.md` §6). Scanout-side grading would violate two frozen contracts. Grading's lawful home is POST.COMPOSITE (its purpose line owns "grading, palette and flash"), whose tables are *generated assets, never runtime-computed* (`design/contracts/POST.COMPOSITE.md` Notes) — so the Noctis per-frame DAC-filter trick ports as *authored table selection per frame*, not as runtime table math.
2. **"Per-vertex dynamic lights baked by ARM into existing vertex colour streams (for creatures)"** — **the stream does not exist.** GEOM.VDECODE decodes "positions/normals/UV" (contract purpose); the meshlet format lists "compact normal and UV encoding" with no colour attribute (charter §10). The only per-vertex colour channels in the machine are terrain layer H and the sky's baked alphas. Creature vertex lighting is designed to be *computed* in GEOM.PROJECT ("projection + lighting"), not uploaded.
3. **"The cut order kills the dynamic shadow feature third — what survives?"** Two different things share the word "shadow": the **pass-5 projected-shadow decal polygons** (charter §8/§12 — ordinary geometry with depth bias; nothing in the cut order touches it) and the **aux "shadow compare" role** (charter §15 — the thing that dies at cut 3, alongside the whole aux lane at cut 2). The fallback look after cut 3 is the donor's own: decals + beams + AO in the tint layer.
4. **"Light + CLUT8 texture interplay" as an open question** — half-settled already: nearest is mandatory wherever bilinear would touch a palette (`stars_and_flares.md` §1), and the beam texture is deliberately direct-colour for exactly that reason (`sky_and_beams.md` §2). Any future light/mask *texture* on the aux lane must be direct-colour (RGB565), or the light stays per-vertex.

---

## 1. What the donors actually did (source-cited)

**Sacrifice (sacengine, `source/envi.d` lines 13–66):** the entire per-level lighting state is one struct — `sunDirectStrength`, `sunAmbientStrength`, `ambient{R,G,B}`, sky background RGB, `sunDirection{X,Y,Z}`, `sunColor{R,G,B}`, `sunFullbright{R,G,B}`, `shadowStrength`, `landscapeSpecularity/Glossiness` + specularity RGB, fog (linear|exponential, nearZ/farZ/density, RGB), cloud alpha clamps, five sky texture tags. **One directional sun + one ambient + fog + a baked LMAP. No dynamic point lights anywhere in the format.** Consumption (`source/renderer.d` 1049–1118): per-tileset sun strength multiplier (james 14.0, stratos 12.0, ethereal/persephone 6.0, charnel 4.0, pyro 2.0), ambient = strength×RGB, sun colour = strength-weighted blend of ambient and sun colours; `shadowMap.shadowColor = ambient`, `shadowBrightness = 1 − shadowStrength` — **shadows fall to ambient colour**, the classic cheap trick. Terrain shading (`EVIDENCE-dagon-terrain2.d.txt`, frag shader): `totalColor = 0.5·diffuse·(1+detail)·lmap` — tile × global tint, where `lmap` is sampled at `coord` (per-vertex resolution). Sky: shadeless/unlit; sun quad additive with energy 25× (renderer.d 1141–1147); god beams unlit additive, depth-write off, energy 4.0 (R1 findings §3). Souls/particles: shadeless with energy and per-part colour (`dagonBackend.d` 2529–2541). The reimplementation's PBR/G-buffer/shadow-map layer is modern Dagon, **not** the original — treat only the format fields as donor truth.

**Noctis (N3 findings §5, labelled donor design-intent via prior recon):** every pixel index = 2-bit ramp selector + 6-bit intensity; *colour never exists at draw time — the DAC colourises at scanout*; all glows/fades/flares are saturating 6-bit arithmetic; whole-scene grading is a per-frame multiplicative RGB filter on the DAC palette. Our ratified port deliberately moved this discipline into CLUT8 *assets* + the 64-entry ARM-rebuilt star pages (`stars_and_flares.md` §1 thesis), because our frozen tile store is 24-bit + tag, not indexed.

**Giants (S3 findings A3/A4, web-derived design intent):** draw-distance pride expressed as **depth-cue blur/haze instead of fog walls** — "distant objects slightly blurred as a depth cue"; **sun-rays on water** as a specular streak toward the sun (S3 costs it: "a CLUT ramp indexed by a fixed-point dot product"); dynamic shadows existed but destructibility was cut. Proven vocabulary: distance degradation *as aesthetic* (our LOD governor's splat/glint tiers already are this — S3 B3), water sun-streak (queued for the sky owner per the tasking), haze-as-CLUT-shift.

**The proven cheap vocabulary across all three donors:** one global directional + ambient; baked AO/variation tint at vertex resolution; unlit additive for everything that glows; shadows as ambient-coloured darkening or decals; grading as palette work; distance as palette/fog shift, never as geometry cost.

---

## 2. The cheat menu, costed honestly

*(fits existing blocks / needs a small extension / refused; "not costed" wherever no ratified number exists — per the no-invented-costs law.)*

### Global

| Cheat | Verdict | Where it rides / what it needs |
|---|---|---|
| Time-of-day via sky-set swap | **Fits** | Overcast/dawn/dusk sets are assets on the ratified lane; the rain queue already requires the crossfade law (three options with costs undecided — that decision is still open). |
| Terrain retint for time-of-day (layer H re-upload) | **Mechanism fits, affordability not costed** | 2,178 B/patch; at the 256-patch composed-cache scale (`terrain_rules.md` §4.2) a full per-frame retint is 545 KiB/frame ≈ 32.7 MB/s at 60 Hz (arithmetic from cited constants) — the same unclosed order as the streaming (41 MB/s, §7) and bake (31.6–41.3 MB/s, §9.2) lanes; `spec/memory_rules.md` has no ratified total bandwidth. Decide with ZH-004 board truth. Cheaper sibling: per-frame *global scalar/RGB* tint consumed by the vertex-light recipe — zero VRAM, one state record. |
| Colour grading | **Fits (POST.COMPOSITE)** / **refused at scanout** | POST.COMPOSITE owns "grading, palette and flash" (`blocks.yml`); tables are generated assets. Scanout path is contractually colourless (`video_rules.md` §6). |
| Full-screen flash / tint | **Fits** | "Screen flashes" is a listed Mirror Gate effect (charter §16). Lightning flash = authored grading-table sequence + sky-set flash; state must be captured (see §5). |
| Depth-banded haze (Giants) | **Small extension** | Fog is mandated but unspecified (see §5); per-vertex fog in GEOM.PROJECT with colour bound to the sky-set horizon ramp needs only the fog law + the tint state. |
| Night mode | **Small extension** | Star/flare gating already has weather/night flags (`stars_and_flares.md` §5); night *terrain/creature* response is the same missing global-tint state. |

### Local

| Cheat | Verdict | Notes |
|---|---|---|
| Emissive + additive glow for spells | **Fits** | Recipes chartered (charter §15); beams prove the additive path; stamp `emissive` out-lane writes the state (field-ir §7.1). |
| Cel bands | **Fits** | Chartered recipe with declared cost class (charter §15). No instance law written yet — recipe detail is unwritten, not unbudgeted. |
| Projected/blob shadows | **Fits (decal form)** | Pass-5 terrain-conforming polygons + depth bias (charter §8/§12); blob = SURFACE.STAMP darkening tag (the Phase-3 stand-in already darkens by sheet strength — `reference/src/zrender/terrain.cpp:422-441`). The *shadow-compare aux* form is cut-order 3. |
| Light flares as screen-space sprites | **Fits** | All machinery exists and is frozen (`stars_and_flares.md` §5); ≤2 lights/view is the frozen bound; widening it is a budget-line change, not architecture. |
| Beam-occlusion law → light shafts | **Fits** | DDA is generic; oracle `zref::beam::occlusion_intensity`; hysteresis law reusable. |
| Per-vertex dynamic lights (creatures) | **Small extension, honestly double** | Needs (a) a light-state ABI record (none exists), (b) a decision: FPGA-side N·L in GEOM.PROJECT (contract already says "lighting"; per-vertex ALU cost **not costed** — no vertex cost model exists beyond 1 vertex/clock) or a meshlet colour lane (format extension; format is provisional). ARM-baking is *not* an option on an existing stream — there is no stream (contradiction #2). |
| Field program computing terrain light into layer H | **Extension — and a lane-contention loser** | See §3. |

### Refused outright (charter §26)
General fragment shaders, floating-point raster, a second unrestricted TMU, deferred G-buffers, multiple full shadow maps, unrestricted render-to-texture (per-texel CLUT boil was refused for exactly this — `stars_and_flares.md` §3), unbounded per-pixel RMW (live Bresenham flares refused, §5), full-frame diffusion (`stars_and_flares.md` §14 risk 1 — "refuse it, or confine it to the low-res glow plane").

---

## 3. The interesting one — can a FIELD.SEQ program compute lighting into layer H?

**Answer: not without an ISA version bump, and even then it's the wrong lane.**

- The earth profile's output record is `{height:fx, velocity:fx, material:u32, nav_cost:fx}` (`field-ir.md` §7.1). No tint lane. Adding one is a profile I/O record change = **FIELD_IR_VERSION bump** (§13) with golden-vector regeneration — the most expensive edit class this spec has, per the §7.4 ruling's own reasoning.
- TERRAIN.PATCH composes height + velocity lattices (contract purpose); a tint lattice would be a new produced cache, new VRAM ownership row, new consumer — real machinery, not a ride.
- **Lane contention kills it anyway:** MAX_PATCH_FIELDS = 16 is *fully allocated* by the donor worst case (8 Erupts + 8 Quakes, `terrain_rules.md` §9.1), and priority lives above the seam — cosmetic fields sort last and are the degrade path. A light pool would be the first thing rejected under load. The worst legal patch already costs ~557,568 field instructions ≈ 33% of a placeholder frame (§9.1, an intake bound, not an affordability certificate).
- **The lawful sibling already exists:** the **stamp** profile outputs `emissive:unit` (§7.1), SURFACE.STAMP has age/decay ops, and charter §12 explicitly lets damage marks carry emissive/glow. A moving light pool is a stamp program writing emissive strength with decay — content on chartered lanes, capture-exact, no ISA bump. Its open cost is the per-frame stamp-rate budget (the bake lane got BAKE_PATCH_BUDGET = 64; **the stamp lane's equivalent is not costed** — SURFACE.STAMP's contract is a stub).
- Phase-split idiom (§7.4) applies if a light effect needs envelopes: temporal splits only, one lane at any instant.

---

## 4. Pressure points

1. **The aux TMU can do exactly one thing per terrain fragment per frame.** The budget line is explicit: "ONE aux consumer on terrain fragments, because tint moved to vertices" (`terrain_rules.md` §6.5) — that consumer is the surface sheet. The aux role list (light/mask map, shadow compare, distortion) is therefore available to *non-terrain* geometry only, or to terrain only by displacing the sheet. And the whole lane dies at cut-order 2.
2. **Cut order 3 ("dynamic shadow feature") fallback look:** pass-5 shadow decals + AO baked in layer H + ambient-tinted undersides (the Phase-3 stand-in already ships `0.25 + 0.75·lambert` ambient floors — `terrain.cpp:391-399`) + beams. That *is* the Sacrifice look; nothing aesthetically essential dies.
3. **Phase 11 owns glow/distortion/grading/flares; sky/beams RTL is Phase 11 (ZH-044/046).** What must land *earlier* for the game to read correctly: the opaque core's `texture × vertex light` recipe (Phase 5 material list), fog (charter lists it in Phase 5 build order — step 12), layer H consumption (Phase 6 terrain materials), and the light-state record whenever GEOM.PROJECT's "lighting" stops being a stub. The ARM/ZRef preview lane can and does deliver the look earlier (the software stand-in lights today).
4. **The vertex-light state does not exist.** No ABI record carries sun direction, ambient, or global tint (`spec/commands.zidl` — 16 commands, zero light/fog state). The Phase-3 stand-in hard-codes one light, (1,2,1)/√6 (`terrain.cpp:371` context). Every global cheat in §2 routes through this one missing record — it is the cheapest, highest-leverage spec gap in the whole subject.
5. **The light-state gap is also a determinism gap** (see §5.1) — state that isn't in a command can't be in a capture.

---

## 5. What is missing entirely

1. **Light state in captures.** `celestial_state` is declared (`stars_and_flares.md` §8) but **absent from `spec/capture_format.md` today** (grep: no match). No chunk or command exists for sun direction, ambient, global tint, fog state, or weather. The precedent to follow is celestial_state: replay from any frame must reproduce the light exactly; a storm must be sim state, not a renderer toggle (the rain queue doc says this in so many words).
2. **The fog law.** Charter mandates deterministic fog in three places (§8, §16, §20.1); no spec owns a formula, Q-format, state, or per-vertex vs per-fragment placement. This is a Phase-5 blocker before it is a lighting question.
3. **Light + CLUT8 interplay rules.** Nearest-mandatory-where-bilinear-touches-palette is law for stars and beams; terrain CLUT8 tiles' filter mode is unstated in `terrain_rules.md` §6. One sentence would close it.
4. **Lighting under Mosaic.** Orthogonal by construction (tint is per-vertex, Mosaic picks the texel id — `terrain_rules.md` §6.2/§6.4) but the *composed* order (tile × tint × sheet-strength × fog) is nowhere written down; the Phase-3 code is the only existing composition order (`terrain.cpp:430-441`).
5. **Night mode** — flag exists in flare gating only (§5 of stars); no terrain/creature/sky response law.
6. **Lightning flash during storms** — cheapest lawful build: authored POST.COMPOSITE grading tables + sky-set flash + lightning-bolt geometry as forge ribbons (the donor's lightning is a frame-animated mesh — `renderer.d:1714-1758`); state lands in the weather/celestial chunk. Untouched by any current spec; it belongs with the queued rain work.
7. **A stamp-rate budget** (the SURFACE.STAMP analogue of BAKE_PATCH_BUDGET) — required before the §3 emissive-pool cheat can be called affordable.
8. **Creature vertex-light input** — meshlet format has no colour lane (contradiction #2); decide FPGA-computed vs format extension before Phase 8 freezes the meshlet format.

---

## 6. Recommended sequencing (for the orchestrator, not law)

1. **Spec-first, one record:** a light/environment state command (sun direction angle16-pair, sun colour RGB565, ambient RGB565, global tint RGB565 + strength, fog params) + its capture chunk. One ABI reservation, unblocks cheats 1, 5, night, and the fog law simultaneously.
2. Write the fog law into whichever file owns raster basics; bind its colour to the sky-set ramp (Giants' depth cue for free).
3. Make the stamp `emissive` out-lane → glow-tag convention explicit in RASTER.FRAGMENT notes; cost the stamp budget.
4. Generalise `SetCelestials`' light array semantics to non-celestial lights in the reserved range when local lights are wanted — machinery is identical.
5. Leave earth-lane lighting dead (§3); leave scanout grading dead (contradiction #1).
