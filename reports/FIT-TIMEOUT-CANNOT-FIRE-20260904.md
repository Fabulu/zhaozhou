# The block-fit timeout cannot interrupt a fit, and is about to discard one

Found 2026-09-04, 203 minutes into `zhao_texture_tmu_pipe`'s fit, which was
launched with `-TimeoutSeconds 9000` (150 minutes).

## The defect

`tools/quartus/run_block_fit.ps1` runs each Quartus tool in turn and checks the
clock **between** invocations:

    & $exe ...                                   # runs to completion, however long
    ...
    if ($sw.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
        $row.status = 'timeout'; $ok = $false; break
    }

The check is *after* the tool it is supposed to bound. It cannot stop a running
`quartus_fit.exe`; it can only observe, once that process has exited, that the
budget was already blown. **A timeout that cannot interrupt is not a timeout, it
is an epitaph.**

## The consequence, which is worse than the defect

`quartus_fit.exe` has been running for 203 minutes and 21,598 seconds of CPU.
When it finishes, the very next statement marks the row `timeout` and `break`s
before STA. So the harness will:

* **discard** a synthesis and placement result that cost three and a half hours;
* record `status: timeout` with no numbers, exactly as `zhao_forge_cliff` reads
  today — which is very likely the same event, already once;
* leave the island one block short with nothing to show for the wall-clock.

`zhao_forge_cliff`'s `timeout` row is the tell. It carries no registers, no
ALMs, no memory bits — a block that was *measured* and whose numbers were
thrown away by this code path.

## Mitigation for THIS run, which does not need the harness fixed

**The data survives in the workspace regardless of what the harness records.**
The fit writes `output_files/blockfit.*.rpt` and `.summary` under its own temp
directory, and those are ordinary files:

    <temp>/zhao-block-fit-15848-<id>/zhao_texture_tmu_pipe/output_files/

The synthesis summary was already read this way while the fitter was still
running — that is where D19m's 72,824 registers came from. When the fitter
exits, the fit summary can be read the same way and the numbers recorded by
hand, whatever `status` the JSON ends up carrying.

**So the measurement is not lost; only the harness's record of it is.** That
distinction is worth stating precisely, because "the fit timed out" and "the fit
completed and the harness discarded it" invite completely different next
actions, and only the second one is true.

## Not fixed in this pass, deliberately

`run_block_fit.ps1` is not in any fit's RTL source closure, so editing it is not
the live-tree trap. It is still the wrong moment: **PowerShell reads a script
into memory at launch, so a change now cannot affect the running fit — but the
queue invokes the script FRESH for `cache_pipe`**, which would then run under
edited timeout semantics mid-campaign. Changing how a measurement harness
behaves between two blocks of the same campaign is how two rows end up
incomparable.

The fix, for after the campaign: give the process a real deadline —
`Start-Process -PassThru` plus `Wait-Process -Timeout`, and kill the tool when
it expires — so the budget bounds the tool instead of eulogising it. And when a
run genuinely does exceed its budget, **write the numbers that were already
produced** rather than blanking the row: a fit that placed successfully and ran
long still measured registers, ALMs and memory, and those are exactly what a
`timeout` row throws away today.
