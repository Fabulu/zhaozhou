#!/usr/bin/env python3
"""Find committed mutant copies that production has moved out from under.

A committed mutant under tests/mutants/ is a renamed copy of a production
module differing by ONE substantive line. That contract has no enforcement:
when the production module is edited, the copy keeps compiling, keeps passing
its own control, and quietly starts describing a machine that no longer
exists.

This was found twice on 2026-09-16, both times by accident:

  * the thirteen combiner copies were cut at 005578fb and production had moved
    twice since. Each "one substantive line" had become seventy to a hundred
    and forty. Every control stayed green the whole time, because the
    MUTATIONS were intact -- it was the body around them that was stale.
  * the eight AUX-pipe copies were cut at 10dd9cc4 and production moved at
    005578fb. Seven went on passing. The eighth failed with
    "exactly 16 live: expected 0x10, got 0x11", which reads exactly like a
    credit overflow in shipped RTL and is nothing of the kind.

That second one is the whole argument for this tool. The failure did not say
"your mutant is stale"; it said something alarming about the design, and it
pointed at the wrong component. Seven copies reporting success about a
two-week-old body is the flattering direction, and nobody audits good news.

THE SIGNAL is provenance, not similarity: if the production file has been
committed since the copy's file was, the copy cannot contain what production
gained. Diff size is reported as corroboration only -- some mutants are
legitimately large, because removing a pipeline stage is a large edit.

Exit 0 when every copy is current, 1 when any has drifted.
"""

from __future__ import annotations

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z_]\w*)", re.M)


def run_git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=repo, capture_output=True, text=True,
        encoding="utf-8", errors="replace")
    return result.stdout.strip() if result.returncode == 0 else ""


DIRTY = -1


def last_commit_time(repo: Path, path: Path) -> int:
    """Seconds since epoch of the file's last commit, or DIRTY.

    A file with uncommitted edits has no commit time to compare, and guessing
    one in either direction is worse than saying so: treating it as infinitely
    new fails the gate on every work-in-progress tree until it is ignored, and
    treating it as old hides real drift. Dirty pairs are REPORTED and do not
    fail -- this gate polices what is committed.
    """
    rel = path.relative_to(repo).as_posix()
    if run_git(repo, "status", "--porcelain", "--", rel):
        return DIRTY
    stamp = run_git(repo, "log", "-1", "--format=%ct", "--", rel)
    return int(stamp) if stamp else 0


def index_production_modules(repo: Path) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for path in sorted((repo / "fpga" / "rtl").rglob("*.sv")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for name in MODULE_RE.findall(text):
            index.setdefault(name, path)
    return index


def squeeze(text: str) -> list[str]:
    """Lines with runs of whitespace collapsed and comments dropped.

    Copies routinely re-align declarations when a longer name is substituted,
    and that is not drift.
    """
    out = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        out.append(re.sub(r"\s+", " ", stripped))
    return out


def extract_module(text: str, module: str) -> str:
    """Just this module's own text.

    Several mutant files hold a dozen renamed copies in one file, so diffing
    the file would report the other eleven as drift and drown the signal.
    """
    start = re.search(rf"^\s*module\s+{re.escape(module)}\b", text, re.M)
    if not start:
        return text
    end = re.compile(r"^\s*endmodule\b.*$", re.M).search(text, start.start())
    return text[start.start():end.end() if end else len(text)]


def substantive_diff(production: str, copy: str, copy_module: str,
                     production_module: str) -> int:
    body = extract_module(copy, copy_module)
    restored = body.replace(copy_module, production_module)
    diff = difflib.unified_diff(
        squeeze(extract_module(production, production_module)),
        squeeze(restored), lineterm="", n=0)
    return sum(
        1 for line in diff
        if line.startswith(("+", "-")) and not line.startswith(("+++", "---")))


def self_check() -> None:
    """The detector must be shown to fire before its silence means anything."""
    production = "module m;\n  assign a = b;\n  assign c = d;\nendmodule\n"
    unchanged = production.replace("module m", "module m_mut")
    drifted = "module m_mut;\n  assign a = b;\nendmodule\n"
    near = substantive_diff(production, unchanged, "m_mut", "m")
    far = substantive_diff(production, drifted, "m_mut", "m")
    if near != 0 or far == 0:
        raise SystemExit(
            f"mutant_copy_drift self-check FAILED: identical={near} "
            f"(want 0), removed-line={far} (want > 0)")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=None)
    parser.add_argument("-q", "--quiet", action="store_true")
    args = parser.parse_args(argv)

    repo = Path(args.repo_root) if args.repo_root \
        else Path(__file__).resolve().parents[2]
    self_check()

    production_index = index_production_modules(repo)
    mutant_dir = repo / "tests" / "mutants"
    if not mutant_dir.is_dir():
        print(f"no mutant directory at {mutant_dir}")
        return 0

    drifted: list[tuple[str, str, int, Path]] = []
    in_progress: list[str] = []
    checked = 0
    unmatched = 0

    for path in sorted(mutant_dir.glob("*.sv")):
        text = path.read_text(encoding="utf-8", errors="replace")
        copy_time = last_commit_time(repo, path)
        for copy_module in MODULE_RE.findall(text):
            # A copy is named <production module>_<something>. Prefer the
            # LONGEST production name that prefixes it, so
            # zhao_texture_aux_pipe_v2_credit_mutant resolves to
            # zhao_texture_aux_pipe_v2 and not zhao_texture_aux_pipe.
            candidates = [
                name for name in production_index
                if copy_module.startswith(name + "_")
            ]
            if not candidates:
                unmatched += 1
                continue
            production_module = max(candidates, key=len)
            production_path = production_index[production_module]

            # A WRAPPER instantiates the production module instead of copying
            # it, so it cannot go stale -- it elaborates whatever production
            # currently is. Only renamed COPIES are at risk. Checking the whole
            # file rather than this module's own text on purpose: a file of
            # macro-generated wrappers shares one instantiation template.
            if re.search(rf"^\s*{re.escape(production_module)}\s+[#(\w]",
                         text, re.M):
                continue
            checked += 1

            production_time = last_commit_time(repo, production_path)
            if DIRTY in (production_time, copy_time):
                in_progress.append(f"{copy_module} / {production_module}")
                continue
            if production_time <= copy_time:
                continue

            production_text = production_path.read_text(
                encoding="utf-8", errors="replace")
            lines = substantive_diff(
                production_text, text, copy_module, production_module)
            drifted.append((copy_module, production_module, lines, path))

    if not args.quiet:
        print(f"mutant copy drift: {checked} copies checked against "
              f"{len(production_index)} production modules "
              f"({unmatched} files matched no production module)")

    if in_progress and not args.quiet:
        print(f"  ({len(in_progress)} pairs skipped: one side has uncommitted "
              f"edits, so there is no commit order to read)")

    if not drifted:
        if not args.quiet:
            print("OK -- every committed mutant copy is at least as new as the "
                  "module it copies")
        return 0

    print(f"\nDRIFTED: {len(drifted)} committed mutant copies are older than "
          f"the production module they copy.\n")
    for copy_module, production_module, lines, path in drifted:
        print(f"  {copy_module}")
        print(f"      copies {production_module}, which was committed later")
        print(f"      {lines} substantive diff lines "
              f"(a faithful copy is a handful)")
        print(f"      {path.relative_to(repo).as_posix()}")
    print("\nA stale copy still compiles, still passes its own control, and\n"
          "still reports numbers -- about a machine that no longer exists.\n"
          "Refresh it onto the current body, keeping its one mutation.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
