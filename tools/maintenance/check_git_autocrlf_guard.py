"""Refuse a git invocation whose answer depends on WORKTREE CONTENT and which
does not pin core.autocrlf.

---------------------------------------------------------------------------
WHY THIS EXISTS
---------------------------------------------------------------------------
Measured 2026-09-09. There are two git binaries on this machine and they
disagree about whether the tree is clean:

    /mingw64/bin/git                        (Git for Windows 2.45.2)  ->    0 dirty
    c:/devkitPro/msys2/usr/bin/git.exe      (msys2 git 2.49.0)        -> 1200 dirty

`core.autocrlf=true` is set in "C:/Program Files/Git/etc/gitconfig", the SYSTEM
config of Git for Windows. The msys2 build reads a different system config and
therefore has it unset, so every worktree file with CRLF endings and no `text`
attribute in .gitattributes reads as modified -- every line of it. STATUS.md
came back as 6,191 insertions against 6,191 deletions on a 6,191-line file.

Which binary you get depends on PATH: Git Bash gives the good one, PowerShell on
this machine gives the msys2 one. So the same script answers differently
depending on the shell that launched it.

THE COST ALREADY PAID: all 42 rows in zhao_block_fit.json carried
`rtlCleanAtHead: false`. That field exists to say whether a 1.5-4 hour
measurement can be trusted against its commit, and it had never once been true,
which is indistinguishable from not having the field at all.

THE COST NEARLY PAID: tools/maintenance/pull_direction.ps1 -- the job the owner
asked for, to pull direction every 30 minutes -- gated its merge on a bare
`git status --porcelain`. Under the msys2 git that gate can never open. It would
have printed "NOT MERGED: the local tree has uncommitted changes", listed ten
phantom files as evidence, and exited 0 forever, which looks exactly like a
legitimate conservative refusal. That is the "instructions are not delivered
until they are read" failure this repository has already paid for four times.

AND IT IS INTERMITTENT, WHICH IS WORSE. git keeps a stat cache in the index. Once
any `status` runs under autocrlf=true the index is refreshed and BOTH binaries
then report clean -- until something moves an mtime. So the bug hides after the
first correct run and returns after a checkout, a rebuild, or a touch. Proving it
requires invalidating the stat cache; see the fire test at the bottom of this
file, which does exactly that on a byte-identical file.

---------------------------------------------------------------------------
WHAT COUNTS AS CONTENT-DEPENDENT
---------------------------------------------------------------------------
Only subcommands that compare or copy between the WORKTREE and the object store:

    status  add  checkout  restore  stash  ls-files      always
    diff                                                 unless given a  A..B  range

`rev-parse`, `log`, `show`, `merge-base`, `fetch`, `rev-list`, `cat-file` and a
`diff A..B` read blobs on both sides, so normalisation cannot change the answer.
Flagging those would be noise, and a checker that cries wolf gets suppressed.

Run:  python tools/maintenance/check_git_autocrlf_guard.py
Exit: 0 clean, 1 unguarded call sites found, 2 self-test failure.
"""

from __future__ import annotations

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Subcommands whose answer depends on worktree content.
ALWAYS_CONTENT = {
    "status", "add", "checkout", "restore", "stash", "ls-files",
}
# `diff` only when it is not comparing two committed things.
RANGE_CONTENT = {"diff"}

SCAN_EXT = (".ps1", ".py", ".sh")
SCAN_DIRS = ("tools", ".github")

# A token that is an option: -C, -c, --git-dir=..., etc.
_OPT = re.compile(r"^-")


def _tokenize(line: str) -> list[str]:
    """Flatten shell and python-list call forms into bare tokens.

    `subprocess.run(["git", "-C", REPO, "-c", "core.autocrlf=true", "status"])`
    and `& git -C $RepoRoot -c core.autocrlf=true status --porcelain`
    both become  [..., 'git', '-C', ..., '-c', 'core.autocrlf=true', 'status', ...]
    """
    t = line.replace("[", " ").replace("]", " ").replace("(", " ").replace(")", " ")
    t = t.replace(",", " ").replace("'", " ").replace('"', " ")
    return t.split()


# A git token immediately followed by "(" is a call to a PROJECT HELPER named
# git(), not the binary. mutation_sweep.py and packet_accounting.py both define
# one. The guard belongs in the helper body -- flagging every call site through
# it would report a dozen findings for one fix and train the reader to ignore
# the tool.
_HELPER_CALL = re.compile(r"\bgit\s*\(")

# The git token inside a message is advice to a human, not an invocation. Three
# real lines in this tree print `git diff --stat fpga/rtl/field/` as a hint.
_EMITTERS = (
    "echo", "printf", "print(", ".write(", "sys.stderr", "sys.stdout",
    "write-output", "write-host", "write-error", "write-warning",
)


# A HELPER THAT TAKES ITS SUBCOMMAND AT RUNTIME could run any of them, so it
# must carry the flag unconditionally -- and the scanner cannot classify it by
# reading the line. packet_accounting.py's `git(*args)` was exactly this: its
# call sites are excluded as helper calls (above) and its own body was
# unclassifiable, so the whole file was invisible to this check in the
# comfortable direction. Found 2026-09-09 while writing this file.
_DYNAMIC = ("*args", "list(args)", "*argv", "list(argv)", "@args", "$args", "$argv")
# A real subcommand name: lowercase letters and dashes, nothing else. `+`,
# `list` and `*args` are what a dynamic splice leaves behind instead.
_LITERAL_SUB = re.compile(r"^[a-z][a-z-]*$")


def _dynamic(line: str) -> bool:
    low = line.lower()
    return any(m in low for m in _DYNAMIC)


def _emitted(line: str, upto: int) -> bool:
    """True if the git token at `upto` sits inside something being printed."""
    head = line[:upto].lower()
    return any(e in head for e in _EMITTERS)


def _is_comment(path: str, stripped: str) -> bool:
    if stripped.startswith("#"):
        return True
    if path.endswith(".py") and (stripped.startswith('"""') or stripped.startswith("*")):
        return True
    return False


def scan_line(path: str, line: str) -> list[tuple[str, str]]:
    """Return [(subcommand, reason)] for unguarded content-dependent calls."""
    stripped = line.strip()
    if _is_comment(path, stripped):
        return []

    # A helper call, e.g. `rc, out = git('status', '--porcelain')`. The helper's
    # own subprocess line is a separate call site and IS checked.
    if _HELPER_CALL.search(line) and "subprocess" not in line:
        return []

    toks = _tokenize(line)
    out: list[tuple[str, str]] = []

    for i, tok in enumerate(toks):
        base = tok.lower()
        if not (base == "git" or base.endswith("/git") or base.endswith("git.exe")):
            continue
        # advice printed to a human is not an invocation
        pos = line.lower().find(base)
        if pos >= 0 and _emitted(line, pos):
            continue

        # Walk forward past options to the subcommand, noting the guard.
        guarded = False
        sub = None
        j = i + 1
        while j < len(toks):
            tj = toks[j]
            if "core.autocrlf" in tj:
                guarded = True
                j += 1
                continue
            if _OPT.match(tj):
                # -C and -c take a value; skip it so a repo path is not read
                # as the subcommand.
                if tj in ("-C", "-c") and j + 1 < len(toks):
                    if "core.autocrlf" in toks[j + 1]:
                        guarded = True
                    j += 2
                    continue
                j += 1
                continue
            sub = tj
            break
        if sub is None:
            continue

        sub = sub.lower()
        # The dynamic rule fires only when the SUBCOMMAND ITSELF could not be
        # resolved to a literal. check_array_storage.py runs
        #   ["git", "log", "-1", "--format=%cI"] + list(args)
        # -- the subcommand is pinned to `log` and only the trailing PATHS are
        # dynamic, which cannot make a blob-side read into a worktree one.
        # Flagging that would be the cry-wolf failure this file's header warns
        # about, so `log` falls through to ordinary classification below.
        if _dynamic(line) and not _LITERAL_SUB.match(sub):
            if not guarded:
                out.append(("<runtime>", "helper builds its subcommand at "
                                         "runtime; guard unconditionally"))
            continue
        if sub in ALWAYS_CONTENT:
            if not guarded:
                out.append((sub, "content-dependent subcommand, core.autocrlf not pinned"))
        elif sub in RANGE_CONTENT:
            rest = " ".join(toks[j:])
            has_range = ".." in rest or "--cached" in rest or "--staged" in rest
            if not has_range and not guarded:
                out.append((sub, "worktree diff (no A..B range), core.autocrlf not pinned"))
    return out


def _shebanged(full: str) -> bool:
    """An EXTENSIONLESS executable script still holds git call sites.

    tools/githooks/pre-commit has no extension and was invisible to the first
    version of this scanner -- which then reported "clean" over a file it had
    never opened. That is the silent-drop pattern: a checker whose coverage is
    narrower than its claim.
    """
    try:
        with open(full, "rb") as fh:
            return fh.read(2) == b"#!"
    except OSError:
        return False


def scan_repo() -> tuple[list[tuple[str, int, str, str]], int]:
    findings = []
    scanned = 0
    for d in SCAN_DIRS:
        root = os.path.join(REPO, d)
        if not os.path.isdir(root):
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [x for x in dirnames
                           if x not in ("__pycache__", "node_modules", ".git")]
            for fn in filenames:
                full = os.path.join(dirpath, fn)
                if not fn.endswith(SCAN_EXT) and not _shebanged(full):
                    continue
                rel = os.path.relpath(full, REPO).replace(os.sep, "/")
                # This file's _MUST_FLAG list is deliberately full of unguarded
                # call sites -- they are the positive controls. Scanning them
                # would make the checker permanently fail on itself.
                if os.path.abspath(full) == os.path.abspath(__file__):
                    continue
                scanned += 1
                try:
                    with open(full, "r", encoding="utf-8", errors="replace") as fh:
                        lines = fh.readlines()
                except OSError:
                    continue
                for n, line in enumerate(lines, 1):
                    for sub, why in scan_line(rel, line):
                        findings.append((rel, n, sub, why))
    return findings, scanned


# ---------------------------------------------------------------------------
# SELF-TEST AT IMPORT. A detector that has not been shown to FIRE has not been
# tested (CLAUDE.md). Every one of these is a real line shape from this tree.
# ---------------------------------------------------------------------------
_MUST_FLAG = [
    # the pull_direction.ps1 defect, verbatim as it was
    '$dirty  = & git status --porcelain',
    # the git_add_safe.py defect, verbatim as it was
    '    r = subprocess.run(["git", "add"] + argv[1:], cwd=ROOT)',
    # the mutation_sweep.py restore, in the form its helper actually runs it.
    # (The `git('checkout', ...)` CALL SITE is deliberately NOT a positive
    # control -- it goes through the helper, and the helper body below is the
    # thing that has to carry the flag. It appears in _MUST_NOT_FLAG instead.)
    "        r = subprocess.run(['git', '-C', REPO, 'checkout', '--', f])",
    # a worktree diff with no range
    '$d = & git diff --name-only',
    # a helper whose subcommand is only known at runtime (packet_accounting.py,
    # verbatim as it was before 2026-09-09)
    '    return subprocess.check_output(["git"] + list(args), cwd=ROOT).decode(',
    "        r = subprocess.run(['git', '-C', REPO, *args], capture_output=True,",
    # the binary named by full path still counts
    '& c:\\devkitPro\\msys2\\usr\\bin\\git.exe status --porcelain',
]
_MUST_NOT_FLAG = [
    # the guarded forms now in the tree
    '$dirty  = & git -c core.autocrlf=true status --porcelain',
    '$dirtyRtl = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain -- fpga/rtl)',
    '    r = subprocess.run(["git", "-c", "core.autocrlf=true", "add"] + argv[1:])',
    # the same two helpers once guarded
    '    return subprocess.check_output(["git", "-c", "core.autocrlf=true"] + list(args), cwd=ROOT).decode(',
    "        r = subprocess.run(['git', '-C', REPO, '-c', 'core.autocrlf=true', *args],",
    # subcommand PINNED to a blob-side read, only the trailing paths dynamic
    '    r = subprocess.run(["git", "log", "-1", "--format=%cI"] + list(args),',
    # tools/githooks/pre-commit, extensionless -- both of its calls are immune
    'staged=$(git diff --cached --name-only --diff-filter=AM)',
    '    sz=$(git cat-file -s "$(git rev-parse ":$f" 2>/dev/null)" 2>/dev/null || echo 0)',
    # blob-vs-blob: normalisation cannot change these
    '$head = (& git -C $RepoRoot rev-parse HEAD).Trim()',
    '$changed = & git diff --name-status "HEAD..$upstream"',
    '$staged = (& git -C $RepoRoot diff --cached --name-only)',
    'out = subprocess.run(["git", "show", "-s", "--format=%ct", sha])',
    '& git fetch --all --quiet',
    # a comment mentioning the hazard is not a call site
    '#   * -c core.autocrlf=true on the git status, or every CRLF file reads dirty',
    '# A bare `git diff HEAD..origin/main` also reports deletions',
    # a call through a project helper named git(): the helper body is the site
    "        rc, out = git('status', '--porcelain', '--', a.file)",
    "        rc, out = git('checkout', '--', a.file)",
    '    diff = git("diff", "--unified=0", before, after, "--", *paths)',
    # advice printed to a human is not an invocation
    '''        echo "         git diff --stat fpga/rtl/field/"''',
    '''    sys.stderr.write('         git diff --stat fpga/rtl/field/\n')''',
    '''            print('*** Run: git checkout -- %s ***' % a.file)''',
    '''    echo "RESTORE FAILED: $FILES may still be mutated -- git checkout them" >&2''',
]


def _self_test() -> None:
    bad = []
    for s in _MUST_FLAG:
        if not scan_line("probe.ps1", s):
            bad.append("FAILED TO FIRE on: %s" % s.strip())
    for s in _MUST_NOT_FLAG:
        hits = scan_line("probe.ps1", s)
        if hits:
            bad.append("FALSE POSITIVE %r on: %s" % (hits, s.strip()))
    if bad:
        sys.stderr.write("check_git_autocrlf_guard SELF-TEST FAILED:\n")
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        raise SystemExit(2)


_self_test()


def main(argv: list[str]) -> int:
    findings, scanned = scan_repo()
    print("check_git_autocrlf_guard: self-test %d fire / %d no-fire cases PASSED"
          % (len(_MUST_FLAG), len(_MUST_NOT_FLAG)))
    # STATE THE COVERAGE ALONGSIDE THE VERDICT. "clean" over a set nobody named
    # is how the extensionless pre-commit hook went unscanned in silence.
    print("scanned %d file(s) under %s (%s, plus any file starting #!)"
          % (scanned, ", ".join(SCAN_DIRS), " ".join(SCAN_EXT)))
    if not findings:
        print("no unguarded content-dependent git call sites")
        return 0
    print("\n%d UNGUARDED content-dependent git call site(s):\n" % len(findings))
    for rel, n, sub, why in findings:
        print("  %s:%d" % (rel, n))
        print("      git %s  --  %s" % (sub, why))
    print("\nAdd  -c core.autocrlf=true  to the invocation. See this file's header")
    print("and reports/TWO-GITS-DISAGREE-ABOUT-CLEAN-20260909.md for why.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
