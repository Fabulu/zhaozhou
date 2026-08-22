# FINDINGS — R2: 360° Skyboxes for Zhaozhou: Architecture Fit

**Run:** RUN-20260814-2015-sacrifice-sky-and-beams — **Date:** 2026-08-14 — **Agent:** RECON R2
*Persisted by orchestrator. Verdict below is the agent's; the piece architect ratifies.*

## SUMMARY

**Recommendation: Hybrid — a Forge-generated "sky drum + cap" (cylinder panorama + zenith disc) drawn as the charter §8 pass-1 backdrop prefill, with Twin Horizons planes as animated cloud/gradient overlays. It fits inside existing law with ZERO charter amendments and possibly zero new opcodes.**

1. **Sky as geometry: YES, and the drum beats the dome.** A lat-long dome needs ~10.7° tessellation for <1px horizon error (sagitta Δθ²/8 × 229 px/rad ⇒ 9 rings × 36 cols ≈ 648 tris/hemisphere). A **drum** (planar quads, exact perspective-correct UV per quad) needs only enough columns to hide rim kinks: 32 cols = 3.5px kink (fine for gradients), 48–64 cols = sub-pixel (fine for visible horizons) ⇒ **144–288 tris/view**. A 12-tri cube is texture-exact per face (planar faces ⇒ linear UV in screen space) but loses on texturing (below).
2. **Translation-strip: don't touch GEOM.PROJECT.** ARM already owns camera math; a `DrawSky` command carries its own **precomputed rotation-only view-projection mat4fx per view**. GEOM.PROJECT stays a plain matrix multiply with a different matrix source — no `sky_mode`, no new transform block. Winding: author the drum inside-out (generator-side); **no cull-flip hardware needed**. Depth: sky IS the tile clear — pass 1 writes constant-far Z, no Z-test; later geometry overwrites normally. Sky doubles as color clear (free fragment pass).
3. **Texture: lat-long wrap strip, NOT a cube atlas.** TMU already has u-wrap ⇒ seamless 360° join for free. Cube atlas needs per-page clamp and bleeds across page edges under bilinear at low res with CLUT8/RGB565 and no border padding — the classic seam. Lat-long pole singularity is avoided entirely by the drum+cap split (cap is its own small disc texture).
4. **Mip/grazing: bilinear+mip is enough at 240p.** A 1024×256 panorama gives ~2.8 tex/° vs ~5.1 px/° at the horizon — the texture is the limiting resolution, not filtering; anisotropic is §26-refused anyway. Optionally a per-material mip-bias spec constant (era/modern convention: mild negative −0.5…−1.0 sharpens, risks shimmer). Default 0.
5. **Twin Horizons 2D sky alone: NO.** Per-line scroll = yaw cylinder (Doom's 256-column sky) — but pitch is inexpressible (no per-line vertical remap in affine planes), and planes composite via POST, so a base sky can't be depth-occluded except in the cut-order-4 world-depth mode. Use planes for scrolling clouds/gradient *over* the geometry sky (charter §16 lists "sky" for planes — still true).
6. **Duo cost: trivial.** Sky is per-camera rotation-only: 2×144–288 tris (≤3.2% of the 18k Duo triangle example), fragments ≤ 92,160 (Z60) / 98,304 (Duo) ≈ 10% of the 950k fast-fragment budget — and it *replaces* the clear, so net cost ≈ 0 vs a cleared framebuffer. Cheapest material class (1 bilinear sample, no blend, no early-Z rejects possible).
7. **Mirror Gate:** sky writes the existing per-pixel 8-bit effect tag (tile storage §8) via the already-specified "glow/distortion writer" material recipe; POST.GATHER picks up sun/horizon glow from resolved tiles. Zero new hardware.
8. **Measure/LOD: exempt with own budget line.** Sky has no pixel-error ladder; declare it a **fixed backdrop cost** in the presentation contract + `costs.zcost` (`sky_triangles`, `sky_fragments` lines), still fully counted in §25 counters and carrying a source ID. The Measure never traverses it.
9. **Determinism:** all animation (scroll offset, palette phase, twinkle) = command parameters per tick ⇒ capture-exact via the ordinary frame packet; texture pages + hashes already ride in `.zcap`. No palette uploads needed (phase-parameter, not CPU palette mutation).
10. **Artifacts:** no new blocks (reuse FORGE.PRIM primitive enum + note in its contract); `DrawSky 0x0310` reservation in commands.zidl (or fold into `DRAW_PROCEDURAL` — §6A's "include" permits either); raster_rules.md pass-1 semantics + `Z_FORCE_FAR`/backdrop flag; verified **no ops.yml additions** (sky is not a field program — grep confirms none exist, none needed). No charter amendment required.

## FULL DETAIL

### A. Pixel-error math (why drum, how many triangles)

Screen scale: Z60, 240px height, assume ~60° vertical FOV ⇒ 229 px/radian.

- **Dome (lat-long sphere):** faceting angular error = chord sagitta ≈ Δθ²/8. For ≤1px screen error: Δθ ≤ √(8/229) = 0.187 rad ≈ 10.7°. Hemisphere: 9 rings × 36 columns × 2 = **648 tris**. Additionally each lat-long quad is non-planar, so UVs wobble within triangles near the horizon.
- **Drum (cylinder) + cap:** wall quads are planar ⇒ perspective-correct UV interpolation is *exactly linear per quad* — no in-face error at any tessellation. Only the rim polygon kink remains: N-gon deviation (2π/N)²/8 × 229 px. N=32 ⇒ 3.5px (invisible for gradients/soft panoramas); N=48 ⇒ 0.5px; N=64 ⇒ 0.28px (crisp painted horizons). Kinks are static in world space — deterministic, no shimmer.
- **Chosen geometry:** drum 48 columns × 2 bands (192 tris) + 16–32-tri cap fan ⇒ **~224 tris/view**; 32-column economy mode = 144 tris. Duo worst case 448 tris ≈ 2.5% of the 18,000-tri example budget.
- **Sub-16 feasibility:** a 12-tri cube is texture-exact (planar faces) but needs 6 clamped atlas pages → seams (see C). A 16-tri drum (8 cols) shows 14px rim kinks — only acceptable under a permanent fog band. Verdict: sub-16 is *possible* but wrong; ~150–290 tris is the sweet spot and still rounding-error cheap.

### B. Pipeline placement and mechanics

- **Pass order:** charter §8 pass 1 is literally "terrain/backdrop prefill" — sky slots there today, no amendment. Sky executes as the tile's Z/color initialization: Z-write = constant far (inverse-depth far value), no depth test, no blend, single bilinear TMU sample, optional effect-tag write. This makes the sky pass replace the clear — its fragment cost is net-free.
- **View lock:** `DrawSky` carries `mat4fx rot_proj[2]` (one per Duo view), computed by ARM from the same camera data that builds SetView (translation rows zeroed). GEOM.PROJECT is untouched — it just multiplies. This respects §29-16 (stable semantic API) and the progressive-lowering rule: in bootstrap, ARM can even submit the sky as ordinary `DRAW_SCREEN_TRIANGLES`.
- **Backface culling:** camera is inside the drum; GEOM.CLIP's winding cull would kill it. Fix at generation: FORGE emits inside-facing winding. No cull-mode bit, no hardware change. Document in FORGE.PRIM contract + raster_rules.
- **Fog:** sky material is exempt from world fog (it *is* the far field) or uses its own horizon-band fog authored in the panorama — a material-recipe note, not a new mode.

### C. Texture strategy (with online evidence)

- **Lat-long wrap strip, recommended:** one page, e.g. 1024×256 CLUT8 (+mips ≈ 341 KB incl. chain; RGB565 ≈ 683 KB) + 64×64 cap (8 KB) + optional 256×64 star/alpha layer. u-wrap gives a mathematically seamless 360° join using a mode the TMU already mandates (§15 wrap/clamp/mirror). VRAM impact ~0.7% of the 48 MB texture pool.
- **Cube atlas, rejected:** requires clamp per face and edge-texel duplication/border padding to avoid bilinear bleed across page boundaries — exactly the artifacts the era hit and modern docs still describe: clamp-to-edge addressing ([gamedev.stackexchange](https://gamedev.stackexchange.com/questions/11931/skybox-texture-artifact-on-edge)), clamp wrap-mode and cube-aware mips ([Bugnet](https://bugnet.io/blog/how-to-fix-skybox-seam-at-cubemap-face-edges)), half-texel shrink rationale ([Khronos community](https://community.khronos.org/t/seams-in-cubemap-texture/14882)), Unity wrap-mode guidance ([Unity discussions](https://discussions.unity.com/t/how-to-remove-edges-from-skybox/183707)). We have no border-padding scheme and CLUT8 shares palettes — don't fight it; wrap instead.
- **Mip/grazing:** at 240p the panorama is texel-limited (2.8 tex/° vs 5.1 px/° at horizon), so bilinear+mip suffices; anisotropic is §26-refused. Optional per-material `mip_bias` spec constant, default 0. Zenith handled by the cap page — no 1/sin(θ) mip explosion.
- **Era practice anchoring the drum choice:** Doom's sky is a 256-column cylindrical panorama scrolled by yaw (line-scroll = yaw cylinder — the exact capability ceiling of option 2); Quake/PS1-era cube skies are the alternative we reject on texturing grounds. The drum is the best of both: cylinder texturing + true 3D rotation (pitch via the cap).

### D. Twin Horizons-only sky (option 2) — why it can't carry the job

Per-scanline scroll expresses **yaw only**. Pitch would need a per-line vertical remap (row scaling/repeat) that an affine/line-scrolled plane cannot express; faking it with vertical scroll breaks parallax the moment the camera pitches. Planes also composite through POST.COMPOSITE, so a 2D base sky can't be occluded by terrain except via the world-space depth plane mode — which is §26 cut-order 4. Charter §16's "sky" listing remains honest for **overlay layers**: scrolling cloud sheets, gradient shifts, aurora — alpha-blended above the geometry sky, phase-animated per tick. That is the recommended division of labor (option 3 hybrid).

### E. Duo, Measure, Mirror Gate, determinism

- **Duo:** two rotation-only renders, ≤98,304 fast fragments total (≈10.3% of the example 950k fast-fragment budget) — and it deletes the clear pass it replaces. 45/45/10 fairness is irrelevant (fixed per-view cost), which is itself an argument for the fixed-cost classification.
- **Measure:** sky has no representation ladder and no pixel-error semantics; it should be **exempt from geometry tokens with its own declared budget line** (`sky_triangles`, `sky_fragments` in `costs.zcost` and the presentation contract), still attributed via source ID and fully visible in §25 counters (`triangles_submitted`, `covered_fragments`, `texture_samples`, `vram_bytes_by_client`). This is a spec/cost-model note, not a Measure change.
- **Mirror Gate:** sun, bright horizon rim and emissive clouds write effect tags through the existing tile-store 8-bit tag/strength field using the §15 "glow/distortion writer" recipe; POST.GATHER accumulates at low resolution as designed. Sky is the dominant natural glow source and needs nothing new.
- **Determinism/capture:** scroll offsets, palette phase, star twinkle rate are `DrawSky` parameters derived from tick — byte-identical in replay; texture pages, palettes and hashes already covered by `.zcap` resource sections. Avoid runtime palette uploads entirely (phase-parameter style keeps captures frame-local).

### F. Concrete artifact list (new/changed)

| Artifact | Change |
|---|---|
| `design/blocks.yml` | **No new block.** FORGE.PRIM purpose/notes gain the `sky_drum`/`sky_cap` primitives (bounded subdivision, inside-facing winding, rotation-only transform contract, exempt-from-Measure note). TWOD.PLANE notes gain "sky overlay layer" use. |
| `design/contracts/FORGE.PRIM.md` | Fill later as normal; add sky primitive semantics, winding law, fixed-cost classification. |
| `spec/commands.zidl` | Reserve `DrawSky 0x0310 { handle32 texture_set, mat4fx rot_proj[2], fx16 scroll_u, u8 palette_phase, u8 flags, u8 viewport_mask }`. Alternative: `DRAW_PROCEDURAL` with `kind=sky` — zero new opcode. |
| `spec/raster_rules.md` (Phase 4/5) | Pass-1 backdrop semantics: Z forced far, Z-test disabled, winding note, fog exemption, effect-tag write allowed. |
| texture spec (§15 area) | Lat-long sky page layout note (u-wrap, cap page, optional star alpha layer, optional `mip_bias` material constant). |
| `spec/form/cost_model.md` | Fixed backdrop budget lines (`sky_triangles`, `sky_fragments`). |
| Form language | A `sky { ... }` **present-domain declaration** (layers, scroll rates, palette phase). **No ops.yml entries** — verified sky is not a field program. |

**Cost estimate at 240p:** Z60: 224 tris, ≤92,160 fragments (= the clear it replaces), ~350 KB VRAM (CLUT8 panorama + cap + mips + optional star layer). Duo: 448 tris, ≤98,304 fragments, same VRAM (textures shared between views).

### G. Charter conformance verdict

- **Fits existing law with no amendment:** §8 pass 1 ("backdrop prefill" already says it), §14 (primitive list says "Initial primitives" — additive), §15/§16 (TMU wrap + plane overlay uses as listed), §6A/§19.3 ("include" permits semantic-command additions, or fold into DRAW_PROCEDURAL), §26 (no refusals touched — no anisotropic, no second TMU, no shaders), §30 (sky strengthens the "one excellent conventional path").
- **Would force an amendment only if:** someone insisted on hardware seam filtering for a cube atlas, a cull-mode flip bit, or a dedicated FORGE.SKY block — all three are unnecessary under this design and recommended against.

**Sources:** [gamedev.stackexchange — skybox edge artifact](https://gamedev.stackexchange.com/questions/11931/skybox-texture-artifact-on-edge) · [Bugnet — fixing cubemap face-edge seams](https://bugnet.io/blog/how-to-fix-skybox-seam-at-cubemap-face-edges) · [Khronos community — cubemap seams](https://community.khronos.org/t/seams-in-cubemap-texture/14882) · [Unity discussions — skybox edges](https://discussions.unity.com/t/how-to-remove-edges-from-skybox/183707) · [Unity discussions — custom skybox seams](https://discussions.unity.com/t/custom-skybox-seams/912) · [Unreal forum — negative LOD bias](https://forums.unrealengine.com/t/texture-mipmap-setting-negative-lod-bias/387731) · [OverTake — mip map bias](https://www.overtake.gg/threads/blurry-textures.10602/) · [guru3d — LOD bias](https://forums.guru3d.com/threads/the-great-lod-bias-mystery-amd-users-must-know.398019/) · [Reddit — mip bias explained](https://www.reddit.com/r/assettocorsa/comments/1fkvtnm/what_exactly_is_mip_bias/)
