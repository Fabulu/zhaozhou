#!/usr/bin/env python3
"""Create and verify complete SuperStation source/report/RBF manifests."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable

PATCHED_SYS_TOP_SHA256 = "24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9"
ARTIFACT_SUFFIXES = (
    "asm.rpt",
    "done",
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

PROFILES = {
    "Bringup": {
        "project": "ZhaozhouBringup",
        "marker": ".zhaozhou-superstation-build",
        "qip": "files_bringup.qip",
        "sourcePaths": (
            ".gitattributes",
            "fpga/sys",
            "fpga/ZhaozhouBringup.qpf",
            "fpga/ZhaozhouBringup.qsf",
            "fpga/ZhaozhouBringup.sdc",
            "fpga/files_bringup.qip",
            "fpga/rtl/platform/zhao_ssone_bringup.sv",
            "fpga/rtl/pll.qip",
            "fpga/rtl/pll.v",
            "fpga/rtl/pll",
            "tools/env/zhao-env.ps1",
            "tools/board/patch_mister_sys_top.py",
            "tools/board/run_superstation_quartus.py",
            "tools/board/superstation_build_manifest.py",
            "tools/board/build_superstation_bringup.ps1",
            "tools/board/verify_superstation_bringup.py",
        ),
        "verificationPaths": (
            "tools/board/invoke_superstation_probe.ps1",
            "tools/board/verify_superstation_receipt.py",
            "tests/tools/test_patch_mister_sys_top.py",
            "tests/tools/test_run_superstation_quartus.py",
            "tests/tools/test_superstation_build_manifest.py",
            "tests/tools/test_verify_superstation_receipt.py",
            "tests/tools/test_verify_superstation_bringup.py",
        ),
    },
    "Specs": {
        "project": "ZhaozhouSpecs",
        "marker": ".zhaozhou-superstation-specs-build",
        "qip": "files_specs.qip",
        "sourcePaths": (
            ".gitattributes",
            "fpga/sys",
            "fpga/ZhaozhouSpecs.qpf",
            "fpga/ZhaozhouSpecs.qsf",
            "fpga/ZhaozhouSpecs.sdc",
            "fpga/files_specs.qip",
            "fpga/rtl/common/zhao_crc32c_fold.sv",
            "fpga/rtl/raster/zhao_raster_fill.sv",
            "fpga/rtl/common/zhao_dual18_mul.sv",
            "fpga/rtl/platform/zhao_ssone_spec_tests.sv",
            "fpga/rtl/platform/zhao_ssone_specs_emu.sv",
            "fpga/rtl/pll.qip",
            "fpga/rtl/pll.v",
            "fpga/rtl/pll",
            "tools/env/zhao-env.ps1",
            "tools/board/patch_mister_sys_top.py",
            "tools/board/run_superstation_quartus.py",
            "tools/board/superstation_build_manifest.py",
            "tools/board/build_superstation_specs.ps1",
            "tools/board/verify_superstation_bringup.py",
            "tools/board/verify_superstation_specs.py",
        ),
        "verificationPaths": (
            "tools/board/invoke_superstation_probe.ps1",
            "tools/board/verify_superstation_receipt.py",
            "tests/board/run_ssone_spec_tests.py",
            "tests/board/ssone_spec_tests_tb.sv",
            "tests/tools/test_patch_mister_sys_top.py",
            "tests/tools/test_run_superstation_quartus.py",
            "tests/tools/test_superstation_build_manifest.py",
            "tests/tools/test_verify_superstation_receipt.py",
            "tests/tools/test_verify_superstation_bringup.py",
            "tests/tools/test_verify_superstation_specs.py",
        ),
    },
}


def file_record(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def expand_files(root: Path, entries: Iterable[str]) -> list[Path]:
    files: list[Path] = []
    for entry in entries:
        path = root / entry
        if path.is_file():
            files.append(path)
        elif path.is_dir():
            files.extend(item for item in path.rglob("*") if item.is_file())
        else:
            raise ValueError(f"manifest input does not exist: {entry}")
    return sorted(set(files), key=lambda item: item.as_posix())


def records(root: Path, files: Iterable[Path]) -> dict[str, dict[str, Any]]:
    return {
        path.relative_to(root).as_posix(): file_record(path)
        for path in files
    }


def git_output(repo: Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        raise ValueError(completed.stderr.strip() or f"git {' '.join(args)} failed")
    return completed.stdout.strip()


def manifest_digest(data: dict[str, Any]) -> str:
    unsigned = dict(data)
    unsigned.pop("manifestSha256", None)
    canonical = json.dumps(unsigned, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def marker_commit(build: Path, marker_name: str) -> str:
    marker = build / marker_name
    if not marker.is_file():
        raise ValueError(f"missing owned-build marker: {marker}")
    for line in marker.read_text(encoding="utf-8").splitlines():
        if line.startswith("sourceCommit="):
            commit = line.removeprefix("sourceCommit=")
            if len(commit) == 40 and all(char in "0123456789abcdef" for char in commit):
                return commit
    raise ValueError(f"build marker has no full sourceCommit: {marker}")


def relative_file_set(root: Path, entries: Iterable[str]) -> set[str]:
    return {path.relative_to(root).as_posix() for path in expand_files(root, entries)}


def expected_build_input_relatives(repo: Path, profile: dict[str, Any]) -> set[str]:
    project = profile["project"]
    expected = {
        f"{project}.qpf",
        f"{project}.qsf",
        f"{project}.sdc",
        profile["qip"],
    }
    for source in expand_files(repo, ("fpga/sys",)):
        expected.add(source.relative_to(repo / "fpga").as_posix())
    for source in expand_files(repo, profile["sourcePaths"]):
        relative = source.relative_to(repo).as_posix()
        if relative.startswith("fpga/rtl/"):
            expected.add(relative.removeprefix("fpga/"))
    return expected


def build_input_paths(repo: Path, build: Path, profile: dict[str, Any]) -> list[Path]:
    relatives = sorted(expected_build_input_relatives(repo, profile))
    paths = [build / relative for relative in relatives]
    missing = [path.relative_to(build).as_posix() for path in paths if not path.is_file()]
    if missing:
        raise ValueError(f"build workspace is missing declared inputs: {missing}")
    return paths


def exact_record_set(
    actual: dict[str, Any], expected: set[str], label: str, errors: list[str]
) -> None:
    actual_set = set(actual)
    missing = sorted(expected - actual_set)
    extra = sorted(actual_set - expected)
    if missing or extra:
        errors.append(f"{label} record set mismatch: missing={missing} extra={extra}")


def expected_artifact_relatives(project: str) -> set[str]:
    return {f"output_files/{project}.{suffix}" for suffix in ARTIFACT_SUFFIXES}


def create_source_manifest(repo: Path, build: Path, profile_name: str) -> dict[str, Any]:
    profile = PROFILES[profile_name]
    commit = marker_commit(build, profile["marker"])
    head = git_output(repo, "rev-parse", "HEAD")
    if head != commit:
        raise ValueError(f"build marker commit {commit} differs from HEAD {head}")
    owned = (*profile["sourcePaths"], *profile["verificationPaths"])
    dirty = git_output(repo, "-c", "core.autocrlf=true", "status", "--porcelain", "--", *owned)
    if dirty:
        raise ValueError(f"manifest source is dirty at {commit}:\n{dirty}")

    source_files = expand_files(repo, profile["sourcePaths"])
    verification_files = expand_files(repo, profile["verificationPaths"])
    build_inputs = build_input_paths(repo, build, profile)
    patched = file_record(build / "sys" / "sys_top.v")
    if patched["sha256"] != PATCHED_SYS_TOP_SHA256:
        raise ValueError(
            "build-copy sys_top safety digest mismatch: "
            f"{patched['sha256']} != {PATCHED_SYS_TOP_SHA256}"
        )

    data: dict[str, Any] = {
        "schema": "zhaozhou.superstation.build-manifest.v1",
        "phase": "source",
        "status": "source-captured",
        "profile": profile_name,
        "project": profile["project"],
        "sourceCommit": commit,
        "sourceFiles": records(repo, source_files),
        "verificationFiles": records(repo, verification_files),
        "buildInputs": records(build, build_inputs),
        "patchedSysTop": patched,
        "artifacts": {},
    }
    data["manifestSha256"] = manifest_digest(data)
    return data


def create_complete_manifest(
    repo: Path, build: Path, profile_name: str, source_manifest: Path
) -> dict[str, Any]:
    source = json.loads(source_manifest.read_text(encoding="utf-8"))
    errors = verify_manifest_data(source, repo, build, require_complete=False)
    if errors:
        raise ValueError("source manifest failed before completion: " + "; ".join(errors))
    if source.get("profile") != profile_name:
        raise ValueError("source manifest profile mismatch")

    output = build / "output_files"
    if not output.is_dir():
        raise ValueError(f"missing Quartus output directory: {output}")
    project = PROFILES[profile_name]["project"]
    expected_artifacts = expected_artifact_relatives(project)
    observed_artifacts = {
        path.relative_to(build).as_posix()
        for path in output.rglob("*")
        if path.is_file()
    }
    missing = sorted(expected_artifacts - observed_artifacts)
    extra = sorted(observed_artifacts - expected_artifacts)
    if missing or extra:
        raise ValueError(
            f"Quartus output record set mismatch: missing={missing} extra={extra}"
        )
    artifact_files = [build / relative for relative in sorted(expected_artifacts)]
    artifact_records = records(build, artifact_files)

    data = dict(source)
    data["phase"] = "complete"
    data["status"] = "candidate"
    data["sourceManifestSha256"] = source["manifestSha256"]
    data["artifacts"] = artifact_records
    data["manifestSha256"] = manifest_digest(data)
    return data


def compare_records(
    root: Path, expected: dict[str, dict[str, Any]], label: str, errors: list[str]
) -> None:
    for relative, record in expected.items():
        path = root / relative
        if not path.is_file():
            errors.append(f"{label} missing: {relative}")
            continue
        actual = file_record(path)
        if actual != record:
            errors.append(f"{label} hash/size mismatch: {relative}")


def verify_manifest_data(
    data: dict[str, Any], repo: Path, build: Path, *, require_complete: bool = True
) -> list[str]:
    errors: list[str] = []
    if data.get("schema") != "zhaozhou.superstation.build-manifest.v1":
        errors.append("manifest schema mismatch")
    profile_name = data.get("profile")
    if profile_name not in PROFILES:
        errors.append(f"unknown manifest profile: {profile_name!r}")
        return errors
    profile = PROFILES[profile_name]
    if data.get("project") != profile["project"]:
        errors.append("manifest project/profile mismatch")
    if data.get("manifestSha256") != manifest_digest(data):
        errors.append("manifest self-digest mismatch")

    expected_sources = relative_file_set(repo, profile["sourcePaths"])
    expected_verification = relative_file_set(repo, profile["verificationPaths"])
    expected_inputs = expected_build_input_relatives(repo, profile)
    exact_record_set(data.get("sourceFiles", {}), expected_sources, "source", errors)
    exact_record_set(
        data.get("verificationFiles", {}),
        expected_verification,
        "verification",
        errors,
    )
    exact_record_set(data.get("buildInputs", {}), expected_inputs, "build input", errors)

    if data.get("sourceCommit") != marker_commit(build, profile["marker"]):
        errors.append("manifest/build-marker source commit mismatch")
    source_commit = data.get("sourceCommit")
    try:
        git_output(repo, "cat-file", "-e", f"{source_commit}^{{commit}}")
    except ValueError as exc:
        errors.append(str(exc))
    else:
        owned = (*profile["sourcePaths"], *profile["verificationPaths"])
        try:
            git_output(
                repo,
                "-c",
                "core.autocrlf=true",
                "diff",
                "--quiet",
                str(source_commit),
                "--",
                *owned,
            )
        except ValueError:
            errors.append("manifest source/verification files differ from sourceCommit")

    compare_records(repo, data.get("sourceFiles", {}), "source", errors)
    compare_records(repo, data.get("verificationFiles", {}), "verification", errors)
    compare_records(build, data.get("buildInputs", {}), "build input", errors)
    patched = data.get("patchedSysTop", {})
    if not isinstance(patched, dict):
        errors.append("manifest patched sys_top record is invalid")
        patched = {}
    patched_path = build / "sys" / "sys_top.v"
    if not patched_path.is_file():
        errors.append("patched sys_top missing from build")
    else:
        actual_patched = file_record(patched_path)
        if patched != actual_patched:
            errors.append("manifest patched sys_top record mismatch")
        if data.get("buildInputs", {}).get("sys/sys_top.v") != actual_patched:
            errors.append("build-input sys_top record mismatch")
    if patched.get("sha256") != PATCHED_SYS_TOP_SHA256:
        errors.append("manifest patched sys_top digest mismatch")

    if require_complete:
        if data.get("phase") != "complete" or data.get("status") != "candidate":
            errors.append("manifest is not a complete candidate")
        expected_artifacts = expected_artifact_relatives(profile["project"])
        exact_record_set(data.get("artifacts", {}), expected_artifacts, "artifact", errors)
        compare_records(build, data.get("artifacts", {}), "artifact", errors)

        source_digest = data.get("sourceManifestSha256")
        source_projection = dict(data)
        source_projection.pop("sourceManifestSha256", None)
        source_projection["phase"] = "source"
        source_projection["status"] = "source-captured"
        source_projection["artifacts"] = {}
        expected_source_digest = manifest_digest(source_projection)
        if source_digest != expected_source_digest:
            errors.append(
                "sourceManifestSha256 mismatch: "
                f"expected {expected_source_digest}, got {source_digest}"
            )
    return errors


def write_manifest(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    create = sub.add_parser("create")
    create.add_argument("--profile", choices=sorted(PROFILES), required=True)
    create.add_argument("--phase", choices=("source", "complete"), required=True)
    create.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    create.add_argument("--build-dir", type=Path, required=True)
    create.add_argument("--source-manifest", type=Path)
    create.add_argument("--output", type=Path, required=True)

    verify = sub.add_parser("verify")
    verify.add_argument("--manifest", type=Path, required=True)
    verify.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    verify.add_argument("--build-dir", type=Path, required=True)
    verify.add_argument("--json", action="store_true")

    args = parser.parse_args()
    try:
        repo = args.repo.resolve()
        build = args.build_dir.resolve()
        if args.command == "create":
            if args.phase == "source":
                data = create_source_manifest(repo, build, args.profile)
            else:
                if args.source_manifest is None:
                    raise ValueError("--source-manifest is required for complete phase")
                data = create_complete_manifest(repo, build, args.profile, args.source_manifest.resolve())
            write_manifest(args.output.resolve(), data)
            print(json.dumps({"status": data["status"], "manifestSha256": data["manifestSha256"]}))
            return 0

        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        errors = verify_manifest_data(data, repo, build)
        result = {"status": "ok" if not errors else "failed", "errors": errors}
        if args.json:
            print(json.dumps(result, indent=2))
        elif errors:
            for error in errors:
                print(f"ERROR: {error}", file=sys.stderr)
        else:
            print(f"SuperStation complete manifest verified: {data['manifestSha256']}")
        return 1 if errors else 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"SUPERSTATION_MANIFEST_FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
