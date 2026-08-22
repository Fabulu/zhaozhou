# CONSOLIDATED ADVERSARIAL REVIEW (Fable) — main @ 8236cee

*Full-project review commissioned 2026-08-15 before further building. Verdict: **ACCEPTED-WITH-FIXES**. Everything verified against actual files, by hand-computation, or by compiled probes. Full suite runs GREEN at HEAD — which sharpens the headline: **every bug below is invisible to the current tests.***

## A. Previous review's fixes (RUN-20260814-1912/REVIEW.md) — ALL REAL

- **C1 (SPLINE 2^16)** fixed in both interpreters (`zfield_interpret.cpp:267`, `interpret.ts:181-182`). Hand anchors re-derived independently: uniform 5-knot 0,0,1,1,1 → a=1.5 gives 32768, a=2.75 gives 67072 (CR tail overshoot); match committed assertions. Genuine independent anchors, not oracle echoes.
- **Corpus regeneration real**: b05f6f7 regenerated exactly the 4 SPLINE-hitting .zvec (seeds 238/31255/48879/61453); parity replays 8 programs × 31 records bit-identically. Corpus parity is no longer two-implementations-sharing-a-bug.
- **C2 (unit8 wrap)** clamp at `zref_fixp.hpp:257-258`; boundary tests `test_fixp.cpp:648-657` (0xFF7F/0xFF80/0xFFFF/INT32_MAX).
- **M1** .clang-format + format job (clang-format LLVM 15) + cppcheck in ci.yml:137-175; formal smoke in fast (smoke is only `sby --version` — disclosed).
- **m1-m4** all real and verified.

## B. Numeric/semantic exactness sweep

**Verified clean by hand** (expected values computed independently): fx_add/sub/mul/mad single-rounding; rescale round-half-up incl. negative-tie and k≥32; TS 16-bit-limb mulS32/acc96 sign-correction algebra; DOT2/3 s128-vs-96-bit equivalence incl. |v|≥2^53 shortcut; field_rcp NR scaling, INT32_MIN, ±saturation; rcp_u24_norm chain; isqrt (two algorithms, provably identical); sin table T[1]=402, T[128]=46341 ✓; quadrant mirroring; NOISE2/RIDGE hash+lane laws; RING midpoint rescale; CURVE mad form; smoothstep t²(3−2t); TS literal conversion BigInt-exact.

### CRITICAL-1 — Sky drum collapses to a single angle; the 360° skybox does not exist
`reference/src/zsky/emit_layers.cpp:89-90`: `ga_l = drum_yaw.raw + (a_l.raw >> 16)`. `a_l` is an fx16 turn fraction in [0,65536) so `>>16` is 0 for every column (1 for the last edge). Correct conversion is the raw value itself (the zenith cap does this correctly at :114). Compiled probe against real sources: all 1536 band triangles share **9 distinct X coordinates** → zero-area → `raster_tri` rejects at `rast.cpp:77` → **bands render nothing**. Only fallback background, cap, under-plane, clouds appear. No test asserts band geometry/pixels (`render_sky.cpp` checks census/UV/scroll only; the fallback test :243-245 checks cap+under visibility but not bands). Golden CRCs bake the bug.
**Fix:** `angle16{(uint16_t)(drum_yaw.raw + a_l.raw)}`; add band-visibility/geometry test; regen goldens.

### CRITICAL-2 — Sun centre alpha wraps 256→0; sun invisible for any energy ≥ ⅓ (default 1.0)
`emit_layers.cpp:203-204`: `ca = (uint8_t)((a_raw + 128) >> 8)` with a_raw clamped to 0x10000 → 256 → 0. Empirical: energy 65536/32768/21846/21845 → alpha 0; only <0.332 survives. Rim vertices 0 by design, so additive blend contributes nothing. **Exact bug class of review C2, re-introduced by hand-rolling the conversion** instead of calling the just-fixed `unit8_from_fx16` — simultaneously a charter no-second-implementation violation. No sun-visibility test exists.
**Fix:** `unit8_from_fx16(fx_clamp(e3,0,1<<16), L).raw`; add test; regen goldens.

### MAJOR-1 — `fx_sin` reads one past the end of the 257-entry table
`zref_trig.hpp:34` reads `SIN_Q16[i+1]` with i=256 whenever a13=0 in an odd quadrant — i.e. `fx_sin(0x4000)`, `fx_sin(0xC000)`, **every `fx_cos(0)`/`fx_cos(0x8000)`**. Compiler-proven: constexpr eval hard-errors `array subscript 257 outside bounds [257]`. `numeric.ts:66` mirrors it (undefined→NaN→0). Value-correct today only because t=0 zeroes the garbage; exhaustive golden test cannot see it; spec §7.1's formula requires T[257] (spec defect too). UBSan/constexpr/RTL-X-prop hazard.
**Fix:** guard i==256 (return T[256]) both languages + spec note + boundary regression test.

### MAJOR-2 — qformats §8 fill rule drops shared-edge pixels on BOTH sides (spec defect, faithfully implemented)
`spec/qformats.md:352` pins `inside ⟺ E'(p) + bias > 0` (bias 0 TL / −1 else); `rast.cpp:146` implements it exactly. D3D's rule is ≥: with strict >, a pixel center exactly on a shared edge (E0=0) fails the TL side (0>0) *and* the non-TL side (−1>0) — hand-traced on a 10px axis-aligned quad split: every diagonal pixel center is a hole; non-TL edges additionally drop the strictly-interior E'=1 rank. Contradicts the same sentence's "D3D top-left fill convention" and the exactly-once formal property planned for Phase 4/5.
**Fix:** spec amendment to `≥ 0`, `>=` in rast.cpp, golden regen.

### MAJOR-3 — Duo canvas layout: renderer and W2.2 RTL implement two incompatible readings
Renderer: single 512×240 row-major image, views side-by-side (`internal.hpp:31-40`), displayed CRC rows 0..191 of 512-wide (`resolve.cpp:66-74`). W2.2 branch + video_rules §3.1: view0 = slot bytes [0,0x18000), view1 = [0x18000,0x30000). **The spec is self-contradictory** (§1 mode table: Duo canvas 0x3C000; §3.1: packed 0x30000). Same .zcap FRAMEBUFFER_EXPECTED cannot match both; merging W2.2 as-is scrambles Duo frames.
**Fix:** ratify one layout (packed §3.1 is what RTL and future .zcap goldens assume), rewrite renderer Duo path + displayed_crc32c, regen goldens. **Before merging wp/w2.2-video.**

### MAJOR-4 — Pinned pass-6 sub-order violated: cloud rasters before sun
sky_and_beams §1 pins "sun before cloud"; `render_frame.cpp:340` rasters in emission order (cloud first); the file header (:19) claims "sun BEFORE cloud". Different pixels once CRITICAL-2 is fixed.

### MAJOR-5 — Marker world-space sizing inverted under perspective
`sprites.cpp:105`: `fx_div_exact(size, c.s.d)` with d=1/w computes size·w — markers **grow** with distance. Should be `fx_mul(size, d)`. Exact only for ortho demo matrices (w=1), so every test passes; W3.7 would bake it into goldens.

## C. Dimensions 3–5

### CRITICAL-3 — MEM.GUARD "Formally proven." is false
`tests/formal/formal_mem_guard.sv:84` **fails when actually run** (no init/reset discipline — the free-init trap W2.3's own TASK_LOG documented); headline A1/A2 no-escape assertions are **vacuous** in every buildable configuration (`arb_req.valid` unreachable in the elaborated model; prime suspect: LRM-illegal mixed continuous/procedural driving of `rsp` in `zhao_mem_guard.sv:111/119`); zero cover statements; a tightened-bound mutation still passes. blocks.yml MEM.GUARD purpose says "Formally proven."
Supporting MAJORs: the SKIP condition (`mem_formal_lane.cmake.in:41-51`) matches *any* "syntax error" — masks everything forever; the lane was **never elaborated once** although `read_slang` works on the same frozen packages (the input lane proves it; all three MEM harnesses elaborate cleanly with it). W2.3 also overclaims: "frozen while absent" is not among committed properties (mutation passes formal; sim covers it); "gaps==0 forever" rests on depth-30 BMC with no induction. W2.4's mutation verification is genuine (though manual/unscripted). VRAM arbiter proof, run today via read_slang, passes non-vacuously.

**Semantics-implemented-twice grep**: CRC single-source ✓; field ops exactly twice (sanctioned) ✓; terrain calls the one `zfield::interpret` ✓. One violation: the hand-rolled fx16→u8 in CRITICAL-2.

**ABI v2/D7 reinterpretations verified byte-exact**: SurfaceStamp pad[12]→radius/ring_width/pad[4] and DrawProcedural pad[12]→kind/pad[11] reuse mandatory-zero pads at unchanged offsets; v2-era records stay valid; enum range validated; zcap_minimal replays green; PadFrame 20B consistent.

### MINORs (Fable's, adding to the fork recaps')
1. `DrawSky.reserved0/reserved1` are named fields not pads — `ZHAO_PADS_DRAW_SKY` covers only 146..159, so never zero-validated.
2. Shell executor vs renderer disagree on reserved DrawSky (UNIMPLEMENTED_COMMAND vs executes) — W3.6 capture replay needs one ratified reading.
3. `DrawSky.cloud_scroll_u/v` dead on the wire — renderer derives scroll from tick, ignores payload.
4. Cloud r² uses truncating shifts/division instead of the frozen rescale primitive; r² law pinned nowhere in spec.
5. Cap UV comment vs code mismatch (`lerp_fx(cos,0,1)` = cos ∈ [−1,1]).
6. `grid_lerp` claims round-half-up but truncating division breaks it for negative spans.
7. SPLINE table `dy` signedness: spec implies unsigned, both interpreters multiply signed i32.
8. commands.zidl:40-42 justifies version-stays-2 by "every committed wave-2 capture" — `captures/golden/wave2/` **does not exist**. Decision fine; stated rationale false.
9. Blend/`kGoldenFieldOffCanvasCrc` dead slot; zero covers.

### From the fork recaps (also part of this review)
- **MAJOR (maturity)**: MEM.HPS.BRIDGE at RTL_VERIFIED with **no randomized differential** — `tests.random` aliases the directed file; cited `mem_random.cpp` doesn't instantiate the bridge. Charter §21 step 5 skipped; V4 satisfied only by the alias.
- MINOR: guard/bridge maturity_log evidence copy-pasted from the arbiter (V3 checks path existence only); INPUT commit pins point at ledger-bookkeeping commits; `blocked_on_hardware.md` "no Quartus" stale after 5189f0c; W2.4 "mutation-verified" has no committed re-runnable artifact.
- MINOR (SV): `zhao_hps_bridge.sv:109` err fires on `req.valid && busy` while `req_grant` is a registered pulse — a client holding valid until grant gets a spurious err; de-facto pulse discipline never stated in memory_rules §3.
- VERIFIED CLEAN: all 23 .sv files SV-subset compliant (zero always@/initial/#delay/classes/queues/latches); §25 counter catalog complete with matching IDs + D9 shadows; **zero zhao_pkg drift** across all three unmerged branches; determinism sweep clean (no floats/time/rand in deterministic paths); 31 lint waivers all justified.

## VERDICT: ACCEPTED-WITH-FIXES

Wave-1/2 numeric core, ABI discipline, determinism, SV subset, counters, ledger mechanics and the previous review's remediations are sound — verified clean, negative space trustworthy. The newest work shipped two invisible critical semantic bugs and one false formal claim, all three protected from detection by self-generated goldens or never-run proofs.

**Required before further building on these paths (priority order):**
1. CRITICAL-1 drum angle fix + band-geometry test + golden regen.
2. CRITICAL-2 sun alpha via `unit8_from_fx16` + sun-visibility test (fold MAJOR-4 sun/cloud order into the same golden regen).
3. CRITICAL-3: strike "Formally proven" from MEM.GUARD; rewrite guard harness (init discipline + covers); fix the mixed-driver struct; switch MEM lane to `read_slang`; tighten SKIP to the pinned signature.
4. MAJOR-3 Duo layout ratification **before** merging wp/w2.2-video.
5. MAJOR-2 fill-rule spec amendment (`≥`) + MAJOR-1 sin boundary guard — spec-level, must land before any RTL freeze copies them.
6. MAJOR-5 marker sizing; HPS-bridge randomized differential before its RTL_VERIFIED claim stands.

Strongly recommended: the nine MINORs plus the recaps' minors (req.valid pulse documentation; ledger evidence-pin hygiene).
