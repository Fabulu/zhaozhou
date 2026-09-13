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


class SuperStationBuildManifestTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.repo = root / "repo"
        self.build = root / "build"
        self.repo.mkdir()
        (self.repo / "src.sv").write_text("module src; endmodule\n", encoding="utf-8")
        (self.repo / "verify.py").write_text("# verifier\n", encoding="utf-8")
        (self.build / "sys").mkdir(parents=True)
        (self.build / "rtl").mkdir()
        (self.build / "sys" / "sys_top.v").write_bytes(b"patched sys top\n")
        (self.build / "rtl" / "src.sv").write_text("module src; endmodule\n", encoding="utf-8")
        for name in ("Test.qpf", "Test.qsf", "Test.sdc", "files.qip"):
            (self.build / name).write_text(name + "\n", encoding="utf-8")
        self.commit = "a" * 40
        (self.build / ".test-build").write_text(
            f"sourceCommit={self.commit}\ncreated=test\n", encoding="utf-8"
        )

        self.old_profiles = MANIFEST.PROFILES
        self.old_patched = MANIFEST.PATCHED_SYS_TOP_SHA256
        self.old_git_output = MANIFEST.git_output
        MANIFEST.PROFILES = {
            "Test": {
                "project": "Test",
                "marker": ".test-build",
                "qip": "files.qip",
                "sourcePaths": ("src.sv",),
                "verificationPaths": ("verify.py",),
            }
        }
        MANIFEST.PATCHED_SYS_TOP_SHA256 = hashlib.sha256(b"patched sys top\n").hexdigest()

        def fake_git_output(repo: Path, *args: str) -> str:
            if args[:2] == ("rev-parse", "HEAD"):
                return self.commit
            if "status" in args:
                return ""
            if args[:2] == ("cat-file", "-e"):
                return ""
            raise AssertionError(args)

        MANIFEST.git_output = fake_git_output
        self.addCleanup(setattr, MANIFEST, "PROFILES", self.old_profiles)
        self.addCleanup(setattr, MANIFEST, "PATCHED_SYS_TOP_SHA256", self.old_patched)
        self.addCleanup(setattr, MANIFEST, "git_output", self.old_git_output)

    def complete_manifest(self) -> dict:
        source = MANIFEST.create_source_manifest(self.repo, self.build, "Test")
        source_path = self.build / "source.json"
        MANIFEST.write_manifest(source_path, source)
        output = self.build / "output_files"
        output.mkdir()
        for suffix in ("flow.rpt", "map.rpt", "fit.rpt", "asm.rpt", "sta.rpt", "pin", "rbf", "sof"):
            (output / f"Test.{suffix}").write_bytes(f"artifact:{suffix}\n".encode())
        return MANIFEST.create_complete_manifest(self.repo, self.build, "Test", source_path)

    def test_complete_manifest_verifies(self) -> None:
        complete = self.complete_manifest()
        self.assertEqual(MANIFEST.verify_manifest_data(complete, self.repo, self.build), [])

    def test_artifact_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        (self.build / "output_files" / "Test.rbf").write_bytes(b"mutant\n")

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("artifact hash/size mismatch: output_files/Test.rbf" in error for error in errors))

    def test_source_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        (self.repo / "src.sv").write_text("module mutant; endmodule\n", encoding="utf-8")

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("source hash/size mismatch: src.sv" in error for error in errors))

    def test_manifest_self_digest_mutation_fires(self) -> None:
        complete = self.complete_manifest()
        complete["status"] = "forged"

        errors = MANIFEST.verify_manifest_data(complete, self.repo, self.build)

        self.assertTrue(any("manifest self-digest mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
