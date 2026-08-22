# Task Log: RUN-20260815-0544 - [Describe objective here]

**Created:** 2026-08-15 05:44 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260815-0544-wave3-phase3-software-console/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-15 05:44 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260815-0544
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

## W3.1 — Spec wave, ABI v3 content, format freeze — COMPLETE (2026-08-15, implementer agent)

### Commits (main, pushed)

1. `04a6401` spec(w3): Form L1 v1 specs, .zpak cartridge, capture format w3 extensions — spec/form/{language-semantics,domains-and-effects,deterministic-scheduling,cost-model}.md expanded from stubs (Phase-1 content retained, banner-append); spec/cartridge.md new (D8); spec/capture_format.md w3 extensions.
2. `bc43f75` abi(w3): D7 Phase-3 command promotions + same-bytes reinterpretations, regen — commands.zidl six promotions reserved→implemented; forge_kind enum + SurfaceStamp radius/ring_width from pad bytes; all abi-gen outputs regenerated in-commit; tests/unit/test_zref_shell.cpp reserved-command vehicle moved DrawForm→DrawSky.
3. `73baf99` ledger(w3): four contracts filled (all headings, zero TODOs) + blocks.yml notes only (no maturity moves); ledger:gen byte-identical.

### Decisions

- **ABI version stays 2** (task hint confirmed against the frozen rule): promotions are execution-semantics only; the two payload reinterpretations (DrawProcedural `forge_kind` u8 @ payload 36; SurfaceStamp `radius`/`ring_width` fx16 @ 36-43) reuse never-executed mandatory-zero pad bytes at unchanged offsets/sizes — the v2 `u8 mode`→`video_mode` precedent. Member 0 = heightfield_patch keeps every v2-era record valid. A bump would have invalidated all committed wave-2 captures (their frame headers pin abi_version 2). Documented in the .zidl header + capture_format §1.2 [w3].
- **Source-ID kinds appended as 8-11, not plan D11's literal 5-8** (DEVIATION RECORDED): kinds 5-7 (command site / audio event / stamp operation) were frozen in Phase 1 and appear in committed goldens (`zcap_minimal.zcap` carries kind-5 command-site entries). New kinds: 8=system, 9=presentation emit site, 10=pool, 11=scenario; deviation note in capture_format §5.
- L1 grammar adds `global` (not in D1's IN list) as the necessary consequence of FORM §6 "explicit persistent state" + D4 FormState (SoA pools + globals + RNG states) + D5 hash serialization; justified in the spec text.
- `population` declarations refused (FORM-E-714): pool + @flow field + draw_population emit is the L1 surface (keeps D1 exact while W3.7's shatter_storm remains expressible).
- Field dialect surface = exactly the frozen FieldBuilder method set (field-ir.md §2 opcode table + SMOOTHSTEP macro); division, bitwise ops, calls refused (FORM-E-665). field-ir.md untouched.

### Verification (all green)

- `npm run abi:gen` + `abi:check` clean (25 outputs)
- `npm run -w tools/abi-gen test` 20/20; full `npm test` 79/79 (compiler 32, ledger 13, fixgen 14, abi-gen 20)
- `npm run ledger:check` green (87 blocks, V1-V14 + staleness); `ledger:gen` byte-identical
- `ctest -L fast` 31/31 (one wave-2 shell test updated: its reserved-command vehicle DrawForm was promoted, moved to DrawSky 0x0310 — the only test affected by the promotions)
- Zero TODOs in the four contracts and five specs; 147 distinct FORM-E codes, one per rule (W3.2 must test every code)

### Frozen grammar summary for W3.2 (what to implement)

Lexical: UTF-8, CRLF-tolerant byte-offset spans, keywords §1.1, literals target-typed (i32/u32, fx16 `1.5`/`m`/`px`, fx24 `w`, angle16 `turn`/`deg`, unit8 `%`, u32 `120t`, colour8 `#RRGGBB`, strings only in 4 positions). Declarations: module/import/const/enum/struct/pool[cap]/global/system(every N ticks [stagger over p] reads..writes..)/fn/@earth|@flow field(params: P)->record footprint.. max_ops N/presentation(view+emit)/scenario/sound. Statements: single-assignment let, assign (declared writes only), if-stmt, ascending bounded for (a..b or pool sugar), spawn/kill (stable compaction at system end), apply terrain_field. Expressions: Pratt table §2.2; if = select-expression (both arms evaluated; no short-circuit anywhere); mixed-precision and world3/velocity3 mixing refused. Field dialect: branchless/loopless/no-calls, fx16-only lanes, in-scope = profile input record + params struct (≤8 fx16 earth / ≤4 flow), body forms = exactly the builder op table, instr_count ≤ max_ops ≤ ceiling (32/48). Diagnostics: {file, span, "FORM-E-nnn", message}, collected never thrown, non-zero exit, no partial emission. Full law: spec/form/language-semantics.md.

### Notes for downstream W3.x

- Emit vocabulary → ABI mapping table: language-semantics §5 / domains-and-effects §4 (DrawForm flags b0 billboard/b1 screen-size; DrawPopulation flags b0 point/b1 triangle — payload-flag bits documented in .zidl comments).
- costs.zcost schema: cost-model §2 (canonical JSON, sorted keys, no whitespace, LF; unknown members must be ignored by readers).
- sourceids.zmap binary: capture_format §7 (magic ZSMP, 24-B entries, span byte offsets = the SourceSpan the builder consumes).
- .zpak: cartridge.md (zcap discipline verbatim; RESOURCE_PAGES kinds 0-5; packer determinism + pack:check gate for W3.6).

## 2026-08-15 — W3.2 Form frontend (branch wp/w3.2-frontend, commits 8b4872b + 06fbe87 + 1b4230a)

**Scope delivered (plan §2 W3.2):** `compiler/src/frontend/` — span.ts (byte-offset SourceSpan + field-ir line/col bridge), diagnostics.ts (FORM-E catalog mirroring §7 + spec-table extractor), lexer.ts (UTF-8 gate, CRLF-tolerant byte spans, nesting comments, all target-typed literal forms, §1.1 keywords incl. domain keywords), parser.ts (recursive-descent + Pratt per §2 EBNF, collected-never-thrown diagnostics, statement recovery, OUT-list refusals in place), ast.ts (every node carries span; canonical byte-stable serializer), checker.ts (domains as function kinds, present-purity, reads/writes admission, field-dialect admission onto the frozen builder surface with ceilings 32/48 + register budget, one-writer-per-component-per-phase schedule with whole-pool expansion = the D6 analysis feeding W3.3, call-graph recursion, const-eval, qformats exactness), index.ts (compileFrontend; no partial emission on any error).

**Corpora:** positive = 7 modules compiled as one cartridge (every D1 IN feature: declarations, systems+effects+stagger+multi-rate+apply, both profiles with the full §6.3 op surface, all five emit kinds, scenario, imports); negative = 148 manifest cases, one per frontend-raisable code, first-diagnostic exact-code match; AST goldens committed (7 × .ast.json, byte-stable run-twice). `npm run -w compiler test` 215/215 (183 new, 32 existing keep passing); full `npm test` 262/262 across workspaces. Goldens + corpus pinned `text eol=lf` in .gitattributes (byte-offset spans are checkout-sensitive).

**Catalog finding:** spec §7 defines **136** codes, not 147 (W3.1's log entry overcounted). `catalog.test.ts` pins exact set equality both directions + the 136 count. Documented non-frontend exemptions (spec-issue notes in the W3.2 report): E-668 (no L1 table grammar), E-821/822 (runtime aborts), E-830/831 (pack-time).

**Resolution highlights (full list in the W3.2 report):** vector record literals `world3 { x = 0w, ... }` fill the missing §1.2 vector-literal form; flow application = call statement `prog(pool, params)` (§6.2 prose basis); `stagger N over p` optional explicit rate makes E-507 reachable; E-407 fires inside scheduling cycles only (plain read-modify-write is the F(stateₜ) law); catalog wins over inline §4.2/§4.3/§4.4 code citations (E-213/214→E-301/801, E-810/811→E-800/801, E-401/402→E-302/303, continue→E-712).

**For W3.3:** `checkModules()` returns `Schedule{phases:[{index, systems:[{name, module, every, span}]}]}` — declaration order within phase, readers after writers, whole-pool accesses expanded to field + `#members` components. AST shape summary in the W3.2 report; spans bridge via `lineColAt()`.

## 2026-08-15 — W3.5 ZRef renderer + sky + audio-event path (branch wp/w3.5-renderer, commits 993d8a9 + 38c466d)

**Scope delivered (plan §2 W3.5 / D7):** `zref::render` — the Phase-3 software console over a validated sealed frame packet + 2-slot RGB565 canvas. `reference/src/zrender/` (render_frame = fail-safe-validate-then-walk + charter §8 pass order per view; rast = qformats §8 edge law; terrain = heightfield + TerrainField via the ONE zfield interpreter + sheet tint; sprites = wall-clamped 8×8 markers + point/triangle particles; resolve = 4×4 Bayer dither + video_rules §3/§4 CRCs) and `reference/src/zsky/emit_layers.cpp` (the spec's named preview functions). Public headers `reference/include/zref/zref_render.hpp` + `zref_sky.hpp`. EmitAudioEvent → `RenderResult.audio_events` (ZEmu feeds `zref::MixerTone`; `tone_id_for()` maps sample 0/1/2 → A4/A5/C4 per audio_rules §4). 36/36 ctest `-L fast` green (31 existing + 5 new), run-twice identical; cppcheck warning tier + pinned clang-format clean over the tracked set; no floats in the render path (grep-audited by test).

**Golden canvas CRCs (committed in tests/render/render_golden.cpp):** synthetic 2-view Duo frame (sky all layers + hand-built earth .zprog TerrainField + ring SurfaceStamp + 17×17 island + 2 wizards + population + 2 audio events): canvas **0x15AB7655**, displayed **0x72D938D5**. Budget: 600-frame Duo island sequence **16.3 s** (27 ms/frame) < 60 s law.

**PAINTER/DEPTH LAW (frozen in zrender/internal.hpp):** terrain cells rasterize depth_test OFF / depth_write ON — a constant-w (ortho) view-projection makes Q16.16 1/z identical for every triangle, so the strict-greater §8 test would reject every later cell against the first writer; the painter sort IS the terrain ordering, and depth stays written so sky pass-6/markers/particles test against terrain exactly per §8. Under perspective the two agree (no-op refinement).

**[w3.5-software] readings W3.6/W3.7 must know (full list in each file header):**
- Sky layer depths are PINNED per layer-table row (under 25, cloud 36, sun 26, bands/cap write far 0) because rotation-only rot_proj keeps w = 1 — no camera-relative 1/z exists; sun gets the +1 tie-break so the additive sun survives the under-plane's depth write (both at layer z 2560).
- TerrainField applies at the DrawProcedural draws (the field needs the patch transform); age = tick − start saturated to duration, phase = round(age/duration); velocity recorded once per frame (first view).
- SurfaceStamp: op 0 = replace/max, op 1 = decay-accumulate (halve + add, saturate u8); sheet tint = rgb·(255 − strength/2)/256, ~50% max darkening; sheets persistent per PATCH HANDLE.
- Marker law: flags b1 = the fx16 size lane IS the pixel half-extent (screen-space); world-space uses the perspective divide at projection scale 1; wall-clamp = quad centre clamps to [half, extent−half].
- DrawSky 0x0310 still `reserved` in the .zidl (RTL wave 8) but the software console EXECUTES it per sky_and_beams §6; `cloud_scroll_u/v` payload fields are recorded-unused — emission derives scroll from the tick.
- NDC +Y maps to +Y canvas row (matrices are authored y-down); a SetView-less view renders nothing; a plane/patch vertex with w ≤ 0 culls the whole primitive (Phase-3 near-plane law; the clipper is Phase 4/5).
- `RenderResources` is the host-fill table set (field programs, terrain patches, materials, forms/transforms, populations, sky sets, tones) keyed by FULL handle value; misses count in `resource_misses` and skip deterministically.
- No fixgen dither table exists; the 4×4 Bayer is defined once in resolve.cpp (single regeneration point for the Phase-4 RTL resolve freeze).

## 2026-08-15 — Adversarial-review remediation (FIXER, branch main, commits ae3bdf2 → dc3eac0)

**Scope:** the 3 CRITICAL + 5 MAJOR findings of `REVIEW-FABLE-ALL-WAVES.md` (MAJOR-3 Duo layout excepted — it needs orchestrator ratification and is untouched), plus three clean-rebuild failures the orchestrator relayed. Every fix was verified by *reintroducing the bug* and confirming the new test catches it.

**CRITICAL-1 — the 360° skybox did not exist** (ae3bdf2). `emit_layers.cpp` computed the drum column angle as `drum_yaw + (a.raw >> 16)`; `a` is an fx16 turn fraction in [0,65536), so the shift is always 0 and all 48 columns of both bands shared one angle. Every one of the 1536 band triangles was a vertical line, rejected by `raster_tri`'s zero-area check. The fx16 turn fraction *is* the angle16 container (qformats §2) — the zenith cap already did it right. New `test_band_geometry` + a canvas assertion; measured **distinct X = 9 and 0 band pixels before, 371 after**.

**CRITICAL-2 — the sun was invisible** (2237766). `(a_raw + 128) >> 8` with a_raw clamped to 0x10000 yields 256, which truncates to 0 in a u8: centre alpha 0 for any energy ≥ ⅓ (default 1.0), rim alpha 0 by design, so the additive quad contributed nothing. Same defect class as the previous review's C2, re-introduced by hand-rolling a frozen §2 conversion (charter §29-6); fixed by calling `unit8_from_fx16`. **0 sun pixels before, 476 after.** MAJOR-4 folded in: the raster walked the primitive list in emission order (cloud-first) while sky_and_beams §1, the file header and the `SkyLayer` enum all pinned **sun before cloud**; the sky raster now sweeps the list twice.

**MAJOR-1 — `fx_sin` read past the end of the table** (4399420). `T[i+1]` with i = 256 on the 257-entry table, reached by every `fx_cos(0)`/`fx_cos(0x8000)` and `fx_sin(0x4000)`/`fx_sin(0xC000)`. Values were correct (t = 0 multiplied the garbage away), which is exactly why the exhaustive 2^16 golden compare could never see it. Guarded in C++ and TS; **spec/qformats.md §7.1 had the same defect** — its formula required a T[257] — and now carries the guard plus an "Endpoint guard" note. The regression test is a `static_assert`, so a reintroduction **cannot compile** (verified: *"array subscript value '257' is outside the bounds"*). No golden churn.

**MAJOR-2 — the fill rule dropped shared-edge pixels on BOTH sides** (57f1639). qformats §8 pinned `E' + bias > 0`; with a strict `>` a pixel centre exactly on a shared edge fails the top-left side (0 > 0) *and* the other side (−1 > 0), contradicting the "D3D top-left fill convention" named in the same sentence. Spec amended to `≥ 0` and `rast.cpp` with it. `fpga/rtl/raster/` is still empty, so the spec was corrected **before** any RTL freeze inherited it. `test_shared_edge_exactly_once` brings the Phase-4/5 exactly-once property forward: a 10×10 square split along its main diagonal (chosen because y = x puts every diagonal pixel centre exactly on the shared edge). **100 covered / 0 holes / 0 doubles after; 10 holes and 0/10 diagonal pixels claimed before**, matching the review's hand trace.

**MAJOR-5 — world-space markers grew with distance** (6cf33be). `fx_div_exact(size, c.s.d)` where `d` is 1/w computes size·w. Invisible because every fixture used an ortho matrix (w = 1), where multiply and divide agree exactly. New perspective fixture asserts each of three markers is smaller than the last and that doubling w halves the marker: **20 / 10 / 6 px after; all three saturate to the full 128 px viewport before.** No golden regen — the golden frame's markers are screen-space (flag 0x0002).

**CRITICAL-3 — "Formally proven." was false; the proof is real now** (1bc42a5). The claim is struck from blocks.yml. The proof had never been elaborated *once*, and underneath the SKIP its no-escape assertions were vacuous. The review's suspected cause (the mixed `rsp` driver) was **not** it — testing shows the proof passes with that driver restored. The real causes: (1) the lane SKIPped on any `"syntax error"` match, masking a property that had never parsed — and it never needed to, since `read_slang` reads these frozen packages fine (the INPUT lane has used it since W2.3); (2) **`(* anyseq *)` on local declarations does not survive this frontend** — it elaborates to constants, so the witness contained no environment signal at all and `arb_req.valid` was unreachable; (3) a free `rst_n` let the solver start mid-reset. Fixed with free environment **ports**, a deterministic reset counter, and a mandatory `cover` task. The LRM-illegal mixed driver is fixed regardless.

**Real result: bmc PASSES (depth 30), cover PASSES with all 7 covers reached** — including `c_forward`, the exact condition that was previously unreachable. **Mutation-verified** (4 mutations, each caught by the semantically right assertion). Found and fixed a genuine escape while doing it: `blit_span` was unclamped, so an over-large span wrapped `blit_base + blit_span` past 32 bits and the wrapped window admitted writes far outside the map; removing the new clamp makes the proof fail. The lane's SKIP is now pinned to one signature.

**⚠ NEW, NEEDS RATIFICATION — the D3 liveness bound B = 40 does not hold.** The arbiter and refresh harnesses had the same two defects and had also never run. Fixed the same way; the arbiter proof then **failed for real**. Measured by re-running with the bound raised: **fails at 40, passes at 60, 90 and 120** — true worst case 41..60 cycles. The derivation `B = G·MAX_BURST + REFRESH_OVERHEAD = 2·14 + 12` budgets one maximum burst per client turn, but a 64-byte request is four bursts. Deliberately **left failing and documented in `formal_mem_arbiter.sv`** rather than papered over by editing a frozen constant: choosing between re-deriving B and interleaving at burst granularity is an orchestrator call. MEM.VRAM.ARBITER sits at RTL_VERIFIED citing this property. (Lane is formal/nightly, not fast.)

**MEM.SDRAM refresh-bound property** — also never elaborated before, same two harness defects, both fixed and covers added. Its **cover task PASSES in ~11 min** (init_done at step 28,  at step 129, a real AUTO_REFRESH at step 812), so the bound is not a vacuous implication on . Its **bmc task was still running after 50 min and is NOT ESTABLISHED** — an unfinished proof, not a failing one; depth 900 is forced by the ~862-cycle bound. It is therefore kept runnable by hand and deliberately **excluded from the automated lane**, documented in both  and the .sby, rather than left to manufacture timeouts that read as failures. MEM.SDRAM is SPECIFIED / blocked_on: hardware, so no maturity claim rests on it today.

**Ledger honesty** (dc3eac0). MEM.HPS.BRIDGE was RTL_VERIFIED with **no** randomized differential — `tests.random` aliased the directed file and the log cited `mem_random.cpp`, which never instantiates the bridge. Written rather than demoted: `tests/memory/hps_bridge_random.cpp`, a PCG differential vs `zref::HpsBridge` (beat counts, 1-beat/cycle spacing, **constant** grant→first-beat latency, malformed rejection, per-client byte accounting); 300 bursts fast / 20,000 nightly; measured 131 reads, 124 writes, 45 malformed, latency constant at 17. Harness extracted to `hps_bridge_harness.hpp` so both tests drive the same one. Copy-pasted maturity evidence for MEM.GUARD and MEM.HPS.BRIDGE (both citing the *arbiter's* files — V3 only checks path existence) now cites files that actually exercise each block. MEM.GUARD's notes disclose that it still has no randomized differential either. `reports/blocked_on_hardware.md` de-staled: Quartus 17.0.2 is installed and 5189f0c opened the lane, so the blanket "no Quartus" line and the ZH-002 entry were wrong; board-dependent entries stay blocked.

**Orchestrator-relayed build failures** (b8db7e8). `cppcheck_check`: `VramArbiter::port_grant_` was reported uninitialised — it turned out to be **dead**, declared and referenced nowhere, so it was removed rather than given a plausible-but-meaningless initial value. Static linking: W4's note claimed `-static-libgcc -static-libstdc++` made the tests "PATH-order independent"; false, since neither covers **libwinpthread**, which every `-pthread` target still imported. Added `-static`; verified with `objdump -p` (only KERNEL32 + api-ms-win-crt-* remain) and by running three test binaries with `PATH=C:\Windows\System32` only.

**`mem_hps_bridge_directed` timeout — diagnosed, not a DLL stall.** It was not fixed by `-static`. The binary completes every check, **prints PASS, and then deadlocks in process teardown** (blocked, ~0.02 s CPU), so CTest only ever saw a timeout with no output at all. It reproduces from a bare shell and **predates this wave** (stale hung instances were in the process table before any change here). Worked around with a documented `_Exit` after the result is known — this skips the teardown lock, not any verification — and the teardown lock is filed for follow-up at the call site. The memory group went from 165 s with a 200 s timeout to **24 s, 9/9 passing**.

**Golden CRC lineage** (each regen committed with the fix that caused it): canvas `15AB7655 → 4D179BE6` (drum) `→ 5475D2D1` (sun + pass-6 order) `→ B56D4F2A` (fill rule); displayed `72D938D5 → 619FCA25 → EC9B19A7 → 8F17C2F6`.

**NOT DONE, by instruction:** MAJOR-3 (Duo canvas layout) is untouched — renderer and W2.2 RTL implement two incompatible readings of a self-contradictory spec, and it needs ratification before `wp/w2.2-video` merges.

---

## 2026-08-16 — renderer fix agent: the user's "weird lines", the black sub-metre terrain, the ratified Duo layout, and which way is up

Four items from `QUEUE-renderer-fixes.md` (1-4; items 5-6 wait for a separate design consolidation). Commits `cc98d94`, `f7ad8d8`, `8ef382e`, `3d6e422` on `main`, each pushed after a green fast lane. Fast lane 48/48 before and after (3 new test functions in `render_directed`, 1 in `render_heightfield`, 1 in `render_sky`); compiler 216/216; `abi:check` / `tables:check` / `ledger:check` clean throughout.

**1. The "weird lines" were CRACKS — two §8 coverage defects** (`cc98d94`). Instrumented, not guessed: a scratch harness built the rasteriser verbatim plus a per-pixel coverage counter and a per-primitive dump. Census of a full-screen drum at 384x240: **2160 background-coloured pixels, every one with coverage == 0** — covered by nobody, not double-filled, not a background primitive. Trace of pixel (37,60) at a seam whose subpixel x was 9547 (px 37.293): the right-hand column's primitives reported `EDGE_TEST=INSIDE` (E' = 6189/26206/403 against biases 0/0/-1) but `IN_BBOX=NO`; the left-hand column's reported outside.

*Root cause 1 — the scan bounding box.* It was `ceil(v_min/256)`, but a pixel is a candidate iff its CENTRE (`256p + 128`) can be inside, so the first column is `ceil((v_min-128)/256) = (v_min+127)>>8`. Whenever the seam's subpixel fraction lands in (0,128] the right-hand quad's bbox skips a column its own edge functions accept while the left-hand quad correctly rejects it — a full-height 1px crack showing the clear colour. Both axes. The max side was conservative rather than wrong and is tightened to the same law.

*Root cause 2 — found by the new exhaustive seam sweep.* The top-left bias was applied to the FLOORED `E' = E0 >> 8`, which merges "strictly inside by less than one subpixel² unit" with "exactly on the edge"; on a shared edge the two sides see `(E', -E'-1)` and for `E0 in [1,255]` BOTH reject. The bias now applies to the exact s64 `E0` — literally the D3D rule the convention names. The RTL keeps s32 tile stepping: the low 8 bits of `E0` are constant per edge, so `E0 >= 1` is `E' > 0 || (E' == 0 && r != 0)`.

`spec/qformats.md` §8 gains both laws (the bbox is part of the coverage law, not an optimization; the fill comparison is on `E0`). New tests, all verified to fail on the pre-fix rasteriser: `test_seam_subpixel_sweep` (a shared vertical AND horizontal seam through all 256 subpixel offsets), `test_no_seam_cracks_drum` (full-screen 48-column drum, ZERO background pixels allowed), `test_no_seam_cracks_terrain` (full-screen 33x33 heightfield).

**User-visible confirmation.** Rebuilt the site's own `zshot` gallery tool (read-only, from `../zhaozhou-site/tools/`) against the fixed renderer, into a scratch output directory, and re-ran the same scenes. Thin-insertion anomaly census (a column brighter than both neighbours while the neighbours match each other, sampled every 8th row): `sky-dusk` **67 -> 0**, `island-terrain` **44 -> 2**. The 2 survivors are single-row shading steps at x = 227 and 246 (1 of 240 rows each), not lines. Every column the orchestrator measured — sky 105/139/174/311/342/370 and island 21/81/105/131/185/354/370 — is gone.

**2. Heightfield normal quantised to whole world-units²** (`f7ad8d8`). The edge vectors are Q16.16, so their cross product is Q32.32 and the shift back to the Q16.16 the rest of the code assumes is `rescale(.,16)`. It was `rescale(.,32)` = Q32.0. Below ~1 m spacing every component of a near-flat cell rounds to 0, the `nmag2 == 0` guard fires, the whole patch is black. New `test_submetre_shading`: a 41x41 bump over ±12 m (0.6 m) must be lit over >90% of its extent and show >= 3 distinct undithered values (sampled at the Bayer B == 0 phase so the variation is shading, not dither), plus a 25x25 control at 1.0 m. Pre-fix: **0/11592 lit pixels, 1 distinct value**. This unblocks item 5 (bigger terrain) and Phase-6 Mantle patches.

**3. Duo packed two-block layout applied** (`8ef382e`), per `RATIFICATION-duo-layout.md`. `spec/video_rules.md` §1 now separates slot ALLOCATION (0x3C000, largest canvas, mode-switch stable) from frame OCCUPANCY (Duo 0x30000), with a paragraph that says in as many words that the old table mislabelled an allocation — so §1 and §3.1 cannot be read as contradictory again. §3.1 states the packed layout explicitly and §4 makes "on the displayed stream" literal for Duo. The renderer's Duo STORAGE raster is now 256x384 (the two view blocks STACKED), so the plain row-major resolve emits exactly `view0 = [0,0x18000)`, `view1 = [0x18000,0x30000)`; the 4x4 Bayer phase is unchanged for both views, so the dither law is untouched. `displayed_crc32c` walks two cursors: 24 black rows, then per row view-0's 512 B followed by view-1's 512 B, then 24 black rows.

*Correction to the ratification's finding 3:* the displayed CRC was NOT missing the border rows — `resolve.cpp` already CRC'd 24 + 192 + 24 rows. What was wrong was only the row assembly, which the packed layout changes. Noted so the record is accurate.

*A defect the layout change exposed:* `draw_form_marker`'s wall-clamp used viewport-RELATIVE bounds on canvas coordinates, so any viewport not at the canvas origin had its markers clamped into the other view's region and then scissored away. **Duo view 1's markers were silently invisible.** Fixed; this is why the golden canvas moves by more than a byte permutation — the second wizard renders for the first time. New `test_duo_packed_layout` pins view 1's first pixel to slot byte 0x18000, asserts a view-1-only draw writes nothing into [0,0x18000), all 256 marker pixels land in view 1's block, nothing is written past 0x30000, and recomputes the scanout-assembled displayed CRC by hand.

**4. Which way is UP** (`3d6e422`). `test_sky_orientation` renders a canonically-authored downward tilt three times (bands only / +cap / +under) and takes each layer's pixels as the ones it changes against the baseline, then asserts every zenith-cap pixel is in the upper half of the frame and every under-plane pixel in the lower half. Verified: negating the matrix row puts all 27,244 cap pixels below the midline and all 35,328 under pixels above it, and both assertions fire.

**Golden CRC lineage extended** (each regen in the commit that caused it): canvas `B56D4F2A -> 6396C558` (raster coverage) `-> 803958B5` (normal rescale) `-> D56BB9F4` (Duo layout + view-1 markers); displayed `8F17C2F6 -> 168F145B -> 99F2AD34 -> 8614BB64`.

**HANDOFF — the site's gallery tool needs a Duo update.** `../zhaozhou-site/tools/zshot.cpp:315-320` composes `duo-frame.png` by reading a 512-wide, 192-tall canvas. Under the ratified layout it must read view 0 as 256x192 from slot offset 0 and view 1 as 256x192 from 0x18000 and place them side by side. Measured against the current tool the image is now wrong (row 120 mean R: left 97 -> 4, right 105 -> 8). The sky and island shots need no change. Not touched here: that repo is outside this agent's scope.

**NOT DONE, by instruction:** queue items 5 (much larger/more complex terrain) and 6 (sun lens flare) — they wait for a separate design consolidation. Item 2 was the blocker for item 5 and is now cleared.

---

## 2026-08-17 — W3.3 HIR/ZIR, scheduler, C++ backend, maps, and costs — REMEDIATED; SECOND ADVERSARIAL VERIFICATION PENDING

The original W3.3 implementation commits remain on `main` and were followed by three isolated remediation commits, all pushed to `origin/main`:

1. `1cd7e92` — `Fix exact Form typing and stagger lowering`
2. `005cb80` — `Close deterministic Form C++ lowering`
3. `0a8a337` — `Ratify stagger cadence and cost accounting`

**Remediation delivered:** exact checker-to-HIR expression types (including imported nominal pool fields and symbolic Field IR table operands); hard HIR refusal when an admitted expression lacks an exact type; stable HIR/ZIR RNG call-site slots with authored stream seeding and persistent state advancement; ordinary and staggered rate lowerings made mutually exclusive; narrow stagger admission with global/off-loop writes refused; closed C++17 lowering over the W3.2-valid non-W3.4 surface; exact fixed-point division; eager source-order expression and argument evaluation; explicit lowering of every admitted intrinsic; imported pool capacities owner-qualified; consolidated refusal of all unlinked earth/flow invocations before file production; and useful presentation payloads with authored transforms/positions, legal handles/flags, ABI decode evidence, and practical ZRef visibility.

**Cadence and determinism evidence:** ordinary `every N` uses only `tick % N == 0`; staggered `every N` invokes every tick and selects `index % N == tick % N`. Native tests cover every residue, two complete cycles, 600 ticks, ordinary cadence, and the capacity-10/period-4 peak pattern `3,3,2,2`. The 600-tick CRC-32C hash-chain anchor is `7b14278f`; the retained fixture anchor is `d981e8e2 65c1abfe 07e322c7`. Independent fixed arithmetic/intrinsic and multi-stream RNG oracles pass, including repeated draws, slot mutation, multi-tick evolution, scenario-authored seeds, and repeat-run equality.

**Verification accounting:**

- **PASS:** compiler **242 passed**; ledger **40 passed**; fixgen **14 passed**; abi-gen **20 passed** — **316 passed total**.
- **FAIL:** **0** across those npm workspaces.
- **SKIP:** **0** across those npm workspaces.
- **PASS:** `form:check` verified **11 byte-stable files**. Two fresh generations and the pre-run tree shared aggregate SHA-256 `ee3d3f4296adb85be48c4f786a288993ef33ddbdb7f872cb36a68c05ca399036`; canonical `sourceids.zmap` and explicit-input cost-report mechanisms remain covered.
- **PASS:** WinLibs C++17 compiled and ran the complete W3.2-positive nonphysical surface with `-O2 -Wall -Wextra -Werror`, including all emitted translation units. Generated C++ contains no `float`, `double`, `<cmath>`, or host-clock token and has no undeclared generic intrinsic fallback.
- **PASS:** all six directly invoked renderer executables (`render_directed`, `render_heightfield`, `render_sky`, `render_star`, `render_golden`, `render_budget`) and the compiler's native presentation-to-`SoftwareRenderer` visibility test.
- **FAIL:** **0** in the reported native/compiler/renderer evidence.
- **SKIP:** **0** in the reported native/compiler/renderer evidence. CTest's six `BAD_COMMAND` results under Git Bash were harness path-construction errors, not test outcomes; the same six built executables were run directly and passed.

**Honest W3.4 boundary:** W3.3 does not emit physical Field IR wrappers and does not invent physical instruction counts. Every unlinked earth/flow invocation is refused before generated files are constructed. `costs.zcost` is emitted only when genuine W3.4 `FieldProgram` metadata is supplied; the former fabricated one-instruction fixture and golden were removed. Schedule-only rate/peak cost accounting remains canonical and tested.

This record means **remediated, not accepted**. W3.3 remains pending the parent's second adversarial verification, and W3.4 must not begin until that review completes.

## 2026-08-17 — W3.3 second adversarial review remediation — REMEDIATED; THIRD VERIFICATION PENDING

The second review verified four remaining blockers: imported nominal types lost their defining-module identity before HIR qualification; mandatory D11 `costs.zcost` had been omitted rather than truthfully representing unlinked declarations; stagger admission did not reject persistent RNG in all expression positions; and generated `SurfaceStamp` patch handles had no renderer resource binding. Three isolated commits remediate only those gaps, and all are pushed to `origin/main`:

1. `bea957b` — `Preserve nominal owners and stagger RNG safety`
2. `1200b52` — `Restore mandatory truthful Form cost reports`
3. `f858251` — `Bind SurfaceStamp terrain patch resources`

**Fixes and counterexamples:** checker `Type` values now carry exact owners for structs, enums, pools, and pool element structs; HIR consumes that identity directly and retains canonical numeric qualified names. A two-module fixture imports only values/functions/a pool—not either hidden type—and passes both exact HIR assertions and strict native compilation. Stagger's recursive AST admission walk rejects every persistent `random.stream`/draw operation before, inside, or after the selected iteration under `FORM-E-504`, including calls nested in conditions and record fields, while matched pure-expression placements remain legal. `costs.zcost` is mandatory again: linked physical Field IR appears only in `programs[]`; declarations awaiting W3.4 appear in exact-key `unlinked_programs[]` rows without instruction, cycle, DSP, table, class-count, or register claims. A tested same-source-ID transition moves a field between those arrays, and PresentZIR command templates are censused with ABI record bytes. Generated presentation resources now publish each deterministic nonzero stamp patch handle to a runtime terrain-patch callback; strict native evidence binds that exact handle to a real `TerrainPatch`, requires zero resource misses, and verifies persistent stamped sheet texels.

**Verification accounting:**

- **PASS:** focused checker/HIR/cost counterexample suites: **30 passed**, **0 failed**, **0 skipped**.
- **PASS:** compiler: **247 passed**; ledger: **40 passed**; fixgen: **14 passed**; abi-gen: **20 passed** — **321 passed total**, **0 failed**, **0 skipped**.
- **PASS:** the unfiltered WinLibs C++17 backend suite, including imported hidden nominals, all presentation-record decoding, real renderer linkage, fixed-point/RNG oracles, all stagger residues, the 600-tick anchor, and generated state/hash smoke.
- **PASS:** `form:check` verified the mandatory **12 byte-stable artifacts**, including restored `compiler/tests/form/golden/costs.zcost`.
- **PASS:** two complete 12-artifact generations were byte-identical; aggregate path-and-byte SHA-256: `26d35eff76fa203c4346dd4a9c5aababd48faf3617a8ae1526622dcf266b41ed`.
- **PASS:** generated C++ audit found none of `float`, `double`, `<cmath>`, `std::chrono`, host clock APIs, or host `time(...)`.
- **PASS:** all six direct renderer executables (`render_directed`, `render_heightfield`, `render_sky`, `render_star`, `render_golden`, `render_budget`); the 600-frame renderer budget chain remained `243DDA8B`.
- **PASS:** `HEAD == origin/main == f85825110c019e77d9194089c8e8057ec917a8c4`; each remediation commit has the required co-author trailer and only its logical paths.
- **PASS:** all **43** unrelated dirty/staged/untracked records remain byte-for-byte preserved; state fingerprint `e00da3176a6bce0e31b550efb6c3adb4c534cef4267efbd9f60b83d76ec489b8`. No W3.3-owned path remains dirty.

This record means **remediated, not accepted**. W3.3 is stopped pending the parent's third adversarial verification. W3.4 has not begun.

## 2026-08-17 — W3.3 third adversarial review remediation — REMEDIATED; FOURTH VERIFICATION PENDING

The third review identified four remaining blockers: namespace-scope aggregate constants were lowered through capture-default lambdas instead of legal direct `constexpr` initialization; imported-struct recursion tracked unqualified names rather than declaration owners and identities; transient presentation handles truncated module identity and collided for modules 0 and 256; and two Form specifications still assigned emit sites stale source kind 6 instead of authoritative kind 9. Three isolated commits remediate only those findings, and all are pushed to `origin/main`:

1. `f3b553ad3901e98e4e34206f78e8217d3078439c` — `Fix Form constant and recursion admission`
2. `740811c7b0fe8a30fb3ccd3801c771e0a5d75dbe` — `Fix constexpr and presentation resource lowering`
3. `d4f2c0aab911b3bb7811f123d44a741e71c1138d` — `Synchronize Form source-kind documentation`

**Fixes and counterexamples:** namespace constants now use direct aggregate initialization in declaration-field order, exact owner-qualified enum casts, recursive genuine-constant admission, and runtime-compatible fixed-point/integer folding; nonconstant initializers are refused, while eager source-order runtime aggregate lowering remains unchanged. Strict native fixtures cover imported and local structs, `world2`, `world3`, `velocity3`, `colour8`, enums, fixed-point division, and later constant/global uses. Struct-cycle DFS now carries each declaration owner, resolves fields in that owner, and keys visited/active state by owner plus `StructDecl` identity: the exact acyclic imported-shadowing graph is accepted, while real local and cross-module cycles are rejected without false cycles for equal unqualified names in different modules. Presentation resources now receive bounded canonical cartridge-wide generation-1 handles keyed by full source ID and role, reject duplicates and overflow before output, and publish complete compile-time mapping rows. The strict 257-module counterexample proves modules 0 and 256 with identical local source indices have distinct, stable DrawForm transform and SurfaceStamp terrain-patch handles across frames and views, with real renderer resources, zero misses, and distinct persistent sheets. `capture_format.md` §5 remains authoritative; both Form specifications now say kind 9, and a synchronization test pins the compiler and documentation registries mechanically.

**Verification accounting:**

- **PASS:** compiler **255 passed**; ledger **40 passed**; fixgen **14 passed**; abi-gen **20 passed** — **329 passed total**, **0 failed**, **0 skipped**.
- **PASS:** `form:check` verified all **12 byte-stable artifacts**. A second complete generation was byte-identical; aggregate path-and-byte SHA-256: `dedd9e1e7667ad1bc5199b200fa90b3dc67cbf2e744b0ee0f7191a4810985db0`.
- **PASS:** strict WinLibs C++17 (`-std=c++17 -O2 -Wall -Wextra -Werror`) namespace-aggregate and 257-module native render tests, including exact mapping-table agreement, stable handles, visible transforms/terrain patches, and persistent sheets.
- **PASS:** all six direct renderer executables (`render_directed`, `render_heightfield`, `render_sky`, `render_star`, `render_golden`, `render_budget`); the 600-frame budget completed in **5.36 s** with CRC chain `243DDA8B`.
- **PASS:** generated-C++ audit found no namespace `inline constexpr` capture-default lambda, `transient_handle`, `float`, `double`, `<cmath>`, host clocks, `time(...)`, `while`, or `goto`.
- **PASS:** mandatory `costs.zcost` remains truthful: 12 artifacts, a 208-byte per-frame command estimate, two real command templates, real pool/rate data, no fabricated physical program, and only declaration metadata in `unlinked_programs`.
- **PASS:** `HEAD == origin/main == d4f2c0aab911b3bb7811f123d44a741e71c1138d`; every remediation commit has the required co-author trailer.
- **PASS:** all **43** unrelated dirty/staged/untracked records remain byte-for-byte preserved; state fingerprint `e00da3176a6bce0e31b550efb6c3adb4c534cef4267efbd9f60b83d76ec489b8`. No W3.3-owned path remains dirty.

This record means **remediated, not accepted**. W3.3 is stopped pending the fourth adversarial acceptance review. W3.4 has not begun.

## 2026-08-17 — W3.3 fourth adversarial review remediation — REMEDIATED; FIFTH VERIFICATION PENDING

The fourth review identified twelve remaining blockers: compositional member/index identity; owner-qualified scheduler keys; complete scenario/TestZIR semantics; shared whole-module qualification; dependency-correct declarations; definite return; stream-select reference identity; exact source-ID admission; full-u32 authored-resource mapping; exact declaration numeric bounds; an explicit comparison matrix; and collision-proof pool-layout names.

Five pushed commits contain the remediation and its canonical artifacts:

1. `427cd1744a98e371526516ebf8be347a74bb4af6` — `Fix exact Form ownership and TestZIR semantics`
2. `39c26016240b83f82b530fdac2562353b8bda9a7` — `Close deterministic Form C++ identity gaps`
3. `d838c2949feb2a8f2c990e8691afdd16376aba3b` — `Regenerate exact Form semantic artifacts`
4. `249e3eb41ee8587ad2723b215b6d4f30d7dc7d12` — `Keep concurrent capture specification out of W3.3`
5. `386f935f18a36e7eb198e6dbdb8de6a3d0a00c63` — `Specify exact Form source and resource identities`

**Semantic remediation:** ordinary member/index HIR remains recursively compositional, while only direct authored SoA selections carry pool-column metadata; pool membership is separate. Access and schedule identities now carry declaration owners, and one qualified resolver covers whole-module constants, enums, types, functions, pools, globals and effect declarations. Structs and constants emit in stable dependency order with cycle refusal. Value functions use control-flow definite-return analysis. Comparison admission is an explicit numeric/bool/colour/exact-enum matrix and refuses aggregates/handles. Stream-valued eager selects preserve source-order evaluation but return the selected persistent RNG slot by reference.

Source allocation now counts only pools, systems, field programs, each emit and scenarios, starts at local row zero, admits exactly 4096 modules × 65536 rows and validates every generated ID. Exact bigint checks precede host conversion for arrays, pools, enums, rates and scenario numbers. TestZIR lowers all seven scenario operations into immutable native-consumable scripts with exact owner/source identity and a driver surface. Authored resources map `(semantic role, full u32 ID)` to collision-free generation-1 handles before transient allocation; IDs 1 and 16777217 remain distinct. Generated pool metadata and every authored column use disjoint, injective names through spawn, kill, compaction and serialization.

`spec/capture_format.md` was already one of the 43 concurrent dirty paths. Commit `249e3eb` removes its concurrent bytes from the W3.3 commits and restores that path byte-for-byte; the normative Form source-row and authored-resource laws therefore live in clean `spec/form/language-semantics.md` §3.4.1 rather than consuming someone else's worktree state.

**Verification accounting:**

- **PASS:** compiler **271 passed**; ledger **40 passed**; fixgen **14 passed**; abi-gen **20 passed** — **345 passed total**, **0 failed**, **0 skipped**.
- **PASS:** the complete 21-test C++ backend suite, including strict WinLibs C++17 compilation with `-std=c++17 -O2 -Wall -Wextra -Werror`, nested composition/pool lifecycle, whole-module declarations, full-u32 page separation, declaration order, independent true/false RNG-slot oracles, the full positive corpus and all seven native scenario operation kinds.
- **PASS:** focused checker and HIR/TestZIR counterexamples independently assert exact scheduler ownership, qualification, definite return, numeric boundaries, comparison admission, source-row zero/boundaries and direct-column metadata.
- **PASS:** `form:check` verified all **12 byte-stable artifacts**. A second complete generation was byte-identical; aggregate path-and-byte SHA-256: `78ed61a8a23b128d81075eada667a264889f1b775620a3b72c97ef8f575249ab`.
- **PASS:** generated-code audit found no namespace capture-default lambda, `transient_handle`, floating-point type/header, host clock/time API, `while`, or `goto`; no page-ID low-24-bit derivation remains.
- **PASS:** mandatory `costs.zcost` remains truthful: 208-byte per-frame estimate, two real command templates, exact pool/rate rows, no fabricated physical program, and only declaration metadata in `unlinked_programs`.
- **PASS:** all six direct renderer executables (`render_directed`, `render_heightfield`, `render_sky`, `render_star`, `render_golden`, `render_budget`); the 600-frame budget completed in **3.61 s** with CRC chain `243DDA8B`.
- **PASS:** `HEAD == origin/main == 386f935f18a36e7eb198e6dbdb8de6a3d0a00c63`; every commit has the required co-author trailer.
- **PASS:** all **43** unrelated dirty/staged/untracked records remain byte-for-byte preserved; state fingerprint `e00da3176a6bce0e31b550efb6c3adb4c534cef4267efbd9f60b83d76ec489b8`. No W3.3-owned path remains dirty.

This record means **remediated, not accepted**. W3.3 is stopped pending the fifth adversarial acceptance review. W3.4 has not begun.

## 2026-08-17 — W3.3 fifth adversarial review remediation — REMEDIATED; SIXTH VERIFICATION PENDING

The fifth review rejected the prior remediation on four remaining blockers: `capture_format.md` did not itself carry the authoritative source-row and full-u32 resource-handle laws; authored Form names could collide with generated C++ symbols; checker and HIR constant reduction disagreed and silently recovered required bounds as zero; and whole-module-qualified flow calls did not follow the selective-import resolution path or reach the intended W3.4 refusal. Three isolated commits remediate those findings, all pushed to `origin/main`:

1. `1fc6bf2b7716a3ee817bd61a1c351ed68ebf35ce` — `Restore authoritative W3.3 registry laws`
2. `e32d353625f16fe0b6697179eeef952b80c36971` — `Fix exact bounds and qualified flow calls`
3. `6674426101ad049d9f0531c756ee41fd04dc9441` — `Isolate authored C++ identifiers`

**Fixes and counterexamples:** `capture_format.md` now explicitly assigns zero-based rows only to pools, systems, field programs, every presentation emit, and scenarios, admits module IDs `0..4095` and local rows `0..65535`, and keys authored resource handles by semantic role plus the complete u32 ID; IDs 1 and 16777217 and equal IDs in different roles remain distinct. A shared bigint exact evaluator now supplies checker and HIR semantics for typed width normalization, unary `!`/`~`, shifts, arithmetic, comparisons, booleans, enums, and constants. Irreducible array and pool bounds diagnose before HIR, and HIR no longer has a zero fallback. Qualified and selective function/field/system calls share owner-preserving resolution, so whole-module-qualified flow calls retain canonical pool effects and reach the explicit unlinked-W3.4 physical-field refusal. Authored C++ spelling is now deterministic and injective, with separate collision domains for module namespaces, declarations/locals, generated pool/system/presentation/scenario families, and leading-underscore temporaries; strict native adversarial fixtures cover `u32`, `State`, `state`, `entries_pool`, generated prefixes, and `_form_value_0`.

**Verification accounting:**

- **PASS:** direct capture-law assertions **6 passed**; focused checker/HIR regressions **43 passed**, **0 failed**, **0 skipped**.
- **PASS:** complete C++ backend suite **25 passed**, **0 failed**, **0 skipped**, including native C++17 compilation with `-std=c++17 -O2 -Wall -Wextra -Werror` and the qualified-flow W3.4 refusal.
- **PASS:** compiler **279 passed**; ledger **40 passed**; fixgen **14 passed**; abi-gen **20 passed** — **353 passed total**, **0 failed**, **0 skipped**.
- **PASS:** `form:check` verified all **12 byte-stable artifacts**. A second complete `form:gen` was byte-identical; SHA-256 over the ordered per-file SHA-256 listing remained `ff78f58bfbf4b695cf01e0da533fcbc34e7444f0ce4ed8092fd805bda6c1f2bd` before and after.
- **PASS:** all **43** pre-existing dirty/staged/untracked records remain preserved. The raw whole-state digest is expected to differ because it includes the intentionally changed `capture_format.md` law hunks. Reversing exactly commit `1fc6bf2` on a copy of the current mixed worktree file reconstructs the original worktree SHA-256 `b0320bedc679f6138bca489bbafb91b2e0934fe6a39d1b0c36fc0c202d70e742`; doing the same to the current index copy reconstructs original blob `133312435ab22c801c9f71374e9a10d793b2d6ca`. With only those intentional law hunks normalized, the original 43-record state fingerprint is exactly `e00da3176a6bce0e31b550efb6c3adb4c534cef4267efbd9f60b83d76ec489b8`; every other path matches its original index blob and worktree bytes.
- **PASS:** `HEAD == origin/main == 6674426101ad049d9f0531c756ee41fd04dc9441`; every remediation commit has the required co-author trailer and was pushed. The repository still has exactly **43** dirty records, and among paths touched by this remediation only the expected mixed `spec/capture_format.md` worktree record remains dirty.

This record means **remediated, not accepted**. W3.3 is stopped pending a sixth adversarial acceptance review. W3.4 has not begun.

## 2026-08-17 — W3.3 sixth adversarial review remediation — REMEDIATED; FURTHER ACCEPTANCE VERIFICATION PENDING

The sixth review rejected the fifth remediation on five P1 blockers: incompatible `sourceids.zmap` layouts and a u16 string-offset bottleneck; inherited C++ helper/type collisions; checker/HIR drift for aggregate constant projection; inexact `Number`-based Q-format range checks; and divergent qualified/selective callable resolution with qualifier/local precedence gaps. Six isolated commits remediate those findings, all pushed to `origin/main`:

1. `fba476b5eb286c882b3c3cdb5e0cbe79875ac464` — `Unify canonical Form source maps`
2. `5343b781733fbc9c3709765eb22a1c59d0dca4c6` — `Prevent inherited C++ helper collisions`
3. `497716186941b1400951adc7ad9aa28342a3ca01` — `Reduce aggregate constants exactly`
4. `fae35e5e0fb450a16ac6f2e8c0043ac0ae1718b3` — `Check Q-format literals with exact rationals`
5. `dd1abf15c73667ecd64f5da71cc011fb3c57f27d` — `Canonicalize callable resolution`
6. `5793cdd6a5407f61ff2ef72bd13b16746fc3f21d` — `Preserve flow diagnostic precedence`

**Fixes and counterexamples:** capture producers, generated consumers, the specification and golden producer now share the 32-byte ZSMP header, 24-byte rows, 8-byte file rows, u32 UTF-8 offsets and CRC-32C over `[32, EOF)`; coverage includes direct producer/consumer round-trip, all 65,536 rows with a multi-megabyte string table, and malformed boundaries. Authored C++ identifiers are disjoint from inherited `form` helper/type families; a strict native counterexample proves an authored `fx16_add` cannot hide generated fixed-point addition. One shared exact reducer now projects records, named aggregates and admitted fixed arrays/member/index forms identically for checker and HIR, preserving declaration owners and refusing irreducible values instead of substituting zero. Q-format rails use signed BigInt rational comparisons, including huge numerators/denominators and exact positive/negative rails. The checker now owns canonical intrinsic/function/field/system call targets and flow-pool owners; HIR consumes that identity without spelling reconstruction, whole-module and selective imports agree, qualifier-shaped locals take lexical precedence, ambiguity remains diagnostic, and legal qualified flow calls reach the explicit W3.4 preflight refusal. The follow-up preserves `FORM-E-667` as the first diagnostic for a wrong flow pool while retaining secondary diagnostics.

**Verification accounting:**

- **PASS:** canonical source-map artifact/generated-conformance suite **15 passed**, **0 failed**, **0 skipped**, including round-trip, maximum-row/large-string-table and malformed-boundary cases.
- **PASS:** complete C++ backend suite **26 passed**, **0 failed**, **0 skipped**, including strict native C++17 with `-std=c++17 -O2 -Wall -Wextra -Werror`, inherited-family collisions and qualified-flow W3.4 refusal.
- **PASS:** final focused checker/HIR suite **51 passed**, **0 failed**, **0 skipped**; the targeted diagnostic rerun passed **4** selected cases with **201** excluded by its name filter.
- **PASS:** complete compiler **290 passed**, **0 failed**, **0 skipped**. Full npm workspace matrix: compiler **290**, ledger **40**, fixgen **14**, ABI generator **20** — **364 passed total**, **0 failed**, **0 skipped**.
- **PASS:** prior blocker matrix **23/23** explicitly identified tests, covering source-row limits, owner-qualified schedules/effects, qualification, exact bounds and rails, nested composition, TestZIR, declaration ordering, definite return, RNG identities, full-u32 resources, comparison admission and pool metadata/layout.
- **PASS:** `form:gen` wrote all **12** artifacts; `form:check` verified all **12** byte-stable files; a second complete generation was identical. Ordered path-and-file SHA-256 remained `a6af3d51c651bfa0fc1152585d5241fdef4542bc078f35eb237d6a77d86a8ccf` before and after both generations.
- **PASS:** `HEAD == origin/main == 5793cdd6a5407f61ff2ef72bd13b16746fc3f21d`; every remediation commit has the required co-author trailer and was pushed.
- **PASS:** all **43** pre-existing dirty/staged/untracked records retain their unrelated state. The same 43 status/path records remain; **39/39** paths not mixed with the source-map remediation match their baseline index blobs and worktree SHA-256 values directly. On the four mixed paths (`compiler/src/generated/zcap.ts`, `compiler/tests/generated_conformance.test.ts`, `spec/capture_format.md`, `tests/abi/golden/zcap_minimal.zcap`), removing only the isolated source-map block/test/spec replacements or replacing only binary ZCAP section type 9 reconstructs every baseline index blob and worktree SHA-256 exactly. The resulting normalized 43-record fingerprint is the sixth-review baseline `4557159fc74b743341ce09c22b5c321d57c2664fafcfd15b1036d1ccd8a313e4`.

This record means **remediated, not accepted**. Independent acceptance remains pending. **W3.4 has not begun.**

## 2026-08-17 — W3.3 seventh adversarial review remediation — REMEDIATED; INDEPENDENT ACCEPTANCE PENDING

The seventh review rejected the sixth remediation on four P1 blockers: exact aggregate projections could reinterpret a shadowing declaration name as a whole-module qualifier; a bare call could bypass a winning parameter/local/non-callable declaration and fall through to a selectively imported callable; authored module names did not map injectively and safely to case-insensitive filesystem artifact paths; and standalone/container source maps lacked one globally enforced byte-size law. Four isolated commits remediate those findings, all pushed to `origin/main`:

1. `c5c0c390ebb179f3d8143d4ae32dad83cade6b19` — `Make Form artifact paths filesystem-safe`
2. `31330b56e2a8552449a4cd5239599acf3b5c6029` — `Enforce the Form source-map size law`
3. `86e3358dbd02a5f7afbb26fa6e833da388091248` — `Honor aggregate shadows over module qualifiers`
4. `ed6eb48fd111c76447a22e2f8b04f72360f62b71` — `Respect lexical shadows in bare Form calls`

**Fixes and counterexamples:** checker exact reduction and HIR lowering now treat a whole-module qualifier only when no parameter, current/future let, current-module declaration or selective import wins that spelling. The exact imported-module/local-aggregate counterexample lowers `settings.capacity` from the local constant and proves pool capacity 4 rather than the imported module's 9. Bare calls use the same binding precedence: parameters, current lets and future lets cannot fall through to imported callables; non-callable constants/globals/pools stop resolution; ambiguity remains `FORM-E-205`; and explicit `library.invoke(...)` remains the whole-module escape. Invalid shadows record no canonical call target.

C++ artifact stems now preserve safe ordinary lowercase module names and otherwise use an injective UTF-8 hexadecimal encoding with a reserved prefix. Preflight rejects empty names, unpaired UTF-16 surrogates and names over 64 UTF-8 bytes, and case-folded collision checks cover fixed outputs (`form_game`, `form_types`), encoded-prefix aliases, Windows device stems, path-like names, non-ASCII names and case-only variants before any file is written.

`sourceids.zmap` v1 now has one inclusive **134,217,728-byte (128 MiB)** ceiling for the complete map including its 32-byte header, identically for standalone files, ZCAP and ZPAK. Producers and consumers perform exact BigInt structural arithmetic before allocation or host-number narrowing; u32 wire fields remain representational limits and u64 container framing does not widen the map law. The compiler producer, ABI-generator builder, generated-consumer template, generated consumer and all three normative specifications carry the same rule. ZCAP table totals, section offsets and section lengths are compared in BigInt against the actual file before conversion.

**Verification accounting:**

- **PASS:** focused checker exact-name/call-shadow suite **36 passed**, **0 failed**, **0 skipped**; focused HIR/ZIR exact-constant suite **18 passed**, **0 failed**, **0 skipped**.
- **PASS:** focused source-map/artifact suite **9 passed**, **0 failed**, **0 skipped**. It covers the exact 128-MiB inclusive boundary, maximum+1 refusal without a large allocation, negative/individual/aggregate overflow, producer-consumer law identity, round-trip and CRC, malformed boundaries, oversized SOURCE_MAP admission, and u64 offsets/totals above `2^53` without precision loss.
- **PASS:** generated capture conformance **8 passed**, **0 failed**, **0 skipped**; ABI-generator suite **21 passed**, **0 failed**, **0 skipped**; `abi:check` is clean with **26 outputs matching**.
- **PASS:** complete strict C++ backend suite **28 passed**, **0 failed**, **0 skipped**, including native C++17 compilation and execution with `-std=c++17 -O2 -Wall -Wextra -Werror` for adversarial module paths and the complete backend surface.
- **PASS:** complete compiler **297 passed**, **0 failed**, **0 skipped**. Full npm workspace matrix: compiler **297**, ledger **40**, fixgen **14**, ABI generator **21** — **372 passed total**, **0 failed**, **0 skipped**.
- **PASS:** prior W3.3 blocker matrix remains **23/23**, covering source-row boundaries, owner-qualified schedules/effects, whole-module qualification, exact bounds and Q-format rails, nested composition, TestZIR, declaration ordering, definite return, RNG identities, full-u32 resources, comparison admission and pool metadata/layout.
- **PASS:** two complete `form:gen` / `form:check` cycles each wrote and verified all **12** byte-stable artifacts. The sorted, path-and-byte-length-framed artifact SHA-256 was identical both times: `dfae0c5265b1f34b2db6ce04b86e715b0c982ce9ee46093886d788ed8dd749fa`.
- **PASS:** commit validation found exactly the four logical path sets above, no whitespace errors, and the required `Co-Authored-By: Claude <noreply@anthropic.com>` trailer on every commit. `HEAD == origin/main == ed6eb48fd111c76447a22e2f8b04f72360f62b71`.
- **PASS:** the exact seventh-review inventory remains **43** dirty/staged/untracked records with identical status/path pairs. **40/40** non-overlapping paths retain their exact baseline index blobs and worktree SHA-256 bytes. On the three intentionally mixed paths (`compiler/src/generated/zcap.ts`, `spec/capture_format.md`, `tools/abi-gen/test/abi_gen.test.ts`), reversing only commit `31330b5`'s size-law transformation in memory reconstructs every baseline index blob and worktree SHA-256 exactly. The normalized state fingerprint is the seventh-review baseline `69fed592958e365a6a9ddd43994d38040a9addbe40b4ce040c7c3235c65f803c`.

This record means **remediated, not accepted**. W3.3 remains stopped pending another independent acceptance review. **W3.4 has not begun.**

## 2026-08-17 — W3.3 eighth adversarial review remediation — REMEDIATED; INDEPENDENT ACCEPTANCE PENDING

The eighth review rejected the seventh remediation on three P1 blockers: native SOURCE_MAP helpers still used the legacy count-prefixed 16-byte format rather than canonical ZSMP v1; bare intrinsics could override ordinary current-module or imported functions; and enum/pool/aggregate/call-qualifier fast paths could bypass future-let reservation and omit `FORM-E-303`. Two logical commits remediate all three findings and are pushed to `origin/main`:

1. `3f48592ee7adb88b554e50f0a7a629f4d2e12692` — `Fix Form call and future-let precedence`
2. `0d1859d59f49065b41a805248c1b2cafb9342909` — `Implement canonical native SOURCE_MAP interop`

**Fixes and counterexamples:** the checker now applies one lexical-root reservation law, including future lets, before enum, pool metadata/field, aggregate, nested member/index and call-qualifier fast paths. Ordinary current-module declarations, selective imports, ambiguity and non-callability all precede intrinsic fallback. Every unique bare intrinsic spelling is tested against a same-named authored function; qualified intrinsic-like member names remain ordinary calls. Checker-owned call targets lower unchanged into HIR, rejected future-let sources have no call targets, and `lowerHir` returns `null`. Strict generated C++ executes authored `min(2, 3)` as 5 rather than intrinsic minimum 2.

The native SOURCE_MAP API now models the complete canonical map and implements the exact 32-byte header, 24-byte ascending entry rows, 8-byte file rows, u32 counts/offsets, source spans, optional u32 program hashes, UTF-8 NUL strings, CRC-32C over `[32, EOF)`, inclusive 128 MiB complete-map ceiling and non-allocating wide structural arithmetic. It returns structured build/parse errors, rejects embedded NUL or malformed UTF-8 on emission, and returns no partial map on any parse fault. All three native callers were audited and migrated. The native minimal ZCAP remains byte-identical to the TypeScript-generated committed golden.

A genuine cross-language bridge is compiled with `-std=c++17 -O2 -Wall -Wextra -Werror`: compiler-emitted bytes are parsed natively and every semantic field is compared; native-emitted bytes are consumed by generated TypeScript `parseSourceMap` and compared semantically. The same 23-case byte corpus is then classified by both implementations with identical results: three valid maps accepted and twenty malformed maps rejected. The malformed set covers truncation, magic/version/header flags/reserved bytes, structural count/length arithmetic, CRC, duplicate ordering, kind, file bounds and module/file mismatch, entry flags, reversed spans, hash flag/value mismatch, file reserved words, string bounds/termination and fatal UTF-8. Native non-allocating checks separately prove exact-max admission, max+1 refusal, u32 structural overflow, oversized parse refusal and no partial result.

**Verification accounting:**

- **PASS:** focused checker/HIR suite **60 passed**, **0 failed**, **0 skipped**, including intrinsic precedence, all future-let fast paths and rejected-source no-lowering.
- **PASS:** source-map artifact plus generated-capture conformance suites **18 passed**, **0 failed**, **0 skipped**. The artifact suite alone is **10/10**, including both interoperability directions and shared malformed-corpus parity.
- **PASS:** complete strict C++ backend suite **29 passed**, **0 failed**, **0 skipped**, with native compilation/execution under `-std=c++17 -O2 -Wall -Wextra -Werror`.
- **PASS:** native CMake capture/reference matrix **11 passed**, **0 failed**, including CRC, ABI golden, ZCAP round-trip, capture verification, reel CRC, ABI staleness/fuzz parity, empty-frame replay, ZRef shell and field round-trip/fuzz parity. Direct migrated tests also report ZCAP **40 checks passed**, empty replay **37 checks passed**, and crater-ring all gates green.
- **PASS:** complete compiler **305 passed**, **0 failed**, **0 skipped**. Full npm workspace matrix: compiler **305**, ledger **40**, fixgen **14**, ABI generator **21** — **380 passed total**, **0 failed**, **0 skipped**.
- **PASS:** prior W3.3 blocker matrix remains **23/23**, covering source-row boundaries, owner-qualified schedules/effects, whole-module qualification, exact bounds and Q-format rails, nested composition, TestZIR, declaration ordering, definite return, RNG identities, full-u32 resources, comparison admission and pool metadata/layout.
- **PASS:** two complete `form:gen` / `form:check` cycles each wrote and verified all **12** artifacts. The sorted path-and-byte-length-framed SHA-256 was identical both times: `dfae0c5265b1f34b2db6ce04b86e715b0c982ce9ee46093886d788ed8dd749fa`.
- **PASS:** `abi:gen` completed and `abi:check` reports all **26 outputs matching**.
- **PASS:** both commits contain only their exact logical path sets and carry `Co-Authored-By: Claude <noreply@anthropic.com>`. `HEAD == origin/main == remote main == 0d1859d59f49065b41a805248c1b2cafb9342909`.
- **PASS:** all **43** eighth-review unrelated dirty/staged/untracked records remain preserved. **42/42** paths not mixed with this remediation retain their current baseline index state and worktree bytes directly. On the one mixed path, `reference/include/zref/zref_frame.hpp`, reversing only commit `0d1859d`'s SOURCE_MAP API change in memory reconstructs baseline index blob `0bf70c4a105e628ab04b5c50487ce4f84266a19e` and baseline worktree SHA-256 `22e127534a5935ea5590dfa4776ed323089764835025bbed3bee7bb92927b0d8`, while preserving the unrelated celestial/environment enums. The normalized 43-record state fingerprint exactly matches the eighth-review baseline: `1dfb580f75c6f8e1e8ecdc41f206569a9bba84a7dc0356dbc2fc8a8b5c85481f`.

This record means **remediated, not accepted**. W3.3 remains stopped pending independent acceptance. **W3.4 has not begun.**

## 2026-08-17 — W3.3 ninth adversarial review remediation — REMEDIATED; INDEPENDENT ACCEPTANCE PENDING

The ninth review rejected the eighth remediation on one P1 blocker: field bodies did not initialize the future-let reservation set, so a later field-local `min` could incorrectly allow an earlier bare `min(...)` to resolve as the intrinsic. Commit `9f44327e0acd6dcd969d7feb8fe3fe09c59e2897` (`Reserve future lets per lexical body`) remediates the blocker and is pushed to `origin/main`.

The checker now enters every executable lexical body through one shared body-context initializer. Direct `let`/`field_let` declarations reserve their bare spellings from the body's first expression; child bodies inherit enclosing bindings and outstanding reservations but initialize their own direct reservations independently. Function, system, `@earth` field, `@flow` field, `if`/`else`, and loop bodies follow this law. Nested declarations no longer reserve names in parent or sibling scopes, and nested locals no longer leak. L1 scenarios/scripts admit expressions but no local-let body, so their valid intrinsic use is covered as preservation evidence. The canonical lexical-root reservation continues to guard root, member, index, and call fast paths.

The exact earth-field counterexample and equivalent flow/function/system/nested cases now emit exactly one `FORM-E-303`, record zero checker-owned canonical call targets, and return `null` from `lowerHir`. Positive regressions prove valid `min`/`max` intrinsic calls remain accepted in earth and flow fields, functions, systems, scenarios, parent scopes, sibling branches, and after unrelated nested scopes.

**Verification accounting:**

- **PASS:** focused checker/HIR suites **64 passed**, **0 failed**, **0 skipped**.
- **PASS:** prior lexical/call blocker matrix **23/23 selected tests passed**, **0 selected failures**; **70** non-matching tests were intentionally excluded by the name filter.
- **PASS:** complete strict C++ backend suite **29 passed**, **0 failed**, **0 skipped**, including native compilation under `-std=c++17 -O2 -Wall -Wextra -Werror`.
- **PASS:** complete compiler **309 passed**, **0 failed**, **0 skipped**. Full npm workspace matrix: compiler **309**, ledger **40**, fixgen **14**, ABI generator **21** — **384 passed total**, **0 failed**, **0 skipped**.
- **PASS:** two complete `form:gen` / `form:check` cycles each wrote and verified all **12** byte-stable artifacts. The sorted golden-relative path-and-byte-length-framed SHA-256 was identical both times: `dfae0c5265b1f34b2db6ce04b86e715b0c982ce9ee46093886d788ed8dd749fa`.
- **PASS:** no ABI file was touched, so no additional ABI generation was required; the full workspace still passed all **21** ABI-generator tests.
- **PASS:** the commit contains exactly `compiler/src/frontend/checker.ts`, `compiler/tests/frontend/checker.test.ts`, and `compiler/tests/form/hir_zir.test.ts`, has no whitespace errors, and carries `Co-Authored-By: Claude <noreply@anthropic.com>`.
- **PASS:** all **43** unrelated dirty/staged/untracked records were preserved exactly. Their length-framed status/index/worktree fingerprint was identical before and after commit: `f88eb76ca4bba53e6a96df414d10874fca6d6c6da7c3997acddb6b66a2468685`.
- **PASS:** `HEAD == origin/main == remote main == 9f44327e0acd6dcd969d7feb8fe3fe09c59e2897`.

This record means **remediated, not accepted**. W3.3 remains stopped pending independent acceptance. **W3.4 has not begun.**

## 2026-08-17 — W3.3 tenth adversarial review remediation — REMEDIATED; INDEPENDENT ACCEPTANCE PENDING

The tenth review rejected the ninth remediation on two P1 resolver-drift blockers. Specialized declaration operands (`spawn`, `kill`, pool-sugar `for`, and field `apply`) could reinterpret a qualified root through module imports even when a local or future `let` reserved that spelling. Separately, exact loop-bound reduction could reinterpret a checked local, parameter, loop index, future `let`, global, or shadowed import as a same-spelled top-level constant and incorrectly admit a runtime upper bound. Commit `ad41d38f493e2bb4ea6f0a15c39605a018bca871` (`Canonicalize declaration operands and loop bounds`) remediates both blockers and is pushed to `origin/main`.

The checker now applies one lexical-root precedence law to all declaration-valued operands and records checker-owned canonical identities for pools, pool element structs, ordinary record types, and field programs. Whole-module and selective-import spellings converge on the same owner identity; local roots win, future lets emit `FORM-E-303`, and rejected operands record no target. HIR requires and validates these identities, carries canonical targets on specialized statements and record expressions, and returns no HIR after frontend rejection. The C++ backend consumes HIR targets for pool iteration, spawn, kill, compaction discovery, and field-application preflight; its authored-string statement declaration resolver was removed.

Exact reduction now distinguishes AST nodes in the checked source expression from declaration-owned constant initializers. Source identifiers, qualified roots, enum members, aggregate projections, indices, apply durations, presentation resource IDs, player indices, and loop bounds reduce under their checked lexical context, while followed constant initializers retain their declaration owner's context. Stagger-shape analysis runs after body checking and consumes the checked exact-value, canonical pool-count, call-target, and declaration-target identities rather than re-resolving syntax. Runtime/shadowed upper bounds emit `FORM-E-502`; a shadowed stagger lower bound emits `FORM-E-504`; unshadowed current-module, qualified-module, selective-import, and aggregate/member constants remain accepted.

**Verification accounting:**

- **PASS:** focused checker/HIR suites **72 passed**, **0 failed**, **0 skipped**. Coverage includes exact two-module spawn/kill/pool-for/apply counterexamples, nested reservations, module-shaped locals and future lets, selective imports, record-type operands, no-HIR rejection, canonical accepted HIR targets, the complete loop-bound shadow matrix, positive constant/aggregate projections, apply-duration/player-index probes, and stagger-shape lexical reduction.
- **PASS:** canonical qualified pool native semantics **1 selected passed**, **0 selected failed**; **28** non-matching backend tests were intentionally excluded by the name filter. The generated strict C++ executes canonical spawn, pool-sugar iteration, and kill with the expected observed value and final empty pool.
- **PASS:** complete strict C++ backend suite **29 passed**, **0 failed**, **0 skipped**, including native compilation/execution with `-std=c++17 -O2 -Wall -Wextra -Werror`.
- **PASS:** prior lexical/call blocker matrix remains **23/23 selected tests passed**, **0 selected failures**; **78** non-matching tests were intentionally excluded by the name filter.
- **PASS:** complete compiler **317 passed**, **0 failed**, **0 skipped**. Full npm workspace matrix: compiler **317**, ledger **40**, fixgen **14**, ABI generator **21** — **392 passed total**, **0 failed**, **0 skipped**. Compiler `form:check` verified all **12** artifacts.
- **PASS:** two final complete `form:gen` / `form:check` cycles each wrote and verified all **12** byte-stable artifacts. The sorted golden-relative path, u32 path-length, path bytes, u64 content-length, and content-bytes SHA-256 was identical both times: `7ba9f90c41073a112b384672024897780cc055ed6c636cd7c71548abfbec59e5`.
- **PASS:** no ABI file was touched by this remediation, so no ABI generation was required; the full workspace still passed all **21** ABI-generator tests.
- **PASS:** the commit contains exactly `compiler/src/frontend/checker.ts`, `compiler/src/hir/model.ts`, `compiler/src/hir/lower.ts`, `compiler/src/backends/cpp/emitter.ts`, `compiler/tests/frontend/checker.test.ts`, `compiler/tests/form/hir_zir.test.ts`, `compiler/tests/form/cpp_backend.test.ts`, `compiler/tests/form/golden/hir.json`, and `compiler/tests/form/golden/sim.zir.json`; it has no whitespace errors and carries `Co-Authored-By: Claude <noreply@anthropic.com>`.
- **PASS:** all **43** unrelated dirty/staged/untracked records were preserved exactly. Their length-framed status/index/worktree fingerprint remains `f88eb76ca4bba53e6a96df414d10874fca6d6c6da7c3997acddb6b66a2468685` before and after commit/push.
- **PASS:** `HEAD == origin/main == remote main == ad41d38f493e2bb4ea6f0a15c39605a018bca871`.

This record means **remediated, not accepted**. W3.3 remains stopped pending independent acceptance. **W3.4 has not begun.**

## 2026-08-18 — Nanquan ownership correction and bounded compiler close-out — PROVISIONAL; NO ACCEPTANCE CLAIM

Repository ownership is corrected: **Nanquan is the canonical language/compiler repository** at `C:\programmieren\zencrifice\nanquan` (`https://github.com/Fabulu/nanquan.git`). The compiler/spec snapshot formerly living under Zhaozhou was misplaced provisional language work, not a Zhaozhou-owned or accepted language baseline. It was imported only from committed Zhaozhou Git objects at `77b60057b21f4aedd668e57c6ace37095d9aee70`; no dirty Zhaozhou working-tree file was used. Legacy internal `Form` names remain explicit technical debt rather than triggering a rename campaign.

Three logical Nanquan commits are pushed to `origin/main`:

1. `97b5e51b883662955e6cdcde924b0c362e815f35` — `Import provisional Nanquan compiler snapshot`
2. `af350eb169b878e373d57ac55c17a6eac70110cc` — `Close portable C++ runtime arithmetic`
3. `d1048f8f9c46523882945e2839c2e8efebbdc80d` — `Make Nanquan artifacts cross-platform stable`

The bounded close-out replaces generated-runtime `__int128` arithmetic with portable ISO C++17 `U128` hi/lo and signed-magnitude `S128` operations, including exact 64x64 multiply, add/subtract, shifts, restoring 128-by-64 division, floor and round-half-up division, saturation, and 128-bit integer square root. Fixed-point, RNG, vector dot/length/normalization, narrowing and signed-mix paths use that runtime. Signed minima emit valid expressions such as `(-9223372036854775807LL - 1LL)` and `(-2147483647 - 1)`, never the ill-formed `-9223372036854775808LL`. The emitter and generated Nanquan runtime contain no `__int128`; remaining occurrences are confined to imported Zhaozhou target-SDK test support under `tests/support/zhaozhou` and are not compiler-emitted runtime code.

Nanquan is independently reproducible through `package-lock.json`, `nanquan:gen`, `nanquan:check`, LF checkout policy, twelve committed artifacts, minimal GitHub Actions, and self-contained test fixtures/support. The final Windows and clean Linux results agree, including the canonical state/hash anchor `d981e8e2 65c1abfe 07e322c7`.

**Verification accounting:**

- **PASS:** imported focused compiler integration matrix **66 passed**, **0 failed**, **0 skipped**.
- **PASS:** complete strict C++ backend suite **31 passed**, **0 failed**, **0 skipped**, including generated-only compilation with `-std=c++17 -pedantic-errors -Wall -Wextra -Werror`.
- **PASS:** bounded portable-wide BigInt differential regression **1 selected passed**, **0 selected failed**; signed-minimum emission/strict-compilation regression **1 selected passed**, **0 selected failed**.
- **PASS:** final complete Nanquan `npm test`: **324 passed**, **0 failed**, **0 skipped**; `nanquan:check` verified all **12** byte-stable artifacts.
- **PASS:** two final `nanquan:gen` / `nanquan:check` cycles each wrote and verified all **12** artifacts. The SHA-256 over the Git-ordered per-file SHA-256 listing was identical both times and remains `7ad2d8bef45bb1fd109ae2ff3c731e417f71ae694bf1524fee40d8ddb80c9cd2`.
- **PASS:** GitHub Actions run `32073320022` completed green: `npm ci`, `npm test`, and `npm run nanquan:check` all passed.
- **Compiler availability:** WinLibs GCC `16.1.0` at `C:\programmieren\dsstuff\mingw64\bin\g++.exe` was used. `clang++` and MSVC `cl` were not available on `PATH`; nothing was installed.
- **PASS:** Nanquan is clean and `HEAD == origin/main == d1048f8f9c46523882945e2839c2e8efebbdc80d`.
- **PASS:** Zhaozhou remains untouched at `HEAD == origin/main == remote main == 77b60057b21f4aedd668e57c6ace37095d9aee70`. All **43** unrelated dirty/staged/untracked records remain byte-for-byte identical; their length-framed status/index/worktree fingerprint remains `f88eb76ca4bba53e6a96df414d10874fca6d6c6da7c3997acddb6b66a2468685`.

This is a **provisional remediation record, not an acceptance claim**. W3.3 remains pending independent acceptance; **W3.4 has not begun**. Compiler/language work stops here. Any future language/compiler work belongs only in Nanquan, not Zhaozhou; no hardware, gallery, migration, rename, feature, semantic-audit, or acceptance-review work was started.
