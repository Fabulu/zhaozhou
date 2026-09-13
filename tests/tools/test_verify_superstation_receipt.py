from __future__ import annotations

import copy
import importlib.util
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
VERIFY_PATH = REPO / "tools" / "board" / "verify_superstation_receipt.py"
SPEC = importlib.util.spec_from_file_location("verify_superstation_receipt", VERIFY_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
IDENTITY_STATE = {
    "utc": "2026-09-13T20:00:00Z",
    "core": "MENU",
    "rbf": "MENU",
    "fpga": "operating",
    "bridge:lwhps2fpga": "enabled",
    "bridge:hps2fpga": "enabled",
    "bridge:fpga2hps": "enabled",
    "mister_pid": "111",
}
IDENTITY = {
    "schema": "zhaozhou.superstation.load-receipt.v1",
    "sourceCommit": "a" * 40,
    "loaderSource": {
        "path": "tools/board/invoke_superstation_probe.ps1",
        "sourceCommit": "a" * 40,
        "gitBlob": "b" * 40,
        "workingSha256": "c" * 64,
    },
    "mode": "identity-preflight",
    "status": "ok",
    "error": None,
    "sshHostKey": {
        "host": "192.168.178.59",
        "algorithm": "ssh-ed25519",
        "fingerprint": VERIFY.EXPECTED_FINGERPRINT,
    },
    "boardIdentity": copy.deepcopy(VERIFY.EXPECTED_BOARD),
    "before": [f"{key}={value}" for key, value in IDENTITY_STATE.items()],
    "beforeState": copy.deepcopy(IDENTITY_STATE),
    "loaded": [],
    "loadedState": None,
    "afterRollback": [],
    "afterRollbackState": None,
    "localRbf": None,
    "remoteRbf": None,
    "buildSourceCommit": None,
    "buildAudit": None,
    "buildManifest": None,
    "menuRbf": {
        "expectedSha256": VERIFY.EXPECTED_BOARD["menuSha256"],
        "observedSha256": VERIFY.EXPECTED_BOARD["menuSha256"],
    },
    "watchdog": None,
    "watchdogLog": [],
    "watchdogEvidence": None,
    "rollbackAttempted": False,
    "rollbackSucceeded": False,
    "stagedFileRemoved": False,
}


class SuperStationReceiptTest(unittest.TestCase):
    def future_receipt(self) -> dict:
        data = copy.deepcopy(IDENTITY)
        build_commit = "d" * 40
        rbf_sha = "e" * 64
        loaded_state = {
            **copy.deepcopy(data["beforeState"]),
            "core": "Zhaozhou Hardware Specs",
            "rbf": "Zhaozhou Hardware Specs",
            "mister_pid": "222",
        }
        rollback_state = {
            **copy.deepcopy(data["beforeState"]),
            "mister_pid": "333",
        }
        data.update(
            {
                "profile": "Specs",
                "mode": "watchdog-fire-test",
                "loaded": [f"{key}={value}" for key, value in loaded_state.items()],
                "loadedState": loaded_state,
                "afterRollback": [f"{key}={value}" for key, value in rollback_state.items()],
                "afterRollbackState": rollback_state,
                "rollbackAttempted": True,
                "rollbackSucceeded": True,
                "stagedFileRemoved": True,
                "buildSourceCommit": build_commit,
                "buildManifest": {
                    "path": "HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
                    "manifestSha256": "f" * 64,
                    "status": "candidate",
                    "sourceCommit": build_commit,
                    "rbfSha256": rbf_sha,
                },
                "buildAudit": {
                    "path": "HARDWARE-SPECS-BUILD-AUDIT-V2.json",
                    "status": "ok",
                    "sourceCommit": build_commit,
                    "rbfSha256": rbf_sha,
                },
                "localRbf": {
                    "path": "C:/build/ZhaozhouSpecs.rbf",
                    "bytes": 1234,
                    "sha256": rbf_sha,
                },
                "remoteRbf": f"/media/fat/_Utility/ZhaozhouSpecs-{rbf_sha[:12]}.rbf",
                "watchdog": {
                    "pid": 444,
                    "token": f"/tmp/zhaozhou-rollback-{rbf_sha[:12]}.armed",
                    "log": f"/tmp/zhaozhou-rollback-{rbf_sha[:12]}.log",
                    "script": f"/tmp/zhaozhou-rollback-{rbf_sha[:12]}.sh",
                    "delaySeconds": 10,
                },
                "watchdogLog": [
                    "watchdog-fired=2026-09-13T20:00:00Z",
                    "watchdog-attempt=1",
                    "watchdog-write-ok=1",
                    "watchdog-menu-ok=1",
                ],
                "watchdogEvidence": {
                    "firedUtc": "2026-09-13T20:00:00Z",
                    "attempts": [1],
                    "successfulWriteAttempt": 1,
                    "menuVerifiedAttempt": 1,
                },
            }
        )
        return data

    def test_complete_identity_preflight_passes(self) -> None:
        self.assertEqual(VERIFY.validate_identity_receipt(copy.deepcopy(IDENTITY)), [])

    def test_complete_future_receipt_passes(self) -> None:
        self.assertEqual(VERIFY.validate_future_load_receipt(self.future_receipt()), [])

    def test_host_fingerprint_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["sshHostKey"]["fingerprint"] = "SHA256:wrong"
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("SSH fingerprint mismatch" in error for error in errors))

    def test_bridge_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["loadedState"]["bridge:fpga2hps"] = "disabled"
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("bridge:fpga2hps not enabled" in error for error in errors))

    def test_watchdog_menu_evidence_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["watchdogLog"].remove("watchdog-menu-ok=1")
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("exact contiguous" in error for error in errors))

    def test_rollback_attempt_false_fires(self) -> None:
        data = self.future_receipt()
        data["rollbackAttempted"] = False
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("rollbackAttempted is not true" in error for error in errors))

    def test_empty_raw_load_and_rollback_arrays_fire(self) -> None:
        data = self.future_receipt()
        data["loaded"] = []
        data["afterRollback"] = []
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("loaded: raw state lines are empty" in error for error in errors))
        self.assertTrue(any("after rollback: raw state lines are empty" in error for error in errors))

    def test_null_remote_rbf_and_build_commit_fire(self) -> None:
        data = self.future_receipt()
        data["remoteRbf"] = None
        data["buildSourceCommit"] = None
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("remoteRbf is null" in error for error in errors))
        self.assertTrue(any("buildSourceCommit is null" in error for error in errors))

    def test_manifest_source_binding_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["buildManifest"]["sourceCommit"] = "0" * 40
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("manifest/build source binding mismatch" in error for error in errors))

    def test_noncontiguous_watchdog_attempts_fire(self) -> None:
        data = self.future_receipt()
        data["watchdogLog"] = [
            "watchdog-fired=2026-09-13T20:00:00Z",
            "watchdog-attempt=1",
            "watchdog-write-timeout=1",
            "watchdog-attempt=3",
            "watchdog-write-ok=3",
            "watchdog-menu-ok=3",
        ]
        data["watchdogEvidence"].update(
            {"attempts": [1, 3], "successfulWriteAttempt": 3, "menuVerifiedAttempt": 3}
        )
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("exact contiguous" in error for error in errors))
        self.assertTrue(any("summary attempts are not contiguous" in error for error in errors))

    def test_old_manifest_name_fires(self) -> None:
        data = self.future_receipt()
        data["buildManifest"]["path"] = "HARDWARE-SPECS-BUILD-MANIFEST.json"
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("V2 complete manifest path mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
