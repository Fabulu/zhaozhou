# Contract — TERRAIN.BAKEREC (the patch-bake record producer)

> Ledger: `design/blocks.yml` id `TERRAIN.BAKEREC` · owner ZH-036 · phase 7
>
> Implemented as `fpga/rtl/terrain/zhao_terrain_bakerec.sv`.
> Composed in `zhao_console_core.sv` as `u_terrain_bakerec`, 2026-09-21, in the
> same commit as `zhao_terrain_pageio`, `zhao_terrain_sheetseam`,
> `zhao_terrain_bake_v2`, `zhao_surface_sheetshare` and a second
> `zhao_terrain_psmux`.

## 1. Why this exists

`zhao_console_core.sv` entry I32 measured `zhao_terrain_bake_v2`'s `cmd_*`
intake field by field and closed with **"WHAT IS MISSING IS A THIRD BLOCK
BETWEEN THEM."** Both halves it sits between already existed:

* **the record** — `spec/commands.zidl` SurfaceStamp 0x0210, `implemented`,
  carrying `handle32[patch] patch`, the transform whose translation is the
  centre, `fx16 radius` and `u16 strength`;
* **the executor arm** — `zhao_cmd_exec`'s `EX_STAMP`, nine ports off a
  CRC-validated packet.

Neither of them produces a **patch-bake record**: one
`{patch, centre, radius, envelope, layer flags}` per patch per frame, together
with the **page identity** — slot, generation, epoch — that `TERRAIN.PAGEIO`
needs to find the page at all. Nor does anything sequence the three page agents
a bake needs. That is this block.

## 2. Ports

```
  clk, rst_n, frame_start_i

  ---- the stamp, on SURFACE.STAMP's own accept -------------------------
  st_fire_i, st_patch_valid_i
  st_handle_i[31:0]                     handle32, the ABI's identity (C4)
  st_patch_ix_i, st_patch_iz_i          signed 16, the dispatch's key
  st_cx_i, st_cz_i, st_radius_i         signed 32, world fx16
  st_env_x0_i / _z0_i / _x1_i / _z1_i   signed 32, the R45 envelope
  st_src_id_i[15:0]

  ---- the page, WATCHED off TERRAIN.HDRREAD's forward handshake --------
  pg_fire_i, pg_ix_i, pg_iz_i, pg_slot_i, pg_gen_i, pg_epoch_i, pg_flags_i

  ---- TERRAIN.PAGEIO ---------------------------------------------------
  io_valid_o / io_ready_i / io_slot_o / io_gen_o / io_epoch_o / io_src_id_o
  io_serving_i                          <- pageio.serving_o (THE INTERLOCK)
  io_done_valid_i / io_done_ready_o

  ---- TERRAIN.SHEETSEAM ------------------------------------------------
  job_valid_o / job_ready_i / job_handle_o / job_want_sheet_o / job_src_id_o
  bk_valid_i / bk_ready_i / bk_fallback_i

  ---- the fields TERRAIN.BAKE reads ------------------------------------
  cmd_patch_id_o, cmd_cx_o, cmd_cz_o, cmd_radius_o,
  cmd_depth_from_o, cmd_depth_to_o,
  cmd_env_x0_o / _z0_o / _x1_o / _z1_o, cmd_dual_o, cmd_cells_o, cmd_src_id_o

  ---- TERRAIN.PAGESTREAM, through a psmux client -----------------------
  ps_j_valid_o / ps_j_ready_i / ps_j_slot_o / ps_j_gen_o / ps_j_epoch_o /
  ps_j_src_id_o / ps_j_flags_o / ps_done_valid_i / ps_done_ready_o

  bake_done_i                           <- bake.bake_done_o

  ---- evidence ---------------------------------------------------------
  stamps_seen_o, records_queued_o, coalesced_o, overflow_o, unplaced_o,
  records_issued_o, records_retired_o, records_retried_o,
  records_dropped_o, aged_out_o, stray_done_o, idle_o
```

`cmd_valid_i`/`cmd_ready_o` on `zhao_terrain_bake_v2` are **not** this block's.
They belong to `zhao_terrain_sheetseam`, which sits in the middle of the record's
path and decides exactly one of its fields — R221's `bk_depth_sheet_o`.

## 3. What it MUST NOT own

* **The handle.** `job_handle_o` is `st_handle_i` **carried**. `SURFACE.SHEET`
  choice C4: *"the handle is the identity the ABI carries; using anything else
  re-derives identity that was already stated."* Entry I32 names synthesising it
  from `cmd_patch_id_i` as the cheap mistake to avoid.
* **The envelope.** `zhao_surface_dispatch` computes it under ruling R45 and
  `zhao_surface_stamp` is fed from the same wires; this block carries those four
  fx16 and computes nothing.
* **Residency.** It never looks a patch up and never pins one. It **watches**
  `zhao_terrain_hdrread`'s forwarded job — a page the compose engine has already
  claimed, pinned and read the header of. A slot that moves underneath a record
  is caught by `zhao_terrain_pageio`'s own `stale_gen_o` / `jobs_refused_o`, not
  by a check invented here.
* **Arbitration.** The lattice pass goes through a SECOND `zhao_terrain_psmux`
  instance in the composer, chained under the first. No mux is written inline.
* **Arithmetic.** There is none, which is why the ledger row carries no
  `reference_model:` — see that row's note.

## 4. The disc depths — read this before changing them

`cmd_depth_from_o` and `cmd_depth_to_o` are `FALLBACK_DEPTH_FROM` /
`FALLBACK_DEPTH_TO`, **zero by default**, and the RTL header carries the full
argument. In short:

Every record this block emits is a **sheet** record (`job_want_sheet_o` high),
because a stamp's dig law is R194's per-vertex layer-F read as **R231** amended
it to a **delta**. Bake reads the disc depths on exactly one path: R221's
`ST_MISS` fallback. Three things could be put there and two are wrong.

| | |
|---|---|
| **rejected** | `depth_to = stamp_depth(strength)`, the art table on the command's own strength. A ratified law, and the wrong one here: `scar_sum = h_scar + delta16` **accumulates**, so an absolute depth digs the full crater a second time. That is the defect R231 repaired, reintroduced where it would be rare, unlogged and indistinguishable from terrain. |
| **rejected** | a fabricated `from`/`to` pair. The ABI has no depth field; anything here is an art value invented inside a composition packet. |
| **taken** | **zero, with a retry.** A fallback record digs nothing, is counted at both ends (`fallbacks_o` at the seam, `records_retried_o` here) and is **re-queued**. |

**The retry is exact, not hopeful.** `zhao_terrain_sheetseam` does not consume
its `before` plane on a fallback: `bf_live_q` is cleared only `if (serve_q)` and
a `seen` bit is retired only by a *served* read. So the pre-blend strengths
survive and the next issue digs the whole delta, once. A miss is an eviction or
a mid-fill release — **transient by construction**.

That makes R221's fallback a **deferral** here rather than a second crater
shape, and `spec/terrain_rules.md` §9.2 item 3's identity is what licenses it:
*applying from→mid then mid→to == from→to*, which is EXACT under the delta law
(`tests/terrain/bake_delta_idempotence_directed.cpp` case 4) and is precisely
what entry I32's **D-TERRCMD-C** says the absolute law could not give.

Bounded: after `RETRIES` re-issues the record is dropped and
`records_dropped_o` fires. A stamp whose sheet never becomes resident is a lost
scar and must be **loud**, not a queue that never drains.

## 5. Coalescing is the delta law's consequence, not a convenience

Two stamps on one patch inside one frame produce ONE record; the second updates
the first's geometry in place. Under R231 that is exact: layer F holds the
**accumulated** `after` and the seam's plane holds the **first** pre-blend
`before` for every texel either stamp touched, so one dig of `after − before` is
the sum of both. Under the absolute law it would have been wrong — and the same
code would have looked correct.

It is also what keeps `BAKE_PATCH_BUDGET` meaningful: a budget counted in
patches is only a budget if a patch costs one record.

**The record UNDER SERVICE is excluded from coalescing.** Its fields are held
stable for the seam under the ready/valid contract, and a stamp that rewrote
them mid-prefetch would hand bake a record the seam never saw. Such a stamp
opens a SECOND record, which the before-plane makes correct: the first dig
retires only the `seen` bits it read.

## 6. The sequence, and the one interlock that is not optional

A bake needs three agents live at once and they do not start together.

1. **`TERRAIN.PAGEIO`** — the job, then ~1,200 clocks of layer-D read and
   no-bake scatter before its face is live.
2. **`TERRAIN.SHEETSEAM`** — the job, then 1,089 prefetch reads through
   `zhao_surface_sheetshare` before it admits the record to bake at all.
3. **`TERRAIN.PAGESTREAM`** — the lattice pass that pushes layers A/B/C.

**PAGEIO first, and wait for `serving_o`.** `nb_o` is combinational on a shadow
plane that does not exist until `S_SERVE`, and nothing else on that block's face
says so — `idle_o` drops at the **job accept**. A dig started early reads an
empty no-bake plane, §3.3's corner shadow silently vanishes, and every handshake
and every counter agrees. `serving_o` was added to PAGEIO in the same commit as
this block, and it publishes an existing internal fact: one line,
`(state_q == S_SERVE)`, no new state and no new decision.

**The seam second**, and its `job_ready_o` **is** the prefetch's completion — the
block holds it low while filling, so this FSM's own handshake is the wait.
**The stream last**, once the seam has admitted the record: bake pulls vertices,
and stalling it costs nothing while starting it early cannot help.

The completions are collected from the moment the stream is started, because
`TERRAIN.PAGESTREAM` retires its job when the **lattice** ends — the end of
bake's DIG phase, before BREACH and therefore before `bake_done_i`. A ready
raised only in a later state would deadlock the share against a block waiting to
be taken.

## 7. Parameters

| name | default | what it is |
|---|---|---|
| `SLOTW` / `GENW` | 11 / 8 | the residency handle, one bit wider than the pool needs (TERRAIN.PAGELOADER's reason) |
| `DEPTH` | 2 | pending records. Stamps coalesce by patch, so the third **different** patch inside one bake window is `overflow_o` — a LOST scar, loud on purpose |
| `RETRIES` | 3 | R221 fallback re-issues before the scar is declared lost |
| `MAX_AGE` | 8 | frames a record may wait for its patch to be composed; a stamp on a patch the camera never looks at ages out and is counted |
| `FALLBACK_DEPTH_FROM` / `_TO` | 0 | §4 |
| `DUAL_BIT` | 3 | `kFlagDual` in TERRAIN.PAGESTREAM's T5 record flags |
| `CELLS_PRESENT` | 1 | layer D is served, because TERRAIN.PAGEIO is composed beside it |

`cmd_patch_id_o` is the low byte of each of ix and iz. It is a **trace key**:
`zhao_terrain_bake_v2` reads it for `trace_patch_id_o` and nothing else. Said
here because a 16-bit field called `patch_id` beside a 32-bit field called
`handle` is exactly the shape somebody keys a cache on later.

## 8. Counters

Every one moves under legal stimulus at this block's own boundary;
`tests/terrain/bakerec_rtl_directed.cpp` fires each deliberately and shows it
silent beside it. **None of them is a guard only a mutant could reach**, so none
owes a committed mutant.

`stray_done_o` is the one worth a sentence: a `bake_done_i` arriving with no
record in flight. It is reachable (a completion after a retirement) and it is a
counter rather than an assertion for that reason — the distinction
`zhao_terrain_pageio` draws between its five stimulus-fired counters and
`wq_overflow_o`.

## 9. What this does NOT own or claim

* It does not decide **when** a patch is baked beyond "the compose engine
  touched it and a stamp is pending". `BAKE_PATCH_BUDGET` and §9.2's frame
  window are `zhao_terrain_bake_v2`'s.
* It does not produce a **disc** record and could not: the ABI has no depth
  field, and entry I32's D-TERRCMD-B is still true. The disc arm's producer is a
  cast's progress in the FIELD subsystem, for §9.2's *"(to − from) × stencil so
  an interrupted cast un-applies"*. That producer will drive `cmd_*` alongside
  this one and will need an arbiter; it does not exist yet and no port is
  reserved for it here.
* **Cost is counted by hand, not fitted** (R236): two records of about 280 flops
  each, a seven-state sequencer and eleven counters — order 700 flops and a few
  hundred ALM. No multiplier, no memory.
