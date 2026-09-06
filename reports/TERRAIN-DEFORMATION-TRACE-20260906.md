# Terrain Deformation — end-to-end trace (2026-09-06)

> Read-mostly investigation. **This file is the entire output**: no RTL, no
> `design/*.yml`, no `spec/`, no test was touched. The RCP-tile fit closure
> (`zhao_field_rcp24_rom.sv`, `zhao_raster_ticketq.sv`, `zhao_raster_rcp24_mul.sv`,
> `zhao_raster_rcp24_v3.sv`) was not read or edited; `zhao_terrain_writeback.sv`
> and its tests (another lane's) were not edited.
>
> The owner's question, verbatim: *"if geom.warp isn't needed, how are we going
> to do terrain deformation and effects? I thought that's what it was for. If
> its architecture doesn't fit that and we have nothing, we need to rearchitect."*

## Verdict, in four sentences

**Terrain deformation never ran through GEOM.WARP and never was going to** —
that part of the provisional answer survives testing (§Q3 counts the evidence;
consumers of GEOM.WARP in the whole tree: zero outside its own deferred cone).
**But "no gap, nothing to do" does not survive testing.** The machine that DOES
own terrain deformation is a set of individually verified organs that no
command stream can reach: the hardware command scheduler routes exactly four
opcodes and none of them is terrain, the Earth field slice has never run one
end-to-end program into a patch, and — the sharpest finding — **there is no
bake command on the wire at all, so not even the software reference console can
make a permanent hole in the ground from a command stream.** No rearchitecture
is needed; three specific compositions and one owner ruling are (§Fix).

---

## Trace 1 — Volcano rise / Bore scar (persistent height change)

Per `spec/terrain_rules.md:498` (§9.1 table), **Volcano is explicitly NOT a
live field**: "the rise is an incremental bake sequence (stamp records)" that
pressures the §9.2 bake budget. A Bore scar is the same machinery with a
negative delta. So the persistent chain is the bake chain, and the wave chain
(Trace 2) is genuinely different — the two-trace instruction was warranted.

| # | link | status | evidence |
|---|------|--------|----------|
| 1 | **game command carrying a dig/bake record** | **ABSENT — nothing on the wire** | `spec/commands.zidl:267–507` defines the complete command surface: `TerrainField 0x0200` (live fields) and `SurfaceStamp 0x0210` (layer-F *appearance* sheet, `tag/strength` — no depth field) are the only terrain commands. No record anywhere carries `depth_from/depth_to`, which is the entire vocabulary `zref::terrain::bake_dig` and TERRAIN.BAKE's packet table speak (`design/contracts/TERRAIN.BAKE.md:104–117`). Ruling T5's `TerrainEpoch 0x0220` / `SubmitTerrainSet 0x0230` are ruled but not yet in the zidl or the generated ABI (`grep 0x0230 fpga/rtl/generated/*.sv spec/commands.zidl` → zero hits) — and they are patch-set submission, not bakes. |
| 2 | CMD.SCHEDULER route | **ABSENT** | `fpga/rtl/command/zhao_cmd_scheduler.sv:357–378` dispatches exactly four opcodes: BeginFrame, SetPresentationContract, DebugFrameBlit, DebugRumble. The ledger's `CMD.SCHEDULER → SURFACE.STAMP / TERRAIN.SEQ / TERRAIN.PATCH` edges (`design/blocks.yml:410`) are graph intent, not RTL. |
| 3 | the SURFACE.STAMP → TERRAIN.BAKE seam | **SPECIFIED ONLY, explicitly undecided** | `design/contracts/TERRAIN.BAKE.md:48` ("THE SHEET SEAM — explicitly undecided, not invented"): `stamp_results` names two different wires; closing the seam needs **two laws that exist nowhere** — a strength(u8)→depth(fx16) mapping and a 64×64→33×33 resample — both look decisions per the art law. `tests/terrain/terrain_bake_chain.cpp:28–35` refuses to chain it for exactly this reason. |
| 4 | **TERRAIN.BAKE** (dig + breach law) | **BUILT, UNIT_VERIFIED** | `fpga/rtl/terrain/zhao_terrain_bake.sv` + `_bake_delta.sv`; ledger `design/blocks.yml:2292` UNIT_VERIFIED; tests `terrain_bake_directed/random/chain.cpp`. The §3.4 breach law (SOLID→VOID_BREACHED on exact compose_top==bottom at all four corners, heal on lift) is in the RTL's BREACH phase. *Caveat the contract itself states:* **no VRAM port** — the DIG lane is answered by the caller (`TERRAIN.BAKE.md:34–41`), so in a composed machine nothing yet connects it to the resident page it must mutate. |
| 5 | TERRAIN.BAKE → TERRAIN.PATCH (layer B into compose) | **BUILT and cross-verified** | `tests/terrain/terrain_bake_chain.cpp` runs both REAL blocks against each other and proves the cross-block invariant on all 1,024 cells: a cell the bake marks VOID_BREACHED is exactly a cell whose four corners the composer clamps onto the underside. This is the strongest seam in the whole chain. |
| 6 | TERRAIN.PATCH compose (§3.4) | **BUILT, UNIT_VERIFIED** | `fpga/rtl/terrain/zhao_terrain_patch.sv`; `design/blocks.yml:1694`; measured 64-of-64-clock vertex acceptance with no live field (`TERRAIN.PATCH.md`, Target throughput). |
| 7 | TERRAIN.COMPCACHE → TESS → NORMALS | **BUILT per block; PATCH→TESS composition through the cache NOT composed** | compcache front UNIT_VERIFIED (`blocks.yml:1820`, `compcache_front_rtl_directed.cpp`), but `design/prod_manifest.yml:216`: "nothing composes PATCH → TESS through it yet". TESS (`blocks.yml:1781`) and NORMALS (41,731 checks, `blocks.yml:1738` quotes the count) are UNIT_VERIFIED and pairwise composed in sim (`terrain_lod_tess.cpp`, `terrain_tess_normals.cpp`). |
| 8 | TERRAIN.PROJECT → renderer | **BUILT but unlit and un-shelled** | PROJECT UNIT_VERIFIED (`blocks.yml:2383`, `terrain_project_chain.cpp`), **but** `blocks.yml:1738` (TERRAIN.SHADE's purpose, current text): "production terrain has NO lighting path … TERRAIN.PROJECT has no colour port, and **the composed shell contains no terrain at all**." SHADE and NORMALMAP are REFERENCE_COMPLETE — reference only, no RTL. |
| 9 | the composed shell (step 8) | **ABSENT** | `design/prod_manifest.yml:215–234`: `zhao_terrain_visible`, `_compcache_front`, `_pageloader`, `_seq`, `_writeback` all "unused … zhao_shell_top does not compose it yet — that composition is step 8 of the world-layer build sequence". `zhao_prod_top.sv` instantiates blocks off separate LFSRs — "a resource-counting harness, not the console". |

**First broken link: #1 — the command itself.** And it is broken even in
software: `reference/src/zrender/terrain.cpp` executes `TerrainField` (line
197) and `SurfaceStamp` and nothing else; `zref::terrain::bake_dig`
(`reference/src/zterrain/terrain_core.cpp:91`) has exactly **one** caller in
the tree — `tools/reel/zhao_reel.cpp:2726`, the reel harness. The scars in
`terrain-scars/` and `terrain-breach/` were rendered by a tool calling the
bake function directly, not by any console executing a command. **The machine
— hardware OR reference — cannot today make a permanent hole in the ground
from a command stream.** Everything from TERRAIN.BAKE downward through PATCH
is built and genuinely well verified; the wound has no way to be ordered.

## Trace 2 — Waves (live, animated, per-frame)

| # | link | status | evidence |
|---|------|--------|----------|
| 1 | **game command** | **BUILT (wire + reference)** | `TerrainField 0x0200 implemented`, `spec/commands.zidl:341–348`: earth `.zprog` handle, footprint, `start_tick`/`duration_ticks` (which drive the age/phase lanes — the per-frame animation driver), 64 B of packed Q16.16 params. The programs exist and are committed: `compiler/tests/generated/wave_pool.zprog` (27 instrs), `impact_wave.zprog` (30), `crater_ring.zprog` — the §9 existence proofs. |
| 2 | software console execution | **BUILT** | `reference/src/zrender/terrain.cpp:197–260` applies TerrainField apps in command order via the one zfield interpreter, into the §3.4 composition. The reference can and does animate waves from a real command stream today (`terrain-wave/` reels; `terrain_dual.cpp` etc. pin the composition). |
| 3 | CMD.SCHEDULER hardware route | **ABSENT** | Same four-opcode dispatch as Trace 1. `zhao_cmd_dma.sv:134` knows the 112-B record size (the generated ABI decodes 0x0200), but the scheduler drops it on the floor — decoded, never dispatched. |
| 4 | FIELD.PROGCACHE | **BUILT, UNIT_VERIFIED** | `blocks.yml:786`; `zhao_field_progcache.sv`, `field_progcache_directed.cpp`. |
| 5 | **FIELD.SEQ.EARTH — the Earth slice** | **SPECIFIED; every organ built and swept, the composed slice never run** | Ledger `blocks.yml:872` maturity SPECIFIED, `implemented_by: FIELD.SEQ.CORE` (RTL_VERIFIED, `blocks.yml:829`). All 31 ops exist with differentials; the Earth lattice walker is built and mutation-swept (`field_walk_earth_directed.cpp`; STATUS.md:1278 — 17 mutants caught + 1 proven equivalent); the four-bank patch accumulator with the four per-lane reducer laws is built as probe (`fpga/rtl/synth/zhao_probe_patch_acc.sv`, `field_patch_acc_directed.cpp`); dispatcher/services/executor closed at 31/31, 28/28 etc. **But** `reports/REMAINING_BLOCKERS.md:1206–1210`: "deliberately NOT advanced past SPECIFIED … there is still no end-to-end Earth8 PROGRAM differential, and op coverage plus a shared implementation is not the same evidence as the profile having been run." The Phase-4 gate (descriptor → walker → executor → accumulator → composed cache, ≤850,000 clocks for the 128-association stress frame, `design/contracts/FIELD.SEQ.EARTH.md`, Target throughput) has not been run. This is precisely the shape the tasking warned about: a sibling once passed 21 checks over every input while dropping answers under backpressure — components green, composition untested. |
| 6 | field results → TERRAIN.PATCH | **port BUILT, driver ABSENT** | PATCH's `field_results` lane exists and is verified against harness-supplied lanes, but the only chain test drives it EMPTY: `tests/terrain/terrain_bake_chain.cpp:36–38` — "TERRAIN.PATCH's live-field lane is driven EMPTY. Composing a real field lane needs a zfield program through FIELD.SEQ.EARTH, which is not built". The only RTL drivers of `fld_valid_i` are the LFSR prod harness and the block itself. |
| 7 | per-frame recomposition (§4.2 once-per-frame into compcache, both views + sim mirror) | **SPECIFIED ONLY as a composition** | The cache front exists (Trace 1 #7); the once-per-frame drive is TERRAIN.SEQ's compose-slot allocation (built, 74 checks, `runs/CLAUDE-RUNS/RUN-20260906-2032-terrain-seq/TASK_LOG.md:60`) — but TERRAIN.SEQ is "unused" in the shell (`prod_manifest.yml:233`). |
| 8 | render | same as Trace 1 #8–9 | unlit, un-shelled. |

**First broken link (hardware): #3, the scheduler route** — with #5's
uncomposed Earth slice immediately behind it. The reference path is whole;
the hardware path has every organ and no bloodstream.

---

## The five questions

### Q1 — Can the machine, today, make a permanent hole in the ground and draw it?

**No — and not for the comfortable reason.** The chain is
`bake command → CMD.SCHEDULER → [bake queue] → TERRAIN.BAKE → TERRAIN.PATCH →
COMPCACHE → TESS → NORMALS → PROJECT → renderer`, and the first link that is
not built is the **first one**: no bake record exists on the wire
(`spec/commands.zidl` — counted: 20 commands, zero carrying a depth), no
scheduler route exists (counted: 4 routed opcodes, zero terrain), and the
reference console never executes a bake (one `bake_dig` caller in the tree,
and it is the reel tool). From TERRAIN.BAKE onward the chain is built and the
BAKE→PATCH seam is the best-verified composition in the terrain cone
(1,024-cell cross-block breach invariant). Secondary absences on the same
chain: BAKE has no memory attachment for its DIG lane, and the shell composes
no terrain.

### Q2 — Can it animate terrain (waves)? What drives it?

**Reference: yes, end to end, from a real command stream** — TerrainField
0x0200 with `start_tick`/`duration_ticks` driving the age/phase lanes is the
per-frame driver, `wave_pool.zprog` is committed, and the software console
renders it. **Hardware: no.** The engine (FIELD.SEQ.CORE) is RTL_VERIFIED,
the walker/accumulator/services are individually mutation-swept, and yet no
Earth8 program has ever run end-to-end into a patch in RTL — the ledger holds
the profile at SPECIFIED for exactly that reason, and the only chained terrain
test drives the field lane empty. The per-frame path *design* is sound and
fully specified (walker generates lattice points; TERRAIN.SEQ allocates
frame-scoped compose slots; §4.2 once-per-frame into the compcache; §9.1
MAX_PATCH_FIELDS=16 bounds the intake) — what is missing is the Phase-4
composition and its 850k-clock gate, then the shell.

### Q3 — Is anything relying on GEOM.WARP that has no other route?

**No. Counted, not assumed.** Every reference to GEOM.WARP in
`design/`, `spec/`, `reference/`, `fpga/`, `tools/`, `compiler/`:

- ledger edges IN: `FIELD.SEQ.WARP → GEOM.WARP` (`blocks.yml:910`) and
  `GEOM.LOOM → GEOM.WARP` (`blocks.yml:2788`) — its own deferred feeder and an
  edge `reports/GEOM-WARP-ARCHITECTURE-20260906.md` §1.3 already flags as
  stale;
- ledger edge OUT: `GEOM.WARP → GEOM.PROJECT` (`blocks.yml:2929`), where
  PROJECT's contract says vertices arrive "from GEOM.WCACHE **or** GEOM.WARP"
  (`GEOM.PROJECT.md:55`) — WCACHE is the built route, so PROJECT does not
  depend on WARP;
- `zref::GeomWarp`: named at `blocks.yml:2823`, exists nowhere in code;
- one comment in `zhao_geom_skin.sv:640`; the V1 release definition's
  DEFERRED row; the dashboard row. **Nothing in the terrain cone — not one
  contract, spec section, reference file or test — cites it.**

The division of labour is ratified, not incidental: terrain deformation is the
Field IR **E** profile plus the bake organs (`spec/form/field-ir.md` §7.1;
`terrain_rules.md` §4.1's one-evaluator law), and GEOM.WARP is the **W**
profile's *mesh-vertex* applier — spell ripples through a creature or
structure, wind response beyond bones. Owner ruling 2026-08-31 §6.3 already
names the substitutes for that (bounded creature-deform sidecar, Loom, HPS
preprocessing), and `reports/CREATURESANDLIGHTS:186–193` says in so many words
that GEOM.WARP "remains deferred and unnecessary". **The owner's worry is
answered in the good direction: deferring GEOM.WARP costs terrain nothing.
What terrain deformation lacks, it lacks for its own reasons (Q1/Q2), and
reviving GEOM.WARP would not supply one link of it.**

### Q4 — Rotated terrain sheets

**Nothing in the built path supports rotation, and by construction it cannot**:
`zref::terrain::ComposedLattice` (`reference/include/zref/zref_terrain.hpp:80`)
stores `wx` **per lattice column** and `wz` **per lattice row** — a separable
placement in which a rotated lattice is unrepresentable, which is what the
`column_query` precondition ("axis-aligned monotone", `zref_terrain.hpp:118`)
formalises. TERRAIN.TESS, PART.COLLIDE, SW.CPUCOLL and the §4.3 point query
all live inside that assumption, and the one-solid-interval column law itself
holds only in island space.

That is not a hole; it is the recorded design, with the owner's own resolution
already on the docket (`docs/OWNER_DOCKET.md:3288–…`):

- **Static rotation: rotate the ISLAND, not the patch.** Everything inside the
  island stays axis-aligned in island space. What changes, concretely: (1) one
  `orientation` field in the island table — `spec/terrain_rules.md` islands
  carry translation only today; (2) an inverse transform on **every**
  world-space query that reaches terrain (`column_query` callers: SW.CPUCOLL,
  PART.COLLIDE, TERRAIN.TESS's placement, creature ground taps in
  `zref_creature.hpp:800`); (3) the render side places patch geometry under
  the island transform. Nothing inside PATCH/BAKE/TESS/NORMALS changes —
  scars and deformation live in island space and ride along.
- **Real-time rotation (the falling skyscraper)** leans on GEOM.LOOM parenting
  terrain patches under transform nodes — and GEOM.LOOM is **SPECIFIED, not
  built** (`blocks.yml`, maturity), so this ask has a real unbuilt dependency,
  plus the docket's open questions (world-space column walk after tilt, keel
  direction, static→dynamic hand-off, pivot source).

Worth saying once: this is the ONE owner effect with any GEOM-adjacent
dependency, and its dependency is GEOM.**LOOM** (deferred-adjacent but not
cut), never GEOM.WARP.

### Q5 — Honest end-to-end status

`reports/Missingterrain`'s sentence — organs without a circulatory system —
is **half repaired since it was written**. Landed since: TERRAIN.RESIDENCY
suites, the compcache front, TERRAIN.PAGELOADER, `zref::swstream::WorldStreamer`
(the visible-set builder, `zref_sw_stream.hpp:361`), TERRAIN.ISLAND,
TERRAIN.VISIBLE, TERRAIN.SEQ (steps 1–5; SEQ on 2026-09-06, 74 checks), and
TERRAIN.WRITEBACK's F-sheet evacuation is in flight in another lane (step 6,
`runs/CLAUDE-RUNS/RUN-20260906-2035-…/LEDGER-ENTRY-REQUEST.md`). What is true
NOW:

1. **The heart exists but is not sutured in**: step 8 — TERRAIN.SEQ +
   directory + loader + compcache + organs in one Verilator top
   (`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md:503`) — is not done;
   `prod_manifest.yml:215–234` lists five terrain blocks as "unused".
2. **No terrain command reaches hardware** (four routed opcodes, zero terrain),
   and T5's SubmitTerrainSet/TerrainEpoch are ruled but absent from the zidl
   and the generated ABI.
3. **No field program has ever run into a patch in RTL** (Q2).
4. **No permanent wound can be ordered by anyone**, reference included (Q1).
5. **Even a composed terrain draw would be unlit**: TERRAIN.SHADE and
   TERRAIN.NORMALMAP are reference-only, PROJECT has no colour port
   (`blocks.yml:1738`).

The machine cannot yet draw ANY terrain from a command stream — deformed or
not. That is a sequencing truth, not an architecture failure: every missing
piece is either already sequenced (steps 6–9) or waits on one command-surface
ruling (below). Nothing needs rearchitecting.

---

## The gap that has no architecture yet, architected: the bake verb

Everything else missing is sequenced work with rulings in hand. One thing is
neither: **no law anywhere says how a permanent deformation is ORDERED.**
`terrain_rules.md` §9/§9.2 speak of "stamp records" with from/to depths;
TERRAIN.BAKE's contract takes that record; no command produces it, and the
one seam the ledger draws (SURFACE.STAMP → TERRAIN.BAKE) is explicitly
undecided with two missing look-laws. Proposal, in the COMPCACHE/PAGELOADER
house style:

**Purpose.** A `TerrainBake` command (suggested 0x0240, 48 B — the 0x02xx
terrain range is reserved for exactly this) carrying the dig record verbatim:
`{stencil handle, cx, cz fx16 island-datum, radius fx16, depth_from fx16,
depth_to fx16, flags}` — the exact argument list of `zref::terrain::bake_dig`
(`terrain_core.cpp:91`), so the wire speaks the language the arithmetic
already has. CMD.SCHEDULER routes it; a small bake-queue front feeds
TERRAIN.BAKE under §9.2's deferral law.

**Exclusions.** No strength→depth mapping and no 64×64→33×33 resample — the
SurfaceStamp coupling stays exactly as undecided as TERRAIN.BAKE.md records
it, and layer-F appearance stamping remains SurfaceStamp's. Taking the dig
record directly sidesteps both missing look-laws for v1 instead of inventing
them. No stencil evaluation (BAKE's), no breach law (BAKE's), no residency
decision (below).

**Owning block: extend TERRAIN.SEQ, not a new block.** The bake queue is a
second, simpler walk of the same machine SEQ already is: it knows residency,
claims, pins and issue order. And ruling T4 hands it an elegant simplification
— because the HPS keeps the canonical B/D mirror current from the same
deterministic command stream (which is why B/D are never written back), a bake
against a **non-resident** patch needs no hardware at all: the mirror applies
it and the scar arrives baked when the page next loads. Hardware bakes are
needed only for RESIDENT pages, which is exactly the set SEQ can already name.
The counted refusal is `bake_skipped_not_resident` and it is *healthy*, not a
fault — the TERRAIN.ISLAND argument.

**Arithmetic.** None new. The queue reorders nothing (§9.2 law 1: FIFO, never
dropped); the budget check is one counter compare against
`BAKE_PATCH_BUDGET = 64`; deferral is exact by the incremental-scaling
identity (§9.2 law 3). Every height belongs to `bake_dig`'s existing law.

**Storage.** One queue: 128 deep (one frame's budget of 64 plus one frame of
carryover) × ~328-bit record = 41,984 bits ≈ **5 M10K** against the device's
553 — with the terrain compcache's 15 and the pool pressure
TERRAIN.COMPCACHE.md records, still comfortably affordable. Plus BAKE's DIG
lane finally attached to memory: a read-write guard client on
TERRAIN.PAGE_POOL (the window exists write-only for PAGELOADER; widening it
is a guard-map amendment in the step-7 family, not a new mechanism). Zero DSP.

**Reference model: COMPOSE, never write.** Wire a `ZhCmdTerrainBake` into
`reference/src/zrender/terrain.cpp`'s existing terrain stage calling the
existing `zref::terrain::bake_dig` + `apply_breach_law`; queue policy composes
`zref::terrain::seq::Sequencer`'s patterns. One new law only: the queue/budget
policy, which §9.2 already wrote.

**Needs an owner ruling (Class C):** (1) the command ABI itself — a
command-surface addition under capture_format's versioning law, same shape as
T5's ruling for SubmitTerrainSet; (2) whether the stencil handle names the
paraboloid (v1, parameter-only) or a byte-stencil asset (the contract already
says byte stencils land with the asset lane); (3) BAKE_PATCH_BUDGET is
replay-semantics-affecting (§9.2 law 4) and must be ratified with the command.

---

## What to build first, and why

**First: step 8, the composed shell path** (TERRAIN.SEQ + directory + loader
+ compcache + organs in one Verilator top, harness supplying only the
FRAME_RING). Both traced effects die at the same missing seam — nothing
composes command→terrain→renderer — and this repo's own G1-D lesson
(2026-09-05) is that the first composed test of verified blocks found four
real defects the standalone suites could not see. Every ruling step 8 needs
exists (T1–T12); five of its blocks sit "unused" in the manifest today. Until
it runs, every terrain claim in this file rests on unit evidence, which this
repository has learned to distrust at exactly this moment.

Then, in order: the **Earth-slice Phase-4 composition** and its 850k-clock
gate (makes waves real in RTL — the plan already exists in
FIELD.SEQ.EARTH.md); the **bake-verb ruling + queue** above (makes permanent
wounds orderable — the only piece with no architecture until now); and
**TERRAIN.SHADE** (so the first composed frame is not flat-black ground).

GEOM.WARP appears nowhere in that list because terrain never needed it. The
rearchitecture the owner braced for does not exist to be done; the composition
work does, it is sequenced, and its first act is a Verilator top, not a
redesign.
