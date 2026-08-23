[CmdletBinding()]
param(
    [string[]]$Module,
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    # 1800, and the number is measured rather than picked: zhao_surface_sheet
    # MAPS in 1,096 s. A budget under that would have recorded a timeout for a
    # module that completes, which is the exact mistake run_block_fit.ps1's
    # header documents at the 900 s default.
    [int]$TimeoutSeconds = 1800,
    [string[]]$TopParameters,
    [string]$RowLabel = '',
    [switch]$KeepWorkspace,
    [switch]$ListOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# MAP-ONLY per-block characterization.  RUN-20260823-2226 (budget audit wave 1).
#
# WHY THIS EXISTS, and why it is not a worse run_block_fit.ps1:
#
# The census covers 41 of 94 RTL modules. Closing that gap with FITS is not
# affordable -- a constrained fit of one small block now costs 300-1300 s and
# zhao_forge_cliff has already burned 5,000+ s without finishing. But the
# questions that rank work are answered by ANALYSIS AND SYNTHESIS alone:
#
#   * how many DSP BLOCKS does this module infer, AND their decomposition --
#     Analysis & Synthesis prints `Total DSP Blocks` plus a `Two Independent
#     18x18` / `Sum of two 18x18` / signed-unsigned-mixed breakdown, which is
#     exactly the width-and-signedness evidence design/budgets/dsp.md now
#     demands and which no fit summary carries
#   * does the storage in it INFER AS RAM, or become flops and muxes
#   * roughly how big is it (estimated ALUTs / registers)
#   * do its parameters actually elaborate differently
#   * does it even COMPILE under Quartus 17.0.2's parser (GOTCHAS 1 and 8 were
#     both parser errors that three other frontends accepted)
#
# What map CANNOT answer, and what therefore must never be read out of this
# file: Fmax, slack, real placed ALM counts, routing. Those need a fit. The row
# schema below deliberately has no timing fields at all rather than null ones,
# so nothing can quote a timing number that was never measured.
#
# INHERITED DELIBERATELY from run_block_fit.ps1, because each exists to stop a
# specific loss that has already happened once:
#   * merge-never-replace, and a failed run KEEPS the prior good row
#   * per-invocation workspace uniquifier (not $PID -- three fits of one module
#     from one shell overwrote each other's evidence)
#   * -c core.autocrlf=true on the git status, or every CRLF file reads dirty
#   * the VIRTUAL_PIN wildcard, which is right for a retargetable leaf lane and
#     wrong for the composed top
#
# NEW HERE: sourceListHash. The fit lane's rows say which COMMIT they were
# measured at but not which FILES were in the project, and the shell QSF has
# already drifted from the CMake source list once (see zhao_shell_fit.qsf's own
# comment). A row whose source list cannot be reconstructed is not provenance.
# ---------------------------------------------------------------------------

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$mapExe = Join-Path $QuartusBin 'quartus_map.exe'
if (-not (Test-Path -LiteralPath $mapExe -PathType Leaf)) {
    throw "Required Quartus executable not found: $mapExe"
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve HEAD.' }
$dirtyRtl = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain -- fpga/rtl) -join ''
$rtlClean = [string]::IsNullOrWhiteSpace($dirtyRtl)

# ---- THE SOURCE LIST -------------------------------------------------------
# EVERY .sv under fpga/rtl, packages first. Quartus elaborates only what the
# top reaches, and parsing the whole cone was measured FREE (0.24 GB, 23 s) in
# run_block_fit.ps1's own header -- so a per-module cone list buys nothing and
# costs a maintenance surface that has already drifted once.
$pkgOrder = @(
    'fpga/rtl/generated/zhao_abi_pkg.sv',
    'fpga/rtl/common/zhao_pkg.sv',
    'fpga/rtl/memory/zhao_sdram_params_pkg.sv'
)
$allSv = Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'fpga\rtl') -Recurse -Filter '*.sv' |
    ForEach-Object { $_.FullName.Substring($RepoRoot.Length + 1).Replace('\', '/') } |
    Sort-Object
$sources = @()
$sources += $pkgOrder
$sources += ($allSv | Where-Object { $pkgOrder -notcontains $_ })

$sha = [Security.Cryptography.SHA256]::Create()
$hashBytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($sources -join "`n")))
$sourceListHash = ([BitConverter]::ToString($hashBytes) -replace '-', '').ToLowerInvariant().Substring(0, 16)

if ($ListOnly) {
    $sources | ForEach-Object { Write-Host $_ }
    Write-Host ("sourceListHash = {0}  ({1} files)" -f $sourceListHash, $sources.Count)
    return
}

if (-not $Module -or $Module.Count -eq 0) { throw 'Give -Module.' }

$Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-block-map-{0}-{1}" -f $PID, [DateTime]::UtcNow.Ticks)
New-Item -ItemType Directory -Path $Workspace -Force | Out-Null
$results = New-Object 'System.Collections.Generic.List[object]'

function Get-Field([string]$Text, [string[]]$Labels) {
    foreach ($label in $Labels) {
        $m = [regex]::Match($Text, "(?im)^\s*" + [regex]::Escape($label) + "\s*:\s*([^\r\n]+)")
        if ($m.Success) {
            $n = [regex]::Match($m.Groups[1].Value, '-?[0-9][0-9,]*')
            if ($n.Success) {
                return [int64]::Parse($n.Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture)
            }
        }
    }
    return $null
}

# The .map.summary is `Label : Value` lines; the .map.rpt is ASCII TABLES whose
# rows are `; Label ; Value ;`. Reading the report with the summary's parser
# returns null for every field, which is how the first draft of this script
# reported estimatedAlms = null next to a report line reading
# `; Estimate of Logic utilization (ALMs needed) ; 5028 ;`. Two formats, two
# parsers, and the row now says which one produced each number.
function Get-TableField([string]$Text, [string[]]$Labels) {
    foreach ($label in $Labels) {
        $m = [regex]::Match($Text, '(?m)^;\s*' + [regex]::Escape($label) + '\s*;\s*([0-9][0-9,]*)\s*;')
        if ($m.Success) {
            return [int64]::Parse($m.Groups[1].Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture)
        }
    }
    return $null
}

try {
    foreach ($mod in $Module) {
        $dir = Join-Path $Workspace $mod
        New-Item -ItemType Directory -Path $dir -Force | Out-Null

        # No SDC. Analysis & Synthesis has no timing objective to meet and this
        # lane reports no timing number, so handing it one would only invite a
        # reader to believe a constrained fit had happened.
        'PROJECT_REVISION = "blockmap"' | Set-Content -LiteralPath (Join-Path $dir 'blockmap.qpf') -Encoding ascii

        $qsf = @(
            '# Generated per module by tools/quartus/run_block_map.ps1. Map only.',
            'set_global_assignment -name FAMILY "Cyclone V"',
            'set_global_assignment -name DEVICE 5CSEBA6U23I7',
            "set_global_assignment -name TOP_LEVEL_ENTITY $mod",
            'set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files',
            'set_global_assignment -name VERILOG_MACRO "QUARTUS_SYNTHESIS=1"',
            'set_global_assignment -name NUM_PARALLEL_PROCESSORS 4',
            'set_global_assignment -name SEED 1',
            'set_global_assignment -name OPTIMIZATION_MODE "BALANCED"'
        )
        foreach ($s in $sources) {
            $abs = (Join-Path $RepoRoot $s).Replace('\', '/')
            $qsf += "set_global_assignment -name SYSTEMVERILOG_FILE $abs"
        }
        $qsf += 'set_instance_assignment -name VIRTUAL_PIN ON -to *'
        foreach ($tp in $TopParameters) {
            $kv = $tp -split '=', 2
            if ($kv.Count -ne 2) { throw "TopParameters entry '$tp' is not NAME=VALUE" }
            $qsf += "set_parameter -name $($kv[0]) $($kv[1])"
        }
        $qsf | Set-Content -LiteralPath (Join-Path $dir 'blockmap.qsf') -Encoding ascii

        $rowModule = if ($RowLabel) { "$mod$RowLabel" } else { $mod }
        $row = [ordered]@{
            module          = $rowModule
            status          = 'unknown'
            sourceCommit    = $head
            rtlCleanAtHead  = $rtlClean
            sourceListHash  = $sourceListHash
            sourceFileCount = $sources.Count
            tool            = 'Quartus Prime Lite 17.0.2'
            device          = '5CSEBA6U23I7'
            stage           = 'map-only'
        }
        if ($TopParameters) { $row.topParameters = ($TopParameters -join ' ') }
        if ($RowLabel) { $row.variantOf = $mod }

        # ---- THE TIMEOUT, WHICH THE FIRST VERSION ACCEPTED AND IGNORED ----
        #
        # This script took a `-TimeoutSeconds` parameter from run_block_fit.ps1
        # and never used it. The fit lane checks elapsed time BETWEEN its three
        # stages; the map lane has one stage, so there is no "between", and the
        # copy left the parameter as decoration. Measured 2026-08-23:
        # `zhao_surface_sheet` held the lane for 1,096 s on a single map and
        # nothing could have stopped it, so an unbounded module would stall the
        # whole 90-module sweep indefinitely.
        #
        # Start-Process, not `&`, because a direct call blocks with no handle to
        # kill. run_block_fit.ps1's header warns that Start-Process -PassThru
        # returns an EMPTY ExitCode with redirected streams -- true, and avoided
        # here by not depending on the exit code at all: success is judged by
        # whether Analysis & Synthesis wrote a summary, which is what the code
        # below already did.
        $sw = [Diagnostics.Stopwatch]::StartNew()
        Push-Location $dir
        try {
            $p = Start-Process -FilePath $mapExe -ArgumentList 'blockmap' `
                -WorkingDirectory $dir -NoNewWindow -PassThru `
                -RedirectStandardOutput (Join-Path $dir 'quartus_map.log') `
                -RedirectStandardError  (Join-Path $dir 'quartus_map.err.log')
            if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
                try { $p.Kill() } catch { }
                $row.status = 'timeout'
                $row.timeoutSeconds = $TimeoutSeconds
                Write-Warning ("{0}: quartus_map exceeded {1}s and was killed. This is a MEASUREMENT, not a verdict on the block -- run_block_fit.ps1's own header records a module that reported `timeout` at 900 s and then fitted cleanly in 749 s with a larger budget." -f $mod, $TimeoutSeconds)
            }
        } finally { Pop-Location }
        $sw.Stop()
        $row.seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

        $summary = Join-Path $dir 'output_files\blockmap.map.summary'
        if ($row.status -eq 'unknown' -and (Test-Path -LiteralPath $summary)) {
            $t = [IO.File]::ReadAllText($summary)
            $row.status = 'ok'
            # VERIFIED against a real blockmap.map.summary before being trusted
            # (zhao_geom_project, 2026-08-23). The first draft of this function
            # guessed the label 'Embedded Multiplier 9-bit elements' -- a
            # Stratix/Cyclone-IV name -- and the row came back with a null DSP
            # count next to a summary that said `Total DSP Blocks : 33`. A field
            # that silently reads null is exactly the failure the fit lane's
            # merge guard exists for, so the labels below are copied from the
            # tool's own output rather than from memory.
            $row.dspBlocks       = Get-Field $t @('Total DSP Blocks')
            $row.registers       = Get-Field $t @('Total registers')
            $row.blockMemoryBits = Get-Field $t @('Total block memory bits')
            $row.virtualPins     = Get-Field $t @('Total virtual pins')
            $row.pins            = Get-Field $t @('Total pins')
        } elseif ($row.status -eq 'unknown') {
            $row.status = 'no-summary'
        }

        # RAM AND DSP INFERENCE, from the map report rather than the summary.
        # GOTCHAS 3 is the rule this implements: measure that the directive did
        # what you asked. blockMemoryBits > 0 is the only proof that an array
        # became a memory; a passing test is not.
        $maprpt = Join-Path $dir 'output_files\blockmap.map.rpt'
        if (Test-Path -LiteralPath $maprpt) {
            $r = [IO.File]::ReadAllText($maprpt)

            # The ALM figure. NOT the fitter's -- Analysis & Synthesis calls it
            # an ESTIMATE and it is one. Kept because ranking work needs an
            # order of magnitude for 53 modules that have no fit at all, and
            # named `estimated` so it can never be pasted into a fit column.
            $row.estimatedAlms  = Get-TableField $r @('Estimate of Logic utilization (ALMs needed)')
            $row.combAluts      = Get-TableField $r @('Combinational ALUT usage for logic')
            $row.mlabMemoryBits = Get-TableField $r @('Total MLAB memory bits')

            # ---- THE RAM SUMMARY -------------------------------------------
            # Rows are `; Name ; Type ; Mode ; PortADepth ; PortAWidth ; ... ;`
            # and the Mode column carries the shape (Simple Dual Port, ROM, ...).
            #
            # `altshift_taps|...|ALTSYNCRAM` entries are Quartus's OWN pipeline
            # shift-register conversion, not a memory the RTL asked for. They are
            # tagged rather than dropped: a module whose only "inferred RAM" is
            # auto-generated shift taps has inferred NONE of its declared
            # storage, and a count that hid that distinction would read as a
            # pass. zhao_geom_project is exactly this case -- 2,870 block memory
            # bits, every one of them altshift_taps.
            $mems = New-Object 'System.Collections.Generic.List[object]'
            foreach ($m in [regex]::Matches($r, '(?m)^;\s*([^;]{3,200}?)\s*;\s*(AUTO|M10K|MLAB|LCs?)\s*;\s*([A-Za-z0-9 ]+?)\s*;\s*(\d+)\s*;\s*(\d+)\s*;')) {
                $nm = $m.Groups[1].Value.Trim()
                $mems.Add([ordered]@{
                    name      = $nm
                    type      = $m.Groups[2].Value.Trim()
                    mode      = $m.Groups[3].Value.Trim()
                    depth     = [int]$m.Groups[4].Value
                    width     = [int]$m.Groups[5].Value
                    autoShift = ($nm -like '*altshift_taps*')
                })
            }
            $row.inferredMemories      = $mems.ToArray()
            $row.inferredMemoryCount   = $mems.Count
            $row.inferredDesignMemoryCount = @($mems | Where-Object { -not $_.autoShift }).Count

            # ---- THE DSP DECOMPOSITION -------------------------------------
            # `design/budgets/dsp.md`'s corrected rule says operand width and
            # signedness change DSP cost discontinuously. This table is where
            # the tool says so itself: `Two Independent 18x18`, `Sum of two
            # 18x18`, and the signed/unsigned/mixed split.
            $dspStats = [ordered]@{}
            foreach ($lbl in @('Two Independent 18x18', 'Sum of two 18x18', 'Sum of four 18x18',
                               'One 27x27', 'Independent 27x27', 'Total number of DSP blocks',
                               'Fixed Point Signed Multiplier', 'Fixed Point Unsigned Multiplier',
                               'Fixed Point Mixed Sign Multiplier')) {
                $m = [regex]::Match($r, '(?m)^;\s*' + [regex]::Escape($lbl) + '\s*;\s*(\d+)\s*;')
                if ($m.Success) { $dspStats[$lbl] = [int]$m.Groups[1].Value }
            }
            if ($dspStats.Count -gt 0) { $row.dspDecomposition = $dspStats }

            $row.ramConversionWarnings = ([regex]::Matches($r, 'cannot be converted to (a )?RAM|276003')).Count
            $row.errors   = ([regex]::Matches($r, '(?m)^Error ')).Count
            $row.critical = ([regex]::Matches($r, '(?m)^Critical Warning ')).Count
        }

        $results.Add([pscustomobject]$row)
        $dspShow = if ($row.Contains('dspBlocks') -and $null -ne $row.dspBlocks) { $row.dspBlocks } else { '-' }
        $memShow = if ($row.Contains('blockMemoryBits') -and $null -ne $row.blockMemoryBits) { $row.blockMemoryBits } else { '-' }
        Write-Host ("{0,-34} {1,-20} {2,7}s   DSP {3,-5} membits {4}" -f $rowModule, $row.status, $row.seconds, $dspShow, $memShow)
        if (-not $KeepWorkspace) {
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    $dest = Join-Path $RepoRoot 'reports\synthesis\zhao_block_map.json'
    $merged = [ordered]@{}
    if (Test-Path -LiteralPath $dest) {
        try {
            $prior = [IO.File]::ReadAllText($dest) | ConvertFrom-Json
            foreach ($row in $prior.blocks) { $merged[$row.module] = $row }
        } catch { Write-Warning "existing $dest could not be parsed; it will be replaced rather than merged" }
    }
    foreach ($row in $results) {
        $prior = $merged[$row.module]
        if ($row.status -ne 'ok' -and $null -ne $prior -and $prior.status -eq 'ok') {
            $kept = $prior | Select-Object *
            $kept | Add-Member -NotePropertyName 'lastAttemptStatus' -NotePropertyValue $row.status -Force
            $kept | Add-Member -NotePropertyName 'lastAttemptCommit' -NotePropertyValue $row.sourceCommit -Force
            $merged[$row.module] = $kept
            Write-Warning ("{0}: this run ended '{1}'; KEEPING the previous map row" -f $row.module, $row.status)
        } else { $merged[$row.module] = $row }
    }

    $out = [ordered]@{
        schemaVersion    = 1
        characterization = 'map-only-per-block'
        newestRunCommit  = $head
        newestRunClean   = $rtlClean
        tool             = [ordered]@{ name = 'Quartus Prime Lite'; version = '17.0.2' }
        device           = '5CSEBA6U23I7'
        sourceListHash   = $sourceListHash
        blocks           = @($merged.Keys | Sort-Object | ForEach-Object { $merged[$_] })
        limitations      = @(
            'MAP ONLY. No fit, no placement, no routing, no SDC and therefore NO TIMING AT ALL. Any Fmax, slack or WNS attributed to a row in this file is fabricated.',
            'estimatedAluts and registers are Analysis and Synthesis ESTIMATES of a pre-placement netlist, not the fitter ALM count. They are not comparable to reports/synthesis/zhao_block_fit.json alms column.',
            'dspBlocks here is Analysis and Synthesis inference. It matched the fitter exactly on the modules cross-checked (see reports/BUDGET_HEATMAP.md), but the fitter is still the authority; a map DSP count is evidence, not a fit row.',
            'inferredMemories entries whose autoShift flag is true are Quartus own altshift_taps pipeline conversion, NOT storage the RTL declared. A module whose only inferred memory is autoShift has inferred none of its declared storage.',
            'blockMemoryBits > 0 is the ONLY evidence an array inferred as memory. Zero here with a large array in the source is an EXPECTED_RAM_NOT_INFERRED finding, not an absence of data.',
            '5CSEBA6U23I7 is a provisional capacity target, not board truth.',
            'Nothing here is a programmed device.'
        )
    }
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null
    [IO.File]::WriteAllText($dest, (($out | ConvertTo-Json -Depth 8) + "`n"), $Utf8NoBom)
    Write-Host ("WROTE {0} ({1} row(s); {2} this run)" -f $dest, $out.blocks.Count, $results.Count)
} finally {
    if (-not $KeepWorkspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    }
}
