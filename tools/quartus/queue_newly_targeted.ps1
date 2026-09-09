# queue_newly_targeted.ps1 -- fit the twelve blocks that became measurable today.
#
# ---------------------------------------------------------------------------
# WHY FULL FITS AND NOT MapOnly
# ---------------------------------------------------------------------------
# The DSP and ALM totals are FLOORS because 43 of 45 unmeasured production
# blocks had no `- top:` entry in design/fit_targets.yml, so run_block_fit
# refused them at preflight and nobody could measure them. Fourteen targets were
# authored on 2026-09-09; two of those blocks already had rows, leaving twelve
# that are now mappable and still unmeasured.
#
# MapOnly will not do, for two reasons that compound:
#
#   * `tools/budget/dsp_census.py` counts UNLABELLED rows only -- a labelled row
#     is an alternate measurement of one module, not additional hardware -- and
#     run_block_fit now REQUIRES a label on any MapOnly (it silently replaced two
#     full-fit rows before that guard existed). So a map row cannot enter the
#     census by construction.
#   * A map row carries no `alms` and no `ramBlocks` at all. Half the budget
#     question is ALM, and the island is 3,337 over its redline.
#
# So: full fits, unlabelled, which is what puts a real number where a floor is.
#
# THREE OF THESE SCORE PRE-REGISTERED RULES, quoted from each block's own header
# before any measurement existed:
#
#   zhao_field_v3_mulbank   max_dsp 12    "four 33-bit lanes map to about 12 DSPs"
#   zhao_field_v3_len       max_alms 2000 "EIGHT ROOTS IS ROUGHLY 2,000 ALMs"
#   zhao_field_v3_rf        max_m10k 12   the probe's figure, stated as a LOWER bound
#
# `max_alms` and `max_m10k` cannot fire on a map row -- another reason these are
# fits. A rule that cannot fire is the failure this repository keeps paying for.
#
# Each block is wrapped: one failure must not take the other eleven with it.
# ErrorActionPreference does not survive a `throw`, and run_block_fit's preflight
# throws by design.
$ErrorActionPreference = 'Continue'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

function Wait-Idle {
    while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 30 }
}

# Smallest-looking first, so a kill part-way through still banks rows. The three
# with rules go early: they answer a filed question, and an unanswered prediction
# is worth more than another unknown block measured.
$mods = @(
    'zhao_field_v3_mulbank',      # rule: max_dsp 12
    'zhao_field_v3_len',          # rule: max_alms 2000
    'zhao_field_v3_rf',           # rule: max_m10k 12
    'zhao_field_v3_dispatch',
    'zhao_field_v3_sbank',
    'zhao_field_v3_spline',
    'zhao_field_v3_noise',
    'zhao_field_v3_normalize',
    'zhao_field_v3_trig',
    'zhao_field_v3_rot',
    'zhao_field_v3_ring_svc',
    'zhao_geom_skin_norm'
)

Write-Output ('queue_newly_targeted: ' + $mods.Count + ' block(s) to fit.')
$done = 0
$failed = @()
foreach ($m in $mods) {
    Write-Output ''
    Write-Output ('queue_newly_targeted: [' + ($done + 1) + '/' + $mods.Count + '] ' + $m)
    Wait-Idle
    try {
        & "$PSScriptRoot\run_block_fit.ps1" -Module $m *>&1 |
            Tee-Object -FilePath ('fit-new-' + $m + '.log') | Select-Object -Last 4
    } catch {
        Write-Output ('queue_newly_targeted: ' + $m + ' FAILED -- ' + $_.Exception.Message)
        $failed += $m
    }
    $done++
}

Write-Output ''
Write-Output ('QUEUE_NEWLY_TARGETED DONE: ' + ($mods.Count - $failed.Count) + ' of ' + $mods.Count + ' completed.')
if ($failed.Count) { Write-Output ('  failed: ' + ($failed -join ', ')) }
