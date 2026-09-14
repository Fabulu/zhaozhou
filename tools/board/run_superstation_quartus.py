#!/usr/bin/env python3
"""Run a SuperStation Quartus flow without allowing QSF mutation."""

from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path
from typing import Callable


PATCHED_BUILD_ID_SHA256 = "e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6"

PROJECTS = {
    "ZhaozhouBringup": {
        "device": "5CSEBA6U23I7",
        "outputDirectory": "output_files",
    },
    "ZhaozhouSpecs": {
        "device": "5CSEBA6U23I7",
        "outputDirectory": "output_files",
    },
}


def stage_commands(quartus_bin: Path, build: Path, project: str) -> list[list[str]]:
    profile = PROJECTS[project]
    settings = ["--read_settings_files=on", "--write_settings_files=off"]
    return [
        [
            str(quartus_bin / "quartus_sh.exe"),
            "-t",
            str(build / "sys" / "build_id.tcl"),
            project,
            profile["device"],
            profile["outputDirectory"],
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
    build_id = build / "sys" / "build_id.tcl"
    if not build_id.is_file():
        raise ValueError(f"projectless build-ID script is missing: {build_id}")
    build_id_bytes = build_id.read_bytes()
    build_id_sha = hashlib.sha256(build_id_bytes).hexdigest()
    if build_id_sha != PATCHED_BUILD_ID_SHA256:
        raise ValueError(
            "projectless build-ID digest mismatch: "
            f"{build_id_sha} != {PATCHED_BUILD_ID_SHA256}"
        )
    forbidden = (b"project_open", b"project_close", b"get_global_assignment")
    if any(token in build_id_bytes for token in forbidden):
        raise ValueError("build-ID script still has a project/settings access path")
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
