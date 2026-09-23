r"""A remote whose fetch refspec has lost its wildcard, which FREEZES every
remote-tracking ref for that remote and makes "local == remote" a dead reading.

WHAT HAPPENED, 2026-09-23
-------------------------
A push reported success --

    To https://github.com/Fabulu/zhaozhou.git
       a8d4443d..0c0aa5d9  claude/ceiling-architecture-20260912 -> ...

-- and immediately afterwards:

    git rev-parse origin/claude/ceiling-architecture-20260912
    f98f5846...          <- a commit from 2026-09-19

`git ls-remote` showed the real remote at the new commit, so nothing was lost.
The INSTRUMENT was broken, not the push. `remote.origin.fetch` had been reduced
to a single line --

    +refs/heads/zixxtrixx-v8-closeout:refs/remotes/origin/zixxtrixx-v8-closeout

-- with the default `+refs/heads/*:refs/remotes/origin/*` gone.

WHY IT MATTERS MORE THAN IT LOOKS
---------------------------------
Two consequences, and the second is the one that bites:

  * `git fetch <remote>` stops updating `refs/remotes/<remote>/*`;
  * SO DOES `git push`. A push updates a remote-tracking ref only if the
    remote's fetch refspec MAPS it. Nobody expects a push to stop maintaining
    the ref it just advanced.

So the tracking refs stand still at whatever they last held while the real
remote moves on. "local == remote" has been a closure check in this campaign,
and for some window it was comparing HEAD against a value that COULD NOT CHANGE.
It happened to read DISagreement, which is the only reason it was noticed; had
HEAD matched the frozen ref it would have read as a clean sync forever -- which
is the broken-instrument law's flattering direction exactly.

The config lives in the COMMON git dir, so all 96 lane worktrees shared the
fault and every one of them would have reported the same reassuring nonsense.

WHAT THIS GATE ASKS
-------------------
For every configured remote: is there a fetch refspec that maps `refs/heads/*`
into that remote's own `refs/remotes/<name>/*` namespace? A remote with no
refspec at all fails too -- `git fetch` still works there, and still maintains
nothing.

It deliberately does NOT go to the network. `git ls-remote` is the truth about
where a remote actually is, but a gate that needs the network is a gate people
learn to skip. This one answers the cheap structural question: CAN the cache be
maintained at all?

    python tools/maintenance/check_git_remote_refspec.py            # rc 1 on a finding
    python tools/maintenance/check_git_remote_refspec.py --fix      # restore the wildcard

NOTE WHAT THIS IS NOT, for the same reason `check_eol_worktree.py` says it:
it is a LOCAL hazard check and must never be registered as a CI gate. CI clones
fresh, so its `remote.origin.fetch` is the default wildcard by construction and
this check would pass forever there without ever having been able to fail. It
belongs in `gate_sweep.py`, which runs on the machine where the fault lives.

AND IT PROVES ITSELF AT IMPORT. A detector that has not been shown to fire has
not been tested (CLAUDE.md), so the classifier is run against the exact broken
refspec above and against three good shapes before any real config is read.
"""
from __future__ import annotations

import re
import subprocess
import sys


def _wildcard_dst(remote: str) -> str:
    return "refs/remotes/%s/*" % remote


def maps_all_heads(remote: str, refspecs: list[str]) -> bool:
    """Does any refspec bring every remote branch into this remote's cache?

    The destination must be the remote's OWN namespace: a refspec that fetched
    `refs/heads/*` into `refs/remotes/upstream/*` maintains somebody else's
    cache and leaves this one just as frozen.
    """
    want_dst = _wildcard_dst(remote)
    for spec in refspecs:
        body = spec[1:] if spec.startswith("+") else spec
        if ":" not in body:
            continue
        src, dst = body.split(":", 1)
        if src.strip() == "refs/heads/*" and dst.strip() == want_dst:
            return True
    return False


# ---------------------------------------------------------------------------
# positive and negative controls -- run at import, before any real config
# ---------------------------------------------------------------------------
_BAD_SINGLE = ["+refs/heads/zixxtrixx-v8-closeout:refs/remotes/origin/zixxtrixx-v8-closeout"]
_BAD_EMPTY: list[str] = []
_BAD_OTHER_NS = ["+refs/heads/*:refs/remotes/upstream/*"]
_GOOD_PLAIN = ["+refs/heads/*:refs/remotes/origin/*"]
_GOOD_NO_FORCE = ["refs/heads/*:refs/remotes/origin/*"]
_GOOD_PLUS_EXTRA = [
    "+refs/heads/*:refs/remotes/origin/*",
    "+refs/heads/zixxtrixx-v8-closeout:refs/remotes/origin/zixxtrixx-v8-closeout",
]

for _name, _specs, _want in (
    ("BAD_SINGLE", _BAD_SINGLE, False),
    ("BAD_EMPTY", _BAD_EMPTY, False),
    ("BAD_OTHER_NS", _BAD_OTHER_NS, False),
    ("GOOD_PLAIN", _GOOD_PLAIN, True),
    ("GOOD_NO_FORCE", _GOOD_NO_FORCE, True),
    ("GOOD_PLUS_EXTRA", _GOOD_PLUS_EXTRA, True),
):
    if maps_all_heads("origin", _specs) is not _want:
        raise SystemExit(
            "check_git_remote_refspec SELF-TEST FAILED on %s: the classifier no "
            "longer agrees with the case this gate was written from. Fix the "
            "classifier before trusting any verdict it prints." % _name
        )


def _git(*args: str) -> str:
    # `-c core.autocrlf=true` unconditionally: this helper builds its subcommand
    # at runtime, so no reader (and no gate) can tell from the call site whether
    # the output is content-dependent. `check_git_autocrlf_guard.py` fired on
    # this exact line the first time this file was swept, which is the gate
    # doing its job on a brand-new tool.
    return subprocess.run(["git", "-c", "core.autocrlf=true", *args],
                          capture_output=True, text=True).stdout


def remotes() -> list[str]:
    return [r for r in _git("remote").split() if r]


def refspecs(remote: str) -> list[str]:
    return [l.strip() for l in _git("config", "--get-all",
                                   "remote.%s.fetch" % remote).splitlines() if l.strip()]


def tracking_count(remote: str) -> int:
    out = _git("for-each-ref", "refs/remotes/%s" % remote, "--format=%(refname)")
    return len([l for l in out.splitlines() if l.strip()])


def main(argv: list[str]) -> int:
    fix = "--fix" in argv
    names = remotes()
    if not names:
        print("no git remotes configured -- nothing to check.")
        return 0

    bad = []
    for r in names:
        specs = refspecs(r)
        if not maps_all_heads(r, specs):
            bad.append((r, specs))

    for r, specs in bad:
        print("FAIL: remote '%s' has no refspec mapping refs/heads/* into %s"
              % (r, _wildcard_dst(r)))
        if specs:
            for s in specs:
                print("        configured: %s" % s)
        else:
            print("        configured: (none at all)")
        print("      CONSEQUENCE: neither `git fetch` nor `git push` will update")
        print("      refs/remotes/%s/* . Those %d cached refs are FROZEN, and any"
              % (r, tracking_count(r)))
        print("      'local == remote' comparison against them means nothing.")
        print("      `git ls-remote %s` is the only truth while this holds." % r)
        if fix:
            subprocess.run(["git", "config", "--add", "remote.%s.fetch" % r,
                            "+refs/heads/*:%s" % _wildcard_dst(r)], check=True)
            print("      --fix: wildcard refspec ADDED. Run `git fetch %s` now." % r)
        else:
            print("      REPAIR: git config --add remote.%s.fetch "
                  "'+refs/heads/*:%s'" % (r, _wildcard_dst(r)))
            print("      then `git fetch %s`, then re-read the tracking ref." % r)

    if bad:
        return 0 if fix else 1

    print("git remote refspec check: %d remote(s), every one maps refs/heads/* "
          "into its own cache." % len(names))
    for r in names:
        print("    %-12s %d tracking ref(s) maintained" % (r, tracking_count(r)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
