# TERRAIN.SHEETSEAM — the layer-F read path for TERRAIN.BAKE's per-vertex dig

**Module:** `fpga/rtl/terrain/zhao_terrain_sheetseam.sv`
**Companion:** `fpga/rtl/surface/zhao_surface_sheetshare.sv` (the two-client
share of `zhao_surface_sheet`'s single request port — a separate file because
`zhao_console_core.sv` entry I32 says an arbiter written inline in a composer
is "an arbiter nobody can point at")
**Law:** owner rulings **R194** (the seam dig accepted, the page format frozen
at 64×64) and **R221** (the `ST_MISS` law).
**Test:** `tests/terrain/sheetseam_rtl_directed.cpp` — **103 checks**, built and run
(81 before owner ruling R231 added the `before` plane; cases 13 and 14 are its).

---

## 1. What was missing

R194 gave `zhao_terrain_bake_v2` a second depth law — the per-vertex mode on
`cmd_depth_sheet_i`, whose depth comes from layer F through
`zhao_terrain_stampdepth`. The block presents `sheet_texel_o` and samples
`sheet_strength_i`, and its own header says: *"WHO SERVES IT is NOT this block
and is not settled."*

The PAGEIO packet measured the seam against the actual port and recorded, as
its decision 5, that calling it an arbiter **understates it by three items**.
`zhao_surface_sheet`'s `req_*` is a control-and-read port — `OP_ACQUIRE` /
`OP_READ` / `OP_RELEASE`, a 32-bit handle, a separate `pg_*` response stream
carrying `ST_HIT` / `ST_ALLOCATED` / `ST_OVERFLOW` / `ST_MISS` — while bake
wants a **combinational** lookup on the same beat as layers A/B/C. Four things
were missing:

1. a handle and its lifetime;
2. a latency adapter;
3. the arbiter proper;
4. a law for `ST_MISS` — "and this one is not an engineering question".

Item 4 is **R221**. Items 1–3 are this contract.

## 2. Item 1 — the handle's lifetime is that there isn't one

**The reader issues `OP_READ` and nothing else.** Never `OP_ACQUIRE`, never
`OP_RELEASE`. This is forced, not thrifty.

`zhao_surface_sheet.sv`'s `do_acquire_new` sets `dir_live`, starts the
4,096-cycle clear sweep and answers `ST_ALLOCATED`. An `OP_ACQUIRE` from the
bake reader therefore **allocates a blank sheet** — every texel zero, every
vertex uncovered, the record digs nothing. That is R221's explicitly refused
*"dig zero: a visible no-op"*, wearing a residency costume so that no status
code says a miss happened. It also steals one of `Slots = 2` from
SURFACE.STAMP, the only block `spec/terrain_rules.md` §7 allows to write layer
F. `OP_READ` is the only opcode that reports residency **without changing it**.

The 32-bit handle is an **input** (`job_handle_i`), not derived from
`cmd_patch_id_i`, per the store's own choice C4: *"the handle is the identity
the ABI carries (`commands.zidl` SurfaceStamp `handle32[patch] patch`); using
anything else re-derives identity that was already stated."*

**Measured, not claimed:** `sheetseam_rtl_directed` samples the seam's own
`req_op` every cycle of every case. Over the suite: 0 `OP_ACQUIRE`, 0
`OP_RELEASE`, thousands of `OP_READ`, and `res_occupancy_o` byte-identical
across every bake.

## 3. Item 2 — the latency adapter, and the price that moved

PAGEIO priced it at *"1,089 round trips per record, **or** an 8,192-byte second
copy of layer F"*. **The second figure is an overstatement by 7.5×**, and the
correction is what decides the choice:

* **Bake never reads the tag.** `zhao_terrain_bake_v2` has `sheet_strength_i`
  and no tag port. Layer F is {tag u8, strength u8}, so half of the 8,192 bytes
  is a plane this consumer cannot see. 4,096 left.
* **Only 1,089 of the 4,096 texels are addressable.** §9.3(b), as
  `zhao_terrain_stampdepth` states it, is `ti = (vi >= 32) ? 63 : 2*vi`, so the
  33×33 lattice samples a decimated grid — 33 distinct `ti` and 33 distinct
  `tj`. The other 3,007 can never be asked for. *Checked in RTL*: case 0 of the
  directed test walks all 1,089 vertices through the real `stampdepth` instance
  and counts the distinct texels.

**1,089 bytes = 8,712 bits = one M10K**, against the feared 65,536 bits (about
seven) at 306 of 553 in use (R87). It is not a second copy of layer F; it is
**layer F resampled onto the lattice**, a different and much smaller object.

**Taken: the prefetch.** Measured at **1,091 cycles uncontended, 1,092 under
contention**, once per sheet record, off the dig's critical path. A disc record
prefetches nothing and pays nothing.

The per-vertex round trip loses on three counts and only the first is speed:

* ~2 cycles per vertex *in series* with the dig, ~2,178 added, in the worse
  place;
* `sheet_strength_i` is sampled with `vtx_valid_i`, which this block **does not
  own** — `zhao_terrain_pagestream` drives A/B/C and `zhao_terrain_pageio`
  drives `nb_o` — so the blast radius is a port change on the terrain page
  spine rather than one new file;
* **the sheet's latency is not bounded.** `req_ready_o` is low for the whole
  4,096-cycle clear sweep of an allocating ACQUIRE. Per-vertex, a stamp's
  ACQUIRE stalls the dig mid-lattice while holding the A/B/C page beat. The
  prefetch phase absorbs it and the dig never sees it.

### 3.1 The answer is a valid, not an assumption about bake's timing

A 1,089-byte store answering combinationally is **8,712 flops**, eight times
`zhao_terrain_pageio`'s shadow plane. A synchronous M10K answers one cycle
late. Bake leaves three cycles between advancing the cursor at `StEmit` and
raising `vtx_ready_o` at `StVtx` (`StVxM`, `StVxC`, `StDxM`), so a registered
read would always be in time — **and relying on that is a coupling to a
traversal**, the exact trade `zhao_terrain_pageio` wrote down and refused for
its own shadow plane.

So the block publishes `str_valid_o`, which the composer ANDs into bake's
`vtx_valid_i`. Bake's own port comment already specifies that contract: *"the
page server delivers all five together or the vertex is not ready."*
`dig_stall_cycles_o` **measures** the difference instead of asserting it: it
reads 0 at bake's real traversal, and it was fired deliberately before that
zero was quoted.

`str_valid_o` is **high throughout** a record this block is not serving, so a
disc record — or a record that fell back under R221 — is never slowed.

## 4. Item 3 — the arbiter, and why its policy is not a new decision

`zhao_surface_sheetshare` is a typed two-client share. The policy is
`zhao_terrain_psmux`'s **round robin**, one `last_q` flip-flop, adopted rather
than re-argued. Both priority orders are rejected with their numbers:

* priority to the stamp costs the prefetch ~13 cycles of 1,089
  (`design/blocks.yml`: "one texel per ~83 clocks") and is unbounded in a burst;
* priority to the bake locks the stamp out for 1,089 consecutive cycles on the
  player's own action.

Round robin bounds both at one beat, so neither number has to be trusted.
Measured: the stamp waited **0 cycles**; the prefetch went 1,091 → 1,092.

**Granularity is one transaction, not one job.** `zhao_surface_sheet` accepts a
request whenever its single response slot is free, so exactly one transaction
is outstanding and the response stream is strictly in issue order. The share
re-arbitrates every beat, and the bake's 1,089-beat prefetch interleaves with
the stamp at single-beat granularity.

**`can_grant_c` is load-bearing.** The store accepts a request in the cycle it
hands back the previous response — it sustains one read per clock. An arbiter
that waits for `!busy_q` halves that, turning the prefetch into 2,178 cycles
with every handshake legal and every counter agreeing. The first version of
this file did exactly that and it was found by measurement, not by review.

**It does not touch the sheet's write port.** §7 gives layer F one writer, and
`zhao_surface_sheet`'s choice C5 makes the write port physically separate, so a
stamp's writes are never delayed by a bake's reads.

## 5. Item 4 — owner ruling R221, the `ST_MISS` law

> **RULED: fall back to the parametric disc, and COUNT the fallback.**

The fallback is **one bit**: `bk_depth_sheet_o` goes low and
`zhao_terrain_bake_v2` runs the law it always ran. R221's reasoning is the
whole implementation — SEAMDIG measured the sheet mode additive
(`terrain_bake_v2_directed` 267/267 unchanged), so a fallback record is
bit-identical to the same record with `cmd_depth_sheet_i` low, and that
equivalence is 267 checks that already exist rather than an argument.

**Any non-`ST_HIT` fails the record.** `ST_ALLOCATED` and `ST_OVERFLOW` are
unreachable from here (§2) and both would mean a blank sheet.

**A miss anywhere in the 1,089 reads fails the WHOLE record**, not the vertex.
Mixing laws inside one crater would invent a fourth shape — half sheet, half
disc, with a seam nobody authored — and R221 forbids inventing a third. It also
means a sheet RELEASEd mid-prefetch lands on the ratified law rather than on a
half-read page, and `miss_texels_o` records how far the fill got.

`fallbacks_o` is R221's mandated counter. Its positive control is **stimulus,
not a mutant**: a miss is legally reachable at this block's own port, which is
the distinction `zhao_terrain_pageio` draws between its five stimulus-fired
counters and `wq_overflow_o`.

## 6. The record-swap defect is designed out, twice

`pf_handle_q` is latched by this block at fill start and differenced against
the handle the producer is **offering**; `rd_texel_q` is loaded by this block's
read issue and differenced against the texel **bake's cursor** drives. Both
comparisons have their two sides loaded by different enables — CLAUDE.md's
lockstep chapter applied before the fact rather than after it. A record that
moves under a running fill is caught and refetched rather than digging one
patch's crater from another patch's sheet.

## 7. What is NOT decided here, and what composition still needs

**This block is BUILT and NOT COMPOSED**, for the same reason
`zhao_terrain_pageio` is: its consumer `zhao_terrain_bake_v2` is not composed,
and bake still cannot be — **`cmd_*` has no producer.** `zhao_terrain_cmd`
emits a patch *directory* record (`rec_island_o` / `rec_ix_o` / `rec_iz_o` /
`rec_hps_addr_o` / `rec_crc_o` / `rec_flags_o`), not a bake record
(`{cx, cz, radius, depth_from, depth_to, env_*}`). Re-verified in this tree.

**`job_handle_i` therefore has no producer either, and that is the same gap.**
Whatever block eventually emits bake records must emit the patch's sheet
handle32 beside them; it must not be synthesised from `cmd_patch_id_i` (§2).

> **The VALUE, though, is already on the console's wires.** Measured
> 2026-09-21 (terrcmd): `zhao_cmd_exec`'s `stamp_patch_o` is the 32-bit
> `handle32[patch]` lifted off a CRC-validated SurfaceStamp packet, composed in
> `zhao_console_core` as `cmd_exec_stamp_patch_w`. Carrying that beside the
> record satisfies §2/C4 exactly — it is the ABI's own identity, not a
> synthesis — so the constraint above is **satisfiable and needs no new
> identity law**. Only the carrier is missing.

> **DISCHARGED 2026-09-21 (deltalaw) — OWNER RULING R231 TOOK THE DELTA.**
> The note below was right, and the cost it recorded before the decision was
> close but LOW. What was actually built, and what it actually costs:
>
> * **`sheet_before_o`**, beside `sheet_strength_o`. Bake ACCUMULATES, so what
>   is added to it must be a CHANGE; the pair `{before, after}` is what S3 has
>   said this seam owes its consumer since the stamp was written.
> * **A `stamp_results` SINK (`sr_*`)** — `surf_res_before_o`'s FIRST CONSUMER
>   IN THIS TREE. The PAGEIO packet had measured *"ZERO CONSUMERS of
>   `res_texel_i` / `res_strength_i` / `res_before_i` in `fpga/` OR `tests/`"*;
>   that stops being true here. `zhao_surface_stamp` gains `res_handle_o` so
>   the stream can be ROUTED to a patch — `st_handle` presented, not a new
>   register and not a derived identity, which is choice C4 satisfied by
>   CARRYING as §7 below says it can be.
> * **`sr_ready_o` is constant high**, and that is a decision: the stamp's rate
>   budget is one texel per clock and it is the PLAYER'S OWN ACTION. A seam
>   that backpressured it would drop frames to protect a bake. A result the
>   plane cannot HOLD is dropped and COUNTED (`zhao_terrain_psmux`'s rule).
>
> **THE PRICE, CORRECTED.** The note says "one more 1,089-byte M10K half", and
> R231 records that figure. The plane is **1,089 × 9 bits** — a ninth `seen`
> bit per entry, not eight — so it is 9,801 bits rather than 8,712, plus a
> 32-bit handle, an 11-bit occupancy count and two flags. Nine bits is free in
> M10K terms (the word COUNT is what picks the block, and 1,089 words is the
> same either way), so the figure is right in blocks and light in bits. It is
> recorded corrected rather than quietly accepted.
>
> **AND A NOTE ON "ONE M10K", WHICH IS THIS FILE'S OWN CLAIM AND IS NOT YET
> MEASURED.** §3 says 1,089 bytes "= 8,712 bits = one M10K". An M10K is 1,024
> words deep in ×8/×10 mode and the plane is **1,089 words**, so depth — not
> bit count — may force a second block, and there are now two planes. This
> packet may not run Quartus, so it is flagged rather than decided:
> `F-SHEETSEAM1` already carries `min_m10k` as well as `max_m10k` precisely
> because nobody has measured whether these stores infer RAM at all. **Do not
> quote "one M10K" as measured.**
>
> **THE PLANE CLEARS ON CONSUME**, and two simpler schemes were designed and
> rejected with their reasons (the RTL header carries them):
> * a clear-at-`bake_done` **sweep** has a 1,089-cycle window in which an
>   arriving stamp is silently wiped — it UNDER-digs, which is R221's refused
>   *"dig zero: a visible no-op"*;
> * an **epoch tag** aliases (a 1-bit toggle mistakes epoch N−2 for N, and
>   widening only moves the period); an aliased entry reads as `seen` with a
>   stale `before` and DOUBLE-DIGS — the very defect R231 repaired,
>   reintroduced by the instrument meant to prevent it.
>
> Clearing on the dig's own read has neither hazard and costs nothing: the dig
> visits every vertex, so it retires exactly the deltas the scar just absorbed.
> A dig abandoned mid-lattice leaves the vertices it never reached still
> `seen`, which is right — they have not been dug. **Idempotence is a
> structural property of this block, not an assertion about it.**
>
> **NO COLD-START HOLE.** A patch is only baked BECAUSE something stamped it,
> and those stamps arrive here first reporting `before = 0` on a fresh sheet.
> `kStampDepthTable[0]` is 0, so the first bake digs the full depth exactly as
> the absolute law did.
>
> **TWO NEW WAYS TO REFUSE, BOTH ON R221's EXISTING LAW AND NEITHER A NEW ONE.**
> `bk_depth_sheet_o` also requires that the plane is THIS record's or empty
> (`bf_ok_c`), and that no result has been dropped since it was emptied
> (`bf_torn_q`). Without the first, a record would dig its crater from another
> patch's pre-blend strengths — §6's record-swap defect through the door R231
> opened, and invisible to every counter for the same reason R231 itself was.
> `bf_torn_q` clears on ANY retirement, served or fallen back: gated on a
> SERVED dig it could never clear at all, because a fallback leaves `serve_q`
> low, and the sheet law would be silently dead for the life of the machine
> with every counter agreeing.
>
> *The original note follows, unedited, because the cost it recorded before the
> decision is exactly what it was for.*
>
> **AND ONE THING THIS BLOCK SHOULD KNOW ABOUT ITS OWN READ (D-TERRCMD-A).**
> `design/contracts/SURFACE.STAMP.md` S3 rejects, by name, *"emitting only the
> new value and letting BAKE re-read — a second reader on a store whose whole
> rate budget is one texel per clock"*, because TERRAIN.BAKE *"needs the DELTA,
> not just the new value"*. **This block is that second reader, and it reads
> `strength_after`.** Since bake accumulates (`scar += delta`) while
> `stamp_depth_at_vertex` is absolute, a re-issued stamp digs twice. Whether
> the law is the delta or the absolute is an **owner decision**, written up in
> `design/contracts/TERRAIN.BAKE.md` and in `zhao_console_core.sv` entry I32.
> If the delta is chosen this block needs a `before` plane beside its prefetch,
> which is one more 1,089-byte M10K half — recorded now so the cost is known
> before the decision rather than discovered after it.

Composing this seam alone would connect a port to a block nothing drives, so it
is refused here rather than taken quietly.

**DELETE THIS SECTION when TERRAIN.BAKE composes.**

## 8. Counters

| port | fires when | control |
|---|---|---|
| `jobs_o` | a record is admitted to bake | stimulus |
| `sheet_served_o` | ... on the layer-F law | stimulus; zero on a fallback |
| `fallbacks_o` | **R221**: asked for the sheet, got the disc | ghost handle, mid-fill RELEASE, real `Slots = 2` overflow |
| `miss_texels_o` | a non-`ST_HIT` response | the same |
| `prefetch_beats_o` | a texel read in | every sheet record |
| `refetches_o` | the offered job moved under a fill | handle swapped mid-fill |
| `dig_stall_cycles_o` | bake was ready and we were not | `dig_ready` held through a jumping cursor |
| `bad_texels_o` | an address no lattice vertex can produce | an odd texel |
| `stray_done_o` | `bake_done` with no record in flight | a bare pulse |
| `before_texels_o` | **R231**: a `stamp_results` beat absorbed into the plane | stimulus; fired at 1,089 in case 13 |
| `sr_dropped_o` | ... and one the plane could not hold | a result for a second handle while the first is live |
| `before_torn_o` | a RECORD diverted to the disc because of a drop | the same, then a record offered |

`sr_dropped_o` and `before_torn_o` are two counters and not one deliberately:
the first says a RESULT was lost, the second says a RECORD paid for it. A
design that loses a result for a patch nobody bakes moves only the first. That
is R95's *discriminate, do not merely move*, and case 14 pins it by asserting
`miss_texels_o` is still zero while `before_torn_o` reads one — so the fallback
cannot be misread as a residency problem.

Every one is asserted **silent** on the clean path and then **fired**. The one
structural invariant that no legal input can reach — two reads in flight —
carries a simulation assertion and **not** a counter, because a counter no
legal input can move owes a committed mutant and this is a statement about the
composition rather than about the design's own reachable states.
