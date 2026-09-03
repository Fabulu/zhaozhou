# fit_rules.ps1 -- the structural gate on a fit, as ONE law with two callers.
#
# ---------------------------------------------------------------------------
# WHY
# ---------------------------------------------------------------------------
# reports/islandrearchitecture5.md S3.4 sets numeric tripwires per block and
# closes with the sentence this file exists to enforce:
#
#     A fit that meets Fmax while violating its memory/DSP structure is not a
#     pass.
#
# Those tripwires existed only as prose. Nothing mechanical checked them, so on
# 2026-09-03 a cache rebuild that put 9,728 bits of array into flip-flops -- 2
# M10K where its own predecessor had 4 -- was reported as a PASS on the
# strength of 98.66 MHz, because Fmax was the only number the tooling produced.
#
# ---------------------------------------------------------------------------
# WHY IT IS A SEPARATE FILE
# ---------------------------------------------------------------------------
# run_block_fit.ps1 applies these rules when a fit finishes, and
# check_fit_rules.ps1 applies the identical law to already-recorded rows
# without running Quartus at all. Two copies of a rule drift; one file
# dot-sourced twice cannot.
#
# The first attempt at testing this gate regex-extracted the functions out of
# the runner, the extraction silently returned an empty string, and the harness
# printed PASS for a block that violates all three of its rules -- the same
# false-pass shape the gate is here to remove. Hence: a real file.
#
# ASCII ONLY. Windows PowerShell 5.1 reads a UTF-8 script as ANSI, so an
# em-dash in a comment becomes a parse error forty lines later.

function Read-FitRules([string]$Path) {
    $map = @{}
    if (-not (Test-Path -LiteralPath $Path)) { return $map }
    $top = $null
    $inRules = $false
    foreach ($raw in (Get-Content -LiteralPath $Path)) {
        $line = $raw -replace '#.*$', ''
        if ($line -match '^\s*-\s*top:\s*(\S+)\s*$') {
            $top = $Matches[1]; $inRules = $false
            continue
        }
        if ($line -match '^\s*rules:\s*$')   { $inRules = $true;  continue }
        if ($line -match '^\s*sources:\s*$') { $inRules = $false; continue }
        if ($inRules -and $null -ne $top -and
            $line -match '^\s*(min_m10k|max_m10k|max_registers|max_alms|max_dsp|min_dsp):\s*(\d+)\s*$') {
            if (-not $map.ContainsKey($top)) { $map[$top] = @{} }
            $map[$top][$Matches[1]] = [int]$Matches[2]
        }
    }
    return $map
}

# $Row is anything carrying .alms/.registers/.ramBlocks/.dspBlocks -- an
# ordered hashtable from a live fit, or a PSCustomObject out of the recorded
# JSON. Returns the list of violations; empty means the structure is what the
# architecture asked for.
function Test-FitRules($Row, $Rules) {
    $bad = New-Object System.Collections.Generic.List[string]
    if ($null -eq $Rules -or $null -eq $Row) { return $bad }

    $alm  = Get-FitMetric $Row 'alms'
    $reg  = Get-FitMetric $Row 'registers'
    $m10k = Get-FitMetric $Row 'ramBlocks'
    $dsp  = Get-FitMetric $Row 'dspBlocks'

    # The MINIMUM is the important half. A maximum catches a block that grew; a
    # minimum catches a block whose storage quietly stopped being storage,
    # which is the failure that looks like success.
    if ($Rules.ContainsKey('min_m10k') -and $null -ne $m10k -and $m10k -lt $Rules['min_m10k']) {
        $bad.Add(("M10K {0} < required {1} -- the storage did not infer as memory" -f $m10k, $Rules['min_m10k']))
    }
    if ($Rules.ContainsKey('max_m10k') -and $null -ne $m10k -and $m10k -gt $Rules['max_m10k']) {
        $bad.Add(("M10K {0} > allowed {1}" -f $m10k, $Rules['max_m10k']))
    }
    if ($Rules.ContainsKey('max_registers') -and $null -ne $reg -and $reg -gt $Rules['max_registers']) {
        $bad.Add(("registers {0} > allowed {1} -- state that belongs in memories is in flip-flops" -f $reg, $Rules['max_registers']))
    }
    if ($Rules.ContainsKey('max_alms') -and $null -ne $alm -and $alm -gt $Rules['max_alms']) {
        $bad.Add(("ALM {0} > allowed {1}" -f $alm, $Rules['max_alms']))
    }
    if ($Rules.ContainsKey('max_dsp') -and $null -ne $dsp -and $dsp -gt $Rules['max_dsp']) {
        $bad.Add(("DSP {0} > allowed {1}" -f $dsp, $Rules['max_dsp']))
    }
    if ($Rules.ContainsKey('min_dsp') -and $null -ne $dsp -and $dsp -lt $Rules['min_dsp']) {
        $bad.Add(("DSP {0} < required {1}" -f $dsp, $Rules['min_dsp']))
    }
    return $bad
}

function Get-FitMetric($Row, [string]$Key) {
    if ($Row -is [hashtable] -or
        $Row -is [System.Collections.Specialized.OrderedDictionary]) {
        if ($Row.Contains($Key)) { return $Row[$Key] }
        return $null
    }
    $prop = $Row.PSObject.Properties[$Key]
    if ($null -ne $prop) { return $prop.Value }
    return $null
}
