# Contract — SURFACE.SHEET (Surface sheet store)

> Ledger: `design/blocks.yml` · owner ZH-031 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

The resident store for **layer F** — the per-patch 64×64 `{tag u8, strength u8}`
surface sheet of charter §12 and `spec/terrain_rules.md` §2 — with residency
tracking. RTL: `fpga/rtl/surface/zhao_surface_sheet.sv`.

**In scope:** the texel store, the residency directory, the clear-on-allocate
sweep, the read port, the write port, and the `surface_texels_touched` counter.

**Out of scope, deliberately:** no VRAM port and no page loader (`MEM.GUARD`
owns the 21,376 B page, and `terrain_rules` §7 makes the streaming path
HPS → VRAM rather than fabric); no writeback of a dirty sheet; no blend
arithmetic (`SURFACE.STAMP` owns the vocabulary, and `zhao_surface_blend`
owns the arithmetic); no draw-time sampling (`zref::render::sample_sheet` and
`TEXTURE.MOSAIC` own that); no compression or residency trimming (charter §12
mentions both as future work and neither is specified anywhere).

## Law FOUND versus law CHOSEN

**FOUND, and obeyed:**

| Law | Source |
|---|---|
| the sheet is 64×64 texels of `{tag u8, strength u8}`, 8,192 B per patch | charter §12; `spec/terrain_rules.md` §2 layer table |
| it PERSISTS across frames | charter §12 ("persistent"); the reference renderer keeps `sheets_` alive across `render_frame` calls |
| a sheet that has never been stamped reads ZERO everywhere | `zref::render::SurfaceSheet` is value-initialised; `SoftwareRenderer::sheet_for` "creates on first use" |
| layer F is written by `SURFACE.STAMP` and by nothing else | `spec/terrain_rules.md` §7 |
| overflow REJECTS the stamp and never partial-writes | `design/blocks.yml` `notes:` for this block |
| the resident world set is 1,024 patches, 14.38 MiB of sheets pool | `spec/terrain_rules.md` §1.2, §8 |

**CHOSEN.** The ledger's `notes:` says "Residency policy per §11".
`spec/terrain_rules.md` §11 is the list of things that section **explicitly does
not decide**; it contains nothing about sheet residency. There is therefore no
law to find, and every rule below is a choice with its rejected alternative
recorded. All five are also argued in the RTL header and in
`reference/include/zref/zref_surface.hpp`.

**C1 — fully associative, keyed by the 32-bit patch handle, `Slots` slots
(default 2).**
*Rejected:* direct-mapped on the handle's low bits. One comparator instead of
`Slots`, and two patches whose handles collide evict each other every frame,
destroying the one property (persistence) the block exists for. At `Slots = 2`
the associativity costs two 32-bit comparators.

**C2 — never evict.** An `ACQUIRE` that finds no free slot answers `OVERFLOW`
and changes nothing: no slot is stolen, no handle rewritten, no texel touched.
*Rejected:* LRU eviction. It needs recency state that a capture does not carry,
so replay stops being exact, and it can silently discard a scar the player can
see.

**C3 — a freshly allocated slot is cleared by a 4,096-cycle sweep, and the
`ACQUIRE` response is withheld until the sweep finishes** (this is why the
ledger's latency is `variable`). **Re-acquiring a resident handle does NOT
clear** — that single line is persistence.
*Rejected:* a per-texel `present` bit with a clear-word bypass, which is what
`design/contracts/RASTER.TILESTORE.md` chose for its 256-word banks. Here it
costs 4,096 × `Slots` flops (8,192 at the default) plus a 3-way output mux, to
save 4,096 cycles once per patch per frame — 0.25 % of a 1.67 M-cycle frame.
Flops are the scarcer resource at this scale; the trade inverts at TILESTORE's,
which is why the two blocks answer differently.

**C4 — reads and writes name the HANDLE, not the slot.** The associative lookup
happens on every access.
*Rejected:* returning a slot index from `ACQUIRE` and indexing with it. One
comparator cheaper, and a stale index from a previous frame writes into another
patch's scars. The handle is the identity the ABI already carries
(`commands.zidl` `handle32[patch] patch`).

**C5 — the read port and the write port are separate.** One simple dual-port
M10K offers exactly one read and one write per clock, and the ledger asks for
one sheet texel per clock while `SURFACE.STAMP` is a read-modify-write engine.
A single shared port halves the rate to one texel per two clocks and **misses
the ledger target**. Read-during-write at the same address returns the **old**
word (both accesses live in one `always_ff`). `SURFACE.STAMP` never does that —
its cursor marches forward and its write trails its read by two texels — but
the semantics are stated rather than left to the synthesiser.

## Clock and reset semantics

Single clock `clk`, `gpu` domain, no CDC. Reset `rst_n`, async assert / sync
release, `always_ff @(posedge clk or negedge rst_n)`, matching every other block
in this tree.

On reset: the directory is empty (`res_occupancy_o` = 0), no sweep is running,
`pg_valid_o` is low, `wr_miss_o` low, `surface_texels_touched_o` = 0, `idle_o`
high. **The texel array is deliberately NOT reset** — a reset loop over the
array is exactly what stops M10K inference — and C3's sweep makes the initial
contents unobservable: no read is served for a handle that has not been
acquired, and an allocating acquire zeroes the slot before it answers.
`surface_sheet_directed` checks all 4,096 texels of a freshly allocated sheet.

## Input and output packet layouts

### `sheet_requests` (in, ready/valid)

| Port | Width | Meaning |
|---|---:|---|
| `req_op_i` | 2 | `0` ACQUIRE, `1` READ, `2` RELEASE |
| `req_handle_i` | 32 | `handle32 {index:24, generation:8}`, the patch |
| `req_texel_i` | 12 | `j*64 + i`, scan order (READ only) |
| `req_src_id_i` | 16 | source id, rides its own response |

### `sheet_pages` (out, ready/valid)

| Port | Width | Meaning |
|---|---:|---|
| `pg_op_o` | 2 | echoes `req_op_i` |
| `pg_status_o` | 2 | `0` HIT, `1` ALLOCATED, `2` OVERFLOW, `3` MISS |
| `pg_tag_o` | 8 | layer F tag on a served READ, else 0 |
| `pg_strength_o` | 8 | layer F strength on a served READ, else 0 |
| `pg_src_id_o` | 16 | the request's source id |

A **missed** READ answers zero, which is the same value a resident-but-untouched
texel answers. The STATUS distinguishes them, and a consumer that ignores status
gets the fail-safe reading (nothing was stamped there) rather than another
patch's scar.

### the write port (in, ready/valid)

`wr_handle_i` (32), `wr_texel_i` (12), `wr_tag_i` (8), `wr_strength_i` (8),
`wr_we_tag_i`, `wr_we_strength_i` (byte enables, so a blend that only moves
strength leaves the tag alone), `wr_src_id_i` (16). No response stream; a write
naming a non-resident handle is **dropped** and raises `wr_miss_o` for one cycle
with `wr_miss_src_id_o`.

### `residency_status` (out, combinational)

`res_occupancy_o[Slots-1:0]` (bit *s* = slot *s* is live), `res_busy_o` (a clear
sweep is running), `res_overflow_o` (1-cycle pulse on a rejected ACQUIRE).

## Backpressure rules

`req_ready_o` = the response register is free **and** no sweep is running **and**
no deferred answer is queued. `wr_ready_o` = no sweep is running. `pg_valid_o`
holds its word until `pg_ready_i` — the read-data register's enable is gated by
the accept, so a stalled response does not lose the word.
`surface_sheet_directed` holds `pg_ready_i` low for 20 cycles and checks the
word is still there and still correct.

The two ports are independent: a read and a write are accepted in the same cycle
(checked directly).

## Memory ownership

`Slots × 4,096 × 16` bits, one array, no VRAM port. At the default `Slots = 2`
that is 131,072 bits ≈ 13 M10K on a Cyclone V — **the block's entire memory
bill**, and the first fit report should retune `Slots` rather than the logic.
Word layout `{tag[15:8], strength[7:0]}`, byte enables on the two halves.

The 14.38 MiB pool of `terrain_rules` §8 lives in VRAM and is not modelled here;
this block is the fabric-side resident window onto it. Nothing in this tree
states how many sheets that window should hold, which is why `Slots` is an
elaboration parameter.

## Q formats and rounding

**None.** Every value is a raw byte: `tag` is an opaque effect/material id and
`strength` is a `U 0.0.8` magnitude that this block only stores and returns. No
arithmetic happens here at all, and stating that is itself worth doing — the
rounding law lives entirely in `SURFACE.STAMP`.

## Latency (fixed or variable)

**Variable**, as the ledger records:

- READ / RELEASE / a HIT ACQUIRE: the answer is registered at the accept edge,
  so it is readable the next cycle — one cycle.
- an ALLOCATING ACQUIRE: **4,097 cycles** (the 4,096-cycle clear sweep, then the
  answer). `surface_sheet_directed` measures the sweep as exactly 4,096 cycles
  and checks the request port is refused for every one of them.

## Target throughput

Ledger: **1 sheet texel per clock**. **Met, measured:** 256 pipelined reads in
**257 cycles** (`surface_sheet_directed`), i.e. one per clock plus the
pipeline fill. The write port accepts one texel per clock independently.

## Overflow and malformed-input behaviour

- **ACQUIRE with no free slot** → `OVERFLOW`, `res_overflow_o` pulses, nothing
  is evicted, nothing is written. The requester must then write nothing at all;
  `SURFACE.STAMP` enforces that structurally (its texel loop cannot start before
  the acquire answers) and this block enforces the other half by dropping any
  write for a non-resident handle.
- **READ / RELEASE of a non-resident handle** → `MISS`, data zero, directory
  unchanged. A double release misses.
- **WRITE for a non-resident handle** → dropped, `wr_miss_o` pulses, the counter
  does **not** advance. A counter that disagreed with what the sheet holds would
  not be observability.
- **`req_op_i == 3`** is unassigned; it is decoded as none of the three and
  answers `MISS` without changing anything.

## Counters and traces

`surface_texels_touched_o` (32-bit, saturating at `0xFFFF_FFFF`) counts
**accepted writes that landed**. Cleared only by reset. `wr_miss_o` +
`wr_miss_src_id_o` are the trace for a dropped write; `res_overflow_o` for a
rejected acquire. `source_ids: true` is honoured on the request port
(`pg_src_id_o` rides its own response) and on the write port's miss trace.

## Scalar reference function

`zref::surface::SheetStore` in `reference/include/zref/zref_surface.hpp`. It is
the **first** implementation of the residency law (there was none), so it is not
a view onto anything — the RTL and it were written to the same argued rules and
the randomized differential holds them together. The 64×64 byte pair itself is
`zref::surface::Sheet`, byte-identical in content to the existing
`zref::render::SurfaceSheet`.

**The ledger's `reference_model` was AMENDED, from `zref::SurfaceSheet` to the
symbol above.** That is a deviation from "honour the ledger entry" and it is
recorded as one, in the same shape `design/contracts/TERRAIN.PATCH.md` records
its own. The reason: `zref::SurfaceSheet` names nothing anywhere in this tree,
and the symbol it was presumably reaching for — `zref::render::SurfaceSheet` —
is the STORAGE (two byte arrays, no residency, no directory, no clear), while
this block's model is the resident directory *plus* that storage. Citing the
struct would have made the residency law — every choice C1..C5 above, and the
only part of this block anybody could get wrong — belong to no reference model
at all.

## Directed tests

`tests/surface/surface_sheet_directed.cpp`: reset state; a fresh ACQUIRE
allocates and **all 4,096 texels read zero**; write/readback with each byte
enable and both neighbours checked untouched; re-acquire HITS and does **not**
clear; a second handle takes the second slot and writing it leaves the first
alone; a third handle OVERFLOWS and evicts nothing and both incumbents still
hold their contents; a write for a non-resident handle is dropped with the miss
pulse and lands in neither sheet; RELEASE frees the slot, a double release
misses, and re-acquiring after release ALLOCATES and clears while the
un-released sheet keeps everything; the clear sweep is exactly 4,096 cycles with
the request port refused throughout; 256 pipelined reads in 257 cycles; the
response held under 20 cycles of backpressure; a read and a write accepted in
the same cycle.

## Randomized differential tests

`tests/surface/surface_sheet_random.cpp`, two lanes against
`zref::surface::SheetStore`:

- **Lane A, gameplay-shaped.** Two handles — the resident set — held for many
  operations, written in stamp-sized runs, read back, occasionally re-acquired.
  Overflow is impossible here by construction. This is where a persistence
  defect shows up and nothing else does.
- **Lane B, at the residency limit.** Five live handles over two slots, constant
  acquire/release churn, texels deliberately at both ends of the 0..4,095 range.
  Overflow and miss are the common case.

Both lanes end with a full sweep over every resident sheet. **Coverage is
asserted, not printed:** lane A must have allocated both slots, re-acquired a
resident handle, written texels, read back non-zero contents and never
overflowed; lane B must have overflowed, read a non-resident handle, dropped
writes, released slots and touched both texel 0 and texel 4,095.

## Composition

`tests/surface/surface_stamp_chain.cpp` runs this block against the real
`SURFACE.STAMP` — see that block's contract. Two cases here are specifically
this block's: the real 4,096-cycle sweep stalling the real stamp (the recycled
slot must be clean even though the array still holds the previous tenant's
255s), and two patches resident at once so the slot field is observable through
the pair.

## Formal properties

**None, deliberately.** There is no bounded arithmetic core here to state
anything about: the block is a directory, an address decoder and an 8,192-word
array. The properties that matter — "a write for a non-resident handle changes
no memory", "an overflow evicts nothing" — are quantified over the whole array
and over sequences of acquires, which is a memory-equivalence obligation rather
than a bounded arithmetic one; a BMC over 131,072 bits of state would prove it
to a horizon and call it a theorem. The randomized differential against
`SheetStore` covers exactly those obligations over 6,000 operations per lane in
the nightly run, and the mutation evidence below shows the lanes are load-bearing.

The arithmetic that *does* admit a real theorem lives next door and has one:
`tests/formal/surface_blend.sby` proves the blend TOTAL over all 4,194,304 of
its inputs.

## Synthesis / resource ceiling

Budget group `geometry_mantle`. **Estimate only — this block has not been
synthesized:** no Quartus fit, no timing closure, no device numbers, and it is
deliberately not in `fpga/files.qip`. Shape: `Slots × 4,096 × 16` bits of block
RAM (131,072 bits ≈ 13 M10K at the default), `Slots` 32-bit comparators, a
12-bit sweep counter, a 32-bit saturating counter and roughly 120 flops of
directory and response registers. **The RAM is the cost**, and `Slots` is an
elaboration parameter precisely so a fit that needs the memory back trades
residency for it — and C2's reject-never-evict makes that trade *safe* rather
than a corruption.

One thing to watch at fit time, unmeasured: the associative lookup sits
combinationally in front of the RAM address on both ports.

## Integration capture cases

None yet. The captures that already exercise layer F go through the software
console (`tests/render/render_golden.cpp`'s crack ring on patch 44,
`tests/render/render_heightfield.cpp`'s annulus); a fabric-side capture case
arrives with the `TERRAIN.BAKE` seam.

## Mutation evidence

Each mutation was applied alone, rebuilt, and the **test binary re-hashed** to
prove the relink actually happened before the result was believed.

| # | Mutation | Caught by |
|---|---|---|
| 5 | a resident re-ACQUIRE also starts the clear sweep (persistence dropped between frames) | `surface_sheet_directed`, `surface_sheet_random` (both lanes), `surface_stamp_chain` |
| 7 | the write address drops the SLOT bits (the scar lands in the wrong sheet) | `surface_sheet_directed`, `surface_sheet_random`, `lint_surface_sheet`, and — after the gap below was closed — `surface_stamp_chain` |

**Finding, recorded rather than quietly fixed.** Mutation 7 was originally NOT
caught by the composition, because every chain case stamped a single patch and
slot 0 is exactly the address a missing slot field decays to. A case holding two
patches resident and stamping both was added; the mutation then fails the chain
as well. A composition that only ever uses one slot cannot see the slot field.

## Notes

Residency policy per §11 — see **Law FOUND versus law CHOSEN** above: §11 of
`spec/terrain_rules.md` is the *not decided* list, so the policy is chosen here
and argued, not inherited. Overflow rejects the stamp, never partial-writes.
