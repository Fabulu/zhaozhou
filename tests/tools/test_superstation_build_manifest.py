from __future__ import annotations

import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
MANIFEST_PATH = REPO / "tools" / "board" / "superstation_build_manifest.py"
SPEC = importlib.util.spec_from_file_location("superstation_build_manifest", MANIFEST_PATH)
assert SPEC is not None and SPEC.loader is not None
MANIFEST = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MANIFEST)
PRODUCTION_PROFILES = MANIFEST.PROFILES


class SuperStationBuildManifestTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.repo = root / "repo"
        self.build = root / "build"
        self.repo.mkdir()
        (self.repo / "fpga" / "sys").mkdir(parents=True)
        (self.repo / "fpga" / "rtl").mkdir()
        (self.repo / "fpga" / "sys" / "sys_top.v").write_bytes(b"upstream sys top\n")
        (self.repo / "fpga" / "sys" / "build_id.tcl").write_bytes(b"upstream build id\n")
        (self.repo / "fpga" / "rtl" / "src.sv").write_text(
            "module src; endmodule\n", encoding="utf-8"
        )
        (self.repo / "verify.py").write_text("# verifier\n", encoding="utf-8")
        (self.build / "sys").mkdir(parents=True)
        (self.build / "rtl").mkdir()
        (self.build / "sys" / "sys_top.v").write_bytes(b"patched sys top\n")
        (self.build / "sys" / "build_id.tcl").write_bytes(b"patched build id\n")
        (self.build / "rtl" / "src.sv").write_text("module src; endmodule\n", encoding="utf-8")
        for name in ("Test.qpf", "Test.qsf", "Test.sdc", "files.qip"):
            (self.build / name).write_text(name + "\n", encoding="utf-8")
        self.commit = "a" * 40
        (self.build / ".test-build").write_text(
            f"sourceCommit={self.commit}\ncreated=test\n", encoding="utf-8"
        )

        self.old_profiles = MANIFEST.PROFILES
        self.old_patched = MANIFEST.PATCHED_SYS_TOP_SHA256
        self.old_patched_build_id = MANIFEST.PATCHED_BUILD_ID_SHA256
        self.old_git_output = MANIFEST.git_output
        MANIFEST.PROFILES = {
            "Test": {
                "project": "Test",
                "marker": ".test-build",
                "qip": "files.qip",
                "sourcePaths": ("fpga/sys", "fpga/rtl/src.sv"),
                "verificationPaths": ("verify.py",),
            }
        }
        MANIFEST.PATCHED_SYS_TOP_SHA256 = hashlib.sha256(b"patched sys top\n").hexdigest()
        MANIFEST.PATCHED_BUILD_ID_SHA256 = hashlib.sha256(b"patched build id\n").hexdigest()

        def fake_git_output(repo: Path, *args: str) -> str:
            if args[:2] == ("rev-parse", "HEAD"):
                return self.commit
            if "status" in args:
                return ""
            if args[:2] == ("cat-file", "-e"):
                return ""
            if "diff" in args and "--quiet" in args:
                return ""
            raise AssertionError(args)

        MANIFEST.git_output = fake_git_output
        self.addCleanup(setattr, MANIFEST, "PROFILES", self.old_profiles)
        self.addCleanup(setattr, MANIFEST, "PATCHED_SYS_TOP_SHA256", self.old_patched)
        self.addCleanup(
            setattr,
            MANIFEST,
            "PATCHED_BUILD_ID_SHA256",
            self.old_patched_build_id,
        )
        self.addCleanup(setattr, MANIFEST, "git_output", self.old_git_output)

    def complete_manifest(self) -> dict:
        source = MANIFEST.create_source_manifest(self.repo, self.build, "Test")
        source_path = self.build / "source.json"
        MANIFEST.write_manifest(source_path, source)
        output = self.build / "output_files"
        output.mkdir()
        for suffix in MANIFEST.ARTIFACT_SUFFIXES:
            (output / f"Test.{suffix}").write_bytes(f"artifact:{suffix}\n".encode())
        return MANIFEST.create_complete_manifest(self.repo, self.build, "Test", source_path)

    def test_production_profiles_bind_compile_runner(self) -> None:
        for name in ("Bringup", "Specs"):
            profile = PRODUCTION_PROFILES[name]
            self.assertIn("tools/env/zhao-env.ps1", profile["sourcePaths"])
            self.assertIn("tools/board/patch_mister_build_id.py", profile["sourcePaths"])
            self.assertIn("tools/board/run_superstation_quartus.py", profile["sourcePaths"])
            self.assertIn(
                "tests/tools/test_patch_mister_build_id.py",
                profile["verificationPaths"],
            )
            self.assertIn(
                "tests/tools/test_run_superstation_quartus.py",
                profile["verificationPaths"],
            )

    def test_complete_manifest_verifies(self) -> None:
        complete = self.complete_manifest()
        self.assertEqual(MANIFEST.verify_manifest_data(complete, self.repo, self.build), [])

    def test_direct_stage_schema_replaces_done_with_receipt(self) -> None:
        self.assertIn("stage-receipt.json", MANIFEST.ARTIFACT_SUFFIXES)
        self.assertNotIn("done", MANIFEST.ARTIFACT_SUFFIXES)

    def test_missing_stage_receipt_artifact_fires(self) -> None:
        complete = self.complete_manifest()
        (self.build / "output_files" / "Test.stage-receipt.json").unlink()

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("Test.stage-receipt.json" in error for error in errors))

    def test_unexpected_done_artifact_fires(self) -> None:
        complete = self.complete_manifest()
        (self.build / "output_files" / "Test.done").write_bytes(b"forbidden\n")

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(
            any("actual artifact set mismatch" in error and "Test.done" in error for error in errors)
        )

    def test_source_commit_drift_fires(self) -> None:
        complete = self.complete_manifest()
        clean_git_output = MANIFEST.git_output

        def drift_git_output(repo: Path, *args: str) -> str:
            if "diff" in args and "--quiet" in args:
                raise ValueError("fixture source differs")
            return clean_git_output(repo, *args)

        MANIFEST.git_output = drift_git_output
        try:
            errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)
        finally:
            MANIFEST.git_output = clean_git_output

        self.assertIn("manifest source/verification files differ from sourceCommit", errors)

    def test_detached_patched_sys_top_record_fires(self) -> None:
        complete = self.complete_manifest()
        patched_path = self.build / "sys" / "sys_top.v"
        patched_path.write_bytes(b"unsafe mutant\n")
        complete["buildInputs"]["sys/sys_top.v"] = MANIFEST.file_record(patched_path)
        source_projection = dict(complete)
        source_projection.pop("sourceManifestSha256", None)
        source_projection["phase"] = "source"
        source_projection["status"] = "source-captured"
        source_projection["artifacts"] = {}
        complete["sourceManifestSha256"] = MANIFEST.manifest_digest(source_projection)
        complete["manifestSha256"] = MANIFEST.manifest_digest(complete)

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertIn("manifest patched sys_top record mismatch", errors)

    def test_detached_patched_build_id_record_fires(self) -> None:
        complete = self.complete_manifest()
        build_id_path = self.build / "sys" / "build_id.tcl"
        build_id_path.write_bytes(b"project_open mutant\n")
        complete["buildInputs"]["sys/build_id.tcl"] = MANIFEST.file_record(build_id_path)
        source_projection = dict(complete)
        source_projection.pop("sourceManifestSha256", None)
        source_projection["phase"] = "source"
        source_projection["status"] = "source-captured"
        source_projection["artifacts"] = {}
        complete["sourceManifestSha256"] = MANIFEST.manifest_digest(source_projection)
        complete["manifestSha256"] = MANIFEST.manifest_digest(complete)

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertIn("manifest patched build-ID record mismatch", errors)

    def test_artifact_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        (self.build / "output_files" / "Test.rbf").write_bytes(b"mutant\n")

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("artifact hash/size mismatch: output_files/Test.rbf" in error for error in errors))

    def test_source_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        (self.repo / "fpga" / "rtl" / "src.sv").write_text(
            "module mutant; endmodule\n", encoding="utf-8"
        )

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("source hash/size mismatch: fpga/rtl/src.sv" in error for error in errors))

    def test_omitted_source_with_recomputed_self_digest_fires(self) -> None:
        complete = self.complete_manifest()
        complete["sourceFiles"].pop("fpga/rtl/src.sv")
        complete["manifestSha256"] = MANIFEST.manifest_digest(complete)

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("source record set mismatch" in error for error in errors))

    def test_added_output_with_recomputed_self_digest_fires(self) -> None:
        complete = self.complete_manifest()
        extra = self.build / "output_files" / "Test.extra"
        extra.write_bytes(b"extra\n")
        complete["artifacts"]["output_files/Test.extra"] = MANIFEST.file_record(extra)
        complete["manifestSha256"] = MANIFEST.manifest_digest(complete)

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("artifact record set mismatch" in error for error in errors))

    def test_source_manifest_digest_mutant_fires(self) -> None:
        complete = self.complete_manifest()
        complete["sourceManifestSha256"] = "0" * 64
        complete["manifestSha256"] = MANIFEST.manifest_digest(complete)

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("sourceManifestSha256 mismatch" in error for error in errors))

    def test_manifest_self_digest_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        complete["status"] = "forged"

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("manifest self-digest mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
