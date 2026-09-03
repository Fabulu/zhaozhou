# fit-running-stop-guard.ps1 -- do not idle while a Quartus fit is running.
#
# WHY THIS EXISTS
# ---------------
# Fabian, 2026-09-03: "make this a rule, after you start a fit, try to see if
# there's something else you can feasibly work on" -- and then, after the rule
# was written into CLAUDE.md and violated twice in the same session:
# "maybe a hook instead of a claude.md?"
#
# He is right. A rule in a prose file is advisory and gets skipped under the
# pull of reporting a status. A Stop hook is mechanical.
#
# Quartus fits in this project run 20-90 minutes. Waiting one out is the single
# most expensive habit available here.
#
# WHAT IT DOES
# ------------
# On a stop attempt, if a quartus process is alive, it blocks once and hands
# back the specific next question -- what is OUTSIDE the running fit's source
# closure -- rather than a vague "keep going".
#
# `stop_hook_active` is honoured as the loop guard: it blocks at most once per
# stop sequence, so a genuine "there is nothing left to do" still terminates.
#
# THIS FILE IS THE AUTHORITATIVE COPY. It lives in the repo so it is version
# controlled and survives a machine; ~/.claude/settings.json points at this
# path. A hook kept only in a home directory is a rule that vanishes with the
# laptop -- the same failure as direction kept only in a run folder.
$ErrorActionPreference = 'Stop'

$raw = [Console]::In.ReadToEnd()
$active = $false
if ($raw) {
    try {
        $payload = $raw | ConvertFrom-Json
        if ($payload.PSObject.Properties['stop_hook_active']) {
            $active = [bool]$payload.stop_hook_active
        }
    } catch {
        # A hook that dies on unexpected input would block every stop forever.
        $active = $false
    }
}

# Already blocked once in this stop sequence: let it go, or this loops.
if ($active) { exit 0 }

$fit = @(Get-Process -ErrorAction SilentlyContinue |
         Where-Object { $_.ProcessName -like 'quartus*' })
if ($fit.Count -eq 0) { exit 0 }

$names = ($fit | ForEach-Object {
    '{0} ({1:N0} min elapsed)' -f $_.ProcessName,
        ((Get-Date) - $_.StartTime).TotalMinutes
}) -join ', '

$reason = @"
A Quartus fit is still running: $names.

Do not idle and do not stop here to report status. Per CLAUDE.md, a running fit
is not a reason to wait -- find work that does not touch its sources and do it.

To know what is safe: a per-block fit compiles ONLY its own closure, and
design/fit_targets.yml names exactly which files. Everything outside that list
is free. The one hard constraint is the live-tree trap (QUARTUS_GOTCHAS.md 11)
-- never edit a file inside the running fit's closure, because the fit reads
the working tree.

Pick the next item off reports/DOCKET.md or the run's TASK_LOG.md and continue.
If the honest answer is that every remaining task genuinely requires this
toolchain, say exactly that and which tasks they are -- then stopping is fine.
"@

@{ decision = 'block'; reason = $reason } | ConvertTo-Json -Compress
exit 0
