#!/usr/bin/env python3
"""Validate reports/board_truth.json without turning unknowns into assumptions."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

EXPECTED_OPEN = {
    "pcb_revision",
    "fpga_top_marking",
    "current_usb_pd_contract",
    "measured_power_rails",
    "physical_oscillator_markings",
    "physical_button_reset_mapping",
    "fpga_sdram_component_and_geometry",
    "fpga_sdram_timing_and_bandwidth",
    "hps_ddr_fabric_interface",
    "audio",
    "controller_input",
    "gpio_user_port_snac",
    "analog_video",
    "external_io_timing",
    "jtag_chain",
    "configuration_flash",
    "persistent_boot",
    "full_zhaozhou_shell",
}


def at(data: dict[str, Any], path: str) -> Any:
    value: Any = data
    for part in path.split("."):
        if not isinstance(value, dict) or part not in value:
            raise KeyError(path)
        value = value[part]
    return value


def require_equal(data: dict[str, Any], path: str, expected: Any, errors: list[str]) -> None:
    try:
        actual = at(data, path)
    except KeyError:
        errors.append(f"missing field: {path}")
        return
    if actual != expected:
        errors.append(f"{path}: expected {expected!r}, got {actual!r}")


def validate(data: dict[str, Any], repo: Path) -> list[str]:
    errors: list[str] = []
    require_equal(data, "schemaVersion", 1, errors)
    require_equal(data, "status", "partial", errors)
    require_equal(data, "identity.pcbRevision", None, errors)
    require_equal(data, "identity.fpgaTopMarking", None, errors)
    require_equal(data, "fpga.buildTarget", "5CSEBA6U23I7", errors)
    require_equal(data, "fpga.compatibilityStatus", "physically_proven_for_mister_rbf", errors)
    require_equal(data, "fpga.physicalMarkingStatus", "unread", errors)
    require_equal(data, "fpga.volatileLoad.status", "historical_proof_future_loads_held", errors)
    require_equal(data, "fpga.volatileLoad.configurationFlashTouched", False, errors)
    require_equal(data, "fpga.volatileLoad.jtagUsed", False, errors)
    require_equal(data, "reviewHold.futurePhysicalLoadsAuthorized", False, errors)
    require_equal(data, "jtag.status", "no_hardware_available", errors)
    require_equal(data, "clocks.hpsOsc1.frequencyHz", 25_000_000, errors)
    require_equal(data, "memory.fpgaSdram.timingStatus", "unmeasured", errors)
    require_equal(data, "memory.fpgaSdram.bandwidthStatus", "unmeasured", errors)
    require_equal(data, "memory.fpgaSdram.physicalTransactionStatus", "untested", errors)
    require_equal(data, "memory.hpsDdr.fabricInterfaceStatus", "untested", errors)
    require_equal(data, "network.ssh.status", "pinned_read_only_identity_preflight_passed", errors)
    require_equal(
        data,
        "network.ssh.ed25519Fingerprint",
        "SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU",
        errors,
    )
    require_equal(
        data,
        "network.ssh.loaderSourceCommit",
        "bd73428561da57d373c9da1ea449edbc444dc907",
        errors,
    )
    require_equal(
        data,
        "network.ssh.loaderGitBlob",
        "98fbb8f856e18cbb5faab01fee085399062fff00",
        errors,
    )
    require_equal(data, "physicalCapabilities.selectedEngineBlockVectors.signature", "e5f1c57f", errors)
    require_equal(data, "physicalCapabilities.selectedEngineBlockVectors.vectorCount", 16, errors)

    clocks = data.get("clocks", {}).get("fpgaInputs", [])
    expected_clocks = {
        "FPGA_CLK1_50": (50_000_000, "V11", "3.3-V LVTTL"),
        "FPGA_CLK2_50": (50_000_000, "Y13", "3.3-V LVTTL"),
        "FPGA_CLK3_50": (50_000_000, "E11", "3.3-V LVTTL"),
    }
    actual_clocks = {
        item.get("name"): (
            item.get("frequencyHz"),
            item.get("pin"),
            item.get("ioStandard"),
        )
        for item in clocks
        if isinstance(item, dict)
    }
    if actual_clocks != expected_clocks:
        errors.append(f"clocks.fpgaInputs mismatch: {actual_clocks!r}")

    open_capabilities = set(data.get("openCapabilities", []))
    if open_capabilities != EXPECTED_OPEN:
        errors.append(
            "openCapabilities mismatch: missing=%s extra=%s"
            % (sorted(EXPECTED_OPEN - open_capabilities), sorted(open_capabilities - EXPECTED_OPEN))
        )

    evidence = data.get("evidence", {})
    if not isinstance(evidence, dict):
        errors.append("evidence must be an object")
        return errors
    for name, relative in evidence.items():
        if name == "gitCommit":
            continue
        if not isinstance(relative, str) or not (repo / relative).is_file():
            errors.append(f"evidence.{name}: missing file {relative!r}")

    commit = evidence.get("gitCommit")
    if not isinstance(commit, str) or len(commit) != 40:
        errors.append("evidence.gitCommit must be a full 40-hex commit")
    else:
        result = subprocess.run(
            ["git", "-C", str(repo), "cat-file", "-e", f"{commit}^{{commit}}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if result.returncode:
            errors.append(f"evidence.gitCommit is not present: {commit}")

    identity_path = evidence.get("identityPreflight")
    if isinstance(identity_path, str) and (repo / identity_path).is_file():
        identity = json.loads((repo / identity_path).read_text(encoding="utf-8"))
        require_equal(identity, "mode", "identity-preflight", errors)
        require_equal(identity, "status", "ok", errors)
        require_equal(
            identity,
            "sourceCommit",
            data.get("network", {}).get("ssh", {}).get("loaderSourceCommit"),
            errors,
        )
        require_equal(
            identity,
            "loaderSource.gitBlob",
            data.get("network", {}).get("ssh", {}).get("loaderGitBlob"),
            errors,
        )
        require_equal(
            identity,
            "sshHostKey.fingerprint",
            data.get("network", {}).get("ssh", {}).get("ed25519Fingerprint"),
            errors,
        )
        require_equal(identity, "loaded", [], errors)
        require_equal(identity, "remoteRbf", None, errors)
    else:
        errors.append("source-bound identity preflight evidence is missing")

    build_path = evidence.get("specBuildAudit")
    load_path = evidence.get("specLoadReceipt")
    if isinstance(build_path, str) and (repo / build_path).is_file():
        build = json.loads((repo / build_path).read_text(encoding="utf-8"))
        require_equal(build, "status", "historical-invalidated", errors)
        require_equal(build, "reviewHold.loadAuthorization", False, errors)
        require_equal(build, "reviewHold.greenVectorResultCredible", True, errors)
        require_equal(build, "flow.criticalWarnings", 1, errors)
        require_equal(build, "expectedSignature", "e5f1c57f", errors)
        require_equal(build, "physicalHierarchy.zhao_dual18_mul:u_mul.dspBlocks", 1, errors)
        require_equal(build, "flow.virtualPins", 0, errors)
    else:
        build = None

    if isinstance(load_path, str) and (repo / load_path).is_file():
        load = json.loads((repo / load_path).read_text(encoding="utf-8"))
        require_equal(load, "profile", "Specs", errors)
        require_equal(load, "status", "ok", errors)
        require_equal(load, "rollbackSucceeded", True, errors)
        require_equal(load, "stagedFileRemoved", True, errors)
        require_equal(load, "ownerVisualObservation.display", "green signature bands", errors)
        if build is not None:
            build_rbf = build.get("artifacts", {}).get("ZhaozhouSpecs.rbf", {}).get("sha256")
            load_rbf = load.get("localRbf", {}).get("sha256")
            if build_rbf != load_rbf:
                errors.append(f"spec build/load RBF mismatch: {build_rbf!r} != {load_rbf!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    repo = args.repo.resolve()
    path = repo / "reports" / "board_truth.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    errors = validate(data, repo)
    if args.json:
        print(json.dumps({"status": "ok" if not errors else "failed", "errors": errors}, indent=2))
    elif errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
    else:
        print("board_truth validation passed: status=partial unknowns remain explicit")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
