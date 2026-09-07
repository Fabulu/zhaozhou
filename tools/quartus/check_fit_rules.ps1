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
#
# ---------------------------------------------------------------------------
# STALENESS, ADDED 2026-09-07, AND THE NUMBER THAT PROMPTED IT
# ---------------------------------------------------------------------------
# Every row carries the commit its sources were at. Nothing looked at it. So a
# row could PASS or FAIL on numbers measured from a DIFFERENT VERSION of the
# block, and the report would say neither.
#
# Counted on the day this was added: 38 OF 87 MEASURED ROWS were stale -- the
# module's own .sv had commits after the fit that produced the row. Nine for
# zhao_cmd_dma, eight for zhao_raster_edgewalk, five for zhao_mem_guard.
#
# The one that found it was zhao_terrain_residency_v2, reported FAIL on
# `block memory 150528 bits < required 167936` from a fit taken two commits
# before HEAD. Whether that failure is real is not knowable from a row that
# describes an older block -- which is the whole point.
#
# THIS IS THE SAME FAULT THE REPO KEEPS RECORDING in another costume: a
# generated artefact nobody re-generates is a stale artefact with a reassuring
# provenance line at the top. A PASS on a stale row is worse than no row,
# because it reads as evidence.
#
# A stale row is NOT counted as a failure -- it is not a violation, it is an
# unanswered question -- but it is marked on its line and totalled at the end,
# so "6 pass, 10 FAIL" can no longer be read without knowing how much of it
# describes the machine that exists.
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

# Does the module's own source file have commits after the one its row was
# measured at? `git log <commit>..HEAD -- <file>` answers it. Returns the count,
# or -1 when the question cannot be asked (no commit recorded, no file found,
# git unavailable) -- which is reported as unknown rather than as fresh.
function Get-RowStaleness($Row, [string]$Name, [string]$Root) {
    $prop = $Row.PSObject.Properties['sourceCommit']
    if ($null -eq $prop -or [string]::IsNullOrWhiteSpace($prop.Value)) { return -1 }
    $commit = $prop.Value

    $file = Get-ChildItem -LiteralPath (Join-Path $Root 'fpga\rtl') -Recurse -Filter ($Name + '.sv') -ErrorAction SilentlyContinue |
            Select-Object -First 1
    if ($null -eq $file) { return -1 }

    $rel = $file.FullName.Substring($Root.Length).TrimStart('\', '/')
    try {
        $out = & git -C $Root log --oneline ($commit + '..HEAD') -- $rel 2>$null
    } catch {
        return -1
    }
    if ($LASTEXITCODE -ne 0) { return -1 }
    if ([string]::IsNullOrWhiteSpace(($out -join ''))) { return 0 }
    return @($out).Count
}

$fail = 0
$pass = 0
$unmeasured = 0
$stale = 0

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
    # STALE IS A SUFFIX, NOT A VERDICT. The row still passes or fails its rules;
    # what the suffix says is whether the numbers describe the block that is in
    # the tree right now.
    $since = Get-RowStaleness $row $name $RepoRoot
    $suffix = ''
    if ($since -gt 0) {
        $suffix = ("   [STALE: {0} commit(s) to the source since this fit]" -f $since)
        $stale++
    } elseif ($since -lt 0) {
        $suffix = '   [provenance unknown]'
    }

    $violations = @(Test-FitRules $row $rules[$name])
    if ($violations.Count -eq 0) {
        $colour = if ($since -gt 0) { 'Yellow' } else { 'Green' }
        Write-Host ("  PASS  {0}{1}" -f $name, $suffix) -ForegroundColor $colour
        $pass++
    } else {
        Write-Host ("  FAIL  {0}{1}" -f $name, $suffix) -ForegroundColor Red
        foreach ($v in $violations) { Write-Host ("          {0}" -f $v) -ForegroundColor Red }
        $fail++
    }
}

Write-Host ""
Write-Host ("{0} pass, {1} FAIL, {2} unmeasured, {3} STALE" -f $pass, $fail, $unmeasured, $stale)
if ($stale -gt 0) {
    Write-Host ("  {0} row(s) were measured from a source that has since changed. Their verdicts describe an older block; refit before believing either half." -f $stale) -ForegroundColor Yellow
}
if ($fail -gt 0) {
    Write-Host "A fit that meets Fmax while violating its memory/DSP structure is not a pass." -ForegroundColor Red
    exit 1
}
exit 0
