# Sky and Beams Specification (Sacrifice-style 360° sky + god beams)

**Status:** RATIFIED v1 (2026-08-14) — architecture addendum from run RUN-20260814-2015 (evidence: reverse-engineered Sacrifice engine structure + architecture-fit recons; see `runs/CLAUDE-RUNS/RUN-20260814-2015-sacrifice-sky-and-beams/`).
**Authority:** this file owns sky/beam semantics. Charter §8 pass 1 ("terrain/backdrop prefill") admits the sky prefill without charter amendment. ABI reservation: `0x0310..0x031F` (`DrawSky 0x0310` + extensions; lands in `spec/commands.zidl` post-W4). Version this file on any semantic change.
**Cross-references:** `spec/qformats.md` (fx16/angle16/rounding law), charter §8 (pass order), §9 (Measure/hysteresis), §14 (Forge), §15 (textures/recipes), §16 (Mirror Gate), §25 (counters), §26 (refusals — none touched).

---

## 1. Sky architecture (ratified D1–D4)

Hybrid pass placement, proven pixel-equivalent to Sacrifice's late-drawn sky for all opaque cases (equivalence proof recorded in the run's ADDENDUM §1.1):

- **Pass 1 (backdrop prefill)**: drum (both bands) + zenith cap — Z-test off, Z-write = far constant, blend off, effect-tag init. Replaces the tile clear; net-zero fragment cost. Ordered before any terrain prefill.
- **Pass 3 (opaque)**: under-plane at real world depth (correct parallax off floating-island edges; cliffs/skirts occlude it properly).
- **Pass 6 (translucent)**: cloud sheet + sun quad — Z-test on (interpolated invw24), Z-write off. Deterministic sub-order: sun (z=+2560) before cloud (z=+1792) via coarse back-to-front binning; additive layers commute.

**Assembly**: anchored to map centre, world-fixed (never camera-translated). `DrawSky` carries per-view rotation-only `mat4fx rot_proj` (translation rows MUST be zero — validated, error code otherwise). **Fallback**: DrawSky absent, a layer flag-disabled, or an uncovered direction → flat clear in the sky-set background colour. **No pixel is ever left unwritten.**

### 1.1 Layer table

| Layer | Primitive (FORGE.PRIM) | Pass | State | Texture | Animation (exact, tick-derived) |
|---|---|---|---|---|---|
| Lower band `skyb` (z −2560…0, r 4096→5120) | `sky_drum` band 0 — 48 cols, 8 mirrored repeats, inside-facing winding | 1 | Z-test off, Z-write far, blend off | 1024×128 CLUT8, u-**mirror**, v-clamp, +mips | `drum_yaw` (default 0) |
| Upper band `skyt` (z 0…+2560, r 5120) | `sky_drum` band 1 | 1 | same | 1024×128 CLUT8, u-mirror, +mips | `drum_yaw` |
| Zenith cap | `sky_cap` — 16-tri fan, z +2560 | 1 | same | 64×64 CLUT8, clamp, +mips | none |
| Under-plane `undr` | `sky_plane` — 10240×10240 quad, z −2560, generator-seamed to band 0 inner radius | 3 | Z-test on, Z-write on, fog-**exempt** | 512×512 CLUT8, clamp, +mips | `drum_yaw` |
| Cloud sheet `sky_` (z +1792) | `sky_cloud_sheet` — 8×8 vertex grid (128 tris), UV 0..4, per-vertex `α=(1−r²)·max_alpha` in fx16 baked at generation | 6 | Z-test on, Z-write off, alpha blend `sky_cloud_fade`: `out = dst·(1−a)+src·a`, `a = tex.a × vertex.a` | 256×256 ARGB4444, u/v-repeat, +mips | `scroll_u = ((tick % 3840) << 16) / 3840` (floor); `scroll_v = −scroll_u` (1 tile / 64 s @60 Hz, direction (1,−1)); `max_alpha` per set |
| Sun `sun_` (z +2560, world-fixed, NOT billboarded) | plain quad generated with the sky set | 6 | Z-test on, Z-write off, additive `sun_additive`: `dst = sat(dst + src·tex.a)`, glow effect-tag write on | 64×64 ARGB4444, alpha pre-baked to `min(3·lum,1)` at asset compile; optional halo baked to inverse `min(1,96r²)` | none; energy per set |

Sun punch-through: **approximated** — no runtime cloud-alpha modification; the additive sun/halo reads through the cloud (revisit hook: ARM per-tick sub-grid vertex-alpha update if Wound Lab footage demands hard burn-through — remains capture-exact).

**Costs (cost-model lines)**: `sky_triangles ≤ 352` total (192 drum + 16 cap + 2 under + 128 cloud + 2 sun + margin), ×2 Duo views; `sky_fragments ≤ 92,160` (the clear it replaces) + cloud ≤ ~45K blended; VRAM ≈ 0.9 MB (~1.9% of the texture pool), shared between Duo views. Measure-**exempt** with these declared budget lines; fully counted in §25 counters; carries the DrawSky source ID.

## 2. Beam architecture (ratified D6)

**Geometry — FORGE.PRIM `beam_cone`**: world-anchored slanted cone, fixed orientation (never billboard-to-axis, except the crossed-sheet rung). (a) circumferential U mapped for the radial-fade texture, seam at the back (α≈0, invisible); (b) V mapped to axis height; (c) vertex-colour fade band over the bottom fraction (`fade_band`, default 0.2) — base dissolves above ground; (d) base ring at ARM-supplied ground height from canonical terrain (no FPGA terrain query); (e) bounded subdivision caps as spec constants.

**Material — `beam_additive_fade`**: `colour = tex.RGB × vertex.RGB; dst = sat(dst + src)`. Additive fast-path class. **Bilinear TMU mandatory** (nearest 16-texel ramp = visible stairs). Texture 16×64 **direct colour** RGB565/ARGB4444 — deliberately not CLUT, so bilinear never touches a palette. No alpha channel, no sorting (addition commutes; charter §26 no-OIT refusal is moot; coarse depth binning harmless). No dithered alpha in steady state; resolve-time ordered dither only; LOD crossfades reuse the §9 stable screen-space dither.

**Occlusion — two exact mechanisms**:
1. *Per-fragment*: pass-6 depth test against opaque invw24 — beams behind terrain are pixel-exactly rejected, free. Depth-write OFF: beams never occlude anything, including each other. This is Sacrifice's contract exactly (depth-test ON, depth-write OFF, additive, unlit).
2. *Per-beam dimming (DDA, ARM)*: per beam per active view, one ray from the cone apex toward that view's camera eye, marched against the canonical heightfield (base + baked scars): N = 64 fixed parametric steps, fixed point; `occluded = count(step.terrain_height(x,z) > step.y)`; `v = 1 − occluded/64`; `intensity = round(v·255)` → u8 in the DRAW_PROCEDURAL descriptor. Short-circuit: apex above the heightfield global max + monotonic-upward ray → 255. §9 minimum-hold hysteresis on intensity. Oracle: `zref::beam::occlusion_intensity`.

**Representation ladder** (Measure vocabulary; §9 hysteresis + min-hold; thresholds in `pixel` units):

| Projected width | Representation | Triangles |
|---|---|---|
| ≥ 24 px | 8–12-sided cone, bilinear U/V ramp | 16–24 |
| 12–24 px | 6-sided cone | 12 |
| 6–12 px | 2 crossed billboard sheets | 4 |
| 2–6 px | soft sprite (PART.SOFT) | 2 |
| < 2 px | culled | 0 |

Min projected-width policy: ≥6 px (Z60), ≥5 px per Duo view — below that, rungs, not thin cones.

## 3. Mirror Gate secondary — `radial_decay` (ratified D7)

Mode of POST.COMPOSITE's glow path. Per texel of the 96×60 (Z60) / 2×64×48 (Duo) glow buffer: `delta = (uv − sunUV) · density/12`; 12 taps; `acc += s · w[i]` with the frozen table `w[i] = round((61/64)^i · 65536)`; guard-clamped march distance; saturating u16 accumulate; rounding per `spec/qformats.md` rescale law. Upscaled additive composite via the existing bloom path. Sun/glow gather rides the existing effect-tag path unchanged.

**Frozen constants**: buffer 96×60 / 2×64×48; 12 taps; decay 61/64 (exact binary fraction — Q0.16 table is exact); guard clamp on |uv−sunUV|. Cost 69,120 / 73,728 taps ≈ 4% of a 1.67M-cycle frame (100 MHz placeholder; Phase 0 freezes the clock); 11.5–12.3 KB M10K POSTBUF; ~0.5–1% ALM inside the 6% twod_post group; §26 cut-order 6 protects it.

## 4. Command surface (post-W4 patch)

One new semantic command (see ADDENDUM §4 for exact .zidl — `DrawSky 0x0310` with `sky_set` handle, per-view `rot_proj[2]`, cloud scroll u/v, `drum_yaw`, viewport mask, layer flags; reserved `0x0311..0x031F` for extensions; version-bump this file before allocating). Beams: **no new opcode** — `DRAW_PROCEDURAL` with forge kind `beam_cone`; parameter layout (apex/axis/height/radii/fade_band/intensity[2]/min_px/semantic_weight) defined inside the generic parameter blob. Bootstrap lowering: `DRAW_SCREEN_TRIANGLES` (charter §6); game-facing meaning never changes.

## 5. Form declarations (present domain)

`sky { bands, cap, under, clouds (scroll/direction/max_alpha), sun (energy), background, drum_yaw period, fog_exempt }` and `beam { anchor, height, slant, radius top/bottom, tint, fade_band, min_px, semantic_weight, ladder {…}, hysteresis }` — shapes recorded in ADDENDUM §6; land in `spec/form/language_semantics.md` when Phase 3 expands it.

## 6. ZRef preview functions

`zref::sky::emit_layers(SkySet, tick, view) → SkyPrimitive[]`; `zref::sky::cloud_vertex_alpha(r2_fx16, max_alpha_fx16) → u8`; `zref::beam::emit_cone(BeamDesc, rung) → triangles`; `zref::beam::projected_width(desc, camera) → pixel`; `zref::beam::occlusion_intensity(apex, eye, heightfield) → u8`; `zref::post::radial_decay(glow_in, sun_uv) → glow_out`. Integer-only, single rounding law. Phase 3's software renderer consumes them → Wound Lab gets the Sacrifice look before Phase 11 RTL.

## 7. Test plan

`sky_golden_two_cameras` (yaw sweep 64 steps, pitch to zenith, down-past-edge under-plane view; CRC-locked .zcap per view) · `sky_pass1_equivalence` (prefill vs late-drawn, byte-equality) · `sky_scroll_determinism` (tick T vs T+3840 byte-equal) · `sky_fallback_clear` (every pixel written) · `beam_ladder_thresholds` (24/12/6/2 px straddles; hysteresis no-flip) · `beam_occlusion_hill` (depth rejects; two overlapping beams both visible) · `beam_dda_differential` (ARM DDA == oracle == brute-force) · `sun_through_cloud` · `post_radial_decay` (frozen constants, guard clamp, saturation) · `cost_assertions` · full capture-corpus replay after every integration change.

## 8. Ledger hooks

No new blocks; no ops.yml entries (sky/beams are not field programs). Notes amended on FORGE.PRIM, TWOD.PLANE, POST.GATHER, POST.COMPOSITE (purpose), PART.SOFT, RASTER.FRAGMENT, SW.CPUCOLL. Maturity path: this spec + oracle functions + golden captures + DDA corpus → FORGE.PRIM / POST.COMPOSITE may advance to REFERENCE_COMPLETE; RTL is Phase 11 (ZH-044/ZH-046).
