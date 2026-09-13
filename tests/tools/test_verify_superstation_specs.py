from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
VERIFY_PATH = REPO / "tools" / "board" / "verify_superstation_specs.py"
sys.path.insert(0, str(VERIFY_PATH.parent))
SPEC = importlib.util.spec_from_file_location("verify_superstation_specs", VERIFY_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class SuperStationSpecsVerifierTest(unittest.TestCase):
    def make_build(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        build = Path(temporary.name)
        output = build / "output_files"
        output.mkdir()
        (output / "ZhaozhouSpecs.flow.rpt").write_text(
            "; Flow Status ; Successful - now ;\n; Device ; 5CSEBA6U23I7 ;\n",
            encoding="utf-8",
        )
        (output / "ZhaozhouSpecs.map.rpt").write_text(
            """zhao_crc32c_fold
zhao_raster_fill
zhao_dual18_mul
cyclonev_mac
Implemented 34 DSP elements
Quartus Prime Analysis & Synthesis was successful
""",
            encoding="utf-8",
        )
        (output / "ZhaozhouSpecs.fit.rpt").write_bytes(
            "Device 5CSEBA6U23I7 at 100° C\nQuartus Prime Fitter was successful\n".encode("cp1252")
        )
        (output / "ZhaozhouSpecs.sta.rpt").write_text(
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
        (output / "ZhaozhouSpecs.pin").write_text(
            """CHIP  "ZhaozhouSpecs"  ASSIGNED TO AN: 5CSEBA6U23I7
FPGA_CLK1_50 : V11 : input : 3.3-V LVTTL : : 3B : Y
FPGA_CLK2_50 : Y13 : input : 3.3-V LVTTL : : 4A : Y
FPGA_CLK3_50 : E11 : input : 3.3-V LVTTL : : 8A : Y
RESERVED_INPUT : A4 : : : : 7C :
""",
            encoding="utf-8",
        )
        (output / "ZhaozhouSpecs.rbf").write_bytes(b"RBF")
        return temporary, build

    def test_spec_build_receipt_passes(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        errors: list[str] = []
        summary: dict[str, object] = {}

        VERIFY.verify_build(build, errors, summary)

        self.assertEqual(errors, [])
        self.assertEqual(summary["implementedDsps"], 34)

    def test_missing_shipping_module_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouSpecs.map.rpt"
        path.write_text(path.read_text("utf-8").replace("zhao_raster_fill", "removed_fill"), "utf-8")
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("zhao_raster_fill" in error for error in errors))

    def test_missing_packed_dsp_fires(self) -> None:
        temporary, build = self.make_build()
        self.addCleanup(temporary.cleanup)
        path = build / "output_files" / "ZhaozhouSpecs.map.rpt"
        path.write_text(path.read_text("utf-8").replace("Implemented 34", "Implemented 33"), "utf-8")
        errors: list[str] = []

        VERIFY.verify_build(build, errors, {})

        self.assertTrue(any("packed Zhaozhou block may be absent" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
