[CmdletBinding()]
param(
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [switch]$NoPush,
    [switch]$KeepWorkspace,
    # Default 1 was chosen when the map was believed to peak at 28.4 GB, since
    # each worker carries its own working set. That figure was a wildcard bug
    # (see the header) and the real peak is 6.2 GB, so 1 is now merely
    # conservative rather than necessary. It costs wall time and changes
    # nothing about the result; raise it freely.
    [int]$Processors = 1
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# COMPOSED SHELL FIT, run on a machine that can actually hold it.
#
# WHY THIS EXISTS. Per-block fits cannot answer the one question that matters
# before a device is frozen: does the COMPOSED machine fit, and does it close
# timing. Twenty-two blocks summing to some ALM count is an UPPER BOUND,
# inflated by ~9,000 virtual pins that become plain wires once composed and by
# the absence of any cross-block optimisation or resource sharing.
#
# CORRECTED 2026-08-21. This header used to say the composed fit "does not fit
# the primary development machine", citing quartus_map committing 28.4 GB
# against 24 GB. THAT WAS A BUG, NOT A REQUIREMENT. The project's last line was
# `set_instance_assignment -name VIRTUAL_PIN ON -to *`, and a wildcard instance
# assignment is matched against every node name in the design rather than the
# top-level ports it was written for, so Quartus carried a VIRTUAL_PIN
# candidacy for every internal net in the cone. Fixed in d1a2b8a by naming the
# 101 ports explicitly; the composed run then completed analysis and synthesis
# in 42:33 at a 6.2 GB peak (f3506b6).
#
# Two things followed from believing the false figure: this script was written
# for a second machine that was never needed, and $Processors was defaulted to
# 1 to cut a memory peak that had already gone away.
#
# 6.2 GB is still not fully explained. 9c693a9 measured that parsing the entire
# source cone is free (0.24 GB) and that the cost is in ELABORATION, which is
# superlinear here for a top of sixteen ordinary instances with no generate
# blocks and no large arrays. The Quartus 17.0.2 Lite elaborator is the
# suspect, and trying a newer Quartus is the named lever nobody has pulled.
#
# So this script runs on a second machine with more memory and brings the
# answer back.
#
# ---------------------------------------------------------------------------
# THE CONCURRENCY RULE, and it is the whole design of this script
# ---------------------------------------------------------------------------
#
# Another agent is working in this repository AT THE SAME TIME. So this script
# must never produce a merge conflict, and it achieves that by one rule:
#
#     IT ONLY EVER CREATES FILES INSIDE ITS OWN NEW DIRECTORY.
#
# It edits no shared file. Not the block-fit report, not the ledger, not a
# contract, not a log. Every run mints
# `reports/composed/<host>-<shortsha>-<utc>/` from the machine name, the commit
# it measured and the UTC start time, so two machines cannot collide even if
# they start in the same second on the same commit. Git merges disjoint new
# files without a conflict, always.
#
# It also NEVER rebases or merges anyone's work into its own. If the push is
# rejected because the branch moved, it fetches, rebases ITS OWN single commit
# on top, and retries. Its commit touches only files nobody else has.
# ---------------------------------------------------------------------------

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Fail([string]$m) { Write-Host "FAILED: $m" -ForegroundColor Red; exit 1 }
function Step([string]$m) { Write-Host "`n== $m" -ForegroundColor Cyan }

# ---- 1. the tree must be clean, or the result is not attributable ----------
Step '1/6 checking the tree'
# `-c core.autocrlf=true` is not decoration. On the primary development machine
# PowerShell resolves `git` to devkitPro's msys2 build (the same shadowing the
# env script documents for cmake), which carries no core.autocrlf, and a status
# taken through it reported 156 modified files that were PURE line-ending churn
# — 673 insertions against 673 deletions on one spec file, and nothing at all
# under --ignore-all-space. Forcing the setting makes this check answer the same
# way whichever git is first on PATH. Without it this script would refuse to run
# on a perfectly clean tree, or worse, a `git add -A` somewhere would commit the
# churn.
$dirty = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain) -join "`n"
if (-not [string]::IsNullOrWhiteSpace($dirty)) {
    Write-Host $dirty
    Fail 'working tree is not clean. A fit result is only worth anything if it names the exact commit it measured; commit or stash first.'
}
$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
$short = (& git -C $RepoRoot rev-parse --short HEAD).Trim()
if ($head -notmatch '^[0-9a-f]{40}$') { Fail 'could not resolve HEAD' }

foreach ($exe in @('quartus_map.exe', 'quartus_fit.exe', 'quartus_sta.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $QuartusBin $exe) -PathType Leaf)) {
        Fail "Quartus executable not found: $(Join-Path $QuartusBin $exe). Pass -QuartusBin if it lives elsewhere."
    }
}

# ---- 2. mint this run's own directory --------------------------------------
Step '2/6 minting the run directory'
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
$hostTag = ($env:COMPUTERNAME -replace '[^A-Za-z0-9]', '').ToLower()
if ([string]::IsNullOrWhiteSpace($hostTag)) { $hostTag = 'unknown' }
$runId = "$hostTag-$short-$stamp"
$runDir = Join-Path $RepoRoot "reports\composed\$runId"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
Write-Host "run: $runId"

# ---- 3. record the machine, because capacity results are machine-shaped ----
Step '3/6 recording the machine'
$cs = Get-CimInstance Win32_ComputerSystem
$os = Get-CimInstance Win32_OperatingSystem
$cpu = @(Get-CimInstance Win32_Processor)[0]
$qver = 'unknown'
try {
    $qout = & (Join-Path $QuartusBin 'quartus_map.exe') --version 2>&1 | Select-Object -First 4
    $qver = ($qout -join ' ').Trim()
} catch { }
$machine = [ordered]@{
    runId          = $runId
    sourceCommit   = $head
    startedUtc     = (Get-Date).ToUniversalTime().ToString('o')
    host           = $env:COMPUTERNAME
    os             = $os.Caption
    osVersion      = $os.Version
    cpu            = $cpu.Name
    cores          = $cpu.NumberOfCores
    threads        = $cpu.NumberOfLogicalProcessors
    ramTotalBytes  = $cs.TotalPhysicalMemory
    ramFreeBytesAtStart = ($os.FreePhysicalMemory * 1KB)
    pageFileBytes  = ($os.TotalVirtualMemorySize * 1KB)
    quartus        = $qver
    quartusBin     = $QuartusBin
    processors     = $Processors
}
[IO.File]::WriteAllText((Join-Path $runDir 'MACHINE.json'),
    (($machine | ConvertTo-Json -Depth 6) + "`n"), $Utf8NoBom)
Write-Host ("RAM total {0:N1} GB, free at start {1:N1} GB" -f ($cs.TotalPhysicalMemory / 1GB), (($os.FreePhysicalMemory * 1KB) / 1GB))
if ($cs.TotalPhysicalMemory -lt 8GB) {
    Write-Host 'WARNING: the composed map last peaked at 6.2 GB. Under ~8 GB this may thrash rather than fail cleanly.' -ForegroundColor Yellow
}

# ---- 4. run it -------------------------------------------------------------
Step '4/6 composed fit (this is the long part; the map stage is where memory peaks)'
$sw = [Diagnostics.Stopwatch]::StartNew()
$rc = 0
$transcript = Join-Path $runDir 'run.log'
try {
    # NOT $args: that is a PowerShell automatic variable and assigning to it
    # inside a script is a good way to get a confusing failure later.
    $fitArgs = @('-ReportRoot', $runDir, '-QuartusBin', $QuartusBin,
                 '-Processors', $Processors)
    if ($KeepWorkspace) { $fitArgs += '-KeepWorkspace' }
    & (Join-Path $PSScriptRoot 'run_shell_fit.ps1') @fitArgs *>&1 |
        Tee-Object -FilePath $transcript
    $rc = $LASTEXITCODE
} catch {
    $rc = 1
    Add-Content -LiteralPath $transcript -Value ("EXCEPTION: " + $_.Exception.Message) -Encoding utf8
}
$sw.Stop()

# A FAILED run is still a result and is still committed. "The composed fit did
# not complete on 32 GB either, and here is how far it got and what it was
# doing" is real evidence about the device decision. Discarding it would leave
# the next person to rediscover it.
$outcome = [ordered]@{
    runId        = $runId
    sourceCommit = $head
    exitCode     = $rc
    completed    = ($rc -eq 0)
    seconds      = [math]::Round($sw.Elapsed.TotalSeconds, 1)
    finishedUtc  = (Get-Date).ToUniversalTime().ToString('o')
    ramFreeBytesAtEnd = ((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory * 1KB)
    note         = if ($rc -eq 0) { 'composed fit completed' } else { 'composed fit did NOT complete; see run.log. This is recorded deliberately rather than discarded.' }
}
[IO.File]::WriteAllText((Join-Path $runDir 'OUTCOME.json'),
    (($outcome | ConvertTo-Json -Depth 6) + "`n"), $Utf8NoBom)
Write-Host ("`nexit={0} after {1:N0}s" -f $rc, $sw.Elapsed.TotalSeconds)

# ---- 5. commit ONLY this run's directory -----------------------------------
Step '5/6 committing (this directory only)'
$rel = "reports/composed/$runId"
& git -C $RepoRoot add -- $rel
$staged = (& git -C $RepoRoot diff --cached --name-only) -join "`n"
if ([string]::IsNullOrWhiteSpace($staged)) { Fail 'nothing was produced to commit' }
foreach ($line in ($staged -split "`n")) {
    if ($line -and -not $line.StartsWith($rel)) {
        Fail "refusing to commit: '$line' is outside $rel. This script must only ever add files in its own run directory."
    }
}
$verdict = if ($rc -eq 0) { 'completed' } else { "did NOT complete (exit $rc)" }
$msg = @"
Composed shell fit on $($env:COMPUTERNAME): $verdict

Ran the composed zhao_shell_top against the provisional device on a machine
with $([math]::Round($cs.TotalPhysicalMemory / 1GB, 1)) GB, because the primary development machine cannot hold it
(quartus_map committed 28.4 GB against 24 GB and thrashed).

Measured commit $short. Wall time $([math]::Round($sw.Elapsed.TotalSeconds)) s. Machine details, the full
transcript and the outcome are in $rel.

This commit adds files in that directory and touches nothing else, so it
cannot conflict with concurrent work.
"@
& git -C $RepoRoot commit -q -m $msg
if ($LASTEXITCODE -ne 0) { Fail 'commit failed' }
Write-Host (& git -C $RepoRoot log --oneline -1)

# ---- 6. push, rebasing our own single commit if the branch moved -----------
if ($NoPush) { Step '6/6 push skipped (-NoPush)'; exit 0 }
Step '6/6 pushing'
for ($try = 1; $try -le 5; $try++) {
    & git -C $RepoRoot push origin HEAD 2>&1 | Write-Host
    if ($LASTEXITCODE -eq 0) { Write-Host "`nPUSHED. $rel is on origin." -ForegroundColor Green; exit 0 }
    Write-Host "push rejected (attempt $try); fetching and rebasing this one commit on top" -ForegroundColor Yellow
    & git -C $RepoRoot fetch origin 2>&1 | Write-Host
    # The branch by name, not origin/HEAD: that symbolic ref is only set if
    # someone ran `git remote set-head`, and on a fresh clone it is often absent.
    $branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD).Trim()
    if ([string]::IsNullOrWhiteSpace($branch) -or $branch -eq 'HEAD') { $branch = 'main' }
    & git -C $RepoRoot rebase "origin/$branch" 2>&1 | Write-Host
    if ($LASTEXITCODE -ne 0) {
        & git -C $RepoRoot rebase --abort 2>&1 | Out-Null
        Fail 'rebase failed, which should be impossible for a commit that only adds new files. Stop and report this rather than forcing anything.'
    }
}
Fail 'could not push after 5 attempts. The commit is local; report that and leave it.'
