#!/usr/bin/env python3
"""Run a SuperStation Quartus flow without allowing QSF mutation."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import Callable, Sequence


PROJECTS = {"ZhaozhouBringup", "ZhaozhouSpecs"}


def stage_commands(quartus_bin: Path, build: Path, project: str) -> list[list[str]]:
    settings = ["--read_settings_files=on", "--write_settings_files=off"]
    return [
        [
            str(quartus_bin / "quartus_sh.exe"),
            "-t",
            str(build / "sys" / "build_id.tcl"),
            "compile",
            project,
            project,
        ],
        [str(quartus_bin / "quartus_map.exe"), *settings, project, "-c", project],
        [str(quartus_bin / "quartus_fit.exe"), *settings, project, "-c", project],
        [str(quartus_bin / "quartus_asm.exe"), *settings, project, "-c", project],
        [str(quartus_bin / "quartus_sta.exe"), project, "-c", project],
    ]


def run_compile(
    quartus_bin: Path,
    build: Path,
    project: str,
    *,
    runner: Callable[..., subprocess.CompletedProcess[bytes]] = subprocess.run,
) -> None:
    if project not in PROJECTS:
        raise ValueError(f"unsupported SuperStation project: {project}")
    qsf = build / f"{project}.qsf"
    if not qsf.is_file():
        raise ValueError(f"project QSF is missing: {qsf}")
    original_qsf = qsf.read_bytes()
    commands = stage_commands(quartus_bin, build, project)
    for command in commands:
        executable = Path(command[0])
        if not executable.is_file():
            raise ValueError(f"required Quartus executable is missing: {executable}")
    for command in commands:
        completed = runner(command, cwd=build, check=False)
        current_qsf = qsf.read_bytes() if qsf.is_file() else None
        if current_qsf != original_qsf:
            raise ValueError(
                f"Quartus mutated manifest-bound {qsf.name} while running {Path(command[0]).name}"
            )
        if completed.returncode:
            raise ValueError(
                f"{Path(command[0]).name} failed with exit code {completed.returncode}"
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--quartus-bin", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--project", choices=sorted(PROJECTS), required=True)
    args = parser.parse_args()
    try:
        run_compile(args.quartus_bin.resolve(), args.build_dir.resolve(), args.project)
    except (OSError, ValueError) as exc:
        print(f"SUPERSTATION_QUARTUS_FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"SuperStation Quartus compile completed without QSF mutation: {args.project}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
