[CmdletBinding()]
param(
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [string]$Family = '',
    [switch]$Fit,
    [switch]$KeepWorkspace
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------------------
# Map (optionally fit) the generated calibration microbenches, ONE AT A TIME.
#
# The point of these is to stop the audit guessing the multiplier and memory
# mapping. design/budgets/dsp.md's rule is now "operator count is a LOWER
# BOUND, exact only while every operand stays inside one block's native width"
# -- which is honest and unusable until something says what the cost IS at 24,
# 33, 40 and 64 bits, signed and unsigned.
#
# MAP, NOT FIT, BY DEFAULT. Analysis & Synthesis decides DSP inference and RAM
# inference; the fitter only places what synthesis already chose. Cross-checked
# on real blocks in the same session: zhao_texture_tmu maps at 6 DSPs against a
# fitted 6, and zhao_terrain_project maps at 33 against a fitted 33. Map costs
# ~15 s per point where a constrained fit costs 300-1300 s, and 90 points is
# the difference between half an hour and a day.
#
# -Fit is available for the handful of points where a real ALM count or an Fmax
# is wanted, and those rows say `stage: fit` so the two can never be mistaken
# for one another.
# ---------------------------------------------------------------------------

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$CalibDir = Join-Path $RepoRoot 'build-budget\calib'
$manifestPath = Join-Path $CalibDir 'manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "No manifest at $manifestPath -- run: python tools/budget/gen_calib.py"
}
$manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
$points = $manifest.points
if ($Family) { $points = @($points | Where-Object { $_.family -eq $Family }) }

if ($SkipMeasured) {
    $done = @{}
    $destPath = Join-Path $RepoRoot 'toolsudget\calibration.json'
    if (Test-Path -LiteralPath $destPath) {
        $prior = [IO.File]::ReadAllText($destPath) | ConvertFrom-Json
        foreach ($r in $prior.points) { if ($r.status -eq 'ok') { $done[$r.module] = $true } }
    }
    $before = $points.Count
    $points = @($points | Where-Object { -not $done.ContainsKey($_.module) })
    Write-Host ("-SkipMeasured: {0} of {1} already ok; {2} to go" -f ($before - $points.Count), $before, $points.Count)
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
$Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-calib-{0}-{1}" -f $PID, [DateTime]::UtcNow.Ticks)
New-Item -ItemType Directory -Path $Workspace -Force | Out-Null

# ---------------------------------------------------------------------------
# WNS AND TNS, FROM THE TABLE THE TOOL ACTUALLY WRITES
#
# This script used to extract slack with
#
#     '(?m)^\s*Worst-case Setup Slack\D+(-?[0-9.]+)'
#
# and that regex HAS NEVER ONCE MATCHED. Found 2026-08-23 (RUN-20260823-2226)
# by asking reports/synthesis/zhao_block_fit.json how many of its 41 rows carry
# a slack figure: ZERO -- including the four rows that carry an Fmax read out
# of the very same .sta.rpt. Quartus 17.0.2's STA report has no such line. What
# it has is a summary table per analysis:
#
#     +--------------------------------------------------+
#     ; Setup Summary                                    ;
#     +-------+-------+---------------+------------------+
#     ; Clock ; Slack ; End Point TNS ; ...
#     +-------+-------+---------------+
#     ; clk   ; -17.691 ; -8901.234   ;
#
# So the fix reads the table -- and the table carries TOTAL NEGATIVE SLACK as
# well, which the ledger has never had at all. A block holding one path at
# -0.1 ns and one holding four hundred were previously indistinguishable.
#
# Same failure shape as every entry in reports/QUARTUS_GOTCHAS.md: an
# extraction that fails silently, whose only symptom is a number that never
# appears. Do not trust a new field here until a real report has been read.
# ---------------------------------------------------------------------------
function Get-StaSummary([string]$Text, [string]$Section) {
    $i = $Text.IndexOf("; $Section")
    if ($i -lt 0) { return $null }
    $tail = $Text.Substring($i, [Math]::Min(2000, $Text.Length - $i))
    # First data row after the header separator: `; <clock> ; <slack> ; <tns> ;`
    $m = [regex]::Match($tail, '(?m)^;\s*[^;]+;\s*(-?[0-9]+\.[0-9]+)\s*;\s*(-?[0-9]+\.[0-9]+)\s*;')
    if (-not $m.Success) { return $null }
    return [ordered]@{
        slackNs = [double]$m.Groups[1].Value
        tnsNs   = [double]$m.Groups[2].Value
    }
}

function Get-TableField([string]$Text, [string[]]$Labels) {
    foreach ($label in $Labels) {
        $m = [regex]::Match($Text, '(?m)^;\s*' + [regex]::Escape($label) + '\s*;\s*([0-9][0-9,]*)\s*;')
        if ($m.Success) { return [int64]::Parse($m.Groups[1].Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture) }
    }
    return $null
}
function Get-SummaryField([string]$Text, [string[]]$Labels) {
    foreach ($label in $Labels) {
        $m = [regex]::Match($Text, "(?im)^\s*" + [regex]::Escape($label) + "\s*:\s*([^\r\n]+)")
        if ($m.Success) {
            $n = [regex]::Match($m.Groups[1].Value, '-?[0-9][0-9,]*')
            if ($n.Success) { return [int64]::Parse($n.Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture) }
        }
    }
    return $null
}


# ---------------------------------------------------------------------------
# WRITE AFTER EVERY POINT, NOT AT THE END.
#
# The first version of this script accumulated 102 measurements in memory and
# serialised them once, after the loop. Measured 2026-08-24: the background
# harness hosting it hit its lifetime limit at point 97 of 102 and **all 97
# measurements were lost** -- forty minutes of Quartus time, and every one of
# them already printed to the console where they could be read but not merged.
#
# tools/quartus/map_sweep.ps1 was written the same night specifically to avoid
# this, by invoking its runner once per module. The lesson did not travel the
# ten metres to this file. So: merge-and-write after every single point, which
# costs a few milliseconds and makes any kill recoverable.
# ---------------------------------------------------------------------------
function Save-Calibration([object[]]$NewRows) {
    $dest = Join-Path $RepoRoot 'tools\budget\calibration.json'
    $merged = [ordered]@{}
    if (Test-Path -LiteralPath $dest) {
        try {
            $prior = [IO.File]::ReadAllText($dest) | ConvertFrom-Json
            foreach ($r in $prior.points) { $merged[($r.module + ':' + $r.stage)] = $r }
        } catch { Write-Warning "existing $dest unparseable; replacing" }
    }
    foreach ($r in $NewRows) { $merged[($r.module + ':' + $r.stage)] = $r }
    $out = [ordered]@{
        schemaVersion = 1
        purpose = 'Measured mapping from RTL arithmetic and storage shapes to Cyclone V resources, on THIS tool and THIS device.'
        tool = [ordered]@{ name = 'Quartus Prime Lite'; version = '17.0.2' }
        device = '5CSEBA6U23I7'
        sourceCommit = $head
        generator = 'tools/budget/gen_calib.py + tools/quartus/run_calib.ps1'
        points = @($merged.Keys | Sort-Object | ForEach-Object { $merged[$_] })
        limitations = @(
            'Rows with stage=map-only carry NO timing. estimatedAlms is an Analysis and Synthesis estimate, not a placed ALM count.',
            'Each microbench is a lone module with virtual pins. It characterises the SHAPE, not what the same shape costs surrounded by a real design.',
            'Allow Any RAM Size For Recognition is DISABLED in the project settings these were run under, matching the design lane. A RAM result here is what the design would get, not what a permissive setting could get.',
            'Nothing here is a programmed device.'
        )
    }
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null
    [IO.File]::WriteAllText($dest, (($out | ConvertTo-Json -Depth 8) + "`n"), $Utf8NoBom)
}

$rows = New-Object 'System.Collections.Generic.List[object]'
$i = 0
try {
    foreach ($p in $points) {
        $i++
        $mod = $p.module
        $dir = Join-Path $Workspace $mod
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        $rev = if ($Fit) { 'calibfit' } else { 'calibmap' }
        "PROJECT_REVISION = `"$rev`"" | Set-Content -LiteralPath (Join-Path $dir "$rev.qpf") -Encoding ascii

        $src = (Join-Path $RepoRoot $p.file).Replace('\', '/')
        $qsf = @(
            '# Generated by tools/quartus/run_calib.ps1.',
            'set_global_assignment -name FAMILY "Cyclone V"',
            'set_global_assignment -name DEVICE 5CSEBA6U23I7',
            "set_global_assignment -name TOP_LEVEL_ENTITY $mod",
            'set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files',
            'set_global_assignment -name NUM_PARALLEL_PROCESSORS 4',
            'set_global_assignment -name SEED 1',
            'set_global_assignment -name OPTIMIZATION_MODE "BALANCED"',
            'set_global_assignment -name FITTER_EFFORT "STANDARD FIT"',
            "set_global_assignment -name SYSTEMVERILOG_FILE $src",
            'set_instance_assignment -name VIRTUAL_PIN ON -to *'
        )
        if ($Fit) { $qsf += 'set_global_assignment -name SDC_FILE calib.sdc' }
        $qsf | Set-Content -LiteralPath (Join-Path $dir "$rev.qsf") -Encoding ascii

        if ($Fit) {
            # Same model as the per-block lane: clock constrained AND I/O
            # delays declared, because QUARTUS_GOTCHAS 9 records TimeQuest
            # silently excluding every pin-to-register path when none is given.
            @(
                'create_clock -name clk -period 10.000 [get_ports {clk}]',
                'derive_clock_uncertainty',
                'set _c [get_ports -nowarn {clk}]',
                'if {[get_collection_size $_c] > 0} {',
                '    set _din [remove_from_collection [all_inputs] $_c]',
                '    if {[get_collection_size $_din] > 0} { set_input_delay -clock clk 0.000 $_din }',
                '    if {[get_collection_size [all_outputs]] > 0} { set_output_delay -clock clk 0.000 [all_outputs] }',
                '}'
            ) | Set-Content -LiteralPath (Join-Path $dir 'calib.sdc') -Encoding ascii
        }

        $stageName = if ($Fit) { 'fit' } else { 'map-only' }
        $row = [ordered]@{ module = $mod; stage = $stageName }
        foreach ($k in $p.PSObject.Properties.Name) {
            if ($k -ne 'module') { $row[$k] = $p.$k }
        }
        $row.sourceCommit = $head
        $row.tool = 'Quartus Prime Lite 17.0.2'
        $row.device = '5CSEBA6U23I7'

        $sw = [Diagnostics.Stopwatch]::StartNew()
        $ok = $true
        Push-Location $dir
        try {
            $stages = if ($Fit) { @('quartus_map.exe', 'quartus_fit.exe', 'quartus_sta.exe') } else { @('quartus_map.exe') }
            foreach ($exe in $stages) {
                & (Join-Path $QuartusBin $exe) $rev *> (Join-Path $dir "$exe.log")
                if ($LASTEXITCODE -ne 0) { $row.status = "failed:$exe"; $ok = $false; break }
            }
        } finally { Pop-Location }
        $sw.Stop()
        $row.seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

        if ($ok) {
            $ms = Join-Path $dir "output_files\$rev.map.summary"
            if (Test-Path -LiteralPath $ms) {
                $t = [IO.File]::ReadAllText($ms)
                $row.status = 'ok'
                $row.dspBlocks = Get-SummaryField $t @('Total DSP Blocks')
                $row.blockMemoryBits = Get-SummaryField $t @('Total block memory bits')
                $row.registers = Get-SummaryField $t @('Total registers')
            }
            $mr = Join-Path $dir "output_files\$rev.map.rpt"
            if (Test-Path -LiteralPath $mr) {
                $r = [IO.File]::ReadAllText($mr)
                $row.estimatedAlms = Get-TableField $r @('Estimate of Logic utilization (ALMs needed)')
                $row.combAluts     = Get-TableField $r @('Combinational ALUT usage for logic')
                $row.mlabMemoryBits = Get-TableField $r @('Total MLAB memory bits')
                $d = [ordered]@{}
                foreach ($lbl in @('Two Independent 18x18', 'Sum of two 18x18', 'Sum of four 18x18',
                                   'Total number of DSP blocks', 'Fixed Point Signed Multiplier',
                                   'Fixed Point Unsigned Multiplier', 'Fixed Point Mixed Sign Multiplier')) {
                    $m = [regex]::Match($r, '(?m)^;\s*' + [regex]::Escape($lbl) + '\s*;\s*(\d+)\s*;')
                    if ($m.Success) { $d[$lbl] = [int]$m.Groups[1].Value }
                }
                if ($d.Count -gt 0) { $row.dspDecomposition = $d }
                $mem = @()
                foreach ($m in [regex]::Matches($r, '(?m)^;\s*([^;]{3,200}?)\s*;\s*(AUTO|M10K|MLAB|LCs?)\s*;\s*([A-Za-z0-9 ]+?)\s*;\s*(\d+)\s*;\s*(\d+)\s*;')) {
                    $mem += ('{0} [{1} {2} {3}x{4}]' -f $m.Groups[1].Value.Trim(), $m.Groups[2].Value.Trim(),
                             $m.Groups[3].Value.Trim(), $m.Groups[4].Value, $m.Groups[5].Value)
                }
                $row.inferredMemories = @($mem | Select-Object -Unique)
            }
            if ($Fit) {
                $fs = Join-Path $dir "output_files\$rev.fit.summary"
                if (Test-Path -LiteralPath $fs) {
                    $t = [IO.File]::ReadAllText($fs)
                    $row.fittedAlms = Get-SummaryField $t @('Logic utilization (in ALMs)', 'Logic utilization')
                    $row.fittedRamBlocks = Get-SummaryField $t @('Total RAM Blocks')
                    $row.fittedDspBlocks = Get-SummaryField $t @('Total DSP Blocks')
                }
                $st = Join-Path $dir "output_files\$rev.sta.rpt"
                if (Test-Path -LiteralPath $st) {
                    $s = [IO.File]::ReadAllText($st)
                    $m = [regex]::Match($s, '(?m)^;\s*([0-9.]+)\s*MHz\s*;\s*([0-9.]+)\s*MHz\s*;\s*(\S+)')
                    if ($m.Success) { $row.fmaxMhz = [double]$m.Groups[2].Value }
                    $setup = Get-StaSummary $s 'Setup Summary'
                    if ($null -ne $setup) { $row.setupSlackNs = $setup.slackNs; $row.setupTnsNs = $setup.tnsNs }
                    $hold = Get-StaSummary $s 'Hold Summary'
                    if ($null -ne $hold) { $row.holdSlackNs = $hold.slackNs; $row.holdTnsNs = $hold.tnsNs }
                    # Evidence that the fit was CONSTRAINED at all.
                    $row.constraintEvidence = [regex]::IsMatch($s, '10\.000\s+clk')
                }
            }
        }
        if (-not $row.Contains('status')) { $row.status = 'no-summary' }
        $rows.Add([pscustomobject]$row)
        $dsp = if ($row.Contains('dspBlocks') -and $null -ne $row.dspBlocks) { $row.dspBlocks } else { '-' }
        $bmb = if ($row.Contains('blockMemoryBits') -and $null -ne $row.blockMemoryBits) { $row.blockMemoryBits } else { '-' }
        $alm = if ($row.Contains('estimatedAlms') -and $null -ne $row.estimatedAlms) { $row.estimatedAlms } else { '-' }
        Write-Host ("[{0}/{1}] {2,-38} {3,-12} {4,6}s  DSP {5,-4} memBits {6,-7} ALM~ {7}" -f
            $i, $points.Count, $mod, $row.status, $row.seconds, $dsp, $bmb, $alm)
        Save-Calibration $rows
        if (-not $KeepWorkspace) { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
    }

    Save-Calibration $rows
} finally {
    if (-not $KeepWorkspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    }
}
