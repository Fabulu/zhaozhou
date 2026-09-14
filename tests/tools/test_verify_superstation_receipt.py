from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import subprocess
import tempfile
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
ACTUAL_IDENTITY = json.loads(
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

    def git(self, repo: Path, *args: str) -> str:
        completed = subprocess.run(
            ["git", "-C", str(repo), *args],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        return completed.stdout.strip()

    def future_evidence_fixture(self) -> tuple[Path, dict]:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        repo = root / "repo"
        build = root / "build"
        repo.mkdir()
        build.mkdir()

        old_receipt_profile = VERIFY.PROFILES["Specs"]
        old_manifest_profile = VERIFY.BUILD_MANIFEST.PROFILES["Specs"]
        old_patched_sha = VERIFY.BUILD_MANIFEST.PATCHED_SYS_TOP_SHA256
        self.addCleanup(VERIFY.PROFILES.__setitem__, "Specs", old_receipt_profile)
        self.addCleanup(
            VERIFY.BUILD_MANIFEST.PROFILES.__setitem__, "Specs", old_manifest_profile
        )
        self.addCleanup(
            setattr,
            VERIFY.BUILD_MANIFEST,
            "PATCHED_SYS_TOP_SHA256",
            old_patched_sha,
        )
        VERIFY.PROFILES["Specs"] = {
            **old_receipt_profile,
            "manifestRelative": "evidence/HARDWARE-SPECS-BUILD-MANIFEST-V2.json",
            "auditRelative": "evidence/HARDWARE-SPECS-BUILD-AUDIT-V2.json",
        }
        VERIFY.BUILD_MANIFEST.PROFILES["Specs"] = {
            "project": "ZhaozhouSpecs",
            "marker": ".fixture-build",
            "qip": "files.qip",
            "sourcePaths": ("fpga/sys", "fpga/rtl/src.sv"),
            "verificationPaths": ("tools/board/invoke_superstation_probe.ps1",),
        }

        (repo / ".gitattributes").write_text("* -text\n", encoding="utf-8")
        (repo / "fpga" / "sys").mkdir(parents=True)
        (repo / "fpga" / "rtl").mkdir()
        (repo / "tools" / "board").mkdir(parents=True)
        (repo / "fpga" / "sys" / "sys_top.v").write_bytes(b"upstream sys top\n")
        (repo / "fpga" / "rtl" / "src.sv").write_text(
            "module src; endmodule\n", encoding="utf-8"
        )
        loader_path = repo / "tools" / "board" / "invoke_superstation_probe.ps1"
        loader_path.write_text("# fixture loader\n", encoding="utf-8")
        self.git(repo, "init")
        self.git(repo, "config", "user.name", "Receipt Fixture")
        self.git(repo, "config", "user.email", "receipt-fixture@example.invalid")
        self.git(repo, "add", ".")
        self.git(repo, "commit", "-m", "fixture source")
        build_commit = self.git(repo, "rev-parse", "HEAD")

        (build / "sys").mkdir()
        (build / "rtl").mkdir()
        patched = b"patched sys top\n"
        (build / "sys" / "sys_top.v").write_bytes(patched)
        (build / "rtl" / "src.sv").write_text(
            "module src; endmodule\n", encoding="utf-8"
        )
        for name in (
            "ZhaozhouSpecs.qpf",
            "ZhaozhouSpecs.qsf",
            "ZhaozhouSpecs.sdc",
            "files.qip",
        ):
            (build / name).write_text(name + "\n", encoding="utf-8")
        (build / ".fixture-build").write_text(
            f"sourceCommit={build_commit}\ncreated=test\n", encoding="utf-8"
        )
        VERIFY.BUILD_MANIFEST.PATCHED_SYS_TOP_SHA256 = hashlib.sha256(patched).hexdigest()
        source = VERIFY.BUILD_MANIFEST.create_source_manifest(repo, build, "Specs")
        source_path = build / "source.json"
        VERIFY.BUILD_MANIFEST.write_manifest(source_path, source)
        output = build / "output_files"
        output.mkdir()
        for suffix in VERIFY.BUILD_MANIFEST.ARTIFACT_SUFFIXES:
            (output / f"ZhaozhouSpecs.{suffix}").write_bytes(
                f"artifact:{suffix}\n".encode()
            )
        complete = VERIFY.BUILD_MANIFEST.create_complete_manifest(
            repo, build, "Specs", source_path
        )

        evidence = repo / "evidence"
        evidence.mkdir()
        manifest_path = evidence / "HARDWARE-SPECS-BUILD-MANIFEST-V2.json"
        VERIFY.BUILD_MANIFEST.write_manifest(manifest_path, complete)
        rbf_path = output / "ZhaozhouSpecs.rbf"
        rbf_record = VERIFY.BUILD_MANIFEST.file_record(rbf_path)
        audit = {
            "schema": "zhaozhou.superstation.hardware-specs-build-audit.v1",
            "status": "ok",
            "sourceCommit": build_commit,
            "project": "ZhaozhouSpecs",
            "flow": {"criticalWarnings": 0},
            "pinAudit": {"userIoDisabledOutputEnables": list(range(7))},
            "artifacts": {"ZhaozhouSpecs.rbf": rbf_record},
        }
        audit_path = evidence / "HARDWARE-SPECS-BUILD-AUDIT-V2.json"
        audit_path.write_text(
            json.dumps(audit, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        self.git(repo, "add", "evidence")
        self.git(repo, "commit", "-m", "fixture evidence")
        receipt_commit = self.git(repo, "rev-parse", "HEAD")

        data = self.future_receipt()
        rbf_sha = rbf_record["sha256"]
        data["sourceCommit"] = receipt_commit
        data["loaderSource"] = {
            "path": "tools/board/invoke_superstation_probe.ps1",
            "sourceCommit": receipt_commit,
            "gitBlob": self.git(
                repo,
                "rev-parse",
                f"{receipt_commit}:tools/board/invoke_superstation_probe.ps1",
            ),
            "workingSha256": hashlib.sha256(loader_path.read_bytes()).hexdigest(),
        }
        data["buildSourceCommit"] = build_commit
        data["localRbf"] = {"path": str(rbf_path.resolve()), **rbf_record}
        data["remoteRbf"] = f"/media/fat/_Utility/ZhaozhouSpecs-{rbf_sha[:12]}.rbf"
        data["buildManifest"] = {
            "path": str(manifest_path.resolve()),
            "manifestSha256": complete["manifestSha256"],
            "status": "candidate",
            "sourceCommit": build_commit,
            "rbfSha256": rbf_sha,
        }
        data["buildAudit"] = {
            "path": str(audit_path.resolve()),
            "status": "ok",
            "sourceCommit": build_commit,
            "rbfSha256": rbf_sha,
        }
        identifier = rbf_sha[:12]
        data["watchdog"].update(
            {
                "token": f"/tmp/zhaozhou-rollback-{identifier}.armed",
                "log": f"/tmp/zhaozhou-rollback-{identifier}.log",
                "script": f"/tmp/zhaozhou-rollback-{identifier}.sh",
            }
        )
        return repo, data

    def test_complete_identity_preflight_passes(self) -> None:
        self.assertEqual(VERIFY.validate_identity_receipt(copy.deepcopy(IDENTITY)), [])

    def test_actual_source_bound_identity_preflight_passes(self) -> None:
        self.assertEqual(
            VERIFY.validate_identity_receipt(copy.deepcopy(ACTUAL_IDENTITY), REPO),
            [],
        )

    def test_loader_working_sha_zero_mutant_fires(self) -> None:
        data = copy.deepcopy(ACTUAL_IDENTITY)
        data["loaderSource"]["workingSha256"] = "0" * 64
        errors = VERIFY.validate_identity_receipt(data, REPO)
        self.assertTrue(any("working SHA-256 mismatch" in error for error in errors))

    def test_nonexistent_loader_source_commit_fires(self) -> None:
        data = copy.deepcopy(ACTUAL_IDENTITY)
        data["sourceCommit"] = "0" * 40
        data["loaderSource"]["sourceCommit"] = "0" * 40
        errors = VERIFY.validate_identity_receipt(data, REPO)
        self.assertTrue(any("commit/path is absent" in error for error in errors))

    def test_complete_future_receipt_passes(self) -> None:
        self.assertEqual(VERIFY.validate_future_load_structure(self.future_receipt()), [])

    def test_complete_future_evidence_passes(self) -> None:
        repo, data = self.future_evidence_fixture()
        self.assertEqual(VERIFY.validate_future_load_receipt(data, repo), [])

    def test_future_evidence_requires_repository(self) -> None:
        errors = VERIFY.validate_future_load_receipt(self.future_receipt(), None)
        self.assertTrue(any("repository is required" in error for error in errors))

    def test_nonexistent_v2_evidence_path_fires(self) -> None:
        repo, data = self.future_evidence_fixture()
        Path(data["buildManifest"]["path"]).unlink()
        errors = VERIFY.validate_future_load_receipt(data, repo)
        self.assertTrue(any("manifest file does not exist" in error for error in errors))

    def test_nonexistent_build_source_commit_fires(self) -> None:
        repo, data = self.future_evidence_fixture()
        data["buildSourceCommit"] = "0" * 40
        data["buildManifest"]["sourceCommit"] = "0" * 40
        data["buildAudit"]["sourceCommit"] = "0" * 40
        errors = VERIFY.validate_future_load_receipt(data, repo)
        self.assertTrue(any("buildSourceCommit does not exist" in error for error in errors))

    def test_arbitrary_manifest_digest_fires_against_actual_file(self) -> None:
        repo, data = self.future_evidence_fixture()
        data["buildManifest"]["manifestSha256"] = "0" * 64
        errors = VERIFY.validate_future_load_receipt(data, repo)
        self.assertTrue(any("receipt/manifest self-digest mismatch" in error for error in errors))

    def test_local_rbf_mutation_fires(self) -> None:
        repo, data = self.future_evidence_fixture()
        Path(data["localRbf"]["path"]).write_bytes(b"mutated RBF\n")
        errors = VERIFY.validate_future_load_receipt(data, repo)
        self.assertTrue(any("receipt/local RBF SHA-256 mismatch" in error for error in errors))

    def test_uncommitted_audit_mutation_fires(self) -> None:
        repo, data = self.future_evidence_fixture()
        audit_path = Path(data["buildAudit"]["path"])
        audit_path.write_text(audit_path.read_text(encoding="utf-8") + " ", encoding="utf-8")
        errors = VERIFY.validate_future_load_receipt(data, repo)
        self.assertTrue(any("V2 build audit is not clean" in error for error in errors))
        self.assertTrue(
            any("V2 build audit working file differs" in error for error in errors)
        )

    def test_host_fingerprint_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["sshHostKey"]["fingerprint"] = "SHA256:wrong"
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("SSH fingerprint mismatch" in error for error in errors))

    def test_bridge_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["loadedState"]["bridge:fpga2hps"] = "disabled"
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("bridge:fpga2hps not enabled" in error for error in errors))

    def test_watchdog_menu_evidence_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["watchdogLog"].remove("watchdog-menu-ok=1")
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("exact contiguous" in error for error in errors))

    def test_rollback_attempt_false_fires(self) -> None:
        data = self.future_receipt()
        data["rollbackAttempted"] = False
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("rollbackAttempted is not true" in error for error in errors))

    def test_empty_raw_load_and_rollback_arrays_fire(self) -> None:
        data = self.future_receipt()
        data["loaded"] = []
        data["afterRollback"] = []
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("loaded: raw state lines are empty" in error for error in errors))
        self.assertTrue(any("after rollback: raw state lines are empty" in error for error in errors))

    def test_null_remote_rbf_and_build_commit_fire(self) -> None:
        data = self.future_receipt()
        data["remoteRbf"] = None
        data["buildSourceCommit"] = None
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("remoteRbf is null" in error for error in errors))
        self.assertTrue(any("buildSourceCommit is null" in error for error in errors))

    def test_manifest_source_binding_mutant_fires(self) -> None:
        data = self.future_receipt()
        data["buildManifest"]["sourceCommit"] = "0" * 40
        errors = VERIFY.validate_future_load_structure(data)
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
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("exact contiguous" in error for error in errors))
        self.assertTrue(any("summary attempts are not contiguous" in error for error in errors))

    def test_old_manifest_name_fires(self) -> None:
        data = self.future_receipt()
        data["buildManifest"]["path"] = "HARDWARE-SPECS-BUILD-MANIFEST.json"
        errors = VERIFY.validate_future_load_structure(data)
        self.assertTrue(any("V2 complete manifest path mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
