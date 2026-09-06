# watch_fit.ps1 -- periodic liveness check-in for a running Quartus stage.
#
# WHY THIS EXISTS, AND WHY IT IS NOT A WATCHDOG
# ---------------------------------------------
# `run_block_fit.ps1` starts a watchdog that KILLS Quartus at a deadline. That
# is the right default for an unattended run and the wrong thing for a fit the
# owner wants to finish: a deadline that fires on a healthy fit destroys hours
# of work and leaves a row saying "timeout", which reads as "this block does not
# fit" when all it meant was "we did not wait".
#
# So this reports and never kills. Killing the watchdog leaves the fitter and
# the run_block_fit host alive, and the host still runs quartus_sta and writes
# the row when the fitter finishes -- the supervision is what goes away, not the
# bookkeeping.
#
# LIVENESS IS CPU, NOT LOG GROWTH
# -------------------------------
# An earlier version of a watcher in this tree used log growth and would have
# declared a healthy fit hung: the fitter's log sat silent for 107 minutes while
# its CPU climbed about 50 seconds per minute. Placement and routing are long
# quiet stretches of pure computation. CPU delta over the sample interval is the
# measurement that matches what the process is actually doing; the stage name
# tells you WHICH long stretch it is in.
param(
    [int]$IntervalSeconds = 600,
    [int]$MaxHours = 12,
    [string]$LogPath
)

if (-not $LogPath) { $LogPath = Join-Path $env:TEMP 'zhao-fit-watch.log' }

function Sample {
    $rows = @()
    foreach ($n in 'quartus_map', 'quartus_fit', 'quartus_sta') {
        foreach ($p in (Get-Process -Name $n -ErrorAction SilentlyContinue)) {
            $rows += [pscustomobject]@{ Name = $n; Id = $p.Id; Cpu = $p.CPU }
        }
    }
    return $rows
}

$stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
Add-Content -LiteralPath $LogPath -Value "== watch start $stamp interval ${IntervalSeconds}s =="

$prev = @{}
$deadline = (Get-Date).AddHours($MaxHours)
while ((Get-Date) -lt $deadline) {
    $rows = Sample
    $stamp = (Get-Date).ToString('HH:mm:ss')
    if ($rows.Count -eq 0) {
        Add-Content -LiteralPath $LogPath -Value "$stamp  no quartus stage running -- the run has finished or died"
        break
    }
    foreach ($r in $rows) {
        $key = "$($r.Name)-$($r.Id)"
        $delta = if ($prev.ContainsKey($key)) { [math]::Round($r.Cpu - $prev[$key], 1) } else { -1 }
        $prev[$key] = $r.Cpu
        # A delta at or near zero across a ten-minute window is the only thing
        # that should worry anyone. Reported as a number rather than a verdict:
        # this script does not decide that a fit is stuck.
        $note = if ($delta -lt 0) { 'first sample' }
                elseif ($delta -lt 5) { 'CPU BARELY MOVED -- look at it' }
                else { 'alive' }
        Add-Content -LiteralPath $LogPath -Value (
            "$stamp  {0} pid {1}  cpu {2:N0}s  (+{3}s this window)  {4}" -f `
                $r.Name, $r.Id, $r.Cpu, $delta, $note)
    }
    Start-Sleep -Seconds $IntervalSeconds
}
Add-Content -LiteralPath $LogPath -Value "== watch end $((Get-Date).ToString('HH:mm:ss')) =="
