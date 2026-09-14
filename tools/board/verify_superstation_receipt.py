#!/usr/bin/env python3
"""Validate pinned SuperStation identity and future guarded-load receipts."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

EXPECTED_FINGERPRINT = "SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU"
EXPECTED_BOARD = {
    "hostname": "MiSTer",
    "model": "Terasic DE10-nano",
    "compatible": "altr,socfpga-cyclone5 altr,socfpga",
    "ethernetMac": "ce:cd:87:14:8d:44",
    "hpsSiliconId1": "0x00000003",
    "misterSha256": "9f6e5a237c36be6404ab4823d804821491db4bf125827f84aca2a1ca31f0a8a6",
    "menuSha256": "25d5461b55e4d45e79c876a02d69f32b22f414b64e600a1adc930eefea6ea4a7",
}
EXPECTED_BRIDGES = {
    "bridge:lwhps2fpga": "enabled",
    "bridge:hps2fpga": "enabled",
    "bridge:fpga2hps": "enabled",
}
PROFILES = {
    "Bringup": {
        "core": "Zhaozhou Board Bring-up",
        "project": "ZhaozhouBringup",
        "manifest": "BRINGUP-BUILD-MANIFEST-V2.json",
        "manifestRelative": "runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/BRINGUP-BUILD-MANIFEST-V2.json",
        "audit": "BRINGUP-BUILD-AUDIT-V2.json",
        "auditRelative": "runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/BRINGUP-BUILD-AUDIT-V2.json",
        "auditSchema": "zhaozhou.superstation.quartus-build-audit.v1",
        "remotePrefix": "ZhaozhouBringup",
    },
    "Specs": {
        "core": "Zhaozhou Hardware Specs",
        "project": "ZhaozhouSpecs",
        "manifest": "HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
        "manifestRelative": "runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
        "audit": "HARDWARE-SPECS-BUILD-AUDIT-V2.json",
        "auditRelative": "runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-AUDIT-V2.json",
        "auditSchema": "zhaozhou.superstation.hardware-specs-build-audit.v1",
        "remotePrefix": "ZhaozhouSpecs",
    },
}

_MANIFEST_PATH = Path(__file__).with_name("superstation_build_manifest.py")
_MANIFEST_SPEC = importlib.util.spec_from_file_location(
    "superstation_build_manifest_for_receipt", _MANIFEST_PATH
)
if _MANIFEST_SPEC is None or _MANIFEST_SPEC.loader is None:
    raise RuntimeError(f"could not load build manifest verifier: {_MANIFEST_PATH}")
BUILD_MANIFEST = importlib.util.module_from_spec(_MANIFEST_SPEC)
_MANIFEST_SPEC.loader.exec_module(BUILD_MANIFEST)


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def is_hex(value: Any, length: int) -> bool:
    return isinstance(value, str) and bool(re.fullmatch(rf"[0-9a-f]{{{length}}}", value))


def parse_raw_state(lines: Any, context: str, errors: list[str]) -> dict[str, str] | None:
    if not isinstance(lines, list) or not lines:
        errors.append(f"{context}: raw state lines are empty or not an array")
        return None
    result: dict[str, str] = {}
    for line in lines:
        match = re.fullmatch(r"([^=]+)=(.*)", str(line))
        if not match:
            errors.append(f"{context}: malformed raw state line {line!r}")
            continue
        key, value = match.groups()
        if key in result:
            errors.append(f"{context}: duplicate raw state key {key}")
        result[key] = value
    return result


def validate_state(state: Any, expected_core: str, context: str, errors: list[str]) -> None:
    require(isinstance(state, dict), f"{context}: state is not an object", errors)
    if not isinstance(state, dict):
        return
    expected_keys = {"utc", "core", "rbf", "fpga", "mister_pid", *EXPECTED_BRIDGES}
    require(set(state) == expected_keys, f"{context}: state keys mismatch", errors)
    require(bool(re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", str(state.get("utc", "")))), f"{context}: UTC malformed", errors)
    require(state.get("core") == expected_core, f"{context}: core mismatch", errors)
    require(state.get("rbf") == expected_core, f"{context}: RBF identity mismatch", errors)
    require(state.get("fpga") == "operating", f"{context}: FPGA not operating", errors)
    require(bool(re.fullmatch(r"\d+", str(state.get("mister_pid", "")))), f"{context}: MiSTer PID invalid", errors)
    for key, value in EXPECTED_BRIDGES.items():
        require(state.get(key) == value, f"{context}: {key} not enabled", errors)


def validate_state_binding(
    data: dict[str, Any], raw_key: str, parsed_key: str, expected_core: str,
    context: str, errors: list[str]
) -> None:
    parsed = data.get(parsed_key)
    validate_state(parsed, expected_core, context, errors)
    raw = parse_raw_state(data.get(raw_key), context, errors)
    if raw is not None and isinstance(parsed, dict):
        require(raw == parsed, f"{context}: raw and parsed state differ", errors)


def validate_loader_source(
    data: dict[str, Any], errors: list[str], repo: Path | None = None
) -> None:
    commit = data.get("sourceCommit")
    loader = data.get("loaderSource")
    require(is_hex(commit, 40), "receipt sourceCommit is not full lowercase hex", errors)
    require(isinstance(loader, dict), "loaderSource is not an object", errors)
    if not is_hex(commit, 40) or not isinstance(loader, dict):
        return
    require(
        set(loader) == {"path", "sourceCommit", "gitBlob", "workingSha256"},
        "loaderSource keys mismatch",
        errors,
    )
    require(loader.get("path") == "tools/board/invoke_superstation_probe.ps1", "loaderSource path mismatch", errors)
    require(loader.get("sourceCommit") == commit, "loaderSource/sourceCommit mismatch", errors)
    require(is_hex(loader.get("gitBlob"), 40), "loaderSource gitBlob invalid", errors)
    require(is_hex(loader.get("workingSha256"), 64), "loaderSource workingSha256 invalid", errors)
    if repo is not None and is_hex(loader.get("gitBlob"), 40):
        repo = repo.resolve()
        loader_path = (repo / str(loader.get("path"))).resolve()
        require(loader_path.is_relative_to(repo), "loaderSource path escapes repository", errors)
        require(loader_path.is_file(), "loaderSource working file is absent", errors)
        if loader_path.is_file():
            working_sha = hashlib.sha256(loader_path.read_bytes()).hexdigest()
            require(working_sha == loader.get("workingSha256"), "loaderSource working SHA-256 mismatch", errors)
        clean = subprocess.run(
            ["git", "-C", str(repo), "status", "--porcelain", "--", str(loader.get("path"))],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        require(clean.returncode == 0 and not clean.stdout.strip(), "loaderSource working file is not clean", errors)
        completed = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", f"{commit}:tools/board/invoke_superstation_probe.ps1"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        require(completed.returncode == 0, "loaderSource commit/path is absent", errors)
        if completed.returncode == 0:
            require(completed.stdout.strip() == loader.get("gitBlob"), "loaderSource git blob mismatch", errors)


def validate_watchdog(data: dict[str, Any], errors: list[str]) -> None:
    lines = data.get("watchdogLog")
    evidence = data.get("watchdogEvidence")
    require(isinstance(lines, list) and bool(lines), "watchdogLog is empty or not an array", errors)
    require(isinstance(evidence, dict), "watchdogEvidence is not an object", errors)
    if not isinstance(lines, list) or not lines or not isinstance(evidence, dict):
        return
    require(
        set(evidence) == {"firedUtc", "attempts", "successfulWriteAttempt", "menuVerifiedAttempt"},
        "watchdogEvidence keys mismatch",
        errors,
    )
    fired_match = re.fullmatch(
        r"watchdog-fired=(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)",
        str(lines[0]),
    )
    require(fired_match is not None, "watchdog fired evidence missing or not first", errors)
    success = evidence.get("successfulWriteAttempt")
    menu = evidence.get("menuVerifiedAttempt")
    require(isinstance(success, int) and 1 <= success <= 5, "watchdog successful attempt invalid", errors)
    require(menu == success, "watchdog write/MENU attempts differ", errors)
    if not isinstance(success, int) or not 1 <= success <= 5:
        return
    expected_lines = [lines[0]]
    for attempt in range(1, success + 1):
        expected_lines.append(f"watchdog-attempt={attempt}")
        if attempt < success:
            expected_lines.append(f"watchdog-write-timeout={attempt}")
        else:
            expected_lines.append(f"watchdog-write-ok={attempt}")
            expected_lines.append(f"watchdog-menu-ok={attempt}")
    require(lines == expected_lines, "watchdog log is not the exact contiguous fired/attempt/write/MENU sequence", errors)
    require(evidence.get("attempts") == list(range(1, success + 1)), "watchdog summary attempts are not contiguous", errors)
    if fired_match is not None:
        require(evidence.get("firedUtc") == fired_match.group(1), "watchdog summary fired UTC mismatch", errors)


def validate_identity_receipt(
    data: dict[str, Any], repo: Path | None = None
) -> list[str]:
    errors: list[str] = []
    require(data.get("schema") == "zhaozhou.superstation.load-receipt.v1", "schema mismatch", errors)
    require(data.get("mode") == "identity-preflight", "not an identity preflight", errors)
    require(data.get("status") == "ok" and data.get("error") is None, "identity preflight did not pass", errors)
    validate_loader_source(data, errors, repo)
    host_key = data.get("sshHostKey", {})
    require(host_key.get("algorithm") == "ssh-ed25519", "SSH algorithm mismatch", errors)
    require(host_key.get("fingerprint") == EXPECTED_FINGERPRINT, "SSH fingerprint mismatch", errors)
    require(bool(host_key.get("host")), "SSH host missing", errors)
    require(data.get("boardIdentity") == EXPECTED_BOARD, "board identity mismatch", errors)
    validate_state_binding(data, "before", "beforeState", "MENU", "identity preflight", errors)
    require(data.get("loaded") == [] and data.get("loadedState") is None, "identity preflight loaded a core", errors)
    require(data.get("afterRollback") == [] and data.get("afterRollbackState") is None, "identity preflight has rollback state", errors)
    require(data.get("localRbf") is None and data.get("remoteRbf") is None, "identity preflight has an RBF", errors)
    require(data.get("buildSourceCommit") is None, "identity preflight has buildSourceCommit", errors)
    require(data.get("buildAudit") is None and data.get("buildManifest") is None, "identity preflight has build authorization", errors)
    require(data.get("watchdog") is None and data.get("watchdogEvidence") is None, "identity preflight has watchdog state", errors)
    require(data.get("watchdogLog") == [], "identity preflight has watchdog log", errors)
    require(data.get("rollbackAttempted") is False, "identity preflight attempted rollback", errors)
    require(data.get("rollbackSucceeded") is False, "identity preflight claims rollback", errors)
    require(data.get("stagedFileRemoved") is False, "identity preflight claims staged-file cleanup", errors)
    menu = data.get("menuRbf", {})
    require(menu.get("expectedSha256") == EXPECTED_BOARD["menuSha256"], "identity menu expected hash mismatch", errors)
    require(menu.get("observedSha256") == EXPECTED_BOARD["menuSha256"], "identity menu observed hash mismatch", errors)
    return errors


def validate_future_load_structure(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    require(data.get("schema") == "zhaozhou.superstation.load-receipt.v1", "schema mismatch", errors)
    profile_name = data.get("profile")
    require(profile_name in PROFILES, "unknown load profile", errors)
    if profile_name not in PROFILES:
        return errors
    profile = PROFILES[profile_name]
    expected_core = profile["core"]
    project = profile["project"]

    require(data.get("status") == "ok" and data.get("error") is None, "load receipt did not pass", errors)
    require(data.get("mode") == "watchdog-fire-test", "future load did not exercise watchdog", errors)
    validate_loader_source(data, errors)

    host_key = data.get("sshHostKey", {})
    require(host_key.get("algorithm") == "ssh-ed25519", "SSH algorithm mismatch", errors)
    require(host_key.get("fingerprint") == EXPECTED_FINGERPRINT, "SSH fingerprint mismatch", errors)
    require(bool(host_key.get("host")), "SSH host missing", errors)
    require(data.get("boardIdentity") == EXPECTED_BOARD, "board identity mismatch", errors)
    validate_state_binding(data, "before", "beforeState", "MENU", "before load", errors)
    validate_state_binding(data, "loaded", "loadedState", expected_core, "loaded", errors)
    validate_state_binding(data, "afterRollback", "afterRollbackState", "MENU", "after rollback", errors)

    require(data.get("rollbackAttempted") is True, "rollbackAttempted is not true", errors)
    require(data.get("rollbackSucceeded") is True, "rollback did not succeed", errors)
    require(data.get("stagedFileRemoved") is True, "staged RBF was not removed", errors)

    build_commit = data.get("buildSourceCommit")
    require(is_hex(build_commit, 40), "buildSourceCommit is null or invalid", errors)
    local_rbf = data.get("localRbf")
    require(isinstance(local_rbf, dict), "localRbf is null or invalid", errors)
    if not isinstance(local_rbf, dict):
        local_rbf = {}
    local_sha = local_rbf.get("sha256")
    require(is_hex(local_sha, 64), "local RBF SHA-256 invalid", errors)
    require(isinstance(local_rbf.get("bytes"), int) and local_rbf.get("bytes", 0) > 0, "local RBF size invalid", errors)
    require(str(local_rbf.get("path", "")).endswith(f"{project}.rbf"), "profile RBF path mismatch", errors)

    remote_rbf = data.get("remoteRbf")
    require(isinstance(remote_rbf, str) and bool(remote_rbf), "remoteRbf is null", errors)
    if is_hex(local_sha, 64):
        expected_remote = f"/media/fat/_Utility/{profile['remotePrefix']}-{local_sha[:12]}.rbf"
        require(remote_rbf == expected_remote, "remoteRbf path/hash/profile mismatch", errors)

    manifest = data.get("buildManifest")
    audit = data.get("buildAudit")
    require(isinstance(manifest, dict), "buildManifest is null", errors)
    require(isinstance(audit, dict), "buildAudit is null", errors)
    if not isinstance(manifest, dict):
        manifest = {}
    if not isinstance(audit, dict):
        audit = {}
    require(Path(str(manifest.get("path", ""))).name == profile["manifest"], "V2 complete manifest path mismatch", errors)
    require(manifest.get("status") == "candidate", "complete manifest is not a candidate", errors)
    require(is_hex(manifest.get("manifestSha256"), 64), "complete manifest self-digest invalid", errors)
    require(manifest.get("sourceCommit") == build_commit, "manifest/build source binding mismatch", errors)
    require(manifest.get("rbfSha256") == local_sha, "manifest/local RBF mismatch", errors)
    require(Path(str(audit.get("path", ""))).name == profile["audit"], "V2 build audit path mismatch", errors)
    require(audit.get("status") == "ok", "V2 build audit not ok", errors)
    require(audit.get("sourceCommit") == build_commit, "audit/build source binding mismatch", errors)
    require(audit.get("rbfSha256") == local_sha, "audit/local RBF mismatch", errors)

    menu = data.get("menuRbf", {})
    require(menu.get("expectedSha256") == EXPECTED_BOARD["menuSha256"], "MENU expected hash mismatch", errors)
    require(menu.get("observedSha256") == EXPECTED_BOARD["menuSha256"], "MENU observed hash mismatch", errors)

    watchdog = data.get("watchdog")
    require(isinstance(watchdog, dict), "watchdog is null", errors)
    if isinstance(watchdog, dict):
        require(set(watchdog) == {"pid", "token", "log", "script", "delaySeconds"}, "watchdog keys mismatch", errors)
        require(isinstance(watchdog.get("pid"), int) and watchdog.get("pid", 0) > 0, "watchdog PID invalid", errors)
        require(isinstance(watchdog.get("delaySeconds"), int) and watchdog.get("delaySeconds", 0) >= 5, "watchdog delay invalid", errors)
        if is_hex(local_sha, 64):
            identifier = local_sha[:12]
            require(watchdog.get("token") == f"/tmp/zhaozhou-rollback-{identifier}.armed", "watchdog token mismatch", errors)
            require(watchdog.get("log") == f"/tmp/zhaozhou-rollback-{identifier}.log", "watchdog log path mismatch", errors)
            require(watchdog.get("script") == f"/tmp/zhaozhou-rollback-{identifier}.sh", "watchdog script path mismatch", errors)
    validate_watchdog(data, errors)
    return errors


def resolve_evidence_path(
    repo: Path, recorded: Any, expected_relative: str, label: str, errors: list[str]
) -> Path | None:
    expected = (repo / expected_relative).resolve()
    if not isinstance(recorded, str) or not recorded:
        errors.append(f"{label} path is null")
        return None
    actual = Path(recorded)
    if not actual.is_absolute():
        actual = repo / actual
    actual = actual.resolve()
    matches = actual == expected
    require(matches, f"{label} path mismatch: {actual} != {expected}", errors)
    require(actual.is_file(), f"{label} file does not exist: {actual}", errors)
    return actual if matches and actual.is_file() else None


def validate_evidence_git_binding(
    repo: Path,
    receipt_commit: Any,
    path: Path,
    label: str,
    errors: list[str],
) -> None:
    relative = path.relative_to(repo).as_posix()
    clean = subprocess.run(
        ["git", "-C", str(repo), "status", "--porcelain", "--", relative],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    require(clean.returncode == 0 and not clean.stdout.strip(), f"{label} is not clean", errors)
    if not is_hex(receipt_commit, 40):
        return
    committed = subprocess.run(
        ["git", "-C", str(repo), "rev-parse", f"{receipt_commit}:{relative}"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    require(committed.returncode == 0, f"{label} is absent from receipt sourceCommit", errors)
    if committed.returncode != 0:
        return
    working = subprocess.run(
        ["git", "-C", str(repo), "hash-object", "--", str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    require(
        working.returncode == 0 and working.stdout.strip() == committed.stdout.strip(),
        f"{label} working file differs from receipt sourceCommit",
        errors,
    )


def validate_future_load_receipt(
    data: dict[str, Any], repo: Path | None
) -> list[str]:
    errors = validate_future_load_structure(data)
    if repo is None:
        errors.append("repository is required for future-load evidence validation")
        return errors
    repo = repo.resolve()
    validate_loader_source(data, errors, repo)

    profile_name = data.get("profile")
    if profile_name not in PROFILES:
        return errors
    profile = PROFILES[profile_name]
    project = profile["project"]
    build_commit = data.get("buildSourceCommit")
    if is_hex(build_commit, 40):
        commit_check = subprocess.run(
            ["git", "-C", str(repo), "cat-file", "-e", f"{build_commit}^{{commit}}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        require(commit_check.returncode == 0, "buildSourceCommit does not exist", errors)

    local = data.get("localRbf") if isinstance(data.get("localRbf"), dict) else {}
    local_path_value = local.get("path")
    local_path = Path(str(local_path_value)).resolve() if local_path_value else None
    build_dir: Path | None = None
    if local_path is None:
        errors.append("local RBF path is null")
    else:
        require(local_path.is_file(), f"local RBF file does not exist: {local_path}", errors)
        build_dir = local_path.parent.parent
        expected_local = (build_dir / "output_files" / f"{project}.rbf").resolve()
        require(local_path == expected_local, "local RBF is not the profile artifact in output_files", errors)
        if local_path.is_file():
            actual_record = BUILD_MANIFEST.file_record(local_path)
            require(actual_record.get("sha256") == local.get("sha256"), "receipt/local RBF SHA-256 mismatch", errors)
            require(actual_record.get("bytes") == local.get("bytes"), "receipt/local RBF size mismatch", errors)

    manifest_summary = data.get("buildManifest") if isinstance(data.get("buildManifest"), dict) else {}
    audit_summary = data.get("buildAudit") if isinstance(data.get("buildAudit"), dict) else {}
    manifest_path = resolve_evidence_path(
        repo,
        manifest_summary.get("path"),
        profile["manifestRelative"],
        "V2 complete manifest",
        errors,
    )
    audit_path = resolve_evidence_path(
        repo,
        audit_summary.get("path"),
        profile["auditRelative"],
        "V2 build audit",
        errors,
    )

    receipt_commit = data.get("sourceCommit")
    if manifest_path is not None:
        validate_evidence_git_binding(repo, receipt_commit, manifest_path, "V2 complete manifest", errors)
    if audit_path is not None:
        validate_evidence_git_binding(repo, receipt_commit, audit_path, "V2 build audit", errors)

    manifest_data: dict[str, Any] | None = None
    if manifest_path is not None:
        try:
            parsed_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if not isinstance(parsed_manifest, dict):
                raise ValueError("top level is not an object")
            manifest_data = parsed_manifest
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            errors.append(f"V2 complete manifest cannot be parsed: {exc}")
    if manifest_data is not None:
        require(build_dir is not None, "cannot validate manifest without local build directory", errors)
        if build_dir is not None:
            try:
                manifest_errors = BUILD_MANIFEST.verify_manifest_data(manifest_data, repo, build_dir)
            except (OSError, ValueError, TypeError, AttributeError) as exc:
                errors.append(f"complete manifest validation failed: {exc}")
            else:
                errors.extend(f"complete manifest: {error}" for error in manifest_errors)
        require(manifest_data.get("profile") == profile_name, "actual manifest profile mismatch", errors)
        require(manifest_data.get("project") == project, "actual manifest project mismatch", errors)
        require(manifest_data.get("sourceCommit") == build_commit, "actual manifest source mismatch", errors)
        require(manifest_summary.get("manifestSha256") == manifest_data.get("manifestSha256"), "receipt/manifest self-digest mismatch", errors)
        artifact_key = f"output_files/{project}.rbf"
        artifacts = manifest_data.get("artifacts")
        manifest_rbf = artifacts.get(artifact_key, {}) if isinstance(artifacts, dict) else {}
        require(isinstance(manifest_rbf, dict), "actual manifest RBF record is invalid", errors)
        if not isinstance(manifest_rbf, dict):
            manifest_rbf = {}
        require(manifest_rbf.get("sha256") == local.get("sha256"), "actual manifest/local RBF SHA-256 mismatch", errors)
        require(manifest_rbf.get("bytes") == local.get("bytes"), "actual manifest/local RBF size mismatch", errors)

    audit_data: dict[str, Any] | None = None
    if audit_path is not None:
        try:
            parsed_audit = json.loads(audit_path.read_text(encoding="utf-8"))
            if not isinstance(parsed_audit, dict):
                raise ValueError("top level is not an object")
            audit_data = parsed_audit
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            errors.append(f"V2 build audit cannot be parsed: {exc}")
    if audit_data is not None:
        require(audit_data.get("schema") == profile["auditSchema"], "actual V2 audit schema mismatch", errors)
        require(audit_data.get("status") == "ok", "actual V2 audit status is not ok", errors)
        require(audit_data.get("sourceCommit") == build_commit, "actual V2 audit source mismatch", errors)
        require(audit_data.get("project") == project, "actual V2 audit project mismatch", errors)
        audit_artifacts = audit_data.get("artifacts")
        audit_rbf = audit_artifacts.get(f"{project}.rbf", {}) if isinstance(audit_artifacts, dict) else {}
        require(isinstance(audit_rbf, dict), "actual V2 audit RBF record is invalid", errors)
        if not isinstance(audit_rbf, dict):
            audit_rbf = {}
        require(audit_rbf.get("sha256") == local.get("sha256"), "actual audit/local RBF SHA-256 mismatch", errors)
        require(audit_rbf.get("bytes") == local.get("bytes"), "actual audit/local RBF size mismatch", errors)
        flow = audit_data.get("flow")
        require(isinstance(flow, dict) and flow.get("criticalWarnings") == 0, "actual V2 audit has Critical Warnings", errors)
        pin_audit = audit_data.get("pinAudit")
        require(
            isinstance(pin_audit, dict)
            and pin_audit.get("userIoDisabledOutputEnables") == list(range(7)),
            "actual V2 audit does not prove all USER_IO output enables disabled",
            errors,
        )
        require(audit_summary.get("status") == audit_data.get("status"), "receipt/audit status mismatch", errors)
        require(audit_summary.get("sourceCommit") == audit_data.get("sourceCommit"), "receipt/audit source mismatch", errors)
        require(audit_summary.get("rbfSha256") == audit_rbf.get("sha256"), "receipt/audit RBF mismatch", errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--kind", choices=("identity", "future-load"), required=True)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    data = json.loads(args.receipt.read_text(encoding="utf-8"))
    repo = args.repo.resolve()
    errors = validate_identity_receipt(data, repo) if args.kind == "identity" else validate_future_load_receipt(data, repo)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"SuperStation {args.kind} receipt verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
