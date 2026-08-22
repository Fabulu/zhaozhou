# WAVE 3 PLAN v1 — Phase 3 Software Console + Minimal Form Language

*Architect consolidation, 2026-08-15. Inputs: charter v0.2 (§3, §6, §6A, §19, §20, §23-P3, §24, §26, §29), FORM_LANGUAGE_HARDWARE_CODESIGN.md (§4–§6, §8–§16, §18-L1, §20, §22), wave-1 + wave-2 PLANs, repo state. Persisted by orchestrator; all paths relative to `C:\programmieren\zencrifice\zhaozhou\`.*

**Ground rules:** truth/presentation separated by compiler checks (§29-8); no host floats in deterministic paths — qformats single-rounding law only (§29-7); one Field IR semantics — L1 field code lowers through the FROZEN field-ir pipeline, never a second path (§29-6); generated code committed and byte-stable; source IDs and costs first-class; language deliberately small — L1 non-goals refused, not deferred; maturity with commit-pinned evidence; banner-append only; root package.json scripts-block only. **Machine fact:** `C:\devkitpro\devkitA64\bin\aarch64-none-elf-g++.exe` 15.2.0 EXISTS — the ARM target is real.

## 1. Ratified decisions (12)

| # | Decision |
|---|---|
| **D1** | **Form L1 grammar scope.** Syntax per FORM §4–§13 illustrations. IN: modules+import; const; enum; struct; `pool T: Struct[cap]` capacity literal; `system name every N ticks reads..writes..`; functions (pure unless in system body); single-assignment `let`; expressions over fx16/fx24/angle16/unit8/i32/u32/bool + world2/3/velocity3/colour8; bounded `for` ascending-index only; `if` as select-expression; `random.stream(seed, id...)`; `@earth/@flow field` declarations; pure `presentation` blocks; `scenario` blocks; `sound` as tone-event declaration only. OUT until L2+ (refused with error codes): forms/ladders (L3), terrain_material grammar (L4), build/warp/formation/stamp domains, macros, generics beyond capacity literals, OOP/closures/strings/first-class functions/pointers/while/recursion/host FFI. |
| **D2** | **Parser + errors.** Hand-written recursive-descent + Pratt in TS, zero deps (abi-gen lexer pattern). Every AST node carries SourceSpan (same type the Field IR builder consumes). Structured diagnostics `{file, span, code "FORM-E-nnn", message}` collected never thrown; non-zero exit on any error; no partial emission. Warnings never affect semantics. |
| **D3** | **HIR/ZIR + Field IR boundary.** HIR = resolved, typed, domain-tagged AST (`compiler/src/hir/`). ZIR = domain-partitioned (`compiler/src/zir/`): SimZIR (phase-scheduled systems over SoA state tables), PresentZIR (pure emit trees → command templates), TestZIR (deterministic scripts). Everything inside a `field` declaration is type-checked in the **field dialect** (branchless, loopless, no state access, record→record, max_ops ≤ ceiling) and lowers to calls on the EXISTING frozen FieldBuilder → .zprog + C++ wrapper + .zvec goldens. `spec/form/field-ir.md` is NOT edited. |
| **D4** | **C++17 backend.** One .hpp/.cpp per module under `<out>/generated/form/`, `namespace form::<module>`; cartridge-wide `form_game.hpp`: `FormState` (SoA pools + globals + RNG states), `sim_tick(FormState&, const PadFrame[4], u32 tick)`, `present_frame(const FormState&, zref::FrameBuilder&)`, `sim_hash`, scenario entries. Links against ZRef runtime only (fixp/trig, zfield interpreter, zhao_abi.h builder, .zcap writer). No `<cmath>`, no float/double token in emitted files (grep-audited). Byte-stable emission (fixed order, no timestamps, LF, version banner). |
| **D5** | **Simulation hash.** Canonical truth-state serialization (declaration order, dense pool index order, fixed-width LE, no padding). `H_t = CRC-32C(H_{t−1} ‖ canonical_state_t)`; `H_0 = CRC-32C(program_manifest_hash ‖ cartridge_hash)`. 600-tick golden hash chains per scenario. Frame CRCs reuse wave-2 displayed-stream law. Cross-target equality by construction. |
| **D6** | **Deterministic scheduler.** Compile-time schedule, zero runtime decisions: systems declare reads/writes; compiler orders into phases topologically; **rejects >1 writer per state component per phase** (error cites both spans). Multi-rate `tick % N == 0`; stagger `(entity_id % N == tick % N)`. Ascending pool/array iteration always. Emitted sim_tick = flat phase-ordered call list. Expands spec/form/deterministic-scheduling.md. |
| **D7** | **Software renderer + Phase-3 draw subset.** `zref::render` — exact, slow, integer-only, CRC-able. ABI v3 promotes reserved→implemented: TerrainField 0x0200 (earth .zprog over heightfield footprint via zfield interpreter), SurfaceStamp 0x0210 (circle/ring into 64×64 sheet — the scar response), DrawForm 0x0300 (marker/billboard quads — the wizards), DrawPopulation 0x0301 (point/triangle particle sprites), DrawProcedural 0x0302 (forge kind heightfield_patch — the island), EmitAudioEvent 0x0400 (wave-2 mixer tone). DrawSky 0x0310 renders through **zref::sky::emit_layers** per spec/sky_and_beams.md. Raster: fixed-point projection with mat4fx, painter's algorithm with Q16.16 1/z depth per view, flat integer shading, ordered-dither RGB565 resolve, per-frame canvas CRC. Two 256×192 views, Duo canvas map per video_rules.md. |
| **D8** | **Cartridge .zpak.** New spec/cartridge.md; container reuses .zcap section discipline verbatim (magic ZPAK, u32 version, u64 lengths, CRC-32C per section + trailer). Resource pages reuse the exact .zcap RESOURCE_PAGES record shape; kinds = field programs, sourceids.zmap, generated-code manifest, sky set, terrain patch, tone bank. Packer: tools/pack (TS, zero deps, deterministic, pack:check staleness gate). |
| **D9** | **ARM target — real, local.** Cross-compile smoke via devkitA64: compile the deterministic core with `aarch64-none-elf-g++ -std=c++17 -fno-exceptions -fno-rtti -ffreestanding -c` + archive, wired as CTest `arm-cross` label (SKIP-if-absent machine probe). Optional CI job on ubuntu-24.04-arm64 compiling natively AND running the sim-hash golden test. Real HPS run stays hardware lane; nothing unexecuted is claimed. |
| **D10** | **ZEmu.** Loads .zpak, instantiates the generated game, runs the software console: tick = pad snapshot (scenario stream or live) → sim_tick → present_frame → sealed packet → zref::render → RGB565 canvas + CRC → optional PCM window. Deterministic, wall-clock-independent by default; `--realtime` pacing never feeds timing into truth. Writes .zcap replayable by tools/capture. |
| **D11** | **Source maps + costs.zcost.** sourceids.zmap: binary (magic ZSMP, CRC-32C), `source_id → {kind; file_index; span; name}`; kinds extend capture_format §5/§6 (add 5=system, 6=presentation emit site, 7=pool, 8=scenario). costs.zcost: canonical JSON (sorted keys, no whitespace, LF, byte-stable) with schema in spec/form/cost-model.md: pool capacities, per-system phase/rate, per-field instruction counts + cost classes + footprints, command-memory ceiling, particle-state bandwidth, source-ID attribution. |
| **D12** | **Maturity targets.** SW.COMPILER.FORM → UNIT_VERIFIED; SW.FIELDIR → UNIT_VERIFIED (on top of REFERENCE_COMPLETE); SW.ZEMU → REFERENCE_COMPLETE; SW.TOOLS.ASSET → REFERENCE_COMPLETE (packer subset); SW.RUNTIME.HPS → REFERENCE_COMPLETE (deterministic core + cross evidence; real run = hardware lane); SW.MIXER unchanged; no FPGA block advances. |

## 2. Work packages

### W3.1 — Spec wave, ABI v3, format freeze *(L; serial first)*
Expand the four Phase-1 stubs to L1 v1 (language-semantics.md full grammar+types+field dialect+error catalog; domains-and-effects.md effect contracts + present purity law + earth/flow admission; deterministic-scheduling.md D6; cost-model.md D11 schema + budget registry). spec/form/field-ir.md NOT touched. spec/cartridge.md (D8). capture_format.md §6 extension (D11). commands.zidl: promote D7 subset reserved→implemented + struct additions (rebased on ABI v2 post-W2.1-merge); regenerate. Fill/refresh contracts SW.COMPILER.FORM, SW.ZEMU, SW.TOOLS.ASSET, SW.RUNTIME.HPS. Ledger notes only.
**Acceptance:** abi:check + ledger:check + ledger:gen byte-identical; D7 records in spec/generated/abi.md; zero TODOs; D1 IN/OUT reviewed vs charter §26.
**Owns:** spec/form/{language-semantics,domains-and-effects,deterministic-scheduling,cost-model}.md, spec/cartridge.md, spec/capture_format.md, spec/commands.zidl + regen, 4 contracts.

### W3.2 — Form frontend *(L; owns compiler/src/frontend/)*
Lexer (byte-offset spans, CRLF-tolerant), parser (recursive-descent + Pratt), AST with spans. Type checker: domains as function kinds; capacity literals; one-writer phase analysis; present-purity; field-dialect admission; scenario checking. Diagnostics D2 with full FORM-E catalog; golden corpus positive+negative (every error code exercised); AST goldens committed.
**Acceptance:** ≥1 test per error code; D1 OUT-features rejected with correct codes; AST goldens byte-stable.

### W3.3 — HIR/ZIR, scheduler, backend, maps, costs *(L; owns compiler/src/{hir,zir,backends}/)*
HIR resolution → ZIR partitioning → phase scheduler → C++ emitter (D4/D5) → sourceids.zmap + costs.zcost emitters. Emitter byte-stability tests (run twice, diff zero); grep-audit no float/`<cmath>`/host-clock. ZIR goldens. Scheduler tests: conflicting writes rejected with both spans; schedule pure function of declaration order + deps; multi-rate/stagger goldens. form:gen/form:check npm scripts (scripts block only).
**Acceptance:** goldens byte-stable; determinism green; costs.zcost schema-valid (Wound Lab numbers once W3.7 exists).

### W3.4 — Field dialect + Earth/Flow programs *(M; owns compiler/src/field_dialect/)*
L1 @earth/@flow bodies → FieldBuilder programs (D3 boundary): validator-backed lowering, max_ops→ceilings, footprints, parameter packing into frozen I/O records. Deliver: rising_ridge (earth), travelling_wave (earth), crater_break (earth), shatter_storm (flow) — each with committed .zprog, C++ wrapper, C++-oracle golden .zvec, TS differential (wave-1 discipline). No frozen-ISA edits.
**Acceptance:** four programs validate under the frozen validator; goldens byte-stable; TS↔C++ green; instruction counts within ceilings recorded.

### W3.5 — ZRef renderer + sky + audio-event path *(L; owns reference/src/zrender/, zsky/)*
Implements D7 over a sealed frame packet: heightfield patch projection + painter/1-z depth (island), TerrainField via zfield interpreter, surface-sheet stamping + sheet shading (scar response), marker quads, particle sprites, zref::sky::emit_layers per spec/sky_and_beams.md (fallback flat clear — the spec's own rule), EmitAudioEvent → MixerTone, Duo dual-view, ordered-dither resolve, canvas CRC. Integer-only, spec-cited. Directed per command (hand-computed projections) + differential vs naive per-pixel oracle where cheap + CRC goldens.
**Acceptance:** directed green; synthetic 2-view frame → committed golden canvas CRC; no floats; 600-frame island sequence < ~60 s.

### W3.6 — Cartridge packer + ZEmu *(L; owns tools/pack/, emulator/, runtime/)*
tools/pack: module set → compiler build → .zpak deterministic + pack:check gate. ZEmu (D10): .zpak load, console loop, capture writer, tools/capture replay, --realtime optional. runtime/mister = build-config skeleton only (hardware lane). Tests: pack→load round-trip byte-stable; ZEmu capture replays identically; run-twice-identical hashes.
**Acceptance:** `zemu --cart wound_lab.zpak --ticks 600` identical hash chain + frame CRCs across runs; pack:check green.

### W3.7 — Wound Lab Phase-3 slice *(L; owns demos/wound_lab/form/**, generated/**, captures/golden/wave3/, tests/e2e/)*
The first real Form program (FORM §22): one floating island (authored heightfield + TerrainField application), two player wizards (pad-driven, marker-law lineage), two 256×192 cameras, two PCG pad streams, four spells — Raise = rising_ridge + crack-ring SurfaceStamp; Wave = travelling_wave; Break = crater_break + scorch stamp; Shatter = shatter_storm flow population — one scar response, one particle population, declarative present duo block, scenario opposing_waves (seed 0x5A17, casts at 120t, capture frame 150, budget assert). Generated C++ committed byte-stable. Deliverables: 600-tick sim-hash chain golden, per-frame CRCs, .zcap + .zpak, costs.zcost + sourceids.zmap, executable ctest comparing every golden.
**Acceptance (Phase-3 gate as tests):** desktop green + ARM cross-compile clean (+ ARM64 CI run if enabled); two-player input + audio tones + software island at deterministic 60 Hz semantics; truth/presentation enforced (negative test: present-block write rejected); field+scar+population live in software; capture/replay + hashes stable; source IDs + costs emitted.

### W3.8 — ARM cross, CI, maturity, status *(M)*
D9 executed (machine probe, CTest arm-cross freestanding compile+archive; optional ARM64 CI native run, SKIP-with-notice). D12 maturity moves with commit-pinned evidence. reports/status/phase3_wave3.md + blocked-on-hardware update. Final clean-clone full-suite pass.

## 3. Sequencing + ownership

```
W3.1 (after W2.1 merges to main — ABI v2 base)
  ├─ W3.2 frontend  (A; needs grammar spec)
  ├─ W3.5 renderer  (B; needs command set)
  ├─ W3.4 field dialect (C; uses existing builder)
  │   after W3.2: W3.3 HIR/ZIR+backend (D)
  │              W3.6 packer+ZEmu (E; after W3.1)
W3.7 (needs W3.3+W3.4+W3.5+W3.6) → W3.8 (needs all)
```
Hard ownership: wave-3 never touches fpga/**, sim/models/**, tests/{video,input,audio,memory,command,debug,formal}/**, spec/{video,input,audio,memory,counters}_rules.md, spec/sky_and_beams.md (implement only), demos/wound_lab/duo_markers*, captures/golden/wave2/**, zhao_pkg.sv, compiler/src/field_ir/**, reference/src/zfield/** (call only). commands.zidl solely W3.1 (post-merge); field-ir.md frozen. blocks.yml: own D12 entries only. Merge order W3.1→W3.2→W3.4→W3.5→W3.3→W3.6→W3.7→W3.8; abi drift fixed by abi:gen in the causing commit.

## 4. Test strategy
Compiler units (goldens, one test per FORM-E code, purity/one-writer rejections, ZIR/schedule goldens, emitter byte-stability, no-float grep) · backend golden snapshots CI-regenerated · scheduler determinism (seeded 600-tick hash chains) · field dialect (validator acceptance, .zvec goldens, TS differential, ceiling assertions) · renderer (directed hand-computed vectors, canvas CRC goldens, naive-oracle differential) · e2e (packed demo 600 ticks vs committed goldens, capture replay, run-twice-identical, named terrain-sample differential) · costs schema + Wound Lab assertions · ARM cross label + optional ARM64 native CI. Labels: fast (units + e2e ≤60 s), nightly (full fuzz + soak e2e), arm-cross (SKIP-if-absent).

## 5. Deferred + markings
Real HPS execution / MiSTer build → SW.RUNTIME.HPS hardware lane beyond REFERENCE_COMPLETE · forms/ladders/pixel-error language concepts → L3 · terrain_material grammar/scar tables/sample banks/glTF assets → L4 / packer-subset · build/warp/formation/stamp domains → refused per D1 · full mixer + beams RTL → wave-2 state / Phase 11 · hardware field lowering + ZDL backend → untouched, progressive lowering later · non-goals → refused, error-coded.

## 6. Git strategy
1 spec(w3) · 2 abi v3 + regen · 3 frontend · 4 field-dialect · 5 zref renderer+sky · 6 compiler HIR/ZIR+backend · 7 pack+zemu · 8 demo · 9 arm+ci+ledger+status. `Co-Authored-By: Claude <noreply@anthropic.com>`; drift fixed in causing commit; no merge without abi:check/form:check/pack:check green.

## 7. Risks
R1 commands.zidl collision with wave-2 → W3.1 sole owner post-merge, additive wire-compatible promotions, abi:check every merge · R2 golden churn → version banner + deterministic order from day one; regen owned by causing commit · R3 grammar creep → D1 IN/OUT is spec law with error codes · R4 renderer speed → explicit runtime budget test; failure = optimize integer path, never loosen exactness · R5 devkitA64 freestanding edges → core is I/O-free by construction; compile -c + archive locally; CI ARM64 native run is the execution evidence; both SKIP-with-notice · R6 wave-2 drift (canvas map, PadFrame, DrawSky) → W3.5 implements wave-2's own specs, never parallels.

**Definition of done** — clean clone: ctest -L fast 100%; npm ci && all checks (ledger/abi/tables/pack/form) && npm test 100%; ctest -L arm-cross green; Phase-3 gate fully mapped to named passing tests + committed artifacts in reports/status/phase3_wave3.md; every D12 move commit-pinned.
