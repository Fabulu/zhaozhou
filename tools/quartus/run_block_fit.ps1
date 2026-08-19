[CmdletBinding()]
param(
    [string[]]$Module,
    # Extra SystemVerilog files to append to the source cone, so blocks that are
    # NOT in the shell cone (raster, texture, geometry, terrain) can be
    # characterized with the same flow rather than a second one.
    [string[]]$ExtraSources,
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    # 3000, not 900. MEASURED 2026-08-19: zhao_measure_tokens reported
    # `timeout` at the 900 s default and then fitted cleanly in 749 s of
    # quartus_fit on the very next run with a larger budget. A "timeout" row in
    # the report reads as "this block does not fit", when all it meant was
    # "we did not wait". Ten rows in the committed report carry that status and
    # every one of them is suspect for the same reason.
    [int]$TimeoutSeconds = 3000,
    [switch]$KeepWorkspace
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Per-block capacity characterization against a provisional device.
#
# WHY THIS EXISTS rather than run_shell_fit.ps1 with a different top: the
# composed zhao_shell_top does not fit this machine. Measured 2026-08-18 on
# Quartus 17.0.2 Lite, quartus_map committed 28.4 GB against 24 GB of RAM and
# thrashed at near-zero CPU until it was killed. A single leaf module
# elaborates cleanly in about 67 seconds, and its ~4.8 GB peak is spent
# PARSING the 22-file source cone before any elaboration begins. So the
# per-block lane is not a lesser substitute picked for speed. It is the
# granularity that fits, and it is the granularity design/blocks.yml wants
# numbers at anyway.
#
# LIMITATIONS, which travel with every number this produces:
#   - 5CSEBA6U23I7 is a PROVISIONAL target, not board truth.
#   - All I/O is virtual. No package pins, no board I/O delays, no PLLs.
#   - A per-block fit says nothing about the composed machine's routing or
#     timing closure.
#   - Nothing here is a programmed device. This is not hardware proof.

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$SrcQsf = Join-Path $RepoRoot 'fpga\quartus\shell_fit\zhao_shell_fit.qsf'
$SrcSdc = Join-Path $RepoRoot 'fpga\quartus\shell_fit\zhao_shell_fit.sdc'

foreach ($exe in @('quartus_map.exe', 'quartus_fit.exe')) {
    $full = Join-Path $QuartusBin $exe
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
        throw "Required Quartus executable not found: $full"
    }
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve HEAD.' }
$dirtyRtl = (& git -C $RepoRoot status --porcelain -- fpga/rtl) -join ''
$rtlClean = [string]::IsNullOrWhiteSpace($dirtyRtl)

if (-not $Module -or $Module.Count -eq 0) {
    # The shell cone's entities, leaves first so a failure surfaces cheaply.
    $Module = @(
        'zhao_video_mode', 'zhao_audio_fifo', 'zhao_debug_crc', 'zhao_input_rumble',
        'zhao_input_snapshot', 'zhao_debug_counters', 'zhao_mem_guard', 'zhao_hps_bridge',
        'zhao_scanout_linebuf', 'zhao_scanout_serializer', 'zhao_scanout_fetch',
        'zhao_video_framectl', 'zhao_video_scaler', 'zhao_video_scanout',
        'zhao_cmd_dma', 'zhao_cmd_scheduler', 'zhao_sdram_ctrl', 'zhao_vram_arbiter'
    )
}

$Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-block-fit-{0}" -f $PID)
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

try {
    foreach ($mod in $Module) {
        $dir = Join-Path $Workspace $mod
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Copy-Item $SrcSdc (Join-Path $dir 'blockfit.sdc')
        'PROJECT_REVISION = "blockfit"' | Set-Content -LiteralPath (Join-Path $dir 'blockfit.qpf') -Encoding ascii

        # The same ordered source cone with absolute paths; only the top moves.
        $rtlAbs = (Join-Path $RepoRoot 'fpga\rtl').Replace('\', '/')
        $qsf = Get-Content -LiteralPath $SrcQsf
        $qsf = $qsf -replace '^set_global_assignment -name TOP_LEVEL_ENTITY.*', "set_global_assignment -name TOP_LEVEL_ENTITY $mod"
        $qsf = $qsf -replace '^set_global_assignment -name SDC_FILE.*', 'set_global_assignment -name SDC_FILE blockfit.sdc'
        $qsf = $qsf -replace '\.\./\.\./rtl/', "$rtlAbs/"
        if ($ExtraSources) {
            foreach ($extra in $ExtraSources) {
                $abs = (Resolve-Path (Join-Path $RepoRoot $extra)).Path.Replace([char]92, [char]47)
                $qsf += "set_global_assignment -name SYSTEMVERILOG_FILE $abs"
            }
        }
        $qsf | Set-Content -LiteralPath (Join-Path $dir 'blockfit.qsf') -Encoding ascii

        $row = [ordered]@{ module = $mod; status = 'unknown' }
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $ok = $true

        # Call the executables directly rather than through Start-Process.
        # Start-Process -PassThru combined with -NoNewWindow and redirected
        # streams returns a Process whose ExitCode reads back EMPTY, which
        # made every stage look like a failure while Quartus was in fact
        # reporting "0 errors". $LASTEXITCODE from a direct call is
        # authoritative. Peak memory per block is given up with it; the
        # figure that mattered (the composed shell at 28.4 GB) is already
        # measured and recorded in this file's header.
        Push-Location $dir
        try {
            foreach ($exe in @('quartus_map.exe', 'quartus_fit.exe')) {
                & (Join-Path $QuartusBin $exe) 'blockfit' *> (Join-Path $dir "$exe.log")
                if ($LASTEXITCODE -ne 0) { $row.status = "failed:$exe"; $ok = $false; break }
                if ($sw.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
                    $row.status = 'timeout'; $ok = $false; break
                }
            }
        } finally {
            Pop-Location
        }
        $sw.Stop()
        $row.seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

        $summary = Join-Path $dir 'output_files\blockfit.fit.summary'
        if ($ok -and (Test-Path -LiteralPath $summary)) {
            $t = [IO.File]::ReadAllText($summary)
            $row.status = 'ok'
            $row.alms = Get-Field $t @('Logic utilization (in ALMs)', 'Logic utilization')
            $row.registers = Get-Field $t @('Total registers', 'Dedicated logic registers')
            $row.blockMemoryBits = Get-Field $t @('Total block memory bits', 'Total memory bits')
            $row.ramBlocks = Get-Field $t @('Total RAM Blocks')
            $row.dspBlocks = Get-Field $t @('Total DSP Blocks')
            $row.virtualPins = Get-Field $t @('Total virtual pins')
        } elseif ($row.status -eq 'unknown') {
            $row.status = 'no-summary'
        }

        $results.Add([pscustomobject]$row)
        # StrictMode: a failed or timed-out fit never gains the resource keys.
        $almShow = if ($row.Contains('alms') -and $null -ne $row.alms) { $row.alms } else { '-' }
        Write-Host ("{0,-28} {1,-16} {2,7}s   ALM {3}" -f $mod, $row.status, $row.seconds, $almShow)
        if (-not $KeepWorkspace) {
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    $out = [ordered]@{
        schemaVersion    = 1
        characterization = 'provisional-per-block-fit'
        sourceCommit     = $head
        rtlCleanAtHead   = $rtlClean
        tool             = [ordered]@{ name = 'Quartus Prime Lite'; version = '17.0.2' }
        device           = '5CSEBA6U23I7'
        blocks           = $results.ToArray()
        limitations      = @(
            '5CSEBA6U23I7 is a provisional capacity target, not board truth.',
            'All I/O is virtual: no package pins, no board I/O delays, no PLLs, no physical clocks.',
            'A per-block fit does not characterize the composed machine routing or timing closure.',
            'The composed zhao_shell_top does not fit this machine: 28.4 GB committed against 24 GB of RAM on 2026-08-18.',
            'Nothing here is a programmed device. This is not hardware proof.'
        )
    }
    $dest = Join-Path $RepoRoot 'reports\synthesis\zhao_block_fit.json'
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null

    # MERGE, never replace. This script characterizes whichever modules it was
    # ASKED for, and a targeted run (one module, or a couple of new ones) used
    # to write a report containing only those. Measured 2026-08-19: a two-module
    # run turned a committed 21-block report into a 2-block one, silently
    # destroying nineteen blocks' worth of fitter evidence. Recoverable from git
    # that time; the point is that nothing said it had happened.
    #
    # So: load whatever is already on disk, replace only the rows this run
    # actually produced, and keep the rest. A module re-run overwrites its own
    # row (the newest measurement wins, which is what re-running means) and
    # touches nothing else. Rows are then sorted by module so the file's diff
    # shows measurements changing rather than lines moving around.
    $merged = [ordered]@{}
    if (Test-Path -LiteralPath $dest) {
        try {
            $prior = [IO.File]::ReadAllText($dest) | ConvertFrom-Json
            foreach ($row in $prior.blocks) { $merged[$row.module] = $row }
        } catch {
            Write-Warning "existing $dest could not be parsed; it will be replaced rather than merged"
        }
    }
    foreach ($row in $results) { $merged[$row.module] = $row }
    $out.blocks = @($merged.Keys | Sort-Object | ForEach-Object { $merged[$_] })

    [IO.File]::WriteAllText($dest, (($out | ConvertTo-Json -Depth 8) + "`n"), $Utf8NoBom)
    Write-Host ("WROTE {0} ({1} block(s); {2} measured this run)" -f $dest, $out.blocks.Count, $results.Count)
} finally {
    if (-not $KeepWorkspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    }
}
