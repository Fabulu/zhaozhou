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
PATCH_PATH = REPO / "tools" / "board" / "patch_mister_sys_top.py"
PATCH_SPEC = importlib.util.spec_from_file_location("patch_mister_sys_top", PATCH_PATH)
assert PATCH_SPEC is not None and PATCH_SPEC.loader is not None
PATCH = importlib.util.module_from_spec(PATCH_SPEC)
PATCH_SPEC.loader.exec_module(PATCH)


class SuperStationBringupVerifierTest(unittest.TestCase):
    def make_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        repo = Path(temporary.name)
        (repo / "fpga" / "rtl" / "platform").mkdir(parents=True)
        (repo / "tools" / "board").mkdir(parents=True)
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
        shutil.copy2(REPO / "fpga" / "rtl" / "pll.qip", repo / "fpga" / "rtl" / "pll.qip")
        shutil.copy2(REPO / "fpga" / "rtl" / "pll.v", repo / "fpga" / "rtl" / "pll.v")
        shutil.copytree(REPO / "fpga" / "rtl" / "pll", repo / "fpga" / "rtl" / "pll")
        for name in (
            "patch_mister_sys_top.py",
            "superstation_build_manifest.py",
            "invoke_superstation_probe.ps1",
            "build_superstation_bringup.ps1",
            "build_superstation_specs.ps1",
        ):
            shutil.copy2(REPO / "tools" / "board" / name, repo / "tools" / "board" / name)
        return temporary, repo

    def make_build(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        build = Path(temporary.name)
        output = build / "output_files"
        output.mkdir()
        (build / "sys").mkdir()
        upstream = (REPO / "fpga" / "sys" / "sys_top.v").read_bytes()
        (build / "sys" / "sys_top.v").write_bytes(PATCH.patch_bytes(upstream))
        (output / "ZhaozhouBringup.flow.rpt").write_text(
            """; Flow Status ; Successful - now ;
; Top-level Entity Name ; sys_top ;
; Family ; Cyclone V ;
; Device ; 5CSEBA6U23I7 ;
""",
            encoding="utf-8",
        )
        fit_text = "Device 5CSEBA6U23I7 at 100° C\nQuartus Prime Fitter was successful\n"
        fit_text += "".join(
            f"Pin USER_IO[{bit}] has a permanently disabled output enable\n"
            for bit in range(7)
        )
        (output / "ZhaozhouBringup.fit.rpt").write_bytes(fit_text.encode("cp1252"))
        (output / "ZhaozhouBringup.sta.rpt").write_text(
            """Device 5CSEBA6U23I7
Quartus Prime TimeQuest Timing Analyzer was successful
Worst-case setup slack is 0.100
Worst-case hold slack is 0.200
Worst-case recovery slack is 0.300
Worst-case removal slack is 0.400
Worst-case minimum pulse width slack is 0.500
; Illegal Clocks ; 0 ; 0 ;
; Unconstrained Clocks ; 0 ; 0 ;
""",
            encoding="utf-8",
        )
        (output / "ZhaozhouBringup.pin").write_text(
            """CHIP  "ZhaozhouBringup"  ASSIGNED TO AN: 5CSEBA6U23I7
FPGA_CLK1_50 : V11 : input : 3.3-V LVTTL : : 3B : Y
FPGA_CLK2_50 : Y13 : input : 3.3-V LVTTL : : 4A : Y
FPGA_CLK3_50 : E11 : input : 3.3-V LVTTL : : 8A : Y
RESERVED_INPUT : A4 : : : : 7C :
""",
            encoding="utf-8",
        )
        (output / "ZhaozhouBringup.rbf").write_bytes(b"RBF")
        return temporary, build

    def test_current_sources_pass(self) -> None:
        errors, summary = VERIFY.verify_sources(REPO)
        self.assertEqual(errors, [])
        self.assertEqual(summary["sourceStatus"], "ok")

    def test_minimal_build_receipt_passes_with_cp1252_report(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        errors: list[str] = []
        summary: dict[str, object] = {}

        VERIFY.verify_build(build, errors, summary)

        self.assertEqual(errors, [])
        self.assertEqual(summary["buildStatus"], "ok")

    def test_unnumbered_tabular_critical_warning_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouBringup.flow.rpt"
        path.write_text(
            path.read_text(encoding="utf-8")
            + "; mode ; Input ; Critical Warning ; 4-bit value drives 5-bit port ;\n",
            encoding="utf-8",
        )
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("critical Quartus warnings present" in error for error in errors))

    def test_negative_timing_slack_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouBringup.sta.rpt"
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                "Worst-case setup slack is 0.100",
                "Worst-case setup slack is -0.100",
            ),
            encoding="utf-8",
        )
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("negative worst-case setup slack" in error for error in errors))

    def test_unused_output_pin_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouBringup.pin"
        path.write_text(
            path.read_text(encoding="utf-8") + "RESERVED_OUTPUT_DRIVEN_HIGH : B4 : output : : : 7C :\n",
            encoding="utf-8",
        )
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("unused output-driving pins" in error for error in errors))

    def test_missing_user_io_high_z_evidence_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouBringup.fit.rpt"
        data = path.read_bytes().replace(
            b"Pin USER_IO[2] has a permanently disabled output enable\n",
            b"",
            1,
        )
        path.write_bytes(data)
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("expected all bits 0..6" in error for error in errors))

    def test_loader_fingerprint_pin_removal_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "tools" / "board" / "invoke_superstation_probe.ps1"
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace(
                "SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU",
                "SHA256:removed",
            ),
            encoding="utf-8",
        )

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("FqNJOsj3FLUoMQxgn" in error for error in errors))

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

    def test_pll_mutation_fires(self) -> None:
        temporary, repo = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = repo / "fpga" / "rtl" / "pll.v"
        path.write_bytes(path.read_bytes() + b"\n// committed mutant\n")

        errors, _ = VERIFY.verify_sources(repo)

        self.assertTrue(any("PLL blob mismatch" in error for error in errors))

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
