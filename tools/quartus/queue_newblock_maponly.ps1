# queue_newblock_maponly.ps1 — MapOnly the three new texture blocks.
#
# Answers the Decrufter brief's own open memory-geometry question (§D) without
# spending a fit on it:
#
#   "A 119-bit-wide descriptor is not one M10K just because 64 x 119 is less than
#    10,240. A direct same-width simple-dual-port realization needs at least three
#    parallel width slices at a maximum 40 bits per slice. ... The metadata bank's
#    256 x 40 shape is compatible with one block; that still needs confirmation in
#    the actual fit."
#
# MapOnly reports inferred memories and estimated ALMs, which is exactly the
# claim under test, in minutes rather than the twenty-to-sixty a leaf fit costs.
# It cannot report Fmax and does not pretend to.
#
# It also closes a real gap: `zhao_texture_metajoin` is instantiated by the
# island and had NO fit row at all, so its contribution was uncounted AND
# unmeasured (reports/DSP-BUDGET-CENSUS-20260908.md).
#
# The rules in design/fit_targets.yml encode the geometry ARGUMENT, so a block
# that infers more memory than the argument allows comes back
# `failed:structure` and says so rather than passing quietly. A row that comes
# back `ok` here is the argument surviving contact with the tool.
#
# Runs LAST in the queue: gate 4 and gate 1's MapOnly pair both matter more, and
# one Quartus at a time.
#
# ---------------------------------------------------------------------------
# WHY THIS WAITS ON A PREDECESSOR AND NOT JUST ON "IDLE"
# ---------------------------------------------------------------------------
# The first version waited only for `Get-Process quartus*` to come back empty --
# exactly what `queue_gate1_maponly.ps1` is already waiting for. Two queues
# watching the same free-resource condition do not form a queue: they form a
# RACE, and both would have launched Quartus the instant gate 4 exited, which is
# the CPU contention this whole one-at-a-time discipline exists to avoid.
#
# It was caught before it fired, and the fix is that a queued job must wait for
# its PREDECESSOR to finish, not for the resource to look free. `map-g1-0.log` is
# gate 1's last output, so its existence is the predecessor's completion.
#
# Note the shape of the near-miss: nothing would have failed loudly. Two
# concurrent fits produce valid rows, just slower, and the contention only shows
# up as wall-clock nobody attributes to it.
$ErrorActionPreference = 'Stop'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

# Gate 1 writes map-g1-1.log then map-g1-0.log. The second one existing means it
# has finished its pair.
Write-Host 'newblock-maponly: waiting for gate 1 to finish (map-g1-0.log)...'
while (-not (Test-Path 'map-g1-0.log')) { Start-Sleep -Seconds 60 }
Write-Host 'newblock-maponly: gate 1 done; waiting for the toolchain to go idle...'
while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 60 }
Write-Host 'newblock-maponly: toolchain idle.'

foreach ($mod in @('zhao_texture_metajoin',
                   'zhao_texture_early_desc',
                   'zhao_texture_uv_join')) {
    Write-Host ("newblock-maponly: " + $mod)
    & "$PSScriptRoot\run_block_fit.ps1" -Module $mod -MapOnly 2>&1 |
        Tee-Object -FilePath ("map-newblock-" + $mod + ".log") | Select-Object -Last 4
}
Write-Host 'NEWBLOCKMAPONLY DONE'
