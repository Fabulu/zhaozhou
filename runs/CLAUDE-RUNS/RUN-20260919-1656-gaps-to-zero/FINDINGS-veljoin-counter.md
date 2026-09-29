# `terr_vj_sweeps_aborted_o` reads the WRONG BLOCK, and veljoin's own count cannot be read

Coordinator, 2026-09-29. Found while triaging `console_core_tieoff_audit`, which has
been RED and inherited since at least the unpark packet (`FINDINGS-unpark.md` §6.6,
which verified it inherited at base `c93fe343`). That packet named the two SILENT
literal connections correctly and stopped there. **One of them is not a missing
comment. It is a misattributed counter.**

---

## The evidence, and it is four lines of one file

`fpga/rtl/prod/zhao_console_core.sv` declares five counters in the veljoin
namespace (`:14892-14896`):

```systemverilog
  output logic [31:0]             terr_vj_lanes_joined_o,
  output logic [31:0]             terr_vj_sweeps_started_o,
  output logic [31:0]             terr_vj_sweeps_aborted_o,
  output logic [31:0]             terr_vj_vtx_mismatch_o,
  output logic [31:0]             terr_vj_arm_stall_o,
```

`u_terrain_veljoin` drives **four** of them and drops the fifth (`:30216-30220`):

```systemverilog
    .lanes_joined_o    (terr_vj_lanes_joined_o),
    .sweeps_started_o  (terr_vj_sweeps_started_o),
    .sweeps_aborted_o  (),                          // <- DROPPED
    .vtx_mismatch_o    (terr_vj_vtx_mismatch_o),
    .arm_stall_clocks_o(terr_vj_arm_stall_o),
```

and `u_terrain_velocity` — **a different module** — drives the port that was left
open (`:30236`):

```systemverilog
    .sweeps_aborted_o(terr_vj_sweeps_aborted_o),
```

So `terr_vj_*` is unambiguously veljoin's namespace, four fifths of it is wired that
way, and the fifth member carries `zhao_terrain_velocity`'s count instead.

## Why it matters, in both directions

**A reader who sees it fire blames the wrong block.** `zhao_terrain_veljoin.sv:114`
says of its own counter: *"`sweeps_aborted_o` is the count, and a non-zero value is a
real finding"*. `zhao_terrain_velocity.sv:170` says the same thing of its own. Both
are right, and only one of them can be read through a port named for the other.

**And veljoin's count cannot be read at all.** It is not merely unexported — with
nothing consuming it, synthesis is free to prune the counter and its increment
entirely, so the flops may not exist in the shipped machine. A counter that does not
exist reads zero forever, and this repository's own law is that *a detector reading
zero is a claim, and it is the claim to check hardest.*

**Neither error is visible to any test.** Both blocks' directed tests drive the
module directly and read its own port, so they measure the right counter every time.
`FINDINGS-MATERIALPATH.md:213` records `sweeps_aborted=1` from exactly such a test —
a real firing, at the module boundary, that says nothing about the console's wiring.
The only instrument that can see this is the tie-off audit, and what the audit
reports is "a literal with no reason", not "the name is wrong".

This is the campaign's own pattern with a new face: two errors that cancel into a
reassuring signal. The console port is driven, so nothing is UNDRIVEN; veljoin's
output is unused, so nothing is UNUSED; the names disagree and no tool compares
names.

## What the fix is, and why it is not a comment

**Not** a `// TIE:` line. The audit's remedy text — *"Add `// TIE: <why>` on the
line, or connect it"* — offers both, and here the second one is correct: there is no
honest reason for veljoin's abort count to be discarded while its four siblings are
exported.

The shape of the repair:

1. give `zhao_terrain_velocity`'s counter a port of its own on
   `zhao_console_core` (`terr_vel_sweeps_aborted_o`, in the namespace of the block
   that produces it);
2. wire `u_terrain_veljoin.sweeps_aborted_o` to `terr_vj_sweeps_aborted_o`, so the
   veljoin namespace is complete and self-consistent;
3. regenerate `zhao_console_board.sv` and `zhao_prod_top.sv`, because this is a
   **port change** and CLAUDE.md's rule applies: a new port nobody connects is a
   `PINMISSING` only the next fit discovers;
4. check the counter-id registry, because `counter_ids_append_only` is currently RED
   for reasons that belong to another lane's work and must not be tangled with this.

**Cost:** one 32-bit output port plus whatever veljoin's counter costs once it is
no longer prunable — about 32 flops and an incrementer. It is a real, small area
addition, not a free relabelling, and that is the honest way to describe it.

## Why it is NOT being done in this moment

The full console fit is running (`zhao_console_core`, launched 2026-09-29 from
`c033dc00`). `run_block_fit.ps1` snapshots its closure, so an edit could not corrupt
the run — but `zhao_console_core.sv` is in that closure, and a commit landing between
the snapshot and HEAD degrades the receipt's provenance, which is the one thing this
fit exists to produce. A counter-observability defect that has been latent for weeks
does not justify weakening the measurement the owner is waiting for.

So: repaired immediately after the fit's receipt is written, as the first item.

## Not claimed

* No claim that either count is currently non-zero on real workloads. Nothing here
  has been run on hardware or in a console-level bench.
* No claim about which of the two blocks aborts sweeps more often, or that either
  ever does in normal operation.
* No claim that this is the cause of any observed behaviour. It is an observability
  defect: the machine may be computing correctly while reporting the wrong block.
* The other SILENT tie-off, `u_cliff_lat_share.poison_value_i (113'd0)`, is a
  different and probably benign case — that instance sets `.POISON_EN(1'b0)`, so the
  value is never presented. It needs a stated reason, not a rewire, but the reason
  has not been verified against `zhao_forge_cliff_srvshare.sv` yet and is therefore
  not written here as fact.
