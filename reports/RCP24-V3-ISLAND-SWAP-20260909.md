# RCP24 V3 island swap — R3 performed, 2026-09-09

Owner ruling R3 (`reports/OWNER-RULINGS-20260909-2300.md`), verbatim: *"Use V3
with NCTX=12, but note this down as possible to reverse if we ever find
ourselves wit enough DSPs to flex."*

`zhao_texture_island_v3_top` now instantiates
`zhao_raster_rcp24_v3 #(.NCTX(12), .TOKW(14))` where it held
`zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(14))`. NCTX=12 is the condition, not
a preference: at the island's parameters NCTX=8 measures 5.78 clk/recip
against the tile's own 4.6 threshold (fails, 1/52); NCTX=12 measures 4.38
(passes, 52/52). Buys −3 DSP, clearing the live `max_dsp: 14` breach on the
island, for four more contexts of per-context state and +7 M10K. The reversal
condition (DSP headroom) is recorded in `design/prod_manifest.yml` beside the
superseded entry, tense corrected from "is APPROVED" to "WAS PERFORMED".

## What changed

**`fpga/rtl/texture/zhao_texture_island_v3_top.sv`** (the only RTL edit):

1. Instantiation at the former `:625`: module name and `NCTX(8)` → `NCTX(12)`.
2. `rcp_occ` widened `[3:0]` → `[5:0]`. Verified before widening: **nothing
   downstream consumes `rcp_occ` at any width** — like `pu_occ` it is
   declared, driven and never read (the brief asked whether a 4-bit consumer
   needed fixing; there is no consumer). Widened anyway so the capture cannot
   truncate, with a comment citing the pu_occ precedent.
3. `rcp_mul_busy` deleted along with its `.mul_busy_o` connection. Verified
   the brief's claim myself: the name appeared exactly twice (declaration
   `:504`, connection `:635`) — a dead-end capture.
4. v3's four job counters (`mul_jobs_o`, `zero_jobs_o`, `phase_jobs_o`,
   `negcorr_jobs_o`) land on locals `rcp_mul_jobs` etc., following the
   island's committed `pu_zero_products` precedent and its "one summary
   counter per block" law (`cnt_rcp_completed_o` remains RCP's boundary pin).
5. **`qerr_o` → new island output port `err_rcp_q_o`**, joining the
   `err_fragrob_wq_overflow_o` / `err_fragrob_id_error_o` /
   `err_aux_degenerate_o` tripwire family. Rationale below.

**`tests/texture/island_composed_directed.cpp`**: new check, guarded
`#ifdef ISLAND_V3` (the oracle island has no such port), asserting
`err_rcp_q_o == 0` beside the other three tripwire assertions.

**`tests/CMakeLists.txt`**: the three v3-island targets
(`test_island_v3_composed_directed`, `test_island_v3_prod_composed_directed`,
`test_island_v3_fault_directed`) swap `zhao_raster_rcp24_svc.sv` for the v3
tile's four files (`rcp24_v3`, `rcp24_mul`, `ticketq`, `ticketq_rh`). The
oracle island target (`:974`), `tb_rcp24_pair` and the shell bench keep svc.

**`design/fit_targets.yml`**: same four-for-one swap in the
`zhao_texture_island_v3_top` closure and the `zhao_prod_top` closure. The
oracle island's closure and the standalone svc leaf target are untouched.

**`design/prod_manifest.yml`**: `zhao_raster_rcp24_v3`, `zhao_raster_rcp24_mul`,
`zhao_raster_ticketq`, `zhao_raster_ticketq_rh` leave `excluded:` — they are
now counted inside the island's composed fit by instantiation, the same move
the island's other children made when it became a counted top.
`zhao_raster_rcp24_svc` gains an explicit
`superseded by zhao_raster_rcp24_v3 at NCTX=12 (R3)` row — it survives only
inside the `zhao_texture_island_top` oracle (excluded: probe) and the shell
test bench, which the file's own words call "accounted for by accident" if
left implicit. The R3 ruling comment block is preserved in full, including the
reversal condition and the GATE4 −816 ALM overstatement warning.

**`fpga/rtl/prod/zhao_prod_top.sv`**: REGENERATED via
`tools/quartus/gen_prod_top.py` (66 instances, unchanged count). The new
`err_rcp_q_o` is declared, connected, and folded into `u61_fold_q`'s XOR — so
it is observed, not just wired. Diff is exactly those three lines.

## Where `qerr_o` went, and why

To the island boundary as `err_rcp_q_o`, pass-through. Three options were on
the table:

* **Fold into an existing error output** — rejected: every existing `err_*_o`
  names one fault in one block, and folding would make a fired bit
  undiagnosable, which is the wrong trade for a debug boundary that already
  pays one pin per tripwire.
* **Latch into a counter** — rejected as redundant: `qerr_o` is already
  sticky at the source. Each `zhao_raster_ticketq` latches `err_o` on
  push-into-full or pop-from-empty (`zhao_raster_ticketq.sv:90`) and
  `zhao_raster_rcp24_v3.sv:437` ORs the four latches. A second latch or a
  count of a sticky level adds state and no information.
* **Boundary port** — chosen. The island's own header law (the "TRIPWIRES
  THAT WERE DANGLING" note) says a fault signal nobody can see is decoration
  that synthesis deletes; V3 section 0 point G says preserve tripwires, and
  preserving requires observability. The composed test now asserts it zero,
  so its silence means something from its first run.

The four job counters deliberately did NOT become ports: they are throughput
evidence (the standalone tile test divides them to prove the saturated launch
rate), not fault detectors, and the island's port list is explicit that
counter pins are rationed to one per block to keep the I/O register count from
inflating the ALM number the fits measure. `cnt_rcp_completed_o` is that pin.

## `occupancy_o` at NCTX=12

`occupancy_o` is `[5:0]` in v3 (`zhao_raster_rcp24_v3.sv:124`) and the
island's capture `rcp_occ` is now `[5:0]`: a 12-context tile reporting
occupancy 12 (or anything up to 63) passes through without truncation. At the
old `[3:0]` width, occupancy 16 would have aliased to 0 — and NCTX=12 with
v3's queue discipline is precisely what makes >15 in-flight bookkeeping
representable, so the ruling itself would have caused the truncation the
brief predicted. Nothing in the island consumes the value; it exists so the
occupancy is observable in traces and cannot be optimised into a lie.

## Verification

**Run 2026-09-10 by the reviewing session, not the implementing one.** A Windows
update rebooted the machine overnight and killed the lane mid-verification, so
its own gates never ran; everything below was executed fresh against the surviving
working tree. Standalone verilator builds into throwaway Mdirs outside `build/`,
so no shared-tree contention.

    island lint, MODMISSING                     0   (was 1 -- see the repair below)
    island_v3_composed_directed, HEAD baseline  132/132 passed
    island_v3_composed_directed, AFTER the swap 133/133 passed
    check_quartus17_syntax.py                   clean, 220 files

**+1 check, and it is the right one.** The delta is exactly the new
`err_rcp_q_o == 0` tripwire assertion this change added. Every pre-existing check
still passes, so the swap changed no behaviour the suite can see.

### An inherited defect found while verifying, and it would have killed the fit

Linting the island standalone **from its own declared closure** returned:

    %Error-MODMISSING: zhao_texture_island_v3_top.sv:1031:3:
      Cannot find file containing module: 'zhao_raster_perspuv_pairpipe'

`design/fit_targets.yml`'s `zhao_texture_island_v3_top` closure listed
`zhao_raster_perspuv_svc.sv` -- the **superseded** block -- while the island
instantiates `zhao_raster_perspuv_pairpipe` at `:1031`. Left over from the
pair-pipe swap earlier the same day: the instantiation changed and the source list
did not. `zhao_prod_top`'s closure has both, so only the island target was stale.

**The island fit owed for BOTH the pair-pipe swap and this one would have died at
elaboration.** A 1.5-to-4-hour fit spent on a stale source list is exactly what
the subsystem-boundary law exists to prevent. Repaired one-for-one, with the
dependency checked first: `pairpipe` instantiates nothing (it is a leaf, so it
brings no children), and the only remaining mentions of `svc` in the island and in
`pairpipe` are comments.

### A consistency repair to this change itself

The lint delta against a HEAD baseline -- HEAD's island with the same closure
repair applied, so the comparison isolates the swap -- was **+3 `UNUSEDSIGNAL`**:

    gone:  rcp_mul_busy
    new:   rcp_mul_jobs, rcp_zero_jobs, rcp_phase_jobs, rcp_negcorr_jobs

The swap deleted ONE dead-end capture and created FOUR. The stated rationale for
deleting `rcp_mul_busy` was that it was "declared, driven and read nowhere", and
the four replacements are the same shape.

Changed to the empty-connection form -- `.mul_jobs_o(), .zero_jobs_o(), ...` --
which is **this file's own convention** for a deliberately unobserved pin
(`u_v3bank` at `:938` does it three times, and the island carries nine such pins
already). The reasoning is now in a comment beside them rather than encoded in a
name that looks like it means something.

Final delta against the HEAD baseline: **+4 `PINCONNECTEMPTY`** (explicit,
intentional, self-documenting) and **-1 `UNUSEDSIGNAL`** (one fewer dead signal
than HEAD). No new error classes, no MODMISSING, and every other diagnostic count
identical.


## The fit gate

**No fit was run.** The one gate this swap owes is the island composed fit
(`zhao_texture_island_v3_top` @g2-prod, MIGRATION_SHADOWS=0), and its question
is: *does the composed island now meet `max_dsp: 14` (expected 17 → 14), and
what do the +7 M10K and the per-context flops do to its 49 M10K / 10,837 ALM
row?* This batches with the island fit already owed for the pair-pipe swap —
same top, same closure, one fit answers both. Per the subsystem-boundary law
there is no reason to spend two.

## Not verified (instrument named per item)

* **DSP 17 -> 14 on the composed island.** Structural: `svc` measures 6 DSP in
  every row ever produced and `v3` measures 3 in every row, across NCTX 8/12/16
  and TOKW 8/14, so the halving is parameter-invariant -- but the composed
  island's own DSP total after the swap is unmeasured. **Instrument:** the one
  named fit gate below.
* **The +7 M10K and what the four extra contexts of flop-held state do to the
  island's 49 M10K / 10,837 ALM row.** Same gate.
* **Fmax.** Does not transfer from the tile's standalone rows: the island's
  reported 62.83 MHz is set by a pin path unrelated to the tile. Same gate.
* **Whether the nine pre-existing `PINCONNECTEMPTY` warnings are intentional.**
  They pre-date this change (identical count at HEAD) and were not investigated.
  **Instrument:** reading each one; not attempted here.
* **`zhao_prod_top` regeneration.** The report states it was regenerated via
  `tools/quartus/gen_prod_top.py` with the instance count unchanged at 66. I
  confirmed the file's diff is small and that `err_rcp_q_o` appears in it, but I
  did **not** re-run the generator to prove the committed file is byte-identical
  to what it produces. **Instrument:** re-running `gen_prod_top.py` and diffing.
