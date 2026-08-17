# Form Domains and Effects — L1 v1

**Status:** Phase 1 stub expanded to **L1 v1** (wave 3, plan W3.1, decision
D1; FORM §1–§2, §13; charter §23 Phase 3, §29-8). Companion law to
`spec/form/language-semantics.md` (grammar, FORM-E codes) and
`spec/form/deterministic-scheduling.md` (ordering). `spec/form/field-ir.md`
is FROZEN and authoritative for the five profile envelopes.

## 1. Truth/presentation law (frozen since Phase 1, unchanged)

Field programs compute **truth** (terrain height, velocity, material,
nav_cost) — never presentation. A field program is a pure function
(field-ir.md §1.4); its only effects are its output record lanes (§7.1) and
the sticky Status word (§3.19). Presentation (colour, emissive, shake) is
derived downstream from truth by raster/presentation blocks, never baked into
a field program. This is the seam that lets the same .zprog replay on
C++/RTL/board byte-identically.

L1 restates the law at language level: **truth is deterministic gameplay
state; form is its presentation; presentation may never mutate truth**
(FORM §1). The compiler enforces it structurally — presentation blocks have
no path to a write (language-semantics §3.3, FORM-E-405/400).

## 2. The L1 domains and their effect contracts

Every function and block belongs to exactly one domain (FORM §2); domains
are kinds, not annotations — the compiler checks the allowed effects per
domain and the domain is visible in HIR/ZIR (D3). L1 admits three *language*
domains (`sim`, `present`, `test`) plus two *field* profiles (`@earth`,
`@flow`) — the other six domains exist in the machine plan and are refused
in L1 (FORM-E-715/716/713; language-semantics §8).

| Domain | Carrier | May read | May write | May emit | Purity |
|---|---|---|---|---|---|
| `sim` | `system` bodies | declared `reads` | declared `writes` | — (schedules truth work) | deterministic function of (stateₜ, pads, tick) |
| `present` | `presentation` blocks | any truth state, read-only | **nothing** | ABI command templates (§4) | deterministic function of truth state |
| `test` | `scenario` blocks | everything | scenario-local expectations | captures, budget asserts | build/test-time only, never shipped |
| `fn` (unqualified) | pure functions | parameters + constants | nothing | nothing | total, side-effect-free |
| `@earth` | field declarations | the frozen input record | the frozen output record | — | pure (field-ir §1.4) |
| `@flow` | field declarations | the frozen input record | the frozen output record | — | pure (field-ir §1.4) |

### 2.1 `sim` effect contract

A system's effects are exactly: writes to the pool components/globals/terrain
channels named in its `writes` list, pool membership changes (spawn/kill via
§4.5 laws), and enqueued `@earth` applications (terrain truth). The tick
function is `stateₜ₊₁ = F(stateₜ, pad_snapshot, tick)` — no clocks, no
wall-time, no host environment reads (charter §29-7). Reads of components
not in `reads`: FORM-E-402. Writes not in `writes`: FORM-E-401.

`input.player(n)` is readable in `sim` (and `test`); the snapshot is taken
once at the tick boundary (input_rules §2.1) — all systems in one tick see
the identical snapshot; a system reading input declares `input` in `reads`.

### 2.2 `present` effect contract — THE PRESENT-PURITY LAW

> A presentation block **reads truth and emits commands; it writes nothing,
> ever.**

Consequences (each compiler-checked; FORM §13):

1. No assignment to pool/global/terrain state (FORM-E-405/400); no
   spawn/kill/apply (FORM-E-406/461).
2. `present_frame` is a pure function of `FormState` (D4): given the same
   truth state it emits a byte-identical command stream — this is what makes
   600-frame capture replay and frame-repeat-on-missed-deadline (video_rules)
   safe.
3. Reorderability: within a presentation block the compiler/runtime may sort
   opaque work, merge draws, cache templates and assign view masks (FORM
   §13). Source order is not a synchronization primitive; L1 has no
   ordering constructs (refused, FORM-E-717).
4. Presentation may consume explicit-derived randomness
   (`random.stream` seeded from truth state or constants) but never from
   wall-clock or input outside the sealed snapshot (the stream draws are
   pure given the seed — capture-replay law holds).
5. Presentation degradation (dropping effects under pressure) lives in the
   runtime's Measure policy, not in the language: the *declared* emit set is
   the maximal frame; the budget contract (§4 views) is the floor.

### 2.3 `test` effect contract

Scenario blocks are desktop/CI-time drivers (FORM §16): they synthesize pad
streams, drive ticks, and assert on truth (terrain height, state), captures
(frame N) and budgets. Scenarios never ship in the cartridge's executed
content — they compile to test-executable entries and golden manifests
(sim-hash chains D5, canvas CRCs). A scenario that changes truth outside the
driven systems (there is no construct for it) is unrepresentable; the only
"writes" are to test-local expectations.

## 3. Earth/flow profile admission rules (L1)

Profiles restrict **shape, never semantics**: one opcode table, one
interpreter, five envelopes (field-ir.md §7; frozen). L1 admits two:

**`@earth` admission.** A declaration is admitted iff:

1. Signature is `field name(params: P) -> terrain_delta` (or no params);
   in-scope lanes are the earth input record (field-ir §7.1: x, z, age,
   phase, p0..p7) — language-semantics §6.2.
2. `P` has ≤ 8 fields, all `fx16` (lane packing in declaration order).
3. A conservative footprint is declared (`rect`/`circle`/`capsule`,
   language-semantics §6.4) — the compiler records its `rectfx` envelope for
   the TerrainField command and the cost report (terrain footprint budget
   line, cost-model.md §4).
4. `max_ops ≤ 32` (earth ceiling, field-ir §7.3) and the lowered count
   `≤ max_ops` (FORM-E-654/655).
5. The body uses only dialect expression forms
   (language-semantics §6.3) — which is exactly the frozen opcode table
   plus the SMOOTHSTEP macro, so admission onto the *hardware* lane later
   changes nothing semantically (hardware-validity is a profile-engine
   concern, field-ir §9; wave-3 software runs the interpreter).

**`@flow` admission.** Same laws with the flow envelope: input record
(x..vz, age, seed, dt, p0..p3), `≤ 4` fx16 params, output `flow_update`
(including `attr0`), `footprint none`, `max_ops ≤ 48`, and an **attached
pool** whose struct matches the lane mapping (language-semantics §6.2;
FORM-E-664). A flow program runs once per live pool element per application
tick — the application rate is the applying system's rate (stagger applies
per entity id, deterministic-scheduling §3).

**Refused profiles in L1:** `@warp`, `@formation`, `@stamp`, `@build` and
their I/O records remain machine law (field-ir §7.1 rows) but no L1 syntax
lowers to them (FORM-E-715/713). When L2 admits them, this section gains
admission rows; field-ir.md does not change.

## 4. Presentation emit vocabulary ↔ ABI v3 (L1 table)

The L1 emit set is exactly the ABI v3 implemented Phase-3 subset
(commands.zidl; D7): `draw_form` → DrawForm 0x0300 (marker/billboard quads),
`draw_population` → DrawPopulation 0x0301 (point/triangle sprites),
`draw_procedural` → DrawProcedural 0x0302 (forge kind `heightfield_patch`),
`surface_stamp` → SurfaceStamp 0x0210 (circle/ring into the 64×64 sheet),
`audio` → EmitAudioEvent 0x0400 (wave-2 mixer tone). TerrainField 0x0200 is
not a presentation emit — terrain is truth (§1) — it is the *execution* of
an `@earth` application enqueued by `sim` (language-semantics §6.4); the
software console executes it in the terrain stage of the frame
(deterministic-scheduling §2). DrawSky 0x0310 remains reserved (wave 8; the
renderer's clear path emits layers per spec/sky_and_beams.md — not an L1
statement).

The authoritative source-kind registry is `capture_format.md` §5: every emit
site is kind **9** (presentation emit site), systems are kind **8**, pools are
kind **10**, and scenarios are kind **11**.

## 5. Interaction with TerrainField/SurfaceStamp records (Phase-1 stub §3, resolved)

- `TerrainField`: footprint = the program's `rectfx` envelope; `parameters`
  = the params-struct lanes packed Q16.16 LE in declaration order (bytes
  beyond 4·lane_count must be zero); `start_tick`/`duration_ticks` drive the
  `age`/`phase` input lanes.
- `SurfaceStamp`: `brush` = page-id const; circle/ring geometry from
  `radius`/`ring_width` (fx16); `tag`/`strength` from the emit arguments;
  the 64×64 sheet target is the surface-sheet asset named by the brush page.
- Material/nav vocabularies (`material: u32`, `nav_cost: fx16` lanes) are
  opaque word indices in L1: the material grammar that names them is L4
  (refused, FORM-E-713); L1 programs pass constants/derived values through.
  Hazard/tag semantics consumed by stamp outputs likewise remain
  machine-reserved until L4.
