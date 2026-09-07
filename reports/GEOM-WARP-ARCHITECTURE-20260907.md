# GEOM.WARP — What it is for, and the architecture that fits (2026-09-07)

> Recon-and-architect deliverable. **This file is the entire output**: no RTL,
> no `design/*.yml`, no `spec/`, no test, no contract file was touched. The
> running terrain fit's closure (`fpga/rtl/terrain/zhao_terrain_loadq.sv`) was
> not read or edited — nothing here needed it.
>
> **Standing status this document does not change:** GEOM.WARP is DEFERRED by
> owner ruling 2026-08-31 §6.3 (`reports/OWNER-RULINGS-COMPLETE-20260831.md:605–617`,
> cut-order 5), and every section of `design/contracts/GEOM.WARP.md` says
> "Deliberately unwritten … Fill this in only if an evidence-backed revival
> happens." Everything in Parts 2–5 below is the material a revival would
> ratify, not a revival.
>
> Two sibling documents from yesterday carry the deep recon and are cited, not
> repeated: `reports/GEOM-WARP-ARCHITECTURE-20260906.md` (the deferral recon
> and a first revival-ready spec) and `reports/TERRAIN-DEFORMATION-TRACE-20260906.md`
> (the terrain question traced end to end, both directions, every link graded).
> What this document adds: the owner's question answered in one place; port
> lists checked against the RTL seams **as they exist today** (`zhao_geom_skin.sv`,
> `zhao_geom_project.sv`, `zhao_field_seq.sv` — read this morning, widths
> quoted verbatim); the staged build plan with a first stage that fits; and the
> ruling list extended by one genuinely new gap (Q5 — there is no warp verb on
> the command wire at all).

---

# Part 1 — The owner's question, answered with counts

The question, verbatim: *"if geom.warp isn't needed, how are we going to do
terrain deformation and effects? I thought that's what it was for. If its
architecture doesn't fit that, we need to rearchitect."*

## 1.1 Terrain deformation never ran through GEOM.WARP, and was never specified to

This is not an interpretation; it is countable, and yesterday's trace counted
it (`reports/TERRAIN-DEFORMATION-TRACE-20260906.md`, Q3: every reference to
GEOM.WARP in `design/`, `spec/`, `reference/`, `fpga/`, `tools/`, `compiler/`
enumerated — **nothing in the terrain cone cites it**; its only graph edges
are its own deferred feeder, one stale LOOM edge, and an *or*-alternative into
GEOM.PROJECT that WCACHE already satisfies). Terrain deformation is a
different, ratified machine:

* **The law**: `spec/terrain_rules.md` §3.4 (lines 205–224) — per-vertex
  height composition `compose_top = max(fx(base)+fx(scar), fx(bottom))`,
  `live_top = max(compose_top + Σ field height lanes, fx(bottom))`. Base and
  scar are baked lattice planes; live animation is the field-lane sum.
* **The reference**: `zref::terrain::compose_vertex`
  (`reference/include/zref/zref_terrain_patch.hpp:195`) implements §3.4
  verbatim, saturating `fx_add` chain in command order, and is what the
  BAKE↔PATCH cross-block test proves against the RTL.
* **The evaluator**: live field lanes are Field IR **E-profile** programs
  (earth, id 0 — `spec/form/field-ir.md` §7.1) evaluated at lattice vertices
  by the ONE interpreter — FIELD.SEQ.EARTH, which is itself a *profile* of
  FIELD.SEQ.CORE, not a block (`design/contracts/FIELD.SEQ.EARTH.md`, "This
  is a PROFILE, not a block").
* **The permanence**: scars, breaches and heals are TERRAIN.BAKE's
  (`fpga/rtl/terrain/zhao_terrain_bake.sv`, UNIT_VERIFIED), under §3.4's
  breach law.

So the honest answer to "how are we going to do terrain deformation?" is:
**with the machinery that already owns it**, whose real gaps are composition
gaps, not architecture gaps — the command scheduler routes four opcodes and no
terrain, no bake verb exists on the wire, and the Earth slice has never run
one end-to-end program into a patch in RTL. All of that is traced, sequenced
and partly ruled in `TERRAIN-DEFORMATION-TRACE-20260906.md` (§Fix and "What
to build first"), and **reviving GEOM.WARP would supply not one link of it**.

## 1.2 What GEOM.WARP is actually for

GEOM.WARP is the application stage for the Field IR **W profile** (warp, id 1
— `spec/form/field-ir.md` §7.1): programs that take a **mesh vertex**
(position, normal, per-vertex amplitudes, time, params — 14 lanes) and return
a **displacement plus a replacement normal** (6 lanes), instruction ceiling 48
(§7.3). Its job in the game is the effects layer on *meshes*: spell-impact
ripples through a creature or structure, wind response beyond what bones
justify, Sacrifice-style spell-warped geometry. The full effects taxonomy, so
the question "how do we do effects?" has one table:

| effect class | profile / machine | status |
|---|---|---|
| terrain waves, rise, live height | E profile via FIELD.SEQ.EARTH → §3.4 compose | organs built; composition + command route sequenced (trace §Fix) |
| permanent terrain wounds (scar/breach) | TERRAIN.BAKE + bake verb | BAKE built; the verb needs a ruling (trace, "the bake verb") |
| particles | FLOW profile + PART.* | own cone |
| surface appearance stamping | STAMP profile + SURFACE.STAMP | own cone |
| creature identity deformation (flatten/spread) | CREATURE.DEFORM sidecar before SKIN | ruled, `reports/CREATURESANDLIGHTS:186–193` |
| instancing, gait, formations | GEOM.LOOM + FORMATION profile | SPECIFIED |
| **programmable mesh-surface motion** | **W profile + GEOM.WARP** | **DEFERRED, §6.3** |

The deferral ruling names the v1 substitutes for the last row explicitly:
"the bounded fixed creature-deform path; Loom transforms; HPS/PC preprocessing
where necessary" (§6.3). And the charter's cut ladder
(`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:1904–1913`) lists item 5 as "Warp8
**throughput**" — the ladder cuts the *rate*, not the concept, which is why an
architecture whose population knob is a counted budget (Part 2) is the shape
that survives the ladder.

## 1.3 Does the architecture fit? Verdict

**No rearchitecture is needed, in either direction.** Terrain deformation's
architecture exists and is the right one (§1.1). GEOM.WARP's specified
semantics — the W record — are exactly deformation-shaped for the thing it IS
for (displacement + normal out, applied to streamed mesh vertices). What
GEOM.WARP lacks is not a fitting architecture but *any* ratified architecture:
the contract is deliberately blank, `zref::GeomWarp` is a phantom (named at
`design/blocks.yml:3005`, exists nowhere — confirmed again today), and the
named tests (`tests/geometry/geom_warp_directed.cpp`, `geom_warp_random.cpp`)
do not exist (directory listed today). Part 2 supplies the architecture so
that a revival, if the owner rules one, costs days rather than weeks — and so
the owner can see concretely what the deferred thing would be before deciding
whether the substitutes still cover the game's needs (Q1).

---

# Part 2 — The block, concretely

## 2.1 One sentence, and the law it descends from

GEOM.WARP is the **W-profile stream adapter plus the displacement applier**:
it presents flagged skinned vertices to the ONE field engine as the §7.1 W
input record, reads back `(dx,dy,dz,nx′,ny′,nz′)`, applies the displacement
with three saturating adds, and emits exactly the packet GEOM.PROJECT already
takes. This shape is not a choice made here — the Field v3 amendment already
names it: a profile is "a program set plus a STREAM ADAPTER — … **Warp:
vertex position/normal** … Adapters generate and consume streams; they NEVER
re-implement an op, and there are not five engines"
(`design/contracts/FIELD.SEQ.CORE.md`, AMENDED 2026-08-27). Anything larger
contradicts a ratified ruling; anything smaller is not a block.

## 2.2 Position in the graph

    GEOM.SKIN ──► GEOM.WARP ──► GEOM.PROJECT        (warped batches)
    GEOM.SKIN ──► GEOM.WCACHE ─► GEOM.PROJECT       (everything else; unchanged)

Two ledger edges disagree with this and are flagged, not silently corrected:

* `GEOM.LOOM → GEOM.WARP` carrying `instanced_transforms`
  (`design/blocks.yml:2970, 2997`). Matrix application is GEOM.SKIN's job (its
  rigid path takes any 3×4 via `a_m_i`), and a warp stage that also applied
  transforms would be a second matrix-applier. Proposed dropped at revival —
  an owner/ledger call, listed under Q6.
* The ledger's `target_throughput: 1 warped vertex per clock`
  (`design/blocks.yml:3002`) is unreachable on any engine that exists for any
  program longer than one instruction (§2.7) and should be restated at
  revival as "engine rate; population bounded by `WARP_VERTEX_BUDGET`".

Dual-view note: GEOM.SKIN's output carries no view bit
(`zhao_geom_skin.sv:203–206`); the view is attached where PROJECT is fed
(WCACHE replays a projection into both views). Warp is world-space and
view-agnostic, so a warped vertex must be warped ONCE and projected twice —
which means the warped path either feeds WCACHE's fill like any other
projection source, or presents each warped vertex to PROJECT once per view.
Choosing between those is a capture-and-look question at revival, recorded in
Q6 rather than decided by assertion here.

## 2.3 Ports, widths verbatim from the seams that exist

Vertex ingress mirrors GEOM.SKIN's egress exactly
(`fpga/rtl/geometry/zhao_geom_skin.sv:201–206`), plus one flag; vertex egress
mirrors GEOM.PROJECT's ingress exactly
(`fpga/rtl/geometry/zhao_geom_project.sv:79–85`). House conventions
throughout: ready/valid, `_i`/`_o` suffixes, single `clk`, async `rst_n`,
conservative subset.

    module zhao_geom_warp (
        input  logic clk,
        input  logic rst_n,

        // ---- batch configuration (command shell; written only between batches)
        input  logic               cfg_we_i,
        input  logic [ 3:0]        cfg_addr_i,   // 0..3 p0..p3, 4..7 a0..a3,
                                                 // 8 time, 9 prog handle, 10 flags
        input  logic [31:0]        cfg_data_i,

        // ---- vertices in: GEOM.SKIN's output packet + the warp flag ----------
        input  logic               v_valid_i,
        output logic               v_ready_o,
        input  logic signed [31:0] v_x_i,        // fx16 world, as o_x_o
        input  logic signed [31:0] v_y_i,
        input  logic signed [31:0] v_z_i,
        input  logic        [15:0] v_src_id_i,
        input  logic               v_warp_i,     // batch-gated per-vertex flag

        // ---- engine seam: the v1 walker's register-file port, driven, not owned
        // (fpga/rtl/field/zhao_field_seq.sv:168–179 — quoted, existing widths)
        output logic               rf_we_o,
        output logic [ 5:0]        rf_waddr_o,
        output logic signed [31:0] rf_wdata_o,
        output logic [ 5:0]        rf_raddr_o,
        input  logic signed [31:0] rf_rdata_i,
        output logic               clear_o,
        output logic               start_o,
        input  logic               busy_i,
        input  logic               done_i,
        input  logic [ 7:0]        status_i,

        // ---- vertices out: GEOM.PROJECT's input packet ------------------------
        output logic               out_valid_o,
        input  logic               out_ready_i,
        output logic signed [31:0] out_x_o,
        output logic signed [31:0] out_y_o,
        output logic signed [31:0] out_z_o,
        output logic        [15:0] out_src_id_o,

        // ---- counters ---------------------------------------------------------
        output logic [31:0]        warped_vertices_o,
        output logic [31:0]        bypass_vertices_o
        // + refusal/sat counters per §2.6
    );

The engine instance itself is NOT inside this block — one engine, five
profiles; the adapter drives a seam to the shared walker (or, later, to the
v3 fabric's adapter slot). Program fetch stays FIELD.PROGCACHE's; the adapter
carries a handle, never bytes. Normal lanes: under the Q3 baseline the W
record's `nx,ny,nz` inputs are loaded (the program may read them) and the
`nx′,ny′,nz′` outputs are read and **discarded** — GEOM.PROJECT takes no
normal, GEOM.LIGHT's upstreams do not include warp, and inventing that edge
is exactly what Q3 exists to prevent.

## 2.4 Pipeline: a six-state adapter, not a pipeline at all

    IDLE → LOAD(14) → RUN(engine busy) → READ(6) → APPLY → EMIT
                └──────────── bypass vertices skip straight to EMIT ─────────┘

One vertex in flight, strictly in order — a bypass vertex behind a warped
vertex waits, because downstream assembles positionally (the order IS the
index; reordering would silently re-index geometry). Inputs are **latched on
accept**: the walker reads its file for the whole run, and GEOM.SKIN's
contract records why a combinational read across a multi-cycle engine is
wrong (producer may change data the cycle after `ready`). LOAD walks the 14
lanes through `rf_we_o` in §7.1 record order — the lane binding this block
exists to pin (`design/contracts/FIELD.SEQ.WARP.md`: the binding "belongs
with the blocks that consume the output", i.e. here). READ walks the
program's declared `out_lanes` (`zfield::Decoded` metadata), not fixed
registers, on the v1 walker; the v3 snooped-export form wants fixed output
registers, which is Q4.

## 2.5 Arithmetic: three adds, zero multiplies, zero DSP

Every multiply, curve, rot, noise and their roundings live in the engine
under `zfield::interpret`'s law, already golden-pinned. This block:

    warped.x = sat_s32( sx33(px) + sx33(dx) )    // and y, z alike

s32 + s32 → s33, narrowed one component at a time with its own counted
saturate — the exact semantics of `zref::fx_add`
(`reference/include/zref/zref_fixp.hpp`). No rounding exists: same-format
integer add moves no fraction bits. Amplitude scaling (`a0..a3 × anything`)
happens inside the program under the engine's `fx_mul`; a multiply in the
applier would be a second statement of arithmetic the interpreter owns.

**DSP: 0.** This is a hard requirement, not a virtue: the block-census
arithmetic is 171 DSP demanded against 112 on the device
(`reports/RASTER_Polygon_Budget_Proposal.md:166`), and GEOM.SKIN's own header
records what one stage carelessly provisioned cost (72 DSP measured, 64% of
the chip, later resequenced). A warp stage is affordable only because the
expensive machinery — register file, shared multiplier, sin/rcp ROMs, program
cache — is the field cone's, already spent and shared whether or not warp
ever runs.

## 2.6 Storage, backpressure, refusals

**M10K: 0. All state is registers**, with the arithmetic shown:

| structure | bits | derivation |
|---|---|---|
| batch config | ≈ 340 | handle 32 + p0..p3 4×32 + a0..a3 4×32 + time 32 + flags ~4 + budget residue 16 |
| vertex in flight | 113 | x,y,z 3×32 + src_id 16 + warp flag 1 |
| output skid | 113 | same shape, one beat |

≈ 570 payload flops plus FSM and counters. There is deliberately no vertex
buffer: buffering would re-open the M10K-vs-flop inference trap for nothing
the one-in-flight design needs, and against a Cyclone V M10K (10,240 bits,
≤ 40 wide) a 113-bit record would burst into three M10Ks per buffered lane —
spent to hold vertices a strictly-ordered stream cannot overtake anyway.

**ALM ceiling: 1,200 — an estimate, not a fit.** Anchor: GEOM.LOOM's ratified
ceiling is 1,500 ALM *including* twelve multiplies-in-logic
(`design/contracts/GEOM.LOOM.md`); this block is three 33-bit saturating
adders, a 14-entry lane sequencer, a 32-bit 14:1 source mux, an FSM and
counters — strictly less machine. The first standalone fit (Stage B, Part 5)
replaces this estimate with a measurement.

Backpressure: house hygiene (`in_ready_o` independent of `in_valid_i`;
`out_valid_o` independent of `out_ready_i`); `v_ready_o` is low for the whole
engine occupancy — the `!busy` term GEOM.SKIN's contract records learning the
hard way. Refusals — each counted, each `src_id`-attributed, none silent:

| condition | behaviour | counter |
|---|---|---|
| warp batch, no valid program bound | whole batch degrades to identity | `warp_refused_no_program` |
| bound program's profile ≠ 1 | refused at bind, before any vertex | `warp_refused_bad_profile` |
| `WARP_VERTEX_BUDGET` exhausted (provisional 4,096 — a spec constant, the cut-ladder's sanctioned knob) | remaining flagged vertices pass identity | `warp_refused_budget` |
| engine status ≠ ran-to-END | defined saturated outputs applied; status accumulated | `warp_status_*` |

Identity fallback rather than drop, because a warp vertex is *geometry*:
dropping holes a mesh, and each vertex is independent (no dependency chain to
protect, unlike LOOM's drop-the-stream law). Whether identity-under-pressure
LOOKS acceptable in scene is an art-law question — a capture to look at, not
a number, noted in Q1's evidence bar.

## 2.7 Throughput, honestly, derivation shown

Per warped vertex on the **v1 scalar walker** (the only FROZEN, measured-class
engine — `FIELD.SEQ.CORE.md` engine table): 14 lane loads + start + I
instructions × ~8 clocks (the docket's worked rate,
`design/budgets/workloads.yml:289–296`) + 6 lane reads + apply/emit
≈ **8·I + ~24 clocks**. At 100 MHz / 60 Hz there are 1,666,666 clocks per
frame; **assuming** a 20% warp reserve (assumption, not a ruling — 333,333
clocks):

    I = 8  →  ~88 clk/vertex  →  ~3,700 warped vertices/frame
    I = 48 →  ~408 clk/vertex →  ~800 warped vertices/frame

That is the honest v1 tier: one hero creature's spell-hit ripple and a
handful of warped props — not scene-wide wind, and ~400× short of the
docket's 120,000-vertex example, which is the same arithmetic that forced the
deferral (workloads.yml: "4.6× the whole frame"). The v3 vector fabric
raises the population by whatever its Phase-3 probes actually deliver — and
those are **targets, not measurements** (`reports/FIELD_V3_COST_MODEL.md`
§1); today's v3 demonstrator fit (`reports/V3-DEMONSTRATOR-FIT-20260907.md`)
is a texture-lane result and says nothing about field-engine rates. The seam
in §2.3 is engine-agnostic on purpose: lane binding and application law are
identical under both engines, so v3 changes the batch scheduler, never the
contract.

**The number most likely wrong in this whole document is the +24 constant**
(and the 8 clk/instruction it rides beside): derived from the docket example
and v2's measured transport shape, never measured on the v1 walker with a
real W-shaped program. v2's transport was ~25 clk for a 12-in/4-out profile;
warp is 14-in/6-out through a synchronous-read file, so the constant could
plausibly be ~40+, halving every population figure above. That is why
Stage 0 of the build plan is a measurement, not RTL.

---

# Part 3 — What GEOM.WARP must NOT do, and who owns each excluded thing

| excluded | owner |
|---|---|
| op semantics, evaluation, per-op rounding | FIELD.SEQ.CORE / `zfield::interpret` (one-engine ruling 2026-08-22; adapters "NEVER re-implement an op") |
| program storage, decode, validation | FIELD.PROGCACHE + the `zfield` decoder |
| terrain deformation of any kind | E profile + TERRAIN.BAKE/PATCH (§1.1 — the owner's question, closed) |
| fixed creature deformation (flatten/spread) | CREATURE.DEFORM sidecar before SKIN (`reports/CREATURESANDLIGHTS:186–193`) |
| transform composition, instancing, gait, formations | GEOM.LOOM (+ FORMATION profile) |
| matrix application to vertices | GEOM.SKIN's rigid path — warp applies **no matrices** (why the LOOM edge is flagged, Q6) |
| skinning, normal blending | GEOM.SKIN, GEOM.SKIN.NORM |
| projection, lighting | GEOM.PROJECT, GEOM.LIGHT |
| routing warped normals to lighting | nobody, today — no ledger edge exists; creating one is Q3, an owner call |
| memory access | none: no VRAM port, no guard client, no bus master — same stance as SKIN and PROJECT |
| ordering warp batches on the wire | the command surface — which today has no warp verb at all (Q5) |

Every row is a thing the blank contract could plausibly have been read to
include; the exclusions are the defect-prevention.

---

# Part 4 — Reference model and oracle

**`zref::GeomWarp`** — the name the ledger already reserves
(`design/blocks.yml:3005`) — as a thin header
`reference/include/zref/zref_geom_warp.hpp` COMPOSING, never authoring:

1. **lane packing** — the §7.1 W record built from {vertex, batch config,
   time}: owned here, it is the binding this block exists to pin;
2. **evaluation** — `zref::fieldir::interpret`
   (`reference/include/zref/zref_fieldir.hpp:52`, forwarding to
   `zfield::interpret`, the ONE interpreter, TS-mirrored, `.zvec`-pinned —
   that header's own words on why a wrapper must forward and never reimplement);
3. **application** — `zref::fx_add` per component, `SatLedger` threaded;
4. **policy** — identity bypass, budget, refusal taxonomy: owned here.

**The oracle is values AND counts AND order, explicitly.** Values: every
output word differentially against the composed reference. Order: `src_id`
sequence out equals sequence in (positional assembly downstream). Counts:
every counter delta against the reference's ledger — this repo has twice
watched a machine do the work multiple times while producing byte-identical
output (the combiner double-issue, the shell bench's fifteen-fold resubmit;
`CLAUDE.md`, "Counters see what pictures cannot"), and a warp block's bugs
live exactly in the bypass/warp interleave under backpressure where value
comparison is blind. Every refusal counter is deliberately fired once in the
directed suite — a detector never seen to fire has not been tested.

---

# Part 5 — Staged build plan (every stage gated on Q1 except the first)

**Stage 0 — measure, before any RTL. Ungated; one afternoon.** Run one
committed W-shaped program (a real 14-in/6-out lane shape) through the
existing v1 walker bench for one vertex and count the clocks. This replaces
the 8·I+24 model (§2.7) with a number, and every population claim and the
`WARP_VERTEX_BUDGET` proposal moves with it. Read-only with respect to the
tree; needs no ruling because it decides nothing — it removes a bias from the
decision the owner has to make (Q1), which is exactly the side of the line
measurement belongs on.

**Stage A — reference + lane binding (gated on Q1 ≥ "adopt as revival
text").** `zref_geom_warp.hpp` per Part 4, plus
`tests/geometry/geom_warp_directed.cpp` cases 1–3 (identity program;
constant displacement; saturation corner) driven against the reference alone.
No RTL, no fit, no ledger maturity change beyond REFERENCE_COMPLETE evidence.

**Stage B — the adapter, standalone. The first fit, and it is small.**
`zhao_geom_warp.sv` per §2.3–2.6 with a real `zhao_field_seq` +
`zhao_field_rf_ram` instance in the testbench (the engine is not stubbed —
stubbing it would test a machine that does not exist). Differential: directed
cases 1–7 (add: bypass/warp interleave under three `out_ready` duty patterns
asserting exact counts; budget edge; both refusals fired) and the random
suite (valid W programs via the existing `.zvec` generator machinery, random
duty-cycle ready schedules including long-low — the 15,625-case
all-ready-high blindness is this repo's own scar tissue). Fit: one leaf
block, estimated ≤ 1,200 ALM, 0 DSP, 0 M10K — physically trivial beside the
texture island's 16,192 ALM, and nothing like an island run. The fit's
closure is this block plus the field-cone sources; per the standing rule,
work outside that list continues while it runs.

**Stage C — the PROJECT seam pair.** `zhao_geom_warp` feeding the built
`zhao_geom_project` in one Verilator top, captured stimulus, both views —
the G1-D lesson (first composed test of verified blocks found four defects
the standalone suites could not see) applied at the cheapest possible scale:
two blocks, one seam. This is where the dual-view question (Q6) produces its
capture instead of its argument.

**Stage D — the v3 adapter lane.** Gated on the v3 Phase-3 probes existing
at all; changes the batch scheduler and the export-register snoop (Q4), and
must not touch the contract's lane binding or application law.

---

# Part 6 — Open questions requiring an OWNER RULING

Listed, not answered. Inventing any of these is the fault class this tree
treats as serious; recommendations are marked as such and bind nothing.

* **Q1 — Revival of §6.3, and at which tier.** (a) keep deferred, ratify this
  document as the revival-ready text; (b) revive at the v1 selective tier
  (~10³ vertices/frame, §2.7); (c) wait for v3 probes. *Recommendation: (a).*
  The evidence on file (workloads.yml's 4.6×-frame arithmetic;
  CREATURESANDLIGHTS' "deferred and unnecessary") still points away from
  building; the §6.3 bar is an *evidence-backed* revival, and the evidence
  that would clear it is a game capture showing an effect the
  sidecar+Loom+HPS substitutes cannot do — looked at, per the art law, not
  argued.
* **Q2 — Where `a0..a3` per-vertex amplitudes come from.** (a) batch
  constants, zero format change; (b) a vertex-format extension — which is
  SW.TOOLS.ASSET's spec to write (format-0 bytes 24–31 are
  reserved-must-zero); (c) derived from `w0`. *Recommendation: (a) for v1.*
* **Q3 — Warped normals.** The W record outputs `nx′,ny′,nz′` and **no
  consumer edge exists anywhere in the ledger** — GEOM.PROJECT takes no
  normal, GEOM.LIGHT's upstreams are SKIN/SKIN.NORM. (a) v1 position-only,
  `n′` read-and-discarded, lighting keeps the unwarped normal; (b) a new
  ledger edge routing `n′` to GEOM.LIGHT. *Recommendation: (a), with the
  lighting delta looked at in a capture before (b) is priced.*
* **Q4 — Fixed W export registers** for the v3 snooped-export adapter: a
  validator whitelist-table edit, to be agreed with the FPLAN/ops owner.
* **Q5 — The command ABI (new in this document).** `spec/commands.zidl`
  (commands 0x0000–0xF004, enumerated today) carries **no warp verb**:
  `TerrainField 0x0200` binds E-profile programs to terrain, but nothing
  binds a W program + params + budget to a draw — `DrawForm 0x0300`
  (`spec/commands.zidl:381`) has handles for form/material/transform and
  16 flag bits, no field-program handle. Whether warp batches are ordered by
  a new 0x03xx command, a DrawForm-adjacent binding, or flags is a
  command-surface addition under the frozen additive-change law
  ("additive change = new opcode") and is the same *class* of ruling the
  terrain trace requested for the bake verb. Without it, a revived block
  would be reachable only from testbenches — the exact circulatory-system
  failure the terrain trace documents.
* **Q6 — Two graph corrections at revival time.** (a) drop the
  `GEOM.LOOM → GEOM.WARP / instanced_transforms` edge
  (`design/blocks.yml:2970, 2997`) — warp applies no matrices; (b) decide the
  dual-view composition (warped vertices through WCACHE fill vs. once per
  view into PROJECT), from Stage C's capture. Both are ledger edits and so
  are flagged here rather than made — this lane wrote exactly one file.

---

*Load-bearing sources, all read today unless noted: `design/blocks.yml:2989–3016,
933, 2970, 3111`; `design/contracts/GEOM.WARP.md`; `design/contracts/FIELD.SEQ.WARP.md`;
`design/contracts/FIELD.SEQ.CORE.md` (2026-08-27 amendment + engine table);
`design/contracts/GEOM.WCACHE.md`; `spec/form/field-ir.md` §§7.1–7.4;
`spec/terrain_rules.md` §3.4; `spec/commands.zidl:267–507`;
`reference/include/zref/zref_terrain_patch.hpp:170–220`;
`reference/include/zref/zref_fieldir.hpp`; `fpga/rtl/geometry/zhao_geom_skin.sv`;
`fpga/rtl/geometry/zhao_geom_project.sv`; `fpga/rtl/field/zhao_field_seq.sv:163–219`;
`reports/OWNER-RULINGS-COMPLETE-20260831.md:605–617`;
`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:1904–1913`;
`design/budgets/workloads.yml:280–296`; `reports/RASTER_Polygon_Budget_Proposal.md:166`;
`reports/CREATURESANDLIGHTS:178–196`; `reports/TERRAIN-DEFORMATION-TRACE-20260906.md`;
`reports/GEOM-WARP-ARCHITECTURE-20260906.md`; `reports/V3-DEMONSTRATOR-FIT-20260907.md`.*
