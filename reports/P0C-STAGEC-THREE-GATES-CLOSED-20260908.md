# P0-C Stage C: fragrob replaced by v3own inside the texture island

**2026-09-08.** All three gates pass. The Stage C fit is running; **this report
contains no resource numbers for the composition**, because it does not have any
yet, and a restructure report is exactly where an estimate would get quoted back
later as a measurement.

## Gates

| gate | asks | result |
|---|---|---|
| 1 | v3own's 541-check adversarial suite on the **unmodified** `zhao_texture_v3own.sv` | passes |
| 2 | `island_v3_composed_directed` -- the oracle's own 119-check source compiled `-DISLAND_V3` | **119/119** |
| 3 | `island_v3_paired` -- both tops, identical stimulus, retired streams compared | **392 records byte-identical on rgb/a/tag/refused AND order** |

`zhao_texture_v3own.sv` and `zhao_texture_island_top.sv` are both **unedited**.
The first because gate 1 is only meaningful on the file the suite was written
against; the second because it is gate 3's oracle, and editing an oracle
destroys the comparison that makes the work checkable.

### Gate 3 is a file diff, not a third harness

The obvious shape -- a test instantiating both tops -- is a **second driver**,
and a second driver is a copy of the first. `island_composed_directed.cpp`
already refuses to be copied for this reason. So both tops run the SAME source
with `--dump` and the streams are compared. A byte compare cannot drift.

Order is compared positionally, not as a set: v3own's §5.4 emits in strict owner
order, and a composition retiring every correct colour in the wrong sequence
would satisfy a set comparison while being broken. The comparator's fire test
includes a reordered-but-identical stream for exactly that reason.

## What the integration actually cost: two defect families

### Five stale slices -- the re-key's real bill

The owner handle went from `{slot[3:0], gen[7:0]}` to `{slot[5:0], gen[7:0]}`.
Five places had taken the old identity apart:

| site | slice | how it failed |
|---|---|---|
| `uvw_m` index | old slot width | in range, wrong row |
| `fc_wp` | queue pointer | in range, wrong entry |
| `fc_rp` | queue pointer | in range, wrong entry |
| `rsp_class_i` | `[15:14]` | in range, wrong class |
| AUX return token | `[AUX_TOKW-1 -: 4]` of a 6-bit slot | in range, wrong owner |

**None failed elaboration. None failed lint.** All produced plausible wrong
data. A re-key's cost is not in the port list -- it is in every place that ever
took the old identity apart, and nothing in this tree finds those.

The AUX one is the instructive case: the token **is** the owner handle, and the
fix was to stop taking it apart at all rather than to correct the slice.

### Two stage misalignments -- a different mistake

* the AUX request's world coordinates came from `own_out_ctx`: the context of
  whatever the **output** stage was emitting, a different fragment entirely;
* `palslot_m`/`palgen_m` were written at admission (an **input**-stage event)
  from planner-stage values, while `mat_m` beside them used the input ports.

Both are "an attribute read from the wrong stage" -- and this island's own test
**already had a check for that**, in these words: *"travels with its fragment
instead of being read off the input pin twelve clocks late"*. The check existed;
the defect was reintroduced next to it.

The palette one presented as only **three** stale lookups, because a phase holds
one palette slot for most of its fragments and the misalignment is invisible
wherever old and new happen to be equal. **A defect mostly masked by uniform
stimulus is not a small defect** -- it is a large one with a lucky workload.

### Four undriven outputs

`fr_tmu_valid` and the AUX valid had no driver; a later sweep of all 42 outputs
found `cnt_fragments_o` and `cnt_fragrob_id_errors_o`, both fragrob's. An
undriven output does not fail elaboration or lint -- it reads as a clean zero,
and a counter reading zero is indistinguishable from a stage that is quiet.

`tools/quartus/undriven_outputs.py` now sweeps for this. It was verified to fire
by deleting a real driver from a copy of the real top, not only against its
synthetic example. Its first version reproduced the `output var logic signed
[31:0] name` parse bug that `gen_prod_top.py` had already had and already
fixed -- **a defect re-created in a tree that contained its own fix**.

`cnt_fragrob_id_errors_o` is now driven from v3own's unsolicited + stale +
duplicate counters. That is a **re-interpretation, not an identity**, and it is
labelled as such in the source rather than hidden behind a matching port name.

## One handshake, one alignment term

The COMBINE handshake briefly had two: `f_valid_i` on a registered flag,
`cmb_ready_i` on a combinational compare. A valid and a ready computed from
different notions of "the material is here" cannot agree, and 32 retired
fragments became 0. The registered flag was **deleted rather than repaired** --
the fault was having a second source of truth, and correcting one of two would
have left the trap armed.

## Stale measurement, declared

The expander's leaf fit -- 323 ALM, 451 reg, 0 DSP, worst path +1.623 ns -- ran
at 05:21. The `f_ctx_i`/`aux_ctx_o` ports and the 64-bit `ctx` field in the
4-deep queue landed at 05:48. **That number describes a block that no longer
exists** and is short by roughly 256 flops plus routing. Re-fit is queued.

It is written down as stale rather than repeated, because a fit number carries a
reassuring air of having been measured, and "never compare a current file to an
old measurement" is the trap it walks into.

## Open

* Stage C fit running, under the **oracle's own redline** (7,500 ALM / 9,000 reg
  / 64 M10K / 14 DSP). Same rules as `zhao_texture_island_top`: the question is
  whether the restructure fits the budget the island already had.
* Expander re-fit, queued behind it.
* `check_prod_manifest.py` reports both new blocks UNACCOUNTED. Correct: the v3
  top is not production until the fit says it can be, and registering it before
  then would put an unmeasured block in the ledger.

## Gate 3 was shown to FIRE, on real RTL

It passed on its first run, which by this repository's own law means it had not
been tested. `assign out_a_o = own_out_result[31:24]` was replaced with
`8'hFF` -- the island's actual historical defect, alpha hardwired -- in the live
tree (safe: `run_block_fit.ps1` snapshots its 18 sources into a workspace, so a
running fit cannot see the working tree).

Both halves fired, and they fire differently, which is worth separating:

* **The gate refused to compare.** The mutant's own 119-check run failed 7/119,
  and `gate3_paired.py` returns 1 rather than diffing, because a paired
  comparison between a passing top and a failing one is not evidence about
  ordering -- it is evidence about the thing that already failed.
* **The stream comparison itself caught it**, when run directly against the
  mutant's dump: **9 of 392 records differed**, each `alpha 254` against
  `alpha 255`. So the byte compare sees a real single-channel defect in real
  output, not only in its synthetic fire test.

The file was restored from a byte backup and `git diff` confirms it is identical
to HEAD; gate 3 passes again at 392/392.

**And the mutation run caught a reporting trap in my own command**: `python3
gate3_paired.py | tail` printed `exit=0` while the script had returned 1. That
is CLAUDE.md's *"read the build's exit code, not the pipeline's"* -- the same
trap, in a new costume, inside the very run that was checking whether a detector
could fail. The real exit code was confirmed separately.
