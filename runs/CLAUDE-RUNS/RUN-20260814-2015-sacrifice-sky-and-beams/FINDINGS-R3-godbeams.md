# FINDINGS — R3: God Beams (Volumetric Light Shafts), Sacrifice-style

**Run:** RUN-20260814-2015-sacrifice-sky-and-beams — **Date:** 2026-08-14 — **Agent:** RECON R3
*Persisted by orchestrator. Verdict below is the agent's; the piece architect ratifies.*

## SUMMARY

**Verdict: build geometric world-anchored beams as primary (Phase 11, near-free); add Mirror Gate radial-decay mode as secondary (cheap, bounded); do NOT build tile-depth-percentile fade (option 2b) — per-frame ARM ray-march against the heightfield covers the one case (a) misses, for ~zero fabric.**

1. **Primary — geometric beams.** Slanted world-anchored cone (fixed orientation, NOT billboard-to-axis; Sacrifice shafts are world-fixed and at 240p a camera-locked billboard visibly swims). Radial falloff = texture RGB luminance gradient around the cone circumference (U), read via the **bilinear** TMU path so the ramp is continuous, not steppy. Vertical fade = texture V + vertex-colour ramp emitted by Forge. Bottom fade-to-zero before the cone base kills the hard ground-clip line.
2. **Material:** new recipe `beam_additive_fade` (tex.RGB × vertex.RGB, `dst = sat(dst + src)`). Pure additive needs **no alpha channel, no sorting**: addition commutes, so the §26 "no exact OIT" refusal is moot and coarse depth-binning is harmless. No dithered alpha in steady state — continuous luminance + resolve-time ordered dither is deterministic and shimmer-free.
3. **Occlusion (a) verified free and exact.** Opaque pass 3 writes invw24; translucent pass 6 depth-tests per fragment, so beams behind terrain are pixel-exactly rejected regardless of coarse binning (bins order translucent-vs-translucent only). Residual gap — source itself hidden → whole beam should dim — solved by **one heightfield DDA ray query per beam per frame on ARM** (canonical terrain is ARM-owned), feeding a per-beam intensity word. Option 2b (tile depth percentile fade in RASTER.FRAGMENT) is 16×16-pixel-granular, a new blend mode, and worse than the ray query — refused.
4. **Secondary — screen-space sun shafts.** Gather sun/sky brightness into the existing low-res glow buffer via the existing glow-writer recipe + effect tag; new **radial-decay blur mode** in POST.COMPOSITE (Mitchell GPU Gems 3 ch.13 structure, fixed 12 taps, decay weights as frozen constants, u8/fx16 saturating accumulate). 96×60 = 69,120 taps ≈ 70K cycles worst case ≈ 4% of a 1.67M-cycle frame (100 MHz placeholder; Phase-0 freezes). Buffer is 11.5 KB — fits M10K POSTBUF, zero VRAM traffic. Bounded constants satisfy §16 by construction; amend §16's effect list to name it.
5. **240p survival: wide soft beams read, thin beams shimmer.** Min projected width policy: ≥6 px (Z60), ≥5 px per Duo view; ladder by projected width in the Measure's pixel-error language: ≥24 px → 8–12-sided cone; 12–24 px → 6-sided; 6–12 px → crossed billboard sheets (Forge "billboard sheet" already planned); <6 px → soft sprite (PART.SOFT, which already has depth-fade); <2 px → culled. Hysteresis + min-hold per §9.
6. **Fabric:** ~0% new blocks. Beam = mode of FORGE.PRIM (cone with spec'd UV/vertex-alpha assignment), recipe entry, DRAW_PROCEDURAL + material class, `beam` Form surface shape. Radial mode ~0.5–1% ALM inside the 6% twod_post group. Both inside budget with ≥10% reserve untouched.
7. **Banding: confirmed non-issue.** Beams accumulate in the 24-bit working tile; the only banding source is the texture ramp itself — use bilinear TMU sampling of a ≥16-texel ramp. Resolve ordered dither (8→5/6-bit) is generated and deterministic.
8. **Charter conflicts: none requiring violation** — only spec amendments: §14 (beam cone mode), §15 (recipe), §16 (radial mode + frozen constants), ops.yml cost classes, FORGE.PRIM/POST.COMPOSITE contracts, cost model ladder entry.

## 1. Geometric beams — detail

**Orientation.** World-anchored slanted cones (Sacrifice-style shafts through storm clouds). A billboard-to-camera-axis beam is cheaper per-vertex but rotates under camera motion; at 240p the rotation reads as swimming because the beam's own texture gradient (U around the cone) re-projects every frame. Fixed world cones have stable UVs — only perspective changes. The one billboard exception is the *bottom ladder rung* (crossed sheets), where the beam is <12 px and stability no longer matters.

**Cross-section falloff.** Around the cone: U = circumferential coordinate; texture is a horizontal ramp, peak at U=0.5, zero at U=0/1, seam placed facing away or at the cone's back (the seam is where alpha≈0 so it is invisible). Mechanism is pure fixed-function: sample texture, add. **Critical detail: use the bilinear TMU path.** A nearest-sampled 16-texel ramp across a 24-px-wide half-beam gives ~1.5 px/step — visible stairs. Bilinear interpolation is continuous, so even a 16-texel ramp is smooth.

**Vertical fade / ground intersection.** Hard clip where the cone base intersects terrain is the classic ugliness; era games faded the base out. Exact fixed-function mechanism: Forge emits the cone with (i) V mapped to world height along the beam axis, texture V-ramp fading to black in the lowest fade band (author-controlled, e.g. bottom 20%), and (ii) vertex colour alpha/RGB ramp over the same band. Both are vertex-path quantities Forge already computes when it places the base ring at the anchor's ground height (ARM supplies ground height from canonical terrain in the beam descriptor — no FPGA terrain query needed). The beam visually dissolves ~1–2 m above ground; below that, depth-rejection against terrain silhouettes gives believable wrap.

**Dither.** Ordered dither at resolve (RASTER.RESOLVE, generated table) is deterministic and applies to everything. Dithered *alpha* on the beam itself would crawl under slow camera motion at 240p — refused for steady state. If a crossfade needs it during LOD transitions, reuse the Measure's existing "stable screen-space dither during representation crossfade" machinery (§9 Stability).

## 2. Occlusion — analysis of (a) first

Tile pass order: opaque geometry (pass 3) writes invw24 into the tile store; pass 6 "coarse-depth-binned translucent geometry" then depth-tests each fragment. So:
- Beam fragment behind a hill → depth test fails → rejected. **Pixel-exact occlusion against all opaque geometry, free.**
- Coarse depth bins only order translucent-vs-translucent draws; the per-fragment depth test uses interpolated invw24, so binning cannot cause false occlusion.
- Translucent-vs-translucent: additive is commutative/associative — order-independent by arithmetic, not by machinery.

Gap in (a): a source *fully* behind terrain (sun below a ridge) still casts a full-strength beam wherever the beam itself is in front of the ridge — era games shipped exactly this, but the payoff fix is cheap: **one DDA ray march per beam per frame on ARM** from the beam's source point toward the camera (or a few fixed samples along the beam axis) against the canonical heightfield; result = per-beam intensity multiplier in the DRAW_PROCEDURAL descriptor. Deterministic (canonical terrain is game truth), ~O(tens of μs) per frame for a dozen beams, zero fabric.

Option (b) — tile-local depth percentile fade in RASTER.FRAGMENT — would need a new blend mode reading tile depth statistics (~0.1–0.2% ALM), but statistics at 16×16 granularity cannot resolve thin occluders, and it's a new spec surface in the most verified block in the machine. **Refused; revisit only if Wound Lab footage shows a need, as a §15 recipe amendment, never ad-hoc.**

## 3. Screen-space sun shafts — quantified

Structure (Mitchell, GPU Gems 3 ch.13, adapted to fixed function):
1. **Gather:** sky/sun pixels and glow-tagged geometry already write the 8-bit effect tag/strength in the working tile; POST.GATHER accumulates the low-res glow intensity buffer as specced. Sun brightness mask = Twin Horizons sky plane + any additive sun disc, tagged glow. No new gather work.
2. **Radial-decay blur (NEW, in POST.COMPOSITE):** per texel, `delta = (uv − sunUV) · density/12`; 12 taps walking delta, each tap `acc += s · w[i]`, `w[i] = decay^i` from a frozen constant table; saturating fixed-point accumulate. Deterministic: fixed taps, fixed weights, fixed-point rounding per the wave-1 rescale primitive.
3. **Composite:** upscaled additive add, exactly the existing bloom path.

Cost, Z60: 96×60 = 5,760 texels × 12 taps = **69,120 taps ≈ 70K cycles** ≈ **4.2% of a 1.67M-cycle frame** (100 MHz placeholder; scale at Phase 0). Duo: 2 × 64×48 = 6,144 texels × 12 = **73,728 taps** — same envelope. M10K: 11.5 KB (Z60), 12.3 KB (Duo) — inside POSTBUF, **zero VRAM traffic**. Fabric: est. 0.5–1% ALM within the 6% twod_post group. All constants frozen in spec → §16's "intentionally low-resolution and bounded" satisfied by construction; cut-order 6 protects it under pressure.

## 4. Recommendation & 240p width policy

**Build both, in this order:** geometric beams in Phase 11 (ride FORGE.PRIM, ZH-044) — they carry the Sacrifice look. Radial Mirror Gate mode last in Phase 11 or Phase 12 (rides ZH-046's POST work) — adds the sky-wide "sun bleeds through clouds" wash that geometry can't.

| Projected width | Representation |
|---|---|
| ≥24 px | 8–12-sided cone, bilinear U/V ramp |
| 12–24 px | 6-sided cone |
| 6–12 px | 2 crossed billboard sheets (Forge "billboard sheet") |
| 2–6 px | soft sprite via PART.SOFT (depth-fade already specced) |
| <2 px | culled |

Hysteresis + minimum hold per §9; ladder rungs chosen by The Measure with the beam's semantic weight.

## 5. New/changed artifacts (complete list)

- **No new ledger blocks.** Everything is a mode/recipe/constant of existing blocks.
- **FORGE.PRIM** (contract + charter §14 amendment): new primitive mode `beam_cone` — cone/tube with (a) circumferential U mapped for radial-fade texture, seam parameter, (b) V mapped to axis height, (c) vertex-colour fade band (top/bottom parameters), (d) base-ring placement at supplied ground height, (e) bounded subdivision caps as spec constants.
- **Charter §15 / recipe table / ops.yml:** recipe `beam_additive_fade` — `colour = tex.RGB × vertex.RGB; blend = dst = sat(dst + src)`; cost class = additive fast path; requires bilinear TMU mode.
- **Semantic command:** no `DRAW_BEAM` — `DRAW_PROCEDURAL` with forge primitive `beam_cone`, material class beam, plus per-beam `intensity_word` (the ARM occlusion-ray result) in the descriptor parameters.
- **Form surface declaration:** `beam` shape in the present domain — `beam anchor, height, slant, radius(t), tint, fade_band, min_px, semantic_weight`.
- **POST.COMPOSITE** (contract + charter §16 amendment): `radial_decay` mode of the glow path; frozen constants: buffer 96×60 (Z60) / 2×64×48 (Duo), 12 taps, decay/density tables, guard clamp. Add "sun shafts" to §16's effect list.
- **Cost model (spec/form/cost_model.md):** fragment class per beam (additive class), triangle cost per ladder rung (16/12/4/2/0 tris), POST radial class (69–74K taps), per-beam ARM ray query cost.
- **LOD:** beam ladder registered as a representation ladder in the Measure's vocabulary (mirrors the Myriad ladder style).

## 6. Cost table

| Item | Z60 (384×240, 360 tiles) | Duo (2×256×192, 384 tiles) |
|---|---|---|
| Beam fragments (budget: 8 beams, ~3K px avg) | ~24K ≈ 26% screen overdraw, 24K cycles @1 frag/clk | ~2×12K ≈ 24% per view, 24K cycles |
| Beam triangles | 8×16 = 128 (trivial) | 2×(5×16) = 160 |
| Texture samples | 24K (bilinear, cache-hot ramp) | 24K |
| ARM occlusion rays | ~8 heightfield DDAs/frame | ~10/frame |
| POST radial taps | 69,120 (96×60×12) ≈ 4% frame | 73,728 (2×64×48×12) ≈ 4% frame |
| M10K POSTBUF | +11.5 KB (u16 glow) | +12.3 KB |
| New fabric | ~0.3–0.5% ALM (FORGE mode) | same |
| POST radial fabric | ~0.5–1% ALM in twod_post (6%) | same |
| Reserve impact | none — inside myriad_forge (9%) + twod_post (6%) | none |

## 7. Charter-law conflicts

None requiring violation; three spec amendments (the charter's own resolution path):
1. §14 FORGE.PRIM primitive list grows a mode — "bounded subdivision / explicit cost / deterministic generation / dual-view" applies verbatim.
2. §15 recipe table gains `beam_additive_fade` — a named, cost-classed parameterisation of additive.
3. §16 Mirror Gate effect list gains "sun shafts (radial-decay glow mode)" with frozen constants — one bounded mode of one bounded buffer, exactly like bloom; §26's "no unrestricted render-to-texture graphs" intact.

Determinism audit: additive saturate with round-half-up per wave-1 rescale = deterministic; decay tables frozen; DDA ray on canonical terrain = game truth; bilinear TMU rounding already specced. Capture-exact throughout.

## 8. Online recon — era & modern techniques

- **Era geometric cheat:** cone/cylinder with ray-like transparent additive texture, alpha-0 at edge verts, vertex fade top/bottom — the primary recommendation ([Blender Artists](https://blenderartists.org/t/light-god-rays-in-cycles/5447882), [Unity forums](https://discussions.unity.com/t/god-rays/428141)). Dungeon Keeper 2's dungeon god-rays and OoT's Forest Temple shafts are of this family; no primary-source confirmation of their internals found online — treat era attribution as community-knowledge, not citation-backed; the technique itself is citation-backed.
- **Post-process radial scattering:** Kenny Mitchell, [GPU Gems 3 ch.13](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process) — occlusion prepass, decay^i accumulation toward the screen-space light, NUM_SAMPLES parameterised; two fixed-function-relevant notes: downsampling for bandwidth, and for fixed-function hardware "multiple additive frame-buffer passes over stencil-limited regions" — degrades gracefully to our bounded-buffer single-pass form ([three.js adaptation](https://medium.com/@andrew_b_berg/volumetric-light-scattering-in-three-js-6e1850680a41)).
- **Banding at low colour depth:** gradients quantised to 16-bit band; ordered dithering is the standard remedy ([LVGL](https://lvgl.io/blog/tutorial-dithering-16bit), [ImageMagick](https://github.com/ImageMagick/ImageMagick/discussions/8087), [FrostKiwi](https://blog.frost.kiwi/GLSL-noise-and-radial-gradient/)). Confirms: blend in the 24-bit working tile, dither once at resolve, keep the source ramp continuous via bilinear TMU sampling.
