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
$ErrorActionPreference = 'Stop'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

Write-Host 'newblock-maponly: waiting for the toolchain to go idle...'
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
