# Sky and Beams Specification (Sacrifice-style 360° sky + god beams)

**Status:** RATIFIED v1 (2026-08-14) — architecture addendum from run RUN-20260814-2015 (evidence: reverse-engineered Sacrifice engine structure + architecture-fit recons; see `runs/CLAUDE-RUNS/RUN-20260814-2015-sacrifice-sky-and-beams/`). **v1.1 (2026-08-16): §1.2 elevation-ramp continuity law added** — closes the layer-join colour-discontinuity spec gap the project owner reported ("ovals, not sky"). **v1.2 (2026-08-17, lighting & pose consolidation wave, RUN-20260816-0046): §1.3 sky-set crossfade law and §4a the environment-state record (`SetEnvironment 0x0311`) added; §8 notes amended.** The crossfade decision unblocks the queued rain work (owner idea #9); the environment record is the ABI home of sun/ambient/tint/fog state that S6's lighting recon identified as the highest-leverage gap.
**Authority:** this file owns sky/beam/environment-light state semantics. Charter §8 pass 1 ("terrain/backdrop prefill") admits the sky prefill without charter amendment. ABI reservation: `0x0310..0x031F` (`DrawSky 0x0310` + extensions; lands in `spec/commands.zidl` post-W4; `SetEnvironment 0x0311` allocated by the v1.2 amendment — the first allocation from the extension range). Version this file on any semantic change.
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

### 1.2 Elevation-ramp continuity law (amendment, 2026-08-16)

**Defect this closes (spec gap, not an emitter bug):** v1 of this file assigned each layer its own colour source and never required continuity across the layer joins. Rendered from any normal perspective camera the three geometric seams — under-plane rim → lower band horizon, band equator (z = 0), upper band top → cap rim — then read as **hard elliptical outlines** ("an oval on top, an oval at the bottom"; project owner, 2026-08-16). The seams are geometric and unavoidable; they are *visible* only when colour jumps across them.

**The law:** the whole sky is one **elevation-indexed colour ramp**; the layers are geometry that samples it. A legal sky set is **C0 across every layer join**, to within one gradient banding step:

1. `under` == `band_lower_horizon` (the under-plane meets the lower band's inner edge);
2. `band_lower_top` == `band_upper_bottom` (the band equator at z = 0);
3. `cap` == `band_upper_top` (the cap rim meets the drum's top edge; the cap covers elevations above ≈ atan(2560/5120) ≈ 26.6° from the anchor, so `band_upper_top` IS the zenith colour).

For the textured layers (Phase 6+ assets) the same law binds the v-edge texel rows of adjacent layers: the asset compiler MUST reject a sky set whose adjacent edge rows differ. For the Phase-3 flat-colour stand-in it binds the `SkySet` colour fields as written above. A violating set renders with visible rims — that is an asset defect, not a renderer defect, and the enforcement lives where the colours are authored.

**Cap-fan pitch corollary (same amendment):** the zenith cap fan is emitted at the **drum's own column pitch (48 segments) and yawed column angles**, superseding §1.1 row 3's fixed 16-tri fan: a 16-gon rim inside the 48-gon drum leaves background slivers between its chords and the drum's top edge, and an unyawed rim detaches from a yawed drum — both render as thin dark arcs at the cap join. Using the identical `angle16` raws makes rim and drum-edge vertices bit-identical, so the join is exact.

**Projection corollary (same amendment):** the Phase-3 software renderer previously projected sky vertices with `w = 1` (a linear map). That painted *both* sides of the inside-facing drum onto the screen — the behind-camera half over the far half — so layer joins rimmed against arbitrary band rows and no colour law could hide them. The corrected stand-in projects with **`w` = the rotation's z row** (true perspective, `ndc = xy/z`): the behind-camera half culls through the one `w ≤ 0` near-plane rejection, each sky direction is painted exactly once, and the authored field of view is the scale ratio of rows 0–1 to row 2. This mirrors what the hardware sky gets for free by riding the ordinary GEOM projection path with a rotation-only model transform; `rot_proj` itself remains rotation-only and validated exactly as §1 states. The under-plane is emitted as an 8×8 cell grid (128 triangles, the cloud sheet's own density) so behind-camera cells cull without deleting the plane (whole-primitive near-plane rejection is the documented Phase-3 clip model).

ENFORCED-BY: `tests/render/render_sky.cpp` `test_seam_continuity` (renders a pitch-spanning column through all three joins and asserts no join step exceeds the largest in-band gradient step).

### 1.3 Sky-set crossfade law (amendment v1.2, 2026-08-17)

**The decision this closes:** the rain queue (owner idea #9 — "rain that
darkens sky as clouds come in; spells cause weather") listed three
mechanisms for moving between sky sets, costs undecided: (a) a discrete
per-frame switch, (b) authored intermediate sets, (c) a per-tick
interpolation parameter in the reserved `0x0311..0x031F` range.

**Decision: (c)'s semantics without (c)'s opcode — the crossfade is
sim-side palette arithmetic whose products already cross the ABI lawfully.
(a) is its degenerate case; (b) is rejected.** Derivation:

1. Since §1.2 the whole sky is one elevation-indexed colour ramp sampled by
   geometry, and the textured layers (drum, cap, under) are **CLUT8** —
   colour lives in palette pages. The machine already ratified per-frame
   ARM palette rebuilds as a zero-fabric lane (stars_and_flares §1: ≤512 B
   upload/frame; the §3 ramp slew ±1/tick law is the anti-pop discipline).
   A sky crossfade is therefore `palette lerped by w`, not `fragments
   blended`: the fragment path samples ONE palette exactly as it does
   today — no second sample (§26's second-TMU refusal stays moot), no
   blend unit, no new recipe.
2. **(a) discrete switch** is legal today (DrawSky names any set per
   frame) but jumps the whole ramp at once — exactly the discontinuity
   class §1.2 was written to kill within a set. Adequate for hard cuts
   (level load, a spell's instant night); wrong for weather fronts and
   day cycles.
3. **(b) authored intermediate sets** approximates (c) with assets: N
   extra sets per transition pair at ~0.9 MB VRAM each (§1.1 costs), N×
   authoring, and it still pops between adjacent rungs. It buys the look
   of (c) at strictly higher cost. Rejected.
4. **(c) as state, not opcode:** the crossfade triple `{set_a, set_b, w}`
   is sim/presentation state (the weather system's, the day clock's). Its
   visible products already have lawful carriers: the ARM-rebuilt palette
   page (an ordinary resource upload — uploads are command-stream bytes,
   so captures replay them exactly) and the resolved environment values
   (`SetEnvironment 0x0311`, §4a). An opcode carrying `w` would add ABI
   surface to state that never needs to cross it; `0x0312..0x031F` stay
   unallocated. Escape hatch: if the rain wave finds the palette-upload
   bandwidth or a two-art transition needs an explicit command, it
   allocates from the reserved range under this file's version-bump law —
   the decision above is about WHERE the interpolation lives, not a
   refusal of the range.

**The law:**

- A sky transition is the pair `(set_a, set_b)` plus a monotonic weight
  `w ∈ [0, 1]` advanced per tick by the sim (deterministic; a spell's
  weather change is sim truth, never a renderer toggle). At `w` the
  active sky palette entry is `lerp(set_a[i], set_b[i], w)` per channel,
  round-half-up, computed by ARM into the active palette page each frame
  while `0 < w < 1`; at the endpoints the owning set's page is used
  verbatim.
- Per-set scalars crossfade by the same weight: `cloud max_alpha`,
  `sun energy` (§1.1), and the environment record's sun colour / ambient /
  tint + strength (§4a) — the resolved values are what the commands
  carry. One weight, one clock: sky, clouds, sun and the world-light tint
  move together; that unity is the point of the record.
- The §1.2 continuity law extends to crossfades: a lerp of two C0 ramps
  is C0 (each join equality is preserved by the per-channel lerp), and
  the ±1/tick slew discipline (stars_and_flares §3) governs palette
  changes during a transition — palette changes never pop, crossfades
  included.
- The cloud sheet is ARGB4444 direct colour (not CLUT): its crossfade
  rides the alpha path (`max_alpha` lerp through the ratified
  `sky_cloud_fade` blend), never a texture blend. A transition that needs
  different cloud ART authors it as palette + alpha-parameter work on one
  sheet; a second sampled sheet is not available to this law.
- Time-of-day and weather are the same law at different rates: a day
  cycle is a slow crossfade between dawn/day/dusk/night sets with
  `SetEnvironment` sun direction advancing per frame. No separate
  mechanism may be built for either.
- Capture-exactness: replay of a captured frame reproduces the sky,
  weather and light bit-exactly — the palette upload bytes and the
  resolved `SetEnvironment` values are in the frame; `w` itself is
  derived sim state and needs no chunk of its own (the celestial_state
  precedent: capture what the machine consumes, not what the sim was
  thinking).
- **Not costed here:** the per-frame palette-upload bandwidth for a sky
  in transition (a 256-entry CLUT8 palette page is 512 B at RGB565 — the
  stars' ≤512 B/frame line is the precedent, not a ratified sky budget);
  the ARM-side lerp cost; any DRAM traffic for the second set's palette
  source. The rain wave costs them against ZH-004 board truth before
  relying on the law at storm intensity.

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

One new semantic command (see ADDENDUM §4 for exact .zidl — `DrawSky 0x0310` with `sky_set` handle, per-view `rot_proj[2]`, cloud scroll u/v, `drum_yaw`, viewport mask, layer flags; reserved `0x0311..0x031F` for extensions [v1.2: `0x0311` is now `SetEnvironment`, §4a — the remainder `0x0312..0x031F` stays reserved]; version-bump this file before allocating). Beams: **no new opcode** — `DRAW_PROCEDURAL` with forge kind `beam_cone`; parameter layout (apex/axis/height/radii/fade_band/intensity[2]/min_px/semantic_weight) defined inside the generic parameter blob. Bootstrap lowering: `DRAW_SCREEN_TRIANGLES` (charter §6); game-facing meaning never changes.

## 4a. The environment-state record — `SetEnvironment 0x0311` (amendment v1.2)

**The gap this closes (S6 lighting recon §4.4/§5.1):** no ABI record
carried sun direction, ambient, global tint or fog state; the Phase-3
stand-in hard-codes one light; every global lighting cheat and the
determinism law (state not in a command cannot be in a capture) routed
through the missing record. This is the donor's `envi.d` per-level
lighting struct (sun direction/colour, ambient, fog — S6 §1) ported to
lawful widths — one directional sun + one ambient + fog, **no dynamic
point lights in the format** (the donor never had them; local light is
the flare/beam/stamp machinery, S6 §2).

**Command:** `SetEnvironment 0x0311` (reserved; layout frozen in
`spec/commands.zidl` since ABI v3, execution lands with the
weather/lighting wave): sun direction as an `angle16` pair, sun colour /
ambient / tint as `rgb565`, `tint_strength` unit8, fog mode + near/far
(fx16). Per-frame **global** state — no viewport mask: one world, one sun
(Duo views share it; `SetView` remains the per-view record).

**Consumption laws:**

- **Sun direction.** `sun_yaw`/`sun_pitch` are turns (qformats §2);
  `0x4000` pitch = zenith. The world-space unit direction is

  ```
  L = ( fx_mul(cos p, sin y),  sin p,  fx_mul(cos p, cos y) )
  ```

  with `fx_sin`/`fx_cos` (qformats §7.1) and single-rounded products
  (§3). `yaw = 0` faces +Z; the convention is content's to steer. The
  sky set's authored sun anchor (§1.1 row 6) MUST equal this direction —
  the asset compiler rejects a divergent set (the §1.2 enforcement home:
  defects live where the values are authored).
- **Vertex light.** In GEOM.PROJECT (its contract purpose already owns
  "projection + lighting"), per vertex with unit normal `N`:
  `ndl = clamp(N·L, 0, 1)` (fx dot, single rounding), then per channel
  `lit = sat_u8( ambient_c + rescale_u(sun_c · ndl, 8) )`. The Phase-3
  stand-in's hard-coded floor — `0.25 + 0.75·lambert` under one light
  (1,2,1)/√6 (`reference/src/zrender/terrain.cpp`, the `ambient` lambda:
  `16384 + rhu(shade·49152)`) — is this law at `ambient = 0x4000`,
  `sun = 0xC000` in the Q16.16 lanes; the record parameterises what the
  stand-in froze. RGB565 → 8-bit expansion by bit replication
  (`c8 = (c5 << 3) | (c5 >> 2)`, `(c6 << 2) | (c6 >> 4)` — the frozen
  stars §2 expansion).
- **Global tint** (time-of-day / weather mood; the §1.3 crossfade's
  world-light leg). Per channel, applied to the LIT vertex colour before
  texture modulation (the donor's `lmap` position — tint the light, not
  the albedo): `lit' = sat_u8( lit + rescale_s((tint_c − lit) · s, 8) )`
  with `s = tint_strength` (unit8). One multiply per channel per vertex;
  zero fragment cost.
- **Fog.** Mode/near/far ride this record; the formula, Q-formats,
  rounding, per-vertex placement, colour binding (to this file's horizon
  ramp) and the exempt list are law in `spec/qformats.md` §8 (the raster-
  basics home; one truth for the numbers). `fog_far ≤ fog_near` disables
  fog regardless of mode — a deterministic no-op, not an error.
- **Defaults.** A frame with no `SetEnvironment` keeps the previous
  state; the power-on default is sun pitch `0x4000` (zenith), sun colour
  packed 565 `0xBDF7` (lanes 23,47,23 — expands to (189,190,189)),
  ambient `0x4208` (lanes 8,16,8 — expands to (66,65,66)), tint white at
  strength 0, fog off. That is the stand-in's `0.25 + 0.75·ndl` to
  within one 565 quantum per channel, so wiring the record reproduces
  today's look by construction. (The 5- and 6-bit lanes expand by
  different replication laws, so no exact grey exists off the r5=g6=b5
  endpoints 0/255 — the packed values above are the law, not a grey
  intent; white sun AND white ambient would rail every lit channel, so
  the 565 lanes carry the strengths.)

**Capture:** the record's state serialises into the `ENVIRONMENT_STATE`
.zcap chunk (capture_format.md §4.2) so replay from any captured frame
reproduces light and fog bit-exactly — the determinism law stated where
the container lives. The Phase-3 stand-in keeps its hard-coded light
until the consuming wave wires the record; the chunk and the struct exist
now so that wiring is a consumer change, never a format change.

## 5. Form declarations (present domain)

`sky { bands, cap, under, clouds (scroll/direction/max_alpha), sun (energy), background, drum_yaw period, fog_exempt }` and `beam { anchor, height, slant, radius top/bottom, tint, fade_band, min_px, semantic_weight, ladder {…}, hysteresis }` — shapes recorded in ADDENDUM §6; land in `spec/form/language_semantics.md` when Phase 3 expands it. **[v1.2]** `environment { sun { yaw, pitch, colour }, ambient, tint { colour, strength }, fog { mode, near, far } }` lowers to `SetEnvironment 0x0311` (§4a); the crossfade weight of §1.3 is weather-sim state and has no form shape of its own — the weather author drives `w`, the form emits the resolved record.

## 6. ZRef preview functions

`zref::sky::emit_layers(SkySet, tick, view) → SkyPrimitive[]`; `zref::sky::cloud_vertex_alpha(r2_fx16, max_alpha_fx16) → u8`; `zref::beam::emit_cone(BeamDesc, rung) → triangles`; `zref::beam::projected_width(desc, camera) → pixel`; `zref::beam::occlusion_intensity(apex, eye, heightfield) → u8`; `zref::post::radial_decay(glow_in, sun_uv) → glow_out`. Integer-only, single rounding law. Phase 3's software renderer consumes them → Wound Lab gets the Sacrifice look before Phase 11 RTL.

## 7. Test plan

`sky_golden_two_cameras` (yaw sweep 64 steps, pitch to zenith, down-past-edge under-plane view; CRC-locked .zcap per view) · `sky_pass1_equivalence` (prefill vs late-drawn, byte-equality) · `sky_scroll_determinism` (tick T vs T+3840 byte-equal) · `sky_fallback_clear` (every pixel written) · `beam_ladder_thresholds` (24/12/6/2 px straddles; hysteresis no-flip) · `beam_occlusion_hill` (depth rejects; two overlapping beams both visible) · `beam_dda_differential` (ARM DDA == oracle == brute-force) · `sun_through_cloud` · `post_radial_decay` (frozen constants, guard clamp, saturation) · `cost_assertions` · full capture-corpus replay after every integration change.

## 8. Ledger hooks

No new blocks; no ops.yml entries (sky/beams are not field programs). Notes amended on FORGE.PRIM, TWOD.PLANE, POST.GATHER, POST.COMPOSITE (purpose), PART.SOFT, RASTER.FRAGMENT, SW.CPUCOLL. Maturity path: this spec + oracle functions + golden captures + DDA corpus → FORGE.PRIM / POST.COMPOSITE may advance to REFERENCE_COMPLETE; RTL is Phase 11 (ZH-044/ZH-046).
**[v1.2]** `SetEnvironment` rides existing blocks only: the vertex-light/tint/fog stage is GEOM.PROJECT's chartered "projection + lighting" purpose (per-vertex ALU cost NOT costed — no vertex cost model exists beyond 1 vertex/clock; the S6 §2 finding, restated so nobody reads "rides" as "free"), the palette crossfade rides TEXTURE.TMU's hot-page lane (stars precedent), the emitted record rides SW.CMDBUILD (an `environment` module beside the celestial one). The ZRef mirror of the record (`zref::sky::EnvState`, serialize/deserialize) landed with this amendment for the capture chunk; the stand-in renderer does not yet consume it (wiring it is the weather wave's consumer change — reel CRCs were byte-identical through this amendment).
