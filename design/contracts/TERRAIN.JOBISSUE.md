# Contract — TERRAIN.JOBISSUE (the subpatch job issuer)

> Ledger: `design/blocks.yml` · owner ZH-050 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

The block `fpga/rtl/prod/zhao_console_core.sv` entry **I21** says does not
exist. It joins `zhao_terrain_lod`'s `lod_target` stream to the patch's own
**draw context** and drives `zhao_terrain_group_seq`'s subpatch job port; and,
because it is the block that "issued its subpatch jobs and counted them home",
it drives `zhao_terrain_compcache_front`'s **retirement pulse** as well. Entry
I21 names both halves and assigns both to the same absent owner. This is that
owner.

Implemented as `fpga/rtl/terrain/zhao_terrain_jobissue.sv`.

**Why it is a block and not a wire.** Entry I21 refuses, correctly, to drive
`job_view_mask` from the compose door while the other job fields arrive from
outside the core, because that "joins two things that move independently — a
worse hidden adapter than the one this entry refused to build". The objection
is about **independent movement**, not about the mask. The remedy is to make
the two move together *and to be able to see when they do not*: the context is
captured at the door, queued, popped for the patch the cache actually serves,
and its `src_id` differenced against the serve port's own. That comparison has
**two independently clocked operands**, so it can fire — which is the
`CLAUDE.md` metadata-swap law applied on the way in rather than discovered
afterwards.

**THE ISSUER OWES THIRTEEN FIELDS, NOT SIXTEEN.** Owner ruling **R13** rules
`job_mat_a`, `job_mat_b` and `job_weight` the **wrong carrier**: layer E is per
CELL, a subpatch job covers 64 of them, and "the job port is not widened to
carry a subpatch-uniform value that is not true". This block therefore has no
material ports at all. R13's honest closure for the sequencer's three is their
**removal**, once a per-triangle layer-E path exists inside TESS — a function
MOVE, and nothing leaves until the replacement lands. Until then they remain
the core's declared boundary, tied off and declared per R159.

Excluded, each with its reason:

* **no material triple** — ruling R13, above;
* **no layer-E reader** — R13 puts it inside TERRAIN.TESS (entry I21 blocker 1);
* **no devstore read orchestration** (`r_start_i`/`r_slot_i`) — the store is
  addressed by page SLOT and this block is keyed by `src_id`. Inventing that
  mapping here would be the second hidden adapter in a block written to refuse
  the first;
* **no view-mask narrowing** — the 8 → 2 discard belongs at the door, where the
  high bits still exist to be refused. This block's port is already two bits,
  as the core's `terr_job_view_mask_i` has been since before this block existed;
* **no timeout, and no fault flag for an early decision** — see Counters;
* **no reordering, no buffering of decisions, no second patch in flight.** The
  compose cache is a two-buffer front and serves one patch at a time.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain. Reset empties
the context queue, returns the FSM to `StIdle`, drops `serve_release_o` and
zeroes every counter. No clock-domain crossing lives here.

## Input and output packet layouts

**`draw_context` in, ready/valid.** `{src_id:16, view_mask:2, sparse_fill:1}`,
pushed once per patch when the compose door hands the lattice to the streamer.
The queue is `CTXD` deep (default 4) with a real `ctx_ready_o`; a context
offered to a full queue is **refused and counted**, never overwritten.

**`serve` in.** `serve_valid_i` is a LEVEL — the compose cache holds it while a
patch is served — with `serve_src_id_i`. `serve_release_o` is a one-cycle
PULSE, one patch per rising edge, as the cache's port requires.

**`lod_target` in, ready/valid.** `zhao_terrain_lod`'s `out_*` port field for
field, including `out_hold_o`, which the LOD contract describes as being "for
the caller to store". This block is that caller.

**`job` out, ready/valid.** `zhao_terrain_group_seq`'s `job_*` port minus the
three material fields: eleven fields forwarded from the decision unchanged,
plus `job_view_mask_o` and `sparse_fill_o` from the patch's own context.

**`history` out, ready/valid.** `zhao_terrain_devstore`'s `h_*` writeback:
`{level, morph, hold}` from each accepted decision, in subpatch order.

A decision is consumed only when **both** the job port and the history port
have taken it. Neither sink is offered the same decision twice. `job_valid_o`
and `h_valid_o` depend on no `*_ready_i`, so there is no combinational
valid↔ready loop with either consumer.

## The lifetime of a patch

| Phase | What happens |
|---|---|
| door | `ctx_valid_i` pushes the context; refusals counted |
| ARM | `serve_valid_i` pops the head; `src_id` compared |
| ISSUE | each decision forked to the job port and the history port |
| DRAIN | wait for `job_ready_i` to return high |
| RETIRE | one-cycle `serve_release_o`; `patches_retired_o` counts it |

The expected decision count is latched from the **first** decision's
`lod_dual_i`: 32 on a dual page, 16 otherwise. That is TERRAIN.LOD's own emit
law, named once in a `SUBPATCHES` parameter so the count this block waits for
cannot drift from the count that block emits.

**The release is the sequencer's idle edge, not a count of clocks.**
`zhao_terrain_group_seq`'s own exit proof says `job_ready_o` rising after a job
means every reference of that job has been accepted by the shell and its arenas
released. That is the earliest instant at which "TESS is finished with the
served patch" is *true* rather than *likely*, and the compose cache's header
records what a wrong reading of this port already cost: a whole patch retired
without one vertex being read, with `patches_served_o` counting it as consumed.

## THE DROP LAW

On a context mismatch the block issues **nothing** and retires the patch,
counting `ctx_src_mismatch_o` and `patches_dropped_o` separately. It gives up
function on a fault path and the reasoning is recorded because of that:

* issuing with the head context anyway projects a patch into the **wrong
  player's view**, silently, and nothing downstream can tell;
* refusing to release **wedges the compose engine** with no timeout and no
  counter — the `zhao_geom_vattr` stall shape owner ruling R88 says "deserves a
  counter now … nobody will be debugging shadows when it fires";
* a counted drop is the only one of the three that is **visible**.

The two counters are separate so a drop for any later reason cannot hide inside
the mismatch's number.

## Counters

| Counter | Kind | Fires when |
|---|---|---|
| `jobs_issued_o` | census | a decision is consumed by both sinks |
| `patches_retired_o` | census | `serve_release_o` pulses after a full patch |
| `patches_dropped_o` | fault | a patch retired without issuing (drop law) |
| `ctx_refused_o` | fault | a context offered to a full queue |
| `serve_no_ctx_o` | fault | a serve with an empty context queue |
| `ctx_src_mismatch_o` | fault | the popped context is not the served patch's |
| `lod_src_mismatch_o` | fault | a decision's `src_id` is not the armed patch's |
| `issue_clocks_o` | instrument | clocks with a patch armed |
| `decision_wait_clocks_o` | instrument | clocks with a decision offered, none armed |

**All five fault counters are reachable from this block's own boundary with
legal stimulus**, so no committed mutant is owed here. `terrain_jobissue_directed`
fires each one deliberately and shows each silent on a clean patch beside it
(R95). That claim is measured, not asserted: the test is 138 checks.

**The two clock instruments are not fault flags, and that is a correction the
test forced.** The block first carried an event counter called
`stray_decision_o` for "a decision offered with no patch armed". It fired on a
**clean patch**: TERRAIN.LOD has the next decision ready before the compose
cache serves the next patch, and a producer valid in the very cycle the patch
arms is early by zero clocks. Both are ordinary backpressure. A counter that is
non-zero in normal operation cannot be read as a fault, and narrowing its window
until it went quiet would have been tuning an instrument to its own test. There
is no locally detectable fault here at all — this block cannot know whether a
patch will ever be armed for a decision it is holding — so the honest instrument
is the **duration**. The pair to read is either clock counter climbing while
`patches_retired_o` is **flat**.

That is also why there is **no timeout**. A timeout is a retirement policy, and
the cache's header is explicit about what a wrong one cost.

## Elaboration guards

`CTXD >= 2` (one filling, one served) and `SUBPATCHES == 16` (TERRAIN.LOD emits
sixteen). Both live in `initial begin … end`: Quartus 17.0 rejects a bare
module-scope `if`, and `--lint-only` does not run them, so a clean lint says
nothing whatever about either.

## Cost

No multiplier, no memory. The context queue is `CTXD × 19` bits (76 flops at
the default), the active-patch registers ~30, the FSM and the fork 5, and nine
32-bit counters. Order of **400 flip-flops and a few hundred ALM**, counted by
hand — **not fitted**, because the owner's instruction for this campaign is
"we're not fitting now, we're going zero gaps". Recorded as a cost per R236:
cost is a fact, never a veto.

## Not yet composed

Its producer `zhao_terrain_lod` and the deviation store `zhao_terrain_devstore`
must compose with it; see the console core's entry I21 for the state of the
group.
