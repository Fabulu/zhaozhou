#!/usr/bin/env python3
"""Validate pinned SuperStation identity and future guarded-load receipts."""

from __future__ import annotations

import argparse
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
        "audit": "BRINGUP-BUILD-AUDIT-V2.json",
        "remotePrefix": "ZhaozhouBringup",
    },
    "Specs": {
        "core": "Zhaozhou Hardware Specs",
        "project": "ZhaozhouSpecs",
        "manifest": "HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
        "audit": "HARDWARE-SPECS-BUILD-AUDIT-V2.json",
        "remotePrefix": "ZhaozhouSpecs",
    },
}


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


def validate_future_load_receipt(
    data: dict[str, Any], repo: Path | None = None
) -> list[str]:
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
    validate_loader_source(data, errors, repo)

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
