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
# WHY IT DOES NOT USE `stop_hook_active`
# --------------------------------------
# The first version bypassed itself whenever `stop_hook_active` was set, as a
# loop guard. That is wrong when ANOTHER Stop hook exists: the session's goal
# hook blocked first, which sets the flag, and this hook then stood down for
# the rest of the session. Fabian caught it on the first fit after installing
# it -- "so uh... your hook didn't fire" -- and he was right.
#
# The loop guard is now a BUDGET PER FIT instead. It blocks up to
# $MaxBlocks times for a given fit process, then allows the stop. That cannot
# loop forever, it is unaffected by other hooks, and every new fit gets a fresh
# budget. State lives beside the temp workspaces and is keyed on the fit's
# start time, so a new fit resets it automatically.
#
# ASCII ONLY. Windows PowerShell 5.1 reads a UTF-8 script as ANSI, so an
# em-dash in a comment becomes a parse error forty lines later.
$ErrorActionPreference = 'Stop'
$MaxBlocks = 4

# Read and discard stdin: the payload is not consulted any more, but a hook
# that leaves it unread can wedge the caller's pipe.
try { [Console]::In.ReadToEnd() | Out-Null } catch { }

$fit = @(Get-Process -ErrorAction SilentlyContinue |
         Where-Object { $_.ProcessName -like 'quartus*' })
if ($fit.Count -eq 0) { exit 0 }

# Key on the OLDEST running quartus process's start time: stable for the life
# of one fit, different for the next.
$oldest = $fit | Sort-Object StartTime | Select-Object -First 1
$key = '{0}-{1}' -f $oldest.ProcessName, $oldest.StartTime.Ticks

$stateFile = Join-Path $env:TEMP 'zhao-fit-stop-guard.state'
$count = 0
if (Test-Path -LiteralPath $stateFile) {
    try {
        $parts = (Get-Content -LiteralPath $stateFile -Raw).Trim() -split '\s+'
        if ($parts.Count -ge 2 -and $parts[0] -eq $key) { $count = [int]$parts[1] }
    } catch { $count = 0 }
}

if ($count -ge $MaxBlocks) { exit 0 }

$count++
try { Set-Content -LiteralPath $stateFile -Value ("{0} {1}" -f $key, $count) -Encoding ascii } catch { }

$names = ($fit | ForEach-Object {
    '{0} ({1:N0} min elapsed)' -f $_.ProcessName,
        ((Get-Date) - $_.StartTime).TotalMinutes
}) -join ', '

$reason = @"
A Quartus fit is still running: $names.
(guard $count of $MaxBlocks for this fit)

Do not idle and do not stop here to report status. Per CLAUDE.md, a running fit
is not a reason to wait -- find work that does not touch its sources and do it.

To know what is safe: a per-block fit compiles ONLY its own closure, and
design/fit_targets.yml names exactly which files. Everything outside that list
is free. The one hard constraint is the live-tree trap (QUARTUS_GOTCHAS.md 11)
-- never edit a file inside the running fit's closure, because the fit reads
the working tree.

Pick the next item off reports/DOCKET.md or the run's TASK_LOG.md and continue.
If the honest answer is that every remaining task genuinely requires this
toolchain, say exactly that and which tasks they are -- then stopping is fine,
and this guard will stand down after $MaxBlocks attempts regardless.
"@

@{ decision = 'block'; reason = $reason } | ConvertTo-Json -Compress
exit 0
