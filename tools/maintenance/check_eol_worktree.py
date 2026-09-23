r"""A file pinned `eol=lf` whose WORKING COPY is CRLF, which reads as a hash bug.

`.gitattributes` governs what happens at CHECKOUT. It does not rewrite a copy
that is already on disk. So when a pin is added to a file that already exists in
somebody's tree, that tree keeps the CRLF copy indefinitely, `git status` stays
clean -- because the INDEX is LF and git compares normalised content -- and
everything looks fine until a tool hashes the bytes.

WHAT IT LOOKS LIKE WHEN IT BITES, measured 2026-09-20 (owner ruling R204):

    packet_i_g8b_registration_static
      : manifest hash does not describe fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv

    G8B generator input is not checkout-stable LF text: ...zhao_terrain_patch_law_pkg.sv

The content was PERFECT. The generator reproduced its output byte-identically
the moment the working copy was re-materialised. The gate hashes the working
tree, so a line-ending difference in a correct file reads as a content
mismatch -- and the obvious next move, editing the file to "fix" it, is exactly
wrong.

The diagnosis is one command and it is the thing to run first:

    git ls-files --eol <path>
    i/lf    w/crlf   attr/text eol=lf     <- index LF, WORKING COPY CRLF

THE FIX IS TO RE-MATERIALISE, NOT TO EDIT:

    rm <path> && git checkout -- <path>

which is safe ONLY when the file has no uncommitted changes -- `git checkout --`
discards unstaged work and unstaged work has no reflog (CLAUDE.md). This script
refuses to name a file that is dirty, for that reason.

A sweep on 2026-09-20 found SEVEN such files beyond the one that bit, including
`tools/quartus/run_block_fit.ps1` and `design/shell_fit_ports.yml` -- both of
which feed the fit. Discovering this DURING a 1.5-4 hour fit is the expensive
place to discover it.

NOTE WHAT THIS IS NOT. It is not a CI gate and must never be registered as one:
CI checks out fresh, so every working copy there is correct by construction and
this check would pass forever without ever having been able to fail. It is a
LOCAL hazard check -- run it when a hash or provenance gate goes red on a file
you did not edit.
"""
from __future__ import annotations

import subprocess
import sys


def mismatches():
    """[(path, index_eol, worktree_eol)] for eol=lf pins whose worktree is not LF."""
    out = subprocess.run(["git", "-c", "core.autocrlf=true", "ls-files", "--eol"],
                         capture_output=True, text=True, check=True).stdout
    bad = []
    for line in out.splitlines():
        if "attr/text eol=lf" not in line:
            continue
        parts = line.split()
        if len(parts) < 3:
            continue
        idx, work = parts[0], parts[1]
        # i/none and i/-text are empty or explicitly non-text files: the pin
        # does not apply to their bytes and a "mismatch" there is noise.
        if idx in ("i/none", "i/-text"):
            continue
        if work != "w/lf":
            bad.append((line.split("\t")[-1].strip(), idx, work))
    return bad


def dirty(paths):
    if not paths:
        return set()
    out = subprocess.run(["git", "-c", "core.autocrlf=true", "status", "--porcelain", "--"] + paths,
                         capture_output=True, text=True, check=True).stdout
    return {l[3:].strip() for l in out.splitlines() if l.strip()}


def main():
    bad = mismatches()
    if not bad:
        print("eol worktree check: every `eol=lf` file has an LF working copy.")
        return 0

    paths = [p for p, _, _ in bad]
    unsafe = dirty(paths)

    print("eol worktree check: %d file(s) pinned `eol=lf` with a non-LF "
          "working copy.\n" % len(bad))
    print("These will fail any gate that hashes the WORKING TREE, while their")
    print("committed content is correct. Do NOT edit them.\n")
    for p, idx, work in bad:
        flag = "  ** DIRTY -- DO NOT re-materialise, you would lose work **" \
            if p in unsafe else ""
        print("  %-62s %s %s%s" % (p, idx, work, flag))

    clean = [p for p in paths if p not in unsafe]
    if clean:
        print("\nTo fix the clean ones (re-materialise, never edit):\n")
        print("  rm %s \\\n    && git checkout -- %s"
              % (" ".join(clean[:2]) + (" ..." if len(clean) > 2 else ""),
                 " ".join(clean[:2]) + (" ..." if len(clean) > 2 else "")))
    if unsafe:
        print("\n%d of them have UNCOMMITTED CHANGES and are NOT listed for "
              "re-materialisation:" % len(unsafe))
        for p in sorted(unsafe):
            print("  %s" % p)
        print("Commit or stash those first. `git checkout --` discards "
              "unstaged work and unstaged work has no reflog.")
    return 1


# SELF-CHECK. A parser that matched nothing would print the clean-sheet message
# forever, which is this repository's most repeated failure. Prove the field
# split still bites on the exact shape `git ls-files --eol` emits.
_PROBE = "i/lf\tw/crlf\tattr/text eol=lf\t\tpath/with space.sv"
assert _PROBE.split()[0] == "i/lf" and _PROBE.split()[1] == "w/crlf"
assert _PROBE.split("\t")[-1].strip() == "path/with space.sv"
assert "attr/text eol=lf" in _PROBE

if __name__ == "__main__":
    sys.exit(main())
