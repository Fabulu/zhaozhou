#!/usr/bin/env python3
"""Validate pinned SuperStation identity and future guarded-load receipts."""

from __future__ import annotations

import argparse
import json
import re
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
    "Bringup": ("Zhaozhou Board Bring-up", "ZhaozhouBringup"),
    "Specs": ("Zhaozhou Hardware Specs", "ZhaozhouSpecs"),
}


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


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


def validate_watchdog(data: dict[str, Any], errors: list[str]) -> None:
    lines = data.get("watchdogLog")
    evidence = data.get("watchdogEvidence")
    require(isinstance(lines, list), "watchdogLog is not an array", errors)
    require(isinstance(evidence, dict), "watchdogEvidence is not an object", errors)
    if not isinstance(lines, list) or not isinstance(evidence, dict):
        return
    fired = [line for line in lines if re.fullmatch(r"watchdog-fired=\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", line)]
    writes = [line for line in lines if re.fullmatch(r"watchdog-write-ok=\d+", line)]
    menus = [line for line in lines if re.fullmatch(r"watchdog-menu-ok=\d+", line)]
    require(len(fired) == len(writes) == len(menus) == 1, "watchdog fired/write/MENU evidence incomplete", errors)
    if writes and menus:
        write_attempt = int(writes[0].split("=")[1])
        menu_attempt = int(menus[0].split("=")[1])
        require(write_attempt == menu_attempt, "watchdog write/MENU attempts differ", errors)
        require(evidence.get("successfulWriteAttempt") == write_attempt, "watchdog summary write attempt mismatch", errors)
        require(evidence.get("menuVerifiedAttempt") == menu_attempt, "watchdog summary MENU attempt mismatch", errors)
    require("watchdog-exhausted" not in lines, "watchdog exhausted", errors)


def validate_identity_receipt(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    require(data.get("schema") == "zhaozhou.superstation.load-receipt.v1", "schema mismatch", errors)
    require(data.get("mode") == "identity-preflight", "not an identity preflight", errors)
    require(data.get("status") == "ok" and data.get("error") is None, "identity preflight did not pass", errors)
    host_key = data.get("sshHostKey", {})
    require(host_key.get("algorithm") == "ssh-ed25519", "SSH algorithm mismatch", errors)
    require(host_key.get("fingerprint") == EXPECTED_FINGERPRINT, "SSH fingerprint mismatch", errors)
    require(data.get("boardIdentity") == EXPECTED_BOARD, "board identity mismatch", errors)
    validate_state(data.get("beforeState"), "MENU", "identity preflight", errors)
    require(not data.get("loaded") and data.get("loadedState") is None, "identity preflight loaded a core", errors)
    require(data.get("rollbackAttempted") is False, "identity preflight attempted rollback", errors)
    return errors


def validate_future_load_receipt(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    profile = data.get("profile")
    require(profile in PROFILES, "unknown load profile", errors)
    if profile not in PROFILES:
        return errors
    expected_core, project = PROFILES[profile]
    require(data.get("status") == "ok" and data.get("error") is None, "load receipt did not pass", errors)
    require(data.get("mode") == "watchdog-fire-test", "future load did not exercise watchdog", errors)
    host_key = data.get("sshHostKey", {})
    require(host_key.get("fingerprint") == EXPECTED_FINGERPRINT, "SSH fingerprint mismatch", errors)
    require(data.get("boardIdentity") == EXPECTED_BOARD, "board identity mismatch", errors)
    validate_state(data.get("beforeState"), "MENU", "before load", errors)
    validate_state(data.get("loadedState"), expected_core, "loaded", errors)
    validate_state(data.get("afterRollbackState"), "MENU", "after rollback", errors)
    require(data.get("rollbackSucceeded") is True, "rollback did not succeed", errors)
    require(data.get("stagedFileRemoved") is True, "staged RBF was not removed", errors)
    manifest = data.get("buildManifest", {})
    audit = data.get("buildAudit", {})
    require(manifest.get("status") == "candidate", "complete manifest is not a candidate", errors)
    require(str(manifest.get("path", "")).endswith("-BUILD-MANIFEST-V2.json"), "V2 complete manifest not pinned", errors)
    require(audit.get("status") == "ok", "V2 build audit not ok", errors)
    local_rbf = data.get("localRbf", {})
    require(manifest.get("rbfSha256") == local_rbf.get("sha256"), "manifest/local RBF mismatch", errors)
    require(str(local_rbf.get("path", "")).endswith(f"{project}.rbf"), "profile RBF path mismatch", errors)
    validate_watchdog(data, errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--kind", choices=("identity", "future-load"), required=True)
    args = parser.parse_args()
    data = json.loads(args.receipt.read_text(encoding="utf-8"))
    errors = validate_identity_receipt(data) if args.kind == "identity" else validate_future_load_receipt(data)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"SuperStation {args.kind} receipt verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
