# sweep_smoke.ps1 -- run all ten console-core smoke forms, one at a time, and
# record each one's duration, %Fatal count, raster pixels and the BL8-alignment
# instrumentation summary printed by sim/models/zhao_sdram_model.sv.
#
# THE DURATION IS THE TELL: a real form takes 260-345 s. RC 1 in ~1 s is a
# verilation failure, not a test failure, and `LintOnly 0s` is verilator's
# content-keyed --skip-identical.
#
# Switches are passed as REAL switch parameters through `&`, never stringified
# through `powershell -File` -- that mistake killed nine forms in 0 s and
# reported clean (HANDOVER-20260919 15.7).
param([string]$Label = 'base')

$ErrorActionPreference = 'Continue'
$repo = 'C:\programmieren\zencrifice\gz-bursttruth'
$script = Join-Path $repo 'tests\prod\run_console_core_smoke.ps1'
$out = Join-Path $repo "subagents\20260923-bursttruth\smoke-$Label"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$forms = @(
  @{ n = 'plain';         a = @{} },
  @{ n = 'Mutant';        a = @{ Mutant = $true } },
  @{ n = 'UntexMutant';   a = @{ UntexMutant = $true } },
  @{ n = 'NoTableLoad';   a = @{ NoTableLoad = $true } },
  @{ n = 'BadDescriptor'; a = @{ BadDescriptor = $true } },
  @{ n = 'BadVertex';     a = @{ BadVertex = $true } },
  @{ n = 'NoEchoArm';     a = @{ NoEchoArm = $true } },
  @{ n = 'BadTraceArm';   a = @{ BadTraceArm = $true } },
  @{ n = 'GlowTag';       a = @{ GlowTag = $true } },
  @{ n = 'LintOnly';      a = @{ LintOnly = $true } }
)

$rows = @()
foreach ($f in $forms) {
  $log = Join-Path $out "$($f.n).log"
  $t0 = Get-Date
  # SPLAT A HASHTABLE VARIABLE. `@($f.a)` would wrap it in an ARRAY and pass
  # the hashtable as a positional argument -- which lands in $Unrecognised and
  # exits 2 in under a second while looking like a switch problem.
  $sw = $f.a
  & $script -Repo $repo @sw *> $log
  $rc = $LASTEXITCODE
  $secs = [int]((Get-Date) - $t0).TotalSeconds
  $fatal = (Select-String -Path $log -Pattern '%Fatal' -SimpleMatch | Measure-Object).Count
  $px = (Select-String -Path $log -Pattern 'raster pixels=\d+' | ForEach-Object { $_.Matches[0].Value }) -join ','
  $bl8 = (Select-String -Path $log -Pattern '\[bl8-align\] SUMMARY.*' | ForEach-Object { $_.Matches[0].Value }) -join ' | '
  $bl8n = (Select-String -Path $log -Pattern 'UNALIGNED' -SimpleMatch | Measure-Object).Count
  $line = "{0,-14} rc={1} {2,4}s fatal={3} px=[{4}] unalignedLines={5} {6}" -f $f.n, $rc, $secs, $fatal, $px, $bl8n, $bl8
  Write-Output $line
  $rows += $line
}
$rows | Set-Content -Encoding utf8 (Join-Path $out '_SUMMARY.txt')
