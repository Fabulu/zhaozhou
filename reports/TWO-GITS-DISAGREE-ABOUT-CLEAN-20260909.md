# Two git binaries disagree about whether this tree is clean, and the owner's direction job was gated on the wrong one

2026-09-09. Two defects, found in one file, both in the same failure mode: a
silent error that produces a **reassuring** message. Neither would have been
caught by any gate in the repository, because both made the tool report *less*
to worry about.

The file is `tools/maintenance/pull_direction.ps1` — the job Fabian asked for on
2026-09-09: *"set up a job to pull repo every 30 minutes. Whenever you pull and
there's files there it means you're getting instructions."*

---

## Defect 1: `core.autocrlf` is not where you think it is

There are two git binaries on this machine and they do not agree:

| binary | version | dirty files on this tree |
|---|---|---|
| `/mingw64/bin/git` (Git for Windows) | 2.45.2.windows.1 | **0** |
| `c:/devkitPro/msys2/usr/bin/git.exe` | 2.49.0 | **1,200** |

Same tree, same instant. `core.autocrlf=true` is set in
`C:/Program Files/Git/etc/gitconfig` — the **system** config of Git for Windows.
The msys2 build reads a different system config and has the setting unset, so
every worktree file with CRLF endings and no `text` attribute in
`.gitattributes` compares unequal to its LF blob. Every line of it:

```
STATUS.md | 12382 ++++++++++++++++++++++++++++++------------------------------
1 file changed, 6191 insertions(+), 6191 deletions(-)
```

6,191 insertions against 6,191 deletions on a 6,191-line file is pure
line-ending churn.

**Which binary you get depends on PATH.** Git Bash gives the good one. PowerShell
on this machine gives the msys2 one. So the same script answers differently
depending on the shell that launched it — and the scheduled jobs are PowerShell.

### What it did to the direction job

```powershell
$dirty  = & git status --porcelain          # 1,200 lines under the msys2 git
...
if ($dirty) {
    Write-Output 'NOT MERGED: the local tree has uncommitted changes.'
    $dirty | Select-Object -First 10 | ForEach-Object { Write-Output "   $_" }
    exit 0
}
```

Under the msys2 git that gate **can never open**. The job would print a
conservative refusal, list ten phantom files as supporting evidence, and exit 0
forever. It looks exactly like a legitimate safety check doing its work, which is
why nobody would have questioned it.

That is the failure this repository has already paid for four times over and
which `CLAUDE.md` records as *"instructions are not delivered until they are
read"* — reintroduced by a PATH accident, in the very tool built to prevent it.

### It is INTERMITTENT, which is worse than broken

git keeps a stat cache in the index. Once any `status` runs under
`autocrlf=true` the index is refreshed, and both binaries then report clean —
until something moves an mtime. My first attempt at a positive control was
contaminated by exactly this: having run the good git once, the bad one reported
0 and I nearly recorded that as evidence the bare form was safe. **The
comfortable answer arrived first**, on schedule.

The honest control invalidates the stat cache on a **byte-identical** file:

```
content unchanged by touch: YES  (sha256 7e16d7bda49e1160, before == after)
OLD  bare                      : [ M STATUS.md]
NEW  -c core.autocrlf=true     : []
```

### The lesson was already learned three times and never propagated

`run_block_fit.ps1`, `run_block_map.ps1`, `run_composed_fit.ps1` and
`scan_rtl.py` all carry `-c core.autocrlf=true` already. `run_block_fit.ps1`'s
own comment records the cost:

> **THE COST OF NOT HAVING THIS:** all 42 rows in `zhao_block_fit.json` carried
> `rtlCleanAtHead:false`. The flag had NEVER once been true, so a field meant to
> say whether a measurement can be trusted against its commit was answering the
> same way regardless — which is indistinguishable from not having it.

Four files fixed one at a time, and the fifth — the one carrying owner
instructions — never got it. This is the `.gitignore`-versus-purge pattern from
`CLAUDE.md`: **a lesson applied where it was learned and nowhere else is
half-fixed.**

---

## Defect 2: PowerShell 5.1 strips braces, so `@{upstream}` cannot work

Found on the same run, one line below the first.

```powershell
$upstream = & git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>$null
```

PowerShell 5.1's native-argument parser removes `{` and `}` from every argument
**regardless of quoting**:

| written | reaches git as | git's answer |
|---|---|---|
| `'@{u}'` | `@u` | ambiguous argument |
| `'@{upstream}'` | `@upstream` | ambiguous argument |
| `"$branch@{upstream}"` | `zixxtrixx-v8-closeout@upstream` | ambiguous argument |

The first one **still prints `@u` on stdout** while the fatal goes to stderr. With
`2>$null` the error vanished and `$upstream` was assigned the literal string
`"@u"` — non-empty, so the `origin/$branch` fallback on the next line never
fired either. Every downstream query then ran against `HEAD..@u`:

```
upstream=@u                             resolvable=False  incoming=0
upstream=origin/zixxtrixx-v8-closeout   resolvable=True   incoming=0
upstream=origin/main                    resolvable=True   incoming=302
```

**The tool could never see direction pushed to its own branch.** It reported only
`origin/main`'s activity and printed *"NO NEW DIRECTION. HEAD is level with @u"*
— a sentence naming the broken ref, in plain sight, for the tool's whole life.

The third row is the positive control that matters: the query mechanism counts
correctly when handed a resolvable ref, so the zero was the ref's fault and not
the query's.

### The repair

`for-each-ref` takes a plain ref path with no brace syntax, so it survives the
parser; the `branch.<b>.remote` / `branch.<b>.merge` config pair is the fallback
and needs no braces either. And the resolved value is now **verified to be a
ref** before anything is counted from it:

```powershell
& git rev-parse --verify --quiet $upstream > $null 2>&1
if (-not $?) { ...refuse rather than report a silent zero... }
```

That check is what was missing. A count of incoming commits from an unresolvable
ref is not a zero, it is an absence of measurement, and the tool was reporting
the two as the same thing.

---

## Fixed, and what carries the rule forward

Every content-dependent call site in `tools/` and `.github/` now pins the
setting:

| file | site |
|---|---|
| `tools/maintenance/pull_direction.ps1` | `status --porcelain` (the merge gate) |
| `tools/maintenance/mutation_sweep.py` | the `git()` helper — covers its `status` gate **and** its `checkout --` restore |
| `tools/quartus/packet_accounting.py` | the `git(*args)` helper |
| `tools/git_add_safe.py` | `git add` |
| `tools/quartus/run_composed_fit.ps1` | `git add` of the run directory |
| `tools/sweep_field_plan.sh` | `git checkout --` of the golden captures |

The two `git add` sites are worse in kind than a wrong report: an unguarded
`add` through the msys2 git stores the worktree CRLF **in the blob** for every
file without a `text` attribute, so the commit carries real churn rather than a
phantom one.

`core.autocrlf=true` is also now set in this clone's `.git/config`, which every
binary reads. That fixes this machine for call sites nobody has found yet — but
`.git/config` is not version controlled, so a fresh clone regresses, and it is
belt-and-braces rather than the fix.

### The checker

`tools/maintenance/check_git_autocrlf_guard.py` refuses an unguarded
content-dependent invocation. It classifies only subcommands that compare or
copy between the worktree and the object store — `status add checkout restore
stash ls-files` always, `diff` unless given an `A..B` range — because
`rev-parse`, `log`, `show` and a ranged `diff` read blobs on both sides and
flagging them would be the cry-wolf failure that gets a tool suppressed.

**Its own self-test asserts at import that it FIRES**, on seven line shapes taken
verbatim from this tree's defects, and that it stays quiet on twenty guarded or
immune ones. Per `CLAUDE.md`: a detector that has not been shown to fire has not
been tested.

Writing it produced three lessons of its own:

1. **It flagged fourteen sites and ten were false positives** — its own test
   fixtures, calls through a helper that is guarded in its body, and `git diff
   --stat` printed as *advice* inside an `echo`. A checker shipped in that state
   would have been ignored within a day.
2. **`\b` became a literal 0x08 backspace again.** `_HELPER_CALL` was written as
   `re.compile(r"\bgit\s*\(")` and reached the file as `r"<0x08>git\s*\("`, a
   regex that can never match — so the helper-call exclusion was silently dead.
   This is the seventh time the heredoc has eaten a backslash escape here, and
   the repository's own `tools/maintenance/no_control_bytes.py` is what caught
   it. The `_HELPER_CALL` fix went in through a byte-exact edit, not a shell.
3. **The first version was blind in the comfortable direction.**
   `packet_accounting.py` defines `git(*args)`, so its call sites were excluded
   as helper calls while its body — where the subcommand is only known at
   runtime — was unclassifiable. The whole file was invisible. A helper that
   could run any subcommand must be guarded unconditionally, and there is now a
   rule and a control for that. It is narrowed to an **unresolvable** subcommand:
   `check_array_storage.py` pins `git log` and splices only trailing paths, which
   cannot turn a blob-side read into a worktree one.
4. **It reported "clean" over a file it had never opened.** The scan matched
   `.ps1 .py .sh`, and `tools/githooks/pre-commit` — a pre-commit hook whose
   entire job is inspecting staged content — has no extension. Its two calls
   turned out to be immune (`diff --cached`, `cat-file`), so nothing was broken;
   the blindness was. It now also scans any file beginning `#!`, and **prints
   the number of files it looked at beside the verdict**, because "no findings"
   over an unnamed set is the silent-drop pattern rather than a result. Verified
   by listing the scanned set and finding the hook in it — 171 files,
   extensions `.ps1 .py .sh` and extensionless — not by trusting the count.

---

## What this does not establish

**Not that every git call in the repository is now correct.** The checker covers
171 files under `tools/` and `.github/` — `.ps1 .py .sh` plus any extensionless
file beginning `#!`, which is how `tools/githooks/pre-commit` is reached. A call
in a `CMakeLists.txt`, a `package.json` script, a `.qsf`/`.tcl`, or a C++ source
is **outside** it and unchecked.

**Not that any existing measurement is invalidated.** The four fit and scan
tools were already guarded, so the `rtlCleanAtHead` fields in the current ledgers
were computed correctly. The rows predating those fixes are the ones the
`run_block_fit.ps1` comment already documents.

**Not a texture result, and not authorised work beyond §0.1.** This is a repair
to the instrument that delivers owner direction, made while the reduction
programme stays deferred. It touches no HDL, no fit closure, and no acceptance
criterion.
