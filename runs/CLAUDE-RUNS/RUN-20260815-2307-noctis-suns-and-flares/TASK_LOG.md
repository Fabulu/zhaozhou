# Task Log: RUN-20260815-2307 - Noctis star gamut and lens flares

**Created:** 2026-08-15 23:07 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260815-2307-noctis-suns-and-flares/

---

## Objective

Implement the ratified ADDENDUM-star-gamut-and-flares.md in the reference
model (Phase 3+ scope: ARM/asset side + ZRef preview): the 12-class star
gamut, CLUT ramp discipline, starface/corona bakes, the lens-flare ghost
chain as a bounded splat POST mode, the procedural starfield (harness-
equivalent sector hash), the space case, hand-computable anchor tests, and
reel subjects for the gallery. No RTL (charter §21); no host floats in
deterministic paths (§29-7).

---

## Progress Timeline

### 2026-08-15 23:07 UTC+02:00 - Recon phase (previous agents)

- Three Noctis IV recons persisted: FINDINGS-N1/N2/N3
- ADDENDUM-star-gamut-and-flares.md ratified from them

### 2026-08-16 - Implementation started (this agent)

- Read the addendum, sky_and_beams.md, qformats.md, the renderer, the reel
  machinery, RENDER-POLICY.md, and the noctis harness oracle.
- Verified the §13 anchors by hand BEFORE implementing. Finding: the D3 ramp
  anchor `S00 ramp[32] = (254,224,136)` is only reproducible when the ramp
  control points P1/P2 use the plain ×4 expansion into the s16 pre-clamp
  domain (255→(252,232,160) etc.), NOT the D2 display expansion
  `c8=(c6<<2)|(c6>>4)`. The anchor disambiguates the addendum's two
  expansion statements: ×4 in the ramp domain (consistent with P3 = "64,70,76
  ×4"), bit-replication only where a class colour is used directly as a
  display colour. Implemented that way.

### 2026-08-16 - Sky continuity fix (owner priority, coordinator directive)

- Diagnosed the "ovals, not sky" report: (1) spec gap - no C0 continuity
  across sky layer joins; (2) deeper: the Phase-3 sky projected with w=1
  (linear), painting BOTH drum sides; (3) the 16-gon cap fan inside the
  48-gon drum leaked background slivers. All three fixed and recorded as
  sky_and_beams.md v1.1 S1.2 amendments. Enforced by render_sky
  test_seam_continuity (conforming 25 vs violating 124 - fires) and the
  bit-exact cap-rim assertion. Under-plane subdivided 8x8 for near-plane
  rejection. Reel re-authored: C0 dusk elevation ramp, camera-consistent
  sky rotation, new sky-sweep subject (horizon->zenith ping-pong). All
  five reel CRCs re-pinned deliberately; render_golden re-pinned with
  history. Commit e5eff3e, pushed.
- Copy gate: reworded the wave reel note ("seamlessly" banned); audited
  all other notes against copycheck-banned.txt.

### 2026-08-16 - Star gamut module (commit acfb9e2, pushed)

- spec/stars_and_flares.md landed in-repo (verbatim addendum + two
  implementation clarifications the anchors forced):
  1. ramp control points are x4 expansion (the ramp[32] anchor proves it);
  2. flare bake lines draw 255 - the literal "value 32" bakes a 4/255
     invisible flare (the 512^2 canvas is 8x supersampling); 32/255 is the
     DOWNSAMPLED arm brightness (measured exactly 32 after the fix).
- zref::star / zref::flare / zref::post / zref::sky::starfield implemented;
  render_star: 16 groups, all S13 anchors hand-computed first, gamut sheet
  CRC pinned 87A069ED. Starfield oracle transliteration byte-exact vs the
  imported three-way-verified goldens; mutation (unsigned fold) observed
  RED then restored.

### 2026-08-16 - Celestial reel subjects (commit bb1d8bb, pushed)

- SoftwareRenderer pre-resolve hook ([phase3-preview], default off); four
  reel subjects: star-boil (63f, one full CLUT revolution per loop),
  noctis-flare (burst + ghost chain sweep), pulsar (duty strobe,
  halo_airless), flare-occlusion (distant sun, streak rung, probe fade
  behind the island). Palette-law authoring lessons recorded in the tool
  (white glints saturate for free; a near sun's burst over terrain lambert
  is unpublishable under the 256-colour law; the far streak is both the
  signature and the one that fits).

### 2026-08-16 - DEFECT FOUND + FIXED: resolve white rail (commit 9f63d1d)

- The star compositor is the first producer of saturated pure white and it
  exposed a latent ordered-dither overflow in resolve_rgb565: green's
  dither amplitude is double red/blue's with half the headroom, so g in
  [252,255] at Bayer >= 8 computed g6 = 64 -> WRAPPED to 0 (white became
  a white/magenta 0xFFFF/0xF81F checkerboard on odd parity). The header
  comment had proven the 5-bit channels safe and assumed green matched.
  Clamped at the rails; pinned by render_directed test_resolve_white_rail
  (red on 8 of 16 phases before the fix). render_golden + the four star
  subjects re-pinned; the five terrain/sky subjects held (nothing reaches
  252 green).

### 2026-08-16 - The suite caught MY test red (af555b3)

- The full fast suite failed render_directed: my new white-rail test's
  floor was wrong (g=250 at Bayer 0 resolves to 61, I demanded >= 62), and
  worse, my pre-commit "green" was the exit code of a tail PIPE, not the
  test binary. Fixed the bound to the hand floor (61; the wrap defect
  yields 0, sensitivity intact) and re-verified every touched binary with
  its REAL exit code. The suite doing its job on the tester is recorded
  here on purpose.

### 2026-08-16 - Ledger notes (db91b93) + site

- blocks.yml notes amended per D10 (8 blocks); ledger:check green.
- Site: reel.ps1 + update.ps1 source lists, zshot stills re-authored (C0
  dusk ramp, goldens re-pinned twice, legacy sun quad replaced by the
  compositor sun in sky-dusk and island-terrain), togif STILL_FRAME for the
  5 new subjects, template gallery entries (sky-sweep, star-boil,
  noctis-flare, pulsar, flare-occlusion) + the white-to-magenta bug story,
  assemble.py REEL map + goldens. All nine GIFs encoded palette-exact and
  decode-VERIFIED byte-exact; copycheck clean; deployed to
  zhaozhou.pages.dev.

### 2026-08-17 - Moving-star smear correction, amendment v1.2

- Re-read the authoritative Noctis paths before editing. `pfade` strips the
  palette bank and subtracts 8 with saturation. `psmooth_64` writes the
  four-sample average from rows y+1 and y+2, so each of the two passes moves
  energy up and left. The frame draws the fresh star after both passes.
- Replaced eight flat, bright corona copies with one bounded six-bit replay
  from the unchanged `TrailHistory`. Each moving historical entry stamps a
  graded corona plus a scaled face when present, then receives one decay and
  two exact smoothing passes per age. The combined plane gets one class-ramp
  lookup. The current head remains sharp.
- Preserved static skip, eight-entry overwrite eviction, 236 B capture ABI,
  fixed integer arithmetic, strict depth, halo skip, and byte-exact replay.
- Replaced flat-center tests with subtract-8, exact asymmetric kernel, lit
  continuity, graded falloff, newest-to-oldest decay, static skip, eviction,
  replay, and ramp-ceiling checks.
- `noctis-flare` initially measured 268 colors. Pairing adjacent entries in
  that reel's authored ramp retained the exact intensity reconstruction and
  brought the complete sequence to 252 colors. The other affected reels
  measured 128 to 232 colors.
- Visually inspected frames 8, 16, 24, and 31 for `noctis-flare`,
  `blue-giant`, `white-dwarf`, `orange-giant`, `blue-dwarf`, `multiple`, and
  `infant`. The tails are connected, asymmetric, graded, and class-colored.
  The Noctis subject now reads as a soft flame-like smear instead of white
  circular beads.
- Remaining fidelity gap: history stores positions only. Reconstruction uses
  the current size, face, class, distance washout, and depth. It does not
  retain historical full-scene pixels, flares, terrain, or sky.
- Focused validation passed: `render_star`, `reel_sequence_crc`, catalogue
  parsing, two complete reel generations, palette-exact GIF decode against
  every source frame, and byte-identical second-run GIF hashes for all seven
  affected subjects.
- Full fast suite result: 80 passed, 6 failed, 1 skipped. The failures are in
  the pre-existing dirty ABI, QFMT, DMA, and shell-golden lanes: `stub_top`,
  `fixp`, `tables_tri`, `cmd_dma_directed`, `cmd_dma_random`, and
  `shell_golden_replay`. `format_check` skipped. No star or reel test failed.
- Coordinator verification reran `render_star` and `reel_sequence_crc` through
  the project WinLibs toolchain: 2 passed, 0 failed, 0 skipped. A 28-frame
  contact sheet independently confirmed the white bead chain was replaced by
  connected, graded, class-colored trails.
- The correction was isolated from all pre-existing staged and unstaged work
  in commit `273142d` (`stars: reconstruct Noctis indexed motion smear`) and
  pushed to `origin/main`. The unrelated capture-container editorial hunk in
  `spec/stars_and_flares.md` remains unstaged and unchanged.
- The hard-gated site path passed assembly and copycheck, uploaded 22 changed
  files, and deployed the corrected canonical GIFs to
  `https://08845a29.zhaozhou.pages.dev` and the `zhaozhou.pages.dev` project.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

Repo (commits e5eff3e, acfb9e2, bb1d8bb, 9f63d1d, db91b93 on main):
- spec/stars_and_flares.md (verbatim ratified addendum + 2 clarifications)
- reference/include/zref/zref_star.hpp
- reference/src/zsky/star_gamut.cpp / star_bake.cpp / star_flare.cpp /
  star_field.cpp / star_compose.cpp
- tests/render/render_star.cpp; tests/golden/starfield/{oracle.bin,README.md}
- amended: spec/sky_and_beams.md (S1.2), emit_layers.cpp, render_frame.cpp,
  zref_render.hpp (pre-resolve hook), resolve.cpp (white rail),
  render_sky/directed/golden tests, tools/reel/zhao_reel.cpp,
  reference/CMakeLists.txt, tests/CMakeLists.txt, design/blocks.yml

Site (zhaozhou-site, not a git repo): zshot.cpp, reel.ps1, update.ps1,
togif.py, assemble.py, template/index.html; deployed to zhaozhou.pages.dev.

---

## Decisions Made

1. Ramp control points use x4 expansion (the S13 anchor proves it); the D2
   c8 bit-replication is display-only. Recorded in the spec header.
2. Flare bake lines draw 255; "32/255" is the DOWNSAMPLED arm brightness
   (the literal reading bakes a 4/255 invisible flare). Recorded.
3. Rarity gate runs in the SECTOR-INDEX domain (the unit domain would gate
   away every star beyond ~1 sector of the origin); test-pinned.
4. Flare texel budget drops over-budget splats WHOLE (deterministic; the
   far sun loses its big ghost first).
5. The celestial preview rides a renderer pre-resolve hook until the
   reserved SetCelestials 0x0320 lands in the zidl (recorded deviation).
6. Sky S1.2: C0 elevation-ramp continuity + perspective sky projection +
   48-segment yawed cap fan, all amended into sky_and_beams.md v1.1.
7. Pulsar duty gates splats INSTANTLY (a strobe must not be smeared by the
   fade counter; the fade tracks visibility only).

---

## Next Steps

- Register the celestial_state chunk in the .zcap container (serialize /
  deserialize + roundtrip test exist; the container kind allocation belongs
  to the capture-format lane).
- SetCelestials 0x0320 into spec/commands.zidl + abi-gen when the command
  surface lane opens; retire the preview hook then.
- Mip chains for starface/corona (asset-pack, W3.6 lineage); dfs into the
  surface-sun grading when that sky path is built.
- POST.ECHO (cut-order 1) would restore the fade-not-clear accumulation
  that made Noctis's far streak bright; the bounded streak is uniformly
  32/255 by construction and is honest about it.
