#!/usr/bin/env python3
"""Capture exact Git evidence bytes for a shell-fit receipt."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import uuid

from shell_fit_reports import read_direct_git_evidence, read_git_blobs_at_commit
from shell_ports import ShellPortError


FILENAMES = {
    "gitHead": "git-head.txt",
    "gitStatus": "git-status.txt",
    "gitWorktreeDiff": "git-worktree.diff",
    "gitStagedDiff": "git-staged.diff",
    "gitIndexFlags": "git-index-flags.bin",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--compare-crlf-only", type=Path)
    parser.add_argument("--commit")
    args = parser.parse_args()
    try:
        repo = args.repo_root.resolve()
        if args.compare_crlf_only is not None:
            if args.output_dir is not None or args.commit is None:
                raise ShellPortError(
                    "CRLF-only comparison requires --commit and forbids --output-dir"
                )
            live_path = args.compare_crlf_only.resolve()
            relative = live_path.relative_to(repo).as_posix()
            committed = read_git_blobs_at_commit(
                repo,
                source_commit=args.commit,
                source_paths={"launcher": relative},
            )["launcher"]
            live = live_path.read_bytes()
            for label, data in (("committed", committed), ("live", live)):
                try:
                    data.decode("utf-8")
                except UnicodeDecodeError as exc:
                    raise ShellPortError(f"{label} file is not UTF-8: {exc}") from exc
            if live.replace(b"\r\n", b"\n") != committed.replace(b"\r\n", b"\n"):
                raise ShellPortError(
                    "file differs from the captured commit beyond CRLF/LF normalization"
                )
            print(f"shell-fit-git-capture: CRLF/LF-only comparison passed for {relative}")
            return 0
        if args.output_dir is None or args.commit is not None:
            raise ShellPortError("evidence capture requires --output-dir and forbids --commit")
        output = args.output_dir.resolve()
        output.relative_to(repo)
        output.mkdir(parents=True, exist_ok=True)
        evidence = read_direct_git_evidence(repo)
        temporaries: list[Path] = []
        try:
            for name, filename in FILENAMES.items():
                destination = output / filename
                temporary = output / f".{filename}.{uuid.uuid4().hex}.tmp"
                temporaries.append(temporary)
                with temporary.open("xb") as stream:
                    stream.write(evidence[name])
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(temporary, destination)
                temporaries.remove(temporary)
        finally:
            for temporary in temporaries:
                if temporary.exists():
                    temporary.unlink()
    except (OSError, ValueError, ShellPortError) as exc:
        print(f"shell-fit-git-capture: {exc}", file=sys.stderr)
        return 1
    print(f"shell-fit-git-capture: clean evidence written to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
