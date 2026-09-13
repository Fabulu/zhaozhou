from __future__ import annotations

import copy
import importlib.util
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
VERIFY_PATH = REPO / "tools" / "board" / "verify_superstation_receipt.py"
SPEC = importlib.util.spec_from_file_location("verify_superstation_receipt", VERIFY_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
IDENTITY = json.loads(
    (
        REPO
        / "runs"
        / "CLAUDE-RUNS"
        / "RUN-20260913-1651-board-bringup"
        / "IDENTITY-PREFLIGHT.json"
    ).read_text("utf-8")
)


class SuperStationReceiptTest(unittest.TestCase):
    def future_receipt(self) -> dict:
        data = copy.deepcopy(IDENTITY)
        data.update(
            {
                "profile": "Specs",
                "mode": "watchdog-fire-test",
                "loadedState": {
                    **copy.deepcopy(data["beforeState"]),
                    "core": "Zhaozhou Hardware Specs",
                    "rbf": "Zhaozhou Hardware Specs",
                    "mister_pid": "222",
                },
                "afterRollbackState": {
                    **copy.deepcopy(data["beforeState"]),
                    "mister_pid": "333",
                },
                "rollbackSucceeded": True,
                "stagedFileRemoved": True,
                "buildManifest": {
                    "path": "HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
                    "status": "candidate",
                    "rbfSha256": "a" * 64,
                },
                "buildAudit": {"status": "ok"},
                "localRbf": {"path": "C:/build/ZhaozhouSpecs.rbf", "sha256": "a" * 64},
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

    def test_actual_identity_preflight_passes(self) -> None:
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
        self.assertTrue(any("watchdog fired/write/MENU evidence incomplete" in error for error in errors))

    def test_old_manifest_name_fires(self) -> None:
        data = self.future_receipt()
        data["buildManifest"]["path"] = "HARDWARE-SPECS-BUILD-MANIFEST.json"
        errors = VERIFY.validate_future_load_receipt(data)
        self.assertTrue(any("V2 complete manifest not pinned" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
