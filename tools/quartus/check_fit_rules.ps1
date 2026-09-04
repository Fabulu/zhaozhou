# check_fit_rules.ps1 -- audit every RECORDED fit against its structural rules.
#
# Runs no Quartus. It reads `design/fit_targets.yml` and the rows already in
# `reports/synthesis/zhao_block_fit.json`, so the question "does the machine we
# have obey the architecture's storage and DSP laws?" can be answered in a
# second, at any time, including while a fit is running.
#
# Exit code 1 if any recorded row violates its rules, so this is usable as a
# gate. A block with rules but no recorded fit is reported and is NOT a failure
# -- it has not been measured yet, which is a different thing from failing.
[CmdletBinding()]
param(
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'fit_rules.ps1')

$rulesPath = Join-Path $RepoRoot 'design\fit_targets.yml'
$jsonPath  = Join-Path $RepoRoot 'reports\synthesis\zhao_block_fit.json'

$rules = Read-FitRules $rulesPath
if ($rules.Count -eq 0) {
    # No backticks in a double-quoted PowerShell string: backtick is the escape
    # character, so a markdown-style `rules:` becomes a carriage return.
    Write-Host 'no rules: blocks in design/fit_targets.yml -- nothing to enforce' -ForegroundColor Yellow
    exit 0
}

$rows = @()
if (Test-Path -LiteralPath $jsonPath) {
    $json = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    $rows = if ($json.PSObject.Properties['blocks']) { $json.blocks } else { $json }
}

$fail = 0
$pass = 0
$unmeasured = 0

Write-Host ("structural rules: {0} block(s) carry them" -f $rules.Count)
Write-Host ""

foreach ($name in ($rules.Keys | Sort-Object)) {
    $row = $rows | Where-Object { $_.module -eq $name } | Select-Object -First 1
    if ($null -eq $row) {
        Write-Host ("  ----  {0}   (no recorded fit -- not measured, not failed)" -f $name) -ForegroundColor DarkGray
        $unmeasured++
        continue
    }
    # A row can EXIST and carry no numbers. A fit that failed, timed out, or was
    # killed writes its row with every resource field null, and every rule in
    # Test-FitRules is guarded by `$null -ne $x` -- so all of them silently pass
    # and the block is reported PASS in green.
    #
    # Measured 2026-09-04: zhao_texture_tmu_pipe reported PASS against a fresh
    # `max_registers: 12000` while being the one block in the tree holding a
    # 65,536-bit palette cache in flip-flops (72,824 registers, D19m). Its row
    # said `status: failed:quartus_fit.exe` with registers null. The gate was
    # green on the worst block it had.
    #
    # A missing measurement is not a pass. It is the same fact as "no recorded
    # fit" and is now counted with it.
    # StrictMode rejects reading a property that is ABSENT (not merely null), and
    # rows differ in which fields they carry, so ask the object rather than the
    # dot-accessor.
    $hasNumbers = $false
    foreach ($f in 'registers', 'alms', 'blockMemoryBits', 'ramBlocks') {
        $prop = $row.PSObject.Properties[$f]
        if ($null -ne $prop -and $null -ne $prop.Value) { $hasNumbers = $true; break }
    }
    $rowStatus = if ($row.PSObject.Properties['status']) { $row.status } else { '?' }
    if (-not $hasNumbers) {
        Write-Host ("  ----  {0}   (row exists but carries NO resource numbers, status '{1}' -- not measured, not passed)" -f $name, $rowStatus) -ForegroundColor DarkGray
        $unmeasured++
        continue
    }
    # @() so an EMPTY result is still countable: PowerShell unrolls a
    # zero-item List to $null and StrictMode then rejects .Count.
    $violations = @(Test-FitRules $row $rules[$name])
    if ($violations.Count -eq 0) {
        Write-Host ("  PASS  {0}" -f $name) -ForegroundColor Green
        $pass++
    } else {
        Write-Host ("  FAIL  {0}" -f $name) -ForegroundColor Red
        foreach ($v in $violations) { Write-Host ("          {0}" -f $v) -ForegroundColor Red }
        $fail++
    }
}

Write-Host ""
Write-Host ("{0} pass, {1} FAIL, {2} unmeasured" -f $pass, $fail, $unmeasured)
if ($fail -gt 0) {
    Write-Host "A fit that meets Fmax while violating its memory/DSP structure is not a pass." -ForegroundColor Red
    exit 1
}
exit 0
