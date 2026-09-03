# Round 3: measure what changed, and run the seed sweep that the rcp24 skew
# question actually needs. Sources for every module below are FROZEN while this
# runs -- the QSF names them by absolute path in the live working tree, so an
# edit mid-fit either contaminates the row or, worse, does not.
$ErrorActionPreference = 'Continue'

$jobs = @(
  @{ m = 'zhao_raster_texjoin_v2';  seed = 0; label = '' },   # work FIFO + registered outputs
  @{ m = 'zhao_texture_aux_pipe';   seed = 0; label = '' },   # input boundary registered
  @{ m = 'zhao_texture_tmu_plan';   seed = 0; label = '' },   # narrowed to MAXLOG2
  @{ m = 'zhao_raster_rcp24_svc';   seed = 2; label = 'seed2' },
  @{ m = 'zhao_raster_rcp24_svc';   seed = 3; label = 'seed3' }
)

foreach ($j in $jobs) {
  Write-Host ("=== FIT " + $j.m + " seed=" + $j.seed + " " + $j.label + " ===")
  try {
    if ($j.label -ne '') {
      & .\tools\quartus\run_block_fit.ps1 -Module $j.m -Seed $j.seed -RowLabel $j.label -TimeoutSeconds 9000
    } else {
      & .\tools\quartus\run_block_fit.ps1 -Module $j.m -TimeoutSeconds 9000
    }
  } catch {
    Write-Host ("ROW FAILED " + $j.m + " : " + $_)
  }
  Write-Host ("=== DONE " + $j.m + " " + $j.label + " ===")
}
Write-Host '=== ROUND 3 DONE ==='
