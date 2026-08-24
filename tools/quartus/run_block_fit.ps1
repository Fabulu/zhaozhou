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
    # Top-level parameter overrides, NAME=VALUE, emitted into the generated
    # QSF as `set_parameter`. A parameterised block's resource frontier is only
    # measurable if each setting can be fitted, and editing the RTL's default
    # between fits would label every row with a commit that no longer describes
    # what was measured.
    #
    # VERIFY THAT IT TOOK. QUARTUS_GOTCHAS 3: this tool accepts directives and
    # silently ignores them, and the only symptom is a number that does not
    # move. Two settings that differ in multiplier count MUST produce different
    # DSP counts; identical rows mean the parameter was ignored, not that the
    # parameter does not matter.
    [string[]]$TopParameters,
    # Suffix for the JSON row's `module` key, so parameter points do not
    # overwrite each other in a file that merges by module name.
    [string]$RowLabel = '',
    [switch]$KeepWorkspace
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Per-block capacity characterization against a provisional device.
#
# WHY THIS EXISTS rather than run_shell_fit.ps1 with a different top: the
# composed zhao_shell_top does not fit this machine. Measured 2026-08-18 on
# Quartus 17.0.2 Lite, quartus_map committed 28.4 GB against 24 GB of RAM and
# thrashed at near-zero CPU until it was killed. So the per-block lane is not a
# lesser substitute picked for speed. It is the granularity that fits, and it is
# the granularity design/blocks.yml wants numbers at anyway.
#
# CORRECTION 2026-08-20. This comment used to claim that a leaf module's
# "~4.8 GB peak is spent PARSING the 22-file source cone before any elaboration
# begins". That is FALSE, and it was believed long enough to shape decisions.
# Measured directly, with a one-flop module as top so nothing real elaborates:
#
#   trivial top, no other sources ............ 0.24 GB, 27 s
#   trivial top + both shared packages ....... 0.24 GB, 22 s
#   trivial top + ALL 22 cone files .......... 0.24 GB, 23 s
#
# Parsing the entire cone is free. The memory goes into ELABORATING the real
# top, and it is superlinear: every one of these blocks elaborates on its own,
# while zhao_shell_top -- sixteen ordinary instances, no generate blocks, no
# large arrays -- exceeded ten minutes and 16 GB just to elaborate.
#
# So the lever is NOT the source cone, the packages, or the virtual-pin
# assignments (all three were tried and measured). It is the Quartus 17.0.2
# elaborator itself, and the thing worth trying is a newer Quartus.
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

foreach ($exe in @('quartus_map.exe', 'quartus_fit.exe', 'quartus_sta.exe')) {
    $full = Join-Path $QuartusBin $exe
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
        throw "Required Quartus executable not found: $full"
    }
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve HEAD.' }
# -c core.autocrlf=true, for the reason run_composed_fit.ps1 already documents
# and this script did not inherit: PowerShell resolves `git` to whichever binary
# is first on PATH, and c:\devkitPro\msys2\usr\bin\git.exe carries no
# core.autocrlf. A status taken through it calls EVERY CRLF worktree file
# modified -- 29 RTL files here, 279 insertions against 279 deletions on a
# 279-line file, which is every line and therefore pure line-ending churn.
#
# THE COST OF NOT HAVING THIS: all 42 rows in zhao_block_fit.json carried
# rtlCleanAtHead:false. The flag had NEVER once been true, so a field meant to
# say whether a measurement can be trusted against its commit was answering the
# same way regardless -- which is indistinguishable from not having it. Forcing
# the setting makes the check answer the same way whichever git is first.
$dirtyRtl = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain -- fpga/rtl) -join ''
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

# THE WORKSPACE NAME CARRIES A PER-INVOCATION UNIQUIFIER, not just $PID.
#
# MEASURED 2026-08-23, SURFACE.STAMP frontier run. Three fits of the SAME module
# at three parameter settings were issued from ONE PowerShell process:
#
#   run_block_fit.ps1 -Module zhao_surface_stamp -KeepWorkspace
#   run_block_fit.ps1 -Module zhao_surface_stamp -TopParameters SQ_RADIX=2 ... -KeepWorkspace
#   run_block_fit.ps1 -Module zhao_surface_stamp -TopParameters SQ_RADIX=4 ... -KeepWorkspace
#
# $PID is the same for all three, and the per-module subdirectory is named after
# the module -- so all three landed in ONE directory and each overwrote the
# previous one's quartus_map/fit/sta logs and output_files. `-KeepWorkspace`
# exists precisely so the `Info (332111): 10.000 clk` constraint evidence can be
# captured before the harness deletes it, and it kept only the LAST fit's. The
# JSON rows were unaffected (they are written per run), so the loss was silent:
# three good measurements, one surviving set of evidence, and nothing saying so.
#
# The ticks-based suffix makes each invocation its own directory. It does not
# change what is measured; it changes whether the measurement can be shown.
$Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-block-fit-{0}-{1}" -f $PID, [DateTime]::UtcNow.Ticks)
New-Item -ItemType Directory -Path $Workspace -Force | Out-Null
$results = New-Object 'System.Collections.Generic.List[object]'

# The fitter writes resources as "USED / AVAILABLE ( PCT % )". Get-Field takes
# only the numerator, which is how this report ended up able to say a block is
# 1,422 ALMs without being able to say whether the design fits anything. This
# takes the DENOMINATOR from the same line, so the device's capacity travels
# with the measurement instead of being looked up separately and misremembered.
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

function Get-Capacity([string]$Text, [string[]]$Labels) {
    foreach ($label in $Labels) {
        $m = [regex]::Match($Text, "(?im)^\s*" + [regex]::Escape($label) + "\s*:\s*([^
]+)")
        if ($m.Success) {
            $n = [regex]::Match($m.Groups[1].Value, '[0-9][0-9,]*\s*/\s*([0-9][0-9,]*)')
            if ($n.Success) {
                return [int64]::Parse($n.Groups[1].Value.Replace(',', ''), [Globalization.CultureInfo]::InvariantCulture)
            }
        }
    }
    return $null
}

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
        # ---- THE BLOCK SDC, WRITTEN RATHER THAN COPIED -----------------------
        #
        # This used to `Copy-Item $SrcSdc`, i.e. hand every leaf block the SHELL's
        # SDC. That file constrains ports named gpu_clk / vid_clk / audio_clk --
        # and 63 of this design's 71 clock ports are simply named `clk`. So for
        # every leaf block Quartus resolved all three create_clock statements to
        # an empty collection and said so, three times, in every single run:
        #
        #   Warning: Ignored create_clock at blockfit.sdc(4):
        #            Argument <targets> is an empty collection
        #
        # WHICH MEANS EVERY PER-BLOCK FIT EVER RUN IN THIS PROJECT HAD NO TIMING
        # OBJECTIVE. The Fmax column of reports/synthesis/zhao_block_fit.json is
        # not a slow measurement, it is not a measurement at all -- and the area
        # columns were obtained with the fitter under no timing pressure, which
        # understates rather than overstates what a constrained fit needs. It is
        # why the old Field engine's 7.75 MHz was invisible for 47 rows.
        #
        # Found 2026-08-23 by an agent that went looking for why a block it had
        # just rebuilt reported an implausible Fmax.
        #
        # Every clock-shaped port name in fpga/rtl is covered below. A name a
        # given block does not have still logs the empty-collection warning, which
        # is harmless -- what was NOT harmless was covering none of them.
        $blockSdc = @(
            '# Generated per block by tools/quartus/run_block_fit.ps1.',
            '# The shell SDC is deliberately NOT used here: it names gpu_clk/vid_clk/',
            '# audio_clk, and leaf blocks name their clock port `clk`.',
            'create_clock -name clk        -period 10.000 [get_ports {clk}]',
            'create_clock -name clk_gpu    -period 10.000 [get_ports {clk_gpu}]',
            'create_clock -name gpu_clk    -period 10.000 [get_ports {gpu_clk}]',
            'create_clock -name vid_clk    -period 20.000 [get_ports {vid_clk}]',
            'create_clock -name clk_audio  -period 40.000 [get_ports {clk_audio}]',
            'create_clock -name audio_clk  -period 40.000 [get_ports {audio_clk}]',
            'create_clock -name wr_clk     -period 10.000 [get_ports {wr_clk}]',
            'create_clock -name rd_clk     -period 10.000 [get_ports {rd_clk}]',
            'derive_clock_uncertainty',
            '',
            '# ---- I/O DELAYS. Without these the Fmax below is not the block. ----',
            '# Added 2026-08-23 (RUN-20260823-1736), and it is the SECOND HALF of the',
            '# fix above rather than a refinement of it. Constraining the clock makes',
            '# REGISTER-TO-REGISTER paths timed. It does nothing for pin-to-register or',
            '# register-to-pin paths, which TimeQuest simply excludes when no I/O delay',
            '# is declared -- and for a leaf block whose arithmetic sits BETWEEN its',
            '# ports, that is most of the block.',
            '#',
            '# MEASURED, on three kept workspaces, by re-running quartus_sta on the',
            '# databases the fits had already produced:',
            '#',
            '#   zhao_texture_tmu @pre-rearch   worst path: texture_samples_o[19]',
            '#                                  -> texture_samples_o[27], 4.818 ns',
            '#   zhao_texture_tmu FILT_LANES=4  worst path: texture_samples_o[3]',
            '#                                  -> texture_samples_o[21], 4.634 ns',
            '#',
            '# Both blocks reported ~195 MHz. Both numbers are the 32-bit SATURATING',
            '# SAMPLE COUNTER carry chain. The 32 multiplies, the format decode, the',
            '# 48-bit address generator and the wrap folds appeared in NO timed path at',
            '# all, because each of them runs from an input pin or to an output pin.',
            '# The third workspace (FILT_LANES=2) differs only in that one filter output',
            '# happens to land in a real register -- and there the worst path is 20.462',
            '# ns, i.e. 48.9 MHz, through the very arithmetic the other two called fast.',
            '#',
            '# The model below is `same clock, no external budget`: every non-clock port',
            '# is assumed driven by, or captured into, a register in a neighbouring block',
            '# on this clock, with none of the period spent outside. That is optimistic',
            '# about inter-block routing and exact about the logic inside the block,',
            '# which is what a per-block characterisation is for.',
            '#',
            '# Guarded, because 8 of this design`s 71 clock ports are NOT called clk and',
            '# an unguarded set_input_delay against a clock that does not exist is an',
            '# error rather than a warning.',
            'set _zhao_clk [get_ports -nowarn {clk}]',
            'if {[get_collection_size $_zhao_clk] > 0} {',
            '    set _zhao_clkports [get_ports -nowarn {clk clk_gpu gpu_clk vid_clk clk_audio audio_clk wr_clk rd_clk}]',
            '    set _zhao_datain [remove_from_collection [all_inputs] $_zhao_clkports]',
            '    if {[get_collection_size $_zhao_datain] > 0} {',
            '        set_input_delay -clock clk 0.000 $_zhao_datain',
            '    }',
            '    if {[get_collection_size [all_outputs]] > 0} {',
            '        set_output_delay -clock clk 0.000 [all_outputs]',
            '    }',
            '}'
        )
        $blockSdc | Set-Content -LiteralPath (Join-Path $dir 'blockfit.sdc') -Encoding ascii
        'PROJECT_REVISION = "blockfit"' | Set-Content -LiteralPath (Join-Path $dir 'blockfit.qpf') -Encoding ascii

        # The same ordered source cone with absolute paths; only the top moves.
        $rtlAbs = (Join-Path $RepoRoot 'fpga\rtl').Replace('\', '/')
        $qsf = Get-Content -LiteralPath $SrcQsf
        $qsf = $qsf -replace '^set_global_assignment -name TOP_LEVEL_ENTITY.*', "set_global_assignment -name TOP_LEVEL_ENTITY $mod"
        $qsf = $qsf -replace '^set_global_assignment -name SDC_FILE.*', 'set_global_assignment -name SDC_FILE blockfit.sdc'
        $qsf = $qsf -replace '\.\./\.\./rtl/', "$rtlAbs/"

        # THE VIRTUAL-PIN ASYMMETRY, and it is deliberate.
        #
        # The shell project names zhao_shell_top's 101 ports explicitly, because
        # `-to *` matches every node in the design and that cost the composed fit
        # roughly 23 GB of assignment database. That fix is right for ONE top.
        #
        # This flow retargets the same project at a different top every
        # iteration, and those port names do not exist in any other module. Left
        # in place they would assign nothing, every block's real ports would
        # become PHYSICAL pins, and a block with 673 of them would fail the fit
        # or -- worse -- report a number shaped by pin pressure rather than by
        # logic. So the explicit list is stripped and the wildcard restored.
        #
        # The wildcard's cost is tolerable HERE precisely because these designs
        # are one block each: the measured fits ran 300-1300 s. It is the
        # composed cone where the same line became unaffordable.
        $qsf = $qsf | Where-Object { $_ -notmatch '^set_instance_assignment -name VIRTUAL_PIN' }
        $qsf += 'set_instance_assignment -name VIRTUAL_PIN ON -to *'
        if ($ExtraSources) {
            foreach ($extra in $ExtraSources) {
                $abs = (Resolve-Path (Join-Path $RepoRoot $extra)).Path.Replace([char]92, [char]47)
                $qsf += "set_global_assignment -name SYSTEMVERILOG_FILE $abs"
            }
        }
        foreach ($tp in $TopParameters) {
            $kv = $tp -split '=', 2
            if ($kv.Count -ne 2) { throw "TopParameters entry '$tp' is not NAME=VALUE" }
            $qsf += "set_parameter -name $($kv[0]) $($kv[1])"
        }
        $qsf | Set-Content -LiteralPath (Join-Path $dir 'blockfit.qsf') -Encoding ascii

        # ---------------------------------------------------------------------
        # SOURCE PROVENANCE, ENFORCED
        # ---------------------------------------------------------------------
        # The QSF above names sources by ABSOLUTE PATH IN THE LIVE WORKING TREE.
        # Nothing is copied into the workspace. So an edit made while a fit is
        # running silently changes what is being measured, and the row still
        # carries `sourceCommit = $head` as though it described that commit.
        #
        # MEASURED 2026-08-24: an edit to zhao_field_normalize.sv landed 101
        # seconds BEFORE this flow wrote its map report, during a 90-minute
        # zhao_field_seq fit. Afterwards there was no way to tell which version
        # had been elaborated, so a fit that may well have been perfectly good
        # was thrown away -- discarded for unprovable provenance, not for being
        # known wrong. That is the expensive way to learn this.
        #
        # Hashing every source costs milliseconds and turns a SILENT
        # contamination into a loud one. As with the ticks-suffixed workspace
        # above: it does not change what is measured, it changes whether the
        # measurement can be shown.
        $srcBefore = @{}
        $srcAfterMap = $null
        foreach ($line in $qsf) {
            if ($line -match '^set_global_assignment -name (?:SYSTEMVERILOG|VERILOG|VHDL)_FILE (.+)$') {
                $sp = $Matches[1].Trim()
                if (Test-Path -LiteralPath $sp) {
                    $srcBefore[$sp] = (Get-FileHash -LiteralPath $sp -Algorithm SHA256).Hash
                }
            }
        }

        # PER-ROW PROVENANCE. This file MERGES rows across runs (see the merge
        # note below), so a single top-level sourceCommit is a lie the moment
        # two runs contribute: it labels every row with the newest run's
        # commit. Measured 2026-08-21, a one-module run relabelled 41 rows
        # from 96c0394 as HEAD. Each row now carries the commit it was
        # actually measured at, and rows without one predate this change.
        $rowModule = if ($RowLabel) { "$mod$RowLabel" } else { $mod }
        $row = [ordered]@{ module = $rowModule; status = 'unknown'; sourceCommit = $head; rtlCleanAtHead = $rtlClean }
        if ($TopParameters) { $row.topParameters = ($TopParameters -join ' ') }
        # A parameter VARIANT is a second measurement of the SAME block, not a
        # second block. Marked so a census that totals DSPs by row cannot count
        # one block's frontier three times.
        if ($RowLabel) { $row.variantOf = $mod }
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
            # quartus_sta IS A STAGE, not an extra. Without it the lane fits a
            # block against a clock and then never asks whether it met it: the
            # fitter summary carries area and DSPs and NO timing at all. Fixing
            # the SDC (see the block-SDC comment above) made the fitter OPTIMISE
            # for a clock; this is what makes the answer visible. Before both
            # changes there was no Fmax for any of 47 rows, and no way to get one.
            foreach ($exe in @('quartus_map.exe', 'quartus_fit.exe', 'quartus_sta.exe')) {
                & (Join-Path $QuartusBin $exe) 'blockfit' *> (Join-Path $dir "$exe.log")
                if ($LASTEXITCODE -ne 0) { $row.status = "failed:$exe"; $ok = $false; break }
                # quartus_map is the ONLY stage that reads the sources, so hash
                # again the moment it finishes. Start-vs-end alone cannot see an
                # edit that is reverted before the run ends; straddling the read
                # with a checkpoint narrows that blind spot to an edit made AND
                # undone entirely inside the elaboration window. Stated rather
                # than hidden: this is tighter, not airtight -- only copying the
                # sources into the workspace would be airtight, and the one
                # `include in the tree (sdram_params.svh) means that needs
                # directory-structure preservation, which is not free.
                if ($exe -eq 'quartus_sta.exe') {
                    # Second STA invocation, purely to emit node-level paths.
                    $tcl = (Join-Path $PSScriptRoot 'block_paths.tcl').Replace([char]92, [char]47)
                    & (Join-Path $QuartusBin 'quartus_sta.exe') -t $tcl *> (Join-Path $dir 'quartus_sta_paths.log')
                }
                if ($exe -eq 'quartus_map.exe') {
                    $srcAfterMap = @{}
                    foreach ($k in @($srcBefore.Keys)) {
                        $srcAfterMap[$k] = if (Test-Path -LiteralPath $k) {
                            (Get-FileHash -LiteralPath $k -Algorithm SHA256).Hash
                        } else { 'MISSING' }
                    }
                }
                if ($sw.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
                    $row.status = 'timeout'; $ok = $false; break
                }
            }
        } finally {
            Pop-Location
        }
        $sw.Stop()
        $row.seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

        # See SOURCE PROVENANCE above. Re-hash before any result is believed.
        # This runs BEFORE the summary parse, so a contaminated run can never
        # reach `status = 'ok'` and can never be merged into the census.
        $srcChanged = @()
        foreach ($k in @($srcBefore.Keys)) {
            $nowHash = if (Test-Path -LiteralPath $k) {
                (Get-FileHash -LiteralPath $k -Algorithm SHA256).Hash
            } else { 'MISSING' }
            if ($nowHash -ne $srcBefore[$k]) { $srcChanged += (Split-Path -Leaf $k) }
            elseif ($srcAfterMap -and $srcAfterMap.ContainsKey($k) -and $srcAfterMap[$k] -ne $srcBefore[$k]) {
                # Changed during elaboration and put back afterwards. The row
                # would look pristine at the end; it is not.
                $srcChanged += ((Split-Path -Leaf $k) + '(during-map)')
            }
        }
        $row.sourcesHashed = $srcBefore.Count
        if ($srcChanged.Count -gt 0) {
            $row.status = 'contaminated:source-changed-during-fit'
            $row.contaminatedSources = ($srcChanged -join ' ')
            $ok = $false
            Write-Warning ("CONTAMINATED: {0} changed while {1} was being measured. Row discarded." -f ($srcChanged -join ', '), $mod)
        }

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
            $row.almsAvailable = Get-Capacity $t @('Logic utilization (in ALMs)', 'Logic utilization')
            $row.ramBlocksAvailable = Get-Capacity $t @('Total RAM Blocks')
            $row.dspBlocksAvailable = Get-Capacity $t @('Total DSP Blocks')

            # ---- the timing answer, from the STA report ----------------------
            # Slow 1100mV 85C is the corner the shell fit reports against, so
            # the two lanes stay comparable. Fmax here is the block ALONE with
            # virtual I/O -- it is an upper bound on what the block contributes
            # composed, never a claim about the machine.
            $sta = Join-Path $dir ('output_files' + [char]92 + 'blockfit.sta.rpt')
            if (Test-Path -LiteralPath $sta) {
                $s = [IO.File]::ReadAllText($sta)
                $m = [regex]::Match($s, '(?m)^;\s*([0-9.]+)\s*MHz\s*;\s*([0-9.]+)\s*MHz\s*;\s*(\S+)')
                if ($m.Success) {
                    $row.fmaxMhz = [double]$m.Groups[2].Value   # restricted Fmax
                    $row.fmaxClock = $m.Groups[3].Value
                }
                $setup = Get-StaSummary $s 'Setup Summary'
                if ($null -ne $setup) {
                    $row.setupSlackNs = $setup.slackNs
                    $row.setupTnsNs   = $setup.tnsNs
                }
                # KEEP THE REPORT, NOT ONLY THE NUMBER. Same gap the shell fit
                # had until 2026-08-24: the row carried an Fmax and the .sta.rpt
                # that explains WHERE it comes from died with the workspace, so
                # a slow block could be measured but never diagnosed.
                #
                # It bit immediately. Four pair fits ranked the renderer
                # 31.10 / 37.25 / 55.52 / 88.79 MHz, and the obvious next
                # question -- is the slowest pair limited by TESS or by the
                # sequenced multiply walk I put into NORMALS this morning --
                # could not be answered from anything that survived.
                $pathDir = Join-Path $RepoRoot 'reports/synthesis/blockpaths'
                New-Item -ItemType Directory -Path $pathDir -Force | Out-Null
                Copy-Item -LiteralPath $sta -Destination (Join-Path $pathDir ($rowModule + '.sta.rpt')) -Force
                # NODE-LEVEL PATHS. The .sta.rpt above carries summaries only;
                # it names an Fmax and never a From/To node, which ranks blocks
                # but cannot diagnose one. block_paths.tcl runs the report_timing
                # the shell lane has always had.
                foreach ($pr in @('setup','hold')) {
                    $prSrc = Join-Path $dir ('output_files/blockfit_' + $pr + '_paths.rpt')
                    if (Test-Path -LiteralPath $prSrc) {
                        Copy-Item -LiteralPath $prSrc -Destination (Join-Path $pathDir ($rowModule + '.' + $pr + '.rpt')) -Force
                    }
                }

                $hold = Get-StaSummary $s 'Hold Summary'
                if ($null -ne $hold) {
                    $row.holdSlackNs = $hold.slackNs
                    $row.holdTnsNs   = $hold.tnsNs
                }
            }
        } elseif ($row.status -eq 'unknown') {
            $row.status = 'no-summary'
        }

        $results.Add([pscustomobject]$row)
        # StrictMode: a failed or timed-out fit never gains the resource keys.
        $almShow = if ($row.Contains('alms') -and $null -ne $row.alms) { $row.alms } else { '-' }
        Write-Host ("{0,-34} {1,-16} {2,7}s   ALM {3}" -f $rowModule, $row.status, $row.seconds, $almShow)
        if (-not $KeepWorkspace) {
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    $out = [ordered]@{
        schemaVersion    = 1
        characterization = 'provisional-per-block-fit'
        # The commit of the MOST RECENT run only. Rows carry their own
        # sourceCommit; read those, not this.
        newestRunCommit  = $head
        newestRunClean   = $rtlClean
        tool             = [ordered]@{ name = 'Quartus Prime Lite'; version = '17.0.2' }
        device           = '5CSEBA6U23I7'
        blocks           = $results.ToArray()
        limitations      = @(
            '5CSEBA6U23I7 is a provisional capacity target, not board truth.',
            'All I/O is virtual: no package pins, no board I/O delays, no PLLs, no physical clocks.',
            'A per-block fit does not characterize the composed machine routing or timing closure.',
            'Rows carry their own sourceCommit. A row measured at an older commit describes THAT code, not HEAD. Check per-row provenance before totalling anything.',
            'This census does not cover the design: 42 of the repository''s 88 RTL modules, and several of those rows carry no data. See reports/DSP_Audit_2026-08-21.md.',
            'The composed fit is NOT blocked on machine memory. The 28.4 GB figure once recorded here was a wildcard virtual-pin bug fixed in d1a2b8a; composed synthesis completes in 42:33 at a 6.2 GB peak (f3506b6). It IS blocked on zhao_cmd_dma, which times out: 156 dependent CRC steps in one cycle (reports/REMAINING_BLOCKERS.md).',
            'Rows carrying a variantOf field are alternate PARAMETER SETTINGS of the row they name, measured to expose a resource frontier. They are the same block. EXCLUDE them from any total.',
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
    # AND A FAILED RUN MUST NOT DESTROY A GOOD MEASUREMENT.
    #
    # The merge above keeps rows this run did not touch. It did NOT protect a
    # row this run touched and FAILED to measure -- a timeout or a killed
    # fitter produced a row with status set and every number null, which
    # replaced a perfectly good prior measurement.
    #
    # Measured 2026-08-23, on this script's own author: zhao_geom_lod stood at
    # 1,183 ALMs / 6 DSPs; a re-fit was killed by memory contention; the report
    # then read status=failed with alm=null, dsp=null, and the real numbers were
    # gone. Recoverable from another branch that time -- the point, again, is
    # that nothing said it had happened.
    #
    # So a non-ok result KEEPS the prior measurement and records the failed
    # attempt beside it. The failure is still visible (lastAttempt*), and the
    # number is still there. Overwriting only happens ok -> ok, which is what
    # re-running a block actually means.
    foreach ($row in $results) {
        $prior = $merged[$row.module]
        if ($row.status -ne 'ok' -and $null -ne $prior -and $prior.status -eq 'ok') {
            $kept = $prior | Select-Object *
            $kept | Add-Member -NotePropertyName 'lastAttemptStatus'  -NotePropertyValue $row.status  -Force
            $kept | Add-Member -NotePropertyName 'lastAttemptSeconds' -NotePropertyValue $row.seconds -Force
            $kept | Add-Member -NotePropertyName 'lastAttemptCommit'  -NotePropertyValue $row.sourceCommit -Force
            $merged[$row.module] = $kept
            Write-Warning ("{0}: this run ended '{1}'; KEEPING the previous measurement ({2} ALM / {3} DSP) rather than erasing it" -f $row.module, $row.status, $prior.alms, $prior.dspBlocks)
        } else {
            $merged[$row.module] = $row
        }
    }
    $out.blocks = @($merged.Keys | Sort-Object | ForEach-Object { $merged[$_] })

    [IO.File]::WriteAllText($dest, (($out | ConvertTo-Json -Depth 8) + "`n"), $Utf8NoBom)
    Write-Host ("WROTE {0} ({1} block(s); {2} measured this run)" -f $dest, $out.blocks.Count, $results.Count)
} finally {
    if (-not $KeepWorkspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    }
}
