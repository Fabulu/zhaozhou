# FINDINGS — P2 Design Ledger (design/blocks.yml + design/ops.yml) — ZH-001 recon

*Author: P2 recon agent, 2026-08-14. Persisted by orchestrator (subagent file-write blocked by harness policy).*

**Sources:** Charter v0.2 (§4, §5, §6A, §11.3, §19, §22, §25, §27, §28), FORM_LANGUAGE_HARDWARE_CODESIGN.md (§2, §9, §14, §19), online recon (sources at end). Environment verified: Node 20.17.0, npm 10.8.2, Python 3.14.0, PyYAML 6.0.2. Repo `zhaozhou/` is scaffolding only (constraints/, docs/SPEC.md, empty rtl/sim/tools) — nothing exists yet, so **all blocks start at SPECIFIED**.

## Executive summary

- **83 blocks proposed: 69 FPGA (RTL) + 14 software.** Every charter §5 diagram node decomposes into named blocks; every `fpga/rtl/*` dir in §22 gets ≥1 block.
- **39 ops proposed: 28 shared Field-IR ALU ops + 6 profile sinks + 5 stamp blend modes.**
- **Key decisions:** (1) ledger carries architecture *edges* (`upstream`/`downstream`) so the §5 diagram regenerates from data — "the ledger is the authoritative console schematic"; (2) maturity ordering is temporal → JSON Schema validates shape, a TS validator checks advancement against git history; (3) ZRef functions are NOT blocks — they're values of a block's `reference_model`; ZRef/ZEmu as products are software blocks; (4) software blocks use a reduced ladder (stop at INTEGRATED unless `runs_on_target_hardware: true`); (5) generator in Node/TypeScript per §27.
- Biggest risks: resource budgets and clock frequencies are Phase-0 data; shared-vs-separate field sequencers unresolved until synthesis; op Q-formats depend on ZH-012.

## 1. Block inventory (conventions)

- ID `GROUP.NAME`; group prefixes match §22 rtl dirs: SYS, CMD, MEM, FIELD, INPUT, AUDIO, VIDEO, MEASURE, TERRAIN, SURFACE, GEOM, RASTER, TEXTURE, PART, FORGE, TWOD, POST, DEBUG; software `SW.<product>.<name>`.
- Clock domains (relative only; absolute frozen post-Phase-0): `hps`, `gpu` (charter §4 example), `sdram`, `video`, `audio`, `async` (CDC bridges).
- All entries: maturity `SPECIFIED`, plus `phase` (first §23 build phase) and `owner_issue` (ZH-0xx link — OpenTitan pattern).

### FPGA blocks (69)

**SYS (3):** SYS.PLL (vendor PLL wrappers, async, P0) · SYS.RESET (reset sequencer; formal reset-reaches-idle, async, P0) · SYS.CDC (ready/valid CDC FIFO library, async, P0).

**CMD (3):** CMD.DMA (fetch sealed frame packets from HPS-DDR ring; CRC+epoch, gpu, P1) · CMD.DECODER (validate/dispatch semantic commands; malformed→safe error, gpu, P1) · CMD.SCHEDULER (frame-slot ownership FSM FREE→…→DONE, deadline enforcement + fault counter, completion fence, gpu, P1).

**MEM (4):** MEM.SDRAM (local 128MB controller, sdram, P2) · MEM.VRAM.ARBITER (guaranteed-client liveness, formal, gpu, P2) · MEM.HPS.BRIDGE (framework DDR burst client, gpu, P1) · MEM.GUARD (region/ownership checker — malformed commands cannot write outside assigned memory, formal, gpu, P1).

**FIELD (6):** FIELD.SEQ.EARTH (Earth8 patch evaluator, gpu, P7/ZH-035) · FIELD.SEQ.WARP (Warp8 vertex deformation, gpu, P9+/ZH-045, deferred) · FIELD.SEQ.FLOW (particle/force field, gpu, P10/ZH-042) · FIELD.SEQ.FORMATION (transform generation, gpu, P9/ZH-041) · FIELD.SEQ.STAMP (Scar Scribe sheet ops, gpu, P6+/ZH-045) · FIELD.PROGCACHE (microprogram+constant cache, hash/version check, safe rejection — §19.4, gpu, P7). *Planning split; §6A allows merging/ALU-sharing after Phase 0 — ledger needs `superseded_by`.*

**INPUT (2+1 deferred):** INPUT.SNAPSHOT (pad snapshot at frame boundary + sequence → canonical PadFrame, async, P2/ZH-017) · INPUT.RUMBLE (rumble out, async, P2) · INPUT.SNAC (optional PS1/SNAC adapter, deferred).

**AUDIO (1):** AUDIO.FIFO (PCM ring reader + async FIFO into framework audio clock, underrun repeats/fades, sample+underrun counters, optional tone, audio/gpu crossing, P2/ZH-018).

**VIDEO (4):** VIDEO.MODE (Z60/Storm/Duo timing, video, P2/ZH-016) · VIDEO.SCANOUT (double-buffered scanout, line buffers, frame repeat on missed deadline, video, P2) · VIDEO.SCALER (scaler feed, video, P2) · VIDEO.FRAMECTL (frame-complete/repeat handshake with CMD.SCHEDULER, video async, P2).

**MEASURE (3):** MEASURE.GOVERNOR (per-camera threshold traversal; hysteresis/min-hold/geomorph, gpu, P8) · MEASURE.TOKENS (global geometry/fragment token guard; Duo 45/45/10 fairness, gpu, P8) · MEASURE.HISTOGRAM (error-bucket histogram + cutoff, "Version 2", gpu, P8+).

**TERRAIN/Mantle (7):** TERRAIN.PATCH (state layers + bounded field-list intake with bake/compose/reject on overflow §11.4, gpu, P6-7) · TERRAIN.TESS (subpatch crack-safe resolution + stitch + geomorph, gpu, P6) · TERRAIN.NORMALS (deformed normals, gpu, P6) · TERRAIN.VELOCITY (height velocity for gameplay, gpu, P7) · TERRAIN.BAKE (persistent scar bake, gpu, P7/ZH-036) · TERRAIN.LOD (projected-error LOD + deformed-height cache built once, gpu, P6) · TERRAIN.PROJECT (dual-view projection of shared cache, gpu, P6). Wounds/sparse volumetrics deferred (§11.7), not registered.

**SURFACE/Scar Scribe (2):** SURFACE.SHEET (64×64/patch tag+strength storage, residency, gpu, P6/ZH-031) · SURFACE.STAMP (deterministic stamp engine: circle/ring/spline/brush, material conversion, age/decay, capture-exact, gpu, P6).

**GEOM/Ten Thousand Forms (10):** GEOM.MESHFETCH (descriptor fetch + cull + LOD decision, gpu, P8/ZH-037) · GEOM.VDECODE (compressed vertex fetch+decode, gpu, P8) · GEOM.SKIN (rigid + two-weight skinning, gpu, P9) · GEOM.LOOM (Transform Loom bounded graph: orbit/aim/billboard/oscillator/spline/gait/formations, gpu, P9/ZH-041) · GEOM.WARP (Warp8 application, gpu, P9+/ZH-045) · GEOM.WCACHE (world-space/post-transform cache — dual-view sharing point, gpu, P8/ZH-040) · GEOM.PROJECT (camera 0/1 projection + lighting, gpu, P8) · GEOM.CLIP (near+guard-band clip, backface cull, scissor/viewport, gpu, P5) · GEOM.SETUP (screen-space triangle setup, gpu, P5) · GEOM.BINNER (tile binner + chunked lists with safe overflow, formal arena bounds, gpu, P5).

**RASTER (5):** RASTER.TILESTORE (ping-pong 16×16 tile RAM, 64bpp, clear, gpu, P4/ZH-021) · RASTER.EDGEWALK (edge walker + exact top-left coverage, gpu, P4/ZH-022/023) · RASTER.FRAGMENT (depth/stencil/blend/fog — the §4 example block, gpu, P4-5/ZH-025) · RASTER.EARLYZ (early-Z reject + coarse transparent depth bins, gpu, P5) · RASTER.RESOLVE (ordered-dither RGB565 resolve + tile CRC handoff, gpu, P4/ZH-024).

**TEXTURE (4):** TEXTURE.TMU (primary TMU: nearest/bilinear/mip/CLUT/direct/wrap modes, gpu, P5/ZH-027/028) · TEXTURE.AUX (restricted aux source: sheet/light/shadow-compare/distortion — never a second TMU, gpu, P6) · TEXTURE.CACHE (M10K texture/palette/material cache + tags + counters, gpu, P5) · TEXTURE.MOSAIC (terrain Mosaic selector: stable pattern picks candidate, one primary sample, gpu, P6/ZH-030).

**PART/Myriad (7):** PART.STATE (128-bit state streaming from HPS DDR, gpu, P10/ZH-042) · PART.UPDATE (fixed recipes: integrate/gravity/drag/attract/orbit/vortex/wind/shockwave/spline-flow, gpu, P10) · PART.COLLIDE (plane+heightfield collision following live terrain; bounce/slide/stick/die, gpu, P10) · PART.SPAWN (deterministic child spawn, gpu, P10) · PART.LADDER (representation ladder by projected size/budget, gpu, P10/ZH-043) · PART.EXPAND (polygon-particle instancing → normal geometry packets, gpu, P10) · PART.SOFT (soft sprite/streak endpoint, gpu, P10).

**FORGE (2):** FORGE.PRIM (ribbon/tube/radial shell/ring/chain/shard burst/billboard sheet/spline wall/cone; bounded subdivision + screen-error LOD, gpu, P11/ZH-044) · FORGE.CLIFF (terrain cliffs/skirts, gpu, P6).

**TWOD (2):** TWOD.PLANE (two scanout/tile-aware planes: affine/scroll, sky, water/lava, fog sheet, world-space depth plane, gpu, P11) · TWOD.SPRITE (HUD overlay: descriptors, affine, CLUT+direct, text, windows, heat maps; after glow/distortion, gpu, P11).

**POST/Mirror Gate (3):** POST.GATHER (low-res glow/distortion-XY/outline accumulation, gpu, P11/ZH-046) · POST.COMPOSITE (bloom/haze/shockwave/refraction/grading/palette/flash, gpu, P11) · POST.ECHO (optional frame echo, `cut_order: 1` — first in §26 cut order, deferred).

**DEBUG (3):** DEBUG.COUNTERS (§25 mandatory counter aggregation; bytes by client, gpu, P1) · DEBUG.CRC (tile/frame CRC, gpu, P4) · DEBUG.TRACE (selectable trace ring §20.6 + source-ID propagation, gpu, P1).

### Software blocks (14)

Game runtime is content, excluded. **SW.RUNTIME.HPS** (HPS runtime glue, P1-3) · **SW.MIXER** (48kHz fixed-point 32-voice 3-bus mixer, same code in ZRef/ZEmu/hw, P2) · **SW.CMDBUILD** (semantic frame-packet builder + epochs + seal/CRC, P1) · **SW.CPUCOLL** (terrain collision, canonical scars, navigation from exact field semantics, P7/ZH-036) · **SW.STREAM** (streaming/asset prep, P3+) · **SW.ZREF** (scalar oracle library — host of all `reference_model` symbols, P1/ZH-015) · **SW.ZEMU** (desktop emulator, P1-3) · **SW.COMPILER.FORM** (TS compiler frontend→HIR→ZIR→backends, P1) · **SW.FIELDIR** (exact Field IR evaluator/serializer/vectors/validity checker, P1/ZH-010/011) · **SW.TOOLS.LEDGER** (**this piece** — validator + generators, P1/ZH-001) · **SW.TOOLS.ABIDOC** (IDL generator spec/*.zidl → sv/h/ts+docs+fuzz, P1/ZH-013) · **SW.TOOLS.ASSET** (meshlets/LODs/microforms/textures/packers, P3/12) · **SW.TOOLS.REPORT** (Quartus report parser → feeds `resource_actual`, P0/ZH-002) · **SW.TOOLS.BOARDPROBE** (Phase 0 probes → board_truth.json, P0/ZH-003…006). (SW.TOOLS.CAPTURE may be added or folded into ZEMU — architect's call.)

**ZRef functions: NOT blocks.** The charter's own §4 example puts `reference_model: zref::FragmentPipeline` inside the block; §20.3's differential unit is block↔reference↔RTL 1:1; registering ~70 extra "reference blocks" adds no validation rule. SW.ZREF the product is one block so the oracle has an owner and maturity.

**Non-blocks:** memory pools are budgets → `design/budgets/*.yml`, referenced via block `memory:` field; generated ABI packages are artifacts (`kind: generated`), no maturity.

## 2. Ops inventory (`design/ops.yml`)

Profiles (frozen five, §6A): **E**=earth, **W**=warp, **F**=flow, **M**=formation, **S**=stamp. Cost = provisional instruction slots (feeds FORM §14 `costs.zcost`); Q formats are *pointers* to spec/qformats.md (ZH-012, unfrozen). All reference functions are `zref::fieldir::op_*`.

**28 shared ALU ops:** FIELD.MOV (EWFMS,1) · ADD (EWFMS,1, saturating) · SUB (EWFMS,1) · MUL (EWFMS,1, rounding per qformats) · MAD (EWFMS,1, fused single rounding) · MIN (EWFMS,1) · MAX (EWFMS,1) · ABS (EWFMS,1) · CLAMP (EWFMS,1) · SELECT (EWFMS,1, branchless) · CMP (EWFMS,1, unit8 predicate) · DOT2 (E W F M,2) · DOT3 (W F M,2) · NORM.APPROX (W F M,3, table-based, never exact rsqrt) · SIN (E W F M,2, sine table §7.3) · COS (W F M,2) · CURVE (E F M,1, phase/envelope sample) · NOISE2 (E F S,2, low-res deterministic) · LEN.APPROX (E W F M,2) · DIST.APPROX (E W F M,2) · SMOOTHSTEP (E W F M S,2) · RING (E S,2) · RIDGE (E,2) · SAMPLE.SPLINE (E M,2) · SAMPLE.CURVE (E F M,1) · ROT2 (F M S,2) · ROT3 (W M,3) · **SMOOTH.D (E,1 — proposed addition**: derivative of CURVE phase; Earth8's velocity = envelope × derivative needs an explicit op; architect must ratify or fold into CURVE).

**6 profile sinks:** FIELD.OUT.HEIGHT (E) · FIELD.OUT.VELOCITY (E, "height-velocity output") · FIELD.WRITE.MATERIAL (E, "material-state write", 2 candidate IDs + weight) · FIELD.WRITE.NAV (E) · FIELD.WRITE.TAG (S, sheet tag/strength) · FIELD.WRITE.HAZARD (E, §11.2 layer 6).

**5 stamp blend modes:** FIELD.STAMP.MAX / ADD / SUB / REPLACE / AGE (all S; §12 stamp ops max/add/subtract/replace/age-decay).

Earth8 §11.3 maps 1:1 onto the shared list (add/multiply→ADD/MUL/MAD, min/max, abs, approx distance→DIST.APPROX, smoothstep, ring, ridge, spline distance→SAMPLE.SPLINE, sine→SIN, low-res noise→NOISE2) + two sinks — confirming FORM §9's "no private semantics" claim. Other Form domains (sim/present/build/audio/test) never appear in ops.yml — compiler concerns, not hardware ops.

## 3. Schema sketches

**blocks.yml fields:** `schema_version`; per block: `id` (unique, regex), `name`, `kind: rtl|software`, `subsystem` (must match fpga/rtl dir or "sw"), `clock_domain` (hps|gpu|sdram|video|audio|async), `purpose`, `contract` (path to §4 human-readable contract .md), `phase`, `owner_issue`, `inputs[]`, `outputs[]`, **`upstream[]`/`downstream[]` (the schematic edges — the §5 diagram regenerates from these)**, `backpressure: ready_valid|none|credit`, `latency: fixed:<n>|variable|variable_bounded:<n>`, `target_throughput`, `reference_model` (zref:: symbol), `implementation` (sv path, optional until RTL starts), `memory[]` (pool refs into design/budgets/), `resource_budget` {alm/dsp/m10k_percent, budget_group keying §25 table}, `resource_actual` {filled by tools/report from reports/synthesis/*.json — never hand-typed}, `tests` {directed, random, formal?, captures[]}, `counters[]`, `source_ids: true`, `maturity`, `maturity_log[]` {state, date, commit, evidence}, `notes`, `cut_order`, `deferred`, `superseded_by` (recommended for sequencer merges). Software blocks: drop resource_budget/counters; reduced maturity ladder unless `runs_on_target_hardware: true`.

**ops.yml fields:** `schema_version`; `profiles` (frozen five with input/output records); per op: `id`, `name`, `class: alu|sink|stamp_mode`, `profiles[]` (≥1), `semantics`, `operand_q[]`/`result_q` (pointers to qformats.md), `rounding: saturating|wrapping`, `reference_function` (zref::fieldir:: symbol), `implementation_blocks[]` (must exist in blocks.yml), `cost_units` (>0 for alu), `differential_tests` path, `notes`.

## 4. Generator plan

**Node/TypeScript tool at `tools/ledger`** (charter §27: Node/TS for tools; Python only orchestration). Bonus: Block/Op/Profile TS types become importable by `compiler/src/`, killing a duplicate-definition risk. Stack: `yaml` package (or js-yaml) → parse; **Ajv 8 with JSON Schema draft 2020-12** for structural validation (standard parse-YAML→object→ajv.validate pattern); hand-written TS pass for temporal/relational rules (maturity ordering, cross-refs, git history); Mermaid + Markdown emitters with no runtime deps (Mermaid renders natively in GitHub). CI: `npm run ledger:check` every commit (§27 tier 1); `ledger:gen` writes `design/diagrams/architecture.mmd` + `dashboard.md`; CI fails if regenerated files differ from committed (staleness = failure, same policy as ABI byte-identity). Outputs: (1) architecture.mmd — subgraph per subsystem mirroring §5, node per block, edges from upstream/downstream, **classDef per maturity → the schematic IS the status dashboard**; (2) dashboard.md — maturity matrix, evidence links, budget-group sums vs §25 ceilings, resource_actual deltas, §25 counter coverage, op×profile matrix; (3) later spec/architecture.md block table (P13 "documentation generated from Design Ledger"). Python fallback (PyYAML+jsonschema, verified installed) documented but not recommended.

## 5. Validation rules

V1 id unique/regex; subsystem matches rtl dir. **V2 maturity advances only in order, one step at a time** — new ≤ prev-committed + 1 (read `git show HEAD:design/blocks.yml`); regression needs `regression_reason`. V3 every maturity > SPECIFIED has dated, commit-pinned maturity_log entry with existing evidence path. **V4 RTL blocks must have reference_model + tests.directed + tests.random + counters(≥1) + contract + source_ids:true** (enforced from birth: "no block begins RTL before contract and reference exist"). V5 once SYNTHESIZED: resource_actual present and ≤ budget; group sums ≤ §25 ceilings; **total ALM ≤ 90%** (10% untouchable reserve). V6 all referenced paths exist on disk. V7 diagram edges symmetric and reference existing blocks; every block in ≥1 edge unless `leaf: true`. **V8 any edge crossing clock domains must route via SYS.CDC or a documented async bridge**. V9 op profiles ⊆ frozen five, ≥1 each. **V10 every op names its reference_function** (zref symbol) + existing differential_tests path. V11 implementation_blocks exist; every FIELD.SEQ.* implements ≥1 op. V12 counters extend the §25 allowlist via a declared counter_catalog. V13 software ladder restrictions. V14 schema_version match; regenerated diagrams identical (CI). Evidence mapping per state: SPECIFIED→contract; REFERENCE_COMPLETE→reference+golden vectors; UNIT_VERIFIED→directed+random pass; RTL_VERIFIED→Verilator differential green; SYNTHESIZED→report JSON within ceiling; INTEGRATED→feature-flag + golden corpus; HARDWARE_PROVEN→tile/frame CRC on silicon.

## 6. Risks / architect decisions

1. **Phase-0-dependent fields**: absolute resource budgets, clock frequencies, meshlet limits, program-cache capacity — schema keeps them optional-then-mandatory (V5) so ledger exists day one.
2. **Field sequencer split is provisional** (§6A allows shared ALUs) — add `superseded_by`, expect merges post-synthesis.
3. **FIELD.SMOOTH.D is an addition** (Earth8 velocity gap) — ratify or remove.
4. **Single maturity ladder vs OpenTitan's split D/V tracks** — recommendation: keep the charter-frozen single ladder with evidence distinguishing; conscious ratification requested.
5. **Cut-order field from day one** (POST.ECHO=1…) so the dashboard shows what is expendable.
6. **Over-split candidates** (FRAGMENT vs EARLYZ, TERRAIN.PROJECT vs GEOM.PROJECT, PART.SOFT vs EXPAND) — list errs fine-grained; merging later is a trivial edit, splitting late is not.
7. **ops.yml must not freeze numeric behaviour ahead of ZH-012** — Q formats stay pointers.

## 7. Sources

Ajv/YAML validation: [Stack Overflow](https://stackoverflow.com/questions/65779523/yaml-validation-using-json-schema), [ajv.js.org](https://ajv.js.org/), [ajv-cli](https://ajv.js.org/packages/ajv-cli.html). Mermaid: [mermaid.js.org](https://mermaid.js.org/), [github.com/mermaid-js/mermaid](https://github.com/mermaid-js/mermaid), [Quarto diagrams](https://quarto.org/docs/authoring/diagrams.html). Maturity practice: [OpenTitan Hardware Development Stages](https://opentitan.org/book/doc/project_governance/development_stages.html) (L0-L2/D0-D3/V0-V3/S0-S3 stages, Hjson per-IP metadata, PR-nominated transitions with commit_id), [OTBN checklist](https://opentitan.org/book/hw/ip/otbn/doc/checklist.html), [hw design guidelines](https://opentitan.org/book/doc/contributing/hw_design.html). IP registries: [Arteris IP-XACT guide](https://www.arteris.com/learn/ip-xact/), [Accellera IP-XACT WG](https://www.accellera.org/activities/working-groups/ip-xact), [Design&Reuse IP-XACT value](https://www.design-reuse.com/article/59303-the-value-of-high-quality-ip-xact-xml/), [Synopsys IP-XACT DV environment](https://www.synopsys.com/articles/design-verification-environment.html).
