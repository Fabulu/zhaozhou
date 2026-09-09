# The Projected-Vertex Arena and Triangle-Reference Replay

**Date:** 2026-09-09 · **Branch:** zixxtrixx-v8-closeout
**New RTL:** `fpga/rtl/common/zhao_proj_arena3.sv` (lint 0, quartus17-syntax clean)
**New test:** `tests/geometry/proj_arena3_directed.cpp` (standalone-buildable; ready to register in `tests/CMakeLists.txt`)
**Companion (already landed):** `fpga/rtl/common/zhao_project_service.sv` — one shared projector, round-robin, id-in-rider

---

## 0. One finding before anything else

The rescue roadmap says **"SUPPLIED: rtl/zhao_rescue_arena3.sv"**. That file
does not exist anywhere under the zencrifice root (searched every sibling
checkout and the repo). The kit's arena was never delivered. Everything below
is therefore authored fresh against the roadmap's *written* contract, and each
of the roadmap's numbers was re-derived rather than inherited. Where a number
checked out I say so; where I could not check it, I say that too (§7).

## 1. Why the arena exists: the shared projector is not fast enough WITHOUT it

The frame window at 100 MHz / 60 Hz with the roadmap's 20% reserve is
1,666,667 × 0.8 = **1,333,333 clocks**.

| Profile (two views) | Projection jobs | Verdict |
|---|---|---|
| Naive per-corner terrain: 2 × 256 patches × 6,144 corners | 3,145,728 | **fails 2.4×** even at one-per-clock |
| Subpatch reuse: 2 × (256 × 16 × 81 + 120,000 geom) | 903,552 | fits, **32% headroom** |

So the arena is not an optimisation layered on `zhao_project_service` — it is
the *precondition* for a single physical projector at fine terrain resolution.
Deleting the second 6,068-ALM / 33-DSP engine is only legal because reuse
brings demand under one-per-clock. The two wins are one architecture.

Redundancy verified from first principles: a 33×33 patch is 1,089 unique
lattice vertices; 32×32 cells × 2 triangles × 3 corners = 6,144 corner
references; **5.64×** redundant. At subpatch granularity (16 9×9 subpatches of
81 vertices), 16 × 81 = 1,296 fills against 1,089 unique — the 19% shared-edge
duplication is the price of bounded group size, and it is **already inside**
the 451,776/view number. Net projection reduction against naive: 4.74×.

## 2. The record: 106 bits, every field an invariant

`{behind[1], w[30:0], d[31:0], y[20:0], x[20:0]}` — verified field-by-field
against `zhao_project_core.sv`'s output ports (out_x_o/out_y_o signed [20:0],
out_d_o signed [31:0], out_w_o [30:0], out_behind_o). 21+21+32+31+1 = **106**.
The roadmap's number holds.

* **x, y** — S12.8 canvas coordinates clamped ±2048 px. What raster consumes.
* **d** — Q16.16 legacy 1/w. Today's depth-interpolation consumers eat exactly
  this; changing depth's format is a product-law change, out of scope.
* **w** — guarded clip w, fx16 raw, 31 bits. GEOM.DEPTHQUANT consumes w
  itself. Recovering w from the *rounded* 1/w compounds a rounding that
  already happened — forbidden. **Never dropped.**
* **behind** — clip.w ≤ 0. The core zeroes x/y/d/w behind the eye, so this
  flag is the only thing distinguishing "behind" from "a plausible vertex at
  the origin". **Never dropped.**
* **view is NOT a record field.** Projection is view-dependent, so view lives
  in the *group's* identity; a group holds one view's results only. Storing it
  per row would repeat one fact 81 times.

## 3. Identity: what makes a projected vertex "the same vertex"

This is the correctness crux and it has two halves.

**The key prevents false matches; it does not find matches.** The arena is
direct-indexed, not associative — reuse comes from the producer's *schedule*
(each subpatch's 81 vertices projected once, then 384 corner references replay
them), not from tag comparison. The key's job is refusal: a replay whose
provenance is not what the group holds must be impossible to satisfy.

A projected vertex's full identity is:

    (lattice index within subpatch)                — the row address
  × (patch id, subpatch id)                        — which lattice
  × (surface: top vs underside)                    — different final position
  × (deformation epoch)                            — different final position
  × (final morph / stitch state)                   — different final position
  × (view)                                         — different projection
  × (view-projection / viewport config epoch)      — different projection

A lattice index alone is **not** sufficient — the same index under a different
deformation epoch, morph state, surface or view is a different projected
vertex. The design splits identity by rate of change:

* Everything except the row index is **constant across one group lifetime**
  (dense fill under one seal guarantees all 81 rows share one provenance). So
  it is packed once, by the tessellator, into an opaque `KEY_W`-bit canonical
  key captured at `open` and returned with every read reply. Suggested
  packing (owner-editable, the arena never interprets it):
  `{view:1, surface:1, patch:8, subpatch:4, deform_epoch:8, stitch:4, cfg_epoch:6}` = 32.
* The **temporal** hazard the key cannot catch — a handle from a *previous
  lifetime* of the same group, every field individually plausible — is caught
  by the per-group `GEN_W` generation, checked on every read, refused
  deterministically. This is the old arena's generation law, kept.
* **Mid-lifetime config turnover is a producer-schedule violation by
  construction:** the key embeds the config epoch at open; the roadmap's rule
  (job captures its epoch at acceptance; drain before overwrite) applies to
  the group as the job. A group whose fill straddled a matrix write carries a
  key that no honest consumer's current epoch matches.

Rule inherited from the roadmap and honoured: *a different final position for
the same lattice index must create a distinct entry or separate group* — here,
a distinct key means a distinct lifetime, and the generation makes the old
handles refuse. Omitting view from a key is legal only for proven
view-independent caches; **for projection, view always matters** — it is bit 0
of the suggested packing precisely so it can never be truncated silently.

## 4. M10K arithmetic (shown, not asserted)

Per replica: 4 groups × 81 rows = 324 live rows × 106 bits = 34,344 bits.

* **512×20 aspect:** ⌈106/20⌉ = 6 blocks side by side, 512 rows deep.
  6 blocks/replica.
* **256×40 aspect:** 324 rows needs 2 banks deep × ⌈106/40⌉ = 3 wide = 6
  blocks/replica. Same count.
* **Bit floor:** 34,344 / 10,240 = 3.4 → ≥ 4 blocks, unreachable at legal
  aspects with one write + one read port. **6 is the honest per-replica count.**

Three replicas × 6 = **18 M10K**. The roadmap's "324×106, 3 SDP copies, 18"
row checks out. Against 553 blocks with ~147 historically used, 18 is cheap;
the roadmap's projection-domain envelope (48) holds it three times over.

**The padding discovery:** because the 512×20 aspect is 512 rows deep anyway,
padding each group's lane from 81 to a 128-row power-of-two stride costs
**zero extra blocks** — so the address is a pure `{group[1:0], index[6:0]}`
concatenation against a fully-allocated 512-row array. This satisfies the
roadmap's A3 repair ("linear `group*DEPTH+index` **or** fully allocated padded
groups, never a mixture") with no multiplier at all, and cannot reproduce the
old arena's stride bug (arena 1 starting at 2048 in a 2,178-word array). An
elaboration `$fatal` guards `STRIDE >= DEPTH` and power-of-two-ness, so a
future DEPTH change cannot silently reintroduce the mixture.

Replica count is 3 because replay, not projection, is the second bottleneck:
3,145,728 two-view corner *reads* through one port fails the same window that
naive projection did. Three simultaneous corner reads = one triangle per clock
= 1,048,576 replay clocks for the two-view stress (roadmap's number,
re-derived: 2 views × 256 × 16 × 128 triangles), inside the window at 79%.
Fill broadcasts to all three copies on the write port each replica already
owns (SDP), so fill-during-replay adds no port.

## 5. Lifetime, and why payload RAM needs no bulk reset

`OPEN → FILL ×DEPTH → SEAL → READ/REFERENCE → RELEASE`, all per group:

* **OPEN** (IDLE→FILLING): generation increments, key captured, counter
  zeroed. Opening a non-IDLE group refused + counted.
* **FILL** is **dense**: there is no fill-index port — the write address IS
  the group's fill counter. Rows 0..80 are written exactly once, in order,
  per lifetime. This one restriction deletes the sparse-validity problem: no
  per-row valid bitmap (the old arena pays ARENAS×DEPTH flops), no
  generation-tag-in-RAM ambiguity after power-up.
* **SEAL** (FILLING→SEALED) is *refused* unless the count is exactly DEPTH.
  Readability begins at seal and not one cycle earlier.
* **READ**: three replica ports, 1-cycle reply. Refused when group oob / not
  SEALED / generation stale / index ≥ DEPTH. Refusal is a **reply with a
  deterministically zeroed payload** (the core's behind-eye convention) — a
  consumer ignoring the flag reads zeros, never another group's vertex.
* **REFERENCE**: per-group outstanding-reference counter (the roadmap's
  starter held exactly one response; real raster/attribute stages retain
  handles longer, so it is a real REF_W counter with acquire/release ports).
* **RELEASE** (SEALED→IDLE) accepted only at zero references; a blocked
  release is refused + counted, visibly — never a hidden deadlock.

**Why no bulk reset:** an incompletely filled group can never seal, so it can
never be read; whatever the RAM holds after power-up or a previous lifetime is
*unreachable*, not hazardous. Reads of unsealed groups are refused by control
state (flops), deterministically — option 2 of the old arena's analysis
(generation-in-RAM, "almost certainly refuses") is not needed because dense
fill makes "was this row written this lifetime" a property of the *group*, not
the row. A reset loop over the payload would also break M10K inference
(QUARTUS_GOTCHAS 10). Same-address read-during-write is protocol-impossible
(fill requires FILLING, read requires SEALED), so no bypass network exists.

With GROUPS=4 (2 views × 2 working generations): per view one group fills
(81 clocks) while the previous replays (128 triangle clocks) — fill is never
the local bottleneck, and release/reopen rotates the pair.

## 6. Throughput: the whole argument in one table

| Quantity | Value | Source |
|---|---|---|
| Frame window (20% reserve) | 1,333,333 clk | 100 MHz / 60 Hz × 0.8 |
| Naive two-view terrain projection | 3,145,728 | re-derived, fails alone |
| Two-view subpatch projection + geom | 903,552 | re-derived = roadmap ✓ |
| Projection headroom at II=1 | 32% | 903,552 / 1,333,333 |
| Two-view corner reads | 3,145,728 | re-derived ✓ |
| Replay clocks with 3 replicas | 1,048,576 | = two-view triangles ✓ |
| Replay headroom | 21% | 1,048,576 / 1,333,333 |
| Roadmap's II=3 single-view subpatch check | 1,355,328 — fails | re-derived ✓ (why FAST II=1 is the default) |

Projection (fill side) and replay (read side) overlap on genuinely disjoint
ports; their maxima are not added. The binding resource after this change is
**replay at 79% of the window**, not projection — any future triangle-count
growth hits the read side first, and the mitigation is a fourth read port
group or a second arena bank, both M10K purchases, which is the correct
direction for this device.

## 7. Verified vs not verified

**Verified by me (recomputed or read from source):**
106-bit record composition against the core's ports; 5.64× redundancy; all
four roadmap stress rows (398,784 / 451,776 / 903,552 / 1,048,576 and the
II=3 failure); 18-M10K layout at both legal aspects plus the free-padding
observation; the missing kit file; that both project wrappers share one
view-projection matrix per view (core's own operand note); service demand
under one-per-clock; dense-fill compatibility with the shared service
(the rigid pipeline plus round-robin admission preserves each client's FIFO
order, so terrain's in-order submissions produce in-order fills).

**Not verified — named, with the instrument that would verify each:**
* Actual M10K inference and block count (18) — needs a `quartus_map`, batched
  into the next subsystem fit per the fit-boundary law. A geometry
  calculation is not a fitted receipt.
* Arena control ALM cost (est. small: ~4×(2+8+32+7+8) state flops + decode)
  — same fit.
* Geometry's 120,000 projections/view and the 256-patch × 16-subpatch visible
  workload — the roadmap's workload model, not re-measured here; the trace,
  not the example, chooses the shipping mode (roadmap's own words).
* Fmax of the three-replica read mux — Verilator cannot answer; same fit.

## 8. Implementation order (fit gates named in advance)

1. **Review, register, run** `proj_arena3_directed` in `tests/CMakeLists.txt`
   (copy `geom_wcache_directed`'s block). Every counter fires in it; the
   elaboration guards are fired by a deliberate bad-parameter build
   (`-GSTRIDE=64`), inverted polarity — lint cannot run `initial` blocks.
2. **Key producer:** teach the terrain tessellator to emit the canonical
   group key and dense (row-major subpatch) fill order. The key packing is a
   named, owner-editable localparam table, not scattered constants.
3. **Replay generator:** the static 128-triangle subpatch topology table
   (3 × 7-bit corner indices = 21 bits × 128 rows — one M10K or LUTRAM)
   driving the three read ports one triangle per clock, acquiring/releasing
   group references around each subpatch.
4. **Wire the fill path:** terrain client B of `zhao_project_service` →
   arena fill. The 42-bit terrain rider shrinks to a group handle plus corner
   bookkeeping the replay side no longer needs in flight — measure the rider
   width it actually still needs.
5. **Differential:** every replayed triangle compared against the retained
   `zhao_terrain_project` oracle across shared edges, underside winding,
   behind-eye corners, stale-handle replay and mid-frame reconfiguration —
   the roadmap's own required case list.
6. **ONE fit** for the projection subsystem (service + arena + replay),
   answering exactly: DSP 33 not 66; M10K = 18 + topology table; arena
   control ALM; Fmax at 100 MHz. Regenerate `zhao_prod_top.sv` first if any
   port reaches the top.

## 9. Evidence ledger (all run 2026-09-09, this session)

* `verilator_bin --lint-only -Wall` on `zhao_proj_arena3.sv`: **0 diagnostics**.
  (Quoted only for what lint settles: one tool's opinion, not synthesizability
  and not the initial-block guards.)
* `tools/quartus/check_quartus17_syntax.py`: clean, 215 files scanned, its own
  3-fire / 6-no-fire self-test passing first.
* `proj_arena3_directed` (Verilator 5.051 + g++, standalone build):
  **ALL CHECKS PASSED** — full-reply compares on all three replicas across
  fill/seal/replay, stale-generation refusal, index==DEPTH refusal, group-oob
  refusal, unsealed refusal, reference-gated release, reopen with old-handle
  refusal, and fill-during-replay overlap; every one of the nine counters
  moved to an exactly predicted value.
* Positive control 1 — `-GSTRIDE=64` build: `$fatal` fires at time 0
  ("STRIDE (64) must be >= DEPTH (81)"), process exits 1. The elaboration
  guard is a proven instrument; lint alone said nothing about it.
* Positive control 2 — `-GDEPTH=80` build: the directed test FAILS loudly
  (5 checks, exit 1). The checker has been seen to fire; its green run is
  therefore evidence, not decoration.
