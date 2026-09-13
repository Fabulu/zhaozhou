from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
VERIFY_PATH = REPO / "tools" / "board" / "verify_superstation_bringup.py"
SPEC = importlib.util.spec_from_file_location("verify_superstation_bringup", VERIFY_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class SuperStationBringupVerifierTest(unittest.TestCase):
    def make_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        repo = Path(temporary.name)
        (repo / "fpga" / "rtl" / "platform").mkdir(parents=True)
        shutil.copy2(REPO / ".gitattributes", repo / ".gitattributes")
        shutil.copytree(REPO / "fpga" / "sys", repo / "fpga" / "sys")
        for name in (
            "ZhaozhouBringup.qsf",
            "ZhaozhouBringup.sdc",
            "files_bringup.qip",
        ):
            shutil.copy2(REPO / "fpga" / name, repo / "fpga" / name)
        shutil.copy2(
            REPO / "fpga" / "rtl" / "platform" / "zhao_ssone_bringup.sv",
            repo / "fpga" / "rtl" / "platform" / "zhao_ssone_bringup.sv",
        )
        return temporary, repo

    def test_current_sources_pass(self) -> None:
        errors, summary = VERIFY.verify_sources(REPO)
        self.assertEqual(errors, [])
        self.assertEqual(summary["sourceStatus"], "ok")

    def test_vendor_attribute_removal_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / ".gitattributes"
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace("fpga/sys/** -text", "fpga/sys/** text eol=lf"), encoding="utf-8")

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("fpga/sys/** -text" in error for error in errors))

    def test_vendor_mutation_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "fpga" / "sys" / "math.sv"
        path.write_bytes(path.read_bytes() + b"\n// committed mutant\n")

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("sys tree mismatch" in error for error in errors))

    def test_wrong_device_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "fpga" / "sys" / "sys.tcl"
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace("5CSEBA6U23I7", "5CSEBA5U23I7", 1), encoding="utf-8")

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("DEVICE 5CSEBA6U23I7" in error for error in errors))

    def test_removed_unused_pin_policy_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "fpga" / "ZhaozhouBringup.qsf"
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace(
                'set_global_assignment -name RESERVE_ALL_UNUSED_PINS "AS INPUT TRI-STATED"',
                'set_global_assignment -name RESERVE_ALL_UNUSED_PINS "AS OUTPUT DRIVING GROUND"',
            ),
            encoding="utf-8",
        )

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("AS INPUT TRI-STATED" in error for error in errors))

    def test_sdram_drive_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "fpga" / "rtl" / "platform" / "zhao_ssone_bringup.sv"
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace("    } = 'z;", "    } = '0;", 1),
            encoding="utf-8",
        )

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("tri-state the complete SDRAM bundle" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
