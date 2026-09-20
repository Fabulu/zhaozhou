#!/usr/bin/env python3
"""Find committed mutants that NOTHING RUNS.

Owner ruling R93 (2026-09-20): `tests/mutants/zhao_terrain_bake_v2_mutant.sv`
was grepped for its own module name and returned ONE hit -- the file itself.
Nothing in `tests/CMakeLists.txt` elaborates it, no directed test defines it in,
no control asserts it fires. Its header is exemplary; none of it has ever been
executed.

    "This is CLAUDE.md's 'BUILT, INSTALLED NOWHERE' applied to a positive
    control, and it is the worse variant: an uninstantiated MODULE costs the
    fitter nothing, but an unrun MUTANT is cited as evidence. ... The rule
    generalises: mutant_copy_drift.py proves a copy is CURRENT and says nothing
    about whether anything RUNS it, so the two checks are different questions
    and only one of them is being asked."

This asks the other one.

WHAT COUNTS AS A DRIVER, and why the credit is PRINTED rather than assumed
--------------------------------------------------------------------------
A mutant is driven when something OUTSIDE `tests/mutants/` names it: the file
path in a `SOURCES` list, a declared module instantiated by a bench, a
`TOP_MODULE`, a `.cpp` compiled into a target. Those are the four shapes in
this tree and they are all just "the name appears somewhere that builds".

So the search is deliberately GENEROUS -- it credits any mention of the file or
of any module it declares, anywhere under `tests/`, `tools/`, `design/` or
`fpga/` -- and every credit is printed with the file and line that granted it.
A generous checker that shows its working can be audited; a strict one that
prints a number cannot. CLAUDE.md: "when a tool explains itself, the
explanation is a claim too".

THE ONE REFERENCE SHAPE THAT IS NOT A LITERAL NAME. `tests/CMakeLists.txt`
builds some mutant paths from a loop variable:

    SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_forge_cliff_ram_${mut}.sv)

A literal-name search reports those three files as undriven, which is a FALSE
ALARM -- the direction that gets a tool switched off. So a reference containing
`${...}` is matched by its literal prefix and suffix, and the credit line says
PREFIX so the reader knows it is weaker evidence than a literal hit. It can
over-credit a file that happens to share a prefix; it is reported as such.

WHAT THIS TOOL DOES NOT CLAIM. That a driver EXISTS is not that the driver
RUNS, and not that it has inverted polarity, and not that the mutant is current
(`mutant_copy_drift.py` owns that last question). Three different questions;
this one answers the cheapest and most neglected.

Exit 0 when every mutant has a driver, 1 when any does not.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z_]\w*)", re.M)
# A reference to tests/mutants/<something> inside a build file, with the
# CMake-variable case kept so it can be turned into a prefix/suffix rule.
MUTANT_REF_RE = re.compile(r"tests/mutants/([A-Za-z0-9_.$\{\}\-]+)")

SEARCH_ROOTS = ("tests", "tools", "design", "fpga")
# Files that BUILD things. `.md` is deliberately absent: a contract, a report or
# a ruling that mentions a mutant is prose, and prose elaborates nothing.
SEARCH_SUFFIXES = (".txt", ".cmake", ".py", ".ps1", ".sv", ".v", ".cpp", ".hpp",
                   ".yml", ".yaml", ".sby")
# Comment markers by suffix. A mention inside a COMMENT is prose too, and this
# tool's first version proved it the embarrassing way: it credited
# `zhao_terrain_bake_v2_mutant.sv` -- the one file owner ruling R93 states has
# no driver -- to ITS OWN DOCSTRING, which names the file while quoting R93.
# The detector reported zero for the case it was written for. Nobody audits
# good news, so this is written down where it happened.
LINE_COMMENT = {".txt": "#", ".cmake": "#", ".py": "#", ".ps1": "#",
                ".yml": "#", ".yaml": "#", ".sby": "#",
                ".sv": "//", ".v": "//", ".cpp": "//", ".hpp": "//"}

# THE DEBT LIST. A mutant here is KNOWN to have no driver, with the reason and
# whose decision it is. It keeps this gate GREEN on the tree as it stands while
# going RED the moment a NEW undriven mutant is committed -- which is the
# regression worth catching -- and it makes the debt visible on every run
# instead of forgotten. An entry that GAINS a driver is reported too, so the
# list cannot rot in the flattering direction.
KNOWN_UNDRIVEN = {
    "zhao_terrain_bake_v2_mutant.sv":
        "owner ruling R93, 2026-09-20: 'it gets a driver or it gets retired, and "
        "the next terrain packet decides which'. Not refreshed again until then.",
    "shell_fit_postmap_connectivity_dangling_mutant.sv":
        "found 2026-09-20 by this tool's first run. Undecided: the shell lane owns it.",
    "zhao_texture_timing4_r1t_e1_mutants.sv":
        "found 2026-09-20 by this tool's first run. Undecided: the texture lane owns it.",
}


def declared_modules(text: str) -> list[str]:
    return MODULE_RE.findall(text)


def read(path: Path) -> str:
    # R92: line endings are NOT uniform in this tree, so never anchor on "\n"
    # without deriving it. Reading as text with universal newlines sidesteps it.
    return path.read_text(encoding="utf-8", errors="replace")


def self_check() -> None:
    """Assert the two patterns still match a known-good example.

    Three tools in this repo do this because one self-check was written with an
    escape a heredoc ate, so it matched nothing and printed reassurance for its
    whole life. A pattern that has not been shown to match is a claim.
    """
    sample = "`default_nettype none\nmodule zhao_example_mutant #(\n"
    assert declared_modules(sample) == ["zhao_example_mutant"], \
        "mutant_drivers: MODULE_RE no longer matches a plain module declaration"
    line = "  SOURCES ${CMAKE_SOURCE_DIR}/tests/mutants/zhao_forge_cliff_ram_${mut}.sv)"
    hits = MUTANT_REF_RE.findall(line)
    assert hits and "${mut}" in hits[0], \
        "mutant_drivers: MUTANT_REF_RE no longer sees a variable-expanded path"
    assert MUTANT_REF_RE.findall("tests/mutants/zhao_thing_mutant.sv") == \
        ["zhao_thing_mutant.sv"], "mutant_drivers: MUTANT_REF_RE broke on a literal path"


def strip_comment(line: str, suffix: str) -> str:
    """Return the CODE half of a line. A mention in a comment drives nothing."""
    marker = LINE_COMMENT.get(suffix)
    if not marker:
        return line
    cut = line.find(marker)
    return line if cut < 0 else line[:cut]


def searchable_files(root: Path, me: Path) -> list[Path]:
    out: list[Path] = []
    for sub in SEARCH_ROOTS:
        base = root / sub
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file() or p.suffix not in SEARCH_SUFFIXES:
                continue
            # The mutants themselves never credit each other.
            if "mutants" in p.parts and p.parent.name == "mutants":
                continue
            # And this file never credits anything: it names mutants in prose.
            if p.resolve() == me:
                continue
            out.append(p)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo-root", default=".", type=Path)
    ap.add_argument("--verbose", action="store_true",
                    help="print the file and line that credits each mutant")
    args = ap.parse_args()
    self_check()

    root = args.repo_root.resolve()
    mdir = root / "tests" / "mutants"
    if not mdir.is_dir():
        print(f"mutant_drivers: no {mdir}", file=sys.stderr)
        return 2

    mutants = sorted(p for p in mdir.iterdir()
                     if p.is_file() and p.suffix in (".sv", ".v", ".cpp"))
    if not mutants:
        print("mutant_drivers: tests/mutants/ is empty -- nothing to check")
        return 0

    # tokens[name] -> the strings whose appearance credits this mutant
    tokens: dict[str, set[str]] = {}
    for m in mutants:
        toks = {m.name, m.stem}
        if m.suffix in (".sv", ".v"):
            toks |= set(declared_modules(read(m)))
        tokens[m.name] = toks

    credit: dict[str, tuple[str, str, int, str]] = {}   # mutant -> (kind, file, line, text)
    prefix_rules: list[tuple[str, str, Path, int]] = []  # (prefix, suffix, file, line)

    for f in searchable_files(root, Path(__file__).resolve()):
        try:
            text = read(f)
        except OSError:
            continue
        rel = f.relative_to(root).as_posix()
        for lineno, raw in enumerate(text.splitlines(), 1):
            line = strip_comment(raw, f.suffix)
            if not line.strip():
                continue
            # collect variable-expanded mutant references for the second pass
            for ref in MUTANT_REF_RE.findall(line):
                if "${" in ref:
                    pre, _, rest = ref.partition("${")
                    _, _, suf = rest.partition("}")
                    prefix_rules.append((pre, suf, f, lineno))
            for m in mutants:
                if m.name in credit:
                    continue
                for tok in tokens[m.name]:
                    if tok in line:
                        credit[m.name] = ("literal", rel, lineno, line.strip()[:120])
                        break

    for m in mutants:
        if m.name in credit:
            continue
        for pre, suf, f, lineno in prefix_rules:
            if pre and m.name.startswith(pre) and m.name.endswith(suf):
                credit[m.name] = ("PREFIX", f.relative_to(root).as_posix(), lineno,
                                  f"{pre}${{...}}{suf}")
                break

    undriven = [m for m in mutants if m.name not in credit]
    # The debt list cannot rot in the flattering direction: an entry that has
    # SINCE gained a driver is reported, so the list shrinks when the tree does.
    stale_debt = [n for n in KNOWN_UNDRIVEN if n in credit]
    new_undriven = [m for m in undriven if m.name not in KNOWN_UNDRIVEN]
    known_undriven = [m for m in undriven if m.name in KNOWN_UNDRIVEN]

    if args.verbose:
        for m in mutants:
            c = credit.get(m.name)
            if c:
                print(f"  {m.name:<55} {c[0]:<7} {c[1]}:{c[2]}")
            else:
                print(f"  {m.name:<55} NONE")

    weak = [n for n, c in credit.items() if c[0] == "PREFIX"]
    rc = 0

    # The debt list is never silent. It is a deferral written down, and
    # CLAUDE.md's rule is that a deferral written down stays OPEN.
    if known_undriven:
        print(f"DEBT LIST: {len(known_undriven)} committed mutant(s) that NOTHING RUNS.")
        print("  An unrun mutant costs the fitter nothing and is cited as evidence,")
        print("  which is the expensive half (ruling R93). Each gets a driver or")
        print("  gets retired; what it does not get is another refresh.")
        for m in known_undriven:
            print(f"    {m.name}")
            print(f"      {KNOWN_UNDRIVEN[m.name]}")

    if stale_debt:
        print(f"DEBT LIST IS STALE: {len(stale_debt)} entry(ies) now HAVE a driver.")
        print("  Remove them from KNOWN_UNDRIVEN in this file -- an allowlist that")
        print("  outlives its reason is the next silent pass.")
        for n in sorted(stale_debt):
            c = credit[n]
            print(f"    {n}  <- {c[1]}:{c[2]}")
        rc = 1

    if new_undriven:
        print(f"NEW MUTANTS WITH NO DRIVER: {len(new_undriven)} of {len(mutants)}")
        print("  Nothing outside tests/mutants/ names these files or any module")
        print("  they declare, so nothing has ever elaborated them:")
        for m in new_undriven:
            print(f"    {m.name}")
        print("  Add the test that runs it in the same commit as the mutant (R93),")
        print("  or put it on this file's DEBT LIST with a reason and an owner.")
        rc = 1

    if rc == 0:
        print(f"OK -- {len(mutants) - len(known_undriven)} of {len(mutants)} committed "
              "mutants are named by something that builds"
              + (f"; {len(known_undriven)} on the debt list" if known_undriven else "")
              + (f"; {len(weak)} credited by a PREFIX rule (--verbose to audit)" if weak else ""))
    return rc


if __name__ == "__main__":
    sys.exit(main())
