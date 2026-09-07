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
            $line -match '^\s*(min_m10k|max_m10k|min_memory_bits|max_registers|max_alms|max_dsp|min_dsp|min_fmax_mhz):\s*(\d+)\s*$') {
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
    $fmax = Get-FitMetric $Row 'fmaxMhz'

    # The MINIMUM is the important half. A maximum catches a block that grew; a
    # minimum catches a block whose storage quietly stopped being storage,
    # which is the failure that looks like success.
    # min_memory_bits -- THE SOUND WAY TO ASK "DID THE STORAGE INFER".
    #
    # min_m10k is not, and it cost two false failures on 2026-09-04. A count of
    # M10K blocks is NOT bounded below by declared_bits / 10240, because
    # Quartus has a second memory primitive: bits can land in MLAB instead, and
    # a wide array can be packed in shapes a capacity argument does not
    # predict. zhao_terrain_residency_v2 declares 167,936 bits, "must therefore
    # need 17 M10K", and fitted into 16.
    #
    # BITS are well defined and are what the question actually means. The
    # fitter reports blockMemoryBits directly; an array that stayed in
    # flip-flops contributes nothing to it, whichever primitive the rest chose.
    # zhao_texture_cache_pipe measured 128 bits before its rework and 8,320
    # after -- the same fact min_m10k was groping at, stated in the unit that
    # does not depend on how the tool packed it.
    $bits = Get-FitMetric $Row 'blockMemoryBits'
    if ($Rules.ContainsKey('min_memory_bits') -and $null -ne $bits -and
        $bits -lt $Rules['min_memory_bits']) {
        $bad.Add(("block memory {0} bits < required {1} -- the storage did not infer as memory" -f $bits, $Rules['min_memory_bits']))
    }

    if ($Rules.ContainsKey('min_m10k') -and $null -ne $m10k -and $m10k -lt $Rules['min_m10k']) {
        $bad.Add(("M10K {0} < required {1} -- NOTE: an M10K COUNT is not a sound floor, see min_memory_bits" -f $m10k, $Rules['min_m10k']))
    }
    if ($Rules.ContainsKey('max_m10k') -and $null -ne $m10k -and $m10k -gt $Rules['max_m10k']) {
        $bad.Add(("M10K {0} > allowed {1}" -f $m10k, $Rules['max_m10k']))
    }
    if ($Rules.ContainsKey('max_registers') -and $null -ne $reg -and $reg -gt $Rules['max_registers']) {
        # The message used to assert WHY: "state that belongs in memories is in
        # flip-flops". It has now been wrong twice. zhao_texture_fragrob fired
        # this rule at 2,631 while holding 96% of its wide payload (6,464 of
        # 6,720 declared bits) in 13 M10Ks -- its overrun is control state, not
        # misplaced payload. zhao_terrain_residency_v2 fired the memory-floor
        # rule with only 1,243 registers, which cannot hold the missing 17,408
        # bits either.
        #
        # A gate that explains itself is better than one that does not, right up
        # until the explanation is wrong -- then it sends the next reader to
        # reshape arrays that are already RAM. So state the MEASUREMENT and name
        # where the answer lives, rather than asserting a cause.
        $bad.Add(("registers {0} > allowed {1} -- read blockMemoryBits/ramBlocks beside this before assuming misplaced payload; the fit's RAM Summary names every array that inferred (QUARTUS_GOTCHAS 14)" -f $reg, $Rules['max_registers']))
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

    # min_fmax_mhz -- ADDED 2026-09-07, AND THE REASON IS TWO BLOCKS IN A ROW.
    #
    # This file's own header says "a fit that meets Fmax while violating its
    # memory/DSP structure is not a pass". The converse had no way to be said at
    # all: there was no rule for the clock, so a block whose storage was exactly
    # right and whose ALM count was comfortable PASSED while missing the 100 MHz
    # product clock. zhao_terrain_pagestream came back at 97.11 MHz and
    # zhao_terrain_cmd at 90.87, both `ok`, both 3% and 9% short, and both only
    # noticed because a human read the number next to the row.
    #
    # A gate that can only fail in one direction is half a gate. This is the
    # other half.
    #
    # WHOLE MHz, deliberately: `min_fmax_mhz: 100` is the product clock, and a
    # rule file that could say 99.5 would invite tuning the gate to the
    # measurement, which is the habit this whole file exists against. A block
    # that wants headroom asks for more than 100, not for a fraction less.
    #
    # It is OPT-IN. Nothing gains this rule by existing; a block gets it when
    # its owner writes it into design/fit_targets.yml, because a clock a block
    # was never designed against is not a gate, it is a surprise.
    if ($Rules.ContainsKey('min_fmax_mhz') -and $null -ne $fmax -and
        [double]$fmax -lt [double]$Rules['min_fmax_mhz']) {
        $bad.Add(("Fmax {0} MHz < required {1} MHz -- the resource gates can all pass while the clock does not; see this rule's note" -f $fmax, $Rules['min_fmax_mhz']))
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
