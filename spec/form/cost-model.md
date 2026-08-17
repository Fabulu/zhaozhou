# Form Cost Model — L1 v1

**Status:** Phase 1 stub expanded to **L1 v1** (wave 3, plan W3.1, decision
D11; FORM §14; charter §23 Phase 3 gate: "a useful `costs.zcost` report").
The Phase-1 mechanics below are retained; this file now fixes (a) the
canonical `costs.zcost` artifact schema, (b) the budget-line registry, and
(c) the per-field cost records the compiler emits.

## 1. Field IR costs (frozen v1 mechanics, provisional numbers)

Per-op classes and provisional cycle/DSP estimates live in
`spec/form/field-ir.md` §2/§9 (ALU/MUL/TABLE/NOISE/SPECIAL). The compiler
emits a static cost report per program: instruction count, per-class counts,
estimated cycles, DSP demand, table bytes, register high-water mark. The
numbers are provisional until the RTL profile engine pins real latencies;
the *mechanics* (class assignment, per-program report) are frozen.

Instruction ceilings per profile: field-ir.md §7.3 (earth 32, warp 48,
flow 48, formation 64, stamp 32; global 64 — provisional, R7).

## 2. `costs.zcost` — canonical artifact schema (D11)

One `costs.zcost` per cartridge build, emitted by the compiler (W3.3) beside
the generated C++ and packed into the .zpak (resource kind
`generated-code manifest` page set, spec/cartridge.md §5).

**Canonicalization law (D11):** UTF-8 JSON, **keys sorted lexicographically**,
**no insignificant whitespace** (no spaces after `:` or `,`), **LF-only**
newlines between top-level values, integers only where the schema says
integer (no floats anywhere — cost is integer by construction), one trailing
LF at EOF. Two builds of the same sources are byte-identical (checked by
`form:check`; the same law as abi-gen, capture_format §6).

### 2.1 Schema

A single top-level object with these members (all required unless marked
optional; `$schema` names this file):

```json
{"$schema":"zhaozhou/spec/form/cost-model.md#2.1",
 "abi_version":2,
 "budgets":[{"line":"sky_triangles","limit":352,"owner":"spec/sky_and_beams.md"}],
 "command_memory":{"ceiling_bytes":1048576,"per_frame_estimate_bytes":0},
 "modules":[{"index":0,"name":"wound_lab"}],
 "particle_bandwidth":{"bytes_per_element":48,"peak_elements":8192,
                       "bytes_per_tick":393216,"pools":["shards"]},
 "pools":[{"capacity":8192,"element_bytes":48,"module":0,"name":"shards"}],
 "programs":[{"attr0_writes":0,"class_counts":{"ALU":9,"MUL":3,"NOISE":1,"SPECIAL":2,"TABLE":1},
              "cycles_est":21,"dsp":3,"footprint_rect":[0,0,0,0],"instr_count":16,
              "kind":"field","max_ops":16,"module":0,"name":"rising_ridge",
              "profile":"earth","register_hwm":7,"source_id":50331649,
              "table_bytes":40}],
 "rates":[{"every":4,"invocation_every":1,"module":0,"name":"update_shards","phase":2,"selected_peak":2048,"stagger":true}],
 "scenario_asserts":[{"lines":["Duo"],"module":0,"name":"opposing_waves"}],
 "source_attribution":"sourceids.zmap"}
```

Member law (each row of FORM §14's list maps to a member):

| Member | Type | Law |
|---|---|---|
| `$schema` | string | fixed sentinel above; version-bumps only with this file |
| `abi_version` | u32 | the .zidl ABI version the build targets (pin/repro) |
| `pools[]` | array | one per pool: capacity (source-visible maximum population, FORM §14), element bytes (SoA sum), module index, name — capacities law FORM §21-9 |
| `rates[]` | array | one per system: authored `every` N, actual `invocation_every` cadence, assigned `phase`, `stagger` flag, and `selected_peak` entity count — the compile-time schedule (deterministic-scheduling §3/§5) |
| `programs[]` | array | one per field program: profile, `max_ops` declared, `instr_count` lowered (must satisfy `instr_count ≤ max_ops ≤` field-ir §7.3 ceiling), per-class counts, estimated cycles, DSP demand, table bytes, register high-water mark (all per field-ir §9), footprint rect (earth only; `[0,0,0,0]` for flow), source_id (kind 3) |
| `command_memory` | object | ceiling = `FRAME_SLOT_BYTES` (commands.zidl) and the per-frame estimate from the PresentZIR template census (W3.3) — FORM §14 "maximum command memory" |
| `particle_bandwidth` | object | bytes per live element per tick summed over flow-attached pools at capacity — FORM §14 "particle-state bandwidth" |
| `budgets[]` | array | the budget-line registry rows this build asserts (§3 below; values copied verbatim) |
| `scenario_asserts[]` | array | scenario `assert_budget` names carried into the release gate (FORM §14 build modes) |
| `modules[]` | array | module index → name (source-ID `module` field, capture_format §5) |
| `source_attribution` | string | names the sidecar mapping every `source_id` above to a span: `sourceids.zmap` (capture_format §7) |

Deferred members (NOT emitted in wave 3; reserved names so later waves add,
never break): `form_hierarchy_sizes`, `triangle_ceilings`,
`material_classes`, `program_cache_residency`, `terrain_samples_max` — they
belong to L3/L4 artifacts (forms, materials) that do not exist yet; adding
one is a schema-constant edit, and readers MUST ignore unknown members
(forward-compat law, mirroring .zcap §4.3-1).

The scenario `assert_budget` construct itself is L3 (FORM-E-720); wave-3
budget assertions are e2e tests reading this file (W3.7).

### 2.2 Rate and stagger accounting

For non-staggered `every N`, `invocation_every = N` and `selected_peak = 0`
(the row costs the whole system on its execution ticks). For staggered
`every N over pool`, `invocation_every = 1` because the system is called every
tick, and `selected_peak = ceil(pool.capacity / N)`. Cost tooling must use that
peak rather than multiplying it by an outer `1/N` rate: no such outer guard
exists. These fields derive only from HIR/ZIR schedule and declared pool
capacity; they do not claim Field IR instructions or any W3.4 physical cost.

## 3. Budget-line registry

Declared cost lines already ratified elsewhere; this file is their index.
Each line names its owner spec; `costs.zcost.budgets[]` copies the row
verbatim when a build asserts it. Wave-3 entries:

- `sky_triangles ≤ 352` total (192 drum + 16 cap + 2 under + 128 cloud +
  2 sun + margin), ×2 Duo views — spec/sky_and_beams.md §sky.
- `sky_fragments ≤ 92,160` (the clear it replaces) + cloud ≤ ~45K blended;
  VRAM ≈ 0.9 MB (~1.9% of the texture pool), shared between Duo views;
  measure-**exempt** with these declared budget lines, fully counted in
  charter §25 counters — spec/sky_and_beams.md §sky.
- God-beam post buffer: 96×60 / 2×64×48; 12 taps; decay 61/64; cost
  69,120 / 73,728 taps ≈ 4% of a 1.67M-cycle frame (100 MHz placeholder,
  Phase 0 freezes the clock); 11.5–12.3 KB M10K POSTBUF; ~0.5–1% ALM inside
  the 6% twod_post group — spec/sky_and_beams.md §beams.
- `frame_slot_bytes ≤ 1,048,576` — `FRAME_SLOT_BYTES`
  (spec/commands.zidl; capture_format §3) — the command-memory ceiling.
- Field program ceilings — field-ir.md §7.3 (earth 32 / flow 48 / global
  64): enforced at admission (language-semantics §6.1), reported per program
  in `programs[]`.
- `terrain_live_fields_per_patch ≤ 16` — MAX_PATCH_FIELDS, spec/
  terrain_rules.md §9.1 (enforced by TERRAIN.PATCH intake, command-order
  tail reject; runtime mirror `programs_rejected`).
- `terrain_bake_patches_per_frame ≤ 64` — BAKE_PATCH_BUDGET, spec/
  terrain_rules.md §9.2 (enforced by TERRAIN.BAKE ordered deferral; the
  bandwidth affordability behind it is explicitly not costed until ZH-004
  board truth — the row asserts the cadence cap only).

Hardware-absolute budgets (frame cycles, ALM/DSP/M10K) remain Phase-0 lane
(board truth); when they land, rows are added here, never edited in place.

## 4. Per-field instruction counts (report + enforcement)

For every admitted field program the compiler records (field-ir §9
mechanics): `instr_count` (lowered, post-macro-expansion — SMOOTHSTEP counts
its expansion), per-class counts, `cycles_est`, `dsp`, `table_bytes`,
`register_hwm`, and the admission triple
`instr_count ≤ max_ops ≤ ceiling`. Enforcement points:

1. **Admission** (compile): the triple above; violation is FORM-E-654/655.
2. **Report** (`costs.zcost.programs[]`): the numbers travel with the
   cartridge so tools and the runtime Measure can consume compiler metadata
   rather than infer (FORM §19.5).
3. **Source attribution**: each program row carries its `source_id` (kind 3,
   field program); runtime counter overruns resolve back to the declaring
   span through `sourceids.zmap` (FORM §17; capture_format §5/§7).

Static estimates do not replace runtime counters (FORM §14): the §25 counter
`field_instructions_by_profile` is the runtime mirror of the static
`programs[].instr_count`; the e2e gate (W3.7) asserts the island sequence's
measured counts against this report.
