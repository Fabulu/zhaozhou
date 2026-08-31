[CmdletBinding()]
param(
    [switch]$ParityOnly,
    [switch]$KeepWorkspace,
    # Peak MEMORY is what stops this flow, not time. Measured 2026-08-18:
    # quartus_map committed 28.4 GB and thrashed against 24 GB of RAM.
    # NUM_PARALLEL_PROCESSORS is the biggest lever there, because each worker
    # holds its own working set of the netlist, so the committed footprint
    # scales with it. The project QSF asks for 4. Passing a lower number here
    # overrides it in the STAGED copy only, so the committed project keeps its
    # value and per-block characterization is unaffected.
    # Results are identical either way: this changes how the work is divided,
    # not what is computed.
    [int]$Processors = 0,
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [string]$ReportRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRel = 'fpga/quartus/shell_fit'
$ProjectDir = Join-Path $RepoRoot ($ProjectRel -replace '/', '\')
$QsfPath = Join-Path $ProjectDir 'zhao_shell_fit.qsf'
$CmakePath = Join-Path $RepoRoot 'tests\CMakeLists.txt'

function Get-NormalizedRelativePath([string]$AbsolutePath, [string]$Root) {
    $absolute = [IO.Path]::GetFullPath($AbsolutePath).TrimEnd('\')
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (-not $absolute.StartsWith($rootFull + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path '$absolute' escapes repository root '$rootFull'."
    }
    return $absolute.Substring($rootFull.Length + 1).Replace('\', '/')
}

function Get-CmakeShellSources {
    $text = [IO.File]::ReadAllText($CmakePath)
    $block = [regex]::Match($text, '(?ms)set\(ZHAO_SHELL_RTL\s+(.*?)\)')
    if (-not $block.Success) {
        throw 'Could not locate set(ZHAO_SHELL_RTL ...) in tests/CMakeLists.txt.'
    }

    $sources = New-Object 'System.Collections.Generic.List[string]'
    foreach ($token in [regex]::Matches($block.Groups[1].Value, '\$\{ZHAO_ABI_PKG\}|\$\{CMAKE_SOURCE_DIR\}/[^\s\)]+')) {
        if ($token.Value -eq '${ZHAO_ABI_PKG}') {
            $sources.Add('fpga/rtl/generated/zhao_abi_pkg.sv')
        } else {
            $sources.Add($token.Value.Substring('${CMAKE_SOURCE_DIR}/'.Length))
        }
    }
    return $sources.ToArray()
}

function Get-QsfShellSources {
    $sources = New-Object 'System.Collections.Generic.List[string]'
    foreach ($line in [IO.File]::ReadAllLines($QsfPath)) {
        $match = [regex]::Match($line, '^\s*set_global_assignment\s+-name\s+SYSTEMVERILOG_FILE\s+(?:"([^"]+)"|(\S+))\s*$')
        if ($match.Success) {
            $value = if ($match.Groups[1].Success) { $match.Groups[1].Value } else { $match.Groups[2].Value }
            $absolute = [IO.Path]::GetFullPath((Join-Path $ProjectDir ($value -replace '/', '\')))
            $sources.Add((Get-NormalizedRelativePath $absolute $RepoRoot))
        }
    }
    return $sources.ToArray()
}

function Assert-SourceParity {
    $cmake = @(Get-CmakeShellSources)
    $qsf = @(Get-QsfShellSources)
    if ($cmake.Count -ne $qsf.Count) {
        throw "Shell source parity failed: CMake has $($cmake.Count) sources; QSF has $($qsf.Count)."
    }
    for ($i = 0; $i -lt $cmake.Count; ++$i) {
        if ($cmake[$i] -cne $qsf[$i]) {
            throw "Shell source parity failed at index $i`: CMake='$($cmake[$i])', QSF='$($qsf[$i])'."
        }
    }
    Write-Host "PASS source parity: $($cmake.Count) ordered shell sources match tests/CMakeLists.txt."
    return $cmake
}


# ---------------------------------------------------------------------------
# VIRTUAL-PIN PARITY, added 2026-08-31 after five attempts to reach the fitter.
#
# Every top-level port of zhao_shell_top must carry a VIRTUAL_PIN assignment or
# Quartus makes it a real pad. The device has 145 user I/O and the shell has
# thousands of port bits, so a single missed port fails the fit -- fifteen
# minutes in, with a NUMBER and no name:
#
#   Error (179000): Design requires 156 user-specified I/O pins
#
# Three attempts were spent doing arithmetic on that 156. It was eight ports
# declared TWO TO A LINE:
#
#   input logic signed [22:0] render_kx0_i, render_ky0_i,
#
# where a first-identifier-per-line scan virtualises the x half and leaves the y
# half a pad. 3x23 + 3x21 + 2x12 = 156, exactly.
#
# So this check parses the ports the way the language actually declares them --
# every identifier on the line, and unpacked arrays element by element -- and
# refuses BEFORE Quartus starts, naming what is missing. See
# fpga/quartus/FIT-PROJECT-STALENESS.md.
# ---------------------------------------------------------------------------
function Get-ShellPortNames {
    $svPath = Join-Path $RepoRoot 'fpga\rtl\common\zhao_shell_top.sv'
    $lines = [IO.File]::ReadAllLines($svPath)
    $keywords = @('logic', 'wire', 'reg', 'signed', 'unsigned', 'var', 'input', 'output', 'inout')
    $names = New-Object 'System.Collections.Generic.List[string]'
    $inPorts = $false
    foreach ($raw in $lines) {
        $line = ($raw -split '//')[0]
        if ($line -match '^\s*module\s+zhao_shell_top') { $inPorts = $true; continue }
        if ($inPorts -and $line -match '^\s*\)\s*;') { break }
        if (-not $inPorts) { continue }
        if ($line -notmatch '^\s*(input|output|inout)\b') { continue }

        # An unpacked array port ends with a range AFTER the identifier.
        $unpacked = $null
        if ($line -match '\b([A-Za-z_][A-Za-z_0-9]*)\s*\[\s*(\d+)\s*:\s*(\d+)\s*\]\s*,?\s*$') {
            $cand = $Matches[1]
            if ($keywords -notcontains $cand) {
                $unpacked = @{ name = $cand; a = [int]$Matches[2]; b = [int]$Matches[3] }
            }
        }

        $rest = $line -replace '^\s*(input|output|inout)\b', ''
        $rest = $rest -replace '\b(var|logic|wire|reg|signed|unsigned)\b', ' '
        $rest = $rest -replace '\[[^\]]*\]', ' '
        foreach ($tok in ($rest -split '[,\s]+')) {
            $t = $tok.Trim()
            if ([string]::IsNullOrWhiteSpace($t)) { continue }
            if ($keywords -contains $t) { continue }
            if ($t -notmatch '^[A-Za-z_][A-Za-z_0-9]*$') { continue }
            $names.Add($t) | Out-Null
        }
        if ($null -ne $unpacked) {
            $lo = [Math]::Min($unpacked.a, $unpacked.b)
            $hi = [Math]::Max($unpacked.a, $unpacked.b)
            for ($i = $lo; $i -le $hi; ++$i) { $names.Add("$($unpacked.name)[$i]") | Out-Null }
        }
    }
    return ($names | Select-Object -Unique)
}

function Assert-VirtualPinParity {
    $ports = @(Get-ShellPortNames)
    if ($ports.Count -lt 50) {
        throw "Virtual-pin parity could not parse zhao_shell_top's ports (found $($ports.Count)); refusing rather than passing a check that did nothing."
    }
    $qsfText = [IO.File]::ReadAllText($QsfPath)
    $assigned = @{}
    foreach ($m in [regex]::Matches($qsfText, 'VIRTUAL_PIN\s+ON\s+-to\s+(\S+)')) {
        $assigned[$m.Groups[1].Value] = $true
    }
    $missing = @($ports | Where-Object { -not $assigned.ContainsKey($_) })
    if ($missing.Count -gt 0) {
        Write-Host "MISSING VIRTUAL_PIN for $($missing.Count) port(s):" -ForegroundColor Red
        foreach ($m in $missing) { Write-Host "    $m" -ForegroundColor Red }
        throw "Virtual-pin parity failed. Every top-level port needs a VIRTUAL_PIN or the fitter runs out of I/O. See fpga/quartus/FIT-PROJECT-STALENESS.md."
    }
    Write-Host "PASS virtual-pin parity: $($ports.Count) shell ports all virtualised."
}

$SourceCone = @(Assert-SourceParity)
Assert-VirtualPinParity
if ($ParityOnly) {
    exit 0
}

$QuartusSh = Join-Path $QuartusBin 'quartus_sh.exe'
$QuartusMap = Join-Path $QuartusBin 'quartus_map.exe'
$QuartusFit = Join-Path $QuartusBin 'quartus_fit.exe'
$QuartusSta = Join-Path $QuartusBin 'quartus_sta.exe'
foreach ($exe in @($QuartusSh, $QuartusMap, $QuartusFit, $QuartusSta)) {
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "Required Quartus executable not found: $exe"
    }
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not resolve repository HEAD.'
}

# A real run is always made from committed HEAD. This prevents any pre-existing
# dirty ABI/environment record in the canonical worktree from entering evidence.
$requiredFlowFiles = @(
    '.gitignore',
    'fpga/quartus/shell_fit/zhao_shell_fit.qpf',
    'fpga/quartus/shell_fit/zhao_shell_fit.qsf',
    'fpga/quartus/shell_fit/zhao_shell_fit.sdc',
    'fpga/quartus/shell_fit/report.tcl',
    'tools/quartus/run_shell_fit.ps1'
)
foreach ($path in $requiredFlowFiles) {
    & git -C $RepoRoot cat-file -e "HEAD`:$path" 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Real characterization requires committed flow file at HEAD: $path (use -ParityOnly before the flow commit)."
    }
}

$Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-shell-fit-{0}-{1}" -f $PID, [guid]::NewGuid().ToString('N'))
$Archive = "$Workspace.zip"
$Snapshot = Join-Path $Workspace 'source'
New-Item -ItemType Directory -Path $Snapshot -Force | Out-Null

try {
    & git -C $RepoRoot archive --format=zip --output=$Archive HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git archive HEAD failed.' }
    # ZipFile over Expand-Archive: the cmdlet pipes every entry through the
    # PowerShell object model and takes tens of minutes on a tree this size,
    # which is longer than the synthesis it exists to feed.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Snapshot)
    Remove-Item -LiteralPath $Archive -Force

    $SnapshotScript = Join-Path $Snapshot 'tools\quartus\run_shell_fit.ps1'
    $SnapshotProject = Join-Path $Snapshot 'fpga\quartus\shell_fit'
    $SnapshotCmake = Join-Path $Snapshot 'tests\CMakeLists.txt'
    if (-not (Test-Path -LiteralPath $SnapshotScript) -or -not (Test-Path -LiteralPath $SnapshotCmake)) {
        throw 'Clean HEAD archive is missing characterization inputs.'
    }

    # Repeat parity against the clean snapshot, not merely the canonical tree.
    $snapshotCmakeText = [IO.File]::ReadAllText($SnapshotCmake)
    $snapshotBlock = [regex]::Match($snapshotCmakeText, '(?ms)set\(ZHAO_SHELL_RTL\s+(.*?)\)')
    $snapshotExpected = New-Object 'System.Collections.Generic.List[string]'
    foreach ($token in [regex]::Matches($snapshotBlock.Groups[1].Value, '\$\{ZHAO_ABI_PKG\}|\$\{CMAKE_SOURCE_DIR\}/[^\s\)]+')) {
        if ($token.Value -eq '${ZHAO_ABI_PKG}') { $snapshotExpected.Add('fpga/rtl/generated/zhao_abi_pkg.sv') }
        else { $snapshotExpected.Add($token.Value.Substring('${CMAKE_SOURCE_DIR}/'.Length)) }
    }
    if (($snapshotExpected -join "`n") -cne ($SourceCone -join "`n")) {
        throw 'Clean HEAD archive source cone differs from the parity-checked canonical source cone.'
    }

    if ($Processors -gt 0) {
        $stagedQsf = Join-Path $SnapshotProject 'zhao_shell_fit.qsf'
        $qsfText = [IO.File]::ReadAllText($stagedQsf)
        # A later assignment of the same name wins in a QSF, so appending is
        # enough and the original line stays visible in the staged file.
        $qsfText += "`n# Overridden by run_shell_fit.ps1 -Processors (peak-memory control).`n"
        $qsfText += "set_global_assignment -name NUM_PARALLEL_PROCESSORS $Processors`n"
        [IO.File]::WriteAllText($stagedQsf, $qsfText, $Utf8NoBom)
        Write-Host "staged override: NUM_PARALLEL_PROCESSORS $Processors"
    }

    $LogDir = Join-Path $Workspace 'logs'
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
    $stageStatus = [ordered]@{}

    function Invoke-QuartusStage([string]$Name, [string]$Exe, [string[]]$Arguments) {
        Write-Host "RUN $Name`: $([IO.Path]::GetFileName($Exe)) $($Arguments -join ' ')"
        Push-Location $SnapshotProject
        try {
            $lines = @(& $Exe @Arguments 2>&1 | ForEach-Object { $_.ToString() })
            $exitCode = $LASTEXITCODE
        } finally {
            Pop-Location
        }
        $logPath = Join-Path $LogDir "$Name.log"
        [IO.File]::WriteAllText($logPath, (($lines -join "`n") + "`n"), $Utf8NoBom)
        foreach ($line in $lines) { Write-Host $line }
        if ($exitCode -ne 0) {
            $stageStatus[$Name] = 'failed'
            throw "Quartus stage '$Name' failed with exit code $exitCode; log: $logPath"
        }
        $stageStatus[$Name] = 'success'
    }

    Invoke-QuartusStage 'analysisAndElaboration' $QuartusMap @('zhao_shell_fit', '--analysis_and_elaboration')
    Invoke-QuartusStage 'synthesis' $QuartusMap @('zhao_shell_fit')
    Invoke-QuartusStage 'fitter' $QuartusFit @('zhao_shell_fit')
    Invoke-QuartusStage 'timequest' $QuartusSta @('zhao_shell_fit', '--report_script=report.tcl')

    $versionLines = @(& $QuartusSh --version 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw 'quartus_sh --version failed.' }
    $version = ($versionLines | Where-Object { $_ -match 'Version|Quartus' } | Select-Object -First 1).Trim()

    $outputDir = Join-Path $SnapshotProject 'output_files'
    $fitSummary = Join-Path $outputDir 'zhao_shell_fit.fit.summary'
    $metricPath = Join-Path $outputDir 'characterization\timing_metrics.tsv'
    $ucpPath = Join-Path $outputDir 'characterization\unconstrained_paths.rpt'
    foreach ($required in @($fitSummary, $metricPath, $ucpPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Expected Quartus report missing: $required"
        }
    }

    function Get-ReportField([string]$Text, [string[]]$Labels) {
        foreach ($label in $Labels) {
            $match = [regex]::Match($Text, "(?im)^\s*" + [regex]::Escape($label) + "\s*:\s*([^\r\n]+)")
            if ($match.Success) { return $match.Groups[1].Value.Trim() }
        }
        return $null
    }

    function Get-LeadingInteger([AllowNull()][string]$Value) {
        if ($null -eq $Value) { return $null }
        $match = [regex]::Match($Value, '-?[0-9][0-9,]*')
        if (-not $match.Success) { return $null }
        return [int64]::Parse($match.Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture)
    }

    $fitText = [IO.File]::ReadAllText($fitSummary)
    $resources = [ordered]@{
        logicUtilizationAlms = Get-LeadingInteger (Get-ReportField $fitText @('Logic utilization (in ALMs)', 'Logic utilization'))
        combinationalFunctions = Get-LeadingInteger (Get-ReportField $fitText @('Total combinational functions', 'Combinational ALUT usage'))
        registers = Get-LeadingInteger (Get-ReportField $fitText @('Total registers', 'Dedicated logic registers'))
        blockMemoryBits = Get-LeadingInteger (Get-ReportField $fitText @('Total block memory bits', 'Total memory bits'))
        ramBlocks = Get-LeadingInteger (Get-ReportField $fitText @('Total RAM Blocks', 'Total block memory implementation bits'))
        dspBlocks = Get-LeadingInteger (Get-ReportField $fitText @('Total DSP Blocks', 'Total DSP block 18-bit elements'))
        pins = Get-LeadingInteger (Get-ReportField $fitText @('Total pins'))
        virtualPins = Get-LeadingInteger (Get-ReportField $fitText @('Total virtual pins'))
    }

    $allLogLines = New-Object 'System.Collections.Generic.List[string]'
    foreach ($log in Get-ChildItem -LiteralPath $LogDir -Filter '*.log' | Sort-Object Name) {
        foreach ($line in [IO.File]::ReadAllLines($log.FullName)) { $allLogLines.Add($line) }
    }
    $criticalWarnings = @($allLogLines | Where-Object { $_ -match '^Critical Warning' } | ForEach-Object {
        $normalized = $_ -replace [regex]::Escape($Snapshot), '<clean-head>'
        $normalized = $normalized -replace '\\', '/'
        $normalized.Trim()
    } | Sort-Object -Unique)

    $metricRows = @(Import-Csv -LiteralPath $metricPath -Delimiter "`t")
    $clocks = [ordered]@{}
    foreach ($row in $metricRows | Where-Object { $_.record -eq 'clock' } | Sort-Object name) {
        $clocks[$row.name] = [double]::Parse($row.value, [Globalization.CultureInfo]::InvariantCulture)
    }
    $analyses = [ordered]@{}
    foreach ($name in @('setup', 'hold', 'recovery', 'removal')) {
        $row = $metricRows | Where-Object { $_.record -eq 'analysis' -and $_.name -eq $name } | Select-Object -First 1
        if ($null -eq $row) { throw "Timing metric missing for analysis '$name'." }
        $slack = if ($row.value -eq 'NA') { $null } else { [double]::Parse($row.value, [Globalization.CultureInfo]::InvariantCulture) }
        $analyses[$name] = [ordered]@{
            worstSlackNs = $slack
            failingEndpointCount = [int64]::Parse($row.count, [Globalization.CultureInfo]::InvariantCulture)
        }
    }

    $ucpText = [IO.File]::ReadAllText($ucpPath)
    $ucpRows = New-Object 'System.Collections.Generic.List[object]'
    foreach ($line in $ucpText -split '\r?\n') {
        $match = [regex]::Match($line, '^\s*([^;|]+?)\s*[;|]\s*([0-9][0-9,]*)\s*$')
        if ($match.Success -and $match.Groups[1].Value.Trim() -notmatch '^[-=]+$') {
            $ucpRows.Add([ordered]@{
                category = $match.Groups[1].Value.Trim()
                count = [int64]::Parse($match.Groups[2].Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture)
            })
        }
    }

    $limitations = @(
        '5CSEBA6U23I7 is a provisional capacity/timing target, not frozen board truth.',
        'All harness I/O is virtual; no package pins, board I/O delays, PLLs, or physical clocks are claimed.',
        'gpu_clk and vid_clk remain timing-related so the known phase-dependent displayed-byte crossing is not waived.',
        'Only audio_clk is grouped asynchronous against GPU/video, matching the dual-clock FIFO boundary.',
        'Harness data/reset paths have no invented I/O delays or reset exceptions; unconstrained and recovery/removal limitations remain reportable.',
        'The result does not characterize a physical SDRAM interface, framework integration, or fabricated hardware.'
    )

    $synthesisJson = [ordered]@{
        schemaVersion = 1
        characterization = 'provisional-shell-fit'
        sourceCommit = $head
        sourceConeParity = $true
        sourceFileCount = $SourceCone.Count
        tool = [ordered]@{ name = 'Quartus Prime Lite'; version = $version }
        design = [ordered]@{ top = 'zhao_shell_top'; device = '5CSEBA6U23I7'; frameworkTopModified = $false }
        stages = $stageStatus
        resources = $resources
        criticalWarningCount = $criticalWarnings.Count
        criticalWarnings = $criticalWarnings
        limitations = $limitations
    }

    $timingJson = [ordered]@{
        schemaVersion = 1
        characterization = 'provisional-shell-fit'
        sourceCommit = $head
        tool = [ordered]@{ name = 'Quartus Prime Lite'; version = $version }
        design = [ordered]@{ top = 'zhao_shell_top'; device = '5CSEBA6U23I7' }
        targetClocksNs = $clocks
        analyses = $analyses
        unconstrainedPathSummary = $ucpRows.ToArray()
        timingPassed = (($analyses.setup.failingEndpointCount -eq 0) -and ($analyses.hold.failingEndpointCount -eq 0) -and ($analyses.recovery.failingEndpointCount -eq 0) -and ($analyses.removal.failingEndpointCount -eq 0))
        knownCdc = 'The GPU-to-video displayed-byte serializer crossing is intentionally not false-pathed; its result remains part of setup/hold characterization.'
        criticalWarningCount = $criticalWarnings.Count
        criticalWarnings = $criticalWarnings
        limitations = $limitations
    }

    if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
        $ReportRoot = Join-Path $RepoRoot 'reports'
    } elseif (-not [IO.Path]::IsPathRooted($ReportRoot)) {
        $ReportRoot = Join-Path $RepoRoot $ReportRoot
    }
    $synthOut = Join-Path $ReportRoot 'synthesis\zhao_shell_fit.json'
    $timingOut = Join-Path $ReportRoot 'timing\zhao_shell_fit.json'
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($synthOut)) -Force | Out-Null
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($timingOut)) -Force | Out-Null
    [IO.File]::WriteAllText($synthOut, (($synthesisJson | ConvertTo-Json -Depth 12) + "`n"), $Utf8NoBom)
    [IO.File]::WriteAllText($timingOut, (($timingJson | ConvertTo-Json -Depth 12) + "`n"), $Utf8NoBom)

    # ---- PRESERVE THE PATH DETAIL, NOT JUST THE SUMMARY --------------------
    # report.tcl already writes setup/hold/recovery/removal path reports at
    # `-detail full_path`, and until now every one of them died with the
    # workspace. The JSON kept "hold worst slack -0.952 ns, 1 failing endpoint"
    # and threw away the only artifact that says WHICH endpoint.
    #
    # MEASURED 2026-08-24: the composed shell fit reported exactly that hold
    # violation, and the path could not be identified afterwards from anything
    # that survived. The only leftovers on disk were workspaces from 2026-08-22
    # -- a different commit, so reading them would have been the familiar error
    # of taking an artifact for something other than what it is.
    #
    # A failing number you cannot act on is barely better than no number. This
    # does not change what is measured; it changes whether it can be worked on.
    $charSrc = Join-Path $outputDir 'characterization'
    if (Test-Path -LiteralPath $charSrc) {
        $charDst = Join-Path $ReportRoot 'characterization'
        New-Item -ItemType Directory -Path $charDst -Force | Out-Null
        Copy-Item -Path (Join-Path $charSrc '*.rpt') -Destination $charDst -Force -ErrorAction SilentlyContinue
        Copy-Item -Path (Join-Path $charSrc '*.tsv') -Destination $charDst -Force -ErrorAction SilentlyContinue
        $staRpt = Join-Path $outputDir 'zhao_shell_fit.sta.rpt'
        if (Test-Path -LiteralPath $staRpt) { Copy-Item -LiteralPath $staRpt -Destination $charDst -Force }
        Write-Host ("WROTE {0} ({1} file(s))" -f $charDst, (Get-ChildItem $charDst -File).Count)
    }

    Write-Host "PASS analysis/elaboration, synthesis, fitter, and TimeQuest."
    Write-Host "WROTE $synthOut"
    Write-Host "WROTE $timingOut"
    Write-Host "CLEAN_HEAD $head"
    if ($KeepWorkspace) { Write-Host "WORKSPACE $Workspace" }
} finally {
    if (-not $KeepWorkspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force
    }
}
