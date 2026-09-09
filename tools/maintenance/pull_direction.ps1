# pull_direction.ps1 -- fetch, report what arrived, and merge ONLY when it is safe.
#
# ---------------------------------------------------------------------------
# WHY THIS IS A SCRIPT AND NOT A LINE IN A CRON PROMPT
# ---------------------------------------------------------------------------
# Fabian, 2026-09-09: pull the repo every 30 minutes, and whenever files arrive
# that is owner direction -- read it, test it, and implement it if it holds up.
#
# The recipe lives here rather than in the scheduled prompt for the reason
# CLAUDE.md gives about briefs: a prompt that carries its own procedure drifts
# from the tree and cannot be corrected by editing the tree. The prompt names
# this file; this file is version controlled.
#
# ---------------------------------------------------------------------------
# THE HAZARD THIS EXISTS TO AVOID: pulling into a LIVE FIT
# ---------------------------------------------------------------------------
# A Quartus fit reads the WORKING TREE, not the commit it started from
# (QUARTUS_GOTCHAS 11). A `git pull` that rewrites a file inside a running fit's
# closure corrupts that fit silently -- it will finish and produce a row that
# describes neither the old sources nor the new ones. Island fits here run 1.5-4
# hours, so an unlucky 30-minute tick would throw away most of a session.
#
# So the merge is CONDITIONAL:
#
#   * nothing incoming            -> report and exit
#   * incoming, no fit running    -> fast-forward and report
#   * incoming touches fpga/rtl   -> DO NOT MERGE while quartus is alive. Report
#     while a fit is alive           that direction is waiting and why. The next
#                                    tick after the fit ends will take it.
#   * incoming touches nothing    -> safe to fast-forward even mid-fit; docs and
#     in fpga/rtl                    reports are in no fit's closure.
#
# Reporting instead of merging is the conservative side of that trade: a delayed
# instruction costs one 30-minute tick, and a corrupted fit costs hours.
#
# NEVER stashes, never merges non-fast-forward, never resets. If the tree is
# dirty or the branch has diverged it says so and stops -- resolving that is a
# judgement call, not a cron job's business.
$ErrorActionPreference = 'Continue'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

function Write-Section($t) { Write-Output ''; Write-Output "== $t" }

# ---- 0. state of the local tree -------------------------------------------
$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
$dirty  = & git status --porcelain
Write-Output "PULL_DIRECTION branch=$branch"

# ---- 1. fetch is always safe: it writes only to .git ----------------------
& git fetch --all --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Output 'FETCH FAILED -- offline or credentials. Nothing else attempted.'
    exit 0
}

$upstream = & git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>$null
if ([string]::IsNullOrWhiteSpace($upstream)) { $upstream = "origin/$branch" }
$upstream = $upstream.Trim()

# ---- 2. what is incoming? -------------------------------------------------
$incoming = & git log --oneline "HEAD..$upstream" 2>$null
$behind   = 0
if ($incoming) { $behind = ($incoming | Measure-Object -Line).Lines }

# Direction may also land on main rather than on this working branch.
$onMain = @()
& git rev-parse --verify --quiet origin/main > $null 2>&1
if ($?) {
    $onMain = & git log --oneline "HEAD..origin/main" 2>$null
}

if ($behind -eq 0 -and -not $onMain) {
    Write-Output "NO NEW DIRECTION. HEAD is level with $upstream."
    if ($dirty) { Write-Output "(local tree has uncommitted changes; not this job's business)" }
    exit 0
}

# ---- 3. name the files, and flag ADDED ones -------------------------------
# The owner's phrasing was "whenever you pull and there's files there" -- so a
# newly ADDED file is the signal, and it is called out separately from an edit.
Write-Section "INCOMING: $behind commit(s) on $upstream"
if ($incoming) { $incoming | ForEach-Object { Write-Output "   $_" } }
if ($onMain) {
    # DO NOT LIST THE COMMITS. First run printed 300+ lines of another lane's
    # creature work and buried the one thing that matters. `origin/main` carries
    # every lane, so what is wanted from it is DIRECTION-SHAPED FILES, not
    # history.
    $mainCount = ($onMain | Measure-Object -Line).Lines
    Write-Section "origin/main is $mainCount commit(s) ahead (not merged here -- other lanes live there too)"
    $mainFiles = & git diff --name-only "HEAD..origin/main" 2>$null
    $mainDir = @($mainFiles | Where-Object {
        $_ -match 'Agent please read|OWNER-DIRECTION|OWNER_DOCKET|please read|DIRECTION-'
    })
    if ($mainDir.Count -gt 0) {
        Write-Output "   DIRECTION-SHAPED FILES differing from HEAD:"
        $mainDir | ForEach-Object { Write-Output "      $_" }
        Write-Output '   Read them WITHOUT touching the working tree:'
        Write-Output '      git show origin/main:<path>'
    } else {
        Write-Output '   no direction-shaped filenames differ from HEAD'
    }
}

$changed = @()
if ($behind -gt 0) {
    $changed = & git diff --name-status "HEAD..$upstream"
}
$added = @($changed | Where-Object { $_ -match '^A' } | ForEach-Object { ($_ -split "`t")[1] })
$paths = @($changed | ForEach-Object { ($_ -split "`t")[-1] })

if ($added.Count -gt 0) {
    Write-Section "NEW FILES -- treat as owner direction"
    $added | ForEach-Object { Write-Output "   $_" }
}
$dirf = @($paths | Where-Object { $_ -match 'OWNER-DIRECTION|DIRECTION|INSTRUCT' })
if ($dirf.Count -gt 0) {
    Write-Section 'DIRECTION-SHAPED FILENAMES'
    $dirf | ForEach-Object { Write-Output "   $_" }
}

# ---- 4. is a fit alive, and does the incoming change touch RTL? -----------
$fit = Get-Process quartus* -ErrorAction SilentlyContinue

# ONLY FILES A FIT CAN ACTUALLY READ COUNT AS A HAZARD.
#
# The first version blocked on any path under `fpga/rtl/`, and that is wrong in
# the expensive direction: `fpga/rtl/texture/` is where the texture OWNER-DIRECTION
# markdown lives (CLAUDE.md puts durable direction beside the thing it governs,
# precisely so a run folder cannot orphan it). Blocking on a `.md` would have
# delayed owner instructions by up to four hours per fit -- which is the
# "instructions are not delivered until they are read" failure this repo has
# already paid for four times over, reintroduced by a safety check.
#
# `design/fit_targets.yml` lists only HDL sources, so the closure is HDL. A
# markdown file, a report, or a test cannot change what Quartus compiles.
$hazardExt = @('.sv', '.svh', '.v', '.vh', '.qsf', '.sdc', '.qip', '.tcl', '.mif', '.hex')
$rtl = @($paths | Where-Object {
    $p = $_
    ($p -like 'fpga/*') -and ($hazardExt | Where-Object { $p.ToLower().EndsWith($_) })
})

Write-Section 'MERGE DECISION'
if ($dirty) {
    Write-Output 'NOT MERGED: the local tree has uncommitted changes.'
    $dirty | Select-Object -First 10 | ForEach-Object { Write-Output "   $_" }
    Write-Output 'Commit or set them aside deliberately, then re-run this script.'
    exit 0
}
if ($fit -and $rtl.Count -gt 0) {
    Write-Output ("NOT MERGED: quartus is running (" + ($fit | ForEach-Object { $_.Name }) +
                  ") and " + $rtl.Count + " incoming file(s) are under fpga/rtl.")
    $rtl | ForEach-Object { Write-Output "   $_" }
    Write-Output 'A fit reads the WORKING TREE, so merging now would corrupt it'
    Write-Output '(QUARTUS_GOTCHAS 11). Direction is WAITING; the next tick after the'
    Write-Output 'fit ends will take it. Read the commits above meanwhile via'
    Write-Output "   git show <sha>:<path>"
    Write-Output 'which reads from .git and does not touch the working tree.'
    exit 0
}
if ($behind -eq 0) {
    Write-Output 'Nothing to merge on this branch (direction is on main only).'
    exit 0
}

& git merge --ff-only "$upstream"
if ($LASTEXITCODE -ne 0) {
    Write-Output 'NOT MERGED: fast-forward refused -- the branch has diverged.'
    Write-Output 'Resolving a divergence is a judgement call, not a cron job.'
    exit 0
}
Write-Output "MERGED $behind commit(s). Now at: $(& git log --oneline -1)"
if ($added.Count -gt 0) {
    Write-Output ''
    Write-Output ("ACTION REQUIRED: " + $added.Count +
                  ' new file(s) arrived. Read them, test the claims, implement what holds up.')
}
