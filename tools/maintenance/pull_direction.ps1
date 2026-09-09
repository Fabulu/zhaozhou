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

# -c core.autocrlf=true, OR THIS JOB CAN NEVER MERGE OWNER DIRECTION.
#
# MEASURED 2026-09-09. PowerShell resolves a bare `git` to whatever is first on
# PATH. On this machine that is c:\devkitPro\msys2\usr\bin\git.exe, whose system
# config carries no core.autocrlf -- the setting lives only in
# "C:/Program Files/Git/etc/gitconfig", which Git for Windows reads and the msys2
# build does not. Through the msys2 git this repository reports **1,200 modified
# files**, every one of them pure line-ending churn (STATUS.md: 6,191 insertions
# against 6,191 deletions on a 6,191-line file). Git Bash's /mingw64/bin/git
# reports ZERO on the same tree at the same instant.
#
# `$dirty` gates the merge at the MERGE DECISION section below. So under the
# msys2 git this script prints "NOT MERGED: the local tree has uncommitted
# changes", lists ten phantom files as evidence, and exits 0 -- forever. It looks
# exactly like a legitimate conservative refusal, which is why it would not have
# been questioned.
#
# That is precisely the failure this file's own header says it exists to prevent:
# "instructions are not delivered until they are read". A cron job that reports a
# plausible reason for not delivering them is the worst form of it.
#
# run_block_fit.ps1, run_block_map.ps1, run_composed_fit.ps1 and scan_rtl.py all
# carry this flag already, each added after the same discovery -- rtlCleanAtHead
# was false on all 42 block-fit rows and had never once been true. The lesson was
# learned three times over and never propagated to this file.
$dirty  = & git -c core.autocrlf=true status --porcelain
Write-Output "PULL_DIRECTION branch=$branch"

# ---- 1. fetch is always safe: it writes only to .git ----------------------
& git fetch --all --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Output 'FETCH FAILED -- offline or credentials. Nothing else attempted.'
    exit 0
}

# NEVER USE git's @{upstream} SYNTAX FROM POWERSHELL 5.1. IT STRIPS BRACES.
#
# MEASURED 2026-09-09, on the same run that found the core.autocrlf split. The
# native-argument parser removes { and } from every argument regardless of
# quoting:
#
#   '@{u}'          reaches git as  @u
#   '@{upstream}'   reaches git as  @upstream
#   "$b@{upstream}" reaches git as  zixxtrixx-v8-closeout@upstream
#
# All three are "ambiguous argument ... unknown revision". And the FIRST of them
# still prints `@u` on stdout while the fatal goes to stderr, so with `2>$null`
# this line assigned the literal string "@u" -- non-empty, so the
# `origin/$branch` fallback below never fired either.
#
# Everything downstream then queried `HEAD..@u`, which fails, with stderr
# suppressed. `$incoming` came back empty, `$behind` was 0, and the script
# reported "NO NEW DIRECTION. HEAD is level with @u."
#
# So THIS TOOL COULD NEVER SEE DIRECTION PUSHED TO ITS OWN BRANCH. It reported
# only origin/main's activity and looked like it was working. Second instance in
# one file of the same failure: a silent error producing a reassuring message.
#
# `for-each-ref` takes the ref as a plain path with no brace syntax, so it
# survives the parser. The config pair is the fallback and needs no braces either.
$upstream = (& git for-each-ref --format='%(upstream:short)' "refs/heads/$branch" |
             Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($upstream)) {
    $rem = & git config --get "branch.$branch.remote"
    $mrg = & git config --get "branch.$branch.merge"
    if ($rem -and $mrg) { $upstream = "$rem/" + ($mrg -replace '^refs/heads/', '') }
}
if ([string]::IsNullOrWhiteSpace($upstream)) { $upstream = "origin/$branch" }
$upstream = $upstream.Trim()

# AND REFUSE A GARBAGE UPSTREAM RATHER THAN REPORTING "0 INCOMING" FROM ONE.
# This is the check whose absence let "@u" through for the tool's whole life.
# No braces in the argument, for the reason above.
& git rev-parse --verify --quiet $upstream > $null 2>&1
if (-not $?) {
    Write-Output "ABORTING: '$upstream' is not a resolvable ref, so a count of"
    Write-Output "incoming commits from it would be a silent zero rather than an"
    Write-Output "answer. Fix the branch's upstream and re-run."
    exit 0
}

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
    # --diff-filter=AM, not a bare diff. A bare `git diff HEAD..origin/main`
    # also reports files DELETED relative to HEAD -- which for a file this
    # branch has and main does not means THIS LANE IS AHEAD, not behind. The
    # first run flagged all three texture OWNER-DIRECTION files that way, and a
    # false alarm repeating every 29 minutes for a week is how a reader learns
    # to skip the section that matters.
    $mainFiles = & git diff --name-only --diff-filter=AM "HEAD..origin/main" 2>$null
    # THE FILENAME FILTER IS THE UNRELIABLE HALF, AND IT MISSED A REAL ONE.
    #
    # 2026-09-09: `reports/OWNER-DOCUMENT-INDEX.md` is absent at HEAD and present
    # on main. It is an owner-document MANIFEST -- the index of all 33 "Agent
    # please read" files -- and this filter reported "no direction-shaped
    # filenames differ from HEAD", because it matched OWNER-DIRECTION and the
    # file says OWNER-DOCUMENT. One noun apart.
    #
    # It surfaced only through the SUBJECT scan below, and only by luck: the
    # commit subject is `Add an owner-document manifest: every "Agent please
    # read" file, chronologically`, so it matched on a QUOTED occurrence of the
    # phrase rather than on an actual request. A detector that works by accident
    # is not working.
    #
    # The index's own first standing rule is "owner documents land wherever they
    # land", and its second is "scan commit subjects -- several were never
    # announced anywhere else". So the pattern is widened here, and the subject
    # scan below is completed to NAME THE FILES rather than tell a reader to go
    # and look them up.
    $mainDir = @($mainFiles | Where-Object {
        $_ -match 'Agent please read|please read|OWNER[-_]|DIRECTION|DOCKET|INSTRUCT|ADVICE|rearchitect'
    })
    if ($mainDir.Count -gt 0) {
        Write-Output "   DIRECTION-SHAPED FILES differing from HEAD:"
        $mainDir | ForEach-Object { Write-Output "      $_" }
        Write-Output '   Read them WITHOUT touching the working tree:'
        Write-Output '      git show origin/main:<path>'
    } else {
        Write-Output '   no direction-shaped filenames differ from HEAD'
    }
    # Same subject-line scan on main. Capped, because main carries every lane and
    # 293 commits of creature work is what buried the signal on the first run.
    $mainAsk = & git log --format='%h %s' 'HEAD..origin/main' 2>$null |
        Where-Object { $_ -imatch 'agent,? please read|please read' }
    if ($mainAsk) {
        $n = ($mainAsk | Measure-Object).Count
        Write-Output "   $n commit subject(s) on main addressed to the agent (newest 8):"
        # NAME THE FILES, AND SAY WHICH ARE ABSENT HERE.
        #
        # The previous version printed the subjects and then told the reader to
        # run `git show --name-only` themselves. That is a recipe in a report,
        # which is the thing CLAUDE.md says a brief must never contain -- and in
        # practice nobody ran it, which is how OWNER-DOCUMENT-INDEX.md sat
        # unlisted while its commit was printed every 30 minutes.
        #
        # `git cat-file -e` against HEAD separates "already here" from "this lane
        # has never had it". Both read from .git; the working tree is untouched.
        foreach ($c in ($mainAsk | Select-Object -First 8)) {
            Write-Output "      $c"
            $sha = ($c -split ' ')[0]
            $files = & git show --name-only --format='' $sha 2>$null |
                Where-Object { $_ -and $_.Trim() }
            foreach ($f in $files) {
                & git cat-file -e "HEAD:$f" 2>$null
                $here = $?
                $mark = if ($here) { 'present here' } else { 'ABSENT HERE' }
                Write-Output ("           {0,-13} {1}" -f $mark, $f)
            }
        }
        Write-Output '   Read one without touching the tree:  git show origin/main:<path>'
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

# THE OWNER SIGNALS IN THE COMMIT SUBJECT, NOT ALWAYS IN THE FILENAME.
#
# Found by the positive control on the --diff-filter=AM change: `bumomapping.md`
# is owner direction for terrain bump mapping, and its filename says nothing --
# the signal is the commit subject, "Agent please read - terrain bump mapping".
# There are at least nine such commits in this repo's history and their files are
# named things like `bumomapping.md`, so a filename-only detector misses the
# owner's actual convention. Checking the subject line is the reliable half.
$askCommits = & git log --format='%h %s' "HEAD..$upstream" 2>$null |
    Where-Object { $_ -imatch 'agent,? please read|please read|owner direction|owner ask' }
if ($askCommits) {
    Write-Section 'COMMIT SUBJECTS THAT SAY THIS IS FOR THE AGENT'
    foreach ($c in $askCommits) {
        Write-Output "   $c"
        $sha = ($c -split ' ')[0]
        & git show --name-only --format='' $sha 2>$null |
            Where-Object { $_ -and $_.Trim() } |
            ForEach-Object { Write-Output "        $_" }
    }
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
