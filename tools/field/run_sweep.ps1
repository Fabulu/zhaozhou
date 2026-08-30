# run_sweep.ps1 — build every Field v3 Earth sweep point and tabulate it.
#
# The sweep points live in tests/CMakeLists.txt (ZHAO_FIELD_SWEEP_POINTS) so
# that a configuration is a committed artefact rather than a line of somebody's
# shell history. This script only RUNS them and prints the table; it decides
# nothing.
#
#   pwsh tools/field/run_sweep.ps1
#   pwsh tools/field/run_sweep.ps1 -Points 2048       # slower, steadier
#
# Every point runs the same three Earth programs against the same oracle. A
# point that reports any WRONG VALUES is printed and NOT compared on speed --
# a fast wrong machine is not a data point.
#
# POINTS DEFAULTS TO 1024 AND MUST NOT GO BELOW IT. At 256 points a CTX=32,
# LANES=4 machine spends most of the run filling and draining 32 contexts, and
# the measurement is the RAMP rather than the steady state: crater_ring reads
# 28 clocks per group at 256 points and 19 at 1024, which is the difference
# between "1.23x over the admission law" and "fits with 22% margin". The
# initiation interval is a steady-state quantity and a short run cannot see it.
param(
  [int]$Points = 1024,
  [string]$Build = "build"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$progs = @(
  "$root/compiler/tests/generated/crater_ring.zprog",
  "$root/compiler/tests/generated/impact_wave.zprog",
  "$root/compiler/tests/generated/wave_pool.zprog"
)

$exes = Get-ChildItem "$root/$Build/tests" -Filter "test_field_v3_earth_sweep_*.exe" |
        Sort-Object Name
if (-not $exes) {
  Write-Host "No sweep binaries. Configure with -DZHAO_FIELD_SWEEP=ON and build target field_sweep."
  exit 1
}

$rows = @()
foreach ($e in $exes) {
  $name = $e.BaseName -replace '^test_field_v3_earth_sweep_', ''
  Write-Host "== $name =="
  $out = & $e.FullName --points $Points @progs 2>&1 | Out-String

  $wrong = ([regex]::Matches($out, 'WRONG VALUES\s+(\d+)') |
            ForEach-Object { [int]$_.Groups[1].Value } | Measure-Object -Sum).Sum
  $fails = 0
  if ($out -match '(\d+)\s+failure\(s\)') { $fails = [int]$Matches[1] }

  # The frame cost of the QUAD drive, per program: that is the drive the
  # admission law is quoted against.
  $frames = [regex]::Matches($out, '(\S+\.zprog)\s+QUAD\s+group\s+(\d+)\s+association\s+(\d+)\s+frame\s+(\d+)')
  $worst = 0
  $per = @{}
  foreach ($m in $frames) {
    $p = [IO.Path]::GetFileNameWithoutExtension($m.Groups[1].Value)
    $f = [int]$m.Groups[4].Value
    $per[$p] = $f
    if ($f -gt $worst) { $worst = $f }
  }
  # Long-op freeze, averaged over the programs -- the number the sweep exists
  # to move.
  $fr = [regex]::Matches($out, 'FROZEN by a long op awaiting the dispatcher\s+(\d+)\s+clocks\s+\((\d+)%\)') |
        ForEach-Object { [int]$_.Groups[2].Value }
  $frozen = if ($fr) { [math]::Round(($fr | Measure-Object -Average).Average) } else { 0 }

  $rows += [pscustomobject]@{
    point   = $name
    crater  = $per['crater_ring']
    impact  = $per['impact_wave']
    wave    = $per['wave_pool']
    worst   = $worst
    frozen  = "$frozen%"
    exact   = ($wrong -eq 0 -and $fails -eq 0)
  }
}

Write-Host ""
Write-Host "== QUAD-drive frame clocks, against the 850,000 admission law =="
$rows | Sort-Object worst | Format-Table -AutoSize
$best = $rows | Where-Object { $_.exact } | Sort-Object worst | Select-Object -First 1
if ($best) {
  $margin = [math]::Round((850000 - $best.worst) / 850000 * 100, 1)
  Write-Host "best exact point: $($best.point) at $($best.worst) -- $margin% margin"
}
