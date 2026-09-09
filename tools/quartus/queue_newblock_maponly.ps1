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

# The two COMBINE blocks ride along, because the question they answer is a
# MapOnly question and the batch is already paid for.
#
# reports/DSP-BUDGET-CENSUS-20260908.md lever 4: these two contain the same
# 8x8-plus-round multiply shape and cost very differently --
#
#   zhao_texture_material_combine_v1   (* multstyle = "logic" *)   2 DSP
#   zhao_texture_combine               no attribute               8 DSP
#
# `unit_mul` is called seven times directly and five more through `mul2x9`, so
# twelve 8x8 multipliers. The hypothesis is that the missing attribute is why one
# costs four times the other, and MapOnly reports DSP, so MapOnly settles it.
#
# It is filed as a HYPOTHESIS and not a claim for a specific reason: the last time
# a combiner read 8 DSP against a rule of 2, "Quartus is ignoring multstyle" was
# the obvious answer and it was WRONG -- the block really did contain fourteen
# multipliers inside two seven-arm case statements. This situation differs (the
# attribute is absent rather than present-and-ignored, and the sibling block
# demonstrates it working in this very tree) but it is the same shape of
# comfortable explanation, so it gets measured before anyone edits anything.
#
# Note also: prod_manifest.yml line 69 already flags zhao_texture_combine as
# "REFUTED (D19q); delete when v1 is measured". So the first question may be
# whether the block is wanted at all -- and v1's row is what that sentence waits
# for, which this batch also produces.
foreach ($mod in @('zhao_texture_metajoin',
                   'zhao_texture_early_desc',
                   'zhao_texture_uv_join',
                   'zhao_texture_combine',
                   'zhao_texture_material_combine_v1')) {
    Write-Host ("newblock-maponly: " + $mod)
    & "$PSScriptRoot\run_block_fit.ps1" -Module $mod -MapOnly 2>&1 |
        Tee-Object -FilePath ("map-newblock-" + $mod + ".log") | Select-Object -Last 4
}
Write-Host 'NEWBLOCKMAPONLY DONE'
