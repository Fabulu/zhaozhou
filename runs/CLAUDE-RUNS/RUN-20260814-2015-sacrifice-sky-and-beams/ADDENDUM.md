# SKY AND BEAMS ARCHITECTURE ADDENDUM v1 — RATIFIED

*Architect consolidation of RUN-20260814-2015 (R1 Sacrifice evidence, R2 skybox fit, R3 godbeams fit), 2026-08-14. Persisted by orchestrator. ABI changes are post-W4 patches against reserved range 0x031n.*

*(Full text is the controlling document for this piece; the implementable spec derived from it is committed to the repo as `spec/sky_and_beams.md`. This file records the ratified decisions and proposals verbatim for provenance.)*

## 1. Ratified decisions table

| # | Decision | Ruling | Rationale |
|---|---|---|---|
| D1 | **Pass placement** | **Hybrid**: drum (both bands) + zenith cap as charter §8 pass-1 backdrop prefill (Z-forced-far, replaces the clear); under-plane as ordinary opaque geometry (real world depth); cloud sheet + sun quad in pass 6 translucent. | Pixel-equivalent to Sacrifice's late-drawn sky for every opaque-geometry case (proof in addendum §1.1), net-zero fragment cost for the drum, and the only layers that genuinely need real depth get it. |
| D2 | **Sky geometry** | FORGE.PRIM `sky_drum` (48 columns × 2 bands, inside-facing winding) + `sky_cap` (16-tri fan) + `sky_plane` (under-world) + `sky_cloud_sheet` (8×8 vertex-alpha grid). No dome, no cube atlas. | Drum walls are planar ⇒ exact per-quad UV interpolation; 48 cols = 0.5 px rim kink; u-wrap gives a mathematically seamless 360° join with a TMU mode §15 already mandates. |
| D3 | **Cloud radial fade / sun punch-out** | Radial fade `α·(1−r²)` as **baked per-vertex alpha** on an 8×8 generation grid. Sun punch-out `sGap=min(1,96r²)` **approximated**: no runtime cloud-alpha modification; the additive sun quad (optionally with a soft halo gradient baked to the inverse-sGap curve) burns through visually. | sGap is per-pixel position-dependent — not fixed-function expressible without a second dependent sample (§26-refused). Zero fabric, deterministic; documented revisit hook if Wound Lab footage demands hard burn-through. |
| D4 | **Animation constants** | Cloud scroll **1 tile / 64 s** kept: `scroll_u = ((tick % 3840) << 16) / 3840`, `scroll_v = −scroll_u`. Drum rotation 2π/512 s **dropped by default** (`drum_yaw` field exists, `period = 0` = disabled). Sun luminance key `min(3·lum, 1)` **baked at asset-compile** into the sun texture's alpha. | All become pure integer functions of tick (capture-exact) or offline bakes. |
| D5 | **Command surface** | **One new semantic command `DrawSky 0x0310`** carrying the whole layered sky state. Beams ride **`DRAW_PROCEDURAL`** with forge kind `beam_cone` (no `DRAW_BEAM`). | Sky is per-frame per-view state like SetView; beams are independent world objects. Minimal set = exactly one new opcode. |
| D6 | **Beam architecture** | R3 ratified in full: `beam_cone` Forge mode, `beam_additive_fade` recipe (bilinear TMU **mandatory**), 5-rung projected-width ladder, refusal of tile-depth-percentile fade. **DDA occlusion semantics fixed**: one ray per beam **per view**, from the beam's source point (cone apex) toward **that view's camera eye point**, marched against the canonical ARM heightfield, N=64 fixed steps. | Depth-test against opaque invw24 reproduces Sacrifice's occlusion contract pixel-exactly for free; "toward camera" is the only choice that matches "the viewer cannot see the source". |
| D7 | **POST secondary** | `radial_decay` mode of POST.COMPOSITE with **frozen constants**: buffer 96×60 (Z60) / 2×64×48 (Duo), 12 taps, `decay = 61/64` (exact binary fraction ⇒ `w[i] = round((61/64)^i · 65536)` exact rational table), guard clamp, saturating u16 accumulate, zero VRAM traffic. | One bounded mode of one bounded buffer, like bloom; 61/64 makes the weight table exact in Q0.16. |
| D8 | **Ledger changes** | **No new blocks.** FORGE.PRIM, TWOD.PLANE, POST.GATHER, POST.COMPOSITE, PART.SOFT, RASTER.FRAGMENT, SW.CPUCOLL note/purpose amendments only. **No ops.yml changes** — sky and beams are not field programs. | Every capability is a mode/recipe/constant of an existing block. |
| D9 | **Fog** | Sky set is **world-fog-exempt** as a material-recipe flag; under-plane texture carries any distance tint pre-baked. | Era practice; keeps fog deterministic. |
| D10 | **Effect tags / Mirror Gate** | Sun quad writes the 8-bit glow effect tag; POST.GATHER picks it up unchanged; radial_decay consumes the gathered sun position. | Zero new hardware. |

**Pixel-equivalence proof (D1)** — recorded in full in the addendum §1.1: identical output for ¬S (sky-only pixels), S (geometry-covered pixels; prefill cost = the clear it replaces), and translucent/particles (pass 6/7 composite over the same depth). Non-equivalences resolved by the hybrid: under-plane parallax (real depth), cloud/sun occlusion by mid-air geometry (pass 6 depth test), fog (recipe exemption).

## 2. Layer-by-layer sky spec (money table)

| Layer | Primitive (FORGE.PRIM mode) | Pass | Depth / blend state | Texture | Animation (tick-derived, exact) | Cost (Z60 / per view) |
|---|---|---|---|---|---|---|
| Lower band `skyb` (below horizon, z −2560…0, r 4096→5120) | `sky_drum` band 0 — 48 columns, 8 mirrored texture repeats, inside-facing winding | **1 (prefill)** | Z-test **off**, Z-write = **far constant**, blend **off**, effect-tag init | 1024×128 **CLUT8**, u-**mirror** wrap, v-clamp, +mips | `drum_yaw` (default 0) | 96 tris; fragments = the clear it replaces |
| Upper band `skyt` (z 0…+2560, r 5120) | `sky_drum` band 1 — same contract | **1 (prefill)** | same | 1024×128 CLUT8, u-mirror, +mips | `drum_yaw` | 96 tris |
| Zenith cap | `sky_cap` — 16-tri fan at z = +2560 | **1 (prefill)** | same | 64×64 CLUT8, clamp, +mips | none | 16 tris |
| Under-plane `undr` (world floor below the floating island — **critical Wound Lab layer**) | `sky_plane` — 10240×10240 quad at z = −2560, seamed to band 0's inner radius by the generator | **3 (opaque)** | Z-test **on**, Z-write **on**, blend off, fog-**exempt** recipe | 512×512 CLUT8, clamp, +mips | `drum_yaw` (shares assembly rotation when enabled) | 2 tris |
| Cloud sheet `sky_` (z = +1792) | `sky_cloud_sheet` — 8×8 vertex grid (128 tris), UV 0..4 (4× repeat), per-vertex `α = (1−r²)·max_alpha` baked at generation in fx16 | **6 (translucent)** | Z-test **on**, Z-write **off**, **alpha blend** `sky_cloud_fade`: `out = dst·(1−a) + src·a`, `a = tex.a × vertex.a` | 256×256 **ARGB4444**, u/v-**repeat**, +mips | scroll: `u += ((tick % 3840) << 16)/3840`, `v −= same` (1 tile / 64 s, direction (1,−1)); `max_alpha` per set | 128 tris; ≤ ~½-screen blended fragments (~45K worst) |
| Sun `sun_` (z = +2560, world-fixed quad, NOT billboarded) | generated with the sky set | **6 (translucent)** | Z-test **on**, Z-write **off**, **additive** `sun_additive`: `dst = sat(dst + src·tex.a)`, **glow effect-tag write on** | 64×64 ARGB4444, alpha pre-baked to `min(3·lum, 1)`; optional halo variant baked to the inverse `min(1,96r²)` curve | none (fixed in the sky set; energy per set, Sacrifice ~42) | 2 tris |

Assembly: anchored to map centre, world-fixed (never camera-translated). DrawSky's `rot_proj` per view has translation rows **zero** (validated). Fallback: DrawSky absent / layer flag-disabled / uncovered direction → flat clear in the sky-set background colour — **no pixel ever left unwritten**. Sky material class = cheapest; Measure-exempt with own budget lines (`sky_triangles`, `sky_fragments`), fully counted in §25 counters, carrying the DrawSky source ID. Pass-1 ordering: sky prefill first, then terrain prefill.

## 3. Beam spec

**Geometry (FORGE.PRIM `beam_cone`).** World-anchored slanted cone, fixed orientation (never billboard-to-axis except the crossed-sheet rung). Contract: (a) circumferential U mapped for radial-fade texture, seam at the back (alpha≈0, invisible); (b) V mapped to axis height; (c) vertex-colour fade band over bottom fraction (`fade_band`, default 0.2); (d) base ring at ARM-supplied ground height from canonical terrain; (e) bounded subdivision caps as spec constants.

**Material (`beam_additive_fade`).** `colour = tex.RGB × vertex.RGB; dst = sat(dst + src)` — additive fast-path class; **bilinear TMU mandatory**. No alpha channel, no sorting (addition commutes; §26 no-OIT refusal moot). No dithered alpha in steady state; resolve-time ordered dither only; crossfades reuse §9 stable screen-space dither. Texture: 16×64 **RGB565/ARGB4444 direct colour** (deliberately not CLUT — bilinear never touches a palette).

**Occlusion (two mechanisms, both exact).**
1. Per-fragment: pass 6 depth-tests every beam fragment against opaque invw24 — beam behind hill pixel-exactly rejected, free. Reproduces Sacrifice's contract (depth-test ON, depth-write OFF, additive, unlit) exactly.
2. Per-beam dimming (DDA): per beam per active view, ARM casts one ray from cone apex toward that view's camera eye against the canonical heightfield (base + baked scars). N = 64 fixed parametric steps, fixed point; `occluded = count(step.terrain_height(x,z) > step.y)`; `v = 1 − occluded/64`; `intensity = round(v·255)` → u8 in the DRAW_PROCEDURAL descriptor. Short-circuit: apex above global max + monotonic-upward ray → 255. Hysteresis: §9 minimum-hold.

**Ladder (Measure vocabulary; hysteresis + min-hold per §9):** ≥24 px → 8–12-sided cone (16–24 tris); 12–24 px → 6-sided cone (12); 6–12 px → 2 crossed billboard sheets (4); 2–6 px → soft sprite via PART.SOFT (2); <2 px → culled. Min width policy ≥6 px Z60 / ≥5 px per Duo view.

**POST secondary (`radial_decay`).** Per texel of 96×60 / 2×64×48 glow buffer: `delta = (uv − sunUV)·density/12`, 12 taps, `acc += s·w[i]`, `w[i] = round((61/64)^i·65536)`, guard-clamped, saturating u16 accumulate, wave-1 rescale rounding; upscaled additive composite via existing bloom path. 69,120 / 73,728 taps ≈ 4% of a 1.67M-cycle frame; 11.5–12.3 KB M10K; ~0.5–1% ALM in twod_post (6%).

## 4. Command reservations (post-W4)

One new command; beams need none. Apply only after W4's `spec/commands.zidl` lands — verify 0x031n free, follow its source-ID convention.

```text
// Sky and beams addendum (spec/sky_and_beams.md) — reserved range 0x0310..0x031F
command DrawSky 0x0310 {
    handle32 sky_set          // authored sky set: band0/band1/cap/under/cloud/sun
                              // pages + palettes, layout constants, max_alpha,
                              // energies, background RGB, fog-exempt flag
    mat4fx   rot_proj[2]      // per-view rotation-only view-projection;
                              // translation rows MUST be zero (validated)
    fx16     cloud_scroll_u   // ((tick % 3840) << 16) / 3840, floor
    fx16     cloud_scroll_v   // -cloud_scroll_u
    angle16  drum_yaw         // ((tick % period_ticks) * 65536) / period_ticks; 0 = off
    u8       viewport_mask
    u8       flags            // b0 under_plane, b1 cloud, b2 sun, b3 cap,
                              // b4 sun_glow_tag, b5..b7 reserved-0 (validated)
    u8       reserved0
    u8       reserved1
    u32      source_id        // if W4 froze per-command source IDs; else omit
}
// 0x0311..0x031F reserved for sky extensions. Never allocate without a
// spec/sky_and_beams.md version bump.
```

**Beams** (no new opcode): `DRAW_PROCEDURAL` with forge kind `beam_cone`; parameter block: apex `world3`, axis `world3`, height `fx16`, `top_radius`/`bottom_radius` `fx16`, `fade_band` `u8`, `intensity[2] u8` (per-view DDA), `min_px` `fx16`, `semantic_weight` `u8`, material `beam_additive_fade`. Defined inside the generic parameter blob (spec-level, no ABI change).

## 5. Ledger patch list

No new blocks; no ops.yml changes. Notes/purpose amendments to FORGE.PRIM, TWOD.PLANE, POST.GATHER, POST.COMPOSITE, PART.SOFT, RASTER.FRAGMENT, SW.CPUCOLL (exact YAML in the applied patch). Cost-model additions: `sky_triangles ≤ 352` total (×2 views), `sky_fragments ≤ 92,160 + cloud ~45K blended`, beam fragment class ~24K, `beam_triangles` per rung 16–24/12/4/2/0, `post_radial_taps` 69,120/73,728, `arm_beam_rays` ≤8/≤10. VRAM: full sky set ≈ 0.9 MB (~1.9% of texture pool), shared between Duo views.

## 6. Form declaration shapes

```form
sky dusk_veil {
    bands  "dusk_band_low", "dusk_band_high"     // u-mirror lat-long strips
    cap    "dusk_cap"
    under  "dusk_under"
    clouds "veil_clouds" scroll 1 tile / 3840t direction (1, -1) max_alpha 0.82
    sun    "dusk_sun" energy 42
    background colour8 0x1A1030
    drum_yaw period 0t                             // disabled; 30720t = 1 rev / 512 s
    fog_exempt
}

beam godshaft {
    anchor world3                // base ring centre (ground)
    height fx16                  // along the slant axis
    slant  vector<world>         // fixed world axis; never billboarded
    radius top fx16 bottom fx16
    tint   colour8
    fade_band unit8 = 0.2
    min_px pixel = 6.0px
    semantic_weight high

    ladder {
        cone_12 while projected_width >= 24.0px
        cone_6  while projected_width >= 12.0px
        sheets  while projected_width >= 6.0px
        sprite  while projected_width >= 2.0px
        cull    otherwise
    }
    hysteresis per §9
}

present {
    sky dusk_veil
    beam godshaft at altar_top importance high views both
}
```

## 7. Spec files + charter amendment PROPOSALS

Spec files: `spec/sky_and_beams.md` NEW (owns everything above); `spec/commands.zidl` post-W4 patch; `spec/raster_rules.md` future Phase 4/5 sections (pass-1 backdrop semantics, pass-6 sub-order, blend/rounding law, fog-exempt flag); texture spec note when it lands (sky page layouts, `mip_bias`); `spec/form/cost_model.md` lines when W5 lands it; `spec/form/language_semantics.md`/`domains_and_effects.md` shapes when Phase 3 expands them.

**Charter amendment PROPOSALS (user-surface only — root charter MDs are not edited by agents):**
1. §14 (Primitive Forge list) — append: `sky drum/cap/plane/cloud-sheet; beam cone`.
2. §15 (Material recipes) — append: `sky backdrop write; cloud fade (tex.a × vertex.a); beam additive fade (bilinear mandatory)`.
3. §16 (Mirror Gate effects) — append `sun shafts (radial-decay glow mode, frozen constants)`; clarify Twin Horizons "sky" = overlay layers above the geometry sky.
4. §8 — **no amendment needed**; state the prefill admission in spec/sky_and_beams.md.

## 8. ZRef software preview + maturity path

Oracle functions (C++, ZRef, integer-only): `zref::sky::emit_layers(SkySet, tick, view) → SkyPrimitive[]`; `zref::sky::cloud_vertex_alpha(r2_fx16, max_alpha_fx16) → u8`; `zref::beam::emit_cone(BeamDesc, rung) → triangles`; `zref::beam::projected_width(desc, camera) → pixel`; `zref::beam::occlusion_intensity(apex, eye, heightfield) → u8`; `zref::post::radial_decay(glow_in, sun_uv) → glow_out`. Phase 3 software renderer consumes them → Wound Lab gets its Sacrifice look before Phase 11 RTL. Maturity: spec committed + oracle functions + golden two-camera captures + DDA differential corpus → FORGE.PRIM / POST.COMPOSITE may advance to REFERENCE_COMPLETE; RTL remains Phase 11 (ZH-044/ZH-046).

## 9. Test plan

`sky_golden_two_cameras` (yaw sweep 64 steps, pitch to zenith, down-past-island-edge under-plane view; CRC-locked .zcap); `sky_pass1_equivalence` (prefill vs late-drawn sky, byte-equality); `sky_scroll_determinism` (tick T vs T+3840 byte-equal); `sky_fallback_clear` (every pixel written); `beam_ladder_thresholds` (24/12/6/2 px straddles, hysteresis no-flip); `beam_occlusion_hill` (depth rejects; two overlapping beams both visible — no depth write); `beam_dda_differential` (ARM DDA == oracle == brute-force); `sun_through_cloud`; `post_radial_decay` (frozen constants, guard clamp, saturation); `cost_assertions`; full capture-corpus replay after every integration change.

## 10. Risks

R1 10240-unit cloud quad through clipper → clip at near plane before setup; 8×8 grid cap is a spec constant; watch `triangles_clipped`/`max_tile_list_depth`; escalate on counter evidence only. R2 under-plane vs fog → fog-exempt + pre-baked tint (art knob). R3 Duo double-cloud → ~90K blended vs 950K budget; per-view cloud disable flag exists (form negotiates). R4 bilinear vs CLUT → beam ramp is direct-colour. R5 W4 collision / opcode drift → post-W4 patch gated on W4's final opcode map; prefill is a recipe state on ordinary triangles, not a new clear opcode; spec carries a version field + explicit range reservation.
