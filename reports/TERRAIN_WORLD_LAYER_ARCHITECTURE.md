# The Terrain World Layer — architecture for the 8 km streamed island world

**Written 2026-09-02. This is an ARCHITECTURE DOCUMENT, not RTL.** It designs
the "circulatory system" that `reports/Missingterrain` says is absent — the
layer that turns the built, heavily-tested per-patch terrain organs into an
actual streamed world. Primary source: `reports/Missingterrain` (the owner
brief, lines 166–179 list exactly what is missing). Everything below either
cites a written law (file:line) or is marked **OPEN — needs an owner ruling**.
No law is invented here; `reports/CONSOLE_REMAINING.md` documents why a block
built on invented laws is worse than no block, and this document obeys that.

Device facts used throughout: Cyclone V 5CSEBA6U23I7 — **553 M10K = 5.53 Mbit**
(`reports/BINNER_CAPACITY_FOR_8KM_MAPS.md:128-129`), **41,910 ALM**
(`reports/EARTH60_CAPACITY.md:176`), **128 MB local SDRAM** behind a 27-bit
guard address (`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:403`,
`fpga/rtl/common/zhao_pkg.sv:227`), **1,666,667 gpu clocks per 60 Hz frame at
the 100 MHz placeholder** (`reports/EARTH60_CAPACITY.md:62`).

---

## 0. The honest frame: what "8 km" can and cannot mean

The arithmetic first, because it bounds every decision below.

* A patch is 32×32 cells on a 33×33 lattice (charter §11.1,
  `spec/terrain_rules.md:37`); at the canonical 2 m pitch
  (`spec/terrain_rules.md:51-56`) one patch is **64×64 m**.
* A full page is **21,376 B stride** (21,320 B body + pad; 334 × 64-B bursts,
  `spec/terrain_rules.md:107`).
* A dense 8×8 km plate: 8,000 / 64 = 125 patches per side; 125² = **15,625
  pages**; 15,625 × 21,376 B = 334,000,000 B = **318.5 MiB** — 2.5× the entire
  128 MB local SDRAM, before textures, framebuffers or anything else
  (`reports/Missingterrain:122-124` derives the same number). **A dense 8×8 km
  plate at 2 m pitch CANNOT be resident. Ever. The design does not try.**
* What IS designed for: the **1,024-patch resident set** (charter §12 sheet
  arithmetic, `spec/terrain_rules.md:38`). 1,024 × (64 m)² = 4,194,304 m² =
  **4.19 km² of registered ground at 2 m pitch, 16.8 km² at 4 m**
  (`reports/Missingterrain:128-130`). Resident bytes: 1,024 × 21,376 B =
  21,889,024 B = **20.9 MiB**, which is exactly what `spec/terrain_rules.md:466-472`
  §8 already budgets (14.38 MiB sheets/material + 6.44 MiB heights inside the
  8.95 MiB hot-cache figure).
* Residency is **sparse**: an absent directory entry is open sky — no page, no
  sheet, no draw (`spec/terrain_rules.md:69-72`). An 8 km world of scattered
  floating islands, or one long narrow island, fits the resident set. A camera
  crossing a larger solid region **streams** — pages enter and leave residency
  as it moves. That is what this layer exists to do.

So the world layer's job, restated from `reports/Missingterrain:99-108`:

    camera moved
      → inspect island directory                 (HPS — §2.1)
      → determine visible patch coordinates      (HPS — §2.1)
      → union the two players' working sets      (HPS — §2.1)
      → prefetch missing pages                   (HPS emits, FPGA loads — §2.3)
      → allocate local-SDRAM residency slots     (FPGA — §2.2, BUILT)
      → preserve dirty scars from evicted pages  (§2.4, one OPEN ruling)
      → issue all visible patches to the engine  (§2.6)
      → PATCH → LOD → TESS → NORMALS → PROJECT → GEOM.PARAMBUF   (§3)

---

## 1. What exists, what this document adds

**Built and tested (the organs):** `zhao_terrain_patch.sv` (compose + intake,
1,379 + 14,730 checks), `zhao_terrain_lod.sv`, `zhao_terrain_tess.sv`,
`zhao_terrain_normals.sv`, `zhao_terrain_project.sv`, `zhao_terrain_velocity.sv`,
`zhao_terrain_bake.sv`/`_bake_delta.sv` — all under `fpga/rtl/terrain/`, with
the TESS→NORMALS (41,731 checks) and other pairwise compositions green
(`design/contracts/TERRAIN.TESS.md:254-266`).

**Built and NOT yet tested or instantiated:** `zhao_terrain_residency.sv` —
the 1,024-slot directory (its own header, line 5: "FIRST BLOCK OF THE WORLD
LAYER. Nothing instantiates it yet."). No test exists under `tests/terrain/`.

**Specified but stub:** `SW.STREAM` — every substantive section is TODO
(`design/contracts/SW.STREAM.md:13-49`). This is the block whose job is
"pull in the rest of the island", and `reports/Missingterrain:64-69` names its
absence as the clearest single answer to why the world is a little spot.

**Missing entirely:** the island directory / visible-patch builder, the page
loader, dirty writeback, the 256-patch composed cache, and the command path
that submits visible patches to the terrain pipeline. Those are §2 below.

---

## 2. Block-by-block breakdown

Staging rule observed throughout: **beside the existing block, not replacing
it.** Nothing below modifies `zhao_terrain_residency.sv`, `zhao_terrain_patch.sv`
or any tested organ; every new block attaches at an existing port or a
documented seam.

### 2.1 WORLD.DIRECTORY — island directory + visible-patch builder

* **Purpose:** hold the sparse island directory and, per frame, produce the
  deterministic ordered list of (island, patch) pairs the frame needs —
  visible set for both views, plus the prefetch ring.
* **Side: HPS. This is written down three times, not a choice made here.**
  `reports/Missingterrain:168` — "A real island directory and visible-patch
  builder on the HPS." Charter §6 (`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:317-330`)
  gives ARM/HPS "resource streaming", "broad visibility sectors" and
  "command-buffer construction"; the FPGA gets only "camera-local visibility
  refinement". The directory data itself is cartridge asset kind 7
  `ISLAND_TABLE` (`spec/cartridge.md:96,143`) per `spec/terrain_rules.md:79-88`
  §1.5: island_id, world origin/datum, pitch_log2, grid extent, tileset, and
  the sparse (ix, iz) → page-handle map. Parsing an asset table is software.
* **Why HPS also on the merits:** the visible-set walk is pointer-chasing over
  a sparse map at two cameras' frusta — irregular, low-bandwidth (≤ a few
  hundred entries/frame), branch-heavy, and already adjacent to the HPS-owned
  command builder that must emit the result. Hardware would buy nothing and
  would freeze a policy that game code should own.
* **Interface sketch (all outputs land in the FRAME_RING packet,
  `spec/memory_rules.md:202-222`):**
  - `visible_list` — ordered records {island_id, patch_ix, patch_iz,
    residency key (§2.2 obligation 1), staging address of the page in HPS DDR,
    flags} — the frame's terrain submission, in a **canonical deterministic
    order** (proposal: island_id ascending, then iz-then-ix — same z-then-x
    convention the lattice already uses, `spec/terrain_rules.md:442`, `spec/cartridge.md:141`).
  - `prefetch_list` — same record shape, pages wanted resident but not yet
    needed this frame.
  - Inputs: both cameras (SetView), the island tables, previous-frame
    residency counters (charter §6: "predictive quality selection from
    previous-frame counters").
* **What it must NOT do:** refine per-subpatch LOD (TERRAIN.LOD's job); touch
  VRAM; make residency decisions (it proposes; the FPGA directory disposes);
  evaluate any field program (`spec/terrain_rules.md:306-315` §4.1 — one
  evaluator, at lattice vertices, on the FPGA/zref side only).
* **Determinism:** pure function of (camera states, island tables, declared
  prefetch policy). Because the list is carried IN the sealed frame packet and
  CRC-gated (`design/contracts/CMD.DMA.md:7`), replay of a capture replays the
  exact working set — the HPS's walk does not even need to be re-run to verify.
* **Home:** this is SW.STREAM's payload-building half plus a visible-set walk
  in SW.RUNTIME.HPS; writing SW.STREAM's contract (currently 15 TODOs) is the
  gating act — `reports/CONSOLE_REMAINING.md` establishes that no build
  happens against a TODO contract.

### 2.2 TERRAIN.RESIDENCY — the deterministic 1,024-slot directory (BUILT)

`fpga/rtl/terrain/zhao_terrain_residency.sv`. **Exists; not redesigned here.**
SLOTS = 1024, PCW = 12 (signed patch coords), GENW = 4 (lines 72-80).
Direct-mapped on the low 5+5 coordinate bits (`slot_of`, lines 156-159) —
victim selection is a pure function of the incoming key, which is the
determinism the console's replay verification demands (header, lines 34-52).
Collision period is 32 slots × 64 m = **2,048 m** per axis; `collisions_o`
makes thrash visible (lines 47-52, 235-240).

**Interface obligations on everything that attaches to it:**

1. **The key.** Lookup/claim take only signed (px, py). There is **no
   island_id in the tag** — see OPEN question 1. Until ruled, no two islands
   may be given overlapping key ranges by WORLD.DIRECTORY.
2. **Claim-then-writeback-then-load ordering.** A claim returns the victim
   {px, py, evicted, evicted_dirty} in the same response (lines 209-215). The
   directory does **not** stall slot reuse: the integration MUST drain the
   dirty victim (§2.4) before the loader's first write to that slot lands.
   The directory reports; the sequencer enforces.
3. **Handles are {slot, generation} and stale means abandon.** Any consumer
   holding a handle across frames (compose jobs, bake jobs, the composed-cache
   allocator) must `check_*` before use and abandon on `chk_stale_o`
   (lines 55-68, 255-261) — never re-resolve silently.
4. **GENW = 4 gives 16 generations.** A handle held across ≥16 claims of the
   same slot aliases (classic ABA). Obligation: no handle outlives the frame
   that minted it unless the holder re-checks each frame; with ≤1 claim per
   slot per frame this needs 16 frames to bite, and frame-scoped handles never
   bite. State this in the integration contract rather than widening GENW.
5. **Same-cycle port hazard (real, found by inspection while writing this
   document).** In the `always_ff`, the claim's `load_r[slot] <= 0` (line 228)
   precedes the finish path's `load_r[slot] <= 1` (line 249) in source order;
   the finish guard compares against the **pre-increment** generation. A
   `fin_valid_i` for the OLD occupant arriving in the SAME cycle as a
   `cl_valid_i` for the same slot passes the guard and marks the freshly
   claimed, unfilled page LOADED. Same shape for `dirty_valid_i` (line 252).
   **Obligation: the integration must never present fin/dirty and claim for
   one slot in the same cycle** (trivially met by a one-deep ordering in the
   sequencer), OR the block gets a one-gate guard (`fin_slot_i != cl_slot_c`)
   when its directed suite is written. Recorded here so it is a test case,
   not a discovery.

**What it lacks (noted, not redesigned):**

* **No directed/random test.** Every other terrain block has both; this one
  has zero checks. First buildable item, §5.
* **Never synthesized, and as written it will not map to M10K:** the arrays
  are read combinationally (lines 162-176), which infers ~1,024 × 31 =
  **31,744 flops**. Moving to M10K (~4 blocks for 31 Kbit) requires a
  registered-read revision — a Class-A internal change
  (`reports/OWNER-RULINGS-20260831.md:131-137`) for whoever synthesizes it,
  flagged now so the flop count is not a Quartus surprise.
* **No unload/invalidate port and `resident_o` never decrements** (only
  incremented, line 233). Level teardown has no mechanism except full reset —
  OPEN question 10.
* **No dirty-sweep/enumerate port.** Writeback triggers only on eviction; a
  "save the world now" walk must come from the HPS side, which knows what it
  submitted. Acceptable if OPEN question 4 resolves toward the CPU-canonical
  mirror; otherwise a debug read port is a later amendment.
* **No per-layer dirty granularity** — one bit covers layers B, D and F
  together. Writeback therefore moves whole pages (or the ruling in OPEN 4
  removes the need).

### 2.3 TERRAIN.PAGELOADER — prefetch from HPS DDR into local SDRAM

* **Purpose:** move whole pages, HPS-DDR staging → local-SDRAM page slots,
  CRC-checked, and report completion to the directory.
* **Side: FPGA** (a DMA fed by the frame packet; the HPS stages, per charter
  §7.2 "upload staging", `ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:419-431`).
  Cartridge → HPS DDR staging is HPS software (SW.STREAM); DDR → VRAM is this
  block.
* **The written laws it executes:** pages stream **whole** — 21,376 B stride,
  334 × 64-B bursts, immutable in flight (`spec/terrain_rules.md:453-455`);
  bursts ride MEM.HPS.BRIDGE's frozen port (64-B aligned, len 1..64 B, 64-bit
  beats, sim profile 16 cycles + 1 beat/cycle —
  `design/contracts/MEM.HPS.BRIDGE.md:13,17`); VRAM writes go through
  MEM.GUARD like every fabric client (`spec/memory_rules.md:272-276`); the
  page carries `page_crc32c` over bytes [64, 21320) (`spec/terrain_rules.md:126`)
  and the console's standing CRC discipline is **verify before commit**
  (CMD.DMA's gate law, `design/contracts/CMD.DMA.md:7,55`).
* **Interface sketch:**
  - `job` in (from the sequencer, §2.6): {slot, gen, hps_addr, vram_addr,
    island_id, px, py}, ready/valid.
  - HPS side: `zhao_hps_burst_req_t/rsp_t` client port.
  - VRAM side: `zhao_guard_req_t/rsp_t` client port (client id: OPEN 3).
  - `fin` out → the directory's `fin_valid_i/slot/gen` (residency lines
    109-113) — pulsed only after the LAST byte is written AND the CRC matched.
  - `crc_fail` out: pulse + counter + the failing (island_id, px, py) as a
    trace event; the page is NOT marked loaded (lookup keeps missing —
    "present but not loaded", residency lines 166-171 — which is the safe
    state), and the failure surfaces in counters rather than in geometry.
  - Counters: `pages_loaded_o`, `page_crc_fails_o`, `load_bytes_o`.
* **CRC staging choice (stated, cheap to reverse):** the CMD.DMA precedent
  verifies before the first byte commits, which for a 21 KiB page means either
  a two-pass read (read once to check, again to move: 2× bridge traffic) or a
  VRAM-side staging slot written then published by directory-fin only after
  the check. **Proposed: single pass, CRC accumulated on the fly, `fin`
  withheld on mismatch.** The slot's bytes are garbage on a mismatch but are
  unreachable — nothing reads a slot the directory does not call loaded, and
  the next claim of that slot overwrites. This honours the gate's intent (no
  corrupt page is ever *consumed*) without double bandwidth. If the owner
  wants the stricter no-corrupt-byte-ever-lands law, the two-pass read is the
  fallback; it fits the budget below with margin.
* **Mip derivation:** TERRAIN.LOD's `sp_dev*` deviations come from the 17×17 +
  9×9 per-surface mips that are "derived at load/bake, not streamed"
  (`spec/terrain_rules.md:109-111`) — 1,480 B/patch. WHICH side derives them
  is not written: OPEN question 8. Both candidates satisfy every written law;
  the loader is architected to carry an optional 1,480 B appendix in the
  staged page so the HPS answer costs zero RTL.
* **Budget arithmetic (sim profile — board truth pending ZH-004, and
  `spec/terrain_rules.md:456-457` explicitly says do not freeze):** the
  provisional worst case is 32 pages/frame = 32 × 21,376 = 684,032 B/frame ≈
  **41 MB/s** (`spec/terrain_rules.md:455-456`). On the sim bridge profile a
  page is 334 bursts × (16 + 8) cycles = 8,016 cycles ≈ 80 µs; 32 pages =
  256,512 cycles = **15.4 % of the 1,666,667-clock frame** on the bridge —
  real but affordable, and it overlaps compute. The SDRAM write side is 684 KB
  against the arbiter's burst framework; sequential 64-B writes are its best
  case.
* **What it must NOT do:** interpret any layer (it moves bytes; TERRAIN.PATCH
  and friends interpret); write outside its granted region (MEM.GUARD's law);
  touch the directory except via `fin`; reorder pages within a frame's job
  list (determinism: jobs execute in list order, completions may not be
  awaited out of order by the sequencer).

### 2.4 TERRAIN.WRITEBACK — dirty-page evacuation

* **Purpose:** when the directory reports `cl_evicted_dirty_o`, move the
  victim's mutated layers out of local SDRAM before the loader reuses the slot
  — so permanent scars and breaches survive eviction and a later return.
* **The tension that produces OPEN question 4 (the most consequential ruling
  in this document):** `spec/terrain_rules.md:462-464` — "The sim (SW.CPUCOLL)
  owns the **canonical** mirror of B/D … CPU owns canonical scars." If the
  HPS-side mirror is canonical and always current, then on eviction there is
  **nothing to write back**: reload composes the page from the cartridge
  layers plus the HPS's own canonical B/D, and the FPGA writeback block
  shrinks to nothing for B/D. But layer F (surface sheets, written only by
  SURFACE.STAMP, `spec/terrain_rules.md:459-460`) and layer H have **no stated
  CPU mirror**, and `reports/Missingterrain:171` lists "dirty-page writeback"
  as a required piece. Both readings are defensible from the written law;
  neither is written as the law. **This document therefore specifies the block
  at the seam where both rulings can use it,** and the ruling decides its
  payload (nothing / F only / B+D+F).
* **Interface sketch:**
  - `job` in: {slot, vram_addr, hps_addr, island_id, px, py, layer mask} from
    the sequencer, issued on every dirty eviction (and only then).
  - VRAM read port (guard client) + HPS-DDR write port (bridge client — note
    MEM.HPS.BRIDGE's current write grants are descriptor words and trace
    extents only, `design/contracts/MEM.HPS.BRIDGE.md:25`; a writeback arena
    is a new granted extent in that contract's law — part of OPEN 4).
  - `done` out to the sequencer — the loader's job for that slot is gated on
    it (obligation 2 of §2.2).
  - Counters: `pages_written_back_o`, `writeback_bytes_o`.
* **What it must NOT do:** write anything for a clean eviction; write HPS DDR
  outside its granted extent; be on the critical path of a frame that evicts
  nothing (zero-cost when idle).

### 2.5 TERRAIN.COMPCACHE — the 256-patch composed-height/velocity cache

The cache `spec/terrain_rules.md:317-324` §4.2 budgets and both TERRAIN.PATCH
and TERRAIN.TESS name as their missing middle
(`design/contracts/TERRAIN.PATCH.md:257-265`,
`design/contracts/TERRAIN.TESS.md:270-275`).

* **Purpose:** hold the once-per-frame composed `live_top` lattice (and the
  velocity lattice) for up to 256 live/visible patches, so both views and the
  sim mirror consume ONE composition (`spec/terrain_rules.md:319-324`,
  charter §11.5).
* **Where it lives — forced by arithmetic, shown:** 256 × 2,178 B = 557,568 B
  = **544.5 KiB** heights, + 544.5 KiB velocity = 1,089 KiB = **8.92 Mbit =
  161 % of the device's entire 5.53 Mbit of M10K.** It cannot be on-chip. It
  lives in the terrain hot-cache pool in local SDRAM, exactly where §8 already
  budgets it (composed 0.53 MiB + velocity 0.53 MiB,
  `spec/terrain_rules.md:471`).
* **The on-chip patch front — forced by TESS's port:** TERRAIN.TESS reads its
  lattice through a **registered port: datum the cycle AFTER the request**
  (`design/contracts/TERRAIN.TESS.md:43-49`), one read per clock in steady
  state. SDRAM cannot serve that. So the patch being tessellated is staged in
  an on-chip front: live_top + bottom lattices, 2 × 1,089 × 32 b = 69,696 bit
  ≈ **7 M10K**, plus the 32×32 cell-state plane (layer D, u8) = 8,192 bit =
  1 M10K. Double-buffered (fill patch N+1 while TESS eats N): **≈16 M10K =
  2.9 % of the device.** This is the same storage the Field-v3 amendment
  already identifies as the per-patch scratch/accumulator
  (`design/contracts/TERRAIN.PATCH.md:305-311` — four-bank, vertex-mod-4
  indexed, ≈16–20 M10K, inside the amendment's ≤64 M10K Earth-slice gate); the
  front IS that scratch's landing zone, not a second copy.
* **Allocator (the piece Missingterrain calls "composed-height-cache
  allocator"):** 256 slots, allocated **per frame in submission order** — slot
  n goes to the n-th patch of the frame's visible_list that needs composition.
  A pure function of the frame's own command list: no history, no timing,
  deterministic by construction. Cache associativity is explicitly Class A
  ("agent decides", `reports/OWNER-RULINGS-20260831.md:131-137`), so this is a
  decision this document may make — recorded with its rejected alternative:
  a coordinate-hashed persistent cache would let unchanged patches skip
  re-composition across frames, but §4.2's law is already "produced once per
  frame" for every patch touched by a live field or needing visibility, and a
  persistent cache adds cross-frame state that replay must then reconstruct.
  Frame-scoped is smaller, lawful and deterministic; revisit only if the
  compose budget (EARTH60) forces reuse.
* **Interface sketch:** write side — TERRAIN.PATCH's `patch_state` output
  stream (top/bottom/dirty per vertex) plus the velocity lane; storage side —
  one guard client doing sequential 2,178 B lattice writes/reads; read side —
  fills of the patch front, then TESS's `lat_*` and `cs_*` registered ports
  served from the front at one datum per clock. `cache_slot` handles carry
  {slot, frame parity} — stale by construction at frame end.
* **Overflow:** the 257th patch needing composition in one frame has **no
  written law** — OPEN question 6. Until ruled the block refuses loudly:
  reject the job, count (`compcache_rejections_o`), trace the (island, px, py)
  — the MEASURE.HISTOGRAM discipline of refusing rather than inventing.
* **What it must NOT do:** compose anything (TERRAIN.PATCH's law); evaluate
  fields (§4.1); persist across frames (chosen above); serve a patch whose
  residency handle went stale (check before fill — §2.2 obligation 3).

### 2.6 TERRAIN.SEQ — command submission into the terrain pipeline

* **Purpose:** the pump. Walk the frame's visible_list and drive every seam
  in order: residency lookup → (miss: writeback → load) → compose (PATCH +
  field engine → COMPCACHE) → LOD → 16/32 TESS jobs → NORMALS → PROJECT →
  GEOM.PARAMBUF. This is the block whose absence makes the current island "the
  unit under test made visible" (`reports/Missingterrain:110-112`).
* **Side: FPGA**, in the command mantle beside CMD.SCHEDULER — which today
  "does not have a draw path yet" (the shell's own comment, quoted at
  `reports/Missingterrain:91`). CMD.SCHEDULER keeps what it owns (FRAME_RING
  state transitions and per-frame guard grants,
  `design/contracts/CMD.SCHEDULER.md:23-25`); TERRAIN.SEQ is the terrain draw
  path it dispatches to, not a replacement.
* **Interface sketch:**
  - in: decoded terrain records for the frame (see the ABI question below);
    the frame's grant/go from CMD.SCHEDULER.
  - directory side: drives `lu_*`, `cl_*`, consumes victims, sequences
    §2.2's obligations 2 and 5 (writeback-before-load; never fin+claim same
    slot same cycle).
  - loader/writeback side: job queues, completion waits.
  - engine side: per-patch job issue to PATCH (dispatch + compose),
    LOD (`patch_state` descriptors ×16), TESS (`lod_target` jobs — LOD's
    output IS TESS's job port field-for-field,
    `design/contracts/TERRAIN.LOD.md:14-16`), and onward — the downstream
    seams are already port-compatible and tested pairwise.
  - Counters: patches submitted / skipped-not-resident / stale-abandoned,
    per-frame high-water marks.
* **Ordering law:** everything in visible_list order. Painter/submission order
  is semantically observable downstream
  (`reports/BINNER_CAPACITY_FOR_8KM_MAPS.md:178-180`), and list order is the
  determinism anchor for the whole layer.
* **A patch that is not resident when its turn comes is SKIPPED and counted,
  not stalled on** — the frame must never wait on a 80 µs page load mid-walk;
  prefetch exists so this is rare, and the counter makes it visible. (One
  frame of missing ground at a streaming edge, loudly counted, versus a
  deadline fault: chosen, cheap to reverse.)
* **The ABI it consumes is OPEN question 5:** today's `DrawProcedural` names
  ONE terrain-patch page per record (`spec/commands.zidl:348-362`, [w3]
  software-executed). Whether the hardware path is N such records (one per
  visible patch — a few KiB against the 1 MiB frame slot,
  `spec/memory_rules.md:204`) or one new set-command is a permanent-ABI
  question, Class C (`reports/OWNER-RULINGS-20260831.md:141-146`).
* **What it must NOT do:** decide visibility (HPS's, §2.1); decide LOD
  (TERRAIN.LOD's); reorder; invent degrade policy (The Measure's / OPEN 6);
  hold any state across frames beyond counters.

### 2.7 Not blocks, but required amendments (the plumbing this layer stands on)

* **MEM.GUARD region map** gains the terrain regions — page pool, composed
  cache, velocity, sheets. `spec/memory_rules.md:282-284` already says later
  phases APPEND regions, never reshape the law; the actual bases/sizes are
  unwritten → OPEN question 2. Ownership rules are already written and are
  simply enacted: layers A/C/E/H read-only to fabric, B/D written only by
  TERRAIN.BAKE, F only by SURFACE.STAMP, composed cache only by TERRAIN.PATCH
  (`spec/terrain_rules.md:458-461`).
* **Client ids:** `zhao_client_e` has ENGINE0/ENGINE1 granted (ENGINE0
  framebuffer writer, ENGINE1 the parameter buffer —
  `reports/OWNER-RULINGS-20260831.md:70-72`) and ids 5/6 free
  (`fpga/rtl/common/zhao_pkg.sv:217-224`); `reports/SUNDER.md:615` already
  proposes client 6 as TERRAIN_BUILD. Loader/writeback/compcache need a home
  → OPEN question 3.
* **MEM.HPS.BRIDGE write grants** gain the writeback arena iff OPEN 4 says
  writeback exists (its granted-writes list is currently descriptor words +
  trace extents, `design/contracts/MEM.HPS.BRIDGE.md:25`).

---

## 3. The full chain, named end to end

    HPS: WORLD.DIRECTORY (island tables, two frusta)
      → FRAME_RING packet {visible_list, prefetch_list}     [CRC-gated, CMD.DMA]
    FPGA: TERRAIN.SEQ walks the list
      → TERRAIN.RESIDENCY  lookup/claim  (BUILT)
      → TERRAIN.WRITEBACK  (dirty victims — payload per OPEN 4)
      → TERRAIN.PAGELOADER (HPS DDR → SDRAM page slot, CRC, fin)
      → TERRAIN.PATCH + field engine → TERRAIN.COMPCACHE    [composed once/frame]
      → TERRAIN.LOD   (reads mips + governor targets; no memory port — fed)
      → TERRAIN.TESS  (reads the compcache patch front, registered port)
      → TERRAIN.NORMALS  (port-for-port, already composed & green)
      → TERRAIN.PROJECT  (its output IS GEOM.SETUP's input packet)
      → GEOM.PARAMBUF    (Wave A, owner ruling 4/12 — NOT this layer's to build;
                          this layer lands on its doorstep)

Every seam from PATCH onward already exists as tested ports; the world layer
adds the four blocks upstream and the two stores, and composes — it does not
reopen any organ.

---

## 4. Determinism ledger

The console verifies by deterministic replay; every policy above is a pure
function of declared inputs:

| policy | pure function of | history/timing dependence |
|---|---|---|
| visible/prefetch set | camera states + island tables + declared policy | none — and it travels IN the capture |
| eviction victim | incoming key (direct map) | none (residency header, lines 34-52) |
| writeback trigger | claim's evicted_dirty in list order | none |
| compcache slot | position in the frame's visible_list | none (frame-scoped) |
| submission order | visible_list order | none |
| load completion | awaited in job order by TERRAIN.SEQ | bridge latency varies; *consumption* order does not |
| not-resident skip | directory state at the patch's list position | deterministic given identical prior frames — which replay guarantees |

The one subtlety is the last row: whether a page IS loaded when its turn comes
depends on load latencies. Replay determinism holds because captures replay
the bridge's deterministic sim profile (harness-as-HPS, MEM.HPS.BRIDGE.md:7);
on the board, frame-to-frame variance appears only as the counted
skip-not-resident, never as different geometry for a composed patch. If exact
board/sim frame parity is later demanded, the skip decision can be moved into
the packet (HPS predicts residency), costing nothing now — stated so the
choice is visible.

---

## 5. Dependency-ordered build sequence

Contract-first throughout (`reports/CONSOLE_REMAINING.md`'s law). Each step
composes with the previous; nothing waits on an owner ruling except where
marked.

**The first three are immediately buildable — every law they need is already
written:**

1. **TERRAIN.RESIDENCY directed + random suite.** The RTL exists with zero
   checks. Laws: its own header + this document's §2.2 obligations. Must pin:
   hit/miss/claim/re-claim (gen must NOT advance on re-claim, lines 216-220),
   gen-guarded fin/dirty, stale handles, dirty-eviction reporting, the
   same-cycle fin+claim hazard (obligation 5) as an explicit case, counter
   laws, and a determinism case (same key sequence twice ⇒ identical victim
   sequence). No new RTL, no rulings needed.
2. **TERRAIN.COMPCACHE patch front + the PATCH→LOD→TESS composition.**
   TERRAIN.PATCH's contract names this "the next increment"
   (`design/contracts/TERRAIN.PATCH.md:257-265`). The on-chip front (≈16 M10K)
   plus a sim-level backing store closes the one missing seam in the organ
   chain and lets the first ever full-chain frame run in Verilator —
   harness-fed pages, no memory subsystem required yet. Laws: terrain_rules
   §4.2, TESS's registered port, PATCH's output port. The SDRAM-backed store
   attaches later without changing the front's ports.
3. **TERRAIN.PAGELOADER contract + RTL against the harness-as-HPS.** Laws all
   written: page format + CRC (§2/§2.1), whole-page streaming (§7), bridge
   port + sim profile, guard discipline. Region base and client id enter as
   parameters so OPEN 2/3 land as constants, not redesign.

Then, in dependency order:

4. **SW.STREAM contract filled + WORLD.DIRECTORY reference implementation**
   (zref-side visible-set builder; needs OPEN 1 for the key law and OPEN 7
   for prefetch policy).
5. **TERRAIN.SEQ** (needs 1–4; needs OPEN 5 for its input records; skip law
   per §2.6).
6. **TERRAIN.WRITEBACK** (needs OPEN 4 — build LAST among the datapaths, since
   its payload may be "nothing for B/D").
7. **Guard map + client amendments** enacted (OPEN 2/3 answers).
8. **The composed shell path**: TERRAIN.SEQ + directory + loader + compcache +
   organs in one Verilator top, harness supplying only the FRAME_RING — the
   first frame where the island is selected by a world manager rather than
   registered by hand.
9. **The 8 km traversal capture** (§6) — the acceptance gate for the layer.

Production-scale geometry storage downstream (GEOM.PARAMBUF, binner capacity)
is Wave A work already ruled and sequenced (`reports/OWNER-RULINGS-20260831.md:148-160`)
and is deliberately not duplicated here.

---

## 6. Validation: an 8 km traversal capture, not a static image

Per `reports/Missingterrain:177-179`, the crucial validation is a capture that
**forces residency churn**: fly fast across ≥8 km of authored world, deform
patches, leave them beyond the 1,024-patch horizon, return, and verify —

* geometry bit-exact against zref on every composed patch, both views;
* scars/breaches identical on return (the writeback/mirror law, OPEN 4, is
  what this measures);
* zero stale-handle consumptions (counted, must be zero *consumed*; stale
  *detections* are expected and healthy);
* LOD/stitch crack-free across every streaming boundary the traversal crosses;
* the counters tell the story: hits/misses/evictions/dirty_evictions/
  collisions/pages_loaded/crc_fails/skips-not-resident, per frame, in the
  capture — run twice, assert identical (the wave-2 discipline,
  `design/contracts/MEM.HPS.BRIDGE.md:69`).

Judged the repo's way: not a handful of stills — per-frame counter
trajectories (a flat collisions line IS "it never thrashed"), and
frames sampled by badness (max skip count, max load latency), per the
CLAUDE.md seeing-the-work rules.

---

## 7. OPEN — needs an owner ruling

Every item below is genuinely unwritten in the tree. Each is one answerable
question. Nothing above builds a law on any of them; where a block touches
one, it refuses loudly or parameterises.

1. **Residency key law:** what is the directory's lookup key — world-absolute
   patch coordinates at one canonical pitch, or (island_id, ix, iz)? The built
   block tags only signed (px, py) with no island_id
   (`zhao_terrain_residency.sv:85-99`), so two islands with overlapping patch
   coordinates would alias, and per-island pitch (`spec/terrain_rules.md:51-56`)
   makes world-absolute patch coordinates ill-defined across pitches. Which
   key is the law, and does the tag gain island_id?
2. **Guard map:** what base and size does each terrain region get in the
   `spec/memory_rules.md` §5 map — page pool, composed-height cache, velocity
   cache, sheets/material pool? (§8 of terrain_rules fixes the sizes; the
   addresses are unwritten.)
3. **Memory client:** do the terrain loader/writeback/compcache get a new
   `zhao_client_e` id (SUNDER proposes client 6, TERRAIN_BUILD,
   `reports/SUNDER.md:615`) or share ENGINE0 — and are they guaranteed
   round-robin or best-effort at the arbiter?
4. **Writeback vs canonical mirror:** on dirty eviction, does the FPGA write
   layers B/D back to HPS DDR, or is SW.CPUCOLL's canonical mirror
   (`spec/terrain_rules.md:462-464`) the reload source, making B/D writeback
   unnecessary — and layer F (surface sheets), which has no stated CPU mirror:
   must F be written back?
5. **Command ABI:** is the hardware terrain path submitted as one
   DrawProcedural-lineage record per visible patch, or as a new
   set-of-visible-patches command? (Permanent ABI — Class C.)
6. **Composed-cache overflow:** when a frame needs more than 256 composed
   patches, what degrades, and in what deterministic order? (§4.2 budgets 256;
   the 257th has no law.)
7. **Prefetch policy:** what look-ahead (in patches, around each camera's
   motion) does the HPS prefetch, and what per-frame page budget is the
   ceiling (the provisional 32 pages/frame ≈ 41 MB/s of terrain_rules §7,
   pending ZH-004 board truth)?
8. **Mip derivation side:** are the 1,480 B/patch LOD mips
   (`spec/terrain_rules.md:109-111`) computed by the HPS into the staged page,
   or by the FPGA loader at page-load time?
9. **Visible-set collision law:** two visible patches 2,048 m apart on both
   axes collide in the direct-mapped directory and would evict each other
   every frame — is that accepted as counted thrash (the built block's
   stance), or must WORLD.DIRECTORY guarantee it never submits a colliding
   pair within one working set (making it software's law)?
10. **Level teardown:** the directory has no invalidate/unload port and its
    resident count never decrements — is full reset the ratified mechanism for
    unloading a world, or does the block gain an invalidate port?

---

## 8. Honest limits

* A dense 8×8 km, 2 m-pitch plate is 318.5 MiB of pages and is **never**
  resident; the design streams a 1,024-patch (≈4.19 km² at 2 m) window and
  counts every eviction. A game that authors a solid 8×8 km plate at 2 m will
  see permanent streaming, and at high traversal speed, counted
  skip-not-resident frames at the leading edge. Sparse islands — the world
  this console is for — fit.
* The direct-mapped directory's 2,048 m collision period is a real, stated
  cost (residency header, lines 47-52); OPEN 9 decides who avoids it.
* Compose throughput is bounded by the Field engine, not by this layer:
  EARTH60 measured one core at 6–13× short of the spec's worst live-field
  case (`reports/EARTH60_CAPACITY.md:88-98`); the world layer feeds patches
  at whatever rate Field can compose and does not pretend otherwise.
* Every bandwidth number above is the frozen SIM profile; board truth (ZH-004)
  may move all of them, and `spec/terrain_rules.md:456-457` forbids freezing
  the streaming figure before it reports.
* Nothing in this document has been synthesized; the residency block's flop
  cost (§2.2) is the first known physical issue for whoever fits it.
