# Round 4: the blocks whose reported Fmax says nothing about their logic.
#
# tools/quartus/internal_paths.py found that for these, ALL 200 archived worst
# paths touch a top-level port -- so no register-to-register path was reported
# at all. block_paths.tcl now asks for 2000, which is what these re-runs are
# for. The RTL is unchanged; only the report is deeper.
$ErrorActionPreference = 'Continue'

$jobs = @(
  @{ m = 'zhao_texture_aux_pipe';     seed = 0; label = '' },
  @{ m = 'zhao_texture_rsp_dispatch'; seed = 0; label = '' },
  @{ m = 'zhao_raster_perspuv_svc';   seed = 0; label = '' },
  @{ m = 'zhao_texture_cache_pipe';   seed = 0; label = '' }
)

foreach ($j in $jobs) {
  Write-Host ("=== FIT " + $j.m + " ===")
  try {
    & .\tools\quartus\run_block_fit.ps1 -Module $j.m -TimeoutSeconds 9000
  } catch {
    Write-Host ("ROW FAILED " + $j.m + " : " + $_)
  }
  Write-Host ("=== DONE " + $j.m + " ===")
}
Write-Host '=== ROUND 4 DONE ==='
