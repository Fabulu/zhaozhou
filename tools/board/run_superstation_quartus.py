#!/usr/bin/env python3
"""Run a SuperStation Quartus flow without allowing QSF mutation."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable


PATCHED_BUILD_ID_SHA256 = "e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6"
STAGE_RECEIPT_SCHEMA = "zhaozhou.superstation.stage-sequence-receipt.v1"
EXPECTED_TOOL_VERSION = "17.0.2 Build 602 07/19/2017 SJ Lite Edition"
STAGE_OUTPUT_SUFFIXES = (
    "asm.rpt",
    "fit.rpt",
    "fit.smsg",
    "fit.summary",
    "flow.rpt",
    "jdi",
    "map.rpt",
    "map.smsg",
    "map.summary",
    "pin",
    "rbf",
    "sld",
    "sof",
    "sta.rpt",
    "sta.summary",
)
STAGE_NAMES = ("buildId", "map", "fit", "asm", "sta")

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


def file_record(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def receipt_bytes(data: dict[str, Any]) -> bytes:
    return (json.dumps(data, sort_keys=True, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )


def receipt_digest(data: dict[str, Any]) -> str:
    unsigned = dict(data)
    unsigned.pop("receiptSha256", None)
    encoded = json.dumps(unsigned, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def expected_stage_outputs(project: str) -> set[str]:
    return {f"{project}.{suffix}" for suffix in STAGE_OUTPUT_SUFFIXES}


def query_tool_version(
    quartus_bin: Path,
    build: Path,
    runner: Callable[..., subprocess.CompletedProcess[Any]],
) -> str:
    command = [str(quartus_bin / "quartus_sh.exe"), "--version"]
    completed = runner(
        command,
        cwd=build,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode:
        raise ValueError(f"quartus_sh.exe --version failed with exit code {completed.returncode}")
    output = completed.stdout
    if isinstance(output, bytes):
        output = output.decode("cp1252")
    match = re.search(r"(?m)^Version (.+)$", str(output or ""))
    if not match:
        raise ValueError("Quartus version output is missing the Version line")
    version = match.group(1).strip()
    if version != EXPECTED_TOOL_VERSION:
        raise ValueError(
            f"Quartus tool version mismatch: {version!r} != {EXPECTED_TOOL_VERSION!r}"
        )
    return version


def verify_stage_receipt_data(
    data: dict[str, Any], quartus_bin: Path, build: Path, project: str
) -> list[str]:
    errors: list[str] = []
    profile = PROJECTS.get(project)
    if profile is None:
        return [f"unsupported SuperStation project: {project}"]
    expected_keys = {
        "schema",
        "project",
        "device",
        "toolVersion",
        "quartusBin",
        "versionCommand",
        "qsf",
        "stages",
        "artifacts",
        "receiptSha256",
    }
    if set(data) != expected_keys:
        errors.append("stage receipt top-level keys mismatch")
    if data.get("schema") != STAGE_RECEIPT_SCHEMA:
        errors.append("stage receipt schema mismatch")
    if data.get("project") != project:
        errors.append("stage receipt project mismatch")
    if data.get("device") != profile["device"]:
        errors.append("stage receipt device mismatch")
    if data.get("toolVersion") != EXPECTED_TOOL_VERSION:
        errors.append("stage receipt tool version mismatch")
    if data.get("quartusBin") != str(quartus_bin):
        errors.append("stage receipt Quartus bin mismatch")
    expected_version_command = [str(quartus_bin / "quartus_sh.exe"), "--version"]
    if data.get("versionCommand") != expected_version_command:
        errors.append("stage receipt version command mismatch")
    if data.get("receiptSha256") != receipt_digest(data):
        errors.append("stage receipt self-digest mismatch")

    qsf = data.get("qsf")
    qsf_path = build / f"{project}.qsf"
    actual_qsf = file_record(qsf_path) if qsf_path.is_file() else None
    if not isinstance(qsf, dict):
        errors.append("stage receipt QSF record is invalid")
    else:
        if set(qsf) != {"file", "before", "afterEach"}:
            errors.append("stage receipt QSF keys mismatch")
        if qsf.get("file") != qsf_path.name:
            errors.append("stage receipt QSF path mismatch")
        if qsf.get("before") != actual_qsf:
            errors.append("stage receipt QSF before record mismatch")
        after_each = qsf.get("afterEach")
        expected_after = {name: actual_qsf for name in STAGE_NAMES}
        if after_each != expected_after:
            errors.append("stage receipt QSF stage records mismatch")

    expected_commands = stage_commands(quartus_bin, build, project)
    stages = data.get("stages")
    if not isinstance(stages, list) or len(stages) != len(STAGE_NAMES):
        errors.append("stage receipt stage list mismatch")
    else:
        for index, (name, command) in enumerate(zip(STAGE_NAMES, expected_commands)):
            expected = {"name": name, "command": command, "returnCode": 0}
            if stages[index] != expected:
                errors.append(f"stage receipt command/RC mismatch: {name}")

    output = build / "output_files"
    expected_names = expected_stage_outputs(project)
    actual_names = {path.name for path in output.iterdir() if path.is_file()} if output.is_dir() else set()
    receipt_name = f"{project}.stage-receipt.json"
    if actual_names != expected_names | {receipt_name}:
        errors.append(
            "stage output record set mismatch: "
            f"missing={sorted((expected_names | {receipt_name}) - actual_names)} "
            f"extra={sorted(actual_names - (expected_names | {receipt_name}))}"
        )
    artifacts = data.get("artifacts")
    if not isinstance(artifacts, dict) or set(artifacts) != expected_names:
        errors.append("stage receipt artifact record set mismatch")
    else:
        for name in sorted(expected_names):
            path = output / name
            if not path.is_file() or artifacts.get(name) != file_record(path):
                errors.append(f"stage receipt artifact mismatch: {name}")
    return errors


def verify_stage_receipt_file(
    quartus_bin: Path, build: Path, project: str
) -> list[str]:
    path = build / "output_files" / f"{project}.stage-receipt.json"
    if not path.is_file():
        return [f"stage sequence receipt is missing: {path}"]
    try:
        encoded = path.read_bytes()
        data = json.loads(encoded.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        return [f"stage sequence receipt cannot be parsed: {exc}"]
    if not isinstance(data, dict):
        return ["stage sequence receipt is not an object"]
    errors = verify_stage_receipt_data(data, quartus_bin, build, project)
    if encoded != receipt_bytes(data):
        errors.append("stage sequence receipt encoding is not canonical")
    return errors


def write_stage_receipt(
    quartus_bin: Path,
    build: Path,
    project: str,
    tool_version: str,
    qsf_record: dict[str, Any],
    commands: list[list[str]],
) -> Path:
    output = build / "output_files"
    expected_names = expected_stage_outputs(project)
    actual_names = {path.name for path in output.iterdir() if path.is_file()} if output.is_dir() else set()
    if actual_names != expected_names:
        raise ValueError(
            "direct-stage output record set mismatch before receipt: "
            f"missing={sorted(expected_names - actual_names)} "
            f"extra={sorted(actual_names - expected_names)}"
        )
    data: dict[str, Any] = {
        "schema": STAGE_RECEIPT_SCHEMA,
        "project": project,
        "device": PROJECTS[project]["device"],
        "toolVersion": tool_version,
        "quartusBin": str(quartus_bin),
        "versionCommand": [str(quartus_bin / "quartus_sh.exe"), "--version"],
        "qsf": {
            "file": f"{project}.qsf",
            "before": qsf_record,
            "afterEach": {name: qsf_record for name in STAGE_NAMES},
        },
        "stages": [
            {"name": name, "command": command, "returnCode": 0}
            for name, command in zip(STAGE_NAMES, commands)
        ],
        "artifacts": {
            name: file_record(output / name) for name in sorted(expected_names)
        },
    }
    data["receiptSha256"] = receipt_digest(data)
    encoded = receipt_bytes(data)
    path = output / f"{project}.stage-receipt.json"
    temporary = path.with_name(path.name + ".tmp")
    try:
        temporary.write_bytes(encoded)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()
    errors = verify_stage_receipt_file(quartus_bin, build, project)
    if errors:
        path.unlink(missing_ok=True)
        raise ValueError("stage sequence receipt failed self-verification: " + "; ".join(errors))
    return path


def run_compile(
    quartus_bin: Path,
    build: Path,
    project: str,
    *,
    runner: Callable[..., subprocess.CompletedProcess[Any]] = subprocess.run,
) -> Path:
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
    qsf_record = file_record(qsf)
    tool_version = query_tool_version(quartus_bin, build, runner)
    if qsf.read_bytes() != original_qsf:
        raise ValueError("Quartus version query mutated manifest-bound QSF")
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
    return write_stage_receipt(
        quartus_bin, build, project, tool_version, qsf_record, commands
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--quartus-bin", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--project", choices=sorted(PROJECTS), required=True)
    args = parser.parse_args()
    try:
        receipt = run_compile(
            args.quartus_bin.resolve(), args.build_dir.resolve(), args.project
        )
    except (OSError, ValueError) as exc:
        print(f"SUPERSTATION_QUARTUS_FAIL: {exc}", file=sys.stderr)
        return 1
    print(
        "SuperStation Quartus compile completed without QSF mutation: "
        f"{args.project} receipt={receipt}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
