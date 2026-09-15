#!/usr/bin/env python3
"""Fail closed when a local Quartus entry point can restore all-core load."""

from __future__ import annotations

from pathlib import Path
import unittest


REPO = Path(__file__).resolve().parents[2]
PS_RUNNERS = (
    "tools/quartus/run_block_fit.ps1",
    "tools/quartus/run_block_map.ps1",
    "tools/quartus/run_calib.ps1",
    "tools/quartus/run_shell_fit.ps1",
)
FIXED_QSF_TOOLS = (
    "tools/budget/gen_calib.py",
    "tools/budget/check_dual18_map.py",
)
TIMING_RUNNER = "tools/quartus/run_g8a_timing3_fit.ps1"


def validate_texts(texts: dict[str, str]) -> None:
    for path in PS_RUNNERS:
        text = texts[path]
        if text.count("[ValidateRange(1, 2)]") != 1:
            raise AssertionError(f"{path} does not reject processor counts above two")
        if text.count("[int]$Processors = 2") != 1:
            raise AssertionError(f"{path} does not default to exactly two processors")
        if "NUM_PARALLEL_PROCESSORS 4" in text or "Processors = 4" in text:
            raise AssertionError(f"{path} retains a four-worker launch path")

    for path in FIXED_QSF_TOOLS:
        text = texts[path]
        if text.count("set_global_assignment -name NUM_PARALLEL_PROCESSORS 2") != 1:
            raise AssertionError(f"{path} does not emit exactly two Quartus processors")
        if "NUM_PARALLEL_PROCESSORS 4" in text:
            raise AssertionError(f"{path} retains four-worker generated QSF")

    timing = texts[TIMING_RUNNER]
    if timing.count("-Processors 2") != 1:
        raise AssertionError("Timing3 does not explicitly select two processors")
    if "-Processors 4" in timing or "-Processors 3" in timing:
        raise AssertionError("Timing3 retains an unsafe processor override")

    block_fit = texts["tools/quartus/run_block_fit.ps1"]
    required = (
        "$RunnerProcess.ProcessorAffinity = [IntPtr]$ThermalAffinityValue",
        "$env:OMP_NUM_THREADS = \"$Processors\"",
        "$env:MKL_NUM_THREADS = \"$Processors\"",
        "$env:QUARTUS_NUM_PARALLEL_PROCESSORS = \"$Processors\"",
        "$qsf += \"set_global_assignment -name NUM_PARALLEL_PROCESSORS $Processors\"",
        "$RunnerProcess.ProcessorAffinity = $OriginalAffinity",
    )
    for marker in required:
        if block_fit.count(marker) != 1:
            raise AssertionError(f"block-fit thermal backstop differs: {marker}")
    if "MSAcpi_ThermalZoneTemperature" in block_fit:
        raise AssertionError("block-fit trusts the non-CPU ACPI thermal zone")
    if "PriorityClass" in block_fit:
        raise AssertionError("block-fit misrepresents process priority as thermal protection")


class QuartusThermalLimitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.texts = {
            path: (REPO / path).read_text(encoding="utf-8")
            for path in (*PS_RUNNERS, *FIXED_QSF_TOOLS, TIMING_RUNNER)
        }

    def test_every_heavy_entry_point_is_bounded(self) -> None:
        validate_texts(self.texts)

    def test_four_worker_mutations_fire(self) -> None:
        for path in PS_RUNNERS:
            with self.subTest(path=path):
                damaged = dict(self.texts)
                damaged[path] = damaged[path].replace(
                    "[int]$Processors = 2", "[int]$Processors = 4", 1
                )
                with self.assertRaises(AssertionError):
                    validate_texts(damaged)
        for path in FIXED_QSF_TOOLS:
            with self.subTest(path=path):
                damaged = dict(self.texts)
                damaged[path] = damaged[path].replace(
                    "NUM_PARALLEL_PROCESSORS 2", "NUM_PARALLEL_PROCESSORS 4", 1
                )
                with self.assertRaises(AssertionError):
                    validate_texts(damaged)


if __name__ == "__main__":
    unittest.main()
