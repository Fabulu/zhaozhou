# edgeclose-lint-core.ps1 -- lint zhao_console_core against ITS OWN closure.
#
# WHY THIS EXISTS AND WHY IT IS COMMITTED. Linting `fpga/rtl/**/*.sv` in bulk
# pulls in the GENERATED fit tops, which are stale between a port change and a
# regeneration, so it reports PINMISSING warnings about a file nobody is asking
# about and buries the ones that matter. The authoritative closure is
# `design/fit_targets.yml`'s `zhao_console_core` source list -- the same list
# `tests/prod/run_console_core_smoke.ps1` parses -- and linting exactly that is
# the question "does the composed console elaborate?".
#
# Usage (repo root, with tools/env/zhao-env.ps1 sourced):
#   powershell -File tools/quartus/edgeclose-lint-core.ps1
param([string]$Repo = (Get-Location).Path)

$ErrorActionPreference = 'Stop'
$repoFwd = $Repo -replace '\\', '/'
$vl = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe'

# Parse the closure exactly as the smoke runner does.
$srcRel = @()
$inTarget = $false
foreach ($line in Get-Content "$Repo\design\fit_targets.yml") {
  if ($line -match '^\s*- top:\s*(\S+)') { $inTarget = ($Matches[1] -eq 'zhao_console_core'); continue }
  if ($inTarget -and $line -match '^\s*- (fpga/.*\.sv)\s*$') { $srcRel += $Matches[1] }
}
if ($srcRel.Count -lt 50) { throw "fit_targets.yml gave only $($srcRel.Count) sources for zhao_console_core" }

$srcs = $srcRel | ForEach-Object { "$repoFwd/$_" }
Write-Host "closure: $($srcRel.Count) RTL sources from fit_targets.yml"

& $vl --lint-only -Wall -Wno-DECLFILENAME -Wno-UNUSEDPARAM --timing `
      --top-module zhao_console_core $srcs
$rc = $LASTEXITCODE
Write-Host "LINT_RC=$rc"
exit $rc
